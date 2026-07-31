/** @file
 *  @brief Published Audio Capabilities Service
 */

/*
 * Copyright (c) 2021 Actions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_PACS_H_
#define ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_PACS_H_

#include <toolchain.h>
#include <zephyr/types.h>
#include <misc/slist.h>

#include <acts_bluetooth/audio/codec.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_BT_PACS_BUF_SIZE
#define BT_PACS_BUF_SIZE	CONFIG_BT_PACS_BUF_SIZE
#else
#define BT_PACS_BUF_SIZE	0
#endif

struct bt_pacs_audio_contexts {
#if 0
	uint16_t sink;
	uint16_t source;
#else
	uint8_t sink[2];
	uint8_t source[2];
#endif
} __packed;

struct bt_pacs_pac {
	sys_snode_t node;	/* for client only */

	uint8_t coding_format;
	uint16_t company_id;
	uint16_t vs_codec_id;

	/*
	 * Codec Specific Capabilities:
	 *
	 * struct bt_supported_sampling_frequencies
	 * struct bt_supported_frame_durations
	 * struct bt_supported_audio_channel_counts
	 * struct bt_supported_octets_per_codec_frame
	 * struct bt_supported_max_codec_frames_per_sdu
	 */
	uint8_t cc_len;
	uint16_t samplings;
	uint8_t durations;
	uint8_t channels;
	uint16_t octets_min;
	uint16_t octets_max;
	uint8_t frames;

	/* Metadata */
	uint8_t meta_len;
	uint16_t audio_contexts;	/* preferred_audio_contexts */
};

/* one for each source or sink PAC characteristic */
struct bt_pacs_server_pac {
	/* characteristic value's attribute */
	const struct bt_gatt_attr *attr;
	sys_snode_t node;

	/* one PAC for each characteristic for simplicity */
	struct bt_pacs_pac pac;
};

struct bt_pacs_server;

typedef struct bt_pacs_server *(*bt_pacs_lookup_cb)(struct bt_conn *conn);

typedef void (*bt_pacs_server_common_cb)(struct bt_conn *conn,
				struct bt_pacs_server *srv, int err);

struct bt_pacs_server_cb {
	bt_pacs_lookup_cb lookup;
	bt_pacs_server_common_cb source_locations;
	bt_pacs_server_common_cb sink_locations;
};

struct bt_pacs_server {
	struct bt_gatt_service *service_p;

	/*
	 * Available Audio Contexts
	 *
	 * FIXME: The server may expose different values of the Available
	 * Audio Contexts characteristic to each client.
	 */
	uint16_t source_aac;
	uint16_t sink_aac;

	/* Supported Audio Contexts */
	uint16_t source_sac;
	uint16_t sink_sac;

	/* Locations */
	uint32_t source_locations;
	uint32_t sink_locations;

	/* PAC */
	sys_slist_t source_pac;	/* list of bt_pacs_server_pac */
	sys_slist_t sink_pac;	/* list of bt_pacs_server_pac */

	uint8_t buf[BT_PACS_BUF_SIZE];
};

ssize_t bt_pacs_read_sink_pac(struct bt_conn *conn,
				const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset);

ssize_t bt_pacs_read_sink_locations(struct bt_conn *conn,
				const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset);

ssize_t bt_pacs_write_sink_locations(struct bt_conn *conn,
				const struct bt_gatt_attr *attr,
				const void *buf, uint16_t len,
				uint16_t offset, uint8_t flags);

ssize_t bt_pacs_read_source_pac(struct bt_conn *conn,
				const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset);

ssize_t bt_pacs_read_source_locations(struct bt_conn *conn,
				const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset);

ssize_t bt_pacs_write_source_locations(struct bt_conn *conn,
				const struct bt_gatt_attr *attr,
				const void *buf, uint16_t len,
				uint16_t offset, uint8_t flags);

ssize_t bt_pacs_read_available_contexts(struct bt_conn *conn,
				const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset);

ssize_t bt_pacs_read_supported_contexts(struct bt_conn *conn,
				const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset);

int bt_pacs_server_sink_locations_set(struct bt_pacs_server *srv,
				uint32_t locations);

int bt_pacs_server_source_locations_set(struct bt_pacs_server *srv,
				uint32_t locations);

int bt_pacs_server_available_contexts_set(struct bt_conn *conn,
				struct bt_pacs_server *srv, uint16_t source_sac, uint16_t sink_sac);

int bt_pacs_server_supported_contexts_set(struct bt_conn *conn,
				struct bt_pacs_server *srv, uint16_t source_sac, uint16_t sink_sac);

const struct bt_pacs_pac *bt_pacs_server_get_sink_pac(struct bt_pacs_server *srv);

const struct bt_pacs_pac *bt_pacs_server_get_source_pac(struct bt_pacs_server *srv);

