/*
 * Copyright (c) 2016 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief audio policy.
*/

#include <os_common_api.h>
#include <mem_manager.h>
#include <msg_manager.h>
#include <audio_system.h>
#include <audio_policy.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <audio_device.h>
#include <media_type.h>
#include <btservice_api.h>

#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "audio policy"
#include <logging/sys_log.h>

#ifdef CONFIG_MUSIC_EXTERNAL_EFFECT
#define FULL_RANGE_DA_VALUE 0
#else
#define FULL_RANGE_DA_VALUE 0x40000000
#endif
#define FULL_RANGE_PA_VALUE 0x28

#define MAX_SBC_DYNAMIC_TIME  (90)
#define MAX_AAC_DYNAMIC_TIME  (400)
#define GET_LIMIT_VALUE(a, limit)    (a > limit ? limit : a)

static const struct audio_policy_t *user_policy = NULL;

struct audio_policy_dynamic_t {
	u16_t nav_frame_size_us; //10ms 10000 (default) 7.5ms 7500 5ms 5000 2.5ms 2500
	u16_t bis_link_delay_ms;
	u16_t lea_start_threshold;
	int16 smart_gain;
	u16_t dynamic_time;
	u16_t user_dynamic_time;
};
static struct audio_policy_dynamic_t dynamic_policy = { 0, 0, 0, 0, 0};

#if 0
static int  pa_to_mdb(short pa)
{
	int mdb;

	//pa to mdB
	if(pa >= 40) {
		mdb = 0;
	} else if (pa >= 27) {
		mdb = 1000* (40 - pa);
	} else if (pa >= 14) {
		mdb = 1000* (40 - 27) + 1500*(27-pa);
	} else if (pa == 13) {
		mdb = 35000;
	} else if (pa >= 1) {
		mdb = 35000 + 2000*(13-pa);
	}else{
		mdb = 97000;
	}

	return -mdb;
}
#endif

timeline_t *audio_policy_get_playback_timeline(uint8_t stream_type, u8_t format,void* rx_timeline_owner)
{
	if (stream_type == AUDIO_STREAM_LE_AUDIO) {
		return timeline_get_by_type(TIMELINE_TYPE_AUDIO_TX);
	}

	if (stream_type == AUDIO_STREAM_USOUND && format == NAV_TYPE) {
		return timeline_get_by_type_and_onwer(TIMELINE_TYPE_BLUETOOTH_LE_RX,rx_timeline_owner);
	}

	return NULL;
}

timeline_t *audio_policy_get_capture_timeline(uint8_t stream_type, u8_t format,void* tx_timeline_owner)
{
	return NULL;
}

timeline_t *audio_policy_get_decode_timeline(uint8_t stream_type, u8_t format,void* rx_timeline_owner)
{
	if (stream_type == AUDIO_STREAM_LE_AUDIO) {
		return timeline_get_by_type_and_onwer(TIMELINE_TYPE_MEDIA_DECODE,rx_timeline_owner);
	}

	if (stream_type == AUDIO_STREAM_USOUND && format == NAV_TYPE) {
		return timeline_get_by_type_and_onwer(TIMELINE_TYPE_MEDIA_DECODE,rx_timeline_owner);
	}

	return NULL;
}

timeline_t *audio_policy_get_encode_timeline(uint8_t stream_type, u8_t format,void* tx_timeline_owner)
{
	if (stream_type == AUDIO_STREAM_LE_AUDIO) {
		return timeline_get_by_type_and_onwer(TIMELINE_TYPE_MEDIA_ENCODE,tx_timeline_owner);
	}

	if ((stream_type == AUDIO_STREAM_USOUND
		|| stream_type == AUDIO_STREAM_SOUNDBAR
		|| stream_type == AUDIO_STREAM_LINEIN
		|| stream_type == AUDIO_STREAM_I2SRX_IN
		|| stream_type == AUDIO_STREAM_SPDIF_IN
		|| stream_type == AUDIO_STREAM_LOCAL_MUSIC)
		&& format == NAV_TYPE) {
		return timeline_get_by_type_and_onwer(TIMELINE_TYPE_MEDIA_ENCODE,tx_timeline_owner);
	}

	return NULL;
}

int audio_policy_get_aps_type(uint8_t stream_type, uint8_t format, uint8_t is_playback, uint8_t is_tws)
{
	if (stream_type == AUDIO_STREAM_LE_AUDIO) {
		if(is_playback)
			return APS_TYPE_PLAYBACK|APS_TYPE_SOFT|APS_TYPE_INTERRUPT;
		else
			return APS_TYPE_CAPTURE|APS_TYPE_SOFT|APS_TYPE_INTERRUPT;
	} else if (stream_type == AUDIO_STREAM_SOUNDBAR && format == NAV_TYPE) {
#ifdef CONFIG_SOUNDBAR_SOURCE_BR
		if(is_playback)
			return APS_TYPE_PLAYBACK|APS_TYPE_SOFT|APS_TYPE_BUFFER;
		else
			return APS_TYPE_CAPTURE|APS_TYPE_SOFT|APS_TYPE_BUFFER;
#else
		if(is_playback)
			return APS_TYPE_PLAYBACK|APS_TYPE_HARDWARE_BT|APS_TYPE_INTERRUPT;
		else
			return APS_TYPE_CAPTURE|APS_TYPE_HARDWARE_BT|APS_TYPE_INTERRUPT;
#endif
	} else if ((stream_type == AUDIO_STREAM_USOUND  || stream_type == AUDIO_STREAM_LOCAL_MUSIC )&& format == NAV_TYPE) {
		if(is_playback)
			return APS_TYPE_PLAYBACK|APS_TYPE_SOFT|APS_TYPE_BUFFER;
		else
			return APS_TYPE_CAPTURE|APS_TYPE_SOFT|APS_TYPE_BUFFER;
	} else if (stream_type == AUDIO_STREAM_SOUNDBAR && (format == AAC_TYPE || format == SBC_TYPE) && is_playback){
		return APS_TYPE_PLAYBACK|APS_TYPE_SOFT|APS_TYPE_BUFFER;
	} else if (stream_type == AUDIO_STREAM_MUSIC || stream_type == AUDIO_STREAM_TTS) {
		if (stream_type == AUDIO_STREAM_MUSIC && is_tws){
			if(is_playback)
				return APS_TYPE_PLAYBACK|APS_TYPE_HARDWARE_AUDIO|APS_TYPE_BUFFER;
			else
				return APS_TYPE_CAPTURE|APS_TYPE_HARDWARE_AUDIO|APS_TYPE_BUFFER;
		} else {
			if(is_playback)
				return APS_TYPE_PLAYBACK|APS_TYPE_SOFT|APS_TYPE_BUFFER;
			else
				return APS_TYPE_CAPTURE|APS_TYPE_SOFT|APS_TYPE_BUFFER;
		}
	} else{
		if(is_playback)
			return APS_TYPE_PLAYBACK|APS_TYPE_SOFT|APS_TYPE_BUFFER;
		else
			return APS_TYPE_CAPTURE|APS_TYPE_SOFT|APS_TYPE_BUFFER;
	}
}

