/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt manager tws snoop.
 */
#define SYS_LOG_DOMAIN "bt manager"

#include <os_common_api.h>

#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stream.h>
#include <acts_ringbuf.h>
#include <bt_manager.h>
#include <sys_event.h>
#include <app_manager.h>
#include <power_manager.h>
#include "bt_manager_inner.h"
#include "audio_policy.h"
#include "btservice_api.h"
#include "media_player.h"
#include <volume_manager.h>
#include <audio_system.h>
#include <input_manager.h>
#ifdef CONFIG_PROPERTY
#include "property_manager.h"
#endif
#include "btmgr_tws_snoop_inner.h"
//#include "bt_porting_inner.h"

#define TWS_PAIR_TRY_TIMES 5
#define TWS_MAX_SS_CB_FUNC	5

static tws_runtime_observer_t *tws_observer;
static struct btmgr_tws_context_t tws_context;
static OS_MUTEX_DEFINE(tws_ss_func_mutex);
static OS_MUTEX_DEFINE(bt_manager_tws_mutex);

static struct tws_snoop_switch_cb_func *tws_ss_cb_func[TWS_MAX_SS_CB_FUNC];		/* ss: snoop switch */
static struct list_head  tws_timer_list = LIST_HEAD_INIT(tws_timer_list);

static void tws_timer_work_proc(os_work *work);

extern int bt_manager_tws_sync_app_status_info(void* param);

static const bt_mgr_event_strmap_t bt_manager_tws_event_map[] =
{
    {BTSRV_TWS_INIT_COMPLETE,               STRINGIFY(BTSRV_TWS_INIT_COMPLETE)},
    {BTSRV_TWS_DISCOVER_CHECK_DEVICE,       STRINGIFY(BTSRV_TWS_DISCOVER_CHECK_DEVICE)},
    {BTSRV_TWS_CONNECTED,                   STRINGIFY(BTSRV_TWS_CONNECTED)},
    {BTSRV_TWS_ROLE_CHANGE,                 STRINGIFY(BTSRV_TWS_ROLE_CHANGE)},
    {BTSRV_TWS_DISCONNECTED,                STRINGIFY(BTSRV_TWS_DISCONNECTED)},
    {BTSRV_TWS_START_PLAY,                  STRINGIFY(BTSRV_TWS_START_PLAY)},
	{BTSRV_TWS_PLAY_SUSPEND,                STRINGIFY(BTSRV_TWS_PLAY_SUSPEND)},
    {BTSRV_TWS_READY_PLAY,                  STRINGIFY(BTSRV_TWS_READY_PLAY)},
    {BTSRV_TWS_RESTART_PLAY,                STRINGIFY(BTSRV_TWS_RESTART_PLAY)},
    {BTSRV_TWS_EVENT_SYNC,                  STRINGIFY(BTSRV_TWS_EVENT_SYNC)},
    {BTSRV_TWS_A2DP_CONNECTED,              STRINGIFY(BTSRV_TWS_A2DP_CONNECTED)},
    {BTSRV_TWS_HFP_CONNECTED,               STRINGIFY(BTSRV_TWS_HFP_CONNECTED)},
    {BTSRV_TWS_IRQ_CB,                      STRINGIFY(BTSRV_TWS_IRQ_CB)},
    {BTSRV_TWS_UNPROC_PENDING_START_CB,     STRINGIFY(BTSRV_TWS_UNPROC_PENDING_START_CB)},
	{BTSRV_TWS_UPDATE_BTPLAY_MODE,          STRINGIFY(BTSRV_TWS_UPDATE_BTPLAY_MODE)},
    {BTSRV_TWS_UPDATE_PEER_VERSION,         STRINGIFY(BTSRV_TWS_UPDATE_PEER_VERSION)},
	{BTSRV_TWS_SINK_START_PLAY,              STRINGIFY(BTSRV_TWS_SINK_START_PLAY)},
    {BTSRV_TWS_PAIR_FAILED,                  STRINGIFY(BTSRV_TWS_PAIR_FAILED)},
    {BTSRV_TWS_EXPECT_TWS_ROLE,              STRINGIFY(BTSRV_TWS_EXPECT_TWS_ROLE)},
    {BTSRV_TWS_SCO_DATA,                     STRINGIFY(BTSRV_TWS_SCO_DATA)},
    {BTSRV_TWS_HFP_CALL_STATE,               STRINGIFY(BTSRV_TWS_HFP_CALL_STATE)},
    {BTSRV_TWS_PRE_SEL_CHANNEL,              STRINGIFY(BTSRV_TWS_PRE_SEL_CHANNEL)},
    {BTSRV_TWS_CONFIRM_SEL_CHANNEL,          STRINGIFY(BTSRV_TWS_CONFIRM_SEL_CHANNEL)},
    {BTSRV_TWS_EVENT_THROUGH_UPPER, 		 STRINGIFY(BTSRV_TWS_EVENT_THROUGH_UPPER)},
    {BTSRV_TWS_REQUEST_SNOOP_SWITCH,         STRINGIFY(BTSRV_TWS_REQUEST_SNOOP_SWITCH)},
    {BTSRV_TWS_SNOOP_SWITCH_START_NOTIFY,    STRINGIFY(BTSRV_TWS_SNOOP_SWITCH_START_NOTIFY)},
    {BTSRV_TWS_SNOOP_CHECK_READY_TO_SWITCH,  STRINGIFY(BTSRV_TWS_SNOOP_CHECK_READY_TO_SWITCH)},
    {BTSRV_TWS_SNOOP_CHECK_BLE_SWITCH,       STRINGIFY(BTSRV_TWS_SNOOP_CHECK_BLE_SWITCH)},
    {BTSRV_TWS_SNOOP_SWITCH_FINISH,          STRINGIFY(BTSRV_TWS_SNOOP_SWITCH_FINISH)},
	{BTSRV_TWS_CLEAR_LIST_COMPLETE,          STRINGIFY(BTSRV_TWS_CLEAR_LIST_COMPLETE)},
};

#define BT_MANAGER_TWS_EVENTNUM_STRS (sizeof(bt_manager_tws_event_map) / sizeof(bt_mgr_event_strmap_t))

const char *bt_manager_tws_evt2str(int num)
{
	return bt_manager_evt2str(num, BT_MANAGER_TWS_EVENTNUM_STRS, bt_manager_tws_event_map);
}

struct btmgr_tws_context_t *btmgr_get_tws_context(void)
{
	return &tws_context;
}

int bt_manager_tws_send_command(uint8_t *command, int command_len)
{
	int ret;

	print_buffer_lazy("tws send", command, command_len);
	ret = btif_tws_send_command(command, command_len);
	if (ret != command_len) {
		SYS_LOG_WRN("Failed send command!");
	}

	return ret;
}

