/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt manager tws snoop ui event.
 */
#define SYS_LOG_DOMAIN "bt manager"

#include <os_common_api.h>

#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stream.h>
#include <acts_ringbuf.h>
#include <bt_manager.h>
#include <sys_event.h>
#include <app_manager.h>
#include <power_manager.h>
#include "bt_manager_inner.h"
#include "audio_policy.h"
#include "btservice_api.h"
#include "media_player.h"
#include <volume_manager.h>
#include <audio_system.h>
#include <input_manager.h>
#ifdef CONFIG_PROPERTY
#include "property_manager.h"
#endif
#include "btmgr_tws_snoop_inner.h"
//#include "bt_porting_inner.h"
#include <hex_str.h>

#define WAIT_TWS_READY_SEND_SYNC_TIMEOUT		500		/* 500ms */

static tws_long_message_cb_t tws_long_msg_cb_func = NULL;

OS_MUTEX_DEFINE(tws_snoop_send_sync_mutex);
OS_MUTEX_DEFINE(tws_snoop_send_long_mutex);

static void btmgr_send_event_to_main(uint32_t event_id, uint32_t bt_clock)
{
	struct app_msg  msg = {0};

	msg.type = MSG_SYS_EVENT;
	msg.cmd = event_id;
	send_async_msg(CONFIG_SYS_APP_NAME, &msg);
	return;
}


static int bt_manager_tws_reply_handle(uint8_t reply,uint8_t event)
{
	struct btmgr_tws_context_t* context = btmgr_get_tws_context();
    bt_manager_context_t*  bt_manager = bt_manager_get_context();

    SYS_LOG_INF("reply:%d, event:0x%x", reply, event);

	if(event == TWS_EVENT_ENTER_PAIR_MODE){
		bt_manager->pair_mode_sync_wait_reply = 1;
	}
	else if(event == TWS_EVENT_CLEAR_PAIRED_LIST){
		bt_manager->clear_paired_list_sync_wait_reply = 1;
	}

	if(context->tws_reply_event == event){
		context->tws_reply_value = reply;
	}
    return 0;
}

static int le_audio_dir_adv_init(void)
{
	struct btmgr_tws_context_t* context = btmgr_get_tws_context();
	struct bt_le_adv_param param = {0};

	if (!context->le_remote_addr_valid) {
		return 0;
	}

	param.id = BT_ID_DEFAULT;
	param.interval_min = BT_GAP_ADV_FAST_INT_MIN_2;
	param.interval_max = BT_GAP_ADV_FAST_INT_MAX_2;
	param.options = BT_LE_ADV_OPT_CONNECTABLE;
	param.options |= BT_LE_ADV_OPT_DIR_MODE_LOW_DUTY;

	param.peer = &context->remote_ble_addr;

	bt_manager_audio_le_adv_param_init(&param);
	bt_manager_audio_le_resume_adv();

	return 0;
}

