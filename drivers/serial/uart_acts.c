/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @brief UART Driver for Actions SoC
 */

#include <kernel.h>
#include <device.h>
#include <string.h>
#include <uart.h>
#include <soc.h>
#include <board.h>
#include <logic.h>
#ifdef CONFIG_STUB_DEV_UART
#include <uart_stub.h>
#endif


#if defined(CONFIG_ACTIONS_TRACE)
#include <trace.h>
#include <cbuf.h>
extern int debug_uart_trace_set_uart(uint8_t uart_idx, uint8_t io_idx, uint32_t uart_baud);
extern int debug_uart_trace_set_mode(trace_mode_e mode, uint8_t dma_chan_idx, cbuf_t *cbuf);
#endif

//减小波特率计算误差，将小数值进行进位处理
#define UART_BAUD_DIV(freq, baud_rate)  (((freq) / (baud_rate))) + ((((freq) % (baud_rate)) + ((baud_rate) >> 1)) / baud_rate)

/**
 * @brief Poll the device for input.
 *
 * @param dev UART device struct
 * @param c Pointer to character
 *
 * @return 0 if a character arrived, -1 if the input buffer if empty.
 */

static int uart_acts_poll_in(struct device *dev, unsigned char *c)
{
	struct acts_uart_controller *uart = UART_STRUCT(dev);

	/* Wait for transmitter to be ready */
	while (uart->stat & UART_STA_RFEM)
		;

	/* got a character */
	*c = (unsigned char)uart->rxdat;

	return 0;
}

/**
 * @brief Output a character in polled mode.
 *
 * Checks if the transmitter is empty. If empty, a character is written to
 * the data register.
 *
 * @param dev UART device struct
 * @param c Character to send
 *
 * @return Sent character
 */
static unsigned char uart_acts_poll_out(struct device *dev,
					     unsigned char c)
{
	struct acts_uart_controller *uart = UART_STRUCT(dev);

	/* Wait for transmitter to be ready */
	while (uart->stat & UART_STA_TFFU)
		;

	/* send a character */
	uart->txdat = (u32_t)c;
	return c;
}

static int uart_acts_err_check(struct device *dev)
{
	struct acts_uart_controller *uart = UART_STRUCT(dev);
	u32_t error = 0;

	if (uart->stat & UART_STA_ERRS) {
		if (uart->stat & (UART_STA_TFER | UART_STA_TFER))
			error |= UART_ERROR_OVERRUN;

		if (uart->stat & (UART_STA_STER | UART_STA_RXER))
			error |= UART_ERROR_FRAMING;

		if (uart->stat & UART_STA_PAER)
			error |= UART_ERROR_PARITY;

		/* clear error status */
		uart->stat = UART_STA_ERRS;
	}

	return error;
}

#ifdef CONFIG_UART_INTERRUPT_DRIVEN

/** Interrupt driven FIFO fill function */
static int uart_acts_fifo_fill(struct device *dev, const u8_t *tx_data, int len)
{
	struct acts_uart_controller *uart = UART_STRUCT(dev);
	u8_t num_tx = 0;

	while ((len - num_tx > 0) && !(uart->stat & UART_STA_TFFU)) {
		/* Send a character */
		uart->txdat = (u8_t)tx_data[num_tx++];
	}

	/* Clear the interrupt */
	uart->stat = UART_STA_TIP;

	return (int)num_tx;
}

/** Interrupt driven FIFO read function */
static int uart_acts_fifo_read(struct device *dev, u8_t *rx_data, const int size)
{
	struct acts_uart_controller *uart = UART_STRUCT(dev);
	u8_t num_rx = 0;

	while ((size - num_rx > 0) && !(uart->stat & UART_STA_RFEM)) {
		/* Receive a character */
		rx_data[num_rx++] = (u8_t)uart->rxdat;
#ifdef CONFIG_LOGIC_ANALYZER
		static unsigned char status=0;
		if ( 1 == num_rx ) {
			if (rx_data[0] == 'u') {
				logic_set_1(39);
			} else if (rx_data[0] == 'd') {
				logic_set_0(39);
			} else if (rx_data[0] == 'r') {
				logic_set_0(0);
				logic_set_0(1);
				logic_set_0(38);
				logic_set_0(39);
				logic_set_0(40);
			} else if (rx_data[0] == 'b') {
				status = !status;
				if(status){
					logic_set_1(39);
				}else{
					logic_set_0(39);
				}
			}
		}
#endif

	}

	/* Clear the interrupt */
	uart->stat = UART_STA_RIP;

	return num_rx;
}

