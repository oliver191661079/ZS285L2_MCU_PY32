/***************************************************************************
 * Copyright (c) 2018 Actions Semi Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.

 * Author: mikeyang<mikeyang@actions-semi.com>
 *
 * Description:    audio in driver
 *
 * Change log:
 *	2018/5/24: Created by mike.
 *	2021/06/09 Modified by SRD3 team.
 ****************************************************************************
 */

#include "audio_inner.h"
#include "audio_asrc.h"
#include <kernel.h>
#include <init.h>
#include <device.h>
#include <irq.h>
#include <dma.h>
#include <soc.h>
#include <string.h>
#include <gpio.h>
#include <board.h>
#include <audio_in.h>
#include <stdlib.h>

#define AIN_SESSION_MAGIC                    (0x11223344)
#define AIN_SESSION_CHECK_MAGIC(x)           ((x) == AIN_SESSION_MAGIC)

#define AIN_ADC_SESSION_MAX                  (1) /* 1 ADC channels */

#ifdef CONFIG_AUDIO_IN_I2SRX0_SUPPORT
#define AIN_I2SRX_SESSION_MAX                (1) /* 1 I2SRX channel */
#else
#define AIN_I2SRX_SESSION_MAX                (0) /* 1 I2SRX channel */
#endif

#ifdef CONFIG_AUDIO_IN_I2SRX1_SUPPORT
#define AIN_I2SRX1_SESSION_MAX               (1) /* 1 I2SRX1 channel */
#else
#define AIN_I2SRX1_SESSION_MAX               (0) /* 1 I2SRX1 channel */
#endif

#ifdef CONFIG_AUDIO_IN_SPDIFRX_SUPPORT
#define AIN_SPDIFRX_SESSION_MAX              (1) /* 1 SPDIFRX channel */
#else
#define AIN_SPDIFRX_SESSION_MAX              (0) /* 1 SPDIFRX channel */
#endif

#define AIN_SESSION_MAX                      (AIN_ADC_SESSION_MAX + AIN_I2SRX_SESSION_MAX \
												+ AIN_I2SRX1_SESSION_MAX + AIN_SPDIFRX_SESSION_MAX) /* totally max sessions */

#define AIN_SESSION_OPEN                     (1 << 0) /* Session open flag */
#define AIN_SESSION_CONFIG                   (1 << 1) /* Session config flag */
#define AIN_SESSION_START                    (1 << 2) /* Session start flag */
#define AIN_SESSION_BIND                     (1 << 3) /* Session start flag */

/* The commands belong to ASRC */
#define AIN_IS_ASRC_CMD(x)                  ((x) & AIN_ASRC_CMD_FLAG)

/*
 * struct ain_session_t
 * @brief audio in session structure
 */
typedef struct {
	int magic; /* session magic value as AIN_SESSION_MAGIC */
	uint16_t channel_type; /* Indicates the channel type selection */
	uint8_t flags; /* session flags */
	uint8_t id; /* the session id */
	ain_channel_node_t channel; /* audio in channel */
	uint16_t input_dev; /* input audio device */
	uint8_t dma_prepare_start : 1; /* use DMA prepare start flag */
	uint8_t asrc_enable : 1; /* FIFO output to ASRC enable flag */
	uint8_t interleave_en: 1; /* DMA interleave mode enable flag */
	uint8_t dsp_enable : 1; /* FIFO output to DSP enable flag */
} ain_session_t;

static ain_session_t ain_sessions[AIN_SESSION_MAX];

/*
 * @brief checkout if there is the same channel within audio in sessions
 */
static bool audio_in_session_check(uint16_t type)
{
	int i;
	ain_session_t *sess = ain_sessions;
	uint8_t max_sess, sess_opened = 0;

	if (AUDIO_CHANNEL_ADC & type) {
		max_sess = AIN_ADC_SESSION_MAX;
	} else if (AUDIO_CHANNEL_I2SRX & type) {
		max_sess = AIN_I2SRX_SESSION_MAX;
	} else if (AUDIO_CHANNEL_I2SRX1 & type) {
		max_sess = AIN_I2SRX1_SESSION_MAX;
	} else if (AUDIO_CHANNEL_SPDIFRX & type) {
		max_sess = AIN_SPDIFRX_SESSION_MAX;
	} else {
		SYS_LOG_ERR("Invalid session type %d", type);
		return true;
	}

	for (i = 0; i < AIN_SESSION_MAX; i++, sess++) {
		if (AIN_SESSION_CHECK_MAGIC(sess->magic)
			&& (sess->flags & AIN_SESSION_OPEN)
			&& (sess->channel_type & type)) {
			sess_opened++;
			SYS_LOG_DBG("Found aout channel type %d in use", type);
		}
	}

	/* check if the number of opened sessions is bigger than the max limit */
	if (sess_opened >= max_sess) {
		SYS_LOG_ERR("channel type:%d session max %d and opened %d",
				type, max_sess, sess_opened);
		return true;
	}

	return false;
}

/*
 * @brief Get audio in session by specified channel
 */
static ain_session_t *audio_in_session_get(uint16_t type)
{
	ain_session_t *sess = ain_sessions;
	int i;

	if (audio_in_session_check(type))
		return NULL;

	for (i = 0; i < AIN_SESSION_MAX; i++, sess++) {
		if (!(sess->flags & AIN_SESSION_OPEN)) {
			memset(sess, 0, sizeof(ain_session_t));
			sess->magic = AIN_SESSION_MAGIC;
			sess->flags = AIN_SESSION_OPEN;
			sess->channel_type = type;
			sess->id = i;
			return sess;
		}
	}

	return NULL;
}

/*
 * @brief Put audio in session by given session address
 */
static void audio_in_session_put(ain_session_t *s)
{
	ain_session_t *sess = ain_sessions;
	int i;
	for (i = 0; i < AIN_SESSION_MAX; i++, sess++) {
		if ((uint32_t)s == (uint32_t)sess) {
			memset(s, 0, sizeof(ain_session_t));
			break;
		}
	}
}

