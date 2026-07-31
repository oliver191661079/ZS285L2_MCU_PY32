/********************************************************************************
 *                            USDK(US283A_master)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2018年3月2日-上午10:42:54             1.0             build this file
 ********************************************************************************/
/*!
 * \file     spi_dev_operation.c
 * \brief
 * \author
 * \version  1.0
 * \date  2018年3月2日-上午10:42:54
 *******************************************************************************/
#include <sdk.h>
#include <phy_spi.h>
#include <spi/dev_spi.h>

int spi_dev_write_status(spi_dev_t *spi_dev, uint32 reg_index, uint8 status)
{
    int retval;
    SpiInstruction cmd, data;

    spi_dev_set_write_protect(spi_dev, 0);

	memset(&cmd, 0, sizeof(SpiInstruction));
	memset(&data, 0, sizeof(SpiInstruction));

	if (reg_index == SPI_DEV_STATUS1) {
		cmd.data = SPI_DEV_CMD_WRSR1;
	} else if (reg_index == SPI_DEV_STATUS2) {
		cmd.data = SPI_DEV_CMD_WRSR2;
	} else if (reg_index == SPI_DEV_STATUS3) {
		cmd.data = SPI_DEV_CMD_WRSR3;
	} else{
		; //nothing for QAC
	}
    cmd.len = 1;

	data.data = (uint32)&status;
	data.len = 1;
	data.line = 1;

	retval = spi_dev->write(spi_dev, &cmd, NULL, NULL, &data, SPI_DEV_TFLAG_CHECKRB);

    spi_dev->dev_wait_busy(spi_dev);

    return retval;
}

int spi_dev_suspend_erase_program(spi_dev_t *spi_dev)
{
    return spi_dev_write_cmd(spi_dev, SPI_DEV_CMD_EPSP);
}

int spi_dev_resume_erase_program(spi_dev_t *spi_dev)
{
    return spi_dev_write_cmd(spi_dev, SPI_DEV_CMD_EPRS);
}

int spi_dev_read_chipid(spi_dev_t *spi_dev, uint8 *chipid, uint32 chiplen)
{
    SpiInstruction cmd, data;

    //spi_dev_set_write_protect(spi_dev, 0);

	memset(&cmd, 0, sizeof(SpiInstruction));
	memset(&data, 0, sizeof(SpiInstruction));

	cmd.data = SPI_DEV_CMD_RDID;
	cmd.len = 1;
	data.data = (uint32)chipid;
	data.len = chiplen;

	return spi_dev->read(spi_dev, &cmd, NULL, NULL, &data, 0);
}

int spi_dev_enable_QPI_mode(spi_dev_t *spi_dev)
{
	int ret = spi_dev_write_cmd(spi_dev, SPI_DEV_CMD_EN_QPI);

	if (ret == 0){
		spi_dev->status |= SPI_DEV_READ_QPI_MODE;
    }

	return ret;
}

int spi_dev_disable_QPI_mode(spi_dev_t *spi_dev)
{
	int ret = spi_dev_write_cmd(spi_dev, SPI_DEV_CMD_DIS_QPI);

	if (ret == 0){
		spi_dev->status &= ~SPI_DEV_READ_QPI_MODE;
	}
	return ret;
}

int spi_dev_reset(spi_dev_t *spi_dev)
{
    return spi_dev_write_cmd(spi_dev, SPI_DEV_CMD_RESET);
}

int spi_dev_read_unique_id(spi_dev_t *spi_dev, uint8 uid[8])
{
    SpiInstruction cmd, dummy, data;

    memset(&cmd, 0, sizeof(SpiInstruction));

    memset(&data, 0, sizeof(SpiInstruction));

	cmd.data = SPI_DEV_CMD_RESET;
	cmd.len = 1;
    dummy.len = 4;
	data.data = (uint32)uid;
	data.len = 8;

	return spi_dev->read(spi_dev, &cmd, NULL, NULL, &data, 0);
}


int spi_dev_switch_read_mode(spi_dev_t *spi_dev, spi_dev_read_mode_e mode)
{
	uint8_t status;
	int ret;

	if (mode == SPI_DEV_READ_QUAD_O_MODE
	    || mode == SPI_DEV_READ_QUAD_IO_MODE
	    || mode == SPI_DEV_READ_QPI_MODE)
	{
		ret = spi_dev_read_status(spi_dev, SPI_DEV_STATUS2, &status);
		if (ret < 0)
			return -1;
		status |= 1 << 1;
		ret = spi_dev_write_status(spi_dev, SPI_DEV_STATUS2, status);
	}
	else
	{
		ret = spi_dev_read_status(spi_dev, SPI_DEV_STATUS2, &status);
		if (ret < 0)
			return -1;
		status &= ~(1 << 1);
		ret = spi_dev_write_status(spi_dev, SPI_DEV_STATUS2, status);
	}

	return ret;
}
