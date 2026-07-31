/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_BLUETOOTH_SERVICES_VOCS_H_
#define ZEPHYR_INCLUDE_BLUETOOTH_SERVICES_VOCS_H_

/**
 * @brief Volume Offset Control Service (VOCS)
 *
 * @defgroup bt_gatt_vocs Volume Offset Control Service (VOCS)
 *
 * @ingroup bluetooth
 * @{
 *
 * The Volume Offset Control Service is a secondary service, and as such should not be used own its
 * own, but rather in the context of another (primary) service.
 *
 * This API implements both the server and client functionality.
 * Note that the API abstracts away the change counter in the volume offset control state and will
 * automatically handle any changes to that. If out of date, the client implementation will
 * autonomously read the change counter value when executing a write request.
 *
 * [Experimental] Users should note that the APIs can change as a part of ongoing development.
 */

#include <zephyr/types.h>
#include <acts_bluetooth/gatt.h>
#include <acts_bluetooth/bluetooth.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Volume Offset Control Service Error codes */
#define BT_VOCS_ERR_INVALID_COUNTER                0x80
#define BT_VOCS_ERR_OP_NOT_SUPPORTED               0x81
#define BT_VOCS_ERR_OUT_OF_RANGE                   0x82

#define BT_VOCS_MIN_OFFSET                         -255
#define BT_VOCS_MAX_OFFSET                         255

/** @brief Opaque Volume Offset Control Service instance. */
struct bt_vocs;
struct bt_vocs_server;
struct bt_vocs_client;

/** @brief Structure for registering a Volume Offset Control Service instance. */
struct bt_vocs_register_param {
	/** Audio Location bitmask */
	uint32_t location;

	/** Boolean to set whether the location is writable by clients */
	//bool location_writable;

	/** Initial volume offset (-255 to 255) */
	int16_t offset;

	/** Initial audio output description */
	char *output_desc;

	/** Boolean to set whether the description is writable by clients */
	//bool desc_writable;

	/** Pointer to the callback structure. */
	struct bt_vocs_cb *cb;
};

/** @brief Structure for discovering a Volume Offset Control Service instance. */
struct bt_vocs_discover_param {
	/**
	 * @brief The start handle of the discovering.
	 *
	 * Typically the @p start_handle of a @ref bt_gatt_include.
	 */
	uint16_t start_handle;
	/**
	 * @brief The end handle of the discovering.
	 *
	 * Typically the @p end_handle of a @ref bt_gatt_include.
	 */
	uint16_t end_handle;
};

/**
 * @brief Get a free service instance of Volume Offset Control Service from the pool.
 *
 * @return Volume Offset Control Service instance in case of success or NULL in case of error.
 */
struct bt_vocs *bt_vocs_free_instance_get(void);

/**
 * @brief Get the service declaration attribute.
 *
 * The first service attribute returned can be included in any other GATT service.
 *
 * @param vocs Volume Offset Control Service instance.
 *
 * @return Pointer to the attributes of the service.
 */
void *bt_vocs_svc_decl_get(struct bt_vocs *vocs);

/**
 * @brief Register the Volume Offset Control Service instance.
 *
 * @param vocs      Volume Offset Control Service instance.
 * @param param     Volume Offset Control Service register parameters.
 *
 * @return 0 if success, errno on failure.
 */
int bt_vocs_register(struct bt_vocs *vocs,
		     const struct bt_vocs_register_param *param);

/**
 * @brief Callback function for the offset state.
 *
 * Called when the value is read, or if the value is changed by either the server or client.
 *
 * @param conn        Connection to peer device, or NULL if local server read.
 * @param inst        The instance pointer.
 * @param err         Error value. 0 on success, GATT error on positive value
 *                    or errno on negative value.
 *                    For notifications, this will always be 0.
 * @param offset      The offset value.
 */
typedef void (*bt_vocs_state_cb_t)(struct bt_conn *conn, struct bt_vocs *inst,
				   int err, int16_t offset);

/**
 * @brief Callback function for setting offset.
 *
 * @param conn        Connection to peer device, or NULL if local server write.
 * @param inst        The instance pointer.
 * @param err         Error value. 0 on success, GATT error on positive value
 *                    or errno on negative value.
 */
typedef void (*bt_vocs_set_offset_cb_t)(struct bt_conn *conn, struct bt_vocs *inst, int err);

