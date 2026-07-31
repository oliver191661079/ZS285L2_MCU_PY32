/*
 * Copyright (c) 2026 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file btmusic_ws2812_rhythm.c
 * @brief AMBIC1010 4×44 矩阵律动灯（SPI2 DMA / GPIO bitbang）
 *
 * GPIO2 MFP=13 = SPI2_MOSI，走标准 SPI API
 */

#include "btmusic_ws2812.h"

#if defined(CONFIG_BT_MUSIC_LED_STRIP2)

#define SYS_LOG_DOMAIN "ws2812"
#include <logging/sys_log.h>

#include <string.h>
#include <kernel.h>
#include <soc.h>
#include <thread_timer.h>

#if defined(CONFIG_BT_MUSIC_LED_STRIP2_SPI_DMA)
#include <spi.h>
#endif

/* ---- Strip2 配置 ---- */
#ifndef CONFIG_BT_MUSIC_LED_STRIP2_GPIO
#ifdef BOARD_285L2_WS2812_RHYTHM_GPIO
#define CONFIG_BT_MUSIC_LED_STRIP2_GPIO BOARD_285L2_WS2812_RHYTHM_GPIO
#else
#define CONFIG_BT_MUSIC_LED_STRIP2_GPIO 14
#endif
#endif
#ifndef CONFIG_BT_MUSIC_LED_STRIP2_COUNT
#define CONFIG_BT_MUSIC_LED_STRIP2_COUNT 176
#endif
#ifndef CONFIG_BT_MUSIC_LED_STRIP2_GROUP_SIZE
#define CONFIG_BT_MUSIC_LED_STRIP2_GROUP_SIZE 4
#endif
#define WS2812_STRIP2_COUNT  CONFIG_BT_MUSIC_LED_STRIP2_COUNT
#define WS2812_STRIP2_GPIO   CONFIG_BT_MUSIC_LED_STRIP2_GPIO
#define WS2812_STRIP2_GROUP  CONFIG_BT_MUSIC_LED_STRIP2_GROUP_SIZE
#define WS2812_STRIP2_ROWS   4U
#define WS2812_STRIP2_ROW_LEDS (WS2812_STRIP2_COUNT / WS2812_STRIP2_ROWS)

#define WS2812_RESET2_US   500U

/* ---- SPI+DMA 缓冲区 ---- */
#if defined(CONFIG_BT_MUSIC_LED_STRIP2_SPI_DMA)
#define WS2812_SPI_BYTES_PER_LED_BYTE  8U
#define WS2812_SPI_BUF_SIZE  (WS2812_STRIP2_COUNT * 3U * WS2812_SPI_BYTES_PER_LED_BYTE)
static uint8_t ws2812_spi_buf[WS2812_SPI_BUF_SIZE] __aligned(4);
static struct device *ws2812_spi_dev;
static struct spi_config ws2812_spi_cfg;
#endif

/* ---- GBR 像素缓冲 ---- */
static uint8_t ws2812_grb2[WS2812_STRIP2_COUNT * 3U];

static void ws2812_strip2_set_px(size_t px, uint8_t r, uint8_t g, uint8_t b)
{
	size_t off = px * 3U;
	ws2812_grb2[off + 0] = g;
	ws2812_grb2[off + 1] = b;
	ws2812_grb2[off + 2] = r;
}

/** 行 0=最下；奇数行蛇形反向 */
static size_t ws2812_strip2_px_index(unsigned row, unsigned col)
{
	unsigned cols = WS2812_STRIP2_ROW_LEDS;
	size_t base;

	if (row >= WS2812_STRIP2_ROWS || col >= cols)
		return WS2812_STRIP2_COUNT;

	base = (size_t)row * cols;
	if (row & 1U)
		return base + (cols - 1U - col);
	return base + col;
}

