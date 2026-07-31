/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief ota view
 */
#define SYS_LOG_DOMAIN "ota"

#include <logging/sys_log.h>
#include "ota_app.h"
#include "app_ui.h"
#include <string.h>
#include <input_manager.h>
#include <ui_manager.h>
#include <property_manager.h>
#include "app_defines.h"

const ui_key_map_t ota_keymap[] = {
	{KEY_VOLUMEUP, KEY_TYPE_ALL, 1, MSG_INPUT_NULL},
	{KEY_VOLUMEDOWN, KEY_TYPE_ALL, 1, MSG_INPUT_NULL},
#ifdef CONFIG_OTA_APP_AUTO_SWITCH
	{KEY_MENU, KEY_TYPE_ALL, 1, MSG_INPUT_NULL},
#endif
	{KEY_POWER, KEY_TYPE_LONG_DOWN, 1, MSG_OTA_MESSAGE_CMD_POWEROFF},
	{KEY_NEXTSONG, KEY_TYPE_ALL, 1, MSG_INPUT_NULL},
	{KEY_PREVIOUSSONG, KEY_TYPE_ALL, 1, MSG_INPUT_NULL},
	{KEY_TBD, KEY_TYPE_ALL, 1, MSG_INPUT_NULL},
	{KEY_RESERVED, KEY_TYPE_ALL, 1, MSG_INPUT_NULL}
};

static int _ota_view_proc(u8_t view_id, u8_t msg_id, u32_t msg_data)
{
	switch (msg_id) {
	case MSG_VIEW_CREATE:
		{
#ifdef CONFIG_SEG_LED_MANAGER
			seg_led_manager_clear_screen(LED_CLEAR_ALL);
			seg_led_display_string(SLED_NUMBER1, "UpgD", true);
#endif
#ifdef CONFIG_LED_MANAGER_APP_USAGE
			led_manager_set_blink(0, 200, 100, OS_FOREVER,
					      LED_START_STATE_ON, NULL);
			led_manager_set_blink(1, 200, 100, OS_FOREVER,
					      LED_START_STATE_OFF, NULL);
#endif

			SYS_LOG_INF("CREATE\n");
			break;
		}
	case MSG_VIEW_DELETE:
		{
			SYS_LOG_INF("DELETE\n");
#ifdef CONFIG_LED_MANAGER_APP_USAGE
			led_manager_set_display(0, LED_OFF, OS_FOREVER, NULL);
			led_manager_set_display(1, LED_OFF, OS_FOREVER, NULL);
#endif
			break;
		}
	case MSG_VIEW_PAINT:
		{
			break;
		}
	}
	return 0;
}

void ota_view_init(void)
{
	ui_view_info_t view_info;

	memset(&view_info, 0, sizeof(ui_view_info_t));
	view_info.view_proc = _ota_view_proc;
	view_info.view_key_map = ota_keymap;
	view_info.order = 1;
	view_info.app_id = APP_ID_DESKTOP;

	ui_view_create(OTA_VIEW, &view_info);

	SYS_LOG_INF("ok\n");
}

void ota_view_deinit(void)
{
	ui_view_delete(OTA_VIEW);
#ifdef CONFIG_SEG_LED_MANAGER
	seg_led_manager_clear_screen(LED_CLEAR_ALL);
#endif
#ifdef CONFIG_PROPERTY
	property_flush(NULL);
#endif
	SYS_LOG_INF("ok\n");
}

void ota_view_show_upgrade_result(u8_t * string, bool is_faill)
{
#ifdef CONFIG_SEG_LED_MANAGER
	seg_led_display_string(SLED_NUMBER1, string, true);
	seg_led_manager_set_timeout_event_start();
	seg_led_manager_set_timeout_event(1000, NULL);
#endif
#ifdef CONFIG_LED_MANAGER_APP_USAGE
	if (is_faill) {
		led_manager_set_display(0, LED_OFF, OS_FOREVER, NULL);
		led_manager_set_display(1, LED_OFF, OS_FOREVER, NULL);
	} else {
		led_manager_set_display(0, LED_ON, OS_FOREVER, NULL);
		led_manager_set_display(1, LED_ON, OS_FOREVER, NULL);
	}
#endif
}

void ota_view_show_upgrade_progress(u8_t progress)
{
	u8_t progress_string[5];
	snprintf(progress_string, sizeof(progress_string), "U%u", progress);
#ifdef CONFIG_SEG_LED_MANAGER
	seg_led_manager_clear_screen(LED_CLEAR_ALL);
	seg_led_display_string(SLED_NUMBER1, progress_string, true);
#endif
}
