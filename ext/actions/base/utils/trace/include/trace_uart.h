/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file trace uart interface
 */

#ifndef TRACE_UART_H_
#define TRACE_UART_H_

void hal_trace_uart_output(char *s, int len, unsigned int async_mode);

int hal_trace_uart_is_busy(void);

#endif /* TRACE_UART_H_ */
