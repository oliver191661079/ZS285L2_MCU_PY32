/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief system app shell.
 */
#include <os_common_api.h>
#include <mem_manager.h>
#include <msg_manager.h>
#include <stdio.h>
#include <init.h>
#include <string.h>
#include <kernel.h>
#include <shell/shell.h>
#include <stdlib.h>
#include <gpio.h>
#include <property_manager.h>
#include <logging/sys_log.h>
#include <sys_event.h>
#include <bt_manager.h>
#include <input_manager.h>
#include <app_manager.h>
#include <media_player.h>
#include <audio_policy.h>
#include <media_effect_param.h>
#include "app_defines.h"
#include "app_common.h"
#include "desktop_manager.h"

#ifdef CONFIG_PLAYTTS
#include "tts_manager.h"
#endif

#ifdef CONFIG_DATA_ANALY
#include <data_analy.h>
#endif
#include "system_app.h"
#include "app_launch.h"
#include <soc.h>

#define APP_SHELL "app"

#ifdef CONFIG_CONSOLE_SHELL
static int shell_enter_adfu(int argc, char *argv[])
{
	(void)argc;
	(void)argv;

	printk("reboot to ADFU mode\n");
	sys_pm_reboot(REBOOT_TYPE_GOTO_ADFU);
	return 0;
}

static int shell_input_key_event(int argc, char *argv[])
{
	if (argv[1] != NULL) {
		u32_t key_event;
		key_event = strtoul(argv[1], (char **)NULL, 0);
		sys_event_report_input(key_event);
	}
	return 0;
}

#ifdef CONFIG_PLAYTTS
void keytone_proc(os_timer * timer)
{
#ifdef CONFIG_BT_ENGINE_ACTS_ANDES
	app_message_post_async("main", MSG_VIEW_PAINT, MSG_UI_EVENT, &evt, 4);
#endif
}

static int shell_keytone_test(int argc, char *argv[])
{
	static os_timer timer;

	if (argv[1] != NULL) {
		if (memcmp(argv[1], "start", sizeof("start")) == 0) {
			os_timer_init(&timer, keytone_proc, NULL);
			os_timer_start(&timer, K_MSEC(10),
				       K_MSEC(strtoul(argv[2], NULL, 0)));
		} else if ((memcmp(argv[1], "stop", sizeof("stop")) == 0)) {
			os_timer_stop(&timer);
		}
	}
	return 0;
}

static int shell_tts_test(int argc, char *argv[])
{
	sys_event_notify(SYS_EVENT_ENTER_PAIR_MODE);

	return 0;
}


static int shell_tts_switch(int argc, char *argv[])
{
	if (argv[1] != NULL) {
		if (memcmp(argv[1], "on", sizeof("on")) == 0) {
			tts_manager_lock();
		} else if ((memcmp(argv[1], "off", sizeof("off")) == 0)) {
			tts_manager_unlock();
		}
	}

	return 0;
}
#endif

static int shell_dump_bt_info(int argc, char *argv[])
{
#ifdef CONFIG_BT_MANAGER
	bt_manager_dump_info();
#endif
	return 0;
}

static int shell_read_bt_rssi(int argc, char *argv[])
{
    int rssi = 0;
#ifdef CONFIG_BT_MANAGER
    rssi = bt_manager_bt_read_rssi(0);
    SYS_LOG_INF("RSSI %d",rssi);
#endif
	return 0;
}

static int shell_read_bt_link_quality(int argc, char *argv[])
{
    int rssi = 0;
#ifdef CONFIG_BT_MANAGER
    rssi = bt_manager_bt_read_link_quality(0);
    SYS_LOG_INF("LINK QUALITY %d",rssi);
#endif
	return 0;
}

static os_delayed_work test_menu_work;
static int test_menu_period;

static void test_menu_work_handler(os_work * work)
{
	sys_event_report_input(KEY_MENU | KEY_TYPE_SHORT_UP);

	os_delayed_work_submit(&test_menu_work, OS_SECONDS(test_menu_period));
}

static int shell_test_menu_key(int argc, char *argv[])
{
	if (argc == 2) {
		test_menu_period = strtoul(argv[1], (char **)NULL, 10);
	} else {
		test_menu_period = 60;
	}

	SYS_LOG_INF("period: %d", test_menu_period);

	os_delayed_work_init(&test_menu_work, test_menu_work_handler);
	os_delayed_work_submit(&test_menu_work, OS_SECONDS(test_menu_period));

	return 0;
}

static int shell_dump_app_data(int argc, char *argv[])
{
	desktop_manager_dump_state();

	return 0;
}

static int shell_charge_enable(int argc, char *argv[])
{
	u8_t en;

	if (argc == 2) {
		en = (u8_t)strtoul(argv[1], (char **)NULL, 10);
	} else {
		en = 1;
	}

	if (en != 0) {
		en = 1;
	}
	SYS_LOG_INF("charge enable=%d", en);

	struct device *dev;
	dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	if (NULL == dev) {
		SYS_LOG_ERR("gpio Device not found\n");
		return -1;
	}
	gpio_pin_configure(dev, 42, GPIO_DIR_OUT);
	gpio_pin_write(dev, 42, en);

	return 0;
}

#ifdef CONFIG_TOOL
extern int tool_init(void);
static int shell_open_stub(int argc, char *argv[])
{
	tool_init();
	return 0;
}
#endif

static int shell_set_da(int argc, char *argv[])
{
	extern void audio_system_volume_set_music_pada(u32_t v);
	u32_t en;

	if (argc == 2) {
		en = strtoul(argv[1], (char **)NULL, 10);
	} else {
		en = 1;
	}
	printk("Set music da to 0x%x", en);
	audio_system_volume_set_music_pada(en);

	return 0;

}

