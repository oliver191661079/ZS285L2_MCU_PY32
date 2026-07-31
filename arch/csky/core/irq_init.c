/*
 * Copyright (c) 2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 *
 */

#include <toolchain.h>
#include <linker/sections.h>
#include <kernel.h>
#include <arch/cpu.h>

/**
 *
 * @brief Initialize interrupts
 *
 * Ensures all interrupts have their priority set to _EXC_IRQ_DEFAULT_PRIO and
 * not 0, which they have it set to when coming out of reset. This ensures that
 * interrupt locking via BASEPRI works as expected.
 *
 * @return N/A
 */

void _IntLibInit(void)
{
	int irq = 0;

	for (; irq < CONFIG_NUM_IRQS; irq++) {
	}
}
