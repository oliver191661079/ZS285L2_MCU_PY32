/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt music
 */

#ifndef _BT_MUSIC_APP_H_
#define _BT_MUSIC_APP_H_

#ifdef CONFIG_SYS_LOG
#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "btmusic"
#include <logging/sys_log.h>
#endif

#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <bt_manager.h>
#include <stream.h>
#include <app_manager.h>
#include <srv_manager.h>
#include <mem_manager.h>
#include <msg_manager.h>
#include <volume_manager.h>
#include <media_player.h>
#include <audio_system.h>
#include <audio_policy.h>
#include <global_mem.h>
#include <thread_timer.h>
#include "btservice_api.h"
#include "app_defines.h"
#include "sys_manager.h"
#include "app_ui.h"
#include "broadcast.h"
#include "desktop_manager.h"
#include <app_launch.h>
#include <app_tws.h>

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

/* 开启频段能量采样与律动调试（依赖 prj 中 CONFIG_ENERGY_SAMPLE_SUPPORT 等） */
#define CONFIG_BT_MUSIC_FREQPOINT_ENERGY_DEMO

/*
 * RGB 音乐律动调试开关（需 CONFIG_BT_MUSIC_FREQPOINT_ENERGY_DEMO 且 CONFIG_BT_MUSIC_LED_RHYTHM）：
 *   1 - 将频段能量平滑/归一化到 0~255，打印 bass/mid/high 与 R/G/B
 *   0 - 原厂 demo：以十六进制打印各频段原始能量
 */
#if defined(CONFIG_BT_MUSIC_LED_RHYTHM)
#define CONFIG_BT_MUSIC_RGB_RHYTHM_DEBUG 1
#else
#define CONFIG_BT_MUSIC_RGB_RHYTHM_DEBUG 0
#endif

/*
 * PCM dump 会挂接解码旁路，导致 malloc 失败/播放异常/无声。正常播放必须保持 0。
 */
#define CONFIG_BT_MUSIC_RGB_PCM_DUMP 0

/*
 * 最后回退：A2DP sink 缓冲长度（非音乐频谱，仅蓝牙数据量）。
 * src=2 时律动与节拍无关，应优先让 src=0（DSP）或 src=1（PCM）生效。
 */
#define CONFIG_BT_MUSIC_RGB_STREAM_FALLBACK 0

#if CONFIG_BT_MUSIC_RGB_PCM_DUMP || CONFIG_BT_MUSIC_RGB_STREAM_FALLBACK
#define CONFIG_BT_MUSIC_RGB_PCM_FALLBACK 1
#else
#define CONFIG_BT_MUSIC_RGB_PCM_FALLBACK 0
#endif

#define BTMUSIC_BMS_TRANSMIT_INDEX        4

#define BT_MUSIC_KEY_FLAG     (0x0 << 6)
#define BT_CALL_KEY_FLAG      (0x1 << 6)

struct btmusic_app_t {
	struct thread_timer resume_timer;
	struct thread_timer energy_timer;
	struct thread_timer play_timer;
#ifdef CONFIG_BTMUSIC_BMS_APP
	struct thread_timer broadcast_start_timer;
#ifdef CONFIG_BT_MUSIC_AUTO_SWITCH_BMR
	struct thread_timer broadcast_switch_timer;
#endif
#endif

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	uint8_t set_dvfs_level;
	uint8_t bms_dvfs;
#endif

	/*
	 * playback: BR -> DSP -> DAC/I2S_OUT
	 */
	media_player_t *playback_player;
	io_stream_t sink_stream;	/* input_playback */
	struct bt_audio_chan sink_chan;

	uint8_t playing:1;
	uint8_t media_opened:1;
	uint8_t restart:1;
	uint8_t media_ctrl_connected:1;
	uint8_t playback_player_run:1;
	uint8_t capture_player_run:1;
	uint8_t tx_start:1;
	uint8_t tx_sync_start:1;

	uint8_t broadcast_source_enabled:1;
	uint8_t encryption:1;
	uint8_t tts_playing:1;
	uint8_t dsp_run:1;
	uint8_t sbc_playing : 1;
	uint8_t media_state : 1;
	uint8_t bms_source : 1;
	uint8_t auracast_pending: 1;

	uint8_t ios_dev : 1;

#ifdef CONFIG_BTMUSIC_BMS_APP
	uint8_t bms_transmit_num_index;
	uint8_t broadcast_level_hold_time;

