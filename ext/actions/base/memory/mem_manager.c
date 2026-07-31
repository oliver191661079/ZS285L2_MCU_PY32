/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * @file
 * @brief mem manager.
*/

#include <init.h>
#include <kernel.h>
#include "mem_inner.h"
#include <soc_regs.h>
#include <string.h>

void * app_mem_malloc(unsigned int num_bytes)
{
#ifdef CONFIG_APP_USED_MEM_SLAB
#ifdef CONFIG_APP_USED_SYSTEM_SLAB
	return mem_slabs_malloc((struct slabs_info *)&sys_slab, num_bytes);
#else
	return mem_slabs_malloc((struct slabs_info *)&app_slab, num_bytes);
#endif
#elif defined(CONFIG_APP_USED_MEM_PAGE)
	return mem_page_malloc(num_bytes, __builtin_return_address(0));
#elif defined(CONFIG_APP_USED_MEM_POOL)
	return mem_pool_malloc(num_bytes);
#endif
}

void app_mem_free(void *ptr)
{
#ifdef CONFIG_APP_USED_MEM_SLAB
#ifdef CONFIG_APP_USED_SYSTEM_SLAB
	mem_slabs_free((struct slabs_info *)&sys_slab, ptr);
#else
	mem_slabs_free((struct slabs_info *)&app_slab, ptr);
#endif
#elif defined(CONFIG_APP_USED_MEM_PAGE)
	mem_page_free(ptr, __builtin_return_address(0));
#elif defined(CONFIG_APP_USED_MEM_POOL)
	mem_pool_free(ptr);
#endif
}

void *mem_malloc_debug(unsigned int num_bytes, void *caller)
{
#ifdef CONFIG_APP_USED_MEM_SLAB
	return mem_slabs_malloc((struct slabs_info *)&sys_slab, num_bytes);
#elif defined(CONFIG_APP_USED_MEM_PAGE)
	return mem_page_malloc(num_bytes,caller);
#elif defined(CONFIG_APP_USED_MEM_POOL)
	return mem_pool_malloc(num_bytes);
#endif
}

void *mem_malloc(unsigned int num_bytes)
{
	return mem_malloc_debug(num_bytes, __builtin_return_address(0));
}

void mem_free(void *ptr)
{
#ifdef CONFIG_APP_USED_MEM_SLAB
	mem_slabs_free((struct slabs_info *)&sys_slab, ptr);
#elif defined(CONFIG_APP_USED_MEM_PAGE)
	mem_page_free(ptr, __builtin_return_address(0));
#elif defined(CONFIG_APP_USED_MEM_POOL)
	mem_pool_free(ptr);
#endif
}

__attribute__((weak))
void *calloc(unsigned int num_bytes)
{
	void *ptr = mem_malloc(num_bytes);
	if (ptr)
		memset(ptr, 0, num_bytes);

	return ptr;
}

void free(void *) __attribute__((weak,  alias("mem_free")));

void mem_manager_dump(void)
{
#ifdef CONFIG_APP_USED_MEM_SLAB
	mem_slabs_dump((struct slabs_info *)&sys_slab, 0);
#elif defined(CONFIG_APP_USED_MEM_PAGE)
	mem_page_dump(1);
#elif defined(CONFIG_APP_USED_MEM_POOL)
	//mem_pool_dump(ptr);
#endif
}

int mem_manager_init(struct device *unused)
{
	ARG_UNUSED(unused);
#ifdef CONFIG_APP_USED_MEM_SLAB
	//mem_slab_init(void);
#elif defined(CONFIG_APP_USED_MEM_PAGE)
	mem_page_init();
#endif
	return 0;
}

SYS_INIT(mem_manager_init, PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_OBJECTS);