/*
 * Copyright (c) 2016 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief audio stream.
 */

#include <os_common_api.h>
#include <mem_manager.h>
#include <msg_manager.h>
#include <audio_hal.h>
#include <audio_system.h>
#include <audio_track.h>
#include <audio_record.h>
#include <media_type.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stream.h>
#include <btservice_api.h>
#include "media_service.h"
#ifdef CONFIG_BLUETOOTH
#include <bt_manager.h>
#endif

#define SYS_LOG_NO_NEWLINE
#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "audio_aps"
#include <logging/sys_log.h>

static inline int abs(int i)
{
	return (i >= 0) ? i : -i;
}

#define AUDIO_APS_ADJUST_INTERVAL            30

static pcm_monitor_callback pcm_monitor_cb = NULL;

void audio_aps_monitor_set_aps(aps_monitor_info_t *handle, uint8_t status, int level)
{
	uint8_t aps_mode = APS_LEVEL_AUDIOPLL;
    uint8_t set_aps = false;

    if (status & APS_OPR_SET) {
       handle->dest_level = level;
    }

    if (status & APS_OPR_FAST_SET) {
		handle->dest_level = level;
		handle->current_level = level;
		set_aps = true;
    }

    if (handle->current_level > handle->dest_level) {
		handle->current_level--;
		set_aps = true;
    }
    if (handle->current_level < handle->dest_level) {
		handle->current_level++;
		set_aps = true;
    }

    if (set_aps) {
		if(handle->aps_type & APS_TYPE_HARDWARE_AUDIO){
			void *audio_handle = NULL;
			if(handle->aps_type & APS_TYPE_PLAYBACK){
				struct audio_track_t *audio_track = (struct audio_track_t *)handle->output_handle;
				audio_handle = audio_track->audio_handle;
				hal_aout_channel_set_aps(audio_handle, handle->current_level, aps_mode);
			}else if(handle->aps_type & APS_TYPE_CAPTURE){
				struct audio_record_t *audio_record = (struct audio_record_t *)handle->output_handle;
				audio_handle = audio_record->audio_handle;
				hal_ain_channel_set_aps(audio_handle, handle->current_level, aps_mode);
			}
#ifdef CONFIG_BLUETOOTH
		}else if(handle->aps_type & APS_TYPE_HARDWARE_BT){
			switch(handle->current_level){
				case APS_LEVEL_4:
					bt_manager_audio_stream_tx_adjust_clk(1);
					break;
				case APS_LEVEL_5:
					bt_manager_audio_stream_tx_adjust_clk(0);
					break;
				case APS_LEVEL_6:
					bt_manager_audio_stream_tx_adjust_clk(-1);
					break;
				default:
					break;
			}
#endif
		}else if(handle->aps_type & APS_TYPE_SOFT){
			//printk("<aps>adjust mode %d: %d status:%d dest:%d\n", handle->aps_type, handle->current_level, status, handle->dest_level);
			extern int capture_service_set_parameter(void *handle, media_param_t *param);
			extern int playback_service_set_parameter(void *handle, media_param_t *param);
			media_param_t param;
			param.type = MEDIA_SET_RESAMPLE_APS;
			level = handle->current_level;
			param.pbuf = (void*)(level);
			if(handle->aps_type & APS_TYPE_PLAYBACK){
#ifndef CONFIG_SOUNDBAR_SOURCE_BR
				playback_service_set_parameter(handle->output_handle, &param);
#endif
			}else if(handle->aps_type & APS_TYPE_CAPTURE){
				capture_service_set_parameter(handle->output_handle, &param);
			}
		}
    }
}

