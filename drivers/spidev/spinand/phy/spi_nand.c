/********************************************************************************
 *                            USDK(US283A_master)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2018年1月12日-下午5:23:23             1.0             build this file
 ********************************************************************************/
/*!
 * \file     spi_nand.c
 * \brief
 * \author
 * \version  1.0
 * \date  2018年1月12日-下午5:23:23
 *******************************************************************************/
#include <sdk.h>
#include <phy_spi.h>
#include <spi/dev_spi.h>
#include <storage/dev_storage.h>
#include "spi_nand.h"
#include <phy_gpio.h>
#include <gpio/dev_gpio.h>


#define NAND_PAGE_SIZE              (2*1024)
#define NAND_BLOCK_SIZE             (NAND_PAGE_SIZE*64)
#define NAND_SEC_PER_PAGE           (NAND_PAGE_SIZE/512)
#define NAND_SEC_PER_BLK            (NAND_BLOCK_SIZE/512)

#define STATUS_REG_ADDR             (0xC0)
#define FEATURE_REG_ADDR            (0xB0)
#define PROTECTION_REG_ADDR         (0xA0)

#define STATUS_P_FAIL               (1<<3)
#define STATUS_E_FAIL               (1<<2)
#define STATUS_ECC_FAIL             (1<<5)

static spi_xfer_instance_t spinand_xfer_instance;

static storage_priv_data_t storage_spinand;

static const struct acts_pin_config spinand_io_config[] =
{
    { 11, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3)},
    { 10, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3) },
    { 9, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3) },
    { 8, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3) },
};

spi_dev_cfg_t spinand_dev_cfg =
{
	//xfer instance
	&spinand_xfer_instance,

    SPI_DEFAULT_CS_PIN_NUM,

	//cs delay
	0,

	//mode
	SPIDEV_MODE3,

	//delaychain
	8,

	//24MHZ
	24,
};

static int32 spinand_reset(spi_dev_t *spinand_dev)
{
    return spi_dev_write_cmd(spinand_dev, SPINAND_CMD_RESET);
}

int32 spinand_get_feature(spi_dev_t *spinand_dev, uint32 reg_addr, uint8 *p_value)
{
    SpiInstruction cmd, addr, data;

    memset(&cmd, 0, sizeof(SpiInstruction));

    memset(&addr, 0, sizeof(SpiInstruction));

    memset(&data, 0, sizeof(SpiInstruction));

    cmd.data = SPINAND_CMD_GETFEATURE;
    cmd.len = 1;
    addr.data = reg_addr;
    addr.len = 1;
    data.data = (uint32)p_value;
    data.len = 1;

    return spinand_dev->read(spinand_dev, &cmd, &addr, NULL, &data, 0);
}

int32 spinand_set_feature(spi_dev_t *spinand_dev, uint32 reg_addr, uint8 value)
{
    SpiInstruction cmd, addr, data;

    memset(&cmd, 0, sizeof(SpiInstruction));

    memset(&addr, 0, sizeof(SpiInstruction));

    memset(&data, 0, sizeof(SpiInstruction));

    cmd.data = SPINAND_CMD_SETFEATURE;
    cmd.len = 1;
    addr.data = reg_addr;
    addr.len = 1;
    data.data = (uint32)&value;
    data.len = 1;

    return spinand_dev->write(spinand_dev, &cmd, &addr, NULL, &data, 0);
}

void spinand_enable_ecc(spi_dev_t *spinand_dev)
{
    uint8 reg_config;
	// nand  enable internal ecc
	spinand_get_feature(spinand_dev, FEATURE_CR_ADDR, &reg_config);
	reg_config |= (1 << SPINAND_CR_ECC_BIT);
	spinand_set_feature(spinand_dev, FEATURE_CR_ADDR, (unsigned int)reg_config);

}

void spinand_disable_write_protect(spi_dev_t *spinand_dev)
{
    uint8 pr_value;

    spinand_set_feature(spinand_dev, FEATURE_PR_ADDR, 0);

    spinand_get_feature(spinand_dev, FEATURE_PR_ADDR, &pr_value);

    pr_info("spinand PR value %x\n", pr_value);
}

int32 spinand_enable_quad_mode(spi_dev_t *spinand_dev)
{
    uint32 feature;
	uint32 ret;

    ret = spinand_get_feature(spinand_dev, FEATURE_CR_ADDR, (uint8 *)&feature);
    if (ret != 0)
        return ret;

    if (feature & 0x01)
        return 0;

    feature |= 0x1;
    ret = spinand_set_feature(spinand_dev, FEATURE_CR_ADDR, feature);

    return ret;
}

