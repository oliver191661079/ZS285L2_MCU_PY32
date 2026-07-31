/*
 * Copyright (c) 2018 Actions Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __USB_AUDIO_HANDLER_H_
#define __USB_AUDIO_HANDLER_H_
#include <drivers/usb/usb_dc.h>
#include <usb/usbstruct.h>
#ifndef SYS_LOG_DOMAIN
#define SYS_LOG_DOMAIN "usbaudio"
#endif
#ifndef SYS_LOG_LEVEL
#define SYS_LOG_LEVEL SYS_LOG_LEVEL_INFO
#endif
#include <logging/sys_log.h>


/* If endpoint direction is out */
#define USB_EP_DIR_IS_OUT(ep)	(USB_EP_ADDR2DIR(ep) == USB_EP_DIR_OUT)

/* If endpoint direction is in */
#define USB_EP_DIR_IS_IN(ep)	(USB_EP_ADDR2DIR(ep) == USB_EP_DIR_IN)

extern void usb_audio_tx_flush(void);

extern int usb_audio_tx(const u8_t *buf, u16_t len);

extern bool usb_audio_tx_enabled(void);

extern void usb_audio_set_tx_start(bool start);

extern int usb_hid_tx(const u8_t *buf, u16_t len);

extern u32_t usb_hid_get_call_status(void);

int usb_hid_device_init(void);

#endif /* __USB_AUDIO_HANDLER_H_ */
