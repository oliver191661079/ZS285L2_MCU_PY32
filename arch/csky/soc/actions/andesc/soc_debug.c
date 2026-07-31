/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file debug configuration interface for Actions SoC
 */

#include <kernel.h>
#include <soc.h>
#include <soc_debug.h>

void soc_debug_enable_jtag(int cpu_id, int group)
{
	switch (cpu_id) {
	case SOC_JTAG_CPU0:
		sys_write32((sys_read32(JTAG_CTL) & ~0x3) |
			((group & 0x3) << 0) | (1 << 4), JTAG_CTL);

		/* don't gating cpu clock when cpu enter low power state if jtag is enabled */
		sys_write32(sys_read32(CK802_WAKEUP_EN1) & ~(1u << 31), CK802_WAKEUP_EN1);
		break;
	case SOC_JTAG_DSP:
		sys_write32((sys_read32(JTAG_CTL) & ~0x300) |
			((group & 0x3) << 8) | (1 << 12), JTAG_CTL);
		break;

	default:
		break;
	}
}

void soc_debug_disable_jtag(int cpu_id)
{
	switch (cpu_id) {
	case SOC_JTAG_CPU0:
		sys_write32(sys_read32(JTAG_CTL) & ~(1 << 4), JTAG_CTL);
		break;
	case SOC_JTAG_DSP:
		sys_write32(sys_read32(JTAG_CTL) & ~(1 << 12), JTAG_CTL);
		break;
	default:
		break;
	}
}