uint16_t audio_aps_monitor_normal(aps_monitor_info_t *handle, int stream_length, uint8_t aps_max_level, uint8_t aps_min_level, uint8_t aps_level)
{
	uint16_t mid_threshold = 0;
	uint16_t diff_threshold = 0;

	if (!handle->need_aps) {
		return 0;
	}

	handle->aps_increase_water_mark = audio_policy_get_increase_threshold(handle->format, handle->stream_type);
	handle->aps_reduce_water_mark = audio_policy_get_reduce_threshold(handle->format, handle->stream_type);
	diff_threshold = (handle->aps_increase_water_mark - handle->aps_reduce_water_mark);

	if(handle->aps_type & APS_TYPE_SOFT){
		mid_threshold = handle->aps_increase_water_mark - (diff_threshold / 5 * 2);
	}else
		mid_threshold = handle->aps_increase_water_mark - (diff_threshold / 2);

    switch (handle->aps_status) {
	case APS_STATUS_DEFAULT:
	    if (stream_length > handle->aps_increase_water_mark) {
			if (handle->aps_type & APS_TYPE_SOFT) {
				SYS_LOG_DBG("dec aps\n");
				audio_aps_monitor_set_aps(handle, APS_OPR_FAST_SET, aps_min_level);
				handle->aps_status = APS_STATUS_DEC;
			}
			else {
				SYS_LOG_DBG("inc aps\n");
				audio_aps_monitor_set_aps(handle, APS_OPR_FAST_SET, aps_max_level);
				handle->aps_status = APS_STATUS_INC;
			}
	    } else if (stream_length < handle->aps_reduce_water_mark) {
			if (handle->aps_type & APS_TYPE_SOFT) {
				SYS_LOG_DBG("fast inc aps\n");
				audio_aps_monitor_set_aps(handle, APS_OPR_FAST_SET, aps_max_level);
				handle->aps_status = APS_STATUS_INC;
			}
			else {
				SYS_LOG_DBG("fast dec aps\n");
				audio_aps_monitor_set_aps(handle, APS_OPR_FAST_SET, aps_min_level);
				handle->aps_status = APS_STATUS_DEC;
			}
	    } else {
			SYS_LOG_DBG("APS_STATUS_DEFAULT APS_OPR_ADJUST aps\n");
			audio_aps_monitor_set_aps(handle, APS_OPR_ADJUST, 0);
	    }
	    break;

	case APS_STATUS_INC:
		if (handle->aps_type & APS_TYPE_SOFT){
			if (stream_length > handle->aps_increase_water_mark) {
				SYS_LOG_DBG("fast dec aps\n");
				audio_aps_monitor_set_aps(handle, APS_OPR_FAST_SET, aps_min_level);
				handle->aps_status = APS_STATUS_DEC;
			} else if (stream_length >= mid_threshold) {
				SYS_LOG_DBG("default aps\n");
				audio_aps_monitor_set_aps(handle, APS_OPR_SET, aps_level);
				handle->aps_status = APS_STATUS_DEFAULT;
			} else {
				SYS_LOG_DBG("APS_STATUS_INC APS_OPR_ADJUST aps\n");
				audio_aps_monitor_set_aps(handle, APS_OPR_ADJUST, 0);
			}
		}else {
			if (stream_length < handle->aps_reduce_water_mark) {
				SYS_LOG_DBG("fast dec aps\n");
				audio_aps_monitor_set_aps(handle, APS_OPR_FAST_SET, aps_min_level);
				handle->aps_status = APS_STATUS_DEC;
			} else if (stream_length <= mid_threshold) {
				SYS_LOG_DBG("default aps\n");
				audio_aps_monitor_set_aps(handle, APS_OPR_SET, aps_level);
				handle->aps_status = APS_STATUS_DEFAULT;
			} else {
				audio_aps_monitor_set_aps(handle, APS_OPR_ADJUST, 0);
			}
		}
	    break;

	case APS_STATUS_DEC:
		if (handle->aps_type & APS_TYPE_SOFT) {
			if (stream_length < handle->aps_reduce_water_mark) {
				SYS_LOG_DBG("fast inc aps\n");
				audio_aps_monitor_set_aps(handle, APS_OPR_FAST_SET, aps_max_level);
				handle->aps_status = APS_STATUS_INC;
			} else if (stream_length <= mid_threshold) {
				SYS_LOG_DBG("default aps\n");
				audio_aps_monitor_set_aps(handle, APS_OPR_SET, aps_level);
				handle->aps_status = APS_STATUS_DEFAULT;
			} else {
				SYS_LOG_DBG("APS_STATUS_DEC APS_OPR_ADJUST aps\n");
				audio_aps_monitor_set_aps(handle, APS_OPR_ADJUST, 0);
			}
		} else {
			if (stream_length > handle->aps_increase_water_mark) {
				SYS_LOG_DBG("fast inc aps\n");
				audio_aps_monitor_set_aps(handle, APS_OPR_FAST_SET, aps_max_level);
				handle->aps_status = APS_STATUS_INC;
			} else if (stream_length >= mid_threshold) {
				SYS_LOG_DBG("default aps\n");
				audio_aps_monitor_set_aps(handle, APS_OPR_SET, aps_level);
				handle->aps_status = APS_STATUS_DEFAULT;
			} else {
				audio_aps_monitor_set_aps(handle, APS_OPR_ADJUST, 0);
			}
		}
	    break;
    }

    return mid_threshold;
}
extern void audio_aps_monitor_slave(aps_monitor_info_t *handle, int stream_length, uint8_t aps_max_level, uint8_t aps_min_level, uint8_t slave_aps_level);
extern void audio_aps_monitor_master(aps_monitor_info_t *handle, int stream_length, uint8_t aps_max_level, uint8_t aps_min_level, uint8_t slave_aps_level);

extern int playback_service_get_cached_framenum(void *handle);
extern int capture_service_get_cached_framenum(void *handle);



/* Run interval DATA_PROCESS_PERIOD =  (4) */
int audio_aps_monitor(void *aps_handle)
{
	int pcm_time = 0;
	uint16_t normal_level = APS_LEVEL_4;
	aps_monitor_info_t *handle = (aps_monitor_info_t *)aps_handle;
	/* Adjust aps every AUDIO_APS_ADJUST_INTERVAL */
	if ((k_uptime_get_32() - handle->aps_time) < handle->duration) {
		return 0;
	}

	handle->aps_time = k_uptime_get_32();

	if(handle->aps_type & APS_TYPE_PLAYBACK){
		pcm_time = playback_service_get_cached_framenum(handle->input_handle);
		// printk("<aps>p pcm:%d\n", pcm_time);
//		if(pcm_monitor_cb){
//		    pcm_monitor_cb(pcm_time);
//		}
	} else if(handle->aps_type & APS_TYPE_CAPTURE){
		pcm_time = capture_service_get_cached_framenum(handle->input_handle);
		// printk("<aps>c pcm:%d\n", pcm_time);
	}

#ifdef CONFIG_TWS
	if (handle->role == BTSRV_TWS_NONE) {
		normal_level = audio_aps_monitor_normal(handle, pcm_time, handle->aps_max_level, handle->aps_min_level, handle->aps_default_level);
	} else if (handle->role == BTSRV_TWS_MASTER) {
		audio_aps_monitor_master(handle, pcm_time, handle->aps_max_level, handle->aps_min_level, handle->aps_default_level);
	} else if (handle->role == BTSRV_TWS_SLAVE) {
		audio_aps_monitor_slave(handle, pcm_time, handle->aps_max_level, handle->aps_min_level, handle->current_level);
	}
#else
	normal_level = audio_aps_monitor_normal(handle, pcm_time, handle->aps_max_level, handle->aps_min_level, handle->aps_default_level);
#endif

	if(handle->aps_type & APS_TYPE_PLAYBACK){
		if(pcm_monitor_cb){
		    if(handle->aps_type & APS_TYPE_SOFT){
		        if(handle->aps_status == APS_STATUS_INC){
		            pcm_monitor_cb(pcm_time,normal_level,0);
		        }
		        else{
		            pcm_monitor_cb(pcm_time,normal_level,1);
		        }
		    }
		    else{
		        if(handle->aps_status == APS_STATUS_DEC){
		            pcm_monitor_cb(pcm_time,normal_level,0);
		        }
		        else{
		            pcm_monitor_cb(pcm_time,normal_level,1);
		        }
		    }
		}
    }
	return 0;
}

inline static unsigned int _get_fifo_cnt(int channel_type)
{
	if (channel_type == AUDIO_CHANNEL_I2SRX1)
		return (sys_read32(AUDIO_I2SRX1_REG_BASE + 8)&0x1f);
	else if(channel_type == AUDIO_CHANNEL_I2SRX)
		return (sys_read32(AUDIO_I2SRX0_REG_BASE + 8)&0x1f);
	else if(channel_type == AUDIO_CHANNEL_ADC)
		return (sys_read32(AUDIO_ADC_REG_BASE + 8)&0x1f);
	else
		return 0;
}

