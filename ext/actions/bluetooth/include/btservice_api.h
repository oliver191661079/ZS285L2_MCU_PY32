/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file bt service interface
 */

#ifndef _BTSERVICE_API_H_
#define _BTSERVICE_API_H_
#include <stream.h>
#include <btservice_base.h>
#include <btservice_tws_api.h>
#include <acts_bluetooth/sdp.h>
#include <acts_bluetooth/audio/common.h>
#include <bt_act_event_id.h>

/**
 * @defgroup bt_service_apis Bt Service APIs
 * @ingroup bluetooth_system_apis
 * @{
 */

enum {
	CONTROLER_ROLE_MASTER,
	CONTROLER_ROLE_SLAVE,
};

/** btsrv auto conn mode */
enum btsrv_auto_conn_mode_e {
	/** btsrv auto connet device alternate */
	BTSRV_AUTOCONN_ALTERNATE,
	/** btsrv auto connect by device order */
	BTSRV_AUTOCONN_ORDER,
};

enum btsrv_disconnect_mode_e {
	/**  Disconnect all device */
	BTSRV_DISCONNECT_ALL_MODE,
	/** Just disconnect all phone */
	BTSRV_DISCONNECT_PHONE_MODE,
	/** Just disconnect tws */
	BTSRV_DISCONNECT_TWS_MODE,
};

enum btsrv_device_type_e {
	/** all device */
	BTSRV_DEVICE_ALL,
	/** Just phone device*/
	BTSRV_DEVICE_PHONE,
	/** Just tws device*/
	BTSRV_DEVICE_TWS,
};

enum btsrv_stop_auto_reconnect_mode_e {
	BTSRV_STOP_AUTO_RECONNECT_NONE    = 0x00,
	/** Just phone device*/
	BTSRV_STOP_AUTO_RECONNECT_PHONE   = 0x01,
	/** Just tws device*/
	BTSRV_STOP_AUTO_RECONNECT_TWS     = 0x02,
	/** all device */
	BTSRV_STOP_AUTO_RECONNECT_ALL     = 0x03,
	/** Just for addr device*/
	BTSRV_STOP_AUTO_RECONNECT_BY_ADDR = 0x04,

	BTSRV_SYNC_AUTO_RECONNECT_STATUS  = 0x08,

};

enum btsrv_scan_mode_e
{
    BTSRV_SCAN_MODE_DEFAULT_INQUIRY_PAGE,
    BTSRV_SCAN_MODE_FAST_PAGE,
    BTSRV_SCAN_MODE_FAST_PAGE_EX,
    BTSRV_SCAN_MODE_NORMAL_PAGE,
    BTSRV_SCAN_MODE_NORMAL_PAGE_S3,
    BTSRV_SCAN_MODE_NORMAL_PAGE_EX,
    BTSRV_SCAN_MODE_FAST_INQUIRY_PAGE,
    BTSRV_SCAN_MODE_MAX,
};

enum btsrv_visual_mode_e
{
    BTSRV_VISUAL_MODE_NODISC_NOCON,
    BTSRV_VISUAL_MODE_DISC_NOCON,
    BTSRV_VISUAL_MODE_NODISC_CON,
    BTSRV_VISUAL_MODE_DISC_CON,
    BTSRV_VISUAL_MODE_LIMIT_DISC_CON,
};

enum btsrv_pair_status_e {
    BT_PAIR_STATUS_NONE             = 0x0000,
    BT_PAIR_STATUS_WAIT_CONNECT     = 0x0001,
	BT_PAIR_STATUS_PAIR_MODE        = 0x0002,
	BT_PAIR_STATUS_TWS_SEARCH       = 0x0004,
    BT_PAIR_STATUS_RECONNECT        = 0x0008,
    BT_PAIR_STATUS_NODISC_NOCON     = 0x0010,
};

enum {
	BTSRV_PAIR_KEY_MODE_NONE        = 0,
    BTSRV_PAIR_KEY_MODE_ONE         = 1,
    BTSRV_PAIR_KEY_MODE_TWO         = 2,
};

enum btsrv_tws_reconnect_mode_e {
    BTSRV_TWS_RECONNECT_NONE          = 0x0000,
    BTSRV_TWS_RECONNECT_AUTO_PAIR     = 0x0001,
	BTSRV_TWS_RECONNECT_FAST_PAIR     = 0x0002,
};
/** BR inquiry/page scan parameter */
struct bt_scan_parameter {
	unsigned char   scan_mode;
	unsigned short	inquiry_scan_window;
	unsigned short	inquiry_scan_interval;
	unsigned char   inquiry_scan_type;
	unsigned short	page_scan_window;
	unsigned short	page_scan_interval;
	unsigned char	page_scan_type;
};

struct bt_scan_policy {
	uint8_t   scan_type;
	uint8_t	  scan_policy_mode;
};

/** device auto connect info */
struct autoconn_info {
	/** bluetooth device address */
	bd_address_t addr;
	/** bluetooth device address valid flag*/
	uint8_t addr_valid:1;
	/** bluetooth device tws role */
	uint8_t tws_role:3;
	/** bluetooth device need connect a2dp */
	uint8_t a2dp:1;
	/** bluetooth device need connect avrcp */
	uint8_t avrcp:1;
	/** bluetooth device need connect hfp */
	uint8_t hfp:1;
	/** bluetooth connect hfp first */
	uint8_t hfp_first:1;
	/** bluetooth device need connect hid */
	uint8_t hid:1;
	uint8_t active:1;
    uint8_t ios:1;
    uint8_t pc:1;
};

struct btsrv_config_info {
	/** Max br connect device number */
	uint8_t max_conn_num:4;
	/** Max br phone device number */
	uint8_t max_phone_num:4;
    uint8_t cfg_device_num:2;
	/** Set pts test mode */
	uint8_t pts_test_mode:1;
	/** Set pts test mode */
	uint8_t double_preemption_mode:1;
	uint8_t pair_key_mode:2;
    uint8_t power_on_auto_pair_search:1;
	uint8_t default_state_discoverable:1;
    uint8_t bt_not_discoverable_when_connected:1;
	uint8_t use_search_mode_when_tws_reconnect:1;
	uint8_t device_l_r_match:1;
	uint8_t sniff_enable:1;
	uint8_t stop_another_when_one_playing:1;
	uint8_t resume_another_when_one_stopped:1;
	uint8_t clear_link_key_when_clear_paired_list:1;
	uint8_t cfg_support_tws:1;
	uint8_t exit_sniff_when_bis:1;
    uint8_t default_state_wait_connect_sec;
	uint16_t a2dp_status_stopped_delay_ms;
	uint16_t idle_enter_sniff_time;
	uint16_t sniff_interval;
	/** delay time for volume sync */
	uint16_t volume_sync_delay_ms;
	/** Tws version */
	uint16_t tws_version;
	/** Tws feature */
	uint32_t tws_feature;
};

/** auto connect info */
struct bt_set_autoconn {
	bd_address_t addr;
	uint8_t strategy;
	uint8_t base_try;
	uint8_t profile_try;
    uint8_t reconnect_mode;
	uint8_t base_interval;
	uint8_t profile_interval;
    uint8_t reconnect_tws_timeout;
    uint8_t reconnect_phone_timeout;
};

struct bt_set_user_visual {
	uint8_t enable;
    uint8_t discoverable;
	uint8_t connectable;
    uint8_t scan_mode;
};

struct bt_linkkey_info {
	bd_address_t addr;
	uint8_t linkkey[16];
};

struct bt_update_qos_param {
    void * conn;
    uint8_t flags;
    uint8_t service_type;
    uint32_t token_rate;
    uint32_t peak_bandwidth;
    uint32_t latency;
    uint32_t delay_variation;
};

enum {
	BT_LINK_EV_ACL_CONNECT_REQ,
	BT_LINK_EV_ACL_CONNECTED,
	BT_LINK_EV_ACL_DISCONNECTED,
	BT_LINK_EV_GET_NAME,
	BT_LINK_EV_ROLE_CHANGE,
	BT_LINK_EV_KEY_MISS,
	BT_LINK_EV_SECURITY_CHANGED,
	BT_LINK_EV_HF_CONNECTED,
	BT_LINK_EV_HF_DISCONNECTED,
	BT_LINK_EV_A2DP_CONNECTED,
	BT_LINK_EV_A2DP_DISCONNECTED,
	BT_LINK_EV_AVRCP_CONNECTED,
	BT_LINK_EV_AVRCP_DISCONNECTED,
	BT_LINK_EV_SPP_CONNECTED,
	BT_LINK_EV_SPP_DISCONNECTED,
	BT_LINK_EV_HID_CONNECTED,
	BT_LINK_EV_HID_DISCONNECTED,
	BT_LINK_EV_SNOOP_ROLE,
    BT_LINK_EV_A2DP_SINGALING_CONNECTED,
    BT_LINK_EV_A2DP_CHANNEL_ERR,
};

struct bt_link_cb_param {
	uint8_t link_event;
	uint8_t is_tws:1;
	uint8_t new_dev:1;
	uint8_t key_miss:1;
	uint8_t param;
	uint16_t hdl;
	bd_address_t *addr;
	uint8_t *name;
};

struct bt_connected_cb_param {
	uint8_t acl_connected:1;	/* Just BR acl connected */
	bd_address_t *addr;
	uint8_t *name;
};

struct bt_disconnected_cb_param {
	uint8_t acl_disconnected:1;	/* BR acl disconnected */
	bd_address_t addr;
	uint8_t reason;
};

/** bt device disconnect info */
struct bt_disconnect_reason {
	/** bt device addr */
	bd_address_t addr;
	/** bt device disconnected reason */
	uint8_t reason;
	/** bt device tws role */
	uint8_t tws_role;
    uint8_t conn_type;
};

/* Bt device rdm state  */
struct bt_dev_rdm_state {
	bd_address_t addr;
	uint8_t acl_connected:1;
	uint8_t hfp_connected:1;
	uint8_t a2dp_connected:1;
	uint8_t avrcp_connected:1;
	uint8_t hid_connected:1;
	uint8_t sco_connected:1;
	uint8_t a2dp_stream_started:1;
};

struct btsrv_discover_result {
	uint8_t discover_finish:1;
	bd_address_t addr;
	int8_t rssi;
	uint8_t len;
	uint8_t *name;
	uint16_t device_id[4];
};

typedef void (*btsrv_discover_result_cb)(void *result);

struct btsrv_discover_param {
	/** Maximum length of the discovery in units of 1.28 seconds. Valid range is 0x01 - 0x30. */
	uint8_t length;

	/** 0: Unlimited number of responses, Maximum number of responses. */
	uint8_t num_responses;

	/** Result callback function */
	btsrv_discover_result_cb cb;
};

struct btsrv_check_device_role_s {
	bd_address_t addr;
	uint8_t len;
	uint8_t *name;
};

/*=============================================================================
 *				global api
 *===========================================================================*/
/** Callbacks to report Bluetooth service's events*/
typedef enum {
	/** notifying that Bluetooth service has been started err, zero on success or (negative) error code otherwise*/
	BTSRV_READY,
	/** notifying link event */
	BTSRV_LINK_EVENT,
	/** notifying remote device disconnected mac with reason */
	BTSRV_DISCONNECTED_REASON,
	/** Request high cpu performace */
	BTSRV_REQ_HIGH_PERFORMANCE,
	/** Release high cpu performace */
	BTSRV_RELEASE_HIGH_PERFORMANCE,
	/** Request flush property to nvram */
	BTSRV_REQ_FLUSH_PROPERTY,
	/** CHECK device is phone or tws */
	BTSRV_CHECK_NEW_DEVICE_ROLE,
	/** notifying that Auto Reconnect complete */
	BTSRV_AUTO_RECONNECT_COMPLETE,

    BTSRV_SYNC_AOTO_RECONNECT_STATUS,
	/** CHECK streaming audio or not */
	BTSRV_CHECK_STREAMING_AUDIO_STATUS,
	BTSRV_LINK_MISS_UPLAYER_CHECK,
} btsrv_event_e;

/**
 * @brief btsrv callback
 *
 * This routine is btsrv callback, this callback context is in bt service
 *
 * @param event btsrv event
 * @param param param of btsrv event
 *
 * @return N/A
 */
typedef int (*btsrv_callback)(btsrv_event_e event, void *param);

/**
 * @brief Rgister base processer
 *
 * @return 0 excute successed , others failed
 */
int btif_base_register_processer(void);

/**
 * @brief start bt service
 *
 * This routine start bt service.
 *
 * @param cb handle of btsrv event callback
 * @param classOfDevice clase of device
 * @param did device ID
 *
 * @return 0 excute successed , others failed
 */
int btif_start(btsrv_callback cb, uint32_t classOfDevice, uint16_t *did);

/**
 * @brief stop bt service
 *
 * This routine stop bt service.
 *
 * @return 0 excute successed , others failed
 */
int btif_stop(void);

/**
 * @brief set base config info to bt service
 *
 * This routine set base config info to bt service.
 *
 * @return 0 excute successed , others failed
 */
int btif_base_set_config_info(void *param);

int btif_base_get_wake_lock(void);

int btif_base_get_link_idle(void);


/**
 * @brief set br discoverable
 *
 * This routine set br discoverable.
 *
 * @param enable true enable discoverable, false disable discoverable
 *
 * @return 0 excute successed , others failed
 */
int btif_base_set_power_off(void);

int btif_br_update_pair_status(uint8_t state);

int btif_br_get_pair_status(uint8_t *pair_status);

int btif_br_set_scan_param(struct bt_scan_parameter *param);

int btif_br_get_scan_visual(uint8_t *visual_mode);

int btif_br_get_scan_mode(uint8_t *scan_mode);

