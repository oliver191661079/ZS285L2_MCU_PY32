#include "data_analy.h"
#include <os_common_api.h>
#include <media_player.h>
#include <nvram_config.h>
#include <stdio.h>

#ifdef SYS_LOG_NO_NEWLINE
//#undef SYS_LOG_NO_NEWLINE
#endif

#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "DA"
#include <logging/sys_log.h>

static product_info_t product_info={0};

static data_analy_t g_data_analy = {0};
static data_analy_get_dev_data_t dev_data_get = {0};

static data_analytics_t *analy_data_p = NULL;
static play_analytics_t *analy_play_p = NULL;

static data_analytics_upload_t *analy_upload_data_p = NULL;
static play_analytics_upload_t *analy_upload_play_p = NULL;

static play_index_info_t *analy_play_index_p = NULL;

static void data_analy_dump_play_record(play_analytics_upload_t * play);

u32_t data_analy_update_power_on_sec(void)
{
	//如果产品需要进入s3bt的话，开机计时，建议将uptime改成 rtc
	if(g_data_analy.power_on){
		s64_t delta_ms = k_uptime_delta(&analy_data_p->pwr_on_stamp);
		analy_upload_data_p->power_on_sec += (delta_ms/1000);
		analy_data_p->pwr_on_stamp = k_uptime_get();
		return analy_upload_data_p->power_on_sec;
	}

	return 0;
}


void data_analy_data_clear(void)
{
	memset(analy_upload_data_p, 0, sizeof(data_analytics_upload_t));
	nvram_config_set(CFG_DATA_ANALY_DATA, analy_upload_data_p, sizeof(data_analytics_upload_t));
}

void data_analy_play_clear(void)
{
	//only clear nvram save id, current recording data need keep counting
	memset(analy_play_index_p, 0, sizeof(play_index_info_t));
	nvram_config_set(CFG_DATA_ANALY_PLAY, analy_play_p, sizeof(play_analytics_t));
}

u32_t data_analy_get_product_history_pwr_on_min(void)
{
	return (u32_t)(product_info.product_pwr_on_sec/60);
}

u32_t data_analy_get_product_history_play_min(void)
{
	return (u32_t)(product_info.product_play_sec/60);
}


int data_analy_data_get(data_analytics_upload_t* buf, u16_t len)
{
	if(!buf || len < sizeof(data_analytics_upload_t))
	{
		return -1;
	}
	data_analy_update_power_on_sec();
	memcpy(buf, analy_upload_play_p, sizeof(data_analytics_upload_t));
	return 0;
}

int data_analy_play_get(play_analytics_upload_t* arr, u16_t arr_num)
{
	if(!arr || arr_num < DATA_ANALY_PLAY_ID_MAX)
	{
		return -1;
	}

	s8_t id = analy_play_index_p->next_write_id;
	u8_t max = analy_play_index_p->ever_full?DATA_ANALY_PLAY_ID_MAX:analy_play_index_p->next_write_id;
	char id_info[16];
	u8_t i = 0;
	for( i = 0; i < max; i++)
	{
		if(--id < 0)
		{
			id = DATA_ANALY_PLAY_MAX_SAVE_ID;
		}
		snprintf(id_info, sizeof(id_info), "DA_PLAY_%d", id);

		int ret = nvram_config_get(id_info, arr, sizeof(play_analytics_upload_t));
		if(ret != sizeof(play_analytics_upload_t))
		{
			break;
		}
		arr ++;
	}

	return i;
}


