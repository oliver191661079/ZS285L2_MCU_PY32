/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file tts manager interface
 */
#include <os_common_api.h>
#include <audio_system.h>
#include <media_player.h>
#include <buffer_stream.h>
#include <file_stream.h>
#include <app_manager.h>
#include <mem_manager.h>
#include <tts_manager.h>
#include <sdfs.h>
#include <ringbuff_stream.h>
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
#include <soc_dvfs.h>
#endif
#ifndef SYS_LOG_DOMAIN
#define SYS_LOG_DOMAIN "TTS"
#endif
#ifndef SYS_LOG_LEVEL
#define SYS_LOG_LEVEL SYS_LOG_LEVEL_INFO
#endif
#include <logging/sys_log.h>

#if (SYS_LOG_LEVEL >= SYS_LOG_LEVEL_INFO)
static int tts_player_count = 0;
#endif

#define CONFIG_TTS_USE_THREADTIMER

#ifdef CONFIG_TWS_UI_EVENT_SYNC
struct tts_item_t {
	/* 1st word reserved for use by list */
	sys_snode_t node;
	void *player_handle;
	io_stream_t tts_stream;
	uint64_t bt_clk;
	char *pattern;
	uint8_t tts_mode;
	uint8_t tts_mix_mode;
	uint8_t tele_num_index;
	char tts_file_name[0];
};
#else
struct tts_item_t {
	/* 1st word reserved for use by list */
	sys_snode_t node;
	void *player_handle;
	io_stream_t tts_stream;
	uint8_t tts_mode;
	uint8_t tts_mix_mode;
	uint8_t tts_file_name[0];
};
#endif

struct tts_manager_ctx_t {
	const struct tts_config_t *tts_config;
	sys_slist_t tts_list;
	struct tts_item_t *tts_curr;
	os_mutex tts_mutex;
	uint32_t tts_manager_locked:8;
	uint32_t tts_item_num:8;
	uint32_t dvfs_set:1;
	uint32_t filter_enable:1;
	tts_event_nodify lisener;
#ifdef CONFIG_TTS_USE_THREADTIMER
    struct thread_timer play_timer;
    struct thread_timer stop_timer;
#else
	os_delayed_work stop_work;
#endif
};

static struct tts_manager_ctx_t *tts_manager_ctx;

static struct tts_manager_ctx_t *_tts_get_ctx(void)
{
	return tts_manager_ctx;
}


static void _tts_event_nodify_callback(struct app_msg *msg, int result, void *reply)
{
	if (msg->sync_sem) {
		os_sem_give((os_sem *)msg->sync_sem);
	}
}

static void _tts_event_nodify(uint32_t event, uint8_t sync_mode)
{
	struct tts_manager_ctx_t *tts_ctx = _tts_get_ctx();

	struct app_msg msg = {0};

	MSG_SEND_TIME_STAT_START();

	msg.type = MSG_TTS_EVENT;
	msg.value = event;
	if (sync_mode) {

		os_sem return_notify;

		os_sem_init(&return_notify, 0, 1);
		msg.callback = _tts_event_nodify_callback;
		msg.sync_sem = &return_notify;
		if (!send_async_msg(CONFIG_FRONT_APP_NAME, &msg)) {
			return;
		}

		os_mutex_unlock(&tts_ctx->tts_mutex);
		if (os_sem_take(&return_notify, OS_FOREVER) != 0) {
			return;
		}
		os_mutex_lock(&tts_ctx->tts_mutex, OS_FOREVER);
	} else {
		if (!send_async_msg(CONFIG_FRONT_APP_NAME, &msg)) {
			return;
		}
	}


	MSG_SEND_TIME_STAT_STOP();
}

