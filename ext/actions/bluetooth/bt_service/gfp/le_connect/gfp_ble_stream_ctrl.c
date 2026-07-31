/*
 * Copyright (c) 2019 Actions Semi Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file
 * @brief
 *
 * Change log:
 *
 */

#define SYS_LOG_DOMAIN "btsrv_gfp"

#include <os_common_api.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <acts_bluetooth/host_interface.h>
#include <bt_manager.h>
#include <mem_manager.h>
#include <fastpair_act.h>
#include <helper.h>
#include <btservice_gfp_api.h>

#include "gfp_ble_stream.h"
#include "gfp_ble_stream_ctrl.h"


#define GFP_TX_CMD_BUF_SIZE	(256)

static OS_MUTEX_DEFINE(gfp_tx_mutex);

typedef struct{
    u8_t recv_cmd_type;
    characteristic_write_callback_t cmd_cbk;
} gfp_recv_cmd_list_s;

/* BLE */
#define GFP_BLE_PAIR_SERVICE BT_UUID_DECLARE_16(0xFE2C)
#define GFP_BLE_FIRMWARE_REVISION_CHARACTERISTIC BT_UUID_DECLARE_16(0x2A26)

#define GFP_BLE_MODEL_ID BT_UUID_DECLARE_128(0xea, 0x0b, 0x10, 0x32, \
	0xde, 0x01, 0xb0, 0x8e, 0x14, 0x48, 0x66, 0x83, 0x33, 0x12, 0x2c, 0xfe)

#define GFP_BLE_KEY_PAIRING BT_UUID_DECLARE_128(0xea, 0x0b, 0x10, 0x32, \
	0xde, 0x01, 0xb0, 0x8e, 0x14, 0x48, 0x66, 0x83, 0x34, 0x12, 0x2c, 0xfe)

#define GFP_BLE_PASS_KEY BT_UUID_DECLARE_128(0xea, 0x0b, 0x10, 0x32, \
	0xde, 0x01, 0xb0, 0x8e, 0x14, 0x48, 0x66, 0x83, 0x35, 0x12, 0x2c, 0xfe)

#define GFP_BLE_ACCOUNT_KEY BT_UUID_DECLARE_128(0xea, 0x0b, 0x10, 0x32, \
	0xde, 0x01, 0xb0, 0x8e, 0x14, 0x48, 0x66, 0x83, 0x36, 0x12, 0x2c, 0xfe)

#define GFP_BLE_ADDITIONAL_DATA BT_UUID_DECLARE_128(0xea, 0x0b, 0x10, 0x32, \
	0xde, 0x01, 0xb0, 0x8e, 0x14, 0x48, 0x66, 0x83, 0x37, 0x12, 0x2c, 0xfe)

// FE2C1239-8366-4814-8EB0-01DE32100BEA (read, encrypted)
#define GFP_BLE_MESSAGE_STREAM_PSM BT_UUID_DECLARE_128(0xea, 0x0b, 0x10, 0x32, \
	0xde, 0x01, 0xb0, 0x8e, 0x14, 0x48, 0x66, 0x83, 0x39, 0x12, 0x2c, 0xfe)

// FE2C123A-8366-4814-8EB0-01DE32100BEA (read,writte,notify encrypted)
#define GFP_BLE_ADDITIONAL_PASSKEY BT_UUID_DECLARE_128(0xea, 0x0b, 0x10, 0x32, \
	0xde, 0x01, 0xb0, 0x8e, 0x14, 0x48, 0x66, 0x83, 0x3a, 0x12, 0x2c, 0xfe)


static struct bt_gatt_attr	gfp_gatt_attr[] = {
	BT_GATT_PRIMARY_SERVICE(GFP_BLE_PAIR_SERVICE),
	BT_GATT_CHARACTERISTIC(GFP_BLE_KEY_PAIRING, BT_GATT_CHRC_WRITE | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, NULL, NULL, NULL),
	BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
	
	BT_GATT_CHARACTERISTIC(GFP_BLE_PASS_KEY, BT_GATT_CHRC_WRITE | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, NULL, NULL, NULL),
	BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

