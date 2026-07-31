/*
 * Copyright (c) 2016 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief audio system.
*/
#include <os_common_api.h>
#include <mem_manager.h>
#include <msg_manager.h>
#include <audio_system.h>
#include <audio_record.h>
#include <audio_track.h>
#include <audio_device.h>
#include <audio_hal.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <property_manager.h>

#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "audio system"
#include <logging/sys_log.h>

static struct audio_system_t *audio_system = NULL;

u8_t audio_system_get_multi_channel_mode(void)
{
#ifdef CONFIG_AUDIO_MULTI_CHANNEL_MODE
	return CONFIG_AUDIO_MULTI_CHANNEL_MODE;
#else
	return MULTI_CH_MODE_2_0;
#endif
}

int audio_system_get_i2stx_mode(void)
{
#ifdef CONFIG_I2STX_MODE
    return CONFIG_I2STX_MODE;
#else
    return 0;
#endif
}

int audio_system_set_output_sample_rate(int value)
{
	if (!audio_system)
		return -ESRCH;

	audio_system->output_sample_rate = value;

	return 0;
}

int audio_system_get_output_sample_rate(void)
{
	if (!audio_system)
		return -ESRCH;

	if (!audio_system->output_sample_rate) {
		struct audio_track_t *track = audio_system_get_track();
		if (track)
			return track->output_sample_rate;
	}

	return audio_system->output_sample_rate;
}

int audio_system_get_output_framecount(void)
{
	if (!audio_system)
		return -ESRCH;

	return 0;
}

int audio_system_set_master_volume(int value)
{
	if (!audio_system)
		return -ESRCH;

	audio_system->master_volume = value;

	return audio_system_set_stream_volume(AUDIO_STREAM_DEFAULT, value);

}

int audio_system_get_master_volume(void)
{
	if (!audio_system)
		return -ESRCH;

	return audio_system->master_volume;
}

int audio_system_set_master_mute(int value)
{
	if (!audio_system)
		return -ESRCH;

	audio_system->master_muted = value;
	/**TODO set master mute */

	return 0;
}

int audio_system_get_master_mute(void)
{
	if (!audio_system)
		return -ESRCH;

	return audio_system->master_muted;
}

int audio_system_set_stream_mute(int stream_type, int mute)
{
	struct audio_track_t *audio_track = NULL;
	int ret = -ESRCH;

	if (!audio_system)
		return -ESRCH;

	for (int i = 0; i < MAX_AUDIO_TRACK_NUM; i++) {
		audio_track = audio_system->audio_track_pool[i];
		if (audio_track && audio_track->stream_type == stream_type) {
			ret = audio_track_set_mute(audio_track, mute);
		}
	}

	return ret;
}

int audio_system_get_stream_mute(int stream_type)
{
	struct audio_track_t *audio_track = NULL;
	int ret = -ESRCH;
	if (!audio_system)
		return -ESRCH;

	for (int i = 0; i < MAX_AUDIO_TRACK_NUM; i++) {
		audio_track = audio_system->audio_track_pool[i];
		if (audio_track && audio_track->stream_type == stream_type) {
			ret = audio_track_get_volume(audio_track);
			break;
		}
	}

	return (ret == 0) ? 1 : 0;
}

int audio_system_mute_microphone(int value)
{
	struct audio_record_t *audio_record = NULL;

	if (!audio_system)
		return -ESRCH;

	audio_system->microphone_muted = value;

	for (int i = 0; i < MAX_AUDIO_RECORD_NUM; i++) {
		audio_record = audio_system->audio_record_pool[i];
		if (audio_record) {
			audio_record->muted = audio_system->microphone_muted;
		}
	}

	return 0;
}

int audio_system_trigger_track_start(int stream_type)
{
	struct audio_track_t *audio_track = NULL;
	int ret = -ESRCH;
	if (!audio_system)
		return -ESRCH;

	for (int i = 0; i < MAX_AUDIO_TRACK_NUM; i++) {
		audio_track = audio_system->audio_track_pool[i];
		if (audio_track && audio_track->stream_type == stream_type) {
			ret = audio_track_start(audio_track);
			break;
		}
	}

	return 0;
}

int audio_system_trigger_record_start(int stream_type)
{
	struct audio_record_t *audio_record = NULL;
	int ret = -ESRCH;
	if (!audio_system)
		return -ESRCH;

	for (int i = 0; i < MAX_AUDIO_TRACK_NUM; i++) {
		audio_record = audio_system->audio_record_pool[i];
		if (audio_record && audio_record->stream_type == stream_type) {
			ret = audio_record_start(audio_record);
			break;
		}
	}

	return 0;
}

