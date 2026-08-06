/*
 * Copyright (c) 2024 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief usound media
 */

#include "usound.h"
#include "tts_manager.h"
#include "buffer_stream.h"
#include "ringbuff_stream.h"
#include "media_mem.h"

#include <audio_record.h>
#include <media_type.h>

static void bms_uac_event_notify(u32_t event, void *data, u32_t len,
				 void *user_data)
{
#ifdef CONFIG_USOUND_FEATURE_RESTART
	if (event == PLAYBACK_EVENT_DATA_INDICATE) {
		bms_uac_player_reset_trigger();
	}
#endif
}

#ifdef CONFIG_USOUND_MIC
static io_stream_t _usound_create_uploadstream(void)
{
	io_stream_t upload_stream;
	int ret = 0;

#if 0
	upload_stream = usb_audio_upload_stream_create();
	if (!upload_stream) {
		goto exit;
	}

	ret = stream_open(upload_stream, MODE_IN_OUT);
	if (ret) {
		stream_destroy(upload_stream);
		upload_stream = NULL;
		goto exit;
	}
#endif
	upload_stream = ringbuff_stream_create_ext(media_mem_get_cache_pool
				       (USB_UPLOAD_CACHE, AUDIO_STREAM_USOUND),
				       media_mem_get_cache_pool_size
				       (USB_UPLOAD_CACHE, AUDIO_STREAM_USOUND));

	if (!upload_stream) {
		goto exit;
	}

	ret =  stream_open(upload_stream, MODE_IN_OUT);
	if (ret) {
		stream_destroy(upload_stream);
		upload_stream = NULL;
		goto exit;
	}
exit:
	SYS_LOG_INF(" %p\n", upload_stream);
	return	upload_stream;
}

void usound_start_capture(void)
{
#if 0
	struct usound_app_t *usound = usound_get_app();
	media_init_param_t init_param;
	io_stream_t upload_stream = NULL;

	if (!usound)
		return;

	if (usound->upload_capture_player) {
		SYS_LOG_INF(" already open\n");
		return;
	}

	memset(&init_param, 0, sizeof(media_init_param_t));

	upload_stream = _usound_create_uploadstream();
	if (!upload_stream) {
		goto err_exit;
	}

	init_param.type = MEDIA_SRV_TYPE_CAPTURE;
	init_param.capture_format = PCM_TYPE;
	init_param.stream_type = AUDIO_STREAM_USOUND;
	init_param.capture_input_stream = NULL;
	init_param.capture_output_stream = upload_stream;
	init_param.capture_channels_input = 1;
	init_param.capture_channels_output = 1;
	init_param.capture_sample_rate_input = CONFIG_USB_AUDIO_DEVICE_SOURCE_SAM_FREQ_UPLOAD / 1000;
	init_param.capture_sample_rate_output = CONFIG_USB_AUDIO_DEVICE_SOURCE_SAM_FREQ_UPLOAD / 1000;
	init_param.capture_sample_bits = 16;

	usound->usound_upload_stream = upload_stream;

	usound->upload_capture_player = media_player_open(&init_param);
	if (!usound->upload_capture_player) {
		SYS_LOG_ERR("open failed\n");
		goto err_exit;
	}

	media_player_play(usound->upload_capture_player);

	SYS_LOG_INF("Player %p", usound->upload_capture_player);
	return;
#endif
	u8_t sample_rate = CONFIG_USB_AUDIO_DEVICE_SOURCE_SAM_FREQ_UPLOAD / 1000;
	struct usound_app_t *usound = usound_get_app();
	io_stream_t upload_stream = NULL;

	if (!usound)
		return;

	if (usound->record_handle) {
		SYS_LOG_INF(" already open\n");
		return;
	}

	upload_stream = _usound_create_uploadstream();
	usb_audio_upload_stream_set(upload_stream);
	if (!upload_stream) {
		goto err_exit;
	}

	usound->usound_upload_stream = upload_stream;

	usound->record_handle = audio_record_create(AUDIO_STREAM_USOUND, sample_rate, sample_rate, AUDIO_FORMAT_PCM_16_BIT, AUDIO_MODE_MONO, stream_get_ringbuffer(upload_stream), PCM_TYPE);
	audio_record_start(usound->record_handle);
	return;
err_exit:
	if (upload_stream) {
		stream_close(upload_stream);
		stream_destroy(upload_stream);
	}

	SYS_LOG_INF("failed\n");
}

