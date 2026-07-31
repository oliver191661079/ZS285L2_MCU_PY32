/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt music event
 */
#include "btmusic.h"
#include <ui_manager.h>
#include <esd_manager.h>
#include "tts_manager.h"
#ifdef CONFIG_PROPERTY
#include <property_manager.h>
#endif

#ifdef CONFIG_ACT_EVENT
#include <app_act_event_id.h>
#include <logging/log_core.h>
LOG_MODULE_DECLARE(main, CONFIG_ACT_EVENT_APP_COMPILE_LEVEL);
#endif

#ifdef CONFIG_BTMUSIC_BMS_APP
static void btmusic_switch_auracast(void);

/* find the existed broad chan */
static struct bt_broadcast_chan *find_broad_chan(uint32_t handle, uint8_t id)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
	struct bt_broadcast_chan *chan;
	int i;

	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		chan = &btmusic->broad_chan[i];
		if (chan->handle == handle && chan->id == id) {
			return chan;
		}
	}
	return NULL;
}

/* find a new broad chan */
static struct bt_broadcast_chan *new_broad_chan(uint32_t handle, uint8_t id)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
	struct bt_broadcast_chan *chan;
	int i;

	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		chan = &btmusic->broad_chan[i];
		if (chan->handle == 0 && chan->id == 0) {
			return chan;
		}
	}
	return NULL;
}
#endif

static int btmusic_handle_enable(struct bt_audio_report *rep)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
	struct bt_audio_chan *br_sink_chan = &btmusic->sink_chan;

	if (BT_TYPE_BR == bt_manager_audio_get_type(rep->handle)) {
		br_sink_chan->handle = rep->handle;
		br_sink_chan->id = rep->id;
		SYS_EVENT_INF(EVENT_BTMUSIC_STREAM_ENABLE,br_sink_chan->handle,br_sink_chan->id);
		/* Defer to BT_AUDIO_STREAM_START (handle_start) */
		return 0;
	}

	return -EINVAL;
}

static int btmusic_handle_disable(struct bt_audio_report *rep)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
	struct bt_audio_chan *br_sink_chan = &btmusic->sink_chan;

	SYS_LOG_INF("\n");

	if ((rep->handle == br_sink_chan->handle) &&
	    (rep->id == br_sink_chan->id)) {
		SYS_LOG_INF("BR\n");
		return 0;
	}

	return -EINVAL;
}

static int btmusic_handle_start(struct bt_audio_report *rep)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
	struct bt_audio_chan *br_sink_chan = &btmusic->sink_chan;

	SYS_LOG_INF("h:0x%x id:%d\n", rep->handle, rep->id);

	if ((rep->handle == br_sink_chan->handle) &&
	    (rep->id == br_sink_chan->id)) {
		btmusic->playing = 1;
		btmusic->ios_dev = bt_manager_audio_is_ios_dev(rep->handle);

		SYS_LOG_INF("[dbg] start h=0x%x id=%d player=%p run=%d sbc=%d tts=%d chan=0x%x",
			    rep->handle, rep->id,
			    btmusic->playback_player,
			    btmusic->playback_player_run,
			    btmusic->sbc_playing,
			    btmusic->tts_playing,
			    btmusic->sink_chan.handle);
#ifdef CONFIG_BTMUSIC_BMS_APP
		if(btmusic_get_auracast_mode() && btmusic->bms_source){
#ifdef CONFIG_BT_MUSIC_AUTO_SWITCH_BMR
			if (thread_timer_is_running(&btmusic->broadcast_switch_timer)){
				thread_timer_stop(&btmusic->broadcast_switch_timer);
			}
#endif
			if (!btmusic->chan) {
				SYS_LOG_INF("broad_chan not config:\n");
				return -EINVAL;
			}else{
#ifdef CONFIG_BT_MUSIC_AUTO_SWITCH_BMR
				if(!btmusic->broadcast_source_enabled){
					bt_manager_broadcast_source_enable(btmusic->chan->handle);
				}
#endif
			}
		}

#endif

		if(btmusic->tts_playing){
			SYS_LOG_INF("tts playing\n");
			return -EINVAL;
		}
		/*
		 * btmusic_init_playback() returns -EALREADY if player
		 * already exists (from prior BT_AUDIO_STREAM_CONFIG_CODEC).
		 * Still call start_playback to restart after any
		 * intermediate stop/reconfigure event.
		 */
		btmusic_init_playback();
		btmusic_start_playback();

#ifdef CONFIG_BTMUSIC_BMS_APP
		if(btmusic_get_auracast_mode() && btmusic->bms_source){
			if (btmusic->broadcast_source_enabled &&
				!btmusic->capture_player_run) {
				btmusic_bms_init_capture();
				btmusic_bms_start_capture();

				bt_manager_audio_sink_stream_set(&btmusic->sink_chan,
								 btmusic->sink_stream);
			}
		}
#endif
		btmusic->ios_dev = 0;
		return 0;
	}

	return -EINVAL;
}

