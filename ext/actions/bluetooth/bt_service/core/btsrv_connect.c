/*
 * Copyright (c) 2016 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt connect/disconnect service
 */

#define SYS_LOG_DOMAIN "btsrv_connect"
#include "btsrv_os_common.h"
#include "btsrv_inner.h"
#include "keys_br_store.h"


#define TWS_CONNECT_TRY_TIMES                   (5)
#define AUTOCONN_START_TIME						(100)	/* 100ms */
#define AUTOCONN_GET_NAME_TIME					(200)	/* 200ms */
#define AUTOCONN_WAIT_IDLE                      (100)   /* 100ms */
#define AUTOCONN_MASTER_DELAY					(2000)	/* 1000ms */
#define AUTOCONN_DELAY_TIME_MASK				(5000)	/* 5000ms */
#define AUTOCONN_TWS_DELAY_MASK			        (1000)	/* 2000ms */
#define AUTOCONN_TWS_WAIT_ROLE                  (3000)  /* 3000ms */
#define AUTOCONN_TWS_SEARCH_MODE_TIMEOUT       (3000)  /* 3000ms */
#define MONITOR_A2DP_TIMES                        (40)    /* 8000ms */
#define MONITOR_A2DP_NOSECURITY_TIMES            (20)   /* 4000ms */
#define MONITOR_AVRCP_TIMES                       (30)   /* 6000ms */
#define MONITOR_AVRCP_CONNECT_INTERVAL          (15)    /* 3000ms */
#define MONITOR_PRIFILE_INTERVEL	                (200)  /* 500ms */

#define BT_SUPERVISION_TIMEOUT					(8000)		/* 8000*0.625ms = 5000ms */
#define BT_CONNECT_PENDING_TIMEOUT			    (3000)

enum {
	AUTOCONN_STATE_IDLE,
	AUTOCONN_STATE_TWS_CONNECTING,
	AUTOCONN_STATE_PHONE_CONNECTING,
	AUTOCONN_STATE_BASE_CONNECTING,
	AUTOCONN_STATE_BASE_CONNECTED,
	AUTOCONN_STATE_PROFILE_CONNETING,
    AUTOCONN_STATE_POWER_OFF,
    AUTOCONN_STATE_TWS_ROLE_CONFIRM,
	AUTOCONN_STATE_END,
};

enum {
	AUTOCONN_PROFILE_IDLE,
	AUTOCONN_PROFILE_HFP_CONNECTING,
	AUTOCONN_PROFILE_A2DP_CONNECTING,
	AUTOCONN_PROFILE_AVRCP_CONNECTING,
	AUTOCONN_PROFILE_HID_CONNECTING,
	AUTOCONN_PROFILE_CONNECTING_MAX,
};

enum {
	AUTOCONN_RECONNECT_CLEAR_ALL,
	AUTOCONN_RECONNECT_CLEAR_PHONE,
	AUTOCONN_RECONNECT_CLEAR_TWS,
};

enum {
	SWITCH_SBC_STATE_IDLE,
	SWITCH_SBC_STATE_DISCONNECTING_A2DP,
	SWITCH_SBC_STATE_CONNECTING_A2DP,
};

struct auto_conn_t {
	bd_address_t addr;
	uint8_t addr_valid:1;
	uint8_t tws_role:3;
	uint8_t a2dp:1;
	uint8_t avrcp:1;
	uint8_t hfp:1;
	uint8_t hfp_first:1;
	uint8_t hid:1;
    uint8_t first_reconnect:1;
    uint8_t remote_connect_req:1;
    uint8_t profile_connect_wait:1;
	uint8_t tws_mode;
	uint8_t strategy;
	uint8_t base_try;
	uint8_t profile_try;
	uint8_t curr_connect_profile;
	uint8_t state;
    uint8_t reason_times;
	uint16_t base_interval;
	uint16_t profile_interval;
    uint8_t reconnect_phone_timeout;
    uint8_t reconnect_tws_timeout;
    uint8_t base_reconnect_times;
    uint8_t profile_reconnect_times;
    uint8_t last_reason;
    uint32_t last_begin_time;
	uint32_t session_id;
};

struct profile_conn_t {
	bd_address_t addr;
	uint8_t valid:1;
	uint8_t avrcp_times;
	uint8_t a2dp_times;
};

struct btsrv_connect_priv {
	struct autoconn_info nvram_reconn_info[BTSRV_SAVE_AUTOCONN_NUM];	/* Reconnect info save in nvram */
	struct auto_conn_t auto_conn[BTSRV_SAVE_AUTOCONN_NUM];				/* btsvr connect use for doing reconnect bt */
	struct thread_timer auto_conn_timer;
	struct auto_conn_t *curr_conn;
	uint8_t connecting_index:3;
	uint8_t auto_connect_running:1;
	uint8_t connect_is_pending:1;
	uint8_t clear_list_disconnecting:1;
	uint8_t clear_list_mode:3;
    uint8_t gfp_running:1;

	/* Monitor connect profile, connect by phone */
	struct profile_conn_t monitor_conn[BTSRV_SAVE_AUTOCONN_NUM];
	struct thread_timer monitor_conn_timer;
	uint8_t monitor_timer_running:1;
	uint8_t curr_req_performance:1;
	uint8_t reconnect_req_high_performance:1;
	uint8_t reconnect_br_connect:1;
    uint8_t reconnect_ramdon_delay:1;
    uint32_t connect_pending_time;
};


static struct btsrv_connect_priv *p_connect;
static struct btsrv_connect_priv p_btsrv_connect;

static const btsrv_event_strmap_t btsrv_link_event_map[] =
{
    {BTSRV_LINK_BASE_CONNECTED,             STRINGIFY(BTSRV_LINK_BASE_CONNECTED)},
    {BTSRV_LINK_BASE_CONNECTED_FAILED,    	STRINGIFY(BTSRV_LINK_BASE_CONNECTED_FAILED)},
    {BTSRV_LINK_BASE_CONNECTED_TIMEOUT,     STRINGIFY(BTSRV_LINK_BASE_CONNECTED_TIMEOUT)},
    {BTSRV_LINK_BASE_DISCONNECTED,          STRINGIFY(BTSRV_LINK_BASE_DISCONNECTED)},
    {BTSRV_LINK_BASE_GET_NAME,              STRINGIFY(BTSRV_LINK_BASE_GET_NAME)},
    {BTSRV_LINK_HFP_CONNECTED,              STRINGIFY(BTSRV_LINK_HFP_CONNECTED)},
    {BTSRV_LINK_HFP_DISCONNECTED,           STRINGIFY(BTSRV_LINK_HFP_DISCONNECTED)},
    {BTSRV_LINK_A2DP_CONNECTED,             STRINGIFY(BTSRV_LINK_A2DP_CONNECTED)},
	{BTSRV_LINK_A2DP_DISCONNECTED,          STRINGIFY(BTSRV_LINK_A2DP_DISCONNECTED)},
    {BTSRV_LINK_AVRCP_CONNECTED,            STRINGIFY(BTSRV_LINK_AVRCP_CONNECTED)},
    {BTSRV_LINK_AVRCP_DISCONNECTED,         STRINGIFY(BTSRV_LINK_AVRCP_DISCONNECTED)},
    {BTSRV_LINK_SPP_CONNECTED,              STRINGIFY(BTSRV_LINK_SPP_CONNECTED)},
	{BTSRV_LINK_SPP_DISCONNECTED,           STRINGIFY(BTSRV_LINK_SPP_DISCONNECTED)},
    {BTSRV_LINK_PBAP_CONNECTED,             STRINGIFY(BTSRV_LINK_PBAP_CONNECTED)},
    {BTSRV_LINK_PBAP_DISCONNECTED,          STRINGIFY(BTSRV_LINK_PBAP_DISCONNECTED)},
    {BTSRV_LINK_HID_CONNECTED,              STRINGIFY(BTSRV_LINK_HID_CONNECTED)},
	{BTSRV_LINK_HID_DISCONNECTED,           STRINGIFY(BTSRV_LINK_HID_DISCONNECTED)},
    {BTSRV_LINK_MAP_CONNECTED,              STRINGIFY(BTSRV_LINK_MAP_CONNECTED)},
    {BTSRV_LINK_MAP_DISCONNECTED,           STRINGIFY(BTSRV_LINK_MAP_DISCONNECTED)},
    {BTSRV_LINK_TWS_CONNECTED,              STRINGIFY(BTSRV_LINK_TWS_CONNECTED)},
    {BTSRV_LINK_TWS_CONNECTED_TIMEOUT,      STRINGIFY(BTSRV_LINK_TWS_CONNECTED_TIMEOUT)},
	{BTSRV_LINK_FORCE_STOP,      			STRINGIFY(BTSRV_LINK_FORCE_STOP)},
};


#define BTSRV_LINK_EVENTNUM_STRS (sizeof(btsrv_link_event_map) / sizeof(btsrv_event_strmap_t))
const char *btsrv_link_evt2str(int num)
{
    return bt_service_evt2str(num, BTSRV_LINK_EVENTNUM_STRS, btsrv_link_event_map);
}

static void btsrv_update_performance_req(void)
{
	uint8_t need_high;
	bool rdm_need_high_performance = btsrv_rdm_need_high_performance();

	if (rdm_need_high_performance || p_connect->reconnect_req_high_performance) {
		need_high = 1;
	} else {
		need_high = 0;
	}

	if (need_high && !p_connect->curr_req_performance) {
		p_connect->curr_req_performance = 1;
		SYS_LOG_INF("BTSRV_REQ_HIGH_PERFORMANCE\n");
		btsrv_adapter_callback(BTSRV_REQ_HIGH_PERFORMANCE, NULL);
	} else if (!need_high && p_connect->curr_req_performance) {
		p_connect->curr_req_performance = 0;
		SYS_LOG_INF("BTSRV_RELEASE_HIGH_PERFORMANCE\n");
		btsrv_adapter_callback(BTSRV_RELEASE_HIGH_PERFORMANCE, NULL);
	}
}

static void btsrv_update_nvram_auto_conn_info(uint8_t mode)
{
    if((mode & AUTOCONN_INFO_PHONE) != 0){
	    btsrv_property_set(CFG_AUTOCONN_INFO,
			(void *)&p_connect->nvram_reconn_info[1],
            sizeof(struct autoconn_info) * (BTSRV_SAVE_AUTOCONN_NUM - 1));
    }

    if((mode & AUTOCONN_INFO_TWS) != 0){
	    btsrv_property_set_factory(CFG_BT_AUTOCONN_TWS,
			(void *)&p_connect->nvram_reconn_info[0],
            sizeof(struct autoconn_info));
    }
}

void btsrv_sync_remote_paired_list_info(struct autoconn_info *info, uint8_t max_cnt)
{
	uint8_t i,j;
	uint8_t cnt = (max_cnt > BTSRV_SAVE_AUTOCONN_NUM) ? BTSRV_SAVE_AUTOCONN_NUM : max_cnt;

	for (int i = 0; i < BTSRV_SAVE_AUTOCONN_NUM; i++) {
		if(p_connect->nvram_reconn_info[i].tws_role == BTSRV_TWS_NONE){
			memset(&p_connect->nvram_reconn_info[i], 0, sizeof(struct autoconn_info));
		}
	}

    for (i = 0; i < cnt; i++) {
        if ((info[i].addr_valid) && (info[i].tws_role == BTSRV_TWS_NONE)){
			for(j = 0; j < BTSRV_SAVE_AUTOCONN_NUM; j++){
		        if((p_connect->nvram_reconn_info[j].tws_role == BTSRV_TWS_SLAVE) 
			        || (p_connect->nvram_reconn_info[j].tws_role == BTSRV_TWS_MASTER)
			        || (p_connect->nvram_reconn_info[j].addr_valid)){
			        continue;
		        }
                memcpy(&p_connect->nvram_reconn_info[j], &info[i], sizeof(struct autoconn_info));
                break;
            }
        }
	}

	btsrv_update_nvram_auto_conn_info(AUTOCONN_INFO_PHONE);
}
int btsrv_connect_get_auto_reconnect_info(struct autoconn_info *info, uint8_t max_cnt)
{
	int dev_cnt = 0, read_cnt, i;

	read_cnt = (max_cnt > BTSRV_SAVE_AUTOCONN_NUM) ? BTSRV_SAVE_AUTOCONN_NUM : max_cnt;
	memcpy((char *)info, (char *)p_connect->nvram_reconn_info, (sizeof(struct autoconn_info)*read_cnt));
	for (i = 0; i < read_cnt; i++) {
		if (info[i].addr_valid) {
			dev_cnt++;
		}
	}
//btsrv_connect_dump_info();
	return dev_cnt;
}

struct autoconn_info * btsrv_connect_get_tws_paired_info(void)
{
	int i;
    struct autoconn_info *info = NULL;
	for (i = 0; i < BTSRV_SAVE_AUTOCONN_NUM; i++) {
		if (p_connect->nvram_reconn_info[i].addr_valid) {
			if(p_connect->nvram_reconn_info[i].tws_role != BTSRV_TWS_NONE){
				info = &(p_connect->nvram_reconn_info[i]);
				break;
			}
		}
	}
	return info;
}

struct autoconn_info *  btsrv_connect_get_phone_paired_info(void)
{
	int i;
    struct autoconn_info *info = NULL;
	for (i = 0; i < BTSRV_SAVE_AUTOCONN_NUM; i++) {
		if (p_connect->nvram_reconn_info[i].addr_valid) {
			if(p_connect->nvram_reconn_info[i].tws_role == BTSRV_TWS_NONE){
				info = &(p_connect->nvram_reconn_info[i]);
				break;
			}
		}
	}
	return info;
}

void btsrv_connect_set_auto_reconnect_info(struct autoconn_info *info, uint8_t max_cnt,uint8_t mode)
{
	uint8_t write_cnt;
    uint8_t i,j;
    uint8_t update_mode = 0;

	write_cnt = (max_cnt > BTSRV_SAVE_AUTOCONN_NUM) ? BTSRV_SAVE_AUTOCONN_NUM : max_cnt;
    if((mode & AUTOCONN_INFO_TWS) != 0){
        for(i = 0; i < write_cnt; i++){
			if(info[i].addr_valid == 0){
				continue;
			}
		    if((info[i].tws_role == BTSRV_TWS_MASTER) || (info[i].tws_role == BTSRV_TWS_SLAVE)){
	            memcpy((char *)&p_connect->nvram_reconn_info[0], (char *)&info[i], (sizeof(struct autoconn_info)));
				update_mode |= AUTOCONN_INFO_TWS;
				break;
		    }
        }
    }
    if((mode & AUTOCONN_INFO_PHONE) != 0){
		j = 1;
        for(i = 0; i < write_cnt; i++){
            if(info[i].addr_valid == 0){
				continue;
			}
		    if(info[i].tws_role == BTSRV_TWS_NONE){
	            memcpy((char *)&p_connect->nvram_reconn_info[j], (char *)&info[i], (sizeof(struct autoconn_info)));
                update_mode |= AUTOCONN_INFO_PHONE;
                j++;
				if(j >= BTSRV_SAVE_AUTOCONN_NUM){
					break;
				}
		    }
        }
    }
    
	SYS_LOG_INF("set_reconn_info_nvram:%d\n",update_mode);
	btsrv_update_nvram_auto_conn_info(update_mode);
	//btsrv_connect_dump_info();
}

void btsrv_autoconn_info_active_clear_check(void)
{
	struct autoconn_info *info;
	struct bt_conn *conn;

	info = btsrv_connect_get_phone_paired_info();
	if (!info) {
		SYS_LOG_ERR("clear_active failed");
		return;
	}

	for (int i = 0; i < BTSRV_SAVE_AUTOCONN_NUM; i++){
		if (!info[i].addr_valid){
			continue;
		}

		conn = btsrv_rdm_find_conn_by_addr(&info[i].addr);
		if ((conn) &&
			(!btsrv_a2dp_stream_open_used_start(conn))) {
			info[i].active = 0;
		}
	}

	btsrv_update_nvram_auto_conn_info(AUTOCONN_INFO_PHONE);
	SYS_LOG_INF("");
}

