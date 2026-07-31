/*
 * Copyright (c) 2016 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief audio track.
*/

#include <os_common_api.h>
#include <mem_manager.h>
#include <msg_manager.h>
#include <audio_track.h>
#include <audio_device.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <media_mem.h>
#include <assert.h>
#include <ringbuff_stream.h>
#include <arithmetic.h>

#include "bluetooth_tws_observer.h"

#ifdef CONFIG_BLUETOOTH
#include <bt_manager.h>
#endif

#ifdef CONFIG_LOGIC_ANALYZER
#include <logic.h>
#endif

#ifdef CONFIG_PLAY_MERGE_TTS
#include <tts_manager.h>
#endif

//#define CONFIG_AUDIO_DATA_DEBUG_SINE

#ifdef CONFIG_AUDIO_DATA_DEBUG_SINE
extern void pcm_data_cycle_load(unsigned char *buf, unsigned int len, unsigned char tag);
#endif

#define SYS_LOG_DOMAIN "track"
#include <logging/sys_log.h>

#ifdef CONFIG_ACT_EVENT
#include <logging/log_core.h>
LOG_MODULE_DECLARE(media_event,CONFIG_ACT_EVENT_APP_COMPILE_LEVEL);
#endif

#define FADE_IN_TIME_MS (0)
#define FADE_OUT_TIME_MS (0)

__in_section_unique(AUDIO_TRACK_RELOAD_RAM)  static uint8_t reload_pcm_buff[CONFIG_AUDIO_TRACK_MAX_RELOAD_BUFFER_SIZE] __aligned(4);

//static uint8_t asrc_pcm_buff[512 * 2] __aligned(4);
extern uint32_t get_sample_rate_hz(uint8_t fs_khz);
extern int stream_read_pcm(asin_pcm_t *aspcm, io_stream_t stream, int max_samples, int debug_space);
extern void *media_resample_open(uint8_t channels, uint8_t samplerate_in,
		uint8_t samplerate_out, int *samples_in, int *samples_out, uint8_t stream_type);
extern int media_resample_process(void *handle, uint8_t channels, void *output_buf[2],
		void *input_buf[2], int input_samples);
extern void media_resample_close(void *handle);

extern void *media_fade_open(uint8_t sample_rate, uint8_t channels, uint8_t is_interweaved);
extern void media_fade_close(void *handle);
extern int media_fade_process(void *handle, void *inout_buf[2], int samples);
extern int media_fade_in_set(void *handle, int fade_time_ms);
extern int media_fade_out_set(void *handle, int fade_time_ms);
extern int media_fade_out_is_finished(void *handle);
extern void* media_fadeinout_open(void);
extern void media_fadeinout_close(void *handle);
extern void media_fadeinout_set(void *handle, int fade_samples, int mode);
extern void media_fadeinout_process(void *handle, char *pcm, uint32_t samples_cnt, int channel, int width16);

extern void *media_mix_open(uint8_t sample_rate, uint8_t channels, uint8_t is_interweaved);
extern void media_mix_close(void *handle);
extern int media_mix_process(void *handle, void *inout_buf[2], void *mix_buf,
			     int samples);

static int _aduio_track_update_output_samples(struct audio_track_t *handle, u32_t length)
{
	SYS_IRQ_FLAGS flags;

	sys_irq_lock(&flags);

	handle->output_samples += (u32_t )(length / handle->frame_size);

	sys_irq_unlock(&flags);

	return handle->output_samples;
}

#ifdef CONFIG_MEDIA_PCM_STREAM
static int _audio_track_data_mix(struct audio_track_t *handle, s16_t *src_buff, uint16_t samples, s16_t *dest_buff)
{
	int ret = 0;
	uint16_t mix_num = 0;
	uint8_t track_channels = (handle->audio_mode != AUDIO_MODE_MONO) ? 2 : 1;
	asin_pcm_t mix_pcm = {
		.channels = handle->mix_channels,
		.sample_bits = 16,
		.pcm = { handle->res_in_buf[0], handle->res_in_buf[1], },
	};

	if (stream_get_length(handle->mix_stream) <= 0 && handle->res_remain_samples <= 0)
		return 0;

	if (track_channels > 1)
		samples /= 2;

	while (1) {
		uint16_t mix_samples = MIN(samples, handle->res_remain_samples);
		s16_t *mix_buff[2] = {
			handle->res_out_buf[0] + handle->res_out_samples - handle->res_remain_samples,
			handle->res_out_buf[1] + handle->res_out_samples - handle->res_remain_samples,
		};

		/* 1) consume remain samples */
		if (handle->mix_handle && dest_buff == src_buff) {
			media_mix_process(handle->mix_handle, (void **)&dest_buff, mix_buff[0], mix_samples);
			dest_buff += track_channels * mix_samples;
			src_buff += track_channels * mix_samples;
		} else {
			if (track_channels > 1) {
				for (int i = 0; i < mix_samples; i++) {
					*dest_buff++ = (*src_buff++) / 2 + mix_buff[0][i] / 2;
					*dest_buff++ = (*src_buff++) / 2 + mix_buff[1][i] / 2;
				}
			} else {
				for (int i = 0; i < mix_samples; i++) {
					*dest_buff++ = (*src_buff++) / 2 + mix_buff[0][i] / 2;
				}
			}
		}

		handle->res_remain_samples -= mix_samples;
		samples -= mix_samples;
		mix_num += mix_samples;
		if (samples <= 0)
			break;

		/* 2) read mix stream and do resample as required */
		mix_pcm.samples = 0;
		ret = stream_read_pcm(&mix_pcm, handle->mix_stream, handle->res_in_samples, INT32_MAX);
		if (ret <= 0)
			break;

		if (handle->res_handle) {
			uint8_t res_channels = MIN(handle->mix_channels, track_channels);

			if (ret < handle->res_in_samples) {
				memset((uint16_t *)mix_pcm.pcm[0] + ret, 0, (handle->res_in_samples - ret) * 2);
				if (res_channels > 1)
					memset((uint16_t *)mix_pcm.pcm[1] + ret, 0, (handle->res_in_samples - ret) * 2);
			}

			/* do resample */
			handle->res_out_samples = media_resample_process(handle->res_handle, res_channels,
					(void **)handle->res_out_buf, (void **)mix_pcm.pcm, handle->res_in_samples);
			handle->res_remain_samples = handle->res_out_samples;
		} else {
			handle->res_out_samples = ret;
			handle->res_remain_samples = handle->res_out_samples;
		}
	}

	return mix_num;
}
#endif

