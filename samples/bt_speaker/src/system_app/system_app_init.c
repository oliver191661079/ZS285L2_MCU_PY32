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
#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <power_supply.h>
#include <srv_manager.h>
#include <app_manager.h>
#include <hotplug_manager.h>
#include <power_manager.h>
#include <input_manager.h>
#include <property_manager.h>
#include "app_defines.h"
#include <sys_manager.h>
#include <ui_manager.h>
#include "app_ui.h"
#include "system_util.h"
#include <thread_timer.h>
#ifdef CONFIG_TWS_UI_EVENT_SYNC
#include "../include/list.h"
#endif
#include <soc_dvfs.h>
#include <stream.h>

/* GPIO4 耳机检测：插入静音喇叭，拔出恢复播放（system_app_headphone_detect.c） */
extern void system_app_headphone_detect_init(void);

#ifdef CONFIG_PLAYTTS
#include "tts_manager.h"
#endif

#ifdef CONFIG_ESD_MANAGER
#include "esd_manager.h"
#endif

#ifdef CONFIG_OTA_APP
#include "../ota/ota_app.h"
#endif

#ifdef CONFIG_TOOL
#include "../tool/tool_app.h"
#endif

#ifdef CONFIG_BLUETOOTH
#include <bt_manager.h>
#endif

#ifdef CONFIG_BT_SELF_APP
#include <selfapp_api.h>
#endif


#include <logic.h>
#include "system_le_audio.h"
#include "system_app.h"
#include "run_mode.h"
#include <stack_backtrace.h>
#include "system_app.h"

#ifdef CONFIG_CONSOLE_SHELL
#include <shell/shell.h>
#include <drivers/console/uart_console.h>
#endif

LOG_MODULE_REGISTER(main, CONFIG_ACT_EVENT_APP_COMPILE_LEVEL);

#ifdef CONFIG_BT_CONTROLER_BQB
extern int btdrv_get_bqb_mode(void);
#endif

enum {
	SYS_INIT_NORMAL_MODE,
	SYS_INIT_ATT_TEST_MODE,
	SYS_INIT_ALARM_MODE,
};

int sys_ble_advertise_init(void);
void sys_ble_advertise_deinit(void);
void system_audio_policy_init(void);
void system_tts_policy_init(void);

extern void system_library_version_dump(void);
extern void trace_init(void);

extern int card_reader_mode_check(void);
#ifdef CONFIG_TEST_BLE_NOTIFY
extern void ble_test_init(void);
#endif

#ifdef CONFIG_GFP_PROFILE
extern int gfp_ble_stream_init(void);
#endif

#ifdef CONFIG_ACTIONS_ATT
void autotest_start(void);
int get_autotest_connect_status(void);
#endif

extern int system_btmgr_configs_update(void);
extern void system_input_handle_init(void);
extern int system_event_map_init(void);

#ifdef CONFIG_XSPI_NOR_SUSPEND_RESUME
int acts_xspi_nor_enable_suspend(uint8_t enable);
#endif

static bool att_enter_bqb_flag = false;
bool bt_manager_config_inited =  false;
bool bt_manager_power_on_setup =  true;



#ifdef CONFIG_TWS
static int app_get_expect_tws_role(void)
{
	int role = BTSRV_TWS_NONE;

#ifdef CONFIG_HOTPLUG
	if (hotplug_manager_get_state(HOTPLUG_LINEIN) == HOTPLUG_IN) {
		role = BTSRV_TWS_MASTER;
	}
#endif

	SYS_LOG_INF("%d\n", role);
	return role;
}
#endif

#ifdef CONFIG_CHARGER_APP
void system_app_report_bat_power(void)
{
	u8_t charging = 0;
	u8_t cap;
	int ret;

	ret = power_manager_get_charge_status();
	charging = (ret == POWER_SUPPLY_STATUS_CHARGING) ? 1 : 0;
	cap = power_manager_get_battery_capacity();

	//fix illegal value.
	if(cap > 100) {
		cap = 100;
	}

	SYS_LOG_INF("charge %d, cap %d%%", charging, cap);

}

static void system_notify_enter_charger_mode(void)
{
	struct app_msg msg = {0};

	SYS_LOG_INF("start charger");
	msg.type = MSG_CHARGER_MODE;
	msg.cmd = 1;
	send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
}

