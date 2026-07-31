/*
 * Copyright (c) 2020 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __SEG_LED_DISPLAY_H
#define __SEG_LED_DISPLAY_H

/**
 * @brief actions seg led display APIs
 * @defgroup mb_display actions seg ledisplay APIs
 * @{
 */

#include <zephyr/types.h>
#include <device.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LED_BITMAP_MAX_NUM                (56)

/*! struct led_com_map
 *  @brief The structure to depict a character to shown and its bitmap.
 */
struct led_bitmap
{
	u8_t character;
	u8_t bitmap;
};

/*! struct led_com_map
 *  @brief The structure to discribe the mapping relationship of COM and LED pin for lighten/hiden a single LED segmemt.
 */
struct led_com_map
{
	u8_t pin;
	u8_t com;
};

/*! struct led_seg_map
 *  @brief The structure to discribe the mapping relationship of showing a digital number by the 7 LED segmemts.
 */
struct led_seg_map
{
	struct led_bitmap bit_map[LED_BITMAP_MAX_NUM];
	struct led_com_map icon_map[CONFIG_LED_NUMBER_ICONS];
	struct led_com_map num_map[CONFIG_LED_NUMBER_ITEMS][CONFIG_LED_COM_NUM];
};

#if defined(CONFIG_LED_USE_DEFAULT_MAP)
enum led_def_icon {
	LED_PAUSE = 0,
	LED_PLAY,
	LED_USB,
	LED_SD,
	LED_COL,
	LED_AUX,
	LED_FM,
	LED_P1,
	ICON_MAX
};
#elif defined(CONFIG_LED_USE_1P2INCH_MAP)
enum led_def_icon {
	LED_PM = 0,
	LED_MOON,
	LED_ALARM1,
	LED_ALARM2,
	LED_BT,
	LED_FM,
	LED_SD,
	LED_TONE,
	LED_COL,
	LED_P1,
	ICON_MAX
};
#endif

enum led_def_num {
	NUMBER1 = 0,
	NUMBER2,
	NUMBER3,
	NUMBER4
};

enum led_clear_mode {
	LED_CLEAR_ALL = 0,
	LED_CLEAR_NUM
};

struct seg_led_driver_api {
	int (*display_number)(struct device *dev, u8_t num_addr, u8_t c, bool display);
	int (*display_number_str)(struct device *dev, u8_t start_pos, const u8_t *str, bool display);
	int (*display_icon)(struct device *dev, u8_t icon_idx, bool display);
	int (*refresh_onoff)(struct device *dev, bool is_on);
	int (*clear_screen)(struct device *dev, u8_t clr_mode);
	int (*sleep)(struct device *dev);
	int (*wakeup)(struct device *dev);
	int (*update_seg_map)(struct device *dev, struct led_seg_map *seg_map);
};

static inline int segled_display_number(struct device *dev, u8_t num_addr, u8_t c, bool display)
{
	const struct seg_led_driver_api *api = dev->driver_api;

	return api->display_number(dev, num_addr, c, display);
}

static inline int segled_display_number_string(struct device *dev, u8_t start_pos, const u8_t *str, bool display)
{
	const struct seg_led_driver_api *api = dev->driver_api;

	return api->display_number_str(dev, start_pos, str, display);
}

static inline int segled_display_icon(struct device *dev, u8_t icon_idx, bool display)
{
	const struct seg_led_driver_api *api = dev->driver_api;

	return api->display_icon(dev, icon_idx, display);
}

static inline int segled_refresh_onoff(struct device *dev, bool is_on)
{
	const struct seg_led_driver_api *api = dev->driver_api;

	return api->refresh_onoff(dev, is_on);
}

static inline int segled_clear_screen(struct device *dev, u8_t clr_mode)
{
	const struct seg_led_driver_api *api = dev->driver_api;

	return api->clear_screen(dev, clr_mode);
}

static inline int segled_sleep(struct device *dev)
{
	const struct seg_led_driver_api *api = dev->driver_api;

	return api->sleep(dev);
}

static inline unsigned int segled_wakeup(struct device *dev)
{
	const struct seg_led_driver_api *api = dev->driver_api;

	return api->wakeup(dev);
}

static inline int segled_update_seg_map(struct device *dev, struct led_seg_map *seg_map)
{
	const struct seg_led_driver_api *api = dev->driver_api;

	return api->update_seg_map(dev, seg_map);
}

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* __SEG_LED_DISPLAY_H */
