/********************************************************************************
 *                            USDK(ZS283A_clean)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2018-10-23-下午6:31:19             1.0             build this file
 ********************************************************************************/
/*!
 * \file     swap_c.c
 * \brief
 * \author
 * \version  1.0
 * \date  2018-10-23-下午6:31:19
 *******************************************************************************/
#include <csi_core.h>
#include <kernel.h>
#include <toolchain.h>
#include <kernel_structs.h>
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_SWAP_C

#ifdef CONFIG_EXECUTION_BENCHMARKING
extern void read_timer_start_of_swap(void);
#endif
extern const int _k_neg_eagain;

/**
 *
 * @brief Initiate a cooperative context switch
 *
 * The __swap() routine is invoked by various kernel services to effect
 * a cooperative context context switch.  Prior to invoking __swap(), the caller
 * disables interrupts via irq_lock() and the return 'key' is passed as a
 * parameter to __swap().  The 'key' actually represents the BASEPRI register
 * prior to disabling interrupts via the BASEPRI mechanism.
 *
 * __swap() itself does not do much.
 *
 * It simply stores the intlock key (the BASEPRI value) parameter into
 * current->basepri, and then triggers a PendSV exception, which does
 * the heavy lifting of context switching.

 * This is the only place we have to save BASEPRI since the other paths to
 * __pendsv all come from handling an interrupt, which means we know the
 * interrupts were not locked: in that case the BASEPRI value is 0.
 *
 * Given that __swap() is called to effect a cooperative context switch,
 * only the caller-saved integer registers need to be saved in the thread of the
 * outgoing thread. This is all performed by the hardware, which stores it in
 * its exception stack frame, created when handling the svc exception.
 *
 * On ARMv6-M, the intlock key is represented by the PRIMASK register,
 * as BASEPRI is not available.
 *
 * @return -EAGAIN, or a return value set by a call to
 * _set_thread_return_value()
 *
 */
#ifdef CONFIG_SWAP_TSPEND
#define VIC_TSPEND 0xE000EC08

#define TSPEND_CHECK_INIT_VALUE		(-7654)

__ramfunc unsigned int __swap(unsigned int key)
{
#ifdef CONFIG_EXECUTION_BENCHMARKING
    read_timer_start_of_swap();
#endif

    /* store off key and return value */
    _current->arch.basepri = key;

#ifdef CONFIG_SWAP_TSPEND_CHECK
    _current->arch.swap_return_value = TSPEND_CHECK_INIT_VALUE;
#else
    _current->arch.swap_return_value = _k_neg_eagain;
#endif

    /* set pending bit to make sure we will take a PendSV exception */
    sys_write32(0x01, VIC_TSPEND);

    /* clear mask or enable all irqs to take a pendsv */
    csi_enable_all_irq();

#ifdef CONFIG_SWAP_TSPEND_CHECK
    /* FIXME: wait until tspend irq is processed */
    while (_current->arch.swap_return_value == TSPEND_CHECK_INIT_VALUE)
          ;
#endif

    if(_current->callee_saved.psr &  (1 << 6)){
        return _current->arch.swap_return_value;
    }else{
        //中断状态下
		printk("<-------------------psr=%x---------------->\n", _current->callee_saved.psr);
		if(_current->callee_saved.psr &  (1 << 6)){
			return _current->arch.swap_return_value;
		}
        panic("_swap in isr");
    }
	return 0;
}
#endif



