/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lemusic.h"

#define BT_STATUS_LEMUSIC_ALL 1	/* non-zero */

static const ui_key_map_t lemusic_keymap[] = {
	{KEY_VOLUMEUP, KEY_TYPE_SHORT_UP | KEY_TYPE_LONG | KEY_TYPE_HOLD,
	 BT_STATUS_LEMUSIC_ALL, MSG_BT_PLAY_VOLUP},
	{KEY_VOLUMEDOWN, KEY_TYPE_SHORT_UP | KEY_TYPE_LONG | KEY_TYPE_HOLD,
	 BT_STATUS_LEMUSIC_ALL, MSG_BT_PLAY_VOLDOWN},
	{KEY_POWER, KEY_TYPE_SHORT_UP, BT_STATUS_LEMUSIC_ALL,
	 MSG_BT_PLAY_PAUSE_RESUME},
	{KEY_NEXTSONG, KEY_TYPE_SHORT_UP, BT_STATUS_LEMUSIC_ALL, MSG_BT_PLAY_NEXT},
	{KEY_PREVIOUSSONG, KEY_TYPE_SHORT_UP, BT_STATUS_LEMUSIC_ALL,
	 MSG_BT_PLAY_PREVIOUS},
	{KEY_NEXTSONG, KEY_TYPE_LONG_DOWN, BT_STATUS_LEMUSIC_ALL,
	 MSG_BT_PLAY_SEEKFW_START},
	{KEY_PREVIOUSSONG, KEY_TYPE_LONG_DOWN, BT_STATUS_LEMUSIC_ALL,
	 MSG_BT_PLAY_SEEKBW_START},
	{KEY_NEXTSONG, KEY_TYPE_LONG_UP, BT_STATUS_LEMUSIC_ALL,
	 MSG_BT_PLAY_SEEKFW_STOP},
	{KEY_PREVIOUSSONG, KEY_TYPE_LONG_UP, BT_STATUS_LEMUSIC_ALL,
	 MSG_BT_PLAY_SEEKBW_STOP},
#ifdef CONFIG_BOARD_ATS2875H_RIDE
	{KEY_PAUSE_AND_RESUME, KEY_TYPE_LONG_UP, BT_STATUS_LEMUSIC_ALL, MSG_SWITCH_BROADCAST},
#else
	{KEY_TBD, KEY_TYPE_SHORT_UP, BT_STATUS_LEMUSIC_ALL, MSG_SWITCH_BROADCAST},
#endif
	{KEY_RESERVED, 0, 0, 0}
};

static int lemusic_view_proc(u8_t view_id, u8_t msg_id, u32_t msg_data)
{
	switch (msg_id) {
	case MSG_VIEW_CREATE:
#ifdef CONFIG_LED_MANAGER_APP_USAGE
		led_manager_set_display(0, LED_ON, OS_FOREVER, NULL);
		led_manager_set_display(1, LED_OFF, OS_FOREVER, NULL);
#endif
		SYS_LOG_INF("CREATE\n");
		break;
	case MSG_VIEW_DELETE:
#ifdef CONFIG_LED_MANAGER_APP_USAGE
		led_manager_set_display(0, LED_OFF, OS_FOREVER, NULL);
		led_manager_set_display(1, LED_OFF, OS_FOREVER, NULL);
#endif
		SYS_LOG_INF("DELETE\n");
		break;
	case MSG_VIEW_PAINT:
		break;
	}

	return 0;
}

void lemusic_view_show_connected(void)
{
#ifdef CONFIG_SEG_LED_MANAGER
	seg_led_manager_clear_screen(LED_CLEAR_ALL);
	seg_led_display_string(SLED_NUMBER1, " bL ", true);
	seg_led_display_icon(SLED_PAUSE, true);
#ifdef CONFIG_TWS
	if (bt_manager_tws_get_dev_role() == BTSRV_TWS_MASTER) {
		seg_led_display_string(SLED_NUMBER1, "-", true);
	}
#endif
#endif
#ifdef CONFIG_LED_MANAGER_APP_USAGE
	led_manager_set_display(0, LED_ON, OS_FOREVER, NULL);
	led_manager_set_display(1, LED_ON, OS_FOREVER, NULL);
#endif
}

void lemusic_view_show_disconnected(void)
{
#ifdef CONFIG_SEG_LED_MANAGER
	seg_led_display_string(SLED_NUMBER2, "bL", true);
	seg_led_display_string(SLED_NUMBER4, " ", false);
	seg_led_display_set_flash(FLASH_2ITEM(SLED_NUMBER2, SLED_NUMBER3), 1000,
				  FLASH_FOREVER, NULL);
	seg_led_display_icon(SLED_PAUSE, false);
	seg_led_display_icon(SLED_PLAY, false);
#ifdef CONFIG_TWS
	if (bt_manager_tws_get_dev_role() == BTSRV_TWS_MASTER) {
		seg_led_display_string(SLED_NUMBER1, "-", true);
	}
#endif
#endif
#ifdef CONFIG_LED_MANAGER_APP_USAGE
	led_manager_set_blink(0, 200, 100, OS_FOREVER, LED_START_STATE_ON,
			      NULL);
	led_manager_set_blink(1, 200, 100, OS_FOREVER, LED_START_STATE_OFF,
			      NULL);
#endif
}

void lemusic_view_show_play_paused(bool playing)
{
#ifdef CONFIG_SEG_LED_MANAGER
	seg_led_display_icon(SLED_PAUSE, !playing);
	seg_led_display_icon(SLED_PLAY, playing);
#endif
#ifdef CONFIG_LED_MANAGER_APP_USAGE
	if (playing) {
		led_manager_set_breath(0, NULL, OS_FOREVER, NULL);
		led_manager_set_breath(1, NULL, OS_FOREVER, NULL);
	} else {
		led_manager_set_display(0, LED_ON, OS_FOREVER, NULL);
		led_manager_set_display(1, LED_ON, OS_FOREVER, NULL);
	}
#endif
}

void lemusic_view_init(void)
{
	ui_view_info_t view_info;

	memset(&view_info, 0, sizeof(ui_view_info_t));

	view_info.view_proc = lemusic_view_proc;
	view_info.view_key_map = lemusic_keymap;
	view_info.order = 1;
	view_info.app_id = APP_ID_DESKTOP;

	ui_view_create(LEMUSIC_VIEW, &view_info);

	sys_event_notify(SYS_EVENT_ENTER_LEMUSIC);

	SYS_LOG_INF("ok\n");
}

void lemusic_view_deinit(void)
{
	ui_view_delete(LEMUSIC_VIEW);

	SYS_LOG_INF("ok\n");
}
