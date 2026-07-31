/*
 * USB Audio Class -- Audio Source Sink driver
 *
 * Copyright (C) 2020 Actions Corporation.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * NOTE: Implement the interfaces which support audio sourcesink and
 * composite device(HID + UAC), support UAC version 1.0 and works in
 * both Full-speed and High-speed modes.
 *
 * Function:
 * # support multi sample rate and multi channels.
 * # add buffer and timing statistics.
 * # more flexible configuration.
 * # support feature unit.
 * # support high-speed.
 * # support version 2.0.
 */
#include <kernel.h>
#include <init.h>
#include <string.h>
#include <misc/byteorder.h>
#include <usb/usb_device.h>
#include <usb/usb_common.h>
#include <usb/class/usb_audio.h>
#include "audio_sourcesink_desc.h"
#ifdef CONFIG_NVRAM_CONFIG
#include <string.h>
#include <nvram_config.h>
#endif

#define SYS_LOG_LEVEL CONFIG_SYS_LOG_USB_SOURCESINK_LEVEL
#define SYS_LOG_DOMAIN "audio_sourcesink"
#include <logging/sys_log.h>

#define GET_SAMPLERATE		0xA2
#define SET_ENDPOINT_CONTROL	0x22
#define SAMPLING_FREQ_CONTROL	0x0100

#define FEATURE_UNIT_INDEX1	0x0901
#define FEATURE_UNIT_INDEX2	0x0501

#define AUDIO_STREAM_INTER2	2
#define AUDIO_STREAM_INTER3	3

/* according to spec */
static int usb_audio_mute_vol = 0x8000;

static int sink_ch0_cur_vol;
static int sink_ch1_cur_vol;
static int sink_ch2_cur_vol;
/* three channels use the same value */
static int sink_max_volume;
static int sink_min_volume;
static int sink_volume_step;

static int source_ch0_cur_vol;
/* three channels use the same value */
static int source_max_volume;
static int source_min_volume;
static int source_volume_step;

static u32_t g_cur_sample_rate = CONFIG_USB_AUDIO_DEVICE_SINK_SAM_FREQ_DOWNLOAD;

static usb_ep_callback iso_in_ep_cb;
static usb_ep_callback iso_out_ep_cb;
static usb_audio_start audio_sink_start_cb;
static usb_audio_start audio_source_start_cb;
static usb_audio_pm audio_device_pm_cb;

static usb_sample_rate_change audio_sink_sam_change_cb;

static usb_audio_volume_sync audio_sink_vol_ctrl_cb;
static usb_audio_volume_sync audio_source_vol_ctrl_cb;

static bool audio_download_streaming_enabled;
static bool audio_upload_streaming_enabled;

bool usb_audio_get_download_streaming_enabled(void)
{
	return audio_download_streaming_enabled;
}

static const struct usb_if_descriptor usb_audio_interface  = {
	.bLength = sizeof(struct usb_if_descriptor),
	.bDescriptorType = USB_INTERFACE_DESC,
	.bInterfaceNumber = CONFIG_USB_AUDIO_DEVICE_IF_NUM,
	.bNumEndpoints = 0,
	.bInterfaceClass = USB_CLASS_AUDIO,
	.bInterfaceSubClass = USB_SUBCLASS_AUDIOCONTROL,
};

