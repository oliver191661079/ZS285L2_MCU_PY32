/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 */

#include <bt_manager.h>
#include <thread_timer.h>
#include "list.h"
#include <ctype.h>
#include <mem_manager.h>
#include <bt_manager_ble.h>
#include <stdlib.h>
#include <property_manager.h>
#include <bt_manager_pawr.h>

#include "system_app.h"

extern bool bt_manager_config_inited;

extern uint8_t bt_manager_config_connect_phone_num(void);
extern uint16_t *bt_manager_config_get_device_id(void);
extern uint32_t bt_manager_config_bt_class(void);


typedef struct  // <"SCAN 参数设置">
{
    uint8   Scan_Mode;              // <"模式", CFG_TYPE_BT_SCAN_MODE, readonly>
    uint16  Inquiry_Scan_Window;    // <"InquiryScan 窗口">
    uint16  Inquiry_Scan_Interval;  // <"InquiryScan 间隔">
    uint8   Inquiry_Scan_Type;      // <"InquiryScan 类型", 0 ~ 1>
    uint16  Page_Scan_Window;       // <"PageScan 窗口">
    uint16  Page_Scan_Interval;     // <"PageScan 间隔">
    uint8   Page_Scan_Type;         // <"PageScan 类型", 0 ~ 1>

} CFG_Type_BT_Scan_Params;


CFG_Type_BT_Scan_Params  cfg_scan_params[7] =
{
    { BTSRV_SCAN_MODE_DEFAULT_INQUIRY_PAGE, 0x12, 0x1000, 0,  0x12, 0x600, 1 },
    { BTSRV_SCAN_MODE_FAST_PAGE,            0,    0,      0,  0x30, 0x180, 1 },
    { BTSRV_SCAN_MODE_FAST_PAGE_EX,         0,    0,      0,  0x60, 0x200, 1 },
    { BTSRV_SCAN_MODE_NORMAL_PAGE,          0x12, 0x800,  0,  0x18, 0x200, 1 },
    { BTSRV_SCAN_MODE_NORMAL_PAGE_S3,       0,    0,      0,  0x12, 0x800, 1 },
    { BTSRV_SCAN_MODE_NORMAL_PAGE_EX,       0,    0,      0,  0x60, 0x500, 1 },
    { BTSRV_SCAN_MODE_FAST_INQUIRY_PAGE,    0x60, 0x400,  1,  0x60, 0x200, 1 },

};  // <"SCAN 参数设置", CFG_Type_BT_Scan_Params>