int audio_aps_bt_clock_monitor(void *handle)
{
#ifdef CONFIG_BLUETOOTH
	int64_t bt_diff = 0;
	uint64_t local_time = 0;
	int base_count = 0;
	aps_monitor_info_t *aps_handle = (aps_monitor_info_t *)handle;
	int interval_us = timeline_get_interval(aps_handle->timeline);
	int bt_interval;
	int is_playback = !!(aps_handle->aps_type & APS_TYPE_PLAYBACK);
	uint8_t level = aps_handle->current_level;
	struct audio_record_t *audio_record;
	uint32_t irq_count;
	int pre_fifo_cnt,cur_fifo_cnt;
	int dma_len = 0,cur_dma_len = 0;

	base_count = audio_policy_get_nav_frame_size_us() / interval_us;
	bt_interval = audio_policy_get_nav_frame_size_us();

	if(is_playback){
		return 0;
	} else {
		audio_record = (struct audio_record_t *)aps_handle->input_handle;

		if (audio_record->stream_type != AUDIO_STREAM_SOUNDBAR) {
			aps_handle->aps_count = 0;
			return 0;
		}
	}
	int sample_rate = audio_record_get_sample_rate(audio_record) * 10;

	if(sample_rate == 440)
		sample_rate = 441;
	//to do mono channel?
	dma_len = sample_rate * interval_us * 2 * 2 *2 / 10000;

	aps_handle->aps_count++;
	if(aps_handle->aps_count <= 1){
		return 0;
	}else if(aps_handle->aps_count <= 2){
		do{
			irq_count = _arch_irq_get_irq_count();
			pre_fifo_cnt = _get_fifo_cnt(audio_record->channel_type);
			while(pre_fifo_cnt == (cur_fifo_cnt = _get_fifo_cnt(audio_record->channel_type)));
			local_time = timeline_get_current_time(aps_handle->ref_timeline);
			//cur_dma_len = hal_ain_get_dma_len(audio_record->audio_handle);
			cur_dma_len = dma_len;//To do get current remain len
		}while(irq_count != _arch_irq_get_irq_count());

		aps_handle->ref_diff = (int64_t)(local_time%bt_interval) - (dma_len - cur_dma_len)* 10000 /(sample_rate * 2 * 2);
		aps_handle->last_frame_count = cur_fifo_cnt;
		audio_aps_monitor_set_aps(handle, APS_OPR_FAST_SET, APS_LEVEL_5);
		printk("<aps>record first %d %u us %d %d\n",(int32_t)aps_handle->ref_diff,(uint32_t)local_time%bt_interval,cur_dma_len,cur_fifo_cnt);
	}else{
		if((base_count == 1 ) || (aps_handle->aps_count % base_count == 2)
			|| (base_count == 2  && aps_handle->aps_count % base_count == 0)){
			do{
				irq_count = _arch_irq_get_irq_count();
				pre_fifo_cnt = _get_fifo_cnt(audio_record->channel_type);
				while(pre_fifo_cnt == (cur_fifo_cnt = _get_fifo_cnt(audio_record->channel_type)));
				local_time = timeline_get_current_time(aps_handle->ref_timeline);
				//cur_dma_len = hal_ain_get_dma_len(audio_record->audio_handle);
				cur_dma_len = dma_len;
			}while(irq_count != _arch_irq_get_irq_count());

			bt_diff = (int64_t)(local_time%bt_interval) - (dma_len - cur_dma_len)* 10000 /(sample_rate * 2 * 2)
				- (int64_t)(cur_fifo_cnt - (int)aps_handle->last_frame_count)* 10000 / sample_rate;
			aps_handle->last_audio_time = bt_diff;
			//if(aps_handle->aps_count % 100 == 2){
			//	printk("<aps>record diff %lld %lld us %d %d %d\n",bt_diff,(int64_t)local_time%bt_interval,base_count,cur_fifo_cnt,level);
			//}

			if (bt_diff > aps_handle->ref_diff + 2) {
				//to low
				level--;
				if (level < APS_LEVEL_4) {
					level = APS_LEVEL_4;
				}
				audio_aps_monitor_set_aps(handle, APS_OPR_FAST_SET, level);
			} else if (bt_diff < aps_handle->ref_diff - 2){
				//to quick
				level++;
				if(level > APS_LEVEL_6){
					level = APS_LEVEL_6;
				}
				audio_aps_monitor_set_aps(handle, APS_OPR_FAST_SET, level);
			} else {
				level = APS_LEVEL_5;
				audio_aps_monitor_set_aps(handle, APS_OPR_FAST_SET, level);
			}
		}
	}

#endif

	return 0;
}

#ifdef CONFIG_BLUETOOTH
static void aps_timer_handler(struct hrtimer *timer, void* pdata)
{
   audio_aps_monitor_set_aps(pdata, APS_OPR_FAST_SET, APS_LEVEL_5);
   //printk("<aps>reset aps\n");
}
#endif

