/** @file
 *  @brief Bluetooth Appearance Assigned Numbers.
 */

/*
 * Copyright (c) 2023 Actions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_BLUETOOTH_APPEARANCE_H_
#define ZEPHYR_INCLUDE_BLUETOOTH_APPEARANCE_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Appearance Catgeory */
#define BT_APPEAR_CAT_UNKNOWN     0x000
#define BT_APPEAR_CAT_PHONE       0x001
#define BT_APPEAR_CAT_COMPUTER    0x002
#define BT_APPEAR_CAT_WATCH       0x003
#define BT_APPEAR_CAT_CLOCK       0x004
#define BT_APPEAR_CAT_DISPLAY     0x005
#define BT_APPEAR_CAT_REMOTE    0x006
#define BT_APPEAR_CAT_GLASSES    0x007
#define BT_APPEAR_CAT_TAG    0x008
#define BT_APPEAR_CAT_KEYRING    0x009
#define BT_APPEAR_CAT_MPLAYER    0x00A
#define BT_APPEAR_CAT_BAR_SCANNER    0x00B
#define BT_APPEAR_CAT_COMPUTER    0x00C
#define BT_APPEAR_CAT_COMPUTER    0x00D

/* Appearance Subcatgeory of BT_APPEAR_COMPUTER */
#define BT_APPEAR_SUB_GENERIC    0x00

/* Appearance */


static inline uint16_t bt_appearance_category(uint16_t value)
{
	return (value >> 6);
}

static inline uint16_t bt_appearance_subcategory(uint16_t value)
{
	return (value & 0x3F);
}

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_BLUETOOTH_APPEARANCE_H_ */
