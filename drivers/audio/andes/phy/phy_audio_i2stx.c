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
 * Description:    audio i2stx physical layer
 *
 * Change log:
 *	2018/11/14: Created by mike.
 ****************************************************************************
 */

#include "../audio_inner.h"

#include <audio_out.h>

#define I2STX_MCLK_INT	(0)
#define I2STX_MCLK_EXT	(1)

#define I2STX_SRD_TH_DEFAULT			(7)
#define I2STX_SRD_CNT_TIM_DEFAULT		(0)

#define I2STX_SRD_CONFIG_TIMEOUT_MS		(500)

/*
 * struct i2stx_private_data
 * @brief Physical I2STX device private data
 */
struct i2stx_private_data {
	u32_t fifo_cnt; /* I2STX FIFO sample counter max value is 0xFFFF */
#ifdef CONFIG_I2STX_SRD
	int (*srd_callback)(void *cb_data, u32_t cmd, void *param);
	void *cb_data;
	u8_t srd_wl; /* The width length detected by SRD */
#endif
};

static struct i2stx_private_data i2stx_priv;

/**
**	i2s tx clock source
**/
//#define CONFIG_I2STX_CLK_SEL	(AOUT_I2S_CLK_I2S0CLK)    //(AOUT_I2S_CLK_DAC256FS)

/**
**	i2s tx bclk rate
**/
#define CONFIG_I2STX_BCLK_RATE	(I2S_BCLK_64FS)

//extern void i2stx0_mclk_config(uint8_t mode);
//extern void i2stx0_mfp_config(uint8_t mode);

/**************************************************************
**	Description:	i2stx0 init
**	Input:      device
**	Output:
**	Return:    success /fail
**	Note:
***************************************************************
**/
int phy_i2stx0_init(struct device *dev)
{
	// i2s normal
	acts_reset_peripheral_deassert(RESET_ID_I2S);

	acts_clock_peripheral_enable(CLOCK_ID_I2S0_TX);
	acts_clock_peripheral_enable(CLOCK_ID_AUDIO_DAC);

	memset(&i2stx_priv, 0, sizeof(struct i2stx_private_data));

#ifdef CONFIG_I2STX_SRD
	acts_clock_peripheral_enable(CLOCK_ID_I2S_SRD);
#endif

	return 0;
}

#ifdef CONFIG_I2STX_SRD
static int i2stx_srd_cfg(struct device *dev)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_i2stx0 *i2stx_reg = cfg->i2stx;
	u32_t reg;

	/* Select the I2STX SRD clock source to be HOSC */
	sys_write32(sys_read32(CMU_I2SCLK) & ~(CMU_I2SCLK_I2SRDCLKSRC_MASK), CMU_I2SCLK);
	sys_write32(sys_read32(CMU_I2SCLK) | (0x3 << CMU_I2SCLK_I2SRDCLKSRC_SHIFT), CMU_I2SCLK);

	reg = (I2ST0_SRDCTL_SRD_TH(I2STX_SRD_TH_DEFAULT) | I2ST0_SRDCTL_CNT_TIM(I2STX_SRD_CNT_TIM_DEFAULT));

	/* enable SRD IRQ */
	reg |= I2ST0_SRDCTL_SRD_IE;

	/* enable mute when detected sample rate or channel width has changed */
	reg |= I2ST0_SRDCTL_MUTE_EN;

	i2stx_reg->tx0_srdctl = reg;

	/* enable slave mode sample rate detect */
	i2stx_reg->tx0_srdctl |= I2ST0_SRDCTL_SRD_EN;

	return 0;
}

static inline u32_t read_i2stx_srd_count(struct device *dev)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_i2stx0 *i2stx_reg = cfg->i2stx;

	/* CNT of LRCLK which sampling by SRC_CLK */
	return ((i2stx_reg->tx0_srdstat & I2ST0_SRDSTA_CNT_MASK) >> I2ST0_SRDSTA_CNT_SHIFT);
}

