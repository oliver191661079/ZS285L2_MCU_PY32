/*
 * Copyright (c) 2024 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief local player.h
 */

#ifndef _MPLAYER_H
#define _MPLAYER_H

#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <bt_manager.h>
#include <stream.h>
#include <file_stream.h>
#include <loop_fstream.h>
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
#include <fs.h>

/* used by file_selector of lcmusic, record, alarm, loop_player app */
#define MAX_DIR_LEVEL		9

struct local_player_t {
	media_player_t *player;
	io_stream_t file_stream;
};

struct lcplay_param {
	const char *url;
	media_breakpoint_info_t bp;
	int seek_time;
	u8_t play_mode;		/* 0-normal, 1-loopplay, others-reserved */
#ifdef CONFIG_SOUNDBAR_SAMPLE_RATE_FILTER
	int min_sample_rate_khz;	/* <=0--disable, others--minius sample rate in kHz */
#endif

	void (*play_event_callback)(u32_t event, void *data, u32_t len,
				    void *user_data);
};

struct local_player_t *mplayer_start_play(struct lcplay_param *lcparam);

void mplayer_stop_play(struct local_player_t *lcplayer);

int mplayer_pause(struct local_player_t *lcplayer);

int mplayer_resume(struct local_player_t *lcplayer);

int mplayer_seek(struct local_player_t *lcplayer, int seek_time,
		 media_breakpoint_info_t * bp_info);

u8_t get_music_file_type(const char *url);
const char* get_music_type_str(u8_t type);

int mplayer_update_breakpoint(struct local_player_t *lcplayer,
			      media_breakpoint_info_t * bp_info);

int mplayer_update_media_info(struct local_player_t *lcplayer,
			      media_info_t * info);

#endif				/* _MPLAYER_H */
