/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief usound app main.
 */

#include "usound.h"
#ifdef CONFIG_PLAYTTS
#include "tts_manager.h"
#endif
#ifdef CONFIG_ESD_MANAGER
#include "esd_manager.h"
#endif

#ifdef CONFIG_PROPERTY
#include "property_manager.h"
#endif

#ifdef CONFIG_TOOL
#include "tool_app.h"
#endif

#ifdef CONFIG_USB_HOTPLUG
#include <hotplug_manager.h>
#endif

#include <ui_manager.h>
#include <audio_policy.h>
#include "app_launch.h"


static struct usound_app_t *p_usound = NULL;

struct usound_app_t *usound_get_app(void)
{
	return p_usound;
}

void _usound_delay_resume(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	SYS_LOG_INF("playing %d\n", p_usound->playing);
	if (!p_usound->playing) {
		usb_hid_control_pause_play();
	}
	//usound_start_playback();
}

static void _usound_restore_play_state(void)
{
	u8_t init_play_state = USOUND_STATUS_PLAYING;
#ifdef CONFIG_ESD_MANAGER
	if (esd_manager_check_esd()) {
		esd_manager_restore_scene(TAG_PLAY_STATE, &init_play_state, 1);
	}
#endif

	SYS_LOG_INF("%d\n", init_play_state);

	if (init_play_state == USOUND_STATUS_PLAYING) {
		if (thread_timer_is_running(&p_usound->monitor_timer)) {
			thread_timer_stop(&p_usound->monitor_timer);
		}
		thread_timer_init(&p_usound->monitor_timer,
				  _usound_delay_resume, NULL);
		thread_timer_start(&p_usound->monitor_timer, 2000, 0);
		usound_show_play_status(true);
	} else {
		usound_show_play_status(false);
	}
}

void usound_sync_tws_vol(u8_t level)
{
	struct app_msg msg = { 0 };

	msg.type = MSG_USOUND_APP_EVENT;
	msg.cmd = MSG_USOUND_VOL_UPDATE;
	msg.value = level;

	send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
}

static u8_t g_usound_app_cmd = MSG_USOUND_CMD_NULL;
static int usound_stream_app_msg_match(void *msg, k_tid_t target_thread, k_tid_t source_thread)
{
	struct app_msg *p_app_msg = (struct app_msg *)msg;

	if(!p_app_msg || source_thread != os_current_get()
		|| target_thread != (k_tid_t)app_manager_get_apptid(CONFIG_FRONT_APP_NAME)){
		return 0;
	}

	if(p_app_msg->type == MSG_USOUND_APP_EVENT){
		if(p_app_msg->cmd == g_usound_app_cmd){
			return 1;
		}
	}

	return 0;
}

