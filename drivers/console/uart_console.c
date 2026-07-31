/*
 * Copyright (c) 2011-2012, 2014-2015 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief UART-driven console
 *
 *
 * Serial console driver.
 * Hooks into the printk and fputc (for printf) modules. Poll driven.
 */

#include <kernel.h>
#include <arch/cpu.h>

#include <stdio.h>
#include <zephyr/types.h>
#include <errno.h>
#include <ctype.h>

#include <device.h>
#include <init.h>
#include <soc.h>
#include <uart.h>
#include <console/console.h>
#include <console/uart_console.h>
#include <toolchain.h>
#include <linker/sections.h>
#include <atomic.h>
#include <misc/printk.h>
#include <misc/sysrq.h>
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_UART_CONSOLE
#ifdef CONFIG_USB_UART_CONSOLE
#include <uart.h>
#endif
#include <string.h>

static struct device *uart_console_dev;

#ifdef CONFIG_USB_UART_CONSOLE
static u8_t console_use_cdc_device;
extern int usb_cdc_acm_init(struct device *dev);
extern int usb_cdc_acm_exit(void);
#endif

#ifdef CONFIG_UART_CONSOLE_DEBUG_SERVER_HOOKS

static uart_console_in_debug_hook_t debug_hook_in;
void uart_console_in_debug_hook_install(uart_console_in_debug_hook_t hook)
{
	debug_hook_in = hook;
}

static UART_CONSOLE_OUT_DEBUG_HOOK_SIG(debug_hook_out_nop)
{
	ARG_UNUSED(c);
	return !UART_CONSOLE_DEBUG_HOOK_HANDLED;
}

static uart_console_out_debug_hook_t *debug_hook_out = debug_hook_out_nop;
void uart_console_out_debug_hook_install(uart_console_out_debug_hook_t *hook)
{
	debug_hook_out = hook;
}
#define HANDLE_DEBUG_HOOK_OUT(c) \
	(debug_hook_out(c) == UART_CONSOLE_DEBUG_HOOK_HANDLED)

#endif /* CONFIG_UART_CONSOLE_DEBUG_SERVER_HOOKS */

#if 0 /* NOTUSED */
/**
 *
 * @brief Get a character from UART
 *
 * @return the character or EOF if nothing present
 */

static int console_in(void)
{
	unsigned char c;

	if (uart_poll_in(uart_console_dev, &c) < 0) {
		return EOF;
	} else {
		return (int)c;
	}
}
#endif

#if defined(CONFIG_PRINTK) || defined(CONFIG_STDOUT_CONSOLE)
/**
 *
 * @brief Output one character to UART
 *
 * Outputs both line feed and carriage return in the case of a '\n'.
 *
 * @param c Character to output
 *
 * @return The character passed as input.
 */

static int console_out(int c)
{
#ifdef CONFIG_UART_CONSOLE_DEBUG_SERVER_HOOKS

	int handled_by_debug_server = HANDLE_DEBUG_HOOK_OUT(c);

	if (handled_by_debug_server) {
		return c;
	}

#endif /* CONFIG_UART_CONSOLE_DEBUG_SERVER_HOOKS */

	if ('\n' == c) {
		uart_poll_out(uart_console_dev, '\r');
	}
	uart_poll_out(uart_console_dev, c);

	return c;
}

#endif

#if defined(CONFIG_STDOUT_CONSOLE)
extern void __stdout_hook_install(int (*hook)(int));
#else
#define __stdout_hook_install(x)		\
	do {/* nothing */			\
	} while ((0))
#endif

#if defined(CONFIG_PRINTK)
extern void __printk_hook_install(int (*fn)(int));
#else
#define __printk_hook_install(x)		\
	do {/* nothing */			\
	} while ((0))
#endif

#if defined(CONFIG_CONSOLE_HANDLER)
static struct k_fifo *avail_queue;
static struct k_fifo *lines_queue;
static u8_t (*completion_cb)(char *line, u8_t len);
static void (*line_end_cb)(void);

/* Control characters */
#define ESC                0x1b
#define DEL                0x7f
#define BACKTRACE          0x8

