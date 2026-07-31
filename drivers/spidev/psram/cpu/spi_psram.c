/********************************************************************************
 *                            USDK(US283A_master)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2018年1月11日-下午7:28:51             1.0             build this file
 ********************************************************************************/
/*!
 * \file     spi_psram.c
 * \brief	 使用cpu模式访问PSRAM
 * \author
 * \version  1.0
 * \date  2018年1月11日-下午7:28:51
 *******************************************************************************/
#include <sdk.h>
#include <phy_spi.h>
#include <spi/dev_spi.h>
#include <storage/dev_storage.h>
#include "spi_psram.h"
#include <phy_gpio.h>
#include <gpio/dev_gpio.h>

static spi_xfer_instance_t psram_xfer_instance;

static storage_priv_data_t storage_psram;

static const struct acts_pin_config psram_io_config[] =
{
    { 11, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3)},
    { 10, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3) },
    { 9, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3) },
    { 8, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3) },
};

spi_dev_cfg_t psram_dev_cfg =
{
	//xfer instance
	&psram_xfer_instance,

    SPI_DEFAULT_CS_PIN_NUM,

	//cs delay
	0,

	//mode
	SPIDEV_MODE3,

	//delaychain
	8,

	//MHZ
	24,
};

void psram_reset(spi_dev_t *psram_dev)
{
	spi_dev_write_cmd(psram_dev, PSRAM_CMD_RESET);

	pr_info("psram reset\n");
}

void psram_read_id(spi_dev_t *psram_dev)
{
    uint8 chipid[8];

    spi_dev_read_chipid(psram_dev, &chipid, 8);

    print_hex("psram id: ", chipid, 8);
}


int32 storage_psram_init(struct device *device, void *arg)
{
    static spi_dev_t *psram_dev;

    spi_host_io_param_t io_param;

    storage_priv_data_t *priv_data = (storage_priv_data_t *)device->driver_data;

    io_param.is_master = TRUE;
    io_param.max_speed_mhz = 100;

    io_param.pin_config = psram_io_config;
    io_param.pins = ARRAY_SIZE(psram_io_config);

    psram_dev = malloc(sizeof(spi_dev_t));

    psram_dev->host = spi_get_host_instance(1);

    psram_dev->dev_cfg = (spi_dev_cfg_t *)&psram_dev_cfg;

    psram_dev->xfer = spi_dev_xfer_data;

    psram_dev->read = spi_dev_read_data;

    psram_dev->write = spi_dev_write_data;

    psram_dev->addr_line = 1;
    psram_dev->cmd_line = 1;
    psram_dev->dummy_line = 1;
    psram_dev->data_line = 1;
    psram_dev->dummy_count = 1;
    psram_dev->read_cmd = SPI_DEV_CMD_FAST_READ;
    psram_dev->write_cmd = SPI_DEV_CMD_PP;

	psram_dev->host->api->init(psram_dev->host, &io_param);

	psram_dev->dev_cfg->xfer->xfer_handle = dma_alloc();

	priv_data->dev = (void *)psram_dev;

	psram_reset(psram_dev);

	psram_read_id(psram_dev);

	return 0;
}

int32 storage_psram_read(struct device *storage_dev, uint32 lba, void *buffer, uint32 len)
{
    storage_priv_data_t *priv_data = (storage_priv_data_t *)(storage_dev->driver_data);
	spi_dev_read((spi_dev_t *)priv_data->dev, lba << 9, buffer, len << 9, SPI_DEV_TFLAG_XFER_DMA);
	return 0;
}

int32 storage_psram_write(struct device *storage_dev, uint32 lba, void *buffer, uint32 len)
{
    storage_priv_data_t *priv_data = (storage_priv_data_t *)(storage_dev->driver_data);

	spi_dev_write((spi_dev_t *)priv_data->dev, lba << 9, buffer, len << 9, SPI_DEV_TFLAG_XFER_DMA);

	return 0;
}

const static device_config_t storage_psram_cfg =
{
	.drv_name = dev_name_psram,
    .init = storage_psram_init,
    .exit = NULL,
    .power_control = NULL,
    .detect = NULL,
    .mount = NULL,
    .config_info = NULL,
};

const static storage_device_api_t storage_psram_api =
{
    .storage_read = storage_psram_read,
    .storage_write = storage_psram_write,
    .storage_erase = NULL,
    .storage_ioctl = NULL,
    .storage_byte_read = NULL,
    .storage_byte_write = NULL,
};

struct device DEVICE_INFO_SECTION device_psram =
{
    .config = (device_config_t *)&storage_psram_cfg,
    .driver_api = (const void *)&storage_psram_api,
    .driver_data = (void *)&storage_psram,
};

