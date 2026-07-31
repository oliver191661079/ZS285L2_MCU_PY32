/*
 * Copyright (c) 2015 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public APIs for UART drivers
 */

#ifndef __INCuarth
#define __INCuarth

/**
 * @brief UART Interface
 * @defgroup uart_interface UART Interface
 * @ingroup io_interfaces
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>
#include <stddef.h>

#include <device.h>

#ifdef CONFIG_PCI
#include <drivers/pci/pci.h>
#include <drivers/pci/pci_mgr.h>
#endif

#ifdef CONFIG_UART_DMA_DRIVEN
#include <dma.h>
#endif
#ifdef CONFIG_UART_DMA_RX_TIMEOUT_DRIVEN
#include <hrtimer.h>
#endif
/**
 * @brief Options for @a UART initialization.
 */
#define UART_OPTION_AFCE 0x01

/** Common line controls for UART.*/
#define LINE_CTRL_BAUD_RATE	(1 << 0)
#define LINE_CTRL_RTS		(1 << 1)
#define LINE_CTRL_DTR		(1 << 2)
#define LINE_CTRL_DCD		(1 << 3)
#define LINE_CTRL_DSR		(1 << 4)

/* Common communication errors for UART.*/

/** @brief Overrun error */
#define UART_ERROR_OVERRUN  (1 << 0)

/** @brief Parity error */
#define UART_ERROR_PARITY   (1 << 1)

/** @brief Framing error */
#define UART_ERROR_FRAMING  (1 << 2)

/**
 * @brief Break interrupt error:
 *
 * A break interrupt was received. This happens when the serial input is
 * held at a logic '0' state for longer than the sum of start time + data bits
 * + parity + stop bits.
 */
#define UART_ERROR_BREAK    (1 << 3)

/**
 * @typedef uart_irq_callback_t
 * @brief Define the application callback function signature for UART.
 *
 * @param port Device struct for the UART device.
 */
typedef void (*uart_irq_callback_t)(struct device *port);

/**
 * @typedef uart_irq_config_func_t
 * @brief For configuring IRQ on each individual UART device.
 *
 * @internal
 */
typedef void (*uart_irq_config_func_t)(struct device *port);

/**
 * @typedef uart_dma_recv_timeout_callback_t
 * @brief For configuring dam recv timeout callback on each individual UART device.
 *
 * @internal
 */
typedef void (*uart_dma_recv_timeout_callback_t)(void *priv_data, uint8_t *data, uint32_t data_len);


/**
 * @brief UART device configuration.
 *
 * @param port Base port number
 * @param base Memory mapped base address
 * @param regs Register address
 * @param sys_clk_freq System clock frequency in Hz
 */
struct uart_device_config {
	union {
		u32_t port;
		u8_t *base;
		u32_t regs;
	};

	u32_t sys_clk_freq;

#ifdef CONFIG_PCI
	struct pci_dev_info  pci_dev;
#endif /* CONFIG_PCI */

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	uart_irq_config_func_t	irq_config_func;
#endif
};

struct acts_uart_config {
	u32_t base;
	u8_t clock_id;
	u8_t reset_id;
	u8_t dma_id;
	u8_t reserved[1];

	u32_t baud_rate;

#ifdef CONFIG_STUB_DEV_UART
    u32_t att_baud_rate;
#endif

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	uart_irq_config_func_t	irq_config_func;
#endif
};


/* Device data structure */
struct acts_uart_data {
	/* Baud rate */
	u32_t baud_rate;

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
	/* IRQ Callback function pointer */
	uart_irq_callback_t	cb;
#endif

#ifdef CONFIG_UART_DMA_DRIVEN
    struct device *dma_dev;

#ifdef CONFIG_UART_DMA_TX_DRIVEN
    u32_t send_chan;
#endif

#ifdef CONFIG_UART_DMA_RX_DRIVEN
    u32_t rev_chan;

#ifdef CONFIG_UART_DMA_RX_TIMEOUT_DRIVEN
    uint32_t last_dma_transfer_timestamp;
    uint32_t last_dma_remain_len;
	uint32_t timeout_us;
	uint32_t dma_buf_size;
	uint8_t *dma_buf_ptr;
    uint32_t phy_dma_handle;
    struct hrtimer check_dma_timer;
	void *priv_data;
	uart_dma_recv_timeout_callback_t timeout_cb;
	dma_stream_handle_t dma_stream_cb;
	void *dma_stream_priv_data;
#endif
#endif
#endif
};

