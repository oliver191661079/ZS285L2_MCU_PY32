/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file ui manager interface
 */

#define SYS_LOG_NO_NEWLINE
#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "ui_manager"
#include <logging/sys_log.h>
#include <os_common_api.h>
#include <msg_manager.h>
#include <mem_manager.h>
#include <app_manager.h>
#include <input_manager.h>
#include <string.h>
#include "framebuffer.h"
#include "ui_manager.h"

#ifdef CONFIG_SEG_LED_MANAGER
#include <seg_led_manager.h>
#endif

#ifdef CONFIG_ESD_MANAGER
#include <esd_manager.h>
#endif

#ifdef CONFIG_LED_MANAGER
#include <led_manager.h>
#endif

#ifdef CONFIG_PLAYTTS
#include <tts_manager.h>
#endif

ui_manager_context_t  *ui_manager_context;

static ui_manager_context_t *_ui_manager_get_context(void)
{
	return ui_manager_context;
}

static void _ui_manager_add_view(ui_view_context_t *view)
{
	ui_view_context_t *cur_view;
	ui_view_context_t *pre_view = NULL;
	ui_manager_context_t *ui_manager = _ui_manager_get_context();

	SYS_SLIST_FOR_EACH_CONTAINER(&ui_manager->view_list, cur_view, node) {
		if (view->info.order > cur_view->info.order) {
			sys_slist_insert(&ui_manager->view_list, &pre_view->node, &view->node);
			goto end;
		}
		pre_view = cur_view;
	}

	sys_slist_append(&ui_manager->view_list, (sys_snode_t *)view);

end:
	return;
}

static ui_view_context_t *_ui_manager_get_view_context(u32_t view_id)
{
	ui_view_context_t *view;

	ui_manager_context_t *ui_manager = _ui_manager_get_context();

	SYS_SLIST_FOR_EACH_CONTAINER(&ui_manager->view_list, view, node) {
		if (view->view_id == view_id)
			return view;
	}

	return NULL;
}

static int _ui_manager_get_view_index(u32_t view_id)
{
	ui_manager_context_t *ui_manager = _ui_manager_get_context();
	ui_view_context_t *view;
	int  index = 0;

	SYS_SLIST_FOR_EACH_CONTAINER(&ui_manager->view_list, view, node) {
		if (view->view_id == view_id) {
			break;
		}
		index += 1;
	}

	return index;
}

bool ui_view_create(u32_t view_id, ui_view_info_t *info)
{
	ui_view_context_t *view;

	view = _ui_manager_get_view_context(view_id);

	if (view != NULL)
		return false;

	view = mem_malloc(sizeof(ui_view_context_t));

	if (!view)
		return false;

	memset(view, 0, sizeof(ui_view_context_t));

	view->view_id = view_id;

	view->app_id = info->app_id;

	memcpy(&view->info, info, sizeof(ui_view_info_t));

	_ui_manager_add_view(view);

#ifdef CONFIG_SEG_LED_MANAGER
	/* unlock seg led */
	seg_led_manager_set_timeout_event(0, NULL);
#endif

	ui_message_send_async(view_id, MSG_VIEW_CREATE, 0);

	ui_view_update(view_id, view->info.region);

	return true;
}

bool ui_view_delete(u32_t view_id)
{
	ui_manager_context_t *ui_manager = _ui_manager_get_context();
	ui_view_context_t *view = _ui_manager_get_view_context(view_id);

	if (!view)
		return false;

	SYS_LOG_INF("view_id %d\n", view_id);

	view->info.view_proc(view->view_id, MSG_VIEW_DELETE, 0);

	sys_slist_find_and_remove(&ui_manager->view_list, &view->node);

	ui_view_all_update(view->info.region);

	mem_free(view);

	return true;
}

