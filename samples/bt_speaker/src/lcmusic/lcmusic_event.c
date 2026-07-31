/*
 * Copyright (c) 2024 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief lcmusic app event
 */

#include "lcmusic.h"
#include "tts_manager.h"
#include <fs_manager.h>

#define MONITOR_TIME_PERIOD	OS_MSEC(500)	/* 500ms */

#ifdef CONFIG_SOUNDBAR_SAMPLE_RATE_FILTER
#define LCMUSIC_SOUNDBAR_MIN_SAMPLE_RATE	(44)	/* kHz */
#endif

#define SEEK_SPEED_LEVEL	(10)
static u32_t start_seek_time;

#ifdef CONFIG_BMS_LCMUSIC_APP
static void lcmusic_switch_auracast();
#endif

#ifdef CONFIG_LCMUSIC_APP_SWITCH_ON_ERROR
static bool _check_disk_plugout(void)
{
	u8_t stat = STA_NODISK;

#ifdef CONFIG_SOUNDBAR_LCMUSIC
	fs_disk_detect("USB:", &stat);
	if (stat != STA_DISK_OK)
		return true;
#else
#ifdef CONFIG_MUTIPLE_VOLUME_MANAGER
	if (strncmp
	    (app_manager_get_current_app(), APP_ID_UHOST_MPLAYER,
	     strlen(APP_ID_UHOST_MPLAYER)) == 0) {
		fs_disk_detect("USB:", &stat);
		if (stat != STA_DISK_OK)
			return true;
	} else
	    if (strncmp
		(app_manager_get_current_app(), APP_ID_SD_MPLAYER,
		 strlen(APP_ID_SD_MPLAYER)) == 0) {
		fs_disk_detect("SD:", &stat);
		if (stat != STA_DISK_OK)
			return true;
	}
#endif
#endif
	return false;

}
#endif

static void _lcmusic_force_play_next(struct lcmusic_app_t *lcmusic,
				     bool force_switch)
{
	struct app_msg msg = {
		.type = MSG_LCMUSIC_EVENT,
		.cmd = MSG_LCMPLAYER_AUTO_PLAY_NEXT,
	};

	SYS_LOG_INF("%d\n", force_switch);

	switch (lcmusic->mplayer_mode) {
	case MPLAYER_REPEAT_ONE_MODE:
		if (!force_switch)
			msg.cmd = MSG_LCMPLAYER_PLAY_CUR;
		break;
	default:
		break;
	}
	if (lcmusic->prev_music_state) {
		msg.type = MSG_INPUT_EVENT;
		msg.cmd = MSG_LCMPLAYER_PLAY_PREV;
		lcmusic->filter_prev_tts = 1;
	}
	/*fix app message lost:decoder thread sleep 20ms to reduce app message */
	os_sleep(20);
	send_async_msg(app_manager_get_current_app(), &msg);
}

static void _lcmusic_clear_breakpoint(struct lcmusic_app_t *lcmusic)
{
	lcmusic->music_bp_info.bp_info.file_offset = 0;
	lcmusic->music_bp_info.bp_info.time_offset = 0;
}

static void _lcmusic_play_event_callback(u32_t event, void *data, u32_t len,
					 void *user_data)
{
	struct lcmusic_app_t *lcmusic = lcmusic_get_app();

	if (!lcmusic)
		return;
	SYS_LOG_INF("event %d\n", event);

	switch (event) {
	case PLAYBACK_EVENT_STOP_ERROR:
		if (lcmusic->mplayer_state != MPLAYER_STATE_NORMAL)
			lcmusic->mplayer_state = MPLAYER_STATE_ERROR;
		lcmusic->music_state = LCMUSIC_STATUS_ERROR;
		_lcmusic_force_play_next(lcmusic, true);
		break;
	case PLAYBACK_EVENT_STOP_COMPLETE:
		lcmusic->prev_music_state = 0;
		lcmusic->mplayer_state = MPLAYER_STATE_NORMAL;
		lcmusic->full_cycle_times = 0;
		_lcmusic_force_play_next(lcmusic, false);
		break;

#ifdef CONFIG_LCMUSIC_FEATURE_RESTART
		if (event == PLAYBACK_EVENT_DATA_INDICATE) {
			bms_lcmusic_player_reset_trigger();
		}
#endif

	default:

		break;
	}
}

void lcmusic_bp_info_save(struct lcmusic_app_t *lcmusic)
{
	if (lcmusic->music_state == LCMUSIC_STATUS_NULL)
		return;
	mplayer_update_breakpoint(lcmusic->lcplayer,
				  &lcmusic->music_bp_info.bp_info);
	_lcmusic_bpinfo_save(lcmusic->cur_dir, &lcmusic->music_bp_info);
}

void lcmusic_stop_play(struct lcmusic_app_t *lcmusic, bool need_updata_bp)
{
	if (!lcmusic->lcplayer)
		return;

	if (need_updata_bp) {
		mplayer_update_breakpoint(lcmusic->lcplayer,
					  &lcmusic->music_bp_info.bp_info);
		mplayer_update_media_info(lcmusic->lcplayer,
					  &lcmusic->media_info);
	} else {
		_lcmusic_clear_breakpoint(lcmusic);
	}

	if (thread_timer_is_running(&lcmusic->monitor_timer))
		thread_timer_stop(&lcmusic->monitor_timer);

	if (lcmusic->music_state != LCMUSIC_STATUS_SEEK) {
		if (thread_timer_is_running(&lcmusic->seek_timer))
			thread_timer_stop(&lcmusic->seek_timer);
	}

	mplayer_stop_play(lcmusic->lcplayer);

	lcmusic->lcplayer = NULL;
	printk("%s %d lcplayer:%p bp:%d %d\n", __FUNCTION__, __LINE__,
	       lcmusic->lcplayer, lcmusic->music_bp_info.bp_info.time_offset, lcmusic->music_bp_info.bp_info.file_offset);

	if (lcmusic->music_state == LCMUSIC_STATUS_PLAYING)
		lcmusic->music_state = LCMUSIC_STATUS_STOP;
}

static int _lcmusic_esd_restore_bp_info(struct lcmusic_app_t *lcmusic,
					u8_t * play_state)
{
	int res = -EINVAL;

#ifdef CONFIG_ESD_MANAGER
	if (esd_manager_check_esd()) {
		res = 0;
		esd_manager_restore_scene(TAG_PLAY_STATE, play_state, 1);
		esd_manager_restore_scene(TAG_BP_INFO,
					  (u8_t *) & lcmusic->music_bp_info,
					  sizeof(lcmusic->music_bp_info));

		SYS_LOG_INF("restored");
		_lcmusic_bpinfo_dump(&lcmusic->music_bp_info);
	}
#endif
	return res;
}

