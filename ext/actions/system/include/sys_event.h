/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file system event interface
 */

#ifndef _SYS_EVENT_H_
#define _SYS_EVENT_H_
#include <os_common_api.h>
//#include <thread_timer.h>

/**
 * @defgroup sys_event_apis App system event Manager APIs
 * @ingroup system_apis
 * @{
 */

/** sys event type */
enum sys_event_e
{
    SYS_EVENT_NONE = 0,

    SYS_EVENT_POWER_ON,
    SYS_EVENT_POWER_OFF,
    SYS_EVENT_STANDBY,
    SYS_EVENT_WAKE_UP,

    SYS_EVENT_BATTERY_LOW,
    SYS_EVENT_BATTERY_TOO_LOW,

    SYS_EVENT_CHARGE_START,
    SYS_EVENT_CHARGE_FULL,

    SYS_EVENT_ENTER_PAIR_MODE,
    SYS_EVENT_CLEAR_PAIRED_LIST,
	SYS_EVENT_FACTORY_DEFAULT,

    SYS_EVENT_BT_WAIT_PAIR,
    SYS_EVENT_BT_CONNECTED,
    SYS_EVENT_2ND_CONNECTED,
    SYS_EVENT_BT_DISCONNECTED,
    SYS_EVENT_BT_UNLINKED,

    SYS_EVENT_TWS_START_PAIR,
    SYS_EVENT_TWS_UNPAIR,
    SYS_EVENT_TWS_CONNECTED,
    SYS_EVENT_TWS_DISCONNECTED,
	SYS_EVENT_TWS_PAIR_FAILED,

    SYS_EVENT_MIN_VOLUME,
    SYS_EVENT_MAX_VOLUME,
    SYS_EVENT_PLAY_START,
    SYS_EVENT_PLAY_PAUSE,
	SYS_EVENT_PLAY_PREVIOUS,
	SYS_EVENT_PLAY_NEXT,

    SYS_EVENT_BT_START_CALL,
    SYS_EVENT_BT_CALL_OUTGOING,
    SYS_EVENT_BT_CALL_INCOMING,
    SYS_EVENT_BT_CALL_3WAYIN,
    SYS_EVENT_BT_CALL_ONGOING,
    SYS_EVENT_BT_CALL_END,
    SYS_EVENT_SWITCH_CALL_OUT,
    SYS_EVENT_MIC_MUTE_ON,
    SYS_EVENT_MIC_MUTE_OFF,

    SYS_EVENT_SIRI_START,
    SYS_EVENT_SIRI_STOP,

	SYS_EVENT_ENTER_BTMUSIC,
    SYS_EVENT_ENTER_LINEIN,
    SYS_EVENT_ENTER_USOUND,
    SYS_EVENT_ENTER_SPDIF_IN,
	SYS_EVENT_ENTER_I2SRX_IN,
    SYS_EVENT_ENTER_SDMPLAYER,
    SYS_EVENT_ENTER_UMPLAYER,
    SYS_EVENT_ENTER_NORMPLAYER,
    SYS_EVENT_ENTER_SDPLAYBACK,
    SYS_EVENT_ENTER_UPLAYBACK,
    SYS_EVENT_ENTER_RECORD,
    SYS_EVENT_ENTER_MIC_IN,
	SYS_EVENT_ENTER_FM,
    SYS_EVENT_ENTER_AUXTWS,
    SYS_EVENT_ENTER_LEARELAY,
    SYS_EVENT_ENTER_LEAUDIO,
    SYS_EVENT_ENTER_LEMUSIC,
    SYS_EVENT_ENTER_OTA,
    SYS_EVENT_ENTER_DEMO,

    SYS_EVENT_PLAY_ENTER_AURACAST_TTS,
    SYS_EVENT_PLAY_EXIT_AURACAST_TTS,

	SYS_EVENT_OTA_FINISHED_REBOOT,
	SYS_EVENT_STEREO_GROUP_INDICATION,
	SYS_EVENT_STEREO_GROUP_INDICATION_NO_FILTER,
    SYS_EVENT_OTA_REQ_START,

    SYS_EVENT_ENTER_LCMUSIC_DEMO,

	SYS_EVENT_REQ_FLUSH_PROPERTY,
	SYS_EVENT_REQ_NVRAM_DIRECT_WRITE,
};

/** sys event ui map structure */
struct sys_event_ui_map {
	/** sys event id */
    u8_t  sys_event;
	/** ui event id */
    u8_t  ui_event;
	/** ui event type */
    u8_t  stable_event;
};

/**
 * @brief report inut event
 *
 * @details report system event to system app.
 *
 * @param key_event input event
 *
 * @return N/A
 */
void sys_event_report_input(u32_t key_event);
/**
 * @brief system event notify
 *
 * @details send system event to system app, if tws
 * master mode send system event to slave too.
 *
 * @param event event id want to send
 *
 * @return N/A
 */
void sys_event_notify(u32_t event);

/**
 * @brief system event notify
 *
 * @details send system event to system app, but wouldn't send to slave.
 *
 * @param event event id want to send
 *
 * @return N/A
 */
void sys_event_notify_single(uint32_t event);

/**
 * @brief process system event
 *
 * @details this routine Find UI event through sys event ID according to
 *  sys event UI map table and send UI event  .
 *
 * @param event system event id
 *
 * @return N/A
 */
void sys_event_process(u32_t event);

/**
 * @brief register a system event map
 *
 * @details register system event map, user can define system event an ui event map
 *  when device sent system event ,may auto trigge ui event which mapped by event map
 *
 * @param event_map user define event map table
 * @param size event map table size
 * @param sys_view_id  system view id
 *
 * @return N/A
 */

void sys_event_map_register(const struct sys_event_ui_map *event_map, int size, int sys_view_id);

/**
 * @brief send system message
 *
 * @details send system message to system app, if tws
 * master mode send system message to slave too.
 *
 * @param message message id want to send
 *
 * @return N/A
 */
void sys_event_send_message_new(uint32_t message, uint8_t cmd, void* extra_data, int extra_data_size);

/**
 * @brief send system message
 *
 * @details send system message to system app, if tws
 * master mode send system message to slave too.
 *
 * @param message message id want to send
 *
 * @return N/A
 */
#define sys_event_send_message(message) sys_event_send_message_new(message, 0, NULL, 0)

/**
 * @} end defgroup sys_event_apis
 */
#endif

