/*
 * Copyright (c) 2026 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file btmusic_ws2812.c
 *
 * @brief GPIO8 WS2812 氛围灯（GPIO bitbang）。
 *
 * Strip1（GPIO8）：整带同色氛围灯，8 色循环（10s 切换）。
 * Strip2 律动灯已移到 btmusic_ws2812_rhythm.c（外部 MCU）。
 */

#include "btmusic_ws2812.h"

#if defined(CONFIG_BT_MUSIC_LED_STRIP) || defined(CONFIG_BT_MUSIC_LED_STRIP2)

#if defined(CONFIG_BT_MUSIC_LED_STRIP)
#define WS2812_HAS_STRIP1  1
#else
#define WS2812_HAS_STRIP1  0
#endif

#define SYS_LOG_DOMAIN "ws2812"
#include <logging/sys_log.h>

#include <string.h>
#include <kernel.h>
#include <soc.h>
#include <irq.h>
#include <thread_timer.h>

/* === 全局变量（不受 #if 影响）=== */
static uint8_t ws2812_inited;
static uint32_t ws2812_show_cnt;


#if WS2812_HAS_STRIP1
#ifndef CONFIG_BT_MUSIC_LED_STRIP_GPIO
#ifdef BOARD_285L2_WS2812_AMBIENT_GPIO
#define CONFIG_BT_MUSIC_LED_STRIP_GPIO BOARD_285L2_WS2812_AMBIENT_GPIO
#else
#define CONFIG_BT_MUSIC_LED_STRIP_GPIO 8
#endif
#endif
#ifndef CONFIG_BT_MUSIC_LED_STRIP_COUNT
#define CONFIG_BT_MUSIC_LED_STRIP_COUNT 25
#endif
#define WS2812_LED_COUNT   CONFIG_BT_MUSIC_LED_STRIP_COUNT
#define WS2812_GPIO_PIN    CONFIG_BT_MUSIC_LED_STRIP_GPIO
#endif

/* GPIO bitbang 是否需要编译：strip1 存在 */
#define WS2812_NEED_GPIO  WS2812_HAS_STRIP1

/* AMBIC1010 / WS2812 时序：码周期 1.2µs，T0H/T1L=300ns，T0L/T1H=900ns，RESET>280µs */
#define WS2812_T0H_NS      300U
#define WS2812_T0L_NS      900U
#define WS2812_T1H_NS      900U
#define WS2812_T1L_NS      300U
#define WS2812_RESET_US    500U

#define WS2812_NS_TO_CYCLES(ns) \
	((u32_t)((u64_t)CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC * (u64_t)(ns) \
		 / 1000000000ULL))



#if WS2812_NEED_GPIO
static u32_t ws2812_nop_per_cycle;
static u32_t ws2812_nop_t0h;
static u32_t ws2812_nop_t0l;
static u32_t ws2812_nop_t1h;
static u32_t ws2812_nop_t1l;
#endif

struct ws2812_timing {
	u32_t t0h;
	u32_t t0l;
	u32_t t1h;
	u32_t t1l;
};

#if WS2812_HAS_STRIP1
/* 多分配 1 颗灯珠（3 字节）作为末尾填充，全零确保末灯蓝通道不被截断 */
static uint8_t ws2812_grb[WS2812_LED_COUNT * 3U + 3U];
#endif
#if WS2812_NEED_GPIO
static void ws2812_fill_timing(struct ws2812_timing *tm)
{
	tm->t0h = ws2812_nop_t0h;
	tm->t0l = ws2812_nop_t0l;
	tm->t1h = ws2812_nop_t1h;
	tm->t1l = ws2812_nop_t1l;
}
#endif

#if WS2812_NEED_GPIO
static void ws2812_gpio_init_pin(u32_t pin);
static void ws2812_gpio_ensure_pin(u32_t pin);
static void ws2812_flush_pin(u32_t pin, const uint8_t *grb, size_t len,
			     u32_t reset_us, const struct ws2812_timing *tm);
#endif
static int ws2812_order_test_active(void);

static void ws2812_nop_burst(u32_t n)
{
	while (n--) {
		__asm__ volatile ("nop");
	}
}

