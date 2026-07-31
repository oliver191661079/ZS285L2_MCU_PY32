/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file Media memory interface
 */
#ifndef __MEDIA_MEMORY_H__
#define __MEDIA_MEMORY_H__

/**
 * @defgroup media_memory_apis Media Memory APIs
 * @ingroup media_system_apis
 * @{
 */

/** ---media service global memory type--- */
typedef enum
{
	/** playback input stream buffer */
	INPUT_PLAYBACK,

	/** capture input stream buffer */
	INPUT_CAPTURE,

	/** playback output stream buffer, after decode and mix*/
	OUTPUT_PLAYBACK,

	/** capture output stream buffer, after aec and encode*/
	OUTPUT_CAPTURE,

	/** decoder output stream buffer*/
	OUTPUT_DECODER,

	/** decoder output resample buffer*/
	OUTPUT_RESAMPLE,

	/** encoder input stream buffer */
	INPUT_ENCBUF,

	/** AEC reference stream buffer */
	AEC_REFBUF0,

	/** audiostream internal input pcm buffer */
	INPUT_PCM,

	/** audiostream internal output pcm buffer */
	OUTPUT_PCM,

	/** audiostream internal output pcm buffer */
	INPUT_RESAMPLE,

	/* ---bt stream global memory--- */

	/** bt upload sco send cache buffer */
	OUTPUT_SCO,

	/** bt upload sco send temp buffer */
	TX_SCO,

	/** bt upload sco send cache buffer */
	RX_SCO,

	/** ---other global memory--- */
	SBC_DECODER_GLOBAL_DATA,
	SBC_DECODER_SHARE_DATA,

	SBC_ENCODER_GLOBAL_DATA,
	SBC_ENCODER_SHARE_DATA,

	SBC_ENCODER_2_GLOBAL_DATA,
	SBC_ENCODER_2_SHARE_DATA,

	DECODER_GLOBAL_DATA,
	DECODER_SHARE_DATA,

	ENCODER_GLOBAL_DATA,
	ENCODER_SHARE_DATA,

	USB_UPLOAD_CACHE,
	USB_UPLOAD_PAYLOAD,

	TWS_LOCAL_INPUT, /* tws local stream */

	TTS_STACK,
	CODEC_STACK,

	TOOL_ASQT_STUB_BUF,
	TOOL_ASQT_DUMP_BUF,
	TOOL_ECTT_BUF,

	/* libraries's exported buffer */
	AEC_GLOBAL_DATA,
	AEC_SHARE_DATA,
	PLC_GLOBAL_DATA,
	PLC_SHARE_DATA,
	RESAMPLE_GLOBAL_DATA,
	RESAMPLE_SHARE_DATA,
	RESAMPLE_FRAME_DATA,
	PARSER_STACK,
	PARSER_CHUCK,
	OUTPUT_PARSER,
	INPUT_ATTACHPCM,

	/*okmic dae memory*/
	DAE_PCM0,
	DAE_PCM1,
	DAE_PCM2,
	DAE_MUSIC_SHARE_DATA,
	DAE_MUSIC_1_GLOBAL_DATA,
	DAE_MUSIC_2_GLOBAL_DATA,
	DAE_MIC_1_GLOBAL_DATA,
	DAE_MIC_2_GLOBAL_DATA,
	DAE_VIRTUALBASS_DATA,
	DAE_ECHO_DATA1,
	DAE_ECHO_DATA2,
	DAE_ECHO_DATA3,
	DAE_ECHO_DATA4,
	DAE_ECHO_DATA5,
	DAE_FREQ_SHIFT_DATA,
	DAE_PITCH_SHIFT_DATA,
	DAE_REVERB_DATA1,
	DAE_REVERB_DATA2,
	DAE_REVERB_DATA3,
	DAE_NHS_DATA,

	OUTPUT_PKG_HDR,

	CACHE_PCM,
	INPUT_CAPTURE2,
	OUTPUT_CAPTURE2,
	INPUT_RESAMPLE2,
	INPUT_ENCBUF2,
	ENERGY_BUFFER,

	OTA_UPGRADE_BUF,
	OTA_THREAD_STACK,

	DAE_PARAM_DATA,
	OUTPUT_RESAMPLE2,
} cache_pool_type_e;

/**
 * @brief get cache mem pool address
 *
 * This routine provides to get target memory addr
 *
 * @param mem_type memory type @see cache_pool_type_e
 * @param stream_type indicator for meida sence
 *
 * @return pointer of mem addr
 */
void *media_mem_get_cache_pool(int mem_type, int stream_type);

/**
 * @brief get cache mem pool size
 *
 * This routine provides to get target memory size
 *
 * @param mem_type memory type @see cache_pool_type_e
 * @param stream_type indicator for meida sence
 *
 * @return size of memory
 */
int media_mem_get_cache_pool_size(int mem_type, int stream_type);


int media_mem_get_codec_frame_size(int format, int channel, int sample_rate_khz, int bit_rate_kbps,
	int interval_us, int sample_bits);
int media_mem_get_cache_pool_size_ext(int mem_type, int stream_type, int format, int block_size, int block_num);
/**
 * @} end defgroup media_memory_apis
 */
#endif /* __MEDIA_MEMORY_H__ */
