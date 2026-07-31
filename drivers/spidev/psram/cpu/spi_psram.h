/********************************************************************************
 *                            USDK(US283A_master)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2018年1月15日-上午10:11:29             1.0             build this file
 ********************************************************************************/
/*!
 * \file     spi_psram.h
 * \brief
 * \author
 * \version  1.0
 * \date  2018年1月15日-上午10:11:29
 *******************************************************************************/

#ifndef PLATFORM_DRIVER_SPI_SPI_DEV_PSRAM_CPU_SPI_PSRAM_H_
#define PLATFORM_DRIVER_SPI_SPI_DEV_PSRAM_CPU_SPI_PSRAM_H_

#define PSRAM_CMD_SLOW_READ	        0x03
#define PSRAM_CMD_FAST_READ	        0x0b
#define PSRAM_CMD_FAST_QUAD_READ	0xeb
#define PSRAM_CMD_WRITE		        0x02
#define PSRAM_CMD_QUAD_WRITE	    0x38
#define PSRAM_CMD_ENTER_QUAD_MODE	0x35
#define PSRAM_CMD_EXIT_QUAD_MODE    0xf5
#define PSRAM_CMD_RESET_ENABLE		0x66
#define PSRAM_CMD_RESET			    0x99

#endif /* PLATFORM_DRIVER_SPI_SPI_DEV_PSRAM_CPU_SPI_PSRAM_H_ */
