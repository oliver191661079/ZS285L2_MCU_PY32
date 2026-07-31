/*
 * Copyright (c) 2024 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt call media
 */
#include <msg_manager.h>
#include <thread_timer.h>
#include <media_player.h>
#include <audio_system.h>
#include <audio_policy.h>
#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ringbuff_stream.h>
#include <zero_stream.h>
#include "ui_manager.h"
#include "media_mem.h"
#include "btcall.h"
#include "tts_manager.h"

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
#define DVFS_LEVEL_MEDIA_BTCALL	SOC_DVFS_LEVEL_FULL_PERFORMANCE
#endif

static io_stream_t bt_call_create_inputstream(void)
{
	int ret = 0;
	io_stream_t input_stream = NULL;

	SYS_LOG_INF("");
	input_stream =
	    ringbuff_stream_create_ext(media_mem_get_cache_pool
				       (INPUT_PLAYBACK, AUDIO_STREAM_VOICE),
				       media_mem_get_cache_pool_size
				       (INPUT_PLAYBACK, AUDIO_STREAM_VOICE));

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
	SYS_LOG_INF(" %p ", input_stream);
	return input_stream;
}

#ifdef CONFIG_AUDIO_VOICE_HARDWARE_REFERENCE_EXTERNAL
static io_stream_t bt_call_create_refstream(void)
{
	int ret = 0;
	io_stream_t stream = NULL;

	SYS_LOG_INF("");
	stream =
	    ringbuff_stream_create_ext(media_mem_get_cache_pool
				       (AEC_REFBUF0, AUDIO_STREAM_VOICE),
				       media_mem_get_cache_pool_size
				       (AEC_REFBUF0, AUDIO_STREAM_VOICE));

	if (!stream) {
		goto exit;
	}

	ret = stream_open(stream, MODE_IN_OUT);
	if (ret) {
		stream_destroy(stream);
		stream = NULL;
		goto exit;
	}

 exit:
	SYS_LOG_INF("stream=%p", stream);
	return stream;
}

void bt_call_start_ref_capture(io_stream_t output_stream)
{
	struct btcall_app_t *btcall = btcall_get_app();
	struct bt_audio_chan *bt_chan;
	struct bt_audio_chan_info chan_info;
	media_init_param_t init_param;
	int ret;

	SYS_LOG_INF("");
	if (NULL == btcall) {
		SYS_LOG_WRN("NULL\n");
		return;
	}

	bt_chan = &btcall->bt_chan;
	ret = bt_manager_audio_stream_info(bt_chan, &chan_info);
	if (ret) {
		return;
	}

	if (!btcall || !output_stream)
		return;

	if (btcall->ref_capture) {
		SYS_LOG_INF(" ref already open ");
		return;
	}
	memset(&init_param, 0, sizeof(media_init_param_t));
	init_param.type = MEDIA_SRV_TYPE_CAPTURE;
	init_param.stream_type = AUDIO_STREAM_VOICE;
	init_param.support_tws = 0;
	init_param.dumpable = 1;

	init_param.capture_format = PCM_TYPE;
	init_param.capture_input_stream = NULL;
	init_param.capture_output_stream = output_stream;
	init_param.capture_channels_input = 2;
	init_param.capture_channels_output = 1;
	//TODO: right sample rate input?
	init_param.capture_sample_rate_input = 96;
	init_param.capture_sample_rate_output = chan_info.sample;
	init_param.capture_sample_bits = 32;
	init_param.waitto_start = 1;
	init_param.output_ref = 1;

	btcall->ref_capture = media_player_open(&init_param);
	if (!btcall->ref_capture) {
		SYS_LOG_ERR("player open failed\n");
		return;
	}

	btcall->ref_capture_media_opened = 1;

	media_player_play(btcall->ref_capture);

	btcall->ref_capture_media_started = 1;

	SYS_LOG_INF("player open sucessed %p ", btcall->ref_capture);
	return;
}

void bt_call_stop_ref_capture(void)
{
	struct btcall_app_t *btcall = btcall_get_app();

	SYS_LOG_INF("");
	if (NULL == btcall) {
		SYS_LOG_WRN("NULL\n");
		return;
	}

	if (!btcall->ref_capture)
		return;

	media_player_stop(btcall->ref_capture);
	btcall->ref_capture_media_started = 0;

	SYS_LOG_INF("stopped\n");
}