/** Interrupt driven transfer enabling function */
static void uart_acts_irq_tx_enable(struct device *dev)
{
	struct acts_uart_controller *uart = UART_STRUCT(dev);

	uart->ctrl |= UART_CTL_TX_IE;
}

/** Interrupt driven transfer disabling function */
static void uart_acts_irq_tx_disable(struct device *dev)
{
	struct acts_uart_controller *uart = UART_STRUCT(dev);

	uart->ctrl &= ~UART_CTL_TX_IE;
}

/** Interrupt driven transfer ready function */
static int uart_acts_irq_tx_ready(struct device *dev)
{
	struct acts_uart_controller *uart = UART_STRUCT(dev);

	return !(uart->stat & UART_STA_TFFU);
}

/** Interrupt driven receiver enabling function */
static void uart_acts_irq_rx_enable(struct device *dev)
{
	struct acts_uart_controller *uart = UART_STRUCT(dev);

	uart->ctrl |= UART_CTL_RX_IE;
}

/** Interrupt driven receiver disabling function */
static void uart_acts_irq_rx_disable(struct device *dev)
{
	struct acts_uart_controller *uart = UART_STRUCT(dev);

	uart->ctrl &= ~UART_CTL_RX_IE;
}

/** Interrupt driven transfer empty function */
static int uart_acts_irq_tx_complete(struct device *dev)
{
	struct acts_uart_controller *uart = UART_STRUCT(dev);

	return (uart->stat & UART_STA_TFES);
}

/** Interrupt driven receiver ready function */
static int uart_acts_irq_rx_ready(struct device *dev)
{
	struct acts_uart_controller *uart = UART_STRUCT(dev);

	return !(uart->stat & UART_STA_RFEM);
}

/** Interrupt driven pending status function */
static int uart_acts_irq_is_pending(struct device *dev)
{
	struct acts_uart_controller *uart = UART_STRUCT(dev);
	int tx_pending, rx_pending;

	tx_pending = (uart->ctrl & UART_CTL_TX_IE) && (uart->stat & UART_STA_TIP);
	rx_pending = (uart->ctrl & UART_CTL_RX_IE) && (uart->stat & UART_STA_RIP);

	return (tx_pending || rx_pending);
}

/** Interrupt driven interrupt update function */
static int uart_acts_irq_update(struct device *dev)
{
	return 1;
}

/** Set the callback function */
static void uart_acts_irq_callback_set(struct device *dev, uart_irq_callback_t cb)
{
	struct acts_uart_data * const dev_data = DEV_DATA(dev);

	dev_data->cb = cb;
}

/**
 * @brief Interrupt service routine.
 *
 * This simply calls the callback function, if one exists.
 *
 * @param arg Argument to ISR.
 *
 * @return N/A
 */
void uart_acts_isr(void *arg)
{
	struct device *dev = arg;
	struct acts_uart_data * const dev_data = DEV_DATA(dev);
	struct acts_uart_controller *uart = UART_STRUCT(dev);

	/* clear error status */
	uart->stat = UART_STA_ERRS;

	if (dev_data->cb) {
		dev_data->cb(dev);
	}
}
#endif /* CONFIG_UART_INTERRUPT_DRIVEN */

