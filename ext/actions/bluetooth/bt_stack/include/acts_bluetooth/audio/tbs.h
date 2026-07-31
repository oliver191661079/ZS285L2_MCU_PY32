/** @file
 *  @brief Telephone Bearer Service
 */

/*
 * Copyright (c) 2021 Actions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_TBS_H_
#define ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_TBS_H_

#include <toolchain.h>
#include <zephyr/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BT_TBS_ERR_VALUE_CHANGED_DURING_READ_LONG 0x80

#if 0
#define BT_TBS_PROVIDER_NAME_SIZE CONFIG_BT_TBS_BEAR_PROVIDER_NAME_SIZE
#define BT_TBS_URI_SCHEMES_LIST_SIZE CONFIG_BT_TBS_URI_SCHEMES_LIST_SIZE
#define BT_TBS_URI_SIZE CONFIG_BT_TBS_URI_SIZE
#define BT_TBS_LOCAL_URI_SIZE CONFIG_BT_TBS_LOCAL_URI_SIZE
#define BT_TBS_CALL_MAX	CONFIG_BT_TBS_CALL_MAX_NUM
#define BT_TBS_FRIENDLY_NAME_SIZE CONFIG_BT_TBS_BEAR_FRIENDLY_NAME_SIZE
#define BT_TBS_BUF_SIZE	CONFIG_BT_TBS_BUF_SIZE
#else
#define BT_TBS_PROVIDER_NAME_SIZE 24
#define BT_TBS_URI_SCHEMES_LIST_SIZE 24
#define BT_TBS_URI_SIZE 24
#define BT_TBS_LOCAL_URI_SIZE 4
#define BT_TBS_CALL_MAX	3
#define BT_TBS_FRIENDLY_NAME_SIZE 4
#define BT_TBS_BUF_SIZE	80	// TODO: > BT_TBS_URI_SIZE + 1 at least
#define BT_TBS_CLIENT_BUF_SIZE 24
#endif

/*
 * Bearer Signal Strength
 */
#define BT_TBS_SIGNAL_STRENGTH_MIN	0
#define BT_TBS_SIGNAL_STRENGTH_MAX	100
#define BT_TBS_SIGNAL_STRENGTH_INVALID	255

/*
 * Status Flags
 */
/* 0: inband ringtone disabled; 1: inband ringtone enabled */
#define BT_TBS_STATUS_RINGTONE_ENABLED	BIT(0)
/* 0: Server is not in silent mode; 1: Server is in silent mode */
#define BT_TBS_STATUS_SILENT_MODE	BIT(1)
#define BT_TBS_STATUS_MASK	0x0003

/*
 * Call State
 */
#define BT_TBS_CALL_STATE_INCOMING         0x00
#define BT_TBS_CALL_STATE_DIALING          0x01	/* outgoing */
#define BT_TBS_CALL_STATE_ALERTING         0x02	/* outgoing */
#define BT_TBS_CALL_STATE_ACTIVE           0x03
#define BT_TBS_CALL_STATE_LOCALLY_HELD     0x04
#define BT_TBS_CALL_STATE_REMOTELY_HELD    0x05
/* locally and remotely held */
#define BT_TBS_CALL_STATE_HELD             0x06

/*
 * Call Flags
 */
/* 0: Call is an incoming call; 1: Call is an outgoing call */
#define BT_TBS_CALL_FLAGS_OUTGOING_CALL	BIT(0)
// TODO: which URI, target or remote???
/* URI, 0: not withheld by server; 1: withheld by server */
#define BT_TBS_CALL_FLAGS_WITHHELD_BY_SRV	BIT(1)
/* friendly name, 0: provided by network; 1: withheld by network */
#define BT_TBS_CALL_FLAGS_WITHHELD_BY_NET	BIT(2)

struct bt_tbs_current_call {
	uint8_t length;
	uint8_t index;
	uint8_t state;
	uint8_t flags;
	uint8_t uri[0];
} __packed;

struct bt_tbs_call_state {
	uint8_t index;
	uint8_t state;
	uint8_t flags;
} __packed;

struct bt_tbs_incoming_call {
	uint8_t index;
	uint8_t uri[0];
} __packed;

