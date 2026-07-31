/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file dynamic voltage and frequency scaling interface
 */

#include <kernel.h>
#include <init.h>
#include <device.h>
#include <soc.h>
#include <soc_dvfs.h>
#include <soc_freq.h>

#define SYS_LOG_DOMAIN "DVFS"
#define SYS_LOG_LEVEL SYS_LOG_LEVEL_INFO
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_SOC_DVFS

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL

struct dvfs_manager {
	struct k_sem lock;
	u8_t cur_dvfs_idx;
	u8_t dvfs_level_cnt;
	u8_t asrc_limit_clk_mhz;
	u8_t spdif_limit_clk_mhz;
	struct dvfs_level *dvfs_level_tbl;

};

static sys_dlist_t dvfs_notify_list = SYS_DLIST_STATIC_INIT(&dvfs_notify_list);



struct dvfs_manager g_soc_dvfs;

struct dvfs_level *soc_dvfs_get_default_dvfs_table(void);
uint32_t soc_dvfs_get_default_dvfs_table_items(void);

static int level_id_to_tbl_idx(int level_id)
{
	int i;

	for (i = 0; i < g_soc_dvfs.dvfs_level_cnt; i++) {
		if (g_soc_dvfs.dvfs_level_tbl[i].level_id == level_id) {
			return i;
		}
	}

	return -1;
}

static int soc_dvfs_get_max_idx(void)
{
	int i;

	for (i = (g_soc_dvfs.dvfs_level_cnt - 1); i >= 0; i--) {
		if (g_soc_dvfs.dvfs_level_tbl[i].enable_cnt > 0) {
			return i;
		}
	}

	return 0;
}

void soc_dvfs_dump_tbl(void)
{
	const struct dvfs_level *dvfs_level = &g_soc_dvfs.dvfs_level_tbl[0];
	int i;

	SYS_LOG_INF("idx   level_id   dsp   cpu  vdd  enable_cnt");
	for (i = 0; i < g_soc_dvfs.dvfs_level_cnt; i++, dvfs_level++) {
		SYS_LOG_INF("%d: %d   %d   %d   %d   %d", i,
			dvfs_level->level_id,
			dvfs_level->dsp_freq,
			dvfs_level->cpu_freq,
			dvfs_level->vdd_volt,
			dvfs_level->enable_cnt);
	}
}

static void dvfs_changed_notify(int state, uint8_t old_level_index, uint8_t new_level_index)
{
	struct dvfs_notifier *obj, *next;

	struct dvfs_freqs dvfs_freqs_info = {0};
	dvfs_freqs_info.state = state;
	dvfs_freqs_info.old_level = old_level_index;
	dvfs_freqs_info.new_level = new_level_index;

	SYS_DLIST_FOR_EACH_CONTAINER_SAFE(&dvfs_notify_list, obj, next, node) {
		if (obj->dvfs_notify_func_t)
			obj->dvfs_notify_func_t(obj->user_data, &dvfs_freqs_info);
	}
}

int dvfs_register_notifier(struct dvfs_notifier *notifier)
{
	struct dvfs_notifier *obj;

	if (!notifier)
		return -EINVAL;

    SYS_DLIST_FOR_EACH_CONTAINER(&dvfs_notify_list, obj, node) {
		if (obj == notifier) {
			SYS_LOG_ERR("dvfs notifier:%p has already registered", notifier);
			return -EEXIST;
		}
    }

	sys_dlist_append(&dvfs_notify_list, &notifier->node);

	SYS_LOG_DBG("dvfs register notifier:%p func:%p\n",
			notifier, notifier->dvfs_notify_func_t);

	return 0;
}

int dvfs_unregister_notifier(struct dvfs_notifier *notifier)
{
	if (!notifier)
		return -EINVAL;

	sys_dlist_remove(&notifier->node);

	return 0;
}

static void soc_dvfs_corepll_changed_notify(int state, int old_corepll_freq, int new_corepll_freq)
{
	if (((state == SOC_DVFS_PRE_CHANGE) && (old_corepll_freq < new_corepll_freq)) ||
	    ((state == SOC_DVFS_POST_CHANGE && old_corepll_freq > new_corepll_freq))) {
		/* update SPI0 clock div */
#ifdef CONFIG_XSPI0_USE_COREPLL
		soc_freq_set_spi_clk_with_corepll(0, CONFIG_XSPI_ACTS_FREQ_MHZ, new_corepll_freq);
#endif

#ifdef CONFIG_XSPI1_USE_COREPLL
		soc_freq_set_spi_clk_with_corepll(1, CONFIG_XSPI1_ACTS_FREQ_MHZ, new_corepll_freq);
#endif

#ifdef CONFIG_SOC_SD_USE_COREPLL
		/* update SD0 clock div */
		soc_freq_set_sd_clk_with_corepll(0, CONFIG_SD0_FREQ_MHZ, new_corepll_freq);
		/* update SD1 clock div */
		soc_freq_set_sd_clk_with_corepll(1, CONFIG_SD1_FREQ_MHZ, new_corepll_freq);
#endif /* CONFIG_SOC_SD_USE_COREPLL */
	}
}

