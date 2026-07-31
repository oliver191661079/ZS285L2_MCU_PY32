/*
 * Copyright (c) 2016 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt srv api interface
 */

#define SYS_LOG_DOMAIN "btif_base"
#include "btsrv_os_common.h"
#include "btsrv_inner.h"

static uint8_t autoconn_info_need_update = 1;

int btif_base_register_processer(void)
{
	int ret = 0;

	ret |= btsrv_register_msg_processer(MSG_BTSRV_BASE, &btsrv_adapter_process);
	ret |= btsrv_register_msg_processer(MSG_BTSRV_CONNECT, &btsrv_connect_process);
	return ret;
}

int btif_start(btsrv_callback cb, uint32_t classOfDevice, uint16_t *did)
{
	struct app_msg msg = {0};

	if (!srv_manager_check_service_is_actived(BLUETOOTH_SERVICE_NAME)) {
		if (srv_manager_active_service(BLUETOOTH_SERVICE_NAME)) {
			SYS_LOG_DBG("btif_start ok\n");
		} else {
			SYS_LOG_ERR("btif_start failed\n");
			return -ESRCH;
		}
	}
	msg.type = MSG_INIT_APP;
	msg.ptr = cb;

	if (classOfDevice) {
		hostif_bt_init_class(classOfDevice);
	}

	if (did) {
		hostif_bt_init_device_id(did);
	}
    SYS_LOG_INF("btif_start:\n");
	return !send_async_msg(BLUETOOTH_SERVICE_NAME, &msg);
}

int btif_stop(void)
{
	int ret = 0;

	if (!srv_manager_check_service_is_actived(BLUETOOTH_SERVICE_NAME)) {
		SYS_LOG_ERR("btif_stop failed\n");
		ret = -ESRCH;
		goto exit;
	}

	if (!srv_manager_exit_service(BLUETOOTH_SERVICE_NAME)) {
		ret = -ETIMEDOUT;
		goto exit;
	}

	SYS_LOG_DBG("btif_stop success!\n");
exit:
	return ret;
}

int btif_base_set_power_off(void)
{
	int ret, flags;

	flags = btsrv_set_negative_prio();
	ret = btsrv_adapter_set_power_off();
	btsrv_revert_prio(flags);

	return ret;
}

int btif_base_set_config_info(void *param)
{
	int ret, flags;

	flags = btsrv_set_negative_prio();
	ret = btsrv_adapter_set_config_info(param);
	btsrv_revert_prio(flags);

	return ret;
}

int btif_base_get_wake_lock(void)
{
	int ret, flags;

	flags = btsrv_set_negative_prio();
	ret = btsrv_adapter_get_wake_lock();
	btsrv_revert_prio(flags);

	return ret;
}


int btif_base_get_link_idle(void)
{
	return btsrv_adapter_get_link_idle();
}


int btif_br_update_pair_status(uint8_t state)
{
	int ret, flags;

	flags = btsrv_set_negative_prio();
    ret = btsrv_adapter_update_pair_state(state);
	btsrv_revert_prio(flags);
	return ret;
}	

int btif_br_set_pair_mode_status(uint8_t enable, uint32_t duration_ms)
{
	int flags;

	flags = btsrv_set_negative_prio();
    btsrv_adapter_set_pair_mode_state(enable, duration_ms);
	btsrv_revert_prio(flags);
	return 0;
}

int btif_br_set_wait_connect_status(uint8_t enable, uint32_t duration_ms)
{
	int flags;

	flags = btsrv_set_negative_prio();
    btsrv_adapter_set_wait_connect_state(enable, duration_ms);
	btsrv_revert_prio(flags);
	return 0;
}

int btif_br_set_tws_search_status(uint8_t enable)
{
	int flags;

	flags = btsrv_set_negative_prio();
    btsrv_adapter_set_tws_search_state(enable);
	btsrv_revert_prio(flags);
	return 0;
}


int btif_br_get_pair_status(uint8_t *pair_status)
{
	int flags;

	flags = btsrv_set_negative_prio();
	*pair_status = btsrv_adapter_get_pair_state();
	btsrv_revert_prio(flags);

	return 0;
}