void btsrv_autoconn_info_active_update(void)
{
	struct autoconn_info *info;
	struct bt_conn *active_conn = btsrv_rdm_a2dp_get_active_dev();
	struct bt_conn *second_conn = btsrv_rdm_a2dp_get_second_dev();
    uint8_t *active_addr = NULL;
    uint8_t *second_addr = NULL;

	if(!btif_br_get_autoconn_info_need_update()) {
		SYS_LOG_INF("active don't need update");
		return;
	}

	info = btsrv_connect_get_phone_paired_info();
	if (!info) {
		SYS_LOG_ERR("update_active failed");
		return;
	}

    active_addr = btsrv_rdm_get_addr_by_conn(active_conn);
    second_addr = btsrv_rdm_get_addr_by_conn(second_conn);

	for (int i = 0; i < BTSRV_SAVE_AUTOCONN_NUM; i++){
		if (!info[i].addr_valid){
			continue;
		}

		info[i].active = 0;

		if (active_addr && !memcmp(info[i].addr.val, active_addr, sizeof(bd_address_t))){
			info[i].active = btsrv_rdm_a2dp_get_active(active_conn);
		}else if (second_addr && !memcmp(info[i].addr.val, second_addr, sizeof(bd_address_t))){
			info[i].active = btsrv_rdm_a2dp_get_active(second_conn);
		}
	}

	btsrv_update_nvram_auto_conn_info(AUTOCONN_INFO_PHONE);
	SYS_LOG_INF("");
}

void btsrv_autoconn_info_update(void)
{
	struct autoconn_info *info, *tmpInfo;
    uint8_t connected_cnt, j, tws_paired = 0;
	int8_t i;
    uint8_t mode = 0;
    uint8_t phone_num = 0;

	if(!btif_br_get_autoconn_info_need_update()) {
		SYS_LOG_INF("info don't need update");
		return;
	}

	info = mem_malloc(sizeof(struct autoconn_info)*BTSRV_SAVE_AUTOCONN_NUM*2);
	if (!info) {
		SYS_LOG_ERR("info_update failed");
		goto update_exit;
	}

	tmpInfo = info;
	tmpInfo += BTSRV_SAVE_AUTOCONN_NUM;
	memset(info, 0, (sizeof(struct autoconn_info)*BTSRV_SAVE_AUTOCONN_NUM*2));
	connected_cnt = btsrv_rdm_get_autoconn_dev(tmpInfo, BTSRV_SAVE_AUTOCONN_NUM);
	if (connected_cnt == 0) {
		goto update_exit;
	}

	/* Only save one tws device info */
	for (i = 0; i < connected_cnt; i++) {
		if((tmpInfo[i].tws_role == BTSRV_TWS_SLAVE) || (tmpInfo[i].tws_role == BTSRV_TWS_MASTER)){
			tws_paired = 1;
        
		#ifdef CONFIG_USE_SHARE_TWS_MAC 
			if(tmpInfo[i].tws_role == BTSRV_TWS_MASTER){
				btsrv_get_local_mac(&tmpInfo[i].addr); //将列表中地址替换成自己
			}
        #endif
			memcpy(&info[0],&tmpInfo[i],sizeof(struct autoconn_info));
            break;
		}
	}

	j = 1;
	//for (i = 0; i < connected_cnt; i++) {
    for (i = (connected_cnt - 1); i >= 0; i--) {
	    if(tmpInfo[i].tws_role == BTSRV_TWS_NONE){
            memcpy(&info[j],&tmpInfo[i],sizeof(struct autoconn_info));
            j++;
		    phone_num++;
		    if(j >= BTSRV_SAVE_AUTOCONN_NUM){
				break;
		    }
        }
	}

	btsrv_connect_get_auto_reconnect_info(tmpInfo, BTSRV_SAVE_AUTOCONN_NUM);
	if((tws_paired == 0) && (tmpInfo[0].addr_valid)){
		memcpy(&info[0], &tmpInfo[0], sizeof(struct autoconn_info));
	}

	for (i = 1; i < BTSRV_SAVE_AUTOCONN_NUM; i++){
		if (tmpInfo[i].addr_valid){
			//for (j = 1; j < BTSRV_SAVE_AUTOCONN_NUM; j++) {
			for (j = 1; j <= phone_num; j++) {
				if (!memcmp(tmpInfo[i].addr.val, info[j].addr.val, sizeof(bd_address_t))){
					info[j].a2dp |= tmpInfo[i].a2dp;
					info[j].avrcp |= tmpInfo[i].avrcp;
					info[j].hfp |= tmpInfo[i].hfp;
					info[j].hid |= tmpInfo[i].hid;
					info[j].active |= tmpInfo[i].active;
					tmpInfo[i].addr_valid = 0;
					break;
				}
			}
			//if ((j == BTSRV_SAVE_AUTOCONN_NUM) && (phone_num < (BTSRV_SAVE_AUTOCONN_NUM -1))){
			if ((j == (phone_num + 1)) && (phone_num < (BTSRV_SAVE_AUTOCONN_NUM -1))){
				memcpy(&info[phone_num + 1], &tmpInfo[i], sizeof(struct autoconn_info));
				phone_num++;
			}
		}
	}

	btsrv_connect_get_auto_reconnect_info(tmpInfo, BTSRV_SAVE_AUTOCONN_NUM);
    for (j = 0; j < BTSRV_SAVE_AUTOCONN_NUM; j++){
        if(memcmp(&info[j],&tmpInfo[j],sizeof(struct autoconn_info))){
            if(info[j].tws_role == BTSRV_TWS_NONE){
                mode |= AUTOCONN_INFO_PHONE;
            }
            else{
                mode |= AUTOCONN_INFO_TWS;
            }
        }
    }
#ifndef CONFIG_USE_SHARE_TWS_MAC

    if ((btsrv_rdm_get_dev_role() == BTSRV_TWS_SLAVE) && ((mode & AUTOCONN_INFO_PHONE) != 0)) {
	    SYS_LOG_INF("mode:%d\n",mode);
	        mode &= (~AUTOCONN_INFO_PHONE);
		SYS_LOG_INF("Do not store phone info into vram:%d\n",mode);	 
	}	
#endif 
    if(mode){
	    btsrv_connect_set_auto_reconnect_info(info, BTSRV_SAVE_AUTOCONN_NUM,mode);
    }

update_exit:
	if (info) {
		mem_free(info);
	}
}

static void btsrv_autoconn_info_clear(void)
{
    uint8_t mode = 0;

	for (int i = 0; i < BTSRV_SAVE_AUTOCONN_NUM; i++) {
		if ((p_connect->nvram_reconn_info[i].tws_role != BTSRV_TWS_NONE && p_connect->clear_list_mode == BTSRV_DEVICE_TWS)
			|| (p_connect->nvram_reconn_info[i].tws_role == BTSRV_TWS_NONE && p_connect->clear_list_mode == BTSRV_DEVICE_PHONE)
			|| (p_connect->clear_list_mode == BTSRV_DEVICE_ALL)) {
			if(p_connect->nvram_reconn_info[i].tws_role == BTSRV_TWS_NONE){
				mode |= AUTOCONN_INFO_PHONE;
			}
            else{
			    mode |= AUTOCONN_INFO_TWS;
            }
			memset(&p_connect->nvram_reconn_info[i], 0, sizeof(struct autoconn_info));
			continue;
		}
	}
	//p_connect->clear_list_mode = 0;
	SYS_LOG_INF("nvram_cls:%d \n",mode);
	btsrv_update_nvram_auto_conn_info(mode);
	if (p_connect->clear_list_mode == BTSRV_DEVICE_ALL) {
        btsrv_storage_clean_linkkey();
	}
    p_connect->clear_list_mode = 0;
#ifdef CONFIG_SUPPORT_TWS
	if((mode & AUTOCONN_INFO_TWS) != 0){
	   btsrv_event_notify(MSG_BTSRV_TWS, MSG_BTSRV_TWS_CLEAR_LIST_COMPLETE, NULL);
	}
#endif		

}

void btsrv_connect_auto_connection_stop(int mode)
{
	int i;

	SYS_LOG_INF("stop %d",mode);

	for (i = 0; i < BTSRV_SAVE_AUTOCONN_NUM; i++) {
        if((mode & BTSRV_STOP_AUTO_RECONNECT_PHONE) != 0){
            if (p_connect->auto_conn[i].addr_valid
                && (p_connect->auto_conn[i].tws_mode == BTSRV_TWS_RECONNECT_NONE)){
                if(p_connect->auto_conn[i].state == AUTOCONN_STATE_BASE_CONNECTING){
                    btsrv_adapter_check_cancal_connect(&p_connect->auto_conn[i].addr);
                }
				if((mode & BTSRV_SYNC_AUTO_RECONNECT_STATUS) != 0){
					btsrv_tws_sync_reconnect_info(&p_connect->auto_conn[i].addr);
				}

                memset(&p_connect->auto_conn[i], 0, sizeof(struct auto_conn_t));
            }
            p_connect->reconnect_br_connect = 0;
        }

        if((mode & BTSRV_STOP_AUTO_RECONNECT_TWS) != 0){
            if(p_connect->auto_conn[i].tws_mode != BTSRV_TWS_RECONNECT_NONE){
                if(btsrv_tws_is_in_pair_searching()){
                    btif_tws_end_pair();
                }
                if(btsrv_tws_is_in_connecting()){
                    btsrv_tws_cancal_auto_connect();
                }
                memset(&p_connect->auto_conn[i], 0, sizeof(struct auto_conn_t));
            }
        }
	}

    if((mode & BTSRV_STOP_AUTO_RECONNECT_ALL) == BTSRV_STOP_AUTO_RECONNECT_ALL){
		if (thread_timer_is_running(&p_connect->auto_conn_timer)) {
			btsrv_event_notify(MSG_BTSRV_BASE, MSG_BTSRV_AUTO_RECONNECT_COMPLETE, (void *)1);
		}
		thread_timer_stop(&p_connect->auto_conn_timer);
		p_connect->reconnect_req_high_performance = 0;
		p_connect->reconnect_ramdon_delay = 0;
		p_connect->curr_conn = NULL;
		btsrv_adapter_set_reconnect_state(false);
        btsrv_update_performance_req();
    }
	btsrv_scan_update_mode(false);
}

void btsrv_connect_remote_connect_stop_except_device(bd_address_t *addr)
{
	int i;
	int ret = 0;
    int req = 0;

	for (i = 0; i < BTSRV_SAVE_AUTOCONN_NUM; i++) {
		if (p_connect->auto_conn[i].addr_valid
			&& (p_connect->auto_conn[i].tws_mode == BTSRV_TWS_RECONNECT_NONE)){
			if(!memcmp(&p_connect->auto_conn[i].addr,addr,sizeof(bd_address_t))) {
				ret = 1;
				req = i;
				continue;
			}

			if(p_connect->auto_conn[i].state == AUTOCONN_STATE_BASE_CONNECTING){
				btsrv_adapter_check_cancal_connect(&p_connect->auto_conn[i].addr);
			}

			memset(&p_connect->auto_conn[i], 0, sizeof(struct auto_conn_t));
		}

		if(p_connect->auto_conn[i].tws_mode != BTSRV_TWS_RECONNECT_NONE){
			if(!memcmp(&p_connect->auto_conn[i].addr,addr,sizeof(bd_address_t))) {
				ret = 1;
				req = i;
				continue;
			}

			if(btsrv_tws_is_in_pair_searching()){
				btif_tws_end_pair();
			}
			if(btsrv_tws_is_in_connecting()){
				btsrv_tws_cancal_auto_connect();
			}
			memset(&p_connect->auto_conn[i], 0, sizeof(struct auto_conn_t));
		}
	}

	SYS_LOG_INF("ret:%d req:%d",ret,req);

	if(!ret) {
		p_connect->reconnect_br_connect = false;
        thread_timer_stop(&p_connect->auto_conn_timer);
        p_connect->reconnect_req_high_performance = 0;
        p_connect->reconnect_ramdon_delay = 0;
		p_connect->curr_conn = NULL;
		btsrv_adapter_set_reconnect_state(false);
        btsrv_update_performance_req();
    }
    else{
        p_connect->auto_conn[req].remote_connect_req = 1;
    }
	btsrv_scan_update_mode(false);
}

void btsrv_connect_auto_connection_cancel_device(void)
{
	int i;
	int ret = 0;

	for (i = 0; i < BTSRV_SAVE_AUTOCONN_NUM; i++) {
		if ((p_connect->auto_conn[i].addr_valid) &&
			(p_connect->auto_conn[i].tws_mode == BTSRV_TWS_RECONNECT_NONE) &&
			(p_connect->auto_conn[i].state == AUTOCONN_STATE_BASE_CONNECTING)) {
				btsrv_adapter_check_cancal_connect(&p_connect->auto_conn[i].addr);
				ret = 1;
		}
	}

	if (ret) {
		SYS_LOG_INF("connection cancel device");
		btsrv_scan_update_mode(false);
	}
}

void btsrv_connect_auto_connection_stop_device(bd_address_t *addr)
{
	int i;

	if(!addr){
		return;
	}

    for (i = 0; i < BTSRV_SAVE_AUTOCONN_NUM; i++){
        if(p_connect->auto_conn[i].addr_valid){
            if(!memcmp(&p_connect->auto_conn[i].addr,addr,sizeof(bd_address_t))){
                if(p_connect->auto_conn[i].tws_mode == BTSRV_TWS_RECONNECT_NONE){
                    if(p_connect->auto_conn[i].state == AUTOCONN_STATE_BASE_CONNECTING){
                        btsrv_adapter_check_cancal_connect(&p_connect->auto_conn[i].addr);
                    }
                }
                else{
                    if(btif_tws_is_in_pair_seraching()){
                        btif_tws_end_pair();
                    }
                    if(btsrv_tws_is_in_connecting()){
                        btsrv_tws_cancal_auto_connect();
                    }
                }
				btsrv_proc_link_change(addr->val, BTSRV_LINK_FORCE_STOP);
                memset(&p_connect->auto_conn[i], 0, sizeof(struct auto_conn_t));
                break;
            }
        }
    }
	btsrv_scan_update_mode(false);
}

void btsrv_connect_auto_connection_clear_device(bd_address_t *addr)
{
	char addrstr[BT_ADDR_STR_LEN];

	hostif_bt_addr_to_str((const bt_addr_t *)addr, addrstr, BT_ADDR_STR_LEN);
	SYS_LOG_INF("clear %s ", addrstr);

	btsrv_connect_auto_connection_stop_device(addr);

	for (int i = 0; i < BTSRV_SAVE_AUTOCONN_NUM; i++) {
		if (p_connect->nvram_reconn_info[i].tws_role == BTSRV_TWS_NONE &&
			p_connect->nvram_reconn_info[i].addr_valid) {

			if(memcmp(&p_connect->nvram_reconn_info[i].addr, addr, sizeof(bd_address_t)) == 0){
				memset(&p_connect->nvram_reconn_info[i], 0, sizeof(struct autoconn_info));
				SYS_LOG_INF("clear %s xxxx", addrstr);
				break;
			}
		}
	}
	btsrv_update_nvram_auto_conn_info(AUTOCONN_INFO_PHONE);
}

static void btsrv_connect_auto_connection_restart(int32_t duration, int32_t period)
{
	SYS_LOG_INF("connection_restart %d, %d", duration, period);
	thread_timer_start(&p_connect->auto_conn_timer, duration, period);
	p_connect->reconnect_req_high_performance = 1;

	if (!(btsrv_adapter_get_pair_state() & BT_PAIR_STATUS_RECONNECT))
	{
        btsrv_adapter_set_reconnect_state(true);
	}
	btsrv_update_performance_req();
}

static void btsrv_connect_monitor_profile_stop(void)
{
	SYS_LOG_INF("monitor_profile_stop");
	thread_timer_stop(&p_connect->monitor_conn_timer);
}

static void btsrv_connect_monitor_profile_start(int32_t duration, int32_t period)
{
	if (thread_timer_is_running(&p_connect->monitor_conn_timer)){
		return;
	}

	SYS_LOG_INF("monitor_profile_start %d, %d", duration, period);
	thread_timer_start(&p_connect->monitor_conn_timer, duration, period);
}