static int audio_in_dma_config(struct device *dev, ain_channel_node_t *channel_node, dma_setting_t *dma_cfg)
{
	struct acts_audio_in_data *data = dev->driver_data;
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_i2srx1 *i2srx1_reg = cfg->i2srx1;
	struct acts_audio_adc *adc_reg = cfg->adc;
    struct dma_config config_data;
    struct dma_block_config head_block;
	uint32_t dma_length;

    if((channel_node == NULL) || (dma_cfg == NULL)) {
        SYS_LOG_INF("%d, audio in start error!\n", __LINE__);
        return -EINVAL;
    }

    if(channel_node->ain_dma < 0) {
        SYS_LOG_INF("%d, audio in start not get dma!\n", __LINE__);
        return -1;
    }

    memset(&config_data, 0, sizeof(config_data));
    config_data.channel_direction = PERIPHERAL_TO_MEMORY;
    config_data.source_data_size = dma_cfg->dma_width;
    config_data.dma_callback = dma_cfg->stream_handle;
    config_data.callback_data = dma_cfg->stream_pdata;

    if(channel_node->channel_fifo_dst == AIN_FIFO_DST_ASRC) {
        if(channel_node->channel_asrc_index == ASRC_IN_0) {
            config_data.dma_slot = DMA_ID_AUDIO_ASRC_FIFO0;
        } else if(channel_node->channel_asrc_index == ASRC_IN_1) {
            config_data.dma_slot = DMA_ID_AUDIO_ASRC_FIFO1;
        } else {
            SYS_LOG_ERR("%d, in channel param error!\n", __LINE__);
            return -1;
        }
    } else if(channel_node->channel_fifo_dst == AIN_FIFO_DST_DMA) {
        if(channel_node->channel_fifo_sel == AIN_FIFO_ADC) {
            config_data.dma_slot = DMA_ID_AUDIO_ADC_FIFO;
        } else if(channel_node->channel_fifo_sel == AIN_FIFO_I2SRX1) {
            config_data.dma_slot = DMA_ID_AUDIO_I2S;
        } else {
            SYS_LOG_ERR("%d, in channel param error!\n", __LINE__);
            return -1;
        }
    } else {
        SYS_LOG_ERR("%d, in channel param error!\n", __LINE__);
        return -1;
    }

    SYS_LOG_INF("in dma slot: %d %p\n", config_data.dma_slot, config_data.dma_callback);

    if(config_data.dma_callback != NULL) {
        config_data.complete_callback_en = dma_cfg->all_complete_callback_en;
        config_data.half_complete_callback_en = dma_cfg->half_complete_callback_en;
    } else {
        config_data.complete_callback_en = 0;
        config_data.half_complete_callback_en = 0;
    }

    config_data.reserved = dma_cfg->dma_interleaved;

    head_block.source_address = (unsigned int)dma_cfg->dma_src_buf;
    head_block.dest_address = (unsigned int)dma_cfg->dma_dst_buf;
    head_block.block_size = dma_cfg->dma_trans_len;
    head_block.source_reload_en = dma_cfg->dma_reload;
    head_block.dest_reload_en = 0;

    config_data.head_block = &head_block;

    dma_config(data->dma_dev, channel_node->ain_dma, &config_data);

    if(channel_node->channel_fifo_sel == AIN_FIFO_I2SRX1) {
        // use I2S RX1 FIFO
        i2srx1_reg->rx1_fifoctl &= (~I2SRX1_FIFOCTL_FIFO_DMAWIDTH_MASK);
        if(dma_cfg->dma_width == 4) {
            i2srx1_reg->rx1_fifoctl |= I2SRX1_FIFOCTL_DMAWIDTH_32;
        } else if(dma_cfg->dma_width == 2) {
            i2srx1_reg->rx1_fifoctl |= I2SRX1_FIFOCTL_DMAWIDTH_16;
        } else {
            SYS_LOG_ERR("%d, ain dma width config error!\n", __LINE__);
            return -1;
        }
    } else {
        // use ADC FIFO
        adc_reg->fifoctl &= (~ADC_FIFOCTL_FIFO_DMAWIDTH_MASK);
        if(dma_cfg->dma_width == 4) {
            adc_reg->fifoctl |= ADC_FIFOCTL_DMAWIDTH_32;
        } else if(dma_cfg->dma_width == 2) {
            adc_reg->fifoctl |= ADC_FIFOCTL_DMAWIDTH_16;
            SYS_LOG_DBG("%d\n", __LINE__);
        } else {
            SYS_LOG_ERR("%d, ain dma width config error!\n", __LINE__);
            return -1;
        }
    }

    if (channel_node->data_mode == AUDIO_MODE_LEFT_MONO_OTHER_MUTE) {
        dma_length = (channel_node->dma_buf_size << 1);
        dma_reload(data->dma_dev, channel_node->ain_dma, CONFIG_SRAM_END_ADDRESS,
					(uint32_t)channel_node->dma_buf_ptr, dma_length);
    } else if (channel_node->data_mode == AUDIO_MODE_LEFT_RIGHT_SEPERATE){
        dma_length = (channel_node->dma_buf_size << 1);
        dma_reload(data->dma_dev, channel_node->ain_dma,
			(uint32_t)(channel_node->dma_buf_ptr + channel_node->dma_buf_size),
			(uint32_t)channel_node->dma_buf_ptr, dma_length);
    } else if (channel_node->data_mode == AUDIO_MODE_RIGHT_MONO_OTHER_MUTE){
        dma_length = (channel_node->dma_buf_size << 1);
        dma_reload(data->dma_dev, channel_node->ain_dma, (uint32_t)channel_node->dma_buf_ptr,
			CONFIG_SRAM_END_ADDRESS, dma_length);
    } else{
        dma_reload(data->dma_dev, channel_node->ain_dma, 0,
			(uint32_t)channel_node->dma_buf_ptr, channel_node->dma_buf_size);
    }

    return 0;
}

