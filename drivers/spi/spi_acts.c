/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief SPI driver for Actions SoC
 */

#define SYS_LOG_DOMAIN "SPI"
#define SYS_LOG_LEVEL CONFIG_SYS_LOG_SPI_LEVEL
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_SPI_ACTS

#include <errno.h>
#include <kernel.h>
#include <board.h>
#include <device.h>
#include <init.h>
#include <misc/util.h>
#include <dma.h>
#include <spi.h>
#include <soc.h>
#include "spi_context.h"

/* SPI registers macros*/
#define SPI_CTL_CLK_SEL_MASK		(0x1 << 31)
#define SPI_CTL_CLK_SEL_CPU		(0x0 << 31)
#define SPI_CTL_CLK_SEL_DMA		(0x1 << 31)

#define SPI_CTL_FIFO_WIDTH_MASK		(0x1 << 30)
#define SPI_CTL_FIFO_WIDTH_8BIT		(0x0 << 30)
#define SPI_CTL_FIFO_WIDTH_32BIT	(0x1 << 30)

#define SPI_CTL_MODE_MASK		(3 << 28)
#define SPI_CTL_MODE(x)			((x) << 28)
#define SPI_CTL_MODE_CPHA		(1 << 28)
#define SPI_CTL_MODE_CPOL		(1 << 29)

#define SPI_CTL_MS_SEL_MASK		(0x1 << 22)
#define SPI_CTL_MS_SEL_MASTER		(0x0 << 22)
#define SPI_CTL_MS_SEL_SLAVE		(0x1 << 22)

#define SPI_CTL_SB_SEL_MASK		(0x1 << 21)
#define SPI_CTL_SB_SEL_MSB		(0x0 << 21)
#define SPI_CTL_SB_SEL_LSB		(0x1 << 21)

#define SPI_CTL_RXW_DELAY_MASK		(0x1 << 20)
#define SPI_CTL_RXW_DELAY_2CYCLE	(0x0 << 20)
#define SPI_CTL_RXW_DELAY_3CYCLE	(0x1 << 20)

#define SPI_CTL_DELAYCHAIN_MASK		(0xf << 16)
#define SPI_CTL_DELAYCHAIN_SHIFT	(16)
#define SPI_CTL_DELAYCHAIN(x)		((x) << 16)

#ifdef CONFIG_SOC_SERIES_ROCKY
#define SPI_CTL_TX_DRQ_EN		(1 << 11)
#define SPI_CTL_RX_DRQ_EN		(1 << 10)
#else
#define SPI_CTL_TX_DRQ_EN		(1 << 7)
#define SPI_CTL_RX_DRQ_EN		(1 << 6)
#endif

#define SPI_CTL_TX_IRQ_EN		(1 << 9)
#define SPI_CTL_RX_IRQ_EN		(1 << 8)
#define SPI_CTL_TX_FIFO_EN		(1 << 5)
#define SPI_CTL_RX_FIFO_EN		(1 << 4)
#define SPI_CTL_SS			(1 << 3)
#define SPI_CTL_LOOP			(1 << 2)
#define SPI_CTL_WR_MODE_MASK		(0x3 << 0)
#define SPI_CTL_WR_MODE_DISABLE		(0x0 << 0)
#define SPI_CTL_WR_MODE_READ		(0x1 << 0)
#define SPI_CTL_WR_MODE_WRITE		(0x2 << 0)
#define SPI_CTL_WR_MODE_READ_WRITE	(0x3 << 0)

#define SPI_STATUS_TX_FIFO_WERR		(1 << 11)
#define SPI_STATUS_RX_FIFO_WERR		(1 << 9)
#define SPI_STATUS_RX_FIFO_RERR		(1 << 8)
#define SPI_STATUS_RX_FULL		(1 << 7)
#define SPI_STATUS_RX_EMPTY		(1 << 6)
#define SPI_STATUS_TX_FULL		(1 << 5)
#define SPI_STATUS_TX_EMPTY		(1 << 4)
#define SPI_STATUS_TX_IRQ_PD		(1 << 3)
#define SPI_STATUS_RX_IRQ_PD		(1 << 2)
#define SPI_STATUS_BUSY			(1 << 0)
#define SPI_STATUS_ERR_MASK		(SPI_STATUS_RX_FIFO_RERR |	\
					 SPI_STATUS_RX_FIFO_WERR |	\
					 SPI_STATUS_TX_FIFO_WERR)

