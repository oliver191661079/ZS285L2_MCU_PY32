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
 * Description:    audio spdifrx physical layer
 *
 * Change log:
 *	2019/03/25: Created by mike.
 ****************************************************************************
 */

#include "../audio_inner.h"
#include <soc_dvfs.h>
#include <audio_in.h>

#define SPDIF_RX_INIT_DELTA			(5)

#define SPDIF_RX_DEFAULT_SAMPLE_DELTA	(8)

#define SPDIF_RX_HIGH_SR_SAMPLE_DELTA	(3)

#define SPDIF_RX_SR_DETECT_MS			(50)

#define SPDIF_RX_SRD_TIMEOUT_THRES      (0xFFFFFF)

static const struct acts_pin_config board_pin_spdifrx_config[] = {
#ifdef BOARD_SPDIFRX_MAP
	BOARD_SPDIFRX_MAP
#endif
};

static ain_channel_node_t *g_spdifrx_channel_node = NULL;

static inline void phy_set_spdifrx_channel_node(ain_channel_node_t *channel_node)
{
	uint32_t key = irq_lock();
	g_spdifrx_channel_node = channel_node;
	irq_unlock(key);
}

static inline ain_channel_node_t *phy_get_spdifrx_channel_node(void)
{
	return g_spdifrx_channel_node;
}


/**************************************************************
**	Description:	spdif rx init
**	Input:      device
**	Output:
**	Return:    success /fail
**	Note:
***************************************************************
**/
int phy_spdifrx_init(struct device *dev, ain_channel_node_t *ptr_channel)
{
    acts_pinmux_set(board_pin_spdifrx_config[0].pin_num,0);
	// fix use corepll
	ptr_channel->channel_dev_clk = AIN_SDF_CLK_COREPLL;
	ptr_channel->channel_clk_type = CLOCK_TYPE_MAX;

	soc_dvfs_set_spdif_rate(SAMPLE_48KHZ);

	// spdif rx normal
	acts_reset_peripheral(RESET_ID_SPDIFRX);

	sys_write32((sys_read32(CMU_SPDIFCLK) & (~CMU_SPDIFCLK_SPDIFRXCLKSRC_MASK) & (~CMU_SPDIFCLK_SPDIFRXCLKDIV_MASK)), CMU_SPDIFCLK);
	sys_write32((sys_read32(CMU_SPDIFCLK) | (0x02 << CMU_SPDIFCLK_SPDIFRXCLKSRC_SHIFT)), CMU_SPDIFCLK);

	// this will also enable the HOSC clock to detect Audio Sample Rate
	acts_clock_peripheral_enable(CLOCK_ID_SPDIF_RX);

	// spdif rx normal
	acts_reset_peripheral(RESET_ID_SPDIFRX);

	return 0;
}

/**************************************************************
**	Description:	spdif rx FIFO register config
**	Input:      device / ain channle node
**	Output:
**	Return:    success /fail
**	Note:
***************************************************************
**/
int phy_spdifrx_fifo_config(struct device *dev, ain_channel_node_t *ptr_channel)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_i2srx1 *i2srx1_reg = cfg->i2srx1;

	uint8_t drq_en=0,irq_en=0;

	if(ptr_channel->channel_fifo_sel != AIN_FIFO_I2SRX1) {
		SYS_LOG_ERR("%d, spdif rx fifo err: %d!\n", __LINE__, ptr_channel->channel_fifo_sel);
		return -1;
	}

	if(ptr_channel->channel_fifo_dst == AIN_FIFO_DST_CPU) {
		irq_en = 1;
	} else if(ptr_channel->channel_fifo_dst == AIN_FIFO_DST_DMA) {
		drq_en = 1;
	} else {
		;	//empty
	}

	// i2s rx 1
	i2srx1_reg->rx1_fifoctl &= (~I2SRX1_FIFOCTL_RXFOS_MASK);
	i2srx1_reg->rx1_fifoctl |= (ptr_channel->channel_fifo_dst << I2SRX1_FIFOCTL_RXFOS_SHIFT);     /* i2s rx1 fifo output */

	i2srx1_reg->rx1_fifoctl &= (~I2SRX1_FIFOCTL_RXFIS_MASK);
	i2srx1_reg->rx1_fifoctl |= I2SRX1_FIFOCTL_RXFIS_SPDIFRX;	/* i2s rx1 fifo input:  spdif rx */

	i2srx1_reg->rx1_fifoctl &= (~(I2SRX1_FIFOCTL_RXFFIE | I2SRX1_FIFOCTL_RXFFDE | I2SRX1_FIFOCTL_RXFRT));
	i2srx1_reg->rx1_fifoctl |= ((irq_en << I2SRX1_FIFOCTL_RXFFIE_SHIFT) | (drq_en << I2SRX1_FIFOCTL_RXFFDE_SHIFT) | I2SRX1_FIFOCTL_RXFRT);     /* irq / drq / fifo reset */

	return 0;
}


