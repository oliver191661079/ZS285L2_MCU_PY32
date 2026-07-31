/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief dma controller driver for Actions SoC
 */

#include <board.h>
#include <device.h>
#include <dma.h>
#include <errno.h>
#include <init.h>
#include <soc.h>

#define SYS_LOG_DOMAIN "DMA"
#define SYS_LOG_LEVEL CONFIG_SYS_LOG_DMA_LEVEL
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_DMA_ACTS_ANDES

#define DMA_INT_HALF_COMP_SHIFT			8
#define DMA_INT_FULL_COMP_SHIFT			0

#define DMA_INT_HALF_COMP(id)			(BIT(id) << DMA_INT_HALF_COMP_SHIFT)
#define DMA_INT_FULL_COMP(id)			(BIT(id) << DMA_INT_FULL_COMP_SHIFT)

#define DMA_CTL_SRC_TYPE_SHIFT			0
#define DMA_CTL_SRC_TYPE(x)			((x) << DMA_CTL_SRC_TYPE_SHIFT)
#define DMA_CTL_SRC_TYPE_MASK			DMA_CTL_SRC_TYPE(0xf)
#define DMA_CTL_SAM_CONSTANT			(0x1 << 4)
#define DMA_CTL_DST_TYPE_SHIFT			8
#define DMA_CTL_DST_TYPE(x)			((x) << DMA_CTL_DST_TYPE_SHIFT)
#define DMA_CTL_DST_TYPE_MASK			DMA_CTL_DST_TYPE(0xf)
#define DMA_CTL_DAM_CONSTANT			(0x1 << 12)
#define DMA_CTL_TWS_SHIFT			13
#define DMA_CTL_TWS(x)				((x) << DMA_CTL_TWS_SHIFT)
#define DMA_CTL_TWS_MASK			DMA_CTL_TWS(0x3)
#define DMA_CTL_TWS_8BIT			DMA_CTL_TWS(3)
#define DMA_CTL_TWS_16BIT			DMA_CTL_TWS(2)
#define DMA_CTL_TWS_32BIT			DMA_CTL_TWS(1)
#define DMA_CTL_TWS_64BIT			DMA_CTL_TWS(0)
#define DMA_CTL_RELOAD				(0x1 << 15)
#define DMA_CTL_AUDIOTYPE_SEPERATED_BUF		(0x1 << 16)
#define DMA_CTL_TRM_SHIFT			17
#define DMA_CTL_TRM(x)				((x) << DMA_CTL_TRM_SHIFT)
#define DMA_CTL_TRM_MASK			DMA_CTL_TRM(0x1)
#define DMA_CTL_TRM_BURST8			DMA_CTL_TRM(0)
#define DMA_CTL_TRM_SINGLE			DMA_CTL_TRM(1)

#define DMA_START_START				(0x1 << 0)

/* dma global registers */
struct acts_dma_global_reg {
	volatile u32_t dma_ip;
	volatile u32_t dma_ie;
	volatile u32_t dma_timeout_ip;
};

/* dma channel registers */
struct acts_dma_chan_reg {
	volatile u32_t ctl;
	volatile u32_t start;
	volatile u32_t saddr0;
	volatile u32_t saddr1;
	volatile u32_t daddr0;
	volatile u32_t daddr1;
	volatile u32_t len;
	volatile u32_t remain_len;
};

struct acts_dma_chan {
	bool busy;
	void (*dma_callback)(struct device *dev, u32_t id,
			     int error_code);
	void *callback_data;

	u32_t  complete_callback_en		: 1;
	u32_t  half_complete_callback_en	: 1;
	u32_t  error_callback_en		: 1;
	u32_t  reserved				: 29;
};

struct acts_dma_device {
	void *base;
	struct acts_dma_chan chan[DMA_ACTS_MAX_CHANNELS];
};

struct acts_dma_config {
	void (*config)(struct acts_dma_device *);
};

static struct acts_dma_device dma_acts_ddata;

