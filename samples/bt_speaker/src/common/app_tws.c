/*
 * Copyright (c) 2024 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file App tws (BT Snooping and bis)
 */

#define SYS_LOG_DOMAIN "apptws"

#ifdef CONFIG_APP_TWS
#include <os_common_api.h>
#include <logging/sys_log.h>
#include <string.h>
#include <mem_manager.h>
#include <msg_manager.h>
#include <app_manager.h>
#include <led_manager.h>
#include <volume_manager.h>
#include <audio_system.h>
#include <property_manager.h>
#include <bt_manager.h>
#include "app_launch.h"
#include "app_defines.h"
#include "desktop_manager.h"
#include "broadcast.h"
#include "app_ui.h"
#include "app_tws.h"
#include <hex_str.h>
#include <media_player.h>
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
#include <soc_dvfs.h>
#endif

#define CFG_TWS_MODE "TWS_MODE"
#define CFG_TWS_ROLE "TWS_ROLE"
#define CFG_BT_TWS_MAC "BT_TWS_MAC"

enum APP_TWS_SCAN_TYPE {
	APP_TWS_SCAN_TYPE_DEFALT,
	APP_TWS_SCAN_TYPE_SCAN_ONLY,
	APP_TWS_SCAN_TYPE_SCAN_ADV,
};

#define APP_PAWR_START_SCAN_TIME		(6*1000)		/* 6s */
#define APP_PAWR_START_ADV_TIME			(6*1000)		/* 6s */
#define APP_PAWR_SCAN_REST_TIME			(50)		/* 50ms */
#define APP_PAWR_ADV_REST_TIME			(50)		/* 50ms */
#define APP_PAWR_SCAN_RETRY_COUNT		(30)
#define APP_PAWR_ADV_RETRY_COUNT		(30)

static u16_t g_remote_version = BT_PAWR_VERSION_INVALID;
static bool app_adv_start = false;
static bool app_scan_start = false;
static u8_t scan_type = APP_TWS_SCAN_TYPE_DEFALT;
static os_delayed_work app_pawr_scan_restart_work;
static os_delayed_work app_pawr_adv_restart_work;
static bool app_adv_status = false;
static bool app_scan_status = false;
static u8_t adv_retry_cnt = 0;
static u8_t scan_retry_cnt = 0;
#ifdef CONFIG_APP_TWS_SNOOP
static struct thread_timer tws_ttimer = {0};
#endif

static OS_MUTEX_DEFINE(pawr_timer_mutex);
static struct thread_timer tws_pawr_timer = {0};
static int pawr_cmd_retry_count = 4;

static void app_tws_pawr_adv_start(bool enable);
static void app_tws_pawr_scan_start(u8_t type, bool enable);
static void app_tws_pawr_adv_stop(void);
static void app_tws_pawr_scan_stop(void);
void app_tws_pawr_adv_set_media_ver(u16_t ver);
int pawr_response_media_version(void);

static bool tws_status_enable = false;
static u8_t tws_status_mode = APP_TWS_MODE_NONE;
static u8_t tws_status_role = APP_TWS_ROLE_PRIMARY;
static bool tws_status_connected = false;

static void app_tws_status_enable(bool tws)
{
	SYS_LOG_INF("%d", tws);
	tws_status_enable = tws;
}

bool app_tws_status_get_enable(void)
{
	return tws_status_enable;
}

u8_t app_tws_status_get_mode(void)
{
	return tws_status_mode;
}

u8_t app_tws_status_get_role(void)
{
	return tws_status_role;
}

bool app_tws_status_get_connected(void)
{
	return tws_status_connected;
}

void app_tws_status_set_connected(bool connect)
{
	tws_status_connected = connect;
}

/* 真立体声 TWS：TWS 连接/断开状态变化时，把当前播放器的 DAE 输出
 * 模式刷新为分声道（主=L_ONLY / 从=R_ONLY）或双声道（DEFAULT），
 * 使播放中连接/断开 TWS 也能即时切换。播放器不存在时忽略——播放器
 * 创建时会按最新 TWS 状态自行设置（见各 media 文件的
 * set_player_effect_output_mode）。 */
static void app_tws_sync_effect_output_mode(void)
{
	media_player_t *player = media_player_get_current_main_player();
	int mode = CONFIG_MEDIA_EFFECT_OUTMODE;

	if (!player) {
		return;
	}

	if (app_tws_status_get_connected() && app_tws_status_get_enable()) {
		if (app_tws_status_get_role() == APP_TWS_ROLE_SECONDARY) {
			mode = MEDIA_EFFECT_OUTPUT_R_ONLY;
		} else {
			mode = MEDIA_EFFECT_OUTPUT_L_ONLY;
		}
	}

	SYS_LOG_INF("tws sync effect output mode %d", mode);
	media_player_set_effect_output_mode(player, mode);
}

/* ---- TWS 使能开关（供外部调用）---- */

static bool tws_user_disabled;

void app_tws_set_enable(bool enable)
{
	u8_t mode = app_tws_status_get_mode();
	u8_t role = app_tws_status_get_role();

	SYS_LOG_INF("enable=%d cur mode=%d role=%d", enable, mode, role);

	tws_user_disabled = !enable;

	if (enable) {
		/* 恢复上次保存的模式，若为 NONE 则默认 BIS */
		if (mode == APP_TWS_MODE_NONE) {
			mode = APP_TWS_MODE_BIS;
		}
		app_tws_mode_select(mode, role);
		sys_event_notify(SYS_EVENT_TWS_START_PAIR);
	} else {
		/* 停止 TWS：退出配对/扫描/广播 */
		sys_event_notify(SYS_EVENT_TWS_UNPAIR);
#ifdef ENABLE_PAWR_APP
		app_tws_pawr_adv_stop();
		app_tws_pawr_scan_stop();
#endif
#ifdef CONFIG_APP_TWS_SNOOP
		bt_manager_tws_end_pair_search();
		if (bt_manager_tws_get_dev_role() != BTSRV_TWS_NONE) {
			bt_manager_tws_disconnect();
		}
#endif
		app_tws_mode_select(APP_TWS_MODE_NONE, role);
		app_tws_status_enable(false);
	}
}

bool app_tws_get_enable(void)
{
	return !tws_user_disabled;
}

static void app_tws_save_tws_mode(u8_t mode, u8_t role)
{
	int ret;

	SYS_LOG_INF("mode %d, role %d", mode, role);

	tws_status_mode = mode;
	tws_status_role = role;

	ret = property_set_int(CFG_TWS_MODE, mode);
	if (ret) {
		SYS_LOG_ERR("err.");
	}

	ret = property_set_int(CFG_TWS_ROLE, role);
	if (ret) {
		SYS_LOG_ERR("err.");
	}
}

