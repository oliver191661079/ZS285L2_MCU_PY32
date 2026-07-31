/********************************************************************************
 *                            USDK(ZS283A_clean)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2018-10-29-下午2:06:47             1.0             build this file
 ********************************************************************************/
/*!
 * \file     audio_service_shell.c
 * \brief
 * \author
 * \version  1.0
 * \date  2018-10-29-下午2:06:47
 *******************************************************************************/
#include <misc/printk.h>
#include <shell/shell.h>
#include <init.h>
#include <stdlib.h>
#include <string.h>
#include <debug/object_tracing.h>

#ifdef CONFIG_AMP_AW882XX
#include <amp.h>
#endif

#if defined(CONFIG_KALLSYMS)
#include <kallsyms.h>
#endif
#if defined(CONFIG_SYS_LOG)
#include <logging/sys_log.h>
#endif

#include <system_recovery.h>

#define SHELL_AUDIODRV "audiodrv"

#include "audio_inner.h"
#include "audio_asrc.h"
#include <audio_out.h>
#include <audio_in.h>

#include <stack_backtrace.h>

#define AUDIODRVVERSION 0x01000000

#define AUDIODRV_VER_MAJOR(ver) ((ver >> 24) & 0xFF)
#define AUDIODRV_VER_MINOR(ver) ((ver >> 16) & 0xFF)
#define AUDIODRV_VER_PATCHLEVEL(ver) ((ver >> 8) & 0xFF)

#ifdef CONFIG_AUDIODRV_TEST

#define SHELL_ADC_USE_DIFFERENT_BUFFER

#define SHELL_CMD_CHL_INDEX_KEY "chl_id="
#define SHELL_CMD_CHL_TYPE_KEY "chl_type="
#define SHELL_CMD_CHL_WIDTH_KEY "chl_width="
#define SHELL_CMD_SAMPLE_RATE_KEY "sr="
#define SHELL_CMD_FIFO_TYPE_KEY "fifo_type="
#define SHELL_CMD_LEFT_VOL_KEY "left_vol="
#define SHELL_CMD_RIGHT_VOL_KEY "right_vol="
#define SHELL_CMD_RELOAD_KEY "reload_en"
#define SHELL_CMD_CH0_GAIN_KEY "ch0_gain="
#define SHELL_CMD_CH1_GAIN_KEY "ch1_gain="
#define SHELL_CMD_CH2_GAIN_KEY "ch2_gain="
#define SHELL_CMD_CH3_GAIN_KEY "ch3_gain="
#define SHELL_CMD_INPUT_DEV_KEY "input_dev="
#define SHELL_CMD_DUMP_LEN_KEY "dump_len="
#define SHELL_CMD_IOCTL_PARAM_KEY "param="
#define SHELL_CMD_IOCTL_CMD_KEY "cmd="
#define SHELL_CMD_IOCTL_CMD_SPDIF_CSL_KEY "csl="
#define SHELL_CMD_IOCTL_CMD_SPDIF_CSH_KEY "csh="
#define SHELL_CMD_IOCTL_CMD_BIND_ID_KEY "bind_id="
#define SHELL_CMD_ADDA_KEY "adda_en"
#define SHELL_CMD_PLAY_MONO_KEY "mono_en"
#define SHELL_CMD_PLAY_PERF_DETAIL_KEY "perf_detail"
#define SHELL_CMD_PLAY_ASRC_POLICY_KEY "asrc_policy="
#define SHELL_CMD_REC_ASRC_OUTSAMPLERATE_KEY "asrc_out_fs="
#define SHELL_CMD_PLAY_ASRC_INSAMPLERATE_KEY "asrc_in_fs="

static const uint16_t L1khz_R2khz_Dat_Sine[] = {
	0x0000, 0x0000,
	0x10B5, 0x2120,
	0x2120, 0x3FFF,
	0x30FB, 0x5A82,
	0x3FFF, 0x6ED9,
	0x4DEB, 0x7BA3,
	0x5A82, 0x7FFF,
	0x658C, 0x7BA3,
	0x6ED9, 0x6ED9,
	0x7641, 0x5A82,
	0x7BA3, 0x3FFF,
	0x7EE7, 0x2120,
	0x7FFF, 0x0000,
	0x7EE7, 0xDEDF,
	0x7BA3, 0xC000,
	0x7641, 0xA57D,
	0x6ED9, 0x9126,
	0x658C, 0x845C,
	0x5A82, 0x8000,
	0x4DEB, 0x845C,
	0x3FFF, 0x9126,
	0x30FB, 0xA57D,
	0x2120, 0xC000,
	0x10B5, 0xDEDF,
	0x0000, 0x0000,
	0xEF4A, 0x2120,
	0xDEDF, 0x3FFF,
	0xCF04, 0x5A82,
	0xC000, 0x6ED9,
	0xB214, 0x7BA3,
	0xA57D, 0x7FFF,
	0x9A73, 0x7BA3,
	0x9126, 0x6ED9,
	0x89BE, 0x5A82,
	0x845C, 0x3FFF,
	0x8118, 0x2120,
	0x8000, 0x0000,
	0x8118, 0xDEDF,
	0x845C, 0xC000,
	0x89BE, 0xA57D,
	0x9126, 0x9126,
	0x9A73, 0x845C,
	0xA57D, 0x8000,
	0xB214, 0x845C,
	0xC000, 0x9126,
	0xCF04, 0xA57D,
	0xDEDF, 0xC000,
	0xEF4A, 0xDEDF
};

static const uint32_t L1khz_R2khz_Dat_Sine_24bits[]= {
	0x00000000,0x00000000,
	0x10B51400,0x2120FB00,
	0x2120FB00,0x3FFFFF00,
	0x30FBC400,0x5A827800,
	0x3FFFFF00,0x6ED9EA00,
	0x4DEBE400,0x7BA37400,
	0x5A827800,0x7FFFFF00,
	0x658C9900,0x7BA37400,
	0x6ED9EA00,0x6ED9EA00,
	0x7641AE00,0x5A827900,
	0x7BA37400,0x3FFFFF00,
	0x7EE7A900,0x2120FB00,
	0x7FFFFF00,0x00000000,
	0x7EE7A900,0xDEDF0600,
	0x7BA37400,0xC0000100,
	0x7641AE00,0xA57D8800,
	0x6ED9EA00,0x91261600,
	0x658C9900,0x845C8D00,
	0x5A827900,0x80000200,
	0x4DEBE400,0x845C8C00,
	0x3FFFFF00,0x91261500,
	0x30FBC500,0xA57D8700,
	0x2120FB00,0xC0000000,
	0x10B51500,0xDEDF0400,
	0x00000000,0x00000000,
	0xEF4AEC00,0x2120FB00,
	0xDEDF0600,0x3FFFFF00,
	0xCF043C00,0x5A827800,
	0xC0000100,0x6ED9EA00,
	0xB2141D00,0x7BA37400,
	0xA57D8800,0x7FFFFF00,
	0x9A736700,0x7BA37400,
	0x91261600,0x6ED9EA00,
	0x89BE5200,0x5A827900,
	0x845C8D00,0x3FFFFF00,
	0x81185700,0x2120FB00,
	0x80000200,0x00000000,
	0x81185700,0xDEDF0600,
	0x845C8C00,0xC0000100,
	0x89BE5200,0xA57D8800,
	0x91261500,0x91261600,
	0x9A736700,0x845C8D00,
	0xA57D8700,0x80000200,
	0xB2141B00,0x845C8C00,
	0xC0000000,0x91261500,
	0xCF043B00,0xA57D8700,
	0xDEDF0400,0xC0000000,
	0xEF4AEB00,0xDEDF0400
};

/**
 * struct audio_play_attr
 * @brief Audio play attribute.
 */
struct audio_play_attr {
	uint8_t chl_type; /* channel type */
	uint8_t chl_width; /* channel width */
	uint8_t sr; /* sample rate */
	uint8_t fifo_type; /* fifo type */
	int32_t left_vol; /* left volume */
	int32_t right_vol; /* right volume */
	uint8_t id; /* channel index */
	uint8_t asrc_in_sr; /* ASRC input sample rate */
	uint8_t reload_en : 1; /* reload enable flag */
	uint8_t adda_en : 1; /* ADC => DAC enable */
	uint8_t mono_en : 1; /* mono mode enable  */
	uint8_t perf_detail_en : 1; /* performance detail information enable */
	uint8_t asrc_policy: 2; /* ASRC memory policy and if set to enable ASRC function */
	uint8_t reserved : 2;
};