	BT_GATT_CHARACTERISTIC(GFP_BLE_ACCOUNT_KEY, BT_GATT_CHRC_WRITE, BT_GATT_PERM_WRITE, NULL, NULL, NULL),

	BT_GATT_CHARACTERISTIC(GFP_BLE_ADDITIONAL_DATA, BT_GATT_CHRC_WRITE | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, NULL, NULL, NULL),
	BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

	BT_GATT_CHARACTERISTIC(GFP_BLE_ADDITIONAL_PASSKEY, BT_GATT_CHRC_WRITE | BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, NULL, NULL, NULL),
	BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

	BT_GATT_CHARACTERISTIC(GFP_BLE_MESSAGE_STREAM_PSM, BT_GATT_CHRC_READ , BT_GATT_PERM_READ , NULL, NULL, NULL),
};

const gfp_recv_cmd_list_s gfp_recv_cmd_list[] = {
    {BLE_KEY_PAIRING, (characteristic_write_callback_t)perform_key_based_pairing},
    {BLE_PASS_KEY, (characteristic_write_callback_t)passkey_written},
    {BLE_ACCOUNT_KEY, (characteristic_write_callback_t)account_key_written},
    {BLE_ADDITIONAL_DATA, (characteristic_write_callback_t)additional_data_written},
    {BLE_ADDITIONAL_PASSKEY, (characteristic_write_callback_t)additional_passkey_written},
};

static void gfp_recv_proc(u8_t *buf, u16_t len)
{
    void *attr;
    u16_t payload_len;
    u8_t num;
    int i;
    
    memcpy(&attr, buf, sizeof(u32_t));
    memcpy(&payload_len, buf+sizeof(u32_t), sizeof(u16_t));
    
    num = gfp_uuid_num_get(attr);

    for (i = 0 ; i < sizeof(gfp_recv_cmd_list) / sizeof(gfp_recv_cmd_list_s); i++) {
        if (gfp_recv_cmd_list[i].recv_cmd_type  == num) {
            SYS_LOG_INF(" gfp cmd %x start!!", num);
            if(btsrv_gfp){
                btsrv_gfp->gfp_gatt_state = num;
                if(num == BLE_KEY_PAIRING){
                    if(payload_len == AES_BLOCK_SIZE + ECDH_PUBLIC_KEY_SIZE){
                        if(btsrv_gfp->gfp_ev_callback){
                            btsrv_gfp->gfp_ev_callback(0, BTSRV_GFP_INITIAL_PAIRING, NULL, 0);
                        }
                    }
                }
            }
            gfp_recv_cmd_list[i].cmd_cbk(buf+sizeof(u32_t)+sizeof(u16_t), payload_len);
            break;
        }
    }

    return;
}

static void gfp_rx_process(void)
{
	u16_t len, total_len = 0;
	u16_t payload_len = 0;
	int timeout_count = 0;
	u8_t read_buf[128];
	
	memset(read_buf, 0, 128);
	len = stream_read(btsrv_gfp->gfp_ble_stream, read_buf, sizeof(u32_t));
	if (len <= 0) {
		return;
	}
	SYS_LOG_INF("gfp attr: %p", (void *)&read_buf[0]);
	len = stream_read(btsrv_gfp->gfp_ble_stream, (u8_t *)&payload_len, sizeof(u16_t));
	if (len <= 0) {
		SYS_LOG_ERR("");
		return;
	}
	SYS_LOG_INF("gfp payload_len: %d", payload_len);
	if (payload_len + sizeof(u32_t) + sizeof(u16_t) > 128)
		return;

	while (stream_get_length(btsrv_gfp->gfp_ble_stream) < payload_len) {
		if (timeout_count++ > 100) {
			break;
		}

		os_sleep(1);
	}

	memcpy(read_buf + sizeof(u32_t), &payload_len, sizeof(u16_t));
	stream_read(btsrv_gfp->gfp_ble_stream, read_buf + sizeof(u32_t) + sizeof(u16_t), payload_len);
	total_len = payload_len + sizeof(u32_t) + sizeof(u16_t);
	print_hex_comm("data:", read_buf, total_len);
	gfp_recv_proc(read_buf, total_len);

}

