/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief OTA storage interface
 */

#define SYS_LOG_LEVEL 3
#define SYS_LOG_DOMAIN "otaul"
#include <logging/sys_log.h>

#include <kernel.h>
#include <flash.h>
#include <string.h>
#include <mem_manager.h>
#include <ota_unload.h>
#include <os_common_api.h>
#include <logging/sys_log.h>
#include <watchdog_hal.h>
#include <nvram_config.h>

#define OTA_ERASE_ALIGN_SIZE 0x10000
#define OTA_WRITE_ALIGN_SIZE 0x1000

#ifdef CONFIG_SOC_MAPPING_PSRAM
#ifdef CONFIG_OTA_TEMP_PART_DEV_NAME
extern char __share2_media_ram_start[];
static char *ota_unloading_temp_sram_buffer = __share2_media_ram_start;
#endif
#endif

int ota_unloading_set_state(ota_unloading_state_e state)
{
	u8_t ota_unload_state = state;
	SYS_LOG_INF("ota_unload_state %d", ota_unload_state);
	nvram_config_set("OTA_UNLOAD", &ota_unload_state, sizeof(ota_unload_state));
	return 0;
}

int ota_unloading_get_state(void)
{
	int rlen;
	u8_t ota_unload_state = OTA_UNLOADING_INIT;
	rlen = nvram_config_get("OTA_UNLOAD", &ota_unload_state, sizeof(ota_unload_state));
	if(rlen != sizeof(ota_unload_state)){
		return OTA_UNLOADING_INIT;
	}

	return ota_unload_state;
}

int ota_unloading_clear_all(void)
{
	nvram_config_set("OTA_UNLOAD", NULL, 0);
	nvram_config_set("OTA_BP", NULL, 0);
	return 0;
}

extern void trace_set_panic(void);
#ifdef CONFIG_SOC_MAPPING_PSRAM
#ifdef CONFIG_OTA_TEMP_PART_DEV_NAME
static int ota_app_flush_spi_cache(void)
{
#if 0
	for(int i = 0; i < 256; i++) {
		sys_write32((i << 5) | (0x3 << 3) | (0 << 1) | 0x01 ,0xC0140004);
		while((sys_read32(0xC0140004) & 0x01) == 0x01);
	}

	sys_write32(0x2400000, SPI1_DCACHE_UNCACHE_ADDR_START);
	sys_write32(0x2800000 - 4, SPI1_DCACHE_UNCACHE_ADDR_END);
	// spi1 dcache disable
	sys_write32(sys_read32(SPI1_DCACHE_CTL) | 0x04, SPI1_DCACHE_CTL);
#endif
	trace_set_panic();
#if 0
	for(int i = 0 ; i < 8 ; i++) {
		printk("DMA%dCTL : 0x%x \n", i,sys_read32(0xc0040100 + i * 0x100));
		printk("DMA%dstart : 0x%x \n",i, sys_read32(0xc0040104 + i * 0x100));
		sys_write32(0 , 0xc0040104 + i * 0x100);
		printk("DMA%dsaddr0: 0x%x \n", i,sys_read32(0xc0040108 + i * 0x100));
		printk("DMA%dsaddr1 : 0x%x \n",i, sys_read32(0xc004010c + i * 0x100));
		sys_write32(0 , 0xc0040108 + i * 0x100);
		sys_write32(0 , 0xc004010c + i * 0x100);
		printk("DMA%ddaddr0 : 0x%x \n",i, sys_read32(0xc0040110 + i * 0x100));
		printk("DMA%ddaddr1 : 0x%x \n",i, sys_read32(0xc0040114 + i * 0x100));
		sys_write32(0 , 0xc0040110 + i * 0x100);
		sys_write32(0 , 0xc0040114 + i * 0x100);
		printk("DMA%dBC  : 0x%x \n", i,sys_read32(0xc0040118 + i * 0x100));
		printk("DMA%dRC : 0x%x \n", i,sys_read32(0xc004011c + i * 0x100));
	}
#endif
	return 0;
}
#endif
#endif