int bt_manager_tws_send_sync_command(uint8_t *command, int command_len)
{
	int ret;

	print_buffer_lazy("tws sync", command, command_len);
	ret = btif_tws_send_command_sync(command, command_len);
	if (ret != command_len) {
		SYS_LOG_WRN("Failed send command!");
	}

	return ret;
}

void bt_manager_tws_cancel_wait_pair(void)
{
	btif_tws_end_pair();
}

void bt_manager_tws_pair_search(void)
{
	btmgr_feature_cfg_t *cfg_feature = bt_manager_get_feature_config();

	if (0 == tws_context.init) {
		SYS_LOG_ERR("Tws not init.");
		return;
	}

	SYS_LOG_INF("connected:%d tws pair:%d role %d support %d",
	    bt_manager_get_connected_dev_num(),
	    bt_manager_is_tws_pair_search(),
	    bt_manager_tws_get_dev_role(),
	    cfg_feature->sp_device_num);

    if(!bt_manager_is_tws_pair_search()
		&& (bt_manager_get_connected_dev_num() < CFG_BT_MANAGER_MAX_BR_NUM)
		&& (bt_manager_tws_get_dev_role() == BTSRV_TWS_NONE)
		&& (cfg_feature->sp_device_num > 1)
		&& (cfg_feature->sp_tws)){
	    bt_manager_tws_start_pair_search(TWS_PAIR_SEARCH_BY_KEY);
	}
}

tws_runtime_observer_t *bt_manager_tws_get_runtime_observer(void)
{
	/* Media request always return valid tws_runtime_observer_t,
	 * return NULL when not support TWS.
	 */
	return btif_tws_get_tws_observer();
}

int bt_manager_tws_get_dev_role(void)
{
	return tws_context.tws_role;
}

uint8_t bt_manager_tws_get_peer_version(void)
{
	return tws_context.peer_version;
}

bool bt_manager_tws_check_is_woodpecker(void)
{
	return false;
}

bool bt_manager_tws_check_support_feature(uint32_t feature)
{
	if (btif_tws_get_dev_role() != BTSRV_TWS_NONE) {
		if ((get_tws_current_feature() & feature) && (tws_context.peer_feature & feature)) {
			return true;
		} else {
			return false;
		}
	} else {
		return true;
	}
}

void bt_manager_tws_set_codec(uint8_t codec)
{
	btif_tws_set_codec(codec);
}

void bt_manager_tws_set_state(uint8_t state)
{
	btif_tws_set_state(state);
}

uint8_t bt_manager_tws_get_state(void)
{
	if (0 == tws_context.init) {
		SYS_LOG_ERR("Tws not init.");
		return 0;
	}

	return btif_tws_get_state();
}

#ifdef CONFIG_SUPPORT_TWS_ROLE_CHANGE
void btmgr_tws_switch_snoop_link(void)
{
	if (tws_context.tws_role == BTSRV_TWS_SLAVE) {
		/* Nothing todo, slave should not do snoop link switch */
	} else if (tws_context.tws_role == BTSRV_TWS_MASTER) {
		btif_tws_switch_snoop_link();
	}
}
#endif

static int bt_manager_tws_discover_check_device(void *param, int param_size)
{
	struct btsrv_discover_result *result = param;
	uint8_t pre_mac[3];
	uint8_t name[31];
	btmgr_base_config_t * cfg_base =  bt_manager_get_base_config();
	btmgr_tws_pair_cfg_t * cfg_tws =  bt_manager_get_tws_pair_config();
    uint8_t match_len = cfg_tws->match_name_length;
	uint8_t match_mode = cfg_tws->match_mode;
	bd_address_t bd_addr;

	if (result->name == NULL) {
		return 0;
	}

	if (bt_manager_config_get_tws_compare_high_mac()) {
		btif_br_get_local_mac(&bd_addr);
		memcpy(pre_mac,&bd_addr.val[3] ,3);
		if (result->addr.val[5] != pre_mac[2] ||
			result->addr.val[4] != pre_mac[1] ||
			result->addr.val[3] != pre_mac[0]){
			return 0;
		}
	}

    if(match_mode == TWS_MATCH_MODE_NAME){
        memset(name, 0, sizeof(name));
        memcpy(name,cfg_base->bt_device_name,30);

        if(match_len > result->len){
            match_len = result->len;
        }
        if (memcmp(result->name, name, match_len)) {
            return 0;
        }
        else{
            return 1;
        }
    }

	if (match_mode == TWS_MATCH_MODE_ID){
		if(memcmp(result->device_id, bt_manager_config_get_device_id(), sizeof(result->device_id))){
		    return 0;
		}
        else{
            return 1;
        }
	}
	return 0;
}

static void btmgr_tws_connected_cb(void *param, int param_size)
{
	uint8_t *tws_role = param;
    struct bt_manager_context_t*  bt_manager = bt_manager_get_context();
    btmgr_feature_cfg_t *cfg_feature = bt_manager_get_feature_config();

    bt_manager_tws_end_pair_search();

	tws_observer = btif_tws_get_tws_observer();
	tws_context.tws_role = *tws_role;
	bt_manager_set_status(BT_STATUS_TWS_PAIRED,1);

	if (*tws_role == BTSRV_TWS_SLAVE) {
#ifdef CONFIG_POWER
		power_manager_sync_slave_battery_state();
#endif
	}
    else{
        if(bt_manager->connected_phone_num >= (cfg_feature->sp_device_num -1)){
            bt_manager_end_pair_mode();
        }
    }
}

static void btmgr_tws_update_ble_state(uint8_t tws_role)
{
#ifdef CONFIG_BT_BLE
	if (tws_role == BTSRV_TWS_SLAVE) {
		bt_manager_halt_ble();
	} else {
		/* Master or not tws device */
		bt_manager_resume_ble();
	}
#endif
}

extern int bt_le_audio_relay_init(void);
extern int bt_le_audio_relay_exit(void);
extern int bt_le_audio_slave_init(void);
extern int bt_le_audio_slave_exit(void);
__weak int bt_le_audio_relay_init(void)
{
	return 0;
}

__weak int bt_le_audio_relay_exit(void)
{
	return 0;
}

__weak int bt_le_audio_slave_init(void)
{
	return 0;
}

__weak int bt_le_audio_slave_exit(void)
{
	return 0;
}

static uint8_t tws_le_audio_role;

