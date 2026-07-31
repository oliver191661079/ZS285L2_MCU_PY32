/** @file
 * @brief Bluetooth host interface.
 */

/*
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr.h>
#include <acts_bluetooth/host_interface.h>
#include "keys_br_store.h"
#include "common_internal.h"
#include "keys.h"
#include "smp.h"

#define CONFIG_CSB 1

int hostif_set_negative_prio(void)
{
	int prio = 0;
	if(!k_is_in_isr()){
		prio = k_thread_priority_get(k_current_get());
		if (prio >= 0) {
			k_thread_priority_set(k_current_get(), -1);
		}
	}

	return prio;
}

void hostif_revert_prio(int prio)
{
	if(!k_is_in_isr()){
		if (prio >= 0) {
			k_thread_priority_set(k_current_get(), prio);
		}
	}
}

struct bt_conn *hostif_bt_conn_ref(struct bt_conn *conn)
{
#ifdef CONFIG_BT_HCI
	int prio;
	struct bt_conn *ret_conn;

	prio = hostif_set_negative_prio();
	ret_conn = bt_conn_ref(conn);
	hostif_revert_prio(prio);

	return ret_conn;
#else
	return NULL;
#endif
}

void hostif_bt_conn_unref(struct bt_conn *conn)
{
#ifdef CONFIG_BT_HCI
	int prio;

	prio = hostif_set_negative_prio();
	bt_conn_unref(conn);
	hostif_revert_prio(prio);
#endif
}

int hostif_bt_init_class(uint32_t classOfDevice)
{
#ifdef CONFIG_BT_HCI
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_init_class_of_device(classOfDevice);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_write_class(uint32_t classOfDevice)
{
#ifdef CONFIG_BT_HCI
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_br_write_class_of_device(classOfDevice);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}


void hostif_bt_set_share_bd_addr(bool set, uint8_t *mac)
{
#ifdef CONFIG_BT_BREDR
	int prio;

	prio = hostif_set_negative_prio();
	bt_set_share_bd_addr(set, mac);
	hostif_revert_prio(prio);
#endif
}

void hostif_bt_update_br_name(void)
{
#ifdef CONFIG_BT_BREDR
	int prio;

	prio = hostif_set_negative_prio();
	bt_update_br_name();
	hostif_revert_prio(prio);
#endif

}

int hostif_bt_init_device_id(uint16_t *did)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_init_device_id(did);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_enable(bt_ready_cb_t cb)
{
#ifdef CONFIG_BT_HCI
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_enable(cb);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_disable(void)
{
#ifdef CONFIG_BT_HCI
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_disable();
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_reg_evt_cb_func(bt_evt_cb_func_t cb)
{
#ifdef CONFIG_BT_HCI
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_reg_evt_cb_func(cb);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_br_discovery_start(const struct bt_br_discovery_param *param,
			  bt_br_discovery_cb_t cb)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_br_discovery_start(param, cb);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_br_discovery_stop(void)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_br_discovery_stop();
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_br_write_scan_enable(uint8_t scan, complete_event_cb cb)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_br_write_scan_enable(scan,cb);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_br_write_iac(bool limit_iac)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_br_write_iac(limit_iac);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_br_write_page_scan_activity(uint16_t interval, uint16_t windown)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_br_write_page_scan_activity(interval, windown);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_br_write_inquiry_scan_type(uint8_t type)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_br_write_inquiry_scan_type(type);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_br_write_page_scan_type(uint8_t type)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_br_write_page_scan_type(type);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_br_read_rssi(struct bt_conn *conn, complete_event_cb cb)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_br_read_rssi(conn, cb);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_br_read_link_quality(struct bt_conn *conn, complete_event_cb cb)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_br_read_link_quality(conn, cb);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_remote_name_request(const bt_addr_t *addr, bt_br_remote_name_cb_t cb)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_br_remote_name_request(addr, cb);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_br_set_page_timeout(uint32_t timeout)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_br_set_page_timeout(timeout);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_br_write_inquiry_scan_activity(uint16_t interval, uint16_t windown)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_br_write_inquiry_scan_activity(interval, windown);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

void hostif_bt_conn_cb_register(struct bt_conn_cb *cb)
{
#ifdef CONFIG_BT_HCI
	int prio;

	prio = hostif_set_negative_prio();
	bt_conn_cb_register(cb);
	hostif_revert_prio(prio);
#endif
}

void hostif_bt_conn_cb_unregister(struct bt_conn_cb *cb)
{
#ifdef CONFIG_BT_HCI
	int prio;

	prio = hostif_set_negative_prio();
	bt_conn_cb_unregister(cb);
	hostif_revert_prio(prio);
#endif
}

int hostif_bt_conn_get_info(const struct bt_conn *conn, struct bt_conn_info *info)
{
#ifdef CONFIG_BT_HCI
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_conn_get_info(conn, info);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

uint8_t hostif_bt_conn_get_type(const struct bt_conn *conn)
{
#ifdef CONFIG_BT_HCI
	uint8_t type = 0;
	int prio;
    if(conn){
        prio = hostif_set_negative_prio();
        type = bt_conn_get_type(conn);
        hostif_revert_prio(prio);
    }
	return type;
#else
	return 0;
#endif
}

uint16_t hostif_bt_conn_get_handle(const struct bt_conn *conn)
{
#ifdef CONFIG_BT_HCI
	uint16_t handle = 0;
	int prio;
    if(conn){
        prio = hostif_set_negative_prio();
        handle = bt_conn_get_handle(conn);
        hostif_revert_prio(prio);
    }
	return handle;
#else
	return 0;
#endif
}

struct bt_conn *hostif_bt_conn_lookup_by_handle(uint16_t handle)
{
#ifdef CONFIG_BT_HCI
		int prio;
		struct bt_conn *conn = NULL;
		prio = hostif_set_negative_prio();
		conn = bt_conn_get_conn_by_handle(handle);
		hostif_revert_prio(prio);

		return conn;
#else
		return NULL;
#endif

}

uint8_t hostif_bt_conn_get_type_by_handle(uint16_t handle)
{
#ifdef CONFIG_BT_HCI
	uint8_t type;
	int prio;

	prio = hostif_set_negative_prio();
	type = bt_conn_get_type_by_handle(handle);
	hostif_revert_prio(prio);

	return type;
#else
	return 0;
#endif
}

uint8_t hostif_bt_conn_get_role_by_handle(uint16_t handle)
{
#ifdef CONFIG_BT_HCI
	uint8_t type;
	int prio;

	prio = hostif_set_negative_prio();
	type = bt_conn_get_role_by_handle(handle);
	hostif_revert_prio(prio);

	return type;
#else
	return 0;
#endif
}

struct bt_conn *hostif_bt_conn_get_acl_conn_by_sco(const struct bt_conn *sco_conn)
{
#ifdef CONFIG_BT_BREDR
    struct bt_conn *conn = NULL;
    int prio;
    if(sco_conn){
        prio = hostif_set_negative_prio();
        conn = bt_conn_get_acl_conn_by_sco(sco_conn);
        hostif_revert_prio(prio);
    }
	return conn;
#else
	return NULL;
#endif
}

int hostif_bt_conn_set_snoop_media_rx_cid(struct bt_conn *conn, uint16_t cid)
{
#ifdef CONFIG_BT_BREDR
    int prio, ret = -1;
    if(conn){
        prio = hostif_set_negative_prio();
        ret = bt_conn_set_snoop_media_rx_cid(conn, cid);
        hostif_revert_prio(prio);
    }
	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_conn_set_snoop_switch_lock(struct bt_conn *conn, uint8_t lock)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_conn_set_snoop_switch_lock(conn, lock);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_sc_enable(uint8_t enable)
{
#ifdef CONFIG_BT_HCI
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_sc_enable(enable);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_ctkd_enable(uint8_t enable)
{
#ifdef CONFIG_BT_HCI
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_ctkd_enable(enable);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_conn_disconnect(struct bt_conn *conn, uint8_t reason)
{
#ifdef CONFIG_BT_HCI
	int prio, ret;

	if (!conn) {
		return -ENODEV;
	}

	prio = hostif_set_negative_prio();
	ret = bt_conn_disconnect(conn, reason);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_conn_disconnect_by_handle(uint16_t handle, uint8_t reason)
{
#ifdef CONFIG_BT_HCI
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_conn_disconnect_by_handle(handle, reason);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_gatt_over_br_disconnect(struct bt_conn *conn)
{
#ifdef CONFIG_BT_BREDR
		int prio, ret;

		prio = hostif_set_negative_prio();
		ret = bt_gatt_over_br_disconnect(conn);
		hostif_revert_prio(prio);

		return ret;
#else
		return -EIO;
#endif
}

struct bt_conn *hostif_bt_conn_create_br(const bt_addr_t *peer,
				  const struct bt_br_conn_param *param)
{
#ifdef CONFIG_BT_BREDR
	struct bt_conn *conn;
	int prio;

	prio = hostif_set_negative_prio();
	conn = bt_conn_create_br(peer, param);
	hostif_revert_prio(prio);

	return conn;
#else
	return NULL;
#endif
}

struct bt_conn *hostif_bt_conn_br_acl_connecting(const bt_addr_t *addr)
{
#ifdef CONFIG_BT_BREDR
	struct bt_conn *conn;
	int prio;

	prio = hostif_set_negative_prio();
	conn = bt_conn_br_acl_connecting(addr);
	hostif_revert_prio(prio);

	return conn;
#else
	return NULL;
#endif
}

int hostif_bt_conn_create_sco(struct bt_conn *br_conn)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret = -1;

	prio = hostif_set_negative_prio();
#ifdef CONFIG_BT_HFP_HF
	ret = bt_hfp_hf_req_codec_connection(br_conn);
#endif
	if (ret != 0) {
		/* HF not connect or AG device, just do sco connect */
		ret = bt_conn_create_sco(br_conn);
	}
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_conn_send_sco_data(struct bt_conn *conn, uint8_t *data, uint8_t len)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_conn_send_sco_data(conn, data, len);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_conn_check_enter_sniff(struct bt_conn *conn, uint16_t min_interval, uint16_t max_interval)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_conn_check_enter_sniff(conn, min_interval, max_interval);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_conn_check_exit_sniff(struct bt_conn *conn)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_conn_check_exit_sniff(conn);
	hostif_revert_prio(prio);

	return ret;
