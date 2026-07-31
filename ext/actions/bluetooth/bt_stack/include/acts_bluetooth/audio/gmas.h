/** @file
 *  @brief Gaming Audio Service
 */

/*
 * Copyright (c) 2024 Actions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_GMAS_H_
#define ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_GMAS_H_

#include <misc/util.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Gaming Role bitfield */
enum bt_gmap_role {
	/**
	 * @brief Gaming Role Unicast Game Gateway
	 *
	 * Requires @kconfig{CONFIG_BT_CAP_INITIATOR}, @kconfig{CONFIG_BT_BAP_UNICAST_CLIENT} and
	 * @kconfig{CONFIG_BT_VCP_VOL_CTLR} to be enabled.
	 */
	BT_GMAP_ROLE_UGG = BIT(0),
	/**
	 * @brief Gaming Role Unicast Game Terminal
	 *
	 * Requires @kconfig{CONFIG_BT_CAP_ACCEPTOR} and @kconfig{CONFIG_BT_BAP_UNICAST_SERVER} to
	 * be enabled.
	 */
	BT_GMAP_ROLE_UGT = BIT(1),
	/**
	 * @brief Gaming Role Broadcast Game Sender
	 *
	 * Requires @kconfig{CONFIG_BT_CAP_INITIATOR} and @kconfig{CONFIG_BT_BAP_BROADCAST_SOURCE}
	 * to be enabled.
	 */
	BT_GMAP_ROLE_BGS = BIT(2),
	/**
	 * @brief Gaming Role Broadcast Game Receiver
	 *
	 * Requires @kconfig{CONFIG_BT_CAP_ACCEPTOR}, @kconfig{CONFIG_BT_BAP_BROADCAST_SINK} and
	 * @kconfig{CONFIG_BT_VCP_VOL_REND} to be enabled.
	 */
	BT_GMAP_ROLE_BGR = BIT(3),
};

/** Unicast Game Gateway Feature bitfield */
enum bt_gmap_ugg_feat {
	/**
	 * @brief Support transmitting multiple LC3 codec frames per block in an SDU
	 *
	 * Requires @kconfig{CONFIG_BT_BAP_UNICAST_CLIENT_ASE_SRC_COUNT} > 0
	 */
	BT_GMAP_UGG_FEAT_MULTIPLEX = BIT(0),
	/**
	 * @brief 96 kbps source support
	 *
	 * Requires @kconfig{CONFIG_BT_BAP_UNICAST_CLIENT_ASE_SRC_COUNT} > 0
	 */
	BT_GMAP_UGG_FEAT_96KBPS_SOURCE = BIT(1),
	/**
	 * @brief Support for receiving at least two channels of audio, each in a separate CIS
	 *
	 * Requires @kconfig{CONFIG_BT_BAP_UNICAST_CLIENT_ASE_SNK_COUNT} > 1 and
	 * @kconfig{CONFIG_BT_BAP_UNICAST_CLIENT_GROUP_STREAM_COUNT} > 1
	 */
	BT_GMAP_UGG_FEAT_MULTISINK = BIT(2),
};

/** Unicast Game Terminal Feature bitfield */
enum bt_gmap_ugt_feat {
	/**
	 * @brief Source support
	 *
	 * Requires @kconfig{CONFIG_BT_ASCS_ASE_SRC_COUNT} > 0
	 */
	BT_GMAP_UGT_FEAT_SOURCE = BIT(0),
	/**
	 * @brief 80 kbps source support
	 *
	 * Requires BT_GMAP_UGT_FEAT_SOURCE to be set as well
	 */
	BT_GMAP_UGT_FEAT_80KBPS_SOURCE = BIT(1),
	/**
	 * @brief Sink support
	 *
	 * Requires @kconfig{CONFIG_BT_ASCS_ASE_SNK_COUNT} > 0
	 */
	BT_GMAP_UGT_FEAT_SINK = BIT(2),
	/**
	 * @brief 64 kbps sink support
	 *
	 * Requires BT_GMAP_UGT_FEAT_SINK to be set as well
	 */
	BT_GMAP_UGT_FEAT_64KBPS_SINK = BIT(3),
	/**
	 * @brief Support for receiving multiple LC3 codec frames per block in an SDU
	 *
	 * Requires BT_GMAP_UGT_FEAT_SINK to be set as well
	 */
	BT_GMAP_UGT_FEAT_MULTIPLEX = BIT(4),
	/**
	 * @brief Support for receiving at least two audio channels, each in a separate CIS
	 *
	 * Requires @kconfig{CONFIG_BT_ASCS_ASE_SNK_COUNT} > 1 and
	 * @kconfig{CONFIG_BT_ASCS_MAX_ACTIVE_ASES} > 1, and BT_GMAP_UGT_FEAT_SINK to be set as well
	 */
	BT_GMAP_UGT_FEAT_MULTISINK = BIT(5),
	/**
	 * @brief Support for sending at least two audio channels, each in a separate CIS
	 *
	 * Requires @kconfig{CONFIG_BT_ASCS_ASE_SRC_COUNT} > 1 and
	 * @kconfig{CONFIG_BT_ASCS_MAX_ACTIVE_ASES} > 1, and BT_GMAP_UGT_FEAT_SOURCE to be set
	 * as well
	 */
	BT_GMAP_UGT_FEAT_MULTISOURCE = BIT(6),
};

