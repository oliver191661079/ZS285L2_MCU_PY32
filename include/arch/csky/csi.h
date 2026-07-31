/*
 * Copyright (c) 2017 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief CSKY interface file
 *
 * This header contains the interface to the ARM CSKY Core headers.
 */

#ifndef _CSKY__H_
#define _CSKY__H_

#ifdef __cplusplus
extern "C" {
#endif

#include <soc.h>

#define __CK802_REV               0x0000U   /* Core revision r0p0 */
#define __MGU_PRESENT             0         /* MGU present or not */
#define __NVIC_PRIO_BITS          CONFIG_NUM_IRQ_PRIO_BITS         /* Number of Bits used for Priority Levels */
#define __Vendor_SysTickConfig    0         /* Set to 1 if different SysTick Config is used */

#if __NVIC_PRIO_BITS != CONFIG_NUM_IRQ_PRIO_BITS
#error "CONFIG_NUM_IRQ_PRIO_BITS and __NVIC_PRIO_BITS are not set to the same value"
#endif

#if defined(CONFIG_CPU_CK801)
#include <core_ck801.h>
#elif defined(CONFIG_CPU_CK802)
#include <core_ck802.h>
#elif defined(CONFIG_CPU_CK803)
#include <core_ck803S.h>
#else
#error "Unknown CSKY device"
#endif

#ifdef __cplusplus
}
#endif

#endif /* _CSKY__H_ */
