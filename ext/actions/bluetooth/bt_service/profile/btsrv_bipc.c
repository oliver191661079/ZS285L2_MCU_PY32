/*
 * Copyright (c) 2017 Actions Semi Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief btsrvice bipc
 */
#define SYS_LOG_DOMAIN "btsrv_bipc"
#include "btsrv_os_common.h"
#include "btsrv_inner.h"

#define BTSRV_BIPC_MAX_CONN 2
#define BTSRV_BIPC_ENCODING_SIZE 32

typedef enum {
	BTSRV_BIPC_STATE_IDLE = 0,
	BTSRV_BIPC_STATE_CONNECTING,
	BTSRV_BIPC_STATE_CONNECTED,
	BTSRV_BIPC_STATE_GETING_IMAGE,
	BTSRV_BIPC_STATE_DISCONNECTING,
} btsrv_bipc_state_t;

typedef struct {
	bt_bipc_perfer_format_t* perfer;
	uint8_t format_count;
	bt_bipc_format_t* format_list;
} btsrv_bipc_cap_t;

typedef struct {
	struct bt_conn* conn;
	uint8_t app_id;
	uint8_t user_id;
	btsrv_bipc_state_t state;
	bt_bipc_supported_capbilities_t cap_mask;
	bt_bipc_supported_feature_t feature_mask;
	btsrv_bipc_cap_t cap;
} btsrv_bipc_conn_t;

typedef struct {
	btsrv_bipc_conn_t conns[BTSRV_BIPC_MAX_CONN];
	btsrv_bipc_callback cb;
	btsrv_bipc_config_t cfg;
} btsrv_bipc_context_t;

static btsrv_bipc_context_t s_btsrv_bipc_ctx = {0};

static btsrv_bipc_conn_t* btsrv_bipc_conn_malloc(void)
{
	for (uint8_t i = 0; i < BTSRV_BIPC_MAX_CONN; i++) {
		if (s_btsrv_bipc_ctx.conns[i].app_id == 0 && s_btsrv_bipc_ctx.conns[i].user_id == 0 &&
			s_btsrv_bipc_ctx.conns[i].conn == NULL) {
			return &s_btsrv_bipc_ctx.conns[i];
		}
	}
	return NULL;
}

static void btsrv_bipc_conn_free(btsrv_bipc_conn_t* bipc_conn)
{
	if (bipc_conn->cap.perfer) {
		mem_free(bipc_conn->cap.perfer);
	}
	if (bipc_conn->cap.format_list) {
		mem_free(bipc_conn->cap.format_list);
	}
	memset(bipc_conn, 0, sizeof(btsrv_bipc_conn_t));
}

static btsrv_bipc_conn_t* btsrv_bipc_conn_find(uint8_t app_id)
{
	for (uint8_t i = 0; i < BTSRV_BIPC_MAX_CONN; i++) {
		if (s_btsrv_bipc_ctx.conns[i].app_id != 0 && s_btsrv_bipc_ctx.conns[i].app_id == app_id) {
			return &s_btsrv_bipc_ctx.conns[i];
		}
	}
	return NULL;
}

static btsrv_bipc_conn_t* btsrv_bipc_conn_find_by_user_id(uint8_t user_id)
{
	for (uint8_t i = 0; i < BTSRV_BIPC_MAX_CONN; i++) {
		if (s_btsrv_bipc_ctx.conns[i].user_id != 0 && s_btsrv_bipc_ctx.conns[i].user_id == user_id) {
			return &s_btsrv_bipc_ctx.conns[i];
		}
	}
	return NULL;
}

static void btsrv_bipc_notify_evt(uint8_t app_id, btsrv_bipc_evt_t evt, void* data)
{
	if (s_btsrv_bipc_ctx.cb) {
		s_btsrv_bipc_ctx.cb(evt, app_id,  data);
	}
}