void app_tws_load_tws_mode(void)
{
	int value;
	u8_t mode, role;

	value = property_get_int(CFG_TWS_MODE, APP_TWS_MODE_NONE);
	mode = value&0xFF;

	value = property_get_int(CFG_TWS_ROLE, APP_TWS_ROLE_PRIMARY);
	role = value&0xFF;

	SYS_LOG_INF("mode %d, role %d", mode, role);
	tws_status_mode = mode;
	tws_status_role = role;
}

static int set_nvram_bt_tws_addr(const u8_t * addr)
{
	u8_t mac_str[16] = { 0 };
	u8_t addr_rev[6];
	int ret_val, i;

	if (!addr) {
		mac_str[0] = 0;
	} else {
		mac_str[0] = 1;
		for (i = 0; i < 6; i++)
			addr_rev[i] = addr[5 - i];
		
		hex_to_str((char *)(mac_str+1), (char *)addr_rev, 6);
	}

	ret_val = property_set(CFG_BT_TWS_MAC, (char *)mac_str, 13);
	if (ret_val < 0)
		return ret_val;

	property_flush(CFG_BT_TWS_MAC);

	return 0;
}

static int get_nvram_bt_tws_addr(u8_t * addr)
{
	char cmd_data[16] = { 0 };
	u8_t addr_rev[6];
	int ret_val, i;

	ret_val = property_get(CFG_BT_TWS_MAC, cmd_data, 16);

	if (ret_val < 13)
		return -1;

	if (0 == cmd_data[0])
		return -1;

	str_to_hex((char *)addr_rev, &cmd_data[1], 6);

	for (i = 0; i < 6; i++)
		addr[i] = addr_rev[5 - i];

	return 0;
}

u16_t app_tws_get_local_tws_ver(void)
{
	int plugin;
	u16_t ver;

	plugin = desktop_manager_get_plugin_id();

	//btmusic supports snoop tws, others not
	if (DESKTOP_PLUGIN_ID_BR_MUSIC == plugin || DESKTOP_PLUGIN_ID_NONE == plugin) {
		ver = TWS_LOCAL_VER;
	} else {
		ver = TWS_VER_BIS;
	}

	return ver;
}

#ifdef ENABLE_PAWR_APP
void app_tws_pawr_handle_cmd_reply(u8_t cmd, u8_t seq, u8_t reply)
{
	static u8_t last_seq = 0;

	if (seq != last_seq) {
		SYS_LOG_INF("seq %d, cmd %d, reply %d", seq, cmd, reply);
		last_seq = seq;
		if (reply == 0) {
			// cmd is not done by remote tws
		} else if (reply == 1) {
			if (cmd == PAWR_CMD_ENTER_SNOOP_TWS) {
				app_tws_send_app_mode_switch_msg(APP_TWS_MODE_SNOOP, APP_TWS_ROLE_PRIMARY);
			} else if (cmd == PAWR_CMD_EXIT_TWS) {
				app_tws_send_app_mode_switch_msg(APP_TWS_MODE_NONE, APP_TWS_ROLE_PRIMARY);
			} else if (cmd == PAWR_CMD_POWER_OFF) {
				system_app_send_input_event(MSG_TWS_POWER_OFF_REPLY, 0);
			}
		} else {
			// reserved
		}
	}
}

void app_tws_pawr_handle_key(u8_t seq, u32_t key)
{
	static u8_t last_seq = 0;
	struct app_msg msg = {0};

	if(!app_tws_status_get_connected()){
		return;
	}

	if (last_seq != seq) {
		SYS_LOG_INF("seq %d %d, key 0x%x", last_seq, seq, key);
		last_seq = seq;
		msg.type = MSG_KEY_INPUT;
		msg.value = key;
		send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
	}
}

static int app_tws_pawr_rsp_vnd_data_handle(const u8_t *buf, u16_t len)
{
	u8_t cmd_length = 0;
	u8_t cmd_type = 0;
	u8_t sync_vol;
	u16_t offset = 0;
	u16_t ver;

	//SYS_LOG_INF("len %d", len);
	if (((buf[offset]) | (buf[offset+1] << 8)) != SERIVCE_UUID) {
		SYS_LOG_INF("Unknow data service.");
		return -1;
	}

	offset+=2;
	while (offset < len)
	{
		cmd_length = buf[offset++];
		cmd_type = buf[offset++];
		if(cmd_length < 1 || cmd_length > 33) {
			SYS_LOG_INF("wrong length.");
			break;
		}
		//SYS_LOG_INF("cmd_type %d", cmd_type);
		switch (cmd_type)
		{
		case PAWR_DT_LIGHT:
			SYS_LOG_INF("light:%d\n", buf[offset]);
			break;

		case PAWR_DT_VOLUME:
			sync_vol = padv_volume_map(buf[offset], 0);
			pawr_sync_volume(sync_vol);
			break;

		case PAWR_DT_TWS_VERSION:
			//pawr version
			ver= buf[offset] | (buf[offset+1]<<8);
			SYS_LOG_INF("remote tws ver 0x%x", ver);
			g_remote_version = ver;
			break;
		case PAWR_DT_KEY:
		{
			u8_t seq;
			u32_t key;

			seq = buf[offset];
			key = buf[offset + 1] 
				| (buf[offset + 2] << 8)
				| (buf[offset + 3] << 16)
				| (buf[offset + 4] << 24);
			app_tws_pawr_handle_key(seq, key);
		}
			break;
		case PAWR_DT_CMD_REPLY:
			app_tws_pawr_handle_cmd_reply(buf[offset + 1], buf[offset], buf[offset + 2]);
			break;
		case PAWR_DT_MEDIA_VERSION:
		{
			ver = buf[offset] | (buf[offset + 1] << 8);
			media_player_set_pair_version(ver);
		}
		break;

		default:
			SYS_LOG_WRN("Unknow type");
			break;
		}
		offset += (cmd_length - 1);
	}

	return 0;
}

static void app_tws_handle_pawr_cmd(u8_t seq, u8_t cmd)
{
	static u8_t last_seq = 0;
	u8_t reply = 0;

	if(seq != last_seq) {
		SYS_LOG_INF("seq %d(%d) cmd %d", seq, last_seq, cmd);
		last_seq = seq;

		switch (cmd) {
		case PAWR_CMD_ENTER_SNOOP_TWS:
			app_tws_send_app_mode_switch_msg(APP_TWS_MODE_SNOOP, APP_TWS_ROLE_SECONDARY);
			reply = 1;
			break;
		case PAWR_CMD_ENTER_BIS_TWS:
			app_tws_send_app_mode_switch_msg(APP_TWS_MODE_BIS, APP_TWS_ROLE_SECONDARY);
			reply = 1;
			break;
		case PAWR_CMD_EXIT_TWS:
			app_tws_send_app_mode_switch_msg(APP_TWS_MODE_NONE, APP_TWS_ROLE_SECONDARY);
			reply = 1;
			break;
		case PAWR_CMD_POWER_OFF:
			sys_event_notify(SYS_EVENT_POWER_OFF);
			reply = 1;
			break;
		default:
			break;
		}
		pawr_response_cmd(seq, cmd, reply);
	}
}

