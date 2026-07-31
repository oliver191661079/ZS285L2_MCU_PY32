/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief usound event
 */

#include "usound.h"
#include "tts_manager.h"
#include <usb/class/usb_audio.h>
#include <app_launch.h>

#ifdef CONFIG_ESD_MANAGER
#include "esd_manager.h"
#endif
#ifdef CONFIG_PROPERTY
#include <property_manager.h>
#endif

#ifdef CONFIG_BMS_UAC_APP
static void usound_switch_auracast();
#endif

static void usound_play_pause(void)
{
	struct usound_app_t *usound = usound_get_app();
	if (NULL == usound) {
		return;
	}
#ifdef CONFIG_BMS_UAC_APP
	SYS_LOG_INF("mode %d, source %d", usound_is_bms_mode(),
		    usound->broadcast_source_enabled);
	if (usound_is_bms_mode()) {
		if (usound->capture_player_run) {
			bms_uac_stop_capture();
			bms_uac_exit_capture();
		}
		usound->tx_start = 0;
		usound->tx_sync_start = 0;
	}
#endif

	if (usound->playback_player_run) {
		usound_stop_playback();
	}

}

static void usound_play_resume(void)
{
	struct usound_app_t *usound = usound_get_app();
	if (NULL == usound) {
		return;
	}

	SYS_LOG_INF("");

	if(!usound->playing) {
		SYS_LOG_INF("not playing");
		return;
	}

#ifdef CONFIG_BMS_UAC_APP
	if (usound_is_bms_mode()) {
		if(NULL == usound->chan) {
			SYS_LOG_INF("source not configed");
			return;
		}
	}
#endif

	if (!usound->playback_player_run) {
		usound_start_playback();
	}

#ifdef CONFIG_BMS_UAC_APP
	if (usound_is_bms_mode()) {
		if (usound->broadcast_source_enabled) {
			if (!usound->capture_player_run) {
				bms_uac_init_capture();
				bms_uac_start_capture();
			}
		} else {
			SYS_LOG_INF("source not enabled");
		}
	}
#endif
}

#ifdef CONFIG_PLAYTTS
void usound_tts_event_proc(struct app_msg *msg)
{
	struct usound_app_t *usound = usound_get_app();

	if (NULL == usound) {
		return;
	}

	SYS_LOG_INF("%d", msg->value);
	switch (msg->value) {
	case TTS_EVENT_START_PLAY:
		usound->tts_playing = 1;
		usound_play_pause();
		break;
	case TTS_EVENT_STOP_PLAY:
		usound->tts_playing = 0;
		usound_play_resume();
		break;
	default:
		break;
	}
}
#endif

#ifdef CONFIG_BMS_UAC_APP

/* find the existed broad chan */
static struct bt_broadcast_chan *find_broad_chan(uint32_t handle, uint8_t id)
{
	struct usound_app_t *usound = usound_get_app();
	struct bt_broadcast_chan *chan;
	int i;

	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		chan = &usound->broad_chan[i];
		if (chan->handle == handle && chan->id == id) {
			return chan;
		}
	}
	return NULL;
}

/* find a new broad chan */
static struct bt_broadcast_chan *new_broad_chan(uint32_t handle, uint8_t id)
{
	struct usound_app_t *usound = usound_get_app();
	struct bt_broadcast_chan *chan;
	int i;

	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		chan = &usound->broad_chan[i];
		if (chan->handle == 0 && chan->id == 0) {
			return chan;
		}
	}
	return NULL;
}

static void bms_uac_tx_start(uint32_t handle, uint8_t id, uint16_t audio_handle)
{
	struct usound_app_t *usound = usound_get_app();

	if (usound->tx_start) {
		return;
	}

	if (usound->broadcast_source_enabled && usound->capture_player_run) {
		media_player_trigger_audio_record_start(usound->capture_player);
		usound->tx_start = 1;
		//Time sensitive, short log to save time.
		printk("[" SYS_LOG_DOMAIN "] trigger record\n");
	}
}

static void bms_uac_tx_sync_start(uint32_t handle, uint8_t id,
				  uint16_t audio_handle)
{
	struct usound_app_t *usound = usound_get_app();

	if (usound->tx_sync_start) {
		return;
	}

	if (usound->broadcast_source_enabled && usound->playback_player_run) {
		media_player_trigger_audio_track_start(usound->playback_player);
		usound->tx_sync_start = 1;
		//Time sensitive, short log to save time.
		printk("[" SYS_LOG_DOMAIN "] trigger track\n");
	}
}

