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
 * Description:    pa antipop deal
 *
 * Change log:
 *	2018/5/19: Created by mike.
 ****************************************************************************
 */

#include "../audio_inner.h"
#include <mem_manager.h>
#include <stdlib.h>


#define ANTIPOP_BUF_SIZE 64

#define DMA_RELOAD_CNT_0 10
#define DMA_RELOAD_CNT_1 10
#define DMA_RELOAD_CNT_2 2400
#define DMA_RELOAD_CNT_3 10

#define RAMP_LEN  (48*2*200)

#define WAIT_TIMEOVER_CNT	(1000)

typedef struct
{
	int audio_dma;
	uint32_t current_dma_cnt;
	uint32_t dma_cnt_total;
	uint32_t finish_flag;
	uint32_t save_audiopll0_ctl;
	uint32_t save_addaclk;
	struct k_sem audio_sem;
} anti_pop_data_t;

static int audio_antipop_wait_finish(struct device *dev, anti_pop_data_t *pdata)
{
	struct acts_audio_out_data *data = dev->driver_data;
	uint32_t cur_time = k_cycle_get_32() / 24;
	uint32_t ret_val = 0;

	while(pdata->finish_flag == 0) {
		//k_busy_wait(1000);
		//count++;
		k_sem_take(&pdata->audio_sem, K_MSEC(1));
		if((k_cycle_get_32() / 24 - cur_time) > 1000000) {
			SYS_LOG_INF("%p, wait dma overtime!\n", pdata);
			dma_stop(data->dma_dev, pdata->audio_dma);
			ret_val = -EIO;
			break;
		}
	}

	//if(count > 100) {
	//	return -1;
	//}

	return ret_val;
}

static void audio_antipop_dma_handler(struct device *dev, uint32_t priv_data, int reson)
{
	anti_pop_data_t *anti_pop_ptr;

	anti_pop_ptr = (anti_pop_data_t*) priv_data;

	if (reson == DMA_IRQ_HF) {
		;	//empty
	} else {
		anti_pop_ptr->current_dma_cnt++;
		k_sem_give(&anti_pop_ptr->audio_sem);
	}

	if(anti_pop_ptr->current_dma_cnt >= anti_pop_ptr->dma_cnt_total) {
		anti_pop_ptr->finish_flag = 1;
		dma_stop(dev, anti_pop_ptr->audio_dma);
	}
}

static void audio_antipop_send_ramp_level(struct device *dev, int32_t cnt, int32_t start_dat)
{
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_dac *dac_reg = cfg->dac;

	//int32_t i;
	int32_t stepsize;
	int32_t level;
	int irq_flag = irq_lock();


	stepsize = 0xffffff/cnt;
	level = start_dat;

	dac_reg->fifoctl &= 0xffffff00;
	dac_reg->fifoctl |= (DAC_FIFOCTL_DAF0RT);

	while(1){
		while((dac_reg->stat & DAC_STAT_DAF0F) != 0) {
			;  //full wait
		}

		dac_reg->dat0 = (level<<8);
		//i++;
		level -= stepsize;
		if(level<((~0x800000))) {
			break;
		}
	}
	irq_unlock(irq_flag);
}


/**
**  start dma transfer
**/
static int audio_antipop_start_dma_trans(struct device *dev, int32_t* buffer, anti_pop_data_t *pdata)
{
	struct acts_audio_out_data *data = dev->driver_data;
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_dac *dac_reg = cfg->dac;

	struct dma_config config_data;
	struct dma_block_config head_block;

	if((buffer == NULL) || (pdata == NULL)) {
		SYS_LOG_INF("%d, antipop start dma error!\n", __LINE__);
		return -1;
	}

	if(pdata->audio_dma < 0) {
		SYS_LOG_INF("%d, antipop not get dma!\n", __LINE__);
		return -1;
	}

	// use DAC FIFO 0,  32 BIT
	dac_reg->fifoctl &= 0xffffff00;
	dac_reg->fifoctl |= (DAC_FIFOCTL_DAF0IS_DMA | DAC_FIFOCTL_DAF0EDE| DAC_FIFOCTL_DAF0RT | DAC_FIFOCTL_DMAWIDTH_32);

	memset(&config_data, 0, sizeof(config_data));
	config_data.channel_direction = MEMORY_TO_PERIPHERAL;
	config_data.source_data_size = 4;
	config_data.dma_callback = audio_antipop_dma_handler;
	config_data.callback_data = (void*)pdata;
	config_data.dma_slot = DMA_ID_AUDIO_DAC_FIFO0;

	config_data.complete_callback_en = 1;
	config_data.half_complete_callback_en = 1;
	config_data.reserved = 0;

	head_block.source_address = (unsigned int)buffer;
	head_block.dest_address = 0;
	head_block.block_size = ANTIPOP_BUF_SIZE;
	head_block.source_reload_en = 1;
	head_block.dest_reload_en = 0;

	config_data.head_block = &head_block;

    if(pdata->audio_dma != 0)
    {
    	dma_stop(data->dma_dev, pdata->audio_dma);
    }

	dma_config(data->dma_dev, pdata->audio_dma, &config_data);

	return 0;
}