#ifdef CONFIG_AUDIO_DATA_DEBUG
static void debug_data_rate(uint8_t* buf, uint16_t len)
{
	static uint32_t received_data;
	static uint32_t cycle_start = 0;
	uint32_t diff_sec;

	if (0 == cycle_start ) {
		received_data = 0;
		cycle_start = k_cycle_get_32();
	}

	received_data += len;

	diff_sec = (k_cycle_get_32() - cycle_start)/sys_clock_hw_cycles_per_sec;
	if (diff_sec >= 5) {
		SYS_LOG_INF("track %dBps", received_data / diff_sec);

		print_buffer(buf, 1, 16, 32, 0xF0000000);
		received_data = 0;
		cycle_start = k_cycle_get_32();
	}

}
#endif

__ramfunc int _stream_get_length(struct audio_track_t *audio_track, io_stream_t handle)
{
#ifdef CONFIG_DSP_OUTPUT_24BIT_WIDTH_IN_BMS
	int len = stream_get_length(audio_track->audio_stream);
	if (audio_policy_get_bis_link_delay_ms()) {
		len = len * 4 / 3;
#ifdef CONFIG_DSP_OUTPUT_1_CH_IN_BMS
		len = len * 2;
#endif
	}

	return len;
#else
	return stream_get_length(handle);
#endif
}

__ramfunc int _stream_read(struct audio_track_t *audio_track, io_stream_t handle, unsigned char *buf, int num)
{
#ifdef CONFIG_DSP_OUTPUT_24BIT_WIDTH_IN_BMS
	int ret = 0;
	int len = num;
	if (audio_policy_get_bis_link_delay_ms()) {
		len = len * 3 / 4;
#ifdef CONFIG_DSP_OUTPUT_1_CH_IN_BMS
		len = len / 2;
#endif
	}

	ret = stream_read(audio_track->audio_stream, buf, len);
	if (ret == len) {
		if(len != num) {
			uint32_t *dst = (uint32_t *)buf;
			uint8_t *src = (uint8_t *)buf;
			int l32 = num;
#ifdef CONFIG_DSP_OUTPUT_1_CH_IN_BMS
			l32 = l32 / 2;
#endif
			for (int i = l32 * 3 / 4 - 1, j = l32 / 4 - 1; i >= 0;) {
				uint32_t v = ((uint32_t)src[i-2])<<8;
				v |= ((uint32_t)src[i-1])<<16;
				v |= ((uint32_t)src[i])<<24;
				i -= 3;
				dst[j--] = v;
			}

#ifdef CONFIG_DSP_OUTPUT_1_CH_IN_BMS
			{
				int32_t *dst = (int32_t *)buf;
				int32_t *src = (int32_t *)buf;
				for (int i = l32 / 4 - 1, j = l32 * 2 / 4 - 1; i >= 0;) {
					dst[j--] = src[i];
					dst[j--] = src[i];
					i--;
				}
			}
#endif
			ret = num;
		}
	}

	return ret;
#else
	return stream_read(handle, buf, num);
#endif
}

