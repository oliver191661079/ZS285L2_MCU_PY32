/*
 * Copyright (c) 2016 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief audio multicore sync.
*/

#include <os_common_api.h>
#include <mem_manager.h>
#include <msg_manager.h>
#include <audio_system.h>

#define GPIO_PIN_NUM (CONFIG_AUDIO_MULTICORE_SYNC_GPIO_NUM)

#include <gpio.h>

static struct device *sync_gpio_dev;

void audio_multicore_sync_init(void)
{
	sync_gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);

	gpio_pin_configure(sync_gpio_dev, GPIO_PIN_NUM, GPIO_DIR_OUT);

	gpio_pin_write(sync_gpio_dev, GPIO_PIN_NUM, 0);
}

void audio_multicore_sync_trigger_start(void)
{
	gpio_pin_write(sync_gpio_dev, GPIO_PIN_NUM, 1);
}


void audio_multicore_sync_trigger_stop(void)
{
	gpio_pin_write(sync_gpio_dev, GPIO_PIN_NUM, 0);
}
