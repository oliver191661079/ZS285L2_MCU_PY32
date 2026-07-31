/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file SoC init for Actions SoC.
 */

#include <kernel.h>
#include <device.h>
#include <init.h>
#include <soc.h>
#include <arch/cpu.h>
#include <string.h>
#include "soc_version.h"


static void soc_rom_patch(void)
{

}

static int soc_init(struct device *arg)
{
	ARG_UNUSED(arg);

	pmu_init();

	soc_cmu_init();

	/* enable cpu clock gating and all interrupt wakeup sources by default */
	sys_write32(0xffffffff, CK802_WAKEUP_EN0);
#ifdef CONFIG_IDLE_CLOSE_CPU_CLK
	sys_write32(0xffffffff, CK802_WAKEUP_EN1);
#else
	sys_write32(0x7fffffff, CK802_WAKEUP_EN1);
#endif

	/* config timer0/1 clock source to HOSC */
	sys_write32((sys_read32(CMU_TIMERCLK) & ~0xf) | 0x0, CMU_TIMERCLK);

	soc_freq_set_cpu_clk(CONFIG_SOC_DSP_CLK_FREQ / 1000, CONFIG_SOC_CPU_CLK_FREQ / 1000, CONFIG_SOC_CPU_CLK_FREQ / 2000);

#ifdef CONFIG_MMC_0
	/* switch SD0BUF to mmc controller */
	sys_write32(sys_read32(CMU_MEMCLKEN) | (1 << 21), CMU_MEMCLKEN);
	sys_write32(sys_read32(CMU_MEMCLKSEL) | (1 << 21), CMU_MEMCLKSEL);
#endif

#ifdef CONFIG_MMC_1
	/* switch SD1BUF to mmc controller */
	sys_write32(sys_read32(CMU_MEMCLKEN) | (1 << 22), CMU_MEMCLKEN);
	sys_write32(sys_read32(CMU_MEMCLKSEL) | (1 << 22), CMU_MEMCLKSEL);
#endif

	/* switch PCM4/5/6 buffer to cpu */
	sys_write32(sys_read32(CMU_MEMCLKEN) | (0x7 << 16), CMU_MEMCLKEN);
	sys_write32(sys_read32(CMU_MEMCLKSEL) & ~(0x7 << 16), CMU_MEMCLKSEL);


#ifdef CONFIG_XIP
	/*  enable SPI cache low power mode */
	sys_write32(sys_read32(SPICACHE_CTL) | (1 << 5), SPICACHE_CTL);

	sys_write32(sys_read32(MEMORYCTL) | (MEMORYCTL_CRCERROR_BIT | MEMORYCTL_INVALID_ADDR_BIT), \
	        MEMORYCTL);
#endif


#ifdef CONFIG_USB_ACTIONS
#if !defined(CONFIG_USB) && !defined(CONFIG_STUB_DEV_USB)
	extern void usb_phy_enter_low_power(void);
	usb_phy_enter_low_power();
#endif
#endif
	/*  disable all ADC channels by default to save power consume */
	sys_write32(sys_read32(PMUADC_CTL) & ~0xffff, PMUADC_CTL);

	//Enable DC5VADC to support 5V plug in/out detection.
	sys_write32(sys_read32(PMUADC_CTL) | (1 << PMUADC_CTL_DC5VADC_EN), PMUADC_CTL);

	soc_rom_patch();

	return 0;
}

uint32_t libsoc_version_get(void)
{
    return LIBSOC_VERSION_NUMBER;
}

SYS_INIT(soc_init, PRE_KERNEL_1, 20);