int audio_system_get_microphone_muted(void)
{
	if (!audio_system)
		return -ESRCH;

	return audio_system->microphone_muted ? 1 : 0;
}

int audio_system_register_track(struct audio_track_t *audio_track)
{
	if (!audio_system)
		return -ESRCH;

	for (int i = 0; i < MAX_AUDIO_TRACK_NUM; i++) {
		if (!audio_system->audio_track_pool[i]) {
			audio_system->audio_track_pool[i] = audio_track;
			audio_system->audio_track_num++;
			SYS_LOG_INF(" %p index %d\n", audio_track, i);
			break;
		}
	}

	return 0;
}

int audio_system_mutex_lock(void)
{
	if (!audio_system)
		return -ESRCH;

	return os_mutex_lock(&audio_system->audio_system_mutex, OS_FOREVER);
}

int audio_system_mutex_unlock(void)
{
	if (!audio_system)
		return -ESRCH;

	os_mutex_unlock(&audio_system->audio_system_mutex);
	return 0;
}

struct audio_track_t *audio_system_get_track(void)
{
	if (!audio_system)
		return NULL;

	for (int i = 0; i < MAX_AUDIO_TRACK_NUM; i++) {
		if (audio_system->audio_track_pool[i]) {
			return audio_system->audio_track_pool[i];
		}
	}

	return NULL;
}

struct audio_record_t *audio_system_get_record(void)
{
	if (!audio_system)
		return NULL;

	for (int i = 0; i < MAX_AUDIO_RECORD_NUM; i++) {
		if (audio_system->audio_record_pool[i]) {
			return audio_system->audio_record_pool[i];
		}
	}

	return NULL;
}


struct audio_track_t * audio_system_get_audio_track_handle(int stream_type)
{
	struct audio_track_t *audio_track = NULL;
	if (!audio_system)
		return NULL;

	for (int i = 0; i < MAX_AUDIO_TRACK_NUM; i++) {
		audio_track = audio_system->audio_track_pool[i];
		if (audio_track && audio_track->stream_type == stream_type) {
			return audio_track;
		}
    }

	return NULL;
}

struct audio_record_t * audio_system_get_audio_record_handle(int stream_type)
{
	struct audio_record_t *audio_record = NULL;

	if (!audio_system)
		return NULL;

	for (int i = 0; i < MAX_AUDIO_RECORD_NUM; i++) {
		audio_record = audio_system->audio_record_pool[i];
		if (audio_record->stream_type == stream_type) {
			return audio_record;
		}
    }

	return NULL;
}

int audio_system_unregister_track(struct audio_track_t *audio_track)
{
	int ret = -ENXIO;

	if (!audio_system)
		return -ESRCH;

	for (int i = 0; i < MAX_AUDIO_TRACK_NUM; i++) {
		if (audio_system->audio_track_pool[i] == audio_track) {
			audio_system->audio_track_pool[i] = NULL;
			audio_system->audio_track_num--;
			SYS_LOG_INF(" %p index %d\n", audio_track, i);
			ret = 0;
			break;
		}
	}

	for (int i = 0; i < MAX_AUDIO_TRACK_NUM; i++) {
		struct audio_track_t *tmp_track = NULL;

		tmp_track = audio_system->audio_track_pool[i];
		if (tmp_track)
			audio_system_set_stream_volume(tmp_track->stream_type, tmp_track->volume);
	}

	return 0;
}

int audio_system_register_record(struct audio_record_t *audio_record)
{
	int ret = -EAGAIN;

	if (!audio_system)
		return -ESRCH;

	for (int i = 0; i < MAX_AUDIO_RECORD_NUM; i++) {
		if (!audio_system->audio_record_pool[i]) {
			audio_system->audio_record_pool[i] = audio_record;
			audio_system->audio_record_num++;
			SYS_LOG_INF(" %p index %d\n", audio_record, i);
			ret = 0;
			break;
		}
	}

	return ret;
}

int audio_system_unregister_record(struct audio_record_t *audio_record)
{
	int ret = -ENXIO;

	if (!audio_system)
		return -ESRCH;

	for (int i = 0; i < MAX_AUDIO_RECORD_NUM; i++) {
		if (audio_system->audio_record_pool[i] == audio_record) {
			audio_system->audio_record_pool[i] = NULL;
			SYS_LOG_INF(" %p index %d\n", audio_record, i);
			audio_system->audio_record_num--;
			ret = 0;
			break;
		}
	}
	return ret;
}