/* ANSI escape sequences */
#define ANSI_ESC           '['
#define ANSI_UP            'A'
#define ANSI_DOWN          'B'
#define ANSI_FORWARD       'C'
#define ANSI_BACKWARD      'D'
#define ANSI_END           'F'
#define ANSI_HOME          'H'
#define ANSI_DEL           '~'

static int read_uart(struct device *uart, u8_t *buf, unsigned int size)
{
	int rx;

	rx = uart_fifo_read(uart, buf, size);
	if (rx < 0) {
		/* Overrun issue. Stop the UART */
		uart_irq_rx_disable(uart);

		return -EIO;
	}

	return rx;
}

static inline void cursor_forward(unsigned int count)
{
	ACT_LOG_ID_INF(ALF_STR_cursor_forward__X1BUC, 1, count);
}

static inline void cursor_backward(unsigned int count)
{
	ACT_LOG_ID_INF(ALF_STR_cursor_backward__X1BUD, 1, count);
}

static inline void cursor_save(void)
{
	ACT_LOG_ID_INF(ALF_STR_cursor_save__X1BS, 0);
}

static inline void cursor_restore(void)
{
	ACT_LOG_ID_INF(ALF_STR_cursor_restore__X1BU, 0);
}

static void uart_put_char(char c)
{
    uart_console_output(&c, 1);
}

static void insert_char(char *pos, char c, u8_t end)
{
	char tmp;

	/* Echo back to console */
	uart_put_char(c);

	if (end == 0) {
		*pos = c;
		return;
	}

	tmp = *pos;
	*(pos++) = c;

	cursor_save();

	while (end-- > 0) {
		uart_put_char(tmp);
		c = *pos;
		*(pos++) = tmp;
		tmp = c;
	}

	/* Move cursor back to right place */
	cursor_restore();
}

static void del_char(char *pos, u8_t end)
{
	uart_put_char('\b');

	if (end == 0) {
		uart_put_char(' ');
		uart_put_char('\b');
		return;
	}

	cursor_save();

	while (end-- > 0) {
		*pos = *(pos + 1);
		uart_put_char(*(pos++));
	}

	uart_put_char(' ');

	/* Move cursor back to right place */
	cursor_restore();
}

enum {
	ESC_ESC,
	ESC_ANSI,
	ESC_ANSI_FIRST,
	ESC_ANSI_VAL,
	ESC_ANSI_VAL_2
};

static atomic_t esc_state;
static unsigned int ansi_val, ansi_val_2;
static u8_t cur, end;

static void handle_ansi(u8_t byte, char *line)
{
	if (atomic_test_and_clear_bit(&esc_state, ESC_ANSI_FIRST)) {
		if (!isdigit(byte)) {
			ansi_val = 1;
			goto ansi_cmd;
		}

		atomic_set_bit(&esc_state, ESC_ANSI_VAL);
		ansi_val = byte - '0';
		ansi_val_2 = 0;
		return;
	}

	if (atomic_test_bit(&esc_state, ESC_ANSI_VAL)) {
		if (isdigit(byte)) {
			if (atomic_test_bit(&esc_state, ESC_ANSI_VAL_2)) {
				ansi_val_2 *= 10;
				ansi_val_2 += byte - '0';
			} else {
				ansi_val *= 10;
				ansi_val += byte - '0';
			}
			return;
		}

		/* Multi value sequence, e.g. Esc[Line;ColumnH */
		if (byte == ';' &&
		    !atomic_test_and_set_bit(&esc_state, ESC_ANSI_VAL_2)) {
			return;
		}

		atomic_clear_bit(&esc_state, ESC_ANSI_VAL);
		atomic_clear_bit(&esc_state, ESC_ANSI_VAL_2);
	}

ansi_cmd:
	switch (byte) {
	case ANSI_BACKWARD:
		if (ansi_val > cur) {
			break;
		}

		end += ansi_val;
		cur -= ansi_val;
		cursor_backward(ansi_val);
		break;
	case ANSI_FORWARD:
		if (ansi_val > end) {
			break;
		}

		end -= ansi_val;
		cur += ansi_val;
		cursor_forward(ansi_val);
		break;
	case ANSI_HOME:
		if (!cur) {
			break;
		}

		cursor_backward(cur);
		end += cur;
		cur = 0;
		break;
	case ANSI_END:
		if (!end) {
			break;
		}

		cursor_forward(end);
		cur += end;
		end = 0;
		break;
	case ANSI_DEL:
		if (!end) {
			break;
		}

		cursor_forward(1);
		del_char(&line[cur], --end);
		break;
	default:
		break;
	}

	atomic_clear_bit(&esc_state, ESC_ANSI);
}

