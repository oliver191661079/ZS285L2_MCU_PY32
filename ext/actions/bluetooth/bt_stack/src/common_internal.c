/** @file common_internel.c
 * @brief Bluetooth common internel used.
 *
 * Copyright (c) 2019 Actions Semi Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr.h>
#include "stdlib.h"
#include <acts_bluetooth/bluetooth.h>
#include <acts_bluetooth/conn.h>
#include <acts_bluetooth/hfp_hf.h>
#include <acts_bluetooth/l2cap.h>
#include <acts_bluetooth/a2dp.h>
#include <acts_bluetooth/avrcp_cttg.h>

#include "hci_core.h"
#include "conn_internal.h"
#include "l2cap_internal.h"
#include "rfcomm_internal.h"
#include "hfp_internal.h"
#include "avdtp_internal.h"
#include "a2dp_internal.h"
#include "avrcp_internal.h"
#include "hid_internal.h"
#include "keys.h"

#include "common_internal.h"

#ifdef CONFIG_PROPERTY
#include <property_manager.h>
#endif

//to do
extern void * mem_malloc(unsigned int num_bytes);
extern void mem_free(void *ptr);

/* For controler acl tx
 * must large than(le acl data pkts + br acl data pkts)
 */
#define BT_ACL_TX_MAX		CONFIG_BT_CONN_TX_MAX
#define BT_BR_SIG_COUNT		(CONFIG_BT_MAX_BR_CONN*2)
#define BT_BR_RESERVE_PKT	3		/* Avoid send data used all pkts */
#define BT_LE_RESERVE_PKT	1		/* Avoid send data used all pkts */

#ifdef CONFIG_BT_STATIC_RANDOM_ADDR
#define CFG_LE_ADDR_TYPE		BT_ADDR_LE_RANDOM		/* Static random address */
#elif defined(CONFIG_BT_PRIVACY)
#define CFG_LE_ADDR_TYPE		BT_ADDR_LE_PUBLIC_ID	/* rpa */
#else
#define CFG_LE_ADDR_TYPE		BT_ADDR_LE_PUBLIC		/* Public address */
#endif

/* rfcomm */
#if defined(CONFIG_BT_HCI_ACL_FLOW_CONTROL)
#define RFCOMM_MAX_CREDITS_BUF         (CONFIG_BT_ACL_RX_COUNT - 1)
#else
#define RFCOMM_MAX_CREDITS_BUF         (CONFIG_BT_RX_BUF_COUNT - 1)
#endif

#if (RFCOMM_MAX_CREDITS_BUF > 7)
#define RFCOMM_MAX_CREDITS			7
#else
#define RFCOMM_MAX_CREDITS			RFCOMM_MAX_CREDITS_BUF
#endif

#ifdef CONFIG_BT_AVRCP_VOL_SYNC
#define CONFIG_SUPPORT_AVRCP_VOL_SYNC		1
#else
#define CONFIG_SUPPORT_AVRCP_VOL_SYNC		0
#endif

#if (CONFIG_SYS_LOG_DEFAULT_LEVEL >= 3)
#define BT_STACK_DEBUG_LOG		1
#else
#define BT_STACK_DEBUG_LOG		0
#endif

#ifdef CONFIG_BT_RFCOMM
#define BT_RFCOMM_L2CAP_MTU		CONFIG_BT_RFCOMM_L2CAP_MTU
#else
#define BT_RFCOMM_L2CAP_MTU		650		/* Just for compile */
#endif

#define L2CAP_BR_MIN_MTU	48

/* hfp */
#define HFP_POOL_COUNT		(CONFIG_BT_MAX_BR_CONN*2)
#define SDP_CLIENT_DISCOVER_USER_BUF_LEN		256

#ifndef CONFIG_BT_STACK_BQB_TEST
#define BT_SUPPORT_LINKKEY_MISS_REJECT_CONN  1
#else
#define BT_SUPPORT_LINKKEY_MISS_REJECT_CONN  0
#endif

/* Bt rx/tx data pool size */
#define BT_DATA_POOL_RX_SIZE		(4*1024)		/* TODO: Better  calculate by CONFIG */
#define BT_DATA_POOL_TX_SIZE		(4*1024)		/* TODO: Better  calculate by CONFIG */
#define BT_CMD_EVEVT_POOL_SIZE		(CONFIG_BT_HCI_CMD_COUNT*(CMD_BUF_SIZE+16)) //16:DATA_INFO_SIZE/align