int audio_aps_clock_monitor(void *handle)
{
#ifdef CONFIG_BLUETOOTH
	int64_t bt_diff = 0;
	uint64_t local_time = 0;
	uint64_t base_count = 0;
	aps_monitor_info_t *aps_handle = (aps_monitor_info_t *)handle;
	uint8_t level = aps_handle->current_level;
	int interval_us = timeline_get_interval(aps_handle->timeline);
	int bt_interval;
	uint32_t cur_frame_count;
	uint32_t pre_frame_count;
	int frame_count;
	uint32_t irq_count;

	base_count = audio_policy_get_nav_frame_size_us() / interval_us;
	bt_interval = audio_policy_get_nav_frame_size_us();
	frame_count = 48 * audio_policy_get_nav_frame_size_us() * 2 / 1000;
	//TODO : add support for capture
	if(aps_handle->aps_type & APS_TYPE_PLAYBACK){
		struct audio_track_t *audio_track = (struct audio_track_t *)aps_handle->input_handle;
		if (audio_track->stream_type == AUDIO_STREAM_LE_AUDIO && audio_track->started ) {
			aps_handle->aps_count++;
			if(aps_handle->aps_count <= 1){
				return 0;
			}else if(aps_handle->aps_count <= 2){
				do{
					irq_count = _arch_irq_get_irq_count();
					pre_frame_count = hal_aout_channel_get_sample_cnt(audio_track->audio_handle);
					while(pre_frame_count == (cur_frame_count = hal_aout_channel_get_sample_cnt(audio_track->audio_handle)));
					local_time = timeline_get_current_time(aps_handle->ref_timeline);
				}while(irq_count != _arch_irq_get_irq_count());
				aps_handle->ref_diff = (int64_t)(local_time%bt_interval);
				aps_handle->last_audio_time = local_time;
				aps_handle->last_frame_count = cur_frame_count;
				printk("<aps>first %d us %u\n",(int32_t)aps_handle->ref_diff,cur_frame_count);
				audio_aps_monitor_set_aps(handle, APS_OPR_FAST_SET, APS_LEVEL_5);
			}else{
				if((base_count == 1) || (aps_handle->aps_count % base_count == 2)
					|| (base_count == 2  && aps_handle->aps_count % base_count == 0)){
					do{
						irq_count = _arch_irq_get_irq_count();
						pre_frame_count = hal_aout_channel_get_sample_cnt(audio_track->audio_handle);
						if(pre_frame_count < aps_handle->last_frame_count){
							printk("<aps>frame count overflow1\n");
							aps_handle->aps_count = 0;
							aps_handle->ref_diff = 0;
							aps_handle->last_audio_time = 0;
							aps_handle->last_frame_count = 0;
							return 0;
						}
						while(pre_frame_count == (cur_frame_count = hal_aout_channel_get_sample_cnt(audio_track->audio_handle)));
						local_time = timeline_get_current_time(aps_handle->ref_timeline);
					}while(irq_count != _arch_irq_get_irq_count());

					int64_t diff_frame_cnt = (int)(cur_frame_count - (uint32_t)aps_handle->last_frame_count) % frame_count;
					if(diff_frame_cnt > frame_count - 50)
						diff_frame_cnt -= frame_count;

					bt_diff = local_time%bt_interval - diff_frame_cnt * 1000 / 96;
					aps_handle->last_audio_time = bt_diff;

					if (bt_diff > aps_handle->ref_diff + 2) {
						//to quick
						level++;
						if (level <= APS_LEVEL_6) {
							audio_aps_monitor_set_aps(handle, APS_OPR_FAST_SET, level);
							if(level == APS_LEVEL_6 && bt_interval == 10000){
#ifdef CONFIG_ACTS_HRTIMER
								hrtimer_init(&aps_handle->timer, aps_timer_handler, (void *)aps_handle);
								hrtimer_start(&aps_handle->timer, 2500, 0);
#endif
							}
						}
					} else if (bt_diff < aps_handle->ref_diff - 2){
						//to low
						level--;
						if (level >= APS_LEVEL_4) {
							audio_aps_monitor_set_aps(handle, APS_OPR_FAST_SET, level);
							if(level == APS_LEVEL_4 && bt_interval == 10000){
#ifdef CONFIG_ACTS_HRTIMER
								hrtimer_init(&aps_handle->timer, aps_timer_handler, (void *)aps_handle);
								hrtimer_start(&aps_handle->timer, 5000, 0);
#endif
							}
						}
					} else {
						level = APS_LEVEL_5;
						audio_aps_monitor_set_aps(handle, APS_OPR_FAST_SET, level);
					}
					//if(audio_track->count % 1000 == 0)
					//	printk("<aps>diff %d %u us %u %d\n",(int32_t)bt_diff,(uint32_t)local_time%bt_interval,cur_frame_count,sys_read32(0xc000010c)&0xf);
				}
			}
		}else{
			aps_handle->aps_count= 0;
		}
	}

#endif

	return 0;
}

__ramfunc void audio_aps_get_bt_audio_snapshot(int handle, int64_t *bt_time, uint64_t *audio_sample_cout)
{
	uint32_t irq_count;
	uint64_t cur_frame_count;
	uint64_t pre_frame_count;
	int64_t local_time = 0;
	u32_t fifo_cnt_reg = AUDIO_DAC_REG_BASE + 0x28; //DAC_FIFO0_CNT
#ifdef CONFIG_AUDIO_OUTPUT_I2S
	fifo_cnt_reg = AUDIO_I2STX0_REG_BASE + 0x20; //I2STX0_FIFO_CNT
#endif
	if(handle < 0)
		return;
	do{
		irq_count = _arch_irq_get_irq_count();
		pre_frame_count = sys_read32(fifo_cnt_reg)&0xffff;
		do {
			cur_frame_count = sys_read32(fifo_cnt_reg)&0xffff;
		}
		while(pre_frame_count == cur_frame_count);
		local_time = bt_manager_audio_get_le_time((uint16_t)handle);
	}while(irq_count != _arch_irq_get_irq_count());

	*bt_time = local_time;
	*audio_sample_cout = cur_frame_count;
}