static void i2stx_srd_fs_change(struct device *dev)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_i2stx0 *i2stx_reg = cfg->i2stx;
	u32_t cnt, fs;
	audio_sr_sel_e sr;
	cnt = read_i2stx_srd_count(dev);

	/* CNT = SRD_CLK / LRCLK and SRD_CLK uses HOSC which is a 24000000Hz clock source*/
	fs = 24000000 / cnt;

	/* Allow a 1% deviation */
	if ((fs > 7920) && (fs < 8080)) { /* 8kfs */
		sr = SAMPLE_RATE_8KHZ;
	} else if ((fs > 10915) && (fs < 11135)) { /* 11.025kfs */
		sr = SAMPLE_RATE_11KHZ;
	} else if ((fs > 11880) && (fs < 12120)) { /* 12kfs */
		sr = SAMPLE_RATE_12KHZ;
	} else if ((fs > 15840) && (fs < 16160)) { /* 16kfs */
		sr = SAMPLE_RATE_16KHZ;
	} else if ((fs > 21830) && (fs < 22270)) { /* 22.05kfs */
		sr = SAMPLE_RATE_22KHZ;
	} else if ((fs > 23760) && (fs < 24240)) { /* 24kfs */
		sr = SAMPLE_RATE_24KHZ;
	} else if ((fs > 31680) && (fs < 32320)) { /* 32kfs */
		sr = SAMPLE_RATE_32KHZ;
	} else if ((fs > 43659) && (fs < 44541)) { /* 44.1kfs */
		sr = SAMPLE_RATE_44KHZ;
	} else if ((fs > 47520) && (fs < 48480)) { /* 48kfs */
		sr = SAMPLE_RATE_48KHZ;
	} else if ((fs > 63360) && (fs < 64640)) { /* 64kfs */
		sr = SAMPLE_RATE_64KHZ;
	} else if ((fs > 87318) && (fs < 89082)) { /* 88.2kfs */
		sr = SAMPLE_RATE_88KHZ;
	} else if ((fs > 95040) && (fs < 96960)) { /* 96kfs */
		sr = SAMPLE_RATE_96KHZ;
	} else if ((fs > 174636) && (fs < 178164)) { /* 176.4kfs */
		sr = SAMPLE_RATE_176KHZ;
	} else if((fs > 190080) && (fs < 193920)) { /* 192kfs */
		sr = SAMPLE_RATE_192KHZ;
	} else {
		SYS_LOG_ERR("Invalid sample rate %d", fs);
		return ;
	}

	SYS_LOG_INF("Detect new sample rate %d -> %d", fs, sr);

	/* FIXME: If not do the fifo reset, the left and right channel will exchange probably. */
	i2stx_reg->tx0_fifoctl &= ~I2STX0_FIFOCTL_FIFO_RST;
	i2stx_reg->tx0_fifoctl |= I2STX0_FIFOCTL_FIFO_RST;

	if (i2stx_priv.srd_callback)
		i2stx_priv.srd_callback(i2stx_priv.cb_data, I2STX_SRD_FS_CHANGE, (void *)&sr);
}

static void i2stx_srd_wl_change(struct device *dev)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_i2stx0 *i2stx_reg = cfg->i2stx;
	u8_t width;

	width = i2stx_reg->tx0_srdstat & I2ST0_SRDSTA_WL_MASK;

	SYS_LOG_INF("Detect new width length: %d", width);

	if (((i2stx_priv.srd_wl == SRDSTA_WL_64RATE) && (width == SRDSTA_WL_32RATE))
		|| ((i2stx_priv.srd_wl == SRDSTA_WL_32RATE) && (width == SRDSTA_WL_64RATE))) {
		i2stx_priv.srd_wl = width;
		if (i2stx_priv.srd_callback) {
			i2stx_priv.srd_callback(i2stx_priv.cb_data,
				I2STX_SRD_WL_CHANGE, (void *)&i2stx_priv.srd_wl);
		}
	}
}
#endif

