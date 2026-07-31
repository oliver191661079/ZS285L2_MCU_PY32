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
 * Description:    audio dac physical layer
 *
 * Change log:
 *	2018/5/18: Created by mike.
 ****************************************************************************
 */

#include "../audio_inner.h"

#include <audio_out.h>


#define EN_VOL_GRADUL_CHANGE

#define VOL_CHANGE_GRADE                                (0x01)
#define VOL_DELAY_MSEC                                  (1)

#define CLEAR_PENDING_DELAY_USEC                        (100)

static volatile uint32_t dac_fifo0_cnt_of;
static volatile uint32_t dac_fifo1_cnt_of;

/**************************************************************
**	Description:	enable internal dac digital part
**	Input:      param
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int enable_dac_digital(struct device *dev, PHY_audio_out_param_t *param)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_dac *dac_reg = cfg->dac;

	// dac digital enable
	dac_reg->digctl = (dac_reg->digctl & ~(DAC_DIGCTL_SDMNDTH_MASK)) |
			  DAC_DIGCTL_DDEN |
			  DAC_DIGCTL_SDMREEN |	    // reset SDM after has detect noise: enable
			  DAC_DIGCTL_SDMNDTH_15BIT; // SDM noise detection threshold: -78 dbFS

	// zero data for dac analog part:  disable
	dac_reg->anactl &= (~DAC_ANACTL_ZERODT);

	if (AOUT_FIFO_DAC0 == param->aout_fifo_sel) {
		//dac fifo0 input set
		dac_reg->fifoctl &= (~DAC_FIFOCTL_DAF0IS_MASK);
		dac_reg->fifoctl |= ((param->aout_fifo_src_type - AOUT_FIFO_SRC_CPU) << DAC_FIFOCTL_DAF0IS_SHIFT);

		if(AOUT_FIFO_SRC_DMA == param->aout_fifo_src_type) {
			dac_reg->fifoctl |= DAC_FIFOCTL_DAF0EDE;
		}

		// dac fifo0 reset
		dac_reg->fifoctl &= (~DAC_FIFOCTL_DAF0RT);
		dac_reg->fifoctl |= DAC_FIFOCTL_DAF0RT;

		// dac fifo0 mix to dac/i2stx/spdiftx
		dac_reg->digctl |= DAC_DIGCTL_DAF0M2DAEN;

	} else if(AOUT_FIFO_DAC1 == param->aout_fifo_sel) {
		//dac fifo1 input set
		dac_reg->fifoctl &= (~DAC_FIFOCTL_DAF1IS_MASK);
		dac_reg->fifoctl |= ((param->aout_fifo_src_type - AOUT_FIFO_SRC_CPU) << DAC_FIFOCTL_DAF1IS_SHIFT);

		if(AOUT_FIFO_SRC_DMA == param->aout_fifo_src_type) {
			dac_reg->fifoctl |= DAC_FIFOCTL_DAF1EDE;
		}

		// dac fifo1 reset
		dac_reg->fifoctl &= (~DAC_FIFOCTL_DAF1RT);
		dac_reg->fifoctl |= DAC_FIFOCTL_DAF1RT;

		// dac fifo1 mix to dac/i2stx/spdiftx
		dac_reg->digctl |= DAC_DIGCTL_DAF1M2DAEN;

	} else {
		SYS_LOG_INF("%d, dac fifo select error!", __LINE__);
		return -1;
	}

	return 0;
}

/**************************************************************
**	Description:	enable internal dac analog part
**	Input:      param
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int enable_dac_analog(struct device *dev, PHY_audio_out_param_t *param)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_dac *dac_reg = cfg->dac;

	//bias / pa / pa_os enble
	dac_reg->anactl |= (DAC_ANACTL_BIASEN | DAC_ANACTL_PAOSENL | DAC_ANACTL_PAOSENR | DAC_ANACTL_PAENL | DAC_ANACTL_PAENR);

	if(param->ptr_pa_setting != NULL) {
		//left right channel set
		if (0 == param->ptr_pa_setting->left_mute_flag) {
			dac_reg->anactl |= DAC_ANACTL_DAENL;
		} else {
			dac_reg->anactl &= (~DAC_ANACTL_DAENL);
		}

		if (0 == param->ptr_pa_setting->right_mute_flag) {
			dac_reg->anactl |= DAC_ANACTL_DAENR;
		} else {
			dac_reg->anactl &= (~DAC_ANACTL_DAENR);
		}
	} else {
		//left right channel set
		//dac_reg->anactl |= (DAC_ANACTL_DAENL | DAC_ANACTL_DAENR);

		// use config value
		#ifndef CONFIG_AUDIO_OUT_ACTS_L_MUTE
		dac_reg->anactl |= DAC_ANACTL_DAENL;
		#else
		dac_reg->anactl &= (~DAC_ANACTL_DAENL);
		#endif

		#ifndef CONFIG_AUDIO_OUT_ACTS_R_MUTE
		dac_reg->anactl |= DAC_ANACTL_DAENR;
		#else
		dac_reg->anactl &= (~DAC_ANACTL_DAENR);
		#endif

	}

	#if (CONFIG_AUDIO_OUT_SE_DF == 1)
	dac_reg->anactl |= (0x0f << DAC_ANACTL_LPANEN_SHIFT);
	#else
	dac_reg->anactl &= (~(0x0f << DAC_ANACTL_LPANEN_SHIFT));
	#endif

	#if (CONFIG_AUDIO_OUT_DD_MODE == 1)
	dac_reg->anactl |= DAC_ANACTL_OPVROEN;
	#else
	dac_reg->anactl &= (~DAC_ANACTL_OPVROEN);
	#endif

	//DAC playback connect
	dac_reg->anactl |= DAC_ANACTL_DPBM;

	return 0;
}

/**************************************************************
**	Description:	get current dac sample rate , unit is KHz
**	Input:
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int PHY_audio_get_dac_samplerate(void)
{
	uint32_t temp_dac_clkdiv, temp_dac_prediv, temp_apll;
	int ret = 0;
	uint8_t use_audiopll_index = 0xff;
	const unsigned char samplerate_list_prediv_1[] =
	{ 192, 96, 64, 48, 32, 24, 16, 176, 88, 0xff, 44, 0xff, 22, 0xff};
	const unsigned char samplerate_list_prediv_2[] =
	{ 96, 48, 32, 24, 16, 12, 8, 88, 44, 0xff, 22, 0xff, 11, 0xff};

	temp_dac_clkdiv = (sys_read32(CMU_ADDACLK) & CMU_ADDACLK_DACCLKDIV_MASK);
	temp_dac_prediv = (sys_read32(CMU_ADDACLK) & (1 << CMU_ADDACLK_DACCLKPREDIV));
	if(temp_dac_prediv != 0) {
		temp_dac_prediv = 1;
	}

	if((sys_read32(CMU_ADDACLK) & (1 << CMU_ADDACLK_DACCLKSRC)) != 0) {
		// dac clock use audio pll 1
		use_audiopll_index = 1;
	} else {
		// dac clock use audio pll 0
		use_audiopll_index = 0;
	}

	if(use_audiopll_index == 0) {
		//get audio pll rate
		temp_apll = sys_read32(AUDIO_PLL0_CTL);
		temp_apll &= CMU_AUDIOPLL0_CTL_APS0_MASK;
	} else if(use_audiopll_index == 1) {
		//get audio pll rate
		temp_apll = sys_read32(AUDIO_PLL1_CTL);
		temp_apll &= CMU_AUDIOPLL1_CTL_APS1_MASK;
	} else {
		return -1;
	}

	if(temp_apll >= 8) {
		// 48k series
		if(temp_dac_prediv == 1) {
			ret = (int)samplerate_list_prediv_2[temp_dac_clkdiv];
		} else {
			ret = (int)samplerate_list_prediv_1[temp_dac_clkdiv];
		}
	} else {
		// 44.1k series
		if(temp_dac_prediv == 1) {
			ret = (int)samplerate_list_prediv_2[temp_dac_clkdiv+7];
		} else {
			ret = (int)samplerate_list_prediv_1[temp_dac_clkdiv+7];
		}
	}

	if(ret == 0xff) {
		return -1;
	}

	return ret;

}

/****************************************************************
* Description:	Set dac gain
* Input:      device / gain
* Output:
* Return:    success/fail
* Note:
*   Set dac level to the target level directly to seave time.
*   Shold be used before DAC is open.
*****************************************************************/
int PHY_audio_set_dac_gain_fast(struct device *dev, uint32_t vol_pa_da)
{
	uint32_t swing_val, cur_da, cur_pa;
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_dac *dac_reg = cfg->dac;

	// pa volume
	uint32_t vol_pa = vol_pa_da >> 8;
	// da volume
	uint32_t vol_da = vol_pa_da & 0x00ff;

	//dac clock gating enable
	acts_clock_peripheral_enable(CLOCK_ID_AUDIO_DAC);

	swing_val = (dac_reg->vol & PA_VOLUME_PALRSW);

	cur_pa = (dac_reg->vol & 0x3f);

	dac_reg->vol = (((uint8_t) vol_pa << PA_VOLUME_PALRVOL_SHIFT) | PA_VOLUME_PALRZCDEN \
			| PA_VOLUME_PALRTOEN | swing_val);

	SYS_LOG_INF("old pa vol:0x%x new pa vol:0x%x", cur_pa, vol_pa);

	cur_da = (dac_reg->vol_lch & 0x00ff);

	SYS_LOG_INF("new da vol:0x%x", vol_da);

	dac_reg->vol_lch = (dac_reg->vol_lch & (~0xff)) | (uint8_t) vol_da;
	dac_reg->vol_rch = (dac_reg->vol_rch & (~0xff)) | (uint8_t) vol_da;

	if ((dac_reg->fifoctl & (DAC_FIFOCTL_DAF0RT | DAC_FIFOCTL_DAF1RT)) == 0) {
		//dac clock gating disable
		acts_clock_peripheral_disable(CLOCK_ID_AUDIO_DAC);
	}

	return 0;

}

