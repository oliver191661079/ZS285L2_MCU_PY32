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
 * Description:    ADC  driver  physical layer
 *
 * Change log:
 *	2018/5/18: Created by mike.
 ****************************************************************************
 */


#include "../audio_inner.h"
#include <audio_in.h>

#define MAX_ADC_GAIN    (0x0f)
#define MAX_DMIC_GAIN	(3)

#define INVALID_ADC_BIAS_VALUE (0)
#define IS_VALID_ADC_BIAS_CONFIG(x) ((x) != 0)

/**************************************************************
**	Description:	disable all input op
**	Input:      device structure
**	Output:
**	Return:
**	Note:
***************************************************************
**/
static void PHY_audio_disable_all_op(struct device *dev)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_adc *adc_reg = cfg->adc;

	//disable ain op0
	adc_reg->anactl &= (~(ADC_ANACTL_OP0LEN | ADC_ANACTL_OP0REN));
	//disable ain op1
	adc_reg->anactl &= (~(ADC_ANACTL_OP1LEN | ADC_ANACTL_OP1REN));

	//disable ain op2
	adc_reg->bias &= (~(ADC_BIAS_OP2LEN | ADC_BIAS_OP2REN));

	//disable ain micop
	adc_reg->anactl &= (~(ADC_ANACTL_MOPLEN | ADC_ANACTL_MOPREN));

	//aux in fd disable
	adc_reg->anactl &= ~ADC_ANACTL_AUXINFDENLH;
	adc_reg->bias &= ~ADC_BIAS_AUXINFDENRH;

}


/**************************************************************
**	Description:	enable internal adc
**	Input:      device structure  / audio in param / channel node
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int PHY_audio_enable_adc(struct device *dev, PHY_audio_in_param_t *setting, ain_channel_node_t *ain_channel_node)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_adc *adc_reg = cfg->adc;
	phy_adc_setting_t *adc_param;
	ain_track_e track_flag;
	ain_source_type_e ain_source;

	if((setting == NULL) || (ain_channel_node == NULL)) {
		return -1;
	}

	if(setting->ptr_adc_setting == NULL) {
		return -1;
	}

	adc_param = setting->ptr_adc_setting;
	track_flag = setting->ptr_input_setting->ain_track;
	ain_source = setting->ain_src;

	SYS_LOG_INF("src=%d, track=%d\n", ain_source, track_flag);

	//enable adc clock
	acts_clock_peripheral_enable(CLOCK_ID_AUDIO_ADC);

	//adc fifo output select
	adc_reg->fifoctl &= (~ADC_FIFOCTL_ADFOS_MASK);
	adc_reg->fifoctl |= ((setting->ain_fifo_dst - AIN_FIFO_DST_CPU) << ADC_FIFOCTL_ADFOS_SHIFT);

	//adc fifo full drq enable
	adc_reg->fifoctl |= ADC_FIFOCTL_ADFFDE;

	//adc fifo input adc
	adc_reg->fifoctl &= (~ADC_FIFOCTL_ADFIS_MASK);
	if(setting->ain_channel_type == AIN_CHANNEL_I2SRX0) {
		adc_reg->fifoctl |= ADC_FIFOCTL_ADFIS(1);
	}

	PHY_audio_set_adc_gain(dev, adc_param->adc_gain);

	//adc lpf enable
	adc_reg->anactl |= ADC_ANACTL_ADLPFEN;

#ifdef CONFIG_SOC_SERIES_ANDES
	//adc bias config
	adc_reg->bias &= ~(ADC_BIAS_VRDABC_MASK | ADC_BIAS_OPBC23_MASK | ADC_BIAS_OPBC1_MASK
			   | ADC_BIAS_OP1BC_MASK | ADC_BIAS_OP0BC_MASK | ADC_BIAS_FD1BUFBC_MASK
			   | ADC_BIAS_FD1BC_MASK | ADC_BIAS_LPFBUFBC_MASK | ADC_BIAS_LPFBC_MASK);
	//根据SD 提供的建议值修改
	if((setting->ain_src == AIN_SOURCE_ASEMIC) || (setting->ain_src == AIN_SOURCE_AFDMIC)) {
		// MICOP
		adc_reg->bias |= 0x20400041;
	} else {
		adc_reg->bias |= 0x20004041;  //0x20000041->0x20004041 以提升大信号下的THD+N
	}
#endif

	/*这里为了减少HPF建立至95%的直流偏置消减时间，将采样率设置到最高后再延时一小段时间再设置回实际采样率*/
	if(adc_param->adc_sample_rate/16 == 0){
		PHY_audio_set_samplerate(192, ain_channel_node, 1, setting->channel_apll_sel);
		PHY_audio_unset_samplerate(ain_channel_node, 1);
		k_sleep(10);
	}
	else if(adc_param->adc_sample_rate/44 == 0){
		PHY_audio_set_samplerate(176, ain_channel_node, 1, setting->channel_apll_sel);
		PHY_audio_unset_samplerate(ain_channel_node, 1);
		k_sleep(10);
	}
	else {
		/*TODO 其他采样率设置*/
	}

	//adc_d high pass filter enable
	adc_reg->digctl |= (ADC_DIGCTL_HPFREN | ADC_DIGCTL_HPFLEN);

	//adc left right channel enable
	//adc_reg->anactl |= (ADC_ANACTL_ADLEN | ADC_ANACTL_ADREN);

	//ADC µÄ×óÓÒÍ¨µÀÊ¹ÄÜ,  ¸ù¾ÝÅäÖÃµÄÊµ¼ÊÍ¨µÀ½øÐÐÉè¶¨

