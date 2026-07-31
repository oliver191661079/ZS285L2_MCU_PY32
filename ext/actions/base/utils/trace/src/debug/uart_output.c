/* Porting from woodpecker bootrom */

#include <kernel.h>
#include <soc.h>
#include <cbuf.h>
#include <trace.h>
#include "debug_inner.h"

#define DMA_CTL_OFFSET                  (0x00)
#define DMA_START_OFFSET                (0x04)
#define DMA_SADDR_OFFSET                (0x08)
#define DMA_DADDR_OFFSET                (0x10)
#define DMA_BC_OFFSET                   (0x18)
#define DMA_RC_OFFSET                   (0x1c)

#define MRCR0_UART1_RESET               (16)
#define MRCR0_UART0_RESET               (15)
#define CMU_DEVCLKEN0_UART1CLKEN        (16)
#define CMU_DEVCLKEN0_UART0CLKEN        (15)

#define UART0_STA_UTBB                  (21)

#define GPIO0_CTL_PADDRV_SHIFT          (12)

#define DMACONTROLLER_BASE              0xc0040000

typedef volatile u32_t *REG32;

typedef struct {
    uint8_t print_mode;
    uint8_t print_sta;
    uint8_t print_uart_idx;
    uint8_t print_dma_chan_idx;
    uint32_t print_dma_chan_base;
	uint32_t uart_print_option;

	cbuf_t *cbuf;
	uint32_t drop_bytes;
	uint32_t last_dma_trans_len;

} debug_uart_t;

static debug_uart_t g_debug_uart;
debug_uart_t *p_debug_uart = &g_debug_uart;

static int cbuf_dma_complete_read(cbuf_t *cbuf, uint32_t dma_bc)
{
    cbuf->read_ptr += dma_bc;
    cbuf->read_ptr %= cbuf->capacity;
    cbuf->length -= dma_bc;

    return 0;
}

static int cbuf_dma_prepare_read(cbuf_t *cbuf, uint8_t **dma_start_addr, uint32_t *dma_bc)
{
    uint32_t copy_len;
    uint32_t block_size;

    copy_len = cbuf->capacity - cbuf->read_ptr;
    if (copy_len > cbuf->length) {
        copy_len = cbuf->length;
    }

    block_size = (p_debug_uart->uart_print_option & PRINT_BLOCK_SIZE_MASK);
    if (copy_len > block_size) {
        copy_len = block_size;
    }

    *dma_start_addr = cbuf->raw_data + cbuf->read_ptr;
    *dma_bc = copy_len;

    return copy_len;
}

static void debug_uart_puts_use_dma_start(uint8_t *s, unsigned int len)
{
    uint32_t start_reg;

    /* reload dma */
    sys_write32((unsigned int)s, p_debug_uart->print_dma_chan_base + DMA_SADDR_OFFSET);
    sys_write32(len, p_debug_uart->print_dma_chan_base + DMA_BC_OFFSET);

    /* start dma */
    start_reg = p_debug_uart->print_dma_chan_base + DMA_START_OFFSET;
    sys_write32(sys_read32(start_reg) | (1 << DMA0START_DMASTART), start_reg);
}

static inline bool is_debug_uart_puts_use_dma_trans_cp(void)
{
    uint32_t start_reg;

    /* start dma */
    start_reg = p_debug_uart->print_dma_chan_base + DMA_START_OFFSET;
    if ((sys_read32(start_reg) & (1 << DMA0START_DMASTART)) == 0) {
        return 1;
    }

    return 0;
}

static inline void debug_uart_puts_use_dma_trans_cp_clear(void)
{
    *((REG32)(DMAIP)) = (1 << p_debug_uart->print_dma_chan_idx);
}


