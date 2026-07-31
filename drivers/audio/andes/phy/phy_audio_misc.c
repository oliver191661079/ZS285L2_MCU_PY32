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
 * Description:    audio physical layer misc code
 *
 * Change log:
 *	2018/5/18: Created by mike.
 ****************************************************************************
 */

#include "../audio_inner.h"
#include "../audio_asrc.h"
#include <stack_backtrace.h>
#include <soc.h>

#define APS_LEVEL_MAX (APS_LEVEL_8 + 1)

struct audio_pll_info
{
	u8_t series;
	atomic_t refcnt;
};

static struct audio_pll_info audiopll_info[2];

struct adc_gain_mapping {
	int16_t gain;
	uint8_t input_gain;
	uint8_t digital_gain;
};

static const struct adc_gain_mapping amic_gain_mapping[] = {
	//<force input gain 0db and digital gain range from 0 to 24db>
	{0, 0, 0},
	{30, 0, 1},
	{60, 0, 2},
	{90, 0, 3},
	{120, 0, 4},
	{150, 0, 5},
	{180, 0, 6},
	{210, 0, 7},
	{240, 0, 8},
	//</force input gain 0db and digital gain range from 0 to 24db>

	{260, 0, 0},
	{290, 0, 1},
	{300, 1, 0},
	{315, 2, 0},
	{320, 0, 2},
	{330, 3, 0},
	{345, 4, 0},
	{350, 0, 3},
	{360, 5, 0},
	{375, 6, 0},
	{380, 3, 2},
	{390, 7, 0},
	{405, 6, 1},
	{420, 7, 1},
	{450, 7, 2},
	{480, 7, 3},
	{510, 7, 4},
	{540, 7, 5},
	{570, 7, 6},
	{600, 7, 7},
	{630, 7, 8},
	{660, 7, 9},
	{690, 7, 10},
	{720, 7, 11},
	{750, 7, 12},
	{780, 7, 13},
	{810, 7, 14},
	{840, 7, 15},
};

static const struct adc_gain_mapping aux_se_gain_mapping[] = {
	{-120, 0, 0},
	{-90, 0, 1},
	{-60, 0, 2},
	{-30, 1, 0},
	{0, 2, 0},
	{15, 3, 0},
	{30, 4, 0},
	{45, 5, 0},
	{60, 6, 0},
	{75, 7, 0},
	{90, 6, 1},
	{105, 7, 1},
	{120, 6, 2},
	{135, 7, 2},
	{165, 7, 3},
	{195, 7, 4},
	{225, 7, 5},
	{255, 7, 6},
	{285, 7, 7},
	{315, 7, 8},
	{345, 7, 9},
	{375, 7, 10},
	{405, 7, 11},
	{435, 7, 12},
	{475, 7, 13},
	{505, 7, 14},
	{535, 7, 15},
};

static const struct adc_gain_mapping aux_fd_gain_mapping[] = {
	{0, 0, 0},
	{60, 1, 0},
	{90, 1, 1},
	{120, 1, 2},
	{150, 1, 3},
	{180, 1, 4},
	{210, 1, 5},
	{240, 1, 6},
	{270, 1, 7},
	{300, 1, 8},
	{330, 1, 9},
	{360, 1, 10},
	{390, 1, 11},
	{420, 1, 12},
	{450, 1, 13},
	{480, 1, 14},
	{510, 1, 15},
};

static const struct adc_gain_mapping dmic_gain_mapping[] = {
	{0, 0, 0},
	{60, 1, 0},
	{120, 2, 0},
	{180, 3, 0},
	{210, 3, 1},
	{240, 3, 2},
	{270, 3, 3},
	{300, 3, 4},
	{330, 3, 5},
	{360, 3, 6},
	{390, 3, 7},
	{420, 3, 8},
	{450, 3, 9},
	{480, 3, 10},
	{510, 3, 11},
	{540, 3, 12},
	{570, 3, 13},
	{600, 3, 14},
	{630, 3, 15},
};

