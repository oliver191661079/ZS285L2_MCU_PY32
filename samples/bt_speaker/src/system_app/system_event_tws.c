/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <mem_manager.h>
#include <msg_manager.h>
#include <sys_event.h>
#include <audio_system.h>
#include <volume_manager.h>
#include <app_manager.h>
#ifdef CONFIG_PLAYTTS
#include <tts_manager.h>
#endif
#ifdef CONFIG_BLUETOOTH
#include <bt_manager.h>
#endif

#include "app_ui.h"
#include "app_tws.h"
#include "app_defines.h"
#include "system_app.h"
#include "media_player.h"

#ifdef CONFIG_TWS_UI_EVENT_SYNC

#define NOTIFY_SYNC_WAIT_MS (100)
#define APP_WAIT_TWS_READY_SEND_SYNC_TIMEOUT 500	/* 500ms */

static bool is_tts_finish = false;

static void system_do_clear_notify(void);

static bool system_is_local_event(u32_t ui_event)
{
	bool ret = false;

	switch (ui_event) {
	case UI_EVENT_POWER_OFF:
	case UI_EVENT_POWER_ON:
	case UI_EVENT_STANDBY:
	case UI_EVENT_WAKE_UP:
	case UI_EVENT_TWS_START_TEAM:
		ret = true;
		break;

		/* BT status LED display could be different for TWS master and slave */
	case UI_EVENT_ENTER_PAIRING:
	case UI_EVENT_WAIT_CONNECTION:
	case UI_EVENT_CONNECT_SUCCESS:
	case UI_EVENT_TWS_TEAM_SUCCESS:
		ret = false;
		break;

	default:
		break;
	}
	return ret;
}

bool system_need_sync_event(u32_t ui_event)
{
	bool ret = true;
	if (system_is_local_event(ui_event) ||
	    (bt_manager_tws_get_dev_role() != BTSRV_TWS_MASTER)) {
		ret = false;
	}
	return ret;
}

static int system_notify_tws_send(int receiver, int event, void *param, int len,
				  bool sync_wait)
{
	int sender = bt_manager_tws_get_dev_role();

	if ((sender != BTSRV_TWS_NONE) && (sender != receiver)) {
		bt_manager_tws_send_message(TWS_USER_APP_EVENT, event, param,
					    len);
		return 0;
	}

	return -1;
}

static void system_notify_voice_tws(uint8_t event_type, uint8_t stage,
				    uint64_t bt_clk)
{
	SYS_LOG_INF("event_type:%d, event_stage:%d, time:%u_%u",
		    event_type,
		    stage,
		    (uint32_t) ((bt_clk >> 32) & 0xffffffff),
		    (uint32_t) (bt_clk & 0xffffffff));

	notify_sync_t notify_event;
	notify_event.event_type = event_type;
	notify_event.event_stage = stage;
	notify_event.sync_time = bt_clk;
	system_notify_tws_send(BTSRV_TWS_SLAVE, TWS_EVENT_SYS_NOTIFY,
			       &notify_event, sizeof(notify_event), false);
}

bool system_notify_is_end(system_notify_ctrl_t * notify_ctrl)
{
	//Support tts play only right now
#if 0
	if (notify_ctrl->flags.end_led_display &&
	    notify_ctrl->flags.end_tone_play &&
	    notify_ctrl->flags.end_voice_play) {
		return true;
	}
#else
	if (notify_ctrl->flags.end_voice_play) {
		return true;
	}
#endif

	return false;
}

void system_notify_finish(system_notify_ctrl_t * notify_ctrl)
{
	SYS_LOG_INF("event:%d", notify_ctrl->Event_Type);
	mem_free(notify_ctrl);
}

static void _system_tts_event_nodify(u8_t * tts_id, u32_t event)
{
	switch (event) {
	case TTS_EVENT_START_PLAY:
		SYS_LOG_INF("%s start", tts_id);
		break;
	case TTS_EVENT_STOP_PLAY:
		SYS_LOG_INF("%s end", tts_id);
		SYS_LOG_INF("tts_finish %d set true", is_tts_finish);
		is_tts_finish = true;
		break;
	default:
		break;
	}
}

