/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief OTA manifest file interface
 */

#ifndef __OTA_MANIFEST_H__
#define __OTA_MANIFEST_H__

#include <fw_version.h>

#define OTA_MANIFEST_MAX_FILE_CNT	15

struct ota_file {
	u8_t name[13];
	u8_t type;
	u8_t file_id;
	u8_t reserved;

	u32_t offset;
	u32_t size;
	u32_t checksum;
} __attribute__((packed));

struct ota_manifest {
	struct fw_version fw_ver;
#ifdef CONFIG_OTA_FILE_PATCH
	struct fw_version old_fw_ver;
#endif

	int file_cnt;
	u8_t *manifest_data;
	u32_t manifest_size;

	struct ota_file wfiles[OTA_MANIFEST_MAX_FILE_CNT];
};

struct ota_image;

int ota_manifest_parse_file(struct ota_manifest *manifest, struct ota_image *img,
			    const char *file_name);
u8_t *ota_manifest_get_data(struct ota_manifest *manifest, u32_t *len);
void ota_manifest_release_data(struct ota_manifest *manifest);

#endif /* __OTA_MANIFEST_H__ */