int gfp_send_pkg_to_stream(u8_t	*data_ptr, u16_t length, int num)
{
	int ret = 0;
	struct btsrv_gfp_context_info *p;
	u8_t number = (u8_t)num;

	if ((!length) || (length > 256) || (!data_ptr)) {
		return 0;
	}

	//SYS_LOG_INF("cmd(%d), %d", cmd, length);
	//print_hex_comm("d2:",data_ptr,16);

	os_mutex_lock(&gfp_tx_mutex, OS_FOREVER);
	p = btsrv_gfp;
	if (!p || !p->ble_stream_opened ) {
		goto exit_send_pkt;
	}
	
	if (num)
	{
		if (!p->tx_loop_buf.data_buf)
		{
			p->tx_loop_buf.buf_size = GFP_TX_CMD_BUF_SIZE;
			p->tx_loop_buf.data_buf = (u8_t *)app_mem_malloc(p->tx_loop_buf.buf_size);
			if (!p->tx_loop_buf.data_buf) goto exit_send_pkt;
		}

		if ((sizeof(u16_t) + sizeof(u8_t) + length) <= (p->tx_loop_buf.buf_size - p->tx_loop_buf.data_count))
		{
			if (sizeof(u8_t) != loop_buffer_write(&p->tx_loop_buf, &number, sizeof(u8_t)))
				SYS_LOG_ERR("");

			if (sizeof(u16_t) != loop_buffer_write(&p->tx_loop_buf, &length, sizeof(u16_t)))
				SYS_LOG_ERR("");
			
			if(length != loop_buffer_write(&p->tx_loop_buf, data_ptr, length))
				SYS_LOG_ERR("");
		}
		else
		{
			SYS_LOG_ERR("0x%x 0x%x.",
				length,(p->tx_loop_buf.buf_size - p->tx_loop_buf.data_count));
			goto exit_send_pkt;
		}
	}

exit_send_pkt:
	os_mutex_unlock(&gfp_tx_mutex);
	SYS_LOG_INF("exit");

	return ret;
}

static void gfp_tx_process(void)
{
	int ret;
	u16_t get_len = 0;
	u8_t tptr[160];
	struct btsrv_gfp_context_info *p;
	u8_t num;

	os_mutex_lock(&gfp_tx_mutex, OS_FOREVER);
	p = btsrv_gfp;
	if (!p || !p->ble_stream_opened) {
		os_mutex_unlock(&gfp_tx_mutex);
		return;
	}

	if (p->tx_loop_buf.data_count) {

		loop_buffer_read(&p->tx_loop_buf, &num, sizeof(u8_t));
		SYS_LOG_INF("num %d",num);
		loop_buffer_read(&p->tx_loop_buf, &get_len, sizeof(u16_t));
		loop_buffer_read(&p->tx_loop_buf, tptr, get_len);

		os_mutex_unlock(&gfp_tx_mutex);

		gfp_tx_attr_set((void *)gfp_uuid_tx_chrc_attr_get(num), (void *)gfp_uuid_tx_attr_get(num));
		ret = stream_write(btsrv_gfp->gfp_ble_stream, &tptr[0], get_len);
       
		if (ret > 0) {
			SYS_LOG_INF("GFP CMD TX: %d, %d\n", get_len, ret);
		} else {
			SYS_LOG_ERR("CMD ERR TX: %d, %d\n", get_len, ret);
		}
		return;
	}	

	os_mutex_unlock(&gfp_tx_mutex);
}

static void gfp_ble_disconnected(void)
{
	int ret;
    int remain_time = 0;

    btif_br_set_gfp_scan_mode(false);

	if (btsrv_gfp->gfp_ble_stream){
		ret = stream_close(btsrv_gfp->gfp_ble_stream);
		if (ret) {
			SYS_LOG_ERR("stream_close Failed");
		} else {
            if((remain_time = os_delayed_work_remaining_get(&btsrv_gfp->auth_work))){
                fastpair_timeout_handle(NULL);
            }

			gfp_auth_timeout_stop();
			os_mutex_lock(&gfp_tx_mutex, OS_FOREVER);

			if(btsrv_gfp->tx_loop_buf.data_buf)
			{
				app_mem_free(btsrv_gfp->tx_loop_buf.data_buf);
				memset(&btsrv_gfp->tx_loop_buf, 0 , sizeof(btsrv_gfp->tx_loop_buf));
			}

			os_mutex_unlock(&gfp_tx_mutex);
		    btsrv_gfp->ble_stream_opened = 0;
            btsrv_gfp->gfp_conn = NULL;
		}
	}
}