bool ui_view_all_update(ui_region_t region)
{
	ui_view_context_t *view;
	ui_manager_context_t *ui_manager = _ui_manager_get_context();

	SYS_SLIST_FOR_EACH_CONTAINER(&ui_manager->view_list, view, node) {
		ui_view_update(view->view_id, region);

		region = ui_region_clip(region, view->info.region);

		if (!ui_region_valid(region))
			break;
	}

	return true;
}

bool ui_view_update(u32_t view_id, ui_region_t region)
{
	ui_manager_context_t *ui_manager = _ui_manager_get_context();
	ui_view_context_t *view = _ui_manager_get_view_context(view_id);

	if (!view)
		return false;

	region = ui_region_intersect(region, view->info.region);

	if (!ui_region_valid(region))
		return true;

	SYS_SLIST_FOR_EACH_CONTAINER(&ui_manager->view_list, view, node) {
		if (view->view_id == view_id) {
			break;
		}

		region = ui_region_clip(region, view->info.region);

		if (!ui_region_valid(region)) {
			return true;
		}
	}

	ui_manager->update_region = ui_region_merge(ui_manager->update_region, region);

	return true;
}

bool ui_view_set_region(u32_t view_id, ui_region_t region)
{
	ui_view_context_t *view = _ui_manager_get_view_context(view_id);

	ui_region_t  old_region;

	if (!view) {
		return false;
	}
	old_region = view->info.region;
	view->info.region = region;

	region = ui_region_merge(old_region, region);

	ui_view_all_update(region);

	return true;
}

bool ui_view_set_order(u32_t view_id, u32_t order)
{
	ui_manager_context_t *ui_manager = _ui_manager_get_context();
	ui_view_context_t *view = _ui_manager_get_view_context(view_id);
	int	old_index;

	if (!view)
		return false;

	old_index = _ui_manager_get_view_index(view_id);

	sys_slist_find_and_remove(&ui_manager->view_list, &view->node);

	view->info.order = order;

	_ui_manager_add_view(view);

	if (_ui_manager_get_view_index(view_id) != old_index)
		ui_view_all_update(view->info.region);

	return true;
}

int ui_view_get_focus(void)
{
	ui_manager_context_t *ui_manager = _ui_manager_get_context();
	ui_view_context_t *view;

	SYS_SLIST_FOR_EACH_CONTAINER(&ui_manager->view_list, view, node) {
		if (view->info.flags & UI_FLAG_SET_FOCUS) {
			return view->view_id;
		}
	}

	return -1;
}

void ui_display_update(void)
{
	ui_manager_context_t *ui_manager = _ui_manager_get_context();
	ui_region_t  update_region = ui_manager->update_region;
	ui_view_context_t *view;

	if (!ui_region_valid(update_region))
		return;

	SYS_SLIST_FOR_EACH_CONTAINER(&ui_manager->view_list, view, node) {
		ui_region_t  region;

		region = ui_region_intersect(update_region, view->info.region);
		if (!ui_region_valid(region)) {
			continue;
		}
		ui_message_send_async(view->view_id, MSG_VIEW_PAINT, region);
		update_region = ui_region_clip(update_region, region);
		if (!ui_region_valid(update_region)) {
			break;
		}
	}

	ui_manager->update_region = update_region;
}

int ui_message_send_async(u32_t view_id, u32_t msg_id, u32_t msg_data)
{
	struct app_msg msg = {0};
	ui_view_context_t *view = _ui_manager_get_view_context(view_id);

	if (!view) {
		return -ESRCH;
	}

    if(!os_is_free_msg_enough()){
		SYS_LOG_INF("drop ui msg ... %d\n", msg_pool_get_free_msg_num());
        return -ENOMEM;
    }

	msg.type = MSG_UI_EVENT;
	msg.sender = view_id;
	msg.cmd = msg_id;
	msg.value = msg_data;
	//SYS_LOG_INF("view_id %d  msg_id %d  msg_data %d\n", view_id, msg_id,  msg_data);
	return send_async_msg("main"/*view->app_id*/, &msg);
}

