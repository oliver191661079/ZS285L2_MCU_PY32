/*
 * Copyright (c) 2017 Actions Semi Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief usb audio upload stream
*/

#include <os_common_api.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <file_stream.h>
#include <acts_ringbuf.h>
#include "media_mem.h"
#include "audio_system.h"

#include <usb_audio_inner.h>
#include <ringbuff_stream.h>

//#define DEBUG_DATA

/* cace info ,used for cache stream */
typedef struct
{
	struct acts_ringbuf *cache_buff;
} usb_audio_info_t;

#ifdef DEBUG_DATA
io_stream_t debug_file_stream = NULL;
#endif

io_stream_t usb_audio_upload_stream = NULL;

int usb_audio_stream_open(io_stream_t handle, stream_mode mode)
{
	usb_audio_info_t *info = (usb_audio_info_t *)handle->data;

	if(!info)
		return -EACCES;

	handle->mode = mode;

	return 0;
}

static int usb_audio_stream_get_length(io_stream_t handle)
{
	usb_audio_info_t *info = (usb_audio_info_t *)handle->data;

	if (!info)
		return -EACCES;

	return acts_ringbuf_length(info->cache_buff);
}

static int usb_audio_stream_get_space(io_stream_t handle)
{
	usb_audio_info_t *info = (usb_audio_info_t *)handle->data;

	if (!info)
		return -EACCES;

	return acts_ringbuf_space(info->cache_buff);
}

int usb_audio_stream_write(io_stream_t handle, unsigned char *buf,int len)
{
	usb_audio_info_t *info = (usb_audio_info_t *)handle->data;
	int ret = 0;

	if(!info)
		return -EACCES;

 	ret = acts_ringbuf_put(info->cache_buff, buf, len);

	if (ret != len) {
		SYS_LOG_WRN("want write %d bytes ,but only write %d bytes \n",len,ret);
	}
	return ret;
}

int usb_audio_stream_read(io_stream_t handle, unsigned char *buf,int len)
{
	usb_audio_info_t *info = (usb_audio_info_t *)handle->data;
	int ret = 0;

	if(!info)
		return -EACCES;

 	ret = acts_ringbuf_get(info->cache_buff, buf, len);

	if (ret != len) {
		SYS_LOG_DBG("want read %d bytes ,but only read %d bytes \n",len,ret);
	}

	return ret;
}


int usb_audio_stream_tell(io_stream_t handle)
{
	usb_audio_info_t *info = (usb_audio_info_t *)handle->data;

	if(!info)
		return -EACCES;

	return acts_ringbuf_space(info->cache_buff);
}

int usb_audio_stream_close(io_stream_t handle)
{
	usb_audio_info_t *info = (usb_audio_info_t *)handle->data;
	int res = 0;

	if (!info)
		return -EACCES;

#ifdef DEBUG_DATA
	if(debug_file_stream) {
		stream_close(debug_file_stream);
		stream_destroy(debug_file_stream);
		debug_file_stream= NULL;
	}
#endif

	return res;
}

int usb_audio_stream_flush(io_stream_t handle)
{
	usb_audio_info_t *info = (usb_audio_info_t *)handle->data;
	int res = 0;

	if (!info)
		return -EACCES;

	acts_ringbuf_drop_all(info->cache_buff);

	return res;
}

int usb_audio_stream_destroy(io_stream_t handle)
{
	int res = 0;
	SYS_IRQ_FLAGS flags;
	usb_audio_info_t *info = (usb_audio_info_t *)handle->data;

	if (!info)
		return -EACCES;

	sys_irq_lock(&flags);

	usb_audio_upload_stream = NULL;

	if (info->cache_buff) {
		mem_free(info->cache_buff);
		info->cache_buff = NULL;
	}

	mem_free(info);
	handle->data = NULL;

	sys_irq_unlock(&flags);

	return res;
}

int usb_audio_stream_init(io_stream_t handle, void *param)
{
	int res = 0;
	usb_audio_info_t *info = NULL;

	info = mem_malloc(sizeof(usb_audio_info_t));
	if (!info) {
		SYS_LOG_ERR("cache stream info malloc failed \n");
		return -ENOMEM;
	}

	info->cache_buff = mem_malloc(sizeof(struct acts_ringbuf));
	if (!info->cache_buff) {
		res = -ENOMEM;
		goto err_exit;
	}
#ifdef DEBUG_DATA
	debug_file_stream = file_stream_create("SD:data.pcm");
	if(!debug_file_stream) {
		SYS_LOG_ERR("debug_file_stream create failed \n");
		res = -ENOMEM;
		goto err_exit;
	}

	res = stream_open(debug_file_stream, MODE_OUT);
	if(res) {
		SYS_LOG_ERR("debug_file_stream open failed \n");
		res = -ENOMEM;
		goto err_exit;
	}
#endif

	acts_ringbuf_init(info->cache_buff,
				media_mem_get_cache_pool(USB_UPLOAD_CACHE, AUDIO_STREAM_USOUND),
				media_mem_get_cache_pool_size(USB_UPLOAD_CACHE, AUDIO_STREAM_USOUND));

	handle->type = RINGBUFFER_STREAM_IMAG_NUM;
	handle->data = info;

	return 0;

err_exit:
	if (info)
		mem_free(info);
	return res;
}

const stream_ops_t usb_audio_stream_ops = {
	.init = usb_audio_stream_init,
	.open = usb_audio_stream_open,
	.seek = NULL,
	.tell = usb_audio_stream_tell,
	.write = usb_audio_stream_write,
	.get_length = usb_audio_stream_get_length,
	.get_space = usb_audio_stream_get_space,
	.flush = usb_audio_stream_flush,
	.read = usb_audio_stream_read,
	.close = usb_audio_stream_close,
	.destroy = usb_audio_stream_destroy,
};

io_stream_t usb_audio_upload_stream_create(void)
{
	usb_audio_upload_stream = stream_create(&usb_audio_stream_ops, NULL);
	return usb_audio_upload_stream;
}

int usb_audio_upload_stream_set(io_stream_t stream)
{
	SYS_IRQ_FLAGS flags;

	sys_irq_lock(&flags);
	usb_audio_upload_stream = stream;
	sys_irq_unlock(&flags);

	return 0;
}