static void _audio_system_get_volume_config(const char *name, uint8_t *volume)
{
#ifdef CONFIG_PROPERTY
	int value;

	value = property_get_int(name, DEFAULT_VOLUME);

	SYS_LOG_INF(" %s  %d\n", name, *volume);

	if(value <= 0 || value > audio_policy_get_volume_level()) {
		value = DEFAULT_VOLUME;
	}

	*volume = (uint8_t)value;
#endif

}

static int _audio_system_save_volume(const char *name, uint8_t volume)
{
	char temp[5];

	if (!audio_policy_check_save_volume_to_nvram()) {
		return 0;
	}

	snprintf(temp, sizeof(temp), "%d", volume);

#ifdef CONFIG_PROPERTY
	return property_set(name, temp, strlen(temp) + 1);
#else
	return 0;
#endif
}

int audio_system_set_stream_volume(int stream_type, int volume)
{
	struct audio_track_t *audio_track = NULL;
	int ret = 0;

	if (!audio_system)
		return -ESRCH;

	SYS_LOG_INF("stream=%d, vol=%d/%d", stream_type, volume, audio_policy_get_volume_level());

	if (volume >= audio_policy_get_volume_level()) {
		volume = audio_policy_get_volume_level();
		ret = MAX_VOLUME_VALUE;
	}

	/**usound default used 16 level */
	if (stream_type == AUDIO_STREAM_USOUND) {
		if (volume >= 16) {
			volume = 16;
			ret = MAX_VOLUME_VALUE;
		}
	}

	if (volume <= 0) {
		ret = MIN_VOLUME_VALUE;
		volume = 0;
	}
	os_mutex_lock(&audio_system->audio_system_mutex, OS_FOREVER);
	for (int i = 0; i < MAX_AUDIO_TRACK_NUM; i++) {
		audio_track = audio_system->audio_track_pool[i];
		if (audio_track &&
					 (audio_track->stream_type == stream_type
						|| stream_type == AUDIO_STREAM_DEFAULT)) {
			audio_track_set_volume(audio_track, volume);
		}
	}

	if (stream_type == AUDIO_STREAM_VOICE) {
	    audio_system->voice_volume = volume;
	    _audio_system_save_volume(CFG_BTCALL_VOLUME, volume);
	}

	if (stream_type == AUDIO_STREAM_LE_AUDIO) {
	    audio_system->lea_volume = volume;
	    _audio_system_save_volume(CFG_LEA_VOLUME, volume);
	}

	if (stream_type == AUDIO_STREAM_BMR) {
		audio_system->bmr_volume = volume;
		_audio_system_save_volume(CFG_BMR_VOLUME, volume);
	}

	if (stream_type == AUDIO_STREAM_MUSIC || stream_type == AUDIO_STREAM_DEFAULT  || stream_type == AUDIO_STREAM_SOUNDBAR) {
	    audio_system->music_volume = volume;
	    _audio_system_save_volume(CFG_BTPLAY_VOLUME, volume);
	}

	if (stream_type == AUDIO_STREAM_LINEIN || stream_type == AUDIO_STREAM_DEFAULT) {
	    audio_system->linein_volume = volume;
	    _audio_system_save_volume(CFG_LINEIN_VOLUME, volume);
	}

	if (stream_type == AUDIO_STREAM_USOUND || stream_type == AUDIO_STREAM_DEFAULT) {
	    audio_system->usound_volume = volume;
	    _audio_system_save_volume(CFG_USOUND_VOLUME, volume);
	}

	if (stream_type == AUDIO_STREAM_LOCAL_MUSIC || stream_type == AUDIO_STREAM_DEFAULT) {
	    audio_system->lcmusic_volume = volume;
	    _audio_system_save_volume(CFG_LOCALPLAY_VOLUME, volume);
	}

	if (stream_type == AUDIO_STREAM_FM || stream_type == AUDIO_STREAM_DEFAULT) {
	    audio_system->fm_volume = volume;
	    _audio_system_save_volume(CFG_FM_VOLUME, volume);
	}

	if (stream_type == AUDIO_STREAM_I2SRX_IN || stream_type == AUDIO_STREAM_DEFAULT) {
	    audio_system->i2srx_in_volume = volume;
	    _audio_system_save_volume(CFG_I2SRX_IN_VOLUME, volume);
	}

	if (stream_type == AUDIO_STREAM_MIC_IN || stream_type == AUDIO_STREAM_DEFAULT) {
	    audio_system->mic_in_volume = volume;
	    _audio_system_save_volume(CFG_MIC_IN_VOLUME, volume);
	}

	if (stream_type == AUDIO_STREAM_SPDIF_IN || stream_type == AUDIO_STREAM_DEFAULT) {
	    audio_system->spidf_in_volume = volume;
	    _audio_system_save_volume(CFG_SPDIF_IN_VOLUME, volume);
	}
	if (stream_type != AUDIO_STREAM_VOICE) {
	    audio_system->master_volume = volume;

	if (volume < audio_system->min_volume)
		    volume = audio_system->min_volume;
	    if(volume > audio_system->max_volume)
		    volume = audio_system->max_volume;

		if (!audio_policy_check_tts_fixed_volume()) {
			audio_system->tts_volume = volume;
			_audio_system_save_volume(CFG_TONE_VOLUME, volume);
		}
	}
	os_mutex_unlock(&audio_system->audio_system_mutex);

	SYS_LOG_INF("ret=%d", ret);

	return ret;
}

