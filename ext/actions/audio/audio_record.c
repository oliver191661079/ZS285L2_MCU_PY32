/*
 * Copyright (c) 2016 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief audio record.
*/

#include <os_common_api.h>
#include <mem_manager.h>
#include <msg_manager.h>
#include <audio_hal.h>
#include <audio_record.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <media_mem.h>
#include <ringbuff_stream.h>
#include <audio_device.h>
#ifdef CONFIG_BLUETOOTH
#include <bt_manager.h>
#endif
#include <logic.h>
#include <media_type.h>

#ifdef CONFIG_SRRC_TEST
#define CONFIG_AUDIO_DATA_DEBUG_SINE
#endif

#ifdef CONFIG_AUDIO_DATA_DEBUG_SINE
extern void pcm_data_cycle_load(unsigned char *buf, unsigned int len, unsigned char tag);
#endif

#define SYS_LOG_DOMAIN "record"
#include <logging/sys_log.h>

#ifdef CONFIG_ACT_EVENT
#include <logging/log_core.h>
LOG_MODULE_DECLARE(media_event,CONFIG_ACT_EVENT_APP_COMPILE_LEVEL);
#endif

#define AUDIO_RECORD_PCM_BUFF_SIZE  1024

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
		SYS_LOG_INF("record %dBps", received_data / diff_sec);

		received_data = 0;
		print_buffer(buf, 1, 16, 32, 0xF0000000);
		cycle_start = k_cycle_get_32();
	}
}
#endif

int _audio_record_request_more_data(void *priv_data, uint32_t reason)
{
	int ret = 0;
    struct audio_record_t *handle = (struct audio_record_t *)priv_data;
	int num = handle->pcm_buff_size / 2;
	uint8_t *buf = NULL;


#ifdef CONFIG_LOGIC_ANALYZER
	logic_switch(0);
#endif

	if (!handle->audio_stream || handle->paused)
		goto exit;


	if (reason == AOUT_DMA_IRQ_HF) {
		buf = handle->pcm_buff;
	} else if (reason == AOUT_DMA_IRQ_TC) {
		buf = handle->pcm_buff + num;
	}

	if (handle->muted) {
		memset(buf, 0, num);
	}

	/**TODO: this avoid adc noise for first block*/
	if (handle->first_frame) {
		memset(buf, 0, num);
		handle->first_frame = 0;
		if(handle->media_format == NAV_TYPE && handle->stream_type != AUDIO_STREAM_LE_AUDIO){
			ret = stream_write(handle->audio_stream, buf, num);
		}
	}

#ifdef CONFIG_AUDIO_DATA_DEBUG_SINE
	pcm_data_cycle_load(buf, num, 0);
#endif

	ret = stream_write(handle->audio_stream, buf, num);
	if (ret != num) {
		SYS_LOG_WRN("data full ret %d %d %d %d", ret, num, stream_get_length(handle->audio_stream), stream_get_space(handle->audio_stream));
	}


	handle->input_samples += num / handle->frame_size;

#ifdef CONFIG_AUDIO_VOICE_HARDWARE_REFERENCE
	if (handle->ref_stream) {
//		ret = stream_write(handle->ref_stream, buf + (num << 1), num);
		ret = acts_ringbuf_put(handle->ref_stream, buf + (num << 1), num);
		if (ret != num) {
			SYS_LOG_WRN("ref data full ret %d %d %d %d", ret, num, acts_ringbuf_length(handle->ref_stream), acts_ringbuf_space(handle->ref_stream));
		}
	}
#endif
	if (handle->timeline) {
		timeline_trigger_listener(handle->timeline);
	}

#ifdef CONFIG_AUDIO_DATA_DEBUG
	debug_data_rate(buf, num);
#endif

exit:
	return 0;
}

