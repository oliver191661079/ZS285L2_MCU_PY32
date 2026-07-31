/** @file
 * @brief Actions bt stack os depend header file.
 */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ACTS_STACK_OS_DEPEND_H_
#define ACTS_STACK_OS_DEPEND_H_


#ifdef __cplusplus
extern "C" {
#endif

#ifdef BUILD_ASSERT
#undef BUILD_ASSERT
#define BUILD_ASSERT(x)
#else
#define BUILD_ASSERT(x)
#endif

#ifndef __IN_BT_SECTION
#define __IN_BT_STACK_SECTION __in_section_unique(bthost_bss)
#else
#define __IN_BT_STACK_SECTION	__IN_BT_SECTION
#endif

#define k_timeout_t	int
#define Z_TIMEOUT_TICKS(x)		(x)

#define K_TIMEOUT_GREATER(a, b)		((a) > (b))
#define K_TIMEOUT_SUB(a, b)			((a) - (b))
#define Z_LIFO_INITIALIZER(x) _K_LIFO_INITIALIZER(x)

#define K_TIMEOUT_EQ(a, b)				((a) == (b))

#define z_tick_get()					k_uptime_get();

void *k_heap_alloc(void *h, size_t bytes, k_timeout_t timeout);
void k_heap_free(void *h, void *mem);
uint64_t z_timeout_end_calc(k_timeout_t timeout);


#ifdef __cplusplus
}
#endif

#endif /* ACTS_STACK_OS_DEPEND_H_ */