int audio_policy_get_out_channel_type(uint8_t stream_type)
{
	int out_channel = AUDIO_CHANNEL_DAC;

	if (user_policy)
		out_channel = user_policy->audio_out_channel;

	if (stream_type == AUDIO_STREAM_SUBWOOFER) {
#ifdef CONFIG_AUDIO_SUBWOOFER
#ifdef CONFIG_AUDIO_SUBWOOFER_USE_I2S
		out_channel = AUDIO_CHANNEL_I2STX;
#else
		out_channel = AUDIO_CHANNEL_DAC;
#endif
#endif

	}
	return out_channel;
}

int audio_policy_get_out_channel_mode(uint8_t stream_type)
{
	int out_channel_mode = AUDIO_DMA_MODE | AUDIO_DMA_RELOAD_MODE;

	switch (stream_type) {
		case AUDIO_STREAM_TTS:
			//out_channel_mode = AUDIO_ASRC_MODE;
			break;
		case AUDIO_STREAM_LINEIN:
		case AUDIO_STREAM_SPDIF_IN:
		case AUDIO_STREAM_FM:
		case AUDIO_STREAM_I2SRX_IN:
		case AUDIO_STREAM_MIC_IN:
		case AUDIO_STREAM_MUSIC:
		case AUDIO_STREAM_SOUNDBAR:
		case AUDIO_STREAM_LOCAL_MUSIC:
		case AUDIO_STREAM_USOUND:
		case AUDIO_STREAM_VOICE:
		case AUDIO_STREAM_SUBWOOFER:
			out_channel_mode = AUDIO_DMA_MODE | AUDIO_DMA_RELOAD_MODE;
			break;
	}
	return out_channel_mode;
}
int audio_policy_get_out_pcm_channel_num(uint8_t stream_type)
{
	int channel_num = 0;

	switch (stream_type) {
	case AUDIO_STREAM_TTS:
		break;
	case AUDIO_STREAM_VOICE:
		break;

	case AUDIO_STREAM_MIC_IN:
		break;
	default: /* decoder decided */
		channel_num = 0;
		break;
	}

	if ( (user_policy && (user_policy->audio_out_channel & AUDIO_CHANNEL_I2STX))
	  || (user_policy && (user_policy->audio_out_channel & AUDIO_CHANNEL_SPDIFTX))) {
		//if (audio_policy_get_out_channel_id(stream_type) != AOUT_FIFO_DAC0) {
		//	 channel_num = 2;
	  	//}
#ifdef CONFIG_I2STX_SLAVE_MCLKSRC_INTERNAL
		if (user_policy->audio_out_channel == AUDIO_CHANNEL_I2STX) {
			channel_num = 0;
		}
#endif
	}

	return channel_num;
}


int audio_policy_get_out_channel_id(uint8_t stream_type)
{
	int out_channel_id = AOUT_FIFO_DAC0;

	//if (stream_type == AUDIO_STREAM_TTS)
	//	out_channel_id = AOUT_FIFO_DAC1;
#ifdef CONFIG_I2STX_SLAVE_MCLKSRC_INTERNAL
	if (user_policy && (user_policy->audio_out_channel == AUDIO_CHANNEL_I2STX)) {
		out_channel_id = AOUT_FIFO_I2STX0;
	}
#endif

#ifdef CONFIG_AUDIO_OUTPUT_I2S
	out_channel_id = AOUT_FIFO_I2STX0;
#endif

	if (stream_type == AUDIO_STREAM_SUBWOOFER) {
#ifdef CONFIG_AUDIO_SUBWOOFER
#ifdef CONFIG_AUDIO_SUBWOOFER_USE_I2S
		out_channel_id = AOUT_FIFO_I2STX0;
#else
		out_channel_id = AOUT_FIFO_DAC0;
#endif
#endif
	}

	return out_channel_id;
}

int audio_policy_get_out_pcm_frame_size(uint8_t stream_type)
{
	int frame_size = 512;

	switch (stream_type) {
		case AUDIO_STREAM_TTS:
			frame_size = 4 * 960;
			break;
		case AUDIO_STREAM_USOUND:
		case AUDIO_STREAM_LINEIN:
		case AUDIO_STREAM_FM:
		case AUDIO_STREAM_I2SRX_IN:
		case AUDIO_STREAM_MIC_IN:
		case AUDIO_STREAM_SPDIF_IN:
		case AUDIO_STREAM_MUSIC:
		case AUDIO_STREAM_SOUNDBAR:
		case AUDIO_STREAM_LOCAL_MUSIC:
			frame_size = 2048;
			break;
		case AUDIO_STREAM_VOICE:
			frame_size = 512;
			break;
		default:
			break;
	}

	return frame_size;
}

