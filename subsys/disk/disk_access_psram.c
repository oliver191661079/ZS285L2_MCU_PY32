/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <stdint.h>
#include <misc/__assert.h>
#include <disk_access.h>
#include <errno.h>
#include <init.h>
#include <device.h>
#include <flash.h>

#define SYS_LOG_DOMAIN "psram_disk"
#define SYS_LOG_LEVEL SYS_LOG_LEVEL_INFO
#include <logging/sys_log.h>

#define SECTOR_SIZE			512

static struct device *psram_disk;

static off_t lba_to_address(u32_t sector_num)
{
	off_t flash_addr;

	flash_addr = sector_num * SECTOR_SIZE;

	return flash_addr;
}

static int psram_disk_access_status(struct disk_info *disk)
{
	if (!psram_disk) {
		return DISK_STATUS_NOMEDIA;
	}

	return DISK_STATUS_OK;
}

static int psram_disk_access_init(struct disk_info *disk)
{
	SYS_LOG_DBG("init spi psram disk");

	if (psram_disk)
		return 0;

	psram_disk = device_get_binding(CONFIG_SPI_PSRAM_DEV_NAME);
	if (!psram_disk)
		return -ENODEV;

	return 0;
}

static int psram_disk_access_read(struct disk_info *disk, u8_t *buff,
				u32_t start_sector, u32_t sector_count)
{
	off_t fl_addr;
	u32_t len;
	u32_t limit;

	SYS_LOG_DBG("start_sector %d, sector_count %u",
		start_sector, sector_count);

	if (!psram_disk) {
		return -ENODEV;
	}

	limit = (CONFIG_DISK_PSRAM_START + CONFIG_DISK_PSRAM_VOLUME_SIZE) >> 9;

	if (start_sector > limit - 1) {
		return -EIO;
	}

	if (start_sector + sector_count > limit) {
		sector_count = limit - start_sector;
	}

	fl_addr = lba_to_address(start_sector);
	len = (sector_count * SECTOR_SIZE);

	if (flash_read(psram_disk, fl_addr, buff, len) != 0) {
		return -EIO;
	}

	//print_buffer(buff, 1, len, 16, -1);

	return 0;
}

static int psram_disk_access_write(struct disk_info *disk, const u8_t *buff,
				 u32_t start_sector, u32_t sector_count)
{
	off_t fl_addr;
	u32_t len;
	u32_t limit;

	SYS_LOG_DBG("start_sector %d, sector_count %u",
		start_sector, sector_count);

	if (!psram_disk) {
		return -ENODEV;
	}

	limit = (CONFIG_DISK_PSRAM_START + CONFIG_DISK_PSRAM_VOLUME_SIZE) >> 9;

	if (start_sector > limit - 1) {
		return -EIO;
	}

	if (start_sector + sector_count > limit) {
		sector_count = limit - start_sector;
	}

	fl_addr = lba_to_address(start_sector);
	len = (sector_count * SECTOR_SIZE);

	if (flash_write(psram_disk, fl_addr, buff, len) != 0) {
		return -EIO;
	}

	//print_buffer(buff, 1, len, 16, -1);
	return 0;
}

static int psram_disk_access_ioctl(struct disk_info *disk, u8_t cmd, void *buff)
{
	SYS_LOG_DBG("cmd %d", cmd);

	switch (cmd) {
	case DISK_IOCTL_CTRL_SYNC:
		return 0;
	case DISK_IOCTL_GET_SECTOR_COUNT:
		*(u32_t *)buff = disk->sector_cnt;
		return 0;
	case DISK_IOCTL_GET_SECTOR_SIZE:
		*(u32_t *) buff = disk->sector_size;
		return 0;
	case DISK_IOCTL_GET_ERASE_BLOCK_SZ: /* in sectors */
		*(u32_t *)buff = 0;
		return 0;
	case DISK_IOCTL_GET_DISK_SIZE:
		*(u32_t *)buff = disk->sector_cnt * disk->sector_size;
		return 0;
	default:
		break;
	}

	return -EINVAL;
}

static const struct disk_operation disk_psram_operation = {
	.init = psram_disk_access_init,
	.get_status = psram_disk_access_status,
	.read = psram_disk_access_read,
	.write = psram_disk_access_write,
	.ioctl = psram_disk_access_ioctl,
};

static const struct disk_info disk_psram0 = {
	.name		= "PSRAM",
	.id		= 0,
	.flag		= 0,
	.sector_size	= 512,
	.sector_offset	= CONFIG_DISK_PSRAM_START >> 9,
	.sector_cnt	= CONFIG_DISK_PSRAM_VOLUME_SIZE >> 9,
	.op		= &disk_psram_operation,
};

static int psram_disk_init(struct device *dev)
{
	SYS_LOG_DBG("register \'%s\' disk", disk_psram0.name);
	disk_register((struct disk_info *)&disk_psram0);

	return 0;
}

SYS_INIT(psram_disk_init, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
