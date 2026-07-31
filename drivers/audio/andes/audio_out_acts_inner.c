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
 * Description:    audio out driver for acts
 *
 * Change log:
 *	2018/9/26: Created by mikeyang.
 ****************************************************************************
 */

#include "audio_inner.h"
#include "audio_asrc.h"

#include <kernel.h>
#include <init.h>
#include <device.h>
#include <irq.h>
#include <dma.h>
#include <soc.h>
#include <string.h>
#include <gpio.h>
#include <board.h>
#include <os_common_api.h>
#include <audio_out.h>

OS_MUTEX_DEFINE(drv_audio_out_mutex);

/**
**  lock
**/
void drv_audio_out_lock(void)
{
    os_mutex_lock(&drv_audio_out_mutex, OS_FOREVER);
}

/**
**  unlock
**/
void drv_audio_out_unlock(void)
{
    os_mutex_unlock(&drv_audio_out_mutex);
}

extern uint32_t cal_asrc_rate(uint32_t sr_input, uint32_t sr_output, int32_t aps_level);

int get_asrc_out_reg_param(PHY_audio_out_param_t *setting)
{
	asrc_ram_index_e asrc_ram;
	uint16_t ram_size;
	uint32_t sr_input;
	uint32_t sr_output;

	if((setting == NULL) || (setting->ptr_asrc_setting == NULL)) {
		return -1;
	}

	asrc_ram = setting->ptr_asrc_setting->asrc_ram;

	switch(asrc_ram) {
		case K_OUT0_P01:
		case K_OUT1_P12_16bit:
			ram_size = 8192;
			break;

		case K_OUT0_P0:
		case K_OUT1_P2:
		case K_OUT1_P3:
		case K_OUT1_P1:
			ram_size = 4096;
			break;

		case K_OUT0_U0:
			ram_size = 2048;
			break;

		default:
			ram_size = 0;
			break;
	}

	if (ram_size == 0) {
		printk("%d asrc_ram:%d error ram_size:%d", __LINE__, asrc_ram, ram_size);
		return -1;
	}

	sr_input = setting->ptr_asrc_setting->input_samplerate;
	sr_output = setting->ptr_asrc_setting->output_samplerate;

	printk("%d, sr_in: %d, sr_out: %d\n", __LINE__, sr_input, sr_output);

	setting->ptr_asrc_setting->reg_dec0 =  cal_asrc_rate(sr_input, sr_output, ASRC_LEVEL_DEFAULT);

	setting->ptr_asrc_setting->reg_dec1 = setting->ptr_asrc_setting->reg_dec0;

	printk("%d, dec0: %d, dec1: %d\n", __LINE__, setting->ptr_asrc_setting->reg_dec0, setting->ptr_asrc_setting->reg_dec1);

	printk("%d, hep: %d, hfl: %d\n", __LINE__, setting->ptr_asrc_setting->reg_hempty, setting->ptr_asrc_setting->reg_hfull);

	printk("%d, rd_clk: %d wr_clk: %d\n", __LINE__, setting->ptr_asrc_setting->ctl_rd_clk, setting->ptr_asrc_setting->ctl_wr_clk);

	return 0;

}