void usound_stop_capture(void)
{
#if 0
	struct usound_app_t *usound = usound_get_app();

	if (!usound)
		return;

	if (!usound->upload_capture_player)
		return;

	if (usound->usound_upload_stream)
		stream_close(usound->usound_upload_stream);

	media_player_stop(usound->upload_capture_player);

	media_player_close(usound->upload_capture_player);

	SYS_LOG_INF(" %p ok\n", usound->upload_capture_player);

	usound->upload_capture_player = NULL;

	if (usound->usound_upload_stream) {
		stream_destroy(usound->usound_upload_stream);
		usound->usound_upload_stream = NULL;
	}
#endif
	struct usound_app_t *usound = usound_get_app();

	if (!usound)
		return;

	if (!usound->record_handle)
		return;

	if (usound->usound_upload_stream)
		stream_close(usound->usound_upload_stream);

	audio_record_stop(usound->record_handle);
	audio_record_destory(usound->record_handle);
	usound->record_handle = NULL;

	if (usound->usound_upload_stream) {
		usb_audio_upload_stream_set(NULL);
		stream_destroy(usound->usound_upload_stream);
		usound->usound_upload_stream = NULL;
	}
}
#endif

static io_stream_t usound_create_playback_input_stream(void)
{
	int ret = 0;
	io_stream_t input_stream = NULL;

	input_stream =
	    ringbuff_stream_create_ext(media_mem_get_cache_pool
				       (INPUT_PLAYBACK, AUDIO_STREAM_USOUND),
				       media_mem_get_cache_pool_size
				       (INPUT_PLAYBACK, AUDIO_STREAM_USOUND));
	if (!input_stream) {
		goto exit;
	}

	ret = stream_open(input_stream,
			  MODE_IN_OUT | MODE_READ_BLOCK | MODE_BLOCK_TIMEOUT);
	if (ret) {
		stream_destroy(input_stream);
		input_stream = NULL;
		goto exit;
	}

 exit:
	SYS_LOG_INF(" %p\n", input_stream);
	return input_stream;
}

static void set_player_effect_output_mode(media_player_t * player)
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

#ifdef CONFIG_BMS_UAC_APP
	if (usound_is_bms_mode()) {
#ifdef CONFIG_BT_SELF_APP
		u8_t ch;
		ch = selfapp_get_channel();
		if (ch == 0) {
			mode = MEDIA_EFFECT_OUTPUT_DEFAULT;
		} else if (ch == 1) {
			mode = MEDIA_EFFECT_OUTPUT_L_ONLY;
		} else {
			mode = MEDIA_EFFECT_OUTPUT_R_ONLY;
		}
#endif
	}
#endif

	SYS_LOG_INF("%d\n", mode);
	if (mode != CONFIG_MEDIA_EFFECT_OUTMODE) {
		media_player_set_effect_output_mode(player, mode);
	}
}

