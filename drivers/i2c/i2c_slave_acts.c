/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief I2C master driver for Actions SoC
 */

#include <errno.h>
#include <kernel.h>
#include <string.h>
#include <i2c_slave.h>
#include <soc.h>

#define SYS_LOG_DOMAIN "I2C_SLAVE"
#define SYS_LOG_LEVEL CONFIG_SYS_LOG_I2C_LEVEL
#include <logging/sys_log.h>

/* I2Cx_CTL */
#define I2C_CTL_GRAS			(0x1 << 0)
#define		I2C_CTL_GRAS_ACK		0
#define		I2C_CTL_GRAS_NACK		I2C_CTL_GRAS
#define I2C_CTL_RB				(0x1 << 1)
#define I2C_CTL_GBCC_MASK		(0x3 << 2)
#define I2C_CTL_GBCC(x)			(((x) & 0x3) << 2)
#define		I2C_CTL_GBCC_NONE		I2C_CTL_GBCC(0)
#define		I2C_CTL_GBCC_START		I2C_CTL_GBCC(1)
#define		I2C_CTL_GBCC_STOP		I2C_CTL_GBCC(2)
#define		I2C_CTL_GBCC_RESTART		I2C_CTL_GBCC(3)
#define I2C_CTL_EN			(0x1 << 5)
#define I2C_CTL_IRQE			(0x1 << 6)
#define I2C_CTL_RX_IRQ_THREHOLD_4BYTES	(0x1 << 10)

/* I2Cx_CLKDIV */
#define I2C_CLKDIV_DIV_MASK		(0xff << 0)
#define I2C_CLKDIV_DIV(x)		(((x) & 0xff) << 0)

/* I2Cx_STAT */
#define I2C_STAT_RACK			(0x1 << 0)
#define I2C_STAT_BEB			(0x1 << 1)
#define I2C_STAT_IRQP			(0x1 << 2)
#define I2C_STAT_STPD			(0x1 << 4)
#define I2C_STAT_STAD			(0x1 << 5)
#define I2C_STAT_BBB			(0x1 << 6)
#define I2C_STAT_TCB			(0x1 << 7)
#define I2C_STAT_LBST			(0x1 << 8)
#define I2C_STAT_SAMB			(0x1 << 9)
#define I2C_STAT_SRGC			(0x1 << 10)

/* I2Cx_CMD */
#define I2C_CMD_SBE			(0x1 << 0)
#define I2C_CMD_AS_MASK			(0x7 << 1)
#define I2C_CMD_AS(x)			(((x) & 0x7) << 1)
#define I2C_CMD_RBE			(0x1 << 4)
#define I2C_CMD_SAS_MASK		(0x7 << 5)
#define I2C_CMD_SAS(x)			(((x) & 0x7) << 5)
#define I2C_CMD_DE			(0x1 << 8)
#define I2C_CMD_NS			(0x1 << 9)
#define I2C_CMD_SE			(0x1 << 10)
#define I2C_CMD_MSS			(0x1 << 11)
#define I2C_CMD_WRS			(0x1 << 12)
#define I2C_CMD_EXEC			(0x1 << 15)

/* I2Cx_FIFOCTL */
#define I2C_FIFOCTL_NIB			(0x1 << 0)
#define I2C_FIFOCTL_RFR			(0x1 << 1)
#define I2C_FIFOCTL_TFR			(0x1 << 2)

/* I2Cx_FIFOSTAT */
#define I2C_FIFOSTAT_CECB		(0x1 << 0)
#define I2C_FIFOSTAT_RNB		(0x1 << 1)
#define I2C_FIFOSTAT_RFE		(0x1 << 2)
#define I2C_FIFOSTAT_RFF		(0x1 << 3)
#define I2C_FIFOSTAT_TFE		(0x1 << 4)
#define I2C_FIFOSTAT_TFF		(0x1 << 5)
#define I2C_FIFOSTAT_WRS		(0x1 << 6)
#define I2C_FIFOSTAT_RFD_MASK		(0xf << 8)
#define I2C_FIFOSTAT_RFD_SHIFT		(8)
#define I2C_FIFOSTAT_TFD_MASK		(0xf << 12)
#define I2C_FIFOSTAT_TFD_SHIFT		(12)

/* extract fifo level from fifostat */
#define I2C_RX_FIFO_LEVEL(x)		(((x) >> 8) & 0xff)
#define I2C_TX_FIFO_LEVEL(x)		(((x) >> 12) & 0xff)


