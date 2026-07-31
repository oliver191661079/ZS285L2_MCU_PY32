/*
 * Copyright (c) 2022 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief system app shell.
 */
#include <os_common_api.h>
#include <mem_manager.h>
#include <msg_manager.h>
#include <stdio.h>
#include <init.h>
#include <string.h>
#include <kernel.h>
#include <stdlib.h>
#include <media_player.h>
#include <acts_ringbuf.h>
#include <soc_dsp.h>
#include <dsp_hal_defs.h>
#include <audio_record.h>

#ifdef CONFIG_MUSIC_EXTERNAL_EFFECT_MIC_STREAM

typedef struct {
	media_player_t * media_player;
	struct audio_record_t *audio_record;
	struct acts_ringbuf mic_rbuf;
	u8_t mic_data[512*4];
	short started;
} ex_mic_t;

static ex_mic_t ext_mic_inner = {0};
static bool _ex_check_mic_stream(int stream_type)
{
	bool mic_stream_enable = false;

	switch (stream_type) {
	case AUDIO_STREAM_MUSIC:
	case AUDIO_STREAM_SOUNDBAR:
	case AUDIO_STREAM_LE_AUDIO:
		mic_stream_enable = true;
		break;
	default:
		break;
	}

	return mic_stream_enable;
}

int ex_mic_stream_start(media_player_t *player, int stream_type)
{
	ex_mic_t *p = &ext_mic_inner;
	u8_t sample_rate = 48;
	uint8_t audio_mode = AUDIO_MODE_MONO;
	uint8_t audio_format = AUDIO_FORMAT_PCM_32_BIT;
	u8_t mic_stream_type = AUDIO_STREAM_MIC_IN;
	u8_t media_format = PCM_TYPE;
	int ret = 0;

	if (!_ex_check_mic_stream(stream_type))
		return 0;

	if (player && !p->started) {
		memset(p, 0, sizeof(ex_mic_t));
		acts_ringbuf_init(&p->mic_rbuf, p->mic_data, sizeof(p->mic_data));

		printk("ext_mic_stream buf:%p\n", &p->mic_rbuf);

		p->audio_record = audio_record_create(mic_stream_type,
						sample_rate,
						sample_rate,
						audio_format, audio_mode, (void *)&p->mic_rbuf, media_format);

		if (!p->audio_record) {
			printk("audio_record_create failed\n");
			goto exit;
		}

		ret = media_player_set_parameter(player, MEDIA_EFFECT_EXT_SET_MIC_PARAM, &p->mic_rbuf, sizeof(&p->mic_rbuf));
		if (ret >= 0) {
			p->started = 1;

			audio_record_start(p->audio_record);
			return 0;
		} else {
			audio_record_stop(p->audio_record);
			audio_record_destory(p->audio_record);
			p->audio_record = NULL;
		}
	}

exit:
	return -1;
}

int ex_mic_stream_stop()
{
	ex_mic_t *p = &ext_mic_inner;

	if (p->started) {
		if (p->audio_record) {
			audio_record_stop(p->audio_record);
			audio_record_destory(p->audio_record);
			p->audio_record = NULL;
		}
		p->started = 0;
	}

	return 0;
}

#endif
