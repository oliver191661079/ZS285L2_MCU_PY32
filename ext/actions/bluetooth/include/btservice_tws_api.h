/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file bt service tws version and feature
 * Not need direct include this head file, just need include <btservice_api.h>.
 * this head file include by btservice_api.h
 */

#ifndef _BTSERVICE_TWS_API_H_
#define _BTSERVICE_TWS_API_H_

/*======================  version and feature =====================================*/
/* Tws feature */
#define TWS_FEATURE_SUBWOOFER					(0x01 << 0)
#define TWS_FEATURE_A2DP_AAC					(0x01 << 1)
#define TWS_FEATURE_LOW_LATENCY					(0x01 << 2)
#define TWS_FEATURE_UI_STARTSTOP_CMD			(0x01 << 3)
#define TWS_FEATURE_HFP_TWS			            (0x01 << 4)
#define TWS_FEATURE_A2DP_LDAC					(0x01 << 5)


/* 281B version and feature */
#define US281B_START_VERSION					0x01
#define US281B_END_VERSION						0x0F
#define IS_281B_VERSION(x)						(((x) >= US281B_START_VERSION) && ((x) <= US281B_END_VERSION))

#define TWS_281B_VERSION_FEATURE_NO_SUBWOOFER	(0)
#define TWS_281B_VERSION_FEATURE_SUBWOOFER		(TWS_FEATURE_SUBWOOFER)

/* Woodperker version and feature
 * version: 0x10~0x1F, not support subwoofer
 * version: 0x20~0x2F, support subwoofer
 */
#define WOODPECKER_START_VERSION				0x10
#define WOODPECKER_END_VERSION					0x2F
#define IS_WOODPECKER_VERSION(x)				(((x) >= WOODPECKER_START_VERSION) && ((x) <= WOODPECKER_END_VERSION))

#define TWS_WOODPERKER_VERSION_0x10_FEATURE		(0)

#ifdef CONFIG_BT_A2DP_AAC
#define TWS_SUPPORT_FEATURE_AAC					TWS_FEATURE_A2DP_AAC
#else
#define TWS_SUPPORT_FEATURE_AAC					0
#endif
#ifdef CONFIG_BT_A2DP_LDAC
#define TWS_SUPPORT_FEATURE_LDAC				TWS_FEATURE_A2DP_LDAC
#else
#define TWS_SUPPORT_FEATURE_LDAC				0
#endif
#ifdef CONFIG_OS_LOW_LATENCY_MODE
#define TWS_SUPPORT_FEATURE_LOW_LATENCY			TWS_FEATURE_LOW_LATENCY
#else
#define TWS_SUPPORT_FEATURE_LOW_LATENCY			0
#endif
#ifdef CONFIG_BT_HFP_TWS
#define TWS_SUPPORT_FEATURE_HFP_TWS				TWS_FEATURE_HFP_TWS
#else
#define TWS_SUPPORT_FEATURE_HFP_TWS				0
#endif

#define TWS_WOODPERKER_VERSION_0x11_FEATURE		(TWS_SUPPORT_FEATURE_AAC | TWS_SUPPORT_FEATURE_LOW_LATENCY | \
												TWS_FEATURE_UI_STARTSTOP_CMD | TWS_SUPPORT_FEATURE_HFP_TWS)


/* Current version and feature for ZS285A  stage 2 */
#define TWS_CURRENT_VERSOIN						(0x11)
#define TWS_CURRENT_VERSION_FEATURE				TWS_WOODPERKER_VERSION_0x11_FEATURE

/*====================== btsrv tws define/struct/api =====================================*/
/*TWS mode*/
enum {
	TWS_MODE_SINGLE = 0,		/* Single play mode */
	TWS_MODE_BT_TWS = 1,		/* bluetooth tws play mode */
	TWS_MODE_AUX_TWS = 2,		/* AUX tws mode(linein/pcm/FM RADIO/USB AUDIO/SPDIF RX/HDMI/HDMI ARC) */
	TWS_MODE_MUSIC_TWS = 3,		/* Local music tws mode(sd card/usb disk) */
};

