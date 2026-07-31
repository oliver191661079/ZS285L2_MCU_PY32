/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file property manager interface
 */
#include <os_common_api.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <version.h>
#include <stdlib.h>
#include <hex_str.h>
#include <property_inner.h>
#define SYS_LOG_DOMAIN "property"
#ifndef SYS_LOG_LEVEL
#define SYS_LOG_LEVEL CONFIG_SYS_LOG_DEFAULT_LEVEL
#endif
#include <logging/sys_log.h>
#include <ctype.h>
#include <sys_event.h>
#include <msg_manager.h>
#include <property_manager.h>
#include <mem_manager.h>

#ifdef CONFIG_NVRAM_USER_STORAGE_EXT_FLASH
static struct k_delayed_work propery_flush_work;
static atomic_t flush_flag;

static void flush_finished_callback(struct app_msg *msg, int result, void *reply)
{
	property_flush(NULL);
	atomic_clear(&flush_flag);
}

static void flushed_work(struct k_work *work)
{
	//avoid too much system app msg
	if(atomic_get(&flush_flag) <= 0){
		atomic_set(&flush_flag, 1);
		struct app_msg  msg = {0};

		msg.type = MSG_SYS_EVENT;
		msg.cmd = SYS_EVENT_REQ_FLUSH_PROPERTY;
		msg.callback = flush_finished_callback;

		send_async_msg(CONFIG_SYS_APP_NAME, &msg);
	}
}

#else

struct nvram_write_msg {
    char *name;
    void *data;
    int len;
};

static void handle_nvram_write_request(struct app_msg *msg)
{
    struct nvram_write_msg *write_msg = (struct nvram_write_msg *)msg->ptr;
    int ret;

    if (!write_msg || !write_msg->name) {
        SYS_LOG_ERR("Invalid nvram write message");
        return;
    }

    ret = nvram_config_set(write_msg->name, write_msg->data, write_msg->len);
    if (ret < 0) {
        SYS_LOG_ERR("NVRAM write failed: %d", ret);
    }
}

static void nvram_write_msg_cleanup(struct app_msg *msg, int result, void *reply)
{
    struct nvram_write_msg *write_msg = (struct nvram_write_msg *)msg->ptr;

    if (write_msg) {
        if (write_msg->name) {
            mem_free(write_msg->name);
        }
        if (write_msg->data) {
            mem_free(write_msg->data);
        }
        mem_free(write_msg);
    }
}

static void nvram_write_msg_callback(struct app_msg *msg, int result, void *reply)
{
	handle_nvram_write_request(msg);
	nvram_write_msg_cleanup(msg, result, reply);
}

int submit_nvram_write_work(const char *name, const void *data, int len)
{
    struct app_msg msg = {0};
    struct nvram_write_msg *write_msg;

    write_msg = mem_malloc(sizeof(struct nvram_write_msg));
    if (!write_msg) {
        SYS_LOG_ERR("Failed to allocate write_msg");
        return -ENOMEM;
    }

    write_msg->name = mem_malloc(strlen(name) + 1);
    if (!write_msg->name) {
        mem_free(write_msg);
        return -ENOMEM;
    }
    strcpy(write_msg->name, name);

    if (data && len > 0) {
        write_msg->data = mem_malloc(len);
        if (!write_msg->data) {
            mem_free(write_msg->name);
            mem_free(write_msg);
            return -ENOMEM;
        }
        memcpy(write_msg->data, data, len);
    } else {
        write_msg->data = NULL;
    }
    write_msg->len = len;

    msg.type = MSG_SYS_EVENT;
    msg.cmd = SYS_EVENT_REQ_NVRAM_DIRECT_WRITE;
    msg.ptr = write_msg;
    msg.callback = nvram_write_msg_callback;

    send_async_msg(CONFIG_SYS_APP_NAME, &msg);

	return 0;
}

#endif

int property_get(const char *key, char *value, int value_len)
{
	int ret = 0;
#ifdef CONFIG_PROPERTY_CACHE
	ret = property_cache_get(key, value, value_len);
#else
#ifdef CONFIG_NVRAM_CONFIG
	ret = nvram_config_get(key, value, value_len);
	if (ret < 0) {
		return -ENOENT;
	}
#endif
#endif
	return ret;
}

