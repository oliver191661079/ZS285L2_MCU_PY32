/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _LEMUSIC_APP_H
#define _LEMUSIC_APP_H

#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "lemusic"

#include <logging/sys_log.h>
#include <mem_manager.h>
#include <app_manager.h>
#include <srv_manager.h>
#include <volume_manager.h>
#include <msg_manager.h>
#include <tts_manager.h>
#include <ui_manager.h>
#include <input_manager.h>
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
#include <sys_manager.h>
#include <buffer_stream.h>
#include <ringbuff_stream.h>
#include <media_mem.h>
#include <audio_record.h>
#include <audio_track.h>

#include <soc_dvfs.h>
#include <thread_timer.h>

#include "app_defines.h"
#include "app_ui.h"
#include "desktop_manager.h"
#include "btservice_api.h"
#include "broadcast.h"
#include <app_launch.h>

#ifdef CONFIG_PROPERTY
#include <property_manager.h>
#endif
#ifdef CONFIG_BT_SELF_APP
#include "selfapp_api.h"
#endif

#define NUM_OF_SINK_CHAN	2

#define BIS_DELAY_BY_RX_START

#if (0 == CONFIG_DSP_COMPUTE_COMPLEXITY)
#define LE_BCST_FREQ             SOC_DVFS_LEVEL_HIGH_PERFORMANCE
#define LE_BCST_FREQ_HIGH        SOC_DVFS_LEVEL_BR_FULL_PERFORMANCE
#define LE_FREQ                  SOC_DVFS_LEVEL_ALL_PERFORMANCE
#elif (1 == CONFIG_DSP_COMPUTE_COMPLEXITY)
#define LE_BCST_FREQ             SOC_DVFS_LEVEL_HIGH_PERFORMANCE
#define LE_BCST_FREQ_HIGH        SOC_DVFS_LEVEL_BR_FULL_PERFORMANCE
#define LE_FREQ                  SOC_DVFS_LEVEL_LE_AUDIO_SLAVE_PERFORMANCE
#elif (2 <= CONFIG_DSP_COMPUTE_COMPLEXITY)
#define LE_BCST_FREQ             SOC_DVFS_LEVEL_FULL_PERFORMANCE
#define LE_BCST_FREQ_HIGH        SOC_DVFS_LEVEL_FULL_PERFORMANCE
#define LE_FREQ                  SOC_DVFS_LEVEL_FULL_PERFORMANCE
#endif

/*
 * As a LE Master, connect to vendor LE Slave
 *
 * support unidirectional audio stream, as audio source.
 */
struct lemusic_master_device {
	/*
	 * store codec and qos info for source channel as master
	 * slave's sink channel will be master's source channel
	 */
	struct bt_audio_codec codec;
	struct bt_audio_qoss_1 qoss;

	uint16_t audio_contexts;

	/* disallowed to connect source stream */
	uint32_t bt_source_disallowed : 1;

	uint32_t bt_source_configured : 1; /* bt audio source configured */
	uint32_t bt_source_enabled : 1; /* bt audio source enabled */
	uint32_t bt_source_started : 1; /* bt audio source started */

	uint32_t bt_source_pending : 1;
	uint32_t bt_source_pending_start : 1;

	/* Audio Source: BT (phone) -> source_stream (lemusic) -> BT (slave) */
	struct bt_audio_chan source_chan;
};

/*
 * As a LE Slave, connect to Phone (which is as LE Master)
 *
 * support bidirectional audio stream.
 */
struct lemusic_slave_device {
	uint32_t bt_source_started : 1;	/* bt audio source started */
	uint32_t adc_to_bt_started : 1;

	uint32_t bt_sink_enabled : 1;	/* bt audio sink enabled */
	uint32_t bt_to_dac_started : 1;

	uint32_t capture_player_load : 1;
	uint32_t capture_player_run : 1;

	uint32_t playback_player_load : 1;
	uint32_t playback_player_run : 1;

#if 1	// TODO: not used?
	uint32_t sink_chan_valid : 1;
	uint32_t sink_chan2_valid : 1;
#endif

	/* Audio Sink: BT -> sink_stream -> DAC */
	media_player_t *playback_player;
	io_stream_t sink_stream;

