/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief OTA bluetooth backend interface
 */

#ifndef __OTA_BACKEND_BT_H__
#define __OTA_BACKEND_BT_H__

#include <stream.h>

#include <ota_backend.h>

struct ota_backend_bt_init_param {
	const u8_t *spp_uuid;
	void *gatt_attr;
	u8_t attr_size;
	void *tx_chrc_attr;
	void *tx_attr;
	void *tx_ccc_attr;
	void *rx_attr;
	s32_t read_timeout;
	s32_t write_timeout;
};

struct ota_backend *ota_backend_bt_init(ota_backend_notify_cb_t cb,
					struct ota_backend_bt_init_param *param);
struct ota_backend *ota_backend_zble_init(ota_backend_notify_cb_t cb);

void ota_backend_bt_exit(struct ota_backend *backend);

#endif /* __OTA_BACKEND_BT_H__ */