void system_notify_voice_play(system_notify_ctrl_t * notify_ctrl)
{
	system_app_context_t *manager = system_app_get_context();
	uint64_t bt_clk = 0;

	if (notify_ctrl->flags.disable_voice) {
		SYS_LOG_INF("voice is disabled");
		notify_ctrl->flags.end_voice_play = true;
		return;
	}

	if (!notify_ctrl->flags.start_voice_play) {
		if (manager->sys_status.disable_audio_voice) {
			SYS_LOG_INF("%d", __LINE__);
			notify_ctrl->flags.end_voice_play = true;
			return;
		}

		notify_ctrl->flags.start_voice_play = true;

		/*
		 * check tws sysc play voice time.
		 */
		SYS_LOG_INF("event %d", notify_ctrl->Event_Type);
		if (notify_ctrl->Event_Type == UI_EVENT_POWER_OFF) {
			tts_manager_wait_finished(true);
			SYS_LOG_INF("wait finish end..");
		}

		if (notify_ctrl->flags.is_tws_mode &&
		    system_need_sync_event(notify_ctrl->Event_Type)) {
			tws_runtime_observer_t *observer =
			    btif_tws_get_tws_observer();
			bt_manager_wait_tws_ready_send_sync_msg
			    (APP_WAIT_TWS_READY_SEND_SYNC_TIMEOUT);
			bt_clk = observer->get_bt_clk_us();
			if (bt_clk != UINT64_MAX) {
				bt_clk += NOTIFY_SYNC_WAIT_MS * 1000;
			}
			system_notify_voice_tws(notify_ctrl->Event_Type,
						START_PLAY_VOICE, bt_clk);
		} else {
			bt_clk = 0;
		}

		tts_manager_add_event_lisener(_system_tts_event_nodify);
		if (0 !=
		    tts_manager_process_ui_event_at_time(notify_ctrl->
							 Event_Type, bt_clk)) {
			SYS_LOG_INF("tts_finish %d set true", is_tts_finish);
			is_tts_finish = true;
		} else {
			SYS_LOG_INF("tts_finish %d set false", is_tts_finish);
			is_tts_finish = false;
		}
	} else if (is_tts_finish) {
		notify_ctrl->flags.end_voice_play = true;
		SYS_LOG_INF("%d, end_voice_play.", notify_ctrl->Event_Type);
	} else {
	}
}

void system_notify_wait_end(system_notify_ctrl_t * notify_ctrl)
{
#ifdef CONFIG_TEMPORARY_CLOSE_TTS
	SYS_LOG_INF("no wait.");
#else
	uint32_t start_time = os_uptime_get_32();

	SYS_LOG_INF("%d", __LINE__);
	while (1) {
		if (system_notify_is_end(notify_ctrl)
		    || (os_uptime_get_32() - start_time > 8000)) {
			SYS_LOG_INF("");
			break;
		}
		thread_timer_handle_expired();
		os_sleep(10);
	}
	SYS_LOG_INF("%d", __LINE__);
#endif
}

void system_notify_timer_handler(struct thread_timer *ttimer,
				 void *expiry_fn_arg)
{
	system_notify_ctrl_t *notify_ctrl;
	system_app_context_t *manager = system_app_get_context();

	/*
	 * all led notify has set to led ctrl
	 */
	if (list_empty(&manager->notify_ctrl_list)) {
		SYS_LOG_INF("notify_ctrl_list empty!");
		if (thread_timer_is_running(&manager->notify_ctrl_timer))
			thread_timer_stop(&manager->notify_ctrl_timer);
		return;
	}

	notify_ctrl =
	    list_first_entry(&manager->notify_ctrl_list, system_notify_ctrl_t,
			     node);
	if (NULL == notify_ctrl) {
		SYS_LOG_INF("NULL notify ctrl!");
		return;
	}

	SYS_LOG_DBG("event=%d, nest=%d, start=%d, end=%d",
		    notify_ctrl->Event_Type,
		    notify_ctrl->nesting,
		    notify_ctrl->flags.start_voice_play,
		    notify_ctrl->flags.end_voice_play);
	notify_ctrl->nesting += 1;
	if (!notify_ctrl->flags.end_voice_play) {
		system_notify_voice_play(notify_ctrl);
	}
	notify_ctrl->nesting -= 1;

	if (system_notify_is_end(notify_ctrl)) {
		SYS_LOG_INF("Event %d end", notify_ctrl->Event_Type);
		/*
		 * delete from notify list.
		 */
		if (!notify_ctrl->flags.list_deleted) {
			notify_ctrl->flags.list_deleted = true;

			list_del(&notify_ctrl->node);
		}

		/*
		 * should not release when in nesting
		 */
		if (notify_ctrl->nesting > 0) {
			return;
		}

		if (!notify_ctrl->flags.need_wait_end) {
			system_notify_finish(notify_ctrl);
		}
	}
}

