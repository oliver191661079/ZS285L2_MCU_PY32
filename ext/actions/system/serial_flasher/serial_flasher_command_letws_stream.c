/*
 * Copyright (c) 2022 Actions Semiconductor Co, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define SYS_LOG_LEVEL 3
#define SYS_LOG_DOMAIN "letws"
#include <logging/sys_log.h>

#include <kernel.h>
#include <string.h>
#include <stream.h>
#include <msg_manager.h>
#include <sys_manager.h>
#include <mem_manager.h>
#include <letws_ota_stream.h>
#include <crc.h>
#include <misc/byteorder.h>
#include <thread_timer.h>
#include "serial_flasher_inner.h"
#ifdef CONFIG_WATCHDOG
#include <watchdog_hal.h>
#endif

#ifdef CONFIG_TASK_WDT
#include <debug/task_wdt.h>
#endif

#define OTA_HOST_VERSION "1.00"

/* support check data checksum in each ota unit */
#define CONFIG_OTA_BT_SUPPORT_UNIT_CRC
#define TLV_TYPE_OTA_SUPPORT_FEATURES		0x09
#define OTA_SUPPORT_FEATURE_UNIT_DATA_CRC	(1 << 0)
#define OTA_ERROR_CODE_SUCCESS		100000
#define OTA_ERROR_CODE_CRC_ERROR    100015

#define SERVICE_ID_OTA		0x9

#define OTA_UNIT_SIZE			(OTA_READ_DATA_SIZE + 4 + sizeof(struct serial_flasher_head))

#define TLV_MAX_DATA_LENGTH	0x3fff
#define TLV_TYPE_ERROR_CODE	0x7f
#define TLV_TYPE_MAIN		0x80
#define TLV_TYPE_CRC        0x81

#define OTA_CMD_H2D_REQUEST_UPGRADE		0x01
#define OTA_CMD_H2D_CONNECT_NEGOTIATION		0x02
#define OTA_CMD_D2H_REQUIRE_IMAGE_DATA		0x03
#define OTA_CMD_H2D_SEND_IMAGE_DATA		0x04
#define OTA_CMD_D2H_REPORT_RECEVIED_DATA_COUNT	0x05
#define OTA_CMD_D2H_VALIDATE_IMAGE		0x06
#define OTA_CMD_D2H_REPORT_STATUS		0x07
#define OTA_CMD_D2H_CANCEL_UPGRADE		0x08
#define OTA_CMD_H2D_NEGOTIATION_RESULT		0x09
#define OTA_CMD_D2H_REQUEST_UPGRADE		0x0A
#define OTA_CMD_H2D_SEND_IMAGE_DATA_WITH_CRC	0x0B
#define OTA_CMD_D2H_REQUST_CANCEL_UPGRADE 0x10
#define TLV_PACK_U8(buf, type, value)	tlv_pack_data(buf, type, sizeof(uint8_t), value)
#define TLV_PACK_U16(buf, type, value)	tlv_pack_data(buf, type, sizeof(uint16_t), value)
#define TLV_PACK_U32(buf, type, value)	tlv_pack_data(buf, type, sizeof(uint32_t), value)

enum serial_flasher_state{
	PROT_STATE_IDLE,
	PROT_STATE_DATA,
};

struct serial_flasher_head {
	uint8_t svc_id;
	uint8_t cmd;
	uint8_t param_type;
	uint16_t param_len;
	uint32_t param_crc;
	uint8_t payload[0];
} __attribute__((packed));

struct serial_flasher_cmd_request{
    uint8_t cmd;
    uint8_t *buf;
    uint16_t size;
    uint16_t recv_size;
    uint8_t *recv_buf;
    uint8_t with_resp;
	int32_t retry_times;
};

struct tlv_head {
	uint8_t type;
	uint16_t len;
	uint8_t value[0];
} __attribute__((packed));

struct tlv_data {
	uint8_t type;
	uint16_t len;
	uint16_t max_len;
	uint8_t *value_ptr;
};

typedef int (*serial_flasher_cmd_handler_t)(struct serial_flasher*ctx, uint32_t param_len);

struct serial_flasher_cmd {
	uint8_t cmd;
	serial_flasher_cmd_handler_t handler;
};


static int serial_flasher_get_rx_data(struct serial_flasher*ctx, void *buf, int size)
{
	int read_size;
    char *pbuf = (char*)buf;

	while (size > 0) {
		read_size = stream_read(ctx->letws_stream, pbuf, size);
		if (read_size <= 0) {
			return -EIO;
		}

		size -= read_size;
		pbuf += read_size;
	}

	return 0;
}