static int uart_acts_fifo_switch(struct device *dev, u32_t is_tx, u32_t fifo_type)
{
    int tmp;

    struct acts_uart_controller *uart = UART_STRUCT(dev);

    tmp = uart->ctrl;

    if(is_tx){
        if(fifo_type > UART_FIFO_DSP){
            return -EINVAL;
        }

        //disable uart tx dma, tx threshold
        tmp &= (~(UART_CTL_TX_DST_MASK | UART_CTL_TX_FIFO_THREHOLD_MASK) | UART_CTL_TX_DE | UART_CTL_TX_IE);

        tmp |=  UART_CTL_TX_DST(fifo_type) | UART_CTL_TX_DE;

        if(fifo_type == UART_FIFO_DMA){
            //dma

            tmp |= UART_CTL_TX_FIFO_THREHOLD_8BYTES;
        }else{
            tmp |= UART_CTL_TX_FIFO_THREHOLD_1BYTE;
        }
    }else{
        if(fifo_type > UART_FIFO_DMA){
            return -EINVAL;
        }

        //disable uart rx dma, rx threshold
        tmp &= (~(UART_CTL_RX_SRC_MASK | UART_CTL_RX_FIFO_THREHOLD_MASK));

        tmp |=  UART_CTL_RX_SRC(fifo_type) | UART_CTL_RX_DE;

        if(fifo_type == UART_FIFO_DMA){
            //dma
            tmp |= UART_CTL_RX_FIFO_THREHOLD_1BYTE;  //8BYTES;
        }else{
            tmp |= UART_CTL_RX_FIFO_THREHOLD_1BYTE;
        }
    }

    uart->ctrl = tmp;

    return 0;
}

#ifdef CONFIG_UART_DMA_DRIVEN

static int uart_acts_dma_send_init(struct device *dev, int channel, dma_stream_handle_t stream_handler, void *stream_data)
{
    struct dma_config config_data;
    struct dma_block_config head_block;

    struct acts_uart_data *uart_data = DEV_DATA(dev);
    struct acts_uart_config *uart_config = (struct acts_uart_config *)DEV_CFG(dev);

    uart_data->dma_dev = device_get_binding(CONFIG_DMA_0_NAME);
    if (NULL == uart_data->dma_dev){
        return -1;
    }

    uart_data->send_chan = dma_request(uart_data->dma_dev, channel);
    if (uart_data->send_chan == 0U) {
        return -ENOMEM;
    }

    memset(&config_data, 0, sizeof(config_data));
    memset(&head_block, 0, sizeof(head_block));
    config_data.channel_direction = MEMORY_TO_PERIPHERAL;
    config_data.dma_slot = uart_config->dma_id;
    config_data.dest_data_size = 1;

    config_data.dma_callback = stream_handler;
    config_data.callback_data = stream_data;
    config_data.complete_callback_en = (stream_handler != NULL) ? 1 : 0;

    head_block.source_address = (unsigned int)NULL;
    head_block.dest_address = (unsigned int)NULL;
    head_block.block_size = 0;
    head_block.source_reload_en = 0;

    config_data.head_block = &head_block;

    dma_config(uart_data->dma_dev, uart_data->send_chan, &config_data);

    return 0;
}

static int uart_acts_dma_send(struct device *dev, char *s, int len)
{
    __ASSERT(dev, "no dev\n");
    struct acts_uart_data *uart_data = DEV_DATA(dev);
    __ASSERT(uart_data, "uart_data\n");
    __ASSERT(uart_data->dma_dev, "uart dma dev not config\n");
    __ASSERT(uart_data->send_chan, "uart dma channel not config\n");

    dma_reload(uart_data->dma_dev, uart_data->send_chan, (unsigned int) s, 0, len);

    dma_start(uart_data->dma_dev, uart_data->send_chan);

    return 0;
}

static int uart_acts_dma_send_complete(struct device *dev)
{
    __ASSERT(dev, "no dev\n");
    struct acts_uart_data *uart_data = DEV_DATA(dev);
    __ASSERT(uart_data, "uart_data\n");
    __ASSERT(uart_data->dma_dev, "uart dma dev not config\n");
    __ASSERT(uart_data->send_chan, "uart dma channel not config\n");

    return (dma_get_remain_size(uart_data->dma_dev, uart_data->send_chan) == 0);
}


