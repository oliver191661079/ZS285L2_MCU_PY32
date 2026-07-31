/********************************************************************************
 *                            USDK(ATS350B_linux)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>        <time>                          <version >      <desc>
 *      wuyufan         2018-8-9 9:56:29 PM             1.0             build this file
 *      henry.han       Tue, Oct 13, 2020  5:26:01 PM   1.1             Modify trace dma transfer flow.
 ********************************************************************************/
/*!
 * \file     trace_impl.c
 * \brief    trace implementation, supporting both sync and async mode.
 * \author
 * \version  1.1
 * \date  Tue, Oct 13, 2020  5:26:01 PM
 *******************************************************************************/
#include <init.h>
#include <kernel.h>
#include <cbuf.h>
#include <trace.h>
#include <soc.h>
#include <trace_impl.h>
#include <ext_types.h>
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_TRACE_IMPL
#ifdef CONFIG_TRACE_UART
#include <dma.h>
#include <drivers/console/uart_console.h>
#endif
#ifdef CONFIG_TRACE_FILE
#include <trace_file.h>
#endif

#include <os_common_api.h>

#include <string.h>
#include <stdio.h>
#include <acts_ringbuf.h>
#include <uart.h>

#include "trace_test.h"

//Warning: Sync mode can not be used when interrupts call trace.
//#define TRACE_SYNC_MODE
//#define TRACE_DEBUG_VERBOSE

//TODO: Connet to cdc acm header file
#define HS_BULK_EP_MPS	512

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

#define LINESEP_FORMAT_WINDOWS  0
#define LINESEP_FORMAT_UNIX     1

trace_ctx_t g_trace_ctx;

#define TRACE_TEMP_BUF_SIZE  128
static __trace_printbuf char trace_print_buffer[CONFIG_TRACE_BUFFER_SIZE];
#ifdef TRACE_MONITOR_ENABLE
static __trace_tempbuf char trace_temp_buffer[TRACE_TEMP_BUF_SIZE];
#endif
#define TRACE_HEX_BUF_SIZE 128
static char trace_hex_buffer[TRACE_HEX_BUF_SIZE];

extern char *bin2hex(char *dst, const void *src, unsigned long count);

extern int _arch_exception_vector_read(void);

extern unsigned int get_PSR(void);
extern
void tracing_init(void);

#ifdef CONFIG_TRACE_USE_ROM_CODE
int rom_vsnprintf(char *buf, size_t size, unsigned int type, const char *fmt,
                  va_list args);
#endif

#ifdef CONFIG_DSP_DEBUG_PRINT
extern struct acts_ringbuf *dsp_dev_get_print_buffer(void);
#endif


#ifdef CONFIG_USE_ROM_LOG
struct log_init_param
{
    u8_t uart_idx;
    u8_t io_idx;
    u32_t uart_baud;
    u8_t dma_chan_idx;
    cbuf_t *cbuf;
};

void rom_log_init(struct log_init_param *init_param);
void rom_log_set_mode(u8_t mode, u32_t max_pkt_size);
int rom_log_vprintk(const char *fmt, va_list args);
void rom_log_hexdump(uint8_t * src_buf, u32_t src_len, u8_t *dst_buf, u32_t dst_len);
void rom_log_output(const char *s, unsigned int len);
void rom_log_sync(void);
void rom_log_set_panic(uint8_t panic);
#endif


#if defined(TRACE_DEBUG_VERBOSE) || defined(TRACE_TEST)
static char *num_to_str(int num, int size, char *str)
{
    int i;

    for (i = 0; i < size; i++) {
        str[size - 1 - i] = '0' + num % 10;
        num = num / 10;
    }

    return str;
}
#endif

#if defined(TRACE_DEBUG_VERBOSE) || defined (LOGIC_ANALYSIS_SERIAL_TEST)
int uart_print_data(const u8_t * buf, int len)
{
    struct device *dev;
    int i;

    dev = device_get_binding(CONFIG_UART_ACTS_PORT_0_NAME);
    if (!dev) {
        //ACT_LOG_ID_INF(ALF_STR_uart_print_data__ACTS_UART_DEVICE_NOT, 0);
        return 0;
    } else {
        for (i = 0; i < len; i++) {
            uart_poll_out(dev, buf[i]);
        }
        return len;
    }
}