static void _audio_in_read_data_reload_dma_cb(struct device *dev, u32_t priv_data, int reson)
{
	ain_channel_node_t  *channel_node;
	unsigned int ret_reson;

	channel_node = (ain_channel_node_t*)priv_data;

	if (!channel_node || !channel_node->callback)
		return;

	// key = irq_lock();

    if (channel_node->callback) {
        if (reson == DMA_IRQ_HF) {
            ret_reson = AIN_DMA_IRQ_HF;
        } else {
            ret_reson = AIN_DMA_IRQ_TC;
        }

        channel_node->callback(channel_node->callback_data, ret_reson);
    }

	// irq_unlock(key);
}

static int _audio_in_config(struct device *dev, ain_channel_node_t *channel_node)
{
    /* 通路ringbuf dma 配置*/
	dma_setting_t rbuf_dma_cfg = {0};

	struct acts_audio_in_data *data = dev->driver_data;

	if(channel_node->dma_run_flag || data->dma_dev == NULL){
		SYS_LOG_ERR("%d\n", __LINE__);
		return -EIO;
	}

	channel_node->dma_run_flag = false;

	channel_node->ain_dma = dma_request(data->dma_dev, 0xff);

	rbuf_dma_cfg.dma_src_buf = 0;
	rbuf_dma_cfg.dma_dst_buf = 0;
	rbuf_dma_cfg.dma_trans_len = 0;
	rbuf_dma_cfg.dma_width = channel_node->dma_width;
    rbuf_dma_cfg.dma_burst_len = 8;

    if(channel_node->data_mode == AUDIO_MODE_LEFT_MONO_OTHER_MUTE \
        || channel_node->data_mode == AUDIO_MODE_RIGHT_MONO_OTHER_MUTE \
        || channel_node->data_mode == AUDIO_MODE_LEFT_RIGHT_SEPERATE){
        rbuf_dma_cfg.dma_interleaved = true;
    }else{
        rbuf_dma_cfg.dma_interleaved = false;
    }

    rbuf_dma_cfg.stream_handle = _audio_in_read_data_reload_dma_cb;
    rbuf_dma_cfg.dma_reload = 1;

	rbuf_dma_cfg.stream_pdata = channel_node;
	rbuf_dma_cfg.all_complete_callback_en = 1;
	rbuf_dma_cfg.half_complete_callback_en = 1;

	return audio_in_dma_config(dev, channel_node, &rbuf_dma_cfg);
}

extern uint32_t cal_asrc_rate(uint32_t sr_input, uint32_t sr_output, int32_t aps_level);

