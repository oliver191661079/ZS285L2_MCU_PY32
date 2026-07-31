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
 * Description:    audio spdiftx physical layer
 *
 * Change log:
 *	2019/03/21: Created by mike.
 ****************************************************************************
 */

#include "../audio_inner.h"
#include <audio_out.h>

static const struct acts_pin_config board_pin_spdiftx_config[] = {
#ifdef BOARD_SPDIFTX_MAP
	BOARD_SPDIFTX_MAP
#endif
};

/**************************************************************
**	Description:   spdif tx module init
**	Input:      device
**	Output:
**	Return:    success /fail
**	Note:
***************************************************************
**/
int phy_spdif_tx_init(struct device *dev)
{
	// spdif tx gpio init
	acts_pinmux_setup_pins(board_pin_spdiftx_config,ARRAY_SIZE(board_pin_spdiftx_config));

	// spdif tx normal
	acts_reset_peripheral_deassert(RESET_ID_SPDIFTX);

	acts_clock_peripheral_enable(CLOCK_ID_SPDIF_TX);
	acts_clock_peripheral_enable(CLOCK_ID_AUDIO_DAC);

	return 0;
}


/**************************************************************
**	Description:	spdif tx clock config
**	Input:      device / channel node / fs / clk_src
**	Output:
**	Return:    success /fail
**	Note:
***************************************************************
**/
int phy_spdiftx_clk_config(struct device *dev, aout_channel_node_t *ptr_channel, uint8_t fs_khz, aout_device_clk_e clk_src)
{
	uint32_t freq;
	uint8_t get_series;
	uint32_t sample_hz;
	uint8_t use_audiopll_index = 0xff;

	uint32_t prediv=0;
	uint32_t clkdiv=0;
	sample_type_e fs_series=SAMPLE_TYPE_48;

	// change fs_khz to fs_hz
	sample_hz = get_sample_rate_hz(fs_khz);

	freq=sample_hz*256;

	ptr_channel->channel_clk_type = CLOCK_TYPE_128FS;

	switch(freq){
		// 192KHz
		case PLL_49152:
			clkdiv=0;
			prediv=0;
			break;

		// 96KHz
		case PLL_24576:
			clkdiv=0;
			prediv=1;
			break;

		// 64KHz
		case PLL_16384:
			clkdiv=2;
			prediv=0;
			break;

		// 48KHz
		case PLL_12288:
			clkdiv=1;
			prediv=1;
			break;

		// 32KHz
		case PLL_8192:
			clkdiv=2;
			prediv=1;
			break;

		// 24KHz
		case PLL_6144:
			clkdiv=3;
			prediv=1;
			break;

		// 16KHz
		case PLL_4096:
			clkdiv=4;
			prediv=1;
			break;

		// 12KHz
		case PLL_3072:
			clkdiv=5;
			prediv=1;
			break;

		// 8KHz
		case PLL_2048:
			clkdiv=6;
			prediv=1;
			break;

		// 176.4KHz
		case PLL_451584:
			clkdiv=0;
			prediv=0;
			fs_series=SAMPLE_TYPE_44;
			break;

		// 88.2KHz
		case PLL_22579:
			clkdiv=0;
			prediv=1;
			fs_series=SAMPLE_TYPE_44;
			break;

		// 44.1KHz
		case PLL_11289:
			clkdiv=1;
			prediv=1;
			fs_series=SAMPLE_TYPE_44;
			break;

		// 22.05KHz
		case PLL_5644:
			clkdiv=3;
			prediv=1;
			fs_series=SAMPLE_TYPE_44;
			break;

		// 11.025KHz
		case PLL_2822:
			clkdiv=5;
			prediv=1;
			fs_series=SAMPLE_TYPE_44;
			break;

		default:
			SYS_LOG_ERR("%d, spdif tx sample rate not supported: %d\n", __LINE__, fs_khz);
			return -1;
	}

	// get audiopll 0 state
	PHY_check_audiopll(AUDIOPLL_TYPE_0, &get_series);
	if((get_series == 0xff) || ((sample_type_e)get_series == fs_series)) {
		//this time use audiopll 0
		use_audiopll_index = 0;
	}

	if(use_audiopll_index == 0xff) {
		// get audiopll 1 state
		PHY_check_audiopll(AUDIOPLL_TYPE_1, &get_series);
		if((get_series == 0xff) || ((sample_type_e)get_series == fs_series)) {
			//this time use audiopll 1
			use_audiopll_index = 1;
		}
	}

	if(use_audiopll_index == 0xff) {
		SYS_LOG_ERR("%d, select audio pll fail!\n", __LINE__);
		return -1;
	}

	PHY_config_audiopll(use_audiopll_index, (uint8_t)fs_series);
	ptr_channel->channel_apll_sel = (audiopll_type_e)use_audiopll_index;
	ptr_channel->channel_sample_rate = fs_khz;
	ptr_channel->channel_sample_type = fs_series;

	sys_write32((sys_read32(CMU_SPDIFCLK) & (~CMU_SPDIFCLK_SPDIFTXCLKSRC_MASK)), CMU_SPDIFCLK);

	if(clk_src == AOUT_SDF_CLK_DAC128FS) {
		// spdif tx clock 使用dac_clk_128fs
		sys_write32(sys_read32(CMU_ADDACLK) & (~0xff), CMU_ADDACLK);
		sys_write32(sys_read32(CMU_ADDACLK) | (use_audiopll_index << CMU_ADDACLK_DACCLKSRC) | (prediv << CMU_ADDACLK_DACCLKPREDIV) | \
			(clkdiv << CMU_ADDACLK_DACCLKDIV_SHIFT), CMU_ADDACLK);
	} else if(clk_src == AOUT_SDF_CLK_I2S0CLK) {
		// spdif tx clock 使用i2s0 clock
		sys_write32((sys_read32(CMU_I2SCLK) & (~0xff)), CMU_I2SCLK);
		sys_write32((sys_read32(CMU_I2SCLK) | (use_audiopll_index << CMU_I2SCLK_I2S0CLKSRC) | (prediv << CMU_I2SCLK_I2S0CLKPREDIV) | \
			(clkdiv << CMU_I2SCLK_I2S0CLKDIV_SHIFT)), CMU_I2SCLK);

		sys_write32((sys_read32(CMU_SPDIFCLK) | (0x01 << CMU_SPDIFCLK_SPDIFTXCLKSRC_SHIFT)), CMU_SPDIFCLK);
	} else if(clk_src == AOUT_SDF_CLK_I2S0CLK_DIV2) {
		// spdif tx clock 使用i2s0 clock / 2
		sys_write32((sys_read32(CMU_I2SCLK) & (~0xff)), CMU_I2SCLK);
		sys_write32((sys_read32(CMU_I2SCLK) | (use_audiopll_index << CMU_I2SCLK_I2S0CLKSRC) | (prediv << CMU_I2SCLK_I2S0CLKPREDIV) | \
			(clkdiv << CMU_I2SCLK_I2S0CLKDIV_SHIFT)), CMU_I2SCLK);

		sys_write32((sys_read32(CMU_SPDIFCLK) | (0x02 << CMU_SPDIFCLK_SPDIFTXCLKSRC_SHIFT)), CMU_SPDIFCLK);
	} else {
		// spdif tx clock select error
		SYS_LOG_ERR("%d, spdif tx clock select error!\n", __LINE__);
		return -1;
	}

	PHY_audio_inc_apll_refcnt(use_audiopll_index, fs_series);

	return 0;
}


