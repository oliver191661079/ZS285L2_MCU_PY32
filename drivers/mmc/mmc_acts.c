/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <init.h>
#include <device.h>
#include <irq.h>
#include <dma.h>
#include <gpio.h>
#include <mmc/mmc.h>
#include <soc.h>
#include <board.h>
#include <string.h>

#define SYS_LOG_DOMAIN "mmc"
#define SYS_LOG_LEVEL CONFIG_SYS_LOG_MMC_LEVEL
#include <logging/sys_log.h>

/* timeout define */
#define MMC_CMD_TIMEOUT_MS              (200)
#define MMC_DAT_TIMEOUT_MS              (500)
#define MMC_DMA_BUFFER_BUSY_TIMEOUT_US  (5000)

/* SD_EN register */
/* data bus width selection */
#define SD_EN_DW_SHIFT                  0
#define SD_EN_DW(x)                     ((x) << SD_EN_DW_SHIFT)
#define SD_EN_DW_MASK                   SD_EN_DW(0x3)
#define SD_EN_DW_1BIT                   SD_EN_DW(0x0)
#define SD_EN_DW_4BIT                   SD_EN_DW(0x1)
/* SDIO function enable */
#define SD_EN_SDIO                      BIT(3)
/* AHB or DMA bus selection */
#define SD_EN_BUS_SEL_SHIFT             6
#define SD_EN_BUS_SEL(x)                ((x) << SD_EN_BUS_SEL_SHIFT)
#define SD_EN_BUS_SEL_MASK              SD_EN_BUS_SEL(0x1)
#define SD_EN_BUS_SEL_AHB               SD_EN_BUS_SEL(0x0)
#define SD_EN_BUS_SEL_DMA               SD_EN_BUS_SEL(0x1)
/* SD module enable */
#define SD_EN_ENABLE                    BIT(7)
/* CS status select in idle mode */
#define SD_EN_SHIFT                     9
#define SD_EN_CSS_MASK                  (1 << SD_EN_SHIFT)
#define SD_EN_CSS_LOW                   (0 << SD_EN_SHIFT)
#define SD_EN_CSS_HIGH                  (1 << SD_EN_SHIFT)

/* SD_CTL register */
/* specify the transfer mode */
#define SD_CTL_TM_SHIFT                 0
#define SD_CTL_TM(x)                    ((x) << SD_CTL_TM_SHIFT)
#define SD_CTL_TM_MASK                  SD_CTL_TM(0xf)
#define SD_CTL_TM_CMD_NO_RESP           SD_CTL_TM(0x0)
#define SD_CTL_TM_CMD_6B_RESP           SD_CTL_TM(0x1)
#define SD_CTL_TM_CMD_17B_RESP          SD_CTL_TM(0x2)
#define SD_CTL_TM_CMD_6B_RESP_BUSY      SD_CTL_TM(0x3)
#define SD_CTL_TM_CMD_RESP_DATA_IN      SD_CTL_TM(0x4)
#define SD_CTL_TM_CMD_RESP_DATA_OUT     SD_CTL_TM(0x5)
#define SD_CTL_TM_DATA_IN               SD_CTL_TM(0x6)
#define SD_CTL_TM_DATA_OUT              SD_CTL_TM(0x7)
#define SD_CTL_TM_CLK_OUT_ONLY          SD_CTL_TM(0x8)
/* enable last block send 8 more clocks */
#define SD_CTL_LBE                      BIT(6)
/* transfer start */
#define SD_CTL_START                    BIT(7)
/* transfer clock number; 0 means 256 clks */
#define SD_CTL_TCN_SHIFT                8
#define SD_CTL_TCN(x)                   ((x) << SD_CTL_TCN_SHIFT)
#define SD_CTL_TCN_MASK                 SD_CTL_TCN(0xf)
/* send continuous clock */
#define SD_CTL_SCC                      BIT(12)
/* write delay time */
#define SD_CTL_WDELAY_SHIFT             16
#define SD_CTL_WDELAY(x)                ((x) << SD_CTL_WDELAY_SHIFT)
#define SD_CTL_WDELAY_MASK              SD_CTL_WDELAY(0xf)
/* read delay time */
#define SD_CTL_RDELAY_SHIFT             20
#define SD_CTL_RDELAY(x)                ((x) << SD_CTL_RDELAY_SHIFT)
#define SD_CTL_RDELAY_MASK              SD_CTL_RDELAY(0xf)