/**************************************************************
**	Description:	set audio in asrc rate
**	Input:      device structure / handle / rate / offset
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int audio_in_asrc_set_rate(struct device *dev, ain_channel_node_t *channel_node, uint32_t sr_input, uint32_t sr_output)
{
    u32_t asrc_offset;

	if((channel_node == NULL) || (channel_node->channel_asrc_index <= ASRC_OUT_MAX) || (channel_node->channel_asrc_index >= ASRC_TYPE_MAX)) {
		SYS_LOG_ERR("%d, ain set asrc rate fail!\n", __LINE__);
		return -1;
	}

	asrc_offset = cal_asrc_rate(sr_input, sr_output, ASRC_LEVEL_DEFAULT);

	return PHY_set_asrc_in_rate(dev, channel_node, 0, asrc_offset);
}

/**************************************************************
**	Description:	get one input channel info
**	Input:		device / channel node
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int audio_in_get_channel_info(struct device *dev, ain_channel_node_t *channel_node, uint8_t index)
{
	ain_channel_info_e info;
	int result;

	if (channel_node == NULL) {
		return -1;
	}

	if ((ain_channel_info_e)index >= GET_AIN_INF_MAX) {
		return -1;
	}

	info = (ain_channel_info_e)index;

	switch(info) {
		case GET_AIN_FIFO_USING:
			result = (int)channel_node->channel_fifo_sel;
			break;

		case GET_AIN_FIFO_DST:
			result = (int)channel_node->channel_fifo_dst;
			break;

		case GET_AIN_ASRC_USING:
			if ((channel_node->channel_asrc_index != ASRC_IN_0)
				&& (channel_node->channel_asrc_index != ASRC_IN_1)) {
				result = -1;
			} else {
				result = (int)(channel_node->channel_asrc_index);
			}
			break;

		case GET_AIN_SAMPLERATE_STATUS:
			if (channel_node->channel_sample_status >= RX_SR_STATUS_GET) {
				result = 1;
			} else {
				result = 0;
			}
			break;

		case GET_AIN_SAMPLERATE_VALUE:
			result = (int)channel_node->channel_sample_rate;
			break;

		default:
			result = -1;
			break;
	}

	return result;
}

/**************************************************************
**	Description:	reset audio in asrc fifo
**	Input:      device structure / handle / rate / offset
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int audio_in_reset_asrc_in_fifo(struct device *dev, ain_channel_node_t *channel_node)
{
	if((channel_node == NULL) || (channel_node->channel_asrc_index <= ASRC_OUT_MAX) || (channel_node->channel_asrc_index >= ASRC_TYPE_MAX)) {
		return -1;
	}

	return PHY_reset_asrc_in_fifo(dev, channel_node);
}

/* @brief Enable the ADC channel */
static int audio_in_enable_adc(struct device *dev, ain_session_t * session, ain_param_t *param)
{
	ain_channel_node_t *channel_node = &session->channel;
	int ret;
	uint8_t physical_dev, track_flag;
	input_setting_t input_setting = {0};
	PHY_audio_in_param_t phy_param = {0};
	phy_adc_setting_t adc_setting = {0};
	int16_t gain;
	uint16_t input_gain = 0, digital_gain = 0;

	if (!session || !channel_node) {
		SYS_LOG_ERR("Invalid parameters");
		return -EINVAL;
	}

	if ((param->channel_width != CHANNEL_WIDTH_16BITS)
		&& (param->channel_width != CHANNEL_WIDTH_24BITS)) {
		SYS_LOG_ERR("Invalid channel width %d", param->channel_width);
		return -EINVAL;
	}

	if (!param->adc_setting) {
		SYS_LOG_ERR("invalid adc setting");
		return -EINVAL;
	}

	ret = board_audio_device_mapping(param->adc_setting->device, &physical_dev, &track_flag);
	if (ret) {
		SYS_LOG_ERR("map audio device:0x%x error:%d", param->adc_setting->device, ret);
		return ret;
	}

	if (physical_dev == AIN_SOURCE_DMIC)
		channel_node->channel_type = AIN_CHANNEL_DMIC;
	else
		channel_node->channel_type = AIN_CHANNEL_ADC;

	channel_node->channel_fifo_sel = AIN_FIFO_ADC;
	channel_node->channel_src_type = physical_dev;

	phy_param.ptr_input_setting = &input_setting;
	phy_param.ptr_adc_setting = &adc_setting;
	phy_param.ain_src = physical_dev;
	phy_param.ain_fifo_dst = channel_node->channel_fifo_dst;
	phy_param.ain_channel_type = channel_node->channel_type;
	phy_param.channel_apll_sel = APLL_ALLOC_AUTO;

	input_setting.ain_track = track_flag;
	gain = param->adc_setting->gain.ch_gain[0];

	if ((physical_dev == AIN_SOURCE_AUX0) || (physical_dev == AIN_SOURCE_AUX1)
		|| (physical_dev == AIN_SOURCE_AUX2)) {
		ret = adc_aux_se_gain_translate(gain, &input_gain, &digital_gain);
		if (ret)
			return ret;
	} else if ((physical_dev == AIN_SOURCE_ASEMIC) || (physical_dev == AIN_SOURCE_AFDMIC)) {
		ret = adc_amic_gain_translate(gain, &input_gain, &digital_gain);
		if (ret)
			return ret;

		if (gain < 260)
			input_setting.input_gain_force_0db = 1;

	} else if (physical_dev == AIN_SOURCE_AUXFD) {
		ret = adc_aux_fd_gain_translate(gain, &input_gain, &digital_gain);
		if (ret)
			return ret;
	} else if ((physical_dev == AIN_SOURCE_AFDMIC_AUXFD) || (physical_dev == AIN_SOURCE_ASEMIC_AUXFD)) {
		ret = adc_amic_gain_translate(gain, &input_gain, &digital_gain);
		if (ret)
			return ret;

		if (gain < 260)
			input_setting.input_gain_force_0db = 1;

		uint16_t _input_gain=0, _digital_gain=0;
		gain = param->adc_setting->gain.ch_gain[1];
		if (gain != ADC_GAIN_INVALID) {
			ret = adc_aux_fd_gain_translate(gain, &_input_gain, &_digital_gain);
			if (ret)
				return ret;
		}

		if (_input_gain)
			input_gain |= (1 << 8);

	}  else if ((physical_dev == AIN_SOURCE_AFDMIC_AUX2) || (physical_dev == AIN_SOURCE_ASEMIC_AUX2)) {
		ret = adc_amic_gain_translate(gain, &input_gain, &digital_gain);
		if (ret)
			return ret;

		if (gain < 260)
			input_setting.input_gain_force_0db = 1;

		uint16_t _input_gain=0, _digital_gain=0;
		gain = param->adc_setting->gain.ch_gain[1];
		if (gain != ADC_GAIN_INVALID) {
			ret = adc_aux_se_gain_translate(gain, &_input_gain, &_digital_gain);
			if (ret)
				return ret;
		}

		if (_input_gain)
			input_gain |= ((_input_gain & 0xFF) << 8);

	} else if (physical_dev == AIN_SOURCE_DMIC) {
		ret = adc_dmic_gain_translate(gain, &input_gain, &digital_gain);
		if (ret)
			return ret;
	}

	input_setting.input_gain = input_gain;
	adc_setting.adc_gain = digital_gain;
	adc_setting.adc_sample_rate = param->sample_rate;

	if (channel_node->channel_type == AIN_CHANNEL_DMIC) {
		ret = PHY_audio_enable_dmic(dev, &phy_param, channel_node);
	} else {
		ret = PHY_audio_enable_inputsrc(dev, &phy_param);
		if (ret) {
			SYS_LOG_ERR("enable inputsrc error:%d", ret);
			return ret;
		}

		if (session->channel_type == AUDIO_CHANNEL_AA)
			return 0;

		ret = PHY_audio_enable_adc(dev, &phy_param, channel_node);
	}

	return ret;
}

/* @brief Enable the I2SRX channel */
static int audio_in_enable_i2srx(struct device *dev, ain_session_t *session, ain_param_t *param)
{
	ain_channel_node_t *channel_node = &session->channel;
	PHY_audio_in_param_t phy_param = {0};
	phy_i2srx_setting_t i2srx_setting = {0};

	if (!session || !channel_node) {
		SYS_LOG_ERR("Invalid parameters");
		return -EINVAL;
	}

	if ((param->channel_width != CHANNEL_WIDTH_16BITS)
		&& (param->channel_width != CHANNEL_WIDTH_20BITS)
		&& (param->channel_width != CHANNEL_WIDTH_24BITS)) {
		SYS_LOG_ERR("Invalid channel width %d", param->channel_width);
		return -EINVAL;
	}

	if (session->channel_type == AUDIO_CHANNEL_I2SRX) {
		channel_node->channel_type = AIN_CHANNEL_I2SRX0;
		channel_node->channel_fifo_sel = AIN_FIFO_ADC;
		channel_node->channel_src_type = AIN_SOURCE_I2S0;
#ifdef CONFIG_I2SRX0_MODE
		i2srx_setting.i2srx_role = CONFIG_I2SRX0_MODE;
#else
		i2srx_setting.i2srx_role = I2S_DEVICE_SLAVE;
#endif
#ifdef CONFIG_I2SRX0_FORMAT
		i2srx_setting.i2srx_format = CONFIG_I2SRX0_FORMAT;
#else
		i2srx_setting.i2srx_format = I2S_FORMAT_I2S;
#endif
	} else {
		channel_node->channel_type = AIN_CHANNEL_I2SRX1;
		channel_node->channel_fifo_sel = AIN_FIFO_I2SRX1;
		channel_node->channel_src_type = AIN_SOURCE_I2S1;
#ifdef CONFIG_I2SRX1_MODE
		i2srx_setting.i2srx_role = CONFIG_I2SRX1_MODE;
#else
		i2srx_setting.i2srx_role = I2S_DEVICE_SLAVE;
#endif

#ifdef CONFIG_I2SRX1_FORMAT
		i2srx_setting.i2srx_format = CONFIG_I2SRX1_FORMAT;
#else
		i2srx_setting.i2srx_format = I2S_FORMAT_I2S;
#endif
	}

	phy_param.ptr_i2srx_setting = &i2srx_setting;
	i2srx_setting.i2srx_sample_rate = param->sample_rate;

	if (param->i2srx_setting && param->i2srx_setting->srd_callback) {
		i2srx_setting.srd_callback = param->i2srx_setting->srd_callback;
		i2srx_setting.cb_data = param->i2srx_setting->cb_data;
	}

	phy_param.ain_src = channel_node->channel_src_type;
	phy_param.ain_fifo_dst = channel_node->channel_fifo_dst;
	phy_param.ain_channel_type = channel_node->channel_type;
	phy_param.channel_apll_sel = APLL_ALLOC_AUTO;

	return PHY_audio_enable_i2srx(dev, &phy_param, channel_node);
}

