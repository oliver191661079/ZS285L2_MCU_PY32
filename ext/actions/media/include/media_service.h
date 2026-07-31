/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file media service interface
 */

#ifndef __MEDIA_SERVICE_H__
#define __MEDIA_SERVICE_H__

#include <os_common_api.h>
#include <msg_manager.h>
#include <stream.h>
#include <thread_timer.h>
#include <arithmetic.h>
#include <media_type.h>
#ifdef CONFIG_BT_MANAGER
#include <btservice_api.h>
#include <bt_manager.h>
#endif
#include <timeline.h>

/**
 * @defgroup media_service_apis App Media Service APIs
 * @ingroup media_system_apis
 * @{
 */
/**
 * @cond INTERNAL_HIDDEN
 */
/**
 * @brief media service event notify callback
 *
 * This routine called by media service, notify media service state changed
 *  event media_event_type_e
 *
 * @param event event type of media_srv_status_e.
 * @param data  event data.
 * @param len   event data length.
 * @param user_data user_data passed in media_init_param_t during media_player_open
 *
 * @return N/A.
 */
typedef void (*media_srv_event_notify_t)(u32_t event, void *data, u32_t len, void *user_data);


typedef void (*media_srv_sync_callback_t)(void *data);

/** media service operation message type*/
enum media_srv_operation_e {
	/** open a new media service instance*/
	MSG_MEDIA_SRV_OPEN = MSG_SRV_MESSAGE_START,
	/** close target media service instance*/
	MSG_MEDIA_SRV_CLOSE,
	/** start to play*/
	MSG_MEDIA_SRV_PLAY,
	/** stop to play*/
	MSG_MEDIA_SRV_STOP,
	/** media service pause*/
	MSG_MEDIA_SRV_PAUSE,
	/** media service resume*/
	MSG_MEDIA_SRV_RESUME,
	MSG_MEDIA_SRV_START_STOP_DECODE,
	/** media service trigger to process data*/
	MSG_MEDIA_SRV_DATA_PROCESS,
	/** media service dump data*/
	MSG_MEDIA_SRV_DUMP_DATA,
	/** media service seek to play*/
	MSG_MEDIA_SRV_SEEK,
	/** media service query parameter*/
	MSG_MEDIA_SRV_GET_PARAMETER,
	/** media service set parameter*/
	MSG_MEDIA_SRV_SET_PARAMETER,
	/** media service get global parameter*/
	MSG_MEDIA_SRV_GET_GLOBAL_PARAMETER,
	/** media service set global parameter*/
	MSG_MEDIA_SRV_SET_GLOBAL_PARAMETER,
};

/** dumpable data tag */
enum media_dump_data_tag_e {
	/** mic left channel */
	MEDIA_DATA_TAG_MIC1 = 0,
	/** mic right channel */
	MEDIA_DATA_TAG_MIC2,
	/** reference (single channel) */
	MEDIA_DATA_TAG_REF,
	/** AEC left channel */
	MEDIA_DATA_TAG_AEC1,
	/** AEC right channel */
	MEDIA_DATA_TAG_AEC2,
	/** encoder (left channel) input */
	MEDIA_DATA_TAG_ENCODE_IN1,
	/** encoder right channel input */
	MEDIA_DATA_TAG_ENCODE_IN2,
	/** encoder output stream */
	MEDIA_DATA_TAG_ENCODE_OUT,

	/** decoder input stream */
	MEDIA_DATA_TAG_DECODE_IN,
	/** decoder (left channel) output */
	MEDIA_DATA_TAG_DECODE_OUT1,
	/** decoder right channel output */
	MEDIA_DATA_TAG_DECODE_OUT2,
	/** plc output */
	MEDIA_DATA_TAG_PLC,
	/** dac (left channel) output */
	MEDIA_DATA_TAG_DAC1,
	/** dac right channel output */
	MEDIA_DATA_TAG_DAC2,
	MEDIA_TAG_FFVP_DATA,
	MEDIA_TAG_ENCODED_DATA,

	MEDIA_DATA_TAG_DRC,