static int btmusic_handle_stop(struct bt_audio_report *rep)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
	struct bt_audio_chan *br_sink_chan = &btmusic->sink_chan;

	if ((rep->handle == br_sink_chan->handle) &&
	    (rep->id == br_sink_chan->id)) {
		btmusic->playing = 0;
		btmusic_stop_playback();
		/* always exit playback for BR */
		btmusic_exit_playback();
		SYS_EVENT_INF(EVENT_BTMUSIC_STREAM_STOP,br_sink_chan->handle,br_sink_chan->id);
#ifdef CONFIG_BTMUSIC_BMS_APP
		if(btmusic_get_auracast_mode()){
			if (btmusic->capture_player_run) {
				btmusic_bms_stop_capture();
				btmusic_bms_exit_capture();
			}
			btmusic->tx_start = 0;
			btmusic->tx_sync_start = 0;
			if (!app_tws_status_get_enable()) {
#ifdef CONFIG_BT_MUSIC_AUTO_SWITCH_BMR
				if (bt_manager_a2dp_reconnect_status_get()) {
					thread_timer_start(&btmusic->broadcast_switch_timer, 8000, 0);
				} else {
					thread_timer_start(&btmusic->broadcast_switch_timer, 1000, 0);
				}
#endif
			}
		}
#endif

		return 0;
	}

	return -EINVAL;
}

static int btmusic_handle_release(struct bt_audio_report *rep)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
	struct bt_audio_chan *br_sink_chan = &btmusic->sink_chan;

	if ((rep->handle == br_sink_chan->handle) &&
	    (rep->id == br_sink_chan->id)) {
	    btmusic->playing = 0;
		btmusic_stop_playback();
		/* always exit playback for BR */
		btmusic_exit_playback();
		return 0;
	}

	return -EINVAL;
}

static void btmusic_handle_disconnect(uint16_t * handle)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
	if(*handle == btmusic->sink_chan.handle)
		memset(&btmusic->sink_chan, 0, sizeof(struct bt_audio_chan));
}

static void btmusic_handle_media_connect(void)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();

	SYS_LOG_INF("playing: %d\n", btmusic->playing);

#ifdef CONFIG_ESD_MANAGER
	btmusic_esd_restore();
#endif
	btmusic->media_ctrl_connected = 1;
}

static void btmusic_handle_playback_status(struct bt_media_play_status *status)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();

	SYS_LOG_INF("%d\n", status->status);

	if (status->status == BT_STATUS_PAUSED) {
		SYS_LOG_INF("pause\n");
		btmusic->media_state = 0;
	} else if (status->status == BT_STATUS_PLAYING) {
		SYS_LOG_INF("play\n");
		btmusic->media_state = 1;
		btmusic->playing = 1;
		/*
		 * AVRCP PLAY 可能先于 A2DP codec 配置到达，此时 sink_chan
		 * 尚未填充，btmanager 会报 "no chan"。这里只设置标记位，
		 * 让后续 BT_AUDIO_STREAM_CONFIG_CODEC 或 STREAM_ENABLE
		 * 事件来实际触发 btmusic_init_playback + start_playback。
		 */
		if (!btmusic->tts_playing && !btmusic->playback_player
		    && btmusic->sink_chan.handle != 0) {
			if (btmusic_init_playback() == 0) {
				btmusic_start_playback();
			}
		}
		bt_manager_audio_stream_restore(BT_TYPE_BR);
	}
}

