/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief cpu load statistic
 */

#ifndef __INCLUDE_CPULOAD_STAT_H__
#define __INCLUDE_CPULOAD_STAT_H__

#include <kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

void cpuload_stat_start(int interval_ms);
void cpuload_stat_stop(void);

#define CPULOAD_DEBUG_LOG_THREAD_RUNTIME  (1<<0)
unsigned int cpuload_debug_log_mask_and(unsigned int log_mask);
unsigned int cpuload_debug_log_mask_or(unsigned int log_mask);

void thread_block_stat_start(int prio, int block_ms);
void thread_block_stat_stop(void);


/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __INCLUDE_CPULOAD_STAT_H__ */
