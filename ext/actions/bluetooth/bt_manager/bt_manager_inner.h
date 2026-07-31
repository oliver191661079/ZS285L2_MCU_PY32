/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt manager.
*/

#ifndef SYS_LOG_DOMAIN
#define SYS_LOG_DOMAIN "btm"
#endif
#ifndef SYS_LOG_LEVEL
#define SYS_LOG_LEVEL SYS_LOG_LEVEL_INFO
#endif
#include <logging/sys_log.h>

#ifndef __BT_MANAGER_INNER_H__
#define __BT_MANAGER_INNER_H__
#include <btservice_api.h>
#include <stream.h>
#include "btmgr_tws.h"
#include "bt_manager_audio.h"
#ifdef CONFIG_BT_LETWS
#include "bt_manager_letws.h"
#endif
#include <bt_manager.h>

extern uint8_t phone_controler_role;

#define ABNORMAL_OFFLINE_SHIELDING_SYSTEM_NOTIFICATION 1

#define POWEROFF_WAIT_MIN_TIME			(1)			/* Just for change to process next state */
#define POWEROFF_WAIT_TIMEOUT			(3000)		/* 3s */

#define ACL_DISCONNECTED_DELAY_TIME		5000
enum {
	POFF_STATE_NONE                   = 0,
	POFF_STATE_START                  = 1,
	POFF_STATE_TWS_PROC               = 2,
	POFF_STATE_DISCONNECT_PHONE       = 3,
	POFF_STATE_WAIT_DISCONNECT_PHONE  = 4,
	POFF_STATE_PHONE_DISCONNECTED     = 5,
	POFF_STATE_FINISH                 = 6,
};

typedef struct {
	bd_address_t addr;
	uint8_t addr_type;
	uint8_t addr_valid:1;
	uint8_t conn_type;
	uint8_t time_cnt;
	uint8_t streaming:1;
	uint32_t connect_time;
}br_lea_autoconn_info_t;

void bt_manager_record_halt_phone(void);

void *bt_manager_get_halt_phone(uint8_t *halt_cnt);
int bt_manager_set_status_ext(int state,uint8_t flag, bd_address_t* addr);
int bt_manager_set_status(int state,uint8_t flag);
int bt_manager_lea_set_status(int state,uint8_t flag, bd_address_t* addr);
const char *bt_manager_evt2str(int num, int max_num, const bt_mgr_event_strmap_t *strmap);
bool bt_manager_is_timeout_disconnected(uint32_t reason);
bool bt_manager_is_resources_disconnected(uint32_t reason);

/* bt_manager_base.c */
void btmgr_poff_check_phone_disconnected(void);
void btmgr_poff_set_state_work(uint8_t state, uint32_t time, uint8_t timeout_flag);
void btmgr_poff_state_proc_work(os_work *work);

/* bt_manager_pair.c*/
void bt_manager_enter_pair_mode_ex(void);
void bt_manager_check_visual_mode(void);
void bt_manager_check_link_status(void);
void bt_manager_clear_paired_list_ex(void);
void bt_manager_check_phone_connected(void);
void bt_manager_start_pair_mode_ex(bool stop_reconnect_tws);

/* bt_manager_connect.c */
bool bt_manager_startup_reconnect(void);
void bt_manager_disconnected_reason(void *param);
void bt_manager_sync_auto_reconnect_status(bd_address_t *addr);

/* bt_manager_a2dp.c */
void bt_manager_a2dp_set_sbc_max_bitpool(uint8_t max_bitpool);
int bt_manager_a2dp_profile_start(void);

int bt_manager_a2dp_profile_stop(void);

/* bt_manager_avrcp.c */
int bt_manager_avrcp_profile_start(void);

int bt_manager_avrcp_profile_stop(void);

#ifdef CONFIG_BT_MANAGER_UART_RX
void bt_manager_uart_rx_init(void);
#endif

/* bt_manager_hfp.c */
int bt_manager_hfp_init(void);
int bt_manager_hfp_profile_start(void);

int bt_manager_hfp_profile_stop(void);
int bt_manager_hfp_set_status(uint32_t state);

/* bt_manager_hfp_ag.c */
int bt_manager_hfp_ag_init(void);
int bt_manager_hfp_ag_profile_start(void);

int bt_manager_hfp_ag_profile_stop(void);

/* bt_manager_sco.c */
int bt_manager_hfp_sco_init(void);
int bt_manager_hfp_sco_start(void);

int bt_manager_hfp_sco_stop(void);
int bt_manager_hfp_disconnect_sco(uint16_t acl_hdl);

