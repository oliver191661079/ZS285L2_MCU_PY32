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

void logic_mcu_ls8a10049t_int_clear(void)
{
	SYS_LOG_INF("fixme");
}

void logic_mcu_ls8a10049t_power_off(void)
{
	SYS_LOG_INF("fixme");
}

void run_mode_is_demo_simple(void)
{
	SYS_LOG_INF("fixme");
}

void flip7_is_idle_mode(void)
{
	SYS_LOG_INF("fixme");
}

void flip7_is_standby_mode(void)
{
	SYS_LOG_INF("fixme");
}

void hm_flip7_get_battery_voltage(void)
{
	SYS_LOG_INF("fixme");
}

void hm_flip7_get_battery_cap_percentage(void)
{
	SYS_LOG_INF("fixme");
}

void  hm_flip7_update_ntc_adc_val(void)
{
	SYS_LOG_INF("fixme");
}

void ep_is_busy(void)
{
	SYS_LOG_INF("fixme");
}

void usb_phy_dc_attached(void)
{
	SYS_LOG_INF("fixme");
}