static uint8_t *tlv_pack(uint8_t *buf, const struct tlv_data *tlv)
{
	uint16_t len;

	len = tlv->len;

	*buf++ = tlv->type;
	*buf++ = len & 0xff;
	*buf++ = len >> 8;

	if (tlv->value_ptr) {
		memcpy(buf, tlv->value_ptr, len);
		buf += len;
	}

	return buf;
}

static uint8_t *tlv_pack_data(uint8_t *buf, uint8_t type, uint16_t len, uint32_t number)
{
	struct tlv_data tlv;

	tlv.type = type;
	tlv.len = len;
	tlv.value_ptr = (uint8_t *)&number;

	return tlv_pack(buf, &tlv);
}

static uint8_t *tlv_pack_array(uint8_t *buf, uint8_t type, void *array, uint32_t array_size)
{
	struct tlv_data tlv;
	tlv.type = type;
	tlv.len = array_size;
	tlv.value_ptr = (uint8_t *)array;

	return tlv_pack(buf, &tlv);
}

static int tlv_unpack_head(struct serial_flasher *ctx, struct tlv_data *tlv)
{
	int total_len;

	/* read type */
	tlv->type = *(ctx->read_ptr);
	ctx->read_ptr++;

	tlv->len = sys_get_le16(ctx->read_ptr);
	ctx->read_ptr += sizeof(tlv->len);

	total_len = sizeof(tlv->type) + sizeof(tlv->len);

	return total_len;
}

static int tlv_unpack_value(struct serial_flasher *ctx, uint8_t *buf, int size)
{
	if(buf){
		memcpy(buf, ctx->read_ptr, size);
	}
	ctx->read_ptr += size;

	return 0;
}

void serial_flasher_dump_data(char *str, const void *buf, int size)
{
	printk("%s\n", str);
	print_buffer(buf, 1, size, 16, -1);
}


int serial_flasher_send_data(struct serial_flasher *ctx, uint8_t *buf, int size)
{
	int err;

	err = stream_write(ctx->letws_stream, buf, size);
	if (err < 0) {
		SYS_LOG_ERR("failed to send data, size %d, err %d", size, err);
		return -EIO;
	}

	serial_flasher_dump_data("send cmd:", buf, 16);

	return 0;
}

static int serial_flasher_resp_is_nak_frame(struct serial_flasher *ctx, struct serial_flasher_head *resp)
{
	uint32_t param_size;
	int head_len;
	struct tlv_data tlv;
	int err_code;

	ctx->read_ptr = resp->payload;
	param_size = resp->param_len;

	while (param_size > 0) {
		head_len = tlv_unpack_head(ctx, &tlv);

		switch (tlv.type) {
		case TLV_TYPE_ERROR_CODE:
			tlv_unpack_value(ctx, (uint8_t *)&err_code, 4);
			if(err_code != OTA_ERROR_CODE_SUCCESS){
				return true;
			}
			break;
		default:
			/* skip other parameters by now */
			tlv_unpack_value(ctx, NULL, tlv.len);
			break;
		}

		if(param_size >= (head_len + tlv.len)){
			param_size -= head_len + tlv.len;
		}else{
			param_size = 0;
		}
	}

	return false;

}

#if 0
static int serial_flasher_resp_is_ack_frame(struct serial_flasher *ctx, struct serial_flasher_head *resp)
{
	uint32_t param_size;
	int head_len;
	struct tlv_data tlv;
	int err_code;

	ctx->read_ptr = resp->payload;
	param_size = resp->param_len;

	while (param_size > 0) {
		head_len = tlv_unpack_head(ctx, &tlv);

		switch (tlv.type) {
		case TLV_TYPE_ERROR_CODE:
			tlv_unpack_value(ctx, (uint8_t *)&err_code, 4);
			if(err_code == OTA_ERROR_CODE_SUCCESS){
				return true;
			}
			break;
		default:
			/* skip other parameters by now */
			tlv_unpack_value(ctx, NULL, tlv.len);
			break;
		}

		if(param_size >= (head_len + tlv.len)){
			param_size -= head_len + tlv.len;
		}else{
			param_size = 0;
		}
	}

	return false;

}
#endif

