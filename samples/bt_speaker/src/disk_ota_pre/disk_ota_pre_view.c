/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief disk_ota_pre app view
 */

#include "disk_ota_pre.h"
#include "ui_manager.h"
#include <input_manager.h>
#include <ui_manager.h>

static const ui_key_map_t disk_ota_pre_keymap[] = {
	{KEY_RESERVED, 0, 0, 0}
};

static int disk_ota_pre_get_status(void)
{
	struct disk_ota_pre_app_t *disk_ota_pre = disk_ota_pre_get_app();
	return disk_ota_pre->status;
}



static int _disk_ota_pre_view_proc(u8_t view_id, u8_t msg_id, u32_t msg_data)
{
	switch (msg_id) {
	case MSG_VIEW_CREATE:
		{
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

void disk_ota_pre_view_init(void)
{
	ui_view_info_t view_info;

	memset(&view_info, 0, sizeof(ui_view_info_t));

	view_info.view_proc = _disk_ota_pre_view_proc;
	view_info.view_key_map = disk_ota_pre_keymap;
	view_info.view_get_state = disk_ota_pre_get_status;
	view_info.order = 1;
	view_info.app_id = APP_ID_DESKTOP;

	ui_view_create(DISK_OTA_PRE_VIEW, &view_info);

	disk_ota_pre_view_clear_screen();

#ifdef CONFIG_SEG_LED_MANAGER
	seg_led_display_string(SLED_NUMBER1, "USBA", true);
#endif

	//sys_event_notify(SYS_EVENT_ENTER_disk_ota_pre);

	SYS_LOG_INF(" ok\n");
}

void disk_ota_pre_view_deinit(void)
{
	ui_view_delete(DISK_OTA_PRE_VIEW);

	SYS_LOG_INF("ok\n");
}

void disk_ota_pre_view_volume_show(int volume_value)
{
	static u32_t volume_timestampe = 0;
#ifdef CONFIG_SEG_LED_MANAGER
	u8_t volume[5];

	snprintf(volume, sizeof(volume), "U %02u", volume_value);
	seg_led_manager_set_timeout_event_start();
	seg_led_display_icon(SLED_P1, false);
	seg_led_display_icon(SLED_COL, false);
	seg_led_display_string(SLED_NUMBER1, volume, true);
	seg_led_manager_set_timeout_event(2000, NULL);
#endif

	if ((u32_t) (k_cycle_get_32() - volume_timestampe) / 24 < 1000000) {
		return;
	}
}

void disk_ota_pre_show_play_status(bool status)
{
#ifdef CONFIG_SEG_LED_MANAGER
	seg_led_display_icon(SLED_PAUSE, !status);
	seg_led_display_icon(SLED_PLAY, status);
#endif
#ifdef CONFIG_LED_MANAGER_APP_USAGE
	if (status) {
		led_manager_set_breath(0, NULL, OS_FOREVER, NULL);
		led_manager_set_breath(1, NULL, OS_FOREVER, NULL);
	} else {
		led_manager_set_display(0, LED_ON, OS_FOREVER, NULL);
		led_manager_set_display(1, LED_ON, OS_FOREVER, NULL);
	}
#endif
}

void disk_ota_pre_view_clear_screen(void)
{
#ifdef CONFIG_SEG_LED_MANAGER
	seg_led_manager_clear_screen(LED_CLEAR_ALL);
#endif
}
