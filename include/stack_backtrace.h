/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief cpu load statistic
 */

#ifndef __INCLUDE_STACK_BACKTRACE_H__
#define __INCLUDE_STACK_BACKTRACE_H__

#include <kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	void (*dump)(void);
}crash_dump_t;

#define CRASH_DUMP_REGISTER(crash_dump_info, level) \
	static const crash_dump_t crash_dump_info __attribute__((__section__(".crash_dump_" #level STRINGIFY(prio)))) __attribute__((used))

#ifdef CONFIG_STACK_BACKTRACE

void dump_stack(void);
void show_thread_stack(struct k_thread *thread);
void show_all_threads_stack(void);
void sh_backtrace_get_funcs(uint32_t *func, uint32_t num, uint32_t skip_func_num);

#else /* !CONFIG_STACK_BACKTRACE */

static inline void dump_stack(void)
{
}

static inline void show_thread_stack(struct k_thread *thread)
{
	ARG_UNUSED(thread);
}

static inline void show_all_threads_stack(void)
{
}

static inline void sh_backtrace_get_funcs(uint32_t *func, uint32_t num, uint32_t skip_func_num)
{
	ARG_UNUSED(func);
	ARG_UNUSED(num);
	ARG_UNUSED(skip_func_num);
}

#endif /* !CONFIG_STACK_BACKTRACE */

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __INCLUDE_STACK_BACKTRACE_H__ */
