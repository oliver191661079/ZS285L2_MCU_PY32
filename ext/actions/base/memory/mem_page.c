/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief mem page manager.
*/
#include <mem_manager.h>
#include <kernel.h>
#include <kernel_structs.h>
#include <atomic.h>
#include <init.h>
#include <toolchain.h>
#include <linker/sections.h>
#include <string.h>
#include <mem_buddy.h>
#include <stack_backtrace.h>

#ifndef CONFIG_DETECT_MEMLEAK
#define MAX_BUDDY_DEBUG_NODE_SIZE   (64)
#else
#define MAX_BUDDY_DEBUG_NODE_SIZE   (128)
#endif

struct mem_info sys_meminfo;


void print_malloc_funcs(uint32_t num_bytes, void *caller, void *ptr)
{
	uint32_t funcs[4] = {0};

	sh_backtrace_get_funcs(funcs, 4, (uint32_t)caller);

	printk("malloc ptr %p %d caller 0x%x 0x%x 0x%x 0x%x\n", ptr, num_bytes, funcs[0], funcs[1], funcs[2], funcs[3]);
}
void *mem_page_malloc(unsigned int num_bytes, void *caller)
{
    void *ptr;

#ifdef CONFIG_DETECT_MEMLEAK
    unsigned int alloc_size;
    unsigned int need_size = num_bytes;

    if(num_bytes < MAX_BUDDY_DEBUG_NODE_SIZE){
        alloc_size = (num_bytes + 15) / 16 * 16;
        if((alloc_size - num_bytes) < sizeof(struct buddy_debug_info)){
            num_bytes += sizeof(struct buddy_debug_info);
        }
    }

    ptr = mem_buddy_malloc(num_bytes, &need_size, &sys_meminfo, caller);

    if(num_bytes >= MAX_BUDDY_DEBUG_NODE_SIZE){
    }

#else
    ptr = mem_buddy_malloc(num_bytes, &num_bytes, &sys_meminfo, caller);

	//if(num_bytes >= MAX_BUDDY_DEBUG_NODE_SIZE){
	//	print_malloc_funcs(num_bytes, caller, ptr);
	//}

#if 0
    if((uint32_t)ptr >= 0x98000 && (uint32_t)ptr <= 0x9c800){
        printk("malloc %d caller %p ptr %p\n\n", num_bytes, caller, ptr);
    }
#endif

#endif

    return ptr;
}

#ifdef CONFIG_DETECT_MEMLEAK
struct mem_free_info
{
    uint32_t where;
    void *caller;
    int8_t malloc_prio;
    int8_t free_prio;
};

struct mem_free_info free_info[20];
uint8_t free_index;

static void add_free_info(void *where, void *caller, int8_t malloc_prio, int8_t free_prio)
{
    free_info[free_index].caller = caller;
    free_info[free_index].where = (uint32_t)where;
    free_info[free_index].malloc_prio = malloc_prio;
    free_info[free_index].free_prio = free_prio;

    free_index++;
    if(free_index == 20){
        free_index = 0;
    }
}
#endif

void check_mem_debug(void *where, struct mem_info *mem_info, void *caller, struct buddy_debug_info *buddy_debug, uint32_t size)
{
#ifdef CONFIG_DETECT_MEMLEAK
    s32_t cur_prio = k_thread_priority_get(k_current_get());
#endif

    if(buddy_debug->size_mask != (unsigned short)(~(buddy_debug->size + (unsigned short)0x1234))){
        buddy_debug->size = size;
    }else{
#ifdef CONFIG_DETECT_MEMLEAK
        if(buddy_debug->prio != INVALID_THREAD_PRIO && buddy_debug->prio != cur_prio){
            printk("mem %p malloc task %d free %d caller %p\n", where, buddy_debug->prio, cur_prio, caller);
        }
#endif
    }

#ifdef CONFIG_DETECT_MEMLEAK
    add_free_info(where, caller, buddy_debug->prio, cur_prio);
#endif

    mem_info->original_size -= buddy_debug->size;
}

void mem_page_free(void *ptr, void *caller)
{
    mem_buddy_free(ptr, &sys_meminfo, caller);

#if 0
    int free_size = mem_buddy_free(ptr, &sys_meminfo, caller);
    if (free_size >= MAX_BUDDY_DEBUG_NODE_SIZE) {
        printk("free ptr %p %d caller %p \n\n", ptr, free_size, caller);
    }
#endif

#if 0
    if ((uint32_t)ptr >= 0x98000 && (uint32_t)ptr <= 0x9c800) {
        printk("free ptr %p %d caller %p \n\n", ptr, free_size, caller);
    }
#endif
}

void mem_page_init(void)
{
	memset(&sys_meminfo, 0, sizeof(sys_meminfo));
	memset(sys_meminfo.buddys, -1, sizeof(sys_meminfo.buddys));
}

extern void pagepool_use_dump(uint32_t use_size);
extern void thread_dump_memory(struct mem_info *mem_info, uint32_t print_detail);
void mem_page_dump(uint32_t dump_detail)
{

    printk("\nHeap memory sumary:\n");
    printk("\talloc: %d\n", sys_meminfo.alloc_size);
    printk("\thistory alloc: %d\n", sys_meminfo.original_size);

    pagepool_use_dump(sys_meminfo.alloc_size);

    if(dump_detail){
        printk("\nmalloc detail:\n");
        printk("address \tsize(space thread)\tcaller\n");
        thread_dump_memory(&sys_meminfo, true);
		printk("\nmalloc detail end\n");
    }
}