static int bms_uac_handle_source_config(struct bt_braodcast_configured *rep)
{
	struct usound_app_t *usound = usound_get_app();
	struct bt_broadcast_chan *chan;
	uint8_t vnd_buf[20] = { 0 };
	uint8_t type = 0;

	SYS_LOG_INF("h:0x%x id:%d\n", rep->handle, rep->id);

	chan = find_broad_chan(rep->handle, rep->id);
	if (!chan) {
		chan = new_broad_chan(rep->handle, rep->id);
		if (!chan) {
			SYS_LOG_ERR("no broad chan handle: %d, id: %d\n",
				    rep->handle, rep->id);
			return -EINVAL;
		}
		usound->num_of_broad_chan++;
	}

	chan->handle = rep->handle;
	chan->id = rep->id;

	vnd_buf[0] = VS_ID & 0xFF;
	vnd_buf[1] = VS_ID >> 8;
	memcpy(&vnd_buf[2], usound->broadcast_code, 16);
	vnd_buf[18] = SERIVCE_UUID & 0xFF;
	vnd_buf[19] = SERIVCE_UUID >> 8;

#if !VS_COMPANY_ID
	type = BT_DATA_SVC_DATA16;
#endif

	if (usound->num_of_broad_chan == 1) {
		bt_manager_broadcast_stream_cb_register(chan, bms_uac_tx_start,
							NULL);
		bt_manager_broadcast_stream_tws_sync_cb_register(chan,
								 bms_uac_tx_sync_start);
		bt_manager_broadcast_stream_set_tws_sync_cb_offset(chan,
								   broadcast_get_tws_sync_offset
								   (usound->
								    qos));
		bt_manager_broadcast_source_vnd_ext_send(chan->handle, vnd_buf,
							 sizeof(vnd_buf), type);
		usound->chan = chan;
		if(!usound->tts_playing && usound->playing) {
			usound_start_playback();
		}
	}

	if(app_tws_status_get_enable()) {
		bt_manager_broadcast_source_enable(usound->chan->handle);
	} else {
		if (usound->num_of_broad_chan >= NUM_OF_BROAD_CHAN) {
			bt_manager_broadcast_source_enable(usound->chan->handle);
		}
	}

	return 0;
}

static int bms_uac_handle_source_enable(struct bt_broadcast_report *rep)
{
	struct usound_app_t *usound = usound_get_app();

	if (NULL == usound) {
		return -1;
	}

	SYS_LOG_INF("handle 0x%x, id %d\n", rep->handle, rep->id);
	SYS_LOG_INF("chan %d\n", usound->num_of_broad_chan);

	if(!app_tws_status_get_enable()) {
		if (BROADCAST_NUM_BIS > rep->id) {
			SYS_LOG_INF("wait all done.");
			return 0;
		}
	}

	usound->broadcast_source_enabled = 1;
#if ENABLE_PADV_APP
	if (usound->chan != NULL) {
		padv_tx_init(usound->chan->handle, AUDIO_STREAM_USOUND);
	}
#endif
	if (usound->tts_playing) {
		SYS_LOG_INF("tts playing\n");
		return -4;
	}

	if (!usound->playing) {
		SYS_LOG_INF("usound stoped\n");
		return -5;
	}
	bms_uac_init_capture();
	bms_uac_start_capture();

	return 0;
}

static int bms_uac_handle_source_disable(struct bt_broadcast_report *rep)
{
	struct usound_app_t *usound = usound_get_app();

	if ((rep->handle != usound->broadcast_dev_handle) &&
	    (usound->broadcast_dev_handle != 0)) {
		SYS_LOG_WRN("Handle mismatch:0x%x,0x%x\n", rep->handle, usound->broadcast_dev_handle);
		return -1;
	}

#if ENABLE_PADV_APP
	padv_tx_deinit();
#endif
	bms_uac_stop_capture();
	bms_uac_exit_capture();

	usb_audio_set_stream(NULL);

	//usound->broadcast_source_enabled = 0;
	usound->tx_start = 0;
	usound->tx_sync_start = 0;

	struct bt_broadcast_chan *chan = find_broad_chan(rep->handle, rep->id);
	if (chan) {
		if (usound->num_of_broad_chan) {
			usound->num_of_broad_chan--;
			chan->handle = 0;
			chan->id = 0;
			if (chan == usound->chan) {
				usound->chan = NULL;
			}
			if (!usound->num_of_broad_chan) {
				usound->broadcast_source_enabled = 0;
				SYS_LOG_INF("all disabled\n");
			}
		} else {
			SYS_LOG_WRN("should not be here\n");
		}
	}

	return 0;
}

