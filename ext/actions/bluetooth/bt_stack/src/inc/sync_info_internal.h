/*
 * sync_info_internal.h

 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <acts_bluetooth/conn.h>
#include <acts_bluetooth/avdtp.h>
#include "spp_internal.h"

#define BT_STACK_SYNC_INFO_MAGIC			0xAA55
#define BT_STACK_SYNC_INFO_VERSOIN_1_0		0x0100

struct stack_sync_conn_info {
	uint8_t 		role;
	uint8_t 		encrypt;
	bt_security_t	sec_level;
	bt_security_t	required_sec_level;

	/* Br info */
	uint8_t			remote_io_capa;
	uint8_t			remote_auth;
	uint8_t			pairing_method;
	uint8_t			features[2][8];
	uint16_t		esco_pkt_type;
};

struct stack_sync_a2dp_info {
	uint8_t info_valid;
	/* signal/media session info */
	uint16_t signal_rx_cid;
	uint16_t signal_rx_mtu;
	uint16_t signal_tx_cid;
	uint16_t signal_tx_mtu;
	uint16_t media_rx_cid;
	uint16_t media_rx_mtu;
	uint16_t media_tx_cid;
	uint16_t media_tx_mtu;
	uint8_t signal_intacp_role:1;
	uint8_t media_intacp_role:1;
	/* stream info */
	uint8_t lsid:6;
	uint8_t rsid:6;
	uint8_t delay_report:1;
	uint8_t cp_type;
	uint8_t stream_state;
	uint8_t acp_state;
	uint8_t int_state;
	struct bt_a2dp_media_codec codec;
	/* req info */
	uint8_t cmdtid;
};

struct stack_sync_avrcp_info {
	uint8_t info_valid;
	uint16_t rx_cid;
	uint16_t rx_mtu;
	uint16_t tx_cid;
	uint16_t tx_mtu;
	uint16_t l_tg_ebitmap;				/* Local target support notify event bit map */
	uint16_t r_tg_ebitmap;				/* Remote target support notify event bit map */
	uint32_t r_reg_notify_interval;		/* Remote register notify interval */
	uint32_t l_reg_notify_event;		/* Local register notify event*/
	uint32_t r_reg_notify_event;		/* Remote register notify event*/
	uint8_t CT_state;
	uint8_t ct_tid;
	uint8_t tg_tid;
	uint8_t tg_notify_tid;
};

struct stack_sync_rfcomm_info {
	uint8_t info_valid;
	uint16_t rx_cid;
	uint16_t rx_mtu;
	uint16_t tx_cid;
	uint16_t tx_mtu;
	uint16_t mtu;
	uint8_t state;
	uint8_t role;
	uint8_t cfc;
	uint8_t fc;
};

struct stack_sync_hf_info {
	uint8_t info_valid;
	uint8_t dlc_role;
	uint16_t dlc_mtu;
	uint8_t dlc_dlci;
	uint8_t dlc_state;
	uint8_t dlc_rx_credit;
	uint8_t dlc_tx_credit;
	uint8_t dlc_msc_state;
	uint32_t ag_features;
	int8_t ind_table[20];		/* Same as HF_MAX_AG_INDICATORS */
	uint8_t codec_id;
	uint8_t slc_step;
};

struct stack_sync_spp_info {
	uint8_t info_valid;
	uint8_t spp_id;
	uint8_t state:4;
	uint8_t active_connect:1;
	uint8_t dlc_role;
	uint16_t dlc_mtu;
	uint8_t dlc_dlci;
	uint8_t dlc_state;
	uint8_t dlc_rx_credit;
	uint8_t dlc_tx_credit;
	uint8_t dlc_msc_state;
};

struct stack_sync_br_link_info {
	uint16_t magic;
	uint16_t info_version;
	bt_addr_t addr;
	struct stack_sync_conn_info conn_info;
	struct stack_sync_a2dp_info a2dp_info;
	struct stack_sync_avrcp_info avrcp_info;
	struct stack_sync_rfcomm_info rfcomm_info;
	struct stack_sync_hf_info hf_info;
	struct stack_sync_spp_info spp_info[CFG_BT_MAX_SPP_CHANNEL];
};

struct stack_sync_le_link_info {
	uint16_t magic;
	uint16_t info_version;
	bt_addr_le_t addr;

	uint16_t att_rx_mtu;
	uint16_t att_tx_mtu;

	uint8_t				id;
	uint8_t				encrypt;
	bt_security_t		sec_level;
	bt_security_t		required_sec_level;
	uint16_t			interval;
	uint16_t			interval_min;
	uint16_t			interval_max;
	uint16_t			latency;
	uint16_t			timeout;
	uint16_t			pending_latency;
	uint16_t			pending_timeout;
	uint8_t				features[8];

#if defined(CONFIG_BT_USER_PHY_UPDATE)
	struct bt_conn_le_phy_info      phy;
#endif

#if defined(CONFIG_BT_USER_DATA_LEN_UPDATE)
	struct bt_conn_le_data_len_info data_len;
#endif
};