static int btsrv_bip_client_evt_cb(struct bt_conn* conn, uint8_t user_id, bt_bipc_evt_t evt, void* data)
{
	SYS_LOG_INF("Eve: %d, user_id: %d", evt, user_id);
	switch (evt) {
		case BT_BIPC_EVT_CONNECT_CFM:
		{
			btsrv_function_call_malloc(MSG_BTSRV_BIPC, MSG_BTSRV_BIPC_CONNECT_CFM, (uint8_t*)data, sizeof(bt_bipc_evt_connect_cfm_t), user_id);
			break;
		}
		case BT_BIPC_EVT_DISCONNECT_IND:
		{
			btsrv_function_call_malloc(MSG_BTSRV_BIPC, MSG_BTSRV_BIPC_DISCONNECT_IND, (uint8_t*)data, sizeof(bt_bipc_evt_disconnect_ind_t), user_id);
			break;
		}
		case BT_BIPC_EVT_GET_CAP_CFM:
		{
			btsrv_bipc_conn_t* bipc_conn = btsrv_bipc_conn_find_by_user_id(user_id);
			if (!bipc_conn) {
				SYS_LOG_ERR("No this user %d", user_id);
				return -1;
			}
			bt_bipc_evt_get_cap_cfm_t* cap = data;
			if (!cap->status) {
				uint32_t len = 0;
				if (cap->perfer) {
					bt_bipc_perfer_format_t* perfer = mem_malloc(BTSRV_BIPC_ENCODING_SIZE + sizeof(bt_bipc_perfer_format_t));
					if (perfer) {
						memcpy(perfer, cap->perfer, sizeof(bt_bipc_perfer_format_t));
						bipc_conn->cap.perfer = perfer;
						bipc_conn->cap.perfer->format.encoding = ((uint8_t*)perfer) + sizeof(bt_bipc_perfer_format_t);
						strcpy(bipc_conn->cap.perfer->format.encoding, cap->perfer->format.encoding);
					} else {
						SYS_LOG_ERR("OOM");
					}
				}
				if (cap->format_count) {
					len += BTSRV_BIPC_ENCODING_SIZE * cap->format_count;
					len += cap->format_count * sizeof(bt_bipc_format_t);

					uint8_t* format = mem_malloc(len);
					if (format) {
						bipc_conn->cap.format_count = cap->format_count;
						bipc_conn->cap.format_list = (bt_bipc_format_t*)format;
						for (uint8_t i = 0; i < cap->format_count; i++) {
							format += sizeof(bt_bipc_format_t) * i;
							bt_bipc_format_t* tmp_fmt = (bt_bipc_format_t*)format;
							memcpy(format, &cap->format_list[i], sizeof(bt_bipc_format_t));
							tmp_fmt->encoding = format + sizeof(bt_bipc_format_t);
							strcpy(tmp_fmt->encoding, cap->format_list[i].encoding);
							format += sizeof(bt_bipc_format_t);
						}
					}
				}
			}
			btsrv_event_notify_value(MSG_BTSRV_BIPC, MSG_BTSRV_BIPC_GET_CAP_CFM, user_id);
			break;
		}
		case BT_BIPC_EVT_GET_IMAGE_PROPERTIES_CFM:
		{
			btsrv_bipc_conn_t* bipc_conn = btsrv_bipc_conn_find_by_user_id(user_id);
			if (!bipc_conn) {
				SYS_LOG_ERR("No this user %d", user_id);
				return -1;
			}
			bt_bipc_evt_get_properties_cfm_t* pro_cfm = data;
			btsrv_bipc_evt_get_properties_cfm_t pro_evt = {0};
			pro_evt.status = pro_cfm->status;
			pro_evt.name = pro_cfm->name;
			pro_evt.img_handle = pro_cfm->img_handle;
			pro_evt.native.encoding = pro_cfm->native.encoding;
			pro_evt.native.size = pro_cfm->native.size;
			pro_evt.native.min.height = pro_cfm->native.min.height;
			pro_evt.native.min.width = pro_cfm->native.min.width;
			pro_evt.native.max.height = pro_cfm->native.max.height;
			pro_evt.native.max.width = pro_cfm->native.max.width;
			pro_evt.variant_num = pro_cfm->variant_count;
			pro_evt.variant = (btsrv_bipc_propertie_variant_t*)pro_cfm->variant;
			btsrv_bipc_notify_evt(bipc_conn->app_id, BTSRV_BIPC_EVT_GET_PROPERTIES_DATA_CFM, &pro_evt);
			break;
		}
		case BT_BIPC_EVT_GET_IMAGE_CFM:
		{
			btsrv_bipc_conn_t* bipc_conn = btsrv_bipc_conn_find_by_user_id(user_id);
			if (!bipc_conn) {
				SYS_LOG_ERR("No this user %d", user_id);
				return -1;
			}
			btsrv_bipc_evt_image_ind_t img_data = {0};
			bt_bipc_evt_get_image_cfm_t* img_cfm = data;
			img_data.status = img_cfm->status;
			if (img_cfm->flag == BT_BIPC_DATA_FLAG_CONTINUE){
				img_data.end = false;
			} else {
				img_data.end = true;
				bipc_conn->state = BTSRV_BIPC_STATE_CONNECTED;
			}
			img_data.len = img_cfm->data_len;
			img_data.data = img_cfm->data;
			btsrv_bipc_notify_evt(bipc_conn->app_id, BTSRV_BIPC_EVT_GET_PROPERTIES_DATA_CFM, &img_data);
			break;
		}
		case BT_BIPC_EVT_ABORT_CFM:
		{
			btsrv_function_call_malloc(MSG_BTSRV_BIPC, MSG_BTSRV_BIPC_ABORT_CFM, (uint8_t*)data, sizeof(bt_bipc_evt_abort_cfm_t), user_id);
			break;
		}

	}
	return 0;
}