static int uart_acts_dma_send_stop(struct device *dev)
{
    SYS_IRQ_FLAGS flags;

    __ASSERT(dev, "no dev\n");
    struct acts_uart_data *uart_data = DEV_DATA(dev);
    __ASSERT(uart_data, "uart_data\n");
    __ASSERT(uart_data->dma_dev, "uart dma dev not config\n");

    sys_irq_lock(&flags);

    if(uart_data->send_chan){
        dma_stop(uart_data->dma_dev, uart_data->send_chan);

        dma_free(uart_data->dma_dev, uart_data->send_chan);

        uart_data->send_chan = 0;
    }

    sys_irq_unlock(&flags);

    return 0;
}

#endif


#ifdef CONFIG_UART_DMA_RX_DRIVEN
#ifdef CONFIG_UART_DMA_RX_TIMEOUT_DRIVEN
static void uart_acts_dma_recv_isr(struct device *dev, u32_t priv_data, int reson)
{
	struct acts_uart_data *uart_data = (struct acts_uart_data *)priv_data;

	uart_data->dma_stream_cb(dev, (uint32_t)uart_data->dma_stream_priv_data, reson);

    uart_data->last_dma_transfer_timestamp = k_cycle_get_32() / 24;

    uart_data->last_dma_remain_len = dma_get_remain_size(uart_data->dma_dev, uart_data->rev_chan);

}
#endif
static int uart_acts_dma_recv_init(struct device *dev, int channel,
    dma_stream_handle_t stream_handler, void *stream_data)
{
    struct dma_config config_data;
    struct dma_block_config head_block;

    struct acts_uart_data *uart_data = DEV_DATA(dev);
    struct acts_uart_config *uart_config = (struct acts_uart_config *)DEV_CFG(dev);

    uart_data->dma_dev = device_get_binding(CONFIG_DMA_0_NAME);

    uart_data->rev_chan = dma_request(uart_data->dma_dev, channel);
    if (uart_data->rev_chan == 0U) {
        return -ENOMEM;
    }

    memset(&config_data, 0, sizeof(config_data));
    memset(&head_block, 0, sizeof(head_block));
    config_data.channel_direction = PERIPHERAL_TO_MEMORY;
    config_data.dma_slot = uart_config->dma_id;
    config_data.source_data_size = 1;
    config_data.source_burst_length = 1;

    config_data.dma_callback = stream_handler;
    config_data.callback_data = stream_data;
    config_data.complete_callback_en = 1;
    config_data.half_complete_callback_en = 1;

    head_block.source_address = (unsigned int)NULL;
    head_block.dest_address = (unsigned int)NULL;
    head_block.block_size = 0;

    head_block.source_reload_en = 1;

    config_data.head_block = &head_block;

#ifdef CONFIG_UART_DMA_RX_TIMEOUT_DRIVEN
    config_data.dma_callback = uart_acts_dma_recv_isr;
    config_data.callback_data = (void *)uart_data;
	uart_data->dma_stream_cb = stream_handler;
	uart_data->dma_stream_priv_data = stream_data;
#endif
    dma_config(uart_data->dma_dev, uart_data->rev_chan, &config_data);

    return 0;
}

static int uart_acts_dma_recv_config(struct device *dev, u8_t *dst, u32_t len)
{
    struct acts_uart_data *uart_data = DEV_DATA(dev);

    dma_reload(uart_data->dma_dev, uart_data->rev_chan, 0, (u32_t)dst, len);

#ifdef CONFIG_UART_DMA_RX_TIMEOUT_DRIVEN
	uart_data->dma_buf_size = len;
	uart_data->dma_buf_ptr = dst;
#endif
    return 0;
}

