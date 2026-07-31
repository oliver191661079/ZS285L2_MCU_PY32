/*
 * Copyright (c) 2016 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt pbap interface
 */

#define SYS_LOG_DOMAIN "btif_pbap"
#include "btsrv_os_common.h"
#include "btsrv_inner.h"

int btif_pbap_register_processer(void)
{
	return btsrv_register_msg_processer(MSG_BTSRV_PBAP, &btsrv_pbap_process);
}

int btif_pbap_get_phonebook(struct bt_get_pb_param *param)
{
	if (!param || !param->app_id ||
		!param->cb || !param->pb_path) {
		return -EINVAL;
	}

	return btsrv_function_call_malloc(MSG_BTSRV_PBAP, MSG_BTSRV_PBAP_GET_PB, (void *)param, sizeof(struct bt_get_pb_param), 0);
}

int btif_pbap_abort_get(uint8_t app_id)
{
	int id = app_id;

	if (id == 0) {
		return -EINVAL;
	}

	return btsrv_function_call(MSG_BTSRV_PBAP, MSG_BTSRV_PBAP_ABORT_GET, (void *)id);
}
