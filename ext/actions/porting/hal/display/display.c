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
 *	2018/8/10: Created by wh.
 */
#include <os_common_api.h>
#include <mem_manager.h>
#include <string.h>
#include "display.h"
#include "framebuffer.h"

struct display_device* display_device_register(const struct display_info* info)
{
	return fb_display_device_register(0, (struct display_info*)info);
}

u16_t display_get_width(struct display_device* device)
{
	if( !device ) return 0;

	return device->width;
}

u16_t display_get_height(struct display_device* device)
{
	if( !device ) return 0;

	return device->height;
}


void display_hw_function_register(struct display_device* device, u8_t func_id, void* driver)
{
	if ( func_id >= NUMBER_OF_HW_FUNC ) return;

	device->hw_func[func_id].func = driver;
	device->hw_func[func_id].state = HW_FUNC_REGISTERED | HW_FUNC_ENABLED;
}

void display_hw_function_enable(struct display_device* device, u8_t func_id)
{
	if ( func_id >= NUMBER_OF_HW_FUNC ) return;

	if ( device->hw_func[func_id].state & HW_FUNC_REGISTERED )
	{
		device->hw_func[func_id].state |= HW_FUNC_ENABLED;
	}
}

void display_hw_function_disable(struct display_device* device, u8_t func_id)
{
	if ( func_id >= NUMBER_OF_HW_FUNC ) return;

	if ( device->hw_func[func_id].state & HW_FUNC_REGISTERED )
	{
		device->hw_func[func_id].state &= ~HW_FUNC_ENABLED;
	}
}
