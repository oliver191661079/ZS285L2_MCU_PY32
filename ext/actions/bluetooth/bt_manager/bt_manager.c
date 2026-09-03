/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt manager.
 */
//#define SYS_LOG_NO_NEWLINE
#define SYS_LOG_DOMAIN "bt manager"

#include <os_common_api.h>

#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <msg_manager.h>
#include <mem_manager.h>
#include <bt_manager.h>
#include "bt_manager_inner.h"
#include <sys_event.h>
#include <btservice_api.h>
#include <soc_dvfs.h>
#include <acts_bluetooth/host_interface.h>
#include <property_manager.h>
#include <thread_timer.h>
#include "ctrl_interface.h"
#ifdef CONFIG_ACT_EVENT
#include <bt_act_event_id.h>
#include <logging/log_core.h>
LOG_MODULE_REGISTER(bt_br, CONFIG_ACT_EVENT_APP_COMPILE_LEVEL);
#endif
#ifdef CONFIG_BT_CONTROLER_BQB
static bool cur_bqb_mode = false;
int btdrv_get_bqb_mode(void);
#endif

__in_section_unique(BT_MANAGER_BSS) static struct bt_manager_context_t bt_manager_context;

static btmgr_base_config_t bt_manager_base_config;
static btmgr_pair_cfg_t bt_manager_pair_config;
static btmgr_tws_pair_cfg_t bt_manager_tws_pair_config;
static btmgr_feature_cfg_t bt_manager_feature_config;
static btmgr_sync_ctrl_cfg_t bt_manager_sync_ctrl_config;
static btmgr_reconnect_cfg_t bt_manager_reconnect_config;
static btmgr_ble_cfg_t ble_config;
static btmgr_rf_param_cfg_t bt_rf_param_config;

uint8_t phone_controler_role = CONTROLER_ROLE_SLAVE;

static const bt_mgr_event_strmap_t bt_manager_status_map[] =
{
    {BT_STATUS_LINK_NONE,                   STRINGIFY(BT_STATUS_LINK_NONE)},
    {BT_STATUS_WAIT_CONNECT,    			STRINGIFY(BT_STATUS_WAIT_CONNECT)},
    {BT_STATUS_PAIR_MODE,                   STRINGIFY(BT_STATUS_PAIR_MODE)},
    {BT_STATUS_CONNECTED,                   STRINGIFY(BT_STATUS_CONNECTED)},
    {BT_STATUS_DISCONNECTED,                STRINGIFY(BT_STATUS_DISCONNECTED)},
    {BT_STATUS_TWS_PAIRED,                  STRINGIFY(BT_STATUS_TWS_PAIRED)},
	{BT_STATUS_TWS_UNPAIRED,                STRINGIFY(BT_STATUS_TWS_UNPAIRED)},
    {BT_STATUS_TWS_PAIR_SEARCH,             STRINGIFY(BT_STATUS_TWS_PAIR_SEARCH)},
};

static const bt_mgr_event_strmap_t bt_manager_link_event_map[] =
{
    {BT_LINK_EV_ACL_CONNECT_REQ,            STRINGIFY(BT_LINK_EV_ACL_CONNECT_REQ)},
    {BT_LINK_EV_ACL_CONNECTED,    			STRINGIFY(BT_LINK_EV_ACL_CONNECTED)},
    {BT_LINK_EV_ACL_DISCONNECTED,           STRINGIFY(BT_LINK_EV_ACL_DISCONNECTED)},
    {BT_LINK_EV_GET_NAME,                   STRINGIFY(BT_LINK_EV_GET_NAME)},
    {BT_LINK_EV_ROLE_CHANGE,                STRINGIFY(BT_LINK_EV_ROLE_CHANGE)},
    {BT_LINK_EV_SECURITY_CHANGED,           STRINGIFY(BT_LINK_EV_SECURITY_CHANGED)},
    {BT_LINK_EV_HF_CONNECTED,               STRINGIFY(BT_LINK_EV_HF_CONNECTED)},
    {BT_LINK_EV_HF_DISCONNECTED,            STRINGIFY(BT_LINK_EV_HF_DISCONNECTED)},
	{BT_LINK_EV_A2DP_CONNECTED,             STRINGIFY(BT_LINK_EV_A2DP_CONNECTED)},
    {BT_LINK_EV_A2DP_DISCONNECTED,          STRINGIFY(BT_LINK_EV_A2DP_DISCONNECTED)},
	{BT_LINK_EV_AVRCP_CONNECTED,            STRINGIFY(BT_LINK_EV_AVRCP_CONNECTED)},
    {BT_LINK_EV_AVRCP_DISCONNECTED,         STRINGIFY(BT_LINK_EV_AVRCP_DISCONNECTED)},
    {BT_LINK_EV_SPP_CONNECTED,              STRINGIFY(BT_LINK_EV_SPP_CONNECTED)},
    {BT_LINK_EV_SPP_DISCONNECTED,           STRINGIFY(BT_LINK_EV_SPP_DISCONNECTED)},
    {BT_LINK_EV_HID_CONNECTED,              STRINGIFY(BT_LINK_EV_HID_CONNECTED)},
    {BT_LINK_EV_HID_DISCONNECTED,           STRINGIFY(BT_LINK_EV_HID_DISCONNECTED)},
    {BT_LINK_EV_SNOOP_ROLE,                 STRINGIFY(BT_LINK_EV_SNOOP_ROLE)},
    {BT_LINK_EV_A2DP_SINGALING_CONNECTED,   STRINGIFY(BT_LINK_EV_A2DP_SINGALING_CONNECTED)},
};

static const bt_mgr_event_strmap_t bt_manager_service_event_map[] =
{
    {BTSRV_READY,                           STRINGIFY(BTSRV_READY)},
    {BTSRV_LINK_EVENT,    			        STRINGIFY(BTSRV_LINK_EVENT)},
    {BTSRV_DISCONNECTED_REASON,             STRINGIFY(BTSRV_DISCONNECTED_REASON)},
    {BTSRV_REQ_HIGH_PERFORMANCE,            STRINGIFY(BTSRV_REQ_HIGH_PERFORMANCE)},
    {BTSRV_RELEASE_HIGH_PERFORMANCE,        STRINGIFY(BTSRV_RELEASE_HIGH_PERFORMANCE)},
    {BTSRV_REQ_FLUSH_PROPERTY,              STRINGIFY(BTSRV_REQ_FLUSH_PROPERTY)},
    {BTSRV_CHECK_NEW_DEVICE_ROLE,           STRINGIFY(BTSRV_CHECK_NEW_DEVICE_ROLE)},
    {BTSRV_AUTO_RECONNECT_COMPLETE,         STRINGIFY(BTSRV_AUTO_RECONNECT_COMPLETE)},
    {BTSRV_SYNC_AOTO_RECONNECT_STATUS, 		STRINGIFY(BTSRV_SYNC_AOTO_RECONNECT_STATUS)},
	{BTSRV_CHECK_STREAMING_AUDIO_STATUS, 	STRINGIFY(BTSRV_CHECK_STREAMING_AUDIO_STATUS)},
};

static void dev_connect_notify(bd_address_t* addr, uint8_t flag);

const char *bt_manager_evt2str(int num, int max_num, const bt_mgr_event_strmap_t *strmap)
{
	int low = 0;
	int hi = max_num - 1;
	int mid;

	do {
		mid = (low + hi) >> 1;
		if (num > strmap[mid].event) {
			low = mid + 1;
		} else if (num < strmap[mid].event) {
			hi = mid - 1;
		} else {
			return strmap[mid].str;
		}
	} while (low <= hi);

	printk("evt num %d\n", num);

	return "Unknown";
}

#define BT_MANAGER_LINK_EVENTNUM_STRS (sizeof(bt_manager_link_event_map) / sizeof(bt_mgr_event_strmap_t))
const char *bt_manager_link_evt2str(int num)
{
	return bt_manager_evt2str(num, BT_MANAGER_LINK_EVENTNUM_STRS, bt_manager_link_event_map);
}

#define BT_MANAGER_STATUS_STRS (sizeof(bt_manager_status_map) / sizeof(bt_mgr_event_strmap_t))
const char *bt_manager_status_evt2str(int status)
{
	return bt_manager_evt2str(status, BT_MANAGER_STATUS_STRS, bt_manager_status_map);
}

#define BT_MANAGER_SERVICE_EVENTNUM_STRS (sizeof(bt_manager_service_event_map) / sizeof(bt_mgr_event_strmap_t))
const char *bt_manager_service_evt2str(int num)
{
	return bt_manager_evt2str(num, BT_MANAGER_SERVICE_EVENTNUM_STRS, bt_manager_service_event_map);
}

struct bt_manager_context_t* bt_manager_get_context(void)
{
    struct bt_manager_context_t*  bt_manager = &bt_manager_context;

    return bt_manager;
}

