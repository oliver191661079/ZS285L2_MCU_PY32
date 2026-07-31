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
#include "../at_parser_inner.h"
#include <uart_at_stream.h>

struct at_zephyr_hal{
	void *at_stream;
	struct k_sem read_sem;
};

static struct at_zephyr_hal g_at_hal_zephyr;

void at_mutex_init(at_mutex_t *mutex)
{
	k_mutex_init(mutex);
}

int at_mutex_lock(at_mutex_t *mutex, uint32_t timeout)
{
	return k_mutex_lock(mutex, timeout);
}

void at_mutex_unlock(at_mutex_t *mutex)
{
	k_mutex_unlock(mutex);
}

void at_sem_init(at_sem_t *sem, uint32_t init_count, uint32_t limit)
{
	k_sem_init(sem, init_count, limit);
}

int at_sem_take(at_sem_t *sem, uint32_t timeout)
{
	return k_sem_take(sem, timeout);
}

void at_sem_give(at_sem_t *sem)
{
	k_sem_give(sem);
}

int at_parser_read_line(struct at_parser_ctx *ctx, uint8_t *data, uint32_t max_line_len)
{
	struct at_zephyr_hal *priv = (struct at_zephyr_hal *)ctx->hal_handle;

	int stream_len = at_stream_get_length(priv->at_stream);

	if(!stream_len){
		return 0;
	}

	return at_stream_read(priv->at_stream, ctx->data_ptr, max_line_len);

}


int at_parser_dev_init(struct at_parser_ctx *ctx, const char * dev_name, uint32_t recv_buf_size, void (*read_cb)(struct at_parser_ctx *ctx))
{
	struct uart_at_stream_param param;

	param.dev_name = dev_name;
	param.recv_buf_size = recv_buf_size;
	param.read_sem = (void *)&g_at_hal_zephyr.read_sem;
	param.end_char_mode = ctx->end_char_mode;
	param.obj_type = 0;
	param.priv_data = (void *)ctx;
	param.read_cb = (void *)read_cb;
	g_at_hal_zephyr.at_stream = at_stream_init(&param);

	if(!g_at_hal_zephyr.at_stream){
		return -EINVAL;
	}

	at_stream_open(g_at_hal_zephyr.at_stream);

	ctx->hal_handle = (void *)&g_at_hal_zephyr;

	return 0;
}

int at_parser_dev_write(struct at_parser_ctx *ctx, const void *buf, uint32_t len)
{
	struct at_zephyr_hal *priv = (struct at_zephyr_hal *)ctx->hal_handle;

    return at_stream_write(priv->at_stream, (unsigned char *)buf, len);
}


