/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief framebuffer interface
*/

#include <os_common_api.h>
#include <display.h>
#include "ugui.h"

#ifndef __FRAME_BUFFER_H__
#define __FRAME_BUFFER_H__

#define NUM_OF_MAX_DISPLAY 2
#define NUM_OF_MAX_FRAMEBUFFER 2

typedef int (*spi_cs_func)(char *name,int val);
typedef enum {
	MAIN_FB_ID,
	EXT_FB_ID,
} FB_ID;

struct framebuffer {
	u8_t display_num;
	u8_t format;
	u16_t width;
	u16_t height;
	int data;
	struct display_device* display[NUM_OF_MAX_DISPLAY];
};

int fb_init(u16_t width, u16_t height, u8_t format);
struct framebuffer* fb_get_framebuffer(u8_t fb_id);
u16_t fb_get_width(u8_t fb_id);
u16_t fb_get_height(u8_t fb_id);
u16_t fb_get_format(u8_t fb_id);
int fb_framebuffer_post(u8_t fb_id);
void fb_regist_cs_func(spi_cs_func func);
void fb_draw_char(s16_t x, s16_t y, UG_COLOR fc, UG_COLOR bc, s16_t charheight, UG_CharBitmap charBitmap);
void fb_draw_pixel(u16_t x1, u16_t y1, u32_t color);
int fb_draw_line(u16_t x1, u16_t y1, u16_t x2, u16_t y2, u32_t color);
int fb_fill_frame(u16_t x1, u16_t y1, u16_t x2, u16_t y2, u32_t color);
int fb_draw_frame(u16_t x1, u16_t y1, u16_t x2, u16_t y2, short *data, u16_t strid);
struct display_device* fb_display_device_register(u8_t fb_id, struct display_info* info);

#endif