/* TWS drc mode */
enum {
	DRC_MODE_NORMAL =  0,
	DRC_MODE_AUX    = 1,
	DRC_MODE_OFF    = 2,	/* SPDIF close DRC */
	DRC_MODE_OLD_VER = 7,	/* Master V1.2 version, not support set DRC mode */
};

/* TWS power off type */
enum {
	TWS_POWEROFF_SYS =  0,		/* System software poweroff */
	TWS_POWEROFF_KEY = 1,		/* Key software poweroff */
	TWS_POWEROFF_S2  = 2,		/* Enter S2 */
};

enum {
	TWS_DEVICE_CHANNEL_NONE = 0,
    TWS_DEVICE_CHANNEL_L    = 1,
    TWS_DEVICE_CHANNEL_R    = 2,
};

/* dev_spk_pos_e */
enum {
	TWS_SPK_STEREO	= 0,
	TWS_SPK_LEFT	= 1,
	TWS_SPK_RIGHT	= 2,
};

/* TWS UI SYNC CMD */
enum {
	TWS_SYNC_S2M_UI_SYNC_REPLY				= 0x01,		/* Replay master sync command */
	TWS_SYNC_S2M_UI_UPDATE_BAT_EV			= 0x02,		/* Update battery level */
	TWS_SYNC_S2M_UI_UPDATE_KEY_EV			= 0x03,		/* Update key message */
	TWS_SYNC_S2M_UI_PLAY_TTS_REQ			= 0x04,		/* Request play tts */
	TWS_SYNC_S2M_UI_UPDATE_BTPLAY_EV		= 0x05,		/* Update enter/exit btplay mode, version */

	TWS_SYNC_S2M_UI_EXCHANGE_FEATURE		= 0x40,		/* Exchange feature, just for woodperker */

	TWS_SYNC_M2S_UI_SET_VOL_VAL_CMD			= 0x81,		/* Set volume */
	TWS_SYNC_M2S_UI_SET_VOL_LIMIT_CMD		= 0x82,		/* Set voluem limit */
	TWS_SYNC_M2S_UI_SWITCH_POS_CMD			= 0x83,		/* Switch sound channel */
	TWS_SYNC_M2S_UI_POWEROFF_CMD			= 0x84,		/* Poweroff */
	TWS_SYNC_M2S_UI_TTS_PLAY_REQ			= 0x85,		/* Request slave play tts */
	TWS_SYNC_M2S_UI_TTS_STA_REQ				= 0x86,		/* Check slave tts play state */
	TWS_SYNC_M2S_UI_TTS_STOP_REQ			= 0x87,		/* Request slave stop play tts */
	TWS_SYNC_M2S_SWITCH_NEXT_APP_CMD		= 0x88,		/* Notify switch to next app */

	TWS_SYNC_M2S_UI_SWITCH_TWS_MODE_CMD		= 0x90,		/* Swith tws mode */
	TWS_SYNC_M2S_UI_ACK_UPDATE_BTPLAY		= 0x91,		/* Replay receive slave TWS_SYNC_S2M_UI_UPDATE_BTPLAY_EV command */

	TWS_SYNC_M2S_UI_EXCHANGE_FEATURE		= 0xC0,		/* Exchange feature, just for woodperker */
	TWS_SYNC_M2S_START_PLAYER				= 0xC1,		/* Notify start player, just for support TWS_FEATURE_UI_STARTSTOP_CMD */
	TWS_SYNC_M2S_STOP_PLAYER				= 0xC2,		/* Notify stop player, just for support TWS_FEATURE_UI_STARTSTOP_CMD */
};

/* tws_sync_reply_e */
enum {
	TWS_SYNC_REPLY_ACK = 0,		/* Sync command replay ACK */
	TWS_SYNC_REPLY_NACK = 1,	/* Sync command replay NACK */
};


