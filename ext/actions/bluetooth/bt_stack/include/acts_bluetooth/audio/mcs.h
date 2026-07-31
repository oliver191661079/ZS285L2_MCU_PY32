/*
 * Copyright (c) 2021 Actions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_BLUETOOTH_SERVICES_MCS_H_
#define ZEPHYR_INCLUDE_BLUETOOTH_SERVICES_MCS_H_

/**
 * @brief Media Control Service (MCS)
 *
 * @defgroup bt_gatt_mcs Media Control Service (MCS)
 *
 * @ingroup bluetooth
 */

#include <toolchain.h>
#include <zephyr/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Media Control Service Error codes */
#define BT_MCS_ERR_VAL_CHANGED_DURING_READ_LONG 0x80

struct bt_mcs_server;
struct bt_mcs_client;
struct bt_mcs;

/** @brief Structure for registering a Media Control Service instance. */
struct bt_mcs_register_param {
	char *media_player_name;
	char *track_title;
	int32_t track_duration;
	int32_t track_position;
    int8_t playback_speed;
    int8_t seeking_speed;
    uint8_t media_state;
    uint32_t opcodes_supported;
    uint8_t ccid;
	/** Pointer to the callback structure. */
	struct bt_mcs_server_cb *cb;
};

/**
 * @brief Register the Media Control Service instance.
 *
 * @param mcs 		Meida Control Service instance.
 * @param param     Media Control Service register parameters.
 *
 * @return 0 if success, errno on failure.
 */
int bt_mcs_register(struct bt_mcs_server *srv,
		     const struct bt_mcs_register_param *param);

typedef void (*bt_mcs_client_common_cb)(struct bt_conn *conn, struct bt_mcs_client *cli, int err);

typedef void (*bt_mcs_client_discover_cb)(struct bt_conn *conn, struct bt_mcs_client *cli, int err);

typedef void (*bt_mcs_client_write_cb)(struct bt_conn *conn, struct bt_mcs_client *cli, int err);

struct bt_mcs_client_cb {
    bt_mcs_client_discover_cb discover;

	bt_mcs_client_common_cb media_player_name;
	bt_mcs_client_common_cb track_changed;
	bt_mcs_client_common_cb track_title;
    bt_mcs_client_common_cb track_duration;
    bt_mcs_client_common_cb track_position;
    bt_mcs_client_common_cb playback_speed;
    bt_mcs_client_common_cb seeking_speed;
    bt_mcs_client_common_cb media_state;
    bt_mcs_client_common_cb opcodes_supported;
    bt_mcs_client_common_cb media_control;
    bt_mcs_client_common_cb ccid;

    bt_mcs_client_write_cb set_track_position;
    bt_mcs_client_write_cb set_playback_speed;
	bt_mcs_client_write_cb media_play_pause_with_rsp;
};

typedef struct bt_mcs_server *(*bt_mcs_lookup_cb)(struct bt_conn *conn, uint16_t handle);

typedef void (*bt_mcs_server_common_cb)(struct bt_conn *conn, struct bt_mcs_server *srv, int err);

typedef void (*bt_mcs_server_control_cb)(struct bt_conn *conn, struct bt_mcs_server *srv, int err, uint8_t opcode);

struct bt_mcs_server_cb {
    bt_mcs_lookup_cb lookup;
    bt_mcs_server_common_cb track_position;
    bt_mcs_server_common_cb media_state;
    bt_mcs_server_control_cb media_control;
};

void bt_mcs_server_cb_register(struct bt_mcs_server_cb *cb);

int bt_mcs_media_player_name_get(struct bt_mcs_server *srv);
int bt_mcs_media_player_name_set(struct bt_mcs_server *srv, char *name);

int bt_mcs_track_changed(struct bt_mcs_server *srv);

int bt_mcs_track_title_get(struct bt_mcs_server *srv);
int bt_mcs_track_title_set(struct bt_mcs_server *srv, char *title);

int bt_mcs_track_duration_get(struct bt_mcs_server *srv);
int bt_mcs_track_duration_set(struct bt_mcs_server *srv, int32_t duration);

