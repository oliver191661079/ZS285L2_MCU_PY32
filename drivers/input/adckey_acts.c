/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief ADC Keyboard driver for Actions SoC
 */

#include <errno.h>
#include <kernel.h>
#include <string.h>
#include <init.h>
#include <irq.h>
#include <adc.h>
#include <input_dev.h>
#include <misc/util.h>
#include <misc/byteorder.h>
#include <board.h>
#include <gpio.h>

#ifdef CONFIG_LOGIC_MCU_LS8A10023T
#include <logic_mcu_ls8a10023t.h>
#endif

#define SYS_LOG_DOMAIN "ADC_KEY"
#define SYS_LOG_LEVEL CONFIG_SYS_LOG_INPUT_DEV_LEVEL
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_ADCKEY_ACTS

struct adckey_map {
	u16_t key_code;
	u16_t adc_val;
};

struct acts_adckey_data {
	struct k_timer timer;

	struct device *adc_dev;
	struct adc_seq_table seq_tbl;
	struct adc_seq_entry seq_entrys;
	u8_t adc_buf[4];

	int scan_count;
	u16_t prev_keycode;
	u16_t prev_stable_keycode;

	input_notify_t notify;

#ifdef CONFIG_INPUT_DEV_ACTS_GPIOKEY
	struct device *gpio_dev;
#ifdef CONFIG_GPIO_WIO_ACTS
	struct device *wio_dev;
#endif
#endif
};

#ifdef CONFIG_INPUT_DEV_ACTS_GPIOKEY
struct gpiokey_map {
	u16_t key_code;
	u16_t gpio_pin;
};
#endif

struct acts_adckey_config {
	char *adc_name;
	u16_t adc_chan;

	u16_t poll_interval_ms;
	u16_t sample_filter_dep;

	u16_t key_cnt;
	const struct adckey_map *key_maps;
#ifdef CONFIG_INPUT_DEV_ACTS_GPIOKEY
	u16_t gpiokey_cnt;
	const struct gpiokey_map *gpiokey_maps;
#endif
};

#ifdef CONFIG_BOARD_ATS2875H_RIDE
#ifdef CONFIG_INPUT_DEV_ACTS_GPIOKEY
#if (1 == CONFIG_BOARD_ATS2875H_RIDE_VERSION)
#define IOKEY_DOWN_VALUE	0 //IOKEY按下有效电平
#else
#define IOKEY_DOWN_VALUE	1 //IOKEY按下有效电平
#endif

//返回按键按下还是抬起，1按下，0抬起
bool key_vol_down_status_read(void)
{
	struct device *gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);

	u32_t val;

	gpio_pin_read(gpio_dev, KEY_VOL_DOWN_PIN, &val);

	return (val == IOKEY_DOWN_VALUE);
}

//返回按键按下还是抬起，1按下，0抬起
bool key_vol_up_status_read(void)
{
	struct device *gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);

	u32_t val;
	
	gpio_pin_read(gpio_dev, KEY_VOL_UP_PIN, &val);

	return (val == IOKEY_DOWN_VALUE);
}

//返回按键按下还是抬起，1按下，0抬起
bool key_play_pause_status_read(void)
{
	struct device *gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);

	u32_t val;
	
	gpio_pin_read(gpio_dev, KEY_PLAY_PAUSE_PIN, &val);

	return (val == IOKEY_DOWN_VALUE);
}

//返回按键按下还是抬起，1按下，0抬起
bool key_bt_status_read(void)
{
	struct device *gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);

	u32_t val;
	
	gpio_pin_read(gpio_dev, KEY_BT_PIN, &val);

	return (val == IOKEY_DOWN_VALUE);
}
#endif
#endif

#ifdef CONFIG_INPUT_DEV_ACTS_GPIOKEY
#ifndef CONFIG_BOARD_ATS2875H_RIDE
static u16_t gpiokey_acts_get_keycode(const struct acts_adckey_config *cfg,
					struct acts_adckey_data *adckey,
					int adc_val)
{
	const struct gpiokey_map *map = cfg->gpiokey_maps;
	int i, value;

	for (i = 0; i < cfg->gpiokey_cnt; i++) {
		if(map->gpio_pin < GPIO_MAX_PIN_NUM)
		{
			gpio_pin_read(adckey->gpio_dev, map->gpio_pin, &value);
		}
		else
		{
#ifdef CONFIG_GPIO_WIO_ACTS
			gpio_pin_read(adckey->wio_dev, map->gpio_pin - GPIO_MAX_PIN_NUM, &value);
#else
			value = 0;
#endif
		}

		if(value != 0) {
			//printk("%d GPIO%d key 0x%x pressed\n", i, map->gpio_pin, map->key_code);
			return map->key_code;
		}
		map++;
	}

	return KEY_RESERVED;
}
#else
static u16_t gpiokey_acts_get_keycode(const struct acts_adckey_config *cfg,
					struct acts_adckey_data *adckey,
					int adc_val)
{
	u16_t ret = KEY_RESERVED;

	struct device *dev = device_get_binding(CONFIG_LOGIC_MCU_LS8A10023T_DEV_NAME);
	
	if (logic_mcu_ls8A10023t_get_power_key_flag(dev))
	{
		printk("---logic_mcu_ls8A10023t_get_power_key_flag---\n");
		ret = KEY_POWER;
	}
	//3个组合键的情况
	else if(key_bt_status_read() && key_vol_down_status_read() && key_vol_up_status_read())
    {
    }
	//2个组合键的情况
	else if(key_play_pause_status_read() && key_vol_up_status_read())
    {
    }
	else if(key_vol_up_status_read() && key_vol_down_status_read())
    {
    }
	else if(key_bt_status_read() && key_play_pause_status_read())
    {
    }
	//1个按键的情况
	else if (key_vol_down_status_read())
	{
		ret = KEY_VOLUMEDOWN;
	}
	else if (key_vol_up_status_read())
	{
		ret = KEY_VOLUMEUP;
	}
	else if (key_play_pause_status_read())
	{
		ret = KEY_PAUSE_AND_RESUME;
	}
	else if (key_bt_status_read())
	{
		ret = KEY_BT;
	}

	if (ret != KEY_RESERVED)
	{
		// printk("[%s,%d] keycode:%d\n", __FUNCTION__, __LINE__, ret);
	}

	return ret;
}
#endif
#endif

