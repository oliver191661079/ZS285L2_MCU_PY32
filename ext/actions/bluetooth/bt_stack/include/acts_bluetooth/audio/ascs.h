/** @file
 *  @brief GATT Audio Stream Control Service
 */

/*
 * Copyright (c) 2020 Actions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_ASCS_H_
#define ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_ASCS_H_

#include <toolchain.h>
#include <zephyr/types.h>
#include <misc/slist.h>

#include <acts_bluetooth/bluetooth.h>
#include <acts_bluetooth/iso.h>
#include <acts_bluetooth/audio/codec.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_BT_ASCS_BUF_SIZE
#define BT_ASCS_BUF_SIZE	CONFIG_BT_ASCS_BUF_SIZE
#else
#define BT_ASCS_BUF_SIZE	0
#endif

#ifdef CONFIG_BT_ASCS_RSP_SIZE
#define BT_ASCS_RSP_SIZE	CONFIG_BT_ASCS_RSP_SIZE
#else
#define BT_ASCS_RSP_SIZE	0
#endif

#ifdef CONFIG_BT_ASCS_CLIENT_BUF_SIZE
#define BT_ASCS_CLIENT_BUF_SIZE	CONFIG_BT_ASCS_CLIENT_BUF_SIZE
#else
#define BT_ASCS_CLIENT_BUF_SIZE	0
#endif

#ifdef CONFIG_BT_ASCS_CCID_COUNT
#define BT_ASCS_CCID_COUNT	CONFIG_BT_ASCS_CCID_COUNT
#else
#define BT_ASCS_CCID_COUNT	4
#endif

/*
 * ASE FSM
 */
#define BT_ASCS_IDLE                0x00
#define BT_ASCS_CODEC_CONFIGURED    0x01
#define BT_ASCS_QOS_CONFIGURED      0x02
#define BT_ASCS_ENABLING            0x03
#define BT_ASCS_STREAMING           0x04
#define BT_ASCS_DISABLING           0x05
#define BT_ASCS_RELEASING           0x06

/*
 * ASE notifications
 */

/* NOTE: Idle/Releasing has no addtional_ase_parameters */
struct bt_ascs_hdr {
	uint8_t ase_id;	/* range: [0x01, 0xFF] */
	uint8_t ase_state;
} __packed;

/* Direction */
#define BT_ASCS_DIR_SINK            0x01
#define BT_ASCS_DIR_SOURCE          0x02

/* Framing */
#define BT_ASCS_UNFRAMED_SUPPORTED        0x00
#define BT_ASCS_UNFRAMED_NOT_SUPPORTED    0x01

/* Preferred_PHY */
#define BT_ASCS_PREFERRED_PHY_1M       BIT(0)
#define BT_ASCS_PREFERRED_PHY_2M       BIT(1)
#define BT_ASCS_PREFERRED_PHY_CODED    BIT(2)
#define BT_ASCS_PREFERRED_PHY_NONE     0x00
#define BT_ASCS_PREFERRED_PHY_MASK     0x07

/* Preferred_Retransmission_Number */
#define BT_ASCS_PREFERRED_RTN_MIN         0x00
#define BT_ASCS_PREFERRED_RTN_MAX         0xFF

/* Max_Transport_Latency, unit: ms */
#define BT_ASCS_MAX_LATENCY_MIN     0x0005
#define BT_ASCS_MAX_LATENCY_MAX     0x0FA0	/* 40000ms */

/* ASE: Codec Configured */
struct bt_ascs_codec_configured {
	uint8_t framing;	/* supported_framing */
	uint8_t preferred_phy;
	uint8_t preferred_rtn;	/* [0x00, 0xFF] */
	uint16_t max_latency;	/* preferred_max_latency, [0x0005, 0x0FA0] */

