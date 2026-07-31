/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt manager event notify.
 */
#define SYS_LOG_NO_NEWLINE
#define SYS_LOG_DOMAIN "bt manager"

#include <os_common_api.h>

#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stream.h>
#include <msg_manager.h>
#include <mem_manager.h>
#include <app_manager.h>
#include <volume_manager.h>
#include "bt_manager_inner.h"
#include "btservice_api.h"
#include <audio_policy.h>
#include <audio_system.h>
#include <sys_event.h>

static int bt_manager_event_callback(uint8_t event, uint8_t* extra_data, uint32_t extra_data_size)
{
	struct bt_manager_context_t *bt_manager = bt_manager_get_context();
	int ret = 0;

	if (bt_manager->event_callback){
		ret = bt_manager->event_callback(event, extra_data, extra_data_size);
	}
	return ret;
}


int bt_manager_event_notify(int event_id, void *event_data, int event_data_size)
{
	return bt_manager_event_callback(event_id, event_data, event_data_size);
}

#if 0
static void _bt_manager_event_notify_cb(struct app_msg *msg, int result, void *not_used)
{
	if (msg && msg->ptr)
		mem_free(msg->ptr);
}

int bt_manager_event_notify(int event_id, void *event_data, int event_data_size)
{
	int ret = 0;
	struct app_msg  msg = {0};
	char *fg_app = app_manager_get_current_app();

	if (!fg_app)
		return -ENODEV;

#if 1
	/**ota not deal bt event when process*/
	if (memcmp(fg_app, "ota", strlen("ota")) == 0) {
		SYS_LOG_INF("skip msg %d to ota.\n", event_id);
		return 0;
	}
#endif

	if (event_data && event_data_size) {
		msg.ptr = mem_malloc(event_data_size + 1);
		if (!msg.ptr)
			return -ENOMEM;
		memset(msg.ptr, 0, event_data_size + 1);
		memcpy(msg.ptr, event_data, event_data_size);
		msg.callback = _bt_manager_event_notify_cb;
	}

	msg.type = MSG_BT_EVENT;
	msg.cmd = event_id;

	if (send_async_msg(fg_app, &msg)) {
		ret = 0;
	} else if (msg.ptr){
		mem_free(msg.ptr);
		ret = -EIO;
	} else {
		ret = -EIO;
	}

	return ret;
}

int bt_manager_event_notify_ext(int event_id, void *event_data, int event_data_size , MSG_CALLBAK call_cb)
{
	int ret = 0;

	struct app_msg  msg = {0};
	char *fg_app = app_manager_get_current_app();

	if (!fg_app)
		return -ENODEV;

#if 1
	/**ota not deal bt event when process*/
	if (memcmp(fg_app, "ota", strlen("ota")) == 0) {
		SYS_LOG_INF("skip msg %d to ota.\n", event_id);
		return -ENODEV;
	}
#endif

	if (event_data && event_data_size) {
		msg.ptr = mem_malloc(event_data_size + 1);
		if (!msg.ptr)
			return -ENOMEM;
		memset(msg.ptr, 0, event_data_size + 1);
		memcpy(msg.ptr, event_data, event_data_size);
		msg.callback = call_cb;
	}

	msg.type = MSG_BT_EVENT;
	msg.cmd = event_id;

	if (send_async_msg(fg_app, &msg)) {
		ret = 0;
	} else if (msg.ptr){
		mem_free(msg.ptr);
		ret = -EIO;
	} else {
		ret = -EIO;
	}

	return ret;
}
#endif


#if 1

typedef struct
{
	bd_address_t addr;
	uint8_t vol;
	uint8_t used;
}volume_table_t;
#define VOLUME_TABLE_SIZE 2
static volume_table_t	phone_music_volume[VOLUME_TABLE_SIZE];
static volume_table_t	phone_call_volume[VOLUME_TABLE_SIZE];
static volume_table_t	phone_lea_volume[VOLUME_TABLE_SIZE];
static uint8_t phone_volume_tws_sync_now = 0;
static OS_MUTEX_DEFINE(bt_manager_volume_mutex);


