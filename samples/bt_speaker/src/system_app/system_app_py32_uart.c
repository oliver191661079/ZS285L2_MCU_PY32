/*
 * Copyright (c) 2026 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file system_app_py32_uart.c
 *
 * @brief PY32F005 主控 <-> ATS2853P2 蓝牙芯片 UART 协议
 *
 * UART1 RX=GPIO22 @115200 硬件 DMA；TX=GPIO21 @115200 硬件 UART（MFP=0xe，表6.4）。
 */

#include <string.h>
#include <zephyr.h>
#include <kernel.h>
#include <device.h>
#include <uart.h>
#include <misc/printk.h>
#include <misc/util.h>
#include <gpio.h>
#include <soc.h>
#include <soc_uart.h>
#include <soc_pinmux.h>
#include <os_common_api.h>

#include <audio_system.h>
#include <audio_policy.h>
#include <volume_manager.h>
#include <media_player.h>
#include <media_effect_param.h>

#include "system_app.h"

#ifdef CONFIG_CODEC_ACM8635
#include <board.h>
#endif

#if defined(CONFIG_SYSTEM_APP_PY32_UART)

#if !defined(CONFIG_UART_DMA_DRIVEN) || !defined(CONFIG_UART_DMA_RX_DRIVEN)
#error "SYSTEM_APP_PY32_UART requires CONFIG_UART_DMA_DRIVEN and CONFIG_UART_DMA_RX_DRIVEN"
#endif

/* ANDESC: ctrl@0 rxdat@4 txdat@8 stat@12 br@16 — 勿读 txdat 当 ctrl */

#ifndef CONFIG_SYSTEM_APP_PY32_UART_INIT_DELAY_MS
#define CONFIG_SYSTEM_APP_PY32_UART_INIT_DELAY_MS 1500
#endif

#define PY32_ADDR_HOST    0x01U  /* PY32 → ATS2853P2 */
#define PY32_ADDR_SLAVE   0x02U  /* ATS2853P2 → PY32 */
#define PY32_CMD_COMMON   0x10U  /* 通用指令码（V1.1） */

#define PY32_SET_DATA_LEN       4U
#define PY32_RSP_STATE_LEN      6U   /* 音量/高音/低音/震感/连接/错误 */
#define PY32_RSP_RHYTHM_LEN     11U  /* band0~9 + 播放状态 */
#define PY32_RSP_DATA_LEN       (PY32_RSP_STATE_LEN + PY32_RSP_RHYTHM_LEN) /* 17 */

#ifndef CONFIG_SYSTEM_APP_PY32_UART_STACK_SIZE
#define CONFIG_SYSTEM_APP_PY32_UART_STACK_SIZE 2048
#endif

#define PY32_FRAME_MAX          128U
#define PY32_RX_RING_SIZE       256U
#define PY32_DMA_RX_BUF_SIZE    256U

#ifndef CONFIG_SYSTEM_APP_PY32_UART_DMA_RX_CHAN
#define CONFIG_SYSTEM_APP_PY32_UART_DMA_RX_CHAN 255
#endif
#ifndef CONFIG_SYSTEM_APP_PY32_UART_DMA_RX_TIMEOUT_US
#define CONFIG_SYSTEM_APP_PY32_UART_DMA_RX_TIMEOUT_US 500
#endif

#define PY32_ERR_NONE           0x00U
#define PY32_ERR_UNSUPPORTED    0x01U
#define PY32_ERR_BAD_LEN        0x02U
#define PY32_ERR_BAD_VALUE      0x03U
#define PY32_ERR_CRC            0x04U
#define PY32_ERR_BUSY           0x05U

#ifdef CONFIG_BT_MANAGER
#include <bt_manager.h>
#endif

#include "app_ui.h"
#include <ui_manager.h>
#include <desktop_manager.h>

extern bool bt_manager_power_on_setup;

static void py32_host_notify_ui(u32_t ui_event)
{
	ui_message_send_async(MAIN_VIEW, MSG_VIEW_PAINT, ui_event);
}

#ifndef PY32_HOST_IDLE_TIMEOUT_MS
#define PY32_HOST_IDLE_TIMEOUT_MS	10000
#endif

/* PY32 UART 有数据=开机；超时无数据=关机（BT 隐藏+断连，禁本地 TTS） */
static bool py32_host_alive;
static os_delayed_work py32_host_idle_work;

struct py32_params {
	uint8_t volume;
	uint8_t treble;
	uint8_t bass;
	uint8_t vibration;
	uint8_t last_error;
};

static struct py32_params py32_state = {
	.treble = 12U,
	.bass = 12U,
};

/* ---- 律动数据缓存（50ms 周期发送到 PY32）---- */
#define PY32_RHYTHM_BAND_NUM    10U
static uint8_t py32_rhythm_bands[PY32_RHYTHM_BAND_NUM];
static uint8_t py32_rhythm_playing;
static uint8_t py32_rhythm_dirty;  /* 有新数据待发送 */

static struct device *py32_uart_dev;
static struct k_thread py32_uart_tid;
K_THREAD_STACK_DEFINE(py32_uart_stack, CONFIG_SYSTEM_APP_PY32_UART_STACK_SIZE);

static u8_t py32_rx_ring[PY32_RX_RING_SIZE];
static volatile u16_t py32_rx_head;
static volatile u16_t py32_rx_tail;
static u8_t py32_dma_rx_buf[PY32_DMA_RX_BUF_SIZE] __aligned(4);
static K_SEM_DEFINE(py32_rx_wake, 0, 32);

enum {
	PY32_RX_ADDR = 0,
	PY32_RX_CMD,
	PY32_RX_LEN,
	PY32_RX_DATA,
	PY32_RX_CRC_L,
	PY32_RX_CRC_H,
};

static u8_t py32_parse_state;
static u8_t py32_frame_addr;
static u8_t py32_frame_cmd;
static u8_t py32_frame_len;
static u8_t py32_frame_data[PY32_FRAME_MAX];
static u8_t py32_frame_got;
static u16_t py32_crc_rx;
static int py32_tx_fifo_err = -1;
static int py32_dma_rx_err = -1;
static uint8_t py32_init_banner_done;
static os_delayed_work py32_uart_init_work;
static u32_t py32_tx_frame_count;