/**
 * @brief Callback function for the location.
 *
 * Called when the value is read, or if the value is changed by either the server or client.
 *
 * @param conn         Connection to peer device, or NULL if local server read.
 * @param inst         The instance pointer.
 * @param err          Error value. 0 on success, GATT error on positive value
 *                     or errno on negative value.
 *                     For notifications, this will always be 0.
 * @param location     The location value.
 */
typedef void (*bt_vocs_location_cb_t)(struct bt_conn *conn, struct bt_vocs *inst, int err,
				      uint32_t location);

/**
 * @brief Callback function for the description.
 *
 * Called when the value is read, or if the value is changed by either the server or client.
 *
 * @param conn         Connection to peer device, or NULL if local server read.
 * @param inst         The instance pointer.
 * @param err          Error value. 0 on success, GATT error on positive value
 *                     or errno on negative value.
 *                     For notifications, this will always be 0.
 * @param description  The description as an UTF-8 encoded string.
 */
typedef void (*bt_vocs_description_cb_t)(struct bt_conn *conn, struct bt_vocs *inst, int err,
					 char *description);

/**
 * @brief Callback function for bt_vocs_discover.
 *
 * This callback should be overwritten by the primary service that
 * includes the Volume Control Offset Service client, and should thus not be
 * set by the application.
 *
 * @param conn         Connection to peer device, or NULL if local server read.
 * @param inst         The instance pointer.
 * @param err          Error value. 0 on success, GATT error on positive value
 *                     or errno on negative value.
 *                     For notifications, this will always be 0.
 */
typedef void (*bt_vocs_discover_cb_t)(struct bt_conn *conn, struct bt_vocs *inst, int err);

struct bt_vocs_cb {
	bt_vocs_state_cb_t              state;
	bt_vocs_location_cb_t           location;
	bt_vocs_description_cb_t        description;

	/* Client only */
	bt_vocs_discover_cb_t           discover;
	bt_vocs_set_offset_cb_t         set_offset;
};

typedef struct bt_vocs_server *(*bt_vocs_lookup_cb_t)(struct bt_conn *conn, uint16_t handle);

/**
 * @brief Read the Volume Offset Control Service offset state.
 *
 * The value is returned in the bt_vocs_cb.state callback.
 *
 * @param conn          Connection to peer device, or NULL to read local server value.
 * @param inst          Pointer to the Volume Offset Control Service instance.
 *
 * @return 0 on success, GATT error value on fail.
 */
int bt_vocs_state_get(struct bt_conn *conn, struct bt_vocs *inst);

/**
 * @brief Set the Volume Offset Control Service offset state.
 *
 * @param conn          Connection to peer device, or NULL to set local server value.
 * @param inst          Pointer to the Volume Offset Control Service instance.
 * @param offset        The offset to set (-255 to 255).
 *
 * @return 0 on success, GATT error value on fail.
 */
int bt_vocs_state_set(struct bt_conn *conn, struct bt_vocs *inst, int16_t offset);

/**
 * @brief Read the Volume Offset Control Service location.
 *
 * The value is returned in the bt_vocs_cb.location callback.
 *
 * @param conn          Connection to peer device, or NULL to read local server value.
 * @param inst          Pointer to the Volume Offset Control Service instance.
 *
 * @return 0 on success, GATT error value on fail.
 */
int bt_vocs_location_get(struct bt_conn *conn, struct bt_vocs *inst);

/**
 * @brief Set the Volume Offset Control Service location.
 *
 * @param conn          Connection to peer device, or NULL to read local server value.
 * @param inst          Pointer to the Volume Offset Control Service instance.
 * @param location      The location to set.
 *
 * @return 0 on success, GATT error value on fail.
 */
int bt_vocs_location_set(struct bt_conn *conn, struct bt_vocs *inst, uint32_t location);

/**
 * @brief Read the Volume Offset Control Service output description.
 *
 * The value is returned in the bt_vocs_cb.description callback.
 *
 * @param conn          Connection to peer device, or NULL to read local server value.
 * @param inst          Pointer to the Volume Offset Control Service instance.
 *
 * @return 0 on success, GATT error value on fail.
 */
int bt_vocs_description_get(struct bt_conn *conn, struct bt_vocs *inst);

/**
 * @brief Set the Volume Offset Control Service description.
 *
 * @param conn          Connection to peer device, or NULL to set local server value.
 * @param inst          Pointer to the Volume Offset Control Service instance.
 * @param description   The UTF-8 encoded string description to set.
 *
 * @return 0 on success, GATT error value on fail.
 */
int bt_vocs_description_set(struct bt_conn *conn, struct bt_vocs *inst,
			    const char *description);

