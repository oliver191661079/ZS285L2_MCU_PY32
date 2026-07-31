/*
 * Copyright (c) 2024 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt call event
 */
#include <msg_manager.h>
#include <thread_timer.h>
#include <media_player.h>
#include <audio_system.h>
#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stream.h>
#include "btcall.h"
#include "ui_manager.h"
#include "tts_manager.h"
#include "audio_system.h"
#include "audio_policy.h"

static void _bt_call_disconnected(void)
{
	SYS_LOG_INF(" enter\n");
}

int bt_call_handle_enable(struct bt_audio_report *rep)
{
	struct btcall_app_t *btcall = btcall_get_app();
	struct bt_audio_chan *bt_chan;

	SYS_LOG_INF("0x%x", rep->handle);
	if (NULL == btcall || NULL == rep) {
		SYS_LOG_WRN("NULL");
		return -1;
	}

	if (rep->audio_contexts != BT_AUDIO_CONTEXT_ASQT_TEST
		&& rep->audio_contexts != BT_AUDIO_CONTEXT_CONVERSATIONAL) {
		SYS_LOG_WRN("wrong context");
		return -2;
	}

	bt_chan = &btcall->bt_chan;
	bt_chan->handle = rep->handle;
	bt_chan->id = rep->id;

	return 0;
}

int bt_call_handle_start(struct bt_audio_report *rep)
{
	struct btcall_app_t *btcall = btcall_get_app();

	if (NULL == btcall || NULL == rep) {
		return -1;
	}

	SYS_LOG_INF("0x%x", rep->handle);

	if (rep->audio_contexts != BT_AUDIO_CONTEXT_ASQT_TEST
		&& rep->audio_contexts != BT_AUDIO_CONTEXT_CONVERSATIONAL) {
		SYS_LOG_WRN("wrong context");
		return -2;
	}
	if (rep->audio_contexts == BT_AUDIO_CONTEXT_ASQT_TEST) {
		btcall->asqt_simulate = true;
	} else {
		btcall->asqt_simulate = false;
	}

#ifdef CONFIG_SEG_LED_MANAGER
	SYS_LOG_INF("..siri_mode %d\n", btcall->siri_mode);
	if (!btcall->siri_mode && btcall->hfp_ongoing) {
		seg_led_display_string(SLED_NUMBER1, "C", true);
		seg_led_display_string(SLED_NUMBER3, "HF",
					true);
	}
#endif

	btcall->stream_established = 1;
	btcall->mic_mute = false;

#ifdef CONFIG_BT_CALL_FORCE_PLAY_PHONE_NUM
	if (!btcall->phonenum_played) {
		return -2;
	}
#endif

	bt_call_start_play_and_capture();
	btcall->playing = 1;

	return 0;
}

int bt_call_handle_stop(struct bt_audio_report *rep)
{
	struct btcall_app_t *btcall = btcall_get_app();

	if (NULL == btcall || NULL == rep) {
		return -1;
	}

	SYS_LOG_INF("0x%x", rep->handle);
	if (rep->audio_contexts != BT_AUDIO_CONTEXT_ASQT_TEST
		&& rep->audio_contexts != BT_AUDIO_CONTEXT_CONVERSATIONAL) {
		SYS_LOG_WRN("wrong context");
		return -2;
	}
#ifdef CONFIG_SEG_LED_MANAGER
	if (!btcall->siri_mode && btcall->hfp_ongoing) {
		seg_led_display_string(SLED_NUMBER1, "C", true);
		seg_led_display_string(SLED_NUMBER3, "AG",
					true);
	}
#endif
	btcall->stream_established = 0;
	bt_call_stop_play_and_capture();
	btcall->playing = 0;
	return 0;
}