void usound_start_playback(void)
{
	struct usound_app_t *usound = usound_get_app();
	int volume = 0;
	media_init_param_t init_param;
	io_stream_t input_stream = NULL;

	if (!usound)
		return;

#ifdef CONFIG_PLAYTTS
	tts_manager_wait_finished(false);
#endif

#if CONFIG_BMS_UAC_APP
	SYS_LOG_INF("mode %d", usound_is_bms_mode());
#endif

	if (usound->playback_player) {
		SYS_LOG_INF(" already open\n");
		return;
	}

	// audio_system_set_output_sample_rate(48);

	memset(&init_param, 0, sizeof(media_init_param_t));

	input_stream = usound_create_playback_input_stream();
	if (!input_stream) {
		goto err_exit;
	}

	init_param.type = MEDIA_SRV_TYPE_PLAYBACK;
	init_param.format = PCM_TYPE;
	init_param.stream_type = AUDIO_STREAM_USOUND;
	init_param.efx_stream_type = AUDIO_STREAM_USOUND;
	init_param.input_stream = input_stream;
	init_param.output_stream = NULL;
	init_param.support_tws = 0;
	init_param.dumpable = 1;
	init_param.channels = 2;
	init_param.sample_rate = CONFIG_USB_AUDIO_DEVICE_SINK_SAM_FREQ_DOWNLOAD / 1000;
	init_param.sample_bits = 32;
	init_param.event_notify_handle = bms_uac_event_notify;



#if CONFIG_BMS_UAC_APP
	if (usound_is_bms_mode() && usound->bms_source) {
		init_param.waitto_start = 1;
		init_param.bind_to_capture = 1;
		init_param.user_data = (void *)usound;
#if (BROADCAST_DURATION == BT_FRAME_DURATION_7_5MS)
		audio_policy_set_nav_frame_size_us(7500);
#else
		audio_policy_set_nav_frame_size_us(10000);
#endif
		audio_policy_set_bis_link_delay_ms(broadcast_get_bis_link_delay
						   (usound->qos));
	}
#endif

	usound->playback_player = media_player_open(&init_param);
	if (!usound->playback_player) {
		SYS_LOG_ERR("open failed\n");
		goto err_exit;
	}
	set_player_effect_output_mode(usound->playback_player);

	usound->usound_stream = init_param.input_stream;
	//For bms mode, set usound stream with playback input stream later after capture is started.
#if CONFIG_BMS_UAC_APP
	if (!usound_is_bms_mode()) {
		usb_audio_set_stream(usound->usound_stream);
	}

	if (usound_is_bms_mode() && 0 == usound->bms_source) {
		usb_audio_set_stream(usound->usound_stream);
	}
#else
	usb_audio_set_stream(usound->usound_stream);
#endif

#if CONFIG_BMS_UAC_APP
	if (usound_is_bms_mode() && 1 == usound->bms_source) {
		bt_manager_broadcast_stream_tws_sync_cb_register_1(usound->chan,
								   media_player_audio_track_trigger_callback,
								   audio_system_get_track
								   ());
	}
#endif

	media_player_fade_in(usound->playback_player, 80);
	media_player_play(usound->playback_player);

	volume = audio_system_get_stream_volume(AUDIO_STREAM_USOUND);
	media_player_set_volume(usound->playback_player, volume, volume);

	usound->playback_player_run = 1;
	SYS_LOG_INF("Player %p", usound->playback_player);
	return;
 err_exit:
	if (input_stream) {
		stream_close(input_stream);
		stream_destroy(input_stream);
	}
	SYS_LOG_INF("failed\n");
}

void usound_stop_playback(void)
{
	struct usound_app_t *usound = usound_get_app();

	if (!usound)
		return;

	if (!usound->playback_player)
		return;

#if CONFIG_BMS_UAC_APP
	if (usound_is_bms_mode()) {
		bt_manager_broadcast_stream_tws_sync_cb_register_1(usound->chan,
								   NULL, NULL);
	}
#endif

	media_player_fade_out(usound->playback_player, 60);

	/** reserve time to fade out*/
	os_sleep(audio_policy_get_bis_link_delay_ms() + 80);

	if (usound->usound_stream) {
		usb_audio_set_stream(NULL);
		stream_close(usound->usound_stream);
	}

	media_player_stop(usound->playback_player);

	media_player_close(usound->playback_player);

	SYS_LOG_INF(" %p ok\n", usound->playback_player);

	usound->playback_player = NULL;

#if CONFIG_BMS_UAC_APP
	if (usound_is_bms_mode()) {
		audio_policy_set_bis_link_delay_ms(0);
	}
#endif

	if (usound->usound_stream) {
		stream_destroy(usound->usound_stream);
		usound->usound_stream = NULL;
	}

	usound->playback_player_run = 0;
	//clear restart at player stop.
	usound->restart = 0;

}

