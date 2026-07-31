/*
 * Copyright (c) 2021 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file SOC boot implementation
 */

#include <kernel.h>
#include <init.h>
#include <device.h>
#include <soc.h>

#ifdef CONFIG_BOOTINFO_MEMORY

__in_section_unique(BOOTINFO_DATA) boot_info_t g_boot_info;
__in_section_unique(BOOTINFO_DATA) u32_t boot_info_addr;

const boot_info_t *soc_boot_get_info(void)
{
	return (const boot_info_t *)&g_boot_info;
}

u32_t soc_boot_get_reboot_reason(void)
{
	boot_info_t *p_boot_info = &g_boot_info;
	return p_boot_info->reboot_reason;
}

static int soc_boot_init(struct device *dev)
{
	const boot_info_t *boot_info = soc_boot_get_info();

	ARG_UNUSED(dev);

	printk("**   BOOT Infomation   **\n");
	printk("BOOT PARAM address: 0x%x\n", boot_info->param_phy_addr);
	printk("       BOOT reason: 0x%x\n", boot_info->reboot_reason);
	printk("    BOOT mirror ID: %d\n", boot_info->mirror_id);

	return 0;
}

SYS_INIT(soc_boot_init, PRE_KERNEL_1, 10);

#endif
