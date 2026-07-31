/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt manager.
*/

#ifndef __BT_MANAGER_H__
#define __BT_MANAGER_H__
#include <stream.h>
#include "btservice_api.h"
#include "bt_manager_ble.h"
#include "bt_manager_pawr.h"
#include "bt_manager_gatt_over_br.h"
#include "bt_manager_audio.h"
#include <msg_manager.h>
#include "bt_manager_config.h"
#include "bt_manager_lea_policy.h"
#include "bt_manager_letws.h"
#include "bt_manager_lea_stereo.h"



/**
 * @brief system APIs
 * @defgroup bluetooth_system_apis Bluetooth system APIs
 * @{
 * @}
 */

/**
 * @defgroup bt_manager_apis Bt Manager APIs
 * @ingroup bluetooth_system_apis
 * @{
 */
#define MAX_MGR_DEV 	              3
#define BT_MANAGER_MAX_SCAN_MODE      7

#define BT_MANAGER_AVRCP_PASSTHROUGH_PAUSE   0x46
#define BT_MANAGER_AVRCP_PASSTHROUGH_PLAY    0x44

#ifdef CONFIG_BT_MAX_BR_CONN
#define BT_MANAGER_MAX_BR_NUM	CONFIG_BT_MAX_BR_CONN
#else
#define BT_MANAGER_MAX_BR_NUM 1

#endif

#define CFG_BT_MANAGER_MAX_BR_NUM  BT_MANAGER_MAX_BR_NUM

#define BT_MAX_AUTOCONN_DEV				3
#define BT_TRY_AUTOCONN_DEV				2
#define BT_BASE_DEFAULT_RECONNECT_TRY	3
#define BT_BASE_STARTUP_RECONNECT_TRY	3
#define BT_BASE_TIMEOUT_RECONNECT_TRY	15
#define BT_TWS_SLAVE_DISCONNECT_RETRY	10
#define BT_BASE_RECONNECT_INTERVAL		60
#define BT_PROFILE_RECONNECT_TRY		3
#define BT_PROFILE_RECONNECT_INTERVAL	50
#define BT_RECONNECT_PHONE_TIMEOUT      50
#define BT_RECONNECT_TWS_TIMEOUT        50

typedef enum{
	BT_MGR_EVENT_READY = 0,
	BT_MGR_EVENT_POWEROFF_RESULT,
	BT_MGR_EVENT_LEA_RESTART,
} btmgr_event_t;


/**bt stream type*/
typedef enum {
    STREAM_TYPE_A2DP,
	STREAM_TYPE_LOCAL,
	STREAM_TYPE_SCO,
    STREAM_TYPE_SPP,
    STREAM_TYPE_MAX_NUM,
} bt_stream_type_e;

typedef enum{
    BT_AUDIO_STREAM_NONE  =   0,
    BT_AUDIO_STREAM_BR_CALL  =   1,
    BT_AUDIO_STREAM_BR_MUSIC =   2,
    BT_AUDIO_STREAM_LE_CALL  =   3,
    BT_AUDIO_STREAM_LE_MUSIC =	 4,
	BT_AUDIO_STREAM_LE_TWS   =   5,
}bt_audio_stream_e;

struct bt_manager_stream_report{
	uint16_t handle;
	/*see @bt_audio_stream_e*/
	uint8_t stream;
};

struct bt_manager_tws_report {
	uint8_t  cmd_id;
	uint8_t  cmd_len;
	uint8_t  cmd_data[32];
};