/**************************************************************
**	Description:	Set dac gain
**	Input:      device / gain
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int PHY_audio_set_dac_gain(struct device *dev, uint32_t vol_pa_da)
{
	uint32_t swing_val, cur_da, cur_pa;
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_dac *dac_reg = cfg->dac;

	// pa volume
	uint32_t vol_pa = vol_pa_da >> 8;
	// da volume
	uint32_t vol_da = vol_pa_da & 0x00ff;

	//dac clock gating enable
	acts_clock_peripheral_enable(CLOCK_ID_AUDIO_DAC);

	swing_val = (dac_reg->vol & PA_VOLUME_PALRSW);

	cur_pa = (dac_reg->vol & 0x3f);

	SYS_LOG_INF("pa 0x%x->0x%x, dac_vol:0x%x", cur_pa, vol_pa, dac_reg->vol);

#ifdef EN_VOL_GRADUL_CHANGE
    do {
	    if (cur_pa < vol_pa) {
		// UP
		if (vol_pa - cur_pa < VOL_CHANGE_GRADE || cur_pa == 0) {
		    dac_reg->vol = (((uint8_t) vol_pa << PA_VOLUME_PALRVOL_SHIFT) | PA_VOLUME_PALRZCDEN \
				    | PA_VOLUME_PALRTOEN | swing_val);
		    cur_pa = vol_pa;
		}else{
		    cur_pa += VOL_CHANGE_GRADE;
		    dac_reg->vol = (((uint8_t) cur_pa << PA_VOLUME_PALRVOL_SHIFT) | PA_VOLUME_PALRZCDEN \
				    | PA_VOLUME_PALRTOEN | swing_val);
		    k_sleep(VOL_DELAY_MSEC);
		}
	    }else if(cur_pa > vol_pa){
		// DOWN
		if (cur_pa - vol_pa < VOL_CHANGE_GRADE || cur_pa == 0) {
		    dac_reg->vol = (((uint8_t) vol_pa << PA_VOLUME_PALRVOL_SHIFT) | PA_VOLUME_PALRZCDEN \
				    | PA_VOLUME_PALRTOEN | swing_val);
		    cur_pa = vol_pa;
		}else{
		    cur_pa -= VOL_CHANGE_GRADE;
		    dac_reg->vol = (((uint8_t) cur_pa << PA_VOLUME_PALRVOL_SHIFT) | PA_VOLUME_PALRZCDEN \
				    | PA_VOLUME_PALRTOEN | swing_val);
		    k_sleep(VOL_DELAY_MSEC);
		}
	    }
	}while (vol_pa != cur_pa);
#else
	dac_reg->vol = (((uint8_t) vol_pa << PA_VOLUME_PALRVOL_SHIFT) | PA_VOLUME_PALRZCDEN \
			| PA_VOLUME_PALRTOEN | swing_val);
#endif

	cur_da = (dac_reg->vol_lch & 0x00ff);

	SYS_LOG_INF("da 0x%x->0x%x, vol_l/r:0x%x,0x%x", cur_da, vol_da, dac_reg->vol_lch, dac_reg->vol_rch);

#ifdef EN_VOL_GRADUL_CHANGE
	do
	{
		SYS_LOG_DBG("%d, cur_da: 0x%x;	vol_da: 0x%x\n", __LINE__, cur_da, vol_da);
		if (cur_da < vol_da) {
			// UP
			if ((vol_da - cur_da < VOL_CHANGE_GRADE) || (cur_da == 0)) {
				dac_reg->vol_lch = (dac_reg->vol_lch & (~0xff)) | (uint8_t) vol_da | VOL_RCH_VOLRZCEN;
				dac_reg->vol_rch = (dac_reg->vol_rch & (~0xff)) | (uint8_t) vol_da | VOL_RCH_VOLRZCEN;
				cur_da = vol_da;
			} else {
				cur_da += VOL_CHANGE_GRADE;
				dac_reg->vol_lch = (dac_reg->vol_lch & (~0xff)) | (uint8_t) cur_da | VOL_RCH_VOLRZCEN;
				dac_reg->vol_rch = (dac_reg->vol_rch & (~0xff)) | (uint8_t) cur_da | VOL_RCH_VOLRZCEN;
				k_sleep(VOL_DELAY_MSEC);
			}
		} else if (cur_da > vol_da) {
			// DOWN
			if ((cur_da - vol_da < VOL_CHANGE_GRADE) || (vol_da == 0)) {
				dac_reg->vol_lch = (dac_reg->vol_lch & (~0xff)) | (uint8_t) vol_da | VOL_RCH_VOLRZCEN;
				dac_reg->vol_rch = (dac_reg->vol_rch & (~0xff)) | (uint8_t) vol_da | VOL_RCH_VOLRZCEN;
				cur_da = vol_da;
			} else {
				cur_da -= VOL_CHANGE_GRADE;
				dac_reg->vol_lch = (dac_reg->vol_lch & (~0xff)) | (uint8_t) cur_da | VOL_RCH_VOLRZCEN;
				dac_reg->vol_rch = (dac_reg->vol_rch & (~0xff)) | (uint8_t) cur_da | VOL_RCH_VOLRZCEN;
				k_sleep(VOL_DELAY_MSEC);
			}
		}
	}while (cur_da != vol_da);
#else
	dac_reg->vol_lch = (dac_reg->vol_lch & (~0xff)) | (uint8_t) vol_da;
	dac_reg->vol_rch = (dac_reg->vol_rch & (~0xff)) | (uint8_t) vol_da;
#endif

	if ((dac_reg->fifoctl & (DAC_FIFOCTL_DAF0RT | DAC_FIFOCTL_DAF1RT)) == 0) {
		//dac clock gating disable
		acts_clock_peripheral_disable(CLOCK_ID_AUDIO_DAC);
	}

	return 0;

}

/**************************************************************
**	Description:	get current dac gain
**	Input:
**	Output:
**	Return:    pa/da gain
**	Note:
***************************************************************
**/
int PHY_audio_get_dac_gain(struct device *dev)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_dac *dac_reg = cfg->dac;

	int vol_src;

	//dac clock gating enable
	acts_clock_peripheral_enable(CLOCK_ID_AUDIO_DAC);

	vol_src = ((dac_reg->vol & PA_VOLUME_PALRVOL_MASK) << 8) ;
	vol_src |= (dac_reg->vol_lch & 0xff);

	if ((dac_reg->fifoctl & (DAC_FIFOCTL_DAF0RT | DAC_FIFOCTL_DAF1RT)) == 0) {
		//dac clock gating disable
		acts_clock_peripheral_disable(CLOCK_ID_AUDIO_DAC);
	}

	return vol_src;
}