static int uart_acts_dma_recv_start(struct device *dev)
{
    struct acts_uart_data *uart_data = DEV_DATA(dev);

    dma_start(uart_data->dma_dev, uart_data->rev_chan);
#ifdef CONFIG_UART_DMA_RX_TIMEOUT_DRIVEN
	uart_data->phy_dma_handle = dma_get_phy_channel(uart_data->dma_dev, uart_data->rev_chan);
	printk("uart %p recv dma %x phy handle %x %x\n", uart_data, uart_data->rev_chan, uart_data->phy_dma_handle, \
		sys_read32(uart_data->phy_dma_handle));
#endif

    return 0;
}

static int uart_acts_dma_recv_remain_len(struct device *dev)
{
    int remain_size;

    __ASSERT(dev, "no dev\n");
    struct acts_uart_data *uart_data = DEV_DATA(dev);
    __ASSERT(uart_data, "uart_data\n");
    __ASSERT(uart_data->dma_dev, "uart dma dev not config\n");
    __ASSERT(uart_data->rev_chan, "uart dma channel not config\n");

    remain_size = dma_get_remain_size(uart_data->dma_dev, uart_data->rev_chan);

    return remain_size;
}


static int uart_acts_dma_recv_stop(struct device *dev)
{
    SYS_IRQ_FLAGS flags;

    __ASSERT(dev, "no dev\n");
    struct acts_uart_data *uart_data = DEV_DATA(dev);
    __ASSERT(uart_data, "uart_data\n");
    __ASSERT(uart_data->dma_dev, "uart dma dev not config\n");

    sys_irq_lock(&flags);

    if(uart_data->rev_chan){
        dma_stop(uart_data->dma_dev, uart_data->rev_chan);

        dma_free(uart_data->dma_dev, uart_data->rev_chan);

        uart_data->rev_chan = 0;
    }

    sys_irq_unlock(&flags);

    return 0;
}

#ifdef CONFIG_UART_DMA_RX_TIMEOUT_DRIVEN

void acts_dma_pause(dma_regs_backup_t *backup_ptr, dma_reg_t *dma_reg);
void acts_dma_resume(dma_regs_backup_t *backup_ptr, dma_reg_t *dma_reg);

static int uart_acts_dma_timer_rx_transfer_restart(struct acts_uart_data *uart_data)
{
    dma_regs_backup_t dma_reg;
    uint32_t data_size;

    if(acts_dma_check_dma_pending((dma_reg_t *)uart_data->phy_dma_handle)){
        return 0;
    }

    /**stop dma*/
    acts_dma_pause(&dma_reg, (dma_reg_t *)uart_data->phy_dma_handle);

    data_size = dma_reg.framelen - dma_reg.frame_remainlen;

    if (data_size){
        /**copy dma remainder data*/
        if (data_size < (dma_reg.framelen >> 1)){
			//print_buffer((const void *)dma_reg.daddr0, 1, data_size, 16, -1);
            sco_dma_mem_copy(dma_reg.daddr0, dma_reg.daddr0 + dma_reg.framelen / 2, data_size, uart_data->phy_dma_handle);
        }else{
            data_size -= (dma_reg.framelen >> 1);
        }
    }

    acts_dma_resume(&dma_reg, (dma_reg_t *)uart_data->phy_dma_handle);

    return data_size;  // return dma data len
}

static void uart_acts_dma_recv_check_timer_handle(struct hrtimer *ttimer, void *expiry_fn_arg)
{

    uint32_t irq_flag;
    uint32_t dma_data_len;
    uint32_t rx_switch_flag;
	uint32_t cur_time = k_cycle_get_32() / 24;
    struct acts_uart_data *uart_data = CONTAINER_OF(ttimer, struct acts_uart_data, check_dma_timer);

	if(!uart_data->phy_dma_handle){
		return;
	}

    if((cur_time - uart_data->last_dma_transfer_timestamp) > uart_data->timeout_us){
        rx_switch_flag = false;
        irq_flag = irq_lock();
        uint32_t cur_dma_remain_len = dma_get_remain_size(uart_data->dma_dev, uart_data->phy_dma_handle);
        if (cur_dma_remain_len != uart_data->dma_buf_size \
			&& (cur_dma_remain_len == uart_data->last_dma_remain_len)){
            rx_switch_flag = true;
            dma_data_len = uart_acts_dma_timer_rx_transfer_restart(uart_data);
            uart_data->last_dma_remain_len = uart_data->dma_buf_size;
        }else{
            uart_data->last_dma_remain_len = cur_dma_remain_len;
            uart_data->last_dma_transfer_timestamp = k_cycle_get_32() / 24;
        }
        irq_unlock(irq_flag);

        if(rx_switch_flag){
            uart_data->timeout_cb(uart_data->priv_data, uart_data->dma_buf_ptr + uart_data->dma_buf_size / 2, dma_data_len);
        }
    }
}

