/*
 * Copyright (c) 2024 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt call main.
 */

#include "btcall.h"
#include "tts_manager.h"
#include "desktop_manager.h"
#ifdef CONFIG_PROPERTY
#include "property_manager.h"
#endif
static struct btcall_app_t *p_btcall_app = NULL;

static int _btcall_init(void *p1, void *p2, void *p3)
{
	int ret = 0;

	SYS_LOG_INF("");
	if(NULL != p_btcall_app) {
		SYS_LOG_ERR("Already inited");
		return -1;
	}
	p_btcall_app = app_mem_malloc(sizeof(struct btcall_app_t));
	if (NULL == p_btcall_app) {
		SYS_LOG_ERR("malloc fail!\n");
		ret = -ENOMEM;
		return ret;
	}
	memset(p_btcall_app, 0, sizeof(struct btcall_app_t));

	bt_manager_set_stream_type(AUDIO_STREAM_VOICE);

	btcall_view_init();

	btcall_ring_manager_init();

	SYS_LOG_INF("done");
	return ret;
}

static int _btcall_exit(void)
{
	SYS_LOG_INF("");
	if (NULL == p_btcall_app) {
		return -1;
	}

	if (p_btcall_app->bt_chan.handle != 0) {
		bt_call_stop_play_and_capture();
	}

	btcall_ring_manager_deinit();

	btcall_view_deinit();

	app_mem_free(p_btcall_app);
	p_btcall_app = NULL;

#ifdef CONFIG_PROPERTY
	property_flush_req(NULL);
#endif

	SYS_LOG_INF("done");
	return 0;
}

static int btcall_proc_msg(struct app_msg *msg)
{
	SYS_LOG_INF("type %d, cmd %d, value %d\n", msg->type,
		    msg->cmd, msg->value);

	switch (msg->type) {
	case MSG_EXIT_APP:
		_btcall_exit();
		break;
	case MSG_BT_EVENT:
		btcall_bt_event_proc(msg);
		break;
	case MSG_INPUT_EVENT:
		btcall_input_event_proc(msg);
		break;
#ifdef CONFIG_PLAYTTS
	case MSG_TTS_EVENT:
		btcall_tts_event_proc(msg);
		break;
#endif

	case MSG_TWS_EVENT:
		//btcall_tws_event_proc(&msg);
		break;
	default:
		SYS_LOG_ERR("msg 0x%x", msg->type);
		break;;
	}
	return 0;
}

struct btcall_app_t *btcall_get_app(void)
{
	return p_btcall_app;
}



int btcall_dump_app_data(void)
{
	print_buffer_lazy(APP_ID_BTCALL, (void *)btcall_get_app(),
			  sizeof(struct btcall_app_t));
	return 0;
}

DESKTOP_PLUGIN_DEFINE(DESKTOP_PLUGIN_ID_BR_CALL, _btcall_init, _btcall_exit,
		      btcall_proc_msg, btcall_dump_app_data, NULL, NULL, NULL);
