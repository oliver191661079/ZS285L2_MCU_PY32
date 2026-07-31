/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file system app launcher
 */

#include <os_common_api.h>
#include <string.h>
#include <mem_manager.h>
#include <msg_manager.h>
#include <app_manager.h>
#include <hotplug_manager.h>
#include <led_manager.h>
#include "app_defines.h"
#include <app_launch.h>
#include "audio_system.h"
#include "desktop_manager.h"
#include <bt_manager.h>
#include <property_manager.h>

#ifdef CONFIG_ESD_MANAGER
#include "esd_manager.h"
#endif
#ifdef CONFIG_STUB_DEV_USB
#include <usb/class/usb_stub.h>
#endif

#ifdef CONFIG_BT_SELF_APP
#include "selfapp_api.h"
#endif

#include "app_ui.h"
#include "run_mode.h"

#if defined(CONFIG_SYS_LOG)
#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "launcher"
#include <logging/sys_log.h>
#endif

#ifdef CONFIG_ACT_EVENT
#include <app_act_event_id.h>
#include <logging/log_core.h>
LOG_MODULE_DECLARE(main, CONFIG_ACT_EVENT_APP_COMPILE_LEVEL);
#endif

static struct k_mutex mutex;


/*
 * app id switch list
 */
#define app_id_list {\
						APP_ID_DESKTOP \
					}


int system_app_launch_init(void)
{
	int def_desktop_id = DESKTOP_PLUGIN_ID_BR_MUSIC;
#ifdef CONFIG_BT_CONTROLER_BQB_SYS
	int btdrv_get_bqb_mode(void);
	if (btdrv_get_bqb_mode() > 0)
		def_desktop_id = DESKTOP_PLUGIN_ID_LINE_IN;
#endif

	k_mutex_init(&mutex);

	if (run_mode_is_demo()) {
		def_desktop_id = DESKTOP_PLUGIN_ID_DEMO;
	}
#ifdef CONFIG_SRRC_TEST
	int mode = property_get_int("SRRC_BMR", 0);
	if(mode) {
		def_desktop_id = DESKTOP_PLUGIN_ID_BMR;
	} else {
		def_desktop_id = DESKTOP_PLUGIN_ID_LINE_IN;
	}
#ifdef CONFIG_SRRC_TEST_BMR
	def_desktop_id = DESKTOP_PLUGIN_ID_BMR;
#endif
#endif


	desktop_manager_init(def_desktop_id);

#ifdef CONFIG_LINE_IN_APP
	if (hotplug_manager_get_state(HOTPLUG_LINEIN) == HOTPLUG_IN) {
		desktop_manager_add(DESKTOP_PLUGIN_ID_LINE_IN);
	}
#endif
#ifdef CONFIG_USOUND_APP
	if (hotplug_manager_get_state(HOTPLUG_USB_DEVICE) == HOTPLUG_IN) {
		desktop_manager_add(DESKTOP_PLUGIN_ID_UAC);
	}
#endif
#ifdef CONFIG_LCMUSIC_APP
	if (hotplug_manager_get_state(HOTPLUG_SDCARD) == HOTPLUG_IN) {
		desktop_manager_add(DESKTOP_PLUGIN_ID_SDCARD_PLAYER);
	}
	if (hotplug_manager_get_state(HOTPLUG_USB_HOST) == HOTPLUG_IN) {
		desktop_manager_add(DESKTOP_PLUGIN_ID_USB_PLAYER);
	}
#endif

#ifdef CONFIG_SPDIF_IN_APP
	desktop_manager_add(DESKTOP_PLUGIN_ID_SPDIFRX_IN);
#endif
#ifdef CONFIG_I2SRX_IN_APP
	desktop_manager_add(DESKTOP_PLUGIN_ID_I2SRX_IN);
#endif

	return 0;
}

bool system_app_launch_add(u8_t plugin_id)
{
	SYS_LOG_INF("%d\n", plugin_id);
	struct app_msg  msg = {0};

	msg.type = MSG_SWITCH_APP;
	msg.cmd = APP_SWITCH_ADD;
	msg.value = plugin_id;

	if(send_async_msg(CONFIG_FRONT_APP_NAME, &msg) == false){
		SYS_LOG_INF("%d failed\n", plugin_id);
		return false;
	}

	return true;
}

