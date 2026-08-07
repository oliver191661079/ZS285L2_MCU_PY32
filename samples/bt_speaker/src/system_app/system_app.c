/*
 * Copyright (c) 2024 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 #include <mem_manager.h>
#include <msg_manager.h>
#include <fw_version.h>
#include <sys_event.h>
#include <app_launch.h>
#include <soc_dvfs.h>
#include "app_ui.h"
#include <stream.h>
#include <thread_timer.h>
#include <property_manager.h>
#include <sys_manager.h>
#include <ui_manager.h>
#include <app_manager.h>
#include <srv_manager.h>
#include <logic.h>
#include <global_mem.h>
#ifdef CONFIG_BLUETOOTH
#include <bt_manager.h>
#endif
#ifdef CONFIG_PLAYTTS
#include <tts_manager.h>
#endif
#ifdef CONFIG_ESD_MANAGER
#include <esd_manager.h>
#endif
#ifdef CONFIG_TASK_WDT
#include <debug/task_wdt.h>
#endif
#ifdef CONFIG_TOOL
#include "tool_app.h"
#endif
#include "system_app.h"
#include "app_defines.h"
#include "app_common.h"

#ifdef CONFIG_INPUT_MANAGER
#include <input_manager.h>
#endif

#ifdef CONFIG_BT_LEATWS
#include "system_le_audio.h"
#endif

#ifdef CONFIG_LOGIC_MCU_LS8A10023T
#include <logic_mcu_ls8a10023t.h>
#endif

#ifdef CONFIG_DATA_ANALY
#include <data_analy.h>
#endif


#ifdef CONFIG_BT_SELF_APP
#include "selfapp_api.h"
#endif

#include "system_app.h"
#ifdef CONFIG_ACT_EVENT
#include <app_act_event_id.h>
#include <logging/log_core.h>
LOG_MODULE_DECLARE(main, CONFIG_ACT_EVENT_APP_COMPILE_LEVEL);
#endif

// bigger than 1000
#define SYSTEM_TASK_WD_PERIOD 5000

system_app_context_t system_app_context;

extern int bt_event_callback(uint8_t event, uint8_t* extra, uint32_t extra_len);

int sys_ble_advertise_init(void);
void sys_ble_advertise_deinit(void);
int system_tws_event_handle(struct app_msg *msg);

static void _system_app_init(void)
{
	system_app_init();
}

static void _system_app_deinit(void)
{

}


static void system_app_do_poweroff(int result)
{
	system_app_context_t*  manager = system_app_get_context();

	SYS_LOG_INF("result %d", result);
	if (result != BTMGR_LREQ_POFF_RESULT_OK && result != BTMGR_RREQ_SYNC_POFF_RESULT_OK) {
		SYS_LOG_ERR("wrong result %d", result);
		return;
	}

#ifdef CONFIG_DATA_ANALY
	data_analy_exit();
#endif

#ifdef CONFIG_PROPERTY
	/* 关机前将 property 缓存（含自动重连设备列表 CFG_AUTOCONN_INFO）写入 nvram flash，
	 * 否则设备记录只留在 RAM，关机丢失后下次开机 btif_br_get_auto_reconnect_info 读到空列表(s:0)，
	 * 导致已配对手机无法自动重连。 */
	property_flush(NULL);
#endif

	system_power_off();


	manager->sys_status.in_power_off_stage = false;
}


void system_app_enter_poweroff(bool tws_trigger)
{
	static bool g_tws_trigger = false;
	system_app_context_t*  manager = system_app_get_context();

	SYS_LOG_INF("tws=%d, in_power_off=%d", tws_trigger, manager->sys_status.in_power_off_stage);
	SYS_EVENT_INF(EVENT_SYSTEM_POWER_OFF, tws_trigger,manager->sys_status.in_power_off_stage);
	if (manager->sys_status.in_power_off_stage) {
		return;
	}

	// ui_key_filter(true);

	if (tws_trigger) {
		g_tws_trigger = true;
		sys_event_notify(SYS_EVENT_POWER_OFF);
	} else {
		if (!g_tws_trigger) {
			bt_manager_tws_send_message(TWS_USER_APP_EVENT, TWS_EVENT_POWER_OFF, 0, 0);
		}
		manager->sys_status.in_power_off_stage = true;
		g_tws_trigger = false;
#ifdef CONFIG_BT_CONTROLER_BQB_SYS
		if (bt_manager_get_bqb_mode() > 0) {
			system_app_do_poweroff(BTMGR_LREQ_POFF_RESULT_OK);
		} else
#endif
		bt_manager_proc_poweroff(false);
	}
}

void system_app_do_poweroff_msg(struct app_msg* msg)
{
	system_app_context_t*  manager = system_app_get_context();

	if (manager->sys_status.in_power_off_stage) {
		system_app_do_poweroff(msg->reserve);
	}
}

static int sys_bt_restart_leaudio(void)
{
#ifdef CONFIG_LE_AUDIO_APP
	btmgr_ble_cfg_t* ble_config = bt_manager_ble_config();
	if (ble_config->leaudio_enable){
		leaudio_base_deinit();
		leaudio_base_init();
	}
#endif

	return 0;
}

#if 0
static void system_exit_front_app(void)
{
	struct app_msg msg = {0};
	msg.type = MSG_EXIT_APP;
	send_async_msg(APP_ID_MAIN, &msg);
}
#endif