static void system_do_clear_notify(void)
{
	system_app_context_t *manager = system_app_get_context();

	system_notify_ctrl_t *notify_ctrl;

	SYS_LOG_INF("start");

	if (manager->notify_ctrl_list.next == NULL) {
		return;
	}

	if (list_empty(&manager->notify_ctrl_list)) {
		return;
	}

	list_for_each_entry(notify_ctrl, &manager->notify_ctrl_list, node) {
		if (notify_ctrl->Event_Type == UI_EVENT_TWS_START_TEAM &&
		    bt_manager_tws_get_dev_role() != BTSRV_TWS_NONE) {
			/* clear UI_EVENT_TWS_START_TEAM when TWS already connected */
		} else if (system_is_local_event(notify_ctrl->Event_Type)) {
			continue;
		}

		if ((!notify_ctrl->flags.set_led_display) &&
		    (!notify_ctrl->flags.set_tone_play) &&
		    (!notify_ctrl->flags.set_voice_play)
		    ) {
			SYS_LOG_INF("%s: %d",
				    (notify_ctrl->flags.
				     led_disp_only) ? "led_disp" : "ui_event",
				    notify_ctrl->Event_Type);

			notify_ctrl->flags.end_led_display = true;
			notify_ctrl->flags.end_tone_play = true;
			notify_ctrl->flags.end_voice_play = true;
		}
	}
	SYS_LOG_INF("end");

}

/* 
  * stop notify
 */
void system_do_stop_notify(u32_t ui_event)
{
	system_app_context_t *manager = system_app_get_context();

	system_notify_ctrl_t *notify_ctrl;

	//SYS_LOG_INF("ui_event:%d", ui_event);

	if (manager->notify_ctrl_list.next == NULL) {
		return;
	}

	if (list_empty(&manager->notify_ctrl_list)) {
		return;
	}

	list_for_each_entry(notify_ctrl, &manager->notify_ctrl_list, node) {
		if (notify_ctrl->flags.led_disp_only) {
			continue;
		}

		if (notify_ctrl->Event_Type != ui_event &&
		    ui_event != (u8_t) - 1) {
			continue;
		}

		notify_ctrl->flags.end_led_display = true;
		notify_ctrl->flags.end_tone_play = true;
		notify_ctrl->flags.end_voice_play = true;

	}

}

void system_do_event_notify(u32_t ui_event)
{
	system_app_context_t *manager = system_app_get_context();
	system_notify_ctrl_t *notify_ctrl = NULL;
	bool ignore = false;

	SYS_LOG_INF("event %d", ui_event);

	if ((bt_manager_tws_get_dev_role() == BTSRV_TWS_SLAVE) &&
	    (!system_is_local_event(ui_event))) {
		SYS_LOG_INF("skip event\n");
		if (ui_event == UI_EVENT_TWS_TEAM_SUCCESS) {
			system_do_clear_notify();
		}
		return;
	}

	/* check repeated event notify */
	if (manager->notify_ctrl_list.next != NULL) {
		list_for_each_entry(notify_ctrl, &manager->notify_ctrl_list,
				    node) {
			if ((notify_ctrl->Event_Type == ui_event)
			    && (notify_ctrl->Event_Type !=
				UI_EVENT_BT_DISCONNECT)
			    ) {
				SYS_LOG_WRN("REPEATED");
				return;
			}
		}
	}

	system_do_stop_notify(ui_event);

	/* 
	 * ignore all other notify event when system will poweroff
	 */
	if (manager->sys_status.in_power_off_stage) {
		if (ui_event != UI_EVENT_POWER_OFF) {
			ignore = true;
		}
	}

	if (ignore) {
		SYS_LOG_WRN("IGNORED");
		return;
	}

	SYS_LOG_INF("create a notify ctrl for event %d", ui_event);
	notify_ctrl = mem_malloc(sizeof(*notify_ctrl));
	if (NULL == notify_ctrl) {
		SYS_LOG_ERR("%d, malloc fail!", __LINE__);
		return;
	}

	memset(notify_ctrl, 0, sizeof(*notify_ctrl));

	notify_ctrl->Event_Type = ui_event;

	if (bt_manager_tws_get_dev_role() >= BTSRV_TWS_MASTER) {
		notify_ctrl->flags.is_tws_mode = true;
	}

	if (manager->sys_status.disable_audio_tone) {
		notify_ctrl->flags.disable_tone = true;
	}

	if (manager->sys_status.disable_audio_voice) {
		notify_ctrl->flags.disable_voice = true;
	}

	/*
	 * init notify_ctrl_list
	 */
	if (manager->notify_ctrl_list.next == NULL) {
		INIT_LIST_HEAD(&manager->notify_ctrl_list);
	}

	list_add_tail(&notify_ctrl->node, &manager->notify_ctrl_list);

	/*
	 * init timer
	 */
	if (!thread_timer_is_running(&manager->notify_ctrl_timer)) {
		thread_timer_init(&manager->notify_ctrl_timer,
				  system_notify_timer_handler, NULL);
		thread_timer_start(&manager->notify_ctrl_timer, 20, 20);
	}

	/*
	 * "power on" tone didn't play immediately, Avoid causing caton during Bluetooth initialization
	 */
	if (ui_event != UI_EVENT_POWER_ON) {
		system_notify_timer_handler(NULL, NULL);
	}

	/* Wait poweroff notify end
	 */
	if (ui_event == UI_EVENT_POWER_OFF) {
		system_notify_wait_end(notify_ctrl);
	}
}
#endif

