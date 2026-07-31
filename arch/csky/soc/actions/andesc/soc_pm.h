/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file reboot configuration macros for Actions SoC
 */

#ifndef	_ACTIONS_SOC_REBOOT_H_
#define	_ACTIONS_SOC_REBOOT_H_

#define REBOOT_REASON_MAGIC 		0x4252	/* 'RB' */
#define REBOOT_TYPE_NORMAL		0x0
#define REBOOT_TYPE_GOTO_ADFU		0x1000
#define REBOOT_TYPE_GOTO_BTSYS		0x1100
#define REBOOT_TYPE_GOTO_WIFISYS	0x1200
#define REBOOT_TYPE_GOTO_SYSTEM		0x1200
#define REBOOT_TYPE_GOTO_RECOVERY	0x1300
#define REBOOT_TYPE_GOTO_CARDBOOT   0x1400
#define REBOOT_TYPE_OTA_DONE        0x1500

// wakeup enable
#define PMU_WKEN_CTL_IR_EN			(0x1 << 13)
#define PMU_WKEN_CTL_BATWK_EN		(0x1 << 12)
#define PMU_WKEN_CTL_REMT_EN		(0x1 << 11)
#define PMU_WKEN_CTL_UVLO_EN		(0x1 << 10)
#define PMU_WKEN_CTL_NFC_EN			(0x1 << 9)
#define PMU_WKEN_CTL_HDSW_BLOCK_EN	(0x1 << 8)
#define PMU_WKEN_CTL_HDSW_OFF_EN	(0x1 << 7)
#define PMU_WKEN_CTL_BT_EN			(0x1 << 6)
#define PMU_WKEN_CTL_DC5VOFF_EN		(0x1 << 5)
#define PMU_WKEN_CTL_DC5VON_EN		(0x1 << 4)
#define PMU_WKEN_CTL_RESET_WKEN		(0x1 << 3)
#define PMU_WKEN_CTL_SHORT_WKEN		(0x1 << 2)
#define PMU_WKEN_CTL_LONG_WKEN		(0x1 << 1)
#define PMU_WKEN_CTL_HDSW_WKEN		(0x1 << 0)

// wakeup pending
#define PMU_WAKE_PD_SVCC_LOW		(0x1 << 24)
#define PMU_WAKE_PD_LB_PD			(0x1 << 23)
#define PMU_WAKE_PD_PMUFAULT_PD		(0x1 << 22)
#define PMU_WAKE_PD_P_RESET			(0x1 << 21)
#define PMU_WAKE_PD_ONOFFRESET_PD	(0x1 << 20)
#define PMU_WAKE_PD_DC5VRST_PD		(0x1 << 19)
#define PMU_WAKE_PD_POWEROK_PD		(0x1 << 18)
#define PMU_WAKE_PD_SYSRESET_PD		(0x1 << 17)
#define PMU_WAKE_PD_S3BT_TON_PD		(0x1 << 16)
#define PMU_WAKE_PD_ONOFF0_L_PD		(0x1 << 15)
#define PMU_WAKE_PD_ONOFF0_S_PD		(0x1 << 14)
#define PMU_WAKE_PD_IRC_WK_PD		(0x1 << 13)
#define PMU_WAKE_PD_BAT_PD			(0x1 << 12)
#define PMU_WAKE_PD_REMOTE_PD		(0x1 << 11)
#define PMU_WAKE_PD_NFC_PD			(0x1 << 9)
#define PMU_WAKE_PD_HDSWOFF_PD		(0x1 << 7)
#define PMU_WAKE_PD_BT_WK_PD		(0x1 << 6)
#define PMU_WAKE_PD_DC5VOFF_PD		(0x1 << 5)
#define PMU_WAKE_PD_DC5VON_PD		(0x1 << 4)
#define PMU_WAKE_PD_RESET_WK_PD		(0x1 << 3)
#define PMU_WAKE_PD_SHORT_WK_PD		(0x1 << 2)
#define PMU_WAKE_PD_LONG_WK_PD		(0x1 << 1)
#define PMU_WAKE_PD_HDSWON_PD		(0x1 << 0)


#define PMUADC_CTL_SENSORADC_EN     (4)
#define SENSADC_DATA_SENSADC_MASK   (0x3FF<<0)
#define SENSADC_DATA_SENSADC_SHIFT  (0)

#ifndef _ASMLANGUAGE

union sys_pm_wakeup_src {
	u32_t data;
	struct {
		u32_t onoff : 1; /* ONOFF key wakeup */
		u32_t reset : 1; /* RESET key wakeup */
		u32_t bat : 1; /* battery plug in wakeup */
		u32_t alarm : 1; /* RTC alarm wakeup */
		u32_t bt_wk : 1; /* Bluetooth wakeup pending */
		u32_t remote : 1; /* remote wakeup */
		u32_t irc : 1; /* IRC wakeup */
		u32_t hdswon : 1; /* Fake hardware power on wakeup */
		u32_t watchdog : 1; /* watchdog reboot */
	} t;
};

void sys_pm_poweroff(void);
void sys_pm_reboot(int type);

void sys_pm_config_onoff_time(unsigned int value_ms);

bool sys_pm_get_power_5v_status(void);
void sys_pm_set_wakeup_src(uint8_t mode);
int sys_pm_get_wakeup_pending(void);
u32_t sys_pm_get_backup_wakeup_pending(void);
void sys_pm_clear_wakeup_pending(void);
int sys_pm_enter_deep_sleep(void);

void sys_pm_enter_light_sleep(void);
void sys_pm_exit_light_sleep(void);

void sys_pm_temp_adc_ctrl_enable(uint32_t enable);

int sys_pm_temp_adc_get_data(void);

u32_t sys_pm_get_rc_timestamp(void);

int sys_pm_get_wakeup_source(union sys_pm_wakeup_src *src);

void sys_pm_update_rtc_regs(void);

int sys_pm_get_reboot_reason(u16_t *reboot_type, u8_t *reason);

int sys_pm_temp_adc_get_temperature(void);

#ifdef CONFIG_PM_SLEEP_TIME_TRACE
int sys_pm_get_deep_sleep_time(void);

int sys_pm_get_light_sleep_time(void);

int sys_pm_get_idle_sleep_time(void);

int sys_pm_get_bt_irq_time(void);

#endif

void soc_pm_reboot(int type, int rtc_bak_reg_num);
int soc_pm_rtc_bak_read(int rtc_bak_reg_num);
int soc_pm_rtc_bak_write(int val, int rtc_bak_reg_num);
void sys_pm_set_reboot_reason(int type);
#endif /* _ASMLANGUAGE */

#endif /* _ACTIONS_SOC_REBOOT_H_	*/
