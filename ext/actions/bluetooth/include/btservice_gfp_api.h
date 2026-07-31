/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file bt service interface
 */

#ifndef _BTSERVICE_GFP_API_H_
#define _BTSERVICE_GFP_API_H_
#include <stream.h>
#include <btservice_base.h>
#include <gfp/sys_comm.h>
#include <thread_timer.h>
#include <btservice_api.h>

struct btsrv_gfp_context_info {
    btsrv_gfp_callback gfp_ev_callback;
    loop_buffer_t  tx_loop_buf;
    struct thread_timer auth_timer;
    struct thread_timer running_timer;
    os_delayed_work auth_work;
    os_delayed_work spp_initial_provider_work;
    os_delayed_work ms_initial_provider_work;
    io_stream_t gfp_ble_stream;
    void *gfp_handle;
    uint8_t gfp_gatt_state;
    uint8_t gfp_spp_chl;
    struct bt_conn *fpms_conn;
    struct bt_conn *gfp_conn;
    uint8_t ble_stream_opened :1;
    uint8_t gfp_spp_connected :1;
    uint8_t gfp_ble_l2cap_connected :1;
    uint8_t gfp_running_timer_init :1;
    uint8_t auth_work_running:1;
    int8_t fixed_chan;

};

typedef int (*btsrv_gfp_pairing_request_callback)(uint8_t* , uint32_t);
typedef void (*btsrv_gfp_auth_timeout_start)(uint32_t);
typedef void (*btsrv_gfp_auth_timeout_stop)(void);
typedef void (*btsrv_gfp_fixed_chan_forbid)(void);


void gfp_running_timer_handler(struct thread_timer *timer, void* pdata);
void gfp_ble_stream_init(void);
void gfp_spp_stream_init(void);
int gfp_spp_send_data(u8_t	*data_ptr, u16_t length);
int gfp_fpms_service_init(void);
int gfp_fpms_send_data(u8_t *data_ptr, u16_t length);
int gfp_message_stream_send_data(u8_t	*data_ptr, u16_t length);
int rfcomm_message_stream_deal(uint8_t* data, uint16_t size);

void fastpair_initial_provider_info(void);
void btsrv_gfp_pairing_request_reg(btsrv_gfp_pairing_request_callback callback);
void btsrv_gfp_cap_io_set(bool enable);
void btsrv_gfp_confirm_pairing_reply(bool success,bool ble_devices);
void btsrv_gfp_timeout_start_cb(btsrv_gfp_auth_timeout_start cb);
void btsrv_gfp_timeout_stop_cb(btsrv_gfp_auth_timeout_stop cb);
void btsrv_gfp_fixed_chan_forbid_cb(btsrv_gfp_fixed_chan_forbid cb);

void btsrv_gfp_le_cap_io_set(bool enable);
extern struct btsrv_gfp_context_info *btsrv_gfp;

#endif