int audio_system_get_current_volume(int stream_type)
{
	int volume = 0;
	struct audio_track_t *audio_track = NULL;

	if (!audio_system)
		return -ESRCH;

	volume = audio_system_get_stream_volume(stream_type);

	/**make sure tts have volume*/
	for (int i = 0; i < MAX_AUDIO_TRACK_NUM; i++) {
		audio_track = audio_system->audio_track_pool[i];
		if (audio_track->stream_type == AUDIO_STREAM_TTS) {
			if (volume != audio_system->tts_volume) {
				volume = audio_system->tts_volume;
			}
		}
	}

	return volume;
}

int audio_system_get_current_pa_volume(int stream_type)
{
	int volume = 0;
	int pa_volume = 0;
	struct audio_track_t *audio_track = NULL;

	if (!audio_system)
		return -ESRCH;

	volume = audio_system_get_current_volume(stream_type);
	pa_volume = audio_policy_get_pa_volume(stream_type, volume);


	/**make sure tts have volume*/
	for (int i = 0; i < MAX_AUDIO_TRACK_NUM; i++) {
		audio_track = audio_system->audio_track_pool[i];
		if (audio_track->stream_type == AUDIO_STREAM_TTS) {
			pa_volume = audio_policy_get_pa_volume(AUDIO_STREAM_TTS, volume);
		}
	}

	return pa_volume;
}

int audio_system_get_stream_volume(int stream_type)
{
	int volume = 0;

	if (!audio_system)
		return -ESRCH;

	switch (stream_type) {
	case AUDIO_STREAM_MUSIC:
	case AUDIO_STREAM_SOUNDBAR:
		volume = audio_system->music_volume;
		break;
	case AUDIO_STREAM_LE_AUDIO:
		volume = audio_system->lea_volume;
		break;
	case AUDIO_STREAM_BMR:
		volume = audio_system->bmr_volume;
		break;
	case AUDIO_STREAM_TTS:
		volume = audio_system->tts_volume;
		break;
	case AUDIO_STREAM_VOICE:
		volume = audio_system->voice_volume;
		break;
	case AUDIO_STREAM_LINEIN:
	    volume = audio_system->linein_volume;
	break;
	case AUDIO_STREAM_USOUND:
	    volume = audio_system->usound_volume;
	break;
	case AUDIO_STREAM_LOCAL_MUSIC:
	    volume = audio_system->lcmusic_volume;
	break;
	case AUDIO_STREAM_FM:
	    volume = audio_system->fm_volume;
	break;
	case AUDIO_STREAM_I2SRX_IN:
		volume = audio_system->i2srx_in_volume;
		break;
	case AUDIO_STREAM_SPDIF_IN:
		volume = audio_system->spidf_in_volume;
		break;
	default:
		volume = audio_system->master_volume;
		break;
	}

	return volume;
}

int audio_system_set_stream_pa_volume(int stream_type, int volume)
{
	struct audio_track_t *audio_track = NULL;

	if (!audio_system)
		return -ESRCH;

	os_mutex_lock(&audio_system->audio_system_mutex, OS_FOREVER);
	for (int i = 0; i < MAX_AUDIO_TRACK_NUM; i++) {
		audio_track = audio_system->audio_track_pool[i];
		if (audio_track &&
					 (audio_track->stream_type == stream_type
						|| stream_type == AUDIO_STREAM_DEFAULT)) {
			audio_track_set_pa_volume(audio_track, volume);
		}
	}
	os_mutex_unlock(&audio_system->audio_system_mutex);

	return 0;
}

