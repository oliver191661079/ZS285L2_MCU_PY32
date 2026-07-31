/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt manager le tws.
 */
#ifndef _BTMGR_LETWS_H_
#define _BTMGR_LETWS_H_

#include "bt_manager.h"

typedef struct
{
    uint8_t dev_role;
    bt_addr_le_t addr;
} bt_mgr_saved_letws_info_t;

typedef int (*bt_letws_vnd_rx_cb)(uint16_t handle,const uint8_t *buf,uint16_t len);

struct btmgr_letws_context_t {
	uint8_t tws_role:3;
	uint8_t temp_tws_role:3;
	uint8_t le_remote_addr_valid:1;
	uint8_t restart_adv:1;
	uint8_t aux_plugin:1;
	uint8_t exchange_version_finish:3;
	uint8_t letws_connected:1;
	uint8_t letws_disconn_pending:1;
	uint8_t mismatch_rssi:1;
	uint8_t letws_mode_state;
	uint8_t letws_mode_state_temp;
	uint16_t tws_handle;
	bt_addr_le_t remote_ble_addr;
	bt_mgr_saved_letws_info_t info;
	struct bt_le_ext_adv *ext_adv;
	bt_letws_vnd_rx_cb rx_cb;
	os_mutex letws_mutex;
	os_delayed_work letws_pair_search_work;
	os_delayed_work letws_pair_search_alternate_work;
	os_delayed_work letws_run_work;
#ifdef CONFIG_BT_LETWS_AIRTOUCH
    bt_addr_le_t lock_addr;
	uint8_t lock_flag;
	uint8_t create_flag;
	uint8_t airtouch_disable_restart;
	uint32_t last_time;
	os_delayed_work letws_airtouch_pair_work;
#endif		
};

struct btmgr_letws_context_t *btmgr_get_letws_context(void);
void bt_manager_save_letws_info(uint8_t role,bt_addr_le_t *addr);
void bt_manager_init_letws_info(bt_letws_vnd_rx_cb cb);
void bt_manager_clear_letws_info(void);
int bt_manager_letws_get_dev_role(void);
uint16_t bt_manager_letws_get_handle(void);

int bt_mamager_letws_connected(uint16_t handle);
void bt_mamager_letws_disconnected(uint16_t handle, uint8_t role, uint8_t reason);
int bt_mamager_set_remote_ble_addr(bt_addr_le_t *addr);
void bt_manager_letws_start_pair_search(uint8_t role,int time_out_s,int dir_flag);
int bt_mamager_letws_disconnect(int reason);
int bt_mamager_letws_reconnect(void);
void bt_manager_letws_reset(void);
void bt_manager_letws_deinit(void);
void bt_manager_letws_set_aux_status(int plugin);

#ifdef CONFIG_BT_LETWS_AIRTOUCH
void bt_manager_set_mismatch_rssi(uint8_t mismatch_rssi);
/*
  Once this function is called, if the current airtouch pairing stops, 
  the subsequent scene switching will not be automatically started, 
  and the application needs to actively manage and enable it
*/
void bt_manager_airtouch_disable_restart(uint8_t disable_restart);
void bt_manager_letws_airtouch_pair(uint8_t role,int time_out_s, int dir_flag);
void bt_manager_airtouch_set_create_falg(uint8_t val);
uint8_t bt_manager_airtouch_get_create_falg(void);
#endif //#ifdef CONFIG_BT_LETWS_AIRTOUCH

#endif
