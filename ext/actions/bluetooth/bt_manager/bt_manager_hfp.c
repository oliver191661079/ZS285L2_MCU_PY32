/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt manager a2dp profile.
 */
#define SYS_LOG_DOMAIN "bt manager"

#include <os_common_api.h>
#include <media_type.h>
#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stream.h>
#include <bt_manager.h>
#include <power_manager.h>
#include <app_manager.h>
#include <sys_event.h>
#include <mem_manager.h>
#include "bt_manager_inner.h"
#include <assert.h>
#include "btservice_api.h"
#include <audio_policy.h>
#include <audio_system.h>
#include <volume_manager.h>
#include <alarm_manager.h>

enum {
	SIRI_INIT_STATE,
	SIRI_START_STATE,
	SIRI_RUNNING_STATE,
	SIRI_STOP_STATE,
};

struct bt_manager_hfp_info_t {
	uint8_t siri_mode:3;
	uint8_t only_sco:1;
	uint8_t allow_sco:1;
	uint16_t hfp_status;
	uint32_t sco_connect_time;
	uint32_t send_atcmd_time;
};

union bt_manager_sync_info_t {
	struct {
		uint8_t siri_mode:3;
		uint8_t only_sco:1;
		uint16_t hfp_status;
	};
	int value;
};

static struct bt_manager_hfp_info_t hfp_manager;

static const bt_mgr_event_strmap_t bt_manager_hfp_event_map[] =
{
    {BTSRV_HFP_NONE_EV,                     STRINGIFY(BTSRV_HFP_NONE_EV)},
    {BTSRV_HFP_CONNECTED,    			    STRINGIFY(BTSRV_HFP_CONNECTED)},
    {BTSRV_HFP_DISCONNECTED,                STRINGIFY(BTSRV_HFP_DISCONNECTED)},
    {BTSRV_HFP_CODEC_INFO,                  STRINGIFY(BTSRV_HFP_CODEC_INFO)},
    {BTSRV_HFP_PHONE_NUM,                   STRINGIFY(BTSRV_HFP_PHONE_NUM)},
    {BTSRV_HFP_PHONE_NUM_STOP,              STRINGIFY(BTSRV_HFP_PHONE_NUM_STOP)},
	{BTSRV_HFP_CALL_INCOMING,               STRINGIFY(BTSRV_HFP_CALL_INCOMING)},
    {BTSRV_HFP_CALL_OUTGOING,               STRINGIFY(BTSRV_HFP_CALL_OUTGOING)},
    {BTSRV_HFP_CALL_3WAYIN,                 STRINGIFY(BTSRV_HFP_CALL_3WAYIN)},
    {BTSRV_HFP_CALL_ONGOING,                STRINGIFY(BTSRV_HFP_CALL_ONGOING)},
    {BTSRV_HFP_CALL_MULTIPARTY,             STRINGIFY(BTSRV_HFP_CALL_MULTIPARTY)},
	{BTSRV_HFP_CALL_ALERTED,                STRINGIFY(BTSRV_HFP_CALL_ALERTED)},
    {BTSRV_HFP_CALL_EXIT,                   STRINGIFY(BTSRV_HFP_CALL_EXIT)},
	{BTSRV_HFP_VOLUME_CHANGE,               STRINGIFY(BTSRV_HFP_VOLUME_CHANGE)},
    {BTSRV_HFP_ACTIVE_DEV_CHANGE,           STRINGIFY(BTSRV_HFP_ACTIVE_DEV_CHANGE)},
    {BTSRV_HFP_SIRI_STATE_CHANGE,           STRINGIFY(BTSRV_HFP_SIRI_STATE_CHANGE)},
    {BTSRV_HFP_SCO,                         STRINGIFY(BTSRV_HFP_SCO)},
    {BTSRV_SCO_CONNECTED,                   STRINGIFY(BTSRV_SCO_CONNECTED)},
	{BTSRV_SCO_DISCONNECTED,                STRINGIFY(BTSRV_SCO_DISCONNECTED)},
    {BTSRV_SCO_DATA_INDICATED,              STRINGIFY(BTSRV_SCO_DATA_INDICATED)},
    {BTSRV_HFP_TIME_UPDATE,                 STRINGIFY(BTSRV_HFP_TIME_UPDATE)},
    {BTSRV_HFP_AT_CMD,                      STRINGIFY(BTSRV_HFP_AT_CMD)},
    {BTSRV_HFP_GET_BTMGR_STATE,             STRINGIFY(BTSRV_HFP_GET_BTMGR_STATE)},
    {BTSRV_HFP_SET_BTMGR_STATE,             STRINGIFY(BTSRV_HFP_SET_BTMGR_STATE)},
	{BTSRV_HFP_CALL_3WAYOUT, 				STRINGIFY(BTSRV_HFP_CALL_3WAYOUT)},
};

#define BT_MANAGER_HFP_EVENTNUM_STRS (sizeof(bt_manager_hfp_event_map) / sizeof(bt_mgr_event_strmap_t))
const char *bt_manager_hfp_evt2str(int num)
{
	return bt_manager_evt2str(num, BT_MANAGER_HFP_EVENTNUM_STRS, bt_manager_hfp_event_map);
}