static struct auto_conn_t *btsrv_autoconn_find_wait_reconn(void)
{
    struct auto_conn_t* curr_conn = NULL;
    
    int      index = p_connect->connecting_index;
    int      i;
    uint32_t curr_time = os_uptime_get_32();

    uint8_t  curr_addr_valid   = p_connect->auto_conn[index].addr_valid;
    uint8_t  curr_tws_mode     = p_connect->auto_conn[index].tws_mode;
    uint8_t  curr_reconn_times = p_connect->auto_conn[index].base_reconnect_times;

    /* clear the reconnect end devices
     */
    for (i = 0; i < BTSRV_SAVE_AUTOCONN_NUM; i++)
    {
        struct auto_conn_t* p = &p_connect->auto_conn[i];

        if (p->state >= AUTOCONN_STATE_END)
        {
            memset(p, 0, sizeof(struct auto_conn_t));
        }
    }
    
    if (p_connect->auto_conn[index].addr_valid)
    {
        struct auto_conn_t* p = &p_connect->auto_conn[index];
        curr_conn = p;

        /* current reconnect device need begin?
         */
        if (p->last_begin_time == 0 ||
            curr_time - p->last_begin_time >= p->base_interval * 100)
        {
            return p;
        }
    }

    /* current reconnect phone device is idle? try to switch reconnect TWS device
     */
    if (curr_addr_valid &&
        curr_tws_mode == BTSRV_TWS_RECONNECT_NONE)
    {
        for (i = 0; i < BTSRV_SAVE_AUTOCONN_NUM; i++)
        {
            struct auto_conn_t* p = &p_connect->auto_conn[i];

            if (p->addr_valid &&
                p->tws_mode != BTSRV_TWS_RECONNECT_NONE)
            {
                /* reconnect TWS device need begin?
                 */
                if (p->last_begin_time == 0 ||
                    curr_time - p->last_begin_time >= p->base_interval * 100)
                {
                    p_connect->connecting_index = i;
                    
                    return p;
                }
            }
        }
    }

    /* current reconnect TWS device is idle? try to switch reconnect phone device
     */
    if (curr_addr_valid &&
        curr_tws_mode != BTSRV_TWS_RECONNECT_NONE)
    {
        for (i = 0; i < BTSRV_SAVE_AUTOCONN_NUM; i++)
        {
            struct auto_conn_t* p = &p_connect->auto_conn[index];
    
            if (p->addr_valid &&
                p->tws_mode == BTSRV_TWS_RECONNECT_NONE)
            {
                /* reconnect phone device need begin?
                 */
                if (p->last_begin_time == 0 ||
                    curr_time - p->last_begin_time >= p->base_interval * 100)
                {
                    p_connect->connecting_index = index;
                    
                    return p;
                }
            }

            /* interlaced reconnect TWS and phone device
             */
            if ((curr_reconn_times % 2) != 0)
            {
                index += 1;

                if (index >= BTSRV_SAVE_AUTOCONN_NUM)
                    index = 0;
            }
            else
            {
                index -= 1;

                if (index < 0)
                    index = BTSRV_SAVE_AUTOCONN_NUM - 1;
            }
        }
    }

    index = p_connect->connecting_index;
    
    for (i = 0; i < BTSRV_SAVE_AUTOCONN_NUM; i++)
    {
        struct auto_conn_t* p = &p_connect->auto_conn[index];

        if (p->addr_valid)
        {
            /* reconnect device need begin?
             */
            if (p->last_begin_time == 0 ||
                curr_time - p->last_begin_time >= p->base_interval * 100)
            {
                p_connect->connecting_index = index;
                
                return p;
            }
        }

        index = (index + 1) % BTSRV_SAVE_AUTOCONN_NUM;
    }

    if (curr_conn != NULL)
    {
        return curr_conn;
    }

    index = p_connect->connecting_index;
    
    for (i = 0; i < BTSRV_SAVE_AUTOCONN_NUM; i++)
    {
        struct auto_conn_t* p = &p_connect->auto_conn[index];

        if (p->addr_valid)
        {
            p_connect->connecting_index = index;
            
            return p;
        }

        index = (index + 1) % BTSRV_SAVE_AUTOCONN_NUM;
    }

    return NULL;
}

static void btsrv_update_autoconn_state(uint8_t *addr, uint8_t event)
{
	uint8_t i;
	uint8_t index = BTSRV_SAVE_AUTOCONN_NUM;
	uint8_t wait_reconn_dev_cnts = 0;

	for (i = 0; i < BTSRV_SAVE_AUTOCONN_NUM; i++) {
		if (p_connect->auto_conn[i].addr_valid &&
			!memcmp(p_connect->auto_conn[i].addr.val, addr, sizeof(bt_addr_t))) {
			index = i;
			break;
		}

		if(p_connect->auto_conn[i].tws_mode == BTSRV_TWS_RECONNECT_AUTO_PAIR){
            if(!memcmp(&p_connect->auto_conn[i].addr.val[3], &addr[3], 3)){
			    index = i;
                break;
            }
		}
	}

	if (index == BTSRV_SAVE_AUTOCONN_NUM) {
		return;
	}

	for (i = 0; i < BTSRV_SAVE_AUTOCONN_NUM; i++){
		if (p_connect->auto_conn[i].addr_valid){
			wait_reconn_dev_cnts ++;
		}
	}

	switch (event) {
	case BTSRV_LINK_BASE_CONNECTED:
		if (p_connect->auto_conn[index].state < AUTOCONN_STATE_BASE_CONNECTED) {
			p_connect->auto_conn[index].state = AUTOCONN_STATE_BASE_CONNECTED;
		}
		p_connect->auto_conn[index].last_begin_time = os_uptime_get_32();

		if (index == p_connect->connecting_index) {
			/* After base connected, wait get name finish to trigger next step, set get name time */
            if(p_connect->auto_conn[index].tws_mode == BTSRV_TWS_RECONNECT_NONE){
                btsrv_connect_auto_connection_restart(AUTOCONN_GET_NAME_TIME, 0);
            }
            else{
                btsrv_connect_auto_connection_restart(AUTOCONN_TWS_WAIT_ROLE, 0);
            }
		}

		p_connect->reconnect_br_connect = false;
//		p_connect->auto_conn[index].first_reconnect = 0;
		break;

	case BTSRV_LINK_BASE_DISCONNECTED:
		p_connect->auto_conn[index].state = AUTOCONN_STATE_IDLE;

        /* stop reconnection when some reason disconnected
         */
		if (p_connect->auto_conn[index].reason_times >= 3)
		{
		    SYS_LOG_INF("%d, TERMINATE 0x%x", index, p_connect->auto_conn[index].last_reason);
		    p_connect->auto_conn[index].state = AUTOCONN_STATE_END;
		}

		if (index == p_connect->connecting_index)
		{
	        struct auto_conn_t* p = &p_connect->auto_conn[index];

            /* reconnect times out?
             */
			if (p->reason_times == 0 &&
			    p->base_try != 0 &&
			    p->base_reconnect_times >= p->base_try)
			{
			    SYS_LOG_INF("%d, TIMES_OUT %d", index, p->base_try);
				p->state = AUTOCONN_STATE_END;
			}

            /* switch reconnect device quickly */
			btsrv_connect_auto_connection_restart(1 /*AUTOCONN_START_TIME*/, 0);
		}

        p_connect->reconnect_br_connect = false;
		break;

	case BTSRV_LINK_BASE_GET_NAME:
        if((p_connect->auto_conn[index].tws_mode == BTSRV_TWS_RECONNECT_AUTO_PAIR)
		    || (p_connect->auto_conn[index].tws_mode == BTSRV_TWS_RECONNECT_FAST_PAIR)){
            p_connect->auto_conn[index].state = AUTOCONN_STATE_TWS_ROLE_CONFIRM;

            if (index == p_connect->connecting_index)
                btsrv_connect_auto_connection_restart(AUTOCONN_TWS_WAIT_ROLE, 0);
			break;
        }

		if (p_connect->auto_conn[index].state < AUTOCONN_STATE_PROFILE_CONNETING) {
			p_connect->auto_conn[index].state = AUTOCONN_STATE_PROFILE_CONNETING;
			/* set page timeout before create ACL connection */
            // hostif_bt_br_set_page_timeout(p_connect->auto_conn[index].reconnect_phone_timeout * 100);

            /* reset current reconnect profile */
            p_connect->auto_conn[index].curr_connect_profile = AUTOCONN_PROFILE_IDLE;
            p_connect->auto_conn[index].profile_reconnect_times = 0;
		}

		if (index == p_connect->connecting_index) {
		    /* start connect profile quickly */
            if(p_connect->auto_conn[index].remote_connect_req){
                p_connect->auto_conn[index].profile_connect_wait = 1;
                p_connect->auto_conn[index].state = AUTOCONN_STATE_END;
            }
            btsrv_connect_auto_connection_restart(1 /*AUTOCONN_START_TIME*/, 0);
		}
		break;

    case BTSRV_LINK_TWS_CONNECTED:
	case BTSRV_LINK_FORCE_STOP:
        /* TWS device reconnect complete */
        p_connect->auto_conn[index].state = AUTOCONN_STATE_END;
        
		if (index == p_connect->connecting_index)
		{
            /* switch reconnect device quickly */
			btsrv_connect_auto_connection_restart(1 /*AUTOCONN_START_TIME*/, 0);
		}
        break;
        
	case BTSRV_LINK_TWS_CONNECTED_TIMEOUT:
	    p_connect->auto_conn[index].state = AUTOCONN_STATE_IDLE;
	    
		if (index == p_connect->connecting_index)
	    {
	        struct auto_conn_t* p = &p_connect->auto_conn[index];

			/* reconnect times out?
             */
			if (p->base_try != 0 &&
			    p->base_reconnect_times >= p->base_try)
			{
			    SYS_LOG_INF("%d, TIMES_OUT %d, %d", index, p->base_try, os_uptime_get_32());
				p->state = AUTOCONN_STATE_END;
			}

            /* switch reconnect device quickly */
			btsrv_connect_auto_connection_restart(1 /*AUTOCONN_START_TIME*/, 0);
		}
		break;

	case BTSRV_LINK_BASE_CONNECTED_FAILED:
        /* let timeout continue process */
        if (!p_connect->reconnect_br_connect)
            break;

		p_connect->auto_conn[index].first_reconnect = 0;

        if (index == p_connect->connecting_index)
        {
	        struct auto_conn_t* p = &p_connect->auto_conn[index];

            /* reconnect phone device fail by some reason, unnormal page timeout?
             */
	        if (wait_reconn_dev_cnts < 2 && 
	            p->tws_mode == BTSRV_TWS_RECONNECT_NONE &&
	            p->state == AUTOCONN_STATE_BASE_CONNECTING &&
	            p->base_reconnect_times > 0 &&
	            os_uptime_get_32() - p->last_begin_time < p->reconnect_phone_timeout * 100 / 2)
	        {
                btsrv_adapter_check_cancal_connect(&p->addr);

                /* retry to connect ACL link later, not wait reconnect interval
                 */
                p->base_reconnect_times -= 1;
                p->state = AUTOCONN_STATE_PHONE_CONNECTING;
                
                p_connect->reconnect_br_connect = false;
                btsrv_scan_update_mode(true);

                btsrv_connect_auto_connection_restart(1000, 0);
                break;
	        }
        }
        /* let timeout continue process */
    
	case BTSRV_LINK_BASE_CONNECTED_TIMEOUT:
	    p_connect->auto_conn[index].state = AUTOCONN_STATE_IDLE;
	    p_connect->auto_conn[index].first_reconnect = 0;
	    
        if (p_connect->auto_conn[index].tws_mode == BTSRV_TWS_RECONNECT_NONE)
        {
            btsrv_adapter_check_cancal_connect(&p_connect->auto_conn[index].addr);
        }
        else if (p_connect->reconnect_br_connect)
        {
            btsrv_tws_cancal_auto_connect();
        }
        else if (btif_tws_is_in_pair_seraching())
        {
            btif_tws_end_pair();
        }

		if (index == p_connect->connecting_index)
	    {
	        struct auto_conn_t* p = &p_connect->auto_conn[index];

			/* reconnect times out?
             */
			if (p->base_try != 0 &&
			    p->base_reconnect_times >= p->base_try)
			{
			    SYS_LOG_INF("%d, TIMES_OUT %d, %d", index, p->base_try, os_uptime_get_32());
				p->state = AUTOCONN_STATE_END;
			}

            /* switch reconnect device quickly */
			btsrv_connect_auto_connection_restart(1 /*AUTOCONN_START_TIME*/, 0);
		}
		
        p_connect->reconnect_br_connect = false;
        btsrv_scan_update_mode(true);
		break;

	case BTSRV_LINK_HFP_CONNECTED:
	case BTSRV_LINK_A2DP_CONNECTED:
	case BTSRV_LINK_AVRCP_CONNECTED:
	case BTSRV_LINK_HID_CONNECTED:
		if (p_connect->auto_conn[index].state == AUTOCONN_STATE_PROFILE_CONNETING &&
			index == p_connect->connecting_index) {

			if (p_connect->auto_conn[index].profile_reconnect_times > 0)
			    p_connect->auto_conn[index].profile_reconnect_times -= 1;

			/* reconnect next profile quickly */
            if(p_connect->auto_conn[index].remote_connect_req){
                p_connect->auto_conn[index].profile_connect_wait = 1;
            }
            btsrv_connect_auto_connection_restart(1 /*AUTOCONN_START_TIME*/, 0);
		}
        else{
            p_connect->auto_conn[index].first_reconnect = 0;
        }
		break;

	case BTSRV_LINK_HFP_DISCONNECTED:
	case BTSRV_LINK_A2DP_DISCONNECTED:
	case BTSRV_LINK_AVRCP_DISCONNECTED:
	case BTSRV_LINK_HID_DISCONNECTED:
		if (p_connect->auto_conn[index].state == AUTOCONN_STATE_PROFILE_CONNETING &&
			index == p_connect->connecting_index)
	    {
			/* reconnect next profile quickly */
			btsrv_connect_auto_connection_restart(1 /*AUTOCONN_START_TIME*/, 0);
		}
		break;
	default:
		break;
	}
}

static void btsrv_autoconn_check_clear_auto_info(uint8_t clear_type)
{
	uint8_t i;

	for (i = 0; i < BTSRV_SAVE_AUTOCONN_NUM; i++) {
		if (p_connect->auto_conn[i].addr_valid) {
			if ((clear_type == AUTOCONN_RECONNECT_CLEAR_PHONE) &&
				(p_connect->auto_conn[i].tws_role != BTSRV_TWS_NONE)) {
				continue;
			} else if ((clear_type == AUTOCONN_RECONNECT_CLEAR_TWS) &&
				(p_connect->auto_conn[i].tws_role == BTSRV_TWS_NONE)) {
				continue;
			}

			if (btsrv_rdm_find_conn_by_addr(&p_connect->auto_conn[i].addr) == NULL) {
				memset(&p_connect->auto_conn[i], 0, sizeof(struct auto_conn_t));
			}
		}
	}
}

