/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "demo_app.h"

#ifdef CONFIG_USB_HOTPLUG
#include <hotplug_manager.h>
#endif

#include <ui_manager.h>
#include <audio_policy.h>
#include "app_common.h"

static struct demo_app_t *p_usound;

static void _usb_audio_event_callback_handle(u8_t type, int param)
{
	bool skip = false;
	struct app_msg msg = {0};

	SYS_LOG_INF("t:%d v:%d", type, param);

	msg.type = MSG_DEMO_APP_EVENT;
	switch (type)
	{
	case USOUND_SYNC_HOST_MUTE:
		msg.cmd = MSG_USOUND_STREAM_MUTE;
		break;
	case USOUND_SYNC_HOST_UNMUTE:
		msg.cmd = MSG_USOUND_STREAM_UNMUTE;
		break;
	case USOUND_STREAM_STOP:
		msg.cmd = MSG_USOUND_STREAM_STOP;
		break;
	case USOUND_STREAM_START:
		msg.cmd = MSG_USOUND_STREAM_START;
		msg.value = param;
		break;
	default:
		skip = true;
		break;
	}

	if (!skip) {
		send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
	}
}

#ifdef CONFIG_USB_UART_CONSOLE
void trace_set_usb_console_active(u8_t active);
#endif

static int _demo_init(void *p1, void *p2, void *p3)
{
	if (p_usound)
		return 0;

	p_usound = app_mem_malloc(sizeof(struct demo_app_t));
	if (!p_usound) {
		SYS_LOG_ERR("malloc failed!\n");
		return -ENOMEM;
	}

	memset(p_usound, 0, sizeof(struct demo_app_t));

	demo_view_init();

#ifdef CONFIG_USB_UART_CONSOLE
#ifdef CONFIG_USB_HOTPLUG
	if (hotplug_manager_get_state(HOTPLUG_USB_DEVICE) == HOTPLUG_IN) {
		trace_set_usb_console_active(0);
	}
#endif
#endif

#ifdef CONFIG_BT_ADV_MANAGER
	send_message_to_system(MSG_BT_MGR_EVENT, MSG_BLE_ADV_CONTROL, 0);
#endif
	bt_manager_set_user_visual(1,0,0,BTSRV_SCAN_MODE_DEFAULT_INQUIRY_PAGE);
	btif_br_auto_reconnect_stop(BTSRV_STOP_AUTO_RECONNECT_ALL);
	btif_br_disconnect_device(BTSRV_DISCONNECT_PHONE_MODE);
	bt_manager_halt_ble();

#ifdef CONFIG_USB_HOTPLUG
	if (hotplug_manager_get_state(HOTPLUG_USB_DEVICE) == HOTPLUG_IN) {
#endif
		usb_audio_init(_usb_audio_event_callback_handle);
		p_usound->device_init = true;
#ifdef CONFIG_USB_HOTPLUG
	}
#endif

	SYS_LOG_INF("init ok\n");
	return 0;
}

static int _demo_exit(void)
{
	if (!p_usound)
		goto exit;

	if (p_usound->playing) {
		usb_hid_control_pause_play();
	}

	demo_stop_play();

	if (p_usound->device_init){
		usb_audio_deinit();
		p_usound->device_init = false;
	}

	demo_view_deinit();

	bt_manager_set_user_visual(0,0,0,0);
	bt_manager_resume_ble();
#ifdef CONFIG_BT_ADV_MANAGER
	send_message_to_system(MSG_BT_MGR_EVENT, MSG_BLE_ADV_CONTROL, 1);
#endif

	app_mem_free(p_usound);

	p_usound = NULL;

#ifdef CONFIG_USB_UART_CONSOLE
#ifdef CONFIG_USB_HOTPLUG
	if (hotplug_manager_get_state(HOTPLUG_USB_DEVICE) == HOTPLUG_IN) {
		trace_set_usb_console_active(1);
	}
#endif
#endif

exit:
	SYS_LOG_INF("exit ok\n");
	return 0;
}

struct demo_app_t *demo_get_app(void)
{
	return p_usound;
}

static int _demo_proc_msg(struct app_msg *msg)
{
	SYS_LOG_INF("type %d, cmd %d, value 0x%x\n", msg->type, msg->cmd, msg->value);
	switch (msg->type) {
	case MSG_INPUT_EVENT:
		demo_input_event_proc(msg);
		break;

	case MSG_DEMO_APP_EVENT:
		demo_event_proc(msg);
		break;

#ifdef CONFIG_PLAYTTS
	case MSG_TTS_EVENT:
		demo_tts_event_proc(msg);
		break;
#endif

	case MSG_EXIT_APP:
		_demo_exit();
		break;

#ifdef CONFIG_USB_HOTPLUG
	case MSG_HOTPLUG_EVENT:
		if (msg->value == HOTPLUG_IN){
			if(!p_usound->device_init){
				usb_audio_init(_usb_audio_event_callback_handle);
				p_usound->device_init = true;
			}
		}else{
			if(p_usound->device_init){
				usb_audio_deinit();
				p_usound->device_init = false;
			}
		}
		break;
#endif

	default:
		break;
	}
	return 0;
}

static int _demo_dump_app_state(void)
{
	print_buffer_lazy(APP_ID_DEMO, (void *)p_usound, sizeof(struct demo_app_t));
	return 0;
}

DESKTOP_PLUGIN_DEFINE(DESKTOP_PLUGIN_ID_DEMO, _demo_init, _demo_exit, _demo_proc_msg, \
	_demo_dump_app_state, NULL, NULL, NULL);

