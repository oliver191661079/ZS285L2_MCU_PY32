/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _MAIN_APP_H_
#define _MAIN_APP_H_
#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef CONFIG_TWS_UI_EVENT_SYNC
#include <thread_timer.h>
#include "../include/list.h"
#endif

#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "main"
#ifdef SYS_LOG_LEVEL
#undef SYS_LOG_LEVEL
#endif
#include <logging/sys_log.h>
#include "desktop_manager.h"

#ifdef CONFIG_OTA_APP
#include "../ota/ota_app.h"
#endif
#ifdef CONFIG_USB_HOST
#include <usb/usb_host.h>
#endif

#ifdef CONFIG_TWS_UI_EVENT_SYNC
enum
{
    START_PLAY_LED = 0,
    START_STOP_LED,
    START_PLAY_TONE,
    START_PLAY_VOICE
};

struct notify_sync
{
    u8_t event_type;
    u8_t event_stage;
    u64_t sync_time;
} __packed;

typedef struct notify_sync notify_sync_t;

typedef struct
{
    u8_t set_led_display : 1;
    u8_t set_tone_play : 1;
    u8_t set_voice_play : 1;

    u8_t start_tone_play : 1;
    u8_t start_voice_play : 1;

    u8_t end_led_display : 1;
    u8_t end_tone_play : 1;
    u8_t end_voice_play : 1;

    u8_t disable_tone : 1;
    u8_t disable_voice : 1;

    u8_t led_disp_only : 1;
    u8_t need_wait_end : 1;
    u8_t list_deleted : 1;
    u8_t is_tws_mode : 1;

} system_notify_flags_t;

typedef struct
{
    struct list_head node;
    u8_t nesting;
    system_notify_flags_t flags;
    unsigned int tag;
    u8_t Event_Type;
} system_notify_ctrl_t;

void system_do_event_notify(u32_t ui_event);

#endif


void main_key_event_handle(uint32_t key_event);
void main_input_event_handle(struct app_msg *msg);
int main_bt_event_handle(struct app_msg *msg);
int main_tws_event_handle(struct app_msg *msg);
void main_hotplug_event_handle(struct app_msg *msg);

extern void main_app_view_init(void);
extern void main_app_view_exit(void);
extern int main_app_ui_event(int event);

void main_app_volume_show(struct app_msg *msg);
void main_app(void);

#endif
