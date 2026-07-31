/** @file
 *  @brief Broadcast Audio Scan Service
 */

/*
 * Copyright (c) 2021 Actions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_BASS_H_
#define ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_BASS_H_

#include <misc/util.h>
#include <acts_bluetooth/addr.h>

#ifdef __cplusplus
extern "C" {
#endif

#if IS_ENABLED(CONFIG_BT_BASS)
#define BT_BASS_MAX_METADATA_LEN CONFIG_BT_BASS_MAX_METADATA_LEN
#define BT_BASS_MAX_SUBGROUPS    CONFIG_BT_BASS_MAX_SUBGROUPS
#define BT_BASS_BUF_SIZE         CONFIG_BT_BASS_BUF_SIZE
#else
#define BT_BASS_MAX_METADATA_LEN 0
#define BT_BASS_MAX_SUBGROUPS    0
#define BT_BASS_BUF_SIZE         0
#endif

#if IS_ENABLED(CONFIG_BT_BASS_CLIENT)
#define BT_BASS_CLIENT_BUF_SIZE  CONFIG_BT_BASS_CLIENT_BUF_SIZE
#else
#define BT_BASS_CLIENT_BUF_SIZE  0
#endif

#define BT_BASS_BROADCAST_CODE_SIZE        16

#define BT_BASS_PA_STATE_NOT_SYNCED        0x00
#define BT_BASS_PA_STATE_INFO_REQ          0x01
#define BT_BASS_PA_STATE_SYNCED            0x02
#define BT_BASS_PA_STATE_FAILED            0x03
#define BT_BASS_PA_STATE_NO_PAST           0x04

#define BT_BASS_BIG_ENC_STATE_NO_ENC       0x00
#define BT_BASS_BIG_ENC_STATE_BCODE_REQ    0x01
#define BT_BASS_BIG_ENC_STATE_DEC          0x02
#define BT_BASS_BIG_ENC_STATE_BAD_CODE     0x03

#define BT_BASS_ERR_OPCODE_NOT_SUPPORTED   0x80
#define BT_BASS_ERR_INVALID_SRC_ID         0x81

#define BT_BASS_PA_INTERVAL_UNKNOWN        0xFFFF

#define BT_BASS_BROADCAST_MAX_ID           0xFFFFFF

#define BT_BASS_BIS_SYNC_NO_PREF           0xFFFFFFFF

struct bt_bass_subgroup {
	uint32_t bis_sync;
	uint32_t requested_bis_sync;
	uint8_t metadata_len;
	uint8_t metadata[BT_BASS_MAX_METADATA_LEN];
};

/* TODO: Only expose this as an opaque type */
struct bt_bass_recv_state {
	uint8_t src_id;
	bt_addr_le_t addr;
	uint8_t adv_sid;
	uint8_t req_pa_sync_value;
	uint8_t pa_sync_state;
	uint8_t encrypt_state;
	uint32_t broadcast_id; /* 24 bits */
	uint8_t bad_code[BT_BASS_BROADCAST_CODE_SIZE];
	uint8_t num_subgroups;
	struct bt_bass_subgroup subgroups[BT_BASS_MAX_SUBGROUPS];
};

// TODO: -> struct bt_bass_server_recv_state
struct bass_recv_state_internal {
	const struct bt_gatt_attr *attr;

	bool active;
	uint8_t index;
	struct bt_bass_recv_state state;
	uint8_t broadcast_code[BT_BASS_BROADCAST_CODE_SIZE];
	uint16_t pa_interval;
	bool broadcast_code_received;
	struct bt_le_per_adv_sync *pa_sync;
	bool pa_sync_pending;
	struct k_delayed_work pa_timer;
#if 0
	struct k_work_sync work_sync;
#endif
	uint8_t biginfo_num_bis;
	bool biginfo_received;
	bool big_encrypted;
	uint16_t iso_interval;
	struct bt_iso_big *big;	// TODO: need?
};

// TODO:  review cb
// TODO: add cb for add/modify/remove_source/set_code
struct bt_bass_server_cb {
	void (*pa_synced)(struct bt_bass_recv_state *recv_state,
			  const struct bt_le_per_adv_sync_synced_info *info);
	void (*pa_term)(struct bt_bass_recv_state *recv_state,
			const struct bt_le_per_adv_sync_term_info *info);
	void (*pa_recv)(struct bt_bass_recv_state *recv_state,
			const struct bt_le_per_adv_sync_recv_info *info,
			struct net_buf_simple *buf);
	void (*biginfo)(struct bt_bass_recv_state *recv_state,
			const struct bt_iso_biginfo *biginfo);
};

