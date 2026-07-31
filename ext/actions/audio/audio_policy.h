/*
 * Copyright (c) 2016 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief audio policy.
*/

#ifndef __AUDIO_POLICY_H__
#define __AUDIO_POLICY_H__

#include <audio_hal.h>
#include <media_effect_param.h>
#include <timeline.h>

/**
 * @defgroup audio_policy_apis Auido policy APIs
 * @{
 */

struct audio_policy_t {
	u8_t audio_out_channel;
	u8_t tts_fixed_volume:1;
	u8_t volume_saveto_nvram:1;
	u8_t audio_out_volume_level;

	s16_t audio_in_linein_gain;
	s16_t audio_in_fm_gain;
	s16_t audio_in_mic_gain;

	u8_t voice_aec_tail_length_8k;
	u8_t voice_aec_tail_length_16k;

#ifdef CONFIG_AUDIO_VOLUME_PA
	const u32_t *music_da_table; /* [0~0x4000000000], DSP digital decrease, new = original*(da_val/0x40000000) */
	const u8_t *music_pa_table;	 /* [0~40] PA_VOLUME level */
	const u16_t *voice_da_table; /* [0~0x7FFF], DSP digital decrease, new = original*(da_val/0x7FFF) */
	const u8_t *voice_pa_table;	 /* [0~40] PA_VOLUME level */
	const u32_t *usound_da_table;
	const u8_t *usound_pa_table;
#else
	const u32_t *music_da_table; /* 0.1 dB */
	const int   *music_pa_table; /* 0.001 dB */
	const u16_t *voice_da_table; /* 0.1 dB */
	const int   *voice_pa_table; /* 0.001 dB */
	const short *usound_da_table; /* 0.1 dB */
	const int   *usound_pa_table; /* 0.001 dB */
#endif
#ifdef CONFIG_AUDIO_MULTI_CHANNEL_MODE
	const u16_t *multi_ch_da0_table;
	const u16_t *multi_ch_da1_table;
#endif
};

#define LE_VOLUME_TO_MDB(volume)  (-60000 + (60000 * volume) / 255)

timeline_t *audio_policy_get_playback_timeline(uint8_t stream_type, u8_t format,void* bt_rx_timeline_owner);
timeline_t *audio_policy_get_capture_timeline(uint8_t stream_type, u8_t format,void* bt_tx_timeline_owner);
timeline_t *audio_policy_get_decode_timeline(uint8_t stream_type, u8_t format,void* bt_rx_timeline_owner);
timeline_t *audio_policy_get_encode_timeline(uint8_t stream_type, u8_t format,void* bt_tx_timeline_owner);

int audio_policy_get_aps_type(uint8_t stream_type, uint8_t format, uint8_t is_playback, uint8_t is_tws);

int audio_policy_get_out_channel_type(u8_t stream_type);

int audio_policy_get_out_channel_id(u8_t stream_type);

int audio_policy_get_out_channel_mode(u8_t stream_type);

/* return 0 for decoder decided */
int audio_policy_get_out_pcm_channel_num(u8_t stream_type);

int audio_policy_get_out_pcm_frame_size(u8_t stream_type);

int audio_policy_get_out_channel_asrc_alloc_method(u8_t stream_type);

int audio_policy_get_out_input_start_threshold(u8_t stream_type, int format,
			u8_t exf_stream_type, u8_t sample_rate, u8_t channels, u8_t tws_mode);

int audio_policy_get_out_input_stop_threshold(u8_t stream_type, int format,
			u8_t exf_stream_type, u8_t sample_rate, u8_t channels, u8_t tws_mode);

int audio_policy_get_out_audio_mode(u8_t stream_type);

u8_t audio_policy_get_out_effect_type(u8_t stream_type,
			u8_t efx_stream_type, bool is_tws);

uint8_t audio_policy_get_out_effect_support_dynamic_peq(uint8_t stream_type);

uint8_t audio_policy_dynamic_update_peq(u8_t * arr);

int audio_policy_set_dynamic_peq_info(eq_band_t * eq);

int audio_policy_get_output_support_get_energy(u8_t stream_type);

int audio_policy_postprocessor_use_input_sample_rate(uint8_t stream_type);

int audio_policy_is_out_channel_aec_reference(u8_t stream_type);

int audio_policy_is_out_channel_aec_reference(u8_t stream_type);

int audio_policy_is_da_mute(u8_t stream_type);

int audio_policy_is_fill_zero_fadein(u8_t stream_type);

