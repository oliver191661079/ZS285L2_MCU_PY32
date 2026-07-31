/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt manager.
 */
#define SYS_LOG_NO_NEWLINE
#define SYS_LOG_DOMAIN "btsrv_gfp"

#include <os_common_api.h>

#include <os_common_api.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <mem_manager.h>
#include <btservice_gfp_api.h>
#include <fastpair_act.h>
#include <helper.h>
#include <acts_bluetooth/host_interface.h>
#include "gfp_ble_l2cap_ctrl.h"

static void gfp_fpms_connected_cb(struct bt_conn * conn)
{
    if(!btsrv_gfp){
        SYS_LOG_ERR("GFP INVAL!");
		return;
    }
    
	SYS_LOG_INF("fpms conn:0x%p\n", conn);

    btsrv_gfp->fpms_conn = conn;

    btsrv_gfp->gfp_ble_l2cap_connected = 1;

    os_delayed_work_submit(&btsrv_gfp->ms_initial_provider_work, 0);

}

static void gfp_fpms_disconnected_cb(struct bt_conn * conn)
{
    if(!btsrv_gfp){
        SYS_LOG_ERR("GFP INVAL!");
		return;
    }

	btsrv_gfp->fpms_conn = NULL;
	btsrv_gfp->gfp_ble_l2cap_connected = 0;

    SYS_LOG_INF("\n");    
}

static void gfp_fpms_receive_data_cb(struct bt_conn * conn, uint8_t *data, uint16_t len)
{
	if (btsrv_gfp && (btsrv_gfp->fpms_conn == conn)) {	    
		rfcomm_message_stream_deal(data,len);      
	}
}

static const struct bt_fpms_app_cb gfp_fpms_cb = {
	.connected = gfp_fpms_connected_cb,
	.disconnected = gfp_fpms_disconnected_cb,
	.recv = gfp_fpms_receive_data_cb,
};

int gfp_fpms_send_data(u8_t *data_ptr, u16_t length)
{
    if ((!length) || (!data_ptr)) {
        return -EINVAL;
    }

    if (!btsrv_gfp->fpms_conn) {
        return -EIO;
    }

	print_hex_comm("fpms tx:",data_ptr,16);

    return hostif_bt_fpms_send_data(btsrv_gfp->fpms_conn,data_ptr, length);
}

static void ms_initial_provider_work_callback(struct k_work *work)
{
	printk("ms_initial_provider_work_callback %d\n",btsrv_gfp->gfp_ble_l2cap_connected);

    if(btsrv_gfp->gfp_ble_l2cap_connected){
        fastpair_initial_provider_info();
    }
}

int gfp_fpms_service_init(void)
{

    int ret;
    
    ret = hostif_bt_fpms_register_cb((struct bt_fpms_app_cb *)&gfp_fpms_cb);

    if (ret < 0) {
        SYS_LOG_ERR("Failed register fpms!");
        return -EIO;
    }

    btsrv_gfp->gfp_ble_l2cap_connected = 0;
    btsrv_gfp->fpms_conn = NULL;

	os_delayed_work_init(&btsrv_gfp->ms_initial_provider_work, ms_initial_provider_work_callback);

	return 0;
}

