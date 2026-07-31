/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _DEMO_APP_H
#define _DEMO_APP_H


#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "demo"

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
#include <app_launch.h>

#include <soc_dvfs.h>
#include <thread_timer.h>


#include "app_defines.h"
#include "sys_manager.h"
#include "app_ui.h"
#include "desktop_manager.h"

#ifdef CONFIG_PLAYTTS
#include "tts_manager.h"
#endif

enum {
	MSG_DEMO_NO_ACTION = MSG_APP_INPUT_MESSAGE_CMD_START,
	MSG_DEMO_PLAY_PAUSE_RESUME,
	MSG_DEMO_EXIT,
};

enum {
	MSG_USOUND_STREAM_START = 1,
	MSG_USOUND_STREAM_STOP,
	MSG_USOUND_STREAM_MUTE,
	MSG_USOUND_STREAM_UNMUTE,
};

enum USOUND_PLAY_STATUS {
	USOUND_STATUS_NULL = 0x0000,
	USOUND_STATUS_PLAYING = 0x0001,
	USOUND_STATUS_PAUSED = 0x0002,
};

#define USOUND_STATUS_ALL  (USOUND_STATUS_PLAYING | USOUND_STATUS_PAUSED)

struct demo_app_t {
	media_player_t *player;
	io_stream_t usound_stream;
	bool playing;
	struct thread_timer monitor_timer;
	u16_t need_resume_play : 1;
	uint8_t device_init;
};

void demo_view_init(void);
void demo_view_deinit(void);
struct demo_app_t *demo_get_app(void);
int demo_get_status(void);
void demo_start_play(void);
void demo_stop_play(void);
void demo_input_event_proc(struct app_msg *msg);
void demo_show_play_status(bool status);
void demo_event_proc(struct app_msg *msg);
void demo_dump_app_data(void);
void demo_tts_event_proc(struct app_msg *msg);

#endif  /* _DMEO_APP_H */

