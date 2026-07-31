/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt manager spp profile.
 */
#define SYS_LOG_DOMAIN "bt manager"

#include <os_common_api.h>

#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stream.h>
#include <sys_event.h>
#include <bt_manager.h>
#include "bt_manager_inner.h"
#include "btservice_api.h"

#define HID_MAX_REPORT     (3)

struct report_info{
	uint8_t report_id;
	uint8_t type;
	uint16_t max_len;
	uint8_t last_report[64];
	uint16_t data_len;
};

struct btmgr_hid_info {
	uint8_t			sub_cls;			/* HID device subclass */
	struct report_info info[HID_MAX_REPORT];
};

static struct btmgr_hid_info mgr_hid_info;

static const uint8_t hid_descriptor[] =
{
    0x05, 0x01, //USAGE_PAGE (Generic Desktop Controls)
    0x09, 0x06, //USAGE (Keyboard)
    0xa1, 0x01, //COLLECTION (Application (mouse, keyboard))
    0x85, 0x01, //REPORT_ID (1)
    0x75, 0x01, //report size(1)
    0x95, 0x08, //report count(8)
    0x05, 0x07, //usage page(Keyboard/Keypad )
    0x19, 0xe0, //Usage Minimum
    0x29, 0xe7, //Usage Maximum
    0x15, 0x00, //Logical Minimum
    0x25, 0x01, //Logical Maxiimum
    0x81, 0x02, //Input()

    0x75, 0x08, //report size()
    0x95, 0x01, //report count()
    0x91, 0x03, //Output

    0x95, 0x03, //Report Count
    0x75, 0x08, //report size
    0x15, 0x00, //Logical Minimum
    0x26, 0xff, 0x00, //Logical Maxiimum
    0x05, 0x07, //usage page(Keyboard/Keypad )
    0x19, 0x00, //Usage Minimum
    0x29, 0xff, //usage Maximum
    0x81, 0x00, //input()

    0xc0,       //END_COLLECTION
    
    0x05, 0x0c, // USAGE_PAGE (Consumer)
    0x09, 0x01, //USAGE (Consumer control)
    0xa1, 0x01, //COLLECTION (Application (mouse, keyboard))
    0x85, 0x02, //REPORT_ID (2)
    0x15, 0x00, //Logical Minimum
    0x25, 0x01, //Logical Maximum
    0x75, 0x01, //Report size(1)
    0x95, 0x08, //Report Count(8)

    0x09, 0xea, //USAGE (volume down)
    0x09, 0xe9, //USAGE (volume up)
    0x09, 0xe2, //USAGE (mute)
    0x09, 0xcd, //USAGE (play/pause)
    0x09, 0xb6, //USAGE (scan previous track)
    0x09, 0xb5, //USAGE (scan next track)
    0x09, 0x83, //USAGE (fast forward)
    0x09, 0xb4, //USAGE (rewind)
    0x81, 0x02, //input(data, variable, absolute)
    0xc0,       //END_COLLECTION
};

