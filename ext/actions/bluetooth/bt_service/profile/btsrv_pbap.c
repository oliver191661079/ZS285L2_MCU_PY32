/*
 * Copyright (c) 2017 Actions Semi Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief btsrvice pbap
 */

#define SYS_LOG_DOMAIN "btsrv_pbap"
#include "btsrv_os_common.h"
#include "btsrv_inner.h"

struct btsrv_pbap_priv {
	struct bt_conn *conn;
	uint8_t app_id;	/* ID from bt manager */
	uint8_t user_id;	/* ID from pbap client */
	char *path;
	uint8_t connected:1;
	btsrv_pbap_callback cb;
};

static struct btsrv_pbap_priv pbap_priv[CONFIG_MAX_PBAP_CONNECT];

static void *btsrv_pbap_find_free_priv(void)
{
	int i;

	for (i = 0; i < CONFIG_MAX_PBAP_CONNECT; i++) {
		if (pbap_priv[i].app_id == 0) {
			return &pbap_priv[i];
		}
	}

	return NULL;
}

static void btsrv_pbap_free_priv(struct btsrv_pbap_priv *info)
{
	memset(info, 0, sizeof(struct btsrv_pbap_priv));
}

static void *btsrv_pbap_find_priv_by_app_id(uint8_t app_id)
{
	int i;

	for (i = 0; i < CONFIG_MAX_PBAP_CONNECT; i++) {
		if (pbap_priv[i].app_id == app_id) {
			return &pbap_priv[i];
		}
	}

	return NULL;
}

static void *btsrv_pbap_find_priv_by_user_id(struct bt_conn *conn, uint8_t user_id)
{
	int i;

	for (i = 0; i < CONFIG_MAX_PBAP_CONNECT; i++) {
		if (pbap_priv[i].user_id == user_id && pbap_priv[i].conn == conn) {
			return &pbap_priv[i];
		}
	}

	return NULL;
}

static void btsrv_pbap_connect_failed_cb(struct bt_conn *conn, uint8_t user_id)
{
	btsrv_event_notify_ext(MSG_BTSRV_PBAP, MSG_BTSRV_PBAP_CONNECT_FAILED, conn, user_id);
}

static void btsrv_pbap_connected_cb(struct bt_conn *conn, uint8_t user_id)
{
	btsrv_event_notify_ext(MSG_BTSRV_PBAP, MSG_BTSRV_PBAP_CONNECTED, conn, user_id);
}

void btsrv_pbap_disconnected_cb(struct bt_conn *conn, uint8_t user_id)
{
	btsrv_event_notify_ext(MSG_BTSRV_PBAP, MSG_BTSRV_PBAP_DISCONNECTED, conn, user_id);
}

void btsrv_pbap_recv_cb(struct bt_conn *conn, uint8_t user_id, struct parse_vcard_result *result, uint8_t array_size)
{
	/* Callback by hci_rx thread, in negative priority,
	 * is better send message to bt service, process by btservice??
	 */
	struct btsrv_pbap_priv *info = btsrv_pbap_find_priv_by_user_id(conn, user_id);

	if (!info) {
		return;
	}

	info->cb(BTSRV_PBAP_VCARD_RESULT, info->app_id, result, array_size);
}

static const struct bt_pbap_client_user_cb pbap_client_cb = {
	.connect_failed = btsrv_pbap_connect_failed_cb,
	.connected = btsrv_pbap_connected_cb,
	.disconnected = btsrv_pbap_disconnected_cb,
	.recv = btsrv_pbap_recv_cb,
};

static void btsrv_pbap_get_phonebook(struct bt_get_pb_param *param)
{
	struct btsrv_pbap_priv *info = btsrv_pbap_find_free_priv();
	struct bt_conn *conn = btsrv_rdm_find_conn_by_addr(&param->bd);

	if (!info || !conn) {
		param->cb(BTSRV_PBAP_CONNECT_FAILED, param->app_id, NULL, 0);
		return;
	}

	info->conn = conn;
	info->app_id = param->app_id;
	info->path = param->pb_path;
	info->cb = param->cb;
	info->user_id = hostif_bt_pbap_client_get_phonebook(info->conn, info->path,
									(struct bt_pbap_client_user_cb *)&pbap_client_cb);
	if (!info->user_id) {
		info->cb(BTSRV_PBAP_CONNECT_FAILED, param->app_id, NULL, 0);
		btsrv_pbap_free_priv(info);
	}
}

static void btsrv_pbap_abort_get(uint8_t app_id)
{
	struct btsrv_pbap_priv *info = btsrv_pbap_find_priv_by_app_id(app_id);

	if (!info) {
		return;
	}

	hostif_bt_pbap_client_abort_get(info->conn, info->user_id);
}

static void btsrv_pbap_connect_failed(struct bt_conn *conn, uint8_t user_id)
{
	struct btsrv_pbap_priv *info = btsrv_pbap_find_priv_by_user_id(conn, user_id);

	if (!info) {
		return;
	}

	info->cb(BTSRV_PBAP_CONNECT_FAILED, info->app_id, NULL, 0);
	btsrv_pbap_free_priv(info);
}

static void btsrv_pbap_connected(struct bt_conn *conn, uint8_t user_id)
{
	struct btsrv_pbap_priv *info = btsrv_pbap_find_priv_by_user_id(conn, user_id);

	if (!info) {
		return;
	}

	if (info->connected) {
		SYS_LOG_INF("Already connected\n");
		return;
	}

	info->connected = 1;
	btsrv_event_notify(MSG_BTSRV_CONNECT, MSG_BTSRV_PBAP_CONNECTED, conn);
	info->cb(BTSRV_PBAP_CONNECTED, info->app_id, NULL, 0);
}

static void btsrv_pbap_disconnected(struct bt_conn *conn, uint8_t user_id)
{
	struct btsrv_pbap_priv *info = btsrv_pbap_find_priv_by_user_id(conn, user_id);

	if (!info) {
		return;
	}

	if (!info->connected) {
		SYS_LOG_INF("Connect failed\n");
	}

	info->connected = 0;
	btsrv_event_notify(MSG_BTSRV_CONNECT, MSG_BTSRV_PBAP_DISCONNECTED, conn);
	info->cb(BTSRV_PBAP_DISCONNECTED, info->app_id, NULL, 0);
	btsrv_pbap_free_priv(info);
}

int btsrv_pbap_process(struct app_msg *msg)
{
	switch (_btsrv_get_msg_param_cmd(msg)) {
	case MSG_BTSRV_PBAP_GET_PB:
		btsrv_pbap_get_phonebook(msg->ptr);
		break;
	case MSG_BTSRV_PBAP_ABORT_GET:
		btsrv_pbap_abort_get((uint8_t)_btsrv_get_msg_param_value(msg));
		break;
	case MSG_BTSRV_PBAP_CONNECT_FAILED:
		btsrv_pbap_connect_failed(_btsrv_get_msg_param_ptr(msg), _btsrv_get_msg_param_reserve(msg));
		break;
	case MSG_BTSRV_PBAP_CONNECTED:
		btsrv_pbap_connected(_btsrv_get_msg_param_ptr(msg), _btsrv_get_msg_param_reserve(msg));
		break;
	case MSG_BTSRV_PBAP_DISCONNECTED:
		btsrv_pbap_disconnected(_btsrv_get_msg_param_ptr(msg), _btsrv_get_msg_param_reserve(msg));
		break;
	default:
		break;
	}

	return 0;
}
