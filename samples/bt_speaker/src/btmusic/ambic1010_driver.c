/*
 * ambic1010_driver.c — AMBIC1010 4×44 RGB LED 矩阵驱动实现
 *
 * 依赖：只依赖 ambic1010_driver.h 中的 4 个硬件抽象函数。
 *       无 RTOS 依赖，无 Zephyr 依赖，纯 C99。
 *
 * 硬件接线：
 *   MCU GPIO → AMBIC1010 DIN（数据输入）
 *   VCC → 5V（灯带电源）
 *   GND → 共地
 *
 * 矩阵物理布局（蛇形走线，从下往上）：
 *
 *   Row3 (top)    LED132 ← LED133 ... ← LED175  (col 43←0, 右→左)
 *   Row2           LED88 → LED89  ... → LED131  (col 0→43, 左→右)
 *   Row1           LED44 ← LED45  ... ← LED87   (col 43←0, 右→左)
 *   Row0 (bottom)  LED0  → LED1   ... → LED43   (col 0→43, 左→右)
 *
 *   数据流向：LED0 → LED1 → ... → LED175（单链串联）
 */

#include "ambic1010_driver.h"

/* ================================================================
 * 色板
 * ================================================================ */

static const struct {
	uint8_t r, g, b;
} pal[AMBIC1010_PAL_NUM] = {
	{ 255, 0,   0   }, { 255, 80,  0   }, { 255, 160, 0   },
	{ 220, 255, 0   }, { 80,  255, 0   }, { 0,   255, 100 },
	{ 0,   220, 255 }, { 0,   100, 255 }, { 60,  0,   255 },
	{ 160, 0,   255 }, { 255, 0,   200 }, { 255, 0,   100 },
	{ 255, 120, 0   }, { 180, 255, 0   }, { 0,   200, 255 },
};

/* ================================================================
 * 像素缓冲区（GBR 格式，176×3=528 字节）
 * ================================================================ */

static uint8_t g_pixels[AMBIC1010_LED_COUNT * 3U];

/* ================================================================
 * 矩阵坐标映射（蛇形走线）
 * ================================================================ */

uint16_t ambic1010_xy(uint8_t row, uint8_t col)
{
	uint16_t base;

	if (row >= AMBIC1010_ROWS || col >= AMBIC1010_COLS) {
		return AMBIC1010_LED_COUNT; /* 越界 */
	}

	base = (uint16_t)row * AMBIC1010_COLS;

	if (row & 1U) {
		/* 奇数行反向 */
		return base + (AMBIC1010_COLS - 1U - col);
	}
	return base + col;
}

/* ================================================================
 * 像素设置（GBR 格式：byte0=G, byte1=B, byte2=R）
 * ================================================================ */

void ambic1010_set_pixel(uint16_t px, uint8_t r, uint8_t g, uint8_t b)
{
	uint16_t off;

	if (px >= AMBIC1010_LED_COUNT) {
		return;
	}

	off = px * 3U;
	g_pixels[off + 0] = g;
	g_pixels[off + 1] = b;
	g_pixels[off + 2] = r;
}

/* ================================================================
 * WS2812/AMBIC1010 单字节 bitbang 发送（MSB first）
 * ================================================================ */

static void ambic1010_send_byte(uint8_t byte)
{
	int b;

	for (b = 7; b >= 0; b--) {
		if (byte & (1U << (unsigned)b)) {
			/* "1" 码: H=900ns, L=300ns */
			ambic1010_hw_gpio_set(1);
			ambic1010_hw_delay_us(AMBIC1010_NS_TO_US(AMBIC1010_T1H_NS));
			ambic1010_hw_gpio_set(0);
			ambic1010_hw_delay_us(AMBIC1010_NS_TO_US(AMBIC1010_T1L_NS));
		} else {
			/* "0" 码: H=300ns, L=900ns */
			ambic1010_hw_gpio_set(1);
			ambic1010_hw_delay_us(AMBIC1010_NS_TO_US(AMBIC1010_T0H_NS));
			ambic1010_hw_gpio_set(0);
			ambic1010_hw_delay_us(AMBIC1010_NS_TO_US(AMBIC1010_T0L_NS));
		}
	}
}

/* ================================================================
 * 整帧发送
 * ================================================================ */

void ambic1010_flush(void)
{
	unsigned int key;
	uint16_t i;

	/*
	 * 关中断保护时序。完整一帧约 5ms（176×24×1.2µs）。
	 * 如果对音频/通信有影响，可在帧间拆分为多段发送——
	 * 但 WS2812 协议要求所有数据在一次 RESET 前连续发送，
	 * 中间不能插入 >50µs 的空闲，否则会触发锁存。
	 */
	key = ambic1010_hw_irq_lock();

	for (i = 0; i < (AMBIC1010_LED_COUNT * 3U); i++) {
		ambic1010_send_byte(g_pixels[i]);
	}

	ambic1010_hw_irq_unlock(key);

	/* RESET 脉冲（拉低 >200µs） */
	ambic1010_hw_gpio_set(0);
	ambic1010_hw_delay_us(AMBIC1010_RESET_US);
}

/* ================================================================
 * 初始化
 * ================================================================ */

void ambic1010_init(void)
{
	ambic1010_hw_gpio_set(0);
	ambic1010_hw_delay_us(AMBIC1010_RESET_US); /* 上电复位 */
	ambic1010_clear();
}