static void system_btmgr_event_proc(struct app_msg* msg)
{
	switch (msg->cmd)
	{
	case MSG_BT_POWEROFF_RESULT:
		system_app_do_poweroff_msg(msg);
		break;

	case MSG_BT_LEAUDIO_RESTART:
		sys_bt_restart_leaudio();
		break;

#ifdef CONFIG_GFP_PROFILE
	case MSG_BT_GFP_CONNECTED:
		gfp_routine_start();
		break;

	case MSG_BT_GFP_DISCONNECTED:
		gfp_routine_stop();
		break;
#endif

#ifdef CONFIG_BT_ADV_MANAGER
	case MSG_BLE_ADV_CONTROL:
		if (msg->reserve == 0) {
			sys_ble_advertise_deinit();
		} else if (msg->reserve == 1) {
			sys_ble_advertise_init();
		}
		break;
#endif

	default:
		break;
	}
}

static void system_sys_event_proc(struct app_msg *msg)
{
	
	system_app_context_t*  manager = system_app_get_context();
	
	if (manager->sys_status.in_power_off_stage == true)
	{
		SYS_LOG_INF("drop event when power off\n");
		return;
	} 

	if (SYS_EVENT_BT_WAIT_PAIR == msg->cmd) {
		bt_manager_set_user_visual(false,false,false,0);
	}
	sys_event_process(msg->cmd);

#if 0
	if (SYS_EVENT_POWER_OFF == msg->cmd) {
#ifdef CONFIG_INPUT_MANAGER
		input_manager_lock();
#endif
		SYS_LOG_INF("power off\n");
		system_exit_front_app();
	}
#endif
}

static void system_app_msg_proc(struct app_msg* msg)
{
	switch (msg->type)
	{
	case MSG_SYS_EVENT:
		system_sys_event_proc(msg);
		break;

	case MSG_POWER_OFF:
		// Do real system power off after tws sync finish.
		// system_power_off();
		system_app_enter_poweroff(false);
		break;

	case MSG_REBOOT:
		SYS_EVENT_INF(EVENT_SYSTEM_REBOOT, msg->cmd);
		system_power_reboot(msg->cmd);
		break;

	case MSG_NO_POWER:
		SYS_EVENT_INF(EVENT_SYSTEM_NO_POWER);
		sys_event_notify(SYS_EVENT_POWER_OFF);
		break;

	case MSG_BT_ENGINE_READY:
		system_app_init_bte_ready();
	break;

	case MSG_AUTOTEST_START_BT:
#ifdef CONFIG_BT_MANAGER
		printk("autotest_start_BT_manager\n");
		bt_manager_init(system_bt_event_callback);
#endif
		break;

	case MSG_BT_MGR_EVENT:
		system_btmgr_event_proc(msg);
		break;

#ifdef CONFIG_PLAYTTS
	case MSG_TTS_EVENT:
		if (msg->cmd == TTS_EVENT_START_PLAY) {
			tts_manager_play_process();
		}
		break;
#endif

#ifdef CONFIG_BT_SELF_APP
	case MSG_BAT_CHARGE_EVENT:
		selfapp_report_bat();
		break;

	case MSG_SELFAPP_APP_EVENT:
		if (SELFAPP_CMD_CALLBACK == msg->cmd) {
			selfapp_on_connect_event((msg->reserve >> 4) & 0xF, msg->reserve & 0xF, msg->value);
		} else if (SELFAPP_CMD_ROLE_UPDATE == msg->cmd) {
			selfapp_eq_cmd_switch_auracast(msg->value);
			selfapp_notify_role();
		}
		break;
#endif

#ifdef CONFIG_APP_TWS_SNOOP
	case MSG_TWS_EVENT:
		system_tws_event_handle(msg);
	break;
#endif

	default:
		break;
	}

	if (msg->callback) {
		msg->callback(msg, 0, NULL);
	}
}

static void _system_app_loop(void *parama1, void *parama2, void *parama3)
{
	struct app_msg msg = { 0 };

	_system_app_init();

#ifdef CONFIG_TASK_WDT
	task_wdt_add(TASK_WDT_CHANNEL_SYSTEM_APP, 5000, NULL, NULL);
#endif

	while (true) {
		int timeout;

		timeout = thread_timer_next_timeout();
#ifdef CONFIG_TASK_WDT
		if (timeout > SYSTEM_TASK_WD_PERIOD * 1000) {
			timeout = (SYSTEM_TASK_WD_PERIOD - 1000)*1000;
		}
#endif
		if (receive_msg(&msg, timeout)) {
			SYS_LOG_INF("type %d, cmd %d, value %d\n", msg.type, msg.cmd, msg.value);
			MSG_RECV_TIME_STAT_START();
			system_app_msg_proc(&msg);
			MSG_RECV_TIME_STAT_STOP(msg.cmd, msg.type, msg.value);
		}

		thread_timer_handle_expired();

#ifdef CONFIG_TASK_WDT
		task_wdt_feed(TASK_WDT_CHANNEL_SYSTEM_APP);
#endif
	}

	_system_app_deinit();
}


static char __stack_sevice_noinit  __aligned(STACK_ALIGN) sysapp_stack_area[CONFIG_APP_STACKSIZE];

APP_DEFINE(system, sysapp_stack_area, sizeof(sysapp_stack_area),
	CONFIG_APP_PRIORITY, BACKGROUND_APP, NULL, NULL, NULL,
	_system_app_loop, NULL);

