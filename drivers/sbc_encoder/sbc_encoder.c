#include "common.h"
#include "string.h"
#include "sbc_math.h"
#include "sbc_tables.h"
#include "sbc_encode.h"
#include "arithmetic.h"
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_SBC_ENCODER

#ifdef CONFIG_SBC_ENCODER_USE_GLOBAL_MEM
#include <global_mem.h>
#endif

typedef ssize_t (*sbc_encode_t)(sbc_t *sbc, const void *input, size_t input_len, size_t bit_depth, size_t gain,
								void *output, size_t output_len, ssize_t *written);
extern void mem_free(void *ptr);
extern void * mem_malloc(unsigned int num_bytes);

int sbc_encoder_frame_encode(void *handle, void *param)
{
	int ret  = 0;
	sbc_encode_param_t *encode_param = (sbc_encode_param_t *) param;
	music_frame_info_t music_frame_info;

	music_frame_info.input = (void *)encode_param->input_buffer;
	music_frame_info.input_len = encode_param->input_len;
	music_frame_info.output = (void *)encode_param->output_buffer;
	music_frame_info.output_len = encode_param->output_len;

	ret = audio_encoder_ops_sbc(handle, AE_CMD_FRAME_ENCODE, (u32_t)&music_frame_info);

	if(ret == 0){
		encode_param->output_data_size = music_frame_info.output_len;
	}

	return ret;
}

static int sbc_encoder_close(void *handle)
{
	return 0;
}

extern uint32_t bt_manager_get_sbc_bitpool(void);
extern uint32_t bt_manager_get_codec_sample_rate(void);
extern int audio_policy_get_audio_merge_mono_mode(void);

static void* sbc_encoder_open(sbc_encode_param_t * param)
{
	void *enc_handle;

	sbc_enc_info_t enc_info;

	memset(&enc_info, 0, sizeof(sbc_enc_info_t));

	enc_info.msbc = 0;

	enc_info.enc_type = SBC_MODE_JOINT_STEREO;

#ifdef CONFIG_CSB_PARA_CONSISTANT
	if(audio_policy_get_audio_merge_mono_mode()) {
	    enc_info.enc_type = SBC_MODE_MONO;
	}
#endif

	enc_info.allocation = 0;

	enc_info.enc_bitpool = bt_manager_get_sbc_bitpool();

	ACT_LOG_ID_INF(ALF_STR_sbc_encoder_open__ENC_BITPOOL_DN, 1, enc_info.enc_bitpool);

	ACT_LOG_ID_INF(ALF_STR_sbc_encoder_open__ENC_TYPE_DN, 1, enc_info.enc_type);

	enc_info.handle_enc_buffer = global_mem_get_cache_pool(SBC_ENCODER_DATA, 0);

    switch(bt_manager_get_codec_sample_rate())
    {
        case 48:
            enc_info.enc_samplerate = 48000;
            break;
        case 44:
            enc_info.enc_samplerate = 44100;
            break;
        case 32:
            enc_info.enc_samplerate = 32000;
            break;
        case 16:
            enc_info.enc_samplerate = 16000;
            break;
        default:
            enc_info.enc_samplerate = 44100;
            break;
    }

	audio_encoder_ops_sbc(&enc_handle, AE_CMD_OPEN, (u32_t)&enc_info);

	return enc_handle;
}

int audio_encoder_ops_sbc_wrapper(void *handle, audioenc_ex_ops_cmd_t cmd, unsigned int args)
{
	int ret = 0;
	void *encoder = NULL;
	switch (cmd) {
		case AE_CMD_OPEN:
		{
			encoder = sbc_encoder_open((sbc_encode_param_t *)args);
			if (!encoder)
				ret = -ENXIO;
			else
				*(void **)handle = encoder;
		}
		break;
    	case AE_CMD_CLOSE:
		ret = sbc_encoder_close(handle);
		break;
		case AE_CMD_FRAME_ENCODE:
		ret = sbc_encoder_frame_encode(handle,(void *)args);
		break;
		default:
		break;
	}
	return ret;
}