	MEDIA_DATA_TAG_EFFECT_IN,
	MEDIA_DATA_TAG_EFFECT_OUT,
};

/** media service state */
typedef enum {
	/** media service stop state */
	MEDIA_SRV_STATUS_STOP = 0,
	/** media service configed state */
	MEDIA_SRV_STATUS_CONFIG,
	/** media service ready state */
	MEDIA_SRV_STATUS_READY,
	/** media service playing state */
	MEDIA_SRV_STATUS_PLAY,
	/** media service paused state */
	MEDIA_SRV_STATUS_PAUSE,
	/** media service finished state */
	MEDIA_SRV_STATUS_FINISHED,
	/** media service Error state */
	MEDIA_SRV_STATUS_ERROR,
	/** media service NULL state */
	MEDIA_SRV_STATUS_NULL,
} media_srv_status_e;

/** media service parameter name */
typedef enum {
	/** media service global parameter */
	/** set background recording */
	MEDIA_PARAM_SET_RECORD,

	/** media player parameter */

	/** media info */
	MEDIA_PARAM_MEDIAINFO,
	/** break point */
	MEDIA_PARAM_BREAKPOINT,
	/** output mode */
	MEDIA_PARAM_OUTPUT_MODE,
	/** get state */
	MEDIA_PARAM_GET_STATE,
	/** energy filter param */
	MEDIA_PARAM_ENERGY_FILTER,
	/** audio sync play time */
	MEDIA_PARAM_SET_PLAY_TIME,

	/** The following are audio effect operations, do not mix other operation types */

	/** audio effect operation set volume */
	MEDIA_EFFECT_EXT_SET_VOLUME,
	/** audio effect operation set mic mute */
	MEDIA_EFFECT_EXT_SET_MIC_MUTE,
	/** audio effect operation set hfp connected flag */
	MEDIA_EFFECT_EXT_HFP_CONNECTED,
	/** audio effect operation enable dae all functions */
	MEDIA_EFFECT_EXT_SET_DAE_ENABLE,
	/** audio effect operation bypass dae all functions, except fade in/out */
	MEDIA_EFFECT_EXT_SET_DAE_BYPASS,
	/** audio effect operation set dae output mode*/
	MEDIA_EFFECT_EXT_SET_DAE_OUTPUT_MODE,
	/** audio effect operation update effect param*/
	MEDIA_EFFECT_EXT_UPDATE_PARAM,

	/** audio effect operation get dae output mode*/
	MEDIA_EFFECT_EXT_GET_DAE_OUTPUT_MODE,
	/** audio effect operation get freq point energy*/
	MEDIA_EFFECT_EXT_GET_FREQPOINT_ENERGY,
	MEDIA_EFFECT_EXT_GET_TIMEDOMAIN_ENERGY,
	MEDIA_EFFECT_EXT_GET_FREQBAND_ENERGY,
	MEDIA_EFFECT_EXT_GET_EFFECT_ENABLE,

	/** audio effect operation set energy freq points */
	MEDIA_EFFECT_EXT_SET_ENERGY_FREQPOINT,

	/** audio effect operation set fade in*/
	MEDIA_EFFECT_EXT_FADEIN,
	MEDIA_EFFECT_EXT_GET_FADEIN_TIME,
	MEDIA_EFFECT_EXT_GET_FADEIN_STATUS,
	/** audio effect operation set fade out*/
	MEDIA_EFFECT_EXT_FADEOUT,
	MEDIA_EFFECT_EXT_GET_FADEOUT_TIME,
	MEDIA_EFFECT_EXT_GET_FADEOUT_STATUS,
	/** audio effect operation check fade out finished*/
	MEDIA_EFFECT_EXT_CHECK_FADEOUT_FINISHED,
	MEDIA_EFFECT_EXT_SET_ENERGY_PARAM,
	/** audio effect operation set subwoofer volume */
	MEDIA_EFFECT_EXT_SET_SUBWOOFER_VOLUME,
	/** audio effect operation set LR balance*/
	MEDIA_EFFECT_EXT_SET_LR_BALANCE,
	MEDIA_EFFECT_EXT_SET_SUBEFFECT,

	MEDIA_EFFECT_EXT_SET_MIC_PARAM,

	MEDIA_SET_RESAMPLE_APS,
	MEDIA_GET_RESAMPLE_APS_CACHE_DATA_LEN,
	MEDIA_SET_RESAMPLE_APS_BT_TIME,

	MEDIA_EFFECT_DYNAMIC_UPDATE_PEQ,

	MEDIA_EFFECT_EXT_SET_SMART_CONTROL_VOLUME,
	MEDIA_SET_RESAMPLE_APS_BT_TIME2,
} media_param_e;

