/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file media player interface
 */
#define SYS_LOG_DOMAIN "media"
#include <os_common_api.h>
#include <misc/byteorder.h>
#include <mem_manager.h>
#include <msg_manager.h>
#include <srv_manager.h>
#ifdef CONFIG_BLUETOOTH
#include <bt_manager.h>
#endif
#include <sys_wakelock.h>
#include <string.h>
#include "audio_policy.h"
#include "audio_system.h"
#include "media_player.h"
#include "media_service.h"
#include <logging/sys_log.h>
#include <property_manager.h>
#ifdef CONFIG_ACT_EVENT
#include <logging/log_core.h>
LOG_MODULE_REGISTER(media_event,CONFIG_ACT_EVENT_APP_COMPILE_LEVEL);
#endif
#ifdef CONFIG_CONSOLE_SHELL
#include <stdlib.h>
#include <shell/shell.h>
#endif

static OS_MUTEX_DEFINE(media_srv_mutex);

static int media_player_ref_cnt;
static int media_player_lock_cnt = 0;

static media_player_t *current_media_dumpable_player;
static media_player_t *current_media_main_player;

/* the media_player_t will be return as parameter "uesr_data" */
static media_srv_event_notify_t pfn_lifecycle_notify;

static void _notify_player_lifecycle_changed(media_player_t *player, uint32_t event, void *data, uint32_t len)
{
	if (pfn_lifecycle_notify)
		pfn_lifecycle_notify(event, data, len, player);
}

static void _media_service_default_callback(struct app_msg *msg, int result, void *reply)
{
	if (msg->sync_sem) {
		os_sem_give((os_sem *)msg->sync_sem);
	}
}

#ifdef CONFIG_DVFS_DYNAMIC_LEVEL
static int _media_player_get_dvfs_level(int stream_type, int format, bool is_tws, bool is_dumpable)
{
	int dvfs_level = DVFS_LEVEL_NORMAL;

	if (is_tws) {
		if (stream_type == AUDIO_STREAM_MUSIC && (format == SBC_TYPE)) {
		#ifdef CONFIG_MUSIC_DRC_EFFECT
			dvfs_level = DVFS_LEVEL_PERFORMANCE;
		#else
		#ifdef CONFIG_MUSIC_EFFECT
			dvfs_level = DVFS_LEVEL_NORMAL;
		#else
			dvfs_level = DVFS_LEVEL_NORMAL_NO_EFX;
		#endif
		#endif
		} else {
			dvfs_level = DVFS_LEVEL_HIGH_PERFORMANCE;
		}
	} else {
		switch (stream_type) {
		case AUDIO_STREAM_VOICE:
			if (system_check_low_latencey_mode()) {
				dvfs_level = DVFS_LEVEL_HIGH_PERFORMANCE;
			} else {
				dvfs_level = DVFS_LEVEL_MIDDLE;
			}
			break;
		case AUDIO_STREAM_MUSIC:
			if (format == SBC_TYPE) {
			#ifdef CONFIG_MUSIC_DRC_EFFECT
				dvfs_level = DVFS_LEVEL_MIDDLE;
			#else
			#ifdef CONFIG_MUSIC_EFFECT
				dvfs_level = DVFS_LEVEL_NORMAL_NO_DRC;
			#else
				dvfs_level = DVFS_LEVEL_NORMAL_NO_EFX;
			#endif
			#endif
			} else {
			#ifdef CONFIG_MUSIC_DRC_EFFECT
			if (system_check_low_latencey_mode()) {
				dvfs_level = DVFS_LEVEL_PERFORMANCE;
			} else {
				dvfs_level = DVFS_LEVEL_MIDDLE;
			}
			#else
			#ifdef CONFIG_MUSIC_EFFECT
				dvfs_level = DVFS_LEVEL_NORMAL;
			#else
				dvfs_level = DVFS_LEVEL_NORMAL_NO_DRC;
			#endif
			#endif
			}
			break;
		case AUDIO_STREAM_LOCAL_RECORD:
		case AUDIO_STREAM_FM:
		case AUDIO_STREAM_LINEIN:
		case AUDIO_STREAM_MIC_IN:
		#ifdef CONFIG_MUSIC_DRC_EFFECT
			dvfs_level = DVFS_LEVEL_NORMAL;
		#else
			dvfs_level = DVFS_LEVEL_NORMAL_NO_DRC;
		#endif
			break;
		case AUDIO_STREAM_SPDIF_IN:
			dvfs_level = DVFS_LEVEL_HIGH_PERFORMANCE;
			break;
		case AUDIO_STREAM_LOCAL_MUSIC:
			dvfs_level = DVFS_LEVEL_PERFORMANCE;
			break;
		case AUDIO_STREAM_GMA_RECORD:
			dvfs_level = DVFS_LEVEL_PERFORMANCE;
			break;
		case AUDIO_STREAM_USOUND:
			dvfs_level = DVFS_LEVEL_PERFORMANCE;
			break;
		case AUDIO_STREAM_TTS:
			dvfs_level = DVFS_LEVEL_HIGH_PERFORMANCE;
			break;
		}
	}

	return dvfs_level;
}
#endif