static int app_tws_pawr_vnd_data_handle(const uint8_t *data, uint16_t len)
{
	u16_t offset = 0;
	u16_t ver;
	u8_t cmd_length = 0;
	u8_t cmd_type = 0;

	SYS_LOG_INF("len %d", len);

	if (((data[offset]) | (data[offset + 1] << 8)) != SERIVCE_UUID) {
		SYS_LOG_INF("Unknow data service.");
		return -1;
	}

	offset += 2;
	while (offset < len)
	{
		cmd_length = data[offset++];
		cmd_type = data[offset++];
		if(cmd_length < 1 || cmd_length > 33) {
			SYS_LOG_INF("wrong length.");
			break;
		}

		SYS_LOG_INF("cmd_type %d", cmd_type);
		switch (cmd_type)
		{
		case PAWR_DT_TWS_VERSION:
			//pawr version
			ver = data[offset] | (data[offset+1]<<8);
			g_remote_version = ver;
			SYS_LOG_INF("remote tws ver 0x%x", ver);
			break;
		case PAWR_DT_CMD:
			app_tws_handle_pawr_cmd(data[offset], data[offset+1]);
			break;

		case PAWR_DT_MEDIA_VERSION:
			ver = data[offset] | (data[offset + 1] << 8);
			SYS_LOG_INF("ver 0x%x", ver);
			media_player_set_pair_version(ver);
			pawr_response_media_version();
			break;

		default:
			SYS_LOG_INF("Unknow type");
			break;
		}
		offset += (cmd_length - 1);
	}

	return 0;
}

static u16_t app_tws_get_compatible_ver(void)
{
	u16_t ver, l_ver, r_ver;

 	r_ver = g_remote_version;
	l_ver = app_tws_get_local_tws_ver();

	SYS_LOG_INF("r_ver 0x%x, l_ver 0x%x", r_ver, l_ver);

	if (BT_PAWR_VERSION_INVALID == r_ver) {
		SYS_LOG_INF("invalid remote ver=0x%x.", r_ver);
		ver = BT_PAWR_VERSION_INVALID;
	} else {
		if (l_ver <= r_ver) {
			ver = l_ver;
		} else {
			ver = r_ver;
		}
	}

	return ver;
}

int app_tws_pawr_adv_set_cmd(u8_t cmd)
{
	static u8_t seq = 1;
	u8_t offset = 0;
	u8_t buf[16];

	SYS_LOG_INF("seq %d, cmd %d", seq, cmd);

	//uuid
	buf[offset++] = SERIVCE_UUID & 0xFF;
	buf[offset++] = SERIVCE_UUID >> 8;

	//cmd
	buf[offset++] = 3;
	buf[offset++] = PAWR_DT_CMD;
	buf[offset++] = seq++;
	buf[offset++] = cmd;

	bt_manager_pawr_vnd_per_send(buf, offset, BT_DATA_MANUFACTURER_DATA);
	return 0;
}

static void app_tws_pawr_adv_set_ver(u16_t ver)
{
	uint8_t ver_data[6];

	SYS_LOG_INF("ver 0x%x", ver);

	//UUID
	ver_data[0] = SERIVCE_UUID & 0xFF;
	ver_data[1] = SERIVCE_UUID >> 8;
	//version
	ver_data[2] = 1 + 2;
	ver_data[3] = PAWR_DT_TWS_VERSION;
	ver_data[4] = ver & 0xFF ; // minor version
	ver_data[5] = ver >> 8; // major version
	bt_manager_pawr_vnd_per_send(ver_data,6,BT_DATA_MANUFACTURER_DATA);
}

void app_tws_pawr_adv_set_media_ver(u16_t ver)
{
	uint8_t ver_data[6];

	SYS_LOG_INF("media ver 0x%x", ver);

	//UUID
	ver_data[0] = SERIVCE_UUID & 0xFF;
	ver_data[1] = SERIVCE_UUID >> 8;
	//version
	ver_data[2] = 1 + 2;
	ver_data[3] = PAWR_DT_MEDIA_VERSION;
	ver_data[4] = ver & 0xFF ; // minor version
	ver_data[5] = ver >> 8; // major version
	bt_manager_pawr_vnd_per_send(ver_data,6,BT_DATA_MANUFACTURER_DATA);
}


static int app_tws_pawr_set_ext_adv(u16_t ver)
{
	int err;
	u8_t ver_data[6];

	SYS_LOG_INF("ver 0x%x", ver);

	//UUID
	ver_data[0] = SERIVCE_UUID & 0xFF;
	ver_data[1] = SERIVCE_UUID >> 8;
	//version, Notice: Do not modify
	ver_data[2] = 1 + 2;
	ver_data[3] = PAWR_DT_TWS_VERSION;
	ver_data[4] = ver & 0xFF ; // minor version
	ver_data[5] = ver >> 8; // major version

	err = bt_manager_pawr_set_ext_adv(ver_data, 6);
	if (err != 0) {
		SYS_LOG_ERR("err %d.",err);
		return err;
	}

	return 0;
}

static void app_tws_pawr_rsp_set_ver(u16_t ver)
{
	// Notice: Do not modify
	/**/
	uint8_t ver_data[6];
	
	SYS_LOG_INF("ver 0x%x", ver);

	//UUID
	ver_data[0] = SERIVCE_UUID & 0xFF;
	ver_data[1] = SERIVCE_UUID >> 8;
	//version
	ver_data[2] = 1 + 2;
	ver_data[3] = PAWR_DT_TWS_VERSION;
	ver_data[4] = ver & 0xFF ; // minor version
	ver_data[5] = ver >> 8; // major version
	bt_manager_pawr_vnd_rsp_send(ver_data, 6,BT_DATA_MANUFACTURER_DATA);
}

static void app_tws_pawr_adv_stop(void)
{
	SYS_LOG_INF("");
	app_tws_stop_pawr_cmd_reply();

	if (app_adv_start)
		os_delayed_work_cancel(&app_pawr_adv_restart_work);

	bt_manager_pawr_adv_stop();
	app_adv_status = false;
}

static void app_tws_pawr_scan_stop(void)
{
	SYS_LOG_INF("");

	if (app_scan_start)
		os_delayed_work_cancel(&app_pawr_scan_restart_work);

	bt_manager_pawr_receive_stop();
	app_scan_status = false;
}

static void app_pawr_scan_continue(void)
{
	SYS_LOG_INF("app scan continue %d.\n", app_scan_status);
	if (true == app_scan_status) {
		os_delayed_work_cancel(&app_pawr_scan_restart_work);
		os_delayed_work_submit(&app_pawr_scan_restart_work, APP_PAWR_START_SCAN_TIME);
	}
}