/* ---- SPI 编码 ---- */
#if defined(CONFIG_BT_MUSIC_LED_STRIP2_SPI_DMA)
static void ws2812_spi_encode(const uint8_t *grb, size_t grb_len)
{
	size_t i;
	int b;
	for (i = 0; i < grb_len; i++) {
		uint8_t byte = grb[i];
		uint8_t *dst = &ws2812_spi_buf[i * WS2812_SPI_BYTES_PER_LED_BYTE];
		for (b = 7; b >= 0; b--)
			*dst++ = (byte & (1U << (unsigned)b)) ? 0xFCU : 0xC0U;
	}
}
#endif

/* ---- GPIO bitbang（回退）---- */
#if !defined(CONFIG_BT_MUSIC_LED_STRIP2_SPI_DMA)
static u32_t ws2812_nop_t0h, ws2812_nop_t0l, ws2812_nop_t1h, ws2812_nop_t1l;
static u32_t ws2812_nop_per_cycle;

static u32_t ws2812_measure_nops(u32_t n)
{
	u32_t t0 = k_cycle_get_32(), t1;
	while (n--) __asm__ volatile ("nop");
	t1 = k_cycle_get_32();
	return t1 - t0;
}

static void ws2812_refresh_timing(void)
{
	u32_t c100 = ws2812_measure_nops(100U);
	u32_t c200 = ws2812_measure_nops(200U);

	ws2812_nop_per_cycle = (c200 > c100) ? ((c200 - c100) / 100U) : 1U;
	if (ws2812_nop_per_cycle == 0U) ws2812_nop_per_cycle = 1U;

	ws2812_nop_t0h = (WS2812_NS_TO_CYCLES(300U) + ws2812_nop_per_cycle - 1U) / ws2812_nop_per_cycle;
	ws2812_nop_t0l = (WS2812_NS_TO_CYCLES(900U) + ws2812_nop_per_cycle - 1U) / ws2812_nop_per_cycle;
	ws2812_nop_t1h = (WS2812_NS_TO_CYCLES(900U) + ws2812_nop_per_cycle - 1U) / ws2812_nop_per_cycle;
	ws2812_nop_t1l = (WS2812_NS_TO_CYCLES(300U) + ws2812_nop_per_cycle - 1U) / ws2812_nop_per_cycle;
}

static void ws2812_gpio_set_pin(u32_t pin, int high)
{
	u32_t bit = GPIO_BIT(pin);
	if (high)
		sys_write32(bit, GPIO_REG_BSR(GPIO_REG_BASE, pin));
	else
		sys_write32(bit, GPIO_REG_BRR(GPIO_REG_BASE, pin));
}

static void ws2812_flush_buf(u32_t pin, const uint8_t *grb, size_t len)
{
	const u32_t bit = GPIO_BIT(pin);
	size_t i;
	int b;

	for (i = 0; i < len; i++) {
		uint8_t val = grb[i];
		for (b = 7; b >= 0; b--) {
			if (val & (1U << b)) {
				sys_write32(bit, GPIO_REG_BSR(GPIO_REG_BASE, pin));
				for (u32_t n = ws2812_nop_t1h; n > 0U; n--) __asm__ volatile ("nop");
				sys_write32(bit, GPIO_REG_BRR(GPIO_REG_BASE, pin));
				for (u32_t n = ws2812_nop_t1l; n > 0U; n--) __asm__ volatile ("nop");
			} else {
				sys_write32(bit, GPIO_REG_BSR(GPIO_REG_BASE, pin));
				for (u32_t n = ws2812_nop_t0h; n > 0U; n--) __asm__ volatile ("nop");
				sys_write32(bit, GPIO_REG_BRR(GPIO_REG_BASE, pin));
				for (u32_t n = ws2812_nop_t0l; n > 0U; n--) __asm__ volatile ("nop");
			}
		}
	}
	sys_write32(bit, GPIO_REG_BRR(GPIO_REG_BASE, pin));
}
#endif

