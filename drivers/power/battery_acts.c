/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Actions SoC battery driver
 */

#include <errno.h>
#include <kernel.h>
#include <device.h>
#include <adc.h>
#include <misc/byteorder.h>
#include <power_supply.h>
#include <gpio.h>
#include <board.h>
#include "soc.h"

#define SYS_LOG_LEVEL SYS_LOG_LEVEL_INFO
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_BATTERY_ACTS

#define MULTI_USED_UVLO              9

#ifndef CONFIG_SOC_SERIES_ANDESC
#define V_REF                        (1510)
#define V_LSB_MULT1000               (V_REF*2*1000/1024)
#else
#define V_REF                        (1500)
#define V_LSB_MULT1000               (48*100/1024)
#define CALC_BAT_mV(adc_v)           ((adc_v*4800)/1024)
#endif

#define BATTERY_DRV_POLL_INTERVAL             100
#define CHG_STATUS_DETECT_DEBOUNCE_MS         300
#define BAT_MIN_VOLTAGE                       2400
#define BAT_VOL_LSB_MV                        3
#define BAT_VOLTAGE_RESERVE                   (0x1fff)
#define BAT_CAP_RESERVE                       (101)

#define BAT_VOL_SAMPLE_ONE_TIMEOUT_MS         (100)

struct battery_cap
{
	u16_t cap;
	u16_t volt;
};
/*
	BAT_VOLTAGE_TBL_DCNT * BAT_VOLTAGE_TBL_ACNT : the samples number
	BAT_VOLTAGE_TBL_DCNT + BAT_VOLTAGE_TBL_ACNT : Save space occupied by sample points
*/
#define  BAT_VOLTAGE_TBL_DCNT                 8
#define  BAT_VOLTAGE_TBL_ACNT                 4
#define  BAT_VOLTAGE_FILTER_DROP_DCNT         1
#define  BAT_VOLTAGE_FILTER_DROP_ACNT         1
#define  INDEX_VOL                           (0)
#define  INDEX_CAP                           (INDEX_VOL+1)

struct report_deb_ctr {
	uint16_t rise;
	uint16_t decline;
	uint16_t times;
	uint8_t step;
};

struct voltage_tbl
{
	u16_t vol_data[BAT_VOLTAGE_TBL_DCNT];
	u16_t d_index;
	u16_t _vol_average[BAT_VOLTAGE_TBL_ACNT];
	u8_t a_index;
	u8_t data_collecting;   // 标志是否正在采集数据中
};
struct acts_battery_info {
	struct k_timer timer;

	struct device *gpio_dev;
	struct device *adc_dev;

	struct adc_seq_table seq_tbl;
	struct adc_seq_entry seq_entrys;
	u8_t adcval[2];
	struct voltage_tbl vol_tbl;
	u32_t cur_voltage       : 13;
	u32_t cur_cap           : 7;
	u32_t cur_dc5v_status   : 1;
	u32_t cur_chg_status    : 3;
	u32_t last_cap_report       : 7;
	u32_t last_voltage_report : 13;
	u32_t last_dc5v_report      : 1;
	u32_t last_chg_report       : 3;

	struct report_deb_ctr report_ctr[2];

	const struct battery_cap *cap_tbl;
	bat_charge_callback_t notify;

	u8_t chg_debounce_buf[CHG_STATUS_DETECT_DEBOUNCE_MS/BATTERY_DRV_POLL_INTERVAL];

};

struct acts_battery_config {
	char *adc_name;
	u16_t adc_chan;
	u16_t pin;
	char *gpio_name;

	u16_t poll_interval_ms;
};


#define BATTERY_CAP_TABLE_CNT		12
static const struct battery_cap battery_cap_tbl[BATTERY_CAP_TABLE_CNT] = {
    BOARD_BATTERY_CAP_MAPPINGS
};
static void battery_volt_selectsort(u16_t *arr, u16_t len);
bool bat_charge_callback(bat_charge_event_t event, bat_charge_event_para_t *para, struct acts_battery_info *bat);


/* 参考spec 7.4.5.1 BATADC. 1LSB = 2.93mv
*/
static unsigned int battery_adcval_to_voltage(unsigned int adcval)
{
#ifndef CONFIG_SOC_SERIES_ANDESC
	return (V_REF + ((V_LSB_MULT1000*adcval + V_LSB_MULT1000) + 500) / 1000 );
#else
    return CALC_BAT_mV(adcval);
#endif
}

