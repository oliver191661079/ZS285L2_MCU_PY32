/********************************************************************************
 *                            USDK(US283A_master)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2018年1月15日-上午11:58:00             1.0             build this file
 ********************************************************************************/
/*!
 * \file     spi_norflash.c
 * \brief
 * \author
 * \version  1.0
 * \date  2018年1月15日-上午11:58:00
 *******************************************************************************/
#include <sdk.h>
#include <phy_spi.h>
#include <spi/dev_spi.h>
#include <spi/spi_norflash.h>
#include <storage/dev_storage.h>
#include <irq_lowlevel.h>
#include <dmac/dmac.h>
#include <phy_gpio.h>
#include <gpio/dev_gpio.h>

#define SPI_NOR_INSTANCE_0

#define SPI_NOR_XFER_MODE  SPI_DEV_TFLAG_XFER_DMA

//#define SPI_NOR_XFER_MODE  0


static spi_xfer_instance_t spinor_xfer_instance0;

static spi_xfer_instance_t spinor_xfer_instance1;

static spi_xfer_instance_t spinor_xfer_instance2;

static storage_priv_data_t storage_spinor0;

static storage_priv_data_t storage_spinor1;

static storage_priv_data_t storage_spinor2;


static const struct acts_pin_config spinor_dev_pin_config0[] =
{
    { 31, 3 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3) },
    { 30, 6 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3) },
    { 29, 6 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3) },
    { 28, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3) },
};

static const struct acts_pin_config spinor_dev_pin_config1[] =
{
    { 11, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3)},
    { 10, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3) },
    { 9, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3) },
    { 8, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3) },

};

static const struct acts_pin_config spinor_dev_pin_config2[] =
{
    { 16, 1 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3)},
    { 17, 1 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3) },
    { 20, 1 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3) },
    { 21, 1 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3) },
};


spi_dev_cfg_t spinor_dev_cfg0 = {
     //xfer instance
     &spinor_xfer_instance0,

     SPI_DEFAULT_CS_PIN_NUM,

     //cs delay
     0,

     //mode3 默认都是MODE3,但存在有些flash只能使用MODE0的情况
     SPIDEV_MODE3,

     //delaychain
     8,

     //MHZ
     24,
};

spi_dev_cfg_t spinor_dev_cfg1 = {
     //xfer instance
     &spinor_xfer_instance1,

     SPI_DEFAULT_CS_PIN_NUM,

     //cs delay
     0,

     //mode3 默认都是MODE3,但存在有些flash只能使用MODE0的情况
     SPIDEV_MODE3,

     //delaychain
     8,

     //MHZ
     24,
};

spi_dev_cfg_t spinor_dev_cfg2 = {
     //xfer instance
     &spinor_xfer_instance2,

     SPI_DEFAULT_CS_PIN_NUM,

     //cs delay
     0,

     //mode3 默认都是MODE3,但存在有些flash只能使用MODE0的情况
     SPIDEV_MODE3,

     //delaychain
     8,

     //MHZ
     24,
};

static int32 read_partiton_options(struct device *storage_dev, uint32 lba, uint32 len)
{
    int i;

    storage_priv_data_t *priv_data = (storage_priv_data_t *)(storage_dev->driver_data);

    for(i = 0; i < priv_data->partition_num; i++)
    {
        if((lba >= priv_data->partition[i].start_lba)
            && ((lba + len) << priv_data->partition[i].end_lba))
        {
            return priv_data->partition[i].options;
        }
    }

    return -1;
}

int32 storage_spinor_read(struct device *storage_dev, uint32 lba, void *buffer, uint32 len)
{
    unsigned int retval;

    storage_priv_data_t *priv_data = (storage_priv_data_t *)(storage_dev->driver_data);

    //int32 partition_options = read_partiton_options(storage_dev, lba, len);

    //if(partition_options & PARTITION_OPT_SECURE_EN)
    //{
    retval = rom_spinor_read((spi_dev_t *)priv_data->dev, lba << 9, buffer, len << 9, SPI_DEV_TFLAG_3BYTES | SPI_NOR_XFER_MODE, SPINOR_WRITE_PAGE_SIZE);
    //}
    //else
    //{
    //    retval = rom_spinor_read((spi_dev_t *)priv_data->dev, lba << 9, buffer, len << 9, SPI_DEV_TFLAG_3BYTES | SPI_NOR_XFER_MODE, SPINOR_WRITE_PAGE_SIZE);
    //}

    return retval;
}

