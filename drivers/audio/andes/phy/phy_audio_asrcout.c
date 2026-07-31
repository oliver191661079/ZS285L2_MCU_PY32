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
#include <soc_dvfs.h>
#include <audio_out.h>
#include <soc_cmu.h>

static void (*asrc_irq_callabck)(void);

const void* dspram_addr[ASRC_MAX_RAM_NUM] =
{
    PCM0_ADDR, PCM1_ADDR, PCM2_ADDR, URAM0_ADDR, URAM1_ADDR, PCM3_ADDR, PCM4_ADDR, PCM5_ADDR, PCM6_ADDR
};

const u32_t dspram_size[ASRC_MAX_RAM_NUM] =
{
    PCM_SIZE, PCM_SIZE, PCM_SIZE, URAM_SIZE, URAM_SIZE, PCM_SIZE, PCM4_SIZE, PCM5_SIZE, PCM6_SIZE
};

const u32_t asrc_memclk_en[ASRC_MAX_RAM_NUM] =
{
	(1 << CMU_MEMCLKEN_PCMRAM0CLKEN),
	(1 << CMU_MEMCLKEN_PCMRAM1CLKEN),
	(1 << CMU_MEMCLKEN_PCMRAM2CLKEN),
	(1 << CMU_MEMCLKEN_URAM0CLKEN),
	(1 << CMU_MEMCLKEN_URAM1CLKEN),
	(1 << CMU_MEMCLKEN_PCMRAM3CLKEN),
	(1 << CMU_MEMCLKEN_PCMRAM4CLKEN),
	(1 << CMU_MEMCLKEN_PCMRAM5CLKEN),
	(1 << CMU_MEMCLKEN_PCMRAM6CLKEN)
};

const u32_t asrc_clksel_mask[ASRC_MAX_RAM_NUM] =
{
	1 << CMU_MEMCLKSEL_PCMRAM0CLKSEL,
	1 << CMU_MEMCLKSEL_PCMRAM1CLKSEL,
	1 << CMU_MEMCLKSEL_PCMRAM2CLKSEL,
	3 << CMU_MEMCLKSEL_URAM0CLKSEL,
	3 << CMU_MEMCLKSEL_URAM1CLKSEL,
	1 << CMU_MEMCLKSEL_PCMRAM3CLKSEL,
	1 << CMU_MEMCLKSEL_PCMRAM4CLKSEL,
	1 << CMU_MEMCLKSEL_PCMRAM5CLKSEL,
	1 << CMU_MEMCLKSEL_PCMRAM6CLKSEL
};

const u32_t asrc_clksel[ASRC_MAX_RAM_NUM] =
{
	(1 << CMU_MEMCLKSEL_PCMRAM0CLKSEL),
	(1 << CMU_MEMCLKSEL_PCMRAM1CLKSEL),
	(1 << CMU_MEMCLKSEL_PCMRAM2CLKSEL),
	(2 << CMU_MEMCLKSEL_URAM0CLKSEL),
	(2 << CMU_MEMCLKSEL_URAM1CLKSEL),
	(1 << CMU_MEMCLKSEL_PCMRAM3CLKSEL),
	(1 << CMU_MEMCLKSEL_PCMRAM4CLKSEL),
	(1 << CMU_MEMCLKSEL_PCMRAM5CLKSEL),
	(1 << CMU_MEMCLKSEL_PCMRAM6CLKSEL)
};

const u32_t asrc_sample_tbl[] = { 8, 11, 12, 16, 22, 24, 32, 44, 48, 96 };

//for both out & in &index is for sample rate
const u32_t asrc_freq_table[][2] =
{
	//mhz, out / in
	{ 3, 6 }, //8khz
	{ 4, 8 }, //11.025khz
	{ 4, 8 }, //12khz
	{ 6, 11 }, //16khz
	{ 8, 15 }, //22.05khz
	{ 8, 16 }, //24khz
	{ 11, 21 }, //32khz
	{ 16, 29 }, //44.1khz
	{ 18, 31 }, //48khz
	{ 32, 64 }, //96khz
};

/**************************************************************
**	Description:	config asrc channel clock
**	Input:
**	Output:
**	Return:
**	Note:
***************************************************************
**/
void config_asrc_channel_clk(struct acts_audio_asrc *asrc_reg, uint8_t index, u32_t enable)
{
	u32_t shift;

	if(index == 0) {
		//asrc out 0
		shift = 2;
	} else if(index == 1) {
		//asrc out 1
		shift = 3;
	} else if(index == 2) {
		//asrc in 0
		shift = 0;
	} else if(index == 3) {
		//asrc in 1
		shift = 1;
	} else {
		// all channel
		shift = 0xff;
	}

	if(shift != 0xff) {
		// single channel
		asrc_reg->asrc_clk_ctl &= (~(1 << shift));
		asrc_reg->asrc_clk_ctl |= (enable << shift);
	} else {
		// all channel
		asrc_reg->asrc_clk_ctl &= (~(0x0f << 0));
		if(enable != 0) {
			asrc_reg->asrc_clk_ctl |= (0x0f << 0);
		}
	}
}

