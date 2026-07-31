/*
 * Copyright (c) 2026 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file system_app_ble_remote_cmd.c
 *
 * @brief BLE GATT 业务控制：Write 收命令 + Notify 回状态（与 SPP D2 协议一致）。
 *
 * 拓扑：手机(Central/GATT Client) --BLE--> 音箱(Peripheral/GATT Server)
 *
 * GATT 定义（16-bit UUID，Bluetooth Base）：
 *   Service  0xFFE0  远程控制服务
 *   Char RX  0xFFE1  Write / Write Without Response，载荷 D2 + 子命令
 *   Char TX  0xFFE2  Notify + CCC，可选 ACK：D2 80 | 子命令 | 状态
 *
 * 子命令表与 bt_manager_remote_cmd.c / SPP 相同（0x00~0x0A，0x0A 进 ADFU）。
 *
 * 初始化：BT_READY -> system_app_ble_remote_cmd_init()
 */

#include <string.h>
#include <zephyr.h>
#include <misc/printk.h>

#include <acts_bluetooth/gatt.h>
#include <acts_bluetooth/uuid.h>
#include <acts_bluetooth/gap.h>
#include <acts_bluetooth/host_interface.h>

#include <bt_manager.h>
#include <bt_manager_ble.h>

#include "system_app.h"

#ifdef CONFIG_BT_BLE

#ifndef CONFIG_SYSTEM_APP_BLE_REMOTE_CMD
#define CONFIG_SYSTEM_APP_BLE_REMOTE_CMD 1
#endif

#if CONFIG_SYSTEM_APP_BLE_REMOTE_CMD

#define BLE_REMOTE_CMD_SVC_UUID   BT_UUID_DECLARE_16(0xFFE0)
#define BLE_REMOTE_CMD_RX_UUID    BT_UUID_DECLARE_16(0xFFE1)
#define BLE_REMOTE_CMD_TX_UUID    BT_UUID_DECLARE_16(0xFFE2)

#define BLE_REMOTE_CMD_ACK_MARKER 0x80U

/* 与 SPP 相同的 A5 5A 帧头（可选） */
#define BLE_CMD_SYNC0           0xA5U
#define BLE_CMD_SYNC1           0x5AU
#define BLE_CMD_FRAME_HDR       4U

/* attrs[] 下标：与 bt_manager_test_sample ble_speed_attrs 布局一致 */
#define BLE_REMOTE_CMD_ATTR_NOTIFY_CHRC  3
#define BLE_REMOTE_CMD_ATTR_NOTIFY_VAL   4

static struct bt_conn *ble_cmd_conn;
static uint8_t ble_cmd_notify_on;
static uint8_t ble_cmd_registered;

static uint8_t ble_cmd_ack_status(int ret)
{
	if (ret == 0) {
		return 0U;
	}
	if (ret < 0) {
		return (uint8_t)(-ret);
	}
	return 0xFFU;
}

static struct bt_gatt_attr ble_remote_cmd_attrs[];

#if defined(CONFIG_SYSTEM_APP_BLE_CMD_ACK)
static void ble_cmd_send_ack(uint8_t subcmd, int ret)
{
	uint8_t ack[4];

	if (!ble_cmd_conn || !ble_cmd_notify_on) {
		printk("[ble_cmd] ACK skip: conn=%p notify=%u\n",
		       ble_cmd_conn, (unsigned)ble_cmd_notify_on);
		return;
	}

	ack[0] = 0xD2u;
	ack[1] = BLE_REMOTE_CMD_ACK_MARKER;
	ack[2] = subcmd;
	ack[3] = ble_cmd_ack_status(ret);

	if (bt_manager_ble_send_data(ble_cmd_conn,
				     &ble_remote_cmd_attrs[BLE_REMOTE_CMD_ATTR_NOTIFY_CHRC],
				     &ble_remote_cmd_attrs[BLE_REMOTE_CMD_ATTR_NOTIFY_VAL],
				     ack, sizeof(ack)) >= 0) {
		printk("[ble_cmd] TX ACK D2 80 %02x %02x ret=%d\n",
		       (unsigned)ack[2], (unsigned)ack[3], ret);
	}
}
#endif

/**
 * 解析 Write 载荷：裸 D2 xx，或单包 A5 5A | LEN | D2 xx（与 SPP 协议一致）。
 */
