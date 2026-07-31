/*
 * Copyright (c) 2017 Actions Semi Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief btsrvice spp
 */

#define SYS_LOG_DOMAIN "btsrv_spp"
#include "btsrv_os_common.h"
#include "btsrv_inner.h"


struct btsrv_spp_priv {
	uint8_t spp_id;		/* spp_id from spp stack */
	uint8_t app_id;		/* app_id from spp manager */
	uint8_t connected:1;
	uint8_t active_connect:1;
	uint8_t *uuid;
	struct bt_conn *conn;
};

static struct btsrv_spp_priv spp_priv[CONFIG_MAX_SPP_CHANNEL];
static btsrv_spp_callback spp_user_callback;

static struct btsrv_spp_priv *btsrv_spp_find(uint8_t spp_id, uint8_t app_id, uint8_t *uuid)
{
	int i;
	struct btsrv_spp_priv *spp_info = NULL;

	for (i = 0; i < CONFIG_MAX_SPP_CHANNEL; i++) {
		if (spp_id) {
			if (spp_priv[i].spp_id == spp_id) {
				spp_info = &spp_priv[i];
				break;
			}
		} else if (app_id) {
			if (spp_priv[i].app_id == app_id) {
				spp_info = &spp_priv[i];
				break;
			}
		} else if (uuid) {
			if (!memcmp(spp_priv[i].uuid, uuid, SPP_UUID_LEN)) {
				spp_info = &spp_priv[i];
				break;
			}
		} else {
			if (!spp_priv[i].app_id) {
				spp_info = &spp_priv[i];
				spp_info->active_connect = 0;
				break;
			}
		}
	}

	return spp_info;
}

static void btsrv_spp_free(struct btsrv_spp_priv *spp_info)
{
	memset(spp_info, 0, sizeof(struct btsrv_spp_priv));
}

static void spp_connect_failed(struct bt_conn *conn, uint8_t spp_id)
{
	btsrv_event_notify_ext(MSG_BTSRV_SPP, MSG_BTSRV_SPP_CONNECT_FAILED, conn, spp_id);
}

static void spp_connected_cb(struct bt_conn *conn, uint8_t spp_id)
{
	btsrv_event_notify_ext(MSG_BTSRV_SPP, MSG_BTSRV_SPP_CONNECTED, conn, spp_id);
}

static void spp_disconnected_cb(struct bt_conn *conn, uint8_t spp_id)
{
	btsrv_event_notify_ext(MSG_BTSRV_SPP, MSG_BTSRV_SPP_DISCONNECTED, conn, spp_id);
}

static void spp_recv_cb(struct bt_conn *conn, uint8_t spp_id, uint8_t *data, uint16_t len)
{
	/* Callback by hci_rx thread, in negative priority,
	 * just check spp_user_callback is enough, not need to lock.
	 */

	struct btsrv_spp_priv *p_spp;

	p_spp = btsrv_spp_find(spp_id, 0, NULL);
	if (p_spp && spp_user_callback) {
		spp_user_callback(BTSRV_SPP_DATA_INDICATED, p_spp->app_id, data, len);
	}
}

static const struct bt_spp_app_cb spp_app_cb = {
	.connect_failed = spp_connect_failed,
	.connected = spp_connected_cb,
	.disconnected = spp_disconnected_cb,
	.recv = spp_recv_cb,
};