uint8_t put_phone_volume(bd_address_t *addr, int stream_type, uint8_t volume)
{
	int  i;
	volume_table_t* volume_table = (volume_table_t*)phone_music_volume;

	if (stream_type == AUDIO_STREAM_VOICE){
		volume_table = (volume_table_t*)phone_call_volume;
	}else if (stream_type == AUDIO_STREAM_LE_AUDIO){
		volume_table = (volume_table_t*)phone_lea_volume;
	}

	os_mutex_lock(&bt_manager_volume_mutex, OS_FOREVER);
	for (i = 0; i < VOLUME_TABLE_SIZE; i++)
	{
		if (volume_table[i].used == 0)
		{
			volume_table[i].vol = volume;
			volume_table[i].used = 1;
			memcpy(&volume_table[i].addr, addr, sizeof(bd_address_t));
		    break;
		}
	}

	if (i >= VOLUME_TABLE_SIZE)
	{
		for (i = 0; i < VOLUME_TABLE_SIZE - 1; i++)
		{
		    volume_table[i].vol = volume_table[i+1].vol;
			memcpy(&volume_table[i].addr, &volume_table[i+1].addr, sizeof(bd_address_t));
		}
		volume_table[i].vol = volume;
		memcpy(&volume_table[i].addr, addr, sizeof(bd_address_t));
	}

	os_mutex_unlock(&bt_manager_volume_mutex);
	return 0;
}


int16_t get_phone_volume(int stream_type, bd_address_t *addr)
{
	int  i;
	int16_t volume = 0;
	volume_table_t* volume_table = (volume_table_t*)phone_music_volume;

	if (stream_type == AUDIO_STREAM_VOICE){
		volume_table = (volume_table_t*)phone_call_volume;
	}else if (stream_type == AUDIO_STREAM_LE_AUDIO){
		volume_table = (volume_table_t*)phone_lea_volume;
	}

	os_mutex_lock(&bt_manager_volume_mutex, OS_FOREVER);

	if (!volume_table[0].used){
		os_mutex_unlock(&bt_manager_volume_mutex);
		return -1;
	}
	volume = volume_table[0].vol;
	memcpy(addr, &volume_table[0].addr, sizeof(bd_address_t));
	volume_table[0].used = 0;
	memset(&volume_table[0].addr, 0, sizeof(bd_address_t));

	for (i = 1; i < VOLUME_TABLE_SIZE; i++)
	{
		if (volume_table[i].used)
		{
			volume_table[i-1].vol = volume_table[i].vol;
			memcpy(&volume_table[i-1].addr, &volume_table[i].addr, sizeof(bd_address_t));
			volume_table[i-1].used = 1;
			volume_table[i].used = 0;
		}
	}
	SYS_LOG_INF("volume:%d",volume);
	os_mutex_unlock(&bt_manager_volume_mutex);
	return volume;
}

void do_phone_volume(void)
{
	bd_address_t addr;
	int volume = get_phone_volume(AUDIO_STREAM_VOICE, &addr);

	if (volume >=0)
	{
		bt_manager_tws_sync_volume_to_slave_inner(&addr, AUDIO_STREAM_VOICE, volume, 1);
	}
	else
	{
		volume = get_phone_volume(AUDIO_STREAM_MUSIC, &addr);
		if (volume >=0)
		{
			bt_manager_tws_sync_volume_to_slave_inner(&addr, AUDIO_STREAM_MUSIC, volume, 1);
		}
		else
		{
			volume = get_phone_volume(AUDIO_STREAM_LE_AUDIO, &addr);
			if (volume >=0){
				bt_manager_tws_sync_volume_to_slave_inner(&addr, AUDIO_STREAM_LE_AUDIO, volume, 1);
			}else {
				phone_volume_tws_sync_now = 0;
			}
		}
	}
}