static int bt_manager_tws_inner_event_handle(tws_message_t *msg)
{
    int ret_val = 0;
    uint8_t reply = TWS_SYNC_REPLY_ACK;

    switch (msg->cmd_id)
    {
        case TWS_EVENT_REPLY:
			bt_manager_tws_reply_handle(msg->cmd_data[0],msg->cmd_data[1]);
            break;
	    case TWS_EVENT_SYNC_PAIR_MODE:
			bt_manager_tws_sync_pair_mode(msg->cmd_data[0]);
			break;
        case TWS_EVENT_ENTER_PAIR_MODE:
			bt_manager_tws_enter_pair_mode();
            break;
		case TWS_EVENT_CLEAR_PAIRED_LIST:
			bt_manager_tws_clear_paired_list();
			break;
		case TWS_EVENT_BT_HID_SEND_KEY:
#ifdef CONFIG_BT_HID
			bt_manager_hid_send_key(msg->cmd_data[0]);
#endif
			break;
		case TWS_EVENT_SYNC_POWEROFF_INFO:
			btmgr_tws_poff_info_proc(msg->cmd_data, msg->cmd_len);
			break;
		case TWS_EVENT_BT_MUSIC_VOL:
		{
			bt_mgr_dev_info_t * a2dp_dev = bt_mgr_get_a2dp_active_dev();
			bt_mgr_dev_info_t* phone_dev = bt_mgr_find_dev_info((bd_address_t*)&msg->cmd_data[0]);
			if (phone_dev)
			{
				phone_dev->bt_music_vol = msg->cmd_data[6];
				SYS_LOG_INF("bt_music_vol:%d", phone_dev->bt_music_vol);
				bt_manager_save_dev_volume();
			}

			if (phone_dev == NULL ||
				a2dp_dev == NULL  ||
				phone_dev->hdl == a2dp_dev->hdl)
			{
				if (bt_manager_audio_current_stream() == BT_AUDIO_STREAM_BR_MUSIC)
				{
#if 0
					uint8_t from_phone = msg->cmd_data[7];
					int old_volume = system_volume_get(AUDIO_STREAM_MUSIC);
					int new_volume = system_player_volume_set(AUDIO_STREAM_MUSIC, msg->cmd_data[6]);
#else
					system_player_volume_set(AUDIO_STREAM_MUSIC, msg->cmd_data[6]);
#endif

#if 0
					if ((btif_tws_get_dev_role() == BTSRV_TWS_MASTER) && from_phone
						&& new_volume != old_volume)
					{
						if(new_volume >= audio_policy_get_volume_level())
						{
							bt_manager_sys_event_notify(SYS_EVENT_MAX_VOLUME);
						}
						else if(new_volume <= 0)
						{
							bt_manager_sys_event_notify(SYS_EVENT_MIN_VOLUME);
						}
					}
#endif
				}
			}
		}
			break;
		case TWS_EVENT_BT_CALL_VOL:
		{
			bt_mgr_dev_info_t * hfp_dev = bt_mgr_get_hfp_active_dev();
			bt_mgr_dev_info_t* phone_dev = bt_mgr_find_dev_info((bd_address_t*)&msg->cmd_data[0]);
			if (phone_dev)
			{
				phone_dev->bt_call_vol = msg->cmd_data[6];
				SYS_LOG_INF("bt_call_vol:%d", phone_dev->bt_call_vol);
				bt_manager_save_dev_volume();
			}
			if (phone_dev == NULL ||
				hfp_dev == NULL  ||
				phone_dev->hdl == hfp_dev->hdl)
			{
				if (bt_manager_audio_current_stream() == BT_AUDIO_STREAM_BR_CALL)
				{
					uint8_t from_phone = msg->cmd_data[7];
					int old_volume = system_volume_get(AUDIO_STREAM_VOICE);
					int new_volume = system_player_volume_set(AUDIO_STREAM_VOICE, msg->cmd_data[6]);

					if ((btif_tws_get_dev_role() == BTSRV_TWS_MASTER) && from_phone
						&& new_volume != old_volume)
					{
						if(new_volume >= audio_policy_get_volume_level())
						{
							bt_manager_sys_event_notify(SYS_EVENT_MAX_VOLUME);
						}
						else if(new_volume <= 0)
						{
							bt_manager_sys_event_notify(SYS_EVENT_MIN_VOLUME);
						}
					}
				}
			}
		}
			break;
		case TWS_EVENT_BT_LEA_VOL:
		{
		//	bt_manager_audio_update_volume((bd_address_t*)&msg->cmd_data[0],msg->cmd_data[6]);
			if (bt_manager_audio_current_stream() == BT_AUDIO_STREAM_LE_MUSIC){
				bt_manager_volume_event_to_app(0,  msg->cmd_data[6], msg->cmd_data[7]);
			}
		}
			break;
		case TWS_EVENT_SYNC_PHONE_INFO:
		{
			bt_manager_tws_sync_phone_info(0, msg->cmd_data);
		}
			break;

		case TWS_EVENT_SYNC_A2DP_STATUS:
	    {
			bt_manager_tws_sync_a2dp_status(0, msg->cmd_data);
			break;
		}

		case TWS_EVENT_SYNC_LEA_INFO:
		{
			bt_manager_tws_sync_lea_info(msg->cmd_data);
		}
			break;

		case TWS_EVENT_SYNC_BLE_MAC:
		{
			bt_manager_tws_sync_ble_mac(msg->cmd_data);
			break;
		}
		case TWS_EVENT_SYNC_ACTIVE_APP:
		{
			bt_manager_tws_sync_active_dev(msg->cmd_data);
		}
			break;
		case TWS_EVENT_ENABLE_LEA_DIR_ADV:
			//le_audio_dir_adv_init();
			bt_manager_tws_sync_enable_lea_dir_adv_inner(msg->cmd_data);
			break;
	    default:
			break;
    }

    if ((msg->cmd_id == TWS_EVENT_ENTER_PAIR_MODE)
		|| (msg->cmd_id == TWS_EVENT_CLEAR_PAIRED_LIST)){
		bt_manager_tws_reply_sync_message(TWS_BT_MGR_EVENT, msg->cmd_id, reply);
    }
    return ret_val;
}
void bt_manager_tws_send_event(uint8_t event, uint32_t event_param)
{
	uint8_t tws_event_data[5];

    if (btif_tws_get_dev_role() < BTSRV_TWS_MASTER) {
		return;
    }

	memset(tws_event_data, 0, sizeof(tws_event_data));
	tws_event_data[0] = event;
	memcpy(&tws_event_data[1], &event_param, 4);

	bt_manager_tws_send_command(tws_event_data, sizeof(tws_event_data));
}

