/*
 * Copyright (c) 2024 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief lcmusic app view
 */

#include "lcmusic.h"
#include "ui_manager.h"
#include <input_manager.h>
#include <ui_manager.h>

#define CONFIG_SUPPORT_KEY_SW_DIR	1

static void _lcmusic_display_icon_disk(void);

static const ui_key_map_t lcmusic_keymap[] = {

	{KEY_VOLUMEUP, KEY_TYPE_SHORT_UP | KEY_TYPE_LONG | KEY_TYPE_HOLD,
	 LCMUSIC_STATUS_ALL, MSG_LCMPLAYER_PLAY_VOLUP},
	{KEY_VOLUMEDOWN, KEY_TYPE_SHORT_UP | KEY_TYPE_LONG | KEY_TYPE_HOLD,
	 LCMUSIC_STATUS_ALL, MSG_LCMPLAYER_PLAY_VOLDOWN},
	{KEY_POWER, KEY_TYPE_SHORT_UP, LCMUSIC_STATUS_PLAYING,
	 MSG_LCMPLAYER_PLAY_PAUSE},
	{KEY_POWER, KEY_TYPE_SHORT_UP, LCMUSIC_STATUS_STOP,
	 MSG_LCMPLAYER_PLAY_PLAYING},
	{KEY_NEXTSONG, KEY_TYPE_SHORT_UP, LCMUSIC_STATUS_ALL,
	 MSG_LCMPLAYER_PLAY_NEXT},
	{KEY_PREVIOUSSONG, KEY_TYPE_SHORT_UP, LCMUSIC_STATUS_ALL,
	 MSG_LCMPLAYER_PLAY_PREV},
#if CONFIG_SUPPORT_KEY_SW_DIR
	{KEY_NEXTSONG, KEY_TYPE_DOUBLE_CLICK, LCMUSIC_STATUS_ALL,
	 MSG_LCMPLAYER_PLAY_NEXT_DIR},
	{KEY_PREVIOUSSONG, KEY_TYPE_DOUBLE_CLICK, LCMUSIC_STATUS_ALL,
	 MSG_LCMPLAYER_PLAY_PREV_DIR},
#endif
	{KEY_NEXTSONG, KEY_TYPE_LONG_DOWN, LCMUSIC_STATUS_PLAYING,
	 MSG_LCMPLAYER_SEEKFW_START_CTIME},
	{KEY_PREVIOUSSONG, KEY_TYPE_LONG_DOWN, LCMUSIC_STATUS_PLAYING,
	 MSG_LCMPLAYER_SEEKBW_START_CTIME},
	{KEY_NEXTSONG, KEY_TYPE_LONG_UP, LCMUSIC_STATUS_SEEK,
	 MSG_LCMPLAYER_PLAY_SEEK_FORWARD},
	{KEY_PREVIOUSSONG, KEY_TYPE_LONG_UP, LCMUSIC_STATUS_SEEK,
	 MSG_LCMPLAYER_PLAY_SEEK_BACKWARD},
	{KEY_TBD, KEY_TYPE_LONG_UP, LCMUSIC_STATUS_ALL,
	 MSG_LCMPLAYER_SET_PLAY_MODE},
#ifdef	CONFIG_INPUT_DEV_ACTS_IRKEY
	{KEY_NUM0, KEY_TYPE_SHORT_UP, LCMUSIC_STATUS_ALL,
	 MSG_LCMPLAYER_SET_NUM0},
	{KEY_NUM1, KEY_TYPE_SHORT_UP, LCMUSIC_STATUS_ALL,
	 MSG_LCMPLAYER_SET_NUM1},
	{KEY_NUM2, KEY_TYPE_SHORT_UP, LCMUSIC_STATUS_ALL,
	 MSG_LCMPLAYER_SET_NUM2},
	{KEY_NUM3, KEY_TYPE_SHORT_UP, LCMUSIC_STATUS_ALL,
	 MSG_LCMPLAYER_SET_NUM3},
	{KEY_NUM4, KEY_TYPE_SHORT_UP, LCMUSIC_STATUS_ALL,
	 MSG_LCMPLAYER_SET_NUM4},
	{KEY_NUM5, KEY_TYPE_SHORT_UP, LCMUSIC_STATUS_ALL,
	 MSG_LCMPLAYER_SET_NUM5},
	{KEY_NUM6, KEY_TYPE_SHORT_UP, LCMUSIC_STATUS_ALL,
	 MSG_LCMPLAYER_SET_NUM6},
	{KEY_NUM7, KEY_TYPE_SHORT_UP, LCMUSIC_STATUS_ALL,
	 MSG_LCMPLAYER_SET_NUM7},
	{KEY_NUM8, KEY_TYPE_SHORT_UP, LCMUSIC_STATUS_ALL,
	 MSG_LCMPLAYER_SET_NUM8},
	{KEY_NUM9, KEY_TYPE_SHORT_UP, LCMUSIC_STATUS_ALL,
	 MSG_LCMPLAYER_SET_NUM9},
#endif

#ifdef CONFIG_BMS_UAC_APP
	{KEY_TBD, KEY_TYPE_SHORT_UP, LCMUSIC_STATUS_ALL, MSG_SWITCH_BROADCAST},
#endif

	{KEY_RESERVED, 0, 0, 0}
};