int ota_unloading(void)
{
#ifdef CONFIG_SOC_MAPPING_PSRAM
#ifdef CONFIG_OTA_TEMP_PART_DEV_NAME
	int ret = 0;
	int irq_flag = 0;
	int total_size = 0;
	int addr = 0;
	void *ram_buff = (void *)(ota_unloading_temp_sram_buffer);

	struct device *nor_dev = device_get_binding(CONFIG_OTA_TEMP_PART_DEV_NAME);
	if (!nor_dev) {
		SYS_LOG_ERR("cannot found storage device %s", CONFIG_OTA_TEMP_PART_DEV_NAME);
		return -ENOENT;
	}

	struct device *psram_dev = device_get_binding(CONFIG_SPI_PSRAM_CACHE_DEV_NAME);
	if (!psram_dev) {
		SYS_LOG_ERR("cannot found storage device %s", CONFIG_SPI_PSRAM_CACHE_DEV_NAME);
		return -ENOENT;
	}

	ret = flash_ioctl(psram_dev, 0x01, &total_size);
	if (ret) {
		SYS_LOG_ERR(" ret %d total_size %d \n",ret,  total_size);
		return -ENOENT;
	}

	/**align 64K*/
	total_size = ROUND_UP(total_size, OTA_ERASE_ALIGN_SIZE);

	SYS_LOG_INF("total_size %d \n", total_size);
	ota_app_flush_spi_cache();
	SYS_LOG_INF("start %d \n", total_size);
	irq_flag = irq_lock();

	for (addr = 0 ; addr < total_size;) {
		ret = flash_erase(nor_dev, (off_t)addr, (size_t)OTA_ERASE_ALIGN_SIZE);
		if (ret) {
			SYS_LOG_ERR("nor erase failed 0x%x \n", addr);
			return -EIO;
		}
		addr += OTA_ERASE_ALIGN_SIZE;
		watchdog_clear();
	}

	for (addr = 0 ; addr < total_size;) {
		ret = flash_read(psram_dev, addr, ram_buff, OTA_WRITE_ALIGN_SIZE);
		if (ret) {
			SYS_LOG_ERR("psram read failed 0x%x %d \n", addr, ret);
			return -EIO;
		}
		if (!addr) {
			printk("psram----------11\n");
			print_buffer(ram_buff, 1, OTA_WRITE_ALIGN_SIZE, 16, -1);
		}
		ret = flash_write(nor_dev, addr, ram_buff, OTA_WRITE_ALIGN_SIZE);
		if (ret) {
			SYS_LOG_ERR("nor write failed 0x%x %d \n", addr, ret);
			return -EIO;
		}

		memset(ram_buff, 0, OTA_WRITE_ALIGN_SIZE);
		ret = flash_read(nor_dev, addr, ram_buff, OTA_WRITE_ALIGN_SIZE);
		if (ret) {
			SYS_LOG_ERR("nor write failed 0x%x %d \n", addr, ret);
			return -EIO;
		}
		if (!addr) {
			printk("NOR----------11\n");
			print_buffer(ram_buff, 1, OTA_WRITE_ALIGN_SIZE, 16, -1);
		}
		addr += OTA_WRITE_ALIGN_SIZE;
		watchdog_clear();
	}
	memset(ram_buff, 0, OTA_WRITE_ALIGN_SIZE);
	ret = flash_read(nor_dev, 0, ram_buff, OTA_WRITE_ALIGN_SIZE);
	if (ret) {
		SYS_LOG_ERR("nor write failed 0x%x %d \n", addr, ret);
		return -EIO;
	}
	if (!addr) {
		printk("after NOR----------11\n");
		print_buffer(ram_buff, 1, OTA_WRITE_ALIGN_SIZE, 16, -1);
	}

	irq_unlock(irq_flag);
	SYS_LOG_INF("finished  0x%x \n",addr);
	ota_unloading_set_state(OTA_UNLOADING_FINISHED);
#endif
#endif

	return 0;
}