static void app_pawr_adv_retry_start(os_work *work)
{
	SYS_LOG_INF("app adv retry %d.\n", app_adv_status);
	if (true == app_adv_status) {
		app_tws_pawr_adv_stop();
		if (adv_retry_cnt < APP_PAWR_ADV_RETRY_COUNT) {
			os_delayed_work_submit(&app_pawr_adv_restart_work, APP_PAWR_ADV_REST_TIME);
		} else if (app_tws_status_get_enable()) {
			/* PAWR 组对超时（广播 retry 耗尽）：退出 TWS 组对模式并保存 mode=NONE，
			 * 停止 PAWR 广播，避免 285L_PAWR 广播名被手机扫描到后覆盖设备名 */
			SYS_LOG_INF("pawr adv retry timeout, exit tws pair mode");
			app_tws_set_enable(false);
		}
	} else {
		app_tws_pawr_adv_start(true);
	}
	SYS_LOG_INF(":");
}

static void app_pawr_scan_retry_start(os_work *work)
{
	SYS_LOG_INF("app scan retry %d.\n", app_scan_status);
	if (true == app_scan_status) {
		app_tws_pawr_scan_stop();
		if (scan_retry_cnt < APP_PAWR_SCAN_RETRY_COUNT) {
			os_delayed_work_submit(&app_pawr_scan_restart_work, APP_PAWR_SCAN_REST_TIME);
		} else if (app_tws_status_get_enable()) {
			/* PAWR 扫描超时（retry 耗尽）：退出 TWS 组对模式，停止广播/扫描 */
			SYS_LOG_INF("pawr scan retry timeout, exit tws pair mode");
			app_tws_set_enable(false);
		}
	} else {
		app_tws_pawr_scan_start(scan_type, true);
	}
	SYS_LOG_INF(":");
}

static void app_tws_pawr_adv_start(bool retry)
{
	u16_t ver;
	struct bt_le_per_adv_param per_adv_param1 = {0};
	u8_t addr[6];

	ver = app_tws_get_local_tws_ver();
	SYS_LOG_INF("ver 0x%x", ver);

	if (0 == get_nvram_bt_tws_addr(addr)) {
		bt_manager_pawr_match_mac_set(addr);
	} else {
		bt_manager_pawr_match_mac_set(NULL);
	}
	if (false == app_adv_start) {
		os_delayed_work_init(&app_pawr_adv_restart_work, app_pawr_adv_retry_start);
		app_adv_start = true;
	} else {
		os_delayed_work_cancel(&app_pawr_adv_restart_work);
	}

	os_delayed_work_submit(&app_pawr_adv_restart_work, APP_PAWR_START_ADV_TIME);
	per_adv_param1.interval_min = PAwR_INTERVAL;
	per_adv_param1.interval_max = PAwR_INTERVAL;
	per_adv_param1.subevent_interval = PAwR_SUB_INTERVAL;
	per_adv_param1.response_slot_delay = PAwR_RSP_DELAY;
	bt_manager_pawr_adv_start(app_tws_pawr_rsp_vnd_data_handle, &per_adv_param1);
	app_adv_status = true;
	app_tws_pawr_set_ext_adv(ver);
	app_tws_pawr_adv_set_ver(ver);
	if (true == retry)
		adv_retry_cnt ++;
	else
		adv_retry_cnt = 0;
}

static void app_tws_pawr_scan_start(u8_t type, bool retry)
{
	SYS_LOG_INF("");
	u8_t addr[6];
	struct bt_le_scan_param param;
	if (0 == get_nvram_bt_tws_addr(addr)) {
		bt_manager_pawr_match_mac_set(addr);
	} else {
		bt_manager_pawr_match_mac_set(NULL);
	}

	if (false == app_scan_start) {
		os_delayed_work_init(&app_pawr_scan_restart_work, app_pawr_scan_retry_start);
		app_scan_start = true;
	} else {
		os_delayed_work_cancel(&app_pawr_scan_restart_work);
	}

	scan_type = type;
	os_delayed_work_submit(&app_pawr_scan_restart_work, APP_PAWR_START_SCAN_TIME);
	memset(&param, 0, sizeof(struct bt_le_scan_param));
	param.type = BT_LE_SCAN_TYPE_PASSIVE;
	param.options = BT_LE_SCAN_OPT_NONE/*BT_LE_SCAN_OPT_FILTER_DUPLICATE*//*BT_LE_SCAN_OPT_NONE*/;
	if (APP_TWS_SCAN_TYPE_SCAN_ONLY == type) {
		/* [65ms, 110ms], almost 60% duty cycle by default */
		param.interval = 176;
		param.window = 70;
	} else if (APP_TWS_SCAN_TYPE_SCAN_ADV == type) {
		/* [30ms, 60ms], almost 50% duty cycle by default */
		param.interval = 0x60;
		param.window = 0x30;
	} else {
		param.interval = 176;
		param.window = 70;
	}
	param.timeout = 0;
	bt_manager_pawr_receive_start(app_tws_pawr_vnd_data_handle, &param, -50);
	app_scan_status = true;
	if (true == retry)
		scan_retry_cnt ++;
	else
		scan_retry_cnt = 0;
}

static void app_tws_version_switch(u16_t ver, u8_t role)
{
	SYS_LOG_INF("ver 0x%x role %d.", ver, role);

	if (BT_PAWR_VERSION_003 == ver) {
		bt_manager_set_user_visual(false,false,false,0);
		if (APP_TWS_ROLE_PRIMARY == role) {
			app_tws_pawr_scan_stop();
		}
		app_tws_pawr_adv_stop();

#ifdef CONFIG_APP_TWS_SNOOP
		if (bt_manager_tws_get_dev_role() != BTSRV_TWS_NONE) {
			bt_manager_tws_end_pair_search();
			//bt_manager_tws_clear_paired_list();
			bt_manager_tws_disconnect();
		}
		bt_manager_tws_pair_search();
#endif
	} else if (BT_PAWR_VERSION_002 == ver) {
#ifdef CONFIG_APP_TWS_SNOOP
		//to diable snoop tws auto connted when pawr in on connect.
		bt_manager_auto_reconnect_stop();
#endif
		if (APP_TWS_ROLE_PRIMARY == role) {
			app_tws_pawr_scan_stop();
		} else if (APP_TWS_ROLE_SECONDARY == role) {
			app_tws_pawr_adv_stop();
		}
	} else if (BT_PAWR_VERSION_001 == ver) {
		//Not compatible.
		SYS_LOG_WRN("Not compatible ver = %d.", ver);
	} else {
		SYS_LOG_WRN("no version.");
		//disconnect
		app_tws_pawr_scan_stop();
		app_tws_pawr_adv_stop();
		if (APP_TWS_ROLE_PRIMARY == role) {
			app_tws_pawr_adv_start(false);
		} else if (APP_TWS_ROLE_SECONDARY == role) {
			app_tws_pawr_scan_start(APP_TWS_SCAN_TYPE_SCAN_ONLY, false);
		}
	}
}

