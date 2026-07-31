/*
 * Copyright (c) 2016 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt hid interface
 */

#define SYS_LOG_DOMAIN "btif_hid"
#include "btsrv_os_common.h"
#include "btsrv_inner.h"

int btif_did_register_processer(void)
{
	return btsrv_register_msg_processer(MSG_BTSRV_DID, &btsrv_did_process);
}

int btif_did_register_sdp(uint8_t *data, uint32_t len)
{
	return btsrv_function_call_malloc(MSG_BTSRV_DID, MSG_BTSRV_DID_REGISTER, data,len,0);
}

int btif_did_register_sdp_cb(uint8_t *data, uint32_t len)
{
	return btsrv_function_call_malloc(MSG_BTSRV_DID, MSG_BTSRV_DID_REGISTER, data,len,0);
}

