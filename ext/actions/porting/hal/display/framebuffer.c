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
#include <string.h>
#include <framebuffer.h>
#include <mem_manager.h>
#include "psram.h"

static struct framebuffer* global_fb[NUM_OF_MAX_FRAMEBUFFER];
static u8_t global_fb_num = 0;

#define FRAME_BUFFER_IN_PSRAM 0

#define LINE_WIDTH 320

#if FRAME_BUFFER_IN_PSRAM
static u8_t cache_line = 0xFF;
#endif

static short line_buff[LINE_WIDTH + 2];

extern const unsigned short logo_image[];

int fb_dump(struct framebuffer* fb)
{
	int i, j;

	for(i = 0; i < 320 * 240 * 2; i += LINE_WIDTH * 2)
	{
		psram_read(fb->data + i, (u8_t *)line_buff, LINE_WIDTH * 2);
		for(j = 0; j < LINE_WIDTH; j++){
			if(j % 16 == 0)
			{
				printk(" \n");
			}
			printk(" 0x%4x ",(u16_t)line_buff[j]);
		}
	}
	return 0;
}

int fb_display_on(void)
{
	struct framebuffer* fb = global_fb[MAIN_FB_ID];
	struct display_device* display = NULL;
	int res = 0;
	if (!fb) {
		SYS_LOG_ERR("fb not register\n");
		return -1;
	}

	display = fb->display[MAIN_DISPLAY];
	if (!display) {
		SYS_LOG_ERR("display not register\n");
		return -2;
	}
	if (display->hw_func[HW_FUNC_DISPLAY_ON].state & HW_FUNC_ENABLED) {
		res = ((int(*)(void))display->hw_func[HW_FUNC_DISPLAY_ON].func)();
		if (res != 0) {
			SYS_LOG_ERR("HW_FUNC_DISPLAY_ON error\n");
			return -1;
		}
	}
	return 0;
}
int fb_display_off(void)
{
	struct framebuffer* fb = global_fb[MAIN_FB_ID];
	struct display_device* display = NULL;
	int res = 0;
	if (!fb) {
		SYS_LOG_ERR("fb not register\n");
		return -1;
	}

	display = fb->display[MAIN_DISPLAY];
	if (!display) {
		SYS_LOG_ERR("display not register\n");
		return -2;
	}
	if (display->hw_func[HW_FUNC_DISPLAY_OFF].state & HW_FUNC_ENABLED) {
		res = ((int(*)(void))display->hw_func[HW_FUNC_DISPLAY_OFF].func)();
		if (res != 0) {
			SYS_LOG_ERR("HW_FUNC_DISPLAY_OFF error\n");
			return -1;
		}
	}
	return 0;
}
int fb_init(u16_t width, u16_t height, u8_t format)
{
	struct framebuffer* fb = NULL;
#if FRAME_BUFFER_IN_PSRAM
	int i = 0;
#endif
	if (global_fb_num >= NUM_OF_MAX_FRAMEBUFFER) return -1;

	fb = mem_malloc(sizeof(struct framebuffer));
	if ( !fb ) return -ENOMEM;
	
	memset(fb, 0, sizeof(struct framebuffer));

	fb->width = width;
	fb->height = height;
	fb->format = format;

#if FRAME_BUFFER_IN_PSRAM
	fb->data = psram_alloc(PSRAM_MIN_BLOCK_SIZE);
	if(fb->data < 0)
	{
		SYS_LOG_ERR("fb data mem_malloc fail \n");
	}

	for(i = 0; i < 320 * 240 * 2; i += LINE_WIDTH * 2)
	{
		memcpy((u8_t *)line_buff, (u8_t *)logo_image + i, LINE_WIDTH * 2);
		psram_write(fb->data + i, (u8_t *)line_buff, LINE_WIDTH * 2);
	}
#endif
//	fb_dump(fb);
	global_fb[global_fb_num++] = fb;
	SYS_LOG_INF("width %d height %d format %d fb->data 0x%x \n",fb->width,fb->height,fb->format,fb->data);
	return global_fb_num - 1;
}

struct framebuffer* fb_get_framebuffer(u8_t fb_id)
{
	if ( fb_id >= NUM_OF_MAX_FRAMEBUFFER ) return NULL;

	return global_fb[fb_id];
}

u16_t fb_get_width(u8_t fb_id)
{
	if ( fb_id >= NUM_OF_MAX_FRAMEBUFFER ) return 0;

	if ( !global_fb[fb_id] ) return 0;

	return global_fb[fb_id]->width;
}

u16_t fb_get_height(u8_t fb_id)
{
	if ( fb_id >= NUM_OF_MAX_FRAMEBUFFER ) return 0;

	if ( !global_fb[fb_id] ) return 0;

	return global_fb[fb_id]->height;
}

u16_t fb_get_format(u8_t fb_id)
{
	if ( fb_id >= NUM_OF_MAX_FRAMEBUFFER ) return 0;

	if ( !global_fb[fb_id] ) return 0;

	return global_fb[fb_id]->format;
}

