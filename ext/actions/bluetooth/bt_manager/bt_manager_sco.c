/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt manager a2dp profile.
 */
#define SYS_LOG_NO_NEWLINE
#define SYS_LOG_DOMAIN "bt manager"

#include <os_common_api.h>
#include <media_type.h>
#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stream.h>
#include <bt_manager.h>
#include <power_manager.h>
#include <app_manager.h>
#include <sys_event.h>
#include <mem_manager.h>
#include "bt_manager_inner.h"
#include <assert.h>
#include "btservice_api.h"
#include <audio_policy.h>
#include <audio_system.h>

static void _bt_manager_sco_callback(uint16_t hdl, btsrv_hfp_event_e event, uint8_t *param, int param_size)
{
	switch (event) {
	case BTSRV_SCO_DATA_INDICATED:
	{
		int ret = 0;
		static uint8_t print_cnt = 0;

		bt_manager_stream_pool_lock();
		io_stream_t bt_stream = bt_manager_get_stream(STREAM_TYPE_SCO);

		if (!bt_stream) {
			bt_manager_stream_pool_unlock();
			if (print_cnt == 0) {
				SYS_LOG_WRN("stream is null\n");
			}
			print_cnt++;
			break;
		}
#if 0
		if (stream_get_space(bt_stream) < param_size) {
			bt_manager_stream_pool_unlock();
			if (print_cnt == 0) {
				SYS_LOG_WRN("stream is full\n");
			}
			print_cnt++;
			break;
		}
#endif
		ret = stream_write(bt_stream, param, param_size);
		if (ret != param_size) {
			SYS_LOG_WRN("stream is full, write %d error %d\n", param_size, ret);
		}
		bt_manager_stream_pool_unlock();
		print_cnt = 0;
		break;
	}
	default:
	break;
	}
}

int bt_manager_hfp_sco_start(void)
{
	return btif_sco_start(&_bt_manager_sco_callback);
}

int bt_manager_hfp_sco_stop(void)
{
	return btif_sco_stop();
}

int bt_manager_hfp_disconnect_sco(uint16_t acl_hdl)
{
	return btif_disconnect_sco(acl_hdl);
}

int bt_manager_sco_get_codecid(void)
{
	uint8_t codec_id = 0;
	bt_mgr_dev_info_t* hfp_dev = bt_mgr_get_hfp_active_dev();
	if (hfp_dev){
		codec_id = hfp_dev->hfp_codec_id;
	}

	if (!codec_id) {
		codec_id = BTSRV_SCO_CVSD;
	}
	SYS_LOG_INF("codec_id %d\n", codec_id);

	return codec_id;
}

int bt_manager_sco_get_sample_rate(void)
{
	uint8_t hfp_sample_rate = 0;
	bt_mgr_dev_info_t* hfp_dev = bt_mgr_get_hfp_active_dev();
	if (hfp_dev){
		hfp_sample_rate = hfp_dev->hfp_sample_rate;
	}

	if (!hfp_sample_rate) {
		hfp_sample_rate = 8;
	}

	SYS_LOG_INF("sample rate %d\n", hfp_sample_rate);
	return hfp_sample_rate;
}

/**add for asqt btcall simulator*/
void bt_manager_sco_set_codec_info(uint16_t handle,uint8_t codec_id, uint8_t sample_rate)
{
	assert((codec_id == BTSRV_SCO_CVSD && sample_rate == 8)
			|| (codec_id == BTSRV_SCO_MSBC && sample_rate == 16)
			|| (codec_id == BTSRV_SCO_LC3_SWB && sample_rate == 32));

	struct bt_audio_codec codec;
	codec.handle = handle;
	if (codec_id == BTSRV_SCO_CVSD) {
		codec.format = BT_AUDIO_CODEC_CVSD;
	} else if (codec_id == BTSRV_SCO_MSBC) {
		codec.format = BT_AUDIO_CODEC_MSBC;
	} else if (codec_id == BTSRV_SCO_LC3_SWB) {
		codec.format = BT_AUDIO_CODEC_LC3_SWB;
	}
	codec.sample_rate = sample_rate;
	codec.dir = BT_AUDIO_DIR_SINK;
	codec.id = BT_AUDIO_ENDPOINT_CALL;

	bt_mgr_dev_info_t* hfp_dev = bt_mgr_get_hfp_active_dev();
	if (hfp_dev)
	{
		hfp_dev->hfp_codec_id = codec_id;
		hfp_dev->hfp_sample_rate = sample_rate;
	}
	SYS_LOG_INF("codec_id %d sample rate %d\n", codec_id,codec.sample_rate);

	bt_manager_audio_stream_event(BT_AUDIO_STREAM_CONFIG_CODEC, (void*)&codec, sizeof(struct bt_audio_codec));
}



int bt_manager_hfp_sco_init(void)
{
	return 0;
}