/* bt_manager_spp.c */
int bt_manager_spp_profile_start(void);
int bt_manager_spp_profile_stop(void);

int bt_manager_hid_profile_start(void);

int bt_manager_hid_profile_stop(void);

int bt_manager_hid_register_sdp();

int bt_manager_did_register_sdp();

/* btmgr_tws */
void bt_manager_tws_init(void);
void bt_manager_tws_deinit(void);
bool bt_manager_tws_powon_auto_pair(void);
void bt_manager_tws_sync_pair_mode(uint8_t pair_mode);
void bt_manager_tws_set_visual_mode_ex(void);
void bt_manager_tws_set_visual_mode(void);
void bt_manager_tws_sync_a2dp_status(uint16_t hdl, void* param);


void sys_event_send_message_new(uint32_t message, uint8_t cmd, void* extra_data, int extra_data_size);


/* bt_manager_config.c */
uint8_t bt_manager_config_get_tws_limit_inquiry(void);
uint8_t bt_manager_config_get_tws_compare_high_mac(void);
uint8_t bt_manager_config_get_tws_compare_device_id(void);
void bt_manager_config_set_pre_bt_mac(uint8_t *mac);
void bt_manager_updata_pre_bt_mac(uint8_t *mac);
bool bt_manager_config_enable_tws_sync_event(void);
bool bt_manager_config_support_a2dp_aac(void);
bool bt_manager_config_support_a2dp_ldac(void);
uint16_t bt_manager_config_volume_sync_delay_ms(void);
uint32_t bt_manager_config_bt_class(void);
uint16_t *bt_manager_config_get_device_id(void);
int bt_manager_config_expect_tws_connect_role(void);
int bt_manager_config_expect_phone_role(void);

/* bt_manager_map.c */
int btmgr_map_time_client_connect(bd_address_t *bd);

/* bt_manager_check_mac_name.c */
void bt_manager_check_mac_name(void);
void bt_manager_bt_mac(uint8_t *mac_str);
#define BT_MANAGER_INVALID_VOLUME  0xff
#define BT_MAX_SAVE_DEV_NUM 3

enum BT_MANAGER_AVRCP_EXT_STATUS {
    BT_MANAGER_AVRCP_EXT_STATUS_NONE    = 0,
    BT_MANAGER_AVRCP_EXT_STATUS_PLAYING = 1,
    BT_MANAGER_AVRCP_EXT_STATUS_PAUSED  = 2,
    BT_MANAGER_AVRCP_EXT_STATUS_SUSPEND = 4,
    BT_MANAGER_AVRCP_EXT_STATUS_PASSTHROUGH_PLAY = 8,
};

typedef struct
{
    bd_address_t  addr;
    u8_t  bt_music_vol;
    u8_t  bt_call_vol;
} bt_mgr_saved_dev_volume_t;

typedef struct bt_mgr_dev_info {
	bd_address_t addr;
	uint16_t hdl;	/* Use hdl replace dev_no, hdl same as bt servcie and stack used */
	uint8_t *name;

	uint16_t used:1;
	uint16_t notify_connected:1;
	uint16_t is_tws:1;
	uint16_t snoop_role:3;
	uint16_t hf_connected:1;
	uint16_t a2dp_connected:1;
	uint16_t avrcp_connected:1;
	uint16_t spp_connected:3;
	uint16_t hid_connected:1;
	uint16_t sco_connected:1;
	uint16_t halt_phone:1;
	uint16_t dev_role:1;
	uint16_t a2dp_singnaling_connected:1;
	uint16_t deemed_disconnected:1;
	uint16_t ios_tts_delay:1;

    uint8_t  a2dp_codec_type;
    uint8_t  a2dp_sample_khz;
    uint8_t  a2dp_stream_started:1;
    uint8_t  a2dp_status_playing:1;
    uint8_t  new_dev:1;
    uint8_t  force_disconnect_by_remote:1;
    uint8_t  timeout_disconnected:1;
    uint8_t  auto_reconnect:1;
    uint8_t  need_resume_play:1;
    uint8_t  a2dp_stream_is_check_started:1;

	uint8_t	 avrcp_remote_support_vol_sync:1;
	uint8_t	 phone_connect_request:1;
	uint8_t	 a2dp_reconnect:1;
	uint8_t	 a2dp_connecting:1;
	uint8_t	 a2dp_reconnect_complete:1;
	uint8_t	 key_miss:1;
	uint8_t	 security:1;
    uint8_t  need_reconnect:1;

	uint8_t  user_pause_pending:1;
    uint16_t self_name_update:1;

	uint8_t  a2dp_indicated_cnt;

    uint8_t  avrcp_remote_vol;
    uint8_t  avrcp_ext_status;  /* BT_MANAGER_AVRCP_EXT_STATUS */
	
    uint8_t  hfp_remote_vol;
	uint8_t  hfp_codec_id;
	uint8_t  hfp_sample_rate;
	
    uint8_t  bt_music_vol;
    uint8_t  bt_call_vol;

	uint8_t  hid_state;
	uint8_t  hid_report_id;

	os_delayed_work hid_delay_work;
	os_delayed_work a2dp_stop_delay_work;
	os_delayed_work hfp_vol_to_remote_timer;
	os_delayed_work resume_play_delay_work;
	os_delayed_work sco_disconnect_delay_work;
	os_delayed_work profile_disconnected_delay_work;
	os_delayed_work a2dp_reconnect_delay_work;

	uint32_t a2dp_connect_time;
	
}bt_mgr_dev_info_t;