/**bt event type*/
typedef enum {
	BT_CONNECTED = 1,
	BT_DISCONNECTED,
	BT_AUDIO_CONNECTED,
	BT_AUDIO_DISCONNECTED,
	/* param null*/
	BT_A2DP_CONNECTION_EVENT,
	/* dev_addr,param :reason*/
	BT_A2DP_DISCONNECTION_EVENT,
	/* param null*/
	BT_HFP_CONNECTION_EVENT ,
	/* param null*/
	BT_HFP_DISCONNECTION_EVENT,
	BT_AUDIO_ASCS_CONNECTION,
	BT_AUDIO_ASCS_DISCONNECTION = 10,

	/* for slave */
	BT_AUDIO_STREAM_CONFIG_CODEC,
	/* for master */
	BT_AUDIO_STREAM_PREFER_QOS,
	BT_AUDIO_STREAM_CONFIG_QOS,
	BT_AUDIO_STREAM_ENABLE,
	BT_AUDIO_STREAM_UPDATE,
	BT_AUDIO_STREAM_DISABLE,
	BT_AUDIO_STREAM_START,
	BT_AUDIO_STREAM_STOP,
	BT_AUDIO_STREAM_RELEASE,

	/* for master */
	BT_AUDIO_DISCOVER_CAPABILITY,
	/* for master */
	BT_AUDIO_DISCOVER_ENDPOINTS,

	BT_AUDIO_PARAMS,

	/* restore audio stream */
	BT_AUDIO_STREAM_RESTORE,
	/* restore audio volume */
	BT_AUDIO_VOLUME_RESTORE,

	/* notify for lemusic when bt audio switch */
	BT_AUDIO_SWITCH_NOTIFY,

	/* notify for lemusic to update bt scan */
	BT_AUDIO_UPDATE_SCAN_MODE,

	/*
	 * Fake a audio stream for BR Audio only yet.
	 *
	 * BR call event may come without audio stream, but we
	 * depends a stream to switch active_slave.
	 */
	BT_AUDIO_STREAM_FAKE,

	BT_MEDIA_CONNECTED = 30,
	BT_MEDIA_DISCONNECTED,
	/* param: avrcp_ui.h 6.7 Notification PDUs */
	BT_MEDIA_PLAYBACK_STATUS_CHANGED_EVENT,
	/* param: null */
	BT_MEDIA_TRACK_CHANGED_EVENT,
	/* param: struct id3_info avrcp.h */
	BT_MEDIA_UPDATE_ID3_INFO_EVENT,

	BT_MEDIA_PASSTHROU_RELEASE_EVENT,

	/* as a server, report message initiated by a client */
	BT_MEDIA_SERVER_PLAY = 40,
	BT_MEDIA_SERVER_PAUSE,
	BT_MEDIA_SERVER_STOP,
	BT_MEDIA_SERVER_FAST_REWIND,
	BT_MEDIA_SERVER_FAST_FORWARD,
	BT_MEDIA_SERVER_NEXT_TRACK,
	BT_MEDIA_SERVER_PREV_TRACK,
	/* as a client, report message initiated by a server */
	BT_MEDIA_PLAY,
	BT_MEDIA_PAUSE,
	BT_MEDIA_STOP,

	BT_CALL_CONNECTED = 80,	// TODO: 65?
	BT_CALL_DISCONNECTED,
	BT_CALL_INCOMING,
	BT_CALL_DIALING,	/* outgoing */
	BT_CALL_ALERTING,	/* outgoing */
	BT_CALL_ACTIVE,
	BT_CALL_LOCALLY_HELD,
	BT_CALL_REMOTELY_HELD,
	BT_CALL_HELD,
	BT_CALL_ENDED,
	BT_CALL_SIRI_MODE,
	BT_CALL_3WAYIN,
	BT_CALL_3WAYOUT,
	BT_CALL_MULTIPARTY,
	BT_CALL_RING_STATR_EVENT,
	BT_CALL_RING_STOP_EVENT,

	/* as a slave, report message initiated by a master */
	BT_VOLUME_UP = 120,	// TODO: 95?
	BT_VOLUME_DOWN,
	BT_VOLUME_VALUE,
	BT_VOLUME_MUTE,
	BT_VOLUME_UNMUTE,
	BT_VOLUME_UNMUTE_UP,
	BT_VOLUME_UNMUTE_DOWN,
	/* as a master, report message initiated by a slave */
	BT_VOLUME_CLIENT_CONNECTED,
	BT_VOLUME_CLIENT_UP,
	BT_VOLUME_CLIENT_DOWN,
	BT_VOLUME_CLIENT_VALUE,
	BT_VOLUME_CLIENT_MUTE,
	BT_VOLUME_CLIENT_UNMUTE,
	BT_VOLUME_CLIENT_UNMUTE_UP,
	BT_VOLUME_CLIENT_UNMUTE_DOWN,

	/* as a slave, report message initiated by a master */
	BT_MIC_MUTE = 150,	// TODO: 125?
	BT_MIC_UNMUTE,
	BT_MIC_GAIN_VALUE,
	/* as a master, report message initiated by a slave */
	BT_MIC_CLIENT_CONNECTED,
	BT_MIC_CLIENT_MUTE,
	BT_MIC_CLIENT_UNMUTE,
	BT_MIC_CLIENT_GAIN_VALUE,

	/* param null */
	BT_TWS_CONNECTION_EVENT,
	 /* param null */
	BT_TWS_DISCONNECTION_EVENT,
	/* param null */
	BT_TWS_CHANNEL_MODE_SWITCH,
	/* param null */
	BT_TWS_ROLE_CHANGE,
	/* param null*/
	BT_REQ_RESTART_PLAY,

	/*
	 * Broadcast Audio
	 */
	/* broadcast source */
	BT_BROADCAST_SOURCE_CONFIG = 170,
	BT_BROADCAST_SOURCE_ENABLE,
	BT_BROADCAST_SOURCE_UPDATE,
	BT_BROADCAST_SOURCE_DISABLE,
	BT_BROADCAST_SOURCE_RELEASE,
	BT_BROADCAST_SOURCE_SENDDATA_EMPTY,
	BT_BROADCAST_SOURCE_UPDATE_SQ,

	/* broadcast sink */
	BT_BROADCAST_SINK_CONFIG = 180,
	BT_BROADCAST_SINK_ENABLE,
	BT_BROADCAST_SINK_UPDATE,
	BT_BROADCAST_SINK_DISABLE,
	BT_BROADCAST_SINK_RELEASE,
	BT_BROADCAST_SINK_BASE_CONFIG,
	BT_BROADCAST_SINK_SYNC,

	BT_BIS_CONNECTED,
	BT_BIS_DISCONNECTED,

	/* broadcast assistant */
	BT_BROADCAST_ASSISTANT_xxx = 190,

	/* scan delegator */
	BT_SCAN_DELEGATOR_xxx = 200,

	BT_ASCS_ENABLE = 220,

	/* This value will transter to different tws version, must used fixed value */
	/* param null */
	BT_TWS_START_PLAY = 0xE0,
	/* param null */
	BT_TWS_STOP_PLAY = 0xE1,

	/* param:NULL */
	BT_HFP_SIRI_START,
	/* param:NULL */
	BT_HFP_SIRI_STOP,
	/* param null */
	BT_HID_CONNECTION_EVENT,
	/* param null */
	BT_HID_DISCONNECTION_EVENT,
	/* param hid_active_id */
	BT_HID_ACTIVEDEV_CHANGE_EVENT,

	/* BT Audio switched, need to restore */
	BT_AUDIO_SWITCH = 0xF0,
	/* PAWR */
	BT_PAWR_SYNCED,
	BT_PAWR_SYNC_LOST,
	BT_PAWR_SCAN_SYNCED,
	BT_PAWR_SCAN_SYNC_LOST,
	BT_PAWR_SCAN_SYNCING,

	/*bluetooth init success, eg. btserver/btdrv ready.*/
	BT_READY,
	/*bluetooth poweoff, param: see @bt_manager_poweroff_result_e*/
	BT_POWEROFF_RESULT,
	/*bluetooth request restart initial LEaudio*/
	BT_REQUEST_LEAUDIO_RESTART,

    BT_GFP_BLE_CONNECTED,
    BT_GFP_BLE_DISCONNECTED,
    BT_GFP_BLE_INITIAL_PAIRING,

	/* param: see @ struct bt_manager_tws_report */
	BT_TWS_EVENT,

	BT_KEY_MISS,

	BT_SECURITY_CHANGED,
}bt_event_type_e;

/** bt link state */
enum BT_LINKS_STATUS {
    BT_STATUS_LINK_NONE,
    BT_STATUS_WAIT_CONNECT,
    BT_STATUS_PAIR_MODE,
    BT_STATUS_CONNECTED,
    BT_STATUS_DISCONNECTED,
    BT_STATUS_TWS_PAIRED,
    BT_STATUS_TWS_UNPAIRED,
    BT_STATUS_TWS_PAIR_SEARCH,
};

/** bt play state */
enum BT_PLAY_STATUS {
    BT_STATUS_INACTIVE              = 0x0000,
    BT_STATUS_PAUSED                = 0x0001,
    BT_STATUS_PLAYING               = 0x0002,
};

enum BT_HFP_STATUS {
	BT_STATUS_HFP_NONE              = 0x0000,
	BT_STATUS_INCOMING              = 0x0001,
	BT_STATUS_OUTGOING              = 0x0002,
	BT_STATUS_ONGOING               = 0x0004,
	BT_STATUS_3WAYOUT				= 0x0010,
	BT_STATUS_MULTIPARTY            = 0x0020,
	BT_STATUS_3WAYIN                = 0x0040,
	BT_STATUS_SIRI                  = 0x0080,

};