btmgr_base_config_t * bt_manager_get_base_config(void)
{
    btmgr_base_config_t*  base_config = &bt_manager_base_config;

    return base_config;
}

btmgr_pair_cfg_t * bt_manager_get_pair_config(void)
{
    btmgr_pair_cfg_t*  pair_config = &bt_manager_pair_config;

    return pair_config;
}

btmgr_tws_pair_cfg_t * bt_manager_get_tws_pair_config(void)
{
    btmgr_tws_pair_cfg_t*  tws_pair_config = &bt_manager_tws_pair_config;

    return tws_pair_config;
}

btmgr_reconnect_cfg_t * bt_manager_get_reconnect_config(void)
{
    btmgr_reconnect_cfg_t*  reconnect_config = &bt_manager_reconnect_config;

    return reconnect_config;
}

btmgr_feature_cfg_t * bt_manager_get_feature_config(void)
{
    btmgr_feature_cfg_t*  feature_config = &bt_manager_feature_config;

    return feature_config;
}

btmgr_sync_ctrl_cfg_t * bt_manager_get_sync_ctrl_config(void)
{
    return &bt_manager_sync_ctrl_config;
}

btmgr_ble_cfg_t *bt_manager_ble_config(void)
{
    return &ble_config;
}

btmgr_rf_param_cfg_t *bt_manager_rf_param_config(void)
{
    return &bt_rf_param_config;
}


static void bt_manager_set_config_info(void)
{
	struct btsrv_config_info cfg;
	btmgr_base_config_t * cfg_base = bt_manager_get_base_config();
	btmgr_pair_cfg_t *cfg_pair = bt_manager_get_pair_config();
	btmgr_tws_pair_cfg_t *cfg_tws_pair = bt_manager_get_tws_pair_config();
	btmgr_feature_cfg_t *cfg_feature = bt_manager_get_feature_config();
	btmgr_sync_ctrl_cfg_t *sync_ctrl_config = bt_manager_get_sync_ctrl_config();

    uint8_t i;

	memset(&cfg, 0, sizeof(cfg));
	cfg.max_conn_num = CONFIG_BT_MAX_BR_CONN;
	cfg.max_phone_num = bt_manager_config_connect_phone_num();
    cfg.cfg_device_num = cfg_feature->sp_device_num;
	//cfg.pts_test_mode = bt_manager_config_pts_test() ? 1 : 0;
	cfg.volume_sync_delay_ms = bt_manager_config_volume_sync_delay_ms();
	cfg.tws_version = get_tws_current_versoin();
	cfg.tws_feature = get_tws_current_feature();
	cfg.pair_key_mode = cfg_tws_pair->pair_key_mode;
	cfg.power_on_auto_pair_search = cfg_tws_pair->power_on_auto_pair_search;
	cfg.default_state_discoverable = cfg_pair->default_state_discoverable;
    cfg.default_state_wait_connect_sec = cfg_pair->default_state_wait_connect_sec;
	cfg.bt_not_discoverable_when_connected = cfg_pair->bt_not_discoverable_when_connected;
	cfg.use_search_mode_when_tws_reconnect = cfg_tws_pair->use_search_mode_when_tws_reconnect;
	cfg.device_l_r_match = cfg_tws_pair->sp_device_l_r_match;
	cfg.sniff_enable = cfg_feature->sp_sniff_enable;
	cfg.idle_enter_sniff_time = cfg_feature->enter_sniff_time;
	cfg.sniff_interval = cfg_feature->sniff_interval;
	cfg.stop_another_when_one_playing = sync_ctrl_config->stop_another_when_one_playing;
	cfg.resume_another_when_one_stopped = sync_ctrl_config->resume_another_when_one_stopped;
	cfg.a2dp_status_stopped_delay_ms = sync_ctrl_config->a2dp_status_stopped_delay_ms;
    cfg.use_search_mode_when_tws_reconnect = cfg_tws_pair->use_search_mode_when_tws_reconnect;
    cfg.clear_link_key_when_clear_paired_list = cfg_feature->sp_clear_linkkey;
    cfg.cfg_support_tws = cfg_feature->sp_tws;
	cfg.exit_sniff_when_bis = cfg_feature->sp_exit_sniff_when_bis;
#ifdef CONFIG_BT_DOUBLE_PHONE_PREEMPTION_MODE
	cfg.double_preemption_mode = 1;
#else
	cfg.double_preemption_mode = 0;
#endif
	btif_base_set_config_info(&cfg);

    for(i = 0; i < BT_MANAGER_MAX_SCAN_MODE; i++){
	    btif_br_set_scan_param((struct bt_scan_parameter *)&cfg_base->scan_params[i]);
    }
}

static struct bt_device_id_info device_id;

int bt_manager_did_register_sdp()
{
	uint16 *cfg_device_id  = bt_manager_config_get_device_id();

	memset(&device_id,0,sizeof(struct bt_device_id_info));
	device_id.id_source = cfg_device_id[0];//1:SIG assigned Device ID Vendor ID value;2:usb
	device_id.product_id = cfg_device_id[2];
	device_id.vendor_id = cfg_device_id[1];//actions
	int ret = btif_did_register_sdp((uint8_t*)&device_id,sizeof(struct bt_device_id_info));
	if (ret) {
		SYS_LOG_INF("Failed %d\n", ret);
	}

	return ret;
}

void bt_manager_btsrv_ready(void)
{
	struct bt_manager_context_t *bt_manager = bt_manager_get_context();

#if 1
	btmgr_base_config_t *cfg_base = bt_manager_get_base_config();
#endif

	bt_manager_set_config_info();

#ifdef CONFIG_BT_A2DP
#if 1
	bt_manager_a2dp_set_sbc_max_bitpool(cfg_base->ad2p_bitpool);
#endif
	bt_manager_a2dp_profile_start();
#endif
#ifdef CONFIG_BT_AVRCP
	bt_manager_avrcp_profile_start();
#endif
#ifdef CONFIG_BT_MANAGER_UART_RX
	printk("[BT] bt_manager_btsrv_ready: calling bt_manager_uart_rx_init dev=%s\n",
	       CONFIG_BT_MANAGER_UART_RX_DEV_NAME);
	bt_manager_uart_rx_init();
#endif
#ifdef CONFIG_BT_HFP_HF
	bt_manager_hfp_sco_start();
	bt_manager_hfp_profile_start();
#endif
#ifdef CONFIG_BT_HFP_AG
	bt_manager_hfp_ag_profile_start();
#endif

#ifdef CONFIG_BT_SPP
	bt_manager_spp_profile_start();
#endif

//#ifdef CONFIG_BT_HID
//	bt_manager_hid_register_sdp();
//	bt_manager_hid_profile_start();
	bt_manager_did_register_sdp();
//#endif

#ifdef CONFIG_BT_BLE
	bt_manager_ble_init();

#ifdef CONFIG_BT_PTS_TEST
	bt_manager_pts_test_init();
#endif /*CONFIG_BT_PTS_TEST*/

#endif /*CONFIG_BT_BLE*/


#if 0
	uint8_t version[200] = {0};

	int ret = bt_manager_bt_read_bt_build_version(version,sizeof(version));
	if(!ret)
		SYS_LOG_INF("version %s\n",version);
	else
		SYS_LOG_INF("get bt controller version failed\n");
#endif

    bt_manager->bt_ready = 1;
#ifdef CONFIG_SRRC_TEST
	bt_manager_set_user_visual(true, false, false, 0);
#endif

	SYS_LOG_INF("bt_ready\n");

	bt_manager_event_notify(BT_READY, NULL, 0);
}

bool bt_manager_is_inited(void)
{
	struct bt_manager_context_t *bt_manager = bt_manager_get_context();
	return (bt_manager->bt_ready == 1);
}


void bt_manager_clear_list(int mode)
{
    SYS_LOG_INF("\n");
	btif_br_clear_list(mode);
	bt_manager_lea_clear_paired_list();
	sys_event_notify(SYS_EVENT_CLEAR_PAIRED_LIST);
}

void bt_manager_clear_bt_info()
{
	SYS_LOG_INF("\n");
	bt_manager_clear_list(BTSRV_DEVICE_ALL);
	bt_manager_clear_dev_volume();
	property_set(CFG_BT_NAME, NULL, 0);
	property_set(CFG_BLE_NAME, NULL, 0);
	property_set(CFG_BT_LOCAL_NAME, NULL, 0);
	property_set(CFG_BLE_AUDIO, NULL, 0);
	property_set(CFG_BT_BROADCAST_NAME, NULL, 0);
	property_set(CFG_LEA_ONOFF_STATUS, NULL, 0);
	property_set(CFG_BT_PTS_MODE, NULL, 0);
}

