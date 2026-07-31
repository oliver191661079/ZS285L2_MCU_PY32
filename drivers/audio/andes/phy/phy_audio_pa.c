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
 * Description:    PA driver physical layer
 *
 * Change log:
 *	2018/5/22: Created by mike.
 ****************************************************************************
 */

#include "../audio_inner.h"
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_PHY_AUDIO_PA

static void PHY_audio_dac_bias_setting(struct acts_audio_dac *dac_reg)
{
#ifdef CONFIG_SOC_SERIES_ANDESC
	/*
		From caijiegeng@actions-semi.com, same as FT setting.
		Use this bias config for both differential and single-end output.
	*/
	//dac_reg->bias = 0x23345243;

	/* from <chenxujian@actionstech.com>, at Mar. 21 2024
	  在DAC_BIAS=23345243的基础上，建议把DAC_BIAS的bit5:3（OPVROOSIQ）调成3’b011，
	  即DAC_BIAS变为2334525b.
	  这是为了避免使用直驱方式推耳机功能时，可能导致的稳定性问题。
	  Ft采用23345243等效于ft测试会更严格，但是也可能会导致ft的VRO测试项时误宰良品。
	  方案中要使用2334525b
	*/
	dac_reg->bias = 0x2334525b;

#else
#error "TODO"
#endif

}
/**************************************************************
**	Description:	enable pa
**	Input:		param
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int PHY_audio_enable_pa(struct device *dev, phy_pa_setting_t *param, int antipop_enable)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_dac *dac_reg = cfg->dac;

	if ((dac_reg->fifoctl & (DAC_FIFOCTL_DAF0RT | DAC_FIFOCTL_DAF1RT)) == 0) {
		// all dac fifo is disable,  reset dac
		acts_reset_peripheral(RESET_ID_DAC);
	}

	acts_clock_peripheral_enable(CLOCK_ID_AUDIO_DAC);

	//dac lr dither enable
	dac_reg->digctl |= DAC_DIGCTL_ENDITH;

	//Interpolation Filter Rate Select: 8X
	dac_reg->digctl &= (~DAC_DIGCTL_INTFRS_MASK);
	dac_reg->digctl |= (3 << DAC_DIGCTL_INTFRS_SHIFT);

	//DAC Over Sample Rate: 64X
	dac_reg->digctl &= (~(DAC_DIGCTL_OSRDA_MASK));
	dac_reg->digctl |= DAC_DIGCTL_OSRDA_64X;

	//DAC FIFO 0 mix enable
	dac_reg->digctl |= DAC_DIGCTL_DAF0M2DAEN;

	//DAC Digital enable
	dac_reg->digctl |= DAC_DIGCTL_DDEN;

	//for pa channel param set
	if (param->left_mute_flag == 0) {
		//left not mute
		dac_reg->anactl |= DAC_ANACTL_DAENL;
	} else {
		//left mute
		dac_reg->anactl &= ~DAC_ANACTL_DAENL;
	}

	if (param->right_mute_flag == 0) {
		//right not mute
		dac_reg->anactl |= DAC_ANACTL_DAENR;
	} else {
		//right mute
		dac_reg->anactl &= ~DAC_ANACTL_DAENR;
	}

	//open 过零检测 和 过零检测超时
	dac_reg->vol |= (PA_VOLUME_PALRZCDEN | PA_VOLUME_PALRTOEN);

	//配置out swing
	dac_reg->vol &= (~PA_VOLUME_PALRSW);
#if (CONFIG_AUDIO_PA_SWING == 1)
	dac_reg->vol |= PA_VOLUME_PALRSW;
#endif

	// init pa volume to 0
	dac_reg->vol &= (~PA_VOLUME_PALRVOL_MASK);


	PHY_audio_dac_bias_setting(dac_reg);

	/*
	from <chenxujian@actionstech.com>, at Mar. 21 2024
	  DAC_ANACTL的bit17:16（PAIQ）建议直接默认调节为2’b11，避免使用PA的时候，由于外部环境导致PA震荡
	  在FT时，不配置bit17:16，等效于ft测试更严格。方案上建议使用2’b11.
	*/
	dac_reg->anactl |= DAC_ANACTL_PAIQ(0x3);

	//pa enable
	dac_reg->anactl |= (DAC_ANACTL_PAENL | DAC_ANACTL_PAENR);

	//all bias enble
	dac_reg->anactl |= DAC_ANACTL_BIASEN;

