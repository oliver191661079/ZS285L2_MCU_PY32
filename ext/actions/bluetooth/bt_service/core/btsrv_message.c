/*
 * Copyright (c) 2016 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt service notify interface
 */
#define SYS_LOG_DOMAIN "btsrv_msg"
#include "btsrv_os_common.h"
#include "btsrv_inner.h"

static void msg_free_memory(struct app_msg *msg, int result, void *param)
{
	if (msg->ptr) {
		mem_free(msg->ptr);
	}
}

int btsrv_function_call(int func_type, int cmd, void *param)
{
	struct app_msg msg = {0};

	msg.type = func_type;
	msg.cmd = cmd;
	msg.ptr = param;
	SYS_LOG_DBG("func_type %d, cmd %d\n", func_type, cmd);
	return !send_async_msg(BLUETOOTH_SERVICE_NAME, &msg);
}

int btsrv_event_notify(int event_type, int cmd, void *param)
{
	struct app_msg msg = {0};

	msg.type = event_type;
	msg.cmd = cmd;
	msg.ptr = param;
	SYS_LOG_DBG("event_type %d, cmd %d\n", event_type, cmd);
	return !send_async_msg(BLUETOOTH_SERVICE_NAME, &msg);
}

int btsrv_event_notify_value(int event_type, int cmd, int value)
{
	struct app_msg msg = {0};

	msg.type = event_type;
	msg.cmd = cmd;
	msg.value = value;
	SYS_LOG_DBG("event_type %d, cmd %d, value: %d\n", event_type, cmd, value);
	return !send_async_msg(BLUETOOTH_SERVICE_NAME, &msg);
}

int btsrv_event_notify_value_ext(int event_type, int cmd, int value, uint8_t code)
{
	struct app_msg msg = {0};

	msg.type = event_type;
	msg.cmd = cmd;
	msg.reserve = code;
	msg.value = value;
	SYS_LOG_DBG("event_type %d, cmd %d, value: %d, code: %d\n", event_type, cmd, value, code);
	return !send_async_msg(BLUETOOTH_SERVICE_NAME, &msg);
}

int btsrv_event_notify_ext(int event_type, int cmd, void *param, uint8_t code)
{
	struct app_msg msg = {0};

	msg.type = event_type;
	msg.cmd = cmd;
	msg.reserve = code;
	msg.ptr = param;
	SYS_LOG_DBG("event_type %d, cmd %d\n", event_type, cmd);
	return !send_async_msg(BLUETOOTH_SERVICE_NAME, &msg);
}

int btsrv_event_notify_malloc(int event_type, int cmd, uint8_t *data, uint16_t len, uint8_t code)
{
	int ret;
	uint8_t *addr;
	struct app_msg msg = {0};

	/*malloc add 1 bytes reserve for string \0*/
	addr = mem_malloc(len + 1);
	if (addr == NULL) {
		return -ENOMEM;
	}
	memset(addr, 0, len + 1);
	if (data) {
		memcpy(addr, data, len);
	}

	msg.type = event_type;
	msg.cmd = cmd;
	msg.reserve = code;
	msg.ptr = addr;
	msg.callback = &msg_free_memory;
	SYS_LOG_DBG("event_type %d, cmd %d\n", event_type, cmd);

	if (send_async_msg(BLUETOOTH_SERVICE_NAME, &msg)) {
		ret = 0;
	} else {
		mem_free(addr);
		ret = -EIO;
	}

	return ret;
}

/* paired with btsrv_event_notify_cb() */
void *btsrv_event_buf_malloc(uint16_t len)
{
	/* malloc add 1 bytes reserve for string \0 */
	return mem_malloc(len + 1);
}

/* paired with btsrv_event_buf_malloc() */
int btsrv_event_notify_buf(int event_type, int cmd, uint8_t *data, uint8_t code)
{
	int ret;
	struct app_msg msg = {0};

	msg.type = event_type;
	msg.cmd = cmd;
	msg.reserve = code;
	msg.ptr = data;
	msg.callback = &msg_free_memory;
	SYS_LOG_DBG("event_type %d, cmd %d\n", event_type, cmd);

	if (send_async_msg(BLUETOOTH_SERVICE_NAME, &msg)) {
		ret = 0;
	} else {
		mem_free(data);
		ret = -EIO;
	}

	return ret;
}
