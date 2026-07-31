/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt pawr manager.
 */
#define SYS_LOG_DOMAIN "btmgr_twsble"

#include <os_common_api.h>

#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <msg_manager.h>
#include <mem_manager.h>
#include <acts_bluetooth/host_interface.h>
#include <bt_manager.h>
#include "bt_manager_inner.h"
#include <sys_event.h>
#include <hex_str.h>
#include <acts_ringbuf.h>
#include <property_manager.h>

#ifdef CONFIG_BT_ADV_MANAGER
#define BLE_ADV_DATA_MAX_LEN			(31)
#define BLE_ADV_MAX_ARRAY_SIZE			(4)
#endif

typedef struct
{
    uint8_t len;
    uint8_t type;
    uint8_t data[29];
}adv_data_t;

struct leatws_info {
#ifdef CONFIG_BT_ADV_MANAGER
	uint8_t ad_data[BLE_ADV_DATA_MAX_LEN];
	uint8_t sd_data[BLE_ADV_DATA_MAX_LEN];
#endif
};

static struct leatws_info leatws_ctx;
signed int print_hex(const char* prefix, const uint8_t* data, int size);

#if defined(CONFIG_BT_LEATWS_FL)
static uint32_t sink_location = BT_AUDIO_LOCATIONS_FL;
#elif defined(CONFIG_BT_LEATWS_FR)
static uint32_t sink_location = BT_AUDIO_LOCATIONS_FR;
#endif

void leatws_set_audio_channel(uint32_t audio_channel)
{
	sink_location = audio_channel;
}

uint32_t leatws_get_audio_channel(void)
{
	return sink_location;
}

void leatws_notify_leaudio_reinit(void)
{
	struct app_msg msg = { 0 };

	msg.type = MSG_LEAUDIO_REINIT;
	send_async_msg(CONFIG_SYS_APP_NAME, &msg);
}

static uint8_t _leatws_format_adv_data(struct bt_data *ad, uint8_t *ad_data)
{
	uint8_t ad_len = 0;
	uint8_t i, pos = 0;

	for (i = 0; i < BLE_ADV_MAX_ARRAY_SIZE; i++) {
		if (ad_data[pos] == 0) {
			break;
		}

		if ((pos + ad_data[pos] + 1) > BLE_ADV_DATA_MAX_LEN) {
			break;
		}

		ad[ad_len].data_len = ad_data[pos] - 1;
		ad[ad_len].type = ad_data[pos + 1];
		ad[ad_len].data = &ad_data[pos + 2];
		ad_len++;

		pos += (ad_data[pos] + 1);
	}

	return ad_len;
}

static void _leatws_adv_start(void)
{
	struct bt_le_adv_param param;
	struct bt_data ad[BLE_ADV_MAX_ARRAY_SIZE], sd[BLE_ADV_MAX_ARRAY_SIZE];
	size_t ad_len, sd_len;
	int err;

	ad_len = _leatws_format_adv_data((struct bt_data *)ad, leatws_ctx.ad_data);
	sd_len = _leatws_format_adv_data((struct bt_data *)sd, leatws_ctx.sd_data);

	memset(&param, 0, sizeof(param));
	param.id = BT_ID_DEFAULT;
	param.interval_min = BT_GAP_ADV_FAST_INT_MIN_2;
	param.interval_max = BT_GAP_ADV_FAST_INT_MAX_2;
	param.options = (BT_LE_ADV_OPT_CONNECTABLE/* | BT_LE_ADV_OPT_USE_NAME*/);

	err = hostif_bt_le_adv_start(&param, (ad_len ? (const struct bt_data *)ad : NULL), ad_len,
								(sd_len ? (const struct bt_data *)sd : NULL), sd_len);
	if (err < 0 && err != (-EALREADY)) {
		SYS_LOG_ERR("Failed to start advertising (err %d)", err);
	} else {
		SYS_LOG_INF("Advertising started");
	}
}

static uint8_t _leatws_set_adv_data(adv_data_t *adv_data)
{
    uint8_t ad_buf[31], sd_buf[31];
    int ad_len, sd_len,i;
    adv_data_t *t;
    uint8_t ret = 0;

    ad_len = sd_len = 0;

    for(i = 0; i < 4;i++)
    {
        t = &adv_data[i];
        if (t->len > 0 && 0 == sd_len &&
            ad_len + t->len + 1 <= sizeof(ad_buf) )
        {
        	memcpy(&ad_buf[ad_len], t, t->len + 1);
			    ad_len += t->len + 1;
            t->len = 0;
        }
        else if (t->len > 0 &&
            sd_len + t->len + 1 <= sizeof(sd_buf) )
        {
        	memcpy(&sd_buf[sd_len], t, t->len + 1);
			    sd_len += t->len + 1;
            t->len = 0;
        }
    }

    if(ad_len > 0 || sd_len > 0)
    {
		if (ad_len <= BLE_ADV_DATA_MAX_LEN) {
			memset(leatws_ctx.ad_data, 0, BLE_ADV_DATA_MAX_LEN);
			memcpy(leatws_ctx.ad_data, ad_buf, ad_len);
		}

		if (0 == sd_len) {
			memset(leatws_ctx.sd_data, 0, BLE_ADV_DATA_MAX_LEN);
		}

		if (sd_len <= BLE_ADV_DATA_MAX_LEN) {
			memset(leatws_ctx.sd_data, 0, BLE_ADV_DATA_MAX_LEN);
			memcpy(leatws_ctx.sd_data, sd_buf, sd_len);
		}
		ret = 1;
	}

	return ret;
}