static void bt_mgr_check_role(struct bt_mgr_dev_info *info){
	if(info->is_tws){
		SYS_LOG_INF("tws\n");
		return;
	}
	/* Advise not to set, just let phone make dicision. */
	int role = phone_controler_role;
	int dev_exp_role = (role == CONTROLER_ROLE_MASTER) ? CONTROLER_ROLE_SLAVE : CONTROLER_ROLE_MASTER;

	if(dev_exp_role != info->dev_role){
		if(role == CONTROLER_ROLE_MASTER){
            SYS_LOG_INF("remote role is master\n");
			btif_br_set_phone_controler_role(&info->addr, CONTROLER_ROLE_MASTER);	/* Set phone controler as master */
		}else if(role == CONTROLER_ROLE_SLAVE){
            SYS_LOG_INF("remote role is slave\n");
			btif_br_set_phone_controler_role(&info->addr, CONTROLER_ROLE_SLAVE);		/* Set phone controler as slave */
		}
		SYS_EVENT_INF(EVENT_BT_LINK_ROLE_SET, info->hdl, role,
						UINT8_ARRAY_TO_INT32(info->addr.val), os_uptime_get_32());
	}
}

static void bt_manager_control_role_check(struct bt_mgr_dev_info *info)
{
	struct bt_manager_context_t *bt_manager = bt_manager_get_context();

    int dev_exp_role = (phone_controler_role == CONTROLER_ROLE_MASTER) ? CONTROLER_ROLE_SLAVE : CONTROLER_ROLE_MASTER;

	SYS_LOG_INF("exp:%d,dev_role:%d\n", dev_exp_role,info->dev_role);

    if(dev_exp_role != info->dev_role){
        if(dev_exp_role == CONTROLER_ROLE_MASTER){
            bt_manager_halt_phone();
            os_delayed_work_submit(&bt_manager->role_switch_work, 50);
            return;
        } else {
            bt_mgr_check_role(info);
        }
    }
}

static void bt_manager_notify_connected(struct bt_mgr_dev_info *info)
{
	uint8_t tmp_flag = 1;

	if (info->is_tws) {
		/* Tws not notify in here*/
		return;
	}

	SYS_LOG_INF("%s,%d %d %d %d\n", (char *)info->name,
	    info->timeout_disconnected,
	    info->auto_reconnect,
	    info->notify_connected,
	    info->deemed_disconnected);

#ifdef ABNORMAL_OFFLINE_SHIELDING_SYSTEM_NOTIFICATION
	if (info->timeout_disconnected == 1 && info->auto_reconnect) {
	   tmp_flag = 0;
	}
#endif

	/* already notify */
	if (info->notify_connected) {
		//ACL isn't disconnected, connecting Bluetooth again
		if (info->deemed_disconnected) {
			info->deemed_disconnected = 0;
			dev_connect_notify(&info->addr, tmp_flag);
			bt_manager_check_phone_connected();
		}
		return;
	}

	info->notify_connected = 1;

	bt_manager_set_status_ext(BT_STATUS_CONNECTED, tmp_flag, &info->addr);

	if (info->snoop_role != BTSRV_SNOOP_SLAVE) {
#ifdef CONFIG_BT_MAP_CLIENT
	/**only for get datatime*/
	if (btmgr_map_time_client_connect(&info->addr)) {
		SYS_LOG_INF("bt map time connect:%s\n", (char *)info->name);
	} else {
		SYS_LOG_INF("bt map time connect:%s failed\n", (char *)info->name);
	}
#endif
	}
}

static void role_switch_work(struct k_work *work)
{
    struct bt_manager_context_t *bt_manager = bt_manager_get_context();
	int num = btif_br_get_connected_phone_num();
	if(!num){
		bt_manager_resume_phone();
	} else {
		os_delayed_work_submit(&bt_manager->role_switch_work, 20);
	}
}

static void tts_delay_work(struct k_work *work)
{
	struct bt_manager_context_t *bt_manager = bt_manager_get_context();
	bt_manager->ios_tts_delay = 0;
}

static void bt_manager_check_disconnect_notify(struct bt_mgr_dev_info *info, uint8_t reason)
{
	struct bt_manager_context_t*  bt_manager = bt_manager_get_context();
    uint8_t phone_num = 0;
	uint8_t tmp_flag = 1;

    if (info->force_disconnect_by_remote) {
        reason = BT_HCI_ERR_REMOTE_USER_TERM_CONN;
    }

	if (info->is_tws) {
		/* Tws not notify in here */
		return;
	}

	if (info->notify_connected) {
		SYS_LOG_INF("reason:%x\n", reason);
		info->deemed_disconnected = 1;
		info->notify_connected = 0;
		bt_manager->dis_reason = reason;		/* Transfer to BT_STATUS_DISCONNECTED */
#ifdef ABNORMAL_OFFLINE_SHIELDING_SYSTEM_NOTIFICATION
		if (reason == 0x08) {
            tmp_flag = 0;
		}
#endif
        if (bt_manager_audio_is_lea_open()) {  // lea connected not nofify tts
            if(bt_manager_get_dev_another_audio_conn(&info->addr,BT_TYPE_BR)){
                tmp_flag = 0;
            }
        }

		bt_manager_set_status(BT_STATUS_DISCONNECTED,tmp_flag);

		phone_num = bt_manager_get_connected_dev_num();
		if( phone_num == 0){
			bt_manager_set_status(BT_STATUS_WAIT_CONNECT,1);
		}
	}

	SYS_LOG_INF("reason:0x%x phone:%d", reason, bt_manager_get_connected_dev_num());
}

/* return 0: accept connect request, other: rejuect connect request
 * Direct callback from bt stack, can't do too much thing in this function.
 */
static int bt_manager_check_connect_req(struct bt_link_cb_param *param)
{

	SYS_LOG_INF("%s connect request\n", param->is_tws ? "Tws" : "Phone");

	SYS_EVENT_INF(EVENT_BT_LINK_ACL_CONNECT_REQ, param->new_dev, param->is_tws,
					UINT8_ARRAY_TO_INT32(param->addr->val), os_uptime_get_32());
	return 0;
}

/* Sample code, just for reference */
#ifdef CONFIG_BT_DOUBLE_PHONE_EXT_MODE
#define SUPPORT_CHECK_DISCONNECT_NONACTIVE_DEV		1
#else
#define SUPPORT_CHECK_DISCONNECT_NONACTIVE_DEV		0
#endif

#if SUPPORT_CHECK_DISCONNECT_NONACTIVE_DEV
static void bt_mgr_check_disconnect_nonactive_dev(struct bt_mgr_dev_info *info)
{
	int i, phone_cnt = 0, tws_cnt = 0;
	struct bt_mgr_dev_info *exp_disconnect_info = NULL;
    struct bt_manager_context_t*  bt_manager = bt_manager_get_context();
	uint16_t hdl = btif_a2dp_get_active_hdl();

	for (i = 0; (i < MAX_MGR_DEV); i++) {
		if(bt_manager->dev[i].used){
			if (bt_manager->dev[i].is_tws) {
				tws_cnt++;
			} else {
				phone_cnt++;
				if (bt_manager->dev[i].hdl != hdl && bt_manager->dev[i].hdl != info->hdl) {
					exp_disconnect_info = &bt_manager->dev[i];
				}
			}
		}
	}

	/* Tws paired */
	if (tws_cnt) {
		if (phone_cnt >= 2) {
			bt_manager_br_disconnect(&info->addr);
		}
		return;
	}

	if (phone_cnt >= 3) {
		if (exp_disconnect_info) {
			bt_manager_br_disconnect(&exp_disconnect_info->addr);
		}
	}
}
#endif

static bool bt_manager_on_acl_disconnected(struct bt_mgr_dev_info *dev_info, uint8_t reason)
{
    if (dev_info == NULL)
    {
        return false;
    }

    /* ���沥��״̬���ڳ�ʱ�Ͽ�������ɹ�ʱ�ָ�����
     */
    if (bt_manager_is_timeout_disconnected(reason))
    {
        dev_info->timeout_disconnected = 1;

        if (!dev_info->need_resume_play)
        {
            dev_info->need_resume_play = dev_info->a2dp_status_playing;
        }
    }
    else if (bt_manager_is_resources_disconnected(reason))
    {
        dev_info->timeout_disconnected = 1;

        if (!dev_info->need_resume_play)
        {
            dev_info->need_resume_play = dev_info->a2dp_status_playing;
        }
    }
    else if (dev_info->halt_phone)
    {
		SYS_LOG_INF("halt phone");
    }
    else
    {
		if(!dev_info->auto_reconnect){
			dev_info->timeout_disconnected = 0;
			dev_info->need_resume_play     = 0;
		}
    }

	SYS_LOG_INF("reason:0x%x td:%d rp:%d ar:%d\n", reason, dev_info->timeout_disconnected,
		dev_info->need_resume_play,dev_info->auto_reconnect);

    return true;
}

//#define GFP_AUTO_TEST

#ifdef GFP_AUTO_TEST
struct thread_timer gfp_auto_test_timer;
uint8_t gfp_auto_enter_pair_mode_valid = 0;
uint8_t gfp_auto_enter_pair_mode_delay = 0;

