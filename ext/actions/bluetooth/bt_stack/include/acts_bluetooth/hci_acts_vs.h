/** @file hci_acts_vs.h
 * @brief Bluetooth actions hci vendor command/event.
 *
 * Copyright (c) 2019 Actions Semi Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_BLUETOOTH_HCI_ACTS_VS_H_
#define ZEPHYR_INCLUDE_BLUETOOTH_HCI_ACTS_VS_H_

#include <acts_bluetooth/hci.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Vendor commands */

#define BT_HCI_OP_VS_SEND_1ST_SYNC_BTCLK		BT_OP(BT_OGF_VS, 0x0005)
#define BT_HCI_OP_VS_RX_1ST_SYNC_BTCLK			BT_OP(BT_OGF_VS, 0x0006)
struct bt_hci_cp_vs_1st_sync_btclk {
	uint16_t handle;
	uint8_t enable;
} __packed;

#define BT_HCI_OP_VS_READ_1ST_SYNC_BTCLK		BT_OP(BT_OGF_VS, 0x0007)
struct bt_hci_cp_vs_read_1st_sync_btclk {
	uint16_t handle;
} __packed;

struct bt_hci_rp_vs_read_1st_sync_btclk {
	uint8_t  status;
	uint16_t handle;
	uint32_t btclk;
} __packed;

#define BT_HCI_OP_VS_CREATE_ACL_SNOOP_LINK		BT_OP(BT_OGF_VS, 0x0008)
struct bt_hci_cp_vs_create_acl_snoop_link {
	uint16_t handle;
} __packed;

#define BT_HCI_OP_VS_DISCONECT_ACL_SNOOP_LINK	BT_OP(BT_OGF_VS, 0x0009)
struct bt_hci_cp_vs_disconnect_acl_snoop_link {
	uint16_t handle;
} __packed;

#define BT_HCI_OP_VS_CREATE_SYNC_SNOOP_LINK		BT_OP(BT_OGF_VS, 0x000A)
struct bt_hci_cp_vs_create_sync_snoop_link {
	uint16_t handle;
} __packed;

#define BT_HCI_OP_VS_DISCONECT_SYNC_SNOOP_LINK	BT_OP(BT_OGF_VS, 0x000B)
struct bt_hci_cp_vs_disconnect_sync_snoop_link {
	uint16_t handle;
} __packed;

#define BT_HCI_OP_VS_SET_TWS_LINK				BT_OP(BT_OGF_VS, 0x000C)
struct bt_hci_cp_vs_set_tws_link {
	uint16_t handle;
} __packed;

#define BT_HCI_OP_VS_WRITE_BT_ADDR				BT_OP(BT_OGF_VS, 0x000D)
struct bt_hci_cp_vs_write_bt_addr {
	bt_addr_t bdaddr;
} __packed;

#define BT_HCI_OP_VS_SWITCH_SNOOP_LINK			BT_OP(BT_OGF_VS, 0x000E)
struct bt_hci_cp_vs_switch_snoop_link {
	uint16_t handle;
} __packed;

#define BT_HCI_OP_VS_SET_SNOOP_PKT_TIMEOUT		BT_OP(BT_OGF_VS, 0x000F)
struct bt_hci_cp_vs_set_snoop_pkt_timeout {
	uint8_t max_retrans_timeout;
} __packed;

#define BT_HCI_OP_VS_SWITCH_LE_LINK				BT_OP(BT_OGF_VS, 0x0010)
struct bt_hci_cp_vs_switch_le_link {
	uint16_t handle;
} __packed;

struct visual_mode_user_data {
	uint8_t connected_phone;
	uint8_t channel;		/* L/R channel */
	uint8_t match_mode;
	uint8_t pair_keyword;	/* 0: one device key pair, 1: tow device key pair */
	uint8_t match_len;
	uint8_t data[TWS_MATCH_NAME_MAX_LEN];
};

#define BT_HCI_OP_VS_SET_TWS_VISUAL_MODE		BT_OP(BT_OGF_VS, 0x0011)
struct bt_hci_cp_vs_set_tws_visual_mode {
	uint8_t enable;
	uint16_t interval;
	bt_addr_t local_bdaddr;
	u8_t user_data_len;
	struct visual_mode_user_data data;
} __packed;

struct pair_mode_ref_data {
	bt_addr_t local_bdaddr;
	uint8_t connected_phone;
	uint8_t channel;		/* L/R channel */
	uint8_t match_mode;
	uint8_t pair_keyword;
	uint8_t match_len;
	uint8_t data[26];
};

