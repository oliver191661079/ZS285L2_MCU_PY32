/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt manager genarate bt mac and name.
 */
#define SYS_LOG_DOMAIN "bt manager"

#include <os_common_api.h>

#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <msg_manager.h>
#include <mem_manager.h>
#include <bt_manager.h>
#include "bt_manager_inner.h"
#include <sys_event.h>
#include <property_manager.h>
#include <hex_str.h>
#include <acts_bluetooth/host_interface.h>

//#define CONFIG_AUDIO_RANDOM 1

#ifdef CONFIG_AUDIO_RANDOM
#ifdef CONFIG_AUDIO_IN
#include <audio_policy.h>
#include <audio_system.h>
#include <audio_hal.h>
#endif
#else
#if WAIT_TODO
#include <random.h>
#endif
#include <crc.h>
#endif

#define MAC_STR_LEN		(12+1)	/* 12(len) + 1(NULL) */
#define BT_NAME_LEN		(CONFIG_MAX_BT_NAME_LEN+1)	/* 32(len) + 1(NULL) */
#define BT_PRE_NAME_LEN	(20+1)	/* 10(len) + 1(NULL) */


#if defined(CONFIG_PROPERTY) && defined(CONFIG_AUDIO_IN) && defined(CONFIG_AUDIO_RANDOM)
#define AUDIO_BUF_SIZE			512
#define MIC_SAMPLE_TIMES		20

typedef struct {
	uint32_t samples;
	uint32_t rand;
	uint8_t *buffer;
	uint16_t buffer_size;
} mic_randomizer_t;

static int mic_in_stream_handle(void *callback_data, uint32_t reason)
{
	int  i;
	mic_randomizer_t *p = (mic_randomizer_t *)callback_data;

	for (i = 0; i < p->buffer_size / 2; i++) {
		p->rand = (p->rand*131) + p->buffer[i];
	}

	if (p->samples > 0) {
		p->samples--;
	}
	return 0;
}

static uint32_t mic_gen_rand(void)
{
	mic_randomizer_t  mic_randomizer;
	audio_in_init_param_t init_param;
	void *ain_handle;
	void *sample_buf;

	memset(&init_param, 0, sizeof(audio_in_init_param_t));

	sample_buf = mem_malloc(AUDIO_BUF_SIZE);
	__ASSERT_NO_MSG(sample_buf != NULL);

	init_param.channel_type = audio_policy_get_record_channel_type(AUDIO_STREAM_VOICE);
	init_param.sample_rate = 16;
	init_param.reload_addr = sample_buf;
	init_param.reload_len = AUDIO_BUF_SIZE;
	init_param.adc_gain = 0;
	init_param.data_mode = 0;
	init_param.input_gain = 2;
	init_param.dma_reload = 1;
	init_param.left_input = audio_policy_get_record_left_channel_id(AUDIO_STREAM_VOICE);
	init_param.right_input = audio_policy_get_record_left_channel_id(AUDIO_STREAM_VOICE);

	init_param.callback = mic_in_stream_handle;
	init_param.callback_data = (void *)&mic_randomizer;

	mic_randomizer.samples = MIC_SAMPLE_TIMES;
	mic_randomizer.buffer = sample_buf;
	mic_randomizer.buffer_size = AUDIO_BUF_SIZE;

	ain_handle = hal_ain_channel_open(&init_param);
	if (!ain_handle) {
		SYS_LOG_ERR("ain open faild");
		return k_uptime_get_32();
	}

	hal_ain_channel_start(ain_handle);

	while (mic_randomizer.samples != 0) {
		os_sleep(1);
	}

	hal_ain_channel_stop(ain_handle);
	hal_ain_channel_close(ain_handle);

	mem_free(sample_buf);

	return mic_randomizer.rand;
}
#endif

static int bt_manager_name_from_mac(uint8_t *mac_str, const char *suffix,
				    uint8_t *name, size_t name_size)
{
	uint8_t prefix[BT_PRE_NAME_LEN];
	int ret;
	int len;

	if (!mac_str || !name || name_size == 0) {
		return -1;
	}

	memset(prefix, 0, sizeof(prefix));
	ret = property_get_string(CFG_BT_PRE_NAME, prefix,
				  sizeof(prefix) - 1, "mag table_");
	if (ret < 0 || prefix[0] == '\0') {
		strncpy(prefix, "mag table_", sizeof(prefix) - 1);
	}

	len = snprintk(name, name_size, "%s%s%s", prefix, mac_str, suffix);
	if (len <= 0 || len >= (int)name_size) {
		return -1;
	}

	return len;
}

static void bt_manager_bt_name(uint8_t *mac_str)
{
	uint8_t bt_name[BT_NAME_LEN];
	uint8_t le_name[BT_NAME_LEN];
	int len;
	btmgr_base_config_t *cfg_base = bt_manager_get_base_config();

	if (!mac_str || strlen((char *)mac_str) < (MAC_STR_LEN - 1)) {
		SYS_LOG_WRN("invalid mac for bt name");
		return;
	}

	len = bt_manager_name_from_mac(mac_str, "_br", bt_name, BT_NAME_LEN);
	if (len > 0) {
		property_set_factory(CFG_BT_NAME, bt_name, len);
		property_set_factory(CFG_BT_LOCAL_NAME, bt_name, len);
		property_set_factory(CFG_BT_BROADCAST_NAME, bt_name, len);
#ifdef CONFIG_BT_SELF_APP
		property_set(CFG_APP_NAME, bt_name, len);
#endif
		memset(cfg_base->bt_device_name, 0,
		       sizeof(cfg_base->bt_device_name));
		memcpy(cfg_base->bt_device_name, bt_name,
		       MIN(len, (int)sizeof(cfg_base->bt_device_name) - 1));
		SYS_LOG_INF("%s: %s", CFG_BT_NAME, bt_name);
	}

	len = bt_manager_name_from_mac(mac_str, "_le", le_name, BT_NAME_LEN);
	if (len > 0) {
		property_set_factory(CFG_BLE_NAME, le_name, len);
		property_set_factory(CFG_BLE_AUDIO, le_name, len);
		SYS_LOG_INF("%s: %s", CFG_BLE_NAME, le_name);
	}
}