/* convenience defines */
#define DEV_CFG(dev) \
	((const struct acts_uart_config * const)(dev)->config->config_info)
#define DEV_DATA(dev) \
	((struct acts_uart_data * const)(dev)->driver_data)
#define UART_STRUCT(dev) \
	((struct acts_uart_controller *)(DEV_CFG(dev))->base)

/** @brief Driver API structure. */
struct uart_driver_api {
	/** Console I/O function */
	int (*poll_in)(struct device *dev, unsigned char *p_char);
	unsigned char (*poll_out)(struct device *dev, unsigned char out_char);

	/** Console I/O function */
	int (*err_check)(struct device *dev);

#ifdef CONFIG_UART_INTERRUPT_DRIVEN

	/** Interrupt driven FIFO fill function */
	int (*fifo_fill)(struct device *dev, const u8_t *tx_data, int len);

	/** Interrupt driven FIFO read function */
	int (*fifo_read)(struct device *dev, u8_t *rx_data, const int size);

	/** Interrupt driven transfer enabling function */
	void (*irq_tx_enable)(struct device *dev);

	/** Interrupt driven transfer disabling function */
	void (*irq_tx_disable)(struct device *dev);

	/** Interrupt driven transfer ready function */
	int (*irq_tx_ready)(struct device *dev);

	/** Interrupt driven receiver enabling function */
	void (*irq_rx_enable)(struct device *dev);

	/** Interrupt driven receiver disabling function */
	void (*irq_rx_disable)(struct device *dev);

	/** Interrupt driven transfer complete function */
	int (*irq_tx_complete)(struct device *dev);

	/** Interrupt driven receiver ready function */
	int (*irq_rx_ready)(struct device *dev);

	/** Interrupt driven error enabling function */
	void (*irq_err_enable)(struct device *dev);

	/** Interrupt driven error disabling function */
	void (*irq_err_disable)(struct device *dev);

	/** Interrupt driven pending status function */
	int (*irq_is_pending)(struct device *dev);

	/** Interrupt driven interrupt update function */
	int (*irq_update)(struct device *dev);

	/** Set the callback function */
	void (*irq_callback_set)(struct device *dev, uart_irq_callback_t cb);

#endif

#ifdef CONFIG_UART_LINE_CTRL
	int (*line_ctrl_set)(struct device *dev, u32_t ctrl, u32_t val);
	int (*line_ctrl_get)(struct device *dev, u32_t ctrl, u32_t *val);
#endif

#ifdef CONFIG_UART_DRV_CMD
	int (*drv_cmd)(struct device *dev, u32_t cmd, u32_t p);
#endif

    int (*fifo_switch)(struct device *dev, u32_t is_tx, u32_t fifo_type);

#ifdef CONFIG_UART_DMA_DRIVEN

#ifdef CONFIG_UART_DMA_TX_DRIVEN
    int (*dma_send_init)(struct device *dev, int channel, dma_stream_handle_t stream_handler, void *stream_data);
    int (*dma_send)(struct device *dev, char *s, int len);
    int (*dma_send_complete)(struct device *dev);
    int (*dma_send_stop)(struct device *dev);
#endif

#ifdef CONFIG_UART_DMA_RX_DRIVEN
    int (*dma_recv_init)(struct device *dev, int channel, dma_stream_handle_t stream_handler, void *stream_data);
    int (*dma_recv_config)(struct device *dev, u8_t *dst, u32_t len);
    int (*dma_recv_start)(struct device *dev);
    int (*dma_recv_remain_len)(struct device *dev);
    int (*dma_recv_stop)(struct device *dev);
#ifdef CONFIG_UART_DMA_RX_TIMEOUT_DRIVEN
	int (*dma_recv_set_timeout_start)(struct device *dev, int timeout_us, uart_dma_recv_timeout_callback_t callback, void *priv_data);
	int (*dma_recv_set_timeout_stop)(struct device *dev);
#endif
#endif
#endif

#ifdef CONFIG_STUB_DEV_UART
    int (*baud_rate_switch)(struct device *dev, int mode);
#endif
	int (*baud_rate_set)(struct device *dev, u32_t bps);
};