void phy_i2stx_isr(void *arg)
{
	struct device *dev = (struct device *)arg;
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_i2stx0 *i2stx_reg = cfg->i2stx;

	ARG_UNUSED(arg);

	if (i2stx_reg->tx0_fifocnt & I2ST0_FIFO_CNT_IP) {
		i2stx_priv.fifo_cnt++;
		/* Here we need to wait 100us for the synchronization of audio clock fields */
		k_busy_wait(100);
		i2stx_reg->tx0_fifocnt |= I2ST0_FIFO_CNT_IP;
	}

	/* Sample rate detection timeout irq pending */
	if (i2stx_reg->tx0_srdstat & I2ST0_SRDSTA_TO_PD) {
		i2stx_reg->tx0_srdstat |= I2ST0_SRDSTA_TO_PD;
#ifdef CONFIG_I2STX_SRD
		if (i2stx_priv.srd_callback)
			i2stx_priv.srd_callback(i2stx_priv.cb_data, I2STX_SRD_TIMEOUT, NULL);
#endif
	}

	/* Sample rate changed detection irq pending */
	if (i2stx_reg->tx0_srdstat & I2ST0_SRDSTA_SRC_PD) {
		i2stx_reg->tx0_srdstat |= I2ST0_SRDSTA_SRC_PD;
#ifdef CONFIG_I2STX_SRD
		i2stx_srd_fs_change(dev);
#endif
	}

	/* Channel width change irq pending */
	if (i2stx_reg->tx0_srdstat & I2ST0_SRDSTA_CHW_PD) {
		i2stx_reg->tx0_srdstat |= I2ST0_SRDSTA_CHW_PD;
#ifdef CONFIG_I2STX_SRD
		i2stx_srd_wl_change(dev);
#endif
	}
}

static void audio_out_i2stx_irq_disable(void)
{
	if(irq_is_enabled(IRQ_ID_AUDIO_I2S0_TX) != 0)
		irq_disable(IRQ_ID_AUDIO_I2S0_TX);
}

