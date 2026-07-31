/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Kernel thread timer support
 *
 * This module provides thread timer support.
 */

#include <kernel.h>
#include <kernel_structs.h>
#include <thread_timer.h>

#undef THREAD_TIMER_DEBUG

#ifdef THREAD_TIMER_DEBUG
#include <misc/printk.h>
#define TT_DEBUG(fmt, ...) printk("[%d] thread %p: " fmt, k_uptime_get_32(), \
				(void *)_current, ##__VA_ARGS__)
#else
#define TT_DEBUG(fmt, ...)
#endif

#define compare_time(a, b) ((int)((u32_t)(a) - (u32_t)(b)))

#ifdef CONFIG_SOC_SERIES_ANDESC
#define THREAD_TIMER_ROM_CODE
#endif

#ifdef CONFIG_THREAD_TIMER_DEBUG
static void _dump_thread_timer(struct thread_timer *ttimer)
{
	printk("timer %p, prev: %p, next: %p\n",
		ttimer, ttimer->node.prev, ttimer->node.next);

	printk("\tthread: %p, period %d ms, delay %d ms\n",
		_current, ttimer->period, ttimer->duration);

	printk("\texpiry_time: %u, expiry_fn: %p, arg %p\n\n",
		ttimer->expiry_time,
		ttimer->expiry_fn, ttimer->expiry_fn_arg);
}

void _dump_thread_timer_q(void)
{
	sys_dlist_t *thread_timer_q = &_current->thread_timer_q;
	struct thread_timer *ttimer;

	printk("thread: %p, thread_timer_q: %p, head: %p, tail: %p\n",
		_current, thread_timer_q, thread_timer_q->head,
		thread_timer_q->tail);

	SYS_DLIST_FOR_EACH_CONTAINER(thread_timer_q, ttimer, node) {
		_dump_thread_timer(ttimer);
	}
}
#endif

void rom_thread_timer_init(sys_dlist_t *thread_timer_q, void *tid, struct thread_timer *ttimer, thread_timer_expiry_t expiry_fn,
               void *expiry_fn_arg);
void rom_thread_timer_start(sys_dlist_t *thread_timer_q, struct thread_timer *ttimer, s32_t duration, s32_t period,  u32_t cur_time);
void rom_thread_timer_stop(sys_dlist_t *thread_timer_q, struct thread_timer *ttimer);
bool rom_thread_timer_is_running(sys_dlist_t *thread_timer_q, struct thread_timer *ttimer);
int rom_thread_timer_next_timeout(sys_dlist_t *thread_timer_q, u32_t (*get_cur_time)(void));
void rom_thread_timer_handle_expired(sys_dlist_t *thread_timer_q, u32_t (*get_cur_time)(void));

#ifndef THREAD_TIMER_ROM_CODE
static void _thread_timer_remove(struct k_thread *thread, struct thread_timer *ttimer)
{
	struct thread_timer *in_q, *next;
	int irq_flag;

	irq_flag = irq_lock();
	SYS_DLIST_FOR_EACH_CONTAINER_SAFE(&thread->thread_timer_q, in_q, next, node) {
		if (ttimer == in_q) {
			sys_dlist_remove(&in_q->node);
			irq_unlock(irq_flag);
			return;
		}
	}

	irq_unlock(irq_flag);
}

static void _thread_timer_insert(struct k_thread *thread, struct thread_timer *ttimer,
				 u32_t expiry_time)
{
	int irq_flag;
	sys_dlist_t *thread_timer_q = &thread->thread_timer_q;
	struct thread_timer *in_q;

	irq_flag = irq_lock();
	SYS_DLIST_FOR_EACH_CONTAINER(thread_timer_q, in_q, node) {
		if (compare_time(ttimer->expiry_time, in_q->expiry_time) < 0) {
			sys_dlist_insert_before(thread_timer_q, &in_q->node,
						&ttimer->node);
			irq_unlock(irq_flag);
			return;
		}
	}

	sys_dlist_append(thread_timer_q, &ttimer->node);
	irq_unlock(irq_flag);
}
#endif

void thread_timer_init(struct thread_timer *ttimer, thread_timer_expiry_t expiry_fn,
		       void *expiry_fn_arg)
{
	__ASSERT(ttimer != NULL, "");

	if(!ttimer){
		return;
	}

	TT_DEBUG("timer %p: init func %p arg 0x%x\n", ttimer,
		ttimer->expiry_fn, ttimer->expiry_fn_arg);
#ifndef THREAD_TIMER_ROM_CODE
	/* remove thread timer if already submited */
	_thread_timer_remove(_current, ttimer);

	memset(ttimer, 0, sizeof(struct thread_timer));
	ttimer->expiry_time = UINT_MAX;
	ttimer->expiry_fn = expiry_fn;
	ttimer->expiry_fn_arg = expiry_fn_arg;
	ttimer->tid = _current;
	sys_dlist_init(&ttimer->node);
#else
	rom_thread_timer_init(&_current->thread_timer_q, _current, ttimer, expiry_fn, expiry_fn_arg);
#endif
}

