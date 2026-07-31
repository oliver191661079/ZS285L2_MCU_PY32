/********************************************************************************
 *                            USDK(US283A_master)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2018年1月15日-下午7:21:12             1.0             build this file
 ********************************************************************************/
/*!
 * \file     spi_nand.h
 * \brief
 * \author
 * \version  1.0
 * \date  2018年1月15日-下午7:21:12
 *******************************************************************************/

#ifndef PLATFORM_DRIVER_SPI_SPI_DEV_SPINAND_PHY_SPI_NAND_H_
#define PLATFORM_DRIVER_SPI_SPI_DEV_SPINAND_PHY_SPI_NAND_H_

//spi nand cmd
#define  SPINAND_CMD_RESET         (0xFF)
#define  SPINAND_CMD_PAGEREAD      (0x13)
#define  SPINAND_CMD_RCACHE        (0x03)
#define  SPINAND_CMD_WRLOAD        (0x02)
#define  SPINAND_CMD_WRLOADRANM    (0x84)
#define  SPINAND_CMD_WREXEC        (0x10)
#define  SPINAND_CMD_WRENABLE      (0x06)
#define  SPINAND_CMD_WRDISABLE     (0x04)
#define  SPINAND_CMD_SETFEATURE    (0x1F)
#define  SPINAND_CMD_GETFEATURE    (0x0F)
#define  SPINAND_CMD_ERASE         (0xD8)

#define  FEATURE_PR_ADDR           (0xA0)
#define  FEATURE_CR_ADDR           (0xB0)
#define  FEATURE_SR_ADDR           (0xC0)
#define  FEATURE_ODR_ADDR          (0xD0)

#define  SPINAND_CR_ECC_BIT        (4)

#define  SPINAND_SR_ECCSTATUS_BIT  (4)
#define  SPINAND_ECCS_SET          (3 << SPINAND_SR_ECCSTATUS_BIT)


typedef enum
{
    DATA_LINES_X1 = 0,
    DATA_LINES_X2,
    DATA_LINES_X4
} SFC_DATA_LINES;

typedef struct
{
    uint32  capacity;                   //sec unit
    uint16  PageSecs;                   //sec uint
    uint16  BlockSecs;                  //sec uint
    uint8   Manufacturer;
    uint8   DevID;

    uint8   ReadLines;                  //Read Data Lines
    uint8   ProgLines;                  //Write Data Lines
}spinand_dev_t;



#endif /* PLATFORM_DRIVER_SPI_SPI_DEV_SPINAND_PHY_SPI_NAND_H_ */
