/*
 * Copyright (c) 2026 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file Optional UART RX for bt_manager: line-oriented ASCII or binary command frames.
 *
 * IRQ + uart_fifo_read + ring buffer. Line mode: CR/LF delimited lines to
 * bt_manager_uart_rx_line_hook(). Binary mode (CONFIG_BT_MANAGER_UART_RX_PROTO_BINARY_CMD):
 * frames D3 | VER | len_hi | len_lo | payload | CRC8.
 */

#define SYS_LOG_DOMAIN "bt_mgr_uart_rx"
#define SYS_LOG_LEVEL SYS_LOG_LEVEL_INF
#include <logging/sys_log.h>

#include <zephyr.h>
#include <kernel.h>
#include <misc/printk.h>
#include <device.h>
#include <uart.h>
#include <irq.h>
#include <string.h>
#include <soc.h>

#ifdef CONFIG_BT_MANAGER_UART_RX

#ifndef CONFIG_UART_INTERRUPT_DRIVEN
#error "CONFIG_BT_MANAGER_UART_RX requires CONFIG_UART_INTERRUPT_DRIVEN"
#endif

#define RX_RING_SZ CONFIG_BT_MANAGER_UART_RX_RING_SIZE

static struct k_thread bt_mgr_uart_rx_tid;
K_THREAD_STACK_DEFINE(bt_mgr_uart_rx_stack, CONFIG_BT_MANAGER_UART_RX_STACK_SIZE);

static struct device *rx_uart_dev;
static u8_t rx_ring[RX_RING_SZ];
static volatile u16_t rx_head;
static volatile u16_t rx_tail;

static K_SEM_DEFINE(rx_wake, 0, 32);

/* Debug counters (ISR: bytes enqueued; thread: wakes / chars / lines) */
static volatile u32_t dbg_isr_enqueue_bytes;
static volatile u32_t dbg_isr_batches;
static volatile u32_t dbg_wake_count;
static volatile u32_t dbg_char_thread;
static volatile u32_t dbg_line_count;
static volatile u32_t dbg_ring_drop;
/** Set on first ISR batch that delivered RX bytes (confirms IRQ + fifo_read path). */
static volatile u8_t dbg_uart_rx_isr_alive;

__attribute__((weak)) void bt_manager_uart_rx_line_hook(const char *line, unsigned int len)
{
	ARG_UNUSED(line);
	ARG_UNUSED(len);
}

#ifdef CONFIG_BT_MANAGER_UART_RX_PROTO_BINARY_CMD
#include <bt_manager.h>

void bt_manager_uart_rx_binary_dispatch(const uint8_t *payload, uint16_t len);

enum {
	BIN_RX_SYNC = 0,
	BIN_RX_VER,
	BIN_RX_LEN_H,
	BIN_RX_LEN_L,
	BIN_RX_PAYLOAD,
	BIN_RX_CRC,
};

static u8_t bin_rx_state;
static u8_t bin_rx_ver;
static u16_t bin_rx_len;
static u16_t bin_rx_got;
static u8_t bin_rx_payload[CONFIG_BT_MANAGER_UART_RX_BIN_MAX_PAYLOAD];

static u8_t bin_crc8_feed(u8_t crc, u8_t byte)
{
	int i;

	crc ^= byte;
	for (i = 0; i < 8; i++) {
		crc = (u8_t)((crc & 1u) ? ((crc >> 1) ^ 0x8Cu) : (crc >> 1));
	}
	return crc;
}

static u8_t bin_crc8(const u8_t *data, size_t len)
{
	u8_t crc = 0;

	while (len--) {
		crc = bin_crc8_feed(crc, *data++);
	}
	return crc;
}

static void bin_rx_reset(void)
{
	bin_rx_state = BIN_RX_SYNC;
	bin_rx_ver = 0;
	bin_rx_len = 0;
	bin_rx_got = 0;
}