static io_stream_t _tts_create_stream(struct tts_item_t *item)
{
	struct tts_manager_ctx_t *tts_ctx = _tts_get_ctx();
	io_stream_t stream = NULL;
	struct buffer_t buffer;

	if (tts_ctx->tts_config->tts_storage_media == TTS_SOTRAGE_SDFS) {

	#ifdef CONFIG_SD_FS
		if (sd_fmap(&item->tts_file_name[0], (void **)&buffer.base, &buffer.length) != 0) {
			goto exit;
		}
	#endif

		buffer.cache_size = 0;
		if(!(item->tts_mode & TTS_MIX_MODE_WITH_TRACK)){
			stream = buffer_stream_create(&buffer);
		}else{
			stream = ringbuff_stream_create_ext(buffer.base, buffer.length);

			stream_open(stream, MODE_IN_OUT);

			stream_write(stream, NULL, buffer.length);

			return stream;
		}
		if (!stream) {
			SYS_LOG_ERR("create failed\n");
			goto exit;
		}
	} else if (tts_ctx->tts_config->tts_storage_media == TTS_SOTRAGE_SDCARD) {
	#ifdef CONFIG_FILE_STREAM
		stream = file_stream_create(&item->tts_file_name[0]);
		if (!stream) {
			SYS_LOG_ERR("create failed\n");
			goto exit;
		}
	#endif
	}

	if (NULL != stream) {
		if (stream_open(stream, MODE_IN) != 0) {
			SYS_LOG_ERR("open failed\n");
			stream_destroy(stream);
			stream = NULL;
			goto exit;
		}
	}


	SYS_LOG_INF("tts stream %p tts_item %s\n", stream, &item->tts_file_name[0]);
exit:
	return stream;
}

static void _tts_remove_current_node(struct tts_manager_ctx_t *tts_ctx)
{
	struct tts_item_t *tts_item;

	if (!tts_ctx->tts_curr)
		return;

	tts_item  = tts_ctx->tts_curr;

	SYS_LOG_INF("%s\n", tts_item->tts_file_name);
	if (tts_item->player_handle) {
		SYS_LOG_INF("tts player: %d\n", tts_player_count--);
		media_player_fade_out(tts_item->player_handle, 10);
		os_sleep(15);
		media_player_stop(tts_item->player_handle);
		media_player_close(tts_item->player_handle);
		tts_item->player_handle = NULL;
	}else{
		if(tts_item->tts_mix_mode){
			tts_merge_manager_stop();
		}
	}

	if (tts_item->tts_stream) {
		stream_close(tts_item->tts_stream);
		stream_destroy(tts_item->tts_stream);
		tts_item->tts_stream = NULL;
	}

	tts_manager_ctx->tts_curr = NULL;

	if (tts_ctx->lisener) {
		tts_ctx->lisener(&tts_item->tts_file_name[0], TTS_EVENT_STOP_PLAY);
	}

	if(!tts_item->tts_mix_mode){
		_tts_event_nodify(TTS_EVENT_STOP_PLAY, 0);
		if(strncmp(tts_item->tts_file_name, "poweroff", 8) == 0){
			_tts_event_nodify(TTS_EVENT_POWER_OFF, 0);
			tts_manager_lock();
		}
	}

	mem_free(tts_item);
}

#ifdef CONFIG_TTS_USE_THREADTIMER

static void _tts_manager_play_timer_handle(struct thread_timer *ttimer, void *expiry_fn_arg)
{
#if (SYS_LOG_LEVEL >= SYS_LOG_LEVEL_INFO)
	struct tts_manager_ctx_t *tts_ctx = _tts_get_ctx();
#endif
	SYS_LOG_INF("%d", tts_ctx->tts_manager_locked);
	tts_manager_play_process();
}

static void _tts_manager_triggle_play_tts(struct tts_manager_ctx_t *tts_ctx)
{
	thread_timer_start_prio(&tts_ctx->play_timer, 0, 0, app_manager_get_apptid(CONFIG_SYS_APP_NAME));
}

static void _tts_manager_stop_timer_handle(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	struct tts_manager_ctx_t *tts_ctx = _tts_get_ctx();

	os_mutex_lock(&tts_ctx->tts_mutex, OS_FOREVER);

	SYS_LOG_INF("");
	_tts_remove_current_node(tts_ctx);
	os_mutex_unlock(&tts_ctx->tts_mutex);

	_tts_manager_triggle_play_tts(tts_ctx);

}

static void _tts_manager_trigger_stop_tts(struct tts_manager_ctx_t *tts_ctx)
{
	thread_timer_start_prio(&tts_ctx->stop_timer, 0, 0, app_manager_get_apptid(CONFIG_SYS_APP_NAME));
}

