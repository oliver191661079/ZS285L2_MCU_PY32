/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt manager.
 */
#define SYS_LOG_NO_NEWLINE
#define SYS_LOG_DOMAIN "bt manager"

#include <os_common_api.h>

#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <soc.h>
#include <sys_manager.h>
#include <msg_manager.h>
#include <mem_manager.h>
#include <bt_manager.h>
#include <bt_manager_inner.h>
#include <sys_event.h>
#include <btservice_api.h>
#include <shell/shell.h>
#include <acts_bluetooth/host_interface.h>
#include <property_manager.h>
#ifdef CONFIG_BT_BAS
#include <acts_bluetooth/services/bas.h>
#endif
#ifdef CONFIG_BT_HRS
#include <acts_bluetooth/services/hrs.h>
#endif
#ifdef CONFIG_BT_DIS
#include <acts_bluetooth/services/dis.h>
#endif
#ifdef CONFIG_BT_HIDS
#include <acts_bluetooth/services/hog.h>
#endif
#ifdef CONFIG_BT_CTS
#include <acts_bluetooth/services/cts.h>
#endif

#define PTS_TEST_SHELL_MODULE		"pts"

typedef enum
{
	PTS_MODE_DISABLE = 0,
	PTS_MODE_ENABLE = 1,
	PTS_MODE_INVALID = 0xFF,
} bt_pts_mode_e;

static uint8_t bt_pts_mode = PTS_MODE_INVALID;

uint8_t bt_pts_is_enabled(void)
{
	if (bt_pts_mode != PTS_MODE_INVALID) {
		return bt_pts_mode;
	}

#if defined(CONFIG_PROPERTY)
	int len = property_get(CFG_BT_PTS_MODE, &bt_pts_mode, sizeof(bt_pts_mode));
	if (len != sizeof(bt_pts_mode)) {
		SYS_LOG_ERR("pts mode get fail:%d\n",len);
		bt_pts_mode = PTS_MODE_DISABLE;
	} else {
		SYS_LOG_INF("mode:%d\n",bt_pts_mode);
	}
#endif

	return bt_pts_mode;
}

static int pts_create_connect(uint8_t a2dp, uint8_t avrcp, uint8_t hfp)
{
	int cnt;
	struct autoconn_info info[3];

	memset(info, 0, sizeof(info));
	cnt = btif_br_get_auto_reconnect_info(info, 3);
	if (cnt == 0) {
		SYS_LOG_WRN("Never connect to pts dongle\n");
		return -1;
	}

    for (int i = 0; i < 3; i++) {
        if ((info[i].addr_valid) && (info[i].tws_role == 0)){
            info[i].addr_valid = 1;
            info[i].a2dp = a2dp;
            info[i].hfp = hfp;
            info[i].avrcp = avrcp;
            info[i].hfp_first = hfp ? 1 : 0;
            btif_br_set_auto_reconnect_info(info, 3);
#ifdef CONFIG_PROPERTY
            property_flush(NULL);
#endif
            bt_manager_startup_reconnect();
            break;
        }
    }

	return 0;
}


static int pts_connect_acl(int argc, char *argv[])
{
	return pts_create_connect(0, 0, 0);
}

static int pts_connect_acl_a2dp_avrcp(int argc, char *argv[])
{
	return pts_create_connect(1, 1, 0);
}

#ifdef CONFIG_BT_HFP_HF
static int pts_connect_acl_hfp(int argc, char *argv[])
{
	return pts_create_connect(0, 0, 1);
}
#endif

static int pts_connect_a2dp(int argc, char *argv[])
{
	return pts_create_connect(1, 0, 0);
}

static int pts_connect_avrcp(int argc, char *argv[])
{
	return pts_create_connect(0, 1, 0);
}