static uint32_t cbuf_full_tips(uint32_t t_drop_bytes)
{
    char tmp_buf[10];

    tmp_buf[0] = '\r';
    tmp_buf[1] = '\n';
    tmp_buf[2] = '@';

    t_drop_bytes = t_drop_bytes % 10000;
    tmp_buf[3] = '0' + t_drop_bytes/1000;
    t_drop_bytes = t_drop_bytes % 1000;
    tmp_buf[4] = '0' + t_drop_bytes/100;
    t_drop_bytes = t_drop_bytes % 100;
    tmp_buf[5] = '0' + t_drop_bytes/10;
    t_drop_bytes = t_drop_bytes % 10;
    tmp_buf[6] = '0' + t_drop_bytes;
    tmp_buf[7] = '@';
    tmp_buf[8] = '\r';
    tmp_buf[9] = '\n';

    cbuf_write(p_debug_uart->cbuf, tmp_buf, 10);

    return 10;
}

static void debug_uart_puts_use_dma_dma_proc(uint8_t mode)
{
    uint8_t *trans_buf;

    if (mode == PRINT_MODE_IRQ_RELOAD) {
        p_debug_uart->print_sta = PRINT_BUF_PRINT_CP;
    }

    if ((p_debug_uart->print_sta == PRINT_BUF_PRINTING) && (is_debug_uart_puts_use_dma_trans_cp() == 1)) {
        debug_uart_puts_use_dma_trans_cp_clear();
        p_debug_uart->print_sta = PRINT_BUF_PRINT_CP;
    }

    if ((p_debug_uart->print_sta == PRINT_BUF_PRINT_CP) && (p_debug_uart->last_dma_trans_len > 0)) {
        cbuf_dma_complete_read(p_debug_uart->cbuf, p_debug_uart->last_dma_trans_len);
        p_debug_uart->last_dma_trans_len = 0;
    }

    if ((p_debug_uart->drop_bytes > 0) && (cbuf_get_free_space(p_debug_uart->cbuf) >= 10)) {
        cbuf_full_tips(p_debug_uart->drop_bytes);
        p_debug_uart->drop_bytes = 0;
    }

    if ((p_debug_uart->print_sta == PRINT_BUF_PRINTING) || (cbuf_get_used_space(p_debug_uart->cbuf) == 0)) {
        return;
    }

    cbuf_dma_prepare_read(p_debug_uart->cbuf, &trans_buf, &(p_debug_uart->last_dma_trans_len));

    debug_uart_puts_use_dma_start(trans_buf, p_debug_uart->last_dma_trans_len);

    p_debug_uart->print_sta = PRINT_BUF_PRINTING;
}

static void debug_uart_puts_use_dma(uint8_t *s, unsigned int len)
{
    uint32_t free_space;

    if (p_debug_uart->drop_bytes > 0) {
        p_debug_uart->drop_bytes += len;
        len = 0;
    } else {
        free_space = cbuf_get_free_space(p_debug_uart->cbuf);
        if (len > free_space) {
            p_debug_uart->drop_bytes += (len - free_space);
            len = free_space;
        }
    }

    if (len > 0) {
        cbuf_write(p_debug_uart->cbuf, (void *)s, len);
    }

    debug_uart_puts_use_dma_dma_proc(PRINT_MODE_CALL_START);
}

static void debug_uart_puts(uint8_t *s, unsigned int len)
{
    uint32_t uart_base;

    if (p_debug_uart->print_uart_idx == 0) {
        uart_base = UART0_REG_BASE;
    } else {
        uart_base = UART1_REG_BASE;
    }

    while (*s != 0 && len != 0) {
        /* Wait for fifo to flush out */
        while(sys_read32(REG_UART_STA(uart_base)) & (1 << UART0_STA_UTBB));

        sys_write32((unsigned int)(*s), REG_UART_TXDAT(uart_base));

        s++;
        len--;
    }
}

void debug_uart_puts_use_dma_dma_tc_isr(void)
{
    uint32_t flags;

    flags = irq_lock();
    debug_uart_puts_use_dma_dma_proc(PRINT_MODE_IRQ_RELOAD);
    irq_unlock(flags);
}