#if 0
    if(ain_source == AIN_SOURCE_AUXFD) {
		adc_reg->anactl |= (ADC_ANACTL_ADLEN | ADC_ANACTL_ADREN);
    } else if(ain_source == AIN_SOURCE_AFDMIC) {
    	adc_reg->anactl |= ADC_ANACTL_ADLEN;
    } else if(track_flag == INPUTSRC_ONLY_L) {
    	adc_reg->anactl |= ADC_ANACTL_ADLEN;
    } else if(track_flag == INPUTSRC_ONLY_R) {
    	adc_reg->anactl |= ADC_ANACTL_ADREN;
    } else {
    	adc_reg->anactl |= (ADC_ANACTL_ADLEN | ADC_ANACTL_ADREN);
    }
#else
    // aux ²î·ÖÖ§³Öµ¥ÉùµÀÊäÈë;   mic  ²î·ÖÓÉÉÏ²ãÅäÖÃ¾ßÌåÉùµÀ
    if((ain_source == AIN_SOURCE_AFDMIC) && (track_flag == INPUTSRC_L_R)) {
	    adc_reg->anactl |= ADC_ANACTL_ADLEN;             // ²î·Ömic Ö»Ö§³Öµ¥ÉùµÀ£¬ÈçÅäÖÃ´íÎó£¬Ä¬ÈÏÊ¹ÓÃ×óÉùµÀ
    } else if(track_flag == INPUTSRC_ONLY_L) {
	    adc_reg->anactl |= ADC_ANACTL_ADLEN;
    } else if(track_flag == INPUTSRC_ONLY_R) {
	    adc_reg->anactl |= ADC_ANACTL_ADREN;
    } else {
	    adc_reg->anactl |= (ADC_ANACTL_ADLEN | ADC_ANACTL_ADREN);
    }

#endif

	//adc fifo reset & enable
	adc_reg->fifoctl &= (~ADC_FIFOCTL_ADFRT);
	adc_reg->fifoctl |= ADC_FIFOCTL_ADFRT;

	//k_busy_wait(10*1000);

	PHY_audio_set_samplerate(adc_param->adc_sample_rate, ain_channel_node, 1, setting->channel_apll_sel);

	// 现有IC上，HPF建立至95%的直流偏置消减时间，约100ms，延时100ms，来规避录音初始的噪声
	// k_sleep(100);

	//adc fifo reset & enable
	adc_reg->fifoctl &= (~ADC_FIFOCTL_ADFRT);
	adc_reg->fifoctl |= ADC_FIFOCTL_ADFRT;

	return 0;
}