#ifdef CONFIG_BT_HFP_HF
static int pts_hfp_cmd(int argc, char *argv[])
{
	char *cmd;

	if (argc < 2) {
		SYS_LOG_INF("Used: pts hfp_cmd xx");
		return -EINVAL;
	}

	cmd = argv[1];
	SYS_LOG_INF("hfp cmd:%s", cmd);

	/* AT cmd
	 * "ATA"			Answer call
	 * "AT+CHUP"		rejuet call
	 * "ATD1234567;"	Place a Call with the Phone Number Supplied by the HF.
	 * "ATD>1;"			Memory Dialing from the HF.
	 * "AT+BLDN"		Last Number Re-Dial from the HF.
	 * "AT+CHLD=0"	Releases all held calls or sets User Determined User Busy (UDUB) for a waiting call.
	 * "AT+CHLD=x"	refer HFP_v1.7.2.pdf.
	 * "AT+NREC=x"	Noise Reduction and Echo Canceling.
	 * "AT+BVRA=x"	Bluetooth Voice Recognition Activation.
	 * "AT+VTS=x"		Transmit DTMF Codes.
	 * "AT+VGS=x"		Volume Level Synchronization.
	 * "AT+VGM=x"		Volume Level Synchronization.
	 * "AT+CLCC"		List of Current Calls in AG.
	 * "AT+BTRH"		Query Response and Hold Status/Put an Incoming Call on Hold from HF.
	 * "AT+CNUM"		HF query the AG subscriber number.
	 * "AT+BIA="		Bluetooth Indicators Activation.
	 * "AT+COPS?"		Query currently selected Network operator.
	 */

	if (btif_pts_send_hfp_cmd(cmd)) {
		SYS_LOG_WRN("Not ready\n");
	}
	return 0;
}

static int pts_hfp_connect_sco(int argc, char *argv[])
{
	if (btif_pts_hfp_active_connect_sco()) {
		SYS_LOG_WRN("Not ready\n");
	}

	return 0;
}

static int bt_pts_hfp_hf_connect(void)
{
	int cnt,i;
	static struct autoconn_info info[3];

    memset(info, 0, sizeof(info));
    cnt = btif_br_get_auto_reconnect_info(info, 3);
    if (cnt == 0) {
        SYS_LOG_WRN("Never connect to pts dongle\n");
        return -1;
    }

    for (i = 0; i < 3; i++) {
        if ((info[i].addr_valid) && (info[i].tws_role == 0)) {
            break;
        }
    }

    if(i == 3){
        return -1;
    }

    return btif_hfp_hf_connect(&info[i].addr);
}


static int pts_hfp_hf_test(int argc, char *argv[])
{
	char *cmd;

	if (argc < 2) {
		SYS_LOG_INF("Used: pts hfp_hf xx\n");
		return -EINVAL;
	}

	cmd = argv[1];
	SYS_LOG_INF("hfp cmd:%s\n", cmd);
	if (!strcmp(cmd, "conn")) {
		bt_pts_hfp_hf_connect();
	}

	return 0;
}
#endif

static int pts_disconnect(int argc, char *argv[])
{
	btif_br_disconnect_device(BTSRV_DISCONNECT_ALL_MODE);
	return 0;
}

static int pts_a2dp_test(int argc, char *argv[])
{
	char *cmd;

	if (argc < 2) {
		SYS_LOG_INF("Used: pts a2dp xx\n");
		return -EINVAL;
	}

	cmd = argv[1];
	SYS_LOG_INF("a2dp cmd:%s\n", cmd);

	if (!strcmp(cmd, "delayreport")) {
		bt_manager_a2dp_send_delay_report(1000);	/* Delay report 100ms */
	}

	return 0;
}

static int pts_avrcp_test(int argc, char *argv[])
{
	char *cmd;
	u8_t value;

	if (argc < 2) {
		SYS_LOG_INF("Used: pts avrcp xx\n");
		return -EINVAL;
	}

	cmd = argv[1];
	SYS_LOG_INF("avrcp cmd:%s", cmd);

	if (!strcmp(cmd, "playstatus")) {
		btif_pts_avrcp_get_play_status();
	} else if (!strcmp(cmd, "pass")) {
		if (argc < 3) {
			SYS_LOG_INF("Used: pts avrcp pass 0xXX\n");
			return -EINVAL;
		}
		value = strtoul(argv[2], NULL, 16);
		if (0x41 == value) {
			value = BTSRV_AVRCP_CMD_VOLUMEUP;
		} else if (0x41 == value) {
			value = BTSRV_AVRCP_CMD_VOLUMEDOWN;
		} else if (0x44 == value) {//play
			value = BTSRV_AVRCP_CMD_PLAY;
		} else if (0x46 == value) {//pause
			value = BTSRV_AVRCP_CMD_PAUSE;
		} else if (0x4B == value) {//forward
			value = BTSRV_AVRCP_CMD_FORWARD;
		} else if (0x4C == value) {//backward
			value = BTSRV_AVRCP_CMD_BACKWARD;
		}
		btif_avrcp_send_command(value);
	} else if (!strcmp(cmd, "volume")) {
		if (argc < 3) {
			SYS_LOG_INF("Used: pts avrcp volume 0xXX\n");
			return -EINVAL;
		}
		value = strtoul(argv[2], NULL, 16);
		btif_avrcp_notify_volume_change(value);
	} else if (!strcmp(cmd, "regnotify")) {
		btif_pts_avrcp_reg_notify_volume_change();
	} else if (!strcmp(cmd, "setabsvol")) {
		if (argc < 3) {
			SYS_LOG_INF("Used: pts avrcp setabsvol 0xXX\n");
			return -EINVAL;
		}
		value = strtoul(argv[2], NULL, 16);
		btif_avrcp_set_absolute_volume(&value, 1);
	} else if (!strcmp(cmd, "getele")) {
		btif_avrcp_get_id3_info();
	}

	return 0;
}

