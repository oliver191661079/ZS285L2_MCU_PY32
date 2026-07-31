/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <misc/util.h>
#include <soc.h>
#include <mem_manager.h>
#include <msg_manager.h>
#include <sys_monitor.h>
#include <sys_wakelock.h>
#include <ui_manager.h>
#include <app_manager.h>
#include <input_manager.h>
#include <bt_manager.h>
#include <sys_manager.h>
#ifdef CONFIG_ESD_MANAGER
#include <esd_manager.h>
#endif
#ifdef CONFIG_PLAYTTS
#include <tts_manager.h>
#endif

#include "main_app.h"
#include "app_tws.h"
#include "app_ui.h"

#ifdef CONFIG_INPUT_MANAGER


static bool main_event_key_tws_forward(u32_t key_event)
{
	bool forward = false;

	//do not forward factory reset to tws speaker.
	if ((KEY_TYPE_LONG6S|KEY_MENU) == key_event) {
		return false;
	}

	if (app_tws_status_get_enable() && app_tws_status_get_connected()) {
#ifdef ENABLE_PAWR_APP
		if ((desktop_manager_get_plugin_id() == DESKTOP_PLUGIN_ID_BMR) 
			&& (APP_TWS_ROLE_SECONDARY == app_tws_status_get_role())) {
			pawr_response_key_event(key_event);
			SYS_LOG_INF("on pawr");
			forward = true;
		}
#endif
#ifdef CONFIG_TWS
		if (bt_manager_tws_get_dev_role() == BTSRV_TWS_SLAVE)
		{
			bt_manager_tws_send_message(TWS_USER_APP_EVENT, TWS_EVENT_BT_MUSIC_KEY_CTRL, (u8_t *)&key_event, 4);
			SYS_LOG_INF("on snoop");
			forward = true;
		}
#endif
	}

	return forward;
}

void main_key_event_handle(u32_t key_event)
{
	/**input event means esd proecess finished*/
#ifdef CONFIG_ESD_MANAGER
	if (esd_manager_check_esd()) {
#ifdef CONFIG_PLAYTTS
		tts_manager_unlock();
#endif
		esd_manager_reset_finished();
	}
#endif

	sys_wake_lock(WAKELOCK_INPUT);

#ifdef CONFIG_PLAYTTS
	if ((key_event & KEY_TYPE_DOUBLE_CLICK) == KEY_TYPE_DOUBLE_CLICK
	    || (key_event & KEY_TYPE_TRIPLE_CLICK) == KEY_TYPE_TRIPLE_CLICK
	    || (key_event & KEY_TYPE_SHORT_UP) == KEY_TYPE_SHORT_UP
	    || (key_event & KEY_TYPE_LONG_DOWN) == KEY_TYPE_LONG_DOWN) {
		//tts_manager_stop(NULL);
	}
#endif

	SYS_LOG_INF("key 0x%08x\n", key_event);

	/**drop fisrt key event when resume*/
	if (system_wakeup_time() > 400) {
#ifdef CONFIG_DATA_ANALY
		extern u8_t app_count_key_press_cnt(u32_t key_event);
		app_count_key_press_cnt(key_event);
#endif
		if (!main_event_key_tws_forward(key_event)) {
#ifdef CONFIG_UI_MANAGER
			ui_manager_dispatch_key_event(key_event);
#endif
		}
	}
	else
	{
		SYS_LOG_INF("drop key workup %d \n", system_wakeup_time());
	}

	sys_wake_unlock(WAKELOCK_INPUT);
}
#endif