__ramfunc int _audio_track_request_more_data(void *handle, uint32_t reason)
{
	static uint8_t printk_cnt;
	struct audio_track_t *audio_track = (struct audio_track_t *)handle;
	int read_len = audio_track->pcm_frame_size / 2;
	int stream_length = _stream_get_length(audio_track, audio_track->audio_stream);
	int ret = 0;
	bool reload_mode = ((audio_track->channel_mode & AUDIO_DMA_RELOAD_MODE) == AUDIO_DMA_RELOAD_MODE);
	uint8_t *buf = NULL;

#ifdef CONFIG_LOGIC_ANALYZER
	logic_switch(1);
#endif


	if (reload_mode) {
		if (reason == AOUT_DMA_IRQ_HF) {
			buf = audio_track->pcm_frame_buff;
		} else if (reason == AOUT_DMA_IRQ_TC) {
			buf = audio_track->pcm_frame_buff + read_len;
		}
	} else {
		buf = audio_track->pcm_frame_buff;
		if (stream_length > audio_track->pcm_frame_size) {
			read_len = audio_track->pcm_frame_size;
		} else {
			read_len = audio_track->pcm_frame_size / 2;
		}
		if (audio_track->channel_id == AOUT_FIFO_DAC0) {
			if (reason == AOUT_DMA_IRQ_TC) {
				printk("pcm empty\n");
			}
		}
	}
#if 0
	if (audio_track->compensate_samples > 0) {
		/* insert data */
		if (audio_track->compensate_samples < read_len) {
			read_len = audio_track->compensate_samples;
		}
		memset(buf, 0, read_len);
		audio_track->fill_cnt += read_len;
		audio_track->compensate_samples -= read_len;
		goto exit;
	} else if (audio_track->compensate_samples < 0) {
		/* drop data */
		if (read_len > -audio_track->compensate_samples) {
			read_len = -audio_track->compensate_samples;
		}
		if (stream_get_length(audio_track->audio_stream) >= read_len * 2) {
			stream_read(audio_track->audio_stream, buf, read_len);
			_aduio_track_update_output_samples(audio_track, read_len);
			audio_track->fill_cnt -= read_len;
			audio_track->compensate_samples += read_len;
		}
	}
#endif
	if (read_len > _stream_get_length(audio_track, audio_track->audio_stream)) {
		if (!audio_track->flushed) {
			_stream_read(audio_track, audio_track->audio_stream, buf, stream_length);
			_aduio_track_update_output_samples(audio_track, stream_length);
			memset(buf + stream_length, 0, read_len - stream_length);
			audio_track->fill_cnt += read_len;
			if (!printk_cnt++) {
				if (audio_track->stream_type != AUDIO_STREAM_USOUND
					&& audio_track->stream_type != AUDIO_STREAM_SUBWOOFER){
					extern void playback_service_dump_all_simple(void);
					playback_service_dump_all_simple();
				}
				printk("<md>f s%d %d/%d %d\n", audio_track->stream_type, stream_length, read_len, printk_cnt);
			}
			goto exit;
		} else {
			read_len = _stream_get_length(audio_track, audio_track->audio_stream);
			if (read_len > audio_track->pcm_frame_size / 2)
				read_len = audio_track->pcm_frame_size / 2;
			if (read_len < 0)
				goto exit;
		}
	} else {
		printk_cnt = 0;
	}

	ret = _stream_read(audio_track, audio_track->audio_stream, buf, read_len);
	if (ret != read_len) {
		memset(buf, 0, read_len);
		audio_track->fill_cnt += read_len;
		if (!printk_cnt++) {
			printk("F\n");
		}
	} else {
		_aduio_track_update_output_samples(audio_track, read_len);
	}

	if (audio_track->timeline) {
		timeline_trigger_listener(audio_track->timeline);
	}

	if (audio_track->muted) {
		memset(buf, 0, read_len);
		//audio_track->fill_cnt += read_len;
		//goto exit;
	}
#ifdef CONFIG_AUDIO_DATA_DEBUG_SINE
	pcm_data_cycle_load(buf, read_len, 1);
#endif


#ifdef CONFIG_AUDIO_DATA_DEBUG

	debug_data_rate(buf, read_len);
#endif

#ifdef CONFIG_PLAYTTS
#ifdef CONFIG_PLAY_MERGE_TTS
	tts_merge_manager_request_more_data(buf, read_len);
#endif
#endif

exit:
#ifdef CONFIG_AUDIO_TRACK_LESS_DATA_FADE
#if FADE_OUT_TIME_MS == 0
	if (audio_track->fade_handle && reload_mode) {
		media_fadeinout_process(audio_track->fade_handle, buf, read_len / audio_track->frame_size, audio_track->audio_mode == AUDIO_MODE_MONO ? 1 : 2,
				audio_track->audio_format == AUDIO_FORMAT_PCM_16_BIT ? 1 : 2);
	}
#endif
#endif

	if (!reload_mode && read_len > 0) {
#if FADE_OUT_TIME_MS > 0
		if (audio_track->flushed && audio_track->fade_handle) {
			int samples = audio_track->audio_mode == AUDIO_MODE_MONO ?
					    read_len / 2 : read_len / 4;
			media_fade_process(audio_track->fade_handle, (void **)&buf, samples);
		}
#endif
		hal_aout_channel_write_data(audio_track->audio_handle, buf, read_len);
	}

	if ((audio_track->channel_id == AOUT_FIFO_DAC0 || audio_track->channel_id == AOUT_FIFO_DAC1) && !reload_mode) {
		/**local music max send 3K pcm data */
		if (audio_track->stream_type == AUDIO_STREAM_LOCAL_MUSIC) {
			if (_stream_get_length(audio_track, audio_track->audio_stream) > audio_track->pcm_frame_size) {
				_stream_read(audio_track, audio_track->audio_stream, buf, audio_track->pcm_frame_size);
				_aduio_track_update_output_samples(audio_track, audio_track->pcm_frame_size);
#if FADE_OUT_TIME_MS > 0
				if (audio_track->flushed && audio_track->fade_handle) {
					int samples = audio_track->audio_mode == AUDIO_MODE_MONO ?
								audio_track->pcm_frame_size / 2 : audio_track->pcm_frame_size / 4;
					media_fade_process(audio_track->fade_handle, (void **)&buf, samples);
				}
#endif
				hal_aout_channel_write_data(audio_track->audio_handle, buf, audio_track->pcm_frame_size);
			}

			if (_stream_get_length(audio_track, audio_track->audio_stream) > audio_track->pcm_frame_size) {
				_stream_read(audio_track, audio_track->audio_stream, buf, audio_track->pcm_frame_size);
				_aduio_track_update_output_samples(audio_track, audio_track->pcm_frame_size);
#if FADE_OUT_TIME_MS > 0
				if (audio_track->flushed && audio_track->fade_handle) {
					int samples = audio_track->audio_mode == AUDIO_MODE_MONO ?
								audio_track->pcm_frame_size / 2 : audio_track->pcm_frame_size / 4;
					media_fade_process(audio_track->fade_handle, (void **)&buf, samples);
				}
#endif
				hal_aout_channel_write_data(audio_track->audio_handle, buf, audio_track->pcm_frame_size);
			}
		}

		/**last frame send more 2 samples */
		if (audio_track->flushed && _stream_get_length(audio_track, audio_track->audio_stream) == 0) {
			memset(buf, 0, 8);
			hal_aout_channel_write_data(audio_track->audio_handle, buf, 8);
		}
	}
	return 0;
}