static void _usb_audio_event_callback_handle(uint8_t type, int param)
{
	bool skip = false;
	bool flush= false;
	struct app_msg msg = { 0 };

	SYS_LOG_INF("t:%d v:%d", type, param);

	msg.type = MSG_USOUND_APP_EVENT;

	switch (type) {
	case USOUND_SYNC_HOST_MUTE:
		msg.cmd = MSG_USOUND_STREAM_MUTE;
		break;
	case USOUND_SYNC_HOST_UNMUTE:
		msg.cmd = MSG_USOUND_STREAM_UNMUTE;
		break;
	case USOUND_SYNC_HOST_VOL_TYPE:
		msg.cmd = MSG_USOUND_STREAM_VOLUME;
		msg.value = param;
		break;
	case USOUND_STREAM_START:
		flush = true;
		msg.cmd = MSG_USOUND_STREAM_START;
		msg.value = param;
		break;
	case USOUND_STREAM_STOP:
		flush = true;
		msg.cmd = MSG_USOUND_STREAM_STOP;
		break;
	case USOUND_SAMPLERATE_CHANGE:
		skip = true;
		SYS_LOG_INF("samplerate %d", param);
		break;
#ifdef CONFIG_USOUND_MIC
	case USOUND_UPLOAD_STREAM_START:
		flush = true;
		msg.cmd = MSG_USOUND_UPLOAD_STREAM_START;
		msg.value = param;
		break;
	case USOUND_UPLOAD_STREAM_STOP:
		flush = true;
		msg.cmd = MSG_USOUND_UPLOAD_STREAM_STOP;
		break;
	case UMIC_SYNC_HOST_VOL_TYPE:
		msg.cmd = MSG_USOUND_UPLOAD_STREAM_VOLUME;
		msg.value = param;
		break;
	case UMIC_SYNC_HOST_MUTE:
		msg.cmd = MSG_USOUND_UPLOAD_STREAM_MUTE;
		break;
	case UMIC_SYNC_HOST_UNMUTE:
		msg.cmd = MSG_USOUND_UPLOAD_STREAM_UNMUTE;
		break;
#endif
	default:
		skip = true;
		break;
	}

	if(flush) {
		if(!os_is_free_msg_enough()){
			SYS_LOG_INF("delete msg");
			g_usound_app_cmd = msg.cmd;
			os_msg_delete(usound_stream_app_msg_match);
		}
	}

	if(!skip) {
		send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
	}
}

#ifdef CONFIG_USOUND_FEATURE_RESTART
void bms_uac_player_reset_trigger(void)
{
	if (p_usound != NULL) {
		p_usound->restart = 1;
		p_usound->restart_count++;
		SYS_LOG_INF("%d", p_usound->restart_count);
	}
}

static void bms_uac_restart_handler(struct thread_timer *ttimer,
				    void *expiry_fn_arg)
{
	struct app_msg msg = { 0 };

	if (p_usound && p_usound->restart) {
		SYS_LOG_INF("restart\n");
		msg.type = MSG_USOUND_APP_EVENT;
		msg.cmd = MSG_USOUND_STREAM_RESTART;
		send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
		p_usound->restart = 0;
		p_usound->restart_count = 0;
	}
}
#endif

#ifdef CONFIG_BMS_UAC_APP

static struct bt_broadcast_qos source_qos = {
	.packing = BT_AUDIO_PACKING_INTERLEAVED,
	.framing = BT_AUDIO_UNFRAMED,
	.phy = BT_AUDIO_PHY_2M,
	.rtn = 2,
	.max_sdu = BROADCAST_SDU,
	/* max_transport_latency, unit: ms */
	.latency = BROADCAST_LAT,
	/* sdu_interval, unit: us */
	.interval = BROADCAST_SDU_INTERVAL,
	/* presentation_delay, unit: us */
	.delay = BCST_QOS_DELAY,
	/* processing_time, unit: us */
	.processing = 8000,
};

int bms_uac_source_init(void)
{
	struct bt_broadcast_source_create_param* param;
	int ret;

	if (!p_usound) {
		return -EINVAL;
	}

	SYS_LOG_INF("%d", p_usound->bms_source);
	p_usound->bms_source = 1;

	if (thread_timer_is_running(&p_usound->broadcast_start_timer))
		thread_timer_stop(&p_usound->broadcast_start_timer);

	if (p_usound->broadcast_dev_handle) {
		SYS_LOG_WRN("already exist\n");
		return -EINVAL;
	}

	param = broadcast_init_source_param();
	if(NULL == param) {
		p_usound->bms_source = 0;
		SYS_LOG_ERR("no source param\n");
		return -EINVAL;
	}

#if ENABLE_ENCRYPTION
	memcpy(p_usound->broadcast_code, param->broadcast_code, 16);
#endif

	param->qos = p_usound->qos;
	p_usound->irc = param->big_param->irc;
	ret = bt_manager_broadcast_source_create(param);
	broadcast_free_source_param(param);
	if (ret < 0) {
		SYS_LOG_ERR("failed %d", ret);
		thread_timer_start(&p_usound->broadcast_start_timer, 300, 0);
		p_usound->bms_source = 0;
		return ret;
	}
	p_usound->broadcast_dev_handle = ret;
	SYS_LOG_INF("dev 0x%x\n", ret);

	return 0;
}

