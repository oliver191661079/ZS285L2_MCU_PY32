/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <mem_manager.h>
#include <msg_manager.h>
#ifdef CONFIG_LED_MANAGER
#include <led_manager.h>
#endif
#include <ui_manager.h>
#include <power_manager.h>
#include <tts_manager.h>
#include <input_manager.h>
#include <property_manager.h>
#include "app_defines.h"
#include "app_ui.h"
#include "main_app.h"
#include <sys_manager.h>
#include <app_tws.h>
#if defined(CONFIG_SYSTEM_APP_PY32_UART)
#include "../system_app/system_app.h"
#endif

const ui_key_map_t common_keymap[] = {
	{KEY_MENU, KEY_TYPE_SHORT_UP, 1, MSG_KEY_SWITCH_APP},
	{KEY_MENU, KEY_TYPE_LONG6S, 1, MSG_FACTORY_DEFAULT},
	{KEY_POWER, KEY_TYPE_LONG_DOWN, 1, MSG_KEY_POWER_OFF},
	/* 开关机短按：未连接时进配对（btmusic 内另有已连接=播放/暂停逻辑） */
	{KEY_POWER, KEY_TYPE_SHORT_UP, 1, MSG_ENTER_PAIRING_MODE},
	{KEY_BT, KEY_TYPE_SHORT_UP, 1, MSG_ENTER_PAIRING_MODE}, //phone pair
	/* TWS 无按键控制：默认开启、组对超时自动关闭（见 btmusic _btmusic_init / app_tws.c） */
	//{KEY_BT, KEY_TYPE_LONG_DOWN, 1, MSG_BT_PLAY_TWS_PAIR}, //tws pair/unpair
	//{KEY_COMBO_VOL, KEY_TYPE_SHORT_UP, 1, MSG_DEMO_SWITCH},

	{KEY_POWER, KEY_TYPE_DOUBLE_CLICK, 1, MSG_BT_CALL_LAST_NO},
	//{KEY_TBD, KEY_TYPE_LONG_DOWN, 1, MSG_BT_PLAY_DISCONNECT_TWS_PAIR},
	{KEY_TBD, KEY_TYPE_DOUBLE_CLICK, 1, MSG_BT_SIRI_START},
	{KEY_TBD, KEY_TYPE_LONG_DOWN, 1, MSG_START_DISK_OTA},
	{KEY_RESERVED, 0, 0, 0},
};

int main_app_ui_event(int event)
{
	SYS_LOG_INF(" %d\n", event);

	ui_message_send_async(MAIN_VIEW, MSG_VIEW_PAINT, event);

	return 0;
}

void main_app_volume_show(struct app_msg *msg)
{
#ifdef CONFIG_SEG_LED_MANAGER
	int volume_value = msg->value;
	u8_t volume[5];

	snprintf(volume, sizeof(volume), "U %02u", volume_value);
	seg_led_manager_set_timeout_event_start();
	seg_led_display_icon(SLED_P1, false);
	seg_led_display_icon(SLED_COL, false);
	seg_led_display_string(SLED_NUMBER1, volume, true);
	seg_led_manager_set_timeout_event(2000, NULL);
#endif
#ifdef CONFIG_LED_MANAGER
	if (msg->cmd) {
		led_manager_set_timeout_event_start();
		led_manager_set_blink(0, 100, 50, OS_FOREVER,
				      LED_START_STATE_ON, NULL);
		led_manager_set_blink(1, 100, 50, OS_FOREVER,
				      LED_START_STATE_OFF, NULL);
		/*blink 3 times */
		led_manager_set_timeout_event(100 * 3, NULL);
	}
#endif
}