#define BT_HCI_OP_VS_SET_TWS_PAIR_MODE			BT_OP(BT_OGF_VS, 0x0012)
struct bt_hci_cp_vs_set_tws_pair_mode {
	uint8_t enable;
	uint8_t setup_link;
	uint16_t interval;
	uint16_t window;
	int8_t rssi_thres;
	bt_addr_t local_bdaddr;
	uint8_t cod[3];
	uint8_t ref_data_len;
	struct pair_mode_ref_data data;
	struct pair_mode_ref_data mask;
} __packed;

#define BT_HCI_OP_VS_SET_SNOOP_LINK_ACTIVE			BT_OP(BT_OGF_VS, 0x0013)
struct bt_hci_cp_vs_set_snoop_link_active {
	uint16_t handle;
} __packed;

#define BT_HCI_OP_VS_GET_SNOOP_LINK_ACTIVE			BT_OP(BT_OGF_VS, 0x0014)

#define BT_HCI_OP_VS_WRITE_MANUFACTURE_INFO			BT_OP(BT_OGF_VS, 0x0016)
struct bt_hci_cp_vs_write_manufacture_info {
	uint8_t bt_version;
	uint16_t bt_subversion;
	uint16_t company_id;
} __packed;

#define BT_HCI_OP_VS_SET_SNOOP_LINK_INACTIVE			BT_OP(BT_OGF_VS, 0x0017)
struct bt_hci_cp_vs_set_snoop_link_inactive {
	uint16_t handle;
} __packed;


#define BT_HCI_OP_VS_SET_CIS_LINK_BACKGROUND			BT_OP(BT_OGF_VS, 0x0018)
struct bt_hci_cp_vs_set_cis_link_bg {
	uint8_t cis_bg_enable;
} __packed;

#define BT_HCI_OP_VS_SET_APLL_TEMP_COMP				BT_OP(BT_OGF_VS, 0x0030)
struct bt_hci_cp_vs_set_apll_temp_comp {
	uint8_t enable;
} __packed;

#define BT_HCI_OP_VS_DO_APLL_TEMP_COMP				BT_OP(BT_OGF_VS, 0x0031)

#define BT_HCI_OP_VS_READ_BT_BUILD_INFO			BT_OP(BT_OGF_VS, 0x0032)
struct bt_hci_rp_vs_read_bt_build_info {
	uint8_t  status;
	uint16_t rf_version;
	uint16_t bb_version;
	uint16_t board_type;
	uint16_t svn_version;
	uint8_t  str_data[128];
} __packed;

#define BT_HCI_OP_ACTS_VS_ADJUST_LINK_TIME		BT_OP(BT_OGF_VS, 0x0080)
struct bt_hci_cp_acts_vs_adjust_link_time {
	uint16_t handle;
	int16_t value;
} __packed;

#define BT_HCI_OP_ACTS_VS_READ_BT_CLOCK			BT_OP(BT_OGF_VS, 0x0081)
struct bt_hci_cp_acts_vs_read_bt_clock {
	uint16_t handle;
} __packed;

struct bt_hci_rp_acts_vs_read_bt_clock {
	uint8_t  status;
	uint32_t bt_clk;
	uint16_t bt_intraslot;
} __packed;

#define BT_HCI_OP_ACTS_VS_SET_TWS_INT_CLOCK		BT_OP(BT_OGF_VS, 0x0082)
struct bt_hci_cp_acts_vs_set_tws_int_clock {
	uint16_t handle;
	uint32_t bt_clk;
	uint16_t bt_intraslot;
} __packed;

#define BT_HCI_OP_ACTS_VS_ENABLE_TWS_INT		BT_OP(BT_OGF_VS, 0x0083)
struct bt_hci_cp_acts_vs_enable_tws_int {
	uint8_t enable;
} __packed;

#define BT_HCI_OP_ACTS_VS_ENABLE_ACL_FLOW_CTRL 	BT_OP(BT_OGF_VS, 0x0084)
struct bt_hci_cp_acts_vs_enable_acl_flow_ctrl {
	uint16_t handle;
	uint8_t  enable;
} __packed;

#define BT_HCI_OP_ACTS_VS_READ_ADD_NULL_CNT		BT_OP(BT_OGF_VS, 0x0085)
struct bt_hci_cp_acts_vs_read_add_null_cnt {
	uint16_t handle;
} __packed;
struct bt_hci_rp_acts_vs_read_add_null_cnt {
	uint8_t  status;
	uint16_t null_cnt;
} __packed;

