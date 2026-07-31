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

#ifdef CONFIG_XSPI_PSRAM_ROM_ACTS
#include <soc_rom_xspi.h>
#else
#include "xspi_mem_acts.h"
#endif

#define SYS_LOG_DOMAIN "spipsram"
#define SYS_LOG_LEVEL SYS_LOG_LEVEL_INFO
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_XSPI_PSRAM_ACTS

/* set psram page to 256 to satisfy tCEM (max 8us) requirement */
#define PSRAM_PAGE_SIZE			256

/* SPI PSRAM commands */
#define	SPISRAM_CMD_READ_CHIPID		0x9f	/* JEDEC ID */

#define	SPISRAM_CMD_READ		0x0b	/* fast read page by 1x */
#define	SPISRAM_CMD_READ_X4		0xeb	/* fast read page by 4x */

#define	SPISRAM_CMD_WRITE		0x02	/* write data to page by 1x */
#define	SPISRAM_CMD_WRITE_X4		0x38	/* write data to page by 4x */

#define	SPISRAM_CMD_ENTER_QE		0x35	/* enter qual enable mode */
#define	SPISRAM_CMD_EXIT_QE		0xf5	/* exit qual enable mode */

#define	SPISRAM_CMD_RESET_ENABLE	0x66	/* reset enable command */
#define	SPISRAM_CMD_RESET		0x99	/* reset command  */

struct spi_psram_chip {
	const char *name;
	u32_t chipid;
	u32_t size;
	u32_t flag;
};

struct xspi_psram_info {
#ifdef CONFIG_XSPI_PSRAM_ROM_ACTS
	struct xspi_rom_info spi;
	rom_spimem_trans_func_t transfer;
#else
	struct xspi_info spi;
#endif

	struct device *gpio_dev;
	const struct spi_psram_chip *chip;
};

static const struct spi_psram_chip spi_psram_chips_tbl[] = {
	/* ATP1008 */
	{"ATP1008", 0x5d0d, 0x800000, 0},
	{NULL, 0, 0, 0},
};

static int xspi_psram_send_cmd(struct xspi_psram_info *psram, u8_t cmd, u32_t addr,
			s32_t addr_len, void *buf, s32_t length,
			u8_t dummy_len, u32_t flag)
{
#ifdef CONFIG_XSPI_PSRAM_ROM_ACTS
	return psram->transfer(&psram->spi, cmd, addr, addr_len, buf, length,
			       dummy_len, flag);
#else
	return xspi_mem_transfer(&psram->spi, cmd, addr, addr_len, buf, length,
				 dummy_len, flag);
#endif
}

#ifdef CONFIG_XSPI_PSRAM_ROM_ACTS
__ramfunc static void xspi_psram_set_cs(struct xspi_rom_info *si, int value)
#else
__ramfunc static void xspi_psram_set_cs(struct xspi_info *si, int value)
#endif
{
	/* FIXME: must run in ram, so we cannot call gpio driver */
	if (value)
		/* output high level */
		sys_write32(GPIO_BIT(si->cs_gpio),
			    GPIO_REG_BSR(GPIO_REG_BASE, si->cs_gpio));
	else
		/* output low level */
		sys_write32(GPIO_BIT(si->cs_gpio),
			    GPIO_REG_BRR(GPIO_REG_BASE, si->cs_gpio));
}

static int xspi_psram_read_chipid(struct xspi_psram_info *psram, u8_t *buf, int len)
{
	return xspi_psram_send_cmd(psram, SPISRAM_CMD_READ_CHIPID, 0, 3, buf,
			    len, 0, 0);
}

static void xspi_psram_enable_qe(struct xspi_psram_info *psram)
{
	if (psram->spi.bus_width == 4) {
		xspi_psram_send_cmd(psram, SPISRAM_CMD_ENTER_QE, 0, 0,
			NULL, 0, 0, 0);
	}
}

static void xspi_psram_disable_qe(struct xspi_psram_info *psram)
{
	if (psram->spi.bus_width == 4) {
		xspi_psram_send_cmd(psram, SPISRAM_CMD_EXIT_QE, 0, 0,
			NULL, 0, 0, XSPI_MEM_TFLAG_MIO_CMD_ADDR_DATA);
	}
}

static void xspi_psram_reset(struct xspi_psram_info *psram)
{
	xspi_psram_disable_qe(psram);

	xspi_psram_send_cmd(psram, SPISRAM_CMD_RESET_ENABLE, 0, 0,
		NULL, 0, 0, 0);

	xspi_psram_send_cmd(psram, SPISRAM_CMD_RESET, 0, 0,
		NULL, 0, 0, 0);
}

static int xspi_psram_read(struct xspi_psram_info *psram, u32_t addr,
		u8_t *buf, u32_t len)
{
	u8_t cmd, dummy_len;
	u32_t flag;

	SYS_LOG_DBG("read addr 0x%u, buf %p, len 0x%x",
		addr, buf, len);

	if (psram->spi.bus_width == 4) {
		cmd = SPISRAM_CMD_READ_X4;
		dummy_len = 3;
		flag = XSPI_MEM_TFLAG_MIO_CMD_ADDR_DATA;
	} else {
		cmd = SPISRAM_CMD_READ;
		dummy_len = 1;
		flag = 0;
	}

	xspi_psram_send_cmd(psram, cmd, addr, 3, buf, len, dummy_len, flag);

	return 0;
}

