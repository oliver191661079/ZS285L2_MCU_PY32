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
#include <board.h>
#include <app_manager.h>
#include <mem_manager.h>
#include <input_manager.h>
#include <msg_manager.h>
#include <sys_event.h>
#include <property_manager.h>

#define MAX_KEY_SUPPORT			10
#define MAX_HOLD_KEY_SUPPORT        4
#define MAX_MUTIPLE_CLICK_KEY_SUPPORT        4

#define LONG_PRESS_TIMER 10
#define SUPER_LONG_PRESS_TIMER             50    /* time */
#define SUPER_LONG_PRESS_6S_TIMER         150   /* time */
#define QUICKLY_CLICK_DURATION 300 /* ms */
#define KEY_EVENT_CANCEL_DURATION 50 /* ms */

struct input_manager_info {
	struct k_delayed_work work_item;
	struct k_delayed_work hold_key_work;
	event_trigger event_cb;
	u32_t press_type;
	u32_t press_code;
	u32_t report_key_value;
	int64_t report_stamp;
	u16_t press_timer : 12;
	u16_t click_num    : 4;
	bool input_manager_lock;
	bool key_hold;
	bool filter_itself;   /* filter all key event after current key event*/
};


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

struct input_manager_info *input_manager;

void report_key_event(struct k_work *work)
{
	struct input_manager_info *input = CONTAINER_OF(work, struct input_manager_info, work_item);

	if (input_manager->filter_itself) {
		if ((input->report_key_value & KEY_TYPE_SHORT_UP)
			|| (input->report_key_value & KEY_TYPE_DOUBLE_CLICK)
			|| (input->report_key_value & KEY_TYPE_TRIPLE_CLICK)
			|| (input->report_key_value & KEY_TYPE_LONG_UP)) {
			input_manager->filter_itself = false;
		}

		return;
	}

#ifdef CONFIG_INPUT_DEV_ACTS_ADC_SR
	if (input->press_type == EV_SR) {
		sys_event_report_srinput(&input->report_key_value);
		return ;
	}
#endif

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

void key_event_handle(struct device *dev, struct input_value *val)
{
	bool need_report = false;

	if (val->type != EV_KEY) {
		SYS_LOG_ERR("input type %d not support\n", val->type);
		return;
	}

	switch (val->value) {
	case KEY_VALUE_UP:
	{
		input_manager->press_code = val->code;
		input_manager->key_hold = false;
		if (input_manager->press_type == KEY_TYPE_LONG_DOWN
			 || input_manager->press_type == KEY_TYPE_LONG
			 || input_manager->press_type == KEY_TYPE_LONG6S) {
			input_manager->press_type = KEY_TYPE_LONG_UP;
		} else {
		#ifdef CONFIG_INPUT_MUTIPLE_CLICK
			if (is_support_mutiple_click(input_manager->press_code)) {
				if ((input_manager->report_key_value ==
					 (input_manager->press_type	| input_manager->press_code))
					&& k_uptime_delta_32(&input_manager->report_stamp) <= QUICKLY_CLICK_DURATION) {
					os_delayed_work_cancel(&input_manager->work_item);
					input_manager->click_num++;
					input_manager->report_stamp = k_uptime_get();
				}
				switch (input_manager->click_num) {
				case 0:
					input_manager->press_type = KEY_TYPE_SHORT_UP;
					break;
				case 1:
					input_manager->press_type = KEY_TYPE_DOUBLE_CLICK;
					break;
				case 2:
					input_manager->press_type = KEY_TYPE_TRIPLE_CLICK;
					break;
				}
			} else {
				input_manager->press_type = KEY_TYPE_SHORT_UP;
			}
		#else
			input_manager->press_type = KEY_TYPE_SHORT_UP;
		#endif
		}
		input_manager->press_timer = 0;
		need_report = true;
		break;
	}
	case KEY_VALUE_DOWN:
	{
		if (val->code != input_manager->press_code) {
			input_manager->press_code = val->code;
			input_manager->press_timer = 0;
			input_manager->press_type = 0;
			input_manager->filter_itself = false;
		} else {
			input_manager->press_timer++;

			if (input_manager->press_timer >= SUPER_LONG_PRESS_6S_TIMER) {
				if (input_manager->press_type != KEY_TYPE_LONG6S) {
					input_manager->press_type = KEY_TYPE_LONG6S;
					need_report = true;
				}
			} else if (input_manager->press_timer >= SUPER_LONG_PRESS_TIMER) {
				if (input_manager->press_type != KEY_TYPE_LONG) {
					input_manager->press_type = KEY_TYPE_LONG;
					need_report = true;
				}
			} else if (input_manager->press_timer >= LONG_PRESS_TIMER)	{
				if (input_manager->press_type != KEY_TYPE_LONG_DOWN) {
					input_manager->press_type = KEY_TYPE_LONG_DOWN;
					if (is_support_hold(input_manager->press_code)) {
						input_manager->key_hold = true;
						os_delayed_work_submit(&input_manager->hold_key_work, 500);
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
		input_manager->report_key_value = input_manager->press_type
									| input_manager->press_code;
		if (input_manager->event_cb) {
			input_manager->event_cb(input_manager->report_key_value, EV_KEY);
		}

		if (input_manager_islock()) {
			SYS_LOG_INF("input manager locked");
			return;
		}

		if (!os_is_free_msg_enough()) {
			SYS_LOG_INF("drop input msg ... %d", msg_pool_get_free_msg_num());
			return;
		}

	#ifdef CONFIG_INPUT_MUTIPLE_CLICK
		if (is_support_mutiple_click(input_manager->press_code)) {
			input_manager->report_stamp = k_uptime_get();
			os_delayed_work_submit(&input_manager->work_item, QUICKLY_CLICK_DURATION + KEY_EVENT_CANCEL_DURATION);
		} else {
			os_delayed_work_submit(&input_manager->work_item, 0);
		}
	#else
		os_delayed_work_submit(&input_manager->work_item, 0);
	#endif
	}
}

#ifdef CONFIG_ENCODER_INPUT

void encoder_event_handle(struct device *dev, struct input_value *val)
{
	bool need_report = false;

	if (val->type != EV_KEY) {
		SYS_LOG_ERR("input type %d not support\n", val->type);
		return;
	}

	if(val->value == 0)
		val->code = KEY_VOLUMEUP;
	else
		val->code = KEY_VOLUMEDOWN;

	input_manager->press_code = val->code;
	input_manager->key_hold = false;
	input_manager->press_type = KEY_TYPE_SHORT_UP;
	input_manager->press_timer = 0;
	need_report = true;

	if (need_report) {
		input_manager->report_key_value = input_manager->press_type
									| input_manager->press_code;
		if (input_manager->event_cb) {
			input_manager->event_cb(input_manager->report_key_value, EV_KEY);
		}

		if (input_manager_islock()) {
			SYS_LOG_INF("input manager locked");
			return;
		}

		if (!os_is_free_msg_enough()) {
			SYS_LOG_INF("drop input msg ... %d", msg_pool_get_free_msg_num());
			return;
		}

		os_delayed_work_submit(&input_manager->work_item, 0);
	}
}

#endif
static void check_hold_key(struct k_work *work)
{
	struct app_msg  msg = {0};

	msg.type = MSG_KEY_INPUT;
	if (input_manager->key_hold) {
		os_delayed_work_submit(&input_manager->hold_key_work, 200);

		if (!input_manager_islock()) {
			msg.value = KEY_TYPE_HOLD | input_manager->press_code;
			if (input_manager->event_cb) {
				input_manager->event_cb(msg.value, EV_KEY);
			}

			if (input_manager->filter_itself) {
				return;
			}
			send_async_msg("main", &msg);
		}
	} else {
		if (!input_manager_islock()) {
			msg.value = KEY_TYPE_HOLD_UP | input_manager->press_code;
			if (input_manager->event_cb) {
				input_manager->event_cb(msg.value, EV_KEY);
			}
			if (input_manager->filter_itself) {
				input_manager->filter_itself = false;
				return;
			}
			send_async_msg("main", &msg);
		}
	}
}

#ifdef CONFIG_INPUT_DEV_ACTS_ADC_SR
void sr_event_handle(struct device *dev, struct input_value *val)
{
	bool need_report = false;

	if (val->type != EV_SR) {
		SYS_LOG_ERR("input type %d not support\n", val->type);
		return;
	}

	/* note: not compatible with adckey hold key */

	input_manager->press_type = (u32_t)val->type;
	input_manager->key_hold = false;
	input_manager->press_timer = 0;
	need_report = true;

	if (need_report) {
		input_manager->report_key_value = (val->code << 16) | val->value;

		if (input_manager->event_cb) {
			input_manager->event_cb(input_manager->report_key_value, EV_SR);
		}

		if (input_manager_islock()) {
			SYS_LOG_INF("input manager locked");
			return;
		}

		if (!os_is_free_msg_enough()) {
			SYS_LOG_INF("drop input msg ... %d", msg_pool_get_free_msg_num());
			return;
		}

		os_delayed_work_submit(&input_manager->work_item, 0);
	}
}
#endif

static struct input_manager_info global_input_manager;

/*init manager*/
bool input_manager_init(event_trigger event_cb)
{
	input_manager = &global_input_manager;

	memset(input_manager, 0, sizeof(struct input_manager_info));

	os_delayed_work_init(&input_manager->work_item, report_key_event);

	os_delayed_work_init(&input_manager->hold_key_work, check_hold_key);

	input_manager->event_cb = event_cb;

	if (!key_device_open(&key_event_handle, "adckey")) {
		SYS_LOG_ERR("adckey devices open failed");
		return false;
	}

	if (!key_device_open(&key_event_handle, "onoff_key")) {
		SYS_LOG_ERR("onoffkey devices open failed");
	}

#ifdef CONFIG_ENCODER_INPUT
		if (!key_device_open(&encoder_event_handle, "encoder_ec11")) {
			SYS_LOG_ERR("encoder_ec11 devices open failed");
		}
#endif

#ifdef CONFIG_INPUT_DEV_ACTS_IRKEY
	if (!key_device_open(&key_event_handle, "irkey")) {
		SYS_LOG_ERR("irkey devices open failed");
	}
#endif

#ifdef CONFIG_INPUT_DEV_ACTS_ADC_SR
	if (!key_device_open(&sr_event_handle, CONFIG_INPUT_DEV_ACTS_ADC_SR_NAME)) {
		SYS_LOG_ERR("adcsr devices open failed");
	}
#endif

	SYS_LOG_INF("init success\n");
	input_manager_lock();

	return true;
}

bool input_manager_lock(void)
{
	unsigned int key;

	key = irq_lock();
	input_manager->input_manager_lock = true;
	irq_unlock(key);
	return true;
}

bool input_manager_unlock(void)
{
	unsigned int key;

	key = irq_lock();
	input_manager->input_manager_lock = false;
	irq_unlock(key);
	return true;
}

bool input_manager_islock(void)
{
	bool result = false;
	unsigned int key;

	key = irq_lock();
	result = input_manager->input_manager_lock;
	irq_unlock(key);
	return result;
}

bool input_manager_filter_key_itself(void)
{
	unsigned int key;

	key = irq_lock();
	input_manager->filter_itself = true;
	irq_unlock(key);

	return true;
}


