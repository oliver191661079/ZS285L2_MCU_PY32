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

#define OPT_BY_DMA 1

const struct display_info display = {
	.display_type = DISP_LCD,
	.color_format = COLOR_16_BIT,
	.rotation = 90,
	.width = 240,
	.height = 320,
};

static struct device *lcdc_dev;
static short line_buff[322];

K_MUTEX_DEFINE(panel_mutex);

typedef enum {
	OP_READ_MEMORY,
	OP_WRITE_MEMORY,
	OP_READ_MEMORY_CONTINUE,
	OP_WRITE_MEMORY_CONTINUE,
} LCM_OP_MODE;

static void panel_set_address(unsigned int x1,unsigned int y1,unsigned int x2,unsigned int y2)
{
	lcdc_send_cmd(lcdc_dev, 0x2a); //col
	lcdc_send_data(lcdc_dev, x1>>8);	// first
	lcdc_send_data(lcdc_dev, x1);	//
	lcdc_send_data(lcdc_dev, x2>>8);	// end
	lcdc_send_data(lcdc_dev, x2);	//
	lcdc_send_cmd(lcdc_dev, 0x2b); //row
	lcdc_send_data(lcdc_dev, y1>>8);	// first
	lcdc_send_data(lcdc_dev, y1);	//
	lcdc_send_data(lcdc_dev, y2>>8);	// end
	lcdc_send_data(lcdc_dev, y2);	//
	/*set to mem write mode */
	lcdc_send_cmd(lcdc_dev, 0x2C);
}
#if 0
static void panel_set_operation_mode(int mode)
{
	switch (mode) {
	case OP_READ_MEMORY:
		/*read mem RGB 888 format*/
		lcdc_send_cmd(lcdc_dev, 0x3A);
		lcdc_send_data(lcdc_dev, 0x66);
		lcdc_send_cmd(lcdc_dev, 0x2E);
		break;
	case OP_WRITE_MEMORY:
		/*write mem RGB 565 format*/
		lcdc_send_cmd(lcdc_dev, 0x3A);
		lcdc_send_data(lcdc_dev, 0x55);

		lcdc_send_cmd(lcdc_dev, 0x2C);
		break;
	case OP_READ_MEMORY_CONTINUE:
		/*read mem RGB 888 format*/
		lcdc_send_cmd(lcdc_dev, 0x3A);
		lcdc_send_data(lcdc_dev, 0x66);
		lcdc_send_cmd(lcdc_dev, 0x3E);
		break;
	case OP_WRITE_MEMORY_CONTINUE:
		/*write mem RGB 565 format*/
		lcdc_send_cmd(lcdc_dev, 0x3A);
		lcdc_send_data(lcdc_dev, 0x55);
		lcdc_send_cmd(lcdc_dev, 0x3C);
		break;
	}
}
#endif
int panel_draw_single_pixel(int x1, int y1, int color)
{
	int x, y ;

	if (display.rotation == 90 || display.rotation == 270) {
		x = display.width - y1;
		y = x1;
		x1 = x;
		y1 = y;
	}

	panel_set_address(x1, y1, x1, y1);
	lcdc_send_pixel_data(lcdc_dev, color);
	return 0;
}

int panel_draw_single_line(int line, int line_start, short *data, int len)
{
	panel_set_address(line_start, line, line_start + len, line);
	//panel_set_operation_mode(OP_WRITE_MEMORY);
	lcdc_send_data_by_dma(lcdc_dev, data, len + 1);
	return 0;
}