static u32_t ws2812_measure_nops(u32_t n)
{
	u32_t t0 = k_cycle_get_32();
	u32_t t1;

	ws2812_nop_burst(n);
	t1 = k_cycle_get_32();
	return t1 - t0;
}

static u32_t ws2812_cycles_to_nops(u32_t cycles)
{
	u32_t n;

	if (cycles == 0U || ws2812_nop_per_cycle == 0U) {
		return 0U;
	}

	n = (cycles + ws2812_nop_per_cycle - 1U) / ws2812_nop_per_cycle;
	return n ? n : 1U;
}

/* 播放中 CPU 频率会变，每帧快速重算 NOP 时序（无 2ms 等待） */
static void ws2812_refresh_timing(void)
{
	u32_t c100;
	u32_t c200;

	c100 = ws2812_measure_nops(100U);
	c200 = ws2812_measure_nops(200U);
	if (c200 > c100) {
		ws2812_nop_per_cycle = (c200 - c100) / 100U;
	} else {
		ws2812_nop_per_cycle = 1U;
	}
	if (ws2812_nop_per_cycle == 0U) {
		ws2812_nop_per_cycle = 1U;
	}

	ws2812_nop_t0h = ws2812_cycles_to_nops(WS2812_NS_TO_CYCLES(WS2812_T0H_NS));
	ws2812_nop_t1h = ws2812_cycles_to_nops(WS2812_NS_TO_CYCLES(WS2812_T1H_NS));
	ws2812_nop_t0l = ws2812_cycles_to_nops(WS2812_NS_TO_CYCLES(WS2812_T0L_NS));
	ws2812_nop_t1l = ws2812_cycles_to_nops(WS2812_NS_TO_CYCLES(WS2812_T1L_NS));
	/* 减去 GPIO 寄存器写入开销（约 2 个 NOP 周期） */
	if (ws2812_nop_t0h > 1U) {
		ws2812_nop_t0h -= 1U;
	}
	if (ws2812_nop_t1h > 2U) {
		ws2812_nop_t1h -= 2U;
	}
	if (ws2812_nop_t0l < ws2812_nop_t0h) {
		ws2812_nop_t0l = ws2812_nop_t0h;
	}
	if (ws2812_nop_t1l < ws2812_nop_t1h) {
		ws2812_nop_t1l = ws2812_nop_t1h;
	}
}

static void ws2812_calibrate_internal(int log_result)
{
	k_busy_wait(2000);
	ws2812_refresh_timing();

	if (!log_result) {
		return;
	}

#if WS2812_HAS_STRIP1
	SYS_LOG_INF("gpio=%d leds=%d clk=%u nop/cyc=%u t0h=%u t0l=%u t1h=%u t1l=%u\n",
		    WS2812_GPIO_PIN, WS2812_LED_COUNT,
		    (unsigned)CONFIG_SYS_CLOCK_HW_CYCLES_PER_SEC,
		    ws2812_nop_per_cycle,
		    ws2812_nop_t0h, ws2812_nop_t0l,
		    ws2812_nop_t1h, ws2812_nop_t1l);
#endif
}

static void ws2812_calibrate(void)
{
	ws2812_calibrate_internal(1);
}

#if WS2812_NEED_GPIO
static void ws2812_gpio_ensure_pin(u32_t pin)
{
	if ((sys_read32(GPIO_CTL(pin)) & GPIO_CTL_MFP_MASK) != GPIO_CTL_MFP_GPIO) {
		ws2812_gpio_init_pin(pin);
	}
}

static void ws2812_flush_buf(u32_t pin, const uint8_t *grb, size_t len,
			     const struct ws2812_timing *tm)
{
	const u32_t bit = GPIO_BIT(pin);
	u32_t n;
	size_t i;
	int b;

	for (i = 0; i < len; i++) {
		uint8_t val = grb[i];

		for (b = 7; b >= 0; b--) {
			if (val & (1U << b)) {
				sys_write32(bit, GPIO_REG_BSR(GPIO_REG_BASE, pin));
				for (n = tm->t1h; n > 0U; n--) {
					__asm__ volatile ("nop");
				}
				sys_write32(bit, GPIO_REG_BRR(GPIO_REG_BASE, pin));
				for (n = tm->t1l; n > 0U; n--) {
					__asm__ volatile ("nop");
				}
			} else {
				sys_write32(bit, GPIO_REG_BSR(GPIO_REG_BASE, pin));
				for (n = tm->t0h; n > 0U; n--) {
					__asm__ volatile ("nop");
				}
				sys_write32(bit, GPIO_REG_BRR(GPIO_REG_BASE, pin));
				for (n = tm->t0l; n > 0U; n--) {
					__asm__ volatile ("nop");
				}
			}
		}
	}

	sys_write32(bit, GPIO_REG_BRR(GPIO_REG_BASE, pin));
}

