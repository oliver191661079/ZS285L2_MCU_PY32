/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lemusic.h"
#include "app_tws.h"

#ifdef CONFIG_ACT_EVENT
#include <app_act_event_id.h>
#include <logging/log_core.h>
LOG_MODULE_DECLARE(main, CONFIG_ACT_EVENT_APP_COMPILE_LEVEL);
#endif

static struct lemusic_app_t *p_lemusic = NULL;

/* define BMS iso interval for cis and bis coexist */
#define BMS_ISO_INTERVAL_7_5MS   6
#define BMS_ISO_INTERVAL_10MS    8
#define BMS_ISO_INTERVAL_15MS    12
#define BMS_ISO_INTERVAL_20MS    16
#define BMS_ISO_INTERVAL_30MS    24

#define BMS_DEFAULT_AUDIO_CHAN   1
#define BMS_DEFAULT_KBPS        80//96
#define BMS_DEFAULT_ISO_INTERVAL BMS_ISO_INTERVAL_20MS

static struct bt_broadcast_qos source_qos = {
	.packing = BT_AUDIO_PACKING_INTERLEAVED,
	.framing = BT_AUDIO_UNFRAMED,
	.phy = BT_AUDIO_PHY_2M,
	.rtn = 2,
	.max_sdu = BROADCAST_SDU,
	/* max_transport_latency, unit: ms */
	.latency = BROADCAST_LAT,
	/* sdu_interval, unit: us */
	.interval = BROADCAST_SDU_INTERVAL,
	/* presentation_delay, unit: us */
	.delay = BCST_QOS_DELAY,
	/* processing_time, unit: us */
	.processing = 9000,
};

#ifdef ENABLE_PAWR_APP_XXX
static int pawr_le_handle_response(const uint8_t *buf, uint16_t len)
{
	return pawr_handle_response(AUDIO_STREAM_LE_AUDIO, buf, len);//need check volume value(0-16 or 0-255)
}
#endif

static void lemusic_bms_restart_handler(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	struct app_msg msg = { 0 };

	if (NULL == p_lemusic || !p_lemusic->bms.enable) {
		return;
	}
	struct lemusic_bms_device *bms = &p_lemusic->bms;

	if (bms->restart) {
		SYS_LOG_INF("restart\n");
		msg.type = MSG_LEMUSIC_APP_EVENT;
		msg.cmd = MSG_LEMUSIC_MESSAGE_CMD_PLAYER_RESET;
		send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
		bms->restart = 0;
		bms->restart_count = 0;
	}
}

static void lemusic_bms_start_handler(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	if (NULL == p_lemusic) {
		return;
	}

	SYS_LOG_INF("%d", lemusic_get_auracast_mode());	
	if (1 == lemusic_get_auracast_mode()
		|| 3 == lemusic_get_auracast_mode()) {
		lemusic_bms_source_init();
	}
}

static void lemusic_delay_start(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	SYS_LOG_INF("");

	if (NULL == p_lemusic) {
		return;
	}

	if (lemusic_get_auracast_mode()) {
		if (p_lemusic->bms.player_run && p_lemusic->slave.playback_player_run) {
			SYS_LOG_INF("run already");
			return;
		}
	} else {
		if (p_lemusic->slave.playback_player_run) {
			SYS_LOG_INF("run already");
			return;
		}
	}

	SYS_LOG_INF("restore");
	bt_manager_audio_stream_restore(BT_TYPE_LE);
}

static void lemusic_switch_bmr_handler(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	if (NULL == p_lemusic) {
		return;
	}

	SYS_LOG_INF("stream type %d\n", p_lemusic->bms.cur_stream);

	int8_t temp_role = 0;
#ifdef CONFIG_BT_LETWS
	temp_role = bt_manager_letws_get_dev_role();
#endif

	if (1 == lemusic_get_auracast_mode() && p_lemusic->bms.cur_stream == BT_AUDIO_STREAM_NONE
		&& temp_role == BTSRV_TWS_NONE) {
		if(app_tws_status_get_enable()) {
			lemusic_bms_source_exit();
		} else {
			lemusic_set_auracast_mode(2);
		}
	}
}

