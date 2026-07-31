/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <mem_manager.h>
#include <msg_manager.h>
#include <hotplug_manager.h>
#include <sys_manager.h>
#include <sys_event.h>
#include <soc_pm.h>
#include <app_launch.h>
#include <app_manager.h>
#ifdef CONFIG_USB_MASS_STORAGE
#ifdef CONFIG_SYS_WAKELOCK
#include <sys_wakelock.h>
#endif
#ifdef CONFIG_FS_MANAGER
#include <fs_manager.h>
#endif
#include <usb/class/usb_msc.h>
#endif
#ifdef CONFIG_BLUETOOTH
#include <bt_manager.h>
#endif
#include <input_manager.h>
#include <input_dev.h>
#include "app_ui.h"
#include "app_defines.h"
#include "main_app.h"
#include "audio_system.h"
#ifdef CONFIG_BT_SELF_APP
#include "selfapp_api.h"
#endif
#include "broadcast.h"
#include "run_mode.h"
#include <app_tws.h>
#ifdef CONFIG_ACT_EVENT
#include <app_act_event_id.h>
#include <logging/log_core.h>
LOG_MODULE_DECLARE(main, CONFIG_ACT_EVENT_APP_COMPILE_LEVEL);
#endif

#ifdef CONFIG_USB_UART_CONSOLE
extern void trace_set_usb_console_active(u8_t active);
extern int usb_cdc_acm_init(struct device *dev);
extern int usb_cdc_acm_exit(void);
#endif

#ifdef CONFIG_GFP_PROFILE
void account_key_clear(void);
void personalized_name_clear(void);
#endif