static void btsrv_spp_reg(struct bt_spp_reg_param *param)
{
	struct btsrv_spp_priv *p_spp;

	p_spp = btsrv_spp_find(0, 0, param->uuid);
	if (p_spp) {
		spp_user_callback(BTSRV_SPP_REGISTER_FAILED, param->app_id, NULL, 0);
		SYS_LOG_WRN("Already register\n");
		return;
	}

	p_spp = btsrv_spp_find(0, 0, NULL);
	if (!p_spp) {
		spp_user_callback(BTSRV_SPP_REGISTER_FAILED, param->app_id, NULL, 0);
		SYS_LOG_WRN("Not more idle spp\n");
		return;
	}

	p_spp->connected = 0;
	p_spp->conn = NULL;
	p_spp->uuid = param->uuid;
	p_spp->app_id = param->app_id;
	p_spp->spp_id = hostif_bt_spp_register_service(param->uuid);
	if (p_spp->spp_id == 0) {
		btsrv_spp_free(p_spp);
		if (spp_user_callback) {
			spp_user_callback(BTSRV_SPP_REGISTER_FAILED, param->app_id, NULL, 0);
		}
		SYS_LOG_WRN("Failed to register spp\n");
	} else {
		if (spp_user_callback) {
			spp_user_callback(BTSRV_SPP_REGISTER_SUCCESS, p_spp->app_id, NULL, 0);
		}
		SYS_LOG_INF("Register spp, app_id %d, spp_id %d\n", p_spp->app_id, p_spp->spp_id);
	}
}

static void btsrv_spp_connect(struct bt_spp_connect_param *param)
{
	struct btsrv_spp_priv *p_spp;
	struct bt_conn *conn;
	uint8_t connect_uuid[16], i;

	conn = btsrv_rdm_find_conn_by_addr(&param->bd);
	if (conn == NULL) {
		SYS_LOG_WRN("BR not connected\n");
		goto connect_failed;
	}

	p_spp = btsrv_spp_find(0, 0, NULL);
	if (!p_spp) {
		SYS_LOG_WRN("Not more idle spp\n");
		goto connect_failed;
	}

	for (i = 0; i < 16; i++) {
		connect_uuid[i] = param->uuid[15 - i];
	}

	p_spp->connected = 0;
	p_spp->conn = NULL;
	p_spp->active_connect = 1;
	p_spp->uuid = param->uuid;
	p_spp->app_id = param->app_id;
	p_spp->spp_id = hostif_bt_spp_connect(conn, connect_uuid);
	if (p_spp->spp_id == 0) {
		btsrv_spp_free(p_spp);
		goto connect_failed;
	}

	return;

connect_failed:
	spp_user_callback(BTSRV_SPP_CONNECT_FAILED, param->app_id, NULL, 0);
}

static void btsrv_spp_disconnect(uint8_t app_id)
{
	struct btsrv_spp_priv *p_spp;

	p_spp = btsrv_spp_find(0, app_id, NULL);
	if (p_spp) {
		hostif_bt_spp_disconnect(p_spp->spp_id);
	}
}

static void btsrv_spp_connect_failed(struct bt_conn *conn, uint8_t spp_id)
{
	struct btsrv_spp_priv *spp_info = NULL;

	spp_info = btsrv_spp_find(spp_id, 0, NULL);
	if (!spp_info) {
		SYS_LOG_WRN("Not find spp_info for spp_id %d\n", spp_id);
		return;
	}

	if (spp_user_callback) {
		spp_user_callback(BTSRV_SPP_CONNECT_FAILED, spp_info->app_id, NULL, 0);
	}

	if (spp_info->active_connect) {
		btsrv_spp_free(spp_info);
	}
}

static void btsrv_spp_connected(struct bt_conn *conn, uint8_t spp_id)
{
	struct btsrv_spp_priv *spp_info = NULL;

	spp_info = btsrv_spp_find(spp_id, 0, NULL);
	if (!spp_info) {
		SYS_LOG_WRN("Not find spp_info for spp_id %d\n", spp_id);
		return;
	}

	if (spp_info->connected) {
		SYS_LOG_INF("Already connected\n");
		return;
	}

	spp_info->connected = 1;
	spp_info->conn = conn;
	btsrv_event_notify(MSG_BTSRV_CONNECT, MSG_BTSRV_SPP_CONNECTED, conn);
	if (spp_user_callback) {
		spp_user_callback(BTSRV_SPP_CONNECTED, spp_info->app_id, NULL, 0);
	}

	btsrv_tws_sync_info_type(conn, BTSRV_SYNC_TYPE_SPP, 0);
}