int32 storage_spinor_write(struct device *storage_dev, uint32 lba, void *buffer, uint32 len)
{
    unsigned int retval;

    storage_priv_data_t *priv_data = (storage_priv_data_t *)(storage_dev->driver_data);

    //int32 partition_options = read_partiton_options(storage_dev, lba, len);

    //if(partition_options & PARTITION_OPT_SECURE_EN)
    //{
        retval = rom_spinor_write((spi_dev_t *)priv_data->dev, lba << 9, buffer, len << 9, SPI_DEV_TFLAG_3BYTES  | SPI_NOR_XFER_MODE, SPINOR_WRITE_PAGE_SIZE);
    //}
    //else
    //{
    //    retval = rom_spinor_write((spi_dev_t *)priv_data->dev, lba << 9, buffer, len << 9, SPI_DEV_TFLAG_3BYTES | SPI_NOR_XFER_MODE, SPINOR_WRITE_PAGE_SIZE);
    //}

    return retval;

}

int32 spinor_erase_size(spi_dev_t *spi_dev, uint32 addr)
{
#if 0
	int erase_mode = spi_dev->status & SPI_DEV_ERASE_MODE_MASK;

	if(erase_mode == SPI_DEV_ERASE_4KB){
        return ERASE_SIZE_4KB;
	}else if(erase_mode == SPI_DEV_ERASE_32KB){
        return ERASE_SIZE_32KB;
	}else if(erase_mode == SPI_DEV_ERASE_64KB){
        return ERASE_SIZE_64KB;
	}else{
        return 0;
	}
#else
    return ERASE_SIZE_4KB;
#endif
}

int32 storage_spinor_erase(struct device *storage_dev, uint32 lba, uint32 len)
{
    unsigned int retval;

    storage_priv_data_t *priv_data = (storage_priv_data_t *)(storage_dev->driver_data);

	retval =  rom_spinor_erase((spi_dev_t *)priv_data->dev, lba, len, SPI_DEV_TFLAG_3BYTES, ERASE_SIZE_4KB);

    return retval;
}

int32 storage_spinor_byte_read(struct device *storage_dev, uint32 byte_addr, void *buffer, uint32 len)
{
    unsigned int retval;

    storage_priv_data_t *priv_data = (storage_priv_data_t *)(storage_dev->driver_data);

    return rom_spinor_read((spi_dev_t *)priv_data->dev, byte_addr, buffer, len, 0, SPINOR_WRITE_PAGE_SIZE);
}

int32 storage_spinor_byte_write(struct device *storage_dev, uint32 byte_addr, void *buffer, uint32 len)
{
    unsigned int retval;

    storage_priv_data_t *priv_data = (storage_priv_data_t *)(storage_dev->driver_data);

    return rom_spinor_write((spi_dev_t *)priv_data->dev, byte_addr, buffer, len, 0, SPINOR_WRITE_PAGE_SIZE);
}

uint32 spinor_read_id(struct device *device, uint8 *chipid, uint32 chiplen)
{
    storage_priv_data_t *priv_data = (storage_priv_data_t *)(device->driver_data);

    spi_dev_read_chipid((spi_dev_t *)priv_data->dev, chipid, chiplen);

    return 0;
}


void spinor_disable_write_protect(spi_dev_t *spi_dev)
{
    uint8 pr_value;

    spi_dev_read_status(spi_dev, SPI_DEV_STATUS1, &pr_value);

    if(pr_value & 0x3c){
        spi_dev_write_status(spi_dev, SPI_DEV_STATUS1, 0);
        spi_dev_read_status(spi_dev, SPI_DEV_STATUS1, &pr_value);
    }

    pr_info("spinor SR value %x\n", pr_value);
}




