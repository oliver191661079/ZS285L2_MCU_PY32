/*
 * Copyright (c) 2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 *
 */

#include <kernel.h>
#include <toolchain.h>
#include <linker/sections.h>
#include <kernel_structs.h>
#include <misc/printk.h>
#include <soc.h>

/**
 *
 * @brief Fatal error handler
 *
 * This routine implements the corrective action to be taken when the system
 * detects a fatal error.
 *
 * This sample implementation attempts to abort the current thread and allow
 * the system to continue executing, which may permit the system to continue
 * functioning with degraded capabilities.
 *
 * System designers may wish to enhance or substitute this sample
 * implementation to take other actions, such as logging error (or debug)
 * information to a persistent repository and/or rebooting the system.
 *
 * @param reason fatal error reason
 * @param pEsf pointer to exception stack frame
 *
 * @return N/A
 */
void _SysFatalErrorHandler(unsigned int reason,
					 const NANO_ESF *pEsf)
{
#ifndef CONFIG_CPU_REBOOT_IF_EXCEPTION
    volatile int flag = 0;
#endif

#if !defined(CONFIG_SIMPLE_FATAL_ERROR_HANDLER)
	if (k_is_in_isr() || _is_thread_essential()) {
		PR_EXC("Fatal fault in %s! Spinning...\n\n\n",
		       k_is_in_isr() ? "ISR" : "essential thread");
	}else{
    	PR_EXC("Fatal fault in thread %p! Aborting.\n\n\n", _current);
    	//k_thread_abort(_current);
    }
#endif

#ifndef CONFIG_CPU_REBOOT_IF_EXCEPTION

	soc_debug_enable_jtag(SOC_JTAG_CPU0, CONFIG_CPU0_EJTAG_GROUP_EXCEPTION);

	while(!flag) {
		;
	}
#else
	//0x04 reboot reason: REBOOT_REASON_SYSTEM_EXCEPTION
	sys_pm_reboot(0x04);
#endif
	//CODE_UNREACHABLE;
}
