/*
 * Copyright (c) 2021 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt manager config.
*/

#ifndef __BT_MANAGER_CONFIG_H__
#define __BT_MANAGER_CONFIG_H__

// 蓝牙设备基本配置
#define CFG_KEY_BRMGR_BASE "BTMGR_BASE"
typedef enum
{
    SCAN_MODE_DEFAULT_INQUIRY_PAGE,
    SCAN_MODE_FAST_PAGE,
    SCAN_MODE_FAST_PAGE_EX,
    SCAN_MODE_NORMAL_PAGE,
    SCAN_MODE_NORMAL_PAGE_S3,
    SCAN_MODE_NORMAL_PAGE_EX,
    SCAN_MODE_FAST_INQUIRY_PAGE,
    SCAN_MODE_MAX,
}bt_scan_mode_e;

typedef struct 
{
    unsigned char   bt_device_name[CONFIG_MAX_BT_NAME_LEN + 1];  // 蓝牙设备名称
    unsigned char   bt_address[6];                               // 蓝牙地址
    unsigned char   use_random_bt_address;                       // 使用随机蓝牙地址, 通过 MIC 采样噪声生成低 3 字节蓝牙地址
    unsigned int    bt_device_class;                             // 蓝牙设备类型

    unsigned char   default_hosc_capacity;                       // 缺省频偏电容值 (pF), 0.0 ~ 24.0
    unsigned char   force_default_hosc_capacity;                 // 总是使用配置的频偏电容值

    unsigned char   bt_max_rf_tx_power;                          // 蓝牙最大发射功率, 0 ~ 22
    unsigned char   ble_rf_tx_power;                             // BLE 发射功率, 0 ~ 22
    unsigned char   ad2p_bitpool;                                // A2DP Bitpool, 2 ~ 53
    unsigned short  vendor_id;                                   // 厂商 ID
    unsigned short  product_id;                                  // 产品 ID
    unsigned short  version_id;                                  // 版本 ID

	struct	// 链路质量监控
	{
		unsigned char quality_monitor;
		unsigned char quality_pre_val;
		unsigned char quality_diff;
		unsigned char quality_esco_diff;
	} link_quality;

	struct	// SCAN 参数设置
	{
		bt_scan_mode_e	scan_mode;				// 模式
		unsigned short	inquiry_scan_window;	// InquiryScan 窗口
		unsigned short	inquiry_scan_interval;	// InquiryScan 间隔
		unsigned char	  inquiry_scan_type;		// InquiryScan 类型, 0 ~ 1
		unsigned short	page_scan_window;		// PageScan 窗口
		unsigned short	page_scan_interval; 	// PageScan 间隔
		unsigned char	  page_scan_type; 		// PageScan 类型, 0 ~ 1
	} scan_params[7];
	
} btmgr_base_config_t;


//蓝牙功能配置
#define CFG_KEY_BTMGR_FEATURE "BTMGR_FEATURE"

typedef struct
{
    unsigned short sp_a2dp_cp: 2;               //是否支持A2DP内容保护，0-不支持，1-支持DTCP，2-支持SCMS-T
    unsigned short sp_avdtp_aac: 1;             //是否支持AAC格式蓝牙推歌，0-表示不支持，1-表示支持
    unsigned short sp_avrcp_vol_syn: 1;         //是否支持AVRCP音量同步服务，0-表示不支持，1-表示支持
    unsigned short sp_hfp_vol_syn: 1;           //是否支持HFP音量同步服务，0-表示不支持，1-表示支持
    unsigned short sp_hfp_bat_report: 1;        //是否支持HFP电量上报服务，0-表示不支持，1-表示支持
    unsigned short sp_hfp_3way_call: 1;	        //是否支持三方通话,0-表示不支持，1-表示支持
    unsigned short sp_hfp_siri: 1;              //是否支持siri功能,0-表示不支持，1-表示支持
    unsigned short sp_hfp_codec_negotiation :1; //是否启动 hfp codec negotiatuon 0-表示不启动；1-表示启动
    unsigned short sp_hfp_enable_nrec:1;       	//是否HF端支持回音取消和噪声削弱 0-表示不支持;1-表示支持
    unsigned short sp_tws: 1;                   //是否支持tws功能，0-表示不支持，1-表示支持

    unsigned char sp_sniff_enable: 1;          //是否允许sniff功能，0-表示禁止; 1-表示使能
    unsigned char sp_dual_phone: 1;            //是否支持双手机设备连接，0表示不支持，1表示支持				
    unsigned char sp_phone_takeover: 1;        //是否支持手机抢占连接，0表示不支持，1表示支持
    unsigned char sp_linkkey_miss_reject: 1;   //回连时倘若手机取消配对了，则不弹出提示框并自动停止连接选项，0-默认行为;1-表示不弹出
    unsigned char sp_clear_linkkey:1;          //是否支持清除配对列表同时清除 LINKKEY,0表示不支持，1表示支持

    unsigned char  max_pair_list_num: 4;       //最多保存已配对设备连接信息个数
    unsigned char  sp_device_num: 4;           //支持同时连接设备个数
       
    unsigned char  controller_test_mode : 2;    //0-disable test mode 1-DUT_TEST 2-LE_TEST 3-DUT_TEST&LE_TEST
    unsigned char  enter_bqb_test_mode_by_key:1;
	
    unsigned char  con_real_test_mode:2;     //btmgr 初始化时，通过该参数进入相应测试模式

    unsigned char  sp_exit_sniff_when_bis:1;

    unsigned short enter_sniff_time;         //闲置进入sniff的时间
    unsigned short sniff_interval;           //sniff 周期
} btmgr_feature_cfg_t; 