int xspi_psram_write(struct xspi_psram_info *psram, u32_t addr,
		const void *buf, uint16_t len)
{
	u32_t flag;
	u8_t cmd;

	SYS_LOG_DBG("write addr 0x%u, buf %p, len 0x%x",
		addr, buf, len);

	flag = XSPI_MEM_TFLAG_WRITE_DATA;
	if (psram->spi.bus_width == 4) {
		cmd = SPISRAM_CMD_WRITE_X4;
		flag |= XSPI_MEM_TFLAG_MIO_CMD_ADDR_DATA;
	} else {
		cmd = SPISRAM_CMD_WRITE;
	}

	xspi_psram_send_cmd(psram, cmd, addr, 3, (void *)buf, len, 0, flag);

	return 0;
}

void xspi_psram_init_internal(struct xspi_psram_info *psram)
{
	if (psram->spi.bus_width == 4) {
		xspi_psram_enable_qe(psram);
	}
}

static int xspi_psram_acts_read(struct device *dev, off_t addr,
			      void *data, size_t len)
{
	struct xspi_psram_info *psram = (struct xspi_psram_info *)dev->driver_data;
	u32_t addr_in_page;
	size_t remaining, read_size;

	ACT_LOG_ID_DBG(ALF_STR_xspi_psram_acts_read__READ_ADDR_0XX_SIZE_0, 2, addr, len);

	if (!len)
		return 0;

	remaining = len;
	while (remaining > 0) {
		addr_in_page = addr % PSRAM_PAGE_SIZE;
		if (remaining > (PSRAM_PAGE_SIZE - addr_in_page))
			read_size = (PSRAM_PAGE_SIZE - addr_in_page);
		else
			read_size = remaining;

		xspi_psram_read(psram, addr, data, read_size);

		addr += read_size;
		data += read_size;
		remaining -= read_size;
	}

	return 0;
}

static int xspi_psram_acts_write(struct device *dev, off_t addr,
			       const void *data, size_t len)
{
	struct xspi_psram_info *psram = (struct xspi_psram_info *)dev->driver_data;
	u32_t addr_in_page;
	size_t remaining, write_size;

	ACT_LOG_ID_DBG(ALF_STR_xspi_psram_acts_write__WRITE_ADDR_0XX_SIZE_, 2, addr, len);

	if (!len)
		return 0;

	remaining = len;
	while (remaining > 0) {
		addr_in_page = addr % PSRAM_PAGE_SIZE;
		if (remaining > (PSRAM_PAGE_SIZE - addr_in_page))
			write_size = (PSRAM_PAGE_SIZE - addr_in_page);
		else
			write_size = remaining;

		xspi_psram_write(psram, addr, data, write_size);

		addr += write_size;
		data += write_size;
		remaining -= write_size;
	}

	return 0;
}

static const struct flash_driver_api xspi_psram_acts_driver_api = {
	.read = xspi_psram_acts_read,
	.write = xspi_psram_acts_write,
};

static const struct spi_psram_chip *spi_psram_match_chip(u32_t chipid)
{
	const struct spi_psram_chip *chip = &spi_psram_chips_tbl[0];

	while (chip->chipid != 0) {
		if (chip->chipid == chipid)
			return chip;

		chip++;
	}

	return NULL;
}

static int xspi_psram_acts_init(struct device *dev)
{
	struct xspi_psram_info *psram = (struct xspi_psram_info *)dev->driver_data;
	u32_t chipid = 0;

	SYS_LOG_INF("init xspi_psram");

	psram->gpio_dev = device_get_binding(BOARD_SPI_PSRAM_CS_GPIO_NAME);
	if (!psram->gpio_dev) {
		SYS_LOG_ERR("cannot found GPIO dev: %s", BOARD_SPI_PSRAM_CS_GPIO_NAME);
		return -ENODEV;
	}

	/* output high level by default */
	gpio_pin_configure(psram->gpio_dev, BOARD_SPI_PSRAM_CS_GPIO,
		GPIO_DIR_OUT | GPIO_POL_INV | GPIO_PUD_PULL_UP);

	xspi_psram_reset(psram);

	xspi_psram_read_chipid(psram, (u8_t *)&chipid, 2);
	psram->chip = spi_psram_match_chip(chipid);
	if (!psram->chip) {
		SYS_LOG_ERR("unsupport PSRAM chip(id:0x%x)", chipid);
		return -ENODEV;
	}

	SYS_LOG_INF("Found PSRAM chip: %s (id:%x)", psram->chip->name, chipid);

	xspi_psram_init_internal(psram);

	dev->driver_api = &xspi_psram_acts_driver_api;

	return 0;
}

#ifdef CONFIG_XSPI_PSRAM_ROM_ACTS

extern void system_nor_prepare_hook(struct xspi_rom_info *);

static struct xspi_psram_info system_xspi_psram = {
	.spi = {
		.base = SPI0_REG_BASE,
		.delay_chain = 6,
		.bus_width = CONFIG_SPI_PSRAM_IO_BUS_WIDTH,
		.flag = 0,
		.prepare_hook = system_nor_prepare_hook,
		.cs_gpio = BOARD_SPI_PSRAM_CS_GPIO,
		.set_cs = xspi_psram_set_cs,
	},
	.transfer = (rom_spimem_trans_func_t)XSPI_ROM_ACTS_API_TRANSFER_ADDR,
};
#else
static struct xspi_psram_info system_xspi_psram = {
	.spi = {
		.base = SPI0_REG_BASE,
		.delay_chain = 6,
		.bus_width = CONFIG_SPI_PSRAM_IO_BUS_WIDTH,
		.flag = 0,
		.dma_chan = -1,
		.cs_gpio = BOARD_SPI_PSRAM_CS_GPIO,
		.set_cs = xspi_psram_set_cs,
	},
};
#endif

DEVICE_INIT(xspi_psram_acts, CONFIG_SPI_PSRAM_DEV_NAME,
	    xspi_psram_acts_init, &system_xspi_psram, NULL,
	    POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);
