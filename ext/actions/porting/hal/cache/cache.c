/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief cache interface
*/


#include <os_common_api.h>
#include <acts_cache.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "cache_internel.h"

static int _cache_check_read(io_cache_t handle)
{
	int avail = 0;

	do {
		avail = handle->wofs - handle->rofs;
		if (avail) {
			break;
		}

		if (handle->write_finished) {
			goto exit;
		} 

		if (-EAGAIN == os_sem_take(&handle->sem, OS_MSEC(10))) {
			//SYS_LOG_DBG("10 ms timeout\n");
		}
	} while (true);

exit:
	return avail;
}

static int _cache_check_write(io_cache_t handle)
{
	int avail = 0;

	do {
		avail = handle->size - (handle->wofs - handle->rofs);
		if (avail) {
			break;
		}

		if (handle->write_finished) {
			goto exit;
		} 

		if (-EAGAIN == os_sem_take(&handle->sem, OS_MSEC(10))) {
			//SYS_LOG_DBG("10 ms timeout\n");
		}
	} while (true);

exit:
	return avail;
}


int cache_read(io_cache_t handle, void *buf, int num)
{
	int ret;

	handle->work_finished = false;

	ret = _cache_check_read(handle);

	if (ret) {
		ret = cache_read_nonblock(handle, buf, num);
	}

	if(ret > 0){
		os_sem_give(&handle->sem);
	}

	handle->work_finished = true;
	return ret;
}

int cache_read_nonblock(io_cache_t handle, void *buf, int num)
{
	int ret = 0;
	handle->work_finished = false;

	switch (handle->cache_type) {
#ifdef CONFIG_RAM_CACHE
	case TYPE_RAM_CACHE:
		ret = ram_cache_read(handle, buf, num);
	break;
#endif

#ifdef CONFIG_FILE_CACHE
	case TYPE_FILE_CACHE:
		ret = file_cache_read(handle, buf, num);
	break;
#endif

#ifdef CONFIG_PSRAM_CACHE
	case TYPE_PSRAM_CACHE:
		ret = psram_cache_read(handle, buf, num);
	break;
#endif

	default:
		break;
	}

	handle->work_finished = true;
	return ret;
}

int cache_write(io_cache_t handle, const void *buf, int num)
{
	int ret;

	handle->work_finished = false;	

	ret = _cache_check_write(handle);

	if (ret) {
		ret = cache_write_nonblock(handle, buf, num);
	}

	if(ret > 0){
		os_sem_give(&handle->sem);
	}
	handle->work_finished = true;
	return ret;
}

int cache_write_nonblock(io_cache_t handle, const void *buf, int num)
{
	int ret = 0;

	handle->work_finished = false;

	switch (handle->cache_type) {
#ifdef CONFIG_RAM_CACHE
	case TYPE_RAM_CACHE:
		ret = ram_cache_write(handle, buf, num);
	break;
#endif

#ifdef CONFIG_FILE_CACHE
	case TYPE_FILE_CACHE:
		ret = file_cache_write(handle, buf, num);
	break;
#endif

#ifdef CONFIG_PSRAM_CACHE
	case TYPE_PSRAM_CACHE:
		ret = psram_cache_write(handle, buf, num);
	break;
#endif
	default:
		break;
	}
	handle->work_finished = true;
	return ret;
}

int cache_destroy(io_cache_t handle)
{
	//os_sem_give(&handle->sem);

	switch (handle->cache_type) {
#ifdef CONFIG_RAM_CACHE
	case TYPE_RAM_CACHE:
		return ram_cache_destroy(handle);
	break;
#endif

#ifdef CONFIG_FILE_CACHE
	case TYPE_FILE_CACHE:
		return file_cache_destroy(handle);
	break;
#endif

#ifdef CONFIG_PSRAM_CACHE
	case TYPE_PSRAM_CACHE:
		return psram_cache_destroy(handle);
	break;
#endif

	default:
		break;
	}
	return 0;
}

io_cache_t cache_create(char *parama, int size, cache_type_e type)
{
	io_cache_t cache = NULL;

	switch (type) {
#ifdef CONFIG_RAM_CACHE
	case TYPE_RAM_CACHE:
		cache = ram_cache_create(parama, size);
	break;
#endif

#ifdef CONFIG_FILE_CACHE
	case TYPE_FILE_CACHE:
		cache = file_cache_create(parama, size);
	break;
#endif

#ifdef CONFIG_PSRAM_CACHE
	case TYPE_PSRAM_CACHE:
		cache = psram_cache_create(parama, size);
	break;
#endif

	default:
		break;
	}

	if (cache) {
		cache->cache_type = type;
		os_sem_init(&cache->sem, 0, 1);
	}
	return cache;
}

int cache_reset(io_cache_t handle)
{
	handle->wofs = 0;
	handle->rofs = 0;
	handle->write_finished = false;
	handle->work_finished = true;

	os_sem_give(&handle->sem);
#ifdef CONFIG_FILE_CACHE
	if(handle->cache_type == TYPE_FILE_CACHE){
		file_cache_reset();
	}
#endif
	return 0;
}