#define SPI_DMA_STRANFER_MIN_LEN 8

struct acts_spi_controller
{
	volatile uint32_t ctrl;
	volatile uint32_t status;
	volatile uint32_t txdat;
	volatile uint32_t rxdat;
	volatile uint32_t bc;
} ;

struct acts_spi_config {
	struct acts_spi_controller *spi;
	u32_t spiclk_reg;
#ifdef CONFIG_SPI_ACTS_DMA
	u8_t txdma_id;
	u8_t rxdma_id;
#endif
	u8_t clock_id;
	u8_t reset_id;
	u8_t delay_chain;

	u8_t flag_use_dma:1;
	u8_t flag_use_dma_duplex:1;
	u8_t flag_reserved:6;
};

struct acts_spi_data {
	struct spi_context ctx;
#ifdef CONFIG_SPI_ACTS_DMA
	struct device *dma_dev;
	struct k_sem dma_sync;
	u32_t dma_chan0;
	/* use dma chan1 for full duplex mode */
	u32_t dma_chan1;
#endif
};

#define CONFIG_CFG(cfg)						\
((const struct acts_spi_config * const)(cfg)->dev->config->config_info)

#define CONFIG_DATA(cfg)					\
((struct acts_spi_data * const)(cfg)->dev->driver_data)

bool spi_acts_transfer_ongoing(struct acts_spi_data *data)
{
	return spi_context_tx_on(&data->ctx) || spi_context_rx_on(&data->ctx);
}

static void spi_acts_wait_tx_complete(struct acts_spi_controller *spi)
{
	while (!(spi->status & SPI_STATUS_TX_EMPTY))
		;

	/* wait until tx fifo is empty for master mode*/
	while (((spi->ctrl & SPI_CTL_MS_SEL_MASK) == SPI_CTL_MS_SEL_MASTER) &&
		spi->status & SPI_STATUS_BUSY)
		;
}

static void spi_acts_set_clk(const struct acts_spi_config *cfg, u32_t freq_khz)
{
	/* 根据实际 corepll 频率计算 SPI 分频
	 * soc_freq_get_corepll_freq() 返回 MHz！
	 */
	unsigned int corepll_khz = soc_freq_get_corepll_freq() * 1000;
	unsigned int sel;
	unsigned int reg_shift;
	int spi_id;

	if ((void *)cfg->spi == (void *)SPI0_REG_BASE)
		spi_id = 0;
	else if ((void *)cfg->spi == (void *)SPI1_REG_BASE)
		spi_id = 1;
	else
		spi_id = 2;

	reg_shift = spi_id * 8;

	if (corepll_khz > 0 && freq_khz > 0 && corepll_khz > freq_khz) {
		sel = (corepll_khz + freq_khz - 1) / freq_khz - 1;
		if (sel > 31) sel = 31;
	} else {
		sel = 5; /* fallback default */
	}

	sys_write32((sys_read32(CMU_SPICLK) & ~(0xff << reg_shift))
		    | ((0x80 | sel) << reg_shift), CMU_SPICLK);
	k_busy_wait(100);
}

#ifdef CONFIG_SPI_ACTS_DMA
static void dma_done_callback(struct device *dev, u32_t callback_data, int type)
{
	struct acts_spi_data *data = (struct acts_spi_data *)callback_data;

	if (type != DMA_IRQ_TC)
		return;

	ACT_LOG_ID_DBG(ALF_STR_dma_done_callback__SPI_DMA_TRANSFER_IS_, 0);

	k_sem_give(&data->dma_sync);
}