int uart_print(const char *str)
{
    return uart_print_data((u8_t *) str, strlen(str));
}
#endif

int hal_trace_check_busy(trace_ctx_t * trace_ctx)
{
    int trace_busy = 0;

#ifdef CONFIG_TRACE_UART
    if (trace_transport_support(trace_ctx, TRACE_TRANSPORT_UART)) {
        trace_busy |= uart_console_is_busy();
    }
#endif

    return trace_busy;
}

#ifdef TRACE_MONITOR_ENABLE
static void trace_putc(int c)
{
    static int temp_index;

    if (temp_index < TRACE_TEMP_BUF_SIZE) {
        trace_temp_buffer[temp_index] = (char)c;
        temp_index++;
    }
}
#endif

static int trace_out(int c)
{
#ifdef TRACE_MONITOR_ENABLE
    if ('\n' == c) {
        trace_putc('\r');
    }
    trace_putc(c);
#endif

    return 0;
}

void hal_trace_output(char *buf, int len)
{
#ifdef CONFIG_TRACE_UART
    uart_console_output(buf, len);
#endif

#ifdef CONFIG_TRACE_FILE
    hal_trace_file_output(buf, len);
#endif
}

int hal_trace_need_sync(trace_ctx_t * trace_ctx)
{
    int vec;

    int need_sync = 0;

	if (trace_ctx->trace_mode != TRACE_MODE_DMA_USB_CDC_ACM){
		if (!trace_ctx->init || trace_ctx->panic) {
			need_sync = 1;
		} else {
			vec = _arch_exception_vector_read();

			if (vec != 0 && vec != 22 && vec < 32) {
				need_sync = 1;
			}
#ifdef CONFIG_TRACE_SYNC_SHELL_THREAD
			else if (os_thread_priority_get(os_current_get()) ==
					 CONFIG_CONSOLE_SHELL_PRIORITY) {
				need_sync = 1;
			}
#endif
		}
    }

    return need_sync;
}

void trace_send_isr(struct device *dev, u32_t priv_data, int reson)
{
}

void trace_output(const unsigned char *buf, unsigned int len,
                  trace_ctx_t * trace_ctx)
{
#ifndef LOGIC_ANALYSIS_SERIAL_TEST

    if (trace_ctx->print_disable){
        return;
    }

    if ((trace_ctx->trace_mode == TRACE_MODE_DMA)
        || (trace_ctx->trace_mode == TRACE_MODE_DMA_USB_CDC_ACM)) {
#ifdef CONFIG_USE_ROM_LOG
	    rom_log_output(buf, len);
	    return;
#endif

    } else {
        hal_trace_output((char *)buf, len);
    }
#endif
}


#ifdef CONFIG_ACTION_LOG_FRAME
void trace_output_data(const uint8_t *buf, size_t len)
{
	trace_ctx_t *trace_ctx = get_trace_ctx();

	if (NULL != trace_ctx) {
		if (!trace_ctx->init) {
			return;
		}
		trace_output((const unsigned char *)buf, len, trace_ctx);
	}
}
#endif

