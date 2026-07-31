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
#include <stdio.h>
#include <stdlib.h>
#include <misc/printk.h>
#include <fm.h>
#ifdef CONFIG_NVRAM_CONFIG
#include <nvram_config.h>
#endif

int fm_transmitter_set_freq(struct device *fm_dev, u32_t freq)
{
	struct acts_fm_device_data *fm_info = fm_dev->driver_data;
	struct fm_tx_device *sub_dev = NULL;

	if (!fm_info || !fm_info->tx_subdev) {
		return -ENODEV;
	}
	sub_dev = fm_info->tx_subdev;

	if (!sub_dev->set_freq) {
		return -EINVAL;
	}

	if (!sub_dev->set_freq(sub_dev, freq)) {
	#ifdef CONFIG_NVRAM_CONFIG
		char temp[8];

		memset(temp, 0, sizeof(temp));
		snprintf(temp, sizeof(temp), "%d", freq);

		nvram_config_set("FM_DEFAULT_FREQ", temp, sizeof(temp));
	#endif
		fm_info->current_freq = freq;
	}

	return 0;
}

int fm_transmitter_get_freq(struct device *fm_dev)
{
	struct acts_fm_device_data *fm_info = fm_dev->driver_data;

	if (!fm_info)
		return 0;

	printk("fm_transmitter_get_freq : %d HZ\n", fm_info->current_freq);

	return fm_info->current_freq;
}

int fm_transmitter_set_mute(struct device *fm_dev, bool flag)
{
	struct acts_fm_device_data *fm_info = fm_dev->driver_data;
	struct fm_tx_device *sub_dev = NULL;

	if (!fm_info || !fm_info->tx_subdev)
		return -ENODEV;

	sub_dev = fm_info->tx_subdev;

	if (!sub_dev->set_mute)
		return -EINVAL;

	if (fm_info->mute_flag != flag) {
		if (!sub_dev->set_mute(sub_dev, flag)) {
			fm_info->mute_flag = flag;
			return 0;
		}
	}

	return -EINVAL;
}