/**
 * struct audio_play_objects
 * @brief The resource that belongs to audio player.
 */
struct audio_play_object {
#define AUDIO_PLAY_START_FLAG			(1 << 0)
#define AUDIO_PLAY_SAMPLES_CNT_EN_FLAG	(1 << 1)
#define AUDIO_PLAY_SDM_CNT_EN_FLAG		(1 << 2)
	struct device *aout_dev;
	uint8_t *audio_buffer;
	uint32_t audio_buffer_size;
	void *aout_handle;
	uint32_t play_total_size;
	uint32_t flags;
	struct audio_play_attr attribute;
};

/* @brief audio play handler */
typedef struct audio_play_object *audio_play_handle;

/* audio player instance */
#define AUDIO_PLAY_MAX_HANLDER (2)
static audio_play_handle audio_player[AUDIO_PLAY_MAX_HANLDER];

/* statistics audio play total size */
static uint64_t audio_play_total_size;

/**
 * struct audio_record_attr
 * @brief Audio record attribute.
 */
struct audio_record_attr {
	uint8_t chl_type; /* channel type */
	uint8_t chl_width; /* channel width */
    uint8_t sr; /* sample rate */
	uint8_t dump_len; /* the length of data that per-second dump */
	uint8_t id; /* channel id */
    int16_t ch_gain[ADC_CH_NUM_MAX]; /* ADC left gain */
	uint16_t input_dev; /* input audio device */
	uint8_t asrc_out_sr; /* ASRC output sample rate */
	uint8_t asrc_policy: 2; /* ASRC memory policy and if set to enable ASRC function */
};

/**
 * struct audio_record_object
 * @brief The resource that belongs to audio recorder.
 */
struct audio_record_object {
    struct device *ain_dev;
    uint8_t *audio_buffer;
    uint32_t audio_buffer_size;
    void *ain_handle;
    struct audio_record_attr attribute;
};

/* @brief audio record handler */
typedef struct audio_record_object *audio_record_handle;

/* audio recorder instance */
#define AUDIO_RECORD_MAX_HANLDER (1)
static audio_record_handle audio_recorder[AUDIO_RECORD_MAX_HANLDER];

#define SHELL_AUDIO_BUFFER_SIZE (1024)
extern char __ram_dsp_start[];
static uint8_t *shell_audio_buffer = __ram_dsp_start;

#ifdef SHELL_ADC_USE_DIFFERENT_BUFFER
static uint8_t *shell_audio_buffer1 = __ram_dsp_start + SHELL_AUDIO_BUFFER_SIZE;
#endif

/* @brief find the value according to the key */
static int cmd_search_value_by_key(size_t argc, char **argv,
				const char *key, uint32_t *value)
{
	int i;
	char *p;

	for (i = 1; i < argc; i++) {
		if (strstr(argv[i], key)) {
			SYS_LOG_DBG("found string:%s", argv[i]);
			/* found key */
			p = strchr(argv[i], '=');
			if (!p) {
				*value = 1;
			} else {
				SYS_LOG_DBG("found key value:%s", p);
				*value = strtoul(++p, NULL, 0);
			}
			break;
		}
	}

	if (i == argc)
		return -ENXIO;

	return 0;
}

/* @brief prepare audio PCM data */
static void audio_prepare_data(audio_play_handle hdl, uint8_t *buf, uint32_t len, uint8_t width)
{
	uint32_t fill_len;
	uint16_t count;
	uint16_t i, j;

	if (hdl)
		hdl->audio_buffer = buf;

	if (width == CHANNEL_WIDTH_16BITS) {
		fill_len = len / sizeof(L1khz_R2khz_Dat_Sine) * sizeof(L1khz_R2khz_Dat_Sine);
		if (hdl)
			hdl->audio_buffer_size = fill_len;
		count = fill_len / sizeof(L1khz_R2khz_Dat_Sine);
		const uint16_t *src = L1khz_R2khz_Dat_Sine;
		uint16_t *dst = (uint16_t *)buf;
		for (i = 0; i < count; i++) {
			for (j = 0; j < sizeof(L1khz_R2khz_Dat_Sine)/2; j++)
				*dst++ = src[j];
		}
	} else {
		/* 32bits width */
		fill_len = len / sizeof(L1khz_R2khz_Dat_Sine_24bits) * sizeof(L1khz_R2khz_Dat_Sine_24bits);
		if (hdl)
			hdl->audio_buffer_size = fill_len;
		count = fill_len / sizeof(L1khz_R2khz_Dat_Sine_24bits);
		const uint32_t *src = L1khz_R2khz_Dat_Sine_24bits;
		uint32_t *dst = (uint32_t *)buf;
		for (i = 0; i < count; i++) {
			for (j = 0; j < sizeof(L1khz_R2khz_Dat_Sine)/4; j++) {
				*dst++ = src[j];
			}
		}
	}
}

/* @brief audio i2stx SRD callback */
static int audio_i2stx_srd_cb(void *cb_data, uint32_t cmd, void *param)
{
	switch (cmd) {
		case I2STX_SRD_FS_CHANGE:
			SYS_LOG_INF("New sample rate %d", *(audio_sr_sel_e *)param);
			break;
		case I2STX_SRD_WL_CHANGE:
			SYS_LOG_INF("New width length %d", *(audio_i2s_srd_wl_e *)param);
			break;
		case I2STX_SRD_TIMEOUT:
			SYS_LOG_INF("SRD timeout");
			break;
		default:
			SYS_LOG_ERR("Error command %d", cmd);
			return -EFAULT;
	}

	return 0;
}

/* @brief dump the detail informatioin of audio play  */
static void audio_play_perf_detail(void)
{
	uint8_t i;
	int ret;
	uint32_t count = 0;

	for (i = 0; i < AUDIO_PLAY_MAX_HANLDER; i++) {
		if (audio_player[i]->flags & AUDIO_PLAY_START_FLAG) {
			SYS_LOG_INF("** play[%d] performance %dB/s **",
					i, audio_player[i]->play_total_size);

			audio_play_total_size += audio_player[i]->play_total_size;

			audio_player[i]->play_total_size = 0;

			if (audio_player[i]->flags & AUDIO_PLAY_SAMPLES_CNT_EN_FLAG) {
				ret = audio_out_control(audio_player[i]->aout_dev,
					audio_player[i]->aout_handle, AOUT_CMD_GET_SAMPLE_CNT, &count);
				if (!ret)
					SYS_LOG_INF("	play[%d] counter samples:%d   ", i, count);
			}
		}
	}

	SYS_LOG_INF("   play total size %lld   ", audio_play_total_size);
}

/* @brief The callback from the low level audio play device for the request to write more data */
static int audio_play_write_data_cb(void *handle, uint32_t reason)
{
	audio_play_handle hdl = (audio_play_handle)handle;
	uint32_t len, delta;
	uint8_t *buf = NULL;
	int ret;
	static uint32_t play_timestamp = 0;

	SYS_LOG_DBG("handle %p, cb reason %d", handle, reason);

	if (hdl->attribute.reload_en) {
		len = hdl->audio_buffer_size / 2;
		if (AOUT_DMA_IRQ_HF == reason) {
			buf = hdl->audio_buffer;
			SYS_LOG_DBG("DMA IRQ HF");
		} else if (AOUT_DMA_IRQ_TC == reason) {
			buf = hdl->audio_buffer + len;
			SYS_LOG_DBG("DMA IRQ TC");
		} else {
			SYS_LOG_ERR("invalid reason %d", reason);
			return -1;
		}
	} else {
		buf = hdl->audio_buffer;
		len = hdl->audio_buffer_size;
	}

	delta = (uint32_t)SYS_CLOCK_HW_CYCLES_TO_NS((k_cycle_get_32() - play_timestamp));
	if (hdl->flags & AUDIO_PLAY_START_FLAG)
		hdl->play_total_size += len;

	if (!hdl->attribute.reload_en) {
		ret = audio_out_write(hdl->aout_dev, hdl->aout_handle, buf, len);
		if (ret) {
			SYS_LOG_ERR("write data error:%d", ret);
			return ret;
		}
	}

	if (delta > 1000000000UL) {
		play_timestamp = k_cycle_get_32();
		audio_play_perf_detail();
	}

	return 0;
}

