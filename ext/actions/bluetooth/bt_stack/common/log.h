/** @file
 *  @brief Bluetooth subsystem logging helpers.
 */

/*
 * Copyright (c) 2017 Nordic Semiconductor ASA
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __BT_LOG_H
#define __BT_LOG_H

#include <linker/sections.h>
#include <offsets.h>
#include <zephyr.h>
#include <misc/__assert.h>

#include <acts_bluetooth/bluetooth.h>
#include <acts_bluetooth/uuid.h>
#include <acts_bluetooth/hci.h>

#if !defined(SYS_LOG_DOMAIN)
#define SYS_LOG_DOMAIN "bt"
#endif
#ifndef SYS_LOG_LEVEL
#define SYS_LOG_LEVEL SYS_LOG_LEVEL_DEBUG
#endif

#include <logging/sys_log.h>

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(BT_DBG_ENABLED)
#define BT_DBG_ENABLED 0
#endif

static inline char *log_strdup(const char *str)
{
	return (char *)str;
}


#if defined(CONFIG_ATCS_BT_DEBUG_MONITOR)
#include <stdio.h>

/* These defines follow the values used by syslog(2) */
#define BT_LOG_ERR      3
#define BT_LOG_WARN     4
#define BT_LOG_INFO     6
#define BT_LOG_DBG      7

__printf_like(2, 3) void bt_log(int prio, const char *fmt, ...);

#define BT_DBG(fmt, ...) \
	if (BT_DBG_ENABLED) { \
		bt_log(BT_LOG_DBG, "%s (%p): " fmt, \
		       __func__, k_current_get(), ##__VA_ARGS__); \
	}

#define BT_ERR(fmt, ...) bt_log(BT_LOG_ERR, "%s: " fmt, \
				__func__, ##__VA_ARGS__)
#define BT_WARN(fmt, ...) bt_log(BT_LOG_WARN, "%s: " fmt, \
				 __func__, ##__VA_ARGS__)
#define BT_INFO(fmt, ...) bt_log(BT_LOG_INFO, fmt, ##__VA_ARGS__)

/* Enabling debug increases stack size requirement */
#define BT_STACK_DEBUG_EXTRA	300

#elif defined(CONFIG_ATCS_BT_DEBUG_LOG)
#define BT_DBG(fmt, ...) \
	if (BT_DBG_ENABLED) { \
		SYS_LOG_INF("(%p) " fmt, k_current_get(), \
			    ##__VA_ARGS__); \
	}

#define BT_ERR(fmt, ...) SYS_LOG_ERR(fmt, ##__VA_ARGS__)
#define BT_WARN(fmt, ...) SYS_LOG_WRN(fmt, ##__VA_ARGS__)
#define BT_INFO(fmt, ...) SYS_LOG_INF(fmt, ##__VA_ARGS__)

/* Enabling debug increases stack size requirement considerably */
#define BT_STACK_DEBUG_EXTRA	0

#else
static inline __printf_like(1, 2) void _bt_log_dummy(const char *fmt, ...) {};

#define BT_DBG(fmt, ...) \
		if (0) { \
			_bt_log_dummy(fmt, ##__VA_ARGS__); \
		}
//#define BT_ERR BT_DBG
#define BT_ERR(fmt, ...) SYS_LOG_ERR(fmt, ##__VA_ARGS__)
#define BT_WARN BT_DBG
#define BT_INFO BT_DBG

#define BT_STACK_DEBUG_EXTRA	0
#endif

#define BT_SYS_ERR(fmt, ...) SYS_LOG_ERR(fmt, ##__VA_ARGS__)
#define BT_SYS_WRN(fmt, ...) SYS_LOG_WRN(fmt, ##__VA_ARGS__)
#define BT_SYS_INF(fmt, ...) SYS_LOG_INF(fmt, ##__VA_ARGS__)
#define BT_SYS_DBG(fmt, ...) SYS_LOG_DBG(fmt, ##__VA_ARGS__)

//To Do
#if 0 
#define BT_ASSERT_PRINT(test) __ASSERT_LOC(test)
#define BT_ASSERT_PRINT_MSG(fmt, ...) __ASSERT_MSG_INFO(fmt, ##__VA_ARGS__)
#else
#define BT_ASSERT_PRINT(test)
#define BT_ASSERT_PRINT_MSG(fmt, ...)
#endif /* CONFIG_BT_ASSERT_VERBOSE */

#if defined(CONFIG_BT_ASSERT_PANIC)
#define BT_ASSERT_DIE k_panic
#else
#define BT_ASSERT_DIE k_oops
#endif /* CONFIG_BT_ASSERT_PANIC */

#if defined(CONFIG_BT_ASSERT)
#define BT_ASSERT(cond)                          \
	do {                                     \
		if (!(cond)) {                   \
			BT_ASSERT_PRINT(cond);   \
			BT_ASSERT_DIE();         \
		}                                \
	} while (0)

#define BT_ASSERT_MSG(cond, fmt, ...)                              \
	do {                                                       \
		if (!(cond)) {                                     \
			BT_ASSERT_PRINT(cond);                     \
			BT_ASSERT_PRINT_MSG(fmt, ##__VA_ARGS__);   \
			BT_ASSERT_DIE();                           \
		}                                                  \
	} while (0)
#else
#define BT_ASSERT(cond) __ASSERT_NO_MSG(cond)
#define BT_ASSERT_MSG(cond, msg, ...) __ASSERT(cond, msg, ##__VA_ARGS__)
#endif/* CONFIG_BT_ASSERT*/

//to do
//#define BT_HEXDUMP_DBG(_data, _length, _str) 
//		LOG_HEXDUMP_DBG((const uint8_t *)_data, _length, _str)

#define BT_STACK(name, size) \
		K_THREAD_STACK_MEMBER(name, (size) + BT_STACK_DEBUG_EXTRA)
#define BT_STACK_NOINIT(name, size) \
		K_THREAD_STACK_DEFINE(name, (size) + BT_STACK_DEBUG_EXTRA)
		
/* NOTE: These helper functions always encodes into the same buffer storage.
 * It is the responsibility of the user of this function to copy the information
 * in this string if needed.
 *
 * NOTE: These functions are not thread-safe!
 */
const char *bt_hex_real(const void *buf, size_t len);
const char *bt_addr_str_real(const bt_addr_t *addr);
const char *bt_addr_le_str_real(const bt_addr_le_t *addr);
const char *bt_uuid_str_real(const struct bt_uuid *uuid);

/* NOTE: log_strdup does not guarantee a duplication of the string.
 * It is therefore still the responsibility of the user to handle the
 * restrictions in the underlying function call.
 */
#define bt_hex(buf, len) log_strdup(bt_hex_real(buf, len))
#define bt_addr_str(addr) log_strdup(bt_addr_str_real(addr))
#define bt_addr_le_str(addr) log_strdup(bt_addr_le_str_real(addr))
#define bt_uuid_str(uuid) log_strdup(bt_uuid_str_real(uuid))

/* Wait todo */
//#define BT_SYS_ERR(fmt, ...) LOG_ERR(fmt, ##__VA_ARGS__)
//#define BT_SYS_WRN(fmt, ...) LOG_WRN(fmt, ##__VA_ARGS__)
//#define BT_SYS_INF(fmt, ...) LOG_INF(fmt, ##__VA_ARGS__)
//#define BT_SYS_DBG(fmt, ...) LOG_DBG(fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* __BT_LOG_H */
