/*
 * Copyright (c) 2016 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt service adapter interface
 */

#define SYS_LOG_DOMAIN "btsrv_adapter"
#include "btsrv_os_common.h"
#include "btsrv_inner.h"
#include "ctrl_interface.h"

#define BTSRV_CONNREQINFO_NUM                (3)
#define BTSRV_DEV_CLASS_SIZE					(3)

#define CHECK_WAIT_DISCONNECT_INTERVAL		(100)	/* 100ms */
#define CHECK_WAIT_AVRCP_PAUSE_INTERVAL		(1000)	/* 1000ms */
#define CHECK_WAIT_A2DP_CONNECTED_INTERVAL  (3000)	/* 3000ms */

#define CHECK_WAIT_PROFILE_DISCONNECTED_INTERVAL	(500)   /* 500ms */

#define WAKE_LOCK_TIMER_INTERVAL			(1000)	/* 1s */
#define WAKE_LOCK_IDLE_TIMEOUT				(1000*10)	/* 10s */

struct btsrv_info_t *btsrv_info;
static struct btsrv_info_t btsrv_btinfo;
static btsrv_discover_result_cb btsrv_discover_cb;
static struct conn_req_info btsrv_connreq_info[BTSRV_CONNREQINFO_NUM];
#ifdef CONFIG_LOGSRV_BT_DEBUG
static bool btsrv_link_quality_always = false;
#endif


struct conn_req_info * btsrv_get_connreq_info(bt_addr_t *addr)
{
	int i;
    struct conn_req_info *info = NULL;
	for (i = 0; i < BTSRV_CONNREQINFO_NUM; i++) {
		if (btsrv_connreq_info[i].addr_valid) {
			if(!memcmp(btsrv_connreq_info[i].addr.val, addr->val, sizeof(bt_addr_t))){
				info = &(btsrv_connreq_info[i]);
				break;
			}
		}
	}
	return info;
}


static struct conn_req_info * btsrv_add_connreq_info(bt_addr_t *addr,uint8_t *cod)
{
	int i;
    struct conn_req_info *info = NULL;

	for (i = 0; i < BTSRV_CONNREQINFO_NUM; i++) {
		if (btsrv_connreq_info[i].addr_valid == 0) {
			btsrv_connreq_info[i].addr_valid = 1;
			memcpy(&btsrv_connreq_info[i].addr, addr, sizeof(bt_addr_t));
			memcpy(&btsrv_connreq_info[i].dev_class, cod, BTSRV_DEV_CLASS_SIZE);
			info = &(btsrv_connreq_info[i]);
			break;
		}
	}

	return info;
}


static void btsrv_remove_connreq_info(bt_addr_t *addr)
{
	int i;
	for (i = 0; i < BTSRV_CONNREQINFO_NUM; i++) {
		if (btsrv_connreq_info[i].addr_valid) {
			if(!memcmp(btsrv_connreq_info[i].addr.val, addr->val, sizeof(bt_addr_t))){
				btsrv_connreq_info[i].addr_valid = 0;
				memset(&btsrv_connreq_info[i], 0, sizeof(struct conn_req_info));
				break;
			}
		}
	}
}




static void _btsrv_adapter_connected_remote_name_cb(bt_addr_t *addr, uint8_t *name)
{
	uint8_t role = BTSRV_TWS_NONE;
	char addr_str[BT_ADDR_STR_LEN];
	struct btsrv_addr_name *info = NULL;
	uint32_t name_len;

	hostif_bt_addr_to_str(addr, addr_str, BT_ADDR_STR_LEN);
	info = mem_malloc(sizeof(struct btsrv_addr_name));
	if (info == NULL) {
		SYS_LOG_ERR("malloc failed!!\n");
		return;
	}

	memset(info, 0, sizeof(struct btsrv_addr_name));
	memcpy(info->mac.val, addr->val, sizeof(bd_address_t));
	//max len is 248
	if(name[247]){
		name_len = MIN(CONFIG_MAX_REMOTE_BT_NAME_LEN, 248);
	}else{
		name_len = MIN(CONFIG_MAX_REMOTE_BT_NAME_LEN, strlen(name));
	}
	memcpy(info->name, name, name_len);

	role = btsrv_adapter_tws_check_role(addr, info->name);
	SYS_LOG_INF("%s %d %d %s \n", addr_str, role, strlen(info->name), info->name);
	btsrv_event_notify_malloc(MSG_BTSRV_CONNECT, MSG_BTSRV_GET_NAME_FINISH, (uint8_t *)info, sizeof(*info), role);

	if (role == BTSRV_TWS_NONE){
		struct conn_req_info* connreq_info = btsrv_get_connreq_info(addr);
		if (connreq_info){
			btsrv_event_notify_malloc(MSG_BTSRV_CONNECT, MSG_BTSRV_CONNREQ_ADD_MONITOR, (uint8_t *)info, sizeof(*info), role);
		}
	}

	if(info){
		mem_free(info);
	}

}

static bool _btsrv_adapter_connect_req_cb(bt_addr_t *peer, uint8_t *cod)
{
	int i;
	uint8_t role = BTSRV_TWS_NONE;
	struct autoconn_info info[BTSRV_SAVE_AUTOCONN_NUM];
	struct bt_link_cb_param param;

	memset(info, 0, sizeof(info));
	btsrv_connect_get_auto_reconnect_info(info, BTSRV_SAVE_AUTOCONN_NUM);
	for (i = 0; i < BTSRV_SAVE_AUTOCONN_NUM; i++) {
		if (info[i].addr_valid && !memcmp(&info[i].addr, peer, sizeof(bd_address_t))) {
			role = info[i].tws_role;
			break;
		}
	}

	memset(&param, 0, sizeof(param));
	param.link_event = BT_LINK_EV_ACL_CONNECT_REQ;
	param.hdl = 0x0;
	param.addr = (bd_address_t *)peer;
	param.new_dev = (i == BTSRV_SAVE_AUTOCONN_NUM) ? 1 : 0;
	param.is_tws = (role == BTSRV_TWS_NONE) ? 0 : 1;

	btsrv_add_connreq_info(peer,cod);
	
	if (btsrv_adapter_callback(BTSRV_LINK_EVENT, &param)) {
		return false;
	} else {
		return true;
	}
}

