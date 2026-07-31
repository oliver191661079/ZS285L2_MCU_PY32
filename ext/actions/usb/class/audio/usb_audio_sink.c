/**
 * @file
 * @brief USB audio device class driver.
 *
 * driver for USB audio sink device.
 */

#include <kernel.h>
#include <init.h>
#include <usb/class/usb_audio.h>
#include "usb_audio_sink_desc.h"

#ifdef CONFIG_NVRAM_CONFIG
#include <string.h>
#include <nvram_config.h>
#endif

#define SYS_LOG_LEVEL CONFIG_SYS_LOG_USB_SINK_LEVEL
#define SYS_LOG_DOMAIN "usb_audio_sink"
#include <logging/sys_log.h>

static int audio_sink_mute_vol = 0x8000;
static int audio_sink_ch0_cur_vol;
static int audio_sink_ch1_cur_vol;
static int audio_sink_ch2_cur_vol;
static int audio_sink_max_volume;
static int audio_sink_min_volume;
static int audio_sink_volume_step;

static usb_audio_start audio_sink_start_cb;
static usb_ep_callback iso_out_ep_cb;
static usb_audio_volume_sync sink_vol_sync_cb;

static bool audio_download_streaming_enabled;

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
		case AUDIO_STREAM_INTER2:
			if (!alt_setting) {
				audio_download_streaming_enabled = false;
			} else {
				audio_download_streaming_enabled = true;
			}

			if (audio_sink_start_cb) {
				audio_sink_start_cb(audio_download_streaming_enabled);
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
		SYS_LOG_DBG("USB device configured");
		break;

	case USB_DC_DISCONNECTED:
		audio_download_streaming_enabled = false;
		if (audio_sink_start_cb) {
			audio_sink_start_cb(audio_download_streaming_enabled);
		}
		SYS_LOG_DBG("USB device disconnected");
		break;

	case USB_DC_SUSPEND:
		audio_download_streaming_enabled = false;
		if (audio_sink_start_cb) {
			audio_sink_start_cb(audio_download_streaming_enabled);
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

static void usb_audio_sink_handle_req_in(struct usb_setup_packet *setup, s32_t *len, u8_t **data)
{
	if (setup->wValue == ((MUTE_CONTROL << 8) | MAIN_CHANNEL_NUMBER0)) {
		SYS_LOG_DBG("sink get main_ch mute vol");
		*data = (u8_t *)&audio_sink_mute_vol;
		*len  = MUTE_LENGTH;
	} else if (setup->wValue == ((VOLUME_CONTROL << 8) | MAIN_CHANNEL_NUMBER0)) {
		SYS_LOG_DBG("sink get main_ch vol");
		*data = (u8_t *) &audio_sink_ch0_cur_vol;
		*len = VOLUME_LENGTH;
	} else if (setup->wValue == ((VOLUME_CONTROL << 8) | MAIN_CHANNEL_NUMBER1)) {
		SYS_LOG_DBG("sink get ch_1 vol");
		*data = (u8_t *) &audio_sink_ch1_cur_vol;
		*len = VOLUME_LENGTH;
	} else if (setup->wValue == ((VOLUME_CONTROL << 8) | MAIN_CHANNEL_NUMBER2)) {
		SYS_LOG_DBG("sink get ch_2 vol");
		*data = (u8_t *) &audio_sink_ch2_cur_vol;
		*len = VOLUME_LENGTH;
	}
}

static void usb_audio_sink_handle_mute_ctrl(struct usb_setup_packet *setup, u8_t *data)
{
	SYS_LOG_DBG("data[0]: %d", data[0]);
	if (audio_sink_mute_vol != data[0]) {
		audio_sink_mute_vol = data[0];
		if (sink_vol_sync_cb) {
			if (audio_sink_mute_vol == 1) {
				sink_vol_sync_cb(USOUND_SYNC_HOST_MUTE, LOW_BYTE(setup->wValue), &audio_sink_mute_vol);
			} else {
				sink_vol_sync_cb(USOUND_SYNC_HOST_UNMUTE, LOW_BYTE(setup->wValue), &audio_sink_mute_vol);
			}
		}
	}
}

static void usb_audio_sink_handle_vol_ctrl(struct usb_setup_packet *setup, u8_t *data)
{
	audio_sink_ch0_cur_vol  = (data[1] << 8) | (data[0]);
	SYS_LOG_DBG("host set channel_%d volume: 0x%04x", LOW_BYTE(setup->wValue), audio_sink_ch0_cur_vol);
	if (audio_sink_ch0_cur_vol == 0) {
		audio_sink_ch0_cur_vol = 65536;
	}

	if (sink_vol_sync_cb) {
		sink_vol_sync_cb(USOUND_SYNC_HOST_VOL_TYPE, LOW_BYTE(setup->wValue), &audio_sink_ch0_cur_vol);
	}
}

static int usb_audio_class_handle_req(struct usb_setup_packet *setup, s32_t *len, u8_t **data)
{
	u8_t *temp_data = *data;

	switch (setup->bmRequestType) {
	case SPECIFIC_REQUEST_IN:
		if (setup->wIndex == FEATURE_UNIT_INDEX1) {
			if (setup->bRequest == UAC_GET_CUR) {
				usb_audio_sink_handle_req_in(setup, len, data);
			} else if (setup->bRequest == UAC_GET_MIN) {
				*data = (u8_t *) &audio_sink_min_volume;
				*len = VOLUME_LENGTH;
			} else if (setup->bRequest == UAC_GET_MAX) {
				*data = (u8_t *) &audio_sink_max_volume;
				*len = VOLUME_LENGTH;
			} else if (setup->bRequest == UAC_GET_RES) {
				*data = (u8_t *) &audio_sink_volume_step;
				*len = VOLUME_LENGTH;
			}
		}
	break;

	case SPECIFIC_REQUEST_OUT:
		if (setup->wIndex == FEATURE_UNIT_INDEX1) {
			if (setup->bRequest == UAC_SET_CUR) {
				if (HIGH_BYTE(setup->wValue) == MUTE_CONTROL) {
					usb_audio_sink_handle_mute_ctrl(setup, temp_data);
				} else if (HIGH_BYTE(setup->wValue) == VOLUME_CONTROL) {
					usb_audio_sink_handle_vol_ctrl(setup, temp_data);
				}
			}
		}
	break;
	}
	return 0;
}

static int usb_audio_custom_handle_req(struct usb_setup_packet *setup, s32_t *len, u8_t **data)
{
	SYS_LOG_DBG("custom request: 0x%x 0x%x %d",
		    setup->bRequest, setup->bmRequestType, *len);
	return -ENOTSUP;
}

static int usb_audio_vendor_handle_req(struct usb_setup_packet *setup, s32_t *len, u8_t **data)
{
	SYS_LOG_DBG("vendor request: 0x%x 0x%x %d",
		    setup->bRequest, setup->bmRequestType, *len);
	return -ENOTSUP;
}

int usb_audio_sink_ep_flush(void)
{
	return usb_dc_ep_flush(CONFIG_USB_AUDIO_SINK_OUT_EP_ADDR);
}

static void usb_audio_sink_isoc_out(u8_t ep, enum usb_dc_ep_cb_status_code cb_status)
{
	SYS_LOG_DBG("audio_isoc_out");
	if (iso_out_ep_cb) {
		iso_out_ep_cb(ep, cb_status);
	}
}

static struct usb_ep_cfg_data usb_audio_sink_ep_cfg[] = {
	{
		.ep_cb = usb_audio_sink_isoc_out,
		.ep_addr = CONFIG_USB_AUDIO_SINK_OUT_EP_ADDR,
	},
};

static struct usb_cfg_data usb_audio_sink_config = {
	.usb_device_description = NULL,
	.cb_usb_status = usb_audio_status_cb,
	.interface = {
		.class_handler  = usb_audio_class_handle_req,
		.custom_handler = usb_audio_custom_handle_req,
		.vendor_handler = usb_audio_vendor_handle_req,

	},
	.num_endpoints = ARRAY_SIZE(usb_audio_sink_ep_cfg),
	.endpoint = usb_audio_sink_ep_cfg,
};


/*
 * API: initialize USB audio sound
 */
static int usb_audio_sink_fix_dev_sn(void)
{
	int ret;
#ifdef CONFIG_NVRAM_CONFIG
	int read_len;

	u8_t mac_str[CONFIG_USB_DEVICE_STRING_DESC_MAX_LEN];

	read_len = nvram_config_get(CONFIG_USB_AUDIO_SINK_SN_NVRAM, mac_str, CONFIG_USB_DEVICE_STRING_DESC_MAX_LEN);
	if (read_len < 0) {
		SYS_LOG_DBG("no sn data in nvram: %d", read_len);
		ret = usb_device_register_string_descriptor(DEV_SN_DESC, CONFIG_USB_AUDIO_SINK_SN, strlen(CONFIG_USB_AUDIO_SINK_SN));
		if (ret)
			return ret;
	} else {
		ret = usb_device_register_string_descriptor(DEV_SN_DESC, mac_str, read_len);
		if (ret)
			return ret;
	}
#else
	ret = usb_device_register_string_descriptor(DEV_SN_DESC, CONFIG_USB_AUDIO_SINK_SN, strlen(CONFIG_USB_AUDIO_SINK_SN));
	if (ret) {
			return ret;
	}
#endif
	return 0;
}


/*
 * API: initialize USB audio sink
 */
int usb_audio_sink_init(struct device *dev)
{
	int ret;

	/* register string descriptor */
	ret = usb_device_register_string_descriptor(MANUFACTURE_STR_DESC, CONFIG_USB_AUDIO_SINK_MANUFACTURER, strlen(CONFIG_USB_AUDIO_SINK_MANUFACTURER));
	if (ret) {
			return ret;
	}

	ret = usb_device_register_string_descriptor(PRODUCT_STR_DESC, CONFIG_USB_AUDIO_SINK_PRODUCT, strlen(CONFIG_USB_AUDIO_SINK_PRODUCT));
	if (ret) {
			return ret;
	}

	ret = usb_audio_sink_fix_dev_sn();
	if (ret) {
			return ret;
	}

	/* register device descriptor */
	usb_device_register_descriptors(usb_audio_sink_fs_descriptor, usb_audio_sink_hs_descriptor);

	/* initialize the usb driver with the right configuration */
	ret = usb_set_config(&usb_audio_sink_config);
	if (ret < 0) {
			SYS_LOG_ERR("Failed to config USB");
			return ret;
	}

	/* enable usb driver */
	ret = usb_enable(&usb_audio_sink_config);
	if (ret < 0) {
			SYS_LOG_ERR("Failed to enable USB");
			return ret;
	}

	return 0;
}

/*
 * API: deinitialize USB audio sink
 */
int usb_audio_sink_deinit(void)
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

int usb_audio_sink_ep_read(u8_t *data, u32_t data_len, u32_t *bytes_ret)
{
	return usb_read(CONFIG_USB_AUDIO_SINK_OUT_EP_ADDR,
				data, data_len, bytes_ret);
}

void usb_audio_sink_register_start_cb(usb_audio_start cb)
{
	audio_sink_start_cb = cb;
}

void usb_audio_sink_register_inter_out_ep_cb(usb_ep_callback cb)
{
	iso_out_ep_cb = cb;
}

void usb_audio_sink_register_volume_sync_cb(usb_audio_volume_sync cb)
{
	sink_vol_sync_cb = cb;
}

void usb_audio_sink_set_cur_vol(uint8_t channel_type, int vol_dat)
{
	if (channel_type == MAIN_CH) {
		audio_sink_ch0_cur_vol = vol_dat;
	} else if (channel_type == CHANNEL_1) {
		audio_sink_ch1_cur_vol = vol_dat;
	} else if (channel_type == CHANNEL_2) {
		audio_sink_ch2_cur_vol = vol_dat;
	}
}

void usb_audio_sink_set_vol_info(uint8_t vol_type, int vol_dat)
{
	if (vol_type == MAXIMUM_VOLUME) {
		audio_sink_max_volume = vol_dat;
	} else if (vol_type == MINIMUM_VOLUME) {
		audio_sink_min_volume = vol_dat;
	} else if (vol_type == VOLUME_STEP) {
		audio_sink_volume_step = vol_dat;
	}
}

