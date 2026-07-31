/** @file
 *  @brief Service Discovery Protocol handling.
 */

/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr.h>
#include <errno.h>
#include <atomic.h>
#include <misc/byteorder.h>
#include <misc/util.h>
#include <misc/printk.h>
#include <acts_bluetooth/bluetooth.h>
#include <acts_bluetooth/sdp.h>
#include "common_internal.h"

#define SDP_ATT_FULL_VERSION	1

static const struct bt_sdp_attribute a2dp_sink_attrs[] = {
	BT_SDP_NEW_RECORD_HANDLE,
	BT_SDP_LIST(
		BT_SDP_ATTR_SVCLASS_ID_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 3),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
			BT_SDP_ARRAY_16_CONST(BT_SDP_AUDIO_SINK_SVCLASS)
		},
		)
	),
	BT_SDP_LIST(
		BT_SDP_ATTR_PROTO_DESC_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 16),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 6),
			BT_SDP_DATA_ELEM_LIST_CONST(
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
				BT_SDP_ARRAY_16_CONST(BT_SDP_PROTO_L2CAP)
			},
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
				BT_SDP_ARRAY_16_CONST(0x0019)		/* fsm */
			},
			)
		},
		{
			BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 6),
			BT_SDP_DATA_ELEM_LIST_CONST(
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
				BT_SDP_ARRAY_16_CONST(BT_SDP_PROTO_AVDTP)
			},
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
				BT_SDP_ARRAY_16_CONST(0x0103)		/* AVDTP Version 1.3 */
			},
			)
		},
		)
	),
#if SDP_ATT_FULL_VERSION
	BT_SDP_LIST(
		BT_SDP_ATTR_BROWSE_GRP_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 3),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
			BT_SDP_ARRAY_16_CONST(BT_SDP_PUBLIC_BROWSE_GROUP)
		},
		)
	),
	BT_SDP_LIST(
		BT_SDP_ATTR_LANG_BASE_ATTR_ID_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 9),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
			BT_SDP_ARRAY_8_CONST('n', 'e')
		},
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
			BT_SDP_ARRAY_16_CONST(106)
		},
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
			BT_SDP_ARRAY_16_CONST(BT_SDP_PRIMARY_LANG_BASE)
		},
		)
	),
	{
		BT_SDP_ATTR_SERVICE_AVAILABILITY,
		{ BT_SDP_TYPE_SIZE(BT_SDP_UINT8), BT_SDP_ARRAY_8_CONST(0xFF) }
	},
#endif
	BT_SDP_LIST(
		BT_SDP_ATTR_PROFILE_DESC_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 8),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 6),
			BT_SDP_DATA_ELEM_LIST_CONST(
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
				BT_SDP_ARRAY_16_CONST(BT_SDP_ADVANCED_AUDIO_SVCLASS)
			},
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
				BT_SDP_ARRAY_16_CONST(0x0104)		/* Version 1.4 */
			},
			)
		},
		)
	),
#if SDP_ATT_FULL_VERSION
	BT_SDP_SERVICE_NAME("A2DP Sink"),
#endif
	{
		BT_SDP_ATTR_SUPPORTED_FEATURES,
		{ BT_SDP_TYPE_SIZE(BT_SDP_UINT16), BT_SDP_ARRAY_16_CONST(0x0001) }
	},
};

static struct bt_sdp_record a2dp_sink_rec = BT_SDP_RECORD(a2dp_sink_attrs);

static const struct bt_sdp_attribute avrcp_controller_attrs[] = {
	BT_SDP_NEW_RECORD_HANDLE,
	BT_SDP_LIST(
		BT_SDP_ATTR_SVCLASS_ID_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 6),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
			BT_SDP_ARRAY_16_CONST(BT_SDP_AV_REMOTE_SVCLASS)
		},
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
			BT_SDP_ARRAY_16_CONST(BT_SDP_AV_REMOTE_CONTROLLER_SVCLASS)
		},
		)
	),
	BT_SDP_LIST(
		BT_SDP_ATTR_PROTO_DESC_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 16),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 6),
			BT_SDP_DATA_ELEM_LIST_CONST(
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
				BT_SDP_ARRAY_16_CONST(BT_SDP_PROTO_L2CAP)
			},
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
				BT_SDP_ARRAY_16_CONST(0x0017)			/* AVCTP-CONTROLLER */
			},
			)
		},
		{
			BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 6),
			BT_SDP_DATA_ELEM_LIST_CONST(
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
				BT_SDP_ARRAY_16_CONST(BT_SDP_PROTO_AVCTP)
			},
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
				BT_SDP_ARRAY_16_CONST(0x0104)		/* AVCTP Version 1.4 */
			},
			)
		},
		)
	),