int btif_br_start_discover(struct btsrv_discover_param *param)
{
	int ret, flags;

	flags = btsrv_set_negative_prio();
	ret = btsrv_adapter_start_discover(param);
	btsrv_revert_prio(flags);

	return ret;
}

int btif_br_stop_discover(void)
{
	int ret, flags;

	flags = btsrv_set_negative_prio();
	ret = btsrv_adapter_stop_discover();
	btsrv_revert_prio(flags);

	return ret;
}

/* Mac low address store in low memory address */
int btif_br_connect(bd_address_t *bd)
{
	return btsrv_function_call_malloc(MSG_BTSRV_BASE, MSG_BTSRV_CONNECT_TO, (void *)bd, sizeof(bd_address_t), 0);
}

int btif_br_disconnect(bd_address_t *bd)
{
	return btsrv_function_call_malloc(MSG_BTSRV_BASE, MSG_BTSRV_DISCONNECT, (void *)bd, sizeof(bd_address_t), 0);
}

void btif_br_set_gfp_mode(bool valid)
{
    int flags;

    flags = btsrv_set_negative_prio();
    btsrv_connect_set_gfp_status(valid);
    btsrv_scan_update_mode(true);
    btsrv_revert_prio(flags);
}


int btif_br_set_scan_param(struct bt_scan_parameter *param)
{
    int ret, flags;

    flags = btsrv_set_negative_prio();
    ret = btsrv_scan_set_param(param);
    btsrv_revert_prio(flags);
    
    return ret;
}

void btif_br_set_gfp_scan_mode(bool valid)
{
    int flags;

    flags = btsrv_set_negative_prio();
    btsrv_connect_set_gfp_status(valid);
    btsrv_scan_update_mode(true);
    btsrv_revert_prio(flags);
}

int btif_br_update_scan_mode(void)
{
    return btsrv_function_call(MSG_BTSRV_BASE, MSG_BTSRV_UPDATE_SCAN_MODE, NULL);
}

int btif_br_set_scan_policy(struct bt_scan_policy *policy)
{
    return btsrv_function_call_malloc(MSG_BTSRV_BASE, MSG_BTSRV_SET_SCAN_POLICY, (void *)policy, sizeof(struct bt_scan_policy), 0);
}

int btif_br_get_scan_visual(uint8_t *visual_mode)
{
	int flags;

	flags = btsrv_set_negative_prio();
	*visual_mode = btsrv_scan_get_visual();
	btsrv_revert_prio(flags);

	return 0;
}

int btif_br_get_scan_mode(uint8_t *scan_mode)
{
	int flags;

	flags = btsrv_set_negative_prio();
	*scan_mode = btsrv_scan_get_mode();
	btsrv_revert_prio(flags);

	return 0;
}

int btif_br_set_user_visual(struct bt_set_user_visual *user_visual)
{
    return btsrv_function_call_malloc(MSG_BTSRV_BASE, MSG_BTSRV_SET_USER_VISUAL, (void *)user_visual, sizeof(struct bt_set_user_visual), 0);
}

int btif_br_get_auto_reconnect_info(struct autoconn_info *info, uint8_t max_cnt)
{
	int ret, flags;

	flags = btsrv_set_negative_prio();
	ret = btsrv_connect_get_auto_reconnect_info(info, max_cnt);
	btsrv_revert_prio(flags);

	return ret;
}

int btif_br_is_reconnect_info_empty(void)
{
	int ret, flags;

	flags = btsrv_set_negative_prio();
	ret = btsrv_connect_is_reconnect_info_empty();
	btsrv_revert_prio(flags);

	return ret;
}

bool btif_br_check_tws_paired_info(void)
{
	int flags;
    bool ret = false;

	flags = btsrv_set_negative_prio();
	if(btsrv_connect_get_tws_paired_info() != NULL){
		ret = true;
	}
	btsrv_revert_prio(flags);

	return ret;
}

bool btif_br_check_phone_paired_info(void)
{
	int flags;
    bool ret = false;

	flags = btsrv_set_negative_prio();
	if(btsrv_connect_get_phone_paired_info() != NULL){
		ret = true;
	}
	btsrv_revert_prio(flags);

	return ret;
}

void btif_br_set_auto_reconnect_info(struct autoconn_info *info, uint8_t max_cnt)
{
	int flags;

	flags = btsrv_set_negative_prio();
	btsrv_connect_set_auto_reconnect_info(info, max_cnt,AUTOCONN_INFO_ALL);
	btsrv_revert_prio(flags);
}

