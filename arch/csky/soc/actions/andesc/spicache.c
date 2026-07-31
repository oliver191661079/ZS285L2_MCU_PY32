/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief SPICACHE profile interface for Actions SoC
 */

#include <kernel.h>
#include <device.h>
#include <init.h>
#include <soc.h>
#include <misc/printk.h>
#include "spicache.h"
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_SPICACHE

#define SPIDCACHE_MASTER_COUNT	3
static const char *spidcache_master_name[SPIDCACHE_MASTER_COUNT] = {
	"CPU",
	"DSP",
	"DMA",
};

void spicache_profile_dump(struct spicache_profile *profile)
{
	u32_t interval_ms;
	u64_t hit_cnt, total;

	if (!profile)
		return;

	interval_ms = SYS_CLOCK_HW_CYCLES_TO_NS64((profile->end_time - profile->start_time)) / 1000000;

	printk("spi%d cache profile: addr range 0x%08x ~ 0x%08x, profile time %u ms\n",
		profile->spi_id, profile->start_addr, profile->end_addr, interval_ms);

	if (profile->spi_id == 0) {
		hit_cnt = (u64_t)profile->hit_cnt * 8;
		total = hit_cnt + profile->miss_cnt;
		if (total != 0)
			printk("range hit: %u,%u  miss: %u  miss ratio: %u %%%% \n",
				(u32_t)(hit_cnt >> 32), (u32_t)hit_cnt,
				profile->miss_cnt,
				(u32_t)(((u64_t)profile->miss_cnt * 100 * 100) / total));

		else
			printk("cpu not run into the specific address range!\n");

		hit_cnt = (u64_t)profile->total_hit_cnt * 8;
		total = hit_cnt + profile->total_miss_cnt;
		if (total != 0)
			printk("total hit: %u,%u  miss: %u  miss ratio: %u %%%% \n",
				(u32_t)(hit_cnt >> 32), (u32_t)hit_cnt, profile->total_miss_cnt,
				(u32_t)(((u64_t)profile->total_miss_cnt * 100 * 100) / total));
	} else {
		int i;

		for (i = 0; i < SPIDCACHE_MASTER_COUNT; i++) {
			hit_cnt = (u64_t)profile->master[i].hit_cnt * 8;
			total = hit_cnt + profile->master[i].miss_cnt + profile->master[i].writeback_cnt;

			if (total == 0)
				continue;

			printk("[%s] hit: %u,%u  miss: %u  writeback: %u  total: %u,%u  ",
				spidcache_master_name[i],
				(u32_t)(hit_cnt >> 32), (u32_t)hit_cnt,
				profile->master[i].miss_cnt, profile->master[i].writeback_cnt,
				(u32_t)(total >> 32), (u32_t)total);
			printk( "miss ratio: %u %%%%  miss+wb ratio: %u %%%% \n",
				(u32_t)((((u64_t)profile->master[i].miss_cnt) * 100 * 100) / total),
				(u32_t)((((u64_t)profile->master[i].miss_cnt +
					profile->master[i].writeback_cnt) * 100 * 100) / total));
		}
	}
}

static void spicache_update_profile_data(struct spicache_profile *profile)
{
	int i = 4;

	if (profile->spi_id == 0) {
		profile->hit_cnt = sys_read32(SPICACHE_RANGE_ADDR_HIT_COUNT);
		profile->miss_cnt = sys_read32(SPICACHE_RANGE_ADDR_MISS_COUNT);
		profile->total_hit_cnt = sys_read32(SPICACHE_TOTAL_HIT_COUNT);
		profile->total_miss_cnt = sys_read32(SPICACHE_TOTAL_MISS_COUNT);
	} else {
		for (i = 0; i < SPIDCACHE_MASTER_COUNT; i++) {
			profile->master[i].hit_cnt = sys_read32(i * 12 + SPI1_DCACHE_CK802_HIT_COUNT);
			profile->master[i].miss_cnt = sys_read32(i * 12 + SPI1_DCACHE_CK802_MISS_COUNT);
			profile->master[i].writeback_cnt = sys_read32(i * 12 + SPI1_DCACHE_CK802_WRITEBACK_COUNT);
		}
	}
}

int spicache_profile_get_data(struct spicache_profile *profile)
{
	if (!profile)
		return -EINVAL;

	spicache_update_profile_data(profile);

	return 0;
}

int spicache_profile_stop(struct spicache_profile *profile)
{
	if (!profile)
		return -EINVAL;

	profile->end_time = k_cycle_get_32();
	spicache_update_profile_data(profile);

	if (profile->spi_id == 0) {
		sys_write32(sys_read32(SPICACHE_CTL) & ~(1 << 4), SPICACHE_CTL);
	} else {
		sys_write32(sys_read32(SPI1_DCACHE_CTL)& ~(1 << 4), SPI1_DCACHE_CTL);
	}

	return 0;
}

int spicache_profile_start(struct spicache_profile *profile)
{
	if (!profile)
		return -EINVAL;

	if (profile->spi_id == 0) {
		printk("spi 0 profile start\n");
		sys_write32(profile->start_addr, SPICACHE_PROFILE_ADDR_START);
		sys_write32(profile->end_addr, SPICACHE_PROFILE_ADDR_END);
		sys_write32(sys_read32(SPICACHE_CTL) & ~(1 << 4), SPICACHE_CTL);
		sys_write32(sys_read32(SPICACHE_CTL) | (1 << 4), SPICACHE_CTL);
	} else {
		sys_write32(sys_read32(SPI1_DCACHE_CTL) & ~(1 << 4), SPI1_DCACHE_CTL);
		sys_write32(sys_read32(SPI1_DCACHE_CTL) | (1 << 4), SPI1_DCACHE_CTL);
	}

	profile->start_time = k_cycle_get_32();

	return 0;
}