static void ws2812_gpio_set_pin(u32_t pin, int high)
{
	u32_t bit = GPIO_BIT(pin);

	if (high) {
		sys_write32(bit, GPIO_REG_BSR(GPIO_REG_BASE, pin));
	} else {
		sys_write32(bit, GPIO_REG_BRR(GPIO_REG_BASE, pin));
	}
}

/* 关中断发送（0=不关，1=关；中断打断会导致 WS2812 时序错乱，灯珠部分不亮） */
#define WS2812_USE_IRQ_LOCK  1

static void ws2812_flush_pin(u32_t pin, const uint8_t *grb, size_t len,
			     u32_t reset_us, const struct ws2812_timing *tm)
{
#if WS2812_USE_IRQ_LOCK
	unsigned int key;
#endif
	struct ws2812_timing local;

	if (!tm) {
		ws2812_fill_timing(&local);
		tm = &local;
	}

	ws2812_gpio_ensure_pin(pin);
#if WS2812_USE_IRQ_LOCK
	key = irq_lock();
#endif
	ws2812_flush_buf(pin, grb, len, tm);
#if WS2812_USE_IRQ_LOCK
	irq_unlock(key);
#endif
	ws2812_gpio_set_pin(pin, 0);
	k_busy_wait(reset_us);
}

#if WS2812_HAS_STRIP1
static void ws2812_flush(void)
{
	ws2812_flush_pin(WS2812_GPIO_PIN, ws2812_grb, sizeof(ws2812_grb),
			 WS2812_RESET_US, NULL);
}
#endif

static void ws2812_gpio_init_pin(u32_t pin)
{
#ifdef BOARD_285L2_WS2812_PIN_CTL
	u32_t ctl = BOARD_285L2_WS2812_PIN_CTL;
#else
	u32_t ctl = GPIO_CTL_MFP_GPIO | GPIO_CTL_GPIO_OUTEN | GPIO_CTL_SMIT
		    | GPIO_CTL_PADDRV_LEVEL(4);
#endif

	sys_write32(ctl, GPIO_CTL(pin));
	ws2812_gpio_set_pin(pin, 0);
}
#endif /* WS2812_NEED_GPIO */

#if defined(CONFIG_BT_MUSIC_LED_STRIP_AMBIENT)
void btmusic_ws2812_ambient_set(uint8_t r, uint8_t g, uint8_t b)
{
	size_t i;

	if (!ws2812_inited) {
		btmusic_ws2812_init();
	}
#if WS2812_HAS_STRIP1
	if (ws2812_order_test_active()) {
		return;
	}

	for (i = 0; i < WS2812_LED_COUNT; i++) {
		size_t off = i * 3U;

		ws2812_grb[off + 0] = g;
		ws2812_grb[off + 1] = r;
		ws2812_grb[off + 2] = b;
	}
	ws2812_flush();
#endif
}

void btmusic_ws2812_ambient_off(void)
{
	btmusic_ws2812_ambient_set(0U, 0U, 0U);
}

/* 氛围灯模式循环：每10秒切换 */
static struct thread_timer ambient_cycle_timer;
static int ambient_mode;
static const struct { uint8_t r; uint8_t g; uint8_t b; } ambient_modes[] = {
	{ 80,  0,   0  },  /* 红 */
	{ 0,   80,  0  },  /* 绿 */
	{ 0,   0,   80 },  /* 蓝 */
	{ 80,  60,  0  },  /* 黄 */
	{ 80,  0,   80 },  /* 紫 */
	{ 0,   80,  80 },  /* 青 */
	{ 40,  20,  0  },  /* 暖白 */
	{ 32,  12,  0  },  /* 柔和暖色 */
};

