/*
 * Copyright (c) 2016 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief audio system.
*/

#ifndef __AUDIO_SYSTEM_H__
#define __AUDIO_SYSTEM_H__
#include <stream.h>
#include <acts_ringbuf.h>
#include <timeline.h>
#include <hrtimer.h>
#include <media_service.h>

#define MULTI_CH_MODE_2_0 	0
#define MULTI_CH_MODE_2_1 	1
#define MULTI_CH_MODE_2_2 	2
#define MULTI_CH_MODE_2_11 	3
#define MULTI_CH_MODE_11 	4

/**
 * @defgroup audio_system_apis Auido System APIs
 * @ingroup media_system_apis
 * @{
 */
typedef enum {
	AUDIO_STREAM_DEFAULT = 1,
	AUDIO_STREAM_MUSIC,
	AUDIO_STREAM_LOCAL_MUSIC,
	AUDIO_STREAM_TTS,
	AUDIO_STREAM_VOICE,
	AUDIO_STREAM_LINEIN,
	AUDIO_STREAM_LINEIN_MIX,
	AUDIO_STREAM_SUBWOOFER,
	AUDIO_STREAM_ASR,
	AUDIO_STREAM_AI,
	AUDIO_STREAM_USOUND,
	AUDIO_STREAM_USOUND_MIX,
	AUDIO_STREAM_USPEAKER,
	AUDIO_STREAM_I2SRX_IN,
	AUDIO_STREAM_SPDIF_IN,
	AUDIO_STREAM_GENERATE_IN,
	AUDIO_STREAM_GENERATE_OUT,
	AUDIO_STREAM_LOCAL_RECORD,
	AUDIO_STREAM_GMA_RECORD,
	AUDIO_STREAM_BACKGROUND_RECORD,
	AUDIO_STREAM_MIC_IN,
	AUDIO_STREAM_FM,
	AUDIO_STREAM_TWS,
	AUDIO_STREAM_OKMIC,
	AUDIO_STREAM_OKMIC_FM,
	AUDIO_STREAM_OKMIC_LINE_IN,
	AUDIO_STREAM_LE_AUDIO,
	AUDIO_STREAM_SOUNDBAR,
	AUDIO_STREAM_BMR,
}audio_stream_type_e;

typedef enum {
	AUDIO_MODE_DEFAULT = 0,
	AUDIO_MODE_MONO,                    //mono->left mono->right
	AUDIO_MODE_STEREO,                  //left->left right->right
}audio_mode_e;

typedef enum {
	AUDIO_STREAM_TRACK = 1,
	AUDIO_STREAM_RECORD,
}audio_stream_mode_e;

typedef enum {
	AUDIO_FORMAT_PCM_8_BIT = 0,
	AUDIO_FORMAT_PCM_16_BIT,
	AUDIO_FORMAT_PCM_24_BIT,
	AUDIO_FORMAT_PCM_32_BIT,
}audio_format_e;

/**
 * @cond INTERNAL_HIDDEN
 */

#define MIN_WRITE_SAMPLES    1 * 1024

#define MAX_AUDIO_TRACK_NUM  2
#define MAX_AUDIO_RECORD_NUM 2
#define MAX_AUDIO_DEVICE_NUM 1

#define MAX_VOLUME_VALUE 2
#define MIN_VOLUME_VALUE 1
#define DEFAULT_VOLUME   5

struct audio_track_t {
	u8_t stream_type;
	u8_t audio_format;
	u8_t audio_mode;
	u8_t channel_type;
	u8_t channel_id;
	u8_t channel_mode;
	u8_t sample_rate;
	u8_t output_sample_rate;
	u8_t frame_size;
	u8_t muted:1;
	u8_t started:1;
	u8_t flushed:1;
	u8_t waitto_start:1;
	/**debug flag*/
	u8_t dump_pcm:1;
	u8_t fill_zero:1;
	u8_t fade_out:1;
	u16_t volume;


	u16_t pcm_frame_size;
	u8_t *pcm_frame_buff;

	io_stream_t audio_stream;
	io_stream_t mix_stream;
	u8_t mix_sample_rate;
	u8_t mix_channels;

	/** audio hal handle*/
	void *audio_handle;

	void (*event_cb)(u8_t, void *);
	void *user_data;

	/* For tws sync fill samples */
	int compensate_samples;
	int fill_cnt;
	u32_t output_samples;

	/* resample */
	void *res_handle;
	int res_in_samples;
	int res_out_samples;
	int res_remain_samples;
	s16_t *res_in_buf[2];
	s16_t *res_out_buf[2];

	/* fade in/out */
	void *fade_handle;

	/* mix */
	void *mix_handle;

	timeline_t *timeline;

    uint64_t samples_filled;
    uint32_t sdm_sample_rate;
    uint16_t sdm_osr;

	void (*sync_cb)(void *);
	void *sync_data;

	uint32_t last_samples_cnt;
	u64_t samples_overflow_cnt;

	u32_t phy_dma;
};