#else
static void _tts_manager_triggle_play_tts(struct tts_manager_ctx_t *tts_ctx)
{
	struct app_msg msg = {0};
	struct tts_manager_ctx_t *tts_ctx = _tts_get_ctx();
	msg.type = MSG_TTS_EVENT;
	msg.cmd = TTS_EVENT_START_PLAY;
	SYS_LOG_INF("%d", tts_ctx->tts_manager_locked);
	send_async_msg(CONFIG_SYS_APP_NAME, &msg);
}

static void _tts_manager_stop_work(os_work *work)
{

	struct tts_manager_ctx_t *tts_ctx = _tts_get_ctx();

	os_mutex_lock(&tts_ctx->tts_mutex, OS_FOREVER);

	SYS_LOG_INF("");
	_tts_remove_current_node(tts_ctx);
	os_mutex_unlock(&tts_ctx->tts_mutex);

	_tts_manager_triggle_play_tts(tts_ctx);
}

static void _tts_manager_trigger_stop_tts(struct tts_manager_ctx_t *tts_ctx)
{
	os_delayed_work_submit(&tts_ctx->stop_work, OS_MSEC(40));
}

#endif

static void _tts_manager_play_event_notify(uint32_t event, void *data, uint32_t len, void *user_data)
{
	struct tts_manager_ctx_t *tts_ctx = _tts_get_ctx();

	SYS_LOG_INF("event %d, data %p, len %d\n", event, data, len);

	switch (event) {
	case PLAYBACK_EVENT_STOP_COMPLETE:
	case PLAYBACK_EVENT_STOP_ERROR:
	#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		if(tts_ctx->dvfs_set) {
			tts_ctx->dvfs_set = 0;
			soc_dvfs_unset_level(SOC_DVFS_LEVEL_FULL_PERFORMANCE, "tts");
		}
	#endif
		_tts_manager_trigger_stop_tts(tts_ctx);
		break;
	case PARSER_EVENT_STOP_COMPLETE:
	#if 0
		if(tts_ctx->dvfs_set) {
			tts_ctx->dvfs_set = 0;
			soc_dvfs_unset_level(SOC_DVFS_LEVEL_FULL_PERFORMANCE, "tts");
		}
	#endif
		break;
	}
}

static int _tts_start_play(struct tts_item_t *tts_item,io_stream_t tts_stream)
{
	struct tts_manager_ctx_t *tts_ctx = _tts_get_ctx();
	int ret = 0;
	media_init_param_t init_param;
	SYS_LOG_INF("%s", tts_item->tts_file_name);

	memset(&init_param, 0, sizeof(media_init_param_t));

	tts_ctx->tts_curr = tts_item;

	tts_item->tts_stream = tts_stream;

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	if(!tts_ctx->dvfs_set) {
		tts_ctx->dvfs_set = 1;
		soc_dvfs_set_level(SOC_DVFS_LEVEL_FULL_PERFORMANCE, "tts");
	}
#endif

	// audio_system_set_output_sample_rate(48);

	init_param.stream_type = AUDIO_STREAM_TTS;
	init_param.efx_stream_type = AUDIO_STREAM_TTS;

	if(!(tts_item->tts_mode & TTS_MIX_MODE_WITH_TRACK)){
		init_param.format = MP3_TYPE;
		init_param.type = MEDIA_SRV_TYPE_PARSER_AND_PLAYBACK;
		init_param.sample_rate = 16;
	}else{
		init_param.format = LOCAL_PCM_TYPE;
		init_param.type = MEDIA_SRV_TYPE_PLAYBACK;
		init_param.sample_rate = 48;
	}

	init_param.sample_bits = 32;
	init_param.channels = 1;
	init_param.input_stream = tts_item->tts_stream;
	init_param.output_stream = NULL;
	init_param.event_notify_handle = _tts_manager_play_event_notify;
#ifdef CONFIG_TWS_UI_EVENT_SYNC
	init_param.support_tws = (tts_item->bt_clk == UINT64_MAX) ? 0 : 1;
#endif

	tts_item->player_handle = media_player_open(&init_param);
	if (!tts_item->player_handle) {
		SYS_LOG_WRN("no player");
		_tts_remove_current_node(tts_ctx);
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		if(tts_ctx->dvfs_set) {
			tts_ctx->dvfs_set = 0;
			soc_dvfs_unset_level(SOC_DVFS_LEVEL_FULL_PERFORMANCE, "tts");
		}
#endif
		ret = -2;
		goto exit;
	}

#ifdef CONFIG_TWS_UI_EVENT_SYNC
    if(init_param.support_tws) {
        media_player_set_sync_play_time(tts_item->player_handle, tts_item->bt_clk);
    }
#endif

	SYS_LOG_INF("tts player: %d\n", tts_player_count++);
	media_player_play(tts_item->player_handle);

	if(tts_item->tts_mode & TTS_MIX_MODE_WITH_TRACK){
		media_player_trigger_audio_track_start(tts_item->player_handle);
	}
exit:
	return ret;
}

