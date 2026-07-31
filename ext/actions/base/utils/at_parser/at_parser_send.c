/*******************************************************************************
 *                                      US283C
 *                            Module: usp Driver
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>    <time>           <version >             <desc>
 *       wuyufan    2023-4-27  11:39     1.0             build this file
*******************************************************************************/
/*!
 * \file     at_parser.c
 * \brief    at parser
 * \author   wuyufan
 * \par      GENERAL DESCRIPTION:
 *               function related to at parser
 * \par      EXTERNALIZED FUNCTIONS:
 *
 * \version 1.0
 * \date  2023-4-27
*******************************************************************************/
#include "at_parser_inner.h"
#include "misc/printk.h"

static int at_parser_get_padding_char_bytes(uint8_t mode)
{
	int add_bytes = 0;
	switch(mode){
		case AT_CHAR_MODE_WITH_CR:
			add_bytes = 1;
			break;

		case AT_CHAR_MODE_WITH_LF:
			add_bytes = 1;
			break;

		case AT_CHAR_MODE_WITH_CRLF:
			add_bytes = 2;
			break;
	}

	return add_bytes;
}

static int at_parser_add_padding_char_bytes(uint8_t mode, uint8_t *data)
{
	int add_bytes = 0;
	switch(mode){
		case AT_CHAR_MODE_WITH_CR:
			data[add_bytes++] = AT_PARSER_CR_C;
			break;

		case AT_CHAR_MODE_WITH_LF:
			data[add_bytes++] = AT_PARSER_LF_C;
			break;

		case AT_CHAR_MODE_WITH_CRLF:
			data[add_bytes++] = AT_PARSER_CR_C;
			data[add_bytes++] = AT_PARSER_LF_C;
			break;
	}

	return add_bytes;
}

static int at_parser_send_cmd_data(struct at_parser_ctx *ctx, const char *data, uint32_t len)
{
    uint16_t index = 0;

	uint8_t padding_bytes;

	padding_bytes = at_parser_get_padding_char_bytes(ctx->end_char_mode);

	//add with CR
	if(len > (AT_PARSER_EXEC_CMD_MAX_LEN - padding_bytes)){
		return -ENOMEM;
	}

	memcpy(ctx->send_buf, data, len);
	index += len;

    index += at_parser_add_padding_char_bytes(ctx->end_char_mode, &ctx->send_buf[index]);

    at_parser_dev_write(ctx, ctx->send_buf, index);

#ifdef AT_PARSER_DUMP_RAW_CMD_DATA
	at_parser_dump_data("raw cmd", ctx->send_buf, index);
#endif	

    return 0;
}

static int at_parser_send_response_data(struct at_parser_ctx *ctx, const char *data, uint32_t len)
{
    uint16_t index = 0;

	uint8_t lead_padding_bytes;
	uint8_t end_padding_bytes;

	lead_padding_bytes = at_parser_get_padding_char_bytes(ctx->start_response_char_mode);
	end_padding_bytes = at_parser_get_padding_char_bytes(ctx->end_char_mode);

    if(len > (AT_PARSER_EXEC_CMD_MAX_LEN - (lead_padding_bytes + end_padding_bytes))){
        return -ENOMEM;
    }

	at_parser_add_padding_char_bytes(ctx->start_response_char_mode, ctx->send_buf);
	index += lead_padding_bytes;

    memcpy(&ctx->send_buf[index], data, len);
    index += len;

    at_parser_add_padding_char_bytes(ctx->end_char_mode, &ctx->send_buf[index]);
	index += end_padding_bytes;

    at_parser_dev_write(ctx, ctx->send_buf, index);

#ifdef AT_PARSER_DUMP_RAW_CMD_DATA
	at_parser_dump_data("raw resp", ctx->send_buf, index);
#endif
    return 0;
}