/**************************************************************
**	Description:	i2stx0 clock config
**	Input:      device / audio pll / fs / clock source / bclk rate
**	Output:
**	Return:    success /fail
**	Note:
***************************************************************
**/
int phy_i2stx0_clk_config(struct device *dev, aout_channel_node_t *ptr_channel, uint8_t fs_khz, aout_device_clk_e clk_src, i2s_bclk_rate_e bclk_rate)
{
	uint32_t freq;
	uint8_t get_series;
	uint32_t sample_hz;
	uint8_t use_audiopll_index = 0xff;

	uint32_t prediv=0;
	uint32_t clkdiv=0;
	sample_type_e fs_series=SAMPLE_TYPE_48;

	// config i2s tx clock
	sample_hz = get_sample_rate_hz(fs_khz);

	// 128fs  or  256fs
	if(bclk_rate != I2S_BCLK_64FS) {
		freq=sample_hz*128;
		ptr_channel->channel_clk_type = CLOCK_TYPE_128FS;
	} else {
		freq=sample_hz*256;
		ptr_channel->channel_clk_type = CLOCK_TYPE_256FS;
	}

#ifdef CONFIG_I2STX_SLAVE_MCLKSRC_INTERNAL
	freq*=2;
#endif

	switch(freq){
		case PLL_49152:
			clkdiv=0;
			prediv=0;
			break;

		case PLL_24576:
			clkdiv=0;
			prediv=1;
			break;

		case PLL_12288:
			clkdiv=1;
			prediv=1;
			break;

		case PLL_8192:
			clkdiv=2;
			prediv=1;
			break;

		case PLL_6144:
			clkdiv=3;
			prediv=1;
			break;

		case PLL_4096:
			clkdiv=4;
			prediv=1;
			break;

		case PLL_3072:
			clkdiv=5;
			prediv=1;
			break;

		case PLL_2048:
			clkdiv=6;
			prediv=1;
			break;

		case PLL_451584:
			clkdiv=0;
			prediv=0;
			fs_series=SAMPLE_TYPE_44;
			break;

		case PLL_22579:
			clkdiv=0;
			prediv=1;
			fs_series=SAMPLE_TYPE_44;
			break;

		case PLL_11289:
			clkdiv=1;
			prediv=1;
			fs_series=SAMPLE_TYPE_44;
			break;

		case PLL_5644:
			clkdiv=3;
			prediv=1;
			fs_series=SAMPLE_TYPE_44;
			break;

		case PLL_2822:
			clkdiv=5;
			prediv=1;
			fs_series=SAMPLE_TYPE_44;
			break;

		default:
			break;
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

	sys_write32((sys_read32(CMU_I2SCLK) & (~0xfff)), CMU_I2SCLK);

	if(clk_src == AOUT_I2S_CLK_DAC256FS) {
		// i2s tx 0  mclk 使用dac_clk_256fs
		sys_write32(sys_read32(CMU_ADDACLK) & (~0xff), CMU_ADDACLK);
		sys_write32((sys_read32(CMU_I2SCLK) | (0<<CMU_I2SCLK_I2STX0MCLKDACSRC) | (0 << CMU_I2SCLK_I2STX0MCLKSRC_SHIFT)), CMU_I2SCLK);
		sys_write32(sys_read32(CMU_ADDACLK) | (use_audiopll_index << CMU_ADDACLK_DACCLKSRC) | (prediv << CMU_ADDACLK_DACCLKPREDIV) | \
			(clkdiv << CMU_ADDACLK_DACCLKDIV_SHIFT), CMU_ADDACLK);
	} else if(clk_src == AOUT_I2S_CLK_DAC128FS) {
		// i2s tx 0 mclk 使用dac_clk_128fs
		sys_write32(sys_read32(CMU_ADDACLK) & (~0xff), CMU_ADDACLK);
		sys_write32((sys_read32(CMU_I2SCLK) | (1<<CMU_I2SCLK_I2STX0MCLKDACSRC) | (0 << CMU_I2SCLK_I2STX0MCLKSRC_SHIFT)), CMU_I2SCLK);
		sys_write32(sys_read32(CMU_ADDACLK) | (use_audiopll_index << CMU_ADDACLK_DACCLKSRC) | (prediv << CMU_ADDACLK_DACCLKPREDIV) | \
			(clkdiv << CMU_ADDACLK_DACCLKDIV_SHIFT), CMU_ADDACLK);
	} else {
		sys_write32(sys_read32(CMU_I2SCLK) | ((clk_src&0x0f) << CMU_I2SCLK_I2STX0MCLKSRC_SHIFT) | (use_audiopll_index << CMU_I2SCLK_I2S0CLKSRC) | \
			(prediv << CMU_I2SCLK_I2S0CLKPREDIV) | (clkdiv << CMU_I2SCLK_I2S0CLKDIV_SHIFT), CMU_I2SCLK);
	}

	PHY_audio_inc_apll_refcnt(use_audiopll_index, fs_series);

	return 0;
}

/**************************************************************
**	Description:	i2stx0 control register config
**	Input:      device / i2s setting / clock type / bclk rate
**	Output:
**	Return:    success /fail
**	Note:
***************************************************************
**/
int phy_i2stx0_ctl_config(struct device *dev, phy_i2stx_setting_t *i2stx_setting, uint8_t i2stx_clk_type, i2s_bclk_rate_e bclk_rate)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_i2stx0 *i2stx_reg = cfg->i2stx;

	// fix 24 bit wide
	i2stx_reg->tx0_ctl = 0;

	i2stx_reg->tx0_ctl |= I2STX0_CTL_WIDTH_24BIT;

	if(i2stx_setting->i2stx_role == I2S_DEVICE_SLAVE) {
		i2stx_reg->tx0_ctl |= I2STX0_CTL_MODE_SLAVE;
	}

	if(i2stx_clk_type == I2STX_MCLK_EXT) {
		i2stx_reg->tx0_ctl |= I2STX0_CTL_SMCLK_EXT;
	}

	i2stx_reg->tx0_ctl |= (i2stx_setting->i2stx_format << I2STX0_CTL_DATA_FORMAT_SHIFT);
	i2stx_reg->tx0_ctl |= (bclk_rate << I2STX0_CTL_BCLK_SET_SHIFT);

#ifdef CONFIG_I2STX_SRD
	if (i2stx_setting->i2stx_role == I2S_DEVICE_SLAVE) {
		SYS_LOG_INF("I2STX SRD enable");
		i2stx_srd_cfg(dev);
		i2stx_priv.srd_callback = i2stx_setting->srd_callback;
		i2stx_priv.cb_data = i2stx_setting->cb_data;
		i2stx_priv.srd_wl = SRDSTA_WL_64RATE;
		audio_out_i2stx_irq_enable();
	}
#endif

	return 0;
}