int fb_framebuffer_post(u8_t fb_id)
{
	struct framebuffer* fb = global_fb[fb_id];
	struct display_device* display = NULL;
#if FRAME_BUFFER_IN_PSRAM
	int i, j;
	int res = 0;
#endif

	if (!fb) {
		SYS_LOG_ERR("fb not register\n");
		return -1;
	}

	display = fb->display[fb_id];
	if (!display) {
		SYS_LOG_ERR("display not register\n");
		return -1;
	}
#if FRAME_BUFFER_IN_PSRAM
	for (i = 0; i < fb->height; i++) {
		psram_read(fb->data + i * LINE_WIDTH * 2, (u8_t *)line_buff, LINE_WIDTH * 2);
		cache_line = i;
		if (display->hw_func[HW_FUNC_DRAW_LINE].state & HW_FUNC_ENABLED) {
			res = ((int(*)(int line, int line_start, short *data, int len))display->hw_func[HW_FUNC_DRAW_LINE].func)(i, 0, line_buff,fb->width);
			if (res != 0) {
				SYS_LOG_ERR("HW_FUNC_DRAW_PIXEL error\n");
				return -1;
			}
		}
	}
#endif
	return 0;
}

void fb_draw_pixel(u16_t x1, u16_t y1, u32_t color)
{
	struct framebuffer* fb = global_fb[MAIN_FB_ID];
	struct display_device* display = NULL;
	int res = 0;

	if (!fb) {
		//SYS_LOG_ERR("fb not register\n");
		return ;
	}

	display = fb->display[MAIN_DISPLAY];
	if (!display) {
		SYS_LOG_ERR("display not register\n");
		return ;
	}
#if FRAME_BUFFER_IN_PSRAM
	if(cache_line != y1)
	{
		if (cache_line != 0xFF) {
			psram_write(fb->data + cache_line * LINE_WIDTH * 2, line_buff, LINE_WIDTH * 2);
		}
		psram_read(fb->data + y1 * LINE_WIDTH * 2, (u8_t *)line_buff, LINE_WIDTH * 2);
		cache_line = y1;
	}
	line_buff[x1] = color;	
#else
	if (display->hw_func[HW_FUNC_DRAW_PIXEL].state & HW_FUNC_ENABLED) {
		res = ((int(*)(u16_t x1, u16_t y1,  u32_t color))display->hw_func[HW_FUNC_DRAW_PIXEL].func)(x1, y1, color);
		if (res != 0) {
			//SYS_LOG_ERR("HW_FUNC_DRAW_PIXEL error\n");
			return;
		}
	}
#endif

	return;
}

void fb_regist_cs_func(spi_cs_func func)
{

	struct framebuffer* fb = global_fb[MAIN_FB_ID];
	struct display_device* display = NULL;
	int res = 0;

	if (!fb) {
		//SYS_LOG_ERR("fb not register\n");
		return ;
	}

	display = fb->display[MAIN_DISPLAY];
	if (!display) {
		SYS_LOG_ERR("display not register\n");
		return ;
	}
	if (display->hw_func[HW_FUNC_DISPLAY_REGIST_CS_FUNC].state & HW_FUNC_ENABLED) {
		res = ((int(*)(spi_cs_func func))display->hw_func[HW_FUNC_DISPLAY_REGIST_CS_FUNC].func)(func);
		if (res != 0) {
			//SYS_LOG_ERR("HW_FUNC_DISPLAY_REGIST_CS_FUNC error\n");
			return;
		}
	}

	return;
}

