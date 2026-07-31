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
#include <srv_manager.h>
#include <property_manager.h>
#include <input_manager_inner.h>

//K_MSGQ_DEFINE(keypad_scan_msgq, sizeof(input_dev_data_t), 10, 4);

#define MAX_KEY_SUPPORT			10
#define MAX_HOLD_KEY_SUPPORT        4
#define MAX_MUTIPLE_CLICK_KEY_SUPPORT        4

#define LONG_PRESS_TIMER 10
#define SUPER_LONG_PRESS_TIMER             50    /* time */
#define SUPER_LONG_PRESS_6S_TIMER         150   /* time */
#define QUICKLY_CLICK_DURATION 300 /* ms */
#define KEY_EVENT_CANCEL_DURATION 50 /* ms */

struct input_keypad_info {
	struct k_delayed_work report_work;
	struct k_delayed_work hold_key_work;
	event_trigger event_cb;
	uint32_t press_type;
	uint32_t press_code;
	uint32_t report_key_value;
	int64_t report_stamp;
	uint16_t press_timer : 12;
	uint16_t click_num    : 4;
	uint16_t key_hold:1;
	uint16_t filter_itself:1;   /* 过滤当前按键后续所有的事件 */
};

static struct input_keypad_info input_keypad;

static struct input_keypad_info *keypad;

#ifdef HOLDABLE_KEY
const char support_hold_key[MAX_HOLD_KEY_SUPPORT] = HOLDABLE_KEY;
#else
const char support_hold_key[MAX_HOLD_KEY_SUPPORT] = {0};
#endif

#ifdef MUTIPLE_CLIK_KEY
const char support_mutiple_key[MAX_MUTIPLE_CLICK_KEY_SUPPORT] = MUTIPLE_CLIK_KEY;
#else
const char support_mutiple_key[MAX_MUTIPLE_CLICK_KEY_SUPPORT] = {0};
#endif

void report_key_event_work_handle(struct k_work *work)
{
	struct input_keypad_info *input = CONTAINER_OF(work, struct input_keypad_info, report_work);

	if (input->filter_itself) {
		if ((input->report_key_value & KEY_TYPE_SHORT_UP)
			|| (input->report_key_value & KEY_TYPE_DOUBLE_CLICK)
			|| (input->report_key_value & KEY_TYPE_TRIPLE_CLICK)
			|| (input->report_key_value & KEY_TYPE_LONG_UP)) {
			input->filter_itself = false;
		}
		return;
	}

	input->click_num = 0;
	sys_event_report_input(input->report_key_value);
}

static bool is_support_hold(int key_code)
{
	for (int i = 0 ; i < MAX_HOLD_KEY_SUPPORT; i++) {
		if (support_hold_key[i] == key_code)	{
			return true;
		}
	}
	return false;
}

#ifdef CONFIG_INPUT_MUTIPLE_CLICK
static bool is_support_mutiple_click(int key_code)
{
	for (int i = 0 ; i < MAX_MUTIPLE_CLICK_KEY_SUPPORT; i++) {
		if (support_mutiple_key[i] == key_code)	{
			return true;
		}
	}
	return false;
}
#endif

static void check_hold_key_work_handle(struct k_work *work)
{
	struct input_keypad_info *keypad = CONTAINER_OF(work, struct input_keypad_info, hold_key_work);

	struct app_msg  msg = {0};

	msg.type = MSG_KEY_INPUT;

	if (keypad->key_hold) {
		os_delayed_work_submit(&keypad->hold_key_work, 200);

		if (!input_manager_islock()) {
			msg.value = KEY_TYPE_HOLD | keypad->press_code;
			if (keypad->event_cb) {
				keypad->event_cb(msg.value, EV_KEY);
			}

			if (keypad->filter_itself) {
				return;
			}
			send_async_msg(UI_SERVICE_NAME, &msg);
		}
	} else {
		if (!input_manager_islock()) {
			msg.value = KEY_TYPE_HOLD_UP | keypad->press_code;
			if (keypad->event_cb) {
				keypad->event_cb(msg.value, EV_KEY);
			}
			if (keypad->filter_itself) {
				keypad->filter_itself = false;
				return;
			}
			send_async_msg(UI_SERVICE_NAME, &msg);
		}
	}
}

