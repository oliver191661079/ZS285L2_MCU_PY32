/* hci_core.c - HCI core Bluetooth handling */

/*
 * Copyright (c) 2017-2021 Nordic Semiconductor ASA
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <atomic.h>
#include <misc/util.h>
#include <misc/slist.h>
#include <misc/byteorder.h>
#include <misc/stack.h>
#include <misc/__assert.h>
#include <soc.h>

#include <acts_bluetooth/bluetooth.h>
#include <acts_bluetooth/conn.h>
#include <acts_bluetooth/l2cap.h>
#include <acts_bluetooth/hci.h>
#include <acts_bluetooth/hci_vs.h>
#include <drivers/bluetooth/hci_driver.h>
#include <acts_bluetooth/hci_acts_vs.h>
#include <acts_bluetooth/hfp_hf.h>
#include <acts_bluetooth/addr_internal.h>

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_DEBUG_HCI_CORE)
#define LOG_MODULE_NAME bt_hci_core
#include "common/log.h"

#include "common/rpa.h"
#include "keys.h"
#include "monitor.h"
#include "hci_core.h"
#include "hci_ecc.h"
#include "ecc.h"
#include "id.h"
#include "adv.h"
#include "scan.h"

#include "conn_internal.h"
#include "iso_internal.h"
#include "l2cap_internal.h"
#include "gatt_internal.h"
#include "smp.h"
#include "crypto.h"
#include "property.h"

#if IS_ENABLED(CONFIG_BT_DF)
#include "direction_internal.h"
#endif /* CONFIG_BT_DF */

/* Actions add start */
#include "common_internal.h"
#include "keys_br_store.h"
#include "ctrl_interface.h"

/* Phoenex controller use BT_HCI_OP_ACCEPT_CONN_REQ command accept sco connect request */
#define MATCH_PHOENIX_CONTROLLER			1
#define DEFAULT_BT_CLASE_OF_DEVICE  	0x240404		/* Rendering,Audio, Audio/Video, Wearable Headset Device */
#define BT_DID_ACTIONS_COMPANY_ID		0x03E0
#define BT_DID_DEFAULT_COMPANY_ID		0xFFFF
#define BT_DID_PRODUCT_ID				0x0001
#define BT_DID_VERSION					0x0001
#define BT_DID_VENDOR_ID_SOURCE		    0x0001

#define BT_CONTROLLER_ACTIONS_COMPANY_ID 0x03E0

extern struct net_buf_pool acl_tx_pool;
extern struct net_buf_pool hci_rx_pool;
#if defined(CONFIG_BT_HCI_ACL_FLOW_CONTROL)
extern struct net_buf_pool acl_in_pool;
#endif
extern struct net_data_pool host_tx_pool;
extern struct net_data_pool host_cmd_event_pool;

static uint32_t bt_class_of_device = DEFAULT_BT_CLASE_OF_DEVICE;
static uint16_t bt_device_id[4] = {BT_DID_VENDOR_ID_SOURCE, BT_DID_ACTIONS_COMPANY_ID, BT_DID_PRODUCT_ID, BT_DID_VERSION};

//#if defined(CONFIG_BT_BREDR)			/* For build lib, always keep member */
static bt_hci_hf_codec_func get_hf_codec_func;
const uint8_t GIAC[3] = { 0x33, 0x8b, 0x9e };
const uint8_t LIAC[3] = { 0x00, 0x8b, 0x9e };		/* Range: 0x9E8B00 to 0x9E8B3F. */
static bt_br_remote_name_cb_t *remote_name_cb;
static bt_evt_cb_func_t evt_cb_func;

static void bt_evt_callback(uint16_t event, void *param);
static int br_write_mode_type(uint16_t opcode, uint8_t mode_type, bool send_sync);
static int br_ext_cmd_init(void);
static void mode_change(struct net_buf *buf);
static void remote_name_request_complete(struct net_buf *buf);
static void inquiry_complete(struct net_buf *buf);
static void inquiry_result_with_rssi(struct net_buf *buf);
static void extended_inquiry_result(struct net_buf *buf);
static uint16_t device_supported_pkt_type(uint8_t features[][8]);
static int bt_set_br_name(const char *name);
//#endif

static void hci_sco(struct net_buf *buf);
#ifdef MATCH_PHOENIX_CONTROLLER
static void bt_sco_conn_req(struct bt_hci_evt_conn_request *evt);
#endif

#ifdef CONFIG_BT_PROPERTY
static int bt_set_br_name(const char *name);
#endif
static void hci_sync_train_receive(struct net_buf *buf);
static void hci_csb_receive(struct net_buf *buf);
static void hci_csb_timeout(struct net_buf *buf);

#ifdef CONFIG_ACTS_BT_HCI_VS
static void vs_evt_acl_snoop_link_complete(struct net_buf *buf);
static void vs_evt_snoop_link_disconnect(struct net_buf *buf);
static void vs_evt_sync_snoop_link_complete(struct net_buf *buf);
static void vs_evt_snoop_switch_complete(struct net_buf *buf);
static void vs_evt_snoop_mode_change(struct net_buf *buf);
static void vs_evt_ll_conn_switch_complete(struct net_buf *buf);
static void bt_check_ctrl_active_connect(struct bt_hci_evt_conn_complete *evt);
static void vs_evt_tws_match_report(struct net_buf *buf);
static void vs_evt_snoop_active_link(struct net_buf *buf);
static void vs_evt_sync_1st_pkt_chg(struct net_buf *buf);
static void vs_evt_tws_local_exit_sniff(struct net_buf *buf);
static void vs_read_bb_reg_report(struct net_buf *buf);
static void vs_read_rf_reg_report(struct net_buf *buf);
static void vs_evt_pa_send_complete(struct net_buf *buf);
#endif

/* Actions add end */

#define RPA_TIMEOUT_MS       (CONFIG_BT_RPA_TIMEOUT * MSEC_PER_SEC)
#define RPA_TIMEOUT          K_MSEC(RPA_TIMEOUT_MS)

#define HCI_CMD_TIMEOUT      K_SECONDS(10)

/* Stacks for the threads */
#if !defined(CONFIG_BT_RECV_IS_RX_THREAD)
static struct k_thread rx_thread_data;
static BT_STACK_NOINIT(rx_thread_stack, CONFIG_BT_RX_STACK_SIZE);
#endif
static struct k_thread tx_thread_data;
static BT_STACK_NOINIT(tx_thread_stack, CONFIG_BT_HCI_TX_STACK_SIZE);

static void init_work(struct k_work *work);

struct bt_dev bt_dev = {
	.init          = _K_WORK_INITIALIZER(init_work),
	/* Give cmd_sem allowing to send first HCI_Reset cmd, the only
	 * exception is if the controller requests to wait for an
	 * initial Command Complete for NOP.
	 */
#if !defined(CONFIG_BT_WAIT_NOP)
	.ncmd_sem      = _K_SEM_INITIALIZER(bt_dev.ncmd_sem, 1, 1),
#else
	.ncmd_sem      = _K_SEM_INITIALIZER(bt_dev.ncmd_sem, 0, 1),
#endif
	.cmd_tx_queue  = _K_FIFO_INITIALIZER(bt_dev.cmd_tx_queue),
#if !defined(CONFIG_BT_RECV_IS_RX_THREAD)
	.rx_queue      = _K_FIFO_INITIALIZER(bt_dev.rx_queue),
#endif
};

static bt_ready_cb_t ready_cb;

#if defined(CONFIG_BT_HCI_VS_EVT_USER)
static bt_hci_vnd_evt_cb_t *hci_vnd_evt_cb;
#endif /* CONFIG_BT_HCI_VS_EVT_USER */

#if defined(CONFIG_BT_ECC)
static uint8_t pub_key[64];
static struct bt_pub_key_cb *pub_key_cb;
static bt_dh_key_cb_t dh_key_cb;
#endif /* CONFIG_BT_ECC */

#if defined(CONFIG_BT_BREDR)
static bt_br_discovery_cb_t *discovery_cb;
#if 0	/* Replace original discovery */
struct bt_br_discovery_result *discovery_results;
static size_t discovery_results_size;
static size_t discovery_results_count;
#endif
#endif /* CONFIG_BT_BREDR */

struct cmd_data {
	/** HCI status of the command completion */
	uint8_t  status;

	/** The command OpCode that the buffer contains */
	uint16_t opcode;

	/** The state to update when command completes with success. */
	struct bt_hci_cmd_state_set *state;

	/** Used by bt_hci_cmd_send_sync. */
	struct k_sem *sync;

	/* Actions add start, for asynchronization callback event complete. */
	complete_event_cb event_cb;
	/* Actions add start */
};

static struct cmd_data cmd_data[CONFIG_BT_HCI_CMD_COUNT];

#define cmd(buf) (&cmd_data[net_buf_id(buf)])
#define acl(buf) ((struct acl_data *)net_buf_user_data(buf))

void bt_hci_cmd_state_set_init(struct net_buf *buf,
			       struct bt_hci_cmd_state_set *state,
			       atomic_t *target, int bit, bool val)
{
	state->target = target;
	state->bit = bit;
	state->val = val;
	cmd(buf)->state = state;
}

BT_BUF_POOL_DEFINE(hci_cmd_pool, CONFIG_BT_HCI_CMD_COUNT,
			  CMD_BUF_SIZE, 4, NULL, &host_cmd_event_pool);

struct event_handler {
	uint8_t event;
	uint8_t min_len;
	void (*handler)(struct net_buf *buf);
};

#define EVENT_HANDLER(_evt, _handler, _min_len) \
{ \
	.event = _evt, \
	.handler = _handler, \
	.min_len = _min_len, \
}

static inline void handle_event(uint8_t event, struct net_buf *buf,
				const struct event_handler *handlers,
				size_t num_handlers)
{
	size_t i;

	for (i = 0; i < num_handlers; i++) {
		const struct event_handler *handler = &handlers[i];

		if (handler->event != event) {
			continue;
		}

		if (buf->len < handler->min_len) {
			BT_ERR("Too small (%u bytes) event 0x%02x",
			       buf->len, event);
			return;
		}

		handler->handler(buf);
		return;
	}

	BT_WARN("Unhandled event 0x%02x len %u: %s", event,
		buf->len, bt_hex(buf->data, buf->len));
}