/**************************************************************
**	Description:	dac isr
**	Input:      device structure
**	Output:
**	Return:
**	Note:
***************************************************************
**/
void audio_out_dac_isr(void* arg)
{
	struct device *dev = arg;
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_dac *dac_reg = cfg->dac;

	SYS_LOG_DBG("dat0cnt:0x%x dat1cnt:0x%x", dac_reg->dat0cnt, dac_reg->dat1cnt);

	// DAC FIFO 0 sample count overflow
	if((dac_reg->dat0cnt & DAC_FIFO0_CNT_IP) != 0) {
		dac_fifo0_cnt_of++;

		k_busy_wait(CLEAR_PENDING_DELAY_USEC);

		dac_reg->dat0cnt |= DAC_FIFO0_CNT_IP;

	}

	// DAC FIFO 1 sample count overflow
	if((dac_reg->dat1cnt & DAC_FIFO1_CNT_IP) != 0) {
		dac_fifo1_cnt_of++;

		k_busy_wait(CLEAR_PENDING_DELAY_USEC);

		dac_reg->dat1cnt |= DAC_FIFO1_CNT_IP;

	} else {
		;    // other pending
	}
}

/**************************************************************
**	Description:	disable dac irq
**	Input:      device structure
**	Output:
**	Return:
**	Note:
***************************************************************
**/
static void audio_out_dac_irq_disable(struct device *dev)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_dac *dac_reg = cfg->dac;

	if(((dac_reg->dat0cnt & (DAC_FIFO0_CNT_IE)) == 0) && ((dac_reg->dat1cnt & (DAC_FIFO1_CNT_IE)) == 0) && \
		((dac_reg->fifoctl & ((DAC_FIFOCTL_DAF0EIE) | (DAC_FIFOCTL_DAF1EIE))) == 0)) {
		irq_disable(IRQ_ID_AUDIO_DAC);
	}
}