/**media event type*/
typedef enum {
	EVENT_PLAYER_START,
	EVENT_PLAYER_STOP,
	EVENT_PLAYER_ERROR_TYPE,
	EVENT_PLAYER_TYPE,
	EVENT_PLAYER_TOTAL_TIME,
	EVENT_PLAYER_PCM_TIME,
	EVENT_PLAYER_BUFFER_PLAYBACK_IN,
	EVENT_PLAYER_BUFFER_PLAYBACK_DE,
	EVENT_PLAYER_BUFFER_PLAYBACK_RE,
	EVENT_PLAYER_BUFFER_PLAYBACK_OU,
	EVENT_PLAYER_BUFFER_CAPTURE_IN,
	EVENT_PLAYER_BUFFER_CAPTURE_RE,
	EVENT_PLAYER_BUFFER_CAPTURE_OU,
	EVENT_PLAYER_PACKET_LOSS,
	EVENT_PLAYER_ERR_PACKET_LOSS,
	EVENT_PLAYER_DSP_STATUS,
	EVENT_PLAYER_DSP_ERR_CODE,
	EVENT_CAPTURE_DATA_LESS,
	EVENT_AUDIO_ERROR_TYPE,
	EVENT_AUDIO_DATA_LESS,
	EVENT_AUDIO_APS_LEVEL,
} media_event_e;

/* MEDIA_PARAM_OUTPUT_MODE */
typedef enum {
	MEDIA_OUTPUT_MODE_DEFAULT = 0,
	MEDIA_OUTPUT_MODE_LEFT    = 1,
	MEDIA_OUTPUT_MODE_RIGHT   = 2,

	NUM_MEDIA_OUTPUT_MODES,
} media_output_mode_e;

/* MEDIA_EFFECT_EXT_SET_DAE_OUTPUT_MODE */
typedef enum {
	MEDIA_EFFECT_OUTPUT_DEFAULT  = 0,
	MEDIA_EFFECT_OUTPUT_L_R_SWAP = 1,
	MEDIA_EFFECT_OUTPUT_L_R_MIX  = 2,
	MEDIA_EFFECT_OUTPUT_L_ONLY   = 3,
	MEDIA_EFFECT_OUTPUT_R_ONLY   = 4,

	NUM_MEDIA_EFFECT_OUTPUT_MODES,
} media_effect_output_mode_e;

typedef struct {
	/** param type */
	int type;
	/** param buffer */
	void *pbuf;
	/** param buffer length */
	unsigned int plen;
} media_param_t;

typedef struct {
	/** media handle */
	void *handle;
	/** param */
	media_param_t param;
	/** store the result: 0 successful, others failed */
	int result;
} media_srv_param_t;

typedef struct {
	int total_time;  /* total time in milliseconds */
	int avg_bitrate; /* average bit reate in kbps */
	int sample_rate; /* sample rate in Hz */
	int channels;    /* channels */
	unsigned int file_hdr_len;	/* file header(tags) length */
} media_info_t;

typedef struct {
	/** required in ms*/
	int time_offset;
	/** optional, may accelerate the seeking for some codec format (mp3, etc.)
	 * must set <= 0, if not available. in bytes
	 */
	int file_offset;
} media_breakpoint_info_t;

typedef struct {
	int num_points;
	short values[10];
} media_freqpoint_energy_info_t;

#define MEDIA_SEEK_SET SEEK_DIR_BEG
#define MEDIA_SEEK_CUR SEEK_DIR_CUR
#define MEDIA_SEEK_END SEEK_DIR_END