/* voltage: mV */
static int __battery_get_voltage(struct acts_battery_info *bat)
{
	unsigned int adc_val, volt_mv;

	adc_read(bat->adc_dev, &bat->seq_tbl);
	adc_val = sys_get_le16(bat->seq_entrys.buffer);

	volt_mv = battery_adcval_to_voltage(adc_val);

	//SYS_LOG_INF("adc %d volt %d", adc_val, volt_mv);

	return volt_mv;
}

static int _battery_get_voltage(struct acts_battery_info *bat)
{
	unsigned int i, sum, num;
	unsigned int volt_mv;

	// order the hostory data
	battery_volt_selectsort(bat->vol_tbl.vol_data, BAT_VOLTAGE_TBL_DCNT);

	sum = 0;
	// drop two min and max data
	for (i = BAT_VOLTAGE_FILTER_DROP_DCNT;
		i < BAT_VOLTAGE_TBL_DCNT - BAT_VOLTAGE_FILTER_DROP_DCNT; i++)
	{
		sum += bat->vol_tbl.vol_data[i];
	}
	num = BAT_VOLTAGE_TBL_DCNT - BAT_VOLTAGE_FILTER_DROP_DCNT*2;

	// Rounded to one decimal place
	volt_mv = ((sum*10 / num) + 5) / 10;

	//SYS_LOG_INF("volt avr %d", volt_mv);

	return volt_mv;
}

/* Get a voltage sample point
*/
static unsigned int sample_one_voltage(struct acts_battery_info *bat)
{
	unsigned int volt_mv = __battery_get_voltage(bat);

	bat->vol_tbl.vol_data[bat->vol_tbl.d_index] = volt_mv;
	bat->vol_tbl.d_index++;
	bat->vol_tbl.d_index %= BAT_VOLTAGE_TBL_DCNT;

	if (bat->vol_tbl.d_index == 0)
	{
		bat->vol_tbl._vol_average[bat->vol_tbl.a_index] = _battery_get_voltage(bat);
		bat->vol_tbl.a_index++;
		bat->vol_tbl.a_index %= BAT_VOLTAGE_TBL_ACNT;

		if ( (bat->vol_tbl.data_collecting == 1)
			&& (bat->vol_tbl.a_index == 0) )
		{
			SYS_LOG_INF("collect done");
			bat->vol_tbl.data_collecting = 0;
		}
	}

	return volt_mv;
}

static int battery_get_voltage(struct acts_battery_info *bat)
{
	unsigned int i, sum, num;
	u16_t temp_vol_average[BAT_VOLTAGE_TBL_ACNT];

	/* 获取平均电压值前，先采样一个电压值，保证任何时候调用battery_get_voltage函数都不会发生除数为0的异常
	*/
	sample_one_voltage(bat);

	num = 0;
	sum = 0;

	if (!bat->vol_tbl.data_collecting){
		for (i = 0; i < BAT_VOLTAGE_TBL_ACNT; i++ ){
			temp_vol_average[i] =  bat->vol_tbl._vol_average[i];
		}

		battery_volt_selectsort(temp_vol_average, BAT_VOLTAGE_TBL_ACNT);

		for (i = BAT_VOLTAGE_FILTER_DROP_ACNT;
			i < BAT_VOLTAGE_TBL_ACNT - BAT_VOLTAGE_FILTER_DROP_ACNT; i++ ){
			sum += temp_vol_average[i];
		}
		num = BAT_VOLTAGE_TBL_ACNT - BAT_VOLTAGE_FILTER_DROP_ACNT*2;
	}else{
		for (i = 0; i < bat->vol_tbl.d_index ; i++ ){
			sum +=  bat->vol_tbl.vol_data[i];
			num++;
		}

		for (i = 0; i < bat->vol_tbl.a_index; i++ ){
			sum +=  bat->vol_tbl._vol_average[i];
			num++;
		}
	}

	if(num == 0){
		return -EAGAIN;
	}

	//SYS_LOG_INF("sum %d, num %d", sum, num);
	return ((sum*10 / num + 5) / 10);

}

