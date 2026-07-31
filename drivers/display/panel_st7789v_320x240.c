/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * References:
 *
 * https://www.microbit.co.uk/device/screen
 * https://lancaster-university.github.io/microbit-docs/ubit/display/
 */
#include <zephyr.h>
#include <init.h>
#include <board.h>
#include <gpio.h>
#include <device.h>
#include <string.h>
#ifdef CONFIG_SWITCH_APPFAMILY
#include <soc_reboot.h>
#endif
#include "act_lcd_display.h"
#include "display.h"
#include "resource.h"
#include "sdfs.h"
#include "spi.h"
#include "ugui.h"

#define OPT_BY_DMA 1
#define LINE_BUFF_MAX_LEN (240*2*2)/*max support 24*20 font display*/
#define MISO_FUNC 0x3025
#define DC_SW_FUNC 0x2000060

const struct display_info display = {
	.display_type = DISP_LCD,
	.color_format = COLOR_16_BIT,
	.rotation = 90,
	.width = 240,
	.height = 320,
};

static struct device *lcdc_dev;
#ifndef CONFIG_ACTS_SPI_DISPLAY
static short line_buff[322];
#else
static u8_t line_buff[LINE_BUFF_MAX_LEN+2];
#endif

K_MUTEX_DEFINE(panel_mutex);

struct spi_cs_control st7789v_cs;
struct spi_cd_miso st7789v_dc;
static struct spi_func st7789v_cs_func = {
.cs_func = NULL,
.dev_spi_user = "st7789v_display",
.driver_or_controller = 0,
};

static void panel_set_address(unsigned int x1, unsigned int y1, unsigned int x2, unsigned int y2)
{
	lcdc_send_cmd(lcdc_dev, 0x2a); /* col */
	lcdc_send_data(lcdc_dev, x1>>8);	/* first */
	lcdc_send_data(lcdc_dev, x1);	/*  */
	lcdc_send_data(lcdc_dev, x2>>8);	/* end */
	lcdc_send_data(lcdc_dev, x2);	/*  */
	lcdc_send_cmd(lcdc_dev, 0x2b); /* row */
	lcdc_send_data(lcdc_dev, y1>>8);	/* first */
	lcdc_send_data(lcdc_dev, y1);	/*  */
	lcdc_send_data(lcdc_dev, y2>>8);	/* end */
	lcdc_send_data(lcdc_dev, y2);	/*  */
	/*set to mem write mode */
	lcdc_send_cmd(lcdc_dev, 0x2C);
}

static void draw_char(u32_t x1, u32_t y1, u32_t x2, u32_t y2, u16_t pixel_num)
{
	if (pixel_num == 0) {
		printk("draw fail\n");
		return;
	}

	int x, y ;

	if (display.rotation == 90 || display.rotation == 270) {
		x = display.width - y1;
		y = x1;
		x1 = x;
		y1 = y;
		x = display.width - y2;
		y = x2;
		x2 = x;
		y2 = y;
		if(x1 > x2){
			x = x1;
			x1 = x2;
			x2 = x;
		}
		if(y1 > y2){
			y = y1;
			y1 = y2;
			y2 = y;
		}
	}

	if(x2 >= display.width)
		x2 = display.width;

	if(y2 >= display.height)
		y2 = display.height;

	x2 -= 1;
	y2 -= 1;

	panel_set_address(x1, y1, x2, y2);

	spi_send_data_by_dma(lcdc_dev, line_buff, pixel_num);
}

static inline u16_t AlphaBlend_RGB_565(u32_t p1, u32_t p2, u8_t a2)
{
	u32_t  v1 = (p1 | (p1 << 16)) & 0x07E0F81F;
	u32_t  v2 = (p2 | (p2 << 16)) & 0x07E0F81F;

	u32_t  v3 = (((v2 - v1) * (a2 >> 3) >> 5) + v1) & 0x07E0F81F;

	return (u16_t)((v3 & 0xFFFF) | (v3 >> 16));
}


static inline u16_t BlendPixel_RGB_565(u32_t p1, u32_t p2, u8_t a2)
{
	if (a2 >= 0xF8) {
		return p2;
	} else if (a2 >= 0x08) {
		return AlphaBlend_RGB_565(p1, p2, a2);
	} else {
		return p1;
	}
}
static int convert_pixel(int *x1, int *y1, int size_x, int size_y)
{
	int x,y;

	if (display.rotation == 90 || display.rotation == 270) {
		x = *x1;
		y = *y1;
		*x1 = size_y - y;
		*y1 = x;

		if(*x1 > display.width || *y1 > display.height)
			return -1;
	}
	if (display.rotation == 180 || display.rotation == 270) {
		x = *x1;
		y = *y1;
		*y1 = size_y - y;
		*x1 = size_x - x;

		if(*x1 > display.width || *y1 > display.height)
			return -1;
	}
	return 0;
}