static void btmgr_tws_le_audio_init(void)
{
	uint8 unused;
	int role;

	role = bt_manager_tws_get_dev_role_ext(&unused);

	SYS_LOG_INF("role: %d\n", role);
	tws_le_audio_role = role;

	switch (role) {
	case BTSRV_TWS_NONE:
	case BTSRV_TWS_MASTER:
	case BTSRV_TWS_TEMP_MASTER:
		//bt_le_audio_relay_init();
		break;
	case BTSRV_TWS_SLAVE:
	case BTSRV_TWS_TEMP_SLAVE:
		//bt_le_audio_slave_init();
		break;
	default:
		break;
	}
}

/* update LE Audio role too if tws role changes! */
static void btmgr_tws_le_audio_role_change(uint8_t new_role)
{
	uint8_t old_role = tws_le_audio_role;

	SYS_LOG_INF("role: %d -> %d", old_role, new_role);

	if (old_role == new_role) {
		return;
	}

	tws_le_audio_role = new_role;

	switch (old_role) {
	case BTSRV_TWS_NONE:
		switch (new_role) {
		/* none -> slave: exit master, init slave */
		case BTSRV_TWS_SLAVE:
		case BTSRV_TWS_TEMP_SLAVE:
			//bt_le_audio_relay_exit();
			//bt_le_audio_slave_init();
			break;
		/* do nothing */
		default:
			break;
		}
		break;

	case BTSRV_TWS_MASTER:
	case BTSRV_TWS_TEMP_MASTER:
		switch (new_role) {
		/* master -> slave: exit master, init slave */
		case BTSRV_TWS_SLAVE:
		case BTSRV_TWS_TEMP_SLAVE:
			//bt_le_audio_relay_exit();
			//bt_le_audio_slave_init();
			break;
		/* do nothing */
		default:
			break;
		}
		break;

	case BTSRV_TWS_SLAVE:
	case BTSRV_TWS_TEMP_SLAVE:
		switch (new_role) {
		/* slave -> master: exit slave, init master */
		case BTSRV_TWS_MASTER:
		case BTSRV_TWS_TEMP_MASTER:
			//bt_le_audio_slave_exit();
			//bt_le_audio_relay_init();
			break;
		/* do nothing */
		default:
			break;
		}
		break;

	default:
		break;
	}
}

static void btmgr_tws_role_change_cb(void *param, int param_size)
{
	uint8_t *tws_role = param;

	tws_context.tws_role = *tws_role;
	bt_manager_event_notify(BT_TWS_ROLE_CHANGE, NULL, 0);

	char * role_string = "null";
	if (tws_context.tws_role == BTSRV_TWS_SLAVE)
		role_string = "Slave";
	else if (tws_context.tws_role == BTSRV_TWS_MASTER)
		role_string = "Master";

	SYS_LOG_INF("### TWS Role:%s ###", role_string);
}

static void btmgr_tws_disconnected_cb(uint8_t *reason)
{
    struct bt_manager_context_t*  bt_manager = bt_manager_get_context();

	tws_observer = NULL;
	tws_context.peer_version = 0x0;
	tws_context.tws_role = 0;
	tws_context.le_remote_addr_valid = 0;

	if(tws_context.halt_aac){
		tws_context.halt_aac = 0;
		bt_manager_a2dp_halt_aac(false);
	}
	if(tws_context.halt_ldac){
		tws_context.halt_ldac = 0;
		bt_manager_a2dp_halt_ldac(false);
	}
	SYS_LOG_INF("Tws disconnect reason 0x%x", *reason);

	bt_manager->tws_dis_reason = *reason;
	bt_manager_set_status(BT_STATUS_TWS_UNPAIRED,1);
}

void bt_manager_tws_set_pair_keyword(u8_t keyword)
{
	if (0 == tws_context.init) {
		SYS_LOG_ERR("Tws not init.");
		return;
	}

	btif_tws_set_pair_keyword(keyword);
}

uint8_t bt_manager_tws_get_pair_keyword(void)
{
	if (0 == tws_context.init) {
		SYS_LOG_ERR("Tws not init.");
		return 0;
	}

	uint8_t keyword = btif_tws_get_pair_keyword();
	return keyword ;
}

void bt_manager_tws_set_rssi_threshold(char threshold)
{
	if (0 == tws_context.init) {
		SYS_LOG_ERR("Tws not init.");
		return;
	}

	btif_tws_set_rssi_threshold(threshold);
}

void bt_manager_tws_set_pair_mode_ex(bool enable)
{
	btmgr_tws_pair_cfg_t *cfg_tws_pair = bt_manager_get_tws_pair_config();

    SYS_LOG_INF("enable:%d", enable);
	if (0 == tws_context.init) {
		SYS_LOG_ERR("Tws not init.");
		return;
	}

    if (enable){
        if (cfg_tws_pair->check_rssi_when_tws_pair_search
            && (btif_tws_get_pair_addr_match() == 0)){
            btif_tws_set_rssi_threshold(cfg_tws_pair->rssi_threshold);
        }
        btif_tws_start_pair(TWS_PAIR_TRY_TIMES);
    }
    else{
        if (cfg_tws_pair->check_rssi_when_tws_pair_search
            && (btif_tws_get_pair_addr_match() == 0)){
            btif_tws_set_rssi_threshold(-120);
        }
        btif_tws_end_pair();
    }
}

void bt_manager_tws_set_inquiry_ex(bool enable)
{
	btmgr_tws_pair_cfg_t *cfg_tws_pair = bt_manager_get_tws_pair_config();
    uint8_t search_times = cfg_tws_pair->wait_pair_search_time_sec >> 3;
    SYS_LOG_INF("enable:%d times:%d", enable, search_times);

    if (enable){
        btif_tws_start_pair(search_times);
    }
    else{
        btif_tws_end_pair();
    }
}

void bt_manager_tws_set_pair_addr_match(bool enable,bd_address_t *addr)
{
	if (0 == tws_context.init) {
		SYS_LOG_ERR("Tws not init.");
		return;
	}

	btif_tws_set_pair_addr_match(enable,addr);
}