static void *_audio_track_init(struct audio_track_t *handle)
{
	audio_out_init_param_t aout_param = {0};
	asrc_setting_t asrc_setting = {0};

#ifdef AOUT_CHANNEL_AA
	if (handle->channel_type == AOUT_CHANNEL_AA) {
		aout_param.aa_mode = 1;
	}
#endif

	aout_param.sample_rate =  handle->output_sample_rate;
	aout_param.channel_type = handle->channel_type;
	aout_param.channel_id =  handle->channel_id;
	if(handle->audio_format != AUDIO_FORMAT_PCM_16_BIT)
		aout_param.data_width = 32;
	else
		aout_param.data_width = 16;
	aout_param.channel_mode = handle->audio_mode;

	aout_param.left_volume = audio_system_get_current_pa_volume(handle->stream_type);
	aout_param.right_volume = aout_param.left_volume;

	aout_param.sample_cnt_enable = 1;

	aout_param.callback = _audio_track_request_more_data;
	aout_param.callback_data = handle;

	if ((handle->channel_mode & AUDIO_DMA_RELOAD_MODE) == AUDIO_DMA_RELOAD_MODE) {
		aout_param.dma_reload = 1;
		aout_param.reload_len = handle->pcm_frame_size;
		aout_param.reload_addr = handle->pcm_frame_buff;
	}

	if (handle->output_sample_rate != handle->sample_rate) {
		asrc_setting.ctl_mode = 0;
		asrc_setting.input_sr = handle->sample_rate;
		asrc_setting.by_pass = 0;
		if(handle->audio_format != AUDIO_FORMAT_PCM_16_BIT)
			asrc_setting.data_width = 32;
		else
			asrc_setting.data_width = 16;
		aout_param.asrc_setting = &asrc_setting;
	}

	handle->sdm_osr = 1;
#ifdef CONFIG_SOFT_SDM_CNT
    handle->sdm_osr = SOFT_SDM_OSR;
#endif
	handle->sdm_sample_rate = get_sample_rate_hz(handle->output_sample_rate) * handle->sdm_osr;

	SYS_LOG_INF("sample_rate=%d, width=%d", aout_param.sample_rate, aout_param.data_width);
	return hal_aout_channel_open(&aout_param);
}

static void _audio_track_stream_observer_notify(void *observer, int readoff, int writeoff, int total_size,
										unsigned char *buf, int num, stream_notify_type type)
{
	struct audio_track_t *handle = (struct audio_track_t *)observer;

#if FADE_IN_TIME_MS > 0
	if (handle->fade_handle && type == STREAM_NOTIFY_PRE_WRITE) {
		int samples = handle->audio_mode == AUDIO_MODE_MONO ? num / 2 : num / 4;

		media_fade_process(handle->fade_handle, (void **)&buf, samples);
	}
#endif

#ifdef CONFIG_MEDIA_PCM_STREAM
	audio_system_mutex_lock();
	if (handle->mix_stream && (type == STREAM_NOTIFY_PRE_WRITE)) {
		_audio_track_data_mix(handle, (s16_t *)buf, num / 2, (s16_t *)buf);
	}
	audio_system_mutex_unlock();
#endif

	if (!handle->started
		&& (type == STREAM_NOTIFY_WRITE)
		&& !audio_track_is_waitto_start(handle)
		&& _stream_get_length(handle, handle->audio_stream) >= handle->pcm_frame_size) {

		os_sched_lock();

		if (handle->event_cb)
			handle->event_cb(1, handle->user_data);

		handle->started = 1;

		_stream_read(handle, handle->audio_stream, handle->pcm_frame_buff, handle->pcm_frame_size);
		_aduio_track_update_output_samples(handle, handle->pcm_frame_size);

		if ((handle->channel_mode & AUDIO_DMA_RELOAD_MODE) == AUDIO_DMA_RELOAD_MODE) {
			printk("hal_aout_channel_start \n");
			hal_aout_channel_start(handle->audio_handle);
		} else {
			hal_aout_channel_write_data(handle->audio_handle, handle->pcm_frame_buff, handle->pcm_frame_size);
		}

		os_sched_unlock();
	}
}