static int panel_draw_single_char(s16_t start_x, s16_t start_y, u32_t fc, u32_t bc, s16_t charheight, UG_CharBitmap charBitmap)
{
	u16_t i, j, xo, yo;
	u16_t bitsOffs = 0;
	u16_t color = 0;
	u16_t bitsVal = 0;
	u32_t _bsCount = 0;
	u32_t _bsHold  = 0;
	u32_t x, y;
	u16_t pixel_num = 0;
	u8_t *_bsData  = NULL;

	xo = start_x;
	yo = start_y;

	memset(line_buff, 0, sizeof(line_buff));

	for(j = 0; j < charheight; j++ )
	{
		xo = start_x;
		_bsData  = (u8_t*)(charBitmap.bitsData) + ((bitsOffs) / 8);
		_bsCount = 8 - ((bitsOffs) % 8);
		_bsHold  = (*_bsData++) >> ((bitsOffs) % 8);

		for( i= 0 ;i < charBitmap.charWidth ;i++ )
		{
			while (_bsCount < (u32_t)(charBitmap.bitsPerPixel)) {
				_bsHold  += (s32_t)(*_bsData++) << _bsCount;
				_bsCount += 8;
			}

			bitsVal = _bsHold & ((1 << (charBitmap.bitsPerPixel)) - 1);
			_bsHold >>= (charBitmap.bitsPerPixel);
			_bsCount -= (charBitmap.bitsPerPixel);

			color = BlendPixel_RGB_565(bc, fc, 0xFF * bitsVal / (charBitmap.grayLevel  - 1));

			x = xo - start_x;
			y = yo - start_y;
			if (convert_pixel(&x, &y, charBitmap.charWidth - 1, charheight - 1)) {
				printk("address invalid\n");
				return -1;
			}
			if (2 *(x + y * charheight) >= LINE_BUFF_MAX_LEN) {
				printk("address exceed\n");
				return -1;
			}
			line_buff[2 *(x + y * charheight)] = color>>8;
			line_buff[2 *(x + y * charheight) + 1] = color;
			pixel_num += 2;

			xo++;
		}
		yo++;
		bitsOffs += charBitmap.frameRect.w * charBitmap.bitsPerPixel;
	}
	if (pixel_num)
		draw_char(start_x, start_y, xo, yo, pixel_num);
	return 0;
}

int panel_draw_single_pixel(int x1, int y1, int color)
{
#ifdef CONFIG_ACTS_SPI_DISPLAY
	int x, y;

	if (display.rotation == 90 || display.rotation == 270) {
		x = display.width - y1;
		y = x1;
		x1 = x;
		y1 = y;
	}

	panel_set_address(x1, y1, x1, y1);
	lcdc_send_pixel_data(lcdc_dev, color);
#endif
	return 0;
}

int panel_draw_single_line(int line, int line_start, short *data, int len)
{
	panel_set_address(line_start, line, line_start + len, line);
	/* panel_set_operation_mode(OP_WRITE_MEMORY); */
#ifndef CONFIG_ACTS_SPI_DISPLAY
	lcdc_send_data_by_dma(lcdc_dev, data, len + 1);
#else
	u32_t j = 0;
	u32_t spi_send_len = 0;

	for (j = 0; j < len+1; j++) {
		line_buff[spi_send_len++] = data[j]>>8;
		line_buff[spi_send_len++] = data[j];
		if (spi_send_len >= LINE_BUFF_MAX_LEN)
			break;
	}
	spi_send_data_by_dma(lcdc_dev, line_buff, spi_send_len);
#endif
	return 0;
}

static void convert_coord(int *x1, int *x2, int *y1, int *y2)
{
	int x, y;

	if (display.rotation == 90 || display.rotation == 270) {
		y = *y1;
		*y1 = *x1;
		*x1 = display.width - *y2;

		*y2 = *x2;
		*x2 = display.width - y;
	}

	if (display.rotation == 180 || display.rotation == 270) {
		x = *x1;
		*x1 = display.width - *x2;
		*x2 = display.width - x;

		y = *y1;
		*y1 = display.height - *y2;
		*y2 = display.height - y;
	}
	*x2 -= 1;
	*y2 -= 1;
}