void bt_manager_tws_send_event_sync(uint8_t event, uint32_t event_param)
{
	uint8_t tws_event_data[10];
	uint32_t bt_clock;

    if (btif_tws_get_dev_role() < BTSRV_TWS_MASTER) {
		return;
    }

	bt_manager_wait_tws_ready_send_sync_msg(WAIT_TWS_READY_SEND_SYNC_TIMEOUT);
	bt_clock = bt_manager_tws_get_bt_clock() + 100;

	memset(tws_event_data, 0, sizeof(tws_event_data));
	tws_event_data[0] = event;
	memcpy(&tws_event_data[1], &event_param, 4);
	memcpy(&tws_event_data[5], &bt_clock, 4);

	bt_manager_tws_send_command(tws_event_data, sizeof(tws_event_data));
	btmgr_send_event_to_main(event_param, bt_clock);
}


int bt_manager_tws_send_message(uint8_t event_type, uint8_t app_event, uint8_t* param,uint32_t param_len)
{
	#define LONG_SEND_MAX_TIME (20)
    int ret = 0, retry_times;

    if (btif_tws_get_dev_role() < BTSRV_TWS_MASTER) {
		return 0;
    }

    if ((event_type == TWS_USER_APP_EVENT) || (event_type == TWS_BT_MGR_EVENT))
    {
        tws_message_t data;
        memset(&data, 0, sizeof(tws_message_t));
        data.sub_group_id = event_type;
        data.cmd_id = app_event;
        if (param && param_len > 0)
        {
            if(param_len > sizeof(data.cmd_data))
            {
                SYS_LOG_ERR("param_len too long: %d", param_len);
                ret = -1;
                goto End;
            }
            data.cmd_len = param_len;
            memcpy(data.cmd_data, param, param_len);
        }
        SYS_LOG_INF("type=%d, %u", event_type, app_event);
        bt_manager_tws_send_command((char*)&data,
            sizeof(tws_message_t) - sizeof(data.cmd_data) + data.cmd_len
        );
    }
    else if (event_type == TWS_LONG_MSG_EVENT)
    {
        uint32_t size = param_len + sizeof(tws_long_message_t);
        char* send_buffer = mem_malloc(size);
        if (!send_buffer)
        {
            SYS_LOG_ERR("malloc fail");
            ret = -1;
            goto End;
        }
        os_mutex_lock(&tws_snoop_send_long_mutex, OS_FOREVER);
        memset(send_buffer, 0, size);
        tws_long_message_t* msg = (tws_long_message_t*)send_buffer;
        msg->sub_group_id = event_type;
        msg->cmd_id = app_event;
        msg->cmd_len = param_len;
        if (param)
        {
            memcpy((send_buffer + sizeof(tws_long_message_t)), param, param_len);
        }

		retry_times = 0;
		while (retry_times < LONG_SEND_MAX_TIME) {
			ret = bt_manager_tws_send_command(send_buffer, param_len + sizeof(tws_long_message_t));
			if (ret == (param_len + sizeof(tws_long_message_t))) {
				break;
			} else {
				retry_times++;
				os_sleep(10);
			}
		}

		if (retry_times >= LONG_SEND_MAX_TIME) {
			SYS_LOG_ERR("Failed to send long mesage!");
		} else {
			ret = param_len;
		}
		mem_free(send_buffer);
		os_mutex_unlock(&tws_snoop_send_long_mutex);
    }
    else
    {
        SYS_LOG_ERR("event: 0x%x unsupport", event_type);
        ret = -1;
    }

End:
    return ret;
}

static void tws_app_event_sync_cb(void* param, uint32_t bt_clock)
{
	tws_message_t* tws_data = (tws_message_t*)param;
	
	SYS_LOG_INF("%u, %u",tws_data->sync_clock, bt_clock);

	bt_manager_event_notify(BT_TWS_EVENT, tws_data, sizeof(tws_message_t));
	mem_free(tws_data);
}

static void tws_mgr_event_sync_cb(void* param, uint32_t bt_clock)
{
	tws_message_t* tws_data = (tws_message_t*)param;

	SYS_LOG_INF("%u, %u",tws_data->sync_clock, bt_clock);
	bt_manager_tws_inner_event_handle((tws_message_t *)tws_data);
	mem_free(tws_data);

	do_phone_volume();
}