static void btsrv_spp_disconnected(struct bt_conn *conn, uint8_t spp_id)
{
	struct btsrv_spp_priv *spp_info = NULL;

	spp_info = btsrv_spp_find(spp_id, 0, NULL);
	if (!spp_info) {
		SYS_LOG_WRN("Not find spp_info for spp_id %d\n", spp_id);
		return;
	}

	if (!spp_info->connected) {
		SYS_LOG_INF("Already disconnected\n");
		if (spp_info->active_connect) {
			if (spp_user_callback) {
				spp_user_callback(BTSRV_SPP_CONNECT_FAILED, spp_info->app_id, NULL, 0);
			}
			btsrv_spp_free(spp_info);
		}
		return;
	}

	spp_info->connected = 0;
	spp_info->conn = NULL;
	btsrv_event_notify(MSG_BTSRV_CONNECT, MSG_BTSRV_SPP_DISCONNECTED, conn);

	if (spp_user_callback) {
		spp_user_callback(BTSRV_SPP_DISCONNECTED, spp_info->app_id, NULL, 0);
	}

	if (spp_info->active_connect) {
		btsrv_spp_free(spp_info);
	}

	btsrv_tws_sync_info_type(conn, BTSRV_SYNC_TYPE_SPP, 0);
}

int btsrv_spp_send_data(uint8_t app_id, uint8_t *data, uint32_t len)
{
	struct btsrv_spp_priv *p_spp;

	p_spp = btsrv_spp_find(0, app_id, NULL);
	if (p_spp) {
		return hostif_bt_spp_send_data(p_spp->spp_id, data, len);
	} else {
		return -EIO;
	}
}

int btsrv_spp_process(struct app_msg *msg)
{
	switch (_btsrv_get_msg_param_cmd(msg)) {
	case MSG_BTSRV_SPP_START:
		spp_user_callback = (btsrv_spp_callback)msg->ptr;
		hostif_bt_spp_register_cb((struct bt_spp_app_cb *)&spp_app_cb);
		SYS_LOG_INF("MSG_BTSRV_SPP_START\n");
		break;
	case MSG_BTSRV_SPP_STOP:
		spp_user_callback = NULL;
		break;
	case MSG_BTSRV_SPP_REGISTER:
		btsrv_spp_reg(msg->ptr);
		break;
	case MSG_BTSRV_SPP_CONNECT:
		btsrv_spp_connect(msg->ptr);
		break;
	case MSG_BTSRV_SPP_DISCONNECT:
		btsrv_spp_disconnect((uint8_t)_btsrv_get_msg_param_value(msg));
		break;
	case MSG_BTSRV_SPP_CONNECT_FAILED:
		SYS_LOG_INF("MSG_BTSRV_SPP_CONNECT_FAILED\n");
		btsrv_spp_connect_failed(_btsrv_get_msg_param_ptr(msg), _btsrv_get_msg_param_reserve(msg));
		break;
	case MSG_BTSRV_SPP_CONNECTED:
		SYS_LOG_INF("MSG_BTSRV_SPP_CONNECTED\n");
		btsrv_spp_connected(_btsrv_get_msg_param_ptr(msg), _btsrv_get_msg_param_reserve(msg));
		break;
	case MSG_BTSRV_SPP_DISCONNECTED:
		SYS_LOG_INF("MSG_BTSRV_SPP_DISCONNECTED\n");
		btsrv_spp_disconnected(_btsrv_get_msg_param_ptr(msg), _btsrv_get_msg_param_reserve(msg));
		break;
	default:
		break;
	}
	return 0;
}

int btsrv_spp_sync_get_info(struct bt_conn *conn, void *info)
{
	int i;
	struct btsrv_sync_spp_info *local_info = info;

	for (i = 0; i < CONFIG_MAX_SPP_CHANNEL; i++) {
		local_info->info[i].spp_id = spp_priv[i].spp_id;
		local_info->info[i].app_id = spp_priv[i].app_id;
		local_info->info[i].connected = spp_priv[i].connected;
		local_info->info[i].active_connect = spp_priv[i].active_connect;
		local_info->info[i].conn_for_spp = (conn == spp_priv[i].conn) ? 1 : 0;
	}

	return 0;
}