void gfp_auto_test_handler(struct thread_timer *ttimer,void *expiry_fn_arg)
{
    if(gfp_auto_enter_pair_mode_valid){
        gfp_auto_enter_pair_mode_delay++;
        if(gfp_auto_enter_pair_mode_delay >= 3){
            gfp_ble_mgr_clear_sleep_time();
            bt_manager_enter_pair_mode();
            gfp_auto_enter_pair_mode_delay = 0;
            gfp_auto_enter_pair_mode_valid = 0;
        }
    }
}
#endif

int bt_manager_link_event(void *param)
{
	int ret = 0;
	struct bt_mgr_dev_info *info;
	struct bt_link_cb_param *in_param = param;
	struct bt_manager_context_t*  bt_manager = bt_manager_get_context();

	info = bt_mgr_find_dev_info_by_hdl(in_param->hdl);
	if ((info == NULL) && (in_param->link_event != BT_LINK_EV_ACL_CONNECTED) &&
		(in_param->link_event != BT_LINK_EV_ACL_CONNECT_REQ) &&
		(in_param->link_event != BT_LINK_EV_KEY_MISS)) {
		SYS_LOG_WRN("Already free %d\n", in_param->link_event);
		return ret;
	}

	SYS_LOG_INF("ev:%s hdl:0x%x", bt_manager_link_evt2str(in_param->link_event), in_param->hdl);
	switch (in_param->link_event) {
	case BT_LINK_EV_ACL_CONNECT_REQ:
		//case specification requirements
		btif_br_auto_reconnect_stop_except_device(in_param->addr);
		ret = bt_manager_check_connect_req(in_param);
		break;
	case BT_LINK_EV_ACL_CONNECTED:
		bt_mgr_add_dev_info(in_param->addr, in_param->hdl);
		info = bt_mgr_find_dev_info_by_hdl(in_param->hdl);
		if(info){
			if(in_param->param){
				info->phone_connect_request = 1;
				if(in_param->key_miss){
					info->key_miss = 1;
				}
			}
		}
		break;
	case BT_LINK_EV_ACL_DISCONNECTED:
		bt_manager_on_acl_disconnected(info, in_param->param);
		bt_manager_check_disconnect_notify(info, in_param->param);

		uint8_t acl_param[3] = {0};
		memcpy(&acl_param[0], (uint8_t *)(&in_param->hdl), sizeof(in_param->hdl));
		acl_param[2] = in_param->param;
		bt_manager_audio_conn_event(BT_DISCONNECTED,acl_param, sizeof(acl_param));

        if(info->need_reconnect){
            info->need_reconnect = 0;
            bt_manager_manual_reconnect();
        }

        if(info->self_name_update){
            bt_manager->update_local_name = 0;
            bt_manager->personalized_name_update = 0;
            btif_br_update_devie_name();
        }
        else if(bt_manager->personalized_name_update && !bt_manager->connected_phone_num){
            bt_manager->personalized_name_update = 0;
            btif_br_update_devie_name();
#ifdef CONFIG_BT_SELF_APP
            bt_manager_app_name_update();
#endif
        }

		bt_mgr_free_dev_info(info);
		btmgr_poff_check_phone_disconnected();
#ifdef GFP_AUTO_TEST
        gfp_auto_enter_pair_mode_delay = 0;
        gfp_auto_enter_pair_mode_valid = 1;
#endif
		break;
	case BT_LINK_EV_GET_NAME:
		info->name = in_param->name;
		info->is_tws = in_param->is_tws;
		bt_manager_audio_conn_event(BT_CONNECTED, &in_param->hdl,sizeof(in_param->hdl));
		if (info->is_tws){
			bt_manager_audio_conn_event(BT_TWS_CONNECTION_EVENT, &in_param->hdl,sizeof(in_param->hdl));
		} else {
			//bt_mgr_check_role(info);
		}
		if(info->key_miss){
			bt_manager_audio_conn_event(BT_KEY_MISS, &in_param->hdl,sizeof(in_param->hdl));
		}
#if SUPPORT_CHECK_DISCONNECT_NONACTIVE_DEV
		bt_mgr_check_disconnect_nonactive_dev(info);
#endif
		break;
	case BT_LINK_EV_ROLE_CHANGE:
		info->dev_role = in_param->param;
		SYS_LOG_INF("role change %s\n",in_param->param?"slave":"master");
		SYS_EVENT_INF(EVENT_BT_LINK_ROLE_CHANGE, info->hdl, info->dev_role,
						UINT8_ARRAY_TO_INT32(info->addr.val), os_uptime_get_32());
		break;
	case BT_LINK_EV_KEY_MISS:
		if(bt_manager_is_pair_mode()){
			for (int i = 0; (i < MAX_MGR_DEV); i++) {
				if (bt_manager->dev[i].used && bt_manager->dev[i].key_miss && bt_manager->dev[i].hdl) {
					SYS_LOG_INF("reject conn\n");
					ret = -1;
					break;
				}
			}
		}
		info = bt_mgr_find_dev_info_by_hdl(in_param->hdl);
		if(info && info->phone_connect_request){
			if(!info->key_miss && !info->security){
				info->key_miss = 1;
				bt_manager_audio_conn_event(BT_KEY_MISS, &in_param->hdl,sizeof(in_param->hdl));
			}
		}
		break;
	case BT_LINK_EV_SECURITY_CHANGED:
		bt_manager_audio_conn_event(BT_SECURITY_CHANGED, &in_param->hdl,sizeof(in_param->hdl));
		bt_manager_check_phone_connected();
		info->key_miss = 0;
		info->security = 1;
		SYS_EVENT_INF(EVENT_BT_LINK_SECURITY_CHANGED, info->hdl,
						UINT8_ARRAY_TO_INT32(info->addr.val), os_uptime_get_32());
		break;
	case BT_LINK_EV_HF_CONNECTED:
		info->hf_connected = 1;
		SYS_EVENT_INF(EVENT_BT_LINK_HF_CONNECTED, info->hdl,
						UINT8_ARRAY_TO_INT32(info->addr.val), os_uptime_get_32());
		bt_manager_notify_connected(info);
		break;
	case BT_LINK_EV_HF_DISCONNECTED:
		info->hf_connected = 0;
		SYS_EVENT_INF(EVENT_BT_LINK_HF_DISCONNECTED, info->hdl,
						UINT8_ARRAY_TO_INT32(info->addr.val), os_uptime_get_32());
		break;
	case BT_LINK_EV_A2DP_CONNECTED:
		info->a2dp_connected = 1;
		if(!bt_manager_audio_is_ios_dev(info->hdl) ||
			info->ios_tts_delay) {
			info->ios_tts_delay = 0;
			bt_manager_notify_connected(info);
		}
		bt_manager_control_role_check(info);
		bt_manager_auto_reconnect_resume_play(in_param->hdl);
		SYS_EVENT_INF(EVENT_BT_LINK_A2DP_CONNECTED, info->hdl,
			UINT8_ARRAY_TO_INT32(info->addr.val), os_uptime_get_32());
		break;
	case BT_LINK_EV_A2DP_SINGALING_CONNECTED:
		info->a2dp_singnaling_connected = 1;
		os_delayed_work_cancel(&info->profile_disconnected_delay_work);
		if(bt_manager_audio_is_ios_dev(info->hdl)){
			if (0 == bt_manager->ios_tts_delay)
				bt_manager_notify_connected(info);
			else
				info->ios_tts_delay = 1;
		}
		break;

	case BT_LINK_EV_A2DP_DISCONNECTED:
		info->a2dp_connected = 0;
        info->a2dp_singnaling_connected = 0;
		info->ios_tts_delay = 0;
		SYS_EVENT_INF(EVENT_BT_LINK_A2DP_DISCONNECTED, info->hdl,
						UINT8_ARRAY_TO_INT32(info->addr.val), os_uptime_get_32());
		if (!info->deemed_disconnected) {
			SYS_LOG_INF("Deemed to be disconnected\n");
			info->deemed_disconnected = 1;
		}
		break;

        case BT_LINK_EV_A2DP_CHANNEL_ERR:
        info->need_reconnect = 1;
        bt_manager_br_disconnect(&info->addr);
        break;

	case BT_LINK_EV_AVRCP_CONNECTED:
		info->avrcp_connected = 1;
		SYS_EVENT_INF(EVENT_BT_LINK_AVRCP_CONNECTED, info->hdl,
						UINT8_ARRAY_TO_INT32(info->addr.val), os_uptime_get_32());
		break;
	case BT_LINK_EV_AVRCP_DISCONNECTED:
		info->avrcp_connected = 0;
		SYS_EVENT_INF(EVENT_BT_LINK_AVRCP_DISCONNECTED, info->hdl,
						UINT8_ARRAY_TO_INT32(info->addr.val), os_uptime_get_32());
		break;
	case BT_LINK_EV_SPP_CONNECTED:
		info->spp_connected++;
		SYS_EVENT_INF(EVENT_BT_LINK_SPP_CONNECTED, info->hdl, info->spp_connected,
						UINT8_ARRAY_TO_INT32(info->addr.val), os_uptime_get_32());
		break;
	case BT_LINK_EV_SPP_DISCONNECTED:
		if (info->spp_connected) {
			info->spp_connected--;
			SYS_EVENT_INF(EVENT_BT_LINK_SPP_DISCONNECTED, info->hdl, info->spp_connected,
							UINT8_ARRAY_TO_INT32(info->addr.val), os_uptime_get_32());
		}
		break;
	case BT_LINK_EV_HID_CONNECTED:
		info->hid_connected = 1;
		SYS_EVENT_INF(EVENT_BT_LINK_HID_CONNECTED, info->hdl,
						UINT8_ARRAY_TO_INT32(info->addr.val), os_uptime_get_32());
#ifdef CONFIG_BT_HID
		bt_manager_hid_connected_check_work(info->hdl);
#endif
		break;
	case BT_LINK_EV_HID_DISCONNECTED:
		info->hid_connected = 0;
		SYS_EVENT_INF(EVENT_BT_LINK_HID_DISCONNECTED, info->hdl,
						UINT8_ARRAY_TO_INT32(info->addr.val), os_uptime_get_32());
		break;
	case BT_LINK_EV_SNOOP_ROLE:
		info->snoop_role = in_param->param&0x7;
		if (info->snoop_role == BTSRV_SNOOP_MASTER){
			bt_manager_tws_sync_phone_info(info->hdl, NULL);
		}
		btmgr_tws_poff_check_snoop_disconnect();
		break;
	default:
		break;
	}

	return ret;
}

