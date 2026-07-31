/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "recovery"

#include <kernel.h>
#include <device.h>
#include <string.h>
#include <misc/util.h>
#include <logging/sys_log.h>
#include <fw_version.h>
#include <ota_upgrade.h>
#include <ota_backend.h>
#include <ota_backend_temp_part.h>
#include <ota_backend_disk.h>
#include <soc_pm.h>
#include <soc.h>

#ifdef CONFIG_FS_MANAGER
#include <fs_manager.h>
#endif

#ifdef CONFIG_PROPERTY
#include <property_manager.h>
#endif

#include "sys_manager.h"
#include "flash.h"



#ifdef CONFIG_USB_HOTPLUG
extern int usb_hotplug_host_check(void);
#endif

static struct ota_upgrade_info *g_ota;

#ifdef CONFIG_OTA_BACKEND_TEMP_PART
#define OTA_STORAGE_DEVICE_NAME		CONFIG_XSPI_NOR_ACTS_DEV_NAME
static struct ota_backend *backend_spinor;
#endif

#ifdef CONFIG_OTA_BACKEND_SDCARD
#define OTA_STORAGE_DEVICE_NAME		CONFIG_XSPI_NOR_ACTS_DEV_NAME
static struct ota_backend *backend_sdcard;
#endif

static void ota_app_start(void)
{
	os_sleep(10);

	SYS_LOG_INF("ota app start");
	struct device *flash_device =
	    device_get_binding(CONFIG_XSPI_NOR_ACTS_DEV_NAME);
	if (flash_device){
		//flash_write_protection_set(flash_device, false);
		flash_write_protection_region_set(flash_device, FLASH_PROTECT_REGION_NONE);
	}
}

static void ota_app_stop(void)
{
	SYS_LOG_INF("ota app stop");
	struct device *flash_device =
	    device_get_binding(CONFIG_XSPI_NOR_ACTS_DEV_NAME);
	if (flash_device){
		//flash_write_protection_set(flash_device, true);
		flash_write_protection_region_set(flash_device, FLASH_PROTECT_REGION_ALL_EXCLUDE_NVRAM);
	}
}

static int ota_app_notify(int state, int old_state)
{
	SYS_LOG_INF("ota state: %d->%d", old_state, state);

	if (old_state != OTA_RUNNING && state == OTA_RUNNING) {
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		soc_dvfs_set_level(SOC_DVFS_LEVEL_BR_FULL_PERFORMANCE, "ota");
#endif
	} else if (old_state == OTA_RUNNING && state != OTA_RUNNING) {
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		soc_dvfs_unset_level(SOC_DVFS_LEVEL_BR_FULL_PERFORMANCE, "ota");
#endif
	}

	return 0;
}

static void ota_app_backend_callback(struct ota_backend *backend, int cmd,
				     int state)
{
	int err;

	SYS_LOG_INF("backend %p state %d", backend, state);
	if (cmd == OTA_BACKEND_UPGRADE_STATE) {
		if (state == 1) {
			err = ota_upgrade_attach_backend(g_ota, backend);
			if (err) {
				SYS_LOG_INF("unable attach backend");
				return;
			}
		} else {
			ota_upgrade_detach_backend(g_ota, backend);
		}
	}
}

#ifdef CONFIG_OTA_BACKEND_TEMP_PART
int ota_app_init_spinor(void)
{
	struct ota_backend_temp_part_init_param temp_part_init_param;

	memset(&temp_part_init_param, 0x0,
	       sizeof(struct ota_backend_temp_part_init_param));
	temp_part_init_param.dev_name = CONFIG_OTA_TEMP_PART_DEV_NAME;

	backend_spinor =
	    ota_backend_temp_part_init(ota_app_backend_callback,
				       &temp_part_init_param);
	if (!backend_spinor) {
		SYS_LOG_ERR("failed to init temp part for ota");
		return -ENODEV;
	}

	return 0;
}
#endif

#ifdef CONFIG_OTA_BACKEND_SDCARD
struct ota_backend_sdcard_init_param sdcard_init_param = {
	.fpath = "SD:ota.bin",
};

struct ota_backend_sdcard_init_param usb_init_param = {
	.fpath = "USB:ota.bin",
};