int bt_mcs_track_position_get(struct bt_mcs_server *srv);
int bt_mcs_track_position_set(struct bt_mcs_server *srv, int32_t position);

int bt_mcs_playback_speed_get(struct bt_mcs_server *srv);
int bt_mcs_playback_speed_set(struct bt_mcs_server *srv, int8_t speed);

int bt_mcs_seeking_speed_get(struct bt_mcs_server *srv);
int bt_mcs_seeking_speed_set(struct bt_mcs_server *srv, int8_t speed);

int bt_mcs_media_state_get(struct bt_mcs_server *srv);
int bt_mcs_media_state_set(struct bt_mcs_server *srv, uint8_t state);

int bt_mcs_client_opcodes_get(struct bt_conn *conn, struct bt_mcs_client *cli);

int bt_mcs_ccid_get(struct bt_mcs_server *srv);

int bt_mcs_play(struct bt_mcs_server *srv);
int bt_mcs_pause(struct bt_mcs_server *srv);
int bt_mcs_fast_rewind(struct bt_mcs_server *srv);
int bt_mcs_fast_forward(struct bt_mcs_server *srv);
int bt_mcs_stop(struct bt_mcs_server *srv);
int bt_mcs_previous_track(struct bt_mcs_server *srv);
int bt_mcs_next_track(struct bt_mcs_server *srv);

ssize_t bt_mcs_read_media_player_name(struct bt_conn *conn,
			       const struct bt_gatt_attr *attr, void *buf,
			       uint16_t len, uint16_t offset);

ssize_t bt_mcs_read_track_title(struct bt_conn *conn,
			       const struct bt_gatt_attr *attr, void *buf,
			       uint16_t len, uint16_t offset);

ssize_t bt_mcs_read_track_duration(struct bt_conn *conn,
			       const struct bt_gatt_attr *attr, void *buf,
			       uint16_t len, uint16_t offset);

ssize_t bt_mcs_read_track_position(struct bt_conn *conn,
			       const struct bt_gatt_attr *attr, void *buf,
			       uint16_t len, uint16_t offset);

ssize_t bt_mcs_write_track_position(struct bt_conn *conn,
				  const struct bt_gatt_attr *attr,
				  const void *buf, uint16_t len,
				  uint16_t offset, uint8_t flags);

ssize_t bt_mcs_read_playback_speed(struct bt_conn *conn,
			       const struct bt_gatt_attr *attr, void *buf,
			       uint16_t len, uint16_t offset);

ssize_t bt_mcs_write_playback_speed(struct bt_conn *conn,
				  const struct bt_gatt_attr *attr,
				  const void *buf, uint16_t len,
				  uint16_t offset, uint8_t flags);

ssize_t bt_mcs_read_seeking_speed(struct bt_conn *conn,
			       const struct bt_gatt_attr *attr, void *buf,
			       uint16_t len, uint16_t offset);

ssize_t bt_mcs_read_media_state(struct bt_conn *conn,
			       const struct bt_gatt_attr *attr, void *buf,
			       uint16_t len, uint16_t offset);

ssize_t bt_mcs_write_media_control(struct bt_conn *conn,
				  const struct bt_gatt_attr *attr,
				  const void *buf, uint16_t len,
				  uint16_t offset, uint8_t flags);

ssize_t bt_mcs_read_opcodes_supported(struct bt_conn *conn,
			       const struct bt_gatt_attr *attr, void *buf,
			       uint16_t len, uint16_t offset);

ssize_t bt_mcs_read_ccid(struct bt_conn *conn,
			       const struct bt_gatt_attr *attr, void *buf,
			       uint16_t len, uint16_t offset);

#if defined(CONFIG_BT_MCS)
#define BT_MCS_MAX_MEDIA_PLAYER_NAME_SIZE CONFIG_BT_MCS_MAX_MEDIA_PLAYER_NAME_SIZE
#define BT_MCS_MAX_TRACK_TITLE_SIZE CONFIG_BT_MCS_MAX_TRACK_TITLE_SIZE
#else
#define BT_MCS_MAX_MEDIA_PLAYER_NAME_SIZE 24
#define BT_MCS_MAX_TRACK_TITLE_SIZE 24
#endif