int property_set(const char *key, char *value, int value_len)
{
	int ret = -ENOENT;
#ifdef CONFIG_PROPERTY_CACHE
#ifndef CONFIG_NVRAM_USER_STORAGE_EXT_FLASH
	ret = property_cache_set(key, value, value_len);
	if(ret == -ENOMEM){
		/** direct write to nvram*/
		SYS_LOG_INF("submit nvram write work %s\n", key);
		ret = submit_nvram_write_work(key, value, value_len);
	}
#else
	ret = property_cache_set(key, value, value_len);
	k_delayed_work_submit(&propery_flush_work, K_NO_WAIT);
#endif
#else
#ifdef CONFIG_NVRAM_CONFIG
	ret = nvram_config_set(key, value, value_len);
#endif
#endif
	return ret;
}

int property_set_factory(const char *key, char *value, int value_len)
{
	int ret = -ENOENT;
#ifdef CONFIG_NVRAM_CONFIG
	ret = nvram_config_set_factory(key, value, value_len);
#endif
	return ret;
}

int property_get_int(const char *key, int default_value)
{
	int property = 0;
	char temp_data[16] = {0};

	if (property_get(key, temp_data, sizeof(temp_data)) > 0) {
        char *endptr;
        long int temp_value = strtol(temp_data, &endptr, 10);

        if (endptr != temp_data && *endptr == '\0' && temp_value >= INT_MIN && temp_value <= INT_MAX) {
            property = (int)temp_value;
        }else{
			property = default_value;
		}
	} else {
		property = default_value;
	}

	return property;
}

int property_get_integer(const char *key, int *value, int default_value)
{
	int ret;
	char temp_data[16] = {0};

	ret = property_get(key, temp_data, sizeof(temp_data) - 1);

	if (ret > 0) {
        char *endptr;
        long int temp_value = strtol(temp_data, &endptr, 10);

        if (endptr != temp_data && *endptr == '\0' && temp_value >= INT_MIN && temp_value <= INT_MAX) {
            *value = (int)temp_value;
        }else{
			*value = default_value;
			ret = -EINVAL;
		}
	} else {
		*value = default_value;
		ret = -EACCES;
	}

	return ret;
}


int property_set_int(const char *key, int value)
{
	int ret = 0;
	char temp_data[16] = {0};

	snprintf(temp_data, sizeof(temp_data), "%d", value);

	if (0 != property_set(key, temp_data, strlen(temp_data))) {
		SYS_LOG_ERR("key %s\n", key);
		ret = -EACCES;
	}

	return ret;
}



int property_get_string(const char *key, char *value, int value_len, char *def_value)
{
	int ret = 0;
	int str_len;
#ifdef CONFIG_PROPERTY_CACHE
	ret = property_cache_get(key, value, value_len);
#else
#ifdef CONFIG_NVRAM_CONFIG
	ret = nvram_config_get(key, value, value_len);
#endif
#endif

	if(ret <= 0){
		ret = -EACCES;

		if(!def_value){
			return ret;
		}

		memset(value, 0, value_len);

		str_len = strlen(def_value);
		if(str_len > value_len){
			str_len = value_len;
		}

		strncpy(value, def_value, str_len);
	}

	return ret;
}

int property_get_hexstring(const char *key, char *value, int value_len)
{
	int ret = 0;
#ifdef CONFIG_PROPERTY_CACHE
	ret = property_cache_get(key, value, value_len);
#else
#ifdef CONFIG_NVRAM_CONFIG
	ret = nvram_config_get(key, value, value_len);
#endif
#endif

	if (ret <= 0) {
		return -ENOENT;
	}

    if (ret > value_len) {
        return -EINVAL;
    }

    for (size_t i = 0; i < ret; i++) {
        if (!isxdigit(value[i])) {
            return -EACCES;
        }
    }

	return ret;
}