static void system_wait_exit_charger_mode(void)
{
	int terminaltion = 0;
	struct app_msg msg = {0};

	while (!terminaltion) {
		if (receive_msg(&msg, thread_timer_next_timeout())) {
			SYS_LOG_INF("type %d, cmd %d, value 0x%x\n", msg.type, msg.cmd, msg.value);
			switch (msg.type) {
			case MSG_CHARGER_MODE:
				if(!msg.cmd)
					terminaltion = true;
				break;
			case MSG_BAT_CHARGE_EVENT:
				system_app_report_bat_power();
				break;
			default:
				SYS_LOG_ERR("error type: 0x%x! \n", msg.type);
				continue;
			}
			if (msg.callback != NULL)
				msg.callback(&msg, 0, NULL);
		}
		thread_timer_handle_expired();
	}
}

#endif

#ifdef CONFIG_ACT_EVENT
static void act_event_callback_process(uint8_t event)
{
	if(event == ACT_EVENT_FLASH_FULL){
		act_event_set_level_filter("all", 0);
	}
}
#else
int act_event_data_send(uint32_t pack_data, ...)
{
	//dummy function
	return 0;
}
#endif

static void system_app_ota_init(void)
{
#ifdef CONFIG_OTA_APP
		ota_app_init();
#endif
}

void system_app_init(void)
{
	bool play_welcome = true;
#ifdef CONFIG_BT_MANAGER
	bool init_bt_manager = true;
#endif
	u16_t reboot_type = 0;
	u8_t reason = 0;

	system_power_get_reboot_reason(&reboot_type, &reason);
	SYS_LOG_INF("reboot type %d, reason %d", reboot_type, reason);

#ifdef CONFIG_FIRMWARE_VERSION
	fw_version_dump_current();
#endif

#ifdef CONFIG_XSPI_NOR_SUSPEND_RESUME
	acts_xspi_nor_enable_suspend(true);
#endif

	system_library_version_dump();

	system_init();

	system_app_headphone_detect_init();

	system_audio_policy_init();

	system_tts_policy_init();

	system_event_map_init();

	system_input_handle_init();


#ifdef CONFIG_BT_MANAGER
	system_btmgr_configs_update();
#endif

	trace_init();

#ifdef CONFIG_CONSOLE_SHELL
	/* shell_init runs before trace_init; restore UART0 RX + show prompt */
	uart_console_shell_restore();
	shell_show_prompt();
#endif

#ifdef CONFIG_BT_SELF_APP
	selfapp_config_init();
#endif

#ifdef CONFIG_ESD_MANAGER
	if (esd_manager_check_esd()) {
#ifdef CONFIG_WDT_MODE_RESET
		if (reboot_type == REBOOT_TYPE_WATCHDOG) {
			play_welcome = false;
		} else {
			esd_manager_reset_finished();
		}
#else
		if (reboot_type == REBOOT_TYPE_WATCHDOG
		    || reboot_type == REBOOT_TYPE_HW_RESET) {
			play_welcome = false;
		} else {
			esd_manager_reset_finished();
		}
#endif
	} else {
		esd_manager_reset_finished();
	}
#endif

    bt_manager_power_on_setup = true;

	if ((reboot_type == REBOOT_TYPE_SF_RESET)
	    && ((reason == REBOOT_REASON_GOTO_BQB_ATT) || (reason == REBOOT_REASON_GOTO_BQB ))) {
		play_welcome = false;
#ifdef CONFIG_BT_CONTROLER_BQB
		att_enter_bqb_flag = true;
#endif
#ifdef CONFIG_BT_PTS_TEST
		bt_manager_power_on_setup = false;
#endif
	}

	if (!play_welcome) {
#ifdef CONFIG_PLAYTTS
		tts_manager_lock();
#endif
	}

#ifdef CONFIG_BT_CONTROLER_BQB
	if (btdrv_get_bqb_mode() != 0) {
		att_enter_bqb_flag = true;
	}
#endif
	if (!att_enter_bqb_flag && reason != REBOOT_REASON_OTA_FINISHED && reason != REBOOT_REASON_OTA_FAILED \
		&& reason != REBOOT_REASON_SYSTEM_EXCEPTION) {
		bool enter_stub_tool = false;

#if (defined CONFIG_TOOL && defined CONFIG_ACTIONS_ATT)
#if 0
		if (tool_att_connect_try() == 0) {
			enter_stub_tool = true;
			init_bt_manager = false;
		}
#else
		if (get_autotest_connect_status() == 0) {
			enter_stub_tool = true;
			init_bt_manager = false;
#ifdef CONFIG_PLAYTTS
			tts_manager_lock();
#endif
			autotest_start();
		}
#endif
#endif

		system_app_ota_init();


		if (enter_stub_tool == false) {
#ifdef CONFIG_CARD_READER_APP
			if (usb_hotplug_device_mode()
			    && !(reason == REBOOT_REASON_NORMAL)) {

				system_ready();
				if (card_reader_mode_check() == NO_NEED_INIT_BT) {
					init_bt_manager = false;
				}
			} else
#endif
			{
#ifdef CONFIG_CHARGER_APP
				if(!run_mode_is_demo()){
					system_notify_enter_charger_mode();

					system_ready();

					system_wait_exit_charger_mode();
				}else{
					system_ready();
				}
#else
				system_ready();
#endif
			}
		}
	}else{
		system_app_ota_init();

		system_ready();

	}






#ifdef CONFIG_BT_MANAGER
	if (init_bt_manager&&bt_manager_config_inited) {
		if (run_mode_is_demo() == true)
		{
			bt_manager_power_on_setup = false;
		}

		bt_manager_init(system_bt_event_callback);

#ifdef CONFIG_TWS
		extern void audio_tws_pre_init(void);
		audio_tws_pre_init();
		bt_manager_tws_register_config_expect_role_cb(app_get_expect_tws_role);
#endif
	}
#endif

#ifdef CONFIG_TEST_BLE_NOTIFY
	ble_test_init();
#endif

#ifdef CONFIG_LOGIC_ANALYZER
	logic_init();
#endif

#ifdef CONFIG_TOOL
extern int tool_init(void);
	//tool_init();
#endif

#ifdef CONFIG_ACT_EVENT
	act_event_init(act_event_callback_process);
#endif

}