typedef struct bt_manager_context_t {
	uint16_t bt_ready:1;
	uint16_t connected_phone_num:2;
	uint16_t tws_mode:1;
    uint16_t wait_connect_work_vaild:1;
	uint16_t pair_mode_work_valid:1;
	uint16_t clear_paired_list_work_valid:1;
	uint16_t clear_paired_list_disc_tws:1;
	uint16_t remote_in_pair_mode:1;
    uint16_t auto_reconnect_startup:1;
	uint16_t stop_reconnect_tws_in_pair_mode:1;
	uint16_t pair_mode_sync_wait_reply:1;
	uint16_t clear_paired_list_sync_wait_reply:1;
    uint16_t clear_paired_list_state_update:1;
	uint16_t enter_pair_mode_state_update:1;
	uint16_t bt_music_auracast_status:1;
	uint16_t bt_temp_comp_stage:2;
	uint16_t auto_reconnect_timeout:1;
	uint16_t update_local_name:1;
	uint16_t personalized_name_update:1;
	uint16_t ios_tts_delay:1;
	int16_t  bt_comp_last_temp;
	uint8_t bt_state;
	uint8_t pair_mode_state;
	uint8_t clear_paired_list_state;
	uint16_t pair_mode_timeout;
	uint16_t clear_paired_list_timeout;
	uint8_t pair_status;
	uint8_t remote_pair_state;
	uint8_t dis_reason;
	uint8_t visual_mode;
	uint8_t scan_mode;
	uint8_t tws_dis_reason;
	bt_mgr_dev_info_t dev[MAX_MGR_DEV];
#if 1//#ifndef CONFIG_BT_EARPHONE_SPEC
	bd_address_t halt_addr[MAX_MGR_DEV];
	int halt_dev_num;
#endif
	bt_mgr_saved_dev_volume_t  saved_dev_volume[BT_MAX_SAVE_DEV_NUM];
	uint16_t cur_a2dp_hdl;

	os_delayed_work wait_connect_work;
	os_delayed_work pair_mode_work;
	os_delayed_work tws_pair_search_work;
	os_delayed_work clear_paired_list_work;
	os_delayed_work bt_temp_comp_timer;
	os_delayed_work role_switch_work;
	os_delayed_work tts_delay_work;
	os_delayed_work auto_reconnect_work;
	os_delayed_work auto_reconnect_second_dev_work;

	/* For process power off */
	uint8_t poweroff_state:4;
	uint8_t local_req_poweroff:1;
	uint8_t remote_req_poweroff:1;
	uint8_t single_poweroff:1;
	uint8_t poweroff_wait_timeout:1;
	uint8_t local_later_poweroff:1;
	uint8_t cur_bqb_mode:1;
	os_mutex poweroff_mutex;
	os_delayed_work poweroff_proc_work;

	/* per adv info */
	uint8_t total_per_sync_count;
	uint8_t per_synced_count;

	bt_manager_event_callback_t event_callback;
}bt_manager_context_t;

struct bt_manager_context_t* bt_manager_get_context(void);

/* bt_manager_dev_info.c */
void bt_manager_init_dev_volume(void);
void bt_manager_clear_dev_volume(void);
void bt_manager_save_dev_volume(void);
void bt_mgr_add_dev_info(bd_address_t *addr, uint16_t hdl);
void bt_mgr_free_dev_info(struct bt_mgr_dev_info *info);
struct bt_mgr_dev_info *bt_mgr_find_dev_info(bd_address_t *addr);
struct bt_mgr_dev_info *bt_mgr_find_dev_info_by_hdl(uint16_t hdl);