/*
 * create: 1. server/client originates a call; 2. server has a incomming call.
 * delete: when the call is terminated.
 */
struct bt_tbs_call {
	uint8_t index;	/* [1, 255] */
	uint8_t state;	/* Call State */
	uint8_t flags;	/* Call Flags */
#if 1
	uint8_t join : 1;
	uint8_t reserved : 7;
#endif
	/* incoming or outgoing according to flags */
	char remote[BT_TBS_URI_SIZE];
	/* target URI, for incoming call */
	char local[BT_TBS_URI_SIZE];	/* incoming call target (local?) uri */
	char friendly[BT_TBS_FRIENDLY_NAME_SIZE];
};

/*
 * Opcode
 */
#define BT_TBS_OPCODE_ACCEPT            0x00
#define BT_TBS_OPCODE_TERMINATE         0x01
#define BT_TBS_OPCODE_LOCAL_HOLD        0x02
#define BT_TBS_OPCODE_LOCAL_RETRIEVE    0x03
#define BT_TBS_OPCODE_ORIGINATE         0x04
#define BT_TBS_OPCODE_JOIN              0x05

/*
 * Result Code
 */
#define BT_TBS_RESULT_SUCCESS           0x00
#define BT_TBS_RESULT_NOT_SUPPORTED     0x01
#define BT_TBS_RESULT_NOT_POSSIBLE      0x02
#define BT_TBS_RESULT_INVALID_INDEX     0x03
#define BT_TBS_RESULT_STATE_MISMATCH    0x04
#define BT_TBS_RESULT_NO_RESOURCES      0x05
#define BT_TBS_RESULT_INVALID_URI       0x06

struct bt_tbs_response {
	uint8_t opcode;
	uint8_t index;
	uint8_t result;
} __packed;

/*
 * Optional Opcodes
 */
/* 0: Local Hold and Local Retrieve Opcodes not supported;
 * 1: Local Hold and Local Retrieve Opcodes supported */
#define BT_TBS_OPCODE_LOCAL_HOLD_SUPPORTED    BIT(0)
/* 0: Join not supported; 1: Join supported */
#define BT_TBS_OPCODE_JOIN_SUPPORTED          BIT(1)

/*
 * Termination Reason
 */
#define BT_TBS_REASON_URI               0x00
#define BT_TBS_REASON_CALL_FAILED       0x01
#define BT_TBS_REASON_REMOTE_END        0x02
#define BT_TBS_REASON_SERVER_END        0x03
#define BT_TBS_REASON_LINE_BUSY         0x04
#define BT_TBS_REASON_NETWORK           0x05
#define BT_TBS_REASON_CLIENT_END        0x06
#define BT_TBS_REASON_NO_SERVICE        0x07
#define BT_TBS_REASON_NO_ANSWER         0x08
#define BT_TBS_REASON_UNSPECIFIED       0x09

struct bt_tbs_reason {
	uint8_t index;
	uint8_t code;
} __packed;

struct bt_tbs_friendly {
	uint8_t index;
	uint8_t name[0];
} __packed;

/* Bearer Technology */
#define BT_TBS_TECHNOLOGY_3G                       0x01
#define BT_TBS_TECHNOLOGY_4G                       0x02
#define BT_TBS_TECHNOLOGY_LTE                      0x03
#define BT_TBS_TECHNOLOGY_WIFI                     0x04
#define BT_TBS_TECHNOLOGY_5G                       0x05
#define BT_TBS_TECHNOLOGY_GSM                      0x06
#define BT_TBS_TECHNOLOGY_CDMA                     0x07
#define BT_TBS_TECHNOLOGY_2G                       0x08
#define BT_TBS_TECHNOLOGY_WCDMA                    0x09
#define BT_TBS_TECHNOLOGY_IP                       0x0a

struct bt_tbs_server;

typedef struct bt_tbs_server *(*bt_tbs_lookup_cb)(struct bt_conn *conn,
				uint16_t handle);

typedef void (*bt_tbs_server_call_cb)(struct bt_conn *conn,
				struct bt_tbs_server *srv,
				struct bt_tbs_call *call, int err);

