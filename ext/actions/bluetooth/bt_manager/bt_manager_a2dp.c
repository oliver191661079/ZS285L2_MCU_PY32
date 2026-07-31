/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt manager a2dp profile.
 */
//#define SYS_LOG_NO_NEWLINE
#define SYS_LOG_DOMAIN "bt manager"

#include <os_common_api.h>

#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stream.h>
#include <sys_event.h>
#include <bt_manager.h>
#include "bt_manager_inner.h"
#include "btservice_api.h"
#ifdef CONFIG_ACT_EVENT
#include <bt_act_event_id.h>
#include <logging/log_core.h>
LOG_MODULE_DECLARE(bt_br, CONFIG_ACT_EVENT_APP_COMPILE_LEVEL);
#endif

#define A2DP_ENDPOINT_MAX		CONFIG_BT_MAX_BR_CONN
#define A2DP_DISCONNECTED_DELAY_TIME 5000
#define A2DP_SBC_MAX_BITPOOL_INDEX	5

#define A2DP_LDAC_RECONNECT_SUPPORT (0)
#define A2DP_LDAC_CONTINUOUS_CNT (50)
#define A2DP_LDAC_RECONNECT_FRAME_CNT (4)

static uint8_t a2dp_sbc_codec[] = {
	0x00,	/* BT_A2DP_AUDIO << 4 */
	0x00,	/* BT_A2DP_SBC */
	0xFF,	/* (SNK optional) 16000, 32000, (SNK mandatory)44100, 48000, mono, dual channel, stereo, join stereo */
	0xFF,	/* (SNK mandatory) Block length: 4/8/12/16, subbands:4/8, Allocation Method: SNR, Londness */
	0x02,	/* min bitpool */
#ifdef CONFIG_BT_A2DP_MAX_BITPOOL
	CONFIG_BT_A2DP_MAX_BITPOOL,	/* max bitpool */
#else
	0x35,
#endif
};

static const uint8_t a2dp_aac_codec[] = {
	0x00,	/* BT_A2DP_AUDIO << 4 */
	0x02,	/* BT_A2DP_MPEG2 */
	0xC0,	/* MPEG2 AAC LC, MPEG4 AAC LC*/
	0x01,	/* Sampling Frequecy 44100 */
	0x8F,	/* Sampling Frequecy 48000, channels 1, channels 2 */
	0x84,	/* VBR, bit rate */
	0xe2,	/* bit rate */
	0x00,	/* bit rate */
};

static const uint8_t a2dp_ldac_codec[] = {
	0x00,	/* BT_A2DP_AUDIO << 4 */
	0xFF,	/* BT_A2DP_VENDOR */
	0x2D,   /* VENDOR_ID */ 	
	0x01,	/* VENDOR_ID */
	0x00,   /* VENDOR_ID */
	0x00,   /* VENDOR_ID */
	0xAA,   /* VENDOR_CODEC_ID */
	0x00,   /* VENDOR_CODEC_ID */
	0x3c,   /* Sampling Frequecy 44100 48000 96000*/
	0x07,	/* mono, dual channel, stereo */
};

static const bt_mgr_event_strmap_t bt_manager_a2dp_event_map[] =
{
    {BTSRV_A2DP_NONE_EV,                    STRINGIFY(BTSRV_A2DP_NONE_EV)},
    {BTSRV_A2DP_CONNECTED,    			    STRINGIFY(BTSRV_A2DP_CONNECTED)},
    {BTSRV_A2DP_DISCONNECTED,               STRINGIFY(BTSRV_A2DP_DISCONNECTED)},
    {BTSRV_A2DP_STREAM_STARTED,             STRINGIFY(BTSRV_A2DP_STREAM_STARTED)},
    {BTSRV_A2DP_STREAM_CHECK_STARTED, 		STRINGIFY(BTSRV_A2DP_STREAM_CHECK_STARTED)},
    {BTSRV_A2DP_STREAM_SUSPEND,             STRINGIFY(BTSRV_A2DP_STREAM_SUSPEND)},
    {BTSRV_A2DP_DATA_INDICATED,             STRINGIFY(BTSRV_A2DP_DATA_INDICATED)},
    {BTSRV_A2DP_CODEC_INFO,                 STRINGIFY(BTSRV_A2DP_CODEC_INFO)},
    {BTSRV_A2DP_GET_INIT_DELAY_REPORT,      STRINGIFY(BTSRV_A2DP_GET_INIT_DELAY_REPORT)},
    {BTSRV_A2DP_REQ_DELAY_CHECK_START,      STRINGIFY(BTSRV_A2DP_REQ_DELAY_CHECK_START)},
    {BTSRV_A2DP_DISCONNECTED_STREAM_SUSPEND,STRINGIFY(BTSRV_A2DP_DISCONNECTED_STREAM_SUSPEND)},
    {BTSRV_A2DP_STREAM_STARTED_AVRCP_PAUSED,STRINGIFY(BTSRV_A2DP_STREAM_STARTED_AVRCP_PAUSED)},
	{BTSRV_A2DP_NON_PROMPT_TONE_STARTED,	STRINGIFY(BTSRV_A2DP_NON_PROMPT_TONE_STARTED)},
};