void bt_manager_tws_end_pair_search(void)
{
    struct bt_manager_context_t*  bt_manager = bt_manager_get_context();

	btmgr_tws_pair_cfg_t *cfg_tws_pair = bt_manager_get_tws_pair_config();

	btif_br_get_pair_status(&bt_manager->pair_status);

    SYS_LOG_INF("search:%d %d", bt_manager_is_tws_pair_search(),bt_manager->pair_status);

	if (0 == tws_context.init) {
		SYS_LOG_ERR("Tws not init.");
		return;
	}

    if (!bt_manager_is_tws_pair_search() &&
		((bt_manager->pair_status & BT_PAIR_STATUS_TWS_SEARCH) == 0)){
        return;
    }

	os_delayed_work_cancel(&bt_manager->tws_pair_search_work);

    bt_manager->pair_status &= (~BT_PAIR_STATUS_TWS_SEARCH);

    if ((cfg_tws_pair->enable_advanced_pair_mode) || (bt_manager_tws_get_pair_keyword() != 0)){
        bt_manager_tws_set_pair_keyword(0);
        bt_manager_tws_set_pair_mode_ex(false);
        bt_manager_tws_set_pair_addr_match(false,NULL);
    }
    else{
        bt_manager_tws_set_inquiry_ex(0);
    }

    btif_br_set_tws_search_status(0);

    bt_manager_check_visual_mode();

    bt_manager_check_link_status();
}


void bt_manager_tws_start_pair_search_ex(u8_t tws_pair_keyword)
{
    uint32_t timeout;

    struct bt_manager_context_t*  bt_manager = bt_manager_get_context();
	btmgr_tws_pair_cfg_t *cfg_tws_pair = bt_manager_get_tws_pair_config();
    btmgr_feature_cfg_t *cfg_feature = bt_manager_get_feature_config();

    if (!cfg_feature->sp_tws) {
        return;
    }

	if (0 == tws_context.init) {
		SYS_LOG_ERR("Tws not init.");
		return;
	}

    bt_manager_tws_end_pair_search();

	btif_br_get_pair_status(&bt_manager->pair_status);

    bt_manager->pair_status |= BT_PAIR_STATUS_TWS_SEARCH;

    btif_br_auto_reconnect_stop(BTSRV_STOP_AUTO_RECONNECT_ALL);
	os_delayed_work_cancel(&bt_manager->auto_reconnect_work);
	os_delayed_work_cancel(&bt_manager->auto_reconnect_second_dev_work);

    bt_manager_tws_set_pair_keyword(tws_pair_keyword);

	btif_br_set_tws_search_status(1);

    bt_manager_check_visual_mode();

    do{
        if((cfg_tws_pair->pair_key_mode == BTSRV_PAIR_KEY_MODE_TWO) ||
                cfg_tws_pair->power_on_auto_pair_search){
            if(cfg_tws_pair->tws_device_channel_l_r == 2){
                break;
            }
        }
        if (cfg_tws_pair->enable_advanced_pair_mode || (tws_pair_keyword != 0)){
            bt_manager_tws_set_pair_addr_match(false,NULL);
            bt_manager_tws_set_pair_mode_ex(true);
        }
        else {
            bt_manager_tws_set_inquiry_ex(true);
        }
    }
    while(0);

    if(cfg_tws_pair->wait_pair_search_time_sec > 0){
		timeout = cfg_tws_pair->wait_pair_search_time_sec * 1000;
	    os_delayed_work_submit(&bt_manager->tws_pair_search_work,timeout);
    }

    bt_manager_set_status(BT_STATUS_TWS_PAIR_SEARCH,1);
}

void bt_manager_tws_set_visual_mode_ex(void)
{
    btif_tws_update_visual_pair();
}

void bt_manager_tws_set_visual_mode(void)
{
	btmgr_tws_pair_cfg_t *cfg_tws_pair = bt_manager_get_tws_pair_config();

    if ((cfg_tws_pair->enable_advanced_pair_mode == 0) && (bt_manager_tws_get_pair_keyword() == 0)){
        return;
    }

    bt_manager_tws_set_visual_mode_ex();
}

void bt_manager_tws_start_pair_search(uint8_t mode)
{
	btmgr_tws_pair_cfg_t *cfg_tws_pair = bt_manager_get_tws_pair_config();
	uint8_t tws_pair_keyword = 0;

	if (0 == tws_context.init) {
		SYS_LOG_ERR("Tws not init.");
		return;
	}

    SYS_LOG_INF("mode:%d advanced:%d key:%d",mode,
		cfg_tws_pair->enable_advanced_pair_mode,
		cfg_tws_pair->pair_key_mode);

    if(mode == TWS_PAIR_SEARCH_BY_KEY){
        if ((cfg_tws_pair->enable_advanced_pair_mode)
			&& (cfg_tws_pair->pair_key_mode == BTSRV_PAIR_KEY_MODE_TWO)){
            tws_pair_keyword = TWS_KEYWORD_PAIR_SEARCH_BY_KEY;
        }
    }
    else{
		tws_pair_keyword = TWS_KEYWORD_POWER_ON_AUTO_PAIR;
    }

    bt_manager_tws_start_pair_search_ex(tws_pair_keyword);
}

void bt_manager_tws_enter_pair_mode(void)
{
	btmgr_pair_cfg_t *cfg_pair = bt_manager_get_pair_config();
    uint8_t role = bt_manager_tws_get_dev_role();
    struct bt_manager_context_t*  bt_manager = bt_manager_get_context();

    SYS_LOG_INF("0x%x", role);

    if(role == BTSRV_TWS_SLAVE){
        if (cfg_pair->clear_paired_list_when_enter_pair_mode){
            btif_br_clear_list(BTSRV_DEVICE_PHONE);
        }
    }
    else if(role == BTSRV_TWS_MASTER){
        bt_manager->pair_mode_state = BT_PAIR_MODE_DISCONNECT_PHONE;
            if(bt_manager->pair_mode_work_valid == true){
            os_delayed_work_cancel(&bt_manager->pair_mode_work);
            bt_manager->pair_mode_work_valid = 0;
        }
        bt_manager_enter_pair_mode_ex();
    }
	else{
		;
	}
}

void bt_manager_tws_sync_pair_mode(uint8_t pair_mode)
{
    struct bt_manager_context_t*  bt_manager = bt_manager_get_context();
    uint8_t role = bt_manager_tws_get_dev_role();

    SYS_LOG_INF("role:%d mode%d", role,pair_mode);

    if(role == BTSRV_TWS_SLAVE){
        if((pair_mode & BT_PAIR_STATUS_PAIR_MODE) != 0){
            bt_manager->remote_in_pair_mode = true;
        }
		else{
			bt_manager->remote_in_pair_mode = false;
		}
    }
}

void bt_manager_tws_clear_paired_list(void)
{
    struct bt_manager_context_t*  bt_manager = bt_manager_get_context();

    uint8_t role = bt_manager_tws_get_dev_role();

    SYS_LOG_INF("role:%d", role);

    bt_manager->clear_paired_list_disc_tws = 0;

	if(role == BTSRV_TWS_SLAVE){
		bt_manager->clear_paired_list_state = BT_CLEAR_PAIRED_LIST_PHONE_CLEAR;
		bt_manager->clear_paired_list_timeout = 30;
	}
	else{
		bt_manager->clear_paired_list_state = BT_CLEAR_PAIRED_LIST_DISCONNECT_PHONE;
	}

    if(!bt_manager->clear_paired_list_work_valid){
		bt_manager->clear_paired_list_work_valid = true;
        os_delayed_work_submit(&bt_manager->clear_paired_list_work,0);
    }
}

