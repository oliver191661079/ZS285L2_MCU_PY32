/*
 * Copyright (c) 2022 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _APP_HOT_H_
#define _APP_HOT_H_

typedef enum
{
	APP_SWITCH_ADD,
	APP_SWITCH_DEL,
	APP_SWITCH_REPLACE,
} APP_SWITCH_MODE;

bool system_app_launch_add(u8_t plugin);
bool system_app_launch_del(u8_t plugin);
bool system_app_launch_switch(u8_t from, u8_t to);

void system_app_send_input_event(u8_t cmd, u32_t value);

int system_app_launch_init(void);


uint8_t system_app_get_auracast_mode(void);
#ifdef CONFIG_AURACAST
void system_app_set_auracast_mode(int mode);
void system_app_switch_auracast(bool auracast);
#endif

#endif