/* @brief Enable the SPDIFRX channel */
static int audio_in_enable_spdifrx(struct device *dev, ain_session_t * session, ain_param_t *param)
{
	ain_channel_node_t *channel_node = &session->channel;
	PHY_audio_in_param_t phy_param = {0};
	phy_spdifrx_setting_t spdifrx_setting = {0};

	if (!session || !channel_node) {
		SYS_LOG_ERR("Invalid parameters");
		return -EINVAL;
	}

	if ((param->channel_width != CHANNEL_WIDTH_16BITS)
		&& (param->channel_width != CHANNEL_WIDTH_24BITS)) {
		SYS_LOG_ERR("Invalid channel width %d", param->channel_width);
		return -EINVAL;
	}

	channel_node->channel_type = AIN_CHANNEL_SPDIF;
	channel_node->channel_fifo_sel = AIN_FIFO_I2SRX1;
	channel_node->channel_src_type = AIN_SOURCE_SPDIF;

	phy_param.ain_src = channel_node->channel_src_type;
	phy_param.ain_fifo_dst = channel_node->channel_fifo_dst;
	phy_param.ain_channel_type = channel_node->channel_type;

	phy_param.ptr_spdifrx_setting = &spdifrx_setting;
	spdifrx_setting.spdifrx_callback = param->spdifrx_setting->srd_callback;
	spdifrx_setting.callback_data = param->spdifrx_setting->cb_data;

	return PHY_audio_enable_spdifrx(dev, &phy_param, channel_node);
}

/* @brief Disable the ADC channel */
static int audio_in_disable_adc(struct device *dev, ain_session_t *session)
{
	ain_channel_node_t *channel_node = &session->channel;
	int ret;

	if (channel_node->channel_type == AIN_CHANNEL_DMIC) {
		ret = PHY_audio_disable_dmic(dev);
	} else {
		if (session->channel_type == AUDIO_CHANNEL_ADC) {
			ret = PHY_audio_disable_adc(dev, channel_node);
			if (ret) {
				SYS_LOG_ERR("disable inputsrc error:%d", ret);
				return ret;
			}
		}

		ret = PHY_audio_disable_inputsrc(dev, channel_node);
	}

	return ret;
}

/* @brief Disable the I2SRX channel */
static int audio_in_disable_i2srx(struct device *dev, ain_session_t *session)
{
	ain_channel_node_t *channel_node = &session->channel;

	return PHY_audio_disable_i2srx(dev, channel_node);
}


/* @brief Disable the SPDIFRX channel */
static int audio_in_disable_spdifrx(struct device *dev, ain_session_t *session)
{
	ARG_UNUSED(session);

	return PHY_audio_disable_spdifrx(dev);
}

/* @brief prepare the DMA configuration for the audio in transfer */
static int audio_in_dma_prepare(struct device *dev, ain_session_t *session)
{
	int ret;
	ain_channel_node_t *channel_node = &session->channel;

	ret = _audio_in_config(dev, channel_node);
	if (ret) {
        SYS_LOG_ERR("audio out dma config error:%d", ret);
		return ret;
	}

	return 0;
}