static uint32_t _bt_manager_call_to_hfp_volume(uint32_t call_vol)
{
	uint32_t  hfp_vol = 0;
#ifdef CONFIG_AUDIO
	if (call_vol == 0) {
		hfp_vol = 0;
	} else if (call_vol >= audio_policy_get_volume_level()) {
		hfp_vol = 0x0F;
	} else {
		hfp_vol = (call_vol * 0x10 / audio_policy_get_volume_level());
		if (hfp_vol == 0) {
			hfp_vol = 1;
		}
	}
#endif
	return hfp_vol;
}
#ifdef CONFIG_VOLUME_MANAGER
static uint32_t _bt_manager_hfp_to_call_volume(uint32_t hfp_vol)
{
	uint32_t  call_vol = 0;
#ifdef CONFIG_AUDIO
	if (hfp_vol == 0) {
		call_vol = 0;
	} else if (hfp_vol >= 0x0F) {
		call_vol = audio_policy_get_volume_level();
	} else {
		call_vol = (hfp_vol + 1) * audio_policy_get_volume_level() / 0x10;

		if (call_vol == 0) {
	        call_vol = 1;
		}
	}
#endif
	return call_vol;
}
#endif



#if 0
static void bt_manager_hfp_status_changed(bt_mgr_dev_info_t* dev_info)
{
	if (btif_tws_get_dev_role() == BTSRV_TWS_SLAVE)
	{
		return;
	}
	os_delayed_work_cancel(&dev_info->sco_disconnect_delay_work);

	if (dev_info->sco_connected)
	{
		os_delayed_work_submit(&dev_info->sco_disconnect_delay_work, 3000);
	}
}
#endif

void bt_manager_hfp_sco_disconnect_proc(os_work *work)
{
    bt_mgr_dev_info_t* dev_info =
		CONTAINER_OF(work, bt_mgr_dev_info_t, sco_disconnect_delay_work);

    if (!dev_info)
        return;

    os_delayed_work_cancel(&dev_info->a2dp_stop_delay_work);

    if (hfp_manager.hfp_status > BT_STATUS_HFP_NONE)
    {
        return;
    }

    if (!dev_info->sco_connected)
    {
        SYS_LOG_INF("0x%x sco disconnect", dev_info->hdl);
        return;
    }

    SYS_LOG_ERR("0x%x", dev_info->hdl);
    bt_manager_hfp_disconnect_sco(dev_info->hdl);
}

int bt_manager_hfp_set_status(uint32_t state)
{
	hfp_manager.hfp_status = state;
	return 0;
}

int bt_manager_hfp_get_status(void)
{
	return hfp_manager.hfp_status;
}

uint8_t string_to_num(uint8_t c){
	if(c >= '0' && c <= '9'){
		return c - '0';
	}else
		return 0;
}

//format like "20/11/30, 18:58:09"
//https://m2msupport.net/m2msupport/atcclk-clock/
int _bt_hfp_set_time(uint8_t *time)
{
#if WAIT_TODO
	SYS_LOG_INF("time %s\n", time);

	struct rtc_time tm;
	tm.tm_year = 2000 + string_to_num(time[0])*10 + string_to_num(time[1]);
	tm.tm_mon = string_to_num(time[3])*10 + string_to_num(time[4]);
	tm.tm_mday = string_to_num(time[6])*10 + string_to_num(time[7]);

	if(time[9] == ' '){
		tm.tm_hour = string_to_num(time[10])*10 + string_to_num(time[11]);
		tm.tm_min = string_to_num(time[13])*10 + string_to_num(time[14]);
		tm.tm_sec = string_to_num(time[16])*10 + string_to_num(time[17]);
	}else if(time[9] >= '0' && time[9] <= '9'){
		tm.tm_hour = string_to_num(time[9])*10 + string_to_num(time[10]);
		tm.tm_min = string_to_num(time[12])*10 + string_to_num(time[13]);
		tm.tm_sec = string_to_num(time[15])*10 + string_to_num(time[16]);
	}
#ifdef CONFIG_ALARM_MANAGER
	int ret = alarm_manager_set_time(&tm);
	if (ret) {
		SYS_LOG_ERR("set time error ret=%d\n", ret);
		return -1;
	}
#endif
#endif
	return 0;
}

static int btmgr_hfp_sync_cb_get_state(void)
{
	union bt_manager_sync_info_t info;

	info.siri_mode = hfp_manager.siri_mode;
	info.only_sco = hfp_manager.only_sco;
	info.hfp_status = (uint16_t)bt_manager_hfp_get_status();

	return info.value;
}