/**************************************************************
**	Description:	disable internal adc
**	Input:      device structure
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int PHY_audio_disable_adc(struct device *dev, ain_channel_node_t *ain_channel_node)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_adc *adc_reg = cfg->adc;

	//adc_d high pass filter disable
	adc_reg->digctl &= (~(ADC_DIGCTL_HPFREN | ADC_DIGCTL_HPFLEN));

	//disable left/right channel
	adc_reg->anactl &= (~(ADC_ANACTL_ADLEN | ADC_ANACTL_ADREN));

	// adc lpf disable
	adc_reg->anactl &= (~ADC_ANACTL_ADLPFEN);

	//reset adc fifo
	adc_reg->fifoctl &= (~ADC_FIFOCTL_ADFRT);

	//disable the VRDA for adc
	adc_reg->anactl &= (~ADC_ANACTL_ADVREN);

	PHY_audio_unset_samplerate(ain_channel_node, 1);

	return 0;
}

/**************************************************************
**	Description:	enable dmic
**	Input:      device structure  / audio in param / channel node
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int PHY_audio_enable_dmic(struct device *dev, PHY_audio_in_param_t *setting, ain_channel_node_t *ain_channel_node)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_adc *adc_reg = cfg->adc;
	phy_adc_setting_t *adc_param;
	ain_track_e track_flag;

	if((setting == NULL) || (ain_channel_node == NULL)) {
		return -1;
	}

	if(setting->ptr_adc_setting == NULL) {
		return -1;
	}

	adc_param = setting->ptr_adc_setting;
	track_flag = setting->ptr_input_setting->ain_track;

	if (AIN_SOURCE_DMIC != setting->ain_src) {
		SYS_LOG_ERR("Invalid channel type %d", setting->ain_src);
		return -1;
	}

	acts_clock_peripheral_enable(CLOCK_ID_AUDIO_ADC);

	//enable dmic clock
	acts_clock_peripheral_enable(CLOCK_ID_DMIC);

	// make sure there is not any analog audio enable
	adc_reg->anactl &= (~(ADC_ANACTL_ADLEN | ADC_ANACTL_ADREN));

	//adc fifo output select
	adc_reg->fifoctl &= (~ADC_FIFOCTL_ADFOS_MASK);
	adc_reg->fifoctl |= ((setting->ain_fifo_dst - AIN_FIFO_DST_CPU) << ADC_FIFOCTL_ADFOS_SHIFT);

	//adc fifo full drq enable
	adc_reg->fifoctl |= ADC_FIFOCTL_ADFFDE;

	//adc fifo input adc
	adc_reg->fifoctl &= (~ADC_FIFOCTL_ADFIS_MASK);
	adc_reg->fifoctl |= ADC_FIFOCTL_ADFIS(0);

#ifdef CONFIG_DMIC_LR_CHANNEL_EXCHANGE
	adc_reg->dmicctl |= DMIC_CTL_DRFS;
#endif

	PHY_audio_set_dmic_gain(dev, setting->ptr_input_setting->input_gain);
	PHY_audio_set_adc_gain(dev, adc_param->adc_gain);

	//adc_d high pass filter enable
	adc_reg->digctl |= (ADC_DIGCTL_HPFREN | ADC_DIGCTL_HPFLEN);

	if(track_flag == INPUTSRC_ONLY_L) {
	    adc_reg->dmicctl |= DMIC_CTL_DMLEN;
    } else if(track_flag == INPUTSRC_ONLY_R) {
	    adc_reg->dmicctl |= DMIC_CTL_DMREN;
    } else {
	    adc_reg->dmicctl |= (DMIC_CTL_DMREN | DMIC_CTL_DMLEN);
    }

	//adc fifo reset & enable
	adc_reg->fifoctl &= (~ADC_FIFOCTL_ADFRT);
	adc_reg->fifoctl |= ADC_FIFOCTL_ADFRT;

	PHY_audio_set_samplerate(adc_param->adc_sample_rate, ain_channel_node, 1, setting->channel_apll_sel);

	//adc fifo reset & enable
	adc_reg->fifoctl &= (~ADC_FIFOCTL_ADFRT);
	adc_reg->fifoctl |= ADC_FIFOCTL_ADFRT;

	SYS_LOG_DBG("dmic control register: 0x%08x", adc_reg->dmicctl);

	return 0;
}

/**************************************************************
**	Description:	disable dmic
**	Input:      device structure
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int PHY_audio_disable_dmic(struct device *dev)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_adc *adc_reg = cfg->adc;

	//adc_d high pass filter disable
	adc_reg->digctl &= (~(ADC_DIGCTL_HPFREN | ADC_DIGCTL_HPFLEN));

	//disable DMIC left/right channel
	adc_reg->dmicctl &= (~(DMIC_CTL_DMREN | DMIC_CTL_DMLEN));

	//reset adc fifo
	adc_reg->fifoctl &= (~ADC_FIFOCTL_ADFRT);

	return 0;
}

/**************************************************************
**	Description:	set internal dmic gain
**	Input:      device structure  / gain
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int PHY_audio_set_dmic_gain(struct device *dev, uint32_t gain_value)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_adc *adc_reg = cfg->adc;

	if(gain_value > MAX_DMIC_GAIN)
		gain_value = MAX_DMIC_GAIN;

	//dmic gain set
	adc_reg->dmicctl &= (~DMIC_CTL_PREGAIN_MASK);
	adc_reg->dmicctl |= DMIC_CTL_PREGAIN(gain_value);

	return 0;
}

/**************************************************************
**	Description:	set internal adc gain
**	Input:      device structure  / gain
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int PHY_audio_set_adc_gain(struct device *dev, uint32_t gain_value)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_adc *adc_reg = cfg->adc;

	if(gain_value > MAX_ADC_GAIN) {
		return -EINVAL;
	}

	//adc digital gain set
	adc_reg->digctl &= (~ADC_DIGCTL_ADCGC_MASK);
	adc_reg->digctl |= (gain_value << ADC_DIGCTL_ADCGC_SHIFT);

	return 0;
}

/**************************************************************
**	Description:	set analog in source gain
**	Input:      device structure  / input source / gain
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int PHY_audio_set_inputsrc_gain(struct device *dev, ain_source_type_e inputsrc, uint32_t gain_value)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_adc *adc_reg = cfg->adc;

	if (inputsrc == AIN_SOURCE_AUX0) {
		//ain op0 gain set
		adc_reg->anactl &= (~ADC_ANACTL_OP0G_MASK);
		adc_reg->anactl |= (gain_value << ADC_ANACTL_OP0G_SHIFT);
	} else if (inputsrc == AIN_SOURCE_AUX1) {
		//ain op1 gain set
		adc_reg->anactl &= (~ADC_ANACTL_OP1G_MASK);
		adc_reg->anactl |= (gain_value << ADC_ANACTL_OP1G_SHIFT);
	} else if ((inputsrc == AIN_SOURCE_AFDMIC) || (inputsrc == AIN_SOURCE_ASEMIC)) {
		//ain micop gain set
		adc_reg->anactl &= (~ADC_ANACTL_MOPG_MASK);
		adc_reg->anactl |= (gain_value << ADC_ANACTL_MOPG_SHIFT);
	} else if ((inputsrc == AIN_SOURCE_AFDMIC_AUXFD) || (inputsrc == AIN_SOURCE_ASEMIC_AUXFD)) {
        //set micop gain
        adc_reg->anactl &= (~ADC_ANACTL_MOPG_MASK);
        adc_reg->anactl |= ((gain_value & 0xff) << ADC_ANACTL_MOPG_SHIFT);

		// set aux diff boost 6db
		if(((gain_value >> 8) & 0xff) != 0)
			adc_reg->anactl |= ADC_ANACTL_AUXFDBTENH;
		else
			adc_reg->anactl &= (~ADC_ANACTL_AUXFDBTENH);
	} else if ((inputsrc == AIN_SOURCE_AFDMIC_AUX2) || (inputsrc == AIN_SOURCE_ASEMIC_AUX2)) {
        //set micop gain
        adc_reg->anactl &= (~ADC_ANACTL_MOPG_MASK);
        adc_reg->anactl |= ((gain_value & 0x7) << ADC_ANACTL_MOPG_SHIFT);

		// set aux2 gain which the same as aux0
		if(((gain_value >> 8) & 0x7) != 0) {
			adc_reg->anactl &= (~ADC_ANACTL_OP0G_MASK);
			adc_reg->anactl |= (((gain_value >> 8) & 0x7) << ADC_ANACTL_OP0G_SHIFT);
		}
	} else {
		; //do nothing
	}

	return 0;
}


/**************************************************************
**	Description:	enable analog in source
**	Input:      device structure  / audio in param
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int PHY_audio_enable_inputsrc(struct device *dev, PHY_audio_in_param_t *setting)
{
	uint32_t input_gain;
	uint16_t boost;
	ain_track_e track_flag;
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_adc *adc_reg = cfg->adc;

	if((setting == NULL) || (setting->ptr_input_setting == NULL)) {
		SYS_LOG_ERR("%d, enable inputsrc fail!\n", __LINE__);
		return -EINVAL;
	}

	input_gain = setting->ptr_input_setting->input_gain;
	track_flag = setting->ptr_input_setting->ain_track;
	boost = setting->ptr_input_setting->boost;

	SYS_LOG_INF("src=%d, track=%d, gain=%d, boost=%d\n", setting->ain_src, track_flag, input_gain, boost);
	acts_clock_peripheral_enable(CLOCK_ID_AUDIO_ADC);

	if (setting->ain_src <= AIN_SOURCE_DMIC) {
		//vrda enable
		adc_reg->anactl |= ADC_ANACTL_ADVREN;
	}

#ifdef CONFIG_SOC_SERIES_ANDESC
	/* From caijiegeng@actions-semi.com, same as FT setting.  */
	adc_reg->bias = 0x88445069;