/* 去重：相同 PY32 上行帧只打印一次 */
static struct {
	uint8_t addr;
	uint8_t cmd;
	uint8_t len;
	uint8_t data[PY32_FRAME_MAX];
	uint16_t crc;
} py32_last_rx_log;

/* 去重：相同 BTM→PY32 下行帧只打印一次（律动变化时仍会打印） */
static uint8_t py32_last_tx_log[PY32_FRAME_MAX];
static size_t py32_last_tx_len;

bool system_app_py32_host_is_alive(void)
{
	return py32_host_alive;
}

static void py32_host_bt_enable(void)
{
#ifdef CONFIG_BT_MANAGER
	if (!bt_manager_is_inited()) {
		return;
	}

	/* 先回连上次手机；失败/超时或列表空 → 进配对（见 system_btmgr_config） */
	bt_manager_set_user_visual(false, false, false, 0);
	bt_manager_start_wait_connect();
	if (bt_manager_power_on_setup) {
		printk("[py32] bt enable: reconnect then pair on fail\n");
		bt_manager_powon_auto_reconnect(0);
	}
#endif
}

static void py32_host_bt_disable(void)
{
#ifdef CONFIG_BT_MANAGER
	if (!bt_manager_is_inited()) {
		return;
	}

	printk("[py32] bt disable: end pair, disconnect, hide\n");
	bt_manager_auto_reconnect_stop();
	bt_manager_end_pair_mode();
	bt_manager_br_disconnect_all_phone_device();
	bt_manager_end_wait_connect();
	bt_manager_set_user_visual(true, false, false, 0);
#endif
}

static void py32_host_power_on(void)
{
	printk("[py32] host on (uart active)\n");
	py32_host_alive = true;
	py32_host_bt_enable();
	py32_host_notify_ui(UI_EVENT_POWER_ON);
	if (desktop_manager_get_plugin_id() == DESKTOP_PLUGIN_ID_BR_MUSIC) {
		py32_host_notify_ui(UI_EVENT_ENTER_BTMUSIC);
	}
}

static void py32_host_power_off(void)
{
	printk("[py32] host off (idle %dms)\n", PY32_HOST_IDLE_TIMEOUT_MS);
	py32_host_alive = false;
	py32_host_bt_disable();
}

static void py32_host_idle_work_handler(os_work *work)
{
	ARG_UNUSED(work);

	if (py32_host_alive) {
		py32_host_power_off();
	}
}

static void py32_host_on_rx_activity(void)
{
	if (!py32_host_alive) {
		py32_host_power_on();
	}

	os_delayed_work_cancel(&py32_host_idle_work);
	os_delayed_work_submit(&py32_host_idle_work, PY32_HOST_IDLE_TIMEOUT_MS);
}
static u32_t py32_tx_byte_count;

/* 硬件 TX：走 UART1 外设（CPU poll 模式），GPIO21 MFP=0xe */

#if defined(CONFIG_SYSTEM_APP_PY32_UART_RX_DEBUG)
static volatile u32_t py32_rx_dma_hf_bytes;
static volatile u32_t py32_rx_dma_tc_bytes;
static volatile u32_t py32_rx_dma_idle_bytes;
static volatile u32_t py32_rx_ring_drops;
static u32_t py32_rx_thread_bytes;
static u32_t py32_rx_last_activity_ms;

static const char *py32_parse_state_name(u8_t state)
{
	switch (state) {
	case PY32_RX_CMD:
		return "CMD";
	case PY32_RX_LEN:
		return "LEN";
	case PY32_RX_DATA:
		return "DATA";
	case PY32_RX_CRC_L:
		return "CRC_L";
	case PY32_RX_CRC_H:
		return "CRC_H";
	default:
		return "?";
	}
}

static void py32_log_rx_idle(void)
{
	u16_t ring_used;
	unsigned int key = irq_lock();
	uint32_t rx_ctl = sys_read32(GPIO_CTL(CONFIG_BOARD_UART1_RX_GPIO));

	if (py32_rx_head >= py32_rx_tail) {
		ring_used = py32_rx_head - py32_rx_tail;
	} else {
		ring_used = (u16_t)(PY32_RX_RING_SIZE - py32_rx_tail + py32_rx_head);
	}
	irq_unlock(key);

	printk("[py32] RX idle 5s: dma_hf=%u dma_tc=%u dma_idle=%u thread=%u "
	       "drops=%u ring=%u rx_ctl=0x%x gpio%d tx_fifo=%d dma_rx=%d tx_frames=%u\n",
	       (unsigned)py32_rx_dma_hf_bytes, (unsigned)py32_rx_dma_tc_bytes,
	       (unsigned)py32_rx_dma_idle_bytes, (unsigned)py32_rx_thread_bytes,
	       (unsigned)py32_rx_ring_drops, (unsigned)ring_used, rx_ctl,
	       CONFIG_BOARD_UART1_RX_GPIO, py32_tx_fifo_err, py32_dma_rx_err,
	       (unsigned)py32_tx_frame_count);
}
#endif /* CONFIG_SYSTEM_APP_PY32_UART_RX_DEBUG */

#if defined(CONFIG_CODEC_ACM8635)
#define PY32_UART1_TX_MFP	BOARD_285L2_UART1_TX_MFP
#define PY32_UART1_RX_MFP	BOARD_285L2_UART1_RX_MFP
#define PY32_UART1_TX_PIN_CTL	BOARD_285L2_UART1_TX_PIN_CTL
#define PY32_UART1_RX_PIN_CTL	BOARD_285L2_UART1_RX_PIN_CTL
#else
#define PY32_UART1_TX_MFP	0xeU
#define PY32_UART1_RX_MFP	0xeU
#define PY32_UART1_TX_PIN_CTL \
	(PY32_UART1_TX_MFP | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(0) | GPIO_CTL_PULLUP)
#define PY32_UART1_RX_PIN_CTL \
	(PY32_UART1_RX_MFP | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(0) | GPIO_CTL_PULLUP)
#endif

#define PY32_UART1_TX_PIN_HW \
	(PY32_UART1_TX_MFP | GPIO_CTL_GPIO_OUTEN | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3) | GPIO_CTL_PULLUP)