void btif_br_set_autoconn_info_need_update(uint8_t need_update)
{
	autoconn_info_need_update = need_update;
}

uint8_t btif_br_get_autoconn_info_need_update(void)
{
	return autoconn_info_need_update;
}

void btif_autoconn_info_active_clear_check(void)
{
	btsrv_autoconn_info_active_clear_check();
}

bool btif_br_is_auto_reconnect_tws_first(uint8_t tws_role)
{
    return btsrv_autoconn_is_tws_pair_first(tws_role);
}

bool btif_br_is_ios_pc(uint16_t handle)
{
	struct bt_conn * conn = btsrv_rdm_find_conn_by_hdl(handle);

	if (conn && btsrv_rdm_is_ios_pc(conn))
	{
		return true;
	}
	return false;
}

bool btif_br_is_ios_dev(uint16_t handle)
{
	struct bt_conn * conn = btsrv_rdm_find_conn_by_hdl(handle);

	if (conn && btsrv_rdm_is_ios_dev(conn))
	{
		return true;
	}
	return false;
}

int btif_br_auto_reconnect(struct bt_set_autoconn *param)
{
	return btsrv_function_call_malloc(MSG_BTSRV_CONNECT, MSG_BTSRV_AUTO_RECONNECT, (void *)param, sizeof(struct bt_set_autoconn), 0);
}

int btif_br_auto_reconnect_stop(int mode)
{
	return btsrv_function_call(MSG_BTSRV_CONNECT, MSG_BTSRV_AUTO_RECONNECT_STOP, (void *)mode);
}

int btif_br_auto_reconnect_stop_device(bd_address_t *bd)
{
	return btsrv_function_call_malloc(MSG_BTSRV_CONNECT, MSG_BTSRV_AUTO_RECONNECT_STOP_DEVICE, (uint8_t *)bd, sizeof(bd_address_t),0);
}

int btif_br_auto_reconnect_stop_except_device(bd_address_t *bd)
{
	return btsrv_function_call_malloc(MSG_BTSRV_CONNECT, MSG_BTSRV_AUTO_RECONNECT_STOP_EXCEPT_DEVICE, (uint8_t *)bd, sizeof(bd_address_t),0);
}

int btif_br_auto_reconnect_connection_cancel_device(void)
{
	return btsrv_function_call_malloc(MSG_BTSRV_CONNECT, MSG_BTSRV_AUTO_RECONNECT_CONNECTION_CANCEL_DEVICE, NULL, 0, 0);
}

bool btif_br_is_auto_reconnect_runing(void)
{
	return btsrv_autoconn_is_runing();
}

bool btif_br_is_auto_reconnect_phone_first(void)
{
	return btsrv_autoconn_is_phone_first_reconnect();
}

int btif_br_clear_list(int mode)
{
	return btsrv_function_call(MSG_BTSRV_CONNECT, MSG_BTSRV_CLEAR_LIST_CMD, (void *)mode);
}

int btif_br_clear_share_tws(void)
{
	return btsrv_function_call(MSG_BTSRV_CONNECT, MSG_BTSRV_CLEAR_SHARE_TWS, NULL);
}

bool btif_br_check_share_tws(void)
{
	return btsrv_connect_check_share_tws();
}

int btif_br_disconnect_device(int disconnect_mode)
{
	return btsrv_function_call(MSG_BTSRV_CONNECT, MSG_BTSRV_DISCONNECT_DEVICE, (void *)disconnect_mode);
}

int btif_br_allow_sco_connect(bool allowed)
{
	return btsrv_function_call(MSG_BTSRV_BASE, MSG_BTSRV_ALLOW_SCO_CONNECT, (void *)allowed);
}

/* Device include phone and tws device */
int btif_br_get_connected_device_num(void)
{
	int ret, flags;

	flags = btsrv_set_negative_prio();
	ret = btsrv_rdm_get_connected_dev(NULL, NULL);
	btsrv_revert_prio(flags);

	return ret;
}

int btif_br_get_connected_phone_num(void)
{
	int ret, flags;

	flags = btsrv_set_negative_prio();
	ret = btsrv_rdm_get_connected_dev_cnt_by_type(BTSRV_DEVICE_PHONE);
	btsrv_revert_prio(flags);

	return ret;
}