#endif

	PHY_audio_disable_all_op(dev);

	if (setting->ptr_input_setting->input_gain_force_0db)
		adc_reg->anactl |= ADC_ANACTL_MOPGTEN;

	switch(setting->ain_src) {
		// enable ain op 0
		case AIN_SOURCE_AUX0:
			if(track_flag == INPUTSRC_ONLY_L) {
				adc_reg->anactl |= ADC_ANACTL_OP0LEN;
			} else if(track_flag == INPUTSRC_ONLY_R) {
				adc_reg->anactl |= ADC_ANACTL_OP0REN;
			} else {
				adc_reg->anactl |= (ADC_ANACTL_OP0LEN | ADC_ANACTL_OP0REN);
			}

        	//set ain op0 gain
        	adc_reg->anactl &= (~ADC_ANACTL_OP0G_MASK);
			adc_reg->anactl |= (input_gain << ADC_ANACTL_OP0G_SHIFT);
#ifdef CONFIG_SOC_SERIES_ANDESC
			if (IS_VALID_ADC_BIAS_CONFIG(CONFIG_AUDIO_ADC_AUX_BIAS))
				adc_reg->bias = CONFIG_AUDIO_ADC_AUX_BIAS;
#endif
			break;

		// enable ain op 1
		case AIN_SOURCE_AUX1:
			if(track_flag == INPUTSRC_ONLY_L) {
				adc_reg->anactl |= ADC_ANACTL_OP1LEN;
			} else if(track_flag == INPUTSRC_ONLY_R) {
				adc_reg->anactl |= ADC_ANACTL_OP1REN;
			} else {
				adc_reg->anactl |= (ADC_ANACTL_OP1LEN | ADC_ANACTL_OP1REN);
			}

			//set ain op1 gain
        	adc_reg->anactl &= (~ADC_ANACTL_OP1G_MASK);
			adc_reg->anactl |= (input_gain << ADC_ANACTL_OP1G_SHIFT);
#ifdef CONFIG_SOC_SERIES_ANDESC
			if (IS_VALID_ADC_BIAS_CONFIG(CONFIG_AUDIO_ADC_AUX_BIAS))
				adc_reg->bias = CONFIG_AUDIO_ADC_AUX_BIAS;
#endif

			break;

		// enable ain op 2
		case AIN_SOURCE_AUX2:
			if(track_flag == INPUTSRC_ONLY_L) {
				adc_reg->bias |= ADC_BIAS_OP2LEN;
			} else if(track_flag == INPUTSRC_ONLY_R) {
				adc_reg->bias |= ADC_BIAS_OP2REN;
			} else {
				adc_reg->bias |= (ADC_BIAS_OP2LEN | ADC_BIAS_OP2REN);
			}
			//set ain op2 gain, the same to op0
        	adc_reg->anactl &= (~ADC_ANACTL_OP0G_MASK);
			adc_reg->anactl |= (input_gain << ADC_ANACTL_OP0G_SHIFT);

			// AUX2 use default BIAS

			break;

		// enable ain op mic
		case AIN_SOURCE_ASEMIC:
		case AIN_SOURCE_AFDMIC:
			if(setting->ain_src == AIN_SOURCE_AFDMIC) {
				adc_reg->anactl |= ADC_ANACTL_MOPLEN;
			} else {
				if(track_flag == INPUTSRC_ONLY_L) {
					adc_reg->anactl |= ADC_ANACTL_MOPLEN;
				} else if(track_flag == INPUTSRC_ONLY_R) {
					adc_reg->anactl |= ADC_ANACTL_MOPREN;
				} else {
					adc_reg->anactl |= (ADC_ANACTL_MOPLEN | ADC_ANACTL_MOPREN);
				}
			}

			//set micop gain
			adc_reg->anactl &= (~ADC_ANACTL_MOPG_MASK);
			adc_reg->anactl |= (input_gain << ADC_ANACTL_MOPG_SHIFT);


			if (setting->ain_src == AIN_SOURCE_AFDMIC) {
				//enable differencial mode
				adc_reg->anactl &= (~ADC_ANACTL_MFDSES);
			} else {
				//micop SE enable
				adc_reg->anactl |= ADC_ANACTL_MFDSES;
			}

			//if(boost)
			//	adc_reg->anactl |= ADC_ANACTL_MOPGBEN;
#ifdef CONFIG_SOC_SERIES_ANDESC
			if (IS_VALID_ADC_BIAS_CONFIG(CONFIG_AUDIO_ADC_MIC_BIAS))
				adc_reg->bias = CONFIG_AUDIO_ADC_MIC_BIAS;
#endif
			break;

		// enable ain op fd
		case AIN_SOURCE_AUXFD:
			//aux in fd enable
			adc_reg->anactl |= ADC_ANACTL_AUXINFDENLH;
			adc_reg->bias |= ADC_BIAS_AUXINFDENRH;
			//lpf enable
			adc_reg->anactl |= ADC_ANACTL_ADLPFEN;

			// aux differential input only support 2 level gain
			adc_reg->anactl &= (~ADC_ANACTL_AUXFDBTENH);
			if(input_gain != 0) {
				// boost 6db
				adc_reg->anactl |= ADC_ANACTL_AUXFDBTENH;
			}
#ifdef CONFIG_SOC_SERIES_ANDESC
			if (IS_VALID_ADC_BIAS_CONFIG(CONFIG_AUDIO_ADC_AUX_BIAS))
				adc_reg->bias = CONFIG_AUDIO_ADC_AUX_BIAS;
#endif
			break;

	    case AIN_SOURCE_AFDMIC_AUXFD:
		case AIN_SOURCE_ASEMIC_AUXFD:
	        adc_reg->anactl |= ADC_ANACTL_MOPLEN;

			//set micop gain
			adc_reg->anactl &= (~ADC_ANACTL_MOPG_MASK);
			adc_reg->anactl |= ((input_gain & 0xff) << ADC_ANACTL_MOPG_SHIFT);

			if (setting->ain_src == AIN_SOURCE_AFDMIC_AUXFD)
				adc_reg->anactl &= (~ADC_ANACTL_MFDSES); //enable differencial mode
			else
				adc_reg->anactl |= ADC_ANACTL_MFDSES; //micop SE enable

			//aux in fd enable
			adc_reg->anactl |= ADC_ANACTL_AUXINFDENLH;
			adc_reg->bias |= ADC_BIAS_AUXINFDENRH;
			//lpf enable
			adc_reg->anactl |= ADC_ANACTL_ADLPFEN;

			// aux differential input only support 2 level gain
			adc_reg->anactl &= (~ADC_ANACTL_AUXFDBTENH);
			if(((input_gain >> 8) & 0xff) != 0) {
				// boost 6db
				adc_reg->anactl |= ADC_ANACTL_AUXFDBTENH;
			}

#ifdef CONFIG_AUDIO_VOICE_HARDWARE_REFERENCE
#ifdef CONFIG_AUDIO_VOICE_HARDWARE_REFERENCE_INNER
        		adc_reg->anactl |= ADC_ANACTL_PALMADR;
#else
        		adc_reg->bias |= ADC_BIAS_OP2REN;
        		adc_reg->anactl &= (~ADC_ANACTL_OP0G_MASK);
        		adc_reg->anactl |= (0x07 << ADC_ANACTL_OP0G_SHIFT);
#endif
#endif
            break;

		case AIN_SOURCE_AFDMIC_AUX2:
		case AIN_SOURCE_ASEMIC_AUX2:

	        adc_reg->anactl |= ADC_ANACTL_MOPLEN;

			//set micop gain
			adc_reg->anactl &= (~ADC_ANACTL_MOPG_MASK);
			adc_reg->anactl |= ((input_gain & 0xff) << ADC_ANACTL_MOPG_SHIFT);

			if (setting->ain_src == AIN_SOURCE_AFDMIC_AUX2)
				adc_reg->anactl &= (~ADC_ANACTL_MFDSES); //enable differencial mode
			else
				adc_reg->anactl |= ADC_ANACTL_MFDSES; //micop SE enable

#ifdef CONFIG_AUDIO_VOICE_HARDWARE_REFERENCE_INNER
			adc_reg->anactl |= ADC_ANACTL_PALMADR;
#else
			adc_reg->bias |= ADC_BIAS_OP2REN;
			adc_reg->anactl &= (~ADC_ANACTL_OP0G_MASK);
			adc_reg->anactl |= (((input_gain >> 8) & 0x7) << ADC_ANACTL_OP0G_SHIFT);
#endif

			break;

		default:
			SYS_LOG_ERR("%d, ain source error %d \n", __LINE__, setting->ain_src);
			return -1;
			break;

	}

	return 0;
}