#if SDP_ATT_FULL_VERSION
	BT_SDP_LIST(
		BT_SDP_ATTR_BROWSE_GRP_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 3),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
			BT_SDP_ARRAY_16_CONST(BT_SDP_PUBLIC_BROWSE_GROUP)
		},
		)
	),
	BT_SDP_LIST(
		BT_SDP_ATTR_LANG_BASE_ATTR_ID_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 9),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
			BT_SDP_ARRAY_8_CONST('n', 'e')
		},
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
			BT_SDP_ARRAY_16_CONST(106)
		},
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
			BT_SDP_ARRAY_16_CONST(BT_SDP_PRIMARY_LANG_BASE)
		},
		)
	),
	{
		BT_SDP_ATTR_SERVICE_AVAILABILITY,
		{ BT_SDP_TYPE_SIZE(BT_SDP_UINT8), BT_SDP_ARRAY_8_CONST(0xFF) }
	},
#endif
	BT_SDP_LIST(
		BT_SDP_ATTR_PROFILE_DESC_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 8),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 6),
			BT_SDP_DATA_ELEM_LIST_CONST(
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
				BT_SDP_ARRAY_16_CONST(BT_SDP_AV_REMOTE_SVCLASS)
			},
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
				BT_SDP_ARRAY_16_CONST(0x0106)		/* Version 1.6 */
			},
			)
		},
		)
	),
#if SDP_ATT_FULL_VERSION
	BT_SDP_SERVICE_NAME("AVRCP Controller"),
#endif
	{
		BT_SDP_ATTR_SUPPORTED_FEATURES,
		{ BT_SDP_TYPE_SIZE(BT_SDP_UINT16), BT_SDP_ARRAY_16_CONST(0x0001) }
	},
};

static struct bt_sdp_record avrcp_controller_rec = BT_SDP_RECORD(avrcp_controller_attrs);

static const struct bt_sdp_attribute hfp_hf_attrs[] = {
	BT_SDP_NEW_RECORD_HANDLE,
	BT_SDP_LIST(
		BT_SDP_ATTR_SVCLASS_ID_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 6),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
			BT_SDP_ARRAY_16_CONST(BT_SDP_HANDSFREE_SVCLASS)
		},
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
			BT_SDP_ARRAY_16_CONST(BT_SDP_GENERIC_AUDIO_SVCLASS)
		},
		)
	),
	BT_SDP_LIST(
		BT_SDP_ATTR_PROTO_DESC_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 12),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 3),
			BT_SDP_DATA_ELEM_LIST_CONST(
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
				BT_SDP_ARRAY_16_CONST(BT_SDP_PROTO_L2CAP)
			},
			)
		},
		{
			BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 5),
			BT_SDP_DATA_ELEM_LIST_CONST(
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
				BT_SDP_ARRAY_16_CONST(BT_SDP_PROTO_RFCOMM)
			},
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UINT8),
				BT_SDP_ARRAY_8_CONST(0x01)		/* RFCOMM channel: 1(BT_RFCOMM_CHAN_HFP_HF), define in rfcomm.h */
			},
			)
		},
		)
	),
#if SDP_ATT_FULL_VERSION
	BT_SDP_LIST(
		BT_SDP_ATTR_BROWSE_GRP_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 3),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
			BT_SDP_ARRAY_16_CONST(BT_SDP_PUBLIC_BROWSE_GROUP)
		},
		)
	),
	BT_SDP_LIST(
		BT_SDP_ATTR_LANG_BASE_ATTR_ID_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 9),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
			BT_SDP_ARRAY_8_CONST('n', 'e')
		},
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
			BT_SDP_ARRAY_16_CONST(106)
		},
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
			BT_SDP_ARRAY_16_CONST(BT_SDP_PRIMARY_LANG_BASE)
		},
		)
	),
	{
		BT_SDP_ATTR_SERVICE_AVAILABILITY,
		{ BT_SDP_TYPE_SIZE(BT_SDP_UINT8), BT_SDP_ARRAY_8_CONST(0xFF) }
	},
#endif
	BT_SDP_LIST(
		BT_SDP_ATTR_PROFILE_DESC_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 8),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 6),
			BT_SDP_DATA_ELEM_LIST_CONST(
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
				BT_SDP_ARRAY_16_CONST(BT_SDP_HANDSFREE_SVCLASS)
			},
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
				BT_SDP_ARRAY_16_CONST(0x0109)		/* Version 1.9 */
			},
			)
		},
		)
	),