/* ---- flush ---- */
static void ws2812_flush2_len(size_t grb_len)
{
#if defined(CONFIG_BT_MUSIC_LED_STRIP2_SPI_DMA)
	u32_t spi_len = (u32_t)(grb_len * WS2812_SPI_BYTES_PER_LED_BYTE);
	struct spi_buf tx_buf = { .buf = ws2812_spi_buf, .len = spi_len };

	if (!ws2812_spi_dev) return;
	ws2812_spi_encode(ws2812_grb2, grb_len);
	spi_write(&ws2812_spi_cfg, &tx_buf, 1);
	k_busy_wait(WS2812_RESET2_US);
#else
	ws2812_gpio_ensure_pin(WS2812_STRIP2_GPIO);
	ws2812_refresh_timing();
	ws2812_flush_buf(WS2812_STRIP2_GPIO, ws2812_grb2, grb_len);
	ws2812_gpio_set_pin(WS2812_STRIP2_GPIO, 0);
	k_busy_wait(WS2812_RESET2_US);
#endif
}

static void ws2812_flush2(void)
{
	ws2812_flush2_len(sizeof(ws2812_grb2));
}

/* ---- 初始化 ---- */
void btmusic_ws2812_rhythm_init(void)
{
#if defined(CONFIG_BT_MUSIC_LED_STRIP2_SPI_DMA)
	ws2812_spi_dev = device_get_binding("SPI_2");
	if (ws2812_spi_dev) {
		ws2812_spi_cfg.dev = ws2812_spi_dev;
		ws2812_spi_cfg.frequency = 12000000;
		ws2812_spi_cfg.operation = SPI_OP_MODE_MASTER
			| SPI_MODE_CPOL | SPI_MODE_CPHA | SPI_WORD_SET(8);
		ws2812_spi_cfg.cs = NULL;
		SYS_LOG_INF("rhythm SPI2 init @%dHz", ws2812_spi_cfg.frequency);
	} else {
		SYS_LOG_ERR("rhythm SPI2 dev not found");
	}
#else
	{
		u32_t ctl = GPIO_CTL_MFP_GPIO | GPIO_CTL_GPIO_OUTEN
			    | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(4);
		sys_write32(ctl, GPIO_CTL(WS2812_STRIP2_GPIO));
		ws2812_gpio_set_pin(WS2812_STRIP2_GPIO, 0);
	}
	k_busy_wait(2000);
	ws2812_refresh_timing();
#endif
	/* 初始闪烁一次确认接线 */
	memset(ws2812_grb2, 0, sizeof(ws2812_grb2));
	for (unsigned col = 0; col < WS2812_STRIP2_ROW_LEDS; col++) {
		size_t px = ws2812_strip2_px_index(0U, col);
		if (px < WS2812_STRIP2_COUNT)
			ws2812_strip2_set_px(px, 32U, 0U, 0U);
	}
	ws2812_flush2();
	k_sleep(200);
	memset(ws2812_grb2, 0, sizeof(ws2812_grb2));
	ws2812_flush2();
	SYS_LOG_INF("rhythm init blink ok");
}

void btmusic_ws2812_rhythm_deinit(void)
{
	memset(ws2812_grb2, 0, sizeof(ws2812_grb2));
	ws2812_flush2();
}

/* ---- 律动显示 ---- */
#if defined(CONFIG_BT_MUSIC_LED_RHYTHM)
static uint8_t beat_pal_offset;
static const struct { uint8_t r; uint8_t g; uint8_t b; } beat_pal[] = {
	{ 255U, 0U, 0U }, { 255U, 80U, 0U }, { 255U, 160U, 0U },
	{ 220U, 255U, 0U }, { 80U, 255U, 0U }, { 0U, 255U, 100U },
	{ 0U, 220U, 255U }, { 0U, 100U, 255U }, { 60U, 0U, 255U },
	{ 160U, 0U, 255U }, { 255U, 0U, 200U }, { 255U, 0U, 100U },
	{ 255U, 120U, 0U }, { 180U, 255U, 0U }, { 0U, 200U, 255U },
};
#define BEAT_PAL_NUM (sizeof(beat_pal) / sizeof(beat_pal[0]))

