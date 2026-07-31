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
 * \file     at_parser_hal_zephyr.h
 * \brief    at parser
 * \author   wuyufan
 * \par      GENERAL DESCRIPTION:
 *               function related to at parser
 * \par      EXTERNALIZED FUNCTIONS:
 *
 * \version 1.0
 * \date  2023-4-27
*******************************************************************************/
#ifndef __AT_PARSER_HAL_ZEPHYR_H
#define __AT_PARSER_HAL_ZEPHYR_H

#include <kernel.h>

#define AT_TIMEOUT_FOREVER K_FOREVER

typedef struct k_sem  at_sem_t;

typedef struct k_mutex at_mutex_t;

#define at_malloc       mem_malloc
#define at_free         mem_free
#define os_uptime_get   k_uptime_get_32
#define at_vsnprintk    vsnprintk

#endif
