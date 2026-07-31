/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief OTA bluetooth backend interface
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
#include <ota_backend.h>
#include <ota_backend_letws_stream.h>
#include <crc.h>
#include <misc/byteorder.h>
#include <thread_timer.h>
#include <acts_ringbuf.h>

/* support check data checksum in each ota unit */
#define CONFIG_OTA_BT_SUPPORT_UNIT_CRC
#define SHAKEHAD_PERIOD                 20
#define OTA_CMD_RETRY_TIMES             10

#define TLV_TYPE_OTA_SUPPORT_FEATURES		0x09
#define OTA_SUPPORT_FEATURE_UNIT_DATA_CRC	(1 << 0)
#define OTA_ERROR_CODE_SUCCESS		100000
#define OTA_ERROR_CODE_CRC_ERROR    100015

#define SERVICE_ID_OTA		0x9

#define OTA_SVC_SEND_BUFFER_SIZE	0x80
#define UART_OTA_RECV_BUFFER_SIZE   0x800

#define OTA_READ_DATA_SIZE      (220)
#define OTA_MAX_READ_DATA_SIZE  (64 * 1024)
#define OTA_UNIT_SIZE			(OTA_READ_DATA_SIZE + 4 + sizeof(struct letws_ota_head))

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

#define TLV_PACK_U8(buf, type, value)	tlv_pack_data(buf, type, sizeof(uint8_t), value)
#define TLV_PACK_U16(buf, type, value)	tlv_pack_data(buf, type, sizeof(uint16_t), value)
#define TLV_PACK_U32(buf, type, value)	tlv_pack_data(buf, type, sizeof(uint32_t), value)


enum letws_ota_state{
	PROT_STATE_IDLE,
	PROT_STATE_DATA,
};

struct letws_ota_stream_ota_ctx {
	uint8_t state;
	int send_buf_size;
	uint8_t *send_buf;

	uint8_t *read_buf;
	int read_len;
	uint8_t read_mask[OTA_MAX_READ_DATA_SIZE / OTA_READ_DATA_SIZE / 8];
	int read_done_len;
	uint8_t last_psn;
	uint8_t timer_start_flag;
	uint8_t timer_need_stop_flag;
	uint8_t host_features;
	uint16_t ota_unit_size;

	io_stream_t letws_ota_stream;
	int letws_ota_stream_opened;
	int negotiation_done;
	int negotiation_result;

	uint8_t *frame_buffer;
	uint8_t *read_ptr;

	os_sem read_sem;
	struct thread_timer shakehand_looper;
	uint32_t read_timeout;

	struct acts_ringbuf *data_cbuf;
};

struct letws_ota_head {
	uint8_t svc_id;
	uint8_t cmd;
	uint8_t param_type;
	uint16_t param_len;
	uint32_t param_crc;
	uint8_t payload[0];
} __attribute__((packed));

