/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file system reboot interface for Actions SoC
 */

#include <kernel.h>
#include <device.h>
#include <init.h>
#include <watchdog_hal.h>
#include <misc/printk.h>
#include <soc.h>
#include <mem_manager.h>
#include <logging/sys_log.h>

#ifdef CONFIG_RTC_ACTS
#include <rtc.h>
#endif

#ifdef CONFIG_LOGIC_MCU_LS8A10023T
#include <logic_mcu_ls8a10023t.h>
#endif

#define ACT_LOG_MODEL_ID ALF_MODEL_SOC_PM

#define UPDATE_MAGIC  0xA596
#define UPDATE_OK	  0x5A69

#define PMU_POWEROFF_MAGIC 0x504F4646 /* POFF */

#ifdef CONFIG_PM_SLEEP_TIME_TRACE
static u32_t idle_start_time;
static u32_t idle_total_time;
#endif

/*
**  刷新rtc  域寄存器
*/
void sys_pm_update_rtc_regs(void)
{
    sys_write32(UPDATE_MAGIC, RTC_REGUPDATE);

    while (sys_read32(RTC_REGUPDATE) != UPDATE_OK)
    {
        ;    // wait rtc register update
    }
}

bool sys_pm_get_power_5v_status(void)
{
	/* From zhongxu@actionstech.com, April 10 2024
	 Use DC5VADC_DATA instead of MULTI_USED:UVLO.

	 About MULTI_USED:UVLO
	 When "dc5v < bat + 30mv", it's assumed that DC5V is pluged out, which is too 
	 sensitive and there is no debounce.

	 About DC5VADC_DATA:[9~0]
		n = V/((6/2^10)*Vref/1.5)
		Vref = 1.5
	 Use 3V as the threshold to judge 5v power plug in/out, the number is 512.
	*/

	//return (sys_read32(MULTI_USED) & (1 << 9));
	return (sys_read32(DC5VADC_DATA) & 0x3FF) > 512 ? true : false;
}

/*
**  配置onoff  长按时间
*/
void sys_pm_config_onoff_time(uint32_t value_ms)
{
	uint32_t val,val_tmp;

    /*select the nearest val set to reg*/
    if (value_ms < 187){
        val_tmp = 0;
    }else if (value_ms < 375){
        val_tmp = 1;
    }else if (value_ms < 750){
        val_tmp = 2;
    }else if (value_ms < 1250){
        val_tmp = 3;
    }else if (value_ms < 1750){
        val_tmp = 4;
    }else if (value_ms < 2500){
        val_tmp = 5;
    }else if (value_ms < 3500){
        val_tmp = 6;
    }else {
        val_tmp = 7;
    }

	/* config on/off key long press time */
	val = sys_read32(PMU_ONOFF_KEY);
	val &= ~(0x7 << 7);
	val |= ((val_tmp & 0x07) << 7);
	sys_write32(val, PMU_ONOFF_KEY);
}

void sys_pm_clear_wakeup_pending(void)
{
	uint32_t val;

	uint32_t start_cycle;

	val = sys_read32(PMU_WAKE_PD);
	val |= (0x1ffffff);
	sys_write32(val, PMU_WAKE_PD);

	start_cycle = k_cycle_get_32();

	while(1){
        if((sys_read32(PMU_WAKE_PD) & 0x1ffffff) == 0){
            break;
        }

        //最多等待500us
        if((k_cycle_get_32() - start_cycle) > 500 * 24){
            break;
        }
	}
}

int __ramfunc sys_pm_get_wakeup_pending(void)
{
	int val;

	val = sys_read32(PMU_WAKE_PD);

    //if(val != 0){
    //    ACT_LOG_ID_INF(ALF_STR_sys_pm_get_wakeup_pending__D_GET_WAKEUP_PEND_0X, 2, __LINE__, val);
    //}

	return val;
}

u32_t sys_pm_get_backup_wakeup_pending(void)
{
	static u32_t pmu_wakeup_pending;

	if (!pmu_wakeup_pending) {
		pmu_wakeup_pending = sys_pm_get_wakeup_pending();
	}

	return pmu_wakeup_pending;
}

