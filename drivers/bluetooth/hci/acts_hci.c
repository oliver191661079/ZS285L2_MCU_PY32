/* h4.c - H:4 UART based Bluetooth driver */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <stddef.h>
#include <ctype.h>

#include <zephyr.h>
#include <arch/cpu.h>

#include <init.h>
#include <uart.h>
#include <misc/util.h>
#include <misc/byteorder.h>
#include <string.h>

#include <acts_bluetooth/buf.h>
#include <acts_bluetooth/bluetooth.h>
#include <acts_bluetooth/hci.h>
#include <bluetooth/hci_driver.h>

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_DEBUG_HCI_DRIVER)
#include "common/log.h"
#include "../util.h"
#include <property_manager.h>
#include <btdrv_api.h>

#define HCIT_COMMAND             1
#define HCIT_ACLDATA             2
#define HCIT_SCODATA             3
#define HCIT_EVENT               4
#define HCIT_ISODATA             5

struct bt_hci_l2cap_hdr {
	uint16_t len;
	uint16_t cid;
} __packed;

static int btdrv_cal_acl_packet_need_len(void *buf, int len)
{
	struct bt_hci_acl_hdr *acl_hdr;
	struct bt_hci_l2cap_hdr *l2cap_hdr;
	uint8_t *pData = buf;
	uint16_t handle, need_len;
	uint8_t flags;

	acl_hdr = (void *)pData;
	handle = sys_le16_to_cpu(acl_hdr->handle);
	flags = bt_acl_flags(handle);

	if (flags == BT_ACL_START) {
		l2cap_hdr = (void *)(pData + sizeof(struct bt_hci_acl_hdr));
		need_len = sys_le16_to_cpu(l2cap_hdr->len);
		need_len = need_len + sizeof(struct bt_hci_acl_hdr) + sizeof(struct bt_hci_l2cap_hdr);
		return (need_len > len)? need_len : len;
	} else {
		return len;
	}
}

static int btdrv_cal_iso_packet_need_len(void *buf, int len)
{
	struct bt_hci_iso_hdr *hdr;
	struct bt_hci_iso_data_hdr *data_hdr;
	uint8_t flags;
	uint8_t *data = buf;
	uint16_t need_len;

	hdr = (struct bt_hci_iso_hdr *)data;
	flags = bt_iso_flags(sys_le16_to_cpu(hdr->handle));

	if (bt_iso_flags_pb(flags) == BT_ISO_START) {
		data += sizeof(struct bt_hci_iso_hdr);
		need_len = sizeof(struct bt_hci_iso_hdr);

		if (bt_iso_flags_ts(flags)) {
			/* sizeof timestamp is 4-byte */
			data += 4;
			need_len += 4;
		}

		data_hdr = (struct bt_hci_iso_data_hdr *)data;
		need_len += sizeof(struct bt_hci_iso_data_hdr);
		need_len += bt_iso_pkt_len(sys_le16_to_cpu(data_hdr->slen));

		return need_len;
	}

	return len;
}

static void btdrv_rx_cb(void *buf, int len, uint8_t type)
{
	enum bt_buf_type buf_type = BT_BUF_EVT;
	uint8_t evt_code = 0;
	int need_len;

	struct net_buf *rx_buf = NULL;

	BT_DBG("buf %p, len %d, type %d", buf, len, type);

	switch (type) {
	case HCIT_EVENT:
		buf_type = BT_BUF_EVT;

		evt_code = *(uint8_t *)buf;
		if (evt_code == BT_HCI_EVT_CMD_COMPLETE ||
		    evt_code == BT_HCI_EVT_CMD_STATUS)
			rx_buf = bt_buf_get_cmd_complete(K_NO_WAIT);
		else
			rx_buf = bt_buf_get_rx_len(buf_type, K_NO_WAIT, len);
		break;

	case HCIT_ACLDATA:
		buf_type = BT_BUF_ACL_IN;
		need_len = btdrv_cal_acl_packet_need_len(buf, len);
		rx_buf = bt_buf_get_rx_len(buf_type, K_NO_WAIT, need_len);
		break;

	case HCIT_SCODATA:
		buf_type = BT_BUF_SCO_IN;
		rx_buf = bt_buf_get_rx_len(buf_type, K_NO_WAIT, len);
		break;

	case HCIT_ISODATA:
		buf_type = BT_BUF_ISO_IN;
		need_len = btdrv_cal_iso_packet_need_len(buf, len);
		rx_buf = bt_buf_get_rx_len(buf_type, K_NO_WAIT, need_len);
		break;

	default:
		BT_ERR("invalid type(%d)", type);
		break;
	}

	if (!rx_buf) {
		BT_ERR("Failed to allocate %d %d, Discarding(TODO, info, etc.)\n",
			buf_type, len);
		return;
	}
	bt_buf_set_type(rx_buf, buf_type);

	net_buf_add_mem(rx_buf, buf, len);

	//if (type == HCIT_EVENT && bt_hci_evt_is_prio(evt_code))
	//	bt_recv_prio(rx_buf);
	//else
	bt_recv(rx_buf);
}

static int acts_hci_send(struct net_buf *buf)
{
	int ret = 0;

	BT_DBG("buf %p type %u len %u", buf, bt_buf_get_type(buf), buf->len);

	if (buf == NULL)
		return -EINVAL;

	switch (bt_buf_get_type(buf)) {
	case BT_BUF_CMD:
		ret = btdrv_send(buf->data, buf->len, HCIT_COMMAND);
		break;

	case BT_BUF_EVT:
		ret = btdrv_send(buf->data, buf->len, HCIT_EVENT);
		break;

	case BT_BUF_ACL_OUT:
	case BT_BUF_ACL_IN:
		ret = btdrv_send(buf->data, buf->len, HCIT_ACLDATA);
		break;

	case BT_BUF_SCO_OUT:
		ret = btdrv_send(buf->data, buf->len, HCIT_SCODATA);
		break;

	case BT_BUF_ISO_OUT:
		ret = btdrv_send(buf->data, buf->len, HCIT_ISODATA);
		break;

	default:
		BT_ERR("error packet!!");
		break;
	}

	BT_DBG("btdrv_send ret %d", ret);
	if (ret != buf->len)
		ret = -EFAULT;
	else
		ret = 0;

	net_buf_unref(buf);

	return ret;
}

static int acts_hci_open(void)
{
	BT_DBG("btdrv_init");

	return btdrv_init(btdrv_rx_cb);
}

static int acts_hci_close(void)
{
	BT_DBG("btdrv_deinit");
	btdrv_exit();
	return 0;
}

static struct bt_hci_driver drv = {
	.name		= "H:ACTS",
	.bus		= BT_HCI_DRIVER_BUS_VIRTUAL,
	.open		= acts_hci_open,
	.close		= acts_hci_close,
	.send		= acts_hci_send,
	.quirks     = BT_QUIRK_NO_AUTO_DLE,
};

static int _bt_acts_init(struct device *unused)
{
	ARG_UNUSED(unused);

	bt_hci_driver_register(&drv);

	return 0;
}

SYS_INIT(_bt_acts_init, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);