	uint16_t pcm_time;
	uint16_t bis_delay;

	/*
	 * capture of BIS: BR -> DSP -> BIS
	 */
	media_player_t *capture_player;
	io_stream_t stream[NUM_OF_BROAD_CHAN];
	io_stream_t input_stream;
	struct bt_broadcast_chan broad_chan[NUM_OF_BROAD_CHAN];
	struct bt_broadcast_chan *chan;
	uint8_t num_of_broad_chan;
	int32_t broadcast_dev_handle;

	uint8_t broadcast_code[16];
	struct bt_broadcast_qos *qos;
	uint8_t irc;
#endif
};

enum {
	/* bt play key message */
	MSG_BT_PLAY_PAUSE_RESUME = MSG_APP_INPUT_MESSAGE_CMD_START,

	MSG_BT_PLAY_NEXT,

	MSG_BT_PLAY_PREVIOUS,

	MSG_BT_PLAY_VOLUP,

	MSG_BT_PLAY_VOLDOWN,

	MSG_BT_PLAY_SEEKFW_START,

	MSG_BT_PLAY_SEEKBW_START,

	MSG_BT_PLAY_SEEKFW_STOP,

	MSG_BT_PLAY_SEEKBW_STOP,

	MSG_SWITCH_BROADCAST,
};

enum {
	MSG_BTMUSIC_MESSAGE_CMD_PLAYER_RESET,
};

enum BT_MUSIC_KEY_FUN {
	BT_MUSIC_KEY_NULL,
	BT_MUSIC_KEY_PLAY_PAUSE,
	BT_MUSIC_KEY_PREVMUSIC,
	BT_MUSIC_KEY_NEXTMUSIC,
	BT_MUSIC_KEY_FFSTART,
	BT_MUSIC_KEY_FFEND,
	BT_MUSIC_KEY_FBSTART,
	BT_MUSIC_KEY_FBEND,
	BT_MUSIC_KEY_VOLUP,
	BT_MUSIC_KEY_VOLDOWN,
};

struct btmusic_app_t *btmusic_get_app(void);

#ifdef CONFIG_BT_MUSIC_FREQPOINT_ENERGY_DEMO
/* 向 DSP/DAE 注册 10 段频带能量输出（播放 init 时调用一次） */
void btmusic_output_energy_sample_config(media_player_t *player);
/* 能量数据来源：0=DSP，1=PCM 分段统计，2=A2DP 缓冲长度（后两者需对应宏开启） */
extern uint8_t btmusic_rgb_energy_src;
#if CONFIG_BT_MUSIC_RGB_PCM_DUMP
void btmusic_rgb_pcm_dump_start(media_player_t *player);
void btmusic_rgb_pcm_dump_stop(void);
#endif
/* 读取频段能量（含多级回退），供律动定时器与 LED 映射使用 */
int btmusic_a2dp_get_freqpoint_energy(short * info, int size);
#endif

int btmusic_get_auracast_mode(void);

void btmusic_set_auracast_mode(int mode);

void btmusic_bt_event_proc(struct app_msg *msg);

void btmusic_tts_event_proc(struct app_msg *msg);

void btmusic_input_event_proc(struct app_msg *msg);

void btmusic_app_event_proc(struct app_msg *msg);

void btmusic_esd_restore(void);

void btmusic_delay_resume(struct thread_timer *ttimer, void *expiry_fn_arg);

void btmusic_view_init(void);

void btmusic_view_deinit(void);

void btmusic_view_show_connected(void);

void btmusic_view_show_disconnected(void);

void btmusic_view_show_play_paused(bool playing);

void btmusic_view_show_tws_connect(bool connect);

void btmusic_view_show_id3_info(struct bt_media_id3_info *pinfo);

void btmusic_tts_delay_resume(struct thread_timer *ttimer,
			       void *expiry_fn_arg);

int btmusic_init_playback(void);

int btmusic_start_playback(void);

int btmusic_stop_playback(void);

int btmusic_exit_playback(void);

int btmusic_bms_init_capture(void);

int btmusic_bms_start_capture(void);

int btmusic_bms_stop_capture(void);

int btmusic_bms_exit_capture(void);

void btmusic_player_reset_trigger(void);

int btmusic_bms_source_init(void);

int btmusic_bms_source_exit(void);

#endif				/* _BT_MUSIC_APP_H */
