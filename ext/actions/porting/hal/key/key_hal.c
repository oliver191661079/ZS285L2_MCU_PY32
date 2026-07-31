/*
 * Copyright (c) 2017 Actions Semi Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.

 * Author: wh<wanghui@actions-semi.com>
 *
 * Change log:
 *	2017/3/15: Created by wh.
 */
#include <os_common_api.h>
#include <mem_manager.h>
#include <misc/printk.h>
#include <shell/shell.h>

#include "key_hal.h"

void * key_device_open(key_notify_cb cb,char * dev_name)
{
	struct key_handle * handle = mem_malloc(sizeof(struct key_handle));
	if(handle == NULL)
	{
		SYS_LOG_ERR("adc key in mem_malloc failed  need %d  bytes ",(int)sizeof(struct key_handle));
		return NULL;
	}

	handle->input_dev = device_get_binding(dev_name);

	if (!handle->input_dev) {
		printk("cannot found key dev %s\n",dev_name);
		mem_free(handle);
		return NULL;
	}

	input_dev_enable(handle->input_dev);

	input_dev_register_notify(handle->input_dev, cb);

	handle->key_notify = cb;

	return handle;
}

void key_device_close(void * handle)
{
	struct key_handle * key = (struct key_handle *)handle;

	input_dev_unregister_notify(key->input_dev, key->key_notify);

	input_dev_disable(key->input_dev);

	mem_free(key);
}