void _tts_merge_mode_stop_cb(void)
{
	struct tts_manager_ctx_t *tts_ctx = _tts_get_ctx();

	_tts_manager_trigger_stop_tts(tts_ctx);
}

int tts_manager_play_process(void)
{
	sys_snode_t *head;
	struct tts_item_t *tts_item;
	struct tts_manager_ctx_t *tts_ctx = _tts_get_ctx();
	int res = 0;

#ifdef CONFIG_TEMPORARY_CLOSE_TTS
	return 0;
#endif

	//mutex_unlock will change the thread priority, so os_thread_priority_set
	//needs to set before os_mutex_lock
	int prio = os_thread_priority_get(os_current_get());
	os_thread_priority_set(os_current_get(), -1);

	os_mutex_lock(&tts_ctx->tts_mutex, OS_FOREVER);

	if (sys_slist_is_empty(&tts_ctx->tts_list)) {
		res = -ENOENT;
		goto exit;
	}

	if (tts_ctx->tts_curr || tts_ctx->tts_manager_locked) {
		SYS_LOG_INF("tts_curr %p, locked: %d\n", tts_ctx->tts_curr, tts_ctx->tts_manager_locked);
		res = -ENOLCK;
		goto exit;
	}

	head  = sys_slist_peek_head(&tts_ctx->tts_list);
	tts_item = CONTAINER_OF(head, struct tts_item_t, node);
	sys_slist_find_and_remove(&tts_ctx->tts_list, head);
	tts_ctx->tts_item_num--;

	if (tts_ctx->lisener) {
		tts_ctx->lisener(&tts_item->tts_file_name[0], TTS_EVENT_START_PLAY);
	}

	struct audio_track_t *track = audio_system_get_track();
	if (media_player_get_current_main_player() != NULL && (tts_item->tts_mode & TTS_MIX_MODE_WITH_TRACK)
		&& track && track->started){
		tts_item->tts_mix_mode = true;
	}else{
		tts_item->tts_mix_mode = false;
	}

	if (!tts_item->tts_mix_mode){

		io_stream_t tts_stream = _tts_create_stream(tts_item);

		if(tts_stream){
			_tts_event_nodify(TTS_EVENT_START_PLAY, 1);

			res = _tts_start_play(tts_item,tts_stream);

			if(0 != res){
				_tts_event_nodify(TTS_EVENT_STOP_PLAY, 0);
				if (tts_ctx->lisener) {
					tts_ctx->lisener(&tts_item->tts_file_name[0], TTS_EVENT_STOP_PLAY);
				}
			}
		}else{
			SYS_LOG_WRN("creat stream failed");
			mem_free(tts_item);
			_tts_manager_triggle_play_tts(tts_ctx);
		}
	}else{
		if(tts_merge_manager_is_running()){
			SYS_LOG_WRN("drop merge tts %s", tts_item->tts_file_name);
		}else{
			tts_ctx->tts_curr = tts_item;
			tts_merge_manager_start(tts_item->tts_file_name, audio_system_get_output_sample_rate(), _tts_merge_mode_stop_cb);
		}
	}
exit:
	os_mutex_unlock(&tts_ctx->tts_mutex);
	os_thread_priority_set(os_current_get(), prio);
	return res;
}



#ifdef CONFIG_TWS_UI_EVENT_SYNC
static struct tts_item_t *_tts_create_item(uint8_t *tts_name, uint32_t mode, uint64_t bt_clk, char *pattern)
{
	struct tts_item_t *item;
	int size;

