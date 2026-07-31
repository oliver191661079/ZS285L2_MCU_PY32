/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief cpu load statistic
 */

#include <kernel.h>
#include <kernel_structs.h>
#include <misc/printk.h>
#include <linker/sections.h>
#include <debug/object_tracing.h>
#include <ksched.h>
#include <stack_backtrace.h>
#include <cpuload_stat.h>

#define RUNNING_CYCLES(end, start)	((uint32_t)((long)(end) - (long)(start)))

/* start flag */
static int cpuload_started;

/* cpu load poll interval, unit: ms */
static int cpuload_interval;

struct k_delayed_work cpuload_stat_work;

#ifdef CONFIG_CPU_LOAD_DEBUG
static uint8_t block_thread_prio = 0xff;
static uint16_t block_time_ms;

static u32_t cpuload_high_prio_ivt_cycles;
static u32_t cpuload_total_cycles;
static u32_t pre_tick_time, curr_tick_time;
static u32_t cpuload_debug_log_mask = 0xffffffff;

void cpuload_curr_cycle_state(void)
{
	unsigned int key;
	key = irq_lock();

	printk("\n(%u)total %d us, prio < 0 run %d us\n", k_uptime_get_32(),
			SYS_CLOCK_HW_CYCLES_TO_NS_AVG(cpuload_total_cycles, 1000),
			SYS_CLOCK_HW_CYCLES_TO_NS_AVG(cpuload_high_prio_ivt_cycles, 1000));

	irq_unlock(key);
}
#endif

extern int printk_tiny(const char *fmt, ...);

#ifdef CONFIG_CPU_LOAD_DEBUG
static void __sys_cpuload_context_stat(struct k_thread *from, struct k_thread *to)
{
	uint32_t curr_time;
	u32_t run_cycles;

	curr_time = k_cycle_get_32();
	run_cycles = RUNNING_CYCLES(curr_time, from->start_time);

	if(to->base.prio == block_thread_prio){
		curr_time = k_uptime_get_32();

		if(to->last_time != 0){
			if(curr_time - to->last_time > block_time_ms){
				printk_tiny("th %d blocked %d(ms)\n", block_thread_prio, curr_time - to->last_time);
				show_thread_stack(to);
			}
		}

		to->last_time = curr_time;
	}

	if (cpuload_debug_log_mask & CPULOAD_DEBUG_LOG_THREAD_RUNTIME) {
		if ((run_cycles > MSEC_TO_HW_CYCLES(10)) && (from->base.prio < K_LOWEST_THREAD_PRIO)) {
			printk_tiny("thread %p(%d) %dus\n", from, from->base.prio, SYS_CLOCK_HW_CYCLES_TO_NS_AVG(run_cycles, 1000));
			printk_tiny("ent %p->%p\n", from->keep_entry, to->keep_entry);
		}
	}

	if (from->base.prio < K_LOWEST_THREAD_PRIO) {
		cpuload_total_cycles += run_cycles;
	}

	if (from->base.prio < 0) {
		cpuload_high_prio_ivt_cycles += run_cycles;
	}

	curr_tick_time = k_uptime_get_32();
	if ((curr_tick_time - pre_tick_time) > 1000) {
		pre_tick_time = curr_tick_time;

		if (cpuload_total_cycles > MSEC_TO_HW_CYCLES(500)
			&& cpuload_high_prio_ivt_cycles * 100 /  cpuload_total_cycles > 10) {
			printk_tiny("(%u) t%dus, h%dus\n", curr_tick_time,
				SYS_CLOCK_HW_CYCLES_TO_NS_AVG(cpuload_total_cycles, 1000),
				SYS_CLOCK_HW_CYCLES_TO_NS_AVG(cpuload_high_prio_ivt_cycles, 1000));
		}
		cpuload_total_cycles = 0;
		cpuload_high_prio_ivt_cycles = 0;
	}

}


void _sys_cpuload_context_stat(struct k_thread *from, struct k_thread *to)
{
    uint32_t param[4] = {
            (uint32_t) from,
            (uint32_t) to,
            (uint32_t) _interrupt_stack + CONFIG_ISR_STACK_SIZE,
            (uint32_t) __sys_cpuload_context_stat };

    uint32_t flags;

    flags = irq_lock();

	if ((uint32_t) &flags > (uint32_t) _interrupt_stack
			&& (uint32_t) &flags < \
			((uint32_t) _interrupt_stack + CONFIG_ISR_STACK_SIZE)) {

        __sys_cpuload_context_stat(from, to);
    }else{
        //maybe overflow in thread stack, we must switch it to interrupt stack
        //save the thread context,include sp, argument registers ...
		__asm__ __volatile (
				"	ldw	r0,	%0	\n"
				"	ldw	r1,	%1	\n"
				"	ldw	r2,	%2	\n"
				"	mov	r4,	r14	\n"
				"	mov	r14,r2	\n"
				"	ldw	r2,	%3	\n"
				"	jsr	r2		\n"
				"	mov	r14, r4	\n"
				:
				: "m" (param[0]), "m" (param[1]), "m" (param[2]), "m" (param[3])
				: "r0", "r1", "r2", "r4", "r14");
    }

    irq_unlock(flags);
}
#endif


