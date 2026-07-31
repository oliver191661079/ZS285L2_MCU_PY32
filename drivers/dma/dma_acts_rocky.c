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
#define ACT_LOG_MODEL_ID ALF_MODEL_DMA_ACTS_ROCKY

#define DMA_INT_HALF_COMP_SHIFT			8
#define DMA_INT_FULL_COMP_SHIFT			0

#define DMA_INT_HALF_COMP(id)			(BIT(id) << DMA_INT_HALF_COMP_SHIFT)
#define DMA_INT_FULL_COMP(id)			(BIT(id) << DMA_INT_FULL_COMP_SHIFT)

#define DMA_CTL_START				(0x1 << 0)
#define DMA_CTL_RELOAD				(0x1 << 1)
#define DMA_CTL_SRC_TYPE_SHIFT			4
#define DMA_CTL_SRC_TYPE(x)			((x) << DMA_CTL_SRC_TYPE_SHIFT)
#define DMA_CTL_SRC_TYPE_MASK			DMA_CTL_SRC_TYPE(0xf)
#define DMA_CTL_DST_TYPE_SHIFT			8
#define DMA_CTL_DST_TYPE(x)			((x) << DMA_CTL_DST_TYPE_SHIFT)
#define DMA_CTL_DST_TYPE_MASK			DMA_CTL_DST_TYPE(0xf)
#define DMA_CTL_TWS_SHIFT			12
#define DMA_CTL_TWS(x)				((x) << DMA_CTL_TWS_SHIFT)
#define DMA_CTL_TWS_MASK			DMA_CTL_TWS(0x1)
#define DMA_CTL_TWS_8BIT			DMA_CTL_TWS(0)
#define DMA_CTL_TWS_16BIT			DMA_CTL_TWS(0)
#define DMA_CTL_TWS_24BIT			DMA_CTL_TWS(1)
#define DMA_CTL_TWS_32BIT			DMA_CTL_TWS(1)

/* dma global registers */
struct acts_dma_global_reg {
	volatile u32_t dma_prio;
	volatile u32_t dma_ip;
	volatile u32_t dma_ie;
};