/**************************************************************
**	Description:	i2stx0 fifo register config
**	Input:      device / channel node
**	Output:
**	Return:    success /fail
**	Note:
***************************************************************
**/
int phy_i2stx0_fifo_config(struct device *dev, aout_channel_node_t *ptr_channel)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_i2stx0 *i2stx_reg = cfg->i2stx;
	struct acts_audio_dac *dac_reg = cfg->dac;

	uint8_t drq_en=0,irq_en=0;
	uint8_t fifo_sel;

	if (ptr_channel->channel_fifo_src == AOUT_FIFO_SRC_CPU) {
		irq_en = 1;
	} else if(ptr_channel->channel_fifo_src == AOUT_FIFO_SRC_DMA) {
		drq_en = 1;
	} else {
		;	//empty
	}

	if (ptr_channel->channel_fifo_sel > AOUT_FIFO_I2STX0) {
		return -1;
	} else if(ptr_channel->channel_fifo_sel == AOUT_FIFO_I2STX0) {
		fifo_sel = 1;	// i2stx0 fifo
	} else {
		fifo_sel = 0;	// dac fifo
	}

	if (ptr_channel->channel_fifo_src == AOUT_FIFO_SRC_ASRC) {
        i2stx_reg->tx0_fifoctl &= (~I2STX0_FIFOCTL_ASRC_SEL_MASK);
        if(ptr_channel->channel_asrc_index == ASRC_OUT_0){
            i2stx_reg->tx0_fifoctl |= I2STX0_FIFOCTL_ASRC_SEL(0);
        }else{
            i2stx_reg->tx0_fifoctl |= I2STX0_FIFOCTL_ASRC_SEL(1);
        }
	}

	// fifo select
	i2stx_reg->tx0_fifoctl &= (~I2STX0_FIFOCTL_FIFO_SEL_MASK);
	i2stx_reg->tx0_fifoctl |= (fifo_sel << I2STX0_FIFOCTL_FIFO_SEL_SHIFT);

	if (ptr_channel->channel_fifo_sel == AOUT_FIFO_DAC0) {
		dac_reg->digctl |= (DAC_DIGCTL_DAF0M2DAEN);
		dac_reg->fifoctl &= (~DAC_FIFOCTL_DAF0IS_MASK);
		dac_reg->fifoctl |= (ptr_channel->channel_fifo_src << DAC_FIFOCTL_DAF0IS_SHIFT);
		dac_reg->fifoctl |= (DAC_FIFOCTL_DAF0RT | DAC_FIFOCTL_DAF0EDE);
	} else if (ptr_channel->channel_fifo_sel == AOUT_FIFO_DAC1) {
		dac_reg->digctl |= (DAC_DIGCTL_DAF1M2DAEN);
		dac_reg->fifoctl &= (~DAC_FIFOCTL_DAF1IS_MASK);
		dac_reg->fifoctl |= (ptr_channel->channel_fifo_src << DAC_FIFOCTL_DAF1IS_SHIFT);
		dac_reg->fifoctl |= (DAC_FIFOCTL_DAF1RT | DAC_FIFOCTL_DAF1EDE);
	}  else if (ptr_channel->channel_fifo_sel == AOUT_FIFO_I2STX0) {
		// fifo source
		i2stx_reg->tx0_fifoctl &= (~I2STX0_FIFOCTL_FIFO_IN_MASK);
		i2stx_reg->tx0_fifoctl |= (ptr_channel->channel_fifo_src << I2STX0_FIFOCTL_FIFO_IN_SHIFT);

		i2stx_reg->tx0_fifoctl &= (~(I2STX0_FIFOCTL_FIFO_IEN | I2STX0_FIFOCTL_FIFO_DEN | I2STX0_FIFOCTL_FIFO_RST));
		k_busy_wait(100);
		i2stx_reg->tx0_fifoctl |= ((irq_en << I2STX0_FIFOCTL_FIFO_IEN_SHIFT) | (drq_en << I2STX0_FIFOCTL_FIFO_DEN_SHIFT) | I2STX0_FIFOCTL_FIFO_RST);

		if (ptr_channel->channel_fifo_src == AOUT_FIFO_SRC_CPU) {
			int i=32;
			for (i=0; i<32; i++) {
				i2stx_reg->tx0_dat = 0;
			}
		}
	} else {
		SYS_LOG_ERR("invalid fifo sel %d", ptr_channel->channel_fifo_sel);
		return -EINVAL;
	}

	return 0;
}