static void *_audio_record_init(struct audio_record_t *handle)
{
	audio_in_init_param_t ain_param = {0};

	memset(&ain_param, 0, sizeof(audio_in_init_param_t));

	ain_param.sample_rate = handle->sample_rate;
	ain_param.adc_gain = handle->adc_gain;
	ain_param.input_gain = handle->input_gain;
	ain_param.boost_gain = 0;
	ain_param.data_mode = handle->audio_mode;
	ain_param.dma_reload = 1;
	ain_param.reload_addr = handle->pcm_buff;
	ain_param.reload_len = handle->pcm_buff_size;
	ain_param.channel_type = handle->channel_type;
	if(handle->audio_format == AUDIO_FORMAT_PCM_32_BIT) {
		ain_param.data_width = 32;
	} else {
		ain_param.data_width = 16;
	}

	ain_param.audio_device = audio_policy_get_record_channel_id(handle->stream_type);

	ain_param.callback = _audio_record_request_more_data;
	ain_param.callback_data = handle;

	SYS_LOG_INF("sample_rate=%d, width=%d", ain_param.sample_rate, ain_param.data_width);
    return hal_ain_channel_open(&(ain_param));
}

struct audio_record_t *audio_record_create(uint8_t stream_type, int sample_rate_input, int sample_rate_output,
									uint8_t format, uint8_t audio_mode, void *outer_stream, uint8_t media_format)
{
	struct audio_record_t *audio_record = NULL;

	audio_record = mem_malloc(sizeof(struct audio_record_t));
	if (!audio_record)
		return NULL;

	/* dma reload buff */
	audio_record->pcm_buff_size = audio_policy_get_record_pcm_buff_size(stream_type, sample_rate_input, audio_mode, format);
	SYS_LOG_INF("size %d", audio_record->pcm_buff_size);
	if (0 == audio_record->pcm_buff_size) {
		SYS_LOG_ERR("zero size.");
		SYS_EVENT_ERR(EVENT_AUDIO_ERROR_TYPE, 'r', __LINE__);
		goto err_exit;
	}
	audio_record->pcm_buff = mem_malloc(audio_record->pcm_buff_size);
	if (!audio_record->pcm_buff)
		goto err_exit;
#ifdef CONFIG_AUDIO_VOICE_HARDWARE_REFERENCE
	if (stream_type == AUDIO_STREAM_VOICE) {
		audio_record->pcm_buff_size >>= 1;
	}
#endif
	audio_record->stream_type = stream_type;
	audio_record->audio_format = format;
	audio_record->media_format = media_format;
	audio_record->audio_mode = audio_mode;
	audio_record->output_sample_rate = sample_rate_output;

	audio_record->channel_type = audio_policy_get_record_channel_type(stream_type);
	audio_record->channel_mode = audio_policy_get_record_channel_mode(stream_type);
	audio_record->adc_gain = audio_policy_get_record_adc_gain(stream_type);
	audio_record->input_gain = audio_policy_get_record_input_gain(stream_type);
	audio_record->sample_rate = sample_rate_input;
	audio_record->first_frame = 1;

	if (audio_record->audio_mode == AUDIO_MODE_DEFAULT)
		audio_record->audio_mode = audio_policy_get_record_audio_mode(stream_type);

	if (audio_record->audio_mode == AUDIO_MODE_MONO) {
		audio_record->frame_size = 2;
	} else if (audio_record->audio_mode == AUDIO_MODE_STEREO) {
		audio_record->frame_size = 4;
	}
	if(audio_record->audio_format == AUDIO_FORMAT_PCM_32_BIT) {
		audio_record->frame_size *= 2;
	}

	audio_record->timeline = timeline_create(TIMELINE_TYPE_AUDIO_RX,
		audio_record->pcm_buff_size/2/audio_record->sample_rate*1000/audio_record->frame_size);
	if (!audio_record->timeline) {
		SYS_LOG_ERR("audio record timeline malloc failed");
			SYS_EVENT_ERR(EVENT_AUDIO_ERROR_TYPE, 'r', __LINE__);
	} else {
		timeline_start(audio_record->timeline);
	}

	audio_record->audio_handle = _audio_record_init(audio_record);
	if (!audio_record->audio_handle) {
		goto err_exit;
	}

	if (outer_stream) {
		audio_record->audio_stream = ringbuff_stream_create((struct acts_ringbuf *)outer_stream);
	} else {
		audio_record->audio_stream = ringbuff_stream_create_ext(
									media_mem_get_cache_pool(INPUT_PCM, stream_type),
									media_mem_get_cache_pool_size(INPUT_PCM, stream_type));
	}

	if (!audio_record->audio_stream) {
		SYS_LOG_ERR("audio_stream create failed");
		SYS_EVENT_ERR(EVENT_AUDIO_ERROR_TYPE, 'r', __LINE__);
		goto err_exit;
	}

	if (stream_open(audio_record->audio_stream, MODE_IN_OUT)) {
		stream_destroy(audio_record->audio_stream);
		audio_record->audio_stream = NULL;
		SYS_LOG_ERR(" audio_stream open failed ");
		SYS_EVENT_ERR(EVENT_AUDIO_ERROR_TYPE, 'r', __LINE__);
		goto err_exit;
	}

	if (audio_system_register_record(audio_record)) {
		SYS_LOG_ERR(" audio_system_registy_track failed ");
		SYS_EVENT_ERR(EVENT_AUDIO_ERROR_TYPE, 'r', __LINE__);
		stream_close(audio_record->audio_stream);
		stream_destroy(audio_record->audio_stream);
		goto err_exit;
	}
	if (system_check_low_latencey_mode()) {
		if (audio_record->stream_type == AUDIO_STREAM_VOICE) {
			if(sample_rate_input == 16) {
				memset(audio_record->pcm_buff, 0, audio_record->pcm_buff_size);
				stream_write(audio_record->audio_stream, audio_record->pcm_buff, 256);
				stream_write(audio_record->audio_stream, audio_record->pcm_buff, 256);
				stream_write(audio_record->audio_stream, audio_record->pcm_buff, 208);
			}
		}
	}

	SYS_LOG_INF("record(%d,%d,%d,%d,%d,%d,%d,%d,%d,%d)\n",
				audio_record->stream_type,
				audio_record->audio_format,
				audio_record->audio_mode,
				audio_record->channel_type,
				audio_record->channel_id,
				audio_record->channel_mode,
				audio_record->sample_rate,
				audio_record->output_sample_rate,
				audio_record->volume,
				audio_record->pcm_buff_size);

	SYS_LOG_INF("audio_stream:%p\n", audio_record->audio_stream);

	return audio_record;

err_exit:
	if (audio_record->pcm_buff) {
		mem_free(audio_record->pcm_buff);
	}

	if (audio_record->timeline) {
		timeline_release(audio_record->timeline);
	}

	mem_free(audio_record);

	return NULL;
}