int audio_policy_get_out_input_start_threshold(uint8_t stream_type, int format, uint8_t exf_stream_type, uint8_t sample_rate, uint8_t channels, uint8_t tws_mode)
{
	int th = 0;

	switch (stream_type) {
	case AUDIO_STREAM_SOUNDBAR:
		if (exf_stream_type == AUDIO_STREAM_MUSIC) {
			/* limit avoid input buffer overfow */
			th = 220 + GET_LIMIT_VALUE(audio_policy_get_bis_link_delay_ms(), 50);
			if (format == AAC_TYPE)
				th += GET_LIMIT_VALUE(audio_policy_get_dynamic_waterlevel_ms(), MAX_AAC_DYNAMIC_TIME);
			else
				th += GET_LIMIT_VALUE(audio_policy_get_dynamic_waterlevel_ms(), MAX_SBC_DYNAMIC_TIME);
		}
		break;
	case AUDIO_STREAM_MUSIC:
		if (exf_stream_type == AUDIO_STREAM_MUSIC) {
		#ifdef CONFIG_TWS
			if (!btif_tws_check_is_woodpecker()
					&& btif_tws_get_dev_role() == BTSRV_TWS_SLAVE) {
				th = 6;
			} else
		#endif
			{
				if (system_check_low_latencey_mode()) {
					th = 100;
				} else {
					th = 210;
				}
			}
		} else if (exf_stream_type == AUDIO_STREAM_USOUND) {
			th = 32;
		} else if (exf_stream_type == AUDIO_STREAM_LINEIN) {
			/* For us281b linein tws is latency, need start with samll threshold */
			/* return 12 * 119; */
			th = 6;
		} else if (exf_stream_type == AUDIO_STREAM_FM) {
			th = 32;
		} else if (exf_stream_type == AUDIO_STREAM_LOCAL_MUSIC) {
			th = 60;
		} else if (exf_stream_type == AUDIO_STREAM_SOUNDBAR) {
			th = 6;
		} else {
			th = 0;
		}
		break;
	case AUDIO_STREAM_USOUND:
		th = 11;
		break;
	case AUDIO_STREAM_VOICE:
		if (system_check_low_latencey_mode()) {
			th = 0;
		} else {
#ifndef CONFIG_AUDIO_VOICE_HARDWARE_REFERENCE_EXTERNAL
			return 50;
#else
			return 60;
#endif
		}
		break;
	case AUDIO_STREAM_FM:
		th = 0;
		break;
	case AUDIO_STREAM_SPDIF_IN:
	case AUDIO_STREAM_LINEIN:
	case AUDIO_STREAM_I2SRX_IN:
		th = 10;
		break;
	case AUDIO_STREAM_TTS:
	default:
		th = 0;
		break;
	}

	return th;
}

int audio_policy_get_out_input_stop_threshold(uint8_t stream_type, int format, uint8_t exf_stream_type, uint8_t sample_rate, uint8_t channels, uint8_t tws_mode)
{
	int th = 0;

	switch (stream_type)
	{
	case AUDIO_STREAM_MUSIC:
	case AUDIO_STREAM_SOUNDBAR:
		if (exf_stream_type == AUDIO_STREAM_LINEIN) {
			th = 0;
		} else {
			th = 2;
		}
		break;
	case AUDIO_STREAM_VOICE:
	case AUDIO_STREAM_TTS:
	default:
		th = 0;
		break;
	}

	return th;
}

int audio_policy_get_out_audio_mode(uint8_t stream_type)
{
	int audio_mode = AUDIO_MODE_STEREO;

	switch (stream_type) {
		/**283D tts is mono ,but zs285A tts is stereo*/
		/* case AUDIO_STREAM_TTS: */
		case AUDIO_STREAM_VOICE:
		case AUDIO_STREAM_LOCAL_RECORD:
		case AUDIO_STREAM_GMA_RECORD:
		case AUDIO_STREAM_MIC_IN:
		audio_mode = AUDIO_MODE_MONO;
		break;
	}

	return audio_mode;
}

uint8_t audio_policy_get_out_effect_type(uint8_t stream_type,
			uint8_t efx_stream_type, bool is_tws)
{
	switch (stream_type) {
	case AUDIO_STREAM_LOCAL_MUSIC:
		if (is_tws)
			efx_stream_type = AUDIO_STREAM_TWS;
		break;
	case AUDIO_STREAM_MUSIC:
	case AUDIO_STREAM_SOUNDBAR:
		if (is_tws && efx_stream_type == AUDIO_STREAM_LINEIN) {
			efx_stream_type = AUDIO_STREAM_LINEIN;
		}
		break;
	default:
		break;
	}

	return efx_stream_type;
}

uint8_t audio_policy_get_out_effect_support_dynamic_peq(uint8_t stream_type)
{
	uint8_t support = 0;
#ifdef CONFIG_AUDIO_SUPPORT_DYNAMIC_PEQ
	switch (stream_type) {
		case AUDIO_STREAM_LOCAL_MUSIC:
		case AUDIO_STREAM_MUSIC:
		case AUDIO_STREAM_SOUNDBAR:
			support = 1;
			break;

		default:
			break;
	}
#endif
	return support;
}

#define AUDIO_PEQ_IDX_TREBLE	16U
#define AUDIO_PEQ_IDX_BASS	15U

static eq_arr_t dy_peq[] = {
	/* [0] 高音：shell dump_update_peq 仍写 dy_peq[0] */
	{AUDIO_PEQ_IDX_TREBLE, {8000, 70, 0, 1}},
	/* [1] 低音 */
	{AUDIO_PEQ_IDX_BASS, {200, 70, 0, 1}},
};

uint8_t audio_policy_dynamic_update_peq(u8_t * arr)
{
	if(arr)
	{
		for(u8_t i = 0; i < ARRAY_SIZE(dy_peq); i++)
		{
			u8_t* ptr = arr + 0xc0 + (dy_peq[i].index - 1)*sizeof(eq_band_t);
			memcpy(ptr, (void *)&dy_peq[i].eq, sizeof(eq_band_t));
		}
	}

	return 0;
}

int audio_policy_set_dynamic_peq_band(u8_t index, eq_band_t *eq)
{
	u8_t i;

	if (!eq || index == 0U) {
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(dy_peq); i++) {
		if (dy_peq[i].index == index) {
			memcpy((void *)&dy_peq[i].eq, (void *)eq, sizeof(eq_band_t));
			return 0;
		}
	}

	SYS_LOG_WRN("peq index %u not found\n", index);
	return -1;
}

int audio_policy_set_dynamic_peq_info(eq_band_t * eq)
{
	if (eq) {
		memcpy((void *)&dy_peq[0].eq, (void *)eq, sizeof(eq_band_t));
		return 0;
	}

	SYS_LOG_WRN("info err!!\n");
	return -1;
}