int system_btmgr_config_update_1st(void)
{
    int ret = 0;
    int i;
    uint8_t bt_name_len = 0;

    btmgr_base_config_t *base_config = bt_manager_get_base_config();
    btmgr_feature_cfg_t *feature_config = bt_manager_get_feature_config();
	

    memset(base_config, 0, sizeof(btmgr_base_config_t));
    memset(feature_config, 0, sizeof(btmgr_feature_cfg_t));

	base_config->bt_device_class = bt_manager_config_bt_class();

    bt_name_len = MIN(strlen(CONFIG_BT_DEVICE_NAME), CONFIG_MAX_BT_NAME_LEN);
    if(bt_name_len > 0){
        memcpy(base_config->bt_device_name, CONFIG_BT_DEVICE_NAME, bt_name_len);
    }

    uint16_t *base_cfg_device_id = bt_manager_config_get_device_id();

	base_config->vendor_id =  base_cfg_device_id[1];
	base_config->product_id = base_cfg_device_id[2];
	base_config->version_id = base_cfg_device_id[3];

    base_config->ad2p_bitpool = 0x35;
    SYS_LOG_INF(":%x,%x,%x\n",base_config->vendor_id,base_config->product_id,base_config->version_id);

	base_config->link_quality.quality_pre_val	= 50;
	base_config->link_quality.quality_diff		= 50;
	base_config->link_quality.quality_esco_diff = 20;
	base_config->link_quality.quality_monitor	= 0;

	for(i=0; i< 7;i++)
	{
		base_config->scan_params[i].scan_mode = cfg_scan_params[i].Scan_Mode;
		base_config->scan_params[i].inquiry_scan_window = cfg_scan_params[i].Inquiry_Scan_Window;
		base_config->scan_params[i].inquiry_scan_interval = cfg_scan_params[i].Inquiry_Scan_Interval;		
		base_config->scan_params[i].inquiry_scan_type = cfg_scan_params[i].Inquiry_Scan_Type;
		base_config->scan_params[i].page_scan_window = cfg_scan_params[i].Page_Scan_Window;
		base_config->scan_params[i].page_scan_interval = cfg_scan_params[i].Page_Scan_Interval;
		base_config->scan_params[i].page_scan_type = cfg_scan_params[i].Page_Scan_Type;
	}
	
    //Support_Features;

	/*����֧������
	 */
#ifdef CONFIG_BT_A2DP_AAC
	feature_config->sp_avdtp_aac 	 = 1;//(BT_SUPPORT_A2DP_AAC)			 ? 1 : 0;
#endif	
	feature_config->sp_avrcp_vol_syn  = 1;//(BT_SUPPORT_AVRCP_VOLUME_SYNC)	 ? 1 : 0;

	feature_config->sp_sniff_enable	  = 1;//(BT_SUPPORT_ENABLE_SNIFF)		   ? 1 : 0;

	feature_config->sp_exit_sniff_when_bis	= 1;

	feature_config->sp_hfp_enable_nrec		  = 1;//(BT_SUPPORT_ENABLE_NREC)		   ? 1 : 0;
#ifdef CONFIG_BT_HFP_MSBC
	feature_config->sp_hfp_codec_negotiation   = 1;//(BT_SUPPORT_HFP_CODEC_NEGOTIATION) ? 1 : 0;
#endif


#ifdef CONFIG_BT_DOUBLE_PHONE
    feature_config->sp_dual_phone = 1;  
#else
    feature_config->sp_dual_phone = 0;
#endif 

#ifdef CONFIG_BT_DOUBLE_PHONE_TAKEOVER_MODE
	feature_config->sp_phone_takeover = 1;
#else
	feature_config->sp_phone_takeover = 0;
#endif

	/* Call after set sp_dual_phone and sp_phone_takeover  */
	feature_config->sp_device_num = bt_manager_config_connect_phone_num();
	
	if (feature_config->sp_tws)
	{
     feature_config->sp_device_num++; 	
	}	

    SYS_LOG_INF("sp_tws: %d :%d \n",feature_config->sp_tws,feature_config->sp_device_num);
	feature_config->enter_sniff_time 		 = 5000;//cfg_bt_mgr.Idle_Enter_Sniff_Time_Ms;// <"空闲进入 Sniff 模式时间 (毫秒)", 2000 ~ 20000, dev_mode>
	feature_config->sniff_interval			 = 500;//cfg_bt_mgr.Sniff_Interval_Ms; <"Sniff 周期 (毫秒)", 100 ~ 500, dev_mode>
	

    return ret;
}