static int spi_acts_start_dma(const struct acts_spi_config *cfg,
				  struct acts_spi_data *data,
				  u32_t dma_chan,
				  u8_t *buf,
				  s32_t len,
				  bool is_tx,
				  void (*callback)(struct device *, u32_t, int))
{
	struct acts_spi_controller *spi = cfg->spi;
	struct dma_config dma_cfg = {0};
	struct dma_block_config dma_block_cfg = {0};

	if (callback) {
		dma_cfg.dma_callback = dma_done_callback;
		dma_cfg.callback_data = data;
		dma_cfg.complete_callback_en = 1;
	}

	dma_cfg.block_count = 1;
	dma_cfg.head_block = &dma_block_cfg;
	dma_block_cfg.block_size = len;
	if (is_tx) {
		dma_cfg.dma_slot = cfg->txdma_id;
		dma_cfg.channel_direction = MEMORY_TO_PERIPHERAL;
		dma_block_cfg.source_address = (u32_t)buf;
		dma_block_cfg.dest_address = (u32_t)&spi->txdat;
		dma_cfg.dest_data_size = 1;
	} else {
		dma_cfg.dma_slot = cfg->rxdma_id;
		dma_cfg.channel_direction = PERIPHERAL_TO_MEMORY;
		dma_block_cfg.source_address = (u32_t)&spi->rxdat;
		dma_block_cfg.dest_address = (u32_t)buf;
		dma_cfg.source_data_size = 1;
	}

	if (dma_config(data->dma_dev, dma_chan, &dma_cfg)) {
		SYS_LOG_ERR("dma%d config error", dma_chan);
		return -1;
	}

	if (dma_start(data->dma_dev, dma_chan)) {
		SYS_LOG_ERR("dma%d start error", dma_chan);
		return -1;
	}

	return 0;
}

static void spi_acts_stop_dma(const struct acts_spi_config *cfg,
			       struct acts_spi_data *data,
			       u32_t dma_chan)
{
	dma_stop(data->dma_dev, dma_chan);
}

static int spi_acts_read_data_by_dma(const struct acts_spi_config *cfg,
				     struct acts_spi_data *data,
				     u8_t *buf, s32_t len)
{
	struct acts_spi_controller *spi = cfg->spi;
	int ret;

	spi->bc = len;
	spi->ctrl = (spi->ctrl & ~SPI_CTL_WR_MODE_MASK) |
			SPI_CTL_WR_MODE_READ | SPI_CTL_CLK_SEL_DMA |
			SPI_CTL_RX_DRQ_EN;

	ret = spi_acts_start_dma(cfg, data, data->dma_chan0, buf, len,
				 false, dma_done_callback);
	if (ret) {
		SYS_LOG_ERR("faield to start dma chan 0x%x\n", data->dma_chan0);
		goto out;
	}

	/* wait until dma transfer is done */
	k_sem_take(&data->dma_sync, K_FOREVER);

out:
	spi_acts_stop_dma(cfg, data, data->dma_chan0);
	spi->ctrl = spi->ctrl & ~(SPI_CTL_CLK_SEL_DMA | SPI_CTL_RX_DRQ_EN);

	return ret;
}

static int spi_acts_write_data_by_dma(const struct acts_spi_config *cfg,
				      struct acts_spi_data *data,
				      const u8_t *buf, s32_t len)
{
	struct acts_spi_controller *spi = cfg->spi;
	int ret;

	spi->bc = len;
	spi->ctrl = (spi->ctrl & ~SPI_CTL_WR_MODE_MASK) |
			SPI_CTL_WR_MODE_WRITE | SPI_CTL_CLK_SEL_DMA |
			SPI_CTL_TX_DRQ_EN;

	ret = spi_acts_start_dma(cfg, data, data->dma_chan0, (u8_t *)buf, len,
				 true, dma_done_callback);
	if (ret) {
		SYS_LOG_ERR("faield to start dma chan0 0x%x\n", data->dma_chan0);
		goto out;
	}

	/* wait until dma transfer is done */
	k_sem_take(&data->dma_sync, K_FOREVER);

out:
	spi_acts_stop_dma(cfg, data, data->dma_chan0);
	spi->ctrl = spi->ctrl & ~(SPI_CTL_CLK_SEL_DMA | SPI_CTL_TX_DRQ_EN);

	return ret;
}