struct bt_bass_server {
	uint8_t next_src_id;

	/*
	 * the valid number of recv states.
	 *
	 * num_of_state = min(the number of BT_UUID_BASS_RECV_STATE Chrc,
	 *                    the number of recv_states)
	 */
	uint8_t num_of_state;

	// TODO: if support add/modify/remove souce
	// TODO: used to distinguish if support broadcast_sink???
	uint8_t support_broadcast_sink : 1;

	struct bass_recv_state_internal recv_states
		[CONFIG_BT_BASS_RECV_STATE_COUNT];

	// TODO: move to bt_service???
	struct bt_le_per_adv_sync_cb pa_sync_cb;

	struct net_buf_simple read_buf;
	uint8_t data[BT_BASS_BUF_SIZE];
};

/**
 * @brief Registers the callbacks used by BASS server.
 */
void bt_bass_server_cb_register(struct bt_bass_server_cb *cb);

/**
 * @brief Set the sync state of a receive state in the server
 *
 * @param src_id         The source id used to identify the receive state.
 * @param pa_sync_state  The sync state of the PA.
 * @param bis_synced     Array of bitfields to set the BIS sync state for each
 *                       subgroup.
 * @param encrypted      The BIG encryption state.
 * @return int           Error value. 0 on success, ERRNO on fail.
 */
int bt_bass_set_sync_state(uint8_t src_id, uint8_t pa_sync_state,
			   uint32_t bis_synced[BT_BASS_MAX_SUBGROUPS],
			   uint8_t encrypted);

int bt_bass_remove_source(uint8_t src_id);

ssize_t bt_bass_write_scan_control(struct bt_conn *conn,
				const struct bt_gatt_attr *attr,
				const void *data, uint16_t len,
				uint16_t offset, uint8_t flags);

ssize_t bt_bass_read_receive_state(struct bt_conn *conn,
				const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset);

int bt_bass_server_init(struct bt_bass_server *srv);

int bt_bass_server_exit(void);









struct bt_bass_client_recv_state {
	uint8_t broadcast_code[BT_BASS_BROADCAST_CODE_SIZE];
	uint8_t src_id;	// TODO: seems no need?

	uint8_t broadcast_code_valid : 1;
	uint8_t src_id_valid : 1;

	uint16_t value_handle;
	struct bt_gatt_subscribe_params sub_params;

	struct bt_bass_recv_state state;
};

struct bt_bass_client {
	uint8_t busy : 1;
	uint8_t reading : 1;

	uint8_t pa_sync;
	uint8_t recv_state_cnt;	// TODO: ->num_of_state
	// TODO: if server's recv_state > CONFIG_BT_BASS_CLIENT_RECV_STATE_COUNT

	// TODO: need?
	/* Source ID cache so that we can notify application about
	 * which source ID was removed
	 */
	uint8_t src_ids[CONFIG_BT_BASS_CLIENT_RECV_STATE_COUNT];

	uint16_t start_handle;
	uint16_t end_handle;
	uint16_t cp_handle;

#if 1
	uint16_t recv_state_handles[CONFIG_BT_BASS_CLIENT_RECV_STATE_COUNT];

	struct bt_gatt_subscribe_params recv_state_sub_params
		[CONFIG_BT_BASS_CLIENT_RECV_STATE_COUNT];
#endif

#if 0	// TODO: for saving ram
	struct bt_gatt_discover_params recv_state_disc_params
		[CONFIG_BT_BASS_CLIENT_RECV_STATE_COUNT];
#endif
	struct bt_gatt_read_params read_params;
#if 1	// TODO: disable for saving ram??
	struct bt_gatt_write_params write_params;
#endif
	struct bt_gatt_discover_params discover_params;

	struct bt_bass_client_recv_state recv_states
		[CONFIG_BT_BASS_CLIENT_RECV_STATE_COUNT];

	struct bt_uuid_16 uuid;

	struct net_buf_simple cp_buf;
	uint8_t data[BT_BASS_CLIENT_BUF_SIZE];
};

// TODO: compare with zephyr
#if 1	////////////////////////////////////////////////////////////////
typedef void (*bt_bass_client_discover_cb)(struct bt_conn *conn,
				struct bt_bass_client *cli, int err);