static void usb_audio_status_cb(enum usb_dc_status_code status, u8_t *param)
{
	static u8_t alt_setting, iface;

	/* Check the USB status and do needed action if required */
	switch (status) {
	case USB_DC_INTERFACE:
		/* iface: the higher byte, alt_setting: the lower byte */
		iface = *(u16_t *)param >> 8;
		alt_setting = *(u16_t *)param & 0xff;
		SYS_LOG_DBG("Set_Int: %d, alt_setting: %d", iface, alt_setting);
		switch (iface) {
		case AUDIO_STREAM_INTER2:
			if (!alt_setting) {
				audio_download_streaming_enabled = false;
			} else {
				audio_download_streaming_enabled = true;

				#if CONFIG_USB_SINK_UAC_MODE > UAC_MODE_UNSUPPORTED
					/* 16bit */
					if(alt_setting == 1)
					{
						USB_AudioSinkBitDepthSet(UAC_16BIT_DEPTH);
					}
					/* 24bit */
					else
					{
						USB_AudioSinkBitDepthSet(UAC_24BIT_DEPTH);
					}
				#endif
			}

			if (audio_sink_start_cb) {
				audio_sink_start_cb(audio_download_streaming_enabled);
			}
			break;

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
			SYS_LOG_WRN("Unavailable interface number");
			break;
		}
		break;

	case USB_DC_ALTSETTING:
		/* iface: the higher byte, alt_setting: the lower byte */
		iface = *(u16_t *)param >> 8;
		*(u16_t *)param = (iface << 8) + alt_setting;
		SYS_LOG_DBG("Get_Int: %d, alt_setting: %d", iface, alt_setting);
		break;

	case USB_DC_ERROR:
		SYS_LOG_DBG("USB device error");
		break;

	case USB_DC_RESET:
		audio_upload_streaming_enabled = false;
		if (audio_source_start_cb) {
			audio_source_start_cb(audio_upload_streaming_enabled);
		}

		audio_download_streaming_enabled = false;
		if (audio_sink_start_cb) {
			audio_sink_start_cb(audio_download_streaming_enabled);
		}
		SYS_LOG_DBG("USB device reset detected");
		break;

	case USB_DC_CONNECTED:
		SYS_LOG_DBG("USB device connected");
		break;

	case USB_DC_CONFIGURED:
		SYS_LOG_DBG("USB configuration done");
		break;

	case USB_DC_DISCONNECTED:
		audio_upload_streaming_enabled = false;
		if (audio_source_start_cb) {
			audio_source_start_cb(audio_upload_streaming_enabled);
		}

		audio_download_streaming_enabled = false;
		if (audio_sink_start_cb) {
			audio_sink_start_cb(audio_download_streaming_enabled);
		}
		SYS_LOG_DBG("USB device disconnected");
		break;

	case USB_DC_SUSPEND:
		audio_upload_streaming_enabled = false;
		if (audio_source_start_cb) {
			audio_source_start_cb(audio_upload_streaming_enabled);
		}

		audio_download_streaming_enabled = false;
		if (audio_sink_start_cb) {
			audio_sink_start_cb(audio_download_streaming_enabled);
		}

		if (audio_device_pm_cb) {
			audio_device_pm_cb(true);
		}
		SYS_LOG_DBG("USB device suspended");
		break;

	case USB_DC_RESUME:
		SYS_LOG_DBG("USB device resumed");
		if (audio_device_pm_cb) {
			audio_device_pm_cb(false);
		}
		break;

	case USB_DC_HIGHSPEED:
		SYS_LOG_DBG("USB device stack work in high-speed mode");
		break;

	case USB_DC_UNKNOWN:
		break;

	default:
		SYS_LOG_DBG("USB unknown state");
		break;
	}
}

static inline int usb_audio_handle_set_sample_rate(struct usb_setup_packet *setup,
				s32_t *len, u8_t *buf)
{
	switch (setup->bRequest) {
	case UAC_SET_CUR:
	case UAC_SET_MIN:
	case UAC_SET_MAX:
	case UAC_SET_RES:
		*len = 3;
		SYS_LOG_DBG("buf[0]: 0x%02x", buf[0]);
		SYS_LOG_DBG("buf[1]: 0x%02x", buf[1]);
		SYS_LOG_DBG("buf[2]: 0x%02x", buf[2]);
		g_cur_sample_rate = (buf[2] << 16) | (buf[1] << 8) | buf[0];

		USB_AudioSinkSampleRateSet(g_cur_sample_rate);			/* Update the current sampling rate */
		audio_sink_sam_change_cb(USOUND_SAMPLERATE_CHANGE, g_cur_sample_rate);		/* Trigger system audio switching sampling rate */

		SYS_LOG_DBG("g_cur_sample_rate:%d ", g_cur_sample_rate);
		return 0;
	default:
		break;
	}

	return -ENOTSUP;
}