#if defined(CONFIG_I2STX_SLAVE_MCLKSRC_INTERNAL)
static inline uint8_t i2stx_sample_rate_update_internal_mclk(uint8_t sample_rate)
{
	uint8_t new_sample_rate;

	if (sample_rate == SAMPLE_48KHZ) {
		new_sample_rate = SAMPLE_64KHZ;
	} else if (sample_rate == SAMPLE_11KHZ) {
		new_sample_rate = SAMPLE_22KHZ;
	} else if (sample_rate == SAMPLE_12KHZ) {
		new_sample_rate = SAMPLE_16KHZ;
	} else if (sample_rate == SAMPLE_22KHZ) {
		new_sample_rate = SAMPLE_44KHZ;
	} else if (sample_rate == SAMPLE_24KHZ) {
		new_sample_rate = SAMPLE_48KHZ;
	} else if (sample_rate == SAMPLE_32KHZ) {
		new_sample_rate = SAMPLE_48KHZ;
	} else if (sample_rate == SAMPLE_88KHZ) {
		new_sample_rate = SAMPLE_176KHZ;
	} else if (sample_rate == SAMPLE_96KHZ) {
		new_sample_rate = SAMPLE_192KHZ;
	} else if (sample_rate == SAMPLE_176KHZ) {
		new_sample_rate = SAMPLE_176KHZ;
	} else if (sample_rate == SAMPLE_192KHZ) {
		new_sample_rate = SAMPLE_192KHZ;
	} else {
		new_sample_rate = sample_rate * 3 / 2;
	}

	return new_sample_rate;
}
#endif
/**************************************************************
**	Description:	enable i2s tx module
**	Input:      device / audio out param / channel node
**	Output:
**	Return:    success / fail
**	Note:
***************************************************************
**/
int PHY_audio_enable_i2stx(struct device *dev, PHY_audio_out_param_t *aout_param, aout_channel_node_t *ptr_channel)
{
#ifdef CONFIG_AUDIO_OUT_I2STX_ALWAYS_ON
	static bool i2stx_enabled = false;
#endif
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_i2stx0 *i2stx_reg = cfg->i2stx;

	phy_i2stx_setting_t *i2s_param;
	aout_device_clk_e i2stx_clk_src;
	i2s_bclk_rate_e i2stx_bclk_rate;
	uint8_t i2stx_clk_type;
	bool is_dac_linkmode;
	aout_fifo_src_e saved_fifo_src;

	if((aout_param == NULL) || (ptr_channel == NULL) ||(aout_param->ptr_i2stx_setting == NULL)) {
		SYS_LOG_ERR("parameters error!");
		return -EINVAL;
	}

#ifdef CONFIG_AUDIO_OUT_I2STX_ALWAYS_ON
	if(i2stx_enabled) {
		SYS_LOG_INF("i2stx always on");
	}
#endif
	SYS_LOG_INF("channel %d", ptr_channel->channel_type);

	i2s_param = aout_param->ptr_i2stx_setting;
	uint8_t sample_rate = i2s_param->i2stx_sample_rate;
#if defined(CONFIG_I2S0_PSEUDO_5WIRE) || defined(CONFIG_I2S1_PSEUDO_5WIRE)
	if (aout_param->ptr_i2stx_setting->i2stx_role != I2S_DEVICE_MASTER) {
		SYS_LOG_ERR("In 5 wires mode I2STX only support master mode");
		return -ENOTSUP;
	}
#endif

	// init i2s tx
	phy_i2stx0_init(dev);

	if((ptr_channel->channel_type == AOUT_CHANNEL_LINKAGE_DAC_I2S) || \
		(ptr_channel->channel_type == AOUT_CHANNEL_LINKAGE_DAC_I2S_SPDIF)) {
		is_dac_linkmode = TRUE;
		i2stx_clk_src = AOUT_I2S_CLK_DAC256FS;
	} else {
		is_dac_linkmode = FALSE;

		if(aout_param->ptr_i2stx_setting->i2stx_role == I2S_DEVICE_MASTER){
			if ((ptr_channel->channel_fifo_sel == AOUT_FIFO_DAC0)
				|| (ptr_channel->channel_fifo_sel == AOUT_FIFO_DAC1))
				i2stx_clk_src = AOUT_I2S_CLK_DAC256FS;
			else
            	i2stx_clk_src = AOUT_I2S_CLK_I2S0CLK;
		}else{
            i2stx_clk_src = AOUT_I2S_CLK_I2STX0MCLK_EXT;
		}
	}

	if((i2stx_clk_src & 0x10) == 0) {
		SYS_LOG_ERR("%d, i2stx0 clock config error!", __LINE__);
		return -1;
	}

	ptr_channel->channel_dev_clk = i2stx_clk_src;

	if(i2stx_clk_src != AOUT_I2S_CLK_I2STX0MCLK_EXT) {
		i2stx_clk_type = I2STX_MCLK_INT;           // i2s tx  使用内部时钟
	} else {
		i2stx_clk_type = I2STX_MCLK_EXT;           // i2s tx 使用外部时钟
	}

#ifdef CONFIG_I2STX_SLAVE_MCLKSRC_INTERNAL
	if (aout_param->ptr_i2stx_setting->i2stx_role == I2S_DEVICE_SLAVE) {
		if ((ptr_channel->channel_fifo_sel == AOUT_FIFO_DAC0)
			|| (ptr_channel->channel_fifo_sel == AOUT_FIFO_DAC1))
			i2stx_clk_src = AOUT_I2S_CLK_DAC256FS;
		else
			i2stx_clk_src = AOUT_I2S_CLK_I2S0CLK;

		i2stx_clk_type = I2STX_MCLK_INT;
		sample_rate = i2stx_sample_rate_update_internal_mclk(i2s_param->i2stx_sample_rate);
	}
#endif

	i2stx_bclk_rate = CONFIG_I2STX_BCLK_RATE;

	phy_i2stx0_clk_config(dev, ptr_channel, sample_rate, i2stx_clk_src, i2stx_bclk_rate);

	phy_i2stx0_ctl_config(dev, i2s_param, i2stx_clk_type, i2stx_bclk_rate);

	saved_fifo_src = ptr_channel->channel_fifo_src;

	if (ptr_channel->channel_fifo_sel == AOUT_FIFO_I2STX0) {
		ptr_channel->channel_fifo_src = AOUT_FIFO_SRC_CPU;
		phy_i2stx0_fifo_config(dev, ptr_channel);
		ptr_channel->channel_fifo_src = saved_fifo_src;
	}

	phy_i2stx0_fifo_config(dev, ptr_channel);

	if(!is_dac_linkmode) {

#ifdef CONFIG_I2S0_PSEUDO_5WIRE
		i2stx_reg->tx0_ctl |= I2STX0_CTL_I2S0RX_PSEUDO_5WEN;
#elif CONFIG_I2S1_PSEUDO_5WIRE
		i2stx_reg->tx0_ctl |= I2STX0_CTL_I2S1RX_PSEUDO_5WEN;
#endif
		i2stx_reg->tx0_ctl |= I2STX0_CTL_TXEN;
	}

#ifdef CONFIG_AUDIO_OUT_I2STX_ALWAYS_ON
	i2stx_enabled = true;
#endif

	return 0;
}