/** bt manager battery report mode*/
enum BT_BATTERY_REPORT_MODE_E {
	/** bt manager battery report mode init*/
    BT_BATTERY_REPORT_INIT = 1,
	/** bt manager battery report mode report data*/
    BT_BATTERY_REPORT_VAL,
};

#define BT_STATUS_A2DP_ALL (BT_STATUS_PAUSED | BT_STATUS_PLAYING)

#define BT_STATUS_HFP_ALL   (BT_STATUS_INCOMING | BT_STATUS_OUTGOING \
				| BT_STATUS_ONGOING | BT_STATUS_MULTIPARTY | BT_STATUS_SIRI | BT_STATUS_3WAYIN | BT_STATUS_3WAYOUT)


typedef enum {
    BT_PAIR_MODE_STATE_NONE,
	BT_PAIR_MODE_TWS_SYNC,
	BT_PAIR_MODE_TWS_SYNC_WAIT,
    BT_PAIR_MODE_DISCONNECT_PHONE,
	BT_PAIR_MODE_CHECK_PHONE,
    BT_PAIR_MODE_CHECK_RECONNECT,
    BT_PAIR_MODE_START_PAIR,
	BT_PAIR_MODE_RUNNING,
} bt_pair_mode_state_e;

typedef enum {
    BT_LETWS_MODE_STATE_NONE,
	BT_LETWS_MODE_SCAN,
	BT_LETWS_MODE_ADV,
	BT_LETWS_MODE_CONNECTING,
    BT_LETWS_MODE_CONNECTED,
    BT_LETWS_MODE_DISCONNECT,
	BT_LETWS_MODE_SCAN_ADV_ALTERNATE,
} bt_letws_mode_state_e;


typedef enum {
    BT_CLEAR_PAIRED_LIST_NONE,
    BT_CLEAR_PAIRED_LIST_TWS_SYNC,
    BT_CLEAR_PAIRED_LIST_TWS_SYNC_WAIT,
    BT_CLEAR_PAIRED_LIST_DISCONNECT_PHONE,
    BT_CLEAR_PAIRED_LIST_PHONE_CLEAR,
    BT_CLEAR_PAIRED_LIST_TWS_CLEAR,
    BT_CLEAR_PAIRED_LIST_COMPLETE,
    BT_CLEAR_PAIRED_LIST_RESTORE_STATE,
} bt_clear_paired_list_state_e;

/* Bt manager request poweoff result */
enum {
	BTMGR_LREQ_POFF_RESULT_OK             = 0,		/* Local request bt manager poweroff ok */
	BTMGR_LREQ_POFF_RESULT_TIMEOUT        = 1,		/* Local request bt manager poweroff timeout */
	BTMGR_RREQ_SYNC_POFF_RESULT_OK        = 2,		/* Remote request sync bt manager poweroff ok */
	BTMGR_RREQ_SYNC_POFF_RESULT_TIMEOUT   = 3,		/* Remote request sync bt manager poweroff timeout */
};

typedef enum
{
    BT_MANAGER_REBOOT_MODE_NONE = 0,
    BT_MANAGER_REBOOT_CLEAR_TWS_INFO  = (1 << 0),
    BT_MANAGER_REBOOT_ENTER_PAIR_MODE = (1 << 1),
} bt_manager_reboot_mode_t;

enum {
	NONE_CONNECT_TYPE,
	SPP_CONNECT_TYPE,
	BLE_CONNECT_TYPE,
	GATT_OVER_BR_CONNECT_TYPE,
};

/**bt manager spp call back*/
struct btmgr_spp_cb {
	void (*connect_failed)(uint8_t channel);
	void (*connected)(uint8_t channel, uint8_t *uuid);
	void (*disconnected)(uint8_t channel);
	void (*receive_data)(uint8_t channel, uint8_t *data, uint32_t len);
};

/**bt manager spp ble strea init param*/
struct sppble_stream_init_param {
	uint8_t *spp_uuid;
	void *gatt_attr;
	uint8_t attr_size;
	void *tx_chrc_attr;
	void *tx_attr;
	void *tx_ccc_attr;
	void *rx_attr;
	void *connect_cb;
	void *rx_data_on_changed_con_cb;
	void *connect_filter_cb;
	int32_t read_timeout;
	int32_t write_timeout;
	int32_t read_buf_size;
	uint8_t write_attr_enable_ccc;
};

struct mgr_pbap_result {
	uint8_t type;
	uint16_t len;
	uint8_t *data;
};

struct mgr_map_result {
	uint8_t type;
	uint16_t len;
	uint8_t *data;
};

struct btmgr_pbap_cb {
	void (*connect_failed)(uint8_t app_id);
	void (*connected)(uint8_t app_id);
	void (*disconnected)(uint8_t app_id);
	void (*result)(uint8_t app_id, struct mgr_pbap_result *result, uint8_t size);
};

struct btmgr_map_cb {
	void (*connect_failed)(uint8_t app_id);
	void (*connected)(uint8_t app_id);
	void (*disconnected)(uint8_t app_id);
	void (*set_path_finished)(uint8_t user_id);
	void (*result)(uint8_t app_id, struct mgr_map_result *result, uint8_t size);
};

typedef struct  {
	int8_t event;
	const char *str;
}bt_mgr_event_strmap_t;

typedef int (*bt_manager_event_callback_t)(uint8_t event, uint8_t* extra, uint32_t extra_len);

/**
 * @brief bt manager init
 *
 * This routine init bt manager
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_init(bt_manager_event_callback_t event_cb);

/**
 * @brief bt manager deinit
 *
 * This routine deinit bt manager
 *
 * @return N/A
 */
void bt_manager_deinit(void);

/**
 * @brief bt manager check inited
 *
 * This routine check bt manager is inited
 *
 * @return true if inited others false
 */
bool bt_manager_is_ready(void);

/**
 * @brief bt manager set bluetooth name
 *
 * This routine set BT NAME
 *
 * @return 0 if ready others false
 */
int bt_manager_set_bt_name(uint8_t *bt_name);

/**
 * @brief bt manager get bluetooth name
 *
 * This routine get BT NAME
 *
 * @return 0 if ready others false
 */
int bt_manager_get_bt_name(uint8_t *buffer, int buffer_len);
bool bt_manager_is_inited(void);



/**
 * @brief Bt manager process power off.
 *
 * This routine Bt manager process power off.
 *
 * @param single_poweroff 0: Tws master/slave sync power off,
 *                                              1: Only current device power off.
 *
 * @return 0: Start successed, others failed.
 */
int bt_manager_proc_poweroff(uint8_t single_poweroff);
/**
 * @brief dump bt manager info
 *
 * This routine dump bt manager info
 *
 * @return N/A
 */