static int serial_flasher_check_frame_valid(struct serial_flasher *ctx, struct serial_flasher_head *head)
{
	int err;
	uint8_t frame_len;
	uint32_t calc_crc;
	uint32_t recv_crc;

	os_sem_take(&ctx->read_sem, ctx->read_timeout);

	err = serial_flasher_get_rx_data(ctx, &frame_len, sizeof(frame_len));
	if (err) {
		SYS_LOG_ERR("cannot read head bytes");
		return -EIO;
	}

	if (frame_len > OTA_UNIT_SIZE){
		SYS_LOG_ERR("invalid framelen: %d", frame_len);
		return -EINVAL;
	}

	err = serial_flasher_get_rx_data(ctx, head, frame_len);
	if (err) {
		SYS_LOG_ERR("cannot read head bytes");
		return -EINVAL;
	}

	if (head->svc_id != SERVICE_ID_OTA) {
		SYS_LOG_ERR("invalid svc_id: %d", head->svc_id);
		return -EINVAL;
	}

	if (head->param_type != TLV_TYPE_MAIN) {
		SYS_LOG_ERR("invalid param type: %d", head->param_type);
		return -EINVAL;
	}

	if (head->param_len > OTA_UNIT_SIZE){
		SYS_LOG_ERR("invalid param len: %d", head->param_len);
		return -EINVAL;
	}

	recv_crc = head->param_crc;
	head->param_crc = 0;
	calc_crc = utils_crc32(0, (u8_t *)head, sizeof(struct serial_flasher_head) + head->param_len);

	if (recv_crc != calc_crc){
		SYS_LOG_ERR("recv crc %x cal crc %x\n", recv_crc, calc_crc);
		return -EINVAL;
	}
	head->param_crc = recv_crc;

	//printk("recv cmd %d len %d\n", head->cmd, sizeof(struct serial_flasher_head) + head->param_len);
	//print_buffer(head, 1, 16, 16, -1);

	return 0;

}

#if 0
static int serial_flasher_receive_ack_frame(struct serial_flasher *ctx, uint32_t cmd)
{
	int err;
	struct serial_flasher_head *resp;

	resp = (struct serial_flasher_head *)ctx->frame_buffer;
	err = serial_flasher_check_frame_valid(ctx, resp);
	if(err != 0){
		return -EIO;
	}else{
		if((resp->cmd == cmd) && serial_flasher_resp_is_ack_frame(ctx, resp)){
			return 0;
		}
	}

	return -EINVAL;
}
#endif

static int serial_flasher_send_command_reqeust(struct serial_flasher *ctx, struct serial_flasher_cmd_request *request)
{
	int err = 0;

	struct serial_flasher_head *head;
	struct serial_flasher_head *resp;

	uint32_t ota_cmd_state;
    enum {
        OTA_CMD_STATE_CMD,
        OTA_CMD_STATE_RESP,
        OTA_CMD_STATE_SUCCESS,
        OTA_CMD_STATE_FAIL,
    };

	head = (struct serial_flasher_head *)request->buf;
	head->svc_id = SERVICE_ID_OTA;
	head->cmd = request->cmd;
	head->param_type = TLV_TYPE_MAIN;
	head->param_crc = 0;
	head->param_len = request->size - sizeof(struct serial_flasher_head);

	//serial_flasher_dump_data("cmd buf", request->buf, request->size);
	//add crc value
	head->param_crc = utils_crc32(0, request->buf, request->size);

    ota_cmd_state = OTA_CMD_STATE_CMD;
	while(1){
		switch(ota_cmd_state){
			case OTA_CMD_STATE_CMD:
			err = serial_flasher_send_data(ctx, request->buf, request->size);
			if (err) {
				SYS_LOG_ERR("failed to send cmd %d, err %d", request->cmd, err);
				return err;
			}

			if (request->with_resp){
				ota_cmd_state = OTA_CMD_STATE_RESP;
			}else{
				ota_cmd_state = OTA_CMD_STATE_SUCCESS;
			}
			break;

			case OTA_CMD_STATE_RESP:
			resp = (struct serial_flasher_head *)ctx->frame_buffer;
			err = serial_flasher_check_frame_valid(ctx, resp);
			if(err != 0){
				//printk("recv resp err\n");
				if(request->retry_times <= 0){
                    ota_cmd_state = OTA_CMD_STATE_FAIL;
					err = -EIO;
                }else{
                    request->retry_times--;
                    ota_cmd_state = OTA_CMD_STATE_CMD;
					printk("resend cmd %d %d\n", request->cmd, request->retry_times);
                }
				continue;
			}else{
				if((resp->cmd != request->cmd) || serial_flasher_resp_is_nak_frame(ctx, resp)){
					if(request->retry_times <= 0){
	                    ota_cmd_state = OTA_CMD_STATE_FAIL;
						err = -EINVAL;
	                }else{
	                    request->retry_times--;
	                    ota_cmd_state = OTA_CMD_STATE_CMD;
						printk("resend cmd %d %d\n", request->cmd, request->retry_times);
	                }
				}else{
					request->recv_buf = resp->payload;
					request->recv_size = resp->param_len;
					ota_cmd_state = OTA_CMD_STATE_SUCCESS;
				}
			}
			break;

		}

		if(ota_cmd_state == OTA_CMD_STATE_FAIL
            || ota_cmd_state == OTA_CMD_STATE_SUCCESS){
            break;
        }

	}

	return err;
}

