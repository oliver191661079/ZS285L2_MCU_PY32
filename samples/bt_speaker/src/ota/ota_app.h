/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief OTA application interface
 */

#ifndef __OTA_APP_H__
#define __OTA_APP_H__
#include <kernel.h>
#include <app_ui.h>

enum {
	MSG_OTA_MESSAGE_CMD_INIT_APP = MSG_APP_INPUT_MESSAGE_CMD_START,
	MSG_OTA_MESSAGE_CMD_POWEROFF,
	MSG_OTA_MESSAGE_CMD_EXIT_APP,
};

extern int ota_app_init(void);
extern int ota_app_init_uhost(void);
extern int ota_app_init_sdcard(void);
extern int ota_app_init_bluetooth(void);
extern int ota_app_init_uart(void);
extern int ota_app_init_selfapp(void);

extern void ota_view_deinit(void);
extern void ota_view_init(void);
extern void ota_view_show_upgrade_progress(u8_t progress);
extern void ota_view_show_upgrade_result(u8_t * string, bool is_faill);

extern const unsigned char *ota_get_public_key(void);
#endif				/* __OTA_APP_H__ */