int btif_br_get_dev_rdm_state(struct bt_dev_rdm_state *state)
{
	int ret, flags;

	flags = btsrv_set_negative_prio();
	ret = btsrv_rdm_get_dev_state(state);
	btsrv_revert_prio(flags);

	return ret;
}

void btif_br_set_phone_controler_role(bd_address_t *bd, uint8_t role)
{
	if (role != CONTROLER_ROLE_MASTER && role != CONTROLER_ROLE_SLAVE) {
		return;
	}

	btsrv_connect_set_phone_controler_role(bd, role);
}

int btif_br_get_linkkey(struct bt_linkkey_info *info, uint8_t cnt)
{
	int ret, flags;

	flags = btsrv_set_negative_prio();
	ret = btsrv_storage_get_linkkey(info, cnt);
	btsrv_revert_prio(flags);

	return ret;
}

int btif_br_update_linkkey(struct bt_linkkey_info *info, uint8_t cnt)
{
	int ret, flags;

	flags = btsrv_set_negative_prio();
	ret = btsrv_storage_update_linkkey(info, cnt);
	btsrv_revert_prio(flags);

	return ret;
}

int btif_br_write_ori_linkkey(bd_address_t *addr, uint8_t *link_key)
{
	int ret, flags;

	flags = btsrv_set_negative_prio();
	ret = btsrv_storage_write_ori_linkkey(addr, link_key);
	btsrv_revert_prio(flags);

	return ret;
}

void btif_br_clean_linkkey(void)
{
	int flags;

	flags = btsrv_set_negative_prio();
	btsrv_storage_clean_linkkey();
	btsrv_revert_prio(flags);
}


void btif_br_get_local_mac(bd_address_t *addr)
{
	int flags;

	flags = btsrv_set_negative_prio();
	btsrv_get_local_mac(addr);
	btsrv_revert_prio(flags);
}

void btif_ble_get_local_mac(bt_addr_le_t *addr)
{
	int flags;

	flags = btsrv_set_negative_prio();
	hostif_bt_read_ble_mac((bt_addr_le_t *)addr);
	btsrv_revert_prio(flags);
}


uint16_t btif_br_get_active_phone_hdl(void)
{
	int flags;
	uint16_t hdl;

	flags = btsrv_set_negative_prio();
	hdl = btsrv_rdm_get_actived_phone_hdl();
	btsrv_revert_prio(flags);

	return hdl;
}

int btif_br_disconnect_noactive_device(void)
{
	return btsrv_function_call(MSG_BTSRV_BASE, MSG_BTSRV_DISCONNECT_DEV_BY_PRIORITY, NULL);
}


int btif_dump_brsrv_info(void)
{
	return btsrv_function_call(MSG_BTSRV_BASE, MSG_BTSRV_DUMP_INFO, NULL);
}

int btif_br_update_qos(struct bt_update_qos_param *qos)
{
    return btsrv_function_call_malloc(MSG_BTSRV_BASE, MSG_BTSRV_UPDATE_QOS, (void *)qos, sizeof(struct bt_update_qos_param), 0);
}

int btif_get_type_by_handle(uint16_t handle)
{
	int ret, flags;

	flags = btsrv_set_negative_prio();
	ret = btsrv_adapter_get_type_by_handle(handle);
	btsrv_revert_prio(flags);
	return ret;
}

int btif_get_conn_role(uint16_t handle)
{
	return btsrv_adapter_get_role(handle);
}

int btif_bt_set_apll_temp_comp(u8_t enable)
{
	int flags, ret;

	flags = btsrv_set_negative_prio();
	ret = hostif_bt_vs_set_apll_temp_comp(enable);
	btsrv_revert_prio(flags);

	return ret;
}

int btif_bt_do_apll_temp_comp(void)
{
	int flags, ret;

	flags = btsrv_set_negative_prio();
	ret = hostif_bt_vs_do_apll_temp_comp();
	btsrv_revert_prio(flags);

	return ret;
}

int btif_bt_read_bt_build_version(uint8_t* str_version, int len)
{
	int flags, ret;

	flags = btsrv_set_negative_prio();
	ret = hostif_bt_vs_read_bt_build_version(str_version,len);
	btsrv_revert_prio(flags);

	return ret;
}


