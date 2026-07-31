/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief virtual dma controller driver for Actions SoC
 */

#include <kernel.h>
#include <dma.h>
#include <soc.h>
#include <string.h>
#include "rom_vdma_andes.h"

#define SYS_LOG_DOMAIN "DMA"
#define SYS_LOG_LEVEL CONFIG_SYS_LOG_DMA_LEVEL
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_VDMA_ACTS_ANDES

rom_dma_data_t rom_dma_global_data;
rom_dma_data_t *rom_dma_global_p;

struct dma_controller dmacs[CONFIG_VDMA_ACTS_PCHAN_NUM];

//1个是固定分配给printk使用的dma
static struct dma_wrapper_data wrapper_data[CONFIG_VDMA_ACTS_VCHAN_NUM + 1];

K_MEM_SLAB_DEFINE(vdma_pool, sizeof(struct dma), CONFIG_VDMA_ACTS_VCHAN_NUM, 4);

static void dma_acts_irq(int irq, int type, void *pdata);
 void dma_fixed_acts_irq(int irq);

void install_dma_phy_channel(unsigned int channel_num)
{
	rom_dma_data_t *rom_data = (rom_dma_data_t *) rom_dma_global_p;

	irq_enable(IRQ_ID_DMA0 + channel_num);

	rom_data->dmacs[rom_data->dma_num].dma_chan_num = channel_num;
	rom_data->dmacs[rom_data->dma_num].channel_assigned = 0;
	rom_data->dmacs[rom_data->dma_num].dma = NULL;

	rom_data->dma_num++;
}

void unistall_dma_phy_channel(unsigned int channel_num)
{
	int i;
	rom_dma_data_t *rom_data = (rom_dma_data_t *) rom_dma_global_p;

	irq_disable(IRQ_ID_DMA0 + channel_num);

	for (i = 0; i < CONFIG_VDMA_ACTS_VCHAN_NUM; i++) {
		if (rom_data->dmacs[i].dma_chan_num == channel_num) {
			rom_data->dmacs[i].dma_chan_num = 0xff;
			rom_data->dmacs[i].channel_assigned = 1;
			rom_data->dmacs[i].dma = NULL;

			rom_data->dma_num--;
		}
	}
}

static struct dma_wrapper_data * dma_wrapper_data_alloc(u32_t id)
{
	int i;

	for (i = 0; i < CONFIG_VDMA_ACTS_VCHAN_NUM; i++) {
		if (wrapper_data[i].dma_handle == id)
			break;
	}

	if (i == CONFIG_VDMA_ACTS_VCHAN_NUM) {
		for (i = 0; i < CONFIG_VDMA_ACTS_VCHAN_NUM; i++) {
			if (wrapper_data[i].dma_handle == 0)
				break;
		}
	}

	return &wrapper_data[i];
}

static void dma_wrapper_data_free(u32_t id)
{
	int i;

	for (i = 0; i < CONFIG_VDMA_ACTS_VCHAN_NUM; i++) {
		if (wrapper_data[i].dma_handle == id) {
			wrapper_data[i].dma_handle = 0;
			break;
		}
	}
}

int dma_acts_request(struct device *dev, u32_t id)
{
	if (id != DMA_INVALID_DMACS_NUM) {
		return dma_alloc_channel(id);
	} else {
		struct dma *dma;
		int err;

		err = k_mem_slab_alloc(&vdma_pool, (void **)&dma, K_FOREVER);
		if (err || dma == NULL)
			return 0;

		memset(dma, 0, sizeof(*dma));
		dma->dmac_num = DMA_INVALID_DMACS_NUM;
		set_dma_state(dma, DMA_STATE_ALLOC);

		return (int)dma;
	}
}

void dma_acts_free(struct device *dev, u32_t id)
{
	dma_wrapper_data_free(id);

	if(is_virtual_dma_handle(id)){
		k_mem_slab_free(&vdma_pool, (void **)&id);
	}
}

static int dma_acts_config(struct device *dev, u32_t id,
			    struct dma_config *config)
{
	struct dma_block_config *head_block = config->head_block;
	struct dma_wrapper_data *wrapper_data;
	u32_t ctl;
	int data_width = 1;

    if(id == 0){
        SYS_LOG_ERR("invalid dma chan");
        return -EINVAL;
    }

	if (head_block->block_size > DMA_ACTS_MAX_DATA_ITEMS) {
		SYS_LOG_ERR("DMA error: Data size too big: %d",
		       head_block->block_size);
		return -EINVAL;
	}