int panel_fill_frame(int x1, int y1, int x2, int y2, int color)
{
	int i, remain_pixel, send_cnt;
	int x, y ;

	k_mutex_lock(&panel_mutex,K_FOREVER);

	if (display.rotation == 90 || display.rotation == 270) {
		x = display.width - y1;
		y = x1;
		x1 = x;
		y1 = y;
		x = display.width - y2;
		y = x2;
		x2 = x;
		y2 = y;
		if (x1 > x2) {
			x = x1;
			x1 = x2;
			x2 = x;
		}

		if (y1 > y2) {
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

	panel_set_address(x1,y1,x2,y2);

	remain_pixel = (x2 - x1 + 1) * (y2 - y1 + 1);

#if OPT_BY_DMA
	while(remain_pixel > 0)
	{
		if(remain_pixel > sizeof(line_buff) / 2) {
			send_cnt = sizeof(line_buff) / 2;
		} else {
			send_cnt = remain_pixel;
		}
		for (i = 0; i < send_cnt; i++) {
			line_buff[i] = color;
		}
		lcdc_send_data_by_dma(lcdc_dev, line_buff, send_cnt);
		remain_pixel -= send_cnt;
	}
#else
	while(remain_pixel > 0)	{
		lcdc_send_data(lcdc_dev,  color>>8);
		lcdc_send_data(lcdc_dev,  color);
		remain_pixel--;
	}			
#endif
	k_mutex_unlock(&panel_mutex);
	return 0;
}

int panel_draw_frame(int x1, int y1, int x2, int y2, short *data, int strid)
{
	int i,j, send_len;
	int x, y ;

	k_mutex_lock(&panel_mutex,K_FOREVER);

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

	panel_set_address(x1,y1,x2,y2);

	int64_t begin = k_uptime_get();

#if OPT_BY_DMA
	for (i = 0; i < y2 - y1 + 1 ; i++) {
		send_len = x2 - x1 + 1;
		for (j = 0; j < send_len; j++) {
			line_buff[j] = data[i * strid + j];
		}
		lcdc_send_data_by_dma(lcdc_dev, line_buff, send_len);
	}
#else
	for (i = 0 ; i < y2 - y1 + 1; i++) {
		for (j = 0; j < x2 - x1 + 1; j++) {
			lcdc_send_data(lcdc_dev,  data[i * strid + j]>>8);
			lcdc_send_data(lcdc_dev,  data[i * strid + j]);
		}
	}
#endif

	k_mutex_unlock(&panel_mutex);
	int32_t cost =  k_uptime_delta_32(&begin);
	//printk("panel_draw_frame case %d ms \n",cost);
	return 0;
}

void panel_reset( void )
{
	struct device *dev = NULL;
	dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);

	gpio_pin_configure(dev, 45, GPIO_DIR_OUT);
	gpio_pin_write(dev, 45, 1);
	k_sleep(100);
	gpio_pin_write(dev, 45, 0);
	k_sleep(100);
	gpio_pin_write(dev, 45, 1);
}

void panel_backlight_on( void )
{
	struct device *dev = NULL;
	dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);

	gpio_pin_configure(dev, 34, GPIO_DIR_OUT);
	gpio_pin_write(dev, 34, 0);
	k_sleep(100);
}

void panel_backlight_off( void )
{
	struct device *dev = NULL;
	dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);

	gpio_pin_configure(dev, 34, GPIO_DIR_OUT);
	gpio_pin_write(dev, 34, 1);
	k_sleep(100);
}

int panel_display_off( void )
{
	panel_backlight_off();
	k_sleep(100);
    lcdc_send_cmd(lcdc_dev, 0xfe);
	lcdc_send_cmd(lcdc_dev, 0xef);
	lcdc_send_cmd(lcdc_dev, 0x28);
	k_sleep(120);
	lcdc_send_cmd(lcdc_dev, 0x10);  //display off
	return 0;
}

int panel_display_on( void )
{
    //lcdc_send_cmd(lcdc_dev, 0xfe);
	//lcdc_send_cmd(lcdc_dev, 0xef);
	lcdc_send_cmd(lcdc_dev, 0x11);
	k_sleep(120);
	lcdc_send_cmd(lcdc_dev, 0x29);  //display on
	k_sleep(100);
	panel_backlight_on();
	return 0;
}