#ifdef CONFIG_BT_SPP
static uint8_t spp_chnnel;

static void pts_spp_connect_failed_cb(uint8_t channel)
{
	SYS_LOG_INF("channel:%d\n", channel);
	spp_chnnel = 0;
}

static void pts_spp_connected_cb(uint8_t channel, uint8_t *uuid)
{
	SYS_LOG_INF("channel:%d\n", channel);
	spp_chnnel = channel;
}

static void pts_spp_receive_data_cb(uint8_t channel, uint8_t *data, uint32_t len)
{
	SYS_LOG_INF("channel:%d data len %d\n", channel, len);
}

static void pts_spp_disconnected_cb(uint8_t channel)
{
	SYS_LOG_INF("channel:%d\n", channel);
	spp_chnnel = 0;
}

static const struct btmgr_spp_cb pts_spp_cb = {
	.connect_failed = pts_spp_connect_failed_cb,
	.connected = pts_spp_connected_cb,
	.disconnected = pts_spp_disconnected_cb,
	.receive_data = pts_spp_receive_data_cb,
};

static const uint8_t pts_spp_uuid[16] = {0xFB, 0x34, 0x9B, 0x5F, 0x80, \
	0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x01, 0x11, 0x00, 0x00};

static int bt_pts_spp_connect(void)
{
	int cnt,i;
	struct autoconn_info info[3];

    memset(info, 0, sizeof(info));
    cnt = btif_br_get_auto_reconnect_info(info, 3);
    if (cnt == 0) {
        SYS_LOG_WRN("Never connect to pts dongle\n");
        return -1;
    }

    for (i = 0; i < 3; i++) {
        if ((info[i].addr_valid) && (info[i].tws_role == 0)) {
            break;
        }
    }

    if(i == 3){
        return -1;
    }

    spp_chnnel = bt_manager_spp_connect(&info[i].addr, (uint8_t *)pts_spp_uuid, (struct btmgr_spp_cb *)&pts_spp_cb);
    SYS_LOG_INF("channel:%d\n", spp_chnnel);
    return 0;
}

static int pts_spp_test(int argc, char *argv[])
{
	char *cmd;

	if (argc < 2) {
		SYS_LOG_INF("Used: pts spp xx\n");
		return -EINVAL;
	}

	cmd = argv[1];
	SYS_LOG_INF("spp cmd:%s\n", cmd);

	if (!strcmp(cmd, "register")) {
		bt_manager_spp_reg_uuid((uint8_t *)pts_spp_uuid, (struct btmgr_spp_cb *)&pts_spp_cb);
	} else if (!strcmp(cmd, "connect")) {
		bt_pts_spp_connect();
	} else if (!strcmp(cmd, "disconnect")) {
		if (spp_chnnel) {
			bt_manager_spp_disconnect(spp_chnnel);
		}
	}

	return 0;
}
#endif

static int pts_scan_test(int argc, char *argv[])
{
	char *cmd;

	if (argc < 2) {
		SYS_LOG_INF("Used: pts scan on/off\n");
		return -EINVAL;
	}

	cmd = argv[1];
	SYS_LOG_INF("scan cmd:%s\n", cmd);

	if (!strcmp(cmd, "on")) {
		bt_manager_set_user_visual(true,true,true,0);

	} else if (!strcmp(cmd, "off")) {
		bt_manager_set_user_visual(false,0,0,0);
	}

	return 0;
}

static int pts_clean_test(int argc, char *argv[])
{
	char *cmd;

	if (argc < 2) {
		SYS_LOG_INF("Used: pts clean xxx\n");
		return -EINVAL;
	}

	cmd = argv[1];
	SYS_LOG_INF("clean cmd:%s\n", cmd);

	if (!strcmp(cmd, "linkkey")) {
		btif_br_clean_linkkey();
	}

	return 0;
}

