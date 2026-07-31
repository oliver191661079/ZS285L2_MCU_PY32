/*
 * Copyright (c) 2024 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief usound header file.
 */

#ifndef _USOUND_APP_H
#define _USOUND_APP_H

#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "usound"

#include <logging/sys_log.h>
#include <mem_manager.h>
#include <app_manager.h>
#include <srv_manager.h>
#include <volume_manager.h>
#include <msg_manager.h>
#include <thread_timer.h>
#include <media_player.h>
#include <audio_system.h>
#include <audio_policy.h>
#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <bt_manager.h>
#include <global_mem.h>
#include <stream.h>
#include <usb_audio_hal.h>

#include <soc_dvfs.h>
#include <thread_timer.h>

#include "app_defines.h"
#include "sys_manager.h"
#include "app_ui.h"
#include "desktop_manager.h"
#include "broadcast.h"
#include <app_tws.h>

#ifdef CONFIG_PLAYTTS
#include "tts_manager.h"
#endif

#define CONFIG_USOUND_FEATURE_RESTART

#if (0 == CONFIG_DSP_COMPUTE_COMPLEXITY)
#define BCST_FREQ                SOC_DVFS_LEVEL_HIGH_PERFORMANCE
#define BCST_FREQ_HIGH           SOC_DVFS_LEVEL_BR_FULL_PERFORMANCE
#define BR_FREQ                  SOC_DVFS_LEVEL_ALL_PERFORMANCE
#elif (1 == CONFIG_DSP_COMPUTE_COMPLEXITY)
#define BCST_FREQ                SOC_DVFS_LEVEL_HIGH_PERFORMANCE
#define BCST_FREQ_HIGH           SOC_DVFS_LEVEL_BR_FULL_PERFORMANCE
#define BR_FREQ                  SOC_DVFS_LEVEL_ALL_PERFORMANCE
#elif (2 <= CONFIG_DSP_COMPUTE_COMPLEXITY)
#define BCST_FREQ                SOC_DVFS_LEVEL_FULL_PERFORMANCE
#define BCST_FREQ_HIGH           SOC_DVFS_LEVEL_FULL_PERFORMANCE
#define BR_FREQ                  SOC_DVFS_LEVEL_BR_FULL_PERFORMANCE
#endif

enum {
	MSG_USOUND_PLAY_PAUSE_RESUME = MSG_APP_INPUT_MESSAGE_CMD_START,
	MSG_USOUND_PLAY_VOLUP,
	MSG_USOUND_PLAY_VOLDOWN,
	MSG_USOUND_PLAY_NEXT,
	MSG_USOUND_PLAY_PREV,
	MSG_SWITCH_BROADCAST,
};

enum {
	MSG_USOUND_CMD_NULL = 0,
	MSG_USOUND_STREAM_START = 1,
	MSG_USOUND_STREAM_STOP,
	MSG_USOUND_STREAM_VOLUME,
	MSG_USOUND_STREAM_MUTE,
	MSG_USOUND_STREAM_UNMUTE,
	MSG_USOUND_VOL_UPDATE,
	MSG_USOUND_STREAM_RESTART,
	MSG_USOUND_UPLOAD_STREAM_START,
	MSG_USOUND_UPLOAD_STREAM_STOP,
	MSG_USOUND_UPLOAD_STREAM_VOLUME,
	MSG_USOUND_UPLOAD_STREAM_MUTE,
	MSG_USOUND_UPLOAD_STREAM_UNMUTE,
};

enum USOUND_PLAY_STATUS {
	USOUND_STATUS_NULL = 0x0000,
	USOUND_STATUS_PLAYING = 0x0001,
	USOUND_STATUS_PAUSED = 0x0002,
};

enum USOUND_VOLUME_REQ_TYPE {
	USOUND_VOLUME_NONE = 0x0000,
	USOUND_VOLUME_DEC = 0x0001,
	USOUND_VOLUME_INC = 0x0002,
};

#define USOUND_STATUS_ALL  (USOUND_STATUS_PLAYING | USOUND_STATUS_PAUSED)

struct usound_app_t {
	struct thread_timer monitor_timer;
#ifdef CONFIG_USOUND_FEATURE_RESTART
	struct thread_timer restart_timer;
#endif
#ifdef CONFIG_BMS_UAC_APP
	struct thread_timer broadcast_start_timer;
#endif

	media_player_t *playback_player;
#ifdef CONFIG_BMS_UAC_APP
	media_player_t *capture_player;
#endif

	u32_t playing:1;
	u32_t playback_player_run:1;
	u32_t tts_playing:1;
#ifdef CONFIG_USOUND_MIC
	u32_t mic_record:1;
#endif
	u32_t volume_req_type:2;
	u32_t volume_req_level:8;
	u32_t current_volume_level:8;
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	u32_t set_dvfs_level:8;
	u32_t bms_dvfs:8;
#endif
	io_stream_t usound_stream;
#ifdef CONFIG_USOUND_MIC
	io_stream_t usound_upload_stream;
	int mic_gain;
	void *record_handle;
	media_player_t *upload_capture_player;
#endif

#ifdef CONFIG_USOUND_FEATURE_RESTART
	u32_t restart:1;
	u32_t restart_count;
#endif

#ifdef CONFIG_BMS_UAC_APP
	u32_t capture_player_load:1;
	u32_t capture_player_run:1;
	u32_t tx_start:1;
	u32_t tx_sync_start:1;
	u32_t broadcast_source_enabled:1;
	u32_t encryption:1;
	u32_t bms_source : 1;
	u32_t auracast_pending : 1;

	int32_t broadcast_dev_handle;

	io_stream_t stream[NUM_OF_BROAD_CHAN];
	io_stream_t input_stream;
	struct bt_broadcast_chan broad_chan[NUM_OF_BROAD_CHAN];
	struct bt_broadcast_chan *chan;
	u8_t num_of_broad_chan;

	u8_t broadcast_code[16];
	struct bt_broadcast_qos *qos;
	u8_t irc;
#endif
};
void usound_view_init(void);
void usound_view_deinit(void);
struct usound_app_t *usound_get_app(void);
void usound_start_playback(void);
void usound_stop_playback(void);

void usound_start_capture(void);
void usound_stop_capture(void);

void usound_input_event_proc(struct app_msg *msg);
void usound_tts_event_proc(struct app_msg *msg);
void usound_app_event_proc(struct app_msg *msg);

u32_t usound_get_audio_stream_type(char *app_name);
void usound_show_play_status(bool status);
void usound_view_clear_screen(void);
void usound_view_volume_show(int volume_value);

void bms_uac_bt_event_proc(struct app_msg *msg);

int bms_uac_init_capture(void);
int bms_uac_start_capture(void);
int bms_uac_stop_capture(void);
int bms_uac_exit_capture(void);

void bms_uac_show_play_status(bool status);
void bms_uac_player_reset_trigger(void);

bool usound_is_bms_mode(void);
void usound_set_auracast_mode(bool mode);

int bms_uac_source_init(void);
int bms_uac_source_exit(void);
#endif				/* _USOUND_APP_H */