bool system_app_launch_del(u8_t plugin_id)
{

	SYS_LOG_INF("%d", plugin_id);
	struct app_msg	msg = {0};
	
	msg.type = MSG_SWITCH_APP;
	msg.cmd = APP_SWITCH_DEL;
	msg.value = plugin_id;
	
	if(send_async_msg(CONFIG_FRONT_APP_NAME, &msg) == false){
		SYS_LOG_INF("%d failed\n", plugin_id);
		return false;
	}
	
	return true;
}

bool system_app_launch_switch(u8_t from, u8_t to)
{
	SYS_LOG_INF("%d->%d", from, to);

	struct app_msg	msg = {0};
	
	msg.type = MSG_SWITCH_APP;
	msg.cmd = APP_SWITCH_REPLACE;
	msg.value = to|(from<<8);
	
	if(send_async_msg(CONFIG_FRONT_APP_NAME, &msg) == false){
		SYS_LOG_INF("failed\n");
		return false;
	}
	return true;
}

#ifdef CONFIG_AURACAST

#if !defined(CONFIG_BT_LE_AUDIO)
#define BIS_CIS_RESTRICT_ENABLE
#endif

#if defined(BIS_CIS_RESTRICT_ENABLE)
static uint8_t bis_cis_restrict = 0;
#endif

static int g_auracast_mode = 0; // [0,1,2] 0-Normal, 1-BMS, 2-BMR

/*
mode [0,1,2] : 0-normal, 1-bms, 2-bmr
*/
void system_app_set_auracast_mode(int mode)
{
	SYS_LOG_INF("mode %d->%d", g_auracast_mode, mode);

	if(g_auracast_mode == mode){
		return;
	}
	int last_mode = g_auracast_mode;

	k_mutex_lock(&mutex, K_FOREVER);
	g_auracast_mode = mode;
	k_mutex_unlock(&mutex);
	
	SYS_EVENT_INF(EVENT_MAIN_AURACAST_MODE, g_auracast_mode);

	if(mode == 0) {
#if defined(BIS_CIS_RESTRICT_ENABLE)
		if(bis_cis_restrict){
			bis_cis_restrict = 0;
			bt_manager_audio_le_resume_adv();
		}
#endif
		bt_manager_audio_switch_multi_point(1);
		sys_event_notify(SYS_EVENT_PLAY_EXIT_AURACAST_TTS);
#ifdef CONFIG_APP_TWS_SNOOP
		btif_tws_set_state(TWS_STATE_INIT);
#endif
	} else {
#if defined(BIS_CIS_RESTRICT_ENABLE)
		bis_cis_restrict = 1;
		bt_manager_audio_le_pause_adv();
#endif
		if (bt_manager_audio_get_cur_dev_num() != 0) {
			bt_manager_end_pair_mode();
			bt_manager_auto_reconnect_stop();
		}
		bt_manager_audio_switch_multi_point(0);

#ifdef CONFIG_APP_TWS_SNOOP
		btif_tws_set_state(TWS_STATE_DETECT_ROLE);
#endif
		if(!last_mode){
			sys_event_notify(SYS_EVENT_PLAY_ENTER_AURACAST_TTS);
		}
	}

	if (mode == 1) {
		if ( bt_manager_get_connected_dev_num()) {
			bt_manager_set_user_visual(true,false,false,0);
		}
	} else if (mode == 0) {
		bt_manager_set_user_visual(false,false,false,0);
	} else {}

#ifdef CONFIG_BT_SELF_APP
	selfapp_send_msg(MSG_SELFAPP_APP_EVENT, SELFAPP_CMD_ROLE_UPDATE, 0, mode);
#endif
}

/*
return [0,1,2] : 0-normal, 1-bms, 2-bmr
*/
uint8_t system_app_get_auracast_mode(void)
{
	return g_auracast_mode;
}

void system_app_send_input_event(u8_t cmd, u32_t value)
{
	struct app_msg msg = { 0 };

	msg.type = MSG_INPUT_EVENT;
	msg.cmd = cmd;
	msg.value = value;
	send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
}

void system_app_switch_auracast(bool auracast)
{
	SYS_LOG_INF("%d", auracast);
	if (auracast) {
		system_app_send_input_event(MSG_AURACAST_ENTER, 0);
	} else {
		system_app_send_input_event(MSG_AURACAST_EXIT, 0);
	}
}
#else
uint8_t system_app_get_auracast_mode(void)
{
	return 0;
}
#endif