#define BT_MANAGER_A2DP_EVENTNUM_STRS (sizeof(bt_manager_a2dp_event_map) / sizeof(bt_mgr_event_strmap_t))
const char *bt_manager_a2dp_evt2str(int num)
{
	return bt_manager_evt2str(num, BT_MANAGER_A2DP_EVENTNUM_STRS, bt_manager_a2dp_event_map);
}

static void _bt_manager_a2dp_callback(uint16_t hdl, btsrv_a2dp_event_e event, void *packet, int size)
{
    uint8_t need_change = 0;
    uint8_t update_volume_wait = 0;
	uint16_t tmp_a2dp_hdl = 0;
	struct bt_manager_context_t*  bt_manager = bt_manager_get_context();
	bt_mgr_dev_info_t* dev_info = bt_mgr_find_dev_info_by_hdl(hdl);
	btmgr_sync_ctrl_cfg_t *sync_ctrl_config = bt_manager_get_sync_ctrl_config();
	struct bt_audio_report rep;
	if (dev_info == NULL)
		return;		

    if(event != BTSRV_A2DP_DATA_INDICATED){
	    SYS_LOG_INF("0x%x %s", dev_info->hdl, bt_manager_a2dp_evt2str(event));
    }

	switch (event) {

#ifdef CONFIG_BT_PHONE_FILTER_PROMPT_TONE
	case BTSRV_A2DP_NON_PROMPT_TONE_STARTED:
#else
	case BTSRV_A2DP_STREAM_STARTED:
#endif
	case BTSRV_A2DP_STREAM_CHECK_STARTED:
	{
		dev_info->user_pause_pending = 0;

		dev_info->a2dp_stream_is_check_started = 0;
		if (event == BTSRV_A2DP_STREAM_CHECK_STARTED){
			dev_info->a2dp_stream_is_check_started = 1;
		}

        if(bt_manager_audio_current_stream() == BT_AUDIO_STREAM_NONE){
            update_volume_wait = 1;
            SYS_LOG_INF("stream none");
        }
        else{
            if(bt_manager_media_get_active_br_handle() == hdl){
                bt_manager_update_phone_volume(hdl, 1);
            }else {
				SYS_LOG_INF("active_br_handle_0 %x, hdl %x", bt_manager_media_get_active_br_handle(), hdl);
            }
        }

		dev_info->a2dp_stream_started = 1;
		dev_info->a2dp_status_playing = 1;
        dev_info->avrcp_ext_status &= ~BT_MANAGER_AVRCP_EXT_STATUS_SUSPEND;
		bt_manager_avrcp_sync_playing_vol(hdl);
#ifdef CONFIG_BT_AVRCP_GET_ID3
		bt_manager_media_get_id3_info();
#endif
		rep.id = BT_AUDIO_ENDPOINT_MUSIC;
		rep.audio_contexts = BT_AUDIO_CONTEXT_MEDIA;
	
		if (bt_manager->cur_a2dp_hdl && bt_manager->cur_a2dp_hdl != hdl)
		{
			rep.handle = bt_manager->cur_a2dp_hdl;
	#if 0
			bt_manager_media_event(BT_MEDIA_SERVER_PAUSE, (void*)&rep, sizeof(struct bt_audio_report));
			bt_manager_audio_stream_event(BT_AUDIO_STREAM_STOP, (void*)&rep, sizeof(struct bt_audio_report));
			bt_manager_audio_stream_event(BT_AUDIO_STREAM_RELEASE, (void*)&rep, sizeof(struct bt_audio_report));
	#endif
			if (sync_ctrl_config->stop_another_when_one_playing) {
				need_change = 1;
				tmp_a2dp_hdl = bt_manager->cur_a2dp_hdl;
				btif_avrcp_send_command_by_hdl(bt_manager->cur_a2dp_hdl,BTSRV_AVRCP_CMD_PAUSE);
				SYS_LOG_INF("stop_another: 0x%x\r\n", bt_manager->cur_a2dp_hdl);
				SYS_EVENT_INF(EVENT_BT_A2DP_BREAK_ANOTHER, hdl, bt_manager->cur_a2dp_hdl, os_uptime_get_32());
				//bt_manager_event_notify(BT_AUDIO_SWITCH, NULL, 0);
				bt_manager_event_notify(BT_REQ_RESTART_PLAY, NULL, 0);
			}	
		}
		SYS_EVENT_INF(EVENT_BT_A2DP_STARTED, hdl, bt_manager->cur_a2dp_hdl, os_uptime_get_32());
		bt_manager->cur_a2dp_hdl = hdl;
		bt_manager_a2dp_cancel_hfp_status(dev_info);
		rep.handle = hdl;		
		bt_manager_audio_stream_event(BT_AUDIO_STREAM_ENABLE, (void*)&rep, sizeof(struct bt_audio_report));
        if(update_volume_wait == 1){
            if(bt_manager_media_get_active_br_handle() == hdl){
                bt_manager_update_phone_volume(hdl, 1);
			}else {
				SYS_LOG_INF("active_br_handle_1 %x, hdl %x", bt_manager_media_get_active_br_handle(), hdl);
			}
        }
		bt_manager_audio_stream_event(BT_AUDIO_STREAM_START, (void*)&rep, sizeof(struct bt_audio_report));
        //bt_manager_media_event(BT_MEDIA_SERVER_PLAY, (void*)&rep, sizeof(struct bt_audio_report));
		bt_manager_check_visual_mode();
		if (need_change) {
			rep.handle = tmp_a2dp_hdl;
			//bt_manager_media_event(BT_MEDIA_SERVER_PAUSE, (void*)&rep, sizeof(struct bt_audio_report));
			bt_manager_audio_stream_event(BT_AUDIO_STREAM_STOP, (void*)&rep, sizeof(struct bt_audio_report));
			bt_manager_audio_stream_event(BT_AUDIO_STREAM_RELEASE, (void*)&rep, sizeof(struct bt_audio_report));
		}
	}
	break;
	case BTSRV_A2DP_STREAM_SUSPEND:
	case BTSRV_A2DP_DISCONNECTED_STREAM_SUSPEND:		
	{
		dev_info->user_pause_pending = 0;

		if (event == BTSRV_A2DP_DISCONNECTED_STREAM_SUSPEND)
		{
			//fix bluetooth timeout disconnect, reconnect to resume play.
			dev_info->need_resume_play = dev_info->a2dp_status_playing;
		}

		SYS_EVENT_INF(EVENT_BT_A2DP_SUSPEND, hdl, bt_manager->cur_a2dp_hdl, os_uptime_get_32());

		if (bt_manager->cur_a2dp_hdl && bt_manager->cur_a2dp_hdl != hdl) {
			rep.handle = hdl;
		    rep.id = BT_AUDIO_ENDPOINT_MUSIC;
		    rep.audio_contexts = BT_AUDIO_CONTEXT_MEDIA;
            //bt_manager_media_event(BT_MEDIA_SERVER_PAUSE, (void*)&rep, sizeof(struct bt_audio_report));
		    bt_manager_audio_stream_event(BT_AUDIO_STREAM_STOP, (void*)&rep, sizeof(struct bt_audio_report));
		//    bt_manager_audio_stream_event(BT_AUDIO_STREAM_RELEASE, (void*)&rep, sizeof(struct bt_audio_report));
			break;
		}
	
		dev_info->a2dp_stream_is_check_started = 0;	
		dev_info->a2dp_stream_started = 0;
		dev_info->a2dp_status_playing = 0;
		bt_manager->cur_a2dp_hdl = 0;

        if (dev_info->avrcp_ext_status & BT_MANAGER_AVRCP_EXT_STATUS_PLAYING) {
            dev_info->avrcp_ext_status |= BT_MANAGER_AVRCP_EXT_STATUS_SUSPEND;
        }
        else if (dev_info->avrcp_ext_status & BT_MANAGER_AVRCP_EXT_STATUS_PAUSED) {
            dev_info->avrcp_ext_status = BT_MANAGER_AVRCP_EXT_STATUS_NONE;
        }
		rep.handle = hdl;
		rep.id = BT_AUDIO_ENDPOINT_MUSIC;
		rep.audio_contexts = BT_AUDIO_CONTEXT_MEDIA;
#ifdef CONFIG_BT_A2DP_LDAC
		if (!A2DP_LDAC_RECONNECT_SUPPORT || (!dev_info->a2dp_reconnect && !dev_info->a2dp_connecting)) {
			// reconnect a2dp, not switch application.
#endif
		bt_manager_audio_stream_event(BT_AUDIO_STREAM_STOP, (void*)&rep, sizeof(struct bt_audio_report));
		bt_manager_audio_stream_event(BT_AUDIO_STREAM_RELEASE, (void*)&rep, sizeof(struct bt_audio_report));

		btmgr_sync_ctrl_cfg_t *sync_ctrl_config = bt_manager_get_sync_ctrl_config();
		bt_manager_a2dp_status_stopped(dev_info, sync_ctrl_config->a2dp_status_stopped_delay_ms);
		bt_manager_check_visual_mode();
#ifdef CONFIG_BT_A2DP_LDAC
		}
#endif
	}
	break;
	case BTSRV_A2DP_DATA_INDICATED:
	{
		static uint8_t print_cnt;
		int ret = 0;
#ifdef CONFIG_BT_A2DP_LDAC
		uint8_t frame_cnt = 0;

		if (dev_info->a2dp_reconnect) {
			break;
		}
#endif
		bt_manager_stream_pool_lock();
		io_stream_t bt_stream = bt_manager_get_stream(STREAM_TYPE_A2DP);

		if (!bt_stream) {
			bt_manager_stream_pool_unlock();
			if (print_cnt == 0) {
				SYS_LOG_INF("stream is null\n");
			}
			print_cnt++;
			break;
		}

#ifdef CONFIG_BT_A2DP_LDAC
		if (!A2DP_LDAC_RECONNECT_SUPPORT && 
				BTSRV_A2DP_LDAC == bt_manager_a2dp_get_codecid() &&
				true == bt_manager->bt_music_auracast_status) {
			bt_manager_stream_pool_unlock();
			dev_info->a2dp_reconnect = 1;
			btif_a2dp_disconnect(&dev_info->addr);
			btif_avrcp_disconnect(&dev_info->addr);
			SYS_LOG_WRN("reconnect..\n");
			break;
		}

		if (size > 0) {
			frame_cnt = (*((char *)packet));
			if (BTSRV_A2DP_LDAC == bt_manager_a2dp_get_codecid()) {
				SYS_LOG_DBG("frame_cnt %d\n",frame_cnt);
			}
		}
		if ((BTSRV_A2DP_LDAC == bt_manager_a2dp_get_codecid()) && 
			(frame_cnt >= A2DP_LDAC_RECONNECT_FRAME_CNT)) {
			dev_info->a2dp_indicated_cnt = 0;
		}
		if (A2DP_LDAC_RECONNECT_SUPPORT &&
			(BTSRV_A2DP_LDAC == bt_manager_a2dp_get_codecid()) && 
			(0 == dev_info->a2dp_reconnect) && 
			(frame_cnt < A2DP_LDAC_RECONNECT_FRAME_CNT) &&
			(0 == dev_info->a2dp_reconnect_complete) &&
			(true == bt_manager->bt_music_auracast_status)
			/*(0 == dev_info->a2dp_reconnect_disable)*/) {
			dev_info->a2dp_indicated_cnt++;
			#if 0
			if (os_uptime_get_32() - dev_info->a2dp_connect_time < 5000) {
				if (dev_info->a2dp_reconnect_complete) {
					bt_manager_stream_pool_unlock();
					dev_info->a2dp_reconnect_disable = 1;
					SYS_LOG_WRN("ldac a2dp_reconnect_disable..\n");
					break;
				}
			} else 
			#endif
			if (dev_info->a2dp_indicated_cnt > A2DP_LDAC_CONTINUOUS_CNT) {
				if ((0 != dev_info->addr.val[0]) || (0 != dev_info->addr.val[5])) {
					// For auracast mode, LDAC high bitrate will not allowed. 
					bt_manager_stream_pool_unlock();
					dev_info->a2dp_reconnect = 1;
					btif_a2dp_disconnect(&dev_info->addr);
					SYS_LOG_WRN("ldac reconnect..\n");
					break;
				}
			} else {
				bt_manager_stream_pool_unlock();
				SYS_LOG_WRN("drop 990 packet..\n");
				break;
			}
		}
#endif
		ret = stream_write(bt_stream, packet, size);
		if (ret != size) {
			if (print_cnt == 0) {
				SYS_LOG_WRN("write %d error %d\n", size, ret);
			}
			print_cnt++;
			bt_manager_stream_pool_unlock();
			break;
		}
		bt_manager_stream_pool_unlock();
		print_cnt = 0;
		break;
	}
	case BTSRV_A2DP_CODEC_INFO:
	{
		struct bt_audio_codec codec;
		uint8_t *codec_info = (uint8_t *)packet;
		dev_info->a2dp_codec_type = codec_info[0];
		dev_info->a2dp_sample_khz = codec_info[1];

		if (codec_info[0] == BTSRV_A2DP_SBC) { /*BT_A2DP_SBC*/
			codec.format = BT_AUDIO_CODEC_SBC;
		} else if (codec_info[0] == BTSRV_A2DP_MPEG2) { /*BT_A2DP_MPEG2*/
			codec.format = BT_AUDIO_CODEC_AAC;
		} else if (codec_info[0] == BTSRV_A2DP_LDAC) { /*BT_A2DP_LDAC*/
			codec.format = BT_AUDIO_CODEC_LDAC;
		} else {
			SYS_LOG_INF("stream unknow codec %d\n",codec_info[0]);
			codec.format = BT_AUDIO_CODEC_SBC;
		}
		codec.handle = hdl;
		codec.sample_rate = codec_info[1];
		codec.dir = BT_AUDIO_DIR_SINK;
		codec.id = BT_AUDIO_ENDPOINT_MUSIC;
		SYS_LOG_INF("stream %x config %d %d\n",hdl,codec.format,codec.sample_rate);
		SYS_EVENT_INF(EVENT_BT_A2DP_CONFIG_CODEC, hdl, codec.format, codec.sample_rate, os_uptime_get_32());
		bt_manager_audio_stream_event(BT_AUDIO_STREAM_CONFIG_CODEC, (void*)&codec, sizeof(struct bt_audio_codec));
		break;
	}
	case BTSRV_A2DP_CONNECTED:
		dev_info->a2dp_connect_time = os_uptime_get_32();
		dev_info->a2dp_connected = 1;
#ifdef CONFIG_BT_A2DP_LDAC
		dev_info->a2dp_reconnect = 0;
		if (dev_info->a2dp_connecting) {
			os_delayed_work_cancel(&dev_info->a2dp_reconnect_delay_work);
			dev_info->a2dp_reconnect_complete = 1;
			// play
			os_delayed_work_cancel(&dev_info->resume_play_delay_work);
			os_delayed_work_submit(&dev_info->resume_play_delay_work, 1000);
		}
		dev_info->a2dp_connecting = 0;
#endif
		break;
	case BTSRV_A2DP_DISCONNECTED:
	{
		if (!dev_info->a2dp_connected)
		{
			SYS_LOG_INF("a2dp is not connected");		
			break;
		}

#ifdef CONFIG_BT_A2DP_LDAC
		//dev_info->a2dp_reconnect_disable = 0;
		dev_info->a2dp_reconnect_complete = 0;
		dev_info->a2dp_connecting = 0;
		dev_info->a2dp_indicated_cnt = 0;
		if (dev_info->a2dp_reconnect) {
			dev_info->a2dp_reconnect = 0;
			if ((0 != dev_info->addr.val[0]) || (0 != dev_info->addr.val[5])) {
				if (!A2DP_LDAC_RECONNECT_SUPPORT) {
					bt_manager_a2dp_halt_ldac(true);
				}

				dev_info->a2dp_connecting = 1;
				SYS_LOG_WRN("reconnecting.\n");
				btif_a2dp_connect(0, &dev_info->addr);
				btif_avrcp_connect(&dev_info->addr);
				if (A2DP_LDAC_RECONNECT_SUPPORT) {
					os_delayed_work_submit(&dev_info->a2dp_reconnect_delay_work, 5000);
				}
				break;
			}
		}
#endif

        os_delayed_work_submit(&dev_info->profile_disconnected_delay_work, ACL_DISCONNECTED_DELAY_TIME);

		if (!dev_info->need_resume_play)
		{
			dev_info->need_resume_play = dev_info->a2dp_status_playing;
		}
	
		dev_info->a2dp_connected = 0;
		dev_info->a2dp_stream_started = 0;
		dev_info->a2dp_status_playing = 0;
		dev_info->a2dp_stream_is_check_started = 0;
		dev_info->a2dp_connect_time = 0;

		if (btif_tws_get_dev_role() == BTSRV_TWS_SLAVE) 
		{
			break;
		}

		uint16_t active_hdl = btif_a2dp_get_active_hdl();
		if (active_hdl == hdl)
		{
			 btif_a2dp_user_pause(hdl);
		}
	}
		break;
	case BTSRV_A2DP_GET_INIT_DELAY_REPORT:
	{
		uint16_t *deplay_report = (uint16_t *)packet;

		/* initialize delay report, unit(1/10ms), can't block thread */
		*deplay_report = system_check_low_latencey_mode() ? 700 : 2000;
		break;
	}
	case BTSRV_A2DP_REQ_DELAY_CHECK_START:
		bt_manager_a2dp_status_stopped(dev_info, 0);
		break;
	case BTSRV_A2DP_STREAM_STARTED_AVRCP_PAUSED:
	{
		struct bt_media_report rep;
		rep.handle = hdl;
		bt_manager_media_event(BT_MEDIA_PAUSE, &rep, sizeof(rep));
	}
		break;
		
	default:
		break;
	}
}

