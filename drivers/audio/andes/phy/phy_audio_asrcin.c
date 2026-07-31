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
 * Description:    audio asrc driver physical layer 
 *
 * Change log:
 *	2018/5/17: Created by mike.
 *   2018/8/1:   detach asrc in and asrc out interface
 ****************************************************************************
 */

#include "../audio_inner.h"
#include "../audio_asrc.h"

#include <audio_out.h>

#if 0
const asrc_param_t asrc_in_param[] =
{
    /* for record ??? */
    { ASRC_IN_CTL_WCLKSEL_I2SRX0, ASRC_IN_CTL_RCLKSEL_DSP, ASRC_IN_CTL_MODESEL_SRC, 5000, 5000, K_HF_SAMP_PAIR_NUM, K_HE_SAMP_PAIR_NUM , 0 },
    /* for hfp adc */
    { ASRC_IN_CTL_WCLKSEL_I2SRX0, ASRC_IN_CTL_RCLKSEL_DSP, ASRC_IN_CTL_MODESEL_SRC, 0, 0, 384, 128 , 1 },
    /* for usound adc(adc-asrc-dma-memery) */
    { ASRC_IN_CTL_WCLKSEL_I2SRX0, ASRC_IN_CTL_RCLKSEL_DMA, ASRC_IN_CTL_MODESEL_ASRC, 5000, 5000, 768, 256, 0 },
    /* for ain and da in */
    { ASRC_IN_CTL_WCLKSEL_I2SRX0, ASRC_IN_CTL_RCLKSEL_DSP, ASRC_IN_CTL_MODESEL_SRC, 0, 0, 768, 256, 1 },
};

const asrc_param_t asrc_in1_param[] =
{
    /* for record ??? */
    { ASRC_IN1_CTL_WCLKSEL_I2SRX0, ASRC_IN1_CTL_RCLKSEL_DSP, ASRC_IN1_CTL_MODESEL_SRC, 5000, 5000, K_HF_SAMP_PAIR_NUM, K_HE_SAMP_PAIR_NUM , 0 },
    /* for hfp adc */
    { ASRC_IN1_CTL_WCLKSEL_I2SRX0, ASRC_IN1_CTL_RCLKSEL_DSP, ASRC_IN1_CTL_MODESEL_SRC, 0, 0, 384, 128 , 1 },
    /* for usound adc(adc-asrc-dma-memery) */
    { ASRC_IN1_CTL_WCLKSEL_I2SRX0, ASRC_IN1_CTL_RCLKSEL_DMA, ASRC_IN1_CTL_MODESEL_ASRC, 5000, 5000, 768, 256, 0 },
    /* for ain and da in */
    { ASRC_IN1_CTL_WCLKSEL_I2SRX0, ASRC_IN1_CTL_RCLKSEL_DSP, ASRC_IN1_CTL_MODESEL_SRC, 0, 0, 768, 256, 1 },
};

#endif