/**
 * @brief Check whether an error was detected.
 *
 * @param dev UART device structure.
 *
 * @retval UART_ERROR_OVERRUN if an overrun error was detected.
 * @retval UART_ERROR_PARITY if a parity error was detected.
 * @retval UART_ERROR_FRAMING if a framing error was detected.
 * @retval UART_ERROR_BREAK if a break error was detected.
 * @retval 0 Otherwise.
 */
static inline int uart_err_check(struct device *dev)
{
	const struct uart_driver_api *api = dev->driver_api;

	if (api->err_check) {
		return api->err_check(dev);
	}
	return 0;
}


/**
 * @brief Poll the device for input.
 *
 * @param dev UART device structure.
 * @param p_char Pointer to character.
 *
 * @retval 0 If a character arrived.
 * @retval -1 If no character was available to read (i.e., the UART
 *            input buffer was empty).
 * @retval -ENOTSUP If the operation is not supported.
 */
static inline int uart_poll_in(struct device *dev, unsigned char *p_char)
{
	const struct uart_driver_api *api = dev->driver_api;

	return api->poll_in(dev, p_char);
}

/**
 * @brief Output a character in polled mode.
 *
 * This routine checks if the transmitter is empty.
 * When the transmitter is empty, it writes a character to the data
 * register.
 *
 * To send a character when hardware flow control is enabled, the handshake
 * signal CTS must be asserted.
 *
 * @param dev UART device structure.
 * @param out_char Character to send.
 *
 * @retval char Sent character.
 */
static inline unsigned char uart_poll_out(struct device *dev,
					  unsigned char out_char)
{
	const struct uart_driver_api *api = dev->driver_api;

	return api->poll_out(dev, out_char);
}


#ifdef CONFIG_UART_INTERRUPT_DRIVEN

/**
 * @brief Fill FIFO with data.
 *
 * @details This function is expected to be called from UART
 * interrupt handler (ISR), if uart_irq_tx_ready() returns true.
 * Result of calling this function not from an ISR is undefined
 * (hardware-dependent). Likewise, *not* calling this function
 * from an ISR if uart_irq_tx_ready() returns true may lead to
 * undefined behavior, e.g. infinite interrupt loops. It's
 * mandatory to test return value of this function, as different
 * hardware has different FIFO depth (oftentimes just 1).
 *
 * @param dev UART device structure.
 * @param tx_data Data to transmit.
 * @param size Number of bytes to send.
 *
 * @return Number of bytes sent.
 */
static inline int uart_fifo_fill(struct device *dev, const u8_t *tx_data,
				 int size)
{
	const struct uart_driver_api *api = dev->driver_api;

	if (api->fifo_fill) {
		return api->fifo_fill(dev, tx_data, size);
	}

	return 0;
}

/**
 * @brief Read data from FIFO.
 *
 * @details This function is expected to be called from UART
 * interrupt handler (ISR), if uart_irq_rx_ready() returns true.
 * Result of calling this function not from an ISR is undefined
 * (hardware-dependent). It's unspecified whether "RX ready"
 * condition as returned by uart_irq_rx_ready() is level- or
 * edge- triggered. That means that once uart_irq_rx_ready() is
 * detected, uart_fifo_read() must be called until it reads all
 * available data in the FIFO (i.e. until it returns less data
 * than was requested).
 *
 * @param dev UART device structure.
 * @param rx_data Data container.
 * @param size Container size.
 *
 * @return Number of bytes read.
 */
static inline int uart_fifo_read(struct device *dev, u8_t *rx_data,
				 const int size)
{
	const struct uart_driver_api *api = dev->driver_api;

	if (api->fifo_read) {
		return api->fifo_read(dev, rx_data, size);
	}

	return 0;
}

/**
 * @brief Enable TX interrupt in IER.
 *
 * @param dev UART device structure.
 *
 * @return N/A
 */
static inline void uart_irq_tx_enable(struct device *dev)
{
	const struct uart_driver_api *api = dev->driver_api;

	if (api->irq_tx_enable) {
		api->irq_tx_enable(dev);
	}
}
/**
 * @brief Disable TX interrupt in IER.
 *
 * @param dev UART device structure.
 *
 * @return N/A
 */
