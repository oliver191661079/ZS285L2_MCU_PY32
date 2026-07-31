/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief OTA upgrade interface
 */

#ifndef __OTA_UPGRADE_H__
#define __OTA_UPGRADE_H__

struct ota_upgrade_info;
struct ota_backend;

enum ota_state
{
	OTA_INIT = 0,
	OTA_RUNNING,
	OTA_DONE,
	OTA_FAIL,
	OTA_CANCEL,

	OTA_STATE_MAX = 0x20,

	OTA_FILE_WRITE,
	OTA_FILE_WRITE_DONE,
	OTA_FILE_VERIFY,
	OTA_FILE_VERIFY_DONE,

	OTA_BREAKPOINT_REPORT,

	OTA_UPLOADING,
	OTA_INIT_FINISHED,
	OTA_UPGRADE_PREPARE,
	OTA_UPGRADE_PREPARE_DONE,
	OTA_UPGRADE_TEMP_IMG_DONE,
};

typedef int (*ota_notify_t)(int state, int old_state);

struct ota_upgrade_param {
	const char *storage_name;
#ifdef CONFIG_OTA_DOWNLOAD_EXTERNAL_FLASH
	const char *storage_ext_name;
#endif
	ota_notify_t notify;
	const char *public_key;
	/* flags */
	unsigned int flag_use_recovery:1;	/* use recovery */
	unsigned int flag_use_recovery_app:1;	/* use recovery app */
	unsigned int no_version_control:1; /* OTA without version control */
	unsigned int flag_skip_init_erase : 1;
	unsigned int flag_secure_boot : 1;
	unsigned int flag_enable_reboot_resume_upgrade : 1;
};

struct ota_upgrade_check_param{
	unsigned char *data_buf;
	unsigned int data_buf_size;
};

struct ota_upgrade_info *ota_upgrade_init(struct ota_upgrade_param *param);
int ota_upgrade_check(struct ota_upgrade_info *ota, struct ota_upgrade_check_param *param);
int ota_upgrade_is_in_progress(struct ota_upgrade_info *ota);

int ota_upgrade_attach_backend(struct ota_upgrade_info *ota, struct ota_backend *backend);
void ota_upgrade_detach_backend(struct ota_upgrade_info *ota, struct ota_backend *backend);
int ota_upgrade_get_backend_type(struct ota_upgrade_info *ota);
int ota_upgrade_is_ota_running(void);
int ota_upgrade_cancel_read(struct ota_upgrade_info *ota);

int ota_upgrade_get_temp_img_size(struct ota_upgrade_info *ota);

int ota_upgrade_set_ota_partition_other_writing(uint8_t file_id);

int ota_upgrade_clear_ota_bp_state(struct ota_upgrade_info *ota);

int ota_upgrade_verify_temp_image(struct ota_upgrade_info *ota);

int ota_upgrade_set_ota_buf(struct ota_upgrade_info *ota, uint8_t *buf, uint32_t buf_size);

int ota_upgrade_flash_code_copy(struct ota_upgrade_info *ota, uint8_t src_partition_id, uint8_t dst_partition_id);

int ota_upgrade_get_file_information(struct ota_upgrade_info *ota, const char *filename, uint32_t *file_offset, uint32_t *file_size);

#ifdef CONFIG_OTA_UPGRADE_SAVE_OTA_HEADER

int ota_upgrade_header_data_load(struct ota_upgrade_info *ota, int offs, uint8_t *data, uint32_t len);

#endif

#endif /* __OTA_UPGRADE_H__ */