int audio_record_destory(struct audio_record_t *handle)
{
	if (!handle)
		return -EINVAL;

	if (audio_system_unregister_record(handle)) {
		SYS_LOG_ERR(" audio_system_unregisty_record failed ");
		SYS_EVENT_ERR(EVENT_AUDIO_ERROR_TYPE, 'r', __LINE__);
		return -ESRCH;
	}

	if (handle->timeline) {
		timeline_stop(handle->timeline);
		timeline_release(handle->timeline);
	}

	if (handle->audio_handle)
		hal_ain_channel_close(handle->audio_handle);

	if (handle->audio_stream)
		stream_destroy(handle->audio_stream);

	if (handle->pcm_buff) {
		mem_free(handle->pcm_buff);
		handle->pcm_buff = NULL;
	}

	mem_free(handle);

	SYS_LOG_INF(" handle: %p ok ", handle);

	return 0;
}

int audio_record_start(struct audio_record_t *handle)
{
	if (!handle)
		return -EINVAL;

	if (!handle->started){
		hal_ain_channel_read_data(handle->audio_handle, handle->pcm_buff, handle->pcm_buff_size);

		hal_ain_channel_start(handle->audio_handle);

		handle->started = true;
	}

	return 0;
}

int audio_record_stop(struct audio_record_t *handle)
{
	if (!handle)
		return -EINVAL;

	if (handle->audio_handle)
		hal_ain_channel_stop(handle->audio_handle);

	if (handle->audio_stream)
		stream_close(handle->audio_stream);

	return 0;
}