int btif_br_update_scan_mode(void);

int btif_br_set_scan_policy(struct bt_scan_policy *policy);

int btif_br_set_pair_mode_status(uint8_t enable, uint32_t duration_ms);

int btif_br_set_wait_connect_status(uint8_t enable, uint32_t duration_ms);

int btif_br_set_tws_search_status(uint8_t enable);

int btif_br_update_devie_name(void);

int btif_br_get_rssi_by_handle(uint16_t handle);

int btif_br_get_link_quality_by_handle(uint16_t handle);

int btif_br_get_linkinfo_by_handle(uint16_t handle, complete_event_cb cb);

int btif_linkinfo_set(uint8_t *map);

/**
 * @brief set br connnectable
 *
 * This routine set br connnectable.
 *
 * @param enable true enable connnectable, false disable connnectable
 *
 * @return 0 excute successed , others failed
 */

int btif_br_set_user_visual(struct bt_set_user_visual *user_visual);

/**
 * @brief allow br sco connnectable
 *
 * This routine disable br sco connect or not.
 *
 * @param enable true enable, false disable
 *
 * @return 0 excute successed , others failed
 */
int btif_br_allow_sco_connect(bool allowed);

/**
 * @brief Start br disover
 *
 * This routine Start br disover.
 *
 * @param disover parameter
 *
 * @return 0 excute successed , others failed
 */
int btif_br_start_discover(struct btsrv_discover_param *param);

/**
 * @brief Stop br disover
 *
 * This routine Stop br disover.
 *
 * @return 0 excute successed , others failed
 */
int btif_br_stop_discover(void);

/**
 * @brief connect to target bluetooth device
 *
 * This routine connect to target bluetooth device with bluetooth mac addr.
 *
 * @param bd bluetooth addr of target bluetooth device
 *                 Mac low address store in low memory address
 *
 * @return 0 excute successed , others failed
 */
int btif_br_connect(bd_address_t *bd);

/**
 * @brief disconnect to target bluetooth device
 *
 * This routine disconnect to target bluetooth device with bluetooth mac addr.
 *
 * @param bd bluetooth addr of target bluetooth device
 *                 Mac low address store in low memory address
 *
 * @return 0 excute successed , others failed
 */
int btif_br_disconnect(bd_address_t *bd);

/**
 * @brief get auto reconnect info from bt service
 *
 * This routine get auto reconnect info from bt service.
 *
 * @param info pointer store the audo reconnect info
 * @param max_cnt max number of bluetooth device
 *
 * @return 0 excute successed , others failed
 */
int btif_br_get_auto_reconnect_info(struct autoconn_info *info, uint8_t max_cnt);

/**
 * @brief check if the reconnect info is empty
 *
 * This routine check if the reconnect info is empty.
 *
 * @return 0 empty , others not empty
 */
int btif_br_is_reconnect_info_empty(void);

/**
 * @brief set auto reconnect info from bt service
 *
 * This routine set auto reconnect info from bt service.
 *
 * @param info pointer store the audo reconnect info
 * @param max_cnt max number of bluetooth device
 */
void btif_br_set_auto_reconnect_info(struct autoconn_info *info, uint8_t max_cnt);


/**
 * @brief check tws paired info from bt service
 *
 * This routine check tws paired info from bt service.
 *
 * @return true ,others null
 */
bool btif_br_check_tws_paired_info(void);

/**
 * @brief check phone paired info from bt service
 *
 * This routine check phone paired info from bt service.
 *
 * @return true ,others null
 */

bool btif_br_check_phone_paired_info(void);

/**
 * @brief auto reconnect to all need reconnect device
 *
 * This routine auto reconnect to all need reconnect device
 *
 * @param param auto reconnect info
 *
 * @return 0 excute successed , others failed
 */
int btif_br_auto_reconnect(struct bt_set_autoconn *param);

/**
 * @brief stop auto reconnect
 *
 * This routine stop auto reconnect
 *
 * @return 0 excute successed , others failed
 */

int btif_br_auto_reconnect_stop(int mode);

/**
 * @brief stop auto reconnect device
 *
 * This routine stop auto reconnect device
 *
 * @return 0 excute successed , others failed
 */
int btif_br_auto_reconnect_stop_device(bd_address_t *bd);

/**
 * @brief stop auto reconnect except device
 *
 * This routine stop auto reconnect except device
 *
 * @return 0 excute successed , others failed
 */
int btif_br_auto_reconnect_stop_except_device(bd_address_t *bd);

/**
 * @brief connection cancel paging device
 *
 * This routine connection cancel paging device
 *
 * @return 0 excute successed , others failed
 */
int btif_br_auto_reconnect_connection_cancel_device(void);

/**
 * @brief check tws reconnect is first time
 *
 * This routine check tws auto reconnect is first time
 *
 * @return ture: first; false not;
 */
bool btif_br_is_auto_reconnect_tws_first(uint8_t tws_role);

bool btif_br_is_ios_pc(uint16_t handle);

bool btif_br_is_ios_dev(uint16_t handle);

/**
 * @brief check auto reconnect is runing
 *
 * This routine check auto reconnect is runing
 *
 * @return ture: runing; false stop;
 */
bool btif_br_is_auto_reconnect_runing(void);

/**
 * @brief check auto reconnect is connecting phone first time
 *
 * This routine check auto reconnect is connecting phone first time.
 *
 * @return ture: runing; false not first;
 */
bool btif_br_is_auto_reconnect_phone_first(void);

/**
 * @brief clear br connected info list
 *
 * This routine clear br connected info list, will disconnected all connnected device first.
 *
 * @return 0 excute successed , others failed
 */
int btif_br_clear_list(int mode);

/**
 * @brief disconnect all connected device
 *
 * This routine disconnect all connected device.
 * @param:  disconnect_mode
 *
 * @return 0 excute successed , others failed
 */
int btif_br_disconnect_device(int disconnect_mode);

/**
 * @brief get br connected device number
 *
 * This routine get connected device number.
 *
 * @return connected device number(include phone and tws device)
 */
int btif_br_get_connected_device_num(void);

/**
 * @brief get br connected phone number
 *
 * This routine get connected phone number.
 *
 * @return connected phone number
 */
int btif_br_get_connected_phone_num(void);

/**
 * @brief get br device state from rdm
 *
 * This routine get device state from rdm.
 *
 * @return 0 successed, others failed.
 */
int btif_br_get_dev_rdm_state(struct bt_dev_rdm_state *state);

/**
 * @brief set phone controler role
 *
 * This routine get device state from rdm.
 */
void btif_br_set_phone_controler_role(bd_address_t *bd, uint8_t role);

/**
 * @brief Get linkkey information
 *
 * This routine Get linkkey information.
 * @param info for store get linkkey information
 * @param cnt want to get linkkey number
 *
 * return int  number of linkkey getted
 */
int btif_br_get_linkkey(struct bt_linkkey_info *info, uint8_t cnt);

/**
 * @brief Update linkkey information
 *
 * This routine Update linkkey information.
 * @param info for update linkkey information
 * @param cnt update linkkey number
 *
 * return int 0: update successful, other update failed.
 */
int btif_br_update_linkkey(struct bt_linkkey_info *info, uint8_t cnt);

/**
 * @brief Writ original linkkey information
 *
 * This routine Writ original linkkey information.
 * @param addr update device mac address
 * @param link_key original linkkey
 *
 * return 0: successfull, other: failed.
 */
int btif_br_write_ori_linkkey(bd_address_t *addr, uint8_t *link_key);

/**
 * @brief clean bt service linkkey from nvram
 *
 * This routine clean bt service linkkey from nvram.
 */
void btif_br_clean_linkkey(void);

int btif_br_clear_share_tws(void);
bool btif_br_check_share_tws(void);
void btif_br_get_local_mac(bd_address_t *addr);
uint16_t btif_br_get_active_phone_hdl(void);
int btif_br_disconnect_noactive_device(void);
int btif_bt_set_apll_temp_comp(uint8_t enable);
int btif_bt_do_apll_temp_comp(void);
void btif_ble_get_local_mac(bt_addr_le_t *addr);
void btif_br_set_autoconn_info_need_update(uint8_t need_update);
void btif_autoconn_info_active_clear_check(void);
uint8_t btif_br_get_autoconn_info_need_update(void);
void btif_br_set_gfp_scan_mode(bool valid);

/**
 * @brief get connection type
 *
 * This routine return connection type
 *
 * @param handle connection acl handle
 * @return connection type.
 */
int btif_get_type_by_handle(uint16_t handle);
int btif_get_conn_role(uint16_t handle);

bd_address_t *btif_get_addr_by_handle(uint16_t handle);
bt_addr_le_t *btif_get_le_addr_by_handle(uint16_t handle);
int btif_conn_disconnect_by_handle(uint16_t handle, uint8_t reason);

int btif_set_leaudio_connected(int connected);
int btif_set_leaudio_foreground_dev(bool enable);

int btif_disable_service(void);

int btif_br_update_qos(struct bt_update_qos_param *qos);

/**
 * @brief dump bt srv info
 *
 * This routine dump bt srv info.
 *
 * @return 0 excute successed , others failed
 */
int btif_dump_brsrv_info(void);

/*=============================================================================
 *				a2dp service api
 *===========================================================================*/

/** @brief Codec ID */
enum btsrv_a2dp_codec_id {
	/** Codec SBC */
	BTSRV_A2DP_SBC = 0x00,
	/** Codec MPEG-1 */
	BTSRV_A2DP_MPEG1 = 0x01,
	/** Codec MPEG-2 */
	BTSRV_A2DP_MPEG2 = 0x02,
	/** Codec ATRAC */
	BTSRV_A2DP_ATRAC = 0x04,
	/** Codec LDAC */
	BTSRV_A2DP_LDAC = 0xaa,
	/** Codec Non-A2DP */
	BTSRV_A2DP_VENDOR = 0xff
};

/** Callbacks to report a2dp profile events*/
typedef enum {
	/** Nothing for notify */
	BTSRV_A2DP_NONE_EV,
	/** notifying that a2dp connected*/
	BTSRV_A2DP_CONNECTED,
	/** notifying that a2dp disconnected */
	BTSRV_A2DP_DISCONNECTED,
	/** notifying that a2dp stream started */
	BTSRV_A2DP_STREAM_STARTED,
	/** notifying that a2dp stream check started */
	BTSRV_A2DP_STREAM_CHECK_STARTED,
	/** notifying that a2dp stream suspend */
	BTSRV_A2DP_STREAM_SUSPEND,
	/** notifying that a2dp stream data indicated */
	BTSRV_A2DP_DATA_INDICATED,
	/** notifying that a2dp codec info */
	BTSRV_A2DP_CODEC_INFO,
	/** Get initialize delay report  */
	BTSRV_A2DP_GET_INIT_DELAY_REPORT,

	/** Not callback event, just for snoop sync info.  */
	BTSRV_A2DP_JUST_SNOOP_SYNC_INFO,
	/** Notifying device need delay check start play.  */
	BTSRV_A2DP_REQ_DELAY_CHECK_START,
	/** notifying that a2dp stream suspend because a2dp disconnected */
	BTSRV_A2DP_DISCONNECTED_STREAM_SUSPEND,
	/** notifying that BR a2dp stream started but avrcp paused */
	BTSRV_A2DP_STREAM_STARTED_AVRCP_PAUSED,
	/** notifying that BR non_prompt_tone start*/
	BTSRV_A2DP_NON_PROMPT_TONE_STARTED,
} btsrv_a2dp_event_e;

/**
 * @brief callback of bt service a2dp profile
 *
 * This routine callback of bt service a2dp profile.
 *
 * @param event id of bt service a2dp event
 * @param packet param of btservice a2dp event
 * @param size length of param
 *
 * @return N/A
 */
typedef void (*btsrv_a2dp_callback)(uint16_t hdl, btsrv_a2dp_event_e event, void *packet, int size);

struct btsrv_a2dp_start_param {
	btsrv_a2dp_callback cb;
	uint8_t *sbc_codec;
	uint8_t *aac_codec;
	uint8_t *ldac_codec;
	uint8_t sbc_endpoint_num;
	uint8_t aac_endpoint_num;
	uint8_t ldac_endpoint_num;
	uint8_t a2dp_cp_scms_t:1;		/* Support content protection SCMS-T */
	uint8_t a2dp_delay_report:1;	/* Support delay report */
};

/**
 * @brief Rgister a2dp processer
 *
 * @return 0 excute successed , others failed
 */
int btif_a2dp_register_processer(void);

/**
 * @brief start bt service a2dp profile
 *
 * This routine start bt service a2dp profile.
 *
 * @param cbs handle of btsrv event callback
 *
 * @return 0 excute successed , others failed
 */
int btif_a2dp_start(struct btsrv_a2dp_start_param *param);

/**
 * @brief stop bt service a2dp profile
 *
 * This routine stop bt service a2dp profile.
 *
 * @return 0 excute successed , others failed
 */
int btif_a2dp_stop(void);

/**
 * @brief connect target device a2dp profile
 *
 * This routine connect target device a2dp profile.
 *
 * @param is_src mark as a2dp source or a2dp sink
 * @param bd bt address of target device
 *
 * @return 0 excute successed , others failed
 */
int btif_a2dp_connect(bool is_src, bd_address_t *bd);

/**
 * @brief disconnect target device a2dp profile
 *
 * This routine disconnect target device a2dp profile.
 *
 * @param bd bt address of target device
 *
 * @return 0 excute successed , others failed
 */
int btif_a2dp_disconnect(bd_address_t *bd);

/**
 * @brief trigger start bt service a2dp profile
 *
 * This routine use by app to check a2dp playback
 * state,if state is playback ,trigger start event
 *
 * @return 0 excute successed , others failed
 */