int bt_manager_tws_send_sync_message(uint8_t event_type, uint8_t app_event, void* param,uint32_t param_len, uint32_t sync_wait)
{
    #define SYNC_DEFAULT_WAIT_MS (100)
    int ret = 0;
    uint32_t bt_clock = 0;
    uint32_t wait_ms = sync_wait? sync_wait:SYNC_DEFAULT_WAIT_MS;
    uint32_t real_bt_clock = 0;

    if (btif_tws_get_dev_role() < BTSRV_TWS_MASTER) {
        return 0;
    }
    os_mutex_lock(&tws_snoop_send_sync_mutex, OS_FOREVER);
    if ((event_type == TWS_USER_APP_EVENT) || (event_type == TWS_BT_MGR_EVENT))
    {
        tws_message_t* data = mem_malloc(sizeof(tws_message_t));
        if (!data)
        {
            SYS_LOG_ERR("malloc fail");
            ret = -1;
            goto End;
        }
        memset(data, 0, sizeof(tws_message_t));

        data->sub_group_id = event_type;
        data->cmd_id = app_event;
        if (param && param_len > 0)
        {
            if(param_len > sizeof(data->cmd_data))
            {
                SYS_LOG_ERR("param_len too long: %d\n", param_len);
                ret = -1;
				mem_free(data);
                goto End;
            }
            data->cmd_len = param_len;
            memcpy(data->cmd_data, param, param_len);
        }

		bt_manager_wait_tws_ready_send_sync_msg(WAIT_TWS_READY_SEND_SYNC_TIMEOUT);
		bt_clock = tws_timer_get_near_btclock() + wait_ms;
        real_bt_clock = bt_clock;
        if (real_bt_clock == 0)
        {
            real_bt_clock = SYNC_DEFAULT_WAIT_MS;
        }
        data->sync_clock = real_bt_clock;
        bt_manager_tws_send_command((char*)data,
            sizeof(tws_message_t) - sizeof(data->cmd_data) + data->cmd_len
        );

		if (event_type == TWS_USER_APP_EVENT){
			tws_timer_add(real_bt_clock, tws_app_event_sync_cb, data);
		}else{
			tws_timer_add(real_bt_clock, tws_mgr_event_sync_cb, data);
		}
    }
    else
    {
        SYS_LOG_ERR("event: 0x%x unsupport.\n", event_type);
        ret = -1;
    }
End:
	SYS_LOG_INF("%u, %u", app_event, real_bt_clock);
    os_mutex_unlock(&tws_snoop_send_sync_mutex);
    return ret;
}

int bt_manager_tws_reply_sync_message(uint8_t event_type, uint8_t reply_event, uint8_t reply_value)
{
    uint8_t  param[2];

    param[0] = (uint8_t)reply_value;
    param[1] = reply_event;

    return bt_manager_tws_send_message(event_type, TWS_EVENT_REPLY, param, 2);
}

int bt_manager_tws_register_long_message_cb(tws_long_message_cb_t cb)
{
	tws_long_msg_cb_func = cb;
	return 0;
}


void bt_manager_tws_notify_start_play(uint8_t media_type, uint8_t codec_id, uint8_t sample_rate)
{
    //unused for snoop
}

void bt_manager_tws_notify_stop_play(void)
{
    //unused for snoop
}



