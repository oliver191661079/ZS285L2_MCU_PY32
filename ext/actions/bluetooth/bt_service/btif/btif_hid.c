/*
 * Copyright (c) 2016 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt hid interface
 */

#define SYS_LOG_DOMAIN "btif_hid"
#include "btsrv_os_common.h"
#include "btsrv_inner.h"

int btif_hid_register_processer(void)
{
	return btsrv_register_msg_processer(MSG_BTSRV_HID, &btsrv_hid_process);
}

int btif_hid_register_sdp(struct bt_sdp_attribute * hid_attrs,uint8_t attrs_size)
{
	return btsrv_event_notify_ext(MSG_BTSRV_HID, MSG_BTSRV_HID_REGISTER, hid_attrs,attrs_size);
}

int btif_hid_send_ctrl_data(uint8_t report_type,uint8_t *data, uint32_t len)
{
	int data_len = len + sizeof(struct bt_hid_report);
	uint8_t *addr = mem_malloc(data_len);
	if(addr == NULL)
		return -ENOMEM;
	struct bt_hid_report *report = (struct bt_hid_report*)addr;
	report->report_type = report_type;
	report->has_size = 0;
	report->data = addr + sizeof(struct bt_hid_report);
	report->len = len;
	memcpy(report->data,data,len);
	int ret = btsrv_function_call_malloc(MSG_BTSRV_HID,MSG_BTSRV_HID_SEND_CTRL_DATA,addr,data_len,0);
	mem_free(addr);
	return ret;
}

int btif_hid_send_intr_data(uint8_t report_type,uint8_t *data, uint32_t len)
{
	int data_len = len + sizeof(struct bt_hid_report);
	uint8_t *addr = mem_malloc(data_len);
	if(addr == NULL)
		return -ENOMEM;
	struct bt_hid_report *report = (struct bt_hid_report*)addr;
	report->report_type = report_type;
	report->has_size = 0;
	report->data = addr + sizeof(struct bt_hid_report);
	report->len = len;
	memcpy(report->data,data,len);
	int ret = btsrv_function_call_malloc(MSG_BTSRV_HID,MSG_BTSRV_HID_SEND_INTR_DATA,addr,data_len,0);
	mem_free(addr);
	return ret;
}

int btif_hid_send_rsp(uint8_t status)
{
	return btsrv_event_notify_ext(MSG_BTSRV_HID, MSG_BTSRV_HID_SEND_RSP, NULL,status);
}

int btif_hid_start(btsrv_hid_callback cb)
{
	return btsrv_function_call(MSG_BTSRV_HID, MSG_BTSRV_HID_START, cb);
}

int btif_hid_stop(void)
{
	return btsrv_function_call(MSG_BTSRV_HID, MSG_BTSRV_HID_STOP, NULL);
}