struct audio_track_t *audio_track_create(uint8_t stream_type, int sample_rate,
									uint8_t format, uint8_t audio_mode, void *outer_stream,
									void (*event_cb)(uint8_t event, void *user_data), void *user_data)
{
	struct audio_track_t *audio_track = NULL;

	audio_system_mutex_lock();

	audio_track = mem_malloc(sizeof(struct audio_track_t));
	if (!audio_track) {
		SYS_LOG_ERR("audio track malloc failed");
		SYS_EVENT_ERR(EVENT_AUDIO_ERROR_TYPE, 't', __LINE__);
		goto err_exit;
	}

	memset(audio_track, 0, sizeof(struct audio_track_t));

	audio_track->stream_type = stream_type;
	audio_track->audio_format = format;
	audio_track->audio_mode = audio_mode;
	audio_track->sample_rate = sample_rate;
	audio_track->compensate_samples = 0;

	audio_track->channel_type = audio_policy_get_out_channel_type(stream_type);
	audio_track->channel_id = audio_policy_get_out_channel_id(stream_type);
	audio_track->channel_mode = audio_policy_get_out_channel_mode(stream_type);
	audio_track->volume = audio_system_get_stream_volume(stream_type);
	audio_track->output_sample_rate = audio_system_get_output_sample_rate();

	if (!audio_track->output_sample_rate)
		audio_track->output_sample_rate = audio_track->sample_rate;


	if (audio_track->audio_mode == AUDIO_MODE_DEFAULT)
		audio_track->audio_mode = audio_policy_get_out_audio_mode(stream_type);

	if (audio_track->audio_mode == AUDIO_MODE_MONO) {
		audio_track->frame_size = 2;
	} else if (audio_mode == AUDIO_MODE_STEREO) {
		audio_track->frame_size = 4;
	}
    if(audio_track->audio_format != AUDIO_FORMAT_PCM_16_BIT) {
        audio_track->frame_size *= 2;
    }

	/* dma reload buff */
	audio_track->pcm_frame_size = audio_policy_get_track_pcm_buff_size(stream_type, sample_rate, audio_mode, format);

	if (stream_type != AUDIO_STREAM_SUBWOOFER) {
		audio_track->timeline = timeline_create(TIMELINE_TYPE_AUDIO_TX,
		audio_track->pcm_frame_size/2/audio_track->sample_rate*1000/audio_track->frame_size);
		if (!audio_track->timeline) {
			SYS_LOG_ERR("audio track timeline malloc failed");
			SYS_EVENT_ERR(EVENT_AUDIO_ERROR_TYPE, 't', __LINE__);
		} else {
			timeline_start(audio_track->timeline);
		}
		audio_track->pcm_frame_buff = reload_pcm_buff;

	}else {
		audio_track->pcm_frame_size = media_mem_get_cache_pool_size(OUTPUT_PCM, stream_type);
		audio_track->pcm_frame_buff = media_mem_get_cache_pool(OUTPUT_PCM, stream_type);
	}

	if (!audio_track->pcm_frame_buff) {
		SYS_LOG_ERR("pcm_frame_buff init failed");
		SYS_EVENT_ERR(EVENT_AUDIO_ERROR_TYPE, 't', __LINE__);
		goto err_exit;
	}

	audio_track->audio_handle = _audio_track_init(audio_track);
	if (!audio_track->audio_handle) {
		SYS_LOG_ERR("audio track init failed");
		SYS_EVENT_ERR(EVENT_AUDIO_ERROR_TYPE, 't', __LINE__);
		goto err_exit;
	}

	if (audio_system_get_current_volume(audio_track->stream_type) == 0) {
		hal_aout_channel_mute_ctl(audio_track->audio_handle, 1);
	} else {
		hal_aout_channel_mute_ctl(audio_track->audio_handle, 0);
	}

	if (outer_stream) {
		audio_track->audio_stream = ringbuff_stream_create((struct acts_ringbuf *)outer_stream);
	} else {
		audio_track->audio_stream = ringbuff_stream_create_ext(
									media_mem_get_cache_pool(OUTPUT_PCM, stream_type),
									media_mem_get_cache_pool_size(OUTPUT_PCM, stream_type));
	}

	if (!audio_track->audio_stream) {
		SYS_LOG_ERR("stream create failed");
		SYS_EVENT_ERR(EVENT_AUDIO_ERROR_TYPE, 't', __LINE__);
		goto err_exit;
	}

	if (stream_open(audio_track->audio_stream, MODE_IN_OUT | MODE_WRITE_BLOCK)) {
		stream_destroy(audio_track->audio_stream);
		audio_track->audio_stream = NULL;
		SYS_LOG_ERR(" stream open failed ");
		SYS_EVENT_ERR(EVENT_AUDIO_ERROR_TYPE, 't', __LINE__);
		goto err_exit;
	}

	stream_set_observer(audio_track->audio_stream, audio_track,
		_audio_track_stream_observer_notify, STREAM_NOTIFY_WRITE | STREAM_NOTIFY_PRE_WRITE);

	if (audio_system_register_track(audio_track)) {
		SYS_LOG_ERR(" registy track failed ");
		SYS_EVENT_ERR(EVENT_AUDIO_ERROR_TYPE, 't', __LINE__);
		goto err_exit;
	}

	if (audio_track->stream_type != AUDIO_STREAM_VOICE) {
#if FADE_IN_TIME_MS > 0 || FADE_OUT_TIME_MS > 0
		audio_track->fade_handle = media_fade_open(
				audio_track->sample_rate,
				audio_track->audio_mode == AUDIO_MODE_MONO ? 1 : 2, 1);
#else
#ifdef CONFIG_AUDIO_TRACK_LESS_DATA_FADE
		audio_track->fade_handle = media_fadeinout_open();
#endif
#endif
#if FADE_IN_TIME_MS > 0
		if (audio_track->fade_handle)
			media_fade_in_set(audio_track->fade_handle, FADE_IN_TIME_MS);
#endif
	}

	audio_track->event_cb = event_cb;
	audio_track->user_data = user_data;

	if (system_check_low_latencey_mode() && audio_track->sample_rate <= 16) {
		hal_aout_set_pcm_threshold(audio_track->audio_handle, 0x10, 0x20);
	}
	hal_aout_channel_enable_sample_cnt(audio_track->audio_handle,1);
	hal_aout_channel_reset_sample_cnt(audio_track->audio_handle);

	SYS_LOG_INF("track(%d,%d,%d,%d,%d,%d,%d,%d,%d)\n",
				audio_track->stream_type,
				audio_track->audio_format,
				audio_track->audio_mode,
				audio_track->channel_type,
				audio_track->channel_id,
				audio_track->channel_mode,
				audio_track->sample_rate,
				audio_track->output_sample_rate,
				audio_track->volume);
	SYS_LOG_INF("audio_stream:%p\n", audio_track->audio_stream);

	audio_system_mutex_unlock();


	return audio_track;

err_exit:
	if (audio_track)
		mem_free(audio_track);
	audio_system_mutex_unlock();
	return NULL;
}

int audio_track_destory(struct audio_track_t *handle)
{
	assert(handle);

	SYS_LOG_INF("destory %p begin", handle);
	audio_system_mutex_lock();

	if (audio_system_unregister_track(handle)) {
		SYS_LOG_ERR(" failed\n");
		SYS_EVENT_ERR(EVENT_AUDIO_ERROR_TYPE, 't', __LINE__);
		return -ESRCH;
	}

	if (handle->timeline) {
		timeline_stop(handle->timeline);
		timeline_release(handle->timeline);
	}

	if (handle->audio_handle) {
		hal_aout_channel_close(handle->audio_handle);
	}

	if (handle->audio_stream)
		stream_destroy(handle->audio_stream);

#if FADE_IN_TIME_MS > 0 || FADE_OUT_TIME_MS > 0
	if (handle->fade_handle)
		media_fade_close(handle->fade_handle);
#else
#ifdef CONFIG_AUDIO_TRACK_LESS_DATA_FADE
	if (handle->fade_handle)
		media_fadeinout_close(handle->fade_handle);
#endif
#endif

	mem_free(handle);

	audio_system_mutex_unlock();
	SYS_LOG_INF("destory %p ok", handle);
	return 0;
}

