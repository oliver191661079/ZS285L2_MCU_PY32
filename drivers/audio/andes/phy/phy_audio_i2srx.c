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
 * Description:    audio i2srx physical layer
 *
 * Change log:
 *	2018/11/15: Created by mike.
 ****************************************************************************
 */

#include "../audio_inner.h"

#include <audio_in.h>

#define I2SRX_MCLK_INT	(0)
#define I2SRX_MCLK_EXT	(1)

typedef volatile u32_t *REG32;

#define I2SRX_SRD_TH_DEFAULT			(1)
#define I2SRX_SRD_CNT_TIM_DEFAULT		(1)

#define I2SRX_SRD_CONFIG_TIMEOUT_MS		(500)

/*
 * struct i2srx_private_data
 * @brief Physical I2SRX device private data
 */
struct i2srx_private_data {
	uint8_t sample_rate;
#if defined(CONFIG_I2SRX_SRD) || defined(CONFIG_I2SRX1_SRD)
	int (*rx0_srd_callback)(void *cb_data, u32_t cmd, void *param);
	void *rx0_cb_data;
	u8_t rx0_srd_wl;

	int (*rx1_srd_callback)(void *cb_data, u32_t cmd, void *param);
	void *rx1_cb_data;
	u8_t rx1_srd_wl;
#endif
};

static struct i2srx_private_data i2srx_priv;

/**
**	i2s rx0 clock source
**/
//#define CONFIG_I2SRX0_CLK_SEL	(AIN_I2S0_CLK_I2SRX0MCLKEXT)
#define CONFIG_I2SRX0_CLK_SEL	(AIN_I2S0_CLK_I2STX0MCLK) //AIN_I2S0_CLK_I2S1CLK

/**
**	i2s rx0 bclk rate
**/
#define CONFIG_I2SRX0_BCLK_RATE	(I2S_BCLK_64FS)

/**
**	i2s rx1 clock source
**/
//#define CONFIG_I2SRX1_CLK_SEL	(AIN_I2S1_CLK_I2SRX1MCLKEXT)
#define CONFIG_I2SRX1_CLK_SEL	(AIN_I2S1_CLK_I2S1CLK)

/**
**	i2s rx1 bclk rate
**/
#define CONFIG_I2SRX1_BCLK_RATE	(I2S_BCLK_64FS)

#if defined(CONFIG_I2SRX_SRD) || defined(CONFIG_I2SRX1_SRD)
static int i2srx_srd_cfg(struct device *dev, ain_channel_type_e channel_type)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_i2srx0 *i2srx_reg = cfg->i2srx0;
	struct acts_audio_i2srx1 *i2srx1_reg = cfg->i2srx1;
	u32_t reg;

	/* Select the I2STX SRD clock source to be HOSC */
	sys_write32(sys_read32(CMU_I2SCLK) & ~(CMU_I2SCLK_I2SRDCLKSRC_MASK), CMU_I2SCLK);
	sys_write32(sys_read32(CMU_I2SCLK) | (0x3 << CMU_I2SCLK_I2SRDCLKSRC_SHIFT), CMU_I2SCLK);

	reg = (I2ST0_SRDCTL_SRD_TH(I2SRX_SRD_TH_DEFAULT) | I2ST0_SRDCTL_CNT_TIM(I2SRX_SRD_CNT_TIM_DEFAULT));

	/* enable SRD IRQ */
	reg |= I2ST0_SRDCTL_SRD_IE;

	/* enable mute when detected sample rate or channel width has changed */
	reg |= I2ST0_SRDCTL_MUTE_EN;

	if (channel_type == AIN_CHANNEL_I2SRX0) {
		i2srx_reg->rx0_srdctl = reg;
		i2srx_reg->rx0_srdctl |= I2ST0_SRDCTL_SRD_EN;

	} else {
		i2srx1_reg->rx1_srdctl = reg;
		i2srx1_reg->rx1_srdctl |= I2ST0_SRDCTL_SRD_EN;
	}

	return 0;
}

static inline u32_t read_i2srx_srd_count(struct device *dev, ain_channel_type_e channel_type)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_i2srx0 *i2srx_reg = cfg->i2srx0;
	struct acts_audio_i2srx1 *i2srx1_reg = cfg->i2srx1;

	if (channel_type == AIN_CHANNEL_I2SRX0)
		return ((i2srx_reg->rx0_srdsta & I2ST0_SRDSTA_CNT_MASK) >> I2ST0_SRDSTA_CNT_SHIFT);
	else
		return ((i2srx1_reg->rx1_srdsta & I2ST0_SRDSTA_CNT_MASK) >> I2ST0_SRDSTA_CNT_SHIFT);
}