static int uart_acts_dma_recv_set_timeout_start(struct device *dev, int timeout_us, uart_dma_recv_timeout_callback_t callback, void *priv_data)
{
    struct acts_uart_data *uart_data = DEV_DATA(dev);

	uart_data->timeout_cb = callback;
	uart_data->priv_data = priv_data;
    uart_data->timeout_us = timeout_us;

    hrtimer_init(&uart_data->check_dma_timer, uart_acts_dma_recv_check_timer_handle, NULL);
    hrtimer_start(&uart_data->check_dma_timer, 0, uart_data->timeout_us);

    return 0;
}

static int uart_acts_dma_recv_set_timeout_stop(struct device *dev)
{
    struct acts_uart_data *uart_data = DEV_DATA(dev);

	hrtimer_stop(&uart_data->check_dma_timer);
    return 0;
}
#endif
#endif

static void uart_acts_baud_rate_set(struct device *dev)
{
    int uart_div;
    struct acts_uart_data * dev_data = DEV_DATA(dev);
    struct acts_uart_controller *uart = UART_STRUCT(dev);

    uart_div = UART_BAUD_DIV(CONFIG_HOSC_CLK_MHZ * 1000000, dev_data->baud_rate);

    /* TXRX */
    uart->br = (uart_div | (uart_div << 16));
}

#ifdef CONFIG_STUB_DEV_UART
static int uart_acts_baud_rate_switch(struct device *dev, int mode)
{
    int flag;
    const struct acts_uart_config *dev_cfg = DEV_CFG(dev);
    struct acts_uart_data * dev_data = DEV_DATA(dev);
    struct acts_uart_controller *uart = UART_STRUCT(dev);

    switch(mode)
    {

    case UART_BR_ATT_MODE:
        dev_data->baud_rate = dev_cfg->att_baud_rate;
        break;
    default:
        dev_data->baud_rate = dev_cfg->baud_rate;
        break;
    }

    flag = irq_lock();
    while(uart->stat & UART_STA_UTBB);
    uart_acts_baud_rate_set(dev);
    irq_unlock(flag);

    return 0;
}
#endif
static int uart_acts_baud_config(struct device *dev, u32_t bps)
{
    int flag;
    struct acts_uart_data * dev_data = DEV_DATA(dev);
    struct acts_uart_controller *uart = UART_STRUCT(dev);

    dev_data->baud_rate = bps;

    flag = irq_lock();
    while(uart->stat & UART_STA_UTBB);
    uart_acts_baud_rate_set(dev);
    irq_unlock(flag);

    return 0;
}