int audio_policy_get_output_support_get_energy(u8_t stream_type)
{
	int support_get_energy = false;

#ifdef CONFIG_ENERGY_SAMPLE_SUPPORT
	switch (stream_type){
		case AUDIO_STREAM_MUSIC:
		case AUDIO_STREAM_LINEIN:
		case AUDIO_STREAM_SPDIF_IN:
		case AUDIO_STREAM_I2SRX_IN:
		case AUDIO_STREAM_USOUND:
		case AUDIO_STREAM_SOUNDBAR:
		case AUDIO_STREAM_LE_AUDIO:
		case AUDIO_STREAM_LOCAL_MUSIC:
			support_get_energy = true;
			break;
	}
#endif
	return support_get_energy;
}

int audio_policy_postprocessor_use_input_sample_rate(uint8_t stream_type)
{
	int use_input_sample_rate = false;

	switch (stream_type){
		case AUDIO_STREAM_VOICE:
			use_input_sample_rate = true;
			break;
	}

	return use_input_sample_rate;
}

uint8_t audio_policy_get_record_effect_type(uint8_t stream_type,
			uint8_t efx_stream_type)
{
	return efx_stream_type;
}

int audio_policy_get_record_audio_mode(uint8_t stream_type)
{
	int audio_mode = AUDIO_MODE_STEREO;

	switch (stream_type) {
#ifndef CONFIG_AUDIO_VOICE_HARDWARE_REFERENCE
		case AUDIO_STREAM_VOICE:
#endif
		case AUDIO_STREAM_LOCAL_RECORD:
		case AUDIO_STREAM_GMA_RECORD:
		case AUDIO_STREAM_MIC_IN:
		audio_mode = AUDIO_MODE_MONO;
		break;
	}

	return audio_mode;
}

int audio_policy_get_record_channel_id(uint8_t stream_type)
{
	int channel_id = AIN_LOGIC_SOURCE_MIC0; //AUDIO_ANALOG_MIC0;

	switch (stream_type) {
	case AUDIO_STREAM_LE_AUDIO:
		// channel_id = AIN_LOGIC_SOURCE_MIC0;
		// break;
#if 0
		channel_id = AIN_LOGIC_SOURCE_LINEIN; //AUDIO_LINE_IN0;
		break;
#endif
	case AUDIO_STREAM_VOICE:
#ifdef CONFIG_AUDIO_VOICE_HARDWARE_REFERENCE
		channel_id = AIN_LOGIC_SOURCE_MIC0;
		break;
#endif
	case AUDIO_STREAM_USOUND:
	case AUDIO_STREAM_LOCAL_RECORD:
	case AUDIO_STREAM_GMA_RECORD:
	case AUDIO_STREAM_MIC_IN:
	case AUDIO_STREAM_MUSIC:
	case AUDIO_STREAM_LOCAL_MUSIC:
		channel_id = AIN_LOGIC_SOURCE_MIC1; //AUDIO_ANALOG_MIC0;
		break;
	case AUDIO_STREAM_LINEIN:
	case AUDIO_STREAM_SOUNDBAR:
		channel_id = AIN_LOGIC_SOURCE_LINEIN; //AUDIO_LINE_IN0;
		break;
	case AUDIO_STREAM_FM:
		channel_id = AIN_LOGIC_SOURCE_FM; //AUDIO_ANALOG_FM0;
		break;
	}
	return channel_id;
}



u16_t audio_policy_get_record_pcm_buff_size(u8_t stream_type, int sr, uint8_t audio_mode, uint8_t format)
{
	u16_t size;
	//SYS_LOG_INF("%d, %d, %d, %d", stream_type, sr, audio_mode, format);

	//TODO: support for 44.1k
	if (stream_type == AUDIO_STREAM_SPDIF_IN
		|| stream_type == AUDIO_STREAM_LINEIN
		|| stream_type == AUDIO_STREAM_I2SRX_IN
		|| stream_type == AUDIO_STREAM_LE_AUDIO
		|| stream_type == AUDIO_STREAM_SOUNDBAR
		|| stream_type == AUDIO_STREAM_I2SRX_IN
		)
	{
		if (audio_mode == AUDIO_MODE_MONO) {
			size = sr;
		} else {
			size =  sr * 2; //2 channels
		}
		if(format == AUDIO_FORMAT_PCM_32_BIT) {
			size *= 4;
		} else {
			size *= 2;
		}

		// time of ms * 2 for dma dest pingpong buffer.
		if (stream_type == AUDIO_STREAM_SPDIF_IN
			|| stream_type == AUDIO_STREAM_LINEIN
			|| stream_type == AUDIO_STREAM_I2SRX_IN) {

			if (sr > 48) {
				//malloc may err when size too large
				size *= 2;
			} else {
				size *= 2 * 2;
			}
		}
	} else {
		if (system_check_low_latencey_mode()) {
			size = (sr <= 16) ? 256 : 512;
		} else {
			size = (sr <= 16) ? 512 : 1024;
		}
		if (sr > 48) {
			size *= 2;
		}
	}

	//SYS_LOG_INF("size %d", pcm_buff_size);
	return size;
}

u16_t audio_policy_get_track_pcm_buff_size(u8_t stream_type, int sample_rate, uint8_t audio_mode, uint8_t format)
{
	u16_t pcm_buff_size;

	if (stream_type == AUDIO_STREAM_LE_AUDIO || stream_type == AUDIO_STREAM_SOUNDBAR) {
		pcm_buff_size = sample_rate * 2 * 2 *2;//to do for 44.1k
		if (stream_type == AUDIO_STREAM_SOUNDBAR)
			pcm_buff_size *= 2;

		if (audio_mode == AUDIO_MODE_MONO)
			pcm_buff_size /= 2;

		if(format == AUDIO_FORMAT_PCM_32_BIT) {
			pcm_buff_size *= 2;
		}
	} else if (stream_type == AUDIO_STREAM_SPDIF_IN
		|| stream_type == AUDIO_STREAM_LINEIN
		|| stream_type == AUDIO_STREAM_I2SRX_IN
		|| stream_type == AUDIO_STREAM_USOUND) {
		//TODO: support for 44.1k
		pcm_buff_size =  sample_rate * 2 * 2 * 2 * 2;

		if (audio_mode == AUDIO_MODE_MONO) {
			pcm_buff_size /= 2;
		}
		if(format == AUDIO_FORMAT_PCM_32_BIT) {
			pcm_buff_size *= 2;
		}
	} else {
		if (system_check_low_latencey_mode()) {
			pcm_buff_size = (sample_rate <= 16) ? 256 : 512;
		} else {
			pcm_buff_size = (sample_rate <= 16) ? 512 : 1024;
		}

		if (sample_rate > 48 && format == AUDIO_FORMAT_PCM_32_BIT) {
			pcm_buff_size *= 2;
		}
	}

	return pcm_buff_size;
}