static void bt_mgr_uart_rx_binary_feed(u8_t c)
{
	u8_t crc_rx;
	u8_t crc_calc;
	u8_t mid[3];
	u16_t i;

	switch (bin_rx_state) {
	case BIN_RX_SYNC:
		if (c == 0xD3u) {
			bin_rx_state = BIN_RX_VER;
		}
		break;
	case BIN_RX_VER:
		bin_rx_ver = c;
		if (c == (u8_t)(CONFIG_BT_MANAGER_UART_RX_BIN_VER & 0xff)) {
			bin_rx_state = BIN_RX_LEN_H;
		} else if (c == 0xD3u) {
			bin_rx_state = BIN_RX_VER;
		} else {
			bin_rx_reset();
		}
		break;
	case BIN_RX_LEN_H:
		bin_rx_len = (u16_t)((u16_t)c << 8);
		bin_rx_state = BIN_RX_LEN_L;
		break;
	case BIN_RX_LEN_L:
		bin_rx_len = (u16_t)(bin_rx_len | c);
		if (bin_rx_len > (u16_t)CONFIG_BT_MANAGER_UART_RX_BIN_MAX_PAYLOAD) {
			bin_rx_reset();
			break;
		}
		bin_rx_got = 0;
		if (bin_rx_len == 0U) {
			bin_rx_state = BIN_RX_CRC;
		} else {
			bin_rx_state = BIN_RX_PAYLOAD;
		}
		break;
	case BIN_RX_PAYLOAD:
		bin_rx_payload[bin_rx_got++] = c;
		if (bin_rx_got >= bin_rx_len) {
			bin_rx_state = BIN_RX_CRC;
		}
		break;
	case BIN_RX_CRC:
		crc_rx = c;
		mid[0] = bin_rx_ver;
		mid[1] = (u8_t)((bin_rx_len >> 8) & 0xffu);
		mid[2] = (u8_t)(bin_rx_len & 0xffu);
		crc_calc = bin_crc8(mid, sizeof(mid));
		for (i = 0; i < bin_rx_len; i++) {
			crc_calc = bin_crc8_feed(crc_calc, bin_rx_payload[i]);
		}
		if (crc_calc == crc_rx) {
			printk("[uart_rx] BIN frame OK len=%u [0]=0x%02x [1]=0x%02x\n",
			       (unsigned int)bin_rx_len,
			       bin_rx_len > 0 ? (unsigned int)bin_rx_payload[0] : 0,
			       bin_rx_len > 1 ? (unsigned int)bin_rx_payload[1] : 0);
			bt_manager_uart_rx_binary_dispatch(bin_rx_payload, bin_rx_len);
		} else {
			printk("[uart_rx] BIN crc FAIL calc=0x%02x rx=0x%02x len=%u\n",
			       crc_calc, crc_rx, (unsigned int)bin_rx_len);
			SYS_LOG_WRN("bin uart crc err calc=0x%02x rx=0x%02x len=%u\n",
				    crc_calc, crc_rx, (unsigned int)bin_rx_len);
		}
		bin_rx_reset();
		break;
	default:
		bin_rx_reset();
		break;
	}
}

/* UART 帧解析完成后转统一命令入口；应用可覆盖本 weak 函数扩展协议 */
__attribute__((weak)) void bt_manager_uart_rx_binary_dispatch(const uint8_t *payload, uint16_t len)
{
	printk("[uart_rx] -> bt_manager_remote_cmd_dispatch\n");
	(void)bt_manager_remote_cmd_dispatch(payload, len, "uart");
}
#endif /* CONFIG_BT_MANAGER_UART_RX_PROTO_BINARY_CMD */

static void bt_mgr_uart_isr(struct device *dev)
{
	u8_t chunk[32];
	int n;
	unsigned int key;
	int i;

	n = uart_fifo_read(dev, chunk, sizeof(chunk));
	if (n <= 0)
		return;

	dbg_isr_batches++;

	if (n > 0 && dbg_uart_rx_isr_alive == 0) {
		dbg_uart_rx_isr_alive = 1;
		printk("[uart_rx] ISR alive: first uart_fifo_read n=%d (RX IRQ works)\n", n);
	}

	key = irq_lock();
	for (i = 0; i < n; i++) {
		u16_t next = (u16_t)((rx_head + 1U) % RX_RING_SZ);

		if (next == rx_tail) {
			dbg_ring_drop++;
			break;
		}
		rx_ring[rx_head] = chunk[i];
		rx_head = next;
		dbg_isr_enqueue_bytes++;
	}
	irq_unlock(key);

	k_sem_give(&rx_wake);
}