int panel_draw_frame(int x1, int y1, int x2, int y2, short *data, int strid)
{
	k_mutex_lock(&panel_mutex, K_FOREVER);

	convert_coord(&x1, &x2, &y1, &y2);
	panel_set_address(x1, y1, x2, y2);

#ifndef CONFIG_ACTS_SPI_DISPLAY
#if OPT_BY_DMA
	for (int i = 0; i < y2 - y1 + 1 ; i++) {
		send_len = x2 - x1 + 1;
		for (int j = 0; j < send_len; j++) {
			line_buff[j] = data[i * strid + j];
		}
		lcdc_send_data_by_dma(lcdc_dev, line_buff, send_len);
	}
#else
		for (int i = 0 ; i < y2 - y1 + 1; i++) {
			for (int j = 0; j < x2 - x1 + 1; j++) {
				lcdc_send_data(lcdc_dev,  data[i * strid + j]>>8);
				lcdc_send_data(lcdc_dev,  data[i * strid + j]);
			}
		}
	#endif/* OPT_BY_DMA */
#else
	u32_t spi_send_len = 0;
	int pix_map = (y2 - y1+1)*(x2 - x1 + 1);
	u32_t send_len = 0;
	u32_t data_loc = 0;

	while (pix_map > 0) {
		if (pix_map > LINE_BUFF_MAX_LEN/2)
			spi_send_len = LINE_BUFF_MAX_LEN/2;
		else
			spi_send_len = pix_map;

		for (int j = 0; j < spi_send_len; j++) {
			line_buff[send_len++] = data[data_loc]>>8;
			line_buff[send_len++] = data[data_loc++];
			if (send_len > LINE_BUFF_MAX_LEN)
				break;
		}
		if (send_len > LINE_BUFF_MAX_LEN)
			break;
		spi_send_data_by_dma(lcdc_dev, line_buff, send_len);
		pix_map -= spi_send_len;
		spi_send_len = 0;
		send_len = 0;
	}

#endif

	k_mutex_unlock(&panel_mutex);

	return 0;
}

void panel_reset(void)
{
	struct device *dev = NULL;

	dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
#if CONFIG_ACTS_SPI_DISPLAY
	gpio_pin_configure(dev, BOARD_PANEL_ST7789V_RES, GPIO_DIR_OUT);
	gpio_pin_configure(dev, BOARD_PANEL_ST7789V_DC, GPIO_DIR_OUT);
	gpio_pin_write(dev, BOARD_PANEL_ST7789V_DC, 1);
	gpio_pin_write(dev, BOARD_PANEL_ST7789V_RES, 1);
	k_sleep(10);
	gpio_pin_write(dev, BOARD_PANEL_ST7789V_RES, 0);
	k_sleep(10);
	gpio_pin_write(dev, BOARD_PANEL_ST7789V_RES, 1);
	k_sleep(150);
#endif
}

void panel_display_off(void)
{
	lcdc_send_cmd(lcdc_dev, 0x28);  /* display off */
}

void panel_display_on(void)
{
	lcdc_send_cmd(lcdc_dev, 0x29);
	k_sleep(300);/* display on */
}

int panel_fill_frame(int x1, int y1, int x2, int y2, int color)
{
	/* int i,j; */
	k_mutex_lock(&panel_mutex, K_FOREVER);

	convert_coord(&x1, &x2, &y1, &y2);

	panel_set_address(x1, y1, x2, y2);
#ifndef CONFIG_ACTS_SPI_DISPLAY
#if OPT_BY_DMA
	int send_len;

	for (int i = 0; i < y2 - y1 + 1 ; i++) {
		send_len = x2 - x1 + 1;
		for (int j = 0; j < send_len; j++) {
			line_buff[j] = color;
		}
		lcdc_send_data_by_dma(lcdc_dev, line_buff, send_len);
	}
#else
		for (i = 0 ; i < y2 - y1 + 1; i++) {
			for (j = 0; j < x2 - x1 + 1; j++) {
				lcdc_send_data(lcdc_dev, color>>8);
				lcdc_send_data(lcdc_dev, color);
			}
		}
	#endif/* OPT_BY_DMA */
#else
	u32_t spi_send_len = 0;
	int pix_map = (y2 - y1+1)*(x2 - x1 + 1);
	u32_t send_len = 0;

	while (pix_map > 0) {
		if (pix_map > LINE_BUFF_MAX_LEN/2)
			spi_send_len = LINE_BUFF_MAX_LEN/2;
		else
			spi_send_len = pix_map;
		for (int j = 0; j < spi_send_len; j++) {
			line_buff[send_len++] = color>>8;
			line_buff[send_len++] = color;
			if (send_len > LINE_BUFF_MAX_LEN)
				break;
		}
		if (send_len > LINE_BUFF_MAX_LEN)
			break;
		spi_send_data_by_dma(lcdc_dev, line_buff, send_len);
		pix_map -= spi_send_len;
		spi_send_len = 0;
		send_len = 0;
	}
#endif
	k_mutex_unlock(&panel_mutex);
	return 0;
}

