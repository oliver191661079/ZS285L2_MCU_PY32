/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */
 
/**
 * @file audio stream interface
 */

#ifndef __AUDIO_STREAM_H__
#define __AUDIO_STREAM_H__
#include <stream.h>
#include <acts_ringbuf.h>

typedef enum 
{
	AUDIO_TYPE_IN,
	AUDIO_TYPE_OUT,
} audio_type_e;

typedef enum 
{
	AUDIO_DMA_MODE = 1,
	AUDIO_DSP_MODE = 2,
	AUDIO_ASRC_MODE = 4,
	AUDIO_DMA_RELOAD_MODE = 8,
} audio_channel_mode_e;

typedef struct 
{
	u8_t meida_type;
	u8_t audio_stream_type;
	u8_t audio_stream_mode;
	u8_t audio_channel_mode;
	u16_t audio_pcm_frame_size;
	void *audio_handle;
	void *audio_stream;
} audio_stream_init_param_t;

/**
 * @brief create audio buffer stream , return stream handle
 *
 * This routine provides create stream ,and return stream handle.
 * and stream state is  STATE_INIT
 *
 * @param param create stream parama
 *
 * @return stream handle if create stream success
 * @return NULL  if create stream failed
 */
 
io_stream_t audio_stream_create(audio_stream_init_param_t *param);

/**
 * @} end defgroup buffer_stream_apis
 */


#endif