/* @brief Configure the extral infomation to the low level audio play device */
static int audio_play_ext_config(audio_play_handle handle)
{
	int ret = 0;
	uint32_t count;

	if (!handle) {
		SYS_LOG_ERR("null handle");
		return -EINVAL;
	}

	SYS_LOG_DBG("perf_detail_en %d", handle->attribute.perf_detail_en);

	if (handle->attribute.perf_detail_en) {
		ret = audio_out_control(handle->aout_dev,
					handle->aout_handle, AOUT_CMD_RESET_SAMPLE_CNT, NULL);
		if (!ret) {
			handle->flags |= AUDIO_PLAY_SAMPLES_CNT_EN_FLAG;
			SYS_LOG_INF("enable samples counter function");
		}

		ret = audio_out_control(handle->aout_dev,
				handle->aout_handle, AOUT_CMD_GET_SAMPLE_CNT, &count);
		if (!ret) {
			SYS_LOG_INF("orign samples counter:%d", count);
		}
	}

	if (handle->attribute.asrc_policy) {
		asrc_out_setting_t asrc_out_setting = {0};
		--handle->attribute.asrc_policy;

		asrc_out_setting.mode = SRC_MODE;
		asrc_out_setting.wclk = ASRC_OUT_WCLK_DMA;
		asrc_out_setting.input_samplerate = handle->attribute.asrc_in_sr;
		asrc_out_setting.mem_policy = handle->attribute.asrc_policy;

		ret = audio_out_control(handle->aout_dev,
					handle->aout_handle, AOUT_CMD_ASRC_ENABLE, &asrc_out_setting);
		if (!ret) {
			SYS_LOG_INF("set ASRC mode successfully");
		}
	}

	return ret;
}

/* @brief Configure the low level audio play device by specified handler with necessary attribute */
static int audio_play_config(audio_play_handle handle)
{
	aout_param_t aout_param = {0};
	dac_setting_t dac_setting = {0};
	i2stx_setting_t i2stx_setting = {0};
	spdiftx_setting_t spdiftx_setting = {0};
	audio_reload_t reload_setting = {0};

	if (!handle) {
		SYS_LOG_ERR("null handle");
		return -EINVAL;
	}

	aout_param.sample_rate = handle->attribute.sr;
	aout_param.channel_type = handle->attribute.chl_type;
	aout_param.outfifo_type = handle->attribute.fifo_type;
	aout_param.channel_width = handle->attribute.chl_width;

	SYS_LOG_INF("sr:%d channel type:0x%x fifo type:%d width:%d",
			aout_param.sample_rate, aout_param.channel_type,
			aout_param.outfifo_type, aout_param.channel_width);

	aout_param.callback = audio_play_write_data_cb;
	aout_param.cb_data = handle;

	/* DMA reload mode */
	if (handle->attribute.reload_en) {
		reload_setting.reload_addr = handle->audio_buffer;
		reload_setting.reload_len = handle->audio_buffer_size;
		aout_param.reload_setting = &reload_setting;
	}

	/* DAC interface setting */
	if (handle->attribute.chl_type & AUDIO_CHANNEL_DAC) {
		dac_setting.volume.left_volume = handle->attribute.left_vol;
		dac_setting.volume.right_volume = handle->attribute.right_vol;
		dac_setting.channel_mode = handle->attribute.mono_en ? MONO_MODE : STEREO_MODE;
		aout_param.dac_setting = &dac_setting;
	}

	/* I2S interface setting */
	if (handle->attribute.chl_type & AUDIO_CHANNEL_I2STX) {
		/* SRD function is only used in slave mode */
		i2stx_setting.srd_callback = audio_i2stx_srd_cb;
		i2stx_setting.cb_data = handle;
		aout_param.i2stx_setting = &i2stx_setting;
		if (AOUT_FIFO_DAC0 == aout_param.outfifo_type) {
			aout_param.dac_setting = &dac_setting;
			dac_setting.volume.left_volume = handle->attribute.left_vol;
			dac_setting.volume.right_volume = handle->attribute.right_vol;
		}
	}

	/* SPDIF interface setting */
	if (handle->attribute.chl_type & AUDIO_CHANNEL_SPDIFTX) {
		if (AOUT_FIFO_DAC0 == aout_param.outfifo_type) {
			aout_param.dac_setting = &dac_setting;
			dac_setting.volume.left_volume = handle->attribute.left_vol;
			dac_setting.volume.right_volume = handle->attribute.right_vol;
		}
		aout_param.spdiftx_setting = &spdiftx_setting;
	}

	handle->aout_handle = audio_out_open(handle->aout_dev, (void *)&aout_param);
	if (!handle->aout_handle)
		return -EFAULT;

	return audio_play_ext_config(handle);
}

/* @brief create an audio play instance and return the play handler */
static audio_play_handle audio_play_create(struct audio_play_attr *attr)
{
	struct device *dev;
	audio_play_handle hdl = NULL;
	static struct audio_play_object play_object[AUDIO_PLAY_MAX_HANLDER];

	if (!attr) {
		SYS_LOG_ERR("Invalid attr");
		return NULL;
	}

	dev = (struct device *)device_get_binding(CONFIG_AUDIO_OUT_ACTS_DEV_NAME);
	if (!dev) {
		SYS_LOG_ERR("find to bind dev %s", CONFIG_AUDIO_OUT_ACTS_DEV_NAME);
		return NULL;
	}

	hdl = &play_object[attr->id];
	memset(hdl, 0, sizeof(struct audio_play_object));

#ifdef SHELL_ADC_USE_DIFFERENT_BUFFER
	if (!attr->id)
		hdl->audio_buffer = shell_audio_buffer;
	else
		hdl->audio_buffer = shell_audio_buffer1;
#else
	hdl->audio_buffer = shell_audio_buffer;
#endif

	hdl->audio_buffer_size = SHELL_AUDIO_BUFFER_SIZE;

	if (!attr->adda_en) {
		audio_prepare_data(hdl, hdl->audio_buffer,
				SHELL_AUDIO_BUFFER_SIZE, attr->chl_width);
	} else {
		memset(hdl->audio_buffer, 0, SHELL_AUDIO_BUFFER_SIZE);
	}

	hdl->aout_dev = (struct device *)dev;
	memcpy(&hdl->attribute, attr, sizeof(struct audio_play_attr));

	return hdl;
}


/* @brief start to play stream */
static int audio_play_start(audio_play_handle handle)
{
	int ret;
	if (handle->attribute.reload_en) {
		ret = audio_out_start(handle->aout_dev, handle->aout_handle);
	} else {
		ret = audio_out_write(handle->aout_dev, handle->aout_handle,
						handle->audio_buffer, handle->audio_buffer_size);
		audio_play_total_size += handle->audio_buffer_size;
	}

	if (ret) {
		SYS_LOG_ERR("player[%d] start error:%d", handle->attribute.id, ret);
	} else {
		handle->flags |= AUDIO_PLAY_START_FLAG;
		SYS_LOG_INF("player[%d] start successfully", handle->attribute.id);
	}

	return ret;
}

/* @brief stop playing stream */
static void audio_play_stop(audio_play_handle handle)
{
	uint8_t i;
	bool all_stop_flag = true;

	if (handle && handle->aout_handle) {
		audio_out_stop(handle->aout_dev, handle->aout_handle);
		audio_out_close(handle->aout_dev, handle->aout_handle);
		audio_player[handle->attribute.id & (AUDIO_PLAY_MAX_HANLDER - 1)] = NULL;
		handle->flags = 0;
	}

	for (i = 0; i < AUDIO_PLAY_MAX_HANLDER; i++) {
		if (audio_player[i]) {
			all_stop_flag = false;
			break;
		}
	}

	if (all_stop_flag)
		audio_play_total_size = 0;
}