int audio_track_start(struct audio_track_t *handle)
{
	assert(handle);
	int prepare_start = 0;
	u32_t ret = 0;
	SYS_IRQ_FLAGS flags;

	sys_irq_lock(&flags);
	if (!handle->started && _stream_get_length(handle, handle->audio_stream) >= handle->pcm_frame_size) {
		if(!k_is_in_isr())
			os_sched_lock();

		if (handle->event_cb) {
			handle->event_cb(1, handle->user_data);
		}

		handle->started = 1;
		if(handle->waitto_start) {
			if(handle->stream_type == AUDIO_STREAM_LE_AUDIO
			|| handle->stream_type == AUDIO_STREAM_LINEIN
			|| handle->stream_type == AUDIO_STREAM_USOUND
			|| handle->stream_type == AUDIO_STREAM_SPDIF_IN
			|| handle->stream_type == AUDIO_STREAM_I2SRX_IN
			|| handle->stream_type == AUDIO_STREAM_SOUNDBAR
			|| handle->stream_type == AUDIO_STREAM_LOCAL_MUSIC
			|| handle->stream_type == AUDIO_STREAM_SUBWOOFER){
				prepare_start = 1;
			}
			audio_track_set_waitto_start(handle, false);
		}

		if(handle->stream_type == AUDIO_STREAM_LE_AUDIO){
			memset(handle->pcm_frame_buff, 0, handle->pcm_frame_size);
			_stream_read(handle, handle->audio_stream, handle->pcm_frame_buff + handle->pcm_frame_size/2, handle->pcm_frame_size/2);
			_aduio_track_update_output_samples(handle, handle->pcm_frame_size/2);
		} else {
			_stream_read(handle, handle->audio_stream, handle->pcm_frame_buff, handle->pcm_frame_size);
			_aduio_track_update_output_samples(handle, handle->pcm_frame_size);
		}

		if ((handle->channel_mode & AUDIO_DMA_RELOAD_MODE) == AUDIO_DMA_RELOAD_MODE) {
			if (prepare_start)
				hal_aout_channel_prepare_start(handle->audio_handle);
			ret = (u32_t)hal_aout_channel_start(handle->audio_handle);
			if (prepare_start && (ret > DMA_REG_BASE && ret < DMA_REG_BASE + 0x1000)){
				handle->phy_dma = ret;
				SYS_LOG_INF("pre dma:%x %d\n", ret, k_cycle_get_32());
			}
		} else {
			hal_aout_channel_write_data(handle->audio_handle, handle->pcm_frame_buff, handle->pcm_frame_size);
		}

		if (handle->sync_cb){
		    handle->sync_cb(handle->sync_data);
		}
		if(!k_is_in_isr())
			os_sched_unlock();
	}
	sys_irq_unlock(&flags);

	return 0;
}

int audio_track_stop(struct audio_track_t *handle)
{
	assert(handle);

	SYS_LOG_INF("stop %p begin ", handle);

	if (handle->audio_handle)
		hal_aout_channel_stop(handle->audio_handle);

	if (handle->audio_stream)
		stream_close(handle->audio_stream);

	SYS_LOG_INF("stop %p ok ", handle);
	return 0;
}

int audio_track_pause(struct audio_track_t *handle)
{
	assert(handle);
	if (handle->stream_type != AUDIO_STREAM_LE_AUDIO){
		handle->muted = 1;

		stream_flush(handle->audio_stream);
	} else {
		if (handle->audio_handle)
			hal_aout_channel_stop(handle->audio_handle);

		handle->started = 0;
	}

	return 0;
}

int audio_track_resume(struct audio_track_t *handle)
{
	assert(handle);
	if (handle->stream_type != AUDIO_STREAM_LE_AUDIO){
		handle->muted = 0;
	}

	return 0;
}

int audio_track_mute(struct audio_track_t *handle, int mute)
{
	assert(handle);

    handle->muted = mute;

	return 0;
}

int audio_track_is_muted(struct audio_track_t *handle)
{
	assert(handle);

    return handle->muted;
}

int audio_track_write(struct audio_track_t *handle, unsigned char *buf, int num)
{
	int ret = 0;

	assert(handle && handle->audio_stream);

	ret = stream_write(handle->audio_stream, buf, num);
	if (ret != num) {
		SYS_LOG_WRN(" %d %d\n", ret, num);
	}

	return ret;
}