/* SD_STATE register */
#define SD_STATE_C7ER                   BIT(0) /* CRC command response error */
#define SD_STATE_RC16ER                 BIT(1) /* CRC read data error */
#define SD_STATE_WC16ER                 BIT(2) /* CRC write data error */
#define SD_STATE_CLC                    BIT(3) /* Command Line transfer complete */
#define SD_STATE_CLNR                   BIT(4) /* Command Line not response */
#define SD_STATE_TRANS_IRQ_PD           BIT(5) /* Transfer end IRQ pending */
#define SD_STATE_TRANS_IRQ_EN           BIT(6) /* Transfer end IRQ enable */
#define SD_STATE_DAT0S                  BIT(7) /* DAT0 status */
#define SD_STATE_SDIO_IRQ_EN            BIT(8) /* SDIO IRQ enable */
#define SD_STATE_SDIO_IRQ_PD            BIT(9) /* SDIO IRQ pending bit */
#define SD_STATE_DAT1S                  BIT(10) /* DAT1 status */
#define SD_STATE_CMDS                   BIT(11) /* CMD status */
#define SD_STATE_MEMRDY                 BIT(12) /* Memory ready; 1: ready for read or write */
#define SD_STATE_FIFO_FULL              BIT(12) /* 1: FIFO is full; 0: FIFO is not full; ONLY used in cpu mode */
#define SD_STATE_FIFO_EMPTY             BIT(13) /* 1: FIFO is empty; 0: FIFO not empty */
#define SD_STATE_FIFO_RESET             BIT(14) /* reset the SD FIFO */

#define SD_STATE_ERR_MASK               (SD_STATE_CLNR | SD_STATE_WC16ER | \
                                            SD_STATE_RC16ER | SD_STATE_C7ER)
/* SD_CMD register */
#define SD_CMD_MASK                     (0xFF)

/* SD_BLK_SIZE register */
#define SD_BLK_SIZE_MASK                (0x3FF)

/* SD_BLK_NUM register */
#define SD_BLK_NUM_MASK                 (0xFFFF)

/* mmc hardware controller */
struct acts_mmc_controller {
	volatile u32_t en;
	volatile u32_t ctl;
	volatile u32_t state;
	volatile u32_t cmd;
	volatile u32_t arg;
	volatile u32_t rspbuf[5];
	volatile u32_t dat;
	volatile u32_t blk_size;
	volatile u32_t blk_num;
};

/* specify the config capbility flags */
#define MMC_CFG_FLAGS_USE_FIFO          BIT(0)

struct acts_mmc_config {
	struct acts_mmc_controller *base;
	u32_t capability;
	u32_t flags;
	u32_t clk_reg;
	u8_t clock_id;
	u8_t reset_id;
	u8_t dma_id;

	u8_t id;
	u8_t data_reg_width;
	u8_t reserved[2];

	void (*irq_config)(struct device *dev);

#ifdef CONFIG_MMC_SDIO_GPIO_IRQ
	char *sdio_irq_gpio_name;
	u32_t sdio_irq_gpio;
#endif
};

struct acts_mmc_data {
	struct k_sem trans_done;
	struct device *dma_dev;

	u8_t rdelay;
	u8_t wdelay;
	u8_t reserved[2];

	void (*sdio_irq_cbk)(void *arg);
	void *sdio_irq_cbk_arg;

#ifdef CONFIG_MMC_ACTS_DMA
	int dma_chan;
	struct dma_config dma_cfg;
	struct dma_block_config dma_block_cfg;
#endif

#ifdef CONFIG_MMC_SDIO_GPIO_IRQ
	struct device *sdio_irq_gpio_dev;
	struct gpio_callback sdio_irq_gpio_cb;
#endif

	u8_t device_release_flag : 1; /* device has been released flag such as sd card pluged out */
};

#ifdef CONFIG_MMC_ACTS_ERROR_DETAIL
static void mmc_acts_dump_regs(struct acts_mmc_controller *mmc)
{
	SYS_LOG_INF("** mmc contoller register **");
	SYS_LOG_INF("       BASE: %08x", (u32_t)mmc);
	SYS_LOG_INF("      SD_EN: %08x", mmc->en);
	SYS_LOG_INF("     SD_CTL: %08x", mmc->ctl);
	SYS_LOG_INF("   SD_STATE: %08x", mmc->state);
	SYS_LOG_INF("     SD_CMD: %08x", mmc->cmd);
	SYS_LOG_INF("     SD_ARG: %08x", mmc->arg);
	SYS_LOG_INF(" SD_RSPBUF0: %08x", mmc->rspbuf[0]);
	SYS_LOG_INF(" SD_RSPBUF1: %08x", mmc->rspbuf[1]);
	SYS_LOG_INF(" SD_RSPBUF2: %08x", mmc->rspbuf[2]);
	SYS_LOG_INF(" SD_RSPBUF3: %08x", mmc->rspbuf[3]);
	SYS_LOG_INF(" SD_RSPBUF4: %08x", mmc->rspbuf[4]);
	SYS_LOG_INF("SD_BLK_SIZE: %08x", mmc->blk_size);
	SYS_LOG_INF(" SD_BLK_NUM: %08x", mmc->blk_num);
	SYS_LOG_INF(" CMU_SD0CLK: %08x", sys_read32(CMU_SD0CLK));
	SYS_LOG_INF(" CMU_SD1CLK: %08x", sys_read32(CMU_SD1CLK));
}
#endif

