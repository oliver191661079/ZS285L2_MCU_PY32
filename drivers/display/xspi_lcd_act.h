#include <errno.h>
#include <kernel.h>
#include <soc_rom_xspi.h>
#include <soc_dma.h>
#include <spi.h>
#include <gpio.h>

#define SPI_CTL_DELAYCHAIN(x)		((x) << 16)
#define BIT(n)  (1UL << (n))
#define SPI_LINES_DUAL		BIT(11)
#define SPI_LINES_QUAD		BIT(12)
#define SPI_OP_MODE_SLAVE	BIT(0)

#define SPI_OP_MODE_MASK	0x1

#define SPI_OP_MODE_GET(_operation_) ((_operation_) & SPI_OP_MODE_MASK)
#define SPI_CTL_MS_SEL_SLAVE		(0x1 << 22)
#define SPI_CTL_FIFO_WIDTH_MASK		(0x1 << 30)
#define SPI_CTL_FIFO_WIDTH_8BIT		(0x0 << 30)
#define SPI_CTL_FIFO_WIDTH_32BIT	(0x1 << 30)
#define SPI_CTL_LOOP			(1 << 2)
#define SPI_CTL_WR_MODE_MASK		(0x3 << 0)
#define SPI_CTL_WR_MODE_WRITE		(0x2 << 0)
#define SPI_STATUS_TX_FULL		(1 << 5)
#define SPI_CTL_TX_FIFO_EN		(1 << 5)
#define SPI_CTL_RX_FIFO_EN		(1 << 4)
#define SPI_STATUS_TX_EMPTY		(1 << 4)
#define SPI_CTL_MS_SEL_MASK		(0x1 << 22)
#define SPI_CTL_MS_SEL_MASTER		(0x0 << 22)
#define SPI_STATUS_BUSY			(1 << 0)
#define SPI_STATUS_TX_FIFO_WERR		(1 << 11)
#define SPI_STATUS_RX_FIFO_WERR		(1 << 9)
#define SPI_STATUS_RX_FIFO_RERR		(1 << 8)
#define SPI_STATUS_ERR_MASK		(SPI_STATUS_RX_FIFO_RERR |	\
					 SPI_STATUS_RX_FIFO_WERR |	\
					 SPI_STATUS_TX_FIFO_WERR)
#define SPI_CTL_WR_MODE_DISABLE		(0x0 << 0)

#ifdef CONFIG_SOC_SERIES_ROCKY
#define SPI_CTL_TX_DRQ_EN		(1 << 11)
#define SPI_CTL_RX_DRQ_EN		(1 << 10)
#else
#define SPI_CTL_TX_DRQ_EN		(1 << 11)
#define SPI_CTL_RX_DRQ_EN		(1 << 10)
#endif

#define SPI_CTL_MODE_CPHA		(1 << 28)
#define SPI_CTL_MODE_CPOL		(1 << 29)
#define SPI_CTL_SB_SEL_LSB		(0x1 << 21)
#define SPI_CTL_SS			(1 << 3)
#define SPI_CTL_WR_MODE_READ		(0x1 << 0)
#define SPI_CTL_CLK_SEL_DMA		(0x1 << 31)

#define CONFIG_CFG(cfg)						\
((const struct acts_spi_config * const)(cfg)->dev->config->config_info)

#define CONFIG_DATA(cfg)					\
((struct acts_spi_data * const)(cfg)->dev->driver_data)

struct spi_context {
	struct spi_config *config;

	struct k_sem lock;
	struct k_sem sync;
	int sync_status;

#ifdef CONFIG_POLL
	struct k_poll_signal *signal;
	bool asynchronous;
#endif
	const struct spi_buf *current_tx;
	size_t tx_count;
	struct spi_buf *current_rx;
	size_t rx_count;

	u8_t *tx_buf;
	size_t tx_len;
	u8_t *rx_buf;
	size_t rx_len;
};

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
	struct k_sem spi_sync;
	struct spi_context ctx;
#ifdef CONFIG_SPI_ACTS_DMA
	struct device *dma_dev;
	struct k_sem dma_sync;
	u32_t dma_chan0;
	/* use dma chan1 for full duplex mode */
	u32_t dma_chan1;
#endif
};

typedef void (*gpio_config_irq_t)(struct device *dev);
struct acts_gpio_config {
	u32_t base;
	u32_t irq_num;
	gpio_config_irq_t config_func;
};