/**************************************************************
**	Description:	spdif rx control register config
**	Input:      device
**	Output:
**	Return:    success /fail
**	Note:
***************************************************************
**/
int phy_spdifrx_ctl_config(struct device *dev)
{
	if(NULL == dev || NULL == dev->config) {
		return 0;
	}
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	if(NULL == cfg) {
		return 0;
	}
	struct acts_audio_spdifrx *spdifrx_reg = cfg->spdifrx;
	if(NULL == spdifrx_reg ) {
		return 0;
	}

	spdifrx_reg->spdrx_ctl0 = 0;

	// hw cal mode
	spdifrx_reg->spdrx_ctl0 |= SPDIF_RXCTL0_CAL_MODE_SET_HW;

	//config init delta
	spdifrx_reg->spdrx_ctl0 |= SPDIF_RXCTL0_DELTA_MIN_SET(SPDIF_RX_INIT_DELTA);
	spdifrx_reg->spdrx_ctl0 |= SPDIF_RXCTL0_DELTA_ADD_SET(SPDIF_RX_INIT_DELTA);

	//DAMEN
	spdifrx_reg->spdrx_ctl0 |= SPDIF_RX_DAMEN;

	//spdif rx enable
	spdifrx_reg->spdrx_ctl0 |= SPDIF_RXEN;

	//sample rate detect enable
	spdifrx_reg->spdrx_samp &= (~SPDIF_RX_SAMP_DELTA_MASK);
	spdifrx_reg->spdrx_samp |= SPDIF_RX_SAMP_DELTA_SET(SPDIF_RX_DEFAULT_SAMPLE_DELTA);
	spdifrx_reg->spdrx_samp |= SPDIF_RX_SAMP_EN;

	return 0;
}