/* @brief command to start playing */
static int shell_cmd_play_start(int argc, char *argv[])
{
	struct audio_play_attr play_attr = {0};
	uint32_t val;
	audio_play_handle handle;
	int ret;

	if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_CHL_TYPE_KEY, &val))
		play_attr.chl_type = val;
	else
		play_attr.chl_type = AUDIO_CHANNEL_DAC;

	if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_CHL_WIDTH_KEY, &val))
		play_attr.chl_width = val;
	else
		play_attr.chl_width = CHANNEL_WIDTH_16BITS;

	if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_SAMPLE_RATE_KEY, &val))
		play_attr.sr = val;
	else
		play_attr.sr = SAMPLE_RATE_48KHZ;

	if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_FIFO_TYPE_KEY, &val))
		play_attr.fifo_type = val;
	else
		play_attr.fifo_type = AOUT_FIFO_DAC0;

	if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_LEFT_VOL_KEY, &val))
		play_attr.left_vol = (int32_t)val;
	else
		play_attr.left_vol = 0; /* 0dB */

	if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_RIGHT_VOL_KEY, &val))
		play_attr.right_vol = (int32_t)val;
	else
		play_attr.right_vol = 0; /* 0dB */

	if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_RELOAD_KEY, &val))
		play_attr.reload_en = val;
	else
		play_attr.reload_en = 0;

	if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_CHL_INDEX_KEY, &val))
		play_attr.id = val;
	else
		play_attr.id = 0;

	if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_ADDA_KEY, &val))
		play_attr.adda_en = val;
	else
		play_attr.adda_en = 0;

	if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_PLAY_MONO_KEY, &val))
		play_attr.mono_en = val;
	else
		play_attr.mono_en = 0;

	if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_PLAY_PERF_DETAIL_KEY, &val))
		play_attr.perf_detail_en = val;
	else
		play_attr.perf_detail_en = 0;

	if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_PLAY_ASRC_POLICY_KEY, &val))
		play_attr.asrc_policy = val + 1;
	else
		play_attr.asrc_policy = 0;

	if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_PLAY_ASRC_INSAMPLERATE_KEY, &val))
		play_attr.asrc_in_sr = val;
	else
		play_attr.asrc_in_sr = play_attr.sr;

	if (play_attr.id >= AUDIO_PLAY_MAX_HANLDER) {
		SYS_LOG_ERR("invalid channel id %d", play_attr.id);
		return -EINVAL;
	}

	if (audio_player[play_attr.id])
		audio_play_stop(audio_player[play_attr.id]);

	handle = audio_play_create(&play_attr);
	if (!handle)
		return -EFAULT;

	audio_player[play_attr.id] = handle;

	ret = audio_play_config(handle);
	if (ret)
		return ret;

	ret = audio_play_start(handle);
	if (ret)
		return ret;

	return ret;
}

/* @brief command to stop playing */
static int shell_cmd_play_stop(int argc, char *argv[])
{
	uint32_t val;
	uint8_t id;

	if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_CHL_INDEX_KEY, &val))
		id = val;
	else
		id = 0;

	if (id >= AUDIO_PLAY_MAX_HANLDER) {
		SYS_LOG_ERR("invalid channel id %d", id);
		return -EINVAL;
	}

	if (audio_player[id])
		audio_play_stop(audio_player[id]);

	return 0;
}

/* @brief command to control playing dynamically */
static int shell_cmd_play_ioctl(int argc, char *argv[])
{
	uint32_t val, param = 0;
	uint8_t cmd, id = 0;
	int ret = 0;
	bool is_in = false;

	if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_IOCTL_CMD_KEY, &val)) {
		cmd = val;
	} else {
		SYS_LOG_ERR("invalid cmd");
		return -EINVAL;
	}

	if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_IOCTL_PARAM_KEY, &val)) {
		param = val;
		is_in = true;
	}

	if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_CHL_INDEX_KEY, &val))
		id = (val < AUDIO_PLAY_MAX_HANLDER) ? val : 0;

	if (!audio_player[id]) {
		SYS_LOG_ERR("audio player[%d] does not start yet", id);
		return -ENXIO;
	}

	if (cmd == AOUT_CMD_GET_VOLUME) {
		volume_setting_t volume = {0};

		ret = audio_out_control(audio_player[id]->aout_dev,
				audio_player[id]->aout_handle, cmd, (void *)&volume);
		if (!ret) {
			SYS_LOG_INF("Left volume: %d", volume.left_volume);
			SYS_LOG_INF("Right_volume: %d", volume.right_volume);
		} else {
			SYS_LOG_ERR("Get volume error:%d", ret);
		}
	} else if (cmd == AOUT_CMD_SET_VOLUME) {
		volume_setting_t volume = {0};

		if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_LEFT_VOL_KEY, &val))
			volume.left_volume = val;
		if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_RIGHT_VOL_KEY, &val))
			volume.right_volume = val;

		ret = audio_out_control(audio_player[id]->aout_dev,
				audio_player[id]->aout_handle, cmd, (void *)&volume);
		if (ret) {
			SYS_LOG_ERR("Set volume error:%d", ret);
		}
	} else if (cmd == AOUT_CMD_SPDIF_SET_CHANNEL_STATUS) {
		audio_spdif_ch_status_t ch_status = {0};

		if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_IOCTL_CMD_SPDIF_CSL_KEY, &val))
			ch_status.csl = val;
		if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_IOCTL_CMD_SPDIF_CSH_KEY, &val))
			ch_status.csh = val;

		ret = audio_out_control(audio_player[id]->aout_dev,
				audio_player[id]->aout_handle, cmd, (void *)&ch_status);
		if (ret) {
			SYS_LOG_ERR("Set SPDIF status error:%d", ret);
		}
	} else if (cmd == AOUT_CMD_SPDIF_GET_CHANNEL_STATUS) {
		audio_spdif_ch_status_t ch_status = {0};

		ret = audio_out_control(audio_player[id]->aout_dev,
				audio_player[id]->aout_handle, cmd, (void *)&ch_status);
		if (ret) {
			SYS_LOG_ERR("Get SPDIF status error:%d", ret);
		} else {
			SYS_LOG_INF("SPDIF CSL: 0x%x", ch_status.csl);
			SYS_LOG_INF("SPDIF CSH: 0x%x", ch_status.csh);
		}
	} else {
		ret = audio_out_control(audio_player[id]->aout_dev,
				audio_player[id]->aout_handle, cmd, (void *)&param);
		if (ret) {
			SYS_LOG_ERR("execute cmd:%d error:%d", cmd, ret);
		} else {
			if (!is_in) {
				SYS_LOG_INF("cmd:%d result:%d", cmd, param);
			}
		}
	}

	return ret;
}

/* @brief audio i2srx SRD(sample rate detection) callback */
static int audio_i2srx_srd_cb(void *cb_data, uint32_t cmd, void *param)
{
	switch (cmd) {
		case I2SRX_SRD_FS_CHANGE:
			SYS_LOG_INF("New sample rate %d", *(audio_sr_sel_e *)param);
			break;
		case I2SRX_SRD_WL_CHANGE:
			SYS_LOG_INF("New width length %d", *(audio_i2s_srd_wl_e *)param);
			break;
		case I2SRX_SRD_TIMEOUT:
			SYS_LOG_INF("SRD timeout");
			break;
		default:
			SYS_LOG_ERR("Error command %d", cmd);
			return -EFAULT;
	}

	return 0;
}

/* @brief audio spdifrx SRD(sample rate detection) callback */
static int audio_spdifrx_srd_cb(void *cb_data, uint32_t cmd, void *param)
{
	switch (cmd) {
		case SPDIFRX_SRD_FS_CHANGE:
			SYS_LOG_INF("New sample rate %d", *(audio_sr_sel_e *)param);
			break;
		case SPDIFRX_SRD_TIMEOUT:
			SYS_LOG_INF("SRD timeout");
			break;
		default:
			SYS_LOG_ERR("Error command %d", cmd);
			return -EFAULT;
	}

	return 0;
}

/* @brief read data callback from the low level audio record device */
static int audio_rec_read_data_cb(void *callback_data, uint32_t reason)
{
	static uint32_t rec_timestamp = 0;
	static uint32_t rec_total_size = 0; /* per-second total size  */
	audio_record_handle handle = (audio_record_handle)callback_data;
	uint32_t delta, len = handle->audio_buffer_size / 2;
	uint8_t *buf = NULL;

	if (AIN_DMA_IRQ_HF == reason) {
		buf = handle->audio_buffer;
	} else if (AIN_DMA_IRQ_TC == reason) {
		buf = handle->audio_buffer + len;
	} else {
		SYS_LOG_ERR("invalid reason:%d", reason);
		return 0;
	}

	delta = (uint32_t)SYS_CLOCK_HW_CYCLES_TO_NS(k_cycle_get_32() - rec_timestamp);
	rec_total_size += len;

	if (delta > 1000000000UL) {
		if (handle->attribute.dump_len)
			print_buffer(buf, 1, handle->attribute.dump_len, 16, -1);
		rec_timestamp = k_cycle_get_32();
		printk("** record[%d] performance %dB/s ** \n",
				handle->attribute.id, rec_total_size);
		rec_total_size = 0;
	}

	return 0;
}

