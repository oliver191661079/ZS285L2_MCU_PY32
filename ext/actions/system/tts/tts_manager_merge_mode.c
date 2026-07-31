/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
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
#include <audio_policy.h>
#include <sdfs.h>
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

/**
 * clamp_t - return a value clamped to a given range using a given type
 * @type: the type of variable to use
 * @val: current value
 * @min: minimum allowable value
 * @max: maximum allowable value
 *
 * This macro does no typechecking and uses temporary variables of type
 * 'type' to make all the comparisons.
 */
#define clamp_t(type, val, min, max) ({		\
	type __val = (val);			\
	type __min = (min);			\
	type __max = (max);			\
	__val = __val < __min ? __min: __val;	\
	__val > __max ? __max: __val; })

struct tts_manager_merge_ctx_t
{
	uint8_t running;
	uint8_t sample_rate;
	uint16_t next_read_offset;
	uint16_t total_len;
	int16_t vol;
	uint8_t *data;
	os_mutex tts_merge_mutex;
	void (*stop_cb)(void);

	uint16_t channel;
};

#define TTS_MERGE_MANAGER_SAMPLE_RATE          (48)
#define TTS_MERGE_MANAGER_MAX_MIX_VOLUME_LEVEL (17)

static struct tts_manager_merge_ctx_t g_tts_manager_merge_ctx;

//mix gain0~17
static const short mix_volume_table[TTS_MERGE_MANAGER_MAX_MIX_VOLUME_LEVEL] = {
        0,  4330,  4860,  6113, 7684,
     9657, 12137, 15254, 19172,
    22068, 23262, 24521, 25848,
    27247, 28722, 30277, 31916,
};

static struct tts_manager_merge_ctx_t *tts_merge_manager_get_ctx(void)
{
	return &g_tts_manager_merge_ctx;
}

static void tts_merge_manager_mix_data(int *src_buf0, short *src_buf1, int *track_buf, uint32_t src_len, int gain, short ch)
{
	uint32_t i = 0;
	int temp_data;

	gain &= 0xffff;

	for (i = 0; i < (src_len / (4*ch)); i++){
		temp_data = (short)((src_buf1[i]  * gain) >> 15);
		//temp_data = (int)clamp_t(int, (src_buf1[i] * (int)gain) >> 14, INT16_MIN, INT16_MAX);

		temp_data <<= 16;
		if (ch > 1){
			track_buf[2 * i] = (temp_data >> 1);
			track_buf[2 * i + 1] = (temp_data >> 1);
		} else {
			track_buf[i] = (temp_data >> 1);
		}
	}

	//print_buffer(track_buf, 4, 16, 4, (uint32_t)src_buf1);

}

static uint32_t tts_merge_manager_mix(struct tts_manager_merge_ctx_t *ctx, uint8_t *buf, uint32_t len)
{
    uint32_t need_len = len;
    int *track_buf = (int *)buf;
    uint32_t cur_size = 0;

    if (ctx->next_read_offset == ctx->total_len){
        return len;
    }

	//printk("mix data idx %d off %x size %d %p\n", len, ctx->next_read_offset, ctx->total_len, ctx->stop_cb);

    if (need_len > ctx->total_len){
        tts_merge_manager_mix_data(track_buf, (int16_t *)ctx->data, track_buf, ctx->total_len, ctx->vol, ctx->channel);
        ctx->next_read_offset = ctx->total_len;
		ctx->total_len = 0;
		if(ctx->stop_cb){
			ctx->stop_cb();
			ctx->stop_cb = NULL;
		}
    }else{
        if ((ctx->total_len - ctx->next_read_offset) >= need_len){
            tts_merge_manager_mix_data(track_buf, (int16_t *)(ctx->data + ctx->next_read_offset), track_buf, need_len, ctx->vol, ctx->channel);
            ctx->next_read_offset += (need_len >> ctx->channel);
        }else{
            cur_size = ctx->total_len - ctx->next_read_offset;
            tts_merge_manager_mix_data(track_buf, (int16_t *)(ctx->data + ctx->next_read_offset), track_buf, cur_size, ctx->vol, ctx->channel);
            ctx->next_read_offset = ctx->total_len;
			ctx->total_len = 0;
			if(ctx->stop_cb){
				ctx->stop_cb();
				ctx->stop_cb = NULL;
			}
        }
    }


    return len;
}


uint32_t tts_merge_manager_request_more_data(uint8_t *buf, uint32_t len)
{
    uint32_t ret_len = len;

	struct tts_manager_merge_ctx_t *ctx = tts_merge_manager_get_ctx();

    if (ctx->running && ctx->total_len){
        ret_len = tts_merge_manager_mix(ctx, buf, len);
    }

    return ret_len;
}


