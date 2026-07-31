/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file bt service base define.
 * Not need direct include this head file, just need include <btservice_api.h>.
 * this head file include by btservice_api.h
 */

#ifndef _BTSERVICE_BASE_H_
#define _BTSERVICE_BASE_H_

#define __IN_BT_SECTION	__in_section_unique(bthost_bss)

/** bluetooth device address */
typedef struct {
	uint8_t  val[6];
} bd_address_t;

//#define CONFIG_SUPPORT_TWS_ROLE_CHANGE
#define CONFIG_SNOOP_LINK_TWS 1

//max length of the local bt name
#define CONFIG_MAX_BT_NAME_LEN             32
//max length of the remote bt device name
#define CONFIG_MAX_REMOTE_BT_NAME_LEN      248

#endif