/**************************************************************
**	Description:	prepare asrc out param
**	Input:      device structure / audio out param / channel node
**	Output:
**	Return:    success / fail
**	Note:
                12k         8k       4k     2K      3K
    OUT0       P0+P1+P2     P0+P1    P0     U0      U0+P5

                4k          4k       8k     4k      4k      6k
    OUT1        P2          P3       P1+P2  P3      P1      P1+P2

                8k          4k       2k     3k
    IN0         P1+P2       P2       U1     U1+P4

                4k          4k       4k     6k
    IN1         P3          P3       P1     P3+P6
***************************************************************
**/
int prepare_asrc_out_param(struct device *dev, PHY_audio_out_param_t *setting, aout_channel_node_t *channel)
{
	struct acts_audio_out_data *data = dev->driver_data;
	uint8_t need_asrc_out_index = 0;
	uint8_t asrc_ram = 0;

    if(setting->asrc_alloc_method == ASRC_ALLOC_AUTO){
        if(channel->channel_fifo_sel == AOUT_FIFO_DAC0) {
            // $)AJ9SCDAC FIFO 0#,TrV;D\SCASRC OUT 0
            need_asrc_out_index = ASRC_OUT_0;
        } else if(channel->channel_fifo_sel == AOUT_FIFO_DAC1) {
            // $)AJ9SCDAC FIFO 1#,TrV;D\SCASRC OUT 1
            need_asrc_out_index = ASRC_OUT_1;
        } else if(channel->channel_fifo_sel == AOUT_FIFO_I2STX0) {
            // $)AJ9SCI2S TX0 FIFO#,TrQ!TqN41;U<SC5DASRC OUT
            if(asrc_status_check(dev, ASRC_OUT_0) == FALSE) {
                need_asrc_out_index = ASRC_OUT_0;
            } else if(asrc_status_check(dev, ASRC_OUT_1) == FALSE) {
                need_asrc_out_index = ASRC_OUT_1;
            } else {
                return -EINVAL;
            }
        }

        if(need_asrc_out_index == ASRC_OUT_0) {
            // asrc out 0 fix use PCMRAM 0
            asrc_ram = K_OUT0_P0;
        } else if(need_asrc_out_index == ASRC_OUT_1) {
            // asrc out 1 fix use PCMRAM 1
            asrc_ram = K_OUT1_P1;
        } else {
            ;   //empty, not enter
        }
    }else if(setting->asrc_alloc_method == ASRC_ALLOC_LOW_LATENCY){
		if (channel->channel_fifo_sel == AOUT_FIFO_DAC0) {
        	need_asrc_out_index = ASRC_OUT_0;
        	asrc_ram = K_OUT0_U0;
		} else if (channel->channel_fifo_sel == AOUT_FIFO_DAC1) {
        	need_asrc_out_index = ASRC_OUT_1;
        	asrc_ram = K_OUT1_P1;
		} else if (channel->channel_fifo_sel == AOUT_FIFO_I2STX0) {
            need_asrc_out_index = ASRC_OUT_1;
            asrc_ram = K_OUT1_P1;
		}
    }else if(setting->asrc_alloc_method == ASRC_ALLOC_SUBWOOFER){
        if(channel->channel_fifo_sel == AOUT_FIFO_DAC0) {
            need_asrc_out_index = ASRC_OUT_0;
            asrc_ram = K_OUT0_P0;
        } else if(channel->channel_fifo_sel == AOUT_FIFO_DAC1) {
            need_asrc_out_index = ASRC_OUT_1;
            asrc_ram = K_OUT1_P1;
		} else if(channel->channel_fifo_sel == AOUT_FIFO_I2STX0) {
            need_asrc_out_index = ASRC_OUT_1;
            asrc_ram = K_OUT1_P1;
        }
    }

    // $)AH7HOASRC OUT N41;U<SC
    if(asrc_status_check(dev, need_asrc_out_index) == TRUE) {
        SYS_LOG_ERR("%d, asrc out %d  %d can not use!", __LINE__, need_asrc_out_index, setting->asrc_alloc_method);
        panic("asrc error\n");
        return -EINVAL;
    }

    channel->channel_asrc_index = need_asrc_out_index;

    setting->ptr_asrc_setting->asrc_ram = asrc_ram;

    printk("channel %d asrc index %d ram index %d\n", \
        channel->channel_fifo_sel, need_asrc_out_index, asrc_ram);

    if(need_asrc_out_index == ASRC_OUT_0){
        data->cur_asrc_out0_ramindex = setting->ptr_asrc_setting->asrc_ram;
    }else{
        data->cur_asrc_out1_ramindex = setting->ptr_asrc_setting->asrc_ram;
    }

	if(channel->channel_type == AOUT_CHANNEL_DAC){
        //need config asrc out
        setting->ptr_asrc_setting->output_samplerate = channel->channel_sample_rate;
        // asrc out -> dac
        setting->ptr_asrc_setting->ctl_rd_clk = (uint32_t)ASRC_OUT_RCLK_DAC;
	}else if(channel->channel_type == AOUT_CHANNEL_I2S || channel->channel_type == AOUT_CHANNEL_LINKAGE_DAC_I2S){
        //need config asrc out
        setting->ptr_asrc_setting->output_samplerate = channel->channel_sample_rate;
        if(setting->aout_fifo_sel == AOUT_FIFO_I2STX0) {
            //asrc out -> i2stx0 fifo
            setting->ptr_asrc_setting->ctl_rd_clk = (uint32_t)ASRC_OUT_RCLK_IIS;
        } else {
            //asrc out -> dac fifo
            setting->ptr_asrc_setting->ctl_rd_clk = (uint32_t)ASRC_OUT_RCLK_DAC;
        }
	}else if(channel->channel_type == AOUT_CHANNEL_SPDIF || channel->channel_type == AOUT_CHANNEL_LINKAGE_DAC_SPDIF ||
        channel->channel_type == AOUT_CHANNEL_LINKAGE_DAC_I2S_SPDIF || channel->channel_type == AOUT_CHANNEL_LINKAGE_I2S_SPDIF
    ){
        //need config asrc out
        setting->ptr_asrc_setting->output_samplerate = channel->channel_sample_rate;
        if(setting->aout_fifo_sel == AOUT_FIFO_I2STX0) {
            //asrc out -> i2stx0 fifo
            setting->ptr_asrc_setting->ctl_rd_clk = (uint32_t)ASRC_OUT_RCLK_IIS;
        } else {
            //asrc out -> dac fifo
            setting->ptr_asrc_setting->ctl_rd_clk = (uint32_t)ASRC_OUT_RCLK_DAC;
        }
	}

	if(get_asrc_out_reg_param(setting) < 0) {
        SYS_LOG_ERR("%d, asrc out %d  %d can not use!", __LINE__, need_asrc_out_index, setting->asrc_alloc_method);
	    panic("asrc error\n");
		return -EINVAL;
	}

	channel->channel_asrc_dec = setting->ptr_asrc_setting->reg_dec0;
    channel->sr_input = setting->ptr_asrc_setting->input_samplerate;
    channel->sr_output = setting->ptr_asrc_setting->output_samplerate;

	return 0;
}