#ifdef CONFIG_AUDIO_SOFT_ANTIPOP_ENABLE
	if(antipop_enable){
		audio_device_enable_pa_antipop(dev);
	}
#endif

#if ((CONFIG_AUDIO_OUT_DD_MODE == 1) || (CONFIG_AUDIO_OUT_SE_DF == 0))
	// ?????,??antipop  ??
	dac_reg->anactl |= DAC_ANACTL_DACINVL | DAC_ANACTL_DACINVR;
#endif

#if (CONFIG_AUDIO_OUT_SE_DF == 1)
	//??????
	dac_reg->anactl |= (0x0f << DAC_ANACTL_LPANEN_SHIFT);
#endif

#if (CONFIG_AUDIO_OUT_DD_MODE == 1)
	//dac vro enable direct drive
	dac_reg->anactl |= DAC_ANACTL_OPVROEN;
#endif

	dac_reg->vol_lch = (dac_reg->vol_lch & (~0xff)) | 0xbf | VOL_RCH_VOLRZCEN;
	dac_reg->vol_rch = (dac_reg->vol_rch & (~0xff)) | 0xbf | VOL_RCH_VOLRZCEN;

	dac_reg->anactl |= (DAC_ANACTL_ZERODT);

#if ((CONFIG_AUDIO_OUT_DD_MODE != 0) || (CONFIG_AUDIO_OUT_SE_DF != 0))
	dac_reg->anactl |= (DAC_ANACTL_PAOSENL | DAC_ANACTL_PAOSENR);
#endif

	dac_reg->fifoctl &= ~DAC_FIFOCTL_DAF0RT;
	dac_reg->digctl &= ~DAC_DIGCTL_DAF0M2DAEN;
	dac_reg->digctl &= ~DAC_DIGCTL_DDEN;

	return 0;
}