static inline void uart_irq_tx_disable(struct device *dev)
{
	const struct uart_driver_api *api = dev->driver_api;

	if (api->irq_tx_disable) {
		api->irq_tx_disable(dev);
	}
}

/**
 * @brief Check if UART TX buffer can accept a new char
 *
 * @details Check if UART TX buffer can accept at least one character
 * for transmission (i.e. uart_fifo_fill() will succeed and return
 * non-zero). This function must be called in a UART interrupt
 * handler, or its result is undefined. Before calling this function
 * in the interrupt handler, uart_irq_update() must be called once per
 * the handler invocation.
 *
 * @param dev UART device structure.
 *
 * @retval 1 If at least one char can be written to UART.
 * @retval 0 Otherwise.
 */
static inline int uart_irq_tx_ready(struct device *dev)
{
	const struct uart_driver_api *api = dev->driver_api;

	if (api->irq_tx_ready) {
		return api->irq_tx_ready(dev);
	}

	return 0;
}

/**
 * @brief Enable RX interrupt in IER.
 *
 * @param dev UART device structure.
 *
 * @return N/A
 */
static inline void uart_irq_rx_enable(struct device *dev)
{
	const struct uart_driver_api *api = dev->driver_api;

	if (api->irq_rx_enable) {
		api->irq_rx_enable(dev);
	}
}

/**
 * @brief Disable RX interrupt in IER.
 *
 * @param dev UART device structure.
 *
 * @return N/A
 */
static inline void uart_irq_rx_disable(struct device *dev)
{
	const struct uart_driver_api *api = dev->driver_api;

	if (api->irq_rx_disable) {
		api->irq_rx_disable(dev);
	}
}

/**
 * @brief Check if UART TX block finished transmission
 *
 * @details Check if any outgoing data buffered in UART TX block was
 * fully transmitted and TX block is idle. When this condition is
 * true, UART device (or whole system) can be power off. Note that
 * this function is *not* useful to check if UART TX can accept more
 * data, use uart_irq_tx_ready() for that. This function must be called
 * in a UART interrupt handler, or its result is undefined. Before
 * calling this function in the interrupt handler, uart_irq_update()
 * must be called once per the handler invocation.
 *
 * @param dev UART device structure.
 *
 * @retval 1 If nothing remains to be transmitted.
 * @retval 0 Otherwise.
 */
static inline int uart_irq_tx_complete(struct device *dev)
{
	const struct uart_driver_api *api = dev->driver_api;

	if (api->irq_tx_complete) {
		return api->irq_tx_complete(dev);
	}

	return 0;
}

/**
 * @deprecated This API is deprecated.
 */
static inline int __deprecated uart_irq_tx_empty(struct device *dev)
{
	return uart_irq_tx_complete(dev);
}

/**
 * @brief Check if UART RX buffer has a received char
 *
 * @details Check if UART RX buffer has at least one pending character
 * (i.e. uart_fifo_read() will succeed and return non-zero). This function
 * must be called in a UART interrupt handler, or its result is undefined.
 * Before calling this function in the interrupt handler, uart_irq_update()
 * must be called once per the handler invocation. It's unspecified whether
 * condition as returned by this function is level- or edge- triggered (i.e.
 * if this function returns true when RX FIFO is non-empty, or when a new
 * char was received since last call to it). See description of
 * uart_fifo_read() for implication of this.
 *
 * @param dev UART device structure.
 *
 * @retval 1 If a received char is ready.
 * @retval 0 Otherwise.
 */
static inline int uart_irq_rx_ready(struct device *dev)
{
	const struct uart_driver_api *api = dev->driver_api;

	if (api->irq_rx_ready) {
		return api->irq_rx_ready(dev);
	}

	return 0;
}
/**
 * @brief Enable error interrupt in IER.
 *
 * @param dev UART device structure.
 *
 * @return N/A
 */
static inline void uart_irq_err_enable(struct device *dev)
{
	const struct uart_driver_api *api = dev->driver_api;

	if (api->irq_err_enable) {
		api->irq_err_enable(dev);
	}
}

/**
 * @brief Disable error interrupt in IER.
 *
 * @param dev UART device structure.
 *
 * @retval 1 If an IRQ is ready.
 * @retval 0 Otherwise.
 */