#define PY32_UART1_RX_PIN_HW \
	(PY32_UART1_RX_MFP | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3) | GPIO_CTL_PULLUP)

static void py32_log_hw_diag(void)
{
	unsigned int tx_mfp;
	unsigned int rx_mfp;

	if (!py32_uart_dev) {
		printk("[py32] hw_diag: uart dev not bound\n");
		return;
	}

	tx_mfp = sys_read32(GPIO_CTL(CONFIG_BOARD_UART1_TX_GPIO));
	rx_mfp = sys_read32(GPIO_CTL(CONFIG_BOARD_UART1_RX_GPIO));

	printk("[py32] hw_diag pinmux TXgpio%d=0x%08x RXgpio%d=0x%08x "
	       "tx_fifo=%d dma_rx=%d tx_frames=%u tx_bytes=%u\n",
	       CONFIG_BOARD_UART1_TX_GPIO, tx_mfp,
	       CONFIG_BOARD_UART1_RX_GPIO, rx_mfp,
	       py32_tx_fifo_err, py32_dma_rx_err, (unsigned)py32_tx_frame_count,
	       (unsigned)py32_tx_byte_count);

	if ((tx_mfp & GPIO_CTL_MFP_MASK) != PY32_UART1_TX_MFP) {
		printk("[py32] hw_diag WARN: TX mfp=0x%x (expect 0x%x, board.c)\n",
		       tx_mfp & GPIO_CTL_MFP_MASK, (unsigned)PY32_UART1_TX_MFP);
	}
	if (tx_mfp != PY32_UART1_TX_PIN_HW && tx_mfp != PY32_UART1_TX_PIN_CTL) {
		printk("[py32] hw_diag WARN: TX ctl=0x%x expect=0x%x (board.c)\n",
		       tx_mfp, (unsigned)PY32_UART1_TX_PIN_HW);
	}
	if ((rx_mfp & GPIO_CTL_MFP_MASK) != PY32_UART1_RX_MFP) {
		printk("[py32] hw_diag WARN: RX mfp=0x%x (expect 0x%x, board.c)\n",
		       rx_mfp & GPIO_CTL_MFP_MASK, (unsigned)PY32_UART1_RX_MFP);
	}
	if (rx_mfp != PY32_UART1_RX_PIN_HW && rx_mfp != PY32_UART1_RX_PIN_CTL) {
		printk("[py32] hw_diag WARN: RX ctl=0x%x expect=0x%x (board.c)\n",
		       rx_mfp, (unsigned)PY32_UART1_RX_PIN_HW);
	}
}

static void py32_log_init_banner(void)
{
	if (py32_init_banner_done) {
		return;
	}
	py32_init_banner_done = 1;

	printk("[py32] init OK role=slave TX=GPIO%d(HW-UART1) RX=GPIO%d UART1 DMA-RX\n",
	       CONFIG_BOARD_UART1_TX_GPIO, CONFIG_BOARD_UART1_RX_GPIO);
	py32_log_hw_diag();
}

static void py32_rx_ring_push(const uint8_t *data, uint32_t len, const char *via)
{
	unsigned int key;
	uint32_t i;
	uint32_t pushed = 0U;

	ARG_UNUSED(via);

	key = irq_lock();
	for (i = 0; i < len; i++) {
		u16_t next = (u16_t)((py32_rx_head + 1U) % PY32_RX_RING_SIZE);

		if (next == py32_rx_tail) {
#if defined(CONFIG_SYSTEM_APP_PY32_UART_RX_DEBUG)
			py32_rx_ring_drops++;
#endif
			break;
		}
		py32_rx_ring[py32_rx_head] = data[i];
		py32_rx_head = next;
		pushed++;
	}
	irq_unlock(key);

	if (pushed > 0U) {
		/* 仅唤醒线程：host on/off / BT 不可在 ISR 中调用 */
		k_sem_give(&py32_rx_wake);
	}
}

static void py32_dma_rx_isr(struct device *dev, u32_t priv_data, int reason)
{
	uint8_t *data;
	uint32_t half = PY32_DMA_RX_BUF_SIZE / 2U;

	ARG_UNUSED(dev);
	ARG_UNUSED(priv_data);

	if (reason == DMA_IRQ_HF) {
		data = py32_dma_rx_buf;
#if defined(CONFIG_SYSTEM_APP_PY32_UART_RX_DEBUG)
		py32_rx_dma_hf_bytes += half;
#endif
	} else {
		data = py32_dma_rx_buf + half;
#if defined(CONFIG_SYSTEM_APP_PY32_UART_RX_DEBUG)
		py32_rx_dma_tc_bytes += half;
#endif
	}

	py32_rx_ring_push(data, half, "dma");
}

#if defined(CONFIG_UART_DMA_RX_TIMEOUT_DRIVEN)
static void py32_dma_rx_timeout_handle(void *priv_data, uint8_t *data, uint32_t len)
{
	ARG_UNUSED(priv_data);

#if defined(CONFIG_SYSTEM_APP_PY32_UART_RX_DEBUG)
	py32_rx_dma_idle_bytes += len;
#endif
	py32_rx_ring_push(data, len, "idle");
}
#endif

/* ==== 硬件 UART1 TX (GPIO21, MFP=0xe, CPU poll 模式) ==== */

static struct acts_uart_controller *py32_uart1_regs(void)
{
	return (struct acts_uart_controller *)UART1_REG_BASE;
}

static void py32_uart1_putc_hw(uint8_t c);
static void py32_uart1_tx_wait_idle(void);

static void py32_uart1_reapply_tx_pinmux(void)
{
	sys_write32(PY32_UART1_TX_PIN_HW, GPIO_CTL(CONFIG_BOARD_UART1_TX_GPIO));
}

static void py32_uart1_force_cpu_tx(void)
{
	py32_tx_fifo_err = uart_fifo_switch(py32_uart_dev, 1, UART_FIFO_CPU);
	uart_irq_tx_disable(py32_uart_dev);
}