int btif_a2dp_check_state(void);

/**
 * @brief User set pause(for tirgger switch to second phone)
 *
 * @return 0 excute successed , others failed
 */
int btif_a2dp_user_pause(uint16_t hdl);

/**
 * @brief update avrcp state for a2dp
 *
 * @return 0 excute successed , others failed
 */

int btif_a2dp_update_avrcp_state(struct bt_conn * conn, uint8_t avrcp_state);

/**
 * @brief User disable or enable aac
 *
 * @return 0 excute successed , others failed
 */
int btif_a2dp_halt_aac(bool halt);

/**
 * @brief User disable or enable ldac
 *
 * @return 0 excute successed , others failed
 */
int btif_a2dp_halt_ldac(bool halt);

/**
 * @brief Send delay report to a2dp active phone
 *
 * This routine Send delay report to a2dp active phone
 *
 * @param delay_time delay time (unit: 1/10 milliseconds)
 *
 * @return 0 excute successed , others failed
 */
int btif_a2dp_send_delay_report(uint16_t delay_time);

/**
 * @brief Get a2dp active device handl
 *
 * @return Active device handl.
 */
uint16_t btif_a2dp_get_active_hdl(void);

/**
 * @brief check a2dp active device stream open ?
 *
 * @return 1:open, others: close.
 */
int btif_a2dp_stream_is_open(uint16_t hdl);
/**
 * @brief check a2dp active device delay stoping ?
 *
 * @return 1:delay stoping, others: close.
 */
int btif_a2dp_stream_is_delay_stop(uint16_t hdl);


/*=============================================================================
 *				avrcp service api
 *===========================================================================*/

/** avrcp controller cmd*/
typedef enum {
	BTSRV_AVRCP_CMD_PLAY,
	BTSRV_AVRCP_CMD_PAUSE,
	BTSRV_AVRCP_CMD_STOP,
	BTSRV_AVRCP_CMD_FORWARD,
	BTSRV_AVRCP_CMD_BACKWARD,
	BTSRV_AVRCP_CMD_VOLUMEUP,
	BTSRV_AVRCP_CMD_VOLUMEDOWN,
	BTSRV_AVRCP_CMD_MUTE,
	BTSRV_AVRCP_CMD_FAST_FORWARD_START,
	BTSRV_AVRCP_CMD_FAST_FORWARD_STOP,
	BTSRV_AVRCP_CMD_FAST_BACKWARD_START,
	BTSRV_AVRCP_CMD_FAST_BACKWARD_STOP,
	BTSRV_AVRCP_CMD_REPEAT_SINGLE,
	BTSRV_AVRCP_CMD_REPEAT_ALL_TRACK,
	BTSRV_AVRCP_CMD_REPEAT_OFF,
	BTSRV_AVRCP_CMD_SHUFFLE_ON,
	BTSRV_AVRCP_CMD_SHUFFLE_OFF
} btsrv_avrcp_cmd_e;

/** Callbacks to report avrcp profile events*/
typedef enum {
	/** Nothing for notify */
	BTSRV_AVRCP_NONE_EV,
	/** notifying that avrcp connected */
	BTSRV_AVRCP_CONNECTED,
	/** notifying that avrcp disconnected*/
	BTSRV_AVRCP_DISCONNECTED,
	/** notifying that avrcp stoped */
	BTSRV_AVRCP_STOPED,
	/** notifying that avrcp paused */
	BTSRV_AVRCP_PAUSED,
	/** notifying that avrcp playing */
	BTSRV_AVRCP_PLAYING,
	/** notifying cur id3 info */
	BTSRV_AVRCP_UPDATE_ID3_INFO,
	/** notifying cur track change */
	BTSRV_AVRCP_TRACK_CHANGE,
	/** notifying play status */
	BTSRV_AVRCP_GET_PLAY_STATUS,
    /** notifying passthrough released */
	BTSRV_AVRCP_PASS_THROUGH_RELEASED,
	BTSRV_AVRCP_UPDATE_SUPPORT_FEATURE,
} btsrv_avrcp_event_e;
typedef struct {
	uint16_t feature_mask;
	uint16_t covrt_art_psm;
} btsrv_avrcp_evt_update_support_feature;

typedef void (*btsrv_avrcp_callback)(uint16_t hdl, btsrv_avrcp_event_e event, void *param);
typedef void (*btsrv_avrcp_get_volume_callback)(uint16_t hdl, uint8_t *volume, bool remote_reg);
typedef void (*btsrv_avrcp_set_volume_callback)(uint16_t hdl, uint8_t volume, bool sync_to_remote_pending);
typedef void (*btsrv_avrcp_pass_ctrl_callback)(uint16_t hdl, uint8_t cmd, uint8_t state);

typedef struct {
	btsrv_avrcp_callback              event_cb;
	btsrv_avrcp_get_volume_callback   get_volume_cb;
	btsrv_avrcp_set_volume_callback   set_volume_cb;
	btsrv_avrcp_pass_ctrl_callback    pass_ctrl_cb;
} btsrv_avrcp_callback_t;

/**
 * @brief Rgister avrcp processer
 *
 * @return 0 excute successed , others failed
 */
int btif_avrcp_register_processer(void);
int btif_avrcp_start(btsrv_avrcp_callback_t *cbs);
int btif_avrcp_stop(void);
int btif_avrcp_connect(bd_address_t *bd);
int btif_avrcp_disconnect(bd_address_t *bd);
int btif_avrcp_send_command(int command);
int btif_avrcp_send_command_by_hdl(uint16_t hdl, int command);

int btif_avrcp_sync_vol(uint16_t hdl, uint16_t delay_ms);		/* Hdl as parameter, will use get_volume_cb get volume to sync */
int btif_avrcp_get_id3_info();
int btif_avrcp_set_absolute_volume(uint8_t *data, uint8_t len);
int btif_avrcp_notify_volume_change(uint8_t volume);

/*return 0:pause, 1:play*/
int btif_avrcp_get_avrcp_status(uint16_t hdl);
int btif_avrcp_get_play_status(struct bt_conn *conn);
int btif_avrcp_get_support_feature(uint16_t hdl);

/*=============================================================================
 *				hfp hf service api
 *===========================================================================*/

enum btsrv_sco_codec_id {
	/** Codec CVSD */
	BTSRV_SCO_CVSD = 0x01,
	/** Codec MSBC */
	BTSRV_SCO_MSBC = 0x02,
	/** Codec LC3_SWB */
	BTSRV_SCO_LC3_SWB = 0x03,
};

/** Callbacks to report avrcp profile events*/
typedef enum {
	/** Nothing for notify */
	BTSRV_HFP_NONE_EV            = 0,
	/** notifying that hfp connected */
	BTSRV_HFP_CONNECTED          = 1,
	/** notifying that hfp disconnected*/
	BTSRV_HFP_DISCONNECTED       = 2,
	/** notifying that hfp codec info*/
	BTSRV_HFP_CODEC_INFO         = 3,
	/** notifying that hfp phone number*/
	BTSRV_HFP_PHONE_NUM          = 4,
	/** notifying that hfp phone number stop report*/
	BTSRV_HFP_PHONE_NUM_STOP     = 5,
	/** notifying that hfp in coming call*/
	BTSRV_HFP_CALL_INCOMING      = 6,
	/** notifying that hfp call out going*/
	BTSRV_HFP_CALL_OUTGOING      = 7,
	BTSRV_HFP_CALL_3WAYIN        = 8,
	/** notifying that hfp call ongoing*/
	BTSRV_HFP_CALL_ONGOING       = 9,
	BTSRV_HFP_CALL_MULTIPARTY    = 10,
	BTSRV_HFP_CALL_ALERTED       = 11,
	BTSRV_HFP_CALL_EXIT          = 12,
	BTSRV_HFP_VOLUME_CHANGE      = 13,
	BTSRV_HFP_ACTIVE_DEV_CHANGE  = 14,
	BTSRV_HFP_SIRI_STATE_CHANGE  = 15,
	BTSRV_HFP_SCO                = 16,
	/** notifying that sco connected*/
	BTSRV_SCO_CONNECTED          = 17,
	/** notifying that sco disconnected*/
	BTSRV_SCO_DISCONNECTED       = 18,
	/** notifying that data indicatied*/
	BTSRV_SCO_DATA_INDICATED     = 19,
	/** update time from phone*/
	BTSRV_HFP_TIME_UPDATE        = 20,
	/** at cmd from hf*/
	BTSRV_HFP_AT_CMD             = 21,
	/** Get bt manger hfp state, for snoop sync used */
	BTSRV_HFP_GET_BTMGR_STATE    = 22,
	/** Get bt manger hfp state, for snoop sync used */
	BTSRV_HFP_SET_BTMGR_STATE    = 23,
	/** Not callback event, just for snoop sync info.  */
	BTSRV_HFP_JUST_SNOOP_SYNC_INFO = 24,

	/** notifying that hfp call 3way out going*/
	BTSRV_HFP_CALL_3WAYOUT        = 25,
} btsrv_hfp_event_e;

enum {
	HFP_SIRI_STATE_DEACTIVE     = 0,
	HFP_SIRI_STATE_ACTIVE       = 1,
	HFP_SIRI_STATE_SEND_ACTIVE  = 2,	/* Send SIRI start command */
};

enum btsrv_hfp_stack_event {
	/* CIEV call_setup */
	BTSRV_HFP_EVENT_CALL_SETUP_EXITED      = 0,
	BTSRV_HFP_EVENT_CALL_INCOMING          = 1,
	BTSRV_HFP_EVENT_CALL_OUTCOMING         = 2,
	BTSRV_HFP_EVENT_CALL_ALERTED           = 3,
	/* CIEV call */
	BTSRV_HFP_EVENT_CALL_EXITED            = 10,
	BTSRV_HFP_EVENT_CALL_ONGOING           = 11,
	/* CIEV call_held */
	BTSRV_HFP_EVENT_CALL_HELD_EXITED       = 20,
	BTSRV_HFP_EVENT_CALL_MULTIPARTY_HELD   = 21,
	BTSRV_HFP_EVENT_CALL_HELD              = 22,
	/* Sco event */
	BTSRV_HFP_EVENT_SCO_ESTABLISHED        = 30,
	BTSRV_HFP_EVENT_SCO_RELEASED           = 31,
};

enum btsrv_hfp_state {
	BTSRV_HFP_STATE_INIT                  = 0,
	BTSRV_HFP_STATE_LINKED                = 1,
	BTSRV_HFP_STATE_CALL_INCOMING         = 2,
	BTSRV_HFP_STATE_CALL_OUTCOMING        = 3,
	BTSRV_HFP_STATE_CALL_ALERTED          = 4,
	BTSRV_HFP_STATE_CALL_ONGOING          = 5,
	BTSRV_HFP_STATE_CALL_3WAYIN           = 6,
	BTSRV_HFP_STATE_CALL_MULTIPARTY       = 7,
	BTSRV_HFP_STATE_SCO_ESTABLISHED       = 8,
	BTSRV_HFP_STATE_SCO_RELEASED          = 9,
	BTSRV_HFP_STATE_CALL_EXITED           = 10,
	BTSRV_HFP_STATE_CALL_SETUP_EXITED     = 11,
	BTSRV_HFP_STATE_CALL_HELD             = 12,
	BTSRV_HFP_STATE_CALL_MULTIPARTY_HELD  = 13,
	BTSRV_HFP_STATE_CALL_HELD_EXITED      = 14,
};

typedef int (*btsrv_hfp_callback)(uint16_t, btsrv_hfp_event_e event, uint8_t *param, int param_size);

int btif_hfp_register_processer(void);
int btif_hfp_start(btsrv_hfp_callback cb);
int btif_hfp_stop(void);
int btif_hfp_hf_connect(bd_address_t *bd);
int btif_hfp_hf_dial_number(uint8_t *number);
int btif_hfp_hf_dial_last_number(void);
int btif_hfp_hf_dial_memory(int location);
int btif_hfp_hf_volume_control(uint8_t type, uint8_t volume);
int btif_hfp_hf_battery_report(uint8_t mode, uint8_t bat_val);
int btif_hfp_hf_accept_call(void);
int btif_hfp_hf_reject_call(void);
int btif_hfp_hf_hangup_call(void);
int btif_hfp_hf_hangup_another_call(void);
int btif_hfp_hf_holdcur_answer_call(void);
int btif_hfp_hf_hangupcur_answer_call(void);
int btif_hfp_hf_voice_recognition_start(void);
int btif_hfp_hf_voice_recognition_stop(void);
int btif_hfp_hf_send_at_command(uint8_t *command,uint8_t active_call);
int btif_hfp_hf_switch_sound_source(void);
int btif_hfp_hf_get_call_state(uint8_t active_call);
int btif_hfp_hf_get_time(void);
uint16_t btif_hfp_get_active_hdl(void);
void btif_hfp_get_active_conn_handle(uint16_t *handle);

io_stream_t sco_upload_stream_create(uint8_t hfp_codec_id);

typedef void (*btsrv_sco_callback)(uint16_t hdl, btsrv_hfp_event_e event,  uint8_t *param, int param_size);
int btif_sco_start(btsrv_sco_callback cb);
int btif_sco_stop(void);
int btif_disconnect_sco(uint16_t acl_hdl);

/*=============================================================================
 *				hfp ag service api
 *===========================================================================*/
enum btsrv_hfp_hf_ag_indicators {
	BTSRV_HFP_AG_SERVICE_IND,
	BTSRV_HFP_AG_CALL_IND,
	BTSRV_HFP_AG_CALL_SETUP_IND,
	BTSRV_HFP_AG_CALL_HELD_IND,
	BTSRV_HFP_AG_SINGNAL_IND,
	BTSRV_HFP_AG_ROAM_IND,
	BTSRV_HFP_AG_BATTERY_IND,
	BTSRV_HFP_AG_MAX_IND,
};