int spi_acts_write_read_data_by_dma(const struct acts_spi_config *cfg,
		struct acts_spi_data *data,
		const u8_t *tx_buf, u8_t *rx_buf, s32_t len)
{
	struct acts_spi_controller *spi = cfg->spi;
	int ret;

	spi->bc = len;
	spi->ctrl = (spi->ctrl & ~SPI_CTL_WR_MODE_MASK) |
			SPI_CTL_WR_MODE_READ_WRITE | SPI_CTL_CLK_SEL_DMA |
			SPI_CTL_TX_DRQ_EN | SPI_CTL_RX_DRQ_EN;

	ret = spi_acts_start_dma(cfg, data, data->dma_chan0, rx_buf, len,
				 false, dma_done_callback);
	if (ret) {
		SYS_LOG_ERR("faield to start dma chan0 0x%x\n", data->dma_chan0);
		goto out;
	}

	ret = spi_acts_start_dma(cfg, data, data->dma_chan1, (u8_t *)tx_buf, len,
				 true, NULL);
	if (ret) {
		SYS_LOG_ERR("faield to start dma chan1 0x%x\n", data->dma_chan1);
		goto out;
	}

	/* wait until dma transfer is done */
	k_sem_take(&data->dma_sync, K_FOREVER);

out:
	spi_acts_stop_dma(cfg, data, data->dma_chan0);
	spi_acts_stop_dma(cfg, data, data->dma_chan1);

	spi->ctrl = spi->ctrl & ~(SPI_CTL_CLK_SEL_DMA |
			SPI_CTL_TX_DRQ_EN | SPI_CTL_RX_DRQ_EN);

	return 0;
}
#endif	/* CONFIG_SPI_ACTS_DMA */

static int spi_acts_write_data_by_cpu(struct acts_spi_controller *spi,
		const u8_t *wbuf, s32_t len)
{
	int tx_len = 0;

	/* switch to write mode */
	spi->bc = len;
	spi->ctrl = (spi->ctrl & ~SPI_CTL_WR_MODE_MASK) | SPI_CTL_WR_MODE_WRITE;

	while (tx_len < len) {
		if(!(spi->status & SPI_STATUS_TX_FULL)) {
			spi->txdat = *wbuf++;
			tx_len++;
		}
	}

	return 0;
}

static int spi_acts_read_data_by_cpu(struct acts_spi_controller *spi,
		u8_t *rbuf, s32_t len)
{
	int rx_len = 0;

	/* switch to write mode */
	spi->bc = len;
	spi->ctrl = (spi->ctrl & ~SPI_CTL_WR_MODE_MASK) | SPI_CTL_WR_MODE_READ;

	while (rx_len < len) {
		if(!(spi->status & SPI_STATUS_RX_EMPTY)) {
			*rbuf++ = spi->rxdat;
			rx_len++;
		}
	}

	return 0;
}

static int spi_acts_write_read_data_by_cpu(struct acts_spi_controller *spi,
		const u8_t *wbuf, u8_t *rbuf, s32_t len)
{
	int rx_len = 0, tx_len = 0;

	/* switch to write mode */
	spi->bc = len;
	spi->ctrl = (spi->ctrl & ~SPI_CTL_WR_MODE_MASK) | SPI_CTL_WR_MODE_READ_WRITE;

	while (rx_len < len || tx_len < len) {
		while((tx_len < len) && !(spi->status & SPI_STATUS_TX_FULL)) {
			spi->txdat = *wbuf++;
			tx_len++;
		}

		while((rx_len < len) && !(spi->status & SPI_STATUS_RX_EMPTY)) {
			*rbuf++ = spi->rxdat;
			rx_len++;
		}
	}

	return 0;
}

int spi_acts_write_data(const struct acts_spi_config *cfg, struct acts_spi_data *data,
	u8_t *tx_buf, int len)
{
	int ret;

#ifdef CONFIG_SPI_ACTS_DMA
	if (cfg->flag_use_dma) {
		ret = spi_acts_write_data_by_dma(cfg, data,
			tx_buf, len);
	} else {
#endif
		ret = spi_acts_write_data_by_cpu(cfg->spi,
			tx_buf, len);
#ifdef CONFIG_SPI_ACTS_DMA
	}
#endif

	return ret;
}

