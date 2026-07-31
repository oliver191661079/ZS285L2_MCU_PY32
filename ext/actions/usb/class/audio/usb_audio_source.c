/*
 * USB Audio Source Device (ISO-Transfer) class core.
 *
 * Copyright (c) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <init.h>
#include <misc/byteorder.h>
#include <usb/usb_device.h>
#include <usb/usb_common.h>
#include <usb/class/usb_audio.h>
#ifdef CONFIG_NVRAM_CONFIG
#include <string.h>
#include <nvram_config.h>
#endif

#define SYS_LOG_LEVEL CONFIG_SYS_LOG_USB_SOURCE_LEVEL
#define SYS_LOG_DOMAIN "usb_audio_source"
#include <logging/sys_log.h>

#include "usb_audio_source_desc.h"

/* high byte: feature unit ID
 * low byte:  interface number(Standard  Audio Control Interface Descriptor)
 */
#define FEATURE_UNIT1_INDEX	0x0500
/* according to spec */
static int audio_source_mute_vol = 0x8000;
static int audio_source_ch0_cur_vol;
static int audio_source_max_volume;
static int audio_source_min_volume;
static int audio_source_volume_step;

static usb_audio_start audio_source_start_cb;
static usb_ep_callback iso_in_ep_cb;
static usb_audio_volume_sync source_vol_sync_cb;

static bool audio_upload_streaming_enabled;

static void usb_audio_isoc_in(u8_t ep, enum usb_dc_ep_cb_status_code cb_status)
{
	SYS_LOG_DBG("**isoc_in_cb!**\n");
	if (iso_in_ep_cb) {
		iso_in_ep_cb(ep, cb_status);
	}
}

static void usb_audio_status_cb(enum usb_dc_status_code status, u8_t *param)
{
	static u8_t alt_setting;
	u8_t iface;

	switch (status) {
	case USB_DC_INTERFACE:
		/* iface: the higher byte, alt_setting: the lower byte */
		iface = *(u16_t *)param >> 8;
		alt_setting = *(u16_t *)param & 0xff;
		SYS_LOG_DBG("Set_Int: %d, alt_setting: %d\n", iface, alt_setting);
		switch (iface) {
		case AUDIO_STREAM_INTER3:
			if (!alt_setting) {
				audio_upload_streaming_enabled = false;
			} else {
				audio_upload_streaming_enabled = true;
			}

			if (audio_source_start_cb) {
				audio_source_start_cb(audio_upload_streaming_enabled);
			}
			break;

		default:
			SYS_LOG_ERR("Unavailable interface number");
			break;
		}
	break;

	case USB_DC_ERROR:
		SYS_LOG_ERR("USB device error");
		break;

	case USB_DC_RESET:
		audio_upload_streaming_enabled = false;
		if (audio_source_start_cb) {
			audio_source_start_cb(audio_upload_streaming_enabled);
		}
		SYS_LOG_DBG("USB device reset detected");
		break;

	case USB_DC_CONNECTED:
		SYS_LOG_DBG("USB device connected");
		break;

	case USB_DC_CONFIGURED:
		SYS_LOG_DBG("USB device configured");
		break;

	case USB_DC_DISCONNECTED:
		audio_upload_streaming_enabled = false;
		if (audio_source_start_cb) {
			audio_source_start_cb(audio_upload_streaming_enabled);
		}
		SYS_LOG_DBG("USB device disconnected");
		break;

	case USB_DC_SUSPEND:
		audio_upload_streaming_enabled = false;
		if (audio_source_start_cb) {
			audio_source_start_cb(audio_upload_streaming_enabled);
		}
		SYS_LOG_DBG("USB device suspended");
		break;

	case USB_DC_RESUME:
		SYS_LOG_DBG("USB device resumed");
		break;

	case USB_DC_HIGHSPEED:
		SYS_LOG_INF("High-Speed mode handshake package");
		break;

	case USB_DC_SOF:
		SYS_LOG_DBG("USB device sof inter");
		break;

	case USB_DC_UNKNOWN:

	default:
		SYS_LOG_DBG("USB unknown state");
		break;
	}
}

/* process main channel only */
static void usb_audio_source_handle_mute_ctrl(struct usb_setup_packet *psetup, u8_t *data)
{
	SYS_LOG_DBG("audio source mute_ch0:%d %d", audio_source_mute_vol, data[0]);
	if (audio_source_mute_vol != data[0]) {
		audio_source_mute_vol = data[0];
		if (source_vol_sync_cb) {
			if (audio_source_mute_vol == 1) {
				source_vol_sync_cb(UMIC_SYNC_HOST_MUTE, LOW_BYTE(psetup->wValue), &audio_source_mute_vol);
			} else {
				source_vol_sync_cb(UMIC_SYNC_HOST_UNMUTE, LOW_BYTE(psetup->wValue), &audio_source_mute_vol);
			}
		}
	}
}

