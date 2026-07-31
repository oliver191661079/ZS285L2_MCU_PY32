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
#include <i2c.h>
#include <gpio.h>

#if 1
#define SYS_LOG_DOMAIN "TOUCH_KEY"
#define SYS_LOG_LEVEL CONFIG_SYS_LOG_INPUT_DEV_LEVEL
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_TOUCH_KEY_ACTS

/* 20ms poll interval */
#define TOUCH_KEY_POLL_INTERVAL_MS	20

/* debouncing filter depth */
#define TOUCH_KEY_POLL_DEBOUNCING_DEP	3

// touch hy1600a-e
#define TOUCH_KEY_SLAVE_ADDR  0x53
//#define TOUCH_KEY_STATUE_RESIGTER  0xa7
#define TOUCH_KEY_INT_PIN 42

struct touch_key_info {
	struct k_timer timer;

	u8_t scan_count;
	u8_t prev_keycode;
	u8_t prev_stable_keycode;

	//u8_t touch_int_pin;
	//u8_t slave_addr;

	input_notify_t notify;

	struct device *input_touch_key;
	struct device *touch_key_gpio_int_pin;
};

struct acts_touchkey_config {
	char *i2c_name;
	char *gpio_name;

	//u8_t touch_int_pin;
	//u8_t slave_addr;
	//u16_t key_cnt;
	//const struct adckey_map *key_maps;
};

static void touchkey_acts_report_key(struct touch_key_info *touchkey,
				   int key_code, int value)
{
	struct input_value val;

	if (touchkey->notify) {
		val.type = EV_KEY;
		val.code = key_code;
		val.value = value;

		SYS_LOG_DBG("report key_code %d value %d",
			key_code, value);

		touchkey->notify(NULL, &val);
	}
}

/* register value -> key_code */
static u8_t tuchkey_acts_get_keycode(u8_t register_value)
{
	u8_t key_code = KEY_RESERVED;

	switch (register_value)
	{
		case 0x81:
			key_code = KEY_PREVIOUSSONG;
			break;

		case 0x82:
			key_code = KEY_BT;
			break;

		case 0x90:
			key_code = KEY_VOLUMEDOWN;
			break;

		case 0x88:
			key_code = KEY_PAUSE_AND_RESUME;
			break;

		case 0x84:
			key_code = KEY_TBD;
			break;

		case 0xa0:
			key_code = KEY_NEXTSONG;
			break;

		case 0xc0:
			key_code = KEY_VOLUMEUP;
			break;

		default:
			break;
	}

	return key_code;
}

static void touch_key_acts_poll(struct k_timer *timer)
{
	struct touch_key_info *touch = k_timer_user_data_get(timer);
	u8_t key_pressed = KEY_RESERVED;
	int int_pin_value = 0;

	/* no key is pressed or releasing */
	gpio_pin_read(touch->touch_key_gpio_int_pin, TOUCH_KEY_INT_PIN, &int_pin_value);
	if (int_pin_value != 0)
	{
		//u8_t touch_key_data[2];
		u8_t register_value = 0;
		int ret;

		ret = i2c_read(touch->input_touch_key, &register_value, 1, TOUCH_KEY_SLAVE_ADDR);
		if (ret == 0) {
			//ACT_LOG_ID_INF(ALF_STR_touch_key_acts_poll__READ_TOUCH_DATA_00XX, 2, touch_key_data[0], touch_key_data[1]);
			key_pressed = tuchkey_acts_get_keycode(register_value);
		}
	}

	/* no key is pressed or releasing */
	if (key_pressed == KEY_RESERVED &&
	    touch->prev_stable_keycode == KEY_RESERVED)
		return;

	if (key_pressed == touch->prev_keycode) {
		touch->scan_count++;
		if (touch->scan_count == TOUCH_KEY_POLL_DEBOUNCING_DEP) {
			/* previous key is released? */
			if (touch->prev_stable_keycode != KEY_RESERVED
				&& key_pressed != touch->prev_stable_keycode)
				touchkey_acts_report_key(touch, touch->prev_stable_keycode, 0);// key up

			/* a new key press? */
			if (key_pressed != KEY_RESERVED)
				touchkey_acts_report_key(touch, key_pressed, 1);// key down

			/* clear counter for new checking */
			touch->prev_stable_keycode = key_pressed;
			touch->scan_count = 0;
		}
	} else {
		/* new key pressed? */
		touch->prev_keycode = key_pressed;
		touch->scan_count = 0;
	}
}

