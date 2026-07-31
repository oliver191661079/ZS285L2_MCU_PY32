/*
 * Copyright (c) 2016 Intel Corporation.
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
#include <partition.h>

#define SYS_LOG_DOMAIN "nor_disk"
#define SYS_LOG_LEVEL SYS_LOG_LEVEL_INFO
#include <logging/sys_log.h>

#define SECTOR_SIZE			512

static struct device *nor_disk;
static u32_t disk_nor_start = 0;
static u32_t disk_nor_volume_size = 0;

static off_t lba_to_address(u32_t sector_num)
{
	off_t flash_addr;

	flash_addr = sector_num * SECTOR_SIZE;

	return flash_addr;
}

static int nor_disk_access_status(struct disk_info *disk)
{
	if (!nor_disk) {
		return DISK_STATUS_NOMEDIA;
	}

	return DISK_STATUS_OK;
}

static int nor_disk_access_init(struct disk_info *disk)
{
	SYS_LOG_DBG("init nor disk");

	if (nor_disk)
		return 0;

	nor_disk = device_get_binding(CONFIG_DISK_NOR_DEV_NAME);
	if (!nor_disk)
		return -ENODEV;

	return 0;
}

static int nor_disk_access_read(struct disk_info *disk, u8_t *buff,
				u32_t start_sector, u32_t sector_count)
{
	off_t fl_addr;
	u32_t len;
	u32_t limit;

	SYS_LOG_DBG("start_sector %d, sector_count %u",
		start_sector, sector_count);

	if (!nor_disk) {
		return -ENODEV;
	}

	limit = (disk_nor_start + disk_nor_volume_size) >> 9;

	if (start_sector > limit - 1) {
		return -EIO;
	}

	if (start_sector + sector_count > limit) {
		sector_count = limit - start_sector;
	}

	fl_addr = lba_to_address(start_sector);
	len = (sector_count * SECTOR_SIZE);

	if (flash_read(nor_disk, fl_addr, buff, len) != 0) {
		return -EIO;
	}

	//print_buffer(buff, 1, len, 16, -1);

	return 0;
}

static int nor_disk_access_write(struct disk_info *disk, const u8_t *buff,
				 u32_t start_sector, u32_t sector_count)
{
	return 0;
}

static int nor_disk_access_ioctl(struct disk_info *disk, u8_t cmd, void *buff)
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
	case DISK_IOCTL_HW_DETECT:
		*(u8_t *)buff = STA_DISK_OK;
		break;
	default:
		break;
	}

	return -EINVAL;
}

static const struct disk_operation disk_nor_operation = {
	.init = nor_disk_access_init,
	.get_status = nor_disk_access_status,
	.read = nor_disk_access_read,
	.write = nor_disk_access_write,
	.ioctl = nor_disk_access_ioctl,
};

static struct disk_info disk_nor0 = {
	.name		= "NOR",
	.id		= 0,
	.flag		= 0,
	.sector_size	= 512,
	.op		= &disk_nor_operation,
};

static int nor_disk_init(struct device *dev)
{
	SYS_LOG_DBG("register \'%s\' disk", disk_nor0.name);
	disk_register((struct disk_info *)&disk_nor0);

	const struct partition_entry * part = partition_get_part(PARTITION_FILE_ID_FAT_NOR);

	if(!part){
		printk("XML no FAT_NOR info\n");
		#ifdef CONFIG_DISK_NOR_START
		disk_nor_start = CONFIG_DISK_NOR_START;
		#endif

		#ifdef CONFIG_DISK_NOR_VOLUME_SIZE
		disk_nor_volume_size = CONFIG_DISK_NOR_VOLUME_SIZE;
		#endif
	}else{
		disk_nor_start = part->offset;
		disk_nor_volume_size = part->size;
	}

	disk_nor0.sector_offset	= disk_nor_start >> 9;
	disk_nor0.sector_cnt	= disk_nor_volume_size >> 9;

	printk("disk_nor 0x%x 0x%x\n", disk_nor_start, disk_nor_volume_size);

	return 0;
}

SYS_INIT(nor_disk_init, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