#if defined(CONFIG_SYSTEM_APP_PY32_UART_LB_TEST)
static void py32_uart1_loopback_test(void)
{
	struct acts_uart_controller *uart = py32_uart1_regs();
	uint32_t ctrl_saved;
	uint8_t got = 0;
	int ok = 0;

	py32_uart1_reapply_tx_pinmux();
	uart_fifo_switch(py32_uart_dev, 0, UART_FIFO_CPU);
	py32_uart1_force_cpu_tx();

	ctrl_saved = uart->ctrl;
	uart->ctrl = ctrl_saved | UART_CTL_LB_EN;
	py32_uart1_putc_hw(0x55);
	py32_uart1_tx_wait_idle();
	k_busy_wait(200);

	if (!(uart->stat & UART_STA_RFEM)) {
		got = (uint8_t)uart->rxdat;
		ok = (got == 0x55);
		uart->stat = UART_STA_RIP;
	}

	uart->ctrl = ctrl_saved & ~UART_CTL_LB_EN;

	printk("[py32] LB test tx=0x55 rx=0x%02x %s stat=0x%x\n",
	       (unsigned)got, ok ? "OK" : "FAIL", uart->stat);
}
#endif

static void py32_log_uart1_tx_regs(const char *tag)
{
	struct acts_uart_controller *uart = py32_uart1_regs();
	uint32_t ctrl = uart->ctrl;
	uint32_t stat = uart->stat;

	printk("[py32] uart1 %s ctrl=0x%x stat=0x%x tx_dst=%u rx_src=%u "
	       "tx_en=%u rx_en=%u utbb=%u tfes=%u tffu=%u tip=%u\n",
	       tag, ctrl, stat,
	       (unsigned)((ctrl >> UART_CTL_TX_DST_SHIFT) & 0x3U),
	       (unsigned)((ctrl >> UART_CTL_RX_SRC_SHIFT) & 0x3U),
	       (unsigned)((ctrl >> 30) & 0x1U), (unsigned)((ctrl >> 31) & 0x1U),
	       (unsigned)((stat >> 21) & 0x1U), (unsigned)((stat >> 10) & 0x1U),
	       (unsigned)((stat >> 6) & 0x1U), (unsigned)((stat >> 1) & 0x1U));
}

static void py32_uart1_putc_hw(uint8_t c)
{
	struct acts_uart_controller *uart = py32_uart1_regs();

	while (uart->stat & UART_STA_UTBB) {
		;
	}
	uart->txdat = c;
}

static void py32_uart1_tx_wait_idle(void)
{
	struct acts_uart_controller *uart = py32_uart1_regs();
	unsigned int wait_ms = 0U;

	while (uart->stat & UART_STA_UTBB) {
		k_sleep(K_MSEC(1));
		wait_ms++;
		if (wait_ms > 30U) {
			printk("[py32] TX UTBB timeout\n");
			py32_log_uart1_tx_regs("utbb");
			break;
		}
	}
}

static int py32_uart_dma_setup(void)
{
	int ret;

	/* 硬件 UART1 TX: 配置 GPIO21 pinmux + 强制 CPU poll 模式 */
	py32_uart1_reapply_tx_pinmux();
	py32_uart1_force_cpu_tx();

	/* RX 保持硬件 UART DMA */
	py32_dma_rx_err = uart_fifo_switch(py32_uart_dev, 0, UART_FIFO_DMA);
	uart_irq_tx_disable(py32_uart_dev);

	ret = uart_dma_recv_init(py32_uart_dev,
				 CONFIG_SYSTEM_APP_PY32_UART_DMA_RX_CHAN,
				 py32_dma_rx_isr, NULL);
	if (ret != 0) {
		printk("[py32] uart_dma_recv_init failed %d\n", ret);
		return ret;
	}

	uart_dma_recv_config(py32_uart_dev, py32_dma_rx_buf, PY32_DMA_RX_BUF_SIZE);

#if defined(CONFIG_UART_DMA_RX_TIMEOUT_DRIVEN)
	uart_dma_recv_set_timeout_start(py32_uart_dev,
					CONFIG_SYSTEM_APP_PY32_UART_DMA_RX_TIMEOUT_US,
					py32_dma_rx_timeout_handle, NULL);
#endif

	ret = uart_dma_recv_start(py32_uart_dev);
	if (ret != 0) {
		printk("[py32] uart_dma_recv_start failed %d\n", ret);
		return ret;
	}

	printk("[py32] DMA setup RXchan=%d TX=HW-UART1-GPIO%d fifo_rx=%d "
	       "buf=%u timeout=%dus\n",
	       CONFIG_SYSTEM_APP_PY32_UART_DMA_RX_CHAN,
	       CONFIG_BOARD_UART1_TX_GPIO, py32_dma_rx_err,
	       (unsigned)PY32_DMA_RX_BUF_SIZE,
	       CONFIG_SYSTEM_APP_PY32_UART_DMA_RX_TIMEOUT_US);
	return 0;
}

/** BTM→PY32 完整帧十六进制；内容不变则跳过（避免 50ms 空律动刷屏） */
static void py32_print_tx_frame(const uint8_t *data, size_t len)
{
	size_t i;

	if (!data || len == 0U || len > PY32_FRAME_MAX) {
		return;
	}

	if (len == py32_last_tx_len &&
	    memcmp(py32_last_tx_log, data, len) == 0) {
		return;
	}

	memcpy(py32_last_tx_log, data, len);
	py32_last_tx_len = len;

	/* 与 RX 同格式，前缀 TX 区分方向：Addr CMD LEN DATA CRC16(LE) */
	printk("[py32] TX");
	for (i = 0U; i < len; i++) {
		printk(" %02x", (unsigned)data[i]);
	}
	printk("\n");
}

/** 硬件 UART1 TX：逐字节写入 FIFO，发完后等待空闲 */
static void py32_uart_tx_bytes(const uint8_t *data, size_t len)
{
	size_t i;

	if (!data || len == 0U || len > PY32_FRAME_MAX) {
		return;
	}

	py32_print_tx_frame(data, len);

	for (i = 0U; i < len; i++) {
		py32_uart1_putc_hw(data[i]);
	}
	py32_uart1_tx_wait_idle();

	py32_tx_frame_count++;
	py32_tx_byte_count += (u32_t)len;
}