void trace_hexdump(uint8_t *buf, uint32_t len)
{
    uint32_t i, cnt_hex;
    uint32_t flags;
    uint8_t *hex_ptr;
    trace_ctx_t *trace_ctx = get_trace_ctx();

    //Parameters checking.
    if (len == 0 || TRACE_HEX_BUF_SIZE < 4 || trace_ctx->lowpower_mode)
    {
        return;
    }

    flags = irq_lock();

    hex_ptr = trace_hex_buffer;
    cnt_hex = 0;

    for (i = 0; i < len; i++)
    {
        //Each UINT8 is translated to 3 or 4 chars (2Hex char + Space + New line (optinal))
        if (cnt_hex + 4 >= TRACE_HEX_BUF_SIZE)
        {
            trace_output(trace_hex_buffer, cnt_hex, trace_ctx);
            hex_ptr = trace_hex_buffer;
            cnt_hex = 0;
        }

        if (i != 0)
        {
            if (i % 16 == 0)
            {
                *hex_ptr = '\n';
                hex_ptr++;
                cnt_hex++;
            }
        }

        hex_ptr = bin2hex(hex_ptr, (const void *)(buf + i), 1);
        cnt_hex += 2;

        *hex_ptr = ' ';
        hex_ptr++;
        cnt_hex++;
    }

    *hex_ptr = '\n';
    cnt_hex++;
    trace_output(trace_hex_buffer, cnt_hex, trace_ctx);

    irq_unlock(flags);
}

#ifdef CONFIG_DSP_DEBUG_PRINT
int trace_dsp_debug_data(void)
{
#define PRE_STR_LEN 6
#define POST_STR_LEN 7
#define DSP_STR_LEN 0x40

    u8_t buffer[PRE_STR_LEN + DSP_STR_LEN*2];
    trace_ctx_t *trace_ctx;
    int length;
    struct acts_ringbuf *debug_buf = dsp_dev_get_print_buffer();

    if (!debug_buf) {
        return 0;
    }

    length = acts_ringbuf_length(debug_buf);

    if (length >= DSP_STR_LEN*2) {
        length = DSP_STR_LEN*2;
    }

    if (length >= 2) {
        trace_ctx = get_trace_ctx();

        memcpy(buffer, "\n<dsp>", 6);

        acts_ringbuf_get(debug_buf, buffer+PRE_STR_LEN, length);
        /* dsp puts 16 bits little-endian char in ringbuffer,
         * so convert it to 8 bits for cpu.
        */
        for(int i = 0; i < length/2; i++){
            buffer[PRE_STR_LEN + i] = buffer[PRE_STR_LEN + i * 2];
        }

        memcpy(buffer + PRE_STR_LEN + length / 2, "</dsp>\n", POST_STR_LEN);

        trace_output(buffer, PRE_STR_LEN+length/2+POST_STR_LEN, trace_ctx);
    }

    return length;
}
#else
int trace_dsp_debug_data(void)
{
    return 0;
}
#endif

void trace_transport_onoff(uint8_t transport_type, uint8_t is_on)
{
    trace_ctx_t *trace_ctx = get_trace_ctx();

    if (is_on) {
        trace_ctx->transport |= transport_type;
    } else {
        trace_ctx->transport &= (~transport_type);
    }
}

int trace_mode_set(unsigned int trace_mode)
{
    int old_trace_mode;
#ifdef CONFIG_SYS_IRQ_LOCK
    SYS_IRQ_FLAGS irq_flag;
	int vec = _arch_exception_vector_read();

	if (vec == 0 || vec >= 32) {
    	sys_irq_lock(&irq_flag);
	}
#endif

    trace_ctx_t *trace_ctx = get_trace_ctx();

    old_trace_mode = trace_ctx->trace_mode;

    trace_ctx->trace_mode = trace_mode;

    if (trace_mode != TRACE_MODE_CPU) {
#ifdef CONFIG_TRACE_UART
        uart_console_dma_switch(1, trace_send_isr, trace_ctx);
#endif
        __printk_hook_install(trace_out);
        __stdout_hook_install(trace_out);

    } else {
#ifdef CONFIG_USE_ROM_LOG
		rom_log_sync();
#endif

#ifdef CONFIG_TRACE_UART
        uart_console_dma_switch(0, 0, 0);
#endif
    }

#ifdef CONFIG_USE_ROM_LOG
	if(trace_mode == TRACE_MODE_DMA_USB_CDC_ACM){
#ifdef CONFIG_USB_AOTG_DC_FS
		rom_log_set_mode(trace_mode, 64);
#else
		rom_log_set_mode(trace_mode, 512);
#endif
	}else{
		rom_log_set_mode(trace_mode, 0);
	}
#endif

#ifdef CONFIG_SYS_IRQ_LOCK
	if (vec == 0 || vec >= 32) {
		sys_irq_unlock(&irq_flag);
	}
#endif

    return old_trace_mode;
}