int serial_flasher_send_cmd(struct serial_flasher *ctx, uint8_t cmd,
		    uint8_t *buf, int size)
{
	struct serial_flasher_head *head;
	int err;

	head = (struct serial_flasher_head *)buf;
	head->svc_id = SERVICE_ID_OTA;
	head->cmd = cmd;
	head->param_type = TLV_TYPE_MAIN;
	head->param_crc = 0;
	head->param_len = size - sizeof(struct serial_flasher_head);
	//add crc value
	head->param_crc = utils_crc32(0, buf, size);

	//SYS_LOG_INF("send cmd: %d", cmd);
	//print_buffer(buf, 1, size, 16, buf);
	err = serial_flasher_send_data(ctx, buf, size);
	if (err) {
		SYS_LOG_ERR("failed to send cmd %d, err %d", cmd, err);
		return err;
	}

	return 0;
}


static int serial_flasher_send_ack_frame(struct serial_flasher *ctx, uint32_t cmd, uint32_t err_code)
{
	uint8_t *send_buf;
	uint32_t send_len;
	int err;

	/* init ack data */
	send_buf = ctx->send_buf + sizeof(struct serial_flasher_head);
	send_buf = TLV_PACK_U32(send_buf, TLV_TYPE_ERROR_CODE, err_code);

	send_len = send_buf - ctx->send_buf;

	err = serial_flasher_send_cmd(ctx, cmd, ctx->send_buf, send_len);
	if (err) {
		SYS_LOG_ERR("failed to send cmd %d, error %d",
			cmd, err);
		return err;
	}

	return 0;

}

int serial_flasher_cmd_h2d_request_upgrade(struct serial_flasher *ctx)
{
	uint8_t *send_buf;
	int err, send_len;
	uint8_t *host_version = OTA_HOST_VERSION;
	struct serial_flasher_cmd_request request;

	SYS_LOG_INF("request upgrade\n");

	send_buf = ctx->send_buf + sizeof(struct serial_flasher_head);
	send_buf = tlv_pack_array(send_buf, 0x1, (void *)host_version, strlen(host_version));
	send_buf = TLV_PACK_U16(send_buf, 0x2, 0);
	send_buf = TLV_PACK_U8(send_buf, 0x3, 0);
	send_buf = TLV_PACK_U8(send_buf, 0x4, 30);

	send_len = send_buf - ctx->send_buf;

	request.buf = ctx->send_buf;
	request.size = send_len;
	request.cmd = OTA_CMD_H2D_REQUEST_UPGRADE;
	request.with_resp = true;
	request.retry_times = 5;

	err = serial_flasher_send_command_reqeust(ctx, &request);
	if (err) {
		SYS_LOG_ERR("failed to send cmd %d, error %d",
			OTA_CMD_D2H_REQUIRE_IMAGE_DATA, err);
		return err;
	}

	ctx->state = PROT_STATE_IDLE;

	return 0;
}



int serial_flasher_cmd_h2d_cancel_upgrade(struct serial_flasher *ctx)
{

	uint8_t *send_buf;
	int err, send_len;
	struct serial_flasher_cmd_request request;

	SYS_LOG_DBG("cancel upgrade\n");

	send_buf = ctx->send_buf + sizeof(struct serial_flasher_head);
	send_buf = TLV_PACK_U8(send_buf, 0x1, 0x1);

	send_len = send_buf - ctx->send_buf;

	request.buf = ctx->send_buf;
	request.size = send_len;
	request.cmd = OTA_CMD_D2H_REQUST_CANCEL_UPGRADE;
	request.with_resp = true;
	request.retry_times = 2;

	err = serial_flasher_send_command_reqeust(ctx, &request);

	ctx->state = PROT_STATE_IDLE;

	return err;

}