#ifdef CONFIG_TWS_UI_EVENT_SYNC
void system_slave_voice_play(u8_t event, u64_t sync_time)
{
	uint64_t bt_clk = 0;

	tws_runtime_observer_t * observer = btif_tws_get_tws_observer();
	bt_clk = observer->get_bt_clk_us();
	SYS_LOG_INF("event_type:%d curr_time:%u_%u play_time:%u_%u",
		event, (uint32_t)((bt_clk >> 32) & 0xffffffff), (uint32_t)(bt_clk & 0xffffffff),
		(uint32_t)((sync_time >> 32) & 0xffffffff), (uint32_t)(sync_time & 0xffffffff));

#ifdef CONFIG_PLAYTTS
	tts_manager_process_ui_event_at_time(event, sync_time);
#endif
}

static void system_slave_notify_proc(uint8_t *slave_notify)
{
    system_app_context_t*  manager = system_app_get_context();
    notify_sync_t notify_event; //= (notify_sync_t *)slave_notify;

    if(manager->sys_status.in_power_off_stage)
    {
        SYS_LOG_INF("in_power_off_stage..");
        return;
    }

    memcpy(&notify_event, slave_notify, sizeof(notify_event));

    SYS_LOG_INF("event_type:%d, event_stage:%d, time:%u_%u", 
                notify_event.event_type, 
                notify_event.event_stage, 
                (uint32_t)((notify_event.sync_time >> 32) & 0xffffffff),
                (uint32_t)(notify_event.sync_time & 0xffffffff)
               );

    switch(notify_event.event_stage)
    {
        case START_PLAY_LED:
            break;
        case START_PLAY_TONE:
            break;
        case START_PLAY_VOICE:
			system_slave_voice_play(notify_event.event_type, notify_event.sync_time);
			break;
        default:
            break;
    }
}

int bt_manager_tws_sync_app_status_info(void* param)
{
    int dev_role = bt_manager_tws_get_dev_role();

    if (dev_role == BTSRV_TWS_MASTER)
    {
        app_tws_sync_status_info_t info;
		info.app_ver = 1;
		info.bt_music_vol = audio_system_get_stream_volume(AUDIO_STREAM_MUSIC);
		info.bt_call_vol = audio_system_get_stream_volume(AUDIO_STREAM_VOICE);
		info.auxtws_vol = audio_system_get_stream_volume(AUDIO_STREAM_SOUNDBAR);

		SYS_LOG_INF("tx: ver=%d music=%d call=%d aux=%d", 
			info.app_ver,
			info.bt_music_vol,
			info.bt_call_vol,
			info.auxtws_vol);
		bt_manager_tws_send_message(
			TWS_USER_APP_EVENT, TWS_EVENT_SYNC_STATUS_INFO,
			(uint8_t *)&info, sizeof(info));
    }
    else if(dev_role == BTSRV_TWS_SLAVE)
    {
        uin8_t audio_stream_type;

        if (param == NULL){
            return -1;
		}

        app_tws_sync_status_info_t*  info = (app_tws_sync_status_info_t*)param;
		SYS_LOG_INF("rx: ver=%d music=%d call=%d aux=%d", 
			info->app_ver,
			info->bt_music_vol,
			info->bt_call_vol,
			info->auxtws_vol);

        audio_stream_type = bt_manager_audio_current_stream();
        if (audio_stream_type == BT_AUDIO_STREAM_NONE){
            return -1;
		}

		audio_system_set_stream_volume(AUDIO_STREAM_MUSIC, info->bt_music_vol);
		audio_system_set_stream_volume(AUDIO_STREAM_VOICE, info->bt_call_vol);
		audio_system_set_stream_volume(AUDIO_STREAM_SOUNDBAR, info->auxtws_vol);
		if (audio_stream_type == BT_AUDIO_STREAM_BR_CALL) {
            system_player_volume_set(AUDIO_STREAM_VOICE, info->bt_call_vol);
        }
		if (audio_stream_type == BT_AUDIO_STREAM_BR_MUSIC) {
            system_player_volume_set(AUDIO_STREAM_MUSIC, info->bt_music_vol);
		}

		//if (audio_stream_type == BT_AUDIO_STREAM_LE_CALL) {
        ///    system_player_volume_set(AUDIO_STREAM_SOUNDBAR, info->bt_music_vol);
		//}
    }

	return 0;
}