static void panel_init(void)
{
	void *logo_image = NULL;
	panel_backlight_off();
	/* lcd reset */
	panel_reset();

	//------------end Reset Sequence-----------//
	lcdc_send_cmd(lcdc_dev, 0xfe);
	lcdc_send_cmd(lcdc_dev, 0xef);
	lcdc_send_cmd(lcdc_dev, 0x36);//
	lcdc_send_data(lcdc_dev, 0x98);
	lcdc_send_cmd(lcdc_dev, 0x3a);//
	lcdc_send_data(lcdc_dev, 0x05);


	lcdc_send_cmd(lcdc_dev, 0xa4);//
	lcdc_send_data(lcdc_dev, 0x44);
	lcdc_send_data(lcdc_dev, 0x44);
	lcdc_send_cmd(lcdc_dev, 0xa5);//
	lcdc_send_data(lcdc_dev, 0x42);
	lcdc_send_data(lcdc_dev, 0x42);
	lcdc_send_cmd(lcdc_dev, 0xaa);//
	lcdc_send_data(lcdc_dev, 0x88);
	lcdc_send_data(lcdc_dev, 0x88);

	lcdc_send_cmd(lcdc_dev, 0xe8);//Frame rate=71.8hz
	lcdc_send_data(lcdc_dev, 0x12);
	lcdc_send_data(lcdc_dev, 0x8D);

	lcdc_send_cmd(lcdc_dev, 0xe3);//source ps=01
	lcdc_send_data(lcdc_dev, 0x01);
	lcdc_send_data(lcdc_dev, 0x10);

	lcdc_send_cmd(lcdc_dev, 0xff);//
	lcdc_send_data(lcdc_dev, 0x61);

	lcdc_send_cmd(lcdc_dev, 0xAC);//ldo enable
	lcdc_send_data(lcdc_dev, 0x00);
	lcdc_send_cmd(lcdc_dev, 0xAd);//ldo enable
	lcdc_send_data(lcdc_dev, 0x33);




	lcdc_send_cmd(lcdc_dev, 0xAf);//DIG_VREFAD_VRDD[2]
	lcdc_send_data(lcdc_dev, 0x55);

	lcdc_send_cmd(lcdc_dev, 0xa6);
	lcdc_send_data(lcdc_dev, 0x2a);
	lcdc_send_data(lcdc_dev, 0x2a);//29
	lcdc_send_cmd(lcdc_dev, 0xa7);
	lcdc_send_data(lcdc_dev, 0x2b);
	lcdc_send_data(lcdc_dev, 0x2b);
	lcdc_send_cmd(lcdc_dev, 0xa8);
	lcdc_send_data(lcdc_dev, 0x18);
	lcdc_send_data(lcdc_dev, 0x18);//17
	lcdc_send_cmd(lcdc_dev, 0xa9);
	lcdc_send_data(lcdc_dev, 0x2a);
	lcdc_send_data(lcdc_dev, 0x2a);
	//======================gamma===========================
	//-----display window 240X320---------//
	lcdc_send_cmd(lcdc_dev, 0x2a);
	lcdc_send_data(lcdc_dev, 0x00);
	lcdc_send_data(lcdc_dev, 0x00);
	lcdc_send_data(lcdc_dev, 0x00);
	lcdc_send_data(lcdc_dev, 0xef);
	lcdc_send_cmd(lcdc_dev, 0x2b);
	lcdc_send_data(lcdc_dev, 0x00);
	lcdc_send_data(lcdc_dev, 0x00);
	lcdc_send_data(lcdc_dev, 0x01);
	lcdc_send_data(lcdc_dev, 0x3f);
	lcdc_send_cmd(lcdc_dev, 0x2c);
	//--------end display window --------------//

	lcdc_send_cmd(lcdc_dev, 0x35);
	lcdc_send_data(lcdc_dev, 0x00);
	lcdc_send_cmd(lcdc_dev, 0x44);
	lcdc_send_data(lcdc_dev, 0x00);
	lcdc_send_data(lcdc_dev, 0x0a);
	lcdc_send_cmd(lcdc_dev, 0xf0);
	lcdc_send_data(lcdc_dev, 0x2);
	lcdc_send_data(lcdc_dev, 0x0);
	lcdc_send_data(lcdc_dev, 0x0);
	lcdc_send_data(lcdc_dev, 0x0);
	lcdc_send_data(lcdc_dev, 0x0);
	lcdc_send_data(lcdc_dev, 0x4);
	lcdc_send_cmd(lcdc_dev, 0xf1);
	lcdc_send_data(lcdc_dev, 0x1);
	lcdc_send_data(lcdc_dev, 0x2);
	lcdc_send_data(lcdc_dev, 0x0);
	lcdc_send_data(lcdc_dev, 0x5);
	lcdc_send_data(lcdc_dev, 0x1a);
	lcdc_send_data(lcdc_dev, 0x15);
	lcdc_send_cmd(lcdc_dev, 0xf2);
	lcdc_send_data(lcdc_dev, 0x6);
	lcdc_send_data(lcdc_dev, 0x6);
	lcdc_send_data(lcdc_dev, 0x20);
	lcdc_send_data(lcdc_dev, 0x5);
	lcdc_send_data(lcdc_dev, 0x5);
	lcdc_send_data(lcdc_dev, 0x31);//2d
	lcdc_send_cmd(lcdc_dev, 0xf3);
	lcdc_send_data(lcdc_dev, 0x15);
	lcdc_send_data(lcdc_dev, 0xb);
	lcdc_send_data(lcdc_dev, 0x55);
	lcdc_send_data(lcdc_dev, 0x2);
	lcdc_send_data(lcdc_dev, 0x2);
	lcdc_send_data(lcdc_dev, 0x65);//6e
	lcdc_send_cmd(lcdc_dev, 0xf4);
	lcdc_send_data(lcdc_dev, 0xE);
	lcdc_send_data(lcdc_dev, 0x1c);
	lcdc_send_data(lcdc_dev, 0x1a);
	lcdc_send_data(lcdc_dev, 0x3);//5
	lcdc_send_data(lcdc_dev, 0x5);//3
	lcdc_send_data(lcdc_dev, 0xF);
	lcdc_send_cmd(lcdc_dev, 0xf5);
	lcdc_send_data(lcdc_dev, 0x6);
	lcdc_send_data(lcdc_dev, 0x13);
	lcdc_send_data(lcdc_dev, 0x15);
	lcdc_send_data(lcdc_dev, 0x33);//32
	lcdc_send_data(lcdc_dev, 0x31);
	lcdc_send_data(lcdc_dev, 0xF);

	//====================end gamma=========================	
	if(sys_get_reboot_cmd() == REBOOT_CMD_NONE) {
		if(sd_fmap(BOOT_LOGO_BMP,(void**)&logo_image,NULL)) {
			printk("boot logo not find \n");
		}
	
		panel_draw_frame(0, 0, 320, 240, (short *)(logo_image + 32), 240);
	}
	panel_display_on();
}


static int gc9306_320x240_panel_init(struct device *dev)
{
	ARG_UNUSED(dev);
	struct display_device * device = NULL;

	printk("%s %d:\n", __func__, __LINE__);

	lcdc_dev = lcdc_get_device();

	panel_init();

	device = display_device_register(&display);
	if (device) {
		display_hw_function_register(device, HW_FUNC_DRAW_PIXEL, panel_draw_single_pixel);
		display_hw_function_register(device, HW_FUNC_DRAW_LINE, panel_draw_single_line);
		display_hw_function_register(device, HW_FUNC_DRAW_FRAME, panel_draw_frame);
		display_hw_function_register(device, HW_FUNC_FILL_FRAME, panel_fill_frame);
		display_hw_function_register(device, HW_FUNC_DISPLAY_ON, panel_display_on);
		display_hw_function_register(device, HW_FUNC_DISPLAY_OFF, panel_display_off);
	}
	return 0;
}

SYS_INIT(gc9306_320x240_panel_init, POST_KERNEL, 80);