void bt_manager_a2dp_set_sbc_max_bitpool(uint8_t max_bitpool)
{
	a2dp_sbc_codec[A2DP_SBC_MAX_BITPOOL_INDEX] = max_bitpool;
}

int bt_manager_a2dp_profile_start(void)
{
	struct btsrv_a2dp_start_param param;

	memset(&param, 0, sizeof(param));
	param.cb = &_bt_manager_a2dp_callback;
	param.sbc_codec = (uint8_t *)a2dp_sbc_codec;
	param.sbc_endpoint_num = A2DP_ENDPOINT_MAX;
	if (bt_manager_config_support_a2dp_aac()) {
		param.aac_codec = (uint8_t *)a2dp_aac_codec;
		param.aac_endpoint_num = A2DP_ENDPOINT_MAX;
	}
	if (bt_manager_config_support_a2dp_ldac()) {
		param.ldac_codec = (uint8_t *)a2dp_ldac_codec;
		param.ldac_endpoint_num = A2DP_ENDPOINT_MAX;
	}
	param.a2dp_cp_scms_t = 0;
	param.a2dp_delay_report = 1;

	return btif_a2dp_start((struct btsrv_a2dp_start_param *)&param);
}

int bt_manager_a2dp_profile_stop(void)
{
	return btif_a2dp_stop();
}