#define I2C_SLAVE_STATUS_IDLE		0
#define I2C_SLAVE_STATUS_READY		1
#define I2C_SLAVE_STATUS_START		2
#define I2C_SLAVE_STATUS_ADDRESS	3
#define I2C_SLAVE_STATUS_MASTER_WRITE	4
#define I2C_SLAVE_STATUS_MASTER_READ	5
#define I2C_SLAVE_STATUS_STOPED		6
#define I2C_SLAVE_STATUS_ERR		15

/* I2C controller */
struct i2c_acts_controller {
	volatile uint32_t ctl;
	volatile uint32_t clkdiv;
	volatile uint32_t stat;
	volatile uint32_t addr;
	volatile uint32_t txdat;
	volatile uint32_t rxdat;
	volatile uint32_t cmd;
	volatile uint32_t fifoctl;
	volatile uint32_t fifostat;
	volatile uint32_t datcnt;
	volatile uint32_t rcnt;
};

struct acts_i2c_slave_config {
	struct i2c_acts_controller *base;
	void (*irq_config_func)(struct device *dev);
	u32_t pclk_freq;
	u16_t clock_id;
	u16_t reset_id;
};

struct acts_i2c_slave_adapter
{
	u16_t i2c_addr;
	u16_t status;
	i2c_rx_callback_t rx_cbk;

	u8_t *rx_buf;
	u8_t *tx_buf;
	u16_t rx_buf_size;
	u16_t tx_buf_size;
	u16_t rx_offs;
	u16_t tx_offs;
};

static void i2c_acts_dump_regs(struct i2c_acts_controller *i2c)
{
	SYS_LOG_INF("I2C base 0x%x:\n"
		"  ctl:      %x  clkdiv: %x  stat:    %x\n"
		"  addr:     %x  cmd:    %x  fifoctl: %x\n"
		"  fifostat: %x  datcnt: %x  rcnt:    %x\n",
		(unsigned int)i2c,
		i2c->ctl, i2c->clkdiv, i2c->stat,
		i2c->addr, i2c->cmd, i2c->fifoctl,
		i2c->fifostat, i2c->datcnt, i2c->rcnt);
}

static void i2c_acts_reset(struct i2c_acts_controller *i2c)
{
	/* reenable i2c controller */
	i2c->ctl = 0;

	/* clear i2c status */
	i2c->stat = 0xff;

	/* clear i2c fifo status */
	i2c->fifoctl = I2C_FIFOCTL_RFR | I2C_FIFOCTL_TFR;

	/* wait until fifo reset complete */
	while(i2c->fifoctl & (I2C_FIFOCTL_RFR | I2C_FIFOCTL_TFR))
		;
}