/* process main channel only */
static void usb_audio_source_handle_vol_ctrl(struct usb_setup_packet *psetup, u8_t *data)
{
	audio_source_ch0_cur_vol = (data[1] << 8) | (data[0]);
	SYS_LOG_DBG("source set main_ch vol:0x%04x", audio_source_ch0_cur_vol);
	if (audio_source_ch0_cur_vol == 0) {
		audio_source_ch0_cur_vol = 65536;
	}

	if (source_vol_sync_cb) {
		source_vol_sync_cb(UMIC_SYNC_HOST_VOL_TYPE, LOW_BYTE(psetup->wValue), &audio_source_ch0_cur_vol);
	}
}

static void usb_audio_source_handle_req_in(struct usb_setup_packet *psetup, s32_t *len, u8_t **data)
{
	if (psetup->wValue == ((MUTE_CONTROL << 8) | MAIN_CHANNEL_NUMBER0)) {
		SYS_LOG_DBG("source get main_ch mute vol");
		*data = (u8_t *)&audio_source_mute_vol;
		*len  = MUTE_LENGTH;
	} else if (psetup->wValue == ((VOLUME_CONTROL << 8) | MAIN_CHANNEL_NUMBER0)) {
		SYS_LOG_DBG("source get main_ch vol");
		*data = (u8_t *) &audio_source_ch0_cur_vol;
		*len  = VOLUME_LENGTH;
	}
}

static int usb_audio_class_handle_req(struct usb_setup_packet *psetup,
							s32_t *len, u8_t **data)
{
	int ret = -ENOTSUP;
	u8_t *temp_data = *data;

	switch (psetup->bmRequestType) {
	case SPECIFIC_REQUEST_OUT:
	if (psetup->wIndex == FEATURE_UNIT1_INDEX) {
		if (psetup->bRequest == UAC_SET_CUR) {
			if (psetup->wValue == ((MUTE_CONTROL << 8) | MAIN_CHANNEL_NUMBER0)) {
				usb_audio_source_handle_mute_ctrl(psetup, temp_data);
			} else if (psetup->wValue == ((VOLUME_CONTROL << 8) | MAIN_CHANNEL_NUMBER0)) {
				usb_audio_source_handle_vol_ctrl(psetup, temp_data);
			}
		}
	}
	ret = 0;
	break;

	case SPECIFIC_REQUEST_IN:
	if (psetup->wIndex == FEATURE_UNIT1_INDEX) {
		if (psetup->bRequest == UAC_GET_CUR) {
			usb_audio_source_handle_req_in(psetup, len, data);
		} else if (psetup->bRequest == UAC_GET_MIN) {
			*data = (u8_t *) &audio_source_min_volume;
			*len  = VOLUME_LENGTH;
		} else if (psetup->bRequest == UAC_GET_MAX) {
			*data = (u8_t *) &audio_source_max_volume;
			*len  = VOLUME_LENGTH;
		} else if(psetup->bRequest == UAC_GET_RES) {
			*data = (u8_t *) &audio_source_volume_step;
			*len  = VOLUME_LENGTH;
		}
	}
	ret = 0;
	break;
	}
	return ret;
}

static int usb_audio_custom_handle_req(struct usb_setup_packet *setup, s32_t *len,
					u8_t **data)
{
	SYS_LOG_DBG("custom request: 0x%x 0x%x %d",
		    setup->bRequest, setup->bmRequestType, *len);
	return -ENOTSUP;
}

static int usb_audio_vendor_handle_req(struct usb_setup_packet *setup, s32_t *len,
					u8_t **data)
{
	SYS_LOG_DBG("vendor request: 0x%x 0x%x %d",
		    setup->bRequest, setup->bmRequestType, *len);
	return -ENOTSUP;
}

/* USB endpoint configuration */
static const struct usb_ep_cfg_data usb_audio_source_ep_cfg[] = {
	{
		.ep_cb	= usb_audio_isoc_in,
		.ep_addr = CONFIG_USB_AUDIO_SOURCE_IN_EP_ADDR,
	},
};