	/* Presentation_Delay, unit: us */
	uint8_t delay_min[3];
	uint8_t delay_max[3];
	uint8_t preferred_delay_min[3];
	uint8_t preferred_delay_max[3];

//	struct codec_id codec;
	uint8_t coding_format;
	uint16_t company_id;
	uint16_t vs_codec_id;

	/*
	 * struct sampling_frequency
	 * struct frame_duration
	 * struct audio_channel_allocation
	 * struct octets_per_codec_frame
	 * struct codec_frame_blocks_per_sdu
	 */
	/* Codec Specific Configuration length */
	uint8_t cc_len;
	/* Codec Specific Configuration */
	uint8_t cc[0];
} __packed;

#define BT_ASCS_SDU_INTERVAL_MIN         0x0000FF
#define BT_ASCS_SDU_INTERVAL_MAX         0x0FFFFF

/* Framing */
#define BT_ASCS_UNFRAMED              0x00
#define BT_ASCS_FRAMED                0x01

/* PHY */
#define BT_ASCS_PHY_1M             BIT(0)
#define BT_ASCS_PHY_2M             BIT(1)
#define BT_ASCS_PHY_CODED          BIT(2)

/* Max_SDU */
#define BT_ASCS_MAX_SDU_MIN           0x0000
#define BT_ASCS_MAX_SDU_MAX           0x0FFF

/* Retransmission_Number */
#define BT_ASCS_RTN_MIN               0x00
#define BT_ASCS_RTN_MAX               0xFF

/* ASE: QoS Configured */
struct bt_ascs_qos_configured {
	uint8_t cig_id;
	uint8_t cis_id;
	uint8_t sdu_interval[3];	/* [0x0000FF, 0x0FFFFF], unit: us */
	uint8_t framing;
	uint8_t phy;
	uint16_t max_sdu;	/* [0x0000, 0x0FFF] */
	uint8_t rtn;	/* [0x00, 0xFF] */
	uint16_t max_latency;	/* [0x0005, 0x0FA0], unit: ms */
	uint8_t delay[3];	/* unit: us */
} __packed;

/* ASE: Enabling/Streaming/Disabling */
struct bt_ascs_state_common {
	uint8_t cig_id;
	uint8_t cis_id;
	uint8_t meta_len;
	uint8_t meta[0];
} __packed;

struct bt_ascs_enabling {
	uint8_t cig_id;
	uint8_t cis_id;
	uint8_t meta_len;
	uint8_t meta[0];
} __packed;

struct bt_ascs_streaming {
	uint8_t cig_id;
	uint8_t cis_id;
	uint8_t meta_len;
	uint8_t meta[0];
} __packed;

struct bt_ascs_disabling {
	uint8_t cig_id;
	uint8_t cis_id;
	uint8_t meta_len;
	uint8_t meta[0];
} __packed;

/*
 * ASE control point opcode
 */
#define BT_ASCS_OPCODE_CONFIG_CODEC           0x01
#define BT_ASCS_OPCODE_CONFIG_QOS             0x02	/* client initiates only */
#define BT_ASCS_OPCODE_ENABLE                 0x03	/* client initiates only */
#define BT_ASCS_OPCODE_RECV_START             0x04
#define BT_ASCS_OPCODE_DISABLE                0x05
#define BT_ASCS_OPCODE_RECV_STOP              0x06
#define BT_ASCS_OPCODE_UPDATE_META            0x07
#define BT_ASCS_OPCODE_RELEASE                0x08
#define BT_ASCS_OPCODE_RELEASED               0xEE	/* helper: server initiates only */

/*
 * response_code
 */