/* Return 0: phone device; other : tws device
 * Direct callback from bt stack, can't do too much thing in this function.
 */
int bt_manager_check_new_device_role(void *param)
{
#if 1
    struct btsrv_check_device_role_s *cb_param = param;
    uint8_t pre_mac[3];
    btmgr_base_config_t * cfg_base =  bt_manager_get_base_config();
    btmgr_tws_pair_cfg_t * cfg_tws =  bt_manager_get_tws_pair_config();
    uint8_t match_len = cfg_tws->match_name_length;
    bd_address_t bd_addr;

	if (bt_manager_config_get_tws_compare_high_mac()) {
		btif_br_get_local_mac(&bd_addr);
		memcpy(pre_mac,&bd_addr.val[3] ,3);
		if (cb_param->addr.val[5] != pre_mac[2] ||
			cb_param->addr.val[4] != pre_mac[1] ||
			cb_param->addr.val[3] != pre_mac[0]){
			return 0;
		}
	}

    if(match_len > cb_param->len){
        match_len = cb_param->len;
    }
    if (memcmp(cb_param->name, cfg_base->bt_device_name, match_len)) {
        return 0;
    }
    else{
        return 1;
    }
    return 0;
#else
	struct btsrv_check_device_role_s *cb_param = param;
	uint8_t pre_mac[3];

	if (bt_manager_config_get_tws_compare_high_mac()) {
		bt_manager_config_set_pre_bt_mac(pre_mac);
		if (cb_param->addr.val[5] != pre_mac[0] ||
			cb_param->addr.val[4] != pre_mac[1] ||
			cb_param->addr.val[3] != pre_mac[2]) {
			return 0;
		}
	}

#ifdef CONFIG_PROPERTY
	uint8_t name[33];
	memset(name, 0, sizeof(name));
	property_get(CFG_BT_NAME, name, 32);
	if (strlen(name) != cb_param->len || memcmp(cb_param->name, name, cb_param->len)) {
		return 0;
	}
#endif

	return 1;
#endif
}

void bt_manager_auto_reconnect_complete(void)
{
    struct bt_manager_context_t*  bt_manager = bt_manager_get_context();
	btmgr_reconnect_cfg_t *cfg_reconnect = bt_manager_get_reconnect_config();

	SYS_LOG_INF("startup:%d",bt_manager->auto_reconnect_startup);
    for(int i = 0; i < MAX_MGR_DEV; i++){
        //TODO:clear all info data??
        if(bt_manager->dev[i].used){
            bt_manager->dev[i].auto_reconnect = 0;
        }
    }
	bt_manager->auto_reconnect_timeout = 0;

	if(bt_manager->auto_reconnect_startup){
		bt_manager->auto_reconnect_startup = 0;
        if (cfg_reconnect->enter_pair_mode_when_startup_reconnect_fail == 0){
            return;
        }
		if (btif_tws_get_dev_role() == BTSRV_TWS_SLAVE)
		{
			return;
		}
        if (bt_manager_audio_get_cur_dev_num()> 0){
            return;
        }
        sys_event_notify(SYS_EVENT_ENTER_PAIR_MODE);
        bt_manager_enter_pair_mode();
	}
}

static void bt_manager_reconnect_active_halt(void)
{
	struct bt_manager_context_t*  bt_manager = bt_manager_get_context();

	SYS_LOG_INF("halt reconnect :%d",bt_manager->auto_reconnect_timeout);
	for(int i = 0; i < MAX_MGR_DEV; i++){
        if(bt_manager->dev[i].used){
            bt_manager->dev[i].auto_reconnect = 0;
        }
	}
	bt_manager->auto_reconnect_timeout = 0;

}

static int bt_manager_service_callback(btsrv_event_e event, void *param)
{
	int ret = 0;

	SYS_LOG_INF("ev: %s",bt_manager_service_evt2str(event));

	switch (event) {
	case BTSRV_READY:
		bt_manager_btsrv_ready();
		break;
	case BTSRV_LINK_EVENT:
		ret = bt_manager_link_event(param);
		break;
	case BTSRV_DISCONNECTED_REASON:
		bt_manager_disconnected_reason(param);
		break;
	case BTSRV_REQ_HIGH_PERFORMANCE:
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		soc_dvfs_set_level(SOC_DVFS_LEVEL_BR_FULL_PERFORMANCE, "btmanager");
#endif
	break;
	case BTSRV_RELEASE_HIGH_PERFORMANCE:
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		soc_dvfs_unset_level(SOC_DVFS_LEVEL_BR_FULL_PERFORMANCE, "btmanager");
#endif
		break;
	case BTSRV_REQ_FLUSH_PROPERTY:
		//SYS_LOG_INF("Req flush %s\n", (char *)param);
#ifdef CONFIG_PROPERTY
		/* 标记 property 缓存待写 flash（延迟到 media_player 创建时 flush_req_deal 批量写），
		 * 否则 CFG_AUTOCONN_INFO 等设备记录只留在 RAM，关机丢失后无法自动重连。 */
		property_flush_req(param);
#endif
		break;
	case BTSRV_CHECK_NEW_DEVICE_ROLE:
		ret = bt_manager_check_new_device_role(param);
		break;
    case BTSRV_AUTO_RECONNECT_COMPLETE:
		if (1 == (int)param) {
			bt_manager_reconnect_active_halt();
		} else {
			bt_manager_auto_reconnect_complete();
		}
		break;
    case BTSRV_SYNC_AOTO_RECONNECT_STATUS:
        bt_manager_sync_auto_reconnect_status(param);
        break;
	case BTSRV_CHECK_STREAMING_AUDIO_STATUS:
		if(bt_manager_audio_current_stream() != BT_AUDIO_STREAM_NONE) {
			SYS_LOG_INF("now streaming audio");
			ret = -1;
		}
		break;
	case BTSRV_LINK_MISS_UPLAYER_CHECK:
		if(bt_manager_audio_is_lea_open() &&
			bt_manager_audio_has_same_lea_notify_connected(param)) {
			ret = 1;
		}
		break;
	default:
		break;
	}

	return ret;
}

#if 1
void bt_manager_record_halt_phone(void)
{
	struct bt_manager_context_t*  bt_manager = bt_manager_get_context();
	uint8_t i;

	memset(bt_manager->halt_addr, 0, sizeof(bt_manager->halt_addr));
	bt_manager->halt_dev_num = 0;
	for (i = 0; i < MAX_MGR_DEV; i++) {
		if (bt_manager->dev[i].used && !bt_manager->dev[i].is_tws && bt_manager->dev[i].hdl
			&& (bt_manager->dev[i].a2dp_connected || bt_manager->dev[i].a2dp_singnaling_connected || bt_manager->dev[i].hf_connected)) {
			memcpy(&bt_manager->halt_addr[bt_manager->halt_dev_num], &bt_manager->dev[i].addr, sizeof(bd_address_t));
			bt_manager->dev[i].halt_phone = 1;
			bt_manager->dev[i].need_resume_play = bt_manager->dev[i].a2dp_status_playing;
			bt_manager->halt_dev_num++;
		}
	}
}