int audio_aps_soft_resample_by_interrupt_monitor(void *handle)
{
#ifdef CONFIG_BLUETOOTH
	int64_t bt_diff = 0;
	int64_t local_time = 0;
	uint64_t base_count = 0;
	aps_monitor_info_t *aps_handle = (aps_monitor_info_t *)handle;
	int interval_us = timeline_get_interval(aps_handle->timeline);
	uint64_t cur_frame_count;
	uint32_t frame_count;
	int bt_handle = 0;
	uint32_t sample_rate = 48;
	struct audio_track_t *audio_track = NULL;
	struct audio_record_t *audio_record = NULL;
	extern int playback_service_set_parameter(void *handle, media_param_t *param);
	extern int capture_service_set_parameter(void *handle, media_param_t *param);

	media_param_t param;
	param.type = MEDIA_SET_RESAMPLE_APS_BT_TIME;

	if(aps_handle->aps_type & APS_TYPE_PLAYBACK){
		audio_track = (struct audio_track_t *)aps_handle->input_handle;
		sample_rate = audio_track->output_sample_rate;
		if(aps_handle->device_info.rx_chan.type == MEDIA_AUDIO_DEVICE_TYPE_BIS){
			bt_handle = bt_manager_broadcast_get_audio_handle(aps_handle->device_info.rx_chan.bis_chan.handle,
				aps_handle->device_info.rx_chan.bis_chan.id);
		}else if(aps_handle->device_info.rx_chan.type == MEDIA_AUDIO_DEVICE_TYPE_CIS){
			bt_handle = bt_manager_audio_get_audio_handle(aps_handle->device_info.rx_chan.bt_chan.handle,
				aps_handle->device_info.rx_chan.bt_chan.id);
		} else if (aps_handle->device_info.tx_chan.type == MEDIA_AUDIO_DEVICE_TYPE_BIS) {
			bt_handle = bt_manager_broadcast_get_audio_handle(aps_handle->device_info.tx_chan.bis_chan.handle,
				aps_handle->device_info.tx_chan.bis_chan.id);
				param.type = MEDIA_SET_RESAMPLE_APS_BT_TIME2;
		}
	} else {
		audio_record = (struct audio_record_t *)aps_handle->input_handle;
		sample_rate = audio_record->sample_rate;
		if(aps_handle->device_info.tx_chan.type == MEDIA_AUDIO_DEVICE_TYPE_BIS){
			bt_handle = bt_manager_broadcast_get_audio_handle(aps_handle->device_info.tx_chan.bis_chan.handle,
				aps_handle->device_info.tx_chan.bis_chan.id);
		}else if(aps_handle->device_info.tx_chan.type == MEDIA_AUDIO_DEVICE_TYPE_CIS){
			bt_handle = bt_manager_audio_get_audio_handle(aps_handle->device_info.tx_chan.bt_chan.handle,
				aps_handle->device_info.tx_chan.bt_chan.id);
		}
	}
	base_count = audio_policy_get_nav_frame_size_us() / interval_us;
	frame_count = sample_rate * audio_policy_get_nav_frame_size_us() * 2 / 1000;

	if(aps_handle->aps_type & APS_TYPE_PLAYBACK){
		if (//audio_track->stream_type == AUDIO_STREAM_LE_AUDIO &&
			audio_track->started ) {
			aps_handle->aps_count++;
			if(aps_handle->aps_count <= 1){
				return 0;
			}else if(aps_handle->aps_count <= 2){
				audio_aps_get_bt_audio_snapshot(bt_handle,&local_time, &cur_frame_count);
				cur_frame_count = (hal_aout_channel_get_sample_cnt(audio_track->audio_handle)&0xffff0000) + cur_frame_count;
				if(local_time < 0 || bt_handle < 0){
					printk("<aps>get current time failed %d %d\n",(int32_t)local_time,bt_handle);
					return 0;
				}

				aps_handle->last_audio_time = local_time - aps_handle->ref_diff;
				bt_diff = aps_handle->ref_diff;
				aps_handle->first_frame_count = cur_frame_count;
				aps_handle->last_frame_count = cur_frame_count;
				printk("<aps>first %lld %d us %llu\n",aps_handle->last_audio_time,(int32_t)aps_handle->ref_diff,
					cur_frame_count);
				param.pbuf = (void*)(&bt_diff);
				param.plen = sizeof(bt_diff);
				playback_service_set_parameter(aps_handle->output_handle, &param);
			}else{
				if((base_count == 1) || (aps_handle->aps_count % base_count == 2)
					|| (base_count == 2  && aps_handle->aps_count % base_count == 0)){
					audio_aps_get_bt_audio_snapshot(bt_handle,&local_time, &cur_frame_count);
					cur_frame_count = (hal_aout_channel_get_sample_cnt(audio_track->audio_handle)&0xffff0000) + cur_frame_count;
					if (cur_frame_count > aps_handle->last_frame_count + frame_count * 2) {
						printk("<aps>fix frame count:%lld %lld\n", cur_frame_count, aps_handle->last_frame_count);
						cur_frame_count = cur_frame_count - 0x10000;
					}
					if(cur_frame_count < aps_handle->last_frame_count){
						printk("<aps>frame count overflow2\n");
						aps_handle->aps_count = 0;
						return 0;
					}
					if(local_time < 0 || bt_handle < 0)
						return 0;

					int64_t diff_frame_cnt = (int)((uint32_t)cur_frame_count - (uint32_t)aps_handle->first_frame_count);
					bt_diff = local_time - aps_handle->last_audio_time - diff_frame_cnt * 1000 / sample_rate / 2;
					if(abs(bt_diff - aps_handle->ref_diff) > audio_policy_get_nav_frame_size_us() / 100){
						printk("<aps>invalidate %d\n",abs(bt_diff - aps_handle->ref_diff));
						return 0;
					}
					aps_handle->ref_diff = bt_diff;
					aps_handle->last_frame_count = cur_frame_count;
					param.pbuf = (void*)(&aps_handle->ref_diff);
					param.plen = sizeof(aps_handle->ref_diff);
					playback_service_set_parameter(aps_handle->output_handle, &param);
					//if(aps_handle->aps_count % 100 == 0)
					//	printk("<aps>diff %lld %lld us %d %d\n",bt_diff,local_time,cur_frame_count,aps_handle->current_level);
				}
			}
		}else{
			aps_handle->aps_count = 0;
			aps_handle->ref_diff = 0;
		}
	}
#if 0
	else if(aps_handle->aps_type & APS_TYPE_CAPTURE){
		if (audio_record->stream_type == AUDIO_STREAM_LE_AUDIO) {
			aps_handle->aps_count++;
			if(aps_handle->aps_count <= 1){
				return 0;
			}else if(aps_handle->aps_count <= 2){
				do{
					irq_count = _arch_irq_get_irq_count();
					pre_frame_count = _get_fifo_cnt(audio_record->channel_type);
					while(pre_frame_count == (cur_frame_count = _get_fifo_cnt(audio_record->channel_type)));
					local_time = timeline_get_current_time(aps_handle->ref_timeline);
					if(local_time < 0)
						return 0;
				}while(irq_count != _arch_irq_get_irq_count());

				aps_handle->last_audio_time = local_time - aps_handle->ref_diff;
				bt_diff = aps_handle->ref_diff;
				aps_handle->first_frame_count = cur_frame_count;
				aps_handle->last_frame_count = cur_frame_count;
				printk("<aps>first %lld %d us %llu\n",aps_handle->last_audio_time,(int32_t)aps_handle->ref_diff,cur_frame_count);
				param.pbuf = (void*)(&bt_diff);
				param.plen = sizeof(bt_diff);
				capture_service_set_parameter(aps_handle->output_handle, &param);
			}else{
				if((base_count == 1) || (aps_handle->aps_count % base_count == 2)
					|| (base_count == 2  && aps_handle->aps_count % base_count == 0)){
					do{
						irq_count = _arch_irq_get_irq_count();
						pre_frame_count = _get_fifo_cnt(audio_record->channel_type);
						while(pre_frame_count == (cur_frame_count = _get_fifo_cnt(audio_record->channel_type)));
						local_time = timeline_get_current_time(aps_handle->ref_timeline);
						if(local_time < 0)
							return 0;
					}while(irq_count != _arch_irq_get_irq_count());

					cur_frame_count = aps_handle->last_frame_count + frame_count;
					int64_t diff_frame_cnt = cur_frame_count - aps_handle->first_frame_count;
					bt_diff = local_time - aps_handle->last_audio_time - diff_frame_cnt * 1000 / sample_rate / 2;
					if(abs(bt_diff - aps_handle->ref_diff) > audio_policy_get_nav_frame_size_us() / 100){
						printk("<aps>invalidate %d\n",abs(bt_diff - aps_handle->ref_diff));
						return 0;
					}
					aps_handle->ref_diff = bt_diff;
					aps_handle->last_frame_count = cur_frame_count;
					param.pbuf = (void*)(&aps_handle->ref_diff);
					param.plen = sizeof(aps_handle->ref_diff);
					capture_service_set_parameter(aps_handle->output_handle, &param);
					//if(aps_handle->aps_count % 100 == 0)
					//	printk("<aps>diff %lld %lld us %lld %d\n",bt_diff,local_time,cur_frame_count,aps_handle->current_level);
				}
			}
		}

		else{
			aps_handle->aps_count = 0;
			aps_handle->ref_diff = 0;
		}
	}

#endif
#endif
	return 0;
}