int spi_acts_read_data(const struct acts_spi_config *cfg, struct acts_spi_data *data,
	u8_t *rx_buf, int len)
{
	int ret;

#ifdef CONFIG_SPI_ACTS_DMA
	if (cfg->flag_use_dma) {
		ret = spi_acts_read_data_by_dma(cfg, data,
			rx_buf, len);
	} else {
#endif
		ret = spi_acts_read_data_by_cpu(cfg->spi,
			rx_buf, len);
#ifdef CONFIG_SPI_ACTS_DMA
	}
#endif

	return ret;
}

int spi_acts_write_read_data(const struct acts_spi_config *cfg, struct acts_spi_data *data,
	u8_t *tx_buf, u8_t *rx_buf, int len)
{
	int ret;

#ifdef CONFIG_SPI_ACTS_DMA
	if ((cfg->flag_use_dma_duplex)) {
		ret = spi_acts_write_read_data_by_dma(cfg, data,
			tx_buf, rx_buf, len);
	} else {
#endif
		ret = spi_acts_write_read_data_by_cpu(cfg->spi,
			tx_buf, rx_buf, len);
#ifdef CONFIG_SPI_ACTS_DMA
	}
#endif

	return ret;
}

int spi_acts_transfer_data(const struct acts_spi_config *cfg, struct acts_spi_data *data)
{
	struct acts_spi_controller *spi = cfg->spi;
	struct spi_context *ctx = &data->ctx;
	int chunk_size;
	int ret = 0;

	chunk_size = spi_context_longest_current_buf(ctx);

	SYS_LOG_DBG("tx_len %d, rx_len %d, chunk_size %d",
		ctx->tx_len, ctx->rx_len, chunk_size);

	spi->ctrl |= SPI_CTL_RX_FIFO_EN | SPI_CTL_TX_FIFO_EN;

	if (ctx->tx_len && ctx->rx_len) {
		ret = spi_acts_write_read_data(cfg, data, ctx->tx_buf, ctx->rx_buf, chunk_size);
		spi_context_update_tx(ctx, 1, chunk_size);
		spi_context_update_rx(ctx, 1, chunk_size);
	} else if (ctx->tx_len) {
		ret = spi_acts_write_data(cfg, data, ctx->tx_buf, chunk_size);
		spi_context_update_tx(ctx, 1, chunk_size);
	} else {
		ret = spi_acts_read_data(cfg, data, ctx->rx_buf, chunk_size);
		spi_context_update_rx(ctx, 1, chunk_size);
	}

	if (!ret) {
		spi_acts_wait_tx_complete(spi);
		if (spi->status & SPI_STATUS_ERR_MASK) {
			ret = -EIO;
		}
	}

	if (ret) {
		SYS_LOG_ERR("spi(%p) transfer error: ctrl: 0x%x, status: 0x%x",
			    spi, spi->ctrl, spi->status);
	}

	spi->ctrl = (spi->ctrl & ~SPI_CTL_WR_MODE_MASK) | SPI_CTL_WR_MODE_DISABLE;
	spi->status |= SPI_STATUS_ERR_MASK;

	return ret;
}

int spi_acts_configure(const struct acts_spi_config *cfg,
			    struct acts_spi_data *spi,
			    struct spi_config *config)
{
	u32_t ctrl, word_size;
	u32_t op = config->operation;

	ACT_LOG_ID_DBG(ALF_STR_spi_acts_configure__08X_PREV_08X_OP_0XX, 3, config, spi->ctx.config, op);

	ctrl = SPI_CTL_DELAYCHAIN(cfg->delay_chain);

	if (spi_context_configured(&spi->ctx, config)) {
		/* Nothing to do */
		return 0;
	}

	spi_acts_set_clk(cfg, config->frequency / 1000);

	if (op & (SPI_LINES_DUAL | SPI_LINES_QUAD))
		return -EINVAL;

	if (SPI_OP_MODE_SLAVE == SPI_OP_MODE_GET(op))
		ctrl |= SPI_CTL_MS_SEL_SLAVE;

