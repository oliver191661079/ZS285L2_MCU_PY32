/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_BLUETOOTH_SERVICES_FPMS_H_
#define ZEPHYR_INCLUDE_BLUETOOTH_SERVICES_FPMS_H_

/**
 * @brief Fast Pair Message Stream Service (FPMS)
 * @ingroup bluetooth
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

struct bt_fpms_app_cb {
	void (*connected)(struct bt_conn *conn);
	void (*disconnected)(struct bt_conn *conn);
	void (*recv)(struct bt_conn *conn, uint8_t *data, uint16_t len);
};

int bt_fpms_register_cb(struct bt_fpms_app_cb *cb);

int bt_fpms_send(struct bt_conn *conn,uint8_t *data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_BLUETOOTH_SERVICES_FPMS_H_ */
