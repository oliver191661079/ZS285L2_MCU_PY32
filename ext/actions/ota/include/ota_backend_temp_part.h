/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief OTA Temp partition backend interface
 */

#ifndef __OTA_BACKEND_TEMP_PART_H__
#define __OTA_BACKEND_TEMP_PART_H__

#include <ota_backend.h>

struct ota_backend_temp_part_init_param {
	const char *dev_name;
};

struct ota_backend *ota_backend_temp_part_init(ota_backend_notify_cb_t cb,
		struct ota_backend_temp_part_init_param *param);

void *ota_backend_temp_part_file_open(const char *dev_name, uint32_t file_length, uint32_t file_offset);

int ota_backend_temp_part_file_get_length(void *handle);

int ota_backend_temp_part_file_get_original_length(void *handle);

int ota_backend_temp_part_file_read(void *handle, uint8_t *buf, uint32_t offset, uint32_t size);

int ota_backend_temp_part_file_close(void *handle);

#endif /* __OTA_BACKEND_TEMP_PART_H__ */