#define BT_ASCS_RSP_SUCCESS               0x00
#define BT_ASCS_RSP_UNSUPP_OPCODE         0x01
#define BT_ASCS_RSP_INVALID_LEN           0x02
#define BT_ASCS_RSP_INVALID_ASE_ID        0x03
#define BT_ASCS_RSP_INVALID_FSM           0x04
#define BT_ASCS_RSP_INVALID_ASE_DIR       0x05
#define BT_ASCS_RSP_UNSUPP_AUDIO_CAP      0x06
#define BT_ASCS_RSP_UNSUPP_CONFIG         0x07	/* reason: BT_ASCS_REA_XXX */
#define BT_ASCS_RSP_REJECTED_CONFIG       0x08	/* reason: BT_ASCS_REA_XXX */
#define BT_ASCS_RSP_INVALID_CONFIG        0x09	/* reason: BT_ASCS_REA_XXX */
#define BT_ASCS_RSP_UNSUPP_META           0x0A	/* reason: Metadata Type */
#define BT_ASCS_RSP_REJECTED_META         0x0B	/* reason: Metadata Type */
#define BT_ASCS_RSP_INVALID_META          0x0C	/* reason: Metadata Type */
#define BT_ASCS_RSP_INSUFF_RESOURCES      0x0D
#define BT_ASCS_RSP_UNSPECIFIED_ERR       0x0E

/*
 * reasons for Unsupported/Rejected/Invalid Configuretion Parameter Value
 */
#define BT_ASCS_REA_CODEC_ID              0x01
#define BT_ASCS_REA_CODEC_CONFIG          0x02
#define BT_ASCS_REA_SDU_INTERVAL          0x03
#define BT_ASCS_REA_FRAMING               0x04
#define BT_ASCS_REA_PHY                   0x05
#define BT_ASCS_REA_MAX_SDU_SIZE          0x06
#define BT_ASCS_REA_RTN                   0x07
#define BT_ASCS_REA_MAX_LATENCY           0x08
#define BT_ASCS_REA_DELAY                 0x09
#define BT_ASCS_REA_INVALID_MAP           0x0A

/* BAP: section 4.6 ASE Control operations */
#define BT_ASCS_CTRL_TIMEOUT	K_MSEC(1000)

#define BT_ASCS_READ_TIEMOUT	K_MSEC(1000)

/*
 * ASE control point notifications
 */
struct bt_ascs_rsp_hdr {
	uint8_t opcode;
	uint8_t num_of_ase;
} __packed;

struct bt_ascs_rsp_data {
	uint8_t ase_id;	/* 0x00: if response_code is 0x01 or 0x02 */
	uint8_t code;	/* response_code */
	uint8_t reason;	/* 0x00: if response_code is 0x01 or 0x02 */
} __packed;

struct bt_ascs_response {
	uint8_t opcode;
	uint8_t num_of_ase;	/* 0xFF: if response_code is 0x01 or 0x02 */

	struct bt_ascs_rsp_data data[0];
} __packed;

/* Target_Latency: bound with Target_PHY */
#define BT_ASCS_TARGET_LATENCY_LOW        0x01
#define BT_ASCS_TARGET_LATENCY_BALANCED   0x02
#define BT_ASCS_TARGET_LATENCY_HIGH       0x03

/* Target_PHY: bound with Target_Latency */
#define BT_ASCS_TARGET_PHY_1M          0x01
#define BT_ASCS_TARGET_PHY_2M          0x02
#define BT_ASCS_TARGET_PHY_CODED       0x03

struct bt_ascs_control_hdr {
	uint8_t opcode;
	uint8_t num_of_ase;
} __packed;

struct bt_ascs_config_codec {
	uint8_t ase_id;
	uint8_t target_latency;
	uint8_t target_phy;
	uint8_t coding_format;
	uint16_t company_id;
	uint16_t vs_codec_id;

	/*
	 * struct bt_sampling_frequency
	 * struct bt_frame_duration
	 * struct bt_audio_channel_allocation
	 * struct bt_octets_per_codec_frame
	 * struct bt_codec_frame_blocks_per_sdu
	 */
	uint8_t cc_len;	/* codec_specific_configuration_length */
	uint8_t cc[0];	/* codec_specific_configuration[0] */
} __packed;

