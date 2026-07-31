/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief ram cache interface
*/

#include <os_common_api.h>
#include <mem_manager.h>
#include <acts_cache.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "cache_internel.h"

int ram_cache_read(io_cache_t handle, void *buf, int num)
{
	ssize_t brw;
	int read_len = 0;
	int file_off = 0;
	int avail;
	while (num > 0) {
		avail = cache_length(handle);

		if (!avail)
			break;

		if (avail > num)
			avail = num;

		num -= avail;

		file_off = handle->rofs % handle->size;
		handle->rofs += avail;

		brw = handle->size - file_off;
		if (avail > brw) {
			memcpy(buf, handle->cache + file_off, brw);

			avail -= brw;
			read_len += brw;
			file_off = 0;
		}

		memcpy((u8_t*)buf + read_len, handle->cache  + file_off, avail);
		read_len += avail;

		if (handle->write_finished) {
			break;
		}
	}

	return read_len;
}

int ram_cache_write(io_cache_t handle, const void *buf, int num)
{
	ssize_t brw = 0;
	int wirte_len = 0;
	int file_off = 0;
	int avail;

	handle->write_finished = (num > 0) ? false : true;

	while (num > 0) {
		avail = cache_free_space(handle);

		if (!avail)
			break;

		if (avail > num)
			avail = num;

		num -= avail;

		file_off = handle->wofs % handle->size;
		handle->wofs += avail;

		brw = handle->size - file_off;
		if (avail > brw) {
			memcpy(handle->cache + file_off, buf, brw);

			avail -= brw;
			wirte_len += brw;
			file_off = 0;
		}

		memcpy(handle->cache + file_off, (u8_t*)buf + wirte_len, avail);
		wirte_len += avail;

		if (handle->write_finished) {
			break;
		}
	}

	return wirte_len;
}

int ram_cache_destroy(io_cache_t handle)
{
	int try_cnt = 0;

	handle->write_finished = true;
	handle->wofs = 0;
	handle->rofs = 0;

	while(!handle->work_finished && try_cnt++ > 100){
		os_sleep(10);
	}

	if (!handle->work_finished) {
		SYS_LOG_ERR("work not filishend\n");
	}

	mem_free(handle);
	return 0;
}

io_cache_t ram_cache_create(void *cachebuffer, int size)
{
	io_cache_t cache;

	cache = mem_malloc(sizeof(struct acts_cache));
	if (cache == NULL) {
		SYS_LOG_ERR("file stream create failed \n");
		return NULL;
	}

	cache->cache = cachebuffer;
	cache->size = size;

	SYS_LOG_INF("size:%d, buffer: %p \n", cache->size, cachebuffer);

	os_sem_init(&cache->sem, 1, 1);

	cache->write_finished = false;

	return cache;
}

