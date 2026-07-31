/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file dynamic voltage and frequency scaling interface
 */

#ifndef	_ACTIONS_SOC_DVFS_H_
#define	_ACTIONS_SOC_DVFS_H_

#ifndef _ASMLANGUAGE

/*
 * dvfs levels
 */
#define SOC_DVFS_LEVEL_IDLE			        (10)
#define SOC_DVFS_LEVEL_MCU_PERFORMANCE      (20)
#define SOC_DVFS_LEVEL_NORMAL			    (30)
#define SOC_DVFS_LEVEL_SINGLE_MUSIC         (40)
#define SOC_DVFS_LEVEL_DSP_ENCOM_PERFM		(45)
#define SOC_DVFS_LEVEL_DSP_PERFORMANCE		(50)
#define SOC_DVFS_LEVEL_ALL_PERFORMANCE		(55)
#define SOC_DVFS_LEVEL_HIGH_PERFORMANCE     (58)
#define SOC_DVFS_LEVEL_LE_AUDIO_SLAVE_PERFORMANCE      (60)
#define SOC_DVFS_LEVEL_MASTER_PERFORMANCE	(65)
#define SOC_DVFS_LEVEL_CSB_PERFORMANCE		(70)
#define SOC_DVFS_LEVEL_BR_FULL_PERFORMANCE             (75)
#define SOC_DVFS_LEVEL_LE_AUDIO_BR_MASTER_PERFORMANCE  (80)
#define SOC_DVFS_LEVEL_FULL_PERFORMANCE		(90)
#define SOC_DVFS_LEVEL_LE_AUDIO_USB_MASTER_PERFORMANCE (95)

#define SOC_DVFS_LEVEL_BQB					(100)

#define SOC_DVFS_PRE_CHANGE		(0)
#define SOC_DVFS_POST_CHANGE		(1)

enum dvfs_dev_clk_type {
    DVFS_DEV_CLK_ASRC,
    DVFS_DEV_CLK_SPDIF,
};

struct dvfs_level {
	u8_t level_id;
	u8_t enable_cnt;
	u16_t cpu_freq;
	u16_t dsp_freq;
	u16_t ahb_freq;
	u16_t vdd_volt;
};

struct dvfs_freqs {
	uint8_t state; /* DVFS_EVENT_PRE_CHANGE / DVFS_EVENT_POST_CHANGE */
	uint8_t old_level;
	uint8_t new_level;
};


struct dvfs_notifier {
	sys_dnode_t node;
	void (*dvfs_notify_func_t)(void *user_data, struct dvfs_freqs *dvfs_freq);
	void *user_data;
};


#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL

int soc_dvfs_set_freq_table(struct dvfs_level *dvfs_level_tbl, int level_cnt);
int soc_dvfs_get_current_level(void);
int soc_dvfs_set_level(int level_id, const char *user_info);
int soc_dvfs_unset_level(int level_id, const char *user_info);
void soc_dvfs_dump_tbl(void);
int dvfs_register_notifier(struct dvfs_notifier *notifier);
int dvfs_unregister_notifier(struct dvfs_notifier *notifier);
#else

#define soc_dvfs_set_freq_table(dvfs_level_tbl, level_cnt)	(0)
#define soc_dvfs_get_current_level()				(0)
#define soc_dvfs_set_level(level_id)				(0)
#define soc_dvfs_unset_level(level_id)				(0)
#define soc_dvfs_dump_tbl()                         (0)
#define dvfs_register_notifier(notify)				(0)
#define dvfs_unregister_notifier(notify)			(0)
#endif	/* CONFIG_SOC_DVFS_DYNAMIC_LEVEL */


#ifdef CONFIG_SOC_DVFS_CPU_IDLE_LOW_POWER

void soc_dvfs_cpu_enter_lowpower(void);
void soc_dvfs_cpu_exit_lowpower(void);
int soc_dvfs_set_asrc_rate(int clk_mhz);
int soc_dvfs_set_spdif_rate(int sample_rate);
#else
#define soc_dvfs_cpu_enter_lowpower()				do {} while (0)
#define soc_dvfs_cpu_exit_lowpower()				do {} while (0)
#define soc_dvfs_set_asrc_rate()				    do {} while (0)
#define soc_dvfs_set_spdif_rate()				    do {} while (0)

#endif	/* CONFIG_SOC_DVFS_CPU_IDLE_LOW_POWER */

#endif /* _ASMLANGUAGE */

#endif /* _ACTIONS_SOC_DVFS_H_	*/