int leatws_adv_enable_check(void)
{
	adv_data_t *adv_data;
	adv_data_t *t;
    int i = 0;

	if(sink_location != BT_AUDIO_LOCATIONS_FL) {
		return -1;
	}

	adv_data = mem_malloc(4 *sizeof(adv_data_t));
	if(adv_data == NULL)
	{
		SYS_LOG_ERR("malloc failed");
		goto end;
	}
	memset(adv_data, 0, 4 *sizeof(adv_data_t));

	/* flags*/
	t = &adv_data[0];
	t->type = 0x01;
	t->data[0] = (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR);
	t->len = 2;

	t = &adv_data[1];
    uint16 *cfg_device_id  = bt_manager_config_get_device_id();
    t->data[i++] = (cfg_device_id[1] & 0xff);
	t->data[i++] = (cfg_device_id[1] >> 8);
    memcpy(&t->data[i], "sirk:", strlen("sirk:"));
    i += strlen("sirk:");
    bt_manager_le_audio_get_sirk(&t->data[i]);
	t->type = BT_DATA_MANUFACTURER_DATA;
	t->len  = 2+strlen("sirk:")+16+1;
    SYS_LOG_INF("%02x, %02x, %02x, %02x", t->data[0], t->data[1], t->data[2], t->data[3]);

	if (_leatws_set_adv_data(adv_data))
	{
		_leatws_adv_start();
	}
	mem_free(adv_data);
end:
	return 0;
}

static bool data_cb(struct bt_data *data, void *user_data)
{
	uint16 vendor_id;
	uint8 sirk[16];

	switch (data->type)
    {
        case BT_DATA_MANUFACTURER_DATA:
			vendor_id = (data->data[1]<<8) | (data->data[0]);
			uint16 *cfg_device_id  = bt_manager_config_get_device_id();
			if(cfg_device_id[1] != vendor_id) {
				SYS_LOG_INF("%x, %x\r\n", cfg_device_id[2], vendor_id);
				return true;
			}
			if(memcmp(&data->data[2], "sirk:", strlen("sirk:"))) {
				return true;
			}

			hostif_bt_le_scan_stop();
			memcpy(sirk, &data->data[2+strlen("sirk:")], 16);
			property_set(CFG_LEA_SIRK, sirk, 16);
			print_hex("recv_sirk:", sirk, 16);
			bt_manager_le_audio_adv_enable();
			leatws_notify_leaudio_reinit();
            return false;
        default:
            return true;
	}
}

static void leatws_scan_recv(const struct bt_le_scan_recv_info *info, struct net_buf_simple *buf)
{
	char data[32];

    if (info->adv_type != BT_GAP_ADV_TYPE_ADV_IND) {
		return;
	}

	(void)memset(data, 0, sizeof(data));
	bt_data_parse(buf, data_cb, data);
}

static struct bt_le_scan_cb leatws_scan_callbacks = {
	.recv = leatws_scan_recv,
};

int leatws_scan_start(void)
{
	struct bt_le_scan_param param;
	param.type = BT_LE_SCAN_TYPE_PASSIVE;
	param.options = BT_LE_SCAN_OPT_FILTER_DUPLICATE;
	param.interval = BT_GAP_SCAN_FAST_INTERVAL,
	param.window = BT_GAP_SCAN_FAST_WINDOW,
	param.timeout = 0;

	bt_manager_le_audio_adv_disable();
	hostif_bt_le_scan_stop();

	hostif_bt_le_scan_cb_register(&leatws_scan_callbacks);
	int err = hostif_bt_le_scan_start(&param, NULL);
	if (err) {
		SYS_LOG_ERR("failed %d\n", err);
	}else {
		SYS_LOG_INF("success\n");
	}
	return err;
}

int leatws_scan_stop(void)
{
	int err = 0;

	err = hostif_bt_le_scan_stop();
	if (err) {
		SYS_LOG_ERR("err:%d\n",err);
		return -1;
	}
	return 0;
}