#ifdef ENABLE_PAWR_APP_XXX
static int bms_get_pawr_param(struct bt_le_per_adv_param *param)
{
	uint16_t iso_interval = 0;

	if (!param) {
		SYS_LOG_ERR("param NULL\n");
		return -1;
	}

	iso_interval = bt_manager_audio_get_active_channel_iso_interval() / 1250;
	if (!iso_interval) {
		iso_interval = BMS_DEFAULT_ISO_INTERVAL;
	}

	switch(iso_interval) {
		case BMS_ISO_INTERVAL_7_5MS:
		{
			param->interval_min = 72; //90ms
			param->interval_max = 72;
			param->subevent_interval = 24; //30ms
			param->response_slot_delay = 12; //15ms
			break;
		}

		case BMS_ISO_INTERVAL_10MS:
		{
			param->interval_min = 80; //100ms
			param->interval_max = 80;
			param->subevent_interval = 32; //40ms
			param->response_slot_delay = 9; //11.25ms
			break;
		}

		case BMS_ISO_INTERVAL_15MS:
		{
			param->interval_min = 72; //90ms
			param->interval_max = 72;
			param->subevent_interval = 24; //30ms
			param->response_slot_delay = 12; //15ms
			break;
		}

		case BMS_ISO_INTERVAL_20MS:
		{
			param->interval_min = 80; //100ms
			param->interval_max = 80;
			param->subevent_interval = 32; //40ms
			param->response_slot_delay = 9; //11.25ms
			break;
		}

		case BMS_ISO_INTERVAL_30MS:
		{
			param->interval_min = 72; //90ms
			param->interval_max = 72;
			param->subevent_interval = 24; //30ms
			param->response_slot_delay = 12; //15ms
			break;
		}

		default:
			SYS_LOG_ERR("unsupport iso_interval:%d\n", iso_interval);
			return -1;
	}
	return 0;
}
#endif /*ENABLE_PAWR_APP*/

int lemusic_bms_source_init(void)
{
	int ret;
	struct lemusic_bms_device *bms = &p_lemusic->bms;
	struct bt_broadcast_source_create_param* param;

	SYS_LOG_INF("\n");

	if (!p_lemusic)
		return -EINVAL;

	if (thread_timer_is_running(&bms->broadcast_start_timer))
		thread_timer_stop(&bms->broadcast_start_timer);

	if (bms->broadcast_dev_handle) {
		SYS_LOG_WRN("already exist\n");
		return -EINVAL;
	}

	bms->iso_interval = BROADCAST_ISO_INTERVAL;
	bms->use_past = 0;

	param = broadcast_init_source_param();
	if(NULL == param) {
		SYS_LOG_ERR("no source param\n");
		return -EINVAL;
	}

	bms->irc = param->big_param->irc;
#if ENABLE_ENCRYPTION
	memcpy(bms->broadcast_code, param->broadcast_code, 16);
#endif

	param->qos = bms->qos;

	ret = bt_manager_broadcast_source_create(param);
	broadcast_free_source_param(param);
	if (ret < 0) {
		SYS_LOG_ERR("failed");
		thread_timer_start(&bms->broadcast_start_timer, 300, 0);
		return ret;
	}

	bms->broadcast_dev_handle = ret;
	SYS_LOG_INF("dev 0x%x\n", ret);

	thread_timer_init(&bms->restart_timer, lemusic_bms_restart_handler,
			  NULL);
	thread_timer_start(&bms->restart_timer, 200, 200);

	bms->enable = 1;

	return 0;
}

int lemusic_bms_source_exit(void)
{
	struct lemusic_bms_device *bms = &p_lemusic->bms;

	SYS_LOG_INF("\n");

	bt_manager_broadcast_source_disable(bms->broadcast_dev_handle);
	bt_manager_broadcast_source_release(bms->broadcast_dev_handle);

#ifdef ENABLE_PAWR_APP
	//bt_manager_pawr_adv_stop();
#endif

	if (thread_timer_is_running(&bms->restart_timer))
		thread_timer_stop(&bms->restart_timer);
	if (thread_timer_is_running(&bms->broadcast_start_timer))
		thread_timer_stop(&bms->broadcast_start_timer);
	if (thread_timer_is_running(&p_lemusic->delay_start))
		thread_timer_stop(&p_lemusic->delay_start);

	bms->enable = 0;

	return 0;
}