/**************************************************************
**	Description:	config asrc in channel
**	Input:	asrc ram select / asrc mode 	
**	Output:    
**	Return:  
**	Note: 
***************************************************************
**/
static void config_in_ch(struct device *dev, u8_t nRamSel, phy_asrc_param_t *phy_asrc_set)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_asrc *asrc_reg = cfg->asrc;
	uint32_t wclk, rclk;

	if(phy_asrc_set == NULL) {
		return ;
	}

	if(phy_asrc_set->ctl_wr_clk == ASRC_IN_WCLK_IISRX0) {
		wclk = ASRC_IN_CTL_WCLKSEL_I2SRX0;
	} else if(phy_asrc_set->ctl_wr_clk == ASRC_IN_WCLK_IISRX1) {
		wclk = ASRC_IN_CTL_WCLKSEL_I2SRX1;
	} else {
		wclk = ASRC_IN_CTL_WCLKSEL_CPU;
	}

	if(phy_asrc_set->ctl_rd_clk == ASRC_IN_RCLK_DMA) {
		rclk = ASRC_IN_CTL_RCLKSEL_DMA;
	} else if(phy_asrc_set->ctl_rd_clk == ASRC_IN_RCLK_DSP) {
		rclk = ASRC_IN_CTL_RCLKSEL_DSP;
	} else {
		rclk = ASRC_IN_CTL_RCLKSEL_CPU;
	}

	// initialize asrc control register
	asrc_reg->asrc_in_ctl = K_ASRC_IN_CTL | (nRamSel << ASRC_IN_CTL_RAMSEL_SHIFT);

	//set wr clk
	asrc_reg->asrc_in_ctl &= (~ASRC_IN_CTL_WCLKSEL_MASK);
	asrc_reg->asrc_in_ctl |= wclk;

	//set rd clk
	asrc_reg->asrc_in_ctl &= (~ASRC_IN_CTL_RCLKSEL_MASK);
	asrc_reg->asrc_in_ctl |= rclk;

	//set asrc mode
	if(phy_asrc_set->ctl_mode != 0) {
		//asrc 
		asrc_reg->asrc_in_ctl |= ASRC_IN_CTL_MODESEL_ASRC;
	} else {
		//src
		asrc_reg->asrc_in_ctl |= ASRC_IN_CTL_MODESEL_SRC;
	}		

	// config decimation ratio
	asrc_reg->asrc_in_dec0 = (phy_asrc_set->reg_dec0);
	asrc_reg->asrc_in_dec1 = (phy_asrc_set->reg_dec1);

	printk("dec0 %d\n", asrc_reg->asrc_in_dec0);

	// config left channel gain
	asrc_reg->asrc_in_lgain = K_ASRC_IN_LGAIN;
	// config right channel gain
	asrc_reg->asrc_in_rgain = K_ASRC_IN_RGAIN;
	// clear pending bit
	asrc_reg->asrc_in_ip = asrc_reg->asrc_in_ip;
	// config half full threshold
	asrc_reg->asrc_in_thres_hf = phy_asrc_set->reg_hfull * (ASRC_IN_CTL_INCH0_AVERAGENUM(1) >> 16) * 2;
	// config half empty threshold
	asrc_reg->asrc_in_thres_he = phy_asrc_set->reg_hempty * (ASRC_IN_CTL_INCH0_AVERAGENUM(1) >> 16) * 2;
	// active FIFO and in channel
	asrc_reg->asrc_in_ctl |= (ASRC_IN_CTL_RESETWFIFO | ASRC_IN_CTL_RESETRFIFO | ASRC_IN_CTL_INEN);

	asrc_reg->asrc_in_ctl |= (phy_asrc_set->by_pass << ASRC_IN_CTL_BYPASSEN_SHIFT);

	config_asrc_channel_clk(asrc_reg, 2, 1);
	
}