void uart_console_isr(struct device *unused)
{
	ARG_UNUSED(unused);

	while (uart_irq_update(uart_console_dev) &&
	       uart_irq_is_pending(uart_console_dev)) {
		static struct console_input *cmd;
		u8_t byte;
		int rx;

#ifdef CONFIG_USB_UART_CONSOLE
		//for usb uart console maybe block by rx_ready
		if (!console_use_cdc_device){
			if (!uart_irq_rx_ready(uart_console_dev)) {
				continue;
			}
		}
#else
		if (!uart_irq_rx_ready(uart_console_dev)) {
			continue;
		}
#endif
		/* Character(s) have been received */

		rx = read_uart(uart_console_dev, &byte, 1);
		if (rx < 0) {
			return;
		}

#ifdef CONFIG_UART_CONSOLE_DEBUG_SERVER_HOOKS
		if (debug_hook_in != NULL && debug_hook_in(byte) != 0) {
			/*
			 * The input hook indicates that no further processing
			 * should be done by this handler.
			 */
			return;
		}
#endif

		/* Check SYSRQ char */
		uart_handle_sysrq_char(uart_console_dev, byte);

		if (!cmd) {
			cmd = k_fifo_get(avail_queue, K_NO_WAIT);
			if (!cmd) {
				return;
			}
			memset(cmd->line, 0, CONSOLE_MAX_LINE_LEN);
		}

		/* Handle ANSI escape mode */
		if (atomic_test_bit(&esc_state, ESC_ANSI)) {
			handle_ansi(byte, cmd->line);
			continue;
		}

		/* Handle escape mode */
		if (atomic_test_and_clear_bit(&esc_state, ESC_ESC)) {
			if (byte == ANSI_ESC) {
				atomic_set_bit(&esc_state, ESC_ANSI);
				atomic_set_bit(&esc_state, ESC_ANSI_FIRST);
			}

			continue;
		}

		/* Handle special control characters */
		if (!isprint(byte)) {
			switch (byte) {
			case BACKTRACE:
			case DEL:
				if (cur > 0) {
					del_char(&cmd->line[--cur], end);
				}
				break;
			case ESC:
				atomic_set_bit(&esc_state, ESC_ESC);
				break;
			case '\r':
				cmd->line[cur + end] = '\0';
				uart_put_char('\r');
				uart_put_char('\n');
				cur = 0;
				end = 0;
				k_fifo_put(lines_queue, cmd);
				if(line_end_cb){
					line_end_cb();
				}
				cmd = NULL;
				break;
			case '\t':
				if (completion_cb && !end) {
					cur += completion_cb(cmd->line, cur);
				}
				break;
			default:
				break;
			}

			continue;
		}

		/* Ignore characters if there's no more buffer space */
		if (cur + end < sizeof(cmd->line) - 1) {
			insert_char(&cmd->line[cur++], byte, end);
		}
	}
}

int uart_console_send_cmd(uint8_t *data)
{
	uint32_t data_len = strlen(data);
	struct console_input *cmd;

	if(data_len > CONSOLE_MAX_LINE_LEN){
		printk("err: cmd %s too long %d\n", data, data_len);
		return -EINVAL;
	}

	cmd = k_fifo_get(avail_queue, K_NO_WAIT);
	if (!cmd) {
		printk("err no cmd\n");
		return -ENOMEM;
	}

	memset(cmd->line, 0, CONSOLE_MAX_LINE_LEN);

	memcpy(cmd->line, data, data_len);

	k_fifo_put(lines_queue, cmd);
	if(line_end_cb){
		line_end_cb();
	}

	return 0;
}

void console_input_init(struct device *dev)
{
	uart_input_init(dev, uart_console_isr);
}

