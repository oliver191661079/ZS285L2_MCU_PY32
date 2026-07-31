/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file file system  manager interface
 */

#include <os_common_api.h>
#include <zephyr.h>
#include <logging/sys_log.h>
#include <init.h>
#include <kernel.h>
#include <kernel_structs.h>
#include <flash.h>
#include <disk_access.h>

#include <mem_manager.h>
#include <fs_manager.h>

static const char * const fat_fs_volume_names[] = {
	"NOR:", "NAND:", "PSRAM:", "USB:", "SD:", "FLASH:",""
};

struct fat_fs_volume {
	FATFS *fat_fs;
	u8_t allocated : 1;	/* 0: buffer not allocated, 1: buffer allocated */
	u8_t mounted : 1;	/* 0: volume not mounted, 1: volume mounted */
};

static struct fat_fs_volume fat_fs_volumes[_VOLUMES];

int fs_manager_init(void)
{
	int i;
	int res = 0;
	FATFS *fat_fs;
	u8_t *work;

	work = mem_malloc(_MAX_SS);
	if (work == NULL) {
		goto malloc_failed;
	}

	for (i = 0; i < DISK_MAX_NUM; i++) {
		/* fs_manager_init() should be called only once */
		if (fat_fs_volumes[i].allocated == 1) {
			continue;
		}

		fat_fs = mem_malloc(sizeof(FATFS));
		if (fat_fs == NULL) {
			goto malloc_failed;
		}

		if (strstr(fat_fs_volume_names[i], "PSRAM:") != NULL) {
			res = f_mkfs(fat_fs_volume_names[i], (FM_FAT | FM_SFD), 0, work, _MAX_SS);
			if (res != FR_OK) {
				mem_free(fat_fs);
				continue;
			}
		}

		res = fs_disk_init(fat_fs, fat_fs_volume_names[i]);
		if (res) {
			if ((i == DISK_SD_CARD) && (res == FR_NO_FILESYSTEM)) {
				res = f_mkfs(fat_fs_volume_names[i], (FM_FAT32 | FM_SFD), 0, work, _MAX_SS);
				if (res != FR_OK) {
					SYS_LOG_INF("fs (%s) mkfs failed (%d)", fat_fs_volume_names[i], res);
					mem_free(fat_fs);
					continue;
				}

				res = fs_disk_init(fat_fs, fat_fs_volume_names[i]);
			}

			if (res) {
				SYS_LOG_INF("fs (%s) init failed (%d)", fat_fs_volume_names[i], res);
				mem_free(fat_fs);
			} else {
				SYS_LOG_INF("fs (%s) init success", fat_fs_volume_names[i]);
				fat_fs_volumes[i].fat_fs = fat_fs;
				fat_fs_volumes[i].allocated = 1;
				fat_fs_volumes[i].mounted = 1;
			}
		} else {
			SYS_LOG_INF("fs (%s) init success", fat_fs_volume_names[i]);
			fat_fs_volumes[i].fat_fs = fat_fs;
			fat_fs_volumes[i].allocated = 1;
			fat_fs_volumes[i].mounted = 1;
		}
	}

	mem_free(work);

	return res;
malloc_failed:
	SYS_LOG_ERR("malloc failed");
	return -ENOMEM;
}

static int fs_manager_get_volume(const u8_t *volume_name)
{
	int index;

	for (index = 0; index < DISK_MAX_NUM; index++) {
		if (!strcmp(volume_name, fat_fs_volume_names[index])) {
			break;
		}
	}

	if (index >= DISK_MAX_NUM) {
		return -EINVAL;
	}

	if (fat_fs_volumes[index].allocated == 0) {
		return -EINVAL;
	}

	if (fat_fs_volumes[index].mounted == 0) {
		return -EINVAL;
	}

	return index;
}

