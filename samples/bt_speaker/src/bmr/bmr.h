/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _BMR_APP_H
#define _BMR_APP_H

#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "bmr"

#include <logging/sys_log.h>
#include <mem_manager.h>
#include <app_manager.h>
#include <srv_manager.h>
#include <tts_manager.h>
#include <volume_manager.h>
#include <msg_manager.h>
#include <input_manager.h>
#include <ui_manager.h>
#include <led_manager.h>
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
#include <clone_stream.h>
#include <buffer_stream.h>
#include <ringbuff_stream.h>
#include <media_mem.h>
#include <audio_record.h>

#include <soc_dvfs.h>
#include <thread_timer.h>

#include "app_defines.h"
#include "app_ui.h"
#include "broadcast.h"
#include "desktop_manager.h"
#include <app_tws.h>

#ifdef CONFIG_PROPERTY
#include <property_manager.h>
#endif

#if (0 == CONFIG_DSP_COMPUTE_COMPLEXITY)
#define BMR_FREQ   SOC_DVFS_LEVEL_SINGLE_MUSIC
#elif (1 == CONFIG_DSP_COMPUTE_COMPLEXITY)
#define BMR_FREQ   SOC_DVFS_LEVEL_ALL_PERFORMANCE
#elif (2 <= CONFIG_DSP_COMPUTE_COMPLEXITY)
#define BMR_FREQ   SOC_DVFS_LEVEL_BR_FULL_PERFORMANCE
#endif

enum {
	/* bt play key message */
	MSG_BMR_PLAY_VOLUP = MSG_APP_INPUT_MESSAGE_CMD_START,
	MSG_BMR_PLAY_VOLDOWN,
	MSG_BMR_PAUSE_RESUME,
	MSG_BMR_PLAY_APP_EXIT,
};

struct bmr_broad_dev {
	uint32_t handle;

	sys_snode_t node;
};

struct bmr_app_t {
	uint8_t player_load : 1;
	uint8_t player_run : 1;
	uint8_t player_paused: 1;
	uint8_t rx_start : 1;

	uint8_t broadcast_sink_enabled : 1;

	uint8_t encryption : 1;
	uint8_t tts_playing :1;
	uint8_t tws : 1;
	/*
	 * playback of BIS: I2S_IN -> DSP -> BIS
	 */
	media_player_t *player;
	io_stream_t stream;
	io_stream_t input_stream;
	struct bt_broadcast_chan broad_chan[NUM_OF_BROAD_CHAN];
	uint8_t num_of_broad_chan;

	sys_slist_t broad_dev_list;

	uint8_t broadcast_code[16];

	struct bt_broadcast_chan *chan;
};

struct bmr_app_t *bmr_get_app(void);

void bmr_bt_event_proc(struct app_msg *msg);

void bmr_input_event_proc(struct app_msg *msg);
void bmr_view_init(void);
void bmr_view_deinit(void);
void bmr_view_show_connected(bool connected);

int bmr_init_playback(void);
int bmr_start_playback(void);
int bmr_stop_playback(void);
int bmr_exit_playback(void);

void bmr_tts_event_proc(struct app_msg *msg);

#endif  /* _BMR_APP_H */

