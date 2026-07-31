/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <soc.h>
#include <system_timer.h>
#include <init.h>
#ifdef CONFIG_ACTS_HRTIMER
#include <hrtimer.h>
#endif

#define TIMER_MAX_CYCLES_VALUE			0xfffffffful

static void timer_reg_wait(void)
{
	volatile int i;

	for (i = 0; i < 10; i++)
		;
}

#ifndef CONFIG_BUSY_WAIT_USES_ALTERNATE_CLOCK
static volatile u32_t accumulated_cycle_count;

static ALWAYS_INLINE void update_accumulated_count(void)
{
	accumulated_cycle_count += sys_clock_hw_cycles_per_tick;
}
#endif

#ifndef CONFIG_ACTS_HRTIMER
static void acts_timer0_isr(void *arg)
{
	ARG_UNUSED(arg);

	/* clear pending */
	sys_write32(sys_read32(T0_CTL), T0_CTL);

	_sys_clock_tick_announce();

#ifndef CONFIG_BUSY_WAIT_USES_ALTERNATE_CLOCK
	update_accumulated_count();
#endif
}
#else
static struct hrtimer tick_timer;

static void acts_tick_timer(struct hrtimer *ttimer, void *arg)
{
	ARG_UNUSED(arg);

	_sys_clock_tick_announce();

#ifndef CONFIG_BUSY_WAIT_USES_ALTERNATE_CLOCK
	update_accumulated_count();
#endif
}

#endif

int _sys_clock_driver_init(struct device *device)
{
	ARG_UNUSED(device);


#ifndef CONFIG_ACTS_HRTIMER
	/* init timer0 as clock event device */
	acts_clock_peripheral_enable(CLOCK_ID_TIMER0);

	/* clear pending & disable timer */
	sys_write32(0x1, T0_CTL);
	timer_reg_wait();

	/* initial counter value for one tick */
	sys_write32(sys_clock_hw_cycles_per_tick, T0_VAL);

	/* enable counter, reload, irq */
	sys_write32(0x26, T0_CTL);

	IRQ_CONNECT(IRQ_ID_TIMER0, CONFIG_IRQ_PRIO_NORMAL, acts_timer0_isr, 0, 0);
	irq_enable(IRQ_ID_TIMER0);
#else
    hrtimer_runtime_init();

    hrtimer_init(&tick_timer, acts_tick_timer, NULL);

    hrtimer_start(&tick_timer, 1000000 / CONFIG_SYS_CLOCK_TICKS_PER_SEC, 1000000 / CONFIG_SYS_CLOCK_TICKS_PER_SEC);
#endif
	return 0;
}

u32_t __ramfunc _timer_cycle_get_32(void)
{
#ifdef CONFIG_BUSY_WAIT_USES_ALTERNATE_CLOCK
	/* T0 work mode is counter down */
	return TIMER_MAX_CYCLES_VALUE  - sys_read32(T1_CNT);
#else
	u32_t acc, elapsed_cycles;

	do {
		acc = accumulated_cycle_count;
		/* T0 work mode is counter down */
		elapsed_cycles = sys_clock_hw_cycles_per_tick - sys_read32(T0_CNT);
	} while (acc != accumulated_cycle_count);

	return (acc + elapsed_cycles);
#endif
}
static int sys_timer_cycle_init(struct device *device)
{
    ARG_UNUSED(device);

#ifdef CONFIG_BUSY_WAIT_USES_ALTERNATE_CLOCK
	/* init timer1 as clock source device */
	acts_clock_peripheral_enable(CLOCK_ID_TIMER1);

	/* clear pending & disable timer */
	sys_write32(0x1, T1_CTL);
	timer_reg_wait();

	/* initial initial counter value */
	sys_write32(TIMER_MAX_CYCLES_VALUE, T1_VAL);

	/* enable counter, reload */
	sys_write32(0x24, T1_CTL);
#endif

    return 0;
}


SYS_INIT(sys_timer_cycle_init, PRE_KERNEL_1, 0);