static inline int usb_audio_handle_get_sample_rate(struct usb_setup_packet *setup,
				s32_t *len, u8_t *buf)
{
	switch (setup->bRequest) {
	case UAC_GET_CUR:
	case UAC_GET_MIN:
	case UAC_GET_MAX:
	case UAC_GET_RES:
		buf[0] = (u8_t)USB_AudioSinkSampleRateGet();
		buf[1] = (u8_t)(USB_AudioSinkSampleRateGet() >> 8);
		buf[2] = (u8_t)(USB_AudioSinkSampleRateGet() >> 16);
		*len = 3;
		return 0;
	default:
		break;
	}
	return -ENOTSUP;
}

int usb_audio_source_inep_flush(void)
{
	return usb_dc_ep_flush(CONFIG_USB_AUDIO_DEVICE_SOURCE_IN_EP_ADDR);
}

int usb_audio_sink_outep_flush(void)
{
	return usb_dc_ep_flush(CONFIG_USB_AUDIO_DEVICE_SINK_OUT_EP_ADDR);
}

static void usb_audio_device_sink_handle_mute_ctrl(struct usb_setup_packet *psetup, u8_t *data)
{
	SYS_LOG_DBG("audio sink mute_ch0:%d %d\n", usb_audio_mute_vol, data[0]);
	if (usb_audio_mute_vol != data[0]) {
		usb_audio_mute_vol = data[0];
		if (audio_sink_vol_ctrl_cb) {
			if (usb_audio_mute_vol == 1) {
				audio_sink_vol_ctrl_cb(USOUND_SYNC_HOST_MUTE, LOW_BYTE(psetup->wValue), &usb_audio_mute_vol);
			} else {
				audio_sink_vol_ctrl_cb(USOUND_SYNC_HOST_UNMUTE, LOW_BYTE(psetup->wValue), &usb_audio_mute_vol);
			}
		}
	}
}

static void usb_host_sync_main_ch_vol(struct usb_setup_packet *psetup, u8_t *data)
{
	sink_ch0_cur_vol = (data[1] << 8) | (data[0]);
	SYS_LOG_DBG("audio sink set main ch:0x%04x", sink_ch0_cur_vol);
	if (sink_ch0_cur_vol == 0) {
		sink_ch0_cur_vol = 65536;
	}

	if (audio_sink_vol_ctrl_cb) {
		audio_sink_vol_ctrl_cb(USOUND_SYNC_HOST_VOL_TYPE, LOW_BYTE(psetup->wValue), &sink_ch0_cur_vol);
	}
}

static void usb_host_sync_ch1_vol(struct usb_setup_packet *psetup, u8_t *data)
{
	sink_ch1_cur_vol = (data[1] << 8) | (data[0]);
	SYS_LOG_DBG("audio sink set ch1:0x%04x", sink_ch1_cur_vol);
	if (sink_ch1_cur_vol == 0) {
		sink_ch1_cur_vol = 65536;
	}

	if (audio_sink_vol_ctrl_cb) {
		audio_sink_vol_ctrl_cb(USOUND_SYNC_HOST_VOL_TYPE, LOW_BYTE(psetup->wValue), &sink_ch1_cur_vol);
	}
}

static void usb_host_sync_ch2_vol(struct usb_setup_packet *psetup, u8_t *data)
{
	sink_ch2_cur_vol = (data[1] << 8) | (data[0]);
	SYS_LOG_DBG("audio sink set ch2:0x%04x", sink_ch2_cur_vol);
	if (sink_ch2_cur_vol == 0) {
		sink_ch2_cur_vol = 65536;
	}

	if (audio_sink_vol_ctrl_cb) {
		audio_sink_vol_ctrl_cb(USOUND_SYNC_HOST_VOL_TYPE, LOW_BYTE(psetup->wValue), &sink_ch2_cur_vol);
	}
}

static void usb_audio_device_sink_handle_vol_ctrl(struct usb_setup_packet *psetup, u8_t *data)
{
	if (LOW_BYTE(psetup->wValue) == MAIN_CHANNEL_NUMBER0) {
		usb_host_sync_main_ch_vol(psetup, data);
	} else if (LOW_BYTE(psetup->wValue) == MAIN_CHANNEL_NUMBER1) {
		usb_host_sync_ch1_vol(psetup, data);
	} else if (LOW_BYTE(psetup->wValue) == MAIN_CHANNEL_NUMBER2) {
		usb_host_sync_ch2_vol(psetup, data);
	}

}