/**************************************************************
**	Description:	spdif tx fifo register config
**	Input:      device / channel node
**	Output:
**	Return:    success /fail
**	Note:
***************************************************************
**/
int phy_spdiftx_fifo_config(struct device *dev, aout_channel_node_t *ptr_channel)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_i2stx0 *i2stx_reg = cfg->i2stx;
	struct acts_audio_dac *dac_reg = cfg->dac;

	uint8_t drq_en=0,irq_en=0;
	uint8_t fifo_sel;

	if(ptr_channel->channel_fifo_src == AOUT_FIFO_SRC_CPU) {
		irq_en = 1;
	} else if(ptr_channel->channel_fifo_src == AOUT_FIFO_SRC_DMA) {
		drq_en = 1;
	} else {
		;	//empty
	}

	if(ptr_channel->channel_fifo_sel > AOUT_FIFO_I2STX0) {
		return -1;
	} else if(ptr_channel->channel_fifo_sel == AOUT_FIFO_I2STX0) {
		fifo_sel = 1;	// i2stx0 fifo
	} else {
		fifo_sel = 0;	// dac fifo
	}

	// fifo select
	i2stx_reg->tx0_fifoctl &= (~I2STX0_FIFOCTL_FIFO_SEL_MASK);
	i2stx_reg->tx0_fifoctl |= (fifo_sel << I2STX0_FIFOCTL_FIFO_SEL_SHIFT);
	if(ptr_channel->channel_fifo_sel == AOUT_FIFO_DAC0) {
		dac_reg->digctl |= (DAC_DIGCTL_DAF0M2DAEN);
		dac_reg->fifoctl &= (~DAC_FIFOCTL_DAF0IS_MASK);
		dac_reg->fifoctl |= (ptr_channel->channel_fifo_src << DAC_FIFOCTL_DAF0IS_SHIFT);
		dac_reg->fifoctl |= (DAC_FIFOCTL_DAF0RT | DAC_FIFOCTL_DAF0EDE);
	} else if(ptr_channel->channel_fifo_sel == AOUT_FIFO_DAC1) {
		dac_reg->digctl |= (DAC_DIGCTL_DAF1M2DAEN);
		dac_reg->fifoctl &= (~DAC_FIFOCTL_DAF1IS_MASK);
		dac_reg->fifoctl |= (ptr_channel->channel_fifo_src << DAC_FIFOCTL_DAF1IS_SHIFT);
		dac_reg->fifoctl |= (DAC_FIFOCTL_DAF1RT | DAC_FIFOCTL_DAF1EDE);
	} else if (ptr_channel->channel_fifo_sel == AOUT_FIFO_I2STX0) {
		// fifo source
		i2stx_reg->tx0_fifoctl &= (~I2STX0_FIFOCTL_FIFO_IN_MASK);
		i2stx_reg->tx0_fifoctl |= (ptr_channel->channel_fifo_src << I2STX0_FIFOCTL_FIFO_IN_SHIFT);

		i2stx_reg->tx0_fifoctl &= (~(I2STX0_FIFOCTL_FIFO_IEN | I2STX0_FIFOCTL_FIFO_DEN | I2STX0_FIFOCTL_FIFO_RST));
		i2stx_reg->tx0_fifoctl |= ((irq_en << I2STX0_FIFOCTL_FIFO_IEN_SHIFT) | (drq_en << I2STX0_FIFOCTL_FIFO_DEN_SHIFT) | I2STX0_FIFOCTL_FIFO_RST);
	} else {
		SYS_LOG_ERR("invalid fifo sel %d", ptr_channel->channel_fifo_sel);
		return -EINVAL;
	}

	return 0;
}