void bt_manager_dump_info(void);

/**
 * @brief bt manager get bt device state
 *
 * This routine provides to get bt device state.
 *
 * @return state of bt device state
 */
int bt_manager_get_status(void);

/**
 * @brief bt manager allow sco connect
 *
 * This routine provides to allow sco connect or not
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_allow_sco_connect(bool allowed);

/**
 * @brief get profile a2dp codec id
 *
 * This routine provides to get profile a2dp codec id .
 *
 * @return codec id of a2dp profile
 */
int bt_manager_a2dp_get_codecid(void);
/**
 * @brief get profile a2dp sample rate
 *
 * This routine provides to get profile a2dp sample rate.
 *
 * @return sample rate of profile a2dp
 */
int bt_manager_a2dp_get_sample_rate(void);

/**
 * @brief check a2dp state
 *
 * This routine use by app to check a2dp playback
 * state,if state is playback ,trigger start event
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_a2dp_check_state(void);

/**
 * @brief Send delay report to a2dp active phone
 *
 * This routine Send delay report to a2dp active phone
 *
 * @param delay_time delay time (unit: 1/10 milliseconds)
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_a2dp_send_delay_report(uint16_t delay_time);

/**
 * @brief enable or disable aac codec
 *
 * This routine enable or disable aac codec
 *
 * @param halt enable or disable
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_a2dp_halt_aac(bool halt);

/**
 * @brief enable or disable ldac codec
 *
 * This routine enable or disable ldac codec
 *
 * @param halt enable or disable
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_a2dp_halt_ldac(bool halt);

/**
 * @brief get a2dp status
 *
 * This routine provides to get a2dp status .
 *
 * @return state of a2dp status, see enum BT_PLAY_STATUS.
 */
int bt_manager_a2dp_get_status(void);

/**
 * @brief set auracast status for master
 *
 * This routine provides to set auracast status .
 *
 * @return.
 */
void bt_manager_a2dp_auracast_status_set(bool status);

/**
 * @brief Control Bluetooth to start playing through AVRCP profile
 *
 * This routine provides to Control Bluetooth to start playing through AVRCP profile.
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_avrcp_play(void);
int bt_manager_avrcp_play_by_hdl(uint16_t hdl);
/**
 * @brief Control Bluetooth to stop playing through AVRCP profile
 *
 * This routine provides to Control Bluetooth to stop playing through AVRCP profile.
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_avrcp_stop(void);
int bt_manager_avrcp_stop_by_hdl(uint16_t hdl);
/**
 * @brief Control Bluetooth to stop playing through AVRCP profile
 *
 * This routine provides to Control Bluetooth to stop playing through AVRCP profile.
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_avrcp_stopplay(void);
/**
 * @brief Control Bluetooth to pause playing through AVRCP profile
 *
 * This routine provides to Control Bluetooth to pause playing through AVRCP profile.
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_avrcp_pause(void);
int bt_manager_avrcp_pause_by_hdl(uint16_t hdl);
/**
 * @brief Control Bluetooth to play next through AVRCP profile
 *
 * This routine provides to Control Bluetooth to play next through AVRCP profile.
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_avrcp_play_next(void);
int bt_manager_avrcp_play_next_by_hdl(uint16_t hdl);
/**
 * @brief Control Bluetooth to play previous through AVRCP profile
 *
 * This routine provides to Control Bluetooth to play previous through AVRCP profile.
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_avrcp_play_previous(void);
int bt_manager_avrcp_play_previous_by_hdl(uint16_t hdl);
/**
 * @brief Control Bluetooth to play fast forward through AVRCP profile
 *
 * This routine provides to Control Bluetooth to play fast forward through AVRCP profile.
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_avrcp_fast_forward(bool start);
int bt_manager_avrcp_fast_forward_by_hdl(uint16_t hdl, bool start);
/**
 * @brief Control Bluetooth to play fast backward through AVRCP profile
 *
 * This routine provides to Control Bluetooth to play fast backward through AVRCP profile.
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_avrcp_fast_backward(bool start);
int bt_manager_avrcp_fast_backward_by_hdl(uint16_t hdl, bool start);
/**
 * @brief Control Bluetooth to sync vol to remote through AVRCP profile
 *
 * This routine provides to Control Bluetooth to sync vol to remote through AVRCP profile.
 *
 * @param vol volume want to sync
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_avrcp_sync_vol_to_remote(uint32_t vol);

/**
 * @brief Control Bluetooth to get current playback track's id3 info through AVRCP profile
 *
 * This routine provides to Control Bluetooth to get id3 info of cur track.
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_avrcp_get_id3_info(void);

#ifdef CONFIG_BT_AVRCP_ID3_UART_DUMP
/**
 * @brief 输出 AVRCP ID3 UART 二进制帧的 **单字节**（曲名/艺人/专辑等元数据帧）。
 *
 * `bt_manager_avrcp` 在组好帧（magic、版本、属性 id、长度、UTF-8 正文、CRC8）后，
 * 每发一字节调用一次。`bt_manager_avrcp.c` 内带 **weak** 默认实现，按 Kconfig 走
 * `CONFIG_BT_AVRCP_ID3_UART_OUT_CONSOLE` 或 `CONFIG_BT_AVRCP_ID3_UART_OUT_POLL`。
 * 应用可提供 **非 weak** 实现，把所有元数据字节改发到指定 UART（例如与 SPP 桥共 UART0）。
 */
void bt_manager_avrcp_id3_uart_tx_byte(unsigned char c);
#endif

#ifdef CONFIG_BT_MANAGER_UART_RX
/**
 * @brief Application hook: UART RX thread delivered one line (without \\r/\\n).
 *
 * Define this symbol in application code (non-weak) to handle commands/data.
 */
void bt_manager_uart_rx_line_hook(const char *line, unsigned int len);
#endif

/**
 * @brief 远程控制命令统一入口（SPP / UART 等解析后的 payload 共用）。
 *
 * @param payload 至少 2 字节，payload[0]==0xD2，payload[1] 为子命令
 * @param len     载荷长度
 * @param src     日志来源标签，如 "spp" / "uart"
 * @return 子命令 API 返回值；格式错误 -EINVAL；未知子命令 -ENOTSUP
 */
int bt_manager_remote_cmd_dispatch(const uint8_t *payload, uint16_t len,
				   const char *src);

#if defined(CONFIG_BT_MANAGER_UART_RX) && defined(CONFIG_BT_MANAGER_UART_RX_PROTO_BINARY_CMD)
/**
 * @brief UART 二进制帧 CRC 通过后的 payload 钩子（默认转 remote_cmd_dispatch）。
 * 应用可强符号覆盖以扩展协议；测试 UART：tools/uart_bt_cmd_test.py
 */