void bt_manager_tws_sync_volume_to_slave_inner(bd_address_t* bt_addr,uint32_t media_type, uint32_t music_vol, uint32_t from_phone)
{
    uint8_t  params[8];
	bd_address_t* dev_addr = bt_addr;

    if (btif_tws_get_dev_role() != BTSRV_TWS_MASTER)
    {
        return;
    }

	SYS_LOG_INF("tws_sync_volume_to_slave:[%d %d]\n", media_type, music_vol);

	memset(params,0,sizeof(params));
	if (AUDIO_STREAM_MUSIC == media_type)
	{
		if (!dev_addr)
		{
			bt_mgr_dev_info_t * dev_info = bt_mgr_get_a2dp_active_dev();
			if (dev_info){
				dev_addr = &dev_info->addr;
			}
		}

        if (dev_addr)
        {
            memcpy(&params[0], dev_addr, sizeof(bd_address_t));
        }
		params[6] = music_vol;
		params[7] = from_phone;
		if (from_phone){
			bt_manager_tws_send_sync_message(TWS_BT_MGR_EVENT, TWS_EVENT_BT_MUSIC_VOL, params, sizeof(params), 40);
		}else{
			bt_manager_tws_send_message(TWS_BT_MGR_EVENT, TWS_EVENT_BT_MUSIC_VOL, params, sizeof(params));
		}
	}
	else if(AUDIO_STREAM_VOICE == media_type)
	{
		if (!dev_addr)
		{
			bt_mgr_dev_info_t * dev_info = bt_mgr_get_hfp_active_dev();
			if (dev_info){
				dev_addr = &dev_info->addr;
			}
		}

        if (dev_addr)
        {
            memcpy(&params[0], dev_addr, sizeof(bd_address_t));
        }

		params[6] = music_vol;
		params[7] = from_phone;
		if (from_phone){
			bt_manager_tws_send_sync_message(TWS_BT_MGR_EVENT, TWS_EVENT_BT_CALL_VOL, params, sizeof(params), 40);
		}else{
			bt_manager_tws_send_message(TWS_BT_MGR_EVENT, TWS_EVENT_BT_CALL_VOL, params, sizeof(params));
		}
	}
#if 0
	else if(AUDIO_STREAM_LE_AUDIO == media_type || AUDIO_STREAM_LE_AUDIO_MUSIC == media_type)
	{
		if (!dev_addr)
		{
			dev_addr = bt_manager_audio_lea_addr();
		}

        if (dev_addr)
        {
            memcpy(&params[0], dev_addr, sizeof(bd_address_t));
        }

		params[6] = music_vol;
		params[7] = from_phone;
		if (from_phone){
			bt_manager_tws_send_sync_message(TWS_BT_MGR_EVENT, TWS_EVENT_BT_LEA_VOL, params, sizeof(params), 40);
		}else{
			bt_manager_tws_send_message(TWS_BT_MGR_EVENT, TWS_EVENT_BT_LEA_VOL, params, sizeof(params));
		}
	}
#endif


}


void bt_manager_tws_sync_volume_to_slave(bd_address_t* bt_addr,uint32_t media_type, uint32_t music_vol)
{
	bt_manager_tws_sync_volume_to_slave_inner(bt_addr, media_type, music_vol, 0);
}


void bt_manager_tws_set_stream_type(uint8_t stream_type)
{
	struct btmgr_tws_context_t *tws_context = btmgr_get_tws_context();

	tws_context->source_stream_type = stream_type;

    //unused for snoop
}

void bt_manager_tws_channel_mode_switch(void)
{
    //unused for snoop
}

int __weak bt_manager_tws_sync_app_status_info(void* param)
{
    SYS_LOG_WRN("Weak function");
    return 0;
}


void bt_manager_tws_sync_phone_info(uint16_t hdl, void* param)
{
    bt_mgr_dev_info_t*  phone_dev;
	char addr_str[BT_ADDR_STR_LEN];

    if (btif_tws_get_dev_role() == BTSRV_TWS_MASTER)
    {
        bt_manager_tws_sync_phone_info_t  phone_info;
        phone_dev = bt_mgr_find_dev_info_by_hdl(hdl);
        if (phone_dev == NULL)
            return;

		bt_addr_to_str((const bt_addr_t *)&phone_dev->addr, addr_str, BT_ADDR_STR_LEN);
        SYS_LOG_INF("[master]addr:%s music_vol:%d, call_vol:%d",addr_str, phone_dev->bt_music_vol,phone_dev->bt_call_vol);
        memcpy(&phone_info.bd_addr, &phone_dev->addr, sizeof(bd_address_t));
        phone_info.hdl = hdl;
        phone_info.a2dp_stream_started = phone_dev->a2dp_stream_started;
        phone_info.a2dp_status_playing = phone_dev->a2dp_status_playing;
        phone_info.bt_music_vol = phone_dev->bt_music_vol;
        phone_info.bt_call_vol = phone_dev->bt_call_vol;
        phone_info.avrcp_ext_status = phone_dev->avrcp_ext_status;

        bt_manager_tws_send_message
        (
        	TWS_BT_MGR_EVENT, TWS_EVENT_SYNC_PHONE_INFO,
        	(uint8_t *)&phone_info, sizeof(phone_info)
        );
    }
    else if (btif_tws_get_dev_role() == BTSRV_TWS_SLAVE)
    {
        bt_manager_tws_sync_phone_info_t*  phone_info = (bt_manager_tws_sync_phone_info_t*)param;
        if (param == NULL)
            return;

        phone_dev = bt_mgr_find_dev_info(&phone_info->bd_addr);
        if (phone_dev != NULL)
        {
            phone_dev->a2dp_stream_started = phone_info->a2dp_stream_started;
            phone_dev->a2dp_status_playing = phone_info->a2dp_status_playing;
            phone_dev->bt_music_vol = phone_info->bt_music_vol;
            phone_dev->bt_call_vol = phone_info->bt_call_vol;
            phone_dev->avrcp_ext_status = phone_info->avrcp_ext_status;

			bt_addr_to_str((const bt_addr_t *)&phone_dev->addr, addr_str, BT_ADDR_STR_LEN);
            SYS_LOG_INF("[slave]addr:%s bt_music_vol:%d bt_call_vol:%d",addr_str, phone_dev->bt_music_vol,phone_dev->bt_call_vol);
            bt_manager_save_dev_volume();
        }
    }
}