static void i2srx_srd_fs_change(struct device *dev, ain_channel_type_e channel_type)
{
	u32_t cnt, fs;
	audio_sr_sel_e sr;
	cnt = read_i2srx_srd_count(dev, channel_type);

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
		//Do not return, and report 0 sample rate to app layer.
		//return ;
		sr = 0;
	}

	SYS_LOG_INF("%d %d -> %d", cnt, fs, sr);

	i2srx_priv.sample_rate = sr;

	if (channel_type == AIN_CHANNEL_I2SRX0) {
		if (i2srx_priv.rx0_srd_callback)
			i2srx_priv.rx0_srd_callback(i2srx_priv.rx0_cb_data, I2SRX_SRD_FS_CHANGE, (void *)&sr);
	} else {
		if (i2srx_priv.rx1_srd_callback)
			i2srx_priv.rx1_srd_callback(i2srx_priv.rx1_cb_data, I2SRX_SRD_FS_CHANGE, (void *)&sr);
	}
}

static void i2srx_srd_wl_change(struct device *dev, ain_channel_type_e channel_type)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_i2srx0 *i2srx_reg = cfg->i2srx0;
	struct acts_audio_i2srx1 *i2srx1_reg = cfg->i2srx1;
	u8_t width, srd_wl;

	if (channel_type == AIN_CHANNEL_I2SRX0) {
		width = i2srx_reg->rx0_srdsta & I2ST0_SRDSTA_WL_MASK;
		srd_wl = i2srx_priv.rx0_srd_wl;
	} else {
		width = i2srx1_reg->rx1_srdsta & I2ST0_SRDSTA_WL_MASK;
		srd_wl = i2srx_priv.rx1_srd_wl;
	}

	//SYS_LOG_INF("%d", width);

	if (((srd_wl == SRDSTA_WL_64RATE) && (width == SRDSTA_WL_32RATE))
		|| ((srd_wl == SRDSTA_WL_32RATE) && (width == SRDSTA_WL_64RATE))) {
		if (channel_type == AIN_CHANNEL_I2SRX0) {
			i2srx_priv.rx0_srd_wl = width;
			if (i2srx_priv.rx0_srd_callback) {
				i2srx_priv.rx0_srd_callback(i2srx_priv.rx0_cb_data,
					I2SRX_SRD_WL_CHANGE, (void *)&i2srx_priv.rx0_srd_wl);
			}
		} else {
			i2srx_priv.rx1_srd_wl = width;
			if (i2srx_priv.rx1_srd_callback) {
				i2srx_priv.rx1_srd_callback(i2srx_priv.rx1_cb_data,
					I2SRX_SRD_WL_CHANGE, (void *)&i2srx_priv.rx1_srd_wl);
			}
		}
	}

	if ((width != SRDSTA_WL_32RATE) && (width != SRDSTA_WL_64RATE)) {
#ifdef RESET_I2S_INTERNAL
		if (channel_type == AIN_CHANNEL_I2SRX0) {
			i2srx_priv.rx0_srd_wl = SRDSTA_WL_64RATE;
			i2srx_reg->rx0_srdctl &= ~I2ST0_SRDCTL_SRD_EN;
			k_busy_wait(1000);
			i2srx_reg->rx0_srdctl |= I2ST0_SRDCTL_SRD_EN;
		} else {
			i2srx_priv.rx1_srd_wl = SRDSTA_WL_64RATE;
			i2srx1_reg->rx1_srdctl &= ~I2ST0_SRDCTL_SRD_EN;
			k_busy_wait(1000);
			i2srx_reg->rx1_srdctl |= I2ST0_SRDCTL_SRD_EN;
		}
#endif
	}
}

static void PHY_i2srx_irq_disable(ain_channel_type_e channel_type)
{
	if (channel_type == AIN_CHANNEL_I2SRX0) {
		if (irq_is_enabled(IRQ_ID_AUDIO_I2S0_RX) != 0)
			irq_disable(IRQ_ID_AUDIO_I2S0_RX);
	} else if (channel_type == AIN_CHANNEL_I2SRX1) {
		if (irq_is_enabled(IRQ_ID_AUDIO_I2S1_RX) != 0)
			irq_disable(IRQ_ID_AUDIO_I2S1_RX);
	}
}
#endif