uint32_t voltage2capacit(uint32_t volt, struct acts_battery_info *bat)
{
	const struct battery_cap *bc, *bc_prev;
	uint32_t  i, cap = 0;

	/* %0 */
	if (volt <= bat->cap_tbl[0].volt)
		return 0;

	/* %100 */
	if (volt >= bat->cap_tbl[BATTERY_CAP_TABLE_CNT - 1].volt)
		return 100;

	for (i = 1; i < BATTERY_CAP_TABLE_CNT; i++) {
		bc = &bat->cap_tbl[i];
		if (volt < bc->volt) {
			bc_prev = &bat->cap_tbl[i - 1];
			/* linear fitting */
			cap = bc_prev->cap + (((bc->cap - bc_prev->cap) *
				(volt - bc_prev->volt)*10 + 5) / (bc->volt - bc_prev->volt)) / 10;

			break;
		}
	}

	return cap;
}

static void bat_voltage_record_reset(struct acts_battery_info *bat)
{
	int d, a;
	for (a = 0; a < BAT_VOLTAGE_TBL_ACNT; a++)
	{
		for (d = 0; d < BAT_VOLTAGE_TBL_DCNT; d++)
		{
			bat->vol_tbl.vol_data[d] = 0;
		}
		bat->vol_tbl._vol_average[a] = 0;
	}
	bat->cur_voltage = 0;
	bat->vol_tbl.d_index = 0;
	bat->vol_tbl.a_index = 0;
	bat->vol_tbl.data_collecting = 1;
	bat->cur_cap = 0;

	SYS_LOG_INF("reset");
}

static int battery_get_volt(struct acts_battery_info *bat)
{
	int volt;

	uint32_t cur_time = k_uptime_get_32();

	volt = battery_get_voltage(bat);

	while (volt < 0) {
		volt = battery_get_voltage(bat);
		if ((k_uptime_get_32() - cur_time) > BAT_VOL_SAMPLE_ONE_TIMEOUT_MS) {
			SYS_LOG_ERR("sample one voltage timeout");
			return -ETIMEDOUT;
		}
	}

	return volt;
}


static int battery_get_capacity(struct acts_battery_info *bat)
{
	int volt = battery_get_volt(bat);

	return voltage2capacit(volt, bat);
}

static int battery_get_charge_status(struct acts_battery_info *bat, const void *cfg)
{
#ifdef BOARD_BATTERY_CHARGING_STATUS_GPIO
	struct device *gpio_dev = bat->gpio_dev;
	const struct acts_battery_config *bat_cfg = cfg;
	u32_t pin_status = 0;
#endif
	u32_t charge_status = 0;
	if (sys_test_bit(MULTI_USED, MULTI_USED_UVLO))
	{
	#ifdef BOARD_BATTERY_CHARGING_STATUS_GPIO
		gpio_pin_read(gpio_dev, bat_cfg->pin , &pin_status);
		if (pin_status == 1)
		{
			charge_status = POWER_SUPPLY_STATUS_CHARGING;
		}
		else
		{
			charge_status = POWER_SUPPLY_STATUS_FULL;
		}
	#endif

		// 电池是否存在的判断, 目前dvb板只能这样来做
		if (__battery_get_voltage(bat) <= BAT_MIN_VOLTAGE)
			charge_status = POWER_SUPPLY_STATUS_BAT_NOTEXIST;
	}
	else
	{
		charge_status = POWER_SUPPLY_STATUS_DISCHARGE;
	}

	return charge_status;
}


static void battery_volt_selectsort(u16_t *arr, u16_t len)
{
	u16_t i, j, min;
	for (i = 0; i < len-1; i++)
	{
		min = i;
		for (j = i+1; j < len; j++)
		{
			if (arr[min] > arr[j])
				min = j;
		}
		// swap
		if(min != i)
		{
			arr[min] ^= arr[i];
			arr[i] ^= arr[min];
			arr[min] ^= arr[i];
		}
	}
}


static int bat_report_debounce(u32_t last_val, u32_t cur_val, struct report_deb_ctr *ctr)
{
	if ((cur_val  > last_val) && (cur_val - last_val >= ctr->step))
	{
		ctr->rise++;
		ctr->decline = 0;
		//SYS_LOG_INF("RISE(%s) %d\n", cur_val>100 ? "VOL" : "CAP", ctr->rise);
	}
	else if ((cur_val  < last_val ) && (last_val - cur_val >= ctr->step))
	{
		ctr->decline++;
		ctr->rise = 0;
		//SYS_LOG_INF("DECLINE(%s) %d\n", cur_val>100 ? "VOL" : "CAP", ctr->decline);
	}

	if (ctr->decline >= ctr->times)
	{
		//ACT_LOG_ID_INF(ALF_STR_bat_report_debounce__DECLINE_DDN, 2, last_val, cur_val);
		ctr->decline = 0;
		return 1;
	}
	else if(ctr->rise >= ctr->times)
	{
		//ACT_LOG_ID_INF(ALF_STR_bat_report_debounce__RISE_DDN, 2, last_val, cur_val);
		ctr->rise = 0;
		return 1;
	}
	else
	{
		return 0;
	}
}

