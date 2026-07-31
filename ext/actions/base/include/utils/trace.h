/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief trace file APIs.
*/

#ifndef TRACE_H_
#define TRACE_H_

typedef enum
{
    TRACE_MODE_DISABLE = 0,
    TRACE_MODE_CPU,
    TRACE_MODE_DMA,
	TRACE_MODE_DMA_USB_CDC_ACM,
}trace_mode_e;

int trace_mode_set(unsigned int trace_mode);

int trace_irq_print_set(unsigned int irq_enable);

int trace_dma_print_set(unsigned int dma_enable);

#endif /* TRACE_H_ */
