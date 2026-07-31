/*
 * Copyright (c) 2016 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief audio stream.
*/

#include <os_common_api.h>
#include <mem_manager.h>
#include <media_mem.h>
#include <msg_manager.h>
#include <audio_hal.h>
#include <acts_cache.h>
#include <audio_system.h>
#include <ringbuff_stream.h>
#include <audio_track.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "audio_stream.h"

static int _audiostream_wait_flush_data(io_stream_t handle)
{
	int result = 0;
	audio_info_t *info = (audio_info_t *)handle->data;

	while (info->data_finished <= 2) {
		os_sleep(4);
		result++;
		if (result > 25) {
			break;
		}
	}

	if (result <= 25) {
		result = 0;
	}
	return result; 
}

static int audiostream_open(io_stream_t handle,stream_mode mode)
{
	audio_info_t *info = (audio_info_t *)handle->data;

	if(!info){
		return -EACCES;
	}

	return 0;
}

static int audiostream_read(io_stream_t handle, u8_t *buf, int num)
{
	int ret = 0;
	u8_t *data_ptr = NULL;
	audio_info_t *info = (audio_info_t *)handle->data;
	if (!info)
		return -EACCES;

	if (info->dma_reload) {
		ret = stream_read(info->pcm_stream, buf, num);

		handle->rofs = info->pcm_buff->head;
		handle->wofs = info->pcm_buff->tail;

		if (!ret) {
			info->data_finished++;
		} else {
			info->data_finished = 0;
		}
		if (ret != num) {
			SYS_LOG_DBG("want read %d bytes but only read %d bytes", num, ret);
		}		
	} else {
		if(acts_ringbuf_drop(info->pcm_buff, info->dma_send_len) != info->dma_send_len) {
			SYS_LOG_ERR("info->dma_send_len %d ", info->dma_send_len);
		}
		if(acts_ringbuf_length(info->pcm_buff) >= info->pcm_frame_size){
			data_ptr = acts_ringbuf_head_ptr(info->pcm_buff, NULL);
			hal_aout_write_data(info->audio_handle, data_ptr, info->pcm_frame_size);
			info->dma_send_len = info->pcm_frame_size;
		} else {
			info->dma_start = false;
		}
		ret = num;
	}
	return ret;
}

static int _audiostream_track_write(io_stream_t handle, u8_t *buf, int num)
{
	int ret = 0;
	u8_t *data_ptr = NULL;
	int data_len = 0;
	unsigned int key;

	audio_info_t *info = (audio_info_t *)handle->data;

	/** if pcm_buff_owner is true , need write data to pcm stream
		others, share with dsp out ring buff ,needn't wirte */
	if (info->pcm_buff_owner) {
		/** fill first block avoid tts noise*/
		if (handle->wofs < 960) {
			memset(buf, 0, num);
		}
		ret = stream_write(info->pcm_stream, buf, num);
	} else {
		/**reset pcm buffer */
		if (!buf) {
			//acts_ringbuf_reset(info->pcm_buff);
		}
		if (!info->stream_start)
			return ret;
	}

	printk("_audiostream_track_write dma_start %d pcm_frame_size %d \n",info->dma_start, info->pcm_frame_size);
	key = irq_lock();
	if (!info->dma_start) {
		data_len = acts_ringbuf_length(info->pcm_buff);
		if (data_len >= info->pcm_frame_size) {
			data_ptr = acts_ringbuf_head_ptr(info->pcm_buff, NULL);
			hal_aout_write_data(info->audio_handle, data_ptr, info->pcm_frame_size);
			if (info->dma_reload) {
				if (acts_ringbuf_drop(info->pcm_buff, info->pcm_frame_size) != info->pcm_frame_size) {
					SYS_LOG_ERR("info->pcm_frame_size %d acts_ringbuf_length %d ", info->pcm_frame_size, acts_ringbuf_length(info->pcm_buff) );
					return ret;
				}
			}
			info->dma_send_len = info->pcm_frame_size;
			info->dma_start = true;
		}
	}

	handle->rofs = info->pcm_buff->head;
	handle->wofs = info->pcm_buff->tail;
	irq_unlock(key);
	return ret;
}

static int _audiostream_record_write(io_stream_t handle, u8_t *buf,int num)
{
	int ret = 0;
	audio_info_t *info = (audio_info_t *)handle->data;

	ret = stream_write(info->pcm_stream, buf, num);

	handle->rofs = info->pcm_buff->head;
	handle->wofs = info->pcm_buff->tail;

	if (ret != num) {
		SYS_LOG_WRN("want write %d bytes but only write %d bytes", num, ret);
	}
	return ret;
}

static int audiostream_write(io_stream_t handle, u8_t *buf, int num)
{
	int ret = 0;
	audio_info_t *info = (audio_info_t *)handle->data;

	if (!info) {
		return -EACCES;
	}

	if (info->audio_type == AUDIO_STREAM_TRACK) {
		ret = _audiostream_track_write(handle, buf, num);
	} else {
		ret = _audiostream_record_write(handle, buf, num);
	}

	return ret;
}

static int audiostream_tell(io_stream_t handle)
{
	int res = 0;
	audio_info_t *info = (audio_info_t *)handle->data;

	if(!info || !info->pcm_stream)
		return -EACCES;

	if (info->audio_type == AUDIO_STREAM_TRACK) {
		res = acts_ringbuf_space(info->pcm_buff);
	} else {
		res = acts_ringbuf_length(info->pcm_buff);
	}

	return res ;
}