static void _lcmusic_esd_save_bp_info(struct lcmusic_app_t *lcmusic,
				      u8_t is_need_update_cluster)
{
	mplayer_update_breakpoint(lcmusic->lcplayer,
				  &lcmusic->music_bp_info.bp_info);
	if (is_need_update_cluster) {
		if (lcmusic_get_cluster_by_url(lcmusic))
			SYS_LOG_INF("get cluster faild\n");
		//print_buffer(&lcmusic->music_bp_info, 1, sizeof(lcmusic->music_bp_info), 16, -1);
	}
#ifdef CONFIG_ESD_MANAGER
	esd_manager_save_scene(TAG_BP_INFO, (u8_t *) & lcmusic->music_bp_info,
			       sizeof(lcmusic->music_bp_info));
#endif
}

static void _lcmusic_monitor_timer(struct thread_timer *ttimer,
				   void *expiry_fn_arg)
{
	struct lcmusic_app_t *lcmusic = lcmusic_get_app();
	struct app_msg msg = { 0 };

	if (!lcmusic) {
		return;
	}

	if (strcmp(lcmusic->cur_dir, "SD:")) {
		if (usbh_is_scanning() != 2) {
			SYS_LOG_INF("usb scan not ready\n");
			thread_timer_start(&lcmusic-> monitor_timer, 200, 0);
			return;
		}
	}

	//fix me:if role master, heap memory may not enought, fs will init after tws disconnect
	if (bt_manager_tws_get_dev_role() != BTSRV_TWS_MASTER && !lcmusic->disk_init) {
#ifdef CONFIG_FS_MANAGER
		fs_manager_disk_init(lcmusic->cur_dir);
#ifdef CONFIG_DISKIO_CACHE
		if (!diskio_is_init() && !lcmusic->disk_init) {
			//init diskio
			diskio_cache_init();
		}
#endif
#endif
	}


	if (!lcmusic->disk_init) {
		SYS_LOG_INF("check volume %s, %d", lcmusic->cur_dir, lcmusic->check_usb_plug_time);
		if (fs_manager_get_volume_state(lcmusic->cur_dir)) {
			SYS_LOG_INF("Inited");
			lcmusic->check_usb_plug_time = 0;
			lcmusic->disk_init = 1;
			lcmusic->music_state = LCMUSIC_STATUS_STOP;
			msg.type = MSG_LCMUSIC_EVENT;
			msg.cmd = MSG_LCMPLAYER_INIT;
			send_async_msg(app_manager_get_current_app(), &msg);
		} else {
			lcmusic->check_usb_plug_time++;
			SYS_LOG_INF("no volume");
			thread_timer_start(&lcmusic-> monitor_timer, 200, 0);
		}

		return;
	}

	/*display seek progress */
	if (lcmusic->music_state == LCMUSIC_STATUS_SEEK) {
		if (lcmusic->seek_direction == SEEK_FORWARD) {
			lcmusic_display_play_time(lcmusic,
						  SEEK_SPEED_LEVEL *
						  (os_uptime_get_32() -
						   start_seek_time));
		} else if (lcmusic->seek_direction == SEEK_BACKWARD) {
			lcmusic_display_play_time(lcmusic,
						  -SEEK_SPEED_LEVEL *
						  (os_uptime_get_32() -
						   start_seek_time));
		}
	}

#ifdef	CONFIG_INPUT_DEV_ACTS_IRKEY
	if (lcmusic->num_play) {
		/*no display bp track_no dearing num play */
		lcmusic->filt_track_no = 1;
		if (++lcmusic->num_play_time > 8000 / MONITOR_TIME_PERIOD) {
			lcmusic->num_play_time = 0;
			lcmusic->num_play = 0;
			msg.type = MSG_LCMUSIC_EVENT;
			msg.cmd = MSG_LCMPLAYER_PLAY_TRACK_NO;
			send_async_msg(app_manager_get_current_app(), &msg);
		}
	}
#endif

	if (!lcmusic->lcplayer
	    || lcmusic->music_state != LCMUSIC_STATUS_PLAYING)
		return;

	if (lcmusic->is_play_in5s) {
		if (++lcmusic->play_time > 5000 / MONITOR_TIME_PERIOD) {
			lcmusic->is_play_in5s = 0;
			lcmusic->play_time = 0;
			SYS_LOG_INF("play 5s\n");
		}
	}
	_lcmusic_esd_save_bp_info(lcmusic, 0);
	lcmusic_display_play_time(lcmusic, 0);
}

static void _lcmusic_seek_timer(struct thread_timer *ttimer,
				void *expiry_fn_arg)
{
	struct app_msg msg = {
		.type = MSG_LCMUSIC_EVENT,
	};
	struct lcmusic_app_t *lcmusic = lcmusic_get_app();

	if (!lcmusic || !lcmusic->seek_direction)
		return;

	/* replay current url from beginning */
	_lcmusic_clear_breakpoint(lcmusic);

	SYS_LOG_INF("%d\n", lcmusic->seek_direction);

	lcmusic->mplayer_state = MPLAYER_STATE_NORMAL;
	lcmusic->prev_music_state = 0;
	if (lcmusic->seek_direction == SEEK_FORWARD) {
		_lcmusic_force_play_next(lcmusic, false);
	} else {
		msg.cmd = MSG_LCMPLAYER_PLAY_CUR;
		send_async_msg(app_manager_get_current_app(), &msg);
	}
	lcmusic->seek_direction = SEEK_NULL;
}

#ifdef CONFIG_SOUNDBAR_LCMUSIC
static void _lcmusic_soundbar_timer(struct thread_timer *ttimer,
				    void *expiry_fn_arg)
{
	struct lcmusic_app_t *lcmusic = lcmusic_get_app();

	/* disk in but no file or music paused, keep checking if disk pluged out */
	if (lcmusic->soundbar_check_disk_out
	    || lcmusic->music_state == LCMUSIC_STATUS_STOP) {
		if (_check_disk_plugout()) {
			lcmusic->disk_init = 0;	/* disk out, need reinitialize */
			lcmusic_exit_iterator();
			lcmusic->soundbar_check_disk_out = 0;
			lcmusic->music_state = LCMUSIC_STATUS_ERROR;
		}

		thread_timer_start(&lcmusic->soundbar_disk_timer, 1000, 0);
		return;
	}

	if (_check_disk_plugout() == false) {
		/* disk in, start monitor timer to initialize */
		thread_timer_start(&lcmusic->monitor_timer, 0, 0);
		return;
	}

	/* disk out, keep checking if disk pluged in */
	thread_timer_start(&lcmusic->soundbar_disk_timer, 1000, 0);
}
#endif

#ifdef CONFIG_LC_MUSIC_FREQPOINT_ENERGY_DEMO
static int lcmusic_a2dp_get_freqpoint_energy(short * info, int size)
{
	struct lcmusic_app_t *lcmusic = lcmusic_get_app();

	if (!lcmusic)
		return -1;

	if (!lcmusic->lcplayer)
		return -1;

	return media_player_get_output_energy_sample(lcmusic->lcplayer->player,
				MEDIA_EFFECT_EXT_GET_FREQBAND_ENERGY, info, size);
}