int serial_flasher_cmd_d2h_request_cancel_upgrade(struct serial_flasher *ctx, uint32_t param_len)
{
#if 0
	uint8_t *send_buf;
	int err, send_len;
	struct serial_flasher_cmd_request request;

	SYS_LOG_DBG("request upgrade\n");

	send_buf = ctx->send_buf + sizeof(struct serial_flasher_head);
	send_buf = TLV_PACK_U8(send_buf, 0x1, 0x1);

	send_len = send_buf - ctx->send_buf;

	request.buf = ctx->send_buf;
	request.size = send_len;
	request.cmd = OTA_CMD_D2H_REQUST_CANCEL_UPGRADE;
	request.with_resp = false;
	request.retry_times = 5;

	err = serial_flasher_send_command_reqeust(ctx, &request);

	ctx->state = PROT_STATE_IDLE;

	serial_flasher_send_ack_frame(ctx, OTA_CMD_D2H_REQUST_CANCEL_UPGRADE, OTA_ERROR_CODE_SUCCESS);

	return 0;
#else
	int head_len;
	struct tlv_data tlv;
	int data_size = param_len;
	u8_t cancel_reason;

	ctx->read_ptr = ctx->frame_buffer + sizeof(struct serial_flasher_head);

	while (param_len > 0) {
		head_len = tlv_unpack_head(ctx, &tlv);

		switch (tlv.type) {
		case 0x01:
			tlv_unpack_value(ctx, (uint8_t *)&cancel_reason, 1);
			SYS_LOG_INF("reason:%d\n", cancel_reason);
			ctx->upgrade_end = true;
			break;

		default:
			/* skip other parameters by now */
			tlv_unpack_value(ctx, NULL, tlv.len);
			break;
		}

		if(param_len >= (head_len + tlv.len)){
			param_len -= head_len + tlv.len;

			if(param_len == 0){
				serial_flasher_send_ack_frame(ctx, OTA_CMD_D2H_REQUST_CANCEL_UPGRADE, OTA_ERROR_CODE_SUCCESS);
			}
		}else{
			printk("parser data err %x %x\n", param_len, head_len + tlv.len);
			print_buffer(ctx->frame_buffer, 1, data_size, 16, -1);
			param_len = 0;
		}
	}

	return 0;

#endif
}


int serial_flasher_cmd_h2d_connect_negotiation(struct serial_flasher *ctx)
{
	uint8_t *send_buf;
	int err, send_len;
	struct serial_flasher_cmd_request request;
	int head_len;
	struct tlv_data tlv;

	SYS_LOG_DBG("connect negotiation\n");

	send_buf = ctx->send_buf + sizeof(struct serial_flasher_head);
	send_len = send_buf - ctx->send_buf;

	request.buf = ctx->send_buf;
	request.size = send_len;
	request.cmd = OTA_CMD_H2D_CONNECT_NEGOTIATION;
	request.with_resp = true;
	request.retry_times = 5;

	err = serial_flasher_send_command_reqeust(ctx, &request);
	if (err) {
		SYS_LOG_ERR("failed to send cmd %d, error %d",
			OTA_CMD_D2H_REQUIRE_IMAGE_DATA, err);
		return err;
	}

	ctx->read_ptr = request.recv_buf;

	while (request.recv_size > 0) {
		head_len = tlv_unpack_head(ctx, &tlv);

		switch (tlv.type) {
		case 0x01:
			tlv_unpack_value(ctx, (uint8_t *)&ctx->host_wait_timeout, 2);
			break;

		case 0x02:
			tlv_unpack_value(ctx, (uint8_t *)&ctx->device_restart_timeout, 2);
			break;

		case 0x03:
			tlv_unpack_value(ctx, (uint8_t *)&ctx->ota_unit_size, 2);
			break;

		default:
			/* skip other parameters by now */
			tlv_unpack_value(ctx, NULL, tlv.len);
			break;
		}

		if(request.recv_size >= head_len + tlv.len){
			request.recv_size -= head_len + tlv.len;
		}else{
			request.recv_size = 0;
		}
	}


	ctx->state = PROT_STATE_IDLE;

	return 0;
}

