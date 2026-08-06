/*
 * Copyright (c) 2022 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief audio_input media
 */

#include "audio_input.h"
#include "tts_manager.h"
#include "buffer_stream.h"
#include "ringbuff_stream.h"
#include "media_mem.h"
#ifdef CONFIG_DVFS
#include <dvfs.h>
#endif
#ifdef CONFIG_BT_SELF_APP
#include "selfapp_api.h"
#endif


static void audio_input_event_notify(u32_t event, void *data, u32_t len,
				     void *user_data)
{
	SYS_LOG_INF("event %d\n", event);

	switch (event) {
	case PLAYBACK_EVENT_DATA_INDICATE:
	case PLAYBACK_EVENT_STOP_ERROR:
#ifdef CONFIG_AUDIO_INPUT_RESTART
		audio_input_player_reset_trigger();
#endif
		break;
	default:
		break;
	}
}

int audio_input_get_status(void)
{
	struct audio_input_app_t *audio_input = audio_input_get_app();

	if (!audio_input)
		return AUDIO_INPUT_STATUS_NULL;

	if (audio_input->playing)
		return AUDIO_INPUT_STATUS_PLAYING;
	else
		return AUDIO_INPUT_STATUS_PAUSED;
}

static io_stream_t _audio_input_create_inputstream(void)
{
	int ret = 0;
	io_stream_t input_stream = NULL;
	int stream_type = audio_input_get_audio_stream_type();

	input_stream =
	    ringbuff_stream_create_ext(media_mem_get_cache_pool
				       (INPUT_PLAYBACK,stream_type),
				       media_mem_get_cache_pool_size
				       (INPUT_PLAYBACK,stream_type));
	if (!input_stream) {
		goto exit;
	}

	ret = stream_open(input_stream, MODE_IN_OUT);
	if (ret) {
		stream_destroy(input_stream);
		input_stream = NULL;
		goto exit;
	}

 exit:
	SYS_LOG_INF(" %p\n", input_stream);
	return input_stream;
}

static void set_player_effect_output_mode(media_player_t *player)
{
	int mode = CONFIG_MEDIA_EFFECT_OUTMODE;

	if (app_tws_status_get_connected() && app_tws_status_get_enable()) {
		/* 真立体声 TWS：主单元播左声道、从单元播右声道 */
		if (app_tws_status_get_role() == APP_TWS_ROLE_SECONDARY) {
			mode = MEDIA_EFFECT_OUTPUT_R_ONLY;
		} else {
			mode = MEDIA_EFFECT_OUTPUT_L_ONLY;
		}
	}

#ifdef CONFIG_AUDIO_INPUT_BMS_APP
	if(audio_input_is_bms_mode()) {
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
	}
#endif

	SYS_LOG_INF("%d\n", mode);
	if(mode != CONFIG_MEDIA_EFFECT_OUTMODE) {
		media_player_set_effect_output_mode(player, mode);
	}
}