#define BT_HCI_OP_ACTS_VS_SET_TWS_SYNC_INT_TIME	BT_OP(BT_OGF_VS, 0x0086)
struct bt_hci_cp_acts_vs_set_tws_sync_int_time {
	uint16_t handle;
	uint8_t  tws_index;
	uint8_t  time_mode;
	uint32_t rx_bt_clk;
	uint16_t rx_bt_slot;
} __packed;

#define BT_HCI_OP_ACTS_VS_SET_TWS_INT_DELAY_PLAY_TIME	BT_OP(BT_OGF_VS, 0x0087)
struct bt_hci_cp_acts_vs_set_tws_int_delay_play_time {
	uint16_t handle;
	uint8_t  tws_index;
	uint8_t  time_mode;
	uint32_t per_int;
	uint16_t delay_bt_clk;
	uint16_t delay_bt_slot;
} __packed;

#define BT_HCI_OP_ACTS_VS_SET_TWS_INT_CLOCK_NEW		BT_OP(BT_OGF_VS, 0x0088)
struct bt_hci_cp_acts_vs_set_tws_int_clock_new {
	uint16_t handle;
	uint8_t  tws_index;
	uint8_t  time_mode;
	uint32_t bt_clk;
	uint16_t bt_intraslot;
	uint32_t per_bt_clk;
	uint16_t per_bt_intraslot;
} __packed;
#define BT_TWS_NATIVE_TIMER_MODE 0
#define BT_TWS_US_TIMER_MODE	 1

#define BT_HCI_OP_ACTS_VS_ENABLE_TWS_INT_NEW		BT_OP(BT_OGF_VS, 0x0089)
struct bt_hci_cp_acts_vs_enable_tws_int_new {
	uint8_t enable;
	uint8_t tws_index;
} __packed;
#define BT_TWS_INT_ENABLE 1
#define BT_TWS_INT_DISABLE 0

#define BT_HCI_OP_VS_WRITE_BB_REG				BT_OP(BT_OGF_VS, 0x008a)
struct bt_hci_cp_vs_write_bb_reg {
	uint32_t base_addr;
	uint32_t value;
} __packed;

#define BT_HCI_OP_VS_READ_BB_REG				BT_OP(BT_OGF_VS, 0x008b)
struct bt_hci_cp_vs_read_bb_reg {
	uint32_t base_addr;
	uint8_t  size;
} __packed;

#define BT_HCI_OP_VS_WRITE_RF_REG				BT_OP(BT_OGF_VS, 0x008c)
struct bt_hci_cp_vs_write_rf_reg {
	uint16_t base_addr;
	uint16_t value;
} __packed;

#define BT_HCI_OP_VS_READ_RF_REG				BT_OP(BT_OGF_VS, 0x008d)
struct bt_hci_cp_vs_read_rf_reg {
	uint16_t base_addr;
	uint8_t  size;
} __packed;

#define BT_HCI_OP_ACTS_VS_READ_BT_US_CNT			BT_OP(BT_OGF_VS, 0x008e)
struct bt_hci_rp_acts_vs_read_bt_us_cnt {
	uint8_t  status;
	uint32_t cnt;
} __packed;

#define BT_HCI_OP_ACTS_VS_SET_LE_AUDIO_USER_PARAM	BT_OP(BT_OGF_VS, 0x008f)
struct bt_hci_cp_acts_vs_set_le_audio_user_param {
	uint8_t  param_size;
	uint32_t param[4];
} __packed;

#define BT_HCI_OP_ACTS_VS_LE_SET_MASTER_LATENCY		BT_OP(BT_OGF_VS, 0x0090)
struct bt_hci_cp_acts_vs_le_set_master_latency {
	uint16_t handle;
	uint16_t latency;
	uint8_t retry;
} __packed;

/* Vendor events */

#define BT_HCI_EVT_VS_ACL_SNOOP_LINK_COMPLETE		0x01
struct bt_hci_evt_vs_acl_snoop_link_complete {
	uint16_t handle;
	bt_addr_t peer_bdaddr;
	bt_addr_t local_bdaddr;
	uint8_t link_key[16];
} __packed;

#define BT_HCI_EVT_VS_SNOOP_LINK_DISCONNECT			0x02
struct bt_hci_evt_vs_snoop_link_disconnect {
	uint8_t reason;
	uint16_t handle;
} __packed;