struct audio_record_t {
	u8_t stream_type;
	u8_t audio_format;
	u8_t media_format;
	u8_t audio_mode;
	u8_t channel_type;
	u8_t channel_id;
	u8_t channel_mode;
	u8_t sample_rate;
	u8_t output_sample_rate;
	u8_t frame_size;
	u8_t muted:1;
	u8_t paused:1;
	u8_t first_frame:1;
	u8_t waitto_start:1;
	/**debug flag*/
	u8_t dump_pcm:1;
	u8_t fill_zero:1;
	u8_t started : 1;

	s16_t adc_gain;
	s16_t input_gain;
	u16_t volume;
	u16_t pcm_buff_size;
	u8_t *pcm_buff;
	/** audio hal handle*/
	void *audio_handle;
	io_stream_t audio_stream;
	struct acts_ringbuf *ref_stream;

	timeline_t *timeline;

	u64_t input_samples;

	u16_t prev_reload_cnt;
	u64_t dma_overflow_bytes;
};

struct audio_system_t {
	os_mutex audio_system_mutex;
	struct audio_track_t *audio_track_pool[MAX_AUDIO_TRACK_NUM];
	struct audio_record_t *audio_record_pool[MAX_AUDIO_RECORD_NUM];
	struct audio_device_t *audio_device_pool[MAX_AUDIO_DEVICE_NUM];
	u8_t audio_track_num;
	u8_t audio_record_num;
	bool microphone_muted;
	u8_t output_sample_rate;
	u8_t capture_output_sample_rate;
	bool master_muted;
	u8_t master_volume;

	u8_t tts_volume;
	u8_t music_volume;
	u8_t voice_volume;
	u8_t linein_volume;
	u8_t fm_volume;
	u8_t i2srx_in_volume;
	u8_t mic_in_volume;
	u8_t spidf_in_volume;
	u8_t usound_volume;
	u8_t lcmusic_volume;
	u8_t max_volume;
	u8_t min_volume;
	u8_t lea_volume;
	u8_t bmr_volume;
};

/** cace info ,used for cache stream */
typedef struct
{
	u8_t audio_type;
	u8_t audio_mode;
	u8_t channel_mode;
	u8_t stream_start:1;
	u8_t dma_start:1;
	u8_t dma_reload:1;
	u8_t pcm_buff_owner:1;
	u8_t data_finished:4;
	u16_t dma_send_len;
	u16_t pcm_frame_size;
	struct acts_ringbuf *pcm_buff;
	/**pcm cache*/
	io_stream_t pcm_stream;
} audio_info_t;

typedef enum
{
    APS_OPR_SET          = (1 << 0),
    APS_OPR_ADJUST       = (1 << 1),
    APS_OPR_FAST_SET     = (1 << 2),
}aps_ops_type_e;

typedef enum
{
    APS_STATUS_DEC,
    APS_STATUS_INC,
    APS_STATUS_DEFAULT,
}aps_status_e;

typedef enum
{
    APS_TYPE_CAPTURE             = (1 << 0),
    APS_TYPE_PLAYBACK            = (1 << 1),
    APS_TYPE_SOFT                = (1 << 2),
    APS_TYPE_HARDWARE_AUDIO      = (1 << 3),
    APS_TYPE_HARDWARE_BT         = (1 << 4),
    APS_TYPE_BUFFER              = (1 << 5),
    APS_TYPE_INTERRUPT           = (1 << 6),
    APS_TYPE_AUDIO_SAMPLES       = (1 << 7),
}aps_type_e;

typedef struct {
	u8_t current_level;
	u8_t dest_level;
	u8_t aps_status;
	u8_t aps_mode;

	u8_t aps_min_level;
	u8_t aps_max_level;
	u8_t aps_default_level;
	u8_t role;
	u8_t duration;
	u8_t need_aps:1;

	u16_t aps_reduce_water_mark;
	u16_t aps_increase_water_mark;

	u8_t aps_type;
	u8_t stream_type;
	u8_t format;
	void *tws_observer;
	void *input_handle;
	void *output_handle;
	timeline_t *timeline;
	timeline_listener_t timeline_listener;
	timeline_t *ref_timeline;
	uint32_t print_count;
	int len;
	u16_t frame_size;
	media_device_info_t device_info;

	uint64_t last_audio_time;
	uint64_t first_frame_count;
	uint64_t last_frame_count;
	uint64_t aps_count;
	int64_t ref_diff;
#ifdef CONFIG_ACTS_HRTIMER
    struct hrtimer timer;
#endif
	uint32_t aps_time;
}aps_monitor_info_t;

typedef struct {
	u8_t aps_type;
	u8_t stream_type;
	u8_t format;
	u8_t is_playback:1;
	void *tws_observer;
	timeline_t *timeline;
	void *input_handle;
	void *output_handle;
	u16_t frame_size;
	media_device_info_t device_info;
} aps_monitor_params_t;

typedef int (*pcm_monitor_callback)(u16_t pcm_time,u16_t normal_level,u8_t aps_status);

/**
 * INTERNAL_HIDDEN @endcond
 */

