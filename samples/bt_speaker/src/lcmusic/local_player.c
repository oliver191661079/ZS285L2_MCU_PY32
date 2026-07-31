/*
 * Copyright (c) 2024 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief local player
 */

#include "local_player.h"
#include "tts_manager.h"
#include "lcmusic.h"

#define ENABLE_LCPLAYER_FADEIN_FADEOUT 1
#define toupper(a)  \
	(((a) >= 'a' && (a) <= 'z') ? (a)-('a'-'A'):(a))

int parse_match_extension_type(const char *url, u8_t * match_str)
{
	int i;
	u32_t str_len, cmp_len;
	u8_t *src_str = (u8_t *) url;

	if (*src_str == '\0') {
		return false;
	}

	str_len = strlen(url);
	cmp_len = strlen(match_str);

	if (str_len < cmp_len) {
		return false;
	}

	for (i = 0; i < strlen(match_str); i++) {
		if (toupper(match_str[i]) !=
		    toupper(src_str[str_len - cmp_len + i])) {
			break;
		}
	}

	if (i == strlen(match_str)) {
		return true;
	}

	return false;
}

const char* get_music_type_str(u8_t type)
{
	switch(type) {
	case M4A_TYPE:
		return ".m4a";
	case WAV_TYPE:
		return ".wav";
	case MP3_TYPE:
		return ".mp3";
	case APE_TYPE:
		return ".ape";
	case WMA_TYPE:
		return ".wma";
	case FLA_TYPE:
		return ".fla";
	default:
		return ".U";
	}
}

u8_t get_music_file_type(const char *url)
{
#ifdef CONFIG_DECODER_AAC
	if (parse_match_extension_type(url, ".aac") ||
		parse_match_extension_type(url, ".m4a"))
		return M4A_TYPE;

#endif

#ifdef CONFIG_DECODER_WAV
	if (parse_match_extension_type(url, ".wav"))
		return WAV_TYPE;
#endif

#ifdef CONFIG_DECODER_MP3
	if (parse_match_extension_type(url, ".mp3") ||
	    parse_match_extension_type(url, ".mp2") ||
	    parse_match_extension_type(url, ".mp1"))
		return MP3_TYPE;
#endif

#ifdef CONFIG_DECODER_APE
	if (parse_match_extension_type(url, ".ape"))
		return APE_TYPE;
#endif

#ifdef CONFIG_DECODER_WMA
	if (parse_match_extension_type(url, ".wma"))
		return WMA_TYPE;
#endif

#ifdef CONFIG_DECODER_FLAC
	if (parse_match_extension_type(url, ".flac")
	    || parse_match_extension_type(url, ".fla"))
		return FLA_TYPE;
#endif

	return UNSUPPORT_TYPE;
}

static void _mplayer_cleanup(struct local_player_t *lcplayer)
{
	struct lcmusic_app_t *lcmusic = lcmusic_get_app();

	if (!lcplayer)
		return;

	if (lcplayer->player) {
		if (lcmusic->playing) {
			media_player_stop(lcplayer->player);
			lcmusic->playing = 0;
		}
		media_player_close(lcplayer->player);
		lcplayer->player = NULL;
	}

	if (lcplayer->file_stream) {
		stream_close(lcplayer->file_stream);
		stream_destroy(lcplayer->file_stream);
		lcplayer->file_stream = NULL;
	}

	if (lcplayer) {
		app_mem_free(lcplayer);
		lcplayer = NULL;
	}
}

static void set_player_effect_output_mode(media_player_t * player)
{
	int mode = CONFIG_MEDIA_EFFECT_OUTMODE;

	if (app_tws_status_get_connected() && app_tws_status_get_enable()) {
		mode = MEDIA_EFFECT_OUTPUT_L_ONLY;
	}
#ifdef CONFIG_BMS_LCMUSIC_APP
	if (lcmusic_is_bms_mode()) {
#ifdef CONFIG_BT_SELF_APP
		u8_t ch;
		ch = selfapp_get_channel();
		if (ch == 0) {
			mode = MEDIA_EFFECT_OUTPUT_DEFAULT;
		} else if (ch == 1) {
			mode = MEDIA_EFFECT_OUTPUT_L_ONLY;
		} else {
			mode = MEDIA_EFFECT_OUTPUT_R_ONLY;
		}
#endif
	}
#endif

	SYS_LOG_INF("%d\n", mode);
	if (mode != CONFIG_MEDIA_EFFECT_OUTMODE) {
		media_player_set_effect_output_mode(player, mode);
	}
}