static void data_analy_scan_auracast(void)
{
	if(!dev_data_get.get_auracast_role){
		SYS_LOG_ERR("get_auracast_role NULL");
		return;
	}

	if(!dev_data_get.get_auracast_mode){
		SYS_LOG_ERR("get_auracast_mode NULL");
		return;
	}

	if(!dev_data_get.get_is_music_playing){
		SYS_LOG_ERR("get_is_music_playing NULL");
		return;
	}

	u8_t playing = dev_data_get.get_is_music_playing();
	auracast_mode_e auracast_mode = dev_data_get.get_auracast_mode();
	auracast_role_e auracast_role = dev_data_get.get_auracast_role();

	//broadcaster ||  receiver
	if(auracast_role ==  AURACAST_ROLE_RECEIVER
			|| auracast_role == AURACAST_ROLE_BROADCAST)
	{
		if(auracast_role != analy_data_p->auracast_role)
		{
			analy_data_p->auracast_role = auracast_role;
			analy_upload_data_p->auracast_enter_times ++;
		}
	}

	if(playing){
		//Receiver
		if(auracast_role == AURACAST_ROLE_RECEIVER)
		{
			analy_upload_data_p->auracast_rx_sec ++;
		}

		if(auracast_mode == AURACAST_MODE_STEREO)
		{
			analy_upload_data_p->auracast_stereo_sec ++;
		}

		if(auracast_mode == AURACAST_MODE_PARTY){
			analy_upload_data_p->auracast_party_sec ++;
		}

	}
}

static void data_analy_scan_eq(void)
{
	if(!dev_data_get.get_is_boost_mode){
		SYS_LOG_ERR("get_is_boost_mode NULL");
		return;
	}

	if(!dev_data_get.get_is_default_eq){
		SYS_LOG_ERR("get_is_default_eq NULL");
		return;
	}

	if(!dev_data_get.get_is_music_playing){
		SYS_LOG_ERR("get_is_music_playing NULL");
		return;
	}

	u8_t playing = dev_data_get.get_is_music_playing();

	if(playing)
	{
		u8_t boost_mode = dev_data_get.get_is_boost_mode();
		u8_t default_eq = dev_data_get.get_is_default_eq();

		if(boost_mode && default_eq)
		{
			analy_upload_data_p->boost_on_defualt_eq_sec ++;
		}else if( !boost_mode && !default_eq){
			analy_upload_data_p->boost_off_non_defualt_eq_sec ++;
		}else if( !boost_mode && default_eq){
			analy_upload_data_p->boost_off_defualt_eq_sec ++;
		}
	}
}

static void data_analy_scan_dev_volume_change(void)
{
	if(!dev_data_get.get_is_vol_change_by_avrcp)
	{
		SYS_LOG_ERR("get_is_vol_change_by_avrcp NULL");
		return;
	}
	if(!dev_data_get.get_is_vol_change_by_phy)
	{
		SYS_LOG_ERR("get_is_vol_change_by_phy NULL");
		return;
	}

	if(dev_data_get.get_is_vol_change_by_avrcp())
	{
		analy_data_p->vol_avrcp_count_down = VOLUME_COUNT_DOWN_SEC;
	}

	if(analy_data_p->vol_avrcp_count_down)
	{
		analy_data_p->vol_avrcp_count_down --;
		if(!analy_data_p->vol_avrcp_count_down)
		{
			analy_upload_data_p->volume_changed_by_avrcp_times ++;
		}
	}

	if(dev_data_get.get_is_vol_change_by_phy())
	{
		analy_data_p->vol_phy_count_down = VOLUME_COUNT_DOWN_SEC;
	}

	if(analy_data_p->vol_phy_count_down)
	{
		analy_data_p->vol_phy_count_down --;
		if(!analy_data_p->vol_phy_count_down)
		{
			analy_upload_data_p->volume_changed_by_phy_times ++;
		}
	}
}