int audio_input_init_playback(void)
{
	struct audio_input_app_t *audio_input = audio_input_get_app();
	media_init_param_t init_param;
	io_stream_t input_stream = NULL;

	if (!audio_input) {
		return -1;
	}
#ifdef CONFIG_PLAYTTS
	tts_manager_wait_finished(false);
#endif

	if (audio_input->playback_player) {
		SYS_LOG_INF(" player already open ");
		return -2;
	}
	memset(&init_param, 0, sizeof(media_init_param_t));

	input_stream = _audio_input_create_inputstream();
	init_param.type = MEDIA_SRV_TYPE_PLAYBACK_AND_CAPTURE;
	init_param.stream_type = audio_input_get_audio_stream_type();
	init_param.efx_stream_type = init_param.stream_type;
	init_param.format = PCM_TYPE;
	init_param.input_stream = input_stream;
	init_param.output_stream = NULL;
	init_param.support_tws = 0;
	init_param.dumpable = 1;

	init_param.capture_format = PCM_TYPE;
	init_param.capture_input_stream = NULL;
	init_param.capture_output_stream = input_stream;
	init_param.capture_channels_input = 2;
	init_param.capture_channels_output = 2;
	init_param.event_notify_handle = audio_input_event_notify;

	SYS_LOG_INF("stream type %d\n", init_param.stream_type);
	if (init_param.stream_type == AUDIO_STREAM_LINEIN) {
		/* TWS requires sbc encoder which only support 32/44.1/48 kHz */
		init_param.capture_channels_input = 2;
		init_param.capture_sample_rate_input = 48;
		init_param.capture_sample_rate_output = 48;
		init_param.capture_sample_bits = 32;
		init_param.channels = 2;
		init_param.sample_rate = 48;
		init_param.sample_bits = 32;
	} else if (init_param.stream_type == AUDIO_STREAM_SPDIF_IN) {
		SYS_LOG_INF("SR %d\n", audio_input->srd_sample_rate);
#ifdef CONFIG_AUDIOIN_SRD
		init_param.capture_sample_rate_input = audio_input->srd_sample_rate;
#else
		init_param.capture_sample_rate_input = 48;
#endif
		if (init_param.capture_sample_rate_input == 44) {
			init_param.sample_rate = 44;
			init_param.capture_sample_rate_output = 44;
		} else if (init_param.capture_sample_rate_input == 88) {
			init_param.sample_rate = 44;
			init_param.capture_sample_rate_output = 44;
		} else {
			init_param.sample_rate = 48;
			init_param.capture_sample_rate_output = 48;
		}
		init_param.capture_channels_input = 2;
		init_param.channels = 2;
		init_param.capture_sample_bits = 32;
		init_param.sample_bits = 32;
	} else if (init_param.stream_type == AUDIO_STREAM_I2SRX_IN) {
		init_param.capture_channels_input = 2;
		SYS_LOG_INF("sr %d", audio_input->srd_sample_rate);
#ifdef CONFIG_AUDIOIN_SRD
		init_param.capture_sample_rate_input = (audio_input->srd_sample_rate == 0) ? 48 : audio_input->srd_sample_rate;
#else
		init_param.capture_sample_rate_input = CONFIG_AUDIO_OUTPUT_SAMPLE_RATE;
#endif
		init_param.capture_sample_rate_output = CONFIG_AUDIO_OUTPUT_SAMPLE_RATE;
		init_param.capture_sample_bits = 32;
		init_param.channels = 2;
		init_param.sample_rate = init_param.capture_sample_rate_output;
		init_param.sample_bits = 32;
	} else if (init_param.stream_type == AUDIO_STREAM_MIC_IN) {
		init_param.channels = 1;
		init_param.sample_rate = 48;
		init_param.capture_channels_input = 1;
		init_param.capture_sample_rate_input = 48;
		init_param.capture_sample_rate_output = 48;
	} else {
		SYS_LOG_ERR("Not supported stream type\n");
		goto err_exit;
	}

	//audio_system_set_output_sample_rate(init_param.capture_sample_rate_output);

#ifdef CONFIG_AUDIO_INPUT_BMS_APP
	if (audio_input_is_bms_mode() && audio_input->bms_source) {
		init_param.waitto_start = 1;
		init_param.bind_to_capture = 1;
#if (BROADCAST_DURATION == BT_FRAME_DURATION_7_5MS)
		audio_policy_set_nav_frame_size_us(7500);
#else
		audio_policy_set_nav_frame_size_us(10000);
#endif
		audio_policy_set_bis_link_delay_ms(broadcast_get_bis_link_delay
							(audio_input->qos));
	}
#endif

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	if (audio_input->set_dvfs_level == 0) {
		audio_input->set_dvfs_level = SOC_DVFS_LEVEL_FULL_PERFORMANCE;
		soc_dvfs_set_level(audio_input->set_dvfs_level, "audioin");
	}
#endif

	audio_input->audio_input_stream = init_param.input_stream;

	audio_input->playback_player = media_player_open(&init_param);
	if (!audio_input->playback_player) {
		SYS_LOG_ERR("player open failed\n");
		goto err_exit;
	}
	SYS_LOG_INF("opened %p ", audio_input->playback_player);

	set_player_effect_output_mode(audio_input->playback_player);

	if (!audio_input->playback_player_load) {
		os_sleep(OS_MSEC(10));
	}
	audio_input->playback_player_load = 1;

	return 0;
 err_exit:
	if (input_stream) {
		stream_close(input_stream);
		stream_destroy(input_stream);
	}
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	if (audio_input->set_dvfs_level) {
		soc_dvfs_unset_level(audio_input->set_dvfs_level, "aduionin");
		audio_input->set_dvfs_level = 0;
	}
#endif
	return -3;
}