#ifdef CONFIG_SOUNDBAR_SAMPLE_RATE_FILTER
static bool soundbar_filter_sample_rate(struct local_player_t *lcplayer,
					int min_sample_rate)
{
	media_info_t temp_media_info;

	if (!lcplayer || !lcplayer->player)
		return false;

	/* update sample rate from decoder, 0--successful */
	if (media_player_get_mediainfo(lcplayer->player, &temp_media_info) != 0)
		return false;

	/* Hz to kHz */
	if (min_sample_rate > 1000)
		min_sample_rate /= 1000;

	if (temp_media_info.sample_rate > 1000)
		temp_media_info.sample_rate /= 1000;

	if (temp_media_info.sample_rate < min_sample_rate) {
		SYS_LOG_INF("not supported sample rate(%d)\n",
			    temp_media_info.sample_rate);
		return true;
	}

	return false;
}
#endif

int mplayer_update_breakpoint(struct local_player_t *lcplayer,
			      media_breakpoint_info_t * bp_info)
{

	if (!lcplayer || !lcplayer->player) {
		SYS_LOG_ERR("player is null\n");
		return -EFAULT;
	}
	if (media_player_get_breakpoint(lcplayer->player, bp_info)) {
		SYS_LOG_ERR("bp failed\n");
		return -EINVAL;
	}

	if (bp_info->time_offset < 0) {
		SYS_LOG_ERR("bp invalid\n");
	}

	SYS_LOG_DBG("bp time_offset=%d, file_offset=%d\n",
		    bp_info->time_offset, bp_info->file_offset);
	return 0;
}

int mplayer_update_media_info(struct local_player_t *lcplayer,
			      media_info_t * info)
{
	if (!lcplayer || !lcplayer->player) {
		SYS_LOG_ERR("player is null\n");
		return -EFAULT;
	}
	if (media_player_get_mediainfo(lcplayer->player, info)) {
		SYS_LOG_ERR("get media info failed\n");
		return -EINVAL;
	}

	if (info->total_time < 0) {
		SYS_LOG_ERR("media info invalid\n");
	}

	SYS_LOG_INF("total_time=%d, sample_rate=%d,\
				channels=%d, avg_bitrate=%d\n", info->total_time, info->sample_rate, info->channels, info->avg_bitrate);
	return 0;
}

struct local_player_t *mplayer_start_play(struct lcplay_param *lcparam)
{
	struct lcmusic_app_t *lcmusic = lcmusic_get_app();

	media_init_param_t init_param;

#ifdef CONFIG_LC_MUSIC_FREQPOINT_ENERGY_DEMO
	effect_f_energy_set_t freqpoint_energy_info;
#endif

	if (!lcparam->url || lcparam->url[0] == 0)
		return NULL;

	SYS_LOG_INF("url:%s\n", lcparam->url);

	if (lcmusic->playback_player_run) {
		SYS_LOG_INF("already run\n");
		return NULL;
	}

	struct local_player_t *lcplayer =
	    app_mem_malloc(sizeof(struct local_player_t));
	if (!lcplayer) {
		SYS_LOG_ERR("malloc faild\n");
		return NULL;
	}
#ifdef CONFIG_PLAYTTS
	tts_manager_wait_finished(false);
#endif

// audio_system_set_output_sample_rate(48);

#ifdef CONFIG_LOOP_FSTREAM
	if (lcparam->play_mode == 1)
		lcplayer->file_stream =
		    loop_fstream_create((void *)lcparam->url);
	else
#endif
		lcplayer->file_stream =
		    file_stream_create((void *)lcparam->url);
	if (!lcplayer->file_stream) {
		SYS_LOG_ERR("stream create failed (%s)\n", lcparam->url);
		goto err_exit;
	}

	if (stream_open(lcplayer->file_stream, MODE_IN)) {
		SYS_LOG_ERR("stream open failed (%s)\n", lcparam->url);
		stream_destroy(lcplayer->file_stream);
		lcplayer->file_stream = NULL;
		goto err_exit;
	}