typedef void (*btsrv_hfp_ag_callback)(btsrv_hfp_event_e event,  uint8_t *param, int param_size);

int btif_hfp_ag_register_processer(void);
int btif_hfp_ag_start(btsrv_hfp_ag_callback cb);
int btif_hfp_ag_stop(void);
int btif_hfp_ag_update_indicator(enum btsrv_hfp_hf_ag_indicators index, uint8_t value);
int btif_hfp_ag_send_event(uint8_t *event, uint16_t len);

/*=============================================================================
 *				spp service api
 *===========================================================================*/
#define SPP_UUID_LEN		16

/** Callbacks to report spp profile events*/
typedef enum {
	/** notifying that spp register failed */
	BTSRV_SPP_REGISTER_FAILED,
	/** notifying that spp connect failed */
	BTSRV_SPP_REGISTER_SUCCESS,
	/** notifying that spp connect failed */
	BTSRV_SPP_CONNECT_FAILED,
	/** notifying that spp connected*/
	BTSRV_SPP_CONNECTED,
	/** notifying that spp disconnected */
	BTSRV_SPP_DISCONNECTED,
	/** notifying that spp stream data indicated */
	BTSRV_SPP_DATA_INDICATED,
	/** notifying that snoop spp connected*/
	BTSRV_SPP_SNOOP_CONNECTED,
	/** notifying that snoop spp disconnected */
	BTSRV_SPP_SNOOP_DISCONNECTED,
} btsrv_spp_event_e;

typedef void (*btsrv_spp_callback)(btsrv_spp_event_e event, uint8_t app_id, void *packet, int size);

struct bt_spp_reg_param {
	uint8_t app_id;
	uint8_t *uuid;
};

struct bt_spp_connect_param {
	uint8_t app_id;
	bd_address_t bd;
	uint8_t *uuid;
};

int btif_spp_register_processer(void);
int btif_spp_reg(struct bt_spp_reg_param *param);
int btif_spp_send_data(uint8_t app_id, uint8_t *data, uint32_t len);
int btif_spp_connect(struct bt_spp_connect_param *param);
int btif_spp_disconnect(uint8_t app_id);
int btif_spp_start(btsrv_spp_callback cb);
int btif_spp_stop(void);

/*=============================================================================
 *				PBAP service api
 *===========================================================================*/
typedef struct parse_vcard_result pbap_vcard_result_t;

typedef enum {
	/** notifying that pbap connect failed*/
	BTSRV_PBAP_CONNECT_FAILED,
	/** notifying that pbap connected*/
	BTSRV_PBAP_CONNECTED,
	/** notifying that pbap disconnected */
	BTSRV_PBAP_DISCONNECTED,
	/** notifying that pbap vcard result */
	BTSRV_PBAP_VCARD_RESULT,
} btsrv_pbap_event_e;

typedef void (*btsrv_pbap_callback)(btsrv_pbap_event_e event, uint8_t app_id, void *data, uint8_t size);

struct bt_get_pb_param {
	bd_address_t bd;
	uint8_t app_id;
	char *pb_path;
	btsrv_pbap_callback cb;
};

int btif_pbap_register_processer(void);
int btif_pbap_get_phonebook(struct bt_get_pb_param *param);
int btif_pbap_abort_get(uint8_t app_id);

/*=============================================================================
 *				MAP service api
 *===========================================================================*/
typedef enum {
	/** notifying that pbap connect failed*/
	BTSRV_MAP_CONNECT_FAILED,
	/** notifying that pbap connected*/
	BTSRV_MAP_CONNECTED,
	/** notifying that pbap disconnected */
	BTSRV_MAP_DISCONNECTED,
	/** notifying that pbap disconnected */
	BTSRV_MAP_SET_PATH_FINISHED,

	BTSRV_MAP_MESSAGES_RESULT,
} btsrv_map_event_e;

typedef void (*btsrv_map_callback)(btsrv_map_event_e event, uint8_t app_id, void *data, uint8_t size);

struct bt_map_connect_param {
	bd_address_t bd;
	uint8_t app_id;
	char *map_path;
	btsrv_map_callback cb;
};

struct bt_map_get_param {
	bd_address_t bd;
	uint8_t app_id;
	char *map_path;
	btsrv_map_callback cb;
};

struct bt_map_set_folder_param {
	uint8_t app_id;
	uint8_t flags;
	char *map_path;
};

struct bt_map_get_messages_listing_param {
    uint8_t app_id;
    uint8_t reserved;
    uint16_t max_list_count;
    uint32_t parameter_mask;
};

int btif_map_client_connect(struct bt_map_connect_param *param);
int btif_map_get_message(struct bt_map_get_param *param);
int btif_map_get_messages_listing(struct bt_map_get_messages_listing_param *param);
int btif_map_get_folder_listing(uint8_t app_id);
int btif_map_abort_get(uint8_t app_id);
int btif_map_client_disconnect(uint8_t app_id);
int btif_map_client_set_folder(struct bt_map_set_folder_param *param);
int btif_map_register_processer(void);


/*=============================================================================
 *				BIP service api
 *===========================================================================*/
typedef enum {
	BTSRV_BIPC_EVT_CONNECT_CFM			= 0,
	BTSRV_BIPC_EVT_DISCONNECT_IND,
	BTSRV_BIPC_EVT_GET_PROPERTIES_DATA_CFM,
	BTSRV_BIPC_EVT_IMAGE_DATA_IND,
	BTSRV_BIPC_EVT_ABORT_CFM,
} btsrv_bipc_evt_t;

typedef enum {
	BTSRV_BIPC_SUPPORTED_CAP_GENERIC_IMG = 0x01,
	BTSRV_BIPC_SUPPORTED_CAP_CAPTURING	 = 0x02,
	BTSRV_BIPC_SUPPORTED_CAP_PRINTING	 = 0x04,
	BTSRV_BIPC_SUPPORTED_CAP_DISPLAYING  = 0x08,
} btsrv_bipc_supported_capabilities_t;

typedef enum {
	BTSRV_BIPC_SUPPORTED_FEATURE_IMG_PUSH              = 0x001,
	BTSRV_BIPC_SUPPORTED_FEATURE_IMG_PUSH_STORE        = 0x002,
	BTSRV_BIPC_SUPPORTED_FEATURE_IMG_PUSH_PRINT        = 0x004,
	BTSRV_BIPC_SUPPORTED_FEATURE_IMG_PUSH_DISPLAY      = 0x008,
	BTSRV_BIPC_SUPPORTED_FEATURE_IMG_PULL              = 0x010,
	BTSRV_BIPC_SUPPORTED_FEATURE_ADVANCEED_IMAGE_PRINT = 0x020,
	BTSRV_BIPC_SUPPORTED_FEATURE_AUTO_ARCHIVE          = 0x040,
	BTSRV_BIPC_SUPPORTED_FEATURE_REMOTE_CAMERA         = 0x080,
	BTSRV_BIPC_SUPPORTED_FEATURE_REMOTE_DISPLAY        = 0x100,
} btsrv_bipc_supported_features_t;

typedef int (*btsrv_bipc_callback)(btsrv_bipc_evt_t event, uint8_t app_id, void *data);

typedef struct {
	bd_address_t bd;
	uint8_t app_id;
	uint16_t psm;
} btsrv_bipc_connect_param_t;

typedef struct {
	uint16_t width;
	uint16_t height;
} btsrv_bipc_pixel_t;

typedef enum {
	BTSRV_BIPC_TRANS_STRETCH = 0x01,
	BTSRV_BIPC_TRANS_CROP	 = 0x02,
	BTSRV_BIPC_TRANS_FILL	 = 0x04,
} btsrv_bipc_trans_t;

typedef struct {
	char* encoding;
	btsrv_bipc_pixel_t pixel;
	uint32_t size;
	uint32_t max_size;
	btsrv_bipc_trans_t trans;
} btsrv_bipc_image_desc_t;

typedef struct {
	char* encoding;
	btsrv_bipc_pixel_t min;  /* if the height is 0 in min and the height is not 0 in max, then the height For each
                                possible intermediate value of W(idth), the corresponding height is calculated using the formula H=(W*H2)/W2 */
	btsrv_bipc_pixel_t max;
	uint32_t size;
} btsrv_bipc_propertie_native_t;

typedef struct {
	char* encoding;
	btsrv_bipc_pixel_t min; /* if the height is 0 in min and the height is not 0 in max, then the height For each
                                possible intermediate value of W(idth), the corresponding height is calculated using the formula H=(W*H2)/W2 */
	btsrv_bipc_pixel_t max;
	uint32_t max_size;
	btsrv_bipc_trans_t trans;
} btsrv_bipc_propertie_variant_t;

typedef struct {
	int status;
	char* name;
	char* img_handle;
	btsrv_bipc_propertie_native_t native;

	uint8_t variant_num;
	btsrv_bipc_propertie_variant_t* variant;
} btsrv_bipc_evt_get_properties_cfm_t;

typedef struct {
	int status;
	bool end;
	uint32_t len;
	uint8_t* data;
} btsrv_bipc_evt_image_ind_t;

typedef struct {
	int status;
	btsrv_bipc_supported_capabilities_t cap_mask;
	btsrv_bipc_supported_features_t features_mask;
} btsrv_bipc_evt_connect_cfm_t;

typedef struct {
	int status;
} btsrv_bipc_evt_disconnect_ind_t;

typedef struct {
	int status;
} btsrv_bipc_evt_abort_cfm_t;


typedef struct {
	bool notify_supported_capability;
	bool notify_supported_feature;
} btsrv_bipc_config_t;

int btif_bipc_start(btsrv_bipc_callback cb, btsrv_bipc_config_t* cfg);

int btif_bipc_stop(void);

int btif_bipc_connect(btsrv_bipc_connect_param_t* param);

int btif_bipc_disconnect(uint8_t app_id);

int btif_bipc_get_image_properties(uint8_t app_id, const char* img_handle);

int btif_bipc_get_image(uint8_t app_id, const char* img_handle, btsrv_bipc_image_desc_t* img_desc);

int btif_bipc_abort(uint8_t app_id);

/*=============================================================================
 *				hid service api
 *===========================================================================*/

struct bt_device_id_info{
	uint16_t vendor_id;
	uint16_t product_id;
	uint16_t id_source;
};

/* Different report types in get, set, data
*/
enum {
	BTSRV_HID_REP_TYPE_OTHER=0,
	BTSRV_HID_REP_TYPE_INPUT,
	BTSRV_HID_REP_TYPE_OUTPUT,
	BTSRV_HID_REP_TYPE_FEATURE,
};

/* Parameters for Handshake
*/
enum {
	BTSRV_HID_HANDSHAKE_RSP_SUCCESS=0,
	BTSRV_HID_HANDSHAKE_RSP_NOT_READY,
	BTSRV_HID_HANDSHAKE_RSP_ERR_INVALID_REP_ID,
	BTSRV_HID_HANDSHAKE_RSP_ERR_UNSUPPORTED_REQ,
	BTSRV_HID_HANDSHAKE_RSP_ERR_INVALID_PARAM,
	BTSRV_HID_HANDSHAKE_RSP_ERR_UNKNOWN=14,
	BTSRV_HID_HANDSHAKE_RSP_ERR_FATAL,
};

struct bt_hid_report{
	uint8_t report_type;
	uint8_t has_size;
	uint8_t *data;
	int len;
};

/** Callbacks to report hid profile events*/
typedef enum {
	/** notifying that hid connected*/
	BTSRV_HID_CONNECTED,
	/** notifying that hid disconnected */
	BTSRV_HID_DISCONNECTED,
	/** notifying hid get reprot data */
	BTSRV_HID_GET_REPORT,
	/** notifying hid set reprot data */
	BTSRV_HID_SET_REPORT,
	/** notifying hid get protocol data */
	BTSRV_HID_GET_PROTOCOL,
	/** notifying hid set protocol data */
	BTSRV_HID_SET_PROTOCOL,
	/** notifying hid intr data */
	BTSRV_HID_INTR_DATA,
	/** notifying hid unplug */
	BTSRV_HID_UNPLUG,
	/** notifying hid suspend */
	BTSRV_HID_SUSPEND,
	/** notifying hid exit suspend */
	BTSRV_HID_EXIT_SUSPEND,
} btsrv_hid_event_e;

typedef void (*btsrv_hid_callback)(btsrv_hid_event_e event, void *packet, int size);

int btif_hid_register_processer(void);
int btif_did_register_processer(void);
int btif_did_register_sdp(uint8_t *data, uint32_t len);
int btif_hid_register_sdp(struct bt_sdp_attribute * hid_attrs,uint8_t attrs_size);
int btif_hid_send_ctrl_data(uint8_t report_type,uint8_t *data, uint32_t len);
int btif_hid_send_intr_data(uint8_t report_type,uint8_t *data, uint32_t len);
int btif_hid_send_rsp(uint8_t status);
int btif_hid_start(btsrv_hid_callback cb);
int btif_hid_stop(void);

int btif_bt_set_apll_temp_comp(u8_t enable);
int btif_bt_do_apll_temp_comp(void);
int btif_bt_read_bt_build_version(uint8_t* str_version, int len);
int btif_bt_link_quality_always(bool enable);