	if (config->channel_direction == MEMORY_TO_PERIPHERAL) {
		ctl = DMA_CTL_SRC_TYPE(DMA_ID_MEMORY) |
		      DMA_CTL_DST_TYPE(config->dma_slot) | (1 << DMA_CTL_DAM_SHIFT);

	} else if (config->channel_direction == PERIPHERAL_TO_MEMORY)  {
		ctl = DMA_CTL_SRC_TYPE(config->dma_slot) |
		      DMA_CTL_DST_TYPE(DMA_ID_MEMORY) | (1 << DMA_CTL_SAM_SHIFT);
	} else {
		ctl = DMA_CTL_SRC_TYPE(DMA_ID_MEMORY) |
		      DMA_CTL_DST_TYPE(DMA_ID_MEMORY);
	}

	if (config->source_data_size) {
		data_width = config->source_data_size;
	}

	if (config->dest_data_size) {
		data_width = config->dest_data_size;
	}

	if (head_block->source_reload_en || head_block->dest_reload_en) {
		ctl |= DMA_CTL_RELOAD;
	}

	switch (data_width) {
	case 2:
		ctl |= DMA_CTL_TWS_16BIT;
		break;
	case 4:
		ctl |= DMA_CTL_TWS_32BIT;
		break;
	case 8:
		ctl |= DMA_CTL_TWS_64BIT;
		break;
	case 1:
	default:
		ctl |= DMA_CTL_TWS_8BIT;
		break;
	}

	if (config->source_burst_length == 1 || config->dest_burst_length == 1) {
		ctl |= DMA_CTL_TRMODE_SINGLE;
	}

	if (config->dma_sam) {
		ctl |= (1 << DMA_CTL_SAM_SHIFT);
	}

	/** extern for actions dma interleaved mode */
	if (config->reserved == 1) {
		ctl |= (1 << DMA0CTL_AUDIOTYPE);
		/* 由于默认dma传参只传一个源地址，一个目的地址,当DMA单声道转双声道时需要
		   两个源地址,固化代码在设计的时候就借用了目的地址参数，因为目的地址参数这个
		   时候没有用，这个使用点需要注意
		 */
		if (config->channel_direction == MEMORY_TO_PERIPHERAL){
            head_block->dest_address = head_block->source_address;
		}else if(config->channel_direction == PERIPHERAL_TO_MEMORY){
//            head_block->source_address = head_block->dest_address;
		}
	}
	rom_dma_global_p->dma_cb->frame_config(id,
		(u32_t)head_block->source_address,
		(u32_t)head_block->dest_address,
		(u32_t)head_block->block_size);
	if (config->complete_callback_en || config->half_complete_callback_en ||
	    config->error_callback_en) {
		wrapper_data = dma_wrapper_data_alloc(id);

		if(wrapper_data != NULL){
			wrapper_data->dma_handle = id;
			wrapper_data->dma_callback = config->dma_callback;
			wrapper_data->callback_data = config->callback_data;
		}
		rom_dma_global_p->dma_cb->config(id, ctl,
			(dma_irq_handler_t)dma_acts_irq, (void *)wrapper_data, 1);

	} else {
		rom_dma_global_p->dma_cb->config(id, ctl, NULL, NULL, 1);
	}

	return 0;
}

static int dma_acts_reload(struct device *dev, u32_t id, u32_t saddr, u32_t daddr, u32_t frame_len)
{
    if(id == 0){
        SYS_LOG_ERR("invalid dma chan");
        return -EINVAL;
    }
	return rom_dma_global_p->dma_cb->frame_config(id, saddr, daddr, frame_len);
}

static int dma_acts_start(struct device *dev, u32_t id)
{
	struct dma *dma;
    if(id == 0){
        SYS_LOG_ERR("invalid dma chan");
        return -EINVAL;
    }

	if(is_virtual_dma_handle(id)){
		dma = (struct dma *) id;

		if(dma->dma_state >= DMA_STATE_CONFIG && dma->dma_state <= DMA_STATE_IRQ_TC){
			SYS_LOG_ERR("error dma state:0x%x", dma->dma_state);
			return -EIO;
		}
	}

	rom_dma_global_p->dma_cb->start(rom_dma_global_p, id);

	return 0;
}

static int dma_acts_stop(struct device *dev, u32_t id)
{
    if(id == 0){
        SYS_LOG_ERR("invalid dma chan");
        return -EINVAL;
    }
	rom_dma_global_p->dma_cb->abort(rom_dma_global_p, id);
	return 0;
}

static int dma_acts_get_phy_channel(struct device *dev, u32_t id);