#ifdef CONFIG_SUPPORT_USB_AUDIO_SOURCE
static void usb_audio_device_source_handle_mute_ctrl(struct usb_setup_packet *psetup, u8_t *data)
{
	SYS_LOG_DBG("audio source mute_ch0:%d %d\n", usb_audio_mute_vol, data[0]);
	if (usb_audio_mute_vol != data[0]) {
		usb_audio_mute_vol = data[0];
		if (audio_source_vol_ctrl_cb) {
			if (usb_audio_mute_vol == 1) {
				audio_source_vol_ctrl_cb(UMIC_SYNC_HOST_MUTE, LOW_BYTE(psetup->wValue), &usb_audio_mute_vol);
			} else {
				audio_source_vol_ctrl_cb(UMIC_SYNC_HOST_UNMUTE, LOW_BYTE(psetup->wValue), &usb_audio_mute_vol);
			}
		}
	}
}

static void usb_audio_device_source_handle_vol_ctrl(struct usb_setup_packet *psetup, u8_t *data)
{
	source_ch0_cur_vol = (data[1] << 8) | (data[0]);
	SYS_LOG_DBG("source set main_ch vol:0x%04x", source_ch0_cur_vol);
	if (source_ch0_cur_vol == 0) {
		source_ch0_cur_vol = 65536;
	}

	if (audio_source_vol_ctrl_cb) {
		audio_source_vol_ctrl_cb(UMIC_SYNC_HOST_VOL_TYPE, LOW_BYTE(psetup->wValue), &source_ch0_cur_vol);
	}
}
#endif

static void usb_audio_device_sink_handle_get_cur(struct usb_setup_packet *psetup, s32_t *len, u8_t **data)
{
	if (psetup->wValue == ((MUTE_CONTROL << 8) | MAIN_CHANNEL_NUMBER0)) {
		SYS_LOG_DBG("sink get main_ch mute vol");
		*data = (u8_t *)&usb_audio_mute_vol;
		*len  = MUTE_LENGTH;
	} else if (psetup->wValue == ((MUTE_CONTROL << 8) | MAIN_CHANNEL_NUMBER1)) {
		SYS_LOG_DBG("sink get ch_1 mute vol");
		*data = (u8_t *)&usb_audio_mute_vol;
		*len  = MUTE_LENGTH;
	} else if (psetup->wValue == ((MUTE_CONTROL << 8) | MAIN_CHANNEL_NUMBER2)) {
		SYS_LOG_DBG("sink get ch_2 mute vol");
		*data = (u8_t *)&usb_audio_mute_vol;
		*len  = MUTE_LENGTH;
	} else if (psetup->wValue == ((VOLUME_CONTROL << 8) | MAIN_CHANNEL_NUMBER0)) {
		SYS_LOG_DBG("sink get main_ch vol");
		*data = (u8_t *) &sink_ch0_cur_vol;
		*len = VOLUME_LENGTH;
	} else if (psetup->wValue == ((VOLUME_CONTROL << 8) | MAIN_CHANNEL_NUMBER1)) {
		SYS_LOG_DBG("sink get ch_1 vol");
		*data = (u8_t *) &sink_ch1_cur_vol;
		*len = VOLUME_LENGTH;
	} else if (psetup->wValue == ((VOLUME_CONTROL << 8) | MAIN_CHANNEL_NUMBER2)) {
		SYS_LOG_DBG("sink get ch_2 vol");
		*data = (u8_t *) &sink_ch2_cur_vol;
		*len = VOLUME_LENGTH;
	}
}