static int mmc_acts_check_err(const struct acts_mmc_config *cfg,
			      struct acts_mmc_controller *mmc,
			      u32_t resp_err_mask)
{
	u32_t state = mmc->state & resp_err_mask;

	if (!(state & SD_STATE_ERR_MASK))
		return 0;

#ifdef CONFIG_MMC_ACTS_ERROR_DETAIL
	if (state & SD_STATE_CLNR)
		SYS_LOG_DBG("Command No response");

	if (state & SD_STATE_C7ER)
		SYS_LOG_ERR("CRC command response Error");

	if (state & SD_STATE_RC16ER)
		SYS_LOG_ERR("CRC Read data Error");

	if (state & SD_STATE_WC16ER)
		SYS_LOG_ERR("CRC Write data Error");
#endif

	return 1;
}

static void mmc_acts_err_reset(const struct acts_mmc_config *cfg,
			       struct acts_mmc_controller *mmc)
{
	u32_t en_bak, state_bak;

	en_bak = mmc->en;
	state_bak = mmc->state;

	/* reset mmc controller */
	acts_reset_peripheral(cfg->reset_id);

	mmc->en = en_bak;
	mmc->state = state_bak;
}

static void mmc_acts_trans_irq_setup(struct acts_mmc_controller *mmc,
				     bool enable)
{
	u32_t key, state;

	key = irq_lock();

	state = mmc->state;

	/* don't clear sdio pending */
	state &= ~SD_STATE_SDIO_IRQ_PD;
	if (enable)
		state |= SD_STATE_TRANS_IRQ_EN;
	else
		state &= ~SD_STATE_TRANS_IRQ_EN;

	mmc->state = state;

	irq_unlock(key);
}

static void mmc_acts_sdio_irq_setup(struct acts_mmc_controller *mmc,
				    bool enable)
{
	u32_t key, state;

	key = irq_lock();

	state = mmc->state;

	/* don't clear transfer irq pending */
	state &= ~SD_STATE_TRANS_IRQ_PD;
	if (enable)
		state |= SD_STATE_SDIO_IRQ_EN;
	else
		state &= ~SD_STATE_SDIO_IRQ_EN;

	mmc->state = state;

	irq_unlock(key);
}

static void mmc_acts_isr(void *arg)
{
	struct device *dev = (struct device *)arg;
	const struct acts_mmc_config *cfg = dev->config->config_info;
	struct acts_mmc_data *data = dev->driver_data;
	struct acts_mmc_controller *mmc = cfg->base;
	u32_t state;

	state = mmc->state;

	SYS_LOG_DBG("enter isr: state 0x%x", state);

	if ((state & SD_STATE_TRANS_IRQ_EN) &&
	    ((state & SD_STATE_TRANS_IRQ_PD) ||
	    (state & SD_STATE_ERR_MASK))) {
		k_sem_give(&data->trans_done);
	}

	if ((state & SD_STATE_SDIO_IRQ_EN) &&
	    (state & SD_STATE_SDIO_IRQ_PD)) {
		if (data->sdio_irq_cbk) {
			data->sdio_irq_cbk(data->sdio_irq_cbk_arg);
		}
	}

	/* clear irq pending, keep the error bits */
	mmc->state = state & (SD_STATE_TRANS_IRQ_EN | SD_STATE_SDIO_IRQ_EN |
			      SD_STATE_TRANS_IRQ_PD | SD_STATE_SDIO_IRQ_PD);
}

static int mmc_acts_get_trans_mode(struct mmc_cmd *cmd, u32_t *trans_mode,
				   u32_t *rsp_err_mask)
{
	u32_t mode, err_mask = 0;

	switch (mmc_resp_type(cmd)) {
	case MMC_RSP_NONE:
		mode = SD_CTL_TM_CMD_NO_RESP;
		break;

	case MMC_RSP_R1:
		if (cmd->buf) {
			if (cmd->flags & MMC_DATA_READ)
				mode = SD_CTL_TM_CMD_RESP_DATA_IN;
			else if (cmd->flags & MMC_DATA_WRITE)
				mode = SD_CTL_TM_CMD_RESP_DATA_OUT;
			else if (cmd->flags & MMC_DATA_WRITE_DIRECT)
				mode = SD_CTL_TM_DATA_OUT;
			else if (cmd->flags & MMC_DATA_READ_DIRECT)
				mode = SD_CTL_TM_DATA_IN;
			else
				return -EINVAL;
		} else {
			mode = SD_CTL_TM_CMD_6B_RESP;
		}
		err_mask = SD_STATE_CLNR | SD_STATE_C7ER | SD_STATE_RC16ER |
			   SD_STATE_WC16ER;

		break;

	case MMC_RSP_R1B:
		mode = SD_CTL_TM_CMD_6B_RESP_BUSY;
		err_mask = SD_STATE_CLNR | SD_STATE_C7ER;
		break;

	case MMC_RSP_R2:
		mode = SD_CTL_TM_CMD_17B_RESP;
		err_mask = SD_STATE_CLNR | SD_STATE_C7ER;
		break;

	case MMC_RSP_R3:
		mode = SD_CTL_TM_CMD_6B_RESP;
		err_mask = SD_STATE_CLNR;
		break;

	default:
		SYS_LOG_ERR("unsupported RSP 0x%x\n", mmc_resp_type(cmd));
		return -ENOTSUP;
	}

	if (trans_mode)
		*trans_mode = mode;

	if (rsp_err_mask)
		*rsp_err_mask = err_mask;

	return 0;
}