#if 0
static void dma_acts_dump_reg(struct acts_dma_device *ddev, u32_t id)
{
	struct acts_dma_chan_reg *cregs = DMA_CHAN(ddev->base, id);

	ACT_LOG_ID_INF(ALF_STR_XXX__USING_CHANNEL_D, 1, id);
	ACT_LOG_ID_INF(ALF_STR_XXX____DMA_CTL______0XX, 1, cregs->ctl);
	ACT_LOG_ID_INF(ALF_STR_XXX____DMA_SADDR0____0XX, 1, cregs->saddr0);
	ACT_LOG_ID_INF(ALF_STR_XXX____DMA_SADDR1____0XX, 1, cregs->saddr1);
	ACT_LOG_ID_INF(ALF_STR_XXX____DMA_DADDR0____0XX, 1, cregs->daddr0);
	ACT_LOG_ID_INF(ALF_STR_XXX____DMA_DADDR1____0XX, 1, cregs->daddr1);
	ACT_LOG_ID_INF(ALF_STR_XXX____DMA_LEN_______0XX, 1, cregs->len);
	ACT_LOG_ID_INF(ALF_STR_XXX____DMA_RMAIN_LEN_0XX, 1, cregs->remain_len);
}
#endif

static int dma_acts_config(struct device *dev, u32_t id,
			    struct dma_config *config)
{
	struct acts_dma_device *ddev = dev->driver_data;
	struct acts_dma_chan *chan = &ddev->chan[id];
	struct acts_dma_chan_reg *cregs = DMA_CHAN(ddev->base, id);
	struct dma_block_config *head_block = config->head_block;
	u32_t ctl;
	int data_width = 0;

	if (id >= DMA_ACTS_MAX_CHANNELS) {
		return -EINVAL;
	}

	if (head_block->block_size > DMA_ACTS_MAX_DATA_ITEMS) {
		SYS_LOG_ERR("DMA error: Data size too big: %d",
		       head_block->block_size);
		return -EINVAL;
	}

	if (config->complete_callback_en || config->half_complete_callback_en ||
	    config->error_callback_en) {
		chan->dma_callback = config->dma_callback;
		chan->callback_data = config->callback_data;

		chan->complete_callback_en = config->complete_callback_en;
		chan->half_complete_callback_en = config->half_complete_callback_en;
		chan->error_callback_en = config->error_callback_en;
	} else {
		chan->dma_callback = NULL;
	}

	cregs->saddr0 = (u32_t)head_block->source_address;
	cregs->daddr0 = (u32_t)head_block->dest_address;
	cregs->len = (u32_t)head_block->block_size;

	if (config->channel_direction == MEMORY_TO_PERIPHERAL) {
		ctl = DMA_CTL_SRC_TYPE(DMA_ID_MEMORY) |
		      DMA_CTL_DST_TYPE(config->dma_slot) |
		      DMA_CTL_DAM_CONSTANT;
	} else if (config->channel_direction == PERIPHERAL_TO_MEMORY)  {
		ctl = DMA_CTL_SRC_TYPE(config->dma_slot) |
		      DMA_CTL_SAM_CONSTANT |
		      DMA_CTL_DST_TYPE(DMA_ID_MEMORY);
	} else {
		ctl = DMA_CTL_SRC_TYPE(DMA_ID_MEMORY) |
		      DMA_CTL_DST_TYPE(DMA_ID_MEMORY);
	}
	/** extern for actions dma interleaved mode */
	if (config->reserved == 1 && config->channel_direction == MEMORY_TO_PERIPHERAL) {
		cregs->saddr1 = (u32_t)head_block->source_address;
		ctl |= DMA_CTL_AUDIOTYPE_SEPERATED_BUF;
	}else if(config->reserved == 1 && config->channel_direction == PERIPHERAL_TO_MEMORY) {
		cregs->daddr1 = (u32_t)head_block->dest_address;
		ctl |= DMA_CTL_AUDIOTYPE_SEPERATED_BUF;
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
	case 1:
	default:
		ctl |= DMA_CTL_TWS_8BIT;
		break;
	}

	cregs->ctl = ctl;

	return 0;
}