void bt_manager_tws_sync_a2dp_status(uint16_t hdl, void* param)
{
    bt_mgr_dev_info_t* phone_dev;

    if (btif_tws_get_dev_role() == BTSRV_TWS_MASTER)
    {
        uint8_t data[sizeof(bd_address_t) + 1];

        phone_dev = bt_mgr_find_dev_info_by_hdl(hdl);
        if (phone_dev == NULL)
            return;

        SYS_LOG_INF("0x%x, %d", hdl, phone_dev->a2dp_status_playing);

        memcpy(&data[0], &phone_dev->addr, sizeof(bd_address_t));
        data[sizeof(bd_address_t)] = phone_dev->a2dp_status_playing;

        bt_manager_tws_send_message(
        	TWS_BT_MGR_EVENT, TWS_EVENT_SYNC_A2DP_STATUS, data, sizeof(data)
        );
    }
    else if (btif_tws_get_dev_role() == BTSRV_TWS_SLAVE)
    {
        uint8_t* data = param;
        uint8_t  playing;

        if (param == NULL)
            return;

        phone_dev = bt_mgr_find_dev_info((bd_address_t*)&data[0]);
        if (phone_dev == NULL)
            return;

        playing = data[sizeof(bd_address_t)];
        SYS_LOG_INF("0x%x, %d", phone_dev->hdl, playing);

        if (phone_dev->a2dp_status_playing != playing)
        {
            phone_dev->a2dp_status_playing = playing;

            if (!phone_dev->a2dp_status_playing)
            {
                struct bt_media_play_status playstatus;

                playstatus.handle = phone_dev->hdl;
                playstatus.status = BT_STATUS_PAUSED;

                bt_manager_media_event(BT_MEDIA_PLAYBACK_STATUS_CHANGED_EVENT,
                    (void*)&playstatus, sizeof(struct bt_media_play_status)
                );
            }
        }
    }
}



void bt_manager_tws_sync_lea_info(void* param)
{
#if 0
	uint8_t app_restart = 0;
	uint8_t local_rank = 0;

    if (btif_tws_get_dev_role() == BTSRV_TWS_MASTER)
    {
        bt_manager_tws_sync_lea_info_t  local_lea_info;

		memset(&local_lea_info, 0, sizeof(bt_manager_tws_sync_lea_info_t));
        if (bt_manager_audio_get_public_sirk(local_lea_info.sirk, sizeof(local_lea_info.sirk)) == 1){
			app_restart = 1;
		}

        if (bt_manager_audio_get_rank_and_size(&local_lea_info.rank, NULL) == 1){
			app_restart = 1;
            local_lea_info.rank = 1;
            bt_manager_audio_set_rank(1);
		}

		if (bt_manager_audio_get_peer_addr(&local_lea_info.peer) == 0){
			local_lea_info.peer_valid = 1;
		}

        bt_manager_tws_send_message
        (
        	TWS_BT_MGR_EVENT, TWS_EVENT_SYNC_LEA_INFO,
        	(uint8_t *)&local_lea_info, sizeof(local_lea_info)
        );
    }
    else if (btif_tws_get_dev_role() == BTSRV_TWS_SLAVE)
    {
        bt_manager_tws_sync_lea_info_t*  remote_lea_info = (bt_manager_tws_sync_lea_info_t*)param;
        if (param == NULL)
            return;

		if (remote_lea_info->rank == 1){
			local_rank = 2;
        }else{
			local_rank = 1;
		}

		if (bt_manager_audio_set_rank(local_rank)){
			app_restart = 1;
		}

		if (bt_manager_audio_set_public_sirk(remote_lea_info->sirk, sizeof(remote_lea_info->sirk))){
			app_restart = 1;
		}

		if (remote_lea_info->peer_valid){
			if (bt_manager_audio_save_peer_addr(&remote_lea_info->peer) == 0){
				app_restart = 1;
			}
		}
    }

	if (app_restart){
		bt_manager_event_notify(BT_REQUEST_LEAUDIO_RESTART, NULL, 0);
	}
#endif
}