/* Wait TODO: Tws sync switch app */
static void btmgr_hfp_sync_cb_set_state(uint16_t hdl, void *param)
{
	union bt_manager_sync_info_t info;
	int event_param;

    memcpy((uint8 *)&event_param, (uint8 *)param, 4);
	//info.value = *(int *)(param);
	info.value = event_param;
	SYS_LOG_INF("0x%08x\n", event_param);
	SYS_LOG_INF("Hfp sync cb %d %d 0x%x\n", info.siri_mode, info.only_sco, info.hfp_status);

	hfp_manager.siri_mode = info.siri_mode;
	hfp_manager.only_sco = info.only_sco;
	bt_manager_hfp_set_status(info.hfp_status);

	struct bt_call_report rep;
	rep.handle = hdl;
	rep.index = 1;

	switch (info.hfp_status) {
	case BT_STATUS_HFP_NONE:
		bt_manager_call_event(BT_CALL_ENDED, (void*)&rep, sizeof(struct bt_call_report));
		break;
	case BT_STATUS_INCOMING:
		bt_manager_call_event(BT_CALL_INCOMING, (void*)&rep, sizeof(struct bt_call_report));
		break;
	case BT_STATUS_OUTGOING:
		bt_manager_call_event(BT_CALL_DIALING, (void*)&rep, sizeof(struct bt_call_report));
		break;
	case BT_STATUS_ONGOING:
		bt_manager_call_event(BT_CALL_ACTIVE, (void*)&rep, sizeof(struct bt_call_report));
		break;
	case BT_STATUS_MULTIPARTY:
		bt_manager_call_event(BT_CALL_MULTIPARTY, (void*)&rep, sizeof(struct bt_call_report));
		break;
	case BT_STATUS_SIRI:
		bt_manager_call_event(BT_CALL_SIRI_MODE, (void*)&rep, sizeof(struct bt_call_report));
		break;
	case BT_STATUS_3WAYIN:
		bt_manager_call_event(BT_CALL_3WAYIN, (void*)&rep, sizeof(struct bt_call_report));
		break;
	case BT_STATUS_3WAYOUT:
		bt_manager_call_event(BT_CALL_3WAYOUT, (void*)&rep, sizeof(struct bt_call_report));
		break;
	default:
		break;
	}
}