struct bt_tbs_server_cb {
	bt_tbs_lookup_cb lookup;
	bt_tbs_server_call_cb control;
	/* err > 0 means termination reason */
	bt_tbs_server_call_cb terminate;
};

struct bt_tbs_server {
	struct bt_gatt_service *service_p;

	struct k_delayed_work interval_work;

	char provider[BT_TBS_PROVIDER_NAME_SIZE];
	char schemes[BT_TBS_URI_SCHEMES_LIST_SIZE];
	char uci[6];	/* client_id, should be enough? */
	uint8_t technology;
	uint8_t strength;
	uint8_t interval;	/* default: 0; unit: second */
	uint8_t last_index;	/* last allocated index */

	uint8_t ccid;
	uint16_t status;	/* Status Flags */
	uint16_t opcodes;	/* Optional Opcodes */

	struct bt_tbs_call calls[BT_TBS_CALL_MAX];
	uint8_t buf[BT_TBS_BUF_SIZE];
};

ssize_t bt_tbs_read_provider(struct bt_conn *conn,
				const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset);

ssize_t bt_tbs_read_uci(struct bt_conn *conn,
				const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset);

ssize_t bt_tbs_read_technology(struct bt_conn *conn,
				const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset);

ssize_t bt_tbs_read_schemes(struct bt_conn *conn,
				const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset);

ssize_t bt_tbs_read_strength(struct bt_conn *conn,
				const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset);

ssize_t bt_tbs_read_interval(struct bt_conn *conn,
				const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset);

ssize_t bt_tbs_write_interval(struct bt_conn *conn,
				const struct bt_gatt_attr *attr,
				const void *buf, uint16_t len,
				uint16_t offset, uint8_t flags);

ssize_t bt_tbs_read_calls(struct bt_conn *conn,
				const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset);

ssize_t bt_tbs_read_ccid(struct bt_conn *conn,
				const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset);

ssize_t bt_tbs_read_status(struct bt_conn *conn,
				const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset);

ssize_t bt_tbs_read_target(struct bt_conn *conn,
				const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset);

ssize_t bt_tbs_read_state(struct bt_conn *conn,
				const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset);

ssize_t bt_tbs_write_control(struct bt_conn *conn,
				const struct bt_gatt_attr *attr,
				const void *buf, uint16_t len,
				uint16_t offset, uint8_t flags);

ssize_t bt_tbs_read_opcodes(struct bt_conn *conn,
				const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset);

ssize_t bt_tbs_read_incoming(struct bt_conn *conn,
				const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset);

ssize_t bt_tbs_read_friendly(struct bt_conn *conn,
				const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset);

int bt_tbs_server_provider_set(struct bt_conn *conn, struct bt_tbs_server *srv,
				const char *provider);

int bt_tbs_server_uci_set(struct bt_conn *conn, struct bt_tbs_server *srv,
				const char *uci);

int bt_tbs_server_technology_set(struct bt_conn *conn, struct bt_tbs_server *srv,
				uint8_t technology);

int bt_tbs_server_schemes_set(struct bt_conn *conn, struct bt_tbs_server *srv,
				const char *schemes);

int bt_tbs_server_strength_set(struct bt_conn *conn, struct bt_tbs_server *srv,
				uint8_t strength);

int bt_tbs_server_ccid_set(struct bt_conn *conn, struct bt_tbs_server *srv,
				uint8_t ccid);

int bt_tbs_server_status_set(struct bt_conn *conn, struct bt_tbs_server *srv,
				uint16_t status);

int bt_tbs_server_target_set(struct bt_conn *conn, struct bt_tbs_server *srv,
				uint8_t index, const char *target);

int bt_tbs_server_incoming_call(struct bt_conn *conn,
				struct bt_tbs_server *srv, const char *uri);

int bt_tbs_server_remote_alerting_call(struct bt_conn *conn,
				struct bt_tbs_server *srv, uint8_t index);

int bt_tbs_server_remote_accept_call(struct bt_conn *conn,
				struct bt_tbs_server *srv, uint8_t index);