extern uint32_t get_sample_rate_hz(uint8_t fs_khz);
extern int playback_service_set_parameter_by_track(void *track_handle, media_param_t *param);

int audio_aps_soft_resample_by_audio_samples(void *aps_handle)
{
	uint64_t track_len, record_len;

	media_param_t param;

	uint32_t cur_time, track_time;

	int64_t bt_diff, adjust_diff, samples_diff;

	struct audio_record_t *record;
	struct audio_track_t *track;

	int output_sample_rate_hz;

	aps_monitor_info_t *handle = (aps_monitor_info_t *)aps_handle;

	handle->aps_count++;

	if(handle->aps_count != 10){
		return 0;
	}

	handle->aps_count = 0;

	output_sample_rate_hz = get_sample_rate_hz(audio_system_get_output_sample_rate());

	record = (struct audio_record_t *)handle->input_handle;
	track = (struct audio_track_t *)handle->output_handle;

	cur_time = k_cycle_get_32() / 24;
	record_len = audio_record_get_samples_cnt(record);
	track_len = audio_track_get_samples_cnt(track);
	track_time = k_cycle_get_32() / 24;

	if(!handle->last_frame_count && !handle->first_frame_count && track_len && record_len){
		handle->last_frame_count = track_len;
		handle->first_frame_count = record_len;
	}

	samples_diff = (record_len - handle->first_frame_count - (track_len - handle->last_frame_count));
	if(samples_diff > 0){
		bt_diff = samples_diff * 1000000 / output_sample_rate_hz;
	}else{
		samples_diff = 0 - samples_diff;
		bt_diff = samples_diff * 1000000 / output_sample_rate_hz;
		bt_diff = 0 - bt_diff;
	}

	if(track_time > cur_time){
		adjust_diff = bt_diff + track_time - cur_time;
	}else{
		adjust_diff = bt_diff;
	}

#if 1
	printk("<aps>track cnt %d record cnt %d %d %d diff %d time %d %d\n", (uint32_t)(track_len- handle->last_frame_count), \
		(uint32_t)(record_len - handle->first_frame_count), (uint32_t)track_len, (uint32_t)record_len, \
		(uint32_t)(record_len - handle->first_frame_count - (track_len- handle->last_frame_count)), track_time - cur_time, k_cycle_get_32() / 24);

#endif

	SYS_LOG_INF("adjust diff %d bt diff %d\n", (uint32_t)adjust_diff, (uint32_t)bt_diff);

	param.type = MEDIA_SET_RESAMPLE_APS_BT_TIME;
	param.pbuf = (void*)(&adjust_diff);
	param.plen = sizeof(adjust_diff);
	playback_service_set_parameter_by_track((void *)track, &param);

	return 0;
}


int audio_aps_soft_resample_monitor(void *handle)
{
	extern int capture_service_get_parameter(void *handle, media_param_t *param);
	extern int playback_service_get_parameter(void *handle, media_param_t *param);

	media_param_t param;
	param.type = MEDIA_GET_RESAMPLE_APS_CACHE_DATA_LEN;

	aps_monitor_info_t *aps_handle = (aps_monitor_info_t *)handle;
	uint8_t level = aps_handle->current_level;
	//sample_rate_khz * channel * sample_bits             48 * 2 * 2 // 16 * 1 * 2
	uint16_t aps_increase_water_mark = aps_handle->frame_size * aps_handle->aps_increase_water_mark; // 4 9
    uint16_t aps_reduce_water_mark = aps_handle->frame_size * aps_handle->aps_reduce_water_mark;     // 1 1
	uint16_t expect_water_mark;

	if(aps_handle->aps_type & APS_TYPE_CAPTURE){
		aps_handle->len = capture_service_get_parameter(aps_handle->input_handle, &param);
		expect_water_mark = aps_handle->frame_size * audio_policy_get_nav_frame_size_us() / 1000 + aps_reduce_water_mark;
		if ((aps_handle->len > aps_reduce_water_mark + 8 && aps_handle->len < aps_increase_water_mark) || aps_handle->len > expect_water_mark) {
			//to quick
			level--;
			if (level >= aps_handle->aps_min_level) {
				audio_aps_monitor_set_aps(handle, APS_OPR_FAST_SET, level);
			}
		} else if (aps_handle->len < aps_reduce_water_mark || (aps_handle->len >= aps_increase_water_mark && aps_handle->len <= expect_water_mark)){
			//to low
			level++;
			if (level <= aps_handle->aps_max_level) {
				audio_aps_monitor_set_aps(handle, APS_OPR_FAST_SET, level);
			}
		} else if(aps_handle->current_level != APS_LEVEL_4){
			audio_aps_monitor_set_aps(handle, APS_OPR_FAST_SET, APS_LEVEL_4);
		}
	}else{
		aps_handle->len = playback_service_get_parameter(aps_handle->input_handle, &param);
		if (aps_handle->len > aps_handle->frame_size * (audio_policy_get_nav_frame_size_us() / 1000 + 1) + 8) {
			//to quick
			level--;
			if (level >= aps_handle->aps_min_level) {
				audio_aps_monitor_set_aps(handle, APS_OPR_FAST_SET, level);
			}
		} else if (aps_handle->len < aps_handle->frame_size * (audio_policy_get_nav_frame_size_us() / 1000 + 1)){
			//to low
			level++;
			if (level <= aps_handle->aps_max_level) {
				audio_aps_monitor_set_aps(handle, APS_OPR_FAST_SET, level);
			}
		} else if(aps_handle->current_level != APS_LEVEL_4){
			audio_aps_monitor_set_aps(handle, APS_OPR_FAST_SET, APS_LEVEL_4);
		}
	}
	return 0;
}

