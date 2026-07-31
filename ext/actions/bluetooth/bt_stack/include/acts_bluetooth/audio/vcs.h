/*
 * Copyright (c) 2020-2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_BLUETOOTH_SERVICES_VCS_H_
#define ZEPHYR_INCLUDE_BLUETOOTH_SERVICES_VCS_H_

/**
 * @brief Volume Control Service (VCS)
 *
 * @defgroup bt_gatt_vcs Volume Control Service (VCS)
 *
 * @ingroup bluetooth
 * @{
 *
 * [Experimental] Users should note that the APIs can change
 * as a part of ongoing development.
 */

#include <zephyr/types.h>
#include <acts_bluetooth/audio/aics.h>
#include <acts_bluetooth/audio/vocs.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(CONFIG_BT_VCS)
#define BT_VCS_VOCS_CNT CONFIG_BT_VCS_VOCS_INSTANCE_COUNT
#define BT_VCS_AICS_CNT CONFIG_BT_VCS_AICS_INSTANCE_COUNT
#else
#define BT_VCS_VOCS_CNT 0
#define BT_VCS_AICS_CNT 0
#endif /* CONFIG_BT_VCS */

/** Volume Control Service Error codes */
#define BT_VCS_ERR_INVALID_COUNTER             0x80
#define BT_VCS_ERR_OP_NOT_SUPPORTED            0x81

/** Volume Control Service Mute Values */
#define BT_VCS_STATE_UNMUTED                   0x00
#define BT_VCS_STATE_MUTED                     0x01

struct bt_vcs;
struct bt_vcs_server;
struct bt_vcs_client;

/** Register structure for Volume Control Service */
struct bt_vcs_register_param {
	/** Initial volume level (0-255) */
	uint8_t volume;

	/** Initial mute state (0-1) */
	uint8_t mute;

	/** Initial step size (1-255) */
	uint8_t step;

	/* vcs volume flags */
	uint8_t flags;

	/** Register parameters for Volume Offset Control Services */
	struct bt_vocs_register_param vocs_param[BT_VCS_VOCS_CNT];

	/** Register parameters  for Audio Input Control Services */
	struct bt_aics_register_param aics_param[BT_VCS_AICS_CNT];

	/** Volume Control Service callback structure. */
	struct bt_vcs_cb *cb;
};

/**
 * @brief Volume Control Service service instance
 *
 * Used for to represent a Volume Control Service instance, for either a client
 * or a server. The instance pointers either represent local server instances,
 * or remote service instances.
 */
#if 0
struct bt_vcs {
	/** Number of Volume Offset Control Service instances */
	uint8_t vocs_cnt;
	/** Array of pointers to Volume Offset Control Service instances */
	struct bt_vocs **vocs;

	/** Number of Audio Input Control Service instances */
	uint8_t aics_cnt;
	/** Array of pointers to Audio Input Control Service instances */
	struct bt_aics **aics;
};
#endif

/**
 * @brief Register the Volume Control Service.
 *
 * This will register and enable the service and make it discoverable by
 * clients.
 *
 * @param param     Volume Control Service register parameters.
 *
 * @return 0 if success, errno on failure.
 */
int bt_vcs_register(struct bt_vcs_register_param *param, struct bt_vcs *inst);

/**
 * @brief Callback function for bt_vcs_discover.
 *
 * This callback is only used for the client.
 *
 * @param conn         The connection that was used to discover
 *                     Volume Control Service.
 * @param err          Error value. 0 on success, GATT error on positive value
 *                     or errno on negative value.
 * @param vocs_count   Number of Volume Offset Control Service instances
 *                     on peer device.
 * @param aics_count   Number of Audio Input Control Service instances on
 *                     peer device.
 */
typedef void (*bt_vcs_discover_cb)(struct bt_conn *conn, int err,
				   uint8_t vocs_count, uint8_t aics_count);

/**
 * @brief Callback function for Volume Control Service volume state.
 *
 * Called when the value is locally read as the server.
 * Called when the value is remotely read as the client.
 * Called if the value is changed by either the server or client.
 *
 * @param conn    NULL if local server read or write, otherwise the connection
 *                to the peer device if remotely read or written.
 * @param err     Error value. 0 on success, GATT error on positive value
 *                or errno on negative value.
 * @param volume  The volume of the Volume Control Service server.
 * @param mute    The mute setting of the Volume Control Service server.
 */
