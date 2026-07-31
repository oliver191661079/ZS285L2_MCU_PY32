/*
 * Copyright (c) 2017 Actions Semi Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief btsrvice did
 */

#define SYS_LOG_DOMAIN "btsrv_gfp"
#include "btsrv_os_common.h"
#include "btsrv_inner.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef CONFIG_BLUETOOTH
#include <acts_bluetooth/host_interface.h>
#include <bt_manager.h>
#endif

#include <mem_manager.h>

#include <fastpair_act.h>
#include <helper.h>
#include <btservice_gfp_api.h>

static struct btsrv_gfp_context_info gfp_context;
struct btsrv_gfp_context_info *btsrv_gfp;

#define GFP_ROUTINE_INTERVAL   (10)

#ifndef NEARBY_FP_BLE_DEVICES
#define NEARBY_FP_BLE_DEVICES 1
#endif /* NEARBY_FP_BLE_DEVICES */

void gfp_routine_start(void)
{
	if (btsrv_gfp) {
        if(!btsrv_gfp->gfp_running_timer_init){
            thread_timer_init(&(btsrv_gfp->running_timer), gfp_running_timer_handler, NULL);
            btsrv_gfp->gfp_running_timer_init = 1;
        }
        thread_timer_start(&btsrv_gfp->running_timer, GFP_ROUTINE_INTERVAL, GFP_ROUTINE_INTERVAL);
	}
}

void gfp_routine_stop(void)
{
	if (btsrv_gfp) {
        thread_timer_stop(&btsrv_gfp->running_timer);    
	}
}

int gfp_message_stream_send_data(u8_t *data_ptr, u16_t length)
{
    if(btsrv_gfp->gfp_ble_l2cap_connected){
        return gfp_fpms_send_data(data_ptr,length);
    }
    else{
        return gfp_spp_send_data(data_ptr,length);
    }
}

int btsrv_gfp_init(struct btsrv_gfp_start_param *param)
{
    gfp_context.gfp_ev_callback = param->cb;    
    btsrv_gfp = &gfp_context;

	btsrv_gfp->gfp_handle = fastpair_init(param->module_id,param->private_anti_spoofing_key);

	if(!btsrv_gfp->gfp_handle){
        SYS_LOG_ERR("EIO");
        return -EIO;
	}
	
    gfp_ble_stream_init();

	gfp_spp_stream_init();

#ifdef NEARBY_FP_BLE_DEVICES
    gfp_fpms_service_init();
#endif

	return 0;
}

int btsrv_gfp_process(struct app_msg *msg)
{
	switch (_btsrv_get_msg_param_cmd(msg)) {
	case MSG_BTSRV_GFP_START:
		SYS_LOG_INF("MSG_BTSRV_GFP_START\n");
		btsrv_gfp_init(msg->ptr);
		break;
	default:
		break;
	}

	return 0;

}

