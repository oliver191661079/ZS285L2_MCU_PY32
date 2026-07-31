/*
 * Copyright (c) 2013-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 *
 */

#include <offsets_short.h>
#include <toolchain.h>
#include <linker/sections.h>
#include <arch/cpu.h>
#ifdef CONFIG_TICKLESS_IDLE
#include <kernel_structs.h>
#endif

GTEXT(_CpuIdleInit)
#ifdef CONFIG_SYS_POWER_MANAGEMENT
GTEXT(_NanoIdleValGet)
GTEXT(_NanoIdleValClear)
#endif
GTEXT(k_cpu_idle)
GTEXT(k_cpu_atomic_idle)

#ifdef CONFIG_SOC_DVFS_CPU_IDLE_LOW_POWER
GTEXT(soc_dvfs_cpu_enter_lowpower)
GTEXT(soc_dvfs_cpu_exit_lowpower)
#endif

/**
 *
 * @brief Initialization of CPU idle
 *
 * Only called by nanoArchInit(). Sets SEVONPEND bit once for the system's
 * duration.
 *
 * @return N/A
 *
 * C function prototype:
 *
 * void _CpuIdleInit (void);
 */

SECTION_FUNC(TEXT, _CpuIdleInit)
	rts

#ifdef CONFIG_SYS_POWER_MANAGEMENT

/**
 *
 * @brief Get the kernel idle setting
 *
 * Returns the kernel idle setting, in ticks. Only called by __systick().
 *
 * @return the requested number of ticks for the kernel to be idle
 *
 * C function prototype:
 *
 * int32_t _NanoIdleValGet (void);
 */

SECTION_FUNC(TEXT, _NanoIdleValGet)
	lrw     r0, _kernel
	lrw     r1, _kernel_offset_to_idle
	add     r0, r1
	ldw     r0, (r0, 0)
	rts

/**
 *
 * @brief Clear the kernel idle setting
 *
 * Sets the kernel idle setting to 0. Only called by __systick().
 *
 * @return N/A
 *
 * C function prototype:
 *
 * void _NanoIdleValClear (void);
 */

SECTION_FUNC(TEXT, _NanoIdleValClear)
	lrw     r0, _kernel
	eors.n  r1, r1
	lrw     r2, _kernel_offset_to_idle
	add     r0, r2
	stw     r1, (r0, 0)
	rts

#endif /* CONFIG_SYS_POWER_MANAGEMENT */

/**
 *
 * @return N/A
 *
 * C function prototype:
 *
 * void k_cpu_idle (void);
 */

SECTION_FUNC(TEXT, k_cpu_idle)
	push    lr

#ifdef CONFIG_KERNEL_EVENT_LOGGER_SLEEP
	bsr    _sys_k_event_logger_enter_sleep
#endif

	psrclr  ie

#ifdef CONFIG_SOC_DVFS_CPU_IDLE_LOW_POWER
	bsr soc_dvfs_cpu_enter_lowpower
#endif

	wait

#ifdef CONFIG_SOC_DVFS_CPU_IDLE_LOW_POWER
	bsr soc_dvfs_cpu_exit_lowpower
#endif

	psrset  ie

	pop    lr

/**
 *
 * @brief Atomically re-enable interrupts and enter low power mode
 *
 * INTERNAL
 * The requirements for k_cpu_atomic_idle() are as follows:
 * 1) The enablement of interrupts and entering a low-power mode needs to be
 *    atomic, i.e. there should be no period of time where interrupts are
 *    enabled before the processor enters a low-power mode.  See the comments
 *    in k_lifo_get(), for example, of the race condition that occurs
 *    if this requirement is not met.
 *
 * 2) After waking up from the low-power mode, the interrupt lockout state
 *    must be restored as indicated in the 'imask' input parameter.
 *
 * @return N/A
 *
 * C function prototype:
 *
 * void k_cpu_atomic_idle (unsigned int imask);
 */

SECTION_FUNC(TEXT, k_cpu_atomic_idle)
#ifdef CONFIG_KERNEL_EVENT_LOGGER_SLEEP
	push    lr
	bl      _sys_k_event_logger_enter_sleep
	pop     lr
#endif

	rts