int audio_policy_is_master_mix_channel(uint8_t stream_type)
{
    if(stream_type == AUDIO_STREAM_LINEIN_MIX
		|| stream_type == AUDIO_STREAM_USOUND_MIX){
        return true;
    }else{
        return false;
    }
}

int audio_policy_get_record_aec_block_size(uint8_t format_type)
{
	int block_size = 0;
/*	if (format_type == MSBC_TYPE) {
		block_size = 256 * 2 - 192;
	}else if (format_type == CVSD_TYPE){
		block_size = 256 * 2 - 128;
	}*/

	return block_size;
}
int audio_policy_get_record_channel_mix_channel(uint8_t stream_type)
{
    int mix_channel = false;

    switch (stream_type){
        case AUDIO_STREAM_LINEIN_MIX:
            mix_channel = true;
            break;
        case AUDIO_STREAM_USOUND_MIX:
            mix_channel = true;
            break;
    }

    return mix_channel;
}

int audio_policy_get_record_adc_gain(uint8_t stream_type)
{
	int gain = 0;

	return gain;
}

int audio_policy_get_record_input_gain(uint8_t stream_type)
{
	int gain = 0; /* 0db */

	switch (stream_type)
	{
	case AUDIO_STREAM_LINEIN:
	case AUDIO_STREAM_SOUNDBAR:
			if (user_policy)
				gain = user_policy->audio_in_linein_gain;
			break;
	case AUDIO_STREAM_FM:
			if (user_policy)
				gain = user_policy->audio_in_fm_gain;
			break;
	case AUDIO_STREAM_USOUND:
	case AUDIO_STREAM_VOICE:
	case AUDIO_STREAM_LE_AUDIO:
	case AUDIO_STREAM_LOCAL_RECORD:
	case AUDIO_STREAM_GMA_RECORD:
	case AUDIO_STREAM_MIC_IN:
	case AUDIO_STREAM_AI:
			if (user_policy) {
				gain = user_policy->audio_in_mic_gain;
			}
			break;
	}

#ifndef CONFIG_VOICE_EFFECT
	//for usound upload stream audio quality test.
	if (stream_type == AUDIO_STREAM_USOUND) {
		gain = 260; //26dB
	}
#endif

	SYS_LOG_INF("gain %d", gain);
	return gain;
}

int audio_policy_get_record_channel_mode(uint8_t stream_type)
{
	int channel_mode =  AUDIO_DMA_MODE | AUDIO_DMA_RELOAD_MODE;

	switch (stream_type) {

	}

	return channel_mode;
}

int audio_policy_get_record_channel_support_aec(uint8_t stream_type)
{
	int support_aec = false;

	switch (stream_type) {
		case AUDIO_STREAM_VOICE:
		/* case AUDIO_STREAM_LE_AUDIO: */
		support_aec = true;
		break;
	}

    return support_aec;
}

int audio_policy_get_record_channel_aec_tail_length(uint8_t stream_type, uint8_t sample_rate, bool in_debug)
{
	switch (stream_type) {
	case AUDIO_STREAM_VOICE:
		if (in_debug) {
			return (sample_rate > 8) ? 32 : 64;
		} else if (user_policy) {
			return (sample_rate > 8) ?
				user_policy->voice_aec_tail_length_16k :
				user_policy->voice_aec_tail_length_8k;
		} else {
			return (sample_rate > 8) ? 48 : 96;
		}
	default:
		return 0;
	}
}