static int audiostream_get_length(io_stream_t handle)
{
	audio_info_t *info = (audio_info_t *)handle->data;

	if(!info || !info->pcm_stream)
		return -EACCES;

	return acts_ringbuf_length(info->pcm_buff);
}

static int audiostream_get_space(io_stream_t handle)
{
	audio_info_t *info = (audio_info_t *)handle->data;

	if(!info || !info->pcm_stream)
		return -EACCES;

	return acts_ringbuf_space(info->pcm_buff);
}

static int audiostream_close(io_stream_t handle)
{
	int res = 0;
	audio_info_t *info = (audio_info_t *)handle->data;

	if (!info) {
		return -EACCES;
	}

	if (info->audio_type == AUDIO_STREAM_TRACK) {
		if (_audiostream_wait_flush_data(handle)) {
			SYS_LOG_WRN("_audiostream_wait_flush_data time out");
		}
	}

	if (info->pcm_stream)
		stream_close(info->pcm_stream);
	SYS_LOG_INF("audiostream_close ok");
	return res;
}

static int audiostream_destroy(io_stream_t handle)
{
	int res = 0;
	audio_info_t *info = (audio_info_t *)handle->data;

	if(!info){
		return -EACCES;
	}
	if (info->pcm_stream)
		stream_destroy(info->pcm_stream);

	if (info->pcm_buff_owner && info->pcm_buff)
		mem_free(info->pcm_buff);

	mem_free(info);

	handle->data = NULL;
	return res;
}

static int audiostream_init(io_stream_t handle, void *param)
{
	int ret = 0;
	audio_info_t *info = NULL;
	audio_stream_init_param_t *init_param = (audio_stream_init_param_t *)param;

	info = mem_malloc(sizeof(audio_info_t));
	if (!info) {
		SYS_LOG_ERR("cache stream info malloc failed \n");
		ret = -ENOMEM;
		goto err_exit;
	}

	if (!init_param->audio_stream) {
		info->pcm_buff = mem_malloc(sizeof(struct acts_ringbuf));
		if (!info->pcm_buff)
			goto err_exit;

		if (init_param->audio_stream_type == AUDIO_STREAM_RECORD) {
			acts_ringbuf_init(info->pcm_buff,
				media_mem_get_cache_pool(INPUT_PCM, init_param->meida_type),
				media_mem_get_cache_pool_size(INPUT_PCM, init_param->meida_type));
		} else {
			acts_ringbuf_init(info->pcm_buff,
				media_mem_get_cache_pool(OUTPUT_PCM, init_param->meida_type),
				media_mem_get_cache_pool_size(OUTPUT_PCM, init_param->meida_type));
		}

		init_param->audio_stream = info->pcm_buff;
		info->pcm_buff_owner = 1;
	} else {
		info->pcm_buff = init_param->audio_stream;
		info->pcm_buff_owner = 0;
	}

	info->pcm_stream = ringbuff_stream_create(init_param->audio_stream);

	if (!info->pcm_stream) {
		ret = -EINVAL;
		goto err_exit;
	}

	if(stream_open(info->pcm_stream, MODE_IN_OUT)) {
		stream_destroy(info->pcm_stream);
		info->pcm_stream = NULL;
		ret = -EINVAL;
		goto err_exit;
	}

	info->audio_type = init_param->audio_stream_type;
	info->audio_mode = init_param->audio_stream_mode;
	info->channel_mode = init_param->audio_channel_mode;
	info->audio_handle = init_param->audio_handle;
	info->pcm_frame_size = init_param->audio_pcm_frame_size;
	info->dma_start = 0;

	/**if used asrc, not used dma reload mode , others used dma reload mode */
	if ((info->channel_mode & AUDIO_DMA_RELOAD_MODE) == AUDIO_DMA_RELOAD_MODE) {
		info->dma_reload = 1;
	} else {
		info->dma_reload = 0;
	}

	SYS_LOG_DBG("stream audio_type : %d",info->audio_type);
	SYS_LOG_DBG("stream audio_mode : %d",info->audio_mode);
	SYS_LOG_DBG("stream audio_handle : %p ",info->audio_handle);

	handle->total_size = acts_ringbuf_size(info->pcm_buff);
	handle->data = info;
	return 0;

err_exit:
	if (!info)
		return ret;

	if (info->pcm_buff_owner && info->pcm_buff)
		acts_ringbuf_free(info->pcm_buff);

	mem_free(info);

	return ret;
}

int audiostream_start(io_stream_t handle)
{
	int ret = 0;
	audio_info_t *info = (audio_info_t *)handle->data;

	if (!info) {
		return -EACCES;
	}
	if (info->stream_start)
		return 0;

	info->stream_start = 1;

	if (info->audio_type == AUDIO_STREAM_TRACK) {
		ret = _audiostream_track_write(handle, NULL, 0);
	} else {
		ret = _audiostream_record_write(handle, NULL, 0);
	}

	return ret;
}

const stream_ops_t audio_stream_ops = {
	.init = audiostream_init,
	.open = audiostream_open,
	.read = audiostream_read,
	.seek = NULL,
	.tell = audiostream_tell,
	.get_length = audiostream_get_length,
	.get_space = audiostream_get_space,
	.write = audiostream_write,
	.close = audiostream_close,
	.destroy = audiostream_destroy,
};

io_stream_t audio_stream_create(audio_stream_init_param_t *param)
{
	return stream_create(&audio_stream_ops, param);
}