/*
 * Copyright (c) 2017 Actions Semi Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief btsrvice hid
 */

#define SYS_LOG_DOMAIN "btsrv_hid"
#include "btsrv_os_common.h"
#include "btsrv_inner.h"

static btsrv_hid_callback hid_user_callback;

static void hid_connected_cb(struct bt_conn *conn)
{
	btsrv_event_notify(MSG_BTSRV_CONNECT, MSG_BTSRV_HID_CONNECTED, conn);
}

static void hid_disconnected_cb(struct bt_conn *conn)
{
	btsrv_event_notify(MSG_BTSRV_CONNECT, MSG_BTSRV_HID_DISCONNECTED, conn);
}

static void hid_event_cb(struct bt_conn *conn, uint8_t event,uint8_t *data, uint16_t len)
{
	/* Callback by hci_rx thread, in negative priority,
	 * just check hid_user_callback is enough, not need to lock.
	 */
	if (hid_user_callback) {
		switch(event){
			case BT_HID_EVENT_GET_REPORT:
				hid_user_callback(BTSRV_HID_GET_REPORT, data, len);
			break;
			case BT_HID_EVENT_SET_REPORT:
				hid_user_callback(BTSRV_HID_SET_REPORT, data, len);
			break;
			case BT_HID_EVENT_GET_PROTOCOL:
				hid_user_callback(BTSRV_HID_GET_PROTOCOL, data, len);
			break;
			case BT_HID_EVENT_SET_PROTOCOL:
				hid_user_callback(BTSRV_HID_SET_PROTOCOL, data, len);
			break;
			case BT_HID_EVENT_INTR_DATA:
				hid_user_callback(BTSRV_HID_INTR_DATA, data, len);
			break;
			case BT_HID_EVENT_UNPLUG:
				btsrv_event_notify(MSG_BTSRV_CONNECT, MSG_BTSRV_HID_UNPLUG, conn);
				hid_user_callback(BTSRV_HID_UNPLUG, data, len);
			break;
			case BT_HID_EVENT_SUSPEND:
				hid_user_callback(BTSRV_HID_SUSPEND, data, len);
			break;
			case BT_HID_EVENT_EXIT_SUSPEND:
				hid_user_callback(BTSRV_HID_EXIT_SUSPEND, data, len);
			break;
		}
	}
}

static const struct bt_hid_app_cb hid_app_cb = {
	.connected = hid_connected_cb,
	.disconnected = hid_disconnected_cb,
	.event_cb = hid_event_cb,
};

void btsrv_hid_connect(struct bt_conn *conn)
{
	int ret = hostif_bt_hid_connect(conn);
	if (!ret) {
		SYS_LOG_INF("Connect hid\n");
	} else {
		SYS_LOG_ERR("Connect hid failed %d\n",ret);
	}
}

static void btsrv_hid_disconnect(struct bt_conn *conn)
{
	if (conn) {
		SYS_LOG_INF("hid_disconnect\n");
		hostif_bt_hid_disconnect(conn);
	}
}

static void btsrv_hid_connected(struct bt_conn *conn)
{
	if (hid_user_callback) {
		hid_user_callback(BTSRV_HID_CONNECTED, NULL, 0);
	}
}

static void btsrv_hid_disconnected(struct bt_conn *conn)
{
	if (hid_user_callback) {
		hid_user_callback(BTSRV_HID_DISCONNECTED, NULL, 0);
	}
}

int btsrv_hid_send_ctrl_data(struct bt_hid_report * report)
{
	struct bt_conn *conn = btsrv_rdm_hid_get_actived();
	if(conn)
		return hostif_bt_hid_send_ctrl_data(conn,report->report_type, report->data, report->len);
	return 0;
}

int btsrv_hid_send_intr_data(struct bt_hid_report * report)
{
	struct bt_conn *conn = btsrv_rdm_hid_get_actived();
	if(conn)
		return hostif_bt_hid_send_intr_data(conn,report->report_type, report->data, report->len);
	return 0;
}

int btsrv_hid_send_rsp(uint8_t status)
{
	struct bt_conn *conn = btsrv_rdm_hid_get_actived();
	return hostif_bt_hid_send_rsp(conn, status);
}

int btsrv_hid_process(struct app_msg *msg)
{
	struct bt_hid_report * report;
	switch (_btsrv_get_msg_param_cmd(msg)) {
	case MSG_BTSRV_HID_START:
		hid_user_callback = (btsrv_hid_callback)msg->ptr;
		hostif_bt_hid_register_cb((struct bt_hid_app_cb *)&hid_app_cb);
		break;
	case MSG_BTSRV_HID_STOP:
		hid_user_callback = NULL;
		break;
	case MSG_BTSRV_HID_REGISTER:
		hostif_bt_hid_register_sdp(msg->ptr,_btsrv_get_msg_param_reserve(msg));
		break;
	case MSG_BTSRV_HID_CONNECT:
		SYS_LOG_INF("MSG_BTSRV_HID_CONNECT\n");
		btsrv_hid_connect(msg->ptr);
		break;
	case MSG_BTSRV_HID_DISCONNECT:
		SYS_LOG_INF("MSG_BTSRV_HID_DISCONNECT\n");
		btsrv_hid_disconnect(msg->ptr);
		break;
	case MSG_BTSRV_HID_CONNECTED:
		SYS_LOG_INF("MSG_BTSRV_HID_CONNECTED\n");
		btsrv_hid_connected(_btsrv_get_msg_param_ptr(msg));
		break;
	case MSG_BTSRV_HID_DISCONNECTED:
		SYS_LOG_INF("MSG_BTSRV_HID_DISCONNECTED\n");
		btsrv_hid_disconnected(_btsrv_get_msg_param_ptr(msg));
		break;
	case MSG_BTSRV_HID_SEND_CTRL_DATA:
		SYS_LOG_INF("MSG_BTSRV_HID_SEND_CTRL_DATA\n");
		report = (struct bt_hid_report * )_btsrv_get_msg_param_ptr(msg);
		report->data = (uint8_t*)report + sizeof(struct bt_hid_report);
		btsrv_hid_send_ctrl_data(report);
		break;
	case MSG_BTSRV_HID_SEND_INTR_DATA:
		SYS_LOG_INF("MSG_BTSRV_HID_SEND_INTR_DATA\n");
		report = (struct bt_hid_report * )_btsrv_get_msg_param_ptr(msg);
		report->data = (uint8_t*)report + sizeof(struct bt_hid_report);
		btsrv_hid_send_intr_data(report);
		break;
	case MSG_BTSRV_HID_SEND_RSP:
		SYS_LOG_INF("MSG_BTSRV_HID_SEND_RSP\n");
		btsrv_hid_send_rsp(_btsrv_get_msg_param_reserve(msg));
		break;
	default:
		break;
	}
	return 0;
}
