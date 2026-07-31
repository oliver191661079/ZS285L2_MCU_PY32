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
#include <os_common_api.h>
#include <fm.h>

static struct acts_fm_device_data fm_dev_data;
static os_delayed_work fm_delaywork;

int fm_receiver_device_register(struct fm_rx_device *device)
{
	if (fm_dev_data.rx_subdev)
		return -ENOSPC;

	fm_dev_data.rx_subdev = device;

	return 0;
}


int fm_transmitter_device_register(struct fm_tx_device *device)
{
	if (fm_dev_data.tx_subdev)
		return -ENOSPC;

	fm_dev_data.tx_subdev = device;

	return 0;
}
#ifdef CONFIG_FM_QN8035
extern int qn8035_init(void);
#endif

#ifdef CONFIG_FM_RDA5820
extern int rda5820_rx_init(void);
extern int rda5820_tx_init(void);
#endif

static void fm_init_delaywork(os_work *work)
{
#ifdef CONFIG_FM_QN8035
	qn8035_init();
#endif

#ifdef CONFIG_FM_RDA5820
	rda5820_rx_init();
	rda5820_tx_init();
#endif

#ifdef CONFIG_FM_EXT_24M
	sys_write32(0, CMU_FMOUTCLK);/* 0 is 1/2 HOSC ,1 is HOSC */
	sys_write32((sys_read32(CMU_DEVCLKEN0) | BIT(20)), CMU_DEVCLKEN0);
#endif
}

static int acts_fm_init(struct device *dev)
{

	os_delayed_work_init(&fm_delaywork, fm_init_delaywork);
	os_delayed_work_submit(&fm_delaywork, 0);

	return 0;
}

const struct fm_driver_api fm_acts_driver_api = {
	.enable = NULL,
	.disable = NULL,
	.set_freq = NULL,
	.get_freq = NULL,
	.check_freq = NULL,
	.set_mute = NULL,
};

DEVICE_AND_API_INIT(fm, CONFIG_FM_DEV_NAME, acts_fm_init, \
		&fm_dev_data, NULL, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &fm_acts_driver_api);