static void usb_audio_device_sink_handle_get_min(struct usb_setup_packet *psetup, s32_t *len, u8_t **data)
{
	if (psetup->wValue == ((VOLUME_CONTROL << 8) | MAIN_CHANNEL_NUMBER0)) {
		SYS_LOG_DBG("sink get main_ch min");
		*data = (u8_t *)&sink_min_volume;
		*len  = VOLUME_LENGTH;
	} else if (psetup->wValue == ((VOLUME_CONTROL << 8) | MAIN_CHANNEL_NUMBER1)) {
		SYS_LOG_DBG("sink get ch_1 min");
		*data = (u8_t *)&sink_min_volume;
		*len  = VOLUME_LENGTH;
	} else if (psetup->wValue == ((VOLUME_CONTROL << 8) | MAIN_CHANNEL_NUMBER2)) {
		SYS_LOG_DBG("sink get ch_2 min");
		*data = (u8_t *)&sink_min_volume;
		*len  = VOLUME_LENGTH;
	}
}

static void usb_audio_device_sink_handle_get_max(struct usb_setup_packet *psetup, s32_t *len, u8_t **data)
{
	if (psetup->wValue == ((VOLUME_CONTROL << 8) | MAIN_CHANNEL_NUMBER0)) {
		SYS_LOG_DBG("sink get main_ch max");
		*data = (u8_t *)&sink_max_volume;
		*len  = VOLUME_LENGTH;
	} else if (psetup->wValue == ((VOLUME_CONTROL << 8) | MAIN_CHANNEL_NUMBER1)) {
		SYS_LOG_DBG("sink get ch_1 max");
		*data = (u8_t *)&sink_max_volume;
		*len  = VOLUME_LENGTH;
	} else if (psetup->wValue == ((VOLUME_CONTROL << 8) | MAIN_CHANNEL_NUMBER2)) {
		SYS_LOG_DBG("sink get ch_2 max");
		*data = (u8_t *)&sink_max_volume;
		*len  = VOLUME_LENGTH;
	}
}

static void usb_audio_device_sink_handle_get_step(struct usb_setup_packet *psetup, s32_t *len, u8_t **data)
{
	if (psetup->wValue == ((VOLUME_CONTROL << 8) | MAIN_CHANNEL_NUMBER0)) {
		SYS_LOG_DBG("sink get main_ch step");
		*data = (u8_t *)&sink_volume_step;
		*len  = VOLUME_LENGTH;
	} else if (psetup->wValue == ((VOLUME_CONTROL << 8) | MAIN_CHANNEL_NUMBER1)) {
		SYS_LOG_DBG("sink get ch_1 step");
		*data = (u8_t *)&sink_volume_step;
		*len  = VOLUME_LENGTH;
	} else if (psetup->wValue == ((VOLUME_CONTROL << 8) | MAIN_CHANNEL_NUMBER2)) {
		SYS_LOG_DBG("sink get volume step");
		*data = (u8_t *)&sink_volume_step;
		*len  = VOLUME_LENGTH;
	}
}

#ifdef CONFIG_SUPPORT_USB_AUDIO_SOURCE
static void usb_audio_device_source_handle_get_cur(struct usb_setup_packet *psetup, s32_t *len, u8_t **data)
{
	if (psetup->wValue == ((MUTE_CONTROL << 8) | MAIN_CHANNEL_NUMBER0)) {
		SYS_LOG_DBG("source get main_ch mute vol");
		*data = (u8_t *)&usb_audio_mute_vol;
		*len  = MUTE_LENGTH;
	} else if (psetup->wValue == ((VOLUME_CONTROL << 8) | MAIN_CHANNEL_NUMBER0)) {
		SYS_LOG_DBG("source get main_ch vol");
		*data = (u8_t *) &source_ch0_cur_vol;
		*len  = VOLUME_LENGTH;
	}
}

static void usb_audio_device_source_handle_get_min(struct usb_setup_packet *psetup, s32_t *len, u8_t **data)
{
	if (psetup->wValue == ((VOLUME_CONTROL << 8) | MAIN_CHANNEL_NUMBER0)) {
		SYS_LOG_DBG("source get main_ch min");
		*data = (u8_t *)&source_min_volume;
		*len  = VOLUME_LENGTH;
	}
}

static void usb_audio_device_source_handle_get_max(struct usb_setup_packet *psetup, s32_t *len, u8_t **data)
{
	if (psetup->wValue == ((VOLUME_CONTROL << 8) | MAIN_CHANNEL_NUMBER0)) {
		SYS_LOG_DBG("source get main_ch max");
		*data = (u8_t *)&source_max_volume;
		*len  = VOLUME_LENGTH;
	}
}

