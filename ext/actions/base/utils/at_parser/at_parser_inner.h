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
#ifndef __CONFIG_AT_PARSER_INNER_H
#define __CONFIG_AT_PARSER_INNER_H

#include "at_parser_hal.h"


/**
 *@brief This enum defines the input CMD mode.
 */
typedef enum {
    AT_PARSER_CMD_MODE_QUERY,        /**<Query mode command, such as "AT+CMD?". */
    AT_PARSER_CMD_MODE_EXEC,         /**< Exec mode command, such as "AT+CMD". */
    AT_PARSER_CMD_MODE_SETUP,        /**<Setup mode command, such as "AT+CMD=<op>". */
    AT_PARSER_CMD_MODE_TEST,        /**< Test mode command, such as "AT+CMD=?". */
    AT_PARSER_CMD_MODE_INVALID,      /**< The input command doesn't belong to any of the four types. */
} at_parser_cmd_mode_e;


typedef enum  {
	AT_PARSER_OK,
	AT_PARSER_CMD_ERR,
	AT_PARSER_CMD_ARGS_ERR,
	AT_PARSER_URC_ERR,
	AT_PARSER_SEND_CMD_ERR,
	AT_PARSER_SEND_RESP_ERR,
}at_parser_result_e;


struct at_parser_ctx{
    uint8_t *data_ptr;
    uint32_t data_len;

	uint8_t *send_buf;

#ifdef CONFIG_AT_PARSER_SUPPORT_URC
	//const struct at_parser_urc_item *urc_item;
	const struct at_parser_urc_item *urc_table;
	uint32_t urc_table_size;
#endif

	uint8_t cmd_name_len;
	uint8_t cmd_mode;
	uint8_t end_char_mode;
	uint8_t start_response_char_mode;

	//const struct at_parser_cmd_item *item;
	const struct at_parser_cmd_item *cmd_table;
	uint32_t cmd_table_size;

	at_mutex_t at_mutex;
	at_sem_t resp_sem;
	struct at_parser_response *response;
	uint16_t cur_resp_recv_len;
	uint8_t cur_resp_line_count;

	void *hal_handle;
};

void at_parser_dump_data(char *str, const void *buf, int size);

void at_parser_print_result(struct at_parser_ctx *ctx, int linenum, at_parser_result_e result);

#endif
