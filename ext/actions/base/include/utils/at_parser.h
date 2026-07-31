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
#ifndef __CONFIG_AT_PARSER_H
#define __CONFIG_AT_PARSER_H

/**
 *@brief This enum defines the max length of at command&response, normal 256bytes.
 */
#define AT_PARSER_EXEC_CMD_MAX_LEN  (CONFIG_AT_PARSER_MAX_CMD_LEN)

/**
 *@brief This enum defines the max length of at command name,not include arguments
 */
#define AT_PARSER_CMD_NAME_MAX_LEN  (16)

#define AT_PARSER_DUMP_RAW_DATA
#define AT_PARSER_DUMP_RAW_CMD_DATA
#define AT_PARSER_DUMP_RAW_RESP_DATA

#define AT_PARSER_RESP_OK    "OK"
#define AT_PARSER_RESP_ERR   "ERR"
#define AT_PARSER_RESP_ERR_SYNTAX   			"ERR=1"
#define AT_PARSER_RESP_ERR_INVAL    			"ERR=2"
#define AT_PARSER_RESP_ERR_OP_FAIL  			"ERR=3"
#define AT_PARSER_RESP_ERR_CONNECT_LIMIT		"ERR=4"
#define AT_PARSER_RESP_ERR_WROING_AUDIO_STATE 	"ERR=5"
#define AT_PARSER_RESP_ERR_A2DP_STATE       	"ERR=6"
#define AT_PRASER_RESP_ERR_AVRCP_STATE      	"ERR=7"
#define AT_PRASER_RESP_ERR_HFP_STATE        	"ERR=8"
#define AT_PARSER_RESP_ERR_NO_ACTIVE_CALL   	"ERR=9"


/**
 *@brief AT command response code
 */
typedef enum {
    AT_RESP_OK = 0,
    AT_RESP_ERROR,
    AT_RESP_FULL,
    AT_RESP_TIMEOUT,
} at_resp_code_e;

struct at_parser_response{
    const char *resp_prefix;
    const char *resp_suffix;
    uint32_t timeout;
	char *resp_buf;
	uint16_t resp_max_len;
	uint8_t resp_line_count; //0 if single command !=0 multi command
    at_resp_code_e code;
};

#ifdef CONFIG_AT_PARSER_SUPPORT_URC
struct at_parser_urc_info{
	uint8_t *urc_buf;
	uint32_t urc_buf_size;
};

struct at_parser_urc_item{
	const char *prefix;
	int (*handler)(struct at_parser_urc_info *info);
};
#endif

struct at_parser_ctx;

struct at_parser_cmd_item{
	const char *prefix;
	int (*test)(struct at_parser_ctx *ctx);
	int (*query)(struct at_parser_ctx *ctx);
	int (*setup)(struct at_parser_ctx * ctx, const uint8_t * args);
	int (*exec)(struct at_parser_ctx *ctx);
};


/**
 * @brief Statically define and initialize at command entry for at parser.
 *
 * The at command entry define statically,
 *
 * Each at command must define the command entry info so that the system wants
 * to parser at command to knoe the corresponding information
 *
 * @param at_name Name of the at command.
 * @param _test_ test at command function.
 * @param _query_ query at command function.
 * @param _setup_ setup at command function .
 * @param _exec_ exec at command function.
 */
#define AT_COMMAND_DEFINE(at_name, _test_, _query_, _setup_, _exec_)	\
        const struct at_parser_cmd_item __at_command_entry_##_test_##_query_##_setup_##_exec_	\
		__attribute__((__section__(".at_command_entry"))) __attribute__((used)) = {	\
		.prefix = at_name,					\
		.test = _test_,					\
		.query = _query_,		\
		.setup = _setup_,		    \
		.exec = _exec_,			\
	}


struct at_parser_ctx *at_parser_init(const char *dev_name, uint32_t recv_buf_size);

int at_parser_send_command(struct at_parser_ctx *ctx, struct at_parser_response *response, const char *cmd);

int at_parser_send_multi_command(struct at_parser_ctx *ctx, struct at_parser_response *response, const char **cmd_array);

int at_parser_send_exec_command(struct at_parser_ctx *ctx, struct at_parser_response *response, const char *cmd, ...);

int at_parser_send_resp(struct at_parser_ctx *ctx, const char *resp_str);

int at_parser_send_exec_resp(struct at_parser_ctx *ctx, const char *resp_str, ...);

int at_parser_parse_args(const char *buf, const char *fmt, ...);

#ifdef CONFIG_AT_PARSER_SUPPORT_URC
void at_parser_set_urc_table(struct at_parser_ctx *ctx, const struct at_parser_urc_item *urc_table, uint32_t urc_table_size);
#endif

int at_parser_receive_process(struct at_parser_ctx *ctx);

int at_parser_wait_connect(struct at_parser_ctx *ctx, uint32_t timeout);

#endif
