
/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief hotplog detect device driver interface
 */

#ifndef __INCLUDE_HOTPLOG_H__
#define __INCLUDE_HOTPLOG_H__

#include <stdint.h>
#include <device.h>
#include <thread_timer.h>

#ifdef __cplusplus
extern "C" {
#endif

enum linein_state
{
    LINEIN_NONE,
    LINEIN_OUT,
    LINEIN_IN
};

struct hotplog_detect_driver_api {
	int (*detect_state)(struct device *dev, int *state);
};

static inline int hotplog_detect_state(struct device *dev, int *state)
{
	const struct hotplog_detect_driver_api *api = dev->driver_api;

	return api->detect_state(dev, state);
}

typedef enum HOTPLGU_USB_state
{
    USB_NONE,
    USB_OUT,
    USB_IN,
    ADAPTER_IN,
    ADAPTER_OUT
}hotplug_usb_state_t;

typedef enum HOTPLGU_SDCARD_state
{
    SDCARD_NONE,
    SDCARD_OUT,
    SDCARD_IN
}hotplug_sdcard_state_t;

typedef enum HOTPLGU_USBHOST_state
{
    USBHOST_NONE,
    USBHOST_OUT,
    USBHOST_IN
}hotplug_usbhost_state_t;

extern int usb_hotplug_init(struct device *dev);
extern void usb_switch_handle(struct thread_timer *ttimer, void *expiry_fn_arg);
extern void detect_usb_isr_fix(struct thread_timer *ttimer, void *expiry_fn_arg);
extern void detect_uhost_isr_fix(struct thread_timer *ttimer, void *expiry_fn_arg);
extern int usb_hotplug_detect_init(int mode);
extern bool usb_hotplug_device_mode(void);
extern bool usb_hotplug_peripheral_mode(void);
extern bool usb_get_attched_status(void);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif  /* __INCLUDE_HOTPLOG_H__ */