static void main_freq_init(void)
{
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	soc_dvfs_unset_level(SOC_DVFS_LEVEL_ALL_PERFORMANCE, "init");

#if (defined(CONFIG_DSP_IP_VENDOR_MUSIC_EFFECT_LIB) || defined(CONFIG_DSP_IP_VENDOR_VOICE_EFFECT_LIB))
	soc_dvfs_set_level(SOC_DVFS_LEVEL_CSB_PERFORMANCE, "init");
#else
	//Enter S2 and set the system clock to 60M
	soc_dvfs_set_level(SOC_DVFS_LEVEL_IDLE, "init");
#endif

#endif
}

#ifdef CONFIG_BT_LEATWS
extern int sys_ble_leatws_adv_register(void);
extern int sys_ble_leatws_adv_unregister(void);
#endif

#if defined(CONFIG_SYSTEM_APP_PY32_UART)
void system_app_py32_uart_init(void);
void py32_rhythm_set_data(const uint8_t *bands, uint8_t playing);
#endif

void system_app_init_bte_ready(void)
{
	main_freq_init();

#if defined(CONFIG_SYSTEM_APP_PY32_UART)
	system_app_py32_uart_init();
#endif

#if defined(CONFIG_SYSTEM_APP_PY32_UART)
	/* 蓝牙可见/可连由 PY32 UART 活动决定，见 system_app_py32_uart.c */
	if (system_app_py32_host_is_alive()) {
		bt_manager_start_wait_connect();
		if (bt_manager_power_on_setup) {
			bt_manager_powon_auto_reconnect(0);
		} else {
			bt_manager_powon_auto_reconnect(BT_MANAGER_REBOOT_ENTER_PAIR_MODE);
		}
	} else {
		bt_manager_set_user_visual(true, false, false, 0);
	}
#else
	bt_manager_start_wait_connect();
	if (bt_manager_power_on_setup) {
		bt_manager_powon_auto_reconnect(0);
	} else {
		bt_manager_powon_auto_reconnect(BT_MANAGER_REBOOT_ENTER_PAIR_MODE);
	}
#endif

#ifdef CONFIG_OTA_BACKEND_UART
	ota_app_init_uart();
#endif
#ifdef CONFIG_OTA_BACKEND_BLUETOOTH
	ota_app_init_bluetooth();
#endif
#ifdef CONFIG_BT_SELF_APP
#ifdef CONFIG_LOGSRV_SELF_APP
	selfapp_init(system_app_log_transfer);
#else
	selfapp_init(NULL);
#endif
#ifdef CONFIG_OTA_SELF_APP
	ota_app_init_selfapp();
#endif
#endif
#ifdef CONFIG_ACTIONS_ATT
	extern int act_att_notify_bt_engine_ready(void);
	act_att_notify_bt_engine_ready();
#endif

#ifdef CONFIG_BT_LEA_PTS_TEST
	if (bt_manager_config_lea_pts_test()) {
		pts_le_audio_init();
	}
	else
#endif /*CONFIG_BT_LEA_PTS_TEST*/
	{
		bt_le_audio_init();
#ifdef CONFIG_BT_ADV_MANAGER
	if (!bt_manager_config_lea_pts_test()){
		sys_ble_advertise_init();
	}
#endif

#ifdef CONFIG_BT_LEATWS
#if defined(CONFIG_BT_LEATWS_FL)
		bt_le_audio_exit();
		sys_ble_leatws_adv_unregister();
		sys_ble_leatws_adv_register();
		leatws_set_audio_channel(BT_AUDIO_LOCATIONS_FL);
		leatws_notify_leaudio_reinit();
#elif defined(CONFIG_BT_LEATWS_FR)
		bt_le_audio_exit();
		sys_ble_leatws_adv_unregister();
		leatws_set_audio_channel(BT_AUDIO_LOCATIONS_FR);
		leatws_scan_start();
#endif
#endif
	}

	struct app_msg msg = {0};

	msg.type = MSG_BT_ENGINE_READY;
	send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
}