/**
 * @brief set audio system output sample rate
 *
 * @details This routine provides to set audio system output sample rate,
 *  if set audio system output sample rate, all out put stream may resample to
 *  the target sample rate
 *
 * @param value sample rate
 *
 * @return 0 excute successed , others failed
 */
int audio_system_set_output_sample_rate(int value);
/**
 * @brief get audio system output sample rate
 *
 * @details This routine provides to get audio system output sample rate,
 *
 * @return value of sample rate
 */
int audio_system_get_output_sample_rate(void);

/**
 * @brief set audio system master volume
 *
 * @details This routine provides to set audio system master volume
 *
 * @param value volume value
 *
 * @return 0 excute successed , others failed
 */

int audio_system_set_master_volume(int value);

/**
 * @brief get audio system master volume
 *
 * @details This routine provides to get audio system master volume
 *
 * @return value of volume level
 */

int audio_system_get_master_volume(void);

/**
 * @brief set audio system master mute
 *
 * @details This routine provides to set audio system master mute
 *
 * @param value mute value 1: mute 0: unmute
 *
 * @return 0 excute successed , others failed
 */

int audio_system_set_master_mute(int value);

/**
 * @brief get audio system master mute state
 *
 * @details This routine provides to get audio system master mute state
 *
 * @return  1: audio system muted
 * @return  0: audio system unmuted
 */

int audio_system_get_master_mute(void);

int audio_system_set_stream_volume(int stream_type, int value);

int audio_system_get_stream_volume(int stream_type);

int audio_system_get_current_volume(int stream_type);

int audio_system_set_stream_mute(int stream_type, int value);

int audio_system_get_stream_mute(int stream_type);

int audio_system_mute_microphone(int value);

int audio_system_get_microphone_muted(void);

int audio_system_get_current_pa_volume(int stream_type);

/* @volume in 0.001 dB */
int audio_system_set_stream_pa_volume(int stream_type, int volume);

/* @volume in 0.1 dB */
int audio_system_set_microphone_volume(int stream_type, int volume);

int audio_system_get_max_volume(void);

int audio_system_get_min_volume(void);

void audio_system_clear_volume(void);

int aduio_system_init(void);
/**
 * @cond INTERNAL_HIDDEN
 */
int audio_system_register_track(struct audio_track_t *audio_track);

int audio_system_unregister_track(struct audio_track_t *audio_track);

int audio_system_register_record(struct audio_record_t *audio_record);

int audio_system_unregister_record(struct audio_record_t *audio_record);

struct audio_track_t * audio_system_get_audio_track_handle(int stream_type);

struct audio_record_t * audio_system_get_audio_record_handle(int stream_type);



void* audio_aps_monitor_init(aps_monitor_params_t *aps_monitor_params);

void audio_aps_monitor_deinit(void *handle, int format, void *tws_observer);

void audio_aps_notify_decode_err(void *handle, uint16_t err_cnt);

void audio_aps_monitor_init_add_samples(int format, u8_t *need_notify, u8_t *need_sync);

void audio_aps_monitor_exchange_samples(u32_t *ext_add_samples, u32_t *sync_ext_samples);

void audio_aps_monitor_tws_init(aps_monitor_info_t *handle,void *tws_observer);

void audio_aps_tws_notify_decode_err(aps_monitor_info_t *handle,u16_t err_cnt);

void audio_aps_monitor_tws_deinit(aps_monitor_info_t *handle,void *tws_observer);

int audio_aps_debug_print(void *handle);

struct audio_track_t * audio_system_get_track(void);
struct audio_record_t *audio_system_get_record(void);

int audio_system_mutex_lock(void);
int audio_system_mutex_unlock(void);

int audio_system_trigger_track_start(int stream_type);

int audio_system_trigger_record_start(int stream_type);

int32_t audio_tws_set_stream_info(
    uint8_t format, uint16_t first_seq, uint8_t sample_rate, uint32_t pkt_time_us, uint64_t play_time);

/**
 * @brief get snoop tws first stream packet seq to play
 *
 * @param start_decode  indicate to start decode or no
 * @param start_play  indicate to start play or no
 * @param first_seq     first seq to decode
 *
 * @return 0 excute successed , others failed
 */
int audio_tws_get_playback_first_seq(uint8_t *start_decode, uint8_t *start_play, uint16_t *first_seq, uint16_t *playtime_us);

int32_t audio_tws_set_pkt_info(uint16_t pkt_seq, uint16_t pkt_len, uint16_t frame_cnt, uint16_t pcm_len);
uint64_t audio_tws_get_play_time_us(void);
uint64_t audio_tws_get_bt_clk_us(void *tws_observer);

void audio_tws_pre_init(void);

void audio_multicore_sync_init(void);

void audio_multicore_sync_trigger_start(void);

void audio_multicore_sync_trigger_stop(void);

void audio_asp_set_pcm_monitor_callback(pcm_monitor_callback cb);
u8_t audio_system_get_multi_channel_mode(void);
int audio_system_get_i2stx_mode(void);

/**
 * INTERNAL_HIDDEN @endcond
 */

/**
 * @} end defgroup audio_system_apis
 */

#endif