static void touch_key_acts_enable(struct device *dev)
{
	struct touch_key_info *touchkey = dev->driver_data;

	i2c_configure(touchkey->input_touch_key,I2C_SPEED_STANDARD|I2C_MODE_MASTER);
	gpio_pin_configure(touchkey->touch_key_gpio_int_pin, TOUCH_KEY_INT_PIN, GPIO_DIR_IN | GPIO_PUD_PULL_DOWN);
}

static void touch_key_acts_disable(struct device *dev)
{

}

static void touch_key_acts_register_notify(struct device *dev, input_notify_t notify)
{
	struct touch_key_info *touch = dev->driver_data;

	ACT_LOG_ID_DBG(ALF_STR_touch_key_acts_register_notify__REGISTER_NOTIFY_0XX, 1, (u32_t)notify);

	touch->notify = notify;
}

static void touch_key_acts_unregister_notify(struct device *dev, input_notify_t notify)
{
	struct touch_key_info *touch = dev->driver_data;

	ACT_LOG_ID_DBG(ALF_STR_touch_key_acts_unregister_notify__UNREGISTER_NOTIFY_0X, 1, (u32_t)notify);

	touch->notify = NULL;
}

static const struct input_dev_driver_api touch_key_acts_driver_api = {
	.enable = touch_key_acts_enable,
	.disable = touch_key_acts_disable,
	.register_notify = touch_key_acts_register_notify,
	.unregister_notify = touch_key_acts_unregister_notify,
};

static int touch_key_acts_init(struct device *dev)
{
	struct touch_key_info *touch = dev->driver_data;
	const struct acts_touchkey_config *cfg = dev->config->config_info;

	ACT_LOG_ID_DBG(ALF_STR_touch_key_acts_init__INIT_TOUCH_KEY, 0);

	touch->input_touch_key = device_get_binding(cfg->i2c_name);
	if (!touch->input_touch_key) {
		SYS_LOG_ERR("cannot found adc dev %s\n", cfg->i2c_name);
		return -ENODEV;
	}

	touch->touch_key_gpio_int_pin = device_get_binding(cfg->gpio_name);
	if (!touch->touch_key_gpio_int_pin) {
		SYS_LOG_ERR("cannot found adc dev %s\n", cfg->gpio_name);
		return -ENODEV;
	}

	k_timer_init(&touch->timer, touch_key_acts_poll, NULL);
	k_timer_user_data_set(&touch->timer, touch);

	k_timer_start(&touch->timer, TOUCH_KEY_POLL_INTERVAL_MS,
		TOUCH_KEY_POLL_INTERVAL_MS);

	return 0;
}

static struct touch_key_info touch_key_acts_ddata;

static const struct acts_touchkey_config touchkey_acts_cdata = {
	.i2c_name = CONFIG_I2C_GPIO_2_NAME,
	.gpio_name = CONFIG_GPIO_ACTS_DEV_NAME,
};

DEVICE_AND_API_INIT(touch_key_acts, CONFIG_INPUT_DEV_ACTS_TOUCH_KEY_NAME,
		    touch_key_acts_init,
		    &touch_key_acts_ddata, &touchkey_acts_cdata,
		    PRE_KERNEL_2, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		    &touch_key_acts_driver_api);
#endif