void ctrl_dump_all_info(void);
void ctrl_mem_dump_info_all(void);

static void crash_dump_version_info(void)
{
	system_library_version_dump();
	fw_version_dump_current();
	ctrl_dump_all_info();
}

CRASH_DUMP_REGISTER(dump_version_info, 12) =
{
    .dump = crash_dump_version_info,
};

static void crash_dump_ctrl_mem_info(void)
{
	ctrl_mem_dump_info_all();
}

CRASH_DUMP_REGISTER(dump_ctrl_mem_info, 40) =
{
    .dump = crash_dump_ctrl_mem_info,
};


#ifdef CONFIG_DEBUG_RAMDUMP

#include <debug/ramdump.h>
#include <debug/data_export.h>
#include <soc.h>
#include <flash.h>
#ifdef CONFIG_TASK_WDT
#include <debug/task_wdt.h>
#endif

static void crash_dump_ramdump(void)
{
	struct device *flash_device = device_get_binding(CONFIG_XSPI_NOR_ACTS_DEV_NAME);

	if (flash_device){
		//flash_write_protection_set(flash_device, false);
		flash_write_protection_region_set(flash_device, FLASH_PROTECT_REGION_BOOT_AND_SYS);
	}

#ifdef CONFIG_TASK_WDT
	task_wdt_exit();
#endif

	dsp_do_wait();

	data_export_print((uint8_t *)0x60000, 0x20000);

	if(ramdump_check() != 0){
		printk("start ramdump...\n");
		ramdump_save();
	}else{
		printk("ramdump data existed, override\n");
		ramdump_save();
	}

}

static void crash_dump_ramdump_print(void)
{
	int i;

	printk("first ramdump...\n");

	ramdump_print();

	for(i = 0; i < 500; i++){
		k_busy_wait(1000);
	}

	printk("second ramdump...\n");

	ramdump_print();
}

CRASH_DUMP_REGISTER(ramdump_register, 10) =
{
    .dump = crash_dump_ramdump,
};

CRASH_DUMP_REGISTER(ramdump_print_register, 11) =
{
    .dump = crash_dump_ramdump_print,
};


#endif



#if defined(CONFIG_IRQ_STAT)
void dump_irqstat(void);

CRASH_DUMP_REGISTER(dump_irqstat_info, 20) =
{
    .dump = dump_irqstat,
};
#endif

#if defined(CONFIG_THREAD_STACK_INFO)
void kernel_dump_all_thread_stack_usage(void);

CRASH_DUMP_REGISTER(dump_stacks_analyze, 30) =
{
    .dump = kernel_dump_all_thread_stack_usage,
};
#endif