static void data_analy_scan_music_playback_with_adapter(void)
{
	if(!dev_data_get.get_is_music_playing)
	{
		SYS_LOG_ERR("get_is_music_playing NULL");
		return;
	}

	if(!dev_data_get.get_is_adapter_connect)
	{
		SYS_LOG_ERR("get_is_adapter_connect NULL");
		return;
	}

	if(!dev_data_get.get_is_battery_full)
	{
		SYS_LOG_ERR("get_is_battery_full NULL");
		return;
	}


	u8_t playing = dev_data_get.get_is_music_playing();
	u8_t adapter = dev_data_get.get_is_adapter_connect();
	u8_t battery_full = dev_data_get.get_is_battery_full();

	if(playing){
		analy_upload_data_p->music_playback_sec ++;
	}

	if(adapter)
	{
		if(playing){
			analy_upload_data_p->music_playback_charge_sec ++;
		}

		if(!battery_full)
		{
			s64_t delta_ms = k_uptime_delta(&analy_data_p->pwr_on_charge_stamp);
			if(delta_ms > 1500)
			{
				analy_upload_data_p->power_on_charge_sec += delta_ms/1000;
			}else{
				analy_upload_data_p->power_on_charge_sec ++;
			}
		}else{
			SYS_LOG_INF("battery_full\n");
		}
	}

	analy_data_p->pwr_on_charge_stamp = k_uptime_get();
}

static void data_analy_scan_key_press(void)
{

	if(!dev_data_get.get_and_reset_PP_key_press_cnt)
	{
		SYS_LOG_ERR("get_and_reset_PP_key_press_cnt NULL");
		return;
	}

	u8_t cnt = dev_data_get.get_and_reset_PP_key_press_cnt();
	if(cnt)
	{
		analy_upload_data_p->press_PP_times += cnt;
	}
}


static void data_analy_scan_smart_ctl_times(void)
{
	if(!dev_data_get.get_is_smart_ctl)
	{
		SYS_LOG_ERR("get_is_smart_ctl NULL");
		return;
	}

	u8_t smart_ctl = dev_data_get.get_is_smart_ctl();
	if(analy_data_p->smart_ctl != smart_ctl)
	{
		analy_data_p->smart_ctl = smart_ctl;
		if(smart_ctl){
			analy_upload_data_p->smart_ctl_times ++;
		}
	}
}

static void data_analy_scan_water_proof(void)
{
	if(!dev_data_get.get_is_water_proof)
	{
		SYS_LOG_ERR("get_is_water_proof NULL");
		return;
	}

	u8_t waterproof = dev_data_get.get_is_water_proof();
	if(analy_data_p->waterproof != waterproof)
	{
		analy_data_p->waterproof = waterproof;
		if(waterproof){
			analy_upload_data_p->usb_waterproof_alarm_times ++;
		}
	}
}



static int data_analy_play_set_data_by_id(u8_t id, play_analytics_upload_t* data)
{
	if(!data)
	{
		SYS_LOG_ERR("param null");
		return -1;
	}


	int ret = 0;
	if(id > DATA_ANALY_PLAY_MAX_SAVE_ID)
	{
		SYS_LOG_ERR("id is error %d", id);
		return -1;
	}

	data_analy_dump_play_record(data);

	char id_info[16];
	snprintf(id_info, sizeof(id_info), "DA_PLAY_%d", id);
	ret = nvram_config_set(id_info, data, sizeof(play_analytics_upload_t));
	return ret;
}

static int data_analy_play_get_data_by_id(u8_t id, play_analytics_upload_t* data)
{
	int ret = 0;

	if(!data)
	{
		SYS_LOG_ERR("param null");
		return -1;
	}

	memset(data, 0, sizeof(play_analytics_upload_t));

	if(id > DATA_ANALY_PLAY_MAX_SAVE_ID)
	{
		SYS_LOG_ERR("id is error %d", id);
		return -1;
	}

	char id_info[16];
	snprintf(id_info, sizeof(id_info), "DA_PLAY_%d", id);
	ret = nvram_config_get(id_info, data, sizeof(play_analytics_upload_t));
	return ret;

}