/**************************************************************
**	Description:	enable spdif tx module
**	Input:      device / audio out param / channel node
**	Output:
**	Return:    success / fail
**	Note:
***************************************************************
**/
int PHY_audio_enable_spdiftx(struct device *dev, PHY_audio_out_param_t *aout_param, aout_channel_node_t *ptr_channel)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_spdiftx *spdiftx_reg = cfg->spdiftx;
	struct acts_audio_dac *dac_reg = cfg->dac;

	phy_spdiftx_setting_t *spdiftx_param;
	aout_device_clk_e spdiftx_clk_src;
	bool is_linkmode;

	if((aout_param == NULL) || (aout_param->ptr_spdiftx_setting == NULL)) {
		SYS_LOG_ERR("%d, spdiftx enable fail!", __LINE__);
		return -EINVAL;
	}

	spdiftx_param = aout_param->ptr_spdiftx_setting;
	is_linkmode = FALSE;

	// init spdif tx
	phy_spdif_tx_init(dev);

	if ((ptr_channel->channel_type == AOUT_CHANNEL_LINKAGE_DAC_SPDIF)
			|| (ptr_channel->channel_type == AOUT_CHANNEL_LINKAGE_DAC_I2S_SPDIF)
			|| (ptr_channel->channel_type == AOUT_CHANNEL_LINKAGE_I2S_SPDIF)) {
		dac_reg->digctl |= DAC_DIGCTL_AUDIO_FS_TWO_FS;
	}

	if ((ptr_channel->channel_type == AOUT_CHANNEL_LINKAGE_DAC_SPDIF)
		|| (ptr_channel->channel_type == AOUT_CHANNEL_LINKAGE_DAC_I2S_SPDIF)) {
		spdiftx_clk_src = AOUT_SDF_CLK_DAC128FS;
	} else {
		if ((ptr_channel->channel_fifo_sel == AOUT_FIFO_DAC0)
			|| (ptr_channel->channel_fifo_sel == AOUT_FIFO_DAC1))
			spdiftx_clk_src = AOUT_SDF_CLK_DAC128FS;
		else
			spdiftx_clk_src = AOUT_SDF_CLK_I2S0CLK_DIV2;
	}

	ptr_channel->channel_dev_clk = spdiftx_clk_src;

	// config clock
	phy_spdiftx_clk_config(dev, ptr_channel, spdiftx_param->spdiftx_sample_rate, spdiftx_clk_src);

	// config fifo
	phy_spdiftx_fifo_config(dev, ptr_channel);

	u32_t csl = 0;
	u32_t csh = 0;
	switch(spdiftx_param->spdiftx_sample_rate){
		case SAMPLE_22KHZ:
			csl |= 4<<24;
			break;
		case SAMPLE_24KHZ:
			csl |= 6<<24;
			break;
		case SAMPLE_32KHZ:
			csl |= 3<<24;
			break;
		case SAMPLE_44KHZ:
			csl |= 0<<24;
			break;
		case SAMPLE_48KHZ:
			csl |= 2<<24;
			break;
		case SAMPLE_88KHZ:
			csl |= 8<<24;
			break;
		case SAMPLE_96KHZ:
			csl |= 0xa<<24;
			break;
		case SAMPLE_176KHZ:
			csl |= 0xc<<24;
			break;
		case SAMPLE_192KHZ:
			csl |= 0xe<<24;
			break;

		default:
		case SAMPLE_64KHZ:
			csl |= 8<<24;// not indicated
			break;

	}
	spdiftx_reg->spdtx_csl = csl;
	spdiftx_reg->spdtx_csh = csh;

	if(!is_linkmode) {
		spdiftx_reg->spdtx_ctl |= SPDTX_CTL_SPDEN;
	}

	return 0;
}


