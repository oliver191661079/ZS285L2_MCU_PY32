/**
 * @file random.h
 *
 * @brief Public APIs for the random driver.
 */

/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __MEM_BUDDY_H__
#define __MEM_BUDDY_H__

#ifdef __cplusplus
extern "C" {
#endif

#define INVALID_THREAD_PRIO     (-16)

struct buddy_debug_info{
    uint16_t size;
    uint16_t size_mask;
    int32_t caller :26;
    int32_t prio : 6;
};

extern void * mem_buddy_malloc(int size, int *need_size, struct mem_info *mem_info, void *caller);
extern int mem_buddy_free(void *where, struct mem_info *mem_info, void *caller);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* __MEM_BUDDY_H__ */