static int data_analy_play_get_pre(play_analytics_upload_t* prev, u8_t* id)
{
	u8_t prev_id = 0;
	int ret = 0;

	if(!prev)
	{
		SYS_LOG_ERR("param null");
		return -1;
	}

	if(analy_play_index_p->next_write_id == 0)
	{
		if(analy_play_index_p->ever_full)
		{
			prev_id = DATA_ANALY_PLAY_MAX_SAVE_ID;
		}else{
			memset(prev, 0, sizeof(play_analytics_upload_t));
			SYS_LOG_INF("no prev");
			return 0;
		}
	}else{
		prev_id = analy_play_index_p->next_write_id - 1;
	}

	ret = data_analy_play_get_data_by_id(prev_id, prev);

	if(id)
	{
		*id = prev_id;
	}
	return ret;
}

static int data_analy_play_write(play_analytics_upload_t* play)
{
	if(!play)
	{
		SYS_LOG_ERR("param null");
		return -1;
	}

	play_analytics_upload_t prev = {0};
	play_analytics_upload_t play_tmp = {0};
	play_tmp.charge_status = 1;
	u8_t prev_id = 0;
	int ret = data_analy_play_get_pre((play_analytics_upload_t *)&prev, &prev_id);

	if(0 == memcmp((void*)&prev, (void *)&play_tmp, sizeof(play_analytics_upload_t))){
		//非播放充电状态
		if(play->charge_status == 1)
		{
			//记录的状态为充电状态时，覆盖前一个。
			SYS_LOG_WRN("cover prev non_play_charge");
			ret = data_analy_play_set_data_by_id(prev_id, play);
			return ret;
		}
	}

	ret = data_analy_play_set_data_by_id(analy_play_index_p->next_write_id, play);

	analy_play_index_p->next_write_id ++;
	if(analy_play_index_p->next_write_id > DATA_ANALY_PLAY_MAX_SAVE_ID)
	{
		analy_play_index_p->next_write_id = 0;
		analy_play_index_p->ever_full = 1;
	}
	return ret;
}

static void data_analy_play_check_time_then_write(void)
{

	if(analy_play_p->param_stay_sec > 60 && !analy_play_p->written)
	{
		analy_upload_play_p->play_min = analy_play_p->param_stay_sec/60;
		data_analy_dump_play();
		data_analy_play_write(analy_upload_play_p);
		analy_play_p->written = 1;
	}
}


