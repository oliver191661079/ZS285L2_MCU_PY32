/*
 * Copyright (c) 2013-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 *
 *
 * All of the absolute symbols defined by this module will be present in the
 * final kernel ELF image (due to the linker's reference to the _OffsetAbsSyms
 * symbol).
 *
 * INTERNAL
 * It is NOT necessary to define the offset for every member of a structure.
 * Typically, only those members that are accessed by assembly language routines
 * are defined; however, it doesn't hurt to define all fields for the sake of
 * completeness.
 */

#include <gen_offset.h>
#include <kernel_structs.h>
#include <kernel_offsets.h>

GEN_OFFSET_SYM(_thread_arch_t, basepri);
GEN_OFFSET_SYM(_thread_arch_t, swap_return_value);

#ifdef CONFIG_FLOAT
GEN_OFFSET_SYM(_thread_arch_t, preempt_float);
#endif

GEN_OFFSET_SYM(_caller_saved_t, r0);
GEN_OFFSET_SYM(_caller_saved_t, r1);
GEN_OFFSET_SYM(_caller_saved_t, r2);
GEN_OFFSET_SYM(_caller_saved_t, r3);
GEN_OFFSET_SYM(_caller_saved_t, r12);
GEN_OFFSET_SYM(_caller_saved_t, r13);
GEN_OFFSET_SYM(_caller_saved_t, lr);

#ifdef CONFIG_FLOAT
GEN_OFFSET_SYM(_esf_t, s);
GEN_OFFSET_SYM(_esf_t, fpscr);
#endif

GEN_ABSOLUTE_SYM(___caller_saved_t_SIZEOF, sizeof(_caller_saved_t));

GEN_OFFSET_SYM(_callee_saved_t, r4);
GEN_OFFSET_SYM(_callee_saved_t, r5);
GEN_OFFSET_SYM(_callee_saved_t, r6);
GEN_OFFSET_SYM(_callee_saved_t, r7);
GEN_OFFSET_SYM(_callee_saved_t, r8);
GEN_OFFSET_SYM(_callee_saved_t, r9);
GEN_OFFSET_SYM(_callee_saved_t, r10);
GEN_OFFSET_SYM(_callee_saved_t, r11);
GEN_OFFSET_SYM(_callee_saved_t, sp);
GEN_OFFSET_SYM(_callee_saved_t, pc);
GEN_OFFSET_SYM(_callee_saved_t, psr);
GEN_OFFSET_SYM(_callee_saved_t, is_irq);

/* size of the entire preempt registers structure */

GEN_ABSOLUTE_SYM(___callee_saved_t_SIZEOF, sizeof(struct _callee_saved));

/* size of the struct tcs structure sans save area for floating point regs */

#ifdef CONFIG_FLOAT
GEN_ABSOLUTE_SYM(_K_THREAD_NO_FLOAT_SIZEOF, sizeof(struct k_thread) -
					    sizeof(struct _preempt_float));
#else
GEN_ABSOLUTE_SYM(_K_THREAD_NO_FLOAT_SIZEOF, sizeof(struct k_thread));
#endif

GEN_ABS_SYM_END
