/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
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
#ifdef CONFIG_BLUETOOTH
#include <bt_manager.h>
#endif
#ifdef CONFIG_PLAYTTS
#include <tts_manager.h>
#endif
#ifdef CONFIG_ESD_MANAGER
#include <esd_manager.h>
#endif
#ifdef CONFIG_TOOL
#include "tool_app.h"
#endif
#include "main_app.h"
#include "app_defines.h"
#include "app_common.h"

#ifdef CONFIG_BT_LEATWS
#include "system_le_audio.h"
#endif

#ifdef CONFIG_BT_SELF_APP
#include "selfapp_api.h"
#endif

#ifdef CONFIG_LOGIC_MCU_LS8A10023T
#include <logic_mcu_ls8a10023t.h>
#endif

#ifdef CONFIG_DATA_ANALY
#include <data_analy.h>
#endif

#ifdef CONFIG_TASK_WDT
#include <debug/task_wdt.h>
#endif

#ifdef CONFIG_ACT_EVENT
#include <app_act_event_id.h>
#include <logging/log_core.h>
LOG_MODULE_DECLARE(main, CONFIG_ACT_EVENT_APP_COMPILE_LEVEL);
#endif
int btdrv_get_bqb_mode(void);

// bigger than 1000
#define MAIN_TASK_WD_PERIOD 5000

static void main_app_init(void)
{
	main_app_view_init();
	system_app_launch_add(desktop_manager_get_default_plugin_id());
}

static int main_app_switch(u32_t mode, u8_t plugin, u8_t plugin_replaced)
{
	int current = desktop_manager_get_plugin_id();

	SYS_LOG_INF("mode %d, plugin %d, repaced %d", mode, plugin, plugin_replaced);

	switch (mode) {
	case APP_SWITCH_ADD:
		if (0 == desktop_manager_add(plugin)) {
			desktop_manager_switch(plugin, DESKTOP_SWITCH_CURR);
			if(plugin == DESKTOP_PLUGIN_ID_BR_CALL || plugin == DESKTOP_PLUGIN_ID_LE_CALL) {
				desktop_manager_lock();
			}
	#ifdef CONFIG_AURACAST
			if (current == DESKTOP_PLUGIN_ID_BMR && plugin != DESKTOP_PLUGIN_ID_BMR
			/* fix BUG02926901 avoid print log [ERR] [desktop] [desktop_manager_switch] plugin 2 no Exist.*/
			&& plugin != DESKTOP_PLUGIN_ID_OTA) {
				desktop_manager_del(current);
				break;
			}
	#endif
		} else {
			SYS_LOG_INF("failed to add plugin %d\n", plugin);
		}
		break;

	case APP_SWITCH_DEL:
		if(plugin == DESKTOP_PLUGIN_ID_BR_CALL || plugin == DESKTOP_PLUGIN_ID_LE_CALL) {
			desktop_manager_unlock();
		}
		desktop_manager_del(plugin);
		if(current == plugin) {
			desktop_manager_switch(0, DESKTOP_SWITCH_LAST);
		}
		break;

	case APP_SWITCH_REPLACE:
		if(plugin_replaced == DESKTOP_PLUGIN_ID_BR_CALL || plugin_replaced == DESKTOP_PLUGIN_ID_LE_CALL) {
			desktop_manager_unlock();
		}
		desktop_manager_del(plugin_replaced);
		if(0 == desktop_manager_add(plugin)){
			desktop_manager_switch(plugin,DESKTOP_SWITCH_CURR);
			if(plugin == DESKTOP_PLUGIN_ID_BR_CALL || plugin == DESKTOP_PLUGIN_ID_LE_CALL) {
				desktop_manager_lock();
			}
	#ifdef CONFIG_AURACAST
			if (current == DESKTOP_PLUGIN_ID_BMR && plugin != DESKTOP_PLUGIN_ID_BMR ) {
				desktop_manager_del(current);
				break;
			}
	#endif
		} else {
			SYS_LOG_WRN("failed to add plugin %d", plugin);
		}
		break;
	default:
		break;
	}

	return 0;
}