int system_btmgr_config_update_2nd(void)
{
    int ret = 0;
//	int i;	
    btmgr_pair_cfg_t *pair_config = bt_manager_get_pair_config();
    btmgr_tws_pair_cfg_t *tws_pair_config = bt_manager_get_tws_pair_config();
    btmgr_sync_ctrl_cfg_t *sync_ctrl_config = bt_manager_get_sync_ctrl_config();
    btmgr_reconnect_cfg_t *reconnect_config = bt_manager_get_reconnect_config();
    btmgr_rf_param_cfg_t* rf_prarm_config = bt_manager_rf_param_config();
	
    memset(pair_config, 0, sizeof(btmgr_pair_cfg_t));
    memset(tws_pair_config, 0, sizeof(btmgr_tws_pair_cfg_t));
    memset(sync_ctrl_config, 0, sizeof(btmgr_sync_ctrl_cfg_t));
    memset(reconnect_config, 0, sizeof(btmgr_reconnect_cfg_t));
    memset(rf_prarm_config, 0, sizeof(btmgr_rf_param_cfg_t));

#ifdef CONFIG_BT_DISABLE_PAIR_MODE
	pair_config->default_state_discoverable = 1;//cfg_bt_pair.Default_State_Discoverable ? 1 : 0;<"默认状态可被搜索发现",      CFG_TYPE_BOOL>
	pair_config->default_state_wait_connect_sec = 0;//cfg_bt_pair.Default_State_Wait_Connect_Sec;<"默认状态等待配对连接 (秒)", 0 ~ 600, /* 设置为 0 时不限时间 */>
	pair_config->pair_mode_duration_sec = 0;//cfg_bt_pair.Pair_Mode_Duration_Sec;<"配对模式持续时间 (秒)",     0 ~ 600, /* 设置为 0 时不限时间 */	
	pair_config->bt_not_discoverable_when_connected = 0;//cfg_bt_pair.BT_Not_Discoverable_When_Connected ? 1 : 0;
#else
	/* 断开后 wait_connect 仍可被搜索，避免「隐身」；已连接时不可被搜 */
	pair_config->default_state_discoverable = 1;//cfg_bt_pair.Default_State_Discoverable ? 1 : 0;<"默认状态可被搜索发现",      CFG_TYPE_BOOL>
	pair_config->default_state_wait_connect_sec = 0;//cfg_bt_pair.Default_State_Wait_Connect_Sec;<"默认状态等待配对连接 (秒)", 0 ~ 600, /* 设置为 0 时不限时间 */>
	pair_config->pair_mode_duration_sec = 180;//cfg_bt_pair.Pair_Mode_Duration_Sec;<"配对模式持续时间 (秒)",     0 ~ 600, /* 设置为 0 时不限时间 */		
	pair_config->bt_not_discoverable_when_connected = 1;//cfg_bt_pair.BT_Not_Discoverable_When_Connected ? 1 : 0;
#endif
	pair_config->disconnect_all_phones_when_enter_pair_mode = 0;//cfg_bt_pair.Disconnect_All_Phones_When_Enter_Pair_Mode ? 1 : 0;
	pair_config->clear_paired_list_when_enter_pair_mode = 0;//cfg_bt_pair.Clear_Paired_List_When_Enter_Pair_Mode ? 1 : 0;
	pair_config->clear_tws_When_key_clear_paired_list = 0;//cfg_bt_pair.Clear_TWS_When_Key_Clear_Paired_List ? 1 : 0;
	pair_config->enter_pair_mode_when_key_clear_paired_list = 1;//cfg_bt_pair.Enter_Pair_Mode_When_Key_Clear_Paired_List ? 1 : 0;
	/* PY32 开机门控后：配对列表空则直接进配对（播 bt_pair.mp3） */
	pair_config->enter_pair_mode_when_paired_list_empty = 1;//cfg_bt_pair.Enter_Pair_Mode_When_Paired_List_Empty ? 1 : 0;	

	tws_pair_config->pair_key_mode = 2;//cfg_tws_pair.TWS_Pair_Key_Mode; 1:单方按键组队;2:双方按键组队
	tws_pair_config->match_mode = 0;//cfg_tws_pair.Match_Mode;0：TWS_MATCH_NAME;1:TWS_MATCH_ID
	tws_pair_config->match_name_length = 10;//cfg_tws_pair.Match_Name_Length;<"名称匹配长度", 1 ~ 30>
	tws_pair_config->wait_pair_search_time_sec = 60;//cfg_tws_pair.TWS_Wait_Pair_Search_Time_Sec;<"等待组对搜索时间 (秒)", 5 ~ 600>
	tws_pair_config->power_on_auto_pair_search = 0;//cfg_tws_pair.TWS_Power_On_Auto_Pair_Search;

    /* check TWS pair match name length (not include suffix name) */
    {
		uint8_t bt_name[CONFIG_MAX_BT_NAME_LEN + 1];
		int len;
		memset(bt_name, 0, sizeof(bt_name));
		len = property_get_string(CFG_BT_NAME, bt_name, CONFIG_MAX_BT_NAME_LEN, CONFIG_BT_DEVICE_NAME);
		if (len)
		{
			len = strlen(bt_name);
			tws_pair_config->match_name_length = len;
		}
		SYS_LOG_INF("tws_match_name_len: %d\n",tws_pair_config->match_name_length);
    }

#ifdef CONFIG_TWS	
	tws_pair_config->enable_advanced_pair_mode = 1;//cfg_tws_advanced_pair.Enable_TWS_Advanced_Pair_Mode? 1: 0;
	tws_pair_config->check_rssi_when_tws_pair_search = 0;//cfg_tws_advanced_pair.Check_RSSI_When_TWS_Pair_Search? 1: 0;
	tws_pair_config->use_search_mode_when_tws_reconnect = 0;//cfg_tws_advanced_pair.Use_Search_Mode_When_TWS_Reconnect? 1: 0;
	tws_pair_config->rssi_threshold = (-40);//cfg_tws_advanced_pair.RSSI_Threshold;<"信号强度阈值",           -120 ~ 0>
#endif

	tws_pair_config->tws_pair_count			  = 0xff;
	tws_pair_config->tws_role_select 		  = 0;


	sync_ctrl_config->a2dp_volume_sync_when_playing = 0;//cfg_bt_music_vol_sync.Volume_Sync_Only_When_Playing? 1:0;
	sync_ctrl_config->a2dp_Playing_volume_sync_delay_ms = 3000;//cfg_bt_music_vol_sync.Playing_Volume_Sync_Delay_Ms;
	sync_ctrl_config->a2dp_origin_volume_sync_to_remote = 1;//cfg_bt_music_vol_sync.Origin_Volume_Sync_To_Remote? 1:0; /* 强制开启：AVRCP 连接后主动把本机音量推给手机，手机调音量即可控制音箱 */
	sync_ctrl_config->a2dp_origin_volume_sync_delay_ms = 3000;//cfg_bt_music_vol_sync.Origin_Volume_Sync_Delay_Ms;
		

	sync_ctrl_config->hfp_origin_volume_sync_to_remote = 1;//cfg_bt_call_vol_sync.Origin_Volume_Sync_To_Remote? 1:0;
	sync_ctrl_config->hfp_origin_volume_sync_delay_ms = 1000;//cfg_bt_call_vol_sync.Origin_Volume_Sync_Delay_Ms;

	sync_ctrl_config->bt_music_default_volume = 10;//cfg_volume_settings.BT_Music_Default_Volume;
	sync_ctrl_config->bt_call_default_volume = 10;//cfg_volume_settings.BT_Call_Default_Volume;
	sync_ctrl_config->bt_music_default_volume_ex = 10;//cfg_volume_settings.BT_Music_Default_Vol_Ex;
	

	sync_ctrl_config->stop_another_when_one_playing = 1;//cfg_two_dev_play.Stop_Another_When_One_Playing? 1:0;
	sync_ctrl_config->resume_another_when_one_stopped = 1;//cfg_two_dev_play.Resume_Another_When_One_Stopped? 1:0;
	sync_ctrl_config->a2dp_status_stopped_delay_ms = 1500;//cfg_two_dev_play.A2DP_Status_Stopped_Delay_Ms;


	reconnect_config->enable_auto_reconnect = 3;//cfg_reconnect.Enable_Auto_Reconnect;
	reconnect_config->reconnect_phone_timeout = 50;//cfg_reconnect.Reconnect_Phone_Timeout;<50:5s;>
	reconnect_config->reconnect_phone_interval = 10;//cfg_reconnect.Reconnect_Phone_Interval;
	reconnect_config->reconnect_phone_times_by_startup = 5;//cfg_reconnect.Reconnect_Phone_Times_By_Startup;
	reconnect_config->reconnect_tws_timeout = 20;//cfg_reconnect.Reconnect_TWS_Timeout;<20:2s>
	reconnect_config->reconnect_tws_interval = 10;//cfg_reconnect.Reconnect_TWS_Interval;
	reconnect_config->reconnect_tws_times_by_startup = 5;//cfg_reconnect.Reconnect_TWS_Times_By_Startup;
	reconnect_config->reconnect_times_by_timeout = 30;//cfg_reconnect.Reconnect_Times_By_Timeout;<"超时断开回连尝试次数",           0 ~ 100, /* 设置为 0 时不限次数 */>
	/* 开机回连失败/超时后进配对（约 timeout×times ≈ 5s×5） */
	reconnect_config->enter_pair_mode_when_startup_reconnect_fail = 1;//cfg_reconnect.Enter_Pair_Mode_When_Startup_Reconnect_Fail;
    
	reconnect_config->always_reconnect_last_device = 0;
	reconnect_config->reconnect_resume_play = 0;

    if(reconnect_config->reconnect_phone_interval <= reconnect_config->reconnect_phone_timeout){
        reconnect_config->reconnect_phone_interval = reconnect_config->reconnect_phone_timeout + 10;
	}
    if(reconnect_config->reconnect_tws_interval <= reconnect_config->reconnect_tws_timeout){
        reconnect_config->reconnect_tws_interval = reconnect_config->reconnect_tws_timeout + 10;
	}

    return ret;
}


int system_btmgr_configs_update(void)
{
    if (system_btmgr_config_update_1st() != 0)
        return -1;
    if (system_btmgr_config_update_2nd() != 0)
        return -1;
    bt_manager_config_inited = true;
    return 0;	
}