static int _media_player_check_audio_effect(media_player_t *handle, int stream_type)
{
	bool effect_enable = true;

	if(property_get_int(CFG_AUDIO_EFFECT, 1) == 0) {
		effect_enable = false;
	}

	switch (stream_type) {
	case AUDIO_STREAM_VOICE:
#ifndef CONFIG_VOICE_EFFECT
		effect_enable = false;
#endif
		break;
	case AUDIO_STREAM_MUSIC:
	case AUDIO_STREAM_LOCAL_MUSIC:
	case AUDIO_STREAM_LOCAL_RECORD:
	case AUDIO_STREAM_GMA_RECORD:
	case AUDIO_STREAM_LINEIN:
	case AUDIO_STREAM_SPDIF_IN:
	case AUDIO_STREAM_MIC_IN:
	case AUDIO_STREAM_FM:
	case AUDIO_STREAM_USOUND:
	case AUDIO_STREAM_TTS:
	case AUDIO_STREAM_SOUNDBAR:
	case AUDIO_STREAM_LE_AUDIO:
#ifndef CONFIG_MUSIC_EFFECT
		effect_enable = false;
#endif
		break;
	default:
		break;
	}

	if (!effect_enable) {
		SYS_LOG_INF("audio effect disable");
		media_player_set_effect_enable(handle, false);
	}

#if CONFIG_MEDIA_EFFECT_OUTMODE != MEDIA_EFFECT_OUTPUT_DEFAULT
	media_player_set_effect_output_mode(handle, CONFIG_MEDIA_EFFECT_OUTMODE);
#endif
	return 0;
}

