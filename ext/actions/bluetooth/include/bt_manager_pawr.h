/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt PAWR manager.
*/

#ifndef __BT_MANAGER_PAWR_H__
#define __BT_MANAGER_PAWR_H__
#include <acts_bluetooth/conn.h>

/**
 * @defgroup bt_manager_pawr_apis Bt Manager Pawr APIs
 * @ingroup bluetooth_system_apis
 * @{
 */

#define PAWR_NAME "pawr_actions" // Notice: Modification not allowed
#define PAWR_NAME_LEN (sizeof(PAWR_NAME) - 1)
#define NAME_LEN 32

typedef int (*bt_pawr_vnd_rsp_cb)(const uint8_t *buf,uint16_t len);
typedef int (*bt_pawr_vnd_rec_cb)(const uint8_t *buf,uint16_t len);

int bt_manager_pawr_adv_start(bt_pawr_vnd_rsp_cb cb,struct bt_le_per_adv_param *per_adv_param);
int bt_manager_pawr_adv_stop(void);

int bt_manager_pawr_receive_start(bt_pawr_vnd_rec_cb cb, struct bt_le_scan_param *param, int8 rssi);
int bt_manager_pawr_receive_stop(void);

int bt_manager_pawr_vnd_rsp_send(uint8_t *buf,uint8_t len, uint8_t type);
int bt_manager_pawr_vnd_per_send(uint8_t *buf,uint8_t len, uint8_t type);

int bt_manager_pawr_set_ext_adv(u8_t *buf, u16_t len);
void bt_manager_pawr_match_mac_set(u8_t *mac);
/**
 * @} end defgroup bt_manager_pawr_apis
 */
#endif  // __BT_MANAGER_PAWR_H__