int sys_pm_get_wakeup_source(union sys_pm_wakeup_src *src)
{
	u32_t wk_pd;

	if (!src)
		return -EINVAL;

	src->data = 0;

	wk_pd = sys_read32(PMU_WAKE_PD);

	printk("PMU wakeup pending:0x%08x\n", wk_pd);

	if ((wk_pd & PMU_WAKE_PD_LONG_WK_PD) || (wk_pd & PMU_WAKE_PD_SHORT_WK_PD))
		src->t.onoff = 1;

	if (wk_pd & PMU_WAKE_PD_ONOFFRESET_PD)
		src->t.reset = 1;

	if (wk_pd & PMU_WAKE_PD_BT_WK_PD)
		src->t.bat = 1;

	if (wk_pd & PMU_WAKE_PD_HDSWON_PD)
		src->t.hdswon = 1;

	if (wk_pd & PMU_WAKE_PD_BT_WK_PD)
		src->t.bt_wk = 1;

	if (wk_pd & PMU_WAKE_PD_REMOTE_PD)
		src->t.remote = 1;

	if (wk_pd & PMU_WAKE_PD_IRC_WK_PD)
		src->t.irc = 1;

	if (soc_boot_get_watchdog_is_reboot() == 1)
		src->t.watchdog = 1;

#ifdef CONFIG_RTC_ACTS
	if (rtc_is_alarm_wakeup(device_get_binding(CONFIG_RTC_0_NAME)))
		src->t.alarm = 1;
#endif

	return 0;
}

/*
**  设置系统可唤醒源
*/
void sys_pm_set_wakeup_src(uint8_t mode)
{
	uint32_t key, val;

	key = irq_lock();

	sys_write32(sys_read32(PMU_WAKE_PD), PMU_WAKE_PD);

	val = sys_read32(PMU_WKEN_CTL);
	// disable all wakeup source
	val &= ~(0x3FFF);

	val = PMU_WKEN_CTL_RESET_WKEN
#ifdef CONFIG_SOC_WKEN_BAT
	      | PMU_WKEN_CTL_BATWK_EN
#endif
#ifdef CONFIG_SOC_WKEN_ONOFF_SHORT
	      | PMU_WKEN_CTL_SHORT_WKEN
#endif
#ifdef CONFIG_SOC_WKEN_DC5V
	      | PMU_WKEN_CTL_DC5VON_EN
#endif
	      | PMU_WKEN_CTL_LONG_WKEN;

	if(mode == 0) {
		// set wakeup source from S4
	} else {
		// set wakeup source from S3BT or S2
		val |= PMU_WKEN_CTL_REMT_EN | PMU_WKEN_CTL_SHORT_WKEN | PMU_WKEN_CTL_BT_EN;
	}

	sys_write32(val, PMU_WKEN_CTL);

	sys_pm_config_onoff_time(CONFIG_SOC_WKEN_ONOFF_LONG_PRESS_TIMER);

	irq_unlock(key);
}

/*
**  系统关机
*/
void sys_pm_poweroff(void)
{
	uint32_t key;
	uint32_t wake_pd;
    uint32_t key_pressed;

    do{
#ifdef CONFIG_WDT_ACTS_DEVICE_NAME
        watchdog_clear();
#endif
        /* bits define */
        #define PMU_ONOFF_KEY_ONOFF_PRESSED	(1 << 0)
	    key_pressed = !!(sys_read32(PMU_ONOFF_KEY) & PMU_ONOFF_KEY_ONOFF_PRESSED);
    }while(key_pressed);
	printk("system power down!\n");

	sys_pm_set_wakeup_src(0);

	key = irq_lock();

#ifdef CONFIG_WDT_ACTS_DEVICE_NAME
	watchdog_stop();
#endif

#ifdef CONFIG_LOGIC_MCU_LS8A10023T
	printk("[%s,%d] logic_mcu_ls8a10023t start\n", __FUNCTION__, __LINE__);
	
	struct device *logic_mcu_ls8a10023t_dev = device_get_binding(CONFIG_LOGIC_MCU_LS8A10023T_DEV_NAME);
	if (logic_mcu_ls8a10023t_dev == NULL)
	{
		printk("[%s,%d] logic_mcu_ls8a10023t_dev is null, error\n", __FUNCTION__, __LINE__);
		return;
	}
	
	logic_mcu_ls8a10023t_int_clear(logic_mcu_ls8a10023t_dev);
	logic_mcu_ls8a10023t_power_off_ready(logic_mcu_ls8a10023t_dev);

	printk("[%s,%d] logic_mcu_ls8a10023t end\n", __FUNCTION__, __LINE__);
#endif


	while(1) {
		sys_write32(0x0, PMU_POWER_CTL);

		/* wait 10ms */
		k_busy_wait(10000);

		/* cannot enter s3/4, poll wakeup source */
		wake_pd = sys_read32(PMU_WAKE_PD) & PMU_WAKE_PD_ONOFF0_L_PD;
		if (wake_pd) {
			printk("poweroff: wake_pd 0x%x, need reboot!\n", wake_pd);
			sys_pm_reboot(0);
		}
	}

	/* never return... */
}

