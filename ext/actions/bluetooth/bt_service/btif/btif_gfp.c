/*
 * Copyright (c) 2016 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt hid interface
 */

#define SYS_LOG_DOMAIN "btif_gfp"
#include "btsrv_os_common.h"
#include "btsrv_inner.h"

int btif_gfp_register_processer(void)
{
	return btsrv_register_msg_processer(MSG_BTSRV_GFP, &btsrv_gfp_process);
}

int btif_gfp_start(struct btsrv_gfp_start_param *param)
{
	return btsrv_function_call_malloc(MSG_BTSRV_GFP, MSG_BTSRV_GFP_START, (uint8_t *)param, sizeof(struct btsrv_gfp_start_param), 0);
}