static inline void uart_irq_err_disable(struct device *dev)
{
	const struct uart_driver_api *api = dev->driver_api;

	if (api->irq_err_disable) {
		api->irq_err_disable(dev);
	}
}

/**
 * @brief Check if any IRQs is pending.
 *
 * @param dev UART device structure.
 *
 * @retval 1 If an IRQ is pending.
 * @retval 0 Otherwise.
 */

static inline int uart_irq_is_pending(struct device *dev)
{
	const struct uart_driver_api *api = dev->driver_api;

	if (api->irq_is_pending)	{
		return api->irq_is_pending(dev);
	}

	return 0;
}

/**
 * @brief Update cached contents of IIR.
 *
 * @param dev UART device structure.
 *
 * @retval 1 Always.
 */
static inline int uart_irq_update(struct device *dev)
{
	const struct uart_driver_api *api = dev->driver_api;

	if (api->irq_update) {
		return api->irq_update(dev);
	}

	return 0;
}

/**
 * @brief Set the IRQ callback function pointer.
 *
 * This sets up the callback for IRQ. When an IRQ is triggered,
 * the specified function will be called.
 *
 * @param dev UART device structure.
 * @param cb Pointer to the callback function.
 *
 * @return N/A
 */
static inline void uart_irq_callback_set(struct device *dev,
					 uart_irq_callback_t cb)
{
	const struct uart_driver_api *api = dev->driver_api;

	if ((api != NULL) && (api->irq_callback_set != NULL)) {
		api->irq_callback_set(dev, cb);
	}
}

#endif

#ifdef CONFIG_UART_LINE_CTRL

/**
 * @brief Manipulate line control for UART.
 *
 * @param dev UART device structure.
 * @param ctrl The line control to manipulate.
 * @param val Value to set to the line control.
 *
 * @retval 0 If successful.
 * @retval failed Otherwise.
 */
static inline int uart_line_ctrl_set(struct device *dev,
				     u32_t ctrl, u32_t val)
{
	const struct uart_driver_api *api = dev->driver_api;

	if (api->line_ctrl_set) {
		return api->line_ctrl_set(dev, ctrl, val);
	}

	return -ENOTSUP;
}

/**
 * @brief Retrieve line control for UART.
 *
 * @param dev UART device structure.
 * @param ctrl The line control to manipulate.
 * @param val Value to get for the line control.
 *
 * @retval 0 If successful.
 * @retval failed Otherwise.
 */
static inline int uart_line_ctrl_get(struct device *dev,
				     u32_t ctrl, u32_t *val)
{
	const struct uart_driver_api *api = dev->driver_api;

	if (api && api->line_ctrl_get) {
		return api->line_ctrl_get(dev, ctrl, val);
	}

	return -ENOTSUP;
}

#endif /* CONFIG_UART_LINE_CTRL */

#ifdef CONFIG_UART_DRV_CMD

/**
 * @brief Send extra command to driver.
 *
 * Implementation and accepted commands are driver specific.
 * Refer to the drivers for more information.
 *
 * @param dev UART device structure.
 * @param cmd Command to driver.
 * @param p Parameter to the command.
 *
 * @retval 0 If successful.
 * @retval failed Otherwise.
 */
static inline int uart_drv_cmd(struct device *dev, u32_t cmd, u32_t p)
{
	const struct uart_driver_api *api = dev->driver_api;

	if (api->drv_cmd) {
		return api->drv_cmd(dev, cmd, p);
	}

	return -ENOTSUP;
}

#endif /* CONFIG_UART_DRV_CMD */

/**
 * @brief switch used fifo for uart driver.
 *
 * Implementation and accepted commands are driver specific.
 * Refer to the drivers for more information.
 *
 * @param dev UART device structure.
 * @param is_tx switch tx fifo or not
 * @param fifo_type uart fifo type
 *
 * @retval 0 If successful.
 * @retval failed Otherwise.
 */
static inline int uart_fifo_switch(struct device *dev, u32_t is_tx, u32_t fifo_type)
{
	const struct uart_driver_api *api = dev->driver_api;

	if (api->fifo_switch) {
		return api->fifo_switch(dev, is_tx, fifo_type);
	}

	return -ENOTSUP;
}

