/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief
*/

#ifndef __BT_MANAGER_GATT_OVER_BR_H__
#define __BT_MANAGER_GATT_OVER_BR_H__
#include <acts_bluetooth/conn.h>
#include <acts_bluetooth/gatt.h>

/**
 * @brief get mtu
 *
 * This routine provides to get mtu
 *
 * @return mtu
 */
uint16_t bt_manager_get_gatt_over_br_mtu(struct bt_conn *conn);

#endif  // __BT_MANAGER_GATT_OVER_BR_H__
