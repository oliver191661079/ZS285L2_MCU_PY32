/*
 * Copyright (c) 2016 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt service tws snoop
 */

#ifndef _BTSRV_TWS_SNOOP_H_
#define _BTSRV_TWS_SNOOP_H_

typedef enum {
    TWS_STATE_NONE,
	TWS_STATE_INIT,
	TWS_STATE_PAIR_SEARCH,
	TWS_STATE_CONNECTING,
	TWS_STATE_DISCONNECTING,
	TWS_STATE_DETECT_ROLE,
	TWS_STATE_CONNECTED,
	TWS_STATE_READY_PLAY,
	TWS_STATE_RESTART_PLAY,
	TWS_STATE_MUTIL_PHONE_CONFLICT,
    TWS_STATE_POWER_OFF,
} btsrv_tws_state_e;

#define btsrv_tws_updata_tws_mode(a, b)
#define btsrv_tws_protocol_data_cb(a, b, c)		false
#define btsrv_tws_hfp_start_callout(a)
#define btsrv_tws_hfp_stop_call()
#define btsrv_connect_check_switch_sbc()
#define btsrv_connect_proc_switch_sbc_state(a, b)
#define btsrv_tws_set_codec(a)

/* btsrv_tws_base.c */
bool btsrv_tws_check_is_tws_dev_connect(bt_addr_t *addr,uint8_t *name);
void btsrv_tws_get_pairing_connecting_state(bool *pairing, bool *connecting);
void btsrv_tws_sync_reconnect_info(bd_address_t *addr);
int btsrv_tws_is_in_switch_state(void);
int btsrv_tws_is_in_connecting(void);
int btsrv_tws_is_in_pair_searching(void);
bool btsrv_tws_is_advanced_mode(void);
int btsrv_tws_can_do_pair(void);
int btsrv_tws_set_bt_local_play(bool bt_play, bool local_play);
void btsrv_tws_set_visual_param(tws_visual_parameter_t *param);
void btsrv_tws_set_pair_param(tws_pair_parameter_t *param);
tws_visual_parameter_t * btsrv_tws_get_visual_param(void);
tws_pair_parameter_t * btsrv_tws_get_pair_param(void);
uint8_t btsrv_tws_get_pair_keyword(void);
void btsrv_tws_set_pair_keyword(uint8_t pair_keyword);
void btsrv_tws_set_pair_addr_match(bool enable,bd_address_t *addr);
uint8_t btsrv_tws_get_pair_addr_match(void);
void btsrv_tws_set_rssi_threshold(int8_t threshold);
int8_t btsrv_tws_get_rssi_threshold(void);
void btsrv_tws_update_sco_codec(struct bt_conn *conn);
uint8_t btsrv_tws_get_state(void);
void btsrv_tws_update_visual_pair_mode(void);
void btsrv_tws_notify_start_power_off(void);
void btsrv_tws_set_expect_role(uint8_t role);
void btsrv_tws_set_leaudio_active(int8_t leaudio_active);


/* btsrv_tws_main.c */
int btsrv_tws_process(struct app_msg *msg);
int btsrv_tws_context_init(void);
void btsrv_tws_dump_info(void);

/* btsrv_tws_device.c */
int btsrv_tws_connect_to(bt_addr_t *addr, uint8_t try_times, uint8_t req_role);
void btsrv_tws_cancal_auto_connect(void);
void btsrv_tws_set_input_stream(io_stream_t stream, bool user_set);
void btsrv_tws_active_sco_disconnected(void);
void btsrv_tws_set_sco_input_stream(io_stream_t stream);
void btsrv_tws_hfp_set_1st_sync(struct bt_conn *conn);
bool btsrv_tws_snoop_conn_is_connected_by_hdl(uint16_t hdl, uint8_t type);


/* btsrv_tws_monitor.c */
tws_runtime_observer_t *btsrv_tws_monitor_get_observer(void);
uint32_t btsrv_tws_get_bt_clock(void);
int btsrv_tws_set_irq_time(uint32_t bt_clock_ms);
int btsrv_tws_wait_ready_send_sync_msg(uint32_t wait_timeout);

/* btsrv_tws_protocol.c */
int btsrv_tws_send_user_command(uint8_t *data, int len);
int btsrv_tws_send_user_command_sync(uint8_t *data, int len);
void btsrv_tws_process_act_fix_cid_data(struct bt_conn *conn, uint16_t cid, uint8_t *data, uint16_t len);

/* btsrv_tws_version_feature.c */
bool btsrv_tws_check_support_feature(uint32_t feature);
bool btsrv_tws_check_is_woodpecker(void);

/* btsrv_tws_sync_info.c */
void btsrv_tws_sync_base_info_to_remote(struct bt_conn *conn);
void btsrv_tws_sync_paired_list_to_remote(void);
void btsrv_tws_sync_base_info_to_local_ext(void *data, uint16_t len);
void btsrv_tws_sync_a2dp_info_to_remote(struct bt_conn *conn, uint8_t cb_ev);
void btsrv_tws_sync_avrcp_info_to_remote(struct bt_conn *conn, uint8_t cb_ev);
void btsrv_tws_sync_hfp_info_to_remote(struct bt_conn *conn, uint8_t cb_ev);
void btsrv_tws_sync_hfp_cbev_data_to_remote(struct bt_conn *conn, uint8_t cb_ev, uint8_t *data, uint8_t len);
void btsrv_tws_sync_spp_info_to_remote(struct bt_conn *conn, uint8_t cb_ev);
void btsrv_tws_sync_base_info_to_remote_ext(struct bt_conn *conn);

/* btsrv_tws_snoop_link.c */
bool btsrv_tws_is_snoop_active_link(struct bt_conn *conn);

/* btsrv_tws_snoop.c */
uint8_t btsrv_adapter_tws_check_role(bt_addr_t *addr, uint8_t *name);
void btsrv_tws_sync_info_type(struct bt_conn *conn, uint8_t type, uint8_t cb_ev);
void btsrv_tws_sync_hfp_cbev_data(struct bt_conn *conn, uint8_t cb_ev, uint8_t *data, uint8_t len);

#endif
