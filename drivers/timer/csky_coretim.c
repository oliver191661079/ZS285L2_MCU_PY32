/*
 * Copyright (c) 2013-2015 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <toolchain.h>
#include <linker/sections.h>
#include <misc/__assert.h>
#include <sys_clock.h>
#include <drivers/system_timer.h>
#include <arch/csky/csi.h>
#include <csi_core.h>
#include <kernel_structs.h>
#include <arch/csky/irq.h>
#include <irq.h>

/* running total of timer count */
static volatile u32_t clock_accumulated_count;

static void coretim_clr_irq(void)
{
	uint32_t reg;

    reg = CORET->CTRL;
    reg |= ~CORET_CTRL_COUNTFLAG_Msk;
    CORET->CTRL = reg;

}

static void coretim_stop(void)
{
    CORET->CTRL = 0;
}

void _timer_int_handler(void *unused)
{
	ARG_UNUSED(unused);

#ifdef CONFIG_EXECUTION_BENCHMARKING
	extern void read_systick_start_of_tick_handler(void);
	read_systick_start_of_tick_handler();
#endif

#ifdef CONFIG_KERNEL_EVENT_LOGGER_INTERRUPT
	extern void _sys_k_event_logger_interrupt(void);
	_sys_k_event_logger_interrupt();
#endif

	coretim_clr_irq();

	clock_accumulated_count += sys_clock_hw_cycles_per_tick;
	_sys_clock_tick_announce();

#ifdef CONFIG_EXECUTION_BENCHMARKING
	extern void read_systick_end_of_tick_handler(void);
	read_systick_end_of_tick_handler();
#endif

}

int _sys_clock_driver_init(struct device *device)
{
	ARG_UNUSED(device);

    drv_coret_config(sys_clock_hw_cycles_per_tick, CONFIG_CSKY_CORETIM_IRQNUM);

	IRQ_CONNECT(CONFIG_CSKY_CORETIM_IRQNUM, 0, _timer_int_handler, 0, 0);

	_arch_irq_enable(CONFIG_CSKY_CORETIM_IRQNUM);

	return 0;
}

/**
 *
 * @brief Read the platform's timer hardware
 *
 * This routine returns the current time in terms of timer hardware clock
 * cycles.
 *
 * @return up counter of elapsed clock cycles
 *
 * \INTERNAL WARNING
 * systick counter is a 24-bit down counter which is reset to "reload" value
 * once it reaches 0.
 */
u32_t _timer_cycle_get_32(void)
{
#ifdef CONFIG_TICKLESS_KERNEL
	return (u32_t) get_elapsed_count();
#else
	u32_t cac, count;

	do {
		cac = clock_accumulated_count;
		count = drv_coret_get_load() - drv_coret_get_value();
	} while (cac != clock_accumulated_count);

	return cac + count;
#endif
}

/**
 *
 * @brief Stop announcing ticks into the kernel
 *
 * This routine disables the systick so that timer interrupts are no
 * longer delivered.
 *
 * @return N/A
 */
void sys_clock_disable(void)
{
	unsigned int key; /* interrupt lock level */

	key = irq_lock();

	/* disable the systick counter and systick interrupt */

	coretim_stop();

	irq_unlock(key);
}