static void main_msg_proc(struct app_msg *msg)
{
	int result = 0;

	SYS_LOG_INF("type %d, cmd %d, value 0x%x\n", msg->type, msg->cmd, msg->value);

	switch (msg->type) {
#ifdef CONFIG_UI_MANAGER
	case MSG_UI_EVENT:
		ui_message_dispatch(msg->sender, msg->cmd, msg->value);
		break;
#endif
	case MSG_BT_EVENT:
		if(!main_bt_event_handle(msg)){
			desktop_manager_proc_app_msg(msg);
		}
		break;
	case MSG_INIT_APP:
#ifdef CONFIG_BT_CONTROLER_BQB_SYS
		if (btdrv_get_bqb_mode() > 0) {
			main_app_init();
		}
#else
#ifndef CONFIG_BT_MANAGER
		main_app_init();
#endif
#endif
		break;

	case MSG_BT_ENGINE_READY:
		main_app_init();
		break;

	case MSG_KEY_INPUT:
		main_key_event_handle(msg->value);
		break;

	case MSG_INPUT_EVENT:
		main_input_event_handle(msg);
		break;

#ifdef CONFIG_HOTPLUG_MANAGER
	case MSG_HOTPLUG_EVENT:
		main_hotplug_event_handle(msg);
		break;
#endif

	case MSG_SWITCH_APP:
		main_app_switch(msg->cmd, (msg->value)&0xFF, (msg->value>>8)&0xFF);
		break;

	case MSG_VOLUME_CHANGED_EVENT:
		main_app_volume_show(msg);
		break;

	case MSG_START_APP:
		//	SYS_LOG_INF("start %s\n", (char *)msg.ptr);
		//	app_switch((char *)msg.ptr, msg.reserve, true);
		break;

	case MSG_EXIT_APP:
		SYS_LOG_INF("exit desktop\n");
		desktop_manager_exit();
		break;

	default:
		desktop_manager_proc_app_msg(msg);
		break;
	}

	if (msg->callback)
	{
		msg->callback(msg, result, NULL);
	}
}

#ifdef CONFIG_LOGIC_MCU_LS8A10023T
static void logic_mcu_ls8a10023t_thread_timer_handler(struct thread_timer *timer, void* pdata)
{
	int ret = 0;

	//printk("[%s,%d]\n", __FUNCTION__, __LINE__);

	struct device *dev = device_get_binding(CONFIG_LOGIC_MCU_LS8A10023T_DEV_NAME);
	if (dev == NULL)
	{
		return;
	}

	ret = logic_mcu_ls8a10023t_power_key(dev);

	//printk("[%s,%d] ret : %d\n", __FUNCTION__, __LINE__, ret);
}
#endif

//#define CONFIG_ACT_EVENT_STRESS_TEST

#ifdef CONFIG_ACT_EVENT_STRESS_TEST
static void event_timer_test(struct thread_timer *timer, void* pdata)
{
	static int test_cnt;

	int i;

	for(i = 0; i < 10; i++){
		SYS_EVENT_INF(22, test_cnt++);
	}
}
#endif

void main_app(void)
{
	SYS_LOG_INF();
#ifdef CONFIG_LOGIC_MCU_LS8A10023T
	struct thread_timer logic_mcu_ls8a10023t_thread_timer;

	thread_timer_init(&logic_mcu_ls8a10023t_thread_timer, logic_mcu_ls8a10023t_thread_timer_handler, NULL);
	thread_timer_start(&logic_mcu_ls8a10023t_thread_timer, 0, 1000);
#endif

#ifdef CONFIG_ACT_EVENT_STRESS_TEST
	struct thread_timer event_timer;

	thread_timer_init(&event_timer, event_timer_test, NULL);
	thread_timer_start(&event_timer, 0, 10);
#endif

#ifdef CONFIG_DATA_ANALY
	system_data_analy_init(1);
#endif

#ifdef CONFIG_TASK_WDT
	task_wdt_add(TASK_WDT_CHANNEL_MAIN_APP, MAIN_TASK_WD_PERIOD, NULL, NULL);
#endif

	while (1) {
		struct app_msg msg = { 0 };
		int timeout;

		timeout = thread_timer_next_timeout();
#ifdef CONFIG_TASK_WDT
		if (timeout > MAIN_TASK_WD_PERIOD * 1000) {
			timeout = (MAIN_TASK_WD_PERIOD - 1000)*1000;
		}
#endif

		if (receive_msg(&msg, timeout)) {
			MSG_RECV_TIME_STAT_START();
			main_msg_proc(&msg);
			MSG_RECV_TIME_STAT_STOP(msg.cmd, msg.type, msg.value);
		}
		thread_timer_handle_expired();

#ifdef CONFIG_TASK_WDT
		task_wdt_feed(TASK_WDT_CHANNEL_MAIN_APP);
#endif
	}

}

extern char _main_stack[CONFIG_MAIN_STACK_SIZE];
APP_DEFINE(main, _main_stack, CONFIG_MAIN_STACK_SIZE,
	   APP_PRIORITY, FOREGROUND_APP, NULL, NULL, NULL, NULL, NULL);