static void data_analy_play_scan(void)
{
	if(!dev_data_get.get_is_adapter_connect)
	{
		SYS_LOG_ERR("get_is_adapter_connect NULL");
		return;
	}

	if(!dev_data_get.get_is_music_playing)
	{
		SYS_LOG_ERR("get_is_music_playing NULL");
		return;
	}

	if(!dev_data_get.get_music_vol_level)
	{
		SYS_LOG_ERR("get_music_vol_level NULL");
		return;
	}

	if(!dev_data_get.get_auracast_status)
	{
		SYS_LOG_ERR("get_auracast_status NULL");
		return;
	}

	if(!dev_data_get.get_bt_audio_in_type)
	{
		SYS_LOG_ERR("get_bt_audio_in_type NULL");
		return;
	}

	if(!dev_data_get.get_eq_id)
	{
		SYS_LOG_ERR("get_eq_id NULL");
		return;
	}

	if(!dev_data_get.get_is_battery_full)
	{
		SYS_LOG_ERR("get_is_battery_full NULL");
		return;
	}


	u8_t adapter = dev_data_get.get_is_adapter_connect();
	u8_t battery_full = dev_data_get.get_is_battery_full();
	u8_t playing = dev_data_get.get_is_music_playing();
	u8_t reset_sec = 0;

#if 0
	if(analy_play_p->adapter_connect != adapter)
	{
		analy_play_p->adapter_connect = adapter;
		if(adapter)
		{
			play_analytics_upload_t prev = {0};

			data_analy_play_get_pre(&prev, NULL);

			if(prev.charge_status == 0)
			{
				//插入 非播放充电状态
				play_analytics_upload_t now = {0};
				now.charge_status = 1;
				data_analy_play_write(&now);
			}
		}
	}
#endif

	if(!playing)
	{
		data_analy_play_check_time_then_write();
		reset_sec ++;

		if(adapter && !battery_full)
		{
			play_analytics_upload_t prev = {0};

			data_analy_play_get_pre(&prev, NULL);
			//插入 非播放充电状态
			play_analytics_upload_t now = {0};
			now.charge_status = 1;

			if(!prev.charge_status)
			{
				data_analy_play_write(&now);
			}
		}
		goto change_out;
	}

	u8_t vol = dev_data_get.get_music_vol_level();
	u8_t eq_id = dev_data_get.get_eq_id();
	auracast_status_e auracast_status = dev_data_get.get_auracast_status();
	audio_bt_type_e audio_in_type = dev_data_get.get_bt_audio_in_type();

	data_analy_play_charge_staus_e charge_status;
	if(adapter)
	{
		if(playing){
			charge_status = DATA_ANALY_PLAY_AC_CHARGE;
		}else{
			charge_status = DATA_ANALY_NON_PLAY_AC_CHARGE;
		}
	}else{
		charge_status = DATA_ANALY_PLAY_DC;
	}


	if(charge_status != analy_upload_play_p->charge_status)
	{
		data_analy_play_check_time_then_write();
		analy_upload_play_p->charge_status = charge_status;
		reset_sec ++;
	}

	if(vol != analy_upload_play_p->vol_level)
	{
		data_analy_play_check_time_then_write();
		analy_upload_play_p->vol_level = vol;
		reset_sec ++;
	}

	if(auracast_status != analy_upload_play_p->auracast_status)
	{

		data_analy_play_check_time_then_write();
		analy_upload_play_p->auracast_status = auracast_status;
		reset_sec ++;
	}

	if(audio_in_type != analy_upload_play_p->audio_in_type)
	{
		data_analy_play_check_time_then_write();
		analy_upload_play_p->audio_in_type = audio_in_type;
		reset_sec ++;
	}

	if(eq_id != analy_upload_play_p->eq_id)
	{
		data_analy_play_check_time_then_write();
		analy_upload_play_p->eq_id = eq_id;
		reset_sec ++;
	}


	if(reset_sec){
change_out:
		analy_play_p->param_stay_sec = 0;
		analy_play_p->written = 0;
		return;
	}else{
		analy_play_p->param_stay_sec ++;

		SYS_LOG_INF("param stay %d s", analy_play_p->param_stay_sec);
		if(analy_play_p->param_stay_sec > DATA_ANALY_PLAY_MAX_PLAY_SEC)
		{
			analy_upload_play_p->play_min = DATA_ANALY_PLAY_MAX_PLAY_MIN;
			data_analy_play_write(analy_upload_play_p);
			analy_play_p->param_stay_sec = 0;
			analy_play_p->written = 0;
		}
	}
}


static void data_analy_dump_play_record(play_analytics_upload_t * play)
{
	if(!play)
	{
		SYS_LOG_ERR("param null ");
		return ;
	}

	printk("***********%s***********\n", __func__);
	printk("\t charge:%d, vol:%d, auracast:%d, pb:%d\n", play->charge_status,\
			play->vol_level, play->auracast_status, \
			play->party_boost);
	printk("\t audio_type:%d, eq:%d, min %d\n",play->audio_in_type, play->eq_id, play->play_min);
	printk("*************************************\n");
}