void battery_acts_voltage_poll(struct acts_battery_info *bat)
{
	int volt_mv;
	bat_charge_event_para_t para;
	uint32_t cap = 0;

	volt_mv = battery_get_voltage(bat);

	if(volt_mv < 0){
		return;
	}

	/* 等待数据收集完毕
	*/
	if (bat->vol_tbl.data_collecting)
		return;

	//ACT_LOG_ID_INF(ALF_STR_battery_acts_voltage_poll__CUR_VOLTAGE_D_VOLT_M, 3, bat->cur_voltage, volt_mv, bat->cur_chg_status);
	if( ( bat->cur_voltage >= volt_mv + BAT_VOL_LSB_MV )
		|| ( bat->cur_voltage + BAT_VOL_LSB_MV <= volt_mv) )
	{
		bat->cur_voltage = volt_mv;
		cap = voltage2capacit(volt_mv, bat);
	}
	if ((cap != 0) && (cap != bat->cur_cap))
	{
		bat->cur_cap = cap;
	}

	if ((bat->cur_chg_status != POWER_SUPPLY_STATUS_BAT_NOTEXIST)
		&& (bat->cur_chg_status != POWER_SUPPLY_STATUS_UNKNOWN))
	{
		bat->report_ctr[INDEX_CAP].times = 600;   // unit 100ms
		bat->report_ctr[INDEX_CAP].step = 5;      //   %

		if ( bat->cur_voltage <= 3400 ){
			bat->report_ctr[INDEX_VOL].times = 50;  // unit 100ms
			bat->report_ctr[INDEX_VOL].step = BAT_VOL_LSB_MV*3;    // mv
		}else if (bat->cur_voltage <= 3700) {
			bat->report_ctr[INDEX_VOL].step = BAT_VOL_LSB_MV*5;    // mv
			bat->report_ctr[INDEX_VOL].times = 150;  // unit 100ms
		} else {
			bat->report_ctr[INDEX_VOL].step = BAT_VOL_LSB_MV*6;    // mv
		 	bat->report_ctr[INDEX_VOL].times = 350; // unit 100ms
		}

		if ( bat_report_debounce(bat->last_voltage_report,    bat->cur_voltage, &bat->report_ctr[INDEX_VOL])
			|| (bat->last_voltage_report == BAT_VOLTAGE_RESERVE))
		{

			para.voltage_val = bat->cur_voltage*1000;
			if (bat_charge_callback(BAT_CHG_EVENT_VOLTAGE_CHANGE, &para, bat))
				bat->last_voltage_report = bat->cur_voltage;
		}

		if ( bat_report_debounce(bat->last_cap_report,    bat->cur_cap, &bat->report_ctr[INDEX_CAP])
			|| (bat->last_cap_report == BAT_CAP_RESERVE))
		{
			para.cap = bat->cur_cap;
			if (bat_charge_callback(BAT_CHG_EVENT_CAP_CHANGE, &para, bat))
				bat->last_cap_report = bat->cur_cap;
		}
	}

}

void dc5v_status_check(struct acts_battery_info *bat)
{
	uint8_t temp_dc5v_status;

	if (!sys_test_bit(MULTI_USED, MULTI_USED_UVLO))
	{
		temp_dc5v_status = 0;
	}
	else
	{
		temp_dc5v_status = 1;
	}

	if (bat->cur_dc5v_status != temp_dc5v_status )
	{
		bat->cur_dc5v_status = temp_dc5v_status;
	}
	if (bat->last_dc5v_report != temp_dc5v_status)
	{
		if (bat_charge_callback((temp_dc5v_status ? BAT_CHG_EVENT_DC5V_IN : BAT_CHG_EVENT_DC5V_OUT), NULL, bat))
			bat->last_dc5v_report = temp_dc5v_status;
	}
}