/* @brief ASRC command and control */
int audio_in_asrc_ioctl(struct device *dev, void *handle, int cmd, void *param)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_i2srx1 *i2srx1_reg = cfg->i2srx1;
	struct acts_audio_adc *adc_reg = cfg->adc;
	ain_session_t *session = (ain_session_t *)handle;
	ain_channel_node_t *channel_node = &session->channel;
	int ret = 0;

	switch (cmd) {
		case AIN_CMD_ASRC_ENABLE:
		{
			phy_asrc_param_t asrc_param = {0};
			PHY_audio_in_param_t phy_param = {0};
			phy_param.ptr_asrc_setting = &asrc_param;
			asrc_in_setting_t *setting = (asrc_in_setting_t *)param;
			if (!setting) {
				SYS_LOG_ERR("invalid asrc setting");
				return -EINVAL;
			}

			SYS_LOG_INF("Enable ASRC mode:%d mem policy:%d rclk:%d out_sr:%d",
						setting->mode, setting->mem_policy,
						setting->rclk,
						setting->output_samplerate);

			if ((channel_node->channel_fifo_sel != AIN_FIFO_ADC)
				&& (channel_node->channel_fifo_sel != AIN_FIFO_I2SRX1)) {
				SYS_LOG_ERR("Invalid fifo type:%d", channel_node->channel_fifo_sel);
				return -EINVAL;
			}

			if ((setting->rclk != ASRC_IN_RCLK_CPU)
				&& (setting->rclk != ASRC_IN_RCLK_DMA)
				&& (setting->rclk != ASRC_IN_RCLK_DSP)) {
				SYS_LOG_ERR("Invalid ASRC write clock:%d", setting->rclk);
				return -EINVAL;
			}

			if ((setting->mem_policy != ASRC_ALLOC_AUTO)
				&& (setting->mem_policy != ASRC_ALLOC_LOW_LATENCY)) {
				SYS_LOG_ERR("Invalid ASRC memory policy:%d", setting->mem_policy);
				return -EINVAL;
			}

			channel_node->channel_fifo_dst = AIN_FIFO_DST_ASRC;
			if (channel_node->channel_fifo_sel == AIN_FIFO_ADC) {
				adc_reg->fifoctl &= ~ADC_FIFOCTL_ADFOS_MASK;
				adc_reg->fifoctl |= ADC_FIFOCTL_ADFOS_ASRC;
			} else {
				i2srx1_reg->rx1_fifoctl &= ~I2SRX1_FIFOCTL_RXFOS_MASK;
				i2srx1_reg->rx1_fifoctl |= I2SRX1_FIFOCTL_RXFOS_ASRC;
			}

			phy_param.ain_src = channel_node->channel_src_type;
			phy_param.ain_fifo_dst = channel_node->channel_fifo_dst;
			phy_param.ain_channel_type = channel_node->channel_type;
			phy_param.asrc_alloc_method = setting->mem_policy;
			asrc_param.by_pass = (setting->mode == BYPASS_MODE) ? 1 : 0;

			if (!asrc_param.by_pass)
				asrc_param.ctl_mode = setting->mode;

			asrc_param.input_samplerate = channel_node->channel_sample_rate;
			asrc_param.output_samplerate = setting->output_samplerate;

			asrc_param.ctl_dma_width = channel_node->dma_width * 8;
			asrc_param.ctl_rd_clk = setting->rclk;

			ret = prepare_asrc_in_param(dev, &phy_param, channel_node);
			if (ret) {
				SYS_LOG_ERR("prepare asrc in error:%d", ret);
				return ret;
			}

			ret = audio_in_asrc_config(dev, &asrc_param);
			if (!ret)
				session->asrc_enable = 1;

			break;
		}
		case AIN_CMD_SET_ASRC_IN_SAMPLE_RATE:
		{
			asrc_sample_rate_t *sr_setting = (asrc_sample_rate_t *)param;
			if (!sr_setting) {
				SYS_LOG_ERR("Invalid asrc sample rate setting");
				return -EINVAL;
			}

			ret = audio_in_asrc_set_rate(dev, channel_node,
					sr_setting->input_sample_rate, sr_setting->output_sample_rate);
			break;
		}
		case AIN_CMD_RESET_ASRC_INFO:
		{
			ret = audio_in_reset_asrc_in_fifo(dev, channel_node);
			break;
		}
		default:
		{
			SYS_LOG_ERR("Unsupport ASRC command:%d", cmd);
			ret = -ENOTSUP;
			break;
		}
	}

	return ret;
}

/**************************************************************
**	Description: open an audio input channel by spcified input parameters.
**	Input: device structure / input parameters
**	Output:
**	Return: NULL or audio input channel handler
**	Note:
***************************************************************
**/
static void *acts_audio_in_open(struct device *dev, ain_param_t *param)
{
	ain_session_t * session = NULL;
	ain_channel_node_t *channel_node;
	uint8_t channel_type;
	int ret;
	void *rtn_sess = NULL;

	if (!param) {
		SYS_LOG_ERR("NULL parameter");
		return NULL;
	}

	drv_audio_in_lock();

	channel_type = param->channel_type;
	session = audio_in_session_get(channel_type);
	if (!session) {
		SYS_LOG_ERR("Failed to get audio session (type:%d)", channel_type);
		goto out;
	}

	if (param->reload_setting.reload_addr && !param->reload_setting.reload_len) {
		SYS_LOG_ERR("Reload parameter error addr:0x%x, len:0x%x",
			(uint32_t)param->reload_setting.reload_addr, param->reload_setting.reload_len);
		audio_in_session_put(session);
		goto out;
	}

	channel_node = &session->channel;
	session->channel_type = channel_type;
	channel_node->channel_sample_rate = param->sample_rate;
	channel_node->callback = param->callback;
	channel_node->callback_data = param->cb_data;
	channel_node->channel_fifo_dst = AOUT_FIFO_SRC_DMA;
    channel_node->dma_reload = 1;
	channel_node->dma_run_flag = 0;
	channel_node->dma_width = (CHANNEL_WIDTH_16BITS == param->channel_width) ? 2 : 4;
    channel_node->dma_buf_ptr = param->reload_setting.reload_addr;
    channel_node->dma_buf_size = param->reload_setting.reload_len;

	if ((channel_type == AUDIO_CHANNEL_ADC)
		|| (channel_type == AUDIO_CHANNEL_AA)) {
		ret = audio_in_enable_adc(dev, session, param);
		session->input_dev = param->adc_setting->device;
	} else if ((channel_type == AUDIO_CHANNEL_I2SRX)
		|| (channel_type == AUDIO_CHANNEL_I2SRX1)) {
		ret = audio_in_enable_i2srx(dev, session, param);
	} else if (channel_type == AUDIO_CHANNEL_SPDIFRX) {
		ret = audio_in_enable_spdifrx(dev, session, param);
	} else {
		SYS_LOG_ERR("Invalid channel type %d", channel_type);
		audio_in_session_put(session);
		goto out;
	}

	if (ret) {
		SYS_LOG_ERR("Enable channel type %d error:%d", channel_type, ret);
		audio_in_session_put(session);
		goto out;
	}

	session->flags |= AIN_SESSION_CONFIG;

	/* The session address is the audio in handler */
	rtn_sess = (void *)session;

	SYS_LOG_INF("channel@%d sess:%p type:%d input_dev:0x%x opened",
			session->id, session, channel_type, session->input_dev);

out:
	drv_audio_in_unlock();
	return rtn_sess;
}