/**************************************************************
**	Description:	config asrc in 1 channel
**	Input:	asrc ram select / asrc mode 	
**	Output:    
**	Return:  
**	Note: 
***************************************************************
**/
static void config_in1_ch(struct device *dev, u8_t nRamSel, phy_asrc_param_t *phy_asrc_set)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_asrc *asrc_reg = cfg->asrc;
	uint32_t wclk, rclk;

	if(phy_asrc_set == NULL) {
		return ;
	}

	if(phy_asrc_set->ctl_wr_clk == ASRC_IN_WCLK_IISRX0) {
		wclk = ASRC_IN_CTL_WCLKSEL_I2SRX0;
	} else if(phy_asrc_set->ctl_wr_clk == ASRC_IN_WCLK_IISRX1) {
		wclk = ASRC_IN_CTL_WCLKSEL_I2SRX1;
	} else {
		wclk = ASRC_IN_CTL_WCLKSEL_CPU;
	}

	if(phy_asrc_set->ctl_rd_clk == ASRC_IN_RCLK_DMA) {
		rclk = ASRC_IN_CTL_RCLKSEL_DMA;
	} else if(phy_asrc_set->ctl_rd_clk == ASRC_IN_RCLK_DSP) {
		rclk = ASRC_IN_CTL_RCLKSEL_DSP;
	} else {
		rclk = ASRC_IN_CTL_RCLKSEL_CPU;
	}

	// initialize asrc control register
	asrc_reg->asrc_in1_ctl = K_ASRC_IN1_CTL | (nRamSel << ASRC_IN1_CTL_RAMSEL_SHIFT);

	//set wr clk
	asrc_reg->asrc_in1_ctl &= (~ASRC_IN1_CTL_WCLKSEL_MASK);
	asrc_reg->asrc_in1_ctl |= wclk;

	//set rd clk
	asrc_reg->asrc_in1_ctl &= (~ASRC_IN1_CTL_RCLKSEL_MASK);
	asrc_reg->asrc_in1_ctl |= rclk;

	//set asrc mode
	if(phy_asrc_set->ctl_mode != 0) {
		//asrc 
		asrc_reg->asrc_in1_ctl |= ASRC_IN1_CTL_MODESEL_ASRC;
	} else {
		//src
		asrc_reg->asrc_in1_ctl |= ASRC_IN1_CTL_MODESEL_SRC;
	}	

	// config decimation ratio
	asrc_reg->asrc_in1_dec0 = (phy_asrc_set->reg_dec0);
	asrc_reg->asrc_in1_dec1 = (phy_asrc_set->reg_dec1);

	// config left channel gain
	asrc_reg->asrc_in1_lgain = K_ASRC_IN1_LGAIN;
	// config right channel gain
	asrc_reg->asrc_in1_rgain = K_ASRC_IN1_RGAIN;
	// clear pending bit
	asrc_reg->asrc_in1_ip = asrc_reg->asrc_in1_ip;
	// config half full threshold
	asrc_reg->asrc_in1_thres_hf = phy_asrc_set->reg_hfull * (ASRC_IN1_CTL_INCH1_AVERAGENUM(1) >> 16) * 2;
	// config half empty threshold
	asrc_reg->asrc_in1_thres_he = phy_asrc_set->reg_hempty * (ASRC_IN1_CTL_INCH1_AVERAGENUM(1) >> 16) * 2;
	// active FIFO and in channel
	asrc_reg->asrc_in1_ctl |= (ASRC_IN1_CTL_RESETWFIFO | ASRC_IN1_CTL_RESETRFIFO | ASRC_IN1_CTL_INEN);

	asrc_reg->asrc_in1_ctl |= (phy_asrc_set->by_pass << ASRC_IN1_CTL_BYPASSEN_SHIFT);

	config_asrc_channel_clk(asrc_reg, 3, 1);
	
}

/**************************************************************
**	Description:	asrc in 0 channel disable 
**	Input:	
**	Output:    
**	Return:   
**	Note: 
***************************************************************
**/
static void asrc_inchan_disbale(struct device *dev, u8_t index)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_asrc *asrc_reg = cfg->asrc;

	if(index == 0) {
		//asrc in 0
		asrc_reg->asrc_in_ctl &= (~(ASRC_IN_CTL_INEN | ASRC_IN_CTL_RESETRFIFO | ASRC_IN_CTL_RESETWFIFO));
	} else {
		//asrc in 1
		asrc_reg->asrc_in1_ctl &= (~(ASRC_IN1_CTL_INEN | ASRC_IN1_CTL_RESETRFIFO | ASRC_IN1_CTL_RESETWFIFO));
	}

	config_asrc_channel_clk(asrc_reg, index+2, 0);
}


/*
 * @fn      PHY_set_asrc_in_datawidth
 *
 * @brief   set asrc in data width, for dma use
 *
 * @param   in_channel @ref:asrcChannel_group
 * @param   data_width @ref:asrcDatawidth_group
 * 
 * @return  Function return value
 */
static void PHY_set_asrc_in_datawidth(struct device *dev, asrc_type_e asrc_type, uint32_t data_width)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_asrc *asrc_reg = cfg->asrc;

	switch (asrc_type)
	{
		case ASRC_IN_0:
			asrc_reg->asrc_in_dmactl = data_width;
			break;

		case ASRC_IN_1:
			asrc_reg->asrc_in1_dmactl = data_width;
			break;

		default:
			SYS_LOG_ERR("%d, set asrc in width error!\n", __LINE__);
			break;			
	}
}