/** media seek info structure*/
typedef struct {
	/** should be one of AP_SEEK_SET, AP_SEEK_CUR and AP_SEEK_END */
	int whence;
	/** required in ms*/
	int time_offset;
	/** optional, may accelerate the seeking for some codec format (mp3, etc.)
	 * must set <= 0, if not available.
	 */
	int file_offset;

	/* return seeking result */
	int chunk_time_offset; /* in ms */
	int chunk_file_offset; /* in bytes */
} media_seek_info_t;

typedef struct {
	/* number of data tags */
	int num;
	/* data tags array */
	const u8_t *tags;
	/* buffer array to stort data */
	struct acts_ringbuf **bufs;
} media_data_dump_info_t;
typedef struct {
	u8_t state;
	int result;
} media_state_info_t;

typedef enum {
	MEDIA_PLAYBACK = 0,
	MEDIA_CAPTURE,
	MEDIA_PARSER,

	NUM_MEDIA_SUBSRVS,
} media_subsrv_index_e;

typedef enum {
	MEDIA_SRV_TYPE_PLAYBACK = BIT(MEDIA_PLAYBACK),
	MEDIA_SRV_TYPE_CAPTURE  = BIT(MEDIA_CAPTURE),
	MEDIA_SRV_TYPE_PLAYBACK_AND_CAPTURE = (MEDIA_SRV_TYPE_PLAYBACK | MEDIA_SRV_TYPE_CAPTURE),
	MEDIA_SRV_TYPE_PARSER   = BIT(MEDIA_PARSER),
	MEDIA_SRV_TYPE_PARSER_AND_PLAYBACK  = (MEDIA_SRV_TYPE_PLAYBACK | MEDIA_SRV_TYPE_PARSER),
} media_srv_sub_type_e;

typedef struct {
	u8_t type;
	u8_t flag;
	u8_t status;
	u8_t prev_status;

	void *subsrv_handles[NUM_MEDIA_SUBSRVS];

#ifdef CONFIG_THREAD_TIMER
	struct thread_timer srv_data_process_timer;
#endif

	/* evnet notify */
	media_srv_event_notify_t event_notify;
	void *user_data;

	/* reference buffer for sub services */
	struct acts_ringbuf *refbuf;

	/* attached background recorder */
	void *bg_recorder;
	u8_t bg_recorder_can_run : 1;
} media_srv_handle_t;

typedef enum {
	MEDIA_AUDIO_DEVICE_TYPE_BR = 0,
	MEDIA_AUDIO_DEVICE_TYPE_CIS ,
	MEDIA_AUDIO_DEVICE_TYPE_BIS ,
	MEDIA_AUDIO_DEVICE_TYPE_USB ,
} media_audio_device_type_e;

typedef struct {
	u8_t validate;
	media_audio_device_type_e type;
	union{
#ifdef CONFIG_BT_MANAGER
		struct bt_audio_chan bt_chan;
		struct bt_broadcast_chan bis_chan;
#endif
		void * usb_audio_chan;
	};
	void * timeline_owner;
} media_audio_device_info_t;

// external output or input audio device sync info
// used for aps and trigger playback or capture
typedef struct {
	media_audio_device_info_t tx_chan;
	media_audio_device_info_t rx_chan;
} media_device_info_t;