static void i2c_slave_acts_isr(void *arg)
{
	struct device *dev = (struct device *)arg;
	const struct acts_i2c_slave_config *config = dev->config->config_info;
	struct acts_i2c_slave_adapter *adapter = dev->driver_data;
	struct i2c_acts_controller *i2c = config->base;
	u32_t stat;
	u16_t addr;

	stat = i2c->stat;

	/* clear pending */
	i2c->stat = I2C_STAT_IRQP;

	/* check error */
	if (stat & I2C_STAT_BEB) {
		SYS_LOG_ERR("bus error\n");
		i2c_acts_dump_regs(i2c);
		i2c_acts_reset(i2c);
		adapter->status = I2C_SLAVE_STATUS_ERR;
		goto out;
	}

	/* detected start signal */
	if (stat & I2C_STAT_STAD) {
		i2c->stat = I2C_STAT_STAD;

		adapter->tx_offs = 0;
		adapter->rx_offs = 0;
		adapter->status = I2C_SLAVE_STATUS_START;
	}

	/* recieved address or data  */
	if (stat & I2C_STAT_TCB) {
		i2c->stat = I2C_STAT_TCB;
		if (!(i2c->stat & I2C_STAT_LBST)) {
			/* receive address */
			adapter->status = I2C_SLAVE_STATUS_ADDRESS;
			addr = i2c->rxdat;
			if ((addr >> 1) != adapter->i2c_addr) {
				SYS_LOG_ERR("bus address (0x%x) not matched with (0x%x)\n",
					addr >> 1, adapter->i2c_addr >> 1);
				i2c->stat |= 0x1ff;
				goto out;
			}

			if (addr & 1) {
				/* master read */
				if(adapter->tx_offs < adapter->tx_buf_size)
					i2c->txdat = adapter->tx_buf[adapter->tx_offs++];
				else
					SYS_LOG_ERR("tx buf overflow");

				adapter->status = I2C_SLAVE_STATUS_MASTER_READ;
			} else {
				/* master write */
				i2c->ctl &= ~I2C_CTL_GRAS_NACK;
				adapter->status = I2C_SLAVE_STATUS_MASTER_WRITE;
			}
		} else {
			/* receive data */
			if (adapter->status == I2C_SLAVE_STATUS_MASTER_READ) {
				/* master <--- slave */
				if (!(stat & I2C_STAT_RACK)) {
					/* received NACK */
					goto out;
				}
				if(adapter->tx_offs < adapter->tx_buf_size)
					i2c->txdat = adapter->tx_buf[adapter->tx_offs++];
				else
					SYS_LOG_ERR("tx buf overflow");
		    } else {
				/* master ---> slave */
				if(adapter->rx_offs < adapter->rx_buf_size)
					adapter->rx_buf[adapter->rx_offs++] = i2c->rxdat;
				else
					SYS_LOG_ERR("rx buf overflow");
			}
		}
	}

	/* detected stop signal */
	if (stat & I2C_STAT_STPD) {
		i2c->stat = I2C_STAT_STPD;

                if (adapter->status == I2C_SLAVE_STATUS_MASTER_WRITE &&
		    adapter->rx_cbk != NULL)
                    adapter->rx_cbk(dev, adapter->rx_buf, adapter->rx_offs);
	}
out:
	i2c->ctl |= I2C_CTL_RB;
}

static int i2c_slave_acts_register(struct device *dev, u16_t i2c_addr,
		    i2c_rx_callback_t rx_cbk)
{
	const struct acts_i2c_slave_config *config = dev->config->config_info;
	struct acts_i2c_slave_adapter *adapter = dev->driver_data;
	struct i2c_acts_controller *i2c = config->base;

	adapter->i2c_addr = i2c_addr;
	adapter->status = 1;
	adapter->rx_cbk = rx_cbk;

	adapter->status = I2C_SLAVE_STATUS_READY;

	i2c->addr = i2c_addr << 1;
	i2c->ctl = I2C_CTL_IRQE | I2C_CTL_EN | I2C_CTL_RB | I2C_CTL_GRAS_ACK;

	return 0;
}

static void i2c_slave_acts_unregister(struct device *dev, u16_t i2c_addr)
{
	const struct acts_i2c_slave_config *config = dev->config->config_info;
	struct acts_i2c_slave_adapter *adapter = dev->driver_data;
	struct i2c_acts_controller *i2c = config->base;

	i2c->ctl = 0;
	i2c->addr = 0;

	adapter->i2c_addr = 0;
	adapter->rx_cbk = NULL;
	adapter->status = I2C_SLAVE_STATUS_IDLE;
}

static int i2c_slave_acts_update_tx_data(struct device *dev, u16_t i2c_addr,
				  const u8_t *buf, int count)
{
	struct acts_i2c_slave_adapter *adapter = dev->driver_data;

	if (!buf || count > adapter->tx_buf_size) {
		return -1;
	}

	memcpy(adapter->tx_buf, buf, count);

	return 0;
}

static void i2c_slave_acts_set_clk(struct i2c_acts_controller *i2c,
		u32_t pclk_freq, u32_t clk_freq)
{
	u32_t div;

	if ((pclk_freq == 0) || (clk_freq == 0))
		return;

	div = (pclk_freq + clk_freq * 16 - 1) / (clk_freq * 16);
	i2c->clkdiv = I2C_CLKDIV_DIV(div);
}

static int i2c_slave_acts_init(struct device *dev)
{
	const struct acts_i2c_slave_config *config = dev->config->config_info;
	struct acts_i2c_slave_adapter *adapter = dev->driver_data;

	adapter->status = I2C_SLAVE_STATUS_IDLE;

	/* enable i2c controller clock */
	acts_clock_peripheral_enable(config->clock_id);

	/* reset i2c controller */
	acts_reset_peripheral(config->reset_id);

	config->irq_config_func(dev);

	/* setup default clock to 100K */
	i2c_slave_acts_set_clk(config->base, config->pclk_freq, 100000);

	return 0;
}