/**************************************************************
**	Description:	open asrc in module
**	Input:	device / asrc ram select	
**	Output:    
**	Return:   success /fail 
**	Note: 
***************************************************************
**/
int PHY_open_asrc_in(struct device *dev, phy_asrc_param_t *phy_asrc_set)
{
	int result = 0;

	if(phy_asrc_set == NULL) {
		return -1;
	}

	// all possible combinations of ram blocks for asrc in
	switch (phy_asrc_set->asrc_ram) {
		// pcmram1/2
		case K_IN_P12: 
		switch_asrc_memory((BITMAP_ASRC_PCMRAM1 | BITMAP_ASRC_PCMRAM2), 0);
		config_in_ch(dev, ASRC_IN_PCMRAM12, phy_asrc_set);
		PHY_set_asrc_in_datawidth(dev, ASRC_IN_0, ASRCDATAWIDTH_16BIT);
		break;

		// pcmram2
		case K_IN_P2: 
		switch_asrc_memory(BITMAP_ASRC_PCMRAM2, 0);
		config_in_ch(dev, ASRC_IN_PCMRAM2, phy_asrc_set);
		PHY_set_asrc_in_datawidth(dev, ASRC_IN_0, ASRCDATAWIDTH_16BIT);
		break;

		// uram1
		case K_IN_U1:                   
		switch_asrc_memory(BITMAP_ASRC_URAM1, 0);
		config_in_ch(dev, ASRC_IN_URAM1, phy_asrc_set);
		PHY_set_asrc_in_datawidth(dev, ASRC_IN_0, ASRCDATAWIDTH_16BIT);
		break;

		// uram1/pcmram4
		case K_IN_U1P4:                   
		switch_asrc_memory((BITMAP_ASRC_URAM1 | BITMAP_ASRC_PCMRAM4), 0);
		config_in_ch(dev, ASRC_IN_URAM1PCMRAM4, phy_asrc_set);
		PHY_set_asrc_in_datawidth(dev, ASRC_IN_0, ASRCDATAWIDTH_24BIT);
		break;		

		// pcmram1
		case K_IN1_P1: 
		switch_asrc_memory(BITMAP_ASRC_PCMRAM1, 0);
		config_in1_ch(dev, ASRC_IN1_PCMRAM1, phy_asrc_set);
		PHY_set_asrc_in_datawidth(dev, ASRC_IN_1, ASRCDATAWIDTH_16BIT);
		break;

		// pcmram3
		case K_IN1_P3: 
		switch_asrc_memory(BITMAP_ASRC_PCMRAM3, 0);
		config_in1_ch(dev, ASRC_IN1_PCMRAM3, phy_asrc_set);
		PHY_set_asrc_in_datawidth(dev, ASRC_IN_1, ASRCDATAWIDTH_16BIT);
		break;

		// pcmram3/pcmram6
		case K_IN1_P36:                   
		switch_asrc_memory((BITMAP_ASRC_PCMRAM3 | BITMAP_ASRC_PCMRAM6), 0);
		config_in1_ch(dev, ASRC_IN1_PCMRAM36, phy_asrc_set);
		PHY_set_asrc_in_datawidth(dev, ASRC_IN_1, ASRCDATAWIDTH_24BIT);
		break;

		default:
		SYS_LOG_ERR("%d, open asrc in error!\n", __LINE__);
		result = -1;			
		break;

	}

	asrc_freq_adjust(dev);	

    return result;
}

/**************************************************************
**	Description:	close asrc in module
**	Input:	device / asrc ram select	
**	Output:    
**	Return:   success /fail 
**	Note: 
***************************************************************
**/
int PHY_close_asrc_in(struct device *dev, u8_t asrc_ramsel)
{
	int result = 0;
	uint16_t bitmap = 0;

	if(asrc_ramsel <= K_OUT1_MAX) {
		return -1;
	} else if(asrc_ramsel < K_IN0_MAX) {
		// close asrc in 0
		asrc_inchan_disbale(dev, 0);
	} else if(asrc_ramsel < K_ASRC_RAM_INDEX_MAX) {
		// close asrc in 1
		asrc_inchan_disbale(dev, 1);
	} else {
		return -1;
	}

	switch (asrc_ramsel) {
		// pcmram1/2
		case K_IN_P12: 
		bitmap = (BITMAP_ASRC_PCMRAM1 | BITMAP_ASRC_PCMRAM2);
		break;

		// pcmram2
		case K_IN_P2: 
		bitmap = BITMAP_ASRC_PCMRAM2;
		break;	

		// uram1
		case K_IN_U1: 
		bitmap = BITMAP_ASRC_URAM1;
		break;

		// uram1 / pcmram4
		case K_IN_U1P4:
		bitmap = (BITMAP_ASRC_URAM1 | BITMAP_ASRC_PCMRAM4);
		break;	

		// pcmram1
		case K_IN1_P1: 
		bitmap = BITMAP_ASRC_PCMRAM1;
		break;

		// pcmram3
		case K_IN1_P3: 
		bitmap = BITMAP_ASRC_PCMRAM3;
		break;	

		// pcmram3 / pcmram6
		case K_IN1_P36: 
		bitmap = (BITMAP_ASRC_PCMRAM3 | BITMAP_ASRC_PCMRAM6);
		break;

		default:
		SYS_LOG_ERR("%d, close asrc in error!\n", __LINE__);
		result = -1;			
		break;

	}

	if(bitmap != 0) {		
		switch_asrc_memory(bitmap, 1);	
	}

	asrc_freq_adjust(dev);
	
	return result;
}