/*
 * 计算公式 new_sr = sr_input * K_ASRC_OUT0_DEC0 / (dec)
 * 反推dec计算公式 dec = sr_input * (K_ASRC_OUT0_DEC0 + offset) / sr_ouput
 * 以输入44.1khz,输出48khz计算则dec = (44100 * K_ASRC_OUT0_DEC0) / 48000 = 963380
 */

static const uint32_t aps_param[] =
{
    //44.205khz 48.115khz
    K_ASRC_OUT0_DEC0 -2500,

    //44.184khz 48.092khz
    K_ASRC_OUT0_DEC0 -2000,

    //44.142khz 48.045khz
    K_ASRC_OUT0_DEC0 -1000,

    //44.1khz   48khz
    K_ASRC_OUT0_DEC0,

    //44.058khz 47.954khz
    K_ASRC_OUT0_DEC0 + 1000,

    //44.016khz 47.909khz
    K_ASRC_OUT0_DEC0 + 2000,

    //43.995khz 47.886khz
    K_ASRC_OUT0_DEC0 + 2500,

    K_ASRC_OUT0_DEC0 + 10000,

    K_ASRC_OUT0_DEC0 - 15000,//K_ASRC_OUT0_DEC0 - 45000,

    K_ASRC_OUT0_DEC0 - 10000,//K_ASRC_OUT0_DEC0 - 25000,

    K_ASRC_OUT0_DEC0 + 10000,//K_ASRC_OUT0_DEC0 + 25000,

    K_ASRC_OUT0_DEC0 + 15000,//K_ASRC_OUT0_DEC0 + 45000,
};

static const uint32_t aps_param_48k[] =
{
    //48.125khz
    963380 -2500,

    //48.100khz
    963380 -2000,

    //48.050khz
    963380 -1000,

    //48khz
    963380,

    //47.950khz
    963380 + 1000,

    //47.901khz
    963380 + 2000,

    //47.875khz
    963380 + 2500,

    963380 + 10000,

    963380 - 15000,//963380 - 45000,

    963380 - 10000,//963380 - 25000,

    963380 + 10000,//963380 + 25000,

    963380 + 15000,//963380 + 45000,
};