static void btsrv_autoconn_idle_proc(void)
{
	struct auto_conn_t *last_conn = p_connect->curr_conn;
	struct auto_conn_t *auto_conn;
	int32_t next_time = 0;
	uint8_t index;

	if ((btsrv_rdm_get_dev_role() == BTSRV_TWS_SLAVE) ||
		(btsrv_rdm_get_connected_dev(NULL, NULL) == btsrv_max_conn_num())) {
		btsrv_autoconn_check_clear_auto_info(AUTOCONN_RECONNECT_CLEAR_ALL);
	}

	/* if (btsrv_rdm_get_dev_role() == BTSRV_TWS_MASTER) {
		btsrv_autoconn_check_clear_auto_info(AUTOCONN_RECONNECT_CLEAR_TWS);
	} */

    /* if TWS ACL connected, but role not confirmed yet, it can't treat as phone device */
    #if 0
	if ((btsrv_rdm_get_dev_role() == BTSRV_TWS_NONE) &&
		(btsrv_rdm_get_connected_dev(NULL, NULL) == btsrv_max_phone_num())) {
		btsrv_autoconn_check_clear_auto_info(AUTOCONN_RECONNECT_CLEAR_PHONE);
	}
	#endif

	auto_conn = btsrv_autoconn_find_wait_reconn();
	if (auto_conn == NULL) {
		SYS_LOG_INF("auto connect finished");
		btsrv_connect_auto_connection_stop(BTSRV_STOP_AUTO_RECONNECT_ALL);
		btsrv_event_notify(MSG_BTSRV_BASE, MSG_BTSRV_AUTO_RECONNECT_COMPLETE, NULL);
		return;
	}

    p_connect->curr_conn = auto_conn;
	index = p_connect->connecting_index;

	if(btsrv_tws_is_in_pair_searching()){
		btif_tws_end_pair();
		btsrv_scan_update_mode(true);
	}

    if (auto_conn->last_begin_time == 0 ||
        os_uptime_get_32() - auto_conn->last_begin_time >= auto_conn->base_interval * 100)
    {
        if (auto_conn->tws_mode != BTSRV_TWS_RECONNECT_NONE)
        {
            auto_conn->state = AUTOCONN_STATE_TWS_CONNECTING;
        }
        else
        {
            auto_conn->state = AUTOCONN_STATE_PHONE_CONNECTING;
        }
    }

	/* switch reconnect device quickly */
	next_time = (p_connect->curr_conn != last_conn || auto_conn->state != AUTOCONN_STATE_IDLE) ? 
	    1 : AUTOCONN_WAIT_IDLE;
	
	btsrv_connect_auto_connection_restart(next_time, 0);
}

static void btsrv_autoconn_phone_connecting_proc(void)
{
	struct auto_conn_t *auto_conn = &p_connect->auto_conn[p_connect->connecting_index];
	int32_t next_time = 0;
	uint8_t index = p_connect->connecting_index;
	char addr[BT_ADDR_STR_LEN];

	hostif_bt_addr_to_str((const bt_addr_t *)&auto_conn->addr, addr, BT_ADDR_STR_LEN);

    auto_conn->base_reconnect_times += 1;
    auto_conn->last_begin_time = os_uptime_get_32();

	if(btsrv_rdm_find_conn_by_addr(&auto_conn->addr) == NULL){
	   if(p_connect->reconnect_br_connect == false){
		   p_connect->auto_conn[index].state = AUTOCONN_STATE_BASE_CONNECTING;
		   p_connect->reconnect_br_connect = true;
		   btsrv_scan_update_mode(true);
		   
		   uint32_t page_timeout = auto_conn->reconnect_phone_timeout * 100;

		   if(btsrv_adapter_callback(BTSRV_CHECK_STREAMING_AUDIO_STATUS, NULL) >= 0)
		   {
			SYS_LOG_INF("page_timeout %d", page_timeout);
			hostif_bt_br_set_page_timeout(page_timeout);
			btsrv_adapter_connect(&auto_conn->addr);
		   }

		   next_time = auto_conn->reconnect_phone_timeout * 100 + 1000;
	   }
	   else{
		   next_time = AUTOCONN_WAIT_IDLE;
	   }
	}
	else {
		if (p_connect->auto_conn[index].state < AUTOCONN_STATE_BASE_CONNECTED) {
			p_connect->auto_conn[index].state = AUTOCONN_STATE_BASE_CONNECTED;
		}
		btsrv_scan_update_mode(true);
		next_time = AUTOCONN_START_TIME;
	}

	SYS_LOG_INF("%s br:%d st:%d try:%d",
        addr,
		p_connect->reconnect_br_connect,
		auto_conn->state,
		auto_conn->base_reconnect_times);

	btsrv_connect_auto_connection_restart(next_time, 0);
}

static void btsrv_autoconn_tws_connecting_proc(void)
{
	struct auto_conn_t *auto_conn = &p_connect->auto_conn[p_connect->connecting_index];
	int32_t next_time = 0;

	if (!auto_conn->addr_valid) {
		auto_conn->state = AUTOCONN_STATE_IDLE;
		btsrv_connect_auto_connection_restart(AUTOCONN_START_TIME, 0);
		return;
	}

    auto_conn->base_reconnect_times += 1;
    auto_conn->last_begin_time = os_uptime_get_32();

	if (btsrv_rdm_find_conn_by_addr(&auto_conn->addr) == NULL) {
		auto_conn->state = AUTOCONN_STATE_BASE_CONNECTING;
	#ifdef CONFIG_SUPPORT_TWS
		/* Base connect do reconnect, no need tws module try more times, just set try_times to 0 */
//		btsrv_tws_connect_to((bt_addr_t *)&auto_conn->addr, 0, auto_conn->tws_role);
        if (auto_conn->tws_mode == BTSRV_TWS_RECONNECT_AUTO_PAIR)
        {
            btsrv_tws_set_pair_addr_match(false,NULL);
            btsrv_tws_set_pair_keyword(TWS_KEYWORD_POWER_ON_AUTO_PAIR);
        }
		else if (btsrv_is_tws_reconnect_search_mode())
		{
            btsrv_tws_set_pair_addr_match(false,NULL);
            btsrv_tws_set_pair_keyword(0);
		}
		else
		{
            btsrv_tws_set_pair_addr_match(true,&auto_conn->addr);
            btsrv_tws_set_pair_keyword(0);
		}
		
        btsrv_scan_update_mode(true);
        
        // btsrv_event_notify(MSG_BTSRV_TWS,MSG_BTSRV_TWS_START_PAIR,(void *)TWS_CONNECT_TRY_TIMES);
        btif_tws_start_pair(TWS_CONNECT_TRY_TIMES);
        
        next_time = auto_conn->reconnect_tws_timeout * 100;
	#endif
//		next_time = auto_conn->base_interval;
	} else {
		SYS_LOG_INF("connecting_proc state %d", auto_conn->state);
		if (auto_conn->state < AUTOCONN_STATE_BASE_CONNECTED) {
			auto_conn->state = AUTOCONN_STATE_BASE_CONNECTED;
		}
		btsrv_scan_update_mode(true);
		next_time = AUTOCONN_START_TIME;
	}

	SYS_LOG_INF("TWS:state:%d try:%d", auto_conn->state,auto_conn->base_reconnect_times);

	btsrv_connect_auto_connection_restart(next_time, 0);
}

static void btsrv_autoconn_tws_role_confirm_proc(void)
{
	/* TWS connect timeout */
	btsrv_proc_link_change(p_connect->auto_conn[p_connect->connecting_index].addr.val,
								BTSRV_LINK_TWS_CONNECTED_TIMEOUT);
    
    if (!thread_timer_is_running(&p_connect->auto_conn_timer))
    {
        int i = p_connect->connecting_index;
        p_connect->auto_conn[i].state = AUTOCONN_STATE_IDLE;
        
        btsrv_connect_auto_connection_restart(AUTOCONN_START_TIME, 0);
    }
}

static void btsrv_autoconn_base_connecting_proc(void)
{
	/* Base connect timeout */
	btsrv_proc_link_change(p_connect->auto_conn[p_connect->connecting_index].addr.val,
								BTSRV_LINK_BASE_CONNECTED_TIMEOUT);
    
    if (!thread_timer_is_running(&p_connect->auto_conn_timer))
    {
        int i = p_connect->connecting_index;
        p_connect->auto_conn[i].state = AUTOCONN_STATE_IDLE;
        
        btsrv_connect_auto_connection_restart(AUTOCONN_START_TIME, 0);
    }
}

static void btsrv_autoconn_base_connected_proc(void)
{
    struct auto_conn_t* p = &p_connect->auto_conn[p_connect->connecting_index];

    if (p->tws_mode != BTSRV_TWS_RECONNECT_NONE)
    {
        if (os_uptime_get_32() - p->last_begin_time >= AUTOCONN_TWS_WAIT_ROLE)
        {
            btsrv_proc_link_change(p->addr.val, BTSRV_LINK_TWS_CONNECTED_TIMEOUT);
            return;
        }
    }
    else
    {
        if (os_uptime_get_32() - p->last_begin_time >= p->profile_interval * 100)
        {
            p->state = AUTOCONN_STATE_PROFILE_CONNETING;
            p->curr_connect_profile = AUTOCONN_PROFILE_IDLE;
            p->profile_reconnect_times = 0;
        }
    }

    /* wait BTSRV_LINK_BASE_GET_NAME or BTSRV_LINK_BASE_DISCONNECTED */
    btsrv_connect_auto_connection_restart(AUTOCONN_START_TIME, 0);
}

static int btsrv_autoconn_check_connect_profile(struct bt_conn *conn, struct auto_conn_t *auto_conn)
{
	int ret, i;
	uint8_t next_connect_profile;
	bool hfp_connected = btsrv_rdm_is_hfp_connected(conn);
	bool a2dp_connected = btsrv_rdm_is_a2dp_connected(conn);
	bool avrcp_connected = btsrv_rdm_is_avrcp_connected(conn);
	bool hid_connected = btsrv_rdm_is_hid_connected(conn);

	if ((auto_conn->hfp && !hfp_connected) ||
		(auto_conn->a2dp && !a2dp_connected) ||
		(auto_conn->avrcp && !avrcp_connected)||
		(auto_conn->hid && !hid_connected)) {
		/* Still have profile to connect */
		ret = 0;
	} else {
		/* Try other device */
		ret = 1;
		goto exit_check;
	}

	if (auto_conn->tws_role != BTSRV_TWS_NONE) {
		/* TWS a2dp/avrcp connect by master in base connected callback,
		 * here just check a2dp/avrcp is connected and try other device.
		 */
		goto exit_check;
	}

	SYS_LOG_INF("connect_profile %d%d, %d%d, %d%d, %d%d %d%d", auto_conn->curr_connect_profile, auto_conn->hfp_first,
			auto_conn->hfp, hfp_connected, auto_conn->a2dp, a2dp_connected, auto_conn->avrcp, avrcp_connected
			, auto_conn->hid, hid_connected);

	if (auto_conn->curr_connect_profile == AUTOCONN_PROFILE_IDLE) {
		if (auto_conn->hfp_first) {
			next_connect_profile = AUTOCONN_PROFILE_HFP_CONNECTING;
		} else {
			next_connect_profile = AUTOCONN_PROFILE_A2DP_CONNECTING;
		}
	} else {
		next_connect_profile = auto_conn->curr_connect_profile + 1;
	}

	for (i = (AUTOCONN_PROFILE_IDLE+1); i < AUTOCONN_PROFILE_CONNECTING_MAX; i++) {
		if (next_connect_profile >= AUTOCONN_PROFILE_CONNECTING_MAX) {
			next_connect_profile = AUTOCONN_PROFILE_HFP_CONNECTING;
		}

		if ((next_connect_profile == AUTOCONN_PROFILE_HFP_CONNECTING) &&
			auto_conn->hfp && !hfp_connected) {
			break;
		} else if ((next_connect_profile == AUTOCONN_PROFILE_A2DP_CONNECTING) &&
					auto_conn->a2dp && !a2dp_connected) {
			break;
		} else if ((next_connect_profile == AUTOCONN_PROFILE_AVRCP_CONNECTING) &&
					(a2dp_connected) && (auto_conn->avrcp && !avrcp_connected)) {
			break;
		}else if ((next_connect_profile == AUTOCONN_PROFILE_HID_CONNECTING) &&
					auto_conn->hid && !hid_connected) {
			break;
		}

		next_connect_profile++;
	}

    auto_conn->curr_connect_profile = next_connect_profile;
	if (next_connect_profile == AUTOCONN_PROFILE_HFP_CONNECTING) {
		btsrv_hfp_connect(conn);
	} else if (next_connect_profile == AUTOCONN_PROFILE_A2DP_CONNECTING) {
		btsrv_a2dp_connect(conn, BT_A2DP_CH_SINK);
	} else if (next_connect_profile == AUTOCONN_PROFILE_AVRCP_CONNECTING) {
		btsrv_avrcp_connect(conn);
	}else if (next_connect_profile == AUTOCONN_PROFILE_HID_CONNECTING) {
		btsrv_hid_connect(conn);
	}

	auto_conn->curr_connect_profile = next_connect_profile;
exit_check:
	return ret;
}

static void btsrv_autoconn_profile_connecting_proc(void)
{
	uint8_t index = p_connect->connecting_index;
	struct auto_conn_t *auto_conn = &p_connect->auto_conn[index];
	struct bt_conn *conn;
    uint16_t profile_interval;
	conn = btsrv_rdm_find_conn_by_addr(&auto_conn->addr);
	profile_interval = auto_conn->profile_interval * 100;

	if (conn == NULL) {
		SYS_LOG_ERR("connecting_proc need to fix!!!");
		goto try_other_dev;
	}
	auto_conn->profile_reconnect_times += 1;

	if (auto_conn->profile_reconnect_times > auto_conn->profile_try) {
		SYS_LOG_WRN("TIMES_OUT %d(%d), %d(%d), %d(%d), %d(%d)",
					auto_conn->hfp, btsrv_rdm_is_hfp_connected(conn),
					auto_conn->a2dp, btsrv_rdm_is_a2dp_connected(conn),
					auto_conn->avrcp, btsrv_rdm_is_avrcp_connected(conn),
					auto_conn->hid, btsrv_rdm_is_hid_connected(conn));
		/* I have linkkey but phone clear linkkey, when do reconnect,
		 * phone will arise one connect notify, but phone do nothing,
		 * then profile will not connect, need active do disconnect.
		 * Better TODO: host check this case, notify upper layer
		 *                          clear linkkey and auto connect info.
		 */
		if (!btsrv_rdm_is_hfp_connected(conn) &&
			!btsrv_rdm_is_a2dp_connected(conn) &&
			!btsrv_rdm_is_avrcp_connected(conn) &&
			!btsrv_rdm_is_spp_connected(conn)  &&
			!btsrv_rdm_is_hid_connected(conn)) {
			/* if only ACL connected, no other profiles connected, will force disconnect */
			btsrv_adapter_disconnect(conn);

            /* continue retry reconnect after ACL force disconnected */
            btsrv_connect_auto_connection_restart(profile_interval, 0);
            return;
		}
		goto try_other_dev;
	}

    if(auto_conn->profile_connect_wait){
        auto_conn->profile_connect_wait = 0;
        btsrv_connect_auto_connection_restart(profile_interval, 0);
        return;
    }

	if (btsrv_autoconn_check_connect_profile(conn, auto_conn)) {
	    SYS_LOG_INF("%d, COMPLETE", index);
		goto try_other_dev;
	}

	btsrv_connect_auto_connection_restart(profile_interval, 0);
	return;

try_other_dev:
    /* phone device reconnect complete */
    auto_conn->state = AUTOCONN_STATE_END;
    
    /* switch reconnect device quickly */
    if(auto_conn->remote_connect_req && auto_conn->profile_connect_wait){
        auto_conn->profile_connect_wait = 0;
        btsrv_connect_auto_connection_restart(profile_interval ,0);
    }
    else{
        btsrv_connect_auto_connection_restart(1 /*AUTOCONN_START_TIME*/, 0);
    }
}

static void btsrv_autoconn_timer_handler(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	uint8_t state, index;

	p_connect->auto_connect_running = 1;
	index = p_connect->connecting_index;
	if (p_connect->auto_conn[index].addr_valid) {
		state = p_connect->auto_conn[index].state;
	} else {
		state = AUTOCONN_STATE_IDLE;
	}
	SYS_LOG_INF("autoconn index %d, state: %d", index, state);

	switch (state) {
	case AUTOCONN_STATE_IDLE:
	case AUTOCONN_STATE_END:
		btsrv_autoconn_idle_proc();
		break;
	case AUTOCONN_STATE_TWS_CONNECTING:
		btsrv_autoconn_tws_connecting_proc();
		break;
    case AUTOCONN_STATE_TWS_ROLE_CONFIRM:
		btsrv_autoconn_tws_role_confirm_proc();
		break;
    case AUTOCONN_STATE_PHONE_CONNECTING:
        btsrv_autoconn_phone_connecting_proc();
		break;
	case AUTOCONN_STATE_BASE_CONNECTING:
		btsrv_autoconn_base_connecting_proc();
		break;
	case AUTOCONN_STATE_BASE_CONNECTED:
		btsrv_autoconn_base_connected_proc();
		break;
	case AUTOCONN_STATE_PROFILE_CONNETING:
		btsrv_autoconn_profile_connecting_proc();
		break;
	default:
		btsrv_connect_auto_connection_stop(BTSRV_STOP_AUTO_RECONNECT_ALL);
		break;
	}

	p_connect->auto_connect_running = 0;
}

