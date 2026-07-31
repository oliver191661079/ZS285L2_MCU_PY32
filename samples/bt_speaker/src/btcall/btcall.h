/*
 * Copyright (c) 2024 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt call header file
 */

#ifndef _BT_CALL_APP_H_
#define _BT_CALL_APP_H_

#ifdef CONFIG_SYS_LOG
#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "btcall"
#include <logging/sys_log.h>
#endif

#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stream.h>
#include <app_manager.h>
#include <srv_manager.h>
#include <mem_manager.h>
#include <msg_manager.h>
#include <bt_manager.h>
#include <volume_manager.h>
#include <media_player.h>
#include <global_mem.h>
#include <audio_system.h>
#include <soc_dvfs.h>
#include <thread_timer.h>
#include "btservice_api.h"
#include "app_defines.h"
#include "sys_manager.h"
#include "app_ui.h"

enum {
	// bt call key message
	MSG_BT_CALL_VOLUP = MSG_APP_INPUT_MESSAGE_CMD_START,

	MSG_BT_CALL_VOLDOWN,

	MSG_BT_CALL_SWITCH_CALLOUT,

	MSG_BT_CALL_SWITCH_MICMUTE,

	MSG_BT_HOLD_CURR_ANSWER_ANOTHER,

	MSG_BT_HANGUP_ANOTHER,

	MSG_BT_HANGUP_CURR_ANSER_ANOTHER,

	MSG_BT_HANGUP_CALL,

	MSG_BT_ACCEPT_CALL,

	MSG_BT_REJECT_CALL,
};

struct btcall_app_t {
	media_player_t *player;
	media_player_t *ref_capture;

	uint32_t ref_capture_media_opened:1;
	uint32_t ref_capture_media_started:1;

	uint32_t capture_media_opened:1;
	uint32_t capture_media_started:1;
	uint32_t playback_media_opened:1;
	uint32_t playback_media_started:1;
	uint32_t playing:1;
	uint32_t mic_mute:1;
	uint32_t need_resume_play:1;
	uint32_t phonenum_played:1;
	uint32_t stream_established:1;
	uint32_t siri_mode:1;
	uint32_t hfp_ongoing:1;
	uint32_t asqt_simulate:1;
	uint8_t hfp_3way_status;
	io_stream_t source_stream;
	io_stream_t sink_stream;

	io_stream_t ref_stream;

	struct bt_audio_chan bt_chan;
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	u8_t set_dvfs_level;
#endif
};

void btcall_bt_event_proc(struct app_msg *msg);
void btcall_input_event_proc(struct app_msg *msg);
void btcall_tts_event_proc(struct app_msg *msg);
bool bt_call_key_event_handle(int key_event, int event_stage);
void bt_call_start_play_and_capture(void);
void bt_call_stop_play_and_capture(void);
void bt_call_restart_play(void);
int bt_call_handle_enable(struct bt_audio_report *rep);
int bt_call_handle_disable(struct bt_audio_report *rep);
int bt_call_handle_start(struct bt_audio_report *rep);
int bt_call_handle_stop(struct bt_audio_report *rep);
int bt_call_handle_incoming(struct bt_call_report *rep);
int bt_call_handle_dialing(struct bt_call_report *rep);
int bt_call_handle_alerting(struct bt_call_report *rep);
int bt_call_handle_active(struct bt_call_report *rep);
int bt_call_handle_locally_held(struct bt_call_report *rep);
int bt_call_handle_remotely_held(struct bt_call_report *rep);
int bt_call_handle_held(struct bt_call_report *rep);
int bt_call_handle_ended(struct bt_call_report *rep);

struct btcall_app_t *btcall_get_app(void);

int btcall_ring_start(u8_t * phone_num, u16_t phone_num_cnt);
void btcall_ring_stop(void);
void btcall_ring_play_next(void);
int btcall_ring_manager_init(void);
int btcall_ring_manager_deinit(void);
void btcall_view_init(void);
void btcall_view_deinit(void);
#endif				// _BT_CALL_APP_H_