media_player_t *media_player_open(media_init_param_t *init_param)
{
	struct app_msg msg = {0};
	os_sem return_notify;
	media_srv_init_param_t srv_param = {0};
#ifdef CONFIG_DVFS_DYNAMIC_LEVEL
	int dvfs_level = DVFS_LEVEL_NORMAL;
#endif
	bool is_tws = false;
#ifdef CONFIG_TWS
	uint8_t codec;
#endif

#ifdef CONFIG_PROPERTY
	property_flush_req_deal();
#endif

	media_player_t *handle = mem_malloc(sizeof(media_player_t));
	if (!handle) {
		return NULL;
	}

	/* fall back to media player handle */
	if (!init_param->user_data)
		init_param->user_data = handle;

	os_sem_init(&return_notify, 0, 1);

	srv_param.user_param = init_param;
	srv_param.mediasrv_handle = NULL;

#if defined(CONFIG_MUSIC_EFFECT)
	/*
	 * TTS uses parser+decoder on the global DSP session. Initializing the
	 * music postprocessor then deadlocks in dsp_session_submit_command
	 * (dsp_session_open: opened already).
	 */
	if (init_param->stream_type == AUDIO_STREAM_TTS &&
	    init_param->efx_stream_type != 0) {
		SYS_LOG_INF("skip music effect for TTS (efx was %u)",
			    init_param->efx_stream_type);
		init_param->efx_stream_type = 0;
	}
#endif

#ifdef CONFIG_TWS
	/**this to get media runtime from media service*/
	srv_param.tws_observer = init_param->support_tws ? bt_manager_tws_get_runtime_observer() : NULL;

	if (srv_param.tws_observer && init_param->stream_type != AUDIO_STREAM_LOCAL_MUSIC) {
		if (init_param->format == AAC_TYPE) {
			codec = 2;		/* BT_A2DP_MPEG2 */
		} else {
			codec = 0;		/* BT_A2DP_SBC */
		}
		bt_manager_tws_notify_start_play(init_param->stream_type, codec, init_param->sample_rate);
	}
#else
	//srv_param.tws_observer = NULL;
#endif

#ifdef CONFIG_TWS_MONO_MODE
	srv_param.tws_mode = MEDIA_TWS_MODE_MONO;
#else
	srv_param.tws_mode = MEDIA_TWS_MODE_STEREO;
#endif

	/* for compatibility */
	srv_param.tws_samplerate_44_48_only = 1;

	msg.type = MSG_MEDIA_SRV_OPEN;
	msg.ptr = &srv_param;
	msg.callback = _media_service_default_callback;
	msg.sync_sem = &return_notify;

	os_mutex_lock(&media_srv_mutex, OS_FOREVER);

#ifdef CONFIG_TWS
	is_tws = srv_param.tws_observer ? true : false;
#endif

#ifdef CONFIG_DVFS_DYNAMIC_LEVEL

	dvfs_level = _media_player_get_dvfs_level(init_param->stream_type,init_param->format, is_tws, init_param->dumpable);

	SYS_LOG_INF("tws %d type %d dvfs %d\n", is_tws, init_param->stream_type, dvfs_level);
	dvfs_set_level(dvfs_level, "media");
#endif

	if (!send_async_msg(MEDIA_SERVICE_NAME, &msg)) {
		goto error_exit;
	}

	if (os_sem_take(&return_notify, OS_FOREVER) != 0) {
		goto error_exit;
	}

	if (!srv_param.mediasrv_handle) {
		goto error_exit;
	}

#ifdef CONFIG_TWS
	if (srv_param.tws_observer && init_param->stream_type == AUDIO_STREAM_LOCAL_MUSIC) {
		codec = 0;		/* BT_A2DP_SBC */
		bt_manager_tws_notify_start_play(init_param->stream_type, codec, init_param->sample_rate);
	}
#endif

	handle->media_srv_handle = srv_param.mediasrv_handle;
	handle->type = init_param->type;
	handle->is_tws = is_tws;

#ifdef CONFIG_DVFS_DYNAMIC_LEVEL
	handle->dvfs_level = dvfs_level;
#endif

	if (init_param->dumpable && !current_media_dumpable_player) {
		current_media_dumpable_player = handle;
	}

	if (!current_media_main_player && (init_param->type & MEDIA_SRV_TYPE_PLAYBACK))
		current_media_main_player = handle;

	media_player_ref_cnt++;

	_media_player_check_audio_effect(handle, init_param->stream_type);
#ifdef CONFIG_MUSIC_EXTERNAL_EFFECT_MIC_STREAM
	ex_mic_stream_start(handle, init_param->stream_type);
#endif

	if (is_tws) {
#if CONFIG_TWS_AUDIO_OUT_MODE == 0
#ifdef CONFIG_PROPERTY
		int tws_mode = property_get_int("TWS_MODE", 0);
#else
		int tws_mode = MEDIA_OUTPUT_MODE_DEFAULT;
#endif
		if (tws_mode == MEDIA_OUTPUT_MODE_LEFT || tws_mode == MEDIA_OUTPUT_MODE_RIGHT) {
			media_player_set_output_mode(handle, tws_mode);
		}
#elif CONFIG_TWS_AUDIO_OUT_MODE == 1
		media_player_set_output_mode(handle, MEDIA_OUTPUT_MODE_DEFAULT);
#elif CONFIG_TWS_AUDIO_OUT_MODE == 2
		media_player_set_output_mode(handle, MEDIA_OUTPUT_MODE_DEFAULT);
		media_player_set_effect_output_mode(handle, MEDIA_EFFECT_OUTPUT_L_R_MIX);
#endif
	}

	os_mutex_unlock(&media_srv_mutex);

	_notify_player_lifecycle_changed(handle, PLAYER_EVENT_OPEN, init_param, sizeof(*init_param));

	return handle;
error_exit:
#ifdef CONFIG_DVFS_DYNAMIC_LEVEL
	SYS_LOG_INF("unset tws %d type %d dvfs %d\n", is_tws, init_param->stream_type, dvfs_level);
	dvfs_unset_level(dvfs_level, "media");
#endif

	os_mutex_unlock(&media_srv_mutex);
	mem_free(handle);
	return NULL;
}

