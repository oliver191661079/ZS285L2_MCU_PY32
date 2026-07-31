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

#ifndef _kernel_arch_thread__h_
#define _kernel_arch_thread__h_

#ifdef __cplusplus
extern "C" {
#endif

#include <toolchain.h>
#include <linker/sections.h>
#include <arch/cpu.h>

#ifndef _ASMLANGUAGE
#include <kernel.h>
#include <stdint.h>
#include <misc/dlist.h>
#include <atomic.h>
#endif

#ifndef _ASMLANGUAGE

struct _caller_saved {
	uint32_t r0;    // r0
	uint32_t r1;    // r1
	uint32_t r2;    // r2
	uint32_t r3;    // r3
	uint32_t r12;    // r12
	uint32_t r13;    // r13
	uint32_t lr;    // r15
};

typedef struct _caller_saved _caller_saved_t;

struct _callee_saved {
	uint32_t r4;  /* r4 */
	uint32_t r5;  /* r5 */
	uint32_t r6;  /* r6 */
	uint32_t r7;  /* r7 */
	uint32_t r8;  /* r8 */
	uint32_t r9;  /* r9 */
	uint32_t r10;  /* r10 */
	uint32_t r11;  /* r11 */
	uint32_t sp; /* r14 */
	uint32_t pc;
	uint32_t psr;
	uint32_t is_irq;
};

typedef struct _callee_saved _callee_saved_t;

#endif /* _ASMLANGUAGE */

/* stacks */

#define STACK_ALIGN_SIZE 4

#define STACK_ROUND_UP(x) ROUND_UP(x, STACK_ALIGN_SIZE)
#define STACK_ROUND_DOWN(x) ROUND_DOWN(x, STACK_ALIGN_SIZE)

#ifdef CONFIG_CPU_CORTEX_M
#include <cortex_m/stack.h>
#include <cortex_m/exc.h>
#endif

#ifndef _ASMLANGUAGE

#ifdef CONFIG_FLOAT
struct _preempt_float {
	float  s16;
	float  s17;
	float  s18;
	float  s19;
	float  s20;
	float  s21;
	float  s22;
	float  s23;
	float  s24;
	float  s25;
	float  s26;
	float  s27;
	float  s28;
	float  s29;
	float  s30;
	float  s31;
};
#endif

struct _thread_arch {

	/* interrupt locking key */
	uint32_t basepri;

	/* r0 in stack frame cannot be written to reliably */
	volatile uint32_t swap_return_value;

#ifdef CONFIG_FLOAT
	/*
	 * No cooperative floating point register set structure exists for
	 * the Cortex-M as it automatically saves the necessary registers
	 * in its exception stack frame.
	 */
	struct _preempt_float  preempt_float;
#endif
};

typedef struct _thread_arch _thread_arch_t;

#endif /* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* _kernel_arch_data__h_ */