#if defined(CONFIG_SYSTEM_APP_PY32_UART_HW_TX_TEST)
/* ==== HW TX test（仅当 Kconfig 开启时编译）==== */
static void py32_uart_hw_tx_test_send(void)
{
	static const uint8_t pat[] = { 0x55, 0xAA, 'H', 'W', '1', 'T', 'X', '\n' };

	py32_uart_tx_bytes(pat, sizeof(pat));
	printk("[py32] HW TX test %uB on GPIO%d (UART1 HW poll)\n",
	       (unsigned)sizeof(pat), CONFIG_BOARD_UART1_TX_GPIO);
}
#endif /* CONFIG_SYSTEM_APP_PY32_UART_HW_TX_TEST */

/** 协议 3.5 节 CRC16（反射 0xA001，初值 0xFFFF） */
static uint16_t py32_crc16(const uint8_t *data, uint16_t length)
{
	uint16_t crc = 0xFFFFU;
	uint16_t i;
	uint8_t j;

	for (i = 0; i < length; i++) {
		crc ^= data[i];
		for (j = 0; j < 8U; j++) {
			if (crc & 0x0001U) {
				crc = (uint16_t)((crc >> 1) ^ 0xA001U);
			} else {
				crc >>= 1;
			}
		}
	}
	return crc;
}

static void py32_parse_reset(void)
{
	py32_parse_state = PY32_RX_ADDR;
	py32_frame_addr = 0;
	py32_frame_cmd = 0;
	py32_frame_len = 0;
	py32_frame_got = 0;
	py32_crc_rx = 0;
}

/*
 * 滚轮 0~100% → 系统音量档：
 * 低区适当抬高（避免偏低时几乎听不见），高区放缓（避免越大跳变越猛）。
 * 锚点（max=16 时）：0/10/20/30/40/50/60/70/80/90/100%
 *                 → 0/ 3/ 6/ 8/10/11/12/13/14/15/ 16
 */
static int py32_volume_pct_to_level(uint8_t pct)
{
	static const uint8_t pct_anchor[] = {
		0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100
	};
	static const uint8_t level_anchor[] = {
		0, 3, 6, 8, 10, 11, 12, 13, 14, 15, 16
	};
	int max = audio_policy_get_volume_level();
	unsigned i;
	unsigned lo_pct, hi_pct, lo_lvl, hi_lvl;
	int level;

	if (max <= 0) {
		return 0;
	}
	if (pct > 100U) {
		pct = 100U;
	}

	for (i = 1; i < ARRAY_SIZE(pct_anchor); i++) {
		if (pct <= pct_anchor[i]) {
			break;
		}
	}
	if (i >= ARRAY_SIZE(pct_anchor)) {
		i = ARRAY_SIZE(pct_anchor) - 1U;
	}

	lo_pct = pct_anchor[i - 1U];
	hi_pct = pct_anchor[i];
	lo_lvl = level_anchor[i - 1U];
	hi_lvl = level_anchor[i];

	if (hi_pct == lo_pct) {
		level = (int)lo_lvl;
	} else {
		level = (int)(lo_lvl +
			      ((uint32_t)(pct - lo_pct) * (hi_lvl - lo_lvl) +
			       (hi_pct - lo_pct) / 2U) / (hi_pct - lo_pct));
	}

	/* 表按 max=16 设计；其它 max 时按比例缩放 */
	if (max != 16) {
		level = (level * max + 8) / 16;
	}
	if (level < 0) {
		level = 0;
	}
	if (level > max) {
		level = max;
	}
	return level;
}

static uint8_t py32_volume_level_to_pct(int level)
{
	static const uint8_t pct_anchor[] = {
		0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100
	};
	static const uint8_t level_anchor[] = {
		0, 3, 6, 8, 10, 11, 12, 13, 14, 15, 16
	};
	int max = audio_policy_get_volume_level();
	unsigned i;
	unsigned lo_pct, hi_pct, lo_lvl, hi_lvl;
	int mapped;

	if (level < 0) {
		level = 0;
	}
	if (max <= 0) {
		return 0U;
	}
	if (level > max) {
		level = max;
	}

	/* 先把当前 max 映射回 0~16 表 */
	if (max != 16) {
		mapped = (level * 16 + max / 2) / max;
	} else {
		mapped = level;
	}
	if (mapped > 16) {
		mapped = 16;
	}

	for (i = 1; i < ARRAY_SIZE(level_anchor); i++) {
		if ((unsigned)mapped <= level_anchor[i]) {
			break;
		}
	}
	if (i >= ARRAY_SIZE(level_anchor)) {
		i = ARRAY_SIZE(level_anchor) - 1U;
	}

	lo_pct = pct_anchor[i - 1U];
	hi_pct = pct_anchor[i];
	lo_lvl = level_anchor[i - 1U];
	hi_lvl = level_anchor[i];

	if (hi_lvl == lo_lvl) {
		return (uint8_t)lo_pct;
	}
	return (uint8_t)(lo_pct +
			 ((uint32_t)((unsigned)mapped - lo_lvl) *
			  (hi_pct - lo_pct) + (hi_lvl - lo_lvl) / 2U) /
			 (hi_lvl - lo_lvl));
}

static void py32_volume_log_map_table(void)
{
	static const uint8_t pcts[] = {
		0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100
	};
	int max = audio_policy_get_volume_level();
	unsigned i;

	printk("[py32] vol map table (max=%d):\n", max);
	for (i = 0; i < ARRAY_SIZE(pcts); i++) {
		printk("[py32]   %3u%% -> level %d/%d\n",
		       (unsigned)pcts[i],
		       py32_volume_pct_to_level(pcts[i]),
		       max);
	}
}

static void py32_apply_volume(uint8_t volume_pct)
{
	int max = audio_policy_get_volume_level();
	int level = py32_volume_pct_to_level(volume_pct);

	py32_state.volume = volume_pct;
	system_volume_set(AUDIO_STREAM_MUSIC, level, true);
	printk("[py32] vol map: pct=%u -> lvl=%d/%d (prev_pct_of_lvl=%u)\n",
	       (unsigned)volume_pct, level, max,
	       (unsigned)py32_volume_level_to_pct(level));
}

#define PY32_PEQ_IDX_BASS	15U
#define PY32_PEQ_IDX_TREBLE	16U