int media_player_play(media_player_t *handle)
{
	struct app_msg msg = {0};
	int ret = 0;

	if (!handle || !handle->media_srv_handle) {
		return -EINVAL;
	}

	msg.type = MSG_MEDIA_SRV_PLAY;
	msg.ptr = handle->media_srv_handle;

#ifdef CONFIG_SYS_WAKELOCK
	os_mutex_lock(&media_srv_mutex, OS_FOREVER);
	media_player_lock_cnt++;
	SYS_LOG_INF("count:%d", media_player_lock_cnt);
	sys_wake_lock(WAKELOCK_PLAYER);
	os_mutex_unlock(&media_srv_mutex);
#endif

	ret = send_async_msg(MEDIA_SERVICE_NAME, &msg);
	if (ret)
		_notify_player_lifecycle_changed(handle, PLAYER_EVENT_PLAY, NULL, 0);

	SYS_EVENT_INF(EVENT_PLAYER_START, handle->type, (((int)handle)), media_player_lock_cnt, ret);
	return !ret;
}

int media_player_stop(media_player_t *handle)
{
	struct app_msg msg = {0};
	int ret = 0;

	if (!handle || !handle->media_srv_handle) {
		return -EINVAL;
	}

	msg.type = MSG_MEDIA_SRV_STOP;
	msg.ptr = handle->media_srv_handle;

#ifdef CONFIG_SYS_WAKELOCK
	os_mutex_lock(&media_srv_mutex, OS_FOREVER);
	media_player_lock_cnt--;
	SYS_LOG_INF("count:%d", media_player_lock_cnt);
	if (media_player_lock_cnt == 0) {
		sys_wake_unlock(WAKELOCK_PLAYER);
	}
	os_mutex_unlock(&media_srv_mutex);
#endif

#ifdef CONFIG_MUSIC_EXTERNAL_EFFECT_MIC_STREAM
	ex_mic_stream_stop();
#endif

#ifdef CONFIG_TWS
	if (handle->is_tws) {
		bt_manager_tws_notify_stop_play();
	}
#endif

	ret = send_async_msg(MEDIA_SERVICE_NAME, &msg);
	if (ret)
		_notify_player_lifecycle_changed(handle, PLAYER_EVENT_STOP, NULL, 0);

	SYS_EVENT_INF(EVENT_PLAYER_STOP, handle->type, (((int)handle)), media_player_lock_cnt, ret);
	return !ret;
}

int media_player_pause(media_player_t *handle)
{
	struct app_msg msg = {0};
	int ret = 0;

	if (!handle || !handle->media_srv_handle) {
		return -EINVAL;
	}

	msg.type = MSG_MEDIA_SRV_PAUSE;
	msg.ptr = handle->media_srv_handle;

	ret = send_async_msg(MEDIA_SERVICE_NAME, &msg);
	if (ret)
		_notify_player_lifecycle_changed(handle, PLAYER_EVENT_PAUSE, NULL, 0);

	return !ret;
}