typedef void (*bt_vcs_state_cb)(struct bt_conn *conn, struct bt_vcs *vcs,
				int err);

typedef void (*bt_vcs_control_cb)(struct bt_conn *conn, struct bt_vcs *vcs,
				int err, uint8_t opcode);

/**
 * @brief Callback function for Volume Control Service flags.
 *
 * Called when the value is locally read as the server.
 * Called when the value is remotely read as the client.
 * Called if the value is changed by either the server or client.
 *
 * @param conn    NULL if local server read or write, otherwise the connection
 *                to the peer device if remotely read or written.
 * @param err     Error value. 0 on success, GATT error on positive value
 *                or errno on negative value.
 * @param flags   The flags of the Volume Control Service server.
 */
typedef void (*bt_vcs_flags_cb)(struct bt_vcs *vcs, int err, uint8_t flags);

/**
 * @brief Callback function for writes.
 *
 * This callback is only used for the client.
 *
 * @param conn    NULL if local server read or write, otherwise the connection
 *                to the peer device if remotely read or written.
 * @param err     Error value. 0 on success, GATT error on fail.
 */
typedef void (*bt_vcs_write_cb)(struct bt_conn *conn, int err);

typedef struct bt_vcs_client *(*bt_vcs_client_lookup_cb)(struct bt_conn *conn);

struct bt_vcs_cb {
	/* Volume Control Service */
	bt_vcs_state_cb               state;
	bt_vcs_control_cb			  control;
	bt_vcs_flags_cb               flags;

	bt_vcs_client_lookup_cb		  lookup;
	bt_vcs_discover_cb            discover;
	bt_vcs_write_cb               vol_down;
	bt_vcs_write_cb               vol_up;
	bt_vcs_write_cb               mute;
	bt_vcs_write_cb               unmute;
	bt_vcs_write_cb               vol_down_unmute;
	bt_vcs_write_cb               vol_up_unmute;
	bt_vcs_write_cb               vol_set;

	/* Volume Offset Control Service */
	struct bt_vocs_cb             vocs_cb;

	/* Audio Input Control Service */
	struct bt_aics_cb             aics_cb;
};

typedef struct bt_vcs_client *(*bt_vcs_lookup_cb)(struct bt_conn *conn);

/**
 * @brief Discover Volume Control Service and included services.
 *
 * This will start a GATT discovery and setup handles and subscriptions.
 * This shall be called once before any other actions can be
 * executed for the peer device.
 *
 * This shall only be done as the client,
 *
 * @param conn    The connection to discover Volume Control Service for.
 *
 * @return 0 if success, errno on failure.
 */
int bt_vcs_discover(struct bt_conn *conn, struct bt_vcs_client *cli);

/**
 * @brief Set the Volume Control Service volume step size.
 *
 * Set the value that the volume changes, when changed relatively with e.g.
 * @ref bt_vcs_vol_down or @ref bt_vcs_vol_up.
 *
 * This can only be done as the server.
 *
 * @param volume_step  The volume step size (1-255).
 *
 * @return 0 if success, errno on failure.
 */
int bt_vcs_vol_step_set(uint8_t volume_step);

/**
 * @brief Read the Volume Control Service volume state.
 *
 * @param conn   Connection to the peer device,
 *               or NULL to read local server value.
 *
 * @return 0 if success, errno on failure.
 */
int bt_vcs_vol_get(struct bt_conn *conn);

/**
 * @brief Read the Volume Control Service flags.
 *
 * @param conn   Connection to peer device, or NULL to read local server value.
 *
 * @return 0 if success, errno on failure.
 */
int bt_vcs_flags_get(struct bt_conn *conn);

/**
 * @brief Turn the volume down by one step on the server.
 *
 * @param conn   Connection to peer device, or NULL to read local server value.
 *
 * @return 0 if success, errno on failure.
 */
int bt_vcs_vol_down(void);

/**
 * @brief Turn the volume up by one step on the server.
 *
 * @param conn   Connection to peer device, or NULL to read local server value.
 *
 * @return 0 if success, errno on failure.
 */
int bt_vcs_vol_up(void);

/**
 * @brief Turn the volume down and unmute the server.
 *
 * @param conn   Connection to peer device, or NULL to read local server value.
 *
 * @return 0 if success, errno on failure.
 */
int bt_vcs_unmute_vol_down(void);