static void py32_push_dynamic_peq(void)
{
#ifdef CONFIG_AUDIO_SUPPORT_DYNAMIC_PEQ
	media_player_t *player = media_player_get_current_dumpable_player();

	if (player) {
		media_player_dynamic_update_peq(player, NULL, 0);
	}
#endif
}

static void py32_apply_treble_bass(uint8_t treble, uint8_t bass)
{
	if (treble > 24U || bass > 24U) {
		py32_state.last_error = PY32_ERR_BAD_VALUE;
		return;
	}

	py32_state.treble = treble;
	py32_state.bass = bass;

#ifdef CONFIG_AUDIO_SUPPORT_DYNAMIC_PEQ
	eq_band_t eq;
	int db;

	/* 低音 200Hz；0~24 映射 -12~+12 dB（0.1dB 步进） */
	db = (int)bass - 12;
	eq.cutoff = 200;
	eq.q = 70;
	eq.gain = (short)(db * 10);
	eq.type = 1;
	audio_policy_set_dynamic_peq_band(PY32_PEQ_IDX_BASS, &eq);

	/* 高音 8kHz；同上 */
	db = (int)treble - 12;
	eq.cutoff = 8000;
	eq.q = 70;
	eq.gain = (short)(db * 10);
	eq.type = 1;
	audio_policy_set_dynamic_peq_band(PY32_PEQ_IDX_TREBLE, &eq);

	py32_push_dynamic_peq();
#endif
	printk("[py32] EQ bass=%ddB treble=%ddB (lvl %u/%u)%s\n",
	       (int)bass - 12, (int)treble - 12,
	       (unsigned)bass, (unsigned)treble,
#ifdef CONFIG_AUDIO_SUPPORT_DYNAMIC_PEQ
	       ""
#else
	       " [PEQ off]"
#endif
	       );
}

void system_app_py32_reapply_eq(void)
{
	py32_apply_treble_bass(py32_state.treble, py32_state.bass);
}

static uint8_t py32_get_bt_connected(void)
{
#ifdef CONFIG_BT_MANAGER
	return (bt_manager_get_connected_dev_num() > 0) ? 1U : 0U;
#else
	return 0U;
#endif
}

static void py32_sync_status_for_response(void)
{
	int level = system_volume_get(AUDIO_STREAM_MUSIC);

	if (level >= 0) {
		py32_state.volume = py32_volume_level_to_pct(level);
	}
}

static void py32_tx_status_frame(uint8_t err_code)
{
	uint8_t frame[PY32_FRAME_MAX];
	uint8_t data[PY32_RSP_DATA_LEN];
	uint16_t crc;
	unsigned int key;
	uint8_t media_playing = 0U;

	if (err_code == PY32_ERR_NONE) {
		py32_sync_status_for_response();
	}

	/* 状态参数（前 6 字节） */
	data[0] = py32_state.volume;
	data[1] = py32_state.treble;
	data[2] = py32_state.bass;
	data[3] = py32_state.vibration;
	data[4] = py32_get_bt_connected();
	data[5] = err_code;

	/* DATA[16]：手机媒体播放状态（AVRCP），非本地 DSP/A2DP 出声 */
#ifdef CONFIG_BT_MANAGER
	media_playing =
		(bt_manager_media_get_status() == BT_STATUS_PLAYING) ? 1U : 0U;
#else
	media_playing = py32_rhythm_playing;
#endif

	/* 律动：播放中带频谱；暂停时频谱清零，避免灯带误律动 */
	key = irq_lock();
	if (media_playing) {
		memcpy(&data[PY32_RSP_STATE_LEN], py32_rhythm_bands,
		       PY32_RSP_RHYTHM_LEN - 1U);
	} else {
		memset(&data[PY32_RSP_STATE_LEN], 0, PY32_RSP_RHYTHM_LEN - 1U);
	}
	py32_rhythm_playing = media_playing;
	py32_rhythm_dirty = 0U;
	irq_unlock(key);
	data[PY32_RSP_DATA_LEN - 1U] = media_playing;

	/* [V1.1] 帧格式: Addr(0x02) + CMD(0x10) + LEN + DATA + CRC16 */
	frame[0] = PY32_ADDR_SLAVE;
	frame[1] = PY32_CMD_COMMON;
	frame[2] = PY32_RSP_DATA_LEN;
	memcpy(&frame[3], data, PY32_RSP_DATA_LEN);
	crc = py32_crc16(frame, 3U + PY32_RSP_DATA_LEN);
	frame[3 + PY32_RSP_DATA_LEN] = (uint8_t)(crc & 0xFFU);
	frame[4 + PY32_RSP_DATA_LEN] = (uint8_t)((crc >> 8) & 0xFFU);

	py32_uart_tx_bytes(frame, 5U + PY32_RSP_DATA_LEN);
}

static void py32_send_status_response(uint8_t err_code)
{
	py32_tx_status_frame(err_code);
}

void system_app_py32_uart_push_now(void)
{
	py32_send_status_response(PY32_ERR_NONE);
}

/* ---- 律动数据发送（50ms 周期 → PY32）---- */

/**
 * @brief 更新律动频谱，由 btmusic 音频线程调用
 * @param bands  10 段频谱能量值（0-255）
 * @param playing 是否正在播放（0=停止, 1=播放中）
 */
void py32_rhythm_set_data(const uint8_t *bands, uint8_t playing)
{
	unsigned int key;

	if (!bands) {
		return;
	}

	key = irq_lock();
	memcpy(py32_rhythm_bands, bands, PY32_RHYTHM_BAND_NUM);
	py32_rhythm_playing = playing ? 1U : 0U;
	py32_rhythm_dirty = 1U;
	irq_unlock(key);
}

static bool py32_rx_frame_same(uint8_t addr, uint8_t cmd,
			       const uint8_t *data, uint8_t len, uint16_t crc)
{
	if (py32_last_rx_log.addr != addr || py32_last_rx_log.cmd != cmd ||
	    py32_last_rx_log.len != len || py32_last_rx_log.crc != crc) {
		return false;
	}

	if (len == 0U) {
		return true;
	}

	return (data != NULL) &&
	       (memcmp(py32_last_rx_log.data, data, len) == 0);
}