static int mmc_acts_wait_cmd(struct acts_mmc_controller *mmc, int timeout_ms)
{
	u32_t start_time, curr_time;

	start_time = k_uptime_get();
	while (mmc->ctl & SD_CTL_START) {
		curr_time = k_uptime_get_32();

		if (curr_time < start_time) {
			start_time = k_uptime_get();
		}
		k_sleep(1);
		if ((curr_time - start_time) >= timeout_ms) {
			SYS_LOG_ERR("mmc cmd timeout");
			return -ETIMEDOUT;
		}
	}

	return 0;
}

static bool mmc_acts_data_is_ready(struct acts_mmc_controller *mmc,
				   bool is_write, bool use_fifo)
{
	u32_t state = mmc->state;

	if (use_fifo)
		if (is_write)
			return (!(state & SD_STATE_FIFO_FULL));
		else
			return (!(state & SD_STATE_FIFO_EMPTY));
	else
		return (state & SD_STATE_MEMRDY);
}

static int mmc_acts_transfer_by_cpu(struct device *dev,
				    bool is_write, u8_t *buf,
				    int len, u32_t timeout_ms)
{
	const struct acts_mmc_config *cfg = dev->config->config_info;
	struct acts_mmc_controller *mmc = cfg->base;
	u32_t start_time, curr_time;
	u32_t data, data_len;

	mmc->ctl |= SD_CTL_START;

	start_time = k_uptime_get();
	while (len > 0) {
		if (mmc_acts_check_err(cfg, mmc, SD_STATE_CLNR))
			return -EIO;
		if (mmc_acts_data_is_ready(mmc, is_write,
					   (cfg->flags & MMC_CFG_FLAGS_USE_FIFO))) {
			data_len = len < cfg->data_reg_width ? len : cfg->data_reg_width;

			if (is_write) {
				if (((u32_t)buf & 0x3) || data_len < cfg->data_reg_width)
					memcpy(&data, buf, data_len);
				else
					data = *((u32_t *)buf);

				mmc->dat = data;
			} else {
				data = mmc->dat;

				if (((u32_t)buf & 0x3) || data_len < cfg->data_reg_width)
					memcpy(buf, &data, data_len);
				else
					*((u32_t *)buf) = data;
			}

			buf += data_len;
			len -= data_len;
		}

		curr_time = k_uptime_get_32();
		if ((curr_time - start_time) >= timeout_ms) {
			SYS_LOG_ERR("mmc io timeout, is_write %d", is_write);
			return -ETIMEDOUT;
		}
	}

	return 0;
}