#if CONFIG_BTMUSIC_BMS_APP
static void btmusic_bms_tx_start(uint32_t handle, uint8_t id, uint16_t audio_handle)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();

	if (!btmusic || btmusic->tx_start) {
		return;
	}

	if (btmusic->broadcast_source_enabled && btmusic->capture_player) {
		media_player_trigger_audio_record_start(btmusic->capture_player);
		btmusic->tx_start = 1;
		//Time sensitive, short log to save time.
		printk("[" SYS_LOG_DOMAIN "] trigger record\n");
	}
}

void btmusic_bms_tx_sync_start(uint32_t handle, uint8_t id,
				 uint16_t audio_handle)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();

	if (!btmusic || btmusic->tx_sync_start) {
		return;
	}

	if (btmusic->broadcast_source_enabled && btmusic->playback_player) {
		media_player_trigger_audio_track_start(btmusic->playback_player);
		btmusic->tx_sync_start = 1;
		//Time sensitive, short log to save time.
		printk("[" SYS_LOG_DOMAIN "] trigger track\n");
	}
}

static int btmusic_bms_handle_source_config(struct bt_braodcast_configured *rep)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
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
		btmusic->num_of_broad_chan++;
	}

	chan->handle = rep->handle;
	chan->id = rep->id;

	vnd_buf[0] = VS_ID & 0xFF;
	vnd_buf[1] = VS_ID >> 8;
	memcpy(&vnd_buf[2], btmusic->broadcast_code, 16);
	vnd_buf[18] = SERIVCE_UUID & 0xFF;
	vnd_buf[19] = SERIVCE_UUID >> 8;

#if !VS_COMPANY_ID
	type = BT_DATA_SVC_DATA16;
#endif

	if (btmusic->num_of_broad_chan == 1) {
		SYS_EVENT_INF(EVENT_BTMUSIC_BIS_SOURCE_CONFIG,chan->handle,chan->id);
		bt_manager_broadcast_stream_cb_register(chan, btmusic_bms_tx_start,
							NULL);
		//bt_manager_broadcast_stream_tws_sync_cb_register(chan,
		//						 btmusic_bms_tx_sync_start);
		bt_manager_broadcast_stream_set_tws_sync_cb_offset(chan,
								   broadcast_get_tws_sync_offset
								   (btmusic->qos));
		bt_manager_broadcast_source_vnd_ext_send(chan->handle, vnd_buf,
							 sizeof(vnd_buf), type);
		btmusic->chan = chan;
		bt_manager_audio_stream_restore(BT_TYPE_BR);
	}

#ifndef CONFIG_BT_MUSIC_AUTO_SWITCH_BMR
	if(app_tws_status_get_enable()) {
		bt_manager_broadcast_source_enable(btmusic->chan->handle);
	} else {
		if (btmusic->num_of_broad_chan >= NUM_OF_BROAD_CHAN) {
			bt_manager_broadcast_source_enable(btmusic->chan->handle);
		}
	}
#endif
	return 0;
}

static int btmusic_bms_handle_source_enable(struct bt_broadcast_report *rep)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();

	if (NULL == btmusic) {
		return -1;
	}

	SYS_LOG_INF("handle 0x%x, id %d\n", rep->handle, rep->id);
	if (NULL == btmusic->chan) {
		SYS_LOG_INF("no chan.");
		return -2;
	}

	if (rep->id != btmusic->chan->id) {
		SYS_LOG_INF("Skip this chan.");
		return -3;
	}

	SYS_EVENT_INF(EVENT_BTMUSIC_BIS_SOURCE_ENABLE,btmusic->chan->handle,btmusic->chan->id);
	btmusic->broadcast_source_enabled = 1;

#if ENABLE_PADV_APP
	padv_tx_init(btmusic->chan->handle, AUDIO_STREAM_SOUNDBAR);
#endif

	if(btmusic->tts_playing){
		SYS_LOG_INF("tts playing\n");
		return -4;
	}

	if (!btmusic->playing){
		SYS_LOG_INF("br stream stoped\n");
		return -5;
	}

	btmusic_bms_init_capture();
	btmusic_bms_start_capture();

	bt_manager_audio_sink_stream_set(&btmusic->sink_chan,
					 btmusic->sink_stream);

	return 0;
}