uint32_t cal_asrc_rate(uint32_t sr_input, uint32_t sr_output, int32_t aps_level)
{
    uint32_t dec_value = 0;

    if(sr_input == sr_output){
        dec_value = aps_param[aps_level];
    }else if(sr_input == 44 && sr_output == 48){
        dec_value = aps_param_48k[aps_level];
    }
	else{
        dec_value = (uint64_t)get_sample_rate_hz(sr_input) * K_ASRC_OUT0_DEC0 / get_sample_rate_hz(sr_output);
    }

    return dec_value;
}
/**************************************************************
**	Description:	set audio out aps
**	Input:      aps / adjust mode
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int PHY_audio_set_aps(struct device *dev, void *channel_node, int32_t value, int32_t mode, bool is_in)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_asrc *asrc_reg = cfg->asrc;
	uint32_t dec_value;
	uint8_t sr_input, sr_output, channel_asrc_index;
	uint8_t channel_sample_type, channel_apll_sel;

	if (is_in) {
		ain_channel_node_t *ain_channel_node = (ain_channel_node_t *)channel_node;
		sr_input = ain_channel_node->sr_input;
		sr_output = ain_channel_node->sr_output;
		channel_asrc_index = ain_channel_node->channel_asrc_index;
		channel_sample_type = ain_channel_node->channel_sample_type;
		channel_apll_sel = ain_channel_node->channel_apll_sel;
	} else {
		aout_channel_node_t *aout_channel_node = (aout_channel_node_t *)channel_node;
		sr_input = aout_channel_node->sr_input;
		sr_output = aout_channel_node->sr_output;
		channel_asrc_index = aout_channel_node->channel_asrc_index;
		channel_sample_type = aout_channel_node->channel_sample_type;
		channel_apll_sel = aout_channel_node->channel_apll_sel;
	}

	if((mode >= APS_LEVEL_MODE_MAX) || (NULL == channel_node)) {
		SYS_LOG_ERR("%d, set aps error!\n", __LINE__);
		dump_stack();
		return -1;
	}

	if(mode == APS_LEVEL_ASRC) {
		//调节asrc
		if(value == ASRC_LEVEL_SLOWER
            || value == ASRC_LEVEL_SLOWEST
            || value == ASRC_LEVEL_FASTER
            || value == ASRC_LEVEL_FASTEST)
        {
            value = value -  ASRC_LEVEL_SLOWEST + ASRC_MAX_LEVEL;
        }
        else if(value >= ASRC_MAX_LEVEL)
        {
			value = ASRC_LEVEL_DEFAULT;
		}

        dec_value = cal_asrc_rate(sr_input, sr_output, value);

		SYS_LOG_DBG("sr_input:%d sr_output:%d level:%d dec:%d",
				sr_input, sr_output, value, dec_value);

		if(channel_asrc_index == ASRC_OUT_0) {
			asrc_reg->asrc_out0_dec0 = dec_value;
		} else if(channel_asrc_index == ASRC_OUT_1) {
			asrc_reg->asrc_out1_dec0 = dec_value;
		} else if (channel_asrc_index == ASRC_IN_0) {
			asrc_reg->asrc_in_dec0 = dec_value;
		} else if (channel_asrc_index == ASRC_IN_1) {
			asrc_reg->asrc_in1_dec0 = dec_value;
		} else {
			//SYS_LOG_ERR("%d, channel not use asrc!\n", __LINE__);
			return -1;
		}
	} else {
		//调节audiopll
		if(value >= APS_LEVEL_MAX) {
			value = APS_LEVEL_1;
		}

		if(channel_sample_type == SAMPLE_TYPE_48) {
			value += APS_LEVEL_MAX;
		}

 		if(channel_apll_sel == AUDIOPLL_TYPE_0) {
			sys_write32((sys_read32(AUDIO_PLL0_CTL) & (~CMU_AUDIOPLL0_CTL_APS0_MASK)) | (value << CMU_AUDIOPLL0_CTL_APS0_SHIFT), AUDIO_PLL0_CTL);
		} else if(channel_apll_sel == AUDIOPLL_TYPE_1) {
			sys_write32((sys_read32(AUDIO_PLL1_CTL) & (~CMU_AUDIOPLL1_CTL_APS1_MASK)) | (value << CMU_AUDIOPLL1_CTL_APS1_SHIFT), AUDIO_PLL1_CTL);
		} else {
			SYS_LOG_ERR("%d, channel not use audiopll!\n", __LINE__);
			return -1;
		}
	}

    return 0;
}

/**************************************************************
**	Description:	get audio out aps
**	Input:      adjust mode
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int PHY_audio_get_aps(struct device *dev, void *channel_node, int32_t mode, bool is_in)
{
	int i;
	int aps_value;
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_asrc *asrc_reg = cfg->asrc;

	uint8_t sr_input, sr_output, channel_asrc_index;
	uint8_t channel_sample_type, channel_apll_sel;

	if (is_in) {
		ain_channel_node_t *ain_channel_node = (ain_channel_node_t *)channel_node;
		sr_input = ain_channel_node->sr_input;
		sr_output = ain_channel_node->sr_output;
		channel_asrc_index = ain_channel_node->channel_asrc_index;
		channel_sample_type = ain_channel_node->channel_sample_type;
		channel_apll_sel = ain_channel_node->channel_apll_sel;
	} else {
		aout_channel_node_t *aout_channel_node = (aout_channel_node_t *)channel_node;
		sr_input = aout_channel_node->sr_input;
		sr_output = aout_channel_node->sr_output;
		channel_asrc_index = aout_channel_node->channel_asrc_index;
		channel_sample_type = aout_channel_node->channel_sample_type;
		channel_apll_sel = aout_channel_node->channel_apll_sel;
	}

	if(mode == APS_LEVEL_ASRC) {
		// asrc rate
		if(channel_asrc_index == ASRC_OUT_0) {
			aps_value = asrc_reg->asrc_out0_dec0;
		} else if(channel_asrc_index == ASRC_OUT_1) {
			aps_value = asrc_reg->asrc_out1_dec0;
		} else if(channel_asrc_index == ASRC_IN_0) {
			aps_value = asrc_reg->asrc_in_dec0;
		} else if(channel_asrc_index == ASRC_IN_1) {
			aps_value = asrc_reg->asrc_in1_dec0;
		} else {
			SYS_LOG_ERR("%d, channel not use asrc!\n", __LINE__);
			return -1;
		}

		for(i = 0; i < ASRC_MAX_LEVEL; i++) {
		    if(sr_input == sr_output){
    			if(aps_value == aps_param[i]) {
    				break;
    			}
		    }else{
    			if(aps_value == aps_param_48k[i]) {
    				break;
    			}
		    }
		}

		if(i == ASRC_MAX_LEVEL) {
			i = -1;
		}
	}else {
		//audio pll
		if(channel_apll_sel == AUDIOPLL_TYPE_0) {
			i = (sys_read32(AUDIO_PLL0_CTL) & (CMU_AUDIOPLL0_CTL_APS0_MASK));
		}else if(channel_apll_sel == AUDIOPLL_TYPE_1) {
			i = (sys_read32(AUDIO_PLL1_CTL) & (CMU_AUDIOPLL1_CTL_APS1_MASK));
		}else {
			SYS_LOG_ERR("%d, channel not use audiopll!\n", __LINE__);
			return -1;
		}

		if((channel_sample_type == SAMPLE_TYPE_48) && (i >= APS_LEVEL_MAX)) {
			i -= APS_LEVEL_MAX;
		}
	}

	return i;
}


/**************************************************************
**	Description:	check audiopll is enable or not,  and get the series
**	Input:		audiopll index
**	Output:    series
**	Return:    success/fail
**	Note:
***************************************************************
**/
int PHY_check_audiopll(audiopll_type_e index, uint8_t* series)
{
	if(index >= AUDIOPLL_TYPE_MAX) {
		return -1;
	}

	if(index == AUDIOPLL_TYPE_0) {
		// audio pll 0
		if((sys_read32(AUDIO_PLL0_CTL) & (1 << CMU_AUDIOPLL0_CTL_EN)) == 0) {
			// audiopll0 not in use
			*series = 0xff;
		}else if((sys_read32(AUDIO_PLL0_CTL) & CMU_AUDIOPLL0_CTL_APS0_MASK) >= 8) {
			// audiopll0 set 48k series
			*series = 1;
		}else {
			// audiopll0 set 44.1k series
			*series = 0;
		}
	} else {
		// audio pll 1
		if((sys_read32(AUDIO_PLL1_CTL) & (1 << CMU_AUDIOPLL1_CTL_EN)) == 0) {
			// audiopll1 not in use
			*series = 0xff;
		}else if((sys_read32(AUDIO_PLL1_CTL) & CMU_AUDIOPLL1_CTL_APS1_MASK) >= 8) {
			// audiopll1 set 48k series
			*series = 1;
		}else{
			// audiopll1 set 44.1k series
			*series = 0;
		}
	}

	return 0;
}

