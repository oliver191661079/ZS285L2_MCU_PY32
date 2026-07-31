/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_FRAMEWORK_INCLUDE_DISPLAY_UI_GRAPHICS_H_
#define ZEPHYR_FRAMEWORK_INCLUDE_DISPLAY_UI_GRAPHICS_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief pixel format definitions
 *
 * In case a pixel format consists out of multiple bytes the byte order is
 * big endian.
 */
enum {
	HAL_PIXEL_FORMAT_ARGB_8888 = 1,
	HAL_PIXEL_FORMAT_ARGB_6666,
	HAL_PIXEL_FORMAT_ARGB_8565,
	HAL_PIXEL_FORMAT_RGB_888,
	HAL_PIXEL_FORMAT_RGB_565,
	HAL_PIXEL_FORMAT_BGR_565,

	HAL_PIXEL_FORMAT_A8,
	HAL_PIXEL_FORMAT_A4,
	HAL_PIXEL_FORMAT_A1,
};

/**
 * @brief Blending mode definitions
 */
enum {
	/* no blending */
	HAL_BLENDING_NONE     = 0x0000,
	/* ONE / ONE_MINUS_SRC_ALPHA */
	HAL_BLENDING_PREMULT  = 0x0001,
	/* SRC_ALPHA / ONE_MINUS_SRC_ALPHA */
	HAL_BLENDING_COVERAGE = 0x0002,
};

/**
 * @brief Transformation definitions
 *
 * IMPORTANT NOTE:
 * HAL_TRANSFORM_ROT_90 is applied CLOCKWISE and AFTER HAL_TRANSFORM_FLIP_{H|V}.
 */
enum {
	/* flip source image horizontally (around the vertical axis) */
	HAL_TRANSFORM_FLIP_H    = 0x01,
	/* flip source image vertically (around the horizontal axis)*/
	HAL_TRANSFORM_FLIP_V    = 0x02,
	/* rotate source image 90 degrees clockwise */
	HAL_TRANSFORM_ROT_90    = 0x04,
	/* rotate source image 180 degrees */
	HAL_TRANSFORM_ROT_180   = 0x03,
	/* rotate source image 270 degrees clockwise */
	HAL_TRANSFORM_ROT_270   = 0x07,
	/* don't use. see system/window.h */
	HAL_TRANSFORM_RESERVED  = 0x08,
};

/**
 * @struct ui_color
 * @brief Structure holding ui color
 */
typedef struct ui_color {
	union {
		struct {
			uint8_t b; /* blue component */
			uint8_t g; /* green component */
			uint8_t r; /* red component */
			uint8_t a; /* alpha component */
		};

		uint32_t full; /* argb8888 full value */
	};
} ui_color_t;

/**
 * @brief Construct ui color
 *
 * @param r red component
 * @param g green component
 * @param b blue component
 * @param a alpha component
 *
 * @return ui color structure
 */
static inline ui_color_t ui_color_make(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	ui_color_t color = { .r = r, .g = g, .b = b, .a = a, };

	return color;
}

/**
 * @brief Construct ui color from 32-bit hex value
 *
 * @param c 32-bit color value argb-8888
 *
 * @return ui color structure
 */
static inline ui_color_t ui_color_hex(uint32_t c)
{
	ui_color_t color = { .full = c, };

	return color;
}

/**
 * @brief Construct ui color from 16-bit hex value
 *
 * @param c 16-bit color value rgb-565
 *
 * @return ui color structure
 */
static inline ui_color_t ui_color_hex16(uint16_t c)
{
	ui_color_t color = {
		.a = 0xff,
		.r = (c & 0xf800) >> 8,
		.g = (c & 0x07f0) >> 3,
		.b = (c & 0x001f) << 3,
	};

	color.r = color.r | ((color.r >> 3) & 0x7);
	color.g = color.g | ((color.g >> 2) & 0x3);
	color.b = color.b | ((color.b >> 3) & 0x7);

	return color;
}

/**
 * @brief Query pixel format is opaque
 *
 * @param pixel_format Display format
 *
 * @return the query result
 */
static inline int hal_pixel_format_is_opaque(uint32_t pixel_format)
{
	switch (pixel_format) {
	case HAL_PIXEL_FORMAT_ARGB_8888:
	case HAL_PIXEL_FORMAT_ARGB_6666:
	case HAL_PIXEL_FORMAT_ARGB_8565:
	case HAL_PIXEL_FORMAT_A8:
	case HAL_PIXEL_FORMAT_A4:
	case HAL_PIXEL_FORMAT_A1:
		return 0;
	default:
		return 1;
	}
}

/**
 * @brief Query pixel format bits per pixel
 *
 * @param pixel_format pixel format
 *
 * @return the query result
 */
static inline uint8_t hal_pixel_format_get_bits_per_pixel(uint32_t pixel_format)
{
	switch (pixel_format) {
	case HAL_PIXEL_FORMAT_RGB_565:
	case HAL_PIXEL_FORMAT_BGR_565:
		return 16;
	case HAL_PIXEL_FORMAT_ARGB_8888:
		return 32;
	case HAL_PIXEL_FORMAT_ARGB_6666:
	case HAL_PIXEL_FORMAT_ARGB_8565:
	case HAL_PIXEL_FORMAT_RGB_888:
		return 24;
	case HAL_PIXEL_FORMAT_A8:
		return 8;
	case HAL_PIXEL_FORMAT_A4:
		return 4;
	case HAL_PIXEL_FORMAT_A1:
		return 1;
	default:
		return 0;
	}
}

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_FRAMEWORK_INCLUDE_DISPLAY_UI_GRAPHICS_H_ */