int32 storage_spinor_init(struct device *device, void *arg)
{
    spi_dev_t *spinor_dev;

    //unsigned char id[8];

    storage_priv_data_t *priv_data = (storage_priv_data_t *)device->driver_data;

    priv_data->disk_size_sector = 4 * 1024 * 2;

    spi_host_io_param_t io_param;

    io_param.is_master = TRUE;
    io_param.max_speed_mhz = 100;

    spinor_dev = malloc(sizeof(spi_dev_t));

    memset(spinor_dev, 0, sizeof(spi_dev_t));

    if(strncmp(device->config->drv_name, dev_name_spinor0, strlen(dev_name_spinor0)) == 0){
        spinor_dev->host = spi_get_host_instance(0);

        spinor_dev->dev_cfg = (spi_dev_cfg_t *)&spinor_dev_cfg0;

        //spinor_dev->dev_cfg->xfer->flag = SPI_FLAG_USE_3WIRE;

        io_param.pins = ARRAY_SIZE(spinor_dev_pin_config0);

        io_param.pin_config = spinor_dev_pin_config0;

    }else if(strncmp(device->config->drv_name, dev_name_spinor1, strlen(dev_name_spinor1)) == 0){
        spinor_dev->host = spi_get_host_instance(1);

        spinor_dev->dev_cfg = (spi_dev_cfg_t *)&spinor_dev_cfg1;

        io_param.pins = ARRAY_SIZE(spinor_dev_pin_config1);

        io_param.pin_config = spinor_dev_pin_config1;
    }else{
        spinor_dev->host = spi_get_host_instance(2);

        spinor_dev->dev_cfg = (spi_dev_cfg_t *)&spinor_dev_cfg2;

        io_param.pins = ARRAY_SIZE(spinor_dev_pin_config2);

        io_param.pin_config = spinor_dev_pin_config2;
    }

    spinor_dev->xfer = spi_dev_xfer_data;

    spinor_dev->read = spi_dev_read_data;

    spinor_dev->write = spi_dev_write_data;

    spinor_dev->dev_wait_busy = spi_dev_wait_busy;

    spinor_dev->addr_line = 1;
    spinor_dev->cmd_line = 1;
    spinor_dev->dummy_line = 1;
    spinor_dev->data_line = 1;
    spinor_dev->dummy_count = 1;
    spinor_dev->read_cmd = SPI_DEV_CMD_FAST_READ;
    spinor_dev->write_cmd = SPI_DEV_CMD_PP;
    spinor_dev->erase_cmd = SPI_DEV_CMD_ERASE_4KB;

    spinor_dev->dev_cfg->xfer->xfer_handle = dma_alloc_channel(SPINOR_DMA_NO);

	spinor_dev->host->api->init(spinor_dev->host, &io_param);

	priv_data->dev = (void *)spinor_dev;

    //spinor_read_id(spinor_dev, id, 4);

	spinor_disable_write_protect(spinor_dev);

    return 0;
}


const static storage_device_api_t storage_spinor_api =
{
    .storage_read = storage_spinor_read,
    .storage_write = storage_spinor_write,
    .storage_erase = storage_spinor_erase,
    .storage_ioctl = NULL,
    .storage_byte_read = storage_spinor_byte_read,
    .storage_byte_write = storage_spinor_byte_write,
};

const static device_config_t storage_spinor_cfg0 =
{
	.drv_name = dev_name_spinor0,
    .init = storage_spinor_init,
    .exit = NULL,
    .power_control = NULL,
    .detect = NULL,
    .mount = NULL,
    .config_info = NULL,
};

const static device_config_t storage_spinor_cfg1 =
{
	.drv_name = dev_name_spinor1,
    .init = storage_spinor_init,
    .exit = NULL,
    .power_control = NULL,
    .detect = NULL,
    .mount = NULL,
    .config_info = NULL,
};

const static device_config_t storage_spinor_cfg2 =
{
	.drv_name = dev_name_spinor2,
    .init = storage_spinor_init,
    .exit = NULL,
    .power_control = NULL,
    .detect = NULL,
    .mount = NULL,
    .config_info = NULL,
};



//SPINOR分区表
const storage_partition_t storage_spinor_partition[] =
{
    //mbrec分区
    {
        .id = PARTITION_TYPE_MBREC,
        .count = 0,
        .desc = "mbrec",
        .start_lba = 0,
        .end_lba = 2,
        .options = PARTITION_OPT_READ_DIS
            | PARTITION_OPT_WRITE_DIS
            | PARTITION_OPT_CRC_DIS
            | PARTITION_OPT_SECURE_EN,
    },

    //bootloader分区 128KB
    {
        .id = PARTITION_TYPE_BOOTLOADER,
        .count = 0,
        .desc = "boot",
        .start_lba = 128,
        .end_lba = 128 + 256,
        .options = PARTITION_OPT_READ_EN
            | PARTITION_OPT_WRITE_EN
            | PARTITION_OPT_CRC_EN
            | PARTITION_OPT_SECURE_EN,
    },

    //app分区
    {
        .id = PARTITION_TYPE_APPLICATION,
        .count = 0,
        .desc = "app",
        .start_lba = 128 + 256,
        .end_lba = 128 + 1024 * 512,
        .options = PARTITION_OPT_READ_EN
            | PARTITION_OPT_WRITE_EN
            | PARTITION_OPT_CRC_EN
            | PARTITION_OPT_SECURE_EN,

    },

    //VDFS分区
    {
        .id = PARTITION_TYPE_VDFS,
        .count = 0,
        .desc = "vdfs",
        .start_lba = 0,
        .end_lba = 0,
        .options = PARTITION_OPT_READ_EN
            | PARTITION_OPT_WRITE_EN
            | PARTITION_OPT_CRC_EN
            | PARTITION_OPT_SECURE_EN,

    },

    //VRAM分区,最后一个64KB
    {
        .id = PARTITION_TYPE_VRAM,
        .count = 0,
        .desc = "vram",
        .start_lba = 0,
        .end_lba = 0,
        .options = PARTITION_OPT_READ_EN
            | PARTITION_OPT_WRITE_EN
            | PARTITION_OPT_CRC_EN
            | PARTITION_OPT_SECURE_EN,

    },
};