void app_tws_close_tws_timer_handler(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	SYS_LOG_INF("");
	app_tws_mode_select(APP_TWS_MODE_NONE, APP_TWS_ROLE_PRIMARY);
}

static void app_tws_pawr_cmd_timer(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	int cmd = (int)expiry_fn_arg;

	SYS_LOG_INF("cmd %d try %d", cmd, pawr_cmd_retry_count);

	if (PAWR_CMD_ENTER_SNOOP_TWS == cmd) {
		app_tws_pawr_adv_set_cmd(PAWR_CMD_ENTER_SNOOP_TWS);
	} else if (PAWR_CMD_EXIT_TWS == cmd) {
		app_tws_pawr_adv_set_cmd(PAWR_CMD_EXIT_TWS);
	} else if (PAWR_CMD_POWER_OFF == cmd) {
		app_tws_pawr_adv_set_cmd(PAWR_CMD_POWER_OFF);
	}

	if (0 < --pawr_cmd_retry_count) {
		thread_timer_start(&tws_pawr_timer, K_MSEC(200), 0);
	} else {
		if (PAWR_CMD_ENTER_SNOOP_TWS == cmd) {
			app_tws_send_app_mode_switch_msg(APP_TWS_MODE_SNOOP, APP_TWS_ROLE_SECONDARY);
		} else if (PAWR_CMD_EXIT_TWS == cmd) {
			app_tws_mode_select(APP_TWS_MODE_NONE, APP_TWS_ROLE_PRIMARY);
		} else if (PAWR_CMD_POWER_OFF == cmd) {
			sys_event_send_message(MSG_POWER_OFF);
		}
	}
}

void app_tws_wait_pawr_cmd_reply(int cmd)
{
	os_mutex_lock(&pawr_timer_mutex, OS_FOREVER);

	SYS_LOG_INF("cmd %d", cmd);

	if(!thread_timer_is_running(&tws_pawr_timer)){
		thread_timer_stop(&tws_pawr_timer);
	}
	thread_timer_init(&tws_pawr_timer, app_tws_pawr_cmd_timer, (void*)cmd);
	if(cmd == PAWR_CMD_POWER_OFF){
		pawr_cmd_retry_count = 5;
	} else {
		pawr_cmd_retry_count = 3;
	}
	thread_timer_start(&tws_pawr_timer, K_MSEC(200), 0);

	os_mutex_unlock(&pawr_timer_mutex);
}

void app_tws_stop_pawr_cmd_reply(void)
{
	os_mutex_lock(&pawr_timer_mutex, OS_FOREVER);
	SYS_LOG_INF("cmd %d", (int)tws_pawr_timer.expiry_fn_arg);

	if(thread_timer_is_running(&tws_pawr_timer)){
		SYS_LOG_INF("stop");
		thread_timer_stop(&tws_pawr_timer);
	}
	os_mutex_unlock(&pawr_timer_mutex);
}
#endif

static bool app_tws_expect_primary_role(void)
{
	int plugin;
	bool role = false;

	plugin = desktop_manager_get_plugin_id();

	if ((DESKTOP_PLUGIN_ID_BR_MUSIC == plugin &&
		bt_manager_get_connected_dev_num() > 0) ||
		(DESKTOP_PLUGIN_ID_LE_MUSIC == plugin) ||
		DESKTOP_PLUGIN_ID_SDCARD_PLAYER == plugin ||
		DESKTOP_PLUGIN_ID_USB_PLAYER == plugin ||
		DESKTOP_PLUGIN_ID_NOR_PLAYER == plugin ||
		DESKTOP_PLUGIN_ID_UAC == plugin ||
		DESKTOP_PLUGIN_ID_LINE_IN == plugin ||
		DESKTOP_PLUGIN_ID_I2SRX_IN == plugin ||
		DESKTOP_PLUGIN_ID_SPDIFRX_IN == plugin ) {
		role = true;
	}

	SYS_LOG_INF("plugin %d, connected %d, role %d.", plugin, bt_manager_get_connected_dev_num(), role);

	return role;
}

void app_tws_manual_switch(void)
{
	set_nvram_bt_tws_addr(NULL);
	broadcast_set_tws_broadcast_name(NULL);
	bt_manager_pawr_match_mac_set(NULL);
#ifdef CONFIG_APP_TWS_SNOOP
	bt_manager_auto_reconnect_stop();
#endif

	if (!app_tws_status_get_enable()) {
		SYS_LOG_INF("Enable");

		if (0 != system_app_get_auracast_mode()) {
			SYS_LOG_INF("Cannot enter tws in auracast mode.");
			return;
		}

		sys_event_notify(SYS_EVENT_TWS_START_PAIR);

		bt_manager_pawr_check_connected_dev();

		//start tws
		bt_manager_set_user_visual(true,false,false,0);
#ifdef CONFIG_APP_TWS_SNOOP
		bt_manager_tws_end_pair_search();
		if (bt_manager_tws_get_dev_role() != BTSRV_TWS_NONE) {
			//bt_manager_tws_clear_paired_list();
			bt_manager_tws_disconnect();
		}
#endif
		app_tws_status_enable(true);
		app_tws_status_set_connected(false);
		if (app_tws_expect_primary_role()) {
			app_tws_pawr_adv_start(false);
		}
		else {
			app_tws_pawr_adv_start(false);
			app_tws_pawr_scan_start(APP_TWS_SCAN_TYPE_SCAN_ADV, false);
		}
	}
	else {
		u8_t mode, role;
		/*close tws. 
		* Only tws primary can enter here, as tws secondary 
		* does not response to key.*/

		sys_event_notify(SYS_EVENT_TWS_UNPAIR);
		mode = app_tws_status_get_mode();
		role = app_tws_status_get_role();
		SYS_LOG_INF("Disable on mode %d, role %d", mode, role);
		if(mode == APP_TWS_MODE_BIS) {
#ifdef ENABLE_PAWR_APP
			if(app_tws_status_get_connected()) {
				app_tws_pawr_adv_set_cmd(PAWR_CMD_EXIT_TWS);
				//Wait reply from secondary
				app_tws_wait_pawr_cmd_reply(PAWR_CMD_EXIT_TWS);
			} else {
				app_tws_mode_select(APP_TWS_MODE_NONE, role);
			}
#else
			app_tws_mode_select(APP_TWS_MODE_NONE, role);
#endif
		} else if(mode == APP_TWS_MODE_SNOOP) {
#ifdef CONFIG_APP_TWS_SNOOP

			if(app_tws_status_get_connected()) {
				mode = APP_TWS_MODE_NONE;
				bt_manager_tws_send_message(TWS_USER_APP_EVENT, TWS_EVENT_TWS_MODE_SWITCH, &mode, 1);
			}
			//close tws on primary without relply from secondary
			if(!thread_timer_is_running(&tws_ttimer)){
				thread_timer_init(&tws_ttimer, app_tws_close_tws_timer_handler, NULL);
			} else {
				thread_timer_stop(&tws_ttimer);
			}
			thread_timer_start(&tws_ttimer, 600, 0);
#endif
		} else if(mode == APP_TWS_MODE_NONE) {
			app_tws_mode_select(APP_TWS_MODE_NONE, role);
            bt_manager_set_user_visual(false,false,false,0);
		}

	}
}

