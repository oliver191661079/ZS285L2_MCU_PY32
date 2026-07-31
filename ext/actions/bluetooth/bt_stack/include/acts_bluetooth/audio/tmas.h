/** @file
 *  @brief Telephony and Media Audio Service
 */

/*
 * Copyright (c) 2021 Actions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_TMAS_H_
#define ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_TMAS_H_

#include <misc/util.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * If support BMS only, no need to instantiate TMAS, else need.
 */
#define BT_TMAS_ROLE_CG	BIT(0)
#define BT_TMAS_ROLE_CT	BIT(1)
#define BT_TMAS_ROLE_UMS	BIT(2)
#define BT_TMAS_ROLE_UMR	BIT(3)
#define BT_TMAS_ROLE_BMS	BIT(4)
#define BT_TMAS_ROLE_BMR	BIT(5)
#define BT_TMAS_ROLE_MASK	0x003F

ssize_t bt_tmas_read_role(struct bt_conn *conn,
				const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset);

int bt_tmas_server_role_init(uint16_t role);

struct bt_tmas_client;

typedef void (*bt_tmas_discover_cb)(struct bt_conn *conn,
				struct bt_tmas_client *cli, int err);

typedef void (*bt_tmas_client_common_cb)(struct bt_conn *conn,
				struct bt_tmas_client *cli, int err);

struct bt_tmas_client_cb {
	bt_tmas_discover_cb discover;
	bt_tmas_client_common_cb role;
};

struct bt_tmas_client {
	uint16_t start_handle;
	uint16_t end_handle;
	uint16_t role_handle;
	uint16_t role;

	uint8_t busy;
	struct bt_uuid_16 uuid;

	struct bt_gatt_read_params read_params;
	struct bt_gatt_discover_params discover_params;
};

int bt_tmas_client_read_role(struct bt_conn *conn, struct bt_tmas_client *cli);

int bt_tmas_discover(struct bt_conn *conn, struct bt_tmas_client *cli);

void bt_tmas_client_cb_register(struct bt_tmas_client_cb *cb);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_TMAS_H_ */