/** Connection Type */
enum {
	/** LE Connection Type */
	BT_TYPE_LE = BIT(0),
	/** BR/EDR Connection Type */
	BT_TYPE_BR = BIT(1),
	/** SCO Connection Type */
	BT_TYPE_SCO = BIT(2),
	/** ISO Connection Type */
	BT_TYPE_ISO = BIT(3),
	/** BR/EDR SNOOP Connection Type */
	BT_TYPE_BR_SNOOP = BIT(4),
	/** SCO SNOOP Connection Type */
	BT_TYPE_SCO_SNOOP = BIT(5),
	/** All Connection Type */
	BT_TYPE_ALL = BT_CONN_TYPE_LE | BT_CONN_TYPE_BR |
			   BT_CONN_TYPE_SCO | BT_CONN_TYPE_ISO | BT_TYPE_BR_SNOOP | BT_TYPE_SCO_SNOOP,
};

struct bt_gaming_audio_role_config {
	uint8_t role_ugg : 1; /* Gaming Role Unicast Game Gateway */
	uint8_t role_ugt : 1; /* Gaming Role Unicast Game Terminal */
	uint8_t role_bgs : 1; /* Gaming Role Broadcast Game Sender */
	uint8_t role_bgr : 1; /* Gaming Role Broadcast Game Receiver */
};

struct bt_gaming_audio_feat_config {
	uint16_t ugg_feat_multiplex : 1; /* Support transmitting multiple LC3 codec frames per block in an SDU */
	uint16_t ugg_feat_96kbps_source : 1; /* 96 kbps source support */
	uint16_t ugg_feat_multisink : 1; /* Support for receiving at least two channels of audio, each in a separate CIS */
	uint16_t ugt_feat_source : 1; /* Source support */
	uint16_t ugt_feat_80kbps_source : 1; /* 80 kbps source support */
	uint16_t ugt_feat_sink : 1; /* Sink support */
	uint16_t ugt_feat_64kbps_sink : 1; /* 64 kbps sink support */
	uint16_t ugt_feat_multiplex : 1; /* Support for receiving multiple LC3 codec frames per block in an SDU */
	uint16_t ugt_feat_multisink : 1; /* Support for receiving at least two audio channels, each in a separate CIS */
	uint16_t ugt_feat_multisource : 1; /* Support for sending at least two audio channels, each in a separate CIS */
	uint16_t bgs_feat_96kbps : 1; /* 96 kbps support */
	uint16_t bgr_feat_multisink : 1; /* Support for receiving at least two audio channels, each in a separate BIS */
	uint16_t bgr_feat_multiplex : 1; /* Support for receiving multiple LC3 codec frames per block in an SDU */
};

struct bt_gaming_audio_config {
	struct bt_gaming_audio_role_config gmas_role; /* local role */
	struct bt_gaming_audio_feat_config feat; /* local gmas roles feat */
};

struct bt_audio_config {
	uint32_t num_master_conn : 3;	/* as a master, range: [0: 7] */
	uint32_t num_slave_conn : 3;	/* as a slave, range: [0: 7] */

	/* if BR coexists */
	uint32_t br : 1;

	/* if support encryption as slave */
	uint32_t encryption : 1;
	/*
	 * Authenticated Secure Connections and 128-bit key which depends
	 * on encryption.
	 */
	uint32_t secure : 1;

	uint32_t target_announcement : 1;

	uint32_t lea_connection_enable : 1;

	/* whether support pacs_client */
	uint32_t pacs_cli_num : 1;
	/* whether support unicast_client */
	uint32_t uni_cli_num : 1;
	/* whether support volume_controller */
	uint32_t vol_cli_num : 1;
	/* whether support microphone_controller */
	uint32_t mic_cli_num : 1;
	/* whether support call_control_server */
	uint32_t call_srv_num : 1;
	/* whether support media_control_server */
	uint32_t media_srv_num : 1;
	/* whether support cas_set_coordinator */
	uint32_t cas_cli_num : 1;
	/* whether support broadcast_assistant */
	uint32_t bass_cli_num : 1;

	/* whether support pacs_server */
	uint32_t pacs_srv_num : 1;
	/* whether support unicast_server */
	uint32_t uni_srv_num : 1;
	/* whether support volume_controller */
	uint32_t vol_srv_num : 1;
	/* whether support microphone_device */
	uint32_t mic_srv_num : 1;
	/* whether support call_control_client */
	uint32_t call_cli_num : 1;
	/* whether support media_control_client */
	uint32_t media_cli_num : 1;
	/* whether support cas_set_member */
	uint32_t cas_srv_num : 1;
	/* whether support scan_delegator */
	uint32_t bass_srv_num : 1;

	/* whether support tmas_client */
	uint32_t tmas_cli_num : 1;

	/* whether support gmas_client */
	uint32_t gmas_cli_num : 1;

	/* support how many connections for unicast_client */
	uint32_t uni_cli_dev_num : 3;	/* [0 : 7] */
	/* support how many connections for unicast_server */
	uint32_t uni_srv_dev_num : 3;	/* [0 : 7] */

	/* support how many CIS channels, >= num_conn */
	uint32_t cis_chan_num : 5;	/* [0 : 31] */
	/* support how many ASEs for ASCS Server */
	uint32_t ascs_srv_ase_num : 5;	/* [0 : 31] */
	/* support how many ASEs for ASCS Client */
	uint32_t ascs_cli_ase_num : 5;	/* [0 : 31] */

	/* support how many connections for call_control_client */
	uint32_t call_cli_dev_num : 3;	/* [0 : 7] */
	/* support how many connections for pacs_client */
	uint32_t pacs_cli_dev_num : 3;	/* [0 : 7] */
	/* support how many connections for tmas_client */
	uint32_t tmas_cli_dev_num : 3;	/* [0 : 7] */
	/* support how many connections for cas_client */
	uint32_t cas_cli_dev_num : 3;	/* [0 : 7] */
	/* support how many connections for gmas_client */
	uint32_t gmas_cli_dev_num : 3;	/* [0 : 7] */

	/* support how many PAC Characteristic for server */
	uint32_t pacs_srv_pac_num : 3;	/* [0 : 7] */
	/* support how many PAC Characteristics for client */
	uint32_t pacs_cli_pac_num : 5;	/* [0 : 31] */
	/* support how many PAC records for client */
	uint32_t pacs_pac_num : 5;	/* [0 : 31] */

	/* support how many connections for volume_control_client */
	uint32_t vcs_cli_dev_num : 3;  /* [0 : 7] */
	/* support how many connections for microphone_control_client */
	uint32_t mic_cli_dev_num : 3;  /* [0 : 7] */
	/* support how many connections for media_control_client */
	uint32_t mcs_cli_dev_num : 3;  /* [0 : 7] */

	/* support how many connections for aics_server */
	uint32_t aics_srv_num : 3;  /* [0 : 7] */
	/* support how many connections for aics_client */
	uint32_t aics_cli_num : 5;  /* [0 : 31] */

	/* support how many connections for broadcast_assistant */
	uint32_t bass_cli_dev_num : 3;  /* [0 : 7] */

	// TODO: other number configurations, refers for btsrv_audio_mem.h
#if 1	// TODO: broadcast related!!!
#if 0
		BTSRV_BASS_CLI_PA,
#endif
	/* whether support broadcast_source */
	uint32_t broad_src_num : 1;
	/* whether support broadcast_sink */
	uint32_t broad_sink_num : 1;

	/* support how many instances for broadcast_source */
	uint32_t broad_src_dev_num : 3;	/* [0 : 7] */
	/* support how many instances for broadcast_sink */
	uint32_t broad_sink_dev_num : 3;	/* [0 : 7] */
	/* support how many BASE subgroups, >= num_broad */
	uint32_t broad_subgroup_num : 5;	/* [0 : 31] */	// TODO: enough???
	/* support how many BASE bis, >= num_broad */
	uint32_t broad_bis_num : 5;	/* [0 : 31] */	// TODO: enough???
	/* support how many BIS channels, >= num_broad */
	uint32_t bis_chan_num : 5;	/* [0 : 31] */
	/* support how many broad_info */
	uint32_t broad_info_num : 5;	/* [0 : 31] */
#endif

	/* if support remote keys */
	uint32_t remote_keys : 1;

	/* matching remote(s) with LE Audio test address(es) */
	uint32_t test_addr : 1;

	/* if enable master latency function */
	uint32_t master_latency : 1;

	/* if set_sirk is enabled  */
	uint32_t sirk_enabled : 1;
	/* if set_sirk is encrypted (otherwise plain) */
	uint32_t sirk_encrypted : 1;

	/*
	 * Work as a unicast relay node, it supports slave and master
	 * simutaneously. It will work as a unicast server to accept
	 * streams and work as a unicast client to relay the streams
	 * to other unicast servers.
	 */
	uint32_t relay : 1;

	/* if support encryption as master */
	uint32_t master_encryption : 1;

	/* master will not connect specific slaves by name */
	uint32_t disable_name_match : 1;

	/* current id, in order to support different functions */
	uint8_t keys_id;

	/* only works for cas_srv_num (set_member) */
	uint8_t set_size;
	uint8_t set_rank;
	uint8_t set_sirk[16];	/* depends on sirk_enabled */

	/* initial value, step, flags for volume renderer */
	uint8_t initial_volume;
	uint8_t volume_step;
	uint8_t volume_persisted : 1;
	uint8_t volume_muted : 1;

	/* Gaming audio configuration*/
	uint8_t gmas_server : 1;
	struct bt_gaming_audio_config gmas_config;
	uint16_t sink_available_contexts;
	uint16_t source_available_contexts;
};

/*
 * Bt Audio Codecs
 */
#define BT_AUDIO_CODEC_UNKNOWN   0x0
/* LE Audio */
#define BT_AUDIO_CODEC_LC3       0x1
/* BR Call */
#define BT_AUDIO_CODEC_CVSD      0x2
#define BT_AUDIO_CODEC_MSBC      0x3
/* BR Music */
#define BT_AUDIO_CODEC_AAC       0x4
#define BT_AUDIO_CODEC_SBC       0x5
/* BR Call */
#define BT_AUDIO_CODEC_LC3_SWB   0x06

#define BT_AUDIO_CODEC_LDAC      0xaa

/* Messages to report LE Audio events */
typedef enum {
	BTSRV_ACL_CONNECTED = 1,
	BTSRV_ACL_DISCONNECTED,
	BTSRV_CIS_CONNECTED,
	BTSRV_CIS_DISCONNECTED,
	BTSRV_BIS_CONNECTED,
	BTSRV_BIS_DISCONNECTED,
	BTSRV_AUDIO_STOPPED,
	BTSRV_AUDIO_TWS_CONNECTED,

	/* ASCS */
	BTSRV_ASCS_CONFIG_CODEC = 10,
	BTSRV_ASCS_PREFER_QOS,
	BTSRV_ASCS_CONFIG_QOS,
	BTSRV_ASCS_ENABLING, /*ase enable;cis not connect*/
	BTSRV_ASCS_ENABLE,
	BTSRV_ASCS_DISABLE,
	BTSRV_ASCS_UPDATE,
	BTSRV_ASCS_RECV_START,
	BTSRV_ASCS_RECV_STOP,
	BTSRV_ASCS_RELEASE,
	BTSRV_ASCS_ASES,	/* ASE topology */
	BTSRV_ASCS_CONNECT,
	BTSRV_ASCS_DISCONNECT,

	/* PACS */
	BTSRV_PACS_CAPS,	/* PACS capabilities */

	/* CIS */
	BTSRV_CIS_PARAMS,

	/* TBS */
	BTSRV_TBS_CONNECTED = 30,
	BTSRV_TBS_DISCONNECTED,
	BTSRV_TBS_INCOMING,
	BTSRV_TBS_DIALING,	/* outgoing */
	BTSRV_TBS_ALERTING,	/* outgoing */
	BTSRV_TBS_ACTIVE,
	BTSRV_TBS_LOCALLY_HELD,
	BTSRV_TBS_REMOTELY_HELD,
	BTSRV_TBS_HELD,
	BTSRV_TBS_ENDED,

	/* VCS */
	/* as a slave, report message initiated by a master */
	BTSRV_VCS_UP = 50,
	BTSRV_VCS_DOWN,
	BTSRV_VCS_VALUE,
	BTSRV_VCS_MUTE,
	BTSRV_VCS_UNMUTE,
	BTSRV_VCS_UNMUTE_UP,
	BTSRV_VCS_UNMUTE_DOWN,
	/* as a master, report message initiated by a slave */
	BTSRV_VCS_CLIENT_CONNECTED,
	BTSRV_VCS_CLIENT_UP,
	BTSRV_VCS_CLIENT_DOWN,
	BTSRV_VCS_CLIENT_VALUE,
	BTSRV_VCS_CLIENT_MUTE,
	BTSRV_VCS_CLIENT_UNMUTE,
	BTSRV_VCS_CLIENT_UNMUTE_UP,
	BTSRV_VCS_CLIENT_UNMUTE_DOWN,

	/* as a slave, report message initiated by a master */
	BTSRV_MICS_MUTE = 80,
	BTSRV_MICS_UNMUTE,
	BTSRV_MICS_GAIN_VALUE,
	/* as a master, report message initiated by a slave */
	BTSRV_MICS_CLIENT_CONNECTED,
	BTSRV_MICS_CLIENT_MUTE,
	BTSRV_MICS_CLIENT_UNMUTE,
	BTSRV_MICS_CLIENT_GAIN_VALUE,

	/* as a server, report message initiated by a client */
	BTSRV_MCS_SERVER_PLAY = 100,
	BTSRV_MCS_SERVER_PAUSE,
	BTSRV_MCS_SERVER_STOP,
	BTSRV_MCS_SERVER_FAST_REWIND,
	BTSRV_MCS_SERVER_FAST_FORWARD,
	BTSRV_MCS_SERVER_NEXT_TRACK,
	BTSRV_MCS_SERVER_PREV_TRACK,
	/* as a client, report message initiated by a server */
	BTSRV_MCS_CONNECTED,
	BTSRV_MCS_PLAY,
	BTSRV_MCS_PAUSE,
	BTSRV_MCS_STOP,

	/*
	 * Broadcast Audio
	 */
	/* broadcast source */
	BTSRV_BROAD_SRC_CONFIG = 140,
	BTSRV_BROAD_SRC_ENABLE,
	BTSRV_BROAD_SRC_UPDATE,
	BTSRV_BROAD_SRC_DISABLE,
	BTSRV_BROAD_SRC_RELEASE,
	BTSSRV_BROAD_SRC_SENDDATA_EMPTY,

	/* broadcast sink */
	BTSRV_BROAD_SINK_CONFIG = 160,
	BTSRV_BROAD_SINK_ENABLE,
	BTSRV_BROAD_SINK_UPDATE,
	BTSRV_BROAD_SINK_DISABLE,
	BTSRV_BROAD_SINK_RELEASE,
	// TODO: error handle message
	BTSRV_BROAD_SINK_SYNC_LOST,
	BTSRV_BROAD_SINK_BASE_CONFIG,
	BTSRV_BROAD_SINK_SYNC,

	/* broadcast assistant */
	BTSRV_BROAD_ASST_XXX = 180,

	/* scan delegator */
	BTSRV_SCAN_DELE_XXX = 200,
} btsrv_audio_event_e;