	memset(&init_param, 0, sizeof(media_init_param_t));

	init_param.type = MEDIA_SRV_TYPE_PARSER_AND_PLAYBACK;
	init_param.stream_type = AUDIO_STREAM_LOCAL_MUSIC;
	init_param.efx_stream_type = AUDIO_STREAM_LOCAL_MUSIC;
	init_param.format = get_music_file_type(lcparam->url);
	init_param.input_stream = lcplayer->file_stream;
	init_param.output_stream = NULL;
	init_param.event_notify_handle = lcparam->play_event_callback;
	init_param.support_tws = 1;
	init_param.dumpable = 1;
	init_param.sample_bits = 32;

	init_param.bp_file_offset = lcparam->bp.file_offset;
	init_param.bp_time_offset = lcparam->bp.time_offset;

	//in mp3 parser, whence is AP_SEEK_SET, seek time must larger than bp_time_offset
	if (init_param.format == MP3_TYPE && lcparam->seek_time != 0) {
		init_param.bp_file_offset = -1;
		init_param.bp_time_offset = lcparam->seek_time;
	}

#if CONFIG_BMS_LCMUSIC_APP
	if (lcmusic_is_bms_mode() && lcmusic->bms_source) {
		init_param.waitto_start = 1;
		init_param.bind_to_capture = 1;
		init_param.user_data = (void *)lcmusic;
#if (BROADCAST_DURATION == BT_FRAME_DURATION_7_5MS)
		audio_policy_set_nav_frame_size_us(7500);
#else
		audio_policy_set_nav_frame_size_us(10000);
#endif
		audio_policy_set_bis_link_delay_ms(broadcast_get_bis_link_delay
						   (lcmusic->qos));
	}
#endif

	/* open service */
	lcplayer->player = media_player_open(&init_param);
	if (!lcplayer->player) {
		SYS_LOG_ERR("open failed\n");
		goto err_exit;
	}

	set_player_effect_output_mode(lcplayer->player);

#if CONFIG_BMS_LCMUSIC_APP
	SYS_LOG_INF("bms %d bms_source:%d", lcmusic_is_bms_mode(),
		    lcmusic->bms_source);

	if (lcmusic_is_bms_mode() && 1 == lcmusic->bms_source) {
		bt_manager_broadcast_stream_tws_sync_cb_register_1(lcmusic->
								   chan,
								   media_player_audio_track_trigger_callback,
								   audio_system_get_track
								   ());

	}
#endif

#ifdef CONFIG_SOUNDBAR_SAMPLE_RATE_FILTER
	/* songs whose sample rate is lower than threshold won't play */
	if (lcparam->min_sample_rate_khz > 0) {
		if (soundbar_filter_sample_rate
		    (lcplayer, lcparam->min_sample_rate_khz))
			goto err_exit;
	}
#endif

	/**seek when necessary**/
	if (init_param.format != MP3_TYPE && lcparam->seek_time != 0) {
		mplayer_seek(lcplayer, lcparam->seek_time, &lcparam->bp);
	}
#if ENABLE_LCPLAYER_FADEIN_FADEOUT
	media_player_fade_in(lcplayer->player, 100);
#endif

#ifdef CONFIG_LC_MUSIC_FREQPOINT_ENERGY_DEMO
	memset(&freqpoint_energy_info, 0x0, sizeof(freqpoint_energy_info));
	freqpoint_energy_info.t_energy_en = 1;
	freqpoint_energy_info.f_band_energy_en = 1,
	freqpoint_energy_info.f_band_energy_timewindow_ms = 10;
	freqpoint_energy_info.f_band_bandwidth = 29;
	freqpoint_energy_info.f_band_energy_num = 10;
	freqpoint_energy_info.f_band_energy_f0 = 100;
	freqpoint_energy_info.f_band_energy_f1 = 500;
	freqpoint_energy_info.f_band_energy_f2 = 1000;
	freqpoint_energy_info.f_band_energy_f3 = 1500;
	freqpoint_energy_info.f_band_energy_f4 = 2000;
	freqpoint_energy_info.f_band_energy_f5 = 3000;
	freqpoint_energy_info.f_band_energy_f6 = 5000;
	freqpoint_energy_info.f_band_energy_f7 = 8000;
	freqpoint_energy_info.f_band_energy_f8 = 10000;
	freqpoint_energy_info.f_band_energy_f9 = 15000;