static void ws2812_build_matrix_columns(const uint8_t *band_lvl, int band_num)
{
	unsigned cols = WS2812_STRIP2_ROW_LEDS;

	if (!band_lvl || band_num <= 0) return;
	memset(ws2812_grb2, 0, sizeof(ws2812_grb2));

	for (unsigned col = 0; col < cols; col++) {
		uint32_t pos = col * (uint32_t)(band_num - 1) * 256U / (uint32_t)(cols - 1U);
		uint32_t b0 = pos / 256U, b1 = b0 + 1U, frac = pos % 256U;
		if (b1 >= (uint32_t)band_num) b1 = (uint32_t)band_num - 1U;
		uint8_t raw = (uint8_t)(((uint32_t)band_lvl[b0] * (256U - frac)
					+ (uint32_t)band_lvl[b1] * frac) / 256U);
		if (raw == 0U) continue;

		uint32_t lit = ((uint32_t)raw * WS2812_STRIP2_GROUP + 254U) / 255U;
		if (lit == 0U) lit = 1U;
		if (lit > WS2812_STRIP2_GROUP) lit = WS2812_STRIP2_GROUP;

		unsigned group = (col / 3U + beat_pal_offset) % BEAT_PAL_NUM;
		uint8_t vis = raw;
		if (vis > 80U) vis = 80U;
		if (vis > 0U && vis < 6U) vis = 6U;

		uint8_t rr = (uint8_t)((uint16_t)beat_pal[group].r * vis / 255U);
		uint8_t gg = (uint8_t)((uint16_t)beat_pal[group].g * vis / 255U);
		uint8_t bb = (uint8_t)((uint16_t)beat_pal[group].b * vis / 255U);

		for (unsigned row = 0; row < WS2812_STRIP2_ROWS && row < lit; row++) {
			size_t px = ws2812_strip2_px_index(row, col);
			if (px < WS2812_STRIP2_COUNT)
				ws2812_strip2_set_px(px, rr, gg, bb);
		}
	}
	if ((beat_pal_offset++ & 7U) == 0U) beat_pal_offset %= BEAT_PAL_NUM;
}

void btmusic_ws2812_show_spectrum(const uint8_t *band_lvl, int band_num)
{
	ws2812_build_matrix_columns(band_lvl, band_num);
	ws2812_flush2();
}

void btmusic_ws2812_show_rhythm(uint8_t bass, uint8_t mid, uint8_t high)
{
	uint8_t lvl[10];
	for (int i = 0; i < 4; i++)  lvl[i] = bass;
	for (int i = 4; i < 7; i++)  lvl[i] = mid;
	for (int i = 7; i < 10; i++) lvl[i] = high;
	btmusic_ws2812_show_spectrum(lvl, 10);
}
#else
void btmusic_ws2812_show_spectrum(const uint8_t *band_lvl, int band_num)
{
	(void)band_lvl; (void)band_num;
}
void btmusic_ws2812_show_rhythm(uint8_t bass, uint8_t mid, uint8_t high)
{
	(void)bass; (void)mid; (void)high;
}
#endif /* CONFIG_BT_MUSIC_LED_RHYTHM */

/* ---- 播放开始 ---- */
void btmusic_ws2812_on_playback_start(void)
{
#if defined(CONFIG_BT_MUSIC_LED_RHYTHM)
	unsigned col;

	memset(ws2812_grb2, 0, sizeof(ws2812_grb2));
	for (col = 0; col < WS2812_STRIP2_ROW_LEDS; col++) {
		size_t px = ws2812_strip2_px_index(0U, col);
		if (px < WS2812_STRIP2_COUNT)
			ws2812_strip2_set_px(px, 48U, 0U, 0U);
	}
	ws2812_flush2();
	k_sleep(400);
	memset(ws2812_grb2, 0, sizeof(ws2812_grb2));
	ws2812_flush2();
	SYS_LOG_INF("rhythm playback start blink ok");
#endif
}

#endif /* CONFIG_BT_MUSIC_LED_STRIP2 */
