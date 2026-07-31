
#ifdef CONFIG_DATA_ANALY

#include <mem_manager.h>
#include <msg_manager.h>
#include <sys_event.h>
#include <app_launch.h>
#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <power_supply.h>
#include <srv_manager.h>
#include <app_manager.h>
#include <hotplug_manager.h>
#include <power_manager.h>
#include <input_manager.h>
#include <property_manager.h>
#include <sys_manager.h>
#include <ui_manager.h>

#include <logic.h>
#include "system_le_audio.h"

#ifdef CONFIG_BT_SELF_APP
#include <selfapp_api.h>
#endif

#include <data_analy.h>
#include <volume_manager.h>
#include <media_player.h>

#include <audio_system.h>

static auracast_status_e app_get_auracast_status(void)
{
	uint8_t mode = system_app_get_auracast_mode();
#ifdef CONFIG_BT_SELF_APP
	u8_t channel = selfapp_get_channel();
#else
	u8_t channel = 0;
#endif
	if(channel != 0 && mode != 0)
	{
		return AURACAST_STATUS_STEREO;
	}

	if(mode == 1){
		return AURACAST_STATUS_BROADCAST;
	}else if(mode == 2){
		return AURACAST_STATUS_RECEIVER;
	}else{
		return AURACAST_STATUS_NORMAL;
	}

}

static auracast_mode_e app_get_auracast_mode(void)
{
	uint8_t mode = system_app_get_auracast_mode();
#ifdef CONFIG_BT_SELF_APP
	u8_t channel = selfapp_get_channel();
#else
	u8_t channel = 0;
#endif

	if(mode == 0)
	{
		return AURACAST_MODE_NORMAL;
	}else{
		if(channel != 0)
		{
			return AURACAST_MODE_STEREO;
		}else{
			return AURACAST_MODE_PARTY;
		}
	}

	return AURACAST_MODE_NORMAL;
}

static auracast_role_e app_get_auracast_role(void)
{
	uint8_t mode = system_app_get_auracast_mode();

	if(mode == 1){
		return AURACAST_ROLE_BROADCAST;
	}else if(mode == 2){
		return AURACAST_ROLE_RECEIVER;
	}

	return AURACAST_ROLE_NORMAL;
}

static audio_bt_type_e app_get_bt_audio_in_type(void)
{
	return AUDIO_IN_A2DP;
}

static u8_t app_get_music_vol_level(void)
{
	return system_volume_get(0);
}



static u8_t app_PP_cnt = 0;
static void app_count_PP_key_press_cnt(u32_t key_event)
{
	if(key_event == (KEY_PAUSE_AND_RESUME|KEY_TYPE_SHORT_UP)
			|| key_event == (KEY_POWER|KEY_TYPE_SHORT_UP))
	{
		app_PP_cnt ++;
	}
}

static u8_t app_count_get_and_reset_PP_key_press_cnt(void)
{
	u8_t ret = app_PP_cnt;
	app_PP_cnt = 0;
	return ret;
}

static u8_t app_avrcp_change_vol_cnt = 0;
void app_count_avrcp_change_vol_cnt(void)
{
	app_avrcp_change_vol_cnt ++;
}

static u8_t app_get_vol_change_by_avrcp_cnt(void)//get_and_clean_flag
{
	u8_t ret = app_avrcp_change_vol_cnt;
	app_avrcp_change_vol_cnt = 0;
	return ret;
}

static u8_t app_phy_change_vol_cnt = 0;
static void app_count_phy_change_vol_cnt(u32_t key_event)
{
	if(key_event == (KEY_VOLUMEUP|KEY_TYPE_SHORT_UP)
			|| key_event == (KEY_VOLUMEDOWN|KEY_TYPE_SHORT_UP))
	{
		app_phy_change_vol_cnt ++;
	}
}

static u8_t app_get_vol_change_by_phy_cnt(void)//get_and_clean_flag
{
	u8_t ret = app_phy_change_vol_cnt;
	app_phy_change_vol_cnt = 0;
	return ret;
}

void app_count_key_press_cnt(u32_t key_event)
{
	app_count_PP_key_press_cnt(key_event);
	app_count_phy_change_vol_cnt(key_event);
}

static u8_t app_get_is_boost_mode(void)
{
#ifdef CONFIG_BT_SELF_APP
	return selfapp_eq_is_playtimeboost();
#else
	return 0;
#endif
}

static u8_t app_get_eq_id(void)
{
#ifdef CONFIG_BT_SELF_APP
	return selfapp_eq_get_index();
#else
	return 0;
#endif
}

static u8_t app_get_is_default_eq(void)
{
#ifdef CONFIG_BT_SELF_APP
	return selfapp_eq_is_default();
#else
	return 0;
#endif
}

static u8_t app_get_is_adapter_connect(void)
{
	return power_manager_get_dc5v_status();
}

static u8_t app_get_is_battery_full(void)
{
	return power_manager_get_battery_is_full();
}

static u8_t app_get_is_music_playing(void)
{
	u8_t ret = 0;

	struct audio_track_t * track = audio_system_get_track();

	if(!track){
		goto out;
	}

	if(track->stream_type != AUDIO_STREAM_TTS)
	{
		ret = 1;
	}else{
		ret = 0;
	}

out:
	return ret;
}

static u8_t app_get_is_water_proof(void)
{
	return 0;
}

static u8_t app_get_is_smart_ctl(void)
{
	return 0;
}


void system_data_analy_init(u8_t power_on)
{
	data_analy_init_param_t init_param = {
		.dev_data_get={
		.get_auracast_role = app_get_auracast_role,
		.get_auracast_mode = app_get_auracast_mode,
		.get_auracast_status = app_get_auracast_status,
		.get_bt_audio_in_type = app_get_bt_audio_in_type,
		.get_music_vol_level = app_get_music_vol_level,
		.get_eq_id = app_get_eq_id,
		.get_and_reset_PP_key_press_cnt = app_count_get_and_reset_PP_key_press_cnt,
		.get_is_vol_change_by_avrcp = app_get_vol_change_by_avrcp_cnt,
		.get_is_vol_change_by_phy = app_get_vol_change_by_phy_cnt,
		.get_is_boost_mode = app_get_is_boost_mode,
		.get_is_default_eq = app_get_is_default_eq,
		.get_is_adapter_connect = app_get_is_adapter_connect,
		.get_is_battery_full = app_get_is_battery_full,
		.get_is_music_playing =app_get_is_music_playing,
		.get_is_water_proof = app_get_is_water_proof,
		.get_is_smart_ctl = app_get_is_smart_ctl,
			//
			//
		},
		.power_on = power_on,
	};

	if(data_analy_init(&init_param))
	{
		SYS_LOG_ERR("init error\n");
	}
}



#endif
