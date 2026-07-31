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
#ifndef __RAM_CACHE_H__
#define __RAM_CACHE_H__
#include <os_common_api.h>
#include <acts_cache.h>

#include <logging/sys_log.h>

#ifdef CONFIG_RAM_CACHE
io_cache_t ram_cache_create(void *cachebuffer, int size);
int ram_cache_read(io_cache_t handle, void *buf, int num);
int ram_cache_write(io_cache_t handle, const void *buf, int num);
int ram_cache_destroy(io_cache_t handle);
#endif

#ifdef CONFIG_FILE_CACHE
io_cache_t file_cache_create(const char * cachename, int size);
int file_cache_read(io_cache_t handle, void *buf, int num);
int file_cache_write(io_cache_t handle, const void *buf, int num);
int file_cache_destroy(io_cache_t handle);
int file_cache_reset(io_cache_t handle);
#endif

#ifdef CONFIG_PSRAM_CACHE
io_cache_t psram_cache_create(const char * cachename, int size);
int psram_cache_read(io_cache_t handle, void *buf, int num);
int psram_cache_write(io_cache_t handle, const void *buf, int num);
int psram_cache_destroy(io_cache_t handle);
#endif

int cache_check_read(io_cache_t handle);
int cache_check_write(io_cache_t handle);

#endif

