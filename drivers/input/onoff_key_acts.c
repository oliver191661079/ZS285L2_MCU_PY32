/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Actions SoC On/Off Key driver
 */

#include <errno.h>
#include <kernel.h>
#include <string.h>
#include <init.h>
#include <irq.h>
#include <soc.h>
#include <input_dev.h>
#include <misc/util.h>

#define SYS_LOG_DOMAIN "ONOFF_KEY"
#define SYS_LOG_LEVEL CONFIG_SYS_LOG_INPUT_DEV_LEVEL
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_ONOFF_KEY_ACTS

/* 20ms poll interval */
#define ONOFF_KEY_POLL_INTERVAL_MS	20

/* debouncing filter depth */
#define ONOFF_KEY_POLL_DEBOUNCING_DEP	3

/* bits define */
#define PMU_ONOFF_KEY_ONOFF_PRESSED	(1 << 0)

struct onoff_key_info {
	struct k_timer timer;

	u8_t scan_count;
	u8_t prev_key_pressed;
	u8_t prev_stable_key_pressed;
	u8_t reserved;

	input_notify_t notify;
};

static void onoff_key_acts_report_key(struct onoff_key_info *onoff, int value)
{
	struct input_value val;

	if (onoff->notify) {
		val.type = EV_KEY;
		val.code = KEY_POWER;
		val.value = value;

		onoff->notify(NULL, &val);
	}
}

static void onoff_key_acts_poll(struct k_timer *timer)
{
	struct onoff_key_info *onoff = k_timer_user_data_get(timer);
	int key_pressed;

	/* no key is pressed or releasing */
	key_pressed = !!(sys_read32(PMU_ONOFF_KEY) & PMU_ONOFF_KEY_ONOFF_PRESSED);

	/* no key is pressed or releasing */
	if (!key_pressed && !onoff->prev_stable_key_pressed)
		return;

	if (key_pressed == onoff->prev_key_pressed) {
		onoff->scan_count++;
		if (onoff->scan_count == ONOFF_KEY_POLL_DEBOUNCING_DEP) {
			/* previous key is released? */
			if (!key_pressed && onoff->prev_stable_key_pressed)
				onoff_key_acts_report_key(onoff, 0);

			/* stable key pressed? */
			if (key_pressed)
				onoff_key_acts_report_key(onoff, 1);

			/* clear counter for new checking */
			onoff->prev_stable_key_pressed = key_pressed;
			onoff->scan_count = 0;
		}
	} else {
		/* new key pressed? */
		onoff->prev_key_pressed = key_pressed;
		onoff->scan_count = 0;
	}
}

static void onoff_key_acts_enable(struct device *dev)
{
}

static void onoff_key_acts_disable(struct device *dev)
{
}

static void onoff_key_acts_register_notify(struct device *dev, input_notify_t notify)
{
	struct onoff_key_info *onoff = dev->driver_data;

	ACT_LOG_ID_DBG(ALF_STR_onoff_key_acts_register_notify__REGISTER_NOTIFY_0XX, 1, (u32_t)notify);

	onoff->notify = notify;
}

static void onoff_key_acts_unregister_notify(struct device *dev, input_notify_t notify)
{
	struct onoff_key_info *onoff = dev->driver_data;

	ACT_LOG_ID_DBG(ALF_STR_onoff_key_acts_unregister_notify__UNREGISTER_NOTIFY_0X, 1, (u32_t)notify);

	onoff->notify = NULL;
}

static const struct input_dev_driver_api onoff_key_acts_driver_api = {
	.enable = onoff_key_acts_enable,
	.disable = onoff_key_acts_disable,
	.register_notify = onoff_key_acts_register_notify,
	.unregister_notify = onoff_key_acts_unregister_notify,
};

static int onoff_key_acts_init(struct device *dev)
{
	struct onoff_key_info *onoff = dev->driver_data;

	ACT_LOG_ID_DBG(ALF_STR_onoff_key_acts_init__INIT_ONOFF_KEY, 0);

	k_timer_init(&onoff->timer, onoff_key_acts_poll, NULL);
	k_timer_user_data_set(&onoff->timer, onoff);

	k_timer_start(&onoff->timer, ONOFF_KEY_POLL_INTERVAL_MS,
		ONOFF_KEY_POLL_INTERVAL_MS);

	return 0;
}

static struct onoff_key_info onoff_key_acts_ddata;

DEVICE_AND_API_INIT(onoff_key_acts, CONFIG_INPUT_DEV_ACTS_ONOFF_KEY_NAME,
		    onoff_key_acts_init,
		    &onoff_key_acts_ddata, NULL,
		    POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		    &onoff_key_acts_driver_api);