int media_player_resume(media_player_t *handle)
{
	struct app_msg msg = {0};
	int ret = 0;

	if (!handle || !handle->media_srv_handle) {
		return -EINVAL;
	}

	msg.type = MSG_MEDIA_SRV_RESUME;
	msg.ptr = handle->media_srv_handle;

	ret = send_async_msg(MEDIA_SERVICE_NAME, &msg);
	if (ret)
		_notify_player_lifecycle_changed(handle, PLAYER_EVENT_RESUNE, NULL, 0);

	return !ret;
}

int media_player_seek(media_player_t *handle, media_seek_info_t *info)
{
	struct app_msg msg = {0};
	os_sem return_notify;
	media_srv_param_t *srv_param = NULL;

	if (!handle || !handle->media_srv_handle) {
		return -EINVAL;
	}

	srv_param = mem_malloc(sizeof(*srv_param));
	if (!srv_param)
		return -ENOMEM;

	os_sem_init(&return_notify, 0, 1);

	srv_param->handle = handle->media_srv_handle;
	srv_param->param.pbuf = info;
	srv_param->param.plen = sizeof(*info);

	msg.ptr = srv_param;
	msg.type = MSG_MEDIA_SRV_SEEK;
	msg.callback = _media_service_default_callback;
	msg.sync_sem = &return_notify;

	if (false == send_async_msg(MEDIA_SERVICE_NAME, &msg)) {
		mem_free(srv_param);
		return -EBUSY;
	}

	if (os_sem_take(&return_notify, OS_FOREVER)) {
		mem_free(srv_param);
		return -ETIME;
	}

	mem_free(srv_param);
	return 0;
}

int media_player_get_parameter(media_player_t *handle,
		int pname, void *param, unsigned int psize)
{
	struct app_msg msg = {0};
	os_sem return_notify;
	media_srv_param_t *srv_param = NULL;
	int ret;

	if (!handle || !handle->media_srv_handle) {
		return -EINVAL;
	}

	srv_param = mem_malloc(sizeof(*srv_param));
	if (!srv_param)
		return -ENOMEM;

	os_sem_init(&return_notify, 0, 1);

	srv_param->handle = handle->media_srv_handle;
	srv_param->param.type = pname;
	srv_param->param.pbuf = param;
	srv_param->param.plen = psize;
	srv_param->result = 0;

	msg.ptr = srv_param;
	msg.type = MSG_MEDIA_SRV_GET_PARAMETER;
	msg.callback = _media_service_default_callback;
	msg.sync_sem = &return_notify;

	if (false == send_async_msg(MEDIA_SERVICE_NAME, &msg)) {
		mem_free(srv_param);
		return -EBUSY;
	}

	if (os_sem_take(&return_notify, OS_FOREVER)) {
		mem_free(srv_param);
		return -ETIME;
	}

	ret = srv_param->result;
	mem_free(srv_param);
	return ret;
}

/* use synchronized message to avoid mem_malloc for large parameter, whose psize is not zero */
int media_player_set_parameter(media_player_t *handle,
		int pname, void *param, unsigned int psize)
{
	struct app_msg msg = {0};
	os_sem return_notify;
	media_srv_param_t *srv_param = NULL;
	int ret = 0;

	SYS_LOG_INF("%p, %d, %p, %d", handle, pname, param, psize);
	if (!handle || !handle->media_srv_handle) {
		SYS_LOG_ERR("NULL");
		return -EINVAL;
	}

	srv_param = mem_malloc(sizeof(media_srv_param_t));
	if (!srv_param) {
		SYS_LOG_ERR("malloc");
		return -ENOMEM;
	}

	srv_param->handle = handle->media_srv_handle;
	srv_param->param.type = pname;
	srv_param->param.pbuf = param;
	srv_param->param.plen = psize;
	srv_param->result = 0;

	msg.ptr = srv_param;
	msg.type = MSG_MEDIA_SRV_SET_PARAMETER;

	if (psize > 0) {
		os_sem_init(&return_notify, 0, 1);
		msg.callback = _media_service_default_callback;
		msg.sync_sem = &return_notify;
	}

	if (false == send_async_msg(MEDIA_SERVICE_NAME, &msg)) {
		mem_free(srv_param);
		return -EBUSY;
	}

	if (psize > 0) {
		if (os_sem_take(&return_notify, OS_FOREVER)) {
			mem_free(srv_param);
			return -ETIME;
		}

		ret = srv_param->result;
		mem_free(srv_param);
	}

	return ret;
}

