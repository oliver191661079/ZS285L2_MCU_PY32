/*
 * Copyright (c) 2013-2016 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 *
 *
 * This file is also included by assembly language files which must #define
 * _ASMLANGUAGE before including this header file.  Note that kernel
 * assembly source files obtains structure offset values via "absolute symbols"
 * in the offsets.o module.
 */

/* this file is only meant to be included by kernel_structs.h */

#ifndef _kernel_arch_func__h_
#define _kernel_arch_func__h_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _ASMLANGUAGE
extern void _FaultInit(void);
extern void _CpuIdleInit(void);
static ALWAYS_INLINE void kernel_arch_init(void)
{
	_CpuIdleInit();
}

static ALWAYS_INLINE void
_arch_switch_to_main_thread(struct k_thread *main_thread,
			    k_thread_stack_t main_stack,
			    size_t main_stack_size, _thread_entry_t _main)
{
	/* get high address of the stack, i.e. its start (stack grows down) */
	char *start_of_main_stack;

	start_of_main_stack = K_THREAD_STACK_BUFFER(main_stack) + main_stack_size;
	start_of_main_stack = (void *)STACK_ROUND_DOWN(start_of_main_stack);

	_current = main_thread;

	/* the ready queue cache already contains the main thread */

	__asm__ __volatile__(

		/* move to main() thread stack */
		"mov     sp, %0 \t\n"
		"psrset  ie \t\n"
		"mov     r0, %1 \t\n"
		"jsr     %2 \t\n"

		:
		: "r"(start_of_main_stack),
		  "r"(_main), "r"(_thread_entry)

		: "r0", "r1", "sp"
	);

	CODE_UNREACHABLE;
}

static ALWAYS_INLINE void
_set_thread_return_value(struct k_thread *thread, unsigned int value)
{
	thread->arch.swap_return_value = value;
}

extern int _arch_irq_vector_read(void);
extern int _arch_exception_vector_read(void);

static ALWAYS_INLINE int _IsInIsr(void)
{
    return (_arch_irq_vector_read() >= 0);
}

static ALWAYS_INLINE int _IsInExc(void)
{
    return (_arch_exception_vector_read() > 0);
}

static ALWAYS_INLINE int __IsInExc_EXCLUDE_TSPEND(void)
{
	return ((_arch_exception_vector_read() > 0 )  && (_arch_exception_vector_read() != 22));
}

extern void k_cpu_atomic_idle(unsigned int key);

#define _is_in_isr() (_IsInIsr())

#define _is_in_exc() (_IsInExc())

#define _is_in_exc_exclude_tspend() (__IsInExc_EXCLUDE_TSPEND())

extern void _IntLibInit(void);

extern void arch_coredump_info_dump(const z_arch_esf_t *esf);

extern uint16_t arch_coredump_tgt_code_get(void);


#endif /* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* _kernel_arch_func__h_ */