/**************************************************************
**	Description: close an audio input channel by specified input channel handler.
**	Input: device structure / input channel handler
**	Output:
**	Return: success/fail
**	Note:
***************************************************************
**/
static int acts_audio_in_close(struct device *dev, void *handle)
{
	struct acts_audio_in_data *data = dev->driver_data;
	uint8_t channel_type;
	int ret;
	ain_session_t *session = (ain_session_t *)handle;
	ain_channel_node_t *channel_node = &session->channel;

	if (!handle) {
		SYS_LOG_ERR("Invalid handle");
		return -EINVAL;
	}

	drv_audio_in_lock();

	if (!AIN_SESSION_CHECK_MAGIC(session->magic)) {
		SYS_LOG_ERR("Session magic error:0x%x", session->magic);
		ret = -EFAULT;
		goto out;
	}

	channel_type = session->channel_type;

	if ((channel_type == AUDIO_CHANNEL_ADC)
		|| (channel_type == AUDIO_CHANNEL_AA)) {
		ret = audio_in_disable_adc(dev, session);
	} else if ((channel_type == AUDIO_CHANNEL_I2SRX)
		|| (channel_type == AUDIO_CHANNEL_I2SRX1)) {
		ret = audio_in_disable_i2srx(dev, session);
	} else if (channel_type == AUDIO_CHANNEL_SPDIFRX) {
		ret = audio_in_disable_spdifrx(dev, session);
	} else {
		SYS_LOG_ERR("Invalid channel type %d", channel_type);
		ret = -EINVAL;
		goto out;
	}

	if (ret) {
		SYS_LOG_ERR("Disable channel type %d error:%d", channel_type, ret);
		goto out;
	}

	/* stop and free dma channel resource */
	if (data->dma_dev && channel_node->ain_dma) {
 		dma_stop(data->dma_dev, channel_node->ain_dma);
 		dma_free(data->dma_dev, channel_node->ain_dma);
	}

	if ((session->asrc_enable)
		&& ((channel_node->channel_asrc_index == ASRC_IN_0)
		|| (channel_node->channel_asrc_index == ASRC_IN_1))) {
		audio_in_asrc_close(dev, channel_node->channel_asrc_index);
	}

 	SYS_LOG_INF("session#%d@%p closed", session->id, session);

	audio_in_session_put(session);

out:
	drv_audio_in_unlock();
	return ret;
}

/**************************************************************
**	Description: start the specified audio input channel.
**	Input: device structure / input channel handler
**	Output:
**	Return: success/fail
**	Note:
***************************************************************
**/
static int acts_audio_in_start(struct device *dev, void *handle)
{
	ain_session_t *session = (ain_session_t *)handle;
	ain_channel_node_t *channel_node = &session->channel;
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_i2srx1 *i2srx1_reg = cfg->i2srx1;
	struct acts_audio_in_data *data = dev->driver_data;

	if (!handle) {
		SYS_LOG_ERR("Invalid handle");
		return -EINVAL;
	}

	if (!AIN_SESSION_CHECK_MAGIC(session->magic)) {
		SYS_LOG_ERR("Session magic error:0x%x", session->magic);
		return -EFAULT;
	}

	/* In DMA reload mode only start one time is enough */
	if (session->flags & AIN_SESSION_START) {
		SYS_LOG_DBG("session%p already started", session);
		return 0;
	}

	if (audio_in_dma_prepare(dev, session)) {
		SYS_LOG_ERR("DMA prepare error");
		return -EACCES;
	}

	SYS_LOG_INF("channel %d %d", channel_node->channel_type, channel_node->channel_src_type);

	if (channel_node->ain_dma) {
		channel_node->dma_run_flag = true;

		if (channel_node->channel_src_type == AIN_SOURCE_I2S1) {
			i2srx1_reg->rx1_fifoctl &= ~I2SRX1_FIFOCTL_RXFRT;
			k_busy_wait(100);
			i2srx1_reg->rx1_fifoctl |= I2SRX1_FIFOCTL_RXFRT;
		}

		if (!session->dma_prepare_start) {
			return dma_start(data->dma_dev, channel_node->ain_dma);
		} else {
			return dma_prepare_start(data->dma_dev, channel_node->ain_dma);
		}
	} else {
		SYS_LOG_ERR("ain dma null");
		return -EIO;
	}

	return 0;
}

/**************************************************************
**	Description: stop the specified audio input channel.
**	Input: device structure / input channel handler
**	Output:
**	Return: success/fail
**	Note:
***************************************************************
**/
static int acts_audio_in_stop(struct device *dev, void *handle)
{
	struct acts_audio_in_data *data = dev->driver_data;
	ain_session_t *session = (ain_session_t *)handle;
	ain_channel_node_t *channel_node = &session->channel;
	int ret = 0;

	if (session && AIN_SESSION_CHECK_MAGIC(session->magic)) {

		SYS_LOG_INF("session#%p, audio in stop", session);
		if (data->dma_dev && channel_node->ain_dma)
			ret = dma_stop(data->dma_dev, channel_node->ain_dma);
	}

	return ret;
}

