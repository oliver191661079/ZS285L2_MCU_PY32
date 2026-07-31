/********************************************************************************
 *                            USDK(ZS283A)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2018-10-13-下午2:15:08             1.0             build this file
 ********************************************************************************/
/*!
 * \file     soc_uart.h
 * \brief
 * \author
 * \version  1.0
 * \date  2018-10-13-下午2:15:08
 *******************************************************************************/

#ifndef SOC_UART_H_
#define SOC_UART_H_

/* UART registers struct */
typedef struct acts_uart_controller {
    volatile uint32_t ctrl;

    volatile uint32_t rxdat;

    volatile uint32_t txdat;

    volatile uint32_t stat;

    volatile uint32_t br;
} uart_register_t;

/* bits */
#define UART_CTL_RX_EN              BIT(31)
#define UART_CTL_TX_EN              BIT(30)

#ifdef CONFIG_SOC_SERIES_ANDESC
#define UART_CTL_TX_FIFO_EN         BIT(29)
#define UART_CTL_RX_FIFO_EN         BIT(28)

#define UART_CTL_TX_DST_SHIFT       26
#define UART_FIFO_CPU         0
#define UART_FIFO_DMA         1
#define UART_FIFO_DSP         2
#define UART_CTL_TX_DST(x)          ((x) << UART_CTL_TX_DST_SHIFT)
#define UART_CTL_TX_DST_MASK        UART_CTL_TX_DST(0x3)

#define UART_CTL_RX_SRC_SHIFT       24
#define UART_CTL_RX_SRC(x)          ((x) << UART_CTL_RX_SRC_SHIFT)
#define UART_CTL_RX_SRC_MASK        UART_CTL_RX_SRC(0x3)
#else
#define UART_CTL_TX_FIFO_EN         BIT(23)
#define UART_CTL_RX_FIFO_EN         BIT(22)
#endif

//#define UART_CTL_TX_DMA             BIT(21)
#define UART_CTL_LB_EN              BIT(20)
#define UART_CTL_TX_IE              BIT(19)
#define UART_CTL_RX_IE              BIT(18)
#define UART_CTL_TX_DE              BIT(17)
#define UART_CTL_RX_DE              BIT(16)
#define UART_CTL_EN             BIT(15)
//#define UART_CTL_RX_DMA             BIT(14)
#define UART_CTL_RTS_EN             BIT(13)
#define UART_CTL_AF_EN              BIT(12)
#define UART_CTL_RX_FIFO_THREHOLD_SHIFT     10
#define UART_CTL_RX_FIFO_THREHOLD(x)        ((x) << UART_CTL_RX_FIFO_THREHOLD_SHIFT)
#define UART_CTL_RX_FIFO_THREHOLD_MASK      UART_CTL_RX_FIFO_THREHOLD(0x3)
#define     UART_CTL_RX_FIFO_THREHOLD_1BYTE     UART_CTL_RX_FIFO_THREHOLD(0x0)
#define     UART_CTL_RX_FIFO_THREHOLD_4BYTES    UART_CTL_RX_FIFO_THREHOLD(0x1)
#define     UART_CTL_RX_FIFO_THREHOLD_8BYTES    UART_CTL_RX_FIFO_THREHOLD(0x2)
#define     UART_CTL_RX_FIFO_THREHOLD_12BYTES   UART_CTL_RX_FIFO_THREHOLD(0x3)
#define UART_CTL_TX_FIFO_THREHOLD_SHIFT     8
#define UART_CTL_TX_FIFO_THREHOLD(x)        ((x) << UART_CTL_TX_FIFO_THREHOLD_SHIFT)
#define UART_CTL_TX_FIFO_THREHOLD_MASK      UART_CTL_TX_FIFO_THREHOLD(0x3)
#define     UART_CTL_TX_FIFO_THREHOLD_1BYTE     UART_CTL_TX_FIFO_THREHOLD(0x0)
#define     UART_CTL_TX_FIFO_THREHOLD_4BYTES    UART_CTL_TX_FIFO_THREHOLD(0x1)
#define     UART_CTL_TX_FIFO_THREHOLD_8BYTES    UART_CTL_TX_FIFO_THREHOLD(0x2)
#define     UART_CTL_TX_FIFO_THREHOLD_12BYTES   UART_CTL_TX_FIFO_THREHOLD(0x3)
//#define UART_CTL_CTS_EN             BIT(7)
#define UART_CTL_PARITY_SHIFT           4
#define UART_CTL_PARITY(x)          ((x) << UART_CTL_PARITY_SHIFT)
#define UART_CTL_PARITY_MASK            UART_CTL_PARITY(0x7)
#define     UART_CTL_PARITY_NONE            UART_CTL_PARITY(0x0)
#define     UART_CTL_PARITY_ODD         UART_CTL_PARITY(0x4)
#define     UART_CTL_PARITY_LOGIC_1         UART_CTL_PARITY(0x5)
#define     UART_CTL_PARITY_EVEN            UART_CTL_PARITY(0x6)
#define     UART_CTL_PARITY_LOGIC_0         UART_CTL_PARITY(0x7)
#define UART_CTL_STOP_SHIFT         2
#define UART_CTL_STOP(x)            ((x) << UART_CTL_STOP_SHIFT)
#define UART_CTL_STOP_MASK          UART_CTL_STOP(0x1)
#define     UART_CTL_STOP_1BIT          UART_CTL_STOP(0x0)
#define     UART_CTL_STOP_2BIT          UART_CTL_STOP(0x1)
#define UART_CTL_DATA_WIDTH_SHIFT       0
#define UART_CTL_DATA_WIDTH(x)          ((x) << UART_CTL_DATA_WIDTH_SHIFT)
#define UART_CTL_DATA_WIDTH_MASK        UART_CTL_DATA_WIDTH(0x3)
#define     UART_CTL_DATA_WIDTH_5BIT        UART_CTL_DATA_WIDTH(0x0)
#define     UART_CTL_DATA_WIDTH_6BIT        UART_CTL_DATA_WIDTH(0x1)
#define     UART_CTL_DATA_WIDTH_7BIT        UART_CTL_DATA_WIDTH(0x2)
#define     UART_CTL_DATA_WIDTH_8BIT        UART_CTL_DATA_WIDTH(0x3)

#define UART_STA_PAER               BIT(23)
#define UART_STA_STER               BIT(22)
#define UART_STA_UTBB                   BIT(21)
#define UART_STA_TXFL_SHIFT         16
#define UART_STA_TXFL_MASK          (0x1f << UART_STA_TXFL_SHIFT)
#define UART_STA_RXFL_SHIFT         11
#define UART_STA_RXFL_MASK          (0x1f << UART_STA_RXFL_SHIFT)
#define UART_STA_TFES                   BIT(10)
#define UART_STA_RFFS               BIT(9)
#define UART_STA_RTSS               BIT(8)
#define UART_STA_CTSS               BIT(7)
#define UART_STA_TFFU               BIT(6)
#define UART_STA_RFEM               BIT(5)
#define UART_STA_RXST               BIT(4)
#define UART_STA_TFER               BIT(3)
#define UART_STA_RXER               BIT(2)
#define UART_STA_TIP                BIT(1)
#define UART_STA_RIP                BIT(0)

#define UART_STA_ERRS               (UART_STA_RXER | UART_STA_TFER | \
                         UART_STA_RXST | UART_STA_PAER | \
                         UART_STA_STER)

#endif /* SOC_UART_H_ */
