/** @file
 *  @brief Common Audio Service
 */

/*
 * Copyright (c) 2021 Actions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_CAS_H_
#define ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_CAS_H_

#include <misc/util.h>
#include <acts_bluetooth/audio/csis.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_cas_client {
	uint16_t start_handle;
	uint16_t end_handle;
	struct bt_uuid_16 uuid;

	/* indicate CSIS is found */
	uint8_t csis_found;

	struct bt_gatt_discover_params discover_params;

	struct bt_csis_client csis;
};

typedef void (*bt_cas_discover_cb)(struct bt_conn *conn,
				struct bt_cas_client *cli, int err);

struct bt_cas_client_cb {
	bt_cas_discover_cb discover;
};

int bt_cas_discover(struct bt_conn *conn, struct bt_cas_client *cli);

int bt_cas_csis_discover(struct bt_conn *conn, struct bt_cas_client *cli);


void bt_cas_client_cb_register(struct bt_cas_client_cb *cb);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_CAS_H_ */