enum
{
	/*===================== tws inner event define start =================================*/
	TWS_EVENT_MSG_ID = 0,
	TWS_EVENT_REPLY,
	TWS_EVENT_SYNC_PAIR_MODE,
	TWS_EVENT_ENTER_PAIR_MODE,
	TWS_EVENT_CLEAR_PAIRED_LIST,
	TWS_EVENT_SYS_NOTIFY,			  // �¼�֪ͨ
	TWS_EVENT_BT_MUSIC_VOL,
	TWS_EVENT_BT_CALL_VOL,
	TWS_EVENT_BT_HID_SEND_KEY,
	TWS_EVENT_SYNC_PHONE_INFO,        // sync phone snoop info
	TWS_EVENT_SYNC_POWEROFF_INFO,		/* For tws poweroff */
	TWS_EVENT_SYNC_SNOOP_SWITCH_INFO,	/* For app sync info in snoop switch */
    TWS_EVENT_SYNC_A2DP_STATUS, 
    TWS_EVENT_SYNC_LEA_INFO,
    TWS_EVENT_SYNC_ACTIVE_APP,
 	TWS_EVENT_BT_LEA_VOL,
	TWS_EVENT_SYNC_BLE_MAC,

	/* Let the tws slave advertise directed_adv */
	TWS_EVENT_ENABLE_LEA_DIR_ADV = 0x30,

	/*===================== tws inner event define end =================================*/
	TWS_EVENT_APP_MSG_BEGIN_ID = 0x40,
	TWS_EVENT_APP_LONG_MSG_BEGIN_ID = 0x80,
};

typedef enum {
     TWS_PAIR_SEARCH_BY_KEY,
     TWS_POWER_ON_AUTO_PAIR,
}btsrv_tws_pair_mode_e;


typedef struct
{
    unsigned char sub_group_id;
    unsigned char cmd_id;
    unsigned char cmd_len;	
    unsigned char reserved;	
    unsigned int  sync_clock;
    unsigned char cmd_data[32];
} tws_message_t;


typedef struct
{
    unsigned char  sub_group_id;
    unsigned char  cmd_id;
    unsigned short cmd_len;
} tws_long_message_t;
typedef enum {
	TWS_UI_EVENT = 0x01,
	TWS_INPUT_EVENT = 0x02,
	TWS_SYSTEM_EVENT = 0x03,
	TWS_VOLUME_EVENT = 0x04,
	TWS_STATUS_EVENT = 0x05,
	TWS_BATTERY_EVENT = 0x06,
	TWS_USER_APP_EVENT = 0x07, /*see tws_message_t*/
	TWS_LONG_MSG_EVENT = 0x08, /*see tws_long_message_t*/
    TWS_BT_MGR_EVENT = 0x09,
} btsrv_tws_event_type_e;

/*===================== Retransmit tws define end =================================*/

/*===================== Snoop tws define start =================================*/
/* Snoop tws feature */
#define SNOOP_TWS_FEATURE_xxx					(0x01 << 0)
#define SNOOP_TWS_EXT_FEATURE			        (0x01 << 31)

#define SNOOP_TWS_VERSION_0x0100_FEATURE		(TWS_SUPPORT_FEATURE_AAC | TWS_SUPPORT_FEATURE_LDAC)

#define SNOOP_TWS_CURRENT_VERSOIN				(0x0100)
#define SNOOP_TWS_CURRENT_VERSION_FEATURE		SNOOP_TWS_VERSION_0x0100_FEATURE

#define TWS_KEYWORD_PAIR_SEARCH_BY_KEY  0xF1
#define TWS_KEYWORD_POWER_ON_AUTO_PAIR  0xF2

/* SNOOP_TWS_SYNC_GROUP_UI message ID, whether used retransmit tws message ID ? */
enum
{
	SNOOP_TWS_UI_EVENT = 0x01,
	SNOOP_TWS_INPUT_EVENT = 0x02,
	SNOOP_TWS_SYSTEM_EVENT = 0x03,
	SNOOP_TWS_VOLUME_EVENT = 0x04,
	SNOOP_TWS_STATUS_EVENT = 0x05,
	SNOOP_TWS_BATTERY_EVENT = 0x06,
};

/*===================== Snoop tws define end =================================*/