static void bt_manager_tws_check_reconnect_phone(void)
{
	int cnt, i;
	struct autoconn_info *info;
	struct bt_set_autoconn reconnect_param;
	struct bt_mgr_dev_info * phone_dev;

	info = mem_malloc(sizeof(struct autoconn_info)*BT_MAX_AUTOCONN_DEV);
	if (info == NULL) {
		SYS_LOG_ERR("malloc failed");
		return;
	}

	cnt = btif_br_get_auto_reconnect_info(info, BT_MAX_AUTOCONN_DEV);

    if(cnt == 0){
		mem_free(info);
		return;
    }

	memset(&reconnect_param, 0, sizeof(struct bt_set_autoconn));

    for (i = 0; i < BT_MAX_AUTOCONN_DEV; i++){
        if((info[i].addr_valid) && (info[i].tws_role == BTSRV_TWS_NONE)){
            phone_dev = bt_mgr_find_dev_info(&info[i].addr);
			if(phone_dev == NULL){
				continue;
			}
			if(phone_dev->a2dp_connected
				&& phone_dev->hf_connected
				&& phone_dev->avrcp_connected){
				continue;
			}

            memcpy(reconnect_param.addr.val, info[i].addr.val, sizeof(bd_address_t));
            reconnect_param.strategy = BTSRV_AUTOCONN_ALTERNATE;
            reconnect_param.base_try = BT_BASE_STARTUP_RECONNECT_TRY;
            reconnect_param.profile_try = BT_PROFILE_RECONNECT_TRY;
            reconnect_param.base_interval = BT_BASE_RECONNECT_INTERVAL;
            reconnect_param.profile_interval = BT_PROFILE_RECONNECT_INTERVAL;
            reconnect_param.reconnect_mode = BTSRV_TWS_RECONNECT_NONE;
            reconnect_param.reconnect_tws_timeout = BT_RECONNECT_TWS_TIMEOUT;
            reconnect_param.reconnect_phone_timeout = BT_RECONNECT_PHONE_TIMEOUT;
            btif_br_auto_reconnect(&reconnect_param);

        }
    }

	mem_free(info);
}

static void bt_manager_tws_dev_role_changed(uint8_t mode)
{
    uint8_t role = bt_manager_tws_get_dev_role();
	if(role == BTSRV_TWS_SLAVE){
		if(btif_br_is_auto_reconnect_runing()){
            if(mode){
                btif_br_auto_reconnect_stop(BTSRV_STOP_AUTO_RECONNECT_ALL | BTSRV_SYNC_AUTO_RECONNECT_STATUS);
            }
            else{
                btif_br_auto_reconnect_stop(BTSRV_STOP_AUTO_RECONNECT_ALL);
            }
		}

        bt_manager_tws_end_pair_search();

        bt_manager_end_pair_mode();

        bt_manager_end_wait_connect();

        bt_manager_check_visual_mode();

        bt_manager_check_link_status();
	}
	else if(role == BTSRV_TWS_MASTER){

		bt_manager_tws_check_reconnect_phone();

        if(bt_manager_is_remote_in_pair_mode()){

            bt_manager_clear_remote_pair_mode();

            bt_manager_start_pair_mode();
        }
        else {
            bt_manager_check_visual_mode();

            bt_manager_check_link_status();
        }
	}
}

int __weak bt_manager_tws_prepare_sel_channel
(
    int tws_pair_search,
    int* local_ch, int* remote_ch)
{
    SYS_LOG_WRN("Weak function");
    return 0;
}

int __weak bt_manager_tws_confirm_sel_channel
(
    int local_acl_requested,
    int remote_sw_sel_dev_channel, int tws_sel_channel)
{
    SYS_LOG_WRN("Weak function");
    return 0;
}

static void tws_reconnect_work_proc(os_work *work)
{
	int num = bt_manager_get_connected_dev_num();
	if(!num){
		if(tws_context.halt_ldac)
			bt_manager_a2dp_halt_ldac(true);
		if(tws_context.halt_aac)
			bt_manager_a2dp_halt_aac(true);
		bt_manager_resume_phone();
	} else {
		os_delayed_work_submit(&tws_context.tws_reconnect_dev_work, 20);
	}
}

static void bt_manager_tws_check_a2dp_codecinfo()
{
	int need_reconnect = 0;

	if(bt_manager_tws_get_dev_role() != BTSRV_TWS_MASTER)
		return;

	int num = bt_manager_get_connected_dev_num();
	if(!bt_manager_tws_check_support_feature(TWS_FEATURE_A2DP_LDAC)
		&& (get_tws_current_feature() & TWS_FEATURE_A2DP_LDAC)
		&& !tws_context.halt_ldac){
		tws_context.halt_ldac = 1;
		if(num)
			need_reconnect = 1;
		else {
			bt_manager_a2dp_halt_ldac(true);
		}
	}
	if(!bt_manager_tws_check_support_feature(TWS_FEATURE_A2DP_LDAC)
		&& !bt_manager_tws_check_support_feature(TWS_FEATURE_A2DP_AAC)
		&& (get_tws_current_feature() & TWS_FEATURE_A2DP_AAC)
		&& !tws_context.halt_aac){
		tws_context.halt_aac = 1;
		if(num)
			need_reconnect = 1;
		else {
			bt_manager_a2dp_halt_aac(true);
		}
	}
	if(need_reconnect){
		bt_manager_halt_phone();
		os_delayed_work_submit(&tws_context.tws_reconnect_dev_work, 50);
	}

}

#ifdef CONFIG_SUPPORT_TWS_ROLE_CHANGE
static void btmgr_tws_event_snoop_switch_start(void)
{
	int i;
	struct tws_snoop_switch_cb_func *cb_func;

	/* Can't block in this callback !
	 * Notify every app snoop switch will start.
	 */
	os_mutex_lock(&tws_ss_func_mutex, OS_FOREVER);
	for (i = 0; i < TWS_MAX_SS_CB_FUNC; i++) {
		cb_func = tws_ss_cb_func[i];
		if (cb_func && cb_func->start_cb) {
			cb_func->start_cb(cb_func->id);
		}
	}
	os_mutex_unlock(&tws_ss_func_mutex);
}