void bt_manager_uart_rx_binary_dispatch(const uint8_t *payload, uint16_t len);
#endif

/**
 * @brief Avrcp set absolute volume
 *
 * @param volume: absolute volume data (1~3byte)
 * @param len: 1~3byte
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_avrcp_set_absolute_volume(uint8_t *data, uint8_t len);

/**
 * @brief get hfp status
 *
 * This routine provides to get hfp status .
 *
 * @return state of hfp status.
 */
int bt_manager_hfp_get_status(void);

/**
 * @brief Control Bluetooth to dial target phone number through HFP profile
 *
 * This routine provides to Control Bluetooth to Control Bluetooth to dial target phone number through HFP profile .
 *
 * @param number number of target phone
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_hfp_dial_number(uint8_t *number);
/**
 * @brief Control Bluetooth to dial last phone number through HFP profile
 *
 * This routine provides to Control Bluetooth to Control Bluetooth to dial last phone number through HFP profile .
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_hfp_dial_last_number(void);
/**
 * @brief Control Bluetooth to dial local memory phone number through HFP profile
 *
 * This routine provides to Control Bluetooth to Control Bluetooth to dial local memory phone number through HFP profile .
 *
 * @param location index of local memory phone number
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_hfp_dial_memory(int location);
/**
 * @brief Control Bluetooth to volume control through HFP profile
 *
 * This routine provides to Control Bluetooth to Control Bluetooth to volume control through HFP profile .
 *
 * @param type type of opertation
 * @param volume level of volume
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_hfp_volume_control(uint8_t type, uint8_t volume);
/**
 * @brief Control Bluetooth to report battery state through HFP profile
 *
 * This routine provides to Control Bluetooth to Control Bluetooth to report battery state through HFP profile .
 *
 * @param mode mode of operation, 0 init mode , 1 report mode
 * @param bat_val battery level value
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_hfp_battery_report(uint8_t mode, uint8_t bat_val);
/**
 * @brief Control Bluetooth to accept call through HFP profile
 *
 * This routine provides to Control Bluetooth to Control Bluetooth to accept call through HFP profile .
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_hfp_accept_call(void);
/**
 * @brief Control Bluetooth to reject call through HFP profile
 *
 * This routine provides to Control Bluetooth to Control Bluetooth to reject call through HFP profile .
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_hfp_reject_call(void);
/**
 * @brief Control Bluetooth to hangup current call through HFP profile
 *
 * This routine provides to Control Bluetooth to Control Bluetooth to hangup current call through HFP profile .
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_hfp_hangup_call(void);
/**
 * @brief Control Bluetooth to hangup another call through HFP profile
 *
 * This routine provides to Control Bluetooth to Control Bluetooth to hangup another call through HFP profile .
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_hfp_hangup_another_call(void);
/**
 * @brief Control Bluetooth to hold current and answer another call through HFP profile
 *
 * This routine provides to Control Bluetooth to Control Bluetooth to hold current and answer another call through HFP profile .
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_hfp_holdcur_answer_call(void);
/**
 * @brief Control Bluetooth to hangup current and answer another call through HFP profile
 *
 * This routine provides to Control Bluetooth to Control Bluetooth to hangup current and answer another call through HFP profile .
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_hfp_hangupcur_answer_call(void);
/**
 * @brief Control Bluetooth to start siri through HFP profile
 *
 * This routine provides to Control Bluetooth to Control Bluetooth to start siri through HFP profile .
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_hfp_start_siri(void);
/**
 * @brief Control Bluetooth to stop siri through HFP profile
 *
 * This routine provides to Control Bluetooth to Control Bluetooth to stop siri through HFP profile .
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_hfp_stop_siri(void);

/**
 * @brief Control Bluetooth to sycn time from phone through HFP profile
 *
 * This routine provides to sycn time from phone through HFP profile .
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_hfp_get_time(void);

/**
 * @brief send user define at command through HFP profile
 *
 * This routine provides to send user define at command through HFP profile .
 *
 * @param command user define at command
 * @param send cmd to active_call or inactive call
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_hfp_send_at_command(uint8_t *command,uint8_t active_call);
/**
 * @brief sync volume through HFP profile
 *
 * This routine provides to sync volume through HFP profile .
 *
 * @param vol volume want to sync
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_hfp_sync_vol_to_remote(uint32_t vol);
/**
 * @brief switch sound source through HFP profile
 *
 * This routine provides to sswitch sound source HFP profile .

 * @return 0 excute successed , others failed
 */
int bt_manager_hfp_switch_sound_source(void);

/**
 * @cond INTERNAL_HIDDEN
 */


/**
 * @brief get profile hfp codec id
 *
 * This routine provides to get profile hfp codec id .
 *
 * @return codec id of hfp profile
 */
/**
 * @cond INTERNAL_HIDDEN
 */
int bt_manager_sco_get_codecid(void);

/**
 * @brief get profile hfp sample rate
 *
 * This routine provides to get profile hfp sample rate.
 *
 * @return sample rate of profile hfp
 */
int bt_manager_sco_get_sample_rate(void);

/**
 * @brief get profile hfp call state
 *
 * This routine provides to get profile hfp call state.
 * @param active_call to get active call or inactive call
 *
 * @return call state of call
 */
int bt_manager_hfp_get_call_state(uint8_t active_call);

/**
 * @brief get active hfp dev handle
 *
 * This routine provides to get active hfp dev handle.
 * @param handle
 *
 */
void bt_manager_hfp_get_active_conn_handle(uint16_t *handle);

/**
 * @brief set bt call indicator by app
 *
 * This routine provides to set bt call indicator by app through HFP profile .
 * @param index  call status index to set (enum btsrv_hfp_hf_ag_indicators)
 * @param value  call status
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_hfp_ag_update_indicator(enum btsrv_hfp_hf_ag_indicators index, uint8_t value);

/**
 * @brief send call event
 *
 * This routine provides to send call event through HFP profile .
 * @param event  call event
 * @param len  event len
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_hfp_ag_send_event(uint8_t *event, uint16_t len);

/**
 * @brief spp reg uuid
 *
 * This routine provides to spp reg uuid, return channel id.
 *
 * @param uuid uuid of spp link
 * @param c call back of spp link
 *
 * @return channel of spp connected
 */
int bt_manager_spp_reg_uuid(uint8_t *uuid, struct btmgr_spp_cb *c);
/**
 * @brief spp send data
 *
 * This routine provides to send data througth target spp link
 *
 * @param chl channel of spp link
 * @param data pointer of send data
 * @param len length of send data
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_spp_send_data(uint8_t chl, uint8_t *data, uint32_t len);
/**
 * @brief spp connect
 *
 * This routine provides to connect spp channel
 *
 * @param uuid uuid of spp connect
 *
 * @return 0 excute successed , others failed
 */