static void acts_asrc_irq_routinue(void *arg)
{
	struct acts_audio_asrc *asrc_reg = (struct acts_audio_asrc *)ASRC_REG_BASE;

	if(asrc_reg->asrc_int_pd & 0x6){
		asrc_reg->asrc_int_pd = 0x6;
	}

	SYS_LOG_INF();

	if(asrc_irq_callabck){
		asrc_irq_callabck();
	}
}

/**************************************************************
**	Description:	init asrc module
**	Input:
**	Output:
**	Return:   success /fail
**	Note:
***************************************************************
**/
int PHY_init_asrc(struct device *dev)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_asrc *asrc_reg = cfg->asrc;

	// enable asrc clock
	acts_clock_peripheral_enable(CLOCK_ID_ASRC);

	// active asrc
	acts_reset_peripheral(RESET_ID_ASRC);

	//select 512 table
	asrc_reg->asrc_in_ctl &= (~ASRC_IN_CTL_TABLESEL_MASK);

	// enable asrc buf0 clock
	sys_write32(sys_read32(CMU_MEMCLKEN) | (1 << CMU_MEMCLKEN_ASRCBUF0CLKEN), CMU_MEMCLKEN);
	sys_write32(sys_read32(CMU_MEMCLKSEL) | (1 << CMU_MEMCLKSEL_ASRCBUF0CLKSEL), CMU_MEMCLKSEL);

	// disable all asrc channel clock
	config_asrc_channel_clk(asrc_reg, 0xff, 0);

	IRQ_CONNECT(IRQ_ID_ASRC, CONFIG_IRQ_PRIO_NORMAL, acts_asrc_irq_routinue, 0, 0);

	return 0;
}

int config_asrc_freq(u32_t asrc_freq)
{
    soc_dvfs_set_asrc_rate(asrc_freq);

	return 0;
}

/**************************************************************
**	Description:  config asrc clock
**	Input:
**	Output:
**	Return:   destination freq
**	Note:
***************************************************************
**/
int asrc_freq_adjust(struct device *dev)
{
	int result = 0;
	u32_t freq_index = 0, temp_data;
	u32_t dest_freq = 0, chan_in = 0, chan_out = 0;
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_asrc *asrc_reg = cfg->asrc;

	temp_data = (asrc_reg->asrc_out0_ctl & ASRC_OUT0_CTL_OUTCH0EN);
	if (temp_data != 0) {
		chan_out++;
	}

	temp_data = (asrc_reg->asrc_out1_ctl & ASRC_OUT1_CTL_OUTCH1EN);
	if (temp_data != 0) {
		chan_out++;
	}

	temp_data = (asrc_reg->asrc_in_ctl & ASRC_IN_CTL_INEN);
	if (temp_data != 0) {
		chan_in++;
	}

	temp_data = (asrc_reg->asrc_in1_ctl & ASRC_IN1_CTL_INEN);
	if (temp_data != 0) {
		chan_in++;
	}

	if (chan_out != 0) {
		//check freq
		result = PHY_audio_get_dac_samplerate();
		if(result < 0) {
			return -1;
		}

		temp_data = (u32_t)result;
		for (freq_index = 0; freq_index < 10; freq_index++) {
			if (asrc_sample_tbl[freq_index] == temp_data) {
				break;
			}
		}

		if (freq_index == 10) {
			freq_index = 9;
		}

		temp_data = asrc_freq_table[freq_index][0];

		dest_freq = temp_data * chan_out;
	}

	if (chan_in != 0) {
		temp_data = asrc_freq_table[freq_index][1];

		dest_freq += temp_data * chan_in;
	}

	SYS_LOG_DBG("%d, asrc freq: %d\n", __LINE__, dest_freq);

	if((chan_out == 0) && (chan_in == 0)) {
		// close asrc clock
        acts_clock_peripheral_disable(CLOCK_ID_ASRC);
		// reset asrc
		acts_reset_peripheral_assert(RESET_ID_ASRC);
	} else {
		result = config_asrc_freq(dest_freq);
	}

	return result;
}



/**************************************************************
**	Description:	alloc or free asrc ram
**	Input:	 memory bitmap / alloc(0) or free(1)
**	Output:
**	Return:   success /fail
**	Note:
***************************************************************
**/
static int deal_for_dspram(u32_t bit_index, uint8_t mode)
{
	void* ram_addr;
	u32_t ram_size;

	if(bit_index >= ASRC_MAX_RAM_NUM) {
		return -EINVAL;
	}

	ram_addr = (void *)dspram_addr[bit_index];
	ram_size = dspram_size[bit_index];

	if(0 == mode) {
		// malloc ram for asrc
		//dspram_alloc_range(ram_addr, ram_size);
	} else {
		// free asrc ram
		//dspram_free_range(ram_addr, ram_size);
	}

	return 0;
}

/**************************************************************
**	Description:	enable or disable asrc ram clock
**	Input:	 memory bitmap / mode
**	Output:
**	Return:   success /fail
**	Note:
***************************************************************
**/
static int deal_for_asrc_memory(u32_t bit_index, uint8_t mode)
{
	if(bit_index >= ASRC_MAX_RAM_NUM) {
		return -EINVAL;
	}

	if(0 == mode) {
		// switch memory clock to asrc
		sys_write32(sys_read32(CMU_MEMCLKEN) | asrc_memclk_en[bit_index], CMU_MEMCLKEN);
		sys_write32((sys_read32(CMU_MEMCLKSEL) & (~(asrc_clksel_mask[bit_index]))) | asrc_clksel[bit_index], CMU_MEMCLKSEL);
	} else {
		// switch memory clock to cpu
		sys_write32(sys_read32(CMU_MEMCLKSEL) & (~(asrc_clksel_mask[bit_index])), CMU_MEMCLKSEL);
	}

	return 0;
}