static int pts_auth_test(int argc, char *argv[])
{
	char *cmd;

	if (argc < 2) {
		SYS_LOG_INF("Used: pts auth xxx\n");
		return -EINVAL;
	}

	cmd = argv[1];
	SYS_LOG_INF("auth cmd:%s\n", cmd);

	if (!strcmp(cmd, "register")) {
		btif_pts_register_auth_cb(true);
	} else if (!strcmp(cmd, "unregister")) {
		btif_pts_register_auth_cb(false);
	}

	return 0;
}

#ifdef CONFIG_BT_BAS
static int pts_bas_register(int argc, char *argv[])
{
    return bt_register_bas_svc();
}

static int pts_bas_notify(int argc, char *argv[])
{
	uint8_t value;

	if (argc < 2) {
		SYS_LOG_INF("Used: bas notify xxx\n");
		return -EINVAL;
	}
	value = strtoul(argv[1], NULL, 16);
	bt_bas_set_battery_level(value);

	return 0;
}
#endif

#ifdef CONFIG_BT_HRS
static int pts_hrs_register(int argc, char *argv[])
{
    return bt_register_hrs_svc();
}

static int pts_hrs_notify(int argc, char *argv[])
{
	uint8_t value;

	if (argc < 2) {
		SYS_LOG_INF("Used: hrs notify xxx\n");
		return -EINVAL;
	}
	value = strtoul(argv[1], NULL, 16);
	bt_hrs_notify(value);

	return 0;
}
#endif

#ifdef CONFIG_BT_DIS
static int pts_dis_register(int argc, char *argv[])
{
    return bt_register_dis_svc();
}
#endif

#ifdef CONFIG_BT_HIDS
static int pts_hog_register(int argc, char *argv[])
{
    return bt_register_hog_svc();
}
#endif

#ifdef CONFIG_BT_CTS
static int pts_cts_set_ct(int argc, char *argv[])
{
    uint32_t value = 0;
 
	if (argc < 3) {
		return -EINVAL;
	}

	value = atoi(argv[2]);

	if (!strcmp(argv[1], "year")) {
		bt_cts_set_current_time_year((uint16_t)value);
	} else if (!strcmp(argv[1], "month")) {
		bt_cts_set_current_time_month((uint8_t)value);
	} else if (!strcmp(argv[1], "day")) {
		bt_cts_set_current_time_day((uint8_t)value);
	} else if (!strcmp(argv[1], "hour")) {
		bt_cts_set_current_time_hours((uint8_t)value);
	} else if (!strcmp(argv[1], "min")) {
		bt_cts_set_current_time_minutes((uint8_t)value);
	} else if (!strcmp(argv[1], "sec")) {
		bt_cts_set_current_time_seconds((uint8_t)value);
	} else if (!strcmp(argv[1], "dow")) {
		bt_cts_set_current_time_week((uint8_t)value);
	} else  {
		return -EINVAL;
	}

	return 0;
}

static int pts_cts_set_lti(int argc, char *argv[])
{
	if (argc < 3) {
		return -EINVAL;
	}
#if 0
	if (!strcmp(argv[1], "tz")) {
		gatt_cts.lti.time_zone = atoi(argv[2]);
		gatt_cts.ct.adjust_reason = CT_CHANGE_OF_TIME_ZONE | CT_MANUAL_TIME_UPDATE;
	} else if (!strcmp(argv[1], "dst")) {
		gatt_cts.lti.daylight_saving_time = atoi(argv[2]);
		gatt_cts.ct.adjust_reason = CT_CHANGE_OF_DST | CT_MANUAL_TIME_UPDATE;
	} else  {
		return -EINVAL;
	}
#endif
	return 0;
}

