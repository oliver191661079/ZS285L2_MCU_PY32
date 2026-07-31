/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file system app main
 */
#include <mem_manager.h>
#include <msg_manager.h>
#include <fw_version.h>
#include <sys_event.h>
#include "app_ui.h"
#include <bt_manager.h>
#include <hotplug_manager.h>
#include <input_manager.h>
#include <thread_timer.h>
#include <stream.h>
#include <property_manager.h>
#include <usb/usb_device.h>
#include <usb/class/usb_msc.h>
#include <soc.h>
#include <ui_manager.h>
#include <stub_hal.h>
#ifdef CONFIG_PLAYTTS
#include "tts_manager.h"
#endif

#ifdef CONFIG_TOOL
#include "tool_app.h"
#endif

#ifdef CONFIG_DATA_ANALY
#include <data_analy.h>
#endif

#include "desktop_manager.h"

#include "charger.h"

int charger_mode_check(void)
{
	struct app_msg msg = {0};
	struct app_msg charger_mode_msg = {0};
	int result = 0;
	bool terminaltion = false;

	if (!sys_pm_get_power_5v_status()) {
		charger_mode_msg.type = MSG_CHARGER_MODE;
		charger_mode_msg.cmd = 0;

		send_async_msg(CONFIG_SYS_APP_NAME, &charger_mode_msg);
		return 0;
	}

	charger_view_init();

#ifdef CONFIG_DATA_ANALY
	system_data_analy_init(0);
#endif


	while (!terminaltion) {
		if (receive_msg(&msg, thread_timer_next_timeout())) {
			SYS_LOG_INF("type %d, cmd %d, value 0x%x\n", msg.type, msg.cmd, msg.value);
			switch (msg.type) {
			case MSG_KEY_INPUT:
				terminaltion = true;

				charger_mode_msg.type = MSG_CHARGER_MODE;
				charger_mode_msg.cmd = 0;

				send_async_msg(CONFIG_SYS_APP_NAME, &charger_mode_msg);
				break;
		#ifdef CONFIG_UI_MANAGER
			case MSG_UI_EVENT:
				ui_message_dispatch(msg.sender, msg.cmd, msg.value);
				break;
		#endif
			case MSG_HOTPLUG_EVENT:

				if (msg.cmd == HOTPLUG_CHARGER){
					if (msg.value == HOTPLUG_OUT) {
						terminaltion = true;
						#ifdef CONFIG_DATA_ANALY
							data_analy_exit();
						#endif
						sys_pm_poweroff();
					}
				} else if (msg.cmd == HOTPLUG_USB_DEVICE) {
#if (defined(CONFIG_USOUND_APP))
					if (msg.value == HOTPLUG_IN) {
						desktop_manager_add(DESKTOP_PLUGIN_ID_UAC);
					} else if (msg.value == HOTPLUG_OUT) {
						desktop_manager_del(DESKTOP_PLUGIN_ID_UAC);
					}
#endif
				} else if (msg.cmd == HOTPLUG_LINEIN) {
#ifdef CONFIG_LINE_IN_APP
					if (msg.value == HOTPLUG_IN) {
						desktop_manager_add(DESKTOP_PLUGIN_ID_LINE_IN);
					} else if (msg.value == HOTPLUG_OUT) {
						desktop_manager_del(DESKTOP_PLUGIN_ID_LINE_IN);
					}
#endif
				} else if (msg.cmd == HOTPLUG_SDCARD) {
#ifdef CONFIG_LCMUSIC_APP
					if (msg.value == HOTPLUG_IN) {
						desktop_manager_add(DESKTOP_PLUGIN_ID_SDCARD_PLAYER);
					} else if (msg.value == HOTPLUG_OUT) {
						desktop_manager_del(DESKTOP_PLUGIN_ID_SDCARD_PLAYER);
					}
#endif
				} else if (msg.cmd == HOTPLUG_USB_HOST) {
#ifdef CONFIG_LCMUSIC_APP
					if (msg.value == HOTPLUG_IN) {
						desktop_manager_add(DESKTOP_PLUGIN_ID_USB_PLAYER);
					} else if (msg.value == HOTPLUG_OUT) {
						desktop_manager_del(DESKTOP_PLUGIN_ID_USB_PLAYER);
					}
#endif
				}
				break;
			default:
				SYS_LOG_ERR("error type: 0x%x! \n", msg.type);
				continue;
			}
			if (msg.callback != NULL)
				msg.callback(&msg, result, NULL);
		}
		thread_timer_handle_expired();
	}

	charger_view_deinit();

	return 0;

}