struct bt_ascs_config_qos {
	uint8_t ase_id;
	uint8_t cig_id;
	uint8_t cis_id;
	uint8_t sdu_interval[3];	/* [0x0000FF, 0x0FFFFF] */
	uint8_t framing;
	uint8_t phy;
	uint16_t max_sdu;	/* [0x0, 0xFFF] */
	uint8_t rtn; /* retransmission_number, [0x00, 0xFF] */
	uint16_t max_latency;	/* max_transport_latency, [0x0005, 0x0FA0] */
	uint8_t delay[3];	/* presentation_delay, unit: us */
} __packed;

#if 1
/* for disable/recv_start_ready/recv_stop_ready/release */
struct bt_ascs_op_common {
	uint8_t ase_id;
} __packed;
#endif

struct bt_ascs_enable {
	uint8_t ase_id;
	uint8_t meta_len;	/* metadata_length */
	uint8_t meta[0];	/* metadata */
} __packed;

struct bt_ascs_update {
	uint8_t ase_id;
	uint8_t meta_len;	/* metadata_length */
	uint8_t meta[0];	/* metadata */
} __packed;

/* for enable/update_metadata */
struct bt_ascs_op_meta {
	uint8_t ase_id;
	uint8_t meta_len;	/* metadata_length */
	uint8_t meta[0];	/* metadata */
} __packed;

/*
 * Common ASE structure
 */
struct bt_ascs_ase {
	uint8_t ase_id;
	uint8_t ase_state;
	uint8_t direction;

	/* Config Codec: client initiates */
	uint8_t target_latency;
	uint8_t target_phy;
	uint8_t coding_format;
	uint16_t company_id;
	uint16_t vs_codec_id;

	/* Codec Configured: server exposes */
	uint8_t supported_framing;
	uint8_t preferred_phy;
	uint8_t preferred_rtn;	/* [0x00, 0xFF] */
	uint16_t preferred_max_latency;	/* [0x0005, 0x0FA0], unit: ms */
	/* Presentation_Delay, unit: us */
	uint32_t delay_min;
	uint32_t delay_max;
	uint32_t preferred_delay_min;
	uint32_t preferred_delay_max;

	/* Config QoS (client initiates) and QoS Configured (server exposes) */
	uint8_t cig_id;
	uint8_t cis_id;
	uint32_t sdu_interval; /* [0x0000FF, 0x0FFFFF] */
	uint8_t framing;
	uint8_t phy;
	uint16_t max_sdu;	/* [0x0, 0xFFF] */
	/*
	 * qos_configured/codec_cofigured/set_cig_param: [0x00, 0x0F]
	 * config_qos/bap_4.6.2_qos_configuration: [0x00, 0xFF]
	 * already fixed in core 5.3
	 */
	uint8_t rtn; /* [0x00, 0xFF] */
	uint16_t max_latency;	/* [0x0005, 0x0FA0], unit: ms */
	uint32_t delay;	/* Presentation_Delay, unit: us */

	/*
	 * codec_specific_configuration:
	 *
	 * struct bt_sampling_frequency
	 * struct bt_frame_duration
	 * struct bt_audio_channel_allocation
	 * struct bt_octets_per_codec_frame
	 * struct bt_codec_frame_blocks_per_sdu
	 */

	/* if need to expose in codec_configured state */
	uint8_t sampling_bit : 1;
	uint8_t duration_bit : 1;
	uint8_t locations_bit : 1;
	uint8_t octets_bit : 1;
	uint8_t blocks_bit : 1;

	uint8_t cc_len;	/* codec_specific_configuration_length */
	uint8_t sampling;
	uint8_t duration;
	uint32_t locations;
	uint16_t octets;
	uint8_t blocks;

	/*
	 * metadata
	 *
	 * struct streaming_audio_contexts
	 */
	uint8_t meta_len;
	uint16_t audio_contexts;
	uint8_t ccid_count;	/* active ccid count */
	uint8_t ccid_list[BT_ASCS_CCID_COUNT];
	/* FIXME: add more metadata types... */
};

struct bt_ascs_server;