int tts_merge_manager_init(void)
{
	struct tts_manager_merge_ctx_t *ctx = tts_merge_manager_get_ctx();

	memset(ctx, 0, sizeof(struct tts_manager_merge_ctx_t));

	os_mutex_init(&ctx->tts_merge_mutex);

    return 0;
}

int tts_merge_manager_deinit(void)
{
	struct tts_manager_merge_ctx_t *ctx = tts_merge_manager_get_ctx();

	os_mutex_lock(&ctx->tts_merge_mutex, OS_FOREVER);

    ctx->running = false;
    ctx->data = NULL;
    ctx->total_len = 0;
    ctx->next_read_offset = 0;

    os_mutex_unlock(&ctx->tts_merge_mutex);

    return 0;
}

int tts_merge_manager_start(char *name, uint8_t sample_rate, void (*stop_cb)(void))
{
	struct buffer_t buffer;
	struct audio_track_t *track = audio_system_get_track();
	int extra_wait_fade_out_ms = 0;
	struct tts_manager_merge_ctx_t *ctx = tts_merge_manager_get_ctx();
        unsigned int key;

	if(sample_rate != TTS_MERGE_MANAGER_SAMPLE_RATE){
		return -EINVAL;
	}

#ifdef CONFIG_SD_FS
	if (sd_fmap(name, (void **)&buffer.base, &buffer.length) != 0) {
		return -EINVAL;
	}
#endif

	//SYS_LOG_INF("merge tts:%s %p %d %p\n", name, buffer.base, buffer.length, stop_cb);


	media_player_fade_out(media_player_get_current_main_player(), 60);

	// fix JBLFLIP7-2116
	if (track && (track->stream_type == AUDIO_STREAM_LE_AUDIO || track->stream_type == AUDIO_STREAM_BMR)
		&& track->sample_rate && track->frame_size) {
		extra_wait_fade_out_ms = stream_get_length(track->audio_stream) / (track->sample_rate*track->frame_size);
		if (extra_wait_fade_out_ms < 0)
			extra_wait_fade_out_ms = 0;
		SYS_LOG_INF("extra_wait_fade_out_ms:%d\n", extra_wait_fade_out_ms);
	}
	os_sleep(audio_policy_get_bis_link_delay_ms() + 80 + extra_wait_fade_out_ms);

	os_mutex_lock(&ctx->tts_merge_mutex, OS_FOREVER);
	if(track->started == 0 || track->flushed) {
		SYS_LOG_INF("main audio track over\n");
		os_mutex_unlock(&ctx->tts_merge_mutex);
		return 1;
	}
        key = irq_lock();
	ctx->running = true;
	ctx->data = (int8_t *)buffer.base;
	ctx->total_len = buffer.length;
	ctx->next_read_offset = 0;
	ctx->stop_cb = stop_cb;
	if (track) {
		ctx->channel = track->audio_mode == AUDIO_MODE_STEREO ? 2 : 1;
	} else {
		ctx->channel = 2;
	}
        irq_unlock(key);
	os_mutex_unlock(&ctx->tts_merge_mutex);


    return 0;
}

int tts_merge_manager_stop(void)
{
	struct tts_manager_merge_ctx_t *ctx = tts_merge_manager_get_ctx();

	SYS_LOG_INF();

	os_mutex_lock(&ctx->tts_merge_mutex, OS_FOREVER);
    ctx->running = false;
	ctx->next_read_offset = 0;
    os_mutex_unlock(&ctx->tts_merge_mutex);

	media_player_fade_in(media_player_get_current_main_player(), 60);

    return 0;
}

int tts_merge_manager_is_running(void)
{
    int running;
	struct tts_manager_merge_ctx_t *ctx = tts_merge_manager_get_ctx();

	os_mutex_lock(&ctx->tts_merge_mutex, OS_FOREVER);
    running = ctx->running;
    os_mutex_unlock(&ctx->tts_merge_mutex);
    return running;
}

int tts_merge_manager_mix_gain_set(uint32_t volume_level)
{
	struct tts_manager_merge_ctx_t *ctx = tts_merge_manager_get_ctx();

	if(volume_level >= TTS_MERGE_MANAGER_MAX_MIX_VOLUME_LEVEL){
		return -EINVAL;
	}

	os_mutex_lock(&ctx->tts_merge_mutex, OS_FOREVER);
    ctx->vol = mix_volume_table[volume_level];
    os_mutex_unlock(&ctx->tts_merge_mutex);

	return 0;
}

int tts_merge_manager_mix_get_gain(void)
{
	return mix_volume_table[TTS_MERGE_MIX_GAIN_LEVEL];
}