int audio_antipop_set_samplerate(anti_pop_data_t *pdata)
{
	uint32_t reg_data;

	pdata->save_audiopll0_ctl = sys_read32(AUDIO_PLL0_CTL);
	pdata->save_addaclk = sys_read32(CMU_ADDACLK);

	// fix use audio pll 0, 48k
	sys_write32((sys_read32(AUDIO_PLL0_CTL) & (0 << CMU_AUDIOPLL0_CTL_MODE) & (~CMU_AUDIOPLL0_CTL_APS0_MASK)) | (1 << CMU_AUDIOPLL0_CTL_EN), AUDIO_PLL0_CTL);
	sys_write32(sys_read32(AUDIO_PLL0_CTL) | (0x0c << CMU_AUDIOPLL0_CTL_APS0_SHIFT), AUDIO_PLL0_CTL);

	sys_write32(sys_read32(CMU_ADDACLK) & (~(1 << CMU_ADDACLK_DACCLKSRC)), CMU_ADDACLK);
	// set dac clk div
	reg_data = (sys_read32(CMU_ADDACLK) & (~CMU_ADDACLK_DACCLKDIV_MASK) & (~(1 << CMU_ADDACLK_DACCLKPREDIV)));
	sys_write32((reg_data | (3 << CMU_ADDACLK_DACCLKDIV_SHIFT)),  CMU_ADDACLK);

	return 0;
}

int audio_antipop_restore_samplerate(anti_pop_data_t *pdata)
{
	sys_write32(pdata->save_audiopll0_ctl, AUDIO_PLL0_CTL);
	sys_write32(pdata->save_addaclk, CMU_ADDACLK);

	return 0;
}


