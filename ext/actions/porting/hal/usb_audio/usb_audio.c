/*
 * Copyright (c) 2020 Actions Semi Co., Ltd.
 *
 * usb audio support
 */
#define SYS_LOG_DOMAIN "usound hal"
#include <logging/sys_log.h>
#include <kernel.h>
#include <kernel_structs.h>
#include <os_common_api.h>
#include <stdio.h>
#include <soc.h>
#include <stream.h>
#include <acts_ringbuf.h>

#include <zephyr.h>
#include <init.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <audio_system.h>

#include <usb/usb_device.h>
#include <usb/class/usb_hid.h>
#include <usb/class/usb_audio.h>
#include <misc/byteorder.h>

#include <usb_hid_inner.h>
#include <usb_audio_inner.h>
#include <usb_audio_hal.h>
#include <energy_statistics.h>
#ifdef CONFIG_PROPERTY
#include <property_manager.h>
#endif
#include <audio_policy.h>

#include "usb_audio_device_desc.h"

/* USB max packet size */
#define UAC_TX_UNIT_SIZE	MAX_UPLOAD_PACKET
#define UAC_TX_DUMMY_SIZE	MAX_UPLOAD_PACKET

/* volume configuration of max, min, resolution,
 * source and sink use the same value
 */
#define _USB_AUDIO_MAX_VOL                 0x0000 //0db
#define _USB_AUDIO_MIN_VOL                 0xC3D8 //-60db
#define _USB_AUDIO_VOL_STEP                0x009A

struct usb_audio_info {
	os_work call_back_work;
	u16_t play_state:1;
	u16_t zero_frame_cnt:8;
	signed int pre_volume_db;
	u8_t call_back_type;
	int call_back_param;
	usb_audio_event_callback cb;
	io_stream_t usound_download_stream;
	u8_t *usb_audio_play_load;
	u8_t *download_buf;
	uint32_t last_out_packet_count;
	uint32_t out_packet_count;
	uint32_t last_diff_num;
};

static struct usb_audio_info *usb_audio;

#ifdef CONFIG_SUPPORT_USB_AUDIO_SOURCE
static int usb_audio_tx_unit(const u8_t *buf)
{
	u32_t wrote;

	return usb_write(CONFIG_USB_AUDIO_DEVICE_SOURCE_IN_EP_ADDR, buf,
					UAC_TX_UNIT_SIZE, &wrote);
}

static int usb_audio_tx_dummy(void)
{
	u32_t wrote;

	memset(usb_audio->usb_audio_play_load, 0, MAX_UPLOAD_PACKET);

	return usb_write(CONFIG_USB_AUDIO_DEVICE_SOURCE_IN_EP_ADDR,
			usb_audio->usb_audio_play_load,
			MAX_UPLOAD_PACKET, &wrote);
}

#endif

/**
 * @brief: Gets the bit depth of the current audio receiver.
 *
 * @note: This function returns the bit depth value in the current audio receiver format configuration.
 *
 * @param: void
 *
 * @return: The bit depth of the current configuration
 */
uint8_t USB_AudioSinkBitDepthGet(void)
{
	return g_stAudioSinkFormat.ucBitDepth;
}

/**
 * @brief: Set the bit depth of the audio sink.
 *
 * @note: This function is used to update the bit depth value in the current audio receiver format configuration.
 *
 * @param: Updated configuration bit depth
 *
 * @return: void
 */
void USB_AudioSinkBitDepthSet(uint8_t ucUpdBitDepth)
{
	g_stAudioSinkFormat.ucBitDepth = ucUpdBitDepth;
}

/**
 * @brief: Gets the sampling rate of the current audio receiver.
 *
 * @note: This function returns the sample rate in the current audio receiver format configuration.
 *
 * @param: void
 *
 * @return: The sampling rate of the current configuration
 */
uint32_t USB_AudioSinkSampleRateGet(void)
{
	return g_stAudioSinkFormat.ulSampleRate;
}

/**
 * @brief: Set the audio sampling rate.
 *
 * @note: This function is used to update the sampling rate in the current audio receiver format configuration.
 *
 * @param: Updated the configuration sampling rate
 *
 * @return: void
 */
void USB_AudioSinkSampleRateSet(uint32_t uwUpdSampleRate)
{
	g_stAudioSinkFormat.ulSampleRate = uwUpdSampleRate;
}

static void _usb_audio_stream_state_notify(u8_t stream_event)
{
	usb_audio->call_back_type = stream_event;
	os_work_submit(&usb_audio->call_back_work);
}