// 蓝牙配对连接配置
#define CFG_KEY_BTMGR_PAIR "BTMGR_PAIR" 
typedef struct
{
    unsigned char	default_state_discoverable :1; 		     // 默认状态可被搜索发现
    unsigned char	disconnect_all_phones_when_enter_pair_mode :1;  // 进入配对模式时断开所有已连接手机设备
    unsigned char	clear_paired_list_when_enter_pair_mode :1; 	 // 进入配对模式时清除配对列表
    unsigned char	clear_tws_When_key_clear_paired_list :1;		 // 按键清除配对列表同时清除 TWS 组对设备信息
    unsigned char	enter_pair_mode_when_key_clear_paired_list :1;  // 按键清除配对列表同时进入配对模式
    unsigned char	enter_pair_mode_when_paired_list_empty :1; 	 // 配对列表为空时开机进入配对模式
    unsigned char	bt_not_discoverable_when_connected :1; 		 // 蓝牙已连接后关闭可见性

    unsigned short	default_state_wait_connect_sec; 			 // 默认状态等待配对连接 (秒), 0 ~ 600, 设置为 0 时不限时间
    unsigned short	pair_mode_duration_sec; 					 // 配对模式持续时间 (秒), 	0 ~ 600, 设置为 0 时不限时间
}btmgr_pair_cfg_t;

//TWS组对配置
#define CFG_KEY_BTMGR_TWS_PAIR "BTMGR_TWS_PAIR" 
typedef struct
{
	// TWS 组对连接
    unsigned char	pair_key_mode;				// 按键组对模式, 1:单方按键组对模式, 2:双方按键组对模式
    unsigned char	match_mode; 			    // 匹配模式,	 0:名称匹配, 1:ID 匹配
    unsigned char	match_name_length;			// 名称匹配长度
    unsigned short	wait_pair_search_time_sec;	// 等待组对搜索时间 (秒), 5 ~ 600
    unsigned char	power_on_auto_pair_search;	// 未组对过时开机自动进行组对搜索

    // TWS 高级组对设置
    unsigned char  enable_advanced_pair_mode :1;		// 启用 TWS 高级组对模式
    unsigned char  check_rssi_when_tws_pair_search :1; 	    // 组对搜索时判断信号强度
    unsigned char  use_search_mode_when_tws_reconnect:1;	// TWS 回连时使用搜索模式
    char   rssi_threshold;						// 信号强度阈值,			 -120 ~ 0

    unsigned char  sp_device_l_r_match: 1;   //是否在tws过滤规则上,进一步匹配L;R.
    unsigned char  tws_device_channel_l_r:2; //当前设备所属L或者R: 0-不确定,1-L, 2-R.	    	
    unsigned char  tws_pair_count;           //tws 组队的次数， 
    unsigned char  tws_role_select;          //是否强制固定住tws角色,0-根据实际场景选择,1-强制固定
}btmgr_tws_pair_cfg_t;


//音量同步，播放控制配置
#define CFG_KEY_BTMGR_SYNC_CTRL "BTMGR_SYNC_CTRL" 

typedef struct
{
    /*音乐音量同步*/
    unsigned char   a2dp_volume_sync_when_playing:1;     // 只在播放状态下同步音量
    unsigned char   a2dp_origin_volume_sync_to_remote:1; // 初始音量同步至远端设备, (连接时同步)
    unsigned short  a2dp_origin_volume_sync_delay_ms;    // 初始音量同步延迟时间 (毫秒), 2000 ~ 5000
    unsigned short  a2dp_Playing_volume_sync_delay_ms;   // 播放音量同步延迟时间 (毫秒), 1000 ~ 3000	

    // 通话音量同步
    unsigned char	hfp_origin_volume_sync_to_remote :1;  // 初始音量同步至远端设备, (开始通话时同步)
    unsigned short	hfp_origin_volume_sync_delay_ms;      // 初始音量同步延迟时间 (毫秒), 1000 ~ 3000

    // 双手机播放控制
    unsigned char	stop_another_when_one_playing :1;	 // 开始播放时停止另一手机
    unsigned char	resume_another_when_one_stopped :1;  // 停止播放时恢复另一手机
    unsigned short	a2dp_status_stopped_delay_ms;	     // 停止播放状态延迟时间 (毫秒), 500 ~ 5000


    unsigned char  bt_music_default_volume;  // 蓝牙音乐默认音量, 0 ~ 16
    unsigned char  bt_call_default_volume;   // 蓝牙通话默认音量, 0 ~ 15
    unsigned char  bt_music_default_volume_ex;  // 蓝牙音乐默认音量 (用于不支持音量同步的设备), 0 ~ 16


}btmgr_sync_ctrl_cfg_t;