/**************************************************************
**	Description:	disable spdif tx module
**	Input:      device / spdiftx used fifo
**	Output:
**	Return:    success / fail
**	Note:
***************************************************************
**/
int PHY_audio_disable_spdiftx(struct device *dev, aout_channel_node_t *channel_node)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_dac *dac_reg = cfg->dac;
	struct acts_audio_i2stx0 *i2stx_reg = cfg->i2stx;
	struct acts_audio_spdiftx *spdtx_reg = cfg->spdiftx;
	uint8_t use_channel = channel_node->channel_fifo_sel;

	// reset gpio
	for (uint8_t i = 0; i < ARRAY_SIZE(board_pin_spdiftx_config); i++) {
		acts_pinmux_set(board_pin_spdiftx_config[i].pin_num, 0);
	}

	if ((channel_node->channel_type == AOUT_CHANNEL_LINKAGE_DAC_SPDIF)
			|| (channel_node->channel_type == AOUT_CHANNEL_LINKAGE_DAC_I2S_SPDIF)
			|| (channel_node->channel_type == AOUT_CHANNEL_LINKAGE_I2S_SPDIF)) {
		dac_reg->digctl &= ~DAC_DIGCTL_AUDIO_FS_MASK;
	}

	// disable dac 0 or dac 1
	if (AOUT_FIFO_DAC0 == use_channel) {
		//DAC FIFO0 reset
		dac_reg->fifoctl &= (~DAC_FIFOCTL_DAF0RT);
		//fifo0 mix disable
		dac_reg->digctl &= (~DAC_DIGCTL_DAF0M2DAEN);
	}else if(AOUT_FIFO_DAC1 == use_channel) {
		//DAC FIFO1 reset
		dac_reg->fifoctl &= (~DAC_FIFOCTL_DAF1RT);
		//fifo1 mix disable
		dac_reg->digctl &= (~DAC_DIGCTL_DAF1M2DAEN);
	}else if(AOUT_FIFO_I2STX0 == use_channel) {
		//i2s tx0 fifo reset
		i2stx_reg->tx0_fifoctl &= (~I2STX0_FIFOCTL_FIFO_RST);
	}else {
		SYS_LOG_ERR("%d, disable spdiftx error!\n", __LINE__);
		return -1;
	}

	spdtx_reg->spdtx_ctl &= (~SPDTX_CTL_SPDEN);

	//spdif tx clock gating disable
	acts_clock_peripheral_disable(CLOCK_ID_SPDIF_TX);


	PHY_audio_dec_apll_refcnt(channel_node->channel_apll_sel);

	return 0;
}