typedef void (*property_flush_cb)(const char *key);
static property_flush_cb flush_cb_func;
static bool bluetooth_pts = 0;


const struct bt_inner_value_t bt_inner_value = {
	.max_conn = CONFIG_BT_MAX_CONN,
	.br_max_conn = CONFIG_BT_MAX_BR_CONN,
	.br_reserve_pkts = BT_BR_RESERVE_PKT,
	.le_reserve_pkts = BT_LE_RESERVE_PKT,
	.acl_tx_max = BT_ACL_TX_MAX,
	.rfcomm_max_credits = RFCOMM_MAX_CREDITS,
	.avrcp_vol_sync = CONFIG_SUPPORT_AVRCP_VOL_SYNC,
	.debug_log = BT_STACK_DEBUG_LOG,
	.le_addr_type = CFG_LE_ADDR_TYPE,
	.l2cap_tx_mtu = CONFIG_BT_L2CAP_TX_MTU,
	.rfcomm_l2cap_mtu = BT_RFCOMM_L2CAP_MTU,
	.avdtp_rx_mtu = BT_AVDTP_MAX_MTU,
	.hf_features = BT_HFP_HF_SUPPORTED_FEATURES,
	.ag_features = BT_HFP_AG_SUPPORTED_FEATURES,
	.br_support_capable = BT_SUPPORT_CAPABLE,
	.link_miss_reject_conn = BT_SUPPORT_LINKKEY_MISS_REJECT_CONN,
};

BT_BUF_POOL_ALLOC_DATA_DEFINE(host_rx_pool, BT_DATA_POOL_RX_SIZE);
BT_BUF_POOL_ALLOC_DATA_DEFINE(host_tx_pool, BT_DATA_POOL_TX_SIZE);
BT_BUF_POOL_ALLOC_CMD_EVENT_DEFINE(host_cmd_event_pool, BT_CMD_EVEVT_POOL_SIZE);

/* Pool for outgoing BR/EDR signaling packets, min MTU is 48 */
BT_BUF_POOL_DEFINE(br_sig_pool, BT_BR_SIG_COUNT,
		    BT_L2CAP_BUF_SIZE(L2CAP_BR_MIN_MTU), 4, NULL, &host_tx_pool);

/* hfp */
BT_BUF_POOL_DEFINE(hfp_pool, HFP_POOL_COUNT,
			 BT_RFCOMM_BUF_SIZE(BT_HF_CLIENT_MAX_PDU), 4, NULL, &host_tx_pool);

BT_BUF_POOL_DEFINE(sdp_client_discover_pool, CONFIG_BT_MAX_BR_CONN,
			 SDP_CLIENT_DISCOVER_USER_BUF_LEN, 4, NULL, &host_tx_pool);

/* L2cap br */
struct bt_l2cap_br bt_l2cap_br_pool[CONFIG_BT_MAX_BR_CONN] __IN_BT_SECTION;
struct bt_keys_link_key br_key_pool[CONFIG_BT_MAX_BR_CONN] __IN_BT_SECTION;

/* conn */
struct bt_conn acl_conns[CONFIG_BT_MAX_CONN] __IN_BT_BSS_SECTION;
struct bt_conn acl_le_adv_conn __IN_BT_BSS_SECTION;

/* CONFIG_BT_MAX_SCO_CONN == CONFIG_BT_MAX_BR_CONN */
struct bt_conn sco_conns[CONFIG_BT_MAX_BR_CONN] __IN_BT_BSS_SECTION;

/* hfp, Wait todo: Can use union to manager  hfp_hf_connection and hfp_ag_connection, for reduce memory */
struct bt_hfp_hf hfp_hf_connection[CONFIG_BT_MAX_BR_CONN] __IN_BT_SECTION;
#ifdef CONFIG_BT_HFP_AG
struct bt_hfp_ag hfp_ag_connection[CONFIG_BT_MAX_BR_CONN] __IN_BT_SECTION;
#endif

/* a2dp */
struct bt_avdtp_conn avdtp_conn[CONFIG_BT_MAX_BR_CONN] __IN_BT_BSS_SECTION;

/* avrcp */
struct bt_avrcp avrcp_connection[CONFIG_BT_MAX_BR_CONN] __IN_BT_BSS_SECTION;