int serial_flasher_cmd_h2d_negotiation_result(struct serial_flasher *ctx, int negotiation_result)
{
	uint8_t *send_buf;
	int err, send_len;
	struct serial_flasher_cmd_request request;

	send_buf = ctx->send_buf + sizeof(struct serial_flasher_head);
	send_buf = TLV_PACK_U32(send_buf, 0x1, negotiation_result);

	send_len = send_buf - ctx->send_buf;

	request.buf = ctx->send_buf;
	request.size = send_len;
	request.cmd = OTA_CMD_H2D_NEGOTIATION_RESULT;
	request.with_resp = true;
	request.retry_times = 5;

	err = serial_flasher_send_command_reqeust(ctx, &request);
	if (err) {
		SYS_LOG_ERR("failed to send cmd %d, error %d",
			OTA_CMD_D2H_REQUIRE_IMAGE_DATA, err);
		return err;
	}

	ctx->state = PROT_STATE_IDLE;

	return 0;
}

int serial_flasher_cmd_h2d_send_image_data(struct serial_flasher *ctx, uint32_t psn, uint8_t *data_buf, uint32_t data_len)
{
	uint8_t *send_buf;
	int err, send_len;
	struct serial_flasher_cmd_request request;

	send_buf = ctx->send_buf + sizeof(struct serial_flasher_head);

	sys_put_le32(psn, send_buf);
	send_buf += 4;


	memcpy(send_buf, data_buf, data_len);
	send_buf += data_len;

	send_len = send_buf - ctx->send_buf;

	request.buf = ctx->send_buf;
	request.size = send_len;
	request.cmd = OTA_CMD_H2D_SEND_IMAGE_DATA;
	request.with_resp = false;
	request.retry_times = 5;

	//printk("send data %d len %d\n", data_len, request.size);

	err = serial_flasher_send_command_reqeust(ctx, &request);
	if (err) {
		SYS_LOG_ERR("failed to send cmd %d, error %d",
			OTA_CMD_D2H_REQUIRE_IMAGE_DATA, err);
		return err;
	}

	ctx->state = PROT_STATE_IDLE;

	return 0;
}

int serial_flasher_cmd_d2h_require_image_data(struct serial_flasher *ctx, uint32_t param_len)
{
	int head_len;
	struct tlv_data tlv;
	uint32_t read_total_index = 0;
	uint32_t i,j;
	uint32_t read_total_len, read_size, read_addr;
	uint32_t send_unit_size;

	ctx->read_ptr = ctx->frame_buffer + sizeof(struct serial_flasher_head);

	while (param_len > 0) {
		head_len = tlv_unpack_head(ctx, &tlv);

		switch (tlv.type) {
		case 0x01:
			tlv_unpack_value(ctx, (uint8_t *)&ctx->read_offset, 4);
			break;

		case 0x02:
			tlv_unpack_value(ctx, (uint8_t *)&ctx->read_len, 4);
			break;

		case 0x03:
			tlv_unpack_value(ctx, ctx->read_mask, tlv.len);
			read_total_index = tlv.len;
			break;

		default:
			/* skip other parameters by now */
			tlv_unpack_value(ctx, NULL, tlv.len);
			break;
		}

		if(param_len >= (head_len + tlv.len)){
			param_len -= head_len + tlv.len;
		}else{
			param_len = 0;
		}
	}

	//serial_flasher_send_ack_frame(ctx, OTA_CMD_D2H_REQUIRE_IMAGE_DATA, OTA_ERROR_CODE_SUCCESS);

	//if(serial_flasher_receive_ack_frame(ctx, OTA_CMD_D2H_REQUIRE_IMAGE_DATA) != 0){
	//	return 0;
	//}

	printk("recve require image off %x len %x %x idx %x \n", ctx->read_offset, ctx->read_len, ctx->read_done_len, read_total_index);

	read_total_len = 0;
	read_addr = ctx->read_offset;

	send_unit_size = ctx->ota_unit_size - sizeof(struct serial_flasher_head) - 4;

	for(i = 0; i < read_total_index; i++){
		for(j = 0; j < 8; j++){
			if((ctx->read_mask[i] & (1 << j)) == 0){
				if((ctx->read_len - read_total_len) >= ctx->ota_unit_size){
					read_size = send_unit_size;
				}else{
					read_size = ctx->read_len - read_total_len;
				}

				if(ctx->binary_read_callback(ctx, read_addr, ctx->frame_buffer, read_size) != 0){
					ctx->upgrade_end = true;
					ctx->upgrade_result = false;
					return 0;
				}

				read_total_len += read_size;

				read_addr += read_size;

				serial_flasher_cmd_h2d_send_image_data(ctx, i * 8 + j, ctx->frame_buffer, read_size);

#ifdef CONFIG_WATCHDOG
				watchdog_clear();
#endif

#ifdef CONFIG_TASK_WDT
				task_wdt_feed_all();
#endif
			}else{
				read_total_len += send_unit_size;

				read_addr += send_unit_size;
			}

			if(read_total_len == ctx->read_len){
				break;
			}
		}
		if(read_total_len == ctx->read_len){
			break;
		}
	}

	return serial_flasher_cmd_h2d_negotiation_result(ctx, ctx->read_offset);
}