static void gfp_ble_connect(BOOL connected, void *param)
{
	SYS_LOG_INF("%s %p", (connected) ? "connected" : "disconnected",param);
	if (connected) {
		if (stream_open(btsrv_gfp->gfp_ble_stream, MODE_IN_OUT)) {		    
			SYS_LOG_ERR("stream_open Failed");
			return;
		}
        btsrv_gfp->ble_stream_opened = 1;
        btsrv_gfp->gfp_conn = (struct bt_conn *)param;
        btsrv_gfp->gfp_gatt_state = 0;
        if(btsrv_gfp->gfp_ev_callback){
            btsrv_gfp->gfp_ev_callback(0, BTSRV_GFP_BLE_CONNECTED, NULL, 0);
        }

	} else {
		SYS_LOG_INF("ble_stream_opened %d.",btsrv_gfp->ble_stream_opened);
		if (!btsrv_gfp->ble_stream_opened){
			return;
		}
        gfp_ble_disconnected();
        if(btsrv_gfp->gfp_ev_callback){
            btsrv_gfp->gfp_ev_callback(0, BTSRV_GFP_BLE_DISCONNECTED, NULL, 0);
        }
	}
}

static bool gfp_auth_status(void)
{
    return btsrv_gfp->auth_work_running;
}

uint8_t gfp_ble_key_pairing(void)
{
    if(btsrv_gfp){
        return btsrv_gfp->gfp_gatt_state;
    }
    else{
        return 0;
    }
}

void gfp_running_timer_handler(struct thread_timer *timer, void* pdata)
{
	if (btsrv_gfp->gfp_ble_stream && btsrv_gfp->ble_stream_opened) {
		gfp_rx_process();

		gfp_tx_process();
	}
}

void gfp_fixed_chan_forbid(void)
{
    if(hostif_bt_le_is_cis_capable(btsrv_gfp->gfp_conn) &&
        hostif_bt_le_is_bis_capable(btsrv_gfp->gfp_conn)){  // only pixel 6 Android 12 need forbid
        return;
    }

    hostif_bt_l2cap_br_fixed_chan_enable(0);
    btsrv_gfp->fixed_chan = 0;
}

void gfp_fixed_chan_enable(void)
{
    hostif_bt_l2cap_br_fixed_chan_enable(1);
    btsrv_gfp->fixed_chan = 1;
}

/* Start after bt engine ready */
void gfp_ble_stream_init(void)
{
	struct gfp_ble_stream_init_param init_param;

    if(!btsrv_gfp){
        return;
    }

	os_delayed_work_init(&btsrv_gfp->auth_work, fastpair_timeout_handle);

	SYS_LOG_INF("%s %d.",__func__,__LINE__);

	init_param.gatt_attr = &gfp_gatt_attr[0];
	init_param.attr_size = ARRAY_SIZE(gfp_gatt_attr);
	init_param.keypairing_write_attr = &gfp_gatt_attr[2];
	init_param.passkey_write_attr = &gfp_gatt_attr[5];
	init_param.accountkey_write_attr = &gfp_gatt_attr[8];
	init_param.addtional_data_write_attr = &gfp_gatt_attr[10];
	init_param.addtional_passkey_write_attr = &gfp_gatt_attr[13];
    init_param.message_stream_psm_attr = &gfp_gatt_attr[16];

	init_param.keypairing_ccc_attr = &gfp_gatt_attr[3];
	init_param.passkey_ccc_attr = &gfp_gatt_attr[6];
	init_param.addtional_data_ccc_attr = &gfp_gatt_attr[11];
	init_param.addtional_passkey_ccc_attr = &gfp_gatt_attr[14];

	init_param.connect_cb = gfp_ble_connect;
    init_param.auth_status = gfp_auth_status;

	/* K_FOREVER, K_NO_WAIT,  K_MSEC(ms) */
	init_param.read_timeout = OS_NO_WAIT;
	init_param.write_timeout = OS_NO_WAIT;

	/* Just call stream_create once, for register spp/ble service
	 * not need call stream_destroy
	 */
	btsrv_gfp->gfp_ble_stream = gfp_ble_stream_create(&init_param);
	if (!btsrv_gfp->gfp_ble_stream) {
		SYS_LOG_ERR("stream_create failed");
	}

	btsrv_gfp_timeout_start_cb(gfp_auth_timeout_start);
	btsrv_gfp_timeout_stop_cb(gfp_auth_timeout_stop);
    btsrv_gfp_fixed_chan_forbid_cb(gfp_fixed_chan_forbid);

    btsrv_gfp->gfp_running_timer_init = 0;

    if(bt_get_fixed_chan_default_status() >= 0){
        btsrv_gfp->fixed_chan = bt_get_fixed_chan_default_status();
    }
    else{
        btsrv_gfp->fixed_chan = 0;
    }
}