int audio_system_set_microphone_volume(int stream_type, int volume)
{
	struct audio_record_t *audio_record = NULL;
	int ret = 0;

	if (!audio_system)
		return -ESRCH;

	SYS_LOG_INF("stream=%d vol=%d", stream_type, volume);
	for (int i = 0; i < MAX_AUDIO_RECORD_NUM; i++) {
		audio_record = audio_system->audio_record_pool[i];
		if (audio_record) {
			audio_record_set_volume(audio_record, volume);
		}
	}

	return ret;
}

int audio_system_get_max_volume(void)
{
	return audio_system->max_volume;
}

int audio_system_get_min_volume(void)
{
	return audio_system->min_volume;
}

void audio_system_clear_volume(void){
	property_set(CFG_BTPLAY_VOLUME, NULL,0);

	property_set(CFG_TONE_VOLUME, NULL,0);

	property_set(CFG_BTCALL_VOLUME, NULL,0);

	property_set(CFG_LINEIN_VOLUME, NULL,0);

	property_set(CFG_USOUND_VOLUME, NULL,0);

	property_set(CFG_LEA_VOLUME, NULL,0);

	property_set(CFG_BMR_VOLUME, NULL,0);

	property_set(CFG_LOCALPLAY_VOLUME, NULL,0);

	property_set(CFG_FM_VOLUME, NULL,0);

	property_set(CFG_I2SRX_IN_VOLUME, NULL,0);

	property_set(CFG_SPDIF_IN_VOLUME, NULL,0);

	property_set(CFG_TTS_MIN_VOL, NULL,0);

	property_set(CFG_TTS_MAX_VOL, NULL,0);
}

static struct audio_system_t global_audio_system;

int aduio_system_init(void)
{
	audio_system = &global_audio_system;

	memset(audio_system, 0, sizeof(struct audio_system_t));

	os_mutex_init(&audio_system->audio_system_mutex);

	_audio_system_get_volume_config(CFG_BTPLAY_VOLUME, &audio_system->music_volume);

	audio_system->master_volume = audio_system->music_volume;

	_audio_system_get_volume_config(CFG_TONE_VOLUME, &audio_system->tts_volume);

	_audio_system_get_volume_config(CFG_BTCALL_VOLUME, &audio_system->voice_volume);

	_audio_system_get_volume_config(CFG_LINEIN_VOLUME, &audio_system->linein_volume);

	_audio_system_get_volume_config(CFG_USOUND_VOLUME, &audio_system->usound_volume);

	_audio_system_get_volume_config(CFG_LEA_VOLUME, &audio_system->lea_volume);

	_audio_system_get_volume_config(CFG_BMR_VOLUME, &audio_system->bmr_volume);

	_audio_system_get_volume_config(CFG_LOCALPLAY_VOLUME, &audio_system->lcmusic_volume);

	_audio_system_get_volume_config(CFG_FM_VOLUME, &audio_system->fm_volume);

	_audio_system_get_volume_config(CFG_I2SRX_IN_VOLUME, &audio_system->i2srx_in_volume);

	_audio_system_get_volume_config(CFG_SPDIF_IN_VOLUME, &audio_system->spidf_in_volume);

	_audio_system_get_volume_config(CFG_TTS_MIN_VOL, &audio_system->min_volume);

	_audio_system_get_volume_config(CFG_TTS_MAX_VOL, &audio_system->max_volume);

	/* audio_system_set_output_sample_rate(48); */

	/* init audio hal */
	hal_audio_out_init();

	hal_audio_in_init();

	return 0;
}

