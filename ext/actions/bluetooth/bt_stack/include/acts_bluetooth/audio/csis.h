/** @file
 *  @brief Coordinated Set Identification Service
 */

/*
 * Copyright (c) 2021 Actions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_CSIS_H_
#define ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_CSIS_H_

#include <misc/util.h>
#include <acts_bluetooth/conn.h>

#ifdef __cplusplus
extern "C" {
#endif

/* set_member_discovery_timeout */
#define BT_CSIS_DISCOVERY_TIMEOUT	K_SECONDS(10)

/* error code */
#define BT_CSIS_ERR_LOCK_DENIED	0x80
#define BT_CSIS_ERR_LOCK_RELEASE_NOT_ALLOWED	0x81
#define BT_CSIS_ERR_INVALID_LOCK_VALUE	0x82
/* SIRK can be read via OOB only */
#define BT_CSIS_ERR_OOB_SIRK_ONLY	0x83
#define BT_CSIS_ERR_LOCK_ALREADY_GRANTED	0x84

/* sirk type */
#define BT_CSIS_SIRK_ENCRYPTED	0x00
#define BT_CSIS_SIRK_PLAIN	0x01

/* size range */
#define BT_CSIS_SIZE_MIN	0x01
#define BT_CSIS_SIZE_MAX	0xff

/* lock state */
#define BT_CSIS_UNLOCKED	0x01
#define BT_CSIS_LOCKED	0x02

#define BT_CSIS_PSRI_SIZE	6

/* lock_timeout */
#define BT_CSIS_LOCK_TIMEOUT	K_SECONDS(60)

/* rank range */
#define BT_CSIS_RANK_MIN	0x01
#define BT_CSIS_RANK_MAX	0xff

struct bt_csis_sirk {
	uint8_t type;
	uint8_t value[16];
} __packed;

struct bt_csis_server;

typedef struct bt_csis_server *(*bt_csis_lookup_cb)(struct bt_conn *conn,
				uint16_t handle);

struct bt_csis_server_cb {
	bt_csis_lookup_cb lookup;
};

struct bt_csis_server {
	struct bt_gatt_service *service_p;

	uint8_t size;
	uint8_t lock;
	uint8_t rank;

	uint8_t psri[BT_CSIS_PSRI_SIZE];
	uint8_t sirk_oob_only;	/* SIRK could be read via OOB only */
	struct bt_csis_sirk sirk;

	bt_addr_le_t lock_client_addr;

	uint8_t is_bonded_client;

	struct k_delayed_work lock_work;
};

struct bt_csis_client;

typedef void (*bt_csis_discover_cb)(struct bt_conn *conn,
				struct bt_csis_client *cli, int err);

typedef void (*bt_csis_client_common_cb)(struct bt_conn *conn,
				struct bt_csis_client *cli, int err);

struct bt_csis_client_cb {
	bt_csis_discover_cb discover;
	bt_csis_client_common_cb sirk;
	bt_csis_client_common_cb size;
	bt_csis_client_common_cb lock;
	bt_csis_client_common_cb rank;
};

struct bt_csis_client {
	uint16_t start_handle;
	uint16_t end_handle;

	uint16_t sirk_handle;
	uint16_t size_handle;
	uint16_t lock_handle;
	uint16_t rank_handle;

	uint8_t busy;
	struct bt_uuid_16 uuid;

	uint8_t size;
	uint8_t lock_value;
	uint8_t lock;
	uint8_t rank;
	struct bt_csis_sirk sirk;

	struct bt_gatt_subscribe_params sirk_sub_params;
	struct bt_gatt_subscribe_params size_sub_params;
	struct bt_gatt_subscribe_params lock_sub_params;

	struct bt_gatt_write_params write_params;
	struct bt_gatt_read_params read_params;
	struct bt_gatt_discover_params discover_params;
};

ssize_t bt_csis_read_sirk(struct bt_conn *conn,
				const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset);

ssize_t bt_csis_read_size(struct bt_conn *conn,
				const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset);

ssize_t bt_csis_read_lock(struct bt_conn *conn,
				const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset);

ssize_t bt_csis_write_lock(struct bt_conn *conn,
				const struct bt_gatt_attr *attr,
				const void *buf, uint16_t len,
				uint16_t offset, uint8_t flags);

ssize_t bt_csis_read_rank(struct bt_conn *conn,
				const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset);

int bt_csis_server_sirk_set(struct bt_csis_server *srv, uint8_t type,
			uint8_t value[16], uint8_t oob_only);

int bt_csis_server_size_set(struct bt_csis_server *srv, uint8_t size);

int bt_csis_server_update_psri(struct bt_csis_server *srv);

int bt_csis_server_disconnect(struct bt_csis_server *srv, struct bt_conn *conn);

int bt_csis_server_init(struct bt_csis_server *srv, uint8_t size, uint8_t rank,
				uint8_t sirk_type, uint8_t *sirk_value,
				uint8_t oob_only);

int bt_csis_server_set_local_rank(struct bt_csis_server *srv, uint8_t rank);

int bt_csis_server_set_lock(struct bt_csis_server *srv, struct bt_conn *conn, uint8_t val);

int bt_csis_server_lock_notify(struct bt_csis_server *srv, struct bt_conn *conn);

void bt_csis_server_cb_register(struct bt_csis_server_cb *cb);

int bt_csis_client_read_sirk(struct bt_conn *conn, struct bt_csis_client *cli);

int bt_csis_client_read_size(struct bt_conn *conn, struct bt_csis_client *cli);

int bt_csis_client_read_lock(struct bt_conn *conn, struct bt_csis_client *cli);

int bt_csis_client_lock(struct bt_conn *conn, struct bt_csis_client *cli);

int bt_csis_client_unlock(struct bt_conn *conn, struct bt_csis_client *cli);

int bt_csis_client_read_rank(struct bt_conn *conn, struct bt_csis_client *cli);

int bt_csis_discover(struct bt_conn *conn, struct bt_csis_client *cli);

void bt_csis_client_cb_register(struct bt_csis_client_cb *cb);

bool bt_csis_client_is_set_member(uint8_t set_sirk[16], struct bt_data *data);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_CSIS_H_ */