static const struct usb_cfg_data usb_audio_source_cfg = {
	.usb_device_description = NULL,
	.cb_usb_status = usb_audio_status_cb,
	.interface = {
		.class_handler  = usb_audio_class_handle_req,
		.custom_handler = usb_audio_custom_handle_req,
		.vendor_handler = usb_audio_vendor_handle_req,
	},
	.num_endpoints = ARRAY_SIZE(usb_audio_source_ep_cfg),
	.endpoint = usb_audio_source_ep_cfg,
};

static int usb_audio_source_fix_dev_sn(void)
{
	int ret;
#ifdef CONFIG_NVRAM_CONFIG
	int read_len;

	u8_t mac_str[CONFIG_USB_DEVICE_STRING_DESC_MAX_LEN];

	read_len = nvram_config_get(CONFIG_USB_AUDIO_SOURCE_SN_NVRAM, mac_str, CONFIG_USB_DEVICE_STRING_DESC_MAX_LEN);
	if (read_len < 0) {
		SYS_LOG_DBG("no sn data in nvram: %d", read_len);
		ret = usb_device_register_string_descriptor(DEV_SN_DESC, CONFIG_USB_AUDIO_SOURCE_SN, strlen(CONFIG_USB_AUDIO_SOURCE_SN));
		if (ret)
			return ret;
	} else {
		ret = usb_device_register_string_descriptor(DEV_SN_DESC, mac_str, read_len);
		if (ret)
			return ret;
	}
#else
	ret = usb_device_register_string_descriptor(DEV_SN_DESC, CONFIG_USB_AUDIO_SOURCE_SN, strlen(CONFIG_USB_AUDIO_SOURCE_SN));
	if (ret) {
		return ret;
	}
#endif
	return 0;
}

/*
 * API: initialize usb audio source device.
 */
int usb_audio_source_init(struct device *dev)
{
	int ret;

	/* register string descriptor */
	ret = usb_device_register_string_descriptor(MANUFACTURE_STR_DESC, CONFIG_USB_AUDIO_SOURCE_MANUFACTURER, strlen(CONFIG_USB_AUDIO_SOURCE_MANUFACTURER));
	if (ret) {
		return ret;
	}
	ret = usb_device_register_string_descriptor(PRODUCT_STR_DESC, CONFIG_USB_AUDIO_SOURCE_PRODUCT, strlen(CONFIG_USB_AUDIO_SOURCE_PRODUCT));
	if (ret) {
		return ret;
	}
	ret = usb_audio_source_fix_dev_sn();
	if (ret) {
		return ret;
	}

	/* register device descriptor */
	usb_device_register_descriptors(usb_audio_source_fs_descriptor, usb_audio_source_hs_descriptor);

	/* initialize the USB driver with the right configuration */
	ret = usb_set_config(&usb_audio_source_cfg);
	if (ret < 0) {
		SYS_LOG_ERR("Failed to config USB");
		return ret;
	}

	/* enable USB driver */
	ret = usb_enable(&usb_audio_source_cfg);
	if (ret < 0) {
		SYS_LOG_ERR("Failed to enable USB");
		return ret;
	}

	return 0;
}

/*
 * API: deinitialize usb audio source device.
 */
int usb_audio_source_deinit(void)
{
	int ret;

	ret = usb_disable();
	if (ret) {
		SYS_LOG_ERR("Failed to disable USB");
		return ret;
	}

	usb_deconfig();

	return 0;
}

void usb_audio_source_register_start_cb(usb_audio_start cb)
{
	audio_source_start_cb = cb;
}

void usb_audio_source_register_inter_in_ep_cb(usb_ep_callback cb)
{
	iso_in_ep_cb = cb;
}

void usb_audio_source_register_volume_sync_cb(usb_audio_volume_sync cb)
{
	source_vol_sync_cb = cb;
}

int usb_audio_source_ep_flush(void)
{
	return usb_dc_ep_flush(CONFIG_USB_AUDIO_SOURCE_IN_EP_ADDR);
}

void usb_audio_source_set_cur_vol(uint8_t channel_type, int vol_dat)
{
	if (channel_type == MAIN_CH) {
		audio_source_ch0_cur_vol = vol_dat;
	}
}

void usb_audio_source_set_vol_info(uint8_t vol_type, int vol_dat)
{
	if (vol_type == MAXIMUM_VOLUME) {
		audio_source_max_volume = vol_dat;
	} else if (vol_type == MINIMUM_VOLUME) {
		audio_source_min_volume = vol_dat;
	} else if (vol_type == VOLUME_STEP) {
		audio_source_volume_step = vol_dat;
	}
}

