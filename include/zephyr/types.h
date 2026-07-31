/*
 * Copyright (c) 2017 Linaro Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __Z_TYPES_H__
#define __Z_TYPES_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef signed char         s8_t;
typedef signed short        s16_t;
typedef signed int          s32_t;
typedef signed long long    s64_t;

typedef unsigned char       u8_t;
typedef unsigned short      u16_t;
typedef unsigned int        u32_t;
typedef unsigned long long  u64_t;

#ifndef uint8
typedef unsigned char uint8;
#endif

#ifndef int8
typedef signed char int8;
#endif

#ifndef uint16
typedef unsigned short uint16;
#endif

#ifndef int16
typedef signed short int16;
#endif

#ifndef uint32
typedef unsigned int uint32;
#endif

#ifndef int32
typedef signed int int32;
#endif

#ifndef ulong32
typedef unsigned long ulong32;
#endif


#ifdef __cplusplus
}
#endif

#endif /* __Z_TYPES_H__ */