static int pts_cts_set_rti(int argc, char *argv[])
{
	if (argc < 3) {
		return -EINVAL;
	}

#if 0
	if (!strcmp(argv[1], "src")) {
		if (!strcmp(argv[2], "ntp")) {
			gatt_cts.rti.source = TIME_SOURCE_NETWORK_TIME_PROTOCOL;
		} else if (!strcmp(argv[2], "gps")) {
			gatt_cts.rti.source = TIME_SOURCE_GPS;
		} else if (!strcmp(argv[2], "rts")) {
			gatt_cts.rti.source = TIME_SOURCE_RADIO_TIME_SIGNAL;
		} else if (!strcmp(argv[2], "manual")) {
			gatt_cts.rti.source = TIME_SOURCE_MANUAL;
		} else if (!strcmp(argv[2], "ac")) {
			gatt_cts.rti.source = TIME_SOURCE_ATOMIC_CLOCK;
		} else if (!strcmp(argv[2], "cn")) {
			gatt_cts.rti.source = TIME_SOURCE_CELLULAR_NETWORK;
		} else  {
			return -EINVAL;
		}
	} else  {
		return -EINVAL;
	}

	gatt_cts.ct.adjust_reason = CT_EXTERNAL_REFERENCE_TIME_UPDATE;
#endif
	return 0;
}

static int pts_cts_notify(int argc, char *argv[])
{
	bt_cts_notify();
	return 0;
}

static int pts_cts_register(int argc, char *argv[])
{
    return bt_register_cts_svc();
}
#endif

#ifdef CONFIG_BT_BIP_CLIENT

static int pts_btsrv_bipc_callback(btsrv_bipc_evt_t event, uint8_t app_id, void *data)
{
	SYS_LOG_INF("Evt: %d, app_id: %d", event, app_id);
	return 0;
}


static int pts_bip_test(int argc, char *argv[])
{
	char *cmd;
	static uint8_t s_bip_start = 0;

	if (argc < 2) {
		SYS_LOG_INF("Used: pts bip xx\n");
		return -EINVAL;
	}

	if (!s_bip_start) {
		btsrv_bipc_config_t cfg = {0};
		btif_bipc_start(pts_btsrv_bipc_callback, &cfg);
		s_bip_start = 1;
	}

	cmd = argv[1];
	SYS_LOG_INF("bip cmd:%s", cmd);
	bt_mgr_dev_info_t* dev = NULL;
	bt_manager_context_t* ctx = bt_manager_get_context();
	for (uint8_t i = 0; i < 2; i++) {
		if (ctx->dev[i].a2dp_connected || ctx->dev[i].avrcp_connected) {
			dev = &ctx->dev[i];
			break;
		}
	}

	if (!dev) {
		SYS_LOG_ERR("No dev connect");
		return -1;
	}
	if (!strcmp(cmd, "conn")) {
		btsrv_bipc_connect_param_t param;
		memcpy(&param.bd, &dev->addr, sizeof(bd_address_t));
		param.app_id = 5;
		param.psm = atoi(argv[2]);
		btif_bipc_connect(&param);
	} else if (!strcmp(cmd, "getpro")) {
		btif_bipc_get_image_properties(5, argv[2]);
	} else if (!strcmp(cmd, "getimg")) {
		btsrv_bipc_image_desc_t img = {0};
		img.encoding = "JPEG";
		img.pixel.height= 768;
		img.pixel.width = 1024;
		btif_bipc_get_image(5, argv[2], &img);
	}
	return 0;
}

#endif

static int pts_config(int argc, char *argv[])
{
	char *cmd;
    struct bt_manager_context_t*  bt_manager = bt_manager_get_context();

	if (argc < 2) {
		SYS_LOG_INF("Used: pts config on/off \n");
		return -EINVAL;
	}

	cmd = argv[1];
	SYS_LOG_INF(" :%s\n", cmd);
	if (!strcmp(cmd, "on")) {
		bt_pts_mode = PTS_MODE_ENABLE;
		property_set(CFG_BT_PTS_MODE, &bt_pts_mode, 1);
		property_flush(CFG_BT_PTS_MODE);

#ifdef CONFIG_BT_LEA_PTS_TEST
		pts_le_clear_keys();
#endif
        os_delayed_work_cancel(&bt_manager->auto_reconnect_work);
		os_delayed_work_cancel(&bt_manager->auto_reconnect_second_dev_work);
		btif_br_auto_reconnect_stop(BTSRV_STOP_AUTO_RECONNECT_ALL);
		btif_br_disconnect_device(BTSRV_DISCONNECT_ALL_MODE);

		while (btif_br_get_connected_device_num()) {
			os_sleep(10);
		}
		btif_bt_set_pts_config(true);
		printk("\n\n##### PTS Test Prepared Successful #####\n\n");
		//sys_pm_reboot(REBOOT_REASON_GOTO_BQB);
	} else if (!strcmp(cmd, "off")){
		bt_pts_mode = PTS_MODE_DISABLE;
		property_set(CFG_BT_PTS_MODE, &bt_pts_mode, 1);
		property_flush(CFG_BT_PTS_MODE);

		btif_bt_set_pts_config(false);
	}

	return 0;
}