/**
 * @brief Turn the volume up and unmute the server.
 *
 * @param conn   Connection to peer device, or NULL to read local server value.
 *
 * @return 0 if success, errno on failure.
 */
int bt_vcs_unmute_vol_up(void);

/**
 * @brief Set the volume on the server
 *
 * @param conn   Connection to peer device, or NULL to set local server value.
 * @param volume The absolute volume to set.
 *
 * @return 0 if success, errno on failure.
 */
int bt_vcs_vol_set(struct bt_conn *conn, uint8_t volume);

/**
 * @brief Unmute the server.
 *
 * @param conn   Connection to peer device, or NULL to read local server value.
 *
 * @return 0 if success, errno on failure.
 */
int bt_vcs_unmute(void);

/**
 * @brief Mute the server.
 *
 * @param conn   Connection to peer device, or NULL to read local server value.
 *
 * @return 0 if success, errno on failure.
 */
int bt_vcs_mute(void);

/**
 * @brief Read the Volume Offset Control Service offset state.
 *
 * @param conn   Connection to peer device, or NULL to read local server value.
 * @param inst   Pointer to the Volume Offset Control Service instance.
 *
 * @return 0 if success, errno on failure.
 */
int bt_vcs_vocs_state_get(struct bt_conn *conn, struct bt_vocs *inst);

/**
 * @brief Read the Volume Offset Control Service location.
 *
 * @param conn   Connection to peer device, or NULL to read local server value.
 * @param inst   Pointer to the Volume Offset Control Service instance.
 *
 * @return 0 if success, errno on failure.
 */
int bt_vcs_vocs_location_get(struct bt_conn *conn, struct bt_vocs *inst);

/**
 * @brief Set the Volume Offset Control Service location.
 *
 * @param conn       Connection to peer device, or NULL to set local server
 *                   value.
 * @param inst       Pointer to the Volume Offset Control Service instance.
 * @param location   The location to set.
 *
 * @return 0 if success, errno on failure.
 */
int bt_vcs_vocs_location_set(struct bt_conn *conn, struct bt_vocs *inst,
			     uint8_t location);

/**
 * @brief Set the Volume Offset Control Service offset state.
 *
 * @param conn    Connection to peer device, or NULL to set local server value.
 * @param inst    Pointer to the Volume Offset Control Service instance.
 * @param offset  The offset to set (-255 to 255).
 *
 * @return 0 if success, errno on failure.
 */
int bt_vcs_vocs_state_set(struct bt_conn *conn, struct bt_vocs *inst,
			  int16_t offset);

/**
 * @brief Read the Volume Offset Control Service output description.
 *
 * @param conn   Connection to peer device, or NULL to read local server value.
 * @param inst   Pointer to the Volume Offset Control Service instance.
 *
 * @return 0 if success, errno on failure.
 */
int bt_vcs_vocs_description_get(struct bt_conn *conn, struct bt_vocs *inst);

/**
 * @brief Set the Volume Offset Control Service description.
 *
 * @param conn          Connection to peer device, or NULL to set local server
 *                      value.
 * @param inst          Pointer to the Volume Offset Control Service instance.
 * @param description   The description to set.
 *
 * @return 0 if success, errno on failure.
 */
int bt_vcs_vocs_description_set(struct bt_conn *conn, struct bt_vocs *inst,
				const char *description);

/**
 * @brief Deactivates an Audio Input Control Service instance.
 *
 * Audio Input Control Services are activated by default, but this will allow
 * the server to deactivate an Audio Input Control Service.
 *
 * @param inst   Pointer to the Audio Input Control Service instance.
 *
 * @return 0 if success, errno on failure.
 */
int bt_vcs_aics_deactivate(struct bt_aics *inst);

/**
 * @brief Activates an Audio Input Control Service instance.
 *
 * Audio Input Control Services are activated by default, but this will allow
 * the server to reactivate an Audio Input Control Service instance after it has
 * been deactivated with @ref bt_vcs_aics_deactivate.
 *
 * @param inst   Pointer to the Audio Input Control Service instance.
 *
 * @return 0 if success, errno on failure.
 */
int bt_vcs_aics_activate(struct bt_aics *inst);

/**
 * @brief Read the Audio Input Control Service input state.
 *
 * @param conn   Connection to peer device, or NULL to read local server value.
 * @param inst   Pointer to the Audio Input Control Service instance.
 *
 * @return 0 if success, errno on failure.
 */