int audio_record_pause(struct audio_record_t *handle)
{
	if (!handle)
		return -EINVAL;

	handle->paused = 1;

	return 0;
}

int audio_record_resume(struct audio_record_t *handle)
{
	if (!handle)
		return -EINVAL;

	handle->paused = 0;

	return 0;
}

int audio_record_read(struct audio_record_t *handle, uint8_t *buff, int num)
{
	if (!handle->audio_stream)
		return 0;

	return stream_read(handle->audio_stream, buff, num);
}

int audio_record_flush(struct audio_record_t *handle)
{
	if (!handle)
		return -EINVAL;

	/**TODO: wanghui wait record data empty*/

	return 0;
}

int audio_record_channel_set_input(struct audio_record_t *handle)
{
	int interleave_mode = 0;
	if (!handle)
		return -EINVAL;

	interleave_mode = audio_policy_get_record_interleave_mode(handle->stream_type);

	hal_ain_channel_set_interleave_mode(handle->audio_handle, interleave_mode);

	return 0;
}

int audio_record_set_volume(struct audio_record_t *handle, int volume)
{
	adc_gain gain;
	uint8_t i;

	if (!handle)
		return -EINVAL;

	handle->volume = volume;

	for (i = 0; i < ADC_CH_NUM_MAX; i++) {
		gain.ch_gain[i] = volume;
	}

	SYS_LOG_INF("%d", volume);
	hal_ain_channel_set_volume(handle->audio_handle, &gain);


	return 0;
}

int audio_record_get_volume(struct audio_record_t *handle)
{
	if (!handle)
		return -EINVAL;

	return handle->volume;
}

int audio_record_set_sample_rate(struct audio_record_t *handle, int sample_rate)
{
	if (!handle)
		return -EINVAL;

	handle->sample_rate = sample_rate;

	return 0;
}

int audio_record_get_sample_rate(struct audio_record_t *handle)
{
	if (!handle)
		return -EINVAL;

	return handle->sample_rate;
}

io_stream_t audio_record_get_stream(struct audio_record_t *handle)
{
	if (!handle)
		return NULL;

	return handle->audio_stream;
}

int audio_record_set_waitto_start(struct audio_record_t *handle, bool wait)
{
	assert(handle);

	handle->waitto_start = wait ? 1 : 0;
	return 0;
}

int audio_record_is_waitto_start(struct audio_record_t *handle)
{
	if (!handle)
		return 0;

	return handle->waitto_start;
}

#include <dma.h>

uint64_t audio_record_get_samples_cnt(struct audio_record_t *handle)
{
#if 0
	int ret_val;
	dma_runtime_info_t prev_info;
	dma_runtime_info_t info;
	u64_t dma_total_bytes;

	if (!handle)
		return 0;

	if(!handle->started){
		return 0;
	}

	while(1){
		ret_val = hal_ain_channel_get_sample_cnt(handle->audio_handle, (void *)&prev_info);
		if (ret_val >= 0){
			break;
		}
	}


	while (1){
		ret_val = hal_ain_channel_get_sample_cnt(handle->audio_handle, (void *)&info);
		if (ret_val >= 0){
			if(info.dma_remainlen != prev_info.dma_remainlen){
				break;
			}
		}
	}

	//check if dma reload count overflow
	if (handle->prev_reload_cnt > info.dma_reload_count){
		handle->dma_overflow_bytes += 0x10000 * info.dma_framelen;
	}

	dma_total_bytes = (info.dma_reload_count + 1) * info.dma_framelen - info.dma_remainlen;

	handle->prev_reload_cnt = info.dma_reload_count;

	printk("dma reload cnt %d remain %d bytes %d\n", info->dma_reload_count, info.dma_remainlen, (uint32_t)dma_total_bytes);

	return (dma_total_bytes + handle->dma_overflow_bytes) / handle->frame_size;
#else
	return handle->input_samples;
#endif
}

int audio_record_set_reference_stream(struct audio_record_t *handle, void *ref_stream)
{
	if (!handle)
		return -EINVAL;

	handle->ref_stream = ref_stream;

	return 0;
}