/** Broadcast Game Sender Feature bitfield */
enum bt_gmap_bgs_feat {
	/** 96 kbps support */
	BT_GMAP_BGS_FEAT_96KBPS = BIT(0),
};

/** Broadcast Game Receiver Feature bitfield */
enum bt_gmap_bgr_feat {
	/**
	 * @brief Support for receiving at least two audio channels, each in a separate BIS
	 *
	 * Requires @kconfig{CONFIG_BT_BAP_BROADCAST_SNK_STREAM_COUNT} > 1
	 */
	BT_GMAP_BGR_FEAT_MULTISINK = BIT(0),
	/** @brief Support for receiving multiple LC3 codec frames per block in an SDU */
	BT_GMAP_BGR_FEAT_MULTIPLEX = BIT(1),
};

/** Broadcast Game Receiver Feature bitfield */
struct bt_gmap_feat {
	/** Unicast Game Gateway features */
	enum bt_gmap_ugg_feat ugg_feat;
	/** Unicast Game Terminal features */
	enum bt_gmap_ugt_feat ugt_feat;
	/** Broadcast Game Sender features */
	enum bt_gmap_bgs_feat bgs_feat;
	/** Broadcast Game Receiver features */
	enum bt_gmap_bgr_feat bgr_feat;
};

/* For gmas client */

struct bt_gmas_client {
	uint8_t role;
	uint8_t ugg_feat;
	uint8_t ugt_feat;
	uint8_t bgs_feat;
	uint8_t bgr_feat;

	uint16_t start_handle;
	uint16_t end_handle;
	uint16_t role_handle;
	uint16_t ugg_feat_handle;
	uint16_t ugt_feat_handle;
	uint16_t bgs_feat_handle;
	uint16_t bgr_feat_handle;

	uint8_t busy;
	struct bt_uuid_16 uuid;

	struct bt_gatt_read_params read_params;
	struct bt_gatt_discover_params discover_params;
};

typedef void (*bt_gmas_discover_cb)(struct bt_conn *conn,
				struct bt_gmas_client *cli, int err);

typedef void (*bt_gmas_client_read_role_cb)(struct bt_conn *conn,
				struct bt_gmas_client *cli, int err);

typedef void (*bt_gmas_client_read_ugg_feat_cb)(struct bt_conn *conn,
				struct bt_gmas_client *cli, int err);

typedef void (*bt_gmas_client_read_ugt_feat_cb)(struct bt_conn *conn,
				struct bt_gmas_client *cli, int err);

typedef void (*bt_gmas_client_read_bgs_feat_cb)(struct bt_conn *conn,
				struct bt_gmas_client *cli, int err);

typedef void (*bt_gmas_client_read_bgr_feat_cb)(struct bt_conn *conn,
				struct bt_gmas_client *cli, int err);

struct bt_gmas_client_cb {
	bt_gmas_discover_cb discover;
	bt_gmas_client_read_role_cb role;
	bt_gmas_client_read_ugg_feat_cb ugg_feat;
	bt_gmas_client_read_ugt_feat_cb ugt_feat;
	bt_gmas_client_read_bgs_feat_cb bgs_feat;
	bt_gmas_client_read_bgr_feat_cb bgr_feat;
};

int bt_gmas_discover(struct bt_conn *conn, struct bt_gmas_client *cli);

int bt_gmas_client_read_role(struct bt_conn *conn, struct bt_gmas_client *cli);

int bt_gmas_client_read_ugg_feat(struct bt_conn *conn, struct bt_gmas_client *cli);

int bt_gmas_client_read_ugt_feat(struct bt_conn *conn, struct bt_gmas_client *cli);

int bt_gmas_client_read_bgs_feat(struct bt_conn *conn, struct bt_gmas_client *cli);

int bt_gmas_client_read_bgr_feat(struct bt_conn *conn, struct bt_gmas_client *cli);

void bt_gmas_client_cb_register(struct bt_gmas_client_cb *cb);

/* For gmas server */

ssize_t bt_gmas_read_role(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset);

ssize_t bt_gmas_read_ugg_feat(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset);

ssize_t bt_gmas_read_ugt_feat(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset);

ssize_t bt_gmas_read_bgs_feat(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset);

ssize_t bt_gmas_read_bgr_feat(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset);

uint8_t bt_gmas_get_loc_role(void);

int bt_gmas_server_init(uint8_t role, struct bt_gmap_feat feat);


#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_GMAS_H_ */

