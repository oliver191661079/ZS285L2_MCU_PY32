/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief SPI PSRAM driver for Actions SoC
 */

#include <errno.h>
#include <kernel.h>
#include <init.h>
#include <string.h>
#include <linker/sections.h>
#include <gpio.h>
#include <flash.h>
#include <soc.h>
#include <board.h>

#define SYS_LOG_DOMAIN "spipsram"
#define SYS_LOG_LEVEL SYS_LOG_LEVEL_INFO
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_XSPI_PSRAM_CACHE_ACTS

struct xspi_psram_info {
	u32_t base_addr;
};

static int total_size = 0;
static int xspi_psram_acts_read(struct device *dev, off_t addr,
			      void *data, size_t len)
{
	struct xspi_psram_info *psram = (struct xspi_psram_info *)dev->driver_data;

	SYS_LOG_DBG("read addr 0x%x, size 0x%x", addr, len);

	if (!len)
		return 0;

	memcpy(data, (void *)(psram->base_addr + addr), len);

	return 0;
}

static int xspi_psram_acts_write(struct device *dev, off_t addr,
			       const void *data, size_t len)
{
	struct xspi_psram_info *psram = (struct xspi_psram_info *)dev->driver_data;

	SYS_LOG_DBG("write addr 0x%x, size 0x%x", addr, len);

	if (!len)
		return 0;

	memcpy((void *)(psram->base_addr + addr), data, len);

	if (addr + len > total_size)
		total_size = addr + len;

	return 0;
}


static int xspi_psram_acts_erase(struct device *dev, off_t addr, size_t len)
{
	struct xspi_psram_info *psram = (struct xspi_psram_info *)dev->driver_data;

	SYS_LOG_DBG("erase addr 0x%x, size 0x%x", addr, len);

	if (!len)
		return 0;

	memset((void *)(psram->base_addr + addr), 0xff, len);

	return 0;
}

static int  xspi_psram_acts_ioctl(struct device *dev, uint8_t cmd, void *buff)
{
	SYS_LOG_DBG("ioctl cmd %d", cmd);
	if (cmd == 0x01) {
		SYS_LOG_INF("psram cmd total size %d", total_size);
		*(int *)buff = total_size;
	}
	return 0;
}

static const struct flash_driver_api xspi_psram_acts_driver_api = {
	.read = xspi_psram_acts_read,
	.write = xspi_psram_acts_write,
	.erase = xspi_psram_acts_erase,
	.ioctl = xspi_psram_acts_ioctl,
};

static int xspi_psram_acts_init(struct device *dev)
{
	SYS_LOG_INF("init xspi_psram");

	dev->driver_api = &xspi_psram_acts_driver_api;

	return 0;
}

static struct xspi_psram_info system_xspi_psram = {
	.base_addr = CONFIG_SOC_MAPPING_PSRAM_ADDR + 0x40000,
};


DEVICE_AND_API_INIT(xspi_psram_acts, CONFIG_SPI_PSRAM_CACHE_DEV_NAME, xspi_psram_acts_init,
	     &system_xspi_psram, NULL, POST_KERNEL,
	     10, &xspi_psram_acts_driver_api);