static void lcmusic_display_freqpoint_energy(struct thread_timer *ttimer,
					     void *expiry_fn_arg)
{
	short temp_info[12] = {0};
	if (lcmusic_a2dp_get_freqpoint_energy(temp_info, sizeof(temp_info)) == 0) {
		SYS_LOG_INF("fp energy:%x,%x,%x,%x,%x,%x,%x,%x,%x\n",
			    temp_info[0],
			    temp_info[1],
			    temp_info[2],
			    temp_info[3],
			    temp_info[4],
			    temp_info[5],
			    temp_info[6],
			    temp_info[7], temp_info[8]);
	}
}
#endif

void lcmusic_thread_timer_init(struct lcmusic_app_t *lcmusic)
{
	if (NULL == lcmusic) {
		return;
	}

	thread_timer_init(&lcmusic->monitor_timer, _lcmusic_monitor_timer,
			  NULL);
	thread_timer_init(&lcmusic->seek_timer, _lcmusic_seek_timer, NULL);
#ifdef CONFIG_SOUNDBAR_LCMUSIC
	thread_timer_init(&lcmusic->soundbar_disk_timer,
			  _lcmusic_soundbar_timer, NULL);
#endif

#ifdef CONFIG_LC_MUSIC_FREQPOINT_ENERGY_DEMO
	thread_timer_init(&lcmusic->energy_timer,
			  lcmusic_display_freqpoint_energy, NULL);
	thread_timer_start(&lcmusic->energy_timer, 100, 1000);
#endif
}

void lcmusic_thread_timer_deinit(struct lcmusic_app_t *lcmusic)
{
	if (NULL == lcmusic) {
		return;
	}

	if (thread_timer_is_running(&lcmusic->monitor_timer)) {
		thread_timer_stop(&lcmusic->monitor_timer);
	}
	if (thread_timer_is_running(&lcmusic->seek_timer)) {
		thread_timer_stop(&lcmusic->seek_timer);
	}
#ifdef CONFIG_SOUNDBAR_LCMUSIC
	if (thread_timer_is_running(&lcmusic->soundbar_disk_timer)) {
		thread_timer_stop(&lcmusic->soundbar_disk_timer);
	}
#endif
#ifdef CONFIG_LC_MUSIC_FREQPOINT_ENERGY_DEMO
	if (thread_timer_is_running(&lcmusic->energy_timer))
		thread_timer_stop(&lcmusic->energy_timer);
#endif
}

static int _lcmusic_start_play(struct lcmusic_app_t *lcmusic, const char *url,
				int seek_time)
{
	struct lcplay_param play_param;

	if(!lcmusic->disk_init) {
		//not init, wait monitor timer
		SYS_LOG_INF("not init");
		return 0;
	}

	if(NULL == url || strlen(url) == 0) {
		SYS_LOG_WRN("url is null");
		return 0;
	}

	if(NULL != lcmusic->lcplayer) {
		SYS_LOG_WRN("skip play");
		return 0;
	}

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	lcmusic->set_dvfs_level = SOC_DVFS_LEVEL_FULL_PERFORMANCE;
	soc_dvfs_set_level(lcmusic->set_dvfs_level, "lcmusic");
#endif
	memset(&play_param, 0, sizeof(struct lcplay_param));
	play_param.url = url;
	play_param.seek_time = seek_time;
	play_param.play_event_callback = _lcmusic_play_event_callback;
	play_param.bp.file_offset = lcmusic->music_bp_info.bp_info.file_offset;
	play_param.bp.time_offset = lcmusic->music_bp_info.bp_info.time_offset;
#ifdef CONFIG_SOUNDBAR_SAMPLE_RATE_FILTER
	play_param.min_sample_rate_khz = LCMUSIC_SOUNDBAR_MIN_SAMPLE_RATE;
#endif

	SYS_LOG_INF("url:%s", url);
	//A newline to separate the log when url is too long.
	printk("\n");
	SYS_LOG_INF("seek_time:%d", play_param.seek_time);
	SYS_LOG_INF("offset:%d, %d", play_param.bp.time_offset, play_param.bp.file_offset);

	lcmusic->lcplayer = mplayer_start_play(&play_param);
	if (!lcmusic->lcplayer) {
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		if (lcmusic->set_dvfs_level) {
			soc_dvfs_unset_level(lcmusic->set_dvfs_level, "lcmusic");
			lcmusic->set_dvfs_level = 0;
		}
#endif
		SYS_LOG_WRN("Start mplayer failed");
		lcmusic->music_state = LCMUSIC_STATUS_ERROR;
		if (lcmusic->mplayer_state != MPLAYER_STATE_NORMAL)
			lcmusic->mplayer_state = MPLAYER_STATE_ERROR;
		_lcmusic_force_play_next(lcmusic, true);
		return 0;
	} else {
		lcmusic->is_play_in5s = 1;
		lcmusic->play_time = 0;
		lcmusic->music_state = LCMUSIC_STATUS_PLAYING;
		lcmusic_store_play_state(lcmusic->cur_dir);
		lcmusic_display_icon_play();

		if (!lcmusic->filt_track_no)
			lcmusic_display_track_no(lcmusic->music_bp_info.
						 track_no, 3000);
		lcmusic->filt_track_no = 0;

		thread_timer_start(&lcmusic->monitor_timer, 0,
				   MONITOR_TIME_PERIOD);

		_lcmusic_esd_save_bp_info(lcmusic, 1);
		return 1;
	}
}

static void _lcmusic_switch_app_check(struct lcmusic_app_t *lcmusic)
{
#ifndef CONFIG_SOUNDBAR_LCMUSIC

	if (lcmusic->mplayer_state == MPLAYER_STATE_ERROR) {
#ifdef CONFIG_LCMUSIC_APP_SWITCH_ON_ERROR
		struct app_msg msg = { 0 };
		msg.reserve = 0;
		msg.type = MSG_INPUT_EVENT;
		msg.cmd = MSG_KEY_SWITCH_APP;
		if (_check_disk_plugout()) {
			msg.ptr = APP_ID_DEFAULT;
			SYS_LOG_WRN("disk plug out,exit app\n");
		} else {
			msg.ptr = NULL;
			SYS_LOG_WRN("no music,exit app\n");
		}
		/* change to next app */
		send_async_msg(APP_ID_MAIN, &msg);
#endif
	} else {
		/*update play time to 00:00 */
		lcmusic_display_play_time(lcmusic, 0);
		lcmusic->music_state = LCMUSIC_STATUS_ERROR;
		SYS_LOG_INF("not switch\n");
	}
#else
	lcmusic->music_state = LCMUSIC_STATUS_ERROR;

	if (_check_disk_plugout()) {
		/* disk out */
		lcmusic->disk_init = 0;
		lcmusic_exit_iterator();
		lcmusic->soundbar_check_disk_out = 0;
	} else {
		/* disk in but no file */
		lcmusic->soundbar_check_disk_out = 1;
	}

	thread_timer_start(&lcmusic->soundbar_disk_timer, 1000, 0);
#endif
}