static void usb_audio_device_source_handle_get_step(struct usb_setup_packet *psetup, s32_t *len, u8_t **data)
{
	if (psetup->wValue == ((VOLUME_CONTROL << 8) | MAIN_CHANNEL_NUMBER0)) {
		SYS_LOG_DBG("source get volume step");
		*data = (u8_t *)&source_volume_step;
		*len  = VOLUME_LENGTH;
	}
}
#endif

static int usb_audio_class_handle_req(struct usb_setup_packet *psetup,
	s32_t *len, u8_t **data)
{

	int ret = -ENOTSUP;
	u8_t *temp_data = *data;

	switch (psetup->bmRequestType) {
	case SPECIFIC_REQUEST_OUT:
	if (psetup->wIndex == FEATURE_UNIT_INDEX1) {
		if (psetup->bRequest == UAC_SET_CUR) {
			if (psetup->wValue == ((MUTE_CONTROL << 8) | MAIN_CHANNEL_NUMBER0)) {
				usb_audio_device_sink_handle_mute_ctrl(psetup, temp_data);
			} else if (HIGH_BYTE(psetup->wValue) == VOLUME_CONTROL) {
				usb_audio_device_sink_handle_vol_ctrl(psetup, temp_data);
			}
		}
	}
#ifdef CONFIG_SUPPORT_USB_AUDIO_SOURCE
	else if (psetup->wIndex == FEATURE_UNIT_INDEX2) {
		if (psetup->bRequest == UAC_SET_CUR) {
			if (psetup->wValue == ((MUTE_CONTROL << 8) | MAIN_CHANNEL_NUMBER0)) {
				usb_audio_device_source_handle_mute_ctrl(psetup, temp_data);
			} else if (psetup->wValue == ((VOLUME_CONTROL << 8) | MAIN_CHANNEL_NUMBER0))
				usb_audio_device_source_handle_vol_ctrl(psetup, temp_data);
			}
	}
#endif
	ret = 0;
	break;

	case SPECIFIC_REQUEST_IN:
	if (psetup->wIndex == FEATURE_UNIT_INDEX1) {
		if (psetup->bRequest == UAC_GET_CUR) {
			usb_audio_device_sink_handle_get_cur(psetup, len, data);
		} else if (psetup->bRequest == UAC_GET_MIN) {
			usb_audio_device_sink_handle_get_min(psetup, len, data);
		} else if (psetup->bRequest == UAC_GET_MAX) {
			usb_audio_device_sink_handle_get_max(psetup, len, data);
		} else if (psetup->bRequest == UAC_GET_RES) {
			usb_audio_device_sink_handle_get_step(psetup, len, data);
		}
	}
#ifdef CONFIG_SUPPORT_USB_AUDIO_SOURCE
	else if (psetup->wIndex == FEATURE_UNIT_INDEX2) {
		if (psetup->bRequest == UAC_GET_CUR) {
			usb_audio_device_source_handle_get_cur(psetup, len, data);
		} else if (psetup->bRequest == UAC_GET_MIN) {
			usb_audio_device_source_handle_get_min(psetup, len, data);
		} else if (psetup->bRequest == UAC_GET_MAX) {
			usb_audio_device_source_handle_get_max(psetup, len, data);
		} else if(psetup->bRequest == UAC_GET_RES) {
			usb_audio_device_source_handle_get_step(psetup, len, data);
		}
	}
#endif
	ret = 0;
	break;

	case SET_ENDPOINT_CONTROL:
		if (psetup->wValue == SAMPLING_FREQ_CONTROL) {
			ret = usb_audio_handle_set_sample_rate(psetup, len, *data);
		}
	break;

	case GET_SAMPLERATE:
		if (psetup->wValue == SAMPLING_FREQ_CONTROL) {
			ret = usb_audio_handle_get_sample_rate(psetup, len, *data);
		}
	break;

	default:
	break;
	}

	return ret;
}

/*
 * NOTICE: In composite device case, this function will never be called,
 * the reason is same as class request as above.
 */