int serial_flasher_cmd_d2h_validate_image_data(struct serial_flasher *ctx, uint32_t param_len)
{
	int head_len;
	struct tlv_data tlv;
	int data_size = param_len;

	//printk("param len %d caller %p", param_len, __builtin_return_address(0));

	ctx->read_ptr = ctx->frame_buffer + sizeof(struct serial_flasher_head);

	while (param_len > 0) {
		head_len = tlv_unpack_head(ctx, &tlv);

		switch (tlv.type) {
		case 0x01:
			tlv_unpack_value(ctx, (uint8_t *)&ctx->upgrade_result, 1);
			if(ctx->upgrade_result == true){
				SYS_LOG_INF("upgrade success!\n");
			}else{
				SYS_LOG_ERR("upgrade error!\n");
			}
			ctx->upgrade_end = true;
			break;

		default:
			/* skip other parameters by now */
			tlv_unpack_value(ctx, NULL, tlv.len);
			break;
		}

		if(param_len >= (head_len + tlv.len)){
			param_len -= head_len + tlv.len;

			if(param_len == 0){
				serial_flasher_send_ack_frame(ctx, OTA_CMD_D2H_VALIDATE_IMAGE, OTA_ERROR_CODE_SUCCESS);
			}
		}else{
			printk("parser data err %x %x\n", param_len, head_len + tlv.len);
			print_buffer(ctx->frame_buffer, 1, data_size, 16, -1);
			param_len = 0;
		}
	}



	return 0;
}

int serial_flasher_cmd_d2h_report_status(struct serial_flasher *ctx, uint32_t param_len)
{
	return 0;
}




int serial_flasher_cmd_d2h_report_image_valid(struct serial_flasher *ctx, int is_valid)
{
	uint8_t *send_buf;
	uint8_t valid_flag;
	int err, send_len;
	struct serial_flasher_cmd_request request;

	if (!ctx->negotiation_done) {
		SYS_LOG_ERR("negotiation not done");
		return -EIO;

	}

	valid_flag = is_valid ? 1 : 0;

	send_buf = ctx->send_buf + sizeof(struct serial_flasher_head);
	send_buf = TLV_PACK_U8(send_buf, 0x1, valid_flag);
	send_len = send_buf - ctx->send_buf;

	request.buf = ctx->send_buf;
	request.size = send_len;
	request.cmd = OTA_CMD_D2H_VALIDATE_IMAGE;
	request.with_resp = true;
	request.retry_times = 5;

	err = serial_flasher_send_command_reqeust(ctx, &request);
	if (err) {
		SYS_LOG_ERR("failed to send cmd %d, error %d",
		request.cmd, err);
	}

	/* reset state to idle anyway */
	ctx->state = PROT_STATE_IDLE;

	return err;
}







static const struct serial_flasher_cmd serial_flasher_cmds[] = {
	{OTA_CMD_D2H_REQUIRE_IMAGE_DATA, serial_flasher_cmd_d2h_require_image_data,},
	{OTA_CMD_D2H_VALIDATE_IMAGE, serial_flasher_cmd_d2h_validate_image_data},
	{OTA_CMD_D2H_REPORT_STATUS, serial_flasher_cmd_d2h_report_status},
	{OTA_CMD_D2H_REQUST_CANCEL_UPGRADE, serial_flasher_cmd_d2h_request_cancel_upgrade},
};

