/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "bmr.h"
#ifdef CONFIG_BT_SELF_APP
#include "selfapp_api.h"
#endif
#include <audio_track.h>

static io_stream_t bmr_create_playback_stream(int sdu)
{
	io_stream_t input_stream = NULL;
	int buff_size;
	int ret;

	buff_size = media_mem_get_cache_pool_size_ext(INPUT_PLAYBACK, AUDIO_STREAM_LE_AUDIO,
					NAV_TYPE, sdu, 14);	// TODO:
	if (buff_size <= 0) {
		goto exit;
	}

	input_stream = ringbuff_stream_create_ext(
			media_mem_get_cache_pool(INPUT_PLAYBACK, AUDIO_STREAM_LE_AUDIO),
			buff_size);

	if (!input_stream) {
		goto exit;
	}

	ret = stream_open(input_stream, MODE_IN_OUT | MODE_READ_BLOCK | MODE_BLOCK_TIMEOUT);
	if (ret) {
		stream_destroy(input_stream);
		input_stream = NULL;
	}

exit:
	SYS_LOG_INF("%p\n", input_stream);
	return input_stream;
}

void bmr_broadcast_sink_stream_set(io_stream_t stream)
{
	struct bmr_app_t *bmr = bmr_get_app();
	struct bt_broadcast_chan *chan;
	int i;

	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		chan = &bmr->broad_chan[i];
		if (chan->handle == 0 && chan->id == 0) {
			continue;
		}
		bt_manager_broadcast_sink_stream_set(chan,stream);
	}
}

static void set_player_effect_output_mode(media_player_t *player)
{
	int mode = CONFIG_MEDIA_EFFECT_OUTMODE;

	if (app_tws_status_get_enable()) {
		mode = MEDIA_EFFECT_OUTPUT_R_ONLY;
	}

#ifdef CONFIG_BT_SELF_APP
	u8_t ch;
	ch = selfapp_get_channel();
	if(ch == 0) {
		mode = MEDIA_EFFECT_OUTPUT_DEFAULT;
	} else if (ch == 1) {
		mode = MEDIA_EFFECT_OUTPUT_L_ONLY;
	}else {
		mode = MEDIA_EFFECT_OUTPUT_R_ONLY;
	}
#endif
	if(mode != CONFIG_MEDIA_EFFECT_OUTMODE) {
		media_player_set_effect_output_mode(player, mode);
	}
}

int bmr_init_playback(void)
{
	struct bmr_app_t *bmr = bmr_get_app();
//	struct bt_broadcast_chan *chan = &bmr->broad_chan;
	struct bt_broadcast_chan_info chan_info;
	io_stream_t input_stream = NULL;
	media_init_param_t init_param;
	int ret;
	struct bt_broadcast_chan *chan = bmr->chan;
#if 0
	if (!bmr) {
		SYS_LOG_INF("exit\n");
		return -EALREADY;
	}
#endif
	if (bmr->player) {
		SYS_LOG_INF("already\n");
		return -EALREADY;
	}

	ret = bt_manager_broadcast_stream_info(chan, &chan_info);
	if (ret) {
		return ret;
	}

#ifdef CONFIG_PLAYTTS
	tts_manager_wait_finished(false);
#endif

	memset(&init_param, 0, sizeof(media_init_param_t));

	input_stream = bmr_create_playback_stream(chan_info.sdu);
	if (!input_stream) {
		goto exit;
	}

	init_param.type = MEDIA_SRV_TYPE_PLAYBACK;
	init_param.format = bt_manager_audio_codec_type(chan_info.format);
	init_param.stream_type = AUDIO_STREAM_LE_AUDIO;
	init_param.efx_stream_type = AUDIO_STREAM_LE_AUDIO;
	init_param.sample_rate = chan_info.sample;
	init_param.sample_bits = 32;
	init_param.input_stream = input_stream;
	init_param.output_stream = NULL;
	init_param.event_notify_handle = NULL;
	init_param.support_tws = 1;
	init_param.dumpable = 1;
	if (bmr->num_of_broad_chan == 2) {
		init_param.channels = 2;
		init_param.nav_channel_separate = 1;
	} else {
		init_param.channels = chan_info.channels;
	}

	init_param.waitto_start = 1;
	init_param.bit_rate = chan_info.kbps;
	init_param.device_info.rx_chan.validate = 1;
	init_param.device_info.rx_chan.type = MEDIA_AUDIO_DEVICE_TYPE_BIS;
	init_param.device_info.rx_chan.bis_chan.handle = chan->handle;
	init_param.device_info.rx_chan.bis_chan.id = chan->id;
	init_param.device_info.rx_chan.timeline_owner = chan_info.chan;

	// audio_system_set_output_sample_rate(48);

	if (chan_info.duration == 7) {
		audio_policy_set_nav_frame_size_us(7500);
	} else {
		audio_policy_set_nav_frame_size_us(10000);
	}

	SYS_LOG_INF("format: %d, sample: %d, channels: %d, kbps: %d, duration: %d\n",
		chan_info.format, chan_info.sample, chan_info.channels,
		chan_info.kbps, chan_info.duration);

	bmr->player = media_player_open(&init_param);
	if (!bmr->player) {
		goto exit;
	}

	set_player_effect_output_mode(bmr->player);

	bmr->stream = input_stream;

	struct audio_track_t * track;
	track = audio_system_get_audio_track_handle(AUDIO_STREAM_LE_AUDIO);

	if (NULL != track) {
		if (bmr->player_paused) {
			audio_track_mute(track, 1);
		}else
			media_player_fade_in(bmr->player, 80);
	}else{
		SYS_LOG_ERR("SHOULD not be here\n");
	}

	SYS_LOG_INF("%p,%d\n", bmr->player,bmr->num_of_broad_chan);

	return 0;

exit:
	if (input_stream) {
		stream_close(input_stream);
		stream_destroy(input_stream);
	}

	SYS_LOG_ERR("open failed\n");
	return -EINVAL;
}

