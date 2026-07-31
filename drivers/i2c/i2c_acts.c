/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief I2C master driver for Actions SoC
 */

#include <errno.h>
#include <kernel.h>
#include <i2c.h>
#include <soc.h>

#define SYS_LOG_DOMAIN "I2C"
#define SYS_LOG_LEVEL CONFIG_SYS_LOG_I2C_LEVEL
#include <logging/sys_log.h>

#define I2C_TIMEOUT_MS				1000	/* 1000ms */

/* I2Cx_CTL */
#define I2C_CTL_GRAS				(0x1 << 0)
#define	I2C_CTL_GRAS_ACK			(0)
#define	I2C_CTL_GRAS_NACK			I2C_CTL_GRAS
#define I2C_CTL_RB					(0x1 << 1)
#define I2C_CTL_GBCC_MASK			(0x3 << 2)
#define I2C_CTL_GBCC(x)				(((x) & 0x3) << 2)
#define	I2C_CTL_GBCC_NONE			I2C_CTL_GBCC(0)
#define	I2C_CTL_GBCC_START			I2C_CTL_GBCC(1)
#define	I2C_CTL_GBCC_STOP			I2C_CTL_GBCC(2)
#define	I2C_CTL_GBCC_RESTART		I2C_CTL_GBCC(3)
#define I2C_CTL_EN					(0x1 << 5)
#define I2C_CTL_IRQE				(0x1 << 6)
#define I2C_CTL_RX_IRQ_THREHOLD_4BYTES	(0x1 << 10)

/* I2Cx_CLKDIV */
#define I2C_CLKDIV_DIV_MASK			(0xff << 0)
#define I2C_CLKDIV_DIV(x)			(((x) & 0xff) << 0)

/* I2Cx_STAT */
#define I2C_STAT_RACK				(0x1 << 0)
#define I2C_STAT_BEB				(0x1 << 1)
#define I2C_STAT_IRQP				(0x1 << 2)
#define I2C_STAT_STPD				(0x1 << 4)
#define I2C_STAT_STAD				(0x1 << 5)
#define I2C_STAT_BBB				(0x1 << 6)
#define I2C_STAT_TCB				(0x1 << 7)
#define I2C_STAT_LBST				(0x1 << 8)
#define I2C_STAT_SAMB				(0x1 << 9)
#define I2C_STAT_SRGC				(0x1 << 10)

/* I2Cx_CMD */
#define I2C_CMD_SBE					(0x1 << 0)
#define I2C_CMD_AS_MASK				(0x7 << 1)
#define I2C_CMD_AS(x)				(((x) & 0x7) << 1)
#define I2C_CMD_RBE					(0x1 << 4)
#define I2C_CMD_SAS_MASK			(0x7 << 5)
#define I2C_CMD_SAS(x)				(((x) & 0x7) << 5)
#define I2C_CMD_DE					(0x1 << 8)
#define I2C_CMD_NS					(0x1 << 9)
#define I2C_CMD_SE					(0x1 << 10)
#define I2C_CMD_MSS					(0x1 << 11)
#define I2C_CMD_WRS					(0x1 << 12)
#define I2C_CMD_EXEC				(0x1 << 15)

/* I2Cx_FIFOCTL */
#define I2C_FIFOCTL_NIB				(0x1 << 0)
#define I2C_FIFOCTL_RFR				(0x1 << 1)
#define I2C_FIFOCTL_TFR				(0x1 << 2)

/* I2Cx_FIFOSTAT */
#define I2C_FIFOSTAT_CECB			(0x1 << 0)
#define I2C_FIFOSTAT_RNB			(0x1 << 1)
#define I2C_FIFOSTAT_RFE			(0x1 << 2)
#define I2C_FIFOSTAT_RFF			(0x1 << 3)
#define I2C_FIFOSTAT_TFE			(0x1 << 4)
#define I2C_FIFOSTAT_TFF			(0x1 << 5)
#define I2C_FIFOSTAT_WRS			(0x1 << 6)
#define I2C_FIFOSTAT_RFD_MASK		(0xf << 8)
#define I2C_FIFOSTAT_RFD_SHIFT		(8)
#define I2C_FIFOSTAT_TFD_MASK		(0xf << 12)
#define I2C_FIFOSTAT_TFD_SHIFT		(12)

