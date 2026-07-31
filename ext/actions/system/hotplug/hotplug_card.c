/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file hotplug sdcard interface
 */

#if defined(CONFIG_SYS_LOG)
#define SYS_LOG_NO_NEWLINE
#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "hotpug_manager"
#include <logging/sys_log.h>
#endif
#include <os_common_api.h>
#include <msg_manager.h>
#include <string.h>
#include <kernel.h>
#include <fs_manager.h>
#include <disk_access.h>
#include <hotplug_manager.h>
#include <flash.h>
#include <soc_dvfs.h>

struct sdcard_detect_state_t {
	uint8_t prev_state;
	uint8_t stable_state;
	uint8_t detect_cnt;
};

static struct sdcard_detect_state_t sdcard_detect_state;

int _sdcard_get_state(void)
{
	u8_t stat = STA_NODISK; /* STA_DISK_OK; */
	struct device *dev = device_get_binding(CONFIG_MMC_SDCARD_DEV_NAME);

	int sdcard_state = HOTPLUG_NONE;

	if (dev)
		flash_ioctl(dev, DISK_IOCTL_HOTPLUG_PERIOD_DETECT, &stat);

	if (stat == STA_DISK_OK) {
		sdcard_state = HOTPLUG_IN;
	} else {
		sdcard_state = HOTPLUG_OUT;
	}

	return sdcard_state;
}

int _sdcard_hotplug_detect(void)
{
	int report_state = HOTPLUG_NONE;
	int state = HOTPLUG_NONE;

	state = _sdcard_get_state();

	if (state <= HOTPLUG_NONE) {
		goto exit;
	}

	if (state == sdcard_detect_state.prev_state) {
		sdcard_detect_state.detect_cnt++;
		if (sdcard_detect_state.detect_cnt >= 1) {
			sdcard_detect_state.detect_cnt = 0;
			if (state != sdcard_detect_state.stable_state) {
				sdcard_detect_state.stable_state = state;
				report_state = sdcard_detect_state.stable_state;
			}
		}
	} else {
		sdcard_detect_state.detect_cnt = 0;
		sdcard_detect_state.prev_state = state;
	}

exit:
	return report_state;
}

static int _sdcard_hotplug_fs_process(int device_state)
{
#ifdef CONFIG_FS_MANAGER
	int res = -1;
#endif

	switch (device_state) {
	case HOTPLUG_IN:
	#ifdef CONFIG_FS_MANAGER
#ifdef CONFIG_USB_SUPPORT_VDD_ADJUST
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		//soc_dvfs_set_level(SOC_DVFS_LEVEL_FULL_PERFORMANCE, "sdcard");
#endif
#endif
		//res = fs_manager_disk_init("SD:");
	#endif
		break;
	case HOTPLUG_OUT:
	#ifdef CONFIG_FS_MANAGER
#ifdef CONFIG_USB_SUPPORT_VDD_ADJUST
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		//soc_dvfs_unset_level(SOC_DVFS_LEVEL_FULL_PERFORMANCE, "sdcard");
#endif
#endif
		res = fs_manager_disk_uninit("SD:");
	#endif
		break;
	default:
		break;
	}
	return 0;
}

static const struct hotplug_device_t sdcard_hotplug_device = {
	.type = HOTPLUG_SDCARD,
	.get_state = _sdcard_get_state,
	.hotplug_detect = _sdcard_hotplug_detect,
	.fs_process = _sdcard_hotplug_fs_process,
};

int hotplug_sdcard_init(void)
{
	memset(&sdcard_detect_state, 0, sizeof(struct sdcard_detect_state_t));

#ifdef CONFIG_MUTIPLE_VOLUME_MANAGER
	if (fs_manager_get_volume_state("SD:")) {
		sdcard_detect_state.stable_state = HOTPLUG_IN;
	} else {
		sdcard_detect_state.stable_state = HOTPLUG_OUT;
	}
#endif

	return hotplug_device_register(&sdcard_hotplug_device);
}
