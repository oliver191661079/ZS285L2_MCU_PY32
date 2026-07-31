/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file property cache interface
 */
 #include <os_common_api.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <stdlib.h>
#include <mem_manager.h>
#include <property_inner.h>
#include <property_manager.h>
#define SYS_LOG_DOMAIN "property"
#ifndef SYS_LOG_LEVEL
#define SYS_LOG_LEVEL CONFIG_SYS_LOG_DEFAULT_LEVEL
#endif
#include <logging/sys_log.h>

#define MAX_NVRAM_ITEM_CACHE_NUM 70

struct cahce_item_data {
	char *name;
	char *data;
	uint16_t data_len:10;
	uint16_t used_flag:2;
	uint16_t flush_req:1;
	uint16_t global_data : 1;
} __packed;

OS_MUTEX_DEFINE(nvram_cache_mutex);

static struct cahce_item_data globle_property_cache[MAX_NVRAM_ITEM_CACHE_NUM];

static struct cahce_item_data *find_property_cache(const char *name)
{
	int i;
	struct cahce_item_data *item = NULL;

	for (i = 0; i < MAX_NVRAM_ITEM_CACHE_NUM; i++) {
		item = &globle_property_cache[i];
		if (item->used_flag && !strcmp(item->name, name)) {
			SYS_LOG_DBG(" %d %s\n", i, item->name);
			return item;
		}
	}
	return NULL;
}

static struct cahce_item_data *get_property_cache(const char *name, int len)
{
	int i;
	struct cahce_item_data *item = NULL;

	for (i = 0; i < MAX_NVRAM_ITEM_CACHE_NUM; i++) {
		item = &globle_property_cache[i];
		if (!item->used_flag) {
			item->name = mem_malloc(strlen(name) + 1);
			if (!item->name) {
				item = NULL;
				goto exit;
			}
			memset(item->name, 0, strlen(name) + 1);
			if (len == 0) {
				item->data = NULL;
				goto exit;
			}
			item->data = mem_malloc(len);
			if (!item->data) {
				mem_free(item->name);
				item = NULL;
				goto exit;
			}
			memset(item->data, 0, len);
exit:
			SYS_LOG_INF("get property %d %s\n", i, name);
			return item;
		}
	}
	return NULL;
}

static struct cahce_item_data *get_property_cache2(const char *name, int len)
{
	int i;
	struct cahce_item_data *item = NULL;

	for (i = 0; i < MAX_NVRAM_ITEM_CACHE_NUM; i++) {
		item = &globle_property_cache[i];
		if (!item->used_flag) {
			item->name = mem_malloc(strlen(name) + 1);
			item->global_data = true;

			if (!item->name) {
				item = NULL;
				goto exit;
			}
			memset(item->name, 0, strlen(name) + 1);
			if (len == 0) {
				item->data = NULL;
				goto exit;
			}

exit:
			SYS_LOG_INF("get property %d %s\n", i, name);
			return item;
		}
	}
	return NULL;
}


static int put_property_cache(struct cahce_item_data *item)
{
	if (item) {
		if (item->data && !item->global_data) {
			mem_free(item->data);
		}
		if (item->name) {
			mem_free(item->name);
		}
		item->data = NULL;
		item->name = NULL;
		item->used_flag = 0;
		item->data_len = 0;
		item->flush_req = 0;
		item->global_data = 0;
	}
	return 0;
}

int property_cache_get(const char *name, void *data, int len)
{
	int read_len = 0;
	struct cahce_item_data *item = NULL;

	os_mutex_lock(&nvram_cache_mutex, OS_FOREVER);

	item = find_property_cache(name);

	/**read from nvram cache */
	if (item) {
		if (item->data_len > len) {
			read_len = len;
		} else {
			read_len = item->data_len;
		}
		if (item->data) {
			memcpy(data, item->data, read_len);
		}
	} else {
		/** read from nvram*/
		read_len = nvram_config_get(name, data, len);
	}

	os_mutex_unlock(&nvram_cache_mutex);

	return read_len;
}


/* @param data NULL to delete the key */
int property_cache_set_global_data(const char *name, const void *data, int len)
{
	int ret = 0;
	struct cahce_item_data *item = NULL;

	item = find_property_cache(name);
	/**write to old nvram cache */
	if (item) {
		if (!data) {
			put_property_cache(item);
			goto exit;
		}

		item->data = (char *)data;
		item->data_len = len;
		ret = 0;
		goto exit;
	}

	/**write to new nvram cache */
	item = get_property_cache2(name, len);
	if (item) {
		/** new item */
		memcpy(item->name, name, strlen(name));
		item->data = (char *)data;
		item->data_len = len;
		item->used_flag = 1;
		ret = 0;
		goto exit;
	}

	ret = -ENOMEM;

exit:

	return ret;
}