/**************************************************************
**	Description:	asrc out config
**	Input:		device type / phy_asrc_param
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int audio_out_asrc_config(struct device *dev, phy_asrc_param_t *phy_asrc_set)
{
	int result;

	if(phy_asrc_set == NULL) {
		SYS_LOG_ERR("%d, aout asrc param error!\n", __LINE__);
		return -1;
	}

	if(!asrc_status_check(dev, 0xff)) {
		PHY_init_asrc(dev);
	}

	result = PHY_open_asrc_out(dev, phy_asrc_set);

	return result;
}


/**************************************************************
**	Description:	asrc out close
**	Input:		device type / asrc out channel
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int audio_out_asrc_close(struct device *dev, asrc_type_e out_channel)
{
	uint8_t ram_index;
	int result;

	struct acts_audio_out_data *data = dev->driver_data;

	if(out_channel >= ASRC_OUT_MAX) {
		SYS_LOG_INF("close asrc out param error");
		return -EINVAL;
	}

	if(!asrc_status_check(dev, (uint8_t)out_channel)) {
		SYS_LOG_INF("asrc out %d is not opened", out_channel);
		return -1;
	}

	if(out_channel == ASRC_OUT_0) {
		ram_index = data->cur_asrc_out0_ramindex;
	}else {
		ram_index = data->cur_asrc_out1_ramindex;
	}

	result = PHY_close_asrc_out(dev, ram_index);

	if(out_channel == ASRC_OUT_0) {
		data->cur_asrc_out0_ramindex = 0xff;
	} else {
		data->cur_asrc_out1_ramindex = 0xff;
	}

	SYS_LOG_INF("asrc out %d close success", out_channel);

	return result;
}


/**************************************************************
**	Description:	open pa
**	Input:		device type / pa param
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int audio_out_open_pa(struct device *dev, phy_pa_setting_t *pa_param, int antipop_enable)
{
	phy_pa_setting_t param;
	struct acts_audio_out_data *data = dev->driver_data;

	if(data->pa_open_flag != 0) {
		SYS_LOG_DBG("pa is opened, no need open again!");
		return -1;
	}

	if(NULL == pa_param) {
		// $)AJ9SCD,HO2NJ}4r?*pa
		SYS_LOG_INF("default pa param!");

#ifndef CONFIG_AUDIO_OUT_ACTS_L_MUTE
		param.left_mute_flag = 0;
#else
		param.left_mute_flag = 1;
#endif

#ifndef CONFIG_AUDIO_OUT_ACTS_R_MUTE
		param.right_mute_flag = 0;
#else
		param.right_mute_flag = 1;
#endif

		param.aa_mode_src = AA_SOURCE_NONE;

		PHY_audio_enable_pa(dev, &param, antipop_enable);
	} else {
		// $)AJ9SCV86(2NJ}EdVCpa
		SYS_LOG_INF("set pa param!");

		PHY_audio_enable_pa(dev, pa_param, antipop_enable);
	}

#ifdef CONFIG_BOARD_EXTERNAL_PA_ENABLE
	// open external pa
	board_extern_pa_ctl(1);
#endif

	data->pa_open_flag = 1;

	return 0;
}

/**************************************************************
**	Description:	close pa
**	Input:		device type
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int audio_out_close_pa(struct device *dev, int antipop_enable)
{
	struct acts_audio_out_data *data = dev->driver_data;

	if(data->pa_open_flag == 0) {
		SYS_LOG_DBG("pa is closed, no need close again!");
		return -1;
	}

	SYS_LOG_INF("%d, audio close pa!\n", __LINE__);

	// internal pa
	PHY_audio_disable_pa(dev, antipop_enable);

#ifdef CONFIG_BOARD_EXTERNAL_PA_ENABLE
	// close external pa
	board_extern_pa_ctl(0);
#endif

	data->pa_open_flag = 0;

    return 0;
}

/**************************************************************
**	Description:	get current pa open/close status
**	Input:		device type
**	Output:    pa status
**	Return:    success/fail
**	Note:
***************************************************************
**/
int audio_out_get_pa_status(struct device *dev, uint32_t *value)
{
	struct acts_audio_out_data *data = dev->driver_data;

	*value = (uint32_t)data->pa_open_flag;

	return 0;
}