static int _bt_manager_hfp_callback(uint16_t hdl, btsrv_hfp_event_e event, uint8_t *param, int param_size)
{
	int ret = 0;
	bt_mgr_dev_info_t* dev_info = bt_mgr_find_dev_info_by_hdl(hdl);
	struct bt_call_report rep;
	struct bt_audio_report audio_rep;


	if (dev_info == NULL) {
		SYS_LOG_ERR("WARN!");
		return ret;
	}

	if(event != BTSRV_SCO_DATA_INDICATED){
	    SYS_LOG_INF("hdl: 0x%x event: %s",dev_info->hdl,bt_manager_hfp_evt2str(event));
	}

	switch (event) {
	case BTSRV_HFP_CONNECTED:
	{
		SYS_LOG_INF("hfp connected\n");

		if (btif_tws_get_dev_role() != BTSRV_TWS_SLAVE) {
#ifdef CONFIG_POWER
			bt_manager_hfp_battery_report(BT_BATTERY_REPORT_INIT, 0);
			bt_manager_hfp_battery_report(BT_BATTERY_REPORT_VAL, power_manager_get_battery_capacity());
#endif
			bt_manager_hfp_get_time();
		}

		//to do :get dev num which connect hfp
		if (bt_manager_get_connected_dev_num() == 1) {
			bt_manager_call_event(BT_CALL_CONNECTED, (void*)&hdl, sizeof(hdl));
		}
		break;
	}
	case BTSRV_HFP_PHONE_NUM:
	{
		rep.handle = hdl;
		rep.index = 1;
		rep.uri = param;
		bt_manager_call_event(BT_CALL_RING_STATR_EVENT,(void*)&rep,sizeof(rep));
		SYS_LOG_INF("hfp phone num %s size %d\n", param, param_size);
		break;
	}
	case BTSRV_HFP_PHONE_NUM_STOP:
	{
		rep.handle = hdl;
		rep.index = 1;
		rep.uri = param;
		bt_manager_call_event(BT_CALL_RING_STOP_EVENT,(void*)&rep,sizeof(rep));
		SYS_LOG_INF("hfp phone stop\n");
		break;
	}
	case BTSRV_HFP_DISCONNECTED:
	{
	    if (dev_info->hf_connected) {
            os_delayed_work_submit(&dev_info->profile_disconnected_delay_work, ACL_DISCONNECTED_DELAY_TIME);
        }
		if (bt_manager_get_connected_dev_num() == 1) {
			bt_manager_call_event(BT_CALL_DISCONNECTED, (void*)&hdl, sizeof(hdl));
		}
		break;
	}
	case BTSRV_HFP_CODEC_INFO:
	{
		uint8_t hfp_codec_id = param[0];
		uint8_t hfp_sample_rate = param[1];
		SYS_LOG_INF("codec_id %d sample_rate %d\n", hfp_codec_id, hfp_sample_rate);
		bt_manager_sco_set_codec_info(hdl, hfp_codec_id,hfp_sample_rate);
		break;
	}
	case BTSRV_HFP_CALL_INCOMING:
	{
		rep.handle = hdl;
		rep.index = 1;
		bt_manager_hfp_sync_origin_vol(hdl);
		bt_manager_hfp_set_status(BT_STATUS_INCOMING);

		if (btif_tws_get_dev_role() != BTSRV_TWS_SLAVE)
		{
		   audio_rep.handle = hdl;
		   audio_rep.id = BT_AUDIO_ENDPOINT_CALL;
		   audio_rep.audio_contexts = BT_AUDIO_CONTEXT_CONVERSATIONAL;
		   bt_manager_audio_stream_event(BT_AUDIO_STREAM_FAKE, (void*)&audio_rep,  sizeof(struct bt_audio_report));
		}
		hfp_manager.only_sco = 0;
		bt_manager_call_event(BT_CALL_INCOMING, (void*)&rep, sizeof(struct bt_call_report));
		break;
	}
	case BTSRV_HFP_CALL_OUTGOING:
	{
		rep.handle = hdl;
		rep.index = 1;
		bt_manager_hfp_sync_origin_vol(hdl);
		bt_manager_hfp_set_status(BT_STATUS_OUTGOING);

		if (btif_tws_get_dev_role() != BTSRV_TWS_SLAVE)
		{
		   audio_rep.handle = hdl;
		   audio_rep.id = BT_AUDIO_ENDPOINT_CALL;
		   audio_rep.audio_contexts = BT_AUDIO_CONTEXT_CONVERSATIONAL;
		   bt_manager_audio_stream_event(BT_AUDIO_STREAM_FAKE, (void*)&audio_rep, sizeof(struct bt_audio_report));
		}
		hfp_manager.only_sco = 0;
		bt_manager_call_event(BT_CALL_DIALING, (void*)&rep, sizeof(struct bt_call_report));
		break;
	}
	case BTSRV_HFP_CALL_ALERTED:
	{
		rep.handle = hdl;
		rep.index = 1;
		bt_manager_hfp_sync_origin_vol(hdl);
		bt_manager_hfp_set_status(BT_STATUS_OUTGOING);

		hfp_manager.only_sco = 0;
		bt_manager_call_event(BT_CALL_ALERTING,(void*)&rep, sizeof(struct bt_call_report));
		break;
	}

	case BTSRV_HFP_CALL_ONGOING:
	{
		rep.handle = hdl;
		rep.index = 1;
		bt_manager_hfp_set_status(BT_STATUS_ONGOING);

		hfp_manager.only_sco = 0;
		bt_manager_call_event(BT_CALL_ACTIVE, (void*)&rep, sizeof(struct bt_call_report));
		break;
	}

	case BTSRV_HFP_CALL_3WAYIN:
	{
		rep.handle = hdl;
		rep.index = 1;
		bt_manager_hfp_sync_origin_vol(hdl);
		bt_manager_hfp_set_status(BT_STATUS_3WAYIN);
		hfp_manager.only_sco = 0;
		bt_manager_call_event(BT_CALL_3WAYIN, (void*)&rep, sizeof(struct bt_call_report));
		break;
	}

	case BTSRV_HFP_CALL_MULTIPARTY:
	{
		rep.handle = hdl;
		rep.index = 1;
		bt_manager_hfp_sync_origin_vol(hdl);
		bt_manager_hfp_set_status(BT_STATUS_MULTIPARTY);
		hfp_manager.only_sco = 0;
		bt_manager_call_event(BT_CALL_MULTIPARTY, (void*)&rep, sizeof(struct bt_call_report));
		break;
	}
	case BTSRV_HFP_CALL_3WAYOUT:
	{
		rep.handle = hdl;
		rep.index = 1;
		bt_manager_hfp_sync_origin_vol(hdl);
		bt_manager_hfp_set_status(BT_STATUS_3WAYOUT);
		hfp_manager.only_sco = 0;
		bt_manager_call_event(BT_CALL_3WAYOUT, (void*)&rep, sizeof(struct bt_call_report));
		break;
	}

	case BTSRV_HFP_CALL_EXIT:
	{
		rep.handle = hdl;
		rep.index = 1;
		bt_manager_hfp_set_status(BT_STATUS_HFP_NONE);
		bt_manager_call_event(BT_CALL_ENDED, (void*)&rep, sizeof(struct bt_call_report));

        if (btif_tws_get_dev_role() != BTSRV_TWS_SLAVE)
		{
		   audio_rep.handle = hdl;
		   audio_rep.id = BT_AUDIO_ENDPOINT_CALL;
	  	   audio_rep.audio_contexts = 0;
		   bt_manager_audio_stream_event(BT_AUDIO_STREAM_FAKE, (void*)&audio_rep,	sizeof(struct bt_audio_report));
		}
		break;
	}

	case BTSRV_HFP_SCO:
	{
		if (hfp_manager.allow_sco) {
			hfp_manager.only_sco = 1;

		    if (btif_tws_get_dev_role() != BTSRV_TWS_SLAVE)
			{
			   audio_rep.handle = hdl;
			   audio_rep.id = BT_AUDIO_ENDPOINT_CALL;
			   audio_rep.audio_contexts = BT_AUDIO_CONTEXT_CONVERSATIONAL;
			   bt_manager_audio_stream_event(BT_AUDIO_STREAM_FAKE, (void*)&audio_rep, sizeof(struct bt_audio_report));
			}
			bt_manager_update_phone_volume(hdl,0);
		}

		rep.handle = hdl;
		rep.index = 1;

		if (hfp_manager.siri_mode == SIRI_START_STATE) {
			hfp_manager.siri_mode = SIRI_RUNNING_STATE;
            bt_manager_call_event(BT_CALL_SIRI_MODE, (void*)&rep, sizeof(struct bt_call_report));
		}

		if (hfp_manager.siri_mode == SIRI_RUNNING_STATE) {
			bt_manager_hfp_set_status(BT_STATUS_SIRI);
			bt_manager_call_event(BT_CALL_SIRI_MODE, (void*)&rep, sizeof(struct bt_call_report));
		}
		break;
	}

	case BTSRV_HFP_VOLUME_CHANGE:
	{
		uint32_t volume = *(uint8_t *)(param);

		SYS_LOG_INF("hfp volume change %d\n", volume);
		uint32_t pass_time = os_uptime_get_32() - hfp_manager.sco_connect_time;
		/* some phone may send volume sync event after sco connect. */
		/* we must drop these event to avoid volume conflict */
		/* To Do: do we have better solution?? */
		if ((hfp_manager.sco_connect_time && (pass_time < 500))
			|| (bt_manager_audio_current_stream() != BT_AUDIO_STREAM_BR_CALL)) {
			SYS_LOG_INF("drop volume sync event from phone\n");
			return ret;
		}
	#ifdef CONFIG_VOLUME_MANAGER
		uint32_t call_vol = _bt_manager_hfp_to_call_volume(volume);
		bt_manager_hfp_sync_vol_to_local(hdl, call_vol);
	#endif
		break;
	}

	case BTSRV_HFP_SIRI_STATE_CHANGE:
	{
		uint32_t state = *(uint8_t *)(param);
		rep.handle = hdl;
		rep.index = 1;
		SYS_LOG_INF("hfp siri state %d\n", state);
		if (state == HFP_SIRI_STATE_ACTIVE) {
			hfp_manager.siri_mode = SIRI_RUNNING_STATE;
			bt_manager_sys_event_notify(SYS_EVENT_SIRI_START);
		} else if (state == HFP_SIRI_STATE_SEND_ACTIVE) {
			hfp_manager.siri_mode = SIRI_START_STATE;
			/* start siri cmd send OK,just play tts */
			bt_manager_sys_event_notify(SYS_EVENT_SIRI_START);
		} else if (state == HFP_SIRI_STATE_DEACTIVE) {
			hfp_manager.siri_mode = SIRI_INIT_STATE;
			bt_manager_sys_event_notify(SYS_EVENT_SIRI_STOP);
			bt_manager_call_event(BT_CALL_ENDED, (void*)&rep, sizeof(struct bt_call_report));
			if (btif_tws_get_dev_role() != BTSRV_TWS_SLAVE)
			{
			   audio_rep.handle = hdl;
			   audio_rep.id = BT_AUDIO_ENDPOINT_CALL;
		  	   audio_rep.audio_contexts = 0;
			   bt_manager_audio_stream_event(BT_AUDIO_STREAM_FAKE, (void*)&audio_rep,	sizeof(struct bt_audio_report));
			}
		}
		if (hfp_manager.siri_mode == SIRI_RUNNING_STATE){
			bt_manager_hfp_set_status(BT_STATUS_SIRI);
			bt_manager_call_event(BT_CALL_SIRI_MODE, (void*)&rep, sizeof(struct bt_call_report));
		} else if (hfp_manager.siri_mode == SIRI_INIT_STATE) {
			if (bt_manager_hfp_get_status() == BT_STATUS_SIRI){
				bt_manager_hfp_set_status(BT_STATUS_HFP_NONE);
			}
		}
		break;
	}

	case BTSRV_SCO_CONNECTED:
	{
		dev_info->sco_connected = 1;
		SYS_LOG_INF("hfp %d, %d", bt_manager_hfp_get_call_state(1),bt_manager_hfp_get_status());

		bt_manager_hfp_sync_origin_vol(hdl);
		hfp_manager.sco_connect_time = os_uptime_get_32();

		audio_rep.handle = hdl;
		audio_rep.id = BT_AUDIO_ENDPOINT_CALL;
		audio_rep.audio_contexts = BT_AUDIO_CONTEXT_CONVERSATIONAL;

		if (btif_tws_get_dev_role() != BTSRV_TWS_SLAVE)
		{
		  bt_manager_audio_stream_event(BT_AUDIO_STREAM_FAKE, (void*)&audio_rep, sizeof(struct bt_audio_report));
		  bt_manager_audio_stream_event(BT_AUDIO_STREAM_ENABLE,(void*)&audio_rep, sizeof(struct bt_audio_report));
		  bt_manager_audio_stream_event(BT_AUDIO_STREAM_START,(void*)&audio_rep, sizeof(struct bt_audio_report));
		}
		bt_manager_check_visual_mode();
		break;
	}
	case BTSRV_SCO_DISCONNECTED:
	{
		audio_rep.handle = hdl;
		audio_rep.id = BT_AUDIO_ENDPOINT_CALL;
		audio_rep.audio_contexts = BT_AUDIO_CONTEXT_CONVERSATIONAL;
		SYS_LOG_INF("hfp sco disconnected %x\n",hdl);
		hfp_manager.sco_connect_time = 0;

		if (btif_tws_get_dev_role() != BTSRV_TWS_SLAVE)
		{
		  bt_manager_audio_stream_event(BT_AUDIO_STREAM_STOP,(void*)&audio_rep, sizeof(struct bt_audio_report));
		  bt_manager_audio_stream_event(BT_AUDIO_STREAM_RELEASE,(void*)&audio_rep, sizeof(struct bt_audio_report));
		}
		/**hfp_manager.only_sco == 0 means sco disconnect by switch sound source, not exit btcall app */
		if (hfp_manager.only_sco && (bt_manager_audio_current_stream() == BT_AUDIO_STREAM_BR_CALL)) {
			hfp_manager.only_sco = 0;
#if 0
			app_switch_unlock(1);
			app_switch(NULL, APP_SWITCH_LAST, false);
#endif
		}
		if (hfp_manager.siri_mode != SIRI_INIT_STATE) {
			SYS_LOG_INF("hfp state:%d", bt_manager_hfp_get_status());
			if (bt_manager_hfp_get_status() == BT_STATUS_SIRI){
				bt_manager_hfp_set_status(BT_STATUS_HFP_NONE);
			}
			hfp_manager.siri_mode = SIRI_INIT_STATE;
			rep.handle = hdl;
			rep.index = 1;
			bt_manager_call_event(BT_CALL_ENDED, (void*)&rep, sizeof(struct bt_call_report));
			if (btif_tws_get_dev_role() != BTSRV_TWS_SLAVE)
			{
			   audio_rep.handle = hdl;
			   audio_rep.id = BT_AUDIO_ENDPOINT_CALL;
		  	   audio_rep.audio_contexts = 0;
			   bt_manager_audio_stream_event(BT_AUDIO_STREAM_FAKE, (void*)&audio_rep,	sizeof(struct bt_audio_report));
			}
		}
		bt_manager_check_visual_mode();
		dev_info->sco_connected = 0;
		break;
	}
	case BTSRV_HFP_ACTIVE_DEV_CHANGE:
	{
		if (btif_tws_get_dev_role() != BTSRV_TWS_SLAVE)
		{
		   audio_rep.handle = hdl;
		   audio_rep.id = BT_AUDIO_ENDPOINT_CALL;
		   audio_rep.audio_contexts = 0;
		   bt_manager_audio_stream_event(BT_AUDIO_STREAM_FAKE, (void*)&audio_rep,	sizeof(struct bt_audio_report));
		   audio_rep.audio_contexts = BT_AUDIO_CONTEXT_CONVERSATIONAL;

		   bt_manager_audio_stream_event(BT_AUDIO_STREAM_STOP,(void*)&audio_rep, sizeof(struct bt_audio_report));
		   bt_manager_audio_stream_event(BT_AUDIO_STREAM_RELEASE,(void*)&audio_rep, sizeof(struct bt_audio_report));
		}
	    SYS_LOG_INF("hfp dev changed %x\n",hdl);
		break;
	}
	case BTSRV_HFP_TIME_UPDATE:
	{
		SYS_LOG_INF("hfp time update\n");
		_bt_hfp_set_time(param);
		break;
	}
	case BTSRV_HFP_GET_BTMGR_STATE:
		ret = btmgr_hfp_sync_cb_get_state();
		break;
	case BTSRV_HFP_SET_BTMGR_STATE:
		btmgr_hfp_sync_cb_set_state(hdl, param);
		break;
	default:
		break;
	}

	return ret;
}