static void btsrv_monitor_timer_handler(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	int i, stop_timer = 1;
	struct bt_conn *conn;

	p_connect->monitor_timer_running = 1;

	for (i = 0; i < BTSRV_SAVE_AUTOCONN_NUM; i++) {
		if (p_connect->monitor_conn[i].valid) {
			conn = btsrv_rdm_find_conn_by_addr(&p_connect->monitor_conn[i].addr);
			if (conn == NULL) {
				p_connect->monitor_conn[i].valid = 0;
				continue;
			}

			/* Now only monitor avrcp after a2dp connected,
			 * some phone active connect a2dp but not connect avrcp
			 */
			if (btsrv_rdm_is_a2dp_connected(conn)) {
				if (btsrv_rdm_is_avrcp_connected(conn)) {
					p_connect->monitor_conn[i].valid = 0;
					continue;
				} else {
					if (p_connect->monitor_conn[i].avrcp_times) {
						p_connect->monitor_conn[i].avrcp_times--;
					}

					if ((p_connect->monitor_conn[i].avrcp_times%MONITOR_AVRCP_CONNECT_INTERVAL) == 0) {
						SYS_LOG_INF("Do avrcp connect");
						btsrv_avrcp_connect(conn);
					}

					if (p_connect->monitor_conn[i].avrcp_times == 0) {
						p_connect->monitor_conn[i].valid = 0;
						continue;
					}
				}
			}
			else {
				/* Just find hongmi2A not do a2dp connect when connect from phone.
				 * if need add this active connect ???
				 */
                if(!btsrv_rdm_is_a2dp_signal_connected(conn)){
                    if (p_connect->monitor_conn[i].a2dp_times) {
                        p_connect->monitor_conn[i].a2dp_times--;
                        if (p_connect->monitor_conn[i].a2dp_times == 0) {
                            SYS_LOG_INF("Phone not do a2dp connect");
                            btsrv_a2dp_connect(conn, BT_A2DP_CH_SINK);
                        }
                    }
                }
			}

			stop_timer = 0;
		}
	}

	p_connect->monitor_timer_running = 0;
	if (stop_timer) {
		btsrv_connect_monitor_profile_stop();
	}
}

static void btsrv_add_monitor(struct bt_conn *conn)
{
	int i, index = BTSRV_SAVE_AUTOCONN_NUM;
	bd_address_t *addr = (bd_address_t *)GET_CONN_BT_ADDR(conn);

	if (btsrv_is_pts_test()) {
		return;
	}

	for (i = 0; i < BTSRV_SAVE_AUTOCONN_NUM; i++) {
		if (p_connect->auto_conn[i].addr_valid &&
			!memcmp(p_connect->auto_conn[i].addr.val, addr->val, sizeof(bd_address_t))) {
			/* Reconnect device, not need monitor profile connect */
			return;
		}
	}

	for (i = 0; i < BTSRV_SAVE_AUTOCONN_NUM; i++) {
		if (!p_connect->monitor_conn[i].valid && index == BTSRV_SAVE_AUTOCONN_NUM) {
			index = i;
		}

		if (p_connect->monitor_conn[i].valid &&
			!memcmp(p_connect->monitor_conn[i].addr.val, addr->val, sizeof(bd_address_t))) {
			/* Already in monitor */
			if (btsrv_rdm_is_security_changed(conn))
				break;
			
			return;
		}
	}

	if (index == BTSRV_SAVE_AUTOCONN_NUM) {
		return;
	}

	SYS_LOG_INF("Add profile monitor");

	memcpy(p_connect->monitor_conn[index].addr.val, addr->val, sizeof(bd_address_t));
	p_connect->monitor_conn[index].valid = 1;

	if (!btsrv_rdm_is_security_changed(conn)){
		p_connect->monitor_conn[index].a2dp_times = MONITOR_A2DP_NOSECURITY_TIMES;
	}else{
		p_connect->monitor_conn[index].a2dp_times = MONITOR_A2DP_TIMES;
	}
    if(btsrv_rdm_is_ios_dev(conn)){
        p_connect->monitor_conn[index].avrcp_times = MONITOR_AVRCP_TIMES;
    }
    else{
        p_connect->monitor_conn[index].avrcp_times = MONITOR_AVRCP_CONNECT_INTERVAL + 6;
    }

	btsrv_connect_monitor_profile_start(MONITOR_PRIFILE_INTERVEL, MONITOR_PRIFILE_INTERVEL);
}

static void btsrv_update_monitor(uint8_t *addr, uint8_t type)
{
	int i, index = BTSRV_SAVE_AUTOCONN_NUM;
	bd_address_t bd_addr;
	struct bt_conn *conn;

	for (i = 0; i < BTSRV_SAVE_AUTOCONN_NUM; i++) {
		if (p_connect->monitor_conn[i].valid &&
			!memcmp(p_connect->monitor_conn[i].addr.val, addr, sizeof(bd_address_t))) {
			index = i;
			break;
		}
	}

	if (index == BTSRV_SAVE_AUTOCONN_NUM) {
		return;
	}

	/* Current only monitor avrcp */
	switch (type) {
	case BTSRV_LINK_AVRCP_CONNECTED:
	case BTSRV_LINK_BASE_DISCONNECTED:
		p_connect->monitor_conn[index].valid = 0;
		break;
	case BTSRV_LINK_A2DP_CONNECTED:
	case BTSRV_LINK_A2DP_SINGAL_CONNECTED:
		p_connect->monitor_conn[index].a2dp_times = MONITOR_A2DP_TIMES;
	    break;
    case BTSRV_LINK_HFP_DISCONNECTED:
    case BTSRV_LINK_A2DP_DISCONNECTED:
    case BTSRV_LINK_AVRCP_DISCONNECTED:
    case BTSRV_LINK_SPP_DISCONNECTED:
    case BTSRV_LINK_HID_DISCONNECTED:
        memcpy(bd_addr.val,addr,6);
        conn = btsrv_rdm_find_conn_by_addr(&bd_addr);
        if (conn && !btsrv_rdm_is_profile_connected(conn)){
            p_connect->monitor_conn[index].valid = 0;
        }
        break;
	}
}

bool btsrv_connect_is_pending(void)
{
    uint32_t current_time;
    if(!p_connect){
        return false;
    }

    if(p_connect->connect_is_pending){
        current_time = os_uptime_get_32();
        if((current_time  - p_connect->connect_pending_time) >= BT_CONNECT_PENDING_TIMEOUT){
            p_connect->connect_is_pending = 0;
        }
    }
    return p_connect->connect_is_pending;
}

void btsrv_proc_link_change(uint8_t *mac, uint8_t type)
{
	uint8_t need_update = 0;
	bool    scan_update = false;
	bool    is_pending  = false;

	SYS_LOG_INF("ev:%s %x%x%x%x%x%x",
		btsrv_link_evt2str(type),
		mac[5],mac[4],mac[3],
		mac[2],mac[1],mac[0]);

    if (type == BTSRV_LINK_BASE_CONNECTED ||
        type == BTSRV_LINK_BASE_GET_NAME)
    {
        is_pending = true;
    }
    
    if (p_connect->connect_is_pending != is_pending)
    {
        p_connect->connect_is_pending = is_pending;
        if(is_pending){
            p_connect->connect_pending_time = os_uptime_get_32();
        }
        scan_update = true;
        btsrv_scan_update_mode(false);
    }
    
	switch (type) {
	case BTSRV_LINK_BASE_GET_NAME:
	case BTSRV_LINK_BASE_CONNECTED_TIMEOUT:
	case BTSRV_LINK_TWS_CONNECTED:
	case BTSRV_LINK_FORCE_STOP:
		btsrv_update_autoconn_state(mac, type);
		break;

	case BTSRV_LINK_BASE_CONNECTED:
		btsrv_update_autoconn_state(mac, type);
		btsrv_scan_update_mode(true);
		break;
    case BTSRV_LINK_TWS_CONNECTED_TIMEOUT:
        btif_tws_disconnect(NULL);
		btsrv_update_autoconn_state(mac, type);
		break;

	case BTSRV_LINK_BASE_CONNECTED_FAILED:
	case BTSRV_LINK_BASE_DISCONNECTED:
	case BTSRV_LINK_HFP_DISCONNECTED:
	case BTSRV_LINK_A2DP_DISCONNECTED:
	case BTSRV_LINK_AVRCP_DISCONNECTED:
	case BTSRV_LINK_HID_DISCONNECTED:
		btsrv_rdm_remove_dev(mac);
		btsrv_update_autoconn_state(mac, type);
		btsrv_update_monitor(mac, type);
		btsrv_scan_update_mode(false);
		if ((type == BTSRV_LINK_HFP_DISCONNECTED) ||
			(type == BTSRV_LINK_A2DP_DISCONNECTED)||
			(type == BTSRV_LINK_HID_DISCONNECTED)) {
			need_update = 1;
		}
		break;

	case BTSRV_LINK_HFP_CONNECTED:
	case BTSRV_LINK_A2DP_SINGAL_CONNECTED:
	case BTSRV_LINK_A2DP_CONNECTED:
	case BTSRV_LINK_AVRCP_CONNECTED:
	case BTSRV_LINK_HID_CONNECTED:
		btsrv_update_autoconn_state(mac, type);
		btsrv_update_monitor(mac, type);
        if(scan_update) {
            btsrv_scan_update_mode(true);
        }
		need_update = 1;
		break;

	case BTSRV_LINK_SPP_DISCONNECTED:
	case BTSRV_LINK_PBAP_DISCONNECTED:
		btsrv_rdm_remove_dev(mac);
		btsrv_update_monitor(mac, type);
		btsrv_scan_update_mode(false);
		break;

	case BTSRV_LINK_SPP_CONNECTED:
	case BTSRV_LINK_PBAP_CONNECTED:
	default:
		break;
	}

	if (need_update) {
		btsrv_autoconn_info_update();
	}

	btsrv_update_performance_req();
}

extern struct conn_req_info * btsrv_get_connreq_info(bt_addr_t *addr);

void btsrv_notify_link_event(struct bt_conn *base_conn, uint8_t event, uint8_t param)
{
	struct bt_link_cb_param cb_param;
	struct conn_req_info* connreq_info;

	memset(&cb_param, 0, sizeof(struct bt_link_cb_param));
	cb_param.link_event = event;
	cb_param.hdl = hostif_bt_conn_get_handle(base_conn);
	cb_param.addr = (bd_address_t *)GET_CONN_BT_ADDR(base_conn);
	cb_param.param = param;

	switch (event) {
	case BT_LINK_EV_GET_NAME:
		cb_param.name = btsrv_rdm_get_dev_name(base_conn);
		cb_param.is_tws = param ? 1 : 0;
		break;
	case BT_LINK_EV_ACL_CONNECTED:
		connreq_info = btsrv_get_connreq_info((bt_addr_t *)GET_CONN_BT_ADDR(base_conn));
		cb_param.param = connreq_info ? 1: 0;
		cb_param.key_miss = connreq_info ? connreq_info->key_miss : 0;
		break;
	default:
		break;
	}

	btsrv_sniff_update_idle_time(base_conn);
	btsrv_adapter_callback(BTSRV_LINK_EVENT, &cb_param);
}

void btsrv_pnp_sdp_completed_cb(struct bt_conn *base_conn,uint16_t vendor_id,uint16_t product_id)
{
    if(!base_conn){
        return;
    }

    btsrv_rdm_set_dev_pnp_info(base_conn,vendor_id,product_id);
}

static int btsrv_connect_connected(struct bt_conn *base_conn)
{
	int ret = 0;

	bd_address_t *addr = (bd_address_t *)GET_CONN_BT_ADDR(base_conn);
	struct conn_req_info* connreq_info;

	ret = btsrv_rdm_add_dev(base_conn);
	if (ret < 0) {
		SYS_LOG_ERR("Should not run to here!!!!");
		return ret;
	}

    /* stop TWS pair search state when ACL connected */
    #if 0 //这里不执行停止动作,避免上层bt_manager_tws_end_pair_search不能执行
	if (btif_tws_is_in_pair_seraching()) {
        btif_tws_end_pair();
    }
	#endif

	btsrv_rdm_update_autoreconn_info(base_conn);

    connreq_info = btsrv_get_connreq_info((bt_addr_t *)GET_CONN_BT_ADDR(base_conn));
    if(connreq_info){
        btsrv_rdm_set_dev_class(base_conn,connreq_info->dev_class);
    }

	btsrv_proc_link_change(addr->val, BTSRV_LINK_BASE_CONNECTED);
	btsrv_notify_link_event(base_conn, BT_LINK_EV_ACL_CONNECTED, 0);

    if(connreq_info){
        hostif_bt_sdp_pnp(base_conn,btsrv_pnp_sdp_completed_cb);
    }

    btsrv_adapter_start_monitor_link_quality_timer();

	return ret;
}

static void btsrv_connect_security_changed(struct bt_conn *base_conn)
{
	if (btsrv_rdm_get_tws_role(base_conn) == BTSRV_TWS_NONE &&
		!btsrv_rdm_is_security_changed(base_conn)) {
		btsrv_rdm_set_security_changed(base_conn);
		if(p_connect && !p_connect->gfp_running){
		    btsrv_add_monitor(base_conn);
		}
		btsrv_event_notify(MSG_BTSRV_TWS, MSG_BTSRV_SECURITY_CHANGED, base_conn);
		btsrv_notify_link_event(base_conn, BT_LINK_EV_SECURITY_CHANGED, 0);
	}
}


static void btsrv_connect_req_changed(void *info)
{
	struct bt_conn *base_conn;
	struct btsrv_addr_name *mac_addr_info = (struct btsrv_addr_name *)info;

	if(!mac_addr_info){
		return;
	}
	
	base_conn = btsrv_rdm_find_conn_by_addr(&mac_addr_info->mac);
	if (base_conn == NULL) {
		SYS_LOG_ERR("Can't find conn for addr");
		return;
	}

    if(!p_connect){
        return;
    }

    SYS_LOG_INF("is gfp: %d",p_connect->gfp_running);

    if(p_connect->gfp_running){
        return;
    }

	if (btsrv_rdm_get_tws_role(base_conn) == BTSRV_TWS_NONE) {
		if(bt_store_check_linkkey((bt_addr_t *)&mac_addr_info->mac) == 0){
		    btsrv_add_monitor(base_conn);
		}
	}
}

static bool btsrv_autoconn_on_acl_disconnected(struct bt_conn *base_conn, uint8_t reason)
{
	int i;
	bd_address_t *addr = (bd_address_t *)GET_CONN_BT_ADDR(base_conn);
    bool ret = true;
	for (i = 0; i < BTSRV_SAVE_AUTOCONN_NUM; i++){
		if(p_connect->auto_conn[i].addr_valid){
			if (!memcmp(&p_connect->auto_conn[i].addr, addr, sizeof(bd_address_t)))
			{
			    p_connect->auto_conn[i].last_reason = reason;
			    
			    if ((reason == BT_HCI_ERR_REMOTE_USER_TERM_CONN) ||
			        (reason == BT_HCI_ERR_CONN_ALREADY_EXISTS)   ||
			        (reason == BT_HCI_ERR_LOCALHOST_TERM_CONN)   ||
			        (reason == BT_HCI_ERR_REMOTE_LOW_RESOURCES))
			    {
                    p_connect->auto_conn[i].reason_times++;
                    if(p_connect->auto_conn[i].reason_times >= 3){
                        ret = true;
                    }
                    else{
    					ret = false;
                    }
                } else {
                    p_connect->auto_conn[i].reason_times = 0;
                }
            }
		}
	}

    return ret;
}