/* @param data NULL to delete the bt keys */
int property_cache_set_bt_keys(const char *name, const void *data, int len)
{
	int ret = 0;
	struct cahce_item_data *item = NULL;

	item = find_property_cache(name);
	/**write to old nvram cache */
	if (item) {
		if (!data) {
			put_property_cache(item);
			goto exit;
		}

		if (item->data_len == len) {
			memcpy(item->data, data, len);
			ret = 0;
		} else {
			void *old_data = item->data;

			item->data = mem_malloc(len);
			if (!item->data)
				goto exit;
			memcpy(item->data, data, len);
			item->data_len = len;
			ret = 0;
			if (old_data)
				mem_free(old_data);
		}
		goto exit;
	}

	/**write to new nvram cache */
	item = get_property_cache(name, len);
	if (item) {
		/** new item */
		memcpy(item->name, name, strlen(name));
		if (item->data) {
			memcpy(item->data, data, len);
		}
		item->data_len = len;
		item->used_flag = 1;
		ret = 0;
		goto exit;
	}

	ret = -ENOMEM;
exit:

	return ret;
}

/* @param data NULL to delete the key */
int property_cache_set2(const char *name, const void *data, int len)
{
	int ret = 0;
	struct cahce_item_data *item = NULL;

	item = find_property_cache(name);
	/**write to old nvram cache */
	if (item) {
		if (!data) {
			if (item->data) {
				mem_free(item->data);
				item->data = NULL;
				item->data_len = 0;
			}
			goto exit;
		}

		if (item->data_len == len) {
			memcpy(item->data, data, len);
			ret = 0;
		} else {
			void *old_data = item->data;

			item->data = mem_malloc(len);
			if (!item->data)
				goto exit;
			memcpy(item->data, data, len);
			item->data_len = len;
			ret = 0;
			if (old_data)
				mem_free(old_data);
		}
		goto exit;
	}

	/**write to new nvram cache */
	item = get_property_cache(name, len);
	if (item) {
		/** new item */
		memcpy(item->name, name, strlen(name));
		if (item->data) {
			memcpy(item->data, data, len);
		}
		item->data_len = len;
		item->used_flag = 1;
		ret = 0;
		goto exit;
	}

	ret = -ENOMEM;
exit:

	return ret;
}


/* @param data NULL to delete the key */
int property_cache_set(const char *name, const void *data, int len)
{
	int ret = 0;

	if (!name || (data && len <= 0)) {
		SYS_LOG_ERR("invalid");
		return -EINVAL;
	}

	/* len is meaningless in this case */
	if (!data) {
		len = 0;
	}

	os_mutex_lock(&nvram_cache_mutex, OS_FOREVER);

	if (name[0] != 'B' || name[1] != 'T'){
		ret = property_cache_set2(name, data, len);
	}else{
		if (strncmp(name, CFG_BT_KEYS, 7) == 0){
			ret = property_cache_set_global_data(name, data, len);
		}else if((strncmp(name, CFG_BT_CCC, 6) == 0) || \
			(strncmp(name, CFG_BT_CF, 6) == 0) || \
			(strncmp(name, CFG_BT_SC, 6) == 0)){
			ret = property_cache_set_bt_keys(name, data, len);
		}else{
			ret = property_cache_set2(name, data, len);
		}
	}

	os_mutex_unlock(&nvram_cache_mutex);

	return ret;
}





int property_cache_flush(const char *name)
{
	int i;
	struct cahce_item_data *item = NULL;

	os_mutex_lock(&nvram_cache_mutex, OS_FOREVER);

	for (i = 0; i < MAX_NVRAM_ITEM_CACHE_NUM; i++) {
		item = &globle_property_cache[i];
		if (item->used_flag &&
	     ((!name) || memcmp(item->name, name, strlen(name)) == 0)) {
			if (!nvram_config_set(item->name, item->data, item->data_len)) {
				put_property_cache(item);
				item->flush_req = false;
			}
		}
	}

	os_mutex_unlock(&nvram_cache_mutex);

	SYS_LOG_INF("ok\n");
	return 0;
}

int property_cache_flush_req(const char *name)
{
	int i;
	struct cahce_item_data *item = NULL;

	os_mutex_lock(&nvram_cache_mutex, OS_FOREVER);

	for (i = 0; i < MAX_NVRAM_ITEM_CACHE_NUM; i++) {
		item = &globle_property_cache[i];
		if (item->used_flag &&
	     ((!name) || memcmp(item->name, name, strlen(name)) == 0)) {
			item->flush_req = true;
		}
	}

	os_mutex_unlock(&nvram_cache_mutex);

	SYS_LOG_INF("ok\n");
	return 0;
}

int property_cache_flush_req_deal(void)
{
	int i;
	struct cahce_item_data *item = NULL;

	os_mutex_lock(&nvram_cache_mutex, OS_FOREVER);

	for (i = 0; i < MAX_NVRAM_ITEM_CACHE_NUM; i++) {
		item = &globle_property_cache[i];
		if (item->used_flag && item->flush_req) {
			if (!nvram_config_set(item->name, item->data, item->data_len)) {
				put_property_cache(item);
			}
			item->flush_req = false;
		}
	}

	os_mutex_unlock(&nvram_cache_mutex);

	SYS_LOG_INF("flush ok\n");
	return 0;
}

int property_cache_init(void)
{
	memset(globle_property_cache, 0, sizeof(globle_property_cache));
	return 0;
}