void main_input_event_handle(struct app_msg *msg)
{
	SYS_LOG_INF("cmd %d\n", msg->cmd);
#ifdef CONFIG_AURACAST
	struct app_msg app_msg;
#endif

#ifdef CONFIG_BT_CONTROLER_BQB_SYS
	//skip input events when in bqb mode
	if (bt_manager_get_bqb_mode() > 0) {
		if(MSG_KEY_POWER_OFF != msg->cmd) {
			SYS_LOG_INF("Skip %d in bqb", msg->cmd);
			desktop_manager_proc_app_msg(msg);
			return;
		}
	}
#endif

	//skip input events when in ota mode
	if (desktop_manager_get_plugin_id() == DESKTOP_PLUGIN_ID_OTA) {
		if(MSG_KEY_POWER_OFF != msg->cmd) {
			SYS_LOG_INF("Skip %d in ota", msg->cmd);
			desktop_manager_proc_app_msg(msg);
			return;
		}
	}

#ifdef CONFIG_APP_TWS
#ifdef CONFIG_APP_TWS_SNOOP
	int role;
	role = bt_manager_tws_get_dev_role();
#endif
#endif

	switch (msg->cmd) {
	case MSG_INPUT_NULL:
		break;
	case MSG_KEY_POWER_OFF:
		sys_event_notify(SYS_EVENT_POWER_OFF);
		break;
	case MSG_FACTORY_DEFAULT:
		SYS_LOG_INF("factory default\n");
#ifdef CONFIG_BT_MANAGER
		bt_manager_end_pair_mode();
		bt_manager_set_user_visual(1,0,0,BTSRV_SCAN_MODE_DEFAULT_INQUIRY_PAGE);
		bt_manager_auto_reconnect_stop();
		bt_manager_clear_bt_info();
#endif
#ifdef CONFIG_APP_TWS
		app_tws_storage_clear();
#endif
		audio_system_clear_volume();
#ifdef CONFIG_GFP_PROFILE
		account_key_clear();
		personalized_name_clear();
#endif
#ifdef CONFIG_BT_SELF_APP
		selfapp_config_reset();
#endif
		sys_event_notify(SYS_EVENT_POWER_OFF);
		system_restore_factory_config();
		break;
	case MSG_KEY_SWITCH_APP:
		if (bt_manager_tws_get_dev_role() != BTSRV_TWS_SLAVE) {
			desktop_manager_switch(0, DESKTOP_SWITCH_NEXT);
		}
		break;
	case MSG_ENTER_PAIRING_MODE:
#ifdef CONFIG_APP_TWS
#ifdef CONFIG_APP_TWS_SNOOP
		if (role == BTSRV_TWS_SLAVE) {
			break;
		}
		if (role == BTSRV_TWS_MASTER) {
			if (bt_manager_get_connected_dev_num() > 0) {
				bt_manager_br_disconnect_all_phone_device();
			}
		}
#endif
#endif
		sys_event_notify(SYS_EVENT_ENTER_PAIR_MODE);
		bt_manager_set_user_visual(false,false,false,0);
		bt_manager_enter_pair_mode();
#ifdef CONFIG_AURACAST
		if (!app_tws_status_get_enable() ||
			DESKTOP_PLUGIN_ID_BMR == desktop_manager_get_plugin_id()) {
			app_msg.type = MSG_INPUT_EVENT;
			app_msg.cmd = MSG_AURACAST_EXIT;
			desktop_manager_proc_app_msg(&app_msg);
		} else {
			app_msg.type = MSG_INPUT_EVENT;
			app_msg.cmd = MSG_TWS_SWITCH;
			desktop_manager_proc_app_msg(&app_msg);
		}
#endif
		break;
#ifdef CONFIG_APP_TWS
	case MSG_BT_PLAY_TWS_PAIR:
		app_tws_manual_switch();
		break;
#endif
#ifdef CONFIG_BT_MANAGER
	case MSG_BT_PLAY_CLEAR_LIST:
#ifdef CONFIG_APP_TWS
		app_tws_storage_clear();
#endif
		bt_manager_clear_list(BTSRV_DEVICE_ALL);
		break;
#ifdef CONFIG_BT_HFP_HF
	case MSG_BT_SIRI_START:
		if ((desktop_manager_get_plugin_id() == DESKTOP_PLUGIN_ID_BR_CALL
			|| desktop_manager_get_plugin_id() == DESKTOP_PLUGIN_ID_LE_CALL)) {
			break;
		}
		bt_manager_hfp_start_siri();
		break;
	case MSG_BT_CALL_LAST_NO:
		if ((desktop_manager_get_plugin_id() == DESKTOP_PLUGIN_ID_BR_CALL
			|| desktop_manager_get_plugin_id() == DESKTOP_PLUGIN_ID_LE_CALL)) {
			break;
		}
		bt_manager_hfp_dial_last_number();
		break;
#endif
#endif
#ifdef CONFIG_BT_HID
	case MSG_BT_HID_START:
		bt_manager_hid_take_photo();
		/* bt_manager_hid_key_func(KEY_FUNC_HID_CUSTOM_KEY); */
		break;
#endif
#ifdef CONFIG_AURACAST
	case MSG_AURACAST_ENTER:
	case MSG_AURACAST_EXIT:
		SYS_LOG_INF("plugin=%d\n", desktop_manager_get_plugin_id());
		desktop_manager_proc_app_msg(msg);
		break;
#endif

#ifdef CONFIG_OTA_BACKEND_DISK
	case MSG_START_DISK_OTA:{
		u8_t ota_dev = 0;
#ifdef CONFIG_OTA_BACKEND_SDCARD
		if (hotplug_manager_get_state(HOTPLUG_SDCARD) == HOTPLUG_IN) {
			ota_dev ++;
		}
#endif
#ifdef CONFIG_OTA_BACKEND_UHOST
		if (hotplug_manager_get_state(HOTPLUG_USB_HOST) == HOTPLUG_IN) {
			ota_dev ++;
		}
#endif
		if(ota_dev){
			desktop_manager_add(DESKTOP_PLUGIN_ID_DISK_OTA_PRE);
			desktop_manager_enter_app(DESKTOP_PLUGIN_ID_DISK_OTA_PRE);
		}
		break;
	}
#endif

	case MSG_ENTER_DEMO:
		//app_switch(APP_ID_DEMO, APP_SWITCH_CURR, true);
		desktop_manager_add(DESKTOP_PLUGIN_ID_DEMO);
		desktop_manager_enter_app(DESKTOP_PLUGIN_ID_DEMO);
		break;
	case MSG_DEMO_SWITCH:
	{
		SYS_LOG_INF("demo switch\n");

		int run_mode = run_mode_get();

		if (run_mode == RUN_MODE_DEMO)
		{
			run_mode_set(RUN_MODE_NORMAL);

			sys_event_notify(SYS_EVENT_POWER_OFF);
		}
		else if (run_mode == RUN_MODE_NORMAL)
		{
			run_mode_set(RUN_MODE_DEMO);

			sys_event_notify(SYS_EVENT_POWER_OFF);
		}
		break;
	}
#ifdef CONFIG_APP_TWS
	case MSG_TWS_MODE_SWITCH:
		app_tws_stop_pawr_cmd_reply();
		app_tws_mode_switch((msg->value >> 8) & 0xFF, msg->value & 0xFF);
		break;
	case MSG_TWS_POWER_OFF_REPLY:
		app_tws_stop_pawr_cmd_reply();
		sys_event_send_message(MSG_POWER_OFF);
#endif
	default:
		desktop_manager_proc_app_msg(msg);
		break;
	}
}