/**************************************************************
**	Description:	config asrc memory clock
**	Input:	 memory bitmap
**	Output:
**	Return:   success /fail
**	Note:
***************************************************************
**/
int switch_asrc_memory(u16_t ram_bitmap, uint8_t mode)
{
	u32_t loop;

	// check input param
	if(ram_bitmap >= (1 << ASRC_MAX_RAM_NUM)) {
		return -EINVAL;
	}

	for(loop = 0; loop < ASRC_MAX_RAM_NUM; loop++) {
		if((ram_bitmap & (1 << loop)) != 0) {
			deal_for_dspram(loop, mode);
			deal_for_asrc_memory(loop, mode);
        }
    }

    return 0;
}

/**************************************************************
**	Description:	check asrc status:  open or close
**	Input:		asrc channel index
**	Output:
**	Return:   true/false
**	Note:
***************************************************************
**/
bool asrc_status_check(struct device *dev, uint8_t index)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_asrc *asrc_reg = cfg->asrc;

	u32_t value = 0;
	bool result = FALSE;

	if(index == ASRC_OUT_0) {
	    printk("asrc %x\n", asrc_reg->asrc_out0_ctl);
		// asrc out 0
		value = (asrc_reg->asrc_out0_ctl & ASRC_OUT0_CTL_OUTCH0EN);
	} else if(index == ASRC_OUT_1) {
		// asrc out 1
		value = (asrc_reg->asrc_out1_ctl & ASRC_OUT1_CTL_OUTCH1EN);
	} else if(index == ASRC_IN_0) {
		// asrc in 0
		value = (asrc_reg->asrc_in_ctl & ASRC_IN_CTL_INEN);
	} else if(index == ASRC_IN_1) {
		// asrc in 1
		value = (asrc_reg->asrc_in1_ctl & ASRC_IN1_CTL_INEN);
	} else {
		// all asrc
		value = ((asrc_reg->asrc_out0_ctl & ASRC_OUT0_CTL_OUTCH0EN) | (asrc_reg->asrc_out1_ctl & ASRC_OUT1_CTL_OUTCH1EN) | \
				(asrc_reg->asrc_in_ctl & ASRC_IN_CTL_INEN) | (asrc_reg->asrc_in1_ctl & ASRC_IN1_CTL_INEN));
	}

	if(value != 0) {
		result = TRUE;
	}

	printk("check index %d result %d\n", index, result);

	return result;
}


/**************************************************************
**	Description:	asrc out 0 channel disable
**	Input:
**	Output:
**	Return:
**	Note:
***************************************************************
**/
static void asrc_outchan_disbale(struct device *dev, uint8_t index)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_asrc *asrc_reg = cfg->asrc;

	if(index == 0) {
		//asrc out 0
		asrc_reg->asrc_out0_ctl &= (~(ASRC_OUT0_CTL_OUTCH0EN | ASRC_OUT0_CTL_RESETRFIFO | ASRC_OUT0_CTL_RESETWFIFO));
	} else {
		//asrc out 1
		asrc_reg->asrc_out1_ctl &= (~(ASRC_OUT1_CTL_OUTCH1EN | ASRC_OUT1_CTL_RESETRFIFO | ASRC_OUT1_CTL_RESETWFIFO));
	}

	config_asrc_channel_clk(asrc_reg, index, 0);
}