int bt_vcs_aics_state_get(struct bt_conn *conn, struct bt_aics *inst);

/**
 * @brief Read the Audio Input Control Service gain settings.
 *
 * @param conn   Connection to peer device, or NULL to read local server value.
 * @param inst   Pointer to the Audio Input Control Service instance.
 *
 * @return 0 if success, errno on failure.
 */
int bt_vcs_aics_gain_setting_get(struct bt_conn *conn, struct bt_aics *inst);

/**
 * @brief Read the Audio Input Control Service input type.
 *
 * @param conn   Connection to peer device, or NULL to read local server value.
 * @param inst   Pointer to the Audio Input Control Service instance.
 *
 * @return 0 if success, errno on failure.
 */
int bt_vcs_aics_type_get(struct bt_conn *conn, struct bt_aics *inst);

/**
 * @brief Read the Audio Input Control Service input status.
 *
 * @param conn   Connection to peer device, or NULL to read local server value.
 * @param inst   Pointer to the Audio Input Control Service instance.
 *
 * @return 0 if success, errno on failure.
 */
int bt_vcs_aics_status_get(struct bt_conn *conn, struct bt_aics *inst);

/**
 * @brief Mute the Audio Input Control Service input.
 *
 * @param conn   Connection to peer device, or NULL to set local server value.
 * @param inst   Pointer to the Audio Input Control Service instance.
 *
 * @return 0 if success, errno on failure.
 */
int bt_vcs_aics_mute(struct bt_conn *conn, struct bt_aics *inst);

/**
 * @brief Unmute the Audio Input Control Service input.
 *
 * @param conn   Connection to peer device, or NULL to set local server value.
 * @param inst   Pointer to the Audio Input Control Service instance.
 *
 * @return 0 if success, errno on failure.
 */
int bt_vcs_aics_unmute(struct bt_conn *conn, struct bt_aics *inst);

/**
 * @brief Set input gain to manual.
 *
 * @param conn   Connection to peer device, or NULL to set local server value.
 * @param inst   Pointer to the Audio Input Control Service instance.
 *
 * @return 0 if success, errno on failure.
 */
int bt_vcs_aics_manual_gain_set(struct bt_conn *conn, struct bt_aics *inst);

/**
 * @brief Set the input gain to automatic.
 *
 * @param conn   Connection to peer device, or NULL to set local server value.
 * @param inst   Pointer to the Audio Input Control Service instance.
 *
 * @return 0 if success, errno on failure.
 */
int bt_vcs_aics_automatic_gain_set(struct bt_conn *conn, struct bt_aics *inst);

/**
 * @brief Set the input gain.
 *
 * @param conn   Connection to peer device, or NULL to set local server value.
 * @param inst   Pointer to the Audio Input Control Service instance.
 * @param gain   The gain in dB to set (-128 to 127).
 *
 * @return 0 if success, errno on failure.
 */
int bt_vcs_aics_gain_set(struct bt_conn *conn, struct bt_aics *inst,
			 int8_t gain);

/**
 * @brief Read the Audio Input Control Service description.
 *
 * @param conn   Connection to peer device, or NULL to read local server value.
 * @param inst   Pointer to the Audio Input Control Service instance.
 *
 * @return 0 if success, errno on failure.
 */
int bt_vcs_aics_description_get(struct bt_conn *conn, struct bt_aics *inst);

/**
 * @brief Set the Audio Input Control Service description.
 *
 * @param conn          Connection to peer device, or NULL to set local server
 *                      value.
 * @param inst          Pointer to the Audio Input Control Service instance.
 * @param description   The description to set.
 *
 * @return 0 if success, errno on failure.
 */
int bt_vcs_aics_description_set(struct bt_conn *conn, struct bt_aics *inst,
				const char *description);

/**
 * @brief Registers the callbacks used by the Volume Control Service client.
 *
 * @param cb   The callback structure.
 *
 * @return 0 if success, errno on failure.
 */
int bt_vcs_client_cb_register(struct bt_vcs_cb *cb);

ssize_t bt_vcs_read_vol_state(struct bt_conn *conn,
			      const struct bt_gatt_attr *attr, void *buf,
			      uint16_t len, uint16_t offset);

ssize_t bt_vcs_write_control(struct bt_conn *conn,
				 const struct bt_gatt_attr *attr,
				 const void *buf, uint16_t len, uint16_t offset,
				 uint8_t flags);