void data_analy_dump_play_all_record(void)
{
	if(!analy_play_index_p)
		return;

	s8_t id = analy_play_index_p->next_write_id;
	u8_t max = analy_play_index_p->ever_full?DATA_ANALY_PLAY_ID_MAX:analy_play_index_p->next_write_id;

	SYS_LOG_INF("reocrd total %d", max);

	char id_info[16];
	u8_t i = 0;

	static play_analytics_upload_t buf;

	for( i=0; i < max; i++)
	{
		if(--id < 0)
		{
			id = DATA_ANALY_PLAY_MAX_SAVE_ID;
		}
		snprintf(id_info, sizeof(id_info), "DA_PLAY_%d", id);

		int ret = nvram_config_get(id_info, &buf, sizeof(play_analytics_upload_t));
		if(ret != sizeof(play_analytics_upload_t))
		{
			break;
		}
		SYS_LOG_INF("new to old %d, buf_id = %d", i, id);
		data_analy_dump_play_record(&buf);
	}
}

void data_analy_dump_play(void)
{
	if(!analy_upload_play_p || !analy_play_p)
	{
		SYS_LOG_ERR(" data_analy not working!!");
		return ;
	}

	printk("***********%s***********\n", __func__);
	printk("\t charge:%d, vol:%d, auracast:%d, pb:%d\n", analy_upload_play_p->charge_status,\
			analy_upload_play_p->vol_level, analy_upload_play_p->auracast_status, \
			analy_upload_play_p->party_boost);
	printk("\t audio_type:%d, eq:%d, sec %d\n",analy_upload_play_p->audio_in_type, \
			analy_upload_play_p->eq_id, analy_play_p->param_stay_sec);

	printk("\t next_write_id:%d, ever_full:%d\n",analy_play_index_p->next_write_id,
			analy_play_index_p->ever_full);

	printk("*************************************\n");
}

void data_analy_dump_data(void)
{

	if(!analy_upload_data_p)
	{
		SYS_LOG_ERR(" data_analy not working!!");
		return ;
	}

	printk("***********%s***********\n", __func__);
	printk("[DA_data] auracast:\n");
	printk("\t\t times:%d", analy_upload_data_p->auracast_enter_times);
	printk("\t rx sec:%d", analy_upload_data_p->auracast_rx_sec);
	printk("\t stero sec:%d", analy_upload_data_p->auracast_stereo_sec);
	printk("\t party sec:%d\n", analy_upload_data_p->auracast_party_sec);

	printk("[DA_data] eq:\n");
	printk("\t\t boost 1 deq sec:%d", analy_upload_data_p->boost_on_defualt_eq_sec);
	printk("\t boost 0 deq sec:%d", analy_upload_data_p->boost_off_defualt_eq_sec);
	printk("\t boost 0 neq sec:%d\n", analy_upload_data_p->boost_off_non_defualt_eq_sec);

	printk("[DA_data] play:\n");
	printk("\t\t play sec:%d", analy_upload_data_p->music_playback_sec);
	printk("\t play charge sec:%d\n", analy_upload_data_p->music_playback_charge_sec);

	printk("[DA_data] charge:\n");
	printk("\t\t charge sec:%d\n", analy_upload_data_p->power_on_charge_sec);

	printk("[DA_data] key_press:\n");
	printk("\t\t power on sec:%d", data_analy_update_power_on_sec());
	printk("\t power on cnt:%d", analy_upload_data_p->power_on_times);
	printk("\t PP cnt:%d\n", analy_upload_data_p->press_PP_times);

	printk("[DA_data] volume_change:");
	printk("\t\t change phy cnt:%d", analy_upload_data_p->volume_changed_by_phy_times);
	printk("\t change avrcp cnt:%d\n", analy_upload_data_p->volume_changed_by_avrcp_times);
	printk("[DA_data] waterproof: \n");
	printk("\t\t cnt:%d\n", analy_upload_data_p->usb_waterproof_alarm_times);

	printk("[product] info:");
	printk("\t\t pwr_on_history min:%d", data_analy_get_product_history_pwr_on_min());
	printk("\t play history min:%d\n", data_analy_get_product_history_play_min());

	printk("*************************************\n");
}

static void data_analy_dump(void)
{
#ifdef CONFIG_DATA_ANALY_DUMP_DATA
	static u8_t cnt = 0;
	if(++cnt < 5)
	{
		return;
	}
	cnt = 0;
	data_analy_dump_data();
#endif

}