static void _btsrv_adapter_connected_cb(struct bt_conn *conn, uint8_t err)
{
	char addr_str[BT_ADDR_STR_LEN];
	bt_addr_t *bt_addr = NULL;
	uint8_t type;

	if (!conn) {
		return;
	}

	type = hostif_bt_conn_get_type(conn);
	if (type != BT_CONN_TYPE_BR && type != BT_CONN_TYPE_BR_SNOOP) {
		return;
	}

	bt_addr = (bt_addr_t *)GET_CONN_BT_ADDR(conn);
	hostif_bt_addr_to_str(bt_addr, addr_str, BT_ADDR_STR_LEN);
	SYS_LOG_INF("%s hdl 0x%x err 0x%x", addr_str, hostif_bt_conn_get_handle(conn), err);

	if (err) {
		btsrv_remove_connreq_info(bt_addr);
		btsrv_event_notify_malloc(MSG_BTSRV_CONNECT, MSG_BTSRV_CONNECTED_FAILED, bt_addr->val, sizeof(bd_address_t), err);
	} else {
		/* In MSG_BTSRV_CONNECTED req high performance is too late,
		 * request in stack rx thread, can't block in callback.
		 * and do release after process MSG_BTSRV_CONNECTED message.
		 */
		btsrv_adapter_callback(BTSRV_REQ_HIGH_PERFORMANCE, NULL);
		/* ref for bt servcie process MSG_BTSRV_CONNECTED,
		 * need unref conn after process MSG_BTSRV_CONNECTED.
		 */
		hostif_bt_conn_ref(conn);

		/* No metter phone/tws/snoop acl link, all can set hfp 1st sync, set as early as possible. */
		btsrv_tws_hfp_set_1st_sync(conn);

		btsrv_event_notify(MSG_BTSRV_CONNECT, MSG_BTSRV_CONNECTED, conn);
		if (!btsrv_is_snoop_conn(conn)) {
		hostif_bt_remote_name_request(bt_addr, _btsrv_adapter_connected_remote_name_cb);
		}
	}
}

static void _btsrv_adapter_disconnected_cb(struct bt_conn *conn, uint8_t reason)
{
	uint8_t type;
	bt_addr_t *bt_addr = NULL;

	if (!conn) {
		return;
	}

	type = hostif_bt_conn_get_type(conn);
	if (type != BT_CONN_TYPE_BR && type != BT_CONN_TYPE_BR_SNOOP) {
		return;
	}
	bt_addr = (bt_addr_t *)GET_CONN_BT_ADDR(conn);
	btsrv_remove_connreq_info(bt_addr);
	btsrv_adapter_callback(BTSRV_REQ_HIGH_PERFORMANCE, NULL);
	SYS_LOG_INF("disconnected_cb hdl 0x%x reason 0x%x", hostif_bt_conn_get_handle(conn), reason);
	btsrv_event_notify_ext(MSG_BTSRV_CONNECT, MSG_BTSRV_DISCONNECTED, conn, reason);
}

static bool _btsrv_adapter_remote_linkkey_miss_cb(struct bt_conn *conn,bt_addr_t *peer)
{
	if (!peer || !conn) {
		return false;
	}

	if (bt_conn_get_type(conn) != BT_CONN_TYPE_BR) {
		return true;
	}

	struct bt_link_cb_param param;
	char addr[BT_ADDR_STR_LEN];

	hostif_bt_addr_to_str((const bt_addr_t *)peer, addr, BT_ADDR_STR_LEN);
	SYS_LOG_INF("addr:%s st:%d\n", addr,btsrv_info->pair_status);
	struct conn_req_info* connreq_info = btsrv_get_connreq_info((bt_addr_t *)GET_CONN_BT_ADDR(conn));

	if(btsrv_rdm_is_security_changed(conn)){
		return true;
	}

	btsrv_event_notify_malloc(MSG_BTSRV_CONNECT, MSG_BTSRV_REMOTE_LINKKEY_MISS, 
							(uint8_t*)peer, sizeof(bd_address_t), 0);

	memset(&param, 0, sizeof(param));
	param.link_event = BT_LINK_EV_KEY_MISS;
	param.hdl = hostif_bt_conn_get_handle(conn);
	param.addr = (bd_address_t *)peer;
	if(connreq_info){
		connreq_info->key_miss = 1;
	}

	if (btsrv_adapter_callback(BTSRV_LINK_EVENT, &param)) {
		return false;
	} else {
		//TODO:move to uplayer
		if(((btsrv_info->pair_status & BT_PAIR_STATUS_PAIR_MODE) == 0) && !btsrv_connect_get_gfp_status()){
	        if(btsrv_adapter_callback(BTSRV_LINK_MISS_UPLAYER_CHECK, peer)) {
				SYS_LOG_INF("uplayer check same dev");
				return true;
			}else {
				SYS_LOG_ERR("Not In Pair mode!");
				return false;
			}
	    }
	    else{
	        return true;
	    }
	}
}

#if defined(CONFIG_BT_SMP) || defined(CONFIG_BT_BREDR)
static void _btsrv_adapter_identity_resolved_cb(struct bt_conn *conn, const bt_addr_le_t *rpa,
			      const bt_addr_le_t *identity)
{
	char addr_identity[BT_ADDR_LE_STR_LEN];
	char addr_rpa[BT_ADDR_LE_STR_LEN];

	hostif_bt_addr_le_to_str(identity, addr_identity, sizeof(addr_identity));
	hostif_bt_addr_le_to_str(rpa, addr_rpa, sizeof(addr_rpa));

	SYS_LOG_INF("resolved_cb %s -> %s", addr_rpa, addr_identity);
}

