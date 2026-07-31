/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file debug configuration macros for Actions SoC
 */

#ifndef	_ACTIONS_SOC_DEBUG_H_
#define	_ACTIONS_SOC_DEBUG_H_

#ifndef _ASMLANGUAGE

enum {
	SOC_JTAG_CPU0 = 0,
	SOC_JTAG_CPU1,
	SOC_JTAG_DSP,
};

void soc_debug_enable_jtag(int cpu_id, int group);
void soc_debug_disable_jtag(int cpu_id);

#endif /* _ASMLANGUAGE */

#endif /* _ACTIONS_SOC_DEBUG_H_	*/
