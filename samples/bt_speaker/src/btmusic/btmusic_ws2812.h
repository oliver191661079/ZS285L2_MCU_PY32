/*
 * Copyright (c) 2026 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BTMUSIC_WS2812_H_
#define BTMUSIC_WS2812_H_

#include <stdint.h>
#include <stddef.h>

/*
 * 假律动演示：生成随机频谱数据驱动灯带，不依赖 DSP 能量采集。
 * 验证通过后置 0 切回真律动。
 */
#define FAKE_RHYTHM_DEMO  0  /* 律动灯移到外部 MCU */

#if defined(CONFIG_BT_MUSIC_LED_STRIP) || defined(CONFIG_BT_MUSIC_LED_STRIP2)

void btmusic_ws2812_init(void);
void btmusic_ws2812_deinit(void);
void btmusic_ws2812_show_rgb(uint8_t r, uint8_t g, uint8_t b);
void btmusic_ws2812_show_grb(const uint8_t *grb, size_t len);
void btmusic_ws2812_show_spectrum(const uint8_t *band_lvl, int band_num);
void btmusic_ws2812_spectrum_reset(void);
void btmusic_ws2812_on_playback_start(void);
void btmusic_ws2812_show_rhythm(uint8_t bass, uint8_t mid, uint8_t high);
#if defined(CONFIG_BT_MUSIC_LED_STRIP_AMBIENT)
void btmusic_ws2812_ambient_set(uint8_t r, uint8_t g, uint8_t b);
void btmusic_ws2812_ambient_off(void);
#endif
#if defined(CONFIG_BT_MUSIC_LED_STRIP2) && defined(CONFIG_BT_MUSIC_LED_RHYTHM)
void btmusic_ws2812_strip2_clear(void);
#endif
#if defined(CONFIG_BT_MUSIC_LED_STRIP2)
void btmusic_ws2812_rhythm_init(void);
void btmusic_ws2812_rhythm_deinit(void);
#endif

#else

static inline void btmusic_ws2812_init(void) { }
static inline void btmusic_ws2812_deinit(void) { }
static inline void btmusic_ws2812_show_rgb(uint8_t r, uint8_t g, uint8_t b)
{
	(void)r;
	(void)g;
	(void)b;
}
static inline void btmusic_ws2812_show_grb(const uint8_t *grb, size_t len)
{
	(void)grb;
	(void)len;
}
static inline void btmusic_ws2812_show_spectrum(const uint8_t *band_lvl,
						int band_num)
{
	(void)band_lvl;
	(void)band_num;
}
static inline void btmusic_ws2812_spectrum_reset(void) { }
static inline void btmusic_ws2812_on_playback_start(void) { }
static inline void btmusic_ws2812_show_rhythm(uint8_t bass, uint8_t mid,
					      uint8_t high)
{
	(void)bass;
	(void)mid;
	(void)high;
}
#if defined(CONFIG_BT_MUSIC_LED_STRIP_AMBIENT)
static inline void btmusic_ws2812_ambient_set(uint8_t r, uint8_t g, uint8_t b)
{
	(void)r;
	(void)g;
	(void)b;
}
static inline void btmusic_ws2812_ambient_off(void) { }
#endif

#endif

#endif /* BTMUSIC_WS2812_H_ */