static int spinand_wait_ready(spi_dev_t *spinand_dev)
{
    int32           ret;
    uint8           status;

    while(1)
    {
        ret = spinand_get_feature(spinand_dev, FEATURE_SR_ADDR, (uint8 *)&status);
        if (ret == 0){
            /*check the value of the Write in Progress (WIP) bit.*/
            if ((status & 0x01) == 0)
            {
                break;
            }
        }
    }

    pr_info("get SR %x\n", status);

    return status;
}



int32 spinand_erase(spi_dev_t *spinand_dev, uint32 row_addr)
{
    spi_dev_set_write_protect(spinand_dev, 0);

    spi_dev_write(spinand_dev, row_addr, NULL, 0, SPI_DEV_TFLAG_3BYTES);

    spinand_wait_ready(spinand_dev);

    spi_dev_set_write_protect(spinand_dev, 1);

    return 0;
}

int32 spinand_page_write(spi_dev_t *spinand_dev, uint32 row_addr, void *buf, uint32 size)
{
    uint32 i;

    uint32 col_addr;

    spinand_dev_t *pDev = (spinand_dev_t *)spinand_dev->priv_data;

    /* 1.WRITE ENABLE*/
    spi_dev_set_write_protect(spinand_dev, 0);

    pDev->ProgLines = 0;

    /* 2.WRLOADRANM */
    for(i = 0; i < 4; i++){
        col_addr = (pDev->ProgLines << 9);

        spi_dev_write(spinand_dev, col_addr, buf, size, SPI_DEV_TFLAG_2BYTES);

        pDev->ProgLines++;
    }

    /* 3.WRITE ENABLE*/
    spi_dev_set_write_protect(spinand_dev, 0);

    /* 4.PROGRAM EXECUTE*/
    spi_dev_write_cmd_addr(spinand_dev, SPINAND_CMD_WREXEC, row_addr, SPI_DEV_TFLAG_3BYTES);

    spinand_wait_ready(spinand_dev);

    spi_dev_set_write_protect(spinand_dev, 1);

    return 0;
}

int32 spinand_page_read(spi_dev_t *spinand_dev,  uint32 row_addr, uint32 col_addr, void *buf, uint32 size)
{
    int status;

    uint32 i;

    spinand_dev_t *pDev = (spinand_dev_t *)spinand_dev->priv_data;

    /* 1.PAGE READ to cache*/
    spi_dev_write_cmd_addr(spinand_dev, SPINAND_CMD_PAGEREAD, row_addr, SPI_DEV_TFLAG_3BYTES);

    /* 2.GET FEATURES command to read the status*/
    status = spinand_wait_ready(spinand_dev);       // 1ms
    if (status < 0)
        return status;

    if (STATUS_ECC_FAIL & status)
        return -1;

    pDev->ProgLines = 0;

    /* 3.RANDOM DATA READ*/
    for(i = 0; i < 4; i++){
        col_addr = (pDev->ProgLines << 9);
        spi_dev_read(spinand_dev, col_addr, buf, size, SPI_DEV_TFLAG_2BYTES);
        pDev->ProgLines++;
    }

    return 0;
}

int32 spinand_read(spi_dev_t *spinand_dev,  uint32 sec, uint32 nSec, void *buf)
{
    int32  ret  = 0;
    uint32 len, offset;
    uint8 *pBuf =  (uint8*)buf;

    spinand_dev_t *pDev = (spinand_dev_t *)spinand_dev->priv_data;

    if ((sec + nSec) > pDev->capacity)
        return -1;

    offset = sec & (pDev->PageSecs-1);
    if (offset)
    {
        len = MIN(pDev->PageSecs-offset, nSec);
        ret = spinand_page_read(spinand_dev, sec/pDev->PageSecs, offset<<9, pBuf, len<<9);
        if (ret != 0)
            return ret;

        nSec -= len;
        sec += len;
        pBuf += len;
    }

    while (nSec)
    {
        len = MIN(pDev->PageSecs, nSec);
        ret = spinand_page_read(spinand_dev, sec/pDev->PageSecs, 0, pBuf, len<<9);
        if (ret != 0)
            return ret;

        nSec -= len;
        sec += len;
        pBuf += len;
    }

    return ret;
}