int lemusic_get_auracast_mode(void)
{
	return system_app_get_auracast_mode();
}

void lemusic_set_auracast_mode(int mode)
{
	if (!p_lemusic)
		return;

	SYS_LOG_INF("mode:%d\n", mode);

	if (thread_timer_is_running(&p_lemusic->switch_bmr_timer)) {
		thread_timer_stop(&p_lemusic->switch_bmr_timer);
	}

	if (mode == lemusic_get_auracast_mode()) {
		return;
	}

	SYS_EVENT_INF(EVENT_LEMUSIC_AURACAST_MODE, mode);
	if (!mode) {
		lemusic_bms_source_exit();
		system_app_set_auracast_mode(0);
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		lemusic_update_dvfs(LE_FREQ);
#endif
	} else if (mode == 1) {
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		lemusic_update_dvfs(LE_BCST_FREQ);
#endif
		system_app_set_auracast_mode(1);
		lemusic_bms_source_init();

	} else {
		system_app_set_auracast_mode(mode);
		system_app_launch_switch(DESKTOP_PLUGIN_ID_LE_MUSIC,DESKTOP_PLUGIN_ID_BMR);
	}
}

void lemusic_bms_player_reset_trigger(void)
{
	struct lemusic_bms_device *bms = &p_lemusic->bms;

	if (p_lemusic != NULL) {
		bms->restart = 1;
		bms->restart_count++;
		SYS_LOG_INF("%d", bms->restart_count);
	}
}

static int _lemusic_init(void *p1, void *p2, void *p3)
{
	if (p_lemusic) {
		return 0;
	}

	SYS_LOG_INF("\n");

	p_lemusic = app_mem_malloc(sizeof(struct lemusic_app_t));
	if (!p_lemusic) {
		SYS_LOG_ERR("malloc failed!\n");
		return -ENOMEM;
	}

#if 0
	bt_manager_set_user_visual(1,0,0,BTSRV_SCAN_MODE_DEFAULT_INQUIRY_PAGE);
	btif_br_auto_reconnect_stop(BTSRV_STOP_AUTO_RECONNECT_ALL);
	btif_br_disconnect_device(BTSRV_DISCONNECT_PHONE_MODE);
#endif

	lemusic_view_init();
	struct lemusic_bms_device *bms = &p_lemusic->bms;

	memset(p_lemusic, 0, sizeof(struct lemusic_app_t));
	bms->qos = &source_qos;
	thread_timer_init(&p_lemusic->switch_bmr_timer, lemusic_switch_bmr_handler, NULL);
	thread_timer_init(&bms->broadcast_start_timer, lemusic_bms_start_handler,
					  NULL);
	thread_timer_init(&p_lemusic->delay_start, lemusic_delay_start, NULL);

#ifdef CONFIG_PLAYTTS
	if(tts_manager_is_playing()){
		p_lemusic->tts_playing = 1;
	}
#endif

	if (lemusic_get_auracast_mode()) {
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		lemusic_update_dvfs(LE_BCST_FREQ);
#endif
		system_app_set_auracast_mode(1);
		lemusic_bms_source_init();
	}else{
		thread_timer_start(&p_lemusic->delay_start, 100, 0);
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		lemusic_update_dvfs(LE_FREQ);
#endif
	}

	SYS_LOG_INF("init ok\n");
	return 0;
}