void main_hotplug_event_handle(struct app_msg *msg)
{
	SYS_LOG_INF("type %d, %d\n", msg->cmd, msg->value);
	SYS_EVENT_INF(EVENT_MAIN_HOTPLUG, msg->cmd, msg->value);

	if(app_tws_status_get_connected()) {
		if(app_tws_status_get_role() == APP_TWS_ROLE_SECONDARY) {
			SYS_LOG_INF("Skip in TWS secondary");
			return;
		}
	}

	switch (msg->cmd)
	{
	case HOTPLUG_LINEIN:
#ifdef CONFIG_LINE_IN_APP
		// APP_ID_LINEIN, APP_ID_AUXTWS
		if (msg->value == HOTPLUG_IN) {
			desktop_manager_add(DESKTOP_PLUGIN_ID_LINE_IN);
			desktop_manager_enter_app(DESKTOP_PLUGIN_ID_LINE_IN);
		} else if (msg->value == HOTPLUG_OUT) {
			desktop_manager_exit_app(DESKTOP_PLUGIN_ID_LINE_IN);
			desktop_manager_del(DESKTOP_PLUGIN_ID_LINE_IN);
		}
#endif
		break;
	case HOTPLUG_USB_DEVICE:
		if (msg->value == HOTPLUG_IN) {
#ifdef CONFIG_USB_UART_CONSOLE
			if (desktop_manager_get_plugin_id() != DESKTOP_PLUGIN_ID_DEMO) {
#ifdef CONFIG_OTA_BACKEND_UART_CDC
				struct device *cdc_dev = device_get_binding(CONFIG_CDC_ACM_PORT_NAME);
				usb_cdc_acm_init(cdc_dev);
#else
				trace_set_usb_console_active(true);
#endif
			} else {
				desktop_manager_proc_app_msg(msg);
			}
#else
#if (defined(CONFIG_USOUND_APP))
			if (system_boot_time() > 5000) {
				desktop_manager_add(DESKTOP_PLUGIN_ID_UAC);
				desktop_manager_enter_app(DESKTOP_PLUGIN_ID_UAC);
			} else {
				desktop_manager_add(DESKTOP_PLUGIN_ID_UAC);
			}
#endif
#if (defined(CONFIG_DEMO_APP))
			if (desktop_manager_get_plugin_id() == DESKTOP_PLUGIN_ID_DEMO) {
				desktop_manager_proc_app_msg(msg);
			}
#endif
#endif
		}
		else if (msg->value == HOTPLUG_OUT)
		{
#ifdef CONFIG_USB_UART_CONSOLE
			if (desktop_manager_get_plugin_id() != DESKTOP_PLUGIN_ID_DEMO) {
#ifdef CONFIG_OTA_BACKEND_UART_CDC
				usb_cdc_acm_exit();
#else
				trace_set_usb_console_active(false);
#endif
			} else {
				desktop_manager_proc_app_msg(msg);
			}
#else
#if (defined(CONFIG_USOUND_APP))
			desktop_manager_exit_app(DESKTOP_PLUGIN_ID_UAC);
			desktop_manager_del(DESKTOP_PLUGIN_ID_UAC);
#endif
#if (defined(CONFIG_DEMO_APP))
			if (desktop_manager_get_plugin_id() == DESKTOP_PLUGIN_ID_DEMO) {
				desktop_manager_proc_app_msg(msg);
			}
#endif
#endif
		}

		break;
	case HOTPLUG_CHARGER:
		if (msg->value == HOTPLUG_IN) {
		} else if (msg->value == HOTPLUG_OUT) {
#ifdef CONFIG_DEMO_APP
			if (run_mode_is_demo()) {
				sys_event_notify(SYS_EVENT_POWER_OFF);
				break;
			}
#endif
		}
		break;
	case HOTPLUG_SDCARD:
		if (msg->value == HOTPLUG_IN) {
#ifndef CONFIG_SOUNDBAR_LCMUSIC
#ifdef CONFIG_LCMUSIC_APP
			desktop_manager_add(DESKTOP_PLUGIN_ID_SDCARD_PLAYER);
			desktop_manager_enter_app(DESKTOP_PLUGIN_ID_SDCARD_PLAYER);
#endif
#endif
		} else if (msg->value == HOTPLUG_OUT) {
#ifndef CONFIG_SOUNDBAR_LCMUSIC
#ifdef CONFIG_LCMUSIC_APP
			desktop_manager_exit_app(DESKTOP_PLUGIN_ID_SDCARD_PLAYER);
			desktop_manager_del(DESKTOP_PLUGIN_ID_SDCARD_PLAYER);
#endif
#endif
		}
		break;
	case HOTPLUG_USB_HOST:
		if (msg->value == HOTPLUG_IN) {
#ifndef CONFIG_SOUNDBAR_LCMUSIC
#ifdef CONFIG_LCMUSIC_APP
			SYS_LOG_INF("bootup %d\n", os_uptime_get_32());
			desktop_manager_add(DESKTOP_PLUGIN_ID_USB_PLAYER);
			//skip app switch at bootup.
			if (3000 < os_uptime_get_32()) {
				desktop_manager_enter_app(DESKTOP_PLUGIN_ID_USB_PLAYER);
			}
#endif
#endif
		} else if (msg->value == HOTPLUG_OUT) {
#ifndef CONFIG_SOUNDBAR_LCMUSIC
#ifdef CONFIG_LCMUSIC_APP
			desktop_manager_exit_app(DESKTOP_PLUGIN_ID_USB_PLAYER);
			desktop_manager_del(DESKTOP_PLUGIN_ID_USB_PLAYER);
#endif
#endif
		}
		break;
	default:
		SYS_LOG_WRN("skip type: %d!\n", msg->cmd);
		break;
	}
}