#ifdef CONFIG_BMS_UAC_APP
static io_stream_t broadcast_create_source_stream(int mem_type, int block_num)
{
	io_stream_t stream = NULL;
	int buff_size;
	int ret;

	buff_size = media_mem_get_cache_pool_size_ext(mem_type, AUDIO_STREAM_USOUND, NAV_TYPE, BROADCAST_SDU, block_num);	// TODO: 3?
	if (buff_size <= 0) {
		goto exit;
	}

	stream =
	    ringbuff_stream_create_ext(media_mem_get_cache_pool
				       (mem_type, AUDIO_STREAM_USOUND),
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

static io_stream_t bms_uac_create_capture_input_stream(void)
{
	io_stream_t input_stream;
	int ret;

	input_stream =
	    ringbuff_stream_create_ext(media_mem_get_cache_pool
				       (INPUT_CAPTURE, AUDIO_STREAM_USOUND),
				       media_mem_get_cache_pool_size
				       (INPUT_CAPTURE, AUDIO_STREAM_USOUND));

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

static void bms_uac_broadcast_source_stream_set(uint8 enable_flag)
{
	struct usound_app_t *usound = usound_get_app();
	struct bt_broadcast_chan *chan;
	int i;

	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		chan = &usound->broad_chan[i];
		if (chan->handle == 0 && chan->id == 0) {
			continue;
		}
		if (enable_flag) {
			bt_manager_broadcast_source_stream_set(chan,
							       usound->stream
							       [i]);
		} else {
			bt_manager_broadcast_source_stream_set(chan, NULL);
		}
	}
}

int bms_uac_init_capture(void)
{
	struct usound_app_t *usound = usound_get_app();
	struct bt_broadcast_chan *chan;
	struct bt_broadcast_chan_info chan_info;
	io_stream_t source_stream = NULL;
	io_stream_t source_stream2 = NULL;
	io_stream_t input_stream = NULL;
	media_init_param_t init_param;
	int ret;

	if (NULL == usound) {
		return -1;
	}

	SYS_LOG_INF("");

	if (usound->capture_player) {
		SYS_LOG_INF("already\n");
		return -2;
	}

	chan = usound->chan;
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

	input_stream = bms_uac_create_capture_input_stream();
	if (!input_stream) {
		goto exit;
	}

	memset(&init_param, 0, sizeof(media_init_param_t));

	init_param.type = MEDIA_SRV_TYPE_CAPTURE;
	init_param.stream_type = AUDIO_STREAM_USOUND;
	init_param.efx_stream_type = AUDIO_STREAM_USOUND;
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

	usound->capture_player = media_player_open(&init_param);
	if (!usound->capture_player) {
		goto exit;
	}

	usound->stream[0] = source_stream;
	usound->stream[1] = source_stream2;
	usound->input_stream = input_stream;
	SYS_LOG_INF("%p opened\n", usound->capture_player);

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

int bms_uac_start_capture(void)
{
	struct usound_app_t *usound = usound_get_app();

	if (NULL == usound) {
		return -1;
	}

	if (!usound->capture_player) {
		SYS_LOG_INF("not ready\n");
		return -EINVAL;
	}

	if (usound->capture_player_run) {
		SYS_LOG_INF("already\n");
		return -EINVAL;
	}
#ifdef CONFIG_PLAYTTS
	tts_manager_wait_finished(false);
#endif

	media_player_play(usound->capture_player);
	/*
	 * NOTE: sleep for resample page miss, capture/playback are
	 * shared, so only need to sleep once.
	 */
	if (!usound->capture_player_load) {
		os_sleep(OS_MSEC(10));
	}
	usound->capture_player_load = 1;

	bms_uac_broadcast_source_stream_set(1);

	usound->capture_player_run = 1;

	btif_broadcast_source_set_retransmit(usound->chan, usound->irc);

	usb_audio_set_stream(usound->usound_stream);

	SYS_LOG_INF("%p played irc%d\n", usound->capture_player,usound->irc);

	return 0;
}

int bms_uac_stop_capture(void)
{
	struct usound_app_t *usound = usound_get_app();
	int i;

	if (!usound->capture_player || !usound->capture_player_run) {
		SYS_LOG_INF("not ready\n");
		return -EINVAL;
	}
	//bt_manager_broadcast_source_stream_set(&usound->broad_chan, NULL);
	bms_uac_broadcast_source_stream_set(0);

	if (usound->capture_player_load) {
		media_player_stop(usound->capture_player);
	}
#ifdef CONFIG_PLAYTTS
	tts_manager_unlock();
#endif

	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		if (usound->stream[i]) {
			stream_close(usound->stream[i]);
		}
	}

	if (usound->input_stream) {
		stream_close(usound->input_stream);
	}

	usound->capture_player_run = 0;

	SYS_LOG_INF("%p\n", usound->capture_player);

	return 0;
}

int bms_uac_exit_capture(void)
{
	struct usound_app_t *usound = usound_get_app();
	int i;

	if (!usound->capture_player) {
		SYS_LOG_INF("already\n");
		return -EALREADY;
	}

	media_player_close(usound->capture_player);
	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		if (usound->stream[i]) {
			stream_destroy(usound->stream[i]);
			usound->stream[i] = NULL;
		}
	}
	if (usound->input_stream) {
		stream_destroy(usound->input_stream);
		usound->input_stream = NULL;
	}

	SYS_LOG_INF("%p\n", usound->capture_player);

	usound->capture_player = NULL;
	usound->capture_player_load = 0;
	usound->capture_player_run = 0;

	return 0;
}
#endif
