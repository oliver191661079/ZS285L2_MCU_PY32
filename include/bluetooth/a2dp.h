/** @file
 * @brief Advance Audio Distribution Profile header.
 */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __BT_A2DP_H
#define __BT_A2DP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <bluetooth/avdtp.h>

/** @brief Stream Structure */
struct bt_a2dp_stream {
	/* TODO */
};

/** @brief Codec ID */
enum bt_a2dp_codec_id {
	/** Codec SBC */
	BT_A2DP_SBC = 0x00,
	/** Codec MPEG-1 */
	BT_A2DP_MPEG1 = 0x01,
	/** Codec MPEG-2 */
	BT_A2DP_MPEG2 = 0x02,
	/** Codec ATRAC */
	BT_A2DP_ATRAC = 0x04,
	/** Codec Non-A2DP */
	BT_A2DP_VENDOR = 0xff
};

enum{
    BT_A2DP_SBC_48000 = 1,
    BT_A2DP_SBC_44100 = 2,
    BT_A2DP_SBC_32000 = 4,
    BT_A2DP_SBC_16000 = 8,
    BT_A2DP_SBC_FREQ_MASK = 0xF,
};

enum{
    BT_A2DP_SBC_JOINT_STEREO  = 1,
    BT_A2DP_SBC_STEREO        = 2,
    BT_A2DP_SBC_DUAL_CHANNEL  = 4,
    BT_A2DP_SBC_MONO          = 8,
    BT_A2DP_SBC_CHANNEL_MODE_MASK = 0xF,
};

enum{
    BT_A2DP_SBC_BLOCK_LENGTH_16 = 1,
    BT_A2DP_SBC_BLOCK_LENGTH_12 = 2,
    BT_A2DP_SBC_BLOCK_LENGTH_8  = 4,
    BT_A2DP_SBC_BLOCK_LENGTH_4  = 8,
    BT_A2DP_SBC_BLOCK_LENGTH_MASK = 0xF,
};

enum{
    BT_A2DP_SBC_SUBBANDS_8 = 1,
    BT_A2DP_SBC_SUBBANDS_4 = 2,
    BT_A2DP_SBC_SUBBANDS_MASK = 0x3,
};

enum{
    BT_A2DP_SBC_ALLOCATION_METHOD_LOUDNESS = 1,
    BT_A2DP_SBC_ALLOCATION_METHOD_SNR      = 2,
    BT_A2DP_SBC_ALLOCATION_METHOD_MASK		= 0x3,
};


/** @brief Preset for the endpoint */
struct bt_a2dp_preset {
	/** Length of preset */
	u8_t len;
	/** Preset */
	u8_t preset[0];
};

/** @brief Stream End Point */
struct bt_a2dp_endpoint {
	/** Code ID */
	u8_t codec_id;
	/** Stream End Point Information */
	struct bt_avdtp_seid_lsep info;
	/** Pointer to preset codec chosen */
	struct bt_a2dp_preset *preset;
	/** Capabilities */
	struct bt_a2dp_preset *caps;
};

/** @brief Stream End Point Media Type */
enum MEDIA_TYPE {
	/** Audio Media Type */
	BT_A2DP_AUDIO = 0x00,
	/** Video Media Type */
	BT_A2DP_VIDEO = 0x01,
	/** Multimedia Media Type */
	BT_A2DP_MULTIMEDIA = 0x02
};

/** @brief Stream End Point Role */
enum ROLE_TYPE {
	/** Source Role */
	BT_A2DP_SOURCE = 0,
	/** Sink Role */
	BT_A2DP_SINK = 1
};

/** @brief A2DP structure */
struct bt_a2dp;

enum {
	//#define BT_AVDTP_OPEN                 0x06
	BT_A2DP_MEDIA_STATE_OPEN	= 0x06,

	//#define BT_AVDTP_START                0x07
	BT_A2DP_MEDIA_STATE_START	= 0x07,

	//#define BT_AVDTP_CLOSE                0x08
	BT_A2DP_MEDIA_STATE_CLOSE	= 0x08,

	//#define BT_AVDTP_SUSPEND              0x09
	BT_A2DP_MEDIA_STATE_SUSPEND 	= 0x09,
};

struct bt_a2dp_app_cb {
	void (*connected)(void *session_hdl, u8_t session_type, struct bt_conn *conn);
	void (*disconnected)(void *session_hdl, u8_t session_type, struct bt_conn *conn);
	void (*media_handler)(void *session_hdl, struct net_buf *buf);

	/* Return 0: accepte state request, other: reject state request */
	int (*media_state_req)(void *session_hdl, u8_t state, struct bt_conn *conn);
	void (*media_codec_config)(struct bt_a2dp_media_sbc_codec *codec);
};

/** @brief A2DP Connect.
 *
 *  This function is to be called after the conn parameter is obtained by
 *  performing a GAP procedure. The API is to be used to establish A2DP
 *  connection between devices.
 *
 *  @param conn Pointer to bt_conn structure.
 *
 *  @return pointer to struct bt_a2dp in case of success or NULL in case
 *  of error.
 */
struct bt_a2dp *bt_a2dp_connect(struct bt_conn *conn);

/* Disconnect a2dp session */
int bt_a2dp_disconnect(void *session);

/** @brief Endpoint Registration.
 *
 *  This function is used for registering the stream end points. The user has
 *  to take care of allocating the memory, the preset pointer and then pass the
 *  required arguments. Also, only one sep can be registered at a time.
 *
 *  @param endpoint Pointer to bt_a2dp_endpoint structure.
 *  @param media_type Media type that the Endpoint is.
 *  @param role Role of Endpoint.
 *
 *  @return 0 in case of success and error code in case of error.
 */
int bt_a2dp_register_endpoint(struct bt_a2dp_endpoint *endpoint,
			      u8_t media_type, u8_t role);

/* Register app callback */
int bt_a2dp_register_cb(struct bt_a2dp_app_cb *cb);

#if defined(CONFIG_BT_PTS_TEST)
int bt_a2dp_discover(void *session);
int bt_a2dp_get_capabilities(void *session);
int bt_a2dp_get_all_capabilities(void *session);
int bt_a2dp_set_configuration(void *session);
int bt_a2dp_reconfig(void *session);
int bt_a2dp_open(void *session);
int bt_a2dp_start(void *session);
int bt_a2dp_suspend(void *session);
int bt_a2dp_close(void *session);
int bt_a2dp_abort(void *session);
int bt_a2dp_delayreport(void *session, u16_t delay_time);
int bt_a2dp_disconnect_media_session(void *session);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __BT_A2DP_H */