static void soc_dvfs_sync(void)
{
	struct dvfs_level *dvfs_level, *old_dvfs_level;
	unsigned int old_idx, new_idx, old_volt;
	unsigned old_dsp_freq;

	old_idx = g_soc_dvfs.cur_dvfs_idx;

	/* get current max dvfs level */
	new_idx = soc_dvfs_get_max_idx();
	if (new_idx == old_idx) {
		/* same level, no need sync */
		SYS_LOG_INF("max idx %d\n", new_idx);
		return;
	}

	dvfs_level = &g_soc_dvfs.dvfs_level_tbl[new_idx];

	old_dvfs_level = &g_soc_dvfs.dvfs_level_tbl[old_idx];
	old_volt = soc_pmu_get_vdd_voltage();
	old_dsp_freq = soc_freq_get_dsp_freq();

	SYS_LOG_INF("level_id [%d] -> [%d]", old_dvfs_level->level_id,
		dvfs_level->level_id);

	/* set vdd voltage before clock setting if new vdd is up */
	if (dvfs_level->vdd_volt > old_volt) {
		soc_pmu_set_vdd_voltage(dvfs_level->vdd_volt);
	}

	/* send notify before clock setting */
	dvfs_changed_notify(SOC_DVFS_PRE_CHANGE, old_dvfs_level->level_id, dvfs_level->level_id);

	/* send notify before clock setting */
	soc_dvfs_corepll_changed_notify(SOC_DVFS_PRE_CHANGE,
		old_dsp_freq, dvfs_level->dsp_freq);

	SYS_LOG_INF("new dsp freq %d, cpu freq %d", dvfs_level->dsp_freq,
		dvfs_level->cpu_freq);

	/* adjust core/dsp/cpu clock */
	soc_freq_set_cpu_clk(dvfs_level->dsp_freq, dvfs_level->cpu_freq, dvfs_level->ahb_freq);

	/* set vdd voltage after clock setting if new vdd is down */
	if (dvfs_level->vdd_volt < old_volt) {
		soc_pmu_set_vdd_voltage(dvfs_level->vdd_volt);
	}

	/* send notify after clock setting */
	soc_dvfs_corepll_changed_notify(SOC_DVFS_POST_CHANGE,
		old_dsp_freq, dvfs_level->dsp_freq);

	/* send notify after clock setting */
	dvfs_changed_notify(SOC_DVFS_POST_CHANGE, old_dvfs_level->level_id, dvfs_level->level_id);

	g_soc_dvfs.cur_dvfs_idx = new_idx;
}

static int soc_dvfs_update_freq(int level_id, bool is_set, const char *user_info)
{
	struct dvfs_level *dvfs_level;
	int tbl_idx;

#if 0
	//FIXME: increate cpu clk temporally to avoid snoop time sequence problem
	if (level_id < SOC_DVFS_LEVEL_FULL_PERFORMANCE) {
		level_id = SOC_DVFS_LEVEL_FULL_PERFORMANCE;
		SYS_LOG_INF("fixme: level %d -> %d", level_id, SOC_DVFS_LEVEL_FULL_PERFORMANCE);
	}
#endif

	tbl_idx = level_id_to_tbl_idx(level_id);
	if (tbl_idx < 0) {
		SYS_LOG_ERR("%s: invalid level id %d\n", __func__, level_id);
		return -EINVAL;
	}

	dvfs_level = &g_soc_dvfs.dvfs_level_tbl[tbl_idx];

	k_sem_take(&g_soc_dvfs.lock, K_FOREVER);

	SYS_LOG_INF("tbl_idx %d, enable_cnt %d", tbl_idx, dvfs_level->enable_cnt);
	if (is_set) {
	    if(dvfs_level->enable_cnt < 255){
            dvfs_level->enable_cnt++;
	    }else{
            SYS_LOG_WRN("max dvfs level count");
	    }
	} else {
		if (dvfs_level->enable_cnt > 0) {
			dvfs_level->enable_cnt--;
		}else{
            SYS_LOG_WRN("min dvfs level count");
		}
	}

	soc_dvfs_sync();

	k_sem_give(&g_soc_dvfs.lock);

	return 0;
}

int soc_dvfs_set_level(int level_id, const char *user_info)
{
	SYS_LOG_INF("level %d %s", level_id, user_info);
	return soc_dvfs_update_freq(level_id, 1, user_info);
}