struct letws_ota_cmd_request{
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

typedef int (*letws_ota_cmd_handler_t)(struct letws_ota_stream_ota_ctx *ctx, uint16_t param_len);

struct letws_ota_cmd {
	uint8_t cmd;
	letws_ota_cmd_handler_t handler;
};

struct ota_backend_letws_ota_stream {
	struct ota_backend backend;
	struct letws_ota_stream_ota_ctx svc_ctx;
};

/* for letws_ota_stream callback */
//static struct ota_backend_letws_ota_stream *g_backend_letws_ota_stream;

static int letws_ota_get_rx_data(struct letws_ota_stream_ota_ctx *ctx, void *buf, int size)
{
	int read_size;

	while (size > 0) {
		read_size = stream_read(ctx->letws_ota_stream, buf, size);
		if (read_size <= 0) {
			if(ctx->read_timeout == OS_FOREVER){
				SYS_LOG_ERR("need read %d bytes, but only got %d bytes",
					size, read_size);
			}
			return -EIO;
		}

		size -= read_size;
		buf += read_size;
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


static int tlv_unpack_head(struct letws_ota_stream_ota_ctx *ctx, struct tlv_data *tlv)
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

static int tlv_unpack_value(struct letws_ota_stream_ota_ctx *ctx, uint8_t *buf, int size)
{
	if(buf){
		memcpy(buf, ctx->read_ptr, size);
	}
	ctx->read_ptr += size;

	return 0;
}

static int ota_backend_letws_rx_data(struct letws_ota_stream_ota_ctx *ctx, int size)
{
	int err;
	uint8_t c;

	while (size > 0) {
		err = stream_read(ctx->letws_ota_stream, &c, sizeof(uint8_t));
		if (err) {
			SYS_LOG_ERR("failed to get data");
			return err;
		}

		size--;
	}

	return 0;
}

static int ota_backend_letws_drop_all_rx_data(struct letws_ota_stream_ota_ctx *ctx , int wait_ms)
{
	int err, data_len;
	int ms = wait_ms;

	while (wait_ms > 0) {
		data_len = stream_tell(ctx->letws_ota_stream);
		if (data_len > 0) {
			SYS_LOG_INF("drop data len 0x%x", data_len);

			err = ota_backend_letws_rx_data(ctx, data_len);
			if (err)
				return err;

			wait_ms = ms;
		}

		os_sleep(20);
		wait_ms -= 20;
	}

	return 0;
}


int letws_ota_send_data(struct letws_ota_stream_ota_ctx *ctx, uint8_t *buf, int size)
{
	int err;

	err = stream_write(ctx->letws_ota_stream, buf, size);
	if (err < 0) {
		SYS_LOG_ERR("failed to send data, size %d, err %d", size, err);
		return -EIO;
	}

	return 0;
}

int ota_letws_ota_stream_send_response(struct letws_ota_stream_ota_ctx *ctx, uint8_t cmd,
		    uint8_t *buf, int size)
{
	struct letws_ota_head *head;
	int err;

	head = (struct letws_ota_head *)buf;
	head->svc_id = SERVICE_ID_OTA;
	head->cmd = cmd;
	head->param_type = TLV_TYPE_MAIN;
	head->param_crc = 0;
	head->param_len = size - sizeof(struct letws_ota_head);
	//add crc value
	head->param_crc = utils_crc32(0, buf, size);

	//SYS_LOG_INF("send response: %d %d", cmd, size);
	//print_buffer(buf, 1, 16, 16, -1);
	err = letws_ota_send_data(ctx, buf, size);
	if (err) {
		SYS_LOG_ERR("failed to send cmd %d, err %d", cmd, err);
		return err;
	}

	return 0;
}

static int ota_send_ack_frame(struct letws_ota_stream_ota_ctx *ctx, uint32_t cmd, uint32_t err_code)
{
	uint8_t *send_buf;
	uint32_t send_len;
	int err;

	/* init ack data */
	send_buf = ctx->send_buf + sizeof(struct letws_ota_head);
	send_buf = TLV_PACK_U32(send_buf, TLV_TYPE_ERROR_CODE, err_code);

	send_len = send_buf - ctx->send_buf;

	err = ota_letws_ota_stream_send_response(ctx, cmd, ctx->send_buf, send_len);
	if (err) {
		SYS_LOG_ERR("failed to send cmd %d, error %d",
			cmd, err);
		return err;
	}

	return 0;

}

int ota_at_stream_cmd_h2d_request_upgrade(struct letws_ota_stream_ota_ctx *ctx, uint16_t param_size)
{
	int err, send_len, battery_threahold, head_len;
	uint8_t *send_buf;
	struct tlv_data tlv;
	uint8_t host_features, device_features;

	SYS_LOG_INF("upgrade request: param_size %d", param_size);

	ctx->host_features = 0;
	device_features = 0;
	while (param_size > 0) {
		head_len = tlv_unpack_head(ctx, &tlv);
		if (head_len <= 0)
			return -EIO;

		printk("parser tlv type %x len %d head len %d\n", tlv.type, tlv.len, head_len);

		switch (tlv.type) {
		case TLV_TYPE_OTA_SUPPORT_FEATURES:
			err = tlv_unpack_value(ctx, &host_features, 1);
			if (err) {
				SYS_LOG_ERR("failed to read tlv value");
				return err;
			}

			ctx->host_features = host_features;
			SYS_LOG_INF("host support features: 0x%x", host_features);
			break;
		default:
			/* skip other parameters by now */
			err = tlv_unpack_value(ctx, NULL, tlv.len);
			if (err)
				return -EIO;
			break;
		}

		param_size -= head_len + tlv.len;
	}

	ctx->state = PROT_STATE_IDLE;

	/* dummy value, for debug only */
	battery_threahold = 30;

	/* init ack data */
	send_buf = ctx->send_buf + sizeof(struct letws_ota_head);
	send_buf = TLV_PACK_U32(send_buf, TLV_TYPE_ERROR_CODE, OTA_ERROR_CODE_SUCCESS);
	send_buf = TLV_PACK_U8(send_buf, 0x04, battery_threahold);

	/* send device features only if host has support features */
	if (ctx->host_features) {
#ifdef CONFIG_OTA_BT_SUPPORT_UNIT_CRC
		device_features |= OTA_SUPPORT_FEATURE_UNIT_DATA_CRC;
#endif
		send_buf = TLV_PACK_U8(send_buf, TLV_TYPE_OTA_SUPPORT_FEATURES, device_features);
		SYS_LOG_INF("device support features: 0x%x", 0);
	}

	send_len = send_buf - ctx->send_buf;

	err = ota_letws_ota_stream_send_response(ctx, OTA_CMD_H2D_REQUEST_UPGRADE, ctx->send_buf, send_len);
	if (err) {
		SYS_LOG_ERR("failed to send cmd %d, error %d",
			OTA_CMD_H2D_REQUEST_UPGRADE, err);
		return err;
	}

	return 0;
}

int ota_at_stream_cmd_h2d_connect_negotiation(struct letws_ota_stream_ota_ctx *ctx, uint16_t param_len)
{
	uint16_t app_wait_timeout, device_restart_timeout, ota_unit_size, interval;
	uint8_t ack_enable;
	uint8_t *send_buf;
	int err, send_len;

	if (param_len != 0) {
		SYS_LOG_ERR("read_param_len 0, real param_len %d\n", param_len);
	}

	ctx->state = PROT_STATE_IDLE;

	app_wait_timeout = 3;
	device_restart_timeout = 5;
	ota_unit_size = ctx->ota_unit_size;
	interval = 0;
	ack_enable= 0;

	SYS_LOG_INF("send app_wait_timeout %d, device_restart_timeout %d\n",
		app_wait_timeout, device_restart_timeout);
	SYS_LOG_INF("ota_unit_size %d, interval %d, ack_enable %d\n",
		ota_unit_size, interval, ack_enable);

	send_buf = ctx->send_buf + sizeof(struct letws_ota_head);
	send_buf = TLV_PACK_U16(send_buf, 0x1, app_wait_timeout);
	send_buf = TLV_PACK_U16(send_buf, 0x2, device_restart_timeout);
	send_buf = TLV_PACK_U16(send_buf, 0x3, ota_unit_size);
	send_buf = TLV_PACK_U16(send_buf, 0x4, interval);
	send_buf = TLV_PACK_U8(send_buf, 0x5, ack_enable);
	send_len = send_buf - ctx->send_buf;

	err = ota_letws_ota_stream_send_response(ctx, OTA_CMD_H2D_CONNECT_NEGOTIATION, ctx->send_buf, send_len);
	if (err) {
		SYS_LOG_ERR("failed to send cmd %d, error %d",
			OTA_CMD_H2D_CONNECT_NEGOTIATION, err);
		return err;
	}

	return err;
}

int ota_at_stream_cmd_h2d_negotiation_result(struct letws_ota_stream_ota_ctx *ctx, uint16_t param_len)
{
	uint32_t negotiation_result;
	int head_len;
	struct tlv_data tlv;

	while (param_len > 0) {
		head_len = tlv_unpack_head(ctx, &tlv);

		switch (tlv.type) {
		case 0x1:
			tlv_unpack_value(ctx, (uint8_t *)&negotiation_result, 4);
			break;
		default:
			/* skip other parameters by now */
			tlv_unpack_value(ctx, NULL, tlv.len);
			break;
		}

		param_len -= head_len + tlv.len;
	}

	SYS_LOG_INF("negotiation_result 0x%x\n", negotiation_result);

	ota_send_ack_frame(ctx, OTA_CMD_H2D_NEGOTIATION_RESULT, OTA_ERROR_CODE_SUCCESS);

	ctx->negotiation_done = 1;

	ctx->negotiation_result = negotiation_result;

	ctx->state = PROT_STATE_IDLE;

	return 0;
}


static int ota_cmd_resp_is_nak_frame(struct letws_ota_stream_ota_ctx *ctx, struct letws_ota_head *resp)
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

		param_size -= head_len + tlv.len;
	}

	return false;

}
void serial_flasher_dump_data(char *str, const void *buf, int size);

static int ota_check_frame_valid(struct letws_ota_stream_ota_ctx *ctx, struct letws_ota_head *head)
{
	int err;
	uint8_t frame_len;
	uint32_t calc_crc;
	uint32_t recv_crc;

	os_sem_take(&ctx->read_sem, ctx->read_timeout);

	err = letws_ota_get_rx_data(ctx, &frame_len, sizeof(frame_len));
	if (err) {
		//printk("read rx data timeout\n");
		return -EIO;
	}

	if (frame_len > ctx->ota_unit_size){
		SYS_LOG_ERR("invalid framelen: %x", frame_len);
		goto err_deal;
	}

	err = letws_ota_get_rx_data(ctx, head, frame_len);
	if (err) {
		SYS_LOG_ERR("cannot read head bytes");
		goto err_deal;
	}

	if (head->svc_id != SERVICE_ID_OTA) {
		SYS_LOG_ERR("invalid svc_id: %d", head->svc_id);
		goto err_deal;
	}

	if (head->param_type != TLV_TYPE_MAIN) {
		SYS_LOG_ERR("invalid param type: %d", head->param_type);
		goto err_deal;
	}

	if (head->param_len > ctx->ota_unit_size){
		SYS_LOG_ERR("invalid param len: %d", head->param_len);
		goto err_deal;
	}

	recv_crc = head->param_crc;
	head->param_crc = 0;

	calc_crc = utils_crc32(0, (u8_t *)head, sizeof(struct letws_ota_head) + head->param_len);

	if (recv_crc != calc_crc){
		SYS_LOG_ERR("recv crc %x cal crc %x\n", recv_crc, calc_crc);
		goto err_deal;
	}
	head->param_crc = recv_crc;

	printk("recv cmd %d len %d\n", head->cmd, sizeof(struct letws_ota_head) + head->param_len);
	print_buffer(head, 1, 16, 16, -1);

	return 0;
err_deal:
	stream_flush(ctx->letws_ota_stream);
	return -EINVAL;
}

static int ota_cmd_d2h_send_reqeust(struct letws_ota_stream_ota_ctx *ctx, struct letws_ota_cmd_request *request)
{
	int err = 0;

	struct letws_ota_head *head;
	struct letws_ota_head *resp;

	uint32_t ota_cmd_state;
    enum {
        OTA_CMD_STATE_CMD,
        OTA_CMD_STATE_RESP,
        OTA_CMD_STATE_SUCCESS,
        OTA_CMD_STATE_FAIL,
    };

	head = (struct letws_ota_head *)request->buf;
	head->svc_id = SERVICE_ID_OTA;
	head->cmd = request->cmd;
	head->param_type = TLV_TYPE_MAIN;
	head->param_crc = 0;
	head->param_len = request->size - sizeof(struct letws_ota_head);
	//add crc value
	head->param_crc = utils_crc32(0, request->buf, request->size);

    ota_cmd_state = OTA_CMD_STATE_CMD;
	while(1){
		switch(ota_cmd_state){
			case OTA_CMD_STATE_CMD:

			SYS_LOG_INF("send cmd: %d %d", request->cmd, request->size);
			print_buffer(request->buf, 1, 16, 16, -1);

			err = letws_ota_send_data(ctx, request->buf, request->size);
			if (err) {
				SYS_LOG_ERR("failed to send cmd %d, err %d", request->cmd, err);
				return err;
			}
			if(request->with_resp){
				ota_cmd_state = OTA_CMD_STATE_RESP;
			}else{
				ota_cmd_state = OTA_CMD_STATE_SUCCESS;
			}
			break;

			case OTA_CMD_STATE_RESP:
			resp = (struct letws_ota_head *)ctx->frame_buffer;
			err = ota_check_frame_valid(ctx, resp);
			if(err != 0){
				printk("recv resp err %d\n", request->retry_times);
				if(request->retry_times <= 0){
                    ota_cmd_state = OTA_CMD_STATE_FAIL;
					err = -EIO;
                }else{
                    request->retry_times--;
                    ota_cmd_state = OTA_CMD_STATE_CMD;
                }
			}else{
				if((resp->cmd != request->cmd) || ota_cmd_resp_is_nak_frame(ctx, resp)){
					printk("recv resp cmd %d %d times %d\n", request->cmd, request->cmd, request->retry_times);
					if(request->retry_times <= 0){
	                    ota_cmd_state = OTA_CMD_STATE_FAIL;
						err = -EINVAL;
	                }else{
	                    request->retry_times--;
	                    ota_cmd_state = OTA_CMD_STATE_CMD;
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

int ota_at_stream_cmd_d2h_require_image_data(struct letws_ota_stream_ota_ctx *ctx, uint32_t offset, int len, uint8_t *buf)
{
	uint8_t *send_buf;
	int err, send_len;
	int mask_len;
	struct letws_ota_cmd_request request;

	SYS_LOG_INF("offset 0x%x, len %d, buf %p, \n", offset, len, buf);

	if (!ctx->negotiation_done) {
		SYS_LOG_ERR("negotiation not done");
		return -EIO;

	}


	send_buf = ctx->send_buf + sizeof(struct letws_ota_head);
	send_buf = TLV_PACK_U32(send_buf, 0x1, offset);
	send_buf = TLV_PACK_U32(send_buf, 0x2, len);

	mask_len = ((len + OTA_READ_DATA_SIZE - 1) / OTA_READ_DATA_SIZE + 7) / 8;

	send_buf = tlv_pack_array(send_buf, 0x3, (void *)ctx->read_mask, mask_len);
	send_len = send_buf - ctx->send_buf;

	request.buf = ctx->send_buf;
	request.size = send_len;
	request.cmd = OTA_CMD_D2H_REQUIRE_IMAGE_DATA;
	request.with_resp = false;
	request.retry_times = OTA_CMD_RETRY_TIMES;

	err = ota_cmd_d2h_send_reqeust(ctx, &request);
	if (err) {
		SYS_LOG_ERR("failed to send cmd %d, error %d",
			OTA_CMD_D2H_REQUIRE_IMAGE_DATA, err);
		return err;
	}

	ota_send_ack_frame(ctx, OTA_CMD_D2H_REQUIRE_IMAGE_DATA, OTA_ERROR_CODE_SUCCESS);

	ctx->state = PROT_STATE_IDLE;

	return 0;
}

int ota_at_stream_cmd_h2d_send_image_data(struct letws_ota_stream_ota_ctx *ctx, uint16_t param_len, int with_crc)
{
	int err, seg_len;
	uint32_t psn;
	uint32_t crc, crc_orig;
	uint32_t data_unit_size;

	SYS_LOG_DBG("buf %p, len %d", ctx->read_buf, ctx->read_len);

	if (ctx->read_buf == NULL || ctx->read_len == 0 || param_len < 1) {
		return -EINVAL;
	}

	/* read psn */
	err = tlv_unpack_value(ctx, (uint8_t *)&psn, sizeof(uint32_t));
	if (err) {
		SYS_LOG_ERR("read error, err %d", err);
		return -EIO;
	}
	param_len -= sizeof(uint32_t);


	if (with_crc) {
		/* read crc field */
		err = tlv_unpack_value(ctx, (uint8_t *)&crc_orig, sizeof(uint32_t));
		if (err) {
			SYS_LOG_ERR("read error, err %d", err);
			return -EIO;
		}

		param_len -= sizeof(uint32_t);
	}

	seg_len = param_len;
	data_unit_size = ctx->ota_unit_size - sizeof(struct letws_ota_head) - 4;
	/* read data */
	err = tlv_unpack_value(ctx, ctx->read_buf + psn * data_unit_size, seg_len);
	if (err) {
		SYS_LOG_ERR("read error, err %d", err);
		return -EIO;
	}

	if (with_crc) {
		crc = utils_crc32(0, ctx->read_buf + psn * data_unit_size, seg_len);
		if (crc != crc_orig) {
			SYS_LOG_ERR("psn%d: crc check error, orig 0x%x != 0x%x",
				psn, crc_orig, crc);
			return -EIO;
		}
	}

	ctx->read_mask[psn / 8] |= (1 << (psn % 8));

	param_len -= seg_len;
	if (param_len != 0) {
		SYS_LOG_ERR("unknown remain param, len %d\n", param_len);
		return -EIO;
	}

	ctx->read_done_len += seg_len;
	ctx->last_psn = psn;
	ctx->state = PROT_STATE_IDLE;

	SYS_LOG_INF("cur psn %d read len %d\n", psn, ctx->read_done_len);

	return 0;
}

int ota_at_stream_cmd_h2d_send_image_data_with_crc(struct letws_ota_stream_ota_ctx *ctx, uint16_t param_len)
{
	return ota_at_stream_cmd_h2d_send_image_data(ctx, param_len, 1);
}

int ota_at_stream_cmd_h2d_send_image_data_no_crc(struct letws_ota_stream_ota_ctx *ctx, uint16_t param_len)
{
	return ota_at_stream_cmd_h2d_send_image_data(ctx, param_len, 0);
}

int ota_at_stream_cmd_d2h_report_image_valid(struct letws_ota_stream_ota_ctx *ctx, int is_valid)
{
	uint8_t *send_buf;
	uint8_t valid_flag;
	int err, send_len;
	struct letws_ota_cmd_request request;

	if (!ctx->negotiation_done) {
		SYS_LOG_ERR("negotiation not done");
		return -EIO;

	}

	valid_flag = is_valid ? 1 : 0;

	send_buf = ctx->send_buf + sizeof(struct letws_ota_head);
	send_buf = TLV_PACK_U8(send_buf, 0x1, valid_flag);
	send_len = send_buf - ctx->send_buf;

	request.buf = ctx->send_buf;
	request.size = send_len;
	request.cmd = OTA_CMD_D2H_VALIDATE_IMAGE;
	request.with_resp = true;
	request.retry_times = OTA_CMD_RETRY_TIMES;

	err = ota_cmd_d2h_send_reqeust(ctx, &request);
	if (err) {
		SYS_LOG_ERR("failed to send cmd %d, error %d",
		request.cmd, err);
	}

	/* reset state to idle anyway */
	ctx->state = PROT_STATE_IDLE;

	return err;
}

static const struct letws_ota_cmd ota_at_stream_cmds[] = {
	{OTA_CMD_H2D_REQUEST_UPGRADE, ota_at_stream_cmd_h2d_request_upgrade,},
	{OTA_CMD_H2D_CONNECT_NEGOTIATION, ota_at_stream_cmd_h2d_connect_negotiation,},
	{OTA_CMD_H2D_NEGOTIATION_RESULT, ota_at_stream_cmd_h2d_negotiation_result,},
	//{OTA_CMD_D2H_REQUIRE_IMAGE_DATA, ota_at_stream_cmd_d2h_require_image_data,},
	{OTA_CMD_H2D_SEND_IMAGE_DATA, ota_at_stream_cmd_h2d_send_image_data_no_crc,},
	{OTA_CMD_H2D_SEND_IMAGE_DATA_WITH_CRC, ota_at_stream_cmd_h2d_send_image_data_with_crc,},
};




int ota_at_stream_process_command(struct letws_ota_stream_ota_ctx *ctx, uint32_t *processed_cmd)
{
	struct letws_ota_head *head;
	letws_ota_cmd_handler_t cmd_handler;
	int i, err;

	if (ctx->state != PROT_STATE_IDLE) {
		SYS_LOG_ERR("current state is not idle");
		return -EIO;
	}

	head = (struct letws_ota_head *)ctx->frame_buffer;

	err = ota_check_frame_valid(ctx, head);
	if(err){
		if(err != -EIO){
			ota_send_ack_frame(ctx, head->cmd, OTA_ERROR_CODE_CRC_ERROR);
		}

		return err;
	}

	SYS_LOG_INF("svc head: svc_id 0x%x, cmd_id 0x%x, param type 0x%x, len 0x%x",
		head->svc_id, head->cmd, head->param_type, head->param_len);

	ctx->state = PROT_STATE_DATA;
	ctx->read_ptr = ctx->frame_buffer + sizeof(struct letws_ota_head);

	for (i = 0; i < ARRAY_SIZE(ota_at_stream_cmds); i++) {
		if (ota_at_stream_cmds[i].cmd == head->cmd) {
			cmd_handler = ota_at_stream_cmds[i].handler;
			err = cmd_handler(ctx, head->param_len);
			if (processed_cmd){
				*processed_cmd = head->cmd;
			}
			if (err) {
				SYS_LOG_ERR("cmd_handler %p, err: %d", cmd_handler, err);
				err = -EACCES;
				return err;
			}
		}
	}

	if (i > ARRAY_SIZE(ota_at_stream_cmds)) {
		SYS_LOG_ERR("invalid cmd: %d", head->cmd);
		return -EACCES;
	}

	SYS_LOG_DBG("after parse ctx %p, state %d", ctx, ctx->state);

	ctx->state = PROT_STATE_IDLE;

	return err;
}

int ota_at_stream_wait_negotiation_done(struct letws_ota_stream_ota_ctx *ctx)
{
	int err = 0;
	uint32_t cur_time = k_uptime_get_32();

	SYS_LOG_INF("wait, ctx %p\n", ctx);

	while (ctx->negotiation_done == 0) {
		err = ota_at_stream_process_command(ctx, NULL);
		if (err == -EACCES) {
			break;
		}

		if((k_uptime_get_32() - cur_time) > 5000){
			err = -ETIME;
			break;
		}
	}

	SYS_LOG_INF("wait ctx %p, return %d\n", ctx, err);

	return err;
}

int ota_backend_letws_ota_stream_ioctl(struct ota_backend *backend, int cmd, unsigned int param)
{
	struct ota_backend_letws_ota_stream *backend_letws_ota_stream = CONTAINER_OF(backend,
		struct ota_backend_letws_ota_stream, backend);
	struct letws_ota_stream_ota_ctx *svc_ctx = &backend_letws_ota_stream->svc_ctx;
	int err;

	SYS_LOG_INF("cmd 0x%x: param %d\n", cmd, param);

	switch (cmd) {
	case OTA_BACKEND_IOCTL_REPORT_IMAGE_VALID:
		err = ota_at_stream_cmd_d2h_report_image_valid(svc_ctx, param);
		if (err) {
			SYS_LOG_INF("send cmd 0x%x error", cmd);
			return -EIO;
		}
		break;

	case OTA_BACKEND_IOCTL_GET_MAX_SIZE:
		*(unsigned int*)param = OTA_READ_DATA_SIZE * 32;
		break;

	case OTA_BACKEND_IOCTL_REPORT_PROCESS:
		backend->cb(backend, OTA_BACKEND_UPGRADE_PROGRESS, param);
		break;
	default:
		SYS_LOG_ERR("unknow cmd 0x%x", cmd);
		return -EINVAL;
	}

	return 0;
}

int ota_backend_letws_ota_stream_read(struct ota_backend *backend, int offset, void *buf, int size)
{
	struct ota_backend_letws_ota_stream *backend_letws_ota_stream = CONTAINER_OF(backend,
		struct ota_backend_letws_ota_stream, backend);
	struct letws_ota_stream_ota_ctx *svc_ctx = &backend_letws_ota_stream->svc_ctx;
	int err, retry_times = 0;
	uint32_t processed_cmd;

	SYS_LOG_INF("offset 0x%x, size %d, buf %p", offset, size, buf);

	if(size > OTA_MAX_READ_DATA_SIZE){
		SYS_LOG_INF("read data max len err %d", size);
		return -EINVAL;
	}

	memset(svc_ctx->read_mask, 0, sizeof(svc_ctx->read_mask));
	svc_ctx->last_psn = 0xff;
	svc_ctx->read_buf = buf;
	svc_ctx->read_len = size;
	svc_ctx->read_done_len = 0;

try_again:
	err = ota_at_stream_cmd_d2h_require_image_data(svc_ctx, offset, size, buf);
	if (err) {
		SYS_LOG_INF("read data err %d", err);
		return -EIO;
	}

	while (true) {
		err = ota_at_stream_process_command(svc_ctx, &processed_cmd);
		if (err){
			if (err == -EACCES || err == -EIO) {
				SYS_LOG_INF("retrun err %d", err);
				break;
			}else{
				continue;
			}
		}

		if(processed_cmd == OTA_CMD_H2D_NEGOTIATION_RESULT && offset == svc_ctx->negotiation_result){
			if(svc_ctx->read_done_len != size){
				SYS_LOG_WRN("read size %d total size %d\n", svc_ctx->read_done_len, size);
				goto try_again;
			}else{
				SYS_LOG_DBG("read end");
				break;
			}
		}

		SYS_LOG_DBG("read_done_len size %d", svc_ctx->read_done_len);
	}

	if (err && retry_times < 1) {

		/* wait 500ms and drop all data in stream buffer */
		ota_backend_letws_drop_all_rx_data(svc_ctx, 500);

		svc_ctx->state = PROT_STATE_IDLE;
		retry_times++;

		SYS_LOG_INF("re-read offset 0x%x, size %d, buf %p", offset, size, buf);
		goto try_again;
	}

	return err;
}

int ota_backend_letws_ota_stream_open(struct ota_backend *backend)
{
	struct ota_backend_letws_ota_stream *backend_letws_ota_stream = CONTAINER_OF(backend,
		struct ota_backend_letws_ota_stream, backend);
	struct letws_ota_stream_ota_ctx *svc_ctx = &backend_letws_ota_stream->svc_ctx;

	SYS_LOG_INF("open letws_ota_stream %p", svc_ctx->letws_ota_stream);

	ota_at_stream_wait_negotiation_done(svc_ctx);

	return 0;
}

int ota_backend_letws_ota_stream_close(struct ota_backend *backend)
{
	SYS_LOG_INF("close: type %d", backend->type);

	int err;
	struct ota_backend_letws_ota_stream *backend_letws_ota_stream = CONTAINER_OF(backend,
		struct ota_backend_letws_ota_stream, backend);
	struct letws_ota_stream_ota_ctx *svc_ctx = &backend_letws_ota_stream->svc_ctx;

	//wait timer stop
	while(svc_ctx->timer_start_flag){
		svc_ctx->timer_need_stop_flag = true;
		os_sleep(10);
	}

	//do not close stream when backend close
	if (svc_ctx->letws_ota_stream_opened) {
		err = stream_close(svc_ctx->letws_ota_stream);
		if (err) {
			SYS_LOG_ERR("stream_close Failed");
		} else {
			svc_ctx->letws_ota_stream_opened = 0;
		}

		/* clear internal status */
		svc_ctx->negotiation_done = 0;
		svc_ctx->state = PROT_STATE_IDLE;
	}

	if (svc_ctx->letws_ota_stream) {
		stream_destroy(svc_ctx->letws_ota_stream);
		svc_ctx->letws_ota_stream = NULL;
	}

	if (backend_letws_ota_stream->svc_ctx.send_buf)
		mem_free(backend_letws_ota_stream->svc_ctx.send_buf);

	backend_letws_ota_stream->backend.cb(&backend_letws_ota_stream->backend, OTA_BACKEND_UPGRADE_STATE, 0);

	mem_free(backend_letws_ota_stream);

	return 0;
}

void ota_backend_letws_ota_stream_exit(struct ota_backend *backend)
{

}

extern int serial_flasher_check_file_exist(const char *path);


static void ota_backend_shakehand_timer(struct thread_timer *ttimer, void *para)
{
    struct ota_backend_letws_ota_stream *backend_uart = para;
	int err = 0;
	uint32_t processed_cmd;

	SYS_LOG_DBG("wait, ctx %p\n", backend_uart->svc_ctx);

	err = ota_at_stream_process_command(&backend_uart->svc_ctx, &processed_cmd);
	if (err) {
		return;
	}

	if(processed_cmd == OTA_CMD_H2D_REQUEST_UPGRADE){
		if(backend_uart->backend.cb)
			backend_uart->backend.cb(&backend_uart->backend, OTA_BACKEND_UPGRADE_STATE, 1);
		backend_uart->svc_ctx.read_timeout = 1000;
		backend_uart->svc_ctx.timer_start_flag = false;
		thread_timer_stop(ttimer);
	}else if(backend_uart->svc_ctx.timer_need_stop_flag){
		backend_uart->svc_ctx.timer_start_flag = false;
		thread_timer_stop(ttimer);
	}

	SYS_LOG_INF("wait ctx %p, cmd %d return %d\n", &backend_uart->svc_ctx, processed_cmd, err);

	return;

}



static const struct ota_backend_api ota_backend_api_letws_ota_stream = {
	.exit = ota_backend_letws_ota_stream_exit,
	.open = ota_backend_letws_ota_stream_open,
	.close = ota_backend_letws_ota_stream_close,
	.read = ota_backend_letws_ota_stream_read,
	.ioctl = ota_backend_letws_ota_stream_ioctl,
};

struct ota_backend *ota_backend_letws_ota_stream_init(ota_backend_notify_cb_t cb,
					struct ota_backend_letws_param *param)
{
	struct ota_backend_letws_ota_stream *backend_letws_ota_stream;
	struct letws_ota_stream_ota_ctx *svc_ctx;
	struct letws_ota_stream_param stream_uparam;
	int err;

	SYS_LOG_INF("init");

	backend_letws_ota_stream = mem_malloc(sizeof(struct ota_backend_letws_ota_stream));
	if (!backend_letws_ota_stream) {
		SYS_LOG_ERR("malloc failed");
		return NULL;
	}

	memset(backend_letws_ota_stream, 0x0, sizeof(struct ota_backend_letws_ota_stream));
	svc_ctx = &backend_letws_ota_stream->svc_ctx;
	os_sem_init(&svc_ctx->read_sem, 0, UINT_MAX);

	/* Just call stream_create once, for register spp/ble service
	 * not need call stream_destroy
	 */

    stream_uparam.sync_mode = false;
	stream_uparam.send_cb = param->send_cb;

	svc_ctx->letws_ota_stream = letws_ota_stream_create(&stream_uparam);
	if (!svc_ctx->letws_ota_stream) {
		SYS_LOG_ERR("stream_create failed");
	}

	svc_ctx->data_cbuf = (struct acts_ringbuf *)stream_uparam.cbuf;

	if (svc_ctx->letws_ota_stream) {
		err = stream_open(svc_ctx->letws_ota_stream, MODE_IN_OUT);
		if (err) {
			SYS_LOG_ERR("stream_open Failed");
			return NULL;
		} else {
			svc_ctx->letws_ota_stream_opened = 1;
		}
	}

	svc_ctx->send_buf = mem_malloc(OTA_SVC_SEND_BUFFER_SIZE);
	svc_ctx->send_buf_size = OTA_SVC_SEND_BUFFER_SIZE;
	svc_ctx->ota_unit_size = OTA_UNIT_SIZE;
	svc_ctx->state = PROT_STATE_IDLE;
	svc_ctx->frame_buffer = mem_malloc(OTA_UNIT_SIZE);
	svc_ctx->read_timeout = 3000;
	svc_ctx->timer_start_flag = true;

	ota_backend_init(&backend_letws_ota_stream->backend, OTA_BACKEND_TYPE_UART,
			 (struct ota_backend_api *)&ota_backend_api_letws_ota_stream, cb);

    thread_timer_init(&svc_ctx->shakehand_looper, ota_backend_shakehand_timer, backend_letws_ota_stream);
    thread_timer_start(&svc_ctx->shakehand_looper, SHAKEHAD_PERIOD, SHAKEHAD_PERIOD);

	return &backend_letws_ota_stream->backend;
}

int ota_backend_letws_rx_data_write(struct ota_backend *backend, uint8_t *data, uint32_t len)
{
	int ret_val;
	struct ota_backend_letws_ota_stream *stream = CONTAINER_OF(backend, struct ota_backend_letws_ota_stream, backend);

	struct letws_ota_stream_ota_ctx *svc_ctx = &stream->svc_ctx;

	if(!svc_ctx->data_cbuf){
		return -EIO;
	}

	ret_val = acts_ringbuf_put(svc_ctx->data_cbuf, data, len);

	os_sem_give(&svc_ctx->read_sem);

	return ret_val;
}
