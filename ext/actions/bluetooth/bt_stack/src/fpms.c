/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <misc/printk.h>
#include <misc/byteorder.h>
#include <zephyr.h>
#include <init.h>

#include <acts_net/buf.h>
#include <acts_bluetooth/l2cap.h>
#include <acts_bluetooth/services/fpms.h>
#include <common_internal.h>
#include <hci_core.h>
#include <conn_internal.h>
#include <l2cap_internal.h>

#define BT_DBG_ENABLED 1
#define LOG_MODULE_NAME bt_fpms
#include "../common/log.h"

#define BT_GATT_FPMS_L2CAP_PSM	0x0080

/* Maximum size of TX buffer and its payload. */
#define FPMS_MTU		256

struct bt_fpms_l2cap {
	struct bt_l2cap_le_chan fpms_chan;
};

#define FPMS_CHAN(_ch) CONTAINER_OF(_ch, struct bt_fpms_l2cap, fpms_chan.chan)

#ifdef CONFIG_BT_LEA_PTS_TEST
struct bt_fpms_l2cap fpms_connection[CONFIG_BT_MAX_BR_CONN];
#else
//struct bt_fpms_l2cap fpms_connection[CONFIG_BT_MAX_BR_CONN] __IN_BT_SECTION;
struct bt_fpms_l2cap fpms_connection[CONFIG_BT_MAX_BR_CONN]__IN_BT_BSS_SECTION;
#endif

/* fpms app register call back handler */
//static struct bt_fpms_app_cb *reg_fpms_app_cb __IN_BT_SECTION;
static struct bt_fpms_app_cb *reg_fpms_app_cb __IN_BT_BSS_SECTION;


static struct bt_fpms_l2cap *fpms_get_new_connection(struct bt_conn *conn)
{
	uint8_t i;

	if (!conn) {
		BT_ERR("Invalid Input (err: %d)", -EINVAL);
		return NULL;
	}

	for (i = 0; i < CONFIG_BT_MAX_BR_CONN; i++) {
		if (!fpms_connection[i].fpms_chan.chan.conn) {
			memset(&fpms_connection[i], 0, sizeof(struct bt_fpms_l2cap));
			return &fpms_connection[i];
		}
	}

	BT_DBG("More connection cannot be supported");
	return NULL;
}

static struct bt_fpms_l2cap *fpms_lookup_by_conn(struct bt_conn *conn)
{
	uint8_t i;

	if (!conn) {
		return NULL;
	}

	for (i = 0; i < CONFIG_BT_MAX_BR_CONN; i++) {
		if (fpms_connection[i].fpms_chan.chan.conn == conn) {
			return &fpms_connection[i];
		}
	}

	return NULL;
}

static int fpms_l2cap_send(struct bt_fpms_l2cap *fpms_l2cap,uint8_t *data, uint32_t len)
{
	int ret;
	u16_t tx_mtu = 0;
	struct net_buf *buf;

	tx_mtu = fpms_l2cap->fpms_chan.tx.mtu;
	if (len > tx_mtu) {
		BT_WARN("MTU exceeded, max %u, wanted %zu",
			tx_mtu, len);
		return -EINVAL;
	}
	buf = bt_l2cap_create_pdu(NULL, 0);
	if (!buf) {
		BT_ERR("Unable to allocate buffer");
		return -ENOMEM;
	}
	net_buf_add_mem(buf, data, len);

	ret = bt_l2cap_chan_send(&fpms_l2cap->fpms_chan.chan, buf);
	if (ret < 0) {
		BT_ERR("Unable to send data over CoC: %d", ret);
		net_buf_unref(buf);

		return -ENOEXEC;
	}

	return 0;
}

static void fpms_l2cap_connected(struct bt_l2cap_chan *chan)
{
	printk("Channel %p connected\n", chan);
	struct bt_fpms_l2cap *fpms_l2cap = FPMS_CHAN(chan);
	if (reg_fpms_app_cb && reg_fpms_app_cb->connected){
		reg_fpms_app_cb->connected(fpms_l2cap->fpms_chan.chan.conn);
    }
}

static void fpms_l2cap_disconnected(struct bt_l2cap_chan *chan)
{
	BT_DBG("Channel %p disconnected", chan);
	struct bt_fpms_l2cap *fpms_l2cap = FPMS_CHAN(chan);
	if (reg_fpms_app_cb && reg_fpms_app_cb->disconnected){
		reg_fpms_app_cb->disconnected(fpms_l2cap->fpms_chan.chan.conn);
    }
}

static int fpms_l2cap_recv(struct bt_l2cap_chan *chan, struct net_buf *buf)
{
	struct bt_fpms_l2cap *fpms_l2cap = FPMS_CHAN(chan);

	if (reg_fpms_app_cb && reg_fpms_app_cb->recv){
		reg_fpms_app_cb->recv(fpms_l2cap->fpms_chan.chan.conn, buf->data, buf->len);
    }

	return 0;
}

static const struct bt_l2cap_chan_ops fpms_l2cap_ops = {
	.connected	= fpms_l2cap_connected,
	.disconnected	= fpms_l2cap_disconnected,
	.recv = fpms_l2cap_recv,
};

static int fpms_l2cap_accept(struct bt_conn *conn, struct bt_l2cap_chan **chan)
{
	struct bt_fpms_l2cap *fpms_l2cap;

	BT_DBG("Incoming conn %p", conn);

	if (fpms_lookup_by_conn(conn)) {
		return -EALREADY;
	}

	fpms_l2cap = fpms_get_new_connection(conn);
	if (!fpms_l2cap) {
		return -ENOMEM;
	}

	printk("session: %p\n", fpms_l2cap);
	fpms_l2cap->fpms_chan.rx.mtu = FPMS_MTU;
	fpms_l2cap->fpms_chan.chan.ops = &fpms_l2cap_ops;

	*chan = &fpms_l2cap->fpms_chan.chan;

	return 0;

}

static struct bt_l2cap_server fpms_l2cap_server = {
	.psm = BT_GATT_FPMS_L2CAP_PSM,
	.accept	= fpms_l2cap_accept,
};

int bt_fpms_init(void)
{
	int err;

	err = bt_l2cap_server_register(&fpms_l2cap_server);
	if (err) {
		BT_ERR("Unable to register FPMS PSM");
		return err;
	}

	BT_DBG("Initialized FPMS L2CAP");

	return 0;
}

int bt_fpms_register_cb(struct bt_fpms_app_cb *cb)
{
	if (reg_fpms_app_cb) {
		BT_WARN("Already register app_cb");
	}

	reg_fpms_app_cb = cb;
	return 0;
}

int bt_fpms_send(struct bt_conn *conn,uint8_t *data, uint32_t len)
{
	int err;

	struct bt_fpms_l2cap *fpms_l2cap;

	if (len > FPMS_MTU) {
		return -ENOMEM;
	}

	fpms_l2cap = fpms_lookup_by_conn(conn);
	if (!fpms_l2cap) {
		return -EINVAL;
	}

	err = fpms_l2cap_send(fpms_l2cap,data,len);
	if (err) {
		BT_ERR("Unable to send data: %d", err);

		return err;
	}

	return 0;
}