static int btsrv_bipc_start(btsrv_bipc_callback cb, btsrv_bipc_config_t* cfg)
{
	s_btsrv_bipc_ctx.cb = cb;
	s_btsrv_bipc_ctx.cfg.notify_supported_capability = cfg->notify_supported_capability;
	s_btsrv_bipc_ctx.cfg.notify_supported_feature = cfg->notify_supported_feature;

	return hostif_bt_bipc_init(btsrv_bip_client_evt_cb);
}

static int btsrv_bipc_stop()
{
	s_btsrv_bipc_ctx.cb = NULL;
	return hostif_bt_bipc_deinit();
}

static int btsrv_bipc_connect(btsrv_bipc_connect_param_t* param)
{
	btsrv_bipc_evt_connect_cfm_t conn_cfm = {0};
	btsrv_bipc_conn_t* bipc_conn = btsrv_bipc_conn_find(param->app_id);
	if (bipc_conn) {
		conn_cfm.status = -1;
		btsrv_bipc_notify_evt(bipc_conn->app_id, BTSRV_BIPC_EVT_CONNECT_CFM, &conn_cfm);
		SYS_LOG_ERR("connected %d", param->app_id);
		return -1;
	}
	struct bt_conn *conn = btsrv_rdm_find_conn_by_addr(&param->bd);
	if (!conn) {
		conn_cfm.status = -1;
		btsrv_bipc_notify_evt(bipc_conn->app_id, BTSRV_BIPC_EVT_CONNECT_CFM, &conn_cfm);
		SYS_LOG_ERR("Address error");
		return -1;
	}

	bipc_conn = btsrv_bipc_conn_malloc();
	if (!bipc_conn) {
		conn_cfm.status = -1;
		btsrv_bipc_notify_evt(bipc_conn->app_id, BTSRV_BIPC_EVT_CONNECT_CFM, &conn_cfm);
		SYS_LOG_ERR("OOM");
		return -1;
	}

	bipc_conn->user_id = hostif_bt_bipc_connect(conn, param->psm);
	if (!bipc_conn->user_id) {
		conn_cfm.status = -1;
		btsrv_bipc_notify_evt(param->app_id, BTSRV_BIPC_EVT_CONNECT_CFM, &conn_cfm);
		SYS_LOG_ERR("conn error");
		return -1;
	}
	bipc_conn->state = BTSRV_BIPC_STATE_CONNECTING;
	bipc_conn->app_id = param->app_id;
	return 0;
}

static int btsrv_bipc_disconnect(uint8_t app_id)
{
	btsrv_bipc_evt_disconnect_ind_t disc_ind = {0};
	btsrv_bipc_conn_t* bipc_conn = btsrv_bipc_conn_find(app_id);
	if (!bipc_conn || bipc_conn->state == BTSRV_BIPC_STATE_DISCONNECTING ||
		bipc_conn->state < BTSRV_BIPC_STATE_CONNECTED) {
		disc_ind.status = -2;
		btsrv_bipc_notify_evt(app_id, BTSRV_BIPC_EVT_DISCONNECT_IND, &disc_ind);
		return -1;
	}

	bipc_conn->state = BTSRV_BIPC_STATE_DISCONNECTING;
	hostif_bt_bipc_disconnect(bipc_conn->conn, bipc_conn->user_id);
	return 0;
}