/* @brief Configure the extral infomation to the low level audio record device */
static int audio_record_ext_config(audio_record_handle handle)
{
	int ret = 0;

	if (!handle) {
		SYS_LOG_ERR("null handle");
		return -EINVAL;
	}

	if (handle->attribute.asrc_policy) {
		asrc_in_setting_t asrc_in_setting = {0};
		--handle->attribute.asrc_policy;

		asrc_in_setting.mode = SRC_MODE;
		asrc_in_setting.rclk = ASRC_IN_RCLK_DMA;
		asrc_in_setting.output_samplerate = handle->attribute.asrc_out_sr;
		asrc_in_setting.mem_policy = handle->attribute.asrc_policy;

		ret = audio_in_control(handle->ain_dev,
					handle->ain_handle, AIN_CMD_ASRC_ENABLE, &asrc_in_setting);
		if (!ret) {
			SYS_LOG_INF("set ASRC mode successfully");
		}
	}

	return ret;
}

/* @brief Configure the low level audio device to record */
static int audio_record_config(audio_record_handle handle)
{
	ain_param_t ain_param = {0};
	adc_setting_t adc_setting = {0};
	i2srx_setting_t i2srx_setting = {0};
	spdifrx_setting_t spdifrx_setting = {0};

	if (!handle)
		return -EINVAL;

	memset(&ain_param, 0, sizeof(ain_param_t));

	ain_param.sample_rate = handle->attribute.sr;
	ain_param.callback = audio_rec_read_data_cb;
	ain_param.cb_data = (void *)handle;
	ain_param.reload_setting.reload_addr = handle->audio_buffer;
	ain_param.reload_setting.reload_len = handle->audio_buffer_size;
	ain_param.channel_type = handle->attribute.chl_type;
	ain_param.channel_width = handle->attribute.chl_width;

	switch (handle->attribute.chl_type) {
		case AUDIO_CHANNEL_ADC:
			adc_setting.device = handle->attribute.input_dev;
			uint8_t i;
			for (i = 0; i < ADC_CH_NUM_MAX; i++) {
				adc_setting.gain.ch_gain[i] = handle->attribute.ch_gain[i];
			}
			ain_param.adc_setting = &adc_setting;
			break;
		case AUDIO_CHANNEL_I2SRX:
		case AUDIO_CHANNEL_I2SRX1:
			/* SRD is only used in i2s slave mode */
			i2srx_setting.srd_callback = audio_i2srx_srd_cb;
			i2srx_setting.cb_data = handle;
			ain_param.i2srx_setting = &i2srx_setting;
			break;
		case AUDIO_CHANNEL_SPDIFRX:
			spdifrx_setting.srd_callback = audio_spdifrx_srd_cb;
			spdifrx_setting.cb_data = handle;
			ain_param.spdifrx_setting = &spdifrx_setting;
			break;
		default:
			SYS_LOG_ERR("Unsupport channel type: %d", handle->attribute.chl_type);
			return -ENOTSUP;
	}

	handle->ain_handle = audio_in_open(handle->ain_dev, &ain_param);
	if (!handle->ain_handle)
		return -EFAULT;

    return audio_record_ext_config(handle);
}

/* @brief create an audio record instance */
static audio_record_handle audio_record_create(struct audio_record_attr *attr)
{
	struct device *dev;
	audio_record_handle hdl = NULL;
	static struct audio_record_object record_object[AUDIO_RECORD_MAX_HANLDER];

	dev = (struct device *)device_get_binding(CONFIG_AUDIO_IN_ACTS_DEV_NAME);
	if (!dev) {
		SYS_LOG_ERR("Failed to get the audio in device %s", CONFIG_AUDIO_IN_ACTS_DEV_NAME);
		return NULL;
	}

	hdl = &record_object[attr->id];
	memset(hdl, 0, sizeof(struct audio_record_object));

    hdl->audio_buffer_size = SHELL_AUDIO_BUFFER_SIZE;

#ifdef SHELL_ADC_USE_DIFFERENT_BUFFER
	if (!attr->id)
		hdl->audio_buffer = shell_audio_buffer;
	else
		hdl->audio_buffer = shell_audio_buffer1;
#else
    hdl->audio_buffer = shell_audio_buffer;
#endif

	memcpy(&hdl->attribute, attr, sizeof(struct audio_record_attr));
    hdl->ain_dev = dev;

    return hdl;
}

/* @brief start record */
static int audio_record_start(audio_record_handle handle)
{
	return audio_in_start(handle->ain_dev, handle->ain_handle);
}

/* @brief stop record */
static void audio_record_stop(audio_record_handle handle)
{
	if (handle && handle->ain_handle) {
		audio_in_stop(handle->ain_dev, handle->ain_handle);
		audio_in_close(handle->ain_dev, handle->ain_handle);
		audio_recorder[handle->attribute.id & (AUDIO_RECORD_MAX_HANLDER - 1)] = NULL;
	}
}

/* @brief command to creat a recorder channel instance */
static int shell_cmd_record_create(int argc, char **argv)
{
	struct audio_record_attr record_attr = {0};
	uint32_t val;
	audio_record_handle handle;
	int ret;

	if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_CHL_TYPE_KEY, &val))
		record_attr.chl_type = val;
	else
		record_attr.chl_type = AUDIO_CHANNEL_ADC;

	if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_CHL_WIDTH_KEY, &val))
		record_attr.chl_width = val;
	else
		record_attr.chl_width = CHANNEL_WIDTH_16BITS;

	if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_SAMPLE_RATE_KEY, &val))
		record_attr.sr = val;
	else
		record_attr.sr = SAMPLE_RATE_48KHZ;

	if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_INPUT_DEV_KEY, &val))
		record_attr.input_dev = val;
	else
		record_attr.input_dev = AIN_LOGIC_SOURCE_LINEIN;

	if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_CH0_GAIN_KEY, &val))
		record_attr.ch_gain[0] = (int16_t)val;
	else
		record_attr.ch_gain[0] = 0;

	if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_CH1_GAIN_KEY, &val))
		record_attr.ch_gain[1] = (int16_t)val;
	else
		record_attr.ch_gain[1] = 0;

#if (ADC_CH_NUM_MAX == 4)
	if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_CH2_GAIN_KEY, &val))
		record_attr.ch_gain[2] = (int16_t)val;
	else
		record_attr.ch_gain[2] = 0;

	if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_CH3_GAIN_KEY, &val))
		record_attr.ch_gain[3] = (int16_t)val;
	else
		record_attr.ch_gain[3] = 0;
#endif

	if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_DUMP_LEN_KEY, &val)) {
		record_attr.dump_len = val;
		SYS_LOG_INF("Dump length: %d", record_attr.dump_len);
	} else {
		record_attr.dump_len = 16;
	}

	if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_PLAY_ASRC_POLICY_KEY, &val))
		record_attr.asrc_policy = val + 1;
	else
		record_attr.asrc_policy = 0;

	if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_REC_ASRC_OUTSAMPLERATE_KEY, &val))
		record_attr.asrc_out_sr = val;
	else
		record_attr.asrc_out_sr = record_attr.sr;

	if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_CHL_INDEX_KEY, &val))
		record_attr.id = val;
	else
		record_attr.id = 0;

	if (record_attr.id >= AUDIO_RECORD_MAX_HANLDER) {
		SYS_LOG_ERR("invalid channel id %d", record_attr.id);
		return -EINVAL;
	}

	if (audio_recorder[record_attr.id])
		audio_record_stop(audio_recorder[record_attr.id]);

	handle = audio_record_create(&record_attr);
	if (!handle)
		return -EFAULT;

	audio_recorder[record_attr.id] = handle;

	ret = audio_record_config(handle);
	if (ret)
		return ret;

	return ret;

}

