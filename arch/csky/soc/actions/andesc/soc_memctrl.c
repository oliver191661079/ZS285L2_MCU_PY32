/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file memory controller interface
 */

#include <kernel.h>
#include <soc.h>
#include "soc_memctrl.h"
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_SOC_MEMCTRL

void soc_memctrl_set_mapping(int idx, u32_t map_addr, u32_t nor_bus_addr)
{
	if (idx >= CACHE_MAPPING_ITEM_NUM)
		return;

	sys_write32(nor_bus_addr, SPI_CACHE_ADDR0_ENTRY + idx * 8);
	/* default: mapping size: 4MB, to support sdfs file which size is bigger than 2MB*/
	sys_write32((map_addr & ~0xf) | 0x3, SPI_CACHE_MAPPING_ADDR0 + idx * 8);
}

int soc_memctrl_item_is_free(int idx)
{
	u32_t map_reg_val;

	map_reg_val = sys_read32(SPI_CACHE_MAPPING_ADDR0 + idx * 8);

	return (map_reg_val ? 0 : 1);
}

void soc_memctrl_mapping_clear(int idx)
{
	if (idx >= CACHE_MAPPING_ITEM_NUM)
		return;

	sys_write32(0, SPI_CACHE_ADDR0_ENTRY + idx * 8);
	sys_write32(0, SPI_CACHE_MAPPING_ADDR0 + idx * 8);
}

int soc_memctrl_mapping(u32_t map_addr, u32_t nor_phy_addr, int enable_crc)
{
	u32_t nor_bus_addr;
	int i;

	if (enable_crc) {
		if (nor_phy_addr % CACHE_CRC_LINE_SIZE)	{
			printk("invalid nor phy addr 0x%x, enable crc %d\n",
				nor_phy_addr, enable_crc);
			return -EINVAL;
		}

		nor_bus_addr = (1u << 31) | (nor_phy_addr / CACHE_CRC_LINE_SIZE * CACHE_LINE_SIZE);
	} else {
		if (nor_phy_addr % CACHE_LINE_SIZE)	{
			printk("invalid nor phy addr 0x%x, enable crc %d\n",
				nor_phy_addr, enable_crc);
			return -EINVAL;
		}

		nor_bus_addr = nor_phy_addr;
	}

	for (i = CACHE_MAPPING_ITEM_NUM - 1; i >= 0; i--) {
		if (soc_memctrl_item_is_free(i)) {
			soc_memctrl_set_mapping(i, map_addr, nor_bus_addr);
			return 0;
		}
	}

	printk("cannot found slot to map nor phy addr 0x%x\n", nor_phy_addr);
	return -ENOENT;
}

void *soc_memctrl_create_temp_mapping(u32_t nor_phy_addr, int enable_crc)
{
	u32_t tmp_cpu_addr = 0x0800000;
	int err;

	printk("create temp mapping 0x%x, nor_phy_addr 0x%x, enable_crc=%d\n",
		tmp_cpu_addr, nor_phy_addr, enable_crc);

	err = soc_memctrl_mapping(tmp_cpu_addr, nor_phy_addr, enable_crc);
	if (err < 0)
		return NULL;

	return (void *)tmp_cpu_addr;
}

void soc_memctrl_clear_temp_mapping(void *cpu_addr)
{
	int i;

	/* */
	for (i = CACHE_MAPPING_ITEM_NUM - 1; i >= 0; i--) {
		if ((sys_read32(SPI_CACHE_MAPPING_ADDR0 + i * 8) & ~0xf) == (u32_t)cpu_addr) {
			printk("clear temp mapping %p, idx %d\n", cpu_addr, i);
			soc_memctrl_mapping_clear(i);
		}
	}
}

u32_t soc_memctrl_cpu_to_nor_phy_addr(u32_t cpu_addr)
{
	u32_t map_reg_val, map_start_addr, map_end_addr;
	u32_t nor_reg_val, nor_addr;
	int i;

	for (i = 0; i < CACHE_MAPPING_ITEM_NUM; i++) {
		map_reg_val = sys_read32(SPI_CACHE_MAPPING_ADDR0 + i * 8);
		if (map_reg_val == 0)
			continue;

		map_start_addr = map_reg_val & ~0xfffff;
		map_end_addr = map_start_addr + (((map_reg_val & 0xf) + 1) << 20);

		if (cpu_addr >= map_start_addr && cpu_addr < map_end_addr) {
			nor_reg_val = sys_read32(SPI_CACHE_ADDR0_ENTRY + i * 8);

			nor_addr = (nor_reg_val & 0xffffff) + (cpu_addr - map_start_addr);
			if (nor_reg_val >> 31) {
				/* crc is enabled for this mapping */
				nor_addr = (nor_addr / 32 * 34) + (cpu_addr % 32);
			}

			return nor_addr;
		}
	}

	return -1;
}
