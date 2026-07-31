/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Magic System Request Key Hacks
 *
 */

#ifndef __INCLUDE_SYSRQ_H__
#define __INCLUDE_SYSRQ_H__

#include <kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_MAGIC_SYSRQ

void uart_handle_sysrq_char(struct device * port, char c);

#else

void uart_handle_sysrq_char(struct device * port, char c)
{
}

#endif

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __INCLUDE_SYSRQ_H__ */