void bt_call_close_ref_capture(void)
{
	struct btcall_app_t *btcall = btcall_get_app();

	SYS_LOG_INF("");
	if (NULL == btcall) {
		SYS_LOG_WRN("NULL\n");
		return;
	}

	if (!btcall->ref_capture)
		return;

	media_player_close(btcall->ref_capture);
	btcall->ref_capture_media_opened = 0;
	btcall->ref_capture = NULL;

	SYS_LOG_INF("closed\n");
}

#endif

void bt_call_start_play_and_capture(void)
{
	struct btcall_app_t *btcall = btcall_get_app();
	struct bt_audio_chan *bt_chan;
	struct bt_audio_chan_info chan_info;
	media_init_param_t init_param;
	int ret;

	SYS_LOG_INF("");
	if (NULL == btcall) {
		SYS_LOG_WRN("NULL\n");
		return;
	}

	bt_chan = &btcall->bt_chan;
	ret = bt_manager_audio_stream_info(bt_chan, &chan_info);
	if (ret) {
		return;
	}
#ifdef CONFIG_PLAYTTS
	tts_manager_wait_finished(true);
	tts_manager_lock();
#endif

	if (btcall->player) {
		bt_call_stop_play_and_capture();
		SYS_LOG_INF("already open\n");
	}

	audio_system_set_output_sample_rate(48);
	memset(&init_param, 0, sizeof(media_init_param_t));
	init_param.type = MEDIA_SRV_TYPE_PLAYBACK_AND_CAPTURE;
	init_param.format = bt_manager_audio_codec_type(chan_info.format);
	init_param.sample_rate = chan_info.sample;
	init_param.sample_bits = 32;

	if (btcall->asqt_simulate) {
		btcall->source_stream = zero_stream_create();
	} else {
		btcall->source_stream =
		    bt_manager_audio_source_stream_create_single(bt_chan, 0);
	}

	if (!btcall->source_stream) {
		SYS_LOG_INF("stream create failed");
		goto err_exit;
	}

	if (stream_open(btcall->source_stream, MODE_OUT)) {
		stream_destroy(btcall->source_stream);
		btcall->source_stream = NULL;
		SYS_LOG_INF("stream open failed ");
		goto err_exit;
	}

	if (init_param.format == MSBC_TYPE) {
		init_param.capture_bit_rate = 26;
	}

	SYS_LOG_INF("codec_id %d sample rate: %d", init_param.format,
		    init_param.sample_rate);

	btcall->sink_stream = bt_call_create_inputstream();
	init_param.stream_type = AUDIO_STREAM_VOICE;
	init_param.efx_stream_type = AUDIO_STREAM_VOICE;
	init_param.input_stream = btcall->sink_stream;
	init_param.output_stream = NULL;
	init_param.event_notify_handle = NULL;
	init_param.capture_format = init_param.format;
	init_param.capture_sample_rate_input = init_param.sample_rate;
	init_param.capture_sample_rate_output = init_param.sample_rate;
	init_param.capture_input_stream = NULL;
	init_param.capture_output_stream = btcall->source_stream;
	init_param.dumpable = true;
#ifdef CONFIG_AUDIO_VOICE_HARDWARE_REFERENCE_EXTERNAL
	init_param.ref_stream = bt_call_create_refstream();
#endif

	if (audio_policy_get_out_audio_mode(init_param.stream_type) ==
	    AUDIO_MODE_STEREO) {
		init_param.channels = 2;
	} else {
		init_param.channels = 1;
	}

	//To do
	if (audio_policy_get_record_audio_mode(init_param.stream_type) ==
	    AUDIO_MODE_STEREO) {
		init_param.capture_channels_input = 2;
		init_param.capture_channels_output = 2;
	} else {
		init_param.capture_channels_input = 1;
		init_param.capture_channels_output = 1;
	}

	bt_manager_audio_sink_stream_set(bt_chan, btcall->sink_stream);

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	btcall->set_dvfs_level = DVFS_LEVEL_MEDIA_BTCALL;
	soc_dvfs_set_level(btcall->set_dvfs_level, APP_ID_BTCALL);
#endif

	btcall->player = media_player_open(&init_param);
	if (!btcall->player) {
		goto err_exit;
	}
#ifdef CONFIG_AUDIO_VOICE_HARDWARE_REFERENCE_EXTERNAL
	btcall->ref_stream = init_param.ref_stream;
	bt_call_start_ref_capture(btcall->ref_stream);
#endif

	audio_system_mute_microphone(btcall->mic_mute);

	media_player_fade_in(btcall->player, 60);

	btcall->playback_media_opened = 1;
	btcall->capture_media_opened = 1;

	bt_manager_volume_set(audio_system_get_stream_volume
			      (AUDIO_STREAM_VOICE), BT_VOLUME_TYPE_BR_CALL);

	media_player_play(btcall->player);

	btcall->playback_media_started = 1;
	btcall->capture_media_started = 1;

	SYS_LOG_INF("start sucessed %p ", btcall->player);
#if 0
	if (bt_manager_sco_get_codecid() == MSBC_TYPE
	    && bt_manager_hfp_get_status() != BT_STATUS_SIRI) {
		seg_led_display_string(SLED_NUMBER2, "-", true);
	}
#endif

	return;

 err_exit:
	if (btcall->source_stream) {
		stream_close(btcall->source_stream);
		stream_destroy(btcall->source_stream);
		btcall->source_stream = NULL;
	}
#ifdef CONFIG_PLAYTTS
	tts_manager_unlock();
#endif

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	if (btcall->set_dvfs_level) {
		soc_dvfs_unset_level(btcall->set_dvfs_level, APP_ID_BTCALL);
		btcall->set_dvfs_level = 0;
	}
#endif
#ifdef CONFIG_AUDIO_VOICE_HARDWARE_REFERENCE_EXTERNAL
	bt_call_stop_ref_capture();
#endif
	audio_system_set_output_sample_rate(CONFIG_AUDIO_OUTPUT_SAMPLE_RATE);

	SYS_LOG_ERR("open failed \n");

	return;
}