int bt_manager_a2dp_check_state(void)
{
	return btif_a2dp_check_state();
}

int bt_manager_a2dp_send_delay_report(uint16_t delay_time)
{
	return btif_a2dp_send_delay_report(delay_time);
}

int bt_manager_a2dp_halt_aac(bool halt)
{
	SYS_LOG_INF(" %d\n", halt);
	return btif_a2dp_halt_aac(halt);
}

int bt_manager_a2dp_halt_ldac(bool halt)
{
	SYS_LOG_INF(" %d\n", halt);
	return btif_a2dp_halt_ldac(halt);
}

char *bt_manager_a2dp_get_dev_name(void)
{
	bt_mgr_dev_info_t* a2dp_dev = bt_mgr_get_a2dp_active_dev();
	if (a2dp_dev) {
		return a2dp_dev->name;
	} else {
		return NULL;
	}
}

char *bt_manager_a2dp_get_last_connected_dev_name(void)
{
	bt_mgr_dev_info_t* a2dp_dev = bt_mgr_get_last_a2dp_connected_dev();
	if (a2dp_dev) {
		return a2dp_dev->name;
	} else {
		return NULL;
	}
}

int bt_manager_a2dp_get_codecid(void)
{
	uint8_t codec_id = 0;

	bt_mgr_dev_info_t* a2dp_dev = bt_mgr_get_a2dp_active_dev();
	if (a2dp_dev)
		codec_id = a2dp_dev->a2dp_codec_type;

//	SYS_LOG_INF("codec_id %d\n", codec_id);
	return codec_id;
}