static const struct uart_driver_api uart_acts_driver_api = {
	.poll_in          = uart_acts_poll_in,
	.poll_out         = uart_acts_poll_out,
	.err_check        = uart_acts_err_check,

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	.fifo_fill        = uart_acts_fifo_fill,
	.fifo_read        = uart_acts_fifo_read,
	.irq_tx_enable    = uart_acts_irq_tx_enable,
	.irq_tx_disable   = uart_acts_irq_tx_disable,
	.irq_tx_ready     = uart_acts_irq_tx_ready,
	.irq_rx_enable    = uart_acts_irq_rx_enable,
	.irq_rx_disable   = uart_acts_irq_rx_disable,
	.irq_tx_complete  = uart_acts_irq_tx_complete,
	.irq_rx_ready     = uart_acts_irq_rx_ready,
	.irq_is_pending   = uart_acts_irq_is_pending,
	.irq_update       = uart_acts_irq_update,
	.irq_callback_set = uart_acts_irq_callback_set,
#endif
    .fifo_switch      = uart_acts_fifo_switch,
#ifdef CONFIG_UART_DMA_DRIVEN

#ifdef CONFIG_UART_DMA_TX_DRIVEN
    .dma_send_init    = uart_acts_dma_send_init,
    .dma_send         = uart_acts_dma_send,
    .dma_send_complete= uart_acts_dma_send_complete,
    .dma_send_stop    = uart_acts_dma_send_stop,
#endif

#ifdef CONFIG_UART_DMA_RX_DRIVEN
    .dma_recv_init = uart_acts_dma_recv_init,
    .dma_recv_config = uart_acts_dma_recv_config,
    .dma_recv_start = uart_acts_dma_recv_start,
    .dma_recv_remain_len = uart_acts_dma_recv_remain_len,
    .dma_recv_stop = uart_acts_dma_recv_stop,
#ifdef CONFIG_UART_DMA_RX_TIMEOUT_DRIVEN
    .dma_recv_set_timeout_start = uart_acts_dma_recv_set_timeout_start,
	.dma_recv_set_timeout_stop  = uart_acts_dma_recv_set_timeout_stop,
#endif
#endif
#endif

#ifdef CONFIG_STUB_DEV_UART
    .baud_rate_switch = uart_acts_baud_rate_switch,
#endif
	.baud_rate_set = uart_acts_baud_config,
};

/**
 * @brief Initialize UART channel
 *
 * This routine is called to reset the chip in a quiescent state.
 * It is assumed that this function is called only once per UART.
 *
 * @param dev UART device struct
 *
 * @return 0
 */
static int uart_acts_init(struct device *dev)
{
    int uart_div;
	const struct acts_uart_config *dev_cfg = DEV_CFG(dev);
	struct acts_uart_data * dev_data = DEV_DATA(dev);
	struct acts_uart_controller *uart = UART_STRUCT(dev);

	/* enable uart clock */
	acts_clock_peripheral_enable(dev_cfg->clock_id);
	/* clear reset */
	acts_reset_peripheral_deassert(dev_cfg->reset_id);

#if defined(CONFIG_ACTIONS_TRACE)
	if (UART0_REG_BASE == dev_cfg->base) {
		debug_uart_trace_set_uart(0, 0, CONFIG_UART_ACTS_PORT_0_BAUD_RATE);
		debug_uart_trace_set_mode(1, 0, NULL); /* TRACE_MODE_CPU = 1 */
	}
#endif

	/* enable rx/tx, 8/1/n
	 * TX_DE/RX_DE 必须置位，否则 UART 外设输出驱动级关闭，引脚无波形。
	 */
	uart->ctrl = UART_CTL_EN |
		UART_CTL_RX_EN | UART_CTL_RX_FIFO_EN | UART_CTL_RX_FIFO_THREHOLD_1BYTE
		| UART_CTL_RX_DE |
		UART_CTL_TX_EN | UART_CTL_TX_FIFO_EN | UART_CTL_TX_FIFO_THREHOLD_1BYTE
		| UART_CTL_TX_DE |
		UART_CTL_DATA_WIDTH_8BIT | UART_CTL_STOP_1BIT | UART_CTL_PARITY_NONE;

    if(dev_data->baud_rate == 0)
    {
        dev_data->baud_rate = dev_cfg->baud_rate;
    }

#if defined(CONFIG_SOC_SERIES_ANDES) || defined(CONFIG_SOC_SERIES_ANDESC)
	uart_div = UART_BAUD_DIV(CONFIG_HOSC_CLK_MHZ * 1000000, dev_data->baud_rate);

    /* 同时设置TX和RX的波特率 */
	uart->br = (uart_div | (uart_div << 16));

#else
	/* clock source: 26M, bordrate: 115200 */
	uart->br = 0x00e200e2;
#endif

	/* clear error status */
	uart->stat = UART_STA_ERRS;

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	DEV_CFG(dev)->irq_config_func(dev);
#endif

#ifdef CONFIG_UART_DMA_RX_TIMEOUT_DRIVEN
	dev_data->phy_dma_handle = 0;
#endif

	return 0;
}