static const struct i2c_slave_driver_api i2c_slave_acts_driver_api = {
	.register_slave = i2c_slave_acts_register,
	.unregister_slave = i2c_slave_acts_unregister,
	.update_tx_data = i2c_slave_acts_update_tx_data,
};

#ifdef CONFIG_I2C_SLAVE_0
static void i2c_slave_acts_config_func_0(struct device *dev);

static const struct acts_i2c_slave_config acts_i2c_slave_config_0 = {
	.base = (struct i2c_acts_controller *)I2C0_REG_BASE,
	.irq_config_func = i2c_slave_acts_config_func_0,
	.pclk_freq = (CONFIG_HOSC_CLK_MHZ*1000000),
	.clock_id = CLOCK_ID_I2C0,
	.reset_id = RESET_ID_I2C0,
};

static u8_t acts_i2c_slave_rx_buf[CONFIG_I2C_SLAVE_0_RX_BUF_SIZE];
static u8_t acts_i2c_slave_tx_buf[CONFIG_I2C_SLAVE_0_TX_BUF_SIZE];

static struct acts_i2c_slave_adapter acts_i2c_slave_adapter_0 = {
	.rx_buf = acts_i2c_slave_rx_buf,
	.rx_buf_size = sizeof(acts_i2c_slave_rx_buf),
	.tx_buf = acts_i2c_slave_tx_buf,
	.tx_buf_size = sizeof(acts_i2c_slave_tx_buf),
};

DEVICE_DEFINE(i2c_slave_0, CONFIG_I2C_SLAVE_0_NAME, &i2c_slave_acts_init,
	      NULL, &acts_i2c_slave_adapter_0, &acts_i2c_slave_config_0,
	      POST_KERNEL, CONFIG_I2C_SLAVE_INIT_PRIORITY,
	      &i2c_slave_acts_driver_api);

static void i2c_slave_acts_config_func_0(struct device *dev)
{
	IRQ_CONNECT(IRQ_ID_I2C0, CONFIG_I2C_SLAVE_0_IRQ_PRI,
		    i2c_slave_acts_isr, DEVICE_GET(i2c_slave_0), 0);

	irq_enable(IRQ_ID_I2C0);
}
#endif /* CONFIG_I2C_SLAVE_0 */

#ifdef CONFIG_I2C_SLAVE_1
static void i2c_slave_acts_config_func_1(struct device *dev);

static const struct acts_i2c_slave_config acts_i2c_slave_config_1 = {
	.base = (struct i2c_acts_controller *)I2C1_REG_BASE,
	.irq_config_func = i2c_slave_acts_config_func_1,
	.pclk_freq = (CONFIG_HOSC_CLK_MHZ*1000000),
	.clock_id = CLOCK_ID_I2C1,
	.reset_id = RESET_ID_I2C1,
};

static u8_t acts_i2c_slave_1_rx_buf[CONFIG_I2C_SLAVE_1_RX_BUF_SIZE];
static u8_t acts_i2c_slave_1_tx_buf[CONFIG_I2C_SLAVE_1_TX_BUF_SIZE];

static struct acts_i2c_slave_adapter acts_i2c_slave_adapter_0 = {
	.rx_buf = acts_i2c_slave_1_rx_buf,
	.rx_buf_size = sizeof(acts_i2c_slave_1_rx_buf),
	.tx_buf = acts_i2c_slave_1_tx_buf,
	.tx_buf_size = sizeof(acts_i2c_slave_1_tx_buf),
};

DEVICE_DEFINE(i2c_slave_1, CONFIG_I2C_SLAVE_1_NAME, &i2c_slave_acts_init,
	      NULL, &acts_i2c_slave_adapter_1, &acts_i2c_slave_config_1,
	      POST_KERNEL, CONFIG_I2C_SLAVE_INIT_PRIORITY,
	      &i2c_slave_acts_driver_api);

static void i2c_slave_acts_config_func_0(struct device *dev)
{
	IRQ_CONNECT(IRQ_ID_I2C1, CONFIG_I2C_SLAVE_1_IRQ_PRI,
		    i2c_slave_acts_isr, DEVICE_GET(i2c_slave_1), 0);

	irq_enable(IRQ_ID_I2C1);
}
#endif /* CONFIG_I2C_SLAVE_0 */