static int mmc_acts_transfer_by_dma(struct device *dev,
				    bool is_write, u8_t *buf,
				    int len, u32_t timeout_ms)
{
	const struct acts_mmc_config *cfg = dev->config->config_info;
	struct acts_mmc_data *data = dev->driver_data;
	struct acts_mmc_controller *mmc = cfg->base;
	int err;

	memset(&data->dma_cfg, 0, sizeof(struct dma_config));
	memset(&data->dma_block_cfg, 0, sizeof(struct dma_block_config));

	data->device_release_flag = 0;

	data->dma_cfg.dest_data_size = cfg->data_reg_width;

	data->dma_cfg.block_count = 1;
	data->dma_cfg.head_block = &data->dma_block_cfg;
	data->dma_block_cfg.block_size = len;
	if (is_write) {
		data->dma_cfg.dma_slot = cfg->dma_id;
		data->dma_cfg.channel_direction = MEMORY_TO_PERIPHERAL;
		data->dma_block_cfg.source_address = (u32_t)buf;
		data->dma_block_cfg.dest_address = (u32_t)&mmc->dat;
	} else {
		data->dma_cfg.dma_slot = cfg->dma_id;
		data->dma_cfg.channel_direction = PERIPHERAL_TO_MEMORY;
		data->dma_block_cfg.source_address = (u32_t)&mmc->dat;
		data->dma_block_cfg.dest_address = (u32_t)buf;
	}
#ifdef CONFIG_MMC_ACTS_DMA
	if (dma_config(data->dma_dev, data->dma_chan, &data->dma_cfg)) {
		SYS_LOG_ERR("dma%d config error", data->dma_chan);
		return -1;
	}

	if (dma_start(data->dma_dev, data->dma_chan)) {
		SYS_LOG_ERR("dma%d start error", data->dma_chan);
		return -1;
	}
#endif
	mmc->en |= SD_EN_BUS_SEL_DMA;
	mmc_acts_trans_irq_setup(mmc, true);

	/* start mmc controller state machine */
	mmc->ctl |= SD_CTL_START;

	/* wait until data transfer is done */
	err = k_sem_take(&data->trans_done, K_MSEC(timeout_ms));

	if (data->device_release_flag) {
		err = -ENXIO;
	} else {
		if (!err)
			err = dma_wait_timeout(data->dma_dev, data->dma_chan, MMC_DMA_BUFFER_BUSY_TIMEOUT_US);

		if (!err)
			/* wait controller idle */
			err = mmc_acts_wait_cmd(mmc, timeout_ms);
	}

	mmc_acts_trans_irq_setup(mmc, false);
	mmc->en &= ~SD_EN_BUS_SEL_DMA;
#ifdef CONFIG_MMC_ACTS_DMA
	dma_stop(data->dma_dev, data->dma_chan);
#endif
	return err;
}

static int mmc_acts_send_cmd(struct device *dev, struct mmc_cmd *cmd)
{
	const struct acts_mmc_config *cfg = dev->config->config_info;
	struct acts_mmc_data *data = dev->driver_data;
	struct acts_mmc_controller *mmc = cfg->base;
	int is_write = cmd->flags & (MMC_DATA_WRITE | MMC_DATA_WRITE_DIRECT);
	u32_t ctl, rsp_err_mask, len, trans_mode;
	int err, timeout;

	SYS_LOG_DBG("CMD%02d: arg 0x%x flags 0x%x is_write %d",
		    cmd->opcode, cmd->arg, cmd->flags, !!is_write);
	SYS_LOG_DBG("       blk_num 0x%x blk_size 0x%x, buf %p",
		    cmd->blk_num, cmd->blk_size, cmd->buf);

	err = mmc_acts_get_trans_mode(cmd, &trans_mode, &rsp_err_mask);
	if (err)
		return err;

	ctl = trans_mode | SD_CTL_RDELAY(data->rdelay) |
	      SD_CTL_WDELAY(data->wdelay);
	if (cmd->buf)
		ctl |= SD_CTL_LBE;

	/* sdio wifi need continues clock */
	if (cfg->capability & MMC_CAP_SDIO_IRQ)
		ctl |= SD_CTL_SCC;

	mmc->ctl = ctl;
	mmc->arg = cmd->arg;
	mmc->cmd = cmd->opcode;
	mmc->blk_num = cmd->blk_num;
	mmc->blk_size = cmd->blk_size;

	if (!cmd->buf) {
		/* only command need to transfer */
		mmc->ctl |= SD_CTL_START;
	} else {
		/* command with data transfer */
		len = cmd->blk_num * cmd->blk_size;
		timeout = cmd->blk_num * MMC_DAT_TIMEOUT_MS;
#ifdef CONFIG_MMC_ACTS_DMA
		if ((!data->dma_chan) || ((u32_t)cmd->buf & 0x3)) {
			err = mmc_acts_transfer_by_cpu(dev, is_write,
						       cmd->buf, len, timeout);
		} else {
			err = mmc_acts_transfer_by_dma(dev, is_write,
						       cmd->buf, len, timeout);
		}
#else
		err = mmc_acts_transfer_by_cpu(dev, is_write,
					       cmd->buf, len, timeout);
#endif
	}

	err |= mmc_acts_wait_cmd(mmc, MMC_CMD_TIMEOUT_MS);
	err |= mmc_acts_check_err(cfg, mmc, rsp_err_mask);
	if (err) {
		/*
		 * FIXME: the operation of detecting card by polling maybe
		 * output no reponse error message periodically. So filter
		 * it out by config.
		 */
#ifdef CONFIG_MMC_ACTS_ERROR_DETAIL
		SYS_LOG_ERR("mmc%d: send cmd%d error, state 0x%x",
			    cfg->id, cmd->opcode, mmc->state);
		mmc_acts_dump_regs(mmc);
#endif
		mmc_acts_err_reset(cfg, mmc);
		return -EIO;
	}

	/* process responses */
	if (!cmd->buf && (cmd->flags & MMC_RSP_PRESENT)) {
		if (cmd->flags & MMC_RSP_136) {
			/* MSB first */
			cmd->resp[3] = mmc->rspbuf[0];
			cmd->resp[2] = mmc->rspbuf[1];
			cmd->resp[1] = mmc->rspbuf[2];
			cmd->resp[0] = mmc->rspbuf[3];
		} else {
			cmd->resp[0] = (mmc->rspbuf[1] << 24) |
				       (mmc->rspbuf[0] >> 8);
			cmd->resp[1] = (mmc->rspbuf[1] << 24) >> 8;
		}
	}

	return 0;
}