static void main_app_view_deal(u32_t ui_event)
{

	switch (ui_event) {
	case UI_EVENT_PLAY_START:
#ifdef CONFIG_SEG_LED_MANAGER
		seg_led_display_icon(SLED_PAUSE, false);
		seg_led_display_icon(SLED_PLAY, true);
#endif
		break;
	case UI_EVENT_PLAY_PAUSE:
#ifdef CONFIG_SEG_LED_MANAGER
		seg_led_display_icon(SLED_PAUSE, true);
		seg_led_display_icon(SLED_PLAY, false);
#endif
		break;
	case UI_EVENT_POWER_OFF:
#ifdef CONFIG_PLAYTTS
		/* make sure powerdown tts */
		tts_manager_wait_finished(true);
#endif
#ifdef ENABLE_PAWR_APP
		if (app_tws_status_get_enable() && app_tws_status_get_connected()) {
			extern int app_tws_pawr_adv_set_cmd(u8_t cmd);
			u8_t mode, role;
			mode = app_tws_status_get_mode();
			role = app_tws_status_get_role();
			if ((mode == APP_TWS_MODE_BIS) && (role == APP_TWS_ROLE_PRIMARY)) {
				app_tws_pawr_adv_set_cmd(PAWR_CMD_POWER_OFF);
				// Wait reply from secondary
				app_tws_wait_pawr_cmd_reply(PAWR_CMD_POWER_OFF);
				//k_sleep(100);
				break;
			}
		}
#endif
		sys_event_send_message(MSG_POWER_OFF);
		break;
	case UI_EVENT_OTA_FINISHED_REBOOT:
#ifdef CONFIG_PLAYTTS
		/* make sure powerdown tts */
		tts_manager_wait_finished(true);
#else
		sys_event_send_message(MSG_REBOOT);
#endif
		//reboot will be handled after poweroff tts is done.
		ui_event = UI_EVENT_POWER_OFF;
		break;
	case UI_EVENT_NO_POWER:
		sys_event_send_message(MSG_NO_POWER);
		break;
	case UI_EVENT_WAIT_CONNECTION:
#ifdef CONFIG_LED_MANAGER
		led_manager_set_blink(0, 500, 200, OS_FOREVER, LED_START_STATE_OFF, NULL);
#endif
		break;
	case UI_EVENT_CONNECT_SUCCESS:
#ifdef CONFIG_LED_MANAGER
		led_manager_set_display(0, LED_ON, OS_FOREVER, NULL);
#endif
#ifdef CONFIG_PROPERTY
		property_flush_req_deal();
#endif
		break;
	case UI_EVENT_BT_DISCONNECT:
	case UI_EVENT_BT_UNLINKED:
#ifdef CONFIG_LED_MANAGER
		led_manager_set_blink(0, 2000, 200, OS_FOREVER, LED_START_STATE_OFF, NULL);
#endif
#ifdef CONFIG_PROPERTY
		property_flush_req_deal();
#endif
		break;
	case UI_EVENT_TWS_TEAM_SUCCESS:
	case UI_EVENT_TWS_DISCONNECTED:
	case UI_EVENT_CLEAR_PAIRED_LIST:
	case UI_EVENT_SECOND_DEVICE_CONNECT_SUCCESS:
#ifdef CONFIG_PROPERTY
		property_flush_req_deal();
#endif
		break;
	}

#ifdef CONFIG_PLAYTTS
#ifdef CONFIG_TWS_UI_EVENT_SYNC
	system_do_event_notify(ui_event);
#else
#if defined(CONFIG_SYSTEM_APP_PY32_UART)
	/* PY32 无 UART 数据时不播本地 TTS（poweron/bt_music 等） */
	if (!system_app_py32_host_is_alive()) {
		/* skip */
	} else
#endif
	{
		static uint32_t last_time = 0;

		if (ui_event == UI_EVENT_MAX_VOLUME) {
			if (last_time && k_uptime_get_32() - last_time < 1100) {
				return;
			}
			last_time = k_uptime_get_32();
		}
		tts_manager_process_ui_event(ui_event);
	}
#endif
#endif

}

static int main_app_view_proc(u8_t view_id, u8_t msg_id, u32_t ui_event)
{
	SYS_LOG_INF(" msg_id %d ui_event %d\n", msg_id, ui_event);
	switch (msg_id) {
	case MSG_VIEW_CREATE:
		main_app_view_deal(UI_EVENT_POWER_ON);
		break;
	case MSG_VIEW_PAINT:
		main_app_view_deal(ui_event);
		break;
	case MSG_VIEW_DELETE:
#ifdef CONFIG_PLAYTTS
		tts_manager_wait_finished(true);
#endif
		break;
	default:
		break;
	}
	return 0;
}

void main_app_view_init(void)
{
	ui_view_info_t view_info;

	memset(&view_info, 0, sizeof(ui_view_info_t));

	view_info.view_proc = main_app_view_proc;
	view_info.view_key_map = common_keymap;
	view_info.view_get_state = NULL;
	view_info.order = 0;
	view_info.app_id = APP_ID_MAIN;

#ifdef CONFIG_UI_MANAGER
	ui_view_create(MAIN_VIEW, &view_info);
#endif

#ifdef CONFIG_LED_MANAGER
	led_manager_set_breath(1, NULL, OS_FOREVER, NULL);
	led_manager_set_blink(0, 1000, 200, OS_FOREVER, LED_START_STATE_OFF, NULL);
#endif

	SYS_LOG_INF(" ok\n");
}

void main_app_view_exit(void)
{
#ifdef CONFIG_UI_MANAGER
	ui_view_delete(MAIN_VIEW);
#endif
}