/* Media State */
#define BT_MCS_MEDIA_STATE_INACTIVE 0x00
#define BT_MCS_MEDIA_STATE_PLAYING  0x01
#define BT_MCS_MEDIA_STATE_PAUSED   0x02
#define BT_MCS_MEDIA_STATE_SEEKING  0x03

/* Media Control Point Opcode */
enum {
	BT_MCS_OPCODE_PLAY = 0x01,
	BT_MCS_OPCODE_PAUSE,
	BT_MCS_OPCODE_FAST_REWIND,
	BT_MCS_OPCODE_FAST_FORWARD,
	BT_MCS_OPCODE_STOP,

	BT_MCS_OPCODE_MOVE_RELATIVE = 0x10,

	BT_MCS_OPCODE_PRE_SEG = 0x20,
	BT_MCS_OPCODE_NEXT_SEG,
	BT_MCS_OPCODE_FIRST_SEG,
	BT_MCS_OPCODE_LAST_SEG,
	BT_MCS_OPCODE_GOTO_SEG,

	BT_MCS_OPCODE_PRE_TRACK = 0x30,
	BT_MCS_OPCODE_NEXT_TRACK,
	BT_MCS_OPCODE_FIRST_TRACK,
	BT_MCS_OPCODE_LAST_TRACK,
	BT_MCS_OPCODE_GOTO_TRACK,

	BT_MCS_OPCODE_PRE_GROUP = 0X40,
	BT_MCS_OPCODE_NEXT_GROUP,
	BT_MCS_OPCODE_FIRST_GROUP,
	BT_MCS_OPCODE_LAST_GROUP,
	BT_MCS_OPCODE_GOTO_GROUP,
};

enum media_opcodes_supported {
    PLAY = 0,
    PAUSE,
    FAST_REWIND,
    FAST_FORWARD,
    STOP,
    MOVE_RELATIVE,
    PREVIOUS_SEGMENT,
    NEXT_SEGMENT,
    FIRST_SEGMENT,
    LAST_SEGMENT,
    GOTO_SEGMENT,
    PREVIOUS_TRACK,
    NEXT_TRACK,
    FIRST_TRACK,
    LAST_TRACK,
    GOTO_TRACK,
    PREVIOUS_GROUP,
	NEXT_GROUP,
	FIRST_GROUP,
	LAST_GROUP,
	GOTO_GROUP,
};

enum {
    BT_MCS_RESULT_SUCCESS = 0x01,
    BT_MCS_RESULT_OPCODE_NOT_SUPPORTED,
    BT_MCS_RESULT_MEDIA_PLAYER_INACTIVE,
    BT_MCS_RESULT_COMMAND_CANNOT_BE_COMPLETED,
};

struct bt_mcs_control {
    uint8_t opcode;
    union {
        /* Only for move relative opcode */
        int32_t offset;
        /* Only for goto segment/track/group opcode */
        int32_t n;
    };
} __packed;

struct bt_mcs_rsp {
    uint8_t opcode;
    uint8_t result;
} __packed;

struct bt_mcs_server {
    char media_player_name[BT_MCS_MAX_MEDIA_PLAYER_NAME_SIZE];
    char track_title[BT_MCS_MAX_TRACK_TITLE_SIZE];
    int32_t track_duration;
    int32_t track_position;
    int8_t playback_speed;
    int8_t seeking_speed;
    uint8_t media_state;
    uint32_t opcodes_supported;
    uint8_t ccid;

    bool initialized;
    struct bt_gatt_service *service_p;
};

struct bt_mcs_client {
    char media_player_name[BT_MCS_MAX_MEDIA_PLAYER_NAME_SIZE];
    char track_title[BT_MCS_MAX_TRACK_TITLE_SIZE];
    int32_t track_duration;
    int32_t track_position;
    int8_t playback_speed;
    int8_t seeking_speed;
    uint8_t media_state;
    uint32_t opcodes_supported;
    uint8_t ccid;