#ifdef CONFIG_USB_UART_CONSOLE
static int trace_device_set(struct device *new_console_dev, int trace_mode)
{
#ifdef CONFIG_SYS_IRQ_LOCK
    SYS_IRQ_FLAGS irq_flag;
    sys_irq_lock(&irq_flag);
#endif

    trace_ctx_t *trace_ctx = get_trace_ctx();

    //ACT_LOG_ID_INF(ALF_STR_trace_device_set__CUR_TRACE_MODE0XXN, 1,trace_ctx->trace_mode);

    if (trace_ctx->trace_mode == TRACE_MODE_DMA) {
#ifdef CONFIG_TRACE_UART
        uart_console_dma_switch(0, 0, 0);
#endif
    }

    uart_console_set_console_device(new_console_dev);

    trace_ctx->trace_mode = trace_mode;

    if ((trace_ctx->trace_mode == TRACE_MODE_DMA)
        || (trace_ctx->trace_mode == TRACE_MODE_DMA_USB_CDC_ACM)) {
#ifdef CONFIG_TRACE_UART
        if (trace_ctx->trace_mode == TRACE_MODE_DMA_USB_CDC_ACM) {
            uart_console_dma_switch(2, trace_send_isr, trace_ctx);
        } else {
            uart_console_dma_switch(1, trace_send_isr, trace_ctx);
        }
#endif
        __printk_hook_install(trace_out);
        __stdout_hook_install(trace_out);
    }

#ifdef CONFIG_USE_ROM_LOG
	if(trace_mode == TRACE_MODE_DMA_USB_CDC_ACM){
#ifdef CONFIG_USB_AOTG_DC_FS
		rom_log_set_mode(trace_mode, 64);
#else
		rom_log_set_mode(trace_mode, 512);
#endif
	}else{
		rom_log_set_mode(trace_mode, 0);
	}
#endif


#ifdef CONFIG_SYS_IRQ_LOCK
    sys_irq_unlock(&irq_flag);
#endif

    return 0;
}

void trace_set_usb_console_active(u8_t active)
{
    struct device *new_console_dev;
    trace_ctx_t *trace_ctx = get_trace_ctx();

    printk("trace_set_usb_console_activ:%d. Old cdc mode is %d\n", active,
           trace_ctx->cdc_mode);

    if (active != trace_ctx->cdc_mode) {
        trace_ctx->init = false;
        //Wait 1 second to let dma flush trace buffer.
        k_sleep(1000);
        if (!trace_ctx->cdc_mode) {
            new_console_dev = uart_console_reinit(true);
            if (NULL != new_console_dev) {
                trace_device_set(new_console_dev, TRACE_MODE_DMA_USB_CDC_ACM);
                trace_ctx->cdc_mode = true;
            }
        } else {
            new_console_dev = uart_console_reinit(false);
            if (NULL != new_console_dev) {
                trace_device_set(new_console_dev, TRACE_MODE_DMA);
                trace_ctx->cdc_mode = false;
            }
        }
        trace_ctx->init = true;
    }

    return;
}

u8_t trace_get_usb_console_status(void)
{
    trace_ctx_t *trace_ctx = get_trace_ctx();
    if (trace_ctx && trace_ctx->cdc_mode) {
        return 1;
    }
    return 0;
}
#endif

int trace_irq_print_set(unsigned int irq_enable)
{
    int old_irq_mode;

    trace_ctx_t *trace_ctx = get_trace_ctx();

    old_irq_mode = trace_ctx->irq_print_disable;

    trace_ctx->irq_print_disable = irq_enable;

    return old_irq_mode;
}