#ifdef	CONFIG_INPUT_DEV_ACTS_IRKEY
static void _lcmusic_deal_play_num(struct lcmusic_app_t *lcmusic, u8_t num)
{
	if (lcmusic->seek_direction)
		return;

	if (lcmusic->track_no > 999)
		lcmusic->track_no = 0;
	lcmusic->track_no = lcmusic->track_no * 10 + num;
	lcmusic_display_track_no(lcmusic->track_no, 8000);
	lcmusic->num_play = 1;
	lcmusic->num_play_time = 0;
	/*no display bp track_no dearing num play */
	lcmusic->filt_track_no = 1;
}
#endif

static void _lcmusic_cancel_num_input_check(struct lcmusic_app_t *lcmusic)
{
#ifdef	CONFIG_INPUT_DEV_ACTS_IRKEY
	if (lcmusic->num_play) {
		lcmusic->num_play = 0;
		lcmusic->num_play_time = 0;
		lcmusic->track_no = 0;
		lcmusic->filt_track_no = 0;
#ifdef CONFIG_SEG_LED_MANAGER
		seg_led_manager_set_timeout_event(0, NULL);
#endif
	}
#endif
}

int lcmusic_exit_playback(void)
{
	SYS_LOG_INF("");
	audio_policy_set_bis_link_delay_ms(0);
	return 0;
}

#ifdef CONFIG_BMS_LCMUSIC_APP

/* find the existed broad chan */
static struct bt_broadcast_chan *find_broad_chan(uint32_t handle, uint8_t id)
{
	struct lcmusic_app_t *lcmusic = lcmusic_get_app();
	struct bt_broadcast_chan *chan;
	int i;

	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		chan = &lcmusic->broad_chan[i];
		if (chan->handle == handle && chan->id == id) {
			return chan;
		}
	}
	return NULL;
}

/* find a new broad chan */
static struct bt_broadcast_chan *new_broad_chan(uint32_t handle, uint8_t id)
{
	struct lcmusic_app_t *lcmusic = lcmusic_get_app();
	struct bt_broadcast_chan *chan;
	int i;

	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		chan = &lcmusic->broad_chan[i];
		if (chan->handle == 0 && chan->id == 0) {
			return chan;
		}
	}
	return NULL;
}

static void bms_lcmusic_tx_start(uint32_t handle, uint8_t id,
				 uint16_t audio_handle)
{
	struct lcmusic_app_t *lcmusic = lcmusic_get_app();

	if (lcmusic->tx_start) {
		return;
	}

	if (lcmusic->broadcast_source_enabled && lcmusic->capture_player_run) {
		lcmusic->tx_start = 1;
		//Time sensitive, short log to save time.
		printk("[" SYS_LOG_DOMAIN "] trigger record\n");
	}
}

static void bms_lcmusic_tx_sync_start(uint32_t handle, uint8_t id,
				      uint16_t audio_handle)
{
	struct lcmusic_app_t *lcmusic = lcmusic_get_app();

	if (lcmusic->tx_sync_start) {
		return;
	}

	if (lcmusic->broadcast_source_enabled && lcmusic->playback_player_run) {
		media_player_trigger_audio_track_start(lcmusic->lcplayer->
						       player);
		lcmusic->tx_sync_start = 1;
		//Time sensitive, short log to save time.
		printk("[" SYS_LOG_DOMAIN "] trigger track\n");
	}
}

static int bms_lcmusic_handle_source_config(struct bt_braodcast_configured *rep)
{
	struct lcmusic_app_t *lcmusic = lcmusic_get_app();
	struct bt_broadcast_chan *chan;
	uint8_t vnd_buf[20] = { 0 };
	uint8_t type = 0;

	SYS_LOG_INF("h:0x%x id:%d, need_resume_play:%d\n", rep->handle, rep->id, lcmusic->need_resume_play);

	chan = find_broad_chan(rep->handle, rep->id);
	if (!chan) {
		chan = new_broad_chan(rep->handle, rep->id);
		if (!chan) {
			SYS_LOG_ERR("no broad chan handle: %d, id: %d\n",
				    rep->handle, rep->id);
			return -EINVAL;
		}
		lcmusic->num_of_broad_chan++;
	}

	chan->handle = rep->handle;
	chan->id = rep->id;

	vnd_buf[0] = VS_ID & 0xFF;
	vnd_buf[1] = VS_ID >> 8;
	memcpy(&vnd_buf[2], lcmusic->broadcast_code, 16);
	vnd_buf[18] = SERIVCE_UUID & 0xFF;
	vnd_buf[19] = SERIVCE_UUID >> 8;

#if !VS_COMPANY_ID
	type = BT_DATA_SVC_DATA16;
#endif

	if (lcmusic->num_of_broad_chan == 1) {
		bt_manager_broadcast_stream_cb_register(chan,
							bms_lcmusic_tx_start,
							NULL);
		bt_manager_broadcast_stream_tws_sync_cb_register(chan,
								 bms_lcmusic_tx_sync_start);
		bt_manager_broadcast_stream_set_tws_sync_cb_offset(chan,
								   broadcast_get_tws_sync_offset
								   (lcmusic->
								    qos));

		bt_manager_broadcast_source_vnd_ext_send(chan->handle, vnd_buf,
							 sizeof(vnd_buf), type);
		lcmusic->chan = chan;
		//if no config tts, start play here
		if (!lcmusic->need_resume_play) {
			_lcmusic_start_play(lcmusic, lcmusic->cur_url, 0);
		}
	}

	if(app_tws_status_get_enable()) {
		bt_manager_broadcast_source_enable(lcmusic->chan->handle);
	} else {
		if (lcmusic->num_of_broad_chan >= NUM_OF_BROAD_CHAN) {
			bt_manager_broadcast_source_enable(lcmusic->chan->handle);
		}
	}

	return 0;
}

static int bms_lcmusic_handle_source_enable(struct bt_broadcast_report *rep)
{
	struct lcmusic_app_t *lcmusic = lcmusic_get_app();

	if (NULL == lcmusic) {
		return -1;
	}

	SYS_LOG_INF("handle 0x%x, id %d\n", rep->handle, rep->id);
	SYS_LOG_INF("resume %d, chan %d\n", lcmusic->need_resume_play,
		    lcmusic->num_of_broad_chan);

	if(!app_tws_status_get_enable()) {
		if (BROADCAST_NUM_BIS > rep->id) {
			SYS_LOG_INF("wait all done.");
			return 0;
		}
	}

	lcmusic->broadcast_source_enabled = 1;
#if ENABLE_PADV_APP
	if (lcmusic->chan != NULL) {
		padv_tx_init(lcmusic->chan->handle, AUDIO_STREAM_LOCAL_MUSIC);
	}
#endif
	if (lcmusic->need_resume_play || !lcmusic->disk_init) {
		SYS_LOG_INF("wait %d, %d", lcmusic->need_resume_play, lcmusic->disk_init);
	} else {
		bms_lcmusic_init_capture();
		bms_lcmusic_start_capture();
	}

	return 0;
}