static void bt_manager_uart_rx_thread(void *p1, void *p2, void *p3)
{
#ifdef CONFIG_BT_MANAGER_UART_RX_PROTO_BINARY_CMD
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	SYS_LOG_INF("thread start ring=%d mode=BINARY_CMD ver=0x%02x max_pl=%d prio=%d\n",
		    RX_RING_SZ, (unsigned int)CONFIG_BT_MANAGER_UART_RX_BIN_VER,
		    CONFIG_BT_MANAGER_UART_RX_BIN_MAX_PAYLOAD, CONFIG_BT_MANAGER_UART_RX_PRIO);
	printk("[uart_rx] thread started (BINARY) dev=%s prio=%d ring=%u\n",
	       CONFIG_BT_MANAGER_UART_RX_DEV_NAME, CONFIG_BT_MANAGER_UART_RX_PRIO, RX_RING_SZ);
#else
	char line[CONFIG_BT_MANAGER_UART_RX_LINE_MAX];
	unsigned int llen = 0;

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	SYS_LOG_INF("thread start ring=%d line_max=%d prio=%d\n", RX_RING_SZ,
		    CONFIG_BT_MANAGER_UART_RX_LINE_MAX, CONFIG_BT_MANAGER_UART_RX_PRIO);
	printk("[uart_rx] thread started (LINE) dev=%s prio=%d\n",
	       CONFIG_BT_MANAGER_UART_RX_DEV_NAME, CONFIG_BT_MANAGER_UART_RX_PRIO);
#endif

	for (;;) {
		u16_t used;
		unsigned int key;

		(void)k_sem_take(&rx_wake, K_FOREVER);
		dbg_wake_count++;

		key = irq_lock();
		if (rx_head >= rx_tail)
			used = (u16_t)(rx_head - rx_tail);
		else
			used = (u16_t)(RX_RING_SZ - rx_tail + rx_head);
		irq_unlock(key);

		if (dbg_wake_count <= 40U || (dbg_wake_count % 64U) == 0U)
			SYS_LOG_INF("wake#%u ring_used=%u isr_bytes=%u batches=%u "
				    "thr_ch=%u lines=%u drop=%u\n",
				    (unsigned int)dbg_wake_count, used,
				    (unsigned int)dbg_isr_enqueue_bytes,
				    (unsigned int)dbg_isr_batches,
				    (unsigned int)dbg_char_thread,
				    (unsigned int)dbg_line_count,
				    (unsigned int)dbg_ring_drop);

		if (used == 0U)
			SYS_LOG_INF("wake#%u: sem fired but ring empty (spurious?)\n",
				    (unsigned int)dbg_wake_count);

		for (;;) {
			u8_t c;

			key = irq_lock();
			if (rx_tail == rx_head) {
				irq_unlock(key);
				break;
			}
			c = rx_ring[rx_tail];
			rx_tail = (u16_t)((rx_tail + 1U) % RX_RING_SZ);
			irq_unlock(key);

			dbg_char_thread++;

#ifdef CONFIG_BT_MANAGER_UART_RX_PROTO_BINARY_CMD
			bt_mgr_uart_rx_binary_feed(c);
#else
			if (c == '\r' || c == '\n') {
				if (llen > 0U) {
					line[llen] = '\0';
					dbg_line_count++;
					SYS_LOG_INF("line#%u len=%u \"%s\"\n",
						    (unsigned int)dbg_line_count, llen,
						    line);
					bt_manager_uart_rx_line_hook(line, llen);
					llen = 0U;
				}
			} else if (llen < sizeof(line) - 1U) {
				line[llen++] = (char)c;
			} else {
				SYS_LOG_WRN("line overflow, reset\n");
				llen = 0U;
			}
#endif
		}
	}
}

void bt_manager_uart_rx_init(void)
{
	printk("[uart_rx] init: binding %s\n", CONFIG_BT_MANAGER_UART_RX_DEV_NAME);
	rx_uart_dev = device_get_binding(CONFIG_BT_MANAGER_UART_RX_DEV_NAME);
	if (!rx_uart_dev) {
		printk("bt_mgr_uart_rx: device_get_binding(%s) failed\n",
		       CONFIG_BT_MANAGER_UART_RX_DEV_NAME);
		SYS_LOG_ERR("bind failed dev=%s\n", CONFIG_BT_MANAGER_UART_RX_DEV_NAME);
		return;
	}

	rx_head = 0;
	rx_tail = 0;
	dbg_uart_rx_isr_alive = 0;

#ifdef CONFIG_BT_MANAGER_UART_RX_PROTO_BINARY_CMD
	bin_rx_reset();
#endif

#if defined(CONFIG_UART_DMA_DRIVEN)
	{
		int sw_err;

		/*
		 * If RX was switched to DMA elsewhere, uart_fifo_read() in our ISR often sees
		 * nothing — force CPU FIFO for RX while bt_manager owns this UART.
		 */
		sw_err = uart_fifo_switch(rx_uart_dev, 0, UART_FIFO_CPU);
		printk("[uart_rx] RX uart_fifo_switch(CPU) ret=%d (0=ok)\n", sw_err);
	}
#endif

	uart_irq_rx_disable(rx_uart_dev);
	uart_irq_callback_set(rx_uart_dev, bt_mgr_uart_isr);
	uart_irq_rx_enable(rx_uart_dev);

	SYS_LOG_INF("init ok dev=%s ring=%u stack=%u prio=%u irq_rx=on\n",
		    CONFIG_BT_MANAGER_UART_RX_DEV_NAME,
		    (unsigned int)RX_RING_SZ,
		    (unsigned int)CONFIG_BT_MANAGER_UART_RX_STACK_SIZE,
		    (unsigned int)CONFIG_BT_MANAGER_UART_RX_PRIO);
	printk("[uart_rx] init done: irq_cb=%p rx_ie=on (send test bytes on this UART)\n",
	       (void *)bt_mgr_uart_isr);

	k_thread_create(&bt_mgr_uart_rx_tid, bt_mgr_uart_rx_stack,
			K_THREAD_STACK_SIZEOF(bt_mgr_uart_rx_stack),
			bt_manager_uart_rx_thread, NULL, NULL, NULL,
			CONFIG_BT_MANAGER_UART_RX_PRIO, 0, K_NO_WAIT);
}

#endif /* CONFIG_BT_MANAGER_UART_RX */
