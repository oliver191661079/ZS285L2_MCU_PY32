/*
 * Copyright (c) 2013-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 *
 *
 * Provides functions for performing kernel handling when exiting exceptions or
 * interrupts that are installed directly in the vector table (i.e. that are not
 * wrapped around by _isr_wrapper()).
 */

#include <kernel_structs.h>
#include <offsets_short.h>
#include <toolchain.h>
#include <arch/cpu.h>

GTEXT(_ExcExit)
GTEXT(_IntExit)
GDATA(_kernel)

#ifdef CONFIG_TIMESLICING
GTEXT(_update_time_slice_before_swap)
#endif

/**
 *
 * @brief Kernel housekeeping when exiting interrupt handler installed
 *            directly in vector table
 *
 * Kernel allows installing interrupt handlers (ISRs) directly into the vector
 * table to get the lowest interrupt latency possible. This allows the ISR to be
 * invoked directly without going through a software interrupt table. However,
 * upon exiting the ISR, some kernel work must still be performed, namely
 * possible context switching. While ISRs connected in the software interrupt
 * table do this automatically via a wrapper, ISRs connected directly in the
 * vector table must invoke _IntExit() as the *very last* action before
 * returning.
 *
 * e.g.
 *
 * void myISR(void)
 *     {
 *     printk("in %s\n", __FUNCTION__);
 *     doStuff();
 *     _IntExit();
 *     }
 *
 * @return N/A
 */

SECTION_SUBSEC_FUNC(ramfunc, _HandlerModeExit, _IntExit)
#ifndef CONFIG_SWAP_TSPEND
    rts
#else
#ifdef CONFIG_PREEMPT_ENABLED
    lrw r0, _kernel

    ldw r1, (r0, _kernel_offset_to_current)

    /*
     * Non-preemptible thread ? Do not schedule (see explanation of
     * preempt field in kernel_struct.h).
     */
    ldh r2, (r1, _thread_offset_to_preempt)
    cmplti  r2, _PREEMPT_THRESHOLD + 1
    bf _EXIT_EXC    

    ldw r0, (r0, _kernel_offset_to_ready_q_cache)
    cmpne r0, r1
    bf _EXIT_EXC

#ifdef CONFIG_TIMESLICING
    push r15
    bsr _update_time_slice_before_swap 
    ld.w    r15, (sp, 0)
    addi    sp, sp, 4      
#endif /* CONFIG_TIMESLICING */

    /* context switch required, pend the PendSV exception */
    lrw    r0, 0XE000EC08  
    bgeni  r1, 0  
    stw    r1, (r0)
    
_EXIT_EXC:
#endif /* CONFIG_PREEMPT_ENABLED */

#ifdef CONFIG_STACK_SENTINEL
    push r15
    bsr _check_stack_sentinel
    ld.w    r15, (sp, 0)
    addi    sp, sp, 4     
#endif /* CONFIG_STACK_SENTINEL */

    rts
    
#endif


/* _IntExit falls through to _ExcExit (they are aliases of each other) */


/**
 *
 * @brief Kernel housekeeping when exiting exception handler installed
 *            directly in vector table
 *
 * See _IntExit().
 *
 * @return N/A
 */
SECTION_SUBSEC_FUNC(TEXT, _HandlerModeExit, _ExcExit)
    rts

