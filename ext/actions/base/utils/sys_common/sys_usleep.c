/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file usleep interface
 */

#include <kernel.h>
#include <hrtimer.h>

#undef THREAD_USLEEP_LATENCY_BENCH

struct usleep_ctx
{
    struct k_sem sem;
#ifdef CONFIG_ACTS_HRTIMER
    struct hrtimer timer;
#endif
};

static void usleep_timer_handler(struct hrtimer *timer, void* pdata)
{
    struct usleep_ctx *ctx = (struct usleep_ctx *)pdata;

    k_sem_give(&ctx->sem);
}

void thread_usleep(uint32_t usec)
{
    struct usleep_ctx ctx;

    __ASSERT(!k_is_in_isr(), "");

#ifdef THREAD_USLEEP_LATENCY_BENCH
    uint32_t cur_time, diff_time, read_time;

    cur_time = k_cycle_get_32();
    read_time = k_cycle_get_32() - cur_time;
    cur_time = k_cycle_get_32();
#endif

    k_sem_init(&ctx.sem, 0, 1);
#ifdef CONFIG_ACTS_HRTIMER
    hrtimer_init(&ctx.timer, usleep_timer_handler, (void *)&ctx);

    hrtimer_start(&ctx.timer, usec, 0);

    k_sem_take(&ctx.sem, K_FOREVER);
#endif

#ifdef THREAD_USLEEP_LATENCY_BENCH
    diff_time = k_cycle_get_32() - cur_time;

    diff_time -= read_time;

    printk("diff time %d\n", diff_time/24);
#endif
}