int soc_dvfs_unset_level(int level_id, const char *user_info)
{
	SYS_LOG_INF("level %d %s", level_id, user_info);
	return soc_dvfs_update_freq(level_id, 0, user_info);
}

int soc_dvfs_get_current_level(void)
{
	int idx;

	if (!g_soc_dvfs.dvfs_level_tbl)
		return -1;

	idx = g_soc_dvfs.cur_dvfs_idx;
	if (idx < 0) {
		idx = 0;
	}

	return g_soc_dvfs.dvfs_level_tbl[idx].level_id;
}

int soc_dvfs_set_freq_table(struct dvfs_level *dvfs_level_tbl, int level_cnt)
{
	if (!dvfs_level_tbl || level_cnt <= 0)
		return -EINVAL;

	g_soc_dvfs.dvfs_level_cnt = level_cnt;
	g_soc_dvfs.dvfs_level_tbl = dvfs_level_tbl;

	g_soc_dvfs.cur_dvfs_idx = 0;

	soc_dvfs_dump_tbl();

	return 0;
}

int soc_dvfs_set_asrc_rate(int clk_mhz)
{
    int level;
    struct dvfs_level *dvfs_cur;

    g_soc_dvfs.asrc_limit_clk_mhz = clk_mhz;

    level = g_soc_dvfs.cur_dvfs_idx;
	if (level < 0) {
		level = 0;
	}

    dvfs_cur = &g_soc_dvfs.dvfs_level_tbl[level];

    if(dvfs_cur->dsp_freq >= g_soc_dvfs.asrc_limit_clk_mhz){

        if(clk_mhz){
            soc_freq_set_asrc_freq(clk_mhz);
        }

        return 0;
    }else{
        SYS_LOG_ERR("\n\ncurrent dvfs freq too low");
        printk("dsp freq %d cpu freq %d\n", dvfs_cur->dsp_freq, dvfs_cur->cpu_freq);
        printk("asrc request freq %d\n", \
            g_soc_dvfs.asrc_limit_clk_mhz);

        return -EINVAL;
    }
}

int soc_dvfs_set_spdif_rate(int sample_rate)
{
    int level;
    struct dvfs_level *dvfs_cur;

    g_soc_dvfs.spdif_limit_clk_mhz = soc_freq_get_spdif_corepll_freq(sample_rate);

    level = g_soc_dvfs.cur_dvfs_idx;
	if (level < 0) {
		level = 0;
	}

    dvfs_cur = &g_soc_dvfs.dvfs_level_tbl[level];

    if(dvfs_cur->dsp_freq >= g_soc_dvfs.spdif_limit_clk_mhz){

        if(sample_rate){
            return soc_freq_set_spdif_freq(sample_rate);
        }
    }else{
        SYS_LOG_ERR("\n\ncurrent dvfs freq too low\n\n");
        printk("dsp freq %d cpu freq %d\n", dvfs_cur->dsp_freq, dvfs_cur->cpu_freq);
        printk("spdif request freq %d\n", \
            g_soc_dvfs.spdif_limit_clk_mhz);
    }

    return 0;
}

int soc_dvfs_get_process_mhz(int type)
{

    int level;
	if (!g_soc_dvfs.dvfs_level_tbl)
		return -1;

	level = g_soc_dvfs.cur_dvfs_idx;
	if (level < 0) {
		level = 0;
	}

    if (type == 1) {
        return g_soc_dvfs.dvfs_level_tbl[level].dsp_freq;
    } else if (type == 2) {
		return g_soc_dvfs.dvfs_level_tbl[level].ahb_freq;
	} else {
        return g_soc_dvfs.dvfs_level_tbl[level].cpu_freq;
    }
}

#endif /* CONFIG_SOC_DVFS_DYNAMIC_LEVEL */


static int soc_dvfs_init(struct device *arg)
{
#ifdef CONFIG_SOC_VDD_VOLTAGE
	/* now k_busy_wait() is ready for adjust vdd voltage */
	soc_pmu_set_vdd_voltage(CONFIG_SOC_VDD_VOLTAGE);
#endif

	SYS_LOG_INF("default dsp freq %dMHz, cpu freq %dMHz, vdd %d mV",
		soc_freq_get_dsp_freq(), soc_freq_get_cpu_freq(),
		soc_pmu_get_vdd_voltage());

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	soc_dvfs_set_freq_table(soc_dvfs_get_default_dvfs_table(),
		soc_dvfs_get_default_dvfs_table_items());

	k_sem_init(&g_soc_dvfs.lock, 1, 1);

	soc_dvfs_set_level(SOC_DVFS_LEVEL_ALL_PERFORMANCE, "init");
#endif

	return 0;
}

SYS_INIT(soc_dvfs_init, PRE_KERNEL_2, 2);
