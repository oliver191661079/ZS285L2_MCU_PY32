/*
 * Copyright (c) 2013-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 *
 */

#ifndef _CSKY_ARCH__H_
#define _CSKY_ARCH__H_

/* Add include for DTS generated information */
#include <irq.h>
#include <arch/csky/asm_inline.h>
#include <arch/csky/irq.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ARM GPRs are often designated by two different names */
#define sys_define_gpr_with_alias(name1, name2) union { uint32_t name1, name2; }

/* APIs need to support non-byte addressable architectures */

#define OCTET_TO_SIZEOFUNIT(X) (X)
#define SIZEOFUNIT_TO_OCTET(X) (X)

#define STACK_ALIGN  4

#define _NANO_ERR_CPU_EXCEPTION (0)     /* Any unhandled exception */
#define _NANO_ERR_INVALID_TASK_EXIT (1) /* Invalid task exit */
#define _NANO_ERR_STACK_CHK_FAIL (2)    /* Stack corruption detected */
#define _NANO_ERR_ALLOCATION_FAIL (3)   /* Kernel Allocation Failure */
#define _NANO_ERR_SPURIOUS_INT (4)      /* Spurious interrupt */
#define _NANO_ERR_KERNEL_OOPS (5)       /* Kernel oops (fatal to thread) */
#define _NANO_ERR_KERNEL_PANIC (6)      /* Kernel panic (fatal to system) */

#ifndef __ASSEMBLER__

#include <arch/csky/sys_io.h>

struct __esf {
        u32_t r0;
        u32_t r1;
        u32_t r2;
        u32_t r3;
        u32_t r4;
        u32_t r5;
        u32_t r6;
        u32_t r7;
        u32_t r8;
        u32_t r9;
        u32_t r10;
        u32_t r11;
        u32_t r12;
        u32_t r13;
        u32_t r14;
        u32_t r15;
        u32_t epsr;
        u32_t epc;
        u32_t vec;
};

struct irq_frame{
        u32_t r15;
        u32_t r0;
        u32_t r1;
        u32_t r2;
        u32_t r3;
        u32_t r12;
        u32_t r13;
        u32_t epsr;
	    u32_t epc;
};

typedef struct __esf NANO_ESF;

typedef struct __esf z_arch_esf_t;

extern NANO_ESF _default_esf;
/** kernel provided routine to report any detected fatal error. */
extern void _NanoFatalErrorHandler(unsigned int reason,
                                                 const NANO_ESF * pEsf);

/** User provided routine to handle any detected fatal error post reporting. */
extern void _SysFatalErrorHandler(unsigned int reason,
                                                const NANO_ESF * pEsf);

extern void panic(const char *err_msg);

#ifndef CONFIG_DISABLE_IRQ_STAT
static ALWAYS_INLINE unsigned int _arch_irq_lock(void)
{
        unsigned long flags;
        __asm__ __volatile__(
                "mfcr   %0, psr \n"
                "psrclr ie      \n"
                :"=r"(flags)
                :
                :"memory"
                );
        return flags;
}

static ALWAYS_INLINE void _arch_irq_unlock(unsigned int flags)
{
        __asm__ __volatile__(
                "mtcr   %0, psr \n"
                :
                :"r" (flags)
                :"memory"
                );
}
#else
extern unsigned int _arch_irq_lock(void);
extern void _arch_irq_unlock(unsigned int flags);
extern void sys_irq_stat_msg_ctl(bool enable);
#endif

#ifdef CONFIG_USE_ROM_TIMER
extern u32_t rom_cycle_get_32(void);

#define _arch_k_cycle_get_32()  rom_cycle_get_32()
#else

extern u32_t _timer_cycle_get_32(void);
#define _arch_k_cycle_get_32()  _timer_cycle_get_32()
#endif

typedef struct sys_irq_flags {
	u32_t keys[2];
} SYS_IRQ_FLAGS;

void sys_irq_lock(SYS_IRQ_FLAGS *flags);
void sys_irq_unlock(const SYS_IRQ_FLAGS *flags);

#endif

#ifdef __cplusplus
}
#endif

#endif /* _CSKY_ARCH__H_ */