void bt_manager_tws_sync_ble_mac(void* param)
{
//#ifdef CONFIG_BT_LE_AUDIO

//    SYS_LOG_INF(":%d\n",btif_tws_get_dev_role());

	if (btif_tws_get_dev_role() == BTSRV_TWS_MASTER)
    {
		bt_addr_le_t local_ble_addr;

		memset(&local_ble_addr, 0, sizeof(bt_addr_le_t));
		btif_ble_get_local_mac(&local_ble_addr);
        bt_manager_tws_send_message
        (
        	TWS_BT_MGR_EVENT, TWS_EVENT_SYNC_BLE_MAC,
        	(uint8_t *)&local_ble_addr, sizeof(local_ble_addr)
        );
        print_buffer(&local_ble_addr,1,6,16,0);
    }
    else if (btif_tws_get_dev_role() == BTSRV_TWS_SLAVE)
    {
        bt_addr_le_t *remote_ble_addr = (bt_addr_le_t*)param;
		struct btmgr_tws_context_t* context = btmgr_get_tws_context();
        if (param == NULL)
            return;

		context->le_remote_addr_valid = 1;
		memcpy(&(context->remote_ble_addr),remote_ble_addr,sizeof(bt_addr_le_t));
        print_buffer(&context->remote_ble_addr,1,6,16,0);
    }
//#endif
}


void bt_manager_tws_sync_active_dev(void* param)
{
#if 0

	if (bt_manager_is_poweroffing())
		return;


    if (btif_tws_get_dev_role() == BTSRV_TWS_MASTER)
    {
        bt_manager_tws_sync_active_dev_t  syncinfo;
		memset(&syncinfo,0, sizeof(bt_manager_tws_sync_active_dev_t));
		bt_manager_audio_get_active_app(&syncinfo.active_app, &syncinfo.active_addr, &syncinfo.interrupt_active_addr);

        SYS_LOG_INF("master %02x:%02x:%02x:%02x:%02x:%02x, %d",
			syncinfo.active_addr.val[5],syncinfo.active_addr.val[4],syncinfo.active_addr.val[3],
			syncinfo.active_addr.val[2],syncinfo.active_addr.val[1],syncinfo.active_addr.val[0],
			syncinfo.active_app);

        SYS_LOG_INF("interrupted %02x:%02x:%02x:%02x:%02x:%02x",
			syncinfo.interrupt_active_addr.val[5],syncinfo.interrupt_active_addr.val[4],syncinfo.interrupt_active_addr.val[3],
			syncinfo.interrupt_active_addr.val[2],syncinfo.interrupt_active_addr.val[1],syncinfo.interrupt_active_addr.val[0]);

        bt_manager_tws_send_message
        (
        	TWS_BT_MGR_EVENT, TWS_EVENT_SYNC_ACTIVE_APP,
        	(uint8_t *)&syncinfo, sizeof(syncinfo)
        );

    }
    else if (btif_tws_get_dev_role() == BTSRV_TWS_SLAVE)
    {
        bt_manager_tws_sync_active_dev_t*  syncinfo = (bt_manager_tws_sync_active_dev_t*)param;
        if (param == NULL)
            return;

        SYS_LOG_INF("slave %02x:%02x:%02x:%02x:%02x:%02x, %d",
			syncinfo->active_addr.val[5],syncinfo->active_addr.val[4],syncinfo->active_addr.val[3],
			syncinfo->active_addr.val[2],syncinfo->active_addr.val[1],syncinfo->active_addr.val[0],
			syncinfo->active_app);

        SYS_LOG_INF("interrupted %02x:%02x:%02x:%02x:%02x:%02x",
			syncinfo->interrupt_active_addr.val[5],syncinfo->interrupt_active_addr.val[4],syncinfo->interrupt_active_addr.val[3],
			syncinfo->interrupt_active_addr.val[2],syncinfo->interrupt_active_addr.val[1],syncinfo->interrupt_active_addr.val[0]);


		bt_manager_audio_sync_remote_active_app(syncinfo->active_app, &syncinfo->active_addr,&syncinfo->interrupt_active_addr);
    }
#endif
}



