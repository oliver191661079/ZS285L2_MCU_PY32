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


#ifndef _ASMLANGUAGE

static inline u32_t soc_boot_get_part_tbl_addr(void)
{
	return (SOC_BOOT_SYSTEM_PARAM_VADDR + SOC_BOOT_PARTITION_TABLE_OFFSET);
}

static inline u32_t soc_boot_get_fw_ver_addr(void)
{
	return (SOC_BOOT_SYSTEM_PARAM_VADDR + SOC_BOOT_FIRMWARE_VERSION_OFFSET);
}

#endif /* _ASMLANGUAGE */

#endif /* _ACTIONS_SOC_BOOT_H_	*/