void phy_i2srx_isr(void *arg)
{
	struct device *dev = (struct device *)arg;
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_i2srx0 *i2srx_reg = cfg->i2srx0;
	struct acts_audio_i2srx1 *i2srx1_reg = cfg->i2srx1;

	ARG_UNUSED(arg);

	/* I2SRX0 Sample rate detection timeout irq pending */
	if (i2srx_reg->rx0_srdsta & I2ST0_SRDSTA_TO_PD) {
		i2srx_reg->rx0_srdsta |= I2ST0_SRDSTA_TO_PD;
#ifdef CONFIG_I2SRX_SRD
		if (i2srx_priv.rx0_srd_callback)
			i2srx_priv.rx0_srd_callback(i2srx_priv.rx0_cb_data, I2SRX_SRD_TIMEOUT, NULL);
#endif
	}

	/* I2SRX0 Sample rate changed detection irq pending */
	if (i2srx_reg->rx0_srdsta & I2ST0_SRDSTA_SRC_PD) {
		i2srx_reg->rx0_srdsta |= I2ST0_SRDSTA_SRC_PD;
		i2srx_reg->rx0_srdsta |= I2ST0_SRDSTA_CHW_PD;
#ifdef CONFIG_I2SRX_SRD
		i2srx_srd_fs_change(dev, AIN_CHANNEL_I2SRX0);
#endif
	}

	/* I2SRX0 Channel width change irq pending */
	if (i2srx_reg->rx0_srdsta & I2ST0_SRDSTA_CHW_PD) {
		i2srx_reg->rx0_srdsta |= I2ST0_SRDSTA_SRC_PD;
		i2srx_reg->rx0_srdsta |= I2ST0_SRDSTA_CHW_PD;
#ifdef CONFIG_I2SRX_SRD
		i2srx_srd_wl_change(dev, AIN_CHANNEL_I2SRX0);
#endif
	}

	/* I2SRX1 Sample rate detection timeout irq pending */
	if (i2srx1_reg->rx1_srdsta & I2ST0_SRDSTA_TO_PD) {
		i2srx1_reg->rx1_srdsta |= I2ST0_SRDSTA_TO_PD;
#ifdef CONFIG_I2SRX1_SRD
		if (i2srx_priv.rx1_srd_callback)
			i2srx_priv.rx1_srd_callback(i2srx_priv.rx1_cb_data, I2SRX_SRD_TIMEOUT, NULL);
#endif
	}

	/* I2SRX1 Sample rate changed detection irq pending */
	if (i2srx1_reg->rx1_srdsta & I2ST0_SRDSTA_SRC_PD) {
		i2srx1_reg->rx1_srdsta |= I2ST0_SRDSTA_SRC_PD;
		i2srx1_reg->rx1_srdsta |= I2ST0_SRDSTA_CHW_PD;
#ifdef CONFIG_I2SRX1_SRD
		i2srx_srd_fs_change(dev, AIN_CHANNEL_I2SRX1);
#endif
	}

	/* I2SRX1 Channel width change irq pending */
	if (i2srx1_reg->rx1_srdsta & I2ST0_SRDSTA_CHW_PD) {
		i2srx1_reg->rx1_srdsta |= I2ST0_SRDSTA_SRC_PD;
		i2srx1_reg->rx1_srdsta |= I2ST0_SRDSTA_CHW_PD;
#ifdef CONFIG_I2SRX1_SRD
		i2srx_srd_wl_change(dev, AIN_CHANNEL_I2SRX1);
#endif
	}
}

/**************************************************************
**	Description:	i2srx init
**	Input:      device
**	Output:
**	Return:    success /fail
**	Note:
***************************************************************
**/
int phy_i2srx_init(struct device *dev, ain_channel_type_e channel_type)
{

	// i2s normal
	acts_reset_peripheral_deassert(RESET_ID_I2S);

	if(channel_type == AIN_CHANNEL_I2SRX0) {
		acts_clock_peripheral_enable(CLOCK_ID_I2S0_RX);
#ifdef CONFIG_I2SRX_SRD
		i2srx_priv.rx0_srd_callback = NULL;
		i2srx_priv.rx0_cb_data = NULL;
		i2srx_priv.rx0_srd_wl = 0;
		acts_clock_peripheral_enable(CLOCK_ID_I2S_SRD);
#endif
	} else {
		acts_clock_peripheral_enable(CLOCK_ID_I2S1_RX);
#ifdef CONFIG_I2SRX1_SRD
		i2srx_priv.rx1_srd_callback = NULL;
		i2srx_priv.rx1_cb_data = NULL;
		i2srx_priv.rx1_srd_wl = 0;
		acts_clock_peripheral_enable(CLOCK_ID_I2S_SRD);
#endif
	}

	acts_clock_peripheral_enable(CLOCK_ID_AUDIO_ADC);

	return 0;
}

