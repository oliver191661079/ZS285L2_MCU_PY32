/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt manager tws snoop.
 */
#ifndef _BTMGR_TWS_SNOOP_H_
#define _BTMGR_TWS_SNOOP_H_

void btmgr_tws_poff_check_snoop_disconnect(void);
void btmgr_tws_poff_state_proc(void);
void bt_manager_tws_sync_phone_info(uint16_t hdl, void* param);
void bt_manager_tws_proc_snoop_switch_sync_info(uint8_t *data, uint16_t len);

void bt_manager_tws_sync_lea_info(void* param);
void bt_manager_tws_sync_active_dev(void* param);

void bt_manager_tws_sync_ble_mac(void* param);

#endif