/**************************************************************
**	Description:	disable i2s tx module
**	Input:      device / i2stx used fifo
**	Output:
**	Return:    success / fail
**	Note:
***************************************************************
**/
int PHY_audio_disable_i2stx(struct device *dev, aout_channel_node_t *channel_node)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_dac *dac_reg = cfg->dac;
	struct acts_audio_i2stx0 *i2stx_reg = cfg->i2stx;
	uint8_t use_channel = channel_node->channel_fifo_sel;

	SYS_LOG_INF("fifo %d", use_channel);
#ifdef CONFIG_AUDIO_OUT_I2STX_ALWAYS_ON
	SYS_LOG_INF("i2stx always on");
	return 0;
#endif

	audio_out_i2stx_irq_disable();

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
		SYS_LOG_ERR("%d, disable i2stx error!\n", __LINE__);
		return -1;
	}

	i2stx_reg->tx0_ctl &= (~I2STX0_CTL_TXEN);

	//i2stx0_mfp_config(0);

	//i2s0 clock gating disable
	acts_clock_peripheral_disable(CLOCK_ID_I2S0_TX);

	PHY_audio_dec_apll_refcnt(channel_node->channel_apll_sel);

	return 0;
}

/**************************************************************
**	Description:	command and control internal i2stx channel
**	Input:      device structure  / channel
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int PHY_audio_ioctl_i2stx(struct device *dev, aout_channel_node_t *ptr_channel, uint32_t cmd, void *param)
{
    const struct acts_audio_out_config *cfg = dev->config->config_info;
    struct acts_audio_i2stx0 *i2stx_reg = cfg->i2stx;
	int ret = 0;
	u32_t cnt;
	SYS_IRQ_FLAGS flags;

	switch (cmd) {
		case AOUT_CMD_GET_SAMPLE_CNT:
		{
			sys_irq_lock(&flags);
			cnt = (i2stx_priv.fifo_cnt << 16) | (i2stx_reg->tx0_fifocnt & I2ST0_FIFO_CNT_CNT_MASK);
			sys_irq_unlock(&flags);
			*(u32_t *)param = cnt & (0x7FFFFFFF);
			//SYS_LOG_INF("I2STX FIFO counter: %d %d", *(u32_t *)param, i2stx_priv.fifo_cnt);
			break;
		}
		case AOUT_CMD_RESET_SAMPLE_CNT:
		{
			uint32_t key = irq_lock();
			i2stx_priv.fifo_cnt = 0;
			i2stx_reg->tx0_fifocnt &= ~(I2ST0_FIFO_CNT_EN | I2ST0_FIFO_CNT_IE);
			i2stx_reg->tx0_fifocnt |= I2ST0_FIFO_CNT_EN;
			i2stx_reg->tx0_fifocnt |= I2ST0_FIFO_CNT_IE;
			irq_unlock(key);
			audio_out_i2stx_irq_enable();

			break;
		}
		case AOUT_CMD_ENABLE_SAMPLE_CNT:
		{
			uint32_t key = irq_lock();
			i2stx_priv.fifo_cnt = 0;
			i2stx_reg->tx0_fifocnt |= I2ST0_FIFO_CNT_EN;
			i2stx_reg->tx0_fifocnt |= I2ST0_FIFO_CNT_IE;
			irq_unlock(key);
			audio_out_i2stx_irq_enable();

			break;
		}
		case AOUT_CMD_DISABLE_SAMPLE_CNT:
		{
			i2stx_priv.fifo_cnt = 0;
			i2stx_reg->tx0_fifocnt &= ~(I2ST0_FIFO_CNT_EN | I2ST0_FIFO_CNT_IE);
			break;
		}
		case AOUT_CMD_GET_CHANNEL_STATUS:
		{
			*(uint8_t *)param = 0;
			uint8_t fifo_status = 0;

			fifo_status = (i2stx_reg->tx0_fifostat & I2STX0_STAT_FIFOS_MASK) >> I2STX0_STAT_FIFOS_SHIFT;
			if (fifo_status < (AUDIO_I2STXFIFO_LEVEL / 2 - 1))
				*(uint8_t *)param |= AUDIO_CHANNEL_STATUS_BUSY;
			break;
		}
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
		default:
		{
			SYS_LOG_ERR("Unsupport command %d", cmd);
			ret = -ENOTSUP;
			break;
		}
	}

	return ret;
}