static int btsrv_connect_disconnected(struct bt_conn *base_conn, uint8_t reason)
{
	int role;
    uint8_t type;
	struct bt_disconnect_reason bt_disparam;
	bd_address_t *addr = (bd_address_t *)GET_CONN_BT_ADDR(base_conn);

	role = btsrv_rdm_get_tws_role(base_conn);
    type = hostif_bt_conn_get_type(base_conn);
	if (role == BTSRV_TWS_NONE) {
		/* Connected, but still not change to tws pending or tws role,
		 * receive disconnect, need notify tws exit connecting state.
		 */
		btsrv_event_notify_malloc(MSG_BTSRV_TWS, MSG_BTSRV_TWS_DISCONNECTED_ADDR, (uint8_t *)addr, sizeof(bd_address_t), 0);

		/* Phone disconnect need check and change active snoop link */
		btsrv_event_notify(MSG_BTSRV_TWS, MSG_BTSRV_TWS_PHONE_ACTIVE_CHANGE, NULL);
	} else if (role > BTSRV_TWS_NONE) {
		btsrv_event_notify_ext(MSG_BTSRV_TWS, MSG_BTSRV_TWS_DISCONNECTED, base_conn, reason);
	}

	memcpy(&bt_disparam.addr, addr, sizeof(bd_address_t));
	bt_disparam.reason = reason;
	bt_disparam.tws_role = role;
    bt_disparam.conn_type = type;

	if(btsrv_autoconn_on_acl_disconnected(base_conn,reason)){
	    btsrv_event_notify_malloc(MSG_BTSRV_BASE, MSG_BTSRV_DISCONNECTED_REASON, (uint8_t *)&bt_disparam, sizeof(bt_disparam), 0);
	}

	btsrv_notify_link_event(base_conn, BT_LINK_EV_ACL_DISCONNECTED, reason);
	btsrv_rdm_base_disconnected(base_conn);
	btsrv_proc_link_change(addr->val, BTSRV_LINK_BASE_DISCONNECTED);

	if (p_connect->clear_list_disconnecting) {
		if (btsrv_rdm_get_connected_dev_cnt_by_type(p_connect->clear_list_mode) == 0) {
			p_connect->clear_list_disconnecting = 0;
			btsrv_event_notify(MSG_BTSRV_CONNECT, MGS_BTSRV_CLEAR_AUTO_INFO, NULL);
			SYS_LOG_INF("clear list finish %d",p_connect->clear_list_mode);
		}
	}

    if(btsrv_rdm_get_connected_dev(NULL, NULL) == 0){
        btsrv_adapter_stop_monitor_link_quality_timer();
    }
    return 0;
}

/* Be careful, it can work well when connect avrcp conflict with phone ??? */
static void btsrv_connect_quick_connect_avrcp(struct bt_conn *base_conn)
{
	int i;
	bd_address_t *addr = (bd_address_t *)GET_CONN_BT_ADDR(base_conn);

	if (btsrv_rdm_is_avrcp_connected(base_conn)) {
		return;
	}

	for (i = 0; i < BTSRV_SAVE_AUTOCONN_NUM; i++) {
		if (p_connect->auto_conn[i].addr_valid &&
			!memcmp(p_connect->auto_conn[i].addr.val, addr->val, sizeof(bd_address_t))) {
			/* Reconnect device, reconnect will process avrcp connect */
			return;
		}
	}

	SYS_LOG_INF("quick connect avrcp\n");
	btsrv_avrcp_connect(base_conn);
}

static int btsrv_connect_a2dp_connected(struct bt_conn *base_conn)
{
	bd_address_t *addr = (bd_address_t *)GET_CONN_BT_ADDR(base_conn);

	btsrv_rdm_set_a2dp_connected(base_conn, true);
	btsrv_notify_link_event(base_conn, BT_LINK_EV_A2DP_CONNECTED, 0);
	btsrv_proc_link_change(addr->val, BTSRV_LINK_A2DP_CONNECTED);

	/* Sync info after set a2dp connected */
	btsrv_rdm_set_a2dp_meida_rx_cid(base_conn, hostif_bt_a2dp_get_media_rx_cid(base_conn));
	btsrv_tws_sync_info_type(base_conn, BTSRV_SYNC_TYPE_A2DP, BTSRV_A2DP_NONE_EV);

    if(p_connect && p_connect->gfp_running){
        return 0;
    }

	/* Same phone not connect avrcp when close/open bluetooth media */
	if (btsrv_rdm_get_tws_role(base_conn) == BTSRV_TWS_NONE) {
		if(0) {
			btsrv_connect_quick_connect_avrcp(base_conn);
		}
		btsrv_add_monitor(base_conn);
	}
	return 0;
}

static int btsrv_connect_a2dp_singnaling_connected(struct bt_conn *base_conn)
{
    bd_address_t *addr = (bd_address_t *)GET_CONN_BT_ADDR(base_conn);

    btsrv_notify_link_event(base_conn, BT_LINK_EV_A2DP_SINGALING_CONNECTED, 0);
    btsrv_proc_link_change(addr->val, BTSRV_LINK_A2DP_SINGAL_CONNECTED);
    btsrv_rdm_set_a2dp_signal_connected(base_conn,true);

    return 0;
}

static int btsrv_connect_a2dp_disconnected(struct bt_conn *base_conn)
{
	bd_address_t *addr = (bd_address_t *)GET_CONN_BT_ADDR(base_conn);

    btsrv_rdm_set_a2dp_signal_connected(base_conn, false);
	btsrv_rdm_set_a2dp_connected(base_conn, false);

	/* Sync info before rdm_remove_dev */
	btsrv_rdm_set_a2dp_meida_rx_cid(base_conn, 0);
	btsrv_tws_sync_info_type(base_conn, BTSRV_SYNC_TYPE_A2DP, BTSRV_A2DP_NONE_EV);

	btsrv_notify_link_event(base_conn, BT_LINK_EV_A2DP_DISCONNECTED, 0);
	btsrv_proc_link_change(addr->val, BTSRV_LINK_A2DP_DISCONNECTED);

	return 0;
}

static int btsrv_connect_a2dp_channel_err(struct bt_conn *base_conn)
{
    btsrv_notify_link_event(base_conn, BT_LINK_EV_A2DP_CHANNEL_ERR, 0);
    return 0;
}

static int btsrv_connect_avrcp_connected(struct bt_conn *base_conn)
{
	bd_address_t *addr = (bd_address_t *)GET_CONN_BT_ADDR(base_conn);

	btsrv_rdm_set_avrcp_connected(base_conn, true);
	btsrv_notify_link_event(base_conn, BT_LINK_EV_AVRCP_CONNECTED, 0);
	btsrv_proc_link_change(addr->val, BTSRV_LINK_AVRCP_CONNECTED);

	/* Sync info after set avrcp connected */
	btsrv_tws_sync_info_type(base_conn, BTSRV_SYNC_TYPE_AVRCP, BTSRV_AVRCP_NONE_EV);
	return 0;
}

static int btsrv_connect_avrcp_disconnected(struct bt_conn *base_conn)
{
	bd_address_t *addr = (bd_address_t *)GET_CONN_BT_ADDR(base_conn);

	btsrv_rdm_set_avrcp_connected(base_conn, false);

	/* Sync info before rdm_remove_dev */
	btsrv_tws_sync_info_type(base_conn, BTSRV_SYNC_TYPE_AVRCP, BTSRV_AVRCP_NONE_EV);

	btsrv_notify_link_event(base_conn, BT_LINK_EV_AVRCP_DISCONNECTED, 0);
	btsrv_proc_link_change(addr->val, BTSRV_LINK_AVRCP_DISCONNECTED);

	return 0;
}

static int btsrv_connect_hfp_connected(struct bt_conn *base_conn, uint8_t cmd)
{
	bd_address_t *addr = (bd_address_t *)GET_CONN_BT_ADDR(base_conn);

	btsrv_rdm_set_hfp_connected(base_conn, true);
	btsrv_notify_link_event(base_conn, BT_LINK_EV_HF_CONNECTED, 0);
	btsrv_proc_link_change(addr->val, BTSRV_LINK_HFP_CONNECTED);

	/* Sync info after set hfp connected */
	if (cmd == MSG_BTSRV_HFP_CONNECTED) {
		btsrv_tws_sync_info_type(base_conn, BTSRV_SYNC_TYPE_HFP, BTSRV_HFP_NONE_EV);
	}
	return 0;
}

static int btsrv_connect_hfp_disconnected(struct bt_conn *base_conn, uint8_t cmd)
{
	bd_address_t *addr = (bd_address_t *)GET_CONN_BT_ADDR(base_conn);

	btsrv_rdm_set_hfp_connected(base_conn, false);
	btsrv_notify_link_event(base_conn, BT_LINK_EV_HF_DISCONNECTED, 0);

	/* Sync info before rdm_remove_dev */
	if (cmd == MSG_BTSRV_HFP_DISCONNECTED) {
		btsrv_tws_sync_info_type(base_conn, BTSRV_SYNC_TYPE_HFP, BTSRV_HFP_NONE_EV);
	}

	btsrv_proc_link_change(addr->val, BTSRV_LINK_HFP_DISCONNECTED);

	return 0;
}

static int btsrv_connect_spp_connected(struct bt_conn *base_conn)
{
	bd_address_t *addr = (bd_address_t *)GET_CONN_BT_ADDR(base_conn);

	btsrv_rdm_set_spp_connected(base_conn, true);
	btsrv_notify_link_event(base_conn, BT_LINK_EV_SPP_CONNECTED, 0);
	btsrv_proc_link_change(addr->val, BTSRV_LINK_SPP_CONNECTED);
	return 0;
}

static int btsrv_connect_spp_disconnected(struct bt_conn *base_conn)
{
	bd_address_t *addr = (bd_address_t *)GET_CONN_BT_ADDR(base_conn);

	btsrv_rdm_set_spp_connected(base_conn, false);
	btsrv_notify_link_event(base_conn, BT_LINK_EV_SPP_DISCONNECTED, 0);
	btsrv_proc_link_change(addr->val, BTSRV_LINK_SPP_DISCONNECTED);
	return 0;
}

static int btsrv_connect_pbap_connected(struct bt_conn *base_conn)
{
	bd_address_t *addr = (bd_address_t *)GET_CONN_BT_ADDR(base_conn);

	btsrv_rdm_set_pbap_connected(base_conn, true);
	btsrv_proc_link_change(addr->val, BTSRV_LINK_PBAP_CONNECTED);
	return 0;
}

static int btsrv_connect_pbap_disconnected(struct bt_conn *base_conn)
{
	bd_address_t *addr = (bd_address_t *)GET_CONN_BT_ADDR(base_conn);

	btsrv_rdm_set_pbap_connected(base_conn, false);
	btsrv_proc_link_change(addr->val, BTSRV_LINK_PBAP_DISCONNECTED);
	return 0;
}

static int btsrv_connect_map_connected(struct bt_conn *base_conn)
{
	bd_address_t *addr = (bd_address_t *)GET_CONN_BT_ADDR(base_conn);

	btsrv_rdm_set_map_connected(base_conn, true);
	btsrv_proc_link_change(addr->val, BTSRV_LINK_MAP_CONNECTED);
	return 0;
}

static int btsrv_connect_map_disconnected(struct bt_conn *base_conn)
{
	bd_address_t *addr = (bd_address_t *)GET_CONN_BT_ADDR(base_conn);

	btsrv_rdm_set_map_connected(base_conn, false);
	btsrv_proc_link_change(addr->val, BTSRV_LINK_MAP_DISCONNECTED);
	return 0;
}

static int btsrv_connect_hid_connected(struct bt_conn *base_conn)
{
	bd_address_t *addr = (bd_address_t *)GET_CONN_BT_ADDR(base_conn);

	btsrv_rdm_set_hid_connected(base_conn, true);
	btsrv_notify_link_event(base_conn, BT_LINK_EV_HID_CONNECTED, 0);
	btsrv_proc_link_change(addr->val, BTSRV_LINK_HID_CONNECTED);
	return 0;
}

static int btsrv_connect_hid_disconnected(struct bt_conn *base_conn)
{
	bd_address_t *addr = (bd_address_t *)GET_CONN_BT_ADDR(base_conn);

	btsrv_rdm_set_hid_connected(base_conn, false);
	btsrv_notify_link_event(base_conn, BT_LINK_EV_HID_DISCONNECTED, 0);
	btsrv_proc_link_change(addr->val, BTSRV_LINK_HID_DISCONNECTED);
	return 0;
}

static int btsrv_connect_hid_unplug(struct bt_conn *base_conn)
{
	int i;
	struct autoconn_info *tmpInfo;
	bd_address_t *addr = (bd_address_t *)GET_CONN_BT_ADDR(base_conn);
	
	tmpInfo = mem_malloc(sizeof(struct autoconn_info)*BTSRV_SAVE_AUTOCONN_NUM);
	if (!tmpInfo) {
		SYS_LOG_ERR("malloc failed");
		return -ENOMEM;
	}
	
	btsrv_connect_get_auto_reconnect_info(tmpInfo, BTSRV_SAVE_AUTOCONN_NUM);
	for (i = 0; i < BTSRV_SAVE_AUTOCONN_NUM; i++) {
		if (tmpInfo[i].addr_valid) {
			if (!memcmp(tmpInfo[i].addr.val, addr->val, sizeof(bd_address_t))) {
				if(tmpInfo[i].hid && !tmpInfo[i].a2dp && !tmpInfo[i].hfp
					&& !tmpInfo[i].avrcp){
					tmpInfo[i].addr_valid = 0;
					SYS_LOG_INF("clear info");
				}
				break;
			}
		}
	}
SYS_LOG_INF(":\n");
	btsrv_connect_set_auto_reconnect_info(tmpInfo,BTSRV_SAVE_AUTOCONN_NUM,AUTOCONN_INFO_PHONE);
	if (tmpInfo) {
		mem_free(tmpInfo);
	}
	return 0;
}

static void btsrv_controler_role_discovery(struct bt_conn *conn)
{
	uint8_t role;

	if (btsrv_is_snoop_conn(conn)) {
		return;
	}

	if (hostif_bt_conn_role_discovery(conn, &role)) {
		SYS_LOG_ERR("Failed to discovery role");
		return;
	}

	btsrv_rdm_set_controler_role(conn, role);

	if (role == CONTROLER_ROLE_MASTER) {
		hostif_bt_conn_set_supervision_timeout(conn, BT_SUPERVISION_TIMEOUT);
	}
	btsrv_notify_link_event(conn, BT_LINK_EV_ROLE_CHANGE, role);
}

void btsrv_connect_set_phone_controler_role(bd_address_t *bd, uint8_t role)
{
	struct bt_conn *conn = btsrv_rdm_find_conn_by_addr(bd);
	uint8_t dev_role, dev_exp_role;

	if (conn == NULL) {
		return;
	}

	dev_exp_role = (role == CONTROLER_ROLE_MASTER) ? CONTROLER_ROLE_SLAVE : CONTROLER_ROLE_MASTER;
	dev_role = btsrv_rdm_get_controler_role(conn);

    if(btsrv_info->cfg.pts_test_mode == 0){
    	if ((btsrv_rdm_get_tws_role(conn) == BTSRV_TWS_NONE) && (dev_role != dev_exp_role)) {
    		/* Controler request do role swith after security */
    		SYS_LOG_INF("Do role(%d) switch", dev_exp_role);
    		hostif_bt_conn_switch_role(conn, dev_exp_role);
    	}
	}
}

static bool btsrv_check_connectable(uint8_t role)
{
	bool connectable = true;
	uint8_t dev_role = btsrv_rdm_get_dev_role();
	uint8_t connected_cnt = btsrv_rdm_get_connected_dev(NULL, NULL);

	SYS_LOG_INF("check_connectable:%d,%d,%d,%d,%d",
		role, dev_role, connected_cnt, btsrv_max_conn_num(), btsrv_max_phone_num());

	if (connected_cnt > btsrv_max_conn_num()) {
		/* Already connect max conn */
		connectable = false;
	} else if ((dev_role == BTSRV_TWS_NONE) && (role == BTSRV_TWS_NONE) &&
				(connected_cnt > btsrv_max_phone_num())) {
		/* Already connect max phone number, can't connect any phone */
		connectable = false;
	} else if (dev_role != BTSRV_TWS_NONE && role != BTSRV_TWS_NONE) {
		/* Already connect as tws device, Can't connect another tws device */
		connectable = false;
	}

	return connectable;
}