void thread_timer_start(struct thread_timer *ttimer, s32_t duration, s32_t period)
{
	__ASSERT((ttimer != NULL) && (ttimer->expiry_fn != NULL), "");

	if(!ttimer){
		return;
	}

	if(ttimer->tid && ttimer->tid != _current){
		printk("ERROR:thread timer(0x%p, 0x%p) start invalid (%d->%d)\n", ttimer, ttimer->expiry_fn, k_thread_priority_get(ttimer->tid), \
			k_thread_priority_get(_current));
		panic(NULL);
	}

#ifndef THREAD_TIMER_ROM_CODE
	_thread_timer_remove(_current, ttimer);

	ttimer->expiry_time = k_uptime_get_32() + duration;
	ttimer->period = period;
	ttimer->duration = duration;

	TT_DEBUG("timer %p: start duration %d period %d, expiry_time %d\n",
		ttimer, duration, period, ttimer->expiry_time);

	_thread_timer_insert(_current, ttimer, ttimer->expiry_time);
#else
	rom_thread_timer_start(&_current->thread_timer_q, ttimer, duration, period, k_uptime_get_32());
#endif

}
/*
static struct k_thread * thread_timer_search_thread(s32_t prio)
{
	struct k_thread *thread_list = NULL;

	thread_list   = (struct k_thread *)k_current_get();
	while (thread_list != NULL) {
		if(k_thread_priority_get(thread_list) == prio){
			break;
		}
		thread_list = (struct k_thread *)thread_list->next_thread;
	}

	return thread_list;
}
*/
void thread_timer_start_prio(struct thread_timer *ttimer, s32_t duration, s32_t period, k_tid_t tid)
{
	struct k_thread *thread_list;

	__ASSERT((ttimer != NULL) && (ttimer->expiry_fn != NULL), "");

	if(!ttimer){
		return;
	}

	if(ttimer->tid && ttimer->tid != tid){
		printk("ERROR:thread timer(0x%p, 0x%p) start invalid (%d->%d)\n", ttimer, ttimer->expiry_fn, k_thread_priority_get(ttimer->tid), \
			k_thread_priority_get(tid));
		panic(NULL);
	}

//	thread_list = thread_timer_search_thread(priority);
	thread_list = tid;

	__ASSERT(thread_list != NULL, "");

#ifndef THREAD_TIMER_ROM_CODE
	_thread_timer_remove(thread_list, ttimer);

	ttimer->expiry_time = k_uptime_get_32() + duration;
	ttimer->period = period;
	ttimer->duration = duration;

	TT_DEBUG("timer %p: start duration %d period %d, expiry_time %d\n",
		ttimer, duration, period, ttimer->expiry_time);

	_thread_timer_insert(thread_list, ttimer, ttimer->expiry_time);
#else
	rom_thread_timer_start(&thread_list->thread_timer_q, ttimer, duration, period, k_uptime_get_32());
#endif
}
void thread_timer_stop(struct thread_timer *ttimer)
{
	__ASSERT(ttimer != NULL, "");

	if(!ttimer){
		return;
	}

	if(ttimer->tid && ttimer->tid != _current){
		printk("ERROR:thread timer(0x%p, 0x%p) stop invalid (%d->%d)\n", ttimer, ttimer->expiry_fn, k_thread_priority_get(ttimer->tid), \
			k_thread_priority_get(_current));
		panic(NULL);
	}

	TT_DEBUG("timer %p: stop\n", ttimer);

#ifndef THREAD_TIMER_ROM_CODE
	_thread_timer_remove(_current, ttimer);
#else
	rom_thread_timer_stop(&_current->thread_timer_q, ttimer);
#endif
}

bool thread_timer_is_running(struct thread_timer *ttimer)
{
	__ASSERT(ttimer != NULL, "");

#ifndef THREAD_TIMER_ROM_CODE
	struct thread_timer *in_q;

	SYS_DLIST_FOR_EACH_CONTAINER(&_current->thread_timer_q, in_q, node) {
		if (ttimer == in_q)
			return true;
	}

	return false;
#else
	return rom_thread_timer_is_running(&_current->thread_timer_q, ttimer);
#endif
}

int thread_timer_next_timeout(void)
{
#ifndef THREAD_TIMER_ROM_CODE
	struct thread_timer *ttimer;
	int timeout;

	ttimer = SYS_DLIST_PEEK_HEAD_CONTAINER(&_current->thread_timer_q, ttimer, node);
	if (ttimer) {
		timeout = (int)((u32_t)ttimer->expiry_time - k_uptime_get_32());
		return (timeout < 0) ? K_NO_WAIT : timeout;
	}

	return K_FOREVER;
#else
	return rom_thread_timer_next_timeout(&_current->thread_timer_q, k_uptime_get_32);
#endif
}

void thread_timer_handle_expired(void)
{
#ifndef THREAD_TIMER_ROM_CODE
	struct thread_timer *ttimer;
	u32_t cur_time;
	int irq_flag;

	cur_time = k_uptime_get_32();

	do {
		irq_flag = irq_lock();
		ttimer = SYS_DLIST_PEEK_HEAD_CONTAINER(
				&_current->thread_timer_q, ttimer, node);
		if (!ttimer || (compare_time(ttimer->expiry_time, cur_time) > 0)) {
			/* no expired thread timer */
			irq_unlock(irq_flag);
			break;
		}

		/* remove this expiry thread timer */
		sys_dlist_remove(&ttimer->node);

		irq_unlock(irq_flag);

		if (ttimer->expiry_fn) {
			/* resubmit this thread timer if it is a period timer */
			if (ttimer->period != 0) {
				thread_timer_start(ttimer, ttimer->period,
						   ttimer->period);
			}

			TT_DEBUG("timer %p: call %p\n", ttimer, ttimer->expiry_fn);

			/* invoke thread timer expiry function */
			ttimer->expiry_fn(ttimer, ttimer->expiry_fn_arg);
		}
	} while (1);
#else
	rom_thread_timer_handle_expired(&_current->thread_timer_q, k_uptime_get_32);
#endif
}