struct device DEVICE_INFO_SECTION device_spinor0 =
{
    .config = (device_config_t *)&storage_spinor_cfg0,
    .driver_api = (const void *)&storage_spinor_api,
    .driver_data = (void *)&storage_spinor0,
};

struct device DEVICE_INFO_SECTION device_spinor1 =
{
    .config = (device_config_t *)&storage_spinor_cfg1,
    .driver_api = (const void *)&storage_spinor_api,
    .driver_data = (void *)&storage_spinor1,
};

struct device DEVICE_INFO_SECTION device_spinor2 =
{
    .config = (device_config_t *)&storage_spinor_cfg2,
    .driver_api = (const void *)&storage_spinor_api,
    .driver_data = (void *)&storage_spinor2,
};

int32 spinor_erase(uint32 byte_addr, uint32 byte_num)
{
    storage_erase((struct device *)&device_spinor0, byte_addr >> 9, byte_num >> 9);

    return 0;
}


int32 spinor_read(unsigned int lba, void *sector_buf, int sectors, int random)
{
    unsigned int retval;

    struct device *p_dev = &device_spinor0;

    storage_priv_data_t *priv_data = (storage_priv_data_t *)(p_dev->driver_data);

    if(random)
    {
        retval = rom_spinor_read((spi_dev_t *)priv_data->dev, lba << 9, sector_buf, sectors << 9, \
        SPI_DEV_TFLAG_3BYTES | SPI_DEV_TFLAG_ENABLE_RANDOMIZE | SPI_DEV_TFLAG_XFER_DMA, 32);
    }
    else
    {
        retval = rom_spinor_read((spi_dev_t *)priv_data->dev, lba << 9, sector_buf, sectors << 9, \
        SPI_DEV_TFLAG_3BYTES | SPI_DEV_TFLAG_XFER_DMA, SPINOR_WRITE_PAGE_SIZE);
    }

    return retval;
}

int32 spinor_write(unsigned int lba, void *sector_buf, int sectors, int random)
{
    unsigned int retval;

    struct device *p_dev = &device_spinor0;

    if(sector_buf == NULL){
        sector_buf = (void *)INVALID_RAM_ADDR;
    }

    storage_priv_data_t *priv_data = (storage_priv_data_t *)(p_dev->driver_data);

    if(random)
    {
        retval = rom_spinor_write((spi_dev_t *)priv_data->dev, lba << 9, sector_buf, sectors << 9, \
        SPI_DEV_TFLAG_3BYTES | SPI_DEV_TFLAG_ENABLE_RANDOMIZE | SPI_DEV_TFLAG_XFER_DMA, 32);
    }
    else
    {
        retval = rom_spinor_write((spi_dev_t *)priv_data->dev, lba << 9, sector_buf, sectors << 9, \
        SPI_DEV_TFLAG_3BYTES | SPI_DEV_TFLAG_XFER_DMA, SPINOR_WRITE_PAGE_SIZE);
    }

    return retval;
}


void spi_write_status_register(uint8 cmd, uint32 data, uint32 write_twice, uint32 status_16bit)
{
	return;
}

void spi_read_status_register(uint8 cmd, uint8 *data)
{
	return;
}

int32 spinor_byte_read(uint32 addr, char *buf, size_t nbytes)
{
    struct device *p_dev = &device_spinor0;

    storage_priv_data_t *priv_data = (storage_priv_data_t *)(p_dev->driver_data);

    return rom_spinor_read((spi_dev_t *)priv_data->dev, addr, buf, nbytes, 0, SPINOR_WRITE_PAGE_SIZE);
}

int32 spinor_byte_write(uint32 addr, char *buf, size_t nbytes)
{
    struct device *p_dev = &device_spinor0;

    storage_priv_data_t *priv_data = (storage_priv_data_t *)(p_dev->driver_data);

    return rom_spinor_write((spi_dev_t *)priv_data->dev, addr, buf, nbytes, 0, SPINOR_WRITE_PAGE_SIZE);
}