	word_size = SPI_WORD_SIZE_GET(op);
	if (word_size == 8)
		ctrl |= SPI_CTL_FIFO_WIDTH_8BIT;
	else if (word_size == 32)
		ctrl |= SPI_CTL_FIFO_WIDTH_32BIT;
	else
		ctrl |= SPI_CTL_FIFO_WIDTH_8BIT;

	if (op & SPI_MODE_CPOL)
		ctrl |= SPI_CTL_MODE_CPOL;

	if (op & SPI_MODE_CPHA)
		ctrl |= SPI_CTL_MODE_CPHA;

	if (op & SPI_MODE_LOOP)
		ctrl |= SPI_CTL_LOOP;

	if (op & SPI_TRANSFER_LSB)
		ctrl |= SPI_CTL_SB_SEL_LSB;

	cfg->spi->ctrl = ctrl;

	/* At this point, it's mandatory to set this on the context! */
	spi->ctx.config = config;

	spi_context_cs_configure(&spi->ctx);

	return 0;
}

int transceive(struct spi_config *config,
		      const struct spi_buf *tx_bufs,
		      size_t tx_count,
		      struct spi_buf *rx_bufs,
		      size_t rx_count,
		      bool asynchronous,
		      struct k_poll_signal *signal)
{
	const struct acts_spi_config *cfg = CONFIG_CFG(config);
	struct acts_spi_data *data = CONFIG_DATA(config);
	struct acts_spi_controller *spi = cfg->spi;
	int ret;

	if (!tx_count && !rx_count) {
		return 0;
	}

	spi_context_lock(&data->ctx, asynchronous, signal);

	/* Configure */
	ret = spi_acts_configure(cfg, data, config);
	if (ret) {
		goto out;
	}

	/* Set buffers info */
	spi_context_buffers_setup(&data->ctx, tx_bufs, tx_count,
				  rx_bufs, rx_count, 1);

	/* assert chip select */
	if (SPI_OP_MODE_MASTER == SPI_OP_MODE_GET(config->operation)) {
		if (data->ctx.config->cs) {
			spi_context_cs_control(&data->ctx, true);
		} else {
			spi->ctrl &= ~SPI_CTL_SS;
		}
	}

	do {
		ret = spi_acts_transfer_data(cfg, data);
	} while (!ret && spi_acts_transfer_ongoing(data));

	/* deassert chip select */
	if (SPI_OP_MODE_MASTER == SPI_OP_MODE_GET(config->operation)) {
		if (data->ctx.config->cs) {
			spi_context_cs_control(&data->ctx, false);
		} else {
			spi->ctrl |= SPI_CTL_SS;
		}
	}

out:
	spi_context_release(&data->ctx, ret);

	return ret;
}

static int spi_acts_transceive(struct spi_config *config,
			     const struct spi_buf *tx_bufs,
			     size_t tx_count,
			     struct spi_buf *rx_bufs,
			     size_t rx_count)
{
	SYS_LOG_DBG("%p, %p (%zu), %p (%zu)",
		    config->dev, tx_bufs, tx_count, rx_bufs, rx_count);

	return transceive(config, tx_bufs, tx_count,
			  rx_bufs, rx_count, false, NULL);
}


static int spi_acts_release(struct spi_config *config)
{
	struct acts_spi_data *data = CONFIG_DATA(config);

	spi_context_unlock_unconditionally(&data->ctx);

	return 0;
}

const struct spi_driver_api acts_spi_driver_api = {
	.transceive = spi_acts_transceive,
	.release = spi_acts_release,
};