static int btmusic_bms_handle_source_disable(struct bt_broadcast_report *rep)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();

	if ((rep->handle != btmusic->broadcast_dev_handle) &&
	    (btmusic->broadcast_dev_handle != 0))
	{
		SYS_LOG_WRN("Handle mismatch:0x%x,0x%x\n", rep->handle, btmusic->broadcast_dev_handle);
		return -1;
	}

#if ENABLE_PADV_APP
	padv_tx_deinit();
#endif

	btmusic_bms_stop_capture();
	btmusic_bms_exit_capture();

	btmusic->tx_start = 0;
	btmusic->tx_sync_start = 0;

	struct bt_broadcast_chan * chan = find_broad_chan(rep->handle, rep->id);
	if(chan){
		if(btmusic->num_of_broad_chan){
			btmusic->num_of_broad_chan--;
			chan->handle = 0;
			chan->id = 0;
			if(chan == btmusic->chan){
				btmusic->chan = NULL;
			}
			if(!btmusic->num_of_broad_chan){
				btmusic->broadcast_source_enabled = 0;
				SYS_LOG_INF("\n");
			}
		}else{
			SYS_LOG_WRN("should not be here\n");
		}
	}

	return 0;
}

static int btmusic_bms_handle_source_release(struct bt_broadcast_report *rep)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();

	if (NULL == btmusic) {
		return -1;
	}

	if ((rep->handle != btmusic->broadcast_dev_handle) &&
	    (btmusic->broadcast_dev_handle != 0))
	{
		SYS_LOG_WRN("Handle mismatch:0x%x,0x%x\n", rep->handle, btmusic->broadcast_dev_handle);
		return -1;
	}

	btmusic->chan = NULL;
	btmusic->broadcast_source_enabled = 0;
	btmusic->num_of_broad_chan = 0;
	btmusic->broadcast_dev_handle = 0;
	memset(btmusic->broad_chan,0,sizeof(btmusic->broad_chan));
	if(btmusic->auracast_pending) {
		btmusic->auracast_pending = 0;
		btmusic_switch_auracast();
	}
	SYS_LOG_INF("\n");
	SYS_EVENT_INF(EVENT_BTMUSIC_BIS_SOURCE_RELEASE);

	return 0;
}
#endif

static int btmusic_handle_volume_value(struct bt_volume_report *rep)
{
	SYS_LOG_INF("%d\n", rep->value);
#if ENABLE_PADV_APP
	if(btmusic_get_auracast_mode())
		padv_tx_data(padv_volume_map(rep->value,1));
#endif
	return 0;
}

#ifdef CONFIG_BTMUSIC_BMS_APP
static bool btmusic_expect_auracast_primary(void)
{
	bool primary = false;

	if(app_tws_status_get_mode() != APP_TWS_MODE_NONE) {
		if(app_tws_status_get_role() == APP_TWS_ROLE_PRIMARY) {
			primary = true;
		}
	}

#ifdef CONFIG_BT_MUSIC_AUTO_SWITCH_BMR
	if(btmusic->playing) {
		primary = true;
	}
#else
	if (bt_manager_get_connected_dev_num() > 0) {
		primary = true;
	}
#endif

	SYS_LOG_INF("%d", primary);
	return primary;
}

static void btmusic_switch_auracast(void)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
	if(btmusic_get_auracast_mode()){
		btmusic_stop_playback();
		btmusic_exit_playback();

		btmusic_bms_stop_capture();
		btmusic_bms_exit_capture();

		btmusic->tx_start = 0;
		btmusic->tx_sync_start = 0;

		btmusic_set_auracast_mode(0);
		bt_manager_audio_stream_restore(BT_TYPE_BR);
	}else{
#ifdef CONFIG_NO_AURACST_ON_LE_CONNECTED
		if(bt_manager_audio_get_leaudio_dev_num()){
			SYS_LOG_ERR("LE conneted!");
			return;
		}
#endif

		if(btmusic_expect_auracast_primary()) {
			//bis maybe in releasing
			if (!btmusic->chan) {
				if (btmusic->playback_player_run) {
					btmusic_stop_playback();
					btmusic_exit_playback();
				}
				btmusic_set_auracast_mode(1);
			} else {
				SYS_LOG_INF("Wait bis releasing");
				btmusic->auracast_pending = 1;
			}
		} else {
			system_app_set_auracast_mode(2);
			system_app_launch_add(DESKTOP_PLUGIN_ID_BMR);
		}
	}
}
#endif