static int  _lemusic_exit(void)
{
	SYS_LOG_INF("\n");
	if (!p_lemusic) {
		goto exit;
	}

	if (thread_timer_is_running(&p_lemusic->monitor_timer)) {
		thread_timer_stop(&p_lemusic->monitor_timer);
	}

	if (thread_timer_is_running(&p_lemusic->switch_bmr_timer)) {
		thread_timer_stop(&p_lemusic->switch_bmr_timer);
#if 0
		//keep auracast role when exit lemusic
		lemusic_switch_bmr_handler(NULL, NULL);
#endif
	}

#if ENABLE_PADV_APP
	padv_tx_deinit();
#endif

	lemusic_bms_stop_capture();
	lemusic_bms_exit_capture();
	lemusic_bms_source_exit();

	lemusic_stop_playback();
	lemusic_exit_playback();

	lemusic_stop_capture();
	lemusic_exit_capture();

	lemusic_set_sink_chan_stream(NULL);

	lemusic_view_deinit();

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	lemusic_clear_current_dvfs();
#endif

	app_mem_free(p_lemusic);

	p_lemusic = NULL;

#ifdef CONFIG_PROPERTY
	//property_flush_req(NULL);
#endif

exit:
	SYS_LOG_INF("exit ok\n");

	return 0;
}

struct lemusic_app_t *lemusic_get_app(void)
{
	return p_lemusic;
}

static int _lemusic_proc_msg(struct app_msg *msg)
{
	SYS_LOG_INF("type %d, value %d\n", msg->type, msg->value);
	switch (msg->type) {
	case MSG_BT_EVENT:
		lemusic_bt_event_proc(msg);
		break;
	case MSG_INPUT_EVENT:
		lemusic_input_event_proc(msg);
		break;
#ifdef CONFIG_PLAYTTS
	case MSG_TTS_EVENT:
		lemusic_tts_event_proc(msg);
		break;
#endif
	case MSG_LEMUSIC_APP_EVENT:
		lemusic_app_event_proc(msg);
		break;
#ifdef CONFIG_BT_LETWS
	case MSG_TWS_EVENT:
		lemusic_tws_event_proc(msg);
		break;
#endif
	case MSG_EXIT_APP:
		_lemusic_exit();
		break;
	default:
		break;
	}
	return 0;
}

static int _lemusic_dump_app_state(void)
{
	print_buffer_lazy(APP_ID_LEMUSIC, (void *)lemusic_get_app(),
					  sizeof(struct lemusic_app_t));
	return 0;
}

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
static void get_dvfs_info(uint8_t dvfs, uint8_t *dvfs_info)
{
	if (dvfs == LE_FREQ) {
		memcpy(dvfs_info, "lemusic", 7);
	} else if (dvfs == LE_BCST_FREQ_HIGH) {
		memcpy(dvfs_info, "bms_le_H", 8);
	} else {
		memcpy(dvfs_info, "bms_le", 6);
	}
}

void lemusic_update_dvfs(uint8_t dvfs)
{
	if (!dvfs || p_lemusic->set_dvfs_level == dvfs) {
		SYS_LOG_INF("%d\n",dvfs);
		return;
	}

	uint8_t dvfs_info[10] = {0};
	/*set new dvfs*/
	get_dvfs_info(dvfs, dvfs_info);
	soc_dvfs_set_level(dvfs, dvfs_info);

	if (p_lemusic->set_dvfs_level != dvfs
		&& p_lemusic->set_dvfs_level != 0) {
		/*unset current dvfs*/
		get_dvfs_info(p_lemusic->set_dvfs_level, dvfs_info);
		soc_dvfs_unset_level(p_lemusic->set_dvfs_level, dvfs_info);
	}
	p_lemusic->set_dvfs_level = dvfs;
}

void lemusic_clear_current_dvfs(void)
{
	uint8_t dvfs_info[10] = {0};

	if (p_lemusic->set_dvfs_level != 0) {
		get_dvfs_info(p_lemusic->set_dvfs_level, dvfs_info);
		soc_dvfs_unset_level(p_lemusic->set_dvfs_level, dvfs_info);
		p_lemusic->set_dvfs_level = 0;
	}
}
#endif /*CONFIG_SOC_DVFS_DYNAMIC_LEVEL*/

DESKTOP_PLUGIN_DEFINE(DESKTOP_PLUGIN_ID_LE_MUSIC, _lemusic_init, _lemusic_exit, _lemusic_proc_msg, \
	_lemusic_dump_app_state, NULL, NULL, NULL);