/**************************************************************
**	Description:	config asrc out0 channel
**	Input:	asrc ram select / asrc mode
**	Output:
**	Return:
**	Note:
***************************************************************
**/
static void config_out0_ch(struct device *dev, uint8_t nRamSel, phy_asrc_param_t *phy_asrc_set)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_asrc *asrc_reg = cfg->asrc;
	u32_t wclk, rclk;

	if(phy_asrc_set == NULL) {
		return;
	}

	if(phy_asrc_set->ctl_wr_clk == ASRC_OUT_WCLK_DMA) {
		wclk = ASRC_OUT0_CTL_WCLKSEL_DMA;
	} else if(phy_asrc_set->ctl_wr_clk == ASRC_OUT_WCLK_DSP) {
		wclk = ASRC_OUT0_CTL_WCLKSEL_DSP;
	} else {
		wclk = ASRC_OUT0_CTL_WCLKSEL_CPU;
	}

	if(phy_asrc_set->ctl_rd_clk == ASRC_OUT_RCLK_DAC) {
		rclk = ASRC_OUT0_CTL_RCLKSEL_DAC;
	} else if(phy_asrc_set->ctl_rd_clk == ASRC_OUT_RCLK_IIS) {
		rclk = ASRC_OUT0_CTL_RCLKSEL_I2S;
	} else {
		rclk = ASRC_OUT0_CTL_RCLKSEL_CPU;
	}

	// initialize asrc control register
	asrc_reg->asrc_out0_ctl = K_ASRC_OUT0_CTL | (nRamSel << ASRC_OUT0_CTL_RAMSEL_SHIFT);

	//set wr clk
	asrc_reg->asrc_out0_ctl &= ~(ASRC_OUT0_CTL_OUTCH0_W_CLKSEL_MASK);
	asrc_reg->asrc_out0_ctl |= wclk;

	//set rd clk
	asrc_reg->asrc_out0_ctl &= ~(ASRC_OUT0_CTL_OUTCH0_R_CLKSEL_MASK);
	asrc_reg->asrc_out0_ctl |= rclk;

	//set write fifo and read fifo
	asrc_reg->asrc_out0_ctl &= (~ASRC_OUT0_CTL_RESETWFIFO);
	asrc_reg->asrc_out0_ctl |= ASRC_OUT0_CTL_RESETWFIFO;

	asrc_reg->asrc_out0_ctl &= (~ASRC_OUT0_CTL_RESETRFIFO);
	asrc_reg->asrc_out0_ctl |= ASRC_OUT0_CTL_RESETRFIFO;

	if (phy_asrc_set->ctl_dma_width)
		asrc_reg->asrc_out0_dmactl = (phy_asrc_set->ctl_dma_width / 8) - 1;


	//set asrc mode
	if(phy_asrc_set->ctl_mode != 0) {
		//asrc
		asrc_reg->asrc_out0_ctl |= ASRC_OUT0_CTL_MODESEL_ASRC;
	} else {
		//src
		asrc_reg->asrc_out0_ctl |= ASRC_OUT0_CTL_MODESEL_SRC;
	}

	// config decimation ratio
	asrc_reg->asrc_out0_dec0 = (phy_asrc_set->reg_dec0);
	asrc_reg->asrc_out0_dec1 = (phy_asrc_set->reg_dec1);

	// config left channel gain
	asrc_reg->asrc_out0_lgain = K_ASRC_OUT0_LGAIN;
	// config right channel gain
	asrc_reg->asrc_out0_rgain = K_ASRC_OUT0_RGAIN;
	// clear pending bit
	asrc_reg->asrc_out0_ip = asrc_reg->asrc_out0_ip;

	// config half full threshold
	asrc_reg->asrc_out0_thres_hf = phy_asrc_set->reg_hfull * (ASRC_OUT0_CTL_OUTCH0_AVERAGENUM(1) >> ASRC_OUT0_CTL_OUTCH0_AVERAGENUM_SHIFT) * 2;
	// config half empty threshold
	asrc_reg->asrc_out0_thres_he = phy_asrc_set->reg_hempty * (ASRC_OUT0_CTL_OUTCH0_AVERAGENUM(1) >> ASRC_OUT0_CTL_OUTCH0_AVERAGENUM_SHIFT) * 2;
	// active FIFO and out0 channel
	asrc_reg->asrc_out0_ctl |= (ASRC_OUT0_CTL_RESETWFIFO | ASRC_OUT0_CTL_RESETRFIFO | ASRC_OUT0_CTL_OUTCH0EN);

	asrc_reg->asrc_out0_ctl |= (phy_asrc_set->by_pass << ASRC_OUT0_CTL_BYPASSEN_SHIFT);

	config_asrc_channel_clk(asrc_reg, 0, 1);

}