int bt_manager_hfp_profile_start(void)
{
	return btif_hfp_start(&_bt_manager_hfp_callback);
}

int bt_manager_hfp_profile_stop(void)
{
	return btif_hfp_stop();
}

int bt_manager_hfp_get_call_state(uint8_t active_call)
{
	return btif_hfp_hf_get_call_state(active_call);
}

int bt_manager_hfp_check_atcmd_time(void)
{
	uint32_t pass_time = os_uptime_get_32() - hfp_manager.send_atcmd_time;

	if (hfp_manager.send_atcmd_time && (pass_time < 600)) {
		return 0;
	}
	hfp_manager.send_atcmd_time = os_uptime_get_32();
	return 1;
}

int bt_manager_hfp_dial_number(uint8_t *number)
{
	if (!bt_manager_hfp_check_atcmd_time()) {
		return -1;
	}
	return btif_hfp_hf_dial_number(number);
}

int bt_manager_hfp_dial_last_number(void)
{
	if (!bt_manager_hfp_check_atcmd_time()) {
		return -1;
	}
	return btif_hfp_hf_dial_last_number();
}

int bt_manager_hfp_dial_memory(int location)
{
	if (!bt_manager_hfp_check_atcmd_time()) {
		return -1;
	}
	return btif_hfp_hf_dial_memory(location);
}