#if SDP_ATT_FULL_VERSION
	BT_SDP_SERVICE_NAME("HFP HF"),
#endif
	{
		BT_SDP_ATTR_SUPPORTED_FEATURES,
		{ BT_SDP_TYPE_SIZE(BT_SDP_UINT16), BT_SDP_ARRAY_16_CONST(0x003F) }
	},
};

static struct bt_sdp_record hfp_hf_rec = BT_SDP_RECORD(hfp_hf_attrs);

static const struct bt_sdp_attribute hfp_ag_attrs[] = {
	BT_SDP_NEW_RECORD_HANDLE,
	BT_SDP_LIST(
		BT_SDP_ATTR_SVCLASS_ID_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 6),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
			BT_SDP_ARRAY_16_CONST(BT_SDP_HANDSFREE_AGW_SVCLASS)
		},
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
			BT_SDP_ARRAY_16_CONST(BT_SDP_GENERIC_AUDIO_SVCLASS)
		},
		)
	),
	BT_SDP_LIST(
		BT_SDP_ATTR_PROTO_DESC_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 12),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 3),
			BT_SDP_DATA_ELEM_LIST_CONST(
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
				BT_SDP_ARRAY_16_CONST(BT_SDP_PROTO_L2CAP)
			},
			)
		},
		{
			BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 5),
			BT_SDP_DATA_ELEM_LIST_CONST(
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
				BT_SDP_ARRAY_16_CONST(BT_SDP_PROTO_RFCOMM)
			},
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UINT8),
				BT_SDP_ARRAY_8_CONST(0x02)		/* RFCOMM channel: 2(BT_RFCOMM_CHAN_HFP_AG), define in rfcomm.h */
			},
			)
		},
		)
	),
#if SDP_ATT_FULL_VERSION
	BT_SDP_LIST(
		BT_SDP_ATTR_BROWSE_GRP_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 3),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
			BT_SDP_ARRAY_16_CONST(BT_SDP_PUBLIC_BROWSE_GROUP)
		},
		)
	),
	BT_SDP_LIST(
		BT_SDP_ATTR_LANG_BASE_ATTR_ID_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 9),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
			BT_SDP_ARRAY_8_CONST('n', 'e')
		},
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
			BT_SDP_ARRAY_16_CONST(106)
		},
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
			BT_SDP_ARRAY_16_CONST(BT_SDP_PRIMARY_LANG_BASE)
		},
		)
	),
	{
		BT_SDP_ATTR_SERVICE_AVAILABILITY,
		{ BT_SDP_TYPE_SIZE(BT_SDP_UINT8), BT_SDP_ARRAY_8_CONST(0xFF) }
	},
#endif
	BT_SDP_LIST(
		BT_SDP_ATTR_PROFILE_DESC_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 8),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 6),
			BT_SDP_DATA_ELEM_LIST_CONST(
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
				BT_SDP_ARRAY_16_CONST(BT_SDP_HANDSFREE_SVCLASS)
			},
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
				BT_SDP_ARRAY_16_CONST(0x0109)		/* Version 1.9 */
			},
			)
		},
		)
	),
#if SDP_ATT_FULL_VERSION
	BT_SDP_SERVICE_NAME("HFP AG"),
#endif
	{
		BT_SDP_ATTR_NETWORK,
		{ BT_SDP_TYPE_SIZE(BT_SDP_UINT8), BT_SDP_ARRAY_8_CONST(0x01) }
	},
	{
		BT_SDP_ATTR_SUPPORTED_FEATURES,
		{ BT_SDP_TYPE_SIZE(BT_SDP_UINT16), BT_SDP_ARRAY_16_CONST(0x002F) }
	},
};

static struct bt_sdp_record hfp_ag_rec = BT_SDP_RECORD(hfp_ag_attrs);


