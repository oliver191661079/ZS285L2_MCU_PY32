/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file mem manager interface 
 */

#ifndef __MEM_MANAGER_H__
#define __MEM_MANAGER_H__

#include <kernel.h>
#ifdef CONFIG_APP_USED_MEM_PAGE

struct mem_info {
       unsigned int alloc_size;
       unsigned int original_size;
       unsigned char buddys[28];
};

extern int pagepool_init(void);

extern void * dspram_alloc_range(void *dsp_ram_start, unsigned int dsp_ram_size);

extern int dspram_free_range(void *dsp_ram_start, unsigned int dsp_ram_size);

extern void * mem_buddy_malloc(int size, int *need_size, struct mem_info *mem_info, void *caller);

extern int mem_buddy_free(void *where, struct mem_info *mem_info, void *caller);


#endif
/**
 * @defgroup mem_manager_apis Mem Manager APIs
 * @ingroup system_apis
 * @{
 */
/**
 * @cond INTERNAL_HIDDEN
 */
/**
 * @brief Allocate memory from system mem heap .
 *
 * This routine provides traditional malloc() semantics. Memory is
 * allocated from the heap memory pool.
 *
 * @param num_bytes Amount of memory requested (in bytes).
 *
 * @return Address of the allocated memory if successful; otherwise NULL.
 */

void * mem_malloc(unsigned int num_bytes);

/**
 * @brief Free memory allocated  system mem heap.
 *
 * This routine provides traditional free() semantics. The memory being
 * returned must have been allocated from the heap memory pool.
 *
 * If @a ptr is NULL, no operation is performed.
 *
 * @param ptr Pointer to previously allocated memory.
 *
 * @return N/A
 */

void mem_free(void *ptr);
/**
 * INTERNAL_HIDDEN @endcond
 */
/**
 * @brief Allocate memory from app mem heap .
 *
 * This routine provides traditional malloc() semantics. Memory is
 * allocated from the heap memory pool.
 *
 * @param num_bytes Amount of memory requested (in bytes).
 *
 * @return Address of the allocated memory if successful; otherwise NULL.
 */

void * app_mem_malloc(unsigned int num_bytes);

/**
 * @brief Free memory allocated  app mem heap.
 *
 * This routine provides traditional free() semantics. The memory being
 * returned must have been allocated from the heap memory pool.
 *
 * If @a ptr is NULL, no operation is performed.
 *
 * @param ptr Pointer to previously allocated memory.
 *
 * @return N/A
 */

void app_mem_free(void *ptr);


/**
 * @brief dump mem info.
 *
 * This routine provides dump mem info .
 *
 * @return N/A
 */


void mem_manager_dump(void);
int mem_manager_init(void);
/**
 * @brief Allocate memory from system mem heap .
 *
 * This routine provides traditional malloc() semantics. Memory is
 * allocated from the heap memory pool.
 *
 * @param num_bytes Amount of memory requested (in bytes).
 * @param caller caller function address, use __builtin_return_address(0).
 *
 * @return Address of the allocated memory if successful; otherwise NULL.
 */
void *mem_malloc_debug(unsigned int num_bytes, void *caller);

#define malloc mem_malloc
#define free mem_free
/**
 * @} end defgroup mem_manager_apis
 */
#endif
