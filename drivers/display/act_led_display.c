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
#include <soc.h>
#include <logging/sys_log.h>
#include "mem_manager.h"
#include "display/led_display.h"
#include "display.h"
#include <pwm.h>

#define MAX_DUTY (32 * 255)

#ifdef BOARD_LED_MAP

const led_map ledmaps[] = {
    BOARD_LED_MAP
};

typedef struct
{
	u8_t     led_id;
	u8_t     led_pin;
	u8_t     led_pwm;
	u8_t     active_level;
	u8_t     state;
	led_pixel_value    pixel_value;
	struct device *op_dev;
}led_dot;

typedef struct
{
	u16_t led_dot_num;
	led_dot pixel[ARRAY_SIZE(ledmaps)];
}led_panel;

static led_panel *led = NULL;

static int led_display_init(struct device *dev)
{
	int i = 0;
	ARG_UNUSED(dev);

	led = mem_malloc(sizeof(led_panel));
	if ( !led ) {
		SYS_LOG_ERR("led malloc failed \n");
		return 0;
	}

	led->led_dot_num = ARRAY_SIZE(ledmaps);

	for (i = 0; i < led->led_dot_num; i ++) {
		led_dot *dot = &led->pixel[i];
		dot->led_pin =  ledmaps[i].led_pin;
		dot->led_id = ledmaps[i].led_id;
		dot->active_level = ledmaps[i].active_level;
		dot->led_pwm =  ledmaps[i].led_pwm;
		if (dot->led_pwm) {
		#ifdef CONFIG_PWM
			dot->op_dev = device_get_binding(CONFIG_PWM_ACTS_DEV_NAME);
        #endif
		} else {
			dot->op_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
			gpio_pin_configure(dot->op_dev, dot->led_pin, GPIO_DIR_OUT | GPIO_POL_INV);
		}
	    if (!dot->op_dev) {
			SYS_LOG_ERR("cannot found dev gpio \n");
			goto err_exit;
	    }
		memset(&dot->pixel_value, 0 , sizeof(dot->pixel_value));
	}

	return 0;

err_exit:
	if (led) {
		mem_free(led);
	}
	return -1;
}

SYS_INIT(led_display_init, POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);

#endif

int led_draw_pixel(int led_id, u32_t color, pwm_breath_ctrl_t *ctrl)
{
#ifdef BOARD_LED_MAP
	int i = 0;
	led_dot *dot = NULL;
	led_pixel_value pixel;

	memcpy(&pixel, &color, 4);

	for (i = 0; i < led->led_dot_num; i ++)	{
		dot = &led->pixel[i];
		if (dot->led_id == led_id) {
			break;
		}
	}

	if (!dot) {
		return 0;
	}

	dot->pixel_value = pixel;

	SYS_LOG_DBG("%d, 0x%x\n", led_id, color);
	switch (pixel.op_code) {
	case LED_OP_OFF:
		if (dot->led_pwm) {
		#ifdef CONFIG_PWM
			if (dot->active_level) {
				pwm_pin_set_cycles(dot->op_dev, dot->led_pwm, MAX_DUTY, 0, START_VOLTAGE_HIGH);
			} else {
				pwm_pin_set_cycles(dot->op_dev, dot->led_pwm, MAX_DUTY, MAX_DUTY, START_VOLTAGE_HIGH);
			}
		#endif
		} else {
			gpio_pin_write(dot->op_dev, dot->led_pin, !(dot->active_level));
		}

		break;
	case LED_OP_ON:
		if (dot->led_pwm) {
		#ifdef CONFIG_PWM
			if (dot->active_level) {
				pwm_pin_set_cycles(dot->op_dev, dot->led_pwm, MAX_DUTY, MAX_DUTY, START_VOLTAGE_HIGH);
			} else {
				pwm_pin_set_cycles(dot->op_dev, dot->led_pwm, MAX_DUTY, 0, START_VOLTAGE_HIGH);
			}
		#endif
		} else {
			gpio_pin_write(dot->op_dev, dot->led_pin, dot->active_level);
		}

		break;
	case LED_OP_BREATH:
		if (dot->led_pwm) {
		#ifdef CONFIG_PWM
			pwm_set_breath_mode(dot->op_dev, dot->led_pwm, ctrl);
		#endif
		}

		break;
	case LED_OP_BLINK:
		if (dot->led_pwm) {
		#ifdef CONFIG_PWM
			pwm_pin_set_usec(dot->op_dev, dot->led_pwm, dot->pixel_value.period * 1000, dot->pixel_value.pulse * 1000, dot->active_level);
		#endif
		}
	}
#endif
	return 0;
}