u32_t dma_acts_get_remain_size(struct device *dev, u32_t id)
{
    struct dma *dma;
	int framelen, remainlen;
	unsigned short reload_cnt;

    if (is_virtual_dma_handle(id)){
        dma = (struct dma *) id;

        if(get_dma_state(dma) == DMA_STATE_ALLOC){
            return 0;
        }

		if (rom_dma_global_p->dma_cb->query_complete(rom_dma_global_p, id)){
			return 0;
		} else {
			rom_dma_global_p->dma_cb->get_samples_info(rom_dma_global_p, (dma_reg_t *)dma_acts_get_phy_channel(dev, id), &framelen, &remainlen, &reload_cnt);

			return remainlen;
		}
    }else{
		dma_reg_t *reg = (dma_reg_t *)id;

		return reg->frame_remainlen;
	}
}

int dma_acts_wait_timeout(struct device *dev, u32_t id, u32_t timeout)
{
	return rom_dma_global_p->dma_cb->wait_complete(rom_dma_global_p, id, timeout);
}

static int dma_acts_prepare_start(struct device *dev, u32_t id)
{
	return acts_dma_pre_start(id);
}

static int dma_acts_get_transfer_length(struct device *dev, u32_t id)
{
	//return acts_dma_get_transfer_length(id);
	return 0;
}

extern int rom_dma_get_hdma_handle_fix(rom_dma_data_t *p_rom_data, int handle);

static int dma_acts_get_phy_channel(struct device *dev, u32_t id)
{
    return rom_dma_get_hdma_handle_fix(rom_dma_global_p, id);
}

int acts_dma_get_samples(int handle)
{
    return rom_dma_global_p->dma_cb->get_samples_bytes(rom_dma_global_p, (dma_reg_t *)handle);
}


int acts_dma_get_samples_param(int handle, unsigned int *framelen,unsigned int *remainlen, unsigned short *reload_count)
{
	return rom_dma_global_p->dma_cb->get_samples_info(rom_dma_global_p, (dma_reg_t *)handle, framelen, remainlen, reload_count);
}


int acts_dma_pre_start(int handle)
{
	return rom_dma_global_p->dma_cb->prestart(rom_dma_global_p, handle);
}

void acts_direct_dma_stop(u32_t dma_no)
{
	rom_dma_global_p->dma_cb->abort_by_num(dma_no);
}


void acts_dma_stop(dma_reg_t *dma_reg)
{
    rom_dma_global_p->dma_cb->asrc_dma_stop(dma_reg);
}

void acts_dma_pause(dma_regs_backup_t *backup_ptr, dma_reg_t *dma_reg)
{
	backup_ptr->dma_ie = sys_read32(DMAIE);
	backup_ptr->ctrl = dma_reg->ctl;

	rom_dma_global_p->dma_cb->dma_stop(dma_reg);

	backup_ptr->saddr0 = dma_reg->saddr0;
	backup_ptr->saddr1 = dma_reg->saddr1;
	backup_ptr->daddr0 = dma_reg->daddr0;
	backup_ptr->daddr1 = dma_reg->daddr1;
	backup_ptr->frame_remainlen = dma_reg->frame_remainlen;
	backup_ptr->framelen = dma_reg->framelen;

}

void acts_dma_resume(dma_regs_backup_t *backup_ptr, dma_reg_t *dma_reg)
{
	dma_reg->saddr0 = backup_ptr->saddr0;
	dma_reg->saddr1 = backup_ptr->saddr1;
	dma_reg->daddr0 = backup_ptr->daddr0;
	dma_reg->daddr1 = backup_ptr->daddr1;
	dma_reg->framelen = backup_ptr->framelen;
	dma_reg->ctl = backup_ptr->ctrl;

	sys_write32(backup_ptr->dma_ie, DMAIE);

	dma_reg->start = 0x1;
}

const struct dma_driver_api dma_acts_driver_api = {
	.config			= dma_acts_config,
	.start			= dma_acts_start,
	.stop			= dma_acts_stop,
	.get_remain_size	= dma_acts_get_remain_size,
	.reload			= dma_acts_reload,
	.request		= dma_acts_request,
	.free			= dma_acts_free,
	.wait_timeout   = dma_acts_wait_timeout,
	.prepare_start  = dma_acts_prepare_start,
	.get_transfer_length = dma_acts_get_transfer_length,
	.get_phy_dma_channel = dma_acts_get_phy_channel,
};

void dma_irq_handle(unsigned int irq)
{
	rom_dma_global_p->dma_cb->irq_handle(rom_dma_global_p, irq);
}