#define BT_HCI_EVT_VS_SYNC_SNOOP_LINK_COMPLETE		0x03
#define AIR_MODE_CVSD		0x02
#define AIR_MODE_MSBC		0x03
struct bt_hci_evt_vs_sync_snoop_link_complete {
	uint16_t handle;
	bt_addr_t peer_bdaddr;
	uint8_t link_type;
	uint16_t packet_len;
	uint8_t codec_id;		/* air mode */
} __packed;

#define BT_HCI_EVT_VS_SNOOP_SWITCH_COMPLETE			0x04
struct bt_hci_evt_vs_snoop_switch_complete {
	uint8_t status;
	bt_addr_t peer_bdaddr;
	uint8_t role;		/* 0: normal link, 1: snoop link */
	uint16_t handle;
} __packed;

#define BT_HCI_EVT_VS_SNOOP_MODE_CHANGE				0x05
struct bt_hci_evt_vs_snoop_mode_change {
	uint8_t active;		/* 0: sleep; 1: active */
	uint16_t handle;
} __packed;

#define BT_HCI_EVT_VS_LL_CONN_SWITCH_COMPLETE		0x06
struct bt_hci_evt_vs_ll_conn_switch_complete {
	uint8_t		status;
	uint16_t	handle;
	uint8_t		role;
	uint8_t		peer_address_type;
	bt_addr_t	peer_address;
	uint16_t	interval;
	uint16_t	latency;
	uint16_t	supv_timeout;
	uint8_t		clock_accuracy;
} __packed;

#define BT_HCI_EVT_VS_TWS_MATCH_REPORT				0x07
struct bt_hci_evt_vs_tws_match_report {
	int8_t rssi;
	uint8_t len;
	uint8_t data[0];
} __packed;

#define BT_HCI_EVT_VS_SNOOP_ACTIVE_LINK				0x08
struct bt_hci_evt_vs_snoop_active_link {
	int16_t handle;				/* 0xFFFF: not in work mode, other: work mode handle */
} __packed;

#define BT_HCI_EVT_VS_SYNC_1ST_PKT_CHG			0x09
struct bt_hci_evt_vs_sync_1st_pkt_chg {
    uint16_t handle;
    uint32_t bt_clk;
} __packed;

#define BT_HCI_EVT_VS_TWS_LOCAL_EXIT_SNIFF		0x0a
#define BT_HCI_EVT_VS_TWS_LOCAL_EXIT_SNIFF_TMP	0x0c		/* Controler temporory change 0xa to 0xc */

#define BT_HCI_EVT_PA_SEND_COMPLETE             0x40
struct bt_hci_pa_send_complete {
    uint8_t adv_handle;
//	uint8_t status;
    uint8_t adv_len;
	uint8_t adv_data[30];
} __packed;

#define BT_HCI_EVT_VS_READ_BB_REG_REPORT		0x80
struct bt_hci_rp_vs_read_bb_reg_report {
	uint32_t base_addr;
	uint32_t size;
	uint32_t result[0];
} __packed;

#define BT_HCI_EVT_VS_READ_RF_REG_REPORT		0x81
struct bt_hci_rp_vs_read_rf_reg_report {
	uint16_t base_addr;
	uint16_t size;
	uint16_t result[0];
} __packed;

#define BT_HCI_EVT_LE_VS_CIS_RX_FEATURES		0x0d
struct bt_hci_evt_le_vs_cis_rx_features {
	uint8_t status;
	uint16_t conn_handle;
	/* if ACK exist when BN_S_To_M is 0 */
	uint8_t no_ack;
	/* time from cis_anchor_point to cis_sync_point */
	uint8_t rx_delay[3];
} __packed;

#define BT_HCI_OP_VS_GET_LINK_INFO				   BT_OP(BT_OGF_VS, 0x0020)
struct bt_hci_cp_get_link_info {
	uint16_t conn_handle;
} __packed;

struct bt_hci_rp_get_link_info {
	uint8_t  status;
	uint16_t handle;
	uint8_t  role;
	uint8_t  rssi;				/* db */
	uint16_t tx_power;			/* 0.1db */
	uint8_t  link_quality;		/* 0~100 */
	uint32_t l2cap_in_bytes;
	uint32_t l2cap_out_bytes;
	uint8_t  afh_channel_map[10];
	uint8_t  total_ft_cnt;
	uint8_t  reserved[15];
} __packed;

#define BT_HCI_OP_VS_SET_LINK_CHANNEL_MAP		   BT_OP(BT_OGF_VS, 0x0021)
struct bt_hci_cp_set_link_info {
	uint16_t conn_handle;
	uint8_t  enable;
	uint8_t  afh_channel_map[10];
} __packed;

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_BLUETOOTH_HCI_ACTS_VS_H_ */