/**************************************************************
**	Description:	i2srx 0/1 clock config
**	Input:      device / fs / clock source / bclk rate / i2srx index
**	Output:
**	Return:    success /fail
**	Note:
***************************************************************
**/
int phy_i2srx_clk_config(struct device *dev, ain_channel_node_t *ptr_channel, uint8_t fs_khz, ain_device_clk_e clk_src, i2s_bclk_rate_e bclk_rate, uint8_t i2srx_index)
{
	uint32_t freq;
	uint32_t sample_hz;
	uint8_t get_series;
	uint8_t use_audiopll_index = 0xff;

	uint32_t prediv=0;
	uint32_t clkdiv=0;
	sample_type_e fs_series=SAMPLE_TYPE_48;

	sample_hz = get_sample_rate_hz(fs_khz);

	// 128fs  or  256fs
	if(bclk_rate != I2S_BCLK_64FS) {
		freq=sample_hz*128;
		ptr_channel->channel_clk_type = CLOCK_TYPE_128FS;
	} else {
		freq=sample_hz*256;
		ptr_channel->channel_clk_type = CLOCK_TYPE_256FS;
	}

	switch(freq) {
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

	if(i2srx_index == 0) {
		// i2s rx 0
		*(REG32)(CMU_I2SCLK) &= (~CMU_I2SCLK_I2SRX0MCLKSRC_MASK);

		if(clk_src == AIN_I2S0_CLK_ADCCLK) {
			*(REG32)(CMU_ADDACLK) &= ~(0x1f<<CMU_ADDACLK_ADCCLKDIV_SHIFT);
			*(REG32)(CMU_ADDACLK) |= (use_audiopll_index<<CMU_ADDACLK_ADCCLKSRC)|(prediv<<CMU_ADDACLK_ADCCLKPREDIV)|(clkdiv<<CMU_ADDACLK_ADCCLKDIV_SHIFT);
			*(REG32)(CMU_I2SCLK) |= ((clk_src & 0x0f)<<CMU_I2SCLK_I2SRX0MCLKSRC_SHIFT);
		} else if(clk_src == AIN_I2S0_CLK_I2STX0MCLK) {
			*(REG32)(CMU_I2SCLK) &= ~(0x1f);
			*(REG32)(CMU_I2SCLK) |= (use_audiopll_index<<CMU_I2SCLK_I2S0CLKSRC)|(prediv<<CMU_I2SCLK_I2S0CLKPREDIV)|(clkdiv<<CMU_I2SCLK_I2S0CLKDIV_SHIFT);
			*(REG32)(CMU_I2SCLK) |= ((clk_src & 0x0f)<<CMU_I2SCLK_I2SRX0MCLKSRC_SHIFT);
		} else if(clk_src == AIN_I2S0_CLK_I2S1CLK){
			*(REG32)(CMU_I2SCLK) &= ~(0x1f<<16);
			*(REG32)(CMU_I2SCLK) |= (use_audiopll_index<<CMU_I2SCLK_I2S1CLKSRC)|(prediv<<CMU_I2SCLK_I2S1CLKPREDIV)|(clkdiv<<CMU_I2SCLK_I2S1CLKDIV_SHIFT);
			*(REG32)(CMU_I2SCLK) |= ((clk_src & 0x0f)<<CMU_I2SCLK_I2SRX0MCLKSRC_SHIFT);
		} else {
			*(REG32)(CMU_I2SCLK) |= ((clk_src & 0x0f)<<CMU_I2SCLK_I2SRX0MCLKSRC_SHIFT);
		}
	}else {
		// i2s rx 1
		*(REG32)(CMU_I2SCLK) &= ~(0x3ff<<16);

		if(clk_src == AIN_I2S1_CLK_I2S1CLK){
			*(REG32)(CMU_I2SCLK) |= ((clk_src & 0x0f)<<CMU_I2SCLK_I2SRX1MCLKSRC_SHIFT)|(use_audiopll_index<<CMU_I2SCLK_I2S1CLKSRC);
			*(REG32)(CMU_I2SCLK) |= (prediv<<CMU_I2SCLK_I2S1CLKPREDIV)|(clkdiv<<CMU_I2SCLK_I2S1CLKDIV_SHIFT);
		} else if(clk_src == AIN_I2S1_CLK_I2STX0MCLK){
			//phy_i2stx0_clk_config(dev, fs_khz, AOUT_I2S_CLK_DAC256FS,bclk_rate);
			*(REG32)(CMU_I2SCLK) |= ((clk_src & 0x0f)<<CMU_I2SCLK_I2SRX1MCLKSRC_SHIFT);
		} else if(clk_src == AIN_I2S1_CLK_I2SRX0MCLK){
			phy_i2srx_clk_config(dev, ptr_channel, fs_khz, AIN_I2S0_CLK_ADCCLK, bclk_rate, 0);
			*(REG32)(CMU_I2SCLK) |= ((clk_src & 0x0f)<<CMU_I2SCLK_I2SRX1MCLKSRC_SHIFT);
		} else{
			*(REG32)(CMU_I2SCLK) |= ((clk_src & 0x0f)<<CMU_I2SCLK_I2SRX1MCLKSRC_SHIFT);
		}
	}

	PHY_audio_inc_apll_refcnt(use_audiopll_index, fs_series);

	return 0;
}

/**************************************************************
**	Description:	i2srx0/1 control register config
**	Input:      device / i2s setting / clock type / bclk rate / i2srx index
**	Output:
**	Return:    success /fail
**	Note:
***************************************************************
**/
int phy_i2srx_ctl_config(struct device *dev, phy_i2srx_setting_t *i2srx_setting, uint8_t i2srx_clk_type, i2s_bclk_rate_e bclk_rate, uint8_t i2srx_index)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_i2srx0 *i2srx0_reg = cfg->i2srx0;
	struct acts_audio_i2srx1 *i2srx1_reg = cfg->i2srx1;

	if(i2srx_index == 0) {
		//i2srx 0
		i2srx0_reg->rx0_ctl = 0;
		i2srx0_reg->rx0_ctl |= I2SRX0_CTL_WIDTH_24BIT;

		if(i2srx_setting->i2srx_role == I2S_DEVICE_SLAVE) {
			i2srx0_reg->rx0_ctl |= I2SRX0_CTL_MODE_SLAVE;
		}

		if(i2srx_clk_type == I2SRX_MCLK_EXT) {
			i2srx0_reg->rx0_ctl |= I2SRX0_CTL_SMCLK_EXT;
		}

		i2srx0_reg->rx0_ctl |= (i2srx_setting->i2srx_format << I2SRX0_CTL_DATA_FORMAT_SHIFT);
		i2srx0_reg->rx0_ctl |= (bclk_rate << I2SRX0_CTL_BCLK_SET_SHIFT);

#ifdef CONFIG_I2SRX_SRD
		if (i2srx_setting->i2srx_role == I2S_DEVICE_SLAVE) {
			SYS_LOG_INF("I2SRX0 SRD enable");
			i2srx_srd_cfg(dev, AIN_CHANNEL_I2SRX0);
			i2srx_priv.rx0_srd_callback = i2srx_setting->srd_callback;
			i2srx_priv.rx0_cb_data = i2srx_setting->cb_data;
			i2srx_priv.rx0_srd_wl = SRDSTA_WL_64RATE;
			audio_in_i2srx_irq_enable(AIN_CHANNEL_I2SRX0);
		}
#endif
	} else {
		// i2srx 1
		i2srx1_reg->rx1_ctl = 0;
		i2srx1_reg->rx1_ctl |= I2SRX1_CTL_WIDTH_24BIT;

		if(i2srx_setting->i2srx_role == I2S_DEVICE_SLAVE) {
			i2srx1_reg->rx1_ctl |= I2SRX1_CTL_MODE_SLAVE;
		}

		if(i2srx_clk_type == I2SRX_MCLK_EXT) {
			i2srx1_reg->rx1_ctl |= I2SRX1_CTL_SMCLK_EXT;
		}

		i2srx1_reg->rx1_ctl |= (i2srx_setting->i2srx_format << I2SRX1_CTL_DATA_FORMAT_SHIFT);
		i2srx1_reg->rx1_ctl |= (bclk_rate << I2SRX1_CTL_BCLK_SET_SHIFT);

#ifdef CONFIG_I2SRX1_SRD
		if (i2srx_setting->i2srx_role == I2S_DEVICE_SLAVE) {
			SYS_LOG_INF("I2SRX1 SRD enable");
			i2srx_srd_cfg(dev, AIN_CHANNEL_I2SRX1);
			i2srx_priv.rx1_srd_callback = i2srx_setting->srd_callback;
			i2srx_priv.rx1_cb_data = i2srx_setting->cb_data;
			i2srx_priv.rx1_srd_wl = SRDSTA_WL_64RATE;
			audio_in_i2srx_irq_enable(AIN_CHANNEL_I2SRX1);
		}
#endif
	}

	return 0;
}