int bt_manager_hfp_volume_control(uint8_t type, uint8_t volume)
{
#ifdef CONFIG_BT_HFP_HF_VOL_SYNC
	return btif_hfp_hf_volume_control(type, volume);
#else
	return 0;
#endif
}

int bt_manager_hfp_battery_report(uint8_t mode, uint8_t bat_val)
{
	if (mode == BT_BATTERY_REPORT_VAL) {
		bat_val = bat_val / 10;
		if (bat_val > 9) {
			bat_val = 9;
		}
	}
	SYS_LOG_INF("mode %d , bat_val %d\n", mode, bat_val);
	return btif_hfp_hf_battery_report(mode, bat_val);
}

int bt_manager_hfp_accept_call(void)
{
	return btif_hfp_hf_accept_call();
}

int bt_manager_hfp_reject_call(void)
{
	return btif_hfp_hf_reject_call();
}

int bt_manager_hfp_hangup_call(void)
{
	return btif_hfp_hf_hangup_call();
}

int bt_manager_hfp_hangup_another_call(void)
{
	if (!bt_manager_hfp_check_atcmd_time()) {
		return -1;
	}
	return btif_hfp_hf_hangup_another_call();
}

int bt_manager_hfp_holdcur_answer_call(void)
{
	if (!bt_manager_hfp_check_atcmd_time()) {
		return -1;
	}
	return btif_hfp_hf_holdcur_answer_call();
}

