/*
 * Copyright (c) 2013-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 *
 */

#include <kernel_structs.h>
#include <offsets_short.h>
#include <toolchain.h>
#include <arch/cpu.h>

GTEXT(__swap)
GTEXT(trap0_handler)
GTEXT(__pendsv)
GTEXT(write_400_reg)
GDATA(_k_neg_eagain)

GDATA(_kernel)

SECTION_FUNC(TEXT, write_400_reg)
    st.b      	r1, (r0, 0x0)
    st.b      	r1, (r0, 0x0)
    ld.b      	r1, (r0, 0x0)
    rts

/**
 *
 */

SECTION_FUNC(TEXT, __tspend)
	bkpt

/**
 *
 * @brief Service call handler
 *
 * The service call (svc) is only used in _Swap() to enter handler mode so we
 * can go through the PendSV exception to perform a context switch.
 *
 * @return N/A
 */

SECTION_FUNC(TEXT, trap0_handler)
#ifdef CONFIG_IRQ_OFFLOAD
	subi    sp, 36
	stm     r0-r3, (sp)
	stw     r12, (sp, 16)
	stw     r13, (sp, 20)
	stw     r15, (sp, 24)

	mfcr    r0, epc
	addi    r0, 4
	stw     r0, (sp, 28)
	mfcr    r0, epsr
	stw     r0, (sp, 32)

	lrw     r0, _kernel
	ldw     r1, (r0, 0)
	addi    r1, 1
	stw     r1, (r0, 0)

	jbsr    _irq_do_offload

	lrw     r0, _kernel
	ldw     r1, (r0, 0)
	subi    r1, 1
	stw     r1, (r0, 0)

	ldw     r0, (sp, 28)
	mtcr    r0, epc
	ldw     r0, (sp, 32)
	mtcr    r0, epsr

	ldm     r0-r3, (sp)
	ldw     r12, (sp, 16)
	ldw     r13, (sp, 20)
	ldw     r15, (sp, 24)
	addi    sp, 36
#endif
	rte

/**
 *
 * @brief Initiate a cooperative context switch
 *
 * The _Swap() routine is invoked by various kernel services to effect
 * a cooperative context context switch.  Prior to invoking _Swap(), the caller
 * disables interrupts via irq_lock() and the return 'key' is passed as a
 * parameter to _Swap().  The 'key' actually represents the BASEPRI register
 * prior to disabling interrupts via the BASEPRI mechanism.
 *
 * _Swap() itself does not do much.
 *
 * It simply stores the intlock key (the BASEPRI value) parameter into
 * current->basepri, and then triggers a service call exception (svc) to setup
 * the PendSV exception, which does the heavy lifting of context switching.

 * This is the only place we have to save BASEPRI since the other paths to
 * __pendsv all come from handling an interrupt, which means we know the
 * interrupts were not locked: in that case the BASEPRI value is 0.
 *
 * Given that _Swap() is called to effect a cooperative context switch,
 * only the caller-saved integer registers need to be saved in the TCS of the
 * outgoing thread. This is all performed by the hardware, which stores it in
 * its exception stack frame, created when handling the svc exception.
 *
 * On Cortex-M0/M0+ the intlock key is represented by the PRIMASK register,
 * as BASEPRI is not available.
 *
 * @return may contain a return value setup by a call to
 * _set_thread_return_value()
 *
 * C function prototype:
 *
 * unsigned int _Swap (unsigned int basepri);
 *
 */
#ifndef CONFIG_SWAP_TSPEND
SECTION_FUNC(TEXT, __swap)
    /* 从kernel结构体得到thread结构体 */
	lrw     r1, _kernel
	ldw     r2, (r1, _kernel_offset_to_current)

	/*
	 * Set _Swap()'s default return code to -EAGAIN. This eliminates the need
	 * for the timeout code to set it itself.
	 */
	/* _kernel->current->arch.swap_return_value = _k_neg_eagain */ 
	lrw     r3, _k_neg_eagain
	ldw     r3, (r3)
	stw     r3, (r2, _thread_offset_to_swap_return_value)

    /* memcpy(&_kernel->current->callee_saved[0], r4-r11, 32) */
    /* callee栈帧布局
            is_irq
            psr
            pc
            sp
            r11
            r10
            r9
            r8
            r7
            r6
            r5
            r4
     *   
     */
	lrw     r3, _thread_offset_to_callee_saved
	add     r3, r2
	stm     r4-r11, (r3)

	/* _kernel->current->callee_saved[sp] = sp
       _kernel->current->callee_saved[pc] = lr
       _kernel->current->callee_saved[psr] = r0
       _kernel->current->callee_saved[is_irq] = 0
	*/
	addi    r3, 32
	stw     sp, (r3)
	stw     lr, (r3, 4)
	stw     r0, (r3, 8)
	movi    r0, 0
	stw     r0, (r3, 12)

	/* _kernel->current = _kernel->ready_q.cache
	*/
	ldw     r2, (r1, _kernel_offset_to_ready_q_cache)
	stw     r2, (r1, _kernel_offset_to_current)

	/* memcpy(r4-r11, &_kernel->current->callee_saved[0], 32)
	*/
	lrw     r3, _thread_offset_to_callee_saved
	add     r0, r3, r2
	ldm     r4-r11, (r0)

	/* sp = _kernel->current->callee_saved[sp] 
       lr = _kernel->current->callee_saved[pc]
       epsr = _kernel->current->callee_saved[psr]
	*/	
	addi    r0, 32
	ldw     sp, (r0)

	ldw     r3, (r0, 4)
	mtcr    r3, epc
	ldw     r3, (r0, 8)
	mtcr    r3, epsr

	/* if(_kernel->current->callee_saved[psr] == 0)
	        goto Lnotirq
	*/
	ldw     r0, (r0, 12)
	btsti   r0, 0
	bf      .Lnotirq

	/* memcpy(r0-r3, &_kernel->ready_q.cache.caller_saved[0], 16)
	   memcpy(r12-r13, &_kernel->ready_q.cache.caller_saved[16], 8)
	   r15 = _kernel->ready_q.cache.caller_saved[24]
	*/	
	ldw     r15, (r1, _kernel_offset_to_ready_q_cache)
	addi    r15, ___thread_t_caller_saved_OFFSET
	ldm     r0-r3, (r15)
	addi    r15, 16
	ldm     r12-r13, (r15)
	ldw     r15, (r15, 8)

	/* PC = EPC */
	rte

.Lnotirq:
	/* coming back from exception, r2 still holds the pointer to _current */
	ldw     r0, (r2, _thread_offset_to_swap_return_value)
	rte     
#endif