#ifdef CONFIG_UART_DMA_DRIVEN

#ifdef CONFIG_UART_DMA_TX_DRIVEN
static inline int uart_dma_send_init(struct device *dev, int channel, dma_stream_handle_t stream_handler, void *stream_data)
{
	const struct uart_driver_api *api = dev->driver_api;

	if (api->dma_send_init) {
		return api->dma_send_init(dev, channel, stream_handler, stream_data);
	}

	return -ENOTSUP;
}

static inline int uart_dma_send(struct device *dev, char *s, int len)
{
	const struct uart_driver_api *api = dev->driver_api;

	if (api->dma_send) {
		return api->dma_send(dev, s, len);
	}

	return -ENOTSUP;
}

static inline int uart_dma_send_complete(struct device *dev)
{
	const struct uart_driver_api *api = dev->driver_api;

	if (api->dma_send_complete) {
		return api->dma_send_complete(dev);
	}

	return -ENOTSUP;
}


static inline int uart_dma_send_stop(struct device *dev)
{
	const struct uart_driver_api *api = dev->driver_api;

	if (api->dma_send_stop) {
		return api->dma_send_stop(dev);
	}

	return -ENOTSUP;
}

#endif


#ifdef CONFIG_UART_DMA_RX_DRIVEN

static inline int uart_dma_recv_init(struct device *dev, int channel, dma_stream_handle_t stream_handler, void *stream_data)
{
    const struct uart_driver_api *api = dev->driver_api;

    if (api->dma_recv_init) {
        return api->dma_recv_init(dev, channel, stream_handler, stream_data);
    }

    return -ENOTSUP;
}

static inline int uart_dma_recv_config(struct device *dev, u8_t *dst, u32_t len)
{
    const struct uart_driver_api *api = dev->driver_api;

    if (api->dma_recv_config) {
        return api->dma_recv_config(dev, dst, len);
    }

    return -ENOTSUP;
}

static inline int uart_dma_recv_start(struct device *dev)
{
    const struct uart_driver_api *api = dev->driver_api;

    if (api->dma_recv_start) {
        return api->dma_recv_start(dev);
    }

    return -ENOTSUP;
}
static inline int uart_dma_recv_remain_len(struct device *dev)
{
    const struct uart_driver_api *api = dev->driver_api;

    if (api->dma_recv_remain_len) {
        return api->dma_recv_remain_len(dev);
    }

    return -ENOTSUP;
}

static inline int uart_dma_recv_stop(struct device *dev)
{
    const struct uart_driver_api *api = dev->driver_api;

    if (api->dma_recv_stop) {
        return api->dma_recv_stop(dev);
    }

    return -ENOTSUP;
}

#ifdef CONFIG_UART_DMA_RX_TIMEOUT_DRIVEN
static inline int uart_dma_recv_set_timeout_start(struct device *dev, int timeout_us, uart_dma_recv_timeout_callback_t callback, void *priv_data)
{
    const struct uart_driver_api *api = dev->driver_api;

    if (api->dma_recv_set_timeout_start) {
        return api->dma_recv_set_timeout_start(dev, timeout_us, callback, priv_data);
    }

    return -ENOTSUP;
}

static inline int uart_dma_recv_set_timeout_stop(struct device *dev)
{
    const struct uart_driver_api *api = dev->driver_api;

    if (api->dma_recv_set_timeout_stop) {
        return api->dma_recv_set_timeout_stop(dev);
    }

    return -ENOTSUP;
}
#endif

#endif
#endif


//struct acts_uart_data * dev_data = DEV_DATA(dev);
#ifdef CONFIG_STUB_DEV_UART

static inline int uart_baud_rate_switch(struct device *dev, int mode)
{
    const struct uart_driver_api *api = dev->driver_api;

    if (api->baud_rate_switch) {
        return api->baud_rate_switch(dev, mode);
    }

    return -ENOTSUP;
}
#endif

static inline int uart_baud_rate_set(struct device *dev, u32_t bps)
{
	const struct uart_driver_api *api = dev->driver_api;

	if (api->baud_rate_set) {
		return api->baud_rate_set(dev, bps);
	}

	return -ENOTSUP;
}

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* __INCuarth */