#if CONFIG_SNOOP_LINK_TWS
#define get_tws_current_versoin()				SNOOP_TWS_CURRENT_VERSOIN
#define get_tws_current_feature()				SNOOP_TWS_CURRENT_VERSION_FEATURE
#else
#define get_tws_current_versoin()				TWS_CURRENT_VERSOIN
#define get_tws_current_feature()				TWS_CURRENT_VERSION_FEATURE
#endif
/** Callbacks to report Bluetooth service's tws events*/
typedef enum {
    BTSRV_TWS_INIT_COMPLETE,
	/** notifying to check is pair tws deivce */
	BTSRV_TWS_DISCOVER_CHECK_DEVICE,
	/** notifying that tws connected */
	BTSRV_TWS_CONNECTED,
	/** notifying that tws role change */
	BTSRV_TWS_ROLE_CHANGE,
	/** notifying that tws disconnected */
	BTSRV_TWS_DISCONNECTED,
	/** notifying that tws start play */
	BTSRV_TWS_START_PLAY,
	/** notifying that tws play suspend */
	BTSRV_TWS_PLAY_SUSPEND,
	/** notifying that tws ready play */
	BTSRV_TWS_READY_PLAY,
	/** notifying that tws restart play*/
	BTSRV_TWS_RESTART_PLAY,
	/** notifying that tws event*/
	BTSRV_TWS_EVENT_SYNC,
	/** notifying that tws a2dp connected */
	BTSRV_TWS_A2DP_CONNECTED,
	/** notifying that tws hfp connected */
	BTSRV_TWS_HFP_CONNECTED,
	/** notifying tws irq trigger, be carefull, call back in irq context  */
	BTSRV_TWS_IRQ_CB,
	/** notifying still have pending start play after disconnected */
	BTSRV_TWS_UNPROC_PENDING_START_CB,
	/** notifying slave btplay mode */
	BTSRV_TWS_UPDATE_BTPLAY_MODE,
	/** notifying peer version */
	BTSRV_TWS_UPDATE_PEER_VERSION,
	/** notifying sink start play*/
	BTSRV_TWS_SINK_START_PLAY,
	/** notifying tws failed to do tws pair */
	BTSRV_TWS_PAIR_FAILED,
	/** Callback request app want tws role */
	BTSRV_TWS_EXPECT_TWS_ROLE,
	/** notifying received sco data */
	BTSRV_TWS_SCO_DATA,
	/** notifying call state */
	BTSRV_TWS_HFP_CALL_STATE,
	/** tws callback request audio channel: local and remote  */
	BTSRV_TWS_PRE_SEL_CHANNEL,
	/** tws callback confirm audio channel: local and remote  */
	BTSRV_TWS_CONFIRM_SEL_CHANNEL,
	/** notifying that tws event through upper app*/
	BTSRV_TWS_EVENT_THROUGH_UPPER,
	/*  notify that clear tws list complete*/
	BTSRV_TWS_CLEAR_LIST_COMPLETE,

	/** tws callback request snoop switch */
	BTSRV_TWS_REQUEST_SNOOP_SWITCH = 0x40,
	/** tws callback notify start snoop switch */
	BTSRV_TWS_SNOOP_SWITCH_START_NOTIFY,
	/** tws callback check if can do snoop switch */
	BTSRV_TWS_SNOOP_CHECK_READY_TO_SWITCH,
	/** tws callback snoop switch finish */
	BTSRV_TWS_SNOOP_SWITCH_FINISH,
	/** tws callback check ble switch is finish */
	BTSRV_TWS_SNOOP_CHECK_BLE_SWITCH,
} btsrv_tws_event_e;

/** avrcp device state */
typedef enum {
	BTSRV_TWS_NONE,
	BTSRV_TWS_PENDING,		/* Get name finish but a2dp not connected, unkown a2dp source role */
	BTSRV_TWS_MASTER,
	BTSRV_TWS_SLAVE,
    BTSRV_TWS_TEMP_MASTER, /*TWS is not connected, but the previous role is MASTER*/
	BTSRV_TWS_TEMP_SLAVE,  /*TWS is not connected, but the previous role is SLAVE*/
} btsrv_role_e;