int audio_policy_is_out_channel_aec_reference(uint8_t stream_type)
{
#ifndef CONFIG_AUDIO_VOICE_HARDWARE_REFERENCE_EXTERNAL
    if(stream_type == AUDIO_STREAM_VOICE || stream_type == AUDIO_STREAM_LE_AUDIO){
#else
	if(stream_type == AUDIO_STREAM_LE_AUDIO){
#endif
        return true;
    }else{
        return false;
    }
}

int audio_policy_get_out_channel_aec_reference_stream_type(uint8_t stream_type)
{
#ifndef CONFIG_AUDIO_VOICE_HARDWARE_REFERENCE_EXTERNAL
    if(stream_type == AUDIO_STREAM_VOICE || stream_type == AUDIO_STREAM_LE_AUDIO){
#else
	if(stream_type == AUDIO_STREAM_LE_AUDIO){
#endif
        return AUDIO_MODE_MONO;
    }else{
        return 0;
    }
}

int audio_policy_get_channel_resample(uint8_t stream_type, bool decoder)
{
	int resample = false;

	switch (stream_type)
	{
	case AUDIO_STREAM_MUSIC:
	case AUDIO_STREAM_TTS:
	case AUDIO_STREAM_LOCAL_MUSIC:
		resample = true;
		break;
	case AUDIO_STREAM_SOUNDBAR:
		resample = true;
		break;
//#ifdef CONFIG_MUSIC_EXTERNAL_EFFECT
	case AUDIO_STREAM_SPDIF_IN:
	case AUDIO_STREAM_LINEIN:
	case AUDIO_STREAM_I2SRX_IN:
//#endif
	case AUDIO_STREAM_USOUND:
		resample = true;
		break;
	case AUDIO_STREAM_LE_AUDIO:
	case AUDIO_STREAM_VOICE:
		if (decoder) {
			resample = true;
		}
		break;
	}

	return resample;
}

int audio_policy_get_channel_resample_aps(uint8_t stream_type, int format, bool decoder)
{
	int resample_aps = false;

	switch (stream_type)
	{
	case AUDIO_STREAM_LE_AUDIO:
	case AUDIO_STREAM_MUSIC:
	case AUDIO_STREAM_USOUND:
	case AUDIO_STREAM_SOUNDBAR:
	case AUDIO_STREAM_SPDIF_IN:
	case AUDIO_STREAM_LINEIN:
	case AUDIO_STREAM_I2SRX_IN:
	case AUDIO_STREAM_LOCAL_MUSIC:
		if (decoder) {
			resample_aps = true;
		}
		break;
	case AUDIO_STREAM_VOICE:
		resample_aps = true;
		break;
	}

	return resample_aps;
}

int audio_policy_get_output_support_multi_track(uint8_t stream_type)
{
	int support_multi_track = false;

#ifdef CONFIG_AUDIO_SUBWOOFER
	switch (stream_type) {
		case AUDIO_STREAM_MUSIC:
		case AUDIO_STREAM_SOUNDBAR:
		case AUDIO_STREAM_LINEIN:
		case AUDIO_STREAM_MIC_IN:
		case AUDIO_STREAM_USOUND:
		case AUDIO_STREAM_LOCAL_MUSIC:
		case  AUDIO_STREAM_LE_AUDIO:
			support_multi_track = true;
			break;
	}
#endif

	return support_multi_track;
}

int audio_policy_get_record_channel_type(uint8_t stream_type)
{
	int channel_type = AUDIO_CHANNEL_ADC;

	switch (stream_type) {
	case AUDIO_STREAM_SPDIF_IN:
		channel_type = AUDIO_CHANNEL_SPDIFRX;
		break;

	case AUDIO_STREAM_I2SRX_IN:
#ifdef CONFIG_AUDIO_IN_I2SRX0_SUPPORT
		channel_type = AUDIO_CHANNEL_I2SRX;
#endif
#ifdef CONFIG_AUDIO_IN_I2SRX1_SUPPORT
		channel_type = AUDIO_CHANNEL_I2SRX1;
#endif
		break;
	case AUDIO_STREAM_SOUNDBAR:
#ifdef CONFIG_AUDIO_IN_I2SRX1_SUPPORT
		channel_type = AUDIO_CHANNEL_I2SRX1;
#else
		channel_type = AUDIO_CHANNEL_ADC;
#endif
		break;
	}
	return channel_type;
}

int audio_policy_get_record_ext_ref_stream_type()
{
	return AUDIO_STREAM_I2SRX_IN;
}

int audio_policy_get_record_interleave_mode(uint8_t stream_type)
{
	int interleave_mode = LEFT_MONO_RIGHT_MUTE_MODE;

	switch(stream_type) {
	case AUDIO_STREAM_SOUNDBAR:
		interleave_mode = LEFT_MUTE_RIGHT_MONO_MODE;
		break;
#ifdef CONFIG_AUDIO_VOICE_HARDWARE_REFERENCE
	case AUDIO_STREAM_VOICE:
		interleave_mode = LEFT_RIGHT_SEPERATE;
		break;
#endif
	}

	return interleave_mode;

}

int audio_policy_get_pa_volume(uint8_t stream_type, uint8_t volume_level)
{
	int pa_volume = FULL_RANGE_PA_VALUE;

	if (!user_policy) {
		return pa_volume;
	}

	if (volume_level > user_policy->audio_out_volume_level) {
		volume_level = user_policy->audio_out_volume_level;
	}

	if (volume_level < 0) {
		volume_level = 0;
	}

	switch (stream_type) {
	case AUDIO_STREAM_VOICE:
		if(NULL != user_policy->voice_pa_table){
			pa_volume = user_policy->voice_pa_table[volume_level];
		}
		break;
	case AUDIO_STREAM_USOUND:
		if(NULL != user_policy->usound_pa_table){
			pa_volume = user_policy->usound_pa_table[volume_level];
		}
		break;
	case AUDIO_STREAM_SOUNDBAR:
	case AUDIO_STREAM_LE_AUDIO:
	case AUDIO_STREAM_LINEIN:
	case AUDIO_STREAM_FM:
	case AUDIO_STREAM_I2SRX_IN:
	case AUDIO_STREAM_MIC_IN:
	case AUDIO_STREAM_MUSIC:
	case AUDIO_STREAM_LOCAL_MUSIC:
	case AUDIO_STREAM_SPDIF_IN:
	case AUDIO_STREAM_TTS:
		if(NULL != user_policy->music_pa_table){
			pa_volume = user_policy->music_pa_table[volume_level];
		}
		break;
	}

	SYS_LOG_INF("stream %d, level %d, pa 0x%x\n", stream_type, volume_level, pa_volume);
	return pa_volume;
}

int audio_policy_get_da_volume(uint8_t stream_type, uint8_t volume_level)
{
	int da_volume = FULL_RANGE_DA_VALUE;

	if (!user_policy) {
		return da_volume ;
	}

	if (volume_level > user_policy->audio_out_volume_level) {
		volume_level = user_policy->audio_out_volume_level;
	}

	if (volume_level < 0) {
		volume_level = 0;
	}

	switch (stream_type) {
	case AUDIO_STREAM_VOICE:
		if(NULL != user_policy->voice_da_table){
			da_volume = user_policy->voice_da_table[volume_level];
		}
		break;
	case AUDIO_STREAM_USOUND:
		if(NULL != user_policy->usound_da_table){
			da_volume = user_policy->usound_da_table[volume_level];
		}
		break;
	case AUDIO_STREAM_SOUNDBAR:
	case AUDIO_STREAM_LE_AUDIO:
	case AUDIO_STREAM_LINEIN:
	case AUDIO_STREAM_FM:
	case AUDIO_STREAM_I2SRX_IN:
	case AUDIO_STREAM_MIC_IN:
	case AUDIO_STREAM_SPDIF_IN:
	case AUDIO_STREAM_MUSIC:
	case AUDIO_STREAM_LOCAL_MUSIC:
	case AUDIO_STREAM_TTS:
		if(NULL != user_policy->music_da_table){
			da_volume = user_policy->music_da_table[volume_level];
		}
		break;
	}

	SYS_LOG_INF("stream %d, level %d, da 0x%x\n", stream_type, volume_level, da_volume);
	return da_volume;
}

#ifdef CONFIG_AUDIO_MULTI_CHANNEL_MODE
s16_t audio_policy_get_multi_ch_da_volume(u8_t stream_type, u8_t level, u8_t ch)
{
	s16_t da = 0;

	if (!user_policy) {
		return da;
	}

	if (level > user_policy->audio_out_volume_level) {
		level = user_policy->audio_out_volume_level;
	}

	if (level < 0) {
		level = 0;
	}

	switch (stream_type) {
	case AUDIO_STREAM_VOICE:
		da = 0;
		break;
	default:
		if(ch == 0 ) {
			if(NULL != user_policy->multi_ch_da0_table){
				da = user_policy->multi_ch_da0_table[level];
			}
		} else if(ch == 1 ) {
			if(NULL != user_policy->multi_ch_da1_table){
				da = user_policy->multi_ch_da1_table[level];
			}
		}
	}

	SYS_LOG_INF("stream %d, level %d, da 0x%x\n", stream_type, level, da);

	return da;
}
#endif

int audio_policy_is_da_mute(u8_t stream_type)
{
#ifdef CONFIG_MUSIC_EXTERNAL_EFFECT
	int da_volume = audio_policy_get_da_volume(stream_type, 0);
	if (da_volume <= -999){
		return 1;
	}
#endif
	return 0;
}

int audio_policy_is_fill_zero_fadein(u8_t stream_type)
{
#ifdef CONFIG_DSP_FADEIN_FILL_ZERO
	return 1;
#else
	return 0;
#endif
}

int audio_policy_get_bybass_adjust_da_volume(uint8_t stream_type)
{
#ifdef CONFIG_DSP_BYPASS_ADJUST_DA_VOLUME
	return CONFIG_DSP_BYPASS_ADJUST_DA_VOLUME;
#else
	return 0;
#endif
}



#ifdef CONFIG_AUDIO_VOLUME_PA
int audio_policy_get_volume_level_by_db(uint8_t stream_type, int volume_db)
{
	//0.1dB table
	const int vol_level_db_table[16+1] = {
		-590, -440, -380, -340,
		-290, -240, -210, -190,
		-170, -147, -123, -100,
		-74, -50, -30, -2,
		0
	};

	int volume_level = 0;
	const int *volume_table = vol_level_db_table;

	if (volume_db >= volume_table[16])
	{
		volume_level = 16;
	}
	else
	{
		for (int i = 0; i <= 16; i++)
		{
			if (volume_db < volume_table[i])
			{
				volume_level = i;
				break;
			}
		}
	}

	volume_level = volume_level * audio_policy_get_volume_level()/16;

	return volume_level;
}

#else
int audio_policy_get_volume_level_by_db(uint8_t stream_type, int volume_db)
{
	int volume_level = 0;
	int max_level = user_policy->audio_out_volume_level;
	const int *volume_table = user_policy->music_pa_table;

	if (!user_policy)
		goto exit;

	switch (stream_type) {
		case AUDIO_STREAM_VOICE:
			volume_table = user_policy->music_pa_table;
			break;
		case AUDIO_STREAM_USOUND:
			volume_table = user_policy->usound_pa_table;
			max_level = 16;
			break;
		case AUDIO_STREAM_LINEIN:
		case AUDIO_STREAM_FM:
		case AUDIO_STREAM_I2SRX_IN:
		case AUDIO_STREAM_MIC_IN:
		case AUDIO_STREAM_SPDIF_IN:
		case AUDIO_STREAM_MUSIC:
		/* case AUDIO_STREAM_SOUNDBAR: */
		case AUDIO_STREAM_LOCAL_MUSIC:
		case AUDIO_STREAM_TTS:
			volume_table = user_policy->music_pa_table;
			break;
	}

	/* to 0.001 dB */
	volume_db *= 100;
	if (volume_db == volume_table[max_level - 1]) {
		volume_level = max_level;
	} else {
		for (int i = 0; i < max_level; i++) {
			if (volume_db < volume_table[i]) {
				volume_level = i;
				break;
			}
		}
	}

exit:
	return volume_level;
}

#endif

int audio_policy_get_aec_reference_type(uint8_t stream_type)
{
#ifdef CONFIG_AUDIO_VOICE_HARDWARE_REFERENCE
	if (stream_type == AUDIO_STREAM_VOICE)
		return 2;
	else
		return 1;
#else
#ifdef CONFIG_AUDIO_VOICE_HARDWARE_REFERENCE_EXTERNAL
 	if (stream_type == AUDIO_STREAM_VOICE)
		return 2;
	else
#endif
    	return 1;
#endif
}

int audio_policy_get_volume_level(void)
{
	if (user_policy)
		return user_policy->audio_out_volume_level;

	return 16;
}

void audio_policy_set_nav_frame_size_us(u16_t frame_size_us)
{
	dynamic_policy.nav_frame_size_us = frame_size_us;
}

int audio_policy_get_nav_frame_size_us()
{
	if (dynamic_policy.nav_frame_size_us && dynamic_policy.nav_frame_size_us % 100 == 0)
		return dynamic_policy.nav_frame_size_us;

	return 10000;
}

void audio_policy_set_bis_link_delay_ms(u16_t delay_ms)
{
	dynamic_policy.bis_link_delay_ms = delay_ms;
}

int audio_policy_get_bis_link_delay_ms()
{
#ifdef CONFIG_DSP_OUTPUT_1_CH_IN_BMS
	return dynamic_policy.bis_link_delay_ms > 112 ? 112 : dynamic_policy.bis_link_delay_ms;
#else
	return dynamic_policy.bis_link_delay_ms > 76 ? 76 : dynamic_policy.bis_link_delay_ms;
#endif
}

void audio_policy_set_dynamic_waterlevel_ms(u16_t time_ms)
{
	dynamic_policy.dynamic_time = time_ms;
}

int audio_policy_get_dynamic_waterlevel_ms()
{
	return dynamic_policy.dynamic_time + dynamic_policy.user_dynamic_time;
}

void audio_policy_set_user_dynamic_waterlevel_ms(u16_t time_ms)
{
	dynamic_policy.user_dynamic_time = time_ms;
}

int audio_policy_get_user_dynamic_waterlevel_ms()
{
	return dynamic_policy.user_dynamic_time;
}

void audio_policy_set_smart_control_gain(int16_t gain)
{
	dynamic_policy.smart_gain = gain;
}

int16_t audio_policy_get_smart_control_gain(void)
{
	return dynamic_policy.smart_gain;
}

void audio_policy_set_lea_start_threshold(u16_t start_threshold)
{
	dynamic_policy.lea_start_threshold = start_threshold;
}

u16_t audio_policy_get_lea_start_threshold()
{
	return dynamic_policy.lea_start_threshold;
}

int audio_policy_get_bis_align_buffer_size(int width16, int sample_rate_hz, int ext_delay)
{
	int buffer_size = 0;
	int delay = dynamic_policy.bis_link_delay_ms + 6; //fixme

	buffer_size = 2 * 2 * width16 * (delay * sample_rate_hz / 1000);
#ifdef CONFIG_DSP_OUTPUT_24BIT_WIDTH_IN_BMS
	buffer_size = buffer_size * 3 / 4;
#ifdef CONFIG_DSP_OUTPUT_1_CH_IN_BMS
	buffer_size = buffer_size / 2;
#endif
#endif
	return buffer_size;
}

int audio_policy_is_record_triggered_by_track(u8_t stream_type)
{
	if (stream_type == AUDIO_STREAM_LE_AUDIO)
		return 0;
	else
		return 1;
}

int audio_policy_check_tts_fixed_volume(void)
{
	if (user_policy)
		return user_policy->tts_fixed_volume;

	return 1;
}



int audio_policy_check_save_volume_to_nvram(void)
{
	if (user_policy)
		return user_policy->volume_saveto_nvram;

	return 1;
}

int audio_policy_get_reduce_threshold(int format, u8_t stream_type)
{
	if (format == NAV_TYPE) {
		return 1;
	}

	if (system_check_low_latencey_mode()) {
		if (format == MSBC_TYPE || format == CVSD_TYPE) {
			return 5;
		} else {
		#ifdef CONFIG_TWS
			if (btif_tws_get_dev_role() == BTSRV_TWS_NONE) {
				return 15;
			} else {
				return 30;
			}
		#else
			return 70;
		#endif
		}
	} else {
		if (format == MSBC_TYPE || format == CVSD_TYPE) {
			return 30;
		} else if (format == PCM_TYPE) {
			return 5 + audio_policy_get_bis_link_delay_ms();
		} else {
			if (stream_type == AUDIO_STREAM_SOUNDBAR) {
				int lth = 170;
#if CONFIG_EXTERNAL_DSP_DELAY == 0
				lth = 150;
#endif
				lth += audio_policy_get_bis_link_delay_ms();
				if (format == AAC_TYPE)
					lth += GET_LIMIT_VALUE(audio_policy_get_dynamic_waterlevel_ms(), MAX_AAC_DYNAMIC_TIME);
				else
					lth += GET_LIMIT_VALUE(audio_policy_get_dynamic_waterlevel_ms(), MAX_SBC_DYNAMIC_TIME);
				return lth;
			} else
				return 150;
		}
	}
}

int audio_policy_get_increase_threshold(int format, u8_t stream_type)
{
	if (format == NAV_TYPE) {
		if (audio_policy_get_nav_frame_size_us() == 5000)
			return 4;
		else
			return 9;
	}

	if (system_check_low_latencey_mode()) {
		if (format == MSBC_TYPE || format == CVSD_TYPE) {
			return 15;
		} else {
#ifdef CONFIG_TWS
			if (btif_tws_get_dev_role() == BTSRV_TWS_NONE) {
				return 50;
			} else {
				return 50;
			}
#else
			return 101;
#endif
		}
	} else {
		if (format == MSBC_TYPE || format == CVSD_TYPE) {
			return 60;
		} else if (format == PCM_TYPE) {
			if (stream_type == AUDIO_STREAM_SPDIF_IN || stream_type == AUDIO_STREAM_LINEIN || stream_type == AUDIO_STREAM_I2SRX_IN) {
				return 16 + audio_policy_get_bis_link_delay_ms();
			} else if (stream_type == AUDIO_STREAM_USOUND ||stream_type == AUDIO_STREAM_LOCAL_MUSIC) {
				return 16 + audio_policy_get_bis_link_delay_ms();
			} else {
				return 16;
			}
		} else {
			if (stream_type == AUDIO_STREAM_SOUNDBAR){
				int hth = 270;
#if CONFIG_EXTERNAL_DSP_DELAY == 0
				hth = 250;
#endif
				hth += audio_policy_get_bis_link_delay_ms();
				if (format == AAC_TYPE)
					hth += GET_LIMIT_VALUE(audio_policy_get_dynamic_waterlevel_ms(), MAX_AAC_DYNAMIC_TIME);
				else
					hth += GET_LIMIT_VALUE(audio_policy_get_dynamic_waterlevel_ms(), MAX_SBC_DYNAMIC_TIME);
				return hth;
			}
			else {
				return 230;
			}
		}
	}
}

int audio_policy_check_snoop_tws_support(uint8_t stream_type)
{
// #ifdef CONFIG_SNOOP_LINK_TWS
//     return 1;
// #else
//     return 0;
// #endif
	if (stream_type  == AUDIO_STREAM_MUSIC)
		return 1;
	else
		return 0;
}

int audio_policy_get_snoop_tws_sync_param(uint8_t format,
    uint16_t *negotiate_time, uint16_t *negotiate_timeout, uint16_t *sync_interval)
{
    assert(negotiate_time);
    assert(negotiate_timeout);
    assert(sync_interval);

    if (user_policy) {
        switch(format) {
        case CVSD_TYPE:
        case MSBC_TYPE:
            *negotiate_time = 100;
            *negotiate_timeout = 600;
            *sync_interval = 200;
            break;
        default:
            *negotiate_time = 120;
            *negotiate_timeout = 1000;
            *sync_interval = 40;
            break;
        }
    }

    if(0 == *negotiate_time) {
        *negotiate_time = 100;
    }
    if(0 == *negotiate_timeout) {
        *negotiate_timeout = 1000;
    }
    if(0 == *sync_interval) {
        *sync_interval = 100;
    }
    return 0;
}

int audio_policy_register(const struct audio_policy_t *policy)
{
	user_policy = policy;
	return 0;
}

u8_t audio_policy_get_subwoofer_channels(void)
{
    u8_t ch = 0;

#ifdef CONFIG_AUDIO_MULTI_CHANNEL_MODE
    u8_t mode;

    mode = audio_system_get_multi_channel_mode();

    if ((mode == MULTI_CH_MODE_2_0) || (mode == MULTI_CH_MODE_11)) {
	    ch = 0;
    } else if ((mode == MULTI_CH_MODE_2_1)) {
	    ch = 2;
    } else if ((mode == MULTI_CH_MODE_2_2) || (mode == MULTI_CH_MODE_2_11)) {
	    ch = 2;
    } else {
	    ch = 0;
    }
#endif

    return ch;
}