int audio_input_start_playback(void)
{
	struct audio_input_app_t *audio_input = audio_input_get_app();
#ifdef CONFIG_AUDIO_INPUT_ENERGY_DETECT_DEMO
	effect_f_energy_set_t new_energy_set;
#endif

	if (!audio_input) {
		return -1;
	}

	if (!audio_input->playback_player) {
		SYS_LOG_INF("NULL player");
		return -2;
	}

	if (audio_input->playback_player_run) {
		SYS_LOG_INF("already run\n");
		return -3;
	}

#ifdef CONFIG_AUDIO_INPUT_BMS_APP
	if(audio_input_is_bms_mode() && audio_input->bms_source){
		bt_manager_broadcast_stream_tws_sync_cb_register_1(audio_input->chan,
			media_player_audio_track_trigger_callback, audio_system_get_track());
	}
#endif

#ifdef CONFIG_AUDIO_INPUT_ENERGY_DETECT_DEMO
	memset(&new_energy_set, 0x0, sizeof(new_energy_set));
	new_energy_set.t_energy_en = 1;
	new_energy_set.f_band_energy_en = 1,
	new_energy_set.f_band_energy_timewindow_ms = 10;
	new_energy_set.f_band_bandwidth = 29;
	new_energy_set.f_band_energy_num = 10;
	new_energy_set.f_band_energy_f0 = 22;
	new_energy_set.f_band_energy_f1 = 230;
	new_energy_set.f_band_energy_f2 = 233;
	new_energy_set.f_band_energy_f3 = 235;
	new_energy_set.f_band_energy_f4 = 237;
	new_energy_set.f_band_energy_f5 = 240;
	new_energy_set.f_band_energy_f6 = 3800;
	new_energy_set.f_band_energy_f7 = 8000;
	new_energy_set.f_band_energy_f8 = 10000;
	new_energy_set.f_band_energy_f9 = 15000;
	media_player_set_output_energy_sample(audio_input->playback_player, &new_energy_set);
#endif

	SYS_LOG_INF("%p ", audio_input->playback_player);
	media_player_fade_in(audio_input->playback_player, 80);
	media_player_play(audio_input->playback_player);

	audio_input->playback_player_run = 1;

	return 0;
}

int audio_input_stop_playback(void)
{
	struct audio_input_app_t *audio_input = audio_input_get_app();

	if (!audio_input) {
		return -1;
	}

	SYS_LOG_INF("");
	if (!audio_input->playback_player) {
		SYS_LOG_INF("NULL player");
		return -2;
	}

	if (!audio_input->playback_player_run) {
		SYS_LOG_INF("not ready\n");
		return -3;
	}

#ifdef CONFIG_AUDIO_INPUT_BMS_APP
	if(audio_input_is_bms_mode()){
		bt_manager_broadcast_stream_tws_sync_cb_register_1(audio_input->chan,
			NULL,NULL);
	}
#endif

	media_player_fade_out(audio_input->playback_player, 30);

	/** reserve time to fade out*/
	os_sleep(audio_policy_get_bis_link_delay_ms() + 80);

	if (audio_input->audio_input_stream) {
		stream_close(audio_input->audio_input_stream);
	}

	media_player_stop(audio_input->playback_player);

	media_player_close(audio_input->playback_player);
	SYS_LOG_INF("player closed");
	audio_input->playback_player_load = 0;
	audio_input->playback_player_run = 0;
	audio_input->playback_player = NULL;
	//clear restart at player stop.
	audio_input->restart = 0;

	if (audio_input->audio_input_stream) {
		stream_destroy(audio_input->audio_input_stream);
		audio_input->audio_input_stream = NULL;
	}
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	if (audio_input->set_dvfs_level) {
		soc_dvfs_unset_level(audio_input->set_dvfs_level, "audioin");
		audio_input->set_dvfs_level = 0;
	}
#endif

	return 0;
}

