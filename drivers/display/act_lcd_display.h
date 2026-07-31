/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * LCD controller interface
 */

#include <kernel.h>
#include <device.h>

#ifndef __ACT_LCD_DISPLAY__
#define __ACT_LCD_DISPLAY__

struct lcdc_config
{
	u8_t bus_width;
	u8_t bus_mode;
	u8_t color_mode;
	u8_t reserved;
};

void lcdc_send_cmd(struct device *dev, u8_t cmd);
void lcdc_send_data(struct device *dev, u8_t data);
void lcdc_send_pixel_data(struct device *dev, u16_t data);
void lcdc_send_data_by_dma(struct device *dev, uint16_t *data, int len);
void spi_send_data_by_dma(struct device *dev, uint8_t *data, uint32_t len);
void lcdc_config(struct device *dev, const struct lcdc_config *cfg);
int lcdc_get_data(struct device *dev);
struct device *lcdc_get_device(void);

#endif