static int mmc_acts_set_sdio_irq_cbk(struct device *dev,
				      sdio_irq_callback_t callback,
				      void *arg)
{
	const struct acts_mmc_config *cfg = dev->config->config_info;
	struct acts_mmc_data *data = dev->driver_data;

	if (!(cfg->capability & MMC_CAP_SDIO_IRQ))
		return -ENOTSUP;

	data->sdio_irq_cbk = callback;
	data->sdio_irq_cbk_arg = arg;

	return 0;
}

#ifdef CONFIG_MMC_SDIO_GPIO_IRQ
static void sdio_irq_gpio_callback(struct device *port,
				   struct gpio_callback *cb,
				   u32_t pins)
{
	struct acts_mmc_data *data =
		CONTAINER_OF(cb, struct acts_mmc_data, sdio_irq_gpio_cb);

	ARG_UNUSED(pins);

	if (data->sdio_irq_cbk)
		data->sdio_irq_cbk(data->sdio_irq_cbk_arg);
}

static void mmc_acts_sdio_irq_gpio_setup(struct device *dev, bool enable)
{
	const struct acts_mmc_config *cfg = dev->config->config_info;
	struct acts_mmc_data *data = dev->driver_data;

	if (enable)
		gpio_pin_enable_callback(data->sdio_irq_gpio_dev,
					 cfg->sdio_irq_gpio);
	else
		gpio_pin_disable_callback(data->sdio_irq_gpio_dev,
					  cfg->sdio_irq_gpio);
}
#endif

static int mmc_acts_enable_sdio_irq(struct device *dev, bool enable)
{
	const struct acts_mmc_config *cfg = dev->config->config_info;
	struct acts_mmc_controller *mmc = cfg->base;

	if (!(cfg->capability & MMC_CAP_SDIO_IRQ))
		return -ENOTSUP;

	if (cfg->capability & MMC_CAP_4_BIT_DATA) {
		mmc_acts_sdio_irq_setup(mmc, enable);
		mmc->en |= SD_EN_SDIO;
	} else {
#ifdef CONFIG_MMC_SDIO_GPIO_IRQ
		mmc_acts_sdio_irq_gpio_setup(dev, enable);
#else
		mmc_acts_sdio_irq_setup(mmc, enable);
		mmc->en |= SD_EN_SDIO;
#endif
	}

	return 0;
}

static int mmc_acts_set_bus_width(struct device *dev, unsigned int bus_width)
{
	const struct acts_mmc_config *cfg = dev->config->config_info;
	struct acts_mmc_controller *mmc = cfg->base;

	if (bus_width == MMC_BUS_WIDTH_1) {
		mmc->en &= ~SD_EN_DW_MASK;
		mmc->en |= SD_EN_DW_1BIT;
	} else if (bus_width == MMC_BUS_WIDTH_4) {
		mmc->en &= ~SD_EN_DW_MASK;
		mmc->en |= SD_EN_DW_4BIT;
	} else {
		return -EINVAL;
	}

	return 0;
}