static os_delayed_work usb_audio_dat_detect_work;
#ifdef CONFIG_USB_APP_AUDIO_ENERGY_DETECT
static u32_t usb_audio_cur_cnt, usb_audio_pre_cnt;
#endif

/* Work item process function */
static void usb_audio_handle_dat_detect(struct k_work *item)
{
#ifdef CONFIG_USB_APP_AUDIO_ENERGY_DETECT
	if (usb_audio_pre_cnt < usb_audio_cur_cnt) {
		usb_audio_pre_cnt = usb_audio_cur_cnt;
		os_delayed_work_submit(&usb_audio_dat_detect_work, OS_MSEC(CONFIG_USB_APP_AUDIO_ENERGY_DETECT_PERIOD));
	} else if (usb_audio_pre_cnt == usb_audio_cur_cnt) {
		if (usb_audio->play_state) {
			usb_audio->play_state = 0;
			_usb_audio_stream_state_notify(USOUND_STREAM_STOP);
		}
	}
#else
	usb_audio->play_state = 0;
	_usb_audio_stream_state_notify(USOUND_STREAM_STOP);
#endif
}

static int _usb_audio_check_stream(unsigned char *buf, int num, short bitdepth)
{
	assert(usb_audio);

#ifdef CONFIG_USB_APP_AUDIO_ENERGY_DETECT
	if (energy_statistics_bitdepth(buf, num, bitdepth) <= CONFIG_USB_APP_AUDIO_ENERGY_DETECT_THRESHOLD) {
		usb_audio->zero_frame_cnt++;
		if (usb_audio->play_state && usb_audio->zero_frame_cnt > CONFIG_USB_APP_AUDIO_ENERGY_DETECT_STOP_STATE_TIME) {
			usb_audio->play_state = 0;
			usb_audio->zero_frame_cnt = 0;
			_usb_audio_stream_state_notify(USOUND_STREAM_STOP);
		}
	} else {
		usb_audio->zero_frame_cnt = 0;
		if (!usb_audio->play_state) {
			usb_audio->play_state = 1;
			_usb_audio_stream_state_notify(USOUND_STREAM_START);
			os_delayed_work_submit(&usb_audio_dat_detect_work, OS_MSEC(CONFIG_USB_APP_AUDIO_ENERGY_DETECT_PERIOD));
			usb_audio_cur_cnt = 0;
			usb_audio_pre_cnt = 0;
		}
		usb_audio_cur_cnt++;
	}
#else
	if (!usb_audio->play_state) {
		usb_audio->play_state = 1;
		_usb_audio_stream_state_notify(USOUND_STREAM_START);
	}
#endif

	return 0;
}

static int _usb_stream_write(io_stream_t handle, unsigned char *buf, int num)
{
	int32_t *buffer = (int32_t *)usb_audio->download_buf;
	int16_t *pbuf = (int16_t *)buf;
	int j = 0;

	if(USB_AudioSinkBitDepthGet() == 24)
	{
		for (int i = 0; i+3 <= num; )
		{
			int32_t v = ((int32_t)buf[i++])<<8;
			v |= ((int32_t)buf[i++])<<16;
			v |= ((int32_t)buf[i++])<<24;
			buffer[j++] = v;
		}

		return stream_write(handle, (unsigned char *)buffer, num * 4 / 3);
	}
	else if(USB_AudioSinkBitDepthGet() == 16)
	{
		for (int i = 0; i < num/2; i++)
		{
			buffer[i] = (int32_t)pbuf[i] << 16;
		}

		return stream_write(handle, (unsigned char *)buffer, num * 2);
	}
	else
	{
		return stream_write(handle, buf, num);
	}
}

/*
 * Interrupt Context
 */
#ifdef CONFIG_SUPPORT_USB_AUDIO_SOURCE
extern io_stream_t usb_audio_upload_stream;

extern int usb_audio_stream_read(io_stream_t handle, unsigned char *buf, int len);

static void _usb_audio_in_ep_complete(u8_t ep,
	enum usb_dc_ep_cb_status_code cb_status)
{
	int len;

	SYS_LOG_DBG("");

	/* In transaction request on this EP, Send recording data to PC */
	if (usb_audio_upload_stream) {
		len = usb_audio_stream_read(usb_audio_upload_stream,
					usb_audio->usb_audio_play_load,
					UAC_TX_UNIT_SIZE);
		if (len == UAC_TX_UNIT_SIZE) {
			usb_audio_tx_unit(usb_audio->usb_audio_play_load);
			return;
		}
	}

