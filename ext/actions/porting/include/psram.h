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
 * 2018/1/20: Created by wh.
 */


#ifndef __PSRAM_H__
#define __PSRAM_H__

#include <logging/sys_log.h>

#define PSRAM_SECTOR_SIZE			1024

#define PSRAM_MAX_BLOCK_SIZE (1024 * PSRAM_SECTOR_SIZE)

#define PSRAM_MIN_BLOCK_SIZE (256 * PSRAM_SECTOR_SIZE)

int psram_read(u32_t offset, u8_t *buff, u32_t len);
int psram_write(u32_t offset, u8_t *buff, u32_t len);
int psram_alloc(u32_t size);
int psram_free(u32_t offset);

#endif