int bms_uac_source_exit(void)
{
	if (thread_timer_is_running(&p_usound->broadcast_start_timer))
		thread_timer_stop(&p_usound->broadcast_start_timer);

	if (p_usound->broadcast_dev_handle) {
		SYS_LOG_INF("0x%x\n", p_usound->broadcast_dev_handle);
		bt_manager_broadcast_source_disable(p_usound->broadcast_dev_handle);
		bt_manager_broadcast_source_release(p_usound->broadcast_dev_handle);
		p_usound->broadcast_dev_handle = 0;
	}

	//bt_manager_pawr_adv_stop();
	p_usound->bms_source = 0;

	return 0;
}

bool usound_is_bms_mode(void)
{
#ifdef CONFIG_BMS_UAC_APP
	return (0 != system_app_get_auracast_mode())? true : false;
#else
	return false;
#endif
}

void usound_set_auracast_mode(bool mode)
{
	struct usound_app_t *usound = usound_get_app();

	if (NULL == usound) {
		return;
	}

	SYS_LOG_INF("%d->%d", usound_is_bms_mode(), mode);
	if (mode == usound_is_bms_mode()) {
		return;
	}

	if (!mode) {
		if (1 == usound->bms_source) {
			bms_uac_source_exit();
		}
		//bt_manager_pawr_adv_stop();
		system_app_set_auracast_mode(0);
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		if (usound->bms_dvfs) {
			soc_dvfs_unset_level(usound->bms_dvfs, "bms_uac");
			usound->bms_dvfs = 0;
		}
#endif
	} else {
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		if (0 == usound->bms_dvfs) {
			usound->bms_dvfs = BCST_FREQ;
			soc_dvfs_set_level(usound->bms_dvfs, "bms_uac");
		}
#endif
		bms_uac_source_init();
		system_app_set_auracast_mode(1);
	}
}

static void bms_uac_start_handler(struct thread_timer *ttimer,
				  void *expiry_fn_arg)
{
	if (NULL == p_usound) {
		return;
	}
	SYS_LOG_INF("%d", system_app_get_auracast_mode());
	if (usound_is_bms_mode()) {
		bms_uac_source_init();
	}
}
#endif

static int _usound_init(void *p1, void *p2, void *p3)
{
	if (p_usound)
		return 0;

	p_usound = app_mem_malloc(sizeof(struct usound_app_t));
	if (!p_usound) {
		SYS_LOG_ERR("malloc failed!\n");
		return -ENOMEM;
	}

#ifdef CONFIG_TWS
#ifndef CONFIG_TWS_BACKGROUND_BT
	bt_manager_halt_phone();
#else
	if (system_check_low_latencey_mode()) {
		bt_manager_halt_phone();
	}
#endif
	bt_manager_set_stream_type(AUDIO_STREAM_USOUND);
#endif

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	p_usound->set_dvfs_level = SOC_DVFS_LEVEL_FULL_PERFORMANCE;
	soc_dvfs_set_level(p_usound->set_dvfs_level, "uac");
#endif

	memset(p_usound, 0, sizeof(struct usound_app_t));

	usound_view_init();

#ifdef CONFIG_PLAYTTS
	if(tts_manager_is_playing()) {
		p_usound->tts_playing = 1;
	}
#endif

#ifdef CONFIG_USOUND_FEATURE_RESTART
	thread_timer_init(&p_usound->restart_timer, bms_uac_restart_handler,
			  NULL);
	thread_timer_start(&p_usound->restart_timer, 200, 200);
#endif

#ifdef CONFIG_APP_TWS_SNOOP
	if (app_tws_status_get_enable()) {
		app_tws_on_source_switch(false);
	}
#endif

#ifdef CONFIG_BMS_UAC_APP
	p_usound->qos = &source_qos;
	thread_timer_init(&p_usound->broadcast_start_timer,
			  bms_uac_start_handler, NULL);

	if (usound_is_bms_mode()) {
		system_app_set_auracast_mode(1);
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		if(0 == p_usound->bms_dvfs) {
			p_usound->bms_dvfs = BCST_FREQ;
			soc_dvfs_set_level(p_usound->bms_dvfs, "bms_uac");
		}
#endif
		bms_uac_source_init();
	}
#endif

	usb_audio_init(_usb_audio_event_callback_handle);
	p_usound->current_volume_level =
	    audio_system_get_stream_volume(AUDIO_STREAM_USOUND);
	_usound_restore_play_state();

	SYS_LOG_INF("init ok\n");
	return 0;
}