/**************************************************************
**	Description:	i2srx0/1 FIFO register config
**	Input:      device / ain channle node
**	Output:
**	Return:    success /fail
**	Note:
***************************************************************
**/
int phy_i2srx_fifo_config(struct device *dev, ain_channel_node_t *ptr_channel)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	//struct acts_audio_i2srx0 *i2srx0_reg = cfg->i2srx0;
	struct acts_audio_i2srx1 *i2srx1_reg = cfg->i2srx1;
	struct acts_audio_adc *adc_reg = cfg->adc;

	uint8_t drq_en=0,irq_en=0;

	if(ptr_channel->channel_fifo_dst == AIN_FIFO_DST_CPU) {
		irq_en = 1;
	} else if(ptr_channel->channel_fifo_dst == AIN_FIFO_DST_DMA) {
		drq_en = 1;
	} else {
		;	//empty
	}

	if(ptr_channel->channel_fifo_sel == AIN_FIFO_ADC) {
		// i2s rx 0
		adc_reg->fifoctl &= (~ADC_FIFOCTL_ADFOS_MASK);
		adc_reg->fifoctl |= (ptr_channel->channel_fifo_dst << ADC_FIFOCTL_ADFOS_SHIFT);     /* adc fifo output */

		adc_reg->fifoctl |= ADC_FIFOCTL_ADFIS_I2SRX0;		/* adc fifo input */

		adc_reg->fifoctl &= (~(ADC_FIFOCTL_ADFFIE | ADC_FIFOCTL_ADFFDE | ADC_FIFOCTL_ADFRT));
		adc_reg->fifoctl |= ((irq_en << ADC_FIFOCTL_ADFFIE_SHIFT) | (drq_en << ADC_FIFOCTL_ADFFDE_SHIFT) | ADC_FIFOCTL_ADFRT);     /* irq / drq / fifo reset */

	} else if(ptr_channel->channel_fifo_sel == AIN_FIFO_I2SRX1) {
		// i2s rx 1
		i2srx1_reg->rx1_fifoctl &= (~I2SRX1_FIFOCTL_RXFOS_MASK);
		i2srx1_reg->rx1_fifoctl |= (ptr_channel->channel_fifo_dst << I2SRX1_FIFOCTL_RXFOS_SHIFT);     /* i2s rx1 fifo output */

		i2srx1_reg->rx1_fifoctl &= (~I2SRX1_FIFOCTL_RXFIS_MASK);		/* i2s rx1 fifo input:  i2s1 rx */

		i2srx1_reg->rx1_fifoctl &= (~(I2SRX1_FIFOCTL_RXFFIE | I2SRX1_FIFOCTL_RXFFDE | I2SRX1_FIFOCTL_RXFRT));
		i2srx1_reg->rx1_fifoctl |= ((irq_en << I2SRX1_FIFOCTL_RXFFIE_SHIFT) | (drq_en << I2SRX1_FIFOCTL_RXFFDE_SHIFT) | I2SRX1_FIFOCTL_RXFRT);     /* irq / drq / fifo reset */
	} else {
		return -1;
	}

	return 0;
}