/**************************************************************
**	Description:	detect spdif rx sample rate
**	Input:      device / channel node / wait time
**	Output:
**	Return:    success / fail
**	Note:
***************************************************************
**/
int PHY_spdifrx_SR_detect(struct device *dev, ain_channel_node_t *ptr_channel, int wait_time_ms, uint32_t sample_cnt)
{
	if(NULL == dev || NULL == dev->config) {
		return 0;
	}
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	if(NULL == cfg) {
		return 0;
	}
	struct acts_audio_spdifrx *spdifrx_reg = cfg->spdifrx;
	if(NULL == spdifrx_reg ) {
		return 0;
	}

	uint32_t time_stamp_begin, time_stamp_now;
	int result = -1;
	uint8_t index;

	const uint16_t sample_cnt_tbl[14][2] = {
		{120, 130},			// 192k,  24M/192K =  125
		{131, 141},			// 176.4k,  24M/176K = 136
		{245, 255},			// 96k,  24M/96K = 250
		{268, 276},			// 88.2k, 24M/88K = 272
		{370, 380},			// 64k, 24M/64K = 375
		{495, 505},			// 48k,  24M/48K = 500
		{540, 550},			// 44.1k, 24M/44K = 544
		{745, 755},			// 32k, 24M/32K = 750
		{995, 1005},		// 24k, 24M/24K = 1000
		{1084,1094},		// 22.05k, 24M/22.05K = 1088
		{1495, 1505},		// 16k, 24M/16K = 1500
		{1995, 2005},		// 12k, 24M/12K = 2000
		{2171, 2181},		// 11.025k, 24M/11.025K = 2176
		{2995, 3005},		// 8k, 24M/8K = 3000
	};

	const uint8_t sample_rate_tbl[14] = {
		SAMPLE_192KHZ, SAMPLE_176KHZ, SAMPLE_96KHZ, SAMPLE_88KHZ, SAMPLE_64KHZ, SAMPLE_48KHZ, SAMPLE_44KHZ,
		SAMPLE_32KHZ, SAMPLE_24KHZ, SAMPLE_22KHZ, SAMPLE_16KHZ, SAMPLE_12KHZ, SAMPLE_11KHZ, SAMPLE_8KHZ
	};

	SYS_LOG_INF("status %d, sample_cnt %d", ptr_channel->channel_sample_status, sample_cnt);

	if(ptr_channel->channel_sample_status >= RX_SR_STATUS_GET) {
		SYS_LOG_INF("skip to detect!\n");
		return result;
	}

	if(wait_time_ms != K_FOREVER) {
		time_stamp_begin = k_uptime_get_32();
	}


    do {
		if(wait_time_ms != K_FOREVER) {
			time_stamp_now = k_uptime_get_32();
			if((time_stamp_now - time_stamp_begin) > wait_time_ms) {
				SYS_LOG_INF("%d, time over!!\n", __LINE__);
				result = -1;
				break;
			}
		}

		if((spdifrx_reg->spdrx_samp & SPDIF_RX_SAMP_VALID_MASK) == 0) {
			SYS_LOG_INF("valid bit is invaild");
		}

		// sample rate valid
		for(index = 0; index < sizeof(sample_rate_tbl); index++) {
			if((sample_cnt >= sample_cnt_tbl[index][0]) && (sample_cnt <= sample_cnt_tbl[index][1])) {
				ptr_channel->channel_sample_rate = sample_rate_tbl[index];
				if((ptr_channel->channel_sample_rate % SAMPLE_11KHZ) == 0) {
					ptr_channel->channel_sample_type = SAMPLE_TYPE_44;
				} else {
					ptr_channel->channel_sample_type = SAMPLE_TYPE_48;
				}

				result = 0;
				break;
			}
		}

		if(result == 0) {
			SYS_LOG_INF("sr: %d\n", ptr_channel->channel_sample_rate);
			// int audio_in_asrc_set_rate(struct device *dev, ain_channel_node_t *channel_node, uint32_t sr_input, uint32_t sr_output);
			// audio_in_asrc_set_rate(dev,ptr_channel,ptr_channel->channel_sample_rate,SAMPLE_48KHZ);
			break;
		}
	}while(0);

	return result;
}


/**************************************************************
**	Description:  do something for new sample rate
**	Input:      device / node
**	Output:
**	Return:
**	Note:
***************************************************************
**/
void deal_for_new_samplerate(struct device *dev, ain_channel_node_t *ptr_channel)
{
	if(NULL == dev || NULL == dev->config) {
		return;
	}
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	if(NULL == cfg) {
		return;
	}
	struct acts_audio_spdifrx *spdifrx_reg = cfg->spdifrx;
	if(NULL == spdifrx_reg ) {
		return;
	}
	uint32_t rxclk, txclk, new_T, new_delta;

	if(NULL == ptr_channel) {
		return;
	}
	if(!ptr_channel->channel_sample_rate){
		return;
	}

    rxclk = soc_dvfs_set_spdif_rate(ptr_channel->channel_sample_rate);

	rxclk *= 1000;
	txclk = 128 * ptr_channel->channel_sample_rate;
	new_T = 2 * (rxclk/txclk);
	new_delta = new_T/4 + 1;

	SYS_LOG_INF("%d, rxclk: %d, txclk: %d\n", __LINE__, rxclk, txclk);
	SYS_LOG_INF("%d, soft_T: %d, hw_T: %d\n", __LINE__, new_T, spdifrx_reg->spdrx_ctl1 & SPDIF_RX_WID1TCFG_MASK);
	SYS_LOG_INF("%d, delta: %d\n", __LINE__, new_delta);

	if(new_delta > 15) {
		SYS_LOG_ERR("%d, delta is too large!\n", __LINE__);
	} else if(new_delta < 3) {
		SYS_LOG_ERR("%d, delta is too small!\n", __LINE__);
	} else {
		//config new delta
        SYS_LOG_INF("delta is %d ",new_delta);
		spdifrx_reg->spdrx_ctl0 &= (~SPDIF_RXCTL0_DELTA_MIN_MASK);
		spdifrx_reg->spdrx_ctl0 &= (~SPDIF_RXCTL0_DELTA_ADD_MASK);
		spdifrx_reg->spdrx_ctl0 |= SPDIF_RXCTL0_DELTA_MIN_SET(new_delta);
		spdifrx_reg->spdrx_ctl0 |= SPDIF_RXCTL0_DELTA_ADD_SET(new_delta);

        if(ptr_channel->channel_sample_rate > SAMPLE_96KHZ){
            spdifrx_reg->spdrx_samp &= (~SPDIF_RX_SAMP_DELTA_MASK);
            spdifrx_reg->spdrx_samp |= SPDIF_RX_SAMP_DELTA_SET(SPDIF_RX_HIGH_SR_SAMPLE_DELTA);
        }else{
            spdifrx_reg->spdrx_samp &= (~SPDIF_RX_SAMP_DELTA_MASK);
            spdifrx_reg->spdrx_samp |= SPDIF_RX_SAMP_DELTA_SET(SPDIF_RX_DEFAULT_SAMPLE_DELTA);
        }

	}
}


