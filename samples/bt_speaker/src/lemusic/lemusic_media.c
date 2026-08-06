/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lemusic.h"
#include "media_mem.h"
#include <ringbuff_stream.h>
#include "tts_manager.h"
#include "ui_manager.h"
#include "app_tws.h"

#ifdef CONFIG_BT_SELF_APP
#include "selfapp_api.h"
#endif

#ifdef CONFIG_ACT_EVENT
#include <app_act_event_id.h>
#include <logging/log_core.h>
LOG_MODULE_DECLARE(main, CONFIG_ACT_EVENT_APP_COMPILE_LEVEL);
#endif

void lemusic_bms_event_notify(u32_t event, void *data, u32_t len, void *user_data)
{
	if (event == PLAYBACK_EVENT_DATA_INDICATE) {
		lemusic_bms_player_reset_trigger();
	}
}

static io_stream_t lemusic_create_playback_stream(struct bt_audio_chan_info *info)
{
	int ret = 0;
	io_stream_t input_stream = NULL;
	int size;

	size = info->sdu;
	if (size & 0x01) {
		size++;
		SYS_LOG_INF("sdu resize: %d %d\n", info->sdu, size);
	}

	int buff_size = media_mem_get_cache_pool_size_ext(INPUT_PLAYBACK, AUDIO_STREAM_LE_AUDIO,
		bt_manager_audio_codec_type(info->format), size, 38);
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

static io_stream_t lemusic_create_source_stream(struct bt_audio_chan_info *info)
{
	io_stream_t stream = NULL;
	int ret;

	int buff_size = media_mem_get_cache_pool_size_ext(OUTPUT_CAPTURE, AUDIO_STREAM_LE_AUDIO,
		bt_manager_audio_codec_type(info->format), info->sdu, 2);
	if (buff_size <= 0) {
		goto exit;
	}
	stream = ringbuff_stream_create_ext(
			media_mem_get_cache_pool(OUTPUT_CAPTURE, AUDIO_STREAM_LE_AUDIO),
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

int lemusic_init_capture(void)
{
	struct lemusic_slave_device *slave = &lemusic_get_app()->slave;
	struct bt_audio_chan *chan = &slave->source_chan;
	struct bt_audio_chan_info chan_info;
	io_stream_t source_stream = NULL;
	media_init_param_t init_param;
	int ret;

	SYS_LOG_INF("%p\n", slave->capture_player);

	if (slave->capture_player) {
		SYS_LOG_INF("already\n");
		return -EALREADY;
	}

	ret = bt_manager_audio_stream_info(chan, &chan_info);
	if (ret) {
		return ret;
	}

	/* wait until start_capture */
#ifdef CONFIG_PLAYTTS
	tts_manager_wait_finished(false);
#endif

	source_stream = lemusic_create_source_stream(&chan_info);
	if (!source_stream) {
		goto err_exit;
	}

	memset(&init_param, 0, sizeof(media_init_param_t));

	init_param.type = MEDIA_SRV_TYPE_CAPTURE;
	init_param.stream_type = AUDIO_STREAM_LE_AUDIO;
	init_param.efx_stream_type = AUDIO_STREAM_LE_AUDIO;

	init_param.capture_format = bt_manager_audio_codec_type(chan_info.format);
	init_param.capture_sample_rate_input = chan_info.sample;
	init_param.capture_sample_rate_output = chan_info.sample;
	init_param.capture_sample_bits = 16;
	// TODO:
#ifdef CONFIG_AUDIO_VOICE_HARDWARE_REFERENCE
	init_param.capture_channels_input = chan_info.channels + 1;
#else
	init_param.capture_channels_input = chan_info.channels;
#endif
	init_param.capture_channels_output = chan_info.channels;
	init_param.capture_bit_rate = chan_info.kbps;
	init_param.capture_input_stream = NULL;
	init_param.capture_output_stream = source_stream;
	init_param.waitto_start = 1;
	init_param.device_info.tx_chan.validate = 1;
	init_param.device_info.tx_chan.type = MEDIA_AUDIO_DEVICE_TYPE_CIS;
	init_param.device_info.tx_chan.bt_chan.handle = chan->handle;
	init_param.device_info.tx_chan.bt_chan.id = chan->id;
	init_param.device_info.tx_chan.timeline_owner = chan_info.chan;

	SYS_LOG_INF("format: %d, sample_rate: %d, channels: %d, kbps: %d\n",
		chan_info.format, chan_info.sample, chan_info.channels,
		chan_info.kbps);

	slave->capture_player = media_player_open(&init_param);
	if (!slave->capture_player) {
		goto err_exit;
	}

	slave->source_stream = source_stream;

	SYS_LOG_INF("%p\n", slave->capture_player);

	return 0;

err_exit:
	if (source_stream) {
		stream_close(source_stream);
		stream_destroy(source_stream);
	}

	SYS_LOG_ERR("open failed\n");
	return -EINVAL;
}

int lemusic_start_capture(void)
{
	struct lemusic_slave_device *slave = &lemusic_get_app()->slave;

	SYS_LOG_INF("%p\n", slave->capture_player);

	if (!slave->capture_player) {
		SYS_LOG_INF("not ready\n");
		return -EINVAL;
	}

	if (slave->capture_player_run) {
		SYS_LOG_INF("already\n");
		return -EINVAL;
	}

#ifdef CONFIG_PLAYTTS
	tts_manager_lock();
	tts_manager_wait_finished(false);
#endif

	media_player_play(slave->capture_player);
	/*
	 * NOTE: sleep for resample page miss, capture/playback are
	 * shared, so only need to sleep once.
	 */
	if (!slave->playback_player_load) {
		os_sleep(OS_MSEC(10));
	}
	slave->capture_player_load = 1;

	bt_manager_audio_source_stream_set_single(&slave->source_chan,
					slave->source_stream, 0);

	slave->capture_player_run = 1;

	return 0;
}

int lemusic_stop_capture(void)
{
	struct lemusic_slave_device *slave = &lemusic_get_app()->slave;

	SYS_LOG_INF("%p\n", slave->capture_player);

	if (!slave->capture_player || !slave->capture_player_run) {
		SYS_LOG_INF("not ready\n");
		return -EINVAL;
	}

	bt_manager_audio_source_stream_set_single(&slave->source_chan,
					NULL, 0);

	if (slave->capture_player_load) {
		media_player_stop(slave->capture_player);
	}

#ifdef CONFIG_PLAYTTS
	tts_manager_unlock();
#endif

	if (slave->source_stream) {
		stream_close(slave->source_stream);
	}

	slave->adc_to_bt_started = 0;
	slave->capture_player_run = 0;

	return 0;
}

int lemusic_exit_capture(void)
{
	struct lemusic_slave_device *slave = &lemusic_get_app()->slave;

	SYS_LOG_INF("%p\n", slave->capture_player);

	if (!slave->capture_player) {
		SYS_LOG_INF("already\n");
		return -EALREADY;
	}

	media_player_close(slave->capture_player);

	if (slave->source_stream) {
		stream_destroy(slave->source_stream);
		slave->source_stream = NULL;
	}

	slave->capture_player = NULL;
	slave->capture_player_load = 0;
	slave->capture_player_run = 0;

	return 0;
}


/* find the first active sink chan */
void lemusic_set_sink_chan_stream(io_stream_t stream)
{
	struct lemusic_slave_device *slave = &lemusic_get_app()->slave;
	struct bt_audio_chan *chan;
	int i;

	SYS_LOG_INF("stream:%p\n", stream);

	for (i = 0; i < NUM_OF_SINK_CHAN; i++) {
		chan = &slave->sink_chans[i];
		if (chan->handle == 0) {
			continue;
		}

		// TODO: need to check if active or not?!
		bt_manager_audio_sink_stream_set(chan, stream);
	}
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

	SYS_LOG_INF("%d\n", mode);
	if (mode != CONFIG_MEDIA_EFFECT_OUTMODE) {
		media_player_set_effect_output_mode(player, mode);
	}
}

int lemusic_init_playback(void)
{
	struct lemusic_bms_device *bms = &lemusic_get_app()->bms;
	struct lemusic_slave_device *slave = &lemusic_get_app()->slave;
	struct bt_audio_chan *chan;
	struct bt_audio_chan_info chan_info;
	io_stream_t input_stream = NULL;
	media_init_param_t init_param;
	int ret;

	SYS_LOG_INF("%p", slave->playback_player);

	if (slave->playback_player) {
		// TODO: check current playback_player's cis stream_count???
#if 1	/* stop and exit current playback */
		lemusic_stop_playback();
		lemusic_exit_playback();
		SYS_LOG_INF("reinit %d\n", slave->num_of_sink_chan);
#else
		SYS_LOG_INF("already\n");
		return -EALREADY;
#endif
	}

	chan = slave->active_sink_chan;
	if (!chan) {
		SYS_LOG_INF("no chan\n");
		return -EINVAL;
	}

	ret = bt_manager_audio_stream_info(chan, &chan_info);
	if (ret) {
		return ret;
	}
	SYS_LOG_INF("f%d,s%d,ch%d,b%d,d%d,m%d,t%d,r%d,i%dms,du%dms\n",
		    chan_info.format,
		    chan_info.sample,
		    chan_info.channels,
		    chan_info.kbps,
		    chan_info.delay,
		    chan_info.max_latency,
		    chan_info.target_latency,
		    chan_info.rtn,
		    chan_info.interval / 1000,
		    chan_info.duration);

#ifdef CONFIG_PLAYTTS
	tts_manager_wait_finished(false);
#endif
	lemusic_view_show_play_paused(true);

	input_stream = lemusic_create_playback_stream(&chan_info);
	if (!input_stream) {
		goto err_exit;
	}

	memset(&init_param, 0, sizeof(media_init_param_t));
	init_param.type = MEDIA_SRV_TYPE_PLAYBACK;

	init_param.format = bt_manager_audio_codec_type(chan_info.format);
	init_param.stream_type = AUDIO_STREAM_LE_AUDIO;
	init_param.efx_stream_type = AUDIO_STREAM_LE_AUDIO;
	init_param.sample_rate = chan_info.sample;
	init_param.sample_bits = 32;
	init_param.input_stream = input_stream;
	init_param.dumpable = 1;
	if (slave->num_of_sink_chan == 2) {
		init_param.channels = 2;	/* force 2 channles */
		init_param.nav_channel_separate = 1;
	} else {
		init_param.channels = chan_info.channels;
	}
	init_param.bit_rate = chan_info.kbps;
	init_param.device_info.rx_chan.validate = 1;
	init_param.device_info.rx_chan.type = MEDIA_AUDIO_DEVICE_TYPE_CIS;
	init_param.device_info.rx_chan.bt_chan.handle = chan->handle;
	init_param.device_info.rx_chan.bt_chan.id = chan->id;
	init_param.device_info.rx_chan.timeline_owner = bt_manager_audio_get_tl_owner_chan(chan->handle);

	init_param.waitto_start = 1;

#if ENABLE_BROADCAST
	if(lemusic_get_auracast_mode()) {
		init_param.bind_to_capture = 1;
		init_param.event_notify_handle = lemusic_bms_event_notify;
		init_param.user_data = (void *)bms;
#if (BROADCAST_DURATION == BT_FRAME_DURATION_7_5MS)
		audio_policy_set_nav_frame_size_us(7500);
#else
		audio_policy_set_nav_frame_size_us(10000);
#endif
		audio_policy_set_bis_link_delay_ms(broadcast_get_bis_link_delay(bms->qos));
#if 1
		audio_policy_set_lea_start_threshold(chan_info.max_latency * 1000/chan_info.interval * chan_info.channels
			* slave->num_of_sink_chan + 8);
#endif
	}
#endif

	audio_system_set_output_sample_rate(48);

	SYS_EVENT_INF(EVENT_LEMUSIC_PLAYER_OPEN,slave->num_of_sink_chan,chan_info.kbps);

	slave->sink_stream = input_stream;

	slave->playback_player = media_player_open(&init_param);
	if (!slave->playback_player) {
		goto err_exit;
	}

	set_player_effect_output_mode(slave->playback_player);


	SYS_LOG_INF("num_of_sink_chan: %d, %p\n", slave->num_of_sink_chan,
		slave->playback_player);

	return 0;

err_exit:
	if (input_stream) {
		stream_close(input_stream);
		stream_destroy(input_stream);
	}

	SYS_LOG_ERR("open failed\n");
	return -EINVAL;
}

int lemusic_start_playback(void)
{
	struct lemusic_bms_device *bms = &lemusic_get_app()->bms;

	struct lemusic_slave_device *slave = &lemusic_get_app()->slave;

	SYS_LOG_INF("%p\n", slave->playback_player);

	if (!slave->playback_player) {
		SYS_LOG_INF("not ready\n");
		return -EINVAL;
	}

	if (slave->playback_player_run) {
		SYS_LOG_INF("already\n");
		return -EINVAL;
	}

	if(lemusic_get_auracast_mode()) {
		bt_manager_broadcast_stream_tws_sync_cb_register_1(bms->chan,
			media_player_audio_track_trigger_callback,audio_system_get_track());
	}

#ifdef CONFIG_EXTERNAL_DSP_DELAY
	media_player_fade_in(slave->playback_player, 150 + CONFIG_EXTERNAL_DSP_DELAY / 1000);
#else
	media_player_fade_in(slave->playback_player, 200);
#endif

	media_player_play(slave->playback_player);
	/*
	 * NOTE: sleep for resample page miss, capture/playback are
	 * shared, so only need to sleep once.
	 */
	if (!slave->capture_player_load) {
		os_sleep(OS_MSEC(10));
	}
	slave->playback_player_load = 1;

	if(lemusic_get_auracast_mode() == 0) {
		lemusic_set_sink_chan_stream(slave->sink_stream);
	}

	slave->playback_player_run = 1;

	return 0;
}

int lemusic_stop_playback(void)
{
	struct lemusic_slave_device *slave = &lemusic_get_app()->slave;
	struct lemusic_bms_device *bms = &lemusic_get_app()->bms;

	if (!slave->playback_player || !slave->playback_player_run) {
		SYS_LOG_INF("not ready\n");
		return -EINVAL;
	}
	SYS_LOG_INF("%p %d\n", slave->playback_player,slave->bt_to_dac_started);

	MSG_SEND_TIME_STAT_START();

	if(lemusic_get_auracast_mode()) {
		bt_manager_broadcast_stream_tws_sync_cb_register_1(bms->chan, NULL, NULL);
	}

	if(slave->bt_to_dac_started){
		media_player_fade_out(slave->playback_player, 60);
		/* reserve time to fade out */
		os_sleep(70);
	}

	if(lemusic_get_auracast_mode() == 0) {
		lemusic_set_sink_chan_stream(NULL);
	}

	lemusic_view_show_play_paused(false);

	if (slave->playback_player_load) {
		media_player_stop(slave->playback_player);
	}

	if (slave->sink_stream) {
		stream_close(slave->sink_stream);
	}

	slave->playback_player_run = 0;
#ifdef BIS_DELAY_BY_RX_START
	bms->time_update = 0;
#endif

	MSG_SEND_TIME_STAT_STOP();

#ifdef CONFIG_EXTERNAL_DSP_DELAY
	if(slave->bt_to_dac_started){
		// wait ext-dsp data flushed to avoid noise.
		os_sleep(CONFIG_EXTERNAL_DSP_DELAY / 1000);
	}
#endif
	slave->bt_to_dac_started = 0;

	SYS_LOG_INF("%p done\n", slave->playback_player);

	return 0;
}

int lemusic_exit_playback(void)
{
	struct lemusic_slave_device *slave = &lemusic_get_app()->slave;

	SYS_LOG_INF("%p\n", slave->playback_player);

	if (!slave->playback_player) {
		SYS_LOG_INF("already\n");
		return -EALREADY;
	}

	media_player_close(slave->playback_player);

	if (slave->sink_stream) {
		stream_destroy(slave->sink_stream);
		slave->sink_stream = NULL;
	}

	if(lemusic_get_auracast_mode()){
		audio_policy_set_bis_link_delay_ms(0);
#if 1
		audio_policy_set_lea_start_threshold(0);
#endif
	}

	slave->playback_player = NULL;
	slave->playback_player_load = 0;
	slave->playback_player_run = 0;
	slave->bt_to_dac_started = 0;
#ifdef BIS_DELAY_BY_RX_START
	lemusic_get_app()->bms.time_update = 0;
#endif

	return 0;
}

#define SUPPORT_RETRANSMIT_NUM_ADJUST
#define CAPTURE_AUDIO_TYPE    AUDIO_STREAM_LE_AUDIO

static io_stream_t broadcast_create_source_stream(int mem_type, int block_num)
{
	io_stream_t stream = NULL;
	int buff_size;
	int ret;
	struct lemusic_bms_device *bms = &lemusic_get_app()->bms;

	buff_size = media_mem_get_cache_pool_size_ext(mem_type, CAPTURE_AUDIO_TYPE, NAV_TYPE, bms->qos->max_sdu, block_num);	// TODO: 3?
	if (buff_size <= 0) {
		goto exit;
	}

	stream =
	    ringbuff_stream_create_ext(media_mem_get_cache_pool
				       (mem_type, CAPTURE_AUDIO_TYPE),
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
	SYS_LOG_INF("type %d stream %p\n", mem_type, stream);
	return stream;
}

static io_stream_t _lemusic_bms_create_capture_input_stream(void)
{
	io_stream_t input_stream;
	int ret;

	input_stream =
	    ringbuff_stream_create_ext(media_mem_get_cache_pool
				       (INPUT_CAPTURE, CAPTURE_AUDIO_TYPE),
				       media_mem_get_cache_pool_size
				       (INPUT_CAPTURE, CAPTURE_AUDIO_TYPE));

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

static void _lemusic_bms_broadcast_source_stream_set(uint8 enable_flag)
{
	struct lemusic_bms_device *bms = &lemusic_get_app()->bms;

	struct bt_broadcast_chan *chan;
	int i;

	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		chan = &bms->broad_chan[i];
		if (chan->handle == 0 && chan->id == 0) {
			continue;
		}
		if (enable_flag) {
			bt_manager_broadcast_source_stream_set(chan, bms->stream[i]);
		} else {
			bt_manager_broadcast_source_stream_set(chan, NULL);
		}

	}
}

int lemusic_bms_init_capture(void)
{
	struct lemusic_bms_device *bms = &lemusic_get_app()->bms;

	struct bt_broadcast_chan *chan;
	struct bt_broadcast_chan_info chan_info;
	io_stream_t source_stream = NULL;
	io_stream_t source_stream2 = NULL;
	io_stream_t input_stream = NULL;
	media_init_param_t init_param;
	int ret;

	SYS_LOG_INF("enter");

	if (NULL == bms) {
		return -EINVAL;
	}
	if (bms->player) {
		SYS_LOG_INF("already\n");
		return -EALREADY;
	}

	chan = bms->chan;
	ret = bt_manager_broadcast_stream_info(chan, &chan_info);
	if (ret) {
		return ret;
	}

	/* wait until start_capture */
#ifdef CONFIG_PLAYTTS
	tts_manager_wait_finished(false);
#endif

	source_stream = broadcast_create_source_stream(OUTPUT_CAPTURE, 6);
	if (!source_stream) {
		goto exit;
	}
#if (BROADCAST_NUM_BIS == 2)
	if(bms->num_of_broad_chan == NUM_OF_BROAD_CHAN){
		source_stream2 = broadcast_create_source_stream(OUTPUT_CAPTURE2, 6);
		if (!source_stream2) {
			goto exit;
		}
	}
#endif

	input_stream = _lemusic_bms_create_capture_input_stream();
	if (!input_stream) {
		goto exit;
	}

	memset(&init_param, 0, sizeof(media_init_param_t));

	init_param.type = MEDIA_SRV_TYPE_CAPTURE;
	init_param.stream_type = CAPTURE_AUDIO_TYPE;
	init_param.efx_stream_type = CAPTURE_AUDIO_TYPE;
	// TODO: broadcast_source no need to get chan_info???
	init_param.capture_format = NAV_TYPE;	// bt_manager_audio_codec_type(chan_info.format);
	init_param.capture_sample_rate_input = 48;	// chan_info.sample;
	init_param.capture_sample_rate_output = chan_info.sample;
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

	SYS_LOG_INF
	    ("f%d,s%d,c%d,b%d,du%d,h%d,id%d,ch%p,n%d\n",
	     chan_info.format, chan_info.sample, chan_info.channels,
	     chan_info.kbps, chan_info.duration, chan->handle, chan->id,
	     chan_info.chan,bms->num_of_broad_chan);

	bms->player = media_player_open(&init_param);
	if (!bms->player) {
		goto exit;
	}

	bms->stream[0] = source_stream;
	bms->stream[1] = source_stream2;
	bms->input_stream = input_stream;
	SYS_LOG_INF("%p\n", bms->player);

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

int lemusic_bms_check_le_audio_stream(u16_t pcm_time,u16_t normal_level,u8_t aps_status)
{
	struct lemusic_bms_device *bms = &lemusic_get_app()->bms;

	if (!bms->player_run) {
		return -EINVAL;
	}

	if (!bms->dsp_run) {
		if (pcm_time >= normal_level) {
			bms->dsp_run = 1;
		}
	}

	if (!bms->broadcast_source_enabled) {
		return 0;
	}

	if (normal_level <= 0) {
		return 0;
	}

	return 0;
}

int lemusic_bms_start_capture(void)
{
	struct lemusic_bms_device *bms = &lemusic_get_app()->bms;

	SYS_LOG_INF("%p\n", bms->player);

	if (!bms->player) {
		SYS_LOG_INF("not ready\n");
		return -EINVAL;
	}

	if (bms->player_run) {
		SYS_LOG_INF("already\n");
		return -EINVAL;
	}
#ifdef CONFIG_PLAYTTS
	tts_manager_wait_finished(false);
#endif

	media_player_play(bms->player);

	_lemusic_bms_broadcast_source_stream_set(1);

	bms->player_run = 1;

#ifdef SUPPORT_RETRANSMIT_NUM_ADJUST
	audio_asp_set_pcm_monitor_callback(lemusic_bms_check_le_audio_stream);
    if(app_tws_status_get_enable()) {
        btif_broadcast_source_set_retransmit(bms->chan,bms->irc);
    }
    else{
        btif_broadcast_source_set_retransmit(bms->chan,bms->irc-1);
    }
#endif

	SYS_LOG_INF("player:%p irc:%d\n", bms->player,bms->irc);
	return 0;
}

int lemusic_bms_stop_capture(void)
{
	struct lemusic_bms_device *bms = &lemusic_get_app()->bms;

	int i;

	SYS_LOG_INF("%p\n", bms->player);

	if (!bms->player || !bms->player_run) {
		SYS_LOG_INF("not ready\n");
		return -EINVAL;
	}

	_lemusic_bms_broadcast_source_stream_set(0);

	media_player_stop(bms->player);

	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		if (bms->stream[i]) {
			stream_close(bms->stream[i]);
		}
	}

	if (bms->input_stream) {
		stream_close(bms->input_stream);
	}

	bms->player_run = 0;
	bms->dsp_run = 0;

#ifdef SUPPORT_RETRANSMIT_NUM_ADJUST
	audio_asp_set_pcm_monitor_callback(NULL);
#endif

	return 0;
}

int lemusic_bms_exit_capture(void)
{
	struct lemusic_bms_device *bms = &lemusic_get_app()->bms;
	int i;

	SYS_LOG_INF("%p\n", bms->player);
	if (!bms->player) {
		SYS_LOG_INF("already\n");
		return -EALREADY;
	}

	media_player_close(bms->player);
	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		if (bms->stream[i]) {
			stream_destroy(bms->stream[i]);
			bms->stream[i] = NULL;
		}
	}
	if (bms->input_stream) {
		stream_destroy(bms->input_stream);
		bms->input_stream = NULL;
	}

	bms->player = NULL;
	bms->dsp_run = 0;
	bms->player_run = 0;
	bms->tx_start = 0;
	bms->tx_sync_start = 0;

#ifdef SUPPORT_RETRANSMIT_NUM_ADJUST
	audio_asp_set_pcm_monitor_callback(NULL);
#endif
	return 0;
}