int property_get_bool(const char *key, bool *value, bool def_value)
{
	int ret = 0;
	char temp[16] = {0};

#ifdef CONFIG_PROPERTY_CACHE
	ret = property_cache_get(key, temp, sizeof(temp));
#else
#ifdef CONFIG_NVRAM_CONFIG
	ret = nvram_config_get(key, temp, sizeof(temp));
#endif
#endif

	if (ret <= 0) {
		*value = def_value;
		ret = -EACCES;
	}else{
		*value = (strcmp(temp, "true") == 0);
	}

	return ret;
}

int property_set_bool(const char *key, bool value)
{
	int ret = 0;
	char temp_data[16] = {0};

    if (value) {
        snprintf(temp_data, sizeof(temp_data), "true");
    } else {
        snprintf(temp_data, sizeof(temp_data), "false");
    }

	if (0 != property_set(key, temp_data, strlen(temp_data))) {
		SYS_LOG_ERR("key %s\n", key);
		ret = -EACCES;
	}

	return ret;

}

int property_get_byte_array(const char *key, char *value, int value_len, const char *default_value)
{
	char temp_data[16] = {0};
	int len = property_get(key, temp_data, sizeof(temp_data));

	if (len > 0) {
		if (value_len > len / 2) {
			value_len = len / 2;
		}
		str_to_hex(value, temp_data, value_len);
	} else {
		memcpy(value, default_value, value_len);
	}

	return 0;
}

int property_set_byte_array(const char *key, char *value, int value_len)
{
	int ret = 0;
	char temp_data[16] = {0};
	int off = 0;

	for (int i = 0 ; i < value_len; i++) {
		if (!off) {
			off += snprintf(&temp_data[off], sizeof(temp_data) - off, ",");
		}
		off += snprintf(&temp_data[off], sizeof(temp_data) - off, "%d", value[i]);
	}

	if (0 != property_set(key, temp_data, sizeof(temp_data))) {
		SYS_LOG_ERR("key %s \n", key);
		ret = -EACCES;
	}

	return ret;
}

int property_get_int_array(const char *key, int *value, int value_len,  const short *default_value)
{
	char temp_data[16] = {0};
	char *str_begin = NULL;
	char *str_end = NULL;
	int index = 0;

	if (property_get(key, temp_data, sizeof(temp_data)) > 0) {
		str_begin = temp_data;
		while (!str_begin) {
			str_end = strstr(str_begin, ",");
			if (str_end) {
				*str_end = 0;
			}
			value[index++] = atoi(str_begin);
			str_begin = str_end + 1;
		}
	} else {
		memcpy(value, default_value, value_len);
	}

	return 0;
}

int property_set_int_array(const char *key, int *value, int value_len)
{
	int ret = 0;
	char temp_data[16] = {0};
	int off = 0;

	for (int i = 0 ; i < value_len; i++) {
		if (!off) {
			off += snprintf(&temp_data[off], sizeof(temp_data) - off, ",");
		}
		off += snprintf(&temp_data[off], sizeof(temp_data) - off, "%d", value[i]);
	}

	if (0 != property_set(key, temp_data, sizeof(temp_data))) {
		SYS_LOG_ERR("key %s error\n", key);
		ret = -EACCES;
	}

	return ret;
}

int property_flush(const char *key)
{
#ifdef CONFIG_PROPERTY_CACHE
	property_cache_flush(key);
#endif
	return 0;
}

int property_flush_req(const char *key)
{
#ifdef CONFIG_PROPERTY_CACHE
	property_cache_flush_req(key);
#endif
	return 0;
}

int property_flush_req_deal(void)
{
#ifdef CONFIG_PROPERTY_CACHE
	property_cache_flush_req_deal();
#endif
	return 0;
}

int property_clear_user_info(void)
{
	property_flush(NULL);
	nvram_config_clear_all();
	return 0;
}




int property_manager_init(void)
{
#ifdef CONFIG_PROPERTY_CACHE
	property_cache_init();
#endif

#ifdef CONFIG_NVRAM_USER_STORAGE_EXT_FLASH
	k_delayed_work_init(&propery_flush_work, flushed_work);
#endif

	return 0;
}