uint8_t bt_manager_spp_connect(bd_address_t *bd, uint8_t *uuid, struct btmgr_spp_cb *spp_cb);

/**
 * @brief spp disconnect
 *
 * This routine provides to disconnect spp channel
 *
 * @param chl channel of spp connect
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_spp_disconnect(uint8_t chl);

/**
 * @brief PBAP get phonebook
 *
 * This routine provides to get phonebook
 *
 * @param bd device bluetooth address
 * @param path phonebook path
 * @param cb callback function
 *
 * @return 0 failed, others success with app_id
 */
uint8_t btmgr_pbap_get_phonebook(bd_address_t *bd, char *path, struct btmgr_pbap_cb *cb);

/**
 * @brief abort phonebook get
 *
 * This routine provides to abort phonebook get
 *
 * @param app_id app_id
 *
 * @return 0 excute successed , others failed
 */
int btmgr_pbap_abort_get(uint8_t app_id);

/**
 * @brief hid send data on intr channel
 *
 * This routine provides to send data throug intr channel
 *
 * @param report_type input or output
 * @param data pointer of send data
 * @param len length of send data
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_hid_send_intr_data(uint8_t report_type, uint8_t *data, uint32_t len);

/**
 * @brief hid send data on ctrl channel
 *
 * This routine provides to send data throug ctrl channel
 *
 * @param report_type input or output
 * @param data pointer of send data
 * @param len length of send data
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_hid_send_ctrl_data(uint8_t report_type, uint8_t *data, uint32_t len);

/**
 * @brief hid send take photo key msg on intr channel
 *
 * This routine provides to hid send take photo key msg on intr channel
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_hid_take_photo(void);

/**
 * @brief bt manager get connected phone device num
 *
 * This routine provides to get connected device num.
 *
 * @return number of connected device
 */
int bt_manager_get_connected_dev_num(void);

/**
 * @brief bt manager clear connected list
 *
 * This routine provides to clear connected list
 * if have bt device connected , disconnect it before clear connected list.
 *
 * @return N/A
 */
void bt_manager_clear_list(int mode);

/**
 * @brief bt manager clear bt info
 *
 * This routine provides to clear btinfo include paired list and volume etc.
 *
 * @return N/A
 */
void bt_manager_clear_bt_info();

/**
 * @brief bt manager set stream type
 *
 * This routine set stream type to bt manager
 *
 * @return N/A
 */
void bt_manager_set_stream_type(uint8_t stream_type);

/**
 * @brief bt manager set codec
 *
 * This routine set codec to tws, only need by set a2dp stream
 *
 * @return N/A
 */
void bt_manager_set_codec(uint8_t codec);

/**
 * @brief Start br disover
 *
 * This routine Start br disover.
 *
 * @param disover parameter
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_br_start_discover(struct btsrv_discover_param *param);

/**
 * @brief Stop br disover
 *
 * This routine Stop br disover.
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_br_stop_discover(void);

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
int bt_manager_br_connect(bd_address_t *bd);

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
int bt_manager_br_disconnect(bd_address_t *bd);

/**
 * @brief disconnect to all br connected device
 *
 * This routine disconnect to all br connected device.
 *
 */
void bt_manager_disconnect_all_device(void);

/**
 * @brief disconnect to all br connected phone device
 *
 * This routine disconnect to all br connected phone device.
 *
 */
void bt_manager_br_disconnect_all_phone_device(void);

/**
 * @brief disconnect another br connected phone device
 *
 * This routine disconnect another br connected phone device.
 *
 */
void bt_manager_disconnect_other_connected_dev(struct bt_conn * conn);

/**
 * @brief bt manager lock stream pool
 *
 * This routine provides to unlock stream pool
 *
 * @return N/A
 */

void bt_manager_stream_pool_lock(void);
/**
 * @brief bt manager unlock stream pool
 *
 * This routine provides to unlock stream pool
 *
 * @return N/A
 */
void bt_manager_stream_pool_unlock(void);
/**
 * @brief bt manager set target type stream enable or disable
 *
 * This routine provides to set target type stream enable or disable
 * @param type type of stream
 * @param enable true :set stream enable false: set steam disable
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_stream_enable(int type, bool enable);
/**
 * @brief bt manager check target type stream enable
 *
 * This routine provides to check target type stream enable
 * @param type type of stream
 *
 * @return ture stream is enable
 * @return false stream is not enable
 */
bool bt_manager_stream_is_enable(int type);
/**
 * @brief bt manager set target type stream
 *
 * This routine provides to set target type stream
 * @param type type of stream
 * @param stream handle of stream
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_set_stream(int type, io_stream_t stream);
/**
 * @brief bt manager get target type stream
 *
 * This routine provides to get target type stream
 * @param type type of stream
 *
 * @return handle of stream
 */
io_stream_t bt_manager_get_stream(int type);

/**
 * @brief tws wait pair
 *
 * This routine provides to make device enter to tws pair modes
 *
 * @return N/A
 */
void bt_manager_tws_pair_search(void);

/**
 * @brief tws get device role
 *
 * This routine provides to get tws device role
 *
 * @return device role of tws
 */
int bt_manager_tws_get_dev_role(void);

/**
 * @brief tws get peer version
 *
 * This routine provides to get tws peer version
 *
 * @return tws peer version
 */
uint8_t bt_manager_tws_get_peer_version(void);

/**
 * @brief Create a new spp ble stream
 *
 * This routine Create a new spp ble stream
 * @param init param for spp ble stream
 *
 * @return  handle of stream
 */
io_stream_t sppble_stream_create(void *param);

int sppble_stream_modify_write_timeout(io_stream_t handle,uint32_t write_timeout);

/**
 * @cond INTERNAL_HIDDEN
 */
/**
 * @brief bt manager state notify
 *
 * This routine provides to notify bt state .
 *
 * @param state  state of bt device.
 *
 * @return 0 excute successed , others failed
 */

int bt_manager_state_notify(int state);

/**
 * @brief bt manager event notify
 *
 * This routine provides to notify bt event .
 *
 * @param event_id  id of bt event
 * @param event_data param of bt event
 * @param event_data_size param size of bt event
 *
 * @return 0 excute successed , others failed
 */

int bt_manager_event_notify(int event_id, void *event_data, int event_data_size);

/**
 * @brief bt manager event notify
 *
 * This routine provides to notify bt event .
 *
 * @param event_id  id of bt event
 * @param event_data param of bt event
 * @param event_data_size param size of bt event
 * @param call_cb param callback function to free mem
 *
 * @return 0 excute successed , others failed
 */

