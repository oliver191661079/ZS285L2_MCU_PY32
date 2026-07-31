/*
 * Copyright (c) 2024 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief lcmusic app main.
 */

#include "lcmusic.h"
#include "tts_manager.h"
#include "fs_manager.h"
#ifdef CONFIG_PROPERTY
#include "property_manager.h"
#endif

#include "buffer_stream.h"
#include "ringbuff_stream.h"
#include "media_mem.h"

#ifdef CONFIG_BMS_LCMUSIC_APP
static io_stream_t broadcast_create_source_stream(int mem_type, int block_num)
{
	io_stream_t stream = NULL;
	int buff_size;
	int ret;

	buff_size = media_mem_get_cache_pool_size_ext(mem_type, AUDIO_STREAM_LOCAL_MUSIC, NAV_TYPE, BROADCAST_SDU, block_num);	// TODO: 3?
	if (buff_size <= 0) {
		goto exit;
	}

	stream =
	    ringbuff_stream_create_ext(media_mem_get_cache_pool
				       (mem_type, AUDIO_STREAM_LOCAL_MUSIC),
				       buff_size);
	if (!stream) {
		goto exit;
	}

	ret = stream_open(stream, MODE_OUT);
	if (ret) {
		stream_destroy(stream);
		stream = NULL;
		goto exit;
	}

 exit:
	SYS_LOG_INF("%p\n", stream);
	return stream;
}

static io_stream_t bms_lcmusic_create_capture_input_stream(void)
{
	io_stream_t input_stream;
	int ret;

	input_stream =
	    ringbuff_stream_create_ext(media_mem_get_cache_pool
				       (INPUT_CAPTURE,
					AUDIO_STREAM_LOCAL_MUSIC),
				       media_mem_get_cache_pool_size
				       (INPUT_CAPTURE,
					AUDIO_STREAM_LOCAL_MUSIC));

	if (!input_stream) {
		goto exit;
	}

	ret =
	    stream_open(input_stream,
			MODE_IN_OUT | MODE_READ_BLOCK | MODE_BLOCK_TIMEOUT);
	if (ret) {
		stream_destroy(input_stream);
		input_stream = NULL;
		goto exit;
	}

 exit:
	SYS_LOG_INF("%p\n", input_stream);
	return input_stream;
}

static void bms_lcmusic_broadcast_source_stream_set(uint8 enable_flag)
{
	struct lcmusic_app_t *lcmusic = lcmusic_get_app();
	struct bt_broadcast_chan *chan;
	int i;

	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		chan = &lcmusic->broad_chan[i];
		if (chan->handle == 0 && chan->id == 0) {
			continue;
		}
		if (enable_flag) {
			bt_manager_broadcast_source_stream_set(chan,
							       lcmusic->stream
							       [i]);
		} else {
			bt_manager_broadcast_source_stream_set(chan, NULL);
		}
	}
}

int bms_lcmusic_init_capture(void)
{
	struct lcmusic_app_t *lcmusic = lcmusic_get_app();
	struct bt_broadcast_chan *chan;
	struct bt_broadcast_chan_info chan_info;
	io_stream_t source_stream = NULL;
	io_stream_t source_stream2 = NULL;
	io_stream_t input_stream = NULL;
	media_init_param_t init_param;
	int ret;

	if (NULL == lcmusic) {
		return -1;
	}

	SYS_LOG_INF("");

	if (lcmusic->capture_player) {
		SYS_LOG_INF("already\n");
		return -2;
	}

	chan = lcmusic->chan;
	if (NULL == chan) {
		SYS_LOG_INF("no chan\n");
		return -3;
	}
	ret = bt_manager_broadcast_stream_info(chan, &chan_info);
	if (ret) {
		return ret;
	}

	SYS_LOG_INF("chan: handle 0x%x id %d\n", chan->handle, chan->id);

	SYS_LOG_INF("chan_info: f%d s%d c%d k%d d%d t%p\n",
		    chan_info.format, chan_info.sample, chan_info.channels,
		    chan_info.kbps, chan_info.duration, chan_info.chan);

	/* wait until start_capture */
#ifdef CONFIG_PLAYTTS
	tts_manager_wait_finished(false);
#endif

	source_stream = broadcast_create_source_stream(OUTPUT_CAPTURE, 6);
	if (!source_stream) {
		goto exit;
	}
#if (BROADCAST_NUM_BIS == 2)
	if(!app_tws_status_get_enable()) {
		source_stream2 = broadcast_create_source_stream(OUTPUT_CAPTURE2, 6);
		if (!source_stream2) {
			goto exit;
		}
	}
#endif

	input_stream = bms_lcmusic_create_capture_input_stream();
	if (!input_stream) {
		goto exit;
	}

	memset(&init_param, 0, sizeof(media_init_param_t));

	init_param.type = MEDIA_SRV_TYPE_CAPTURE;
	init_param.stream_type = AUDIO_STREAM_LOCAL_MUSIC;
	init_param.efx_stream_type = AUDIO_STREAM_LOCAL_MUSIC;
	// TODO: broadcast_source no need to get chan_info???
	init_param.capture_format = NAV_TYPE;	// bt_manager_audio_codec_type(chan_info.format);
	init_param.capture_sample_rate_input = 48;	// chan_info.sample;
	init_param.capture_sample_rate_output = 48;	// chan_info.sample;
	init_param.capture_sample_bits = 32;
	//to do?
	init_param.capture_channels_input = 2;	//BROADCAST_CH;   // chan_info.channels;
	if(app_tws_status_get_enable()) {
		init_param.capture_channels_output = 1;
		init_param.capture_bit_rate = chan_info.kbps;
	} else {
		init_param.capture_channels_output = NUM_OF_BROAD_CHAN;
		init_param.capture_bit_rate = chan_info.kbps*BROADCAST_NUM_BIS; //BROADCAST_KBPS
	}
	init_param.capture_input_stream = input_stream;
	init_param.capture_output_stream = source_stream;
	init_param.capture_output_stream2 = source_stream2;
	init_param.waitto_start = 1;
	init_param.device_info.tx_chan.validate = 1;
	//init_param.device_info.tx_chan.use_trigger = 1;
	init_param.device_info.tx_chan.type = MEDIA_AUDIO_DEVICE_TYPE_BIS;
	init_param.device_info.tx_chan.bis_chan.handle = chan->handle;
	init_param.device_info.tx_chan.bis_chan.id = chan->id;
	init_param.device_info.tx_chan.timeline_owner = chan_info.chan;

	if (chan_info.duration == 7) {
		audio_policy_set_nav_frame_size_us(7500);
	} else {
		audio_policy_set_nav_frame_size_us(10000);
	}

	lcmusic->capture_player = media_player_open(&init_param);
	if (!lcmusic->capture_player) {
		goto exit;
	}

	lcmusic->stream[0] = source_stream;
	lcmusic->stream[1] = source_stream2;
	lcmusic->input_stream = input_stream;
	SYS_LOG_INF("%p opened\n", lcmusic->capture_player);

	return 0;

 exit:
	if (source_stream) {
		stream_close(source_stream);
		stream_destroy(source_stream);
	}
	if (source_stream2) {
		stream_close(source_stream2);
		stream_destroy(source_stream2);
	}
	if (input_stream) {
		stream_close(input_stream);
		stream_destroy(input_stream);
	}

	SYS_LOG_ERR("open failed\n");
	return -EINVAL;
}