#if defined(CONFIG_I2SRX0_SLAVE_MCLKSRC_INTERNAL) || defined(CONFIG_I2SRX1_SLAVE_MCLKSRC_INTERNAL)
static inline uint8_t i2srx_sample_rate_update_internal_mclk(uint8_t sample_rate)
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
**	Description:	enable i2s rx module
**	Input:      device / audio in param / channel node
**	Output:
**	Return:    success / fail
**	Note:
***************************************************************
**/
int PHY_audio_enable_i2srx(struct device *dev, PHY_audio_in_param_t *ain_param, ain_channel_node_t *ptr_channel)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_i2srx0 *i2srx0_reg = cfg->i2srx0;
	struct acts_audio_i2srx1 *i2srx1_reg = cfg->i2srx1;

	phy_i2srx_setting_t *i2s_param;
	ain_device_clk_e i2srx_clk_src;
	ain_channel_type_e channel_type;
	i2s_bclk_rate_e i2srx_bclk_rate;

	uint8_t i2srx_clk_type;
	uint8_t i2srx_index;
	uint8_t sample_rate;

	if((ain_param == NULL) || (ain_param->ptr_i2srx_setting == NULL)) {
		return -EINVAL;
	}

	i2s_param = ain_param->ptr_i2srx_setting;

	sample_rate = i2s_param->i2srx_sample_rate;