/* @brief command to start recording */
static int shell_cmd_record_start(int argc, char **argv)
{
	uint32_t val;
	uint8_t id;
	int ret;

	if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_CHL_INDEX_KEY, &val))
		id = val;
	else
		id = 0;

	if (!audio_recorder[id]) {
		SYS_LOG_ERR("audio recorder[%d] does not create yet", id);
		return -ENXIO;
	}

	ret = audio_record_start(audio_recorder[id]);
	if (ret) {
		SYS_LOG_ERR("recorder[id:%d] start error:%d", id, ret);
	} else {
		SYS_LOG_INF("recorder[id:%d] start successfully", id);
	}

	return ret;
}

/* @brief command to stop recording */
static int shell_cmd_record_stop(int argc, char **argv)
{
	uint32_t val;
	uint8_t id;

	if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_CHL_INDEX_KEY, &val))
		id = val;
	else
		id = 0;

	if (id >= AUDIO_RECORD_MAX_HANLDER) {
		SYS_LOG_ERR("invalid channel id %d", id);
		return -EINVAL;
	}

	if (audio_recorder[id])
		audio_record_stop(audio_recorder[id]);

	return 0;
}

/* @brief command to control recording dynamically */
static int shell_cmd_record_ioctl(int argc, char **argv)
{
	uint32_t val, param = 0;
	uint8_t cmd, id = 0;
	int ret = 0;
	bool is_in = false;

	if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_IOCTL_CMD_KEY, &val)) {
		cmd = val;
	} else {
		SYS_LOG_ERR("invalid cmd");
		return -EINVAL;
	}

	if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_IOCTL_PARAM_KEY, &val)) {
		param = val;
		is_in = true;
	}

	if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_CHL_INDEX_KEY, &val))
		id = (val < AUDIO_RECORD_MAX_HANLDER) ? val : 0;

	if (!audio_recorder[id]) {
		SYS_LOG_ERR("audio recorder[%d] does not create yet", id);
		return -ENXIO;
	}

	if (cmd == AIN_CMD_SET_ADC_GAIN) {
		adc_gain gain = {0};

		if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_CH0_GAIN_KEY, &val))
			gain.ch_gain[0] = val;
		if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_CH1_GAIN_KEY, &val))
			gain.ch_gain[1] = val;
#if (ADC_CH_NUM_MAX == 4)
		if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_CH2_GAIN_KEY, &val))
			gain.ch_gain[2] = val;
		if (!cmd_search_value_by_key(argc, argv, SHELL_CMD_CH2_GAIN_KEY, &val))
			gain.ch_gain[3] = val;
#endif
		ret = audio_in_control(audio_recorder[id]->ain_dev,
				audio_recorder[id]->ain_handle, cmd, (void *)&gain);
		if (ret) {
			SYS_LOG_ERR("Set gain error:%d", ret);
		}
	} else if (cmd == AIN_CMD_SPDIF_GET_CHANNEL_STATUS) {
		audio_spdif_ch_status_t ch_status = {0};

		ret = audio_in_control(audio_recorder[id]->ain_dev,
				audio_recorder[id]->ain_handle, cmd, (void *)&ch_status);
		if (ret) {
			SYS_LOG_ERR("Get SPDIF status error:%d", ret);
		} else {
			SYS_LOG_INF("SPDIF CSL: 0x%x", ch_status.csl);
			SYS_LOG_INF("SPDIF CSH: 0x%x", ch_status.csh);
		}
	} else {
		ret = audio_in_control(audio_recorder[id]->ain_dev,
				audio_recorder[id]->ain_handle, cmd, (void *)&param);
		if (ret) {
			SYS_LOG_ERR("execute cmd:%d error:%d", cmd, ret);
		} else {
			if (!is_in) {
				SYS_LOG_INF("cmd:%d result:%d", cmd, param);
			}
		}
	}

	return ret;
}

#endif /* CONFIG_AUDIODRV_TEST */

static uint32_t audiodrv_version_get(void)
{
    return AUDIODRVVERSION;
}

static int shell_cmd_version(int argc, char *argv[])
{
    u32_t version = audiodrv_version_get();

    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    printk("audiodrv version %d.%d.%d\n",
           AUDIODRV_VER_MAJOR(version),
           AUDIODRV_VER_MINOR(version),
           AUDIODRV_VER_PATCHLEVEL(version));
    return 0;
}

#if CONFIG_SYS_LOG_DEFAULT_LEVEL > 2
static void dma_regs_dump(void)
{
	uint8_t loop;

	printk("------------------- DMA INFO -------------------\n");
	for (loop = 0; loop < 8; loop++)
	{
		printk("****************** DMA%d INFO ****************** \n", loop);
		printk("    CTL: 0x%x\n", sys_read32(DMA_REG_BASE + ((loop + 1) * 0x100)));
		printk("  START: 0x%x\n", sys_read32(DMA_REG_BASE + ((loop + 1) * 0x100) + 4));
		printk(" SADDR0: 0x%x\n", sys_read32(DMA_REG_BASE + ((loop + 1) * 0x100) + 8));
		printk(" SADDR1: 0x%x\n", sys_read32(DMA_REG_BASE + ((loop + 1) * 0x100) + 12));
		printk(" DADDR0: 0x%x\n", sys_read32(DMA_REG_BASE + ((loop + 1) * 0x100) + 16));
		printk(" DADDR1: 0x%x\n", sys_read32(DMA_REG_BASE + ((loop + 1) * 0x100) + 20));
		printk("     BC: 0x%x\n", sys_read32(DMA_REG_BASE + ((loop + 1) * 0x100) + 24));
		printk("     RC: 0x%x\n", sys_read32(DMA_REG_BASE + ((loop + 1) * 0x100) + 28));
	}
	printk("------------------------------------------------\n");
}
#endif