	usb_audio_tx_dummy();
}
#endif

/*
 * Interrupt Context
 */
static void _usb_audio_out_ep_complete(u8_t ep,
	enum usb_dc_ep_cb_status_code cb_status)
{
	u32_t read_byte, res;

	u8_t ISOC_out_Buf[MAX_DOWNLOAD_PACKET];

	/* Out transaction on this EP, data is available for read */
	if (USB_EP_DIR_IS_OUT(ep) && cb_status == USB_DC_EP_DATA_OUT) {
		res = usb_audio_device_ep_read(ISOC_out_Buf,
				sizeof(ISOC_out_Buf), &read_byte);
		if (!res && read_byte != 0) {
			usb_audio->out_packet_count++;
			_usb_audio_check_stream(ISOC_out_Buf, read_byte, USB_AudioSinkBitDepthGet());
			if (usb_audio->usound_download_stream) {
				res = _usb_stream_write(usb_audio->usound_download_stream, ISOC_out_Buf, read_byte);
				if (res != read_byte) {
					SYS_LOG_DBG("%d", res);
				}
			}
		}
	}
}

#ifdef CONFIG_SUPPORT_USB_AUDIO_SOURCE
static void _usb_audio_source_start_stop(bool start)
{
	if (!start) {
		usb_audio_source_inep_flush();
		/*
		 * Don't cleanup in "Set Alt Setting 0" case, Linux may send
		 * this before going "Set Alt Setting 1".
		 */
		/* uac_cleanup(); */
		_usb_audio_stream_state_notify(USOUND_UPLOAD_STREAM_STOP);
	} else {
		/*
		 * First packet is all-zero in case payload be flushed (Linux
		 * may send "Set Alt Setting" several times).
		 */

		if (usb_audio_upload_stream) {
			/**drop stream data before */
			stream_flush(usb_audio_upload_stream);
		}
		usb_audio_tx_dummy();
		_usb_audio_stream_state_notify(USOUND_UPLOAD_STREAM_START);
	}
}
#endif

static void _usb_audio_sink_start_stop(bool start)
{
	if (!start && usb_audio->play_state) {
		usb_audio->play_state = 0;
		_usb_audio_stream_state_notify(USOUND_STREAM_STOP);
	}
}

void usb_audio_tx_flush(void)
{
	usb_audio_source_inep_flush();
}

int usb_audio_set_stream(io_stream_t stream)
{
	SYS_IRQ_FLAGS flags;

	sys_irq_lock(&flags);
	if(!stream && usb_audio){
		usb_audio->last_out_packet_count = 0;
		usb_audio->out_packet_count = 0;
		usb_audio->last_diff_num = 0;
	}

	if(usb_audio){
		usb_audio->usound_download_stream = stream;
	}
	sys_irq_unlock(&flags);
	SYS_LOG_DBG("stream %p\n", stream);
	return 0;
}

int usb_host_sync_volume_to_device(int host_volume)
{
	int vol_db;
	/*
	Range form +127.9961 dB (0x7FFF) to -127.9961 dB (0x8001),
	step is 1/256 dB, or 0.00390625 dB (0x0001)
	*/

	if(host_volume < 0 || host_volume > 0xFFFF){
		host_volume = 0;
	}
	vol_db = (signed short)host_volume;
	vol_db = vol_db * 10 / 256;

	return vol_db;
}

static void _usb_audio_sink_vol_changed_notify(u8_t info_type, int chan_num, int *pstore_info)
{
	int volume_db;

	if (!usb_audio) {
		return;
	}

	if (info_type != USOUND_SYNC_HOST_VOL_TYPE) {
		usb_audio->call_back_type = info_type;
		os_work_submit(&usb_audio->call_back_work);
		return;
	}

	volume_db = usb_host_sync_volume_to_device(*pstore_info);

	if (volume_db != usb_audio->pre_volume_db) {
		SYS_LOG_INF("vol 0x%x -> %d", *pstore_info, volume_db);
		usb_audio->pre_volume_db = volume_db;
		usb_audio->call_back_type = info_type;
		usb_audio->call_back_param = volume_db;
		os_work_submit(&usb_audio->call_back_work);
	}
}

static void _usb_audio_sink_sample_change_notify(u8_t info_type, int sample_rate)
{
	if (!usb_audio) {
		return;
	}

	usb_audio->call_back_type = info_type;
	usb_audio->call_back_param = sample_rate;
	os_work_submit(&usb_audio->call_back_work);
	return;
}

