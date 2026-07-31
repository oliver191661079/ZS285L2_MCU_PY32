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
 * Description:    audio aa physical layer 
 *
 * Change log:
 *	2018/10/24: Created by mike.
 ****************************************************************************
 */

#include "../audio_inner.h"
#include <audio_out.h>

/**************************************************************
**	Description:	enable internal dac analog part
**	Input:      param 
**	Output:
**	Return:    success/fail
**	Note: 
***************************************************************
**/
int enable_aa_analog(struct device *dev, PHY_audio_out_param_t *param)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_dac *dac_reg = cfg->dac;
	aa_source_e source;

	source = param->ptr_pa_setting->aa_mode_src;

	//bias / pa / pa_os enble
	dac_reg->anactl |= (DAC_ANACTL_BIASEN | DAC_ANACTL_PAOSENL | DAC_ANACTL_PAOSENR | DAC_ANACTL_PAENL | DAC_ANACTL_PAENR);

#if (CONFIG_AUDIO_OUT_SE_DF == 1)
	// 差分输出
	dac_reg->anactl |= (0x0f << DAC_ANACTL_LPANEN_SHIFT);

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

	//DAC playback connect
	dac_reg->anactl |= DAC_ANACTL_DPBM;		
#else	
	// 单端输出
	dac_reg->anactl &= (~(0x0f << DAC_ANACTL_LPANEN_SHIFT));
	if((source == AA_SOURCE_AUX0) || (source == AA_SOURCE_AUX2)) {
		// OP0 to PA not mute
		dac_reg->anactl |= DAC_ANACTL_OP0PAMT;
	} else if(source == AA_SOURCE_AUX1) {
		// OP1 to PA not mute
		dac_reg->anactl |= DAC_ANACTL_OP1PAMT;
	} else if(source == AA_SOURCE_ASEMIC) {    //((source == AA_SOURCE_ASEMIC) || (source == AA_SOURCE_AFDMIC)) {
		// MICOP to PA not mute 
		dac_reg->anactl |= DAC_ANACTL_MOPPAMT;
	} else if((source == AA_SOURCE_AUXFD) || (source == AA_SOURCE_AFDMIC)) {
		// ain to dac connect
		dac_reg->anactl |= DAC_ANACTL_AIN2DMT;
		dac_reg->anactl |= DAC_ANACTL_DPBM;    //bug,when fdop aa, play backmute should be enable
	} else {
		;	//empty
	}
#endif


#if (CONFIG_AUDIO_OUT_DD_MODE == 1)
	//直驱输出
	dac_reg->anactl |= DAC_ANACTL_OPVROEN;
#else
	//非直驱输出
	dac_reg->anactl &= (~DAC_ANACTL_OPVROEN);
#endif

	dac_reg->anactl &= (~DAC_ANACTL_ZERODT);

	return 0;
}

/**************************************************************
**	Description:	enable aa channel (output)
**	Input:      device structure / audio param / channel node
**	Output:
**	Return:    success/fail
**	Note: 
***************************************************************
**/
int PHY_audio_enable_aa_out(struct device *dev, PHY_audio_out_param_t *aout_param)
{
	struct acts_audio_out_data *data = dev->driver_data;
	aa_source_e source;

	if((aout_param == NULL) || (aout_param->ptr_pa_setting == NULL)) {
		return -EINVAL;
	}

	source = aout_param->ptr_pa_setting->aa_mode_src;
	if((source < AA_SOURCE_AUX0) || (source >= AA_SOURCE_MAX )) {
		return -EINVAL;
	}
	
	if((data->aa_channel_open_flag == 0) && (data->link_dac_i2s_open_flag == 0) && \
		(data->link_dac_spdif_open_flag == 0) && (data->link_dac_i2s_spdif_open_flag == 0) && (data->dac_channel_open_flag == 0)) {
		// audio dac reset 
		acts_reset_peripheral(RESET_ID_DAC);
	}

	//dac clock gating enable
	acts_clock_peripheral_enable(CLOCK_ID_AUDIO_DAC);		
	
	enable_aa_analog(dev, aout_param);

	PHY_audio_set_dac_gain(dev, aout_param->ptr_dac_setting->dac_gain);

	return 0;
}

/**************************************************************
**	Description:	disable aa channel (output) 
**	Input:      device structure  / channel
**	Output:
**	Return:    success/fail
**	Note: 
***************************************************************
**/
int PHY_audio_disable_aa(struct device *dev, aout_channel_node_t *ptr_channel)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;	
	struct acts_audio_dac *dac_reg = cfg->dac;	

	aa_source_e source;

	if((NULL == ptr_channel) || (ptr_channel->channel_type != AOUT_CHANNEL_AA)) {
		SYS_LOG_ERR("%d, disable a-a channel fail!\n", __LINE__);
		return -EINVAL;
	}

	source = ptr_channel->aa_channel_source;
	if((source == AA_SOURCE_AUX0) || (source == AA_SOURCE_AUX2)) {
		// OP0 to PA mute
		dac_reg->anactl &= (~DAC_ANACTL_OP0PAMT);
	} else if(source == AA_SOURCE_AUX1) {
		// OP1 to PA mute
		dac_reg->anactl &= (~DAC_ANACTL_OP1PAMT);
	} else if((source == AA_SOURCE_ASEMIC) || (source == AA_SOURCE_AFDMIC)) {
		// MICOP to PA mute 
		dac_reg->anactl &= (~DAC_ANACTL_MOPPAMT);
	} else if(source == AA_SOURCE_AUXFD) {
		// ain to dac disconnect
		dac_reg->anactl &= (~DAC_ANACTL_AIN2DMT);
	} else {
		SYS_LOG_ERR("%d, disable a-a channel fail!\n", __LINE__);
		return -1;
	}

	return 0;
}