static inline void xspi_context_cs_configure(struct spi_context *ctx)
{
	struct device *dev=ctx->config->cs->gpio_dev;
	const struct acts_gpio_config *info = dev->config->config_info;
	int pin=ctx->config->cs->gpio_pin;
	unsigned long key, val;
	if(pin >= 52)//非法管脚
		return ;
	if (ctx->config->cs) {
		val= GPIO_CTL_MFP_GPIO | GPIO_CTL_INTC_MASK | GPIO_CTL_SMIT|GPIO_CTL_GPIO_OUTEN;
		key = irq_lock();

		sys_write32(val, GPIO_REG_CTL(info->base, pin));
		sys_write32(GPIO_BIT(pin), GPIO_REG_BSR(info->base, pin));

		irq_unlock(key);
	}
}

static ALWAYS_INLINE void xspi_lcd_configure(const struct acts_spi_config *cfg,struct acts_spi_data *spi,struct spi_config *config)
{
	u32_t ctrl, word_size;
	u32_t op = config->operation;

	ctrl = SPI_CTL_DELAYCHAIN(cfg->delay_chain);

	sys_write32(0x10d, CMU_SPI1CLK);
	k_busy_wait(100);

	if (op & (SPI_LINES_DUAL | SPI_LINES_QUAD))
		return ;

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

	xspi_context_cs_configure(&spi->ctx);

	return ;
}

static ALWAYS_INLINE void xspi_tx_buffers_setup(struct spi_context *ctx,
					     const struct spi_buf *tx_bufs,unsigned char dfs)
{
	ctx->current_tx = tx_bufs;
	ctx->tx_count = 1;
	ctx->current_rx = NULL;
	ctx->rx_count = 0;

	if (tx_bufs) {
		ctx->tx_buf = tx_bufs->buf;
		ctx->tx_len = tx_bufs->len / dfs;
	} else {
		ctx->tx_buf = NULL;
		ctx->tx_len = 0;
	}
#if 0
	if (rx_bufs) {
		ctx->rx_buf = rx_bufs->buf;
		ctx->rx_len = rx_bufs->len / dfs;
	} else {
		ctx->rx_buf = NULL;
		ctx->rx_len = 0;
	}
#endif
	ctx->sync_status = 0;
}
static ALWAYS_INLINE void xspi_lcd_cs_control(struct spi_context *ctx,bool on)
{
	struct device *dev=ctx->config->cs->gpio_dev;
	const struct acts_gpio_config *info = dev->config->config_info;
	int pin=ctx->config->cs->gpio_pin;
	unsigned long key;

	if (ctx->config->cs) {
		if (on) {
			key = irq_lock();
			sys_write32(GPIO_BIT(pin), GPIO_REG_BRR(info->base, pin));
			irq_unlock(key);
			k_busy_wait(ctx->config->cs->delay);
		} else {
			if (ctx->config->operation & SPI_HOLD_ON_CS) {
				return;
			}
			k_busy_wait(ctx->config->cs->delay);
			key = irq_lock();
			sys_write32(GPIO_BIT(pin), GPIO_REG_BRR(info->base, pin));
			irq_unlock(key);
		}
	}




}

