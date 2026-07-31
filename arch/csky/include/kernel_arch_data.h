/*
 * Copyright (c) 2013-2016 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 *
 * This file is also included by assembly language files which must #define
 * _ASMLANGUAGE before including this header file.  Note that kernel
 * assembly source files obtains structure offset values via "absolute symbols"
 * in the offsets.o module.
 */

#ifndef _kernel_arch_data__h_
#define _kernel_arch_data__h_

#ifdef __cplusplus
extern "C" {
#endif

#include <toolchain.h>
#include <linker/sections.h>
#include <arch/cpu.h>
#include <kernel_arch_thread.h>

#ifndef _ASMLANGUAGE
#include <kernel.h>
#include <nano_internal.h>
#include <zephyr/types.h>
#include <misc/dlist.h>
#include <atomic.h>
#endif

#ifndef _ASMLANGUAGE

typedef struct __esf _esf_t;

#endif /* _ASMLANGUAGE */

/* stacks */

#define STACK_ALIGN_SIZE 4

#define STACK_ROUND_UP(x) ROUND_UP(x, STACK_ALIGN_SIZE)
#define STACK_ROUND_DOWN(x) ROUND_DOWN(x, STACK_ALIGN_SIZE)

#ifndef _ASMLANGUAGE

struct _kernel_arch {
	/* empty */
};

typedef struct _kernel_arch _kernel_arch_t;

extern K_THREAD_STACK_DEFINE(_interrupt_stack, CONFIG_ISR_STACK_SIZE);

#ifdef CONFIG_BOOTINFO_MEMORY
extern __in_section_unique(BOOTINFO_DATA) u32_t boot_info_addr;
#endif

#endif /* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* _kernel_arch_data__h_ */