int ui_message_dispatch(u32_t view_id, u32_t msg_id, u32_t msg_data)
{
	ui_view_context_t *view = _ui_manager_get_view_context(view_id);

	if (!view) {
		return -ESRCH;
	}

	return view->info.view_proc(view->view_id, msg_id, msg_data);
}


static int key_input_msg_match(void *msg,k_tid_t target_thread,k_tid_t source_thread){
	struct app_msg *p_app_msg = (struct app_msg *)msg;
	if((p_app_msg != NULL) && (p_app_msg->type == MSG_KEY_INPUT)
		                   && (source_thread == os_current_get())) {
		return 1;
	}
	return 0;
}

int ui_manager_dispatch_key_event(u32_t key_event)
{
	ui_manager_context_t *ui_manager = _ui_manager_get_context();
	ui_view_context_t *view = NULL;
	u8_t index = 0;
	int result = 0;
	u32_t current_state = 0xffffffff;
	u32_t key_type = key_event & 0xffff0000;
	u16_t key_val = key_event & 0xffff;
	struct app_msg new_msg = { 0 };
	int ret = 0;

	SYS_SLIST_FOR_EACH_CONTAINER(&ui_manager->view_list, view, node) {
		const ui_key_map_t *view_key_map = view->info.view_key_map;

		index = 0;
		current_state = 0xffffffff;
		if (view->info.view_get_state)
			current_state = view->info.view_get_state();

		if(!os_is_free_msg_enough()){
			ret = os_msg_delete(key_input_msg_match);
			SYS_LOG_INF("drop key msg ... %d,%d", msg_pool_get_free_msg_num(),ret);
			return -ENOMEM;
		}

		while (view_key_map[index].key_val != KEY_RESERVED) {
			if (key_val == view_key_map[index].key_val) {
				if (view_key_map[index].app_state == TRANSMIT_ALL_KEY_EVENT &&
						(key_type & view_key_map[index].key_type)) {
					new_msg.type = MSG_INPUT_EVENT;
					new_msg.value = key_event;
					result = send_async_msg(view->app_id, &new_msg);
					return result;
				} else if ((key_type & view_key_map[index].key_type)
					&& (current_state & view_key_map[index].app_state)) {
					new_msg.type = MSG_INPUT_EVENT;
					new_msg.cmd = view_key_map[index].app_msg;
					result = send_async_msg(view->app_id, &new_msg);
					return result;
				}
			}
			index++;
		}
	}
	return result;
}

static ui_manager_context_t global_ui_manager;

int ui_manager_init(void)
{
	if (ui_manager_context)
		return -EEXIST;

	ui_manager_context = &global_ui_manager;

	memset(ui_manager_context, 0, sizeof(ui_manager_context_t));

#ifdef CONFIG_GUI
	UG_Init(&ui_manager_context->gui, (void(*)(UG_S16, UG_S16, UG_COLOR))fb_draw_pixel, 320, 240);
#endif

	sys_slist_init(&ui_manager_context->view_list);

#ifdef CONFIG_LED_MANAGER
	led_manager_init();
#endif

#ifdef CONFIG_SEG_LED_MANAGER
	seg_led_manager_init();
#endif

	return 0;
}

int ui_manager_exit(void)
{
	ui_manager_context_t *ui_manager = _ui_manager_get_context();
	ui_view_context_t *view, *tmp;

	if (!ui_manager)
		return -ESRCH;

	SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&ui_manager->view_list, view, tmp, node) {
		if (view) {
			ui_view_delete(view->view_id);
		}
	}

	/* mem_free(ui_manager); */

#ifdef CONFIG_LED_MANAGER
	led_manager_deinit();
#endif


#ifdef CONFIG_SEG_LED_MANAGER
	seg_led_manager_deinit();
#endif
	return 0;
}