static int mmc_acts_set_clock(struct device *dev, unsigned int rate_hz)
{
	const struct acts_mmc_config *cfg = dev->config->config_info;
	struct acts_mmc_data *data = dev->driver_data;
	u32_t val, rdelay, wdelay, real_rate;

	/*
	 * Set the RDELAY and WDELAY based on the sd clk.
	 */
	if (rate_hz < 200000) {
		/* clock source: HOSC(24Mhz), div 1/128, real freq: 187.5KHz */
		val = 0x10;
		real_rate = 187500;
		rdelay = 0x8;
		wdelay = 0xf;
	} else if (rate_hz <= 15000000) {
		/* clock source: HOSC, 1/2, real freq: 12MHz */
		val = 0x1;
		real_rate = 12000000;
		rdelay = 0x8;
		wdelay = 0xa;
	} else if (rate_hz <= 25000000) {
		/* clock source: HOSC, 1/1, real freq: 24MHz */
		val = 0x0;
		real_rate = 24000000;
		rdelay = 0x8;
		wdelay = 0x9;
	} else {
		/* clock source: HOSC(24MHz), 1/1, real freq: 24MHz */
		val = 0x0;
		real_rate = 24000000;
		rdelay = 0x8;
		wdelay = 0x9;
	}

#ifdef CONFIG_SOC_SD_USE_COREPLL
	if (cfg->id == 0) {
		if (rate_hz < 200000) {
			sys_write32((sys_read32(CMU_SDCLK) & ~0xff) | val, CMU_SDCLK);
		} else {
			soc_freq_set_sd_clk_with_corepll(0, CONFIG_SD0_FREQ_MHZ, soc_freq_get_corepll_freq());
			rdelay = 0x6;
			wdelay = 0x6;
		}
	} else {
		if (rate_hz < 200000) {
			sys_write32((sys_read32(CMU_SDCLK) & ~0xff00) | (val << 8), CMU_SDCLK);
		} else {
			soc_freq_set_sd_clk_with_corepll(1, CONFIG_SD1_FREQ_MHZ, soc_freq_get_corepll_freq());
			rdelay = 0x6;
			wdelay = 0x6;
		}
	}
#else
	if (cfg->id == 0) {
		sys_write32((sys_read32(CMU_SDCLK) & ~0xff) | val, CMU_SDCLK);
	} else {
		sys_write32((sys_read32(CMU_SDCLK) & ~0xff00) | (val << 8), CMU_SDCLK);
	}
#endif

	/* config delay chain */
	data->rdelay = rdelay;
	data->wdelay = wdelay;

	SYS_LOG_DBG("mmc%d: set rate %d Hz, real rate %d Hz",
		    cfg->id, rate_hz, real_rate);

	return 0;
}

static u32_t mmc_acts_get_capability(struct device *dev)
{
	const struct acts_mmc_config *cfg = dev->config->config_info;

	return cfg->capability;
}

static int mmc_acts_release_device(struct device *dev)
{
	struct acts_mmc_data *data = dev->driver_data;

	u32_t key = irq_lock();
	data->device_release_flag = 1;
	k_sem_give(&data->trans_done);
	k_sem_reset(&data->trans_done);
	irq_unlock(key);

	return 0;
}

static const struct mmc_driver_api mmc_acts_driver_api = {
	.get_capability	= mmc_acts_get_capability,
	.set_clock		= mmc_acts_set_clock,
	.set_bus_width	= mmc_acts_set_bus_width,
	.send_cmd		= mmc_acts_send_cmd,

	.set_sdio_irq_callback	= mmc_acts_set_sdio_irq_cbk,
	.enable_sdio_irq		= mmc_acts_enable_sdio_irq,

	.release_device = mmc_acts_release_device,
};

static int mmc_acts_init(struct device *dev)
{
	const struct acts_mmc_config *cfg = dev->config->config_info;
	struct acts_mmc_data *data = dev->driver_data;
	struct acts_mmc_controller *mmc = cfg->base;

#ifdef CONFIG_MMC_ACTS_DMA
	data->dma_dev = device_get_binding(CONFIG_DMA_0_NAME);
	if (!data->dma_dev) {
		SYS_LOG_ERR("cannot found dma device");
		return -ENODEV;
	}
#endif

	/* check the sd data with is invalid */
	if ((cfg->data_reg_width != 1) && (cfg->data_reg_width != 4)) {
		SYS_LOG_ERR("data width:%d config is invlaid", cfg->data_reg_width);
		return -EINVAL;
	}


#ifdef CONFIG_MMC_SDIO_GPIO_IRQ
	if (cfg->sdio_irq_gpio_name) {
		data->sdio_irq_gpio_dev = device_get_binding(cfg->sdio_irq_gpio_name);
		if (!data->sdio_irq_gpio_dev) {
			SYS_LOG_ERR("cannot found sdio irq gpio dev device %s",
				cfg->sdio_irq_gpio_name);
			return -ENODEV;
		}

		/* Configure IRQ pin and the IRQ call-back/handler */
		gpio_pin_configure(data->sdio_irq_gpio_dev,
				   cfg->sdio_irq_gpio,
#ifdef CONFIG_WIFI_SSV6030P
				   GPIO_DIR_IN | GPIO_INT |
				   GPIO_INT_EDGE | GPIO_INT_ACTIVE_HIGH);
#else
				   GPIO_DIR_IN | GPIO_INT | GPIO_PUD_PULL_UP |
				   GPIO_INT_LEVEL | GPIO_INT_ACTIVE_LOW);
#endif

		gpio_init_callback(&data->sdio_irq_gpio_cb,
				   sdio_irq_gpio_callback,
				   BIT(cfg->sdio_irq_gpio % 32));

		data->sdio_irq_gpio_cb.pin_group = cfg->sdio_irq_gpio / 32;

		if (gpio_add_callback(data->sdio_irq_gpio_dev,
				      &data->sdio_irq_gpio_cb))
			return -EINVAL;
	}