static int btmgr_tws_event_snoop_switch_check_ready(void)
{
	int i, ret = 1;
	struct tws_snoop_switch_cb_func *cb_func;

	/* Can't block in this callback !
	 * check every app is ready for snoop switch,
	 * ret, 1: can do snoop switch, 0: can't do switch.
	 */
	os_mutex_lock(&tws_ss_func_mutex, OS_FOREVER);
	for (i = 0; i < TWS_MAX_SS_CB_FUNC; i++) {
		cb_func = tws_ss_cb_func[i];
		if (cb_func && cb_func->check_ready_cb) {
			if (cb_func->check_ready_cb(cb_func->id) == 0) {
				ret = 0;
				break;
			}
		}
	}
	os_mutex_unlock(&tws_ss_func_mutex);

	return ret;
}

static void btmgr_tws_event_snoop_switch_finish(uint8_t is_master)
{
	int i;
	struct tws_snoop_switch_cb_func *cb_func;

	/* Can't block in this callback !*/
	os_mutex_lock(&tws_ss_func_mutex, OS_FOREVER);
	for (i = 0; i < TWS_MAX_SS_CB_FUNC; i++) {
		cb_func = tws_ss_cb_func[i];
		if (cb_func && cb_func->finish_cb) {
			cb_func->finish_cb(cb_func->id, is_master);
		}
	}
	os_mutex_unlock(&tws_ss_func_mutex);
}
#endif

static int btmgr_tws_event_notify(int event_type, void *param, int param_size)
{
	int ret = 0;

    if((event_type != BTSRV_TWS_IRQ_CB) && (event_type != BTSRV_TWS_SCO_DATA)
		&&(event_type != BTSRV_TWS_EVENT_THROUGH_UPPER)
		&& (event_type != BTSRV_TWS_EVENT_SYNC)){
	    SYS_LOG_INF("event: %s",bt_manager_tws_evt2str(event_type));
    }

	switch (event_type) {
	case BTSRV_TWS_INIT_COMPLETE:
		btmgr_tws_le_audio_init();
	    break;
	case BTSRV_TWS_DISCOVER_CHECK_DEVICE:
		ret = bt_manager_tws_discover_check_device(param, param_size);
		break;
	case BTSRV_TWS_CONNECTED:
		btmgr_tws_connected_cb(param, param_size);
		bt_manager_tws_sync_app_status_info(NULL);
		bt_manager_tws_sync_lea_info(NULL);
		bt_manager_audio_tws_connected();
		bt_manager_tws_sync_active_dev(NULL);
		bt_manager_tws_dev_role_changed(0);
		btmgr_tws_role_change_cb(param, param_size);
		btmgr_tws_update_ble_state(tws_context.tws_role);
		btmgr_tws_le_audio_role_change(tws_context.tws_role);
		//bt_manager_tws_sync_ble_mac(NULL);
		bt_manager_tws_sync_enable_lea_dir_adv_inner(NULL);
		bt_manager_tws_check_a2dp_codecinfo();
		break;
	case BTSRV_TWS_ROLE_CHANGE:
		btmgr_tws_role_change_cb(param, param_size);
		bt_manager_tws_sync_app_status_info(NULL);
        bt_manager_tws_dev_role_changed(1);
		bt_manager_tws_sync_active_dev(NULL);
		btmgr_tws_poff_check_tws_role_change();
		btmgr_tws_update_ble_state(tws_context.tws_role);
		break;
	case BTSRV_TWS_DISCONNECTED:
		btmgr_tws_disconnected_cb(param);
		btmgr_tws_poff_check_tws_disconnected();
		btmgr_tws_update_ble_state(tws_context.tws_role);
		bt_manager_audio_sync_remote_active_app(0, NULL, NULL);
		break;
	case BTSRV_TWS_EVENT_SYNC:
		btmgr_tws_process_ui_event(param, param_size);
		break;
	case BTSRV_TWS_EVENT_THROUGH_UPPER:
		ret = btmgr_tws_event_through_upper(param, param_size);
		break;
	case BTSRV_TWS_UPDATE_PEER_VERSION:
	{
		struct update_version_param *in_param = param;
		tws_context.peer_version = in_param->versoin;
		tws_context.peer_feature = in_param->feature;
		SYS_LOG_INF("peer version 0x%x, feature 0x%x", tws_context.peer_version, tws_context.peer_feature);
		break;
	}
	case BTSRV_TWS_EXPECT_TWS_ROLE:
		ret = bt_manager_config_expect_tws_connect_role();
		break;
	case BTSRV_TWS_PRE_SEL_CHANNEL:
	{
        int tws_pair_search = bt_manager_is_tws_pair_search(); //todo
        int local_ch =0;
		int remote_ch =0;
		bt_manager_tws_prepare_sel_channel(tws_pair_search, &local_ch, &remote_ch);
		ret = local_ch | (remote_ch<<16);
		break;
	}
	case BTSRV_TWS_CONFIRM_SEL_CHANNEL:
	{
		struct update_channel_param *in_param = param;
		ret = bt_manager_tws_confirm_sel_channel(in_param->local_acl_requested,
				in_param->sw_sel_dev_channel, in_param->tws_dev_channel);
		break;
	}
	case BTSRV_TWS_IRQ_CB:
	{
		/* Call back in tws irq, can't do too mush  */
#if 0
		uint32_t *bt_clock_ms = param;
		if (!list_empty(&tws_timer_list))
		{
			printk("btx: %d\n", *bt_clock_ms);
		}
#endif
		os_work_submit(&tws_context.tws_irq_work);
		break;
	}
#ifdef CONFIG_SUPPORT_TWS_ROLE_CHANGE
	case BTSRV_TWS_REQUEST_SNOOP_SWITCH:
		/* Can send message to app, app decide switch or not */
		btmgr_tws_switch_snoop_link();
		break;
	case BTSRV_TWS_SNOOP_SWITCH_START_NOTIFY:
		btmgr_tws_event_snoop_switch_start();
		break;
	case BTSRV_TWS_SNOOP_CHECK_READY_TO_SWITCH:
		ret = btmgr_tws_event_snoop_switch_check_ready();
		break;
	case BTSRV_TWS_SNOOP_CHECK_BLE_SWITCH:
		/* return ret:  0: ble still in switch. other: ble switch finish. */
//#ifdef CONFIG_BT_BLE
//		if (bt_manager_ble_check_switch_finish()) {
//			ret = 1;
//		} else {
//			ret = 0;
//		}
//#else
		ret = 1;
//#endif
		break;
	case BTSRV_TWS_SNOOP_SWITCH_FINISH:
		btmgr_tws_event_snoop_switch_finish(((tws_context.tws_role == BTSRV_TWS_SLAVE) ? 0 : 1));
		break;
#endif
	case BTSRV_TWS_CLEAR_LIST_COMPLETE:
		break;
	default:
		break;
	}

	return ret;
}