	size = sizeof(struct tts_item_t) + strlen(tts_name) + 1 + (pattern ? (strlen(pattern) + 1) : 0);
	item = mem_malloc(size);
	if (!item) {
		goto exit;
	}

	memset(item, 0, size);
	item->bt_clk = bt_clk;
	item->tts_mode = mode;
	strcpy(item->tts_file_name, tts_name);

	if (pattern) {
		item->pattern = (char *)item + sizeof(struct tts_item_t) + strlen(tts_name) + 1;
		strcpy(item->pattern, pattern);
	}

exit:
	return item;
}
#else
static struct tts_item_t *_tts_create_item(uint8_t *tts_name, uint32_t mode)
{
	struct tts_item_t *item = mem_malloc(sizeof(struct tts_item_t) + strlen(tts_name));

	if (!item) {
		goto exit;
	}
	memset(item, 0, sizeof(struct tts_item_t) + strlen(tts_name));
	item->tts_mode = mode;
	strcpy(item->tts_file_name, tts_name);

exit:
	return item;
}
#endif

int tts_manager_is_playing(void)
{
	struct tts_manager_ctx_t *tts_ctx = _tts_get_ctx();

	os_mutex_lock(&tts_ctx->tts_mutex, OS_FOREVER);
	int status = tts_ctx->tts_curr != NULL;
	os_mutex_unlock(&tts_ctx->tts_mutex);
	return status;
}

void tts_manager_enable_filter(void)
{
	struct tts_manager_ctx_t *tts_ctx = _tts_get_ctx();

	os_mutex_lock(&tts_ctx->tts_mutex, OS_FOREVER);
	tts_ctx->filter_enable = 1;
	os_mutex_unlock(&tts_ctx->tts_mutex);
	SYS_LOG_INF("\n");
}

void tts_manager_disable_filter(void)
{
	struct tts_manager_ctx_t *tts_ctx = _tts_get_ctx();

	os_mutex_lock(&tts_ctx->tts_mutex, OS_FOREVER);
	tts_ctx->filter_enable = 0;
	os_mutex_unlock(&tts_ctx->tts_mutex);
	SYS_LOG_INF("\n");
}

void tts_manager_unlock(void)
{
	struct tts_manager_ctx_t *tts_ctx = _tts_get_ctx();

	os_mutex_lock(&tts_ctx->tts_mutex, OS_FOREVER);

	if(tts_ctx->tts_manager_locked > 0) {
		tts_ctx->tts_manager_locked--;
	}
	SYS_LOG_INF("%d",tts_ctx->tts_manager_locked);
	os_mutex_unlock(&tts_ctx->tts_mutex);
}

void tts_manager_lock(void)
{
	struct tts_manager_ctx_t *tts_ctx = _tts_get_ctx();

	os_mutex_lock(&tts_ctx->tts_mutex, OS_FOREVER);

	tts_ctx->tts_manager_locked++;
	SYS_LOG_INF("%d",tts_ctx->tts_manager_locked);
	os_mutex_unlock(&tts_ctx->tts_mutex);
}

bool tts_manager_is_locked(void)
{
	bool result = false;

	struct tts_manager_ctx_t *tts_ctx = _tts_get_ctx();

	os_mutex_lock(&tts_ctx->tts_mutex, OS_FOREVER);

	result = (tts_ctx->tts_manager_locked > 0);

	SYS_LOG_INF("%d",tts_ctx->tts_manager_locked);

	os_mutex_unlock(&tts_ctx->tts_mutex);

	return result;
}

void tts_manager_wait_finished(bool clean_all)
{
	int try_cnt = 10000;
	struct tts_item_t *temp, *tts_item;
	struct tts_manager_ctx_t *tts_ctx = _tts_get_ctx();

	SYS_LOG_INF("%d\n", clean_all);
	//avoid tts wait finished and second tts start play
	if(clean_all == true){
		tts_manager_lock();
	}

	/**avoid tts wait finished death loop*/
	while (tts_ctx->tts_curr && try_cnt-- > 0) {
		thread_timer_handle_expired();
		os_sleep(4);
	}

	if (try_cnt == 0) {
		SYS_LOG_WRN("timeout\n");
	}

	SYS_LOG_INF("wait done\n");

	os_mutex_lock(&tts_ctx->tts_mutex, OS_FOREVER);

	if (!sys_slist_is_empty(&tts_ctx->tts_list) && clean_all) {
		SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&tts_ctx->tts_list, tts_item, temp, node) {
			if (tts_item) {
				sys_slist_find_and_remove(&tts_ctx->tts_list, &tts_item->node);
				mem_free(tts_item);
			}
		}
		tts_ctx->tts_item_num = 0;

	}

	os_mutex_unlock(&tts_ctx->tts_mutex);

	if (clean_all == true) {
		tts_manager_unlock();
	}
}

