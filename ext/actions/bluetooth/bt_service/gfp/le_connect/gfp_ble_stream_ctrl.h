/*
 * Copyright (c) 2019 Actions Semi Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file 
 * @brief 
 *
 * Change log:
 *
 */

#ifndef __GFP_BLE_STREAM_CTRL_
#define __GFP_BLE_STREAM_CTRL_

#ifndef BOOL
#define BOOL int
#endif

#ifndef SYS_LOG_DOMAIN
#define SYS_LOG_DOMAIN "gfp_ble"
#endif

#ifndef SYS_LOG_LEVEL
#define SYS_LOG_LEVEL 3	/* SYS_LOG_LEVEL_INFO */
#endif

void gfp_auth_timeout_start(u32_t duration);
void gfp_auth_timeout_stop(void);
int gfp_send_pkg_to_stream(u8_t *data_ptr, u16_t length, int num);
int gfp_uuid_tx_chrc_attr_get(u8_t num);
int gfp_uuid_tx_attr_get(u8_t num);
void gfp_auth_timeout_start(u32_t duration);
void gfp_auth_timeout_stop(void);
u8_t gfp_uuid_num_get(void *attr);
int gfp_uuid_tx_chrc_attr_get(u8_t num);
int gfp_uuid_tx_attr_get(u8_t num);

#endif /*__GFP_BLE_STREAM_CTRL_*/