/* ================================================================
 * 清空
 * ================================================================ */

void ambic1010_clear(void)
{
	memset(g_pixels, 0, sizeof(g_pixels));
	ambic1010_flush();
}

/* ================================================================
 * 频带插值（10 段 → 44 列）
 * ================================================================ */

static uint8_t interp_band(const uint8_t band_lvl[AMBIC1010_BANDS],
			   uint8_t col)
{
	uint32_t pos;
	uint8_t  b0, b1;
	uint32_t frac;
	uint32_t lvl;

	if (AMBIC1010_COLS < 2U) {
		return band_lvl[0];
	}

	pos = (uint32_t)col * (AMBIC1010_BANDS - 1U) * 256U
	      / (AMBIC1010_COLS - 1U);
	b0 = (uint8_t)(pos / 256U);
	b1 = b0 + 1U;
	if (b1 >= AMBIC1010_BANDS) {
		b1 = AMBIC1010_BANDS - 1U;
	}
	frac = pos % 256U;
	lvl = (uint32_t)band_lvl[b0] * (256U - frac)
	      + (uint32_t)band_lvl[b1] * frac;
	return (uint8_t)(lvl / 256U);
}

/* ================================================================
 * 柱高计算（0~255 → 亮 0~4 颗）
 * ================================================================ */

static uint8_t lit_from_lvl(uint8_t lvl)
{
	uint8_t lit;

	if (lvl == 0U) {
		return 0U;
	}
	lit = (uint8_t)(((uint16_t)lvl * AMBIC1010_GROUP + 254U) / 255U);
	if (lit == 0U) {
		lit = 1U;
	}
	if (lit > AMBIC1010_GROUP) {
		lit = AMBIC1010_GROUP;
	}
	return lit;
}

/* ================================================================
 * 频谱显示（柱高随能量，每 3 列同色，色板缓慢偏移）
 * ================================================================ */

static uint8_t g_pal_offset;

void ambic1010_show_spectrum(const uint8_t band_lvl[AMBIC1010_BANDS])
{
	uint8_t raw[AMBIC1010_COLS];
	uint8_t col, row;

	/* 1. 插值得到每列能量 */
	for (col = 0; col < AMBIC1010_COLS; col++) {
		raw[col] = interp_band(band_lvl, col);
	}

	/* 2. 构建像素 */
	memset(g_pixels, 0, sizeof(g_pixels));

	for (col = 0; col < AMBIC1010_COLS; col++) {
		uint8_t lvl = raw[col];
		uint8_t lit = lit_from_lvl(lvl);
		uint8_t group;
		uint8_t rr, gg, bb;

		if (lit == 0U) {
			continue;
		}

		/* 亮度钳位 */
		if (lvl > AMBIC1010_BRIGHT_MAX) lvl = AMBIC1010_BRIGHT_MAX;
		if (lvl > 0U && lvl < AMBIC1010_BRIGHT_MIN) lvl = AMBIC1010_BRIGHT_MIN;

		/* 每 3 列一组 + 色板偏移 */
		group = (uint8_t)((col / 3U + g_pal_offset) % AMBIC1010_PAL_NUM);

		rr = (uint8_t)((uint16_t)pal[group].r * lvl / 255U);
		gg = (uint8_t)((uint16_t)pal[group].g * lvl / 255U);
		bb = (uint8_t)((uint16_t)pal[group].b * lvl / 255U);

		for (row = 0; row < AMBIC1010_ROWS; row++) {
			uint16_t px;

			if (row >= lit) break;

			px = ambic1010_xy(row, col);
			if (px < AMBIC1010_LED_COUNT) {
				ambic1010_set_pixel(px, rr, gg, bb);
			}
		}
	}

	/* 3. 色板偏移（每 8 帧移一位，产生颜色流动） */
	static uint8_t flush_cnt;
	if ((++flush_cnt & 7U) == 0U) {
		g_pal_offset++;
		if (g_pal_offset >= AMBIC1010_PAL_NUM) {
			g_pal_offset = 0U;
		}
	}

	/* 4. 刷新 */
	ambic1010_flush();
}

/* ================================================================
 * 随机假律动
 * ================================================================ */

void ambic1010_show_rhythm_random(void)
{
	uint8_t band[AMBIC1010_BANDS];
	static uint32_t seed = 0x2853A5A5UL;
	int i;

	for (i = 0; i < AMBIC1010_BANDS; i++) {
		seed = seed * 1103515245UL + 12345UL;
		band[i] = (uint8_t)((seed >> 16) & 0xFFU);
	}

	ambic1010_show_spectrum(band);
}

/* ================================================================
 * UART 协议接收示例（供外部 MCU 使用）
 * ================================================================
 *
 * 在 UART RX 中断中调用：
 *
 *   void uart_rx_isr(uint8_t byte)
 *   {
 *       static uint8_t buf[12], pos;
 *       static uint8_t sync;
 *
 *       if (!sync) {
 *           if (byte == 0xA5) { sync = 1; pos = 0; }
 *           return;
 *       }
 *       buf[pos++] = byte;
 *       if (pos >= 11) {  // 10 bands + 1 checksum
 *           uint8_t chk = 0;
 *           for (int i = 0; i < 10; i++) chk ^= buf[i];
 *           if (chk == buf[10]) {
 *               ambic1010_show_spectrum(buf);
 *           }
 *           sync = 0;
 *       }
 *   }
 */
