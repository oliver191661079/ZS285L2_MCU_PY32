/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief watchdog hal interface
 */

#include <os_common_api.h>
#include <logging/sys_log.h>
#include <watchdog.h>
#include <watchdog_hal.h>

struct device *system_wdg_dev;

void watchdog_overflow(struct device *dev)
{
	panic("watchdog overflow");
}

void watchdog_start(int timeout_ms)
{
	struct wdt_config config;
#ifdef CONFIG_WDT_ACTS_DEVICE_NAME
	system_wdg_dev = device_get_binding(CONFIG_WDT_ACTS_DEVICE_NAME);
#endif

	if (!system_wdg_dev) {
		SYS_LOG_ERR("cannot found watchdog device");
		return;
	}

	config.timeout = timeout_ms;
#ifdef CONFIG_WDT_MODE_RESET
	config.mode = WDT_MODE_RESET;
#else
	config.mode = WDT_MODE_INTERRUPT_RESET;
#endif
	config.interrupt_fn = watchdog_overflow;
	wdt_set_config(system_wdg_dev, &config);

	SYS_LOG_INF("enable watchdog (%d ms)", config.timeout);

	wdt_enable(system_wdg_dev);
}

void watchdog_clear(void)
{
	if (system_wdg_dev) {
		wdt_reload(system_wdg_dev);
	}
}

void watchdog_stop(void)
{
	SYS_LOG_INF("disable watchdog");

	if (system_wdg_dev) {
		wdt_disable(system_wdg_dev);
		system_wdg_dev = NULL;
	}
}