void bt_manager_sync_volume_from_phone(bd_address_t *addr, bool is_avrcp, uint8_t volume, bool sync_wait, bool notify_app)
{
	int new_volume, old_volume;
	int stream_type = AUDIO_STREAM_VOICE;

	if (btif_tws_get_dev_role() == BTSRV_TWS_SLAVE){
		return;
	}

	if (is_avrcp){
		stream_type = AUDIO_STREAM_MUSIC;
	}
	new_volume = volume;
	old_volume = system_volume_get(stream_type);
    SYS_LOG_INF("volume:%d, %d",volume, old_volume);

	int current_stream = bt_manager_audio_current_stream();
	bool fg_app_btcall = false;
	bool can_update = true;

	if (current_stream  == BT_AUDIO_STREAM_BR_CALL)
	{
		fg_app_btcall = true;
	}

	if (fg_app_btcall && is_avrcp)
	{
		SYS_LOG_INF("volume ignore from avrcp");
		can_update = false;
	}
	else if (!fg_app_btcall && !is_avrcp)
	{
		SYS_LOG_INF("volume ignore from hfp");
		can_update = false;
	}

	if (current_stream == BT_AUDIO_STREAM_LE_MUSIC ||
		current_stream == BT_AUDIO_STREAM_LE_CALL  ||
		current_stream == BT_AUDIO_STREAM_LE_TWS )

	{
		SYS_LOG_INF("volume ignore when le_audio");
		can_update = false;
		notify_app = false;
	}

	//if (is_avrcp && current_app && !strcmp(APP_ID_BROADCAST_BR, current_app))
	if (is_avrcp && current_stream == BT_AUDIO_STREAM_BR_MUSIC)
	{
        bt_manager_volume_event_to_app(0,new_volume,1);
	}

	if (btif_tws_get_dev_role() == BTSRV_TWS_MASTER)
	{
		if (phone_volume_tws_sync_now == 0){
			phone_volume_tws_sync_now = 1;
			bt_manager_tws_sync_volume_to_slave_inner(addr, stream_type, volume, 1);
		}
		else{
			put_phone_volume(addr, stream_type, volume);
		}
	}
	else
	{
		if (can_update && notify_app)
		{
			system_player_volume_set(stream_type, volume);
			new_volume = system_volume_get(stream_type);
			goto Notify;
		}
	}

	return;

Notify:
	if (new_volume == old_volume)
		return;

#if 0
	if(new_volume >= audio_policy_get_volume_level())
	{
		bt_manager_sys_event_notify(SYS_EVENT_MAX_VOLUME);
	}
	else if(new_volume <= 0)
	{
		bt_manager_sys_event_notify(SYS_EVENT_MIN_VOLUME);
	}
#endif
}

int bt_manager_sync_volume_from_phone_leaudio(bd_address_t *addr, uint8_t volume)
{
	int stream_type = AUDIO_STREAM_LE_AUDIO;
	int new_volume, old_volume;
	int current_stream = bt_manager_audio_current_stream();

	new_volume = volume;
	old_volume = system_volume_get(stream_type);
	SYS_LOG_INF("vol new:%d,old:%d\n",volume, old_volume);

	if (current_stream == BT_AUDIO_STREAM_LE_MUSIC)
	{
		bt_manager_volume_event_to_app(0,new_volume,1);
	}

	if (btif_tws_get_dev_role() == BTSRV_TWS_SLAVE){
		return 0;
	}

	if (btif_tws_get_dev_role() == BTSRV_TWS_MASTER)
	{
		if (phone_volume_tws_sync_now == 0){
			phone_volume_tws_sync_now = 1;
			bt_manager_tws_sync_volume_to_slave_inner(addr, stream_type, volume, 1);
		}
		else{
			put_phone_volume(addr, stream_type, volume);
		}
		return 0;
	}

	return 1;
}