static int _usound_exit(void)
{
	if (!p_usound) {
		return -1;
	}

	if (thread_timer_is_running(&p_usound->monitor_timer)) {
		thread_timer_stop(&p_usound->monitor_timer);
	}
#ifdef CONFIG_USOUND_FEATURE_RESTART
	if (thread_timer_is_running(&p_usound->restart_timer)) {
		thread_timer_stop(&p_usound->restart_timer);
	}
#endif

#ifdef CONFIG_BMS_UAC_APP
	if (thread_timer_is_running(&p_usound->broadcast_start_timer))
		thread_timer_stop(&p_usound->broadcast_start_timer);

#if ENABLE_PADV_APP
	padv_tx_deinit();
#endif
#endif
	usound_stop_capture();
	usound_stop_playback();

#ifdef CONFIG_BMS_UAC_APP
	bms_uac_stop_capture();
	bms_uac_exit_capture();
	bms_uac_source_exit();
#endif

	if (p_usound->playing) {
		usb_hid_control_pause_play();
	}
	usb_audio_deinit();

	usound_view_deinit();

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	if (p_usound->set_dvfs_level) {
		soc_dvfs_unset_level(p_usound->set_dvfs_level, "uac");
		p_usound->set_dvfs_level = 0;
	}
	if (p_usound->bms_dvfs) {
		soc_dvfs_unset_level(p_usound->bms_dvfs, "bms_uac");
		p_usound->bms_dvfs = 0;
	}
#endif

	app_mem_free(p_usound);
	p_usound = NULL;

#ifdef CONFIG_PROPERTY
	property_flush_req(NULL);
#endif

	SYS_LOG_INF("exit ok\n");
	return 0;
}

static int _usound_proc_msg(struct app_msg *msg)
{
	int ret = 0;

	SYS_LOG_INF("type %d, cmd %d, value 0x%x\n", msg->type, msg->cmd,
		    msg->value);
	switch (msg->type) {
	case MSG_INPUT_EVENT:
		usound_input_event_proc(msg);
		break;
#ifdef CONFIG_PLAYTTS
	case MSG_TTS_EVENT:
		usound_tts_event_proc(msg);
		break;
#endif
	case MSG_USOUND_APP_EVENT:
		usound_app_event_proc(msg);
		break;
#ifdef CONFIG_BMS_UAC_APP
	case MSG_BT_EVENT:
		bms_uac_bt_event_proc(msg);
		break;
#endif
	case MSG_EXIT_APP:
		_usound_exit();
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}

int usound_dump_app_data(void)
{
	print_buffer_lazy(APP_ID_USOUND, (void *)p_usound,
			  sizeof(struct usound_app_t));
	return 0;
}

DESKTOP_PLUGIN_DEFINE(DESKTOP_PLUGIN_ID_UAC, _usound_init, _usound_exit,
		      _usound_proc_msg, usound_dump_app_data, NULL, NULL, NULL);