static void _btsrv_adapter_security_changed_cb(struct bt_conn *conn, bt_security_t level,
				 enum bt_security_err err)
{
	char addr[BT_ADDR_STR_LEN];

	if (!conn || hostif_bt_conn_get_type(conn) != BT_CONN_TYPE_BR) {
		return;
	}

	hostif_bt_addr_to_str((const bt_addr_t *)GET_CONN_BT_ADDR(conn), addr, BT_ADDR_STR_LEN);
	SYS_LOG_INF("addr:%s l:%d e:%d", addr, level, err);
	btsrv_event_notify(MSG_BTSRV_CONNECT, MSG_BTSRV_SECURITY_CHANGED, conn);
}

static void _btsrv_adapter_role_change_cb(struct bt_conn *conn, uint8_t role)
{
	char addr[BT_ADDR_STR_LEN];

	if (!conn || hostif_bt_conn_get_type(conn) != BT_CONN_TYPE_BR) {
		return;
	}

	hostif_bt_addr_to_str((const bt_addr_t *)GET_CONN_BT_ADDR(conn), addr, BT_ADDR_STR_LEN);
	SYS_LOG_INF("role_change_cb %s %d", addr, role);
	btsrv_event_notify_ext(MSG_BTSRV_CONNECT, MSG_BTSRV_ROLE_CHANGE, conn, role);
}

void _btsrv_adapter_mode_change_cb(struct bt_conn *conn, uint8_t mode, uint16_t interval)
{
	struct btsrv_mode_change_param param;

	if (!conn || hostif_bt_conn_get_type(conn) != BT_CONN_TYPE_BR) {
		return;
	}

	SYS_LOG_INF("mode change hdl 0x%x mode %d interval %d", hostif_bt_conn_get_handle(conn), mode, interval);
	param.conn = conn;
	param.mode = mode;
	param.interval = interval;
	btsrv_event_notify_malloc(MSG_BTSRV_CONNECT, MSG_BTSRV_MODE_CHANGE, (uint8_t *)&param, sizeof(param), 0);
}
#endif

static void _btsrv_act_fix_cid_data_cb(struct bt_conn *conn, uint16_t cid, uint8_t *data, uint16_t len)
{
	btsrv_tws_process_act_fix_cid_data(conn, cid, data, len);
}

static struct bt_conn_cb conn_callbacks = {
	.connect_req = _btsrv_adapter_connect_req_cb,
	.connected = _btsrv_adapter_connected_cb,
	.disconnected = _btsrv_adapter_disconnected_cb,
	.remote_linkkey_miss = _btsrv_adapter_remote_linkkey_miss_cb,
#if defined(CONFIG_BT_SMP) || defined(CONFIG_BT_BREDR)
	.identity_resolved = _btsrv_adapter_identity_resolved_cb,
	.security_changed = _btsrv_adapter_security_changed_cb,
	.role_change = _btsrv_adapter_role_change_cb,
	.mode_change = _btsrv_adapter_mode_change_cb,
#endif
	.rx_act_fix_cid_data = _btsrv_act_fix_cid_data_cb,
};

static void _btsrv_adapter_ready(int err)
{
	if (err) {
		SYS_LOG_ERR("Bt init failed %d", err);
		return;
	}

	SYS_LOG_INF("Bt init");
	hostif_bt_conn_cb_register(&conn_callbacks);
	bt_service_set_bt_ready();
//	btsrv_event_notify(MSG_BTSRV_BASE, MSG_BTSRV_READY, NULL);
}

static void btsrv_adapter_start_wait_avrcp_pause_timer(void)
{
	if (thread_timer_is_running(&btsrv_info->wait_disconnect_timer)) {
		return;
	}

	SYS_LOG_INF("");
	thread_timer_start(&btsrv_info->wait_disconnect_timer, CHECK_WAIT_AVRCP_PAUSE_INTERVAL, CHECK_WAIT_DISCONNECT_INTERVAL);
}

static void btsrv_adapter_start_wait_a2dp_connected_timer(void)
{
	if (thread_timer_is_running(&btsrv_info->wait_disconnect_timer)) {
		return;
	}

	SYS_LOG_INF("");
	thread_timer_start(&btsrv_info->wait_disconnect_timer, CHECK_WAIT_A2DP_CONNECTED_INTERVAL, CHECK_WAIT_DISCONNECT_INTERVAL);
}

static void btsrv_adapter_start_wait_disconnect_timer(void)
{
	if (thread_timer_is_running(&btsrv_info->wait_disconnect_timer)) {
		return;
	}

	SYS_LOG_INF("start_wait_disconnect_timer");
	thread_timer_start(&btsrv_info->wait_disconnect_timer, CHECK_WAIT_DISCONNECT_INTERVAL, CHECK_WAIT_DISCONNECT_INTERVAL);
}

static void btsrv_adapter_stop_wait_disconnect_timer(void)
{
	SYS_LOG_INF("stop_wait_disconnect_timer");
	thread_timer_stop(&btsrv_info->wait_disconnect_timer);
}