int dma_acts_init(struct device *dev)
{
	rom_dma_global_p = &rom_dma_global_data;
	rom_dma_data_t *rom_data = (rom_dma_data_t *) rom_dma_global_p;
	int i;

	acts_clock_peripheral_enable(CLOCK_ID_DMA);
	acts_reset_peripheral(RESET_ID_DMA);

	rom_data->dmacs = dmacs;
	rom_data->dma_cb = (dma_rom_callback_t *)rom_get_dma_drv_api();
	sys_slist_init(&rom_data->dma_req_list);
	//rom_data->irq_save = _arch_irq_lock;
	//rom_data->irq_restore = _arch_irq_unlock;



	/* FIXME: use CONFIG_VDMA_ACTS_PCHAN_NUM */
#if (CONFIG_VDMA_ACTS_PCHAN_START != 2) || (CONFIG_VDMA_ACTS_PCHAN_NUM != 6)
#error dma_acts_init: Invalid physical dma channel config
#endif

    //DMA0分配给BTC, DMA1分配给打印，还剩一路固定的DMA2可以给到memset?
    //IRQ_CONNECT(IRQ_ID_DMA1, CONFIG_IRQ_PRIO_HIGH, dma_fixed_acts_irq, IRQ_ID_DMA1, 0);
    //irq_enable(IRQ_ID_DMA1);

    IRQ_CONNECT(IRQ_ID_DMA2, CONFIG_IRQ_PRIO_HIGH, dma_irq_handle, IRQ_ID_DMA2, 0);
	IRQ_CONNECT(IRQ_ID_DMA3, CONFIG_IRQ_PRIO_HIGH, dma_irq_handle, IRQ_ID_DMA3, 0);
	IRQ_CONNECT(IRQ_ID_DMA4, CONFIG_IRQ_PRIO_HIGH, dma_irq_handle, IRQ_ID_DMA4, 0);
	IRQ_CONNECT(IRQ_ID_DMA5, CONFIG_IRQ_PRIO_HIGH, dma_irq_handle, IRQ_ID_DMA5, 0);
	IRQ_CONNECT(IRQ_ID_DMA6, CONFIG_IRQ_PRIO_HIGH, dma_irq_handle, IRQ_ID_DMA6, 0);
	IRQ_CONNECT(IRQ_ID_DMA7, CONFIG_IRQ_PRIO_HIGH, dma_irq_handle, IRQ_ID_DMA7, 0);

	for (i = CONFIG_VDMA_ACTS_PCHAN_START;
	     i < (CONFIG_VDMA_ACTS_PCHAN_NUM + CONFIG_VDMA_ACTS_PCHAN_START);
	     i++) {
		install_dma_phy_channel(i);
	}

	return 0;
}


DEVICE_AND_API_INIT(vdma_acts_andes, CONFIG_DMA_0_NAME, dma_acts_init,
		    NULL, NULL,
		    PRE_KERNEL_1, CONFIG_DMA_ACTS_DEVICE_INIT_PRIORITY,
		    &dma_acts_driver_api);

static void dma_acts_irq(int irq, int type, void *pdata)
{
	struct dma_wrapper_data *wrapper_data = (struct dma_wrapper_data *)pdata;

	wrapper_data->dma_callback((struct device *)DEVICE_GET(vdma_acts_andes),
				   (u32_t)wrapper_data->callback_data,
				   type);
}

extern dma_reg_t *dma_num_to_reg(int dma_no);

void dma_fixed_acts_irq(int irq)
{
	int i;
	int phy_dma_no = irq - IRQ_ID_DMA0;
	uint32_t pending;

	struct dma_wrapper_data *p_wrapper_data = NULL;

    //只清除这一路的dma pending位
    pending = (sys_read32(DMAIP) & (0x101 << phy_dma_no));

    sys_write32(pending, DMAIP);

	for (i = 0; i < CONFIG_VDMA_ACTS_VCHAN_NUM; i++) {
		if (wrapper_data[i].dma_handle == (int)dma_num_to_reg(phy_dma_no)){
		    p_wrapper_data = &wrapper_data[i];
        }
	}

    if (p_wrapper_data){
        if (pending & (0x100 << phy_dma_no)){
            p_wrapper_data->dma_callback((struct device *)DEVICE_GET(vdma_acts_andes),
                   (u32_t)p_wrapper_data->callback_data,
                   DMA_IRQ_HF);
        }

        if (pending & (0x1 << phy_dma_no)){
            p_wrapper_data->dma_callback((struct device *)DEVICE_GET(vdma_acts_andes),
                   (u32_t)p_wrapper_data->callback_data,
                   DMA_IRQ_TC);
        }
    }
}