#else
	return 0;
#endif
}

int hostif_bt_conn_qos_setup(struct bt_conn_qos_param *qos)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_conn_qos_setup(qos);
	hostif_revert_prio(prio);

	return ret;
#else
	return 0;
#endif
}

uint16_t hostif_bt_conn_get_rxtx_cnt(struct bt_conn *conn)
{
#ifdef CONFIG_BT_HCI
	int prio;
	uint16_t ret;

	prio = hostif_set_negative_prio();
	ret = bt_conn_get_rxtx_cnt(conn);
	hostif_revert_prio(prio);

	return ret;
#else
	return 0;
#endif
}

int hostif_bt_conn_send_act_fix_cid_data(struct bt_conn *conn, uint16_t cid, uint8_t *data, uint16_t len)
{
#ifdef CONFIG_BT_HCI
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_conn_send_act_fix_cid_data(conn, cid, data, len);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_conn_tws_media_send_data(struct bt_conn *conn, uint16_t cid, uint8_t *data, uint16_t len, k_timeout_t timeout)
{
#ifdef CONFIG_BT_HCI
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_conn_send_act_fix_cid_data_by_media(conn, cid, data, len, timeout);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}


void* hostif_bt_conn_alloc_act_fix_cid_netbuf(uint16_t len,k_timeout_t timeout)
{
#ifdef CONFIG_BT_HCI
	int prio;
	void* buf;
	prio = hostif_set_negative_prio();
	buf = bt_conn_alloc_act_fix_cid_netbuf(len, timeout);
	hostif_revert_prio(prio);

	return buf;
#else
	return NULL;
#endif
}

int hostif_bt_conn_send_act_fix_cid_data_with_netbuf(struct bt_conn *conn, uint16_t cid, uint8_t *data, uint16_t len, void*netbuf)
{
#ifdef CONFIG_BT_HCI
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_conn_send_act_fix_cid_data_with_netbuf(conn, cid, data, len,netbuf);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}


int hostif_bt_br_conn_ready_send_data(struct bt_conn *conn)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_br_conn_ready_send_data(conn);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_br_conn_pending_pkt(struct bt_conn *conn, uint8_t *host_pending, uint8_t *controler_pending)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_br_conn_pending_pkt(conn, host_pending, controler_pending);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

void hostif_bt_l2cap_br_reg_pending_rx_data_cb_func(br_pending_rx_data_cb_t cb)
{
#ifdef CONFIG_BT_BREDR
	int prio;

	prio = hostif_set_negative_prio();
	bt_l2cap_br_reg_pending_rx_data_cb_func(cb);
	hostif_revert_prio(prio);
#endif
}

void hostif_bt_l2cap_br_reg_feature_fixed_chan_cb_func(br_feature_fixed_chan_cb_t cb)
{
#ifdef CONFIG_BT_BREDR
	int prio;

	prio = hostif_set_negative_prio();
	bt_l2cap_br_reg_feature_fixed_chan_cb_func(cb);
	hostif_revert_prio(prio);
#endif
}

void hostif_bt_l2cap_br_add_pending_rx_data(uint8_t *addr, uint8_t *data, uint16_t len)
{
#ifdef CONFIG_BT_BREDR
	int prio;

	prio = hostif_set_negative_prio();
	bt_l2cap_br_add_pending_rx_data(addr, data, len);
	hostif_revert_prio(prio);
#endif
}

void hostif_bt_l2cap_br_fixed_chan_enable(uint8_t enable)
{
#ifdef CONFIG_BT_BREDR
	int prio;

	prio = hostif_set_negative_prio();
	bt_l2cap_br_fixed_chan_enable(enable);
	hostif_revert_prio(prio);
#endif
}

int hostif_bt_conn_auth_cb_register(const struct bt_conn_auth_cb *cb)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_conn_auth_cb_register(cb);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_conn_le_auth_cb_register(const struct bt_conn_auth_cb *cb)
{
#ifdef CONFIG_BT_LE_ATT
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_conn_le_auth_cb_register(cb);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_conn_smp_confirm_reply(struct bt_conn *conn)
{
#ifdef CONFIG_BT_SMP
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_smp_auth_pairing_confirm(conn);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_conn_smp_cancel(struct bt_conn *conn)
{
#ifdef CONFIG_BT_SMP
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_smp_auth_cancel(conn);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_conn_ssp_confirm_reply(struct bt_conn *conn)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_conn_ssp_confirm_reply(conn);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_conn_ssp_confirm_neg_reply(struct bt_conn *conn)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_conn_ssp_confirm_neg_reply(conn);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}
int hostif_bt_conn_role_discovery(struct bt_conn *conn, uint8_t *role)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_conn_role_discovery(conn, role);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_conn_switch_role(struct bt_conn *conn, uint8_t role)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_conn_switch_role(conn, role);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_conn_set_supervision_timeout(struct bt_conn *conn, uint16_t timeout)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_conn_set_supervision_timeout(conn, timeout);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_conn_set_security(struct bt_conn *conn, bt_security_t sec)
{
#ifdef CONFIG_BT_HCI
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_conn_set_security(conn, sec);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_conn_drop_tx_buffer(struct bt_conn *conn,bt_conn_buffer_match buffer_match,uint8_t drop_count)
{
#ifdef CONFIG_BT_HCI
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_conn_drop_tx_buffer(conn, buffer_match,drop_count);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_hfp_hf_register_cb(struct bt_hfp_hf_cb *cb)
{
#ifdef CONFIG_BT_HFP_HF
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_hfp_hf_register_cb(cb);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_hfp_hf_send_cmd(struct bt_conn *conn, char *user_cmd)
{
#ifdef CONFIG_BT_HFP_HF
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_hfp_hf_send_cmd(conn, user_cmd);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_hfp_hf_connect(struct bt_conn *conn)
{
#ifdef CONFIG_BT_HFP_HF
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_hfp_hf_connect(conn);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_hfp_hf_disconnect(struct bt_conn *conn)
{
#ifdef CONFIG_BT_HFP_HF
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_hfp_hf_disconnect(conn);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_hfp_ag_register_cb(struct bt_hfp_ag_cb *cb)
{
#ifdef CONFIG_BT_HFP_AG
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_hfp_ag_register_cb(cb);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_hfp_ag_update_indicator(uint8_t index, uint8_t value)
{
#ifdef CONFIG_BT_HFP_AG
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_hfp_ag_update_indicator(index, value);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_hfp_ag_get_indicator(uint8_t index)
{
#ifdef CONFIG_BT_HFP_AG
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_hfp_ag_get_indicator(index);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_hfp_ag_send_event(struct bt_conn *conn, char *event, uint16_t len)
{
#ifdef CONFIG_BT_HFP_AG
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_hfp_ag_send_event(conn, event, len);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_hfp_ag_connect(struct bt_conn *conn)
{
#ifdef CONFIG_BT_HFP_AG
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_hfp_ag_connect(conn);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_hfp_ag_disconnect(struct bt_conn *conn)
{
#ifdef CONFIG_BT_HFP_AG
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_hfp_ag_disconnect(conn);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

void hostif_bt_hfp_ag_display_indicator(void)
{
#ifdef CONFIG_BT_HFP_AG
	int prio;

	prio = hostif_set_negative_prio();
	bt_hfp_ag_display_indicator();
	hostif_revert_prio(prio);
#endif
}

int hostif_bt_a2dp_connect(struct bt_conn *conn, uint8_t role)
{
#ifdef CONFIG_BT_A2DP
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_a2dp_connect(conn, role);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_a2dp_disconnect(struct bt_conn *conn)
{
#ifdef CONFIG_BT_A2DP
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_a2dp_disconnect(conn);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_a2dp_register_endpoint(struct bt_a2dp_endpoint *endpoint,
			      uint8_t media_type, uint8_t role)
{
#ifdef CONFIG_BT_A2DP
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_a2dp_register_endpoint(endpoint, media_type, role);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_a2dp_halt_endpoint(struct bt_a2dp_endpoint *endpoint, bool halt)
{
#ifdef CONFIG_BT_A2DP
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_a2dp_halt_endpoint(endpoint, halt);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_a2dp_register_cb(struct bt_a2dp_app_cb *cb)
{
#ifdef CONFIG_BT_A2DP
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_a2dp_register_cb(cb);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_a2dp_start(struct bt_conn *conn)
{
#ifdef CONFIG_BT_A2DP
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_a2dp_start(conn);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_a2dp_suspend(struct bt_conn *conn)
{
#ifdef CONFIG_BT_A2DP
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_a2dp_suspend(conn);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_a2dp_reconfig(struct bt_conn *conn, struct bt_a2dp_media_codec *codec)
{
#ifdef CONFIG_BT_A2DP
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_a2dp_reconfig(conn, codec);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_a2dp_send_delay_report(struct bt_conn *conn, uint16_t delay_time)
{
#ifdef CONFIG_BT_A2DP
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_a2dp_send_delay_report(conn, delay_time);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_a2dp_send_audio_data(struct bt_conn *conn, uint8_t *data, uint16_t len)
{
#ifdef CONFIG_BT_A2DP
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_a2dp_send_audio_data(conn, data, len);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

struct bt_a2dp_media_codec *hostif_bt_a2dp_get_seted_codec(struct bt_conn *conn)
{
#ifdef CONFIG_BT_A2DP
	struct bt_a2dp_media_codec *codec;
	int prio;

	prio = hostif_set_negative_prio();
	codec = bt_a2dp_get_seted_codec(conn);
	hostif_revert_prio(prio);

	return codec;
#else
	return NULL;
#endif
}

uint8_t hostif_bt_a2dp_get_a2dp_role(struct bt_conn *conn)
{
#ifdef CONFIG_BT_A2DP
	uint8_t role;
	int prio;

	prio = hostif_set_negative_prio();
	role = bt_a2dp_get_a2dp_role(conn);
	hostif_revert_prio(prio);

	return role;
#else
	return BT_A2DP_CH_UNKOWN;
#endif
}

uint16_t hostif_bt_a2dp_get_media_rx_cid(struct bt_conn *conn)
{
#ifdef CONFIG_BT_A2DP
	uint16_t cid;
	int prio;

	prio = hostif_set_negative_prio();
	cid = bt_a2dp_get_media_rx_cid(conn);
	hostif_revert_prio(prio);

	return cid;
#else
	return 0;
#endif
}

int hostif_bt_spp_send_data(uint8_t spp_id, uint8_t *data, uint16_t len)
{
#ifdef CONFIG_BT_SPP
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_spp_send_data(spp_id, data, len);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

uint8_t hostif_bt_spp_connect(struct bt_conn *conn, uint8_t *uuid)
{
#ifdef CONFIG_BT_SPP
	uint8_t spp_id;
	int prio;

	prio = hostif_set_negative_prio();
	spp_id = bt_spp_connect(conn, uuid);
	hostif_revert_prio(prio);

	return spp_id;
#else
	return -EIO;
#endif
}

int hostif_bt_spp_disconnect(uint8_t spp_id)
{
#ifdef CONFIG_BT_SPP
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_spp_disconnect(spp_id);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

uint8_t hostif_bt_spp_register_service(uint8_t *uuid)
{
#ifdef CONFIG_BT_SPP
	uint8_t spp_id;
	int prio;

	prio = hostif_set_negative_prio();
	spp_id = bt_spp_register_service(uuid);
	hostif_revert_prio(prio);

	return spp_id;
#else
	return 0;
#endif
}

int hostif_bt_spp_register_cb(struct bt_spp_app_cb *cb)
{
#ifdef CONFIG_BT_SPP
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_spp_register_cb(cb);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_avrcp_cttg_register_cb(struct bt_avrcp_app_cb *cb)
{
#ifdef CONFIG_BT_AVRCP
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_avrcp_cttg_register_cb(cb);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_avrcp_cttg_connect(struct bt_conn *conn)
{
#ifdef CONFIG_BT_AVRCP
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_avrcp_cttg_connect(conn);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_avrcp_cttg_disconnect(struct bt_conn *conn)
{
#ifdef CONFIG_BT_AVRCP
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_avrcp_cttg_disconnect(conn);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_avrcp_ct_pass_through_cmd(struct bt_conn *conn, avrcp_op_id opid, bool push)
{
#ifdef CONFIG_BT_AVRCP
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_avrcp_ct_pass_through_cmd(conn, opid, push);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_avrcp_tg_notify_change(struct bt_conn *conn, uint8_t play_state)
{
#ifdef CONFIG_BT_AVRCP
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_avrcp_tg_notify_change(conn, play_state);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_avrcp_ct_get_id3_info(struct bt_conn *conn)
{
#ifdef CONFIG_BT_AVRCP
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_avrcp_ct_get_id3_info(conn);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}


int hostif_bt_avrcp_ct_set_absolute_volume(struct bt_conn *conn, uint32_t param)
{
#ifdef CONFIG_BT_AVRCP
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_avrcp_ct_set_absolute_volume(conn, param);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_avrcp_ct_get_play_status(struct bt_conn *conn)
{
#ifdef CONFIG_BT_AVRCP
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_avrcp_ct_get_play_status(conn);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_avrcp_ct_get_capabilities(struct bt_conn *conn)
{
#ifdef CONFIG_BT_AVRCP
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_avrcp_ct_get_capabilities(conn);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_avrcp_ct_register_notification(struct bt_conn *conn)
{
#ifdef CONFIG_BT_AVRCP
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_avrcp_ct_register_notification(conn);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_avrcp_ct_get_support_feature(struct bt_conn *conn)
{
#ifdef CONFIG_BT_AVRCP
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_avrcp_ct_get_support_feature(conn);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif

}


uint8_t hostif_bt_pbap_client_get_phonebook(struct bt_conn *conn, char *path, struct bt_pbap_client_user_cb *cb)
{
#ifdef CONFIG_BT_PBAP_CLIENT
	uint8_t user_id;
	int prio;

	prio = hostif_set_negative_prio();
	user_id = bt_pbap_client_get_phonebook(conn, path, cb);
	hostif_revert_prio(prio);

	return user_id;
#else
	return 0;
#endif
}

int hostif_bt_pbap_client_abort_get(struct bt_conn *conn, uint8_t user_id)
{
#ifdef CONFIG_BT_PBAP_CLIENT
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_pbap_client_abort_get(conn, user_id);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

uint8_t hostif_bt_map_client_connect(struct bt_conn *conn,char *path,struct bt_map_client_user_cb *cb)
{
#ifdef CONFIG_BT_MAP_CLIENT
	uint8_t user_id;
	int prio;

	prio = hostif_set_negative_prio();
	user_id = bt_map_client_connect(conn,path,cb);
	hostif_revert_prio(prio);

	return user_id;
#else
	return 0;
#endif
}

uint8_t hostif_bt_map_client_get_message(struct bt_conn *conn, char *path, struct bt_map_client_user_cb *cb)
{
#ifdef CONFIG_BT_MAP_CLIENT
	uint8_t user_id;
	int prio;

	prio = hostif_set_negative_prio();
	user_id = bt_map_client_get_message(conn, path, cb);
	hostif_revert_prio(prio);

	return user_id;
#else
	return 0;
#endif
}

int hostif_bt_map_client_abort_get(struct bt_conn *conn, uint8_t user_id)
{
#ifdef CONFIG_BT_MAP_CLIENT
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_map_client_abort_get(conn, user_id);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_map_client_disconnect(struct bt_conn *conn, uint8_t user_id)
{
#ifdef CONFIG_BT_MAP_CLIENT
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_map_client_disconnect(conn, user_id);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_map_client_set_folder(struct bt_conn *conn, uint8_t user_id,char *path,uint8_t flags)
{
#ifdef CONFIG_BT_MAP_CLIENT
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_map_client_set_folder(conn, user_id,path,flags);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_map_client_get_folder_listing(struct bt_conn *conn, uint8_t user_id)
{
#ifdef CONFIG_BT_MAP_CLIENT
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_map_client_get_folder_listing(conn, user_id);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}


int hostif_bt_map_client_get_messages_listing(struct bt_conn *conn, uint8_t user_id,uint16_t max_list_count,uint32_t parameter_mask)
{
#ifdef CONFIG_BT_MAP_CLIENT
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_map_client_get_messages_listing(conn, user_id,max_list_count,parameter_mask);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_bipc_init(bt_bip_client_evt_cb cb)
{
#ifdef CONFIG_BT_BIP_CLIENT
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_bipc_init(cb);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_bipc_deinit(void)
{
#ifdef CONFIG_BT_BIP_CLIENT
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_bipc_deinit();
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

uint8_t hostif_bt_bipc_connect(struct bt_conn* conn, uint16_t psm)
{
#ifdef CONFIG_BT_BIP_CLIENT
	int prio;
	uint8_t ret;
	prio = hostif_set_negative_prio();
	ret = bt_bipc_connect(conn, psm);
	hostif_revert_prio(prio);

	return ret;
#else
	return 0;
#endif

}

int hostif_bt_bipc_disconnect(struct bt_conn* conn, uint8_t user_id)
{
#ifdef CONFIG_BT_BIP_CLIENT
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_bipc_disconnect(conn, user_id);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_bipc_get_capabilities(struct bt_conn* conn, uint8_t user_id)
{
#ifdef CONFIG_BT_BIP_CLIENT
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_bipc_get_capabilities(conn, user_id);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_bipc_get_image_properties(struct bt_conn* conn , uint8_t user_id, char* img_handle)
{
#ifdef CONFIG_BT_BIP_CLIENT
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_bipc_get_image_properties(conn, user_id, img_handle);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_bipc_get_image(struct bt_conn* conn, uint8_t user_id, char* img_handle, bt_bipc_image_desc_t* image_desc)
{
#ifdef CONFIG_BT_BIP_CLIENT
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_bipc_get_image(conn, user_id, img_handle, image_desc);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_bipc_abort(struct bt_conn* conn, uint8_t user_id)
{
#ifdef CONFIG_BT_BIP_CLIENT
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_bipc_abort(conn, user_id);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif

}


int hostif_bt_hid_register_sdp(struct bt_sdp_attribute * hid_attrs,int attrs_size)
{
#ifdef CONFIG_BT_HID
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_hid_register_sdp(hid_attrs, attrs_size);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_hid_register_cb(struct bt_hid_app_cb *cb)
{
#ifdef CONFIG_BT_HID
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_hid_register_cb(cb);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_hid_connect(struct bt_conn *conn)
{
#ifdef CONFIG_BT_HID
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_hid_connect(conn);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_hid_disconnect(struct bt_conn *conn)
{
#ifdef CONFIG_BT_HID
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_hid_disconnect(conn);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_hid_send_ctrl_data(struct bt_conn *conn,uint8_t report_type,uint8_t * data,uint16_t len)
{
#ifdef CONFIG_BT_HID
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_hid_send_ctrl_data(conn,report_type, data, len);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_hid_send_intr_data(struct bt_conn *conn,uint8_t report_type,uint8_t * data,uint16_t len)
{
#ifdef CONFIG_BT_HID
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_hid_send_intr_data(conn,report_type, data, len);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_hid_send_rsp(struct bt_conn *conn,uint8_t status)
{
#ifdef CONFIG_BT_HID
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_hid_send_rsp(conn,status);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_did_register_sdp(struct device_id_info *device_id)
{
//#ifdef CONFIG_BT_HID
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_did_register_sdp(device_id);
	hostif_revert_prio(prio);

	return ret;
//#else
//	return -EIO;
//#endif
}

int hostif_bt_sdp_pnp(struct bt_conn *conn, pnp_sdp_complete_cb cb)
{
#ifdef CONFIG_BT_DID_CLIENT
	int prio, ret;
	prio = hostif_set_negative_prio();
	ret = bt_sdp_pnp(conn,cb);
	hostif_revert_prio(prio);
	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_le_adv_start(const struct bt_le_adv_param *param,
		    const struct bt_data *ad, size_t ad_len,
		    const struct bt_data *sd, size_t sd_len)
{

#ifdef CONFIG_BT_LE_ATT
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_le_adv_start(param, ad, ad_len, sd, sd_len);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_le_adv_stop(void)
{
#ifdef CONFIG_BT_LE_ATT
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_le_adv_stop();
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_conn_le_param_update(struct bt_conn *conn,
			    const struct bt_le_conn_param *param)
{
#ifdef CONFIG_BT_LE_ATT
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_conn_le_param_update(conn, param);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_gatt_service_register(struct bt_gatt_service *svc)
{
#ifdef CONFIG_BT_LE_ATT
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_gatt_service_register(svc);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

uint16_t hostif_bt_gatt_get_mtu(struct bt_conn *conn)
{
#ifdef CONFIG_BT_LE_ATT
	uint16_t mtu;
	int prio;

	prio = hostif_set_negative_prio();
	mtu = bt_gatt_get_mtu(conn);
	hostif_revert_prio(prio);

	return mtu;
#else
	return 0;
#endif
}

uint16_t hostif_bt_gatt_over_br_get_mtu(struct bt_conn *conn)
{
#ifdef CONFIG_BT_LE_ATT
	uint16_t mtu;
	int prio;

	prio = hostif_set_negative_prio();
	mtu = bt_gatt_get_over_br_mtu(conn);
	hostif_revert_prio(prio);

	return mtu;
#else
	return 0;
#endif
}

int hostif_bt_gatt_indicate(struct bt_conn *conn,
		     struct bt_gatt_indicate_params *params)
{
#ifdef CONFIG_BT_LE_ATT
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_gatt_indicate(conn, params);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_gatt_notify(struct bt_conn *conn, const struct bt_gatt_attr *attr,
		   const void *data, uint16_t len)
{
#ifdef CONFIG_BT_LE_ATT
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_gatt_notify(conn, attr, data, len);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_gatt_exchange_mtu(struct bt_conn *conn,
			 struct bt_gatt_exchange_params *params)
{
#ifdef CONFIG_BT_LE_ATT
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_gatt_exchange_mtu(conn, params);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_gatt_get_ccc_value(const struct bt_gatt_attr *attr,uint8_t id, const bt_addr_le_t *addr)
{
#ifdef CONFIG_BT_LE_ATT
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_gatt_get_ccc_value(attr,id,addr);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_le_scan_start(const struct bt_le_scan_param *param, bt_le_scan_cb_t cb)
{
#ifdef CONFIG_BT_LE_ATT
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_le_scan_start(param, cb);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_le_scan_stop(void)
{
#ifdef CONFIG_BT_LE_ATT
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_le_scan_stop();
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_reg_flush_nvram_cb(void *cb)
{
#ifdef CONFIG_BT_PROPERTY
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_property_reg_flush_cb(cb);
	hostif_revert_prio(prio);

	return ret;
#else
	return 0;
#endif
}

int hostif_bt_store_get_linkkey(struct br_linkkey_info *info, uint8_t cnt)
{
#ifdef CONFIG_BT_PROPERTY
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_store_get_linkkey(info, cnt);
	hostif_revert_prio(prio);

	return ret;
#else
	return 0;
#endif
}

int hostif_bt_store_update_linkkey(struct br_linkkey_info *info, uint8_t cnt)
{
#ifdef CONFIG_BT_PROPERTY
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_store_update_linkkey(info, cnt);
	hostif_revert_prio(prio);

	return ret;
#else
	return 0;
#endif
}

void hostif_bt_store_write_ori_linkkey(bt_addr_t *addr, uint8_t *link_key)
{
#ifdef CONFIG_BT_PROPERTY
	int prio;

	prio = hostif_set_negative_prio();
	bt_store_write_ori_linkkey(addr, link_key);
	hostif_revert_prio(prio);
#endif
}

void hostif_bt_store_clear_linkkey(const bt_addr_t *addr)
{
#ifdef CONFIG_BT_PROPERTY
	int prio;

	prio = hostif_set_negative_prio();
	bt_store_clear_linkkey(addr);
	hostif_revert_prio(prio);
#endif
}

int hostif_bt_le_find_linkkey(const bt_addr_le_t *addr)
{
#ifdef CONFIG_BT_PROPERTY
	int prio, ret = 0;

	prio = hostif_set_negative_prio();
	ret = acts_le_keys_find_link_key(addr);
	hostif_revert_prio(prio);
	return ret;
#else
	return 0;
#endif
}

void hostif_bt_le_clear_linkkey(const bt_addr_le_t *addr)
{
#ifdef CONFIG_BT_PROPERTY
	int prio;
	prio = hostif_set_negative_prio();
	acts_le_keys_clear_link_key(addr);
	hostif_revert_prio(prio);
#endif
}

uint8_t hostif_bt_le_get_linkkey_mum(void)
{
#ifdef CONFIG_BT_PROPERTY
	int prio, ret = 0;

	prio = hostif_set_negative_prio();
	ret = acts_le_keys_get_link_key_num();
	hostif_revert_prio(prio);
	return ret;
#else
	return 0;
#endif
}

int hostif_bt_csb_set_broadcast_link_key_seed(uint8_t *seed, uint8_t len)
{
#ifdef CONFIG_CSB
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_csb_set_broadcast_link_key_seed(seed, len);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_csb_set_reserved_lt_addr(uint8_t lt_addr)
{
#ifdef CONFIG_CSB
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_csb_set_reserved_lt_addr(lt_addr);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_csb_set_CSB_broadcast(uint8_t enable, uint8_t lt_addr, uint16_t interval_min, uint16_t interval_max)
{
#ifdef CONFIG_CSB
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_csb_set_CSB_broadcast(enable, lt_addr, interval_min, interval_max);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_csb_set_CSB_receive(uint8_t enable, struct bt_hci_evt_sync_train_receive *param, uint16_t timeout)
{
#ifdef CONFIG_CSB
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_csb_set_CSB_receive(enable, param, timeout);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_csb_write_sync_train_param(uint16_t interval_min, uint16_t interval_max, uint32_t trainTO, uint8_t service_data)
{
#ifdef CONFIG_CSB
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_csb_write_sync_train_param(interval_min, interval_max, trainTO, service_data);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_csb_start_sync_train(void)
{
#ifdef CONFIG_CSB
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_csb_start_sync_train();
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_csb_set_CSB_date(uint8_t lt_addr, uint8_t *data, uint8_t len)
{
#ifdef CONFIG_CSB
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_csb_set_CSB_date(lt_addr, data, len);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_csb_receive_sync_train(uint8_t *mac, uint16_t timeout, uint16_t windown, uint16_t interval)
{
#ifdef CONFIG_CSB
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_csb_receive_sync_train(mac, timeout, windown, interval);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_csb_broadcase_by_acl(uint16_t handle, uint8_t *data, uint16_t len)
{
#ifdef CONFIG_CSB
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_csb_broadcase_by_acl(handle, data, len);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_vs_adjust_link_time(struct bt_conn *conn, int16_t time)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_vs_adjust_link_time(conn, time);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_vs_get_bt_clock(struct bt_conn *conn, uint32_t *bt_clk, uint16_t *bt_intraslot)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_vs_get_bt_clock(conn, bt_clk, bt_intraslot);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_vs_set_tws_int_clock(struct bt_conn *conn, uint32_t bt_clk, uint16_t bt_intraslot)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_vs_set_tws_int_clock(conn, bt_clk, bt_intraslot);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_vs_enable_tws_int(uint8_t enable)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_vs_enable_tws_int(enable);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}


int hostif_bt_vs_set_tws_sync_int_time(struct bt_conn *conn, uint8_t index, uint8_t mode,
	uint32_t bt_clk, uint16_t bt_slot)
{
#ifdef CONFIG_BT_CONN
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_vs_set_tws_sync_int_time(conn, index, mode, bt_clk, bt_slot);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_vs_set_tws_int_delay_play_time(struct bt_conn *conn, uint8_t index, uint8_t mode,
	uint8_t per_int, uint32_t bt_clk, uint16_t bt_slot)
{
#ifdef CONFIG_BT_CONN
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_vs_set_tws_int_delay_play_time(conn, index, mode, per_int, bt_clk, bt_slot);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_vs_set_tws_int_clock_new(struct bt_conn *conn, uint8_t index, uint8_t mode,
	uint32_t clk, uint16_t slot, uint32_t per_clk, uint16_t per_slot)
{
#ifdef CONFIG_BT_CONN
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_vs_set_tws_int_clock_new(conn, index, mode, clk, slot, per_clk, per_slot);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_vs_enable_tws_int_new(uint8_t enable, uint8_t index)
{
#ifdef CONFIG_BT_CONN
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_vs_enable_tws_int_new(enable, index);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_vs_read_bt_us_cnt(uint32_t *cnt)
{
#ifdef CONFIG_BT_CONN
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_vs_read_bt_us_cnt(cnt);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_vs_set_1st_sync_btclk(uint8_t send, struct bt_conn *conn, uint8_t enable)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_vs_set_1st_sync_btclk(send, conn, enable);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_vs_read_1st_sync_btclk(struct bt_conn *conn, uint32_t *bt_clk)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_vs_read_1st_sync_btclk(conn, bt_clk);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_vs_create_acl_snoop_link(struct bt_conn *conn)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_vs_create_acl_snoop_link(conn);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_vs_disconnect_acl_snoop_link(struct bt_conn *conn)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_vs_disconnect_acl_snoop_link(conn);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_vs_create_sync_snoop_link(struct bt_conn *conn)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_vs_create_sync_snoop_link(conn);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_vs_set_tws_link(struct bt_conn *conn)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_vs_set_tws_link(conn);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_vs_write_bt_addr(bt_addr_t *addr)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_vs_write_bt_addr(addr);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_vs_switch_snoop_link(struct bt_conn *conn)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_vs_switch_snoop_link(conn);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_vs_switch_le_link(struct bt_conn *conn)
{
#ifdef CONFIG_BT_LE_ATT
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_vs_switch_le_link(conn);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_vs_set_tws_visual_mode(struct tws_visual_mode_param *param)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_vs_set_tws_visual_mode(param);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_vs_set_tws_pair_mode(struct tws_pair_mode_param *param)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_vs_set_tws_pair_mode(param);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_vs_set_snoop_link_active(struct bt_conn *conn)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_vs_set_snoop_link_active(conn);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}


int hostif_bt_vs_set_snoop_link_inactive(struct bt_conn *conn)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_vs_set_snoop_link_inactive(conn);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_vs_set_cis_link_background(uint8_t enable)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_vs_set_cis_link_background(enable);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}


int hostif_bt_vs_set_apll_temp_comp(u8_t enable)
{
#ifdef CONFIG_BT_HCI
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_vs_set_apll_temp_comp(enable);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_vs_do_apll_temp_comp(void)
{
#ifdef CONFIG_BT_HCI
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_vs_do_apll_temp_comp();
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_vs_read_bt_build_version(uint8_t* str_version, int len)
{
#ifdef CONFIG_BT_HCI
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_vs_read_bt_build_version(str_version,len);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_vs_le_set_master_latency(struct bt_conn *conn, uint16_t latency,
				uint8_t retry)
{
#ifdef CONFIG_BT_HCI
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_vs_le_set_master_latency(conn, latency, retry);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}
bool hostif_bt_stack_ready_sync_info(struct bt_conn *conn)
{
#ifdef CONFIG_BT_BREDR
	int prio;
	bool ret;

	prio = hostif_set_negative_prio();
	ret = bt_stack_ready_sync_info(conn);
	hostif_revert_prio(prio);

	return ret;
#else
	return false;
#endif
}

void *hostif_bt_stack_get_sync_info(struct bt_conn *conn, void *info)
{
#ifdef CONFIG_BT_BREDR
	int prio;
	void *ret;

	prio = hostif_set_negative_prio();
	ret = bt_stack_get_sync_info(conn, info);
	hostif_revert_prio(prio);

	return ret;
#else
	return NULL;
#endif
}

struct bt_conn *hostif_bt_stack_set_sync_info(void *info, uint16_t len)
{
#ifdef CONFIG_BT_BREDR
	int prio;
	struct bt_conn *ret;

	prio = hostif_set_negative_prio();
	ret = bt_stack_set_sync_info(info, len);
	hostif_revert_prio(prio);

	return ret;
#else
	return NULL;
#endif
}

bool hostif_bt_le_stack_ready_sync_info(struct bt_conn *conn)
{
#ifdef CONFIG_BT_LE_ATT
	int prio;
	bool ret;

	prio = hostif_set_negative_prio();
	ret = bt_le_stack_ready_sync_info(conn);
	hostif_revert_prio(prio);

	return ret;
#else
	return false;
#endif
}

void *hostif_bt_le_stack_get_sync_info(struct bt_conn *conn, void *info)
{
#ifdef CONFIG_BT_LE_ATT
	int prio;
	void *ret;

	prio = hostif_set_negative_prio();
	ret = bt_le_stack_get_sync_info(conn, info);
	hostif_revert_prio(prio);

	return ret;
#else
	return NULL;
#endif
}

struct bt_conn *hostif_bt_le_stack_set_sync_info(void *info, uint16_t len)
{
#ifdef CONFIG_BT_LE_ATT
	int prio;
	struct bt_conn *ret;

	prio = hostif_set_negative_prio();
	ret = bt_le_stack_set_sync_info(info, len);
	hostif_revert_prio(prio);

	return ret;
#else
	return NULL;
#endif
}

void *hostif_bt_store_sync_get_linkkey_info(uint16_t *len)
{
#ifdef CONFIG_BT_BREDR
	int prio;
	void *ret;

	prio = hostif_set_negative_prio();
	ret = bt_store_sync_get_linkkey_info(len);
	hostif_revert_prio(prio);

	return ret;
#else
	return NULL;
#endif
}

void hostif_bt_store_sync_set_linkkey_info(void *info, uint16_t len, uint8_t local_is_master)
{
#ifdef CONFIG_BT_BREDR
	int prio;

	prio = hostif_set_negative_prio();
	bt_store_sync_set_linkkey_info(info, len, local_is_master);
	hostif_revert_prio(prio);
#endif
}

void hostif_bt_stack_dump_info(void)
{
#ifdef CONFIG_BT_BREDR
	int prio;

	prio = hostif_set_negative_prio();
	bt_stack_dump_info();
	hostif_revert_prio(prio);
#endif
}

void hostif_bt_read_ble_mac(bt_addr_le_t *addr)
{
	int prio;

	prio = hostif_set_negative_prio(); 
    bt_read_ble_mac(addr);
	hostif_revert_prio(prio);    

}

/*
  broadcast 
*/
int hostif_bt_iso_big_terminate(struct bt_iso_big *big)
{
   int ret;
   int prio;
   
   prio = hostif_set_negative_prio();   
   ret = bt_iso_big_terminate(big);
   hostif_revert_prio(prio);

   return ret;
}

int hostif_bt_le_per_adv_sync_delete(struct bt_le_per_adv_sync *per_adv_sync)
{
   int ret;
   int prio;

   prio = hostif_set_negative_prio();
   ret = bt_le_per_adv_sync_delete(per_adv_sync);
   hostif_revert_prio(prio);

   return ret;
}

int hostif_bt_le_ext_adv_create(const struct bt_le_adv_param *param,
			 const struct bt_le_ext_adv_cb *cb,
			 struct bt_le_ext_adv **out_adv)
{
   int ret;
   int prio;

   prio = hostif_set_negative_prio();
   ret = bt_le_ext_adv_create(param,cb,out_adv);
   hostif_revert_prio(prio);

   return ret;
}

int hostif_bt_le_per_adv_set_param(struct bt_le_ext_adv *adv,
			    const struct bt_le_per_adv_param *param)
{
   int ret;
   int prio;

   prio = hostif_set_negative_prio();
   ret =  bt_le_per_adv_set_param(adv,param);  
   hostif_revert_prio(prio);

   return ret;
}

int hostif_bt_le_per_adv_start(struct bt_le_ext_adv *adv)
{
   int ret;
   int prio;

   prio = hostif_set_negative_prio();
   ret = bt_le_per_adv_start(adv);
   hostif_revert_prio(prio);

   return ret;  
}

int hostif_bt_le_ext_adv_start(struct bt_le_ext_adv *adv,
			struct bt_le_ext_adv_start_param *param)
{
   int ret;
   int prio;

   prio = hostif_set_negative_prio();
   ret =  bt_le_ext_adv_start(adv,param);  
   hostif_revert_prio(prio);

   return ret;  

}

int hostif_bt_le_per_adv_set_subevent_data(const struct bt_le_ext_adv *adv, uint8_t num_subevents,
				    const struct bt_le_per_adv_subevent_data_params *params)
{
   int ret;
   int prio;

   prio = hostif_set_negative_prio();
   ret = bt_le_per_adv_set_subevent_data(adv, num_subevents,params);
   hostif_revert_prio(prio);

   return ret; 	
}

int hostif_bt_le_per_adv_sync_subevent(struct bt_le_per_adv_sync *per_adv_sync,
				struct bt_le_per_adv_sync_subevent_params *params)
{
   int ret;
   int prio;

   prio = hostif_set_negative_prio();
   ret = bt_le_per_adv_sync_subevent(per_adv_sync,params);
   hostif_revert_prio(prio);

   return ret;
}

int hostif_bt_le_per_adv_set_response_data(struct bt_le_per_adv_sync *per_adv_sync,
				    const struct bt_le_per_adv_response_params *param,
				    const struct net_buf_simple *data)
{
   int ret;
   int prio;

   prio = hostif_set_negative_prio();
   ret = bt_le_per_adv_set_response_data(per_adv_sync,param,data);
   hostif_revert_prio(prio);

   return ret;       
}

int hostif_bt_le_per_adv_set_info_transfer(const struct bt_le_ext_adv *adv,
				    const struct bt_conn *conn,
				    uint16_t service_data)
{
#if defined(CONFIG_BT_PER_ADV_SYNC_TRANSFER_SENDER)
   int ret;
   int prio;

   prio = hostif_set_negative_prio();
   ret = bt_le_per_adv_set_info_transfer(adv,conn,service_data);
   hostif_revert_prio(prio);

   return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_le_per_adv_sync_transfer(const struct bt_le_per_adv_sync *per_adv_sync,
				const struct bt_conn *conn,
				uint16_t service_data)
{
   int ret;
   int prio;

   prio = hostif_set_negative_prio();
   ret = bt_le_per_adv_sync_transfer(per_adv_sync,conn,service_data);
   hostif_revert_prio(prio);

   return ret;
}

int hostif_bt_le_per_adv_sync_transfer_subscribe(
	const struct bt_conn *conn,
	const struct bt_le_per_adv_sync_transfer_param *param)
{
   int ret;
   int prio;

   prio = hostif_set_negative_prio();
   ret = bt_le_per_adv_sync_transfer_subscribe(conn,param);
   hostif_revert_prio(prio);

   return ret;
}

int hostif_bt_le_per_adv_sync_transfer_unsubscribe(
	const struct bt_conn *conn)
{
   int ret;
   int prio;

   prio = hostif_set_negative_prio();
   ret = bt_le_per_adv_sync_transfer_unsubscribe(conn);
   hostif_revert_prio(prio);

   return ret;
}

void hostif_bt_le_scan_cb_register(struct bt_le_scan_cb *cb)
{
   int prio;

   prio = hostif_set_negative_prio();
   bt_le_scan_cb_register(cb);
   hostif_revert_prio(prio);
   	
}

void hostif_bt_le_per_adv_sync_cb_register(struct bt_le_per_adv_sync_cb *cb)
{
   int prio;

   prio = hostif_set_negative_prio();
   bt_le_per_adv_sync_cb_register(cb);
   hostif_revert_prio(prio);

}


int hostif_bt_le_per_adv_sync_create(const struct bt_le_per_adv_sync_param *param,
			      struct bt_le_per_adv_sync **out_sync)
{
   int ret;
   int prio;

   prio = hostif_set_negative_prio();
   ret = bt_le_per_adv_sync_create(param,out_sync);
   hostif_revert_prio(prio);

   return ret;

}		

int hostif_bt_le_ext_adv_set_data(struct bt_le_ext_adv *adv,
			   const struct bt_data *ad, size_t ad_len,
			   const struct bt_data *sd, size_t sd_len)
{
   int ret;
   int prio;

   prio = hostif_set_negative_prio();
   ret = bt_le_ext_adv_set_data(adv,ad, ad_len,sd, sd_len);   
   hostif_revert_prio(prio);

   return ret;
}

int hostif_bt_le_per_adv_set_data(const struct bt_le_ext_adv *adv,
			   const struct bt_data *ad, size_t ad_len)
{
   int ret;
   int prio;

   prio = hostif_set_negative_prio();
   ret = bt_le_per_adv_set_data(adv,ad,ad_len);
   hostif_revert_prio(prio);

   return ret;
}			   


void hostif_bt_le_scan_cb_unregister(struct bt_le_scan_cb *cb)
{
   int prio;

   prio = hostif_set_negative_prio();
   bt_le_scan_cb_unregister(cb); 
   hostif_revert_prio(prio);

}

void hostif_bt_le_per_adv_sync_cb_unregister(struct bt_le_per_adv_sync_cb *cb)
{
   int prio;

   prio = hostif_set_negative_prio();
   bt_le_per_adv_sync_cb_unregister(cb);
   hostif_revert_prio(prio);

}

int hostif_bt_le_per_adv_stop(struct bt_le_ext_adv *adv)
{
   int ret;
   int prio;

   prio = hostif_set_negative_prio();
   ret = bt_le_per_adv_stop(adv);
   hostif_revert_prio(prio);

   return ret;  	
}

int hostif_bt_le_ext_adv_stop(struct bt_le_ext_adv *adv)
{
   int ret;
   int prio;

   prio = hostif_set_negative_prio();
   ret = bt_le_ext_adv_stop(adv);
   hostif_revert_prio(prio);

   return ret;    	
}

int hostif_bt_le_ext_adv_delete(struct bt_le_ext_adv *adv)
{
   int ret;
   int prio;

   prio = hostif_set_negative_prio();  
   ret = bt_le_ext_adv_delete(adv);
   hostif_revert_prio(prio);

   return ret;    
}

int hostif_bt_iso_big_sync(struct bt_le_per_adv_sync *sync, struct bt_iso_big_sync_param *param,
		    struct bt_iso_big **out_big)
{
    int ret;
    int prio;

    prio = hostif_set_negative_prio();
    ret = bt_iso_big_sync(sync,param,out_big);
	hostif_revert_prio(prio);

   return ret;
}

int hostif_bt_iso_big_create(struct bt_le_ext_adv *padv, struct bt_iso_big_create_param *param,
		      struct bt_iso_big **out_big)
{
    int ret;
    int prio;

    prio = hostif_set_negative_prio();
    ret = bt_iso_big_create(padv,param,out_big);
	hostif_revert_prio(prio);

    return ret;
}

int hostif_bt_iso_big_create_test(struct bt_le_ext_adv *padv,
				struct bt_iso_big_create_test_param *param,
				struct bt_iso_big **out_big)
{
    int ret;
    int prio;

    prio = hostif_set_negative_prio();
	ret = bt_iso_big_create_test(padv,param,out_big);
	hostif_revert_prio(prio);

    return ret;
}				

int hostif_bt_set_pts_enable(bool enable)
{
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_set_pts_enable(enable);
	hostif_revert_prio(prio);

	return ret;
}

uint8_t hostif_bt_le_get_paired_devices(bt_addr_le_t dev_buf[], uint8_t buf_count)
{
	int prio;
	uint8_t ret;

	prio = hostif_set_negative_prio();
	ret = bt_le_get_paired_devices(dev_buf, buf_count);
	hostif_revert_prio(prio);

	return ret;
}

uint8_t hostif_bt_le_get_devices(bt_addr_le_t dev_buf[], uint8_t buf_count)
{
	int prio;
	uint8_t ret;

	prio = hostif_set_negative_prio();
	ret = bt_le_get_devices(dev_buf, buf_count);
	hostif_revert_prio(prio);

	return ret;
}


uint8_t hostif_bt_le_get_paired_devices_by_type(uint8_t addr_type, bt_addr_le_t dev_buf[], uint8_t buf_count)
{
	int prio;
	uint8_t ret;

	prio = hostif_set_negative_prio();
	ret = bt_le_get_paired_devices_by_type(addr_type, dev_buf, buf_count);
	hostif_revert_prio(prio);

	return ret;
}

bool hostif_bt_le_get_last_paired_device(bt_addr_le_t* dev)
{
	int prio;
	bool ret;

	prio = hostif_set_negative_prio();
	ret = bt_le_get_last_paired_device(dev);
	hostif_revert_prio(prio);

	return ret;
}

bool hostif_bt_le_clear_device(bt_addr_le_t* dev)
{
	int prio;
	bool ret;

	prio = hostif_set_negative_prio();
	ret = bt_le_clear_device(dev);
	hostif_revert_prio(prio);

	return ret;
}

void hostif_bt_le_unpair_device(const bt_addr_le_t *addr)
{
	int prio;

	prio = hostif_set_negative_prio();
	acts_le_keys_unpair_device(addr);
	hostif_revert_prio(prio);
}

void hostif_bt_le_clear_list(void)
{
	int prio;

	prio = hostif_set_negative_prio();
	acts_le_keys_clear_list();
	hostif_revert_prio(prio);
}

bool hostif_bt_le_is_bond(const bt_addr_le_t *addr)
{
	int prio;
	bool ret;

	prio = hostif_set_negative_prio();
	ret = acts_le_keys_is_bond(addr);
	hostif_revert_prio(prio);
	return ret;
}

bool hostif_bt_le_is_cis_capable(struct bt_conn *conn)
{
	int prio;
	bool ret;

	prio = hostif_set_negative_prio();
	ret = bt_le_check_remote_cis_feature(conn);
	hostif_revert_prio(prio);
	return ret;
}

bool hostif_bt_le_is_bis_capable(struct bt_conn *conn)
{
	int prio;
	bool ret;

	prio = hostif_set_negative_prio();
	ret = bt_le_check_remote_bis_feature(conn);
	hostif_revert_prio(prio);
	return ret;
}

#if defined(CONFIG_BT_PRIVACY)
int hostif_bt_le_address_resolution_enable(uint8_t enable)
{
	int prio;
	int ret;

	prio = hostif_set_negative_prio();
	ret = bt_le_address_resolution_enable(enable);
	hostif_revert_prio(prio);

	return ret;
}
#endif

int hostif_bt_iso_rssi_read(complete_event_cb cb)
{
#ifdef CONFIG_LOGSRV_BT_DEBUG
	int prio, ret;
	
	prio = hostif_set_negative_prio();
	ret = bt_iso_read_rssi(cb);
	hostif_revert_prio(prio);
	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_iso_link_info_get(complete_event_cb cb)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_iso_read_link_info(cb);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_read_link_info(struct bt_conn *conn, complete_event_cb cb)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_vs_read_link_info(conn, cb);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_link_info_set(struct bt_conn *conn, uint8_t *map)
{
#ifdef CONFIG_BT_BREDR
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_vs_link_channel_set(conn, map);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

uint8_t pa_ack_enable = 0;
void hostif_bt_pa_ack_enable(uint8_t enable)
{
   int prio;
   
   prio = hostif_set_negative_prio();
   pa_ack_enable = enable;
   hostif_revert_prio(prio);
}

int hostif_bt_fpms_register_cb(struct bt_fpms_app_cb *cb)
{
#ifdef CONFIG_BT_FPMS
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_fpms_register_cb(cb);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

int hostif_bt_fpms_send_data(struct bt_conn *conn,uint8_t *data, uint32_t len)
{
#ifdef CONFIG_BT_FPMS
	int prio, ret;

	prio = hostif_set_negative_prio();
	ret = bt_fpms_send(conn, data, len);
	hostif_revert_prio(prio);

	return ret;
#else
	return -EIO;
#endif
}