static int dma_acts_reload(struct device *dev, u32_t id, u32_t saddr,
		      u32_t daddr, u32_t frame_len)
{
	struct acts_dma_device *ddev = dev->driver_data;
	struct acts_dma_chan_reg *cregs = DMA_CHAN(ddev->base, id);
	cregs->saddr0 = saddr;
	cregs->daddr0 = daddr;
	cregs->len = frame_len;
	return 0;
}
void dma_acts_irq(u32_t id)
{
	struct acts_dma_device *ddev = &dma_acts_ddata;
	struct device *dev = CONTAINER_OF(ddev, struct device, driver_data);
	struct acts_dma_global_reg *gregs = DMA_GLOBAL(ddev->base);
	struct acts_dma_chan *chan = &ddev->chan[id];
	u32_t hf_pending, tc_pending;

	if (id >= DMA_ACTS_MAX_CHANNELS)
		return;

	hf_pending = (1 << (id + DMA_INT_HALF_COMP_SHIFT)) &
		  gregs->dma_ip & gregs->dma_ie;

	tc_pending = (1 << (id + DMA_INT_FULL_COMP_SHIFT)) &
		  gregs->dma_ip & gregs->dma_ie;

	/* clear pending */
	gregs->dma_ip = hf_pending | tc_pending;

	/* process half complete callback */
	if (hf_pending && chan->half_complete_callback_en &&
	    chan->dma_callback) {
		if (chan->callback_data)
			chan->dma_callback(dev, (u32_t)chan->callback_data, DMA_IRQ_HF);
		else
			chan->dma_callback(dev, id, DMA_IRQ_HF);
	}

	/* process full complete callback */
	if (tc_pending && chan->complete_callback_en &&
	    chan->dma_callback) {
		if (chan->callback_data)
			chan->dma_callback(dev, (u32_t)chan->callback_data, DMA_IRQ_TC);
		else
			chan->dma_callback(dev, id, DMA_IRQ_TC);
	}
}

static int dma_acts_start(struct device *dev, u32_t id)
{
	struct acts_dma_device *ddev = dev->driver_data;
	struct acts_dma_global_reg *gregs = DMA_GLOBAL(ddev->base);
	struct acts_dma_chan_reg *cregs = DMA_CHAN(ddev->base, id);
	struct acts_dma_chan *chan = &ddev->chan[id];
	u32_t key;

	if (id >= DMA_ACTS_MAX_CHANNELS) {
		return -EINVAL;
	}

	key = irq_lock();

	/* clear old irq pending */
	gregs->dma_ip = DMA_INT_HALF_COMP(id) | DMA_INT_FULL_COMP(id);

	gregs->dma_ie &= ~(DMA_INT_HALF_COMP(id) | DMA_INT_FULL_COMP(id));

	/* enable dma channel full complete irq? */
	if (chan->complete_callback_en) {
		gregs->dma_ie |= DMA_INT_FULL_COMP(id);
	}

	/* enable dma channel half complete irq? */
	if (chan->half_complete_callback_en) {
		gregs->dma_ie |= DMA_INT_HALF_COMP(id);
	}

	/* start dma transfer */
	cregs->start |= DMA_START_START;

	irq_unlock(key);

	return 0;
}

static int dma_acts_stop(struct device *dev, u32_t id)
{
	struct acts_dma_device *ddev = dev->driver_data;
	struct acts_dma_global_reg *gregs = DMA_GLOBAL(ddev->base);
	struct acts_dma_chan_reg *cregs = DMA_CHAN(ddev->base, id);
	u32_t key;

	if (id >= DMA_ACTS_MAX_CHANNELS) {
		return -EINVAL;
	}

	key = irq_lock();

	/* disable dma channel irq */
	gregs->dma_ie &= ~(DMA_INT_FULL_COMP(id) | DMA_INT_HALF_COMP(id));

	/* clear dma channel irq pending */
	gregs->dma_ip = DMA_INT_FULL_COMP(id) | DMA_INT_HALF_COMP(id);

	/* disable reload brefore stop dma */
	cregs->ctl &= ~DMA_CTL_RELOAD;

	cregs->start &= ~DMA_START_START;

	irq_unlock(key);

	return 0;
}

u32_t dma_acts_get_remain_size(struct device *dev, u32_t id)
{
	struct acts_dma_device *ddev = dev->driver_data;
	struct acts_dma_chan_reg *cregs = DMA_CHAN(ddev->base, id);

	return cregs->remain_len;
}