u16_t audio_policy_get_track_pcm_buff_size(u8_t stream_type, int sample_rate, uint8_t audio_mode, uint8_t format);

u8_t audio_policy_get_record_effect_type(u8_t stream_type,
			u8_t efx_stream_type);
int audio_policy_get_record_audio_mode(u8_t stream_type);
int audio_policy_get_record_channel_id(u8_t stream_type);

int audio_policy_get_record_adc_gain(u8_t stream_type);

int audio_policy_get_record_input_gain(u8_t stream_type);

int audio_policy_get_record_channel_mode(u8_t stream_type);

int audio_policy_get_record_channel_type(u8_t stream_type);

int audio_policy_get_record_ext_ref_stream_type();

int audio_policy_get_record_interleave_mode(uint8_t stream_type);

u16_t audio_policy_get_record_pcm_buff_size(u8_t stream_type, int sample_rate_input, uint8_t audio_mode, uint8_t format);

int audio_policy_get_volume_level(void);

/* return volume in 0.001 dB */
int audio_policy_get_pa_volume(u8_t stream_type, u8_t volume_level);
int audio_policy_get_pa_class(u8_t stream_type);

/* return volume in 0.1 dB */
int audio_policy_get_da_volume(u8_t stream_type, u8_t volume_level);

s16_t audio_policy_get_multi_ch_da_volume(u8_t stream_type, u8_t level, u8_t ch);

/* return volume in 0.1 dB */
int audio_policy_get_bybass_adjust_da_volume(uint8_t stream_type);

int audio_policy_get_record_channel_support_aec(u8_t stream_type);

int audio_policy_get_record_channel_aec_tail_length(u8_t stream_type, u8_t sample_rate, bool in_debug);

int audio_policy_get_channel_resample(u8_t stream_type, bool decoder);

int audio_policy_get_channel_resample_aps(u8_t stream_type, int format, bool decoder);

int audio_policy_get_output_support_multi_track(u8_t stream_type);

int audio_policy_check_tts_fixed_volume();

int audio_policy_get_nav_frame_size_us();
void audio_policy_set_nav_frame_size_us(u16_t frame_size_us);

void audio_policy_set_bis_link_delay_ms(u16_t delay_ms);
int audio_policy_get_bis_link_delay_ms();
void audio_policy_set_smart_control_gain(int16_t gain);
int16_t audio_policy_get_smart_control_gain(void);
void audio_policy_set_dynamic_waterlevel_ms(u16_t time_ms);
int audio_policy_get_dynamic_waterlevel_ms();
void audio_policy_set_user_dynamic_waterlevel_ms(u16_t time_ms);
int audio_policy_get_user_dynamic_waterlevel_ms();
void audio_policy_set_lea_start_threshold(u16_t start_threshold);
u16_t audio_policy_get_lea_start_threshold();

int audio_policy_get_bis_align_buffer_size(int width16, int sample_rate_hz, int ext_delay);

int audio_policy_is_record_triggered_by_track(u8_t stream_type);

int audio_policy_check_save_volume_to_nvram(void);

int audio_policy_get_increase_threshold(int format, u8_t stream_type);

int audio_policy_get_reduce_threshold(int format, u8_t stream_type);

/* @volume_db in 0.1 dB */
int audio_policy_get_volume_level_by_db(u8_t stream_type, int volume_db);

int audio_policy_is_master_mix_channel(u8_t stream_type);

int audio_policy_get_record_aec_block_size(u8_t format_type);

int audio_policy_get_record_channel_mix_channel(u8_t stream_type);
int audio_policy_get_aec_reference_type(uint8_t stream_type);

int audio_policy_get_out_channel_aec_reference_stream_type(uint8_t stream_type);

int audio_policy_check_snoop_tws_support(uint8_t stream_type);

int audio_policy_get_snoop_tws_sync_param(uint8_t format,
    uint16_t *negotiate_time, uint16_t *negotiate_timeout, uint16_t *sync_interval);
/**
 * INTERNAL_HIDDEN @endcond
 */

/**
 * @brief Register user audio policy to audio system
 *
 * This routine Register user audio policy to audio system, must be called when system
 * init. if not set user audio policy ,system may used default policy for audio system.
 *
 * @param user_policy user sudio policy
 *
 * @return 0 excute successed , others failed
 */

int audio_policy_register(const struct audio_policy_t *user_policy);

u8_t audio_policy_get_subwoofer_channels(void);
/**
 * @} end defgroup audio_policy_apis
 */
#endif