int btsrv_spp_sync_set_info(struct bt_conn *conn, void *info)
{
	int i, j, param, still_connected;
	struct btsrv_spp_priv *spp_info;
	struct btsrv_sync_spp_info *remote_info = info;
	uint8_t local_pre_connected;

	/* Check connected */
	for (i = 0; i < CONFIG_MAX_SPP_CHANNEL; i++) {
		if (remote_info->info[i].spp_id && remote_info->info[i].connected && remote_info->info[i].conn_for_spp) {
			spp_info = btsrv_spp_find(remote_info->info[i].spp_id, 0, NULL);
			if(!spp_info){
				return -1;
			}
			local_pre_connected = spp_info->connected;
			spp_info->spp_id = remote_info->info[i].spp_id;
			spp_info->app_id = remote_info->info[i].app_id;
			spp_info->connected = remote_info->info[i].connected;
			spp_info->active_connect = remote_info->info[i].active_connect;
			spp_info->conn = conn;

			if (local_pre_connected != spp_info->connected) {
				btsrv_event_notify(MSG_BTSRV_CONNECT, MSG_BTSRV_SPP_CONNECTED, conn);
				if (spp_user_callback) {
					param = spp_info->active_connect;
					spp_user_callback(BTSRV_SPP_SNOOP_CONNECTED, spp_info->app_id, &param, sizeof(param));
				}
			}
		}
	}

	/* Check disconnected */
	for (i = 0; i < CONFIG_MAX_SPP_CHANNEL; i++) {
		spp_info = &spp_priv[i];
		if (spp_info->spp_id && spp_info->connected && spp_info->conn == conn) {
			still_connected = 0;
			for (j = 0; j < CONFIG_MAX_SPP_CHANNEL; j++) {
				if (spp_info->spp_id == remote_info->info[j].spp_id &&
					remote_info->info[j].connected && remote_info->info[j].conn_for_spp) {
					still_connected = 1;
					break;
				}
			}

			if (!still_connected) {
				spp_info->connected = 0;
				spp_info->conn = NULL;
				btsrv_event_notify(MSG_BTSRV_CONNECT, MSG_BTSRV_SPP_DISCONNECTED, conn);

				if (spp_user_callback) {
					spp_user_callback(BTSRV_SPP_SNOOP_DISCONNECTED, spp_info->app_id, NULL, 0);
				}

				if (spp_info->active_connect) {
					btsrv_spp_free(spp_info);
				}
			}
		}
	}

	return 0;
}

void btsrv_spp_snoop_link_disconnected_clear(struct bt_conn *conn)
{
	int i;
	struct btsrv_spp_priv *spp_info;

	for (i = 0; i < CONFIG_MAX_SPP_CHANNEL; i++) {
		spp_info = &spp_priv[i];
		if (spp_info->spp_id && spp_info->connected && spp_info->conn == conn) {
			spp_info->connected = 0;
			spp_info->conn = NULL;
			btsrv_event_notify(MSG_BTSRV_CONNECT, MSG_BTSRV_SPP_DISCONNECTED, conn);

			if (spp_user_callback) {
				spp_user_callback(BTSRV_SPP_SNOOP_DISCONNECTED, spp_info->app_id, NULL, 0);
			}

			if (spp_info->active_connect) {
				btsrv_spp_free(spp_info);
			}
		}
	}
}

void btsrv_spp_dump_info(void)
{
	int i;
	struct btsrv_spp_priv *spp_info;

	printk("Btsrv spp info\n");
	for (i = 0; i < CONFIG_MAX_SPP_CHANNEL; i++) {
		spp_info = &spp_priv[i];
		if (spp_info->conn) {
			printk("Spp hdl 0x%x spp_id %d app_id %d connected %d active_connect %d\n",
					hostif_bt_conn_get_handle(spp_info->conn), spp_info->app_id, spp_info->app_id,
					spp_info->connected, spp_info->active_connect);
		}
	}
	printk("\n");
}