int media_player_get_global_parameter(media_player_t *handle,
		int pname, void *param, unsigned int psize)
{
	struct app_msg msg = {0};
	os_sem return_notify;
	media_srv_param_t *srv_param = NULL;
	int ret;

	srv_param = mem_malloc(sizeof(*srv_param));
	if (!srv_param)
		return -ENOMEM;

	os_sem_init(&return_notify, 0, 1);

	srv_param->handle = handle ? handle->media_srv_handle : NULL;
	srv_param->param.type = pname;
	srv_param->param.pbuf = param;
	srv_param->param.plen = psize;
	srv_param->result = 0;

	msg.ptr = srv_param;
	msg.type = MSG_MEDIA_SRV_GET_GLOBAL_PARAMETER;
	msg.callback = _media_service_default_callback;
	msg.sync_sem = &return_notify;

	if (false == send_async_msg(MEDIA_SERVICE_NAME, &msg)) {
		mem_free(srv_param);
		return -EBUSY;
	}

	if (os_sem_take(&return_notify, OS_FOREVER)) {
		mem_free(srv_param);
		return -ETIME;
	}

	ret = srv_param->result;
	mem_free(srv_param);
	return ret;
}

/* use synchronized message to avoid mem_malloc for large parameter, whose psize is not zero */
int media_player_set_global_parameter(media_player_t *handle, int pname, void *param, unsigned int psize)
{
	struct app_msg msg = {0};
	os_sem return_notify;
	media_srv_param_t *srv_param = NULL;
	int ret = 0;

	srv_param = mem_malloc(sizeof(media_srv_param_t));
	if (!srv_param)
		return -ENOMEM;

	srv_param->handle = handle ? handle->media_srv_handle : NULL;
	srv_param->param.type = pname;
	srv_param->param.pbuf = param;
	srv_param->param.plen = psize;
	srv_param->result = 0;

	msg.ptr = srv_param;
	msg.type = MSG_MEDIA_SRV_SET_GLOBAL_PARAMETER;

	if (psize > 0) {
		os_sem_init(&return_notify, 0, 1);
		msg.callback = _media_service_default_callback;
		msg.sync_sem = &return_notify;
	}

	if (false == send_async_msg(MEDIA_SERVICE_NAME, &msg)) {
		mem_free(srv_param);
		return -EBUSY;
	}

	if (psize > 0) {
		if (os_sem_take(&return_notify, OS_FOREVER)) {
			mem_free(srv_param);
			return -ETIME;
		}

		ret = srv_param->result;
		mem_free(srv_param);
	}

	return ret;
}