static void btsrv_get_name_finish(void *info, uint8_t role)
{
	struct bt_conn *conn;
	struct btsrv_addr_name *mac_addr_info = (struct btsrv_addr_name *)info;

	if (!mac_addr_info) {
		return;
	}

	conn = btsrv_rdm_find_conn_by_addr(&mac_addr_info->mac);
	if (conn == NULL) {
		SYS_LOG_ERR("Can't find conn for addr\n");
		return;
	}

	if (!btsrv_check_connectable(role)) {
		SYS_LOG_INF("Disconnect role:%d", role);
		btsrv_adapter_disconnect(conn);
		return;
	}

	btsrv_rdm_set_dev_name(conn, mac_addr_info->name);
	btsrv_notify_link_event(conn, BT_LINK_EV_GET_NAME, role);
	btsrv_proc_link_change(mac_addr_info->mac.val, BTSRV_LINK_BASE_GET_NAME);
	if (role != BTSRV_TWS_NONE) {
		btsrv_event_notify(MSG_BTSRV_TWS, MSG_BTSRV_GET_NAME_FINISH, conn);
	} else {
#if 0
		uint8_t controler_role;

		controler_role = btsrv_rdm_get_controler_role(conn);
		if (controler_role == CONTROLER_ROLE_MASTER) {
			/* For snoop tws, connected with phone, always as slave */
			hostif_bt_conn_switch_role(conn, CONTROLER_ROLE_SLAVE);
		}
#endif
	}
}

static void connected_dev_cb_do_disconnect(struct bt_conn *base_conn, uint8_t tws_dev, void *cb_param)
{
	int disconnect_mode = (int)cb_param;

	if (disconnect_mode == BTSRV_DISCONNECT_ALL_MODE) {
		if (tws_dev) {
			btsrv_event_notify_malloc(MSG_BTSRV_TWS, MSG_BTSRV_TWS_DISCONNECT, (uint8_t *)GET_CONN_BT_ADDR(base_conn), sizeof(bd_address_t), 0);
		} else {
			btsrv_adapter_disconnect(base_conn);
		}
	} else if (disconnect_mode == BTSRV_DISCONNECT_PHONE_MODE) {
		if (!tws_dev) {
			btsrv_adapter_disconnect(base_conn);
		}
	} else if (disconnect_mode == BTSRV_DISCONNECT_TWS_MODE) {
		if (tws_dev) {
			btsrv_event_notify_malloc(MSG_BTSRV_TWS, MSG_BTSRV_TWS_DISCONNECT, (uint8_t *)GET_CONN_BT_ADDR(base_conn), sizeof(bd_address_t), 0);
		}
	}
}

static void btsrv_connect_disconnect_device(int disconnect_mode)
{
	SYS_LOG_INF("disconnect mode %d", disconnect_mode);
	btsrv_rdm_get_connected_dev(connected_dev_cb_do_disconnect, (void *)disconnect_mode);
}

static void btsrv_connect_clear_list(int mode)
{
	int count;
	int disconnec_mode = 0;
	btsrv_connect_auto_connection_stop(BTSRV_STOP_AUTO_RECONNECT_ALL);

	if (mode == BTSRV_DEVICE_ALL) {
		disconnec_mode = BTSRV_DISCONNECT_ALL_MODE;
	} else if (mode == BTSRV_DEVICE_PHONE) {
		disconnec_mode = BTSRV_DISCONNECT_PHONE_MODE;
	} else if (mode == BTSRV_DEVICE_TWS) {
		disconnec_mode = BTSRV_DISCONNECT_TWS_MODE;
	}

	if(mode != BTSRV_DEVICE_ALL){
	    count = btsrv_rdm_get_connected_dev_cnt_by_type(mode);
		if(count){
			btsrv_rdm_get_connected_dev(connected_dev_cb_do_disconnect, (void *)disconnec_mode);
        }
	}
	else{
        count = btsrv_rdm_get_connected_dev(connected_dev_cb_do_disconnect, (void *)disconnec_mode);
	}

    SYS_LOG_INF("mode:%d count:%d",mode,count);

	if (count == 0) {
		btsrv_scan_update_mode(true);
		p_connect->clear_list_mode = mode;
		btsrv_event_notify(MSG_BTSRV_CONNECT, MGS_BTSRV_CLEAR_AUTO_INFO, NULL);
		SYS_LOG_INF("clear list finish");
	} else {
		p_connect->clear_list_disconnecting = 1;
		p_connect->clear_list_mode = mode;
	}
}

static void btsrv_connect_clear_share_tws(void)
{
    char addr_str[BT_ADDR_STR_LEN];
    uint8_t i, value;
	uint8_t bt_addr[BT_MAC_LEN];

	btsrv_property_set_factory(CFG_BT_SHARE_TWS_MAC, (char *) NULL, 0);
#ifdef CONFIG_PROPERTY
    if (property_get_byte_array(CFG_BT_MAC, btsrv_info->device_addr, sizeof(btsrv_info->device_addr), NULL)) {
        SYS_LOG_WRN("failed to get BT_MAC");
    }
    else
#endif
    {
        /* Like stack, low mac address save in low memory address */
        btsrv_info->share_mac_dev = 0;
        memset(bt_addr,0,BT_MAC_LEN);
        hostif_bt_set_share_bd_addr(false, bt_addr);

        for (i=0; i<3; i++) {
            value = btsrv_info->device_addr[i];
            btsrv_info->device_addr[i] = btsrv_info->device_addr[5 -i];
            btsrv_info->device_addr[5 -i] = value;
        }
		hostif_bt_vs_write_bt_addr((bt_addr_t *)btsrv_info->device_addr);
        memset(addr_str, 0, sizeof(addr_str));
        hostif_bt_addr_to_str((bt_addr_t *)btsrv_info->device_addr, addr_str, BT_ADDR_STR_LEN);
        SYS_LOG_INF("BT_MAC:%s", addr_str);
    }
}

bool btsrv_connect_check_share_tws(void)
{
    return btsrv_info->share_mac_dev;
}

static void btsrv_auto_connect_proc(struct bt_set_autoconn *param)
{
	int i, idle_pos = BTSRV_SAVE_AUTOCONN_NUM, need_start_timer = 0;
	struct autoconn_info * tws_info;
	struct bt_conn *base_conn;

	base_conn = btsrv_rdm_find_conn_by_addr(&param->addr);
	if (base_conn) {
		if(btsrv_rdm_is_a2dp_signal_connected(base_conn) &&
			!btsrv_rdm_is_a2dp_connected(base_conn)) {
			btsrv_rdm_set_wait_to_diconnect(base_conn, false);
		}
		SYS_LOG_INF("Already connected");
		return;
	}

	for (i = 0; i < BTSRV_SAVE_AUTOCONN_NUM; i++) {
		if (!p_connect->auto_conn[i].addr_valid &&
			idle_pos == BTSRV_SAVE_AUTOCONN_NUM) {
			idle_pos = i;
		}

		if (p_connect->auto_conn[i].addr_valid &&
			!memcmp(param->addr.val, p_connect->auto_conn[i].addr.val, sizeof(bd_address_t))) {
			SYS_LOG_INF("Device already in reconnect list");
			return;
		}
	}

	if (idle_pos == BTSRV_SAVE_AUTOCONN_NUM) {
		SYS_LOG_ERR("Not more position for reconnect device");
		return;
	}

	for (i = 0; i < BTSRV_SAVE_AUTOCONN_NUM; i++) {
        if(param->reconnect_mode != BTSRV_TWS_RECONNECT_NONE)
        {
            memcpy(p_connect->auto_conn[idle_pos].addr.val, param->addr.val, sizeof(bd_address_t));
            p_connect->auto_conn[idle_pos].addr_valid = 1;
            
            if(param->reconnect_mode == BTSRV_TWS_RECONNECT_AUTO_PAIR){
				p_connect->auto_conn[idle_pos].tws_role = BTSRV_TWS_NONE;
            }
			else{
                tws_info = btsrv_connect_get_tws_paired_info();
                if(tws_info){
				    p_connect->auto_conn[idle_pos].tws_role = tws_info->tws_role;
                }
				else{
                    p_connect->auto_conn[idle_pos].tws_role = BTSRV_TWS_MASTER;
                }
			}
            btif_tws_set_expect_role(p_connect->auto_conn[idle_pos].tws_role);
            
			p_connect->auto_conn[idle_pos].a2dp = 0;
			p_connect->auto_conn[idle_pos].hfp = 0;
			p_connect->auto_conn[idle_pos].avrcp = 0;
			p_connect->auto_conn[idle_pos].hid = 0;
			p_connect->auto_conn[idle_pos].hfp_first = 0;
			p_connect->auto_conn[idle_pos].strategy = param->strategy;
			p_connect->auto_conn[idle_pos].base_try = param->base_try;
			p_connect->auto_conn[idle_pos].profile_try = param->profile_try;
			p_connect->auto_conn[idle_pos].curr_connect_profile = AUTOCONN_PROFILE_IDLE;
			p_connect->auto_conn[idle_pos].state = AUTOCONN_STATE_IDLE;
			p_connect->auto_conn[idle_pos].base_interval = param->base_interval;
			p_connect->auto_conn[idle_pos].profile_interval = param->profile_interval;
			p_connect->auto_conn[idle_pos].tws_mode = param->reconnect_mode;
			p_connect->auto_conn[idle_pos].reconnect_phone_timeout = param->reconnect_phone_timeout;
            p_connect->auto_conn[idle_pos].reconnect_tws_timeout = param->reconnect_tws_timeout;
			p_connect->auto_conn[idle_pos].first_reconnect = 1;
            p_connect->auto_conn[idle_pos].reason_times = 0;
            p_connect->auto_conn[idle_pos].base_reconnect_times = 0;
            p_connect->auto_conn[idle_pos].profile_reconnect_times = 0;
            p_connect->auto_conn[idle_pos].last_reason = 0;
            p_connect->auto_conn[idle_pos].last_begin_time = 0;
			need_start_timer = 1;
			SYS_LOG_INF("TWS MODE:%d VALID:%d ROLE:%d",param->reconnect_mode,
				p_connect->auto_conn[idle_pos].addr_valid,
				p_connect->auto_conn[idle_pos].tws_role);
			break;
		}
		if (p_connect->nvram_reconn_info[i].addr_valid &&
			!memcmp(p_connect->nvram_reconn_info[i].addr.val, param->addr.val, sizeof(bd_address_t))){
			memcpy(p_connect->auto_conn[idle_pos].addr.val, param->addr.val, sizeof(bd_address_t));
			p_connect->auto_conn[idle_pos].addr_valid = 1;
			p_connect->auto_conn[idle_pos].tws_role = BTSRV_TWS_NONE;
			p_connect->auto_conn[idle_pos].a2dp = p_connect->nvram_reconn_info[i].a2dp;
			p_connect->auto_conn[idle_pos].hfp = p_connect->nvram_reconn_info[i].hfp;
			p_connect->auto_conn[idle_pos].avrcp = p_connect->nvram_reconn_info[i].avrcp;
			p_connect->auto_conn[idle_pos].hid = 0;
			p_connect->auto_conn[idle_pos].hfp_first = p_connect->nvram_reconn_info[i].hfp_first;
			p_connect->auto_conn[idle_pos].strategy = param->strategy;
			p_connect->auto_conn[idle_pos].base_try = param->base_try;
			p_connect->auto_conn[idle_pos].profile_try = param->profile_try;
			p_connect->auto_conn[idle_pos].curr_connect_profile = AUTOCONN_PROFILE_IDLE;
			p_connect->auto_conn[idle_pos].state = AUTOCONN_STATE_IDLE;
			p_connect->auto_conn[idle_pos].base_interval = param->base_interval;
			p_connect->auto_conn[idle_pos].profile_interval = param->profile_interval;
			p_connect->auto_conn[idle_pos].tws_mode = BTSRV_TWS_RECONNECT_NONE;
			p_connect->auto_conn[idle_pos].reconnect_phone_timeout = param->reconnect_phone_timeout;
            p_connect->auto_conn[idle_pos].reconnect_tws_timeout = param->reconnect_tws_timeout;
			p_connect->auto_conn[idle_pos].first_reconnect = 1;
            p_connect->auto_conn[idle_pos].reason_times = 0;
            p_connect->auto_conn[idle_pos].base_reconnect_times = 0;
            p_connect->auto_conn[idle_pos].profile_reconnect_times = 0;
            p_connect->auto_conn[idle_pos].last_reason = 0;
            p_connect->auto_conn[idle_pos].last_begin_time = 0;
			need_start_timer = 1;
			SYS_LOG_INF("PHONE MODE:%d VALID:%d ROLE:%d",
				param->reconnect_mode,
				p_connect->auto_conn[idle_pos].addr_valid,
				p_connect->auto_conn[idle_pos].tws_role);
			SYS_LOG_INF("oo:%d :%d :%d:%d:%d",
				idle_pos,
				p_connect->auto_conn[idle_pos].a2dp,
				p_connect->auto_conn[idle_pos].hfp,
				p_connect->auto_conn[idle_pos].hfp_first,
				p_connect->auto_conn[idle_pos].avrcp);
			break;
		}
	}

	if (i == BTSRV_SAVE_AUTOCONN_NUM) {
		SYS_LOG_ERR("Device not in AUTOCONN_INFO_NVRAM");
	}

	/* Only need start timer when is stop */
	if (need_start_timer && (!thread_timer_is_running(&p_connect->auto_conn_timer))) {
		p_connect->connecting_index = idle_pos;
		thread_timer_start(&p_connect->auto_conn_timer, 1 /*AUTOCONN_START_TIME*/, 0);
		p_connect->reconnect_req_high_performance = 1;
		btsrv_adapter_set_reconnect_state(true);
		btsrv_update_performance_req();
	}
}

