/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief app switch manager.
 */

#include <os_common_api.h>
#include <string.h>
#include <mem_manager.h>
#include <msg_manager.h>
#include <app_manager.h>
#include <hotplug_manager.h>
#include "app_switch.h"
#ifdef CONFIG_ESD_MANAGER
#include "esd_manager.h"
#endif
#ifdef CONFIG_DVFS
#include <dvfs.h>
#endif

#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "app_switcher"
#include <logging/sys_log.h>

extern void recorder_service_app_switch_exit(void);
extern void alarm_need_exit(void);

#define INVALID_APP_INDEX 0xFF

struct app_switch_node_t {
	const char *app_name;
	uint8_t pluged;
} app_switch_node_t;

struct app_switch_ctx_t {
	uint8_t locked;
	uint8_t force_locked;
	uint8_t cur_index;
	uint8_t last_index;
	uint8_t app_num;
	uint8_t app_actived_num;
	struct app_switch_node_t app_list[0];
};

OS_MUTEX_DEFINE(app_switch_mutex);

struct app_switch_ctx_t *g_switch_ctx = NULL;

static inline struct app_switch_ctx_t *_app_switch_get_ctx(void)
{
	return g_switch_ctx;
}

static inline uint8_t _app_switch_get_app_id(char *app_name)
{
	int i = 0;
	int result = -1;
	struct app_switch_ctx_t *switch_ctx = _app_switch_get_ctx();

	os_mutex_lock(&app_switch_mutex, OS_FOREVER);

	for (i = 0; i < switch_ctx->app_num; i++) {
		struct app_switch_node_t *app_node = &switch_ctx->app_list[i];

		if (!memcmp(app_node->app_name, app_name, strlen(app_name))) {
			result = i;
			break;
		}
	}

	os_mutex_unlock(&app_switch_mutex);
	return result;
}

uint8_t *app_switch_get_app_name(uint8_t appid)
{
	uint8_t *result = NULL;
	struct app_switch_ctx_t *switch_ctx = _app_switch_get_ctx();

	os_mutex_lock(&app_switch_mutex, OS_FOREVER);

	if (appid < switch_ctx->app_num) {
		struct app_switch_node_t *app_node = &switch_ctx->app_list[appid];

		result = (uint8_t *)app_node->app_name;
	}

	os_mutex_unlock(&app_switch_mutex);
	return result;
}

static uint8_t get_targe_app(uint32_t switch_mode, bool force, const char *filter_app_name)
{
	struct app_switch_node_t *app_node = NULL;
	struct app_switch_ctx_t *switch_ctx = _app_switch_get_ctx();
	uint8_t target = switch_ctx->cur_index;

	if (switch_ctx->app_num <= 1) {
		return target;
	}

	do {
		if (switch_mode == APP_SWITCH_NEXT) {
			target = (target + 1) % switch_ctx->app_num;
		} else if (switch_mode == APP_SWITCH_PREV) {
			target = (target - 1 + switch_ctx->app_num) % switch_ctx->app_num;
		} else if (switch_mode == APP_SWITCH_LAST) {
			target = switch_ctx->last_index;
		} else {
		}

		if (target == switch_ctx->cur_index) {
			break;
		}

		app_node = &switch_ctx->app_list[target];
		if(NULL == app_node) {
			SYS_LOG_ERR("NULL app node.\n");
			continue;
		}

		if(filter_app_name){
			if(memcmp(app_node->app_name, filter_app_name, strlen(filter_app_name)) == 0){
				continue;
			}
		}

		if (force || app_node->pluged) {
			break;
		} else {
			/* if last is not pluged, go to prev */
			if (switch_mode == APP_SWITCH_LAST) {
				switch_mode = APP_SWITCH_PREV;
			}
		}
	}while(1);

	return target;
}