int bt_manager_a2dp_get_sample_rate(void)
{
	uint8_t sample_rate = 0;

	bt_mgr_dev_info_t* a2dp_dev = bt_mgr_get_a2dp_active_dev();
	if (a2dp_dev)
		sample_rate = a2dp_dev->a2dp_sample_khz;

	SYS_LOG_INF("sample_rate %d\n", sample_rate);
	return sample_rate ? sample_rate : 44;	/* Avoid return 0, default value 44 */
}


int bt_manager_a2dp_get_status(void)
{
	bt_mgr_dev_info_t* a2dp_dev = bt_mgr_get_a2dp_active_dev();
	if (a2dp_dev)
	{
		if ((a2dp_dev->avrcp_ext_status & BT_MANAGER_AVRCP_EXT_STATUS_PAUSED)
			&& a2dp_dev->a2dp_stream_is_check_started)
		{
			SYS_LOG_INF("phone has been paused");
			return BT_STATUS_PAUSED;
		}
	
		return (a2dp_dev->a2dp_status_playing?BT_STATUS_PLAYING:BT_STATUS_PAUSED);
	}
	
	return BT_STATUS_PAUSED;
}



/* A2DP ֹͣ����״̬
 */
void bt_manager_a2dp_status_stopped(bt_mgr_dev_info_t* dev_info, uint16_t delay_check_ms)
{
	if (btif_tws_get_dev_role() == BTSRV_TWS_SLAVE) 
	{
		return;
	}

	os_delayed_work_cancel(&dev_info->a2dp_stop_delay_work);

	os_delayed_work_submit(&dev_info->a2dp_stop_delay_work, delay_check_ms + 50);
}