void app_tws_mode_select(u8_t mode, u8_t role)
{
	SYS_LOG_INF("mode %d, role %d", mode, role);

	app_tws_save_tws_mode(mode, role);
	if (APP_TWS_MODE_SNOOP == mode) {
		if (desktop_manager_get_plugin_id() == DESKTOP_PLUGIN_ID_BMR) {
			system_app_launch_switch(DESKTOP_PLUGIN_ID_BMR, DESKTOP_PLUGIN_ID_BR_MUSIC);
		}
		if(0 != system_app_get_auracast_mode()) {
			system_app_set_auracast_mode(0);
		}
		bt_manager_set_user_visual(false,false,false,0);
		app_tws_status_enable(true);
		if (bt_manager_tws_get_dev_role() == BTSRV_TWS_MASTER ||
			bt_manager_tws_get_dev_role() == BTSRV_TWS_SLAVE) {
			return;
		}

#ifdef ENABLE_PAWR_APP
		//close pawr adv to fast tws connecting
		//app_tws_pawr_adv_scan_stop() can not be called on secondary, as the command reply will not be received.
		if (APP_TWS_ROLE_PRIMARY == role) {
			app_tws_pawr_adv_stop();
		}
#endif

		if (!bt_manager_is_tws_paired_valid()) {
			bt_manager_tws_pair_search();
		} else if (APP_TWS_ROLE_PRIMARY == role) {
			bt_manager_start_wait_connect();
		} else if (APP_TWS_ROLE_SECONDARY == role) {
			bt_manager_tws_end_pair_search();
			bt_manager_manual_reconnect();
		}
	}
	else if (APP_TWS_MODE_BIS == mode) {
		app_tws_status_enable(true);
#ifdef CONFIG_APP_TWS_SNOOP
		bt_manager_tws_end_pair_search();
		if (bt_manager_tws_get_dev_role() != BTSRV_TWS_NONE) {
			//bt_manager_tws_clear_paired_list();
			bt_manager_tws_disconnect();
		}
#endif
		bt_manager_set_user_visual(true,false,false,0);
		if (APP_TWS_ROLE_PRIMARY == role) {
			app_tws_pawr_adv_start(false);
		} else if (APP_TWS_ROLE_SECONDARY == role) {
			app_tws_pawr_scan_start(APP_TWS_SCAN_TYPE_SCAN_ONLY, false);
		}
	}
	else {
		if(role == APP_TWS_ROLE_SECONDARY) {
			set_nvram_bt_tws_addr(NULL);
			broadcast_set_tws_broadcast_name(NULL);
			bt_manager_pawr_match_mac_set(NULL);
#ifdef CONFIG_APP_TWS_SNOOP
			bt_manager_auto_reconnect_stop();
#endif
		}
		system_app_switch_auracast(0);
		app_tws_status_enable(false);
#ifdef ENABLE_PAWR_APP
		app_tws_pawr_scan_stop();
		app_tws_pawr_adv_stop();
		app_tws_status_set_connected(false);
#endif
#ifdef CONFIG_APP_TWS_SNOOP
		bt_manager_tws_end_pair_search();
		//to clear tws snoop paring information in nvram.
		bt_manager_clear_list(BTSRV_DEVICE_TWS);
		if (bt_manager_tws_get_dev_role() != BTSRV_TWS_NONE) {
			//bt_manager_tws_clear_paired_list();
			bt_manager_tws_disconnect();
		}
#endif

	}

}

#ifdef ENABLE_PAWR_APP
int app_tws_on_pawr_primary_sync(bool synced, u8_t *addr)
{
	u16_t ver;
	char str[13];
    bd_address_t bd_addr;

	int bis = 0;

	SYS_LOG_INF("synced %d", synced);
	print_buffer_lazy("mac", addr, 6);
	if (!app_tws_status_get_enable()) {
		return 0;
	}

	if (synced) {
		app_tws_status_set_connected(true);
		os_delayed_work_cancel(&app_pawr_adv_restart_work);
		ver = app_tws_get_compatible_ver();
#ifdef CONFIG_APP_TWS_SNOOP
		if (BT_PAWR_VERSION_003 == ver) {
			//send pawr commands after synced.
			app_tws_pawr_adv_set_cmd(PAWR_CMD_ENTER_SNOOP_TWS);
			app_tws_wait_pawr_cmd_reply(PAWR_CMD_ENTER_SNOOP_TWS);
			return 0;
		}
#endif
		app_tws_version_switch(ver, APP_TWS_ROLE_PRIMARY);
		if (BT_PAWR_VERSION_002 == ver) {
			app_tws_pawr_adv_set_ver(ver);
			// trasmit
			SYS_LOG_INF("ver_002.master.");
			app_tws_save_tws_mode(APP_TWS_MODE_BIS, APP_TWS_ROLE_PRIMARY);
			if(system_app_get_auracast_mode() == 0){
				if (addr) {
					set_nvram_bt_tws_addr(addr);
                    btif_br_get_local_mac(&bd_addr);
                    hex_to_str(str, bd_addr.val,sizeof(bd_address_t));
					broadcast_set_tws_broadcast_name(str);
				}
				bis = 1;
			}
			ver = media_player_get_version();
			app_tws_pawr_adv_set_media_ver(ver);
			sys_event_notify(SYS_EVENT_TWS_CONNECTED);
			app_tws_sync_effect_output_mode();
		} else if (0 == ver) {
			app_tws_pawr_adv_start(false);
		}
	} else {
		app_tws_stop_pawr_cmd_reply();
		app_tws_status_set_connected(false);
		if(system_app_get_auracast_mode() != 0){
			bis = 1;
			sys_event_notify(SYS_EVENT_TWS_DISCONNECTED);
			app_tws_sync_effect_output_mode();
		}
	}

	return bis;
}