/**
 * @brief Registers the callbacks for the Volume Offset Control Service client.
 *
 * @param inst  Pointer to the Volume Offset Control Service client instance.
 * @param cb    Pointer to the callback structure.
 */
void bt_vocs_client_cb_register(struct bt_vocs *inst, struct bt_vocs_cb *cb);

/**
 * @brief Discover a Volume Offset Control Service.
 *
 * Attempts to discover a Volume Offset Control Service on a server given the @p param.
 *
 * @param conn  Connection to the peer with the Volume Offset Control Service.
 * @param inst  Pointer to the Volume Offset Control Service client instance.
 * @param param Pointer to the parameters.
 *
 * @return 0 on success, errno on fail.
 */
int bt_vocs_discover(struct bt_conn *conn, struct bt_vocs *inst,
		     const struct bt_vocs_discover_param *param);

void bt_vocs_server_cb_register(bt_vocs_lookup_cb_t cb);

ssize_t bt_vocs_read_offset_state(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				 void *buf, uint16_t len, uint16_t offset);

ssize_t bt_vocs_write_location(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			      const void *buf, uint16_t len, uint16_t offset, uint8_t flags);

ssize_t bt_vocs_read_location(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			     void *buf, uint16_t len, uint16_t offset);

ssize_t bt_vocs_write_control(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				  const void *buf, uint16_t len, uint16_t offset, uint8_t flags);

ssize_t bt_vocs_write_output_desc(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				 const void *buf, uint16_t len, uint16_t offset, uint8_t flags);

ssize_t bt_vocs_read_output_desc(struct bt_conn *conn, const struct bt_gatt_attr *attr,
				void *buf, uint16_t len, uint16_t offset);

#if defined(CONFIG_BT_VOCS)
#define BT_VOCS_MAX_DESC_SIZE CONFIG_BT_VOCS_MAX_OUTPUT_DESCRIPTION_SIZE
#else
#define BT_VOCS_MAX_DESC_SIZE 1
#endif /* CONFIG_BT_VOCS */

/* VOCS opcodes */
#define BT_VOCS_OPCODE_SET_OFFSET                  0x01

struct bt_vocs_control {
	uint8_t opcode;
	uint8_t counter;
	int16_t offset;
} __packed;

struct bt_vocs_state {
	int16_t offset;
	uint8_t change_counter;
} __packed;

struct bt_vocs_client {
	sys_snode_t node;
	struct bt_vocs_state state;
	bool location_writable;
	uint32_t location;
	char desc[BT_VOCS_MAX_DESC_SIZE];
	bool desc_writable;
	bool active;

	uint16_t start_handle;
	uint16_t end_handle;
	uint16_t state_handle;
	uint16_t location_handle;
	uint16_t control_handle;
	uint16_t desc_handle;
	struct bt_gatt_subscribe_params state_sub_params;
	struct bt_gatt_subscribe_params location_sub_params;
	struct bt_gatt_subscribe_params desc_sub_params;
	uint8_t subscribe_cnt;
	bool cp_retried;

	bool busy;
	struct bt_vocs_control cp;
	struct bt_gatt_write_params write_params;
	struct bt_gatt_read_params read_params;
	struct bt_vocs_cb *cb;
	struct bt_gatt_discover_params discover_params;
	struct bt_conn *conn;
};

struct bt_vocs_server {
	sys_snode_t node;
	struct bt_vocs_state state;
	uint32_t location;
	bool initialized;
	char output_desc[BT_VOCS_MAX_DESC_SIZE];
	struct bt_vocs_cb *cb;

	struct bt_gatt_service *service_p;
};

struct bt_vocs {
	union {
		struct bt_vocs_server srv;
		struct bt_vocs_client cli;
	};
};

int bt_vocs_client_state_get(struct bt_conn *conn, struct bt_vocs *inst);
int bt_vocs_client_state_set(struct bt_conn *conn, struct bt_vocs *inst, int16_t offset);
int bt_vocs_client_location_get(struct bt_conn *conn, struct bt_vocs *inst);
int bt_vocs_client_location_set(struct bt_conn *conn, struct bt_vocs *inst, uint32_t location);
int bt_vocs_client_description_get(struct bt_conn *conn, struct bt_vocs *inst);
int bt_vocs_client_description_set(struct bt_conn *conn, struct bt_vocs *inst,
				   const char *description);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ZEPHYR_INCLUDE_BLUETOOTH_SERVICES_VOCS_H_ */