int audio_aps_set_timeline_listener(aps_monitor_info_t *handle, int format)
{
	timeline_listener_t *listener = &handle->timeline_listener;

	listener->param = handle;
	if((handle->aps_type & (APS_TYPE_HARDWARE_AUDIO|APS_TYPE_BUFFER))
		== (APS_TYPE_HARDWARE_AUDIO|APS_TYPE_BUFFER)) {
		listener->trigger = audio_aps_monitor;
	}
	else if((handle->aps_type & (APS_TYPE_HARDWARE_AUDIO|APS_TYPE_INTERRUPT))
		== (APS_TYPE_HARDWARE_AUDIO|APS_TYPE_INTERRUPT)){
		listener->trigger = audio_aps_clock_monitor;
	}else if((handle->aps_type & (APS_TYPE_HARDWARE_BT|APS_TYPE_INTERRUPT))
		== (APS_TYPE_HARDWARE_BT|APS_TYPE_INTERRUPT)){
		listener->trigger = audio_aps_bt_clock_monitor;
	}else if((handle->aps_type & (APS_TYPE_SOFT|APS_TYPE_INTERRUPT))
		== (APS_TYPE_SOFT|APS_TYPE_INTERRUPT)){
		listener->trigger = audio_aps_soft_resample_by_interrupt_monitor;
	}else if((handle->aps_type & (APS_TYPE_SOFT|APS_TYPE_BUFFER))
		== (APS_TYPE_SOFT|APS_TYPE_BUFFER)){
		if (format == NAV_TYPE)
			listener->trigger = audio_aps_soft_resample_monitor;
		else
			listener->trigger = audio_aps_monitor;
	}else if((handle->aps_type & (APS_TYPE_AUDIO_SAMPLES | APS_TYPE_SOFT))
		== (APS_TYPE_AUDIO_SAMPLES | APS_TYPE_SOFT)){
		listener->trigger = audio_aps_soft_resample_by_audio_samples;
	}else
		listener->trigger = NULL;

	SYS_LOG_INF("%p: %p\n", handle, listener->trigger);
	if(listener->trigger)
		return timeline_add_listener(handle->timeline, listener);
	else
		return 0;
}

int audio_aps_debug_print(void *handle)
{
	aps_monitor_info_t *aps_handle = (aps_monitor_info_t *)handle;
	int time;

	if(!aps_handle || !aps_handle->frame_size)
		return -EINVAL;

	time = aps_handle->len / aps_handle->frame_size;
	aps_handle->print_count++;
	if(aps_handle->print_count % 1000 == 0){
		if(aps_handle->timeline_listener.trigger == audio_aps_soft_resample_monitor){
			if(aps_handle->aps_type & APS_TYPE_CAPTURE){
				if(time > audio_policy_get_nav_frame_size_us() / 1000 -1 && time < audio_policy_get_nav_frame_size_us() / 1000)
					printk("<aps>tx len %d %d\n",aps_handle->len,aps_handle->current_level);
			}else if(aps_handle->aps_type & APS_TYPE_PLAYBACK){
				if(time > audio_policy_get_nav_frame_size_us() / 1000 + 1 || time <= audio_policy_get_nav_frame_size_us() / 1000 -1)
					printk("<aps>rx len %d %d\n",aps_handle->len,aps_handle->current_level);
			}
		}else if(aps_handle->timeline_listener.trigger == audio_aps_soft_resample_by_interrupt_monitor){
			if(aps_handle->aps_type & APS_TYPE_CAPTURE){
				printk("<aps>c s diff %lld us %d\n",aps_handle->ref_diff,aps_handle->current_level);
			}else {
				printk("<aps>p s diff %lld us %d\n",aps_handle->ref_diff,aps_handle->current_level);
			}
		}else if(aps_handle->timeline_listener.trigger == audio_aps_bt_clock_monitor
			|| aps_handle->timeline_listener.trigger == audio_aps_clock_monitor){
			printk("<aps>c diff %d us %d\n",(int32_t)aps_handle->last_audio_time,aps_handle->current_level);
		}
	}
	return 0;
}