/**************************************************************
**	Description:	enable pa antipop
**	Input:	device
**	Output:
**	Return:  success / fail
**	Note:
***************************************************************
**/
int audio_device_enable_pa_antipop(struct device *dev)
{
	int32_t *p_dac_buf;
	uint8_t antipop_buf[ANTIPOP_BUF_SIZE];
	uint32_t i;
	anti_pop_data_t anti_pop_data;
	struct acts_audio_out_data *data = dev->driver_data;
	const struct acts_audio_out_config *cfg = dev->config->config_info;
	struct acts_audio_dac *dac_reg = cfg->dac;

	k_sem_init(&anti_pop_data.audio_sem, 0, 1);

	SYS_LOG_INF("\n");

	if(data->dma_dev == NULL) {
		SYS_LOG_ERR("%d, not find dma device\n", __LINE__);
		return -1;
	}

	anti_pop_data.audio_dma = dma_request(data->dma_dev, 0xff);

	p_dac_buf = (int32_t *)antipop_buf;

	// send pos data
	for (i = 0; i < ANTIPOP_BUF_SIZE/4; i++) {
		*(p_dac_buf + i) = 0x7fffffff;
	}

	audio_antipop_set_samplerate(&anti_pop_data);

	anti_pop_data.current_dma_cnt = 0;
	anti_pop_data.dma_cnt_total = DMA_RELOAD_CNT_0;
	anti_pop_data.finish_flag = 0;
	audio_antipop_start_dma_trans(dev, p_dac_buf, &anti_pop_data);

	dma_start(data->dma_dev, anti_pop_data.audio_dma);

	audio_antipop_wait_finish(dev, &anti_pop_data);

	dac_reg->anactl |= DAC_ANACTL_ATPLP2;		//pa anti-pop loop2 enable

	anti_pop_data.current_dma_cnt = 0;
	anti_pop_data.dma_cnt_total = DMA_RELOAD_CNT_1;
	anti_pop_data.finish_flag = 0;
	audio_antipop_start_dma_trans(dev, p_dac_buf, &anti_pop_data);

	dma_start(data->dma_dev, anti_pop_data.audio_dma);

	audio_antipop_wait_finish(dev, &anti_pop_data);

	dac_reg->anactl |= DAC_ANACTL_ATP2CE;		//left&right ramp connect enable

	// send ramp level
	audio_antipop_send_ramp_level(dev, RAMP_LEN, 0x6fffff);

	// send neg data
	for (i = 0; i < ANTIPOP_BUF_SIZE/4; i++) {
		*(p_dac_buf + i) = 0x80000000;
	}

	anti_pop_data.current_dma_cnt = 0;
	anti_pop_data.dma_cnt_total = DMA_RELOAD_CNT_2;
	anti_pop_data.finish_flag = 0;
	audio_antipop_start_dma_trans(dev, p_dac_buf, &anti_pop_data);

	dma_start(data->dma_dev, anti_pop_data.audio_dma);

	audio_antipop_wait_finish(dev, &anti_pop_data);

	dac_reg->anactl |= (DAC_ANACTL_PAOSENL | DAC_ANACTL_PAOSENR); 	//left&right pa outstage enable

	dac_reg->anactl &= ~DAC_ANACTL_ATPLP2;	//pa anti-pop loop2 disable
	dac_reg->anactl &= ~DAC_ANACTL_ATP2CE;	//left&right ramp connect disable

	dac_reg->anactl |= DAC_ANACTL_DPBM;

	// send zero data
	for (i = 0; i < ANTIPOP_BUF_SIZE/4; i++) {
		*(p_dac_buf + i) = 0;
	}

	anti_pop_data.current_dma_cnt = 0;
	anti_pop_data.dma_cnt_total = DMA_RELOAD_CNT_3;
	anti_pop_data.finish_flag = 0;
	audio_antipop_start_dma_trans(dev, p_dac_buf, &anti_pop_data);

	dma_start(data->dma_dev, anti_pop_data.audio_dma);

	audio_antipop_wait_finish(dev, &anti_pop_data);

	dma_stop(data->dma_dev, anti_pop_data.audio_dma);
	dma_free(data->dma_dev, anti_pop_data.audio_dma);

	audio_antipop_restore_samplerate(&anti_pop_data);

	return 0;
}

/**************************************************************
**	Description:	disable pa antipop
**	Input:	device
**	Output:
**	Return:  success / fail
**	Note:
***************************************************************
**/
int audio_device_disable_pa_antipop(struct device *dev)
{
	int32_t *p_dac_buf;
	uint8_t antipop_buf[ANTIPOP_BUF_SIZE];
	uint32_t i;
	anti_pop_data_t anti_pop_data;
	struct acts_audio_out_data *data = dev->driver_data;
	//const struct acts_audio_out_config *cfg = dev->config->config_info;
	//struct acts_audio_dac *dac_reg = cfg->dac;
	k_sem_init(&anti_pop_data.audio_sem, 0, 1);


	SYS_LOG_INF("%d\n", __LINE__);

	if(data->dma_dev == NULL) {
		SYS_LOG_ERR("%d, not find dma device\n", __LINE__);
		return -1;
	}

	anti_pop_data.audio_dma = dma_request(data->dma_dev, 0xff);

	p_dac_buf = (int32_t *)antipop_buf;

	// send pos data
	for (i = 0; i < ANTIPOP_BUF_SIZE/4; i++) {
		*(p_dac_buf + i) = 0x7fffffff;
	}

	audio_antipop_set_samplerate(&anti_pop_data);

	anti_pop_data.current_dma_cnt = 0;
	anti_pop_data.dma_cnt_total = DMA_RELOAD_CNT_2;
	anti_pop_data.finish_flag = 0;
	audio_antipop_start_dma_trans(dev, p_dac_buf, &anti_pop_data);

	dma_start(data->dma_dev, anti_pop_data.audio_dma);

	audio_antipop_wait_finish(dev, &anti_pop_data);

	//dac_reg->anactl |= DAC_ANACTL_ATP2CE;		//left&right ramp connect enable

	//dac_reg->anactl &= (~DAC_ANACTL_ATP2CE);	//left&right ramp connect disable

	dma_stop(data->dma_dev, anti_pop_data.audio_dma);
	dma_free(data->dma_dev, anti_pop_data.audio_dma);

	audio_antipop_restore_samplerate(&anti_pop_data);

	return 0;
}