u32_t sys_pm_get_rc_timestamp(void)
{
	/* 3.2K RC timer */
	return (sys_read32(0xc017601c) * 10 / 32);
}

/* 通过该函数可以看出timer pending是否置位 */
//static u32_t sys_pm_get_rc_pending(void)
//{
//    /* 3.2K RC timer pending */
//    return (sys_read32(0xc017602c) & (1 << 5));
//}

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
static uint32_t sysclk_bak;

static void sys_pm_enter_low_clock(void)
{
    sysclk_bak = sys_read32(CMU_SYSCLK);
	/* switch cpu to 1.5M */
	sys_write32((sys_read32(CMU_SYSCLK) & ~(0xf << 8)) | (0x7 << 8), CMU_SYSCLK);
	sys_write32((sys_read32(CMU_SYSCLK) & ~0x7) | 0x01, CMU_SYSCLK);
}

static void sys_pm_exit_low_clock(void)
{
	/* switch cpu to 56M */
	sys_write32(sysclk_bak, CMU_SYSCLK);
}
#endif

int sys_pm_temp_adc_get_temperature(void)
{

    int adc_temp;
	int temp;
    adc_temp = sys_pm_temp_adc_get_data();

    // T=160/1024*[data(十进制)]-40° +effuse(21:17)的offset  算出温度
    temp = (int)(((s64_t)(1600 * adc_temp) / 1024) - 400) / 10;

	//printk("%s temp %d\n", __func__, temp);
	return temp;
}

#ifdef CONFIG_SOC_DVFS_CPU_IDLE_LOW_POWER
static unsigned int origin_cpu_div;

void soc_dvfs_cpu_enter_lowpower(void)
{
	unsigned int core_freq, cpu_idle_div;

#ifdef CONFIG_PM_SLEEP_TIME_TRACE
	idle_start_time = sys_pm_get_rc_timestamp();
#endif

	core_freq = soc_freq_get_core_freq();

    //如果corepll已经关闭，说明在低功耗状态下，这个时候idle运行可以把时钟降到3M
	if(core_freq){
        cpu_idle_div = (16 * (CONFIG_SOC_DVFS_CPU_IDLE_FREQ_KHZ / 1000) - 1) / core_freq;
        if (cpu_idle_div > 15)
            cpu_idle_div = 15;

        origin_cpu_div = (sys_read32(CMU_SYSCLK) >> 8) & 0xf;
        sys_write32((sys_read32(CMU_SYSCLK) & ~0xf00) | (cpu_idle_div << 8), CMU_SYSCLK);
	}else{
        sys_pm_enter_low_clock();
	}
}

void soc_dvfs_cpu_exit_lowpower(void)
{
    unsigned int core_freq;

	core_freq = soc_freq_get_core_freq();

    if(core_freq){
        sys_write32((sys_read32(CMU_SYSCLK) & ~0xf00) | (origin_cpu_div << 8), CMU_SYSCLK);
    }else{
        sys_pm_exit_low_clock();
    }

#ifdef CONFIG_PM_SLEEP_TIME_TRACE
    idle_total_time += sys_pm_get_rc_timestamp() - idle_start_time;
#endif

}

#ifdef CONFIG_PM_SLEEP_TIME_TRACE
int sys_pm_get_idle_sleep_time(void)
{
    return idle_total_time;
}
#endif

#endif

/*
**  系统重启
*/
void sys_pm_reboot(int type)
{
	unsigned int key;

	printk("system reboot, type 0x%x!\n", type);

	key = irq_lock();

	/* store reboot reason in RTC_BAK3 for bootloader */
	sys_write32((REBOOT_REASON_MAGIC << 16) | (type & 0xffff), RTC_BAK3);
	sys_pm_update_rtc_regs();

	k_busy_wait(500);

	sys_write32(0x10, WD_CTL);

	while (1) {
		;
	}

	/* never return... */
}

void sys_pm_set_reboot_reason(int type)
{
	unsigned int key;
	key = irq_lock();

	/* store reboot reason in RTC_BAK3 for bootloader */
	sys_write32((REBOOT_REASON_MAGIC << 16) | (type & 0xffff), RTC_BAK3);
	sys_pm_update_rtc_regs();

	k_busy_wait(500);

	irq_unlock(key);

	return;
}