void bt_mgr_disconnect_other_connected_dev(uint16_t hdl);
bt_mgr_dev_info_t* bt_mgr_get_a2dp_active_dev(void);
bt_mgr_dev_info_t* bt_mgr_get_last_a2dp_connected_dev(void);
bt_mgr_dev_info_t* bt_mgr_get_hfp_active_dev(void);
bool btmgr_is_snoop_link_created(void);


void bt_manager_avrcp_sync_vol_to_local(uint16_t hdl, uint8_t music_vol, bool sync_to_remote_pending);
void bt_manager_avrcp_sync_origin_vol(uint16_t hdl);
void bt_manager_avrcp_sync_playing_vol(uint16_t hdl);
int bt_manager_avrcp_sync_vol_to_remote_by_addr(uint16_t hdl, uint32_t music_vol, uint16_t delay_ms);

void bt_manager_hfp_sync_vol_to_local(uint16_t hdl, uint8_t call_vol);
void bt_manager_hfp_sync_origin_vol(uint16_t hdl);

void do_phone_volume(void);
void bt_manager_sync_volume_from_phone(bd_address_t *addr, bool is_avrcp/*true:avrcp,false:hfp*/, uint8_t volume, bool sync_wait, bool notify_app);
int  bt_manager_sync_volume_from_phone_leaudio(bd_address_t *addr, uint8_t volume);

void bt_manager_sys_event_notify(int event);
void bt_manager_update_led_display(void);

void bt_manager_a2dp_status_stopped(bt_mgr_dev_info_t* dev_info, uint16_t delay_check_ms);
void bt_manager_a2dp_stop_delay_proc(os_work *work);
void bt_manager_hfp_sync_origin_vol_proc(os_work *work);

void bt_manager_auto_reconnect_resume_play(uint16_t hdl);
void bt_manager_resume_play_proc(os_work *work);

void bt_manager_a2dp_cancel_hfp_status(bt_mgr_dev_info_t* dev_info);
void bt_manager_hfp_sco_disconnect_proc(os_work *work);

void bt_manager_profile_disconnected_delay_proc(os_work* work);
void bt_manager_a2dp_reconnect_delay_proc(os_work *work);
/*
 * LE Audio
 */
int bt_manager_le_audio_init(struct bt_audio_role *role);
int bt_manager_le_audio_exit(void);
int bt_manager_le_audio_keys_clear(void);
int bt_manager_le_audio_start(void);
int bt_manager_le_audio_stop(void);
int bt_manager_lea_services_enable(void);
int bt_manager_lea_services_disable(void);
int bt_manager_le_audio_pause(void);
int bt_manager_le_audio_resume(void);
int bt_manager_le_audio_server_cap_init(struct bt_audio_capabilities *caps);
int bt_manager_le_audio_stream_config_codec(struct bt_audio_codec *codec);
int bt_manager_le_audio_stream_prefer_qos(struct bt_audio_preferred_qos *qos);
int bt_manager_le_audio_stream_bind_qos(struct bt_audio_qoss *qoss, uint8_t index);
int bt_manager_le_audio_stream_config_qos(struct bt_audio_qoss *qoss);
int bt_manager_le_audio_stream_sync_forward(struct bt_audio_chan **chans,
				uint8_t num_chans);
int bt_manager_le_audio_stream_enable(struct bt_audio_chan **chans,
				uint8_t num_chans, uint32_t contexts);
int bt_manager_le_audio_stream_disable(struct bt_audio_chan **chans,
				uint8_t num_chans);
io_stream_t bt_manager_le_audio_source_stream_create(struct bt_audio_chan **chans,
				uint8_t num_chans, uint32_t locations);
int bt_manager_le_audio_source_stream_set(struct bt_audio_chan **chans,
				uint8_t num_chans, io_stream_t stream,
				uint32_t locations);
int bt_manager_le_audio_sink_stream_set(struct bt_audio_chan *chan,
				io_stream_t stream);
int bt_manager_le_audio_sink_stream_start(struct bt_audio_chan **chans,
				uint8_t num_chans);
int bt_manager_le_audio_sink_stream_stop(struct bt_audio_chan **chans,
				uint8_t num_chans);
int bt_manager_le_audio_stream_update(struct bt_audio_chan **chans,
				uint8_t num_chans, uint32_t contexts);
int bt_manager_le_audio_stream_release(struct bt_audio_chan **chans,
				uint8_t num_chans);
int bt_manager_le_audio_stream_released(struct bt_audio_chan *chan);
int bt_manager_le_audio_stream_disconnect(struct bt_audio_chan **chans,
				uint8_t num_chans);