void gfp_auth_timeout_start(u32_t duration)
{	
    SYS_LOG_INF("auth timer creater");
    os_delayed_work_submit(&btsrv_gfp->auth_work, duration);
    btsrv_gfp->auth_work_running = 1;
}

void gfp_auth_timeout_stop(void)
{	
    SYS_LOG_INF("auth timer destory");
	os_delayed_work_cancel(&btsrv_gfp->auth_work);
    btsrv_gfp->auth_work_running = 0;
    btsrv_gfp->gfp_gatt_state = 0;
    if(bt_get_fixed_chan_default_status() >= 0){
        if(btsrv_gfp->fixed_chan != bt_get_fixed_chan_default_status()){
            btsrv_gfp->fixed_chan = bt_get_fixed_chan_default_status();
            if(btsrv_gfp->fixed_chan){
                hostif_bt_l2cap_br_fixed_chan_enable(1);
            }
            else{
                hostif_bt_l2cap_br_fixed_chan_enable(0);
            }
        }
    }
}

u8_t gfp_uuid_num_get(void *attr)
{
	if ((&gfp_gatt_attr[1] == attr) || (&gfp_gatt_attr[2] == attr))
	{
		return BLE_KEY_PAIRING;
	}
	else if ((&gfp_gatt_attr[4] == attr) || (&gfp_gatt_attr[5] == attr))
	{
		return BLE_PASS_KEY;
	} 
	else if ((&gfp_gatt_attr[7] == attr) || (&gfp_gatt_attr[8] == attr))
	{
		return BLE_ACCOUNT_KEY;
	}
	else if ((&gfp_gatt_attr[9] == attr) || (&gfp_gatt_attr[10] == attr))
	{
		return BLE_ADDITIONAL_DATA;
	}

	else if ((&gfp_gatt_attr[12] == attr) || (&gfp_gatt_attr[13] == attr))
	{
		return BLE_ADDITIONAL_PASSKEY;
	}

	return BLE_FP_CHARACTERISTICS_NUM;
}

int gfp_uuid_tx_chrc_attr_get(u8_t num)
{
	if (BLE_KEY_PAIRING == num)
	{
		return (int)&gfp_gatt_attr[1];
	}
	else if (BLE_PASS_KEY == num)
	{
		return (int)&gfp_gatt_attr[4];
	} 
	else if (BLE_ADDITIONAL_DATA == num)
	{
		return (int)&gfp_gatt_attr[9];
	} 
	else if (BLE_ADDITIONAL_PASSKEY == num)
	{
		return (int)&gfp_gatt_attr[12];
	}

	return 0;
}

int gfp_uuid_tx_attr_get(u8_t num)
{
	if (BLE_KEY_PAIRING == num)
	{
		return (int)&gfp_gatt_attr[2];
	}
	else if (BLE_PASS_KEY == num)
	{
		return (int)&gfp_gatt_attr[5];
	} 
	else if (BLE_ADDITIONAL_DATA == num)
	{
		return (int)&gfp_gatt_attr[10];
	} 
	else if (BLE_ADDITIONAL_PASSKEY == num)
	{
		return (int)&gfp_gatt_attr[13];
	}

	return 0;
}