void bt_manager_ios_tts_delay_set(bool enable)
{
	struct bt_manager_context_t*  bt_manager = bt_manager_get_context();

	if (!bt_manager->tts_delay_work.work.handler) {
		SYS_LOG_ERR("bt manager not init.");
		return;
	}

	os_delayed_work_cancel(&bt_manager->tts_delay_work);
	if (true == enable) {
		os_delayed_work_submit(&bt_manager->tts_delay_work, 10000); // 10s
		bt_manager->ios_tts_delay = 1;
	} else {
		bt_manager->ios_tts_delay = 0;
	}
}

void *bt_manager_get_halt_phone(uint8_t *halt_cnt)
{
	struct bt_manager_context_t*  bt_manager = bt_manager_get_context();
	*halt_cnt = bt_manager->halt_dev_num;
	return bt_manager->halt_addr;
}

uint8_t bt_manager_get_halt_phone_num(void)
{
	struct bt_manager_context_t*  bt_manager = bt_manager_get_context();
	return bt_manager->halt_dev_num + bt_manager_lea_get_halt_phone();
}
#endif

int bt_manager_get_status(void)
{
    struct bt_manager_context_t*  bt_manager = bt_manager_get_context();

	return bt_manager->bt_state;
}

int bt_manager_get_connected_dev_num(void)
{
    struct bt_manager_context_t*  bt_manager = bt_manager_get_context();

	return bt_manager->connected_phone_num;
}

static void dev_connect_notify(bd_address_t* addr, uint8_t flag)
{
	if(bt_manager_audio_has_same_lea_notify_connected(addr)) {
		SYS_LOG_INF("same dev, no tts\n");
		return;
	}

	if (flag) {
		struct bt_manager_context_t*  bt_manager = bt_manager_get_context();
		if (bt_manager->connected_phone_num == 1) {
		   bt_manager_sys_event_notify(SYS_EVENT_BT_CONNECTED);
		} else {
		   bt_manager_sys_event_notify(SYS_EVENT_2ND_CONNECTED);
		}
	}
}

static void lea_dev_connect_notify(bd_address_t* addr, uint8_t flag)
{
	if(bt_manager_audio_has_same_br_notify_connected(addr))
		return;

	if (flag) {
		if (bt_manager_audio_get_cur_dev_num() == 1) {
			bt_manager_sys_event_notify(SYS_EVENT_BT_CONNECTED);
		} else {
			bt_manager_sys_event_notify(SYS_EVENT_2ND_CONNECTED);
		}
	}
}

int bt_manager_set_status_ext(int state,uint8_t flag, bd_address_t* addr)
{
	struct bt_manager_context_t*  bt_manager = bt_manager_get_context();

	SYS_LOG_INF("status:%s,%d", bt_manager_status_evt2str(state),flag);

	switch (state) {
	case BT_STATUS_CONNECTED:
	{
		struct bt_mgr_dev_info *dev_info;
		if (addr) {
			dev_info = bt_mgr_find_dev_info(addr);
			if (dev_info) {
				dev_info->deemed_disconnected = 0;
			}
		}
		bt_manager->connected_phone_num++;
		dev_connect_notify(addr, flag);
		break;
	}
	case BT_STATUS_DISCONNECTED:
	{
		if (bt_manager->connected_phone_num > 0) {
			bt_manager->connected_phone_num--;
			if (bt_manager->dis_reason != 0x16) {
				if (flag) {
				   bt_manager_sys_event_notify(SYS_EVENT_BT_DISCONNECTED);
				}
			}

			if (!bt_manager->connected_phone_num) {
				bt_manager_sys_event_notify(SYS_EVENT_BT_UNLINKED);
			}
		}

		/* restart waiting connect when BT disconnected */
		if (bt_manager_tws_get_dev_role() != BTSRV_TWS_SLAVE &&
			bt_manager->poweroff_state == POFF_STATE_NONE)
		{
			bt_manager_start_wait_connect();
		}
		break;
	}
	case BT_STATUS_TWS_PAIRED:
	{
		if (!bt_manager->tws_mode) {
			bt_manager->tws_mode = 1;
#if 0
			if (btif_tws_get_dev_role() == BTSRV_TWS_MASTER) {
				bt_manager_sys_event_notify(SYS_EVENT_TWS_CONNECTED);
			}
#else
			bt_manager_sys_event_notify(SYS_EVENT_TWS_CONNECTED);
#endif
			bt_manager_event_notify(BT_TWS_CONNECTION_EVENT, NULL, 0);
		}
		break;
	}
	case BT_STATUS_TWS_UNPAIRED:
	{
		if (bt_manager->tws_mode) {
			bt_manager->tws_mode = 0;
#if 1
			if (bt_manager_is_timeout_disconnected(bt_manager->tws_dis_reason)){
				bt_manager_sys_event_notify(SYS_EVENT_TWS_DISCONNECTED);
			}
#endif
			bt_manager_event_notify(BT_TWS_DISCONNECTION_EVENT, NULL, 0);

			/* restart waiting connect when TWS disconnected */
			if (bt_manager->poweroff_state == POFF_STATE_NONE)
			{
				bt_manager_start_wait_connect();
			}
		}
		break;
	}

	case BT_STATUS_TWS_PAIR_SEARCH:
	{
		bt_manager_sys_event_notify(SYS_EVENT_TWS_START_PAIR);
		break;
	}
	case BT_STATUS_WAIT_CONNECT:
	{
		bt_manager_sys_event_notify(SYS_EVENT_BT_WAIT_PAIR);
		break;
	}

	default:
		break;
	}

	bt_manager->bt_state = state;
	bt_manager_check_link_status();

	return 0;
}

int bt_manager_lea_set_status(int state, uint8_t flag, bd_address_t* addr)
{
	struct bt_manager_context_t*  bt_manager = bt_manager_get_context();

	SYS_LOG_INF("status:%s,%d", bt_manager_status_evt2str(state),flag);

	switch (state) {
	case BT_STATUS_CONNECTED:
	{
		lea_dev_connect_notify(addr, flag);
		break;
	}
	case BT_STATUS_DISCONNECTED:
	{
		if (flag) {
			bt_manager_sys_event_notify(SYS_EVENT_BT_DISCONNECTED);
		}
		break;
	}
	case BT_STATUS_TWS_PAIRED:
	{
		//TODO
		break;
	}
	case BT_STATUS_TWS_UNPAIRED:
	{
		//TODO
		break;
	}
	case BT_STATUS_WAIT_CONNECT:
	{
		//TODO
		break;
	}

	default:
		break;
	}

	bt_manager->bt_state = state;
	bt_manager_check_link_status();

	return 0;
}

int bt_manager_set_status(int state,uint8_t flag)
{
	return bt_manager_set_status_ext(state, flag, NULL);
}

void bt_manager_set_stream_type(uint8_t stream_type)
{
#ifdef CONFIG_TWS
	bt_manager_tws_set_stream_type(stream_type);
#endif
}

void bt_manager_set_codec(uint8_t codec)
{
#ifdef CONFIG_TWS
	bt_manager_tws_set_codec(codec);
#endif
}

static void wait_connect_work_callback(struct k_work *work)
{
    bt_manager_end_wait_connect();
}

static void pair_mode_work_callback(struct k_work *work)
{
    struct bt_manager_context_t *bt_manager = bt_manager_get_context();

	bt_manager->pair_mode_work_valid = false;

    if((bt_manager->pair_mode_state != BT_PAIR_MODE_STATE_NONE) &&
		(bt_manager->pair_mode_state != BT_PAIR_MODE_RUNNING)){
	    bt_manager_enter_pair_mode_ex();
    }
    else{
        bt_manager_end_pair_mode();
    }
}

static void tws_pair_search_work_callback(struct k_work *work)
{
    bt_manager_tws_end_pair_search();
}

static void clear_paired_list_work(struct k_work *work)
{
    struct bt_manager_context_t *bt_manager = bt_manager_get_context();
	bt_manager->clear_paired_list_work_valid = false;

    if(bt_manager->clear_paired_list_state != BT_CLEAR_PAIRED_LIST_NONE){
	    bt_manager_clear_paired_list_ex();
    }
}

static void startup_reconnect_work(struct k_work *work)
{
    bt_manager_startup_reconnect();
}

#if 0
static void bt_manager_set_bt_drv_param(void)
{
#if 1
	btdrv_init_param_t param;
	btmgr_base_config_t *cfg = bt_manager_get_base_config();
	btmgr_rf_param_cfg_t *rf_prarm_config = bt_manager_rf_param_config();

	param.set_hosc_cap = cfg->force_default_hosc_capacity;
	param.hosc_capacity = cfg->default_hosc_capacity;

	param.set_max_rf_power = 1;
	param.bt_max_rf_tx_power = cfg->bt_max_rf_tx_power;
	param.set_ble_rf_power = 1;
	param.ble_rf_tx_power = cfg->ble_rf_tx_power;
	memcpy(&param.rf_param, rf_prarm_config, sizeof(param.rf_param));

	btdrv_set_init_param(&param);
#endif
}
#endif