void btcall_bt_event_proc(struct app_msg *msg)
{
	struct btcall_app_t *btcall = btcall_get_app();
	struct bt_call_report *call_rep;

	SYS_LOG_INF("cmd: %d\n", msg->cmd);

	switch (msg->cmd) {
	case BT_CALL_RING_STATR_EVENT:
		call_rep = (struct bt_call_report *)(msg->ptr);
		if (!btcall->phonenum_played) {
			btcall_ring_start((uint8_t *) (call_rep + 1),
					  strlen((uint8_t *) (call_rep + 1)));
			btcall->phonenum_played = 1;
		}
		break;
	case BT_CALL_RING_STOP_EVENT:
		btcall_ring_stop();
#ifdef CONFIG_BT_CALL_FORCE_PLAY_PHONE_NUM
		if (btcall->stream_established) {
			bt_call_restart_play();
			btcall->playing = 1;
		}
#endif
		break;

	case BT_AUDIO_STREAM_ENABLE:
		bt_call_handle_enable((struct bt_audio_report *)(msg->ptr));
		break;
	case BT_AUDIO_STREAM_START:
		bt_call_handle_start((struct bt_audio_report *)(msg->ptr));
		break;
	case BT_AUDIO_STREAM_STOP:
		bt_call_handle_stop((struct bt_audio_report *)(msg->ptr));
		break;

	case BT_CALL_INCOMING:
		{
#ifdef CONFIG_SEG_LED_MANAGER
			seg_led_display_string(SLED_NUMBER1, "C", true);
			seg_led_display_string(SLED_NUMBER3, "IN", true);
#endif
			if (btcall && btcall->player) {
				media_player_set_hfp_connected(btcall->player,
							       false);
			}
			break;
		}

	case BT_CALL_DIALING:
		{
#ifdef CONFIG_SEG_LED_MANAGER
			seg_led_display_string(SLED_NUMBER1, "C", true);
			seg_led_display_string(SLED_NUMBER3, "OU", true);
#endif
			btcall->phonenum_played = 1;
			if (btcall && btcall->player) {
				media_player_set_hfp_connected(btcall->player,
							       false);
			}
			break;
		}

	case BT_CALL_ALERTING:
		{
#ifdef CONFIG_SEG_LED_MANAGER
			seg_led_display_string(SLED_NUMBER1, "C", true);
			seg_led_display_string(SLED_NUMBER3, "OU", true);
#endif
			btcall->phonenum_played = 1;
			if (btcall && btcall->player) {
				media_player_set_hfp_connected(btcall->player,
							       false);
			}
			break;
		}

	case BT_CALL_ACTIVE:
		{
#ifdef CONFIG_SEG_LED_MANAGER
			seg_led_display_string(SLED_NUMBER1, "C", true);
			if (btcall->playing) {
				seg_led_display_string(SLED_NUMBER3, "HF",
						       true);
			} else {
				seg_led_display_string(SLED_NUMBER3, "AG",
						       true);
			}
			btcall->hfp_ongoing = 1;
			btcall->phonenum_played = 1;
#endif
#ifdef CONFIG_LED_MANAGER
			led_manager_set_breath(0, NULL, OS_FOREVER, NULL);
			led_manager_set_breath(1, NULL, OS_FOREVER, NULL);
#endif
			btcall_ring_stop();
			if (btcall && btcall->player) {
				media_player_set_hfp_connected(btcall->player,
							       true);
			}
			break;
		}

	case BT_CALL_LOCALLY_HELD:
		{
			break;
		}

	case BT_CALL_REMOTELY_HELD:
		{
			break;
		}

	case BT_CALL_HELD:
		{
			break;
		}

	case BT_CALL_ENDED:
		{
			break;
		}

	case BT_CALL_SIRI_MODE:
		{
#ifdef CONFIG_SEG_LED_MANAGER
			seg_led_manager_clear_screen(LED_CLEAR_ALL);
			seg_led_display_string(SLED_NUMBER1, "SIRI", true);
			btcall->siri_mode = 1;
#endif
			btcall->phonenum_played = 1;
			SYS_LOG_INF("siri_mode %d\n", btcall->siri_mode);
			break;
		}

	case BT_CALL_DISCONNECTED:
		{
			btcall->need_resume_play = 0;
			_bt_call_disconnected();
			break;
		}

	case BT_VOLUME_VALUE:
		{
			struct bt_volume_report *rep =
			    (struct bt_volume_report *)msg->ptr;

			audio_system_set_stream_volume(AUDIO_STREAM_VOICE,
						       rep->value);

			if (btcall && btcall->player) {
				media_player_set_volume(btcall->player,
							rep->value, rep->value);
			}
			break;
		}

	default:
		break;
	}
}

