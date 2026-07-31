/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "demo_app.h"
#include "ui_manager.h"
#include <input_manager.h>
#include <ui_manager.h>

static const ui_key_map_t demo_keymap[] = {
#ifdef CONFIG_BOARD_ATS2875H_RIDE
    {KEY_PAUSE_AND_RESUME, KEY_TYPE_SHORT_UP, USOUND_STATUS_ALL, MSG_DEMO_PLAY_PAUSE_RESUME},
    {KEY_PAUSE_AND_RESUME, KEY_TYPE_ALL & (~KEY_TYPE_SHORT_UP), USOUND_STATUS_ALL, MSG_DEMO_NO_ACTION},
#else
    {KEY_POWER, KEY_TYPE_SHORT_UP, USOUND_STATUS_ALL, MSG_DEMO_PLAY_PAUSE_RESUME},
    {KEY_POWER, KEY_TYPE_ALL & (~KEY_TYPE_SHORT_UP), USOUND_STATUS_ALL, MSG_DEMO_NO_ACTION},
#endif
	//{KEY_COMBO_VOL, KEY_TYPE_SHORT_UP, USOUND_STATUS_ALL, MSG_DEMO_EXIT},
	
    //Mask key actions on main view.
    {KEY_VOLUMEDOWN, KEY_TYPE_ALL, USOUND_STATUS_ALL, MSG_DEMO_NO_ACTION},
    {KEY_VOLUMEUP, KEY_TYPE_ALL, USOUND_STATUS_ALL, MSG_DEMO_NO_ACTION},
    {KEY_BT, KEY_TYPE_ALL, USOUND_STATUS_ALL, MSG_DEMO_NO_ACTION},
    {KEY_TBD, KEY_TYPE_ALL, USOUND_STATUS_ALL, MSG_DEMO_NO_ACTION},

    {KEY_RESERVED, 0, 0, 0}};

static int _demo_view_proc(u8_t view_id, u8_t msg_id, u32_t msg_data)
{
	SYS_LOG_INF("view %d msg %d\n", view_id, msg_id);

	switch (msg_id) {
	case MSG_VIEW_CREATE:
	{
		SYS_LOG_INF("CREATE\n");
		break;
	}
	case MSG_VIEW_DELETE:
	{
		break;
	}
	case MSG_VIEW_PAINT:
	{
		break;
	}
	}
	return 0;
}

void demo_view_init(void)
{
	ui_view_info_t  view_info;

	memset(&view_info, 0, sizeof(ui_view_info_t));

	view_info.view_proc = _demo_view_proc;
	view_info.view_key_map = demo_keymap;
	view_info.view_get_state = demo_get_status;
	view_info.order = 1;
	view_info.app_id = APP_ID_DESKTOP;

	ui_view_create(DEMO_VIEW, &view_info);

#ifdef CONFIG_LED_MANAGER
	led_manager_set_display(0, LED_OFF, OS_FOREVER, NULL);
#endif

	sys_event_notify(SYS_EVENT_ENTER_DEMO);

	SYS_LOG_INF(" ok\n");
}

void demo_view_deinit(void)
{
	ui_view_delete(DEMO_VIEW);

	SYS_LOG_INF("ok\n");
}

void demo_show_play_status(bool status)
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