int serial_flasher_process_command(struct serial_flasher *ctx, uint32_t *processed_cmd)
{
	struct serial_flasher_head *head;
	serial_flasher_cmd_handler_t cmd_handler;
	int i, err;
	int deal_cmd;

	if (ctx->state != PROT_STATE_IDLE) {
		SYS_LOG_ERR("current state is not idle");
		return -EIO;
	}

	head = (struct serial_flasher_head *)ctx->frame_buffer;

	err = serial_flasher_check_frame_valid(ctx, head);
	if(err){
		if(err != -EIO){
			serial_flasher_send_ack_frame(ctx, head->cmd, OTA_ERROR_CODE_CRC_ERROR);
		}
		return err;
	}

	SYS_LOG_DBG("svc head: svc_id 0x%x, cmd_id 0x%x, param type 0x%x, len 0x%x",
		head->svc_id, head->cmd, head->param_type, head->param_len);

	ctx->state = PROT_STATE_DATA;

	deal_cmd = head->cmd;

	for (i = 0; i < ARRAY_SIZE(serial_flasher_cmds); i++) {
		if (serial_flasher_cmds[i].cmd == deal_cmd) {
			cmd_handler = serial_flasher_cmds[i].handler;
			err = cmd_handler(ctx, head->param_len);
			if (processed_cmd){
				*processed_cmd = deal_cmd;
			}
			if (err) {
				SYS_LOG_ERR("cmd_handler %p, err: %d", cmd_handler, err);
				err = -EACCES;
				return err;
			}
		}
	}

	if (i > ARRAY_SIZE(serial_flasher_cmds)) {
		SYS_LOG_ERR("invalid cmd: %d", head->cmd);
		return -EACCES;
	}

	SYS_LOG_DBG("after parse ctx %p, state %d", ctx, ctx->state);

	ctx->state = PROT_STATE_IDLE;

	return err;
}

int serial_flasher_port_init(struct serial_flasher *ctx, const char *dev_name, uint32_t recv_buf_size, void (*send_cb)(uint8_t *buf, uint32_t len))
{
	struct letws_ota_stream_param param;

	param.sync_mode = false;
	param.send_cb = send_cb;

	ctx->letws_stream = letws_ota_stream_create(&param);

	if(!ctx->letws_stream){
		return -EINVAL;
	}

	ctx->cbuf = (struct acts_ringbuf *)param.cbuf;

	stream_open(ctx->letws_stream, MODE_IN_OUT);

	ctx->send_buf = mem_malloc(OTA_UNIT_SIZE);
	ctx->send_buf_size = OTA_UNIT_SIZE;
	ctx->ota_unit_size = OTA_UNIT_SIZE;
	ctx->state = PROT_STATE_IDLE;
	ctx->frame_buffer = mem_malloc(OTA_UNIT_SIZE);
	ctx->read_timeout = CONFIG_SERIAL_FLASHER_COMMAND_TIMEOUT;

    return 0;
}


int serial_flasher_port_deinit(struct serial_flasher *ctx)
{
    return 0;
}

int serial_flasher_request_upgrade(struct serial_flasher *ctx)
{
	int ret_val = 0;
	uint32_t start_time = os_uptime_get();
	uint32_t cur_time;

	while(1){
		cur_time = os_uptime_get();
		if((cur_time - start_time) > 30000){
			ret_val = -EIO;
			break;
		}

		ret_val = serial_flasher_cmd_h2d_request_upgrade(ctx);

		if(ret_val == 0){
			break;
		}else{
			os_sleep(1000);

			printk("retry time %d %d %d\n", cur_time, start_time, (cur_time - start_time));
		}
	}

    return ret_val;
}

int serial_flasher_cancel_upgrade(struct serial_flasher *ctx)
{
	int ret_val = 0;
	uint32_t start_time = os_uptime_get();
	uint32_t cur_time;

	while(1){
		cur_time = os_uptime_get();
		if((cur_time - start_time) > 30000){
			ret_val = -EIO;
			break;
		}

		ret_val = serial_flasher_cmd_h2d_cancel_upgrade(ctx);

		if(ret_val == 0){
			break;
		}else{
			os_sleep(1000);

			printk("serial_flasher_cancel_upgrade retry time %d %d %d\n", cur_time, start_time, (cur_time - start_time));
		}
	}

    return ret_val;
}



int serial_flasher_connect_negotiation(struct serial_flasher *ctx)
{
    return serial_flasher_cmd_h2d_connect_negotiation(ctx);
}

int serial_flasher_send_negotiation_result(struct serial_flasher *ctx, int result)
{
    return serial_flasher_cmd_h2d_negotiation_result(ctx, result);
}