/* rfcomm */
struct bt_rfcomm_session bt_rfcomm_connection[CONFIG_BT_MAX_BR_CONN] __IN_BT_SECTION;
//struct bt_rfcomm_session bt_rfcomm_connection[CONFIG_BT_MAX_BR_CONN] __IN_BT_BSS_SECTION;


/* hid */
#ifdef CONFIG_BT_HID
struct bt_hid_conn hid_conn __IN_BT_SECTION;
#endif

extern struct net_buf_pool prep_pool;
extern struct net_buf_pool hci_rx_pool;
extern struct net_buf_pool num_complete_pool;
extern struct net_buf_pool discardable_pool;
extern struct net_buf_pool acl_in_pool;
extern struct net_buf_pool acl_tx_pool;
extern struct net_buf_pool frag_pool;
extern struct net_buf_pool disc_pool;
extern struct net_buf_pool hci_cmd_pool;
extern struct net_buf_pool sdp_pool;

void bt_internal_pool_init(void)
{
#if 0
	net_data_pool_reinitializer(&host_rx_pool);
	net_data_pool_reinitializer(&host_tx_pool);

#if CONFIG_BT_ATT_PREPARE_COUNT > 0
	net_buf_pool_reinitializer(&prep_pool, CONFIG_BT_ATT_PREPARE_COUNT);
#endif /* CONFIG_BT_ATT_PREPARE_COUNT */
	net_buf_pool_reinitializer(&hci_rx_pool, CONFIG_BT_RX_BUF_COUNT);
	net_buf_pool_reinitializer(&num_complete_pool, 1);
#if defined(CONFIG_BT_DISCARDABLE_BUF_COUNT)
	net_buf_pool_reinitializer(&discardable_pool, CONFIG_BT_DISCARDABLE_BUF_COUNT);
#endif /* CONFIG_BT_DISCARDABLE_BUF_COUNT */
#if defined(CONFIG_BT_HCI_ACL_FLOW_CONTROL)
	net_buf_pool_reinitializer(&acl_in_pool, CONFIG_BT_ACL_RX_COUNT);
#endif /* CONFIG_BT_HCI_ACL_FLOW_CONTROL */
	net_buf_pool_reinitializer(&br_sig_pool, BT_BR_SIG_COUNT);
	net_buf_pool_reinitializer(&hfp_pool, HFP_POOL_COUNT);
	net_buf_pool_reinitializer(&sdp_client_discover_pool, CONFIG_BT_MAX_BR_CONN);

	net_buf_pool_reinitializer(&acl_tx_pool, CONFIG_BT_L2CAP_TX_BUF_COUNT);
	net_buf_pool_reinitializer(&frag_pool, CONFIG_BT_L2CAP_TX_FRAG_COUNT);
	net_buf_pool_reinitializer(&hci_cmd_pool, CONFIG_BT_HCI_CMD_COUNT);
	net_buf_pool_reinitializer(&disc_pool, 1);
	net_buf_pool_reinitializer(&sdp_pool, CONFIG_BT_MAX_BR_CONN);
#endif
}

void *bt_malloc(size_t size)
{
	return mem_malloc(size);		/* Wait todo */
}

void bt_free(void *ptr)
{
	return mem_free(ptr);		/* Wait todo */
}

int bt_property_delete(const char *key)
{
	int ret = -EIO;

#ifdef CONFIG_PROPERTY
	ret = property_set(key, NULL, 0);
#endif

	return ret;
}

int bt_property_set(const char *key, char *value, int value_len)
{
	int ret = -EIO;

#ifdef CONFIG_PROPERTY
	if (!value || !value_len) {
		return bt_property_delete(key);
	}

	ret = property_set(key, value, value_len);
	if (flush_cb_func) {
		flush_cb_func(key);
	}
#endif
	return ret;
}

int bt_property_get(const char *key, char *value, int value_len)
{
	int ret = -EIO;

#ifdef CONFIG_PROPERTY
	ret = property_get(key, value, value_len);
#endif
	return ret;
}

int bt_property_reg_flush_cb(void *cb)
{
	flush_cb_func = cb;

	return 0;
}

int bt_support_gatt_over_br(void)
{
#ifdef CONFIG_BT_GATT_OVER_BR
	return 1;
#else
	return 0;
#endif
}

int bt_set_pts_enable(bool enable)
{
	bluetooth_pts = enable;

    return 0;
}

bool bt_internal_is_pts_test(void)
{
	return bluetooth_pts;
}