typedef enum {
	BTSRV_SNOOP_NONE,		/* Phone or tws link not as snoop link */
	BTSRV_SNOOP_MASTER,		/* Phone link, as snoop master, need sync info to snoop slave  */
	BTSRV_SNOOP_SLAVE,		/* Phone link as snoop slave, receive sync info from snoop master */
} btsrv_snoop_role_e;

typedef enum {
	TWS_RESTART_SAMPLE_DIFF,
	TWS_RESTART_DECODE_ERROR,
	TWS_RESTART_LOST_FRAME,
	TWS_RESTART_ALREADY_RUNING,
	TWS_RESTART_MONITOR_TIMEOUT,
} btsrv_tws_restart_reason_e;

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

typedef int (*btsrv_tws_callback)(int event, void *param, int param_size);
typedef int (*media_trigger_start_cb)(void *handle);
typedef int (*media_is_ready_cb)(void *handle);
typedef uint32_t (*media_get_samples_cnt_cb)(void *handle, uint8_t *audio_mode);
typedef int (*media_get_error_state_cb)(void *handle);
typedef int (*media_get_aps_level_cb)(void *handle);
typedef int (*media_get_sample_rate_cb)(void *handle);

struct update_version_param {
	uint16_t versoin;
	uint32_t feature;
};

struct update_channel_param {
	uint8_t local_acl_requested;
	uint8_t sw_sel_dev_channel;
	uint8_t tws_dev_channel;
};

typedef struct {
	void *media_handle;
	media_is_ready_cb		is_ready;
	media_trigger_start_cb   trigger_start;
	media_get_samples_cnt_cb  get_samples_cnt;
	media_get_error_state_cb get_error_state;
	media_get_aps_level_cb   get_aps_level;
	media_get_sample_rate_cb    get_sample_rate;
} media_runtime_observer_t;

typedef int (*btsrv_tws_get_role)(void);
typedef int (*btsrv_tws_get_sample_diff)(int *samples_diff, uint8_t *aps_level, uint16_t *bt_clock);
typedef int (*btsrv_tws_set_media_observer)(media_runtime_observer_t *observer);
typedef int (*btsrv_tws_aps_change_notify)(uint8_t level);
typedef int (*btsrv_tws_trigger_restart)(uint8_t reason);
typedef int (*btsrv_tws_exchange_samples)(uint32_t *ext_add_samples, uint32_t *sync_ext_samples);
typedef int (*btsrv_tws_aps_nogotiate)(uint8_t *req_level, uint8_t *curr_level);

/* For snoop tws */
typedef void (*tws_irq_handle)(uint64_t bt_clk_us);
typedef void (*btsrv_tws_set_interrupt_callback)(tws_irq_handle cb);
typedef int (*btsrv_tws_set_interrupt_time)(uint64_t bt_clk_us);
typedef uint32_t (*btsrv_tws_get_first_pkt_clk)(void);
typedef uint64_t (*btsrv_tws_get_bt_clk_us)(void);
typedef int32_t (*btsrv_tws_send_pkt)(void *buf, int32_t size);
typedef void (*tws_recv_pkt_handle)(void *buf, int32_t size);
typedef int32_t (*btsrv_tws_set_recv_callback)(tws_recv_pkt_handle cb);
typedef int (*btsrv_tws_set_period_interrupt_time)(uint64_t start_bt_clk_us, uint64_t period_us);

typedef struct {
	btsrv_tws_set_media_observer set_media_observer;
	btsrv_tws_get_role           get_role;
	btsrv_tws_get_sample_diff	 get_samples_diff;
	btsrv_tws_aps_change_notify  aps_change_notify;
	btsrv_tws_trigger_restart    trigger_restart;
	btsrv_tws_exchange_samples   exchange_samples;
	btsrv_tws_aps_nogotiate      aps_nogotiate;

	/* For snoop tws */
	btsrv_tws_set_interrupt_callback set_interrupt_cb;
	btsrv_tws_set_interrupt_time     set_interrupt_time;
	btsrv_tws_get_first_pkt_clk      get_first_pkt_clk;
	btsrv_tws_get_bt_clk_us          get_bt_clk_us;
	btsrv_tws_send_pkt               send_pkt;
	btsrv_tws_set_recv_callback      set_recv_pkt_cb;
	btsrv_tws_set_period_interrupt_time set_period_interrupt_time;
	btsrv_tws_set_interrupt_callback set_twssrv_period_irq_cb;
} tws_runtime_observer_t;