void btmgr_tws_process_ui_event(uint8_t *data, uint8_t len)
{
	uint8_t event = data[0];
	uint32_t event_param;
	uint32_t value;
	uint8_t *tmp_in_data;

	tmp_in_data = mem_malloc(len);

	memcpy(tmp_in_data, data, len);
	memcpy(&event_param, &data[1], 4);

	print_buffer_lazy("UI Event", data, len);

	switch (event) {
	case TWS_BT_MGR_EVENT:
	{
		tws_message_t* msg = (tws_message_t*)tmp_in_data;
		if (msg->sync_clock){
			tws_message_t* tmp = mem_malloc(sizeof(tws_message_t));
			if(!tmp){
				break;
			}
			memcpy(tmp, msg, sizeof(tws_message_t));
			tws_timer_add(msg->sync_clock, tws_mgr_event_sync_cb, tmp);
		}else{
			bt_manager_tws_inner_event_handle((tws_message_t *)tmp_in_data);
		}
	}
	break;
	case TWS_INPUT_EVENT:
		sys_event_report_input(event_param);
		break;
	case TWS_UI_EVENT:
		memcpy(&value, &data[5], 4);
		btmgr_send_event_to_main(event_param, value);
		break;
	case TWS_SYSTEM_EVENT:
		sys_event_send_message(event_param);
		break;
	case TWS_BATTERY_EVENT:
#ifdef CONFIG_POWER
		power_manager_set_slave_battery_state((event_param >> 24) & 0xff, (event_param & 0xffffff));
#endif
		break;
    case TWS_USER_APP_EVENT:
    {
		tws_message_t* msg = (tws_message_t*)tmp_in_data;
		if (msg->sync_clock){
			tws_message_t* tmp = mem_malloc(sizeof(tws_message_t));
			if(!tmp){
				break;
			}
			memcpy(tmp, msg, sizeof(tws_message_t));
			tws_timer_add(msg->sync_clock, tws_app_event_sync_cb, tmp);
		}else{
			bt_manager_event_notify(BT_TWS_EVENT, msg, sizeof(tws_message_t));
		}
	}
		break;
	case TWS_LONG_MSG_EVENT:
	{
		tws_long_message_t* msg = (tws_long_message_t*)tmp_in_data;
		SYS_LOG_INF("app_event 0x%x, len:%d\n", msg->cmd_id, msg->cmd_len);

		if (msg->cmd_id == TWS_EVENT_SYNC_SNOOP_SWITCH_INFO) {
			bt_manager_tws_proc_snoop_switch_sync_info((tmp_in_data + sizeof(tws_long_message_t)), msg->cmd_len);
		} else if (tws_long_msg_cb_func) {
			tws_long_msg_cb_func(msg->cmd_id, (tmp_in_data + sizeof(tws_long_message_t)), msg->cmd_len);
		}
	}
		break;
	default:
		break;
	}
	mem_free(tmp_in_data);
}


int btmgr_tws_event_through_upper(uint8_t *data, uint8_t len)
{
	uint8_t event = data[0];
	int ret = -1;

	switch (event)
	{
		case TWS_LONG_MSG_EVENT:
		{
			//tws_long_message_t* msg = (tws_long_message_t*)data;
			tws_long_message_t msg;
			memcpy(&msg,data,sizeof(tws_long_message_t));
			SYS_LOG_INF("app_event 0x%x, len:%d\n", msg.cmd_id, msg.cmd_len);
			if (msg.cmd_id == TWS_EVENT_SYNC_SNOOP_SWITCH_INFO) {
				break;
			} else if (tws_long_msg_cb_func) {
				tws_long_msg_cb_func(msg.cmd_id, (data + sizeof(tws_long_message_t)), msg.cmd_len);
				ret = 0;
			}
		}
			break;
		default:
			break;
	}
	return ret;
}

void bt_manager_tws_sync_enable_lea_dir_adv_inner(void* param)
{
	struct btmgr_tws_context_t* context = btmgr_get_tws_context();

	if (btif_tws_get_dev_role() == BTSRV_TWS_MASTER)
    {
		bt_addr_le_t local_ble_addr;

		if (!context->sync_lea_broadcast_enable)
	    {
		  return;
	    }

		memset(&local_ble_addr, 0, sizeof(bt_addr_le_t));
		btif_ble_get_local_mac(&local_ble_addr);
        bt_manager_tws_send_message
        (
        	TWS_BT_MGR_EVENT, TWS_EVENT_ENABLE_LEA_DIR_ADV,
        	(uint8_t *)&local_ble_addr, sizeof(local_ble_addr)
        );
        print_buffer(&local_ble_addr,1,6,16,0);
    }
    else if (btif_tws_get_dev_role() == BTSRV_TWS_SLAVE)
    {
        bt_addr_le_t *remote_ble_addr = (bt_addr_le_t*)param;

        if (param == NULL)
            return;

		context->le_remote_addr_valid = 1;
		memcpy(&(context->remote_ble_addr),remote_ble_addr,sizeof(bt_addr_le_t));
        print_buffer(&context->remote_ble_addr,1,6,16,0);
		le_audio_dir_adv_init();
    }
}

void bt_manager_tws_sync_enable_lea_dir_adv(bool enable)
{
    struct btmgr_tws_context_t* context = btmgr_get_tws_context();

	context->sync_lea_broadcast_enable = enable;

	SYS_LOG_INF(":%d\n", enable);
	if (context->sync_lea_broadcast_enable)
	{
       bt_manager_tws_sync_enable_lea_dir_adv_inner(NULL);
	}
}