void soc_pm_reboot(int type, int rtc_bak_reg_num)
{
	unsigned int key;

	//printk("soc reboot, back type 0x%x @ RTC_BAK%d!\n", type, rtc_bak_reg_num);

	key = irq_lock();

	sys_write32((REBOOT_REASON_MAGIC << 16) | (type & 0xffff), (RTC_BAK0+rtc_bak_reg_num*4));
	sys_pm_update_rtc_regs();

	k_busy_wait(500);

	sys_write32(0x10, WD_CTL);

	while (1) {
		;
	}
	/* never return... */
}

int soc_pm_rtc_bak_read(int rtc_bak_reg_num)
{
    return sys_read32(RTC_BAK0+rtc_bak_reg_num*4);
}

int soc_pm_rtc_bak_write(int val, int rtc_bak_reg_num)
{
    sys_write32(val, RTC_BAK0+rtc_bak_reg_num*4);

    sys_pm_update_rtc_regs();

    return sys_read32(RTC_BAK0+rtc_bak_reg_num*4);
}

/*!
 * \brief 温度采样 ADC 开关控制
 */
void sys_pm_temp_adc_ctrl_enable(uint32_t enable)
{
    uint32_t adc_ctl = sys_read32(PMUADC_CTL);

    if (enable)
    {
        if (!(adc_ctl & (1 << PMUADC_CTL_SENSORADC_EN)))
        {
            adc_ctl |=  (1 << PMUADC_CTL_SENSORADC_EN);
        }
    }
    else
    {
        if (adc_ctl & (1 << PMUADC_CTL_SENSORADC_EN))
        {
            adc_ctl &= ~(1 << PMUADC_CTL_SENSORADC_EN);
        }
    }
    sys_write32(adc_ctl, PMUADC_CTL);
    adc_ctl = sys_read32(PMUADC_CTL);

}


/*!
 * \brief 获取温度adc data
 */
int sys_pm_temp_adc_get_data(void)
{
    int  temp  = 0;

    int  adc_val = ((sys_read32(SENSADC_DATA) & SENSADC_DATA_SENSADC_MASK) \
        >> SENSADC_DATA_SENSADC_SHIFT);

    temp = adc_val;

    return temp;
}

int sys_pm_get_reboot_reason(u16_t *reboot_type, u8_t *reason)
{
	int ret = -1;
#ifdef CONFIG_BOOTINFO_MEMORY
	u32_t reg_val;

	reg_val = soc_boot_get_reboot_reason();
	if ((reg_val >> 16) == REBOOT_REASON_MAGIC) {
		*reboot_type = (reg_val & 0xff00);
		*reason      = (reg_val & 0x00ff);
		ret = 0;
	}
#endif
	return ret;
}

static void pmu_dcdc_adjust(void)
{
	u32_t delta;
	int err;

	err = soc_atp_read_bits(&delta, 125, 2);
	if (0 == err) {
		if (delta == 1) {
			sys_write32((sys_read32(DCDC_CTL2) & (~0xf)) | 0xc, DCDC_CTL2);
		} else if (delta == 3) {
			sys_write32((sys_read32(DCDC_CTL2) & (~0xf)) | 0xa, DCDC_CTL2);
		}
	}
}

static int soc_pm_init(struct device *arg)
{
	ARG_UNUSED(arg);

#if !defined(CONFIG_SOC_WKEN_BAT) && !defined(CONFIG_SOC_AUTO_POWER_ON)
	u32_t wk_pd;
	u32_t rtc_bak2;

	wk_pd = sys_pm_get_wakeup_pending();

	/* backup wakeup pending */
	sys_pm_get_backup_wakeup_pending();

	rtc_bak2 = sys_read32(RTC_BAK2);

	printk("PMU wakeup pending:0x%x rtc_bak2:0x%x\n", wk_pd, rtc_bak2);

	if ((wk_pd & 0x392A5E) && (!rtc_bak2 || (rtc_bak2 == PMU_POWEROFF_MAGIC))) {
		sys_write32(0, RTC_BAK2);
		sys_pm_update_rtc_regs();
		k_busy_wait(500);
	} else if (((wk_pd & PMU_WAKE_PD_POWEROK_PD)
		&& (rtc_bak2 == PMU_POWEROFF_MAGIC || !rtc_bak2))
		|| (wk_pd & PMU_WAKE_PD_BAT_PD)) {
		sys_write32(PMU_POWEROFF_MAGIC, RTC_BAK2);
		sys_pm_update_rtc_regs();
#ifndef CONFIG_SRRC_TEST
		sys_pm_poweroff();
#endif
	}
#endif

	pmu_spll_init();

	pmu_dcdc_adjust();

	return 0;
}

SYS_INIT(soc_pm_init, POST_KERNEL, 20);
