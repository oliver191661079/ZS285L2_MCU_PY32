/*
 * Copyright (c) 2016 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt bip interface
 */


#define SYS_LOG_DOMAIN "btif_bip"
#include "btsrv_os_common.h"
#include "btsrv_inner.h"

int btif_bipc_start(btsrv_bipc_callback cb, btsrv_bipc_config_t* cfg)
{
	uint32_t param[3] = {0};
	if (!cb) {
		return -EIO;
	}
	btsrv_register_msg_processer(MSG_BTSRV_BIPC, &btsrv_bipc_process);
	param[0] = (uint32_t)cb;
	if (cfg) {
		param[1] = cfg->notify_supported_capability;
		param[2] = cfg->notify_supported_feature;
	}

	return btsrv_function_call_malloc(MSG_BTSRV_BIPC, MSG_BTSRV_BIPC_START, (uint8_t*)param, sizeof(param), 0);
}

int btif_bipc_stop(void)
{
	return btsrv_function_call(MSG_BTSRV_BIPC, MSG_BTSRV_BIPC_STOP, (void *)NULL);
}

int btif_bipc_connect(btsrv_bipc_connect_param_t* param)
{
	if (!param->app_id) {
		SYS_LOG_ERR("appid is invalid");
		return -EIO;
	}
	return btsrv_function_call_malloc(MSG_BTSRV_BIPC, MSG_BTSRV_BIPC_CONNECT, (uint8_t*)param, sizeof(btsrv_bipc_connect_param_t), 0);
}

int btif_bipc_disconnect(uint8_t app_id)
{
	return btsrv_event_notify_value(MSG_BTSRV_BIPC, MSG_BTSRV_BIPC_DISCONNECT, app_id);
}

int btif_bipc_get_image_properties(uint8_t app_id, const char* img_handle)
{
	if (!img_handle) {
		SYS_LOG_ERR("img handle is invalid");
		return -EIO;
	}

	return btsrv_function_call_malloc(MSG_BTSRV_BIPC, MSG_BTSRV_BIPC_GET_PROPERTITES, (uint8_t*)img_handle, strlen(img_handle) + 1, app_id);
}

int btif_bipc_get_image(uint8_t app_id, const char* img_handle, btsrv_bipc_image_desc_t* img_desc)
{
	if (!img_handle || !img_desc) {
		SYS_LOG_ERR("img handle or desc is invalid");
		return -EIO;
	}

	if (!img_desc->encoding) {
		SYS_LOG_ERR("encoding is null");
		return -EIO;
	}
	int handle_len = strlen(img_handle);
	int encoding_len = strlen(img_desc->encoding);
	int len = sizeof(btsrv_bipc_image_desc_t) + handle_len + encoding_len + 4;
	uint8_t *data = mem_malloc(len);
	memset(data, 0, len);
	data[0] = handle_len;
	strcpy(&data[1], img_handle);
	data[2 + handle_len] = encoding_len;
	strcpy(&data[3 + handle_len], img_desc->encoding);

	btsrv_bipc_image_desc_t* p_img_desc = (btsrv_bipc_image_desc_t*)&data[handle_len + 2 + encoding_len + 2];
	memcpy(p_img_desc, img_desc, sizeof(btsrv_bipc_image_desc_t));
	p_img_desc->encoding = &data[3 + handle_len];

	int ret = btsrv_function_call_malloc(MSG_BTSRV_BIPC, MSG_BTSRV_BIPC_GET_IMAGE, data, len, app_id);
	mem_free(data);
	return ret;
}

int btif_bipc_abort(uint8_t app_id)
{
	return btsrv_event_notify_value(MSG_BTSRV_BIPC, MSG_BTSRV_BIPC_ABORT, app_id);
}

