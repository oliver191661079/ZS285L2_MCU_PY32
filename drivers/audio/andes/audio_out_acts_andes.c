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
 * Description:    audio out driver for acts
 *
 * Change log:
 *	2018/9/26: Created by mikeyang.
 *	2021/06/09 Modified by SRD3 team.
 ****************************************************************************
 */

#include "audio_inner.h"
#include "audio_asrc.h"
#include <stdlib.h>
#include <kernel.h>
#include <init.h>
#include <device.h>
#include <irq.h>
#include <dma.h>
#ifdef CONFIG_AMP_AW882XX
#include <amp.h>
#endif
#if defined(CONFIG_CODEC_ACM8635)
#include <amp.h>
#endif
#include <soc.h>
#include <string.h>
#include <audio_out.h>

//#define RELOAD_SAM_OPT

#ifdef RELOAD_SAM_OPT
static uint32_t audio_dummy_data;
#endif

/* Audio out session resource definitions */
#define AOUT_SESSION_MAGIC                 (0x1a2b3c4d)
#define AOUT_SESSION_CHECK_MAGIC(x)        ((x) == AOUT_SESSION_MAGIC)
#define AOUT_DAC_SESSION_MAX               (2) /* number of DAC channels */

#ifdef CONFIG_AUDIO_OUT_I2STX0_SUPPORT
#define AOUT_I2STX_SESSION_MAX             (1) /* number of  I2STX channel */
#else
#define AOUT_I2STX_SESSION_MAX             (0) /* number of  I2STX channel */
#endif

#ifdef CONFIG_AUDIO_OUT_SPDIFTX_SUPPORT
#define AOUT_SPDIFTX_SESSION_MAX           (2) /* number of  SPDIFTX channel */
#else
#define AOUT_SPDIFTX_SESSION_MAX           (0) /* number of  SPDIFTX channel */
#endif

#define AOUT_SESSION_MAX                   (AOUT_DAC_SESSION_MAX + AOUT_I2STX_SESSION_MAX + AOUT_SPDIFTX_SESSION_MAX) /* totally max sessions */
#define AOUT_SESSION_OPEN                  (1 << 0) /* session open flag */
#define AOUT_SESSION_CONFIG                (1 << 1) /* session config flag */
#define AOUT_SESSION_START                 (1 << 2) /* session start flag */

#define AOUT_SESSION_WAIT_PA_TIMEOUT       (2000) /* timeout of waitting pa session */

/* audio out io-commands following by the AUDIO FIFO usage */
#define AOUT_IS_FIFO_CMD(x)                ((x) & AOUT_FIFO_CMD_FLAG)

/* The commands belong to ASRC */
#define AOUT_IS_ASRC_CMD(x)                ((x) & AOUT_ASRC_CMD_FLAG)

#define AOUT_DMA_WAIT_TIMEOUT_US           (1000) /* timeout to wait FIFO empty status*/

/**
 * struct aout_session_t
 * @brief audio out session structure
 */
typedef struct {
	int magic; /* session magic value as AOUT_SESSION_MAGIC */
	uint8_t flags; /* session flags */
	uint8_t id; /* the session id */
	uint16_t channel_type; /* session channel type */
	aout_channel_node_t channel; /* audio output channel */
	uint8_t dma_prepare_start : 1; /* use DMA prepare start flag */
	uint8_t asrc_enable : 1; /* ASRC enable flag */
	uint8_t dsp_enable : 1; /* FIFO input data from DSP enable flag */
} aout_session_t;

/* audio out sessions */
static aout_session_t aout_sessions[AOUT_SESSION_MAX];

/*
 * @brief check if there is the same channel within audio out sessions.
 * Moreover check whether the session type is invalid or not.
 */