static inline int xspi_lcd_write_data_by_dma(const struct acts_spi_config *cfg,
				      struct acts_spi_data *data,
				      const u8_t *buf, s32_t len)
{
/*
	struct acts_spi_controller *spi = cfg->spi;
	int ret;

	spi->bc = len;
	spi->ctrl = (spi->ctrl & ~SPI_CTL_WR_MODE_MASK) | SPI_CTL_WR_MODE_MASK|
								SPI_CTL_WR_MODE_READ | SPI_CTL_CLK_SEL_DMA |
								SPI_CTL_RX_DRQ_EN;

	if (spi == (struct acts_spi_controller *) SPI0_REG_BASE) {// SPI0

		sys_write32(DMA_CTL_TWS_8BIT \
			| (1<<DMA_CTL_DAM_SHIFT) | DMA_CTL_DST_TYPE(DMA_ID_SPI0) \
			| (0<<DMA_CTL_SAM_SHIFT) | DMA_CTL_SRC_TYPE(DMA_ID_MEMORY), \
			si->dma_chan_base + DMA_CTL_OFFSET);
	} else if (spi == (struct acts_spi_controller *) SPI1_REG_BASE) {// SPI1

		sys_write32(DMA_CTL_TWS_8BIT \
			| (1<<DMA_CTL_DAM_SHIFT) | DMA_CTL_DST_TYPE(DMA_ID_SPI1) \
			| (0<<DMA_CTL_SAM_SHIFT) | DMA_CTL_SRC_TYPE(DMA_ID_MEMORY), \
			si->dma_chan_base + DMA_CTL_OFFSET);
	}

	sys_write32((unsigned int)buf, si->dma_chan_base + DMA_SADDR_OFFSET);
	sys_write32(si->base + XSPI_TXDAT, si->dma_chan_base + DMA_DADDR_OFFSET);
	sys_write32(len, si->dma_chan_base + DMA_BC_OFFSET);

	return ret;
	*/
	return 0;
}
static inline int xspi_lcd_write_data_by_cpu(struct acts_spi_controller *spi,
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
static inline int xspi_lcd_write_data(const struct acts_spi_config *cfg, struct acts_spi_data *data,
	u8_t *tx_buf, int len)
{
		int ret;
#ifdef CONFIG_SPI_ACTS_DMA
		if (cfg->flag_use_dma) {
			ret = xspi_lcd_write_data_by_dma(cfg, data,
				tx_buf, len);
		} else {
#endif
			ret = xspi_lcd_write_data_by_cpu(cfg->spi,
				tx_buf, len);
#ifdef CONFIG_SPI_ACTS_DMA
		}
#endif
		return ret;
}
static ALWAYS_INLINE int xspi_lcd_transfer_data(const struct acts_spi_config *cfg,struct acts_spi_data *data)
{
	struct acts_spi_controller *spi = cfg->spi;
	struct spi_context *ctx = &data->ctx;
	int chunk_size;
	int ret = 0;
	if(!ctx->tx_len)
		return -EIO;
	chunk_size = ctx->tx_len;

	spi->ctrl |= SPI_CTL_RX_FIFO_EN | SPI_CTL_TX_FIFO_EN;

	ret = xspi_lcd_write_data(cfg, data, ctx->tx_buf, chunk_size);

	ctx->tx_len -= chunk_size;
	if (!ctx->tx_len) {
		ctx->current_tx++;
		ctx->tx_count--;

		if (ctx->tx_count) {
			ctx->tx_buf = ctx->current_tx->buf;
			ctx->tx_len = ctx->current_tx->len ;
		} else {
			ctx->tx_buf = NULL;
		}
	} else if (ctx->tx_buf) {
		ctx->tx_buf +=  chunk_size;
	}

	if (!ret) {
		while (!(spi->status & SPI_STATUS_TX_EMPTY));
		/* wait until tx fifo is empty for master mode*/
		while (((spi->ctrl & SPI_CTL_MS_SEL_MASK) == SPI_CTL_MS_SEL_MASTER) &&spi->status & SPI_STATUS_BUSY);

		if (spi->status & SPI_STATUS_ERR_MASK) {
			ret = -EIO;
		}
	}
	spi->ctrl = (spi->ctrl & ~SPI_CTL_WR_MODE_MASK) | SPI_CTL_WR_MODE_DISABLE;
	spi->status |= SPI_STATUS_ERR_MASK;

	return ret;

}

static ALWAYS_INLINE bool xspi_lcd_transfer_ongoing(struct spi_context *ctx)
{
	return !!(ctx->tx_buf || ctx->tx_len);
}

static ALWAYS_INLINE void xspi_transceive_data(struct spi_config *config,const struct spi_buf *tx_bufs)
{
	const struct acts_spi_config *cfg = CONFIG_CFG(config);
	struct acts_spi_data *data = CONFIG_DATA(config);
	struct spi_context *ctx=&data->ctx;
	struct acts_spi_controller *spi = cfg->spi;

	int ret;
	xspi_tx_buffers_setup(ctx, tx_bufs,1);

		/* assert chip select */
	if (SPI_OP_MODE_MASTER == SPI_OP_MODE_GET(config->operation)) {
		if (data->ctx.config->cs) {
			xspi_lcd_cs_control(&data->ctx, true);
		} else {
			spi->ctrl &= ~SPI_CTL_SS;
		}
	}

	do {
		ret = xspi_lcd_transfer_data(cfg, data);
	} while (!ret && xspi_lcd_transfer_ongoing(ctx));

	/* deassert chip select */
	if (SPI_OP_MODE_MASTER == SPI_OP_MODE_GET(config->operation)) {
		if (data->ctx.config->cs) {
			xspi_lcd_cs_control(&data->ctx, false);
		} else {
			spi->ctrl |= SPI_CTL_SS;
		}
	}
	/*
	key = irq_lock();
	sys_write32(GPIO_BIT(pin), GPIO_REG_BRR(info->base, pin));
	irq_unlock(key);

	do {
		ret = spi_acts_transfer_data(cfg, data);
	} while (!ret && spi_acts_transfer_ongoing(data));

	key = irq_lock();
	sys_write32(GPIO_BIT(pin), GPIO_REG_BRR(info->base, pin));
	irq_unlock(key);
	*/
}