/**************************************************************
**	Description:	spdif rx irq handle
**	Input:      device
**	Output:
**	Return:    success / fail
**	Note:
***************************************************************
**/
void spdifrx_acts_irq(void* arg)
{
	struct device *dev = device_get_binding(CONFIG_AUDIO_IN_ACTS_DEV_NAME);
	if(NULL == dev || NULL == dev->config) {
		return;
	}
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	if(NULL == cfg) {
		return;
	}
	struct acts_audio_spdifrx *spdifrx_reg = cfg->spdifrx;
	if(NULL == spdifrx_reg ) {
		return;
	}
	ain_channel_node_t *channel_node;
	int result;
    u32_t sample_cnt;//sample_cnt_new;

	SYS_LOG_DBG("spdrx_pd 0x%x\n", spdifrx_reg->spdrx_pd);

	channel_node = phy_get_spdifrx_channel_node();
	if (NULL == channel_node) {
		SYS_LOG_ERR("%d, spdif rx node is not exist!\n", __LINE__);
		return;
	}

	// deal for spdif rx irq
	if((spdifrx_reg->spdrx_pd & SPDIF_RX_SRCPD) != 0) {
		// sample rate change
		channel_node->channel_sample_status = RX_SR_STATUS_CHANGE;

		sample_cnt = ((spdifrx_reg->spdrx_samp & SPDIF_RX_SAMP_CNT_MASK) >> SPDIF_RX_SAMP_CNT_SHIFT);
        // clear pending
		spdifrx_reg->spdrx_pd |= SPDIF_RX_SRCPD;

		result = PHY_spdifrx_SR_detect(dev, channel_node, SPDIF_RX_SR_DETECT_MS,sample_cnt);
        if(result != 0) {
			SYS_LOG_ERR("%d, get new samplerate fail!\n", __LINE__);
		} else {
			// get new sample rate,  need cal new delta
            // get spdif rx sample rate
			channel_node->channel_sample_status = RX_SR_STATUS_GET;
			deal_for_new_samplerate(dev, channel_node);
		}

        // sample_cnt_new = ((spdifrx_reg->spdrx_samp & SPDIF_RX_SAMP_CNT_MASK) >> SPDIF_RX_SAMP_CNT_SHIFT);
        // if(sample_cnt_new != sample_cnt){
        //     SYS_LOG_INF("2nd_change in irq,last is %d now is %d ",sample_cnt,sample_cnt_new);
        //     result = PHY_spdifrx_SR_detect(dev, channel_node, SPDIF_RX_SR_DETECT_MS,sample_cnt_new);

        //     if(result != 0) {
        //         SYS_LOG_ERR("%d, get new samplerate fail!\n", __LINE__);
        //     } else {
        //         // get new sample rate,  need cal new delta
        //         // get spdif rx sample rate
        //         channel_node->channel_sample_status = RX_SR_STATUS_GET;
        //         deal_for_new_samplerate(dev, channel_node);
        //     }
        // }

		// data mask state
		k_busy_wait(1000);
		spdifrx_reg->spdrx_ctl0 |= SPDIF_RX_DAMS;

		if(result == 0) {
			// get new sample rate,  callback
			if(channel_node->sr_change_callback != NULL) {
				channel_node->sr_change_callback(channel_node->spdif_callback_data,
								SPDIFRX_SRD_FS_CHANGE, (void *)&channel_node->channel_sample_rate);
			}
		}

	}

	/* sample rate detect timeout pending */
	if (spdifrx_reg->spdrx_pd & SPDIF_RX_SRTOPD) {
		spdifrx_reg->spdrx_pd |= SPDIF_RX_SRTOPD;
		if ((channel_node->sr_change_callback != NULL)
			&& (channel_node->channel_sample_status = RX_SR_STATUS_GET)) {
			channel_node->sr_change_callback(channel_node->spdif_callback_data, SPDIFRX_SRD_TIMEOUT, NULL);
		}
		channel_node->channel_sample_status = RX_SR_STATUS_NOTGET;
	}

}