static void py32_save_rx_log(uint8_t addr, uint8_t cmd,
			     const uint8_t *data, uint8_t len, uint16_t crc)
{
	py32_last_rx_log.addr = addr;
	py32_last_rx_log.cmd = cmd;
	py32_last_rx_log.len = len;
	py32_last_rx_log.crc = crc;
	if (len > 0U && data != NULL) {
		memcpy(py32_last_rx_log.data, data, len);
	}
}

static void py32_print_rx_frame(uint8_t addr, uint8_t cmd,
				const uint8_t *data, uint8_t len, uint16_t crc)
{
	unsigned int i;

	/* 完整 UART 帧：Addr CMD LEN DATA CRC16(LE) */
	printk("[py32] %02x %02x %02x",
	       (unsigned)addr, (unsigned)cmd, (unsigned)len);
	for (i = 0U; i < len; i++) {
		printk(" %02x", (unsigned)data[i]);
	}
	printk(" %02x %02x\n",
	       (unsigned)(crc & 0xffU), (unsigned)((crc >> 8) & 0xffU));
}

static void py32_handle_set_params(const uint8_t *data, uint8_t len)
{
	uint8_t err = PY32_ERR_NONE;

	if (!data || len != PY32_SET_DATA_LEN) {
		py32_state.last_error = PY32_ERR_BAD_LEN;
		py32_send_status_response(PY32_ERR_BAD_LEN);
		return;
	}

	if (data[0] > 100U || data[1] > 24U || data[2] > 24U || data[3] > 100U) {
		err = PY32_ERR_BAD_VALUE;
	} else {
		py32_apply_volume(data[0]);
		py32_apply_treble_bass(data[1], data[2]);
		py32_state.vibration = data[3];
	}

	/* TWS 开关预留：DATA 字节 4（需同步 PY32 发送端和协议文档） */
#if 0 /* 预留 */
#if defined(CONFIG_APP_TWS)
	app_tws_set_enable(data[4] != 0U);
#endif
#endif

	py32_state.last_error = err;
	py32_send_status_response(err);
}

static void py32_handle_frame(uint8_t addr, uint8_t cmd, const uint8_t *data, uint8_t len)
{
	if (addr != PY32_ADDR_HOST) {
		printk("[py32] unknown addr 0x%02x\n", (unsigned)addr);
		return;
	}

	switch (cmd) {
	case PY32_CMD_COMMON:
		py32_handle_set_params(data, len);
		break;
	default:
		py32_state.last_error = PY32_ERR_UNSUPPORTED;
		py32_send_status_response(PY32_ERR_UNSUPPORTED);
		break;
	}
}

static void py32_process_complete_frame(void)
{
	uint8_t hdr[PY32_FRAME_MAX];
	uint16_t crc_calc;

	if (py32_frame_len > (PY32_FRAME_MAX - 5U)) {
		printk("[py32] frame len overflow %u\n", (unsigned)py32_frame_len);
		py32_send_status_response(PY32_ERR_BAD_LEN);
		py32_parse_reset();
		return;
	}

	/* [V1.1] CRC 覆盖: Addr + CMD + LEN + DATA */
	hdr[0] = py32_frame_addr;
	hdr[1] = py32_frame_cmd;
	hdr[2] = py32_frame_len;
	memcpy(&hdr[3], py32_frame_data, py32_frame_len);

	crc_calc = py32_crc16(hdr, (uint16_t)(3U + py32_frame_len));
	if (crc_calc != py32_crc_rx) {
		printk("[py32] CRC fail calc=0x%04x rx=0x%04x addr=0x%02x cmd=0x%02x len=%u\n",
		       crc_calc, py32_crc_rx, (unsigned)py32_frame_addr,
		       (unsigned)py32_frame_cmd, (unsigned)py32_frame_len);
		py32_state.last_error = PY32_ERR_CRC;
		py32_send_status_response(PY32_ERR_CRC);
		py32_parse_reset();
		return;
	}

	if (py32_rx_frame_same(py32_frame_addr, py32_frame_cmd,
			       py32_frame_data, py32_frame_len, py32_crc_rx)) {
		py32_parse_reset();
		return;
	}

	py32_save_rx_log(py32_frame_addr, py32_frame_cmd,
			 py32_frame_data, py32_frame_len, py32_crc_rx);
	py32_print_rx_frame(py32_frame_addr, py32_frame_cmd,
			    py32_frame_data, py32_frame_len, py32_crc_rx);
	py32_handle_frame(py32_frame_addr, py32_frame_cmd,
			   py32_frame_data, py32_frame_len);
	py32_parse_reset();
}

static void py32_rx_feed_byte(u8_t c)
{
	switch (py32_parse_state) {
	case PY32_RX_ADDR:
		/* 只接受主机地址 0x01：避免误同步到数据中的随机字节 */
		if (c != PY32_ADDR_HOST) {
#if defined(CONFIG_SYSTEM_APP_PY32_UART_RX_DEBUG)
			printk("[py32] discard unexpected addr 0x%02x\n",
			       (unsigned)c);
#endif
			break;
		}
		py32_frame_addr = c;
		py32_parse_state = PY32_RX_CMD;
		break;
	case PY32_RX_CMD:
		py32_frame_cmd = c;
		py32_parse_state = PY32_RX_LEN;
		break;
	case PY32_RX_LEN:
		py32_frame_len = c;
		py32_frame_got = 0;
		if (py32_frame_len == 0U) {
			py32_parse_state = PY32_RX_CRC_L;
		} else if (py32_frame_len > (PY32_FRAME_MAX - 5U)) {
			printk("[py32] bad LEN %u\n", (unsigned)c);
			py32_parse_reset();
		} else {
			py32_parse_state = PY32_RX_DATA;
		}
		break;
	case PY32_RX_DATA:
		/* 数据中途收到主机地址 0x01：判定上一帧被截断，重新同步 */
		if (c == PY32_ADDR_HOST && py32_frame_got < py32_frame_len) {
#if defined(CONFIG_SYSTEM_APP_PY32_UART_RX_DEBUG)
			printk("[py32] resync on addr 0x01 (got %u/%u)\n",
			       (unsigned)py32_frame_got,
			       (unsigned)py32_frame_len);
#endif
			py32_parse_reset();
			py32_rx_feed_byte(c);
			break;
		}
		py32_frame_data[py32_frame_got++] = c;
		if (py32_frame_got >= py32_frame_len) {
			py32_parse_state = PY32_RX_CRC_L;
		}
		break;
	case PY32_RX_CRC_L:
		py32_crc_rx = c;
		py32_parse_state = PY32_RX_CRC_H;
		break;
	case PY32_RX_CRC_H:
		py32_crc_rx |= (uint16_t)((uint16_t)c << 8);
		py32_process_complete_frame();
		break;
	default:
		py32_parse_reset();
		break;
	}
}