/* extract fifo level from fifostat */
#define I2C_RX_FIFO_LEVEL(x)		(((x) >> 8) & 0xf)
#define I2C_TX_FIFO_LEVEL(x)		(((x) >> 12) & 0xf)

enum i2c_state {
	STATE_INVALID,
	STATE_READ_DATA,
	STATE_WRITE_DATA,
	STATE_TRANSFER_OVER,
	STATE_TRANSFER_ERROR,
};

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

struct acts_i2c_config {
	struct i2c_acts_controller *base;
	void (*irq_config_func)(struct device *dev);
	union dev_config default_cfg;
	u32_t pclk_freq;
	u16_t clock_id;
	u16_t reset_id;
};

/* Device run time data */
struct acts_i2c_data {
	struct k_mutex mutex;
	union dev_config mode_config;

	struct i2c_msg *cur_msg;
	u32_t msg_buf_ptr;
	enum i2c_state state;
	u32_t clk_freq;

#ifdef CONFIG_DEVICE_POWER_MANAGEMENT
	u32_t device_power_state;
#endif
};

static void i2c_acts_dump_regs(struct i2c_acts_controller *i2c)
{
	SYS_LOG_INF("I2C base 0x%x:\n"
		"       ctl: %08x  clkdiv: %08x     stat: %08x\n"
		"      addr: %08x     cmd: %08x  fifoctl: %08x\n"
		"  fifostat: %08x  datcnt: %08x     rcnt: %08x\n",
		(unsigned int)i2c,
		i2c->ctl, i2c->clkdiv, i2c->stat,
		i2c->addr, i2c->cmd, i2c->fifoctl,
		i2c->fifostat, i2c->datcnt, i2c->rcnt);
}

static void i2c_acts_set_clk(struct i2c_acts_controller *i2c,
		u32_t pclk_freq, u32_t clk_freq)
{
	u32_t div;

	if ((pclk_freq == 0) || (clk_freq == 0))
		return;

	div = (pclk_freq + clk_freq * 16 - 1) / (clk_freq * 16);
	i2c->clkdiv = I2C_CLKDIV_DIV(div);

	return;
}

static int i2c_acts_configure(struct device *dev, u32_t dev_config_raw)
{
	const struct acts_i2c_config *config = dev->config->config_info;
	struct acts_i2c_data *data = dev->driver_data;
	struct i2c_acts_controller *i2c = config->base;

	data->mode_config = (union dev_config)dev_config_raw;

	switch (data->mode_config.bits.speed) {
	case I2C_SPEED_STANDARD:
		/* 100KHz */
		i2c_acts_set_clk(i2c, config->pclk_freq, 100000);
		break;
	case I2C_SPEED_FAST:
		/* 400KHz */
		i2c_acts_set_clk(i2c, config->pclk_freq, 400000);
		break;
	default:
		/* 50KHz */
		i2c_acts_set_clk(i2c, config->pclk_freq, 50000);
		break;
	}

	return 0;
}

static int i2c_acts_reset(struct i2c_acts_controller *i2c, u32_t timeout_ms)
{
	u32_t start_time, curr_time;

	/* reenable i2c controller */
	i2c->ctl = 0;

	/* clear i2c status */
	i2c->stat = 0xff;

	/* clear i2c fifo status */
	i2c->fifoctl = I2C_FIFOCTL_RFR | I2C_FIFOCTL_TFR;

	start_time = k_uptime_get();
	/* wait until fifo reset completly or timeout */
	while (i2c->fifoctl & (I2C_FIFOCTL_RFR | I2C_FIFOCTL_TFR)) {
		curr_time = k_uptime_get_32();
		if ((curr_time - start_time) >= timeout_ms) {
			SYS_LOG_ERR("i2c reset timeout");
			return -ETIMEDOUT;
		}
	}

	return 0;
}

