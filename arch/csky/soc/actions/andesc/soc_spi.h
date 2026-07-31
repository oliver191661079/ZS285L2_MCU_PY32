/********************************************************************************
 *                            USDK(ZS283A)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2018-10-15-下午9:49:52             1.0             build this file
 ********************************************************************************/
/*!
 * \file     soc_spi.h
 * \brief
 * \author
 * \version  1.0
 * \date  2018-10-15-下午9:49:52
 *******************************************************************************/

#ifndef SOC_SPI_H_
#define SOC_SPI_H_

//spi数据传输使用dma还是cpu
#define SSPI_CTL_CLK_SEL_MASK           (1 << 31)
#define SSPI_CTL_CLK_SEL_CPU            (0 << 31)
#define SSPI_CTL_CLK_SEL_DMA            (1 << 31)

#define SSPI0_CTL_MODE_MASK             (1 << 28)
#define SSPI0_CTL_MODE_MODE0            (1 << 28)
#define SSPI0_CTL_MODE_MODE3            (0 << 28)

#define SPPI1_CTL_MODE_SHIFT            (28)
#define SSPI1_CTL_MODE_MASK             (3 << 28)
#define SSPI1_CTL_MODE_MODE0            (0 << 28)
#define SSPI1_CTL_MODE_MODE1            (1 << 28)
#define SSPI1_CTL_MODE_MODE2            (2 << 28)
#define SSPI1_CTL_MODE_MODE3            (3 << 28)

#define SSPI0_CTL_CRC_EN                (1 << 20)

#define SSPI_CTL_DELAYCHAIN_MASK        (0xf << 16)
#define SSPI_CTL_DELAYCHAIN_SHIFT       (16)

#define SSPI0_CTL_RAND_MASK             (0xf << 12)
#define SSPI0_CTL_RAND_PAUSE            (1 << 15)
#define SSPI0_CTL_RAND_SEL              (1 << 14)
#define SSPI0_CTL_RAND_TXEN             (1 << 13)
#define SSPI0_CTL_RAND_RXEN             (1 << 12)

#define SSPI_CTL_IO_MODE_MASK           (0x3 << 10)
#define SSPI_CTL_IO_MODE_SHIFT          (10)
#define SSPI_CTL_IO_MODE_1X             (0x0 << 10)
#define SSPI_CTL_IO_MODE_2X             (0x2 << 10)
#define SSPI_CTL_IO_MODE_4X             (0x3 << 10)

#define SSPI0_CTL_SPI_3WIRE             (1 << 9)

#define SSPI0_CTL_AHB_REQ               (1 << 8)
#define SSPI1_CTL_AHB_REQ               (1 << 15)

#define SSPI1_CTL_TX_IRQ_EN             (1 << 9)
#define SSPI1_CTL_RX_IRQ_EN             (1 << 8)

#define SSPI_CTL_TX_DRQ_EN              (1 << 7)
#define SSPI_CTL_RX_DRQ_EN              (1 << 6)
#define SSPI_CTL_TX_FIFO_EN             (1 << 5)
#define SSPI_CTL_RX_FIFO_EN             (1 << 4)
#define SSPI_CTL_SS                     (1 << 3)

#define SSPI_CTL_WR_MODE_SHIFT          (0)
#define SSPI_CTL_WR_MODE_MASK           (0x3 << 0)
#define SSPI_CTL_WR_MODE_DISABLE        (0x0 << 0)
#define SSPI_CTL_WR_MODE_READ           (0x1 << 0)
#define SSPI_CTL_WR_MODE_WRITE          (0x2 << 0)
#define SSPI_CTL_WR_MODE_READ_WRITE     (0x3 << 0)

#define SSPI0_STATUS_BUSY               (1 << 6)
#define SSPI1_STATUS_BUSY               (1 << 0)

#define SSPI0_STATUS_READY              (1 << 8)
#define SSPI1_STATUS_READY              (1 << 1)

#define SSPI_STATUS_TX_FULL             (1 << 5)
#define SSPI_STATUS_TX_EMPTY            (1 << 4)
#define SSPI0_STATUS_RX_FULL            (1 << 3)
#define SSPI0_STATUS_RX_EMPTY           (1 << 2)
#define SSPI1_STATUS_RX_FULL            (1 << 7)
#define SSPI1_STATUS_RX_EMPTY           (1 << 6)

#define SSPI0_STATUS_ICACHE_ERROR       (1 << 1)

#define SPI_FLAG_NEED_EXIT_CONTINUOUS_READ  (1 << 0)
#define SPI_FLAG_CFG_RESET_SPIHC            (1 << 1)
#define SPI_FLAG_USE_3WIRE                  (1 << 2)
#define SPI_FLAG_SPI_MODE                   (7 << 3)
#define SPI_FLAG_EXT                        (1 << 6)

#define SPI_DEFAULT_FREQ_KHZ                (CONFIG_HOSC_CLK_MHZ)

#define SPI_DEFAULT_CS_PIN_NUM          (0xff)

#define CACHE_MODE   (0xc010001c)

#define CACHE_CTL    (0xc00f0000)

#define SPI1_CTL    (0xc0110000)

typedef struct spi_register {
    volatile uint32_t ctl;
    volatile uint32_t status;
    volatile uint32_t tx_data;
    volatile uint32_t rx_data;
    volatile uint32_t byte_count;
} spi_register_t;

#endif /* SOC_SPI_H_ */