int bt_pacs_server_capability_init(struct bt_pacs_server *srv,
				bool sink, uint8_t coding_format,
				uint16_t company_id, uint16_t vs_codec_id,
				uint16_t samples, uint8_t durations,
				uint8_t channels, uint16_t octets_min,
				uint16_t octets_max, uint8_t frames,
				uint16_t preferrd_contexts);

int bt_pacs_server_locations_init(struct bt_pacs_server *srv,
				bool sink, uint32_t locations);

int bt_pacs_server_contexts_init(struct bt_pacs_server *srv,
				bool sink, uint16_t aac, uint16_t sac);

void bt_pacs_server_cb_register(struct bt_pacs_server_cb *cb);

struct bt_pacs_client_pac {
	uint16_t value_handle;
	uint8_t num_pac;
	struct bt_gatt_subscribe_params sub_params;

	struct bt_pacs_client *cli;

	sys_snode_t node;
	sys_slist_t pac;	/* list of bt_pacs_pac */
};

struct bt_pacs_client;

typedef void (*bt_pacs_discover_cb)(struct bt_conn *conn,
				struct bt_pacs_client *cli, int err);

typedef struct bt_pacs_client_pac *(*bt_pacs_client_alloc_cb)(void);

typedef struct bt_pacs_pac *(*bt_pacs_client_alloc_pac_cb)(void);

typedef void (*bt_pacs_client_common_cb)(struct bt_conn *conn,
				struct bt_pacs_client *cli, int err);

typedef void (*bt_pacs_client_pac_cb)(struct bt_conn *conn,
				struct bt_pacs_client *cli,
				struct bt_pacs_client_pac *cli_pac, int err);

struct bt_pacs_client_cb {
	bt_pacs_discover_cb discover;
	bt_pacs_client_alloc_cb alloc;
	bt_pacs_client_alloc_pac_cb alloc_pac;
	bt_pacs_client_pac_cb sink_pac;
	bt_pacs_client_common_cb sink_locations;
	bt_pacs_client_pac_cb source_pac;
	bt_pacs_client_common_cb source_locations;
	bt_pacs_client_common_cb available_contexts;
	bt_pacs_client_common_cb supported_contexts;
};

struct bt_pacs_client {
	uint16_t start_handle;
	uint16_t end_handle;
	uint16_t aac_handle;	/* available audio contexts */
	uint16_t sac_handle;	/* supported audio contexts */
	uint16_t source_loc_handle;
	uint16_t sink_loc_handle;

	struct bt_gatt_subscribe_params source_loc_sub_params;
	struct bt_gatt_subscribe_params sink_loc_sub_params;
	struct bt_gatt_subscribe_params aac_sub_params;
	struct bt_gatt_subscribe_params sac_sub_params;

	struct bt_gatt_write_params write_params;
	struct bt_gatt_read_params read_params;
	struct bt_gatt_discover_params discover_params;

	uint8_t busy;
	struct bt_uuid_16 uuid;

	/* Available Audio Contexts */
	uint16_t source_aac;
	uint16_t sink_aac;

	/* Supported Audio Contexts */
	uint16_t source_sac;
	uint16_t sink_sac;

	/* Locations */
	uint32_t source_locations;
	uint32_t sink_locations;

	/* PAC */
	sys_slist_t source_pac; /* list of bt_pacs_client_pac */
	sys_slist_t sink_pac;	/* list of bt_pacs_client_pac */

	/* The bt_pacs_client_pac that initiates the reading */
	struct bt_pacs_client_pac *pac_reading;
};

int bt_pacs_client_read_sink_pac(struct bt_conn *conn,
				struct bt_pacs_client *cli,
				struct bt_pacs_client_pac *cli_pac);

int bt_pacs_client_read_sink_locations(struct bt_conn *conn,
				struct bt_pacs_client *cli);

int pacs_client_write_locations(struct bt_conn *conn,
				struct bt_pacs_client *cli, uint32_t locations,
				bool sink);

int bt_pacs_client_write_sink_locations(struct bt_conn *conn,
				struct bt_pacs_client *cli, uint32_t locations);

int bt_pacs_client_read_source_pac(struct bt_conn *conn,
				struct bt_pacs_client *cli,
				struct bt_pacs_client_pac *cli_pac);

int bt_pacs_client_read_source_locations(struct bt_conn *conn,
				struct bt_pacs_client *cli);

int bt_pacs_client_write_source_locations(struct bt_conn *conn,
				struct bt_pacs_client *cli, uint32_t locations);

int bt_pacs_client_read_available_contexts(struct bt_conn *conn,
				struct bt_pacs_client *cli);

int bt_pacs_client_read_supported_contexts(struct bt_conn *conn,
				struct bt_pacs_client *cli);

int bt_pacs_discover(struct bt_conn *conn, struct bt_pacs_client *cli);

void bt_pacs_client_cb_register(struct bt_pacs_client_cb *cb);

int bt_pacs_client_disconnect(struct bt_conn *conn, struct bt_pacs_client *cli);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_PACS_H_ */