static int i2c_acts_wait_complete(struct i2c_acts_controller *i2c, u32_t timeout_ms)
{
	u32_t start_time, curr_time;

	start_time = k_uptime_get();
	while (!(i2c->fifostat & I2C_FIFOSTAT_CECB)) {
		curr_time = k_uptime_get_32();
		if ((curr_time - start_time) >= timeout_ms) {
			SYS_LOG_ERR("wait i2c cmd done timeout");
			return -ETIMEDOUT;
		}
		k_sleep(1);
	}

	return 0;
}

void i2c_acts_isr(void *arg)
{
	struct device *dev = (struct device *)arg;
	const struct acts_i2c_config *config = dev->config->config_info;
	struct acts_i2c_data *data = dev->driver_data;
	struct i2c_acts_controller *i2c = config->base;
	struct i2c_msg *cur_msg = data->cur_msg;
	u32_t stat, fifostat;

	stat = i2c->stat;
	fifostat = i2c->fifostat;

	SYS_LOG_DBG("stat:0x%x fifostat:0x%x", stat, fifostat);

	/* check error */
	if (fifostat & I2C_FIFOSTAT_RNB) {
		SYS_LOG_ERR("no ACK from device");
		data->state = STATE_TRANSFER_ERROR;
		goto stop;
	} else if (stat & I2C_STAT_BEB) {
		SYS_LOG_ERR("bus error");
		data->state = STATE_TRANSFER_ERROR;
		goto stop;
	}

	if (data->state == STATE_READ_DATA) {
		/* read data from FIFO
		 * RFE: RX FIFO empty bit. 1: empty 0: not empty
		 */
		while (!(i2c->fifostat & I2C_FIFOSTAT_RFE) &&
			data->msg_buf_ptr < cur_msg->len) {
			cur_msg->buf[data->msg_buf_ptr++] = i2c->rxdat;
		}
	} else {
		/* write data to FIFO */
		while (!(i2c->fifostat & I2C_FIFOSTAT_TFF) &&
		       data->msg_buf_ptr < cur_msg->len) {
			i2c->txdat = cur_msg->buf[data->msg_buf_ptr++];
		}
	}

	/* all data is transfered? */
	if (data->msg_buf_ptr >= cur_msg->len) {
		data->state = STATE_TRANSFER_OVER;
	}

stop:
	i2c->stat |= I2C_STAT_IRQP;

	if (data->state == STATE_TRANSFER_ERROR ||
	    data->state == STATE_TRANSFER_OVER) {
		/* FIXME: add extra 2 bytes for TX to generate IRQ */
		if (i2c->datcnt != cur_msg->len) {
			i2c->datcnt = cur_msg->len;
		}
	}
}