u8_t debounce(u8_t* debounce_buf, int buf_size, u8_t value)
{
    int  i;

    if (buf_size == 0)
        return 1;

    for (i = 0; i < buf_size - 1; i++)
    {
        debounce_buf[i] = debounce_buf[i+1];
    }

    debounce_buf[buf_size - 1] = value;

    for (i = 0; i < buf_size; i++)
    {
        if (debounce_buf[i] != value)
            return 0;
    }

    return 1;
}

int charge_status_need_debounce(u32_t charge_status)
{
	if ((charge_status == POWER_SUPPLY_STATUS_CHARGING)
		|| (charge_status == POWER_SUPPLY_STATUS_FULL))
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

void battery_acts_charge_status_check(struct acts_battery_info *bat, struct device *dev)
{
	const struct acts_battery_config *bat_cfg = dev->config->config_info;
	u32_t charge_status;

	charge_status = battery_get_charge_status(bat, bat_cfg);

	if ((charge_status_need_debounce(charge_status))
		&& (debounce(bat->chg_debounce_buf, sizeof(bat->chg_debounce_buf), charge_status) == FALSE))
	{
		return;
	}

	if (charge_status != bat->cur_chg_status)
	{
		/* 如果突然插入或者拔出电池，需要重置一下电压记录
		*/
		if ((bat->cur_chg_status == POWER_SUPPLY_STATUS_BAT_NOTEXIST) || (charge_status == POWER_SUPPLY_STATUS_BAT_NOTEXIST))
		{
			bat_voltage_record_reset(bat);
		}
		bat_voltage_record_reset(bat);
		SYS_LOG_INF("%d->%d", bat->cur_chg_status, charge_status);
		bat->cur_chg_status = charge_status;
	}

	if (charge_status != bat->last_chg_report )
	{
		if (charge_status == POWER_SUPPLY_STATUS_FULL)
		{
			if (bat_charge_callback(BAT_CHG_EVENT_CHARGE_FULL , NULL, bat))
				bat->last_chg_report = charge_status;
		}
		else if(charge_status == POWER_SUPPLY_STATUS_CHARGING)
		{
			if (bat_charge_callback(BAT_CHG_EVENT_CHARGE_START, NULL, bat))
				bat->last_chg_report = charge_status;
		}
		else if(charge_status == POWER_SUPPLY_STATUS_DISCHARGE)
		{
			if (bat_charge_callback(BAT_CHG_EVENT_CHARGE_STOP, NULL, bat))
				bat->last_chg_report = charge_status;
		}
	}
}


static void battery_acts_poll(struct k_timer *timer)
{
	struct device *dev = k_timer_user_data_get(timer);
	struct acts_battery_info *bat = dev->driver_data;

	battery_acts_charge_status_check(bat, dev);

	dc5v_status_check(bat);

	battery_acts_voltage_poll(bat);
}

static int battery_acts_get_property(struct device *dev,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	struct acts_battery_info *bat = dev->driver_data;

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		//val->intval = battery_get_charge_status(bat, dev->config->config_info);
		val->intval = bat->cur_chg_status;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = 1;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = battery_get_volt(bat) * 1000;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = battery_get_capacity(bat);
		break;
	case POWER_SUPPLY_PROP_DC5V:
		val->intval = !!sys_test_bit(MULTI_USED, MULTI_USED_UVLO);
		break;
	default:
		return -EINVAL;
	}

	//SYS_LOG_INF("%d, %d", psp, val->intval);
	return 0;
}

static void battery_acts_set_property(struct device *dev,
				       enum power_supply_property psp,
				       union power_supply_propval *val)
{
	/* do nothing */
}

bool bat_charge_callback(bat_charge_event_t event, bat_charge_event_para_t *para, struct acts_battery_info *bat)
{
	//ACT_LOG_ID_INF(ALF_STR_bat_charge_callback__EVENT_D_NOTIFY_08XN, 2, event, bat->notify);
	if (bat->notify) {
		bat->notify(event, para);
	}else {
		return FALSE;
	}

	return 	TRUE;
}

static void battery_acts_register_notify(struct device *dev, bat_charge_callback_t cb)
{
	struct acts_battery_info *bat = dev->driver_data;
	int flag;

	flag = irq_lock();
	ACT_LOG_ID_INF(ALF_STR_battery_acts_register_notify__08XN, 1, cb);
	if ((bat->notify == NULL) && cb)
	{
		bat->notify = cb;
	}
	else
	{
		SYS_LOG_ERR("notify func already exist!\n");
	}
	irq_unlock(flag);
}