void btmusic_bt_event_proc(struct app_msg *msg)
{
	struct bt_audio_report rep;
	struct btmusic_app_t *btmusic =
	    (struct btmusic_app_t *)btmusic_get_app();
#ifdef CONFIG_ESD_MANAGER
	u8_t playing = 0;
#endif

	if (NULL == btmusic) {
		return;
	}
	SYS_LOG_INF("bt event %d\n", msg->cmd);
	switch (msg->cmd) {
	case BT_CONNECTED:
		btmusic_view_show_connected();
		break;
	case BT_DISCONNECTED:
		btmusic_view_show_disconnected();
		btmusic_handle_disconnect(msg->ptr);
		if (app_tws_status_get_enable()) {
			bt_manager_set_user_visual(true,false,true,0);
		}
		break;
	case BT_AUDIO_STREAM_ENABLE:
		btmusic_handle_enable(msg->ptr);
		break;
	case BT_AUDIO_STREAM_CONFIG_CODEC:
	{
		struct bt_audio_codec *codec = (struct bt_audio_codec *)msg->ptr;

		if (codec && BT_TYPE_BR == bt_manager_audio_get_type(codec->handle)) {
			btmusic->sink_chan.handle = codec->handle;
			btmusic->sink_chan.id = codec->id;
			/*
			 * Only store codec handle/id here. Actual
			 * player init+start is deferred to
			 * BT_AUDIO_STREAM_START (handle_start)
			 * after codec negotiation completes.
			 */
		}
		break;
	}
	case BT_AUDIO_STREAM_UPDATE:
		break;
	case BT_AUDIO_STREAM_DISABLE:
		btmusic_handle_disable(msg->ptr);
		break;
	case BT_AUDIO_STREAM_START:
		btmusic_handle_start(msg->ptr);
#ifdef CONFIG_ESD_MANAGER
		playing = btmusic->playing;
		esd_manager_save_scene(TAG_PLAY_STATE, &playing, 1);
#endif
		break;
	case BT_AUDIO_STREAM_STOP:
		btmusic_handle_stop(msg->ptr);
#ifdef CONFIG_ESD_MANAGER
		playing = btmusic->playing;
		esd_manager_save_scene(TAG_PLAY_STATE, &playing, 1);
#endif
		break;
	case BT_AUDIO_STREAM_RELEASE:
		btmusic_handle_release(msg->ptr);
		break;
	case BT_VOLUME_VALUE:
		btmusic_handle_volume_value(msg->ptr);
		break;

	case BT_MEDIA_CONNECTED:
		btmusic_handle_media_connect();
		break;
	case BT_MEDIA_DISCONNECTED:
		btmusic->media_ctrl_connected = 0;
		break;
	case BT_MEDIA_PLAYBACK_STATUS_CHANGED_EVENT:
		btmusic_handle_playback_status((struct bt_media_play_status *)
					      msg->ptr);
		break;
	/* BT Audio switched, need to restore */
	case BT_AUDIO_SWITCH:
	case BT_REQ_RESTART_PLAY:
		SYS_EVENT_INF(EVENT_BTMUSIC_RESTART_PLAY);
		if(!btmusic_get_auracast_mode()){
			if (!btmusic->tts_playing) {
				rep.handle = btmusic->sink_chan.handle;
				rep.id = btmusic->sink_chan.id;

				btmusic_handle_stop(&rep);
				btmusic_handle_release(&rep);
				bt_manager_audio_stream_restore(BT_TYPE_BR);
			}
		}else{
			// Do not restart player when bt data is not enough.
			SYS_LOG_INF("skip BT_REQ_RESTART_PLAY");
		}
		break;

#ifdef CONFIG_BTMUSIC_BMS_APP
		/* Broadcast Source */
	case BT_BROADCAST_SOURCE_CONFIG:
		btmusic_bms_handle_source_config(msg->ptr);
		break;
	case BT_BROADCAST_SOURCE_ENABLE:
		btmusic_bms_handle_source_enable(msg->ptr);
		break;
	case BT_BROADCAST_SOURCE_DISABLE:
		btmusic_bms_handle_source_disable(msg->ptr);
		break;
	case BT_BROADCAST_SOURCE_RELEASE:
		btmusic_bms_handle_source_release(msg->ptr);
		break;
	case BT_BIS_CONNECTED:
		SYS_LOG_INF("bis connected");
		break;
	case BT_BIS_DISCONNECTED:
		SYS_LOG_INF("bis disconnected");
		break;
#endif
	default:
		break;
	}
}