static int usb_audio_custom_handle_req(struct usb_setup_packet *setup,
				s32_t *len, u8_t **data)
{
	SYS_LOG_DBG("custom request: 0x%x 0x%x %d",
		    setup->bRequest, setup->bmRequestType, *len);
	return -ENOTSUP;
}

static int usb_audio_vendor_handle_req(struct usb_setup_packet *setup,
				s32_t *len, u8_t **data)
{
	SYS_LOG_DBG("vendor request: 0x%x 0x%x %d",
		    setup->bRequest, setup->bmRequestType, *len);
	return -ENOTSUP;
}

static void usb_audio_isoc_in_cb(u8_t ep, enum usb_dc_ep_cb_status_code cb_status)
{

	SYS_LOG_DBG("**isoc_in_cb!**");
	if (iso_in_ep_cb) {
		iso_in_ep_cb(ep, cb_status);
	}
}

static void usb_audio_isoc_out_cb(u8_t ep, enum usb_dc_ep_cb_status_code cb_status)
{
	SYS_LOG_DBG("**isoc_out_cb!**");
	if (iso_out_ep_cb) {
		iso_out_ep_cb(ep, cb_status);
	}
}

/* USB endpoint configuration */
static const struct usb_ep_cfg_data usb_audio_ep_cfg[] = {
	{
		.ep_cb = usb_audio_isoc_out_cb,
		.ep_addr = CONFIG_USB_AUDIO_DEVICE_SINK_OUT_EP_ADDR,
	},

	{
		.ep_cb = usb_audio_isoc_in_cb,
		.ep_addr = CONFIG_USB_AUDIO_DEVICE_SOURCE_IN_EP_ADDR,
	}
};

static const struct usb_cfg_data usb_audio_config = {
	.usb_device_description = NULL,
	.interface_descriptor = &usb_audio_interface,
	.cb_usb_status = usb_audio_status_cb,
	.interface = {
		.class_handler = usb_audio_class_handle_req,
		.custom_handler = usb_audio_custom_handle_req,
		.vendor_handler = usb_audio_vendor_handle_req,
	},
	.num_endpoints = ARRAY_SIZE(usb_audio_ep_cfg),
	.endpoint = usb_audio_ep_cfg,
};

static int usb_audio_device_fix_dev_sn(void)
{
	int ret;
#ifdef CONFIG_NVRAM_CONFIG
	int read_len;

	u8_t mac_str[CONFIG_USB_DEVICE_STRING_DESC_MAX_LEN];

	read_len = nvram_config_get(CONFIG_USB_AUDIO_SOURCESINK_SN_NVRAM, mac_str, CONFIG_USB_DEVICE_STRING_DESC_MAX_LEN);
	if (read_len < 0) {
		SYS_LOG_DBG("no sn data in nvram: %d", read_len);
		ret = usb_device_register_string_descriptor(DEV_SN_DESC, CONFIG_USB_AUDIO_SOURCESINK_SN, strlen(CONFIG_USB_AUDIO_SOURCESINK_SN));
		if (ret)
			return ret;
	} else {
		ret = usb_device_register_string_descriptor(DEV_SN_DESC, mac_str, read_len);
		if (ret)
			return ret;
	}
#else
	ret = usb_device_register_string_descriptor(DEV_SN_DESC, CONFIG_USB_AUDIO_SOURCESINK_SN, strlen(CONFIG_USB_AUDIO_SOURCESINK_SN));
		if (ret)
			return ret;
#endif
	return 0;
}

/*
 * API: initialize USB audio dev
 */
int usb_audio_device_init(void)
{
	int ret;

	/* Register string descriptor */
	ret = usb_device_register_string_descriptor(MANUFACTURE_STR_DESC, CONFIG_USB_AUDIO_SOURCESINK_MANUFACTURER, strlen(CONFIG_USB_AUDIO_SOURCESINK_MANUFACTURER));
	if (ret) {
		return ret;
	}

	ret = usb_device_register_string_descriptor(PRODUCT_STR_DESC, CONFIG_USB_AUDIO_SOURCESINK_PRODUCT, strlen(CONFIG_USB_AUDIO_SOURCESINK_PRODUCT));
	if (ret) {
		return ret;
	}

	ret = usb_audio_device_fix_dev_sn();
	if (ret) {
		return ret;
	}

	/* Register device descriptors */
	usb_device_register_descriptors(usb_audio_sourcesink_fs_desc, usb_audio_sourcesink_fs_desc);

	/* Initialize the USB driver with the right configuration */
	ret = usb_set_config(&usb_audio_config);
	if (ret < 0) {
		SYS_LOG_ERR("Failed to config USB");
		return ret;
	}

	/* Enable USB driver */
	ret = usb_enable(&usb_audio_config);
	if (ret < 0) {
		SYS_LOG_ERR("Failed to enable USB");
		return ret;
	}

	return 0;
}