/**************************************************************
**	Description:	config audio pll
**	Input:		audiopll index / fs series
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int PHY_config_audiopll(uint8_t pll_index, uint8_t fs_series)
{
	if(pll_index == 0) {
		// use audio pll 0
		sys_write32((sys_read32(AUDIO_PLL0_CTL) & (0 << CMU_AUDIOPLL0_CTL_MODE) & (~CMU_AUDIOPLL0_CTL_APS0_MASK)) | (1 << CMU_AUDIOPLL0_CTL_EN), AUDIO_PLL0_CTL);
		if(fs_series == 0) {
			// 44.1k
			sys_write32(sys_read32(AUDIO_PLL0_CTL) | (0x04 << CMU_AUDIOPLL0_CTL_APS0_SHIFT), AUDIO_PLL0_CTL);
		} else {
			// 48k
			sys_write32(sys_read32(AUDIO_PLL0_CTL) | (0x0c << CMU_AUDIOPLL0_CTL_APS0_SHIFT), AUDIO_PLL0_CTL);
		}
	} else {
		// use audio pll 1
		sys_write32((sys_read32(AUDIO_PLL1_CTL) & (0 << CMU_AUDIOPLL1_CTL_MODE) & (~CMU_AUDIOPLL1_CTL_APS1_MASK)) | (1 << CMU_AUDIOPLL1_CTL_EN), AUDIO_PLL1_CTL);
		if(fs_series == 0) {
			// 44.1k
			sys_write32(sys_read32(AUDIO_PLL1_CTL) | (0x04 << CMU_AUDIOPLL1_CTL_APS1_SHIFT), AUDIO_PLL1_CTL);
		} else {
			// 48k
			sys_write32(sys_read32(AUDIO_PLL1_CTL) | (0x0c << CMU_AUDIOPLL1_CTL_APS1_SHIFT), AUDIO_PLL1_CTL);
		}
	}

	return 0;
}
void PHY_audio_inc_apll_refcnt(u8_t pll_index, u8_t series)
{
	atomic_inc(&audiopll_info[pll_index].refcnt);
	audiopll_info[pll_index].series = series;

	printk("audiopll%d use series %d cnt %d\n", pll_index, series, audiopll_info[pll_index].refcnt);
}

void PHY_audio_dec_apll_refcnt(u8_t pll_index)
{
	u32_t irq_flag;

	if(pll_index >=2) {
		printk("Wrong pll index");
		return;
	}
	irq_flag = irq_lock();

	atomic_dec(&audiopll_info[pll_index].refcnt);

	if(audiopll_info[pll_index].refcnt == 0){
		if(pll_index == 0){
			sys_write32(sys_read32(AUDIO_PLL0_CTL) & (~(1 << 4)), AUDIO_PLL0_CTL);
		}else{
			sys_write32(sys_read32(AUDIO_PLL1_CTL) & (~(1 << 4)), AUDIO_PLL1_CTL);
		}
	}

	irq_unlock(irq_flag);

	printk("audiopll%d unuse cnt %d\n", pll_index, audiopll_info[pll_index].refcnt);

}

/**************************************************************
**	Description:	set dac/adc sample rate, uint is KHz
**	Input:		sample rate / channel node / dac or adc
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int PHY_audio_set_samplerate(uint8_t sample_rate, void *ptr_channel, uint8_t dac_adc, int8_t pll_index)
{
	uint8_t temp = sample_rate;
	uint8_t index;
	uint8_t pre_div = 0xff;
	uint8_t need_series;
	uint8_t get_series;
	uint8_t use_audiopll_index = 0xff;
	uint32_t reg_data;

	aout_channel_node_t *aout_node = NULL;
	ain_channel_node_t *ain_node = NULL;

	const unsigned char samplerate_list_prediv_1[] =
	{ 192, 96, 64, 48, 32, 24, 16, 176, 88, 0xff, 44, 0xff, 22, 0xff};
	const unsigned char samplerate_list_prediv_2[] =
	{ 96, 48, 32, 24, 16, 12, 8, 88, 44, 0xff, 22, 0xff, 11, 0xff};

	if (dac_adc == 0) {
		// set dac sample rate
		aout_node = (aout_channel_node_t*)ptr_channel;
	} else if (dac_adc == 1) {
		// set adc sample rate
		ain_node = (ain_channel_node_t*)ptr_channel;
	} else {
		// error
		return -1;
	}

	for (index = 0; index < sizeof(samplerate_list_prediv_1); index++) {
		if(temp == samplerate_list_prediv_1[index]) {
			pre_div = 0;
			break;
		}
	}

	if (index >= sizeof(samplerate_list_prediv_1)) {
		for (index = 0; index < sizeof(samplerate_list_prediv_2); index++) {
			if (temp == samplerate_list_prediv_2[index]) {
				pre_div = 1;
				break;
			}
		}
	}

	if (pre_div == 0xff) {
		SYS_LOG_ERR("%d, set dac sapmle rate fail sample_rate %d ", __LINE__,sample_rate);
		return -EINVAL;
	}

	if (index >= 7) {
		// 44.1k series
		need_series = 0;
		index -= 7;
	} else {
		// 48k series
		need_series = 1;
	}

	if (pll_index == APLL_ALLOC_AUTO) {
		// get audiopll 0 state
		PHY_check_audiopll(AUDIOPLL_TYPE_0, &get_series);
		if((get_series == 0xff) || (get_series == need_series)) {
			//this time use audiopll 0
			use_audiopll_index = 0;
		}

		if(use_audiopll_index == 0xff) {
			// get audiopll 1 state
			PHY_check_audiopll(AUDIOPLL_TYPE_1, &get_series);
			if((get_series == 0xff) || (get_series == need_series)) {
				//this time use audiopll 1
				use_audiopll_index = 1;
			}
		}

		if (use_audiopll_index == 0xff) {
			SYS_LOG_ERR("failed to get available AUDIO_PLLs");
			return -1;
		}
	} else {
		//alloc by speicfic
		if (pll_index == APLL_ALLOC_AUDIOPLL0){
			PHY_check_audiopll(AUDIOPLL_TYPE_0, &get_series);
			if((get_series == 0xff) || (get_series == need_series)) {
				//this time use audiopll 0
				use_audiopll_index = 0;
			} else{
				SYS_LOG_ERR("AUDIO_PLL0 is already in use");
				return -1;
			}
		}else{
			PHY_check_audiopll(AUDIOPLL_TYPE_1, &get_series);
			if ((get_series == 0xff) || (get_series == need_series)) {
				//this time use audiopll 0
				use_audiopll_index = 1;
			} else {
				SYS_LOG_ERR("AUDIO_PLL1 is already in use");
				return -1;
			}
		}
	}

	PHY_config_audiopll(use_audiopll_index, need_series);

	if (dac_adc == 0) {
		// set dac sample rate
		if(need_series == 0) {
			aout_node->channel_sample_type = SAMPLE_TYPE_44;
		} else {
			aout_node->channel_sample_type = SAMPLE_TYPE_48;
		}

		if(use_audiopll_index == 0) {
			// use audio pll 0
			aout_node->channel_apll_sel = AUDIOPLL_TYPE_0;
			sys_write32(sys_read32(CMU_ADDACLK) & (~(1 << CMU_ADDACLK_DACCLKSRC)), CMU_ADDACLK);
		} else {
			// use audio pll 1
			aout_node->channel_apll_sel = AUDIOPLL_TYPE_1;
			sys_write32(sys_read32(CMU_ADDACLK) | (1 << CMU_ADDACLK_DACCLKSRC), CMU_ADDACLK);
		}

		aout_node->channel_sample_rate = sample_rate;
		aout_node->channel_clk_type = CLOCK_TYPE_256FS;
		aout_node->channel_dev_clk = AOUT_DAC_CLK_DAC256FS;

		// set dac clk div
		reg_data = (sys_read32(CMU_ADDACLK) & (~CMU_ADDACLK_DACCLKDIV_MASK) & (~(1 << CMU_ADDACLK_DACCLKPREDIV)));
		sys_write32((reg_data | (index << CMU_ADDACLK_DACCLKDIV_SHIFT) | (pre_div << CMU_ADDACLK_DACCLKPREDIV)),  CMU_ADDACLK);

	} else {
		// set adc sample rate
		if(need_series == 0) {
			ain_node->channel_sample_type = SAMPLE_TYPE_44;
		} else {
			ain_node->channel_sample_type = SAMPLE_TYPE_48;
		}

		if(use_audiopll_index == 0) {
			// use audiopll 0
			ain_node->channel_apll_sel = AUDIOPLL_TYPE_0;
			sys_write32(sys_read32(CMU_ADDACLK) & (~(1 << CMU_ADDACLK_ADCCLKSRC)), CMU_ADDACLK);
		} else {
			// use audiopll 1
			ain_node->channel_apll_sel = AUDIOPLL_TYPE_1;
			sys_write32(sys_read32(CMU_ADDACLK) | (1 << CMU_ADDACLK_ADCCLKSRC), CMU_ADDACLK);
		}

		ain_node->channel_sample_rate = sample_rate;
		ain_node->channel_clk_type = CLOCK_TYPE_256FS;
		ain_node->channel_dev_clk = AIN_ADC_CLK_ADCCLK;

		// set adc clk div
		reg_data = (sys_read32(CMU_ADDACLK) & (~CMU_ADDACLK_ADCCLKDIV_MASK) & (~(1 << CMU_ADDACLK_ADCCLKPREDIV)));
		sys_write32((reg_data | (index << CMU_ADDACLK_ADCCLKDIV_SHIFT) | (pre_div << CMU_ADDACLK_ADCCLKPREDIV)),  CMU_ADDACLK);

	}

	PHY_audio_inc_apll_refcnt(use_audiopll_index, need_series);

	SYS_LOG_DBG("adda clk 0x%x\n", sys_read32(CMU_ADDACLK));

	return 0;
}

/**************************************************************
**	Description:	unset dac/adc sample rate, uint is KHz
**	Input:		sample rate / channel node / dac or adc
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int PHY_audio_unset_samplerate(void *ptr_channel, uint8_t dac_adc)
{
	u8_t use_audiopll_index;
	aout_channel_node_t *aout_node = NULL;
	ain_channel_node_t *ain_node = NULL;

	if(dac_adc == 0) {
		// set dac sample rate
		aout_node = (aout_channel_node_t*)ptr_channel;
	} else if(dac_adc == 1) {
		// set adc sample rate
		ain_node = (ain_channel_node_t*)ptr_channel;
	} else {
		// error
		return -1;
	}

	if(dac_adc == 0) {
		use_audiopll_index = aout_node->channel_apll_sel;
	}else{
		use_audiopll_index = ain_node->channel_apll_sel;
	}

	PHY_audio_dec_apll_refcnt(use_audiopll_index);

	return 0;

}


/**************************************************************
**	Description:	translate sample rate from khz to hz
**	Input:      fs, uint is khz
**	Output:
**	Return:    fs, uint is hz
**	Note:
***************************************************************
**/
uint32_t get_sample_rate_hz(uint8_t fs_khz)
{
	audio_sample_rate_e sample_khz;
	uint32_t ret_sample_hz;

	sample_khz = (audio_sample_rate_e)fs_khz;

	if(sample_khz == SAMPLE_NULL) {
		return -1;
	} else if((sample_khz % SAMPLE_11KHZ) == 0) {
		// 44.1k series
		ret_sample_hz = (sample_khz / SAMPLE_11KHZ) * 11025;
	} else {
		ret_sample_hz = sample_khz * 1000;
	}

	return ret_sample_hz;
}