int dma_acts_request(struct device *dev, u32_t id)
{
	struct acts_dma_device *ddev = dev->driver_data;
	int i;

	if (id < DMA_ACTS_MAX_CHANNELS) {
		if (ddev->chan[id].busy) {
			ACT_LOG_ID_ERR(ALF_STR_dma_acts_request__DMA_CHANNEL_D_IS_BUS, 1, id);
			return -EBUSY;
		}

		ddev->chan[id].busy = true;

		return id;
	} else {
		for (i = 0; i < DMA_ACTS_MAX_CHANNELS; i++) {
			if (!ddev->chan[i].busy) {
				ACT_LOG_ID_DBG(ALF_STR_dma_acts_request__ALLOCATE_DMA_CHANNEL, 1, i);

				ddev->chan[i].busy = true;
				return i;
			}
		}
	}

	return -EBUSY;
}

void dma_acts_free(struct device *dev, u32_t id)
{
	struct acts_dma_device *ddev = dev->driver_data;

	if (id >= DMA_ACTS_MAX_CHANNELS)
		return;

	ddev->chan[id].busy = false;
}

int dma_acts_init(struct device *dev)
{
	struct acts_dma_device *ddev = dev->driver_data;
	const struct acts_dma_config *config = dev->config->config_info;
	int i;

	acts_clock_peripheral_enable(CLOCK_ID_DMA);
	acts_reset_peripheral(RESET_ID_DMA);

	for (i = 0; i < DMA_ACTS_MAX_CHANNELS; i++) {
		ddev->chan[i].busy = false;
	}

#ifdef CONFIG_BT_CONTROLER_ANDES
	/* reserve DMA0 for bluetooth controller */
	ddev->chan[0].busy = true;
#endif

	ddev->base = (void *)DMA_REG_BASE;

	config->config(ddev);

	return 0;
}

const struct dma_driver_api dma_acts_driver_api = {
	.config		 = dma_acts_config,
	.reload		 = dma_acts_reload,
	.start		 = dma_acts_start,
	.stop		 = dma_acts_stop,
	.get_remain_size = dma_acts_get_remain_size,
	.request	 = dma_acts_request,
	.free		 = dma_acts_free,
};

static void dma_acts_init_config(struct acts_dma_device *ddev);

const struct acts_dma_config dma_acts_cdata = {
	.config = dma_acts_init_config,
};

DEVICE_AND_API_INIT(dma_acts, CONFIG_DMA_0_NAME, dma_acts_init,
		    &dma_acts_ddata, &dma_acts_cdata,
		    PRE_KERNEL_1, CONFIG_DMA_ACTS_DEVICE_INIT_PRIORITY,
		    &dma_acts_driver_api);

static void dma_acts_init_config(struct acts_dma_device *ddev)
{
	/* reserve DMA0 for bluetooth controller */
#ifndef CONFIG_BT_CONTROLER_ANDES
	IRQ_CONNECT(IRQ_ID_DMA0, CONFIG_DMA_0_IRQ_PRI,
		    dma_acts_irq, 0, 0);
	irq_enable(IRQ_ID_DMA0);
#endif

	IRQ_CONNECT(IRQ_ID_DMA1, CONFIG_IRQ_PRIO_NORMAL,
		    dma_acts_irq, 1, 0);
	irq_enable(IRQ_ID_DMA1);

	IRQ_CONNECT(IRQ_ID_DMA2, CONFIG_IRQ_PRIO_NORMAL,
		    dma_acts_irq, 2, 0);
	irq_enable(IRQ_ID_DMA2);

	IRQ_CONNECT(IRQ_ID_DMA3, CONFIG_IRQ_PRIO_NORMAL,
		    dma_acts_irq, 3, 0);
	irq_enable(IRQ_ID_DMA3);

	IRQ_CONNECT(IRQ_ID_DMA4, CONFIG_IRQ_PRIO_NORMAL,
		    dma_acts_irq, 4, 0);
	irq_enable(IRQ_ID_DMA4);

	IRQ_CONNECT(IRQ_ID_DMA5, CONFIG_IRQ_PRIO_NORMAL,
		    dma_acts_irq, 5, 0);
	irq_enable(IRQ_ID_DMA5);

	IRQ_CONNECT(IRQ_ID_DMA6, CONFIG_IRQ_PRIO_NORMAL,
		    dma_acts_irq, 6, 0);
	irq_enable(IRQ_ID_DMA6);

	IRQ_CONNECT(IRQ_ID_DMA7, CONFIG_IRQ_PRIO_NORMAL,
		    dma_acts_irq, 7, 0);
	irq_enable(IRQ_ID_DMA7);
}