struct bt_ascs_server_ase {
	/* characteristic value's attribute */
	const struct bt_gatt_attr *attr;

	struct bt_iso_chan *chan;

	sys_snode_t node;

	/* whether need to notify ASE after responding to ASE Control */
	uint8_t ase_notify;
	uint8_t ase_released : 1;

	struct bt_ascs_ase ase;
};

struct bt_ascs_server {
	sys_slist_t ases;	/* list of bt_ascs_server_ase */

	uint8_t buf[BT_ASCS_BUF_SIZE];
	uint8_t rsp[BT_ASCS_RSP_SIZE];
};

typedef struct bt_ascs_server *(*bt_ascs_lookup_cb)(struct bt_conn *conn);

/* high-byte: reason, low-byte: response_code */
typedef uint16_t (*bt_ascs_control_cb)(struct bt_conn *conn, uint8_t opcode,
				uint8_t *buf, uint16_t len,
				struct bt_ascs_server_ase *srv_ase);

typedef void (*bt_ascs_connect_cb)(struct bt_conn *conn);
typedef void (*bt_ascs_disconnect_cb)(struct bt_conn *conn);

struct bt_ascs_server_cb {
	bt_ascs_lookup_cb lookup;
	bt_ascs_control_cb control;
	bt_ascs_connect_cb connect;
	bt_ascs_disconnect_cb disconnect;
};

int bt_ascs_ase_state_update(struct bt_ascs_ase *ase, uint8_t opcode,
				bool client);

int bt_ascs_set_preferred_qos(struct bt_ascs_ase *ase, uint8_t framing,
				uint8_t phy, uint8_t rtn, uint16_t latency,
				uint32_t delay_min, uint32_t delay_max,
				uint32_t preferred_delay_min,
				uint32_t preferred_delay_max);

ssize_t bt_ascs_write_control(struct bt_conn *conn,
				const struct bt_gatt_attr *attr,
				const void *buf, uint16_t len,
				uint16_t offset, uint8_t flags);

ssize_t bt_ascs_read_ase(struct bt_conn *conn,
				const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset);

int bt_ascs_server_config_codec(struct bt_conn *conn,
				struct bt_ascs_server *srv,
				struct bt_ascs_server_ase *srv_ase);

int bt_ascs_server_notify_state(struct bt_conn *conn,
				struct bt_ascs_server *srv,
				struct bt_ascs_server_ase *srv_ase);

int bt_ascs_server_disable(struct bt_conn *conn,
				struct bt_ascs_server *srv,
				struct bt_ascs_server_ase *srv_ase);

int bt_ascs_server_recv_start(struct bt_conn *conn,
				struct bt_ascs_server *srv,
				struct bt_ascs_server_ase *srv_ase);

int bt_ascs_server_update(struct bt_conn *conn,
				struct bt_ascs_server *srv,
				struct bt_ascs_server_ase *srv_ase);

int bt_ascs_server_release(struct bt_conn *conn,
				struct bt_ascs_server *srv,
				struct bt_ascs_server_ase *srv_ase);

int bt_ascs_server_released(struct bt_conn *conn,
				struct bt_ascs_server *srv,
				struct bt_ascs_server_ase *srv_ase);

void bt_ascs_server_cb_register(struct bt_ascs_server_cb *cb);

int bt_ascs_server_disconnect(struct bt_conn *conn, struct bt_ascs_server *srv);

struct bt_ascs_client;

struct bt_ascs_client_ase {
	uint16_t value_handle;
	struct bt_gatt_subscribe_params sub_params;

	struct bt_ascs_client *cli;

	struct bt_iso_chan *chan;

	sys_snode_t node;

	/*
	 * To distinguish if the ASE notification is initiated by a server or
	 * is a response to the client control operation.
	 */
	uint8_t opcode;

	struct bt_ascs_ase ase;
};

struct bt_ascs_client {
	uint16_t start_handle;
	uint16_t end_handle;

