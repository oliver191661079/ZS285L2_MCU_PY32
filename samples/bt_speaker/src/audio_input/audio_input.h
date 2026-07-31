/*
 * Copyright (c) 2024 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief audio input app header file
 */

#ifndef _AUDIO_INPUT_APP_H
#define _AUDIO_INPUT_APP_H

#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "audio_input"
#ifdef SYS_LOG_LEVEL
#undef SYS_LOG_LEVEL
#endif

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

// #include <dvfs.h>
#include <thread_timer.h>
#include "btservice_api.h"

#include "app_defines.h"
#include "sys_manager.h"
#include "app_ui.h"
#include "broadcast.h"
#include "desktop_manager.h"

#ifdef CONFIG_PLAYTTS
#include "tts_manager.h"
#endif
#include <ui_manager.h>
#include <app_launch.h>
#include <app_tws.h>

#define CONFIG_AUDIO_INPUT_RESTART
//#define CONFIG_AUDIO_INPUT_ENERGY_DETECT_DEMO

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
	/* audio_input key message */
	MSG_AUDIO_INPUT_NO_ACT = MSG_APP_INPUT_MESSAGE_CMD_START,
	MSG_AUDIO_INPUT_PLAY_PAUSE_RESUME,
	MSG_AUDIO_INPUT_PLAY_VOLUP,
	MSG_AUDIO_INPUT_PLAY_VOLDOWN,
	MSG_SWITCH_BROADCAST,
};

enum AUDIO_INPUT_PLAY_STATUS {
	AUDIO_INPUT_STATUS_NULL = 0x0000,
	AUDIO_INPUT_STATUS_PLAYING = 0x0001,
	AUDIO_INPUT_STATUS_PAUSED = 0x0002,
};
#define AUDIO_INPUT_STATUS_ALL  (AUDIO_INPUT_STATUS_PLAYING | AUDIO_INPUT_STATUS_PAUSED)

enum AUDIO_INPUT_SDR_EVENT {
	SRD_SAMPLE_RATE_CHANGE = 1,
	SRD_TIME_OUT = 2,
};

enum {
	MSG_AUDIO_INPUT_SAMPLE_RATE_CHANGE,
	MSG_APP_PLAYER_RESET,
};

struct audio_input_app_t {
	struct thread_timer resume_timer;
	struct thread_timer broadcast_start_timer;
#ifdef CONFIG_AUDIO_INPUT_RESTART
	struct thread_timer restart_timer;
#endif

	media_player_t *playback_player;
#ifdef CONFIG_AUDIO_INPUT_BMS_APP
	media_player_t *capture_player;
#endif

#ifdef CONFIG_AUDIO_INPUT_RESTART
	u32_t restart_count;
	u32_t restart:1;
#endif
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	u8_t set_dvfs_level;
	u8_t bms_dvfs;
#endif
	u8_t plugin_id;
	u32_t playing:1;
	u32_t playback_player_load:1;
	u32_t playback_player_run:1;
	u32_t tts_playing:1;
	u32_t ain_ready:1;
	u32_t srd_sample_rate:8;
	u32_t srd_event:2;	/*1--sample rate event,2--timeout */

	void *ain_handle;
	os_delayed_work srd_cb_work;

	io_stream_t audio_input_stream;

#ifdef CONFIG_AUDIO_INPUT_BMS_APP
	u32_t tx_start:1;
	u32_t tx_sync_start:1;
	u32_t broadcast_source_enabled:1;
	u32_t encryption:1;
	u32_t bms_source : 1;
	u32_t auracast_pending : 1;
	io_stream_t stream[NUM_OF_BROAD_CHAN];
	io_stream_t input_stream;

	struct bt_broadcast_chan broad_chan[NUM_OF_BROAD_CHAN];
	struct bt_broadcast_chan *chan;
	uint8_t num_of_broad_chan;

	uint8_t broadcast_code[16];
	struct bt_broadcast_qos *qos;

	u32_t capture_player_load:1;
	u32_t capture_player_run:1;
	
	int32_t broadcast_dev_handle;
	uint8_t irc;
#endif

#ifdef CONFIG_AUDIO_INPUT_ENERGY_DETECT_DEMO
	struct thread_timer energy_detect_timer;
#endif
};

void audio_input_view_init(void);
void audio_input_view_deinit(void);
struct audio_input_app_t *audio_input_get_app(void);
int audio_input_get_status(void);
void audio_input_input_event_proc(struct app_msg *msg);
void audio_input_app_event_proc(struct app_msg *msg);
void audio_input_tts_event_proc(struct app_msg *msg);
u32_t audio_input_get_audio_stream_type(void);
void audio_input_bt_event_proc(struct app_msg *msg);
void audio_input_store_play_state(void);
void audio_input_show_play_status(bool status);
void audio_input_view_clear_screen(void);
void audio_input_delay_resume(struct thread_timer *ttimer, void *expiry_fn_arg);
int audio_input_init_playback(void);
int audio_input_start_playback(void);
int audio_input_stop_playback(void);
int audio_input_exit_playback(void);
void audio_input_play_pause(void);
void audio_input_play_resume(void);

#ifdef CONFIG_AUDIO_INPUT_RESTART
void audio_input_player_reset_trigger(void);
#endif

#ifdef CONFIG_AUDIO_INPUT_BMS_APP
bool audio_input_is_bms_mode(void);
void audio_input_set_auracast_mode(bool mode);

int audio_input_init_capture(void);
int audio_input_start_capture(void);
int audio_input_stop_capture(void);
int audio_input_exit_capture(void);

int audio_input_bms_source_init(void);
int audio_input_bms_source_exit(void);
#endif

#endif