bd_address_t *btif_get_addr_by_handle(uint16_t handle)
{
	struct bt_conn_info info;
	int type = 0;

	type = btif_get_type_by_handle(handle);
	if (type == BT_TYPE_SCO || type ==BT_TYPE_BR_SNOOP || type == BT_TYPE_SCO_SNOOP) {
		type = BT_TYPE_BR;
	}

	if (bt_conn_get_info_by_handle(handle, &info) >= 0) {
		if (type == BT_TYPE_BR){
			return (bd_address_t *)info.br.dst;
		}else if (type == BT_TYPE_LE){
			return (bd_address_t *)&(info.le.dst->a);
		}
	}

	return NULL;
}

bt_addr_le_t *btif_get_le_addr_by_handle(uint16_t handle)
{
	struct bt_conn_info info;

	if (bt_conn_get_info_by_handle(handle, &info) >= 0) {
		if (info.type == BT_TYPE_LE) {
			return (bt_addr_le_t *)info.le.dst;
		}
	}

	return NULL;
}

int btif_conn_disconnect_by_handle(uint16_t handle, uint8_t reason)
{
	return hostif_bt_conn_disconnect_by_handle(handle, reason);
}

int btif_set_leaudio_connected(int connected)
{
	return btsrv_function_call(MSG_BTSRV_BASE, MSG_BTSRV_SET_LEAUDIO_CONNECTED, (void *)connected);
}

int btif_set_leaudio_foreground_dev(bool enable)
{
	return btsrv_adapter_set_leaudio_foreground_dev(enable);
}

int btif_disable_service()
{
	struct app_msg msg = {0};

	if (!srv_manager_check_service_is_actived(BLUETOOTH_SERVICE_NAME)) {
		return -ESRCH;
	}
	msg.type = MSG_SUSPEND_APP;

	SYS_LOG_INF("\n");
	return !send_async_msg(BLUETOOTH_SERVICE_NAME, &msg);
}

int btif_br_update_devie_name(void)
{
    return btsrv_function_call(MSG_BTSRV_BASE, MSG_BTSRV_UPDATE_DEVICE_NAME, NULL);
}

int btif_br_get_rssi_by_handle(uint16_t handle)
{ 
    int ret = -EIO;
    int flags;
    struct bt_conn * conn;

    flags = btsrv_set_negative_prio();
    conn = btsrv_rdm_find_conn_by_hdl(handle);
    if (conn){
        ret = btsrv_rdm_get_dev_rssi(conn);
    }
    btsrv_revert_prio(flags);
    return ret;
}

int btif_br_get_linkinfo_by_handle(uint16_t handle, complete_event_cb cb)
{ 
    int ret = -EIO;
    int flags;
    struct bt_conn * conn;

    flags = btsrv_set_negative_prio();
    conn = btsrv_rdm_find_conn_by_hdl(handle);
    if (conn){
        ret = hostif_bt_read_link_info(conn, cb);
    }
    btsrv_revert_prio(flags);
    return ret;
}

int btif_linkinfo_set(uint8_t *map)
{ 
	int ret = -EIO;
	int flags;

	flags = btsrv_set_negative_prio();
	ret = hostif_bt_link_info_set(NULL, map);
	btsrv_revert_prio(flags);
	return ret;
}

int btif_br_get_link_quality_by_handle(uint16_t handle)
{ 
    int ret = -EIO;
    int flags;
    struct bt_conn * conn;

    flags = btsrv_set_negative_prio();
    conn = btsrv_rdm_find_conn_by_hdl(handle);
    if (conn){
        ret = btsrv_rdm_get_dev_link_quality(conn);
    }
    btsrv_revert_prio(flags);
    return ret;
}

int btif_bt_set_pts_config(bool enable)
{
	int ret, flags;

	flags = btsrv_set_negative_prio();
	ret = btsrv_adapter_set_pts_config(enable);
	btsrv_revert_prio(flags);

	return ret;
}

int btif_bt_link_quality_always(bool enable)
{
	int ret, flags;

	flags = btsrv_set_negative_prio();
	ret = btsrv_adapter_link_quality_always(enable);
	btsrv_revert_prio(flags);

	return ret;
}