static void ambient_cycle_cb(struct thread_timer *ttimer, void *arg)
{
	ARG_UNUSED(ttimer);
	ARG_UNUSED(arg);
	btmusic_ws2812_ambient_set(ambient_modes[ambient_mode].r,
				   ambient_modes[ambient_mode].g,
				   ambient_modes[ambient_mode].b);
	ambient_mode = (ambient_mode + 1) % (sizeof(ambient_modes) / sizeof(ambient_modes[0]));
}

static void ambient_cycle_start(void)
{
	thread_timer_init(&ambient_cycle_timer, ambient_cycle_cb, NULL);
	thread_timer_start(&ambient_cycle_timer, 10000, 10000);
	SYS_LOG_INF("ambient cycle timer started @10s");
}
#endif


#if defined(CONFIG_BT_MUSIC_LED_STRIP_SELFTEST)
static void ws2812_selftest(void)
{
	SYS_LOG_INF("selftest R->G->B->off\n");
	btmusic_ws2812_show_rgb(64, 0, 0);
	k_sleep(400);
	btmusic_ws2812_show_rgb(0, 64, 0);
	k_sleep(400);
	btmusic_ws2812_show_rgb(0, 0, 64);
	k_sleep(400);
	btmusic_ws2812_show_rgb(0, 0, 0);
}
#endif


void btmusic_ws2812_init(void)
{
	if (ws2812_inited) {
		return;
	}

#if WS2812_HAS_STRIP1
	ws2812_gpio_init_pin(WS2812_GPIO_PIN);
#endif
#if defined(CONFIG_BT_MUSIC_LED_STRIP2)
	btmusic_ws2812_rhythm_init();
#endif
	k_busy_wait(500);
	ws2812_calibrate();

#if WS2812_HAS_STRIP1
	memset(ws2812_grb, 0, sizeof(ws2812_grb));
	ws2812_flush();
#endif

	ws2812_inited = 1;
	ws2812_show_cnt = 0;

#if defined(CONFIG_BT_MUSIC_LED_STRIP_AMBIENT) && WS2812_HAS_STRIP1
	/* 默认暖色氛围，便于确认 GPIO8 WS2812 接线 */
	btmusic_ws2812_ambient_set(32U, 12U, 0U);
	ambient_cycle_start();
	SYS_LOG_INF("strip1 ambient init ok\n");
#endif

#if defined(CONFIG_BT_MUSIC_LED_STRIP_SELFTEST)
	ws2812_selftest();
#endif
}

void btmusic_ws2812_deinit(void)
{
	if (!ws2812_inited) {
		return;
	}

#if WS2812_HAS_STRIP1
	memset(ws2812_grb, 0, sizeof(ws2812_grb));
	ws2812_flush();
#endif
#if defined(CONFIG_BT_MUSIC_LED_STRIP2)
	btmusic_ws2812_rhythm_deinit();
#endif
	ws2812_inited = 0;
}

static int ws2812_order_test_active(void)
{
	return 0;
}

void btmusic_ws2812_show_rgb(uint8_t r, uint8_t g, uint8_t b)
{
	

	if (!ws2812_inited) {
		btmusic_ws2812_init();
	}
	if (ws2812_order_test_active()) {
		return;
	}

#if WS2812_HAS_STRIP1
	size_t i;

	for (i = 0; i < WS2812_LED_COUNT; i++) {
		size_t off = i * 3U;

		ws2812_grb[off + 0] = g;
		ws2812_grb[off + 1] = r;
		ws2812_grb[off + 2] = b;
	}

	ws2812_flush();
#endif
#if defined(CONFIG_BT_MUSIC_LED_STRIP_DEBUG_LOG)
	if ((ws2812_show_cnt++ % 40U) == 0U) {
		SYS_LOG_INF("show_rgb strip1 R=%u G=%u B=%u\n", r, g, b);
	}
#endif
}

