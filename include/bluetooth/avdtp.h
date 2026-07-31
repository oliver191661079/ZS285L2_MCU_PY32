/** @file
 * @brief Audio/Video Distribution Transport Protocol header.
 */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __BT_AVDTP_H
#define __BT_AVDTP_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
	BT_AVDTP_SIGNALING_SESSION	= 0x0,
	BT_AVDTP_MEDIA_SESSION		= 0x1,
	BT_AVDTP_REPORTING_SESSION	= 0x2,
	BT_AVDTP_RECOVERY_SESSION	= 0x3,
	BT_AVDTP_MAX_SESSION		= 4,
};

enum{
	BT_AVDTP_MEDIA_TYPE_AUDIO = 0,
	BT_AVDTP_MEDIA_TYPE_VIDEO,
	BT_AVDTP_MEDIA_TYPE_MULTIMEDIA,
};

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
/** @brief AVDTP SEID Information */
struct bt_avdtp_seid_info {
	/** Reserved */
	uint8_t rfa0:1;
	/** End Point usage status */
	uint8_t inuse:1;
	/** Stream End Point ID */
	uint8_t id:6;
	/** Reserved */
	uint8_t rfa1:3;
	/** TSEP of the End Point */
	uint8_t tsep:1;
	/** Media-type of the End Point */
	uint8_t media_type:4;
} __packed;

struct bt_a2dp_media_sbc_codec
{
	u8_t rfa0:4;
	u8_t media_type:4;
	u8_t codec_type;
	u8_t channel_mode:4;
	u8_t freq:4;
	u8_t alloc_method:2;
	u8_t subbands:2;
	u8_t block_len:4;
	u8_t min_bitpool;
	u8_t max_bitpool;
}__packed;

#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
/** @brief AVDTP SEID Information */
struct bt_avdtp_seid_info {
	/** Stream End Point ID */
	u8_t id:6;
	/** End Point usage status */
	u8_t inuse:1;
	/** Reserved */
	u8_t rfa0:1;
	/** Media-type of the End Point */
	u8_t media_type:4;
	/** TSEP of the End Point */
	u8_t tsep:1;
	/** Reserved */
	u8_t rfa1:3;
} __packed;
#endif

/** @brief AVDTP Local SEP*/
struct bt_avdtp_seid_lsep {
	/** Stream End Point information */
	struct bt_avdtp_seid_info sep;

	/** Pointer to Stream structure */
	struct bt_avdtp_stream *stream;

	/** Pointer to next local Stream End Point structure */
	struct bt_avdtp_seid_lsep *next;
};

/** @brief AVDTP Stream */
struct bt_avdtp_stream {
	struct bt_avdtp_seid_info lsep; /* Configured Local SEP */
	struct bt_avdtp_seid_info rsep; /* Configured Remote SEP*/
	struct bt_a2dp_media_sbc_codec codec;	/* sbc codec config */
	u8_t stream_state;				/* current state of the stream */
	u8_t acp_state;					/* Device as acp state */
	u8_t int_state;					/* Device as int state */
};

#ifdef __cplusplus
}
#endif

#endif /* __BT_AVDTP_H */