static struct bt_sdp_attribute hid_dev_attrs[] = {
	BT_SDP_NEW_RECORD_HANDLE,
	BT_SDP_LIST(
		BT_SDP_ATTR_SVCLASS_ID_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 3),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
			BT_SDP_ARRAY_16_CONST(BT_SDP_HID_SVCLASS)
		},
		)
	),
	BT_SDP_LIST(
		BT_SDP_ATTR_PROTO_DESC_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 13),
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
				BT_SDP_ARRAY_16_CONST(0x0011)			/* HID-CONTROL */
			},
			)
		},
		{
			BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 3),
			BT_SDP_DATA_ELEM_LIST_CONST(
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
				BT_SDP_ARRAY_16_CONST(0x0011)
			},
			)
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
	BT_SDP_LIST(
		BT_SDP_ATTR_PROFILE_DESC_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 8),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 6),
			BT_SDP_DATA_ELEM_LIST_CONST(
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
				BT_SDP_ARRAY_16_CONST(BT_SDP_HID_SVCLASS)
			},
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
				BT_SDP_ARRAY_16_CONST(0x0101)		/* Version 1.1 */
			},
			)
		},
		)
	),
	BT_SDP_LIST(
		BT_SDP_ATTR_ADD_PROTO_DESC_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 15),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 13),
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
					BT_SDP_ARRAY_16_CONST(0x0013)  /* HID-INTERRUPT */
				},
				)
				
			},
			{
				BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 3),
				BT_SDP_DATA_ELEM_LIST_CONST(
				{
					BT_SDP_TYPE_SIZE(BT_SDP_UUID16),
					BT_SDP_ARRAY_16_CONST(0x0011)
				},
				)
			},
			)
		},
		)
	),
	BT_SDP_SERVICE_NAME("HID CONTROL"),
	{
		BT_SDP_ATTR_HID_PARSER_VERSION,
		{ BT_SDP_TYPE_SIZE(BT_SDP_UINT16), BT_SDP_ARRAY_16_CONST(0x0111) }
	},
	{
		BT_SDP_ATTR_HID_DEVICE_SUBCLASS,
		{ BT_SDP_TYPE_SIZE(BT_SDP_UINT8), &mgr_hid_info.sub_cls }
	},
	{
		BT_SDP_ATTR_HID_COUNTRY_CODE,
		{ BT_SDP_TYPE_SIZE(BT_SDP_UINT8), BT_SDP_ARRAY_16_CONST(0x21) }
	},
	{
		BT_SDP_ATTR_HID_VIRTUAL_CABLE,
		{ BT_SDP_TYPE_SIZE(BT_SDP_BOOL), BT_SDP_ARRAY_8_CONST(0x1) }
	},
	{
		BT_SDP_ATTR_HID_RECONNECT_INITIATE,
		{ BT_SDP_TYPE_SIZE(BT_SDP_BOOL), BT_SDP_ARRAY_8_CONST(0x1) }
	},
	BT_SDP_LIST(
		BT_SDP_ATTR_HID_DESCRIPTOR_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ16, sizeof(hid_descriptor) + 8),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ16, sizeof(hid_descriptor) + 5),
			BT_SDP_DATA_ELEM_LIST_CONST(
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UINT8),
				BT_SDP_ARRAY_8_CONST(0x22),
			},
			{
				BT_SDP_TYPE_SIZE_VAR(BT_SDP_TEXT_STR16,sizeof(hid_descriptor)),
				hid_descriptor,
			},
			),
		},
		)
	),
	BT_SDP_LIST(
		BT_SDP_ATTR_HID_LANG_ID_BASE_LIST,
		BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 8),
		BT_SDP_DATA_ELEM_LIST_CONST(
		{
			BT_SDP_TYPE_SIZE_VAR(BT_SDP_SEQ8, 6),
			BT_SDP_DATA_ELEM_LIST_CONST(
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
				BT_SDP_ARRAY_16_CONST(0x409),
			},
			{
				BT_SDP_TYPE_SIZE(BT_SDP_UINT16),
				BT_SDP_ARRAY_16_CONST(0x100),
			},
			),
		},
		)
	),
	{
		BT_SDP_ATTR_HID_BOOT_DEVICE,
		{ BT_SDP_TYPE_SIZE(BT_SDP_BOOL), BT_SDP_ARRAY_8_CONST(0x0) }
	},
	{
		BT_SDP_ATTR_HID_SUPERVISION_TIMEOUT,
		{ BT_SDP_TYPE_SIZE(BT_SDP_UINT16), BT_SDP_ARRAY_16_CONST(5000) }
	},
	{
		BT_SDP_ATTR_HID_MAX_LATENCY,
		{ BT_SDP_TYPE_SIZE(BT_SDP_UINT16), BT_SDP_ARRAY_16_CONST(240) }
	},
	{
		BT_SDP_ATTR_HID_MIN_LATENCY,
		{ BT_SDP_TYPE_SIZE(BT_SDP_UINT16), BT_SDP_ARRAY_16_CONST(0x0) }
	},
};

static struct report_info *bt_manager_hid_find(int report_id ,int type)
{
	int i;
	struct report_info *info = NULL;

	for (i = 0; i < HID_MAX_REPORT; i++) {
		if(mgr_hid_info.info[i].report_id == report_id
			&& mgr_hid_info.info[i].type == type){
			info = &mgr_hid_info.info[i];
			break;
		}
	}

	return info;
}

int bt_manager_hid_register_sdp()
{
	memset(&mgr_hid_info,0,sizeof(struct btmgr_hid_info));
	mgr_hid_info.sub_cls = 0xc0;
	mgr_hid_info.info[0].report_id = 1;
	mgr_hid_info.info[0].type = BTSRV_HID_REP_TYPE_INPUT;
	mgr_hid_info.info[0].data_len = 5;
	mgr_hid_info.info[0].last_report[0]= mgr_hid_info.info[0].report_id;

	mgr_hid_info.info[1].report_id = 1;
	mgr_hid_info.info[1].type = BTSRV_HID_REP_TYPE_OUTPUT;
	mgr_hid_info.info[1].data_len = 2;
	mgr_hid_info.info[1].last_report[0]= mgr_hid_info.info[1].report_id;

	mgr_hid_info.info[2].report_id = 2;
	mgr_hid_info.info[2].type = BTSRV_HID_REP_TYPE_INPUT;
	mgr_hid_info.info[2].data_len = 2;
	mgr_hid_info.info[2].last_report[0]= mgr_hid_info.info[2].report_id;
	int ret = btif_hid_register_sdp(hid_dev_attrs,ARRAY_SIZE(hid_dev_attrs));
	if (ret) {
		SYS_LOG_INF("Failed %d\n", ret);
	}

	return ret;
}


