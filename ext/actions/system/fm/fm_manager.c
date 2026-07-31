/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file fm hal interface
 */
#include <os_common_api.h>
#include <mem_manager.h>
#include <string.h>
#include <gpio.h>
#include <ui_manager.h>
#include <fm_manager.h>

struct fm_manager_ctx_t {
	struct device *dev;
};

struct fm_manager_ctx_t fm_manager;

int fm_manager_init(void)
{
	memset(&fm_manager, 0, sizeof(struct fm_manager_ctx_t));

	fm_manager.dev = device_get_binding(CONFIG_FM_DEV_NAME);
	if (!fm_manager.dev) {
		return -ENODEV;
	}

	return 0;
}

int fm_rx_init(void *param)
{
	return fm_reciver_init(fm_manager.dev, param);
}

int fm_rx_set_freq(u32_t freq)
{
	return fm_reciver_set_freq(fm_manager.dev, freq);
}

int fm_rx_set_mute(bool mute)
{
	return fm_reciver_set_mute(fm_manager.dev, mute);
}

int fm_rx_check_freq(u32_t freq)
{
	return fm_reciver_check_freq(fm_manager.dev, freq);
}

int fm_tx_set_freq(u32_t freq)
{
	//return fm_transmitter_set_freq(fm_manager.dev, freq);
	return 0;
}

int fm_tx_get_freq(void)
{
	return fm_transmitter_get_freq(fm_manager.dev);
}

int fm_rx_deinit(void)
{
	return fm_reciver_deinit(fm_manager.dev);
}

int fm_manager_deinit(void)
{
	memset(&fm_manager, 0, sizeof(struct fm_manager_ctx_t));
	return 0;
}
