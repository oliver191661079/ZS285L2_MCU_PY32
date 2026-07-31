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
#include <misc/printk.h>
#include <dma.h>
#include "act_lcd_display.h"

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

static struct device *lcdc_dev;

void  lcdc_set_mode(int mode, int data_format)
{
	int i;
	int value = 0;
	static u8_t lcd_mode = 0;

	if (lcd_mode == mode) return;

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

void lcdc_send_cmd(struct device *dev, int cmd)
{
	const struct acts_lcd_config *cfg = dev->config->config_info;
	struct acts_lcd_controller *lcdc = cfg->lcdc;
	lcdc_set_mode(LCD_EXT_MODE, DATA_FORMAT);
	lcdc->extmem_ctl &= ~(1 << 0);
	lcdc->extmem_dat = cmd;
	lcdc->extmem_ctl |= (1 << 0);
}

void lcdc_send_data(struct device *dev, u8_t data)
{
	const struct acts_lcd_config *cfg = dev->config->config_info;
	struct acts_lcd_controller *lcdc = cfg->lcdc;
	lcdc->extmem_ctl |= (1 << 0);
	lcdc->extmem_dat = data;
	lcdc->extmem_ctl &= ~(1 << 0);
}

void lcdc_send_pixel_data(struct device *dev, u16_t data)
{
	const struct acts_lcd_config *cfg = dev->config->config_info;
	struct acts_lcd_controller *lcdc = cfg->lcdc;
	lcdc->extmem_ctl |= (1 << 0);
	lcdc->extmem_dat = data >> 8;
	lcdc->extmem_ctl &= ~(1 << 0);

	lcdc->extmem_ctl |= (1 << 0);
	lcdc->extmem_dat = data;
	lcdc->extmem_ctl &= ~(1 << 0);
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

int lcdc_get_data(struct device *dev)
{
	const struct acts_lcd_config *cfg = dev->config->config_info;
	struct acts_lcd_controller *lcdc = cfg->lcdc;
	int data;

	lcdc_set_mode(LCD_EXT_MODE, DATA_FORMAT);
	lcdc->extmem_ctl |= (1 << 0);
	data = lcdc->extmem_dat;
	lcdc->extmem_ctl &= ~(1 << 0);

	return data;
}

void lcdc_config(struct device *dev, const struct lcdc_config *cfg)
{
}

struct device *lcdc_get_device(void)
{
	return lcdc_dev;
}

static int lcd_acts_init(struct device *dev)
{
	const struct acts_lcd_config *cfg = dev->config->config_info;
	struct acts_lcd_controller *lcdc = cfg->lcdc;
	int i = 0;
	lcdc_dev = dev;

	acts_clock_peripheral_enable(CLOCK_ID_LCD);
	sys_write32(1 << 4 | 0x01, CMU_LCDCLK);
	acts_reset_peripheral(RESET_ID_LCD);

	/* data format: RGB */
	lcdc->extmem_ctl = (1 << 29) | (DATA_FORMAT <<8);

#if CONFIG_SOC_CPU_CLK_FREQ == 216000
	lcdc->clk_ctl = 0x030303;
	lcdc->extmem_clk_ctl = 0x030303;
#else
	lcdc->clk_ctl = 0x020202;
	lcdc->extmem_clk_ctl = 0x020202;
#endif

	lcdc->ctl = 0x1 | (0 << 1) | (0 <<2) | (0 <<3) | (DATA_FORMAT << 4) | (1 << 7);

	printk("lcdc->extmem_ctl 0x%x \n",lcdc->extmem_ctl);
	printk("lcdc->clk_ctl 0x%x \n",lcdc->clk_ctl);
	printk("lcdc->extmem_clk_ctl 0x%x \n",lcdc->extmem_clk_ctl);
	printk("lcdc->ctl 0x%x \n",lcdc->ctl);
	printk("0xc0090000 0x%x \n",sys_read32(0xc0090000));
	printk("CMU_MEMCLKEN 0x%x \n",sys_read32(CMU_MEMCLKEN));
	printk("CMU_MEMCLKSEL 0x%x \n",sys_read32(CMU_MEMCLKSEL));
    printk("CMU_SYSCLK 0x%x \n",sys_read32(CMU_SYSCLK));
	printk("CORE_PLL_CTL 0x%x \n",sys_read32(CORE_PLL_CTL));

	for(i = 0; i < 14; i++)
	{
		printk("GPIO%d_CTL 0x%x\n",14 + i,sys_read32(0xc0090044+i*4));
	}

	return 0;
}

static const struct acts_lcd_config lcd_acts_config = {
	.lcdc = (struct acts_lcd_controller *) 0xc0180000,
	.dma_id = DMA_ID_LCD,
	.dma_chan = 1,
};

DEVICE_INIT(cpu_lcd, CONFIG_LCD_DEV_NAME, lcd_acts_init,
	    NULL, &lcd_acts_config,
	    POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);

