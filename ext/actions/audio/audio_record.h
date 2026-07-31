/*
 * Copyright (c) 2016 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief audio record.
*/
#ifndef __AUDIO_RECORD_H__
#define __AUDIO_RECORD_H__

#include <audio_system.h>
#include <audio_policy.h>
#include <stream.h>

#define AUDIO_RECORD_PCM_BUFF_SIZE  1024

struct audio_record_param_ext {
	io_stream_t clone_stream;
};

struct audio_record_t *audio_record_create(u8_t stream_type, int sample_rate_input, int sample_rate_output,
									u8_t format, u8_t audio_mode, void *outer_stream, uint8_t media_format);

int audio_record_destory(struct audio_record_t *handle);
int audio_record_start(struct audio_record_t *handle);
int audio_record_stop(struct audio_record_t *handle);
int audio_record_pause(struct audio_record_t *handle);
int audio_record_resume(struct audio_record_t *handle);
int audio_record_write(struct audio_record_t *handle, unsigned char *buf, int num);
int audio_record_flush(struct audio_record_t *handle);
int audio_record_set_volume(struct audio_record_t *handle, int volume);
int audio_record_get_volume(struct audio_record_t *handle);
int audio_record_set_sample_rate(struct audio_record_t *handle, int sample_rate);
int audio_record_get_sample_rate(struct audio_record_t *handle);
int audio_record_channel_set_input(struct audio_record_t *handle);
io_stream_t audio_record_get_stream(struct audio_record_t *handle);
int audio_record_set_waitto_start(struct audio_record_t *handle, bool wait);
int audio_record_is_waitto_start(struct audio_record_t *handle);
uint64_t audio_record_get_samples_cnt(struct audio_record_t *handle);
int audio_record_set_reference_stream(struct audio_record_t *handle, void *ref_stream);
#endif