int app_switch(const char* app_name, uint32_t switch_mode, bool force_switch)
{
	int ret = 0;
	uint8_t i = 0;
#ifdef CONFIG_ESD_MANAGER
	uint8_t app_id = INVALID_APP_INDEX;
#endif
	uint8_t target_app_index = INVALID_APP_INDEX;
	struct app_switch_node_t *app_node = NULL;
	struct app_switch_ctx_t *switch_ctx = _app_switch_get_ctx();

	if(NULL != app_name){
		SYS_LOG_INF("name:%s\n", app_name);
	}
	SYS_LOG_INF("mode:%d, force:%d\n", switch_mode, force_switch);
#ifdef CONFIG_DVFS_DYNAMIC_LEVEL
	dvfs_set_level(DVFS_LEVEL_HIGH_PERFORMANCE, "switch");
#endif

	os_mutex_lock(&app_switch_mutex, OS_FOREVER);

	if (switch_ctx->force_locked) {
		SYS_LOG_INF("force locked %d \n", switch_ctx->force_locked);
		goto exit;
	}

	if (switch_ctx->locked && !force_switch) {
		SYS_LOG_INF("locked %d\n", switch_ctx->locked);
		goto exit;
	}

#ifdef CONFIG_RECORD_SERVICE
	recorder_service_app_switch_exit();
#endif
#ifdef CONFIG_ALARM_APP
	alarm_need_exit();
#endif

	if (NULL != app_name) {
		for (i = 0; i < switch_ctx->app_num; i++) {
			app_node = &switch_ctx->app_list[i];
			if (!memcmp(app_node->app_name, app_name, strlen(app_name))
				&& (app_node->pluged || force_switch)) {
				target_app_index = i;
			}
		}
	} else {
		target_app_index = get_targe_app(switch_mode, force_switch, NULL);
	}

	if (target_app_index == switch_ctx->cur_index) {
		SYS_LOG_INF("target is current\n");
		ret = 0xff;
		goto exit;
	}

	if (target_app_index >= switch_ctx->app_num) {
		SYS_LOG_INF("no target\n");
		ret = 0xff;
		goto exit;
	}

	if (switch_ctx->cur_index >= switch_ctx->app_num) {
		switch_ctx->last_index = target_app_index;
	} else {
		switch_ctx->last_index = switch_ctx->cur_index;
	}
	switch_ctx->cur_index = target_app_index;
	app_node = &switch_ctx->app_list[target_app_index];
	SYS_LOG_INF("to active: %s", app_node->app_name);
	ret = app_manager_active_app((char *)app_node->app_name);

#ifdef CONFIG_ESD_MANAGER
	app_id = target_app_index;
	esd_manager_save_scene(TAG_APP_ID, &app_id, 1);
#endif

exit:
	os_mutex_unlock(&app_switch_mutex);

#ifdef CONFIG_DVFS_DYNAMIC_LEVEL
	dvfs_unset_level(DVFS_LEVEL_HIGH_PERFORMANCE, "switch");
#endif
	return ret;
}

int app_switch_filter(const char* app_name, uint32_t switch_mode, bool force_switch, const char *filter_app_name)
{
	int ret = 0;
	uint8_t i = 0;
#ifdef CONFIG_ESD_MANAGER
	uint8_t app_id = INVALID_APP_INDEX;
#endif
	uint8_t target_app_index = INVALID_APP_INDEX;
	struct app_switch_node_t *app_node = NULL;
	struct app_switch_ctx_t *switch_ctx = _app_switch_get_ctx();

	if(NULL != app_name){
		SYS_LOG_INF("name:%s\n", app_name);
	}
	SYS_LOG_INF("mode:%d, force:%d\n", switch_mode, force_switch);
#ifdef CONFIG_DVFS_DYNAMIC_LEVEL
	dvfs_set_level(DVFS_LEVEL_HIGH_PERFORMANCE, "switch");
#endif

	os_mutex_lock(&app_switch_mutex, OS_FOREVER);

	if (switch_ctx->force_locked) {
		SYS_LOG_INF("force locked %d \n", switch_ctx->force_locked);
		goto exit;
	}

	if (switch_ctx->locked && !force_switch) {
		SYS_LOG_INF("locked %d\n", switch_ctx->locked);
		goto exit;
	}

#ifdef CONFIG_RECORD_SERVICE
	recorder_service_app_switch_exit();
#endif
#ifdef CONFIG_ALARM_APP
	alarm_need_exit();
#endif

	if (NULL != app_name) {
		for (i = 0; i < switch_ctx->app_num; i++) {
			app_node = &switch_ctx->app_list[i];

			if(filter_app_name){
				if(memcmp(app_node->app_name, filter_app_name, strlen(filter_app_name)) == 0){
					continue;
				}
			}

			if (!memcmp(app_node->app_name, app_name, strlen(app_name))
				&& (app_node->pluged || force_switch)) {
				target_app_index = i;
			}
		}
	} else {
		target_app_index = get_targe_app(switch_mode, force_switch, filter_app_name);
	}

	if (target_app_index == switch_ctx->cur_index) {
		SYS_LOG_INF("target is current\n");
		ret = 0xff;
		goto exit;
	}

	if (target_app_index >= switch_ctx->app_num) {
		SYS_LOG_INF("no target\n");
		ret = 0xff;
		goto exit;
	}

	if (switch_ctx->cur_index >= switch_ctx->app_num) {
		switch_ctx->last_index = target_app_index;
	} else {
		switch_ctx->last_index = switch_ctx->cur_index;
	}
	switch_ctx->cur_index = target_app_index;
	app_node = &switch_ctx->app_list[target_app_index];
	SYS_LOG_INF("to active: %s", app_node->app_name);
	ret = app_manager_active_app((char *)app_node->app_name);

#ifdef CONFIG_ESD_MANAGER
	app_id = target_app_index;
	esd_manager_save_scene(TAG_APP_ID, &app_id, 1);
#endif

exit:
	os_mutex_unlock(&app_switch_mutex);

#ifdef CONFIG_DVFS_DYNAMIC_LEVEL
	dvfs_unset_level(DVFS_LEVEL_HIGH_PERFORMANCE, "switch");
#endif
	return ret;
}