static int bms_lcmusic_handle_source_disable(struct bt_broadcast_report *rep)
{
	struct lcmusic_app_t *lcmusic = lcmusic_get_app();

	if ((rep->handle != lcmusic->broadcast_dev_handle) &&
	    (lcmusic->broadcast_dev_handle != 0))
	{
		SYS_LOG_WRN("Handle mismatch:0x%x,0x%x\n", rep->handle, lcmusic->broadcast_dev_handle);
		return -1;
	}

#if ENABLE_PADV_APP
	padv_tx_deinit();
#endif
	bms_lcmusic_stop_capture();
	bms_lcmusic_exit_capture();

	//usound->broadcast_source_enabled = 0;
	lcmusic->tx_start = 0;
	lcmusic->tx_sync_start = 0;

	struct bt_broadcast_chan *chan = find_broad_chan(rep->handle, rep->id);
	if (chan) {
		if (lcmusic->num_of_broad_chan) {
			lcmusic->num_of_broad_chan--;
			chan->handle = 0;
			chan->id = 0;
			if (chan == lcmusic->chan) {
				lcmusic->chan = NULL;
			}
			if (!lcmusic->num_of_broad_chan) {
				lcmusic->broadcast_source_enabled = 0;
				SYS_LOG_INF("all disabled\n");
			}
		} else {
			SYS_LOG_WRN("should not be here\n");
		}
	}

	return 0;
}

static int bms_lcmusic_handle_source_release(struct bt_broadcast_report *rep)
{
	struct lcmusic_app_t *lcmusic = lcmusic_get_app();

	if (NULL == lcmusic) {
		return -1;
	}

	if ((rep->handle != lcmusic->broadcast_dev_handle) &&
	    (lcmusic->broadcast_dev_handle != 0))
	{
		SYS_LOG_WRN("Handle mismatch:0x%x,0x%x\n", rep->handle, lcmusic->broadcast_dev_handle);
		return -1;
	}

	SYS_LOG_INF("handle 0x%x, id %d\n", rep->handle, rep->id);
	lcmusic->chan = NULL;
	lcmusic->broadcast_source_enabled = 0;
	lcmusic->num_of_broad_chan = 0;
	lcmusic->broadcast_dev_handle = 0;
	memset(lcmusic->broad_chan, 0, sizeof(lcmusic->broad_chan));

	if (lcmusic->auracast_pending) {
		lcmusic->auracast_pending = 0;
		lcmusic_switch_auracast();
	}

	return 0;

}

static void lcmusic_switch_auracast()
{
	struct lcmusic_app_t *lcmusic = lcmusic_get_app();

	if (lcmusic_is_bms_mode()) {
		lcmusic_stop_play(lcmusic, true);

		bms_lcmusic_stop_capture();
		bms_lcmusic_exit_capture();

		lcmusic->tx_start = 0;
		lcmusic->tx_sync_start = 0;

		lcmusic_set_auracast_mode(false);
	} else {
#ifdef CONFIG_NO_AURACST_ON_LE_CONNECTED
		if (bt_manager_audio_get_leaudio_dev_num()) {
			SYS_LOG_ERR("LE conneted!");
			return;
		}
#endif

		//bis maybe in releasing
		if (!lcmusic->chan) {
			if (lcmusic->playback_player_run) {
				lcmusic_stop_play(lcmusic, true);
			}
			lcmusic_set_auracast_mode(true);
		} else {
			SYS_LOG_INF("Wait bis releasing");
			lcmusic->auracast_pending = 1;
		}
	}

#ifdef CONFIG_PLAYTTS
	//player will be started after tts restart.
	lcmusic->need_resume_play = 1;
#else
	lcmusic_play_resume(0);
#endif
}

