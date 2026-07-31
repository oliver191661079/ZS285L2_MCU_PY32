/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief PSRAM memory driver
 */

#include <errno.h>
#include <flash.h>
#include <spi.h>
#include <init.h>
#include <string.h>
#include <misc/byteorder.h>

#define SYS_LOG_DOMAIN "psram"
#define SYS_LOG_LEVEL SYS_LOG_LEVEL_INFO
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_SPI_PSRAM

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

#define	SPISRAM_CMD_RESET_ENABLE	0x66	/* enable reset */
#define	SPISRAM_CMD_RESET		0x99	/* reset */

struct spi_psram_chip {
	const char *name;
	u32_t chipid;
	u32_t size;
	u32_t flag;
};

static const struct spi_psram_chip spi_psram_chips_tbl[] = {
	/* ATP1008 */
	{"ATP1008", 0x5d0d, 0x800000, 0},
	{NULL, 0, 0, 0},
};

struct spi_psram_data {
	struct spi_config spi;
	struct k_sem sem;
	const struct spi_psram_chip *chip;
};

static int spi_psram_read_page(struct spi_psram_data *psram, u32_t addr,
			   u8_t *buf, u32_t len)
{
	u8_t cmd_addr[5];
	struct spi_buf spi_out_bufs[1] = {{&cmd_addr, 5}};
	struct spi_buf spi_in_bufs[2] = {{&cmd_addr, 5}, {buf, len}};
	int ret;

	sys_put_be32((SPISRAM_CMD_READ << 24) | addr, cmd_addr);

	ret = spi_transceive(&psram->spi, spi_out_bufs, ARRAY_SIZE(spi_out_bufs),
			     spi_in_bufs, ARRAY_SIZE(spi_in_bufs));
	if (ret) {
		SYS_LOG_ERR("failed to read psram(addr:0x%x, len:0x%x), ret %d",
			    addr, len, ret);

		return -EIO;
	}

	return 0;
}

static int spi_psram_write_page(struct spi_psram_data *psram, u32_t addr,
			   const u8_t *buf, u32_t len)
{
	u8_t cmd_addr[4];
	const struct spi_buf spi_out_bufs[2] = {
		{&cmd_addr, 4},
		{(u8_t *)buf, len}
	};
	int ret;

	sys_put_be32((SPISRAM_CMD_WRITE << 24) | addr, cmd_addr);

	ret = spi_transceive(&psram->spi, spi_out_bufs, ARRAY_SIZE(spi_out_bufs),
			     NULL, 0);
	if (ret) {
		SYS_LOG_ERR("failed to write psram(addr:0x%x, len:0x%x), ret %d",
			    addr, len, ret);

		return -EIO;
	}

	return 0;
}

static inline bool spi_psram_addr_is_valid(struct spi_psram_data *psram,
					   u32_t addr, size_t len)
{
	if (((addr + len) > psram->chip->size) ||
	      addr < 0 || (len <= 0)) {
		return false;
	}

	return true;
}

static int spi_psram_read(struct device *dev, off_t addr,
			  void *data, size_t len)
{
	struct spi_psram_data *psram = dev->driver_data;
	u32_t addr_in_page;
	size_t remaining, read_size;
	int ret;

	ACT_LOG_ID_DBG(ALF_STR_spi_psram_read__READ_ADDR_0XX_SIZE_0, 2, addr, len);

	if (!spi_psram_addr_is_valid(psram, addr, len))
		return -EINVAL;

	k_sem_take(&psram->sem, K_FOREVER);

	remaining = len;
	while (remaining > 0) {
		addr_in_page = addr % PSRAM_PAGE_SIZE;
		if (remaining > (PSRAM_PAGE_SIZE - addr_in_page))
			read_size = (PSRAM_PAGE_SIZE - addr_in_page);
		else
			read_size = remaining;

		ret = spi_psram_read_page(psram, addr, data, read_size);
		if (ret) {
			k_sem_give(&psram->sem);
			return -EIO;
		}

		addr += read_size;
		data += read_size;
		remaining -= read_size;
	}

	k_sem_give(&psram->sem);

	return 0;
}

