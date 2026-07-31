/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * @file clone stream interface
 */
#define SYS_LOG_DOMAIN "clonestream"
#include <mem_manager.h>
#include <clone_stream.h>
#include <misc/util.h>
#include <mem_manager.h>
#include <stream.h>
#include <string.h>
#include <assert.h>

#define CLONE_STREAM_MAGIC_TYPE 0x84

static bool validate_stream_state(io_stream_t handle, uint8_t state)
{
	return handle->state == state;
}

static int clone_stream_init(io_stream_t handle, void *param)
{
	struct clone_stream_info *info = param;
	int i;

	if(NULL == info) {
		SYS_LOG_WRN("Null data.");
		return -EINVAL;
	}

	if (!validate_stream_state(info->origin, STATE_INIT)) {
		SYS_LOG_ERR("state not init\n");
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(info->clones); i++) {
		if (!info->clones[i])
			break;

		if (!validate_stream_state(info->clones[i], STATE_OPEN)) {
			SYS_LOG_ERR("state not open %d\n", i);
			return -EINVAL;
		}

		if (!(info->clones[i]->mode & MODE_OUT)) {
			SYS_LOG_ERR("stream clone[%d] not writable\n", i);
			return -EINVAL;
		}
	}

	handle->data = mem_malloc(sizeof(*info));
	if (!handle->data) {
		SYS_LOG_ERR("malloc data %d\n", i);
		return -ENOMEM;
	}

	memcpy(handle->data, info, sizeof(*info));
	handle->type = CLONE_STREAM_MAGIC_TYPE;
	handle->total_size = info->origin->total_size;

	return 0;
}

static int clone_stream_open(io_stream_t handle, stream_mode mode)
{
	struct clone_stream_info *info = handle->data;
	int res;

	SYS_LOG_DBG(" call\n");

	if(NULL == info) {
		SYS_LOG_WRN("Null data.");
		return -EINVAL;
	}

	res = stream_open(info->origin, mode);
	if (res)
		return res;

	handle->write_finished = 0;
	return 0;
}

static inline int clone_stream_copy(io_stream_t s, int mode, unsigned char *buf,int num)
{
	int len = 0;
	int i, j;
	unsigned char *m;

	if (NULL != s) {
		if (MODE_MONO_COPY & mode) {
			m = (unsigned char *)mem_malloc(num/2);
			if (m == NULL) {
				SYS_LOG_WRN("No memory to malloc!");
				return 0;
			}

			//copy interlace data
			for (i = 0, j = 0; j < num - 1; i += 2, j += 4) {
				m[i] = buf[j];
				m[i + 1] = buf[j + 1];
			}
			len = stream_write(s, m, num/2);
			mem_free(m);
		} else {
			len = stream_write(s, buf, num);
		}
	}

	return len;
}

static int clone_stream_read(io_stream_t handle, unsigned char *buf,int num)
{
	struct clone_stream_info *info = handle->data;
	int len, i;
	int l;

	if(NULL == info) {
		SYS_LOG_WRN("Null data.");
		return 0;
	}

	len = stream_read(info->origin, buf, num);
	//SYS_LOG_INF("origin %p, %d, read %d", info->origin, num, len);
	if (len <= 0)
		return len;

	handle->rofs += len;

	/* clone read data */
	if (MODE_OUT & info->clone_mode) {
		for (i = 0; i < ARRAY_SIZE(info->clones); i++) {
			l = clone_stream_copy(info->clones[i], info->clone_mode, buf, num);
			//SYS_LOG_INF("clone %d, %d got %d", i, num, l);
		}
	}

	return len;
}

static int clone_stream_write(io_stream_t handle, unsigned char *buf,int num)
{
	struct clone_stream_info *info = handle->data;
	int len, i;
	int l = 0;

	if(NULL == info) {
		SYS_LOG_WRN("Null data.");
		return 0;
	}

	len = stream_write(info->origin, buf, num);
	//SYS_LOG_INF("origin %p, %d, wrote %d", info->origin, num, len);
	if (len <= 0)
		return len;

	handle->wofs += len;

	/* clone write data */
	if (MODE_IN & info->clone_mode) {
		for (i = 0; i < ARRAY_SIZE(info->clones); i++) {
			l = clone_stream_copy(info->clones[i], info->clone_mode, buf, num);
			//SYS_LOG_INF("clone %d, %d got %d", i, num, l);
		}
	}

	return len;
}

static int clone_stream_seek(io_stream_t handle, int offset, seek_dir origin)
{
	/* FIXME: seek ops will make clone stream data disordered */
	return -ENOSYS;
}

static int clone_stream_tell(io_stream_t handle)
{
	struct clone_stream_info *info = handle->data;

	if(NULL == info) {
		SYS_LOG_WRN("Null data.");
		return -EINVAL;
	}

	return stream_tell(info->origin);
}

static int clone_stream_get_length(io_stream_t handle)
{
	struct clone_stream_info *info = handle->data;

	if(NULL == info) {
		SYS_LOG_WRN("Null data.");
		return -EINVAL;
	}

	return stream_get_length(info->origin);
}

static int clone_stream_get_space(io_stream_t handle)
{
	struct clone_stream_info *info = handle->data;

	if(NULL == info) {
		SYS_LOG_WRN("Null data.");
		return -EINVAL;
	}

	return stream_get_space(info->origin);
}

static int clone_stream_flush(io_stream_t handle)
{
	struct clone_stream_info *info = handle->data;
	int res, i;
	io_stream_t s;

	if(NULL == info) {
		SYS_LOG_WRN("Null data.");
		return -EINVAL;
	}

	res = stream_flush(info->origin);
	for (i = 0; i < ARRAY_SIZE(info->clones); i++) {
			s = info->clones[i];
			if (s && s->ops && s->ops->flush) {
				info->clones[i]->ops->flush(s);
			}
	}

	return res;
}

static int clone_stream_close(io_stream_t handle)
{
	struct clone_stream_info *info = handle->data;
	int i;
	io_stream_t s;

	if(NULL == info) {
		SYS_LOG_WRN("Null data.");
		return -EINVAL;
	}

	/* notify write finished */
	for (i = 0; i < ARRAY_SIZE(info->clones); i++) {
		s = info->clones[i];
		if (s && s->ops && s->ops->write)
		{
			info->clones[i]->ops->write(s, NULL, 0);
		}
	}

	return stream_close(info->origin);
}

static int clone_stream_destroy(io_stream_t handle)
{
	struct clone_stream_info *info = handle->data;
	int res;

	if(NULL == info) {
		SYS_LOG_WRN("Null data.");
		return -EINVAL;
	}

	res = stream_destroy(info->origin);
	mem_free(info);
	return res;
}

const stream_ops_t clone_stream_ops = {
	.init = clone_stream_init,
	.open = clone_stream_open,
	.read = clone_stream_read,
	.seek = clone_stream_seek,
	.tell = clone_stream_tell,
	.get_length = clone_stream_get_length,
	.get_space = clone_stream_get_space,
	.write = clone_stream_write,
	.flush = clone_stream_flush,
	.close = clone_stream_close,
	.destroy = clone_stream_destroy,
};

io_stream_t clone_stream_create(struct clone_stream_info *info)
{
	return stream_create(&clone_stream_ops, info);

}