static int bms_uac_handle_source_release(struct bt_broadcast_report *rep)
{
	struct usound_app_t *usound = usound_get_app();

	if (NULL == usound) {
		return -1;
	}

	if ((rep->handle != usound->broadcast_dev_handle) &&
	    (usound->broadcast_dev_handle != 0)) {
		SYS_LOG_WRN("Handle mismatch:0x%x,0x%x\n", rep->handle, usound->broadcast_dev_handle);
		return -1;
	}

	SYS_LOG_INF("handle 0x%x, id %d\n", rep->handle, rep->id);
	usound->chan = NULL;
	usound->broadcast_source_enabled = 0;
	usound->num_of_broad_chan = 0;
	usound->broadcast_dev_handle = 0;
	memset(usound->broad_chan,0,sizeof(usound->broad_chan));

	if(usound->auracast_pending) {
		usound->auracast_pending = 0;
		usound_switch_auracast();
	}

	return 0;
}

static void usound_switch_auracast()
{
	struct usound_app_t *usound = usound_get_app();

	if (usound_is_bms_mode()) {
		usound_stop_playback();

		bms_uac_stop_capture();
		bms_uac_exit_capture();

		usound->tx_start = 0;
		usound->tx_sync_start = 0;

		usound_set_auracast_mode(false);
#ifdef CONFIG_PLAYTTS
		//player will be started after tts restart.
#else
		usound_play_resume();
#endif
	} else {
#ifdef CONFIG_NO_AURACST_ON_LE_CONNECTED
		if (bt_manager_audio_get_leaudio_dev_num()) {
			SYS_LOG_ERR("LE conneted!");
			return;
		}
#endif

		//bis maybe in releasing
		if (!usound->chan) {
			if (usound->playback_player_run) {
				usound_stop_playback();
			}
			usound_set_auracast_mode(true);
		} else {
			SYS_LOG_INF("Wait bis releasing");
			usound->auracast_pending = 1;
		}
	}
}

void bms_uac_bt_event_proc(struct app_msg *msg)
{
	SYS_LOG_INF("cmd: %d\n", msg->cmd);

	switch (msg->cmd) {
	case BT_REQ_RESTART_PLAY:
		SYS_LOG_WRN("skip BT_REQ_RESTART_PLAY\n");
		break;

	case BT_BROADCAST_SOURCE_CONFIG:
		bms_uac_handle_source_config(msg->ptr);
		break;
	case BT_BROADCAST_SOURCE_ENABLE:
		bms_uac_handle_source_enable(msg->ptr);
		break;
	case BT_BROADCAST_SOURCE_DISABLE:
		bms_uac_handle_source_disable(msg->ptr);
		break;
	case BT_BROADCAST_SOURCE_RELEASE:
		bms_uac_handle_source_release(msg->ptr);
		break;

	case BT_BIS_CONNECTED:
		SYS_LOG_INF("bis connected");
		break;
	case BT_BIS_DISCONNECTED:
		SYS_LOG_INF("bis disconnected");
		break;
	default:
		break;
	}
}
#endif

#ifdef CONFIG_USOUND_FEATURE_RESTART
static int usound_handle_restart(void)
{
	struct usound_app_t *usound = usound_get_app();

	SYS_LOG_INF("%d", usound->playing);

	usound_play_pause();

	// clear restart flag to avoid restart handler to do dumplicated handling.
	usound->restart = 0;

	usound_play_resume();

	return 0;
}
#endif

static void usound_vol_request_host(bool inc, u8_t level)
{
	struct usound_app_t *usound = usound_get_app();
	if (NULL == usound) {
		return;
	}
	SYS_LOG_INF("%d, %d", inc, level);

	usound->volume_req_level = level;
	if(inc) {
		usound->volume_req_type = USOUND_VOLUME_INC;
		usb_hid_control_volume_inc();
	} else {
		usound->volume_req_type = USOUND_VOLUME_DEC;
		usb_hid_control_volume_dec();
	}
}