static int _lcmusic_view_proc(u8_t view_id, u8_t msg_id, u32_t msg_data)
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
#ifdef CONFIG_LED_MANAGER
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

void lcmusic_view_init(struct lcmusic_app_t *lcmusic)
{
	ui_view_info_t view_info;
	u8_t init_play_state = lcmusic_get_play_state(lcmusic->cur_dir);

	memset(&view_info, 0, sizeof(ui_view_info_t));

	view_info.view_proc = _lcmusic_view_proc;
	view_info.view_key_map = lcmusic_keymap;
	view_info.view_get_state = lcmusic_get_status;
	view_info.order = 1;
	view_info.app_id = app_manager_get_current_app();
	ui_view_create(LCMUSIC_VIEW, &view_info);
#ifdef CONFIG_SEG_LED_MANAGER
	seg_led_manager_clear_screen(LED_CLEAR_ALL);
#endif
	_lcmusic_display_icon_disk();

	if (init_play_state == LCMUSIC_STATUS_STOP)
		lcmusic_display_icon_pause();
	else
		lcmusic_display_icon_play();

	/* don't show 0 although bp_info resume fail */
	if (lcmusic->music_bp_info.track_no == 0)
		lcmusic->music_bp_info.track_no = 1;
	lcmusic_display_track_no(lcmusic->music_bp_info.track_no, 3000);
	lcmusic->filt_track_no = 1;

	SYS_LOG_INF(" ok\n");
}

void lcmusic_view_deinit(void)
{
	ui_view_delete(LCMUSIC_VIEW);

	SYS_LOG_INF("ok\n");
}

static void _lcmusic_display_icon_disk(void)
{
	struct lcmusic_app_t *lcmusic = lcmusic_get_app();

	if (!lcmusic)
		return;

#ifdef CONFIG_SEG_LED_MANAGER
	if (memcmp(lcmusic->cur_dir, "SD:", strlen("SD:")) == 0) {
		seg_led_display_icon(SLED_SD, true);
	} else if (memcmp(lcmusic->cur_dir, "USB:", strlen("USB:")) == 0) {
		seg_led_display_icon(SLED_USB, true);
	}
#endif
}

void lcmusic_display_icon_play(void)
{
#ifdef CONFIG_SEG_LED_MANAGER
	seg_led_display_icon(SLED_PLAY, true);
	seg_led_display_icon(SLED_PAUSE, false);
#endif
#ifdef CONFIG_LED_MANAGER
	led_manager_set_breath(0, NULL, OS_FOREVER, NULL);
	led_manager_set_breath(1, NULL, OS_FOREVER, NULL);
#endif
}

void lcmusic_display_icon_pause(void)
{
#ifdef CONFIG_SEG_LED_MANAGER
	seg_led_display_icon(SLED_PLAY, false);
	seg_led_display_icon(SLED_PAUSE, true);
#endif
#ifdef CONFIG_LED_MANAGER
	led_manager_set_display(0, LED_ON, OS_FOREVER, NULL);
	led_manager_set_display(1, LED_ON, OS_FOREVER, NULL);
#endif
}

void lcmusic_display_play_time(struct lcmusic_app_t *lcmusic, int seek_time)
{
#ifdef CONFIG_SEG_LED_MANAGER
	char diplay_str[5] = { 0 };

	/* remove hours */
	int temp_time =
	    (lcmusic->music_bp_info.bp_info.time_offset +
	     seek_time) / 1000 % 3600;

	if (temp_time < 0)
		temp_time = 0;
	snprintf(diplay_str, sizeof(diplay_str), "%02u%02u", temp_time / 60,
		 temp_time % 60);
	seg_led_display_string(SLED_NUMBER1, diplay_str, true);
	seg_led_display_icon(SLED_COL, true);
#endif
}

void lcmusic_display_track_no(u16_t track_no, int display_times)
{
#ifdef CONFIG_SEG_LED_MANAGER
	char diplay_str[5] = { 0 };

	snprintf(diplay_str, sizeof(diplay_str), "%04u", track_no % 10000);
	if (track_no > 9999)
		diplay_str[0] = 'A';
	seg_led_manager_clear_screen(LED_CLEAR_NUM);
	seg_led_display_icon(SLED_COL, false);
	seg_led_manager_set_timeout_event_start();
	seg_led_display_string(SLED_NUMBER1, diplay_str, true);
	seg_led_manager_set_timeout_event(display_times, NULL);
#endif
}
