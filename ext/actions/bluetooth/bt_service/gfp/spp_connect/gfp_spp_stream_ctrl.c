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

#include "gfp_spp_stream_ctrl.h"

/* SPP  */
static const uint8_t gfp_spp_uuid[16] = {0x7c,0x92,0x67,0x4d,0x2c,0xf1,0x86,0x88,
							0xdb,0x4f,0x15,0x25,0x2c,0xfe,0x21,0xdf};

static void gfp_spp_connected_cb(uint8_t channel, uint8_t *uuid)
{
    if(!btsrv_gfp){
        SYS_LOG_ERR("GFP INVAL!");
		return;
    }
    
	SYS_LOG_INF("channel:%d\n", channel);

    btsrv_gfp->gfp_spp_chl = channel;

    btsrv_gfp->gfp_spp_connected = 1;

    os_delayed_work_submit(&btsrv_gfp->spp_initial_provider_work, 0);
}

static void gfp_spp_disconnected_cb(uint8_t channel)
{
    if(!btsrv_gfp){
        SYS_LOG_ERR("GFP INVAL!");
		return;
    }

	btsrv_gfp->gfp_spp_chl = 0;
	btsrv_gfp->gfp_spp_connected = 0;

    SYS_LOG_INF("\n");    
}

static void gfp_spp_receive_data_cb(uint8_t channel, uint8_t *data, uint32_t len)
{
	if (btsrv_gfp && btsrv_gfp->gfp_spp_connected) {	    
		rfcomm_message_stream_deal(data,len);
	}
}

static const struct btmgr_spp_cb gfp_spp_cb = {
	.connected = gfp_spp_connected_cb,
	.disconnected = gfp_spp_disconnected_cb,
	.receive_data = gfp_spp_receive_data_cb,
};

int gfp_spp_send_data(u8_t	*data_ptr, u16_t length)
{
	if ((!length) || (!data_ptr)) {
		return -EINVAL;
	}

	if (!btsrv_gfp->gfp_spp_chl) {
			return -EIO;
	}

	print_hex_comm("ms:",data_ptr,16);

    return bt_manager_spp_send_data(btsrv_gfp->gfp_spp_chl,data_ptr, length);
}

static void initial_provider_work_callback(struct k_work *work)
{
    if(btsrv_gfp->gfp_spp_connected){
        fastpair_initial_provider_info();
    }
}

void gfp_spp_stream_init(void)
{
    int ret;
    ret = bt_manager_spp_reg_uuid((uint8_t *)gfp_spp_uuid, (struct btmgr_spp_cb *)&gfp_spp_cb);

    if (ret < 0) {
        SYS_LOG_ERR("Failed register spp uuid");
        return;
    }

    btsrv_gfp->gfp_spp_connected = 0;
    btsrv_gfp->gfp_spp_chl = 0;

	os_delayed_work_init(&btsrv_gfp->spp_initial_provider_work, initial_provider_work_callback);

	return;
}