static void usound_vol_step_adjust(bool inc)
{
	struct usound_app_t *usound = usound_get_app();
	u8_t level;

	if (!usound) {
		return;
	}

	level = usound->current_volume_level;
	SYS_LOG_INF("%d, %d", inc, level);
	if(inc) {
		if (level < audio_policy_get_volume_level()) {
			level += 1;
		}
	} else {
		if (level >= 1) {
			level -= 1;
		}
	}

	usound_vol_request_host(inc, level);
}

static void usound_vol_update(u8_t level)
{
	struct usound_app_t *usound = usound_get_app();
	u8_t cl;

	if (!usound) {
		return;
	}

	if (level > audio_policy_get_volume_level()) {
		SYS_LOG_WRN("wrong level %d", level);
		level = audio_policy_get_volume_level();
	}
	cl = usound->current_volume_level;
	SYS_LOG_INF("%d->%d", cl, level);

	if(level > cl) {
		usound_vol_request_host(true, level);
	} else if(level < cl) {
		usound_vol_request_host(false, level);
	} else {
	}
}

void usound_input_event_proc(struct app_msg *msg)
{
	struct usound_app_t *usound = usound_get_app();

	if (!usound)
		return;

	SYS_LOG_INF("cmd %d", msg->cmd);
	switch (msg->cmd) {
	case MSG_USOUND_PLAY_PAUSE_RESUME:
		if (usound->playing) {
			usb_hid_control_pause_play();
			sys_event_notify(SYS_EVENT_PLAY_PAUSE);
		} else {
			sys_event_notify(SYS_EVENT_PLAY_START);
			usb_hid_control_pause_play();
		}
		break;
	case MSG_USOUND_PLAY_VOLUP:
		usound_vol_step_adjust(true);
		break;
	case MSG_USOUND_PLAY_VOLDOWN:
		usound_vol_step_adjust(false);
		break;
	case MSG_USOUND_PLAY_NEXT:
		sys_event_notify(SYS_EVENT_PLAY_NEXT);
		usb_hid_control_play_next();
		break;
	case MSG_USOUND_PLAY_PREV:
		sys_event_notify(SYS_EVENT_PLAY_PREVIOUS);
		usb_hid_control_play_prev();
		break;
#ifdef CONFIG_BMS_UAC_APP
	case MSG_SWITCH_BROADCAST:
		if (!app_tws_status_get_enable()) {
			usound_switch_auracast();
		} else {
			SYS_LOG_INF("Please exit TWS mode.\n");
		}
		break;
	case MSG_AURACAST_ENTER:
		if(system_app_get_auracast_mode() == 0){
			usound_switch_auracast();
		} else if(system_app_get_auracast_mode() == 1){
			usound_handle_restart();
		}
		break;
	case MSG_AURACAST_EXIT:
		if(system_app_get_auracast_mode() != 0){
			usound_switch_auracast();
		}
		break;
#endif
	default:
		break;
	}
}

static int usound_sink_start(void)
{
	struct usound_app_t *usound = usound_get_app();

	SYS_LOG_INF("%d", usound->playing);
	usound->playing = 1;

	usound_play_resume();
	usound_show_play_status(true);

#ifdef CONFIG_ESD_MANAGER
	{
		u8_t playing = usound->playing;
		esd_manager_save_scene(TAG_PLAY_STATE, &playing, 1);
	}
#endif

	return 0;
}

static int usound_sink_stop(void)
{
	struct usound_app_t *usound = usound_get_app();

	SYS_LOG_INF("%d", usound->playing);
	usound->playing = 0;

	usound_play_pause();

	usound_show_play_status(false);
#ifdef CONFIG_ESD_MANAGER
	{
		u8_t playing = usound->playing;
		esd_manager_save_scene(TAG_PLAY_STATE, &playing, 1);
	}
#endif

	return 0;
}

static int usound_set_mute(bool mute)
{
	if(mute) {
		audio_system_set_stream_mute(AUDIO_STREAM_USOUND, 1);
#ifdef CONFIG_TWS
		bt_manager_tws_sync_volume_to_slave(NULL, AUDIO_STREAM_USOUND, 0);
#endif
	} else {
		audio_system_set_stream_mute(AUDIO_STREAM_USOUND, 0);
#ifdef CONFIG_TWS
		bt_manager_tws_sync_volume_to_slave(NULL, AUDIO_STREAM_USOUND, 
			audio_system_get_stream_volume(AUDIO_STREAM_USOUND));
#endif
	}
	return 0;
}