static int i2c_acts_transfer(struct device *dev, struct i2c_msg *msgs,
			     u8_t num_msgs, u16_t addr)
{
	const struct acts_i2c_config *config = dev->config->config_info;
	struct acts_i2c_data *data = dev->driver_data;
	struct i2c_acts_controller *i2c = config->base;
	struct i2c_msg *cur_msg = msgs;
	int i, is_read, ret = 0;
	u32_t fifo_cmd, fifostat;
	u8_t msg_left = num_msgs;

	if (!num_msgs)
		return 0;

	k_mutex_lock(&data->mutex, K_FOREVER);

	device_busy_set(dev);

	ret = i2c_acts_reset(i2c, I2C_TIMEOUT_MS);
	if (ret){
		k_mutex_unlock(&data->mutex);
		return ret;
	}
	/* enable I2C controller IRQ */
	i2c->ctl = I2C_CTL_IRQE | I2C_CTL_EN;

	/* Process all the messages */
	while (msg_left > 0) {
		data->cur_msg = cur_msg;

		SYS_LOG_DBG("slave addr:0x%x msg_flags:0x%x msg_len:%d", addr, cur_msg->flags, cur_msg->len);

		is_read = ((cur_msg->flags & I2C_MSG_RW_MASK) == I2C_MSG_READ) ? 1 : 0;

		if (is_read) {
			/* write i2c device address */
			if ((msg_left == num_msgs) || (cur_msg->flags & I2C_MSG_RESTART))
				i2c->txdat = (addr << 1) | 1;

			i2c->datcnt = cur_msg->len;

			data->msg_buf_ptr = 0;
			data->state = STATE_READ_DATA;
		} else {
			/* write i2c device address */
			if ((msg_left == num_msgs) || (cur_msg->flags & I2C_MSG_RESTART))
				i2c->txdat = addr << 1;

			/* FIXME: add extra 2 bytes for TX to generate IRQ (TX FIFO Empty) */
			i2c->datcnt = cur_msg->len + 2;

			/* write data to FIFO */
			for (i = 0; i < cur_msg->len; i++) {
				if (i2c->fifostat & I2C_FIFOSTAT_TFF)
					break;
				i2c->txdat = cur_msg->buf[i];

				/* wait fifo stat is updated */
				fifostat = i2c->fifostat;
			}

			data->msg_buf_ptr = i;
			data->state = STATE_WRITE_DATA;
		}

		/* select master, data enable */
		fifo_cmd = I2C_CMD_EXEC | I2C_CMD_MSS | I2C_CMD_DE;

		/* send 1 byte device address if is the first message */
		if (msg_left == num_msgs)
			fifo_cmd |= I2C_CMD_SBE | I2C_CMD_AS(1);

		/* send 1 byte device address if is the restart message */
		if (cur_msg->flags & I2C_MSG_RESTART)
			fifo_cmd |= I2C_CMD_RBE | I2C_CMD_SAS(1);

		/* the last message? */
		if (msg_left == 1) {
			/* send NAK if is read data from device */
			if (is_read)
				fifo_cmd |= I2C_CMD_NS;

			/* send STOP */
			fifo_cmd |= I2C_CMD_SE;
		}

		/* write fifo command to start transfer */
		i2c->cmd = fifo_cmd;

		/* wait data really output to device */
		ret = i2c_acts_wait_complete(i2c, I2C_TIMEOUT_MS);
		if (ret) {
			/* wait timeout */
			SYS_LOG_ERR("addr 0x%x: wait timeout", addr);
			i2c_acts_dump_regs(i2c);
			ret = -ETIMEDOUT;
			break;
		}

		if (data->state == STATE_TRANSFER_ERROR) {
			SYS_LOG_ERR("addr 0x%x: transfer error", addr);
			i2c_acts_dump_regs(i2c);
			ret = -EIO;
			break;
		}

		cur_msg++;
		msg_left--;
	}

	/* disable i2c controller */
	i2c->ctl = 0;

	device_busy_clear(dev);

	k_mutex_unlock(&data->mutex);

	return ret;
}


int i2c_acts_init(struct device *dev)
{
	const struct acts_i2c_config *config = dev->config->config_info;
	struct acts_i2c_data *data = dev->driver_data;

	/* enable i2c controller clock */
	acts_clock_peripheral_enable(config->clock_id);

	/* reset i2c controller */
	acts_reset_peripheral(config->reset_id);

	/* setup default clock to 100K */
	i2c_acts_set_clk(config->base, config->pclk_freq, 100000);

	k_mutex_init(&data->mutex);

	config->irq_config_func(dev);

	return 0;
}

const struct i2c_driver_api i2c_acts_driver_api = {
	.configure = i2c_acts_configure,
	.transfer = i2c_acts_transfer,
};

#ifdef CONFIG_DEVICE_POWER_MANAGEMENT
static void i2c_acts_set_power_state(struct device *dev, u32_t power_state)
{
	struct acts_i2c_data *drv_data = dev->driver_data;

	drv_data->device_power_state = power_state;
}