/**************************************************************
**	Description:	set dac gain (da/pa)
**	Input:		device type / vol param
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int audio_out_set_gain(struct device *dev, uint32_t vol_param)
{
	SYS_LOG_INF("volume:0x%x", vol_param);
	PHY_audio_set_dac_gain(dev, vol_param);
	return 0;
}

/**************************************************************
**	Description:	get dac gain (da/pa)
**	Input:		device type
**	Output:    gain
**	Return:    success/fail
**	Note:
***************************************************************
**/
int audio_out_get_gain(struct device *dev, int *vol)
{
	*vol = PHY_audio_get_dac_gain(dev);

	SYS_LOG_DBG("%d, get volume:0x%x", __LINE__, *vol);
	return 0;
}


/**************************************************************
**	Description:	pa left / right mute
**	Input:		device type /param
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int audio_out_pa_mute(struct device *dev, pa_mute_setting_t *pa_mute_param)
{
	struct acts_audio_out_data *data = dev->driver_data;

	if(data->pa_open_flag == 0) {
		SYS_LOG_INF("%d, pa is not open! Return!", __LINE__);
		return -1;
	}

	PHY_audio_pa_mute(dev, pa_mute_param);

#ifdef CONFIG_BOARD_EXTERNAL_PA_ENABLE
	// close external pa
	board_extern_pa_ctl(0);
#endif

	return 0;
}

/**************************************************************
**	Description:	enable channel linkage mode
**	Input:		dev / channel
**	Output:
**	Return:    success / fail
**	Note:
***************************************************************
**/
int enable_channel_linkage(struct device *dev, aout_channel_node_t *channel_node)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_i2stx0 *i2stx_reg = cfg->i2stx;
	struct acts_audio_dac *dac_reg = cfg->dac;
	int result = 0;

	switch(channel_node->channel_type) {
		case AOUT_CHANNEL_LINKAGE_DAC_I2S:
			dac_reg->digctl &= ~DAC_DIGCTL_MULT_DEVICE_MASK;
			dac_reg->digctl |= DAC_DIGCTL_MULT_I2S;
			break;

		case AOUT_CHANNEL_LINKAGE_DAC_SPDIF:
			dac_reg->digctl &= ~DAC_DIGCTL_MULT_DEVICE_MASK;
			dac_reg->digctl |= DAC_DIGCTL_MULT_SPF;
			break;

		case AOUT_CHANNEL_LINKAGE_DAC_I2S_SPDIF:
			dac_reg->digctl &= ~DAC_DIGCTL_MULT_DEVICE_MASK;
			dac_reg->digctl |= DAC_DIGCTL_MULT_I2SSPF;
			break;

		case AOUT_CHANNEL_LINKAGE_I2S_SPDIF:
			i2stx_reg->tx0_ctl &= ~I2STX0_CTL_MULT_DEVICE_MASK;
			i2stx_reg->tx0_ctl |= I2STX0_CTL_MULT_SPF;
			break;

		default:
			SYS_LOG_ERR("%d, enable linkage fail\n", __LINE__);
			result = -1;
			break;
	}

	return result;
}


/**************************************************************
**	Description:	disable channel linkage mode
**	Input:		dev / channel
**	Output:
**	Return:    success / fail
**	Note:
***************************************************************
**/
int disable_channel_linkage(struct device *dev, aout_channel_node_t *channel_node)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_i2stx0 *i2stx_reg = cfg->i2stx;
	struct acts_audio_dac *dac_reg = cfg->dac;
	int result = 0;

	switch(channel_node->channel_type) {
		case AOUT_CHANNEL_LINKAGE_DAC_I2S:
		case AOUT_CHANNEL_LINKAGE_DAC_SPDIF:
		case AOUT_CHANNEL_LINKAGE_DAC_I2S_SPDIF:
			dac_reg->digctl &= ~DAC_DIGCTL_MULT_DEVICE_MASK;
			break;

		case AOUT_CHANNEL_LINKAGE_I2S_SPDIF:
			i2stx_reg->tx0_ctl &= ~I2STX0_CTL_MULT_DEVICE_MASK;
			break;

		default:
			SYS_LOG_ERR("%d, disable linkage fail\n", __LINE__);
			result = -1;
			break;
	}

	return result;
}