// 蓝牙自动回连配置
#define CFG_KEY_BTMGR_RECONNECT "BTMGR_RECONNECT" 
typedef struct  
{
    unsigned char enable_auto_reconnect: 2; //启用自动回连, 0:不启用,     1:开机自动回连,2:超时断开自动回连 ,       3:{1+2}

    unsigned char reconnect_phone_timeout;     // 单次连接手机设备超时 (秒)
    unsigned char reconnect_phone_interval;          // 重试回连手机设备间隔时间 (秒)
    unsigned char reconnect_phone_times_by_startup;  // 开机回连手机设备尝试次数,       0 ~ 100, 设置为 0 时不限次数
    unsigned char reconnect_tws_timeout;             // 单次连接 TWS 设备超时 (秒)
    unsigned char reconnect_tws_interval;            // 重试回连 TWS 设备间隔时间 (秒)
    unsigned char reconnect_tws_times_by_startup;    // 开机回连 TWS 设备尝试次数,      0 ~ 100, 设置为 0 时不限次数
    unsigned char reconnect_times_by_timeout;        // 超时断开回连尝试次数,           0 ~ 100, 设置为 0 时不限次数
    unsigned char reconnect_resume_play;             // 超时回连是否恢复播放

    unsigned char enter_pair_mode_when_startup_reconnect_fail;  // 开机回连失败时进入配对模式
    unsigned char always_reconnect_last_device;      // 只回连最后一个设备，即使支持多设备,0:不启用,1：启用
    unsigned char always_reconnect_ota_device;       // 只回连OTA设备，即使支持多设备,0:不启用,1：启用

    unsigned short hid_auto_disconnect_delay_sec;    // <"HID 操作后自动断开延迟时间 (秒)", 0 ~ 600, /* 设置为 0 时不自动断开 */>
    unsigned short hid_connect_operation_delay_ms;  // <"HID 连接时操作延迟时间 (毫秒)",   100 ~ 2000>

} btmgr_reconnect_cfg_t;

// BLE管理配置
#define CFG_KEY_BTMGR_BLE "BTMGR_BLE" 
typedef enum
{
    BLE_ADV_DISABLE              = 0xff,
    BLE_ADV_IND                  = 0x00,
    BLE_ADV_DIRECT_IND_HIGH_DUTY = 0x01,
    BLE_ADV_SCAN_IND             = 0x02,
    BLE_ADV_NONCONN_IND          = 0x03,
    BLE_ADV_DIRECT_IND_LOW_DUTY  = 0x04,
}ble_adv_type_e;

typedef struct
{
    unsigned char  ble_enable:1;                           // 启用 BLE 功能
    unsigned char  use_advertising_mode_2_after_paired:1;  // 配对连接过后使用 BLE 广播模式 2, 配对列表非空且不在配对模式
    unsigned char  ble_address_type;   // BLE 地址类型, 0:public device address,1:static device address,2:Non-resolvable Private Address
	unsigned char  br_lea_auto_switch:1; //经典蓝牙和LE AUDIO同时存在时，活动设备是否自动切换
	unsigned char  leaudio_enable:1; //启用LEAUDIO功能
	unsigned char  leaudio_rank; //LEAUDIO组内编号，0则由代码自动分配

#if 0	
    struct	// BLE 广播模式
    {
        unsigned short	advertising_interval_ms;				  // 广播间隔 (毫秒), 20 ~ 5000
        ble_adv_type_e  advertising_type;					      // 广播类型
        unsigned char	ble_device_name[29];						  // BLE 设备名称
        unsigned char	manufacturer_specific_data[59]; 			  // 厂商自定义数据
        unsigned char	service_uuids_16_bit[59];					  // 服务 UUIDs (16-Bit)
        unsigned char	service_uuids_128_bit[38];  // 服务 UUIDs (128-Bit)
    } advertising_mode[2];
#endif

    struct	// BLE 连接参数
    {
        unsigned short	interval_min_ms;  // 最小间隔 (毫秒), 8 ~ 1000
        unsigned short	interval_max_ms;  // 最大间隔 (毫秒), 8 ~ 1000
        unsigned short	latency;		  // 延迟,			0 ~ 100
        unsigned short	timeout_Ms; 	  // 超时 (毫秒),	  500 ~ 10000
    } connection_param;

#if 0		
    struct	// BLE 数据透传
    {
        unsigned char	enable_pass_through;    // 启用 BLE 数据透传
        unsigned char	service_uuid[38];  // 服务 UUID
        unsigned char	tx_rx_uuid[38];	 // TX/RX UUID
        unsigned short	rf_buffer_size; 		    // RX 缓冲区大小, 128 ~ 4096
    } pass_through;
#endif	
} btmgr_ble_cfg_t;

typedef struct{
    unsigned short addr;
    unsigned short value;
}bt_rf_param_t;

typedef struct{
    bt_rf_param_t param[4];
}btmgr_rf_param_cfg_t;

#endif