int main_bt_event_handle(struct app_msg *msg)
{
	int consumed = 0;

	SYS_LOG_INF("cmd %d\n", msg->cmd);
#ifdef CONFIG_APP_TWS
#ifdef CONFIG_APP_TWS_SNOOP
	int role;
	role = bt_manager_tws_get_dev_role();
#endif
#endif

	switch (msg->cmd) {
#ifdef CONFIG_APP_TWS
#ifdef CONFIG_APP_TWS_SNOOP
	case BT_TWS_CONNECTION_EVENT:
		app_tws_on_snoop_connect(true);
		consumed = 1;
		break;
	case BT_TWS_DISCONNECTION_EVENT:
		app_tws_on_snoop_connect(false);
		if (desktop_manager_get_plugin_id() != DESKTOP_PLUGIN_ID_SDCARD_PLAYER
			 && desktop_manager_get_plugin_id() != DESKTOP_PLUGIN_ID_USB_PLAYER
			 && desktop_manager_get_plugin_id() != DESKTOP_PLUGIN_ID_NOR_PLAYER) {
			consumed = 1;
		}
		break;
	case BT_CONNECTED:
		if (role == BTSRV_TWS_MASTER) {
			bt_manager_set_user_visual(true,false,false,0);
		}
		consumed = 1;
		break;
	case BT_DISCONNECTED:
		if (role == BTSRV_TWS_MASTER || role == BTSRV_TWS_TEMP_MASTER) {
			if (0 == bt_manager_get_connected_dev_num()) {
				bt_manager_set_user_visual(false,false,false,0);
			}
		}
		consumed = 1;
		break;
#endif
#ifdef ENABLE_PAWR_APP
	case BT_PAWR_SYNCED:
		if (1 == app_tws_on_pawr_primary_sync(true, msg->ptr)) {
			system_app_switch_auracast(true);
		}
		consumed = 1;
		break;
	case BT_PAWR_SYNC_LOST:
		if (1 == app_tws_on_pawr_primary_sync(false, msg->ptr)) {
			system_app_switch_auracast(false);
		}
		consumed = 1;
		break;
	case BT_PAWR_SCAN_SYNCED:
		app_tws_on_pawr_secondary_sync(true, msg->ptr);
		consumed = 1;
		break;
	case BT_PAWR_SCAN_SYNC_LOST:
		if(app_tws_status_get_connected()) {
			app_tws_on_pawr_secondary_sync(false, NULL);
		}
		consumed = 1;
		break;
	case BT_PAWR_SCAN_SYNCING:
		app_tws_on_pawr_secondary_syncing();
		consumed = 1;
		break;
#endif
#endif
	default:
		break;
	}

	return consumed;
}