static int pts_reboot(int argc, char *argv[])
{
	char *cmd;

	if (argc < 2) {
		SYS_LOG_INF("Used: pts reboot\n");
		return -EINVAL;
	}

#ifdef CONFIG_BT_LEA_PTS_TEST
	pts_le_clear_keys();
#endif
	cmd = argv[1];
	SYS_LOG_INF("recon:%s\n", cmd);

	if (!strcmp(cmd, "y")) {
	    sys_pm_reboot(REBOOT_REASON_NORMAL);

	} else if (!strcmp(cmd, "n")) {
	    sys_pm_reboot(REBOOT_REASON_GOTO_BQB_ATT);
    }

	return 0;
}

void bt_manager_pts_test_init(void)
{
	if (bt_manager_config_pts_test()) {
		btif_bt_set_pts_config(true);
	}

	if (bt_manager_config_lea_pts_test()) {
#ifdef CONFIG_BT_LEA_PTS_TEST
		bt_manager_le_pts_test_start();
#endif /*CONFIG_BT_LEA_PTS_TEST*/
	}

}

void bt_manager_pts_test_deinit(void)
{
	if (bt_manager_config_lea_pts_test()) {
#ifdef CONFIG_BT_LEA_PTS_TEST
		bt_manager_le_pts_test_stop();
#endif /*CONFIG_BT_LEA_PTS_TEST*/
	}
}

static const struct shell_cmd pts_test_commands[] = {
	{ "reboot", pts_reboot, "pts reboot"},
	{ "config", pts_config, "pts config on/off"},
	{ "connect_acl", pts_connect_acl, "pts active connect acl"},
    { "connect_a2dp", pts_connect_a2dp, "pts active connect acl/a2dp"},
    { "connect_avrcp", pts_connect_avrcp, "pts active connect acl/avrcp"},
	{ "connect_acl_a2dp_avrcp", pts_connect_acl_a2dp_avrcp, "pts active connect acl/a2dp/avrcp"},
	{ "disconnect", pts_disconnect, "pts do disconnect"},
	{ "a2dp", pts_a2dp_test, "pts a2dp test"},
	{ "avrcp", pts_avrcp_test, "pts avrcp test"},
#ifdef CONFIG_BT_SPP
	{ "spp", pts_spp_test, "pts spp test"},
#endif
	{ "scan", pts_scan_test, "pts scan test"},
	{ "clean", pts_clean_test, "pts scan test"},
	{ "auth", pts_auth_test, "pts auth test"},

#ifdef CONFIG_BT_HFP_HF
	{ "connect_acl_hfp", pts_connect_acl_hfp, "pts active connect acl/hfp"},
	{ "hfp_cmd", pts_hfp_cmd, "pts hfp command"},
	{ "hfp_connect_sco", pts_hfp_connect_sco, "pts hfp active connect sco"},
	{ "hfp_hf", pts_hfp_hf_test, "pts hfp hf test"},
#endif

#ifdef CONFIG_BT_BIP_CLIENT
	{ "bip", pts_bip_test, "pts bip test"},
#endif

#ifdef CONFIG_BT_BAS
	{ "bas_reg", pts_bas_register, "bas service register"},
	{ "bas_notify", pts_bas_notify, "bas notify"},
#endif
#ifdef CONFIG_BT_HRS
	{ "hrs_reg", pts_hrs_register, "hrs service register"},
	{ "hrs_notify", pts_hrs_notify, "hrs notify"},
#endif
#ifdef CONFIG_BT_DIS
    { "dis_reg", pts_dis_register, "dis service register"},
#endif
#ifdef CONFIG_BT_HIDS
    { "hog_reg", pts_hog_register, "hog service register"},
#endif
#ifdef CONFIG_BT_CTS
    { "cts_reg", pts_cts_register, "cts service register"},
	{ "cts_set_ct", pts_cts_set_ct, "<year|month|day|hour|min|sec|dow> <val>" },
	{ "cts_set_lti", pts_cts_set_lti, "<tz|dst>" },
	{ "cts_set_rti", pts_cts_set_rti, "<src> <ntp|gps|rts|manual|ac|cn>" },
	{ "cts_notify", pts_cts_notify, "cts notify" },
#endif
	{ NULL, NULL, NULL}
};

SHELL_REGISTER(PTS_TEST_SHELL_MODULE, pts_test_commands);