#if defined(CONFIG_I2S0_PSEUDO_5WIRE) || defined(CONFIG_I2S1_PSEUDO_5WIRE)
	if (ain_param->ptr_i2srx_setting->i2srx_role != I2S_DEVICE_SLAVE) {
		SYS_LOG_ERR("In pseudo 5 wires mode I2SRX only support slave mode");
		return -ENOTSUP;
	}
#endif

	channel_type = ptr_channel->channel_type;

	// init i2s rx
	phy_i2srx_init(dev, channel_type);

	if(channel_type == AIN_CHANNEL_I2SRX0) {
		// config i2s rx0
		i2srx_index = 0;
		//i2srx0_mfp_config(1);

		//如果i2s slave选择slave，则使用外部时钟，否则使用内部时钟
		if(ain_param->ptr_i2srx_setting->i2srx_role == I2S_DEVICE_SLAVE){
            i2srx_clk_src = AIN_I2S0_CLK_I2SRX0MCLKEXT;
		}else{
            i2srx_clk_src = AIN_I2S0_CLK_I2S1CLK;
		}

		if((i2srx_clk_src & 0x10) == 0) {
			SYS_LOG_ERR("%d, i2srx0 clock config error!", __LINE__);
			return -1;
		}

		if(i2srx_clk_src != AIN_I2S0_CLK_I2SRX0MCLKEXT) {
			i2srx_clk_type = I2SRX_MCLK_INT;           // i2s rx0  使用内部时钟
		} else {
			i2srx_clk_type = I2SRX_MCLK_EXT;           // i2s rx0 使用外部时钟
		}

#ifdef CONFIG_I2SRX0_SLAVE_MCLKSRC_INTERNAL
		if (ain_param->ptr_i2srx_setting->i2srx_role == I2S_DEVICE_SLAVE) {
			i2srx_clk_src = AIN_I2S0_CLK_I2S1CLK;
			i2srx_clk_type = I2SRX_MCLK_INT;
			sample_rate = i2srx_sample_rate_update_internal_mclk(i2s_param->i2srx_sample_rate);
		}
#endif

		i2srx_bclk_rate = CONFIG_I2SRX0_BCLK_RATE;
	} else {
		// config i2s rx1
		i2srx_index = 1;
		//i2srx1_mfp_config(1);

		//如果i2s slave选择slave，则使用外部时钟，否则使用内部时钟
		if(ain_param->ptr_i2srx_setting->i2srx_role == I2S_DEVICE_SLAVE){
            i2srx_clk_src = AIN_I2S1_CLK_I2SRX1MCLKEXT;
		}else{
            i2srx_clk_src = AIN_I2S1_CLK_I2S1CLK;
		}

		if((i2srx_clk_src & 0x20) == 0) {
			SYS_LOG_ERR("%d, i2srx1 clock config error!", __LINE__);
			return -1;
		}

		if(i2srx_clk_src != AIN_I2S1_CLK_I2SRX1MCLKEXT) {
			i2srx_clk_type = I2SRX_MCLK_INT;           // i2s rx1  使用内部时钟
		} else {
			i2srx_clk_type = I2SRX_MCLK_EXT;           // i2s rx1 使用外部时钟
		}

#ifdef CONFIG_I2SRX1_SLAVE_MCLKSRC_INTERNAL
		if (ain_param->ptr_i2srx_setting->i2srx_role == I2S_DEVICE_SLAVE) {
			i2srx_clk_src = AIN_I2S1_CLK_I2S1CLK;
			i2srx_clk_type = I2SRX_MCLK_INT;
			sample_rate = i2srx_sample_rate_update_internal_mclk(i2s_param->i2srx_sample_rate);
		}
#endif

		i2srx_bclk_rate = CONFIG_I2SRX1_BCLK_RATE;
	}

	ptr_channel->channel_dev_clk = i2srx_clk_src;

	// config i2s rx clock
	phy_i2srx_clk_config(dev, ptr_channel, sample_rate, i2srx_clk_src, i2srx_bclk_rate, i2srx_index);
	i2srx_priv.sample_rate = i2s_param->i2srx_sample_rate;

	phy_i2srx_ctl_config(dev, i2s_param, i2srx_clk_type, i2srx_bclk_rate, i2srx_index);

	phy_i2srx_fifo_config(dev, ptr_channel);

	if(i2srx_index == 0) {
		//i2srx0_mclk_config(0);
	} else {
		//i2srx1_mclk_config(0);
	}

	//if((i2s_param->i2srx_role == I2S_DEVICE_SLAVE) && (i2srx_clk_type == I2SRX_MCLK_EXT)) {
	if(i2srx_index == 0) {
		//i2srx0_mclk_config(1);
		i2srx0_reg->rx0_ctl |= I2SRX0_CTL_RXEN;
	} else {
		//i2srx1_mclk_config(1);
		i2srx1_reg->rx1_ctl |= I2SRX1_CTL_RXEN;
	}
	//}

	return 0;
}


