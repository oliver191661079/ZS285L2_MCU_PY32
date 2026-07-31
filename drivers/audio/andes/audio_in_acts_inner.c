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
 * Description:    audio in driver
 *
 * Change log:
 *	2018/5/24: Created by mike.
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
#include <audio_in.h>


extern uint32_t cal_asrc_rate(uint32_t sr_input, uint32_t sr_output, int32_t aps_level);


OS_MUTEX_DEFINE(drv_audio_in_mutex);


/**
**  lock
**/
void drv_audio_in_lock(void)
{
    os_mutex_lock(&drv_audio_in_mutex, OS_FOREVER);
}

/**
**  unlock
**/
void drv_audio_in_unlock(void)
{
    os_mutex_unlock(&drv_audio_in_mutex);
}

/**************************************************************
**	Description:	config asrc in module
**	Input:      device structure / asrc ram / asrc mode
**	Output:
**	Return:    success / fail
**	Note:
***************************************************************
**/
int audio_in_asrc_config(struct device *dev, phy_asrc_param_t *phy_asrc_set)
{
	int result;

	if(phy_asrc_set == NULL) {
		SYS_LOG_ERR("%d, ain asrc param error!\n", __LINE__);
		return -1;
	}

	if(!asrc_status_check(dev, 0xff)) {
		PHY_init_asrc(dev);
	}

	result = PHY_open_asrc_in(dev, phy_asrc_set);

	return result;
}


/**************************************************************
**	Description:	asrc in close
**	Input:		device type / asrc in channel
**	Output:
**	Return:    success/fail
**	Note:
***************************************************************
**/
int audio_in_asrc_close(struct device *dev, asrc_type_e in_channel)
{
	uint8_t ram_index;
	int32_t result;

	struct acts_audio_in_data *data = dev->driver_data;

	if((in_channel <= ASRC_OUT_MAX) || (in_channel >= ASRC_TYPE_MAX)) {
		SYS_LOG_INF("close asrc in param error");
		return -EINVAL;
	}

	if(!asrc_status_check(dev, (uint8_t)in_channel)) {
		SYS_LOG_INF("asrc in %d is not opened\n", in_channel);
		return -1;
	}

	if(in_channel == ASRC_IN_0) {
		ram_index = data->cur_asrc_in0_ramindex;
	} else {
		ram_index = data->cur_asrc_in1_ramindex;
	}

	result = PHY_close_asrc_in(dev, ram_index);

	if(in_channel == ASRC_IN_0) {
		data->cur_asrc_in0_ramindex = 0xff;
	} else {
		data->cur_asrc_in1_ramindex = 0xff;
	}

	SYS_LOG_INF("asrc in %d close success\n", in_channel);

	return result;
}

int get_asrc_in_reg_param(PHY_audio_in_param_t *setting)
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
		case K_IN_P12:
			ram_size = 8192;
			break;

		case K_IN_P2:
		case K_IN1_P1:
		case K_IN1_P3:
			ram_size = 4096;
			break;

		case K_IN_U1:
			ram_size = 2048;
			break;

		default:
			ram_size = 0;
			break;
	}

	if(ram_size == 0) {
		return -1;
	}

	setting->ptr_asrc_setting->reg_hempty = ram_size/16;
	setting->ptr_asrc_setting->reg_hfull = (ram_size/16) * 3;

	sr_input = setting->ptr_asrc_setting->input_samplerate;
	sr_output = setting->ptr_asrc_setting->output_samplerate;

	setting->ptr_asrc_setting->reg_dec0 = cal_asrc_rate(sr_input, sr_output, ASRC_LEVEL_DEFAULT);

	setting->ptr_asrc_setting->reg_dec1 = setting->ptr_asrc_setting->reg_dec0;

	printk("%d, sr_in: %d, sr_out: %d\n", __LINE__, sr_input, sr_output);

	printk("%d, dec0: %d, dec1: %d\n", __LINE__, setting->ptr_asrc_setting->reg_dec0, setting->ptr_asrc_setting->reg_dec1);

	printk("%d, hep: %d, hfl: %d\n", __LINE__, setting->ptr_asrc_setting->reg_hempty, setting->ptr_asrc_setting->reg_hfull);

	return 0;

}

