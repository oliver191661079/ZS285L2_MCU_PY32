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
#include <kernel.h>
#include "at_parser.h"


static int cmd_at_exec(struct at_parser_ctx* ctx)
{
    at_parser_send_resp(ctx, AT_PARSER_RESP_OK);
    return 0;
}

AT_COMMAND_DEFINE("AT", NULL, NULL, NULL, cmd_at_exec);

static int cmd_at_get_version_test(struct at_parser_ctx* ctx)
{
    at_parser_send_exec_resp(ctx, "VER=%s", "1.00");
    return 0;
}

AT_COMMAND_DEFINE("AT+VER", cmd_at_get_version_test, NULL, NULL, NULL);