int btsrv_connect_process(struct app_msg *msg)
{
	int ret = 0;

	switch (_btsrv_get_msg_param_cmd(msg)) {
	case MSG_BTSRV_CONNECTED:
		SYS_LOG_INF("MSG_BTSRV_CONNECTED");
		btsrv_connect_connected(msg->ptr);
		btsrv_controler_role_discovery(msg->ptr);
		btsrv_adapter_callback(BTSRV_RELEASE_HIGH_PERFORMANCE, NULL);
		/* ref for bt servcie process MSG_BTSRV_CONNECTED,
		 * need unref conn after process MSG_BTSRV_CONNECTED.
		 */
		hostif_bt_conn_unref(msg->ptr);
		break;
	case MSG_BTSRV_CONNECTED_FAILED:
		SYS_LOG_INF("MSG_BTSRV_CONNECTED_FAILED");
		btsrv_proc_link_change(msg->ptr, BTSRV_LINK_BASE_CONNECTED_FAILED);
		btsrv_event_notify_malloc(MSG_BTSRV_TWS, _btsrv_get_msg_param_cmd(msg), msg->ptr, sizeof(bd_address_t), 0);
		break;
	case MSG_BTSRV_REMOTE_LINKKEY_MISS:
		btsrv_connect_auto_connection_clear_device(msg->ptr);
		break;
	case MSG_BTSRV_SECURITY_CHANGED:
		btsrv_connect_security_changed(msg->ptr);
		break;
	case MSG_BTSRV_ROLE_CHANGE:
		btsrv_rdm_set_controler_role(msg->ptr, _btsrv_get_msg_param_reserve(msg));
		if (_btsrv_get_msg_param_reserve(msg) == CONTROLER_ROLE_MASTER) {
			hostif_bt_conn_set_supervision_timeout(msg->ptr, BT_SUPERVISION_TIMEOUT);
		}
		btsrv_notify_link_event(msg->ptr, BT_LINK_EV_ROLE_CHANGE, _btsrv_get_msg_param_reserve(msg));
		btsrv_event_notify_ext(MSG_BTSRV_TWS, MSG_BTSRV_ROLE_CHANGE, msg->ptr, _btsrv_get_msg_param_reserve(msg));
		break;
	case MSG_BTSRV_MODE_CHANGE:
		btsrv_sniff_mode_change(msg->ptr);
		break;
	case MSG_BTSRV_DISCONNECTED:
		SYS_LOG_INF("MSG_BTSRV_DISCONNECTED");
		btsrv_connect_disconnected(msg->ptr, _btsrv_get_msg_param_reserve(msg));
		btsrv_adapter_callback(BTSRV_RELEASE_HIGH_PERFORMANCE, NULL);
		break;
	case MSG_BTSRV_A2DP_CONNECTED:
		SYS_LOG_INF("MSG_BTSRV_A2DP_CONNECTED");
		btsrv_connect_a2dp_connected(msg->ptr);
		if (btsrv_rdm_get_tws_role(msg->ptr) != BTSRV_TWS_NONE) {
			btsrv_event_notify(MSG_BTSRV_TWS, _btsrv_get_msg_param_cmd(msg), msg->ptr);
		} else {
			/* Phone a2dp may in connecting when receive MSG_BTSRV_A2DP_CHECK_SWITCH_SBC
			 * check agine when phone a2dp connected.
			 */
			btsrv_connect_check_switch_sbc();
			btsrv_connect_proc_switch_sbc_state(msg->ptr, MSG_BTSRV_A2DP_CONNECTED);
		}
		btsrv_event_notify(MSG_BTSRV_A2DP, _btsrv_get_msg_param_cmd(msg), msg->ptr);
		break;
	case MSG_BTSRV_A2DP_SIGNALING_CONNECTED:
		SYS_LOG_INF("MSG_BTSRV_A2DP_SIGNALING_CONNECTED");
		btsrv_connect_a2dp_singnaling_connected(msg->ptr);
	    break;

	case MSG_BTSRV_A2DP_DISCONNECTED:
		SYS_LOG_INF("MSG_BTSRV_A2DP_DISCONNECTED");
		if (btsrv_rdm_get_tws_role(msg->ptr) != BTSRV_TWS_NONE) {
			btsrv_event_notify(MSG_BTSRV_TWS, _btsrv_get_msg_param_cmd(msg), msg->ptr);
		}

		btsrv_connect_a2dp_disconnected(msg->ptr);
		btsrv_connect_proc_switch_sbc_state(msg->ptr, MSG_BTSRV_A2DP_DISCONNECTED);
		btsrv_adapter_callback(BTSRV_RELEASE_HIGH_PERFORMANCE, NULL);
		break;

	case MSG_BTSRV_A2DP_CHANNEL_ERR:
		SYS_LOG_INF("MSG_BTSRV_A2DP_CHANNEL_ERR");
		btsrv_connect_a2dp_channel_err(msg->ptr);
	    break;

	case MSG_BTSRV_AVRCP_CONNECTED:
		SYS_LOG_INF("MSG_BTSRV_AVRCP_CONNECTED");
		btsrv_connect_avrcp_connected(msg->ptr);
		btsrv_event_notify(MSG_BTSRV_AVRCP, _btsrv_get_msg_param_cmd(msg), msg->ptr);
		break;
	case MSG_BTSRV_AVRCP_DISCONNECTED:
		SYS_LOG_INF("MSG_BTSRV_AVRCP_DISCONNECTED");
		btsrv_connect_avrcp_disconnected(msg->ptr);
		btsrv_event_notify(MSG_BTSRV_AVRCP, _btsrv_get_msg_param_cmd(msg), msg->ptr);
		btsrv_adapter_callback(BTSRV_RELEASE_HIGH_PERFORMANCE, NULL);
		break;
	case MSG_BTSRV_HID_CONNECTED:
		SYS_LOG_INF("MSG_BTSRV_HID_CONNECTED");
		btsrv_connect_hid_connected(msg->ptr);
		btsrv_event_notify(MSG_BTSRV_HID, _btsrv_get_msg_param_cmd(msg), msg->ptr);
		break;
	case MSG_BTSRV_HID_DISCONNECTED:
		SYS_LOG_INF("MSG_BTSRV_HID_DISCONNECTED");
		btsrv_connect_hid_disconnected(msg->ptr);
		btsrv_event_notify(MSG_BTSRV_HID, _btsrv_get_msg_param_cmd(msg), msg->ptr);
		break;
	case MSG_BTSRV_HID_UNPLUG:
		SYS_LOG_INF("MSG_BTSRV_HID_UNPLUG");
		btsrv_connect_hid_unplug(msg->ptr);
		break;
	case MSG_BTSRV_HFP_AG_CONNECTED:
		SYS_LOG_INF("MSG_BTSRV_HFP_AG_CONNECTED");
		btsrv_rdm_set_hfp_role(msg->ptr,BTSRV_HFP_ROLE_AG);
		btsrv_connect_hfp_connected(msg->ptr, msg->cmd);
		if (btsrv_rdm_get_tws_role(msg->ptr) >= BTSRV_TWS_PENDING) {
			btsrv_event_notify(MSG_BTSRV_TWS, _btsrv_get_msg_param_cmd(msg), msg->ptr);
		}
		btsrv_event_notify(MSG_BTSRV_HFP_AG, _btsrv_get_msg_param_cmd(msg), msg->ptr);
		break;
	case MSG_BTSRV_HFP_CONNECTED:
		SYS_LOG_INF("MSG_BTSRV_HFP_CONNECTED");
		btsrv_rdm_set_hfp_role(msg->ptr,BTSRV_HFP_ROLE_HF);
		btsrv_connect_hfp_connected(msg->ptr, msg->cmd);
		if (btsrv_rdm_get_tws_role(msg->ptr) >= BTSRV_TWS_PENDING) {
			btsrv_event_notify(MSG_BTSRV_TWS, _btsrv_get_msg_param_cmd(msg), msg->ptr);
		}
		btsrv_event_notify(MSG_BTSRV_HFP, _btsrv_get_msg_param_cmd(msg), msg->ptr);
		break;
	case MSG_BTSRV_HFP_AG_DISCONNECTED:
		btsrv_rdm_set_hfp_role(msg->ptr,BTSRV_HFP_ROLE_HF);
	case MSG_BTSRV_HFP_DISCONNECTED:
		SYS_LOG_INF("MSG_BTSRV_HFP_DISCONNECTED");
		/* Why need check btsrv_rdm_is_hfp_connected ?? */
		if (btsrv_rdm_is_hfp_connected(msg->ptr)) {
			btsrv_connect_hfp_disconnected(msg->ptr, msg->cmd);
		}
		if (btsrv_rdm_get_tws_role(msg->ptr) >= BTSRV_TWS_PENDING) {
			btsrv_event_notify(MSG_BTSRV_TWS, _btsrv_get_msg_param_cmd(msg), msg->ptr);
		}
		btsrv_adapter_callback(BTSRV_RELEASE_HIGH_PERFORMANCE, NULL);		
		break;
	case MSG_BTSRV_SPP_CONNECTED:
		btsrv_connect_spp_connected(msg->ptr);
		break;
	case MSG_BTSRV_SPP_DISCONNECTED:
		btsrv_connect_spp_disconnected(msg->ptr);
		break;
	case MSG_BTSRV_PBAP_CONNECTED:
		btsrv_connect_pbap_connected(msg->ptr);
		break;
	case MSG_BTSRV_PBAP_DISCONNECTED:
		btsrv_connect_pbap_disconnected(msg->ptr);
		break;
	case MSG_BTSRV_MAP_CONNECTED:
		btsrv_connect_map_connected(msg->ptr);
		break;
	case MSG_BTSRV_MAP_DISCONNECTED:
		btsrv_connect_map_disconnected(msg->ptr);
		break;
	case MSG_BTSRV_GET_NAME_FINISH:
		btsrv_get_name_finish(msg->ptr, _btsrv_get_msg_param_reserve(msg));
		break;
	case MSG_BTSRV_CONNREQ_ADD_MONITOR:
		btsrv_connect_req_changed(msg->ptr);
		break;
	case MSG_BTSRV_CLEAR_LIST_CMD:
		btsrv_connect_clear_list((int)(_btsrv_get_msg_param_ptr(msg)));
		break;
	case MGS_BTSRV_CLEAR_AUTO_INFO:
		btsrv_autoconn_info_clear();
		break;
	case MSG_BTSRV_AUTO_RECONNECT:
		SYS_LOG_INF("MSG_BTSRV_AUTO_RECONNECT: %d", os_uptime_get_32());
		btsrv_auto_connect_proc(msg->ptr);
		break;
	case MSG_BTSRV_AUTO_RECONNECT_STOP:
		btsrv_connect_auto_connection_stop((int)(_btsrv_get_msg_param_ptr(msg)));
		break;
    case MSG_BTSRV_AUTO_RECONNECT_STOP_DEVICE:
		btsrv_connect_auto_connection_stop_device(msg->ptr);
		break;
	case MSG_BTSRV_AUTO_RECONNECT_STOP_EXCEPT_DEVICE:
		btsrv_connect_remote_connect_stop_except_device(msg->ptr);
		break;
	case MSG_BTSRV_DISCONNECT_DEVICE:
		btsrv_connect_disconnect_device((int)(_btsrv_get_msg_param_ptr(msg)));
		break;
	case MSG_BTSRV_A2DP_CHECK_SWITCH_SBC:
		btsrv_connect_check_switch_sbc();
		break;
    case MSG_BTSRV_CLEAR_SHARE_TWS:
		btsrv_connect_clear_share_tws();
		break;
	case MSG_BTSRV_AUTO_RECONNECT_CONNECTION_CANCEL_DEVICE:
		btsrv_connect_auto_connection_cancel_device();
		break;
	default:
		break;
	}

	return ret;
}

bool btsrv_connect_is_reconnect_info_empty(void)
{
	int dev_cnt = 0, i;
	for (i = 0; i < BTSRV_SAVE_AUTOCONN_NUM; i++) {
		if (p_connect->nvram_reconn_info[i].addr_valid) {
			dev_cnt++;
		}
	}

	return (dev_cnt == 0) ? true : false;
}

bool btsrv_autoconn_is_reconnecting(void)
{
	uint8_t index = p_connect->connecting_index;

	if ((!p_connect->auto_conn[index].addr_valid)
		&& (p_connect->auto_conn[index].tws_mode != BTSRV_TWS_RECONNECT_AUTO_PAIR)) {
		return false;
	}

	return (p_connect->auto_conn[index].state == AUTOCONN_STATE_IDLE ||
			p_connect->auto_conn[index].state == AUTOCONN_STATE_TWS_CONNECTING ||
			p_connect->auto_conn[index].state == AUTOCONN_STATE_PHONE_CONNECTING) ? false : true;
}

bool btsrv_autoconn_is_nodiscoverable(void)
{
	uint8_t index = p_connect->connecting_index;
	return p_connect->auto_conn[index].first_reconnect;
}

bool btsrv_autoconn_is_runing(void)
{
	return p_connect->reconnect_req_high_performance ? true : false;
}

bool btsrv_autoconn_is_phone_connecting(void)
{
	return p_connect->reconnect_br_connect  ? true : false;
}

bool btsrv_autoconn_is_phone_first_reconnect(void)
{
	uint8_t index = p_connect->connecting_index;
	if(p_connect->auto_conn[index].tws_mode == BTSRV_TWS_RECONNECT_NONE
		&& p_connect->auto_conn[index].first_reconnect){
        return true;
	}
    else
		return false;
}

bool btsrv_autoconn_is_tws_pair_first(uint8_t role)
{
	uint8_t i;
    bool result = false;
	for (i = 0; i < BTSRV_SAVE_AUTOCONN_NUM; i++) {	
		if (p_connect->auto_conn[i].addr_valid) {
			if((p_connect->auto_conn[i].tws_role == role) 
                && p_connect->auto_conn[i].first_reconnect){
				result = true;
				break;
			}
		}
	}
	return result;
}

void btsrv_connect_tws_role_confirm(void)
{
	/* Already as tws slave, stop autoconnect to phone  and disconnect connected phone */
	if (btsrv_rdm_get_dev_role() == BTSRV_TWS_SLAVE) {
		btsrv_connect_auto_connection_stop(BTSRV_STOP_AUTO_RECONNECT_ALL);
		btsrv_connect_disconnect_device(BTSRV_DISCONNECT_PHONE_MODE);
	}
}

void btsrv_connect_set_gfp_status(bool valid)
{
    if(p_connect){
        p_connect->gfp_running = valid;
    }
}

bool btsrv_connect_get_gfp_status(void)
{
    if(p_connect && p_connect->gfp_running){
        return true;
    }
    else{
        return false;
    }
}

int btsrv_connect_init(void)
{
	p_connect = &p_btsrv_connect;

	memset(p_connect, 0, sizeof(struct btsrv_connect_priv));
	thread_timer_init(&p_connect->auto_conn_timer, btsrv_autoconn_timer_handler, NULL);
	thread_timer_init(&p_connect->monitor_conn_timer, btsrv_monitor_timer_handler, NULL);

	btsrv_property_get(CFG_BT_AUTOCONN_TWS, (char *)&p_connect->nvram_reconn_info[0],
						(sizeof(struct autoconn_info)));

	btsrv_property_get(CFG_AUTOCONN_INFO, (char *)&p_connect->nvram_reconn_info[1],
						(sizeof(struct autoconn_info) * (BTSRV_SAVE_AUTOCONN_NUM - 1)));
	
	//for (uint8 i = 0,i < BTSRV_SAVE_AUTOCONN_NUM)

	//SYS_LOG_INF(":%d,%d,%d,%d,%d\n",p_connect->nvram_reconn_info[1].a2dp,p_connect->nvram_reconn_info[1].hfp,
	//p_connect->nvram_reconn_info[1].avrcp,p_connect->nvram_reconn_info[1].hfp_first,
	//p_connect->nvram_reconn_info[1].tws_role);	
	btsrv_connect_dump_info();				

	return 0;
}

void btsrv_connect_deinit(void)
{
	if (p_connect == NULL) {
		return;
	}

	while (p_connect->auto_connect_running) {
		os_sleep(10);
	}

	if (thread_timer_is_running(&p_connect->auto_conn_timer)) {
		thread_timer_stop(&p_connect->auto_conn_timer);
		p_connect->reconnect_req_high_performance = 0;
        btsrv_adapter_set_reconnect_state(false);
		btsrv_update_performance_req();
	}

	while (p_connect->monitor_timer_running) {
		os_sleep(10);
	}

	if (thread_timer_is_running(&p_connect->monitor_conn_timer)) {
		thread_timer_stop(&p_connect->monitor_conn_timer);
	}

	p_connect = NULL;
}

void btsrv_connect_dump_info(void)
{
	char addr_str[BT_ADDR_STR_LEN];
	int i;
	struct auto_conn_t *auto_conn;
	struct autoconn_info *info;

	printk("connect info\n");
	if (p_connect == NULL) {
		SYS_LOG_INF("Auto reconnect not init");
		return;
	}

	SYS_LOG_INF("index: %d\n", p_connect->connecting_index);
	for (i = 0; i < BTSRV_SAVE_AUTOCONN_NUM; i++) {
		auto_conn = &p_connect->auto_conn[i];
		if (auto_conn->addr_valid) {
			hostif_bt_addr_to_str((const bt_addr_t *)&auto_conn->addr, addr_str, BT_ADDR_STR_LEN);
			printk("Dev index %d, state %d, mac: %s\n", i, auto_conn->state, addr_str);
			printk("a2dp %d, avrcp %d, hfp %d hid %d\n", auto_conn->a2dp, auto_conn->avrcp, auto_conn->hfp, auto_conn->hid);
			printk("tws_role %d, strategy %d\n", auto_conn->tws_role, auto_conn->strategy);
			printk("base_try %d, profile_try %d\n", auto_conn->base_try, auto_conn->profile_try);
			printk("base_interval %d, profile_interval %d\n", auto_conn->base_interval, auto_conn->profile_interval);
			printk("\n");
		}
	}

	printk("nvram_reconn_info\n");
	for (i = 0; i < BTSRV_SAVE_AUTOCONN_NUM; i++) {
		info = &p_connect->nvram_reconn_info[i];
		if (info->addr_valid) {
			hostif_bt_addr_to_str((const bt_addr_t *)&info->addr, addr_str, BT_ADDR_STR_LEN);
			printk("(%d)%s tws_role %d, profile %d, %d, %d, %d, %d, active %d\n", i, addr_str, info->tws_role,
					info->hfp, info->a2dp, info->avrcp, info->hid, info->hfp_first, info->active);
		}
	}
	printk("\n");
}
