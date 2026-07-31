/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel_structs.h>
#include <offsets_short.h>

GTEXT(trap)
GTEXT(vectorirq_handler)
GTEXT(tspend_handler)
GTEXT(trace_task_end)
GTEXT(trace_task_start)

SECTION_FUNC(TEXT, trap)
	mtcr    r15, cr<14, 1>
	lrw     r15, _default_esf
	stm     r0-r14, (r15)
	mov     r4, r15
	mfcr    r15, cr<14, 1>
	stw     r15, (r4, 0x3c)
	mfcr    r5, epc
	stw     r5, (r4, 0x44)

	/* save current vec */
	mfcr    r5, psr
	lsri    r5, 16
	sextb   r5
	stw     r5, (r4, 0x48)

	/* save epsr and get epsr vec */
	mfcr    r5, epsr
	stw     r5, (r4, 0x40)
	lsri    r5, 16
	sextb   r5

	/* switch to interrupt stack */
	
    /* 判断epsr vect域是否大于等于0，如果是，则已经位于中断栈 */  
    mov  r1, sp
    cmpnei  r5, 0
    jbt trap_stack

    /* 上一个状态不是中断状态，需要切换sp */
    lrw   r2, _interrupt_stack
    addi r2, r2, CONFIG_ISR_STACK_SIZE
    mov  sp, r2
    
trap_stack:  
    /*save old sp and r15 */ 
    subi sp, 4
    stw  r1, (sp, 0)

	lrw     r0, _default_esf
	jbsr    trap_c
//	br      .

    ld.w    r1,  (sp, 0)
    mov     sp,   r1

    ldw     r5, (r4, 0x44)
    mtcr    r5, epc
    ldw     r5, (r4, 0x40)
    mtcr    r5, epsr
    lrw     r15, _default_esf
    ldm     r0-r14, (r15)
    mfcr    r15, cr<14, 1>
    rte

#if 0
SECTION_FUNC(TEXT, vectorirq_handler)
    /* save r0,r1 后面会用 */
	subi    sp, 8
	stw     r0, (sp, 0)
	stw     r1, (sp, 4)
	lrw     r1, _kernel
	ldw     r0, (r1, _kernel_offset_to_current)

    /* memcpy(&_kernel->current->callee_saved[0], r4-r11, 32) */
	mov     r1, r0
	addi    r0, _thread_offset_to_callee_saved
	stm     r4-r11, (r0)

	/* 暂存r0,r1到r4,r6,因r4-r6已经入栈 */
	mov     r4, r1
	mov     r6, r0

    /* 恢复保存的r0,r1 */
	ldw     r0, (sp, 0)
	ldw     r1, (sp, 4)
	addi    sp, 8

	/* _kernel->current->callee_saved[sp] = sp
       _kernel->current->callee_saved[pc] = lr
       _kernel->current->callee_saved[psr] = psr
       _kernel->current->callee_saved[is_irq] = 1
	*/    
	addi    r6, 32
	stw     sp, (r6)
	mfcr    r5, epc
	stw     r5, (r6, 4)
	mfcr    r5, epsr
	stw     r5, (r6, 8)
	movi    r5, 1
	stw     r5, (r6, 12)

	/* memcpy(&_kernel->ready_q.cache.caller_saved[0], r0-r3, 16)
	   memcpy(&_kernel->ready_q.cache.caller_saved[16], r12-r13, 8)
	   _kernel->ready_q.cache.caller_saved[24] = r15
	*/
	addi    r4, ___thread_t_caller_saved_OFFSET
	stm     r0-r3, (r4)
	addi    r4, 16
	stm     r12-r13, (r4)
	stw     r15, (r4, 8)

	/* 进行换栈处理，切换到中断栈执行中断上下文
	*/
	lrw     sp, _interrupt_stack
	addi    sp, CONFIG_ISR_STACK_SIZE

	jbsr    _isr_wrapper

    /* 配置是否在中断退出的时候进行任务切换
    */