int bt_manager_hfp_hangupcur_answer_call(void)
{
	if (!bt_manager_hfp_check_atcmd_time()) {
		return -1;
	}
	return btif_hfp_hf_hangupcur_answer_call();
}

int bt_manager_hfp_start_siri(void)
{
	int ret = 0;

	if (!bt_manager_hfp_check_atcmd_time()) {
		return -1;
	}
	if (hfp_manager.siri_mode != SIRI_RUNNING_STATE) {
		ret = btif_hfp_hf_voice_recognition_start();
		hfp_manager.siri_mode = SIRI_START_STATE;
	}

	return ret;
}

int bt_manager_hfp_stop_siri(void)
{
	int ret = 0;

	if (!bt_manager_hfp_check_atcmd_time()) {
		return -1;
	}
	if (hfp_manager.siri_mode != SIRI_INIT_STATE) {
		sys_event_notify(SYS_EVENT_SIRI_STOP);
		ret = btif_hfp_hf_voice_recognition_stop();
		hfp_manager.siri_mode = SIRI_STOP_STATE;
	}
	return ret;
}

int bt_manager_hfp_switch_sound_source(void)
{
	if (!bt_manager_hfp_check_atcmd_time()) {
		return -1;
	}
	return btif_hfp_hf_switch_sound_source();
}

int bt_manager_hfp_get_time(void)
{
	return btif_hfp_hf_get_time();
}

int bt_manager_allow_sco_connect(bool allowed)
{
	hfp_manager.allow_sco = allowed ? 1 : 0;
	return btif_br_allow_sco_connect(allowed);
}

int bt_manager_hfp_send_at_command(uint8_t *command,uint8_t active_call)
{
	//if (!bt_manager_hfp_check_atcmd_time()) {
	//	return -1;
	//}
	return btif_hfp_hf_send_at_command(command,active_call);
}

int bt_manager_hfp_sync_vol_to_remote_by_addr(uint16_t hdl, uint32_t call_vol)
{
    uint32_t  hfp_vol;
#if 1
    btmgr_sync_ctrl_cfg_t * cfg =  bt_manager_get_sync_ctrl_config();
    bt_mgr_dev_info_t* dev_info;
    if (hdl)
	{
        dev_info = bt_mgr_find_dev_info_by_hdl(hdl);
    }
	else
	{
        dev_info = bt_mgr_get_hfp_active_dev();
    }

    if ((!dev_info) ||
		(dev_info->is_tws) ||
		(btif_tws_get_dev_role() == BTSRV_TWS_SLAVE))
    {
        return 0;
    }

    if (!dev_info->hf_connected)
    {
        SYS_LOG_INF("dev no hf_connected\n");
        return 0;
    }

 #if 1
    if (cfg->hfp_origin_volume_sync_to_remote &&
		k_work_pending(&dev_info->hfp_vol_to_remote_timer.work))
    {
        SYS_LOG_INF("hfp vol to remote pending\n");
        return 0;
    }
#endif
    if (call_vol <= audio_policy_get_volume_level())
    {
        dev_info->bt_call_vol = call_vol;
    }
    else
    {
        dev_info->bt_call_vol = audio_system_get_stream_volume(AUDIO_STREAM_VOICE);
    }

    hfp_vol = _bt_manager_call_to_hfp_volume(call_vol);
    if (dev_info->hfp_remote_vol == hfp_vol)
    {
        return 0;
    }

    SYS_LOG_INF("hdl:0x%x,call_vol:%d, hfp_vol:%d",dev_info->hdl,call_vol,hfp_vol);
    dev_info->hfp_remote_vol = hfp_vol;
    bt_manager_save_dev_volume();
    return bt_manager_hfp_volume_control(1, (uint8_t)hfp_vol);
#else
    if (btif_tws_get_dev_role() != BTSRV_TWS_SLAVE) {
        hfp_vol = _bt_manager_call_to_hfp_volume(call_vol);
        return bt_manager_hfp_volume_control(1, (uint8_t)hfp_vol);
    } else {
        return 0;
    }
#endif
}