/* A2DP ֹͣ�����ӳٴ���
 */
void bt_manager_a2dp_stop_delay_proc(os_work *work)
{
	bt_mgr_dev_info_t* dev_info = 
		CONTAINER_OF(work, bt_mgr_dev_info_t, a2dp_stop_delay_work);

	if (!dev_info)
		return;

	os_delayed_work_cancel(&dev_info->a2dp_stop_delay_work);

    /* ��ʱ�����ֿ�ʼ����?
     * һЩ�ֻ����л�������ʱ���ܻ����յ���ͣ״̬�������յ�����״̬?
     */
    struct bt_mgr_dev_info *a2dp_dev_info = bt_mgr_get_a2dp_active_dev();
    if (!a2dp_dev_info)
    {
        return;
    }

	struct bt_manager_context_t*  bt_manager = bt_manager_get_context();
    if ((a2dp_dev_info->a2dp_status_playing) && (bt_manager->cur_a2dp_hdl == a2dp_dev_info->hdl))
    {
		SYS_LOG_INF("0x%x a2dp_status_playing", a2dp_dev_info->hdl);
		return;
    }

    SYS_LOG_INF("0x%x", dev_info->hdl);
    bt_manager_a2dp_check_state();
}

void bt_manager_auto_reconnect_resume_play(uint16_t hdl)
{
	struct bt_mgr_dev_info *dev_info;
	btmgr_reconnect_cfg_t *reconnect_config = bt_manager_get_reconnect_config();

	dev_info = bt_mgr_find_dev_info_by_hdl(hdl);
	if (!dev_info)
	{
		return;
	}

	if (!dev_info->timeout_disconnected && !dev_info->halt_phone) 
	{
		dev_info->need_resume_play = 0;
		return;
	}

	dev_info->timeout_disconnected = 0;
	dev_info->halt_phone = 0;

	if (!dev_info->need_resume_play)
		return;

	dev_info->need_resume_play = 0;

	if (reconnect_config->reconnect_resume_play == 0) {
		return;
	}

	SYS_LOG_WRN("ready resume play");
	
	os_delayed_work_cancel(&dev_info->resume_play_delay_work);
	os_delayed_work_submit(&dev_info->resume_play_delay_work, 1000);
}