static void dump_audio_out_reg(struct device *dev, int argc, char *argv[])
{
#if CONFIG_SYS_LOG_DEFAULT_LEVEL > 2
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_dac *dac_reg = cfg->dac;
	struct acts_audio_i2stx0 *i2stx0_reg = cfg->i2stx;
	struct acts_audio_spdiftx *spdiftx_reg = cfg->spdiftx;
	uint8_t loop, flag = 0;

#define DUMP_DAC_REG_FLAG			BIT(0)
#define DUMP_I2STX_REG_FLAG			BIT(1)
#define DUMP_SPDIFTX_REG_FLAG		BIT(2)
#define DUMP_ASRC_OUT_REG_FLAG		BIT(3)
#define DUMP_AOUT_DMA_REG_FLAG		BIT(4)

	if (argc > 1) {
		if (!strcmp("dac", argv[1]))
			flag |= DUMP_DAC_REG_FLAG;
		else if (!strcmp("i2stx", argv[1]))
			flag |= DUMP_I2STX_REG_FLAG;
		else if (!strcmp("spdiftx", argv[1]))
			flag |= DUMP_SPDIFTX_REG_FLAG;
		else if (!strcmp("asrc", argv[1]))
			flag |= DUMP_ASRC_OUT_REG_FLAG;
		else if (!strcmp("dma", argv[1]))
			flag |= DUMP_AOUT_DMA_REG_FLAG;
	} else {
		flag = DUMP_DAC_REG_FLAG | DUMP_I2STX_REG_FLAG
				| DUMP_SPDIFTX_REG_FLAG | DUMP_ASRC_OUT_REG_FLAG
				| DUMP_AOUT_DMA_REG_FLAG;
	}

	k_sleep(1);
	printk("\n------------------ BASIC INFO ------------------\n");
	printk("           MRCR0: 0x%x\n", sys_read32(RMU_MRCR0));
	printk("    CMU_HOSC_CTL: 0x%x\n", sys_read32(CMU_HOSC_CTL));
	printk(" CMU_COREPLL_CTL: 0x%x\n", sys_read32(CMU_COREPLL_CTL));
	printk("      CMU_SYSCLK: 0x%x\n", sys_read32(CMU_SYSCLK));
	printk("  AUDIO_PLL0_CTL: 0x%x\n", sys_read32(AUDIO_PLL0_CTL));
	printk("  AUDIO_PLL1_CTL: 0x%x\n", sys_read32(AUDIO_PLL1_CTL));
	printk("   CMU_DEVCLKEN0: 0x%x\n", sys_read32(CMU_DEVCLKEN0));
	printk("   CMU_DEVCLKEN1: 0x%x\n", sys_read32(CMU_DEVCLKEN1));
	printk("    CMU_MEMCLKEN: 0x%x\n", sys_read32(CMU_MEMCLKEN));
	printk("   CMU_MEMCLKSEL: 0x%x\n", sys_read32(CMU_MEMCLKSEL));
	printk("     CMU_ADDACLK: 0x%x\n", sys_read32(CMU_ADDACLK));
	printk("     CMU_ASRCCLK: 0x%x\n", sys_read32(CMU_ASRCCLK));
	printk("      CMU_I2SCLK: 0x%x\n", sys_read32(CMU_I2SCLK));
	printk("    CMU_SPDIFCLK: 0x%x\n", sys_read32(CMU_SPDIFCLK));
	printk("------------------------------------------------\n");

	k_sleep(1);
	if (flag & DUMP_DAC_REG_FLAG) {
		printk("------------------- DAC INFO -------------------\n");
		printk(" DAC_DIGCTL: 0x%x\n", dac_reg->digctl);
		printk("    FIFOCTL: 0x%x\n", dac_reg->fifoctl);
		printk("   DAC_STAT: 0x%x\n", dac_reg->stat);
		printk("    VOL_LCH: 0x%x\n", dac_reg->vol_lch);
		printk("    VOL_RCH: 0x%x\n", dac_reg->vol_rch);
		printk(" DAC_ANACTL: 0x%x\n", dac_reg->anactl);
		printk("   DAC_BIAS: 0x%x\n", dac_reg->bias);
		printk("     PA_VOL: 0x%x\n", dac_reg->vol);
		printk("    DAT0CNT: 0x%x\n", dac_reg->dat0cnt);
		printk("    DAT1CNT: 0x%x\n", dac_reg->dat1cnt);
		printk("------------------------------------------------\n");
	}

	if (flag & DUMP_I2STX_REG_FLAG) {
		printk("------------------ I2STX INFO ------------------\n");
		printk("      CTL: 0x%x\n", i2stx0_reg->tx0_ctl);
		printk("  FIFOCTL: 0x%x\n", i2stx0_reg->tx0_fifoctl);
		printk(" FIFOSTAT: 0x%x\n", i2stx0_reg->tx0_fifostat);
		printk("      DAT: 0x%x\n", i2stx0_reg->tx0_dat);
		printk("   SRDCTL: 0x%x\n", i2stx0_reg->tx0_srdctl);
		printk("  SRDSTAT: 0x%x\n", i2stx0_reg->tx0_srdstat);
		printk("  FIFOCNT: 0x%x\n", i2stx0_reg->tx0_fifocnt);
		printk("------------------------------------------------\n");
	}

	if (flag & DUMP_SPDIFTX_REG_FLAG) {
		printk("----------------- SPDIFTX INFO -----------------\n");
		printk(" CTL: 0x%x\n", spdiftx_reg->spdtx_ctl);
		printk(" CSL: 0x%x\n", spdiftx_reg->spdtx_csl);
		printk(" CSH: 0x%x\n", spdiftx_reg->spdtx_csh);
		printk("------------------------------------------------\n");
	}

	if (flag & DUMP_ASRC_OUT_REG_FLAG) {
		k_sleep(1);
		printk("---------------- ASRC OUT0 INFO ----------------\n");
		for (loop = 0; loop < 12; loop++) {
			printk("ASRC OUT0: 0x%x -- 0x%x\n", ASRC_REG_BASE+0x30+loop*4, sys_read32(ASRC_REG_BASE+0x30+loop*4));
		}
		printk("------------------------------------------------\n");

		printk("---------------- ASRC OUT1 INFO ----------------\n");
		for (loop = 0; loop < 12; loop++) {
			printk("ASRC OUT1: 0x%x -- 0x%x\n", ASRC_REG_BASE+0x60+loop*4, sys_read32(ASRC_REG_BASE+0x60+loop*4));
		}
		printk("------------------------------------------------\n");

		printk("---------------- ASRC COMMON INFO --------------\n");
		for (loop = 0; loop < 13; loop++) {
			printk("ASRC: 0x%x -- 0x%x\n", ASRC_REG_BASE+0xc0+loop*4, sys_read32(ASRC_REG_BASE+0xc0+loop*4));
		}
		printk("------------------------------------------------\n");
	}

	if (flag & DUMP_AOUT_DMA_REG_FLAG) {
		k_sleep(1);
		dma_regs_dump();
	}

#endif
}

static void dump_audio_in_reg(struct device *dev, int argc, char *argv[])
{
#if CONFIG_SYS_LOG_DEFAULT_LEVEL > 2
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_adc *adc_reg = cfg->adc;
	struct acts_audio_i2srx0 *i2srx0_reg = cfg->i2srx0;
	struct acts_audio_i2srx1 *i2srx1_reg = cfg->i2srx1;
	struct acts_audio_spdifrx *spdifrx_reg = cfg->spdifrx;
	uint8_t loop, flag = 0;

#define DUMP_ADC_REG_FLAG		BIT(0)
#define DUMP_I2SRX_REG_FLAG		BIT(1)
#define DUMP_SPDIFRX_REG_FLAG	BIT(2)
#define DUMP_ASRC_IN_REG_FLAG	BIT(3)
#define DUMP_AIN_DMA_REG_FLAG	BIT(4)

	if (argc > 1) {
		if (!strcmp("adc", argv[1]))
			flag |= DUMP_ADC_REG_FLAG;
		else if (!strcmp("i2srx", argv[1]))
			flag |= DUMP_I2SRX_REG_FLAG;
		else if (!strcmp("spdiftx", argv[1]))
			flag |= DUMP_SPDIFRX_REG_FLAG;
		else if (!strcmp("asrc", argv[1]))
			flag |= DUMP_ASRC_IN_REG_FLAG;
		else if (!strcmp("dma", argv[1]))
			flag |= DUMP_AIN_DMA_REG_FLAG;
	} else {
		flag = DUMP_ADC_REG_FLAG | DUMP_I2SRX_REG_FLAG
				| DUMP_SPDIFRX_REG_FLAG | DUMP_ASRC_IN_REG_FLAG
				| DUMP_AIN_DMA_REG_FLAG;
	}

	k_sleep(1);
	printk("\n------------------ BASIC INFO ------------------\n");
	printk("           MRCR0: 0x%x\n", sys_read32(RMU_MRCR0));
	printk("    CMU_HOSC_CTL: 0x%x\n", sys_read32(CMU_HOSC_CTL));
	printk(" CMU_COREPLL_CTL: 0x%x\n", sys_read32(CMU_COREPLL_CTL));
	printk("      CMU_SYSCLK: 0x%x\n", sys_read32(CMU_SYSCLK));
	printk("  AUDIO_PLL0_CTL: 0x%x\n", sys_read32(AUDIO_PLL0_CTL));
	printk("  AUDIO_PLL1_CTL: 0x%x\n", sys_read32(AUDIO_PLL1_CTL));
	printk("   CMU_DEVCLKEN0: 0x%x\n", sys_read32(CMU_DEVCLKEN0));
	printk("   CMU_DEVCLKEN1: 0x%x\n", sys_read32(CMU_DEVCLKEN1));
	printk("    CMU_MEMCLKEN: 0x%x\n", sys_read32(CMU_MEMCLKEN));
	printk("   CMU_MEMCLKSEL: 0x%x\n", sys_read32(CMU_MEMCLKSEL));
	printk("     CMU_ADDACLK: 0x%x\n", sys_read32(CMU_ADDACLK));
	printk("     CMU_ASRCCLK: 0x%x\n", sys_read32(CMU_ASRCCLK));
	printk("      CMU_I2SCLK: 0x%x\n", sys_read32(CMU_I2SCLK));
	printk("    CMU_SPDIFCLK: 0x%x\n", sys_read32(CMU_SPDIFCLK));
	printk("------------------------------------------------\n");

	k_sleep(1);
	if (flag & DUMP_ADC_REG_FLAG) {
		printk("------------------- ADC INFO -------------------\n");
		printk(" ADC_DIGITAL: 0x%x\n", adc_reg->digctl);
		printk("     FIFOCTL: 0x%x\n", adc_reg->fifoctl);
		printk("        STAT: 0x%x\n", adc_reg->stat);
		printk("         DAT: 0x%x\n", adc_reg->dat);
		printk("  ADC_ANACTL: 0x%x\n", adc_reg->anactl);
		printk("    ADC_BIAS: 0x%x\n", adc_reg->bias);
		printk("    ADC_DMIC: 0x%x\n", adc_reg->dmicctl);
		printk("------------------------------------------------\n");
	}

	if (flag & DUMP_I2SRX_REG_FLAG) {
		printk("------------------ I2SRX INFO ------------------\n");
		printk("     RX0_CTL: 0x%x\n", i2srx0_reg->rx0_ctl);
		printk("  RX0_SRDCTL: 0x%x\n", i2srx0_reg->rx0_srdctl);
		printk("  RX0_SRDSTA: 0x%x\n", i2srx0_reg->rx0_srdsta);
		printk("     RX1_CTL: 0x%x\n", i2srx1_reg->rx1_ctl);
		printk(" RX1_FIFOCTL: 0x%x\n", i2srx1_reg->rx1_fifoctl);
		printk(" RX1_FIFOSTA: 0x%x\n", i2srx1_reg->rx1_fifostat);
		printk("     RX1_DAT: 0x%x\n", i2srx1_reg->rx1_dat);
		printk("  RX1_SRDCTL: 0x%x\n", i2srx1_reg->rx1_srdctl);
		printk("  RX1_SRCSTA: 0x%x\n", i2srx1_reg->rx1_srdsta);
		printk("------------------------------------------------\n");
	}

	if (flag & DUMP_SPDIFRX_REG_FLAG) {
		printk("----------------- SPDIFRX INFO -----------------\n");
		printk("  CTL0: 0x%x\n", spdifrx_reg->spdrx_ctl0);
		printk("  CTL1: 0x%x\n", spdifrx_reg->spdrx_ctl1);
		printk("  CTL2: 0x%x\n", spdifrx_reg->spdrx_ctl2);
		printk("    PD: 0x%x\n", spdifrx_reg->spdrx_pd);
		printk("   DBG: 0x%x\n", spdifrx_reg->spdrx_dbg);
		printk("   CNT: 0x%x\n", spdifrx_reg->spdrx_cnt);
		printk("   CSL: 0x%x\n", spdifrx_reg->spdrx_csl);
		printk("   CSH: 0x%x\n", spdifrx_reg->spdrx_csh);
		printk("  SAMP: 0x%x\n", spdifrx_reg->spdrx_samp);
		printk(" THRES: 0x%x\n", spdifrx_reg->spdrx_srto_thres);
		printk("------------------------------------------------\n");
	}

	if (flag & DUMP_ASRC_IN_REG_FLAG) {
		k_sleep(1);
		printk("----------------- ASRC IN0 INFO ----------------\n");
		for (loop = 0; loop < 12; loop++) {
			printk("ASRC IN0: 0x%x -- 0x%x\n", ASRC_REG_BASE+loop*4, sys_read32(ASRC_REG_BASE+loop*4));
		}
		printk("------------------------------------------------\n");

		printk("----------------- ASRC IN1 INFO ----------------\n");
		for (loop = 0; loop < 12; loop++) {
			printk("ASRC IN1: 0x%x -- 0x%x\n", ASRC_REG_BASE+0x90+loop*4, sys_read32(ASRC_REG_BASE+0x90+loop*4));
		}
		printk("------------------------------------------------\n");

		printk("---------------- ASRC COMMON INFO --------------\n");
		for (loop = 0; loop < 13; loop++) {
			printk("ASRC: 0x%x -- 0x%x\n", ASRC_REG_BASE+0xc0+loop*4, sys_read32(ASRC_REG_BASE+0xc0+loop*4));
		}
		printk("------------------------------------------------\n");
	}

	if (flag & DUMP_AIN_DMA_REG_FLAG) {
		k_sleep(1);
		dma_regs_dump();
	}

#endif

}