/* dma channel registers */
struct acts_dma_chan_reg {
	volatile u32_t ctl;
	volatile u32_t saddr;
	volatile u32_t daddr;
	volatile u32_t len;
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

#if 0
static void dma_acts_dump_reg(struct acts_dma_device *ddev, u32_t id)
{
	struct acts_dma_chan_reg *cregs = DMA_CHAN(ddev->base, id);

	ACT_LOG_ID_INF(ALF_STR_XXX__USING_CHANNEL_D, 1, id);
	ACT_LOG_ID_INF(ALF_STR_XXX____DMA_CTL______0XX, 1, cregs->ctl);
	ACT_LOG_ID_INF(ALF_STR_XXX____DMA_SADDR____0XX, 1, cregs->saddr);
	ACT_LOG_ID_INF(ALF_STR_XXX____DMA_DADDR____0XX, 1, cregs->daddr);
	ACT_LOG_ID_INF(ALF_STR_XXX____DMA_LEN______0XX, 1, cregs->len);
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

	cregs->saddr = (u32_t)head_block->source_address;
	cregs->daddr = (u32_t)head_block->dest_address;
	cregs->len = (u32_t)head_block->block_size;

	if (config->channel_direction == MEMORY_TO_PERIPHERAL) {
		ctl = DMA_CTL_SRC_TYPE(DMA_ID_MEMORY) |
		      DMA_CTL_DST_TYPE(config->dma_slot);
	} else if (config->channel_direction == PERIPHERAL_TO_MEMORY)  {
		ctl = DMA_CTL_SRC_TYPE(config->dma_slot) |
		      DMA_CTL_DST_TYPE(DMA_ID_MEMORY);
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
	case 3:
		ctl |= DMA_CTL_TWS_24BIT;
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

void dma_acts_irq(struct device *dev)
{
	struct acts_dma_device *ddev = dev->driver_data;
	struct acts_dma_global_reg *gregs = DMA_GLOBAL(ddev->base);
	struct acts_dma_chan *chan;
	u32_t pending, id;
	int reason;

	pending = gregs->dma_ip & gregs->dma_ie;

	while (pending) {
		/* first process half-compete irq */
		id = find_msb_set(pending) - 1;

		pending &= ~BIT(id);
		/* clear irq pending */
		gregs->dma_ip = BIT(id);

		if (id >= DMA_INT_HALF_COMP_SHIFT) {
			id -= DMA_INT_HALF_COMP_SHIFT;
			reason = DMA_IRQ_HF;
		} else {
			reason = DMA_IRQ_TC;
		}

		chan = &ddev->chan[id];
		if ((chan->dma_callback) &&
		    ((chan->complete_callback_en && reason == DMA_IRQ_TC) ||
		    (chan->half_complete_callback_en && reason == DMA_IRQ_HF))) {		
			if (chan->callback_data) {
				chan->dma_callback(dev, (u32_t)chan->callback_data, reason);
			} else {
				chan->dma_callback(dev, id, reason);
			}
		}
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
	cregs->ctl |= DMA_CTL_START;

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

	cregs->ctl &= ~DMA_CTL_START;

	irq_unlock(key);

	return 0;
}

u32_t dma_acts_get_remain_size(struct device *dev, u32_t id)
{
	struct acts_dma_device *ddev = dev->driver_data;
	u32_t remain_reg;

	remain_reg = (u32_t)DMA_CHAN_REMAIN_LEN(ddev->base, id);

	return sys_read32(remain_reg);
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

	ddev->base = (void *)DMA_REG_BASE;

	config->config(ddev);

	return 0;
}

const struct dma_driver_api dma_acts_driver_api = {
	.config		 = dma_acts_config,
	.start		 = dma_acts_start,
	.stop		 = dma_acts_stop,
	.get_remain_size = dma_acts_get_remain_size,
};

static void dma_acts_init_config(struct acts_dma_device *ddev);

const struct acts_dma_config dma_acts_cdata = {
	.config = dma_acts_init_config,
};

static struct acts_dma_device dma_acts_ddata;

DEVICE_AND_API_INIT(dma_acts, CONFIG_DMA_0_NAME, dma_acts_init,
		    &dma_acts_ddata, &dma_acts_cdata,
		    PRE_KERNEL_1, CONFIG_DMA_ACTS_DEVICE_INIT_PRIORITY,
		    &dma_acts_driver_api);

static void dma_acts_init_config(struct acts_dma_device *ddev)
{
	IRQ_CONNECT(IRQ_ID_DMA0, CONFIG_DMA_0_IRQ_PRI,
		    dma_acts_irq, DEVICE_GET(dma_acts), 0);
	irq_enable(IRQ_ID_DMA0);

	IRQ_CONNECT(IRQ_ID_DMA1, CONFIG_DMA_0_IRQ_PRI,
		    dma_acts_irq, DEVICE_GET(dma_acts), 0);
	irq_enable(IRQ_ID_DMA1);

	IRQ_CONNECT(IRQ_ID_DMA2, CONFIG_DMA_0_IRQ_PRI,
		    dma_acts_irq, DEVICE_GET(dma_acts), 0);
	irq_enable(IRQ_ID_DMA2);

	IRQ_CONNECT(IRQ_ID_DMA3, CONFIG_DMA_0_IRQ_PRI,
		    dma_acts_irq, DEVICE_GET(dma_acts), 0);
	irq_enable(IRQ_ID_DMA3);

	IRQ_CONNECT(IRQ_ID_DMA4, CONFIG_DMA_0_IRQ_PRI,
		    dma_acts_irq, DEVICE_GET(dma_acts), 0);
	irq_enable(IRQ_ID_DMA4);
}