static void usound_sync_host_vol(int vol)
{
	u8_t level;
	bool update = true;
	struct usound_app_t *usound = usound_get_app();

	if (NULL == usound) {
		return;
	}

	//vol = usb_host_sync_volume_to_device(vol);
	level = (u8_t) audio_policy_get_volume_level_by_db(AUDIO_STREAM_USOUND, vol);
	SYS_LOG_INF("vol %ddB, level %d", vol, level);
	SYS_LOG_INF("req %d %d", usound->volume_req_type, usound->volume_req_level);

	usound->current_volume_level = level;
	usound_view_volume_show(level);

	switch (usound->volume_req_type)
	{
	case USOUND_VOLUME_NONE:
		break;
	case USOUND_VOLUME_INC:
		if (usound->volume_req_level > level) {
			usb_hid_control_volume_inc();
			update = false;
		} else {
			usound->volume_req_type = USOUND_VOLUME_NONE;
		}
		break;
	case USOUND_VOLUME_DEC:
		if (usound->volume_req_level < level) {
			usb_hid_control_volume_dec();
			update = false;
		} else {
			usound->volume_req_type = USOUND_VOLUME_NONE;
		}
		break;
	default:
		SYS_LOG_WRN("Wrong type.");
		break;
	}

	if(update) {
		system_volume_set(AUDIO_STREAM_USOUND, level, false);
#ifdef CONFIG_BMS_UAC_APP
#if ENABLE_PADV_APP
		padv_tx_data(padv_volume_map(level, 1));
#endif
#endif
	}
}

#ifdef CONFIG_USOUND_MIC
int usound_set_mic_gain(int gain)
{
	struct usound_app_t *usound = usound_get_app();

	if (NULL == usound) {
		return -1;
	}

	usound->mic_gain = gain;

	//Convert gain(in 0.1 dB) range from PC audacity app to audio driver.
	gain = (gain + 601) * 840 / 601;
	if(gain > 840) {
		gain = 840;
	} else if (gain < 260) {
		gain = 260;
	}
	audio_system_set_microphone_volume(AUDIO_STREAM_DEFAULT, gain);

	return 0;
}
int usound_set_mic_mute(bool mute)
{
	struct usound_app_t *usound = usound_get_app();

	if (NULL == usound) {
		return -1;
	}

	audio_system_mute_microphone(mute);

	return 0;
}
#endif

void usound_app_event_proc(struct app_msg *msg)
{
	SYS_LOG_INF("cmd:%d value:%d", msg->cmd, msg->value);

	switch (msg->cmd) {
	case MSG_USOUND_STREAM_START:
		usound_sink_start();
		break;
	case MSG_USOUND_STREAM_STOP:
		usound_sink_stop();
		break;
	case MSG_USOUND_STREAM_VOLUME:
		usound_sync_host_vol(msg->value);
		break;
	case MSG_USOUND_STREAM_MUTE:
		usound_set_mute(1);
		break;
	case MSG_USOUND_STREAM_UNMUTE:
		usound_set_mute(0);
		break;
#ifdef CONFIG_USOUND_FEATURE_RESTART
	case MSG_USOUND_STREAM_RESTART:
		usound_handle_restart();
		break;
#endif
	case MSG_USOUND_VOL_UPDATE:
		usound_vol_update(msg->value);
		break;
#ifdef CONFIG_USOUND_MIC
	case MSG_USOUND_UPLOAD_STREAM_START:
//      usound_source_start();
		usound_start_capture();
		break;
	case MSG_USOUND_UPLOAD_STREAM_STOP:
//		usound_source_stop();
		usound_stop_capture();
		break;
	case MSG_USOUND_UPLOAD_STREAM_VOLUME:
		usound_set_mic_gain(msg->value);
		break;
	case MSG_USOUND_UPLOAD_STREAM_MUTE:
		usound_set_mic_mute(1);
		break;
	case MSG_USOUND_UPLOAD_STREAM_UNMUTE:
		usound_set_mic_mute(0);
		break;
#endif
	}
}