int bt_manager_hfp_sync_vol_to_remote(uint32_t call_vol)
{
	return bt_manager_hfp_sync_vol_to_remote_by_addr(0, call_vol);
}


void bt_manager_hfp_sync_vol_to_local(uint16_t hdl, uint8_t call_vol)
{
#ifdef CONFIG_VOLUME_MANAGER
  #if 0
    system_volume_sync_remote_to_device(AUDIO_STREAM_VOICE, call_vol);
  #else
    btmgr_sync_ctrl_cfg_t * cfg =  bt_manager_get_sync_ctrl_config();
    bt_mgr_dev_info_t* dev_info = bt_mgr_find_dev_info_by_hdl(hdl);
    uint8_t  hfp_vol;
    bt_mgr_dev_info_t * hfp_active_dev = bt_mgr_get_hfp_active_dev();
    bool notify_app = true;

    if ((!dev_info) ||
		(dev_info->is_tws) ||
		(btif_tws_get_dev_role() == BTSRV_TWS_SLAVE))
    {
        return;
    }

    hfp_vol = (uint8_t)_bt_manager_call_to_hfp_volume(call_vol);
    dev_info->hfp_remote_vol = hfp_vol;

#if 1
    if (cfg->hfp_origin_volume_sync_to_remote &&
		k_work_pending(&dev_info->hfp_vol_to_remote_timer.work))
    {
        SYS_LOG_INF("hfp vol to remote pending\n");
        return;
    }
#endif

    dev_info->bt_call_vol = call_vol;
    bt_manager_save_dev_volume();
    SYS_LOG_INF("call_vol:%d, hfp_vol:%d",call_vol,hfp_vol);

    /* no current HFP active device
     */
    if (hfp_active_dev && dev_info->hdl != hfp_active_dev->hdl)
    {
		SYS_LOG_INF("no active hfp dev\n");
        notify_app = false;
    }

    bt_manager_sync_volume_from_phone(&dev_info->addr, false, call_vol, false, notify_app);
  #endif
#endif
}

void bt_manager_hfp_sync_origin_vol(uint16_t hdl)
{
#ifdef CONFIG_VOLUME_MANAGER
  #if 0
    bt_manager_hfp_sync_vol_to_remote(audio_system_get_stream_volume(AUDIO_STREAM_VOICE));
  #else
    btmgr_sync_ctrl_cfg_t * cfg =  bt_manager_get_sync_ctrl_config();
    bt_mgr_dev_info_t* dev_info = bt_mgr_find_dev_info_by_hdl(hdl);

    if ((!dev_info) ||
		(dev_info->is_tws) ||
		(btif_tws_get_dev_role() == BTSRV_TWS_SLAVE))
    {
        return;
    }

    if (cfg->hfp_origin_volume_sync_to_remote  == 0)
    {
        return;
    }
	SYS_LOG_INF("hdl:0x%x, bt_call_vol:%d\n",hdl, dev_info->bt_call_vol);

	os_delayed_work_cancel(&dev_info->hfp_vol_to_remote_timer);
	os_delayed_work_submit(&dev_info->hfp_vol_to_remote_timer, cfg->hfp_origin_volume_sync_delay_ms);

  #endif
#endif
}

void bt_manager_hfp_sync_origin_vol_proc(os_work *work)
{
	bt_mgr_dev_info_t* dev_info =
		CONTAINER_OF(work, bt_mgr_dev_info_t, hfp_vol_to_remote_timer);

	if (!dev_info)
		return;

	SYS_LOG_INF("dev_info->hdl:0x%x,%d \n",dev_info->hdl,dev_info->bt_call_vol);

    bt_manager_hfp_sync_vol_to_remote_by_addr(dev_info->hdl, dev_info->bt_call_vol);
}

void bt_manager_hfp_get_active_conn_handle(uint16_t *handle)
{
	btif_hfp_get_active_conn_handle(handle);
}

int bt_manager_hfp_init(void)
{
	memset(&hfp_manager, 0, sizeof(struct bt_manager_hfp_info_t));
	hfp_manager.allow_sco = 1;
	return 0;
}