void bt_call_stop_play_and_capture(void)
{
	struct btcall_app_t *btcall = btcall_get_app();

	if (NULL == btcall) {
		SYS_LOG_WRN("NULL\n");
		return;
	}

	SYS_LOG_INF("%p", btcall->player);

	if (!btcall->player) {
		/**avoid noise when hang up btcall */
		os_sleep(100);
		return;
	}

	media_player_fade_out(btcall->player, 10);

	/** reserve time to fade out*/
	os_sleep(100);

	bt_manager_audio_sink_stream_set(&btcall->bt_chan, NULL);

	if (btcall->sink_stream) {
		stream_close(btcall->sink_stream);
	}

	SYS_LOG_INF("stop player %p", btcall->player);
	media_player_stop(btcall->player);
#ifdef CONFIG_AUDIO_VOICE_HARDWARE_REFERENCE_EXTERNAL
	bt_call_stop_ref_capture();
#endif

	btcall->playback_media_started = 0;
	btcall->capture_media_started = 0;
	media_player_close(btcall->player);
#ifdef CONFIG_AUDIO_VOICE_HARDWARE_REFERENCE_EXTERNAL
	bt_call_close_ref_capture();
#endif
	if (btcall->source_stream) {
		stream_close(btcall->source_stream);
		stream_destroy(btcall->source_stream);
		btcall->source_stream = NULL;
	}

	if (btcall->sink_stream) {
		stream_destroy(btcall->sink_stream);
		btcall->sink_stream = NULL;
	}

	btcall->playback_media_opened = 0;
	btcall->capture_media_opened = 0;

	btcall->player = NULL;

	if (btcall->ref_stream) {
		stream_destroy(btcall->ref_stream);
		btcall->ref_stream = NULL;
	}

	audio_system_set_output_sample_rate(CONFIG_AUDIO_OUTPUT_SAMPLE_RATE);

#ifdef CONFIG_PLAYTTS
	tts_manager_unlock();
#endif

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	if (btcall->set_dvfs_level) {
		soc_dvfs_unset_level(btcall->set_dvfs_level, APP_ID_BTCALL);
		btcall->set_dvfs_level = 0;
	}
#endif
}

void bt_call_restart_play(void)
{
	struct btcall_app_t *btcall = btcall_get_app();

	SYS_LOG_INF("");
	if (NULL == btcall) {
		SYS_LOG_WRN("NULL\n");
		return;
	}

	if (btcall->bt_chan.handle != 0) {
		bt_call_start_play_and_capture();
	}
}