#if defined(CONFIG_BT_HCI_ACL_FLOW_CONTROL)
void bt_hci_host_num_completed_packets(struct net_buf *buf)
{

	struct bt_hci_cp_host_num_completed_packets *cp;
	uint16_t handle = acl(buf)->handle;
	struct bt_hci_handle_count *hc;
	struct bt_conn *conn;

	net_buf_destroy(buf);

	/* Do nothing if controller to host flow control is not supported */
	if (!BT_CMD_TEST(bt_dev.supported_commands, 10, 5)) {
		return;
	}

	conn = bt_conn_lookup_index(acl(buf)->index);
	if (!conn) {
		BT_WARN("Unable to look up conn with index 0x%02x",
			acl(buf)->index);
		return;
	}

	if (conn->state != BT_CONN_CONNECTED &&
	    conn->state != BT_CONN_DISCONNECT) {
		BT_WARN("Not reporting packet for non-connected conn");
		bt_conn_unref(conn);
		return;
	}

	bt_conn_unref(conn);

	BT_DBG("Reporting completed packet for handle %u", handle);

	buf = bt_hci_cmd_create(BT_HCI_OP_HOST_NUM_COMPLETED_PACKETS,
				sizeof(*cp) + sizeof(*hc));
	if (!buf) {
		BT_ERR("Unable to allocate new HCI command");
		return;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->num_handles = sys_cpu_to_le16(1);

	hc = net_buf_add(buf, sizeof(*hc));
	hc->handle = sys_cpu_to_le16(handle);
	hc->count  = sys_cpu_to_le16(1);

	bt_hci_cmd_send(BT_HCI_OP_HOST_NUM_COMPLETED_PACKETS, buf);
}
#endif /* defined(CONFIG_BT_HCI_ACL_FLOW_CONTROL) */

struct net_buf *bt_hci_cmd_create(uint16_t opcode, uint8_t param_len)
{
	struct bt_hci_cmd_hdr *hdr;
	struct net_buf *buf;

	BT_DBG("opcode 0x%04x param_len %u", opcode, param_len);

	buf = net_buf_alloc(&hci_cmd_pool, K_FOREVER);
	__ASSERT_NO_MSG(buf);

	BT_DBG("buf %p", buf);

	net_buf_reserve(buf, BT_BUF_RESERVE);

	bt_buf_set_type(buf, BT_BUF_CMD);

	cmd(buf)->opcode = opcode;
	cmd(buf)->sync = NULL;
	cmd(buf)->state = NULL;
	cmd(buf)->event_cb = NULL;

	hdr = net_buf_add(buf, sizeof(*hdr));
	hdr->opcode = sys_cpu_to_le16(opcode);
	hdr->param_len = param_len;

	return buf;
}

int bt_hci_cmd_send(uint16_t opcode, struct net_buf *buf)
{
	if (!buf) {
		buf = bt_hci_cmd_create(opcode, 0);
		if (!buf) {
			return -ENOBUFS;
		}
	}

	BT_DBG("opcode 0x%04x len %u", opcode, buf->len);

	/* Host Number of Completed Packets can ignore the ncmd value
	 * and does not generate any cmd complete/status events.
	 */
	if (opcode == BT_HCI_OP_HOST_NUM_COMPLETED_PACKETS) {
		int err;

		err = bt_send(buf);
		if (err) {
			BT_ERR("Unable to send to driver (err %d)", err);
			net_buf_unref(buf);
		}

		return err;
	}

	net_buf_put(&bt_dev.cmd_tx_queue, buf);

	return 0;
}

int bt_hci_cmd_send_sync(uint16_t opcode, struct net_buf *buf,
			 struct net_buf **rsp)
{
	struct k_sem sync_sem;
	uint8_t status;
	int err;

	if (!buf) {
		buf = bt_hci_cmd_create(opcode, 0);
		if (!buf) {
			return -ENOBUFS;
		}
	}

	BT_DBG("buf %p opcode 0x%04x len %u", buf, opcode, buf->len);

	k_sem_init(&sync_sem, 0, 1);
	cmd(buf)->sync = &sync_sem;

	/* Make sure the buffer stays around until the command completes */
	net_buf_ref(buf);

	net_buf_put(&bt_dev.cmd_tx_queue, buf);

	err = k_sem_take(&sync_sem, HCI_CMD_TIMEOUT);
	BT_ASSERT_MSG(err == 0, "k_sem_take failed with err %d", err);

	status = cmd(buf)->status;
	if (status) {
		BT_WARN("opcode 0x%04x status 0x%02x", opcode, status);
		net_buf_unref(buf);

		switch (status) {
		case BT_HCI_ERR_CONN_LIMIT_EXCEEDED:
			return -ECONNREFUSED;
		default:
			return -EIO;
		}
	}

	BT_DBG("rsp %p opcode 0x%04x len %u", buf, opcode, buf->len);

	if (rsp) {
		*rsp = buf;
	} else {
		net_buf_unref(buf);
	}

	return 0;
}

static int hci_le_read_max_data_len(uint16_t *tx_octets, uint16_t *tx_time)
{
	struct bt_hci_rp_le_read_max_data_len *rp;
	struct net_buf *rsp;
	int err;

	err = bt_hci_cmd_send_sync(BT_HCI_OP_LE_READ_MAX_DATA_LEN, NULL, &rsp);
	if (err) {
		BT_ERR("Failed to read DLE max data len");
		return err;
	}

	rp = (void *)rsp->data;
	*tx_octets = sys_le16_to_cpu(rp->max_tx_octets);
	*tx_time = sys_le16_to_cpu(rp->max_tx_time);
	net_buf_unref(rsp);

	return 0;
}

uint8_t bt_get_phy(uint8_t hci_phy)
{
	switch (hci_phy) {
	case BT_HCI_LE_PHY_1M:
		return BT_GAP_LE_PHY_1M;
	case BT_HCI_LE_PHY_2M:
		return BT_GAP_LE_PHY_2M;
	case BT_HCI_LE_PHY_CODED:
		return BT_GAP_LE_PHY_CODED;
	default:
		return 0;
	}
}



#if defined(CONFIG_BT_CONN)
static void hci_acl(struct net_buf *buf)
{
	struct bt_hci_acl_hdr *hdr;
	uint16_t handle, len;
	struct bt_conn *conn;
	uint8_t flags;

	BT_DBG("buf %p", buf);

	BT_ASSERT(buf->len >= sizeof(*hdr));

	hdr = net_buf_pull_mem(buf, sizeof(*hdr));
	len = sys_le16_to_cpu(hdr->len);
	handle = sys_le16_to_cpu(hdr->handle);
	flags = bt_acl_flags(handle);

	acl(buf)->handle = bt_acl_handle(handle);
	acl(buf)->index = BT_CONN_INDEX_INVALID;

	BT_DBG("handle %u len %u flags %u", acl(buf)->handle, len, flags);

	if (buf->len != len) {
		BT_ERR("ACL data length mismatch (%u != %u)", buf->len, len);
		net_buf_unref(buf);
		return;
	}

	/* Wait TODO, call back csb receive data */
	/*
	 * if (acl(buf)->handle == 0) {
	 *	csb_slave_receive_data_cb(&buf->data[16], (len - 16));
	 *	net_buf_unref(buf);
	 *	return;
	 * }
	 */

	conn = bt_conn_lookup_handle(acl(buf)->handle);
	if (!conn) {
		BT_ERR("Unable to find conn for handle %u", acl(buf)->handle);
		net_buf_unref(buf);
		return;
	}

	acl(buf)->index = bt_conn_index(conn);

	bt_conn_recv(conn, buf, flags);
	bt_conn_unref(conn);
}

static void hci_data_buf_overflow(struct net_buf *buf)
{
	struct bt_hci_evt_data_buf_overflow *evt = (void *)buf->data;

	BT_WARN("Data buffer overflow (link type 0x%02x)", evt->link_type);
}

#if defined(CONFIG_BT_CONN)
static void hci_num_completed_packets(struct net_buf *buf)
{
	struct bt_hci_evt_num_completed_packets *evt = (void *)buf->data;
	int i;

	BT_DBG("num_handles %u", evt->num_handles);

	for (i = 0; i < evt->num_handles; i++) {
		uint16_t handle, count;
		struct bt_conn *conn;

		handle = sys_le16_to_cpu(evt->h[i].handle);
		count = sys_le16_to_cpu(evt->h[i].count);

		BT_DBG("handle %u count %u", handle, count);

		conn = bt_conn_lookup_handle(handle);
		if (!conn) {
			BT_ERR("No connection for handle %u", handle);
			continue;
		}

		if (conn->type == BT_CONN_TYPE_SCO || conn->type == BT_CONN_TYPE_SCO_SNOOP) {
			if (conn->sco.pending_cnt >= count) {
				conn->sco.pending_cnt -= count;
			} else {
				conn->sco.pending_cnt = 0;
			}
		} else {
			while (count--) {
				struct bt_conn_tx *tx;
				sys_snode_t *node;
				unsigned int key;

				key = irq_lock();

				if (conn->pending_no_cb) {
					conn->pending_no_cb--;
					if (conn->debug_tx_pending) {
						conn->debug_tx_pending--;
					} else {
						BT_ERR("debug packets count mismatch");
					}
					irq_unlock(key);
					k_sem_give(bt_conn_get_pkts(conn));
					bt_conn_set_pkts_signal(conn);
					continue;
				}

				node = sys_slist_get(&conn->tx_pending);
				irq_unlock(key);

				if (!node) {
					BT_ERR("packets count mismatch");
					break;
				}

				tx = CONTAINER_OF(node, struct bt_conn_tx, node);

				key = irq_lock();
				conn->pending_no_cb = tx->pending_no_cb;
				tx->pending_no_cb = 0U;
				sys_slist_append(&conn->tx_complete, &tx->node);
				if (conn->debug_tx_pending) {
					conn->debug_tx_pending--;
				} else {
					BT_ERR("debug packets count mismatch");
				}
				irq_unlock(key);

				k_work_submit(&conn->tx_complete_work);
				k_sem_give(bt_conn_get_pkts(conn));
				bt_conn_set_pkts_signal(conn);
			}
		}

		bt_conn_unref(conn);
	}
}
#endif /* CONFIG_BT_CONN */
#if defined(CONFIG_BT_CENTRAL)
static void set_phy_conn_param(const struct bt_conn *conn,
			       struct bt_hci_ext_conn_phy *phy)
{
	phy->conn_interval_min = sys_cpu_to_le16(conn->le.interval_min);
	phy->conn_interval_max = sys_cpu_to_le16(conn->le.interval_max);
	phy->conn_latency = sys_cpu_to_le16(conn->le.latency);
	phy->supervision_timeout = sys_cpu_to_le16(conn->le.timeout);

	phy->min_ce_len = 0;
	phy->max_ce_len = 0;
}

int bt_le_create_conn_ext(const struct bt_conn *conn)
{
	struct bt_hci_cp_le_ext_create_conn *cp;
	struct bt_hci_ext_conn_phy *phy;
	struct bt_hci_cmd_state_set state;
	bool use_filter = false;
	struct net_buf *buf;
	uint8_t own_addr_type;
	uint8_t num_phys;
	int err;

	if (IS_ENABLED(CONFIG_BT_WHITELIST)) {
		use_filter = atomic_test_bit(conn->flags, BT_CONN_AUTO_CONNECT);
	}

	err = bt_id_set_create_conn_own_addr(use_filter, &own_addr_type);
	if (err) {
		return err;
	}

	num_phys = (!(bt_dev.create_param.options &
		      BT_CONN_LE_OPT_NO_1M) ? 1 : 0) +
		   ((bt_dev.create_param.options &
		      BT_CONN_LE_OPT_CODED) ? 1 : 0);

	buf = bt_hci_cmd_create(BT_HCI_OP_LE_EXT_CREATE_CONN, sizeof(*cp) +
				num_phys * sizeof(*phy));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	(void)memset(cp, 0, sizeof(*cp));

	if (use_filter) {
		/* User Initiated procedure use fast scan parameters. */
		bt_addr_le_copy(&cp->peer_addr, BT_ADDR_LE_ANY);
		cp->filter_policy = BT_HCI_LE_CREATE_CONN_FP_WHITELIST;
	} else {
		const bt_addr_le_t *peer_addr = &conn->le.dst;

#if defined(CONFIG_BT_SMP)
		if (!bt_dev.le.rl_size ||
		    bt_dev.le.rl_entries > bt_dev.le.rl_size) {
			/* Host resolving is used, use the RPA directly. */
			peer_addr = &conn->le.resp_addr;
		}
#endif
		bt_addr_le_copy(&cp->peer_addr, peer_addr);
		cp->filter_policy = BT_HCI_LE_CREATE_CONN_FP_DIRECT;
	}

	cp->own_addr_type = own_addr_type;
	cp->phys = 0;

	if (!(bt_dev.create_param.options & BT_CONN_LE_OPT_NO_1M)) {
		cp->phys |= BT_HCI_LE_EXT_SCAN_PHY_1M;
		phy = net_buf_add(buf, sizeof(*phy));
		phy->scan_interval = sys_cpu_to_le16(
			bt_dev.create_param.interval);
		phy->scan_window = sys_cpu_to_le16(
			bt_dev.create_param.window);
		set_phy_conn_param(conn, phy);
	}

	if (bt_dev.create_param.options & BT_CONN_LE_OPT_CODED) {
		cp->phys |= BT_HCI_LE_EXT_SCAN_PHY_CODED;
		phy = net_buf_add(buf, sizeof(*phy));
		phy->scan_interval = sys_cpu_to_le16(
			bt_dev.create_param.interval_coded);
		phy->scan_window = sys_cpu_to_le16(
			bt_dev.create_param.window_coded);
		set_phy_conn_param(conn, phy);
	}

	bt_hci_cmd_state_set_init(buf, &state, bt_dev.flags,
				  BT_DEV_INITIATING, true);

	return bt_hci_cmd_send_sync(BT_HCI_OP_LE_EXT_CREATE_CONN, buf, NULL);
}

int bt_le_create_conn_synced(const struct bt_conn *conn, const struct bt_le_ext_adv *adv,
			     uint8_t subevent)
{
	struct bt_hci_cp_le_ext_create_conn_v2 *cp;
	struct bt_hci_ext_conn_phy *phy;
	struct bt_hci_cmd_state_set state;
	struct net_buf *buf;
	uint8_t own_addr_type;
	int err;

	err = bt_id_set_create_conn_own_addr(false, &own_addr_type);
	if (err) {
		return err;
	}

	/* There shall only be one Initiating_PHYs */
	buf = bt_hci_cmd_create(BT_HCI_OP_LE_EXT_CREATE_CONN_V2, sizeof(*cp) + sizeof(*phy));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	(void)memset(cp, 0, sizeof(*cp));

	cp->subevent = subevent;
	cp->adv_handle = adv->handle;
	bt_addr_le_copy(&cp->peer_addr, &conn->le.dst);
	cp->filter_policy = BT_HCI_LE_CREATE_CONN_FP_DIRECT;
	cp->own_addr_type = own_addr_type;

	/* The Initiating_PHY is the secondary phy of the corresponding ext adv set */
	if (adv->options & BT_LE_ADV_OPT_CODED) {
		cp->phys = BT_HCI_LE_EXT_SCAN_PHY_CODED;
	} else if (adv->options & BT_LE_ADV_OPT_NO_2M) {
		cp->phys = BT_HCI_LE_EXT_SCAN_PHY_1M;
	} else {
		cp->phys = BT_HCI_LE_EXT_SCAN_PHY_2M;
	}

	phy = net_buf_add(buf, sizeof(*phy));
	(void)memset(phy, 0, sizeof(*phy));
	set_phy_conn_param(conn, phy);

	bt_hci_cmd_state_set_init(buf, &state, bt_dev.flags, BT_DEV_INITIATING, true);

	return bt_hci_cmd_send_sync(BT_HCI_OP_LE_EXT_CREATE_CONN_V2, buf, NULL);
}

int bt_le_create_conn_legacy(const struct bt_conn *conn)
{
	struct bt_hci_cp_le_create_conn *cp;
	struct bt_hci_cmd_state_set state;
	bool use_filter = false;
	struct net_buf *buf;
	uint8_t own_addr_type;
	int err;

	if (IS_ENABLED(CONFIG_BT_WHITELIST)) {
		use_filter = atomic_test_bit(conn->flags, BT_CONN_AUTO_CONNECT);
	}

	err = bt_id_set_create_conn_own_addr(use_filter, &own_addr_type);
	if (err) {
		return err;
	}

	buf = bt_hci_cmd_create(BT_HCI_OP_LE_CREATE_CONN, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	memset(cp, 0, sizeof(*cp));
	cp->own_addr_type = own_addr_type;

	if (use_filter) {
		/* User Initiated procedure use fast scan parameters. */
		bt_addr_le_copy(&cp->peer_addr, BT_ADDR_LE_ANY);
		cp->filter_policy = BT_HCI_LE_CREATE_CONN_FP_WHITELIST;
	} else {
		const bt_addr_le_t *peer_addr = &conn->le.dst;

#if defined(CONFIG_BT_SMP)
		if (!bt_dev.le.rl_size ||
		    bt_dev.le.rl_entries > bt_dev.le.rl_size) {
			/* Host resolving is used, use the RPA directly. */
			peer_addr = &conn->le.resp_addr;
		}
#endif
		bt_addr_le_copy(&cp->peer_addr, peer_addr);
		cp->filter_policy = BT_HCI_LE_CREATE_CONN_FP_DIRECT;
	}

	cp->scan_interval = sys_cpu_to_le16(bt_dev.create_param.interval);
	cp->scan_window = sys_cpu_to_le16(bt_dev.create_param.window);

	cp->conn_interval_min = sys_cpu_to_le16(conn->le.interval_min);
	cp->conn_interval_max = sys_cpu_to_le16(conn->le.interval_max);
	cp->conn_latency = sys_cpu_to_le16(conn->le.latency);
	cp->supervision_timeout = sys_cpu_to_le16(conn->le.timeout);

	bt_hci_cmd_state_set_init(buf, &state, bt_dev.flags,
				  BT_DEV_INITIATING, true);

	return bt_hci_cmd_send_sync(BT_HCI_OP_LE_CREATE_CONN, buf, NULL);
}

int bt_le_create_conn(const struct bt_conn *conn)
{
	if (IS_ENABLED(CONFIG_BT_EXT_ADV) &&
	    BT_DEV_FEAT_LE_EXT_ADV(bt_dev.le.features)) {
		return bt_le_create_conn_ext(conn);
	}

	return bt_le_create_conn_legacy(conn);
}

int bt_le_create_conn_cancel(void)
{
	struct net_buf *buf;
	struct bt_hci_cmd_state_set state;

	buf = bt_hci_cmd_create(BT_HCI_OP_LE_CREATE_CONN_CANCEL, 0);
	if (!buf) {
		return -ENOBUFS;
	}

	bt_hci_cmd_state_set_init(buf, &state, bt_dev.flags,
				  BT_DEV_INITIATING, false);

	return bt_hci_cmd_send_sync(BT_HCI_OP_LE_CREATE_CONN_CANCEL, buf, NULL);
}
#endif /* CONFIG_BT_CENTRAL */

int bt_hci_disconnect(uint16_t handle, uint8_t reason)
{
	struct net_buf *buf;
	struct bt_hci_cp_disconnect *disconn;

	buf = bt_hci_cmd_create(BT_HCI_OP_DISCONNECT, sizeof(*disconn));
	if (!buf) {
		return -ENOBUFS;
	}

	disconn = net_buf_add(buf, sizeof(*disconn));
	disconn->handle = sys_cpu_to_le16(handle);
	disconn->reason = reason;

	return bt_hci_cmd_send_sync(BT_HCI_OP_DISCONNECT, buf, NULL);
}

static uint16_t disconnected_handles[CONFIG_BT_MAX_CONN];
static void disconnected_handles_reset(void)
{
	(void)memset(disconnected_handles, 0, sizeof(disconnected_handles));
}

static void conn_handle_disconnected(uint16_t handle)
{
	for (int i = 0; i < ARRAY_SIZE(disconnected_handles); i++) {
		if (!disconnected_handles[i]) {
			/* Use invalid connection handle bits so that connection
			 * handle 0 can be used as a valid non-zero handle.
			 */
			disconnected_handles[i] = ~BT_ACL_HANDLE_MASK | handle;
		}
	}
}

static bool conn_handle_is_disconnected(uint16_t handle)
{
	handle |= ~BT_ACL_HANDLE_MASK;

	for (int i = 0; i < ARRAY_SIZE(disconnected_handles); i++) {
		if (disconnected_handles[i] == handle) {
			disconnected_handles[i] = 0;
			return true;
		}
	}

	return false;
}

static void hci_disconn_complete_prio(struct net_buf *buf)
{
	struct bt_hci_evt_disconn_complete *evt = (void *)buf->data;
	uint16_t handle = sys_le16_to_cpu(evt->handle);
	struct bt_conn *conn;

	BT_DBG("status 0x%02x handle %u reason 0x%02x", evt->status, handle,
	       evt->reason);

	if (evt->status) {
		return;
	}

	conn = bt_conn_lookup_handle(handle);
	if (!conn) {
		BT_ERR("Unable to look up conn with handle 0x%x", handle);
		/* Priority disconnect complete event received before normal
		 * connection complete event.
		 */
		conn_handle_disconnected(handle);
		return;
	}

	bt_conn_set_state(conn, BT_CONN_DISCONNECT_COMPLETE);
	bt_conn_unref(conn);
}

static void hci_disconn_complete(struct net_buf *buf)
{
	struct bt_hci_evt_disconn_complete *evt = (void *)buf->data;
	uint16_t handle = sys_le16_to_cpu(evt->handle);
	struct bt_conn *conn;

	BT_DBG("status 0x%02x handle %u reason 0x%02x", evt->status, handle,
	       evt->reason);

	if (evt->status) {
		return;
	}

	conn = bt_conn_lookup_handle(handle);
	if (!conn) {
		BT_ERR("Unable to look up conn with handle %u", handle);
		return;
	}

	conn->err = evt->reason;

	bt_conn_set_state(conn, BT_CONN_DISCONNECTED);

	/* reset handle until ref is 0 */
#if 0
	conn->handle = 0U;
#endif

	if (conn->type != BT_CONN_TYPE_LE) {
#if defined(CONFIG_BT_BREDR)
		if (conn->type == BT_CONN_TYPE_SCO) {
			bt_sco_cleanup(conn);
			return;
		}

#ifdef CONFIG_BT_PROPERTY
		/*
		 * Store link key by adapter, we can clear link_key,
		 * so it can connect by another bt device.
		 */
		if (conn->type == BT_CONN_TYPE_BR) {
			atomic_clear_bit(conn->flags, BT_CONN_BR_NOBOND);
			if (conn->br.link_key) {
				bt_keys_link_key_clear(conn->br.link_key);
				conn->br.link_key = NULL;
			}
		}
#else
		/*
		 * If only for one connection session bond was set, clear keys
		 * database row for this connection.
		 */
		if (conn->type == BT_CONN_TYPE_BR &&
		    atomic_test_and_clear_bit(conn->flags, BT_CONN_BR_NOBOND)) {
			bt_keys_link_key_clear(conn->br.link_key);
		}
#endif
#endif
		bt_conn_unref(conn);
		return;
	}

#if defined(CONFIG_BT_CENTRAL) && !defined(CONFIG_BT_WHITELIST)
	if (atomic_test_bit(conn->flags, BT_CONN_AUTO_CONNECT)) {
		bt_conn_set_state(conn, BT_CONN_CONNECT_SCAN);
		bt_le_scan_update(false);
	}
#endif /* defined(CONFIG_BT_CENTRAL) && !defined(CONFIG_BT_WHITELIST) */

	bt_conn_unref(conn);
}

static int hci_le_read_remote_features(struct bt_conn *conn)
{
	struct bt_hci_cp_le_read_remote_features *cp;
	struct net_buf *buf;

	buf = bt_hci_cmd_create(BT_HCI_OP_LE_READ_REMOTE_FEATURES,
				sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->handle = sys_cpu_to_le16(conn->handle);
	bt_hci_cmd_send(BT_HCI_OP_LE_READ_REMOTE_FEATURES, buf);

	return 0;
}

static int hci_read_remote_version(struct bt_conn *conn)
{
	struct bt_hci_cp_read_remote_version_info *cp;
	struct net_buf *buf;

	if (conn->state != BT_CONN_CONNECTED) {
		return -ENOTCONN;
	}

	/* Remote version cannot change. */
	if (atomic_test_bit(conn->flags, BT_CONN_AUTO_VERSION_INFO)) {
		return 0;
	}

	buf = bt_hci_cmd_create(BT_HCI_OP_READ_REMOTE_VERSION_INFO,
				sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->handle = sys_cpu_to_le16(conn->handle);

	return bt_hci_cmd_send_sync(BT_HCI_OP_READ_REMOTE_VERSION_INFO, buf,
				    NULL);
}

/* LE Data Length Change Event is optional so this function just ignore
 * error and stack will continue to use default values.
 */
int bt_le_set_data_len(struct bt_conn *conn, uint16_t tx_octets, uint16_t tx_time)
{
	struct bt_hci_cp_le_set_data_len *cp;
	struct net_buf *buf;

	buf = bt_hci_cmd_create(BT_HCI_OP_LE_SET_DATA_LEN, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->handle = sys_cpu_to_le16(conn->handle);
	cp->tx_octets = sys_cpu_to_le16(tx_octets);
	cp->tx_time = sys_cpu_to_le16(tx_time);

	return bt_hci_cmd_send(BT_HCI_OP_LE_SET_DATA_LEN, buf);
}

#if defined(CONFIG_BT_USER_PHY_UPDATE)
static int hci_le_read_phy(struct bt_conn *conn)
{
	struct bt_hci_cp_le_read_phy *cp;
	struct bt_hci_rp_le_read_phy *rp;
	struct net_buf *buf, *rsp;
	int err;

	buf = bt_hci_cmd_create(BT_HCI_OP_LE_READ_PHY, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->handle = sys_cpu_to_le16(conn->handle);

	err = bt_hci_cmd_send_sync(BT_HCI_OP_LE_READ_PHY, buf, &rsp);
	if (err) {
		return err;
	}

	rp = (void *)rsp->data;
	conn->le.phy.tx_phy = bt_get_phy(rp->tx_phy);
	conn->le.phy.rx_phy = bt_get_phy(rp->rx_phy);
	net_buf_unref(rsp);

	return 0;
}
#endif /* defined(CONFIG_BT_USER_PHY_UPDATE) */

int bt_le_set_phy(struct bt_conn *conn, uint8_t all_phys,
		  uint8_t pref_tx_phy, uint8_t pref_rx_phy, uint8_t phy_opts)
{
	struct bt_hci_cp_le_set_phy *cp;
	struct net_buf *buf;

	buf = bt_hci_cmd_create(BT_HCI_OP_LE_SET_PHY, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->handle = sys_cpu_to_le16(conn->handle);
	cp->all_phys = all_phys;
	cp->tx_phys = pref_tx_phy;
	cp->rx_phys = pref_rx_phy;
	cp->phy_opts = phy_opts;

	return bt_hci_cmd_send(BT_HCI_OP_LE_SET_PHY, buf);
}

static struct bt_conn *find_pending_connect(uint8_t role, bt_addr_le_t *peer_addr)
{
	struct bt_conn *conn;

	/*
	 * Make lookup to check if there's a connection object in
	 * CONNECT or CONNECT_AUTO state associated with passed peer LE address.
	 */
	if (IS_ENABLED(CONFIG_BT_CENTRAL) && role == BT_HCI_ROLE_MASTER) {
		conn = bt_conn_lookup_state_le(BT_ID_DEFAULT, peer_addr,
					       BT_CONN_CONNECT);
		if (IS_ENABLED(CONFIG_BT_WHITELIST) && !conn) {
			conn = bt_conn_lookup_state_le(BT_ID_DEFAULT,
						       BT_ADDR_LE_NONE,
						       BT_CONN_CONNECT_AUTO);
		}

		return conn;
	}

	if (IS_ENABLED(CONFIG_BT_PERIPHERAL) && role == BT_HCI_ROLE_SLAVE) {
		conn = bt_conn_lookup_state_le(bt_dev.adv_conn_id, peer_addr,
					       BT_CONN_CONNECT_DIR_ADV);
		if (!conn) {
			conn = bt_conn_lookup_state_le(bt_dev.adv_conn_id,
						       BT_ADDR_LE_NONE,
						       BT_CONN_CONNECT_ADV);
		}
		return conn;
	}

	return NULL;
}

static void conn_auto_initiate(struct bt_conn *conn)
{
	int err;

	if (conn->state != BT_CONN_CONNECTED) {
		/* It is possible that connection was disconnected directly from
		 * connected callback so we must check state before doing
		 * connection parameters update.
		 */
		return;
	}

	if (!atomic_test_bit(conn->flags, BT_CONN_AUTO_FEATURE_EXCH) &&
	    ((conn->role == BT_HCI_ROLE_MASTER) ||
	     BT_FEAT_LE_SLAVE_FEATURE_XCHG(bt_dev.le.features))) {
		err = hci_le_read_remote_features(conn);
		if (!err) {
			return;
		}
	}

	if (IS_ENABLED(CONFIG_BT_REMOTE_VERSION) &&
	    !atomic_test_bit(conn->flags, BT_CONN_AUTO_VERSION_INFO)) {
		err = hci_read_remote_version(conn);
		if (!err) {
			return;
		}
	}

	if (IS_ENABLED(CONFIG_BT_AUTO_PHY_UPDATE) &&
	    !atomic_test_bit(conn->flags, BT_CONN_AUTO_PHY_COMPLETE) &&
	    BT_FEAT_LE_PHY_2M(bt_dev.le.features) && BT_FEAT_LE_PHY_2M(conn->le.features)) {
		err = bt_le_set_phy(conn, 0U, BT_HCI_LE_PHY_PREFER_2M,
				    BT_HCI_LE_PHY_PREFER_2M,
				    BT_HCI_LE_PHY_CODED_ANY);
		if (!err) {
			atomic_set_bit(conn->flags, BT_CONN_AUTO_PHY_UPDATE);
			return;
		}

		BT_ERR("Failed to set LE PHY (%d)", err);
	}

	if (IS_ENABLED(CONFIG_BT_AUTO_DATA_LEN_UPDATE) &&
	    BT_FEAT_LE_DLE(bt_dev.le.features)) {
		if (IS_BT_QUIRK_NO_AUTO_DLE(&bt_dev)) {
			uint16_t tx_octets, tx_time;

			err = hci_le_read_max_data_len(&tx_octets, &tx_time);
			if (!err) {
				err = bt_le_set_data_len(conn,
						tx_octets, tx_time);
				if (err) {
					BT_ERR("Failed to set data len (%d)", err);
				}
			}
		} else {
			/* No need to auto-initiate DLE procedure.
			 * It is done by the controller.
			 */
		}
	}
}

static void le_conn_complete_cancel(uint8_t err)
{
	struct bt_conn *conn;

	/* Handle create connection cancel.
	 *
	 * There is no need to check ID address as only one
	 * connection in master role can be in pending state.
	 */
	conn = find_pending_connect(BT_HCI_ROLE_MASTER, NULL);
	if (!conn) {
		BT_ERR("No pending master connection");
		return;
	}

	conn->err = err;

	/* Handle cancellation of outgoing connection attempt. */
	if (!IS_ENABLED(CONFIG_BT_WHITELIST)) {
		/* We notify before checking autoconnect flag
		 * as application may choose to change it from
		 * callback.
		 */
		bt_conn_set_state(conn, BT_CONN_DISCONNECTED);
		/* Check if device is marked for autoconnect. */
		if (atomic_test_bit(conn->flags, BT_CONN_AUTO_CONNECT)) {
			/* Restart passive scanner for device */
			bt_conn_set_state(conn, BT_CONN_CONNECT_SCAN);
		}
	} else {
		if (atomic_test_bit(conn->flags, BT_CONN_AUTO_CONNECT)) {
			/* Restart whitelist initiator after RPA timeout. */
			bt_le_create_conn(conn);
		} else {
			/* Create connection canceled by timeout */
			bt_conn_set_state(conn, BT_CONN_DISCONNECTED);
		}
	}

	bt_conn_unref(conn);
}

static void le_conn_complete_adv_timeout(void)
{
	if (!(IS_ENABLED(CONFIG_BT_EXT_ADV) &&
	      BT_DEV_FEAT_LE_EXT_ADV(bt_dev.le.features))) {
		struct bt_le_ext_adv *adv = bt_le_adv_lookup_legacy();
		struct bt_conn *conn;

		/* Handle advertising timeout after high duty cycle directed
		 * advertising.
		 */

		atomic_clear_bit(adv->flags, BT_ADV_ENABLED);

		if (IS_ENABLED(CONFIG_BT_EXT_ADV) &&
		    !BT_DEV_FEAT_LE_EXT_ADV(bt_dev.le.features)) {
			/* No advertising set terminated event, must be a
			 * legacy advertiser set.
			 */
			bt_le_adv_delete_legacy();
		}

		/* There is no need to check ID address as only one
		 * connection in slave role can be in pending state.
		 */
		conn = find_pending_connect(BT_HCI_ROLE_SLAVE, NULL);
		if (!conn) {
			BT_ERR("No pending slave connection");
			return;
		}

		conn->err = BT_HCI_ERR_ADV_TIMEOUT;
		bt_conn_set_state(conn, BT_CONN_DISCONNECTED);

		bt_conn_unref(conn);
	}
}

static void enh_conn_complete_internal(struct bt_hci_evt_le_enh_conn_complete *evt, bool switch_connected);

static void enh_conn_complete(struct bt_hci_evt_le_enh_conn_complete *evt, bool switch_connected)
{
#if (CONFIG_BT_CONN) && (CONFIG_BT_EXT_ADV_MAX_ADV_SET > 1)
	if (IS_ENABLED(CONFIG_BT_PERIPHERAL) &&
		evt->role == BT_HCI_ROLE_SLAVE &&
		evt->status == BT_HCI_ERR_SUCCESS &&
		(IS_ENABLED(CONFIG_BT_EXT_ADV) &&
				BT_FEAT_LE_EXT_ADV(bt_dev.le.features))) {

		/* Cache the connection complete event. Process it later.
		 * See bt_dev.cached_conn_complete.
		 */
		for (int i = 0; i < ARRAY_SIZE(bt_dev.cached_conn_complete); i++) {
			if (!bt_dev.cached_conn_complete[i].valid) {
				(void)memcpy(&bt_dev.cached_conn_complete[i].evt,
					evt,
					sizeof(struct bt_hci_evt_le_enh_conn_complete));
				bt_dev.cached_conn_complete[i].valid = true;
				bt_dev.cached_conn_complete[i].adv = NULL;
				return;
			}
		}

		__ASSERT(false, "No more cache entries available."
				"This should not happen by design");

		return;
	}
#endif
	enh_conn_complete_internal(evt, switch_connected);
}

static void translate_addrs(bt_addr_le_t *peer_addr, bt_addr_le_t *id_addr,
			    const struct bt_hci_evt_le_enh_conn_complete *evt, uint8_t id)
{
	if (bt_addr_le_is_resolved(&evt->peer_addr)) {
		bt_addr_le_copy_resolved(id_addr, &evt->peer_addr);

		bt_addr_copy(&peer_addr->a, &evt->peer_rpa);
		peer_addr->type = BT_ADDR_LE_RANDOM;
	} else {
		bt_addr_le_copy(id_addr, bt_lookup_id_addr(id, &evt->peer_addr));
		bt_addr_le_copy(peer_addr, &evt->peer_addr);
	}
}

static void update_conn(struct bt_conn *conn, const bt_addr_le_t *id_addr,
			const struct bt_hci_evt_le_enh_conn_complete *evt)
{
	conn->handle = sys_le16_to_cpu(evt->handle);
	bt_addr_le_copy(&conn->le.dst, id_addr);
	conn->le.interval = sys_le16_to_cpu(evt->interval);
	conn->le.latency = sys_le16_to_cpu(evt->latency);
	conn->le.timeout = sys_le16_to_cpu(evt->supv_timeout);
	conn->role = evt->role;
	conn->err = 0U;

#if defined(CONFIG_BT_USER_DATA_LEN_UPDATE)
	conn->le.data_len.tx_max_len = BT_GAP_DATA_LEN_DEFAULT;
	conn->le.data_len.tx_max_time = BT_GAP_DATA_TIME_DEFAULT;
	conn->le.data_len.rx_max_len = BT_GAP_DATA_LEN_DEFAULT;
	conn->le.data_len.rx_max_time = BT_GAP_DATA_TIME_DEFAULT;
#endif
}

void bt_hci_le_enh_conn_complete(struct bt_hci_evt_le_enh_conn_complete *evt)
{
	enh_conn_complete_internal(evt, false);
}

static void enh_conn_complete_internal(struct bt_hci_evt_le_enh_conn_complete *evt, bool switch_connected)
{
	uint16_t handle = sys_le16_to_cpu(evt->handle);
	bool is_disconnected = conn_handle_is_disconnected(handle);
	bt_addr_le_t peer_addr, id_addr;
	struct bt_conn *conn = NULL;
	uint8_t id;

	BT_DBG("status 0x%02x handle %u role %u peer %s peer RPA %s",
	       evt->status, handle, evt->role, bt_addr_le_str(&evt->peer_addr),
	       bt_addr_str(&evt->peer_rpa));
	BT_DBG("local RPA %s", bt_addr_str(&evt->local_rpa));

#if defined(CONFIG_BT_SMP)
	bt_id_pending_keys_update();
#endif

	if (evt->status) {
		if (IS_ENABLED(CONFIG_BT_PERIPHERAL) &&
		    evt->status == BT_HCI_ERR_ADV_TIMEOUT) {
			le_conn_complete_adv_timeout();
			return;
		}

		if (IS_ENABLED(CONFIG_BT_CENTRAL) &&
		    evt->status == BT_HCI_ERR_UNKNOWN_CONN_ID) {
			le_conn_complete_cancel(evt->status);
			bt_le_scan_update(false);
			return;
		}
		if (IS_ENABLED(CONFIG_BT_CENTRAL) && IS_ENABLED(CONFIG_BT_PER_ADV_RSP) &&
		    evt->status == BT_HCI_ERR_CONN_FAIL_TO_ESTAB) {
			le_conn_complete_cancel(evt->status);

			atomic_clear_bit(bt_dev.flags, BT_DEV_INITIATING);

			return;
		}

		BT_WARN("Unexpected status 0x%02x", evt->status);

		return;
	}

	/* Translate "enhanced" identity address type to normal one */
	id = evt->role == BT_HCI_ROLE_SLAVE ? bt_dev.adv_conn_id : BT_ID_DEFAULT;
	translate_addrs(&peer_addr, &id_addr, evt, id);

	if (IS_ENABLED(CONFIG_BT_PERIPHERAL) &&
			evt->role == BT_HCI_ROLE_SLAVE){
		for (int i = 0; i < ARRAY_SIZE(bt_dev.cached_conn_complete); i++) {
			if (bt_dev.cached_conn_complete[i].valid && &bt_dev.cached_conn_complete[i].evt == evt) {
				if(bt_dev.cached_conn_complete[i].adv){
					conn = bt_dev.cached_conn_complete[i].adv->conn;
					if(conn){
						bt_conn_ref(conn);
						bt_dev.cached_conn_complete[i].adv->conn = NULL;
						BT_INFO("enh_conn_complete %p %p\n",bt_dev.cached_conn_complete[i].adv,conn);
					}
				}
			}
		}
	}

	if(!conn){
		if (IS_ENABLED(CONFIG_BT_CENTRAL) && evt->role == BT_HCI_ROLE_MASTER) {
			conn = find_pending_connect(evt->role, &id_addr);
		}else if (IS_ENABLED(CONFIG_BT_PERIPHERAL) && evt->role == BT_HCI_ROLE_SLAVE) {
			conn = bt_conn_lookup_state_le_adv(bt_dev.adv_conn_id,
									BT_ADDR_LE_NONE,
									BT_CONN_CONNECT_ADV);
		}
		BT_INFO("enh_conn_complete %p\n",conn);
	}

	if (IS_ENABLED(CONFIG_BT_PERIPHERAL) &&
	    evt->role == BT_HCI_ROLE_SLAVE &&
	    !(IS_ENABLED(CONFIG_BT_EXT_ADV) &&
	      BT_DEV_FEAT_LE_EXT_ADV(bt_dev.le.features))) {
		struct bt_le_ext_adv *adv = bt_le_adv_lookup_legacy();
		/* Clear advertising even if we are not able to add connection
		 * object to keep host in sync with controller state.
		 */
		atomic_clear_bit(adv->flags, BT_ADV_ENABLED);
		(void)bt_le_lim_adv_cancel_timeout(adv);
	}

	if (IS_ENABLED(CONFIG_BT_CENTRAL) &&
	    evt->role == BT_HCI_ROLE_MASTER) {
		/* Clear initiating even if we are not able to add connection
		 * object to keep the host in sync with controller state.
		 */
		atomic_clear_bit(bt_dev.flags, BT_DEV_INITIATING);
	}

	if (!conn) {
		BT_ERR("No pending conn for peer %s",
		       bt_addr_le_str(&evt->peer_addr));
		bt_hci_disconnect(handle, BT_HCI_ERR_UNSPECIFIED);
		return;
	}

	conn->le_switch_connected = switch_connected ? 1 : 0;
	update_conn(conn, &id_addr, evt);

#if defined(CONFIG_BT_USER_PHY_UPDATE)
	conn->le.phy.tx_phy = BT_GAP_LE_PHY_1M;
	conn->le.phy.rx_phy = BT_GAP_LE_PHY_1M;
#endif
	/*
	 * Use connection address (instead of identity address) as initiator
	 * or responder address. Only slave needs to be updated. For master all
	 * was set during outgoing connection creation.
	 */
	if (IS_ENABLED(CONFIG_BT_PERIPHERAL) &&
	    conn->role == BT_HCI_ROLE_SLAVE) {
		bt_addr_le_copy(&conn->le.init_addr, &peer_addr);

		if (!(IS_ENABLED(CONFIG_BT_EXT_ADV) &&
		      BT_DEV_FEAT_LE_EXT_ADV(bt_dev.le.features))) {
			struct bt_le_ext_adv *adv = bt_le_adv_lookup_legacy();

			if (IS_ENABLED(CONFIG_BT_PRIVACY) &&
			    !atomic_test_bit(adv->flags, BT_ADV_USE_IDENTITY)) {
				conn->le.resp_addr.type = BT_ADDR_LE_RANDOM;
				if (bt_addr_cmp(&evt->local_rpa,
						BT_ADDR_ANY) != 0) {
					bt_addr_copy(&conn->le.resp_addr.a,
						     &evt->local_rpa);
				} else {
					bt_addr_copy(&conn->le.resp_addr.a,
						     &bt_dev.random_addr.a);
				}
			} else {
				bt_addr_le_copy(&conn->le.resp_addr,
						&bt_dev.id_addr[conn->id]);
			}
		} else {
			/* Copy the local RPA and handle this in advertising set
			 * terminated event.
			 */
			bt_addr_copy(&conn->le.resp_addr.a, &evt->local_rpa);
		}

		/* if the controller supports, lets advertise for another
		 * slave connection.
		 * check for connectable advertising state is sufficient as
		 * this is how this le connection complete for slave occurred.
		 */
		if (BT_LE_STATES_SLAVE_CONN_ADV(bt_dev.le.states)) {
			bt_le_adv_resume();
		}

		if (IS_ENABLED(CONFIG_BT_EXT_ADV) &&
		    !BT_DEV_FEAT_LE_EXT_ADV(bt_dev.le.features)) {
			struct bt_le_ext_adv *adv = bt_le_adv_lookup_legacy();
			/* No advertising set terminated event, must be a
			 * legacy advertiser set.
			 */
			if (!atomic_test_bit(adv->flags, BT_ADV_PERSIST)) {
				bt_le_adv_delete_legacy();
			}
		}
	}

	if (IS_ENABLED(CONFIG_BT_CENTRAL) &&
	    conn->role == BT_HCI_ROLE_MASTER) {
		bt_addr_le_copy(&conn->le.resp_addr, &peer_addr);

		if (IS_ENABLED(CONFIG_BT_PRIVACY)) {
			if (bt_addr_cmp(&evt->local_rpa, BT_ADDR_ANY) != 0) {
				bt_addr_copy(&conn->le.init_addr.a,
					     &evt->local_rpa);
				conn->le.init_addr.type = BT_ADDR_LE_RANDOM;
			} else {
				if (conn->id == BT_ID_PUBLIC) {
					conn->le.init_addr.type = BT_ADDR_LE_PUBLIC;
						bt_addr_le_copy(&conn->le.init_addr,
								&bt_dev.id_addr[conn->id]);
				} else if (conn->id == BT_ID_STATIC_RANDOM) {
					conn->le.init_addr.type = BT_ADDR_LE_RANDOM;
					bt_addr_copy(&conn->le.init_addr.a,
							 &bt_dev.random_addr.a);
				} else {
					BT_ERR("id:%d",conn->id);
				}
			}
		} else {
			bt_addr_le_copy(&conn->le.init_addr,
					&bt_dev.id_addr[conn->id]);
		}
	}

#if defined(CONFIG_BT_USER_PHY_UPDATE)
	if (IS_ENABLED(CONFIG_BT_EXT_ADV) &&
	    BT_DEV_FEAT_LE_EXT_ADV(bt_dev.le.features) &&
	    (!bt_conn_is_le_switch_connected(conn))) {
		int err;

		err = hci_le_read_phy(conn);
		if (err) {
			BT_WARN("Failed to read PHY (%d)", err);
		} else {
			if (IS_ENABLED(CONFIG_BT_AUTO_PHY_UPDATE) &&
			    conn->le.phy.tx_phy == BT_HCI_LE_PHY_PREFER_2M &&
			    conn->le.phy.rx_phy == BT_HCI_LE_PHY_PREFER_2M) {
				/* Already on 2M, skip auto-phy update. */
				atomic_set_bit(conn->flags,
					       BT_CONN_AUTO_PHY_COMPLETE);
			}
		}
	}
#endif /* defined(CONFIG_BT_USER_PHY_UPDATE) */

	bt_conn_set_state(conn, BT_CONN_CONNECTED);

	if (is_disconnected) {
		/* Mark the connection as already disconnected before calling
		 * the connected callback, so that the application cannot
		 * start sending packets
		 */
		bt_conn_set_state(conn, BT_CONN_DISCONNECT_COMPLETE);
	}

	if (!bt_conn_is_le_switch_connected(conn)) {
		/* Start auto-initiated procedures */
		conn_auto_initiate(conn);
	}

	bt_conn_unref(conn);

	if (IS_ENABLED(CONFIG_BT_CENTRAL) &&
	    conn->role == BT_HCI_ROLE_MASTER) {
		bt_le_scan_update(false);
	}
}

#if defined(CONFIG_BT_PER_ADV_SYNC_RSP)
void bt_hci_le_enh_conn_complete_sync(struct bt_hci_evt_le_enh_conn_complete_v2 *evt,
				      struct bt_le_per_adv_sync *sync)
{
	uint16_t handle = sys_le16_to_cpu(evt->handle);
	bool is_disconnected = conn_handle_is_disconnected(handle);
	bt_addr_le_t peer_addr, id_addr;
	struct bt_conn *conn;

	if (!sync->num_subevents) {
		BT_ERR("Unexpected connection complete event");

		return;
	}

	conn = bt_conn_add_le(BT_ID_DEFAULT, BT_ADDR_LE_ANY);
	if (!conn) {
		BT_ERR("Unable to allocate connection");
		/* Tell the controller to disconnect to keep it in sync with
		 * the host state and avoid a "rogue" connection.
		 */
		bt_hci_disconnect(handle, BT_HCI_ERR_REMOTE_USER_TERM_CONN);

		return;
	}

	BT_DBG("status 0x%02x handle %u role %u peer %s peer RPA %s", evt->status, handle,
		evt->role, bt_addr_le_str(&evt->peer_addr), bt_addr_str(&evt->peer_rpa));
	BT_DBG("local RPA %s", bt_addr_str(&evt->local_rpa));

	if (evt->role != BT_HCI_ROLE_SLAVE) {
		BT_ERR("PAwR sync always becomes peripheral");

		return;
	}

#if defined(CONFIG_BT_SMP)
	bt_id_pending_keys_update();
#endif

	if (evt->status) {
		BT_ERR("Unexpected status 0x%02x", evt->status);

		return;
	}

	translate_addrs(&peer_addr, &id_addr, (const struct bt_hci_evt_le_enh_conn_complete *)evt,
			BT_ID_DEFAULT);
	update_conn(conn, &id_addr, (const struct bt_hci_evt_le_enh_conn_complete *)evt);

#if defined(CONFIG_BT_USER_PHY_UPDATE)
	/* The connection is always initated on the same phy as the PAwR advertiser */
	conn->le.phy.tx_phy = sync->phy;
	conn->le.phy.rx_phy = sync->phy;
#endif

	bt_addr_le_copy(&conn->le.init_addr, &peer_addr);

	if (IS_ENABLED(CONFIG_BT_PRIVACY)) {
		conn->le.resp_addr.type = BT_ADDR_LE_RANDOM;
		bt_addr_copy(&conn->le.resp_addr.a, &evt->local_rpa);
	} else {
		bt_addr_le_copy(&conn->le.resp_addr, &bt_dev.id_addr[conn->id]);
	}

	bt_conn_set_state(conn, BT_CONN_CONNECTED);

	if (is_disconnected) {
		/* Mark the connection as already disconnected before calling
		 * the connected callback, so that the application cannot
		 * start sending packets
		 */
		bt_conn_set_state(conn, BT_CONN_DISCONNECT_COMPLETE);
	}

	/* Since we don't give the application a reference to manage
	 * for peripheral connections, we need to release this reference here.
	 */
	bt_conn_unref(conn);

	/* Start auto-initiated procedures */
	conn_auto_initiate(conn);
}
#endif /* CONFIG_BT_PER_ADV_SYNC_RSP */

static void le_enh_conn_complete(struct net_buf *buf)
{
	enh_conn_complete((void *)buf->data, false);
}

#if defined(CONFIG_BT_PER_ADV_RSP) || defined(CONFIG_BT_PER_ADV_SYNC_RSP)
static void le_enh_conn_complete_v2(struct net_buf *buf)
{
	struct bt_hci_evt_le_enh_conn_complete_v2 *evt =
		(struct bt_hci_evt_le_enh_conn_complete_v2 *)buf->data;

	if (evt->adv_handle == BT_HCI_ADV_HANDLE_INVALID &&
	    evt->sync_handle == BT_HCI_SYNC_HANDLE_INVALID) {
		/* The connection was not created via PAwR, handle the event like v1 */
		enh_conn_complete((struct bt_hci_evt_le_enh_conn_complete *)evt, false);
	}
#if defined(CONFIG_BT_PER_ADV_RSP)
	else if (evt->adv_handle != BT_HCI_ADV_HANDLE_INVALID &&
		 evt->sync_handle == BT_HCI_SYNC_HANDLE_INVALID) {
		/* The connection was created via PAwR advertiser, it can be handled like v1 */
		enh_conn_complete((struct bt_hci_evt_le_enh_conn_complete *)evt, false);
	}
#endif /* CONFIG_BT_PER_ADV_RSP */
#if defined(CONFIG_BT_PER_ADV_SYNC_RSP)
	else if (evt->adv_handle == BT_HCI_ADV_HANDLE_INVALID &&
		 evt->sync_handle != BT_HCI_SYNC_HANDLE_INVALID) {
		/* Created via PAwR sync, no adv set terminated event, needs separate handling */
		struct bt_le_per_adv_sync *sync;

		sync = bt_hci_get_per_adv_sync(evt->sync_handle);
		if (!sync) {
			BT_ERR("Unknown sync handle %d", evt->sync_handle);

			return;
		}

		bt_hci_le_enh_conn_complete_sync(evt, sync);
	}
#endif /* CONFIG_BT_PER_ADV_SYNC_RSP */
	else {
		BT_ERR("Invalid connection complete event");
	}
}
#endif /* CONFIG_BT_PER_ADV_RSP || CONFIG_BT_PER_ADV_SYNC_RSP */

static void le_legacy_conn_complete(struct net_buf *buf)
{
	struct bt_hci_evt_le_conn_complete *evt = (void *)buf->data;
	struct bt_hci_evt_le_enh_conn_complete enh;

	BT_DBG("status 0x%02x role %u %s", evt->status, evt->role,
	       bt_addr_le_str(&evt->peer_addr));

	enh.status         = evt->status;
	enh.handle         = evt->handle;
	enh.role           = evt->role;
	enh.interval       = evt->interval;
	enh.latency        = evt->latency;
	enh.supv_timeout   = evt->supv_timeout;
	enh.clock_accuracy = evt->clock_accuracy;

	bt_addr_le_copy(&enh.peer_addr, &evt->peer_addr);

	if (IS_ENABLED(CONFIG_BT_PRIVACY)) {
		bt_addr_copy(&enh.local_rpa, &bt_dev.random_addr.a);
	} else {
		bt_addr_copy(&enh.local_rpa, BT_ADDR_ANY);
	}

	bt_addr_copy(&enh.peer_rpa, BT_ADDR_ANY);

	enh_conn_complete(&enh, false);
}

static void le_remote_feat_complete(struct net_buf *buf)
{
	struct bt_hci_evt_le_remote_feat_complete *evt = (void *)buf->data;
	uint16_t handle = sys_le16_to_cpu(evt->handle);
	struct bt_conn *conn;

	conn = bt_conn_lookup_handle(handle);
	if (!conn) {
		BT_ERR("Unable to lookup conn for handle %u", handle);
		return;
	}

	if (!evt->status) {
		memcpy(conn->le.features, evt->features,
		       sizeof(conn->le.features));
	}

	atomic_set_bit(conn->flags, BT_CONN_AUTO_FEATURE_EXCH);

	if (IS_ENABLED(CONFIG_BT_REMOTE_INFO) &&
	    !IS_ENABLED(CONFIG_BT_REMOTE_VERSION)) {
		notify_remote_info(conn);
	}

	/* Continue with auto-initiated procedures */
	conn_auto_initiate(conn);

	bt_conn_unref(conn);
}

#if defined(CONFIG_BT_DATA_LEN_UPDATE)
static void le_data_len_change(struct net_buf *buf)
{
	struct bt_hci_evt_le_data_len_change *evt = (void *)buf->data;
	uint16_t max_tx_octets = sys_le16_to_cpu(evt->max_tx_octets);
	uint16_t max_rx_octets = sys_le16_to_cpu(evt->max_rx_octets);
	uint16_t max_tx_time = sys_le16_to_cpu(evt->max_tx_time);
	uint16_t max_rx_time = sys_le16_to_cpu(evt->max_rx_time);
	uint16_t handle = sys_le16_to_cpu(evt->handle);
	struct bt_conn *conn;

	conn = bt_conn_lookup_handle(handle);
	if (!conn) {
		BT_ERR("Unable to lookup conn for handle %u", handle);
		return;
	}

	BT_DBG("max. tx: %u (%uus), max. rx: %u (%uus)", max_tx_octets,
	       max_tx_time, max_rx_octets, max_rx_time);

#if defined(CONFIG_BT_USER_DATA_LEN_UPDATE)
	if (IS_ENABLED(CONFIG_BT_AUTO_DATA_LEN_UPDATE)) {
		atomic_set_bit(conn->flags, BT_CONN_AUTO_DATA_LEN_COMPLETE);
	}

	conn->le.data_len.tx_max_len = max_tx_octets;
	conn->le.data_len.tx_max_time = max_tx_time;
	conn->le.data_len.rx_max_len = max_rx_octets;
	conn->le.data_len.rx_max_time = max_rx_time;
	notify_le_data_len_updated(conn);
#endif

	bt_conn_unref(conn);
}
#endif /* CONFIG_BT_DATA_LEN_UPDATE */

#if defined(CONFIG_BT_PHY_UPDATE)
static void le_phy_update_complete(struct net_buf *buf)
{
	struct bt_hci_evt_le_phy_update_complete *evt = (void *)buf->data;
	uint16_t handle = sys_le16_to_cpu(evt->handle);
	struct bt_conn *conn;

	conn = bt_conn_lookup_handle(handle);
	if (!conn) {
		BT_ERR("Unable to lookup conn for handle %u", handle);
		return;
	}

	BT_DBG("PHY updated: status: 0x%02x, tx: %u, rx: %u",
	       evt->status, evt->tx_phy, evt->rx_phy);

	if (IS_ENABLED(CONFIG_BT_AUTO_PHY_UPDATE) &&
	    atomic_test_and_clear_bit(conn->flags, BT_CONN_AUTO_PHY_UPDATE)) {
		atomic_set_bit(conn->flags, BT_CONN_AUTO_PHY_COMPLETE);

		/* Continue with auto-initiated procedures */
		conn_auto_initiate(conn);
	}

#if defined(CONFIG_BT_USER_PHY_UPDATE)
	conn->le.phy.tx_phy = bt_get_phy(evt->tx_phy);
	conn->le.phy.rx_phy = bt_get_phy(evt->rx_phy);
	notify_le_phy_updated(conn);
#endif

	bt_conn_unref(conn);
}
#endif /* CONFIG_BT_PHY_UPDATE */

bool bt_le_conn_params_valid(const struct bt_le_conn_param *param)
{
	/* All limits according to BT Core spec 5.0 [Vol 2, Part E, 7.8.12] */

	if (param->interval_min > param->interval_max ||
	    param->interval_min < 6 || param->interval_max > 3200) {
		return false;
	}

	if (param->latency > 499) {
		return false;
	}

	if (param->timeout < 10 || param->timeout > 3200 ||
	    ((param->timeout * 4U) <=
	     ((1U + param->latency) * param->interval_max))) {
		return false;
	}

	return true;
}

static void le_conn_param_neg_reply(uint16_t handle, uint8_t reason)
{
	struct bt_hci_cp_le_conn_param_req_neg_reply *cp;
	struct net_buf *buf;

	buf = bt_hci_cmd_create(BT_HCI_OP_LE_CONN_PARAM_REQ_NEG_REPLY,
				sizeof(*cp));
	if (!buf) {
		BT_ERR("Unable to allocate buffer");
		return;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->handle = sys_cpu_to_le16(handle);
	cp->reason = sys_cpu_to_le16(reason);

	bt_hci_cmd_send(BT_HCI_OP_LE_CONN_PARAM_REQ_NEG_REPLY, buf);
}

static int le_conn_param_req_reply(uint16_t handle,
				   const struct bt_le_conn_param *param)
{
	struct bt_hci_cp_le_conn_param_req_reply *cp;
	struct net_buf *buf;

	buf = bt_hci_cmd_create(BT_HCI_OP_LE_CONN_PARAM_REQ_REPLY, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	(void)memset(cp, 0, sizeof(*cp));

	cp->handle = sys_cpu_to_le16(handle);
	cp->interval_min = sys_cpu_to_le16(param->interval_min);
	cp->interval_max = sys_cpu_to_le16(param->interval_max);
	cp->latency = sys_cpu_to_le16(param->latency);
	cp->timeout = sys_cpu_to_le16(param->timeout);

	return bt_hci_cmd_send(BT_HCI_OP_LE_CONN_PARAM_REQ_REPLY, buf);
}

static void le_conn_param_req(struct net_buf *buf)
{
	struct bt_hci_evt_le_conn_param_req *evt = (void *)buf->data;
	struct bt_le_conn_param param;
	struct bt_conn *conn;
	uint16_t handle;

	handle = sys_le16_to_cpu(evt->handle);
	param.interval_min = sys_le16_to_cpu(evt->interval_min);
	param.interval_max = sys_le16_to_cpu(evt->interval_max);
	param.latency = sys_le16_to_cpu(evt->latency);
	param.timeout = sys_le16_to_cpu(evt->timeout);

	conn = bt_conn_lookup_handle(handle);
	if (!conn) {
		BT_ERR("Unable to lookup conn for handle %u", handle);
		le_conn_param_neg_reply(handle, BT_HCI_ERR_UNKNOWN_CONN_ID);
		return;
	}

	if (!le_param_req(conn, &param)) {
		le_conn_param_neg_reply(handle, BT_HCI_ERR_INVALID_LL_PARAM);
	} else {
		le_conn_param_req_reply(handle, &param);
	}

	bt_conn_unref(conn);
}

static void le_conn_update_complete(struct net_buf *buf)
{
	struct bt_hci_evt_le_conn_update_complete *evt = (void *)buf->data;
	struct bt_conn *conn;
	uint16_t handle;

	handle = sys_le16_to_cpu(evt->handle);

	BT_DBG("status 0x%02x, handle %u", evt->status, handle);

	conn = bt_conn_lookup_handle(handle);
	if (!conn) {
		BT_ERR("Unable to lookup conn for handle %u", handle);
		return;
	}

	if (!evt->status) {
		conn->le.interval = sys_le16_to_cpu(evt->interval);
		conn->le.latency = sys_le16_to_cpu(evt->latency);
		conn->le.timeout = sys_le16_to_cpu(evt->supv_timeout);
		notify_le_param_updated(conn);
	} else if (evt->status == BT_HCI_ERR_UNSUPP_REMOTE_FEATURE &&
		   conn->role == BT_HCI_ROLE_SLAVE &&
		   !atomic_test_and_set_bit(conn->flags,
					    BT_CONN_SLAVE_PARAM_L2CAP)) {
		/* CPR not supported, let's try L2CAP CPUP instead */
		struct bt_le_conn_param param;

		param.interval_min = conn->le.interval_min;
		param.interval_max = conn->le.interval_max;
		param.latency = conn->le.pending_latency;
		param.timeout = conn->le.pending_timeout;

		bt_l2cap_update_conn_param(conn, &param);
	}

	bt_conn_unref(conn);
}

#if defined(CONFIG_BT_HCI_ACL_FLOW_CONTROL)
static int set_flow_control(void)
{
	struct bt_hci_cp_host_buffer_size *hbs;
	struct net_buf *buf;
	int err;

	/* Check if host flow control is actually supported */
	if (!BT_CMD_TEST(bt_dev.supported_commands, 10, 5)) {
		BT_WARN("Controller to host flow control not supported");
		return 0;
	}

	buf = bt_hci_cmd_create(BT_HCI_OP_HOST_BUFFER_SIZE,
				sizeof(*hbs));
	if (!buf) {
		return -ENOBUFS;
	}

	hbs = net_buf_add(buf, sizeof(*hbs));
	(void)memset(hbs, 0, sizeof(*hbs));
	hbs->acl_mtu = sys_cpu_to_le16(CONFIG_BT_L2CAP_RX_MTU +
				       sizeof(struct bt_l2cap_hdr));
	hbs->acl_pkts = sys_cpu_to_le16(CONFIG_BT_ACL_RX_COUNT);

	err = bt_hci_cmd_send_sync(BT_HCI_OP_HOST_BUFFER_SIZE, buf, NULL);
	if (err) {
		return err;
	}

	buf = bt_hci_cmd_create(BT_HCI_OP_SET_CTL_TO_HOST_FLOW, 1);
	if (!buf) {
		return -ENOBUFS;
	}

	net_buf_add_u8(buf, BT_HCI_CTL_TO_HOST_FLOW_ENABLE);
	return bt_hci_cmd_send_sync(BT_HCI_OP_SET_CTL_TO_HOST_FLOW, buf, NULL);
}
#endif /* CONFIG_BT_HCI_ACL_FLOW_CONTROL */

static void unpair(uint8_t id, const bt_addr_le_t *addr)
{
	struct bt_keys *keys = NULL;
	struct bt_conn *conn = bt_conn_lookup_addr_le(id, addr);

	if (conn) {
		/* Clear the conn->le.keys pointer since we'll invalidate it,
		 * and don't want any subsequent code (like disconnected
		 * callbacks) accessing it.
		 */
		if (conn->type == BT_CONN_TYPE_LE) {
			keys = conn->le.keys;
			conn->le.keys = NULL;
		}

		bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
		bt_conn_unref(conn);
	}

	if (IS_ENABLED(CONFIG_BT_BREDR)) {
		/* LE Public may indicate BR/EDR as well */
		if (addr->type == BT_ADDR_LE_PUBLIC) {
			bt_keys_link_key_clear_addr(&addr->a);
		}
	}

	if (IS_ENABLED(CONFIG_BT_SMP)) {
		if (!keys) {
			keys = bt_keys_find_addr(id, addr);
		}

		if (keys) {
			bt_keys_clear(keys);
		}
	}

	bt_gatt_clear(id, addr);

#if defined(CONFIG_BT_SMP) || defined(CONFIG_BT_BREDR)
	if (le_auth && le_auth->bond_deleted) {
		le_auth->bond_deleted(id, addr);
	}
#endif /* defined(CONFIG_BT_SMP) || defined(CONFIG_BT_BREDR) */
}

static void unpair_remote(const struct bt_bond_info *info, void *data)
{
	uint8_t *id = (uint8_t *) data;

	unpair(*id, &info->addr);
}

int bt_unpair(uint8_t id, const bt_addr_le_t *addr)
{
	if (id >= CONFIG_BT_ID_MAX) {
		return -EINVAL;
	}

	if (IS_ENABLED(CONFIG_BT_SMP) &&
	    (!addr || !bt_addr_le_cmp(addr, BT_ADDR_LE_ANY))) {
		bt_foreach_bond(id, unpair_remote, &id);
		return 0;
	}

	unpair(id, addr);
	return 0;
}

#endif /* CONFIG_BT_CONN */

#if defined(CONFIG_BT_SMP) || defined(CONFIG_BT_BREDR)
enum bt_security_err bt_security_err_get(uint8_t hci_err)
{
	switch (hci_err) {
	case BT_HCI_ERR_SUCCESS:
		return BT_SECURITY_ERR_SUCCESS;
	case BT_HCI_ERR_AUTH_FAIL:
		return BT_SECURITY_ERR_AUTH_FAIL;
	case BT_HCI_ERR_PIN_OR_KEY_MISSING:
		return BT_SECURITY_ERR_PIN_OR_KEY_MISSING;
	case BT_HCI_ERR_PAIRING_NOT_SUPPORTED:
		return BT_SECURITY_ERR_PAIR_NOT_SUPPORTED;
	case BT_HCI_ERR_PAIRING_NOT_ALLOWED:
		return BT_SECURITY_ERR_PAIR_NOT_ALLOWED;
	case BT_HCI_ERR_INVALID_PARAM:
		return BT_SECURITY_ERR_INVALID_PARAM;
	default:
		return BT_SECURITY_ERR_UNSPECIFIED;
	}
}
#endif /* defined(CONFIG_BT_SMP) || defined(CONFIG_BT_BREDR) */

#if defined(CONFIG_BT_BREDR)
static int reject_conn(const bt_addr_t *bdaddr, uint8_t reason)
{
	struct bt_hci_cp_reject_conn_req *cp;
	struct net_buf *buf;
	int err;

	buf = bt_hci_cmd_create(BT_HCI_OP_REJECT_CONN_REQ, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	bt_addr_copy(&cp->bdaddr, bdaddr);
	cp->reason = reason;

	err = bt_hci_cmd_send_sync(BT_HCI_OP_REJECT_CONN_REQ, buf, NULL);
	if (err) {
		return err;
	}

	return 0;
}

static int accept_sco_conn(const bt_addr_t *bdaddr, struct bt_conn *sco_conn)
{
	struct bt_hci_cp_accept_sync_conn_req *cp;
	struct net_buf *buf;
	int err;
	uint8_t codec_id = BT_CODEC_ID_CVSD;

	buf = bt_hci_cmd_create(BT_HCI_OP_ACCEPT_SYNC_CONN_REQ, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	bt_addr_copy(&cp->bdaddr, bdaddr);
	cp->pkt_type = sco_conn->sco.pkt_type;
	cp->tx_bandwidth = 0x00001f40;
	cp->rx_bandwidth = 0x00001f40;

	if (get_hf_codec_func) {
		codec_id = get_hf_codec_func(sco_conn->sco.acl);
	}
	if ((codec_id == BT_CODEC_ID_MSBC) || (codec_id == BT_CODEC_ID_LC3_SWB)) {
		cp->max_latency = 0x0008;
		cp->retrans_effort = 0x02;
		cp->content_format = BT_VOICE_MSBC_16BIT;
	} else {
		cp->max_latency = 0x0007;
		cp->retrans_effort = 0x01;
		cp->content_format = BT_VOICE_CVSD_16BIT;
	}

	err = bt_hci_cmd_send_sync(BT_HCI_OP_ACCEPT_SYNC_CONN_REQ, buf, NULL);
	if (err) {
		return err;
	}

	return 0;
}

static int accept_conn(const bt_addr_t *bdaddr)
{
	struct bt_hci_cp_accept_conn_req *cp;
	struct net_buf *buf;
	int err;

	buf = bt_hci_cmd_create(BT_HCI_OP_ACCEPT_CONN_REQ, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	bt_addr_copy(&cp->bdaddr, bdaddr);
	cp->role = BT_HCI_ROLE_SLAVE;

	err = bt_hci_cmd_send_sync(BT_HCI_OP_ACCEPT_CONN_REQ, buf, NULL);
	if (err) {
		return err;
	}

	return 0;
}

static void bt_esco_conn_req(struct bt_hci_evt_conn_request *evt)
{
	struct bt_conn *sco_conn;

	sco_conn = bt_conn_add_sco(&evt->bdaddr, evt->link_type);
	if (!sco_conn) {
		reject_conn(&evt->bdaddr, BT_HCI_ERR_INSUFFICIENT_RESOURCES);
		return;
	}

	if (accept_sco_conn(&evt->bdaddr, sco_conn)) {
		BT_ERR("Error accepting connection from %s",
		       bt_addr_str(&evt->bdaddr));
		reject_conn(&evt->bdaddr, BT_HCI_ERR_UNSPECIFIED);
		bt_sco_cleanup(sco_conn);
		return;
	}

	sco_conn->role = BT_HCI_ROLE_SLAVE;
	bt_conn_set_state(sco_conn, BT_CONN_CONNECT);
	bt_conn_unref(sco_conn);
}

static void conn_req(struct net_buf *buf)
{
	struct bt_hci_evt_conn_request *evt = (void *)buf->data;
	struct bt_conn *conn;

	BT_DBG("conn req from %s, type 0x%02x", bt_addr_str(&evt->bdaddr),
	       evt->link_type);

#if MATCH_PHOENIX_CONTROLLER
	if (evt->link_type == BT_HCI_SCO) {
		bt_sco_conn_req(evt);
		return;
	}
#endif

	if (evt->link_type != BT_HCI_ACL) {
		bt_esco_conn_req(evt);
		return;
	}

	if (!bt_conn_notify_connect_req(&evt->bdaddr,evt->dev_class)) {
		reject_conn(&evt->bdaddr, BT_HCI_ERR_LOCALHOST_TERM_CONN);
		return;
	}

	conn = bt_conn_add_br(&evt->bdaddr);
	if (!conn) {
		reject_conn(&evt->bdaddr, BT_HCI_ERR_INSUFFICIENT_RESOURCES);
		return;
	}

	accept_conn(&evt->bdaddr);
	conn->role = BT_HCI_ROLE_SLAVE;
	bt_conn_set_state(conn, BT_CONN_CONNECT);
	bt_conn_unref(conn);
}

static bool br_sufficient_key_size(struct bt_conn *conn)
{
	struct bt_hci_cp_read_encryption_key_size *cp;
	struct bt_hci_rp_read_encryption_key_size *rp;
	struct net_buf *buf, *rsp;
	uint8_t key_size;
	int err;

	buf = bt_hci_cmd_create(BT_HCI_OP_READ_ENCRYPTION_KEY_SIZE,
				sizeof(*cp));
	if (!buf) {
		BT_ERR("Failed to allocate command buffer");
		return false;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->handle = sys_cpu_to_le16(conn->handle);

	err = bt_hci_cmd_send_sync(BT_HCI_OP_READ_ENCRYPTION_KEY_SIZE,
				   buf, &rsp);
	if (err) {
		BT_ERR("Failed to read encryption key size (err %d)", err);
		return false;
	}

	if (rsp->len < sizeof(*rp)) {
		BT_ERR("Too small command complete for encryption key size");
		net_buf_unref(rsp);
		return false;
	}

	rp = (void *)rsp->data;
	key_size = rp->key_size;
	net_buf_unref(rsp);

	BT_DBG("Encryption key size is %u", key_size);

	if (conn->sec_level == BT_SECURITY_L4) {
		return key_size == BT_HCI_ENCRYPTION_KEY_SIZE_MAX;
	}

	return key_size >= BT_HCI_ENCRYPTION_KEY_SIZE_MIN;
}

static bool update_sec_level_br(struct bt_conn *conn)
{
	if (!conn->encrypt) {
		conn->sec_level = BT_SECURITY_L1;
		return true;
	}

	if (conn->br.link_key) {
		if (conn->br.link_key->flags & BT_LINK_KEY_AUTHENTICATED) {
			if (conn->encrypt == 0x02) {
				conn->sec_level = BT_SECURITY_L4;
			} else {
				conn->sec_level = BT_SECURITY_L3;
			}
		} else {
			conn->sec_level = BT_SECURITY_L2;
		}
	} else {
		BT_WARN("No BR/EDR link key found");
		conn->sec_level = BT_SECURITY_L2;
	}

	if (!br_sufficient_key_size(conn)) {
		BT_ERR("Encryption key size is not sufficient");
		bt_conn_disconnect(conn, BT_HCI_ERR_AUTH_FAIL);
		return false;
	}

	if (conn->required_sec_level > conn->sec_level) {
		BT_ERR("Failed to set required security level");
		bt_conn_disconnect(conn, BT_HCI_ERR_AUTH_FAIL);
		return false;
	}

	return true;
}

static void synchronous_conn_complete(struct net_buf *buf)
{
	struct bt_hci_evt_sync_conn_complete *evt = (void *)buf->data;
	struct bt_conn *sco_conn;
	uint16_t handle = sys_le16_to_cpu(evt->handle);

	BT_DBG("status 0x%02x, handle %u, type 0x%02x", evt->status, handle,
	       evt->link_type);

	sco_conn = bt_conn_lookup_addr_sco(&evt->bdaddr);
	if (!sco_conn) {
		BT_ERR("Unable to find conn for %s", bt_addr_str(&evt->bdaddr));
		return;
	}

	if (evt->status) {
		sco_conn->err = evt->status;
		bt_conn_set_state(sco_conn, BT_CONN_DISCONNECTED);
		bt_conn_unref(sco_conn);
		return;
	}

	sco_conn->handle = handle;
	bt_conn_set_state(sco_conn, BT_CONN_CONNECTED);
	bt_conn_unref(sco_conn);
}

static void conn_complete(struct net_buf *buf)
{
	struct bt_hci_evt_conn_complete *evt = (void *)buf->data;
	struct bt_conn *conn;
	struct bt_hci_cp_read_remote_features *cp;
	uint16_t handle = sys_le16_to_cpu(evt->handle);

	BT_DBG("status 0x%02x, handle %u, type 0x%02x", evt->status, handle,
	       evt->link_type);

	bt_check_ctrl_active_connect(evt);

    if(evt->link_type == BT_HCI_SCO){
	    conn = bt_conn_lookup_addr_sco(&evt->bdaddr);
    }
    else{
	    conn = bt_conn_lookup_addr_br(&evt->bdaddr);
	}
	if (!conn) {
		BT_ERR("Unable to find conn for %s", bt_addr_str(&evt->bdaddr));
		return;
	}

	if (evt->status) {
		conn->err = evt->status;
		bt_conn_set_state(conn, BT_CONN_DISCONNECTED);
		if (evt->link_type == BT_HCI_SCO) {
            bt_sco_cleanup(conn);
        }
        else {
            bt_conn_unref(conn);
        }
        return;
	}
    if(conn->type == BT_CONN_TYPE_SCO){
        conn->handle = handle;
        bt_conn_set_state(conn, BT_CONN_CONNECTED);
        bt_conn_unref(conn);
        return;
    }

	conn->handle = handle;
	conn->err = 0U;
	conn->encrypt = evt->encr_enabled;

	if (!update_sec_level_br(conn)) {
		bt_conn_unref(conn);
		return;
	}

	bt_conn_set_state(conn, BT_CONN_CONNECTED);
	bt_conn_unref(conn);

	buf = bt_hci_cmd_create(BT_HCI_OP_READ_REMOTE_FEATURES, sizeof(*cp));
	if (!buf) {
		return;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->handle = evt->handle;

	bt_hci_cmd_send(BT_HCI_OP_REMOTE_NAME_REQUEST, buf);
}

#if 0 /* Replace original discovery */
struct discovery_priv {
	uint16_t clock_offset;
	uint8_t pscan_rep_mode;
	uint8_t resolving;
} __packed;

static int request_name(const bt_addr_t *addr, uint8_t pscan, uint16_t offset)
{
	struct bt_hci_cp_remote_name_request *cp;
	struct net_buf *buf;

	buf = bt_hci_cmd_create(BT_HCI_OP_REMOTE_NAME_REQUEST, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));

	bt_addr_copy(&cp->bdaddr, addr);
	cp->pscan_rep_mode = pscan;
	cp->reserved = 0x00; /* reserver, should be set to 0x00 */
	cp->clock_offset = offset;

	return bt_hci_cmd_send_sync(BT_HCI_OP_REMOTE_NAME_REQUEST, buf, NULL);
}

#define EIR_SHORT_NAME		0x08
#define EIR_COMPLETE_NAME	0x09

static bool eir_has_name(const uint8_t *eir)
{
	int len = 240;

	while (len) {
		if (len < 2) {
			break;
		};

		/* Look for early termination */
		if (!eir[0]) {
			break;
		}

		/* Check if field length is correct */
		if (eir[0] > len - 1) {
			break;
		}

		switch (eir[1]) {
		case EIR_SHORT_NAME:
		case EIR_COMPLETE_NAME:
			if (eir[0] > 1) {
				return true;
			}
			break;
		default:
			break;
		}

		/* Parse next AD Structure */
		len -= eir[0] + 1;
		eir += eir[0] + 1;
	}

	return false;
}

static void report_discovery_results(void)
{
	bool resolving_names = false;
	int i;

	for (i = 0; i < discovery_results_count; i++) {
		struct discovery_priv *priv;

		priv = (struct discovery_priv *)&discovery_results[i]._priv;

		if (eir_has_name(discovery_results[i].eir)) {
			continue;
		}

		if (request_name(&discovery_results[i].addr,
				 priv->pscan_rep_mode, priv->clock_offset)) {
			continue;
		}

		priv->resolving = 1U;
		resolving_names = true;
	}

	if (resolving_names) {
		return;
	}

	atomic_clear_bit(bt_dev.flags, BT_DEV_INQUIRY);

	discovery_cb(discovery_results, discovery_results_count);

	discovery_cb = NULL;
	discovery_results = NULL;
	discovery_results_size = 0;
	discovery_results_count = 0;
}

static void inquiry_complete(struct net_buf *buf)
{
	struct bt_hci_evt_inquiry_complete *evt = (void *)buf->data;

	if (evt->status) {
		BT_ERR("Failed to complete inquiry");
	}

	report_discovery_results();
}

static struct bt_br_discovery_result *get_result_slot(const bt_addr_t *addr,
						      int8_t rssi)
{
	struct bt_br_discovery_result *result = NULL;
	size_t i;

	/* check if already present in results */
	for (i = 0; i < discovery_results_count; i++) {
		if (!bt_addr_cmp(addr, &discovery_results[i].addr)) {
			return &discovery_results[i];
		}
	}

	/* Pick a new slot (if available) */
	if (discovery_results_count < discovery_results_size) {
		bt_addr_copy(&discovery_results[discovery_results_count].addr,
			     addr);
		return &discovery_results[discovery_results_count++];
	}

	/* ignore if invalid RSSI */
	if (rssi == 0xff) {
		return NULL;
	}

	/*
	 * Pick slot with smallest RSSI that is smaller then passed RSSI
	 * TODO handle TX if present
	 */
	for (i = 0; i < discovery_results_size; i++) {
		if (discovery_results[i].rssi > rssi) {
			continue;
		}

		if (!result || result->rssi > discovery_results[i].rssi) {
			result = &discovery_results[i];
		}
	}

	if (result) {
		BT_DBG("Reusing slot (old %s rssi %d dBm)",
		       bt_addr_str(&result->addr), result->rssi);

		bt_addr_copy(&result->addr, addr);
	}

	return result;
}

static void inquiry_result_with_rssi(struct net_buf *buf)
{
	uint8_t num_reports = net_buf_pull_u8(buf);

	if (!atomic_test_bit(bt_dev.flags, BT_DEV_INQUIRY)) {
		return;
	}

	BT_DBG("number of results: %u", num_reports);

	while (num_reports--) {
		struct bt_hci_evt_inquiry_result_with_rssi *evt;
		struct bt_br_discovery_result *result;
		struct discovery_priv *priv;

		if (buf->len < sizeof(*evt)) {
			BT_ERR("Unexpected end to buffer");
			return;
		}

		evt = net_buf_pull_mem(buf, sizeof(*evt));
		BT_DBG("%s rssi %d dBm", bt_addr_str(&evt->addr), evt->rssi);

		result = get_result_slot(&evt->addr, evt->rssi);
		if (!result) {
			return;
		}

		priv = (struct discovery_priv *)&result->_priv;
		priv->pscan_rep_mode = evt->pscan_rep_mode;
		priv->clock_offset = evt->clock_offset;

		memcpy(result->cod, evt->cod, 3);
		result->rssi = evt->rssi;

		/* we could reuse slot so make sure EIR is cleared */
		(void)memset(result->eir, 0, sizeof(result->eir));
	}
}

static void extended_inquiry_result(struct net_buf *buf)
{
	struct bt_hci_evt_extended_inquiry_result *evt = (void *)buf->data;
	struct bt_br_discovery_result *result;
	struct discovery_priv *priv;

	if (!atomic_test_bit(bt_dev.flags, BT_DEV_INQUIRY)) {
		return;
	}

	BT_DBG("%s rssi %d dBm", bt_addr_str(&evt->addr), evt->rssi);

	result = get_result_slot(&evt->addr, evt->rssi);
	if (!result) {
		return;
	}

	priv = (struct discovery_priv *)&result->_priv;
	priv->pscan_rep_mode = evt->pscan_rep_mode;
	priv->clock_offset = evt->clock_offset;

	result->rssi = evt->rssi;
	memcpy(result->cod, evt->cod, 3);
	memcpy(result->eir, evt->eir, sizeof(result->eir));
}

static void remote_name_request_complete(struct net_buf *buf)
{
	struct bt_hci_evt_remote_name_req_complete *evt = (void *)buf->data;
	struct bt_br_discovery_result *result;
	struct discovery_priv *priv;
	int eir_len = 240;
	uint8_t *eir;
	int i;

	result = get_result_slot(&evt->bdaddr, 0xff);
	if (!result) {
		return;
	}

	priv = (struct discovery_priv *)&result->_priv;
	priv->resolving = 0U;

	if (evt->status) {
		goto check_names;
	}

	eir = result->eir;

	while (eir_len) {
		if (eir_len < 2) {
			break;
		};

		/* Look for early termination */
		if (!eir[0]) {
			size_t name_len;

			eir_len -= 2;

			/* name is null terminated */
			name_len = strlen((const char *)evt->name);

			if (name_len > eir_len) {
				eir[0] = eir_len + 1;
				eir[1] = EIR_SHORT_NAME;
			} else {
				eir[0] = name_len + 1;
				eir[1] = EIR_SHORT_NAME;
			}

			memcpy(&eir[2], evt->name, eir[0] - 1);

			break;
		}

		/* Check if field length is correct */
		if (eir[0] > eir_len - 1) {
			break;
		}

		/* next EIR Structure */
		eir_len -= eir[0] + 1;
		eir += eir[0] + 1;
	}

check_names:
	/* if still waiting for names */
	for (i = 0; i < discovery_results_count; i++) {
		struct discovery_priv *priv;

		priv = (struct discovery_priv *)&discovery_results[i]._priv;

		if (priv->resolving) {
			return;
		}
	}

	/* all names resolved, report discovery results */
	atomic_clear_bit(bt_dev.flags, BT_DEV_INQUIRY);

	discovery_cb(discovery_results, discovery_results_count);

	discovery_cb = NULL;
	discovery_results = NULL;
	discovery_results_size = 0;
	discovery_results_count = 0;
}
#endif

static void read_remote_features_complete(struct net_buf *buf)
{
	struct bt_hci_evt_remote_features *evt = (void *)buf->data;
	uint16_t handle = sys_le16_to_cpu(evt->handle);
	struct bt_hci_cp_read_remote_ext_features *cp;
	struct bt_conn *conn;

	BT_DBG("status 0x%02x handle %u", evt->status, handle);

	conn = bt_conn_lookup_handle(handle);
	if (!conn) {
		BT_ERR("Can't find conn for handle 0x%x", handle);
		return;
	}

	if (evt->status) {
		goto done;
	}

	memcpy(conn->br.features[0], evt->features, sizeof(evt->features));
	conn->br.esco_pkt_type = device_supported_pkt_type(conn->br.features);

	if (!BT_FEAT_EXT_FEATURES(conn->br.features)) {
		goto done;
	}

	buf = bt_hci_cmd_create(BT_HCI_OP_READ_REMOTE_EXT_FEATURES,
				sizeof(*cp));
	if (!buf) {
		goto done;
	}

	/* Read remote host features (page 1) */
	cp = net_buf_add(buf, sizeof(*cp));
	cp->handle = evt->handle;
	cp->page = 0x01;

	bt_hci_cmd_send_sync(BT_HCI_OP_READ_REMOTE_EXT_FEATURES, buf, NULL);

done:
	bt_conn_unref(conn);
}

static void read_remote_ext_features_complete(struct net_buf *buf)
{
	struct bt_hci_evt_remote_ext_features *evt = (void *)buf->data;
	uint16_t handle = sys_le16_to_cpu(evt->handle);
	struct bt_conn *conn;

	BT_DBG("status 0x%02x handle %u", evt->status, handle);

	conn = bt_conn_lookup_handle(handle);
	if (!conn) {
		BT_ERR("Can't find conn for handle 0x%x", handle);
		return;
	}

	if (!evt->status && evt->page == 0x01) {
		memcpy(conn->br.features[1], evt->features,
		       sizeof(conn->br.features[1]));
	}

	bt_conn_unref(conn);
}

static void role_change(struct net_buf *buf)
{
	struct bt_hci_evt_role_change *evt = (void *)buf->data;
	struct bt_conn *conn;

	BT_DBG("status 0x%02x role %u addr %s", evt->status, evt->role,
	       bt_addr_str(&evt->bdaddr));

	if (evt->status) {
		return;
	}

	conn = bt_conn_lookup_addr_br(&evt->bdaddr);
	if (!conn) {
		BT_ERR("Can't find conn for %s", bt_addr_str(&evt->bdaddr));
		return;
	}

	if (evt->role) {
		conn->role = BT_CONN_ROLE_SLAVE;
	} else {
		conn->role = BT_CONN_ROLE_MASTER;
	}

	bt_conn_notify_role_change(conn, conn->role);
	bt_conn_unref(conn);
}
#endif /* CONFIG_BT_BREDR */

#if defined(CONFIG_BT_SMP)
static bool update_sec_level(struct bt_conn *conn)
{
	if (conn->le.keys && (conn->le.keys->flags & BT_KEYS_AUTHENTICATED)) {
		if (conn->le.keys->flags & BT_KEYS_SC &&
		    conn->le.keys->enc_size == BT_SMP_MAX_ENC_KEY_SIZE) {
			conn->sec_level = BT_SECURITY_L4;
		} else {
			conn->sec_level = BT_SECURITY_L3;
		}
	} else {
		conn->sec_level = BT_SECURITY_L2;
	}

	BT_INFO("sc level:%d,required:%d",conn->sec_level,conn->required_sec_level);

	return !(conn->required_sec_level > conn->sec_level);
}
#endif /* CONFIG_BT_SMP */

#if defined(CONFIG_BT_SMP) || defined(CONFIG_BT_BREDR)
static void hci_encrypt_change(struct net_buf *buf)
{
	struct bt_hci_evt_encrypt_change *evt = (void *)buf->data;
	uint16_t handle = sys_le16_to_cpu(evt->handle);
	uint8_t status = evt->status;
	struct bt_conn *conn;

	BT_DBG("status 0x%02x handle %u encrypt 0x%02x", evt->status, handle,
	       evt->encrypt);

	conn = bt_conn_lookup_handle(handle);
	if (!conn) {
		BT_ERR("Unable to look up conn with handle %u", handle);
		return;
	}

	if (status) {
        if(status == BT_HCI_ERR_LL_PROC_COLLISION){
            conn->security_collision++;
            if(conn->security_collision < 3){
                bt_conn_start_link_security(conn);
                bt_conn_unref(conn);
                return;
            }
        }

		bt_conn_security_changed(conn, status,
					 bt_security_err_get(status));
		bt_conn_unref(conn);
		return;
	}

	conn->encrypt = evt->encrypt;

#if defined(CONFIG_BT_SMP)
	if (conn->type == BT_CONN_TYPE_LE) {
		/*
		 * we update keys properties only on successful encryption to
		 * avoid losing valid keys if encryption was not successful.
		 *
		 * Update keys with last pairing info for proper sec level
		 * update. This is done only for LE transport, for BR/EDR keys
		 * are updated on HCI 'Link Key Notification Event'
		 */
		if (conn->encrypt) {
			bt_smp_update_keys(conn);
		}

		if (!update_sec_level(conn)) {
			status = BT_HCI_ERR_AUTH_FAIL;
		}
	}
#endif /* CONFIG_BT_SMP */
#if defined(CONFIG_BT_BREDR)
	if (conn->type == BT_CONN_TYPE_BR) {
		if (!update_sec_level_br(conn)) {
			bt_conn_unref(conn);
			return;
		}

		if (IS_ENABLED(CONFIG_BT_SMP)) {
			/*
			 * Start SMP over BR/EDR if we are pairing and are
			 * master on the link
			 */
			if (atomic_test_bit(conn->flags, BT_CONN_BR_PAIRING) &&
			    conn->role == BT_CONN_ROLE_MASTER) {
				bt_smp_br_send_pairing_req(conn);
			}
		}
	}
#endif /* CONFIG_BT_BREDR */

	bt_conn_security_changed(conn, status, bt_security_err_get(status));

	if (status) {
		BT_ERR("Failed to set required security level");
		bt_conn_disconnect(conn, status);
	}

	bt_conn_unref(conn);
}

static void hci_encrypt_key_refresh_complete(struct net_buf *buf)
{
	struct bt_hci_evt_encrypt_key_refresh_complete *evt = (void *)buf->data;
	uint8_t status = evt->status;
	struct bt_conn *conn;
	uint16_t handle;

	handle = sys_le16_to_cpu(evt->handle);

	BT_DBG("status 0x%02x handle %u", evt->status, handle);

	conn = bt_conn_lookup_handle(handle);
	if (!conn) {
		BT_ERR("Unable to look up conn with handle %u", handle);
		return;
	}

	if (status) {
		bt_conn_security_changed(conn, status,
					 bt_security_err_get(status));
		bt_conn_unref(conn);
		return;
	}

	/*
	 * Update keys with last pairing info for proper sec level update.
	 * This is done only for LE transport. For BR/EDR transport keys are
	 * updated on HCI 'Link Key Notification Event', therefore update here
	 * only security level based on available keys and encryption state.
	 */
#if defined(CONFIG_BT_SMP)
	if (conn->type == BT_CONN_TYPE_LE) {
		bt_smp_update_keys(conn);

		if (!update_sec_level(conn)) {
			status = BT_HCI_ERR_AUTH_FAIL;
		}
	}
#endif /* CONFIG_BT_SMP */
#if defined(CONFIG_BT_BREDR)
	if (conn->type == BT_CONN_TYPE_BR) {
		if (!update_sec_level_br(conn)) {
			bt_conn_unref(conn);
			return;
		}
	}
#endif /* CONFIG_BT_BREDR */

	bt_conn_security_changed(conn, status, bt_security_err_get(status));
	if (status) {
		BT_ERR("Failed to set required security level");
		bt_conn_disconnect(conn, status);
	}

	bt_conn_unref(conn);
}
#endif /* CONFIG_BT_SMP || CONFIG_BT_BREDR */

#if defined(CONFIG_BT_REMOTE_VERSION)
static void bt_hci_evt_read_remote_version_complete(struct net_buf *buf)
{
	struct bt_hci_evt_remote_version_info *evt;
	struct bt_conn *conn;
	uint16_t handle;

	evt = net_buf_pull_mem(buf, sizeof(*evt));
	handle = sys_le16_to_cpu(evt->handle);
	conn = bt_conn_lookup_handle(handle);
	if (!conn) {
		BT_ERR("No connection for handle %u", handle);
		return;
	}

	if (!evt->status) {
		conn->rv.version = evt->version;
		conn->rv.manufacturer = sys_le16_to_cpu(evt->manufacturer);
		conn->rv.subversion = sys_le16_to_cpu(evt->subversion);
	}

	atomic_set_bit(conn->flags, BT_CONN_AUTO_VERSION_INFO);

	if (IS_ENABLED(CONFIG_BT_REMOTE_INFO)) {
		/* Remote features is already present */
		notify_remote_info(conn);
	}

	/* Continue with auto-initiated procedures */
	conn_auto_initiate(conn);

	bt_conn_unref(conn);
}
#endif /* CONFIG_BT_REMOTE_VERSION */

static void hci_hardware_error(struct net_buf *buf)
{
	struct bt_hci_evt_hardware_error *evt;

	evt = net_buf_pull_mem(buf, sizeof(*evt));

	BT_ERR("Hardware error, hardware code: %d", evt->hardware_code);
}

#if defined(CONFIG_BT_SMP)
static void le_ltk_neg_reply(uint16_t handle)
{
	struct bt_hci_cp_le_ltk_req_neg_reply *cp;
	struct net_buf *buf;

	buf = bt_hci_cmd_create(BT_HCI_OP_LE_LTK_REQ_NEG_REPLY, sizeof(*cp));
	if (!buf) {
		BT_ERR("Out of command buffers");

		return;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->handle = sys_cpu_to_le16(handle);

	bt_hci_cmd_send(BT_HCI_OP_LE_LTK_REQ_NEG_REPLY, buf);
}

static void le_ltk_reply(uint16_t handle, uint8_t *ltk)
{
	struct bt_hci_cp_le_ltk_req_reply *cp;
	struct net_buf *buf;

	buf = bt_hci_cmd_create(BT_HCI_OP_LE_LTK_REQ_REPLY,
				sizeof(*cp));
	if (!buf) {
		BT_ERR("Out of command buffers");
		return;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->handle = sys_cpu_to_le16(handle);
	memcpy(cp->ltk, ltk, sizeof(cp->ltk));

	bt_hci_cmd_send(BT_HCI_OP_LE_LTK_REQ_REPLY, buf);
}

static void le_ltk_request(struct net_buf *buf)
{
	struct bt_hci_evt_le_ltk_request *evt = (void *)buf->data;
	struct bt_conn *conn;
	uint16_t handle;
	uint8_t ltk[16];

	handle = sys_le16_to_cpu(evt->handle);

	BT_DBG("handle %u", handle);

	conn = bt_conn_lookup_handle(handle);
	if (!conn) {
		BT_ERR("Unable to lookup conn for handle %u", handle);
		return;
	}

	if (bt_smp_request_ltk(conn, evt->rand, evt->ediv, ltk)) {
		le_ltk_reply(handle, ltk);
	} else {
		le_ltk_neg_reply(handle);
	}

	bt_conn_unref(conn);
}
#endif /* CONFIG_BT_SMP */

#if defined(CONFIG_BT_ECC)
static void le_pkey_complete(struct net_buf *buf)
{
	struct bt_hci_evt_le_p256_public_key_complete *evt = (void *)buf->data;
	struct bt_pub_key_cb *cb;

	BT_DBG("status: 0x%02x", evt->status);

	atomic_clear_bit(bt_dev.flags, BT_DEV_PUB_KEY_BUSY);

	if (!evt->status) {
		memcpy(pub_key, evt->key, 64);
		atomic_set_bit(bt_dev.flags, BT_DEV_HAS_PUB_KEY);
	}

	for (cb = pub_key_cb; cb; cb = cb->_next) {
		cb->func(evt->status ? NULL : pub_key);
	}

	pub_key_cb = NULL;
}

static void le_dhkey_complete(struct net_buf *buf)
{
	struct bt_hci_evt_le_generate_dhkey_complete *evt = (void *)buf->data;

	BT_DBG("status: 0x%02x", evt->status);

	if (dh_key_cb) {
		bt_dh_key_cb_t cb = dh_key_cb;

		dh_key_cb = NULL;
		cb(evt->status ? NULL : evt->dhkey);
	}
}
#endif /* CONFIG_BT_ECC */

static void hci_reset_complete(struct net_buf *buf)
{
	uint8_t status = buf->data[0];
	atomic_t flags;

	BT_DBG("status 0x%02x", status);

	if (status) {
		return;
	}

	if (IS_ENABLED(CONFIG_BT_OBSERVER)) {
		bt_scan_reset();
	}

#if defined(CONFIG_BT_BREDR)
	discovery_cb = NULL;
	remote_name_cb = NULL;
#if 0	/* Replace original discovery */

	discovery_results = NULL;
	discovery_results_size = 0;
	discovery_results_count = 0;
#endif
#endif /* CONFIG_BT_BREDR */

	flags = (atomic_get(bt_dev.flags) & BT_DEV_PERSISTENT_FLAGS);
	atomic_set(bt_dev.flags, flags);
}

static void hci_cmd_done(uint16_t opcode, uint8_t status, struct net_buf *buf)
{
	BT_DBG("opcode 0x%04x status 0x%02x buf %p", opcode, status, buf);

	if (net_buf_pool_get(buf->pool_id) != &hci_cmd_pool) {
		BT_WARN("opcode 0x%04x pool id %u pool %p != &hci_cmd_pool %p",
			opcode, buf->pool_id, net_buf_pool_get(buf->pool_id),
			&hci_cmd_pool);
		return;
	}

	if (cmd(buf)->opcode != opcode) {
		BT_WARN("OpCode 0x%04x completed instead of expected 0x%04x",
			opcode, cmd(buf)->opcode);
	}

	if (cmd(buf)->state && !status) {
		struct bt_hci_cmd_state_set *update = cmd(buf)->state;

		atomic_set_bit_to(update->target, update->bit, update->val);
	}

	if (cmd(buf)->event_cb) {
		cmd(buf)->event_cb(status, buf->data, buf->len);
	}

	/* If the command was synchronous wake up bt_hci_cmd_send_sync() */
	if (cmd(buf)->sync) {
		cmd(buf)->status = status;
		k_sem_give(cmd(buf)->sync);
	}
}

static void hci_cmd_complete(struct net_buf *buf)
{
	struct bt_hci_evt_cmd_complete *evt;
	uint8_t status, ncmd;
	uint16_t opcode;

	evt = net_buf_pull_mem(buf, sizeof(*evt));
	ncmd = evt->ncmd;
	opcode = sys_le16_to_cpu(evt->opcode);

	BT_DBG("opcode 0x%04x", opcode);

	/* All command return parameters have a 1-byte status in the
	 * beginning, so we can safely make this generalization.
	 */
	status = buf->data[0];

	hci_cmd_done(opcode, status, buf);

	/* Allow next command to be sent */
	if (ncmd) {
		k_sem_give(&bt_dev.ncmd_sem);
		if (atomic_test_and_clear_bit(bt_dev.flags, BT_DEV_WAIT_NCMD_SEM)) {
			k_poll_signal(&bt_dev.ncmd_signal, 0);
		}
	}
}

static void hci_cmd_status(struct net_buf *buf)
{
	struct bt_hci_evt_cmd_status *evt;
	uint16_t opcode;
	uint8_t ncmd;

	evt = net_buf_pull_mem(buf, sizeof(*evt));
	opcode = sys_le16_to_cpu(evt->opcode);
	ncmd = evt->ncmd;

	BT_DBG("opcode 0x%04x", opcode);

	hci_cmd_done(opcode, evt->status, buf);

	/* Allow next command to be sent */
	if (ncmd) {
		k_sem_give(&bt_dev.ncmd_sem);
		if (atomic_test_and_clear_bit(bt_dev.flags, BT_DEV_WAIT_NCMD_SEM)) {
			k_poll_signal(&bt_dev.ncmd_signal, 0);
		}
	}
}

int bt_hci_get_conn_handle(const struct bt_conn *conn, uint16_t *conn_handle)
{
	if (conn->state != BT_CONN_CONNECTED) {
		return -ENOTCONN;
	}

	*conn_handle = conn->handle;
	return 0;
}

#if defined(CONFIG_BT_EXT_ADV)
int bt_hci_get_adv_handle(const struct bt_le_ext_adv *adv, uint8_t *adv_handle)
{
	if (!atomic_test_bit(adv->flags, BT_ADV_CREATED)) {
		return -EINVAL;
	}

	*adv_handle = adv->handle;
	return 0;
}
#endif /* CONFIG_BT_EXT_ADV */

#if defined(CONFIG_BT_HCI_VS_EVT_USER)
int bt_hci_register_vnd_evt_cb(bt_hci_vnd_evt_cb_t cb)
{
	hci_vnd_evt_cb = cb;
	return 0;
}
#endif /* CONFIG_BT_HCI_VS_EVT_USER */

#ifdef CONFIG_ACTS_BT_HCI_VS
static const struct event_handler vendor_event[] = {
	EVENT_HANDLER(BT_HCI_EVT_VS_ACL_SNOOP_LINK_COMPLETE, vs_evt_acl_snoop_link_complete,
			  sizeof(struct bt_hci_evt_vs_acl_snoop_link_complete)),
	EVENT_HANDLER(BT_HCI_EVT_VS_SNOOP_LINK_DISCONNECT, vs_evt_snoop_link_disconnect,
			  sizeof(struct bt_hci_evt_vs_snoop_link_disconnect)),
	EVENT_HANDLER(BT_HCI_EVT_VS_SYNC_SNOOP_LINK_COMPLETE, vs_evt_sync_snoop_link_complete,
			  sizeof(struct bt_hci_evt_vs_sync_snoop_link_complete)),
	EVENT_HANDLER(BT_HCI_EVT_VS_SNOOP_SWITCH_COMPLETE, vs_evt_snoop_switch_complete,
			  sizeof(struct bt_hci_evt_vs_snoop_switch_complete)),
	EVENT_HANDLER(BT_HCI_EVT_VS_SNOOP_MODE_CHANGE, vs_evt_snoop_mode_change,
			  sizeof(struct bt_hci_evt_vs_snoop_mode_change)),
	EVENT_HANDLER(BT_HCI_EVT_VS_LL_CONN_SWITCH_COMPLETE, vs_evt_ll_conn_switch_complete,
			  sizeof(struct bt_hci_evt_vs_ll_conn_switch_complete)),
	EVENT_HANDLER(BT_HCI_EVT_VS_TWS_MATCH_REPORT, vs_evt_tws_match_report,
			  sizeof(struct bt_hci_evt_vs_tws_match_report)),
	EVENT_HANDLER(BT_HCI_EVT_VS_SNOOP_ACTIVE_LINK, vs_evt_snoop_active_link,
			  sizeof(struct bt_hci_evt_vs_snoop_active_link)),
	EVENT_HANDLER(BT_HCI_EVT_VS_SYNC_1ST_PKT_CHG, vs_evt_sync_1st_pkt_chg,
			  sizeof(struct bt_hci_evt_vs_sync_1st_pkt_chg)),
	EVENT_HANDLER(BT_HCI_EVT_VS_TWS_LOCAL_EXIT_SNIFF, vs_evt_tws_local_exit_sniff, 0),
	EVENT_HANDLER(BT_HCI_EVT_VS_TWS_LOCAL_EXIT_SNIFF_TMP, vs_evt_tws_local_exit_sniff, 0),
	EVENT_HANDLER(BT_HCI_EVT_VS_READ_BB_REG_REPORT, vs_read_bb_reg_report,
			  sizeof(struct bt_hci_rp_vs_read_bb_reg_report)),
	EVENT_HANDLER(BT_HCI_EVT_VS_READ_RF_REG_REPORT, vs_read_rf_reg_report,
			  sizeof(struct bt_hci_rp_vs_read_rf_reg_report)),
#if defined(CONFIG_BT_ISO_UNICAST)
	EVENT_HANDLER(BT_HCI_EVT_LE_VS_CIS_RX_FEATURES, hci_le_vs_cis_rx_features,
			  sizeof(struct bt_hci_evt_le_vs_cis_rx_features)),
#endif /* CONFIG_BT_ISO_UNICAST */

    EVENT_HANDLER(BT_HCI_EVT_PA_SEND_COMPLETE, vs_evt_pa_send_complete,
			  sizeof(struct bt_hci_pa_send_complete)),

};
#endif

static void hci_vendor_event(struct net_buf *buf)
{
	bool handled = false;

#if defined(CONFIG_ACTS_BT_HCI_VS)
	struct bt_hci_evt_vs *evt;

	evt = net_buf_pull_mem(buf, sizeof(*evt));

	BT_DBG("subevent 0x%02x", evt->subevent);

	handle_event(evt->subevent, buf, vendor_event, ARRAY_SIZE(vendor_event));

	return;
#endif

#if defined(CONFIG_BT_HCI_VS_EVT_USER)
	if (hci_vnd_evt_cb) {
		struct net_buf_simple_state state;

		net_buf_simple_save(&buf->b, &state);

		handled = hci_vnd_evt_cb(&buf->b);

		net_buf_simple_restore(&buf->b, &state);
	}
#endif /* CONFIG_BT_HCI_VS_EVT_USER */

	if (IS_ENABLED(CONFIG_BT_HCI_VS_EXT) && !handled) {
		/* do nothing at present time */
		BT_WARN("Unhandled vendor-specific event: %s",
			bt_hex(buf->data, buf->len));
	}
}

static const struct event_handler meta_events[] = {
#if defined(CONFIG_BT_OBSERVER)
	EVENT_HANDLER(BT_HCI_EVT_LE_ADVERTISING_REPORT, bt_hci_le_adv_report,
		      sizeof(struct bt_hci_evt_le_advertising_report)),
#endif /* CONFIG_BT_OBSERVER */
#if defined(CONFIG_BT_CONN)
	EVENT_HANDLER(BT_HCI_EVT_LE_CONN_COMPLETE, le_legacy_conn_complete,
		      sizeof(struct bt_hci_evt_le_conn_complete)),
	EVENT_HANDLER(BT_HCI_EVT_LE_ENH_CONN_COMPLETE, le_enh_conn_complete,
		      sizeof(struct bt_hci_evt_le_enh_conn_complete)),
	EVENT_HANDLER(BT_HCI_EVT_LE_CONN_UPDATE_COMPLETE,
		      le_conn_update_complete,
		      sizeof(struct bt_hci_evt_le_conn_update_complete)),
	EVENT_HANDLER(BT_HCI_EVT_LE_REMOTE_FEAT_COMPLETE,
		      le_remote_feat_complete,
		      sizeof(struct bt_hci_evt_le_remote_feat_complete)),
	EVENT_HANDLER(BT_HCI_EVT_LE_CONN_PARAM_REQ, le_conn_param_req,
		      sizeof(struct bt_hci_evt_le_conn_param_req)),
#if defined(CONFIG_BT_DATA_LEN_UPDATE)
	EVENT_HANDLER(BT_HCI_EVT_LE_DATA_LEN_CHANGE, le_data_len_change,
		      sizeof(struct bt_hci_evt_le_data_len_change)),
#endif /* CONFIG_BT_DATA_LEN_UPDATE */
#if defined(CONFIG_BT_PHY_UPDATE)
	EVENT_HANDLER(BT_HCI_EVT_LE_PHY_UPDATE_COMPLETE,
		      le_phy_update_complete,
		      sizeof(struct bt_hci_evt_le_phy_update_complete)),
#endif /* CONFIG_BT_PHY_UPDATE */
#endif /* CONFIG_BT_CONN */
#if defined(CONFIG_BT_SMP)
	EVENT_HANDLER(BT_HCI_EVT_LE_LTK_REQUEST, le_ltk_request,
		      sizeof(struct bt_hci_evt_le_ltk_request)),
#endif /* CONFIG_BT_SMP */
#if defined(CONFIG_BT_ECC)
	EVENT_HANDLER(BT_HCI_EVT_LE_P256_PUBLIC_KEY_COMPLETE, le_pkey_complete,
		      sizeof(struct bt_hci_evt_le_p256_public_key_complete)),
	EVENT_HANDLER(BT_HCI_EVT_LE_GENERATE_DHKEY_COMPLETE, le_dhkey_complete,
		      sizeof(struct bt_hci_evt_le_generate_dhkey_complete)),
#endif /* CONFIG_BT_SMP */
#if defined(CONFIG_BT_EXT_ADV)
#if defined(CONFIG_BT_BROADCASTER)
	EVENT_HANDLER(BT_HCI_EVT_LE_ADV_SET_TERMINATED, bt_hci_le_adv_set_terminated,
		      sizeof(struct bt_hci_evt_le_adv_set_terminated)),
	EVENT_HANDLER(BT_HCI_EVT_LE_SCAN_REQ_RECEIVED, bt_hci_le_scan_req_received,
		      sizeof(struct bt_hci_evt_le_scan_req_received)),
#endif
#if defined(CONFIG_BT_OBSERVER)
	EVENT_HANDLER(BT_HCI_EVT_LE_SCAN_TIMEOUT, bt_hci_le_scan_timeout,
		      0),
	EVENT_HANDLER(BT_HCI_EVT_LE_EXT_ADVERTISING_REPORT, bt_hci_le_adv_ext_report,
		      sizeof(struct bt_hci_evt_le_ext_advertising_report)),
#endif /* defined(CONFIG_BT_OBSERVER) */
#if defined(CONFIG_BT_PER_ADV_SYNC)
	EVENT_HANDLER(BT_HCI_EVT_LE_PER_ADV_SYNC_ESTABLISHED,
		      bt_hci_le_per_adv_sync_established,
		      sizeof(struct bt_hci_evt_le_per_adv_sync_established)),
	EVENT_HANDLER(BT_HCI_EVT_LE_PER_ADVERTISING_REPORT, bt_hci_le_per_adv_report,
		      sizeof(struct bt_hci_evt_le_per_advertising_report)),
	EVENT_HANDLER(BT_HCI_EVT_LE_PER_ADV_SYNC_LOST, bt_hci_le_per_adv_sync_lost,
		      sizeof(struct bt_hci_evt_le_per_adv_sync_lost)),
#if defined(CONFIG_BT_CONN)
	EVENT_HANDLER(BT_HCI_EVT_LE_PAST_RECEIVED, bt_hci_le_past_received,
		      sizeof(struct bt_hci_evt_le_past_received)),
#endif /* CONFIG_BT_CONN */
#endif /* defined(CONFIG_BT_PER_ADV_SYNC) */
#endif /* defined(CONFIG_BT_EXT_ADV) */
#if defined(CONFIG_BT_ISO_UNICAST)
	EVENT_HANDLER(BT_HCI_EVT_LE_CIS_ESTABLISHED, hci_le_cis_established,
		      sizeof(struct bt_hci_evt_le_cis_established)),
	EVENT_HANDLER(BT_HCI_EVT_LE_CIS_REQ, hci_le_cis_req,
		      sizeof(struct bt_hci_evt_le_cis_req)),
#endif /* (CONFIG_BT_ISO_UNICAST) */
#if defined(CONFIG_BT_ISO_BROADCASTER)
	EVENT_HANDLER(BT_HCI_EVT_LE_BIG_COMPLETE,
		      hci_le_big_complete,
		      sizeof(struct bt_hci_evt_le_big_complete)),
	EVENT_HANDLER(BT_HCI_EVT_LE_BIG_TERMINATE,
		      hci_le_big_terminate,
		      sizeof(struct bt_hci_evt_le_big_terminate)),
#endif /* CONFIG_BT_ISO_BROADCASTER */
#if defined(CONFIG_BT_ISO_SYNC_RECEIVER)
	EVENT_HANDLER(BT_HCI_EVT_LE_BIG_SYNC_ESTABLISHED,
		      hci_le_big_sync_established,
		      sizeof(struct bt_hci_evt_le_big_sync_established)),
	EVENT_HANDLER(BT_HCI_EVT_LE_BIG_SYNC_LOST,
		      hci_le_big_sync_lost,
		      sizeof(struct bt_hci_evt_le_big_sync_lost)),
	EVENT_HANDLER(BT_HCI_EVT_LE_BIGINFO_ADV_REPORT,
		      bt_hci_le_biginfo_adv_report,
		      sizeof(struct bt_hci_evt_le_biginfo_adv_report)),
#endif /* CONFIG_BT_ISO_SYNC_RECEIVER */
#if defined(CONFIG_BT_DF_CONNECTIONLESS_CTE_RX)
	EVENT_HANDLER(BT_HCI_EVT_LE_CONNECTIONLESS_IQ_REPORT, bt_hci_le_df_connectionless_iq_report,
		      sizeof(struct bt_hci_evt_le_connectionless_iq_report)),
#endif /* CONFIG_BT_DF_CONNECTIONLESS_CTE_RX */
#if defined(CONFIG_BT_DF_CONNECTION_CTE_RX)
	EVENT_HANDLER(BT_HCI_EVT_LE_CONNECTION_IQ_REPORT, bt_hci_le_df_connection_iq_report,
		      sizeof(struct bt_hci_evt_le_connection_iq_report)),
#endif /* CONFIG_BT_DF_CONNECTION_CTE_RX */
#if defined(CONFIG_BT_DF_CONNECTION_CTE_REQ)
	EVENT_HANDLER(BT_HCI_EVT_LE_CTE_REQUEST_FAILED, bt_hci_le_df_cte_req_failed,
		      sizeof(struct bt_hci_evt_le_cte_req_failed)),
#endif /* CONFIG_BT_DF_CONNECTION_CTE_REQ */
#if defined(CONFIG_BT_PER_ADV_SYNC_RSP)
	EVENT_HANDLER(BT_HCI_EVT_LE_PER_ADVERTISING_REPORT_V2, bt_hci_le_per_adv_report_v2,
		      sizeof(struct bt_hci_evt_le_per_advertising_report_v2)),
	EVENT_HANDLER(BT_HCI_EVT_LE_PAST_RECEIVED_V2, bt_hci_le_past_received_v2,
		      sizeof(struct bt_hci_evt_le_past_received_v2)),
	EVENT_HANDLER(BT_HCI_EVT_LE_PER_ADV_SYNC_ESTABLISHED_V2,
		      bt_hci_le_per_adv_sync_established_v2,
		      sizeof(struct bt_hci_evt_le_per_adv_sync_established_v2)),
#endif /* CONFIG_BT_PER_ADV_SYNC_RSP */
#if defined(CONFIG_BT_PER_ADV_RSP)
	EVENT_HANDLER(BT_HCI_EVT_LE_PER_ADV_SUBEVENT_DATA_REQUEST,
		      bt_hci_le_per_adv_subevent_data_request,
		      sizeof(struct bt_hci_evt_le_per_adv_subevent_data_request)),
	EVENT_HANDLER(BT_HCI_EVT_LE_PER_ADV_RESPONSE_REPORT, bt_hci_le_per_adv_response_report,
		      sizeof(struct bt_hci_evt_le_per_adv_response_report)),
#endif /* CONFIG_BT_PER_ADV_RSP */
#if defined(CONFIG_BT_CONN)
#if defined(CONFIG_BT_PER_ADV_RSP) || defined(CONFIG_BT_PER_ADV_SYNC_RSP)
	EVENT_HANDLER(BT_HCI_EVT_LE_ENH_CONN_COMPLETE_V2, le_enh_conn_complete_v2,
		      sizeof(struct bt_hci_evt_le_enh_conn_complete_v2)),
#endif /* CONFIG_BT_PER_ADV_RSP || CONFIG_BT_PER_ADV_SYNC_RSP */
#endif /* CONFIG_BT_CONN */

};

static void hci_le_meta_event(struct net_buf *buf)
{
	struct bt_hci_evt_le_meta_event *evt;

	evt = net_buf_pull_mem(buf, sizeof(*evt));

	BT_DBG("subevent 0x%02x", evt->subevent);

	handle_event(evt->subevent, buf, meta_events, ARRAY_SIZE(meta_events));
}

static const struct event_handler normal_events[] = {
	EVENT_HANDLER(BT_HCI_EVT_VENDOR, hci_vendor_event,
		      sizeof(struct bt_hci_evt_vs)),
	EVENT_HANDLER(BT_HCI_EVT_LE_META_EVENT, hci_le_meta_event,
		      sizeof(struct bt_hci_evt_le_meta_event)),
#if defined(CONFIG_BT_BREDR)
	EVENT_HANDLER(BT_HCI_EVT_CONN_REQUEST, conn_req,
		      sizeof(struct bt_hci_evt_conn_request)),
	EVENT_HANDLER(BT_HCI_EVT_CONN_COMPLETE, conn_complete,
		      sizeof(struct bt_hci_evt_conn_complete)),
	EVENT_HANDLER(BT_HCI_EVT_PIN_CODE_REQ, hci_evt_pin_code_req,
		      sizeof(struct bt_hci_evt_pin_code_req)),
	EVENT_HANDLER(BT_HCI_EVT_LINK_KEY_NOTIFY, hci_evt_link_key_notify,
		      sizeof(struct bt_hci_evt_link_key_notify)),
	EVENT_HANDLER(BT_HCI_EVT_LINK_KEY_REQ, hci_evt_link_key_req,
		      sizeof(struct bt_hci_evt_link_key_req)),
	EVENT_HANDLER(BT_HCI_EVT_IO_CAPA_RESP, hci_evt_io_capa_resp,
		      sizeof(struct bt_hci_evt_io_capa_resp)),
	EVENT_HANDLER(BT_HCI_EVT_IO_CAPA_REQ, hci_evt_io_capa_req,
		      sizeof(struct bt_hci_evt_io_capa_req)),
	EVENT_HANDLER(BT_HCI_EVT_SSP_COMPLETE, hci_evt_ssp_complete,
		      sizeof(struct bt_hci_evt_ssp_complete)),
	EVENT_HANDLER(BT_HCI_EVT_USER_CONFIRM_REQ, hci_evt_user_confirm_req,
		      sizeof(struct bt_hci_evt_user_confirm_req)),
	EVENT_HANDLER(BT_HCI_EVT_USER_PASSKEY_NOTIFY,
		      hci_evt_user_passkey_notify,
		      sizeof(struct bt_hci_evt_user_passkey_notify)),
	EVENT_HANDLER(BT_HCI_EVT_USER_PASSKEY_REQ, hci_evt_user_passkey_req,
		      sizeof(struct bt_hci_evt_user_passkey_req)),
	EVENT_HANDLER(BT_HCI_EVT_INQUIRY_COMPLETE, inquiry_complete,
		      sizeof(struct bt_hci_evt_inquiry_complete)),
	EVENT_HANDLER(BT_HCI_EVT_INQUIRY_RESULT_WITH_RSSI,
		      inquiry_result_with_rssi,
		      sizeof(struct bt_hci_evt_inquiry_result_with_rssi)),
	EVENT_HANDLER(BT_HCI_EVT_EXTENDED_INQUIRY_RESULT,
		      extended_inquiry_result,
		      sizeof(struct bt_hci_evt_extended_inquiry_result)),
	EVENT_HANDLER(BT_HCI_EVT_REMOTE_NAME_REQ_COMPLETE,
		      remote_name_request_complete,
		      sizeof(struct bt_hci_evt_remote_name_req_complete)),
	EVENT_HANDLER(BT_HCI_EVT_AUTH_COMPLETE, hci_evt_auth_complete,
		      sizeof(struct bt_hci_evt_auth_complete)),
	EVENT_HANDLER(BT_HCI_EVT_REMOTE_FEATURES,
		      read_remote_features_complete,
		      sizeof(struct bt_hci_evt_remote_features)),
	EVENT_HANDLER(BT_HCI_EVT_REMOTE_EXT_FEATURES,
		      read_remote_ext_features_complete,
		      sizeof(struct bt_hci_evt_remote_ext_features)),
	EVENT_HANDLER(BT_HCI_EVT_ROLE_CHANGE, role_change,
		      sizeof(struct bt_hci_evt_role_change)),
	EVENT_HANDLER(BT_HCI_EVT_SYNC_CONN_COMPLETE, synchronous_conn_complete,
		      sizeof(struct bt_hci_evt_sync_conn_complete)),
#endif /* CONFIG_BT_BREDR */
#if defined(CONFIG_BT_CONN)
	EVENT_HANDLER(BT_HCI_EVT_DISCONN_COMPLETE, hci_disconn_complete,
		      sizeof(struct bt_hci_evt_disconn_complete)),
#endif /* CONFIG_BT_CONN */
#if defined(CONFIG_BT_SMP) || defined(CONFIG_BT_BREDR)
	EVENT_HANDLER(BT_HCI_EVT_ENCRYPT_CHANGE, hci_encrypt_change,
		      sizeof(struct bt_hci_evt_encrypt_change)),
	EVENT_HANDLER(BT_HCI_EVT_ENCRYPT_KEY_REFRESH_COMPLETE,
		      hci_encrypt_key_refresh_complete,
		      sizeof(struct bt_hci_evt_encrypt_key_refresh_complete)),
#endif /* CONFIG_BT_SMP || CONFIG_BT_BREDR */
#if defined(CONFIG_BT_REMOTE_VERSION)
	EVENT_HANDLER(BT_HCI_EVT_REMOTE_VERSION_INFO,
		      bt_hci_evt_read_remote_version_complete,
		      sizeof(struct bt_hci_evt_remote_version_info)),
#endif /* CONFIG_BT_REMOTE_VERSION */
	EVENT_HANDLER(BT_HCI_EVT_HARDWARE_ERROR, hci_hardware_error,
		      sizeof(struct bt_hci_evt_hardware_error)),
	/* Actions add start */
#if defined(CONFIG_BT_BREDR)
		EVENT_HANDLER(BT_HCI_EVT_MODE_CHANGE, mode_change,
				sizeof(struct bt_hci_evt_mode_change)),
#endif
	/* CSB */
	EVENT_HANDLER(BT_HCI_EVT_SYNC_TRAIN_RECEIVE, hci_sync_train_receive,
			sizeof(struct bt_hci_evt_sync_train_receive)),
	EVENT_HANDLER(BT_HCI_EVT_CSB_RECEIVE, hci_csb_receive,
			sizeof(struct bt_hci_evt_csb_receive)),
	EVENT_HANDLER(BT_HCI_EVT_CSB_TIMEOUT, hci_csb_timeout,
			sizeof(struct bt_hci_evt_csb_timeout)),
	/* Actions add end */
};

static void hci_event(struct net_buf *buf)
{
	struct bt_hci_evt_hdr *hdr;

	BT_ASSERT(buf->len >= sizeof(*hdr));

	hdr = net_buf_pull_mem(buf, sizeof(*hdr));
	BT_DBG("event 0x%02x", hdr->evt);
	BT_ASSERT(bt_hci_evt_get_flags(hdr->evt) & BT_HCI_EVT_FLAG_RECV);

	handle_event(hdr->evt, buf, normal_events, ARRAY_SIZE(normal_events));

	net_buf_unref(buf);
}

static void send_cmd(void)
{
	struct net_buf *buf;
	int err;
#if defined(CONFIG_NET_BUF_POOL_USAGE)
	uint32_t timestamp;
#endif

	/* Not block  */
	BT_DBG("calling sem_take_wait");
	/* k_sem_take(&bt_dev.ncmd_sem, K_FOREVER); */
	if (k_sem_take(&bt_dev.ncmd_sem, K_NO_WAIT) < 0) {
		atomic_set_bit(bt_dev.flags, BT_DEV_WAIT_NCMD_SEM);
		return;
	}

	/* Get next command */
	BT_DBG("calling net_buf_get");
	buf = net_buf_get(&bt_dev.cmd_tx_queue, K_NO_WAIT);
	BT_ASSERT(buf);

#if defined(CONFIG_NET_BUF_POOL_USAGE)
	timestamp = k_uptime_get_32();
	if ((timestamp - buf->timestamp) > NET_BUF_TIMESTAMP_CHECK_TIME) {
		BT_WARN("Tx cmd_tx_queue time:%d ms", (timestamp - buf->timestamp));
	}
#endif

	/* Clear out any existing sent command */
	if (bt_dev.sent_cmd) {
		BT_ERR("Uncleared pending sent_cmd");
		net_buf_unref(bt_dev.sent_cmd);
		bt_dev.sent_cmd = NULL;
	}

	bt_dev.sent_cmd = net_buf_ref(buf);

	BT_DBG("Sending command 0x%04x (buf %p) to driver",
	       cmd(buf)->opcode, buf);

	err = bt_send(buf);
	if (err) {
		BT_ERR("Unable to send to driver (err %d)", err);
		k_sem_give(&bt_dev.ncmd_sem);
		hci_cmd_done(cmd(buf)->opcode, BT_HCI_ERR_UNSPECIFIED, buf);
		net_buf_unref(bt_dev.sent_cmd);
		bt_dev.sent_cmd = NULL;
		net_buf_unref(buf);
	}
}

static void process_events(struct k_poll_event *ev, int count)
{
	BT_DBG("count %d", count);

	for (; count; ev++, count--) {
		BT_DBG("ev->state %u", ev->state);

		switch (ev->state) {
		case K_POLL_STATE_SIGNALED:
			break;
		case K_POLL_STATE_FIFO_DATA_AVAILABLE:
			if (ev->tag == BT_EVENT_CMD_TX) {
				send_cmd();
			} else if (IS_ENABLED(CONFIG_BT_CONN) ||
				   IS_ENABLED(CONFIG_BT_ISO)) {
				struct bt_conn *conn;

				if (ev->tag == BT_EVENT_CONN_TX_QUEUE) {
					conn = CONTAINER_OF(ev->fifo,
							    struct bt_conn,
							    tx_queue);
					bt_conn_process_tx(conn);
				}
			}
			break;
		case K_POLL_STATE_NOT_READY:
			break;
		default:
			BT_WARN("Unexpected k_poll event state %u", ev->state);
			break;
		}
	}
}

#if defined(CONFIG_BT_CONN)
#if defined(CONFIG_BT_ISO)
/* command FIFO + conn_change signal + MAX_CONN + ISO_MAX_CHAN */
#define EV_COUNT (2 + CONFIG_BT_MAX_CONN + CONFIG_BT_ISO_MAX_CHAN)
#else
/* command FIFO + conn_change signal + MAX_CONN */
#define EV_COUNT (2 + CONFIG_BT_MAX_CONN)
#endif /* CONFIG_BT_ISO */
#else
#if defined(CONFIG_BT_ISO)
/* command FIFO + conn_change signal + ISO_MAX_CHAN */
#define EV_COUNT (2 + CONFIG_BT_ISO_MAX_CHAN)
#else
/* command FIFO */
#define EV_COUNT 1
#endif /* CONFIG_BT_ISO */
#endif /* CONFIG_BT_CONN */

static void hci_tx_thread(void *p1, void *p2, void *p3)
{
	static struct k_poll_event events[EV_COUNT];

	BT_DBG("Started");

	while (1) {
		int ev_count, err;

		if (atomic_test_bit(bt_dev.flags, BT_DEV_WAIT_NCMD_SEM)) {
			bt_dev.ncmd_signal.signaled = 0;
			k_poll_event_init(&events[0], K_POLL_TYPE_SIGNAL,
					K_POLL_MODE_NOTIFY_ONLY, &bt_dev.ncmd_signal);
		} else {
			k_poll_event_init(&events[0], K_POLL_TYPE_FIFO_DATA_AVAILABLE,
					K_POLL_MODE_NOTIFY_ONLY, &bt_dev.cmd_tx_queue);
			events[0].tag = BT_EVENT_CMD_TX;
		}

		ev_count = 1;

		if (IS_ENABLED(CONFIG_BT_CONN) || IS_ENABLED(CONFIG_BT_ISO)) {
			ev_count += bt_conn_prepare_events(&events[1]);
		}

		BT_DBG("Calling k_poll with %d events", ev_count);

		err = k_poll(events, ev_count, K_FOREVER);
		BT_ASSERT(err == 0);

		if (atomic_test_bit(bt_dev.flags, BT_DEV_TX_THREAD_EXIT)) {
			atomic_clear_bit(bt_dev.flags, BT_DEV_TX_THREAD_EXIT);
			return;
		}

		process_events(events, ev_count);

		/* Make sure we don't hog the CPU if there's all the time
		 * some ready events.
		 */
		k_yield();
	}
}


static void read_local_ver_complete(struct net_buf *buf)
{
	struct bt_hci_rp_read_local_version_info *rp = (void *)buf->data;

	BT_DBG("status 0x%02x", rp->status);

	bt_dev.hci_version = rp->hci_version;
	bt_dev.hci_revision = sys_le16_to_cpu(rp->hci_revision);
	bt_dev.lmp_version = rp->lmp_version;
	bt_dev.lmp_subversion = sys_le16_to_cpu(rp->lmp_subversion);
	bt_dev.manufacturer = sys_le16_to_cpu(rp->manufacturer);
}

static void read_le_features_complete(struct net_buf *buf)
{
	struct bt_hci_rp_le_read_local_features *rp = (void *)buf->data;

	BT_DBG("status 0x%02x", rp->status);

	memcpy(bt_dev.le.features, rp->features, sizeof(bt_dev.le.features));
}

#if defined(CONFIG_BT_BREDR)
static void read_buffer_size_complete(struct net_buf *buf)
{
	struct bt_hci_rp_read_buffer_size *rp = (void *)buf->data;
	uint16_t pkts;

	BT_DBG("status 0x%02x", rp->status);

	bt_dev.br.mtu = sys_le16_to_cpu(rp->acl_max_len);
	pkts = sys_le16_to_cpu(rp->acl_max_num);

	BT_DBG("ACL BR/EDR buffers: pkts %u mtu %u", pkts, bt_dev.br.mtu);

	bt_dev.br.pkts_num = pkts;
	k_sem_init(&bt_dev.br.pkts, pkts, pkts);
}
#elif defined(CONFIG_BT_CONN)
static void read_buffer_size_complete(struct net_buf *buf)
{
	struct bt_hci_rp_read_buffer_size *rp = (void *)buf->data;
	uint16_t pkts;

	BT_DBG("status 0x%02x", rp->status);

	/* If LE-side has buffers we can ignore the BR/EDR values */
	if (bt_dev.le.acl_mtu) {
		return;
	}

	bt_dev.le.acl_mtu = sys_le16_to_cpu(rp->acl_max_len);
	pkts = sys_le16_to_cpu(rp->acl_max_num);

	BT_DBG("ACL BR/EDR buffers: pkts %u mtu %u", pkts, bt_dev.le.acl_mtu);

	k_sem_init(&bt_dev.le.acl_pkts, pkts, pkts);
}
#endif

#if defined(CONFIG_BT_CONN)
static void le_read_buffer_size_complete(struct net_buf *buf)
{
	struct bt_hci_rp_le_read_buffer_size *rp = (void *)buf->data;

	BT_DBG("status 0x%02x", rp->status);

	bt_dev.le.acl_mtu = sys_le16_to_cpu(rp->le_max_len);
	if (!bt_dev.le.acl_mtu) {
		return;
	}

	BT_DBG("ACL LE buffers: pkts %u mtu %u", rp->le_max_num,
	       bt_dev.le.acl_mtu);

	k_sem_init(&bt_dev.le.acl_pkts, rp->le_max_num, rp->le_max_num);
}

static void read_buffer_size_v2_complete(struct net_buf *buf)
{
#if defined(CONFIG_BT_ISO)
	struct bt_hci_rp_le_read_buffer_size_v2 *rp = (void *)buf->data;
	uint8_t max_num;

	BT_DBG("status %u", rp->status);

	bt_dev.le.acl_mtu = sys_le16_to_cpu(rp->acl_mtu);
	if (!bt_dev.le.acl_mtu) {
		return;
	}

	BT_DBG("ACL LE buffers: pkts %u mtu %u", rp->acl_max_pkt,
		bt_dev.le.acl_mtu);

	max_num = MIN(rp->acl_max_pkt, CONFIG_BT_CONN_TX_MAX);
	k_sem_init(&bt_dev.le.acl_pkts, max_num, max_num);

	bt_dev.le.iso_mtu = sys_le16_to_cpu(rp->iso_mtu);
	if (!bt_dev.le.iso_mtu) {
		BT_ERR("ISO buffer size not set");
		return;
	}

	BT_DBG("ISO buffers: pkts %u mtu %u", rp->iso_max_pkt,
		bt_dev.le.iso_mtu);

	max_num = MIN(rp->iso_max_pkt, CONFIG_BT_ISO_TX_BUF_COUNT);
	k_sem_init(&bt_dev.le.iso_pkts, max_num, max_num);
#endif /* CONFIG_BT_ISO */
}

static int le_set_host_feature(uint8_t bit_number, uint8_t bit_value)
{
	struct bt_hci_cp_le_set_host_feature *cp;
	struct net_buf *buf;

	buf = bt_hci_cmd_create(BT_HCI_OP_LE_SET_HOST_FEATURE, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->bit_number = bit_number;
	cp->bit_value = bit_value;

	return bt_hci_cmd_send_sync(BT_HCI_OP_LE_SET_HOST_FEATURE, buf, NULL);
}

#endif /* CONFIG_BT_CONN */

static void read_supported_commands_complete(struct net_buf *buf)
{
	struct bt_hci_rp_read_supported_commands *rp = (void *)buf->data;

	BT_DBG("status 0x%02x", rp->status);

	memcpy(bt_dev.supported_commands, rp->commands,
	       sizeof(bt_dev.supported_commands));

	/*
	 * Report "LE Read Local P-256 Public Key" and "LE Generate DH Key" as
	 * supported if TinyCrypt ECC is used for emulation.
	 */
	if (IS_ENABLED(CONFIG_BT_TINYCRYPT_ECC)) {
		bt_dev.supported_commands[34] |= 0x02;
		bt_dev.supported_commands[34] |= 0x04;
	}
}

static void read_local_features_complete(struct net_buf *buf)
{
	struct bt_hci_rp_read_local_features *rp = (void *)buf->data;

	BT_DBG("status 0x%02x", rp->status);

	memcpy(bt_dev.features[0], rp->features, sizeof(bt_dev.features[0]));
}

static void le_read_supp_states_complete(struct net_buf *buf)
{
	struct bt_hci_rp_le_read_supp_states *rp = (void *)buf->data;

	BT_DBG("status 0x%02x", rp->status);

	bt_dev.le.states = sys_get_le64(rp->le_states);
}

#if defined(CONFIG_BT_SMP)
static void le_read_resolving_list_size_complete(struct net_buf *buf)
{
	struct bt_hci_rp_le_read_rl_size *rp = (void *)buf->data;

	BT_DBG("Resolving List size %u", rp->rl_size);

	bt_dev.le.rl_size = rp->rl_size;
}
#endif /* defined(CONFIG_BT_SMP) */

static int common_init(void)
{
	struct net_buf *rsp;
	int err;

	if (!(bt_dev.drv->quirks & BT_QUIRK_NO_RESET)) {
		/* Send HCI_RESET */
		err = bt_hci_cmd_send_sync(BT_HCI_OP_RESET, NULL, &rsp);
		if (err) {
			return err;
		}
		hci_reset_complete(rsp);
		net_buf_unref(rsp);
	}

	/* Read Local Supported Features */
	err = bt_hci_cmd_send_sync(BT_HCI_OP_READ_LOCAL_FEATURES, NULL, &rsp);
	if (err) {
		return err;
	}
	read_local_features_complete(rsp);
	net_buf_unref(rsp);

	/* Read Local Version Information */
	err = bt_hci_cmd_send_sync(BT_HCI_OP_READ_LOCAL_VERSION_INFO, NULL,
				   &rsp);
	if (err) {
		return err;
	}
	read_local_ver_complete(rsp);
	net_buf_unref(rsp);

	BT_INFO("bt version %d %d\n",bt_dev.hci_version,CONFIG_BT_VERSION);
	struct vs_manufacture_info param;
	param.bt_version = CONFIG_BT_VERSION;
	param.bt_subversion = 0;
	param.company_id = sys_cpu_to_le16(BT_CONTROLLER_ACTIONS_COMPANY_ID);
	bt_dev.hci_version = CONFIG_BT_VERSION; 
	bt_vs_write_manufacture_info(&param);

	/* Read Local Supported Commands */
	err = bt_hci_cmd_send_sync(BT_HCI_OP_READ_SUPPORTED_COMMANDS, NULL,
				   &rsp);
	if (err) {
		return err;
	}
	read_supported_commands_complete(rsp);
	net_buf_unref(rsp);

	if (IS_ENABLED(CONFIG_BT_HOST_CRYPTO)) {
		/* Initialize the PRNG so that it is safe to use it later
		 * on in the initialization process.
		 */
		err = prng_init();
		if (err) {
			return err;
		}
	}

#if defined(CONFIG_BT_HCI_ACL_FLOW_CONTROL)
	err = set_flow_control();
	if (err) {
		return err;
	}
#endif /* CONFIG_BT_HCI_ACL_FLOW_CONTROL */

	return 0;
}

static int le_set_event_mask(void)
{
	struct bt_hci_cp_le_set_event_mask *cp_mask;
	struct net_buf *buf;
	uint64_t mask = 0U;

	/* Set LE event mask */
	buf = bt_hci_cmd_create(BT_HCI_OP_LE_SET_EVENT_MASK, sizeof(*cp_mask));
	if (!buf) {
		return -ENOBUFS;
	}

	cp_mask = net_buf_add(buf, sizeof(*cp_mask));

	mask |= BT_EVT_MASK_LE_ADVERTISING_REPORT;

	if (IS_ENABLED(CONFIG_BT_EXT_ADV) &&
	    BT_DEV_FEAT_LE_EXT_ADV(bt_dev.le.features)) {
		mask |= BT_EVT_MASK_LE_ADV_SET_TERMINATED;
		mask |= BT_EVT_MASK_LE_SCAN_REQ_RECEIVED;
		mask |= BT_EVT_MASK_LE_EXT_ADVERTISING_REPORT;
		mask |= BT_EVT_MASK_LE_SCAN_TIMEOUT;
		if (IS_ENABLED(CONFIG_BT_PER_ADV_SYNC)) {
			mask |= BT_EVT_MASK_LE_PER_ADV_SYNC_ESTABLISHED;
			mask |= BT_EVT_MASK_LE_PER_ADVERTISING_REPORT;
			mask |= BT_EVT_MASK_LE_PER_ADV_SYNC_LOST;
			mask |= BT_EVT_MASK_LE_PAST_RECEIVED;
		}
	}

	if (IS_ENABLED(CONFIG_BT_CONN)) {
		if ((IS_ENABLED(CONFIG_BT_SMP) &&
		     BT_FEAT_LE_PRIVACY(bt_dev.le.features)) ||
		    (IS_ENABLED(CONFIG_BT_EXT_ADV) &&
		     BT_DEV_FEAT_LE_EXT_ADV(bt_dev.le.features))) {
			/* C24:
			 * Mandatory if the LE Controller supports Connection
			 * State and either LE Feature (LL Privacy) or
			 * LE Feature (Extended Advertising) is supported, ...
			 */
			mask |= BT_EVT_MASK_LE_ENH_CONN_COMPLETE;
		} else {
			mask |= BT_EVT_MASK_LE_CONN_COMPLETE;
		}

		mask |= BT_EVT_MASK_LE_CONN_UPDATE_COMPLETE;
		mask |= BT_EVT_MASK_LE_REMOTE_FEAT_COMPLETE;

		if (BT_FEAT_LE_CONN_PARAM_REQ_PROC(bt_dev.le.features)) {
			mask |= BT_EVT_MASK_LE_CONN_PARAM_REQ;
		}

		if (IS_ENABLED(CONFIG_BT_DATA_LEN_UPDATE) &&
		    BT_FEAT_LE_DLE(bt_dev.le.features)) {
			mask |= BT_EVT_MASK_LE_DATA_LEN_CHANGE;
		}

		if (IS_ENABLED(CONFIG_BT_PHY_UPDATE) &&
		    (BT_FEAT_LE_PHY_2M(bt_dev.le.features) ||
		     BT_FEAT_LE_PHY_CODED(bt_dev.le.features))) {
			mask |= BT_EVT_MASK_LE_PHY_UPDATE_COMPLETE;
		}
	}

	if (IS_ENABLED(CONFIG_BT_SMP) &&
	    BT_FEAT_LE_ENCR(bt_dev.le.features)) {
		mask |= BT_EVT_MASK_LE_LTK_REQUEST;
	}

	/*
	 * If "LE Read Local P-256 Public Key" and "LE Generate DH Key" are
	 * supported we need to enable events generated by those commands.
	 */
	if (IS_ENABLED(CONFIG_BT_ECC) &&
	    (BT_CMD_TEST(bt_dev.supported_commands, 34, 1)) &&
	    (BT_CMD_TEST(bt_dev.supported_commands, 34, 2))) {
		mask |= BT_EVT_MASK_LE_P256_PUBLIC_KEY_COMPLETE;
		mask |= BT_EVT_MASK_LE_GENERATE_DHKEY_COMPLETE;
	}

	/*
	 * Enable CIS events only if ISO connections are enabled and controller
	 * support them.
	 */
	if (IS_ENABLED(CONFIG_BT_ISO) &&
	    BT_FEAT_LE_CIS(bt_dev.le.features)) {
		mask |= BT_EVT_MASK_LE_CIS_ESTABLISHED;
		if (BT_FEAT_LE_CIS_SLAVE(bt_dev.le.features)) {
			mask |= BT_EVT_MASK_LE_CIS_REQ;
		}
	}

	/* Enable BIS events for broadcaster and/or receiver */
	if (IS_ENABLED(CONFIG_BT_ISO) && BT_FEAT_LE_BIS(bt_dev.le.features)) {
		if (IS_ENABLED(CONFIG_BT_ISO_BROADCASTER) &&
		    BT_FEAT_LE_ISO_BROADCASTER(bt_dev.le.features)) {
			mask |= BT_EVT_MASK_LE_BIG_COMPLETE;
			mask |= BT_EVT_MASK_LE_BIG_TERMINATED;
		}
		if (IS_ENABLED(CONFIG_BT_ISO_SYNC_RECEIVER) &&
		    BT_FEAT_LE_SYNC_RECEIVER(bt_dev.le.features)) {
			mask |= BT_EVT_MASK_LE_BIG_SYNC_ESTABLISHED;
			mask |= BT_EVT_MASK_LE_BIG_SYNC_LOST;
			mask |= BT_EVT_MASK_LE_BIGINFO_ADV_REPORT;
		}
	}

	/* Enable IQ samples report events receiver */
	if (IS_ENABLED(CONFIG_BT_DF_CONNECTIONLESS_CTE_RX)) {
		mask |= BT_EVT_MASK_LE_CONNECTIONLESS_IQ_REPORT;
	}

	if (IS_ENABLED(CONFIG_BT_DF_CONNECTION_CTE_RX)) {
		mask |= BT_EVT_MASK_LE_CONNECTION_IQ_REPORT;
		mask |= BT_EVT_MASK_LE_CTE_REQUEST_FAILED;
	}

	if (IS_ENABLED(CONFIG_BT_PER_ADV_RSP)) {
		mask |= BT_EVT_MASK_LE_PER_ADV_SUBEVENT_DATA_REQ;
		mask |= BT_EVT_MASK_LE_PER_ADV_RESPONSE_REPORT;
	}

	if (IS_ENABLED(CONFIG_BT_PER_ADV_SYNC_RSP)) {
		mask |= BT_EVT_MASK_LE_PER_ADVERTISING_REPORT_V2;
		mask |= BT_EVT_MASK_LE_PER_ADV_SYNC_ESTABLISHED_V2;
		mask |= BT_EVT_MASK_LE_PAST_RECEIVED_V2;
	}

	if (IS_ENABLED(CONFIG_BT_CONN) &&
	    (IS_ENABLED(CONFIG_BT_PER_ADV_RSP) || IS_ENABLED(CONFIG_BT_PER_ADV_SYNC_RSP))) {
		mask |= BT_EVT_MASK_LE_ENH_CONN_COMPLETE_V2;
	}

	sys_put_le64(mask, cp_mask->events);
	return bt_hci_cmd_send_sync(BT_HCI_OP_LE_SET_EVENT_MASK, buf, NULL);
}

static int le_init_iso(void)
{
	int err;
	struct net_buf *rsp;

	/* Set Isochronous Channels - Host support */
	err = le_set_host_feature(BT_LE_FEAT_BIT_ISO_CHANNELS, 1);
	if (err) {
		return err;
	}

	/* Octet 41, bit 5 is read buffer size V2 */
	if (BT_CMD_TEST(bt_dev.supported_commands, 41, 5)) {
		/* Read ISO Buffer Size V2 */
		err = bt_hci_cmd_send_sync(BT_HCI_OP_LE_READ_BUFFER_SIZE_V2,
					   NULL, &rsp);
		if (err) {
			return err;
		}
		read_buffer_size_v2_complete(rsp);

		net_buf_unref(rsp);
	} else if (IS_ENABLED(CONFIG_BT_CONN)) {
		BT_WARN("Read Buffer Size V2 command is not supported."
			"No ISO buffers will be available");

		/* Read LE Buffer Size */
		err = bt_hci_cmd_send_sync(BT_HCI_OP_LE_READ_BUFFER_SIZE,
					   NULL, &rsp);
		if (err) {
			return err;
		}
		le_read_buffer_size_complete(rsp);

		net_buf_unref(rsp);
	}

	return 0;
}

static int le_init(void)
{
	struct bt_hci_cp_write_le_host_supp *cp_le;
	struct net_buf *buf, *rsp;
	int err;

	/* For now we only support LE capable controllers */
	if (!BT_FEAT_LE(bt_dev.features)) {
		BT_ERR("Non-LE capable controller detected!");
		return -ENODEV;
	}

	/* Read Low Energy Supported Features */
	err = bt_hci_cmd_send_sync(BT_HCI_OP_LE_READ_LOCAL_FEATURES, NULL,
				   &rsp);
	if (err) {
		return err;
	}

	read_le_features_complete(rsp);
	net_buf_unref(rsp);

	if (IS_ENABLED(CONFIG_BT_ISO) &&
	    BT_FEAT_LE_ISO(bt_dev.le.features)) {
		err = le_init_iso();
		if (err) {
			return err;
		}
	} else if (IS_ENABLED(CONFIG_BT_CONN)) {
		/* Read LE Buffer Size */
		err = bt_hci_cmd_send_sync(BT_HCI_OP_LE_READ_BUFFER_SIZE,
					   NULL, &rsp);
		if (err) {
			return err;
		}
		le_read_buffer_size_complete(rsp);
		net_buf_unref(rsp);
	}

	if (BT_FEAT_BREDR(bt_dev.features)) {
		buf = bt_hci_cmd_create(BT_HCI_OP_LE_WRITE_LE_HOST_SUPP,
					sizeof(*cp_le));
		if (!buf) {
			return -ENOBUFS;
		}

		cp_le = net_buf_add(buf, sizeof(*cp_le));

		/* Explicitly enable LE for dual-mode controllers */
		cp_le->le = 0x01;
		cp_le->simul = 0x00;
		err = bt_hci_cmd_send_sync(BT_HCI_OP_LE_WRITE_LE_HOST_SUPP, buf,
					   NULL);
		if (err) {
			return err;
		}
	}

	/* Read LE Supported States */
	if (BT_CMD_LE_STATES(bt_dev.supported_commands)) {
		err = bt_hci_cmd_send_sync(BT_HCI_OP_LE_READ_SUPP_STATES, NULL,
					   &rsp);
		if (err) {
			return err;
		}

		le_read_supp_states_complete(rsp);
		net_buf_unref(rsp);
	}

	if (IS_ENABLED(CONFIG_BT_CONN) &&
	    IS_ENABLED(CONFIG_BT_DATA_LEN_UPDATE) &&
	    IS_ENABLED(CONFIG_BT_AUTO_DATA_LEN_UPDATE) &&
	    BT_FEAT_LE_DLE(bt_dev.le.features)) {
		struct bt_hci_cp_le_write_default_data_len *cp;
		uint16_t tx_octets, tx_time;

		err = hci_le_read_max_data_len(&tx_octets, &tx_time);
		if (err) {
			return err;
		}

		buf = bt_hci_cmd_create(BT_HCI_OP_LE_WRITE_DEFAULT_DATA_LEN,
					sizeof(*cp));
		if (!buf) {
			return -ENOBUFS;
		}

		cp = net_buf_add(buf, sizeof(*cp));
		cp->max_tx_octets = sys_cpu_to_le16(tx_octets);
		cp->max_tx_time = sys_cpu_to_le16(tx_time);

		err = bt_hci_cmd_send_sync(BT_HCI_OP_LE_WRITE_DEFAULT_DATA_LEN,
					   buf, NULL);
		if (err) {
			return err;
		}
	}

#if defined(CONFIG_BT_SMP)
	if (BT_FEAT_LE_PRIVACY(bt_dev.le.features)) {
#if defined(CONFIG_BT_PRIVACY)
		struct bt_hci_cp_le_set_rpa_timeout *cp;

		buf = bt_hci_cmd_create(BT_HCI_OP_LE_SET_RPA_TIMEOUT,
					sizeof(*cp));
		if (!buf) {
			return -ENOBUFS;
		}

		cp = net_buf_add(buf, sizeof(*cp));
		cp->rpa_timeout = sys_cpu_to_le16(CONFIG_BT_RPA_TIMEOUT);
		err = bt_hci_cmd_send_sync(BT_HCI_OP_LE_SET_RPA_TIMEOUT, buf,
					   NULL);
		if (err) {
			return err;
		}
#endif /* defined(CONFIG_BT_PRIVACY) */

		err = bt_hci_cmd_send_sync(BT_HCI_OP_LE_READ_RL_SIZE, NULL,
					   &rsp);
		if (err) {
			return err;
		}
		le_read_resolving_list_size_complete(rsp);
		net_buf_unref(rsp);
	}
#endif

#if defined(CONFIG_BT_PROPERTY)
	/* load keys after read resolving list size and then we can add
	   device to resolving list */
	bt_keys_load();
#endif

#if IS_ENABLED(CONFIG_BT_DF)
	if (BT_FEAT_LE_CONNECTIONLESS_CTE_TX(bt_dev.le.features) ||
	    BT_FEAT_LE_CONNECTIONLESS_CTE_RX(bt_dev.le.features) ||
	    BT_FEAT_LE_RX_CTE(bt_dev.le.features)) {
		err = le_df_init();
		if (err) {
			return err;
		}
	}
#endif /* CONFIG_BT_DF */

	return  le_set_event_mask();
}

#if defined(CONFIG_BT_BREDR)
static int read_ext_features(void)
{
	int i;

	/* Read Local Supported Extended Features */
	for (i = 1; i < LMP_FEAT_PAGES_COUNT; i++) {
		struct bt_hci_cp_read_local_ext_features *cp;
		struct bt_hci_rp_read_local_ext_features *rp;
		struct net_buf *buf, *rsp;
		int err;

		buf = bt_hci_cmd_create(BT_HCI_OP_READ_LOCAL_EXT_FEATURES,
					sizeof(*cp));
		if (!buf) {
			return -ENOBUFS;
		}

		cp = net_buf_add(buf, sizeof(*cp));
		cp->page = i;

		err = bt_hci_cmd_send_sync(BT_HCI_OP_READ_LOCAL_EXT_FEATURES,
					   buf, &rsp);
		if (err) {
			return err;
		}

		rp = (void *)rsp->data;

		memcpy(&bt_dev.features[i], rp->ext_features,
		       sizeof(bt_dev.features[i]));

		if (rp->max_page <= i) {
			net_buf_unref(rsp);
			break;
		}

		net_buf_unref(rsp);
	}

	return 0;
}

static uint16_t device_supported_pkt_type(uint8_t features[][8])
{
	uint16_t esco_pkt_type = 0;

	/* Device supported features and sco packet types */
	if (BT_FEAT_LMP_SCO_CAPABLE(features)) {
		esco_pkt_type |= (HCI_PKT_TYPE_ESCO_HV1);
	}

	if (BT_FEAT_HV2_PKT(features)) {
		esco_pkt_type |= (HCI_PKT_TYPE_ESCO_HV2);
	}

	if (BT_FEAT_HV3_PKT(features)) {
		esco_pkt_type |= (HCI_PKT_TYPE_ESCO_HV3);
	}

	if (BT_FEAT_LMP_ESCO_CAPABLE(features)) {
		esco_pkt_type |= (HCI_PKT_TYPE_ESCO_EV3);
	}

	if (BT_FEAT_EV4_PKT(features)) {
		esco_pkt_type |= (HCI_PKT_TYPE_ESCO_EV4);
	}

	if (BT_FEAT_EV5_PKT(features)) {
		esco_pkt_type |= (HCI_PKT_TYPE_ESCO_EV5);
	}

	if (BT_FEAT_2EV3_PKT(features)) {
		esco_pkt_type |= (HCI_PKT_TYPE_ESCO_2EV3);
	}

	if (BT_FEAT_3EV3_PKT(features)) {
		esco_pkt_type |= (HCI_PKT_TYPE_ESCO_3EV3);
	}

	if (BT_FEAT_2EV3_PKT(features) && BT_FEAT_3SLOT_PKT(features)) {
		esco_pkt_type |= (HCI_PKT_TYPE_ESCO_2EV5);
	}

	if (BT_FEAT_3EV3_PKT(features) && BT_FEAT_3SLOT_PKT(features)) {
		esco_pkt_type |= (HCI_PKT_TYPE_ESCO_3EV5);
	}

	return esco_pkt_type;
}

//0:disable, 1:enable, other:unknown
static int8_t fixed_chan_default_status = -1;
int8_t bt_get_fixed_chan_default_status(void)
{
    return fixed_chan_default_status;
}

static int br_init(void)
{
	struct net_buf *buf;
#if 0	/* Move to br_ext_cmd_init */
	struct bt_hci_cp_write_ssp_mode *ssp_cp;
	struct bt_hci_cp_write_inquiry_mode *inq_cp;
	struct bt_hci_write_local_name *name_cp;
#endif
	int err;

	/* Actions add start */
	/* Set SSP mode, must set SSP mode before read external features */
	err = br_write_mode_type(BT_HCI_OP_WRITE_SSP_MODE, 0x01, true);
	if (err) {
		return err;
	}
	/* Actions add end */

	/* Read extended local features */
	if (BT_FEAT_EXT_FEATURES(bt_dev.features)) {
		err = read_ext_features();
		if (err) {
			return err;
		}
	}

	/* Add local supported packet types to bt_dev */
	bt_dev.br.esco_pkt_type = device_supported_pkt_type(bt_dev.features);

	/* Get BR/EDR buffer size */
	err = bt_hci_cmd_send_sync(BT_HCI_OP_READ_BUFFER_SIZE, NULL, &buf);
	if (err) {
		return err;
	}

	read_buffer_size_complete(buf);
	net_buf_unref(buf);

#if 0	/* Move to br_ext_cmd_init */
	/* Set SSP mode */
	buf = bt_hci_cmd_create(BT_HCI_OP_WRITE_SSP_MODE, sizeof(*ssp_cp));
	if (!buf) {
		return -ENOBUFS;
	}

	ssp_cp = net_buf_add(buf, sizeof(*ssp_cp));
	ssp_cp->mode = 0x01;
	err = bt_hci_cmd_send_sync(BT_HCI_OP_WRITE_SSP_MODE, buf, NULL);
	if (err) {
		return err;
	}

	/* Enable Inquiry results with RSSI or extended Inquiry */
	buf = bt_hci_cmd_create(BT_HCI_OP_WRITE_INQUIRY_MODE, sizeof(*inq_cp));
	if (!buf) {
		return -ENOBUFS;
	}

	inq_cp = net_buf_add(buf, sizeof(*inq_cp));
	inq_cp->mode = 0x02;
	err = bt_hci_cmd_send_sync(BT_HCI_OP_WRITE_INQUIRY_MODE, buf, NULL);
	if (err) {
		return err;
	}

	/* Set local name */
	buf = bt_hci_cmd_create(BT_HCI_OP_WRITE_LOCAL_NAME, sizeof(*name_cp));
	if (!buf) {
		return -ENOBUFS;
	}

	name_cp = net_buf_add(buf, sizeof(*name_cp));
	strncpy((char *)name_cp->local_name, CONFIG_BT_DEVICE_NAME,
		sizeof(name_cp->local_name));

	err = bt_hci_cmd_send_sync(BT_HCI_OP_WRITE_LOCAL_NAME, buf, NULL);
	if (err) {
		return err;
	}
#else
	err = br_ext_cmd_init();
	if (err) {
		return err;
	}
#endif

	/* Set page timeout*/
	buf = bt_hci_cmd_create(BT_HCI_OP_WRITE_PAGE_TIMEOUT, sizeof(uint16_t));
	if (!buf) {
		return -ENOBUFS;
	}

	net_buf_add_le16(buf, CONFIG_BT_PAGE_TIMEOUT);

	err = bt_hci_cmd_send_sync(BT_HCI_OP_WRITE_PAGE_TIMEOUT, buf, NULL);
	if (err) {
		return err;
	}

	/* Enable BR/EDR SC if supported */
	if (BT_FEAT_SC(bt_dev.features) && IS_ENABLED(CONFIG_BT_BR_SC_HOST_SUPP)) {
		struct bt_hci_cp_write_sc_host_supp *sc_cp;

		buf = bt_hci_cmd_create(BT_HCI_OP_WRITE_SC_HOST_SUPP,
					sizeof(*sc_cp));
		if (!buf) {
			return -ENOBUFS;
		}

		sc_cp = net_buf_add(buf, sizeof(*sc_cp));
		sc_cp->sc_support = 0x01;

		err = bt_hci_cmd_send_sync(BT_HCI_OP_WRITE_SC_HOST_SUPP, buf,
					   NULL);
		if (err) {
			return err;
		}else {
			//enable fixed channel after enable sc
			bt_l2cap_br_fixed_chan_enable(1);
            fixed_chan_default_status = 1;
		}
	}

    if (fixed_chan_default_status < 0) {
        fixed_chan_default_status = 0;
    }

	return 0;
}
#else
static int br_init(void)
{
#if defined(CONFIG_BT_CONN)
	struct net_buf *rsp;
	int err;

	if (bt_dev.le.acl_mtu) {
		return 0;
	}

	/* Use BR/EDR buffer size if LE reports zero buffers */
	err = bt_hci_cmd_send_sync(BT_HCI_OP_READ_BUFFER_SIZE, NULL, &rsp);
	if (err) {
		return err;
	}

	read_buffer_size_complete(rsp);
	net_buf_unref(rsp);
#endif /* CONFIG_BT_CONN */

	return 0;
}
#endif

static int set_event_mask(void)
{
	struct bt_hci_cp_set_event_mask *ev;
	struct net_buf *buf;
	uint64_t mask = 0U;

	buf = bt_hci_cmd_create(BT_HCI_OP_SET_EVENT_MASK, sizeof(*ev));
	if (!buf) {
		return -ENOBUFS;
	}

	ev = net_buf_add(buf, sizeof(*ev));

	if (IS_ENABLED(CONFIG_BT_BREDR)) {
		/* Since we require LE support, we can count on a
		 * Bluetooth 4.0 feature set
		 */
		mask |= BT_EVT_MASK_INQUIRY_COMPLETE;
		mask |= BT_EVT_MASK_CONN_COMPLETE;
		mask |= BT_EVT_MASK_CONN_REQUEST;
		mask |= BT_EVT_MASK_AUTH_COMPLETE;
		mask |= BT_EVT_MASK_REMOTE_NAME_REQ_COMPLETE;
		mask |= BT_EVT_MASK_REMOTE_FEATURES;
		mask |= BT_EVT_MASK_ROLE_CHANGE;
		mask |= BT_EVT_MASK_MODE_CHANGE;
		mask |= BT_EVT_MASK_PIN_CODE_REQ;
		mask |= BT_EVT_MASK_LINK_KEY_REQ;
		mask |= BT_EVT_MASK_LINK_KEY_NOTIFY;
		mask |= BT_EVT_MASK_INQUIRY_RESULT_WITH_RSSI;
		mask |= BT_EVT_MASK_REMOTE_EXT_FEATURES;
		mask |= BT_EVT_MASK_SYNC_CONN_COMPLETE;
		mask |= BT_EVT_MASK_EXTENDED_INQUIRY_RESULT;
		mask |= BT_EVT_MASK_IO_CAPA_REQ;
		mask |= BT_EVT_MASK_IO_CAPA_RESP;
		mask |= BT_EVT_MASK_USER_CONFIRM_REQ;
		mask |= BT_EVT_MASK_USER_PASSKEY_REQ;
		mask |= BT_EVT_MASK_SSP_COMPLETE;
		mask |= BT_EVT_MASK_USER_PASSKEY_NOTIFY;
	}

	mask |= BT_EVT_MASK_HARDWARE_ERROR;
	mask |= BT_EVT_MASK_DATA_BUFFER_OVERFLOW;
	mask |= BT_EVT_MASK_LE_META_EVENT;

	if (IS_ENABLED(CONFIG_BT_CONN)) {
		mask |= BT_EVT_MASK_DISCONN_COMPLETE;
		mask |= BT_EVT_MASK_REMOTE_VERSION_INFO;
	}

	if ((IS_ENABLED(CONFIG_BT_SMP) && BT_FEAT_LE_ENCR(bt_dev.le.features)) ||
		IS_ENABLED(CONFIG_BT_BREDR)) {
		mask |= BT_EVT_MASK_ENCRYPT_CHANGE;
		mask |= BT_EVT_MASK_ENCRYPT_KEY_REFRESH_COMPLETE;
	}

	sys_put_le64(mask, ev->events);
	return bt_hci_cmd_send_sync(BT_HCI_OP_SET_EVENT_MASK, buf, NULL);
}

#if defined(CONFIG_BT_DEBUG)
static const char *ver_str(uint8_t ver)
{
	const char * const str[] = {
		"1.0b", "1.1", "1.2", "2.0", "2.1", "3.0", "4.0", "4.1", "4.2",
		"5.0", "5.1", "5.2", "5.3"
	};

	if (ver < ARRAY_SIZE(str)) {
		return str[ver];
	}

	return "unknown";
}

static void bt_dev_show_info(void)
{
	int i;

	BT_INFO("Identity%s: %s", bt_dev.id_count > 1 ? "[0]" : "",
		bt_addr_le_str(&bt_dev.id_addr[0]));

	for (i = 1; i < bt_dev.id_count; i++) {
		BT_INFO("Identity[%d]: %s",
			i, bt_addr_le_str(&bt_dev.id_addr[i]));
	}

	BT_INFO("HCI: version %s (0x%02x) revision 0x%04x, manufacturer 0x%04x",
		ver_str(bt_dev.hci_version), bt_dev.hci_version,
		bt_dev.hci_revision, bt_dev.manufacturer);
	BT_INFO("LMP: version %s (0x%02x) subver 0x%04x",
		ver_str(bt_dev.lmp_version), bt_dev.lmp_version,
		bt_dev.lmp_subversion);
}
#else
static inline void bt_dev_show_info(void)
{
}
#endif /* CONFIG_BT_DEBUG */

#if defined(CONFIG_BT_HCI_VS_EXT)
#if defined(CONFIG_BT_DEBUG)
static const char *vs_hw_platform(uint16_t platform)
{
	static const char * const plat_str[] = {
		"reserved", "Intel Corporation", "Nordic Semiconductor",
		"NXP Semiconductors" };

	if (platform < ARRAY_SIZE(plat_str)) {
		return plat_str[platform];
	}

	return "unknown";
}

static const char *vs_hw_variant(uint16_t platform, uint16_t variant)
{
	static const char * const nordic_str[] = {
		"reserved", "nRF51x", "nRF52x", "nRF53x"
	};

	if (platform != BT_HCI_VS_HW_PLAT_NORDIC) {
		return "unknown";
	}

	if (variant < ARRAY_SIZE(nordic_str)) {
		return nordic_str[variant];
	}

	return "unknown";
}

static const char *vs_fw_variant(uint8_t variant)
{
	static const char * const var_str[] = {
		"Standard Bluetooth controller",
		"Vendor specific controller",
		"Firmware loader",
		"Rescue image",
	};

	if (variant < ARRAY_SIZE(var_str)) {
		return var_str[variant];
	}

	return "unknown";
}
#endif /* CONFIG_BT_DEBUG */

static void hci_vs_init(void)
{
	union {
		struct bt_hci_rp_vs_read_version_info *info;
		struct bt_hci_rp_vs_read_supported_commands *cmds;
		struct bt_hci_rp_vs_read_supported_features *feat;
	} rp;
	struct net_buf *rsp;
	int err;

	/* If heuristics is enabled, try to guess HCI VS support by looking
	 * at the HCI version and identity address. We haven't set any addresses
	 * at this point. So we need to read the public address.
	 */
	if (IS_ENABLED(CONFIG_BT_HCI_VS_EXT_DETECT)) {
		bt_addr_le_t addr;

		if ((bt_dev.hci_version < BT_HCI_VERSION_5_0) ||
		    bt_id_read_public_addr(&addr)) {
			BT_WARN("Controller doesn't seem to support "
				"Zephyr vendor HCI");
			return;
		}
	}

	err = bt_hci_cmd_send_sync(BT_HCI_OP_VS_READ_VERSION_INFO, NULL, &rsp);
	if (err) {
		BT_WARN("Vendor HCI extensions not available");
		return;
	}

	if (IS_ENABLED(CONFIG_BT_HCI_VS_EXT_DETECT) &&
	    rsp->len != sizeof(struct bt_hci_rp_vs_read_version_info)) {
		BT_WARN("Invalid Vendor HCI extensions");
		net_buf_unref(rsp);
		return;
	}

#if defined(CONFIG_BT_DEBUG)
	rp.info = (void *)rsp->data;
	BT_INFO("HW Platform: %s (0x%04x)",
		vs_hw_platform(sys_le16_to_cpu(rp.info->hw_platform)),
		sys_le16_to_cpu(rp.info->hw_platform));
	BT_INFO("HW Variant: %s (0x%04x)",
		vs_hw_variant(sys_le16_to_cpu(rp.info->hw_platform),
			      sys_le16_to_cpu(rp.info->hw_variant)),
		sys_le16_to_cpu(rp.info->hw_variant));
	BT_INFO("Firmware: %s (0x%02x) Version %u.%u Build %u",
		vs_fw_variant(rp.info->fw_variant), rp.info->fw_variant,
		rp.info->fw_version, sys_le16_to_cpu(rp.info->fw_revision),
		sys_le32_to_cpu(rp.info->fw_build));
#endif /* CONFIG_BT_DEBUG */

	net_buf_unref(rsp);

	err = bt_hci_cmd_send_sync(BT_HCI_OP_VS_READ_SUPPORTED_COMMANDS,
				   NULL, &rsp);
	if (err) {
		BT_WARN("Failed to read supported vendor commands");
		return;
	}

	if (IS_ENABLED(CONFIG_BT_HCI_VS_EXT_DETECT) &&
	    rsp->len != sizeof(struct bt_hci_rp_vs_read_supported_commands)) {
		BT_WARN("Invalid Vendor HCI extensions");
		net_buf_unref(rsp);
		return;
	}

	rp.cmds = (void *)rsp->data;
	memcpy(bt_dev.vs_commands, rp.cmds->commands, BT_DEV_VS_CMDS_MAX);
	net_buf_unref(rsp);

	if (BT_VS_CMD_SUP_FEAT(bt_dev.vs_commands)) {
		err = bt_hci_cmd_send_sync(BT_HCI_OP_VS_READ_SUPPORTED_FEATURES,
					   NULL, &rsp);
		if (err) {
			BT_WARN("Failed to read supported vendor features");
			return;
		}

		if (IS_ENABLED(CONFIG_BT_HCI_VS_EXT_DETECT) &&
		    rsp->len !=
		    sizeof(struct bt_hci_rp_vs_read_supported_features)) {
			BT_WARN("Invalid Vendor HCI extensions");
			net_buf_unref(rsp);
			return;
		}

		rp.feat = (void *)rsp->data;
		memcpy(bt_dev.vs_features, rp.feat->features,
		       BT_DEV_VS_FEAT_MAX);
		net_buf_unref(rsp);
	}
}
#endif /* CONFIG_BT_HCI_VS_EXT */

static int hci_init(void)
{
	int err;

	err = common_init();
	if (err) {
		return err;
	}

	err = le_init();
	if (err) {
		return err;
	}

	if (BT_FEAT_BREDR(bt_dev.features)) {
		err = br_init();
		if (err) {
			return err;
		}
	} else if (IS_ENABLED(CONFIG_BT_BREDR)) {
		BT_ERR("Non-BR/EDR controller detected");
		return -EIO;
	}

	err = set_event_mask();
	if (err) {
		return err;
	}

#if defined(CONFIG_BT_HCI_VS_EXT)
	hci_vs_init();
#endif
	err = bt_id_init();
	if (err) {
		return err;
	}

	return 0;
}

int bt_send(struct net_buf *buf)
{
	BT_DBG("buf %p len %u type %u", buf, buf->len, bt_buf_get_type(buf));

	bt_monitor_send(bt_monitor_opcode(buf), buf->data, buf->len);

	if (IS_ENABLED(CONFIG_BT_TINYCRYPT_ECC)) {
		return bt_hci_ecc_send(buf);
	}

	hci_data_log_debug(true, buf);
	return bt_dev.drv->send(buf);
}

static const struct event_handler prio_events[] = {
	EVENT_HANDLER(BT_HCI_EVT_CMD_COMPLETE, hci_cmd_complete,
		      sizeof(struct bt_hci_evt_cmd_complete)),
	EVENT_HANDLER(BT_HCI_EVT_CMD_STATUS, hci_cmd_status,
		      sizeof(struct bt_hci_evt_cmd_status)),
#if defined(CONFIG_BT_CONN)
	EVENT_HANDLER(BT_HCI_EVT_DATA_BUF_OVERFLOW,
		      hci_data_buf_overflow,
		      sizeof(struct bt_hci_evt_data_buf_overflow)),
	EVENT_HANDLER(BT_HCI_EVT_NUM_COMPLETED_PACKETS,
		      hci_num_completed_packets,
		      sizeof(struct bt_hci_evt_num_completed_packets)),
	EVENT_HANDLER(BT_HCI_EVT_DISCONN_COMPLETE, hci_disconn_complete_prio,
		      sizeof(struct bt_hci_evt_disconn_complete)),

#endif /* CONFIG_BT_CONN */
};

void hci_event_prio(struct net_buf *buf)
{
	struct net_buf_simple_state state;
	struct bt_hci_evt_hdr *hdr;
	uint8_t evt_flags;

	net_buf_simple_save(&buf->b, &state);

	BT_ASSERT(buf->len >= sizeof(*hdr));

	hdr = net_buf_pull_mem(buf, sizeof(*hdr));
	evt_flags = bt_hci_evt_get_flags(hdr->evt);
	BT_ASSERT(evt_flags & BT_HCI_EVT_FLAG_RECV_PRIO);

	handle_event(hdr->evt, buf, prio_events, ARRAY_SIZE(prio_events));

	if (evt_flags & BT_HCI_EVT_FLAG_RECV) {
		net_buf_simple_restore(&buf->b, &state);
	} else {
		net_buf_unref(buf);
	}
}

int bt_recv(struct net_buf *buf)
{
	bt_monitor_send(bt_monitor_opcode(buf), buf->data, buf->len);
	hci_data_log_debug(false, buf);

	BT_DBG("buf %p len %u", buf, buf->len);

	switch (bt_buf_get_type(buf)) {
#if defined(CONFIG_BT_CONN)
	case BT_BUF_ACL_IN:
#if defined(CONFIG_BT_RECV_IS_RX_THREAD)
		hci_acl(buf);
#else
		net_buf_put(&bt_dev.rx_queue, buf);
#endif
		return 0;
#endif /* BT_CONN */
	case BT_BUF_EVT:
	{
#if defined(CONFIG_BT_RECV_IS_RX_THREAD)
		hci_event(buf);
#else
		struct bt_hci_evt_hdr *hdr = (void *)buf->data;
		uint8_t evt_flags = bt_hci_evt_get_flags(hdr->evt);

		if (evt_flags & BT_HCI_EVT_FLAG_RECV_PRIO) {
			hci_event_prio(buf);
		}

		if (evt_flags & BT_HCI_EVT_FLAG_RECV) {
			net_buf_put(&bt_dev.rx_queue, buf);
		}
#endif
		return 0;
	}

	case BT_BUF_SCO_IN:
#if defined(CONFIG_BT_RECV_IS_RX_THREAD)
		hci_sco(buf);
#else
		net_buf_put(&bt_dev.rx_queue, buf);
#endif
		return 0;

#if defined(CONFIG_BT_ISO)
	case BT_BUF_ISO_IN:
#if 0
#if defined(CONFIG_BT_RECV_IS_RX_THREAD)
		hci_iso(buf);
#else
		net_buf_put(&bt_dev.rx_queue, buf);
#endif
#else
		/* receive iso packets faster */
		hci_iso(buf);
#endif
		return 0;
#endif /* CONFIG_BT_ISO */
	default:
		BT_ERR("Invalid buf type %u", bt_buf_get_type(buf));
		net_buf_unref(buf);
		return -EINVAL;
	}
}

//#if defined(CONFIG_BT_RECV_IS_RX_THREAD)
int bt_recv_prio(struct net_buf *buf)
{
	bt_monitor_send(bt_monitor_opcode(buf), buf->data, buf->len);
	hci_data_log_debug(false, buf);

	BT_ASSERT(bt_buf_get_type(buf) == BT_BUF_EVT);

	hci_event_prio(buf);

	return 0;
}
//#endif /* defined(CONFIG_BT_RECV_IS_RX_THREAD) */

int bt_hci_driver_register(const struct bt_hci_driver *drv)
{
	if (bt_dev.drv) {
		return -EALREADY;
	}

	if (!drv->open || !drv->send) {
		return -EINVAL;
	}

	bt_dev.drv = drv;

	BT_DBG("Registered %s", drv->name ? drv->name : "");

	bt_monitor_new_index(BT_MONITOR_TYPE_PRIMARY, drv->bus,
			     BT_ADDR_ANY, drv->name ? drv->name : "bt0");

	return 0;
}

void bt_finalize_init(void)
{
	atomic_set_bit(bt_dev.flags, BT_DEV_READY);

	if (IS_ENABLED(CONFIG_BT_OBSERVER)) {
		bt_le_scan_update(false);
	}

	bt_dev_show_info();
}

static int bt_init(void)
{
	int err;

	if (IS_ENABLED(CONFIG_BT_PROPERTY)) {
		if (bt_dev.id_count) {
			atomic_set_bit(bt_dev.flags, BT_DEV_PRESET_ID);
		}

		BT_DBG("No ID address. should load property");
		/*need to load IRK before adding resolving list if keys exist*/
		bt_property_load();
	}

	err = hci_init();
	if (err) {
		return err;
	}

	if (IS_ENABLED(CONFIG_BT_CONN)) {
		err = bt_conn_init();
		if (err) {
			return err;
		}
	}

	if (IS_ENABLED(CONFIG_BT_ISO)) {
		err = bt_conn_iso_init();
		if (err) {
			return err;
		}
	}

	if (!atomic_test_bit(bt_dev.flags, BT_DEV_READY)) {
		bt_finalize_init();
	}
	return 0;
}

static void init_work(struct k_work *work)
{
	int err;

	err = bt_init();
	if (ready_cb) {
		ready_cb(err);
	}
}

#if !defined(CONFIG_BT_RECV_IS_RX_THREAD)
static void hci_rx_thread(void)
{
	struct net_buf *buf;
#if defined(CONFIG_NET_BUF_POOL_USAGE)
	uint32_t timestamp, endtime;
#endif

	BT_DBG("started");

	while (1) {
		BT_DBG("calling fifo_get_wait");
		buf = net_buf_get(&bt_dev.rx_queue, K_FOREVER);
#if defined(CONFIG_NET_BUF_POOL_USAGE)
		timestamp = k_uptime_get_32();
		if ((timestamp - buf->timestamp) > NET_BUF_TIMESTAMP_CHECK_TIME) {
			BT_WARN("Rx thread get time %d ms", (timestamp - buf->timestamp));
		}
#endif
        if(!buf){
            BT_ERR("No Memory!");
            return;
        }

		BT_DBG("buf %p type %u len %u", buf, bt_buf_get_type(buf),
		       buf->len);

		if (atomic_test_bit(bt_dev.flags, BT_DEV_RX_THREAD_EXIT)) {
			atomic_clear_bit(bt_dev.flags, BT_DEV_RX_THREAD_EXIT);
			return;
		}

		switch (bt_buf_get_type(buf)) {
#if defined(CONFIG_BT_CONN)
		case BT_BUF_ACL_IN:
			hci_acl(buf);
			break;
		case BT_BUF_SCO_IN:
			hci_sco(buf);
			break;
#endif /* CONFIG_BT_CONN */
#if defined(CONFIG_BT_ISO)
		case BT_BUF_ISO_IN:
			hci_iso(buf);
			break;
#endif /* CONFIG_BT_ISO */
		case BT_BUF_EVT:
			hci_event(buf);
			break;
		default:
			BT_ERR("Unknown buf type %u", bt_buf_get_type(buf));
			net_buf_unref(buf);
			break;
		}

#if defined(CONFIG_NET_BUF_POOL_USAGE)
		endtime = k_uptime_get_32();
		if ((endtime - timestamp) > NET_BUF_TIMESTAMP_CHECK_TIME) {
			BT_WARN("Rx thread proc time %d ms", (endtime - timestamp));
		}
#endif

		/* Make sure we don't hog the CPU if the rx_queue never
		 * gets empty.
		 */
		k_yield();
	}
}
#endif /* !CONFIG_BT_RECV_IS_RX_THREAD */

int bt_enable(bt_ready_cb_t cb)
{
	int err;

	if (!bt_dev.drv) {
		BT_ERR("No HCI driver registered");
		return -ENODEV;
	}

	if (atomic_test_and_set_bit(bt_dev.flags, BT_DEV_ENABLE)) {
		return -EALREADY;
	}

	k_poll_signal_init(&bt_dev.ncmd_signal);
	k_poll_signal_init(&bt_dev.brpkts_signal);
	k_poll_signal_init(&bt_dev.lepkts_signal);

	bt_internal_pool_init();
#if defined(CONFIG_BT_PROPERTY)
	bt_store_br_init();
	if (bt_property_get_br_name(bt_dev.br_name, sizeof(bt_dev.br_name)) <= 0) {
		bt_set_br_name(CONFIG_BT_DEVICE_NAME);
	}
#if 0
		err = bt_settings_init();
		if (err) {
			return err;
		}
#endif
#else
	bt_set_name(CONFIG_BT_DEVICE_NAME);
#endif

	ready_cb = cb;

	/* TX thread */
	k_thread_create(&tx_thread_data, (k_thread_stack_t)tx_thread_stack,
			K_THREAD_STACK_SIZEOF(tx_thread_stack),
			hci_tx_thread, NULL, NULL, NULL,
			K_PRIO_COOP(CONFIG_BT_HCI_TX_PRIO),
			0, K_NO_WAIT);
	//k_thread_name_set(&tx_thread_data, "BT TX");

#if !defined(CONFIG_BT_RECV_IS_RX_THREAD)
	/* RX thread */
	k_thread_create(&rx_thread_data, (k_thread_stack_t)rx_thread_stack,
			K_THREAD_STACK_SIZEOF(rx_thread_stack),
			(k_thread_entry_t)hci_rx_thread, NULL, NULL, NULL,
			K_PRIO_COOP(CONFIG_BT_RX_PRIO),
			0, K_NO_WAIT);
	//k_thread_name_set(&rx_thread_data, "BT RX");
#endif

	if (IS_ENABLED(CONFIG_BT_TINYCRYPT_ECC)) {
		bt_hci_ecc_init();
	}

	err = bt_dev.drv->open();
	if (err) {
		BT_ERR("HCI driver open failed (%d)", err);
		return err;
	}

	bt_monitor_send(BT_MONITOR_OPEN_INDEX, NULL, 0);
	hci_data_log_init();

	if (!cb) {
		return bt_init();
	}

	k_work_submit(&bt_dev.init);
	return 0;
}

bool bt_is_ready(void)
{
	return atomic_test_bit(bt_dev.flags, BT_DEV_READY);
}

int bt_set_name(const char *name)
{
#if defined(CONFIG_BT_DEVICE_NAME_DYNAMIC) || defined(CONFIG_BT_PROPERTY)
	size_t len = strlen(name);
	int err;

	if (len > CONFIG_BT_DEVICE_NAME_MAX) {
		return -ENOMEM;
	}

	if (!strcmp(bt_dev.name, name)) {
		return 0;
	}

	strncpy(bt_dev.name, name, len);
	bt_dev.name[len] = '\0';

	if (IS_ENABLED(CONFIG_BT_PROPERTY)) {
		err = bt_property_set(CFG_BLE_NAME, bt_dev.name, len);
		if (err) {
			BT_WARN("Unable to store name");
		}
	}

	return 0;
#else
	return -ENOMEM;
#endif
}

const char *bt_get_name(void)
{
#if defined(CONFIG_BT_DEVICE_NAME_DYNAMIC) || defined(CONFIG_BT_PROPERTY)
	return bt_dev.name;
#else
	return CONFIG_BT_DEVICE_NAME;
#endif
}

bool bt_addr_le_is_bonded(uint8_t id, const bt_addr_le_t *addr)
{
	if (IS_ENABLED(CONFIG_BT_SMP)) {
		struct bt_keys *keys = bt_keys_find_addr(id, addr);

		/* if there are any keys stored then device is bonded */
		return keys && keys->keys;
	} else {
		return false;
	}
}

#if defined(CONFIG_BT_WHITELIST)
int bt_le_whitelist_add(const bt_addr_le_t *addr)
{
	struct bt_hci_cp_le_add_dev_to_wl *cp;
	struct net_buf *buf;
	int err;

	buf = bt_hci_cmd_create(BT_HCI_OP_LE_ADD_DEV_TO_WL, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	bt_addr_le_copy(&cp->addr, addr);

	err = bt_hci_cmd_send_sync(BT_HCI_OP_LE_ADD_DEV_TO_WL, buf, NULL);
	if (err) {
		BT_ERR("Failed to add device to whitelist");

		return err;
	}

	return 0;
}

int bt_le_whitelist_rem(const bt_addr_le_t *addr)
{
	struct bt_hci_cp_le_rem_dev_from_wl *cp;
	struct net_buf *buf;
	int err;

	if (!atomic_test_bit(bt_dev.flags, BT_DEV_READY)) {
		return -EAGAIN;
	}

	buf = bt_hci_cmd_create(BT_HCI_OP_LE_REM_DEV_FROM_WL, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	bt_addr_le_copy(&cp->addr, addr);

	err = bt_hci_cmd_send_sync(BT_HCI_OP_LE_REM_DEV_FROM_WL, buf, NULL);
	if (err) {
		BT_ERR("Failed to remove device from whitelist");
		return err;
	}

	return 0;
}

int bt_le_whitelist_clear(void)
{
	int err;

	if (!atomic_test_bit(bt_dev.flags, BT_DEV_READY)) {
		return -EAGAIN;
	}

	err = bt_hci_cmd_send_sync(BT_HCI_OP_LE_CLEAR_WL, NULL, NULL);
	if (err) {
		BT_ERR("Failed to clear whitelist");
		return err;
	}

	return 0;
}
#endif /* defined(CONFIG_BT_WHITELIST) */

int bt_le_set_chan_map(uint8_t chan_map[5])
{
	struct bt_hci_cp_le_set_host_chan_classif *cp;
	struct net_buf *buf;

	if (!IS_ENABLED(CONFIG_BT_CENTRAL)) {
		return -ENOTSUP;
	}

	if (!BT_CMD_TEST(bt_dev.supported_commands, 27, 3)) {
		BT_WARN("Set Host Channel Classification command is "
			"not supported");
		return -ENOTSUP;
	}

	buf = bt_hci_cmd_create(BT_HCI_OP_LE_SET_HOST_CHAN_CLASSIF,
				sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));

	memcpy(&cp->ch_map[0], &chan_map[0], 4);
	cp->ch_map[4] = chan_map[4] & BIT_MASK(5);

	return bt_hci_cmd_send_sync(BT_HCI_OP_LE_SET_HOST_CHAN_CLASSIF,
				    buf, NULL);
}

void bt_data_parse(struct net_buf_simple *ad,
		   bool (*func)(struct bt_data *data, void *user_data),
		   void *user_data)
{
	while (ad->len > 1) {
		struct bt_data data;
		uint8_t len;

		len = net_buf_simple_pull_u8(ad);
		if (len == 0U) {
			/* Early termination */
			return;
		}

		if (len > ad->len) {
			BT_WARN("Malformed data");
			return;
		}

		data.type = net_buf_simple_pull_u8(ad);
		data.data_len = len - 1;
		data.data = ad->data;

		if (!func(&data, user_data)) {
			return;
		}

		net_buf_simple_pull(ad, len - 1);
	}
}

#if defined(CONFIG_BT_BREDR)
#if 0	/* Replace original discovery */
static int br_start_inquiry(const struct bt_br_discovery_param *param)
{
	const uint8_t iac[3] = { 0x33, 0x8b, 0x9e };
	struct bt_hci_op_inquiry *cp;
	struct net_buf *buf;

	buf = bt_hci_cmd_create(BT_HCI_OP_INQUIRY, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));

	cp->length = param->length;
	cp->num_rsp = 0xff; /* we limit discovery only by time */

	memcpy(cp->lap, iac, 3);
	if (param->limited) {
		cp->lap[0] = 0x00;
	}

	return bt_hci_cmd_send_sync(BT_HCI_OP_INQUIRY, buf, NULL);
}

static bool valid_br_discov_param(const struct bt_br_discovery_param *param,
				  size_t num_results)
{
	if (!num_results || num_results > 255) {
		return false;
	}

	if (!param->length || param->length > 0x30) {
		return false;
	}

	return true;
}

int bt_br_discovery_start(const struct bt_br_discovery_param *param,
			  struct bt_br_discovery_result *results, size_t cnt,
			  bt_br_discovery_cb_t cb)
{
	int err;

	BT_DBG("");

	if (!valid_br_discov_param(param, cnt)) {
		return -EINVAL;
	}

	if (atomic_test_bit(bt_dev.flags, BT_DEV_INQUIRY)) {
		return -EALREADY;
	}

	err = br_start_inquiry(param);
	if (err) {
		return err;
	}

	atomic_set_bit(bt_dev.flags, BT_DEV_INQUIRY);

	(void)memset(results, 0, sizeof(*results) * cnt);

	discovery_cb = cb;
	discovery_results = results;
	discovery_results_size = cnt;
	discovery_results_count = 0;

	return 0;
}

int bt_br_discovery_stop(void)
{
	int err;
	int i;

	BT_DBG("");

	if (!atomic_test_bit(bt_dev.flags, BT_DEV_INQUIRY)) {
		return -EALREADY;
	}

	err = bt_hci_cmd_send_sync(BT_HCI_OP_INQUIRY_CANCEL, NULL, NULL);
	if (err) {
		return err;
	}

	for (i = 0; i < discovery_results_count; i++) {
		struct discovery_priv *priv;
		struct bt_hci_cp_remote_name_cancel *cp;
		struct net_buf *buf;

		priv = (struct discovery_priv *)&discovery_results[i]._priv;

		if (!priv->resolving) {
			continue;
		}

		buf = bt_hci_cmd_create(BT_HCI_OP_REMOTE_NAME_CANCEL,
					sizeof(*cp));
		if (!buf) {
			continue;
		}

		cp = net_buf_add(buf, sizeof(*cp));
		bt_addr_copy(&cp->bdaddr, &discovery_results[i].addr);

		bt_hci_cmd_send_sync(BT_HCI_OP_REMOTE_NAME_CANCEL, buf, NULL);
	}

	atomic_clear_bit(bt_dev.flags, BT_DEV_INQUIRY);

	discovery_cb = NULL;
	discovery_results = NULL;
	discovery_results_size = 0;
	discovery_results_count = 0;

	return 0;
}
#endif

static int write_scan_enable(uint8_t scan)
{
	struct net_buf *buf;
	int err;

	BT_DBG("type %u", scan);

	buf = bt_hci_cmd_create(BT_HCI_OP_WRITE_SCAN_ENABLE, 1);
	if (!buf) {
		return -ENOBUFS;
	}

	net_buf_add_u8(buf, scan);
	err = bt_hci_cmd_send_sync(BT_HCI_OP_WRITE_SCAN_ENABLE, buf, NULL);
	if (err) {
		return err;
	}

	atomic_set_bit_to(bt_dev.flags, BT_DEV_ISCAN,
			  (scan & BT_BREDR_SCAN_INQUIRY));
	atomic_set_bit_to(bt_dev.flags, BT_DEV_PSCAN,
			  (scan & BT_BREDR_SCAN_PAGE));

	return 0;
}

int bt_br_set_connectable(bool enable)
{
	if (enable) {
		if (atomic_test_bit(bt_dev.flags, BT_DEV_PSCAN)) {
			return -EALREADY;
		} else {
			return write_scan_enable(BT_BREDR_SCAN_PAGE);
		}
	} else {
		if (!atomic_test_bit(bt_dev.flags, BT_DEV_PSCAN)) {
			return -EALREADY;
		} else {
			return write_scan_enable(BT_BREDR_SCAN_DISABLED);
		}
	}
}

int bt_br_set_discoverable(bool enable)
{
	if (enable) {
		if (atomic_test_bit(bt_dev.flags, BT_DEV_ISCAN)) {
			return -EALREADY;
		}

		if (!atomic_test_bit(bt_dev.flags, BT_DEV_PSCAN)) {
			return -EPERM;
		}

		return write_scan_enable(BT_BREDR_SCAN_INQUIRY |
					 BT_BREDR_SCAN_PAGE);
	} else {
		if (!atomic_test_bit(bt_dev.flags, BT_DEV_ISCAN)) {
			return -EALREADY;
		}

		return write_scan_enable(BT_BREDR_SCAN_PAGE);
	}
}
#endif /* CONFIG_BT_BREDR */

#if defined(CONFIG_BT_ECC)
int bt_pub_key_gen(struct bt_pub_key_cb *new_cb)
{
	int err;

	/*
	 * We check for both "LE Read Local P-256 Public Key" and
	 * "LE Generate DH Key" support here since both commands are needed for
	 * ECC support. If "LE Generate DH Key" is not supported then there
	 * is no point in reading local public key.
	 */
	if (!BT_CMD_TEST(bt_dev.supported_commands, 34, 1) ||
	    !BT_CMD_TEST(bt_dev.supported_commands, 34, 2)) {
		BT_WARN("ECC HCI commands not available");
		return -ENOTSUP;
	}

	new_cb->_next = pub_key_cb;
	pub_key_cb = new_cb;

	if (atomic_test_and_set_bit(bt_dev.flags, BT_DEV_PUB_KEY_BUSY)) {
		return 0;
	}

	atomic_clear_bit(bt_dev.flags, BT_DEV_HAS_PUB_KEY);

	err = bt_hci_cmd_send_sync(BT_HCI_OP_LE_P256_PUBLIC_KEY, NULL, NULL);
	if (err) {
		BT_ERR("Sending LE P256 Public Key command failed");
		atomic_clear_bit(bt_dev.flags, BT_DEV_PUB_KEY_BUSY);
		pub_key_cb = NULL;
		return err;
	}

	return 0;
}

const uint8_t *bt_pub_key_get(void)
{
	if (atomic_test_bit(bt_dev.flags, BT_DEV_HAS_PUB_KEY)) {
		return pub_key;
	}

	return NULL;
}

int bt_dh_key_gen(const uint8_t remote_pk[64], bt_dh_key_cb_t cb)
{
	struct bt_hci_cp_le_generate_dhkey *cp;
	struct net_buf *buf;
	int err;

	if (dh_key_cb == cb) {
		return -EALREADY;
	}

	if (dh_key_cb || atomic_test_bit(bt_dev.flags, BT_DEV_PUB_KEY_BUSY)) {
		return -EBUSY;
	}

	if (!atomic_test_bit(bt_dev.flags, BT_DEV_HAS_PUB_KEY)) {
		return -EADDRNOTAVAIL;
	}

	dh_key_cb = cb;

	buf = bt_hci_cmd_create(BT_HCI_OP_LE_GENERATE_DHKEY, sizeof(*cp));
	if (!buf) {
		dh_key_cb = NULL;
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	memcpy(cp->key, remote_pk, sizeof(cp->key));

	err = bt_hci_cmd_send_sync(BT_HCI_OP_LE_GENERATE_DHKEY, buf, NULL);
	if (err) {
		dh_key_cb = NULL;
		return err;
	}

	return 0;
}
#endif /* CONFIG_BT_ECC */

/* Actions add start */

int bt_reg_evt_cb_func(bt_evt_cb_func_t cb)
{
	evt_cb_func = cb;
	return 0;
}

static void bt_evt_callback(uint16_t event, void *param)
{
	if (evt_cb_func) {
		evt_cb_func(event, param);
	}
}

#if defined(CONFIG_BT_BREDR)
static int br_write_mode_type(uint16_t opcode, uint8_t mode_type, bool send_sync)
{
	struct bt_hci_cp_write_mode_type *cp;
	struct net_buf *buf;

	buf = bt_hci_cmd_create(opcode, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->mode_type = mode_type;

	if (send_sync) {
		return bt_hci_cmd_send_sync(opcode, buf, NULL);
	} else {
		return bt_hci_cmd_send(opcode, buf);
	}
}

int bt_br_set_page_timeout(uint32_t timeout_ms)
{
	struct net_buf *buf;
	uint16_t value;

	buf = bt_hci_cmd_create(BT_HCI_OP_WRITE_PAGE_TIMEOUT, sizeof(uint16_t));
	if (!buf) {
		return -ENOBUFS;
	}

	value = (timeout_ms * 1000 / 625);
	net_buf_add_le16(buf, value);

	return bt_hci_cmd_send_sync(BT_HCI_OP_WRITE_PAGE_TIMEOUT, buf, NULL);
}

int bt_br_write_class_of_device(uint32_t class_of_device)
{
	struct net_buf *buf;
	struct bt_hci_write_clase_of_device *class_cp;
	if (class_of_device) {
		bt_class_of_device = class_of_device;
	}

	buf = bt_hci_cmd_create(BT_HCI_OP_WRITE_CLASS_OF_DEVICE, sizeof(*class_cp));
	if (!buf) {
		return -ENOBUFS;
	}

	class_cp = net_buf_add(buf, sizeof(*class_cp));
	class_cp->class_of_device[0] = (uint8_t)bt_class_of_device;
	class_cp->class_of_device[1] = (uint8_t)(bt_class_of_device >> 8);
	class_cp->class_of_device[2] = (uint8_t)(bt_class_of_device >> 16);

	return bt_hci_cmd_send_sync(BT_HCI_OP_WRITE_CLASS_OF_DEVICE, buf, NULL);
}

static int br_ext_cmd_init(void)
{
	struct net_buf *buf;
	struct bt_hci_write_local_name *name_cp;
	struct bt_hci_write_clase_of_device *class_cp;
	struct bt_hci_cp_wd_link_policy *policy_cp;
	struct bt_hci_cp_write_eir *eir;
	uint8_t eir_len;
	int err;

	/* Enable Inquiry results with RSSI or extended Inquiry */
	err = br_write_mode_type(BT_HCI_OP_WRITE_INQUIRY_MODE, 0x02, true);
	if (err) {
		return err;
	}

	/* Write default link policy */
	buf = bt_hci_cmd_create(BT_HCI_OP_WRITE_DEFAULT_LINK_POLICY, sizeof(*policy_cp));
	if (!buf) {
		return -ENOBUFS;
	}

	policy_cp = net_buf_add(buf, sizeof(*policy_cp));
	policy_cp->link_policy = 0x0005;		/* Enable mastre slave switch, enable sniff mode */

	err = bt_hci_cmd_send_sync(BT_HCI_OP_WRITE_DEFAULT_LINK_POLICY, buf, NULL);
	if (err) {
		return err;
	}

	/* Set class of device */
	buf = bt_hci_cmd_create(BT_HCI_OP_WRITE_CLASS_OF_DEVICE, sizeof(*class_cp));
	if (!buf) {
		return -ENOBUFS;
	}

	class_cp = net_buf_add(buf, sizeof(*class_cp));
	class_cp->class_of_device[0] = (uint8_t)bt_class_of_device;
	class_cp->class_of_device[1] = (uint8_t)(bt_class_of_device >> 8);
	class_cp->class_of_device[2] = (uint8_t)(bt_class_of_device >> 16);

	err = bt_hci_cmd_send_sync(BT_HCI_OP_WRITE_CLASS_OF_DEVICE, buf, NULL);
	if (err) {
		return err;
	}

	/* Set local name */
	buf = bt_hci_cmd_create(BT_HCI_OP_WRITE_LOCAL_NAME, sizeof(*name_cp));
	if (!buf) {
		return -ENOBUFS;
	}

	name_cp = net_buf_add(buf, sizeof(*name_cp));
	memset((char *)name_cp->local_name, 0, sizeof(name_cp->local_name));
	bt_read_br_name(name_cp->local_name, sizeof(name_cp->local_name));
	err = bt_hci_cmd_send_sync(BT_HCI_OP_WRITE_LOCAL_NAME, buf, NULL);
	if (err) {
		return err;
	}

	/* Write extended inquiry response */
	buf = bt_hci_cmd_create(BT_HCI_OP_WRITE_EXTENDED_INQUIRY_RSP, sizeof(*eir));
	if (!buf) {
		return -ENOBUFS;
	}

	eir = net_buf_add(buf, sizeof(*eir));
	eir->fcc = 0;	/* FEC not required */
	memset(eir->eirdata, 0, 240);

	/* Device name */
	eir->eirdata[1] = BT_DATA_NAME_COMPLETE;
	bt_read_br_name(&eir->eirdata[2], (240 - 2));
	eir->eirdata[0] = 1 + strlen(&eir->eirdata[2]);
	eir_len = 1 + eir->eirdata[0];

	/* Device ID */
	eir->eirdata[eir_len] = 9;
	eir->eirdata[eir_len + 1] = BT_DATA_DEVICE_ID;
	memcpy(&eir->eirdata[eir_len + 2], (void *)bt_device_id, sizeof(bt_device_id));

	err = bt_hci_cmd_send_sync(BT_HCI_OP_WRITE_EXTENDED_INQUIRY_RSP, buf, NULL);
	return err;
}

static void mode_change(struct net_buf *buf)
{
	struct bt_hci_evt_mode_change *evt = (void *)buf->data;
	uint16_t handle = sys_le16_to_cpu(evt->handle);
	uint16_t interval = sys_le16_to_cpu(evt->interval);
	struct bt_conn *conn;

	conn = bt_conn_lookup_handle(handle);
	if (!conn) {
		BT_ERR("Can't find conn for handle 0x%x", handle);
		return;
	}

	if (evt->status || (conn->type != BT_CONN_TYPE_BR)) {
		BT_ERR("Error %d, type %d", evt->status, conn->type);
	} else {
		BT_SYS_INF("hdl 0x%x mode %d intervel %d", handle, evt->mode, interval);
		if (evt->mode == BT_ACTIVE_MODE || evt->mode == BT_SNIFF_MODE) {
			conn->br.mode = evt->mode;
			conn->br.mode_entering = 0;
			conn->br.mode_exiting = 0;
			bt_conn_notify_mode_change(conn, evt->mode, interval);
		}
	}

	bt_conn_unref(conn);
}

static void remote_name_request_complete(struct net_buf *buf)
{
	struct bt_hci_evt_remote_name_req_complete *evt = (void *)buf->data;

	if (atomic_test_and_clear_bit(bt_dev.flags, BT_DEV_REMOTE_NAME_REQ)) {
		if (evt->status == 0 && remote_name_cb) {
			remote_name_cb(&evt->bdaddr, evt->name);
		}
	}
}

static void inquiry_complete(struct net_buf *buf)
{
	struct bt_hci_evt_inquiry_complete *evt = (void *)buf->data;

	if (evt->status) {
		BT_ERR("Failed to complete inquiry");
	} else if (discovery_cb) {
		discovery_cb(NULL);
	}

	atomic_clear_bit(bt_dev.flags, BT_DEV_INQUIRY);
}

struct discovery_priv {
	uint16_t clock_offset;
	uint8_t pscan_rep_mode;
	uint8_t resolving;
} __packed;

static void inquiry_result_with_rssi(struct net_buf *buf)
{
	struct bt_hci_evt_inquiry_result_with_rssi *evt;
	uint8_t num_reports = net_buf_pull_u8(buf);

	if (!atomic_test_bit(bt_dev.flags, BT_DEV_INQUIRY)) {
		return;
	}

	BT_DBG("number of results: %u", num_reports);

	evt = (void *)buf->data;
	while (num_reports--) {
		struct bt_br_discovery_result result;
		struct discovery_priv priv;

		BT_DBG("%s rssi %d dBm", bt_addr_str(&evt->addr), evt->rssi);

		memset(&result, 0, sizeof(result));
		memcpy(&result.addr, &evt->addr, sizeof(bt_addr_t));

		priv.pscan_rep_mode = evt->pscan_rep_mode;
		priv.clock_offset = evt->clock_offset;

		memcpy(result.cod, evt->cod, 3);
		result.rssi = evt->rssi;
		if (discovery_cb) {
			discovery_cb(&result);
		}

		/*
		 * Get next report iteration by moving pointer to right offset
		 * in buf according to spec 4.2, Vol 2, Part E, 7.7.33.
		 */
		evt = net_buf_pull(buf, sizeof(*evt));
	}
}

#define EIR_SHORT_NAME		0x08
#define EIR_COMPLETE_NAME	0x09

static void eir_resolve_name_device_id(struct bt_br_discovery_result *result, uint8_t *eir)
{
	int len = 240;

	while (len) {
		if (len < 2) {
			break;
		};

		/* Look for early termination */
		if (!eir[0]) {
			break;
		}

		/* Check if field length is correct */
		if (eir[0] > len - 1) {
			break;
		}

		switch (eir[1]) {
		case BT_DATA_NAME_SHORTENED:
		case BT_DATA_NAME_COMPLETE:
			result->name = &eir[2];
			result->len = eir[0] - 1;
			break;
		case BT_DATA_DEVICE_ID:
			memcpy(result->device_id, &eir[2], sizeof(result->device_id));
			break;
		default:
			break;
		}

		/* Parse next AD Structure */
		len -= eir[0] + 1;
		eir += eir[0] + 1;
	}
}

static void extended_inquiry_result(struct net_buf *buf)
{
	struct bt_hci_evt_extended_inquiry_result *evt = (void *)buf->data;
	struct bt_br_discovery_result result;
	struct discovery_priv priv;

	if (!atomic_test_bit(bt_dev.flags, BT_DEV_INQUIRY)) {
		return;
	}

	BT_DBG("%s rssi %d dBm", bt_addr_str(&evt->addr), evt->rssi);

	memset(&result, 0, sizeof(result));
	memcpy(&result.addr, &evt->addr, sizeof(bt_addr_t));

	priv.pscan_rep_mode = evt->pscan_rep_mode;
	priv.clock_offset = evt->clock_offset;

	result.rssi = evt->rssi;
	memcpy(result.cod, evt->cod, 3);
	eir_resolve_name_device_id(&result, evt->eir);

	if (discovery_cb) {
		discovery_cb(&result);
	}
}

static int br_start_inquiry(const struct bt_br_discovery_param *param)
{
	struct bt_hci_op_inquiry *cp;
	struct net_buf *buf;

	buf = bt_hci_cmd_create(BT_HCI_OP_INQUIRY, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->length = param->length;
	cp->num_rsp = param->num_responses;
	if (param->limited) {
		memcpy(cp->lap, LIAC, 3);
	} else {
		memcpy(cp->lap, GIAC, 3);
	}

	return bt_hci_cmd_send_sync(BT_HCI_OP_INQUIRY, buf, NULL);
}

static bool valid_br_discov_param(const struct bt_br_discovery_param *param)
{
	if (!param->length || param->length > 0x30) {
		return false;
	}

	return true;
}

int bt_br_discovery_start(const struct bt_br_discovery_param *param,
			  bt_br_discovery_cb_t cb)
{
	int err;

	BT_DBG("");

	if (!valid_br_discov_param(param)) {
		return -EINVAL;
	}

	if (atomic_test_bit(bt_dev.flags, BT_DEV_INQUIRY)) {
		return -EALREADY;
	}

	err = br_start_inquiry(param);
	if (err) {
		return err;
	}

	atomic_set_bit(bt_dev.flags, BT_DEV_INQUIRY);
	discovery_cb = cb;

	return 0;
}

int bt_br_discovery_stop(void)
{
	int err;

	BT_DBG("");

	if (!atomic_test_bit(bt_dev.flags, BT_DEV_INQUIRY)) {
		return -EALREADY;
	}

	err = bt_hci_cmd_send_sync(BT_HCI_OP_INQUIRY_CANCEL, NULL, NULL);
	if (err) {
		return err;
	}

	if (discovery_cb) {
		discovery_cb(NULL);
		discovery_cb = NULL;
	}

	atomic_clear_bit(bt_dev.flags, BT_DEV_INQUIRY);

	return 0;
}

static int request_name(const bt_addr_t *addr, uint8_t pscan, uint16_t offset)
{
	struct bt_hci_cp_remote_name_request *cp;
	struct net_buf *buf;

	buf = bt_hci_cmd_create(BT_HCI_OP_REMOTE_NAME_REQUEST, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));

	bt_addr_copy(&cp->bdaddr, addr);
	cp->pscan_rep_mode = pscan;
	cp->reserved = 0x00; /* reserver, should be set to 0x00 */
	cp->clock_offset = offset;

	return bt_hci_cmd_send(BT_HCI_OP_REMOTE_NAME_REQUEST, buf);
}

int bt_br_remote_name_request(const bt_addr_t *addr, bt_br_remote_name_cb_t cb)
{
	remote_name_cb = cb;
	atomic_set_bit(bt_dev.flags, BT_DEV_REMOTE_NAME_REQ);

	if (request_name(addr, 0x02/* R2 */, 0)) {
		remote_name_cb = NULL;
		atomic_clear_bit(bt_dev.flags, BT_DEV_REMOTE_NAME_REQ);
		return -EIO;
	}

	return 0;
}

int bt_br_write_scan_enable(uint8_t scan, complete_event_cb cb)
{
	struct net_buf *buf;
	int err;

	BT_DBG("type %u", scan);

	buf = bt_hci_cmd_create(BT_HCI_OP_WRITE_SCAN_ENABLE, 1);
	if (!buf) {
		return -ENOBUFS;
	}

	cmd(buf)->event_cb = cb;

	net_buf_add_u8(buf, scan);
	err = bt_hci_cmd_send(BT_HCI_OP_WRITE_SCAN_ENABLE, buf);
	if (err) {
		return err;
	}

	return 0;
}

int bt_br_write_iac(bool limit_iac)
{
	struct bt_hci_cp_write_current_iac_lap *cp;
	struct net_buf *buf;

	buf = bt_hci_cmd_create(BT_HCI_OP_WRITE_CURRENT_IAC_LAP, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->num_current_iac = 1;
	if (limit_iac) {
		memcpy(cp->iac_lap, LIAC, 3);
	} else {
		memcpy(cp->iac_lap, GIAC, 3);
	}

	return bt_hci_cmd_send(BT_HCI_OP_WRITE_CURRENT_IAC_LAP, buf);
}

static int br_write_scan_activity(uint16_t opcode, uint16_t interval, uint16_t windown)
{
	struct bt_hci_write_scan_activity *cp;
	struct net_buf *buf;

	buf = bt_hci_cmd_create(opcode, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->interval = sys_cpu_to_le16(interval);
	cp->windown = sys_cpu_to_le16(windown);

	return bt_hci_cmd_send(opcode, buf);
}

int bt_br_write_page_scan_activity(uint16_t interval, uint16_t windown)
{
	return br_write_scan_activity(BT_HCI_OP_WRITE_PAGE_SCAN_ACTIVITY, interval, windown);
}

int bt_br_write_inquiry_scan_activity(uint16_t interval, uint16_t windown)
{
	return br_write_scan_activity(BT_HCI_OP_WRITE_INQUIRY_SCAN_ACTIVITY, interval, windown);
}

int bt_br_write_inquiry_scan_type(uint8_t type)
{
	return br_write_mode_type(BT_HCI_OP_WRITE_INQUIRY_SCAN_TYPE, type, false);
}

int bt_br_write_page_scan_type(uint8_t type)
{
	return br_write_mode_type(BT_HCI_OP_WRITE_PAGE_SCAN_TYPE, type, false);
}

int bt_br_read_rssi(struct bt_conn *conn, complete_event_cb cb)
{
	struct bt_hci_cp_read_rssi *cp;
	struct net_buf *buf;

	if (conn->state != BT_CONN_CONNECTED) {
		return -ENOTCONN;
	}

	buf = bt_hci_cmd_create(BT_HCI_OP_READ_RSSI, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->handle = sys_cpu_to_le16(conn->handle);

	cmd(buf)->event_cb = cb;

	return bt_hci_cmd_send(BT_HCI_OP_READ_RSSI, buf);
}

int bt_br_read_link_quality(struct bt_conn *conn, complete_event_cb cb)
{
	struct bt_hci_cp_read_link_quality *cp;
	struct net_buf *buf;

	if (conn->state != BT_CONN_CONNECTED) {
		return -ENOTCONN;
	}

	buf = bt_hci_cmd_create(BT_HCI_OP_READ_LINK_QUALITY, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->handle = sys_cpu_to_le16(conn->handle);

	cmd(buf)->event_cb = cb;

	return bt_hci_cmd_send(BT_HCI_OP_READ_LINK_QUALITY, buf);
}

void bt_set_share_bd_addr(bool set, uint8_t *mac)
{
	if (set) {
		bt_dev.share_db_addr_valid = 1;
		memcpy(bt_dev.share_db_addr.val, mac, sizeof(bt_dev.share_db_addr));
	} else {
		bt_dev.share_db_addr_valid = 0;
		memset(bt_dev.share_db_addr.val, 0, sizeof(bt_dev.share_db_addr));
	}

	bt_store_share_db_addr_valid_set(set);
	/* FIXME: may need to change the policy */
#if 0
	if (bt_dev.id_count) {
		if (set) {
			bt_setup_public_id_share_addr(&bt_dev.share_db_addr);
		} else {
			bt_setup_public_id_share_addr(&bt_dev.read_bd_addr);
		}
	}
#endif
}

uint32_t bt_get_capable(void)
{
	return BT_SUPPORT_CAPABLE;
}

int bt_init_class_of_device(uint32_t classOfDevice)
{
	if (classOfDevice) {
		bt_class_of_device = classOfDevice;
		return 0;
	}

	return -EINVAL;
}

int bt_init_device_id(uint16_t *did)
{
	if (did[0]) {
		memcpy(bt_device_id, did, sizeof(bt_device_id));
		return 0;
	}

	return -EINVAL;
}

#endif /* CONFIG_BT_BREDR */

uint16_t bt_hci_get_cmd_opcode(struct net_buf *buf)
{
	return cmd(buf)->opcode;
}

uint8_t bt_buf_pkt_len_check(enum bt_buf_type type, int len)
{
	if(type != BT_BUF_ISO_IN){
		uint16_t rx_max_len = L2CAP_BR_MAX_MTU_A2DP + BT_HCI_ACL_HDR_SIZE + BT_L2CAP_HDR_SIZE;
		if (len > rx_max_len) {
			//BT_ERR("Too length %d(%d)\n", len, rx_max_len);
			return 0;
		}
	}else {
		return 0;
	}
	return 1;
}

struct net_buf *bt_buf_get_rx_len(enum bt_buf_type type, int timeout, int len)
{
	struct net_buf *buf;
	uint16_t alloc_len;
	if(type != BT_BUF_ISO_IN){
		uint16_t rx_max_len = L2CAP_BR_MAX_MTU_A2DP + BT_HCI_ACL_HDR_SIZE + BT_L2CAP_HDR_SIZE;

		if (len > rx_max_len) {
			BT_ERR("Too length %d(%d)\n", len, rx_max_len);
			/* For check, later just drop this packet */
			//__ASSERT(false, "Too larger!!");
			return NULL;
		}
	}

	__ASSERT(type == BT_BUF_EVT || type == BT_BUF_ACL_IN || type == BT_BUF_SCO_IN ||
		 type == BT_BUF_ISO_IN, "Invalid buffer type requested");

	alloc_len = len + BT_BUF_RESERVE;

#if defined(CONFIG_BT_HCI_ACL_FLOW_CONTROL)
	if (type == BT_BUF_EVT) {
		buf = net_buf_alloc_len(&hci_rx_pool, alloc_len, timeout);
#if defined(CONFIG_BT_ISO)
	} else if (type == BT_BUF_ISO_IN) {
		return bt_iso_get_rx_len(timeout, alloc_len);
#endif /* CONFIG_BT_ISO */
	} else {
		buf = net_buf_alloc(&acl_in_pool, timeout);
	}
#else
#if defined(CONFIG_BT_ISO)
	if (type == BT_BUF_ISO_IN) {
		return bt_iso_get_rx_len(timeout, alloc_len);
	} else
#endif /* CONFIG_BT_ISO */
	{
		static int max_rx_num = 0;
		int free_buf = k_queue_cnt_sum((struct k_queue *)&hci_rx_pool.free);
		if((free_buf <= 3 && !hci_rx_pool.uninit_count) || (!free_buf &&
			hci_rx_pool.uninit_count <= 3)){
			if(max_rx_num != 4){
				ctrl_set_le_conn_rx_max_num(4);
				max_rx_num = 4;
				BT_INFO("set rx max num 4 (%d)\n",free_buf);
			}
		}else{
			if(max_rx_num != 8){
				ctrl_set_le_conn_rx_max_num(8);
				max_rx_num = 8;
				BT_INFO("set rx max num 8\n");
			}
		}
		buf = net_buf_alloc_len(&hci_rx_pool, alloc_len, timeout);
	}
#endif

	if (buf) {
		net_buf_reserve(buf, BT_BUF_RESERVE);
		bt_buf_set_type(buf, type);
	}

	return buf;
}

static void hci_sco(struct net_buf *buf)
{
	struct bt_hci_sco_hdr *hdr = (void *)buf->data;
	uint16_t handle;
	uint8_t len;
	struct bt_conn *conn;
	uint8_t flags;

	BT_DBG("buf %p", buf);

	handle = sys_le16_to_cpu(hdr->handle);
	flags = bt_acl_flags(handle);
	handle = bt_acl_handle(handle);
	len = hdr->len;

	net_buf_pull(buf, sizeof(*hdr));
	BT_DBG("handle %u len %u flags %u", handle, len, flags);

	if (buf->len != len) {
		BT_ERR("ACL data length mismatch (%u != %u)", buf->len, len);
		net_buf_unref(buf);
		return;
	}

	conn = bt_conn_lookup_handle(handle);
	if (!conn) {
		BT_ERR("Unable to find conn for handle 0x%x", handle);
		net_buf_unref(buf);
		return;
	}

	bt_conn_notify_sco_data(conn, buf->data, len, flags);
	net_buf_unref(buf);

	bt_conn_unref(conn);
}

#if MATCH_PHOENIX_CONTROLLER
static void bt_sco_conn_req(struct bt_hci_evt_conn_request *evt)
{
	struct bt_conn *sco_conn;

	sco_conn = bt_conn_add_sco(&evt->bdaddr, evt->link_type);
	if (!sco_conn) {
		reject_conn(&evt->bdaddr, BT_HCI_ERR_INSUFFICIENT_RESOURCES);
		return;
	}

	if (accept_conn(&evt->bdaddr)) {
		BT_ERR("Error accepting connection from %s",
		       bt_addr_str(&evt->bdaddr));
		reject_conn(&evt->bdaddr, BT_HCI_ERR_UNSPECIFIED);
		bt_sco_cleanup(sco_conn);
		return;
	}

	sco_conn->role = BT_HCI_ROLE_SLAVE;
	bt_conn_set_state(sco_conn, BT_CONN_CONNECT);
	bt_conn_unref(sco_conn);
}
#endif

static int bt_set_br_name(const char *name)
{
#ifdef CONFIG_BT_PROPERTY
	int len = MIN(strlen(name), (sizeof(bt_dev.br_name) - 1));

	memset(bt_dev.br_name, 0, sizeof(bt_dev.br_name));
	strncpy(bt_dev.br_name, name, len);
#endif

	return 0;
}

uint8_t bt_read_br_name(uint8_t *name, uint8_t len)
{
#ifdef CONFIG_BT_PROPERTY
	uint8_t str_len = MIN(strlen(bt_dev.br_name), len);

	memcpy(name, bt_dev.br_name, str_len);
	return str_len;
#else
	uint8_t str_len = MIN(strlen(CONFIG_BT_DEVICE_NAME), len);

	memcpy(name, CONFIG_BT_DEVICE_NAME, str_len);
	return str_len;
#endif
}

uint8_t bt_read_ble_name(uint8_t *name, uint8_t len)
{
#ifdef CONFIG_BT_PROPERTY
	uint8_t str_len = MIN(strlen(bt_dev.name), len);

	memcpy(name, bt_dev.name, str_len);
	return str_len;
#else
	uint8_t str_len = MIN(strlen(CONFIG_BT_DEVICE_NAME), len);

	memcpy(name, CONFIG_BT_DEVICE_NAME, str_len);
	return str_len;
#endif
}

void bt_hci_reg_hf_codec_func(bt_hci_hf_codec_func cb)
{
	get_hf_codec_func = cb;
}

static void bt_thread_exit(void)
{
	struct net_buf *buf;

#ifdef CONFIG_BT_TINYCRYPT_ECC
	if (IS_ENABLED(CONFIG_BT_TINYCRYPT_ECC)) {
		bt_hci_ecc_deinit();
	}
#endif

	/* Exit tx thread */
	atomic_set_bit(bt_dev.flags, BT_DEV_TX_THREAD_EXIT);

	buf = net_buf_alloc(&hci_cmd_pool, K_NO_WAIT);
	if (buf) {
		/* Just need one buf for trigger tx thread run */
		net_buf_put(&bt_dev.cmd_tx_queue, buf);
	} else {
		BT_WARN("Failed to alloc buf for exit tx thread!\n");
	}

	while (atomic_test_bit(bt_dev.flags, BT_DEV_TX_THREAD_EXIT)) {
		k_sleep(K_MSEC(50));
	}

	while ((buf = net_buf_get(&bt_dev.cmd_tx_queue, K_NO_WAIT)) != NULL) {
		net_buf_unref(buf);
	}

	/* Exit tx thread */
#if !defined(CONFIG_BT_RECV_IS_RX_THREAD)
	atomic_set_bit(bt_dev.flags, BT_DEV_RX_THREAD_EXIT);

	buf = bt_buf_get_rx(BT_BUF_EVT, K_NO_WAIT);
	if (buf) {
		/* Just need one buf for trigger rx thread run */
		net_buf_put(&bt_dev.rx_queue, buf);
	} else {
		BT_WARN("Failed to alloc buf for exit rx thread!\n");
	}

	while (atomic_test_bit(bt_dev.flags, BT_DEV_RX_THREAD_EXIT)) {
		k_sleep(K_MSEC(50));
	}

	while ((buf = net_buf_get(&bt_dev.rx_queue, K_NO_WAIT)) != NULL) {
		net_buf_unref(buf);
	}
#endif
}

int bt_disable(void)
{
	BT_INFO("enter\n");

#if WAIT_TODO		/* Controler not support deinit interface now */
	if (bt_dev.drv->close) {
		bt_dev.drv->close();
	}
#endif

#if defined(CONFIG_BT_CONN)
	disconnected_handles_reset();
#endif /* CONFIG_BT_CONN */

	bt_thread_exit();
	atomic_clear_bit(bt_dev.flags, BT_DEV_ENABLE);

	BT_INFO("exit\n");
	return 0;
}

/* For CSB */
static void hci_sync_train_receive(struct net_buf *buf)
{
	/* Wait TODO: Call back csb event */
	/*
	 * struct bt_hci_evt_sync_train_receive *evt = (void *)buf->data;
	 * csb_slave_receive_train_cb(evt);
	 */
}

static void hci_csb_receive(struct net_buf *buf)
{
	/* Wait TODO: Call back csb event */
	/*
	 * struct bt_hci_evt_csb_receive *evt = (void *)buf->data;
	 * csb_slave_receive_data_cb(evt->data, evt->data_len);
	 */
}

static void hci_csb_timeout(struct net_buf *buf)
{
	/* Wait TODO: Call back csb event */
	/*
	 * struct bt_hci_evt_csb_timeout *evt = (void *)buf->data;
	 * csb_slave_timeout_cb(evt->bd_addr, evt->lt_addr);
	 */
}

int bt_csb_set_broadcast_link_key_seed(uint8_t *seed, uint8_t len)
{
	struct bt_hci_cp_csb_link_key_seed *cp;
	struct net_buf *buf;

	if (len > sizeof(*cp)) {
		return -EFBIG;
	}

	buf = bt_hci_cmd_create(BT_HCI_OP_CSB_LINK_KEY_SEED, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	memset(cp->link_key_seed, 0, sizeof(*cp));
	memcpy(cp->link_key_seed, seed, len);

	return bt_hci_cmd_send(BT_HCI_OP_CSB_LINK_KEY_SEED, buf);
}

int bt_csb_set_reserved_lt_addr(uint8_t lt_addr)
{
	struct bt_hci_cp_set_reserved_lt_addr *cp;
	struct net_buf *buf;

	buf = bt_hci_cmd_create(BT_HCI_OP_SET_RESERVED_LT_ADDR, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->lt_addr = lt_addr;

	return bt_hci_cmd_send(BT_HCI_OP_SET_RESERVED_LT_ADDR, buf);
}

int bt_csb_set_CSB_broadcast(uint8_t enable, uint8_t lt_addr, uint16_t interval_min, uint16_t interval_max)
{
	struct bt_hci_cp_set_csb_broadcast *cp;
	struct net_buf *buf;

	buf = bt_hci_cmd_create(BT_HCI_OP_SET_CSB_BROADCAST, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->enable = enable;
	cp->lt_addr = lt_addr;
	cp->lpo_allowed = 0;
	cp->packet_type = 0;
	cp->interval_min = sys_cpu_to_le16(interval_min);
	cp->interval_max = sys_cpu_to_le16(interval_max);
	cp->supervision_timeout = 0;

	return bt_hci_cmd_send(BT_HCI_OP_SET_CSB_BROADCAST, buf);
}

int bt_csb_set_CSB_receive(uint8_t enable, struct bt_hci_evt_sync_train_receive *param, uint16_t timeout)
{
	struct bt_hci_cp_set_csb_receive *cp;
	struct net_buf *buf;

	buf = bt_hci_cmd_create(BT_HCI_OP_SET_CSB_RECEIVE, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->enable = enable;
	memcpy(cp->bd_addr, param->bd_addr, sizeof(cp->bd_addr));
	cp->lt_addr = param->lt_addr;
	cp->interval = param->csb_interval;
	cp->clock_offset = param->clock_offset;
	cp->next_csb_clock = param->next_broadcast_instant;
	cp->csb_supervisionTO = sys_cpu_to_le16(timeout);
	cp->remote_timing_accuracy = 20;
	cp->skip = 0;
	cp->packet_type = 0;
	memcpy(cp->afh_channel_map, param->afh_channel_map, sizeof(cp->afh_channel_map));

	return bt_hci_cmd_send(BT_HCI_OP_SET_CSB_RECEIVE, buf);
}

int bt_csb_write_sync_train_param(uint16_t interval_min, uint16_t interval_max, uint32_t trainTO, uint8_t service_data)
{
	struct bt_hci_cp_write_sync_train_param *cp;
	struct net_buf *buf;

	buf = bt_hci_cmd_create(BT_HCI_OP_WRITE_SYNC_TRAIN_PARAM, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->interval_min = sys_cpu_to_le16(interval_min);
	cp->interval_max = sys_cpu_to_le16(interval_max);
	cp->sync_trainto = sys_cpu_to_le32(trainTO);
	cp->service_data = service_data;

	return bt_hci_cmd_send(BT_HCI_OP_WRITE_SYNC_TRAIN_PARAM, buf);
}

int bt_csb_start_sync_train(void)
{
	return bt_hci_cmd_send(BT_HCI_OP_START_SYNC_TRAIN, NULL);
}

int bt_csb_set_CSB_date(uint8_t lt_addr, uint8_t *data, uint8_t len)
{
	struct bt_hci_cp_set_csb_data *cp;
	struct net_buf *buf;

	if (len > CSB_SET_DATA_LEN) {
		return -EFBIG;
	}

	buf = bt_hci_cmd_create(BT_HCI_OP_SET_CSB_DATA, (sizeof(*cp) + len));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, (sizeof(*cp) + len));
	cp->lt_addr = lt_addr;
	cp->fragmemt = CSB_Single_fragment;
	cp->data_len = len;
	memcpy(cp->data, data, len);

	return bt_hci_cmd_send(BT_HCI_OP_SET_CSB_DATA, buf);
}

int bt_csb_receive_sync_train(uint8_t *mac, uint16_t timeout, uint16_t windown, uint16_t interval)
{
	struct bt_hci_cp_receive_sync_train *cp;
	struct net_buf *buf;

	buf = bt_hci_cmd_create(BT_HCI_OP_RECEIVE_SYNC_TRAIN, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	memcpy(cp->bd_addr, mac, sizeof(cp->bd_addr));
	cp->sync_scan_timeout = sys_cpu_to_le16(timeout);
	cp->sync_scan_windown = sys_cpu_to_le16(windown);
	cp->sync_scan_interval = sys_cpu_to_le16(interval);

	return bt_hci_cmd_send(BT_HCI_OP_RECEIVE_SYNC_TRAIN, buf);
}

int bt_csb_broadcase_by_acl(uint16_t handle, uint8_t *data, uint16_t len)
{
	int err;
	struct net_buf *buf;
	struct bt_hci_acl_hdr *hdr;

	if (len > CONFIG_BT_L2CAP_TX_MTU) {
		return -EIO;
	}

	buf = net_buf_alloc(&acl_tx_pool, K_NO_WAIT);
	if (!buf) {
		return -ENOMEM;
	}

	net_buf_reserve(buf, sizeof(*hdr));
	hdr = net_buf_push(buf, sizeof(*hdr));
	hdr->handle = sys_cpu_to_le16(bt_acl_handle_pack(handle, 0xc));
	hdr->len = len;
	net_buf_add_mem(buf, data, len);

	bt_buf_set_type(buf, BT_BUF_ACL_OUT);
	err = bt_send(buf);
	if (err) {
		BT_ERR("Unable to send to driver (err %d)", err);
		net_buf_unref(buf);
	}

	return err;
}

int bt_vs_set_apll_temp_comp(u8_t enable)
{
	struct net_buf *buf;
	struct bt_hci_cp_vs_set_apll_temp_comp *cp;

	buf = bt_hci_cmd_create(BT_HCI_OP_VS_SET_APLL_TEMP_COMP, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->enable = enable;

	return bt_hci_cmd_send(BT_HCI_OP_VS_SET_APLL_TEMP_COMP, buf);
}

int bt_vs_do_apll_temp_comp(void)
{
	return bt_hci_cmd_send(BT_HCI_OP_VS_DO_APLL_TEMP_COMP, NULL);
}

int bt_vs_read_bt_build_version(uint8_t* str_version, int len)
{
	int err;
	struct net_buf *rsp;
	struct bt_hci_rp_vs_read_bt_build_info *rp;

	if(len < sizeof(rp->str_data) + 4*7){
		BT_ERR("not enough space\n");
		return -EINVAL;
	}
	
	err = bt_hci_cmd_send_sync(BT_HCI_OP_VS_READ_BT_BUILD_INFO, NULL, &rsp);
	if (err) {
		return err;
	}

	rp = (void *)rsp->data;
	if(rp->status)
		return -EIO;
	uint16_t rf_version = sys_cpu_to_le16(rp->rf_version);
	uint16_t bb_version = sys_cpu_to_le16(rp->bb_version);
	uint16_t board_type = sys_cpu_to_le16(rp->board_type);
	uint16_t svn_version = sys_cpu_to_le16(rp->svn_version);

	snprintf(str_version, len, "0x%04x 0x%04x 0x%04x 0x%04x %s", rf_version,bb_version,board_type,
		svn_version,rp->str_data);

	net_buf_unref(rsp);
	return err;
}

#ifndef CONFIG_BT_CTRL_BQB
int bt_vs_write_bb_reg(uint32_t addr, uint32_t val)
{
	struct net_buf *buf;
	struct bt_hci_cp_vs_write_bb_reg *cp;

	buf = bt_hci_cmd_create(BT_HCI_OP_VS_WRITE_BB_REG, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->base_addr = sys_cpu_to_le32(addr);
	cp->value = sys_cpu_to_le32(val);

	return bt_hci_cmd_send_sync(BT_HCI_OP_VS_WRITE_BB_REG, buf, NULL);
}

int bt_vs_read_bb_reg(uint32_t addr, uint8_t size)
{
	struct net_buf *buf;
	struct bt_hci_cp_vs_read_bb_reg *cp;

	buf = bt_hci_cmd_create(BT_HCI_OP_VS_READ_BB_REG, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->base_addr = sys_cpu_to_le32(addr);
	cp->size = size;

	return bt_hci_cmd_send_sync(BT_HCI_OP_VS_READ_BB_REG, buf, NULL);
}

int bt_vs_write_rf_reg(uint16_t addr, uint16_t val)
{
	struct net_buf *buf;
	struct bt_hci_cp_vs_write_rf_reg *cp;

	buf = bt_hci_cmd_create(BT_HCI_OP_VS_WRITE_RF_REG, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->base_addr = sys_cpu_to_le16(addr);
	cp->value = sys_cpu_to_le16(val);

	return bt_hci_cmd_send_sync(BT_HCI_OP_VS_WRITE_RF_REG, buf, NULL);
}

int bt_vs_read_rf_reg(uint16_t addr, uint8_t size)
{
	struct net_buf *buf;
	struct bt_hci_cp_vs_read_rf_reg *cp;

	buf = bt_hci_cmd_create(BT_HCI_OP_VS_READ_RF_REG, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->base_addr = sys_cpu_to_le16(addr);
	cp->size = size;

	return bt_hci_cmd_send_sync(BT_HCI_OP_VS_READ_RF_REG, buf, NULL);
}
#endif

static void vs_read_bb_reg_report(struct net_buf *buf)
{
	struct bt_hci_rp_vs_read_bb_reg_report *evt = (void *)buf->data;

	BT_DBG("base addr 0x%08x size %u", evt->base_addr, evt->size);

	for (int i=0; i<evt->size; i++) {
        printk("0x%08x: 0x%08x\n", evt->base_addr + i*4, evt->result[i]);
    }
}

static void vs_read_rf_reg_report(struct net_buf *buf)
{
	struct bt_hci_rp_vs_read_rf_reg_report *evt = (void *)buf->data;

	BT_DBG("base addr 0x%04x size %u", evt->base_addr, evt->size);

	for (int i=0; i<evt->size; i++) {
        printk("0x%04x: 0x%04x\n", evt->base_addr + i, evt->result[i]);
    }
}

int bt_vs_adjust_link_time(struct bt_conn *conn, int16_t time)
{
	struct net_buf *buf;
	struct bt_hci_cp_acts_vs_adjust_link_time *cp;

	buf = bt_hci_cmd_create(BT_HCI_OP_ACTS_VS_ADJUST_LINK_TIME, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	net_buf_add_le16(buf, conn->handle);
	net_buf_add_le16(buf, time);

	return bt_hci_cmd_send(BT_HCI_OP_ACTS_VS_ADJUST_LINK_TIME, buf);
}

/* Wait to debug: Use send command sync, will block a while,  can acceptable?? */
int bt_vs_get_bt_clock(struct bt_conn *conn, uint32_t *bt_clk, uint16_t *bt_intraslot)
{
	struct net_buf *buf, *rsp;
	struct bt_hci_cp_acts_vs_read_bt_clock *cp;
	struct bt_hci_rp_acts_vs_read_bt_clock *rp;
	int err;

	buf = bt_hci_cmd_create(BT_HCI_OP_ACTS_VS_READ_BT_CLOCK, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	net_buf_add_le16(buf, conn->handle);
	err = bt_hci_cmd_send_sync(BT_HCI_OP_ACTS_VS_READ_BT_CLOCK, buf, &rsp);
	if (err) {
		return err;
	}

	rp = (void *)rsp->data;
	err = rp->status ? -EIO : 0;
	*bt_clk = sys_le32_to_cpu(rp->bt_clk);
	*bt_intraslot = sys_le16_to_cpu(rp->bt_intraslot);

	net_buf_unref(rsp);
	return err;
}

int bt_vs_set_tws_int_clock(struct bt_conn *conn, uint32_t bt_clk, uint16_t bt_intraslot)
{
	struct net_buf *buf;
	struct bt_hci_cp_acts_vs_set_tws_int_clock *cp;

	buf = bt_hci_cmd_create(BT_HCI_OP_ACTS_VS_SET_TWS_INT_CLOCK, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	net_buf_add_le16(buf, conn->handle);
	net_buf_add_le32(buf, bt_clk);
	net_buf_add_le32(buf, bt_intraslot);

	return bt_hci_cmd_send_sync(BT_HCI_OP_ACTS_VS_SET_TWS_INT_CLOCK, buf, NULL);
}

int bt_vs_enable_tws_int(uint8_t enable)
{
	struct net_buf *buf;
	struct bt_hci_cp_acts_vs_enable_tws_int *cp;

	buf = bt_hci_cmd_create(BT_HCI_OP_ACTS_VS_ENABLE_TWS_INT, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	net_buf_add_u8(buf, enable);

	return bt_hci_cmd_send_sync(BT_HCI_OP_ACTS_VS_ENABLE_TWS_INT, buf, NULL);
}

int bt_vs_enable_acl_flow_control(struct bt_conn *conn, uint8_t enable)
{
	struct net_buf *buf;
	struct bt_hci_cp_acts_vs_enable_acl_flow_ctrl *cp;

	buf = bt_hci_cmd_create(BT_HCI_OP_ACTS_VS_ENABLE_ACL_FLOW_CTRL, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->handle = sys_cpu_to_le16(conn->handle);
	cp->enable = enable;

	return bt_hci_cmd_send(BT_HCI_OP_ACTS_VS_ENABLE_ACL_FLOW_CTRL, buf);
}

int bt_vs_read_add_null_cnt(struct bt_conn *conn, uint16_t *cnt)
{
	int err;
	struct net_buf *buf, *rsp;
	struct bt_hci_rp_acts_vs_read_add_null_cnt *rp;

	buf = bt_hci_cmd_create(BT_HCI_OP_ACTS_VS_READ_ADD_NULL_CNT, 2);
	if (!buf) {
		return -ENOBUFS;
	}

	net_buf_add_le16(buf, conn->handle);

	err = bt_hci_cmd_send_sync(BT_HCI_OP_ACTS_VS_READ_ADD_NULL_CNT, buf, &rsp);
	if (err) {
		return err;
	}

	rp = (void *)rsp->data;
	err = rp->status ? -EIO : 0;
	*cnt = sys_le16_to_cpu(rp->null_cnt);

	net_buf_unref(rsp);
	return err;
}

int bt_vs_set_tws_sync_int_time(struct bt_conn *conn, uint8_t index, uint8_t mode,
	uint32_t bt_clk, uint16_t bt_slot)
{
	struct net_buf *buf;
	struct bt_hci_cp_acts_vs_set_tws_sync_int_time *cp;

	buf = bt_hci_cmd_create(BT_HCI_OP_ACTS_VS_SET_TWS_SYNC_INT_TIME, 2);
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->handle = sys_cpu_to_le16(conn->handle);
	cp->tws_index = index;
	cp->time_mode = mode;
	cp->rx_bt_clk = sys_cpu_to_le32(bt_clk);
	cp->rx_bt_slot = sys_cpu_to_le16(bt_slot);

	return bt_hci_cmd_send(BT_HCI_OP_ACTS_VS_SET_TWS_SYNC_INT_TIME, buf);
}

int bt_vs_set_tws_int_delay_play_time(struct bt_conn *conn, uint8_t index, uint8_t mode,
	uint8_t per_int, uint32_t bt_clk, uint16_t bt_slot)
{
	struct net_buf *buf;
	struct bt_hci_cp_acts_vs_set_tws_int_delay_play_time *cp;

	buf = bt_hci_cmd_create(BT_HCI_OP_ACTS_VS_SET_TWS_INT_DELAY_PLAY_TIME, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->handle = sys_cpu_to_le16(conn->handle);
	cp->tws_index = index;
	cp->time_mode = mode;
	cp->per_int = per_int;
	cp->delay_bt_clk = sys_cpu_to_le32(bt_clk);
	cp->delay_bt_slot = sys_cpu_to_le16(bt_slot);

	return bt_hci_cmd_send(BT_HCI_OP_ACTS_VS_SET_TWS_INT_DELAY_PLAY_TIME, buf);
}

int bt_vs_set_tws_int_clock_new(struct bt_conn *conn, uint8_t index, uint8_t mode,
	uint32_t clk, uint16_t slot, uint32_t per_clk, uint16_t per_slot)
{
	struct net_buf *buf;
	struct bt_hci_cp_acts_vs_set_tws_int_clock_new *cp;

	buf = bt_hci_cmd_create(BT_HCI_OP_ACTS_VS_SET_TWS_INT_CLOCK_NEW, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->handle = sys_cpu_to_le16(conn->handle);
	cp->tws_index = index;
	cp->time_mode = mode;
	cp->bt_clk = sys_cpu_to_le32(clk);
	cp->bt_intraslot = sys_cpu_to_le16(slot);
	cp->per_bt_clk = sys_cpu_to_le32(per_clk);
	cp->per_bt_intraslot = sys_cpu_to_le16(per_slot);

	return bt_hci_cmd_send_sync(BT_HCI_OP_ACTS_VS_SET_TWS_INT_CLOCK_NEW, buf, NULL);
}

int bt_vs_enable_tws_int_new(uint8_t enable, uint8_t index)
{
	struct net_buf *buf;
	struct bt_hci_cp_acts_vs_enable_tws_int_new *cp;

	buf = bt_hci_cmd_create(BT_HCI_OP_ACTS_VS_ENABLE_TWS_INT_NEW, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->enable = enable;
	cp->tws_index = index;

	return bt_hci_cmd_send_sync(BT_HCI_OP_ACTS_VS_ENABLE_TWS_INT_NEW, buf, NULL);
}

int bt_vs_read_bt_us_cnt(uint32_t *cnt)
{
	int err;
	struct net_buf *rsp;
	struct bt_hci_rp_acts_vs_read_bt_us_cnt *rp;

	err = bt_hci_cmd_send_sync(BT_HCI_OP_ACTS_VS_READ_BT_US_CNT, NULL, &rsp);
	if (err) {
		return err;
	}

	rp = (void *)rsp->data;
	err = rp->status ? -EIO : 0;
	*cnt = sys_le32_to_cpu(rp->cnt);

	net_buf_unref(rsp);
	return err;
}

int bt_vs_set_1st_sync_btclk(uint8_t send, struct bt_conn *conn, uint8_t enable)
{
	struct net_buf *buf;
	struct bt_hci_cp_vs_1st_sync_btclk *cp;
	uint16_t cmd = send ? BT_HCI_OP_VS_SEND_1ST_SYNC_BTCLK : BT_HCI_OP_VS_RX_1ST_SYNC_BTCLK;

	buf = bt_hci_cmd_create(cmd, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	net_buf_add_le16(buf, conn->handle);
	net_buf_add_u8(buf, enable);

	return bt_hci_cmd_send(cmd, buf);
}

static int bt_vs_command_with_handle(uint16_t cmd, uint16_t handle)
{
	struct net_buf *buf;

	buf = bt_hci_cmd_create(cmd, sizeof(uint16_t));
	if (!buf) {
		return -ENOBUFS;
	}

	net_buf_add_le16(buf, handle);
	return bt_hci_cmd_send(cmd, buf);
}

int bt_vs_read_1st_sync_btclk(struct bt_conn *conn, uint32_t *bt_clk)
{
	struct net_buf *buf, *rsp;
	struct bt_hci_cp_vs_read_1st_sync_btclk *cp;
	struct bt_hci_rp_vs_read_1st_sync_btclk *rp;
	int err;

	buf = bt_hci_cmd_create(BT_HCI_OP_VS_READ_1ST_SYNC_BTCLK, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	net_buf_add_le16(buf, conn->handle);
	err = bt_hci_cmd_send_sync(BT_HCI_OP_VS_READ_1ST_SYNC_BTCLK, buf, &rsp);
	if (err) {
		return err;
	}

	rp = (void *)rsp->data;
	err = rp->status ? -EIO : 0;
	if (sys_le16_to_cpu(rp->handle) != conn->handle) {
		err = -EIO;
	}
	*bt_clk = sys_le32_to_cpu(rp->btclk);

	net_buf_unref(rsp);
	return err;
}

int bt_vs_create_acl_snoop_link(struct bt_conn *conn)
{
	return bt_vs_command_with_handle(BT_HCI_OP_VS_CREATE_ACL_SNOOP_LINK, conn->handle);
}

int bt_vs_disconnect_acl_snoop_link(struct bt_conn *conn)
{
	return bt_vs_command_with_handle(BT_HCI_OP_VS_DISCONECT_ACL_SNOOP_LINK, conn->handle);
}

int bt_vs_create_sync_snoop_link(struct bt_conn *conn)
{
	return bt_vs_command_with_handle(BT_HCI_OP_VS_CREATE_SYNC_SNOOP_LINK, conn->handle);
}

int bt_vs_disconnect_sync_snoop_link(struct bt_conn *conn)
{
	return bt_vs_command_with_handle(BT_HCI_OP_VS_DISCONECT_SYNC_SNOOP_LINK, conn->handle);
}

int bt_vs_set_tws_link(struct bt_conn *conn)
{
	return bt_vs_command_with_handle(BT_HCI_OP_VS_SET_TWS_LINK, conn->handle);
}

int bt_vs_write_bt_addr(bt_addr_t *addr)
{
	struct net_buf *buf;

	buf = bt_hci_cmd_create(BT_HCI_OP_VS_WRITE_BT_ADDR, sizeof(bt_addr_t));
	if (!buf) {
		return -ENOBUFS;
	}

	net_buf_add_mem(buf, (const void *)addr, sizeof(bt_addr_t));

	return bt_hci_cmd_send(BT_HCI_OP_VS_WRITE_BT_ADDR, buf);
}

int bt_vs_switch_snoop_link(struct bt_conn *conn)
{
	return bt_vs_command_with_handle(BT_HCI_OP_VS_SWITCH_SNOOP_LINK, conn->handle);
}

int bt_vs_set_snoop_pkt_timeout(uint8_t timeout)
{
	struct net_buf *buf;

	buf = bt_hci_cmd_create(BT_HCI_OP_VS_SET_SNOOP_PKT_TIMEOUT, sizeof(uint8_t));
	if (!buf) {
		return -ENOBUFS;
	}

	net_buf_add_u8(buf, timeout);

	return bt_hci_cmd_send(BT_HCI_OP_VS_SET_SNOOP_PKT_TIMEOUT, buf);
}

int bt_vs_switch_le_link(struct bt_conn *conn)
{
	return bt_vs_command_with_handle(BT_HCI_OP_VS_SWITCH_LE_LINK, conn->handle);
}

int bt_vs_set_tws_visual_mode(struct tws_visual_mode_param *param)
{
	struct net_buf *buf;
	struct bt_hci_cp_vs_set_tws_visual_mode *cp;

	buf = bt_hci_cmd_create(BT_HCI_OP_VS_SET_TWS_VISUAL_MODE, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	memset((void *)cp, 0, sizeof(*cp));

	cp->enable = param->enable;
	cp->interval = sys_cpu_to_le16(param->interval);
	if (bt_dev.share_db_addr_valid) {
		bt_addr_copy(&cp->local_bdaddr, &bt_dev.share_db_addr);
	} else {
		bt_addr_copy(&cp->local_bdaddr, &bt_dev.read_bd_addr);
	}
	cp->data.connected_phone = param->connected_phone;
	cp->data.channel = param->channel;
	cp->data.match_mode = param->match_mode;
	cp->data.pair_keyword = param->pair_keyword;
	if (param->match_mode == TWS_MATCH_MODE_NAME) {
		cp->data.match_len = bt_read_br_name(cp->data.data, TWS_MATCH_NAME_MAX_LEN);
		if(cp->data.match_len > param->match_len){
			cp->data.match_len = param->match_len;
		}
	} else if (param->match_mode == TWS_MATCH_MODE_ID) {
		cp->data.match_len = 6;
		cp->data.data[0] = bt_device_id[DID_TYPE_ACTIONS_COMPANY_ID] & 0xFF;
		cp->data.data[1] = (bt_device_id[DID_TYPE_ACTIONS_COMPANY_ID] >> 8) & 0xFF;
		cp->data.data[2] = bt_device_id[DID_TYPE_PRODUCT_ID] & 0xFF;
		cp->data.data[3] = (bt_device_id[DID_TYPE_PRODUCT_ID] >> 8) & 0xFF;
		cp->data.data[4] = bt_device_id[DID_TYPE_VERSION] & 0xFF;
		cp->data.data[5] = (bt_device_id[DID_TYPE_VERSION] >> 8) & 0xFF;
	}

	cp->user_data_len = 5 + cp->data.match_len;
	return bt_hci_cmd_send(BT_HCI_OP_VS_SET_TWS_VISUAL_MODE, buf);
}

int bt_vs_set_tws_pair_mode(struct tws_pair_mode_param *param)
{
	struct net_buf *buf;
	struct bt_hci_cp_vs_set_tws_pair_mode *cp;

	buf = bt_hci_cmd_create(BT_HCI_OP_VS_SET_TWS_PAIR_MODE, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	memset((void *)cp, 0, sizeof(*cp));

	cp->enable = param->enable;
	cp->setup_link = param->setup_link;
	cp->interval = sys_cpu_to_le16(param->interval);
	cp->window = sys_cpu_to_le16(param->window);
	cp->rssi_thres = param->rssi_thres;
	if (bt_dev.share_db_addr_valid) {
		bt_addr_copy(&cp->local_bdaddr, &bt_dev.share_db_addr);
	} else {
		bt_addr_copy(&cp->local_bdaddr, &bt_dev.read_bd_addr);
	}
	cp->cod[0] = (uint8_t)bt_class_of_device;
	cp->cod[1] = (uint8_t)(bt_class_of_device >> 8);
	cp->cod[2] = (uint8_t)(bt_class_of_device >> 16);

	if (param->mask_addr) {
	#if 0 //使用外部传替的地址	
		if (bt_dev.share_db_addr_valid) {
			bt_addr_copy(&cp->data.local_bdaddr, &bt_dev.share_db_addr);
		} else {
			bt_addr_copy(&cp->data.local_bdaddr, &bt_dev.read_bd_addr);
		}
	#endif 
	    bt_addr_copy(&cp->data.local_bdaddr, &param->addr);
		memset(&cp->mask.local_bdaddr, 0xFF, sizeof(bt_addr_t));
	}

	if (param->mask_phone) {
		cp->data.connected_phone = param->connected_phone;
		cp->mask.connected_phone = 0xFF;
	}

	if (param->mask_channel) {
		cp->data.channel = param->channel;
		cp->mask.channel = 0xFF;
	}

	if (param->mask_mode) {
		cp->data.match_mode = param->match_mode;
		cp->mask.match_mode = 0xFF;

		if (param->match_mode == TWS_MATCH_MODE_NAME) {
			cp->data.match_len = bt_read_br_name(cp->data.data, TWS_MATCH_NAME_MAX_LEN);
			if(cp->data.match_len > param->match_len){
				cp->data.match_len = param->match_len;
			}
			cp->mask.match_len = 0xFF;
			memset(cp->mask.data, 0xFF, cp->data.match_len);
		} else if (param->match_mode == TWS_MATCH_MODE_ID) {
			cp->data.match_len = 6;
			cp->mask.match_len = 0xFF;

			cp->data.data[0] = bt_device_id[DID_TYPE_ACTIONS_COMPANY_ID] & 0xFF;
			cp->data.data[1] = (bt_device_id[DID_TYPE_ACTIONS_COMPANY_ID] >> 8)&0xFF;
			cp->data.data[2] = bt_device_id[DID_TYPE_PRODUCT_ID] & 0xFF;
			cp->data.data[3] = (bt_device_id[DID_TYPE_PRODUCT_ID] >> 8) & 0xFF;
			cp->data.data[4] = bt_device_id[DID_TYPE_VERSION] & 0xFF;
			cp->data.data[5] = (bt_device_id[DID_TYPE_VERSION] >> 8) & 0xFF;

			memset(cp->mask.data, 0xFF, cp->data.match_len);
		}
	}

	if (param->mask_keyword) {
		cp->data.pair_keyword = param->pair_keyword;
		cp->mask.pair_keyword = 0xFF;
	}

	cp->ref_data_len = 11 + cp->data.match_len;
	return bt_hci_cmd_send(BT_HCI_OP_VS_SET_TWS_PAIR_MODE, buf);
}

int bt_vs_set_snoop_link_active(struct bt_conn *conn)
{
	return bt_vs_command_with_handle(BT_HCI_OP_VS_SET_SNOOP_LINK_ACTIVE, conn->handle);
}

int bt_vs_get_snoop_link_active(void)
{
	return bt_hci_cmd_send(BT_HCI_OP_VS_GET_SNOOP_LINK_ACTIVE, NULL);
}

int bt_vs_write_manufacture_info(struct vs_manufacture_info *param)
{
	struct net_buf *buf;
	struct bt_hci_cp_vs_write_manufacture_info *cp;

	buf = bt_hci_cmd_create(BT_HCI_OP_VS_WRITE_MANUFACTURE_INFO, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->bt_version = param->bt_version;
	cp->bt_subversion = sys_cpu_to_le16(param->bt_subversion);
	cp->company_id = sys_cpu_to_le16(param->company_id);

	return bt_hci_cmd_send_sync(BT_HCI_OP_VS_WRITE_MANUFACTURE_INFO, buf, NULL);
}


int bt_vs_set_snoop_link_inactive(struct bt_conn *conn)
{
	return bt_vs_command_with_handle(BT_HCI_OP_VS_SET_SNOOP_LINK_INACTIVE, conn->handle);
}


int bt_vs_set_cis_link_background(uint8_t enable)
{
	struct net_buf *buf;
	struct bt_hci_cp_vs_set_cis_link_bg *cp;

	buf = bt_hci_cmd_create(BT_HCI_OP_VS_SET_CIS_LINK_BACKGROUND, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}
	cp = net_buf_add(buf, sizeof(*cp));
	cp->cis_bg_enable = enable;

	return bt_hci_cmd_send(BT_HCI_OP_VS_SET_CIS_LINK_BACKGROUND, buf);
}

//int bt_vs_set_apll_temp_comp(uint8_t enable)
//{
//	struct net_buf *buf;
//	struct bt_hci_cp_vs_set_apll_temp_comp *cp;
//
//	buf = bt_hci_cmd_create(BT_HCI_OP_VS_SET_APLL_TEMP_COMP, sizeof(*cp));
//	if (!buf) {
//		return -ENOBUFS;
//	}
//
//	cp = net_buf_add(buf, sizeof(*cp));
//	cp->enable = enable;
//
//	return bt_hci_cmd_send(BT_HCI_OP_VS_SET_APLL_TEMP_COMP, buf);
//}
//
//int bt_vs_do_apll_temp_comp(void)
//{
//	return bt_hci_cmd_send(BT_HCI_OP_VS_DO_APLL_TEMP_COMP, NULL);
//}

static struct bt_conn *bt_check_acl_snoop_link_connected(uint16_t handle, bt_addr_t *addr)
{
	struct bt_conn *conn;

	conn = bt_conn_lookup_addr_br(addr);
	if (conn) {
		/* As master connect to phone. */
		if (conn->handle != handle) {
			BT_ERR("snoop_link_connected handle error 0x%x 0x%x !", conn->handle, handle);
		}
		bt_conn_unref(conn);
		return conn;
	}

	conn = bt_conn_add_br(addr);
	if (!conn) {
		BT_ERR("Not more br conn!");
		return NULL;
	}

	conn->handle = handle;
	conn->type = BT_CONN_TYPE_BR_SNOOP;
	conn->role = BT_CONN_ROLE_SNOOP;
	conn->err = BT_HCI_ERR_SUCCESS;

	bt_conn_set_state(conn, BT_CONN_CONNECTED);
	bt_conn_unref(conn);

	return conn;
}

static void vs_evt_acl_snoop_link_complete(struct net_buf *buf)
{
	struct bt_hci_evt_vs_acl_snoop_link_complete *evt = (void *)buf->data;
	struct bt_cb_evt_vs_snoop_link_param param;
	uint16_t handle;

	handle = sys_le16_to_cpu(evt->handle);
	param.peer_bdaddr = &evt->peer_bdaddr;
	param.local_bdaddr = &evt->local_bdaddr;
	param.link_key = evt->link_key;

	param.conn = bt_check_acl_snoop_link_connected(handle, param.peer_bdaddr);
	bt_evt_callback(BT_CB_EVT_VS_ACL_SNOOP_LINK_COMPLETE, &param);
}

static struct bt_conn *bt_check_snoop_link_disconnect(uint16_t handle, uint8_t reason)
{
	struct bt_conn *conn;

	conn = bt_conn_lookup_handle(handle);
	if (!conn) {
		BT_ERR("Snoop link disconnect can't find handle 0x%x", handle);
		return NULL;
	}

	BT_INFO("Snoop link disconnect type 0x%x hdl 0x%x", conn->type, conn->handle);

	if (conn->type != BT_CONN_TYPE_BR_SNOOP && conn->type != BT_CONN_TYPE_SCO_SNOOP) {
		/* Normal link, just let hci disconnect event process disconnect */
		bt_conn_unref(conn);
		return conn;
	}

	conn->err = reason;
	bt_conn_set_state(conn, BT_CONN_DISCONNECT_COMPLETE);
	bt_conn_set_state(conn, BT_CONN_DISCONNECTED);
	//conn->handle = 0U;		/* Not set to 0, let for debug unref conn  */

	if (conn->type == BT_CONN_TYPE_SCO_SNOOP) {
		bt_sco_cleanup(conn);
	} else if (conn->type == BT_CONN_TYPE_BR_SNOOP) {
#ifdef CONFIG_BT_PROPERTY
		/*
		 * Store link key by adapter, we can clear link_key,
		 * so it can connect by another bt device.
		 */
		atomic_clear_bit(conn->flags, BT_CONN_BR_NOBOND);
		if (conn->br.link_key) {
			bt_keys_link_key_clear(conn->br.link_key);
			conn->br.link_key = NULL;
		}
#else
		/*
		 * If only for one connection session bond was set, clear keys
		 * database row for this connection.
		 */
		if (atomic_test_and_clear_bit(conn->flags, BT_CONN_BR_NOBOND)) {
			bt_keys_link_key_clear(conn->br.link_key);
		}
#endif
		bt_conn_unref(conn);
	}

	return conn;
}

static void vs_evt_snoop_link_disconnect(struct net_buf *buf)
{
	struct bt_hci_evt_vs_snoop_link_disconnect *evt = (void *)buf->data;
	struct bt_cb_evt_vs_snoop_link_param param;
	uint16_t handle;

	handle = sys_le16_to_cpu(evt->handle);
	param.com_param = evt->reason;

	param.conn = bt_check_snoop_link_disconnect(handle, evt->reason);
	bt_evt_callback(BT_CB_EVT_VS_SNOOP_LINK_DISCONNECT, &param);
}

static struct bt_conn *bt_check_sco_snoop_link_connected(uint16_t handle, bt_addr_t *addr, uint8_t link_type)
{
	struct bt_conn *sco_conn;

	sco_conn = bt_conn_lookup_addr_sco(addr);
	if (sco_conn) {
		/* As master connect to phone. */
		bt_conn_unref(sco_conn);
		return sco_conn;
	}

	sco_conn = bt_conn_add_sco(addr, (int)link_type);
	if (!sco_conn) {
		BT_ERR("Not more sco conn!");
		return NULL;
	}

	sco_conn->sco.acl = bt_conn_lookup_addr_br_snoop(addr);
	sco_conn->handle = handle;
	sco_conn->type = BT_CONN_TYPE_SCO_SNOOP;
	sco_conn->role = BT_CONN_ROLE_SNOOP;
	sco_conn->err = 0;
	bt_conn_set_state(sco_conn, BT_CONN_CONNECTED);
	bt_conn_unref(sco_conn);

	return sco_conn;
}

static void vs_evt_sync_snoop_link_complete(struct net_buf *buf)
{
	struct bt_hci_evt_vs_sync_snoop_link_complete *evt = (void *)buf->data;
	struct bt_cb_evt_vs_snoop_link_param param;
	uint16_t handle;

	handle = sys_le16_to_cpu(evt->handle);
	param.peer_bdaddr = &evt->peer_bdaddr;
	param.link_type = evt->link_type;
	param.packet_len = evt->packet_len;
	param.codec_id = (evt->codec_id == AIR_MODE_MSBC) ? BT_CODEC_ID_MSBC : BT_CODEC_ID_CVSD;

	param.conn = bt_check_sco_snoop_link_connected(handle, param.peer_bdaddr, param.link_type);
	bt_evt_callback(BT_CB_EVT_VS_SCO_SNOOP_LINK_COMPLETE, &param);
}

static void vs_evt_snoop_switch_complete(struct net_buf *buf)
{
	struct bt_hci_evt_vs_snoop_switch_complete *evt = (void *)buf->data;
	struct bt_cb_evt_vs_snoop_link_param param;
	struct bt_conn *sco_conn;

	if (evt->status) {
		BT_ERR("Switch error %d", evt->status);
		return;
	}

	if (evt->role == 0) {
		/* Swith from snoop link to normal link */
		param.conn = bt_conn_lookup_addr_br_snoop(&evt->peer_bdaddr);
		if (param.conn) {
			param.conn->type = BT_CONN_TYPE_BR;
			bt_conn_unref(param.conn);

			sco_conn = bt_conn_lookup_addr_sco_snoop(&evt->peer_bdaddr);
			if (sco_conn) {
				sco_conn->type = BT_CONN_TYPE_SCO;
				sco_conn->role = BT_CONN_ROLE_SLAVE;
				bt_conn_unref(sco_conn);
			}
		} else {
			BT_ERR("Without conn!");
			return;
		}
	} else if (evt->role == 1) {
		/* Swith from normal link to snoop link */
		param.conn = bt_conn_lookup_addr_br(&evt->peer_bdaddr);
		if (param.conn) {
			param.conn->type = BT_CONN_TYPE_BR_SNOOP;
			param.conn->role = BT_CONN_ROLE_SNOOP;
			bt_conn_unref(param.conn);

			sco_conn = bt_conn_lookup_addr_sco(&evt->peer_bdaddr);
			if (sco_conn) {
				sco_conn->type = BT_CONN_TYPE_SCO_SNOOP;
				sco_conn->role = BT_CONN_ROLE_SNOOP;
				/* Controler not send num_completed_packets after switch to snoop link, need clear pending_cnt. */
				sco_conn->sco.pending_cnt = 0;
				bt_conn_unref(sco_conn);
			}
		} else {
			BT_ERR("Without conn!");
			return;
		}
	} else {
		BT_ERR("Unknow role %d", evt->role);
		return;
	}

	bt_evt_callback(BT_CB_EVT_VS_SNOOP_SWITCH_COMPLETE, &param);
}

static void vs_evt_snoop_mode_change(struct net_buf *buf)
{
	struct bt_hci_evt_vs_snoop_mode_change *evt = (void *)buf->data;
	struct bt_cb_evt_vs_snoop_link_param param;

	param.conn = bt_conn_lookup_handle(sys_le16_to_cpu(evt->handle));
	if (param.conn) {
		param.com_param = evt->active;
		bt_evt_callback(BT_CB_EVT_VS_SNOOP_MODE_CHANGE, &param);
		bt_conn_unref(param.conn);
	} else {
		BT_ERR("Without conn!");
	}
}

static void vs_evt_ll_conn_switch_complete(struct net_buf *buf)
{

#if 0
	struct bt_hci_evt_vs_ll_conn_switch_complete *evt = (void *)buf->data;
	struct bt_hci_evt_le_enh_conn_complete enh;
	struct bt_conn *conn;

	BT_DBG("status 0x%02x role %u", evt->status, evt->role);

	conn = find_pending_connect(BT_HCI_ROLE_SLAVE, NULL);
	if (conn) {
		bt_conn_unref(conn);
	} else {
		/* Slave without advertise, need add conn */
		conn = bt_conn_add_le(bt_dev.adv_conn_id, BT_ADDR_LE_NONE);
		if (!conn) {
			BT_ERR("Without conn for le!");
			return;
		}
		bt_conn_set_state(conn, BT_CONN_CONNECT_ADV);
		bt_conn_unref(conn);		/* Add ref, bt_conn_set_state ref again, need unref. */
	}

	enh.status         = evt->status;
	enh.handle         = evt->handle;
	enh.role           = evt->role;
	enh.interval       = evt->interval;
	enh.latency        = evt->latency;
	enh.supv_timeout   = evt->supv_timeout;
	enh.clock_accuracy = evt->clock_accuracy;

	enh.peer_addr.type = evt->peer_address_type;
	bt_addr_copy(&enh.peer_addr.a, &evt->peer_address);

	if (IS_ENABLED(CONFIG_BT_PRIVACY)) {
		bt_addr_copy(&enh.local_rpa, &bt_dev.random_addr.a);
	} else {
		bt_addr_copy(&enh.local_rpa, BT_ADDR_ANY);
	}

	bt_addr_copy(&enh.peer_rpa, BT_ADDR_ANY);

	enh_conn_complete(&enh, true);

#endif
	
}

static void bt_check_ctrl_active_connect(struct bt_hci_evt_conn_complete *evt)
{
	struct bt_conn *conn;

	if (evt->link_type != BT_HCI_ACL) {
		return;
	}

	if (bt_addr_cmp(&bt_dev.ctrl_active_connect_addr, &evt->bdaddr)) {
		return;
	}

	/* Clear addr */
	bt_addr_copy(&bt_dev.ctrl_active_connect_addr, BT_ADDR_ANY);

	/* Simulate create br connect without send command */
	conn = bt_conn_lookup_addr_br(&evt->bdaddr);
	if (conn) {
		/* Have add conn, need do nothing. */
		bt_conn_unref(conn);
		return;
	}

	conn = bt_conn_add_br(&evt->bdaddr);
	if (!conn) {
		BT_ERR("Not more br conn!");
		return;
	}

	bt_conn_set_state(conn, BT_CONN_CONNECT);
	conn->role = BT_CONN_ROLE_MASTER;
	bt_conn_unref(conn);
}

static void vs_evt_tws_match_report(struct net_buf *buf)
{
	struct bt_hci_evt_vs_tws_match_report *evt = (void *)buf->data;

	BT_INFO("tws match %d, %s", evt->rssi, bt_addr_str((const bt_addr_t *)evt->data));

	/* Tws pair, controlre active do br connect, record addr,
	 * clear this addr when receive connect request or connect complete.
	 * when receive connect complete without connect request,
	 * need simulate create br connect without send command.
	 */
	bt_addr_copy(&bt_dev.ctrl_active_connect_addr, (const bt_addr_t *)evt->data);
	bt_evt_callback(BT_CB_EVT_VS_TWS_MATCH_REPORT, evt->data);
}

static void vs_evt_snoop_active_link(struct net_buf *buf)
{
	struct bt_hci_evt_vs_snoop_active_link *evt = (void *)buf->data;
	struct bt_cb_evt_vs_snoop_link_param param;

	param.conn = bt_conn_lookup_handle(sys_le16_to_cpu(evt->handle));
	if (param.conn) {
		bt_evt_callback(BT_CB_EVT_VS_SNOOP_ACTIVE_LINK, &param);
		bt_conn_unref(param.conn);
	} else {
		BT_ERR("Without conn!");
	}
}

static void vs_evt_sync_1st_pkt_chg(struct net_buf *buf)
{
	struct bt_hci_evt_vs_sync_1st_pkt_chg *evt = (void *)buf->data;
	struct bt_cb_evt_vs_snoop_link_param param;

	param.conn = bt_conn_lookup_handle(sys_le16_to_cpu(evt->handle));
	if (param.conn) {
		param.com_param = sys_le32_to_cpu(evt->bt_clk);
		bt_evt_callback(BT_CB_EVT_VS_SYNC_1ST_PKT_CHG, &param);
		bt_conn_unref(param.conn);
	} else {
		BT_ERR("Without conn!");
	}
}

static void vs_evt_tws_local_exit_sniff(struct net_buf *buf)
{
	bt_evt_callback(BT_CB_EVT_VS_TWS_SNOOP_EXIT_SNIFF, NULL);
}

static void vs_evt_pa_send_complete(struct net_buf *buf)
{
    struct bt_hci_pa_send_complete *evt = (void *)buf->data;
	struct bt_cb_evt_vs_send_per_data param;

	param.adv_handle = evt->adv_handle;
	param.adv_len = evt->adv_len;
	param.adv_data = evt->adv_data;

	bt_evt_callback(BT_CB_EVT_VS_PA_SEND_COMPLETE, &param);
}


int bt_vs_le_set_master_latency(struct bt_conn *conn, uint16_t latency,
				uint8_t retry)
{
	struct bt_hci_cp_acts_vs_le_set_master_latency *cp;
	struct net_buf *buf;
	int err;

	buf = bt_hci_cmd_create(BT_HCI_OP_ACTS_VS_LE_SET_MASTER_LATENCY, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->handle = sys_cpu_to_le16(conn->handle);
	cp->latency = sys_cpu_to_le16(latency);
	cp->retry = retry;

	err = bt_hci_cmd_send_sync(BT_HCI_OP_ACTS_VS_LE_SET_MASTER_LATENCY, buf, NULL);
	if (err) {
		return err;
	}

	return 0;
}

void bt_read_ble_mac(bt_addr_le_t *addr)
{
	bt_addr_le_t *le_addr_tmp = &bt_dev.id_addr[BT_ID_DEFAULT];

#if defined(CONFIG_BT_PRIVACY)
    if (atomic_test_bit(bt_dev.flags, BT_DEV_RPA_VALID)) {
	    le_addr_tmp = &bt_dev.random_addr;
	}	
#endif
	bt_addr_le_copy(addr, le_addr_tmp);
}

void bt_update_br_name(void)
{
   	struct net_buf *buf;
	struct bt_hci_write_local_name *name_cp;
	struct bt_hci_cp_write_eir *eir;
	uint8_t eir_len;
    int err;

   if (bt_property_get_br_name(bt_dev.br_name, sizeof(bt_dev.br_name)) <= 0) {
       BT_ERR("get br name fail:");
	   return;
   }
   	/* Set local name */
	buf = bt_hci_cmd_create(BT_HCI_OP_WRITE_LOCAL_NAME, sizeof(*name_cp));
	if (!buf) {
		return;
	}

	name_cp = net_buf_add(buf, sizeof(*name_cp));
	memset((char *)name_cp->local_name, 0, sizeof(name_cp->local_name));
	bt_read_br_name(name_cp->local_name, sizeof(name_cp->local_name));
	err = bt_hci_cmd_send_sync(BT_HCI_OP_WRITE_LOCAL_NAME, buf, NULL);
	if (err) {
		return;
	}

	/* Write extended inquiry response */
	buf = bt_hci_cmd_create(BT_HCI_OP_WRITE_EXTENDED_INQUIRY_RSP, sizeof(*eir));
	if (!buf) {
		return;
	}

	eir = net_buf_add(buf, sizeof(*eir));
	eir->fcc = 0;	/* FEC not required */
	memset(eir->eirdata, 0, 240);

	/* Device name */
	eir->eirdata[1] = BT_DATA_NAME_COMPLETE;
	bt_read_br_name(&eir->eirdata[2], (240 - 2));
	eir->eirdata[0] = 1 + strlen(&eir->eirdata[2]);
	eir_len = 1 + eir->eirdata[0];

	/* Device ID */
	eir->eirdata[eir_len] = 9;
	eir->eirdata[eir_len + 1] = BT_DATA_DEVICE_ID;
	memcpy(&eir->eirdata[eir_len + 2], (void *)bt_device_id, sizeof(bt_device_id));

	err = bt_hci_cmd_send_sync(BT_HCI_OP_WRITE_EXTENDED_INQUIRY_RSP, buf, NULL);   
}

int bt_vs_read_link_info(struct bt_conn *conn, complete_event_cb cb)
{
	struct bt_hci_cp_get_link_info *cp;
	struct net_buf *buf;

	if (!conn) {
		return -ENOTCONN;
	}

	if (conn->state != BT_CONN_CONNECTED) {
		return -ENOTCONN;
	}

	buf = bt_hci_cmd_create(BT_HCI_OP_VS_GET_LINK_INFO, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->conn_handle = sys_cpu_to_le16(conn->handle);

	cmd(buf)->event_cb = cb;

	return bt_hci_cmd_send(BT_HCI_OP_VS_GET_LINK_INFO, buf);
}

int bt_vs_link_channel_set(struct bt_conn *conn, uint8_t *map)
{
	struct bt_hci_cp_set_link_info *cp;
	struct net_buf *buf;

	if (conn && conn->state != BT_CONN_CONNECTED) {
		return -ENOTCONN;
	}

	buf = bt_hci_cmd_create(BT_HCI_OP_VS_SET_LINK_CHANNEL_MAP, sizeof(*cp));
	if (!buf) {
		return -ENOBUFS;
	}

	cp = net_buf_add(buf, sizeof(*cp));
	cp->conn_handle = 0;
	if (conn) {
		cp->conn_handle = sys_cpu_to_le16(conn->handle);
	}
	if (map) {
		cp->enable = 1;
		memcpy(cp->afh_channel_map, map, sizeof(cp->afh_channel_map));
	} else {
		cp->enable = 0;
		memset(cp->afh_channel_map, 0, sizeof(cp->afh_channel_map));
	}

	return bt_hci_cmd_send(BT_HCI_OP_VS_SET_LINK_CHANNEL_MAP, buf);
}

bool bt_le_check_remote_cis_feature(struct bt_conn *conn)
{
    if (conn && atomic_test_bit(conn->flags, BT_CONN_AUTO_FEATURE_EXCH))
    {
        if(BT_FEAT_LE_CIS(conn->le.features))
        {
            return true;
        }
    }
    return false;
}

bool bt_le_check_remote_bis_feature(struct bt_conn *conn)
{
    if (conn && atomic_test_bit(conn->flags, BT_CONN_AUTO_FEATURE_EXCH))
    {
        if(BT_FEAT_LE_BIS(conn->le.features))
        {
            return true;
        }
    }
    return false;
}
/* Actions add end */