int32 spinand_write(spi_dev_t *spinand_dev,  uint32 sec, uint32 nSec, void *pData)
{
    int32  ret = 0;
    uint32 len, BlockSecs, PageSecs;
    uint8 *pBuf =  (uint8*)pData;

    spinand_dev_t *pDev = (spinand_dev_t *)spinand_dev->priv_data;

    if ((sec+nSec) > pDev->capacity)
        return -1;


    BlockSecs = pDev->BlockSecs;        //BlockSize is 4K Bytes
    PageSecs = pDev->PageSecs;

    while (nSec)
    {
        if (!(sec & (BlockSecs-1)))
        {
            ret = spinand_erase(spinand_dev, sec/PageSecs);
            if (ret != 0)
                return ret;
        }
        len = MIN(PageSecs, nSec);
        ret = spinand_page_write(spinand_dev, sec/PageSecs, pData, len<<9);
        if (ret != 0)
            return ret;

        nSec -= len;
        sec += len;
        pBuf += len;
    }

    return ret;
}

int32 spinand_read_id(spi_dev_t *spinand_dev)
{
    unsigned int chipid;

    //ID 字节数是2-3B, 不同厂商不一样
    spi_dev_read_chipid(spinand_dev, &chipid, 4);

    pr_info("spinand id: %x\n", chipid, 8);

    return chipid;
}

int32 storage_spinand_init(struct device *device, void *arg)
{
    static spi_dev_t *spinand_dev;

    static spinand_dev_t *spinand_dev_ptr;

	spi_host_io_param_t io_param;

    storage_priv_data_t *priv_data = (storage_priv_data_t *)device->driver_data;

	io_param.is_master = TRUE;
	io_param.max_speed_mhz = 100;
	io_param.pin_config = spinand_io_config;
	io_param.pins = ARRAY_SIZE(spinand_io_config);

    spinand_dev = zalloc(sizeof(spi_dev_t));

    spinand_dev_ptr = zalloc(sizeof(spinand_dev_t));

    spinand_dev->host = spi_get_host_instance(1);

    spinand_dev->dev_cfg = (spi_dev_cfg_t *)&spinand_dev_cfg;

    spinand_dev->xfer = spi_dev_xfer_data;

    spinand_dev->read = spi_dev_read_data;

    spinand_dev->write = spi_dev_write_data;
    
    spinand_dev->dev_wait_busy = spi_dev_wait_busy;

    spinand_dev->priv_data = (void *)spinand_dev_ptr;

    spinand_dev->addr_line = 1;
    spinand_dev->cmd_line = 1;
    spinand_dev->dummy_line = 1;
    spinand_dev->data_line = 1;
    spinand_dev->dummy_count = 1;

    spinand_dev->read_cmd = SPINAND_CMD_RCACHE;
    spinand_dev->write_cmd = SPINAND_CMD_WRLOADRANM;
    spinand_dev->erase_cmd = SPINAND_CMD_ERASE;


	spinand_dev->host->api->init(spinand_dev->host, &io_param);

	priv_data->dev = (void *)spinand_dev;

	//spinand_xfer_instance.xfer_handle = dma_alloc();

	spinand_reset(spinand_dev);

    //spinand_enable_ecc(spinand_dev);

    spinand_disable_write_protect(spinand_dev);

	spinand_read_id(spinand_dev);

	return 0;
}


int32 storage_spinand_read(struct device *storage_dev, uint32 lba, void *buffer, uint32 len)
{
    storage_priv_data_t *priv_data = (storage_priv_data_t *)(storage_dev->driver_data);
	spi_dev_read((spi_dev_t *)priv_data->dev, lba << 9, buffer, len << 9, 0);
	return 0;
}

int32 storage_spinand_write(struct device *storage_dev, uint32 lba, void *buffer, uint32 len)
{
    storage_priv_data_t *priv_data = (storage_priv_data_t *)(storage_dev->driver_data);

	spi_dev_write((spi_dev_t *)priv_data->dev, lba << 9, buffer, len << 9, 0);

	return 0;
}

int32 storage_spinand_erase(struct device *storage_dev, uint32 lba, uint32 len)
{
	return 0;
}


const static device_config_t storage_spinand_cfg =
{
	.drv_name = dev_name_spinand,
    .init = storage_spinand_init,
    .exit = NULL,
    .power_control = NULL,
    .detect = NULL,
    .mount = NULL,
    .config_info = NULL,
};

const static storage_device_api_t storage_spinand_api =
{
    .storage_read = storage_spinand_read,
    .storage_write = storage_spinand_write,
    .storage_erase = NULL,
    .storage_ioctl = NULL,
    .storage_byte_read = NULL,
    .storage_byte_write = NULL,
};

struct device DEVICE_INFO_SECTION device_spinand =
{
    .config = (device_config_t *)&storage_spinand_cfg,
    .driver_api = (const void *)&storage_spinand_api,
    .driver_data = (void *)&storage_spinand,
};