/**************************************************************
**	Description:	disable analog in source
**	Input:      device structure  / audio in channel node
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int PHY_audio_disable_inputsrc(struct device *dev, ain_channel_node_t *channel_node)
{
	const struct acts_audio_in_config *cfg = dev->config->config_info;
	struct acts_audio_adc *adc_reg = cfg->adc;

	if(channel_node == NULL) {
		SYS_LOG_ERR("%d, ain disable input fail!\n", __LINE__);
		return -EINVAL;
	}

	switch(channel_node->channel_src_type) {
		// disable ain op 0
		case AIN_SOURCE_AUX0:
			adc_reg->anactl &= (~(ADC_ANACTL_OP0LEN | ADC_ANACTL_OP0REN));
			break;

		// disable ain op 1
		case AIN_SOURCE_AUX1:
			adc_reg->anactl &= (~(ADC_ANACTL_OP1LEN | ADC_ANACTL_OP1REN));
			break;

		// disable ain op 2
		case AIN_SOURCE_AUX2:
			adc_reg->bias &= (~(ADC_BIAS_OP2LEN | ADC_BIAS_OP2REN));
			break;

		// disable ain op mic
		case AIN_SOURCE_ASEMIC:
		case AIN_SOURCE_AFDMIC:
			adc_reg->anactl &= (~(ADC_ANACTL_MOPLEN | ADC_ANACTL_MOPREN));
			break;

		// disable ain op fd
		case AIN_SOURCE_AUXFD:
			adc_reg->anactl &= ~ADC_ANACTL_AUXINFDENLH;
			adc_reg->bias &= ~ADC_BIAS_AUXINFDENRH;
			break;

		case AIN_SOURCE_AFDMIC_AUXFD:
		case AIN_SOURCE_ASEMIC_AUXFD:
			adc_reg->anactl &= (~(ADC_ANACTL_MOPLEN | ADC_ANACTL_MOPREN));
			adc_reg->anactl &= ~ADC_ANACTL_AUXINFDENLH;
			adc_reg->bias &= ~ADC_BIAS_AUXINFDENRH;
#ifdef CONFIG_AUDIO_VOICE_HARDWARE_REFERENCE
#ifdef CONFIG_AUDIO_VOICE_HARDWARE_REFERENCE_INNER
			adc_reg->anactl &= ~ADC_ANACTL_PALMADR;
#else
			adc_reg->bias &= (~(ADC_BIAS_OP2LEN | ADC_BIAS_OP2REN));
#endif
#endif
            break;

		case AIN_SOURCE_AFDMIC_AUX2:
		case AIN_SOURCE_ASEMIC_AUX2:
			adc_reg->anactl &= (~(ADC_ANACTL_MOPLEN | ADC_ANACTL_MOPREN));
			adc_reg->anactl &= ~ADC_ANACTL_AUXINFDENLH;
#ifdef CONFIG_AUDIO_VOICE_HARDWARE_REFERENCE_INNER
			adc_reg->anactl &= ~ADC_ANACTL_PALMADR;
#else
			adc_reg->bias &= (~(ADC_BIAS_OP2LEN | ADC_BIAS_OP2REN));
#endif
			break;
		default:
			SYS_LOG_ERR("%d, ain source error %d \n", __LINE__, channel_node->channel_src_type);
			return -1;
			break;

	}

	return 0;
}

/**************************************************************
**	Description:	command and control internal adc channel
**	Input:      device structure  / channel
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int PHY_audio_ioctl_adc(struct device *dev, ain_channel_node_t *ptr_channel, uint32_t cmd, void *param)
{
	int ret = 0;

	SYS_LOG_INF("source:%d, type:%d, cmd:%d", ptr_channel->channel_src_type, ptr_channel->channel_type, cmd);

	switch (cmd) {
		case AIN_CMD_SET_ADC_GAIN:
		{
			adc_gain *gain_setting = (adc_gain *)param;
			uint32_t gain = gain_setting->ch_gain[0];
			uint16_t input_gain = 0, digital_gain = 0;


			if (!ptr_channel) {
				SYS_LOG_ERR("invalid channel node");
				return -EINVAL;
			}

			uint8_t physical_dev = ptr_channel->channel_src_type;

			if ((physical_dev == AIN_SOURCE_AUX0) || (physical_dev == AIN_SOURCE_AUX1)
				|| (physical_dev == AIN_SOURCE_AUX2)) {
				ret = adc_aux_se_gain_translate(gain, &input_gain, &digital_gain);
				if (ret)
					return ret;
			} else if ((physical_dev == AIN_SOURCE_ASEMIC) || (physical_dev == AIN_SOURCE_AFDMIC)) {
				ret = adc_amic_gain_translate(gain, &input_gain, &digital_gain);
				if (ret)
					return ret;
			} else if (physical_dev == AIN_SOURCE_AUXFD) {
				ret = adc_aux_fd_gain_translate(gain, &input_gain, &digital_gain);
				if (ret)
					return ret;
			} else if ((physical_dev == AIN_SOURCE_AFDMIC_AUXFD) || (physical_dev == AIN_SOURCE_ASEMIC_AUXFD)) {
				ret = adc_amic_gain_translate(gain, &input_gain, &digital_gain);
				if (ret)
					return ret;

				uint16_t _input_gain=0, _digital_gain=0;
				gain = gain_setting->ch_gain[1];
				if (gain != ADC_GAIN_INVALID) {
					ret = adc_aux_fd_gain_translate(gain, &_input_gain, &_digital_gain);
					if (ret)
						return ret;
				}

				if (_input_gain)
					input_gain |= (1 << 8);

			} else if (physical_dev == AIN_SOURCE_DMIC) {
				ret = adc_dmic_gain_translate(gain, &input_gain, &digital_gain);
				if (ret)
					return ret;
			}

			if (ptr_channel->channel_type == AIN_CHANNEL_DMIC) {
				ret = PHY_audio_set_dmic_gain(dev, input_gain);
				ret |= PHY_audio_set_adc_gain(dev, digital_gain);
			} else if (ptr_channel->channel_type == AIN_CHANNEL_AA) {
				ret = PHY_audio_set_inputsrc_gain(dev, ptr_channel->channel_src_type, input_gain);
			} else if (ptr_channel->channel_type == AIN_CHANNEL_ADC) {
				ret = PHY_audio_set_inputsrc_gain(dev, ptr_channel->channel_src_type, input_gain);
				ret |= PHY_audio_set_adc_gain(dev, digital_gain);
			} else {
				SYS_LOG_ERR("Invalid channel type:%d", ptr_channel->channel_type);
				return -EINVAL;
			}
			break;
		}
		case AIN_CMD_SET_SAMPLERATE:
		{
			uint8_t val = *(uint8_t *)param;
			PHY_audio_unset_samplerate(ptr_channel, 1);
			ret = PHY_audio_set_samplerate(val, ptr_channel, 1, APLL_ALLOC_AUTO);;
			if (ret) {
				SYS_LOG_ERR("Failed to set ADC sample rate err=%d", ret);
				return ret;
			}
			break;
		}
		case AIN_CMD_ADC_AEC_GAIN:
		{
#if defined(CONFIG_AUDIO_VOICE_HARDWARE_REFERENCE) && !defined(CONFIG_AUDIO_VOICE_HARDWARE_REFERENCE_INNER)
			int16_t gain = *(int16_t *)param;
			uint8_t level;
			const struct acts_audio_in_config *cfg = dev->config->config_info;
			struct acts_audio_adc *adc_reg = cfg->adc;

			if (gain < -30) {
				level = 0;
			} else if (gain < 0) {
				level = 1;
			} else {
				level = ((gain + 14) / 15) + 2;
			}

			if (level > 7) {
				SYS_LOG_ERR("invalid aec gain:%d", gain);
				return -EINVAL;
			}

    		adc_reg->anactl &= (~ADC_ANACTL_OP0G_MASK);
    		adc_reg->anactl |= (level << ADC_ANACTL_OP0G_SHIFT);
#endif
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