typedef void (*btsrv_audio_callback)(btsrv_audio_event_e event, void *data, int size);

/*
 * Bt Audio Direction
 *
 * 1. for client, BT_AUDIO_DIR_SINK means Audio Source role.
 * 2. for client, BT_AUDIO_DIR_SOURCE means Audio Sink role.
 * 3. for server, BT_AUDIO_DIR_SINK means Audio Sink role.
 * 4. for server, BT_AUDIO_DIR_SOURCE means Audio Source role.
 */
#define BT_AUDIO_DIR_SINK	0x0
#define BT_AUDIO_DIR_SOURCE	0x1

/* Framing Supported */
#define BT_AUDIO_UNFRAMED_SUPPORTED	0x00
#define BT_AUDIO_UNFRAMED_NOT_SUPPORTED	0x01

static inline bool bt_audio_unframed_supported(uint8_t framing)
{
	if (framing == BT_AUDIO_UNFRAMED_SUPPORTED) {
		return true;
	}

	return false;
}

/* Framing */
#define BT_AUDIO_UNFRAMED	0x00
#define BT_AUDIO_FRAMED	0x01


/* PHY & Preferred_PHY */
#define BT_AUDIO_PHY_1M	BIT(0)
#define BT_AUDIO_PHY_2M	BIT(1)
#define BT_AUDIO_PHY_CODED	BIT(2)

/* Target_Latency */
#define BT_AUDIO_TARGET_LATENCY_LOW	0x01
#define BT_AUDIO_TARGET_LATENCY_BALANCED	0x02
#define BT_AUDIO_TARGET_LATENCY_HIGH	0x03

/* Target_PHY */
#define BT_AUDIO_TARGET_PHY_1M	0x01
#define BT_AUDIO_TARGET_PHY_2M	0x02
#define BT_AUDIO_TARGET_PHY_CODED	0x03

/* Packing */
#define BT_AUDIO_PACKING_SEQUENTIAL	0x00
#define BT_AUDIO_PACKING_INTERLEAVED	0x01

/* Supported Audio Capability of Server */
struct bt_audio_capability {
	uint8_t format;	/* codec format */
	/* units below: refers to codec.h */
	uint8_t durations;	/* frame_durations */
	uint8_t channels;	/* channel_counts */
	uint8_t frames;	/* max_codec_frames_per_sdu */
	uint16_t samples;
	uint16_t octets_min;	/* octets_per_codec_frame_min */
	uint16_t octets_max;	/* octets_per_codec_frame_max */
	uint16_t preferred_contexts;
};

/* Supported Audio Capabilities of Server */
struct bt_audio_capabilities {
	uint16_t handle;	/* for client to distinguish different server */
	uint8_t source_cap_num;
	uint8_t sink_cap_num;

	uint16_t source_available_contexts;
	uint16_t source_supported_contexts;
	uint32_t source_locations;

	uint16_t sink_available_contexts;
	uint16_t sink_supported_contexts;
	uint32_t sink_locations;

	/* source cap first if any, followed by sink cap */
	struct bt_audio_capability cap[0];
};

/* endpoint for br */
#define BT_AUDIO_ENDPOINT_MUSIC    	0
#define BT_AUDIO_ENDPOINT_CALL  	1

/* Stream/Endpoint mapping of Server */
struct bt_audio_endpoint {
	uint8_t id;	/* endpoint id; 0: invalid, range: [1, 255] */
	uint8_t dir : 1;	/* BT_AUDIO_DIR_SINK/SOURCE */
	/*
	 * NOTICE: BT_AUDIO_STREAM_IDLE and BT_AUDIO_STREAM_CODEC_CONFIGURED
	 * are useful only, others are useless!
	 */
	uint8_t state : 3;	/* Audio Stream state */
};

struct bt_audio_endpoints {
	uint16_t handle;
	uint8_t num_of_endp;
	struct bt_audio_endpoint endp[0];
};

/*
 * 1. used to report codec info configured by the client to the server.
 * 2. used to report codec info configured by the server to the client.
 * 3. used to set codec info by the client.
 */
struct bt_audio_codec {
	uint16_t handle;
	uint8_t id;
	uint8_t dir;
	uint8_t format;	/* codec format */
	uint8_t target_latency;
	uint8_t target_phy;
	uint8_t frame_duration;
	uint8_t blocks;	/* codec_frame_blocks_per_sdu */
	uint16_t sample_rate;	/* unit: kHz */
	uint16_t octets;	/* octets_per_codec_frame */
	uint32_t locations;
};

/* Used to report preferred qos info of server */
struct bt_audio_preferred_qos {
	uint16_t handle;
	uint8_t id; /* endpoint id */
	uint8_t framing;
	uint8_t phy;
	uint8_t rtn;
	uint16_t latency;	/* max_transport_latency */
	uint32_t delay_min;	/* presentation_delay_min */
	uint32_t delay_max;	/* presentation_delay_max */
	uint32_t preferred_delay_min;	/* presentation_delay_min */
	uint32_t preferred_delay_max;	/* presentation_delay_max */
};

/*
 * Qos of Server
 *
 * variables with suffix "_m" means the direction is M_To_S,
 * variables with suffix "_s" means the direction is S_To_M.
 */
struct bt_audio_qos {
	uint16_t handle;

	/*
	 * case 1: id_m_to_s != 0 && id_s_to_m != 0, bidirectional
	 * case 2: id_m_to_s != 0 && id_s_to_m == 0, unidirectional
	 * case 3: id_m_to_s == 0 && id_s_to_m != 0, unidirectional
	 */
	uint8_t id_m_to_s;	/* sink endpoint id */
	uint8_t id_s_to_m;	/* source endpoint id */

	uint16_t max_sdu_m;
	uint16_t max_sdu_s;

	uint8_t phy_m;
	uint8_t phy_s;

	uint8_t rtn_m;
	uint8_t rtn_s;

	/* processing_time for this connection only, unit: us */
	uint32_t processing_m;
	uint32_t processing_s;

	/* presentation_delay for this connection only, unit: us */
	uint32_t delay_m;
	uint32_t delay_s;
};

/* Used to config qos actively by the client */
struct bt_audio_qoss {
	uint8_t num_of_qos;
	uint8_t sca;
	uint8_t packing;
	uint8_t framing;

	/* sdu_interval, unit: us */
	uint32_t interval_m;
	uint32_t interval_s;

	/* max_transport_latency, unit: ms */
	uint16_t latency_m;
	uint16_t latency_s;

	/*
	 * presentation_delay for all connections, unit: us
	 */
	/* mutual exclusive with qos->delay_m */
	uint32_t delay_m;
	/* mutual exclusive with qos->delay_s */
	uint32_t delay_s;

	/*
	 * processing_time for all connections, unit: us
	 */
	/* mutual exclusive with qos->processing_m */
	uint32_t processing_m;
	/* mutual exclusive with qos->processing_s */
	uint32_t processing_s;

	struct bt_audio_qos qos[0];	/* 1 for each bt_audio connection */
};

/* Used to report qos info configured by the client to the server */
struct bt_audio_qos_configured {
	uint16_t handle;
	uint8_t id;	/* endpoint id */
	uint8_t framing;
	/* sdu_interval, unit: us */
	uint32_t interval;
	/* presentation_delay, unit: us */
	uint32_t delay;
	/* max_transport_latency, unit: ms */
	uint16_t latency;
	uint16_t max_sdu;
	uint8_t phy;
	uint8_t rtn;
};

/* used to report the current audio stream state and info */
struct bt_audio_report {
	uint16_t handle;
	uint8_t id;	/* endpoint id */
	uint8_t reason;
	/* NOTICE: only for enable and update */
	uint16_t audio_contexts;
};

struct bt_audio_cis_param {
	uint16_t handle;
	uint8_t id;	/* endpoint id */
	uint32_t audio_handle;
	uint32_t iso_interval; /*us*/
	int32_t sync_delay; /*us*/
};

struct bt_audio_chan {
	uint16_t handle;	/* indicate the connection */
	uint8_t id;	/* endpoint id */
};

typedef void (*bt_audio_trigger_cb)(uint16_t handle, uint8_t id,
				uint16_t audio_handle);

typedef int (*bt_audio_start_cb)(void *param, int wait_us);

struct bt_audio_stream_cb {
	bt_audio_trigger_cb tx_trigger;
	bt_audio_trigger_cb tx_period_trigger;
	bt_audio_trigger_cb rx_trigger;
	bt_audio_trigger_cb rx_period_trigger;
	bt_audio_trigger_cb tws_sync_trigger;
};

struct bt_audio_params {
	uint16_t handle;	/* indicate the connection */
	uint8_t id;	/* endpoint id */
	uint8_t nse;
	uint8_t m_bn;
	uint8_t s_bn;
	uint8_t m_ft;
	uint8_t s_ft;
	uint32_t cig_sync_delay;
	uint32_t cis_sync_delay;
	uint32_t rx_delay;
};

/*
 * Termination Reason
 */
#define BT_CALL_REASON_URI               0x00
#define BT_CALL_REASON_CALL_FAILED       0x01
#define BT_CALL_REASON_REMOTE_END        0x02
#define BT_CALL_REASON_SERVER_END        0x03
#define BT_CALL_REASON_LINE_BUSY         0x04
#define BT_CALL_REASON_NETWORK           0x05
#define BT_CALL_REASON_CLIENT_END        0x06
#define BT_CALL_REASON_NO_SERVICE        0x07
#define BT_CALL_REASON_NO_ANSWER         0x08
#define BT_CALL_REASON_UNSPECIFIED       0x09

#define BT_AUDIO_CALL_URI_LEN	24

struct bt_call_report_simple {
	uint16_t handle;
	uint8_t index;	/* call index */
};

/* used to report the current call state and info */
struct bt_call_report {
	uint16_t handle;
	uint8_t index;	/* call index */

	uint8_t reason;	/* termination reason */

	/*
	 * remote's uri, NULL if unnecessary, end with '\0' if valid
	 */
	uint8_t *uri;
};

struct bt_audio_call {
	/*
	 * For call gateway, "handle" will not be used to distinguish diffrent
	 * call (should use "index" instead), because for multiple terminals
	 * cases, all terminals share the same call list.
	 *
	 * For call terminal, "handle" will be used to distinguish diffrent
	 * call gateway, "index" will be used to distinguish diffrent call of
	 * the same call gateway.
	 */
	uint16_t handle;
	uint8_t index;	/* call index */
};

struct bt_attribute_info {
	uint32_t id;
	uint16_t character_id;
	uint16_t len;
	uint8_t *data;
} __packed;

/* Must match TOTAL_ATTRIBUTE_ITEM_NUM in acts_bluetooth/avrcp.h (stack fills all slots). */
#define BT_TOTAL_ATTRIBUTE_ITEM_NUM   6

struct bt_media_id3_info {
	uint16_t handle;
	uint8_t num_of_item;

	struct bt_attribute_info item[0];
};

struct bt_media_play_status {
	uint16_t handle;
	uint8_t status;
};

struct bt_audio_media {
	uint16_t handle;

	uint8_t *name;
};

struct bt_media_report {
	uint16_t handle;
};

struct bt_volume_report {
	uint16_t handle;
	/* only for volume set */
	uint8_t value;
	uint8_t from_phone;
};

struct bt_audio_volume {
	uint16_t handle;
	uint8_t type;

	uint8_t value;
	uint8_t mute;
	uint32_t location;	/* 0: whole device */
	// TODO:
};

struct bt_mic_report {
	uint16_t handle;
	/* only for gain value */
	uint8_t gain;
};

struct bt_audio_microphone {
	uint16_t handle;
	uint8_t type;

	uint8_t mute;
	// TODO:
};

/* if the related handle (BT Connection) belongs to the same group */
struct bt_audio_group {
	uint8_t type;
	uint8_t num_of_handle;
	uint16_t handle[0];
};

struct bt_disconn_report {
	uint16_t handle;
	uint8_t reason;
};

typedef	bool (*bt_audio_scan_recv_cb)(const struct bt_le_scan_recv_info *info,
		     struct net_buf_simple *buf);

typedef int (*bt_audio_vnd_rx_cb)(uint16_t handle, const uint8_t *buf,
				uint16_t len);

typedef void (*bt_audio_adv_cb)(void);

typedef void (* bt_audio_event_proc_cb)(void * param);