void app_tws_on_pawr_secondary_sync(bool synced, u8_t *addr)
{
	u16_t ver;
	char str[13];

	SYS_LOG_INF("synced %d", synced);
	print_buffer_lazy("mac", addr, 6);

	if (!app_tws_status_get_enable()) {
		return;
	}

	if (synced) {
		app_tws_status_set_connected(true);
		os_delayed_work_cancel(&app_pawr_scan_restart_work);
		app_tws_pawr_rsp_set_ver(app_tws_get_local_tws_ver());
		ver = app_tws_get_compatible_ver();
		app_tws_version_switch(ver, APP_TWS_ROLE_SECONDARY);
		if (BT_PAWR_VERSION_002 == ver) {
			// trasmit
			SYS_LOG_INF("ver_002.slave.");
			bt_manager_set_user_visual(true,false,false,0);
			app_tws_save_tws_mode(APP_TWS_MODE_BIS, APP_TWS_ROLE_SECONDARY);
			if (addr) {
				set_nvram_bt_tws_addr(addr);
				hex_to_str(str, addr, 6);
				broadcast_set_tws_broadcast_name(str);
			}
			sys_event_notify(SYS_EVENT_TWS_CONNECTED);
			app_tws_sync_effect_output_mode();
			system_app_set_auracast_mode(2);
			system_app_launch_add(DESKTOP_PLUGIN_ID_BMR);
		}
	} 
	else {
		if (app_tws_status_get_mode() == APP_TWS_MODE_BIS 
			|| app_tws_status_get_mode() == APP_TWS_MODE_NONE) {
			app_tws_status_set_connected(false);
			app_tws_sync_effect_output_mode();
		}
		if (bt_manager_is_tws_pair_search() ||
			(bt_manager_tws_get_dev_role() != BTSRV_TWS_NONE)) {
			app_tws_pawr_scan_stop();
		} else {
			SYS_LOG_INF("stop sync pa.");
			sys_event_notify(SYS_EVENT_TWS_DISCONNECTED);
			system_app_set_auracast_mode(0);
			app_tws_pawr_scan_stop();
			app_tws_pawr_scan_start(APP_TWS_SCAN_TYPE_SCAN_ONLY, false);
			system_app_launch_switch(DESKTOP_PLUGIN_ID_BMR, DESKTOP_PLUGIN_ID_BR_MUSIC);
		}
	}
}

void app_tws_on_pawr_secondary_syncing(void)
{
	app_pawr_scan_continue();
}
#endif

void app_tws_bis_mode_auto_connect(u8_t role)
{
	SYS_LOG_INF("role %d", role);
	app_tws_status_set_connected(false);
	app_tws_status_enable(true);
	app_tws_mode_select(APP_TWS_MODE_BIS, role);
}

#ifdef CONFIG_APP_TWS_SNOOP
void app_tws_on_snoop_connect(bool connected)
{
	u16_t l_ver;
	int role;
	u16_t media_info = media_player_get_version();

	SYS_LOG_INF("%d", connected);

	if(!connected) {
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		soc_dvfs_unset_level(SOC_DVFS_LEVEL_BR_FULL_PERFORMANCE, "snoop");
#endif
		app_tws_status_set_connected(false);
		//bt_manager_set_user_visual(false,false,false,0);
		return;
	}

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	soc_dvfs_set_level(SOC_DVFS_LEVEL_BR_FULL_PERFORMANCE, "snoop");
#endif

	SYS_LOG_INF("dev num %d", bt_manager_get_connected_dev_num());

	if(bt_manager_get_connected_dev_num() > 0) {
		bt_manager_set_user_visual(true,false,false,0);
		btif_br_disconnect_noactive_device();
	}

	if (0 != system_app_get_auracast_mode()) {
		SYS_LOG_INF("Cannot enter tws in auracast mode.");
		return;
	}

	app_tws_status_enable(true);
	app_tws_status_set_connected(true);

	l_ver = app_tws_get_local_tws_ver();
	role = bt_manager_tws_get_dev_role();

	SYS_LOG_INF("l_ver 0x%x, role %d.", l_ver, role);

	if(role == BTSRV_TWS_MASTER) {
		if(APP_TWS_MODE(l_ver) != APP_TWS_MODE_SNOOP) {
			SYS_LOG_INF("Switch to bis tws.");
			u8_t mode = APP_TWS_MODE_BIS;
			bt_manager_tws_send_message(TWS_USER_APP_EVENT, TWS_EVENT_TWS_MODE_SWITCH, &mode, 1);
			//wait 200ms before switch to bis mode.
			os_sleep(200);
			app_tws_mode_select(APP_TWS_MODE_BIS, APP_TWS_ROLE_PRIMARY);
		} else {
			bt_manager_tws_send_message(TWS_USER_APP_EVENT, TWS_EVENT_MEDIA_VERSION_INFO, (u8_t *)&media_info, sizeof(u16_t));
#ifdef ENABLE_PAWR_APP
			app_tws_pawr_adv_stop();
			app_tws_pawr_scan_stop();
#endif
			app_tws_save_tws_mode(APP_TWS_MODE_SNOOP, APP_TWS_ROLE_PRIMARY);
		}
	} else {
		bt_manager_tws_send_message(TWS_USER_APP_EVENT, TWS_EVENT_MEDIA_VERSION_INFO, (u8_t *)&media_info, sizeof(u16_t));
		app_tws_pawr_adv_stop();
		app_tws_pawr_scan_stop();
		app_tws_save_tws_mode(APP_TWS_MODE_SNOOP, APP_TWS_ROLE_SECONDARY);
	}
}
#endif

void app_tws_storage_clear(void)
{
	set_nvram_bt_tws_addr(NULL);
	app_tws_save_tws_mode(APP_TWS_MODE_NONE, APP_TWS_ROLE_PRIMARY);
}

#ifdef CONFIG_APP_TWS_SNOOP
void app_tws_on_source_switch(bool snoop_support)
{
	u8_t mode;
	u16_t ver;

	mode = app_tws_status_get_mode();
	SYS_LOG_INF("snoop %d, cur mode %d", snoop_support, mode);

	if(!app_tws_status_get_connected()) {
		ver = app_tws_get_local_tws_ver();
		SYS_LOG_INF("ver 0x%x", ver);
		app_tws_pawr_set_ext_adv(ver);
		app_tws_pawr_adv_set_ver(ver);
		SYS_LOG_INF("not connected.");
		return;
	}

	if(mode == APP_TWS_MODE_SNOOP) {
		if(!snoop_support) {
			SYS_LOG_INF("Switch to bis tws.");
			u8_t mode = APP_TWS_MODE_BIS;
			bt_manager_tws_send_message(TWS_USER_APP_EVENT, TWS_EVENT_TWS_MODE_SWITCH, &mode, 1);
			//wait 200ms before switch to bis mode.
			os_sleep(200);
			app_tws_mode_select(APP_TWS_MODE_BIS, APP_TWS_ROLE_PRIMARY);
		}
	} else if(mode == APP_TWS_MODE_BIS) {
		if(snoop_support) {
			system_app_set_auracast_mode(0);
			bt_manager_set_user_visual(true,false,false,0);
			//PAWR command can not be sent when pawr is not synced.
			app_tws_pawr_adv_set_cmd(PAWR_CMD_ENTER_SNOOP_TWS);
			app_tws_wait_pawr_cmd_reply(PAWR_CMD_ENTER_SNOOP_TWS);
		}
	}
}
#endif