int ota_app_init_sdcard(void)
{
	backend_sdcard =
	    ota_backend_sdcard_init(ota_app_backend_callback,
				    &sdcard_init_param);
	if (!backend_sdcard) {
		backend_sdcard =
		    ota_backend_sdcard_init(ota_app_backend_callback,
					    &usb_init_param);
		if (!backend_sdcard) {
			SYS_LOG_INF("failed to init sdcard ota");
			return -ENODEV;
		}
	}

	printk("ota_app_init_sdcard ok \n");
	return 0;
}
#endif

const unsigned char *ota_get_public_key(void);

int ota_app_init(void)
{
	struct ota_upgrade_param param;

	SYS_LOG_INF("in");

	memset(&param, 0x0, sizeof(struct ota_upgrade_param));

	param.storage_name = OTA_STORAGE_DEVICE_NAME;
#ifdef CONFIG_OTA_DOWNLOAD_EXTERNAL_FLASH
	param.storage_ext_name = CONFIG_XSPI1_NOR_ACTS_DEV_NAME;
#endif
	param.notify = ota_app_notify;
	param.flag_use_recovery = 1;
	param.flag_use_recovery_app = 1;
	param.no_version_control = 1;
#ifdef CONFIG_SOC_SECURE_BOOT
	param.flag_secure_boot = 1;
	param.public_key = ota_get_public_key();
#endif

	struct device *flash_device =
	    device_get_binding(CONFIG_XSPI_NOR_ACTS_DEV_NAME);

	if (flash_device){
		//flash_write_protection_set(flash_device, false);
		flash_write_protection_region_set(flash_device, FLASH_PROTECT_REGION_NONE);
	}

	g_ota = ota_upgrade_init(&param);
	if (!g_ota) {
		SYS_LOG_INF("ota app init error");
		if (flash_device){
			//flash_write_protection_set(flash_device, true);
			flash_write_protection_region_set(flash_device, FLASH_PROTECT_REGION_ALL_EXCLUDE_NVRAM);
		}
		return -1;
	}

	SYS_LOG_INF("out");
	return 0;
}

extern void trace_init(void);
extern void system_library_version_dump(void);

static uint8_t __ota_bss ota_data_buf[0x1000];
void main(void)
{
	struct ota_upgrade_check_param param;
	int reboot_type = REBOOT_TYPE_GOTO_SYSTEM | REBOOT_REASON_OTA_FAILED;

	soc_watchdog_stop();

#ifdef CONFIG_USB_HOTPLUG
	usb_hotplug_host_check();
#endif

	trace_init();

	system_library_version_dump();

	SYS_LOG_INF("ota recovery main");

	ota_app_init();

#ifdef CONFIG_OTA_BACKEND_TEMP_PART
	ota_upgrade_set_ota_buf(g_ota, ota_data_buf, sizeof(ota_data_buf));
 	if (ota_upgrade_verify_temp_image(g_ota) != 0) {
		SYS_LOG_INF("skip ota recovery");
		goto exit;
	}

	SYS_LOG_INF("do ota recovery");

#ifdef CONFIG_OTA_BACKEND_TEMP_PART
	ota_app_init_spinor();
#endif
#endif

#ifdef CONFIG_OTA_BACKEND_SDCARD
#ifdef CONFIG_MUTIPLE_VOLUME_MANAGER
	fs_manager_init();
#endif

	if (ota_app_init_sdcard()) {
		SYS_LOG_INF("init sdcard failed");
		goto exit;
	}
#endif

	param.data_buf = ota_data_buf;
	param.data_buf_size = sizeof(ota_data_buf);

	ota_app_start();

	if (ota_upgrade_check(g_ota, &param)) {
		ota_app_stop();
		while (true) {
			SYS_LOG_INF
			    ("ota upgrade failed, wait check ota file \n");
			k_sleep(1000);
		}
	}else{
		ota_app_stop();
	}

 	reboot_type = REBOOT_TYPE_GOTO_SYSTEM | REBOOT_REASON_OTA_FINISHED;

 exit:
	SYS_LOG_INF("reboot %d", reboot_type);
	k_sleep(50);
	sys_pm_reboot(reboot_type);
}