// TODO: support ext_adv/per_adv/big_control vendor cb

typedef int (*bt_broadcast_vnd_rx_cb)(uint32_t handle, const uint8_t *buf,
				uint16_t len);

/* BT Audio Broadcast channel */
struct bt_broadcast_chan {
	uint32_t handle;	/* broadcast group handle (broadcast_id) */
	uint8_t id;	/* broadcast stream index (bis_index) */
};

typedef void (*bt_broadcast_trigger_cb)(uint32_t handle, uint8_t id,
				uint16_t audio_handle);

struct bt_broadcast_stream_cb {
	bt_broadcast_trigger_cb tx_trigger;
	bt_broadcast_trigger_cb tx_period_trigger;
	bt_broadcast_trigger_cb rx_trigger;
	bt_broadcast_trigger_cb rx_period_trigger;
	bt_broadcast_trigger_cb tws_sync_trigger;
};

/* BIS level codec */
struct bt_broadcast_bis {
#if 0
	uint8_t subgroup_index;	/* Not used by Broadcast Source */
#endif
	uint8_t bis_index;	/* Not used by Broadcast Source */
	uint8_t reserved;

	uint8_t frame_duration;	/* Frame_Duration */
	uint8_t blocks;	/* Codec_Frame_Blocks_Per_SDU */
	uint16_t sample_rate;	/* unit: kHz */
	uint16_t octets;	/* Octets_Per_Codec_Frame */
	uint32_t locations;	/* Audio Locations */
};

/* Subgroup level codec */
struct bt_broadcast_subgroup {
#if 0
	/*
	 * NOTICE: For Broadcast Source, if broadcast_id == 0, bt_service
	 * will generate a random broadcast_id; else it means broadcast_id
	 * is set by user layer and all subgroups shall be same.
	 *
	 * For Broadcast Sink, it is used to report BASE.
	 */
	uint32_t broadcast_id;
#endif
	uint8_t num_bis;	/* BIS level */
	uint8_t format;	/* BT Audio Codecs */

	/*
	 * Not used in report for simplity
	 */
	uint8_t frame_duration;	/* Frame_Duration */
	uint8_t blocks;	/* Codec_Frame_Blocks_Per_SDU */
	uint16_t sample_rate;	/* unit: kHz */
	uint16_t octets;	/* Octets_Per_Codec_Frame */
	uint32_t locations;	/* Audio Locations */

	uint32_t language;	/* Language types */
	uint16_t contexts;	/* Audio Context types */
	uint16_t reserved;

	struct bt_broadcast_bis bis[0];
};

struct bt_broadcast_base {
	uint32_t handle;	/* broadcast_id */
	uint32_t delay;	/* Presentation_Delay, unit: us */

	uint8_t num_subgroups;
	uint8_t reserved0;
	uint8_t reserved1;
	uint8_t reserved2;

	struct bt_broadcast_subgroup subgroup[0];
};

/* Used to config qos actively by the broadcast_source */
struct bt_broadcast_qos {
	uint8_t packing;
	uint8_t framing;
	uint8_t phy;
	uint8_t rtn;

	uint16_t max_sdu;

	/* max_transport_latency, unit: ms */
	uint16_t latency;

	/* sdu_interval, unit: us */
	uint32_t interval;

	/*
	 * presentation_delay, unit: us
	 */
	uint32_t delay;

	/*
	 * processing_time, unit: us
	 */
	uint32_t processing;
};

/* merge codec and qos configured, need to split? */
struct bt_braodcast_configured {
	uint32_t handle;
	uint8_t id;

	uint8_t framing;
	uint8_t phy;
	uint8_t rtn;	// TODO: irc
	// TODO: add pto, bn ???
	/* sdu_interval, unit: us */
	uint32_t interval;
	/* presentation_delay, unit: us */
	uint32_t delay;
	/* max_transport_latency, unit: ms */
	uint16_t latency;	// TODO: need?
	uint16_t max_sdu;

	uint8_t format;	/* codec format */
	uint8_t frame_duration;
	uint8_t blocks;	/* codec_frame_blocks_per_sdu */
	uint8_t reserved;
	uint16_t sample_rate;	/* unit: kHz */
	uint16_t octets;	/* octets_per_codec_frame */
	uint32_t locations;
};

struct bt_broadcast_base_cfg_info {
	uint32_t broadcast_id;
    uint8_t num_bis;	/* total number of bis in all subgroups */
	/* available bis_indexes */
	uint32_t bis_indexes;	// TODO:
};

/* used to report the current audio stream state and info */
struct bt_broadcast_report {
	uint32_t handle;
	uint8_t id;	/* endpoint id */
	uint8_t reason; /*bis disconn reason*/
	/* NOTICE: only for enable and update */
	uint16_t audio_contexts;
};

struct bt_broadcast_bis_param {
	uint32_t handle;
	uint8_t id;	/* endpoint id */
	uint32_t audio_handle;
	uint16_t iso_interval;
	int32_t sync_delay;
};

struct bt_broadcast_source_big_param {
	uint16_t iso_interval;
	uint16_t max_pdu;
	uint8_t nse;
	uint8_t bn;
	uint8_t irc;
	uint8_t pto;
};

/*
 * any value above 0xFFFFFF is invalid, so we can just use 0xFFFFFFFF to denote
 * invalid broadcast ID
 */
#define INVALID_BROADCAST_ID 0xFFFFFFFF

/** Broadcast Source create parameters */
struct bt_broadcast_source_create_param {
	/*
	 * broadcast_id: any value above 0xFFFFFF is invalid, which means
	 * bt_service will generate one!
	 */
	int32_t broadcast_id;

	char *local_name;

	char *broadcast_name;

	char *program_info;

	struct bt_broadcast_qos *qos;

	/* Whether or not to encrypt the streams */
	bool encryption;

	/**
	 * @brief Broadcast code
	 *
	 * If the value is a string or a the value is less than 16 octets,
	 * the remaining octets shall be 0.
	 *
	 * Example:
	 *   The string "Broadcast Code" shall be
	 *   [42 72 6F 61 64 63 61 73 74 20 43 6F 64 65 00 00]
	 */
	uint8_t broadcast_code[16];

	uint16_t appearance;

	uint8_t num_subgroups;
	struct bt_broadcast_subgroup *subgroup;

	uint8_t past_enable;
    uint16_t acl_handle;
	uint16_t conn_interval;
	/*
	 * Optional user layer specific extended advertising parameters
	 * for Broadcast Source.
	 *
	 * If not set, default parameter will be used.
	 */
	struct bt_le_adv_param *adv_param;

	/*
	 * Optional user layer specific periodic advertising parameters
	 * for Broadcast Source.
	 *
	 * If not set, default parameter will be used.
	 */
	struct bt_le_per_adv_param *per_adv_param;

	/*
	 * Optional user layer specific extended advertising start parameters
	 * for Broadcast Source.
	 *
	 * If not set, default parameter will be used.
	 */
	struct bt_le_ext_adv_start_param *adv_start_param;

	/*
	 * Optional user layer specific BIG parameters for Broadcast Source.
	 * called before bt_manager_broadcast_source_enable().
	 *
	 * If not set, default parameter will be used.
	 */
	struct bt_broadcast_source_big_param *big_param;
};

int bt_manager_broadcast_source_per_adv_param_init(struct bt_le_per_adv_param *param);

int bt_manager_broadcast_source_start_param_init(struct bt_le_ext_adv_start_param *param);

/*
 * LE Audio
 */
#define BT_EXT_ADV_SID_UNICAST 1
#define BT_EXT_ADV_SID_BROADCAST 2
#define BT_EXT_ADV_SID_PAWR 3
#define BT_EXT_ADV_SID_LETWS 4
#define BT_EXT_ADV_SID_BROADCAST_SQ 5

#define BT_LE_AUDIO_ADV_MAX_ENTRY    16
#define BT_LE_AUDIO_ADV_MAX_LEN      128

int btif_audio_register_processer(void);
int btif_audio_init(struct bt_audio_config config, btsrv_audio_callback cb,
				void *data, int size);
int btif_audio_exit(void);
int btif_audio_keys_clear(void);
int btif_audio_adv_param_init(struct bt_le_adv_param *param);
int btif_audio_scan_param_init(struct bt_le_scan_param *param);
int btif_audio_conn_create_param_init(struct bt_conn_le_create_param *param);
int btif_audio_conn_param_init(struct bt_le_conn_param *param);
int btif_audio_conn_idle_param_init(struct bt_le_conn_param *param);
int btif_audio_start(void);
int btif_audio_stop(void);
int btif_audio_services_enable(void);
int btif_audio_services_disable(void);
int btif_audio_pause(void);
int btif_audio_resume(void);
int btif_audio_pause_scan(void);
int btif_audio_resume_scan(void);
int btif_audio_get_scan_status(void);
int btif_audio_scan_recv_cb_register(bt_audio_scan_recv_cb cb);
int btif_audio_pause_adv(void);
int btif_audio_resume_adv(void);
int btif_audio_update_adv_param(struct bt_le_adv_param *param);
int btif_audio_init_adv_data(struct bt_data *data, uint8_t count);
int btif_audio_relay_control(bool enable);
bool btif_audio_stopped(void);
int btif_audio_server_cap_init(struct bt_audio_capabilities *caps);
int btif_audio_stream_config_codec(struct bt_audio_codec *codec);
int btif_audio_stream_prefer_qos(struct bt_audio_preferred_qos *qos);
int btif_audio_stream_config_qos(struct bt_audio_qoss *qoss);
int btif_audio_stream_bind_qos(struct bt_audio_qoss *qoss, uint8_t index);
int btif_audio_stream_sync_forward(struct bt_audio_chan **chans,
				uint8_t num_chans);
int btif_audio_stream_enable(struct bt_audio_chan **chans,
				uint8_t num_chans, uint32_t contexts);
int btif_audio_stream_disble(struct bt_audio_chan **chans, uint8_t num_chans);
io_stream_t btif_audio_source_stream_create(struct bt_audio_chan **chans,
				uint8_t num_chans, uint32_t locations);
int btif_audio_source_stream_set(struct bt_audio_chan **chans,
				uint8_t num_chans, io_stream_t stream,
				uint32_t locations);
int btif_audio_sink_stream_set(struct bt_audio_chan *chan, io_stream_t stream);
int btif_audio_sink_stream_start(struct bt_audio_chan **chans,
				uint8_t num_chans);
int btif_audio_sink_stream_stop(struct bt_audio_chan **chans,
				uint8_t num_chans);
int btif_audio_stream_update(struct bt_audio_chan **chans,
				uint8_t num_chans, uint32_t contexts);
int btif_audio_stream_release(struct bt_audio_chan **chans,
				uint8_t num_chans);
int btif_audio_stream_released(struct bt_audio_chan *chan);
int btif_audio_stream_disconnect(struct bt_audio_chan **chans,
				uint8_t num_chans);
void *btif_audio_stream_bandwidth_alloc(struct bt_audio_qoss *qoss);
int btif_audio_stream_bandwidth_free(void *cig_handle);
int btif_audio_stream_cb_register(struct bt_audio_stream_cb *cb);
int btif_audio_stream_set_tws_sync_cb_offset(struct bt_audio_chan *chan,int32_t offset_from_syncdelay);
int btif_audio_volume_up(uint16_t handle);
int btif_audio_volume_down(uint16_t handle);
int btif_audio_volume_set(uint16_t handle, uint8_t value);
int btif_audio_volume_mute(uint16_t handle);
int btif_audio_volume_unmute(uint16_t handle);
int btif_audio_volume_unmute_up(uint16_t handle);
int btif_audio_volume_unmute_down(uint16_t handle);
int btif_audio_mic_mute(uint16_t handle);
int btif_audio_mic_unmute(uint16_t handle);
int btif_mics_mute_get(uint16_t handle);
int btif_audio_mic_mute_disable(uint16_t handle);
int btif_audio_mic_gain_set(uint16_t handle, uint8_t gain);
int btif_mcs_media_state_get(uint16_t handle);
int btif_audio_media_play(uint16_t handle);
int btif_audio_media_pause(uint16_t handle);
int btif_audio_media_fast_rewind(uint16_t handle);
int btif_audio_media_fast_forward(uint16_t handle);
int btif_audio_media_stop(uint16_t handle);
int btif_audio_media_play_prev(uint16_t handle);
int btif_audio_media_play_next(uint16_t handle);
int btif_audio_conn_max_len(uint16_t handle);
int btif_audio_vnd_register_rx_cb(uint16_t handle, bt_audio_vnd_rx_cb rx_cb);
int btif_audio_vnd_send(uint16_t handle, uint8_t *buf, uint16_t len);
int btif_audio_vnd_connected(uint16_t handle);
int btif_audio_adv_cb_register(bt_audio_adv_cb start, bt_audio_adv_cb stop);
int btif_audio_start_adv(void);
int btif_audio_stop_adv(void);
int btif_audio_call_originate(uint16_t handle, uint8_t *remote_uri);
int btif_audio_call_accept(struct bt_audio_call *call);
int btif_audio_call_hold(struct bt_audio_call *call);
int btif_audio_call_retrieve(struct bt_audio_call *call);
int btif_audio_call_terminate(struct bt_audio_call *call);

typedef void (*get_remote_lea_name_cb)(uint16_t handle, uint8_t err, const void *data, uint16_t length);

int btif_audio_get_remote_name(uint16_t handle, get_remote_lea_name_cb callback);