typedef struct {
	u8_t type;
	u8_t dumpable:1;
	u8_t support_tws:1;
	u8_t aec_enable:1;
	u8_t auto_mute:1;
	u8_t waitto_start:1;
	u8_t output_ref:1; /*capture output stream is ref stream*/
	u8_t bind_to_capture:1;
	u8_t nav_channel_separate:1;
	u8_t stream_type;
	u8_t efx_stream_type;

	u8_t format;
	u8_t sample_rate; /* kHz */
	u8_t sample_bits;
	u8_t channels;
	u16_t bit_rate; /* kbps */
	u16_t auto_mute_threshold;
	io_stream_t input_stream;
	io_stream_t output_stream;
	io_stream_t output_stream_subwoofer;

	u8_t capture_format;
	/* capture input sample rate (kHz) */
	u8_t capture_sample_rate_input;
	/* capture output sample rate (kHz) */
	u8_t capture_sample_rate_output;
	/* capture input sample bits, 16, 24, etc. */
	u8_t capture_sample_bits;
	/* capture input channels */
	u8_t capture_channels_input;
	/* capture output (encoding) channels */
	u8_t capture_channels_output;
	u16_t capture_bit_rate; /* kbps */
	io_stream_t capture_input_stream;
	io_stream_t capture_output_stream;
	io_stream_t capture_output_stream2;/* two bis */

	io_stream_t ref_stream;  /* ref come frome another capture */

	/** break point of time ms */
	u32_t bp_time_offset;
	/** break point of file offset byte */
	u32_t bp_file_offset;

	media_srv_event_notify_t event_notify_handle;
	/* will be passed to event_notify_handle as parameter "user_data".
	 * If set NULL, it will fallback to the media_player_t returned by media_player_open.
	 */
	void *user_data;

	media_srv_sync_callback_t sync_callback;
	void *sync_data;

	media_device_info_t device_info;
} media_init_param_t;

typedef enum {
	/* transmit and output channeles decided by decoding output channels */
	MEDIA_TWS_MODE_DEFAULT = 0,
	/* mono transmit to slave, mono output on master/slave */
	MEDIA_TWS_MODE_MONO,
	/* (force) stereo transmit to slave, (force) stereo output on master/slave */
	MEDIA_TWS_MODE_STEREO,
} media_tws_mode_e;

typedef struct {
	/** public parameters for application */
	media_init_param_t *user_param;

	/** private parameters for media sub-services */
	void *mediasrv_handle;
#ifdef CONFIG_BT_MANAGER
	tws_runtime_observer_t *tws_observer;
#endif
	u8_t tws_mode; /* see media_tws_mode_e */
	/* whether tws only support samplerate 44 or 48kHz */
	u8_t tws_samplerate_44_48_only;
} media_srv_init_param_t;

#define NUM_MEDIA_RECORDS (MEDIA_CAPTURE + 1)

typedef enum {
	MEDIA_RECORD_PLAYBACK = BIT(MEDIA_PLAYBACK),
	MEDIA_RECORD_CAPTURE = BIT(MEDIA_CAPTURE),
	MEDIA_RECORD_PLAYBACK_AND_CAPTURE = MEDIA_RECORD_PLAYBACK | MEDIA_RECORD_CAPTURE,
} media_record_type_t;

typedef struct {
	/* record type */
	u8_t type;

	/* encoder parameters */
	u8_t format;
	u8_t sample_rate;
	u8_t channels;
	u16_t bit_rate; /* kbps */
	io_stream_t stream;
} media_record_info_t;

/*
 * @struct  effect_f_energy_set_t
 *
 * @brief   Use to enable/disable time and freq energy sample, or set the frequency of the
 *          frequency sample.
 */
typedef struct {
	/** Enable time domain energy sample */
	u16_t t_energy_en;

	uint16_t f_band_energy_frame_cnt;

	/** Enable frequency domain (band mode) energy sample */
	u16_t f_band_energy_en;
	/** Frequency domain (band mode) sample time window width(ms)*/
	u16_t f_band_energy_timewindow_ms;
	/** The count of the bands energy to sample */
	u16_t f_band_energy_num;
	/** Bandwidth in band mode */
	u16_t f_band_bandwidth;
	/** Center frequency of the band 0 */
	u16_t f_band_energy_f0;
	/** Center frequency of the band 1 */
	u16_t f_band_energy_f1;
	/** Center frequency of the band 2 */
	u16_t f_band_energy_f2;
	/** Center frequency of the band 3 */
	u16_t f_band_energy_f3;
	/** Center frequency of the band 4 */
	u16_t f_band_energy_f4;
	/** Center frequency of the band 5 */
	u16_t f_band_energy_f5;
	/** Center frequency of the band 6 */
	u16_t f_band_energy_f6;
	/** Center frequency of the band 7 */
	u16_t f_band_energy_f7;
	/** Center frequency of the band 8 */
	u16_t f_band_energy_f8;
	/** Center frequency of the band 9 */
	u16_t f_band_energy_f9;
	/** Enable frequency domain (point mode) energy sample */
	u16_t f_pt_energy_en;
	/** The count of the points energy to sample */
	u16_t f_pt_energy_num;
	u16_t f_pt_energy[16];
} effect_f_energy_set_t;