    uint16_t start_handle;
    uint16_t end_handle;
    uint16_t media_player_name_handle;
    uint16_t track_changed_handle;
    uint16_t track_title_handle;
    uint16_t track_duration_handle;
    uint16_t track_position_handle;
    uint16_t playback_speed_handle;
    uint16_t seeking_speed_handle;
    uint16_t media_state_handle;
    uint16_t mcp_handle;
    uint16_t mcp_ops_handle;
    uint16_t ccid_handle;

    bool busy;
    uint8_t opcode;
	bool ctrl_busy;
    struct bt_gatt_subscribe_params mpn_sub_params;
    struct bt_gatt_subscribe_params tc_sub_params;
    struct bt_gatt_subscribe_params tt_sub_params;
    struct bt_gatt_subscribe_params td_sub_params;
    struct bt_gatt_subscribe_params tp_sub_params;
    struct bt_gatt_subscribe_params ms_sub_params;
    struct bt_gatt_subscribe_params mcp_sub_params;
    struct bt_gatt_write_params write_params;
	struct bt_gatt_read_params read_params;
	struct bt_gatt_discover_params discover_params;
	struct bt_uuid_16 uuid;
    uint8_t buf[8];
};

struct bt_mcs {
    union {
        struct bt_mcs_server srv;
        struct bt_mcs_client cli;
    };
};

int bt_mcs_discover(struct bt_conn *conn, struct bt_mcs_client *cli, bool gmcs);
void bt_mcs_client_cb_register(struct bt_mcs_client_cb *cb);
int bt_mcs_client_media_player_name_get(struct bt_conn *conn, struct bt_mcs_client *cli);
int bt_mcs_client_track_title_get(struct bt_conn *conn, struct bt_mcs_client *cli);
int bt_mcs_client_track_duration_get(struct bt_conn *conn, struct bt_mcs_client *cli);
int bt_mcs_client_track_position_get(struct bt_conn *conn, struct bt_mcs_client *cli);
int bt_mcs_client_track_position_set_no_rsp(struct bt_conn *conn, struct bt_mcs_client *cli, int32_t position);
int bt_mcs_client_track_position_set(struct bt_conn *conn, struct bt_mcs_client *cli, int32_t position);
int bt_mcs_client_playback_speed_get(struct bt_conn *conn, struct bt_mcs_client *cli);
int bt_mcs_client_playback_speed_set(struct bt_conn *conn, struct bt_mcs_client *cli, int8_t speed);
int bt_mcs_client_seeking_speed_get(struct bt_conn *conn, struct bt_mcs_client *cli);
int bt_mcs_client_media_state_get(struct bt_conn *conn, struct bt_mcs_client *cli);
int bt_mcs_client_mcp_opcodes_supported_get(struct bt_conn *conn, struct bt_mcs_client *cli);
int bt_mcs_client_ccid_get(struct bt_conn *conn, struct bt_mcs_client *cli);

int bt_mcs_client_play(struct bt_conn *conn, struct bt_mcs_client *cli);
int bt_mcs_client_pause(struct bt_conn *conn, struct bt_mcs_client *cli);
int bt_mcs_client_fast_rewind(struct bt_conn *conn, struct bt_mcs_client *cli);
int bt_mcs_client_fast_forward(struct bt_conn *conn, struct bt_mcs_client *cli);
int bt_mcs_client_stop(struct bt_conn *conn, struct bt_mcs_client *cli);
int bt_mcs_client_previous_track(struct bt_conn *conn, struct bt_mcs_client *cli);
int bt_mcs_client_next_track(struct bt_conn *conn, struct bt_mcs_client *cli);

#ifdef __cplusplus
}
#endif

/**
 * @}
 */

#endif /* ZEPHYR_INCLUDE_BLUETOOTH_SERVICES_VOCS_H_ */
