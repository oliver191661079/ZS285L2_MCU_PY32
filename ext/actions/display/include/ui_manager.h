/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file ui manager interface
 */

#ifndef __UI_MANGER_H__
#define __UI_MANGER_H__
#ifdef CONFIG_SEG_LED_MANAGER
#include <seg_led_manager.h>
#endif
#ifdef CONFIG_LED_MANAGER
#include <led_manager.h>
#endif

#include "ugui.h"
#include "display/led_display.h"
/**
 * @defgroup ui_manager_apis app ui Manager APIs
 * @ingroup system_apis
 * @{
 */

typedef u32_t  ui_region_t;

#define TRANSMIT_ALL_KEY_EVENT  0xFFFFFFFF

#define UI_REGION_ALL  ((ui_region_t)-1)

typedef int (*ui_view_proc_t)(u8_t view_id, u8_t msg_id, u32_t msg_data);

typedef int (*ui_get_state_t)(void);

typedef struct {
    /** key value, which key is pressed */
	u32_t key_val;

    /** key type, which key type of the pressed key */
	u32_t key_type;

    /** app state, the state of app service to handle the message */
	u32_t app_state;

    /** app msg, the message of app service will be send */
	u32_t app_msg;

    /** key policy */
	u32_t key_policy;

} ui_key_map_t;


enum UI_VIEW_FLAGS {
	UI_FLAG_SET_FOCUS    = (1 << 0),
	UI_FLAG_HOLD_DISPLAY = (1 << 1),
	UI_FLAG_TOP_MOST     = (1 << 2),
};


typedef struct {
	ui_region_t     region;
	ui_view_proc_t  view_proc;
	ui_get_state_t  view_get_state;
	const ui_key_map_t	*view_key_map;
	void		*app_id;
	u16_t	flags;
	u16_t	order;
} ui_view_info_t;

typedef struct {
	sys_snode_t  node;
	void   *app_id;
	ui_view_info_t  info;
	u8_t   view_id;
	u8_t   led_model_id;
	u16_t  disp_leds;
} ui_view_context_t;


typedef struct {
	sys_slist_t  view_list;
	ui_region_t  update_region;

#ifdef CONFIG_GUI
	UG_GUI gui;
#endif

} ui_manager_context_t;

enum UI_MSG_ID {
	MSG_VIEW_CREATE,
	MSG_VIEW_DELETE,
	MSG_VIEW_PAINT,
	MSG_SET_FOCUS,
	MSG_KILL_FOCUS,
	MSG_KEY_EVENT,
	MSG_VIEW_RECOVER,
};

/**
 * @brief Create a new view
 *
 * This routine Create a new view
 *
 * @param view_id init view id
 * @param info init view param &ui_view_info_t
 *
 * @return true if invoked succsess.
 * @return false if invoked failed.
 */

bool ui_view_create(u32_t view_id, ui_view_info_t *info);

/**
 * @brief destory view
 *
 * This routine destory view, delete form ui manager view list
 * and release all resource for view.
 *
 * @param view_id id of view
 *
 * @return true if invoked succsess.
 * @return false if invoked failed.
 */
bool ui_view_delete(u32_t view_id);

/**
 * @brief update display
 *
 * This routine update display, flush all change view to display.
 *
 * @return N/a
 */

void ui_display_update(void);
/**
 * @brief set view order
 *
 * This routine set view order, The higher the order value of the view,
 *  the higher the view
 *
 * @param view_id id of view
 *
 * @return true if invoked succsess.
 * @return false if invoked failed.
 */

bool ui_view_set_order(u32_t view_id, u32_t order);
/**
 * @cond INTERNAL_HIDDEN
 */
/**
 * @brief dispatch key event to target view
 *
 * This routine dispatch key event to target view, ui manager will found the target
 * view by view_key_map
 *
 * @param key_event value of key event
 *
 * @return 0 if invoked succsess.
 * @return others if invoked failed.
 */
int ui_manager_dispatch_key_event(u32_t key_event);
/**
 * INTERNAL_HIDDEN @endcond
 */
/**
 * @brief send ui message to target view
 *
 * This routine send ui message to target view which mark by view id
 *
 * @param view_id id of view
 * @param msg_id id of msg_id
 * @param msg_data param of message
 *
 * @return 0 if invoked succsess.
 * @return others if invoked failed.
 */
int ui_message_send_async(u32_t view_id, u32_t msg_id, u32_t msg_data);

/**
 * @brief dispatch ui message to target view
 *
 * This routine dispatch ui message to target view which mark by view id
 * if the target view not found in view list ,return failed. otherwise target
 * view will process this ui message.
 *
 * @param view_id id of view
 * @param msg_id id of msg_id
 * @param msg_data param of message
 *
 * @return 0 if invoked succsess.
 * @return others if invoked failed.
 */
int ui_message_dispatch(u32_t view_id, u32_t msg_id, u32_t msg_data);

/**
 * @brief ui manager init funcion
 *
 * This routine calls init ui manager ,called by main
 *
 * @return 0 if invoked succsess.
 * @return others if invoked failed.
 */

int ui_manager_init(void);

/**
 * @brief ui manager deinit funcion
 *
 * This routine calls deinit ui manager ,called by main
 *
 * @return 0 if invoked succsess.
 * @return others if invoked failed.
 */

int ui_manager_exit(void);

/**
 * @cond INTERNAL_HIDDEN
 */
bool ui_view_all_update(ui_region_t region);
bool ui_view_update(u32_t view_id, ui_region_t region);
bool ui_view_set_region(u32_t view_id, ui_region_t region);
bool ui_view_set_order(u32_t view_id, u32_t order);
int ui_view_get_focus(void);

static inline ui_region_t ui_region_clip(ui_region_t region1, ui_region_t region2)
{
	return (region1 & ~region2);
}

static inline ui_region_t ui_region_intersect(ui_region_t region1, ui_region_t region2)
{
	return (region1 & region2);
}

static inline ui_region_t ui_region_merge(ui_region_t region1, ui_region_t region2)
{
	return (region1 | region2);
}

static inline bool ui_region_valid(ui_region_t region)
{
	return (region != 0);
}
/**
 * INTERNAL_HIDDEN @endcond
 */
/**
 * @} end defgroup system_apis
 */
#endif
