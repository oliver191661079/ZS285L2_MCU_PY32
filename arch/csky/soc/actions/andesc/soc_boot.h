/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file boot related interface
 */

#ifndef	_ACTIONS_SOC_BOOT_H_
#define	_ACTIONS_SOC_BOOT_H_

#define	SOC_BOOT_PARTITION_TABLE_OFFSET		(0)
#define	SOC_BOOT_FIRMWARE_VERSION_OFFSET	(0x180)

#define SOC_BOOT_SYSTEM_PARAM_VADDR			(0x200000)
#define SOC_BOOT_SYSTEM_PARAM0_OFFSET		(0x2000)
#define SOC_BOOT_SYSTEM_PARAM1_OFFSET		(0x3000)
#define SOC_BOOT_SYSTEM_PARAM_SIZE			(0x1000)

#define SOC_BOOT_MBREC0_OFFSET				(0x0)
#define SOC_BOOT_MBREC1_OFFSET				(0x1000)
#define SOC_BOOT_MBREC_SIZE					(0x1000)

typedef struct {
	u32_t param_phy_addr;
	u32_t reboot_reason;
	u32_t watchdog_reboot : 8;
	u32_t prod_adfu_reboot : 1;
	u32_t prod_card_reboot : 1;
	u32_t mirror_id : 1;
} boot_info_t;

#ifdef CONFIG_BOOTINFO_MEMORY
extern __in_section_unique(BOOTINFO_DATA) boot_info_t g_boot_info;
const boot_info_t *soc_boot_get_info(void);
u32_t soc_boot_get_reboot_reason(void);
#endif

#ifndef _ASMLANGUAGE

static inline u32_t soc_boot_get_part_tbl_addr(void)
{
	return (SOC_BOOT_SYSTEM_PARAM_VADDR + SOC_BOOT_PARTITION_TABLE_OFFSET);
}

static inline u32_t soc_boot_get_fw_ver_addr(void)
{
	return (SOC_BOOT_SYSTEM_PARAM_VADDR + SOC_BOOT_FIRMWARE_VERSION_OFFSET);
}

static inline u32_t soc_boot_get_watchdog_is_reboot(void)
{
	if (sys_read32(WD_CTL) & (1 << 6))
		return 1;
	else
		return 0;
}

#endif /* _ASMLANGUAGE */

#endif /* _ACTIONS_SOC_BOOT_H_	*/