int spi_acts_init(struct device *dev)
{
	const struct acts_spi_config *config = dev->config->config_info;
	struct acts_spi_data *data = dev->driver_data;

#ifdef CONFIG_SPI_ACTS_DMA
	data->dma_dev = device_get_binding(CONFIG_DMA_0_NAME);
	if (!data->dma_dev)
		return -ENODEV;

	k_sem_init(&data->dma_sync, 0, 1);

	if (config->flag_use_dma) {
		data->dma_chan0 = dma_request(data->dma_dev, 0xff);
		ACT_LOG_ID_INF(ALF_STR_spi_acts_init__SPI_BASE_0XX_DMA_CHA, 2, config->spi, data->dma_chan0);
		if (data->dma_chan0 == 0) {
			return -ENODEV;
		}

		if (config->flag_use_dma_duplex) {
			data->dma_chan1 = dma_request(data->dma_dev, 0xff);
			ACT_LOG_ID_INF(ALF_STR_spi_acts_init__DMA_CHAN1_0XXN, 1, data->dma_chan1);
			if (data->dma_chan1 == 0) {
				return -ENODEV;
			}
		}
	}
#endif

	/* enable spi controller clock */
	acts_clock_peripheral_enable(config->clock_id);

	/* reset spi controller */
	acts_reset_peripheral(config->reset_id);

	spi_context_unlock_unconditionally(&data->ctx);

	//dev->driver_api = &acts_spi_driver_api;

	return 0;
}

#ifdef CONFIG_SPI_1
struct acts_spi_data spi_acts_data_port_1 = {
	SPI_CONTEXT_INIT_LOCK(spi_acts_data_port_1, ctx),
	SPI_CONTEXT_INIT_SYNC(spi_acts_data_port_1, ctx),
};

static const struct acts_spi_config spi_acts_cfg_1 = {
	.spi = (struct acts_spi_controller *) SPI1_REG_BASE,
//	.spiclk_reg = CMU_SPI1CLK,
#ifdef CONFIG_SPI_ACTS_DMA
	.txdma_id = DMA_ID_SPI1,
	.rxdma_id = DMA_ID_SPI1,
#endif
#ifdef CONFIG_SPI_1_ACTS_DMA
	.flag_use_dma = 1,
#else
	.flag_use_dma = 0,
#endif

#ifdef CONFIG_SPI_1_ACTS_DMA_DUPLEX
	.flag_use_dma_duplex = 1,
#else
	.flag_use_dma_duplex = 0,
#endif

	.clock_id = CLOCK_ID_SPI1,
	.reset_id = RESET_ID_SPI1,
	.delay_chain = CONFIG_SPI_1_ACTS_DELAY_CHAIN,
};

DEVICE_AND_API_INIT(spi_acts_1, CONFIG_SPI_1_NAME, spi_acts_init,
		    &spi_acts_data_port_1, &spi_acts_cfg_1,
		    PRE_KERNEL_2, CONFIG_SPI_INIT_PRIORITY, &acts_spi_driver_api);
#endif	/* CONFIG_SPI_1 */

#ifdef CONFIG_SPI_2
struct acts_spi_data spi_acts_data_port_2 = {
	SPI_CONTEXT_INIT_LOCK(spi_acts_data_port_2, ctx),
	SPI_CONTEXT_INIT_SYNC(spi_acts_data_port_2, ctx),
};

static const struct acts_spi_config spi_acts_cfg_2 = {
	.spi = (struct acts_spi_controller *) SPI2_REG_BASE,
//	.spiclk_reg = CMU_SPI1CLK,
#ifdef CONFIG_SPI_ACTS_DMA
	.txdma_id = DMA_ID_SPI2,
	.rxdma_id = DMA_ID_SPI2,
#endif

#ifdef CONFIG_SPI_2_ACTS_DMA
	.flag_use_dma = 1,
#else
	.flag_use_dma = 0,
#endif

#ifdef CONFIG_SPI_2_ACTS_DMA_DUPLEX
	.flag_use_dma_duplex = 1,
#else
	.flag_use_dma_duplex = 0,
#endif

	.clock_id = CLOCK_ID_SPI2,
	.reset_id = RESET_ID_SPI2,
	.delay_chain = CONFIG_SPI_2_ACTS_DELAY_CHAIN,
};

DEVICE_AND_API_INIT(spi_acts_2, CONFIG_SPI_2_NAME, spi_acts_init,
		    &spi_acts_data_port_2, &spi_acts_cfg_2,
		    PRE_KERNEL_2, CONFIG_SPI_INIT_PRIORITY, &acts_spi_driver_api);
#endif	/* CONFIG_SPI_2 */
