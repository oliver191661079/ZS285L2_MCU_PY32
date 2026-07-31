/*
 * Copyright (c) 2020, Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <drivers/accelerator/accelerator.h>
#include <as_res_p.h>

#define SYS_LOG_DOMAIN "resample"
#include <logging/sys_log.h>

void *fir_device_open(void)
{
	struct device *device = device_get_binding(CONFIG_ACCELERATOR_ACTS_DEV_NAME);
	if (!device) {
		SYS_LOG_ERR("cannot find device %s", CONFIG_ACCELERATOR_ACTS_DEV_NAME);
		return NULL;
	}

	if (accelerator_open(device))
		return NULL;

	return device;
}

int fir_device_close(void *device)
{
	if (!device)
		return -ENODEV;

	return accelerator_close(device);
}

int fir_device_run(void *device, fir_param_t *param)
{
	if (!device)
		return -ENODEV;

	param->output_len = 0;

	/* struct accelerator_fir_param and fir_param_t have the same definition */
	return accelerator_process(device, ACCELERATOR_MODE_FIR,
				(struct accelerator_fir_param *)param);
}