#ifdef CONFIG_UART_ACTS_PORT_0

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
/* Forward declare function */
static void uart_acts_0_irq_config(struct device *port);
#endif

static const struct acts_uart_config uart_acts_dev_cfg_0 = {
	.base = UART0_REG_BASE,
	.clock_id = CLOCK_ID_UART0,
	.reset_id = RESET_ID_UART0,
    .dma_id = DMA_ID_UART0,
	.baud_rate = CONFIG_UART_ACTS_PORT_0_BAUD_RATE,

#ifdef CONFIG_STUB_DEV_UART
    .att_baud_rate = CONFIG_UART_STUB_BAUD_RATE,
#endif

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	.irq_config_func = uart_acts_0_irq_config,
#endif
};

static struct acts_uart_data uart_acts_dev_data_0;

DEVICE_AND_API_INIT(uart_acts_0, CONFIG_UART_ACTS_PORT_0_NAME, uart_acts_init,
		    &uart_acts_dev_data_0, &uart_acts_dev_cfg_0,
		    PRE_KERNEL_1, CONFIG_UART_ACTS_PORT_0_PRIORITY,
		    &uart_acts_driver_api);

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
static void uart_acts_0_irq_config(struct device *dev)
{
	struct acts_uart_controller *uart = UART_STRUCT(dev);

	/* clear pending irq */
	uart->stat = UART_STA_TIP | UART_STA_RIP;

	IRQ_CONNECT(IRQ_ID_UART0, CONFIG_IRQ_PRIO_NORMAL,
		    uart_acts_isr, DEVICE_GET(uart_acts_0), 0);
	irq_enable(IRQ_ID_UART0);
}
#endif /* CONFIG_UART_INTERRUPT_DRIVEN */
#endif /* CONFIG_UART_ACTS_PORT_0 */

#ifdef CONFIG_UART_ACTS_PORT_1

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
/* Forward declare function */
static void uart_acts_1_irq_config(struct device *port);
#endif

static const struct acts_uart_config uart_acts_dev_cfg_1 = {
	.base = UART1_REG_BASE,
	.clock_id = CLOCK_ID_UART1,
	.reset_id = RESET_ID_UART1,
    .dma_id = DMA_ID_UART1,
	.baud_rate = CONFIG_UART_ACTS_PORT_1_BAUD_RATE,

#ifdef CONFIG_STUB_DEV_UART
    .att_baud_rate = CONFIG_UART_STUB_BAUD_RATE,
#endif

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	.irq_config_func = uart_acts_1_irq_config,
#endif
};

static struct acts_uart_data uart_acts_dev_data_1;

DEVICE_AND_API_INIT(uart_acts_1, CONFIG_UART_ACTS_PORT_1_NAME, uart_acts_init,
		    &uart_acts_dev_data_1, &uart_acts_dev_cfg_1,
		    PRE_KERNEL_1, CONFIG_UART_ACTS_PORT_1_PRIORITY,
		    &uart_acts_driver_api);

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
static void uart_acts_1_irq_config(struct device *dev)
{
	struct acts_uart_controller *uart = UART_STRUCT(dev);

	/* clear pending irq */
	uart->stat = UART_STA_TIP | UART_STA_RIP;

	IRQ_CONNECT(IRQ_ID_UART1, CONFIG_UART_ACTS_PORT_1_IRQ_PRIORITY,
		    uart_acts_isr, DEVICE_GET(uart_acts_1), 0);
	irq_enable(IRQ_ID_UART1);
}
#endif /* CONFIG_UART_INTERRUPT_DRIVEN */
#endif /* CONFIG_UART_ACTS_PORT_1 */