static void battery_acts_enable(struct device *dev)
{
	struct acts_battery_info *bat = dev->driver_data;
	const struct acts_battery_config *cfg = dev->config->config_info;

	ACT_LOG_ID_DBG(ALF_STR_battery_acts_enable__ENABLE_BATTERY_DETEC, 0);

	adc_enable(bat->adc_dev);
	k_timer_start(&bat->timer, cfg->poll_interval_ms,
		cfg->poll_interval_ms);
}

static void battery_acts_disable(struct device *dev)
{
	struct acts_battery_info *bat = dev->driver_data;

	ACT_LOG_ID_DBG(ALF_STR_battery_acts_disable__DISABLE_BATTERY_DETE, 0);

	k_timer_stop(&bat->timer);
	adc_disable(bat->adc_dev);
}

void battery_acts_sample_buf_init(struct acts_battery_info *bat)
{
    int i;

    /* sample BAT ADC data in 10ms */
    for (i = 0; i < BAT_VOLTAGE_TBL_DCNT; i++) {
		sample_one_voltage(bat);
        k_sleep(10);
    }
}


static const struct power_supply_driver_api battery_acts_driver_api = {
	.get_property = battery_acts_get_property,
	.set_property = battery_acts_set_property,
	.register_notify = battery_acts_register_notify,
	.enable = battery_acts_enable,
	.disable = battery_acts_disable,
};

static int battery_acts_init(struct device *dev)
{
	struct acts_battery_info *bat = dev->driver_data;
	const struct acts_battery_config *cfg = dev->config->config_info;

	ACT_LOG_ID_INF(ALF_STR_battery_acts_init__BATTERY_INITIALIZEDN, 0);

	bat->adc_dev = device_get_binding(cfg->adc_name);
	if (!bat->adc_dev) {
		SYS_LOG_ERR("cannot found ADC device \'batadc\'\n");
		return -ENODEV;
	}

	bat->seq_entrys.sampling_delay = 0;
	bat->seq_entrys.buffer = (u8_t *)&bat->adcval;
	bat->seq_entrys.buffer_length = sizeof(bat->adcval);
	bat->seq_entrys.channel_id = cfg->adc_chan;
	bat->seq_tbl.entries = &bat->seq_entrys;
	bat->seq_tbl.num_entries = 1;
	bat->cap_tbl = battery_cap_tbl;
	bat_voltage_record_reset(bat);
	adc_enable(bat->adc_dev);

#ifdef BOARD_BATTERY_CHARGING_STATUS_GPIO
	bat->gpio_dev = device_get_binding(cfg->gpio_name);
	if (!bat->gpio_dev) {
		SYS_LOG_ERR("cannot found GPIO device \'batadc\'\n");
		return -ENODEV;
	}
	gpio_pin_configure(bat->gpio_dev, cfg->pin, GPIO_DIR_IN);
	//ACT_LOG_ID_INF(ALF_STR_battery_acts_init__GPIO15_XN, 1, sys_read32(0xc0090000 + 0x40));
#endif

	bat->cur_chg_status = POWER_SUPPLY_STATUS_UNKNOWN;
	bat->last_cap_report = BAT_CAP_RESERVE;
	bat->last_voltage_report = BAT_VOLTAGE_RESERVE;

	battery_acts_sample_buf_init(bat);

	//dev->driver_api = &battery_acts_driver_api;

	k_timer_init(&bat->timer, battery_acts_poll, NULL);
	k_timer_user_data_set(&bat->timer, dev);
	k_timer_start(&bat->timer, 10, cfg->poll_interval_ms);

	return 0;
}

static struct acts_battery_info battery_acts_ddata;

static const struct acts_battery_config battery_acts_cdata = {
	.adc_name = CONFIG_BATTERY_ACTS_ADC_NAME,
	.adc_chan = CONFIG_BATTERY_ACTS_ADC_CHAN,
	.gpio_name = CONFIG_GPIO_ACTS_DEV_NAME,
#ifdef BOARD_BATTERY_CHARGING_STATUS_GPIO
	.pin = BOARD_BATTERY_CHARGING_STATUS_GPIO,
#endif
	.poll_interval_ms = BATTERY_DRV_POLL_INTERVAL,
};


DEVICE_AND_API_INIT(battery, "battery", battery_acts_init,
	    &battery_acts_ddata, &battery_acts_cdata, POST_KERNEL,
	    CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &battery_acts_driver_api);