/**************************************************************
**	Description:	config DAC FIFO sample count function
**	Input:      device structure / channel node / config mode
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int PHY_audio_config_fifo_cnt(struct device *dev, aout_channel_node_t *ptr_channel, sample_cnt_cmd_e mode)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_dac *dac_reg = cfg->dac;
	u32_t cnt;
	SYS_IRQ_FLAGS flags;

	if(ptr_channel == NULL) {
		return -1;
	}

	switch(mode) {
		case CMD_SAMPLECNT_ENABLE:
			//enable FIFO count
			if(AOUT_FIFO_DAC0 == ptr_channel->channel_fifo_sel) {
				dac_reg->dat0cnt |= (DAC_FIFO0_CNT_EN | DAC_FIFO0_CNT_IE);
				dac_fifo0_cnt_of = 0;
			} else if(AOUT_FIFO_DAC1 == ptr_channel->channel_fifo_sel) {
				dac_reg->dat1cnt |= (DAC_FIFO1_CNT_EN | DAC_FIFO1_CNT_IE);
				dac_fifo1_cnt_of  = 0;
			} else {
				SYS_LOG_ERR("%d, fifo count enable error!\n", __LINE__);
				return -1;
			}

			ptr_channel->fifo_cnt_enable = 1;

			audio_out_dac_irq_enable();
			break;

		case CMD_SAMPLECNT_DISABLE:
			// disable FIFO count
			if(AOUT_FIFO_DAC0 == ptr_channel->channel_fifo_sel) {
				dac_reg->dat0cnt &= (~(DAC_FIFO0_CNT_EN | DAC_FIFO0_CNT_IE));
				dac_fifo0_cnt_of = 0;
			} else if(AOUT_FIFO_DAC1 == ptr_channel->channel_fifo_sel) {
				dac_reg->dat1cnt &= (~(DAC_FIFO1_CNT_EN | DAC_FIFO1_CNT_IE));
				dac_fifo1_cnt_of  = 0;
			} else {
				SYS_LOG_ERR("%d, fifo count disable error!\n", __LINE__);
				return -1;
			}

			ptr_channel->fifo_cnt_enable = 0;

			audio_out_dac_irq_disable(dev);
			break;

		case CMD_SAMPLECNT_RESET:
			// reset FIFO count
			if(AOUT_FIFO_DAC0 == ptr_channel->channel_fifo_sel) {
				dac_reg->dat0cnt &= (~(DAC_FIFO0_CNT_EN | DAC_FIFO0_CNT_IE));
				dac_reg->dat0cnt |= (DAC_FIFO0_CNT_EN | DAC_FIFO0_CNT_IE);
				dac_fifo0_cnt_of = 0;
			} else if(AOUT_FIFO_DAC1 == ptr_channel->channel_fifo_sel) {
				dac_reg->dat1cnt &= (~(DAC_FIFO1_CNT_EN | DAC_FIFO1_CNT_IE));
				dac_reg->dat1cnt |= (DAC_FIFO1_CNT_EN | DAC_FIFO1_CNT_IE);
				dac_fifo1_cnt_of = 0;
			} else {
				SYS_LOG_ERR("%d, fifo count reset error!\n", __LINE__);
				return -1;
			}

			ptr_channel->fifo_cnt_enable = 1;

			audio_out_dac_irq_enable();
			break;

		case CMD_SAMPLECNT_GETCNT:
			// get total count
			if(AOUT_FIFO_DAC0 == ptr_channel->channel_fifo_sel) {
				sys_irq_lock(&flags);
				cnt = DAC_FIFO0_CNT_GET(dac_fifo0_cnt_of, dac_reg->dat0cnt);
				sys_irq_unlock(&flags);
				//unsigned to int
				return cnt & (0x7FFFFFFF);
			} else if (AOUT_FIFO_DAC1 == ptr_channel->channel_fifo_sel) {
				sys_irq_lock(&flags);
				cnt = DAC_FIFO1_CNT_GET(dac_fifo1_cnt_of, dac_reg->dat1cnt);
				sys_irq_unlock(&flags);
				//unsigned to int
				return cnt & (0x7FFFFFFF);
			} else {
				SYS_LOG_ERR("%d, fifo count get error!\n", __LINE__);
				return -1;
			}

			break;

		default:
			SYS_LOG_INF("%d, invalid cmd!\n", __LINE__);
			break;

	}

	return 0;

}


/**************************************************************
**	Description:	enable internal dac
**	Input:      device structure / audio param / channel node
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int PHY_audio_enable_dac(struct device *dev, PHY_audio_out_param_t *aout_param, aout_channel_node_t *ptr_channel)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_dac *dac_reg = cfg->dac;

	if((aout_param == NULL) || (aout_param->ptr_dac_setting == NULL)) {
		return -EINVAL;
	}

	if ((dac_reg->fifoctl & (DAC_FIFOCTL_DAF0RT | DAC_FIFOCTL_DAF1RT)) == 0) {
		// all dac fifo is disable,  reset dac
		acts_reset_peripheral(RESET_ID_DAC);
	}

	//dac clock gating enable
	acts_clock_peripheral_enable(CLOCK_ID_AUDIO_DAC);

	enable_dac_digital(dev, aout_param);

	PHY_audio_set_dac_gain_fast(dev, aout_param->ptr_dac_setting->dac_gain);
	PHY_audio_set_samplerate(aout_param->ptr_dac_setting->dac_sample_rate, ptr_channel, 0, aout_param->channel_apll_sel);

	enable_dac_analog(dev, aout_param);

	//PHY_audio_set_dac_gain(dev, aout_param->ptr_dac_setting->dac_gain);

	if(aout_param->ptr_dac_setting->sample_cnt_enable != 0) {
		//enable sample count function
		PHY_audio_config_fifo_cnt(dev, ptr_channel, CMD_SAMPLECNT_RESET);
	}

	return 0;
}

/**************************************************************
**	Description:	disable internal dac channel
**	Input:      device structure  / channel
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int PHY_audio_disable_dac(struct device *dev, aout_channel_node_t *ptr_channel)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_dac *dac_reg = cfg->dac;

	uint8_t dac_channel = ptr_channel->channel_fifo_sel;

	// disable dac 0 or dac 1
	if (AOUT_FIFO_DAC0 == dac_channel) {
		//DAC FIFO0 reset
		dac_reg->fifoctl &= (~DAC_FIFOCTL_DAF0RT);
		//fifo0 mix disable
		dac_reg->digctl &= (~DAC_DIGCTL_DAF0M2DAEN);
	}else if(AOUT_FIFO_DAC1 == dac_channel) {
		//DAC FIFO1 reset
		dac_reg->fifoctl &= (~DAC_FIFOCTL_DAF1RT);
		//fifo1 mix disable
		dac_reg->digctl &= (~DAC_DIGCTL_DAF1M2DAEN);
	}else {
		SYS_LOG_ERR("%d, disable dac error!\n", __LINE__);
		return -1;
	}

	if ((dac_reg->fifoctl & (DAC_FIFOCTL_DAF0RT | DAC_FIFOCTL_DAF1RT)) == 0) {
		// all dac is disabled
		dac_reg->anactl |= DAC_ANACTL_ZERODT;
		dac_reg->digctl &= (~DAC_DIGCTL_DDEN);
		// disable fifo counter in any case
		PHY_audio_config_fifo_cnt(dev, ptr_channel, CMD_SAMPLECNT_DISABLE);
		//dac clock gating disable
		acts_clock_peripheral_disable(CLOCK_ID_AUDIO_DAC);
	}

	PHY_audio_unset_samplerate(ptr_channel, 0);

	return 0;
}

/**************************************************************
**	Description:	command and control internal dac channel
**	Input:      device structure  / channel
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int PHY_audio_ioctl_dac(struct device *dev, aout_channel_node_t *ptr_channel, uint32_t cmd, void *param)
{
    const struct acts_audio_out_config *cfg = dev->config->config_info;
    struct acts_audio_dac *dac_reg = cfg->dac;
	int ret = 0;

	switch (cmd) {
		case AOUT_CMD_GET_SAMPLERATE:
		{
			*(uint8_t *)param = PHY_audio_get_dac_samplerate();
			break;
		}
		case AOUT_CMD_SET_SAMPLERATE:
		{
			uint8_t sample_rate = *(uint8_t *)param;
			PHY_audio_unset_samplerate(ptr_channel, 0);
			ret = PHY_audio_set_samplerate(sample_rate, ptr_channel, 0, APLL_ALLOC_AUTO);
			break;
		}
		case AOUT_CMD_OUT_MUTE:
		{
			uint8_t mode = *(uint8_t *)param;
			ret = PHY_audio_out_mute(dev, ptr_channel, mode);
			break;
		}
		case AOUT_CMD_GET_SAMPLE_CNT:
		{
			ret = PHY_audio_config_fifo_cnt(dev, ptr_channel, CMD_SAMPLECNT_GETCNT);
			if (ret < 0) {
				SYS_LOG_ERR("get fifo cnt error:%d", ret);
				return ret;
			} else {
				*(uint32_t *)param = ret;
				ret = 0;
			}
			break;
		}
		case AOUT_CMD_RESET_SAMPLE_CNT:
		{
			ret = PHY_audio_config_fifo_cnt(dev, ptr_channel, CMD_SAMPLECNT_RESET);
			break;
		}
		case AOUT_CMD_ENABLE_SAMPLE_CNT:
		{
			ret = PHY_audio_config_fifo_cnt(dev, ptr_channel, CMD_SAMPLECNT_ENABLE);
			break;
		}
		case AOUT_CMD_DISABLE_SAMPLE_CNT:
		{
			ret = PHY_audio_config_fifo_cnt(dev, ptr_channel, CMD_SAMPLECNT_DISABLE);
			break;
		}
		case AOUT_CMD_GET_VOLUME:
		{
			volume_setting_t *volume_setting = (volume_setting_t *)param;
#ifdef CONFIG_AUDIO_VOLUME_PA
			uint8_t level = PHY_audio_get_dac_gain(dev) & 0xFF00;
			int32_t vol = level >> 8;
#else
			uint8_t level = PHY_audio_get_dac_gain(dev) & 0xFF;
			int32_t vol = dac_volume_level_to_db(level);
#endif
			volume_setting->left_volume = volume_setting->right_volume = vol;
			break;
		}
		case AOUT_CMD_SET_VOLUME:
		{
			uint32_t vol_pa_da;
			volume_setting_t *volume_setting = (volume_setting_t *)param;
#ifdef CONFIG_AUDIO_VOLUME_PA
			vol_pa_da = ((volume_setting->left_volume & 0xFF) << 8) | 0xBF;
#else
			uint8_t da_level;
			da_level = dac_volume_db_to_level(volume_setting->left_volume);
			vol_pa_da = AOUT_DAC_PA_VOLUME_DEFAULT | da_level;
#endif

			ret = PHY_audio_set_dac_gain(dev, vol_pa_da);
			break;
		}
		case AOUT_CMD_GET_CHANNEL_STATUS:
		{
			*(uint8_t *)param = 0;
			uint8_t fifo_status = 0;
			if (AOUT_FIFO_DAC0 == ptr_channel->channel_fifo_sel) {
				fifo_status = (dac_reg->stat & DAC_STAT_DAF0S_MASK) >> DAC_STAT_DAF0S_SHIFT;
			} else if (AOUT_FIFO_DAC1 == ptr_channel->channel_fifo_sel) {
				fifo_status = (dac_reg->stat & DAC_STAT_DAF1S_MASK) >> DAC_STAT_DAF1S_SHIFT;
			}
			if (fifo_status < (AUDIO_DACFIFO0_LEVEL / 2 - 1))
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

