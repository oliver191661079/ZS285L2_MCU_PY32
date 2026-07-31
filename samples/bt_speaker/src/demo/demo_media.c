/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "demo_app.h"
#include "tts_manager.h"
#include "buffer_stream.h"
#include "ringbuff_stream.h"
#include "media_mem.h"

int demo_get_status(void)
{
	struct demo_app_t *usound = demo_get_app();

	if (NULL == usound) {
		return USOUND_STATUS_NULL;
	}

	if (usound->playing){
		return USOUND_STATUS_PLAYING;
	} else {
		return USOUND_STATUS_PAUSED;
	}
}

static io_stream_t _demo_create_inputstream(void)
{
	int ret = 0;
	io_stream_t input_stream = NULL;

	input_stream = ringbuff_stream_create_ext(
			media_mem_get_cache_pool(INPUT_PLAYBACK, AUDIO_STREAM_USOUND),
			media_mem_get_cache_pool_size(INPUT_PLAYBACK, AUDIO_STREAM_USOUND));
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
	return	input_stream;
}

void demo_start_play(void)
{
	struct demo_app_t *usound = demo_get_app();
	int volume = 0;
	media_init_param_t init_param;
	io_stream_t input_stream = NULL;

	if (NULL == usound){
		return;
	}

	if (NULL != usound->player) {
		SYS_LOG_INF("already opened.\n");
		return;
	}

	input_stream = _demo_create_inputstream();
	if (!input_stream) {
		SYS_LOG_ERR("Failed to create input stream.\n");
		return;
	}

#ifdef CONFIG_PLAYTTS
	tts_manager_wait_finished(true);
#endif

	// audio_system_set_output_sample_rate(48);

	memset(&init_param, 0, sizeof(media_init_param_t));
	init_param.stream_type = AUDIO_STREAM_USOUND;
	init_param.efx_stream_type = AUDIO_STREAM_USOUND;
	init_param.format = PCM_TYPE;
	init_param.input_stream = input_stream;
	init_param.output_stream = NULL;
	init_param.event_notify_handle = NULL;
	init_param.channels = 2;
	init_param.sample_rate = 48;
	init_param.sample_bits = 32;
	init_param.type = MEDIA_SRV_TYPE_PLAYBACK;
	init_param.dumpable = true;
	usound->player = media_player_open(&init_param);
	if (NULL == usound->player) {
		SYS_LOG_ERR("Failed to open player\n");
		if (input_stream) {
			stream_close(input_stream);
			stream_destroy(input_stream);
		}
#ifdef CONFIG_PLAYTTS
		tts_manager_unlock();
#endif
		return;
	}

	usound->usound_stream = input_stream;
	usb_audio_set_stream(usound->usound_stream);

	media_player_fade_in(usound->player, 80);
	media_player_play(usound->player);

	volume = audio_system_get_stream_volume(AUDIO_STREAM_USOUND);
	media_player_set_volume(usound->player, volume, volume);

	SYS_LOG_INF("Player %p, srv %p\n", usound->player, usound->player->media_srv_handle);
	return;
}

void demo_stop_play(void)
{
	struct demo_app_t *usound = demo_get_app();

	if (NULL == usound) {
		return;
	}

	if (NULL == usound->player) {
		return;
	}

	SYS_LOG_INF("%p\n", usound->player);
#ifdef CONFIG_PLAYTTS
	tts_manager_unlock();
#endif

	media_player_fade_out(usound->player, 60);
	/* wait to fade out */
	os_sleep(60);

	if (NULL != usound->usound_stream) {
		usb_audio_set_stream(NULL);
		stream_close(usound->usound_stream);
	}

	media_player_stop(usound->player);
	media_player_close(usound->player);
	usound->player = NULL;

	if (NULL != usound->usound_stream) {
		stream_destroy(usound->usound_stream);
		usound->usound_stream = NULL;
	}
}