static int py32_uart_feed_rx_byte(u8_t c, const char *via)
{
#if defined(CONFIG_SYSTEM_APP_PY32_UART_RX_DEBUG)
	py32_rx_thread_bytes++;
	py32_rx_last_activity_ms = k_uptime_get_32();
	printk("[py32] RX byte 0x%02x via=%s parse=%s\n",
	       (unsigned)c, via, py32_parse_state_name(py32_parse_state));
#else
	ARG_UNUSED(via);
#endif
	py32_rx_feed_byte(c);
	return 1;
}

static int py32_uart_ring_drain(void)
{
	u8_t c;
	unsigned int key;
	int n = 0;

	for (;;) {
		key = irq_lock();
		if (py32_rx_tail == py32_rx_head) {
			irq_unlock(key);
			break;
		}
		c = py32_rx_ring[py32_rx_tail];
		py32_rx_tail = (u16_t)((py32_rx_tail + 1U) % PY32_RX_RING_SIZE);
		irq_unlock(key);

		py32_uart_feed_rx_byte(c, "thread");
		n++;
	}
	return n;
}

static void py32_uart_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	printk("[py32] thread started dev=%s baud=%d DMA-RX poll-TX "
	       "(init delayed %dms for UART0 console)\n",
	       CONFIG_SYSTEM_APP_PY32_UART_DEV_NAME,
	       CONFIG_UART_ACTS_PORT_1_BAUD_RATE,
	       CONFIG_SYSTEM_APP_PY32_UART_INIT_DELAY_MS);
#if defined(CONFIG_SYSTEM_APP_PY32_UART_RX_DEBUG)
	printk("[py32] RX debug ON: DMA+thread on UART1 GPIO%d, 5s idle heartbeat\n",
	       CONFIG_BOARD_UART1_RX_GPIO);
	py32_rx_last_activity_ms = k_uptime_get_32();
#endif

	py32_log_init_banner();

#if defined(CONFIG_SYSTEM_APP_PY32_UART_HW_TX_TEST)
	py32_uart_hw_tx_test_send();
#endif

	for (;;) {
		(void)k_sem_take(&py32_rx_wake, K_MSEC(10));
		if (py32_uart_ring_drain() > 0) {
			/* 线程上下文处理开/关机，避免 ISR 调 BT/work 崩溃 */
			py32_host_on_rx_activity();
		}

		/* 状态+律动：每 50ms 发一帧（Addr=0x02 CMD=0x10 LEN=17） */
		{
			static u32_t last_rhythm_tx_ms;
			u32_t now = k_uptime_get_32();

			if (py32_init_banner_done && py32_host_alive &&
			    (now - last_rhythm_tx_ms) >= 50U) {
				py32_tx_status_frame(PY32_ERR_NONE);
				last_rhythm_tx_ms = now;
			}
		}

#if defined(CONFIG_SYSTEM_APP_PY32_UART_HW_TX_TEST)
		{
			static u32_t last_hw_tx_ms;
			u32_t now = k_uptime_get_32();

			if ((now - last_hw_tx_ms) >=
			    (u32_t)CONFIG_SYSTEM_APP_PY32_UART_HW_TX_TEST_MS) {
				py32_uart_hw_tx_test_send();
				last_hw_tx_ms = now;
			}
		}
#endif

#if defined(CONFIG_SYSTEM_APP_PY32_UART_RX_DEBUG)
		{
			u32_t now = k_uptime_get_32();

			if ((now - py32_rx_last_activity_ms) >= 5000U) {
				py32_log_rx_idle();
				py32_rx_last_activity_ms = now;
			}
		}
#endif
	}
}

static void py32_uart_do_init(void)
{
	printk("[py32] init start (UART0 console ready)\n");

	memset(&py32_state, 0, sizeof(py32_state));
	py32_state.treble = 12U;
	py32_state.bass = 12U;
	py32_parse_reset();

	py32_uart_dev = device_get_binding(CONFIG_SYSTEM_APP_PY32_UART_DEV_NAME);
	if (!py32_uart_dev) {
		printk("[py32] bind %s failed\n", CONFIG_SYSTEM_APP_PY32_UART_DEV_NAME);
		return;
	}

	py32_rx_head = 0;
	py32_rx_tail = 0;

#if defined(CONFIG_SYSTEM_APP_PY32_UART_LB_TEST)
	py32_uart1_loopback_test();
#endif

	if (py32_uart_dma_setup() != 0) {
		printk("[py32] DMA setup failed\n");
		return;
	}

	printk("[py32] UART1 ready TX=GPIO%d RX=GPIO%d\n",
	       CONFIG_BOARD_UART1_TX_GPIO, CONFIG_BOARD_UART1_RX_GPIO);
	py32_volume_log_map_table();

	k_thread_create(&py32_uart_tid, py32_uart_stack,
			K_THREAD_STACK_SIZEOF(py32_uart_stack),
			py32_uart_thread, NULL, NULL, NULL,
			CONFIG_SYSTEM_APP_PY32_UART_PRIO, 0, K_NO_WAIT);
}

static void py32_uart_delayed_init_handler(os_work *work)
{
	ARG_UNUSED(work);

	py32_uart_do_init();
}

void system_app_py32_uart_init(void)
{
	os_delayed_work_init(&py32_host_idle_work, py32_host_idle_work_handler);
	os_delayed_work_init(&py32_uart_init_work, py32_uart_delayed_init_handler);
	os_delayed_work_submit(&py32_uart_init_work,
			       CONFIG_SYSTEM_APP_PY32_UART_INIT_DELAY_MS);
}

#endif /* CONFIG_SYSTEM_APP_PY32_UART */
