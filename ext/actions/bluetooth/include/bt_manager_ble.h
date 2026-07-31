/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt ble manager.
*/

#ifndef __BT_MANAGER_BLE_H__
#define __BT_MANAGER_BLE_H__
#include <acts_bluetooth/conn.h>
#include <acts_bluetooth/gatt.h>

/**
 * @defgroup bt_manager_ble_apis Bt Manager Ble APIs
 * @ingroup bluetooth_system_apis
 * @{
 */

typedef enum{
	ADV_STATE_BIT_DISABLE = 0,
	ADV_STATE_BIT_ENABLE  = (1<<0),
	ADV_STATE_BIT_UPDATE  = (1<<1),
}btmgr_adv_state_e;

enum {
	ADV_INTERVAL_NORMAL,
	ADV_INTERVAL_STANDBY,
};

/** ble register manager structure */
struct ble_reg_manager {
	void (*link_cb)(struct bt_conn *conn, uint8_t conn_type, uint8_t *mac, uint8_t connected);
	struct bt_gatt_service gatt_svc;
	sys_snode_t node;
};

/**
 * @brief get ble mtu
 *
 * This routine provides to get ble mtu
 *
 * @return ble mtu
 */
uint16_t bt_manager_get_ble_mtu(struct bt_conn *conn);

/**
 * @brief bt manager send ble data
 *
 * This routine provides to bt manager send ble data
 *
 * @param chrc_attr
 * @param des_attr 
 * @param data pointer of send data
 * @param len length of data
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_ble_send_data(struct bt_conn *conn, struct bt_gatt_attr *chrc_attr,
					struct bt_gatt_attr *des_attr, uint8_t *data, uint16_t len);

/**
 * @brief ble disconnect
 *
 * This routine do ble disconnect
 *
 * @return  N/A
 */
int bt_manager_ble_disconnect(struct bt_conn *conn);

/**
 * @brief ble disconnect all link
 *
 * This routine do ble disconnect
 *
 * @return  N/A
 */
void bt_manager_ble_disconnect_all_link(void);

/**
 * @brief ble disconnect by address
 *
 * This routine do ble disconnect
 *
 * @return  N/A
 */
int bt_manager_ble_disconnect_by_addr(bd_address_t *addr);

/**
 * @brief ble service register
 *
 * This routine provides ble service register
 *
 * @param le_mgr ble register info 
 *
 * @return  N/A
 */
void bt_manager_ble_service_reg(struct ble_reg_manager *le_mgr);

/**
 * @brief Set ble idle interval
 *
 * This routine Set ble idle interval
 *
 * @param interval idle interval (unit: 1.25ms)
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_ble_set_idle_interval(uint16_t interval);

/**
 * @brief init btmanager ble
 *
 * This routine init btmanager ble
 *
 * @return  N/A
 */
void bt_manager_ble_init(void);

/**
 * @brief notify ble a2dp play state
 *
 * This routine notify ble a2dp play state
 *
 * @param play a2dp play or stop
 *
 * @return  N/A
 */
void bt_manager_ble_a2dp_play_notify(bool play);

/**
 * @brief notify ble hfp play state
 *
 * This routine notify ble hfp play state
 *
 * @param play hfp play or stop
 *
 * @return  N/A
 */
void bt_manager_ble_hfp_play_notify(bool play);

/**
 * @brief halt ble
 *
 * This routine disable ble adv
 *
 * @return  N/A
 */
void bt_manager_halt_ble(void);

/**
 * @brief resume ble
 *
 * This routine enable ble adv
 *
 * @return  N/A
 */
void bt_manager_resume_ble(void);

/**
 * @brief deinit btmanager ble
 *
 * This routine deinit btmanager ble
 *
 * @return  N/A
 */
void bt_manager_ble_deinit(void);

#ifdef CONFIG_BT_ADV_MANAGER
int bt_manager_ble_set_adv_data(uint8_t *ad_data, uint8_t ad_data_len, uint8_t *sd_data, uint8_t sd_data_len,int8_t power);
/**
 * @brief Ble set advertise data
 *
 * This routine ble set advertise data
 *
 * ad_data sd_data format
 * refer bt core spe ADVERTISING AND SCAN RESPONSE DATA FORMAT
 * @param ad_data Advertise data buffer
 * @param ad_data_len Advertise data length
 * @param sd_data Advertise response data buffer
 * @param sd_data_len Advertise response data length
 *
 * @return  0: set success, other: set failed
 */
int bt_manager_ble_set_advertise_enable(uint8_t enable);

/**
 * @brief Ble set advertise enable for google fast pair
 *
 * This routine ble set advertise enable
 *
 * @return  0: set success, other: set failed
 */
int bt_manager_ble_gfp_set_advertise_enable(uint8_t enable);

/**
 * @brief Ble set advertise enable/disable
 *
 * This routine ble set advertise enable/disable
 *
 * @return  0: set success, other: set failed
 */
int bt_manager_ble_is_connected(void);
/**
* @brief check ble is connected
*
* This routine check ble is connected
*
* @return  0: not connected, other: connected
*/
int bt_manager_ble_max_connected(void);

int bt_manager_ble_get_connected_num(void);

/**
* @brief check ble max connected
*
* This routine check ble max dev connected
*
* @return  1: max dev connected  other: not max
*/
uint8_t gfp_service_data_get(uint8_t *data);

void bt_manager_set_ble_adv_interval_mode(uint8_t mode);

#endif


#ifdef CONFIG_BT_LEATWS
int leatws_scan_start(void);

int leatws_scan_stop(void);

int leatws_adv_enable_check(void);

void leatws_notify_leaudio_reinit(void);

void leatws_set_audio_channel(uint32_t audio_channel);

void bt_manager_le_audio_get_sirk(uint8_t *p_sirk);

uint16_t *bt_manager_config_get_device_id(void);
#endif

/**
 * @} end defgroup bt_manager_ble_apis
 */
#endif  // __BT_MANAGER_BLE_H__
