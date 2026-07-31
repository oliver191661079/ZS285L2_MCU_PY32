/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * References:
 *
 * https://www.microbit.co.uk/device/screen
 * https://lancaster-university.github.io/microbit-docs/ubit/display/
 */

#include <zephyr.h>
#include <init.h>
#include <board.h>
#include <gpio.h>
#include <device.h>
#include <string.h>
#include <misc/printk.h>
#include <fm.h>
#include "audio_out.h"

int fm_reciver_set_freq(struct device *fm_dev, u32_t freq)
{
	struct acts_fm_device_data *fm_info = fm_dev->driver_data;
	struct fm_rx_device *sub_dev = NULL;

	if (!fm_info || !fm_info->rx_subdev)
		return -ENODEV;

	sub_dev = fm_info->rx_subdev;

	if (!sub_dev->set_freq(sub_dev, freq)) {
		return 0;
	}

	return -EINVAL;

}

int fm_reciver_set_mute(struct device *fm_dev, bool flag)
{
	struct acts_fm_device_data *fm_info = fm_dev->driver_data;
	struct fm_rx_device *sub_dev = NULL;

	if (!fm_info || !fm_info->rx_subdev)
		return -ENODEV;

	sub_dev = fm_info->rx_subdev;

	if (!sub_dev->set_mute(sub_dev, flag)) {
		return 0;
	}

	return -EINVAL;

}

int fm_reciver_check_freq(struct device *fm_dev, u32_t freq)
{
	struct acts_fm_device_data *fm_info = fm_dev->driver_data;
	struct fm_rx_device *sub_dev = NULL;

	if (!fm_info || !fm_info->rx_subdev)
		return -ENODEV;

	sub_dev = fm_info->rx_subdev;

	if (!sub_dev->check_inval_freq(sub_dev, freq)) {
		return 0;
	}

	return -EINVAL;

}

int fm_reciver_init(struct device *fm_dev, void *param)
{
	struct acts_fm_device_data *fm_info = NULL;
	struct fm_rx_device *sub_dev = NULL;

	if (!fm_dev || !fm_dev->driver_data)
		return -ENODEV;

	fm_info = fm_dev->driver_data;

	sub_dev = fm_info->rx_subdev;

	if (!sub_dev->init(sub_dev, param)) {
		//set extern pa in AB class
		u8_t pa_module = EXT_PA_CLASS_AB;
		struct device *aout_dev = device_get_binding(CONFIG_AUDIO_OUT_ACTS_DEV_NAME);
		audio_out_control(aout_dev, NULL, AOUT_CMD_PA_CLASS_SEL, &pa_module);
		return 0;
	}

	return -EINVAL;
}

int fm_reciver_deinit(struct device *fm_dev)
{
	struct acts_fm_device_data *fm_info = fm_dev->driver_data;
	struct fm_rx_device *sub_dev = NULL;

	if (!fm_info || !fm_info->rx_subdev)
		return -ENODEV;

	sub_dev = fm_info->rx_subdev;

	if (!sub_dev->deinit(sub_dev)) {
		//set extern pa in D class
#if (CONFIG_EXTERN_PA_CLASS == 1)
		u8_t pa_module = EXT_PA_CLASS_D;
		struct device *aout_dev = device_get_binding(CONFIG_AUDIO_OUT_ACTS_DEV_NAME);
		audio_out_control(aout_dev, NULL, AOUT_CMD_PA_CLASS_SEL, &pa_module);
#endif
		return 0;
	}

	return -EINVAL;
}