#ifdef CONFIG_INPUT_DEV_ACTS_GPIOKEY_ONLY
#else
/* adc_val -> key_code */
static u16_t adckey_acts_get_keycode(const struct acts_adckey_config *cfg,
				     struct acts_adckey_data *adckey,
				     int adc_val)
{
	const struct adckey_map *map = cfg->key_maps;
	int i;

	for (i = 0; i < cfg->key_cnt; i++) {
		if (adc_val < map->adc_val) {
			//printk("i=%d, map-adc=0x%x\n", i, map->adc_val);
			return map->key_code;
		}

		map++;
	}

	return KEY_RESERVED;
}
#endif

static void adckey_acts_report_key(struct acts_adckey_data *adckey,
				   int key_code, int value)
{
	struct input_value val;

	if (adckey->notify) {
		val.type = EV_KEY;
		val.code = key_code;
		val.value = value;

		//printk("code=0x%x, value=0x%x\n", key_code, value);
		adckey->notify(NULL, &val);
	}
}

static void adckey_acts_poll(struct k_timer *timer)
{
	struct device *dev = k_timer_user_data_get(timer);
	const struct acts_adckey_config *cfg = dev->config->config_info;
	struct acts_adckey_data *adckey = dev->driver_data;
	u16_t keycode, adcval;

#ifdef CONFIG_INPUT_DEV_ACTS_GPIOKEY_ONLY
	keycode = KEY_RESERVED;
#else
	/* get adc value */
	adc_read(adckey->adc_dev, &adckey->seq_tbl);
	adcval = sys_get_le16(adckey->seq_entrys.buffer);
	/* get keycode */
	keycode = adckey_acts_get_keycode(cfg, adckey, adcval);
	//printk("adc key val=0x%x, code=0x%x\n", adcval, keycode);
#endif

#ifdef CONFIG_INPUT_DEV_ACTS_GPIOKEY
	if(keycode == KEY_RESERVED)
	{
		keycode = gpiokey_acts_get_keycode(cfg, adckey, adcval);
		//printk("gpio key code=0x%x\n", keycode);
	}
#endif

	/* no key is pressed or releasing */
	if (keycode == KEY_RESERVED &&
	    adckey->prev_stable_keycode == KEY_RESERVED)
		return;

	if (keycode == adckey->prev_keycode) {
		adckey->scan_count++;
		if (adckey->scan_count == cfg->sample_filter_dep) {
			/* previous key is released? */
			if (adckey->prev_stable_keycode != KEY_RESERVED
				&& keycode != adckey->prev_stable_keycode)
				adckey_acts_report_key(adckey,
					adckey->prev_stable_keycode, 0);

			/* a new key press? */
			if (keycode != KEY_RESERVED)
				adckey_acts_report_key(adckey, keycode, 1);

			/* clear counter for new checking */
			adckey->prev_stable_keycode = keycode;
			adckey->scan_count = 0;
		}
	} else {
		/* new key pressed? */
		adckey->prev_keycode = keycode;
		adckey->scan_count = 0;
	}
}

static void adckey_acts_enable(struct device *dev)
{
	const struct acts_adckey_config *cfg = dev->config->config_info;
	struct acts_adckey_data *adckey = dev->driver_data;

	ACT_LOG_ID_DBG(ALF_STR_adckey_acts_enable__ENABLE_ADCKEY, 0);

	adc_enable(adckey->adc_dev);
	k_timer_start(&adckey->timer, cfg->poll_interval_ms,
		cfg->poll_interval_ms);
}

static void adckey_acts_disable(struct device *dev)
{
	struct acts_adckey_data *adckey = dev->driver_data;

	ACT_LOG_ID_DBG(ALF_STR_adckey_acts_disable__DISABLE_ADCKEY, 0);

	k_timer_stop(&adckey->timer);
	adc_disable(adckey->adc_dev);

	//adckey->notify = NULL;
}

