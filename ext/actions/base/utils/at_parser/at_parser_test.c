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
#include <device.h>
#include <init.h>
#include "at_parser.h"

int at_parser_read_version(struct at_parser_ctx *ctx, uint32_t timeout_ms);

static int at_parser_test(struct device *arg)
{
    int ret_val;

    struct at_parser_ctx *ctx = at_parser_init(CONFIG_AT_PARSER_ON_DEV_NAME, 4096);

    if(!ctx){
        printk("parser init failed\n");
        return -EINVAL;
    }

#if 1
    ret_val = at_parser_wait_connect(ctx, 10000);

    if(ret_val != 0){
        printk("parser wait connect failed\n");
    } 

    at_parser_read_version(ctx, 10000);
#endif

    return 0;
}

SYS_INIT(at_parser_test, APPLICATION, 20);



