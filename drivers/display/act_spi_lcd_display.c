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
#include <logging/sys_log.h>
#include <zephyr.h>
#include <init.h>
#include <board.h>
#include <gpio.h>
#include <device.h>
#include <string.h>
#include <misc/printk.h>
#include <dma.h>
#include "act_lcd_display.h"
#include "../spi/spi_context.h"

#include <spi.h>

#define DATA_FORMAT_8BIT 0
#define DATA_FORMAT_16BIT 1

#define LCD_DMA_MODE 0x03
#define LCD_EXT_MODE 0x04

#define DATA_FORMAT DATA_FORMAT_8BIT

/* lcd controller */
struct acts_lcd_controller {
	volatile u32_t ctl;
	volatile u32_t clk_ctl;
	volatile u32_t extmem_ctl;
	volatile u32_t extmem_clk_ctl;
	volatile u32_t extmem_dat;
	volatile u32_t lcd_if;
};

struct acts_lcd_config {
	struct acts_lcd_controller *lcdc;
	u8_t dma_id;
	u8_t dma_chan;
};
struct spi_config st7789v_spi_master_conf = {
	.frequency = 80000000,
	.operation = (SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_WORD_SET(8) |
		     	SPI_LINES_SINGLE | SPI_MODE_CPOL | SPI_MODE_CPHA),
		      //SPI_LINES_SINGLE),
	.vendor    = 0,
	.slave     = 0,
	.cs        = NULL,
};

void  lcdc_set_mode(int mode, int data_format)
{
	int i;
	int value = 0;
	static u8_t lcd_mode = 0;

	if (lcd_mode == mode)
		return;

	for (i = 0; i < 4; i++) {
		value = sys_read32(GPIO_REG_CTL(GPIO_REG_BASE, (i + 14)));
		value = (value & (~(0x0000000F))) | mode;
		sys_write32(value, GPIO_REG_CTL(GPIO_REG_BASE, (i + 14)));
	}

	for (i = 0; i < 8; i++) {
		value = sys_read32(GPIO_REG_CTL(GPIO_REG_BASE, (i + 20)));
		value = (value & (~(0x0000000F))) | mode;
		sys_write32(value, GPIO_REG_CTL(GPIO_REG_BASE, i + 20));
	}

	lcd_mode = mode;
}

void lcdc_send_cmd(struct device *dev, u8_t cmd)
{
	struct device *gpio_dev = NULL;

	gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	u8_t buf[1] = {0};

	buf[0]=cmd;
	struct spi_buf spi_bufs = {buf, 1};
	u8_t buf1[4] = {0};
	struct spi_buf spi_bufs1 = {buf1, 4};

	st7789v_spi_master_conf.dev = dev;
	gpio_pin_write(gpio_dev, BOARD_PANEL_ST7789V_DC,0);

	if(0x04 == cmd) {
		if (spi_transceive(&st7789v_spi_master_conf, &spi_bufs, 1,
				&spi_bufs1, 1) != 0) {
			printk("master io error\n");
			return ;
		}
		print_buffer(buf1, 1, sizeof(buf), 16, 0);
	} else {
		if (spi_transceive(&st7789v_spi_master_conf, &spi_bufs, 1,
				NULL, 0) != 0) {
			printk("master io error\n");
			return ;
		}
	}

	/* idle level high */
	gpio_pin_write(gpio_dev, BOARD_PANEL_ST7789V_DC, 1);

}

void lcdc_send_data(struct device *dev, u8_t temp_data)
{
	struct device *gpio_dev = NULL;

	gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	u8_t buf[1] = {0};
	buf[0] = temp_data;
	struct spi_buf spi_bufs = {buf, 1};

	st7789v_spi_master_conf.dev = dev;
	gpio_pin_write(gpio_dev, BOARD_PANEL_ST7789V_DC, 1);

	if (spi_transceive(&st7789v_spi_master_conf, &spi_bufs, 1,
			NULL, 0) != 0) {
		printk("master io error\n");

		return ;
	}
}

void lcdc_send_pixel_data(struct device *dev, u16_t temp_data)
{
	u8_t buf[2] = {0};

	buf[0] = (u8_t)(temp_data>>8);
	buf[1] = (u8_t)temp_data;
	struct spi_buf spi_bufs = {buf, 2};

	st7789v_spi_master_conf.dev = dev;

	if (spi_transceive(&st7789v_spi_master_conf, &spi_bufs, 1,
			NULL, 0) != 0) {
		printk("master io error\n");

		return ;
	}
}
void spi_send_data_by_dma(struct device *dev, uint8_t *data, uint32_t len)
{
	struct device *gpio_dev = NULL;

	gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	struct spi_buf spi_bufs = {data, len};

	st7789v_spi_master_conf.dev = dev;

	gpio_pin_write(gpio_dev, BOARD_PANEL_ST7789V_DC, 1);

	if (spi_transceive(&st7789v_spi_master_conf, &spi_bufs, 1,
			NULL, 0) != 0) {
		printk("master io error\n");

		return ;
	}
}

static void lcd_config_dma(struct device *dev, uint16_t *data, u32_t data_cnt)
{
	const struct acts_lcd_config *cfg = dev->config->config_info;
	struct dma_config dma_cfg = {0};
	struct dma_block_config dma_block_cfg = {0};
	struct device *dma_dev = device_get_binding(CONFIG_DMA_0_NAME);

	dma_cfg.channel_direction = MEMORY_TO_PERIPHERAL;
	dma_cfg.dma_slot = cfg->dma_id;

	/* 16bit width data */
	dma_cfg.source_data_size = 2;

	/* we use pcm irq to track the transfer, so no need dma irq callback */
	dma_cfg.dma_callback = NULL;

	dma_cfg.block_count = 1;
	dma_cfg.head_block = &dma_block_cfg;

	dma_block_cfg.block_size = data_cnt;
	dma_block_cfg.source_address = (u32_t)data;
	dma_block_cfg.dest_address = 0xc0180010;

	dma_config(dma_dev, cfg->dma_chan, &dma_cfg);
}
void lcdc_send_data_by_dma(struct device *dev, uint16_t *data, int len)
{
	struct device *dma_dev = device_get_binding(CONFIG_DMA_0_NAME);
	const struct acts_lcd_config *cfg = dev->config->config_info;
	struct acts_lcd_controller *lcdc = cfg->lcdc;

	lcdc_set_mode(LCD_DMA_MODE, DATA_FORMAT);

	lcdc->ctl &= 0xfffffffe;
	lcdc->ctl |= 0x1;

	lcd_config_dma(dev, data, len);

	dma_start(dma_dev, cfg->dma_chan);

	dma_wait_finished(dma_dev, cfg->dma_chan);

	//lcdc_set_mode(LCD_EXT_MODE, DATA_FORMAT);
}

