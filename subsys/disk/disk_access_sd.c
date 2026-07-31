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
#include <init.h>
#include <errno.h>
#include <device.h>
#include <flash.h>

static struct device *sd_disk;
static u8_t sd_device_existed = 0;

static u32_t sd_disk_sector_cnt;

static int sd_disk_status(struct disk_info *disk)
{
	if (!sd_device_existed) {
		return DISK_STATUS_NOMEDIA;
	}

	return DISK_STATUS_OK;
}

static int sd_disk_initialize(struct disk_info *disk)
{
	u32_t sector_cnt;
	int ret;

	if (!sd_disk) {
		return -ENODEV;
	}

	if (0 != flash_init(sd_disk)) {
		sd_device_existed = 0;
		return -ENODEV;
	}

	ret = flash_ioctl(sd_disk, DISK_IOCTL_GET_SECTOR_COUNT, (void *)&sector_cnt);
	if (!ret) {
		sd_disk_sector_cnt = sector_cnt;
		sd_device_existed = 1;
	} else {
		printk("failed to get sector count error=%d\n", ret);
		sd_device_existed = 0;
		return -EFAULT;
	}

	return 0;
}

static int sd_disk_read(struct disk_info *disk, u8_t *buff, u32_t start_sector,
		     u32_t sector_count)
{
	if (!sd_disk) {
		return -ENODEV;
	}

	if (sd_disk_sector_cnt == 0) {
		goto read;
	}

	if (start_sector > sd_disk_sector_cnt - 1) {
		return -EIO;
	}

	if (start_sector + sector_count > sd_disk_sector_cnt) {
		sector_count = sd_disk_sector_cnt - start_sector;
	}

read:
	/* is it OK? */
	if (flash_read(sd_disk, start_sector, buff, sector_count) != 0) {
		return -EIO;
	}
	return 0;
}

static int sd_disk_write(struct disk_info *disk, const u8_t *buff, u32_t start_sector,
		      u32_t sector_count)
{
	if (!sd_disk) {
		return -ENODEV;
	}

	if (sd_disk_sector_cnt == 0) {
		goto write;
	}

	if (start_sector > sd_disk_sector_cnt - 1) {
		return -EIO;
	}

	if (start_sector + sector_count > sd_disk_sector_cnt) {
		sector_count = sd_disk_sector_cnt - start_sector;
	}

write:
	if (flash_write(sd_disk, start_sector, buff, sector_count) != 0) {
		return -EIO;
	}

	return 0;
}

static int sd_disk_ioctl(struct disk_info *disk, u8_t cmd, void *buff)
{
	int ret = 0;

	if (!sd_disk) {
		return -ENODEV;
	}

	if (!sd_device_existed && (cmd != DISK_IOCTL_HW_DETECT)) {
		return -ENODEV;
	}

	switch (cmd) {
	case DISK_IOCTL_CTRL_SYNC:
		break;
	case DISK_IOCTL_GET_SECTOR_COUNT:
	case DISK_IOCTL_GET_SECTOR_SIZE:
	case DISK_IOCTL_GET_ERASE_BLOCK_SZ:
	case DISK_IOCTL_GET_DISK_SIZE:
		ret = flash_ioctl(sd_disk, cmd, buff);
		if (!ret && cmd == DISK_IOCTL_GET_SECTOR_COUNT) {
			sd_disk_sector_cnt = *(u32_t *)buff;
		}
		break;

	case DISK_IOCTL_HW_DETECT:
		ret = flash_ioctl(sd_disk, cmd, buff);
		if (ret == 0) {
			if (*(u8_t *)buff != STA_DISK_OK) {
				sd_device_existed = 0;
			} else {
				sd_device_existed = 1;
			}
		}
		break;
	default:
		return -EINVAL;
	}

	return ret;
}

static const struct disk_operation disk_sd_operation = {
	.init		= sd_disk_initialize,
	.get_status	= sd_disk_status,
	.read		= sd_disk_read,
	.write		= sd_disk_write,
	.ioctl		= sd_disk_ioctl,
};

static const struct disk_info disk_sd_mass = {
	.name		= "SD",
	.id		= 0,
	.flag		= 0,
	.sector_size	= 512,
	.sector_offset	= 0,
	.sector_cnt	= 0,
	.op		= &disk_sd_operation,
};

static int sd_disk_init(struct device *dev)
{
	ARG_UNUSED(dev);
	disk_register((struct disk_info *)&disk_sd_mass);
	sd_disk = device_get_binding(CONFIG_MMC_SDCARD_DEV_NAME);
	if (!sd_disk) {
		printk("can not find sd device %s\n", CONFIG_MMC_SDCARD_DEV_NAME);
		return -ENODEV;
	}

	return 0;
}

SYS_INIT(sd_disk_init, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);