void btmusic_ws2812_show_grb(const uint8_t *grb, size_t len)
{
	

	if (!grb || len == 0U) {
		return;
	}

	if (!ws2812_inited) {
		btmusic_ws2812_init();
	}
	if (ws2812_order_test_active()) {
		return;
	}

#if WS2812_HAS_STRIP1
	size_t i;
	{
		size_t max = sizeof(ws2812_grb);

		if (len > max) {
			len = max;
		}

		memset(ws2812_grb, 0, sizeof(ws2812_grb));
		for (i = 0; i < len; i++) {
			ws2812_grb[i] = grb[i];
		}

		ws2812_flush();
	}
#endif

#if defined(CONFIG_BT_MUSIC_LED_STRIP_DEBUG_LOG)
	if ((ws2812_show_cnt++ % 40U) == 0U) {
		SYS_LOG_INF("show_grb len=%u px0=G%02xR%02xB%02x\n",
			    (unsigned)len,
			    len > 0 ? grb[0] : 0,
			    len > 1 ? grb[1] : 0,
			    len > 2 ? grb[2] : 0);
	}
#endif
}

#define WS2812_MIN_BRIGHT     28U

#if WS2812_HAS_STRIP1 && !defined(CONFIG_BT_MUSIC_LED_STRIP_AMBIENT)
static uint8_t ws2812_interp_band(const uint8_t *band_lvl, int band_num,
				  size_t led_idx, size_t led_count)
{
	uint32_t pos;
	uint32_t b0;
	uint32_t b1;
	uint32_t frac;
	uint32_t lvl;

	if (!band_lvl || band_num < 2 || led_count < 2U) {
		return band_lvl ? band_lvl[0] : 0U;
	}

	pos = led_idx * (uint32_t)(band_num - 1) * 256U / (uint32_t)(led_count - 1U);
	b0 = pos / 256U;
	b1 = b0 + 1U;
	if (b1 >= (uint32_t)band_num) {
		b1 = (uint32_t)band_num - 1U;
	}
	frac = pos % 256U;
	lvl = (uint32_t)band_lvl[b0] * (256U - frac)
	      + (uint32_t)band_lvl[b1] * frac;
	return (uint8_t)(lvl / 256U);
}

static void ws2812_band_color(int band, uint8_t lvl, uint8_t *r, uint8_t *g,
			      uint8_t *b)
{
	if (lvl < WS2812_MIN_BRIGHT) {
		lvl = WS2812_MIN_BRIGHT;
	}

	if (band < 4) {
		*r = lvl;
		*g = (uint8_t)(lvl / 3U);
		*b = (uint8_t)(lvl / 6U);
	} else if (band < 7) {
		*r = (uint8_t)(lvl / 3U);
		*g = lvl;
		*b = (uint8_t)(lvl / 3U);
	} else {
		*r = (uint8_t)(lvl / 6U);
		*g = (uint8_t)(lvl / 3U);
		*b = lvl;
	}
}

static void ws2812_build_spectrum(uint8_t *grb, size_t led_count,
				  const uint8_t *band_lvl, int band_num)
{
	size_t i;
	size_t n = led_count;

	if (!grb || !band_lvl || band_num <= 0 || n == 0U) {
		return;
	}

	for (i = 0; i < n; i++) {
		uint8_t lvl = ws2812_interp_band(band_lvl, band_num, i, n);
		int band = (int)((n > 1U) ? (i * (uint32_t)(band_num - 1)
					   / (uint32_t)(n - 1U))
				      : 0U);
		uint8_t rr;
		uint8_t gg;
		uint8_t bb;
		size_t off = i * 3U;

		ws2812_band_color(band, lvl, &rr, &gg, &bb);
		grb[off + 0] = gg;
		grb[off + 1] = rr;
		grb[off + 2] = bb;
	}
}
#endif

#if defined(CONFIG_BT_MUSIC_LED_RHYTHM) && \
	defined(CONFIG_BT_MUSIC_LED_STRIP2) && \
	!defined(CONFIG_BT_MUSIC_LED_STRIP2_RGB_ORDER_TEST) && \
	!defined(CONFIG_BT_MUSIC_LED_STRIP2_ROW_DEMO)

/* 色板偏移状态（在此 #if 块内共享） */
static uint8_t  beat_pal_offset;