#ifdef CONFIG_PREEMPT_ENABLED
	lrw     r1, _kernel
	ldw     r0, (r1, _kernel_offset_to_current)
	ldh     r2, (r0, _thread_offset_to_preempt)
	
    /* if(_kernel->current.base.preempt >= _PREEMPT_THRESHOLD + 1)
            _kernel->current = _kernel->ready_q.cache(任务做了切换)     
    */	
	cmplti  r2, _PREEMPT_THRESHOLD + 1
	bf      .Lret
	ldw     r0, (r1, _kernel_offset_to_ready_q_cache)
	stw     r0, (r1, _kernel_offset_to_current)
#endif

.Lret:
    /* 恢复新就绪的任务的caller和calle寄存器    
    */
	mov     r15, r0
	addi    r15, _thread_offset_to_callee_saved
	ldm     r4-r11, (r15)
	addi    r15, 32
	ldw     sp, (r15)
	ldw     r3, (r15, 4)
	mtcr    r3, epc
	ldw     r3, (r15, 8)
	mtcr    r3, epsr

	ldw     r1, (r15, 12)
	btsti   r1, 0
	bf      .Lnotirq
	mov     r15, r0
	addi    r15, ___thread_t_caller_saved_OFFSET
	ldm     r0-r3, (r15)
	addi    r15, 16
	ldm     r12-r13, (r15)
	ldw     r15, (r15, 8)
	rte

.Lnotirq:
	ldw     r0, (r0, _thread_offset_to_swap_return_value)
	rte
#endif

SECTION_FUNC(ramfunc, vectorirq_handler)
    /* save context */
    subi sp, 32+8
    stm  r0-r3, (sp)
    stw  r12, (sp, 16)
    stw  r13, (sp, 20)

    stw  r9, (sp, 32)
    stw  r10, (sp, 36)

    mfcr r0, epc
    stw  r0, (sp, 28)
    mfcr r0, epsr
    stw  r0, (sp, 24)

    mov  r1, sp

    /* 判断epsr vect域是否大于等于32，如果是，则已经位于中断状态，属于中断嵌套 */  
    lsri    r0, 16                      
    sextb   r0        
    cmphsi  r0, 32
    jbt irq_stack   

    /* 上一个状态不是中断状态，需要切换sp */
switch_stack:
    lrw   r2, _interrupt_stack
    addi r2, r2, CONFIG_ISR_STACK_SIZE
    mov  sp, r2

    /* 切换sp后开中断嵌套 */
irq_stack:
    /*save old sp and r15 */
    subi sp, 8
    stw  r1, (sp, 0)
    stw  r15, (sp, 4)

    psrset ee,ie     /* 开异常和中断 */

#ifdef CONFIG_KERNEL_EVENT_LOGGER_INTERRUPT
    bsr     _sys_k_event_logger_interrupt
#endif

#ifdef CONFIG_KERNEL_EVENT_LOGGER_SLEEP
    bsr     _sys_k_event_logger_exit_sleep
#endif

#ifdef CONFIG_SYS_POWER_MANAGEMENT
    /*
     * All interrupts are disabled when handling idle wakeup.  For tickless
     * idle, this ensures that the calculation and programming of the device
     * for the next timer deadline is not interrupted.  For non-tickless idle,
     * this ensures that the clearing of the kernel idle state is not
     * interrupted.  In each case, _sys_power_save_idle_exit is called with
     * interrupts disabled.
     */
    psrclr ie  /* 关中断 */

    /* is this a wakeup from idle ? */
    lrw     r2, _kernel
    /* requested idle duration, in ticks */
    ldw     r0, (r2, _kernel_offset_to_idle)
    cmpnei  r0, 0

    bf      _idle_state_cleared
    movi    r1, 0
    /* clear kernel idle state */
    stw     r1, (r2, _kernel_offset_to_idle)
    bsr _   sys_power_save_idle_exit
_idle_state_cleared:
    psrset ie     /* 开中断 */
#endif
      
    bsr     _isr_wrapper                /* real call ISR */

    /* exception return is done in _IntExit() */
    bsr     _IntExit

    psrclr ee, ie     /* 关中断 */

    ld.w    r15, (sp, 4)
    ld.w    r1,  (sp, 0)
    mov     sp,   r1
 
    ldw  r9, (sp, 32)
    ldw  r10, (sp, 36)

    ldw r12, (sp, 16)
    ldw r13, (sp, 20)
    ldw r0,  (sp, 24)
    mtcr r0, epsr
    ldw r0,  (sp, 28)
    mtcr r0, epc
    ldm r0-r3, (sp)
    addi sp, 32+8

    rte