uint32_t bt_manager_get_bt_name_crc_value(void)
{
	uint8_t bt_name[BT_NAME_LEN];
	int ret;
	uint8_t *cfg_name = CFG_BT_NAME;

	memset(bt_name, 0, BT_NAME_LEN);
	ret = property_get_string(cfg_name, bt_name, (BT_NAME_LEN-1), CONFIG_BT_DEVICE_NAME);
	if (ret < 0) {
	    ret = strlen(bt_name);
	}

	return utils_crc32(0, bt_name, ret);
}

int bt_manager_bt_name_update(uint8_t *name, uint8_t len)
{
 	uint8_t bt_name[BT_NAME_LEN];
	struct bt_manager_context_t*  bt_manager = bt_manager_get_context();

	if (!name || 0 == len) {
		return -1;
	}

	if( len > (BT_NAME_LEN-1)){
		len = (BT_NAME_LEN-1);
	}

	memset(bt_name, 0, BT_NAME_LEN);
	memcpy(bt_name, name, len);
#ifdef CONFIG_PROPERTY
	property_set(CFG_BT_NAME, bt_name, BT_NAME_LEN);
	property_set(CFG_BLE_NAME, bt_name, BT_NAME_LEN);
	property_set(CFG_BT_LOCAL_NAME, bt_name, BT_NAME_LEN);
	property_set(CFG_APP_NAME, bt_name, BT_NAME_LEN);
	property_set(CFG_BLE_AUDIO, bt_name, BT_NAME_LEN);
#endif

	//to do ble name??
    if(bt_manager && bt_manager->connected_phone_num){
        bt_manager->update_local_name = 1;
    }
    else{
        btif_br_update_devie_name();
    }

#ifdef CONFIG_GFP_PROFILE
	extern void personalized_name_update(uint8_t * name, uint8_t size);
	personalized_name_update(bt_name, BT_NAME_LEN);
	bt_manager->personalized_name_update = 0;
#endif

#ifdef CONFIG_PROPERTY
	//property_flush(NULL);
#endif
	return 0;
}

void bt_manager_app_name_update(void)
{
#ifdef CONFIG_BT_SELF_APP
    uint8_t bt_name[BT_NAME_LEN];
    uint8_t len = 0;
    uint8_t *cfg_name = CFG_BT_NAME;
    int ret;

    memset(bt_name, 0, BT_NAME_LEN);
    ret = property_get_string(cfg_name, bt_name, (BT_NAME_LEN-1), CONFIG_BT_DEVICE_NAME);

    cfg_name = CFG_APP_NAME;
    len = strlen(bt_name);
    property_set(cfg_name, bt_name, len);
    SYS_LOG_INF("%s: %s", cfg_name, bt_name);
#endif
}

void bt_manager_set_selfapp_name_update_dev( struct bt_conn * conn)
{
    uint16_t hdl = hostif_bt_conn_get_handle(conn);
    SYS_LOG_INF("conn:%p hdl:%x",conn,hdl);

    bt_mgr_dev_info_t* dev_info = bt_mgr_find_dev_info_by_hdl(hdl);
    if(dev_info){
        dev_info->self_name_update = 1;
    }
}




void bt_manager_bt_mac(uint8_t *mac_str)
{
#ifdef CONFIG_PROPERTY
	uint8_t mac[6], i;
	int ret;
	uint32_t rand_val;

	ret = property_get_hexstring(CFG_BT_MAC, mac_str, (MAC_STR_LEN-1));
	if (ret < (MAC_STR_LEN-1)) {
		SYS_LOG_WRN("ret: %d", ret);
#ifdef CONFIG_AUDIO_RANDOM
		rand_val = mic_gen_rand();
#else
		rand_val = os_cycle_get_32();
#endif
		bt_manager_config_set_pre_bt_mac(mac);
		memcpy(&mac[3], &rand_val, 3);

		for (i = 3; i < 6; i++) {
			if (mac[i] == 0)
				mac[i] = 0x01;
		}

		hex_to_str(mac_str,mac, 6);

		property_set_factory(CFG_BT_MAC, mac_str, (MAC_STR_LEN-1));
	} else {
		str_to_hex(mac,mac_str, 12);
		bt_manager_updata_pre_bt_mac(mac);
	}
#endif
	SYS_LOG_INF("BT MAC: %s", mac_str);
}

void bt_manager_check_mac_name(void)
{
	uint8_t mac_str[MAC_STR_LEN];

	memset(mac_str, 0, MAC_STR_LEN);
	bt_manager_bt_mac(mac_str);
	bt_manager_bt_name(mac_str);
}