static const struct bt_sdp_attribute a2dp_source_attrs[] = {
	BT_SDP_NEW_RECORD_HANDLE,
	BT_SDP_LIST(
		BT_SDP_ATTR_SVCLASS_ID_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 3),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
			BT_SDP_ARRAY_16_CONST(BT_SDP_AUDIO_SOURCE_SVCLASS)
		},
		)
	),
	BT_SDP_LIST(
		BT_SDP_ATTR_PROTO_DESC_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 16),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 6),
			BT_SDP_DATA_ELEM_LIST_CONST(
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
				BT_SDP_ARRAY_16_CONST(BT_SDP_PROTO_L2CAP)
			},
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
				BT_SDP_ARRAY_16_CONST(0x0019)		/* fsm */
			},
			)
		},
		{
			BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 6),
			BT_SDP_DATA_ELEM_LIST_CONST(
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
				BT_SDP_ARRAY_16_CONST(BT_SDP_PROTO_AVDTP)
			},
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
				BT_SDP_ARRAY_16_CONST(0x0104)		/* AVDTP Version 1.4 */
			},
			)
		},
		)
	),
#if SDP_ATT_FULL_VERSION
	BT_SDP_LIST(
		BT_SDP_ATTR_BROWSE_GRP_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 3),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
			BT_SDP_ARRAY_16_CONST(BT_SDP_PUBLIC_BROWSE_GROUP)
		},
		)
	),
	BT_SDP_LIST(
		BT_SDP_ATTR_LANG_BASE_ATTR_ID_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 9),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
			BT_SDP_ARRAY_8_CONST('n', 'e')
		},
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
			BT_SDP_ARRAY_16_CONST(106)
		},
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
			BT_SDP_ARRAY_16_CONST(BT_SDP_PRIMARY_LANG_BASE)
		},
		)
	),
	{
		BT_SDP_ATTR_SERVICE_AVAILABILITY,
		{ BT_SDP_TYPE_SIZE(BT_SDP_UINT8), BT_SDP_ARRAY_8_CONST(0xFF) }
	},
#endif
	BT_SDP_LIST(
		BT_SDP_ATTR_PROFILE_DESC_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 8),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 6),
			BT_SDP_DATA_ELEM_LIST_CONST(
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
				BT_SDP_ARRAY_16_CONST(BT_SDP_ADVANCED_AUDIO_SVCLASS)
			},
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
				BT_SDP_ARRAY_16_CONST(0x0104)		/* Version 1.4 */
			},
			)
		},
		)
	),
#if SDP_ATT_FULL_VERSION
	BT_SDP_SERVICE_NAME("A2DP Source"),
#endif
	{
		BT_SDP_ATTR_SUPPORTED_FEATURES,
		{ BT_SDP_TYPE_SIZE(BT_SDP_UINT16), BT_SDP_ARRAY_16_CONST(0x0001) }
	},
};

static struct bt_sdp_record a2dp_source_rec = BT_SDP_RECORD(a2dp_source_attrs);

static const struct bt_sdp_attribute avrcp_target_attrs[] = {
	BT_SDP_NEW_RECORD_HANDLE,
	BT_SDP_LIST(
		BT_SDP_ATTR_SVCLASS_ID_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 3),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
			BT_SDP_ARRAY_16_CONST(BT_SDP_AV_REMOTE_TARGET_SVCLASS)
		},
		)
	),
	BT_SDP_LIST(
		BT_SDP_ATTR_PROTO_DESC_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 16),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 6),
			BT_SDP_DATA_ELEM_LIST_CONST(
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
				BT_SDP_ARRAY_16_CONST(BT_SDP_PROTO_L2CAP)
			},
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
				BT_SDP_ARRAY_16_CONST(0x0017)			/* AVCTP-CONTROLLER */
			},
			)
		},
		{
			BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 6),
			BT_SDP_DATA_ELEM_LIST_CONST(
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
				BT_SDP_ARRAY_16_CONST(BT_SDP_PROTO_AVCTP)
			},
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
				BT_SDP_ARRAY_16_CONST(0x0104)		/* AVCTP Version 1.4 */
			},
			)
		},
		)
	),
#if SDP_ATT_FULL_VERSION
	BT_SDP_LIST(
		BT_SDP_ATTR_BROWSE_GRP_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 3),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
			BT_SDP_ARRAY_16_CONST(BT_SDP_PUBLIC_BROWSE_GROUP)
		},
		)
	),
	BT_SDP_LIST(
		BT_SDP_ATTR_LANG_BASE_ATTR_ID_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 9),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
			BT_SDP_ARRAY_8_CONST('n', 'e')
		},
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
			BT_SDP_ARRAY_16_CONST(106)
		},
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
			BT_SDP_ARRAY_16_CONST(BT_SDP_PRIMARY_LANG_BASE)
		},
		)
	),
	{
		BT_SDP_ATTR_SERVICE_AVAILABILITY,
		{ BT_SDP_TYPE_SIZE(BT_SDP_UINT8), BT_SDP_ARRAY_8_CONST(0xFF) }
	},