void debug_uart_trace_output(uint8_t *s, unsigned int len)
{
    if (p_debug_uart->print_mode == TRACE_MODE_DMA) {
        debug_uart_puts_use_dma(s, len);
    } else {
        debug_uart_puts(s, len);
    }
}

int debug_uart_trace_set_uart(uint8_t uart_idx, uint8_t io_idx, uint32_t uart_baud)
{
    p_debug_uart->print_uart_idx = uart_idx;
	p_debug_uart->uart_print_option = 0x100;

    return 0;
}

int debug_uart_trace_set_mode(trace_mode_e mode, uint8_t dma_chan_idx, cbuf_t *cbuf)
{
    uint32_t flags;

    flags = irq_lock();

    p_debug_uart->print_mode = mode;
    p_debug_uart->print_dma_chan_idx = dma_chan_idx;
    p_debug_uart->print_dma_chan_base = DMACONTROLLER_BASE + 0x100 + (0x100 * dma_chan_idx);
    p_debug_uart->cbuf = cbuf;

    irq_unlock(flags);

    return 0;
}

#ifndef CONFIG_TRACE_USE_ROM_CODE

#define LINESEP_FORMAT_WINDOWS  0
#define LINESEP_FORMAT_UNIX     1
enum pad_type {
	PAD_NONE,
	PAD_ZERO_BEFORE,
	PAD_SPACE_BEFORE,
	PAD_SPACE_AFTER,
};

#define _putc(_str, _end, _ch)  \
do                              \
{                               \
    if (_str < _end)            \
    {				\
        *_str = _ch;            \
        _str++;                 \
    }				\
}                               \
while (0)


/**
 * @brief Output an unsigned long in hex format
 *
 * Output an unsigned long on output installed by platform at init time. Should
 * be able to handle an unsigned long of any size, 32 or 64 bit.
 * @param num Number to output
 *
 * @return N/A
 */
__ramfunc static char* _printk_hex_ulong(char* str, char* end,
			      const unsigned long num, enum pad_type padding,
			      int min_width)
{
	int size = sizeof(num) * 2;
	int found_largest_digit = 0;
	int remaining = 8; /* 8 digits max */
	int digits = 0;

	for (; size; size--) {
		char nibble = (num >> ((size - 1) << 2) & 0xf);

		if (nibble || found_largest_digit || size == 1) {
			found_largest_digit = 1;
			nibble += nibble > 9 ? 87 : 48;
			_putc(str, end, (int)nibble);
			digits++;
			continue;
		}

		if (remaining-- <= min_width) {
			if (padding == PAD_ZERO_BEFORE) {
				_putc(str, end, '0');
			} else if (padding == PAD_SPACE_BEFORE) {
				_putc(str, end, ' ');
			}
		}
	}

	if (padding == PAD_SPACE_AFTER) {
		remaining = min_width * 2 - digits;
		while (remaining-- > 0) {
			_putc(str, end, ' ');
		}
	}
	return str;
}


/**
 * @brief Output an unsigned long (32-bit) in decimal format
 *
 * Output an unsigned long on output installed by platform at init time. Only
 * works with 32-bit values.
 * @param num Number to output
 *
 * @return N/A
 */
__ramfunc static char* _printk_dec_ulong(char* str, char* end,
			      const unsigned long num, enum pad_type padding,
			      int min_width)
{
	unsigned long pos = 999999999;
	unsigned long remainder = num;
	int found_largest_digit = 0;
	int remaining = 10; /* 10 digits max */
	int digits = 1;

	/* make sure we don't skip if value is zero */
	if (min_width <= 0) {
		min_width = 1;
	}

	while (pos >= 9) {
		if (found_largest_digit || remainder > pos) {
			found_largest_digit = 1;
			_putc(str, end, (int)((remainder / (pos + 1)) + 48));
			digits++;
		} else if (remaining <= min_width
				&& padding < PAD_SPACE_AFTER) {
			_putc(str, end, (int)(padding == PAD_ZERO_BEFORE ? '0' : ' '));
			digits++;
		}
		remaining--;
		remainder %= (pos + 1);
		pos /= 10;
	}

	_putc(str, end, (int)(remainder + 48));

	if (padding == PAD_SPACE_AFTER) {
		remaining = min_width - digits;
		while (remaining-- > 0) {
			_putc(str, end, ' ');
		}
	}
	return str;
}