static int spi_psram_write(struct device *dev, off_t addr,
			       const void *data, size_t len)
{
	struct spi_psram_data *psram = dev->driver_data;
	u32_t addr_in_page;
	size_t remaining, write_size;
	int ret;

	ACT_LOG_ID_DBG(ALF_STR_spi_psram_write__WRITE_ADDR_0XX_SIZE_, 2, addr, len);

	if (!spi_psram_addr_is_valid(psram, addr, len))
		return -EINVAL;

	k_sem_take(&psram->sem, K_FOREVER);

	remaining = len;
	while (remaining > 0) {
		addr_in_page = addr % PSRAM_PAGE_SIZE;
		if (remaining > (PSRAM_PAGE_SIZE - addr_in_page))
			write_size = (PSRAM_PAGE_SIZE - addr_in_page);
		else
			write_size = remaining;

		ret = spi_psram_write_page(psram, addr, data, write_size);
		if (ret) {
			k_sem_give(&psram->sem);
			return -EIO;
		}

		addr += write_size;
		data += write_size;
		remaining -= write_size;
	}

	k_sem_give(&psram->sem);

	return 0;
}

static int spi_psram_read_id(struct spi_psram_data *psram)
{
	u8_t buf[6] = {0};
	struct spi_buf spi_bufs[1] = {{buf, 6}};
	u32_t chipid;
	int ret;

	buf[0] = SPISRAM_CMD_READ_CHIPID;

	ret = spi_transceive(&psram->spi, spi_bufs, ARRAY_SIZE(spi_bufs),
			     spi_bufs, ARRAY_SIZE(spi_bufs));
	if (ret) {
		return -EIO;
	}

	chipid = (buf[5] << 8) | buf[4];

	return chipid;
}

static int spi_psram_reset(struct spi_psram_data *psram)
{
	u8_t buf[1] = {0};
	struct spi_buf spi_bufs[1] = {{buf, 1}};
	int ret;

	buf[0] = SPISRAM_CMD_RESET_ENABLE;

	ret = spi_transceive(&psram->spi, spi_bufs, ARRAY_SIZE(spi_bufs),
			     NULL, 0);
	if (ret) {
		return -EIO;
	}

	buf[0] = SPISRAM_CMD_RESET;

	ret = spi_transceive(&psram->spi, spi_bufs, ARRAY_SIZE(spi_bufs),
			     NULL, 0);
	if (ret) {
		return -EIO;
	}

	return 0;
}

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

static const struct flash_driver_api spi_psram_api = {
	.read = spi_psram_read,
	.write = spi_psram_write,
};

static int spi_psram_init(struct device *dev)
{
	struct spi_psram_data *psram = dev->driver_data;
	int chipid;

	ACT_LOG_ID_DBG(ALF_STR_spi_psram_init__INIT_PSRAM_CHIP, 0);

	psram->spi.dev = device_get_binding(CONFIG_SPI_PSRAM_SPI_NAME);
	if (!psram->spi.dev) {
		SYS_LOG_ERR("cannot found spi device %s", CONFIG_SPI_PSRAM_SPI_NAME);
		return -EIO;
	}

	psram->spi.operation = SPI_WORD_SET(8) | SPI_MODE_CPOL | SPI_MODE_CPHA;
	psram->spi.frequency = CONFIG_SPI_PSRAM_SPI_FREQ;

	k_sem_init(&psram->sem, 1, UINT_MAX);

	spi_psram_reset(psram);

	chipid = spi_psram_read_id(psram);
	psram->chip = spi_psram_match_chip(chipid);
	if (!psram->chip) {
		SYS_LOG_ERR("unsupport PSRAM chip(id:0x%x)", chipid);
		return -ENODEV;
	}

	SYS_LOG_INF("Found PSRAM chip: %s (id:%x)", psram->chip->name, chipid);

	dev->driver_api = &spi_psram_api;

	return 0;
}

static struct spi_psram_data spi_psram_memory_data;

DEVICE_INIT(spi_psram_memory, CONFIG_SPI_PSRAM_DEV_NAME, spi_psram_init,
	    &spi_psram_memory_data, NULL, POST_KERNEL,
	    CONFIG_KERNEL_INIT_PRIORITY_DEVICE);