typedef enum {
	TBS_CHRC_PROVIDER_NAME = 0,
	TBS_CHRC_UCI,
	TBS_CHRC_TECHNOLOGY,
	TBS_CHRC_URI_SCHEMES,
	TBS_CHRC_SIGNAL_STRENGTH,
	TBS_CHRC_STRENGTH_REPORTING_INTERVAL,
	TBS_CHRC_CURRENT_CALLS,
	TBS_CHRC_CCID,
	TBS_CHRC_STATUS_FLAGS,
	TBS_CHRC_TARGET_URI,
	TBS_CHRC_CALL_STATE,
	TBS_CHRC_CCP,
	TBS_CHRC_CCP_OPTIONAL_OPCODES,
	TBS_CHRC_TERMINATION_REASON,
	TBS_CHRC_INCOMING_CALL,
	TBS_CHRC_FRIENDLY_NAME,
	TBS_CHRC_MAX_NUM,
} btsrv_tbs_chrc_type_e;
int btif_audio_call_read_tbs_chrc(struct bt_conn *conn, uint8_t chrc_type);
int btif_audio_call_set_strength_report_interval(struct bt_conn *conn, uint8_t interval, bool with_rsp);
int btif_audio_call_control_operation(struct bt_audio_call *call, uint8_t opcode);
int btif_audio_call_remote_originate(uint8_t *uri);
int btif_audio_call_remote_alerting(struct bt_audio_call *call);
int btif_audio_call_remote_accept(struct bt_audio_call *call);
int btif_audio_call_remote_hold(struct bt_audio_call *call);
int btif_audio_call_remote_retrieve(struct bt_audio_call *call);
int btif_audio_call_srv_terminate(struct bt_audio_call *call, uint8_t reason);
int btif_audio_call_srv_originate(uint8_t *uri);
int btif_audio_call_set_target_uri(struct bt_audio_call *call, uint8_t *uri);
int btif_audio_call_set_friendly_name(struct bt_audio_call *call, uint8_t *uri);
int btif_audio_call_set_provider(char *provider);
int btif_audio_call_set_technology(uint8_t technology);
int btif_audio_call_set_status_flags(uint8_t status);
int btif_audio_call_state_notify(void);
int btif_audio_call_srv_accept(struct bt_audio_call *call);
int btif_audio_call_srv_hold(struct bt_audio_call *call);
int btif_audio_call_srv_retrieve(struct bt_audio_call *call);

uint16_t btif_audio_get_local_tmas_role(void);
int btif_audio_cb_event_notify(bt_audio_event_proc_cb cb, void *event_param, uint32_t len);
int btif_cas_set_rank(uint8_t rank);
int btif_cas_set_size(uint8_t size);
int btif_cas_set_lock(struct bt_conn *conn, uint8_t val);
int btif_cas_lock_notify(struct bt_conn *conn);
int btif_pacs_set_sink_loc(uint32_t locations);
int btif_audio_set_sirk(uint8_t type, uint8_t *sirk_value);
int btif_audio_update_rsi(uint8_t **rsi_out);
struct bt_audio_config * btif_audio_get_cfg_param(void);
uint8_t btif_audio_get_loc_gmas_role(void);
int btif_audio_get_last_leaudio_device(bt_addr_le_t* dev);

int btif_audio_set_ble_tws_addr(bt_addr_le_t *addr);

/*
 * LE Audio -- Broadcast start
 */

int btif_broadcast_scan_param_init(struct bt_le_scan_param *param);
int btif_broadcast_scan_start(void);
int btif_broadcast_scan_stop(void);
int btif_broadcast_source_create(struct bt_broadcast_source_create_param *param);
int btif_broadcast_source_reconfig(uint32_t handle,
				struct bt_broadcast_qos *qos,
				uint8_t num_subgroups,
				struct bt_broadcast_subgroup *subgroup);
int btif_broadcast_source_update(struct bt_broadcast_chan *chan,
				uint16_t contexts, uint32_t language);
int btif_broadcast_source_enable(uint32_t handle);
int btif_broadcast_source_disable(uint32_t handle);
int btif_broadcast_source_release(uint32_t handle);
int btif_broadcast_source_stream_set(struct bt_broadcast_chan *chan,
				io_stream_t stream);
int btif_broadcast_source_ext_adv_send(uint32_t handle, uint8_t *buf,
				uint16_t len, uint8_t type);
int btif_broadcast_source_per_adv_send(uint32_t handle, uint8_t *buf,
				uint16_t len, uint8_t type);

int btif_broadcast_sink_sync(uint32_t handle, uint32_t bis_indexes,
				const uint8_t broadcast_code[16]);
int btif_broadcast_sink_release(uint32_t handle);
int btif_broadcast_sink_stream_set(struct bt_broadcast_chan *chan,
				io_stream_t stream);
int btif_broadcast_sink_register_ext_rx_cb(bt_broadcast_vnd_rx_cb cb);
int btif_broadcast_sink_register_per_rx_cb(uint32_t handle,
				bt_broadcast_vnd_rx_cb cb);
int btif_broadcast_register_scan_filter_cb(bt_broadcast_scan_filter_cb cb);

int btif_broadcast_source_register_per_tx_cb(uint32_t handle,
				bt_broadcast_vnd_rx_cb cb);

int btif_broadcast_sink_sync_loacations(uint32_t loacations);
int btif_broadcast_filter_local_name(uint8_t *name);
int btif_broadcast_filter_broadcast_name(uint8_t *name);
int btif_broadcast_filter_broadcast_id(uint32_t id);
int btif_broadcast_filter_company_id(uint32_t id);
int btif_broadcast_filter_uuid16(uint16_t value);
int btif_broadcast_filter_rssi(int8_t rssi);/*rssi: -127 to +20 */
int btif_broadcast_clear_filter(void);

int btif_broadcast_stream_cb_register(struct bt_broadcast_stream_cb *cb);
int btif_broadcast_stream_set_media_delay(struct bt_broadcast_chan *chan,
				int32_t delay_us);
int btif_broadcast_stream_set_tws_sync_cb_offset(struct bt_broadcast_chan *chan,
				int32_t offset_from_syncdelay);

int btif_broadcast_source_set_retransmit(struct bt_broadcast_chan *chan,uint8_t number);

int btif_broadcast_past_subscribe(uint16_t handle);
int btif_broadcast_past_unsubscribe(uint16_t handle);

int btif_broadcast_pa_set_info_transfer(uint32_t handle);

/*
 * LE Audio -- Broadcast end
 */

/*
 * LE Audio end
 */

/*
 * pts test
 */
typedef enum
{
	BTSRV_AUDIO_PACS_AUTO_DISCOVER     = 0x01,
	BTSRV_AUDIO_ASCS_AUTO_DISCOVER     = 0x02,
	BTSRV_AUDIO_ASCS_AUTO_CODEC_CONFIG = 0x04,
	BTSRV_AUDIO_ASCS_AUTO_QOS_CONFIG   = 0x08,
	BTSRV_AUDIO_ASCS_AUTO_ENABLE_ASE   = 0x10,
	BTSRV_AUDIO_CAS_AUTO_DISCOVER      = 0x20,
	BTSRV_AUDIO_GENERATE_INCOMING_CALL = 0x40,
} audio_core_evt_flags_e;

typedef enum
{
	PTS_TEST_FLAG_CSIP = 0x01,
} pts_test_flags_e;
/*
 * For pts test lib_service
 */
typedef enum
{
	BTSRV_AUDIO_SET_SM,
	BTSRV_AUDIO_EXCHG_MTU,
	BTSRV_AUDIO_DISCOVER_GATT,
	BTSRV_AUDIO_DISCOVER_PACS,
	BTSRV_AUDIO_READ_PACS,
	BTSRV_AUDIO_WIRTE_PACS_SINK_LOC,
	BTSRV_AUDIO_WIRTE_PACS_SOURCE_LOC,
	BTSRV_AUDIO_DISCOVER_ASCS,
	BTSRV_AUDIO_READ_ASCS_CHRC,
	BTSRV_AUDIO_AUTO_CODEC_CONFIG,
	BTSRV_AUDIO_AUTO_QOS_CONFIG,
	BTSRV_AUDIO_AUTO_ENABLE_ASE,
	BTSRV_AUDIO_AUTO_START_READY,
	BTSRV_AUDIO_DISCOVER_TMAS,
	BTSRV_AUDIO_READ_TMAS_CHRC,
	BTSRV_AUDIO_DISCOVER_VCS,
	BTSRV_AUDIO_READ_VCS_CHRC,
	BTSRV_AUDIO_READ_VCS_STATE,
	BTSRV_AUDIO_DISCOVER_CAS_CSIS,
	BTSRV_AUDIO_READ_CSIS_CHRC,
	BTSRV_AUDIO_READ_CSIS_LOCK,
	BTSRV_AUDIO_CSIS_WRITE_LOCK,
	BTSRV_AUDIO_CSIS_WRITE_UNLOCK,
	BTSRV_AUDIO_DISCOVER_MICS,
	BTSRV_AUDIO_READ_MICS,
	BTSRV_AUDIO_DISCOVER_MCS,
	BTSRV_AUDIO_READ_MCS,
	BTSRV_AUDIO_DISCOVER_TBS,
	BTSRV_AUDIO_READ_TBS,
	BTSRV_AUDIO_DISCOVER_GTBS,
	BTSRV_AUDIO_DISCOVER_GMAS,
	BTSRV_AUDIO_READ_GMAS_CHRC,
} audio_core_evt_type_e;

#ifdef CONFIG_BT_PTS_TEST

int btif_pts_send_hfp_cmd(char *cmd);
int btif_pts_hfp_active_connect_sco(void);
int btif_pts_avrcp_get_play_status(void);
int btif_pts_avrcp_reg_notify_volume_change(void);
int btif_pts_register_auth_cb(bool reg_auth);
int btif_bt_set_pts_config(bool enable);
#endif /*CONFIG_BT_PTS_TEST*/

#ifdef CONFIG_BT_LEA_PTS_TEST
typedef enum
{
	PTS_ADV_TYPE_NONE,
	PTS_ADV_TYPE_CONNECTABLE,
	TMAP_DISCOVERY,
	BASS_DISCOVERY,
	BAP_DISCOVERY,
	CAP_DISCOVERY,
	CSIP_DISCOVERY,
	GAP_NON_DISCOVERABLE,
	GAP_NON_CONNECTABLE,
	GAP_LE_BR_ADV,
} pts_adv_type_e;

typedef void (*btsrv_pts_callback)(uint16_t event, uint8_t *param, int param_size);
int btif_pts_register_processer(void);
int btif_pts_start(btsrv_pts_callback cb);
int btif_pts_stop(void);
void btif_pts_reset(void);
uint8_t btif_pts_get_adv_state(void);
void btif_pts_set_adv_ann_type(void);
int btif_pts_start_adv(uint8_t adv_type);
void btif_pts_test_set_flag(uint8_t          flags);
void btif_pts_test_clear_flag(uint8_t flags);
uint8_t btif_pts_test_is_flag(pts_test_flags_e flag);
void btif_pts_set_pacs_aac(struct bt_conn *conn, uint16_t src_aac, uint16_t sink_aac);
void btif_pts_set_pacs_sac(struct bt_conn *conn, uint16_t src_aac, uint16_t sink_aac);
uint16_t btif_pts_get_pacs_aac(struct bt_conn *conn, uint8_t is_sink);

int btif_pts_dev_connect
(
	const bt_addr_le_t *addr,
	struct bt_conn *conn,
	void *p_create_param,
	void *p_conn_param
);

int btif_pts_audio_core_event(struct bt_conn *conn, uint16_t evt_type, void *param);
void btif_pts_set_auto_audio_evt_flag(audio_core_evt_flags_e flag);
void btif_pts_clear_auto_audio_evt_flag(audio_core_evt_flags_e flag);
uint8_t btif_pts_is_auto_evt(audio_core_evt_flags_e flag);
void btif_pts_set_bass_recv_state(struct bt_conn *conn);
void btif_pts_reset_bass_recv_state(void);
void btif_pts_vcs_set_state(void *state, uint8_t vol_step);
int btif_pts_vcs_notify_state(struct bt_conn *conn);
void btif_pts_set_ascs_ccid_count(uint8_t ccid_cnt);
void btif_pts_aics_set_mute(uint8_t mute_state);
void btif_pts_aics_set_gain_mode(uint8_t gain_mode);
int btif_pts_aics_state_notify(struct bt_conn *conn);
void btif_pts_csis_set_sirk(uint8_t type, uint8_t *sirk_value);
int btif_pts_read_mcs_track_duration(uint16_t handle);
int btif_pts_read_mcs_track_position(uint16_t handle);
int btif_pts_write_mcs_track_position(uint16_t handle, int32_t position);
int btif_pts_write_media_control(uint16_t handle, uint8_t opcode);
int btif_pts_set_le_security_level(bt_security_t sec_level);
#endif /* CONFIG_BT_LEA_PTS_TEST*/


/** Callbacks to report gfp profile events*/
typedef enum {
	/** Nothing for notify */
	BTSRV_GFP_NONE_EV,
	/** notifying that gfp ble connected*/
	BTSRV_GFP_BLE_CONNECTED,
	/** notifying that gfp ble disconnected */
	BTSRV_GFP_BLE_DISCONNECTED,
    /** notifying that gfp initial pairing*/
    BTSRV_GFP_INITIAL_PAIRING,
} btsrv_gfp_event_e;

typedef void (*btsrv_gfp_callback)(uint16_t hdl, btsrv_gfp_event_e event, void *packet, int size);
struct btsrv_gfp_start_param {
	btsrv_gfp_callback cb;
	uint8_t *module_id;
	uint8_t *private_anti_spoofing_key;
};

int btif_gfp_register_processer(void);
int btif_gfp_start(struct btsrv_gfp_start_param *param);

void gfp_routine_stop(void);
void gfp_routine_start(void);

/**
 * @} end defgroup bt_service_apis
 */
#endif