int fs_manager_disk_init(const u8_t *volume_name)
{
	int index, ret;
	FATFS *fat_fs;

	for (index = 0; index < DISK_MAX_NUM; index++) {
		if (!strcmp(volume_name, fat_fs_volume_names[index])) {
			break;
		}
	}

	if (index >= DISK_MAX_NUM) {
		return -EINVAL;
	}

	if (fat_fs_volumes[index].mounted) {
		return 0;
	}

	if (fat_fs_volumes[index].allocated == 0) {
		fat_fs = mem_malloc(sizeof(*fat_fs));
		if (!fat_fs) {
			return -ENOMEM;
		}

		fat_fs_volumes[index].fat_fs = fat_fs;
		fat_fs_volumes[index].allocated = 1;
	} else {
		fat_fs = fat_fs_volumes[index].fat_fs;
	}

	ret = fs_disk_init(fat_fs, volume_name);
	if (ret) {
		SYS_LOG_ERR("fs (%s) init failed", volume_name);
	} else {
		SYS_LOG_INF("fs (%s) init success", volume_name);
		fat_fs_volumes[index].mounted = 1;
	}

	return ret;
}

int fs_manager_disk_uninit(const u8_t *volume_name)
{
	int index, ret;

	index = fs_manager_get_volume(volume_name);
	if (index < 0) {
		return 0;	/* maybe umounted */
	}

	ret = fs_disk_uninit(volume_name);
	if (ret) {
		SYS_LOG_ERR("fs (%s) uninit failed (%d)", volume_name, ret);
		return ret;
	}

	fat_fs_volumes[index].mounted = 0;

	return 0;
}

int fs_manager_get_volume_state(const u8_t *volume_name)
{
	if (fs_manager_get_volume(volume_name) < 0) {
		return 0;
	}

	return 1;
}

int fs_manager_get_free_capacity(const u8_t *volume_name)
{
	struct fs_statvfs stat;
	int ret;

	if (fs_manager_get_volume(volume_name) < 0) {
		return 0;
	}

	ret = fs_statvfs(volume_name, &stat);
	if (ret) {
		SYS_LOG_ERR("Error [%d]", ret);
		ret = 0;
	} else {
		ret = stat.f_frsize / 1024 * stat.f_bfree / 1024;
		SYS_LOG_INF("total %d Mbytes", ret);
	}

	return ret;
}

int fs_manager_get_total_capacity(const u8_t *volume_name)
{
	struct fs_statvfs stat;
	int ret;

	if (fs_manager_get_volume(volume_name) < 0) {
		return 0;
	}

	ret = fs_statvfs(volume_name, &stat);
	if (ret) {
		SYS_LOG_ERR("Error [%d]", ret);
		ret = 0;
	} else {
		ret = stat.f_frsize / 1024 * stat.f_blocks / 1024;
		SYS_LOG_INF("total %d Mbytes", ret);
	}

	return ret;
}

int fs_manager_sdcard_exit_high_speed(void)
{
#ifdef CONFIG_MMC_SDCARD_DEV_NAME
	struct device *sd_disk = device_get_binding(CONFIG_MMC_SDCARD_DEV_NAME);
	if (!sd_disk) {
		return -ENODEV;
	}

	return flash_ioctl(sd_disk, DISK_IOCTL_EXIT_HIGH_SPEED, NULL);
#else
	return 0;
#endif
}

int fs_manager_sdcard_enter_high_speed(void)
{
#ifdef CONFIG_MMC_SDCARD_DEV_NAME
	struct device *sd_disk = device_get_binding(CONFIG_MMC_SDCARD_DEV_NAME);

	if (!sd_disk) {
		return -ENODEV;
	}

	return flash_ioctl(sd_disk, DISK_IOCTL_ENTER_HIGH_SPEED, NULL);
#else
	return 0;
#endif
}

int fs_manager_get_fsinfo(const u8_t *volume_name, FATFS** fatfs)
{
	int index;

	for (index = 0; index < DISK_MAX_NUM; index++) {
		if (!strcmp(volume_name, fat_fs_volume_names[index])) {
			break;
		}
	}

	if (index >= DISK_MAX_NUM) {
		return -EINVAL;
	}

	*fatfs = fat_fs_volumes[index].fat_fs;
	return 0;
}