int bt_tbs_server_remote_hold_call(struct bt_conn *conn,
				struct bt_tbs_server *srv, uint8_t index);

int bt_tbs_server_remote_retrieve_call(struct bt_conn *conn,
				struct bt_tbs_server *srv, uint8_t index);

int bt_tbs_server_originate_call(struct bt_conn *conn,
				struct bt_tbs_server *srv, const char *uri);

int bt_tbs_server_accept_call(struct bt_conn *conn, struct bt_tbs_server *srv,
				uint8_t index);

int bt_tbs_server_hold_call(struct bt_conn *conn, struct bt_tbs_server *srv,
				uint8_t index);

int bt_tbs_server_retrieve_call(struct bt_conn *conn,
				struct bt_tbs_server *srv, uint8_t index);

int bt_tbs_server_terminate_call(struct bt_conn *conn,
				struct bt_tbs_server *srv, uint8_t index,
				uint8_t reason_code);

int bt_tbs_server_join_call(struct bt_conn *conn, struct bt_tbs_server *srv,
				uint8_t num, uint8_t *index_array);

int bt_tbs_server_opcodes_set(struct bt_conn *conn, struct bt_tbs_server *srv,
				uint16_t opcodes);

int bt_tbs_server_friendly_set(struct bt_conn *conn, struct bt_tbs_server *srv,
				uint8_t index, const char *friendly);

int bt_tbs_server_call_state_notify(struct bt_tbs_server *srv);

void bt_tbs_server_cb_register(struct bt_tbs_server_cb *cb);

void bt_tbs_server_exit(struct bt_tbs_server *srv);

struct bt_tbs_client;

typedef void (*bt_tbs_discover_cb)(struct bt_conn *conn,
				struct bt_tbs_client *cli, int err);

/* callback for read or notify */
typedef void (*bt_tbs_client_common_cb)(struct bt_conn *conn,
				struct bt_tbs_client *cli, int err);

typedef void (*bt_tbs_client_call_cb)(struct bt_conn *conn,
				struct bt_tbs_client *cli,
				struct bt_tbs_call *call, int err);

struct bt_tbs_client_cb {
	bt_tbs_discover_cb discover;
	bt_tbs_client_call_cb update_call;
	bt_tbs_client_call_cb incoming_call;
	bt_tbs_client_common_cb provider;
	bt_tbs_client_common_cb uci;
	bt_tbs_client_common_cb technology;
	bt_tbs_client_common_cb schemes;
	bt_tbs_client_common_cb strength;
	bt_tbs_client_common_cb interval;
	bt_tbs_client_common_cb calls;
	bt_tbs_client_common_cb ccid;
	bt_tbs_client_common_cb status;
	bt_tbs_client_call_cb target;
	bt_tbs_client_common_cb state;
	bt_tbs_client_common_cb control;
	bt_tbs_client_common_cb opcodes;
	bt_tbs_client_call_cb reason;
	bt_tbs_client_call_cb friendly;
};

struct bt_tbs_client {
	uint16_t start_handle;
	uint16_t end_handle;
	uint16_t provider_handle;
	uint16_t uci_handle;
	uint16_t technology_handle;
	uint16_t schemes_handle;
	uint16_t strength_handle;
	uint16_t interval_handle;
	uint16_t calls_handle;
	uint16_t ccid_handle;
	uint16_t status_handle;
	uint16_t target_handle;
	uint16_t state_handle;
	uint16_t control_handle;
	uint16_t opcodes_handle;
	uint16_t reason_handle;
	uint16_t incoming_handle;
	uint16_t friendly_handle;
	struct bt_gatt_subscribe_params provider_sub_params;
	struct bt_gatt_subscribe_params technology_sub_params;
	struct bt_gatt_subscribe_params schemes_sub_params;
	struct bt_gatt_subscribe_params strength_sub_params;
	struct bt_gatt_subscribe_params interval_sub_params;
	struct bt_gatt_subscribe_params calls_sub_params;
	struct bt_gatt_subscribe_params status_sub_params;
	struct bt_gatt_subscribe_params target_sub_params;
	struct bt_gatt_subscribe_params state_sub_params;
	struct bt_gatt_subscribe_params control_sub_params;
	struct bt_gatt_subscribe_params reason_sub_params;
	struct bt_gatt_subscribe_params incoming_sub_params;
	struct bt_gatt_subscribe_params friendly_sub_params;