#endif
int bt_manager_state_notify(int state)
{
	struct app_msg  msg = {0};

	msg.type = MSG_BT_EVENT;
	msg.cmd = state;

	return send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
}

void bt_manager_sys_event_notify(int event)
{
    SYS_LOG_INF("event:%d, role:%d\n",event, btif_tws_get_dev_role());
    if (btif_tws_get_dev_role() == BTSRV_TWS_SLAVE){
        return;
    }
	sys_event_send_message_new(MSG_SYS_EVENT, event, NULL, 0);
}


void bt_manager_update_phone_volume(uint16_t hdl,int a2dp)
{
	bt_mgr_dev_info_t* phone_dev = NULL;
	int current_stream = bt_manager_audio_current_stream();

	if (hdl)
	{
		phone_dev = bt_mgr_find_dev_info_by_hdl(hdl);
	}
	else
	{
		uint16_t active_hdl = bt_manager_find_active_slave_handle();
		if (!active_hdl) {
			SYS_LOG_INF("active_hdl %d", active_hdl);
			return;
		}
		phone_dev = bt_mgr_find_dev_info_by_hdl(active_hdl);
	}

	if (!phone_dev || phone_dev->is_tws){
		return;
	}
	SYS_LOG_INF("0x%x, %d, %d", phone_dev->hdl, phone_dev->bt_music_vol,phone_dev->bt_call_vol);

	if (a2dp && (current_stream == BT_AUDIO_STREAM_BR_CALL))
	{
		SYS_LOG_INF("ignore");
		return;
	}

	if (current_stream == BT_AUDIO_STREAM_LE_MUSIC ||
		current_stream == BT_AUDIO_STREAM_LE_CALL  ||
		current_stream == BT_AUDIO_STREAM_LE_TWS )
	{
		SYS_LOG_INF("volume ignore when le_audio");
		return;
	}

	audio_system_mutex_lock();
	if (current_stream == BT_AUDIO_STREAM_BR_CALL)
	{
		audio_system_set_stream_volume(AUDIO_STREAM_MUSIC, phone_dev->bt_music_vol);
		system_player_volume_set(AUDIO_STREAM_VOICE, phone_dev->bt_call_vol);
	}
	else
	{
		audio_system_set_stream_volume(AUDIO_STREAM_VOICE, phone_dev->bt_call_vol);
		system_player_volume_set(AUDIO_STREAM_MUSIC, phone_dev->bt_music_vol);
	}
	audio_system_mutex_unlock();
}
#if 0
static void bt_manager_sys_event_led_display(int event)
{
	struct app_msg	msg = {0};
	msg.type = MSG_SYS_EVENT;
	msg.cmd = event;
	msg.reserve = 1;
	send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
}
#endif

void bt_manager_update_led_display(void)
{
#if 0
    bt_manager_context_t*  bt_manager = bt_manager_get_context();

    switch (bt_manager->bt_state)
    {
        case BT_STATUS_LINK_NONE:
        {
            bt_manager_sys_event_led_display(SYS_EVENT_BT_UNLINKED);
            break;
        }

        case BT_STATUS_WAIT_CONNECT:
        {
            bt_manager_sys_event_led_display(SYS_EVENT_BT_WAIT_PAIR);
            break;
        }

        case BT_STATUS_PAIR_MODE:
        {
            bt_manager_sys_event_led_display(SYS_EVENT_ENTER_PAIR_MODE);
            break;
        }

        case BT_STATUS_TWS_PAIR_SEARCH:
        {
            bt_manager_sys_event_led_display(SYS_EVENT_TWS_START_PAIR);
            break;
        }

        case BT_STATUS_CONNECTED:
        {
            if (bt_manager_get_connected_dev_num() > 0)
            {
				bt_manager_sys_event_led_display(SYS_EVENT_BT_CONNECTED);
            }
            else
            {
				bt_manager_sys_event_led_display(SYS_EVENT_TWS_CONNECTED);
            }
            break;
        }
    }
#endif
}