int audio_input_exit_playback(void)
{
	SYS_LOG_INF("");
	audio_policy_set_bis_link_delay_ms(0);
	return 0;
}

#ifdef CONFIG_AUDIO_INPUT_BMS_APP
static io_stream_t broadcast_create_source_stream(int mem_type, int block_num,
						  u32_t stream_type)
{
	io_stream_t stream = NULL;
	int buff_size;
	int ret;

	buff_size = media_mem_get_cache_pool_size_ext(mem_type, stream_type, NAV_TYPE, BROADCAST_SDU, block_num);	// TODO: 3?
	if (buff_size <= 0) {
		goto exit;
	}

	stream =
	    ringbuff_stream_create_ext(media_mem_get_cache_pool
				       (mem_type, stream_type), buff_size);
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

static io_stream_t audio_input_create_capture_input_stream(u32_t type)
{
	io_stream_t input_stream;
	int ret;

	input_stream =
	    ringbuff_stream_create_ext(media_mem_get_cache_pool
				       (INPUT_CAPTURE2, type),
				       media_mem_get_cache_pool_size
				       (INPUT_CAPTURE2, type));

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

static void audio_input_broadcast_source_stream_set(uint8 enable_flag)
{
	struct audio_input_app_t *audio_input = audio_input_get_app();
	struct bt_broadcast_chan *chan;
	int i;

	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		chan = &audio_input->broad_chan[i];
		if (chan->handle == 0 && chan->id == 0) {
			continue;
		}
		if (enable_flag) {
			bt_manager_broadcast_source_stream_set(chan,
							       audio_input->stream
							       [i]);
		} else {
			bt_manager_broadcast_source_stream_set(chan, NULL);
		}
	}
}

int audio_input_init_capture(void)
{
	struct audio_input_app_t *audio_input = audio_input_get_app();
	struct bt_broadcast_chan_info chan_info;
	struct bt_broadcast_chan *chan;
	io_stream_t source_stream = NULL;
	io_stream_t source_stream2 = NULL;
	io_stream_t input_stream = NULL;
	media_init_param_t init_param;
	int ret;

	if (NULL == audio_input) {
		return -1;
	}

	/* wait until start_capture */
#ifdef CONFIG_PLAYTTS
	tts_manager_wait_finished(false);
#endif

	if (audio_input->capture_player) {
		SYS_LOG_INF("already\n");
		return -2;
	}

	chan = audio_input->chan;
	ret = bt_manager_broadcast_stream_info(chan, &chan_info);
	if (ret) {
		SYS_LOG_WRN("No chan info\n");
		return ret;
	}

	SYS_LOG_INF("chan: handle 0x%x id%d\n", chan->handle, chan->id);

	SYS_LOG_INF("chan_info: f%d s%d c%d k%d d%d t%p\n",
		    chan_info.format, chan_info.sample, chan_info.channels,
		    chan_info.kbps, chan_info.duration, chan_info.chan);

	source_stream =
	    broadcast_create_source_stream(OUTPUT_CAPTURE, 6,
					   audio_input_get_audio_stream_type());
	if (!source_stream) {
		goto exit;
	}
#if (BROADCAST_NUM_BIS == 2)
	if(!app_tws_status_get_enable()) {
		source_stream2 =
		broadcast_create_source_stream(OUTPUT_CAPTURE2, 6,
						audio_input_get_audio_stream_type());
		if (!source_stream2) {
			goto exit;
		}
	}
#endif

	input_stream =
	    audio_input_create_capture_input_stream(audio_input_get_audio_stream_type());
	if (!input_stream) {
		goto exit;
	}

	memset(&init_param, 0, sizeof(media_init_param_t));

	init_param.type = MEDIA_SRV_TYPE_CAPTURE;
	init_param.stream_type = audio_input_get_audio_stream_type();
	init_param.efx_stream_type = audio_input_get_audio_stream_type();
	// TODO: broadcast_source no need to get chan_info???
	init_param.capture_format = NAV_TYPE;	// bt_manager_audio_codec_type(chan_info.format);
	init_param.capture_sample_rate_input = audio_system_get_output_sample_rate();	// chan_info.sample;
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
	init_param.device_info.tx_chan.type = MEDIA_AUDIO_DEVICE_TYPE_BIS;
	init_param.device_info.tx_chan.bis_chan.handle = chan->handle;
	init_param.device_info.tx_chan.bis_chan.id = chan->id;
	init_param.device_info.tx_chan.timeline_owner = chan_info.chan;

	if (chan_info.duration == 7) {
		audio_policy_set_nav_frame_size_us(7500);
	} else {
		audio_policy_set_nav_frame_size_us(10000);
	}

	audio_input->capture_player = media_player_open(&init_param);
	if (!audio_input->capture_player) {
		goto exit;
	}

	audio_input->stream[0] = source_stream;
	audio_input->stream[1] = source_stream2;
	audio_input->input_stream = input_stream;
	SYS_LOG_INF("%p\n", audio_input->capture_player);

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

int audio_input_start_capture(void)
{
	struct audio_input_app_t *audio_input = audio_input_get_app();

	SYS_LOG_INF("%p\n", audio_input->capture_player );

	if (!audio_input->capture_player) {
		SYS_LOG_INF("not ready\n");
		return -EINVAL;
	}

	if (audio_input->capture_player_run) {
		SYS_LOG_INF("already\n");
		return -EINVAL;
	}
#ifdef CONFIG_PLAYTTS
	tts_manager_wait_finished(false);
#endif

	media_player_play(audio_input->capture_player);
	/*
	 * NOTE: sleep for resample page miss, capture/playback are
	 * shared, so only need to sleep once.
	 */
	if (!audio_input->capture_player_load) {
		os_sleep(OS_MSEC(10));
	}
	audio_input->capture_player_load = 1;

	audio_input_broadcast_source_stream_set(1);

	audio_input->capture_player_run = 1;

	btif_broadcast_source_set_retransmit(audio_input->chan, audio_input->irc);
	SYS_LOG_INF("broadcast retransmit:%d\n", audio_input->irc);

	//capture is started ahead, but playback is waiting.
	if (audio_input->playback_player_run) {
		SYS_LOG_INF("playback record start %p\n",
			    audio_input->playback_player);
		media_player_trigger_audio_record_start(audio_input->playback_player);
	}

	return 0;
}

int audio_input_stop_capture(void)
{
	struct audio_input_app_t *audio_input = audio_input_get_app();
	int i;

	if (!audio_input->capture_player || !audio_input->capture_player_run) {
		SYS_LOG_INF("not ready\n");
		return -EINVAL;
	}

	audio_input_broadcast_source_stream_set(0);

	if (audio_input->capture_player_load) {
		media_player_stop(audio_input->capture_player);
	}
#ifdef CONFIG_PLAYTTS
	tts_manager_unlock();
#endif

	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		if (audio_input->stream[i]) {
			stream_close(audio_input->stream[i]);
		}
	}

	if (audio_input->input_stream) {
		stream_close(audio_input->input_stream);
	}

	audio_input->capture_player_run = 0;

	SYS_LOG_INF("%p\n", audio_input->capture_player);

	return 0;
}

int audio_input_exit_capture(void)
{
	struct audio_input_app_t *audio_input = audio_input_get_app();
	int i;

	if (!audio_input->capture_player) {
		SYS_LOG_INF("already\n");
		return -EALREADY;
	}

	media_player_close(audio_input->capture_player);
	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		if (audio_input->stream[i]) {
			stream_destroy(audio_input->stream[i]);
			audio_input->stream[i] = NULL;
		}
	}
	if (audio_input->input_stream) {
		stream_destroy(audio_input->input_stream);
		audio_input->input_stream = NULL;
	}

	SYS_LOG_INF("%p\n", audio_input->capture_player );

	audio_input->capture_player = NULL;
	audio_input->capture_player_load = 0;
	audio_input->capture_player_run = 0;

	return 0;
}
#endif