void* audio_aps_monitor_init(aps_monitor_params_t *aps_monitor_params)
{
	aps_monitor_info_t *handle;

	if (!aps_monitor_params || !aps_monitor_params->timeline || !aps_monitor_params->input_handle ||
		!aps_monitor_params->output_handle)
		return NULL;

	handle = mem_malloc(sizeof(aps_monitor_info_t));
	if (!handle)
		return NULL;

	memset(handle, 0, sizeof(aps_monitor_info_t));
	handle->timeline = aps_monitor_params->timeline;
	handle->input_handle = aps_monitor_params->input_handle;
	handle->output_handle = aps_monitor_params->output_handle;
	handle->aps_status = APS_STATUS_DEFAULT;
	handle->aps_min_level = APS_LEVEL_1;
	handle->aps_max_level = APS_LEVEL_8;
	handle->aps_default_level = APS_LEVEL_5;
	handle->frame_size = aps_monitor_params->frame_size;

	/* Default water mark: no aps adjustment */
	handle->aps_increase_water_mark = UINT16_MAX;
	handle->aps_reduce_water_mark = 0;
	handle->need_aps = 1;
	handle->format = aps_monitor_params->format;
	handle->stream_type = aps_monitor_params->stream_type;
	handle->aps_increase_water_mark = audio_policy_get_increase_threshold(aps_monitor_params->format, aps_monitor_params->stream_type);
	handle->aps_reduce_water_mark = audio_policy_get_reduce_threshold(aps_monitor_params->format, aps_monitor_params->stream_type);
	handle->duration = AUDIO_APS_ADJUST_INTERVAL;
	handle->aps_type = aps_monitor_params->aps_type;
	memcpy(&handle->device_info,&aps_monitor_params->device_info, sizeof(media_device_info_t));

	SYS_LOG_INF("water(%d-%d) type0x%x format%d stream%d\n",
		handle->aps_reduce_water_mark, handle->aps_increase_water_mark,
		aps_monitor_params->aps_type, aps_monitor_params->format, aps_monitor_params->stream_type);

	switch (aps_monitor_params->format) {
		case SBC_TYPE:
		case LDAC_TYPE:
		{
			if(aps_monitor_params->aps_type & APS_TYPE_SOFT){
				handle->aps_default_level = APS_LEVEL_4;
				handle->duration = 6;
			}

			if (aps_monitor_params->tws_observer) {
#ifdef CONFIG_TWS
				audio_aps_monitor_tws_init(handle,aps_monitor_params->tws_observer);
				audio_track_set_waitto_start((struct audio_track_t *)aps_monitor_params->output_handle, true);
#endif
			}
			break;
		}
		case MP3_TYPE:
		{
			handle->need_aps = 0;
#ifdef CONFIG_TWS
			if (aps_monitor_params->tws_observer) {
				audio_aps_monitor_tws_init(handle,aps_monitor_params->tws_observer);
				audio_track_set_waitto_start((struct audio_track_t *)aps_monitor_params->output_handle, true);
			}
#endif
			if (audio_system_get_i2stx_mode() == 1
				&& aps_monitor_params->aps_type & (APS_TYPE_PLAYBACK|APS_TYPE_SOFT|APS_TYPE_INTERRUPT)) {
				handle->need_aps = 1;
				handle->aps_type = aps_monitor_params->aps_type;
			}
			break;
		}
		case AAC_TYPE:
		{
			if(aps_monitor_params->aps_type & APS_TYPE_SOFT){
				handle->aps_default_level = APS_LEVEL_4;
				handle->duration = 6;
			}

			if (aps_monitor_params->tws_observer) {
#ifdef CONFIG_TWS
				audio_aps_monitor_tws_init(handle,aps_monitor_params->tws_observer);
				audio_track_set_waitto_start((struct audio_track_t *)aps_monitor_params->output_handle, true);
#endif
			}
			break;
		}
		case MSBC_TYPE:
		case CVSD_TYPE:
		{
			if (system_check_low_latencey_mode()) {
				handle->aps_min_level = APS_LEVEL_3;
				handle->aps_max_level = APS_LEVEL_6;
				handle->duration = 6;
			}
			if(aps_monitor_params->aps_type & APS_TYPE_SOFT){
				handle->aps_default_level = APS_LEVEL_4;
				handle->duration = 6;
			}
			break;
		}
		case ACT_TYPE:
		{
			break;
		}
		case PCM_TYPE:
		{
			handle->aps_min_level = APS_LEVEL_4;
			handle->aps_max_level = APS_LEVEL_6;
			handle->need_aps = 0;//to do
			if(aps_monitor_params->aps_type & APS_TYPE_SOFT){
				handle->aps_min_level = APS_LEVEL_3;
				handle->aps_max_level = APS_LEVEL_5;
				handle->aps_default_level = APS_LEVEL_4;
				handle->duration = 6;

				if (audio_policy_get_channel_resample_aps(aps_monitor_params->stream_type,
					aps_monitor_params->format, aps_monitor_params->is_playback)) {
					handle->need_aps = 1;
				}
			}

			if (aps_monitor_params->tws_observer) {
#ifdef CONFIG_TWS
				audio_aps_monitor_tws_init(handle,aps_monitor_params->tws_observer);
				audio_track_set_waitto_start((struct audio_track_t *)aps_monitor_params->output_handle, true);
#endif
			}
			break;
		}
		case NAV_TYPE:
		{
			if(aps_monitor_params->aps_type & APS_TYPE_SOFT){
				handle->aps_min_level = APS_LEVEL_3;
				handle->aps_max_level = APS_LEVEL_5;
				handle->aps_default_level = APS_LEVEL_4;
			}else if(aps_monitor_params->aps_type & (APS_TYPE_HARDWARE_AUDIO | APS_TYPE_HARDWARE_BT)){
				handle->aps_min_level = APS_LEVEL_4;
				handle->aps_max_level = APS_LEVEL_6;
				handle->aps_default_level = APS_LEVEL_5;
			}
			if(handle->aps_type & APS_TYPE_PLAYBACK){
				if(handle->device_info.rx_chan.validate)
					handle->ref_timeline = timeline_get_by_type_and_onwer(TIMELINE_TYPE_MEDIA_DECODE
						,handle->device_info.rx_chan.timeline_owner);
			} else if(handle->aps_type & APS_TYPE_CAPTURE){
				if(handle->device_info.tx_chan.validate)
					handle->ref_timeline = timeline_get_by_type_and_onwer(TIMELINE_TYPE_MEDIA_ENCODE
						,handle->device_info.tx_chan.timeline_owner);
			}
			break;
		}
		default:
		{
			handle->need_aps = 0;
			handle->aps_type = 0;//to do ,init in policy
			if (aps_monitor_params->tws_observer) {
#ifdef CONFIG_TWS
				audio_aps_monitor_tws_init(handle,aps_monitor_params->tws_observer);
#endif
				// audio_track_set_waitto_start(audio_track, true);
			}

		if (audio_system_get_i2stx_mode() == 1
			&& aps_monitor_params->aps_type & (APS_TYPE_PLAYBACK|APS_TYPE_SOFT|APS_TYPE_INTERRUPT)) {
			handle->need_aps = 1;
			handle->aps_type = aps_monitor_params->aps_type;
		}
			break;
		}
	}

	handle->current_level = handle->aps_default_level;
	handle->dest_level = handle->current_level;

	if (handle->need_aps && audio_aps_set_timeline_listener(handle, aps_monitor_params->format)) {
		mem_free(handle);
		return NULL;
	}

	return handle;
}

void audio_aps_notify_decode_err(void *handle, uint16_t err_cnt)
{
#ifdef CONFIG_TWS
	audio_aps_tws_notify_decode_err(handle,err_cnt);
#endif
}

void audio_asp_set_pcm_monitor_callback(pcm_monitor_callback cb)
{
    pcm_monitor_cb = cb;
}

void audio_aps_monitor_deinit(void *handle, int format, void *tws_observer)
{
	aps_monitor_info_t *aps_handle = (aps_monitor_info_t *)handle;

	/* Be consistent with audio_aps_monitor_init */
	switch (format) {
		case ACT_TYPE:
		case MSBC_TYPE:
		case CVSD_TYPE:
			break;
		case SBC_TYPE:
		case AAC_TYPE:
		default:
			if (tws_observer) {
#ifdef CONFIG_TWS
				audio_aps_monitor_tws_deinit(aps_handle,tws_observer);
#endif
			}
			break;
	}

	if (aps_handle) {
		if (aps_handle->timeline)
			timeline_remove_listener(aps_handle->timeline, &aps_handle->timeline_listener);
		mem_free(handle);
	}
}