static int btsrv_bipc_get_image_properties(uint8_t app_id, char* img_handle)
{
	btsrv_bipc_evt_get_properties_cfm_t prop_cfm = {0};
	btsrv_bipc_conn_t* bipc_conn = btsrv_bipc_conn_find(app_id);
	if (!bipc_conn) {
		prop_cfm.status = -2;
		btsrv_bipc_notify_evt(app_id, BTSRV_BIPC_EVT_GET_PROPERTIES_DATA_CFM, &prop_cfm);
		return -1;
	}
	return hostif_bt_bipc_get_image_properties(bipc_conn->conn, bipc_conn->user_id, img_handle);
}

static int btsrv_bipc_get_image(uint8_t app_id, char* img_handle, btsrv_bipc_image_desc_t* img_desc)
{
	btsrv_bipc_evt_image_ind_t img_ind = {0};
	btsrv_bipc_conn_t* bipc_conn = btsrv_bipc_conn_find(app_id);
	if (!bipc_conn) {
		img_ind.status = -2;
		btsrv_bipc_notify_evt(app_id, BTSRV_BIPC_EVT_IMAGE_DATA_IND, &img_ind);
		return -1;
	}

	bt_bipc_image_desc_t stack_image_desc = {0};
	stack_image_desc.encoding = img_desc->encoding;
	stack_image_desc.pixel.height = img_desc->pixel.height;
	stack_image_desc.pixel.width = img_desc->pixel.width;
	stack_image_desc.size = img_desc->size;
	stack_image_desc.max_size = img_desc->max_size;
	stack_image_desc.trans = img_desc->trans;
	int ret = hostif_bt_bipc_get_image(bipc_conn->conn, bipc_conn->user_id, img_handle, &stack_image_desc);
	if (!ret) {
		bipc_conn->state = BTSRV_BIPC_STATE_GETING_IMAGE;
	}
	return ret;
}

static int btsrv_bipc_abort(uint8_t app_id)
{
	btsrv_bipc_evt_abort_cfm_t abort_cfm = {0};
	btsrv_bipc_conn_t* bipc_conn = btsrv_bipc_conn_find(app_id);
	if (!bipc_conn || bipc_conn->state != BTSRV_BIPC_STATE_GETING_IMAGE) {
		abort_cfm.status = -2;
		btsrv_bipc_notify_evt(app_id, BTSRV_BIPC_EVT_ABORT_CFM , &abort_cfm);
		return -1;
	}

	return hostif_bt_bipc_abort(bipc_conn->conn, bipc_conn->user_id);
}

static void btsrv_bipc_handle_evt_connect_cfm(uint8_t user_id, bt_bipc_evt_connect_cfm_t* evt)
{
	btsrv_bipc_conn_t* bipc_conn = btsrv_bipc_conn_find_by_user_id(user_id);
	if (!bipc_conn) {
		SYS_LOG_ERR("No user %d", user_id);
		return;
	}
	if (evt->status) {
		SYS_LOG_ERR("connect fail %d", evt->status);
		btsrv_bipc_evt_connect_cfm_t conn_cfm = {0};
		conn_cfm.status = -1;
		btsrv_bipc_notify_evt(bipc_conn->app_id, BTSRV_BIPC_EVT_CONNECT_CFM, &conn_cfm);
		btsrv_bipc_conn_free(bipc_conn);
		return;
	}

	bipc_conn->cap_mask = evt->cap_mask;
	bipc_conn->feature_mask = evt->feature_mask;

	SYS_LOG_INF("cap: %d, feature: %d", bipc_conn->cap_mask, bipc_conn->feature_mask);

	bipc_conn->state = BTSRV_BIPC_STATE_CONNECTED;
	hostif_bt_bipc_get_capabilities(bipc_conn->conn, user_id);
}

static void btsrv_bipc_handle_evt_disconnect_ind(uint8_t user_id, bt_bipc_evt_disconnect_ind_t* evt)
{
	btsrv_bipc_conn_t* bipc_conn = btsrv_bipc_conn_find_by_user_id(user_id);
	if (!bipc_conn) {
		SYS_LOG_ERR("No user %d", user_id);
		return;
	}
	btsrv_bipc_evt_disconnect_ind_t disc_ind = {0};
	disc_ind.status = evt->status;
	btsrv_bipc_notify_evt(bipc_conn->app_id, BTSRV_BIPC_EVT_CONNECT_CFM, &disc_ind);
	if (evt->status) {
		SYS_LOG_ERR("disconnect fail %d", evt->status);
		return;
	}
	btsrv_bipc_conn_free(bipc_conn);
}