/**************************************************************
**	Description:	config asrc out1 channel
**	Input:	asrc ram select / asrc mode
**	Output:
**	Return:
**	Note:
***************************************************************
**/
static void config_out1_ch(struct device *dev, uint8_t nRamSel, phy_asrc_param_t *phy_asrc_set)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_asrc *asrc_reg = cfg->asrc;
	u32_t wclk, rclk;

	if(phy_asrc_set == NULL) {
		return;
	}

	if(phy_asrc_set->ctl_wr_clk == ASRC_OUT_WCLK_DMA) {
		wclk = ASRC_OUT0_CTL_WCLKSEL_DMA;
	} else if(phy_asrc_set->ctl_wr_clk == ASRC_OUT_WCLK_DSP) {
		wclk = ASRC_OUT0_CTL_WCLKSEL_DSP;
	} else {
		wclk = ASRC_OUT0_CTL_WCLKSEL_CPU;
	}

	if(phy_asrc_set->ctl_rd_clk == ASRC_OUT_RCLK_DAC) {
		rclk = ASRC_OUT0_CTL_RCLKSEL_DAC;
	} else if(phy_asrc_set->ctl_rd_clk == ASRC_OUT_RCLK_IIS) {
		rclk = ASRC_OUT0_CTL_RCLKSEL_I2S;
	} else {
		rclk = ASRC_OUT0_CTL_RCLKSEL_CPU;
	}

	// initialize asrc control register
	asrc_reg->asrc_out1_ctl = K_ASRC_OUT1_CTL | (nRamSel << ASRC_OUT1_CTL_RAMSEL_SHIFT);

	//set wr clk
	asrc_reg->asrc_out1_ctl &= ~(ASRC_OUT1_CTL_OUTCH1_W_CLKSEL_MASK);
	asrc_reg->asrc_out1_ctl |= wclk;

	//set rd clk
	asrc_reg->asrc_out1_ctl &= ~(ASRC_OUT1_CTL_OUTCH1_R_CLKSEL_MASK);
	asrc_reg->asrc_out1_ctl |= rclk;

	//set write fifo and read fifo
	asrc_reg->asrc_out1_ctl &= (~ASRC_OUT1_CTL_RESETWFIFO);
	asrc_reg->asrc_out1_ctl |= ASRC_OUT1_CTL_RESETWFIFO;

	asrc_reg->asrc_out1_ctl &= (~ASRC_OUT1_CTL_RESETRFIFO);
	asrc_reg->asrc_out1_ctl |= ASRC_OUT1_CTL_RESETRFIFO;

	if (phy_asrc_set->ctl_dma_width)
		asrc_reg->asrc_out1_dmactl = (phy_asrc_set->ctl_dma_width / 8) - 1;


	//set asrc mode
	if(phy_asrc_set->ctl_mode != 0) {
		//asrc
		asrc_reg->asrc_out1_ctl |= ASRC_OUT1_CTL_MODESEL_ASRC;
	} else {
		//src
		asrc_reg->asrc_out1_ctl |= ASRC_OUT1_CTL_MODESEL_SRC;
	}

	// config decimation ratio
	asrc_reg->asrc_out1_dec0 = (phy_asrc_set->reg_dec0);
	asrc_reg->asrc_out1_dec1 = (phy_asrc_set->reg_dec1);

	// config left channel gain
	asrc_reg->asrc_out1_lgain = K_ASRC_OUT1_LGAIN;
	// config right channel gain
	asrc_reg->asrc_out1_rgain = K_ASRC_OUT1_RGAIN;
	// clear pending bit
	asrc_reg->asrc_out1_ip = asrc_reg->asrc_out1_ip;

	// config half full threshold
	asrc_reg->asrc_out1_thres_hf = phy_asrc_set->reg_hfull * (ASRC_OUT1_CTL_OUTCH1_AVERAGENUM(1) >> ASRC_OUT1_CTL_OUTCH1_AVERAGENUM_SHIFT) * 2;
	// config half empty threshold
	asrc_reg->asrc_out1_thres_he = phy_asrc_set->reg_hempty * (ASRC_OUT1_CTL_OUTCH1_AVERAGENUM(1) >> ASRC_OUT1_CTL_OUTCH1_AVERAGENUM_SHIFT) * 2;
	// active FIFO and out1 channel
	asrc_reg->asrc_out1_ctl |= (ASRC_OUT1_CTL_RESETWFIFO | ASRC_OUT1_CTL_RESETRFIFO | ASRC_OUT1_CTL_OUTCH1EN);

	asrc_reg->asrc_out1_ctl |= (phy_asrc_set->by_pass << ASRC_OUT1_CTL_BYPASSEN_SHIFT);

	config_asrc_channel_clk(asrc_reg, 1, 1);

}

/*
 * @fn      PHY_set_asrc_out_datawidth
 *
 * @brief   set asrc out data width, for dma use
 *
 * @param   out_channel @ref:asrcChannel_group
 * @param   data_width @ref:asrcDatawidth_group
 *
 * @return  Function return value
 */
static void PHY_set_asrc_out_datawidth(struct device *dev, asrc_type_e asrc_type, uint32_t data_width)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_asrc *asrc_reg = cfg->asrc;

	switch (asrc_type)
	{
		case ASRC_OUT_0:
			asrc_reg->asrc_out0_dmactl = data_width;
			break;

		case ASRC_OUT_1:
			asrc_reg->asrc_out1_dmactl = data_width;
			break;

		default:
			SYS_LOG_ERR("%d, set asrc out width error!\n", __LINE__);
			break;
	}
}

/*
 * @fn      PHY_set_asrc_out_start_threashold
 *
 * @brief   set asrc out start threashold, for dma use
 *
 * @param   out_channel @ref:asrcChannel_group
 * @param  st @ref:asrc start threashold
 *
 * @return  Function return value
 */
int PHY_set_asrc_out_start_threashold(struct device *dev, aout_channel_node_t *channel_node, uint32_t start_threashold)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_asrc *asrc_reg = cfg->asrc;

	if(channel_node->channel_asrc_index == ASRC_OUT_0) {
		asrc_reg->asrc_out0_st_level = start_threashold;
	} else if(channel_node->channel_asrc_index == ASRC_OUT_1) {
		asrc_reg->asrc_out1_st_level = start_threashold;
	} else {
		return -1;
	}

	return 0;
}