int tts_manager_play(uint8_t *tts_name, uint32_t mode)
{
	int res = 0;
	struct tts_manager_ctx_t *tts_ctx = _tts_get_ctx();
	struct tts_item_t *item = NULL;

#ifdef CONFIG_TEMPORARY_CLOSE_TTS
	return 0;
#endif
	SYS_LOG_INF("%s mode:%d\n", tts_name, mode);
	os_mutex_lock(&tts_ctx->tts_mutex, OS_FOREVER);

	if (tts_ctx->tts_manager_locked > 0) {
		SYS_LOG_INF("locked %d\n",tts_ctx->tts_manager_locked);
		if(strncmp(tts_name, "poweroff", 8) == 0){
			_tts_event_nodify(TTS_EVENT_POWER_OFF, 0);
		}
		res = -ENOLCK;
		goto exit;
	}

	if (!strcmp(tts_name,"keytone.pcm")) {
	#ifdef CONFIG_PLAY_KEYTONE
		key_tone_play();
	#endif
		goto exit;
	}

	if (tts_ctx->tts_item_num >= 3) {
		sys_snode_t *head = sys_slist_peek_head(&tts_ctx->tts_list);
		item = CONTAINER_OF(head, struct tts_item_t, node);
		sys_slist_find_and_remove(&tts_ctx->tts_list, head);
		tts_ctx->tts_item_num--;
		SYS_LOG_WRN("drop: tts  %s\n", &item->tts_file_name[0]);
		mem_free(item);
	}

#ifdef CONFIG_TWS_UI_EVENT_SYNC
	item = _tts_create_item(tts_name, mode, 0, NULL);
#else
	item = _tts_create_item(tts_name, mode);
#endif
	if (!item) {
		res = -ENOMEM;
		goto exit;
	}

	sys_slist_append(&tts_ctx->tts_list, (sys_snode_t *)item);
	tts_ctx->tts_item_num++;

	_tts_manager_triggle_play_tts(tts_ctx);
exit:
	os_mutex_unlock(&tts_ctx->tts_mutex);
	return res;
}

static bool block_tts_finised = false;
static void _tts_manager_play_block_event_notify(uint32_t event, void *data, uint32_t len, void *user_data)
{
	switch (event) {
	case PLAYBACK_EVENT_STOP_COMPLETE:
		block_tts_finised = true;
		break;
	}
}

int tts_manager_play_block(uint8_t *tts_name)
{
	struct tts_manager_ctx_t *tts_ctx = _tts_get_ctx();
	int ret = 0;
	media_init_param_t init_param;
	io_stream_t stream = NULL;
	struct buffer_t buffer;
	void *player_handle = NULL;
	int try_cnt = 0;
	os_mutex_lock(&tts_ctx->tts_mutex, OS_FOREVER);
	if (sd_fmap(tts_name, (void **)&buffer.base, &buffer.length) != 0) {
		goto exit;
	}

	buffer.cache_size = 0;
	stream = buffer_stream_create(&buffer);
	if (!stream) {
		SYS_LOG_ERR("create failed\n");
		goto exit;
	}

	if (stream_open(stream, MODE_IN) != 0) {
		SYS_LOG_ERR("open failed\n");
		stream_destroy(stream);
		stream = NULL;
		goto exit;
	}

	memset(&init_param, 0, sizeof(media_init_param_t));

	init_param.type = MEDIA_SRV_TYPE_PLAYBACK;
	init_param.stream_type = AUDIO_STREAM_TTS;
	init_param.efx_stream_type = AUDIO_STREAM_TTS;
	init_param.format = MP3_TYPE;
	init_param.sample_rate = 16;
	init_param.input_stream = stream;
	init_param.output_stream = NULL;
	init_param.event_notify_handle = _tts_manager_play_block_event_notify;

	player_handle = media_player_open(&init_param);
	if (!player_handle) {
		goto exit;
	}

	block_tts_finised = false;

	media_player_play(player_handle);

	while (!block_tts_finised && try_cnt ++ < 100) {
		os_sleep(100);
	}

	media_player_stop(player_handle);
	media_player_close(player_handle);

exit:
	if (NULL != stream) {
		stream_close(stream);
		stream_destroy(stream);
	}
	os_mutex_unlock(&tts_ctx->tts_mutex);
	return ret;
}