/**************************************************************
**	Description: send comand and control to the specified input channel handler.
**	Input: device structure / input channel handler / command / command parameters
**	Output:
**	Return: success/fail
**	Note:
***************************************************************
**/
static int acts_audio_in_control(struct device *dev, void *handle, int cmd, void *param)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_i2srx1 *i2srx1_reg = cfg->i2srx1;
	struct acts_audio_adc *adc_reg = cfg->adc;
	ain_session_t *session = (ain_session_t *)handle;
	ain_channel_node_t *channel_node = &session->channel;
	uint8_t channel_type = session->channel_type;
	int ret = 0;

	SYS_LOG_DBG("session#0x%x cmd 0x%x", (uint32_t)handle, cmd);

	if (!AIN_SESSION_CHECK_MAGIC(session->magic)) {
		SYS_LOG_ERR("Session magic error:0x%x", session->magic);
		return -EFAULT;
	}

	if (AIN_CMD_SET_PREPARE_START == cmd) {
		session->dma_prepare_start = 1;
		SYS_LOG_INF("Enable prepare dma start");
		goto out;
	}

	if (AIN_CMD_GET_CH_INFO == cmd) {
		uint8_t index = *(uint8_t *)param;
		ret = audio_in_get_channel_info(dev, channel_node, index);
		if (ret < 0) {
			SYS_LOG_ERR("Get channel info (index:%d) error:%d", index, ret);
			goto out;
		}
		*(uint8_t *)param = ret;
		ret = 0;
		goto out;
	}

	if (AIN_CMD_SET_INTERLEAVED_MODE == cmd) {
		uint8_t mode = *(uint8_t *)param;
		if ((mode != LEFT_MONO_RIGHT_MUTE_MODE)
			&& (mode != LEFT_MUTE_RIGHT_MONO_MODE)
			&& (mode != LEFT_RIGHT_SEPERATE)) {
			SYS_LOG_ERR("Invalid interleave mode:%d", mode);
			return -EINVAL;
		}

		if (mode == LEFT_MONO_RIGHT_MUTE_MODE)
			channel_node->data_mode = AUDIO_MODE_LEFT_MONO_OTHER_MUTE;
		else if (mode == LEFT_MUTE_RIGHT_MONO_MODE)
			channel_node->data_mode = AUDIO_MODE_RIGHT_MONO_OTHER_MUTE;
		else
			channel_node->data_mode = AUDIO_MODE_LEFT_RIGHT_SEPERATE;

		session->interleave_en = 1;

		SYS_LOG_INF("Enable DMA interleave mode:%d", mode);
		goto out;
	}

	if (AIN_CMD_DSP_ENABLE == cmd) {
		if (session->asrc_enable) {
			SYS_LOG_ERR("FIFO target already set to ASRC");
			return -EPERM;
		}
		channel_node->channel_fifo_dst = AIN_FIFO_DST_DSP;
		if (channel_node->channel_fifo_sel == AIN_FIFO_ADC) {
			adc_reg->fifoctl &= ~ADC_FIFOCTL_ADFOS_MASK;
			adc_reg->fifoctl |= ADC_FIFOCTL_ADFOS_DSP;
		} else {
	        i2srx1_reg->rx1_fifoctl &= ~I2SRX1_FIFOCTL_RXFOS_MASK;
	        i2srx1_reg->rx1_fifoctl |= I2SRX1_FIFOCTL_RXFOS_DSP;
		}
		session->dsp_enable = 1;
		goto out;
	}

	if (AIN_IS_ASRC_CMD(cmd)) {
		/* In case of the commands belong to ASRC */
		ret = audio_in_asrc_ioctl(dev, handle, cmd, param);
	} else {
		if (channel_type & AUDIO_CHANNEL_ADC) {
			ret = PHY_audio_ioctl_adc(dev, channel_node, cmd, param);
		} else if (channel_type & AUDIO_CHANNEL_I2SRX) {
			ret = PHY_audio_ioctl_i2srx(dev, channel_node, cmd, param);
		} else if (channel_type & AUDIO_CHANNEL_SPDIFRX) {
			ret = PHY_audio_ioctl_spdifrx(dev, channel_node, cmd, param);
		} else {
			ret = -ENXIO;
		}
	}

out:
	return ret;
}

/**************************************************************
**	Description:	audio in device driver init
**	Input:      device structure
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int audio_in_init(struct device *dev)
{
	struct acts_audio_in_data *data = dev->driver_data;

	SYS_LOG_INF("init audio in device");

	memset(data, 0, sizeof(struct acts_audio_in_data));

	data->dma_dev = device_get_binding(CONFIG_DMA_0_NAME);

	// only prompt, maybe the channel not using dma
	if(data->dma_dev == NULL) {
		SYS_LOG_ERR("%d, ain dma device not found!\n", __LINE__);
	}

	data->cur_asrc_in0_ramindex = 0xff;
	data->cur_asrc_in1_ramindex = 0xff;

	return 0;
}

static struct acts_audio_in_data audio_in_acts_data;

static const struct acts_audio_in_config audio_in_acts_config = {
	.asrc = (struct acts_audio_asrc *)ASRC_REG_BASE,

	.adc = (struct acts_audio_adc *) AUDIO_ADC_REG_BASE,
	.i2srx0 = (struct acts_audio_i2srx0 *)AUDIO_I2SRX0_REG_BASE,
	.i2srx1 = (struct acts_audio_i2srx1 *)AUDIO_I2SRX1_REG_BASE,
	.spdifrx = (struct acts_audio_spdifrx *)AUDIO_SPDIFRX_REG_BASE,
};

const struct ain_driver_api ain_drv_api = {
	.ain_open = acts_audio_in_open,
	.ain_close = acts_audio_in_close,
	.ain_control = acts_audio_in_control,
	.ain_start = acts_audio_in_start,
	.ain_stop = acts_audio_in_stop
};

DEVICE_AND_API_INIT(audio_in, CONFIG_AUDIO_IN_ACTS_DEV_NAME, audio_in_init,
	    &audio_in_acts_data, &audio_in_acts_config,
	    POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &ain_drv_api);

void audio_in_i2srx_irq_enable(ain_channel_type_e channel_type)
{
	if (channel_type == AIN_CHANNEL_I2SRX0) {
		if (irq_is_enabled(IRQ_ID_AUDIO_I2S0_RX) == 0) {
			IRQ_CONNECT(IRQ_ID_AUDIO_I2S0_RX, CONFIG_IRQ_PRIO_NORMAL,
				phy_i2srx_isr, DEVICE_GET(audio_in), 0);
			irq_enable(IRQ_ID_AUDIO_I2S0_RX);
		}
	} else if (channel_type == AIN_CHANNEL_I2SRX1) {
		if (irq_is_enabled(IRQ_ID_AUDIO_I2S1_RX) == 0) {
			IRQ_CONNECT(IRQ_ID_AUDIO_I2S1_RX, CONFIG_IRQ_PRIO_NORMAL,
				phy_i2srx_isr, DEVICE_GET(audio_in), 0);
			irq_enable(IRQ_ID_AUDIO_I2S1_RX);
		}
	}
}

