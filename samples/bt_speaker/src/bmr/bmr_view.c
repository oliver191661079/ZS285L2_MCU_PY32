/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bmr app view
 */

#include "bmr.h"

#ifdef CONFIG_UGUI
#include <resource.h>
#endif

static int _bmr_get_status(void)
{
	return BT_STATUS_A2DP_ALL;
}

static const ui_key_map_t bmr_keymap[] = {
	{ KEY_VOLUMEUP,     KEY_TYPE_SHORT_UP | KEY_TYPE_LONG | KEY_TYPE_HOLD,	BT_STATUS_A2DP_ALL, MSG_BMR_PLAY_VOLUP},
	{ KEY_VOLUMEDOWN,   KEY_TYPE_SHORT_UP | KEY_TYPE_LONG | KEY_TYPE_HOLD,	BT_STATUS_A2DP_ALL, MSG_BMR_PLAY_VOLDOWN},
#ifdef CONFIG_BOARD_ATS2875H_RIDE
	{ KEY_PAUSE_AND_RESUME,KEY_TYPE_LONG_UP, BT_STATUS_A2DP_ALL, MSG_BMR_PLAY_APP_EXIT},
#else
	{ KEY_TBD, KEY_TYPE_SHORT_UP,	BT_STATUS_A2DP_ALL, MSG_BMR_PLAY_APP_EXIT},
	{ KEY_POWER, KEY_TYPE_SHORT_UP,	BT_STATUS_A2DP_ALL, MSG_BMR_PAUSE_RESUME},
#endif
	{ KEY_RESERVED,     0,  0, 0}
};

static int _bmr_view_proc(u8_t view_id, u8_t msg_id, u32_t msg_data)
{
	switch (msg_id) {
	case MSG_VIEW_CREATE:
	{
		SYS_LOG_INF("CREATE\n");
		break;
	}
	case MSG_VIEW_DELETE:
	{
		SYS_LOG_INF(" DELETE\n");

		break;
	}
	case MSG_VIEW_PAINT:
	{
		break;
	}
	}
	return 0;
}

void bmr_view_show_connected(bool connected)
{
#ifdef CONFIG_LED_MANAGER
	if (connected) {
		led_manager_set_breath(0, NULL, OS_FOREVER, NULL);
	} else {
		led_manager_set_display(0, LED_OFF, OS_FOREVER, NULL);
	}
#endif
}

void bmr_view_init(void)
{
	ui_view_info_t  view_info;

	memset(&view_info, 0, sizeof(ui_view_info_t));

	view_info.view_proc = _bmr_view_proc;
	view_info.view_key_map = bmr_keymap;
	view_info.view_get_state = _bmr_get_status;
	view_info.order = 1;
	view_info.app_id = APP_ID_DESKTOP;

	ui_view_create(BMR_VIEW, &view_info);

#ifdef CONFIG_SEG_LED_MANAGER
	seg_led_manager_clear_screen(LED_CLEAR_ALL);
#endif

	if (bt_manager_get_connected_dev_num() == 0) {
	#ifdef CONFIG_SEG_LED_MANAGER
		seg_led_display_string(SLED_NUMBER2, "bL", true);
		seg_led_display_string(SLED_NUMBER4, " ", false);
		seg_led_display_set_flash(FLASH_2ITEM(SLED_NUMBER2, SLED_NUMBER3), 1000, FLASH_FOREVER, NULL);
	#ifdef CONFIG_TWS
		if (bt_manager_tws_get_dev_role() == BTSRV_TWS_MASTER) {
			seg_led_display_string(SLED_NUMBER1, "-", true);
		}
	#endif
	#endif
	#ifdef CONFIG_LED_MANAGER_APP_USAGE
		led_manager_set_blink(0, 200, 100, OS_FOREVER, LED_START_STATE_ON, NULL);
		led_manager_set_blink(1, 200, 100, OS_FOREVER, LED_START_STATE_OFF, NULL);
	#endif
	} else {
	#ifdef CONFIG_SEG_LED_MANAGER
		seg_led_display_string(SLED_NUMBER2, "bL", true);
		seg_led_display_string(SLED_NUMBER4, " ", false);
	#ifdef CONFIG_TWS
		if (bt_manager_tws_get_dev_role() == BTSRV_TWS_MASTER) {
			seg_led_display_string(SLED_NUMBER1, "-", true);
		}
	#endif
		seg_led_display_icon(SLED_PAUSE, true);
		seg_led_display_icon(SLED_PLAY, false);
	#endif
	#ifdef CONFIG_LED_MANAGER_APP_USAGE
		led_manager_set_display(0, LED_OFF, OS_FOREVER, NULL);
		led_manager_set_display(1, LED_ON, OS_FOREVER, NULL);
	#endif
	}
	SYS_LOG_INF("ok\n");
}

void bmr_view_deinit(void)
{
	ui_view_delete(BMR_VIEW);

	SYS_LOG_INF("ok\n");
}