void fb_draw_char(s16_t x, s16_t y, UG_COLOR fc, UG_COLOR bc, s16_t charheight, UG_CharBitmap charBitmap)
{
	struct framebuffer* fb = global_fb[MAIN_FB_ID];
	struct display_device* display = NULL;
	int res = 0;

	if (!fb) {
		return ;
	}

	display = fb->display[MAIN_DISPLAY];
	if (!display) {
		SYS_LOG_ERR("display not register\n");
		return ;
	}

	if (display->hw_func[HW_FUNC_DRAW_CHAR].state & HW_FUNC_ENABLED) {
		res = ((int(*)(s16_t x, s16_t y, UG_COLOR fc, UG_COLOR bc, s16_t charheight, UG_CharBitmap charBitmap))display->hw_func[HW_FUNC_DRAW_CHAR].func)(x, y, fc, bc, charheight, charBitmap);
		if (res != 0) {
			return;
		}
	}
}
int fb_draw_line(u16_t x1, u16_t y1, u16_t x2, u16_t y2, u32_t color)
{
	struct framebuffer* fb = global_fb[MAIN_FB_ID];
	struct display_device* display = NULL;
	int res = 0;
	printk("fb_draw_line [%d,%d][%d,%d] color 0x%x",x1,y1,x2,y2,color);
	if (!fb) {
		SYS_LOG_ERR("fb not register\n");
		return -1;
	}

	display = fb->display[MAIN_DISPLAY];
	if (!display) {
		SYS_LOG_ERR("display not register\n");
		return -2;
	}
#if FRAME_BUFFER_IN_PSRAM
	if(cache_line != y1)
	{
		if (cache_line != 0xFF) {
			psram_write(fb->data + cache_line * LINE_WIDTH * 2, line_buff, LINE_WIDTH * 2);
		}
		psram_read(fb->data + y1 * LINE_WIDTH * 2, (u8_t *)line_buff, LINE_WIDTH * 2);
		cache_line = y1;
	}
	for(int i = x1; i <= x2; i++)
	{
		line_buff[i] = color;
	}
#else
	if (display->hw_func[HW_FUNC_DRAW_LINE].state & HW_FUNC_ENABLED) {
		res = ((int(*)(u16_t x1, u16_t y1, u16_t x2, u16_t y2,  u32_t color))display->hw_func[HW_FUNC_DRAW_LINE].func)(x1, y1, x2, y2, color);
		if (res != 0) {
			SYS_LOG_ERR("HW_FUNC_DRAW_LINE error (%d)\n",res);
			return res;
		}
	}
#endif
	return res;
}
int fb_draw_frame(u16_t x1, u16_t y1, u16_t x2, u16_t y2, short *data, u16_t strid)
{
	struct framebuffer* fb = global_fb[MAIN_FB_ID];
	struct display_device* display = NULL;
#if FRAME_BUFFER_IN_PSRAM
	u16_t i, j;
#endif
	int res = 0;

	if (!fb) {
		SYS_LOG_ERR("fb not register\n");
		return -1;
	}

	display = fb->display[MAIN_DISPLAY];
	if (!display) {
		SYS_LOG_ERR("display not register\n");
		return -2;
	}
#if FRAME_BUFFER_IN_PSRAM
	for(i = y1; i <= y2; i++) {
		if (cache_line != i) 	{
			if (cache_line != 0xFF) {
				psram_write(fb->data + cache_line * LINE_WIDTH * 2, (u8_t *)line_buff, LINE_WIDTH * 2);
			}
			psram_read(fb->data + i * LINE_WIDTH * 2, (u8_t *)line_buff, LINE_WIDTH * 2);
			cache_line = i;
		}
		for(j = x1; j <= x2; j++) {
			line_buff[j] = color;
		}
	}
#else
	if (display->hw_func[HW_FUNC_DRAW_FRAME].state & HW_FUNC_ENABLED) {
		res = ((int(*)(u16_t x1, u16_t y1, u16_t x2, u16_t y2,  u16_t *data, u16_t strid))display->hw_func[HW_FUNC_DRAW_FRAME].func)(x1, y1, x2, y2, data ,strid);
		if (res != 0) {
			SYS_LOG_ERR("HW_FUNC_FILL_FRAME error (%d)\n",res);
			return res;
		}
	}
#endif
	return res;
}
int fb_fill_frame(u16_t x1, u16_t y1, u16_t x2, u16_t y2, u32_t color)
{
	struct framebuffer* fb = global_fb[MAIN_FB_ID];
	struct display_device* display = NULL;
#if FRAME_BUFFER_IN_PSRAM
	u16_t i, j;
#endif
	int res = 0;

	if (!fb) {
		SYS_LOG_ERR("fb not register\n");
		return -1;
	}

	display = fb->display[MAIN_DISPLAY];
	if (!display) {
		SYS_LOG_ERR("display not register\n");
		return -2;
	}

#if FRAME_BUFFER_IN_PSRAM
	for(i = y1; i <= y2; i++) {
		if (cache_line != i) 	{
			if (cache_line != 0xFF) {
				psram_write(fb->data + cache_line * LINE_WIDTH * 2, (u8_t *)line_buff, LINE_WIDTH * 2);
			}
			psram_read(fb->data + i * LINE_WIDTH * 2, (u8_t *)line_buff, LINE_WIDTH * 2);
			cache_line = i;
		}
		for(j = x1; j <= x2; j++) {
			line_buff[j] = color;
		}
	}
#else
	if (display->hw_func[HW_FUNC_FILL_FRAME].state & HW_FUNC_ENABLED) {
		res = ((int(*)(u16_t x1, u16_t y1, u16_t x2, u16_t y2,  u32_t color))display->hw_func[HW_FUNC_FILL_FRAME].func)(x1, y1, x2, y2, color);
		if (res != 0) {
			SYS_LOG_ERR("HW_FUNC_FILL_FRAME error (%d)\n",res);
			return res;
		}
	}
#endif

	return res;
}

struct display_device* fb_display_device_register(u8_t fb_id, struct display_info* info)
{
	struct display_device* display = NULL;

	if ( fb_id >= NUM_OF_MAX_FRAMEBUFFER ) return NULL;

	if ( !global_fb[fb_id] ) {
		if(fb_init(info->width,info->height,info->color_format)) {
			return NULL;
		}
	}

	if ( global_fb[fb_id]->display_num >= NUM_OF_MAX_DISPLAY ) return NULL;

	display = mem_malloc(sizeof(struct display_device));

	if (!display) return NULL;

	memcpy(display, info, sizeof(struct display_info));

	global_fb[fb_id]->display[global_fb[fb_id]->display_num++] = display;

	SYS_LOG_INF("fb_id  %d  display_num %d \n", fb_id, global_fb[fb_id]->display_num);

	return display;
}