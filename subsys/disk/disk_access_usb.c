/*
 * Copyright (c) 2016 Actions Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>
#include <stdint.h>
#include <misc/__assert.h>
#include <errno.h>
#include <misc/util.h>
#include <disk_access.h>
#include <errno.h>
#include <init.h>
#include <device.h>
#include <usb/storage.h>

#define SYS_LOG_DOMAIN "udisk_access"
#include <logging/sys_log.h>

static struct device *usb_disk;

static u32_t usb_disk_sector_cnt;

static int USB_disk_status(struct disk_info *disk)
{
	ARG_UNUSED(disk);

	if (!usb_disk) {
		return DISK_STATUS_NOMEDIA;
	}

	return DISK_STATUS_OK;
}

static int USB_disk_initialize(struct disk_info *disk)
{
	ARG_UNUSED(disk);

	if (usb_disk) {
		return 0;
	}

	/* get usb disk(usb storage) device */
	usb_disk = device_get_binding("usb-storage");
	if (!usb_disk) {
		return -ENODEV;
	}

	return 0;
}

static int USB_disk_read(struct disk_info *disk, u8_t *buff,
				u32_t start_sector, u32_t sector_count)
{
	ARG_UNUSED(disk);

	if (usb_disk_sector_cnt == 0) {
		if(!usb_disk)
			goto read;

		u32_t sector_count = 0;
		int ret = storage_ioctl(usb_disk, DISK_IOCTL_GET_SECTOR_COUNT, &sector_count);
		if (!ret && sector_count > 1) {
			usb_disk_sector_cnt = sector_count;
			SYS_LOG_INF("usb_disk_sector_cnt %d\n", usb_disk_sector_cnt);
		}

		if(!usb_disk_sector_cnt)
			goto read;

	}

	if (start_sector > usb_disk_sector_cnt - 1) {
		SYS_LOG_ERR("start_sector overflow %d %d\n", start_sector, usb_disk_sector_cnt);
		return -EIO;
	}

	if (start_sector + sector_count > usb_disk_sector_cnt) {
		sector_count = usb_disk_sector_cnt - start_sector;
	}

	u16_t sector_size = 0;
	storage_ioctl(usb_disk, DISK_IOCTL_GET_SECTOR_SIZE, (void*)&sector_size);

	if(sector_size > disk->sector_size)
	{
		SYS_LOG_ERR("buff may will be written overflow!!! exit\n");
		SYS_LOG_ERR("sector_size %d, subsys sector_size %d !!!\n", sector_size, disk->sector_size);
		return -EIO;
	}

read:
	if (storage_read(usb_disk, start_sector, buff, sector_count) != 0) {
		return -EIO;
	}

	return 0;
}

static int USB_disk_write(struct disk_info *disk, const u8_t *buff,
				u32_t start_sector, u32_t sector_count)
{
	ARG_UNUSED(disk);

	if (usb_disk_sector_cnt == 0) {
		goto write;
	}

	if (start_sector > usb_disk_sector_cnt - 1) {
		return -EIO;
	}

	if (start_sector + sector_count > usb_disk_sector_cnt) {
		sector_count = usb_disk_sector_cnt - start_sector;
	}

write:
	if (storage_write(usb_disk, start_sector, buff, sector_count) != 0) {
		return -EIO;
	}

	return 0;
}

/* info that get from usb-storage driver */
static int USB_disk_ioctl(struct disk_info *disk, u8_t cmd, void *buff)
{
	int ret = 0;

	ARG_UNUSED(disk);

	if (!usb_disk) {
		return -ENODEV;
	}

	switch (cmd) {
	case DISK_IOCTL_CTRL_SYNC:
		break;

	case DISK_IOCTL_GET_SECTOR_COUNT:
		ret = storage_ioctl(usb_disk, USB_DISK_IOCTL_GET_SECTOR_COUNT, buff);
		if (!ret && *(u32_t *)buff > 1) {
			usb_disk_sector_cnt = *(u32_t *)buff;
		}
		break;

	case DISK_IOCTL_GET_SECTOR_SIZE:
		ret = storage_ioctl(usb_disk, USB_DISK_IOCTL_GET_SECTOR_SIZE, buff);
		break;

	case DISK_IOCTL_GET_ERASE_BLOCK_SZ:
		*(u32_t *)buff = 0;
		break;

	case DISK_IOCTL_GET_DISK_SIZE:
		ret = storage_ioctl(usb_disk, USB_DISK_IOCTL_GET_DISK_SIZE, buff);
		break;

	case DISK_IOCTL_HW_DETECT:
		ret = storage_ioctl(usb_disk, USB_DISK_IOCTL_GET_STATUS, buff);
		break;

	default:
		return -EINVAL;
	}

	return ret;
}

static const struct disk_operation disk_usb_operation = {
	.init		= USB_disk_initialize,
	.get_status	= USB_disk_status,
	.read		= USB_disk_read,
	.write		= USB_disk_write,
	.ioctl		= USB_disk_ioctl,
};

static const struct disk_info disk_usb_mass = {
	.name		= "USB",
	.id		= 0,
	.flag		= 0,
	.sector_size	= 512,
	.sector_offset	= 0,
	.sector_cnt	= 0,
	.op		= &disk_usb_operation,
};

static int usb_disk_init(struct device *dev)
{
	ARG_UNUSED(dev);

	disk_register((struct disk_info *)&disk_usb_mass);

	return 0;
}

SYS_INIT(usb_disk_init, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
