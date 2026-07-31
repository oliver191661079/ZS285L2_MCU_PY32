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
#include "errno.h"
#include "kernel.h"


static struct at_parser_ctx g_at_parser_ctx;

extern struct at_parser_cmd_item __at_cmd_table_start[];
extern struct at_parser_cmd_item __at_cmd_table_end[];

extern int vsscanf(const char * buf, const char * fmt, va_list args);

static struct at_parser_ctx *get_at_parser_ctx(void)
{
	return &g_at_parser_ctx;
}

void at_parser_print_result(struct at_parser_ctx *ctx, int linenum, at_parser_result_e result)
{
	printk("parser %d result %d\n", linenum, result);
}

int at_parser_parse_args(const char *buf, const char *fmt, ...)
{
    va_list args;
    int req_args_num = 0;

    va_start(args, fmt);

    req_args_num = vsscanf(buf, fmt, args);

    va_end(args);

    return req_args_num;
}

void at_parser_dump_data(char *str, const void *buf, int size)
{
	printk("%s\n", str);
	print_buffer(buf, 1, size, 16, -1);
}


void at_parser_read_callback(struct at_parser_ctx *ctx)
{
	at_parser_receive_process(ctx);
}

struct at_parser_ctx *at_parser_init(const char *dev_name, uint32_t recv_buf_size)
{
	int ret_val;

	struct at_parser_ctx *ctx = get_at_parser_ctx();

	ctx->data_ptr = at_malloc(AT_PARSER_EXEC_CMD_MAX_LEN);

	ctx->send_buf = at_malloc(AT_PARSER_EXEC_CMD_MAX_LEN);

	ctx->end_char_mode = AT_CHAR_MODE_WITH_CRLF;

	ctx->start_response_char_mode = AT_CHAR_MODE_WITH_NULL;

	ctx->cmd_table = (const struct at_parser_cmd_item *)&__at_cmd_table_start;

	ctx->cmd_table_size = ((uint32_t)&__at_cmd_table_end - (uint32_t)&__at_cmd_table_start) /sizeof(struct at_parser_cmd_item);

	at_mutex_init(&ctx->at_mutex);

	at_sem_init(&ctx->resp_sem, 0, 1);

	ret_val = at_parser_dev_init(ctx, dev_name, recv_buf_size, at_parser_read_callback);

	if(ret_val != 0){
		return NULL;
	}


	return ctx;
}

int at_parser_wait_connect(struct at_parser_ctx *ctx, uint32_t timeout_ms)
{
    int ret_val;
    struct at_parser_response response = {0};
    int start_time;

	response.timeout = 1000;
	response.resp_max_len = 8;

    start_time = os_uptime_get();

    while (1){

        if (os_uptime_get() - start_time > timeout_ms){
            SYS_LOG_ERR("wait connect timeout (%d)!", timeout_ms);
            ret_val = -ETIME;
            break;
        }

        ret_val = at_parser_send_command(ctx, &response, "AT");

		printk("send cmd result %d\n", ret_val);

		if(ret_val == 0){
			break;
		}
    }

    return ret_val;
}

int at_parser_read_version(struct at_parser_ctx *ctx, uint32_t timeout_ms)
{
    int ret_val;
    struct at_parser_response response = {0};
    int start_time;
	uint8_t resp_buf[16];
	uint8_t ver_buf[8];

	response.timeout = 1000;
	response.resp_buf = resp_buf;
	response.resp_max_len = sizeof(resp_buf);
	response.resp_prefix = "VER=";

    start_time = os_uptime_get();

    while (1){

        if (os_uptime_get() - start_time > timeout_ms){
            SYS_LOG_ERR("wait connect timeout (%d)!", timeout_ms);
            ret_val = -ETIME;
            break;
        }

        ret_val = at_parser_send_command(ctx, &response, "AT+VER=?");

		if(ret_val == 0){
			at_parser_parse_args(resp_buf, "VER=%s", ver_buf);
			printk("read version %s\n", ver_buf);
			break;
		}
    }

    return ret_val;
}