void btmusic_ws2812_spectrum_reset(void)
{
	memset(ws2812_col_lit, 0, sizeof(ws2812_col_lit));
	beat_pal_offset = 0U;
}
#else
void btmusic_ws2812_spectrum_reset(void)
{
}
#endif

#if defined(CONFIG_BT_MUSIC_LED_RHYTHM) && \
	defined(CONFIG_BT_MUSIC_LED_STRIP2) && \
	!defined(CONFIG_BT_MUSIC_LED_STRIP2_RGB_ORDER_TEST) && \
	!defined(CONFIG_BT_MUSIC_LED_STRIP2_ROW_DEMO)
static uint8_t ws2812_interp_col(const uint8_t *band_lvl, int band_num,
				 unsigned col, unsigned cols)
{
	uint32_t pos;
	uint32_t b0;
	uint32_t b1;
	uint32_t frac;
	uint32_t lvl;

	if (!band_lvl || band_num <= 0 || cols == 0U) {
		return 0U;
	}
	if (band_num < 2 || cols < 2U) {
		return band_lvl[0];
	}

	pos = col * (uint32_t)(band_num - 1) * 256U / (uint32_t)(cols - 1U);
	b0 = pos / 256U;
	b1 = b0 + 1U;
	if (b1 >= (uint32_t)band_num) {
		b1 = (uint32_t)band_num - 1U;
	}
	frac = pos % 256U;
	lvl = (uint32_t)band_lvl[b0] * (256U - frac)
	      + (uint32_t)band_lvl[b1] * frac;
	return (uint8_t)(lvl / 256U);
}

#if 0 /* 已被节拍灯替代，保留备用 */
static void ws2812_col_band_color(int band, uint8_t lvl,
				  uint8_t *r, uint8_t *g, uint8_t *b)
{
	static const struct { uint8_t r; uint8_t g; uint8_t b; } pal[10] = {
		{ 255U, 0U,   0U   }, { 255U, 64U,  0U   }, { 255U, 180U, 0U   },
		{ 180U, 255U, 0U   }, { 0U,   255U, 0U   }, { 0U,   255U, 120U },
		{ 0U,   200U, 255U }, { 0U,   80U,  255U }, { 120U, 0U,   255U },
		{ 255U, 0U,   180U },
	};

	if (band < 0) {
		band = 0;
	}
	if (band > 9) {
		band = 9;
	}
	if (lvl == 0U) {
		*r = 0U;
		*g = 0U;
		*b = 0U;
		return;
	}

	*r = (uint8_t)((uint16_t)pal[band].r * lvl / 255U);
	*g = (uint8_t)((uint16_t)pal[band].g * lvl / 255U);
	*b = (uint8_t)((uint16_t)pal[band].b * lvl / 255U);
}
#endif

/** 0~255 线性映射亮 0~4 颗；0 允许整列灭 */
static uint32_t ws2812_lit_from_lvl(uint8_t lvl)
{
	uint32_t lit;

	if (lvl == 0U) {
		return 0U;
	}
	lit = ((uint32_t)lvl * WS2812_STRIP2_GROUP + 254U) / 255U;
	if (lit == 0U) {
		lit = 1U;
	}
	if (lit > WS2812_STRIP2_GROUP) {
		lit = WS2812_STRIP2_GROUP;
	}
	return lit;
}

/* ---- 节拍灯：每 3 列同色，节拍触发全亮，无拍瞬间灭 ---- */

/* 15 组颜色（44 列 ÷ 3 ≈ 15 组），彩虹循环 */
static const struct { uint8_t r; uint8_t g; uint8_t b; } beat_pal[] = {
	{ 255U, 0U,   0U   }, { 255U, 80U,  0U   }, { 255U, 160U, 0U   },
	{ 220U, 255U, 0U   }, { 80U,  255U, 0U   }, { 0U,   255U, 100U },
	{ 0U,   220U, 255U }, { 0U,   100U, 255U }, { 60U,  0U,   255U },
	{ 160U, 0U,   255U }, { 255U, 0U,   200U }, { 255U, 0U,   100U },
	{ 255U, 120U, 0U   }, { 180U, 255U, 0U   }, { 0U,   200U, 255U },
};
#define BEAT_PAL_NUM (sizeof(beat_pal) / sizeof(beat_pal[0]))