/**************************************************************
**	Description:	command and control internal spdiftx channel
**	Input:      device structure  / channel
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int PHY_audio_ioctl_spdiftx(struct device *dev, aout_channel_node_t *ptr_channel, uint32_t cmd, void *param)
{
    const struct acts_audio_out_config *cfg = dev->config->config_info;
    struct acts_audio_spdiftx *spdtx_reg = cfg->spdiftx;
	int ret = 0;

	switch (cmd) {
		case AOUT_CMD_GET_APS:
		{
			ret = PHY_audio_get_aps(dev, ptr_channel, APS_LEVEL_AUDIOPLL, false);
			if (ret < 0) {
				SYS_LOG_ERR("get audio pll aps error:%d", ret);
				return ret;
			}
			*(uint8_t *)param = ret;
			ret = 0;
			break;
		}
		case AOUT_CMD_SET_APS:
		{
			uint8_t aps = *(uint8_t *)param;
			ret = PHY_audio_set_aps(dev, ptr_channel, aps, APS_LEVEL_AUDIOPLL, false);
			break;
		}
		case AOUT_CMD_GET_ASRC_APS:
		{
			ret = PHY_audio_get_aps(dev, ptr_channel, APS_LEVEL_ASRC, false);
			if (ret < 0) {
				SYS_LOG_ERR("get audio pll aps error:%d", ret);
				return ret;
			}
			*(uint8_t *)param = ret;
			break;
		}
		case AOUT_CMD_SET_ASRC_APS:
		{
			uint8_t aps = *(uint8_t *)param;
			ret = PHY_audio_set_aps(dev, ptr_channel, aps, APS_LEVEL_ASRC, false);
			break;
		}
		case AOUT_CMD_SPDIF_SET_CHANNEL_STATUS:
		{
			audio_spdif_ch_status_t *status = (audio_spdif_ch_status_t *)param;
			if (!status) {
				SYS_LOG_ERR("Invalid parameters");
				return -EINVAL;
			}
			spdtx_reg->spdtx_csl = status->csl;
			spdtx_reg->spdtx_csh = status->csh;
			break;
		}
		case AOUT_CMD_SPDIF_GET_CHANNEL_STATUS:
		{
			audio_spdif_ch_status_t *status = (audio_spdif_ch_status_t *)param;
			status->csl = spdtx_reg->spdtx_csl;
			status->csh = spdtx_reg->spdtx_csh;
			break;
		}
		default:
		{
			SYS_LOG_ERR("Unsupport command %d", cmd);
			ret = -ENOTSUP;
			break;
		}
	}

	return ret;
}