static void _btcall_key_func_switch_mic_mute(void)
{
	struct btcall_app_t *btcall = btcall_get_app();

	btcall->mic_mute ^= 1;

	audio_system_mute_microphone(btcall->mic_mute);

	if (btcall->mic_mute)
		sys_event_notify(SYS_EVENT_MIC_MUTE_ON);
	else
		sys_event_notify(SYS_EVENT_MIC_MUTE_OFF);
}

static void _btcall_key_func_volume_adjust(int updown)
{
	int volume = 0;
	struct btcall_app_t *btcall = btcall_get_app();

	if (updown) {
		volume = system_volume_up(AUDIO_STREAM_VOICE, 1);
	} else {
		volume = system_volume_down(AUDIO_STREAM_VOICE, 1);
	}

	if (btcall->player) {
		media_player_set_volume(btcall->player, volume, volume);
	}
}

void btcall_input_event_proc(struct app_msg *msg)
{
	switch (msg->cmd) {
	case MSG_BT_CALL_VOLUP:
		{
			_btcall_key_func_volume_adjust(1);
			break;
		}
	case MSG_BT_CALL_VOLDOWN:
		{
			_btcall_key_func_volume_adjust(0);
			break;
		}
	case MSG_BT_ACCEPT_CALL:
		{
			bt_manager_call_accept(NULL);
			break;
		}
	case MSG_BT_REJECT_CALL:
		{
			bt_manager_call_terminate(NULL, 0);
			break;
		}
	case MSG_BT_HANGUP_CALL:
		{
			bt_manager_call_terminate(NULL, 0);
			break;
		}
	case MSG_BT_HANGUP_ANOTHER:
		{
			bt_manager_call_hangup_another_call();
			break;
		}
	case MSG_BT_HOLD_CURR_ANSWER_ANOTHER:
		{
			bt_manager_call_holdcur_answer_call();
			break;
		}
	case MSG_BT_HANGUP_CURR_ANSER_ANOTHER:
		{
			bt_manager_call_hangupcur_answer_call();
			break;
		}
	case MSG_BT_CALL_SWITCH_CALLOUT:
		{
#ifdef CONFIG_SEG_LED_MANAGER
			struct btcall_app_t *btcall = btcall_get_app();
			seg_led_display_string(SLED_NUMBER1, "C", true);
			if (btcall->playing) {
				seg_led_display_string(SLED_NUMBER3, "AG",
						       true);
			} else {
				seg_led_display_string(SLED_NUMBER3, "HF",
						       true);
			}
#endif
			bt_manager_call_switch_sound_source();
			break;
		}
	case MSG_BT_CALL_SWITCH_MICMUTE:
		{
			_btcall_key_func_switch_mic_mute();
			break;
		}

	case MSG_BT_SIRI_STOP:
		{
			bt_manager_hfp_stop_siri();
			break;
		}

	}
}

#ifdef CONFIG_PLAYTTS
void btcall_tts_event_proc(struct app_msg *msg)
{
	struct btcall_app_t *btcall = btcall_get_app();

	SYS_LOG_INF("val: %d\n", msg->value);
	switch (msg->value) {
	case TTS_EVENT_START_PLAY:
		if (btcall->player) {
			btcall->need_resume_play = 1;
			if (btcall->bt_chan.handle != 0) {
				bt_call_stop_play_and_capture();
			}
		}
		break;
	case TTS_EVENT_STOP_PLAY:
		if (btcall->need_resume_play) {
			btcall->need_resume_play = 0;
			bt_call_restart_play();
		} else {
			btcall_ring_play_next();
		}
		break;
	}
}
#endif