void console_input_deinit(struct device *dev)
{
	uart_input_deinit(dev);
}


void uart_register_input(struct k_fifo *avail, struct k_fifo *lines,
			 u8_t (*completion)(char *str, u8_t len), void (*line_end)(void))
{
	avail_queue = avail;
	lines_queue = lines;
	completion_cb = completion;
	line_end_cb = line_end;

	console_input_init(uart_console_dev);
}

void uart_console_shell_restore(void)
{
	if (!uart_console_dev) {
		return;
	}

#if defined(CONFIG_UART_DMA_DRIVEN)
	/* trace_init() may leave TX on DMA; shell RX must stay on CPU FIFO */
	(void)uart_fifo_switch(uart_console_dev, 0, UART_FIFO_CPU);
#endif

	console_input_init(uart_console_dev);
}

#else
#define console_input_init(x)			\
	do {/* nothing */			\
	} while ((0))
#define uart_register_input(x)			\
	do {/* nothing */			\
	} while ((0))
#define uart_console_shell_restore()		\
	do {/* nothing */			\
	} while ((0))
#endif

void uart_input_init(struct device *dev, uart_irq_callback_t cb)
{
	u8_t c;

	if (!dev || !cb) {
		return;
	}

	uart_irq_rx_disable(dev);
	uart_irq_tx_disable(dev);
	uart_irq_callback_set(dev, cb);

	while (uart_irq_rx_ready(dev)) {
		uart_fifo_read(dev, &c, 1);
	}

	uart_irq_rx_enable(dev);
}

void uart_input_deinit(struct device *dev)
{
	u8_t c;

	if (!dev) {
		return;
	}

	uart_irq_rx_disable(dev);
	uart_irq_tx_disable(dev);
	uart_irq_callback_set(dev, NULL);

	while (uart_irq_rx_ready(dev)) {
		uart_fifo_read(dev, &c, 1);
	}
}

/**
 *
 * @brief Install printk/stdout hook for UART console output
 *
 * @return N/A
 */

void uart_console_hook_install(void)
{
	__stdout_hook_install(console_out);
	__printk_hook_install(console_out);
}

static int __dummy_console_out(int c)
{
    return c;
}

void uart_console_dummy_hook_install(void)
{
	__stdout_hook_install(__dummy_console_out);
	__printk_hook_install(__dummy_console_out);
}

#ifdef CONFIG_UART_CONSOLE_DEFAULT_DISABLE
int uart_console_force_init(struct device *arg)
{
	char temp[5];
	bool force_enable_uart = false;

	memset(temp, 0, sizeof(temp));

	if(nvram_config_get("FORCE_ENABLE_UART_CONSOLE", temp, sizeof(temp)) > 0)
	{
		if(!strcmp(temp,"true"))
		{
			force_enable_uart = true;
		}
	}

	if(force_enable_uart)
	{
		uart_console_hook_install();
	}
}
#endif

/**
 *
 * @brief Initialize one UART as the console/debug port
 *
 * @return 0 if successful, otherwise failed.
 */
static int uart_console_init(struct device *arg)
{

	ARG_UNUSED(arg);

	uart_console_dev = device_get_binding(CONFIG_UART_CONSOLE_ON_DEV_NAME);

#ifdef CONFIG_UART_CONSOLE_DEFAULT_DISABLE
	uart_console_dummy_hook_install();
#else
	uart_console_hook_install();
#endif

	return 0;
}

#ifdef CONFIG_USB_UART_CONSOLE
struct device *uart_console_reinit(u32_t is_cdc_device)
{
	struct device *console_dev;

	u32_t baudrate;
	int ret = 0;
    //u32_t dtr = 0;