/**************************************************************
**	Description:	adjust asrc rate
**	Input:	device / asrc param	
**	Output:    
**	Return:   success /fail 
**	Note: 
***************************************************************
**/
int PHY_set_asrc_in_rate(struct device *dev, ain_channel_node_t *channel_node, uint32_t asrc_rate, uint32_t asrc_offset)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_asrc *asrc_reg = cfg->asrc;	

	if (ASRC_IN_0 == channel_node->channel_asrc_index) {
		// asrc in 0
		asrc_reg->asrc_in_ip = asrc_reg->asrc_in_ip;

		asrc_reg->asrc_in_dec0 = (asrc_offset);
		asrc_reg->asrc_in_dec1 = asrc_reg->asrc_in_dec0;
		//asrc_reg->asrc_in_dec1 = (K_ASRC_IN_DEC1 + asrc_offset);

		//printk("%d, dec0: %x, dec1: %x\n", __LINE__, asrc_reg->asrc_in_dec0, asrc_reg->asrc_in_dec1);

	} else if (ASRC_IN_1 == channel_node->channel_asrc_index) {
		// asrc in 1 
		asrc_reg->asrc_in1_ip = asrc_reg->asrc_in1_ip;

		asrc_reg->asrc_in1_dec0 = (asrc_offset);
		asrc_reg->asrc_in1_dec1 = asrc_reg->asrc_in1_dec0;
		//asrc_reg->asrc_in1_dec1 = (K_ASRC_IN1_DEC1 + asrc_offset);	
		
        //printk("%d, dec0: %x, dec1: %x\n", __LINE__, asrc_reg->asrc_in1_dec0, asrc_reg->asrc_in1_dec1);
	} else {
		return -1;
	}

	return 0;
}

/**************************************************************
**	Description:	reset asrc in fifo
**	Input:	device / asrc param
**	Output:
**	Return:   success /fail
**	Note:
***************************************************************
**/
int PHY_reset_asrc_in_fifo(struct device *dev, ain_channel_node_t *channel_node)
{
	u32_t reg_val, irq_stat;

	irq_stat = irq_lock();

	if(channel_node->channel_asrc_index == ASRC_IN_0)
	{
		reg_val = sys_read32(ASRC_IN_CTL);
		if( (reg_val & ((1<<12)|(1<<1))) == ((1<<12)|(1<<1)) )
		{
			reg_val &= (~(1<<12));  // reset write fifo bit
			sys_write32(reg_val, ASRC_IN_CTL);

			k_busy_wait(1);

			reg_val |= (1<<12);
			sys_write32(reg_val, ASRC_IN_CTL);
		}
	}
	else if(channel_node->channel_asrc_index == ASRC_IN_1)
	{
		reg_val = sys_read32(ASRC_IN1_CTL);
		if( (reg_val & ((1<<12)|(1<<1))) == ((1<<12)|(1<<1)) )
		{
			reg_val &= (~(1<<12));  // reset write fifo bit
			sys_write32(reg_val, ASRC_IN1_CTL);

			k_busy_wait(1);

			reg_val |= (1<<12);
			sys_write32(reg_val, ASRC_IN1_CTL);
		}
	}

	irq_unlock(irq_stat);

	return 0;
}