/**  lte_process_para_info_t */
typedef struct{
	short lteprocess_enable;	   ///< default:0, 0 for disable, 1 for enable
	short csb_syn_cnt;             ///< reserve,not used


	int   lteprocess_input_sample_rate;            ///< sample_rate, default:44100, step 1 Hz
	short TimeDomainEnergy_enable;      ///< default:0, 0 for disable, 1 for enable


	short MultiFreqBandEnergy_enable;    ///< default:0, 0 for disable, 1 for enable.
	short f_c[20];				         ///< center frequency, for example 100 for 100Hz.we only support 10 points,another 10 points are reserved for future use
	int   Freqwidth_perband;             ///< freq width perband in Hz of center frequency when calculate BandEnergy,for example 100 for 100Hz
	short num_band;						 ///< default: 0, max 10;
	short duration_ms;					 ///< reserve,not used
	short fft_overlap;                   ///< reserve,not used
	short fft_size;                      ///< default:512,supprot 256/512/1024
	int*  bitrev;                        ///< bitrev buffer allocated by caller,buf size is fft_size*sizeof(int).buffer should be aligned by 0x1000

	short FreqSpetrumDisplay_enable;///< default:0, 0 for disable, 1 for enable.when MultiFreqBandEnergy_enable is 0,FreqSpetrumDisplay_enable will be forced to 0
	short freq_point[20];
	short num_freq_point;
	short* params;///< reserve params for future use

} lte_process_para_info_t;


#define MEDIA_SRV_DEVICE_USED_FLAG      (1 << 0)

#define MEDIA_SRV_SET_USED_FLAG(HANDLE, FLAG) (((HANDLE)->flag) |= (FLAG))
#define MEDIA_SRV_CLEAR_USED_FLAG(HANDLE, FLAG) (((HANDLE)->flag) &= ~(FLAG))
#define MEDIA_SRV_IS_FLAG_SET(HANDLE, FLAG) (((HANDLE)->flag & (FLAG)) == (FLAG))

/**
 * @brief open new media service instance
 *
 * This routine provides open new media service instance, if open failed
 * or no more media handle ,return NULL. this operation is sync operation,
 * may block calling thread.
 *
 * @param msg message of open service
 *	msg.type = MSG_MEDIA_SRV_OPEN;
 *	msg.ptr = init_param;
 *	msg.callback = _media_service_open_callback;
 *	msg.sync_sem  sem for sync operation;
 *
 * @return handle of new service instance
 */
void *media_service_open(struct app_msg *msg);

/**
 * @brief start play for media service
 *
 * This routine provides to start meida service play
 *
 * @param msg message of service play
 *	msg.type = MSG_MEDIA_SRV_PLAY;
 *	msg.ptr = handle->media_srv_handle handle of media service ;
 *
 * @return 0 excute successed , others failed
 */
int media_service_play(struct app_msg *msg);

/**
 * @brief triggle media service process data
 *
 * This routine provides to triggle media service process data,
 * always triggle from thread timer
 *
 * @param msg message of service play
 *	msg.type = MSG_MEDIA_SRV_DATA_PROCESS;
 *	msg.ptr = handle->media_srv_handle handle of media service ;
 *
 * @return 0 excute successed , others failed
 */

int media_service_data_process(struct app_msg *msg);

/**
 * @brief pause for media service
 *
 * This routine provides to pause meida service . only excute when meida
 * service playing.
 *
 * @param msg message of service play
 *	msg.type = MSG_MEDIA_SRV_PAUSE;
 *	msg.ptr = handle->media_srv_handle handle of media service ;
 *
 * @return 0 excute successed , others failed
 */
int media_service_pause(struct app_msg *msg);