static void connected_dev_cb_check_wait_disconnect(struct bt_conn *base_conn, uint8_t tws_dev, void *cb_param)
{
	int err;
	int *wait_diconnect_cnt = cb_param;
	uint8_t need_disconnect_prifile = 0;
	uint32_t profile_disconnect_timestamp = 0;

	if (btsrv_rdm_is_wait_to_diconnect(base_conn)) {
		if (btsrv_sniff_in_sniff_mode(base_conn) == false) {

			if (btsrv_rdm_is_a2dp_signal_connected(base_conn) ||
				btsrv_rdm_is_a2dp_connected(base_conn) ||
				btsrv_rdm_is_avrcp_connected(base_conn)) {
				need_disconnect_prifile = 1;
			}

			if ((need_disconnect_prifile) &&
				(btsrv_rdm_get_profile_disconnect_timestamp(base_conn) == 0)) {

				SYS_LOG_DBG("%p, %d", base_conn, os_uptime_get_32());
				hostif_bt_gatt_over_br_disconnect(base_conn);
				btsrv_a2dp_disconnect(base_conn);
				btsrv_avrcp_disconnect(base_conn);
				btsrv_rdm_set_profile_disconnect_timestamp(base_conn, os_uptime_get_32());
			}

			profile_disconnect_timestamp = btsrv_rdm_get_profile_disconnect_timestamp(base_conn);

			SYS_LOG_INF("%p, %d, %d, %d, %d", base_conn, os_uptime_get_32(), !btsrv_rdm_is_a2dp_signal_connected(base_conn),
						!btsrv_rdm_is_a2dp_connected(base_conn), os_uptime_get_32() - profile_disconnect_timestamp);

			/* profile isn't connected,
			 * profile disconnected.
			 * profile disconnect timeout.
			 */
			if ((!need_disconnect_prifile) ||
				(!btsrv_rdm_is_a2dp_signal_connected(base_conn) && !btsrv_rdm_is_a2dp_connected(base_conn)) ||
				((os_uptime_get_32() -profile_disconnect_timestamp) >= CHECK_WAIT_PROFILE_DISCONNECTED_INTERVAL)) {

				btsrv_rdm_set_profile_disconnect_timestamp(base_conn, 0);
				err = btsrv_adapter_do_conn_disconnect(base_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
				if (!err) {
					btsrv_rdm_set_wait_to_diconnect(base_conn, false);
				}
			}else {
				(*wait_diconnect_cnt)++;
			}
		} else {
			hostif_bt_conn_check_exit_sniff(base_conn);
			(*wait_diconnect_cnt)++;
		}
	}
}

static void btsrv_adapter_wait_disconnect_timer_handler(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	int wait_diconnect_cnt = 0;

	btsrv_rdm_get_connected_dev(connected_dev_cb_check_wait_disconnect, &wait_diconnect_cnt);
	if (wait_diconnect_cnt == 0) {
		btsrv_adapter_stop_wait_disconnect_timer();
	}
}

struct btsrv_conn_call_info {
	uint8_t call_busy:1;
	uint8_t sniff_active:1;
	uint32_t conn_rxtx_cnt;
};

static void connected_dev_cb_check_wake_lock(struct bt_conn *base_conn, uint8_t tws_dev, void *cb_param)
{
	struct btsrv_conn_call_info *info = cb_param;

	info->conn_rxtx_cnt += hostif_bt_conn_get_rxtx_cnt(base_conn);
	if (btsrv_rdm_hfp_in_call_state(base_conn)) {
		info->call_busy = 1;
	}

	if ((!btsrv_sniff_in_sniff_mode(base_conn)) && btsrv_sniff_enable()) {
		info->sniff_active = 1;
	}
}

static void btsrv_read_rssi_event_cb(uint8_t status, uint8_t *data, uint16_t len)
{
    struct bt_conn *conn;
    uint16_t handle;
    int8_t rssi;

    if (status) {
        SYS_LOG_ERR("Read rssi status err %d", status);
        return;
    }

    /* Data in struct bt_hci_rp_read_rssi */
    handle = data[1] | (data[2] << 8);
    rssi = data[3];

    conn = btsrv_rdm_find_conn_by_hdl(handle);
    if(conn){
        btsrv_rdm_set_dev_rssi(conn,rssi);
    }
}


static void btsrv_read_link_quality_event_cb(uint8_t status, uint8_t *data, uint16_t len)
{
    struct bt_conn *conn;
	uint16_t handle;
	uint8_t   quality;

	if (status) {
		SYS_LOG_ERR("Read quality status err %d", status);
		return;
	}

	/* Data in struct bt_hci_rp_read_link_quality */
	handle = data[1] | (data[2] << 8);
	quality = data[3];

    conn = btsrv_rdm_find_conn_by_hdl(handle);
    if(conn){
        btsrv_rdm_set_dev_link_quality(conn,quality);
    }
}

static void btsrv_adapter_wake_lock_timer_handler(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	static uint32_t conn_idle_start_time;
	static uint32_t call_idle_start_time;
	static uint32_t pre_conn_rxtx_cnt;
	struct btsrv_conn_call_info info;
	uint32_t curr_time;

	memset(&info, 0, sizeof(info));
	btsrv_rdm_get_connected_dev(connected_dev_cb_check_wake_lock, &info);

	curr_time = os_uptime_get_32();

	if ((pre_conn_rxtx_cnt != info.conn_rxtx_cnt) || info.sniff_active) {
		conn_idle_start_time = curr_time;
		pre_conn_rxtx_cnt = info.conn_rxtx_cnt;
		btsrv_adapter_set_clear_wake_lock(BTSRV_WAKE_LOCK_CONN_BUSY, 1);
		if (btsrv_info->link_idle){
			SYS_LOG_INF("LINK BUSY");
		}
		btsrv_info->link_idle = 0;
	} else {
		if ((curr_time - conn_idle_start_time) > WAKE_LOCK_IDLE_TIMEOUT) {
			btsrv_adapter_set_clear_wake_lock(BTSRV_WAKE_LOCK_CONN_BUSY, 0);
			if (!btsrv_info->link_idle){
				SYS_LOG_INF("LINK IDLE");
			}
			btsrv_info->link_idle = 1;
		}
	}

	if (info.call_busy) {
		call_idle_start_time = curr_time;
		btsrv_adapter_set_clear_wake_lock(BTSRV_WAKE_LOCK_CALL_BUSY, 1);
	} else {
		if ((curr_time - call_idle_start_time) > WAKE_LOCK_IDLE_TIMEOUT) {
			btsrv_adapter_set_clear_wake_lock(BTSRV_WAKE_LOCK_CALL_BUSY, 0);
		}
	}
}

static void btsrv_adapter_monitor_link_quality_timer_handler(struct thread_timer *ttimer, void *expiry_fn_arg)
{
    struct bt_conn *active_conn;
	struct btsrv_info_t * info = btsrv_adapter_get_context();

    active_conn = btsrv_rdm_a2dp_get_active_dev();
    if(active_conn){
    #ifdef CONFIG_LOGSRV_BT_DEBUG
        if (btsrv_rdm_is_a2dp_stream_open(active_conn) || true == btsrv_link_quality_always) {
    #else
        if (btsrv_rdm_is_a2dp_stream_open(active_conn)) {
    #endif
            info->quality_time++;
            hostif_bt_br_read_rssi(active_conn,btsrv_read_rssi_event_cb);
            if(info->quality_time >= 5){
                info->quality_time = 0;
                hostif_bt_br_read_link_quality(active_conn,btsrv_read_link_quality_event_cb);
            }
        }
    }
}

void btsrv_get_local_mac(bd_address_t *addr)
{
	struct btsrv_info_t * info = btsrv_adapter_get_context();
	if(info){
		memcpy(addr, info->device_addr, BT_MAC_LEN);
	} 
	else {
		memset(addr, 0, BT_MAC_LEN);
	}
}


int btsrv_adapter_set_power_off(void)
{
    btsrv_info->power_off = 1;
    btif_tws_set_state(TWS_STATE_POWER_OFF);
    return 0;
}

int feature_fixed_chan_cb_func(uint16_t handle)
{
	/*ios doesn't report fixed_chan,
	 * otherwise there will has any display problems
	 */
	if (btif_br_is_ios_dev(handle)) {
		SYS_LOG_INF("ios");
		return 0;
	}
	return 1;
}

struct btsrv_info_t *btsrv_adapter_init(btsrv_callback cb)
{
	int err;
	char addr_str[BT_ADDR_STR_LEN];
	uint8_t share_mac[BT_MAC_LEN];
	uint8_t i, value;

	memset(&btsrv_btinfo, 0, sizeof(struct btsrv_info_t));

	btsrv_info = &btsrv_btinfo;
	btsrv_info->callback = cb;
	btsrv_info->allow_sco_connect = 1;
	btsrv_info->power_off = 0;

	hostif_bt_l2cap_br_reg_feature_fixed_chan_cb_func(feature_fixed_chan_cb_func);

	btsrv_storage_init();
	btsrv_rdm_init();
	btsrv_connect_init();
	btsrv_scan_init();
	btsrv_link_adjust_init();
	btsrv_sniff_init();
	err = hostif_bt_enable(_btsrv_adapter_ready);

	if (err) {
		SYS_LOG_INF("Bt init failed %d", err);
		goto err;
	}


	if (property_get_string(CFG_BT_NAME, btsrv_info->device_name, sizeof(btsrv_info->device_name), CONFIG_BT_DEVICE_NAME) <= 0) {
		SYS_LOG_WRN("failed to get bt name");
	} else {
		SYS_LOG_INF("bt name: %s", (char *)btsrv_info->device_name);
	}

#ifdef CONFIG_PROPERTY
	if (property_get_byte_array(CFG_BT_MAC, btsrv_info->device_addr, sizeof(btsrv_info->device_addr), NULL)) {
		SYS_LOG_WRN("failed to get BT_MAC");
	} else
#endif
	{
		/* Like stack, low mac address save in low memory address */
		for (i=0; i<3; i++) {
			value = btsrv_info->device_addr[i];
			btsrv_info->device_addr[i] = btsrv_info->device_addr[5 -i];
			btsrv_info->device_addr[5 -i] = value;
		}

		memset(addr_str, 0, sizeof(addr_str));
		hostif_bt_addr_to_str((bt_addr_t *)btsrv_info->device_addr, addr_str, BT_ADDR_STR_LEN);
		SYS_LOG_INF("BT_MAC: %s", addr_str);

#ifdef CONFIG_USE_SHARE_TWS_MAC
		if ((btsrv_property_get(CFG_BT_SHARE_TWS_MAC, share_mac, BT_MAC_LEN) == BT_MAC_LEN) &&
			(hostif_bt_addr_cmp((const bt_addr_t *)share_mac, (const bt_addr_t *)BT_ADDR_ANY))) {
			hostif_bt_set_share_bd_addr(true, share_mac);
			memcpy(btsrv_info->device_addr, share_mac, BT_MAC_LEN);
			btsrv_info->share_mac_dev = 1;
			hostif_bt_vs_write_bt_addr((bt_addr_t *)btsrv_info->device_addr);
			hostif_bt_addr_to_str((bt_addr_t *)btsrv_info->device_addr, addr_str, BT_ADDR_STR_LEN);
			SYS_LOG_INF("BT_SHARE_MAC: %s", addr_str);
		} else {
			/* Just for clear share dev linkkey */
			memset(share_mac, 0, sizeof(share_mac));
			hostif_bt_set_share_bd_addr(false, share_mac);
		}
#else
 			/* Just for clear share dev linkkey */
			memset(share_mac, 0, sizeof(share_mac));
			hostif_bt_set_share_bd_addr(false, share_mac);    		
#endif		
	}

	thread_timer_init(&btsrv_info->wait_disconnect_timer, btsrv_adapter_wait_disconnect_timer_handler, NULL);
	thread_timer_init(&btsrv_info->wake_lock_timer, btsrv_adapter_wake_lock_timer_handler, NULL);
	thread_timer_init(&btsrv_info->monitor_link_quality_timer, btsrv_adapter_monitor_link_quality_timer_handler, NULL);
	thread_timer_start(&btsrv_info->wake_lock_timer, WAKE_LOCK_TIMER_INTERVAL, WAKE_LOCK_TIMER_INTERVAL);

    btsrv_event_notify(MSG_BTSRV_BASE, MSG_BTSRV_READY, NULL);

	return btsrv_info;

err:
	return NULL;
}



struct btsrv_info_t *btsrv_adapter_get_context(void)
{
    return btsrv_info;
}

int btsrv_adapter_update_pair_state(uint8_t pair_state)
{
    btsrv_info->pair_status = pair_state;

    if((btsrv_info->pair_status & BT_PAIR_STATUS_WAIT_CONNECT) != 0){
		btsrv_adapter_set_clear_wake_lock(BTSRV_WAKE_LOCK_WAIT_CONNECT,1);
    }
    else{
		btsrv_adapter_set_clear_wake_lock(BTSRV_WAKE_LOCK_WAIT_CONNECT,0);
    }

    if((btsrv_info->pair_status & BT_PAIR_STATUS_PAIR_MODE) != 0){
        btsrv_adapter_set_clear_wake_lock(BTSRV_WAKE_LOCK_PAIR_MODE,1);
    }
    else{
        btsrv_adapter_set_clear_wake_lock(BTSRV_WAKE_LOCK_PAIR_MODE,0);
    }

    if((btsrv_info->pair_status & BT_PAIR_STATUS_RECONNECT) != 0){
	    btsrv_adapter_set_clear_wake_lock(BTSRV_WAKE_LOCK_RECONNECT, 1);
    }
	else{
	    btsrv_adapter_set_clear_wake_lock(BTSRV_WAKE_LOCK_RECONNECT, 0);
	}

    SYS_LOG_INF("pair status:0x%x, lock:0x%x",
        btsrv_info->pair_status,
		btsrv_info->bt_wake_lock);
	return 0;
}

uint8_t btsrv_adapter_get_pair_state(void)
{
    return btsrv_info->pair_status;
}

void btsrv_adapter_set_pair_mode_state(bool enable, uint32_t duration_ms)
{
    if(enable){
        btsrv_info->pair_status |= BT_PAIR_STATUS_PAIR_MODE;
        if (duration_ms > 0) {
            btsrv_adapter_set_clear_wake_lock(BTSRV_WAKE_LOCK_PAIR_MODE,1);
        }
    }
	else{
        btsrv_info->pair_status &= (~BT_PAIR_STATUS_PAIR_MODE);
        btsrv_adapter_set_clear_wake_lock(BTSRV_WAKE_LOCK_PAIR_MODE,0);
	}
    SYS_LOG_INF("enable:%d status:0x%x",enable,btsrv_info->pair_status);
}

void btsrv_adapter_set_wait_connect_state(bool enable, uint32_t duration_ms)
{
    if(enable){
        btsrv_info->pair_status |= BT_PAIR_STATUS_WAIT_CONNECT;
        if (duration_ms > 0) {
            btsrv_adapter_set_clear_wake_lock(BTSRV_WAKE_LOCK_WAIT_CONNECT,1);
        }
    }
	else{
        btsrv_info->pair_status &= (~BT_PAIR_STATUS_WAIT_CONNECT);
        btsrv_adapter_set_clear_wake_lock(BTSRV_WAKE_LOCK_WAIT_CONNECT,0);

	}
    SYS_LOG_INF("enable:%d status:0x%x",enable,btsrv_info->pair_status);
}

void btsrv_adapter_set_reconnect_state(bool enable)
{
    if(enable){
        btsrv_info->pair_status |= BT_PAIR_STATUS_RECONNECT;
        btsrv_adapter_set_clear_wake_lock(BTSRV_WAKE_LOCK_RECONNECT,1);
    }
	else{
        btsrv_info->pair_status &= (~BT_PAIR_STATUS_RECONNECT);
        btsrv_adapter_set_clear_wake_lock(BTSRV_WAKE_LOCK_RECONNECT,0);
	}
    SYS_LOG_INF("enable:%d status:0x%x",enable,btsrv_info->pair_status);
}

void btsrv_adapter_set_tws_search_state(bool enable)
{
    if(enable){
        btsrv_info->pair_status |= BT_PAIR_STATUS_TWS_SEARCH;
    }
	else{
        btsrv_info->pair_status &= (~BT_PAIR_STATUS_TWS_SEARCH);
	}

    SYS_LOG_INF("enable:%d pair status:0x%x",enable,btsrv_info->pair_status);
}

int btsrv_adapter_set_config_info(void *param)
{
	int ret = -EIO;

	if (btsrv_info) {
		memcpy(&btsrv_info->cfg, param, sizeof(btsrv_info->cfg));
		SYS_LOG_INF("btsrv config info: %d, %d, %d", btsrv_info->cfg.max_conn_num,
						btsrv_info->cfg.max_phone_num, btsrv_info->cfg.pts_test_mode);
		ret = 0;
	}

	return ret;
}

int btsrv_adapter_set_pts_config(bool enable)
{
    btsrv_info->cfg.pts_test_mode = enable;

    hostif_bt_set_pts_enable(enable);

    return 0;
}

int btsrv_adapter_link_quality_always(bool enable)
{
#ifdef CONFIG_LOGSRV_BT_DEBUG
	btsrv_link_quality_always = enable;
#endif
	return 0;
}

int btsrv_adapter_get_type_by_handle(uint16_t handle)
{
	return hostif_bt_conn_get_type_by_handle(handle);
}

int btsrv_adapter_get_role(uint16_t handle)
{
	return hostif_bt_conn_get_role_by_handle(handle);
}

void btsrv_adapter_set_clear_wake_lock(uint16_t wake_lock, uint8_t set)
{
	if (set) {
		btsrv_info->bt_wake_lock |= wake_lock;
	} else {
		btsrv_info->bt_wake_lock &= (~wake_lock);
	}
}

bool btsrv_adapter_check_wake_lock_bit(uint16_t wake_lock)
{
	return (btsrv_info->bt_wake_lock & wake_lock) ? true : false;
}

int btsrv_adapter_get_wake_lock(void)
{
	return (btsrv_info->bt_wake_lock) ? 1 : 0;
}

int btsrv_adapter_get_link_idle(void)
{
	return (btsrv_info->link_idle) ? 1 : 0;
}


static void btsrv_adapter_discovery_result(struct bt_br_discovery_result *result)
{
	struct btsrv_discover_result cb_result;

	if (!btsrv_discover_cb) {
		return;
	}

	memset(&cb_result, 0, sizeof(cb_result));
	if (result) {
		memcpy(&cb_result.addr, &result->addr, sizeof(bd_address_t));
		cb_result.rssi = result->rssi;
		if (result->name) {
			cb_result.name = result->name;
			cb_result.len = result->len;
			memcpy(cb_result.device_id, result->device_id, sizeof(cb_result.device_id));
		}
	} else {
		cb_result.discover_finish = 1;
	}

	btsrv_discover_cb(&cb_result);
	if (cb_result.discover_finish) {
		btsrv_discover_cb = NULL;
	}
}

int btsrv_adapter_start_discover(struct btsrv_discover_param *param)
{
	int ret;
	struct bt_br_discovery_param discovery_param;

	btsrv_discover_cb = param->cb;
	discovery_param.length = param->length;
	discovery_param.num_responses = param->num_responses;
	discovery_param.limited = false;
	ret = hostif_bt_br_discovery_start((const struct bt_br_discovery_param *)&discovery_param,
										btsrv_adapter_discovery_result);
	if (ret) {
		btsrv_discover_cb = NULL;
	}

	return ret;
}

int btsrv_adapter_stop_discover(void)
{
	hostif_bt_br_discovery_stop();
	return 0;
}

int btsrv_adapter_connect(bd_address_t *addr)
{
	struct bt_conn *conn = hostif_bt_conn_create_br((const bt_addr_t *)addr, BT_BR_CONN_PARAM_DEFAULT);

	if (!conn) {
		SYS_LOG_ERR("Connection failed");
	} else {
		SYS_LOG_INF("Connection pending");
		/* unref connection obj in advance as app user */
		hostif_bt_conn_unref(conn);
	}

	return 0;
}

int btsrv_adapter_disconnect_by_priority(void)
{
	struct bt_conn *conn = NULL;
	conn = btsrv_rdm_get_nonactived_phone();

    if(conn){
		btsrv_adapter_disconnect(conn);
    }
	return 0;
}

int btsrv_adapter_check_cancal_connect(bd_address_t *addr)
{
	int err;
	struct bt_conn *conn;

	conn = hostif_bt_conn_br_acl_connecting((const bt_addr_t *)addr);
	if (conn) {
		/* In connecting, hostif_bt_conn_disconnect will cancal connect */
		err = hostif_bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
		hostif_bt_conn_unref(conn);
		SYS_LOG_INF("Cancal connect %d", err);
	}

	return 0;
}

int btsrv_adapter_do_conn_disconnect(struct bt_conn *conn, uint8_t reason)
{
	int err = 0;

	if (hostif_bt_conn_ref(conn) == NULL) {
		SYS_LOG_INF("Conn already release");
		return err;
	}

	err = hostif_bt_conn_disconnect(conn, reason);
	if (err) {
		SYS_LOG_ERR("Disconnect failed %d", err);
	}

	hostif_bt_conn_unref(conn);
	return err;
}

int btsrv_adapter_disconnect(struct bt_conn *conn)
{
	int err = 0;
	uint16_t hdl;

	if (btsrv_rdm_get_tws_role(conn) == BTSRV_TWS_NONE) {

		if(btsrv_rdm_get_avrcp_state(conn) && btsrv_rdm_is_a2dp_stream_open(conn)){
            hdl = hostif_bt_conn_get_handle(conn);
            btif_avrcp_send_command_by_hdl(hdl,BTSRV_AVRCP_CMD_PAUSE);
            btsrv_rdm_set_wait_to_diconnect(conn, true);
            btsrv_adapter_start_wait_avrcp_pause_timer();
            return err;
        }

        if(btsrv_rdm_is_a2dp_signal_connected(conn) &&
            !btsrv_rdm_is_a2dp_connected(conn)){
            btsrv_rdm_set_wait_to_diconnect(conn, true);
            btsrv_adapter_start_wait_a2dp_connected_timer();
            return err;
        }
    }

	hostif_bt_conn_check_exit_sniff(conn);
	btsrv_rdm_set_wait_to_diconnect(conn, true);
	btsrv_adapter_start_wait_disconnect_timer();

	return err;
}

void btsrv_adapter_run(void)
{
	btsrv_info->running = 1;
}

void btsrv_adapter_allow_sco_connect(bool allow)
{
	btsrv_info->allow_sco_connect = allow;
}


static void btsrv_adapter_set_leaudio_connected(bool connected)
{
	uint8_t cis_link_background = 0;

	btsrv_info->lea_connected = connected?1:0;

	if (!btsrv_info->lea_is_foreground_dev){
		if (connected){
			cis_link_background = 1;
		}
	}

	SYS_LOG_INF("%d, %d", connected, cis_link_background);
	hostif_bt_vs_set_cis_link_background(cis_link_background);
	btsrv_scan_update_mode(true);
}


int btsrv_adapter_leaudio_is_connected(void)
{
	return btsrv_info->lea_connected?1:0;
}


int btsrv_adapter_set_leaudio_foreground_dev(bool enable)
{
	uint8_t cis_link_background = 0;

	btsrv_info->lea_is_foreground_dev = enable?1:0;

	if (enable)
	{
		btsrv_tws_set_leaudio_active(1);
	}else{
		btsrv_tws_set_leaudio_active(0);
		if (btsrv_info->lea_connected){
			cis_link_background = 1;
		}
	}

	SYS_LOG_INF("%d, %d", enable, cis_link_background);
	hostif_bt_vs_set_cis_link_background(cis_link_background);
	
	return 0;
}

int btsrv_adapter_leaudio_is_foreground_dev(void)
{
	return btsrv_info->lea_is_foreground_dev?1:0;
}



int btsrv_adapter_stop(void)
{
	if (!btsrv_info /*|| btsrv_info->init_state == BT_INIT_NONE*/) {
		btsrv_info = NULL;
		return 0;
	}

	thread_timer_stop(&btsrv_info->wake_lock_timer);
    btsrv_adapter_stop_wait_disconnect_timer();
    btsrv_adapter_stop_monitor_link_quality_timer();
	btsrv_info->running = 0;
	/* TODO: Call btsrv connect to disconnect all connection,
	 * other deinit must wait all disconnect finish.
	 */

	/* Wait TODO:  */
	btsrv_sniff_deinit();
	btsrv_link_adjust_deinit();
	btsrv_scan_deinit();
	btsrv_a2dp_deinit();
	btsrv_avrcp_deinit();
	btsrv_connect_deinit();
	btsrv_rdm_deinit();
	hostif_bt_disable();
//	btsrv_info->init_state = BT_INIT_NONE;
	btsrv_info = NULL;
	return 0;
}

int btsrv_adapter_callback(btsrv_event_e event, void *param)
{
	if (btsrv_info && btsrv_info->callback) {
		//SYS_LOG_INF("%d \n",event);
		return btsrv_info->callback(event, param);
	}

	return 0;
}

static void btsrv_active_disconnect(bd_address_t *addr)
{
	struct bt_conn *conn;

	conn = btsrv_rdm_find_conn_by_addr(addr);
	if (!conn) {
		SYS_LOG_INF("Device not connected!\n");
		return;
	}

	btsrv_adapter_disconnect(conn);
}

void btsrv_adapter_start_monitor_link_quality_timer(void)
{
    btsrv_info->quality_time = 0;
    if(!thread_timer_is_running(&btsrv_info->monitor_link_quality_timer)){
        thread_timer_start(&btsrv_info->monitor_link_quality_timer, 100, 100);
    }
}

void btsrv_adapter_stop_monitor_link_quality_timer(void)
{
    if(thread_timer_is_running(&btsrv_info->monitor_link_quality_timer)){
        thread_timer_stop(&btsrv_info->monitor_link_quality_timer);
    }
}

void btsrv_adapter_update_qos(void *qos)
{
    if(qos){
        hostif_bt_conn_qos_setup(qos);
    }
}

int btsrv_adapter_process(struct app_msg *msg)
{
	int ret = 0; 
	struct bt_scan_policy *policy;
	
	switch (_btsrv_get_msg_param_cmd(msg)) {
	case MSG_BTSRV_READY:
		btsrv_adapter_callback(BTSRV_READY, NULL);
		break;
	case MSG_BTSRV_REQ_FLUSH_NVRAM:
		btsrv_adapter_callback(BTSRV_REQ_FLUSH_PROPERTY, msg->ptr);
		break;
	case MSG_BTSRV_DISCONNECTED_REASON:
		btsrv_adapter_callback(BTSRV_DISCONNECTED_REASON, msg->ptr);
		break;
    case MSG_BTSRV_UPDATE_SCAN_MODE:
        btsrv_scan_update_mode(false);
	    break;
	case MSG_BTSRV_SET_USER_VISUAL:
		btsrv_scan_set_user_visual(msg->ptr);
		break;
	case MSG_BTSRV_SET_SCAN_POLICY:
		//TODO: use hci vendor cmd to set policy
		policy = (struct bt_scan_policy *)msg->ptr;
		SYS_LOG_INF("scan policy %d %d\n",policy->scan_type,policy->scan_policy_mode);
		ctrl_set_scan_policy(policy->scan_type,policy->scan_policy_mode);
		break;
	case MSG_BTSRV_ALLOW_SCO_CONNECT:
		btsrv_adapter_allow_sco_connect(_btsrv_get_msg_param_ptr(msg));
		break;
	case MSG_BTSRV_CONNECT_TO:
		btsrv_adapter_connect(msg->ptr);
		break;
	case MSG_BTSRV_DISCONNECT:
		btsrv_active_disconnect(msg->ptr);
		break;
	case MSG_BTSRV_DISCONNECT_DEV_BY_PRIORITY:
        btsrv_adapter_disconnect_by_priority();
		break;
	case MSG_BTSRV_DUMP_INFO:
		btsrv_rdm_dump_info();
		btsrv_connect_dump_info();
		btsrv_scan_dump_info();
#ifdef CONFIG_SUPPORT_TWS
		btsrv_tws_dump_info();
#endif
		break;
	case MSG_BTSRV_UPDATE_QOS:
	    btsrv_adapter_update_qos(msg->ptr);
	    break;
	case MSG_BTSRV_AUTO_RECONNECT_COMPLETE:
		btsrv_adapter_callback(BTSRV_AUTO_RECONNECT_COMPLETE, msg->ptr);
		break;
	case MSG_BTSRV_SET_LEAUDIO_CONNECTED:
		btsrv_adapter_set_leaudio_connected(_btsrv_get_msg_param_ptr(msg));
		break;
	case MSG_BTSRV_UPDATE_DEVICE_NAME:
	    hostif_bt_update_br_name();
		break;	
	default:
		break;
	}

	return ret;
}