#endif

#ifdef CONFIG_APP_TWS_SNOOP
int system_tws_event_handle(struct app_msg *msg)
{
	tws_message_t *message = (tws_message_t *)msg->ptr;
	int ret_val = 0;

	SYS_LOG_INF("id=%d, sync=%d\n", message->cmd_id, message->sync_clock);
	print_buffer_lazy("message", message->cmd_data, message->cmd_len);
	switch (message->cmd_id)
	{

#ifdef CONFIG_TWS_UI_EVENT_SYNC
	case TWS_EVENT_SYS_NOTIFY:
	{
		system_slave_notify_proc((char *)message->cmd_data);
		break;
	}
	case TWS_EVENT_SYNC_STATUS_INFO:
	{
		bt_manager_tws_sync_app_status_info(message->cmd_data);
		break;
	}
#endif

#if 0
	case TWS_EVENT_SYSTEM_KEY_CTRL:
	{
		system_key_func_proc(message->cmd_data[0], true);
		break;
	}

	case TWS_EVENT_DIAL_PHONE_NO:
	{
		bt_manager_hfp_dial_number((char *)message->cmd_data);
		break;
	}

	case TWS_EVENT_SET_VOICE_LANG:
	{
		system_tws_set_voice_language(message->cmd_data[0]);
		break;
	}

	case TWS_EVENT_KEY_TONE_PLAY:
	{
		system_tws_key_tone_play(message->cmd_data);
		break;
	}

	case TWS_EVENT_SYNC_LOW_LATENCY_MODE:
	{
		audiolcy_set_latency_mode(message->cmd_data[0], 1);
		break;
	}

	case TWS_EVENT_SYNC_CHG_BOX_STATUS:
	{
		system_tws_sync_chg_box_status(message->cmd_data);
		break;
	}

	case TWS_EVENT_IN_CHARGER_BOX_STATE:
	{
		manager->sys_status.tws_remote_in_charger_box = message->cmd_data[0];
		break;
	}
	case TWS_EVENT_AP_RECORD_STATUS:
	{
		record_slave_status_set(message->cmd_data[0]);
		break;
	}
#endif

	case TWS_EVENT_POWER_OFF:
		system_app_enter_poweroff(true);
		break;

	case TWS_EVENT_BT_MUSIC_KEY_CTRL:
	{
		struct app_msg new_msg = {0};
		u32_t key_event;

		if(message->cmd_len == 4) {
			key_event = message->cmd_data[0] 
				| (message->cmd_data[1] << 8)
				| (message->cmd_data[2] << 16)
				| (message->cmd_data[3] << 24);
			new_msg.type = MSG_KEY_INPUT;
			new_msg.value = key_event;
			send_async_msg(CONFIG_FRONT_APP_NAME, &new_msg);
			ret_val = 1;
		}
		break;
	}
	case TWS_EVENT_TWS_MODE_SWITCH:
		if(message->cmd_len > 0) {
			u8_t mode;
			mode=message->cmd_data[0];
			SYS_LOG_INF("mode switch %d", mode);
			app_tws_mode_select(mode, APP_TWS_ROLE_SECONDARY);
		}
		break;

	case TWS_EVENT_MEDIA_VERSION_INFO:
		if(message->cmd_len > 0) {
			u32_t media_version;
			media_version =message->cmd_data[0]
				| (message->cmd_data[1] << 8);

			media_player_set_pair_version(media_version);
		}
	    break;

	case TWS_EVENT_SYNC_BT_MUSIC_DAE:
	case TWS_EVENT_KEY_CTRL_COMPLETE:
	case TWS_EVENT_AVDTP_FILTER:
	case TWS_EVENT_REMOTE_KEY_PROC_BEGIN:
	case TWS_EVENT_REMOTE_KEY_PROC_END:
	case TWS_EVENT_BT_CALL_KEY_CTRL:
	case TWS_EVENT_BT_CALL_START_RING:
	case TWS_EVENT_BT_CALL_PRE_RING:
	case TWS_EVENT_BT_MUSIC_START:
	case TWS_EVENT_BT_CALL_STOP:
		SYS_LOG_INF("not supported cmd %d", message->cmd_id);
	}

	return ret_val;
}
#endif
