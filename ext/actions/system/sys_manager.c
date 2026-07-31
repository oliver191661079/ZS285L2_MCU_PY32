/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file system manager
 */

#include <os_common_api.h>
#include <msg_manager.h>
#include <app_manager.h>
#include <srv_manager.h>
#ifdef CONFIG_FS_MANAGER
#include <fs_manager.h>
#endif
#include <ui_manager.h>
#include <hotplug_manager.h>
#include <esd_manager.h>
#include <audio_system.h>
#include <sys_monitor.h>
#include <power_manager.h>
#include <property_manager.h>
#ifdef CONFIG_PLAYTTS
#include <tts_manager.h>
#endif

#ifdef CONFIG_FM
#include <fm_manager.h>
#endif

#ifdef CONFIG_TASK_WDT
#include <debug/task_wdt.h>
#endif

#ifdef CONFIG_POWEROFF_SIMULATOR
#include <debug/poweroff_simulator.h>
#endif

#include <sys_manager.h>

void system_ready(void)
{
	struct sys_monitor_t *sys_monitor = sys_monitor_get_instance();

	if (sys_monitor)
		sys_monitor->system_ready = 1;
}

bool system_is_ready(void)
{
	struct sys_monitor_t *sys_monitor = sys_monitor_get_instance();

	if (sys_monitor)
		return (sys_monitor->system_ready == 1);

	return false;
}

int system_restore_factory_config(void)
{

#ifdef CONFIG_PROPERTY
	property_flush(NULL);
#endif

	return 0;
}

void system_pre_init(void)
{
	msg_manager_init();
	app_manager_init();
}

void system_init(void)
{
#ifdef CONFIG_PROPERTY
	property_manager_init();
#endif

#ifdef CONFIG_FS_MANAGER
	fs_manager_init();
#endif

	sys_monitor_init();

#ifdef CONFIG_ESD_MANAGER
	esd_manager_init();
#endif

#ifdef CONFIG_MEDIA
	aduio_system_init();
#endif

#ifdef CONFIG_PLAYTTS
	tts_manager_init();
#endif

#ifdef CONFIG_POWER_MANAGER
	power_manager_init();
#endif

#ifdef CONFIG_UI_MANAGER
	ui_manager_init();
#endif

#ifdef CONFIG_HOTPLUG_MANAGER
	hotplug_manager_init();
#endif

#ifdef CONFIG_FM
	fm_manager_init();
#endif

#ifdef CONFIG_TASK_WDT
	task_wdt_init(NULL);
#endif

	sys_monitor_start();

#ifdef CONFIG_POWEROFF_SIMULATOR
	poweroff_sim_init();
#endif

	system_le_cleanup_nvram_on_boot();
}

void system_deinit(void)
{
	sys_monitor_stop();

#ifdef CONFIG_UI_MANAGER
	ui_manager_exit();
#endif

#ifdef CONFIG_PLAYTTS
	tts_manager_deinit();
#endif

#ifdef CONFIG_ESD_MANAGER
	esd_manager_deinit();
#endif

#ifdef CONFIG_FM
	fm_manager_deinit();
#endif
}