extern struct spi_config st7789v_spi_master_conf;

int st7789v_regist_cs_func(spi_cs_func func)//HW_FUNC_DISPLAY_REGIST_CS_FUNC
{
	st7789v_spi_master_conf.cs = &st7789v_cs;
	st7789v_spi_master_conf.dc_func_switch = &st7789v_dc;
	if (func) {
		st7789v_spi_master_conf.cs_func = &st7789v_cs_func;
		st7789v_cs_func.cs_func = func;
	} else {
		st7789v_spi_master_conf.cs_func = NULL;
	}
	return 0;
}
void st7789v_panel_init(void)
{
	void *logo_image = NULL;
	st7789v_spi_master_conf.cs = &st7789v_cs;
	st7789v_spi_master_conf.dc_func_switch = &st7789v_dc;
	st7789v_spi_master_conf.cs_func = NULL;
	st7789v_cs_func.cs_func = NULL;

	/* lcd reset */
	panel_reset();

	/*exit sleep in*/
	lcdc_send_cmd(lcdc_dev, 0x11);
	k_sleep(120);

	/* ST7789V Frame rate setting */
	lcdc_send_cmd(lcdc_dev, 0xb2);

	lcdc_send_data(lcdc_dev, 0x0c);

	lcdc_send_data(lcdc_dev, 0x0c);
	lcdc_send_data(lcdc_dev, 0x00);
	lcdc_send_data(lcdc_dev, 0x33);
	lcdc_send_data(lcdc_dev, 0x33);

	lcdc_send_cmd(lcdc_dev, 0xb7);
	lcdc_send_data(lcdc_dev, 0x35);

	/* ST7789V Power setting */
	lcdc_send_cmd(lcdc_dev, 0xbb);
	lcdc_send_data(lcdc_dev, 0x35);

	lcdc_send_cmd(lcdc_dev, 0xc0);
	lcdc_send_data(lcdc_dev, 0x2c);

	lcdc_send_cmd(lcdc_dev, 0xc2);
	lcdc_send_data(lcdc_dev, 0x01);

	lcdc_send_cmd(lcdc_dev, 0xc3);
	lcdc_send_data(lcdc_dev, 0x0b);

	lcdc_send_cmd(lcdc_dev, 0xc4);
	lcdc_send_data(lcdc_dev, 0x20);

	lcdc_send_cmd(lcdc_dev, 0xc6);
	lcdc_send_data(lcdc_dev, 0x0f);

	lcdc_send_cmd(lcdc_dev, 0xca);
	lcdc_send_data(lcdc_dev, 0x15);

	lcdc_send_cmd(lcdc_dev, 0xc8);
	lcdc_send_data(lcdc_dev, 0x08);

	lcdc_send_cmd(lcdc_dev, 0x55);
	lcdc_send_data(lcdc_dev, 0x90);

	lcdc_send_cmd(lcdc_dev, 0xd0);
	lcdc_send_data(lcdc_dev, 0xa4);
	lcdc_send_data(lcdc_dev, 0xa1);

	/*st7789v set interface pixel format */
	lcdc_send_cmd(lcdc_dev, 0x3A);
	lcdc_send_data(lcdc_dev, 0x55);

	/*ST7789V gamma setting*/
	lcdc_send_cmd(lcdc_dev, 0xe0);
	lcdc_send_data(lcdc_dev, 0xd0);
	lcdc_send_data(lcdc_dev, 0x00);
	lcdc_send_data(lcdc_dev, 0x02);
	lcdc_send_data(lcdc_dev, 0x07);
	lcdc_send_data(lcdc_dev, 0x0b);
	lcdc_send_data(lcdc_dev, 0x1a);
	lcdc_send_data(lcdc_dev, 0x31);
	lcdc_send_data(lcdc_dev, 0x54);
	lcdc_send_data(lcdc_dev, 0x40);
	lcdc_send_data(lcdc_dev, 0x29);
	lcdc_send_data(lcdc_dev, 0x12);
	lcdc_send_data(lcdc_dev, 0x12);
	lcdc_send_data(lcdc_dev, 0x12);
	lcdc_send_data(lcdc_dev, 0x17);

	lcdc_send_cmd(lcdc_dev, 0xe1);
	lcdc_send_data(lcdc_dev, 0xd0);
	lcdc_send_data(lcdc_dev, 0x00);
	lcdc_send_data(lcdc_dev, 0x02);
	lcdc_send_data(lcdc_dev, 0x07);
	lcdc_send_data(lcdc_dev, 0x05);
	lcdc_send_data(lcdc_dev, 0x25);
	lcdc_send_data(lcdc_dev, 0x2d);
	lcdc_send_data(lcdc_dev, 0x44);
	lcdc_send_data(lcdc_dev, 0x45);
	lcdc_send_data(lcdc_dev, 0x1c);
	lcdc_send_data(lcdc_dev, 0x18);
	lcdc_send_data(lcdc_dev, 0x16);
	lcdc_send_data(lcdc_dev, 0x1c);
	lcdc_send_data(lcdc_dev, 0x1d);

	/* /////////////////////////////////// */
	lcdc_send_cmd(lcdc_dev, 0x36); /*  */
	lcdc_send_data(lcdc_dev, 0x00);	/* 0xa0 */

	lcdc_send_cmd(lcdc_dev, 0x2a);
	lcdc_send_data(lcdc_dev, 0x00);
	lcdc_send_data(lcdc_dev, 0x00);
	lcdc_send_data(lcdc_dev, 0x01);
	lcdc_send_data(lcdc_dev, 0x3f);

	lcdc_send_cmd(lcdc_dev, 0x2b);
	lcdc_send_data(lcdc_dev, 0x00);
	lcdc_send_data(lcdc_dev, 0x00);
	lcdc_send_data(lcdc_dev, 0x00);
	lcdc_send_data(lcdc_dev, 0xef);


#ifdef DISPLAY_INVERSION_ON
	lcdc_send_cmd(lcdc_dev, 0x21);
#else
	lcdc_send_cmd(lcdc_dev, 0x20);
#endif

	if (sd_fmap(BOOT_LOGO_BMP, (void **)&logo_image, NULL)) {
		panel_fill_frame(0, 0, 320, 240, 0x1f1f);
		printk("boot logo not find\n");
	} else {
		panel_draw_frame(0, 0, 320, 240, (short *)(logo_image + 32), 240);
	}

	panel_display_on();
	struct display_device *device = NULL;

	device = display_device_register(&display);
	if (device) {
		display_hw_function_register(device, HW_FUNC_DRAW_PIXEL, panel_draw_single_pixel);
		display_hw_function_register(device, HW_FUNC_DRAW_LINE, panel_draw_single_line);
		display_hw_function_register(device, HW_FUNC_DRAW_FRAME, panel_draw_frame);
		display_hw_function_register(device, HW_FUNC_FILL_FRAME, panel_fill_frame);
		display_hw_function_register(device, HW_FUNC_DISPLAY_REGIST_CS_FUNC, st7789v_regist_cs_func);
		display_hw_function_register(device, HW_FUNC_DRAW_CHAR, panel_draw_single_char);
		//display_hw_function_enable(device, HW_FUNC_DRAW_PIXEL);
	}
}


static int st7789v_dc_pad_func_switch(int cmd, int pin)
{

	sys_write32(DC_SW_FUNC, GPIO_REG_BASE+pin*4);

	return 0;
}

static int st7789v_320x240_panel_init(struct device *dev)
{
	ARG_UNUSED(dev);

	lcdc_dev = device_get_binding(CONFIG_SPI_1_NAME);
	st7789v_cs.delay = 0;
	st7789v_cs.gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	st7789v_cs.gpio_pin = BOARD_PANEL_ST7789V_CS;
	st7789v_dc.dc_or_miso = 0;
	st7789v_dc.pin = BOARD_PANEL_ST7789V_DC;
	st7789v_dc.dc_func = st7789v_dc_pad_func_switch;

	st7789v_panel_init();

	if (!lcdc_dev) {
		printk("%s SPI2 master driver was not found!\n", __func__);
		return -1;
	}
	printk("%s,%d,lcdc_dev=%p\n", __func__, __LINE__, lcdc_dev);

	return 0;
}

SYS_INIT(st7789v_320x240_panel_init, POST_KERNEL, 80);
