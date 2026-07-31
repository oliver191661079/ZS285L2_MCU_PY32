/*
 * Copyright (c) 2017 Actions Semi Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.

 * Author: wh<wanghui@actions-semi.com>
 *
 * Change log:
 *	2017/1/20: Created by wh.
 */
#ifndef __ACTS_CACHE_H__
#define __ACTS_CACHE_H__

#include <os_common_api.h>

#ifdef CONFIG_FILE_SYSTEM
#include <fs.h>
#endif

#ifdef CONFIG_PSRAM
#include <psram.h>
#endif

#include <logging/sys_log.h>

/**
 * @defgroup psram_cache_apis psram_cache APIs
 * @ingroup mem_managers
 * @{
 */

/** cache type */
typedef enum
{
	/** type of ram cache */
	TYPE_RAM_CACHE,
	/** type of file cache */
	TYPE_FILE_CACHE,
	/** type of psram cache */
	TYPE_PSRAM_CACHE,
}cache_type_e;

typedef enum
{
	/** mode of block cache */
	CACHE_BLOCK_MODE,
	/** mode of unblock cache */
	CACHE_UNBLOCK_MODE,
}cache_mode_e;

/** structure of psram_cache*/
struct acts_cache
{
	/** flag of ram_cache write finished*/
	u8_t cache_type;
	/** flag of ram_cache write finished*/
	u8_t cache_mode;
	/** flag of ram_cache write finished*/
	bool write_finished;
	/** flag of work finished*/
	bool work_finished;
	/** read off set of ram_cache*/
	int rofs;
	/** write off set of ram_cache*/
	int wofs;
	/** size of cache file */
	int size ;
	/** sem for buffer data sync*/
	os_sem sem;

	union{
	#ifdef CONFIG_RAM_CACHE
		/** hanlde of ram cache buffer*/
		char * cache;
	#endif
	#ifdef CONFIG_FILE_CACHE
		/** hanlde of file cache file fp*/
		fs_file_t *fp;
	#endif
	#ifdef CONFIG_PSRAM_CACHE
		/** hanlde of psram cache buffer*/
		u32_t base_off;
	#endif
	};
};

typedef struct acts_cache *io_cache_t;

/**
 * @brief create cache , return cache handle
 *
 * This routine provides create cache ,and return cache handle.
 * and cache state is  STATE_INIT
 *
 * @param type type of cache
 * @param parama create cache parama
 *
 * @return cache handle if create cache success
 * @return NULL  if create cache failed
 */

io_cache_t cache_create(char *parama, int size, cache_type_e type);

/**
 * @brief read from cache
 *
 * This routine provides read num of bytes from cache, if cache states is
 * not STATE_OPEN , return err , else return the realy read data len to user
 *
 * @param handle handle of cache
 * @param buf out put data buffer pointer
  * @param num bytes user want to read
 *
 * @return >=0 the realy read data length ,if return < num, means read finished.
 * @return <0  cache read failed
 */

int cache_read(io_cache_t handle, void *buf, int num);

/**
 * @brief read from cache
 *
 * This routine provides read num of bytes from cache, if cache states is
 * not STATE_OPEN , return err , else return the realy read data len to user
 *
 * @param handle handle of cache
 * @param buf out put data buffer pointer
  * @param num bytes user want to read
 *
 * @return >=0 the realy read data length ,if return < num, means read finished.
 * @return <0  cache read failed
 */

int cache_read_nonblock(io_cache_t handle, void *buf, int num);

/**
 * @brief write to cache
 *
 * This routine provides write num of bytes to cache, if cache states is
 * not STATE_OPEN , return err , else return the realy write data len to user
 *
 *
 * @param handle handle of cache
 * @param buf data poninter of write
 * @param num bytes user want to write
 *
 * @return >=0 the realy write data length ,if return < num, means write finished.
 * @return <0  cache write failed
 */

int cache_write(io_cache_t handle, const void *buf, int num);

/**
 * @brief write to cache
 *
 * This routine provides write num of bytes to cache, if cache states is
 * not STATE_OPEN , return err , else return the realy write data len to user
 *
 *
 * @param handle handle of cache
 * @param buf data poninter of write
 * @param num bytes user want to write
 *
 * @return >=0 the realy write data length ,if return < num, means write finished.
 * @return <0  cache write failed
 */

int cache_write_nonblock(io_cache_t handle, const void *buf, int num);

/**
 * @brief destroy cache
 *
 * This routine provides destroy cache , free all cache resurce
 * not allowed access cache handle after this routine execute.
 *
 * @param handle handle of cache
 *
 * @return 0 destroy success
 * @return !=0  destroy failed
 */
int cache_destroy(io_cache_t handle);

/**
 * @brief reset cache
 *
 * This routine provides reset cache read/write offset, thus
 * discard all the content that remains in the cache
 * 
 *
 * @param handle handle of cache
 *
 * @return 0 reset success
 * @return !=0  reset failed
 */
int cache_reset(io_cache_t handle);

/**
 * @brief cache size
 *
 * This routine provides get cache size
 *
 * @param handle handle of cache
 *
 * @return cache size
 */
static inline int cache_size(io_cache_t handle)
{
	return handle->size;
}

/**
 * @brief cache length
 *
 * This routine provides get cache length
 *
 * @param handle handle of cache
 *
 * @return cache length
 */
static inline int cache_length(io_cache_t handle)
{
	return handle->wofs - handle->rofs;
}

/**
 * @brief cache free space
 *
 * This routine provides get cache free space
 *
 * @param handle handle of cache
 *
 * @return cache free space
 */
static inline int cache_free_space(io_cache_t handle)
{
	return handle->size - (handle->wofs - handle->rofs);
}

static inline int cache_write_finished(io_cache_t handle)
{
	return cache_write(handle, NULL, 0);
}

/**
 * @} end defgroup cache_apis
 */

#endif

