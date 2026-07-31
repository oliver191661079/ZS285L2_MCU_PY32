/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief SPICACHE profile interface for Actions SoC
 */

#ifndef __SPICACHE_H__
#define __SPICACHE_H__

struct spicache_master_stat {
	uint32_t hit_cnt;
	uint32_t miss_cnt;
	uint32_t writeback_cnt;
};

struct spicache_profile {
	int spi_id;

	/* must aligned to 64-byte */
	uint32_t start_addr;
	uint32_t end_addr;

	/* timestamp, ns */
	uint32_t start_time;
	uint32_t end_time;

	/* hit/miss in user address range */
	uint32_t hit_cnt;
	uint32_t miss_cnt;

	/* hit/miss in all address range */
	uint32_t total_hit_cnt;
	uint32_t total_miss_cnt;

	struct spicache_master_stat master[4];
};

int spicache_profile_start(struct spicache_profile *profile);
int spicache_profile_stop(struct spicache_profile *profile);
int spicache_profile_get_data(struct spicache_profile *profile);
void spicache_profile_dump(struct spicache_profile *profile);

#endif	/* __SPICACHE_H__ */