static int ble_remote_cmd_parse_write(const uint8_t *data, uint16_t len,
				      const uint8_t **payload, uint16_t *plen,
				      int *framed)
{
	if (!data || !payload || !plen || !framed) {
		return -EINVAL;
	}

	*framed = 0;

	if (len >= BLE_CMD_FRAME_HDR &&
	    data[0] == BLE_CMD_SYNC0 && data[1] == BLE_CMD_SYNC1) {
		uint16_t body_len = (uint16_t)(((uint16_t)data[2] << 8) |
					       data[3]);

		if (body_len < 2U || (uint16_t)(BLE_CMD_FRAME_HDR + body_len) > len) {
			printk("[ble_cmd] bad A5 5A frame len=%u body=%u\n",
			       (unsigned)len, (unsigned)body_len);
			return -EINVAL;
		}
		*payload = data + BLE_CMD_FRAME_HDR;
		*plen = body_len;
		*framed = 1;
		printk("[ble_cmd] framed cmd plen=%u sub=0x%02x\n",
		       (unsigned)body_len, (unsigned)data[BLE_CMD_FRAME_HDR + 1]);
		return 0;
	}

	if (len >= 2U && data[0] == 0xD2u) {
		*payload = data;
		*plen = len;
		return 0;
	}

	printk("[ble_cmd] write invalid: need D2 xx or A5 5A frame, len=%u\n",
	       (unsigned)len);
	return -EINVAL;
}

static ssize_t ble_remote_cmd_write_cb(struct bt_conn *conn,
				       const struct bt_gatt_attr *attr,
				       const void *buf, uint16_t len,
				       uint16_t offset, uint8_t flags)
{
	const uint8_t *data = buf;
	const uint8_t *payload;
	uint16_t plen;
	int framed;
	int ret;

	ARG_UNUSED(conn);
	ARG_UNUSED(attr);
	ARG_UNUSED(flags);

	if (offset != 0U) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	if (!data || len < 2U) {
		printk("[ble_cmd] write too short len=%u\n", (unsigned)len);
		return len;
	}

	if (ble_remote_cmd_parse_write(data, len, &payload, &plen, &framed) != 0) {
		return len;
	}

	if (!framed) {
		printk("[ble_cmd] RX len=%u D2 sub=0x%02x\n", (unsigned)plen,
		       (unsigned)payload[1]);
	}

	ret = bt_manager_remote_cmd_dispatch(payload, plen, "ble");

#if defined(CONFIG_SYSTEM_APP_BLE_CMD_ACK)
	if (plen >= 2U && payload[0] == 0xD2u) {
		ble_cmd_send_ack(payload[1], ret);
	}
#endif

	return len;
}

static void ble_remote_cmd_ccc_cfg_changed(const struct bt_gatt_attr *attr,
					   uint16_t value)
{
	ARG_UNUSED(attr);

	ble_cmd_notify_on = (value == BT_GATT_CCC_NOTIFY) ? 1U : 0U;
	printk("[ble_cmd] notify %s\n", ble_cmd_notify_on ? "on" : "off");
}

static struct bt_gatt_attr ble_remote_cmd_attrs[] = {
	BT_GATT_PRIMARY_SERVICE(BLE_REMOTE_CMD_SVC_UUID),

	BT_GATT_CHARACTERISTIC(BLE_REMOTE_CMD_RX_UUID,
			       BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
			       BT_GATT_PERM_WRITE,
			       NULL, ble_remote_cmd_write_cb, NULL),

	BT_GATT_CHARACTERISTIC(BLE_REMOTE_CMD_TX_UUID,
			       BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
			       NULL, NULL, NULL),
	BT_GATT_CCC(ble_remote_cmd_ccc_cfg_changed,
		    BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
};

static struct ble_reg_manager ble_remote_cmd_mgr;

static void ble_remote_cmd_link_cb(struct bt_conn *conn, uint8_t conn_type,
				   uint8_t *mac, uint8_t connected)
{
	ARG_UNUSED(mac);

	if (conn_type != BT_CONN_TYPE_LE) {
		return;
	}

	if (connected) {
		if (ble_cmd_conn) {
			hostif_bt_conn_unref(ble_cmd_conn);
		}
		ble_cmd_conn = hostif_bt_conn_ref(conn);
		printk("[ble_cmd] connected conn=%p\n", conn);
	} else {
		printk("[ble_cmd] disconnected\n");
		if (ble_cmd_conn) {
			hostif_bt_conn_unref(ble_cmd_conn);
			ble_cmd_conn = NULL;
		}
		ble_cmd_notify_on = 0;
	}
}

void system_app_ble_remote_cmd_init(void)
{
	if (ble_cmd_registered) {
		return;
	}

	memset(&ble_remote_cmd_mgr, 0, sizeof(ble_remote_cmd_mgr));
	ble_remote_cmd_mgr.link_cb = ble_remote_cmd_link_cb;
	ble_remote_cmd_mgr.gatt_svc.attrs = ble_remote_cmd_attrs;
	ble_remote_cmd_mgr.gatt_svc.attr_count = ARRAY_SIZE(ble_remote_cmd_attrs);

	bt_manager_ble_service_reg(&ble_remote_cmd_mgr);
	ble_cmd_registered = 1;

	printk("[ble_cmd] init OK svc=0xFFE0 rx=0xFFE1 tx=0xFFE2 ack=%d\n",
#if defined(CONFIG_SYSTEM_APP_BLE_CMD_ACK)
	       1);
#else
	       0);
#endif
}

#endif /* CONFIG_SYSTEM_APP_BLE_REMOTE_CMD */
#endif /* CONFIG_BT_BLE */