static void btsrv_bipc_handle_evt_abort_cfm(uint8_t user_id, bt_bipc_evt_abort_cfm_t* evt)
{
	btsrv_bipc_conn_t* bipc_conn = btsrv_bipc_conn_find_by_user_id(user_id);
	if (!bipc_conn) {
		SYS_LOG_ERR("No user %d", user_id);
		return;
	}

	bipc_conn->state = BTSRV_BIPC_STATE_CONNECTED;
	btsrv_bipc_evt_abort_cfm_t abort_cfm = {0};
	abort_cfm.status = evt->status;
	btsrv_bipc_notify_evt(bipc_conn->app_id, BTSRV_BIPC_EVT_ABORT_CFM, &abort_cfm);
}


static void btsrv_bipc_handle_evt_get_cap_cfm(uint8_t user_id)
{
	btsrv_bipc_conn_t* bipc_conn = btsrv_bipc_conn_find_by_user_id(user_id);
	if (!bipc_conn) {
		SYS_LOG_ERR("No user %d", user_id);
		return;
	}
	btsrv_bipc_evt_connect_cfm_t conn_cfm = {0};
	conn_cfm.status = 0;
	if (s_btsrv_bipc_ctx.cfg.notify_supported_capability) {
		conn_cfm.cap_mask = bipc_conn->cap_mask;
	}

	if (s_btsrv_bipc_ctx.cfg.notify_supported_feature) {
		conn_cfm.features_mask = bipc_conn->feature_mask;
	}

	btsrv_bipc_notify_evt(bipc_conn->app_id, BTSRV_BIPC_EVT_CONNECT_CFM, &conn_cfm);
}

int btsrv_bipc_process(struct app_msg *msg)
{
	switch (_btsrv_get_msg_param_cmd(msg)) {
		case MSG_BTSRV_BIPC_START:
		{
			uint32_t* param = _btsrv_get_msg_param_ptr(msg);
			btsrv_bipc_callback cb = (btsrv_bipc_callback)param[0];
			btsrv_bipc_config_t cfg;
			cfg.notify_supported_capability = param[1];
			cfg.notify_supported_feature = param[2];
			btsrv_bipc_start(cb, &cfg);
			break;
		}
		case MSG_BTSRV_BIPC_STOP:
			btsrv_bipc_stop();
			break;
		case MSG_BTSRV_BIPC_CONNECT:
		{
			SYS_LOG_INF("BIP Connect");
			btsrv_bipc_connect(_btsrv_get_msg_param_ptr(msg));
			break;
		}
		case MSG_BTSRV_BIPC_DISCONNECT:
		{
			btsrv_bipc_disconnect(msg->value);
			break;
		}
		case MSG_BTSRV_BIPC_GET_PROPERTITES:
		{
			btsrv_bipc_get_image_properties(msg->reserve, msg->ptr);
			break;
		}

		case MSG_BTSRV_BIPC_GET_IMAGE:
		{
			uint8_t* p_data = msg->ptr;
			uint8_t img_handle_len = p_data[0];
			char* img_handle = &p_data[1];
			uint8_t encoding_len = p_data[img_handle_len + 2];
			char* encoding = &p_data[3 + img_handle_len];
			btsrv_bipc_image_desc_t* img_desc = (btsrv_bipc_image_desc_t*)&p_data[4 + img_handle_len + encoding_len];
			img_desc->encoding = encoding;
			btsrv_bipc_get_image(msg->reserve, img_handle, img_desc);
			break;
		}

		case MSG_BTSRV_BIPC_ABORT:
		{
			btsrv_bipc_abort(msg->value);
			break;
		}

		case MSG_BTSRV_BIPC_CONNECT_CFM:
		{
			btsrv_bipc_handle_evt_connect_cfm(msg->reserve, (bt_bipc_evt_connect_cfm_t*)msg->ptr);
			break;
		}

		case MSG_BTSRV_BIPC_DISCONNECT_IND:
		{
			btsrv_bipc_handle_evt_disconnect_ind(msg->reserve, (bt_bipc_evt_disconnect_ind_t*)msg->ptr);
			break;
		}
		case MSG_BTSRV_BIPC_ABORT_CFM:
		{
			btsrv_bipc_handle_evt_abort_cfm(msg->reserve, (bt_bipc_evt_abort_cfm_t*)msg->ptr);
			break;
		}
		case MSG_BTSRV_BIPC_GET_CAP_CFM:
		{
			btsrv_bipc_handle_evt_get_cap_cfm(msg->value);
			break;
		}
		default:
			break;
	}
	return 0;
}