int trace_dma_print_set(unsigned int dma_enable)
{
	int old_dma_enable;

	trace_ctx_t *trace_ctx = get_trace_ctx();

	old_dma_enable = !trace_ctx->dma_print_disable;

	if (dma_enable == false){
		trace_ctx->dma_print_disable = 1;
		if (trace_ctx->init) {
			trace_mode_set(TRACE_MODE_CPU);
		}
 	} else{
		trace_ctx->dma_print_disable = 0;
		if (trace_ctx->init) {
	        if (hal_trace_need_sync(trace_ctx)) {
				if(trace_ctx->trace_mode != TRACE_MODE_CPU){
		            trace_mode_set(TRACE_MODE_CPU);
				}
	        } else {
	            if (trace_ctx->trace_mode == TRACE_MODE_CPU) {
	                trace_mode_set(TRACE_MODE_DMA);
	            }
	        }
		}
 	}

	return old_dma_enable;
}

void rom_log_print_disable_set(u8_t print_disable);
int trace_print_disable_set(unsigned int print_disable)
{
	int old_print_disable;

	trace_ctx_t *trace_ctx = get_trace_ctx();

	old_print_disable = trace_ctx->print_disable;

    trace_ctx->print_disable = print_disable;

    rom_log_print_disable_set(print_disable);

	return old_print_disable;
}
#ifdef TRACE_MONITOR_ENABLE
void trace_monitor(trace_ctx_t * trace_ctx)
{
    int out_len;

    if (trace_ctx->nested) {
        out_len = snACT_LOG_ID_INF(trace_temp_buffer, sizeof(trace_temp_buffer), ALF_STR_trace_monitor__TRACE_NESTEDN, 0);

        trace_output(trace_temp_buffer, out_len, trace_ctx);
        trace_ctx->nested = 0;
        trace_ctx->in_trace = 0;
    }

    if (trace_ctx->caller) {
        out_len =
            snprintk(trace_temp_buffer, sizeof(trace_temp_buffer), "trace when irq disable %x\n",
                     trace_ctx->caller);
        trace_output(trace_temp_buffer, out_len, trace_ctx);
        trace_ctx->caller = 0;
    }
}
#endif

#ifdef CONFIG_TRACE_TIME_FREFIX

static uint8_t digits[] = "0123456789abcdef";

static void get_time_prefix(char *num_str)
{
    uint32_t number;
    uint8_t num_len, ch;

    number = k_cycle_get_32() / 24;

    num_str[13] = ' ';
    num_str[12] = ']';
    num_len = 11;
    while (number != 0) {
        ch = digits[number % 10];
        num_str[num_len--] = ch;
        number /= 10;
    }
    ch = num_len;
    while (ch > 0)
        num_str[ch--] = ' ';
    num_str[0] = '[';
    while (++num_len <= 8)
        num_str[num_len - 1] = num_str[num_len];
    num_str[8] = '.';
}
#endif

extern int __vprintk(const char *fmt, va_list ap);

int vprintk_sub(const char *fmt, va_list args)
{
    int ret;
    char *p_buf;
    size_t size;
    char trans_buffer[TRACE_TEMP_BUF_SIZE];
    trace_ctx_t *trace_ctx = get_trace_ctx();

	p_buf = trans_buffer;
	size = TRACE_TEMP_BUF_SIZE;
	ret = 0;

#ifdef CONFIG_TRACE_TIME_FREFIX
	get_time_prefix(p_buf);
	p_buf += 14;
	size -= 14;
	ret += 14;
#endif

#ifdef CONFIG_TRACE_USE_ROM_CODE
	ret += vsnprintf(p_buf, size, fmt, args);
#else
	ret += vsnprintk(p_buf, size, fmt, args);
#endif

	trace_output(trans_buffer, ret, trace_ctx);

	return ret;

}