int audio_track_flush(struct audio_track_t *handle)
{
#if 1
	int try_cnt = 0;
#endif
#ifdef CONFIG_SYS_IRQ_LOCK
	SYS_IRQ_FLAGS flags;
#endif
	assert(handle);
#ifdef CONFIG_SYS_IRQ_LOCK
	sys_irq_lock(&flags);
#endif
	if (handle->flushed) {
	#ifdef CONFIG_SYS_IRQ_LOCK
		sys_irq_unlock(&flags);
	#endif
		return 0;
	}
	/**wait trace stream read empty*/
	handle->flushed = 1;

#ifdef CONFIG_SYS_IRQ_LOCK
	sys_irq_unlock(&flags);
#endif

#if FADE_OUT_TIME_MS > 0
	if (handle->fade_handle) {
		int fade_time = _stream_get_length(handle, handle->audio_stream) / 2;

		if (handle->audio_mode == AUDIO_MODE_STEREO)
			fade_time /= 2;

		fade_time /= handle->sample_rate;

		audio_track_set_fade_out(handle, fade_time);
	}
#endif
#if 1
#ifdef CONFIG_PLAYTTS
	if(tts_merge_manager_is_running()){
		SYS_LOG_INF("wait mix tts\n");
		while(tts_merge_manager_is_running()
			&& try_cnt++ < 450){
			if(_stream_get_length(handle, handle->audio_stream) < handle->pcm_frame_size / 2){
				acts_ringbuf_fill(stream_get_ringbuffer(handle->audio_stream),0,stream_get_space(handle->audio_stream));
			}
			os_sleep(1);
		}
		SYS_LOG_INF("wait mix tts end %d\n",try_cnt);
	}else{
		while (_stream_get_length(handle, handle->audio_stream) > 0
				&& try_cnt++ < 100 && handle->started) {
			if (try_cnt % 10 == 0) {
				SYS_LOG_INF("try_cnt %d stream %d\n", try_cnt,
						_stream_get_length(handle, handle->audio_stream));
			}
			os_sleep(2);
		}
	}
#else
	while (_stream_get_length(handle, handle->audio_stream) > 0
			&& try_cnt++ < 100 && handle->started) {
		if (try_cnt % 10 == 0) {
			SYS_LOG_INF("try_cnt %d stream %d\n", try_cnt,
					_stream_get_length(handle, handle->audio_stream));
		}
		os_sleep(2);
	}
#endif

	SYS_LOG_INF("try_cnt %d, left_len %d\n", try_cnt,
			_stream_get_length(handle, handle->audio_stream));
	//audio_track_set_mix_stream(handle, NULL, 0, 1, AUDIO_STREAM_TTS);
#endif
	// if (handle->audio_stream) {
	// 	stream_close(handle->audio_stream);
	// }

	return 0;
}

int audio_track_set_fade_out(struct audio_track_t *handle, int fade_time)
{
	assert(handle);

#if FADE_OUT_TIME_MS > 0
	if (handle->fade_handle && !handle->fade_out) {

		media_fade_out_set(handle->fade_handle, MIN(FADE_OUT_TIME_MS, fade_time));
		handle->fade_out = 1;
	}
#else
#ifdef CONFIG_AUDIO_TRACK_LESS_DATA_FADE
	if (handle->fade_handle && fade_time){
		if(!handle->fade_out) {
			media_fadeinout_set(handle->fade_handle, fade_time * handle->sample_rate, 1);
			handle->fade_out = 1;
		} else {
			handle->fade_out = 0;
			media_fadeinout_set(handle->fade_handle, fade_time * handle->sample_rate, 0);
		}
	}
#endif
#endif
	return 0;
}

int audio_track_set_waitto_start(struct audio_track_t *handle, bool wait)
{
	assert(handle);

	handle->waitto_start = wait ? 1 : 0;
	return 0;
}

int audio_track_is_waitto_start(struct audio_track_t *handle)
{
	if (!handle)
		return 0;

	return handle->waitto_start;
}

int audio_track_set_volume(struct audio_track_t *handle, int volume)
{
	uint32_t pa_volume = 0;

	assert(handle);

	SYS_LOG_INF(" volume %d\n", volume);
	if (volume) {
		hal_aout_channel_mute_ctl(handle->audio_handle, 0);
		pa_volume = audio_policy_get_pa_volume(handle->stream_type, volume);
#ifdef CONFIG_AUDIO_VOLUME_PA
		SYS_LOG_INF("stream %d, pa val=0x%x\n", handle->stream_type, pa_volume);
#else
		SYS_LOG_INF("stream %d, pa mdB=%d\n", handle->stream_type, pa_volume);
#endif
		hal_aout_channel_set_pa_vol_level(handle->audio_handle, pa_volume);
	} else {
		hal_aout_channel_mute_ctl(handle->audio_handle, 1);

#ifdef CONFIG_AUDIO_VOLUME_PA
		pa_volume = 0;
#else
		pa_volume = -71625;
#endif

		//do not set pa volume at mute status to avoid noise when doing unmute.
		//hal_aout_channel_set_pa_vol_level(handle->audio_handle, pa_volume);
	}

	handle->volume = volume;
	return 0;
}

int audio_track_set_pa_volume(struct audio_track_t *handle, int pa_volume)
{
	assert(handle);

	SYS_LOG_INF("pa_volume %d\n", pa_volume);

	hal_aout_channel_mute_ctl(handle->audio_handle, 0);
	hal_aout_channel_set_pa_vol_level(handle->audio_handle, pa_volume);

#ifndef CONFIG_AUDIO_VOLUME_PA
	/* sync back to volume level, though meaningless for AUDIO_STREAM_VOICE at present. */
	handle->volume = audio_policy_get_volume_level_by_db(handle->stream_type, pa_volume / 100);
#endif
	return 0;
}

int audio_track_set_mute(struct audio_track_t *handle, bool mute)
{
	assert(handle);

	SYS_LOG_INF(" mute %d\n", mute);

	hal_aout_channel_mute_ctl(handle->audio_handle, mute);

	return 0;
}

int audio_track_get_volume(struct audio_track_t *handle)
{
	assert(handle);

	return handle->volume;
}

int audio_track_set_samplerate(struct audio_track_t *handle, int sample_rate)
{
	assert(handle);

	handle->sample_rate = sample_rate;
	return 0;
}

int audio_track_get_samplerate(struct audio_track_t *handle)
{
	assert(handle);

	return handle->sample_rate;
}

io_stream_t audio_track_get_stream(struct audio_track_t *handle)
{
	assert(handle);

	return handle->audio_stream;
}

