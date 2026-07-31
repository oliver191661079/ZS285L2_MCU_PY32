/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "demo_app.h"
#include <usb/class/usb_audio.h>

void demo_input_event_proc(struct app_msg *msg)
{
	struct demo_app_t *usound = demo_get_app();

	if (NULL == usound){
		return;
	}

	SYS_LOG_INF("cmd %d", msg->cmd);
	switch (msg->cmd) {
	case MSG_DEMO_PLAY_PAUSE_RESUME:
		if (usound->playing) {
			usb_hid_control_pause_play();
			sys_event_notify(SYS_EVENT_PLAY_PAUSE);
		} else {
			sys_event_notify(SYS_EVENT_PLAY_START);
			usb_hid_control_pause_play();
		}
		break;
	case MSG_DEMO_EXIT:
		system_app_launch_del(DESKTOP_PLUGIN_ID_DEMO);
		break;
	default:
		break;
	}
}

static int usound_set_mute(bool mute)
{
	if(mute) {
		audio_system_set_stream_mute(AUDIO_STREAM_USOUND, 1);
	} else {
		audio_system_set_stream_mute(AUDIO_STREAM_USOUND, 0);
	}
	return 0;
}

void demo_event_proc(struct app_msg *msg)
{
	struct demo_app_t *usound = demo_get_app();

	if (NULL == usound){
		return;
	}

	SYS_LOG_INF("cmd %d", msg->cmd);
	switch (msg->cmd)
	{
	case MSG_USOUND_STREAM_START:
		if (!usound->playing) {
			demo_start_play();
		} else {
			usb_audio_set_stream(usound->usound_stream);
		}
		usound->playing = 1;
		demo_show_play_status(true);
		break;
	case MSG_USOUND_STREAM_STOP:
		if (usound->playing) {
			demo_stop_play();
		} else {
			usb_audio_set_stream(NULL);
		}
		usound->playing = 0;
		demo_show_play_status(false);
		break;
	case MSG_USOUND_STREAM_MUTE:
		usound_set_mute(1);
		break;
	case MSG_USOUND_STREAM_UNMUTE:
		usound_set_mute(0);
		break;
	default:
		break;
	}
}

#ifdef CONFIG_PLAYTTS
static void _demo_tts_delay_resume(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	struct demo_app_t *usound = demo_get_app();
	usound->need_resume_play = 0;
	demo_start_play();
}

void demo_tts_event_proc(struct app_msg *msg)
{
	struct demo_app_t *usound = demo_get_app();

	if (!usound)
		return;

	switch (msg->value) {
	case TTS_EVENT_START_PLAY:
		if (usound->player) {
			usound->need_resume_play = 1;
			demo_stop_play();
		}
		break;
	case TTS_EVENT_STOP_PLAY:
		if (usound->need_resume_play) {
			if (thread_timer_is_running(&usound->monitor_timer)) {
				thread_timer_stop(&usound->monitor_timer);
			}
			thread_timer_init(&usound->monitor_timer, _demo_tts_delay_resume, NULL);
			thread_timer_start(&usound->monitor_timer, 2000, 0);
		}
		break;
	default:
		break;
	}
}
#endif