/**************************************************************
**	Description:	enable spdif rx module
**	Input:      device / audio in param / channel node
**	Output:
**	Return:    success / fail
**	Note:
***************************************************************
**/
int PHY_audio_enable_spdifrx(struct device *dev, PHY_audio_in_param_t *ain_param, ain_channel_node_t *ptr_channel)
{
	if((ain_param == NULL) || (ptr_channel == NULL)) {
		return -EINVAL;
	}

	// init samplerate status
	ptr_channel->channel_sample_status = RX_SR_STATUS_NOTGET;

	if(ain_param->ptr_spdifrx_setting != NULL) {
		if(ain_param->ptr_spdifrx_setting->spdifrx_callback != NULL) {
			ptr_channel->sr_change_callback = ain_param->ptr_spdifrx_setting->spdifrx_callback;
		} else {
			ptr_channel->sr_change_callback = NULL;
		}

		if(ain_param->ptr_spdifrx_setting->callback_data != NULL) {
			ptr_channel->spdif_callback_data = ain_param->ptr_spdifrx_setting->callback_data;
		} else {
			ptr_channel->spdif_callback_data = NULL;
		}
	}

	// init spdif rx
	phy_spdifrx_init(dev, ptr_channel);

	// config spdif rx fifo
	phy_spdifrx_fifo_config(dev, ptr_channel);

	// config spdif rx control reg
	phy_spdifrx_ctl_config(dev);

	phy_set_spdifrx_channel_node(ptr_channel);

#ifdef CONFIG_AUDIO_IN_SPDIFRX_SUPPORT
	PHY_audio_spdifrx_irq_enable(dev);
#endif

	return 0;
}


/**************************************************************
**	Description:	disable spdif rx module
**	Input:      device
**	Output:
**	Return:    success / fail
**	Note:       spdif rx fix use I2S RX1 FIFO
***************************************************************
**/
int PHY_audio_disable_spdifrx(struct device *dev)
{
	if(NULL == dev || NULL == dev->config) {
		return 0;
	}
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	if(NULL == cfg) {
		return 0;
	}
	struct acts_audio_i2srx1 *i2srx1_reg = cfg->i2srx1;
	if(NULL == i2srx1_reg) {
		return 0;
	}
	struct acts_audio_spdifrx *spdifrx_reg = cfg->spdifrx;
	if(NULL == spdifrx_reg ) {
		return 0;
	}

#ifdef CONFIG_AUDIO_IN_SPDIFRX_SUPPORT
	PHY_audio_spdifrx_irq_disable(dev);
#endif

	// reset gpio
	for (uint8_t i = 0; i < ARRAY_SIZE(board_pin_spdifrx_config); i++) {
		acts_pinmux_set(board_pin_spdifrx_config[i].pin_num, 0);
	}

	//I2S RX1 FIFO reset
	i2srx1_reg->rx1_fifoctl &= (~I2SRX1_FIFOCTL_RXFRT);

	//spdif rx disable
	spdifrx_reg->spdrx_ctl0 &= (~SPDIF_RXEN);

	//spdif rx clock gating disable
	acts_clock_peripheral_disable(CLOCK_ID_SPDIF_RX);

	soc_dvfs_set_spdif_rate(0);

	phy_set_spdifrx_channel_node(NULL);

	return 0;
}