int media_player_close(media_player_t *handle)
{
	struct app_msg msg = {0};
	os_sem return_notify;

	os_sem_init(&return_notify, 0, 1);

	msg.type = MSG_MEDIA_SRV_CLOSE;
	msg.ptr = handle->media_srv_handle;
	msg.callback = _media_service_default_callback;
	msg.sync_sem = &return_notify;

	os_mutex_lock(&media_srv_mutex, OS_FOREVER);

	if (!send_async_msg(MEDIA_SERVICE_NAME, &msg)) {
		SYS_LOG_ERR("MSG_MEDIA_SRV_CLOSE send failed");
		SYS_EVENT_ERR(EVENT_PLAYER_ERROR_TYPE, 'r', __LINE__);
		goto error_exit;
	}

	if (os_sem_take(&return_notify, OS_FOREVER) != 0) {
		goto error_exit;
	}

	if (current_media_dumpable_player == handle)
		current_media_dumpable_player = NULL;
	if (current_media_main_player == handle)
		current_media_main_player = NULL;

	media_player_ref_cnt--;

error_exit:
#ifdef CONFIG_DVFS_DYNAMIC_LEVEL
	SYS_LOG_INF("dvfs level %d\n", handle->dvfs_level);
	dvfs_unset_level(handle->dvfs_level, "media");
#endif
	os_mutex_unlock(&media_srv_mutex);

	_notify_player_lifecycle_changed(handle, PLAYER_EVENT_CLOSE, NULL, 0);
	mem_free(handle);
	return 0;
}

int media_player_dump_data(media_player_t *handle, int num, const uint8_t tags[], struct acts_ringbuf *bufs[])
{
	struct app_msg msg = {0};
	os_sem return_notify;
	media_data_dump_info_t dump_info;
	media_srv_param_t *srv_param = NULL;

	if (!handle)
		handle = current_media_dumpable_player;

	if (!handle || !handle->media_srv_handle) {
		return -EINVAL;
	}

	srv_param = mem_malloc(sizeof(*srv_param));
	if (!srv_param)
		return -ENOMEM;

	os_sem_init(&return_notify, 0, 1);

	dump_info.num = num;
	dump_info.tags = tags;
	dump_info.bufs = bufs;

	srv_param->handle = handle->media_srv_handle;
	srv_param->param.type = 0;
	srv_param->param.pbuf = &dump_info;
	srv_param->param.plen = sizeof(dump_info);

	msg.ptr = srv_param;
	msg.type = MSG_MEDIA_SRV_DUMP_DATA;
	msg.callback = _media_service_default_callback;
	msg.sync_sem = &return_notify;

	if (false == send_async_msg(MEDIA_SERVICE_NAME, &msg)) {
		mem_free(srv_param);
		return -EBUSY;
	}

	if (os_sem_take(&return_notify, OS_FOREVER)) {
		mem_free(srv_param);
		return -ETIME;
	}

	mem_free(srv_param);
	return 0;
}

media_player_t *media_player_get_current_dumpable_player(void)
{
	return current_media_dumpable_player;
}

media_player_t *media_player_get_current_main_player(void)
{
	return current_media_main_player;
}

bool media_player_is_working(void)
{
	return (media_player_ref_cnt != 0);
}

int media_player_set_lifecycle_notifier(media_srv_event_notify_t notify)
{
	os_mutex_lock(&media_srv_mutex, OS_FOREVER);
	pfn_lifecycle_notify = notify;
	SYS_LOG_INF("%p", notify);
	os_mutex_unlock(&media_srv_mutex);
	return 0;
}

/* FIXME: Should sync when encoder library's behaviour changed ? */
int media_player_repair_filhdr(media_player_t *handle, uint8_t format, io_stream_t fstream)
{
	const uint32_t header_size = 512;
	uint32_t data_size;

	switch (format) {
	case WAV_TYPE:
		break;
	case MP3_TYPE: /* MP2 actually, no header */
	case OPUS_TYPE:
		return 0;
	default:
		return -EINVAL;
	}

	if (stream_seek(fstream, 0, SEEK_DIR_END))
		return -EIO;

	data_size = stream_tell(fstream);
	if (data_size < header_size)
		return -EINVAL;

	/* lower byte first */
	data_size = sys_cpu_to_le32(data_size - header_size);

	/* override last 4 bytes of header */
	if (stream_seek(fstream, header_size - 4,  SEEK_DIR_BEG))
		return -EIO;

	stream_write(fstream, (void *)&data_size, sizeof(data_size));
	return 0;
}