/**************************************************************
**	Description:	prepare asrc out param
**	Input:      device structure / audio out param / channel node
**	Output:
**	Return:    success / fail
**	Note:
***************************************************************
**/
int prepare_asrc_in_param(struct device *dev, PHY_audio_in_param_t *setting, ain_channel_node_t *channel)
{
	struct acts_audio_in_data *data = dev->driver_data;
	uint8_t need_asrc_in_index = 0;
	uint8_t asrc_ram = 0;

    if(setting->asrc_alloc_method == ASRC_ALLOC_AUTO){
        if(channel->channel_fifo_sel == AIN_FIFO_ADC) {
            need_asrc_in_index = ASRC_IN_0;
        } else if(channel->channel_fifo_sel == AIN_FIFO_I2SRX1) {
            if(asrc_status_check(dev, ASRC_IN_0) == FALSE) {
                need_asrc_in_index = ASRC_IN_0;
            } else if(asrc_status_check(dev, ASRC_IN_1) == FALSE) {
                need_asrc_in_index = ASRC_IN_1;
            } else {
                return -EINVAL;
            }
        } else{
            SYS_LOG_ERR("ain channel fifo error");
            return -EINVAL;
        }

    	if(need_asrc_in_index == ASRC_IN_0) {
            asrc_ram = K_IN_P2;
    	} else if(need_asrc_in_index == ASRC_IN_1) {
    	    asrc_ram = K_IN1_P3;
    	} else {
    		;	//empty, not enter
    	}
    }else if(setting->asrc_alloc_method == ASRC_ALLOC_LOW_LATENCY){
        need_asrc_in_index = ASRC_IN_0;
        asrc_ram = K_IN_U1;
    }

	// 确认ASRC IN 未被占用
	if(asrc_status_check(dev, need_asrc_in_index) == TRUE) {
		SYS_LOG_ERR("%d, asrc in %d can not use!", __LINE__, need_asrc_in_index);
		return -EINVAL;
	}

    channel->channel_asrc_index = need_asrc_in_index;

    setting->ptr_asrc_setting->asrc_ram = asrc_ram;

    printk("channel %d asrc index %d ram index %d\n", \
        channel->channel_fifo_sel, need_asrc_in_index, asrc_ram);

    if(need_asrc_in_index == ASRC_IN_0){
        data->cur_asrc_in0_ramindex = setting->ptr_asrc_setting->asrc_ram;
    }else{
        data->cur_asrc_in1_ramindex = setting->ptr_asrc_setting->asrc_ram;
    }

	if(channel->channel_type == AIN_CHANNEL_ADC){
        setting->ptr_asrc_setting->ctl_wr_clk = (uint32_t)ASRC_IN_WCLK_IISRX0;
	}else if(channel->channel_type == AIN_CHANNEL_I2SRX0){
        setting->ptr_asrc_setting->ctl_wr_clk = (uint32_t)ASRC_IN_WCLK_IISRX0;
	}else if(channel->channel_type == AIN_CHANNEL_I2SRX1){
        setting->ptr_asrc_setting->ctl_wr_clk = (uint32_t)ASRC_IN_WCLK_IISRX1;
	}else if(channel->channel_type == AIN_CHANNEL_SPDIF){
		setting->ptr_asrc_setting->ctl_wr_clk = (uint32_t)ASRC_IN_WCLK_IISRX1;
	}

	if(get_asrc_in_reg_param(setting) < 0) {
		return -EINVAL;
	}

	channel->channel_asrc_dec = setting->ptr_asrc_setting->reg_dec0;

    channel->sr_input = setting->ptr_asrc_setting->input_samplerate;
    channel->sr_output = setting->ptr_asrc_setting->output_samplerate;

	return 0;
}