void audio_system_dump_record(struct audio_record_t *record)
{
	if (NULL == record) {
		printk("No record!\n");
	} else {

		printk("record %p dump: begin ...\n", record);

		printk("\tstream_type: %d\n", record->stream_type);
		printk("\taudio_format: %d\n", record->audio_format);
		printk("\tmedia_format: %d\n", record->media_format);
		printk("\taudio_mode: %d\n", record->audio_mode);

		printk("\tchannel: %d %d %d\n", record->channel_type, record->channel_mode, record->channel_id);

		printk("\tsample_rate: %d\n", record->sample_rate);
		printk("\toutput_sample_rate: %d\n", record->output_sample_rate);
		//printk("\tinput_samples: 0x%x\n", record->input_samples);
		printk("\tsample_rate: %d\n", record->sample_rate);
		printk("\tframe_size: %d\n", record->frame_size);

		printk("\tmuted: %d\n", record->muted);
		printk("\tadc_gain: %d\n", record->adc_gain);
		printk("\tinput_gain: %d\n", record->input_gain);
		printk("\tvolume: %d\n", record->volume);

		printk("\tpaused: %d\n", record->paused);
		printk("\tstarted: %d\n", record->started);
		printk("\twaitto_start: %d\n", record->waitto_start);

		printk("\ttimeline`: %p\n", record->timeline);

		printk("\nrecord dump: completed ...\n");
	}

}
void audio_system_dump_track(struct audio_track_t *track)
{
	if (NULL == track) {
		printk("No track!\n");
	} else {

		printk("track %p dump: begin ...\n", track);

		printk("\tstream_type: %d\n", track->stream_type);
		printk("\taudio_format: %d\n", track->audio_format);
		printk("\taudio_mode: %d\n", track->audio_mode);
		printk("\tframe_size: %d\n", track->frame_size);
		printk("\tsample_rate: %d\n", track->sample_rate);
		printk("\toutput_sample_rate: %d\n", track->output_sample_rate);

		printk("\tmuted: %d\n", track->muted);
		printk("\tstarted: %d\n", track->started);
		printk("\twaitto_start: %d\n", track->waitto_start);
		printk("\tfill_cnt: %d\n", track->fill_cnt);
		printk("\toutput_samples: %d\n", track->output_samples);
		printk("\tlast_samples_cnt: %d\n", track->last_samples_cnt);
		printk("\tsamples_overflow_cnt: %d\n",
		       (int)track->samples_overflow_cnt);
		printk("\tphy_dma: 0x%x\n", track->phy_dma);

		printk("\ntrack dump: completed ...\n");
	}
}

void audio_system_dump_data(void)
{
	if (NULL == audio_system) {
		printk("No audio system!\n");
	} else {
		printk("audio system dump: begin ...\n");

		//audio tracks
		printk("\ttracks: %d\n", audio_system->audio_track_num);
		for (u8_t i = 0; i < MAX_AUDIO_TRACK_NUM; i++) {
			audio_system_dump_track(audio_system->audio_track_pool[i]);
		}

		//audio records
		for (u8_t i = 0; i < MAX_AUDIO_RECORD_NUM; i++) {
			audio_system_dump_record(audio_system->audio_record_pool[i]);
		}

		/*
		bool microphone_muted;
		u8_t output_sample_rate;
		u8_t capture_output_sample_rate;
		bool master_muted;
		u8_t master_volume;
		*/

		printk("\ttts_volume: %d\n", audio_system->tts_volume);
		printk("\tmusic_volume: %d\n", audio_system->music_volume);
		printk("\tvoice_volume: %d\n", audio_system->voice_volume);
		printk("\tlinein_volume: %d\n", audio_system->linein_volume);
		printk("\ti2srx_in_volume: %d\n", audio_system->i2srx_in_volume);
		printk("\tmic_in_volume: %d\n", audio_system->mic_in_volume);
		printk("\tspidf_in_volume: %d\n", audio_system->spidf_in_volume);
		printk("\tusound_volume: %d\n", audio_system->usound_volume);
		printk("\tlcmusic_volume: %d\n", audio_system->lcmusic_volume);
		printk("\tlea_volume: %d\n", audio_system->lea_volume);
		printk("\tbmr_volume: %d\n", audio_system->bmr_volume);
		printk("\tmax_volume: %d\n", audio_system->max_volume);
		printk("\tmin_volume: %d\n", audio_system->min_volume);

		printk("audio system dump: completed ...\n\n");
	}

}

#ifndef CONFIG_TWS
int32_t audio_tws_set_stream_info(
    uint8_t format, uint16_t first_seq, uint8_t sample_rate, uint32_t pkt_time_us, uint64_t play_time)
{
	return 0;
}

int audio_tws_get_playback_first_seq(uint8_t *start_decode, uint8_t *start_play, uint16_t *first_seq, uint16_t *playtime_us)
{
	return 0;
}

int32_t audio_tws_set_pkt_info(uint16_t pkt_seq, uint16_t pkt_len, uint16_t frame_cnt, uint16_t pcm_len)
{
	return 0;
}

uint64_t audio_tws_get_play_time_us(void)
{
	return 0;
}

uint64_t audio_tws_get_bt_clk_us(void *tws_observer)
{
	return 0;
}
#endif
