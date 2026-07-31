/*
 * Copyright (c) 2016 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt service tws snoop
 */

#define SYS_LOG_DOMAIN "tws_snoop"
#include "btsrv_os_common.h"
#include "btsrv_inner.h"

uint8_t btsrv_adapter_tws_check_role(bt_addr_t *addr, uint8_t *name)
{
	int i;
	uint8_t role = BTSRV_TWS_NONE;
	struct autoconn_info info[BTSRV_SAVE_AUTOCONN_NUM];

	memset(info, 0, sizeof(info));
	btsrv_connect_get_auto_reconnect_info(info, BTSRV_SAVE_AUTOCONN_NUM);
	for (i = 0; i < BTSRV_SAVE_AUTOCONN_NUM; i++) {
		if (info[i].addr_valid && !memcmp(&info[i].addr, addr, sizeof(bd_address_t))) {
			role = (info[i].tws_role == BTSRV_TWS_NONE) ? BTSRV_TWS_NONE : BTSRV_TWS_PENDING;
			break;
		}
	}

    if ((i == BTSRV_SAVE_AUTOCONN_NUM) || (role == BTSRV_TWS_NONE)){
        role = btsrv_tws_check_is_tws_dev_connect(addr,name) ? BTSRV_TWS_PENDING : BTSRV_TWS_NONE;
    }

	return role;
}

void btsrv_tws_sync_info_type(struct bt_conn *conn, uint8_t type, uint8_t cb_ev)
{
	if (btsrv_rdm_get_snoop_role(conn) != BTSRV_SNOOP_MASTER) {
		return;
	}

	switch (type) {
	case BTSRV_SYNC_TYPE_A2DP:
		btsrv_tws_sync_a2dp_info_to_remote(conn, cb_ev);
		break;
	case BTSRV_SYNC_TYPE_AVRCP:
		btsrv_tws_sync_avrcp_info_to_remote(conn, cb_ev);
		break;
	case BTSRV_SYNC_TYPE_HFP:
		btsrv_tws_sync_hfp_info_to_remote(conn, cb_ev);
		break;
	case BTSRV_SYNC_TYPE_SPP:
		btsrv_tws_sync_spp_info_to_remote(conn, cb_ev);
		break;
	default:
		break;
	}
}

void btsrv_tws_sync_hfp_cbev_data(struct bt_conn *conn, uint8_t cb_ev, uint8_t *data, uint8_t len)
{
	if (btsrv_rdm_get_snoop_role(conn) != BTSRV_SNOOP_MASTER) {
		return;
	}

	btsrv_tws_sync_hfp_cbev_data_to_remote(conn, cb_ev, data, len);
}