void app_switch_add_app(const char *app_name)
{
	int i = 0;
	struct app_switch_ctx_t *switch_ctx = _app_switch_get_ctx();

	os_mutex_lock(&app_switch_mutex, OS_FOREVER);

	for (i = 0; i < switch_ctx->app_num; i++) {
		struct app_switch_node_t *app_node = &switch_ctx->app_list[i];

		if (!memcmp(app_node->app_name, app_name, strlen(app_name))
				&& !app_node->pluged) {
			app_node->pluged = 1;
			switch_ctx->app_actived_num++;
			SYS_LOG_INF("name:%s plug in", app_node->app_name);
		}
	}

	os_mutex_unlock(&app_switch_mutex);
}

void app_switch_remove_app(const char *app_name)
{
	int i = 0;
	struct app_switch_ctx_t *switch_ctx = _app_switch_get_ctx();

	os_mutex_lock(&app_switch_mutex, OS_FOREVER);

	for (i = 0; i < switch_ctx->app_num; i++) {
		struct app_switch_node_t *app_node = &switch_ctx->app_list[i];

		if (!memcmp(app_node->app_name, app_name, strlen(app_name))
				&& app_node->pluged) {
			app_node->pluged = 0;
			switch_ctx->app_actived_num--;
			SYS_LOG_INF("name:%s plug out ", app_node->app_name);
		}
	}

	os_mutex_unlock(&app_switch_mutex);
}

void app_switch_lock(uint8_t reason)
{
	struct app_switch_ctx_t *switch_ctx = _app_switch_get_ctx();

	os_mutex_lock(&app_switch_mutex, OS_FOREVER);
	switch_ctx->locked |= reason;
	os_mutex_unlock(&app_switch_mutex);
}

void app_switch_unlock(uint8_t reason)
{
	struct app_switch_ctx_t *switch_ctx = _app_switch_get_ctx();

	os_mutex_lock(&app_switch_mutex, OS_FOREVER);
	switch_ctx->locked &= ~reason;
	os_mutex_unlock(&app_switch_mutex);
}

void app_switch_force_lock(uint8_t reason)
{
	struct app_switch_ctx_t *switch_ctx = _app_switch_get_ctx();

	os_mutex_lock(&app_switch_mutex, OS_FOREVER);
	switch_ctx->force_locked |= reason;
	os_mutex_unlock(&app_switch_mutex);
}

void app_switch_force_unlock(uint8_t reason)
{
	struct app_switch_ctx_t *switch_ctx = _app_switch_get_ctx();

	os_mutex_lock(&app_switch_mutex, OS_FOREVER);
	switch_ctx->force_locked &= ~reason;
	os_mutex_unlock(&app_switch_mutex);
}

int app_switch_init(const char **app_id_switch_list, int app_num)
{
	int i = 0;
	int ctx_size = sizeof(struct app_switch_ctx_t)
						+ sizeof(struct app_switch_node_t) * app_num;

	g_switch_ctx = (struct app_switch_ctx_t *)mem_malloc(ctx_size);
	if (!g_switch_ctx) {
		SYS_LOG_ERR("need mem %d bytes ", ctx_size);
		return -ENOMEM;
	}

	memset(g_switch_ctx, 0, ctx_size);

	for (i = 0; i < app_num; i++) {
		struct app_switch_node_t *app_node = &g_switch_ctx->app_list[i];

		app_node->app_name = app_id_switch_list[i];
		app_node->pluged = 0;
	}

	g_switch_ctx->cur_index = INVALID_APP_INDEX;
	g_switch_ctx->last_index = INVALID_APP_INDEX;
	g_switch_ctx->app_num = app_num;
	g_switch_ctx->app_actived_num = 0;
	g_switch_ctx->locked = 0;
	g_switch_ctx->force_locked = 0;
	return 0;
}

void app_switch_dump_list(void)
{
	uint8_t i;
	struct app_switch_ctx_t *switch_ctx = _app_switch_get_ctx();

	os_mutex_lock(&app_switch_mutex, OS_FOREVER);

	printk("\napp list dump start.\n");
	printk("\tlocked=%d\n", switch_ctx->locked);
	printk("\tforce_locked=%d\n", switch_ctx->force_locked);
	printk("\tcur_index=%d\n", switch_ctx->cur_index);
	printk("\tlast_index=%d\n", switch_ctx->last_index);
	printk("\tapp_num=%d\n", switch_ctx->app_num);
	printk("\tapp_actived_num=%d\n", switch_ctx->app_actived_num);
	for (i = 0; i < switch_ctx->app_num; i++) {
		struct app_switch_node_t *app_node = &switch_ctx->app_list[i];
		if (app_node->pluged) {
			printk("\t* %d: %s\n", i, app_node->app_name);
		} else {
			printk("\t  %d: %s\n", i, app_node->app_name);
		}
	}
	printk("app list dump end.\n\n");

	os_mutex_unlock(&app_switch_mutex);

}