int bt_manager_event_notify_ext(int event_id, void *event_data, int event_data_size , MSG_CALLBAK call_cb);

/**
 * @brief bt manager get tws bt clock
 *
 * This routine get tws bt clock.
 *
 * @return 0xFFFFFFFF faile , others bt clock in ms.
 */
uint32_t bt_manager_tws_get_bt_clock(void);

/**
 * @brief wait tws ready send sync msg
 *
 * This routine wait tws ready send sync msg.
 *
 * @return NONE.
 */
void bt_manager_wait_tws_ready_send_sync_msg(uint32_t wait_timeout);

/**
 * @brief bt manager set tws irq time
 *
 * This routine provides to set bt device state.
 * @param bt_clock_ms, tws irq bt clock time in ms.
 *
 * @return 0: successful, other failed.
 */
int bt_manager_tws_set_irq_time(uint32_t bt_clock_ms);

/**
 * @brief Get bluetooth wake lock
 *
 * This routine get bluetooth wake lock.
 *
 * @return 0: idle can sleep,  other: busy can't sleep.
 */
int bt_manager_get_wake_lock(void);

/**
 * @brief Get bluetooth link idle
 *
 * This routine get bluetooth link idle.
 *
 * @return 0: BT Tx/Rx idle ,  other: BT Tx/Rx busy.
 */
int bt_manager_get_link_idle(void);


/**
 * @brief Bt set apll temperture compensation
 *
 * @param enable Enable or disable apll temperture compensation.
 *
 * @return 0: successful, other failed.
 */
int bt_manager_bt_set_apll_temp_comp(uint8_t enable);

/**
 * @brief Bt do apll temperture compensation
 *
 * @return 0: idle can sleep,  other: busy can't sleep.
 */
int bt_manager_bt_do_apll_temp_comp(void);

btmgr_base_config_t * bt_manager_get_base_config(void);
btmgr_feature_cfg_t * bt_manager_get_feature_config(void);
btmgr_pair_cfg_t * bt_manager_get_pair_config(void);
btmgr_tws_pair_cfg_t * bt_manager_get_tws_pair_config(void);
btmgr_sync_ctrl_cfg_t * bt_manager_get_sync_ctrl_config(void);
btmgr_reconnect_cfg_t * bt_manager_get_reconnect_config(void);
btmgr_ble_cfg_t *bt_manager_ble_config(void);
btmgr_rf_param_cfg_t *bt_manager_rf_param_config(void);

void bt_manager_powon_auto_reconnect(uint8_t reboot_mode);
bool bt_manager_manual_reconnect(void);

uint8_t bt_manager_config_connect_phone_num(void);

void bt_manager_start_wait_connect(void);
void bt_manager_end_wait_connect(void);
void bt_manager_start_pair_mode(void);
void bt_manager_end_pair_mode(void);
void bt_manager_enter_pair_mode(void);
void bt_manager_clear_paired_list(void);
void bt_manager_tws_start_pair_search_ex(u8_t tws_pair_keyword);
bool bt_manager_is_tws_paired_valid(void);
bool bt_manager_is_wait_connect(void);
bool bt_manager_is_tws_pair_search(void);

bool bt_manager_is_pair_mode(void);
bool bt_manager_is_remote_in_pair_mode(void);
void bt_manager_clear_remote_pair_mode(void);
int bt_manager_send_message_to_main(uint8_t type,uint8_t cmd,int value);

void bt_manager_tws_start_pair_search(uint8_t mode);
void bt_manager_tws_end_pair_search(void);
void bt_manager_tws_enter_pair_mode(void);
void bt_manager_tws_sync_volume_to_slave_inner(bd_address_t* bt_addr,uint32_t media_type, uint32_t music_vol, uint32_t from_phone);
void bt_manager_tws_sync_volume_to_slave(bd_address_t* bt_addr,uint32_t media_type, uint32_t music_vol);
int bt_manager_tws_send_command(uint8_t *command, int command_len);
int bt_manager_tws_send_sync_command(uint8_t *command, int command_len);
void bt_manager_tws_send_event(uint8_t event, uint32_t event_param);
void bt_manager_tws_clear_paired_list(void);
void bt_manager_tws_disconnect(void);

void bt_manager_letws_stop_pair_search(void);
bool bt_manager_exist_letws_info(void);


int bt_manager_bt_read_rssi(uint16_t handle);
int bt_manager_bt_read_link_quality(uint16_t handle);
bool bt_manager_a2dp_reconnect_status_get(void);
int bt_manager_link_info_get(uint16_t handle, complete_event_cb cb);
int bt_manager_link_info_set(uint8_t *map);

/* pts test */
#ifdef CONFIG_BT_LEA_PTS_TEST
int pts_le_audio_init(void);
void pts_le_clear_keys(void);
#endif /*CONFIG_BT_LEA_PTS_TEST*/
#ifdef CONFIG_BT_PTS_TEST
void bt_manager_pts_test_init(void);
void bt_manager_pts_test_deinit(void);
#endif /*CONFIG_BT_PTS_TEST*/
bool bt_manager_config_pts_test(void);
bool bt_manager_config_lea_pts_test(void);

#ifdef CONFIG_BT_CONTROLER_BQB
bool bt_manager_get_bqb_mode(void);
#endif

typedef int (*tws_config_expect_role_cb_t)(void);
/**
 * @brief
 *
 *
 *
 *
 * @param cb  :
 *
 * @return BTSRV_TWS_MASTER:Expect as tws master;BTSRV_TWS_SLAVE:Expect as tws slave;BTSRV_TWS_NONE:Decide tws role by bt service
 */
int bt_manager_tws_register_config_expect_role_cb(tws_config_expect_role_cb_t cb);



typedef int (*tws_long_message_cb_t)(uint8_t app_event, char* param, uint32_t param_len);
/**
 * @brief bt manager tws register long message callback
 *
 * This routine provides app register callback to btmanager.
 * When btmanager recieve 'TWS_LONG_MSG_EVENT', call applicant "cb function"
 *
 * @param cb  :  see tws_long_message_cb_t.
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_tws_register_long_message_cb(tws_long_message_cb_t cb);

/**
 * @brief bt manager tws send async message
 *
 * This routine provides app send async tws message to master/slave .
 *
 * @param event_type  :  TWS_USER_APP_EVENT or TWS_LONG_MSG_EVENT.
 * @param app_event :  app event by applicant define
 * @param param :  real message data
 * @param param_len :  real message data len (TWS_USER_APP_EVENT ����С�ڵ��� 32 �ֽ�)
 *
 * @return 0 excute successed , others failed
 */
