#ifndef __DATA_ANALY_H__
#define __DATA_ANALY_H__

#include <zephyr.h>
#include "property_manager.h"
#include <thread_timer.h>

#define CFG_DATA_ANALY_DATA "analy_data"
#define CFG_DATA_ANALY_PLAY "analy_play"
#define CFG_PRODUCT_INFO "product_info"

#define VOLUME_COUNT_DOWN_SEC (10)
#define DATA_ANALY_PLAY_ID_MAX (50)
#define DATA_ANALY_PLAY_MAX_SAVE_ID (DATA_ANALY_PLAY_ID_MAX - 1)
#define DATA_ANALY_PLAY_MAX_PLAY_MIN (241)
#define DATA_ANALY_PLAY_MAX_PLAY_SEC (DATA_ANALY_PLAY_MAX_PLAY_MIN*60)

typedef enum{
	AURACAST_STATUS_NORMAL = 0,
	AURACAST_STATUS_BROADCAST,
	AURACAST_STATUS_RECEIVER,
	AURACAST_STATUS_STEREO,
} auracast_status_e;

typedef enum{
	AURACAST_MODE_NORMAL = 0,
	AURACAST_MODE_PARTY,
	AURACAST_MODE_STEREO,
} auracast_mode_e;

typedef enum{
	AURACAST_ROLE_NORMAL = 0,
	AURACAST_ROLE_BROADCAST,
	AURACAST_ROLE_RECEIVER,
} auracast_role_e;

typedef enum {
	AUDIO_IN_A2DP = 1,
	AUDIO_IN_LE_AUDIO = 2,
}audio_bt_type_e;

typedef enum {
	DATA_ANALY_PLAY_DC = 0,
	DATA_ANALY_PLAY_AC_CHARGE,
	DATA_ANALY_NON_PLAY_AC_CHARGE,
}data_analy_play_charge_staus_e;


typedef auracast_role_e (*get_auracast_role)(void);
typedef auracast_mode_e (*get_auracast_mode)(void);
typedef auracast_status_e (*get_auracast_status)(void);
typedef audio_bt_type_e (*get_bt_audio_in_type)(void);
typedef u8_t (*get_music_vol_level)(void);
typedef u8_t (*get_eq_id)(void);
typedef u8_t (*get_and_reset_PP_key_press_cnt)(void);
typedef u8_t (*get_is_vol_change_by_avrcp)(void);//get_and_clean_flag
typedef u8_t (*get_is_vol_change_by_phy)(void);//get_and_clean_flag
typedef u8_t (*get_is_boost_mode)(void);
typedef u8_t (*get_is_default_eq)(void);
typedef u8_t (*get_is_adapter_connect)(void);
typedef u8_t (*get_is_battery_full)(void);
typedef u8_t (*get_is_music_playing)(void);
typedef u8_t (*get_is_smart_ctl)(void);
typedef u8_t (*get_is_water_proof)(void);



typedef struct {
	get_auracast_role		get_auracast_role;
	get_auracast_mode		get_auracast_mode;
	get_auracast_status		get_auracast_status;
	get_bt_audio_in_type	get_bt_audio_in_type;
	get_music_vol_level		get_music_vol_level;
	get_eq_id				get_eq_id;
	get_and_reset_PP_key_press_cnt		get_and_reset_PP_key_press_cnt;
	get_is_vol_change_by_avrcp	get_is_vol_change_by_avrcp;
	get_is_vol_change_by_phy	get_is_vol_change_by_phy;
	get_is_boost_mode		get_is_boost_mode;
	get_is_default_eq       get_is_default_eq;
	get_is_adapter_connect  get_is_adapter_connect;
	get_is_battery_full     get_is_battery_full;
	get_is_music_playing    get_is_music_playing;
	get_is_water_proof      get_is_water_proof;
	get_is_smart_ctl		get_is_smart_ctl;
}data_analy_get_dev_data_t;

typedef struct {
	data_analy_get_dev_data_t dev_data_get;
	u8_t power_on;
} data_analy_init_param_t;

typedef struct {
    uint32_t auracast_enter_times;		//times
    uint32_t auracast_rx_sec; //unit: second
    uint32_t auracast_stereo_sec; //unit: second
    uint32_t auracast_party_sec; //unit: second
	uint32_t boost_on_defualt_eq_sec;
	uint32_t boost_off_defualt_eq_sec;
	uint32_t boost_off_non_defualt_eq_sec;
    uint32_t music_playback_sec; //unit: second
    uint32_t music_playback_charge_sec; //unit: second
    uint32_t power_on_charge_sec; //unit: second
    uint32_t power_on_sec;
    uint32_t power_on_times;
    uint32_t press_PP_times;
    uint32_t smart_ctl_times;
    uint32_t volume_changed_by_phy_times;
    uint32_t volume_changed_by_avrcp_times;
    uint32_t usb_waterproof_alarm_times;
} data_analytics_upload_t;


typedef struct {
	auracast_role_e auracast_role;
	u8_t waterproof;
	u8_t smart_ctl;
	u8_t vol_phy_count_down;
	u8_t vol_avrcp_count_down;
	s64_t pwr_on_stamp;
	s64_t pwr_on_charge_stamp;

	data_analytics_upload_t upload;
}data_analytics_t;


typedef struct {
	u8_t play_min:8;
	u8_t vol_level:6;
	u8_t auracast_status:2;
	u8_t eq_id:4;
	u8_t party_boost:2;
	u8_t charge_status:1;
	u8_t audio_in_type:1;
} play_analytics_upload_t;

typedef struct {
	u8_t next_write_id;
	u8_t ever_full;
} play_index_info_t;

typedef struct {
	u8_t written;
	u8_t adapter_connect;
	u16_t param_stay_sec;
	play_index_info_t index;
	play_analytics_upload_t upload;
} play_analytics_t;




typedef struct _data_analy_t {
	data_analytics_t data;
	play_analytics_t play;
	struct thread_timer play_time_thread_timer;
	u8_t power_on;
} data_analy_t;


typedef struct _product_info_t {
	u64_t product_play_sec;
	u64_t product_pwr_on_sec;
} product_info_t;




void data_analy_dump_data(void);
void data_analy_dump_play(void);

void data_analy_dump_play_all_record(void);


int data_analy_init(data_analy_init_param_t* init_param);
int data_analy_exit(void);


u32_t data_analy_get_product_history_pwr_on_min(void);
u32_t data_analy_get_product_history_play_min(void);

int data_analy_data_get(data_analytics_upload_t* buf, u16_t len);

//arr is arrary of play_analytics_upload_t,the newest data is at the first index
int data_analy_play_get(play_analytics_upload_t* arr, u16_t arr_num);

void data_analy_data_clear(void);
void data_analy_play_clear(void);

void system_data_analy_init(u8_t power_on);

#endif