__ramfunc int trace_vsnprintf(char* buf, int size, unsigned int linesep, const char* fmt, va_list ap)
{
	int might_format = 0; /* 1 if encountered a '%' */
	enum pad_type padding = PAD_NONE;
	int min_width = -1;
	int long_ctr = 0;
	char*  str = buf;
	char*  end = buf + size - 1;

	/* fmt has already been adjusted if needed */

	while (*fmt && *fmt != '\0') {
		if (!might_format) {
			if (*fmt != '%') {
				if (linesep == LINESEP_FORMAT_WINDOWS) {
					if(*fmt == '\n')
					_putc(str, end, '\r');
				}
				_putc(str, end, (int)*fmt);
			} else {
				might_format = 1;
				min_width = -1;
				padding = PAD_NONE;
				long_ctr = 0;
			}
		} else {
			switch (*fmt) {
			case '-':
				padding = PAD_SPACE_AFTER;
				goto still_might_format;
			case '0':
				if (min_width < 0 && padding == PAD_NONE) {
					padding = PAD_ZERO_BEFORE;
					goto still_might_format;
				}
				/* Fall through */
			case '1' ... '9':
				if (min_width < 0) {
					min_width = *fmt - '0';
				} else {
					min_width = 10 * min_width + *fmt - '0';
				}

				if (padding == PAD_NONE) {
					padding = PAD_SPACE_BEFORE;
				}
				goto still_might_format;
			case 'l':
				long_ctr++;
				/* Fall through */
			case 'z':
			case 'h':
				/* FIXME: do nothing for these modifiers */
				goto still_might_format;
			case 'd':
			case 'i': {
				long d;
				if (long_ctr < 2) {
					d = va_arg(ap, long);
				} else {
					d = (long)va_arg(ap, long long);
				}

				if (d < 0) {
					 _putc(str, end, (int)'-');
					d = -d;
					min_width--;
				}
				str = _printk_dec_ulong(str, end, d, padding,
						  min_width);
				break;
			}
			case 'u': {
				unsigned long u;

				if (long_ctr < 2) {
					u = va_arg(ap, unsigned long);
				} else {
					u = (unsigned long)va_arg(ap,
							unsigned long long);
				}

				str = _printk_dec_ulong(str, end, u, padding,
						  min_width);
				break;
			}
			case 'p':
				  _putc(str, end, '0');
				  _putc(str, end, 'x');
				  /* left-pad pointers with zeros */
				  padding = PAD_ZERO_BEFORE;
				  min_width = 8;
				  /* Fall through */
			case 'x':
			case 'X': {
				unsigned long x;

				if (long_ctr < 2) {
					x = va_arg(ap, unsigned long);
				} else {
					x = (unsigned long)va_arg(ap,
							unsigned long long);
				}

				str = _printk_hex_ulong(str, end, x, padding,
						  min_width);

				break;
			}
			case 's': {
				char *s = va_arg(ap, char *);

				while (*s) {
					_putc(str, end, (int)(*s));
					s++;
				}
				break;
			}
			case 'c': {
				int c = va_arg(ap, int);

				_putc(str, end, c);
				break;
			}
			case '%': {
				_putc(str, end, (int)'%');
				break;
			}
			default:
				_putc(str, end, (int)'%');
				_putc(str, end, (int)*fmt);
				break;
			}
			might_format = 0;
		}
still_might_format:
		++fmt;
	}
    if (str <= end) {
        *str = '\0';
	} else if (size > 0) {
        *end = '\0';
	}
	return (str - buf);
}
#endif