int media_player_trigger_audio_track_start(media_player_t *handle)
{
	extern int playback_service_trigger_track_start(void *handle_param);

	if (!handle || !handle->media_srv_handle) {
		return -EINVAL;
	}

	media_srv_handle_t *media_handle = (media_srv_handle_t *)handle->media_srv_handle;
	if (media_handle->subsrv_handles[MEDIA_PLAYBACK]) {
		return playback_service_trigger_track_start(media_handle->subsrv_handles[MEDIA_PLAYBACK]);
	}

	return -EINVAL;
}

int media_player_trigger_audio_record_start(media_player_t *handle)
{
	extern int capture_service_trigger_record_start(void *handle_param);

	if (!handle || !handle->media_srv_handle) {
		return -EINVAL;
	}

	media_srv_handle_t *media_handle = (media_srv_handle_t *)handle->media_srv_handle;
	if (media_handle->subsrv_handles[MEDIA_CAPTURE]) {
		if (media_handle->subsrv_handles[MEDIA_PLAYBACK]) {
			extern int playback_service_pre_load(void *handle_param);
			playback_service_pre_load(media_handle->subsrv_handles[MEDIA_PLAYBACK]);
		}
		return capture_service_trigger_record_start(media_handle->subsrv_handles[MEDIA_CAPTURE]);
	}

	return -EINVAL;
}

int media_player_get_output_energy_sample(media_player_t *handle, int type, void *param, int param_len)
{
	media_srv_param_t srv_param;

	srv_param.handle = handle->media_srv_handle;
	srv_param.param.type = type;
	srv_param.param.pbuf = param;
	srv_param.param.plen = param_len;

	if (!handle || !handle->media_srv_handle) {
		return -EINVAL;
	}

	media_srv_handle_t *media_handle = (media_srv_handle_t *)handle->media_srv_handle;
	if (media_handle->subsrv_handles[MEDIA_PLAYBACK]) {
		extern int playback_service_get_output_energy_sample(void *handle_param, media_srv_param_t *param);
		return playback_service_get_output_energy_sample(media_handle->subsrv_handles[MEDIA_PLAYBACK], &srv_param);
	}

	return -EINVAL;
}

__ramfunc int media_player_audio_track_trigger_callback(void *audio_track, int wait_us)
{
	struct audio_track_t *handle = (struct audio_track_t *)audio_track;
	if (handle->phy_dma){
		u32_t digtal_ctl_reg = AUDIO_DAC_REG_BASE;
		if (audio_policy_get_out_channel_id(handle->stream_type) == AOUT_FIFO_I2STX0){
			digtal_ctl_reg = AUDIO_I2STX0_REG_BASE;
		}
		if (wait_us > 0){
			sys_write32(sys_read32(digtal_ctl_reg)&(~0x1),digtal_ctl_reg);
			k_busy_wait(wait_us);
			sys_write32(sys_read32(digtal_ctl_reg)|0x1,digtal_ctl_reg);
		}
		acts_dma_start((dma_reg_t *)handle->phy_dma);
		printk("[mp] start dma %d -- %d\n", wait_us, k_cycle_get_32());
		//if support subwoofer
		struct audio_track_t *sub_handle = audio_system_get_audio_track_handle(AUDIO_STREAM_SUBWOOFER);
		if (sub_handle && sub_handle->phy_dma){
			acts_dma_start((dma_reg_t *)sub_handle->phy_dma);
		}
	} else {
		SYS_LOG_WRN("aout not prepare start %d", k_cycle_get_32());
		return -1;
	}
	return 0;
}

#define MEDIA_VERSION_INFO 0x1
u16_t media_player_get_version(void)
{
    u16_t version_info = MEDIA_VERSION_INFO;
    printk("[mp] version:%d\n", version_info);
    return version_info;
}

int media_player_set_pair_version(u16_t version_info)
{
    printk("[mp] pair version:%d\n", version_info);
    return 0;
}
