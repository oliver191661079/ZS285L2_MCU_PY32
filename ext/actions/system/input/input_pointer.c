/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file input manager interface
 */


#include <os_common_api.h>
#include <kernel.h>
#include <string.h>
#include <key_hal.h>
#include <app_manager.h>
#include <mem_manager.h>
#include <input_manager.h>
#include <msg_manager.h>
#include <sys_event.h>
#include <property_manager.h>
#include <input_manager_inner.h>

#define CONFIG_READ_DIRCT 1

#ifndef CONFIG_READ_DIRCT
K_MSGQ_DEFINE(pointer_scan_msgq, sizeof(input_dev_data_t), 10, 4);
#endif

#ifdef CONFIG_INPUT_DEV_ACTS_TP_KEY
static void pointer_scan_callback(struct device *dev, struct input_value *val)
{
#ifndef CONFIG_READ_DIRCT
	input_dev_data_t data = {
		.point.x = val->point.loc_x,
		.point.y = val->point.loc_y,
		.state = (val->point.pessure_value == 1)? INPUT_DEV_STATE_PR : INPUT_DEV_STATE_REL,
		.gesture  = val->point.gesture,
	};

	if (k_msgq_put(&pointer_scan_msgq, &data, K_NO_WAIT) != 0) {
		LOG_ERR("Could put input data into queue");
	}
#endif
}

static bool pointer_scan_read(input_drv_t *drv, input_dev_data_t *data)
{
#if CONFIG_READ_DIRCT
	struct input_value val;
	memset(&val, 0, sizeof(struct input_value));

	input_dev_inquiry(drv->input_dev, &val);

	data->point.x = val.point.loc_x;
	data->point.y = val.point.loc_y;
	data->state = (val.point.pessure_value == 1)? INPUT_DEV_STATE_PR : INPUT_DEV_STATE_REL;
	data->gesture = val.point.gesture;
	return false;
#else
	static input_dev_data_t prev = {
		.point.x = 0,
		.point.y = 0,
		.state = INPUT_DEV_STATE_REL,
		.gesture = 0,
	};

	input_dev_data_t curr;

	if (k_msgq_get(&pointer_scan_msgq, &curr, K_NO_WAIT) == 0) {
		prev = curr;
	}

	*data = prev;

	return k_msgq_num_used_get(&pointer_scan_msgq) > 0;
#endif
}

static void pointer_scan_enable(input_drv_t *drv, bool enable)
{
	struct device *input_dev = (struct device *)device_get_binding("tpkey");

	if (input_dev) {
		if (enable)
			input_dev_enable(input_dev);
		else
			input_dev_disable(input_dev);
	}
}
#endif
int input_pointer_device_init(void)
{

#ifdef CONFIG_INPUT_DEV_ACTS_TP_KEY
	input_drv_t input_drv;
	struct device *input_dev = (struct device *)device_get_binding("tpkey");
	if (!input_dev) {
		printk("cannot found key dev %s\n","tpkey");
		return -ENODEV;
	}

	input_dev_register_notify(input_dev, pointer_scan_callback);

	input_drv.type = INPUT_DEV_TYPE_POINTER;
	input_drv.read_cb = pointer_scan_read;
	input_drv.enable = pointer_scan_enable;
	input_drv.input_dev = input_dev;

	input_driver_register(&input_drv);

	SYS_LOG_INF("init ok");
#endif

	return 0;
}