static void adckey_acts_register_notify(struct device *dev, input_notify_t notify)
{
	struct acts_adckey_data *adckey = dev->driver_data;

	ACT_LOG_ID_DBG(ALF_STR_adckey_acts_register_notify__REGISTER_NOTIFY_0XX, 1, (u32_t)notify);

	adckey->notify = notify;
}

static void adckey_acts_unregister_notify(struct device *dev, input_notify_t notify)
{
	struct acts_adckey_data *adckey = dev->driver_data;

	ACT_LOG_ID_DBG(ALF_STR_adckey_acts_unregister_notify__UNREGISTER_NOTIFY_0X, 1, (u32_t)notify);

	adckey->notify = NULL;
}

const struct input_dev_driver_api adckey_acts_driver_api = {
	.enable = adckey_acts_enable,
	.disable = adckey_acts_disable,
	.register_notify = adckey_acts_register_notify,
	.unregister_notify = adckey_acts_unregister_notify,
};

int adckey_acts_init(struct device *dev)
{
	const struct acts_adckey_config *cfg = dev->config->config_info;
	struct acts_adckey_data *adckey = dev->driver_data;

	adckey->adc_dev = device_get_binding(cfg->adc_name);
	if (!adckey->adc_dev) {
		SYS_LOG_ERR("cannot found adc dev %s\n", cfg->adc_name);
		return -ENODEV;
	}

	adckey->seq_entrys.sampling_delay = 0;
	adckey->seq_entrys.buffer = &adckey->adc_buf[0];
	adckey->seq_entrys.buffer_length = sizeof(adckey->adc_buf);
	adckey->seq_entrys.channel_id = cfg->adc_chan;

	adckey->seq_tbl.entries = &adckey->seq_entrys;
	adckey->seq_tbl.num_entries = 1;

#ifdef CONFIG_INPUT_DEV_ACTS_GPIOKEY
	adckey->gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	if (!adckey->gpio_dev) {
		SYS_LOG_ERR("cannot found adc dev %s\n", CONFIG_GPIO_ACTS_DEV_NAME);
		return -ENODEV;
	}
#ifdef CONFIG_GPIO_WIO_ACTS
	adckey->wio_dev = device_get_binding(CONFIG_GPIO_WIO_ACTS_DEV_NAME);
	if (!adckey->wio_dev) {
		SYS_LOG_ERR("cannot found adc dev %s\n", CONFIG_GPIO_WIO_ACTS_DEV_NAME);
		return -ENODEV;
	}
#endif
	{
		const struct gpiokey_map *map = cfg->gpiokey_maps;
		int i;
		int flags;
#ifdef CONFIG_INPUT_DEV_ACTS_GPIOKEY_PULLUP_BY_INNER
		flags = GPIO_CTL_GPIO_INEN | GPIO_PUD_PULL_UP;
#else
		flags = GPIO_CTL_GPIO_INEN | GPIO_PUD_PULL_DOWN;
#endif
		for (i = 0; i < cfg->gpiokey_cnt; i++) {
			if(map->gpio_pin < GPIO_MAX_PIN_NUM)
				gpio_pin_configure(adckey->gpio_dev, map->gpio_pin, flags);
#ifdef CONFIG_GPIO_WIO_ACTS
			else
				gpio_pin_configure(adckey->wio_dev, map->gpio_pin - GPIO_MAX_PIN_NUM, flags);
#endif
			map++;
		}
	}
#endif

	k_timer_init(&adckey->timer, adckey_acts_poll, NULL);
	k_timer_user_data_set(&adckey->timer, dev);

	return 0;
}

static struct acts_adckey_data adckey_acts_ddata;

static const struct adckey_map adckey_acts_keymaps[] = {
	BOARD_ADCKEY_KEY_MAPPINGS
};

#ifdef CONFIG_INPUT_DEV_ACTS_GPIOKEY
static const struct gpiokey_map gpiokey_acts_keymaps[] = {
   BOARD_GPIOKEY_KEY_MAPPINGS
};
#endif

static const struct acts_adckey_config adckey_acts_cdata = {
	.adc_name = CONFIG_INPUT_DEV_ACTS_ADCKEY_ADC_NAME,
	.adc_chan = CONFIG_INPUT_DEV_ACTS_ADCKEY_ADC_CHAN,

	.poll_interval_ms = 20,
	.sample_filter_dep = 3,

	.key_cnt = ARRAY_SIZE(adckey_acts_keymaps),
	.key_maps = &adckey_acts_keymaps[0],
#ifdef CONFIG_INPUT_DEV_ACTS_GPIOKEY
	.gpiokey_cnt = ARRAY_SIZE(gpiokey_acts_keymaps),
	.gpiokey_maps = &gpiokey_acts_keymaps[0],
#endif
};

DEVICE_AND_API_INIT(adckey_acts, CONFIG_INPUT_DEV_ACTS_ADCKEY_NAME,
		    adckey_acts_init,
		    &adckey_acts_ddata, &adckey_acts_cdata,
		    PRE_KERNEL_2, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		    &adckey_acts_driver_api);