	uint16_t control_handle;
	uint8_t busy;	/* write is in process */
	uint8_t opcode;	/* the opcode is in process */

	struct bt_gatt_subscribe_params ctrl_sub_params;

	sys_slist_t sink_ase;	/* list of bt_ascs_client_ase */
	sys_slist_t source_ase;	/* list of bt_ascs_client_ase */

	/* The cli_ase that initiates the reading */
	struct bt_ascs_client_ase *read_ase;

	struct bt_gatt_read_params read_params;
	struct bt_gatt_discover_params discover_params;

	struct bt_uuid_16 uuid;

	uint8_t buf[BT_ASCS_CLIENT_BUF_SIZE];
};

typedef void (*bt_ascs_discover_cb)(struct bt_conn *conn,
				struct bt_ascs_client *cli, int err);

typedef struct bt_ascs_client_ase *(*bt_ascs_client_alloc_ase_cb)(void);

typedef void (*bt_ascs_client_control_cb)(struct bt_conn *conn,
				struct bt_ascs_client *cli, uint8_t opcode,
				uint8_t ase_id, uint8_t rsp, uint8_t reason);

typedef void (*bt_ascs_client_notify_cb)(struct bt_conn *conn,
				struct bt_ascs_client_ase *cli_ase,
				uint8_t old_state, bool server_initiate);

typedef void (*bt_ascs_client_read_cb)(struct bt_conn *conn,
				struct bt_ascs_client *cli,
				struct bt_ascs_client_ase *cli_ase, int err);

struct bt_ascs_client_cb {
	bt_ascs_discover_cb discover;
	bt_ascs_client_alloc_ase_cb alloc;
	bt_ascs_client_control_cb control;
	bt_ascs_client_notify_cb notify; 
	bt_ascs_client_read_cb read;
};

int bt_ascs_client_read_ase(struct bt_conn *conn,
				struct bt_ascs_client *cli,
				struct bt_ascs_client_ase *cli_ase);

int bt_ascs_client_config_codec(struct bt_conn *conn,
				struct bt_ascs_client *cli, uint8_t num_of_ase,
				struct bt_ascs_client_ase **cli_ases);

int bt_ascs_client_config_qos(struct bt_conn *conn,
				struct bt_ascs_client *cli, uint8_t num_of_ase,
				struct bt_ascs_client_ase **cli_ases);

int bt_ascs_client_enable(struct bt_conn *conn,
				struct bt_ascs_client *cli, uint8_t num_of_ase,
				struct bt_ascs_client_ase **cli_ases);

int bt_ascs_client_disable(struct bt_conn *conn,
				struct bt_ascs_client *cli, uint8_t num_of_ase,
				struct bt_ascs_client_ase **cli_ases);

int bt_ascs_client_recv_start(struct bt_conn *conn,
				struct bt_ascs_client *cli, uint8_t num_of_ase,
				struct bt_ascs_client_ase **cli_ases);

int bt_ascs_client_recv_stop(struct bt_conn *conn,
				struct bt_ascs_client *cli, uint8_t num_of_ase,
				struct bt_ascs_client_ase **cli_ases);

int bt_ascs_client_update(struct bt_conn *conn,
				struct bt_ascs_client *cli, uint8_t num_of_ase,
				struct bt_ascs_client_ase **cli_ases);

int bt_ascs_client_release(struct bt_conn *conn,
				struct bt_ascs_client *cli, uint8_t num_of_ase,
				struct bt_ascs_client_ase **cli_ases);

int bt_ascs_discover(struct bt_conn *conn, struct bt_ascs_client *cli);

void bt_ascs_client_cb_register(struct bt_ascs_client_cb *cb);

int bt_ascs_client_disconnect(struct bt_conn *conn, struct bt_ascs_client *cli);

/* For pts test */
void bt_ascs_cli_pts_set_ccid_count(uint8_t ccid_cnt_val);


#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_ASCS_H_ */