/**************************************************************
**	Description:	disable pa
**	Input:		audio out device
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int PHY_audio_disable_pa(struct device *dev, int antipop_enable)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_dac *dac_reg = cfg->dac;

	acts_clock_peripheral_enable(CLOCK_ID_AUDIO_DAC);

	//dac lr dither enable
	dac_reg->digctl |= DAC_DIGCTL_ENDITH;

	//Interpolation Filter Rate Select: 8X
	dac_reg->digctl &= (~DAC_DIGCTL_INTFRS_MASK);
	dac_reg->digctl |= (3 << DAC_DIGCTL_INTFRS_SHIFT);

	//DAC Over Sample Rate: 64X
	dac_reg->digctl &= (~(DAC_DIGCTL_OSRDA_MASK));
	dac_reg->digctl |= DAC_DIGCTL_OSRDA_64X;

	//DAC FIFO 0 mix enable
	dac_reg->digctl |= DAC_DIGCTL_DAF0M2DAEN;

	//DAC Digital enable
	dac_reg->digctl |= DAC_DIGCTL_DDEN;

	//set volume zero
	dac_reg->vol &= (~PA_VOLUME_PALRVOL_MASK);

	//dac playback mute / pa output stage disable / pa disable
	dac_reg->anactl &= (~(DAC_ANACTL_DPBM | DAC_ANACTL_PAOSENL | DAC_ANACTL_PAOSENR | DAC_ANACTL_PAENL | DAC_ANACTL_PAENR));

	//pa anti-pop loop2 enable / pa anti-pop discharge enable
	dac_reg->anactl |= (DAC_ANACTL_ATPLP2 | DAC_ANACTL_PAVDC);

#ifdef CONFIG_AUDIO_SOFT_ANTIPOP_ENABLE
	if(antipop_enable){
		audio_device_disable_pa_antipop(dev);
	}
#endif

#if ((CONFIG_AUDIO_OUT_DD_MODE == 1) || (CONFIG_AUDIO_OUT_SE_DF == 0))
	// ?????,??antipop  ??
	dac_reg->anactl &= (~(DAC_ANACTL_DACINVL | DAC_ANACTL_DACINVR));
#endif

	//Zero data for DAC analog part disable
	dac_reg->anactl &= (~DAC_ANACTL_ZERODT);

	dac_reg->vol_lch = (dac_reg->vol_lch & (~0xff)) | 0xbf;
	dac_reg->vol_rch = (dac_reg->vol_rch & (~0xff)) | 0xbf;

	dac_reg->anactl &= (~(DAC_ANACTL_PAVDC | DAC_ANACTL_ATPLP2));
	dac_reg->anactl &= (~(DAC_ANACTL_DAENL | DAC_ANACTL_DAENR | DAC_ANACTL_BIASEN | DAC_ANACTL_OPVROEN));

	dac_reg->fifoctl &= ~DAC_FIFOCTL_DAF0RT;
	dac_reg->digctl &= ~DAC_DIGCTL_DAF0M2DAEN;
	dac_reg->digctl &= ~DAC_DIGCTL_DDEN;

	//dac clock gating disable
	acts_clock_peripheral_disable(CLOCK_ID_AUDIO_DAC);

	return 0;
}

/**************************************************************
**	Description:	 aoutl / aoutr  mute function
**	Input:		audio out device / mute param
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int PHY_audio_pa_mute(struct device *dev, pa_mute_setting_t *pa_mute_param)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_dac *dac_reg = cfg->dac;

	//for pa channel param set
	if (pa_mute_param->left_mute_flag == 0) {
		//left enable
		dac_reg->anactl |= DAC_ANACTL_DAENL;
	} else {
		//left mute
		dac_reg->anactl &= (~DAC_ANACTL_DAENL);
	}

	if (pa_mute_param->right_mute_flag == 0) {
		//right enable
		dac_reg->anactl |= DAC_ANACTL_DAENR;
	} else {
		//right mute
		dac_reg->anactl &= (~DAC_ANACTL_DAENR);
	}

	return 0;
}


/**************************************************************
**	Description:	 mute or unmute dac channel 0 or dac channel 1
**	Input:		dev / channel / mode
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int PHY_audio_out_mute(struct device *dev, aout_channel_node_t *channel_node, uint8_t mode)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_dac *dac_reg = cfg->dac;

	if(channel_node->channel_fifo_sel > AOUT_FIFO_DAC1) {
		SYS_LOG_INF("%d, channel fifo is not dac fifo!", __LINE__);
		return -1;
	}

	if(mode == 0) {
		// unmute
		if(channel_node->channel_fifo_sel == AOUT_FIFO_DAC0) {
			dac_reg->digctl |= (DAC_DIGCTL_DAF0M2DAEN);
		} else if(channel_node->channel_fifo_sel == AOUT_FIFO_DAC1){
			dac_reg->digctl |= (DAC_DIGCTL_DAF1M2DAEN);
		} else {
			return -1;
		}
	} else {
		// mute
		if(channel_node->channel_fifo_sel == AOUT_FIFO_DAC0) {
			dac_reg->digctl &= (~DAC_DIGCTL_DAF0M2DAEN);
		} else if(channel_node->channel_fifo_sel == AOUT_FIFO_DAC1){
			dac_reg->digctl &= (~DAC_DIGCTL_DAF1M2DAEN);
		} else {
			return -1;
		}
	}

	return 0;
}