/**************************************************************
**	Description:	disable i2s rx module
**	Input:      device / i2s rx channel
**	Output:
**	Return:    success / fail
**	Note:
***************************************************************
**/
int PHY_audio_disable_i2srx(struct device *dev, ain_channel_node_t *channel_node)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_adc *adc_reg = cfg->adc;
	struct acts_audio_i2srx0 *i2srx0_reg = cfg->i2srx0;
	struct acts_audio_i2srx1 *i2srx1_reg = cfg->i2srx1;

	uint8_t use_channel = channel_node->channel_fifo_sel;

	if (AIN_FIFO_ADC == use_channel) {
		//ADC FIFO reset
		adc_reg->fifoctl &= (~ADC_FIFOCTL_ADFRT);
		i2srx0_reg->rx0_ctl &= (~I2SRX0_CTL_RXEN);

#ifdef CONFIG_I2SRX_SRD
		PHY_i2srx_irq_disable(AIN_CHANNEL_I2SRX0);
		i2srx0_reg->rx0_srdctl &= ~I2ST0_SRDCTL_SRD_EN;
#endif

		//i2s0 clock gating disable
		acts_clock_peripheral_disable(CLOCK_ID_I2S0_RX);
	}else if(AIN_FIFO_I2SRX1 == use_channel) {
		//I2S RX1 FIFO reset
		i2srx1_reg->rx1_fifoctl &= (~I2SRX1_FIFOCTL_RXFRT);
		i2srx1_reg->rx1_ctl &= (~I2SRX1_CTL_RXEN);

#ifdef CONFIG_I2SRX1_SRD
		PHY_i2srx_irq_disable(AIN_CHANNEL_I2SRX1);
		i2srx1_reg->rx1_srdctl &= ~I2ST0_SRDCTL_SRD_EN;
#endif
		//i2s1 clock gating disable
		acts_clock_peripheral_disable(CLOCK_ID_I2S1_RX);
	}else {
		SYS_LOG_ERR("%d, disable i2srx error!\n", __LINE__);
		return -1;
	}

	PHY_audio_dec_apll_refcnt(channel_node->channel_apll_sel);

	return 0;
}

/**************************************************************
**	Description:	command and control internal i2srx channel
**	Input:      device structure  / channel
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int PHY_audio_ioctl_i2srx(struct device *dev, ain_channel_node_t *ptr_channel, uint32_t cmd, void *param)
{
	int ret = 0;

	switch (cmd) {
		case AIN_CMD_I2SRX_QUERY_SAMPLE_RATE:
		{
			*(uint8_t *)param = i2srx_priv.sample_rate;
			break;
		}
		case AIN_CMD_GET_APS:
		{
			ret = PHY_audio_get_aps(dev, ptr_channel, APS_LEVEL_AUDIOPLL, true);
			if (ret < 0) {
				SYS_LOG_ERR("get audio pll aps error:%d", ret);
				return ret;
			}
			*(uint8_t *)param = ret;
			ret = 0;
			break;
		}
		case AIN_CMD_SET_APS:
		{
			uint8_t aps = *(uint8_t *)param;
			ret = PHY_audio_set_aps(dev, ptr_channel, aps, APS_LEVEL_AUDIOPLL, true);
			break;
		}
		case AIN_CMD_GET_ASRC_APS:
		{
			ret = PHY_audio_get_aps(dev, ptr_channel, APS_LEVEL_ASRC, true);
			if (ret < 0) {
				SYS_LOG_ERR("get audio pll aps error:%d", ret);
				return ret;
			}
			*(uint8_t *)param = ret;
			break;
		}
		case AIN_CMD_SET_ASRC_APS:
		{
			uint8_t aps = *(uint8_t *)param;
			ret = PHY_audio_set_aps(dev, ptr_channel, aps, APS_LEVEL_ASRC, true);
			break;
		}
		default:
			SYS_LOG_ERR("Unsupport command %d", cmd);
			return -ENOTSUP;
	}

	return ret;
}