	struct bt_audio_chan *active_sink_chan;	/* for 1 sink chan case */
	struct bt_audio_chan sink_chans[NUM_OF_SINK_CHAN];
	uint32_t num_of_sink_chan;


	/* Audio Source: ADC -> source_stream -> BT */
	media_player_t *capture_player;
	io_stream_t source_stream;
	struct bt_audio_chan source_chan;
};

/*
 * As a LE Slave BMS, broadcast LE audio data which from phone.
 */
struct lemusic_bms_device {
	struct thread_timer restart_timer;
	struct thread_timer broadcast_start_timer;

	uint8_t cur_stream;

#ifdef BIS_DELAY_BY_RX_START
	uint32_t last_time; //24000 unit
	uint8_t time_update:1;
#endif

	uint8_t player_run:1;
	uint8_t restart:1;
	uint8_t tx_start:1;
	uint8_t tx_sync_start:1;
	uint8_t broadcast_source_enabled:1;
	uint8_t encryption:1;
	uint8_t dsp_run:1;
	uint8_t enable:1;
	uint8_t wait_for_past_req : 1;
	uint8_t use_past:1;
	uint8_t sink_stream_set:1;
	uint32_t restart_count;

	/*
	 * capture of BIS: BIS -> DSP -> DAC
	 */
	media_player_t *player;
	io_stream_t stream[NUM_OF_BROAD_CHAN];
	io_stream_t input_stream;
	struct bt_broadcast_chan broad_chan[NUM_OF_BROAD_CHAN];
	struct bt_broadcast_chan *chan;
	uint8_t num_of_broad_chan;
	int32_t broadcast_dev_handle;

	uint8_t broadcast_code[16];
	struct bt_broadcast_qos *qos;
	uint8_t iso_interval;
	uint8_t irc;
};

/*
 * As a lemusic device, support LE Master and LE Slave simutaneously,
 * and support BMS role
 */
struct lemusic_app_t {
	struct thread_timer monitor_timer;
	struct thread_timer switch_bmr_timer;
	struct thread_timer delay_start;

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	uint8_t set_dvfs_level;
#endif
	uint32_t tts_playing : 1;
	uint32_t playing : 1;

	struct lemusic_slave_device slave;

	struct lemusic_master_device master;

	struct lemusic_bms_device bms;
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
	MSG_LEMUSIC_MESSAGE_CMD_PLAYER_RESET,
};

struct lemusic_app_t *lemusic_get_app(void);
void lemusic_bt_event_proc(struct app_msg *msg);
void lemusic_input_event_proc(struct app_msg *msg);
void lemusic_tts_event_proc(struct app_msg *msg);
void lemusic_app_event_proc(struct app_msg *msg);
void lemusic_tws_event_proc(struct app_msg *msg);

void lemusic_view_show_connected(void);
void lemusic_view_show_disconnected(void);
void lemusic_view_show_play_paused(bool playing);
void lemusic_view_init(void);
void lemusic_view_deinit(void);

int lemusic_init_capture(void);
int lemusic_start_capture(void);
int lemusic_stop_capture(void);
int lemusic_exit_capture(void);

int lemusic_init_playback(void);
int lemusic_start_playback(void);
int lemusic_stop_playback(void);
int lemusic_exit_playback(void);
void lemusic_set_sink_chan_stream(io_stream_t stream);

int lemusic_bms_init_capture(void);
int lemusic_bms_start_capture(void);
int lemusic_bms_stop_capture(void);
int lemusic_bms_exit_capture(void);
int lemusic_bms_check_le_audio_stream(u16_t pcm_time,u16_t normal_level,u8_t aps_status);
int lemusic_bms_source_init(void);
int lemusic_bms_source_exit(void);

int lemusic_get_auracast_mode(void);
void lemusic_set_auracast_mode(int mode);
void lemusic_bms_player_reset_trigger(void);
void lemusic_bms_tws_synced(void);

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
void lemusic_update_dvfs(uint8_t dvfs);
void lemusic_clear_current_dvfs(void);
#endif

#endif  /* _LEMUSIC_APP_H */