/**************************************************************
**	Description:	open asrc out module
**	Input:	device / asrc ram select
**	Output:
**	Return:   success /fail
**	Note:
***************************************************************
**/
int PHY_open_asrc_out(struct device *dev, phy_asrc_param_t *phy_asrc_set)
{
	int result = 0;

	if(phy_asrc_set == NULL) {
		return -1;
	}

	// all possible combinations of ram blocks for asrc out
	switch (phy_asrc_set->asrc_ram) {

		// pcmram 0/1/2  2K*2*24bit
		case K_OUT0_P012:
		switch_asrc_memory((BITMAP_ASRC_PCMRAM0 | BITMAP_ASRC_PCMRAM1 | BITMAP_ASRC_PCMRAM2), 0);
		config_out0_ch(dev, ASRC_OUT0_PCMRAM012, phy_asrc_set);
		PHY_set_asrc_out_datawidth(dev, ASRC_OUT_0, ASRCDATAWIDTH_24BIT);
		break;

		// pcmram 0/1  2K*2*16bit
		case K_OUT0_P01:
		switch_asrc_memory((BITMAP_ASRC_PCMRAM0 | BITMAP_ASRC_PCMRAM1), 0);
		config_out0_ch(dev, ASRC_OUT0_PCMRAM01, phy_asrc_set);
		PHY_set_asrc_out_datawidth(dev, ASRC_OUT_0, ASRCDATAWIDTH_16BIT);
		break;

		// pcmram 0  1K*2*16bit
		case K_OUT0_P0:
		switch_asrc_memory(BITMAP_ASRC_PCMRAM0, 0);
		config_out0_ch(dev, ASRC_OUT0_PCMRAM0, phy_asrc_set);
		PHY_set_asrc_out_datawidth(dev, ASRC_OUT_0, ASRCDATAWIDTH_16BIT);
		break;

		// uram0  0.5K*2*16bit
		case K_OUT0_U0:
		switch_asrc_memory(BITMAP_ASRC_URAM0, 0);
		config_out0_ch(dev, ASRC_OUT0_URAM0, phy_asrc_set);
		PHY_set_asrc_out_datawidth(dev, ASRC_OUT_0, ASRCDATAWIDTH_16BIT);
		break;

		// uram0/pcmram5  0.5K*2*24bit
		case K_OUT0_U0P5:
		switch_asrc_memory((BITMAP_ASRC_URAM0 | BITMAP_ASRC_PCMRAM5), 0);
		config_out0_ch(dev, ASRC_OUT0_URAM0PCMRAM5, phy_asrc_set);
		PHY_set_asrc_out_datawidth(dev, ASRC_OUT_0, ASRCDATAWIDTH_24BIT);
		break;

		// pcmram2  1K*2*16bit
		case K_OUT1_P2:
		switch_asrc_memory(BITMAP_ASRC_PCMRAM2, 0);
		config_out1_ch(dev, ASRC_OUT1_PCMRAM2, phy_asrc_set);
		PHY_set_asrc_out_datawidth(dev, ASRC_OUT_1, ASRCDATAWIDTH_16BIT);
		break;

		// pcmram1/2  2K*2*16bit
		case K_OUT1_P12_16bit:

		// 1K*2*24bit
		case K_OUT1_P12_24bit:
		switch_asrc_memory((BITMAP_ASRC_PCMRAM1 | BITMAP_ASRC_PCMRAM2), 0);
		if(phy_asrc_set->asrc_ram == K_OUT1_P12_16bit){
			config_out1_ch(dev, ASRC_OUT1_PCMRAM12_16bit, phy_asrc_set);
			PHY_set_asrc_out_datawidth(dev, ASRC_OUT_1, ASRCDATAWIDTH_16BIT);
		}else{
			config_out1_ch(dev, ASRC_OUT1_PCMRAM12_24bit, phy_asrc_set);
			PHY_set_asrc_out_datawidth(dev, ASRC_OUT_1, ASRCDATAWIDTH_24BIT);
		}
		break;

		// pcmram3  1K*2*16bit
		case K_OUT1_P3:
		switch_asrc_memory(BITMAP_ASRC_PCMRAM3, 0);
		config_out1_ch(dev, ASRC_OUT1_PCMRAM3, phy_asrc_set);
		PHY_set_asrc_out_datawidth(dev, ASRC_OUT_1, ASRCDATAWIDTH_16BIT);
		break;

		// pcmram1  1K*2*16bit
		case K_OUT1_P1:
		switch_asrc_memory(BITMAP_ASRC_PCMRAM1, 0);
		config_out1_ch(dev, ASRC_OUT1_PCMRAM1, phy_asrc_set);
		PHY_set_asrc_out_datawidth(dev, ASRC_OUT_1, ASRCDATAWIDTH_16BIT);
		break;

		default:
		SYS_LOG_ERR("%d, open asrc out error!\n", __LINE__);
		result = -1;
		break;

	}

	asrc_freq_adjust(dev);

	return result;
}