/*
 * API: deinitialize USB audio dev
 */
int usb_audio_device_exit(void)
{
	int ret;

	ret = usb_disable();
	if (ret) {
		SYS_LOG_ERR("Failed to disable USB: %d", ret);
		return ret;
	}
	usb_deconfig();
	return 0;
}

/*
 * API: Initialize USB audio composite devie
 */
int usb_audio_composite_dev_init(void)
{
	return usb_decice_composite_set_config(&usb_audio_config);
}

void usb_audio_device_sink_set_cur_vol(uint8_t channel_type, int vol_dat)
{
	if (channel_type == MAIN_CH) {
		sink_ch0_cur_vol = vol_dat;
	} else if (channel_type == CHANNEL_1) {
		sink_ch1_cur_vol = vol_dat;
	} else if (channel_type == CHANNEL_2) {
		sink_ch2_cur_vol = vol_dat;
	}
}

void usb_audio_device_source_set_cur_vol(uint8_t channel_type, int vol_dat)
{
	if (channel_type == MAIN_CH) {
		source_ch0_cur_vol = vol_dat;
	}
}

void usb_audio_device_sink_set_vol_info(uint8_t vol_type, int vol_dat)
{
	if (vol_type == MAXIMUM_VOLUME) {
		sink_max_volume = vol_dat;
	} else if (vol_type == MINIMUM_VOLUME) {
		sink_min_volume = vol_dat;
	} else if (vol_type == VOLUME_STEP) {
		sink_volume_step = vol_dat;
	}
}

void usb_audio_device_source_set_vol_info(uint8_t vol_type, int vol_dat)
{
	if (vol_type == MAXIMUM_VOLUME) {
		source_max_volume = vol_dat;
	} else if (vol_type == MINIMUM_VOLUME) {
		source_min_volume = vol_dat;
	} else if (vol_type == VOLUME_STEP) {
		source_volume_step = vol_dat;
	}
}

void usb_audio_device_register_pm_cb(usb_audio_pm cb)
{
	audio_device_pm_cb = cb;
}

void usb_audio_device_sink_register_start_cb(usb_audio_start cb)
{
	audio_sink_start_cb = cb;
}

void usb_audio_device_source_register_start_cb(usb_audio_start cb)
{
	audio_source_start_cb = cb;
}

void usb_audio_device_register_inter_in_ep_cb(usb_ep_callback cb)
{
	iso_in_ep_cb = cb;
}

void usb_audio_device_register_inter_out_ep_cb(usb_ep_callback cb)
{
	iso_out_ep_cb = cb;
}

void usb_audio_device_source_register_volume_sync_cb(usb_audio_volume_sync cb)
{
	audio_source_vol_ctrl_cb = cb;
}

void usb_audio_device_sink_register_volume_sync_cb(usb_audio_volume_sync cb)
{
	audio_sink_vol_ctrl_cb = cb;
}

void usb_audio_device_sink_sample_rate_change_cb(usb_sample_rate_change cb)
{
	audio_sink_sam_change_cb = cb;
}

int usb_audio_device_ep_write(const u8_t *data, u32_t data_len, u32_t *bytes_ret)
{
	return usb_write(CONFIG_USB_AUDIO_DEVICE_SOURCE_IN_EP_ADDR,
				data, data_len, bytes_ret);
}

int usb_audio_device_ep_read(u8_t *data, u32_t data_len, u32_t *bytes_ret)
{
	return usb_read(CONFIG_USB_AUDIO_DEVICE_SINK_OUT_EP_ADDR,
				data, data_len, bytes_ret);
}