static void modify_audio_reg(int address, int value)
{
	sys_write32(value, address);

	printk("%d, addr: 0x%x, value: 0x%x\n", __LINE__, address, sys_read32(address));
}

static int shell_cmd_dump_audio_out(int argc, char *argv[])
{
	struct device * aout_dev = device_get_binding(CONFIG_AUDIO_OUT_ACTS_DEV_NAME);
	if(aout_dev != NULL) {
		dump_audio_out_reg(aout_dev, argc, argv);
	}
    return 0;
}

static int shell_cmd_dump_audio_in(int argc, char *argv[])
{
	struct device * ain_dev = device_get_binding(CONFIG_AUDIO_IN_ACTS_DEV_NAME);
	if(ain_dev != NULL) {
		dump_audio_in_reg(ain_dev, argc, argv);
	}
    return 0;
}

static int shell_cmd_modify_audio_reg(int argc, char *argv[])
{
	int addr, value;

	if((argv[1] != NULL) && (argv[2] != NULL))
	{
		addr = atoi(argv[1]);
		value = atoi(argv[2]);
		modify_audio_reg(addr, value);
	}

    return 0;
}

static int shell_cmd_read_reg(int argc, char *argv[])
{
	int addr, count, loop;

	if((argv[1] != NULL) && (argv[2] != NULL))
	{
		addr = atoi(argv[1]);
		count = atoi(argv[2]);

		if (count > 0) {
			for(loop = 0; loop < count; loop++)
			{
			printk("%d, addr: 0x%x, value: 0x%x\n", __LINE__, addr, sys_read32(addr));
				addr += 4;
			}
		}
	}

    return 0;
}

#ifdef CONFIG_AMP_AW882XX
static int shell_cmd_amp_set_reg(int argc, char *argv[])
{
	u8_t addr=0;
	u16_t data=0;

	SYS_LOG_INF("Enter\n");

	struct device *dev = device_get_binding(CONFIG_AMP_DEV_NAME);
	if(dev == NULL) {
		SYS_LOG_INF("No amplifier device");
		return -1;
	}

	if (argc < 3) {
		SYS_LOG_WRN("Wrong parameters.\n");
		return -1;
	}

	addr = (u8_t)strtoul(argv[1], NULL, 16);
	data = (u16_t)strtoul(argv[2], NULL, 16);

	printk("set 0x%x to 0x%x\n", addr, data);

	amp_set_reg(dev, addr, data);

	return 0;
}
static int shell_cmd_dump_regs(int argc, char *argv[])
{
	struct device *dev = device_get_binding(CONFIG_AMP_DEV_NAME);
	if(dev == NULL) {
		SYS_LOG_INF("No amplifier device");
		return -1;
	}

	amp_dump_regs(dev);

	return 0;
}
static int shell_cmd_amp_set_volume(int argc, char *argv[])
{
	u8_t vol;

	struct device *dev = device_get_binding(CONFIG_AMP_DEV_NAME);
	if(dev == NULL) {
		SYS_LOG_INF("No amplifier device");
		return -1;
	}

	if (argc == 2) {
		vol = (u8_t)strtoul(argv[1], (char **)NULL, 10);
	} else {
		vol = 0;
	}

	//SYS_LOG_INF("set amp vol: %d", vol);

	amp_set_volume(dev, vol);

	return 0;
}
#endif

const struct shell_cmd audiodrv_commands[] = {
    {"version", shell_cmd_version, "show audiodrv version"},
    {"dumpaudioout", shell_cmd_dump_audio_out, "dump audio driver out info"},
    {"dumpaudioin", shell_cmd_dump_audio_in, "dump audio driver in info"},
    {"modifyaudioreg", shell_cmd_modify_audio_reg, "modify audio driver register"},
    {"readreg", shell_cmd_read_reg, "read register value"},
#ifdef CONFIG_AMP_AW882XX
    {"ampdump", shell_cmd_dump_regs, "ampdump"},
    {"ampreg", shell_cmd_amp_set_reg, "ampreg hex_addr hex_value."},
    {"ampvol", shell_cmd_amp_set_volume, "ampvol volume(0~255)"},
#endif
#ifdef CONFIG_AUDIODRV_TEST
    {"play_start", shell_cmd_play_start, "audio play start"},
    {"play_stop", shell_cmd_play_stop, "audio play stop"},
    {"play_ioctl", shell_cmd_play_ioctl, "audio play ioctl"},
    {"rec_create", shell_cmd_record_create, "audio record create"},
    {"rec_start", shell_cmd_record_start, "audio record start"},
    {"rec_ioctl", shell_cmd_record_ioctl, "audio record ioctl"},
    {"rec_stop", shell_cmd_record_stop, "audio record stop"},
#endif
    {NULL, NULL, NULL}
};

SHELL_REGISTER(SHELL_AUDIODRV, audiodrv_commands);