	struct bt_gatt_write_params write_params;
	struct bt_gatt_read_params read_params;
	struct bt_gatt_discover_params discover_params;

	struct bt_tbs_client_cb *cb;
#if 0
	struct bt_conn *conn;
#endif

	uint8_t busy;
#if 0
	uint8_t read_op;
#endif

	struct bt_uuid_16 uuid;

	char provider[BT_TBS_PROVIDER_NAME_SIZE];
	char schemes[BT_TBS_URI_SCHEMES_LIST_SIZE];
	char uci[6];	/* client_id, should be enough? */
	uint8_t technology;
	uint8_t strength;
	uint8_t interval;	/* default: 0; unit: second */

	uint8_t ccid;
	uint16_t status;	/* Status Flags */
	uint16_t opcodes;	/* Optional Opcodes */

	struct bt_tbs_call calls[BT_TBS_CALL_MAX];
	uint8_t buf[BT_TBS_CLIENT_BUF_SIZE];
};

int bt_tbs_client_read_provider(struct bt_conn *conn,
				struct bt_tbs_client *cli);

int bt_tbs_client_read_uci(struct bt_conn *conn, struct bt_tbs_client *cli);

int bt_tbs_client_read_technology(struct bt_conn *conn,
				struct bt_tbs_client *cli);

int bt_tbs_client_read_schemes(struct bt_conn *conn,
				struct bt_tbs_client *cli);

int bt_tbs_client_read_strength(struct bt_conn *conn,
				struct bt_tbs_client *cli);

int bt_tbs_client_read_interval(struct bt_conn *conn,
				struct bt_tbs_client *cli);

int bt_tbs_client_write_interval(struct bt_conn *conn,
				struct bt_tbs_client *cli, uint8_t interval);

int bt_tbs_client_write_interval_no_rsp(struct bt_conn *conn,
				struct bt_tbs_client *cli, uint8_t interval);

int bt_tbs_client_read_current_calls(struct bt_conn *conn,
				struct bt_tbs_client *cli);

int bt_tbs_client_read_ccid(struct bt_conn *conn, struct bt_tbs_client *cli);

int bt_tbs_client_read_status(struct bt_conn *conn, struct bt_tbs_client *cli);

int bt_tbs_client_read_target(struct bt_conn *conn,
				struct bt_tbs_client *cli);

int bt_tbs_client_read_state(struct bt_conn *conn,
				struct bt_tbs_client *cli);

int bt_tbs_client_write_control(struct bt_conn *conn, struct bt_tbs_client *cli,
				uint8_t index, uint8_t opcode);

int bt_tbs_client_accept_call(struct bt_conn *conn, struct bt_tbs_client *cli,
				uint8_t index);

int bt_tbs_client_hold_call(struct bt_conn *conn, struct bt_tbs_client *cli,
				uint8_t index);

int bt_tbs_client_retrieve_call(struct bt_conn *conn,
				struct bt_tbs_client *cli, uint8_t index);

int bt_tbs_client_originate_call(struct bt_conn *conn,
				struct bt_tbs_client *cli, const char *uri);

int bt_tbs_client_join_call(struct bt_conn *conn, struct bt_tbs_client *cli,
				uint8_t num, uint8_t *index_array);

int bt_tbs_client_terminate_call(struct bt_conn *conn,
				struct bt_tbs_client *cli, uint8_t index);

int bt_tbs_client_read_opcodes(struct bt_conn *conn,
				struct bt_tbs_client *cli);

int bt_tbs_client_read_incoming(struct bt_conn *conn,
				struct bt_tbs_client *cli);

int bt_tbs_client_read_friendly(struct bt_conn *conn,
				struct bt_tbs_client *cli);

int bt_tbs_discover(struct bt_conn *conn, struct bt_tbs_client *cli, bool gtbs);

void bt_tbs_client_cb_register(struct bt_tbs_client_cb *cb);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_TBS_H_ */