uint32_t get_sample_rate_khz(uint32_t fs_hz)
{
	uint32_t ret_sample_hz = 0;

	if(fs_hz == 0) {
		return ret_sample_hz;
	}

	if((fs_hz % 11) == 0) {
		ret_sample_hz = fs_hz / 11;

		if(ret_sample_hz == 1){
            ret_sample_hz = SAMPLE_11KHZ;
		}else if(ret_sample_hz == 2){
            ret_sample_hz = SAMPLE_22KHZ;
		}else if(ret_sample_hz == 4){
            ret_sample_hz = SAMPLE_44KHZ;
		}else if(ret_sample_hz == 8){
            ret_sample_hz = SAMPLE_88KHZ;
		}else if(ret_sample_hz == 16){
            ret_sample_hz = SAMPLE_176KHZ;
		}
	} else {
		ret_sample_hz = fs_hz / 1000;
	}

	return ret_sample_hz;
}


uint32_t get_sample_pairs(uint8_t sample_hz)
{
    return sample_hz / 100;
}

uint8_t dac_volume_db_to_level(int32_t vol)
{
	uint32_t level = 0;
	if (vol < 0) {
		vol = -vol;
		level = VOL_DB_TO_INDEX(vol);
		if (level > 0xBF)
			level = 0;
		else
			level = 0xBF - level;
	} else {
		level = VOL_DB_TO_INDEX(vol);
		if (level > 0x40)
			level = 0xFF;
		else
			level = 0xBF + level;
	}
	return level;
}