void *bt_manager_le_audio_stream_bandwidth_alloc(struct bt_audio_qoss *qoss);
int bt_manager_le_audio_stream_bandwidth_free(void *cig_handle);
int bt_manager_le_audio_stream_cb_register(struct bt_audio_stream_cb *cb);
int bt_manager_le_audio_stream_set_tws_sync_cb_offset(struct bt_audio_chan *chan,int32_t offset_from_syncdelay);

int bt_manager_le_volume_up(uint16_t handle);
int bt_manager_le_volume_down(uint16_t handle);
int bt_manager_le_volume_set(uint16_t handle, uint8_t value);
int bt_manager_le_volume_mute(uint16_t handle);
int bt_manager_le_volume_unmute(uint16_t handle);
int bt_manager_le_volume_unmute_up(uint16_t handle);
int bt_manager_le_volume_unmute_down(uint16_t handle);
int bt_manager_le_mic_mute(uint16_t handle);
int bt_manager_le_mic_unmute(uint16_t handle);
int bt_manager_le_mic_mute_disable(uint16_t handle);
int bt_manager_le_mic_gain_set(uint16_t handle, uint8_t gain);
int bt_manager_le_media_mcs_state_get(uint16_t handle);
int bt_manager_le_media_play(uint16_t handle);
int bt_manager_le_media_pause(uint16_t handle);
int bt_manager_le_media_fast_rewind(uint16_t handle);
int bt_manager_le_media_fast_forward(uint16_t handle);
int bt_manager_le_media_stop(uint16_t handle);
int bt_manager_le_media_play_previous(uint16_t handle);
int bt_manager_le_media_play_next(uint16_t handle);
int bt_manager_le_call_originate(uint16_t handle, uint8_t *remote_uri);
int bt_manager_le_call_accept(struct bt_audio_call *call);
int bt_manager_le_call_hold(struct bt_audio_call *call);
int bt_manager_le_call_retrieve(struct bt_audio_call *call);
int bt_manager_le_call_terminate(struct bt_audio_call *call);

int bt_manager_le_update_adv_data(struct bt_audio_config* cfg);

uint8_t bt_manager_audio_get_active_app(uint8_t* active_app, bd_address_t* active_dev_addr,bd_address_t* interrupt_dev_addr);

uint8_t bt_manager_audio_sync_remote_active_app(uint8_t active_app, bd_address_t* active_dev_addr,bd_address_t* interrupt_dev_addr);

int bt_manager_is_poweroffing(void);

int bt_manager_audio_adv_param_init(bt_addr_le_t *peer);
int bt_manager_audio_get_peer_addr(bt_addr_le_t* peer);
int bt_manager_audio_save_peer_addr(bt_addr_le_t* peer);

int bt_manager_audio_in_btcall(void);
int bt_manager_audio_in_btmusic(void);


int bt_manager_audio_update_volume(bd_address_t* addr, uint8_t vol);
bd_address_t* bt_manager_audio_lea_addr(void);
int bt_manager_audio_tws_connected(void);
void bt_manager_tws_sync_enable_lea_dir_adv_inner(void* param);

void bt_manager_audio_set_takeover_flag(uint8_t flag);

void bt_manager_check_per_adv_synced(void);

int bt_manager_disconnect_inactive_audio_conn(void);

int bt_manager_disconnect_other_audio_conn(uint8_t *addr, bool only_phone_dev);

int check_audio_conn_preemption_pre_dev(bd_address_t* new_dev_addr, uint16_t handle, int type);

#ifdef CONFIG_BT_PTS_TEST
uint8_t bt_pts_is_enabled(void);
#ifdef CONFIG_BT_LEA_PTS_TEST
int bt_manager_le_pts_test_start(void);
int bt_manager_le_pts_test_stop(void);
void bt_manger_lea_event_pts_process(btsrv_audio_event_e event, void *data, int size);
void pts_le_audio_service_disable(void);
#endif /*CONFIG_BT_LEA_PTS_TEST*/
#endif /*CONFIG_BT_PTS_TEST*/

void bt_manager_enter_pair_mode_inner(void);
int bt_manager_reconnect_one_phone(void);
int bt_manager_reconnect_one_phone_cancel(void);
bool bt_manager_audio_has_phone_encrypted(void);

bool bt_manager_audio_has_same_lea_notify_connected(bd_address_t* addr);
bool bt_manager_audio_has_same_br_notify_connected(bd_address_t* addr);

void bt_manager_audio_set_disconnect_info_need_update(uint8_t need_update);
int bt_manager_audio_add_all_dev_to_disconnect_info(void);
int bt_manager_audio_get_auto_reconnect_info(br_lea_autoconn_info_t *info, uint8_t max_cnt);
bool bt_manager_audio_is_phone_connected(bd_address_t *addr, uint8_t type);
#endif
