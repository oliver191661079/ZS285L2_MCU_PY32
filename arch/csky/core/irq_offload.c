/*
 * Copyright (c) 2015 Intel corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 */

#include <kernel.h>
#include <irq_offload.h>

static irq_offload_routine_t offload_routine;
static void *offload_param;

/* Called by trap0_handler */
void _irq_do_offload(void)
{
	irq_offload_routine_t tmp;

	if (!offload_routine) {
		return;
	}

	tmp = offload_routine;
	offload_routine = NULL;

	tmp((void *)offload_param);
}

void irq_offload(irq_offload_routine_t routine, void *parameter)
{
	int key;

	key = irq_lock();
	offload_routine = routine;
	offload_param = parameter;

	__asm__ volatile ("trap    0" : : : "memory");

	irq_unlock(key);
}
