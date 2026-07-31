/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief mem pool manager.
*/

#include <os_common_api.h>
#include <mem_manager.h>
#include <kernel.h>
#include <kernel_structs.h>
#include <init.h>
#include <toolchain.h>
#include <linker/sections.h>
#include <string.h>
#include "mem_inner.h"

#define SYSTEM_MEM_POOL_MAXSIZE_BLOCK_NUMER 5
#define SYSTEM_MEM_POOL_MAX_BLOCK_SIZE      2048
#define SYSTEM_MEM_POOL_MIN_BLOCK_SIZE      16
#define SYSTEM_MEM_POOL_ALIGN_SIZE         4


K_HEAP_DEFINE(system_mem_pool, SYSTEM_MEM_POOL_MAXSIZE_BLOCK_NUMER * SYSTEM_MEM_POOL_MAX_BLOCK_SIZE);


void *mem_pool_malloc(unsigned int num_bytes)
{
	void *ptr = k_heap_alloc(&system_mem_pool, num_bytes, K_NO_WAIT);
	if (ptr) {
		memset(ptr, 0, num_bytes);
	}

	return ptr;
}

void mem_pool_free(void *ptr)
{
	if (ptr != NULL) {
		k_heap_free(&system_mem_pool, ptr);
	}
}