int bms_lcmusic_start_capture(void)
{
	struct lcmusic_app_t *lcmusic = lcmusic_get_app();

	if (NULL == lcmusic) {
		return -1;
	}
	SYS_LOG_INF("");

	if (!lcmusic->capture_player) {
		SYS_LOG_INF("not ready\n");
		return -EINVAL;
	}

	if (lcmusic->capture_player_run) {
		SYS_LOG_INF("already\n");
		return -EINVAL;
	}
#ifdef CONFIG_PLAYTTS
	tts_manager_wait_finished(false);
#endif

	media_player_trigger_audio_record_start(lcmusic->capture_player);

	media_player_play(lcmusic->capture_player);
	/*
	 * NOTE: sleep for resample page miss, capture/playback are
	 * shared, so only need to sleep once.
	 */
	if (!lcmusic->capture_player_load) {
		os_sleep(OS_MSEC(10));
	}
	lcmusic->capture_player_load = 1;

	bms_lcmusic_broadcast_source_stream_set(1);
	btif_broadcast_source_set_retransmit(lcmusic->chan, lcmusic->irc);

	lcmusic->capture_player_run = 1;

	if (lcmusic_is_bms_mode() && lcmusic->lcplayer
	    && lcmusic->playback_player_run) {
		SYS_LOG_INF("playback start\n");
		media_player_play(lcmusic->lcplayer->player);
		lcmusic->playing = 1;
	}

	SYS_LOG_INF("%p captured irc:%d\n", lcmusic->capture_player,lcmusic->irc);

	return 0;
}

int bms_lcmusic_stop_capture(void)
{
	struct lcmusic_app_t *lcmusic = lcmusic_get_app();
	int i;

	if (!lcmusic->capture_player || !lcmusic->capture_player_run) {
		SYS_LOG_INF("not ready\n");
		return -EINVAL;
	}
	//bt_manager_broadcast_source_stream_set(&usound->broad_chan, NULL);
	bms_lcmusic_broadcast_source_stream_set(0);

	if (lcmusic->capture_player_load) {
		media_player_stop(lcmusic->capture_player);
	}
#ifdef CONFIG_PLAYTTS
	tts_manager_unlock();
#endif

	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		if (lcmusic->stream[i]) {
			stream_close(lcmusic->stream[i]);
		}
	}

	if (lcmusic->input_stream) {
		stream_close(lcmusic->input_stream);
	}

	lcmusic->capture_player_run = 0;

	SYS_LOG_INF("%p\n", lcmusic->capture_player);

	return 0;
}

int bms_lcmusic_exit_capture(void)
{
	struct lcmusic_app_t *lcmusic = lcmusic_get_app();
	int i;

	if (!lcmusic->capture_player) {
		SYS_LOG_INF("already\n");
		return -EALREADY;
	}

	media_player_close(lcmusic->capture_player);
	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		if (lcmusic->stream[i]) {
			stream_destroy(lcmusic->stream[i]);
			lcmusic->stream[i] = NULL;
		}
	}
	if (lcmusic->input_stream) {
		stream_destroy(lcmusic->input_stream);
		lcmusic->input_stream = NULL;
	}

	SYS_LOG_INF("%p\n", lcmusic->capture_player);

	lcmusic->capture_player = NULL;
	lcmusic->capture_player_load = 0;
	lcmusic->capture_player_run = 0;

	return 0;
}
#endif