static u32_t i2c_acts_get_power_state(struct device *dev)
{
	struct acts_i2c_data *drv_data = dev->driver_data;

	return drv_data->device_power_state;
}

static int i2c_suspend_device(struct device *dev)
{
	if (device_busy_check(dev)) {
		return -EBUSY;
	}

	i2c_acts_set_power_state(dev, DEVICE_PM_SUSPEND_STATE);

	return 0;
}

static int i2c_resume_device_from_suspend(struct device *dev)
{
	i2c_acts_set_power_state(dev, DEVICE_PM_ACTIVE_STATE);

	return 0;
}

/*
* Implements the driver control management functionality
* the *context may include IN data or/and OUT data
*/
int i2c_device_ctrl(struct device *dev, u32_t ctrl_command,
			   void *context)
{
	if (ctrl_command == DEVICE_PM_SET_POWER_STATE) {
		if (*((u32_t *)context) == DEVICE_PM_SUSPEND_STATE) {
			return i2c_suspend_device(dev);
		} else if (*((u32_t *)context) == DEVICE_PM_ACTIVE_STATE) {
			return i2c_resume_device_from_suspend(dev);
		}
	} else if (ctrl_command == DEVICE_PM_GET_POWER_STATE) {
		*((u32_t *)context) = i2c_acts_get_power_state(dev);
		return 0;
	}

	return 0;
}
#else
#define i2c_acts_set_power_state(...)
#endif /* CONFIG_DEVICE_POWER_MANAGEMENT */

#ifdef CONFIG_I2C_0
static void i2c_acts_config_func_0(struct device *dev);

static const struct acts_i2c_config acts_i2c_config_0 = {
	.base = (struct i2c_acts_controller *)I2C0_REG_BASE,
	.irq_config_func = i2c_acts_config_func_0,
	.pclk_freq = (CONFIG_HOSC_CLK_MHZ*1000000),
	.clock_id = CLOCK_ID_I2C0,
	.reset_id = RESET_ID_I2C0,
};

static struct acts_i2c_data acts_i2c_data_0;

DEVICE_DEFINE(i2c_0, CONFIG_I2C_0_NAME, &i2c_acts_init,
	      i2c_device_ctrl, &acts_i2c_data_0, &acts_i2c_config_0,
	      POST_KERNEL, CONFIG_I2C_INIT_PRIORITY,
	      &i2c_acts_driver_api);

static void i2c_acts_config_func_0(struct device *dev)
{
	IRQ_CONNECT(IRQ_ID_I2C0, CONFIG_I2C_0_IRQ_PRI,
		    i2c_acts_isr, DEVICE_GET(i2c_0), 0);

	irq_enable(IRQ_ID_I2C0);
}
#endif

#ifdef CONFIG_I2C_1
static void i2c_acts_config_func_1(struct device *dev);

static const struct acts_i2c_config acts_i2c_config_1 = {
	.base = (struct i2c_acts_controller *)I2C1_REG_BASE,
	.irq_config_func = i2c_acts_config_func_1,
	.pclk_freq = (CONFIG_HOSC_CLK_MHZ*1000000),
	.clock_id = CLOCK_ID_I2C1,
	.reset_id = RESET_ID_I2C1,
};

static struct acts_i2c_data acts_i2c_data_1;

DEVICE_DEFINE(i2c_1, CONFIG_I2C_1_NAME, &i2c_acts_init,
	      i2c_device_ctrl, &acts_i2c_data_1, &acts_i2c_config_1,
	      POST_KERNEL, CONFIG_I2C_INIT_PRIORITY,
	      &i2c_acts_driver_api);

static void i2c_acts_config_func_1(struct device *dev)
{
	IRQ_CONNECT(IRQ_ID_I2C1, CONFIG_I2C_1_IRQ_PRI,
		    i2c_acts_isr, DEVICE_GET(i2c_1), 0);

	irq_enable(IRQ_ID_I2C1);
}
#endif
