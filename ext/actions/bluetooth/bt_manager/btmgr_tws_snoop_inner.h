/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt manager tws snoop inner.
 */
#include "list.h"


/* Tws poweoff state */
enum {
	TWS_POFFS_NONE                    = 0,
	TWS_POFFS_START                   = 1,
	TWS_POFFS_CMD_WAIT_ACK            = 2,
	TWS_POFFS_RX_ACK                  = 3,
	TWS_POFFS_WAIT_SWTO_MASTER        = 4,	/* Wait swith to master */
	TWS_POFFS_WAIT_SWTO_SLAVE         = 5,	/* Wait swith to slave */
	TWS_POFFS_WAIT_SNOOP_DISCONNECTED = 6,	/* Wait snoop link disconnected */
	TWS_POFFS_SNOOP_DISCONNECTED      = 7,	/* All snoop link disconnected */
	TWS_POFFS_WAIT_DISCONNECTED       = 8,	/* Wait tws disconnected */
	TWS_POFFS_WAIT_FORCE_DISCONNECTED = 9,	/* Wait tws force disconnected */
	TWS_POFFS_DISCONNECTED            = 10,	/* Tws disconnected */
};

struct tws_sync_event_t {
	sys_snode_t node;
	uint32_t event;
	uint32_t bt_clock;
};

struct btmgr_tws_context_t {
	sys_slist_t tws_timer_list;
	os_work tws_irq_work;
	os_delayed_work tws_reconnect_dev_work;

	uint8_t tws_role:3;
	uint8_t slave_actived:1;
	uint8_t le_remote_addr_valid:1;
	uint8_t halt_aac:1;
	uint8_t halt_ldac:1;
	uint8_t init:1;

	uint16_t peer_version;
	uint32_t peer_feature;
	uint8_t source_stream_type;
	uint8_t sink_tws_mode;
	uint8_t sink_drc_mode;
	uint8_t sink_volume;

	uint8_t tws_reply_event;
	uint8_t	tws_reply_value;
	bt_addr_le_t remote_ble_addr;

	/* For tws poweoff */
	uint8_t poweroff_state;
	uint8_t sync_lea_broadcast_enable;
	tws_config_expect_role_cb_t expect_role_cb;
};


typedef struct
{
    bd_address_t  bd_addr;
	uint16_t hdl;
    u16_t  a2dp_stream_started    : 1;
    u16_t  a2dp_status_playing    : 1;
    u16_t  bt_music_vol           : 5;
    u16_t  bt_call_vol            : 5;
    u16_t  avrcp_ext_status       : 3;  /* BT_MANAGER_AVRCP_EXT_STATUS */
} bt_manager_tws_sync_phone_info_t;


typedef struct
{
    uint8_t  sirk[16];
	uint8_t  rank;
	uint8_t  peer_valid;
	bt_addr_le_t peer;
} bt_manager_tws_sync_lea_info_t;

typedef struct
{
    bd_address_t  active_addr;
    bd_address_t  interrupt_active_addr;	
    uint8_t  active_app ;
} bt_manager_tws_sync_active_dev_t;



/* btmgr_tws_snoop.c */
struct btmgr_tws_context_t *btmgr_get_tws_context(void);

/* btmgr_tws_snoop_ui_event.c */
void btmgr_tws_update_btplay_mode(void);
void btmgr_tws_process_ui_event(uint8_t *data, uint8_t len);
int btmgr_tws_event_through_upper(uint8_t *data, uint8_t len);


/**
 * @brief bt manager reply tws sync message
 *
 * This routine provides app reply sync tws message, when receive tws sync message .
 *
 * @param event_type  :  TWS_SHORT_MSG_EVENT.
 * @param reply_event :  received app event by applicant define.
 * @param reply_value :  TWS_SYNC_REPLY_ACK or TWS_SYNC_REPLY_NACK.
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_tws_reply_sync_message(uint8_t event_type, uint8_t reply_event, uint8_t reply_value);


/* btmgr_tws_snoop_poweroff.c */
void btmgr_tws_poff_check_tws_role_change(void);
void btmgr_tws_poff_check_tws_disconnected(void);
void btmgr_tws_poff_info_proc(uint8_t *data, uint8_t len);
void btmgr_tws_poff_set_state_work(uint8_t main_state, uint8_t tws_state, uint32_t time, uint8_t timeout_flag);


typedef struct
{
	struct list_head node;
	tws_timer_cb_t timer_func;
	void* timer_param;
	uint32_t  bt_clock; 	 
} tws_timer_info_t;

int tws_timer_init(void);
int tws_timer_deinit(void);
int tws_timer_clear(void);
uint32_t tws_timer_get_near_btclock(void);