#endif
	BT_SDP_LIST(
		BT_SDP_ATTR_PROFILE_DESC_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 8),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 6),
			BT_SDP_DATA_ELEM_LIST_CONST(
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
				BT_SDP_ARRAY_16_CONST(BT_SDP_AV_REMOTE_SVCLASS)
			},
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
				BT_SDP_ARRAY_16_CONST(0x0106)		/* Version 1.6 */
			},
			)
		},
		)
	),
#if SDP_ATT_FULL_VERSION
	BT_SDP_SERVICE_NAME("AVRCP target"),
#endif
	{
		BT_SDP_ATTR_SUPPORTED_FEATURES,
		{ BT_SDP_TYPE_SIZE(BT_SDP_UINT16), BT_SDP_ARRAY_16_CONST(0x0002) }
	},
};

static struct bt_sdp_record avrcp_target_rec = BT_SDP_RECORD(avrcp_target_attrs);

#ifdef CONFIG_BT_GATT_OVER_BR
static const struct bt_sdp_attribute gatt_att_attrs[] = {
	BT_SDP_NEW_RECORD_HANDLE,
	BT_SDP_LIST(
		BT_SDP_ATTR_SVCLASS_ID_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 3),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
			BT_SDP_ARRAY_16_CONST(BT_SDP_GENERIC_ATTRIB_SVCLASS)
		},
		)
	),
	BT_SDP_LIST(
		BT_SDP_ATTR_PROTO_DESC_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 19),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 6),
			BT_SDP_DATA_ELEM_LIST_CONST(
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
				BT_SDP_ARRAY_16_CONST(BT_SDP_PROTO_L2CAP)
			},
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
				BT_SDP_ARRAY_16_CONST(0x001f)			/* ATT */
			},
			)
		},
		{
			BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 9),
			BT_SDP_DATA_ELEM_LIST_CONST(
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
				BT_SDP_ARRAY_16_CONST(BT_SDP_PROTO_ATT)
			},
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
				BT_SDP_ARRAY_16_CONST(0x0001)		/* START */
			},
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
				BT_SDP_ARRAY_16_CONST(0xffff)		/* STOP */
			},
			)
		},
		)
	),
	BT_SDP_LIST(
		BT_SDP_ATTR_BROWSE_GRP_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 3),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
			BT_SDP_ARRAY_16_CONST(BT_SDP_PUBLIC_BROWSE_GROUP)
		},
		)
	),
};

#ifdef CONFIG_BT_SELF_APP
static struct bt_sdp_attribute gatt_att_service_attrs[] = {
	BT_SDP_NEW_RECORD_HANDLE,
	BT_SDP_LIST(
		BT_SDP_ATTR_SVCLASS_ID_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 17),
		BT_SDP_DATA_ELEM_LIST(
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UUID128),
#ifdef CONFIG_PROTOCOL_BASIC_SUPPORT
			BT_SDP_ARRAY_8((BOX_DEFAULT_COLOR_ID & 0xff), (BOX_PRODUCT_ID >> 8), (BOX_PRODUCT_ID & 0xff),
								0xff, 0x04, 0x2e, 0x74, 0x6e, 0x69, 0x6f, 0x70, 0x6c, 0x65, 0x63, 0x78, 0x65)
#else
			BT_SDP_ARRAY_8_CONST(
				0xd1, 0x9f, 0x1b, 0x1f, 0x80, 0xf2, 0xb2, 0x8e, 0xe8, 0x11, 0x9a, 0xf6, 0xf8,0x25,0x9a,0xe4)
#endif
		},
		)
	),
	BT_SDP_LIST(
		BT_SDP_ATTR_PROTO_DESC_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 19),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 6),
			BT_SDP_DATA_ELEM_LIST_CONST(
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
				BT_SDP_ARRAY_16_CONST(BT_SDP_PROTO_L2CAP)
			},
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
				BT_SDP_ARRAY_16_CONST(0x001f)			/* ATT */
			},
			)
		},
		{
			BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 9),
			BT_SDP_DATA_ELEM_LIST_CONST(
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
				BT_SDP_ARRAY_16_CONST(BT_SDP_PROTO_ATT)
			},
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
				BT_SDP_ARRAY_16_CONST(0x0001)		/* START */
			},
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
				BT_SDP_ARRAY_16_CONST(0xff00)		/* STOP */
			},
			)
		},
		)
	),
	BT_SDP_LIST(
		BT_SDP_ATTR_BROWSE_GRP_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 3),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
			BT_SDP_ARRAY_16_CONST(BT_SDP_PUBLIC_BROWSE_GROUP)
		},
		)
	),
};

