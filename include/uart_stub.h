/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * @brief UART ATT Driver for Actions SoC.
 *
 */

#ifndef _ACTIONS_UART_ATT_H_
#define _ACTIONS_UART_ATT_H_

#include <uart.h>

#define UART_RSP_TIMEOUT   2
#define UART_PACKAGE_SIZE  1032

#define UART_PACKAGE    'P'
#define UART_REQUEST    'Q'
#define UART_SERVICE    'V'
#define UART_DATA_SEND  'S'
#define UART_DATA_RECV  'R'
#define UART_RSP_NO     'N'
#define UART_RSP_YES    'Y'

#define UART_TX_START  (1 << 0)
#define UART_TX_END    (1 << 1)

#define UART_STUB_BUF_SIZE  (32*1024)

typedef enum{
    UART_BR_NORMAL_MODE,
    UART_BR_ATT_MODE,
    UART_BR_CPU_MODE,
    UART_BR_PRINTK_CPU,
    UART_BR_PRINTK_DMA
}uart_baud_rate_mode;

typedef struct
{
    u16_t  request;
    u16_t  param[3];

    u16_t  send_size;
    u16_t  recv_size;

    u8_t*  send_data;
    u8_t*  recv_data;

    u32_t  timeout;
    u32_t  debug_mode;

} uart_session_request_t;


extern int uart_session_request(uart_session_request_t* q);

static inline u32_t usec_spent_time(u32_t reftime)
{
    u32_t current_cycles = k_cycle_get_32();

    u32_t usec_cycles = (u32_t)(
            (u32_t)sys_clock_hw_cycles_per_sec /
            (u32_t)USEC_PER_SEC
    );

    return ((current_cycles - reftime)/usec_cycles);
}

#endif	/* _ACTIONS_UART_ATT_H_ */