int tts_manager_stop(uint8_t *tts_name)
{
	struct tts_manager_ctx_t *tts_ctx = _tts_get_ctx();
	struct tts_item_t *tts_item;

	os_mutex_lock(&tts_ctx->tts_mutex, OS_FOREVER);

	if (!tts_ctx->tts_curr)
		goto exit;

	tts_item = tts_ctx->tts_curr;

	if ((tts_item->tts_mode & UNBLOCK_UNINTERRUPTABLE)
		|| (tts_item->tts_mode & BLOCK_UNINTERRUPTABLE)) {
		goto exit;
	}

	_tts_remove_current_node(tts_ctx);
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	if(tts_ctx->dvfs_set) {
		tts_ctx->dvfs_set = 0;
		soc_dvfs_unset_level(SOC_DVFS_LEVEL_FULL_PERFORMANCE, "tts");
	}
#endif

exit:
	os_mutex_unlock(&tts_ctx->tts_mutex);

	_tts_manager_triggle_play_tts(tts_ctx);

	return 0;
}

int tts_manager_register_tts_config(const struct tts_config_t *config)
{
	struct tts_manager_ctx_t *tts_ctx = _tts_get_ctx();

	os_mutex_lock(&tts_ctx->tts_mutex, OS_FOREVER);

	tts_ctx->tts_config = config;

	os_mutex_unlock(&tts_ctx->tts_mutex);

	return 0;
}

int tts_manager_add_event_lisener(tts_event_nodify lisener)
{
	struct tts_manager_ctx_t *tts_ctx = _tts_get_ctx();

	os_mutex_lock(&tts_ctx->tts_mutex, OS_FOREVER);

	tts_ctx->lisener = lisener;

	os_mutex_unlock(&tts_ctx->tts_mutex);

	return 0;
}

int tts_manager_remove_event_lisener(tts_event_nodify lisener)
{
	struct tts_manager_ctx_t *tts_ctx = _tts_get_ctx();

	os_mutex_lock(&tts_ctx->tts_mutex, OS_FOREVER);

	tts_ctx->lisener = NULL;

	os_mutex_unlock(&tts_ctx->tts_mutex);

	return 0;
}

const struct tts_config_t tts_config = {
	.tts_storage_media = TTS_SOTRAGE_SDFS,
};

static int tts_manager_find_tts(int ui_event, uint32_t *mode, uint8_t**name)
{
	struct tts_manager_ctx_t *tts_ctx = _tts_get_ctx();

	if (!tts_ctx->tts_config || !tts_ctx->tts_config->tts_map){
		return -EINVAL;
	}

	for (int i = 0; i < tts_ctx->tts_config->tts_map_size; i++) {
		const tts_ui_event_map_t *tts_ui_event_map = &tts_ctx->tts_config->tts_map[i];

		if (tts_ui_event_map->ui_event == ui_event) {
			*name = (uint8_t *)tts_ui_event_map->tts_file_name;
			*mode = tts_ui_event_map->mode;
			return 0;
		}
	}

	return -EINVAL;
}

int tts_manager_process_ui_event(int ui_event)
{
	uint8_t *tts_file_name = NULL;
	uint32_t mode = 0;
	struct tts_manager_ctx_t *tts_ctx = _tts_get_ctx();

	SYS_LOG_INF("event:%d\n", ui_event);
	if( 0 == tts_manager_find_tts(ui_event, &mode, &tts_file_name)){
		if(tts_ctx->filter_enable && !(mode & TTS_CANNOT_BE_FILTERED)){
			SYS_LOG_INF("filter event:%d\n", ui_event);
			return -ENOENT;
		}
		tts_manager_play(tts_file_name, mode);
		return 0;
	} else {
		return -ENOENT;
	}
}

