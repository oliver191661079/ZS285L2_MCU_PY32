/*
 * Copyright (c) 2017 Actions Semi Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief btsrvice did
 */

#define SYS_LOG_DOMAIN "btsrv_hid"
#include "btsrv_os_common.h"
#include "btsrv_inner.h"



int btsrv_did_process(struct app_msg *msg)
{
	switch (_btsrv_get_msg_param_cmd(msg)) {
	case MSG_BTSRV_DID_REGISTER:
	    SYS_LOG_INF("MSG_BTSRV_DID_REGISTER");
		hostif_bt_did_register_sdp(msg->ptr);
		break;
	default:
		break;
	}
	return 0;
}