#ifdef CONFIG_SUPPORT_USB_AUDIO_SOURCE
static void _usb_audio_source_vol_changed_notify(u8_t info_type, int chan_num, int *pstore_info)
{
	int volume_db;

	if (!usb_audio) {
		return;
	}

	if (info_type != UMIC_SYNC_HOST_VOL_TYPE) {
		usb_audio->call_back_type = info_type;
		os_work_submit(&usb_audio->call_back_work);
		return;
	}

	volume_db = usb_host_sync_volume_to_device(*pstore_info);

	usb_audio->call_back_type = info_type;
	usb_audio->call_back_param = volume_db;
	os_work_submit(&usb_audio->call_back_work);
}
#endif

static void _usb_audio_callback(struct k_work *work)
{
	if (usb_audio && usb_audio->cb) {
		usb_audio->cb(usb_audio->call_back_type, usb_audio->call_back_param);
	}
}

static void _usb_audio_call_status_cb(u32_t status_rec)
{
	SYS_LOG_DBG("status_rec: 0x%04x", status_rec);
}

#ifdef CONFIG_NVRAM_CONFIG
#include <string.h>
#include <nvram_config.h>
#include <property_manager.h>
#endif

int usb_device_composite_fix_dev_sn(void)
{
	static u8_t mac_str[CONFIG_USB_DEVICE_STRING_DESC_MAX_LEN];
	int ret;
	int read_len;
#ifdef CONFIG_NVRAM_CONFIG
	read_len = nvram_config_get(CFG_BT_MAC, mac_str, CONFIG_USB_DEVICE_STRING_DESC_MAX_LEN);
	if (read_len < 0) {
		SYS_LOG_DBG("no sn data in nvram: %d", read_len);
		ret = usb_device_register_string_descriptor(DEV_SN_DESC, CONFIG_USB_APP_AUDIO_DEVICE_SN, strlen(CONFIG_USB_APP_AUDIO_DEVICE_SN));
		if (ret)
			return ret;
	} else {
		ret = usb_device_register_string_descriptor(DEV_SN_DESC, mac_str, read_len);
		if (ret)
			return ret;
	}
#else
	ret = usb_device_register_string_descriptor(DEV_SN_DESC, CONFIG_USB_APP_AUDIO_DEVICE_SN, strlen(CONFIG_USB_APP_AUDIO_DEVICE_SN));
		if (ret)
			return ret;
#endif
	return 0;
}

static const int usb_audio_sink_pa_table[16] = {
       -59000, -44000, -38000, -33900,
       -29000, -24000, -22000, -19400,
       -17000, -14700, -12300, -10000,
       -7400,  -5000,  -3000,  0,
};