int bt_manager_hid_send_ctrl_data(uint8_t report_type, uint8_t *data, uint32_t len)
{
	return btif_hid_send_ctrl_data(report_type, data, len);
}

int bt_manager_hid_send_intr_data(uint8_t report_type, uint8_t *data, uint32_t len)
{
	uint8_t report_id = data[0];
	struct report_info * info = bt_manager_hid_find(report_id,report_type);
	if(info){
		if(len != info->data_len){
			SYS_LOG_INF("report format error %d != %d",len,info->data_len);
			return -EINVAL;
		}
		memcpy(info->last_report,data,len);
	}else{
		return -EINVAL;
	}
	return btif_hid_send_intr_data(report_type, data, len);
}

static void bt_manager_hid_callback(btsrv_hid_event_e event, void *packet, int size)
{
	struct bt_hid_report* report;
	struct report_info * info;
	int report_id;

	switch(event){
		case BTSRV_HID_GET_REPORT:
			SYS_LOG_INF("GET_REPORT");
			report = (struct bt_hid_report*)packet;
			report_id = report->data[0];
			info = bt_manager_hid_find(report_id,report->report_type);
			//report id in descriptor
			if(!info){
				btif_hid_send_rsp(BTSRV_HID_HANDSHAKE_RSP_ERR_INVALID_REP_ID);
				break;
			}
			if(report->has_size){	
				info->max_len = *(uint16_t*)&(report->data[1]);
			}
			bt_manager_hid_send_ctrl_data(info->type,info->last_report,info->data_len);
		break;
		case BTSRV_HID_SET_REPORT:
			SYS_LOG_INF("SET_REPORT");
			report = (struct bt_hid_report*)packet;
			report_id = report->data[0];
			info = bt_manager_hid_find(report_id,report->report_type);
			if(info){
				memcpy(info->last_report,report->data,report->len);
			}else{
				btif_hid_send_rsp(BTSRV_HID_HANDSHAKE_RSP_ERR_INVALID_REP_ID);
				break;
			}
			btif_hid_send_rsp(BTSRV_HID_HANDSHAKE_RSP_SUCCESS);
		break;
		case BTSRV_HID_GET_PROTOCOL:
			SYS_LOG_INF("GET_PROTOCOL");
			btif_hid_send_rsp(BTSRV_HID_HANDSHAKE_RSP_ERR_UNSUPPORTED_REQ);
		break;
		case BTSRV_HID_SET_PROTOCOL:
			SYS_LOG_INF("SET_PROTOCOL");
			btif_hid_send_rsp(BTSRV_HID_HANDSHAKE_RSP_ERR_UNSUPPORTED_REQ);
		break;
		case BTSRV_HID_INTR_DATA:
			SYS_LOG_INF("DATA");
		break;
		case BTSRV_HID_UNPLUG:
			SYS_LOG_INF("UNPLUG");
		break;
		case BTSRV_HID_SUSPEND:
			SYS_LOG_INF("SUSPEND");
		break;
		case BTSRV_HID_EXIT_SUSPEND:
			SYS_LOG_INF("EXIT SUSPEND");
		break;
		default:
		break;
	}
}

int bt_manager_hid_take_photo(void){
	uint8_t tmp_data[2];
	SYS_LOG_INF("");

    memset(tmp_data, 0, sizeof(tmp_data));
    /*
     report id=2
     */
    tmp_data[0] = 2;
    /*
     0x09, 0xe9, //USAGE (volume up)
     */
    tmp_data[1] = 0x01<<1;
    bt_manager_hid_send_intr_data(BTSRV_HID_REP_TYPE_INPUT,tmp_data, 2);
    /*
     release
     */
    tmp_data[1] = 0x00;
    bt_manager_hid_send_intr_data(BTSRV_HID_REP_TYPE_INPUT,tmp_data, 2);
	return 0;
}

int bt_manager_hid_profile_start(void)
{
	return btif_hid_start(&bt_manager_hid_callback);
}

int bt_manager_hid_profile_stop(void)
{
	return btif_hid_stop();
}

