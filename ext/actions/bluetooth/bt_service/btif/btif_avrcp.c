/*
 * Copyright (c) 2016 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt avrcp interface
 */

#define SYS_LOG_DOMAIN "btif_avrcp"
#include "btsrv_os_common.h"
#include "btsrv_inner.h"

int btif_avrcp_register_processer(void)
{
	return btsrv_register_msg_processer(MSG_BTSRV_AVRCP, &btsrv_avrcp_process);
}

int btif_avrcp_start(btsrv_avrcp_callback_t *cb)
{
	return btsrv_function_call(MSG_BTSRV_AVRCP, MSG_BTSRV_AVRCP_START, (void *)cb);
}

int btif_avrcp_stop(void)
{
	return btsrv_function_call(MSG_BTSRV_AVRCP, MSG_BTSRV_AVRCP_STOP, (void *)NULL);
}

int btif_avrcp_connect(bd_address_t *addr)
{
	return btsrv_function_call_malloc(MSG_BTSRV_AVRCP, MSG_BTSRV_AVRCP_CONNECT_TO, (void *)addr, sizeof(bd_address_t), 0);
}

int btif_avrcp_disconnect(bd_address_t *addr)
{
	return btsrv_function_call_malloc(MSG_BTSRV_AVRCP, MSG_BTSRV_AVRCP_DISCONNECT, (void *)addr, sizeof(bd_address_t), 0);
}

int btif_avrcp_send_command(int command)
{
	uint32_t param = (0) | (command << 16);
	return btsrv_function_call(MSG_BTSRV_AVRCP, MSG_BTSRV_AVRCP_SEND_CMD, (void *)param);
}

int btif_avrcp_send_command_by_hdl(uint16_t hdl, int command)
{
	uint32_t param = (hdl) | (command << 16);
	return btsrv_function_call(MSG_BTSRV_AVRCP, MSG_BTSRV_AVRCP_SEND_CMD, (void *)param);
}


int btif_avrcp_sync_vol(uint16_t hdl, uint16_t delay_ms)
{
    uint32_t param = (hdl) | (delay_ms << 16);
	return btsrv_function_call(MSG_BTSRV_AVRCP, MSG_BTSRV_AVRCP_SYNC_VOLUME, (void *)param);
}

int btif_avrcp_get_id3_info()
{
	return btsrv_function_call(MSG_BTSRV_AVRCP, MSG_BTSRV_AVRCP_GET_ID3_INFO, (void *)NULL);
}

int btif_avrcp_set_absolute_volume(uint8_t *data, uint8_t len)
{
	uint32_t param, i;

	if (len < 1 || len > 3) {
		return -EINVAL;
	}

	param = (len << 24);
	for (i = 0; i < len; i++) {
		param |= (data[i] << (8*i));
	}

	return btsrv_function_call(MSG_BTSRV_AVRCP, MSG_BTSRV_AVRCP_SET_ABSOLUTE_VOLUME, (void *)param);
}

int btif_avrcp_notify_volume_change(uint8_t volume)
{
	return btsrv_function_call(MSG_BTSRV_AVRCP, MSG_BTSRV_AVRCP_NOTIFY_VOLUME_CHANGE, (void *)((uint32_t)volume));
}

int btif_avrcp_get_play_status(struct bt_conn *conn)
{
	return btsrv_function_call(MSG_BTSRV_AVRCP, MSG_BTSRV_AVRCP_GET_PLAY_STATUS, (void *)conn);
}

/*return 0:pause, 1:play*/
int btif_avrcp_get_avrcp_status(uint16_t hdl)
{
	struct bt_conn * conn = btsrv_rdm_find_conn_by_hdl(hdl);
	if (!conn)
		return 0;

	return btsrv_rdm_get_avrcp_state(conn);
}

int btif_avrcp_get_support_feature(uint16_t hdl)
{
	return btsrv_function_call(MSG_BTSRV_AVRCP, MSG_BTSRV_AVRCP_GET_SUPPORT_FEATURE,  (void *)((uint32_t)hdl));
}