#define BT_TEMP_COMP_CHECK_TIMER_MS 1000


#if 0
static void bt_temp_comp_timer_handler(struct k_work* work)
{
	struct bt_manager_context_t* bt_manager = bt_manager_get_context();

    const struct device* adc_dev = device_get_binding(CONFIG_PMUADC_NAME);

    if (adc_dev == NULL)
        return;

    if (bt_manager->bt_temp_comp_stage == 0)
    {
        struct adc_channel_cfg adc_cfg = { 0, };

        adc_cfg.channel_id = PMUADC_ID_SENSOR;
        adc_channel_setup(adc_dev, &adc_cfg);

        bt_manager->bt_temp_comp_stage = 1;
        os_delayed_work_submit(&bt_manager->bt_temp_comp_timer, 1);
    }
    else
    {
        struct adc_sequence adc_data = { 0, };
        uint16_t adc_buf[2];
        int temp;

        adc_data.channels    = BIT(PMUADC_ID_SENSOR);
        adc_data.buffer      = adc_buf;
        adc_data.buffer_size = sizeof(adc_buf);

        adc_read(adc_dev, &adc_data);
        temp = 1200 - 1600 * adc_buf[0] / 4096;

        if (bt_manager->bt_temp_comp_stage == 1)
        {
            SYS_LOG_INF("0x%x, %d.%d", adc_buf[0], temp / 10, temp % 10);

            bt_manager->bt_temp_comp_stage = 2;
            bt_manager->bt_comp_last_temp  = temp;
        }
        else if (abs(temp - bt_manager->bt_comp_last_temp) >= 150)
        {
            SYS_LOG_WRN("0x%x, %d.%d, DO_TEMP_COMP", adc_buf[0], temp / 10, temp % 10);
            bt_manager->bt_comp_last_temp = temp;

            bt_manager_bt_set_apll_temp_comp(true);
            bt_manager_bt_do_apll_temp_comp();
        }

        os_delayed_work_submit(&bt_manager->bt_temp_comp_timer, BT_TEMP_COMP_CHECK_TIMER_MS);
    }
}
#endif


void bt_manager_get_phone_controller_role(void)
{
    int tmp_controller_role;

	tmp_controller_role = property_get_int("PHONE_CONTROLER_ROLE", CONTROLER_ROLE_MASTER);

	phone_controler_role = tmp_controller_role;
	SYS_LOG_INF("phone_controller_role:%d \n",phone_controler_role);
}

#ifdef CONFIG_BT_CONTROLER_BQB
bool bt_manager_get_bqb_mode(void)
{
	return cur_bqb_mode;
}
#endif

#ifdef CONFIG_BT_CONTROLER_BQB_SYS
static void btdrv_bqb_test_cb(void *buf, int len, uint8_t type)
{
	SYS_LOG_INF("#.\n");
}
#endif

int bt_manager_init(bt_manager_event_callback_t event_callback)
{
	int ret = 0;
	struct bt_manager_context_t *bt_manager = bt_manager_get_context();

#ifdef CONFIG_BT_CONTROLER_BQB_SYS
	typedef void (*btdrv_rx_cb_t)(void *buf, int len, uint8_t type);
	int btdrv_init(btdrv_rx_cb_t rx_cb);
	if (btdrv_get_bqb_mode() > 0) {
		cur_bqb_mode = true;
		bt_manager_context.cur_bqb_mode = 1;
		btdrv_init(btdrv_bqb_test_cb);
		return 0;
	}
#endif
    bt_manager_get_phone_controller_role();

	memset(bt_manager, 0, sizeof(struct bt_manager_context_t));

	bt_manager->event_callback = event_callback;

	bt_manager_init_dev_volume();

//	bt_manager_set_bt_drv_param();
	bt_manager_check_mac_name();

	os_delayed_work_init(&bt_manager->wait_connect_work, wait_connect_work_callback);
	os_delayed_work_init(&bt_manager->pair_mode_work, pair_mode_work_callback);
	os_delayed_work_init(&bt_manager->tws_pair_search_work, tws_pair_search_work_callback);
	os_delayed_work_init(&bt_manager->clear_paired_list_work, clear_paired_list_work);
	os_delayed_work_init(&bt_manager->role_switch_work, role_switch_work);
	os_delayed_work_init(&bt_manager->tts_delay_work, tts_delay_work);
	os_delayed_work_init(&bt_manager->auto_reconnect_work, startup_reconnect_work);

	os_mutex_init(&bt_manager->poweroff_mutex);
	os_delayed_work_init(&bt_manager->poweroff_proc_work, btmgr_poff_state_proc_work);

	bt_manager->pair_status = BT_PAIR_STATUS_NONE;
	bt_manager_set_status(BT_STATUS_LINK_NONE,1);


	btif_base_register_processer();
#ifdef CONFIG_BT_HFP_HF
	btif_hfp_register_processer();
#endif
#ifdef CONFIG_BT_HFP_AG
	btif_hfp_ag_register_processer();
#endif
#ifdef CONFIG_BT_A2DP
	btif_a2dp_register_processer();
#endif
#ifdef CONFIG_BT_AVRCP
	btif_avrcp_register_processer();
#endif
#ifdef CONFIG_BT_SPP
	btif_spp_register_processer();
#endif
#ifdef CONFIG_BT_PBAP_CLIENT
	btif_pbap_register_processer();
#endif
#ifdef CONFIG_BT_MAP_CLIENT
	btif_map_register_processer();
#endif
#if CONFIG_BT_HID
	btif_hid_register_processer();
#endif
#ifdef CONFIG_TWS
	btif_tws_register_processer();
#endif
	btif_audio_register_processer();
	btif_did_register_processer();
   
#ifdef CONFIG_BT_LEA_PTS_TEST
	if (bt_manager_config_lea_pts_test()) {
		btif_pts_register_processer();
	}
#endif

#ifdef CONFIG_GFP_PROFILE
        btif_gfp_register_processer();
#endif

	if (btif_start(bt_manager_service_callback, bt_manager_config_bt_class(), bt_manager_config_get_device_id()) < 0) {
		SYS_LOG_ERR("btsrv start error\n");
		ret = -EACCES;
		goto bt_start_err;
	}

#ifdef CONFIG_TWS
    SYS_LOG_INF("bt_manager_tws_init:\n");
	bt_manager_tws_init();
#endif

#ifdef CONFIG_BT_HFP_HF
	bt_manager_hfp_init();
	bt_manager_hfp_sco_init();
#endif

#ifdef CONFIG_BT_HFP_AG
	bt_manager_hfp_ag_init();
#endif

#ifdef CONFIG_BT_PTS_TEST
	//btif_bt_set_pts_config(true);
#endif

	SYS_LOG_INF("success\n");

#ifdef GFP_AUTO_TEST
    thread_timer_init(&gfp_auto_test_timer, gfp_auto_test_handler,NULL);
    thread_timer_start(&gfp_auto_test_timer, 1000, 1000);
#endif

	return 0;
bt_start_err:
	return ret;
}

void bt_manager_deinit(void)
{
	int time_out = 0;
    struct bt_manager_context_t*  bt_manager = bt_manager_get_context();
    struct bt_set_user_visual user_visual;

	if (bt_manager->cur_bqb_mode == 1 ){
		return;
	}

	SYS_LOG_INF("start\n");

	os_delayed_work_cancel(&bt_manager->tts_delay_work);
	bt_manager->bt_ready = 0;
    bt_manager->pair_status = BT_PAIR_STATUS_NODISC_NOCON;
    btif_br_update_pair_status(bt_manager->pair_status);
	user_visual.enable = 0;
	btif_br_set_user_visual(&user_visual);

#ifdef CONFIG_BT_LETWS
	bt_manager_letws_deinit();
#endif

#ifdef GFP_AUTO_TEST
    thread_timer_stop(&gfp_auto_test_timer);
#endif
    os_delayed_work_cancel(&bt_manager->auto_reconnect_work);
	os_delayed_work_cancel(&bt_manager->auto_reconnect_second_dev_work);
	btif_br_auto_reconnect_stop(BTSRV_STOP_AUTO_RECONNECT_ALL);
	if (bt_manager_audio_is_lea_open()) {
		bt_manager_audio_add_all_dev_to_disconnect_info();
	}
	btif_br_set_autoconn_info_need_update(0);
	bt_manager_audio_set_disconnect_info_need_update(0);
	btif_br_disconnect_device(BTSRV_DISCONNECT_ALL_MODE);
	bt_manager_lea_policy_event_notify(LEA_POLICY_EVENT_BT_OFF, NULL, 0);

	while (btif_br_get_connected_device_num() && time_out++ < 500) {
		os_sleep(10);
	}

#ifdef CONFIG_BT_SPP
	bt_manager_spp_profile_stop();
#endif

	bt_manager_a2dp_profile_stop();
	bt_manager_avrcp_profile_stop();
#ifdef CONFIG_BT_HFP_HF
	bt_manager_hfp_profile_stop();
	bt_manager_hfp_sco_stop();
#endif

#ifdef CONFIG_BT_HFP_AG
	bt_manager_hfp_ag_profile_stop();
#endif

#ifdef CONFIG_BT_HID
	bt_manager_hid_profile_stop();
#endif

#ifdef CONFIG_TWS
	bt_manager_tws_deinit();
#endif
	bt_manager_save_dev_volume();

	btif_disable_service();

#ifdef CONFIG_BT_PTS_TEST
	bt_manager_pts_test_deinit();
#endif /*CONFIG_BT_PTS_TEST*/

#if 0
	/**
	 *  TODO: must clean btdrv /bt stack and bt service when bt manager deinit
	 *  enable this after all is work well.
	 */
	btif_stop();
#endif

	btif_br_set_autoconn_info_need_update(1);
	bt_manager_audio_set_disconnect_info_need_update(1);
	SYS_LOG_INF("done\n");
}