int bt_manager_tws_reg_snoop_switch_cb_func(struct tws_snoop_switch_cb_func *cb, bool reg)
{
	int i, ret = 0;

	os_mutex_lock(&tws_ss_func_mutex, OS_FOREVER);
	if (reg) {
		for (i = 0; i < TWS_MAX_SS_CB_FUNC; i++) {
			if (tws_ss_cb_func[i] && tws_ss_cb_func[i]->id == cb->id) {
				SYS_LOG_ERR("ID already registed 0x%x!", cb->id);
				ret = -EEXIST;
				goto reg_exit;
			}
		}

		for (i = 0; i < TWS_MAX_SS_CB_FUNC; i++) {
			if (tws_ss_cb_func[i] == NULL) {
				tws_ss_cb_func[i] = cb;
				goto reg_exit;
			}
		}

		SYS_LOG_ERR("Need more tws_ss_cb_func!");
		ret = -ENOMEM;
	} else {
		for (i = 0; i < TWS_MAX_SS_CB_FUNC; i++) {
			if (tws_ss_cb_func[i] == cb) {
				tws_ss_cb_func[i] = NULL;
				goto reg_exit;
			}
		}
	}

reg_exit:
	os_mutex_unlock(&tws_ss_func_mutex);
	return ret;
}

int bt_manager_tws_send_snoop_switch_sync_info(uint8_t *data, uint16_t len)
{
	return bt_manager_tws_send_message(TWS_LONG_MSG_EVENT, TWS_EVENT_SYNC_SNOOP_SWITCH_INFO, data, len);
}

void bt_manager_tws_proc_snoop_switch_sync_info(uint8_t *data, uint16_t len)
{
	int i;
	uint8_t id = data[0];

	os_mutex_lock(&tws_ss_func_mutex, OS_FOREVER);
	for (i = 0; i < TWS_MAX_SS_CB_FUNC; i++) {
		if (tws_ss_cb_func[i] && tws_ss_cb_func[i]->id == id) {
			if (tws_ss_cb_func[i]->sync_info_cb) {
				tws_ss_cb_func[i]->sync_info_cb(id, data, len);
			}
			break;
		}
	}
	os_mutex_unlock(&tws_ss_func_mutex);
}

bool bt_manager_tws_powon_auto_pair(void)
{
    btmgr_feature_cfg_t *cfg_feature = bt_manager_get_feature_config();
    btmgr_tws_pair_cfg_t *tws_pair_config = bt_manager_get_tws_pair_config();

    bool result = false;
    struct autoconn_info *info;
    int cnt;

    if (!cfg_feature->sp_tws) {
        return false;
    }

    if (tws_pair_config->power_on_auto_pair_search){
        info = mem_malloc(sizeof(struct autoconn_info)* MAX_MGR_DEV);
        if (info == NULL) {
            SYS_LOG_ERR("malloc failed");
            return result;
        }
        cnt = btif_br_get_auto_reconnect_info(info, MAX_MGR_DEV);
        if (cnt == 0) {
            result = true;
        }
        mem_free(info);
    }
    if (result == true){
        bt_manager_start_pair_mode();

        bt_manager_tws_start_pair_search(TWS_POWER_ON_AUTO_PAIR);
    }

    return result;
}

void bt_manager_tws_init(void)
{
    tws_visual_parameter_t visual_param;
	tws_init_param init_param;
	btmgr_tws_pair_cfg_t *cfg_tws = bt_manager_get_tws_pair_config();
	btmgr_base_config_t *cfg_base = bt_manager_get_base_config();

	memset(&tws_context, 0, sizeof(struct btmgr_tws_context_t));
	tws_observer = NULL;

    btif_tws_context_init();

	memset(&visual_param, 0, sizeof(tws_visual_parameter_t));
    visual_param.interval = 20;
	visual_param.connected_phone = 0;
	visual_param.pair_keyword = 0;
	visual_param.device_channel = cfg_tws->tws_device_channel_l_r;
	visual_param.filtering_rules = cfg_tws->match_mode;
	visual_param.filtering_name_length = cfg_tws->match_name_length;
    btif_tws_set_visual_param(&visual_param);

    tws_pair_parameter_t pair_param;
	memset(&pair_param, 0, sizeof(tws_pair_parameter_t));

	pair_param.interval = 300;
	pair_param.window = 200;
	pair_param.rssi_thres = 0x88;
	pair_param.mask_addr = 0;
	pair_param.connected_phone = 0;
	pair_param.mask_phone = 1;
	if(cfg_tws->tws_device_channel_l_r == TWS_DEVICE_CHANNEL_L){
	    pair_param.channel = TWS_DEVICE_CHANNEL_R;
	}
	else if(cfg_tws->tws_device_channel_l_r == TWS_DEVICE_CHANNEL_R){
		pair_param.channel = TWS_DEVICE_CHANNEL_L;
	}
	else{
		pair_param.channel = TWS_DEVICE_CHANNEL_NONE;
    }
	pair_param.mask_channel = 1;
	pair_param.match_mode = cfg_tws->match_mode;
	pair_param.mask_mode = 1;
	pair_param.pair_keyword = 0;
	pair_param.mask_keyword = 1;
	pair_param.match_len = cfg_tws->match_name_length;
    btif_tws_set_pair_param(&pair_param);

	init_param.cb = btmgr_tws_event_notify;
	init_param.quality_monitor = cfg_base->link_quality.quality_monitor;
	init_param.quality_pre_val = cfg_base->link_quality.quality_pre_val;
	init_param.quality_diff = cfg_base->link_quality.quality_diff;
	init_param.quality_esco_diff = cfg_base->link_quality.quality_esco_diff;
	init_param.advanced_pair_mode = cfg_tws->enable_advanced_pair_mode;
	btif_tws_init(&init_param);

	tws_timer_init();

	os_delayed_work_init(&tws_context.tws_reconnect_dev_work , tws_reconnect_work_proc);
	tws_context.init = 1;
}

void bt_manager_tws_deinit(void)
{
	tws_context.init = 0;
	btif_tws_deinit();
	tws_timer_deinit();

	os_delayed_work_cancel(&tws_context.tws_reconnect_dev_work);
}

int tws_timer_init(void)
{
	os_work_init(&tws_context.tws_irq_work, tws_timer_work_proc);

	return 0;
}

int tws_timer_deinit(void)
{
	return 0;
}