SECTION_FUNC(ramfunc, tspend_handler)
    /* save r0,r1 后面会用 */
	subi    sp, 8
	stw     r0, (sp, 0)
	stw     r1, (sp, 4)
	lrw     r1, _kernel
	ldw     r0, (r1, _kernel_offset_to_current)

    /* memcpy(&_kernel->current->callee_saved[0], r4-r11, 32) */
	mov     r1, r0
	addi    r0, _thread_offset_to_callee_saved
	stm     r4-r11, (r0)

	/* 暂存r0,r1到r4,r6,因r4-r6已经入栈 */
	mov     r4, r1
	mov     r6, r0

    /* 恢复保存的r0,r1 */
	ldw     r0, (sp, 0)
	ldw     r1, (sp, 4)
	addi    sp, 8

	/* _kernel->current->callee_saved[sp] = sp
       _kernel->current->callee_saved[pc] = epc
       _kernel->current->callee_saved[psr] = epsr
       _kernel->current->callee_saved[is_irq] = 1
	*/    
	addi    r6, 32
	stw     sp, (r6)
	mfcr    r5, epc
	stw     r5, (r6, 4)
	mfcr    r5, epsr
	stw     r5, (r6, 8)
	movi    r5, 1
	stw     r5, (r6, 12)

	/* memcpy(&_kernel->ready_q.cache.caller_saved[0], r0-r3, 16)
	   memcpy(&_kernel->ready_q.cache.caller_saved[16], r12-r13, 8)
	   _kernel->ready_q.cache.caller_saved[24] = r15
	*/
	addi    r4, ___thread_t_caller_saved_OFFSET
	stm     r0-r3, (r4)
	addi    r4, 16
	stm     r12-r13, (r4)
	stw     r15, (r4, 8)

#ifdef CONFIG_ACTIONS_TRACE
	bsr	    trace_task_end
#endif

#ifdef CONFIG_CPU_LOAD_STAT
	lrw     r0, _kernel
	ldw     r1, (r0, _kernel_offset_to_ready_q_cache)
	ldw     r0, (r0, _kernel_offset_to_current)
	bsr     _sys_cpuload_context_switch
#endif

	lrw     r0, _kernel

	ldw     r1, (r0, _kernel_offset_to_ready_q_cache)
	stw     r1, (r0, _kernel_offset_to_current)	

#ifdef CONFIG_ACTIONS_TRACE
	bsr	    trace_task_start
#endif

#ifdef CONFIG_SWAP_TSPEND_CHECK
	lrw     r0, _kernel
	ldw     r1, (r0, _kernel_offset_to_ready_q_cache)

	addi    r1, ___thread_t_arch_OFFSET
	ldw	r2, (r1, ___thread_arch_t_swap_return_value_OFFSET)

	/* check whether swap_return_value is updated by other thread */
	lrw	r3, -7654
	cmpne	r2, r3
	bt	1f

	/* no new value, update swap_return_value to original error code */
	lrw	r2, _k_neg_eagain
	ldw	r2, (r2)
	stw	r2, (r1, ___thread_arch_t_swap_return_value_OFFSET)
1:
#endif

	lrw     r0, _kernel
	ldw     r1, (r0, _kernel_offset_to_current)

    /* 恢复新就绪的任务的caller和callee寄存器    
    */
	mov     r15, r1
	addi    r15, _thread_offset_to_callee_saved
	ldm     r4-r11, (r15)
	addi    r15, 32
	ldw     sp, (r15)
	ldw     r3, (r15, 4)
	mtcr    r3, epc
	ldw     r3, (r15, 8)
	mtcr    r3, epsr

	mov     r15, r1
	addi    r15, ___thread_t_caller_saved_OFFSET
	ldm     r0-r3, (r15)
	addi    r15, 16
	ldm     r12-r13, (r15)
	ldw     r15, (r15, 8)
	rte	