static void keypad_scan_callback(struct device *dev, struct input_value *val)
{

	bool need_report = false;

	if (val->keypad.type != EV_KEY) {
		SYS_LOG_ERR("input type %d not support\n", val->keypad.type);
		return;
	}

	switch (val->keypad.value) {
	case KEY_VALUE_UP:
	{
		keypad->press_code = val->keypad.code;
		keypad->key_hold = false;
		if (keypad->press_type == KEY_TYPE_LONG_DOWN
			 || keypad->press_type == KEY_TYPE_LONG
			 || keypad->press_type == KEY_TYPE_LONG6S) {
			keypad->press_type = KEY_TYPE_LONG_UP;
		} else {
		#ifdef CONFIG_INPUT_MUTIPLE_CLICK
			if (is_support_mutiple_click(keypad->press_code)) {
				if ((keypad->report_key_value ==
					 (keypad->press_type	| keypad->press_code))
					&& k_uptime_delta_32(&report_stamp) <= QUICKLY_CLICK_DURATION) {
					os_delayed_work_cancel(&keypad->report_work);
					keypad->click_num++;
					keypad->report_stamp = k_uptime_get();
				}
				switch (keypad->click_num) {
				case 0:
					keypad->press_type = KEY_TYPE_SHORT_UP;
					break;
				case 1:
					keypad->press_type = KEY_TYPE_DOUBLE_CLICK;
					break;
				case 2:
					keypad->press_type = KEY_TYPE_TRIPLE_CLICK;
					break;
				}
			} else {
				keypad->press_type = KEY_TYPE_SHORT_UP;
			}
		#else
			keypad->press_type = KEY_TYPE_SHORT_UP;
		#endif
		}
		keypad->press_timer = 0;
		need_report = true;
		break;
	}
	case KEY_VALUE_DOWN:
	{
		if (val->keypad.code != keypad->press_code) {
			keypad->press_code = val->keypad.code;
			keypad->press_timer = 0;
			keypad->press_type = 0;
			keypad->filter_itself = false;
		} else {
			keypad->press_timer++;

			if (keypad->press_timer >= SUPER_LONG_PRESS_6S_TIMER) {
				if (keypad->press_type != KEY_TYPE_LONG6S) {
					keypad->press_type = KEY_TYPE_LONG6S;
					need_report = true;
				}
			} else if (keypad->press_timer >= SUPER_LONG_PRESS_TIMER) {
				if (keypad->press_type != KEY_TYPE_LONG) {
					keypad->press_type = KEY_TYPE_LONG;
					need_report = true;
				}
			} else if (keypad->press_timer >= LONG_PRESS_TIMER)	{
				if (keypad->press_type != KEY_TYPE_LONG_DOWN) {
					keypad->press_type = KEY_TYPE_LONG_DOWN;
					if (is_support_hold(keypad->press_code)) {
						keypad->key_hold = true;
						os_delayed_work_submit(&keypad->hold_key_work, 500);
					}
					need_report = true;
				}
			}

		}
		break;
	}
	default:
		break;
	}

	if (need_report) {
		keypad->report_key_value = keypad->press_type
									| keypad->press_code;
		if (keypad->event_cb) {
			keypad->event_cb(keypad->report_key_value, EV_KEY);
		}

		if (!os_is_free_msg_enough()) {
			SYS_LOG_INF("drop input msg ... %d", msg_pool_get_free_msg_num());
			return;
		}

	#ifdef CONFIG_INPUT_MUTIPLE_CLICK
		if (is_support_mutiple_click(keypad->press_code)) {
			keypad->report_stamp = k_uptime_get();
			os_delayed_work_submit(&keypad->report_work, QUICKLY_CLICK_DURATION + KEY_EVENT_CANCEL_DURATION);
		} else {
			os_delayed_work_submit(&keypad->report_work, 0);
		}
	#else
		os_delayed_work_submit(&keypad->report_work, 0);
	#endif
	}

}

int input_keypad_device_init(void)
{
	keypad = &input_keypad;
	memset(keypad, 0, sizeof(struct input_keypad_info));

	os_delayed_work_init(&keypad->report_work, report_key_event_work_handle);
	os_delayed_work_init(&keypad->hold_key_work, check_hold_key_work_handle);

	if (!key_device_open(&keypad_scan_callback, ADC_KEY_DEVICE_NAME)) {
		SYS_LOG_ERR("adckey devices open failed");
		return false;
	}

	if (!key_device_open(&keypad_scan_callback, ONOFF_KEY_DEVICE_NAME)) {
		SYS_LOG_ERR("onoffkey devices open failed");
	}

	return 0;
}