int bmr_start_playback(void)
{
	struct bmr_app_t *bmr = bmr_get_app();

#if 0
	if (!bmr) {
		SYS_LOG_INF("exit\n");
		return -EALREADY;
	}
#endif
	if (!bmr->player) {
		SYS_LOG_INF("not ready\n");
		return -EINVAL;
	}

	if (bmr->player_run) {
		SYS_LOG_INF("already\n");
		return -EINVAL;
	}

	bt_manager_broadcast_stream_start_cb_register(bmr->chan,
		media_player_audio_track_trigger_callback,audio_system_get_track());

	media_player_play(bmr->player);
	/*
	 * NOTE: sleep for resample page miss, capture/playback are
	 * shared, so only need to sleep once.
	 */
	if (!bmr->player_load) {
		os_sleep(OS_MSEC(10));
	}
	bmr->player_load = 1;

	bmr_broadcast_sink_stream_set(bmr->stream);

	bmr->player_run = 1;

	SYS_LOG_INF("%p\n", bmr->player);

	return 0;
}

int bmr_stop_playback(void)
{
	struct bmr_app_t *bmr = bmr_get_app();

#if 0
	if (!bmr) {
		SYS_LOG_INF("exit\n");
		return -EALREADY;
	}
#endif
	if (!bmr->player || !bmr->player_run) {
		SYS_LOG_INF("not ready\n");
		return -EINVAL;
	}

	MSG_SEND_TIME_STAT_START();

	bt_manager_broadcast_stream_start_cb_register(bmr->chan,
		NULL,NULL);

	media_player_fade_out(bmr->player, 10);

	/* reserve time to fade out */
	os_sleep(60);

    bmr_broadcast_sink_stream_set(NULL);
	if (bmr->player_load) {
		media_player_stop(bmr->player);
	}

	MSG_SEND_TIME_STAT_STOP();

	if (bmr->stream) {
		stream_close(bmr->stream);
	}

	bmr->player_run = 0;

	SYS_LOG_INF("%p\n", bmr->player);

	return 0;
}

int bmr_exit_playback(void)
{
	struct bmr_app_t *bmr = bmr_get_app();

#if 0
	if (!bmr) {
		SYS_LOG_INF("exit\n");
		return -EALREADY;
	}
#endif
	if (!bmr->player) {
		SYS_LOG_INF("already\n");
		return -EALREADY;
	}

	media_player_close(bmr->player);

	if (bmr->stream) {
		stream_destroy(bmr->stream);
		bmr->stream = NULL;
	}

	SYS_LOG_INF("%p\n", bmr->player);

	bmr->player = NULL;
	bmr->player_load = 0;
	bmr->player_run = 0;

	return 0;
}