#endif

	/* enable mmc controller clock */
	acts_clock_peripheral_enable(cfg->clock_id);

	/* reset mmc controller */
	acts_reset_peripheral(cfg->reset_id);

	/* set initial clock */
	mmc_acts_set_clock(dev, 100000);

	/* enable mmc controller */
	mmc->en |= SD_EN_ENABLE;

	k_sem_init(&data->trans_done, 0, 1);

#ifdef CONFIG_MMC_ACTS_DMA
	data->dma_chan = dma_request(data->dma_dev, 0xff);
	if (!data->dma_chan)
		SYS_LOG_WRN("%s: dma_request failed err=%d", dev->config->name, data->dma_chan);
#endif

	cfg->irq_config(dev);

	dev->driver_api = &mmc_acts_driver_api;

	return 0;
}

#ifdef CONFIG_MMC_0
static void mmc_acts_irq_config_0(struct device *dev);
static struct acts_mmc_data mmc_acts_data_0;

static const struct acts_mmc_config mmc_acts_config_0 = {
	.id		= 0,
	.base		= (void *)MMC0_REG_BASE,

#if (CONFIG_MMC_0_IO_WIDTH == 4)
	.capability	= MMC_CAP_4_BIT_DATA | MMC_CAP_SD_HIGHSPEED,
#else
	.capability	= MMC_CAP_SD_HIGHSPEED,
#endif

	.clk_reg	= CMU_SDCLK,

	.clock_id	= CLOCK_ID_SD0,
	.reset_id	= RESET_ID_SD0,
	.dma_id		= DMA_ID_SD0,

	.data_reg_width = CONFIG_MMC_0_DATA_REG_WIDTH,

	.irq_config	= mmc_acts_irq_config_0,
	.flags		= 0,
};

DEVICE_AND_API_INIT(mmc0_acts, CONFIG_MMC_0_DEVICE_NAME, mmc_acts_init,
	    &mmc_acts_data_0, &mmc_acts_config_0,
	    POST_KERNEL, CONFIG_MMC_0_INIT_PRIORITY, &mmc_acts_driver_api);

static void mmc_acts_irq_config_0(struct device *dev)
{
	IRQ_CONNECT(IRQ_ID_SD0, CONFIG_IRQ_PRIO_NORMAL, mmc_acts_isr,
		    DEVICE_GET(mmc0_acts), 0);
	irq_enable(IRQ_ID_SD0);
}
#endif	/* CONFIG_MMC_0 */

#ifdef CONFIG_MMC_1
static void mmc_acts_irq_config_1(struct device *dev);
static struct acts_mmc_data mmc_acts_data_1;

static const struct acts_mmc_config mmc_acts_config_1 = {
	.id		= 1,
	.base		= (void *)MMC1_REG_BASE,
#if (CONFIG_MMC_1_IO_WIDTH == 4)
#if defined(CONFIG_MMC_SDIO)
	.capability		= MMC_CAP_4_BIT_DATA | MMC_CAP_SDIO_IRQ | \
				  		MMC_CAP_SD_HIGHSPEED,
#else
	.capability		= MMC_CAP_4_BIT_DATA | MMC_CAP_SD_HIGHSPEED,
#endif
#else
#if defined(CONFIG_MMC_SDIO)
	.capability		= MMC_CAP_SDIO_IRQ | MMC_CAP_SD_HIGHSPEED,
#else
	.capability		= MMC_CAP_SD_HIGHSPEED,
#endif
#endif

	.clk_reg	= CMU_SDCLK,

	.clock_id	= CLOCK_ID_SD1,
	.reset_id	= RESET_ID_SD1,
	.dma_id		= DMA_ID_SD1,
	.irq_config	= mmc_acts_irq_config_1,

	.data_reg_width	= CONFIG_MMC_1_DATA_REG_WIDTH,

	.flags		= 0,

#ifdef CONFIG_MMC_SDIO_GPIO_IRQ
	.sdio_irq_gpio_name	= BOARD_SDIO_IRQ_GPIO_NAME,
	.sdio_irq_gpio		= BOARD_SDIO_IRQ_GPIO,
#endif
};

DEVICE_AND_API_INIT(mmc1_acts, CONFIG_MMC_1_DEVICE_NAME, mmc_acts_init,
	    &mmc_acts_data_1, &mmc_acts_config_1,
	    POST_KERNEL, CONFIG_MMC_1_INIT_PRIORITY, &mmc_acts_driver_api);

static void mmc_acts_irq_config_1(struct device *dev)
{
	IRQ_CONNECT(IRQ_ID_SD1, CONFIG_IRQ_PRIO_NORMAL, mmc_acts_isr,
	    DEVICE_GET(mmc1_acts), 0);
	irq_enable(IRQ_ID_SD1);
}
#endif	/* CONFIG_MMC_1 */