#ifdef CONFIG_AUDIO_IN_SPDIFRX_SUPPORT
/**************************************************************
**	Description:	enable spdif rx irq
**	Input:
**	Output:
**	Return:
**	Note:
***************************************************************
**/
void PHY_audio_spdifrx_irq_enable(struct device *dev)
{
	if(NULL == dev || NULL == dev->config) {
		return;
	}
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	if(NULL == cfg) {
		return;
	}
	struct acts_audio_spdifrx *spdifrx_reg = cfg->spdifrx;
	if(NULL == spdifrx_reg ) {
		return;
	}

	spdifrx_reg->spdrx_srto_thres = SPDIF_RX_SRD_TIMEOUT_THRES;

	// spdif rx irq config
	spdifrx_reg->spdrx_pd |= (SPDIF_RX_SRCIRQEN | SPDIF_RX_SRTOEN);
	spdifrx_reg->spdrx_pd |= (SPDIF_RX_BLKRCVPD | SPDIF_RX_SUBRCVPD | SPDIF_RX_BMCERPD | SPDIF_RX_SRCPD | SPDIF_RX_CSUPPD | \
		SPDIF_RX_CSSRUPPD | SPDIF_RX_SRTOPD | SPDIF_RX_BL_HEADPD);

	if(irq_is_enabled(IRQ_ID_AUDIO_I2S1_RX) == 0) {
		IRQ_CONNECT(IRQ_ID_AUDIO_I2S1_RX, CONFIG_IRQ_PRIO_HIGH, spdifrx_acts_irq,
			NULL, 0);
		irq_enable(IRQ_ID_AUDIO_I2S1_RX);

        // spdif rx gpio init
        acts_pinmux_setup_pins(board_pin_spdifrx_config,ARRAY_SIZE(board_pin_spdifrx_config));
	}
}

/**************************************************************
**	Description:	disable spdif rx irq
**	Input:
**	Output:
**	Return:
**	Note:
***************************************************************
**/
void PHY_audio_spdifrx_irq_disable(struct device *dev)
{
	if(NULL == dev || NULL == dev->config) {
		return;
	}
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	if(NULL == cfg) {
		return;
	}
	struct acts_audio_spdifrx *spdifrx_reg = cfg->spdifrx;
	if(NULL == spdifrx_reg) {
		return;
	}

	// free irq
	spdifrx_reg->spdrx_pd &= (~SPDIF_RX_SRCIRQEN);
	if(irq_is_enabled(IRQ_ID_AUDIO_I2S1_RX) != 0) {
        acts_pinmux_set(board_pin_spdifrx_config[0].pin_num,0);
		irq_disable(IRQ_ID_AUDIO_I2S1_RX);
	}
}
#endif

/**************************************************************
**	Description:	command and control internal spdifrx channel
**	Input:      device structure  / channel
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int PHY_audio_ioctl_spdifrx(struct device *dev, ain_channel_node_t *ptr_channel, uint32_t cmd, void *param)
{
	if(NULL == dev || NULL == dev->config) {
		return 0;
	}
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	if(NULL == cfg) {
		return 0;
	}
	struct acts_audio_spdifrx *spdifrx_reg = cfg->spdifrx;
	if(NULL == spdifrx_reg) {
		return 0;
	}
	int ret = 0;

	switch (cmd) {
	case AIN_CMD_SPDIF_GET_CHANNEL_STATUS:
	{
		audio_spdif_ch_status_t *sts = (audio_spdif_ch_status_t *)param;
		if (!sts) {
			SYS_LOG_ERR("Invalid parameters");
			return -EINVAL;
		}
		sts->csl = spdifrx_reg->spdrx_csl;
		sts->csh = spdifrx_reg->spdrx_csh;
		break;
	}
	case AIN_CMD_SPDIF_IS_PCM_STREAM:
	{
		if (spdifrx_reg->spdrx_csl & (1 << 1))
			*(bool *)param = false;
		else
			*(bool *)param = true;
		break;
	}
	case AIN_CMD_SPDIF_CHECK_DECODE_ERR:
	{
		if ((spdifrx_reg->spdrx_pd & SPDIF_RX_BLKRCVPD)
			|| (spdifrx_reg->spdrx_pd & SPDIF_RX_SUBRCVPD)
			|| (spdifrx_reg->spdrx_pd & SPDIF_RX_BMCERPD)) {
			*(bool *)param = true;
		} else {
			*(bool *)param = false;
		}
		break;
	}
	default:
		SYS_LOG_ERR("Unsupport command %d", cmd);
		return -ENOTSUP;

	}

	return ret;
}

