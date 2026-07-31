/** @file
 * @brief Bt stack os depend.
 *
 * Copyright (c) 2019 Actions Semi Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <logging/sys_log.h>

#include <zephyr.h>
#include <shell/shell.h>
#include <misc/printk.h>
#include <stdlib.h>
#include <string.h>

#include <acts_bluetooth/buf.h>

#include <acts_stack_os_depend.h>

extern void *mem_malloc(unsigned int num_bytes);
extern void mem_free(void *ptr);

void *k_heap_alloc(void *h, size_t bytes, k_timeout_t timeout)
{
	return mem_malloc(bytes);
}

void k_heap_free(void *h, void *mem)
{
	mem_free(mem);
}

uint64_t z_timeout_end_calc(k_timeout_t timeout)
{
	if (K_TIMEOUT_EQ(timeout, K_FOREVER)) {
		return UINT64_MAX;
	} else if (K_TIMEOUT_EQ(timeout, K_NO_WAIT)) {
		return k_uptime_get();
	}

	return k_uptime_get() + MAX(1, timeout);
}