int at_parser_send_command(struct at_parser_ctx *ctx, struct at_parser_response *response, const char *cmd)
{
	int ret_val;

#ifdef AT_PARSER_DUMP_RAW_CMD_DATA
	at_parser_dump_data("cmd", cmd, strlen(cmd));
#endif

	at_mutex_lock(&ctx->at_mutex, AT_TIMEOUT_FOREVER);

	if(response){
		ctx->response = response;
		ctx->cur_resp_recv_len = 0;
		ctx->cur_resp_line_count = 0;
	}

	ret_val = at_parser_send_cmd_data(ctx, cmd, strlen(cmd));

	if(ret_val){
		at_parser_print_result(ctx, __LINE__, AT_PARSER_SEND_CMD_ERR);
		at_mutex_unlock(&ctx->at_mutex);
		return ret_val;
	}

	if(response){
		if(at_sem_take(&ctx->resp_sem, response->timeout) < 0){
			response->code = AT_RESP_TIMEOUT;
			ret_val = -ETIME;
		}
	}

	at_mutex_unlock(&ctx->at_mutex);

    return ret_val;
}

int at_parser_send_multi_command(struct at_parser_ctx *ctx, struct at_parser_response *response, const char **cmd_array)
{
	int i;
	int ret_val = 0;

#ifdef AT_PARSER_DUMP_RAW_CMD_DATA
	for(i = 0; cmd_array[i] != NULL; i++){
		at_parser_dump_data("cmd", cmd_array[i], strlen(cmd_array[i]));
	}
#endif

	at_mutex_lock(&ctx->at_mutex, AT_TIMEOUT_FOREVER);

	if(response){
		ctx->response = response;
		ctx->cur_resp_recv_len = 0;
		ctx->cur_resp_line_count = 0;
	}

	for(i = 0; cmd_array[i] != NULL; i++){
		ret_val = at_parser_send_cmd_data(ctx, cmd_array[i], strlen(cmd_array[i]));

		if(ret_val){
			at_parser_print_result(ctx, __LINE__, AT_PARSER_SEND_CMD_ERR);
			at_mutex_unlock(&ctx->at_mutex);
			return ret_val;
		}
	}

	if(response){
		if(at_sem_take(&ctx->resp_sem, response->timeout) < 0){
			response->code = AT_RESP_TIMEOUT;
			ret_val = -ETIME;
		}
	}

	at_mutex_unlock(&ctx->at_mutex);

    return ret_val;
}


int at_parser_send_exec_command(struct at_parser_ctx *ctx, struct at_parser_response *response, const char *cmd, ...)
{
    int exec_len;
    uint8_t exec_buf[AT_PARSER_EXEC_CMD_MAX_LEN];

    va_list args;

    va_start(args, cmd);

    exec_len = at_vsnprintk(exec_buf, AT_PARSER_EXEC_CMD_MAX_LEN, cmd, args);

    if(!exec_len){
        return -EINVAL;
    }

	return at_parser_send_command(ctx, response, exec_buf);
}


int at_parser_send_resp(struct at_parser_ctx *ctx, const char *resp_str)
{
	int ret_val;

#ifdef AT_PARSER_DUMP_RAW_RESP_DATA
	at_parser_dump_data("resp", resp_str, strlen(resp_str));
#endif

	at_mutex_lock(&ctx->at_mutex, AT_TIMEOUT_FOREVER);

	ret_val = at_parser_send_response_data(ctx, resp_str, strlen(resp_str));

	if(ret_val){
		at_parser_print_result(ctx, __LINE__, AT_PARSER_SEND_RESP_ERR);
		at_mutex_unlock(&ctx->at_mutex);
		return ret_val;
	}

	at_mutex_unlock(&ctx->at_mutex);

    return ret_val;
}

int at_parser_send_exec_resp(struct at_parser_ctx *ctx, const char *resp_str, ...)
{
    int exec_len;
    uint8_t exec_buf[AT_PARSER_EXEC_CMD_MAX_LEN];
    va_list args;

    va_start(args, resp_str);

    exec_len = at_vsnprintk(exec_buf, AT_PARSER_EXEC_CMD_MAX_LEN, resp_str, args);

	return at_parser_send_resp(ctx, exec_buf);
}