int bt_manager_tws_send_message(uint8_t event_type, uint8_t app_event, uint8_t* param,uint32_t param_len);

/**
 * @brief bt manager tws send sync message
 *
 * This routine provides app send sync tws message to master/slave .
 *
 * @param event_type :  TWS_USER_APP_EVENT.
 * @param event  :  app event by applicant define
 * @param param :  real message data, see tws_msg_content_t
 * @param param_len :  real message data len (����С�ڵ��� 32 �ֽ�)
 * @param sync_wait :  ��ʾ�ȴ�ͬ������ʱ��� (����),���Ϊ�㣬��Ĭ��Ϊ100����
 *
 * @return 0 excute successed , others failed
 */

int bt_manager_tws_send_sync_message(uint8_t event_type, uint8_t app_event, void* param, uint32_t param_len, uint32_t sync_wait);

void bt_manager_tws_notify_start_play(uint8_t media_type, uint8_t codec_id, uint8_t sample_rate);
void bt_manager_tws_notify_stop_play(void);
void bt_manager_sco_set_codec_info(uint16_t handle,uint8_t codec_id, uint8_t sample_rate);
tws_runtime_observer_t *bt_manager_tws_get_runtime_observer(void);
void bt_manager_tws_send_event_sync(uint8_t event, uint32_t event_param);
int bt_manager_allow_sco_connect(bool allowed);
void bt_manager_halt_phone(void);
void bt_manager_resume_phone(void);
uint8_t bt_manager_get_halt_phone_num(void);
void bt_manager_ios_tts_delay_set(bool enable);
void bt_manager_set_user_visual(bool enable,bool discoverable, bool connectable, uint8_t scan_mode);
bool bt_manager_is_poweron_auto_reconnect_running(void);
bool bt_manager_is_auto_reconnect_runing(void);
void bt_manager_auto_reconnect_stop(void);
int bt_manager_get_linkkey(struct bt_linkkey_info *info, uint8_t cnt);
int bt_manager_update_linkkey(struct bt_linkkey_info *info, uint8_t cnt);
int bt_manager_write_ori_linkkey(bd_address_t *addr, uint8_t *link_key);

bool bt_manager_tws_check_is_woodpecker(void);
bool bt_manager_tws_check_support_feature(uint32_t feature);
void bt_manager_tws_set_stream_type(uint8_t stream_type);
void bt_manager_tws_set_codec(uint8_t codec);
uint8_t bt_manager_tws_get_state(void);
void bt_manager_tws_set_state(uint8_t state);
void bt_manager_tws_channel_mode_switch(void);
void btmgr_tws_switch_snoop_link(void);

typedef void(*tws_timer_cb_t)(void* param, uint32_t bt_clock);
int tws_timer_add(uint32_t bt_clock, tws_timer_cb_t timer_cb , void* timer_param );


struct tws_snoop_switch_cb_func {
	uint8_t id;
	void (*start_cb)(uint8_t id);
	int (*check_ready_cb)(uint8_t id);		/* return 1 Ready for snoop switch, 0 Not ready for snoop switch */
	void (*finish_cb)(uint8_t id, uint8_t is_master);
	void (*sync_info_cb)(uint8_t id, uint8_t *data, uint16_t len);
};

/**
 * @brief bt manager register snoop switch callback function.
 *
 * This routine register/unregister snoop switch callback function.
 *
 * @param cb  :  callback function struct.
 * @param reg : true register, false unregister
 *
 * @return 0 Success, other Failed.
 */
int bt_manager_tws_reg_snoop_switch_cb_func(struct tws_snoop_switch_cb_func *cb, bool reg);

/**
 * @brief bt manager app send snoop switch sync info.
 *
 * This routine for app send snoop switch sync info.
 *
 * @param data  Data, data[0] must id in tws_snoop_switch_cb_func.
 * @param len Data length.
 *
 * @return 0 Success, other Failed.
 */
int bt_manager_tws_send_snoop_switch_sync_info(uint8_t *data, uint16_t len);

uint8_t btmgr_map_client_connect(bd_address_t *bd, char *path, struct btmgr_map_cb *cb);
uint8_t btmgr_map_client_set_folder(uint8_t app_id,char *path, uint8_t flags);
int btmgr_map_get_folder_listing(uint8_t app_id);
int btmgr_map_get_messages_listing(uint8_t app_id,uint16_t max_cn,uint32_t mask);
int btmgr_map_abort_get(uint8_t app_id);
int btmgr_map_client_disconnect(uint8_t app_id);
/**
 * INTERNAL_HIDDEN @endcond
 */
/**
 * @brief Bt set apll temperture compensation
 *
 * @param enable Enable or disable apll temperture compensation.
 *
 * @return 0: successful, other failed.
 */
int bt_manager_bt_set_apll_temp_comp(u8_t enable);

/**
 * @brief Bt do apll temperture compensation
 *
 * @return 0: idle can sleep,  other: busy can't sleep.
 */
int bt_manager_bt_do_apll_temp_comp(void);


/** @brief Bt read build version of bt controller lib
 *
 * *@param str_version input buffer for version string.
 *  @param len input buffer len.
 *
 *  @return 0: successful, other failed.
 */
int bt_manager_bt_read_bt_build_version(uint8_t* str_version, int len);

/**
 * @brief tws get device role
 *
 * This routine provides to get tws device role
 *
 * If the TWS is not connected, the previous role will be returned
 * and exist_ reconn_ Phone indicates whether the phone has been connected previously
 * @return device role of tws
 */
int bt_manager_tws_get_dev_role_ext(uint8 *exist_reconn_phone);

/**
 * @brief
 *
 * @param enable
 */
void bt_manager_tws_sync_enable_lea_dir_adv(bool enable);

/**
 * @brief
 *
 *  Identify le aduio as active, and set snoop as inactive
 *
 * @param leaudio_active
 */
void bt_manager_tws_set_leaudio_active(int8_t leaudio_active);

/**
 * @brief
 *
 *  update device name
 *
 * @param name: device name
 * @param len: device name len
 */
int bt_manager_bt_name_update(uint8_t *name, uint8_t len);

/**
 * @brief
 *
 *  get device name crc32 value
 *
 * @param device name
 */
uint32_t bt_manager_get_bt_name_crc_value(void);

/**
 * @brief
 *
 *  set aes ccm work mode 0:async mode 1:sync mode
 *
 * @param device name
 */
void bt_manager_set_aesccm_mode(uint8_t mode);

void bt_manager_app_name_update(void);

void bt_manager_gfp_personalized_name_update(bool valid);
void bt_manager_update_phone_volume(uint16_t hdl,int a2dp);
#endif