#ifdef CONFIG_TWS_UI_EVENT_SYNC
int tts_manager_play_at_time(uint8_t *tts_name, uint32_t mode, uint64_t bt_clk, char *pattern)
{
	int res = 0;
	struct tts_manager_ctx_t *tts_ctx = _tts_get_ctx();
	struct tts_item_t *item = NULL;

	os_mutex_lock(&tts_ctx->tts_mutex, OS_FOREVER);

	if (tts_ctx->tts_manager_locked > 0) {
		SYS_LOG_INF("lock %d\n", tts_ctx->tts_manager_locked);
		res = -ENOLCK;
		goto exit;
	}

	if (strstr(tts_name, ".pcm")) {
#ifdef CONFIG_PLAY_KEYTONE
		key_tone_play(tts_name);
#endif
		goto exit;
	}

	if (tts_ctx->tts_item_num >= 3) {
		sys_snode_t *head = sys_slist_peek_head(&tts_ctx->tts_list);
		item = CONTAINER_OF(head, struct tts_item_t, node);
		sys_slist_find_and_remove(&tts_ctx->tts_list, head);
		tts_ctx->tts_item_num--;
		SYS_LOG_ERR("drop: tts  %s\n", &item->tts_file_name[0]);
		mem_free(item);
	}

	item = _tts_create_item(tts_name, mode, bt_clk, pattern);
	if (!item) {
		res = -ENOMEM;
		goto exit;
	}

	sys_slist_append(&tts_ctx->tts_list, (sys_snode_t *)item);
	tts_ctx->tts_item_num++;

exit:
	os_mutex_unlock(&tts_ctx->tts_mutex);

	if ((res == 0) && !tts_ctx->tts_curr) {
		if (mode & PLAY_IMMEDIATELY) {
			res = tts_manager_play_process();
		} else {
			_tts_manager_triggle_play_tts(tts_ctx);
		}
	}
	return res;
}

int tts_manager_process_ui_event_at_time(int ui_event, uint64_t bt_clk)
{
	uint8_t *tts_file_name = NULL;
	int mode = 0;

	SYS_LOG_INF("event %d, clk %u_%u\n", ui_event, (uint32_t)(bt_clk>>32), (uint32_t)bt_clk);
	if (0 == tts_manager_find_tts(ui_event, &mode, &tts_file_name)) {
		mode = PLAY_IMMEDIATELY;
		return tts_manager_play_at_time(tts_file_name, mode, bt_clk, NULL);
	} else {
		return -ENOENT;
	}
}
#endif

static struct tts_manager_ctx_t global_tts_manager_ctx;

int tts_manager_init(void)
{
	tts_manager_ctx = &global_tts_manager_ctx;

	memset(tts_manager_ctx, 0, sizeof(struct tts_manager_ctx_t));

	sys_slist_init(&tts_manager_ctx->tts_list);

	os_mutex_init(&tts_manager_ctx->tts_mutex);

#ifdef CONFIG_TTS_USE_THREADTIMER
	thread_timer_init(&tts_manager_ctx->play_timer, _tts_manager_play_timer_handle, (void *)&tts_manager_ctx);

	thread_timer_init(&tts_manager_ctx->stop_timer, _tts_manager_stop_timer_handle, (void *)&tts_manager_ctx);
#else
	os_delayed_work_init(&tts_manager_ctx->stop_work, _tts_manager_stop_work);
#endif

	tts_manager_register_tts_config(&tts_config);

#ifdef CONFIG_PLAY_KEYTONE
	key_tone_manager_init();
#endif

#ifdef CONFIG_PLAY_MERGE_TTS
	tts_merge_manager_init();
	tts_merge_manager_mix_gain_set(TTS_MERGE_MIX_GAIN_LEVEL);
#endif

	return 0;
}

int tts_manager_deinit(void)
{
	tts_manager_wait_finished(true);

#ifndef CONFIG_TTS_USE_THREADTIMER
	os_delayed_work_cancel(&tts_manager_ctx->stop_work);
#endif

#ifdef CONFIG_PLAY_MERGE_TTS
	tts_merge_manager_deinit();
#endif

	return 0;
}