int audio_track_get_fill_samples(struct audio_track_t *handle)
{
	assert(handle);
	if (handle->fill_cnt) {
		//SYS_LOG_INF("fill_cnt %d\n", handle->fill_cnt);
	}
	return handle->fill_cnt;
}

int audio_track_compensate_samples(struct audio_track_t *handle, int samples_cnt)
{
#ifdef CONFIG_SYS_IRQ_LOCK
	SYS_IRQ_FLAGS flags;
#endif
	assert(handle);
#ifdef CONFIG_SYS_IRQ_LOCK
	sys_irq_lock(&flags);
#endif
	handle->compensate_samples = samples_cnt;
#ifdef CONFIG_SYS_IRQ_LOCK
	sys_irq_unlock(&flags);
#endif
	return 0;
}

#ifdef CONFIG_MEDIA_PCM_STREAM
int audio_track_set_mix_stream(struct audio_track_t *handle, io_stream_t mix_stream,
		uint8_t sample_rate, uint8_t channels, uint8_t stream_type)
{
	int res = 0;

	assert(handle);

	audio_system_mutex_lock();

	if (audio_system_get_track() != handle) {
		goto exit;
	}

	if (mix_stream) {
		uint16_t *frame_buf = media_mem_get_cache_pool(RESAMPLE_FRAME_DATA, stream_type);
		int frame_buf_size = media_mem_get_cache_pool_size(RESAMPLE_FRAME_DATA, stream_type);
		uint8_t track_channels = (handle->audio_mode != AUDIO_MODE_MONO) ? 2 : 1;
		uint8_t res_channels = MIN(channels, track_channels);

		if (sample_rate != handle->sample_rate) {
			int frame_size;

			handle->res_handle = media_resample_open(
					res_channels, sample_rate, handle->sample_rate,
					&handle->res_in_samples, &handle->res_out_samples, stream_type);
			if (!handle->res_handle) {
				SYS_LOG_ERR("media_resample_open failed");
				res = -ENOMEM;
				goto exit;
			}

			frame_size = channels * (ROUND_UP(handle->res_in_samples, 2) + ROUND_UP(handle->res_out_samples, 2));

			if (frame_buf_size / 2 < frame_size) {
				SYS_LOG_ERR("frame mem not enough");
				media_resample_close(handle->res_handle);
				handle->res_handle = NULL;
				res = -ENOMEM;
				goto exit;
			}

			handle->res_in_buf[0] = frame_buf;
			handle->res_in_buf[1] = (channels > 1) ?
					handle->res_in_buf[0] + ROUND_UP(handle->res_in_samples, 2) : handle->res_in_buf[0];

			handle->res_out_buf[0] = handle->res_in_buf[1] + ROUND_UP(handle->res_in_samples, 2);
			handle->res_out_buf[1] = (res_channels > 1) ?
					handle->res_out_buf[0] + ROUND_UP(handle->res_out_samples, 2) : handle->res_out_buf[0];
		} else {
			handle->res_in_samples = frame_buf_size / 2 / channels;
			handle->res_in_buf[0] = frame_buf;
			handle->res_in_buf[1] = (channels > 1) ?
					handle->res_in_buf[0] + handle->res_in_samples : handle->res_in_buf[0];

			handle->res_out_buf[0] = handle->res_in_buf[0];
			handle->res_out_buf[1] = handle->res_in_buf[1];
		}

		handle->res_out_samples = 0;
		handle->res_remain_samples = 0;

		/* open mix: only support 1 mix channels */
		if (handle->mix_channels == 1) {
			handle->mix_handle = media_mix_open(handle->sample_rate, track_channels, 1);
		}
	}

	handle->mix_stream = mix_stream;
	handle->mix_sample_rate = sample_rate;
	handle->mix_channels = channels;

	if (!handle->mix_stream) {
		if (handle->res_handle) {
			media_resample_close(handle->res_handle);
			handle->res_handle = NULL;
		}

		if (handle->mix_handle) {
			media_mix_close(handle->mix_handle);
			handle->mix_handle = NULL;
		}
	}

	SYS_LOG_INF("mix_stream %p, sample_rate %d->%d\n",
			mix_stream, sample_rate, handle->sample_rate);

exit:
	audio_system_mutex_unlock();
	return res;
}
#endif

io_stream_t audio_track_get_mix_stream(struct audio_track_t *handle)
{
	assert(handle);

	return handle->mix_stream;
}

u32_t audio_track_get_output_samples(struct audio_track_t *handle)
{
	SYS_IRQ_FLAGS flags;
	u32_t ret = 0;

	assert(handle);

	sys_irq_lock(&flags);

	ret = handle->output_samples;

	handle->output_samples = 0;

	sys_irq_unlock(&flags);
	return ret;
}

int audio_track_set_sync_callback(struct audio_track_t *handle, void(*callback)(void *), void *user_data)
{
	int ret = 0;

	assert(handle);

	handle->sync_cb = callback;
	handle->sync_data = user_data;

	return ret;
}

uint64_t audio_track_get_samples_cnt(struct audio_track_t *handle)
{
	int32_t channels = 1;
	int32_t prev_sample_cnt;
	int32_t sample_cnt;

	if(!handle->started){
		return 0;
	}

	if(handle->audio_mode == AUDIO_MODE_STEREO){
		channels = 2;
	}

	prev_sample_cnt = (hal_aout_channel_get_sample_cnt(handle->audio_handle)  & 0xffff) / channels;

	while(1){
		sample_cnt = (hal_aout_channel_get_sample_cnt(handle->audio_handle)	& 0xffff) / channels;

		if(sample_cnt != prev_sample_cnt){
			break;
		}
	}

	if (handle->last_samples_cnt > sample_cnt){
		handle->samples_overflow_cnt += (65536ll / channels);
	}

	handle->last_samples_cnt = sample_cnt;

	return 	(sample_cnt + handle->samples_overflow_cnt);
}