static struct bt_sdp_record gatt_att_service_rec = BT_SDP_RECORD(gatt_att_service_attrs);
#endif

static struct bt_sdp_record gatt_att_rec = BT_SDP_RECORD(gatt_att_attrs);

void bt_register_gatt_att_sdp(void)
{
	memset(&gatt_att_rec, 0, sizeof(gatt_att_rec));
	gatt_att_rec.attrs = (struct bt_sdp_attribute *)gatt_att_attrs;
	gatt_att_rec.attr_count = ARRAY_SIZE(gatt_att_attrs);
	bt_sdp_register_service(&gatt_att_rec);

#ifdef CONFIG_BT_SELF_APP
	memset(&gatt_att_service_rec, 0, sizeof(gatt_att_service_rec));
	gatt_att_service_rec.attrs = (struct bt_sdp_attribute *)gatt_att_service_attrs;
	gatt_att_service_rec.attr_count = ARRAY_SIZE(gatt_att_service_attrs);
	bt_sdp_register_service(&gatt_att_service_rec);
#endif
}

void bt_sdp_modify_gatt_att_color_id(uint8_t color_id)
{
#ifdef CONFIG_BT_SELF_APP
	uint8_t *data_array;
	struct bt_sdp_data_elem *data_elem = NULL;

	for(int i=0; i<gatt_att_service_rec.attr_count; i++)
	{
		data_elem = (struct bt_sdp_data_elem *)gatt_att_service_rec.attrs[i].val.data;
		if ((gatt_att_service_rec.attrs[i].id == BT_SDP_ATTR_SVCLASS_ID_LIST) && (data_elem)) {
			data_array = (uint8_t *)data_elem->data;
			data_array[0] = color_id;
		}
	}
#endif
}
#endif //CONFIG_BT_GATT_OVER_BR

void bt_register_a2dp_sink_sdp(void)
{
	memset(&a2dp_sink_rec, 0, sizeof(a2dp_sink_rec));
	a2dp_sink_rec.attrs = (struct bt_sdp_attribute *)a2dp_sink_attrs;
	a2dp_sink_rec.attr_count = ARRAY_SIZE(a2dp_sink_attrs);
	bt_sdp_register_service(&a2dp_sink_rec);
}

void bt_register_a2dp_source_sdp(void)
{
	memset(&a2dp_source_rec, 0, sizeof(a2dp_source_rec));
	a2dp_source_rec.attrs = (struct bt_sdp_attribute *)a2dp_source_attrs;
	a2dp_source_rec.attr_count = ARRAY_SIZE(a2dp_source_attrs);
	bt_sdp_register_service(&a2dp_source_rec);
}

void bt_register_avrcp_ct_sdp(void)
{
	memset(&avrcp_controller_rec, 0, sizeof(avrcp_controller_rec));
	avrcp_controller_rec.attrs = (struct bt_sdp_attribute *)avrcp_controller_attrs;
	avrcp_controller_rec.attr_count = ARRAY_SIZE(avrcp_controller_attrs);
	bt_sdp_register_service(&avrcp_controller_rec);
}

void bt_register_avrcp_tg_sdp(void)
{
	memset(&avrcp_target_rec, 0, sizeof(avrcp_target_rec));
	avrcp_target_rec.attrs = (struct bt_sdp_attribute *)avrcp_target_attrs;
	avrcp_target_rec.attr_count = ARRAY_SIZE(avrcp_target_attrs);
	bt_sdp_register_service(&avrcp_target_rec);
}

void bt_register_hfp_hf_sdp(void)
{
	memset(&hfp_hf_rec, 0, sizeof(hfp_hf_rec));
	hfp_hf_rec.attrs = (struct bt_sdp_attribute *)hfp_hf_attrs;
	hfp_hf_rec.attr_count = ARRAY_SIZE(hfp_hf_attrs);
	bt_sdp_register_service(&hfp_hf_rec);
}

void bt_register_hfp_ag_sdp(void)
{
	memset(&hfp_ag_rec, 0, sizeof(hfp_ag_rec));
	hfp_ag_rec.attrs = (struct bt_sdp_attribute *)hfp_ag_attrs;
	hfp_ag_rec.attr_count = ARRAY_SIZE(hfp_ag_attrs);
	bt_sdp_register_service(&hfp_ag_rec);
}