/** 44 竖列×4 颗 连续频谱：每 3 列同色，柱高随列能量变化，每帧都刷新 */
static void ws2812_build_matrix_columns(const uint8_t *band_lvl, int band_num)
{
	unsigned cols = WS2812_STRIP2_ROW_LEDS; /* 44 */
	unsigned col;
	unsigned row;

	if (!band_lvl || band_num <= 0) {
		return;
	}

	memset(ws2812_grb2, 0, sizeof(ws2812_grb2));

	for (col = 0; col < cols; col++) {
		uint8_t raw;
		uint32_t lit;
		unsigned group;
		uint8_t rr;
		uint8_t gg;
		uint8_t bb;
		uint8_t vis;

		/* 列能量：由频带插值 */
		raw = ws2812_interp_col(band_lvl, band_num, col, cols);
		lit = ws2812_lit_from_lvl(raw);
		if (lit == 0U) {
			continue;
		}

		/* 每 3 列一组共享颜色，色板偏移随总能量缓慢流动 */
		group = (col / 3U + beat_pal_offset) % BEAT_PAL_NUM;

		/* 亮度钳位 */
		vis = raw;
		if (vis > WS2812_STRIP2_BRIGHTNESS_MAX) {
			vis = WS2812_STRIP2_BRIGHTNESS_MAX;
		}
		if (vis > 0U && vis < WS2812_STRIP2_BRIGHTNESS_MIN) {
			vis = WS2812_STRIP2_BRIGHTNESS_MIN;
		}

		rr = (uint8_t)((uint16_t)beat_pal[group].r * vis / 255U);
		gg = (uint8_t)((uint16_t)beat_pal[group].g * vis / 255U);
		bb = (uint8_t)((uint16_t)beat_pal[group].b * vis / 255U);

		for (row = 0; row < WS2812_STRIP2_ROWS; row++) {
			size_t px = ws2812_strip2_px_index(row, col);

			if (px < WS2812_STRIP2_COUNT && row < lit) {
				ws2812_strip2_set_px(px, rr, gg, bb);
			}
		}
	}

	/* 色板缓慢偏移：每 8 帧移一位，产生颜色流动但不跳变 */
	if ((ws2812_show_cnt & 7U) == 0U) {
		beat_pal_offset++;
		if (beat_pal_offset >= BEAT_PAL_NUM) {
			beat_pal_offset = 0U;
		}
	}
}
#endif