int32_t dac_volume_level_to_db(uint8_t level)
{
	int32_t vol = 0;
	if (level < 0xBF) {
		level = 0xBF - level;
		vol = VOL_INDEX_TO_DB(level);
		vol = -vol;
	} else {
		level = level - 0xBF;
		vol = VOL_INDEX_TO_DB(level);
	}
	return vol;
}

int adc_dmic_gain_translate(int16_t gain, uint16_t *input_gain, uint16_t *dig_gain)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(dmic_gain_mapping); i++) {
		if (gain <= dmic_gain_mapping[i].gain) {
			*input_gain = dmic_gain_mapping[i].input_gain;
			*dig_gain = dmic_gain_mapping[i].digital_gain;
			SYS_LOG_DBG("gain:%d map [%d %d]",
				gain, *input_gain, *dig_gain);
			break;
		}
	}

	if (i == ARRAY_SIZE(dmic_gain_mapping)) {
		SYS_LOG_ERR("can not find out dmic gain map %d", gain);
		return -ENOENT;
	}

	return 0;
}

int adc_amic_gain_translate(int16_t gain, uint16_t *input_gain, uint16_t *dig_gain)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(amic_gain_mapping); i++) {
		if (gain <= amic_gain_mapping[i].gain) {
			*input_gain = amic_gain_mapping[i].input_gain;
			*dig_gain = amic_gain_mapping[i].digital_gain;
			SYS_LOG_INF("gain:%d map %d [%d %d]",
				gain, i, *input_gain, *dig_gain);
			break;
		}
	}

	if (i == ARRAY_SIZE(amic_gain_mapping)) {
		SYS_LOG_ERR("can not find out amic se gain map %d", gain);
		return -ENOENT;
	}

	return 0;
}