int vprintk(const char *fmt, va_list args)
{
#ifdef CONFIG_USE_ROM_TRACING
//#ifdef TRACE_TEST
    //When do trace test, disable all other print information.
    return 0;
//#endif
#endif

    trace_ctx_t *trace_ctx = get_trace_ctx();

    if (trace_ctx->print_disable){
        return 0;
    }

    if (!trace_ctx->init) {
        return __vprintk(fmt, args);;
    }
#ifdef TRACE_MONITOR_ENABLE
    trace_monitor(trace_ctx);
#endif

    if (trace_ctx->irq_print_disable) {
        if (k_is_in_isr()) {
            return 0;
        }
    }

    if (trace_ctx->dma_print_disable == 0) {
        if (hal_trace_need_sync(trace_ctx)) {
			if(trace_ctx->trace_mode != TRACE_MODE_CPU){
	            trace_mode_set(TRACE_MODE_CPU);
			}
        } else {
            if (trace_ctx->trace_mode == TRACE_MODE_CPU) {
                trace_mode_set(TRACE_MODE_DMA);
            }
        }
    }

#ifndef CONFIG_USE_ROM_LOG
	return vprintk_sub(fmt, args);
#else
	return rom_log_vprintk(fmt, args);
#endif
}

void trace_set_panic(void)
{
    trace_ctx_t *trace_ctx = get_trace_ctx();

    trace_ctx->panic = 1;

#ifdef CONFIG_USE_ROM_LOG
	rom_log_set_panic(true);
#endif

    return;
}

void trace_clear_panic(void)
{
    trace_ctx_t *trace_ctx = get_trace_ctx();

    trace_ctx->panic = 0;

    return;
}

void trace_set_lowpower_mode(u8_t lowpower_mode)
{
    trace_ctx_t *trace_ctx = get_trace_ctx();

    trace_ctx->lowpower_mode = lowpower_mode;

}

#ifdef CONFIG_ASSERT

void trace_assert(ASSERT_PARAM)
{
    va_list ap;

    trace_set_panic();
#if defined(CONFIG_ASSERT_SHOW_FILE_FUNC)
    printk("ASSERTION FAIL [%s] @ %s:%s:%d:\n\t", test, file, func, line);
#elif  defined(CONFIG_ASSERT_SHOW_FILE) || defined(CONFIG_ASSERT_SHOW_FUNC)
    printk("ASSERTION FAIL [%s] @ %s:%d:\n\t", test, str, line);
#else
    printk("ASSERTION FAIL [%s] @ %d:\n\t", test, line);
#endif

    va_start(ap, fmt);
    vprintk(fmt, ap);
    va_end(ap);

    panic(0);
}
#endif

#ifdef CONFIG_USE_ROM_LOG
void trace_init_rom_log(cbuf_t *cbuf)
{
	struct log_init_param init_param;

	init_param.uart_idx = 0;
	init_param.io_idx = 3;
	init_param.uart_baud = CONFIG_UART_ACTS_PORT_0_BAUD_RATE;
	init_param.dma_chan_idx = 1;
	init_param.cbuf = cbuf;

	rom_log_init(&init_param);
}
#endif

void trace_init(void)
{
    trace_ctx_t *trace_ctx = get_trace_ctx();

    if (trace_ctx->init) {
        return;
    }

    cbuf_init(&trace_ctx->cbuf, (void *)trace_print_buffer,
              sizeof(trace_print_buffer));

#ifdef CONFIG_USE_ROM_LOG
	trace_init_rom_log(&trace_ctx->cbuf);
#endif

    trace_transport_onoff(TRACE_TRANSPORT_UART, 1);

    if (trace_ctx->dma_print_disable == 0) {
        trace_mode_set(TRACE_MODE_DMA);
    } else {
        trace_mode_set(TRACE_MODE_CPU);
    }

    __printk_hook_install(trace_out);


#ifdef TRACE_TEST
    trace_test_init();
#endif

#ifdef LOGIC_ANALYSIS_SERIAL_TEST
    serial_speed_test();
#endif

#ifdef CONFIG_USE_ROM_TRACING
	tracing_init();
#endif

    trace_ctx->init = TRUE;

}

//SYS_INIT(trace_init, APPLICATION, CONFIG_UART_CONSOLE_INIT_PRIORITY);