bool bt_manager_is_ready(void)
{
    struct bt_manager_context_t*  bt_manager = bt_manager_get_context();

	return (bt_manager->bt_ready == 1);
}

/* Return bt clock in ms */
uint32_t bt_manager_tws_get_bt_clock(void)
{
    return btif_tws_get_bt_clock();
}

int bt_manager_tws_set_irq_time(uint32_t bt_clock_ms)
{
    return btif_tws_set_irq_time(bt_clock_ms);
}

void bt_manager_wait_tws_ready_send_sync_msg(uint32_t wait_timeout)
{
	int ret;
	uint32_t start_time, stop_time;

	start_time = os_uptime_get_32();
	ret = btif_tws_wait_ready_send_sync_msg(wait_timeout);
	stop_time = os_uptime_get_32();

	if (ret || ((stop_time - start_time) > 10)) {
		SYS_LOG_WRN("Tws wait sync send thread %p(%d) ret %d time %d", os_current_get(),
					os_thread_priority_get(os_current_get()), ret, (stop_time - start_time));
	}
}

/* single_poweroff: 0: tws sync poweroff, 1: single device poweroff */
int bt_manager_proc_poweroff(uint8_t single_poweroff)
{
#ifdef CONFIG_BT_CONTROLER_BQB_SYS
	if (true == bt_manager_get_bqb_mode())
		return -EINVAL;
#endif

	struct bt_manager_context_t *bt_manager = bt_manager_get_context();

	os_mutex_lock(&bt_manager->poweroff_mutex, OS_FOREVER);

	if (bt_manager->poweroff_state != POFF_STATE_NONE) {
		SYS_LOG_INF("Poff in process");
		bt_manager->local_later_poweroff = 1;
		os_mutex_unlock(&bt_manager->poweroff_mutex);
		return 0;
	}

	SYS_LOG_INF("Start poff single %d", single_poweroff);

	if (bt_manager->cur_bqb_mode){
		bt_manager->poweroff_state = POFF_STATE_FINISH;
	}else{
		bt_manager->poweroff_state = POFF_STATE_START;
	}
	bt_manager->local_req_poweroff = 1;
	bt_manager->remote_req_poweroff = 0;
	bt_manager->single_poweroff = single_poweroff ? 1 : 0;
	bt_manager->local_later_poweroff = 0;

	if (bt_manager->local_req_poweroff ||
		(bt_manager->remote_req_poweroff && (bt_manager->single_poweroff == 0)))
	{
		SYS_LOG_INF("leaudio stop");
//		bt_manager_audio_unblocked_stop(BT_TYPE_LE);
	}

	os_delayed_work_submit(&bt_manager->poweroff_proc_work, 0);

	os_mutex_unlock(&bt_manager->poweroff_mutex);
	return 0;
}


int bt_manager_is_poweroffing(void)
{
	struct bt_manager_context_t *bt_manager = bt_manager_get_context();

	return (bt_manager->poweroff_state != POFF_STATE_NONE)?1:0;
}


/* timeout disconnected
 */
bool bt_manager_is_timeout_disconnected(uint32_t reason)
{
    if (reason == 0x08 ||  // HCI_STATUS_CONNECTION_TIMEOUT
        reason == 0x04 ||  // HCI_STATUS_PAGE_TIMEOUT
        reason == 0x22 ||  // HCI_STATUS_LMP_OR_LL_RESPONSE_TIMEOUT
        reason == 0x10)    // HCI_STATUS_CONNECTION_ACCEPT_TIMEOUT_EXCEEDED
    {
        return true;
    }

    return false;
}

/* resources disconnected
 */
bool bt_manager_is_resources_disconnected(uint32_t reason)
{
    if (reason == 0x14)  // BT_HCI_ERR_REMOTE_LOW_RESOURCES
    {
        return true;
    }
    return false;
}

int bt_manager_get_wake_lock(void)
{
#ifdef CONFIG_BT_BLE
	if (btif_base_get_wake_lock() /*|| bt_manager_get_ble_wake_lock()*/)
#else
	if (btif_base_get_wake_lock())
#endif
	{
		return 1;
	} else {
		return 0;
	}
}

int bt_manager_get_link_idle(void)
{
	if (btif_base_get_link_idle()){
		return 1;
	} else {
		return 0;
	}
}



int bt_manager_bt_set_apll_temp_comp(uint8_t enable)
{
	if (!bt_manager_is_ready()) {
		return -EIO;
	}

	return btif_bt_set_apll_temp_comp(enable);
}

int bt_manager_bt_do_apll_temp_comp(void)
{
	if (!bt_manager_is_ready()) {
		return -EIO;
	}

	return btif_bt_do_apll_temp_comp();
}

int bt_manager_bt_read_rssi(uint16_t handle)
{
#ifdef CONFIG_BT_CONTROLER_BQB_SYS
	if (true == bt_manager_get_bqb_mode())
		return -EINVAL;
#endif

    bt_mgr_dev_info_t* a2dp_dev;
    uint16_t hdl = handle;

    if(!handle){
        a2dp_dev = bt_mgr_get_a2dp_active_dev();
        if(a2dp_dev){
            hdl = a2dp_dev->hdl;
        }
    }

    if(hdl){
        return btif_br_get_rssi_by_handle(hdl);
    }
    else{
        return -EINVAL;
    }
}

int bt_manager_link_info_get(uint16_t handle, complete_event_cb cb)
{
    bt_mgr_dev_info_t* a2dp_dev;
    uint16_t hdl = handle;

    if(!handle){
        a2dp_dev = bt_mgr_get_a2dp_active_dev();
        if(a2dp_dev){
            hdl = a2dp_dev->hdl;
        }
    }

    if(hdl){
        return btif_br_get_linkinfo_by_handle(hdl, cb);
    }
    else{
        return -EINVAL;
    }
}

int bt_manager_link_info_set(uint8_t *map)
{
	return btif_linkinfo_set(map);
}

int bt_manager_bt_read_link_quality(uint16_t handle)
{
#ifdef CONFIG_BT_CONTROLER_BQB_SYS
	if (true == bt_manager_get_bqb_mode())
		return -EINVAL;
#endif

    bt_mgr_dev_info_t* a2dp_dev;
    uint16_t hdl = handle;

    if(!handle){
        a2dp_dev = bt_mgr_get_a2dp_active_dev();
        if(a2dp_dev){
            hdl = a2dp_dev->hdl;
        }
    }

    if(hdl){
        return btif_br_get_link_quality_by_handle(hdl);
    }
    else{
        return -EINVAL;
    }
}

void bt_manager_dump_info(void)
{
	int i;
    struct bt_manager_context_t *bt_manager = bt_manager_get_context();

	printk("Bt manager info\n");
	printk("num %d, tws_mode %d, bt_state 0x%x, playing %d\n", bt_manager->connected_phone_num,
		bt_manager->tws_mode, bt_manager->bt_state, (bt_manager_a2dp_get_status() == BT_STATUS_PLAYING));
	for (i = 0; i < MAX_MGR_DEV; i++) {
		if (bt_manager->dev[i].used) {
			printk("Dev hdl 0x%x name %s, tws %d snoop role %d notify_connected %d\n",
				bt_manager->dev[i].hdl, bt_manager->dev[i].name, bt_manager->dev[i].is_tws,
				bt_manager->dev[i].snoop_role, bt_manager->dev[i].notify_connected);
			printk("\t a2dp %d avrcp %d hf %d spp %d hid %d\n", bt_manager->dev[i].a2dp_connected,
				bt_manager->dev[i].avrcp_connected, bt_manager->dev[i].hf_connected,
				bt_manager->dev[i].spp_connected, bt_manager->dev[i].hid_connected);
		}
	}

	printk("\n");
	btif_dump_brsrv_info();
	bt_manager_audio_dump_info();
}

void bt_manager_set_aesccm_mode(uint8_t mode)
{
	ctrl_set_br_aesccm_mode(mode);
}