static int btmusic_handle_client_volume(bool up)
{
	int ret;
	u8_t max;
	u8_t vol;
	int stream_type = AUDIO_STREAM_MUSIC;

#ifdef CONFIG_BTMUSIC_BMS_APP
	if(btmusic_get_auracast_mode())
		stream_type = AUDIO_STREAM_SOUNDBAR;
#endif

	ret = system_volume_get(stream_type);
	if (ret < 0) {
		SYS_LOG_ERR("%d\n", ret);
		return -EINVAL;
	} else {
		vol = ret;
	}

	max = audio_policy_get_volume_level();

	if (up) {
		vol += 1;
		if (vol > max) {
			vol = max;
		}
	} else {
		if (vol >= 1) {
			vol -= 1;
		}
	}

	SYS_LOG_INF("vol=%d/%d\n", vol, max);
	system_volume_set(stream_type, vol, true);

#if ENABLE_PADV_APP
	if(btmusic_get_auracast_mode())
		padv_tx_data(padv_volume_map(vol,1));
#endif

	return 0;
}

static void btmusic_player_reset(void)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();

	SYS_EVENT_INF(EVENT_BTMUSIC_PLAYER_RESET);

	//stop
	if (btmusic->playback_player_run) {
		btmusic_stop_playback();
		btmusic_exit_playback();
	}

#ifdef CONFIG_BTMUSIC_BMS_APP
	if (btmusic->capture_player_run) {
		btmusic_bms_stop_capture();
		btmusic_bms_exit_capture();
	}
	btmusic->tx_start = 0;
	btmusic->tx_sync_start = 0;
#endif
	//clear restart flag to avoid restart handler to do dumplicated handling.
	btmusic->restart = 0;

	if(btmusic->tts_playing){
		SYS_LOG_INF("tts playing\n");
		return;
	}

	if (!btmusic->playing){
		SYS_LOG_INF("br stream stoped\n");
		return;
	}

	//wait a while (use a timer?)
	//os_sleep(10);

	//start
#ifdef CONFIG_BTMUSIC_BMS_APP
	if(btmusic_get_auracast_mode()){
		if (!btmusic->playback_player_run && btmusic->chan) {
			btmusic_init_playback();
			btmusic_start_playback();
		}
		if (btmusic->broadcast_source_enabled) {
			if (!btmusic->capture_player_run) {
				btmusic_bms_init_capture();
				btmusic_bms_start_capture();

				bt_manager_audio_sink_stream_set(&btmusic->sink_chan,
								btmusic->sink_stream);
			}
		} else if (1 == btmusic_get_auracast_mode() && 
					0 == btmusic->bms_source) {
			bt_manager_audio_stream_restore(BT_TYPE_BR);
		}
	}else{
		bt_manager_audio_stream_restore(BT_TYPE_BR);
	}
#else
	bt_manager_audio_stream_restore(BT_TYPE_BR);
#endif
}