#if 0
typedef void (*bt_bass_client_scan_cb)(const struct bt_le_scan_recv_info *info,
				uint32_t broadcast_id);
#endif

// TODO: add struct bt_bass_client *cli ?
typedef void (*bt_bass_client_recv_state_cb)(struct bt_conn *conn, int err,
				const struct bt_bass_client_recv_state *state);

// TODO: replace src_id with struct bt_bass_client_recv_state??
typedef void (*bt_bass_client_recv_state_rem_cb)(struct bt_conn *conn, int err,
				uint8_t src_id);

typedef void (*bt_bass_client_write_cb)(struct bt_conn *conn,
				struct bt_bass_client *cli, int err);
#endif	////////////////////////////////////////////////////////////////

struct bt_bass_client_cb {
	bt_bass_client_discover_cb        discover;
#if 0	// TODO: need? bt_service will take care of scan???
	bt_bass_client_scan_cb            scan;
#endif
	bt_bass_client_recv_state_cb      recv_state;
	bt_bass_client_recv_state_rem_cb  recv_state_removed;

	bt_bass_client_write_cb           scan_start;
	bt_bass_client_write_cb           scan_stop;
	bt_bass_client_write_cb           add_src;
	bt_bass_client_write_cb           mod_src;
	bt_bass_client_write_cb           broadcast_code;
	bt_bass_client_write_cb           rem_src;
};

int bt_bass_client_discover(struct bt_conn *conn, struct bt_bass_client *cli);

/**
 * @brief Registers the callbacks used by Broadcast Audio Scan Service client,
 *        NULL for unregister.
 */
void bt_bass_client_cb_register(struct bt_bass_client_cb *cb);

int bt_bass_client_scan_start(struct bt_conn *conn, struct bt_bass_client *cli);

int bt_bass_client_scan_stop(struct bt_conn *conn, struct bt_bass_client *cli);

// TODO: merge to bt_bass_client???
// TODO: almost same as bt_bass_recv_state???
/** Parameters for adding a source to a Broadcast Audio Scan Service server */
struct bt_bass_add_src_param {
	/** Address of the advertiser. */
	bt_addr_le_t addr;
	/** SID of the advertising set. */
	uint8_t adv_sid;
	/** Whether to sync to periodic advertisements. */
	uint8_t pa_sync;
	/** 24-bit broadcast ID */
	uint32_t broadcast_id;
	/**
	 * @brief Periodic advertising interval in milliseconds.
	 *
	 * BT_BASS_PA_INTERVAL_UNKNOWN if unknown.
	 */
	uint16_t pa_interval;
	/** Number of subgroups */
	uint8_t num_subgroups;
	/** Pointer to array of subgroups */
	struct bt_bass_subgroup *subgroups;
};

int bt_bass_client_add_src(struct bt_conn *conn, struct bt_bass_client *cli,
			   struct bt_bass_add_src_param *param);

/** Parameters for modifying a source */
struct bt_bass_mod_src_param {
	/** Source ID of the receive state. */
	uint8_t src_id;
	/** Whether to sync to periodic advertisements. */
	uint8_t pa_sync;
	/**
	 * @brief Periodic advertising interval.
	 *
	 * BT_BASS_PA_INTERVAL_UNKNOWN if unknown.
	 */
	uint16_t pa_interval;
	/** Number of subgroups */
	uint8_t num_subgroups;
	/** Pointer to array of subgroups */
	struct bt_bass_subgroup *subgroups;
};

int bt_bass_client_mod_src(struct bt_conn *conn, struct bt_bass_client *cli,
				struct bt_bass_mod_src_param *param);

int bt_bass_client_set_broadcast_code(struct bt_conn *conn, 
				struct bt_bass_client *cli,
				struct bt_bass_client_recv_state *state,	// TODO: using src_id???
				uint8_t broadcast_code[BT_BASS_BROADCAST_CODE_SIZE]);	// TODO:

int bt_bass_client_rem_src(struct bt_conn *conn, struct bt_bass_client *cli,
				uint8_t src_id);

int bt_bass_client_read_recv_state(struct bt_conn *conn,
				struct bt_bass_client *cli, uint8_t index);

void bt_bass_server_set_conn(struct bt_conn *conn);
void bt_bass_pts_init_recv_state(void);
void bt_bass_pts_reset_recv_state(void);


#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_BASS_H_ */