static bool audio_out_session_check(uint16_t type)
{
	int i;
	aout_session_t *sess = aout_sessions;
	uint8_t max_sess, sess_opened = 0;

	if (AUDIO_CHANNEL_DAC & type) {
		max_sess = AOUT_DAC_SESSION_MAX;
	} else if (AUDIO_CHANNEL_I2STX & type) {
		max_sess = AOUT_I2STX_SESSION_MAX;
	} else if (AUDIO_CHANNEL_SPDIFTX & type) {
		max_sess = AOUT_SPDIFTX_SESSION_MAX;
	} else {
		SYS_LOG_ERR("Invalid session type %d", type);
		return true;
	}

	for (i = 0; i < AOUT_SESSION_MAX; i++, sess++) {
		if (AOUT_SESSION_CHECK_MAGIC(sess->magic)
			&& (sess->flags & AOUT_SESSION_OPEN)
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
 * @brief Get audio out session by specified channel type.
 */
static aout_session_t *audio_out_session_get(uint16_t type)
{
	aout_session_t *sess = aout_sessions;
	int i;

	if (audio_out_session_check(type))
		return NULL;

	for (i = 0; i < AOUT_SESSION_MAX; i++, sess++) {
		if (!(sess->flags & AOUT_SESSION_OPEN)) {
			memset(sess, 0, sizeof(aout_session_t));
			sess->magic = AOUT_SESSION_MAGIC;
			sess->flags = AOUT_SESSION_OPEN;
			sess->channel_type = type;
			sess->id = i;
			return sess;
		}
	}

	return NULL;
}

/*
 * @brief Put audio out session by given session address
 */
static void audio_out_session_put(aout_session_t *s)
{
	aout_session_t *sess = aout_sessions;
	int i;
	for (i = 0; i < AOUT_SESSION_MAX; i++, sess++) {
		if ((uint32_t)s == (uint32_t)sess) {
			memset(s, 0, sizeof(aout_session_t));
			break;
		}
	}
}

static int audio_out_config(struct device *dev, aout_session_t *session);

/**************************************************************
**	Description:	wait audio out dma transfer over
**	Input:      device structure / handle
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int audio_out_wait_finish(struct device *dev, aout_channel_node_t *channel_node, uint32_t timeout_us)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_dac *dac_reg = cfg->dac;
	struct acts_audio_i2stx0 *i2stx_reg = cfg->i2stx;
	uint32_t delta, time_start = k_cycle_get_32();
	uint8_t fifo_cnt;

	while (1) {
		if (channel_node->channel_fifo_sel == AOUT_FIFO_DAC0)
			fifo_cnt = (dac_reg->stat & DAC_STAT_DAF0S_MASK) >> DAC_STAT_DAF0S_SHIFT;
		else if (channel_node->channel_fifo_sel == AOUT_FIFO_DAC1)
			fifo_cnt = (dac_reg->stat & DAC_STAT_DAF1S_MASK) >> DAC_STAT_DAF1S_SHIFT;
		else if (channel_node->channel_fifo_sel == AOUT_FIFO_I2STX0)
			fifo_cnt = (i2stx_reg->tx0_fifostat & I2STX0_STAT_FIFOS_MASK) >> I2STX0_STAT_FIFOS_SHIFT;
		else
			return -ENOENT;

		/* FIFO empty */
		if (fifo_cnt >= (AUDIO_DACFIFO0_LEVEL / 2 - 1)) {
			SYS_LOG_DBG("wait fifo empty time: %dns",
				SYS_CLOCK_HW_CYCLES_TO_NS(k_cycle_get_32() - time_start));
			break;
		}

		delta = SYS_CLOCK_HW_CYCLES_TO_NS(k_cycle_get_32() - time_start) / 1000;
		if (delta > timeout_us) {
			SYS_LOG_ERR("wait fifo empty timeout %d", fifo_cnt);
			return -ETIMEDOUT;
		}
	}

	return 0;
}

extern uint32_t cal_asrc_rate(uint32_t sr_input, uint32_t sr_output, int32_t aps_level);

/**************************************************************
**	Description:	asrc out set rate
**	Input:		device type / asrc out param
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int audio_out_asrc_set_rate(struct device *dev, aout_channel_node_t *channel_node, uint32_t sr_input, uint32_t sr_output)
{
    u32_t asrc_offset;

	if((channel_node == NULL) || (channel_node->channel_asrc_index >= ASRC_OUT_MAX)) {
		SYS_LOG_ERR("%d, aout set asrc rate fail!\n", __LINE__);
		return -1;
	}

	asrc_offset = cal_asrc_rate(sr_input, sr_output, ASRC_LEVEL_DEFAULT);

	return PHY_set_asrc_out_rate(dev, channel_node, 0, asrc_offset);
}

int audio_out_asrc_get_changed_sample_cnt(struct device *dev, aout_channel_node_t *channel_node, uint32_t duration)
{
	if((channel_node == NULL) || (channel_node->channel_asrc_index >= ASRC_OUT_MAX)) {
		SYS_LOG_ERR("%d, aout set asrc rate fail!\n", __LINE__);
		return -1;
	}

	return PHY_asrc_get_changed_samples_cnt(dev, channel_node, duration);
}

int audio_out_asrc_reset_changed_sample_cnt(struct device *dev, aout_channel_node_t *channel_node)
{
	if((channel_node == NULL) || (channel_node->channel_asrc_index >= ASRC_OUT_MAX)) {
		SYS_LOG_ERR("%d, aout set asrc rate fail!\n", __LINE__);
		return -1;
	}

	return PHY_asrc_reset_changed_samples_cnt(dev, channel_node);
}

static void _audio_dma_reload(struct device *dma_dev, aout_channel_node_t *channel_node, u32_t data_addr, u32_t data_length)
{
	if(channel_node->data_mode == AUDIO_MODE_MONO) {
		data_length <<= 1;
		dma_reload(dma_dev, channel_node->aout_dma, (u32_t)data_addr, (u32_t)data_addr, data_length);
	}else{
        dma_reload(dma_dev, channel_node->aout_dma, (u32_t)data_addr, 0, data_length);
	}
}

extern void acts_dma_stop(void *dma_reg);

static void _audio_reset_dma_config(struct device *dev, aout_session_t *session)
{
    u32_t data_length;
	struct device *aout_dev = device_get_binding(CONFIG_AUDIO_OUT_ACTS_DEV_NAME);
	aout_channel_node_t *channel_node = &session->channel;


	if ((!aout_dev) || (!session)) {
		SYS_LOG_ERR("Invalid parameters %d", __LINE__);
		return ;
	}

	struct acts_audio_out_data *data = aout_dev->driver_data;

    channel_node->dma_buf_size = channel_node->dma_first_buf_size;

    if (!channel_node->phy_dma){
        audio_out_stop(aout_dev, session);

        dma_free(data->dma_dev, channel_node->aout_dma);

        audio_out_config(aout_dev, session);

        _audio_dma_reload(data->dma_dev, channel_node, (u32_t)channel_node->dma_buf_ptr, channel_node->dma_buf_size);

        audio_out_start(aout_dev, session);
    } else {
        acts_dma_stop((void *)channel_node->phy_dma);

		acts_dma_disable_sam_mode(channel_node->phy_dma);

        data_length = channel_node->dma_buf_size;

        if (channel_node->data_mode == AUDIO_MODE_MONO) {
            data_length <<= 1;
            dma_reload(data->dma_dev, channel_node->phy_dma, (u32_t)channel_node->dma_buf_ptr, (u32_t)channel_node->dma_buf_ptr, data_length);
        } else {
            dma_reload(data->dma_dev, channel_node->phy_dma, (u32_t)channel_node->dma_buf_ptr, 0, data_length);
        }

        acts_dma_start((void *)channel_node->phy_dma);
    }
}

static void _audio_out_write_data_reload_dma_cb(struct device *dev, u32_t priv_data, int reson)
{
	uint32_t ret_reason;
	aout_session_t *session = (aout_session_t *)priv_data;
	aout_channel_node_t *channel_node = &session->channel;

	if ((!session) || (!channel_node) || (!channel_node->callback)) {
		SYS_LOG_ERR("Invalid parameters %d", __LINE__);
		return ;
	}

    if (channel_node->dma_buf_size == 0) {
        if (reson == DMA_IRQ_TC){
			ret_reason = AOUT_DMA_IRQ_TC;
            _audio_reset_dma_config(dev, session);
            channel_node->callback(channel_node->callback_data, ret_reason);
        }
    } else {
        if (reson == DMA_IRQ_HF) {
			ret_reason = AOUT_DMA_IRQ_HF;
        } else {
			ret_reason = AOUT_DMA_IRQ_TC;
        }
        channel_node->callback(channel_node->callback_data, ret_reason);
    }
}

static void _audio_out_write_data_direct_dma_cb(struct device *dev, u32_t priv_data, int reson)
{
	aout_session_t *session = (aout_session_t *)priv_data;
	aout_channel_node_t *channel_node = &session->channel;

	if ((!session) || (!channel_node) || (!channel_node->callback)) {
		SYS_LOG_ERR("Invalid parameters %d", __LINE__);
		return ;
	}

	if (reson == DMA_IRQ_HF)
		return;

    channel_node->dma_run_flag = false;

    if (channel_node->dma_buf_size == 0) {
        _audio_reset_dma_config(dev, session);
	}

	channel_node->callback(channel_node->callback_data, AOUT_DMA_IRQ_TC);
}

int audio_out_dma_config(struct device *dev, aout_channel_node_t *channel_node, dma_setting_t *dma_cfg)
{
	struct acts_audio_out_data *data = dev->driver_data;
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_i2stx0 *i2stx_reg = cfg->i2stx;
	struct acts_audio_dac *dac_reg = cfg->dac;

	struct dma_config config_data;
	struct dma_block_config head_block;

	if((channel_node == NULL) || (dma_cfg == NULL)) {
		SYS_LOG_INF("%d, audio out start error!\n", __LINE__);
		return -EINVAL;
	}

	if (!channel_node->aout_dma) {
		SYS_LOG_INF("%d, audio out start not get dma!\n", __LINE__);
		return -EINVAL;
	}

	memset(&config_data, 0, sizeof(config_data));
	config_data.channel_direction = MEMORY_TO_PERIPHERAL;
	config_data.source_data_size = dma_cfg->dma_width;
	config_data.dma_callback = dma_cfg->stream_handle;
	config_data.callback_data = dma_cfg->stream_pdata;
	if (channel_node->channel_fifo_src == AOUT_FIFO_SRC_ASRC) {
		if(channel_node->channel_asrc_index == ASRC_OUT_0) {
			config_data.dma_slot = DMA_ID_AUDIO_ASRC_FIFO0;
		} else if(channel_node->channel_asrc_index == ASRC_OUT_1) {
			config_data.dma_slot = DMA_ID_AUDIO_ASRC_FIFO1;
		} else {
			SYS_LOG_ERR("%d, out channel param error!\n", __LINE__);
			return -EINVAL;
		}
	} else if(channel_node->channel_fifo_src == AOUT_FIFO_SRC_DMA) {
		if(channel_node->channel_fifo_sel == AOUT_FIFO_DAC0) {
			config_data.dma_slot = DMA_ID_AUDIO_DAC_FIFO0;
		} else if(channel_node->channel_fifo_sel == AOUT_FIFO_DAC1) {
			config_data.dma_slot = DMA_ID_AUDIO_DAC_FIFO1;
		} else if(channel_node->channel_fifo_sel == AOUT_FIFO_I2STX0) {
			config_data.dma_slot = DMA_ID_AUDIO_I2S;
		} else {
			SYS_LOG_ERR("%d, out channel param error!\n", __LINE__);
			return -EINVAL;
		}
	} else {
		SYS_LOG_ERR("%d, out channel param error!\n", __LINE__);
		return -EINVAL;
	}

	if(config_data.dma_callback != NULL) {
		config_data.complete_callback_en = dma_cfg->all_complete_callback_en;
		config_data.half_complete_callback_en = dma_cfg->half_complete_callback_en;
	} else {
		config_data.complete_callback_en = 0;
		config_data.half_complete_callback_en = 0;
	}

    config_data.reserved = dma_cfg->dma_interleaved;

	//SYS_LOG_INF("out dma slot: %d interleaved: %d\n", config_data.dma_slot, config_data.reserved);

	head_block.source_address = (unsigned int)dma_cfg->dma_src_buf;
	head_block.dest_address = (unsigned int)dma_cfg->dma_dst_buf;
	head_block.block_size = dma_cfg->dma_trans_len;
	config_data.source_burst_length = dma_cfg->dma_burst_len;
	config_data.dest_burst_length = dma_cfg->dma_burst_len;
	config_data.dma_sam = dma_cfg->dma_sam;

	if(dma_cfg->dma_interleaved) {
		head_block.block_size = head_block.block_size * 2;
	}

	head_block.source_reload_en = dma_cfg->dma_reload;
	head_block.dest_reload_en = 0;

	config_data.head_block = &head_block;

	dma_config(data->dma_dev, channel_node->aout_dma, &config_data);

	if(channel_node->channel_fifo_sel == AOUT_FIFO_I2STX0) {
		// use I2S TX0 FIFO
		i2stx_reg->tx0_fifoctl &= (~I2STX0_FIFOCTL_FIFO_DMAWIDTH_MASK);
		if(dma_cfg->dma_width == 4) {
			i2stx_reg->tx0_fifoctl |= I2STX0_FIFOCTL_DMAWIDTH_32;
		} else if(dma_cfg->dma_width == 2) {
			i2stx_reg->tx0_fifoctl |= I2STX0_FIFOCTL_DMAWIDTH_16;
		} else {
			SYS_LOG_ERR("%d, aout dma width config error!\n", __LINE__);
			return -EINVAL;
		}
	} else {
		// use DAC FIFO
		dac_reg->fifoctl &= (~DAC_FIFOCTL_FIFO_DMAWIDTH_MASK);
		if(dma_cfg->dma_width == 4) {
			dac_reg->fifoctl |= DAC_FIFOCTL_DMAWIDTH_32;
		} else if(dma_cfg->dma_width == 2) {
			dac_reg->fifoctl |= DAC_FIFOCTL_DMAWIDTH_16;
		} else {
			SYS_LOG_ERR("%d, aout dma width config error!\n", __LINE__);
			return -EINVAL;
		}
	}

	if (channel_node->dma_reload) {
		_audio_dma_reload(data->dma_dev, channel_node,
			(u32_t)channel_node->dma_buf_ptr, channel_node->dma_buf_size);
	}

	return 0;
}

static int audio_out_config(struct device *dev, aout_session_t *session)
{
	dma_setting_t rbuf_dma_cfg = {0};
	aout_channel_node_t *channel_node = &session->channel;

	struct acts_audio_out_data *data = dev->driver_data;

	if ((data->dma_dev == NULL) || (session == NULL)){
		SYS_LOG_ERR("Invalid parameters");
		return -EIO;
	}

	channel_node->dma_run_flag = false;

	channel_node->aout_dma = dma_request(data->dma_dev, 0xff);
	if (!channel_node->aout_dma) {
		SYS_LOG_ERR("DMA request error");
		return -ENXIO;
	}

#ifdef RELOAD_SAM_OPT
    if (!channel_node->dma_buf_size \
        && channel_node->channel_fifo_src == AOUT_FIFO_SRC_ASRC \
        && channel_node->dma_reload){
        rbuf_dma_cfg.dma_sam = true;
    }
#endif

	//start dma ,  reload mode,  transfer all ringbuf data
	rbuf_dma_cfg.dma_src_buf = 0;
	rbuf_dma_cfg.dma_dst_buf = 0;
	rbuf_dma_cfg.dma_trans_len = 0;
	rbuf_dma_cfg.dma_width = channel_node->dma_width;
    rbuf_dma_cfg.dma_burst_len = 8;

    if(channel_node->data_mode == AUDIO_MODE_MONO){
        rbuf_dma_cfg.dma_interleaved = true;
    }else{
        rbuf_dma_cfg.dma_interleaved = false;
    }

	if(channel_node->dma_reload){
        rbuf_dma_cfg.stream_handle = _audio_out_write_data_reload_dma_cb;
        rbuf_dma_cfg.dma_reload = 1;
	}else{
        rbuf_dma_cfg.stream_handle = _audio_out_write_data_direct_dma_cb;
        rbuf_dma_cfg.dma_reload = 0;
	}

	rbuf_dma_cfg.stream_pdata = (void *)session;
	rbuf_dma_cfg.all_complete_callback_en = 1;
	rbuf_dma_cfg.half_complete_callback_en = 1;

	return audio_out_dma_config(dev, channel_node, &rbuf_dma_cfg);
}

/* @brief prepare the DMA configuration for the audio out transfer */
static int audio_out_dma_prepare(struct device *dev, aout_session_t *session)
{
	int ret;

	/* check already configured */
	if (session->flags & AOUT_SESSION_CONFIG)
		return 0;

	ret = audio_out_config(dev, session);
	if (ret) {
        SYS_LOG_ERR("audio out dma config error:%d", ret);
		return ret;
	}

	session->flags |= AOUT_SESSION_CONFIG;

	return 0;
}

int PHY_set_asrc_out_start_threashold(struct device *dev, aout_channel_node_t *channel_node, uint32_t start_threashold);

/**************************************************************
**	Description:	audio out write data
**	Input:      device structure / channel node /
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int audio_out_set_asrc_vol_gain(struct device *dev, aout_channel_node_t *channel_node, uint32_t gain)
{
	return PHY_set_asrc_out_vol_gain(channel_node->channel_asrc_index ,gain);
}

int audio_reset_asrc_fifo(struct device *dev, aout_channel_node_t *channel_node, u8_t reset_w, u8_t reset_r)
{
    return phy_reset_asrc_fifo(channel_node->channel_asrc_index, reset_w, reset_r);
}

int audio_out_get_asrc_vol_gain(struct device *dev, aout_channel_node_t *channel_node)
{
	return PHY_get_asrc_out_vol_gain(channel_node->channel_asrc_index);
}

int audio_out_get_asrc_remain_samples(struct device *dev, aout_channel_node_t *channel_node)
{
	return PHY_get_asrc_out_remain_samples_by_type(channel_node->channel_asrc_index);
}

int audio_out_write_zero_to_asrc_fifo_by_cpu(struct device *dev, aout_channel_node_t *channel_node, uint32_t sample)
{
	return PHY_write_zero_to_asrc_fifo_by_cpu(channel_node->channel_asrc_index ,sample);
}

int audio_out_request_asrc_irq(struct device *dev, aout_channel_node_t *channel_node, void (*callback)(void), u32_t threashold)
{
	return PHY_request_asrc_irq(callback, threashold);
}

/* @brief Enable the DAC channel */
static int audio_out_enable_dac(struct device *dev, aout_session_t *session, aout_param_t *param)
{
	aout_channel_node_t *channel_node = &session->channel;
	uint8_t channel_type = param->channel_type;
	dac_setting_t *dac_setting = param->dac_setting;
	PHY_audio_out_param_t phy_param = {0};
	phy_dac_setting_t phy_dac_setting = {0};
	phy_i2stx_setting_t phy_i2stx_setting = {0};
	phy_spdiftx_setting_t phy_spdiftx_setting = {0};
	int ret;

	if (!dac_setting) {
		SYS_LOG_ERR("DAC setting is NULL");
		return -EINVAL;
	}

	channel_node->channel_type = AOUT_CHANNEL_DAC;
	phy_param.ptr_dac_setting = &phy_dac_setting;
	phy_dac_setting.dac_sample_rate = param->sample_rate;
#ifdef CONFIG_AUDIO_VOLUME_PA
#if (CONFIG_MIC0_HW_MAPPING != 4)
	phy_dac_setting.dac_gain = ((dac_setting->volume.left_volume & 0xFF) << 8) | 0xBF;
#else
	//new dmic, config bigger da gain
	phy_dac_setting.dac_gain = ((dac_setting->volume.left_volume & 0xFF) << 8) | 0xFF;
#endif
	//SYS_LOG_INF("dac gain: 0x%x\n", phy_dac_setting.dac_gain);
#else
	uint8_t da_level;
	da_level = dac_volume_db_to_level(dac_setting->volume.left_volume);
	phy_dac_setting.dac_gain = AOUT_DAC_PA_VOLUME_DEFAULT | da_level;
#endif

	phy_param.aout_fifo_sel = channel_node->channel_fifo_sel;
	phy_param.aout_fifo_src_type = channel_node->channel_fifo_src;
	phy_param.channel_apll_sel = APLL_ALLOC_AUTO;

	/* Check the DAC FIFO usage */
	if ((param->outfifo_type != AOUT_FIFO_DAC0)
		&& (param->outfifo_type != AOUT_FIFO_DAC1)) {
		SYS_LOG_ERR("Channel fifo type invalid %d", param->outfifo_type);
		return -EINVAL;
	}

	if ((param->channel_width != CHANNEL_WIDTH_16BITS)
		&& (param->channel_width != CHANNEL_WIDTH_24BITS)) {
		SYS_LOG_ERR("Invalid channel width %d", param->channel_width);
		return -EINVAL;
	}

	if (channel_type & AUDIO_CHANNEL_I2STX) {
		phy_param.ptr_i2stx_setting = &phy_i2stx_setting;
		phy_i2stx_setting.i2stx_sample_rate = param->sample_rate;
#ifdef CONFIG_I2STX_MODE
		phy_i2stx_setting.i2stx_role = CONFIG_I2STX_MODE;
#else
		phy_i2stx_setting.i2stx_role = I2S_DEVICE_MASTER;
#endif

#ifdef CONFIG_I2STX_FORMAT
		phy_i2stx_setting.i2stx_format = CONFIG_I2STX_FORMAT;
#else
		phy_i2stx_setting.i2stx_format = I2S_FORMAT_I2S;
#endif
	}

	if (channel_type & AUDIO_CHANNEL_SPDIFTX) {
		phy_param.ptr_spdiftx_setting = &phy_spdiftx_setting;
		phy_spdiftx_setting.spdiftx_sample_rate = param->sample_rate;
	}

	if ((channel_type & AUDIO_CHANNEL_I2STX)
		&& (channel_type & AUDIO_CHANNEL_SPDIFTX)) {
		/* linkage with I2STX and SPDIFTX */
		channel_node->channel_type = AOUT_CHANNEL_LINKAGE_DAC_I2S_SPDIF;

		ret = PHY_audio_enable_i2stx(dev, &phy_param, &session->channel);
		if (ret < 0) {
			SYS_LOG_ERR("enable DAC link with SPDIFTX error:%d", ret);
			return ret;
		}

		ret = PHY_audio_enable_spdiftx(dev, &phy_param, &session->channel);
		if (ret < 0) {
			SYS_LOG_ERR("enable DAC link with SPDIFTX error:%d", ret);
			return ret;
		}
	} else if (channel_type & AUDIO_CHANNEL_I2STX) {
		/* Linkage with I2STX */
		channel_node->channel_type = AOUT_CHANNEL_LINKAGE_DAC_I2S;

		ret = PHY_audio_enable_i2stx(dev, &phy_param, &session->channel);
		if (ret < 0) {
			SYS_LOG_ERR("enable I2STX link with SPDIFTX error:%d", ret);
			return ret;
		}
	} else if (channel_type & AUDIO_CHANNEL_SPDIFTX) {
		/* Linkage with SPDIFTX */
		channel_node->channel_type = AOUT_CHANNEL_LINKAGE_DAC_SPDIF;

		ret = PHY_audio_enable_spdiftx(dev, &phy_param, &session->channel);
		if (ret < 0) {
			SYS_LOG_ERR("enable DAC link with SPDIFTX error:%d", ret);
			return ret;
		}
	}

	if (AOUT_CHANNEL_DAC != channel_node->channel_type) {
		enable_channel_linkage(dev, channel_node);
	}

	return PHY_audio_enable_dac(dev, &phy_param, &session->channel);
}

/* @brief Enable the I2STX channel */
static int audio_out_enable_i2stx(struct device *dev, aout_session_t *session, aout_param_t *param)
{
	aout_channel_node_t *channel_node = &session->channel;
	uint8_t channel_type = param->channel_type;
	i2stx_setting_t *i2stx_setting = param->i2stx_setting;
	PHY_audio_out_param_t phy_param = {0};
	phy_i2stx_setting_t phy_i2stx_setting = {0};
	phy_spdiftx_setting_t phy_spdiftx_setting = {0};
	int ret;

	channel_node->channel_type = AOUT_CHANNEL_I2S;
	phy_param.ptr_i2stx_setting = &phy_i2stx_setting;
	phy_i2stx_setting.i2stx_sample_rate = param->sample_rate;
#ifdef CONFIG_I2STX_MODE
	phy_i2stx_setting.i2stx_role = CONFIG_I2STX_MODE;
#else
	phy_i2stx_setting.i2stx_role = I2S_DEVICE_MASTER;
#endif

#ifdef CONFIG_I2STX_FORMAT
	phy_i2stx_setting.i2stx_format = CONFIG_I2STX_FORMAT;
#else
	phy_i2stx_setting.i2stx_format = I2S_FORMAT_I2S;
#endif

	if (i2stx_setting && i2stx_setting->srd_callback) {
		phy_i2stx_setting.srd_callback = i2stx_setting->srd_callback;
		phy_i2stx_setting.cb_data = i2stx_setting->cb_data;
	}

	phy_param.aout_fifo_sel = channel_node->channel_fifo_sel;
	phy_param.aout_fifo_src_type = channel_node->channel_fifo_src;
	phy_param.channel_apll_sel = APLL_ALLOC_AUTO;

	/* Check the DAC FIFO usage */
	if ((param->outfifo_type != AOUT_FIFO_DAC0)
		&& (param->outfifo_type != AOUT_FIFO_DAC1)
		&& (param->outfifo_type != AOUT_FIFO_I2STX0)) {
		SYS_LOG_ERR("Channel fifo type invalid %d", param->outfifo_type);
		return -EINVAL;
	}

	if ((param->channel_width != CHANNEL_WIDTH_16BITS)
		&& (param->channel_width != CHANNEL_WIDTH_24BITS)
		&& (param->channel_width != CHANNEL_WIDTH_20BITS)) {
		SYS_LOG_ERR("Invalid channel width %d", param->channel_width);
		return -EINVAL;
	}

	if (channel_type & AUDIO_CHANNEL_SPDIFTX) {
		phy_param.ptr_spdiftx_setting = &phy_spdiftx_setting;
		phy_spdiftx_setting.spdiftx_sample_rate = param->sample_rate;
	}

	/* Linkage with SPDIFTX */
	if (channel_type & AUDIO_CHANNEL_SPDIFTX) {
		channel_node->channel_type = AOUT_CHANNEL_LINKAGE_I2S_SPDIF;
		ret = PHY_audio_enable_spdiftx(dev, &phy_param, &session->channel);
		if (ret < 0) {
			SYS_LOG_ERR("enable I2STX link with SPDIFTX error:%d", ret);
			return ret;
		}
		enable_channel_linkage(dev, channel_node);
	}

	return PHY_audio_enable_i2stx(dev, &phy_param, &session->channel);
}

/* @brief Enable the SPDIFTX channel */
static int audio_out_enable_spdiftx(struct device *dev, aout_session_t *session, aout_param_t *param)
{
	aout_channel_node_t *channel_node = &session->channel;
	spdiftx_setting_t *spdiftx_setting = param->spdiftx_setting;
	PHY_audio_out_param_t phy_param = {0};
	phy_spdiftx_setting_t phy_spdiftx_setting = {0};

	phy_param.ptr_spdiftx_setting = &phy_spdiftx_setting;
	phy_spdiftx_setting.spdiftx_sample_rate = param->sample_rate;

	channel_node->channel_type = AOUT_CHANNEL_SPDIF;

	phy_param.aout_fifo_sel = channel_node->channel_fifo_sel;
	phy_param.aout_fifo_src_type = channel_node->channel_fifo_src;
	phy_param.channel_apll_sel = APLL_ALLOC_AUTO;

	if ((param->channel_width != CHANNEL_WIDTH_16BITS)
		&& (param->channel_width != CHANNEL_WIDTH_24BITS)) {
		SYS_LOG_ERR("Invalid channel width %d", param->channel_width);
		return -EINVAL;
	}

	if (!spdiftx_setting) {
		SYS_LOG_ERR("SPDIFTX setting is NULL");
		return -EINVAL;
	}

	/* Check the DAC FIFO usage */
	if ((param->outfifo_type != AOUT_FIFO_DAC0)
		&& (param->outfifo_type != AOUT_FIFO_DAC1)
		&& (param->outfifo_type != AOUT_FIFO_I2STX0)) {
		SYS_LOG_ERR("Channel fifo type invalid %d", param->outfifo_type);
		return -EINVAL;
	}

	return PHY_audio_enable_spdiftx(dev, &phy_param, &session->channel);
}

/* @brief Enable the audio analog to analog channel */
static int audio_out_enable_aa(struct device *dev, aout_session_t *session, aout_param_t *param)
{
	aout_channel_node_t *channel_node = &session->channel;
	PHY_audio_out_param_t phy_param = {0};
	phy_pa_setting_t phy_pa_setting = {0};

	channel_node->channel_type = AOUT_CHANNEL_AA;

#ifndef CONFIG_AUDIO_OUT_ACTS_L_MUTE
	phy_pa_setting.left_mute_flag = 0;
#else
	phy_pa_setting.left_mute_flag = 1;
#endif

#ifndef CONFIG_AUDIO_OUT_ACTS_R_MUTE
	phy_pa_setting.right_mute_flag = 0;
#else
	phy_pa_setting.right_mute_flag = 1;
#endif

	phy_pa_setting.aa_mode_src = CONFIG_AUDIO_OUT_AA_SOURCE_SEL;

	phy_param.ptr_pa_setting = &phy_pa_setting;

	return PHY_audio_enable_aa_out(dev, &phy_param);
}

#ifdef CONFIG_AMP_AW882XX
static struct k_delayed_work amp_work;
static void amp_start_work(struct k_work *work)
{
	struct device *dev;
	ARG_UNUSED(work);

	dev = device_get_binding(CONFIG_AMP_DEV_NAME);
	if(NULL != dev) {
		SYS_LOG_INF("amp start at %dms.", k_uptime_get_32());
		amp_start(dev);
	}
}

#elif defined(CONFIG_CODEC_ACM8635)
#define ACM8635_AMP_DEFER_OPEN_MS	15
#define ACM8635_AMP_DEFER_DMA_MS	30

static struct k_delayed_work amp_work;

static void acm8635_schedule_amp_start(struct acts_audio_out_data *data,
					 const char *reason)
{
	u32_t defer_ms = ACM8635_AMP_DEFER_OPEN_MS;

	if (data->amp_dev == NULL) {
		return;
	}

	if (reason && !strcmp(reason, "dma start")) {
		defer_ms = ACM8635_AMP_DEFER_DMA_MS;
	}

	k_delayed_work_cancel(&amp_work);
	k_delayed_work_submit(&amp_work, defer_ms);
	SYS_LOG_INF("Schedule ACM8635 amp (%s) in %u ms", reason, defer_ms);
}

static void amp_start_work(struct k_work *work)
{
	struct device *dev;
	ARG_UNUSED(work);

	dev = device_get_binding(CONFIG_AMP_DEV_NAME);
	if (dev != NULL) {
		SYS_LOG_INF("ACM8635 amp start at %u ms", k_uptime_get_32());
		amp_start(dev);
	}
}
#endif

/**************************************************************
**	Description: open an audio output channel by specified parameters.
**	Input: device structure / channel parameters
**	Output:
**	Return: audio output channel handler.
**	Note:
***************************************************************
**/
static void *acts_audio_out_open(struct device *dev, aout_param_t *param)
{
	aout_session_t *session = NULL;
	aout_channel_node_t *channel;
	uint16_t channel_type;
	int ret;
	struct acts_audio_out_data *data = dev->driver_data;

	if (NULL == data) {
		SYS_LOG_ERR("data is NULL!");
		return NULL;
	}

	//SYS_LOG_INF("pa is %d!", data->pa_open_flag);

	if (!param) {
		SYS_LOG_ERR("NULL parameter");
		return NULL;
	}

	drv_audio_out_lock();

	channel_type = param->channel_type;
	session = audio_out_session_get(channel_type);
	if (!session) {
		SYS_LOG_ERR("Failed to get audio session (type:%d)", channel_type);
		drv_audio_out_unlock();
		return NULL;
	}

	if (!param->callback) {
		SYS_LOG_ERR("Channel callback is NULL");
		goto err;
	}

#ifdef CONFIG_AUDIO_POWERON_OPEN_PA
	if(!data->pa_open_flag) {
		SYS_LOG_INF("pa is closed!");
		goto err;
	}
#endif
	session->channel_type = channel_type;
	channel = &session->channel;
	channel->channel_fifo_sel = param->outfifo_type;
	channel->channel_sample_rate = param->sample_rate;
	channel->callback = param->callback;
	channel->callback_data = param->cb_data;
	channel->channel_fifo_src = AOUT_FIFO_SRC_DMA;
	channel->data_mode = AUDIO_MODE_STEREO;
	channel->dma_width = (CHANNEL_WIDTH_16BITS == param->channel_width) ? 2 : 4;

	if (param->reload_setting) {
		if ((!param->reload_setting->reload_addr)
			|| (!param->reload_setting->reload_len)) {
			SYS_LOG_ERR("Invalid reload parameter addr:0x%x, len:0x%x",
				(uint32_t)param->reload_setting->reload_addr,
				param->reload_setting->reload_len);
			goto err;
		}
		channel->dma_reload = 1;
		channel->dma_buf_ptr = param->reload_setting->reload_addr;
		channel->dma_first_buf_size = param->reload_setting->reload_len;
		channel->dma_buf_size = channel->dma_first_buf_size;
#if 0
		SYS_LOG_INF("In reload mode [0x%08x %d]",
			(uint32_t)channel->dma_buf_ptr, channel->dma_first_buf_size);
#endif
	} else {
		channel->dma_reload = 0;
	}

	if (channel_type & AUDIO_CHANNEL_DAC) {
		if (param->dac_setting->channel_mode == MONO_MODE)
			channel->data_mode = AUDIO_MODE_MONO;
		ret = audio_out_enable_dac(dev, session, param);
	} else if (channel_type & AUDIO_CHANNEL_I2STX) {
		ret = audio_out_enable_i2stx(dev, session, param);
	} else if (channel_type & AUDIO_CHANNEL_SPDIFTX) {
		ret = audio_out_enable_spdiftx(dev, session, param);
	}  else if (channel_type & AUDIO_CHANNEL_AA) {
		ret = audio_out_enable_aa(dev, session, param);
	} else {
		SYS_LOG_ERR("Invalid channel type %d", channel_type);
		goto err;
	}

	/* TODO: it is a workaround method for mono mode setting as the audio out API does not satisfy this setting */
	if (param->dac_setting &&
		(param->dac_setting->channel_mode == MONO_MODE)) {
		channel->data_mode = AUDIO_MODE_MONO;
	}

	if (ret) {
		SYS_LOG_ERR("Enable channel type %d error:%d", channel_type, ret);
		goto err;
	}

	SYS_LOG_INF("channel:%d sess:%p type:%d fifo:%d sr:%d opened",
				session->id, session, channel_type,
				param->outfifo_type, param->sample_rate);

	if (audio_out_dma_prepare(dev, session)) {
		SYS_LOG_ERR("prepare session dma error");
		return NULL;
	}

#if 0
#ifdef CONFIG_AMP_AW882XX
	struct acts_audio_out_data *data = dev->driver_data;
	if (data->amp_dev != NULL) {
		amp_open(data->amp_dev);
	}
#endif
#endif

	drv_audio_out_unlock();

#if 0 /* ACM8635: amp_start only on dma start (I2S+PCM ready), not i2stx open */
#if defined(CONFIG_CODEC_ACM8635)
	if (channel_type & AUDIO_CHANNEL_I2STX) {
		acm8635_schedule_amp_start(data, "i2stx open");
	}
#endif
#endif

	return (void *)session;

err:
	audio_out_session_put(session);
	drv_audio_out_unlock();
	return NULL;
}

/* @brief Disable the DAC channel */
static int audio_out_disable_dac(struct device *dev, aout_session_t *session)
{
	int ret;

	if (!session)
		return -EINVAL;

	ret = PHY_audio_disable_dac(dev, &session->channel);

	if ((session->channel_type & AUDIO_CHANNEL_I2STX)
		|| ((session->channel_type & AUDIO_CHANNEL_SPDIFTX))) {
		disable_channel_linkage(dev, &session->channel);
	}

	if (session->channel_type & AUDIO_CHANNEL_I2STX) {
		ret |= PHY_audio_disable_i2stx(dev, &session->channel);
	}

	if (session->channel_type & AUDIO_CHANNEL_SPDIFTX) {
		ret |= PHY_audio_disable_spdiftx(dev, &session->channel);
	}

	return ret;
}

/* @brief Disable the I2STX channel */
static int audio_out_disable_i2stx(struct device *dev, aout_session_t *session)
{
	int ret;

	if (!session)
		return -EINVAL;

	ret = PHY_audio_disable_i2stx(dev, &session->channel);

	if (session->channel_type & AUDIO_CHANNEL_SPDIFTX) {
		disable_channel_linkage(dev, &session->channel);
		ret |= PHY_audio_disable_spdiftx(dev, &session->channel);
	}

	return ret;
}

/* @brief Disable the SPDIFTX channel */
static int audio_out_disable_spdiftx(struct device *dev, aout_session_t *session)
{
	int ret;

	if (!session)
		return -EINVAL;

	ret = PHY_audio_disable_spdiftx(dev, &session->channel);

	return ret;
}

/* @brief Disable the AA channel */
static int audio_out_disable_aa(struct device *dev, aout_session_t *session)
{
	int ret;

	if (!session)
		return -EINVAL;

	ret = PHY_audio_disable_aa(dev, &session->channel);

	return ret;
}

/* @brief Select external PA class */
static int acts_audio_pa_class_select(struct device *dev, u8_t pa_class)
{
	ARG_UNUSED(dev);
#ifdef CONFIG_BOARD_EXTERNAL_PA_ENABLE
	return board_extern_pa_class_select(pa_class);
#else
	return -1;
#endif
}

/* @brief ASRC command and control */
int audio_out_asrc_ioctl(struct device *dev, void *handle, int cmd, void *param)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_i2stx0 *i2stx_reg = cfg->i2stx;
	struct acts_audio_dac *dac_reg = cfg->dac;
	aout_session_t *session = (aout_session_t *)handle;
	aout_channel_node_t *channel_node = &session->channel;
	struct acts_audio_out_data *data = dev->driver_data;
	int ret = 0;

	switch (cmd) {
		case AOUT_CMD_ASRC_ENABLE:
		{
			phy_asrc_param_t asrc_param = {0};
			PHY_audio_out_param_t phy_param = {0};
			phy_param.ptr_asrc_setting = &asrc_param;
			asrc_out_setting_t *setting = (asrc_out_setting_t *)param;
			if (!setting) {
				SYS_LOG_ERR("invalid asrc setting");
				return -EINVAL;
			}

			SYS_LOG_INF("Enable ASRC mode:%d mem policy:%d wclk:%d in_sr:%d",
						setting->mode, setting->mem_policy,
						setting->wclk, setting->input_samplerate);

			if ((channel_node->channel_fifo_sel != AOUT_FIFO_DAC0)
				&& (channel_node->channel_fifo_sel != AOUT_FIFO_DAC1)
				&& (channel_node->channel_fifo_sel != AOUT_FIFO_I2STX0)) {
				SYS_LOG_ERR("Invalid fifo type:%d", channel_node->channel_fifo_sel);
				return -EINVAL;
			}

			if ((setting->wclk != ASRC_OUT_WCLK_CPU)
				&& (setting->wclk != ASRC_OUT_WCLK_DMA)
				&& (setting->wclk != ASRC_OUT_WCLK_DSP)) {
				SYS_LOG_ERR("Invalid ASRC write clock:%d", setting->wclk);
				return -EINVAL;
			}

			channel_node->input_sample_rate = setting->input_samplerate;
			phy_param.asrc_alloc_method = setting->mem_policy;
			phy_param.aout_fifo_sel = channel_node->channel_fifo_sel;
			phy_param.aout_fifo_src_type = channel_node->channel_fifo_src;
			asrc_param.by_pass = (setting->mode == BYPASS_MODE) ? 1 : 0;

			if (!asrc_param.by_pass)
				asrc_param.ctl_mode = setting->mode;

			asrc_param.input_samplerate = channel_node->input_sample_rate;
			asrc_param.ctl_dma_width = channel_node->dma_width * 8;
			asrc_param.ctl_wr_clk = setting->wclk;

			ret = prepare_asrc_out_param(dev, &phy_param, channel_node);
			if (ret) {
				SYS_LOG_ERR("prepare asrc out error:%d", ret);
				return ret;
			}

			ret = audio_out_asrc_config(dev, &asrc_param);
			if (!ret) {
				channel_node->channel_fifo_src = AOUT_FIFO_SRC_ASRC;
				if (channel_node->channel_fifo_sel == AOUT_FIFO_DAC0) {
					dac_reg->fifoctl &= ~DAC_FIFOCTL_DAF0IS_MASK;
					dac_reg->fifoctl |= DAC_FIFOCTL_DAF0IS_ASRC0;
				} else if (channel_node->channel_fifo_sel == AOUT_FIFO_DAC1) {
					dac_reg->fifoctl &= ~DAC_FIFOCTL_DAF1IS_MASK;
					dac_reg->fifoctl |= DAC_FIFOCTL_DAF1IS_ASRC1;
				} else {
			        i2stx_reg->tx0_fifoctl &= ~I2STX0_FIFOCTL_ASRC_SEL_MASK;
			        if (channel_node->channel_asrc_index == ASRC_OUT_0){
			            i2stx_reg->tx0_fifoctl |= I2STX0_FIFOCTL_ASRC_SEL(0);
			        } else {
			            i2stx_reg->tx0_fifoctl |= I2STX0_FIFOCTL_ASRC_SEL(1);
			        }
					i2stx_reg->tx0_fifoctl &= ~I2STX0_FIFOCTL_FIFO_IN_MASK;
					i2stx_reg->tx0_fifoctl |= I2STX0_FIFOCTL_FIFO_IN_ASRC;
				}

				audio_reset_asrc_fifo(dev, channel_node, 1, 1);

			    if (!channel_node->phy_dma){
			        audio_out_stop(dev, session);

			        dma_free(data->dma_dev, channel_node->aout_dma);

			        audio_out_config(dev, session);
			    } else {
			        acts_dma_stop((void *)channel_node->phy_dma);

			        acts_dma_disable_sam_mode(channel_node->phy_dma);

			        audio_out_config(dev, session);
			    }
				session->asrc_enable = 1;
			}
			break;
		}
		case AOUT_CMD_SET_ASRC_OUT_SAMPLE_RATE:
		{
			asrc_sample_rate_t *sr_setting = (asrc_sample_rate_t *)param;
			if (!sr_setting) {
				SYS_LOG_ERR("Invalid asrc sample rate setting");
				return -EINVAL;
			}

			ret = audio_out_asrc_set_rate(dev, channel_node,
					sr_setting->input_sample_rate, sr_setting->output_sample_rate);

			break;
		}
		case AOUT_CMD_GET_ASRC_SAMPLE_CNT:
		{
			int duration;

			if (NULL == param) {
				SYS_LOG_ERR("Invalid duration");
				return -EINVAL;
			}

			duration = *(int *)param;
			ret = audio_out_asrc_get_changed_sample_cnt(dev, channel_node, duration);
			if (ret < 0) {
				SYS_LOG_ERR("get asrc sample cnt error:%d", ret);
				return ret;
			}

			*(uint32_t *)param = ret;
			ret = 0;
			break;
		}
		case AOUT_CMD_RESET_ASRC_SAMPLE_CNT:
		{
			ret = audio_out_asrc_reset_changed_sample_cnt(dev, channel_node);
			break;
		}
		case AOUT_CMD_RESET_ASRC_INFO:
		{
			uint8_t flag = *(uint8_t *)param;
			uint8_t reset_w = 0, reset_r = 0;

			if (flag & AUDIO_ASRC_RESET_WRITE_FIFO)
				reset_w = 1;
			if (flag & AUDIO_ASRC_RESET_READ_FIFO)
				reset_r = 1;

			ret = audio_reset_asrc_fifo(dev, channel_node, reset_w, reset_r);
			break;
		}
		case AOUT_CMD_SET_ASRC_VOLUME:
		{
			uint32_t vol;
			if (!param) {
				SYS_LOG_ERR("Invalid volume");
				return -EINVAL;
			}
			vol = *(uint32_t *)param;

			ret = audio_out_set_asrc_vol_gain(dev, channel_node, vol);
			break;
		}
		case AOUT_CMD_GET_ASRC_VOLUME:
		{
			if (!param) {
				SYS_LOG_ERR("Invalid parameter");
				return -EINVAL;
			}

			ret = audio_out_get_asrc_vol_gain(dev, channel_node);
			if (ret < 0) {
				SYS_LOG_ERR("get asrc volume error:%d", ret);
				return ret;
			}

			*(uint32_t *)param = ret;
			ret = 0;
			break;
		}
		case AOUT_CMD_GET_ASRC_REMAIN_SAMPLES:
		{
			if (!param) {
				SYS_LOG_ERR("Invalid parameter");
				return -EINVAL;
			}

			ret = audio_out_get_asrc_remain_samples(dev, channel_node);
			if (ret < 0) {
				SYS_LOG_ERR("get asrc remain samples error:%d", ret);
				return ret;
			}

			*(uint32_t *)param = ret;
			ret = 0;

			break;
		}
		case AOUT_CMD_WRITE_ASRC_ZERO_DATA_BY_CPU:
		{
			uint32_t sample_cnt = *(uint32_t *)param;
			if (!param) {
				SYS_LOG_ERR("Invalid parameter");
				return -EINVAL;
			}

			ret = audio_out_write_zero_to_asrc_fifo_by_cpu(dev, channel_node, sample_cnt);

			break;
		}
		case AOUT_CMD_REQ_ASRC_IRQ:
		{
			asrc_out_irq_t *irq_setting = (asrc_out_irq_t *)param;
			if ((!irq_setting) || (!irq_setting->callback)) {
				SYS_LOG_ERR("Invalid parameter");
				return -EINVAL;
			}

			ret = audio_out_request_asrc_irq(dev, channel_node, irq_setting->callback,
					irq_setting->threshold);
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
**	Description: close the audio output channel by given channel handler.
**	Input: device structure / channel handler
**	Output:
**	Return: success / fail
**	Note:
***************************************************************
**/
static int acts_audio_out_close(struct device *dev, void *handle)
{
	struct acts_audio_out_data *data = dev->driver_data;
	uint16_t channel_type;
	aout_session_t *session = (aout_session_t *)handle;
	aout_channel_node_t *channel_node = &session->channel;
	int ret;

	if (!handle) {
		SYS_LOG_ERR("Invalid handle");
		return -EINVAL;
	}

	drv_audio_out_lock();

	if (!AOUT_SESSION_CHECK_MAGIC(session->magic)) {
		SYS_LOG_ERR("Session magic error:0x%x", session->magic);
		ret = -EFAULT;
		goto out;
	}

	channel_type = session->channel_type;
	if (channel_type & AUDIO_CHANNEL_DAC) {
		ret = audio_out_disable_dac(dev, session);
	} else if (channel_type & AUDIO_CHANNEL_I2STX) {
		ret = audio_out_disable_i2stx(dev, session);
	} else if (channel_type & AUDIO_CHANNEL_SPDIFTX) {
		ret = audio_out_disable_spdiftx(dev, session);
	} else if (channel_type & AUDIO_CHANNEL_AA) {
		ret = audio_out_disable_aa(dev, session);
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
	if (data->dma_dev && session->channel.aout_dma) {
		dma_stop(data->dma_dev, session->channel.aout_dma);
		dma_free(data->dma_dev, session->channel.aout_dma);
	}

	if (session->asrc_enable
		&& (channel_node->channel_asrc_index < ASRC_OUT_MAX)
		&& (channel_node->channel_asrc_index != ASRC_OUT_NONE)) {
		audio_out_asrc_close(dev, channel_node->channel_asrc_index);
	}

	SYS_LOG_INF("session#%d %p closed", session->id, session);

	audio_out_session_put(session);

#if 0
#ifdef CONFIG_AMP_AW882XX
	if (data->amp_dev != NULL) {
		amp_close(data->amp_dev);
	}
#endif
#endif

out:
	drv_audio_out_unlock();
	return ret;
}

/**************************************************************
**	Description: send control and comand to the audio output channel.
**	Input: device structure / channel handler / command / command parematers.
**	Output:
**	Return: success/fail
**	Note:
***************************************************************
**/
static int acts_audio_out_control(struct device *dev, void *handle, int cmd, void *param)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_i2stx0 *i2stx_reg = cfg->i2stx;
	struct acts_audio_dac *dac_reg = cfg->dac;
	aout_session_t *session = (aout_session_t *)handle;
	aout_channel_node_t *channel_node = &session->channel;
	uint16_t channel_type = session->channel_type;
	struct acts_audio_out_data *data = dev->driver_data;
	int ret = 0;

	if (AOUT_CMD_OPEN_PA == cmd) {
		drv_audio_out_lock();
		ret = audio_out_open_pa(dev, NULL, (int)param);
		drv_audio_out_unlock();
		goto out;
	}

	if (AOUT_CMD_CLOSE_PA == cmd) {
		drv_audio_out_lock();
		ret = audio_out_close_pa(dev, (int)param);
		drv_audio_out_unlock();
		goto out;
	}

	if (AOUT_CMD_PA_CLASS_SEL == cmd)
		return acts_audio_pa_class_select(dev, *(u8_t *)param);

	if (!AOUT_SESSION_CHECK_MAGIC(session->magic)) {
		SYS_LOG_ERR("Session magic error:0x%x", session->magic);
		return -EFAULT;
	}

	if (AOUT_CMD_SET_PREPARE_START == cmd) {
		session->dma_prepare_start = 1;
		//SYS_LOG_INF("Enable prepare dma start");
		goto out;
	}

	if (AOUT_CMD_DSP_ENABLE == cmd) {
		if (session->asrc_enable) {
			SYS_LOG_ERR("FIFO source already set to ASRC");
			return -EPERM;
		}
		channel_node->channel_fifo_src = AOUT_FIFO_SRC_DSP;
		if ((channel_node->channel_fifo_sel == AOUT_FIFO_DAC0)
			|| (channel_node->channel_fifo_sel == AOUT_FIFO_DAC1)) {
			dac_reg->fifoctl &= ~DAC_FIFOCTL_DAF0IS_MASK;
			dac_reg->fifoctl |= DAC_FIFOCTL_DAF0IS_DSP;
		} else {
			i2stx_reg->tx0_fifoctl &= ~I2STX0_FIFOCTL_FIFO_IN_MASK;
			i2stx_reg->tx0_fifoctl |= I2STX0_FIFOCTL_FIFO_IN_ASRC;
		}

		if (!channel_node->phy_dma){
			audio_out_stop(dev, session);

			dma_free(data->dma_dev, channel_node->aout_dma);

			audio_out_config(dev, session);
		} else {
			acts_dma_stop((void *)channel_node->phy_dma);

			acts_dma_disable_sam_mode(channel_node->phy_dma);

			audio_out_config(dev, session);
		}
		session->dsp_enable = 1;
		goto out;
	}

	if (AOUT_IS_ASRC_CMD(cmd)) {
		/* In case of the commands belong to ASRC */
		ret = audio_out_asrc_ioctl(dev, handle, cmd, param);
	} else if (AOUT_IS_FIFO_CMD(cmd)) {
		/* In the case of the commands according to the FIFO attribute */
		if ((AOUT_FIFO_DAC0 == channel_node->channel_fifo_sel)
					|| (AOUT_FIFO_DAC1 == channel_node->channel_fifo_sel)) {
			ret = PHY_audio_ioctl_dac(dev, channel_node, cmd, param);
		} else if (AOUT_FIFO_I2STX0 == channel_node->channel_fifo_sel) {
			ret = PHY_audio_ioctl_i2stx(dev, channel_node, cmd, param);
		} else {
			ret = -ENXIO;
		}
	} else {
		if (channel_type & AUDIO_CHANNEL_DAC) {
			ret = PHY_audio_ioctl_dac(dev, channel_node, cmd, param);
		} else if (channel_type & AUDIO_CHANNEL_I2STX) {
			ret = PHY_audio_ioctl_i2stx(dev, channel_node, cmd, param);
		} else if (channel_type & AUDIO_CHANNEL_SPDIFTX) {
			ret = PHY_audio_ioctl_spdiftx(dev, channel_node, cmd, param);
		} else {
			ret = -ENXIO;
		}
	}

out:
	return ret;
}

/**************************************************************
**	Description: start the audio output channel by given channel handler.
**	Input: device structure / channel handler
**	Output:
**	Return: success/fail
**	Note:
***************************************************************
**/
static int acts_audio_out_start(struct device *dev, void *handle)
{
    struct acts_audio_out_data *data = dev->driver_data;
	aout_session_t *session = (aout_session_t *)handle;
	aout_channel_node_t *channel_node = &session->channel;
	int ret = 0;

	if (!handle) {
		SYS_LOG_ERR("Invalid handle");
		return -EINVAL;
	}

	if (!AOUT_SESSION_CHECK_MAGIC(session->magic)) {
		SYS_LOG_ERR("Session magic error:0x%x", session->magic);
		return -EFAULT;
	}

	if (audio_out_dma_prepare(dev, session)) {
		SYS_LOG_ERR("prepare session dma error");
		return -ENXIO;
	}

	/* In DMA reload mode only start one time is enough */
	if (session->channel.dma_reload
		&& (session->flags & AOUT_SESSION_START)) {
		return 0;
	}

    if (channel_node->aout_dma) {
        channel_node->dma_run_flag = true;

		if (session->dma_prepare_start) {
	        channel_node->phy_dma = dma_prepare_start(data->dma_dev, channel_node->aout_dma);
	        ret = channel_node->phy_dma;
		} else {
			dma_start(data->dma_dev, channel_node->aout_dma);
		}
    } else {
        SYS_LOG_ERR("aout dma null");
        return -1;
    }

#ifdef CONFIG_AMP_AW882XX
	//struct acts_audio_out_data *data = dev->driver_data;
	if (data->amp_dev != NULL) {
		SYS_LOG_INF("Submit amp start work at %dms.", k_uptime_get_32());
		/* amp_start can not be called in ISR. Use a delayed work
		to suppress the noise when start music playing. */
		k_delayed_work_submit(&amp_work, 500);
	}
#elif defined(CONFIG_CODEC_ACM8635)
	if ((session->channel_type & AUDIO_CHANNEL_I2STX) &&
	    !(session->flags & AOUT_SESSION_START)) {
		acm8635_schedule_amp_start(data, "dma start");
	}
#endif

    if (!(session->flags & AOUT_SESSION_START))
	session->flags |= AOUT_SESSION_START;

    return ret;
}

/**************************************************************
**	Description: write the output data to the specified channel handler.
**	Input: device structure / channel handler / output buffer / buffer length
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
static int acts_audio_out_write(struct device *dev, void *handle, uint8_t *buffer, uint32_t length)
{
	struct acts_audio_out_data *data = dev->driver_data;
	aout_session_t *session = (aout_session_t *)handle;
	aout_channel_node_t *channel_node = &session->channel;
	int ret;

	if (!handle || !buffer || !length) {
		SYS_LOG_ERR("Invalid parameters (%p, %p, %d)", handle, buffer, length);
		return -EINVAL;
	}

	if (!AOUT_SESSION_CHECK_MAGIC(session->magic)) {
		SYS_LOG_ERR("Session magic error:0x%x", session->magic);
		return -EFAULT;
	}

	if (session->channel.dma_reload) {
		SYS_LOG_INF("Reload mode can start directly");
		return 0;
	}

	if (audio_out_dma_prepare(dev, session)) {
		SYS_LOG_ERR("prepare session dma error");
		return -ENXIO;
	}

    channel_node->dma_buf_ptr = buffer;
    channel_node->dma_first_buf_size = length;

	SYS_LOG_DBG("DMA channel:0x%x, buffer:%p len:%d", session->dma_chan, buffer, length);

	if (channel_node->aout_dma) {
		if (!channel_node->dma_buf_size \
		   && channel_node->channel_fifo_src == AOUT_FIFO_SRC_ASRC \
		   && channel_node->dma_reload) {
#ifdef RELOAD_SAM_OPT
			_audio_dma_reload(data->dma_dev, channel_node, (u32_t)&audio_dummy_data, length);
#else
			channel_node->dma_buf_size = channel_node->dma_first_buf_size;
			_audio_dma_reload(data->dma_dev, channel_node, (u32_t)buffer, length);
			PHY_set_asrc_out_start_threashold(dev, channel_node, length >> 4);
#endif
		} else {
			channel_node->dma_buf_size = channel_node->dma_first_buf_size;
			_audio_dma_reload(data->dma_dev, channel_node, (u32_t)buffer, length);
#ifndef RELOAD_SAM_OPT
			PHY_set_asrc_out_start_threashold(dev, channel_node, length >> 2);
#endif
		}
	}

	ret = audio_out_start(dev, handle);
	if (ret)
		SYS_LOG_ERR("dma start error %d", ret);

	return ret;
}

/**************************************************************
**	Description: stop the specified audio channel transfer.
**	Input: device structure / channel handler
**	Output:
**	Return: success/fail
**	Note:
***************************************************************
**/
static int acts_audio_out_stop(struct device *dev, void *handle)
{
	struct acts_audio_out_data *data = dev->driver_data;
	aout_session_t *session = (aout_session_t *)handle;
	int ret = 0;

#ifdef CONFIG_AMP_AW882XX
	//struct acts_audio_out_data *data = dev->driver_data;
	if (data->amp_dev != NULL) {
		k_delayed_work_cancel(&amp_work);
		amp_stop(data->amp_dev);
	}
#elif defined(CONFIG_CODEC_ACM8635)
	if (data->amp_dev != NULL) {
		amp_stop(data->amp_dev);
	}
#endif

	if (session && AOUT_SESSION_CHECK_MAGIC(session->magic)) {
		SYS_LOG_INF("session#%p, audio out stop", session);
		if (data->dma_dev && session->channel.aout_dma) {
			ret = dma_stop(data->dma_dev, session->channel.aout_dma);
			if ((!ret) && (!session->asrc_enable))
				audio_out_wait_finish(dev, &session->channel,
					AOUT_DMA_WAIT_TIMEOUT_US);
			session->channel.dma_run_flag = false;
			session->flags &= ~AOUT_SESSION_START;
		}
	}

	return ret;
}

static const struct aout_driver_api aout_drv_api = {
	.aout_open = acts_audio_out_open,
	.aout_close = acts_audio_out_close,
	.aout_control = acts_audio_out_control,
	.aout_start = acts_audio_out_start,
	.aout_write = acts_audio_out_write,
	.aout_stop = acts_audio_out_stop
};

/**************************************************************
**	Description:	audio out device driver init
**	Input:      device structure
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int audio_out_init(struct device *dev)
{
	struct acts_audio_out_data *data = dev->driver_data;

	SYS_LOG_INF("%d\n", __LINE__);

	memset(data, 0, sizeof(struct acts_audio_out_data));

	data->dma_dev = device_get_binding(CONFIG_DMA_0_NAME);

	// only prompt,  maybe the channel not using dma
	if(NULL == data->dma_dev) {
		SYS_LOG_INF("%d, aout dma device not found!\n", __LINE__);
	}

	data->cur_asrc_out0_ramindex = 0xff;
	data->cur_asrc_out1_ramindex = 0xff;

#ifdef CONFIG_AUDIO_ASRC_ENABLE
	asrc_buf_init();
#endif

	// audio global reset
	acts_reset_peripheral(RESET_ID_AUDIO);

	// reset i2s module
	acts_reset_peripheral(RESET_ID_I2S);

#ifdef CONFIG_AUDIO_POWERON_OPEN_PA
	audio_out_open_pa(dev, NULL, true);
#endif

#ifdef CONFIG_AMP_AW882XX
	data->amp_dev = device_get_binding(CONFIG_AMP_DEV_NAME);
	if (!data->amp_dev) {
		SYS_LOG_ERR("cannot found amp dev: %s", CONFIG_AMP_DEV_NAME);
		return -1;
	}
#elif defined(CONFIG_CODEC_ACM8635)
	data->amp_dev = device_get_binding(CONFIG_AMP_DEV_NAME);
	if (!data->amp_dev) {
		SYS_LOG_ERR("cannot found ACM8635 amp dev: %s",
			    CONFIG_AMP_DEV_NAME);
		return -1;
	}
#endif

#if defined(CONFIG_AMP_AW882XX) || defined(CONFIG_CODEC_ACM8635)
	if (data->amp_dev != NULL) {
		amp_open(data->amp_dev);
		k_delayed_work_init(&amp_work, amp_start_work);
	}
#endif

	return 0;
}

static struct acts_audio_out_data audio_out_acts_data;

static const struct acts_audio_out_config audio_out_acts_config = {
	.asrc = (struct acts_audio_asrc *)ASRC_REG_BASE,

	.dac = (struct acts_audio_dac *) AUDIO_DAC_REG_BASE,
	.i2stx = (struct acts_audio_i2stx0 *)AUDIO_I2STX0_REG_BASE,
	.spdiftx = (struct acts_audio_spdiftx *)AUDIO_SPDIFTX_REG_BASE,
};

DEVICE_AND_API_INIT(audio_out, CONFIG_AUDIO_OUT_ACTS_DEV_NAME, audio_out_init,
	    &audio_out_acts_data, &audio_out_acts_config,
	    POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &aout_drv_api);


/**************************************************************
**	Description:	enable dac irq
**	Input:
**	Output:
**	Return:
**	Note:
***************************************************************
**/
void audio_out_dac_irq_enable(void)
{
	if (irq_is_enabled(IRQ_ID_AUDIO_DAC) == 0) {
		IRQ_CONNECT(IRQ_ID_AUDIO_DAC, CONFIG_IRQ_PRIO_HIGH, audio_out_dac_isr,
			DEVICE_GET(audio_out), 0);
		irq_enable(IRQ_ID_AUDIO_DAC);
	}
}

void audio_out_i2stx_irq_enable(void)
{
	if (irq_is_enabled(IRQ_ID_AUDIO_I2S0_TX) == 0) {
		IRQ_CONNECT(IRQ_ID_AUDIO_I2S0_TX, CONFIG_IRQ_PRIO_NORMAL,
			phy_i2stx_isr, DEVICE_GET(audio_out), 0);
		irq_enable(IRQ_ID_AUDIO_I2S0_TX);
	}
}

