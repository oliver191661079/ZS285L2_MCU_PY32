/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief file cache interface
*/

#include <os_common_api.h>
#include <acts_cache.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "cache_internel.h"

int file_cache_read(io_cache_t handle, void *buf, int num)
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
		brw = fs_seek(handle->fp, file_off, FS_SEEK_SET);
		if (brw) {
			SYS_LOG_WRN("fs_seek failed [%zd], file_off=%d\n", brw, file_off);
			goto err_out;
		}

		brw = handle->size - file_off;
		if (avail > brw) {
			brw = fs_read(handle->fp, buf, brw);
			if (brw < 0) {
				SYS_LOG_WRN("Failed read to file [%zd]\n", brw);
				goto err_out;
			}

			avail -= brw;
			read_len += brw;
			handle->rofs += brw;

			brw = fs_seek(handle->fp, 0, FS_SEEK_SET);
			if (brw) {
				SYS_LOG_WRN("fs_seek failed [%zd]\n", brw);
				goto err_out;
			}
		}

		brw = fs_read(handle->fp, (u8_t*)buf + read_len, avail);
		if (brw < 0) {
			SYS_LOG_WRN("Failed read to file [%zd]\n", brw);
			goto err_out;
		}

		read_len += avail;
		handle->rofs += avail;

		if (handle->write_finished) {
			break;
		}
	}

err_out:
out_exit:
	return read_len;

}

int file_cache_write(io_cache_t handle, const void *buf, int num)
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
		brw = fs_seek(handle->fp, file_off, FS_SEEK_SET);
		if (brw) {
			SYS_LOG_WRN("fs_seek failed [%zd], file_off=%d\n", brw, file_off);
			goto err_out;
		}

		brw = handle->size - file_off;
		if (avail > brw) {
			brw = fs_write(handle->fp, buf, brw);
			if (brw < 0) {
				SYS_LOG_WRN("Failed writing to file [%zd]\n", brw);
				goto err_out;
			}

			avail -= brw;
			wirte_len += brw;
			handle->wofs += brw;

			brw = fs_seek(handle->fp, 0, FS_SEEK_SET);
			if (brw) {
				SYS_LOG_WRN("fs_seek failed [%zd]\n", brw);
				goto err_out;
			}
		}

		brw = fs_write(handle->fp, (u8_t*)buf + wirte_len, avail);
		if (brw < 0) {
			SYS_LOG_WRN("Failed writing to file [%zd]\n", brw);
			goto err_out;
		}

		wirte_len += avail;
		handle->wofs += avail;
		if (handle->write_finished) {
			break;
		}
	}

out_exit:
#if 0
	/* sync is unnecessary */
	if (fs_sync(&handle->fp))
		SYS_LOG_ERR("Fail to sync file \n");
#endif
	return wirte_len;
err_out:
	/* FIXME: update the cache size to the file size ? */
	if (handle->size > file_off) {
		SYS_LOG_WRN("shrink cache size from %d to %d\n", handle->size, file_off);
		handle->size = file_off; //f_size(&handle->fp.fp);
	}
	goto out_exit;
}

int file_cache_destroy(io_cache_t handle)
{
	int res;

	handle->write_finished = true;

	res = fs_close(handle->fp);
	if (res) {
		SYS_LOG_ERR("Error closing file [%d]\n", res);
		goto exit;
	}

	handle->wofs = 0;
	handle->rofs = 0;
exit:
	os_sem_give(&handle->sem);

	while(!handle->work_finished && try_cnt++ > 100){
		os_sleep(10);
	}

	if (!handle->work_finished) {
		SYS_LOG_ERR("work not filishend\n");
	}

	mem_free(handle->fp);
	mem_free(handle);
	return res;
}

int file_cache_reset(io_cache_t handle)
{
	fs_trunc_quick(handle->fp, 0);
	return 0;
}

io_cache_t file_cache_create(const char * cachename, int size)
{
	io_cache_t cache;
	int res;

	cache = mem_malloc(sizeof(struct acts_cache));
	if (cache == NULL) {
		SYS_LOG_ERR("file stream create failed \n");
		return NULL;
	}

	cache->fp = mem_malloc(sizeof(fs_file_t));
	if(!cache->fp) {
		goto failed;
	}

	res = fs_open(cache->fp, cachename);
	if (res) {
		SYS_LOG_ERR("Failed opening file [%d]\n", res);
		goto fp_failed;
	}

#if 0
	/* the file may fail to increase upto size, due to the limited disk capacity */
	res = fs_trunc_quick(cache->fp, size);
	if (res) {
		SYS_LOG_ERR("fs_truncate failed [%d]\n", res);
		fs_close(cache->fp);
		goto fp_failed;
	}
#else
	fs_trunc_quick(cache->fp, 0);
#endif

	cache->size = size;

	SYS_LOG_INF("size:%d, name: %s \n", cache->size, cachename);

	cache->write_finished = false;

	return cache;

fp_failed:
	mem_free(cache->fp);
failed:
	mem_free(cache);
	return NULL;

}