static int shell_dump_update_peq(int argc, char *argv[])
{

	eq_band_t eq;
	eq.cutoff = strtoul(argv[1], (char**)NULL, 10);
	eq.q = strtoul(argv[2], (char**)NULL, 10);
	eq.gain = strtoul(argv[3], (char**)NULL, 10);
	eq.type = strtoul(argv[4], (char**)NULL, 10);

	audio_policy_set_dynamic_peq_info(&eq);
	void *dump_player =  media_player_get_current_dumpable_player();
	if(dump_player)
	{
		media_player_dynamic_update_peq(dump_player, NULL, 0);
	}
	return 0;

}

static int shell_dump_volume_table(int argc, char *argv[])
{
	extern void audio_system_volume_dump(void);

	audio_system_volume_dump();

	return 0;

}


static int shell_pa_switch(int argc, char *argv[])
{
	if (argv[1] != NULL) {
		if (memcmp(argv[1], "on", sizeof("on")) == 0) {
		#ifdef CONFIG_AUDIO_POWERON_OPEN_PA
			hal_aout_open_pa(true);
		#endif
		} else if ((memcmp(argv[1], "off", sizeof("off")) == 0)) {
			/**disable pa */
		#ifdef CONFIG_AUDIO_POWERON_OPEN_PA
			hal_aout_close_pa(true);
		#endif
		}
	}

	return 0;
}

static void set_phone_controller_role(int role)
{
    int ret;
	ret = property_set_int("PHONE_CONTROLER_ROLE", role);
	if (ret != 0) {
       SYS_LOG_ERR("err:\n");
	}
	property_flush(NULL);
	SYS_LOG_INF("%d", role);
}

static int shell_phone_controller_role(int argc, char *argv[])
{
	if (argv[1] != NULL) {
		if (memcmp(argv[1], "master", sizeof("master")) == 0) {
            set_phone_controller_role(CONTROLER_ROLE_MASTER);
		} else if ((memcmp(argv[1], "slave", sizeof("slave")) == 0)) {
			set_phone_controller_role(CONTROLER_ROLE_SLAVE);
		}
	}

	return 0;
}

#ifdef CONFIG_DATA_ANALY
static int shell_dump_data_analy_data_info(int argc, char *argv[])
{
	data_analy_dump_data();
	data_analy_dump_play();
	return 0;
}

static int shell_dump_data_analy_play_all_record(int argc, char *argv[])
{
	data_analy_dump_play_all_record();
	return 0;
}


static int shell_dump_data_analy_clear_all_record(int argc, char *argv[])
{
	data_analy_data_clear();
	data_analy_play_clear();
	return 0;
}


#endif

bool bmr_any_audio_flag = false;
bool bmr_any_get(void)
{
	return bmr_any_audio_flag;
}

void bmr_any_set(bool enable)
{
	bmr_any_audio_flag = enable;
	SYS_LOG_INF("bmr_any_audio_flag=%d", bmr_any_audio_flag);
}

static int shell_bmr_any_set(int argc, char *argv[])
{
	if (argv[1] != NULL) {
		if (memcmp(argv[1], "enable", sizeof("enable")) == 0) {
			bmr_any_set(true);
		} else if ((memcmp(argv[1], "disable", sizeof("disable")) == 0)) {
			bmr_any_set(false);
		}
	}

	return 0;
}

#ifdef CONFIG_OTA_BACKEND_UHOST
static int shell_start_uhost_ota(int argc, char *argv[])
{
	extern int fs_manager_disk_init(const u8_t *volume_name);
	extern int ota_app_init_uhost(void);
	fs_manager_disk_init("USB:");
	ota_app_init_uhost();
	return 0;
}
#endif

static const struct shell_cmd app_commands[] = {
	{"adfu", shell_enter_adfu, "reboot into ADFU upgrade mode"},
	{"input", shell_input_key_event, "input key event"},
	{"btinfo", shell_dump_bt_info, "dump bt info"},
	{"rssi",shell_read_bt_rssi,"read bt rssi"},
	{"quality",shell_read_bt_link_quality,"read bt link quality"},

#ifdef CONFIG_PLAYTTS
	{"tts", shell_tts_switch, "tts on/off"},
	{"keytone", shell_keytone_test, "keytone start or stop"},
	{"tts_test", shell_tts_test, "tts test start"},
#endif
	{"menu", shell_test_menu_key, "input menu key periodically"},
	{"dump", shell_dump_app_data, "do currect app data dump."},
#ifdef CONFIG_TOOL
	{"open_stub", shell_open_stub, "open_stub"},
#endif
	{"chgen", shell_charge_enable, "Enable/disable charge."},
	{"setda", shell_set_da, "set da(dsp) value."},
	{"dumpvol", shell_dump_volume_table, "dump volume table."},
	{"peq", shell_dump_update_peq, "dump volume table."},
	{"pa", shell_pa_switch, "enable/disable pa"},

#ifdef CONFIG_DATA_ANALY
	{"dump_da", shell_dump_data_analy_data_info, "dump analy data"},
	{"dump_pa", shell_dump_data_analy_play_all_record, "dump analy data play all record"},
	{"clear_da", shell_dump_data_analy_clear_all_record, "clear all_data_analy record"},
#endif

	{"br_phone_controller_role", shell_phone_controller_role, "master/slave"},
	{"bmr_any", shell_bmr_any_set, "close/open bmr fliter"},

#ifdef CONFIG_OTA_BACKEND_UHOST
	{"uhost_ota", shell_start_uhost_ota, "start uhost ota"},
#endif

	{NULL, NULL, NULL}};
#endif

SHELL_REGISTER(APP_SHELL, app_commands);