typedef struct {
	uint8_t enable;
	uint8_t interval;
	uint8_t connected_phone;
	uint8_t pair_keyword;
	uint8_t device_channel;
	uint8_t filtering_rules;
	uint8_t filtering_name_length;
}tws_visual_parameter_t;

typedef struct {
	bd_address_t addr;
	uint16_t interval;
	uint16_t window;
	int8_t rssi_thres;
	uint8_t mask_addr;
	uint8_t connected_phone;
	uint8_t mask_phone;
	uint8_t channel;		/* L/R channel */
	uint8_t mask_channel;
	uint8_t match_mode;
	uint8_t mask_mode;
	uint8_t pair_keyword;
	uint8_t mask_keyword;
	uint8_t match_len;
	uint8_t reserve;
}tws_pair_parameter_t;

typedef struct {
	btsrv_tws_callback cb;
	uint8_t quality_monitor;
	uint8_t quality_pre_val;
	uint8_t quality_diff;
	uint8_t quality_esco_diff;
	uint8_t advanced_pair_mode;
} tws_init_param;

int btif_tws_register_processer(void);
int btif_tws_init(tws_init_param *param);
int btif_tws_deinit(void);
int btif_tws_context_init(void);
int btif_tws_start_pair(int try_times);
int btif_tws_end_pair(void);
int btif_tws_disconnect(bd_address_t *addr);
int btif_tws_can_do_pair(void);
int btif_tws_is_in_connecting(void);
int btif_tws_is_in_pair_seraching(void);
int btif_tws_get_dev_role(void);
int btif_tws_set_input_stream(io_stream_t stream);
int btif_tws_set_sco_input_stream(io_stream_t stream);
void btif_tws_set_expect_role(uint8_t role);
tws_runtime_observer_t *btif_tws_get_tws_observer(void);
int btif_tws_send_command(uint8_t *data, int len);
int btif_tws_send_command_sync(uint8_t *data, int len);
/* btif_tws_get_bt_clock return bt clock in ms */
uint32_t btif_tws_get_bt_clock(void);
int btif_tws_set_irq_time(uint32_t bt_clock_ms);
int btif_tws_wait_ready_send_sync_msg(uint32_t wait_timeout);
int btif_tws_set_bt_local_play(bool bt_play, bool local_play);
void btif_tws_update_tws_mode(uint8_t tws_mode, uint8_t drc_mode);
bool btif_tws_check_is_woodpecker(void);
bool btif_tws_check_support_feature(uint32_t feature);
void btif_tws_set_codec(uint8_t codec);
int btif_tws_carete_snoop_link(void);
int btif_tws_disconnect_snoop_link(void);
int btif_tws_switch_snoop_link(void);
void btif_tws_notify_start_power_off(void);
void btif_tws_set_visual_param(tws_visual_parameter_t *param);
tws_visual_parameter_t * btif_tws_get_visual_param(void);
void btif_tws_set_pair_param(tws_pair_parameter_t *param);
uint8_t btif_tws_get_pair_keyword(void);
int btif_tws_set_state(uint8_t state);
uint8_t btif_tws_get_state(void);
void btif_tws_set_pair_keyword(uint8_t pair_keyword);
void btif_tws_set_pair_addr_match(bool enable,bd_address_t *addr);
uint8_t btif_tws_get_pair_addr_match(void);
void btif_tws_set_rssi_threshold(int8_t threshold);
int btif_tws_update_visual_pair(void);
void btif_tws_set_leaudio_active(int8_t leaudio_active);
#endif