static void product_info_scan(void)
{
	if(g_data_analy.power_on)
	{
		product_info.product_pwr_on_sec ++;

		if(!dev_data_get.get_is_music_playing)
		{
			SYS_LOG_ERR("get_is_music_playing NULL");
			return;
		}

		u8_t playing = dev_data_get.get_is_music_playing();
		if(playing)
		{
			product_info.product_play_sec ++;
		}
	}
}

static void play_time_thread_timer_cb(struct thread_timer *timer, void* pdata)
{

	if(analy_play_index_p || analy_data_p || analy_play_p
			|| analy_upload_play_p || analy_upload_data_p)
	{
		data_analy_scan_auracast();
		data_analy_scan_eq();
		data_analy_scan_dev_volume_change();
		data_analy_scan_music_playback_with_adapter();
		data_analy_scan_key_press();
		data_analy_scan_smart_ctl_times();
		data_analy_scan_water_proof();

		data_analy_play_scan();

		data_analy_dump();
	}

	product_info_scan();
}


int data_analy_init(data_analy_init_param_t* init_param)
{
	int ret = 0;
	if(!init_param)
	{
		SYS_LOG_ERR("param NULL !!");
		ret = -1;
		return ret;
	}


	if(thread_timer_is_running(&g_data_analy.play_time_thread_timer)|| analy_play_index_p
			|| analy_data_p || analy_play_p || analy_upload_play_p|| analy_upload_data_p)
	{
		SYS_LOG_WRN(" alrealy init!!");
		data_analy_exit();
	}

	g_data_analy.power_on = init_param->power_on;

	memset(&dev_data_get, 0, sizeof(data_analy_get_dev_data_t));
	memcpy(&dev_data_get, &init_param->dev_data_get, sizeof(data_analy_get_dev_data_t));


	memset(&g_data_analy.play.upload, 0, sizeof(play_analytics_t));
	analy_play_p = &g_data_analy.play;
	analy_upload_play_p = &g_data_analy.play.upload;
	analy_play_index_p = &analy_play_p->index;
	nvram_config_get(CFG_DATA_ANALY_PLAY, analy_play_p, sizeof(play_analytics_t));


	memset(&g_data_analy.data, 0, sizeof(data_analytics_t));
	analy_data_p = &g_data_analy.data;
	analy_upload_data_p = &g_data_analy.data.upload;
	nvram_config_get(CFG_DATA_ANALY_DATA, analy_upload_data_p, sizeof(data_analytics_upload_t));

	ret = nvram_config_get(CFG_PRODUCT_INFO, &product_info, sizeof(product_info_t));

	if(g_data_analy.power_on){
		analy_upload_data_p->power_on_times ++;
		analy_data_p->pwr_on_stamp = k_uptime_get();
	}



	thread_timer_init(&g_data_analy.play_time_thread_timer, play_time_thread_timer_cb, NULL);
	thread_timer_start(&g_data_analy.play_time_thread_timer, 1000, 1000);

	return 0;
}



int data_analy_exit(void)
{
	int ret = 0;

	data_analy_update_power_on_sec();

	data_analy_play_scan();// 放器退出时，需要增加play 记录

	ret = nvram_config_set(CFG_DATA_ANALY_DATA, analy_upload_data_p, sizeof(data_analytics_upload_t));
	ret = nvram_config_set(CFG_DATA_ANALY_PLAY, analy_play_p, sizeof(play_analytics_t));
	ret = nvram_config_set(CFG_PRODUCT_INFO, &product_info, sizeof(product_info_t));

	thread_timer_stop(&g_data_analy.play_time_thread_timer);

	analy_data_p = NULL;
	analy_play_p = NULL;

	analy_upload_data_p = NULL;
	analy_upload_play_p = NULL;

	analy_play_index_p = NULL;

	SYS_LOG_INF("");

	return ret;
}