ssize_t bt_vcs_read_flags(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			  void *buf, uint16_t len, uint16_t offset);

/* VCS opcodes */
#define BT_VCS_OPCODE_REL_VOL_DOWN                      0x00
#define BT_VCS_OPCODE_REL_VOL_UP                        0x01
#define BT_VCS_OPCODE_UNMUTE_REL_VOL_DOWN               0x02
#define BT_VCS_OPCODE_UNMUTE_REL_VOL_UP                 0x03
#define BT_VCS_OPCODE_SET_ABS_VOL                       0x04
#define BT_VCS_OPCODE_UNMUTE                            0x05
#define BT_VCS_OPCODE_MUTE                              0x06

struct vcs_state {
	uint8_t volume;
	uint8_t mute;
	uint8_t change_counter;
} __packed;

struct vcs_control {
	uint8_t opcode;
	uint8_t counter;
} __packed;

struct vcs_control_vol {
	struct vcs_control cp;
	uint8_t volume;
} __packed;

struct bt_vcs_server {
	struct vcs_state state;
	uint8_t flags;
	struct bt_vcs_cb *cb;
	uint8_t volume_step;

	struct bt_gatt_service *service_p;

	/* number of aics instances */
	uint8_t aics_cnt;
	/* list of bt aics server */
	sys_slist_t aics_list;
	/* number of vocs instances */
	uint8_t vocs_cnt;
	/* list of bt vocs server */
	sys_slist_t vocs_list;
};

struct bt_vcs_client {
	sys_snode_t node;
	struct vcs_state state;
	uint8_t flags;

	uint16_t start_handle;
	uint16_t end_handle;
	uint16_t state_handle;
	uint16_t control_handle;
	uint16_t flag_handle;
	struct bt_gatt_subscribe_params state_sub_params;
	struct bt_gatt_subscribe_params flag_sub_params;
	bool cp_retried;

	bool busy;
	bool write_control;
	uint16_t pending_vol;	/* pending volume when client is busy */
	struct vcs_control_vol cp_val;
	struct bt_gatt_write_params write_params;
	struct bt_gatt_read_params read_params;
	struct bt_gatt_discover_params discover_params;
	struct bt_uuid_16 uuid;

	uint8_t vocs_inst_cnt;
	/* list of bt vocs client */
	sys_slist_t vocs_list;
	uint8_t aics_inst_cnt;
	/* list of bt aics client */
	sys_slist_t aics_list;
};

struct bt_vcs {
	union {
		struct bt_vcs_client cli;
		struct bt_vcs_server srv;
	};
};

int bt_vcs_client_get(struct bt_conn *conn, struct bt_vcs *client);
int bt_vcs_client_read_vol_state(struct bt_conn *conn, struct bt_vcs_client *cli);
int bt_vcs_client_read_flags(struct bt_conn *conn, struct bt_vcs_client *cli);
int bt_vcs_client_vol_down(struct bt_conn *conn, struct bt_vcs_client *cli);
int bt_vcs_client_vol_up(struct bt_conn *conn, struct bt_vcs_client *cli);
int bt_vcs_client_unmute_vol_down(struct bt_conn *conn, struct bt_vcs_client *cli);
int bt_vcs_client_unmute_vol_up(struct bt_conn *conn, struct bt_vcs_client *cli);
int bt_vcs_client_set_volume(struct bt_conn *conn, struct bt_vcs_client *cli, uint8_t volume);
int bt_vcs_client_unmute(struct bt_conn *conn, struct bt_vcs_client *cli);
int bt_vcs_client_mute(struct bt_conn *conn, struct bt_vcs_client *cli);

bool bt_vcs_client_valid_vocs_inst(struct bt_conn *conn, struct bt_vocs *vocs);
bool bt_vcs_client_valid_aics_inst(struct bt_conn *conn, struct bt_aics *aics);

/* For pts test */
void bt_pts_vcs_set_state(void *state, uint8_t vol_step);
int bt_pts_vcs_notify_state(struct bt_conn *conn);
int bt_pts_vcs_notify_flags(struct bt_conn *conn);
typedef enum
{
	VCS_STATE,
	VCS_FLAGS,
} vcs_notify_type_e;
typedef void (*vcs_notify_callback)(struct bt_conn *conn, uint8_t notify_type);
extern vcs_notify_callback g_vcs_notify_cb;

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ZEPHYR_INCLUDE_BLUETOOTH_SERVICES_VCS_H_ */