/**************************************************************
**	Description:	close asrc out module
**	Input:	device / asrc ram select
**	Output:
**	Return:   success /fail
**	Note:
***************************************************************
**/
int PHY_close_asrc_out(struct device *dev, uint8_t asrc_ramsel)
{
	int result = 0;
	u16_t bitmap = 0;

	if(asrc_ramsel < K_OUT0_MAX) {
		// close asrc out 0
		asrc_outchan_disbale(dev, 0);
	} else if(asrc_ramsel < K_OUT1_MAX) {
		// close asrc out 1
		asrc_outchan_disbale(dev, 1);
	} else {
		// param error
		return -1;
	}

	switch (asrc_ramsel) {

		// pcmram0/1/2
		case K_OUT0_P012:
		bitmap = (BITMAP_ASRC_PCMRAM0 | BITMAP_ASRC_PCMRAM1 | BITMAP_ASRC_PCMRAM2);
		break;

		// pcmram0/1
		case K_OUT0_P01:
		bitmap = (BITMAP_ASRC_PCMRAM0 | BITMAP_ASRC_PCMRAM1);
		break;

		// pcmram0
		case K_OUT0_P0:
		bitmap = BITMAP_ASRC_PCMRAM0;
		break;

		// uram0
		case K_OUT0_U0:
		bitmap = BITMAP_ASRC_URAM0;
		break;

		// uram0 /pcmram5
		case K_OUT0_U0P5:
		bitmap = (BITMAP_ASRC_URAM0 | BITMAP_ASRC_PCMRAM5);
		break;

		// pcmram2
		case K_OUT1_P2:
		bitmap = BITMAP_ASRC_PCMRAM2;
		break;

		// pcmram1/2
		case K_OUT1_P12_16bit:
		case K_OUT1_P12_24bit:
		bitmap = (BITMAP_ASRC_PCMRAM1 | BITMAP_ASRC_PCMRAM2);
		break;

		// pcmram3
		case K_OUT1_P3:
		bitmap = BITMAP_ASRC_PCMRAM3;
		break;

		// pcmram1
		case K_OUT1_P1:
		bitmap = BITMAP_ASRC_PCMRAM1;
		break;

		default:
		SYS_LOG_ERR("%d, close asrc out error!\n", __LINE__);
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
**	Description:	adjust asrc out rate
**	Input:	device / asrc param
**	Output:
**	Return:   success /fail
**	Note:
***************************************************************
**/
int PHY_set_asrc_out_rate(struct device *dev, aout_channel_node_t *channel_node, u32_t asrc_rate, u32_t asrc_offset)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_asrc *asrc_reg = cfg->asrc;

	if (ASRC_OUT_0 == channel_node->channel_asrc_index) {
		// asrc out 0
		asrc_reg->asrc_out0_ip = asrc_reg->asrc_out0_ip;

		asrc_reg->asrc_out0_dec0 = (asrc_offset);
		asrc_reg->asrc_out0_dec1 = asrc_reg->asrc_out0_dec0;

	} else if (ASRC_OUT_1 == channel_node->channel_asrc_index) {
		// asrc out 1
		asrc_reg->asrc_out1_ip = asrc_reg->asrc_out1_ip;

		asrc_reg->asrc_out1_dec0 = (asrc_offset);
		asrc_reg->asrc_out1_dec1 = asrc_reg->asrc_out1_dec0;

	} else {
		return -1;
	}
	printk("asrc_rate --%d channel_sample_rate %d  index %d \n",asrc_rate, channel_node->channel_sample_rate,channel_node->channel_asrc_index);

	return 0;
}
/**************************************************************
**	Description:	config asrc FIFO sample count function
**	Input:      device structure / asrc node / config mode
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/

int PHY_asrc_get_changed_samples_cnt(struct device *dev, aout_channel_node_t *channel_node, int duration)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_asrc *asrc_reg = cfg->asrc;
	int asrc_ratio = 0;

	if (!channel_node) {
		return -1;
	}

	if (channel_node->input_sample_rate == 0) {
		//channel_node->input_sample_rate = channel_node->channel_sample_rate;
	}

	if (duration == 0)
		return channel_node->changed_sample_cnt;

	if (ASRC_OUT_0 == channel_node->channel_asrc_index) {
		asrc_ratio = K_ASRC_OUT0_DEC0 * 1000 /  asrc_reg->asrc_out0_dec0;
	} else if (ASRC_OUT_1 == channel_node->channel_asrc_index) {
		 asrc_ratio = K_ASRC_OUT1_DEC0 * 1000 /  asrc_reg->asrc_out1_dec0;
	}

	channel_node->changed_sample_cnt += channel_node->input_sample_rate * (asrc_ratio - 1000) * duration / 1000;
	return channel_node->changed_sample_cnt;
}
int PHY_asrc_reset_changed_samples_cnt(struct device *dev, aout_channel_node_t *channel_node)
{
	if(channel_node == NULL) {
		return -1;
	}

	channel_node->changed_sample_cnt = 0;

	return 0;

}

int PHY_set_asrc_out_vol_gain(asrc_type_e type,u32_t gain)
{
	if(type == ASRC_OUT_0){
		sys_write32(gain, ASRC_OUT0_LGAIN);
		sys_write32(gain, ASRC_OUT0_RGAIN);
	}else if(type == ASRC_OUT_1){
		sys_write32(gain, ASRC_OUT1_LGAIN);
		sys_write32(gain, ASRC_OUT1_RGAIN);
	}else{
		printk("%s set 0x%x failed\n",__func__, gain);
	}

	return 0;
}

int PHY_get_asrc_out_vol_gain(asrc_type_e type)
{
	if(type == ASRC_OUT_0){
		return sys_read32( ASRC_OUT0_LGAIN);
	}else if(type == ASRC_OUT_1){
		return sys_read32( ASRC_OUT1_LGAIN);
	}

	return -1;
}



int PHY_write_zero_to_asrc_fifo_by_cpu(asrc_type_e type,u32_t sample)
{

    int asrc_out_ctl = 0;
    u32_t asrc_w_clk_select = 0;

    if(!sample)
        return 0;

    if(type == ASRC_OUT_1){
        asrc_out_ctl = sys_read32(ASRC_OUT1_CTL);
        asrc_w_clk_select = asrc_out_ctl & (BIT(6)|BIT(7));
        asrc_out_ctl &= ~(BIT(6)|BIT(7));
        sys_write32(asrc_out_ctl,ASRC_OUT1_CTL);

        for(int i = sample; i > 0; i--){
            sys_write32(0,ASRC_OUT1_WFIFO);
        }
        asrc_out_ctl = sys_read32(ASRC_OUT1_CTL);
        asrc_out_ctl &= ~(BIT(6)|BIT(7));
        asrc_out_ctl |= asrc_w_clk_select;
        sys_write32(asrc_out_ctl,ASRC_OUT1_CTL);
        printk("%s filled %d  asrc_out_ctl 1 0x%x\n",__func__, sample, asrc_out_ctl);
        return sample;

	}else if( type ==ASRC_OUT_0 ){
        asrc_out_ctl = sys_read32(ASRC_OUT0_CTL);
        asrc_w_clk_select = asrc_out_ctl & (BIT(6)|BIT(7));
        asrc_out_ctl &= ~(BIT(6)|BIT(7));
        sys_write32(asrc_out_ctl,ASRC_OUT0_CTL);

        for(int i = sample; i > 0; i--){
            sys_write32(0,ASRC_OUT0_WFIFO);
        }
        asrc_out_ctl = sys_read32(ASRC_OUT0_CTL);
        asrc_out_ctl &= ~(BIT(6)|BIT(7));
        asrc_out_ctl |= asrc_w_clk_select;
        sys_write32(asrc_out_ctl,ASRC_OUT0_CTL);
        printk("%s filled %d  asrc_out_ctl 0 0x%x\n",__func__, sample, asrc_out_ctl);
        return sample;
    }
	return 0;
}


int phy_get_asrc_remain_samples(u32_t channel_no)
{
	if(channel_no == 0){
		return ((sys_read32(ASRC_OUT0_IP) >> 16) & 0x1fff);
	}else{
		return ((sys_read32(ASRC_OUT1_IP) >> 16) & 0x1fff);
	}
}

int PHY_get_asrc_out_remain_samples_by_type(asrc_type_e type)
{
	if(type == ASRC_OUT_0){
		return ((sys_read32(ASRC_OUT0_IP) >> 16) & 0x1fff);
    }else if(type == ASRC_OUT_1){
		return ((sys_read32(ASRC_OUT1_IP) >> 16) & 0x1fff);
	}
    return 0;
}

int PHY_request_asrc_irq(void (*asrc_callback)(void), u32_t threashold)
{
	struct acts_audio_asrc *asrc_reg = (struct acts_audio_asrc *)ASRC_REG_BASE;

	SYS_LOG_INF("cb %p threashold %d", asrc_callback, threashold);
	if(asrc_callback){

		asrc_reg->asrc_out0_int_thres = threashold;
		asrc_reg->asrc_out1_int_thres = threashold;
		asrc_reg->asrc_int_pd = 6;
		asrc_reg->asrc_int_en |= 6;

		asrc_irq_callabck = asrc_callback;
		irq_enable(IRQ_ID_ASRC);
	}else{
		asrc_reg->asrc_out0_int_thres = 0;
		asrc_reg->asrc_out1_int_thres = 0;
		asrc_reg->asrc_int_pd = 6;
		asrc_reg->asrc_int_en = 0;

		irq_disable(IRQ_ID_ASRC);
		asrc_irq_callabck = NULL;
	}

	return 0;
}

int phy_reset_asrc_fifo(asrc_type_e type, u8_t reset_w, u8_t reset_r)
{
    int w = 0;
    int r = 0;
    int asrc_ctl = 0;
    int asrc_fifo_state = 0;
    int asrc_ctl_addr = 0;

    #define RESET_W_FIFO_BIT BIT(11)
    #define RESET_R_FIFO_BIT BIT(12)

    if(reset_w){
        w = RESET_W_FIFO_BIT;
    }

    if(reset_r){
        r = RESET_R_FIFO_BIT;
    }

    if(type == ASRC_OUT_0){
        asrc_ctl_addr = ASRC_OUT0_CTL;
	}else if (type == ASRC_OUT_1){
        asrc_ctl_addr = ASRC_OUT1_CTL;
	}else if(type == ASRC_IN_0){
        asrc_ctl_addr = ASRC_IN_CTL;
    }else if(type == ASRC_IN_1){
        asrc_ctl_addr = ASRC_IN1_CTL;
    }else{
        printk("%s fifo type err %d\n",__func__,type);
        return -1;
    }


    asrc_ctl = sys_read32(asrc_ctl_addr);
    asrc_fifo_state = asrc_ctl &(RESET_W_FIFO_BIT|RESET_R_FIFO_BIT);
    asrc_ctl &= ~(w|r);
    sys_write32(asrc_ctl,asrc_ctl_addr);
    delay(10);

    asrc_ctl = sys_read32(asrc_ctl_addr);
    asrc_ctl &= ~(RESET_W_FIFO_BIT|RESET_R_FIFO_BIT);
    asrc_ctl |= asrc_fifo_state;

    sys_write32(asrc_ctl,asrc_ctl_addr);
    printk("%s asrc_type %d w %d r %d\n",__func__,type, reset_w, reset_r);

    return 0;
}