void bms_lcmusic_bt_event_proc(struct app_msg *msg)
{
	SYS_LOG_INF("cmd: %d\n", msg->cmd);

	switch (msg->cmd) {
	case BT_REQ_RESTART_PLAY:
		SYS_LOG_WRN("skip BT_REQ_RESTART_PLAY\n");
		break;

	case BT_BROADCAST_SOURCE_CONFIG:
		bms_lcmusic_handle_source_config(msg->ptr);
		break;
	case BT_BROADCAST_SOURCE_ENABLE:
		bms_lcmusic_handle_source_enable(msg->ptr);
		break;
	case BT_BROADCAST_SOURCE_DISABLE:
		bms_lcmusic_handle_source_disable(msg->ptr);
		break;
	case BT_BROADCAST_SOURCE_RELEASE:
		bms_lcmusic_handle_source_release(msg->ptr);
		break;

	case BT_TWS_DISCONNECTION_EVENT:
		SYS_LOG_INF("tws disconnected");
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

//#ifdef CONFIG_LCMUSIC_FEATURE_RESTART
static int _lcmusic_handle_restart(void)
{

	lcmusic_play_pause(true);
#ifdef CONFIG_LCMUSIC_FEATURE_RESTART
	struct lcmusic_app_t *lcmusic = lcmusic_get_app();

	// clear restart flag to avoid restart handler to do dumplicated handling.
	lcmusic->restart = 0;
#endif
	lcmusic_play_resume(0);

	return 0;
}

//#endif

void lcmusic_play_pause(bool need_updata_bp)
{
	struct lcmusic_app_t *lcmusic = lcmusic_get_app();

	if (NULL == lcmusic) {
		return;
	}

	SYS_LOG_INF("%d", need_updata_bp);

#ifdef CONFIG_BMS_LCMUSIC_APP
	if (lcmusic_is_bms_mode()) {
		if (lcmusic->capture_player_run) {
			bms_lcmusic_stop_capture();
			bms_lcmusic_exit_capture();
		}
		lcmusic->tx_start = 0;
		lcmusic->tx_sync_start = 0;
	}
#endif

	if (lcmusic->playback_player_run) {
		lcmusic_stop_play(lcmusic, need_updata_bp);
		lcmusic_exit_playback();
	}

}

void lcmusic_play_resume(int seek_time)
{
	struct lcmusic_app_t *lcmusic = lcmusic_get_app();
	if (NULL == lcmusic) {
		return;
	}

	SYS_LOG_INF("seek_time %d", seek_time);

	if (!lcmusic->playback_player_run) {
		if(1 != _lcmusic_start_play(lcmusic, lcmusic->cur_url, seek_time)) {
			return;
		}
	}

#ifdef CONFIG_BMS_LCMUSIC_APP
	SYS_LOG_INF("mode %d, source %d,cap:%d", lcmusic_is_bms_mode(),
		    lcmusic->broadcast_source_enabled,
		    lcmusic->capture_player_run);
	if (lcmusic_is_bms_mode()) {
		if (lcmusic->broadcast_source_enabled) {
			if (!lcmusic->capture_player_run) {
				bms_lcmusic_init_capture();
				bms_lcmusic_start_capture();
			}
		}
	}
#endif
}

#ifdef CONFIG_PLAYTTS
//static void _lcmusic_tts_delay_resume(struct thread_timer *ttimer,
//				      void *expiry_fn_arg)
//{
//	struct lcmusic_app_t *lcmusic = lcmusic_get_app();
//	if ((lcmusic->music_state != LCMUSIC_STATUS_PLAYING)
//	    && (lcmusic->music_state != LCMUSIC_STATUS_ERROR)) {
//		lcmusic->filt_track_no = 1;
//		lcmusic_play_resume(0);
//	}
//	lcmusic->need_resume_play = 0;
//}

void lcmusic_tts_event_proc(struct app_msg *msg)
{
	struct lcmusic_app_t *lcmusic = lcmusic_get_app();

	if (!lcmusic) {
		return;
	}

	SYS_LOG_INF("msg->value=%d", msg->value);

	switch (msg->value) {
	case TTS_EVENT_START_PLAY:
		if (lcmusic->playback_player_run) {
			lcmusic_play_pause(true);
			lcmusic->need_resume_play = 1;
		}
		break;
	case TTS_EVENT_STOP_PLAY:
		if (lcmusic->need_resume_play) {
//			if (thread_timer_is_running(&lcmusic->tts_resume_timer)) {
//				thread_timer_stop(&lcmusic->tts_resume_timer);
//			}
//			thread_timer_init(&lcmusic->tts_resume_timer,
//					  _lcmusic_tts_delay_resume, NULL);
//			thread_timer_start(&lcmusic->tts_resume_timer, 200, 0);

			lcmusic_play_resume(0);
		}
		break;
	}
}
#endif

void lcmusic_input_event_proc(struct app_msg *msg)
{
	struct app_msg new_msg = { 0 };
	struct lcmusic_app_t *lcmusic = lcmusic_get_app();

	SYS_LOG_INF("msg cmd %d", msg->cmd);
	SYS_LOG_INF("music state %d", lcmusic->music_state);

	switch (msg->cmd) {
	case MSG_LCMPLAYER_PLAY_NEXT:
		_lcmusic_cancel_num_input_check(lcmusic);

		if (lcmusic->disk_scanning || lcmusic->seek_direction)
			break;

		lcmusic->prev_music_state = 0;
		lcmusic->filt_track_no = 0;
		lcmusic_play_pause(false);
		sys_event_notify(SYS_EVENT_PLAY_NEXT);

		if (lcmusic_play_next_url(lcmusic, true)) {
			new_msg.type = MSG_LCMUSIC_EVENT;
			new_msg.cmd = MSG_LCMPLAYER_PLAY_URL;
			send_async_msg(app_manager_get_current_app(), &new_msg);
		} else {
			_lcmusic_switch_app_check(lcmusic);
		}
		break;
	case MSG_LCMPLAYER_PLAY_PREV:
		_lcmusic_cancel_num_input_check(lcmusic);

		if (lcmusic->disk_scanning || lcmusic->seek_direction)
			break;

		lcmusic->prev_music_state = 1;
		if (lcmusic->is_play_in5s) {
			lcmusic->is_play_in5s = 0;
			lcmusic->filt_track_no = 0;
			lcmusic_play_pause(false);
			sys_event_notify(SYS_EVENT_PLAY_PREVIOUS);
			if (!lcmusic_play_prev_url(lcmusic)) {
				SYS_LOG_INF("prev url is null\n");
				if (lcmusic->music_state ==
				    LCMUSIC_STATUS_ERROR) {
					new_msg.cmd =
					    MSG_LCMPLAYER_AUTO_PLAY_NEXT;
				} else {
					new_msg.cmd = MSG_LCMPLAYER_PLAY_URL;
				}
			} else {
				new_msg.cmd = MSG_LCMPLAYER_PLAY_URL;
			}
			new_msg.type = MSG_LCMUSIC_EVENT;
			send_async_msg(app_manager_get_current_app(), &new_msg);
		} else {
			if ((lcmusic->music_state == LCMUSIC_STATUS_PLAYING)
			    || (lcmusic->music_state == LCMUSIC_STATUS_STOP)) {

				lcmusic_play_pause(false);
				lcmusic_play_resume(0);

			} else {
				lcmusic_play_pause(false);
				if (!lcmusic->filter_prev_tts) {
					sys_event_notify
					    (SYS_EVENT_PLAY_PREVIOUS);
				}
				lcmusic->filter_prev_tts = 0;
				if (lcmusic_play_prev_url(lcmusic)) {
					lcmusic_play_pause(false);
					lcmusic_play_resume(0);
				} else {
					SYS_LOG_INF("prev url is null\n");
					if (lcmusic->music_state ==
					    LCMUSIC_STATUS_ERROR) {
						new_msg.cmd =
						    MSG_LCMPLAYER_AUTO_PLAY_NEXT;
					} else {
						new_msg.cmd =
						    MSG_LCMPLAYER_PLAY_URL;
					}
					new_msg.type = MSG_LCMUSIC_EVENT;
					send_async_msg
					    (app_manager_get_current_app(),
					     &new_msg);
				}
			}
		}
		break;
	case MSG_LCMPLAYER_PLAY_VOLUP:
		_lcmusic_cancel_num_input_check(lcmusic);

		system_volume_up(AUDIO_STREAM_LOCAL_MUSIC, 1);
#ifdef CONFIG_BMS_LCMUSIC_APP
#if ENABLE_PADV_APP
		padv_tx_data(padv_volume_map
			     (system_volume_get(AUDIO_STREAM_LOCAL_MUSIC), 1));
#endif
#endif
		break;
	case MSG_LCMPLAYER_PLAY_VOLDOWN:
		_lcmusic_cancel_num_input_check(lcmusic);

		system_volume_down(AUDIO_STREAM_LOCAL_MUSIC, 1);
#ifdef CONFIG_BMS_LCMUSIC_APP
#if ENABLE_PADV_APP
		padv_tx_data(padv_volume_map
			     (system_volume_get(AUDIO_STREAM_LOCAL_MUSIC), 1));
#endif
#endif
		break;
	case MSG_LCMPLAYER_PLAY_PAUSE:
#ifdef	CONFIG_INPUT_DEV_ACTS_IRKEY
		if (lcmusic->num_play) {
			lcmusic->num_play = 0;
			lcmusic->num_play_time = 0;
			new_msg.type = MSG_LCMUSIC_EVENT;
			new_msg.cmd = MSG_LCMPLAYER_PLAY_TRACK_NO;
			send_async_msg(app_manager_get_current_app(), &new_msg);
			break;
		}
#endif
		if (lcmusic->music_state == LCMUSIC_STATUS_PLAYING) {
			lcmusic->filt_track_no = 1;
			lcmusic_play_pause(true);
			lcmusic_store_play_state(lcmusic->cur_dir);
			sys_event_notify(SYS_EVENT_PLAY_PAUSE);
			lcmusic_bp_info_save(lcmusic);
#ifdef CONFIG_SOUNDBAR_LCMUSIC
			/* to avoid disk pluged out when paused */
			thread_timer_start(&lcmusic->soundbar_disk_timer, 1000,
					   0);
#endif
		}
		break;
	case MSG_LCMPLAYER_PLAY_PLAYING:
#ifdef	CONFIG_INPUT_DEV_ACTS_IRKEY
		if (lcmusic->num_play) {
			lcmusic->num_play = 0;
			lcmusic->num_play_time = 0;
			new_msg.type = MSG_LCMUSIC_EVENT;
			new_msg.cmd = MSG_LCMPLAYER_PLAY_TRACK_NO;
			send_async_msg(app_manager_get_current_app(), &new_msg);
			break;
		}
#endif
		sys_event_notify(SYS_EVENT_PLAY_START);

		new_msg.type = MSG_LCMUSIC_EVENT;
		new_msg.cmd = MSG_LCMPLAYER_PLAY_URL;
		send_async_msg(app_manager_get_current_app(), &new_msg);
		break;
	case MSG_LCMPLAYER_PLAY_SEEK_FORWARD:
		if (thread_timer_is_running(&lcmusic->seek_timer))
			thread_timer_stop(&lcmusic->seek_timer);

		lcmusic->seek_direction = SEEK_NULL;
		if (lcmusic->music_state == LCMUSIC_STATUS_SEEK) {
			if (thread_timer_is_running(&lcmusic->monitor_timer))
				thread_timer_stop(&lcmusic->monitor_timer);

			lcmusic->filt_track_no = 1;
			lcmusic_play_resume(lcmusic->music_bp_info.bp_info.time_offset +
				(SEEK_SPEED_LEVEL * (os_uptime_get_32() - start_seek_time)));
		}
		break;
	case MSG_LCMPLAYER_PLAY_SEEK_BACKWARD:
		if (thread_timer_is_running(&lcmusic->seek_timer))
			thread_timer_stop(&lcmusic->seek_timer);

		lcmusic->seek_direction = SEEK_NULL;
		if (lcmusic->music_state == LCMUSIC_STATUS_SEEK) {
			if (thread_timer_is_running(&lcmusic->monitor_timer))
				thread_timer_stop(&lcmusic->monitor_timer);

			lcmusic->filt_track_no = 1;
			int seek_bp_time = lcmusic->music_bp_info.bp_info.time_offset -
					(SEEK_SPEED_LEVEL * (os_uptime_get_32() - start_seek_time));

			lcmusic_play_resume(seek_bp_time);

		}
		break;
	case MSG_LCMPLAYER_SEEKFW_START_CTIME:
		_lcmusic_cancel_num_input_check(lcmusic);

		lcmusic->music_state = LCMUSIC_STATUS_SEEK;
		lcmusic->seek_direction = SEEK_FORWARD;
		start_seek_time = os_uptime_get_32();
		lcmusic_play_pause(true);
		thread_timer_start(&lcmusic->monitor_timer,
				   1000 / SEEK_SPEED_LEVEL,
				   1000 / SEEK_SPEED_LEVEL);
		if (lcmusic->media_info.total_time >
		    lcmusic->music_bp_info.bp_info.time_offset) {
			thread_timer_start(&lcmusic->seek_timer,
					   (lcmusic->media_info.total_time -
					    lcmusic->music_bp_info.bp_info.
					    time_offset) / SEEK_SPEED_LEVEL, 0);
		}
		break;
	case MSG_LCMPLAYER_SEEKBW_START_CTIME:
		_lcmusic_cancel_num_input_check(lcmusic);

		lcmusic->music_state = LCMUSIC_STATUS_SEEK;
		lcmusic->seek_direction = SEEK_BACKWARD;
		start_seek_time = os_uptime_get_32();
		//lcmusic_stop_play(lcmusic, true);
		lcmusic_play_pause(true);
		thread_timer_start(&lcmusic->monitor_timer,
				   1000 / SEEK_SPEED_LEVEL,
				   1000 / SEEK_SPEED_LEVEL);
		thread_timer_start(&lcmusic->seek_timer,
				   lcmusic->music_bp_info.bp_info.time_offset /
				   SEEK_SPEED_LEVEL, 0);
		break;
	case MSG_LCMPLAYER_SET_PLAY_MODE:
		_lcmusic_cancel_num_input_check(lcmusic);

		if (++lcmusic->mplayer_mode >= MPLAYER_NUM_PLAY_MODES)
			lcmusic->mplayer_mode = 0;
		lcmusic_play_set_mode(lcmusic, lcmusic->mplayer_mode);
		SYS_LOG_INF("set mplayer_mode :%d\n", lcmusic->mplayer_mode);
		break;
	case MSG_LCMPLAYER_PLAY_NEXT_DIR:
		_lcmusic_cancel_num_input_check(lcmusic);

		if (lcmusic->disk_scanning || lcmusic->seek_direction)
			break;
		lcmusic->prev_music_state = 0;
		lcmusic->filt_track_no = 0;
		//lcmusic_stop_play(lcmusic, false);

		if (lcmusic_play_next_folder_url(lcmusic)) {
			new_msg.type = MSG_LCMUSIC_EVENT;
			new_msg.cmd = MSG_LCMPLAYER_PLAY_URL;
			send_async_msg(app_manager_get_current_app(), &new_msg);
		} else {
			_lcmusic_switch_app_check(lcmusic);
		}
		break;
	case MSG_LCMPLAYER_PLAY_PREV_DIR:
		_lcmusic_cancel_num_input_check(lcmusic);

		if (lcmusic->disk_scanning || lcmusic->seek_direction)
			break;

		lcmusic->filt_track_no = 0;
		//lcmusic_stop_play(lcmusic, false);
		lcmusic_play_pause(false);
		if (lcmusic_play_prev_folder_url(lcmusic)) {
			new_msg.type = MSG_LCMUSIC_EVENT;
			new_msg.cmd = MSG_LCMPLAYER_PLAY_URL;
			send_async_msg(app_manager_get_current_app(), &new_msg);
		} else {
			_lcmusic_switch_app_check(lcmusic);
		}
		break;
#ifdef	CONFIG_INPUT_DEV_ACTS_IRKEY
	case MSG_LCMPLAYER_SET_NUM0:
		_lcmusic_deal_play_num(lcmusic, 0);
		break;
	case MSG_LCMPLAYER_SET_NUM1:
		_lcmusic_deal_play_num(lcmusic, 1);
		break;
	case MSG_LCMPLAYER_SET_NUM2:
		_lcmusic_deal_play_num(lcmusic, 2);
		break;
	case MSG_LCMPLAYER_SET_NUM3:
		_lcmusic_deal_play_num(lcmusic, 3);
		break;
	case MSG_LCMPLAYER_SET_NUM4:
		_lcmusic_deal_play_num(lcmusic, 4);
		break;
	case MSG_LCMPLAYER_SET_NUM5:
		_lcmusic_deal_play_num(lcmusic, 5);
		break;
	case MSG_LCMPLAYER_SET_NUM6:
		_lcmusic_deal_play_num(lcmusic, 6);
		break;
	case MSG_LCMPLAYER_SET_NUM7:
		_lcmusic_deal_play_num(lcmusic, 7);
		break;
	case MSG_LCMPLAYER_SET_NUM8:
		_lcmusic_deal_play_num(lcmusic, 8);
		break;
	case MSG_LCMPLAYER_SET_NUM9:
		_lcmusic_deal_play_num(lcmusic, 9);
		break;
#endif

#ifdef CONFIG_BMS_LCMUSIC_APP
	case MSG_SWITCH_BROADCAST:
		if (!app_tws_status_get_enable()) {
			lcmusic_switch_auracast();
		} else {
			SYS_LOG_INF("Please exit TWS mode.\n");
		}
		break;
	case MSG_AURACAST_ENTER:
		if (system_app_get_auracast_mode() == 0) {
			lcmusic_switch_auracast();
		} else if (system_app_get_auracast_mode() == 1) {
			_lcmusic_handle_restart();
		}
		break;
	case MSG_AURACAST_EXIT:
		if (system_app_get_auracast_mode() != 0) {
			lcmusic_switch_auracast();
		}
		break;
#endif

	default:
		break;
	}
}

/*need to clear bp when filed*/
static int _lcmusic_get_bp_url(struct lcmusic_app_t *lcmusic)
{
	return lcmusic_get_url_by_cluster(lcmusic);
}

void lcmusic_app_event_proc(struct app_msg *msg)
{
	struct app_msg new_msg = { 0 };
	struct lcmusic_app_t *lcmusic = lcmusic_get_app();
	int res = 0;
	u8_t init_play_state = 0;

	SYS_LOG_INF("msg cmd =%d\n", msg->cmd);

	switch (msg->cmd) {
	case MSG_LCMPLAYER_INIT:
		init_play_state = lcmusic_get_play_state(lcmusic->cur_dir);

#ifdef CONFIG_SOUNDBAR_LCMUSIC
		/* auto start play when disk plug in */
		init_play_state = LCMUSIC_STATUS_PLAYING;
#endif

		res = _lcmusic_esd_restore_bp_info(lcmusic, &init_play_state);
		if (0 != res) {
			_lcmusic_bpinfo_resume(lcmusic->cur_dir,
					       &lcmusic->music_bp_info);
		}

		res = _lcmusic_get_bp_url(lcmusic);

		lcmusic_scan_disk();

		if (init_play_state != LCMUSIC_STATUS_STOP) {
			if (!res) {
				new_msg.cmd = MSG_LCMPLAYER_PLAY_URL;
			} else {
				new_msg.cmd = MSG_LCMPLAYER_AUTO_PLAY_NEXT;
			}
			new_msg.type = MSG_LCMUSIC_EVENT;
			send_async_msg(app_manager_get_current_app(), &new_msg);
		} else {
			lcmusic->music_state = LCMUSIC_STATUS_STOP;
			if (res)
				lcmusic_play_next_url(lcmusic, false);
			lcmusic_display_icon_pause();
			lcmusic_display_play_time(lcmusic, 0);
		}
		break;

	case MSG_LCMPLAYER_AUTO_PLAY_NEXT:
#ifdef CONFIG_SOUNDBAR_LCMUSIC
		if (_check_disk_plugout()) {
			/* disk out, play error */
			//lcmusic_stop_play(lcmusic, true);
			lcmusic_play_pause(true);
			_lcmusic_bpinfo_save(lcmusic->cur_dir,
					     &lcmusic->music_bp_info);
			_lcmusic_switch_app_check(lcmusic);
			break;
		}
#endif

		/* avoid wrong track_no display */
		lcmusic->filt_track_no = 0;

		//lcmusic_stop_play(lcmusic, false);
		lcmusic_play_pause(false);
		if (lcmusic_play_next_url(lcmusic, false)) {
			lcmusic_play_resume(0);
		} else {
			_lcmusic_switch_app_check(lcmusic);
		}
		break;
	case MSG_LCMPLAYER_PLAY_URL:
		if (lcmusic->lcplayer) {
			//lcmusic_stop_play(lcmusic, false);
			lcmusic_play_pause(false);
		}
		lcmusic_play_resume(0);
		break;
	case MSG_LCMPLAYER_PLAY_CUR:
		//lcmusic_stop_play(lcmusic, false);
		lcmusic_play_pause(false);
		lcmusic_play_resume(0);
		break;
#ifdef	CONFIG_INPUT_DEV_ACTS_IRKEY
	case MSG_LCMPLAYER_PLAY_TRACK_NO:
		lcmusic->filt_track_no = 0;
		if (lcmusic->disk_scanning || lcmusic->track_no == 0
		    || lcmusic->track_no > lcmusic->sum_track_no) {
			lcmusic->track_no = 0;
#ifdef CONFIG_SEG_LED_MANAGER
			seg_led_manager_set_timeout_event(0, NULL);
#endif
			break;
		}

		lcmusic->music_bp_info.track_no = lcmusic->track_no;
		lcmusic->track_no = 0;
		//lcmusic_stop_play(lcmusic, false);
		lcmusic_play_pause(false);
		if (lcmusic_play_set_track_no
		    (lcmusic, lcmusic->music_bp_info.track_no)) {
			new_msg.type = MSG_LCMUSIC_EVENT;
			new_msg.cmd = MSG_LCMPLAYER_PLAY_URL;
			send_async_msg(app_manager_get_current_app(), &new_msg);
		} else {
			_lcmusic_switch_app_check(lcmusic);
		}
		break;
#endif

#ifdef CONFIG_LCMUSIC_FEATURE_RESTART
	case MSG_LCMUSIC_STREAM_RESTART:
		_lcmusic_handle_restart();
		break;
#endif

	default:
		break;
	}
}

void lcmusic_bt_event_proc(struct app_msg *msg)
{
	SYS_LOG_INF("cmd:%d\n", msg->cmd);
	struct lcmusic_app_t *lcmusic = lcmusic_get_app();
	if (msg->cmd == BT_REQ_RESTART_PLAY) {
		if (lcmusic->lcplayer) {
			//lcmusic_stop_play(lcmusic, true);
			lcmusic_play_pause(true);
		}
		os_sleep(200);
		lcmusic->filt_track_no = 1;
		lcmusic_play_resume(0);
	}
	if (msg->cmd == BT_TWS_DISCONNECTION_EVENT) {
#ifdef CONFIG_FS_MANAGER
		fs_manager_disk_init(lcmusic->cur_dir);
#endif

		if (lcmusic->lcplayer && lcmusic->lcplayer->player) {
			media_player_set_output_mode(lcmusic->lcplayer->player,
						     MEDIA_OUTPUT_MODE_DEFAULT);
		}
	}
#ifndef CONFIG_TWS_BACKGROUND_BT
	if (msg->cmd == BT_TWS_CONNECTION_EVENT) {
		bt_manager_halt_phone();
	} else if (msg->cmd == BT_TWS_DISCONNECTION_EVENT) {
		bt_manager_resume_phone();
	}
#endif
}