__ramfunc void _sys_cpuload_context_switch(struct k_thread *from, struct k_thread *to)
{
	uint32_t curr_time;
	u32_t run_cycles;

#if 1
	/* FIXME: monitor thread switch bug */
	if (!_is_thread_ready(to)) {
		printk("[ERR] thread %p->%p", from, to);
		panic("thread not ready\n");
	}
#endif


#ifndef CONFIG_CPU_LOAD_DEBUG
	if (!cpuload_started)
		return;
#endif

#ifdef CONFIG_CPU_LOAD_DEBUG
	_sys_cpuload_context_stat(from, to);
#endif

	curr_time = k_cycle_get_32();
	run_cycles = RUNNING_CYCLES(curr_time, from->start_time);
	from->running_cycles += run_cycles;
	to->start_time = curr_time;

}

static void cpuload_stat_clear(void)
{
	struct k_thread *thread_list = NULL;
	unsigned int key;

	key = irq_lock();

	thread_list = (struct k_thread *)(_kernel.threads);
	while (thread_list != NULL) {
		if (thread_list == k_current_get())
			_current->start_time = k_cycle_get_32();

		/* clear old cycles counter */
		thread_list->running_cycles = 0;
		thread_list = (struct k_thread *)thread_list->next_thread;
	}

	irq_unlock(key);
}

void cpuload_stat(uint32_t interval)
{
	struct k_thread *thread_list = NULL;
	unsigned int key, ratio, running_cycles, time_us;
	uint32_t curr_time;
	int is_curr_thread, curr_prio;

	printk(" task\t\t prio\t run(us)\t %%cpu\n");

	key = irq_lock();

	/* update current thread cycles counter*/
	curr_time = k_cycle_get_32();
	_current->running_cycles += RUNNING_CYCLES(curr_time, _current->start_time);
	_current->start_time = curr_time;

	thread_list = (struct k_thread *)(_kernel.threads);
	while (thread_list != NULL) {
		running_cycles = thread_list->running_cycles;
		is_curr_thread = thread_list == k_current_get();
		curr_prio = k_thread_priority_get(thread_list);
		irq_unlock(key);

		time_us = SYS_CLOCK_HW_CYCLES_TO_NS_AVG(running_cycles, 1000);
		ratio = time_us / 10 / interval;

		printk("%s%p:\t %d\t %d\t\t %d\n",
		       is_curr_thread ? "*" : " ",
		       thread_list,
		       curr_prio,
		       time_us,
		       ratio);

		/* clear old cycles counter */
		key = irq_lock();
		thread_list->running_cycles = 0;
		thread_list = (struct k_thread *)thread_list->next_thread;
	}

	irq_unlock(key);
}

static void cpuload_stat_callback(struct k_work *work)
{
    cpuload_stat(cpuload_interval);

	k_delayed_work_submit(&cpuload_stat_work, cpuload_interval);
}

void cpuload_stat_start(int interval_ms)
{
	if (!interval_ms)
		return;

	if (cpuload_started)
		k_delayed_work_cancel(&cpuload_stat_work);

	cpuload_stat_clear();

	cpuload_interval = interval_ms;
	cpuload_started = 1;

	k_delayed_work_init(&cpuload_stat_work, cpuload_stat_callback);
	k_delayed_work_submit(&cpuload_stat_work, interval_ms);
}

void cpuload_stat_stop(void)
{
	k_delayed_work_cancel(&cpuload_stat_work);
	cpuload_started = 0;
}

unsigned int cpuload_debug_log_mask_and(unsigned int log_mask)
{
#ifdef CONFIG_CPU_LOAD_DEBUG
	unsigned int log_mask_old;
	log_mask_old = cpuload_debug_log_mask;
	cpuload_debug_log_mask &= log_mask;
	return log_mask_old;
#else
	return log_mask;
#endif
}

unsigned int cpuload_debug_log_mask_or(unsigned int log_mask)
{
#ifdef CONFIG_CPU_LOAD_DEBUG
	unsigned int log_mask_old;
	log_mask_old = cpuload_debug_log_mask;
	cpuload_debug_log_mask |= log_mask;
	return log_mask_old;
#else
	return log_mask;
#endif
}

#ifdef CONFIG_CPU_LOAD_DEBUG
void thread_block_stat_start(int prio, int block_ms)
{
    block_thread_prio = prio;
    block_time_ms = block_ms;
}

void thread_block_stat_stop(void)
{
    block_thread_prio = 0xff;
}
#endif