#if defined(CONFIG_BT_MUSIC_LED_RHYTHM)
void btmusic_ws2812_show_spectrum(const uint8_t *band_lvl, int band_num)
{
#if FAKE_RHYTHM_DEMO
	/*
	 * 假律动：每帧随机生成频谱数据，灯柱随机跳动（三列同色不变）。
	 * 验证 WS2812 硬件 + AMBIC1010 时序是否正常。
	 */
	static uint32_t fake_seed = 0x28532853UL;
	uint8_t fake_lvl[10];
	int i;

	fake_seed = fake_seed * 1103515245UL + 12345UL;
	for (i = 0; i < 10; i++) {
		fake_seed = fake_seed * 1103515245UL + 12345UL;
		fake_lvl[i] = (uint8_t)((fake_seed >> 16) & 0xFFU);
	}
	band_lvl = fake_lvl;
	band_num = 10;
#endif /* FAKE_RHYTHM_DEMO */

	if (!ws2812_inited) {
		btmusic_ws2812_init();
	}
	if (ws2812_order_test_active()) {
		return;
	}

	if (!band_lvl || band_num <= 0) {
		return;
	}

#if defined(CONFIG_BT_MUSIC_LED_STRIP2) && defined(CONFIG_BT_MUSIC_LED_RHYTHM) && \
	!defined(CONFIG_BT_MUSIC_LED_STRIP2_RGB_ORDER_TEST) && \
	!defined(CONFIG_BT_MUSIC_LED_STRIP2_ROW_DEMO)
	ws2812_build_matrix_columns(band_lvl, band_num);
	ws2812_flush2();
	/* 每 10 帧打印一次律动灯刷新确认 */
	if ((ws2812_show_cnt++ % 10U) == 0U) {
		SYS_LOG_INF("[led] flush2 ok b0=%u b5=%u b9=%u",
			    band_lvl[0],
			    band_num > 5 ? band_lvl[5] : 0U,
			    band_num > 9 ? band_lvl[9] : 0U);
	}
#endif

#if WS2812_HAS_STRIP1 && !defined(CONFIG_BT_MUSIC_LED_STRIP_AMBIENT)
	ws2812_build_spectrum(ws2812_grb, WS2812_LED_COUNT, band_lvl, band_num);
	ws2812_flush();
#endif
#if defined(CONFIG_BT_MUSIC_LED_STRIP2) && \
	!defined(CONFIG_BT_MUSIC_LED_STRIP2_RGB_ORDER_TEST)
#endif

#if defined(CONFIG_BT_MUSIC_LED_STRIP_DEBUG_LOG)
	if ((ws2812_show_cnt++ % 10U) == 0U) {
#if WS2812_HAS_STRIP1
		size_t n = WS2812_LED_COUNT;
		size_t last = (n > 0U) ? (n - 1U) : 0U;

		SYS_LOG_INF("spectrum b0=%u b5=%u b9=%u px0=G%uR%uB%u px%u=G%uR%uB%u\n",
			    band_lvl[0], band_num > 5 ? band_lvl[5] : 0U,
			    band_num > 9 ? band_lvl[9] : 0U,
			    ws2812_grb[0], ws2812_grb[1], ws2812_grb[2],
			    (unsigned)last,
			    ws2812_grb[last * 3U + 0],
			    ws2812_grb[last * 3U + 1],
			    ws2812_grb[last * 3U + 2]);
#endif
#if defined(CONFIG_BT_MUSIC_LED_STRIP2) && \
	!defined(CONFIG_BT_MUSIC_LED_STRIP2_RGB_ORDER_TEST) && \
	!defined(CONFIG_BT_MUSIC_LED_STRIP2_ROW_DEMO)
		SYS_LOG_INF("s2 c0 lit=%u raw=%u c21=%u c43=%u\n",
			    (unsigned)ws2812_lit_from_lvl(
				    ws2812_interp_col(band_lvl, band_num, 0U,
						      WS2812_STRIP2_ROW_LEDS)),
			    ws2812_interp_col(band_lvl, band_num, 0U,
					      WS2812_STRIP2_ROW_LEDS),
			    ws2812_interp_col(band_lvl, band_num, 21U,
					      WS2812_STRIP2_ROW_LEDS),
			    ws2812_interp_col(band_lvl, band_num,
					      WS2812_STRIP2_ROW_LEDS - 1U,
					      WS2812_STRIP2_ROW_LEDS));
		SYS_LOG_INF("s2 px0 G=%u B=%u V=%u b0=%u b9=%u\n",
			    ws2812_grb2[0], ws2812_grb2[1], ws2812_grb2[2],
			    band_lvl[0], band_num > 9 ? band_lvl[9] : 0U);
#endif
	}
#endif
}

void btmusic_ws2812_show_rhythm(uint8_t bass, uint8_t mid, uint8_t high)
{
	uint8_t lvl[10];
	int i;

	/* bass → bands 0-3, mid → bands 4-6, high → bands 7-9
	 * 全部填充避免 44 列插值时大面积从 0 插值导致偏暗。
	 */
	for (i = 0; i < 4; i++) {
		lvl[i] = bass;
	}
	for (i = 4; i < 7; i++) {
		lvl[i] = mid;
	}
	for (i = 7; i < 10; i++) {
		lvl[i] = high;
	}
	btmusic_ws2812_show_spectrum(lvl, 10);
}
#endif

#if !defined(CONFIG_BT_MUSIC_LED_RHYTHM)
void btmusic_ws2812_show_spectrum(const uint8_t *band_lvl, int band_num)
{
	(void)band_lvl;
	(void)band_num;
}

void btmusic_ws2812_show_rhythm(uint8_t bass, uint8_t mid, uint8_t high)
{
	(void)bass;
	(void)mid;
	(void)high;
}
#endif

#endif /* CONFIG_BT_MUSIC_LED_STRIP || CONFIG_BT_MUSIC_LED_STRIP2 */