    //printk("is_cdc_device:0x%x\n", is_cdc_device);
	if(is_cdc_device){
		console_dev = device_get_binding(CONFIG_CDC_ACM_PORT_NAME);
		if (!console_dev) {
			printk("CDC ACM device not found\n");
			return NULL;
		}

	    ret = usb_cdc_acm_init(console_dev);
		if (ret) {
			printk("failed to init CDC ACM device\n");
			return NULL;
		}

		/*printk("Wait for DTR\n");
		while (1) {
			uart_line_ctrl_get(dev, LINE_CTRL_DTR, &dtr);
			if (dtr)
				break;
		}
		printk("dtr set, start test\n");*/

        /* Wait 1 sec for the host to do all settings */
		k_sleep(K_SECONDS(1));

		/* They are optional, we use them to test the interrupt endpoint */
		ret = uart_line_ctrl_set(console_dev, LINE_CTRL_DCD, 1);
		if (ret){
			printk("Failed to set DCD, ret code %d\n", ret);
            goto err_exit;
		}

		ret = uart_line_ctrl_set(console_dev, LINE_CTRL_DSR, 1);
		if (ret){
			printk("Failed to set DSR, ret code %d\n", ret);
            goto err_exit;
		}

		/* Wait 1 sec for the host to do all settings */
		k_sleep(K_SECONDS(1));

		ret = uart_line_ctrl_get(console_dev, LINE_CTRL_BAUD_RATE, &baudrate);
		if (ret){
			printk("Failed to get baudrate, ret code %d\n", ret);
            goto err_exit;
        }
		else{
			printk("Baudrate detected: %d\n", baudrate);
        }

		console_use_cdc_device = true;

	}else{
        if (console_use_cdc_device){
            console_use_cdc_device = false;
            usb_cdc_acm_exit();
        }
        printk("t_uart_dev_work!\n");
		console_dev = device_get_binding(CONFIG_UART_CONSOLE_ON_DEV_NAME);
	}

	return console_dev;

    err_exit:
    ret = usb_cdc_acm_exit();
    if (ret) {
        printk("failed to exit CDC ACM device\n");
        console_dev = NULL;
    }
    return console_dev;

}

void uart_console_set_console_device(struct device *console_dev)
{
	int flags;

    if(uart_console_dev != NULL){
	    console_input_deinit(uart_console_dev);
    }
	console_input_init(console_dev);
	flags = irq_lock();
	uart_console_dev = console_dev;
	irq_unlock(flags);
}
#endif


#ifdef CONFIG_UART_CONSOLE_DMA_SUPPORT

static u8_t console_use_dma;

void uart_console_dma_switch(u8_t use_dma, dma_stream_handle_t dma_handler, void *pdata)
{
    if(console_use_dma == use_dma){
        return;
    }

    if(use_dma){
        uart_fifo_switch(uart_console_dev, 1, UART_FIFO_DMA);

        uart_dma_send_init(uart_console_dev, CONFIG_UART_CONSOLE_DMA_CHANNEL, \
            dma_handler, pdata);
    }else{
        uart_dma_send_stop(uart_console_dev);

    	uart_fifo_switch(uart_console_dev, 1, UART_FIFO_CPU);

    	__printk_hook_install(console_out);
    }

    console_use_dma = use_dma;

}

void uart_puts(char *s, int len)
{
    while (len != 0){
        console_out(*s);
        s++;
        len--;
    }
}
void rom_log_output(const char *s, unsigned int len);

void uart_console_output(char *s, int len)
{
    if (console_use_dma) {
        //uart_dma_send(uart_console_dev, s, len);
		rom_log_output(s, len);
    } else {
        uart_puts(s, len);
    }
}

int uart_console_is_busy(void)
{
    if(console_use_dma){
        return (uart_dma_send_complete(uart_console_dev) == 0);
    }else{
        return 0;
    }
}


#endif

/* UART console initializes after the UART device itself */
SYS_INIT(uart_console_init,
#if defined(CONFIG_EARLY_CONSOLE)
			PRE_KERNEL_1,
#else
			POST_KERNEL,
#endif
			CONFIG_UART_CONSOLE_INIT_PRIORITY);
#ifdef CONFIG_UART_CONSOLE_DEFAULT_DISABLE
SYS_INIT(uart_console_force_init,
#if defined(CONFIG_USB_UART_CONSOLE)
		 APPLICATION,
#elif defined(CONFIG_EARLY_CONSOLE)
		 PRE_KERNEL_2,
#else
		 POST_KERNEL,
#endif
		 CONFIG_UART_CONSOLE_INIT_PRIORITY);
#endif