void btmusic_input_event_proc(struct app_msg *msg)
{
	struct btmusic_app_t *btmusic =
	    (struct btmusic_app_t *)btmusic_get_app();

	SYS_LOG_INF("cmd: %d\n", msg->cmd);

	switch (msg->cmd) {
	case MSG_BT_PLAY_PAUSE_RESUME:
		bt_manager_media_playpause();
		break;

	case MSG_BT_PLAY_NEXT:
		bt_manager_media_play_next();
		break;

	case MSG_BT_PLAY_PREVIOUS:
		bt_manager_media_play_previous();
		break;

	case MSG_BT_PLAY_VOLUP:
		btmusic_handle_client_volume(true);
		break;

	case MSG_BT_PLAY_VOLDOWN:
		btmusic_handle_client_volume(false);
		break;

	case MSG_BT_PLAY_SEEKFW_START:
		if (btmusic->playing) {
			bt_manager_media_fast_forward(true);
		}
		break;

	case MSG_BT_PLAY_SEEKFW_STOP:
		if (btmusic->playing) {
			bt_manager_media_fast_forward(false);
		}
		break;

	case MSG_BT_PLAY_SEEKBW_START:
		if (btmusic->playing) {
			bt_manager_media_fast_rewind(true);
		}
		break;

	case MSG_BT_PLAY_SEEKBW_STOP:
		if (btmusic->playing) {
			bt_manager_media_fast_rewind(false);
		}
		break;

#ifdef CONFIG_BTMUSIC_BMS_APP
	case MSG_SWITCH_BROADCAST:
		if (!app_tws_status_get_enable()) {
			btmusic_switch_auracast();
		} else {
			SYS_LOG_INF("Please exit TWS mode.\n");
		}
		break;
	case MSG_AURACAST_ENTER:
		if(system_app_get_auracast_mode() == 0){
			btmusic_switch_auracast();
		} else if(system_app_get_auracast_mode() == 1){
			btmusic_player_reset();
		}
		break;
	case MSG_AURACAST_EXIT:
		if(system_app_get_auracast_mode() != 0){
			btmusic_switch_auracast();
		}
		break;
#endif
	default:
		break;
	}
}

#ifdef CONFIG_PLAYTTS
void btmusic_tts_event_proc(struct app_msg *msg)
{
	struct btmusic_app_t *btmusic =
	    (struct btmusic_app_t *)btmusic_get_app();

	if (!btmusic)
		return;

	SYS_LOG_INF("playing status %d %d\n", btmusic->capture_player_run,
		    btmusic->playback_player_run);

	switch (msg->value) {
	case TTS_EVENT_START_PLAY:
		if (btmusic->playback_player_run) {
			btmusic_stop_playback();
			btmusic_exit_playback();
		}

#ifdef CONFIG_BTMUSIC_BMS_APP
		if (btmusic->capture_player_run) {
			btmusic_bms_stop_capture();
			btmusic_bms_exit_capture();
		}
		btmusic->tx_start = 0;
		btmusic->tx_sync_start = 0;
#endif
		btmusic->tts_playing = true;
		break;
	case TTS_EVENT_STOP_PLAY:
		btmusic->tts_playing = false;
		if (!btmusic->playing){
			SYS_LOG_INF("br stream stoped\n");
			return;
		}
#ifdef CONFIG_BTMUSIC_BMS_APP
		if (btmusic_get_auracast_mode()){
			if (!btmusic->playback_player_run && btmusic->chan) {
				btmusic_init_playback();
				btmusic_start_playback();
			}
			if(btmusic->broadcast_source_enabled){
				if (!btmusic->capture_player_run) {
					btmusic_bms_init_capture();
					btmusic_bms_start_capture();

					bt_manager_audio_sink_stream_set(&btmusic->sink_chan,
									 btmusic->sink_stream);
				}
			} else if (1 == btmusic_get_auracast_mode() && 
					0 == btmusic->bms_source) {
				bt_manager_audio_stream_restore(BT_TYPE_BR);
			}
		}else{
			bt_manager_audio_stream_restore(BT_TYPE_BR);
		}
#else
		bt_manager_audio_stream_restore(BT_TYPE_BR);
#endif
		break;
	}
}
#endif

void btmusic_app_event_proc(struct app_msg *msg)
{
	SYS_LOG_INF("cmd: %d\n", msg->cmd);

	switch (msg->cmd) {
	case MSG_BTMUSIC_MESSAGE_CMD_PLAYER_RESET:
		btmusic_player_reset();
		break;
	}
}