void bt_manager_resume_play_proc(os_work *work)
{
	bt_mgr_dev_info_t* dev_info = 
		CONTAINER_OF(work, bt_mgr_dev_info_t, resume_play_delay_work);

	if (!dev_info)
		return;

	SYS_LOG_WRN("hdl:0x%x,%d,%d",dev_info->hdl,dev_info->a2dp_connected,dev_info->avrcp_connected);
	if (!dev_info->a2dp_connected || !dev_info->avrcp_connected)
	{
		return;
	}

	if (btif_tws_get_dev_role() == BTSRV_TWS_SLAVE) 
	{
		return;
	}

	if (dev_info->a2dp_status_playing) 
	{
		return;
	}

	uint16_t hdl = btif_a2dp_get_active_hdl();	
	if (dev_info->hdl != hdl)
	{
		struct bt_mgr_dev_info *active_dev_info;	
		active_dev_info = bt_mgr_find_dev_info_by_hdl(hdl);

		if (active_dev_info)
		{
			SYS_LOG_WRN("a2dp active:0x%x ,%d",hdl,active_dev_info->a2dp_status_playing);		
			if (active_dev_info->a2dp_status_playing)
			{
				return;
			}
		}
	}
	btif_avrcp_send_command_by_hdl(dev_info->hdl,BTSRV_AVRCP_CMD_PLAY);
}