int tws_timer_clear(void)
{
	tws_timer_info_t *timer_info;
	uint32_t  bt_clock = bt_manager_tws_get_bt_clock();

	os_mutex_lock(&bt_manager_tws_mutex, OS_FOREVER);
	while (!list_empty(&tws_timer_list))
	{
		timer_info = list_first_entry(&tws_timer_list, tws_timer_info_t, node);

		list_del_init(&timer_info->node);

		SYS_LOG_INF("%d,%d\n",timer_info->bt_clock, bt_clock);

		if (timer_info->timer_func){
			timer_info->timer_func(timer_info->timer_param, bt_clock);
		}
		mem_free(timer_info);
	}
	os_mutex_unlock(&bt_manager_tws_mutex);

	return 0;
}

int tws_timer_add(uint32_t bt_clock, tws_timer_cb_t timer_cb , void* timer_param)
{
	tws_timer_info_t* timer_info = (tws_timer_info_t*)mem_malloc(sizeof(tws_timer_info_t));
	uint32_t curr_bt_clock;

	if (!timer_info)
	{
		SYS_LOG_ERR("malloc fail");
		mem_free(timer_param);
		return -1;
	}

	memset(timer_info, 0, sizeof(tws_timer_info_t));

	timer_info->timer_func = timer_cb;
	timer_info->timer_param = timer_param;
	timer_info->bt_clock = bt_clock;

	curr_bt_clock = bt_manager_tws_get_bt_clock();
	SYS_LOG_INF("[%d, %d]", curr_bt_clock, bt_clock);
	os_mutex_lock(&bt_manager_tws_mutex, OS_FOREVER);
	if(list_empty(&tws_timer_list))
	{
		list_add_tail(&timer_info->node, &tws_timer_list);
		bt_manager_tws_set_irq_time(timer_info->bt_clock);
		goto End;
	}

	tws_timer_info_t* ttimeinfo;
	list_for_each_entry(ttimeinfo, &tws_timer_list, node)
	{
		if(timer_info->bt_clock <= ttimeinfo->bt_clock)
		{
			list_add(&timer_info->node, ttimeinfo->node.prev);
			goto End;
		}
	}

	list_add_tail(&timer_info->node, &tws_timer_list);

End:
	os_mutex_unlock(&bt_manager_tws_mutex);
	return 0;
}

#define SRV_ABS(x) ((x) > 0 ? (x) : (-(x)))

static void tws_timer_work_proc(os_work *work)
{
	os_mutex_lock(&bt_manager_tws_mutex, OS_FOREVER);
	if (list_empty(&tws_timer_list))
	{
		os_mutex_unlock(&bt_manager_tws_mutex);
		return;
	}

	struct list_head call_and_del = LIST_HEAD_INIT(call_and_del);
	tws_timer_info_t *timer_info, *ttimer_info;

	uint32_t bt_clock = bt_manager_tws_get_bt_clock();

	list_for_each_entry_safe(timer_info, ttimer_info, &tws_timer_list, node)
	{
		int32_t diff_time = timer_info->bt_clock - bt_clock;
		if((timer_info->bt_clock > bt_clock) && (SRV_ABS(diff_time) > 2))
		{
			SYS_LOG_INF("xxx %d,%d",timer_info->bt_clock, bt_clock);
			break;
		}
		list_del(&timer_info->node);
		list_add_tail(&timer_info->node, &call_and_del);
	}

	while (!list_empty(&call_and_del))
	{
		timer_info = list_first_entry(&call_and_del, tws_timer_info_t, node);

		list_del_init(&timer_info->node);

		SYS_LOG_DBG("%d,%d",timer_info->bt_clock, bt_clock);

		if (timer_info->timer_func){
			timer_info->timer_func(timer_info->timer_param, bt_clock);
		}
		mem_free(timer_info);
	}

	if(!list_empty(&tws_timer_list))
	{
		timer_info = list_first_entry
		(
			&tws_timer_list, tws_timer_info_t, node
		);

		SYS_LOG_INF("xxx new set: %d",timer_info->bt_clock);
		bt_manager_tws_set_irq_time(timer_info->bt_clock);
	}

	os_mutex_unlock(&bt_manager_tws_mutex);
}

uint32_t tws_timer_get_near_btclock(void)
{
	uint32_t bt_clk =  bt_manager_tws_get_bt_clock();

	if(bt_clk == UINT32_MAX){
		bt_clk  = 0;
		SYS_LOG_WRN("btclk invalid");
	}
	return bt_clk;
}

void bt_manager_tws_disconnect(void)
{
     btif_br_disconnect_device(BTSRV_DISCONNECT_TWS_MODE);
}

int bt_manager_tws_get_dev_role_ext(uint8 *exist_reconn_phone)
{
    int temp_dev_role = 0;

	temp_dev_role = bt_manager_tws_get_dev_role();
	if (temp_dev_role)
	{
       return temp_dev_role;
	}
    int cnt = 0;
	int i;
	struct autoconn_info *info;

	info = mem_malloc(sizeof(struct autoconn_info)*BT_MAX_AUTOCONN_DEV);
	if (info == NULL) {
		SYS_LOG_ERR("malloc failed");
		return 0;
	}

	cnt = btif_br_get_auto_reconnect_info(info, BT_MAX_AUTOCONN_DEV);

    if(cnt > 0){
        for (i = 0; i < BT_MAX_AUTOCONN_DEV; i++){
            if(info[i].addr_valid &&
                ((info[i].tws_role == BTSRV_TWS_MASTER) || (info[i].tws_role == BTSRV_TWS_SLAVE))){

				if (info[i].tws_role == BTSRV_TWS_SLAVE)
				{
					temp_dev_role = BTSRV_TWS_TEMP_SLAVE;
				}
				else if (info[i].tws_role == BTSRV_TWS_MASTER)
				{
					temp_dev_role = BTSRV_TWS_TEMP_MASTER;
				}
                //break;
            }
		    if (info[i].addr_valid &&
			    (info[i].tws_role == BTSRV_TWS_NONE) &&
                (info[i].a2dp || info[i].avrcp || info[i].hfp)) {
				 *exist_reconn_phone = 1;
				}
        }
    }
	mem_free(info);
	return temp_dev_role;
}

void bt_manager_tws_set_leaudio_active(int8_t leaudio_active)
{
	btif_tws_set_leaudio_active(leaudio_active);
}

int bt_manager_tws_register_config_expect_role_cb(tws_config_expect_role_cb_t cb)
{
    struct btmgr_tws_context_t* context = btmgr_get_tws_context();

	if (context)
	{
	    context->expect_role_cb = cb;
	}
	return 0;
}