/**
 * @brief resume for media service
 *
 * This routine provides to resume media service, only excute when meida
 * service paused.
 *
 * @param msg message of service play
 *	msg.type = MSG_MEDIA_SRV_RESUME;
 *	msg.ptr = handle->media_srv_handle handle of media service ;
 *
 * @return 0 excute successed , others failed
 */
int media_service_resume(struct app_msg *msg);

/**
 * @brief seek for media service
 *
 * This routine provides to seek meida service
 *
 * @param msg message of service play
 *	msg.type = MSG_MEDIA_SRV_SEEK
 *	msg.ptr = param seek param @see media_srv_seek_param_t;
 *
 * @return 0 excute successed , others failed
 */
int media_service_seek(struct app_msg *msg);

/**
 * @brief stop media service
 *
 * This routine provides to stop meida service, if media service stop
 *  inner process must be stoped, but resource of media service may not
 *  free in this function .
 *
 * @param msg message of service stop
 *	msg.type = MSG_MEDIA_SRV_STOP;
 *	msg.ptr = handle->media_srv_handle handle of media service ;
 *
 * @return 0 excute successed , others failed
 */
int media_service_stop(struct app_msg *msg);

/**
 * @brief close media service handle
 *
 * This routine provides to close media service handle, media service must
 * release all resource of this media servie handle,and put the handle to
 * free media service handle pool.
 *
 * @param msg message of service close
 *	msg.type = MSG_MEDIA_SRV_CLOSE;
 *	msg.ptr = handle->media_srv_handle handle of media service ;
 *
 * @return 0 excute successed , others failed
 */
int media_service_close(struct app_msg *msg);

/**
 * @brief dump media service data
 *
 * This routine provides to dump media service inner data for debug or
 *  for other debug tools , such as asqt ,aset ,ectt.
 *
 * @param msg message of service dump data
	msg.type = MSG_MEDIA_SRV_DUMP_DATA;
	msg.ptr = pointer to media_srv_data_dump_param_t
 *
 * @return 0 excute successed , others failed
 */
int media_service_dump_data(struct app_msg *msg);

/**
 * @brief get media service parameter
 *
 * This routine provides to get media service parameter.
 *
 * @param msg message of service dump data
	msg.type = MSG_MEDIA_SRV_GET_PARAMETER;
	msg.ptr = pointer to media_srv_param_t
 *
 * @return 0 excute successed , others failed
 */
int media_service_get_parameter(struct app_msg *msg);

/**
 * @brief set media service parameter
 *
 * This routine provides to set media service parameter.
 *
 * @param msg message of service dump data
	msg.type = MSG_MEDIA_SRV_SET_PARAMETER;
	msg.ptr = pointer to media_srv_param_t
 *
 * @return 0 excute successed , others failed
 */
int media_service_set_parameter(struct app_msg *msg);

/**
 * @brief get media service global parameter
 *
 * This routine provides to get media service global parameter.
 *
 * @param msg message of service dump data
	msg.type = MSG_MEDIA_SRV_GET_GLOBAL_PARAMETER;
	msg.ptr = pointer to media_srv_param_t
 *
 * @return 0 excute successed , others failed
 */
int media_service_get_global_parameter(struct app_msg *msg);

/**
 * @brief set media service global parameter
 *
 * This routine provides to set media service global parameter.
 *
 * @param msg message of service dump data
	msg.type = MSG_MEDIA_SRV_SET_GLOBAL_PARAMETER;
	msg.ptr = pointer to media_srv_param_t
 *
 * @return 0 excute successed , others failed
 */
int media_service_set_global_parameter(struct app_msg *msg);

int codec_get_overlay_id(enum media_type format, enum codec_type type);

void *codec_get_ops(enum media_type format, enum codec_type type);

bool codec_support_hw_acceleration(enum media_type format, enum codec_type type);

const char *codec_get_hw_acceleration_dec_lib(enum media_type format, enum codec_type type);

const char *codec_get_hw_acceleration_effect_lib(enum media_type format, enum codec_type type);


/**
 * INTERNAL_HIDDEN @endcond
 */
/**
 * @} end defgroup media_service_apis
 */
#endif /* __MEDIA_SERVICE_H__ */