int usb_audio_init(usb_audio_event_callback cb)
{

	int ret = 0;
	int audio_cur_level, audio_cur_dat;

#ifdef CONFIG_USOUND_BG_DETECT_PLAY_INSERT
	if(usb_audio)
		return 0;
#endif

	usb_audio = mem_malloc(sizeof(struct usb_audio_info));
	if (!usb_audio)
		return -ENOMEM;

	memset(usb_audio, 0, sizeof(struct usb_audio_info));

	usb_audio->usb_audio_play_load = mem_malloc(MAX_UPLOAD_PACKET);
	if (!usb_audio->usb_audio_play_load) {
		mem_free(usb_audio);
		return -ENOMEM;
	}

	/* The uac has a maximum depth of 24 bits */
	usb_audio->download_buf = mem_malloc(MAX_DOWNLOAD_PACKET * 2);

	if (!usb_audio->download_buf) {
		mem_free(usb_audio->usb_audio_play_load);
		mem_free(usb_audio);
		return -ENOMEM;
	}

	os_work_init(&usb_audio->call_back_work, _usb_audio_callback);
	usb_audio->cb = cb;
	usb_audio->play_state = 0;
	usb_audio->zero_frame_cnt = 0;
	usb_audio->out_packet_count = 0;
	usb_audio->pre_volume_db = -99999;//set inval value to make sure first vol do not be filt.

	audio_cur_level = audio_system_get_current_volume(AUDIO_STREAM_USOUND);
	if(audio_cur_level >=16){
		audio_cur_level= 15;
	} else if (audio_cur_level < 0) {

		SYS_LOG_DBG("incorrect audio_cur_level:%d", audio_cur_level);

		return -ESRCH;
	}
	audio_cur_dat = (usb_audio_sink_pa_table[audio_cur_level]/1000) * 256 + 65536;
	SYS_LOG_DBG("audio_cur_level:%d, audio_cur_dat:0x%x", audio_cur_level, audio_cur_dat);

	audio_cur_dat = 65536;
	SYS_LOG_INF("uac sink cur defualt %d", audio_cur_dat);

	usb_audio_device_sink_register_start_cb(_usb_audio_sink_start_stop);
	usb_audio_device_register_inter_out_ep_cb(_usb_audio_out_ep_complete);
	usb_audio_device_sink_register_volume_sync_cb(_usb_audio_sink_vol_changed_notify);
	usb_audio_device_sink_sample_rate_change_cb(_usb_audio_sink_sample_change_notify);
	usb_audio_register_call_status_cb(_usb_audio_call_status_cb);
	usb_audio_device_sink_set_cur_vol(MAIN_CH, audio_cur_dat);
	usb_audio_device_sink_set_cur_vol(CHANNEL_1, audio_cur_dat);
	usb_audio_device_sink_set_cur_vol(CHANNEL_2, audio_cur_dat);
	usb_audio_device_sink_set_vol_info(MAXIMUM_VOLUME, _USB_AUDIO_MAX_VOL);
	usb_audio_device_sink_set_vol_info(MINIMUM_VOLUME, _USB_AUDIO_MIN_VOL);
	usb_audio_device_sink_set_vol_info(VOLUME_STEP, _USB_AUDIO_VOL_STEP);

#ifdef CONFIG_SUPPORT_USB_AUDIO_SOURCE
	usb_audio_device_source_register_start_cb(_usb_audio_source_start_stop);
	usb_audio_device_register_inter_in_ep_cb(_usb_audio_in_ep_complete);
	usb_audio_device_source_register_volume_sync_cb(_usb_audio_source_vol_changed_notify);
	usb_audio_device_source_set_cur_vol(MAIN_CH, 0xEF00);
	usb_audio_device_source_set_vol_info(MAXIMUM_VOLUME, _USB_AUDIO_MAX_VOL);
	usb_audio_device_source_set_vol_info(MINIMUM_VOLUME, _USB_AUDIO_MIN_VOL);
	usb_audio_device_source_set_vol_info(VOLUME_STEP, _USB_AUDIO_VOL_STEP);
#endif

	/* Register composite device descriptor */
	usb_device_register_descriptors(usb_audio_dev_fs_desc, usb_audio_dev_hs_desc);

	/* Register composite device string descriptor */
	ret = usb_device_register_string_descriptor(MANUFACTURE_STR_DESC, CONFIG_USB_APP_AUDIO_DEVICE_MANUFACTURER, strlen(CONFIG_USB_APP_AUDIO_DEVICE_MANUFACTURER));
	if (ret) {
		goto exit;
	}
	ret = usb_device_register_string_descriptor(PRODUCT_STR_DESC, CONFIG_USB_APP_AUDIO_DEVICE_PRODUCT, strlen(CONFIG_USB_APP_AUDIO_DEVICE_PRODUCT));
	if (ret) {
		goto exit;
	}
	ret = usb_device_composite_fix_dev_sn();
	if (ret) {
		goto exit;
	}

	/* USB Hid Device Init*/
	ret = usb_hid_device_init();
	if (ret) {
		SYS_LOG_ERR("usb hid init failed");
		goto exit;
	}

	/* USB Audio Source & Sink Initialize */
	ret = usb_audio_composite_dev_init();
	if (ret) {
		SYS_LOG_ERR("usb audio init failed");
		goto exit;
	}

	usb_device_composite_init(NULL);

	/* init system work item */
	os_delayed_work_init(&usb_audio_dat_detect_work, usb_audio_handle_dat_detect);

	SYS_LOG_INF("ok");
exit:
	return ret;
}

int usb_audio_deinit(void)
{
	usb_device_composite_exit();
	os_delayed_work_cancel(&usb_audio_dat_detect_work);

	if (usb_audio) {
		if (usb_audio->usb_audio_play_load)
			mem_free(usb_audio->usb_audio_play_load);

		if (usb_audio->download_buf)
			mem_free(usb_audio->download_buf);

		mem_free(usb_audio);
		usb_audio = NULL;
	}

	return 0;
}

uint32_t usb_audio_out_packet_count(void)
{
	if (!usb_audio) {
		return 0;
	}

	return usb_audio->out_packet_count;
}

uint32_t usb_audio_out_packet_size(void)
{
	return MAX_DOWNLOAD_PACKET;
}
