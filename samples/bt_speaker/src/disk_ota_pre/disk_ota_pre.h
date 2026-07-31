/*
 * Copyright (c) 2024 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief disk_ota_pre header file.
 */

#ifndef _DISK_OTA_PRE_APP_H
#define _DISK_OTA_PRE_APP_H

#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "disk_ota_pre"

#include <logging/sys_log.h>
#include <msg_manager.h>
#include <thread_timer.h>
#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <soc_dvfs.h>

#include "app_defines.h"
#include "sys_manager.h"
#include "app_ui.h"
#include "desktop_manager.h"
#include "mem_manager.h"

#ifdef CONFIG_USB_HOST
#include <usb/usb_host.h>
#endif

#ifdef CONFIG_OTA_APP
#include "../ota/ota_app.h"
#endif
#include "fs_manager.h"
#include "hotplug_manager.h"

enum DISK_OTA_PRE_STATUS {
	DISK_OTA_PRE_STATUS_NULL = 0x0000,
	DISK_OTA_PRE_STATUS_SCAN = 0x0001,
	DISK_OTA_PRE_STATUS_FINISH = 0x0002,
};

#define DISK_OTA_PRE_DVFS_LEVEL (SOC_DVFS_LEVEL_HIGH_PERFORMANCE)

struct disk_ota_pre_app_t {
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	u8_t set_dvfs_level;
#endif
	u8_t status;
};


enum {
	MSG_DISK_OTA_PRE_CMD_NULL = 0,
	MSG_DISK_OTA_PRE_UHOST_SCAN_FINISH = 1,
};

struct disk_ota_pre_app_t *disk_ota_pre_get_app(void);
void disk_ota_pre_view_deinit(void);
void disk_ota_pre_view_clear_screen(void);

#endif				/* _DISK_OTA_PRE_APP_H */