void app_tws_send_app_mode_switch_msg(u8_t mode, u8_t role)
{
	SYS_LOG_INF("mode %d, role %d", mode, role);
	system_app_send_input_event(MSG_TWS_MODE_SWITCH, (mode<<8)|role);
}

void app_tws_mode_switch(u8_t mode, u8_t role)
{
	// sleep a while for tws command reply before pawr scan stop.
	if(APP_TWS_MODE_NONE == mode && APP_TWS_ROLE_SECONDARY == role) {
		os_sleep(100);
	}
	app_tws_mode_select(mode, role);
}

void app_tws_exit(void)
{
	SYS_LOG_INF("");
#ifdef ENABLE_PAWR_APP
	app_tws_pawr_adv_stop();
	app_tws_pawr_scan_stop();
#endif

#ifdef CONFIG_APP_TWS_SNOOP
	bt_manager_tws_end_pair_search();
	if (bt_manager_tws_get_dev_role() != BTSRV_TWS_NONE)
	{
		// bt_manager_tws_clear_paired_list();
		bt_manager_tws_disconnect();
	}
#endif
}

#ifdef ENABLE_PAWR_APP

#define INVALID_VOL 0xFF

int pawr_response_cmd(u8_t seq, u8_t cmd, u8_t reply)
{
	u8_t buf[10];
	u8_t offset = 0;

	SYS_LOG_INF("seq %d cmd %d reply %d", seq, cmd, reply);

	//UUID
	buf[offset++] = SERIVCE_UUID & 0xFF;
	buf[offset++] = SERIVCE_UUID >> 8;

	//cmd reply
	buf[offset++] = 4;
	buf[offset++] = PAWR_DT_CMD_REPLY;
	buf[offset++] = seq;
	buf[offset++] = cmd;
	buf[offset++] = reply;

	bt_manager_pawr_vnd_rsp_send(buf, offset, BT_DATA_MANUFACTURER_DATA);
	return 0;
}

int pawr_response_media_version(void)
{
	u8_t buf[10];
	u8_t offset = 0;
	u16_t ver;

	ver = media_player_get_version();
	SYS_LOG_INF("ver 0x%x", ver);

	//UUID
	buf[offset++] = SERIVCE_UUID & 0xFF;
	buf[offset++] = SERIVCE_UUID >> 8;

	//cmd reply
	buf[offset++] = 3;
	buf[offset++] = PAWR_DT_MEDIA_VERSION;
	buf[offset++] = ver & 0xFF;
	buf[offset++] = (ver>>8) & 0xFF;

	bt_manager_pawr_vnd_rsp_send(buf, offset, BT_DATA_MANUFACTURER_DATA);
	return 0;
}


int pawr_response_key_event(u32_t key_event)
{
	static u8_t seq = 1;
	u8_t buf[10];
	u8_t offset = 0;

	SYS_LOG_INF("seq %d key 0x%x", seq, key_event );

	//UUID
	buf[offset++] = SERIVCE_UUID & 0xFF;
	buf[offset++] = SERIVCE_UUID >> 8;

	//key event
	buf[offset++] = 6;
	buf[offset++] = PAWR_DT_KEY;
	buf[offset++] = seq++;
	buf[offset++] = key_event&0xFF;
	buf[offset++] = (key_event>>8)&0xFF;
	buf[offset++] = (key_event>>16)&0xFF;
	buf[offset++] = (key_event>>24)&0xFF;

	bt_manager_pawr_vnd_rsp_send(buf, offset, BT_DATA_MANUFACTURER_DATA);
	return 0;
}

//BMR sends volume in response of PAWR
int pawr_response_vol(u8_t vol100)
{
	u8_t buf[16];
	u8_t offset = 0;

	SYS_LOG_INF("vol %d", vol100);

	//UUID
	buf[offset++] = SERIVCE_UUID & 0xFF;
	buf[offset++] = SERIVCE_UUID >> 8;

	if (vol100 != INVALID_VOL) {
		//volume
		buf[offset++] = 2;
		buf[offset++] = PAWR_DT_VOLUME;
		buf[offset++] = vol100;
	}

	bt_manager_pawr_vnd_rsp_send(buf, offset, BT_DATA_MANUFACTURER_DATA);
	return 0;
}

int pawr_sync_volume(u8_t sync_vol)
{
	static u8_t last_type = AUDIO_STREAM_DEFAULT;
	static u8_t synced_vol = INVALID_VOL;
	u8_t type = AUDIO_STREAM_DEFAULT;

	int cur_plugin_id = desktop_manager_get_plugin_id();

	if (DESKTOP_PLUGIN_ID_BR_MUSIC == cur_plugin_id) {
		type = AUDIO_STREAM_SOUNDBAR;
	} else if (DESKTOP_PLUGIN_ID_UAC == cur_plugin_id) {
		type = AUDIO_STREAM_USOUND;
	} else if (DESKTOP_PLUGIN_ID_SPDIFRX_IN == cur_plugin_id) {
		type = AUDIO_STREAM_SPDIF_IN;
	} else if (DESKTOP_PLUGIN_ID_I2SRX_IN == cur_plugin_id) {
		type = AUDIO_STREAM_I2SRX_IN;
	} else if (DESKTOP_PLUGIN_ID_LINE_IN == cur_plugin_id) {
		type = AUDIO_STREAM_LINEIN;
	} else if (DESKTOP_PLUGIN_ID_BMR == cur_plugin_id) {
		type = AUDIO_STREAM_SOUNDBAR;
	}
	if (type != last_type) {
		last_type = type;
		synced_vol = INVALID_VOL;
	}

	//Sync remote volume only one time to avoid shielding local volume change.
	if (synced_vol != sync_vol) {
		SYS_LOG_INF("sync %d, prev=%d", sync_vol, synced_vol);
		if(sync_vol != system_volume_get(type)) {
			SYS_LOG_INF("sync to %d", sync_vol);
			if(AUDIO_STREAM_USOUND == type) {
#ifdef CONFIG_USOUND_APP
				extern void usound_sync_tws_vol(u8_t level);
				usound_sync_tws_vol(sync_vol);
#endif
			} else {
				system_volume_set(type, sync_vol, false);
			}
		}
		synced_vol = sync_vol;
	}

	return 0;
}

#endif

#else
#include <os_common_api.h>
#include "app_tws.h"
bool app_tws_status_get_enable(void)
{
	return false;
}

u8_t app_tws_status_get_mode(void)
{
	return APP_TWS_MODE_NONE;
}

u8_t app_tws_status_get_role(void)
{
	return APP_TWS_ROLE_PRIMARY;
}

bool app_tws_status_get_connected(void)
{
	return false;
}

#endif