void bt_manager_a2dp_cancel_hfp_status(bt_mgr_dev_info_t* dev_info)
{
#ifdef CONFIG_BT_HFP_HF
    struct bt_call_report rep;
    
    if (btif_tws_get_dev_role() == BTSRV_TWS_SLAVE) 
    {
        return;
    }
	
    bool need_switch = false;

    if (dev_info->sco_connected)
    {
        if (bt_manager_hfp_get_status() <= BT_STATUS_HFP_NONE)
        {
            SYS_LOG_WRN("CANCEL_SCO");
            bt_manager_hfp_disconnect_sco(dev_info->hdl);
            need_switch = true;
        }
    }
    if (bt_manager_hfp_get_status() >= BT_STATUS_OUTGOING && bt_manager_hfp_get_status() != BT_STATUS_SIRI)
    {
        bt_mgr_dev_info_t* hfp_dev = bt_mgr_get_hfp_active_dev();
	
        if ((hfp_dev) && (hfp_dev->hdl == dev_info->hdl) && (!dev_info->sco_connected))
        {
            SYS_LOG_WRN("CANCEL_HFP");
            bt_manager_hfp_set_status(BT_STATUS_HFP_NONE);
            need_switch = true;			
        }
        else if((!hfp_dev) && (!dev_info->sco_connected)) 
        {
            SYS_LOG_WRN("CANCEL_HFP 2");
            bt_manager_hfp_set_status(BT_STATUS_HFP_NONE);
            need_switch = true;		
        }
    }

    if (bt_manager_hfp_get_status() == BT_STATUS_SIRI)
    {
        if (dev_info->sco_connected)
        {
            SYS_LOG_WRN("CANCEL_VASSIST");
            bt_manager_hfp_stop_siri();     
            bt_manager_hfp_disconnect_sco(dev_info->hdl);
            need_switch = true;	
        }
        else
        {
            bt_mgr_dev_info_t* hfp_dev = bt_mgr_get_hfp_active_dev();			
            if ((!hfp_dev) || ((hfp_dev) && (!hfp_dev->sco_connected)))
            {
                SYS_LOG_WRN("CANCEL_VASSIST 2");
				bt_manager_hfp_stop_siri();
                bt_manager_hfp_set_status(BT_STATUS_HFP_NONE);
                need_switch = true; 
            }
        }
    }

	if (need_switch) {
		rep.handle = dev_info->hdl;
		rep.index = BT_AUDIO_ENDPOINT_CALL;
		bt_manager_call_event(BT_CALL_ENDED,(void*)&rep,sizeof(rep));
	}
#endif	
}

void bt_manager_a2dp_reconnect_delay_proc(os_work *work)
{
	bt_mgr_dev_info_t* dev_info =
		CONTAINER_OF(work, bt_mgr_dev_info_t, a2dp_reconnect_delay_work);

	struct bt_audio_report rep;
	SYS_LOG_WRN("a2dp reconnect fail");

	if (!dev_info) {
		return;
	}

	if (!dev_info->a2dp_connecting) {
		return;
	}

	dev_info->a2dp_connecting = 0;

	SYS_LOG_WRN("a2dp not connect %d.", dev_info->a2dp_connected);
	if (0 == dev_info->a2dp_connected) {
		rep.handle = dev_info->hdl;
		rep.id = BT_AUDIO_ENDPOINT_MUSIC;
		rep.audio_contexts = BT_AUDIO_CONTEXT_MEDIA;
		bt_manager_audio_stream_event(BT_AUDIO_STREAM_STOP, (void*)&rep, sizeof(struct bt_audio_report));
		bt_manager_audio_stream_event(BT_AUDIO_STREAM_RELEASE, (void*)&rep, sizeof(struct bt_audio_report));
		bt_manager_a2dp_status_stopped(dev_info, 0);
		bt_manager_check_visual_mode();
	}
}

void bt_manager_a2dp_auracast_status_set(bool status)
{
	struct bt_manager_context_t*  bt_manager = bt_manager_get_context();

	bt_manager->bt_music_auracast_status = status;
}

bool bt_manager_a2dp_reconnect_status_get(void)
{
	struct bt_manager_context_t*  bt_manager = bt_manager_get_context();
	int i;

	for (i = 0; i < MAX_MGR_DEV; i++) {
		if (bt_manager->dev[i].used && 
			0 == bt_manager->dev[i].a2dp_reconnect &&
			1 == bt_manager->dev[i].a2dp_connecting) {
			return true;
		}
	}

	return false;
}

