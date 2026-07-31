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
 * \file     at_parser_hal.h
 * \brief    at parser
 * \author   wuyufan
 * \par      GENERAL DESCRIPTION:
 *               function related to at parser
 * \par      EXTERNALIZED FUNCTIONS:
 *
 * \version 1.0
 * \date  2023-4-27
*******************************************************************************/
#ifndef __AT_PARSER_HAL_H
#define __AT_PARSER_HAL_H

#if defined(CONFIG_AT_PARSER_ENABLE_ZEPHYR_HAL)
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <kernel.h>
#include <mem_manager.h>
#include <uart_at_stream.h>
#include <at_parser.h>
#include <logging/sys_log.h>
#include "zephyr_hal/at_parser_hal_zephyr.h"
#else
#include <stdio.h>
#include <string.h>
#include <kernel.h>
#include <os_api.h>
#include <uart_at_stream.h>
#include <at_parser.h>
#include "freertos_hal/at_parser_hal_freertos.h"
#include <sys_log.h>
#endif
#endif

struct at_parser_ctx;

void at_mutex_init(at_mutex_t *mutex);

int at_mutex_lock(at_mutex_t *mutex, uint32_t timeou);

void at_mutex_unlock(at_mutex_t *mutex);

void at_sem_init(at_sem_t *sem, uint32_t init_count, uint32_t limit);

int at_sem_take(at_sem_t *sem, uint32_t timeout);

void at_sem_give(at_sem_t *sem);

int at_parser_read_line(struct at_parser_ctx *ctx, uint8_t *data, uint32_t max_line_len);

int at_parser_dev_init(struct at_parser_ctx *ctx, const char * dev_name, uint32_t recv_buf_size, void (*read_cb)(struct at_parser_ctx *ctx));

int at_parser_dev_write(struct at_parser_ctx *ctx, const void *buf, uint32_t len);