	media_player_set_output_energy_sample(lcplayer->player, &freqpoint_energy_info);
#endif

//For bms mode, play later after capture is started.
#if CONFIG_BMS_LCMUSIC_APP
	if (!lcmusic_is_bms_mode()) {
		media_player_play(lcplayer->player);
		lcmusic->playing = 1;
	}

	if (lcmusic_is_bms_mode() && 0 == lcmusic->bms_source) {
		media_player_play(lcplayer->player);
		lcmusic->playing = 1;
	}
#else
	/* start play */
	media_player_play(lcplayer->player);
	lcmusic->playing = 1;
#endif

	lcmusic->playback_player_run = 1;

	SYS_LOG_INF("%p ok\n", lcplayer->player);

	return lcplayer;

 err_exit:
#if CONFIG_BMS_LCMUSIC_APP
	if (lcmusic_is_bms_mode()) {
		audio_policy_set_bis_link_delay_ms(0);
	}
#endif
	_mplayer_cleanup(lcplayer);
	return NULL;
}

void mplayer_stop_play(struct local_player_t *lcplayer)
{
	struct lcmusic_app_t *lcmusic = lcmusic_get_app();

	if (!lcplayer)
		return;

	if (!lcmusic->playback_player_run) {
		SYS_LOG_INF("not ready\n");
		return;
	}
#if CONFIG_BMS_LCMUSIC_APP
	if (lcmusic_is_bms_mode()) {
		bt_manager_broadcast_stream_tws_sync_cb_register_1(lcmusic->
								   chan, NULL,
								   NULL);
	}
#endif

#if ENABLE_LCPLAYER_FADEIN_FADEOUT
	media_player_fade_out(lcplayer->player, 60);
	/** reserve time to fade out*/
	os_sleep(audio_policy_get_bis_link_delay_ms() + 80);
#endif

	_mplayer_cleanup(lcplayer);

#if CONFIG_BMS_LCMUSIC_APP
	if (lcmusic_is_bms_mode()) {
		audio_policy_set_bis_link_delay_ms(0);
	}
#endif
	lcmusic->playback_player_run = 0;

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		if (lcmusic->set_dvfs_level) {
			soc_dvfs_unset_level(lcmusic->set_dvfs_level, "lcmusic");
			lcmusic->set_dvfs_level = 0;
		}
#endif
	SYS_LOG_INF("stopped\n");
}

int mplayer_seek(struct local_player_t *lcplayer, int seek_time,
		 media_breakpoint_info_t * bp_info)
{
	media_seek_info_t info = {.file_offset = -1, };

	if (!lcplayer || !lcplayer->player) {
		SYS_LOG_ERR("player is null\n");
		return -EFAULT;
	}
	info.whence = AP_SEEK_SET;
	info.time_offset = seek_time;

	if (media_player_seek(lcplayer->player, &info)) {
		SYS_LOG_ERR("(%d) failed\n", seek_time);
		return -EINVAL;
	}
	SYS_LOG_INF("seek_time=%d,chunk_time_offset=%d, chunk_file_offset=%d\n",
		    seek_time, info.chunk_time_offset, info.chunk_file_offset);

	/* update breakpoint from seeking feedback */
	if (info.chunk_time_offset >= 0) {
		bp_info->time_offset = info.chunk_time_offset;
		bp_info->file_offset = info.chunk_file_offset;
	} else {
		bp_info->time_offset += seek_time;
		bp_info->file_offset = -1;
		if (bp_info->time_offset < 0)
			bp_info->time_offset = 0;
	}
	return 0;
}

int mplayer_pause(struct local_player_t *lcplayer)
{
	if (!lcplayer || !lcplayer->player) {
		SYS_LOG_ERR("player is null\n");
		return -EFAULT;
	}
	return media_player_pause(lcplayer->player);
}

int mplayer_resume(struct local_player_t *lcplayer)
{
	if (!lcplayer || !lcplayer->player) {
		SYS_LOG_ERR("player is null\n");
		return -EFAULT;
	}
	return media_player_resume(lcplayer->player);
}