int adc_aux_se_gain_translate(int16_t gain, uint16_t *input_gain, uint16_t *dig_gain)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(aux_se_gain_mapping); i++) {
		if (gain <= aux_se_gain_mapping[i].gain) {
			*input_gain = aux_se_gain_mapping[i].input_gain;
			*dig_gain = aux_se_gain_mapping[i].digital_gain;
			SYS_LOG_INF("gain:%d map [%d %d]",
				gain, *input_gain, *dig_gain);
			break;
		}
	}

	if (i == ARRAY_SIZE(aux_se_gain_mapping)) {
		SYS_LOG_ERR("can not find out aux se gain map %d", gain);
		return -ENOENT;
	}

	return 0;
}

int adc_aux_fd_gain_translate(int16_t gain, uint16_t *input_gain, uint16_t *dig_gain)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(aux_fd_gain_mapping); i++) {
		if (gain <= aux_fd_gain_mapping[i].gain) {
			*input_gain = aux_fd_gain_mapping[i].input_gain;
			*dig_gain = aux_fd_gain_mapping[i].digital_gain;
			SYS_LOG_INF("gain:%d map [%d %d]",
				gain, *input_gain, *dig_gain);
			break;
		}
	}

	if (i == ARRAY_SIZE(aux_fd_gain_mapping)) {
		SYS_LOG_ERR("can not find out amic aux fd gain map %d", gain);
		return -ENOENT;
	}

	return 0;
}

