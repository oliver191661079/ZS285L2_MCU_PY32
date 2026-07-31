/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lemusic.h"
#include <app_tws.h>

#ifdef CONFIG_ACT_EVENT
#include <app_act_event_id.h>
#include <logging/log_core.h>
LOG_MODULE_DECLARE(main, CONFIG_ACT_EVENT_APP_COMPILE_LEVEL);
#endif

#define RELAY_DISABLED	1

static u8_t auarcast_hq_switch_sq = 0;

static int lemusic_stream_start(void);

static int lemusic_sync_volume(void)
{
	int value;

	SYS_LOG_INF("\n");

	value = system_volume_get(AUDIO_STREAM_LE_AUDIO);
	if (value < 0) {
		SYS_LOG_ERR("volume: %d\n", value);
		return -EINVAL;
	}

	bt_manager_volume_client_set(bt_manager_music_to_lea_volume(value));

	value = audio_system_get_master_mute();
	if (value < 0) {
		SYS_LOG_ERR("mute: %d\n", value);
		return -EINVAL;
	}

	if (value) {
		bt_manager_volume_client_mute();
	} else {
		bt_manager_volume_client_unmute();
	}

	return 0;
}

static void lemusic_tx_start(uint16_t handle, uint8_t id, uint16_t audio_handle)
{
	struct lemusic_slave_device *slave;

	if (!lemusic_get_app()) {
		return;
	}

	slave = &lemusic_get_app()->slave;

	if (slave->adc_to_bt_started) {
		return;
	}

	if (slave->bt_source_started) {
		media_player_trigger_audio_record_start(slave->capture_player);
		slave->adc_to_bt_started = 1;
		//Time sensitive, short log to save time.
		printk("["SYS_LOG_DOMAIN"] trigger record\n");
	}
}

static void lemusic_rx_start(uint16_t handle, uint8_t id, uint16_t audio_handle)
{
	struct lemusic_slave_device *slave;
	struct lemusic_bms_device *bms;
#ifdef BIS_DELAY_BY_RX_START
	uint32_t now_time = k_cycle_get_32();
#endif

	if (!lemusic_get_app()) {
		return;
	}

	bms = &lemusic_get_app()->bms;
	slave = &lemusic_get_app()->slave;

	if (slave->bt_to_dac_started) {
		return;
	}

	if (lemusic_get_auracast_mode()
		 && (!bms->broadcast_source_enabled)) {
		return;
	}

	if (slave->playback_player && slave->playback_player_run) {
#ifdef BIS_DELAY_BY_RX_START
		if (lemusic_get_auracast_mode()) {
			if (!bms->time_update) {
				bms->last_time = now_time;
				bms->time_update = 1;
				printk("rx start time:%u\n",bms->last_time);
				return;
			}

			uint8_t time_diff = (now_time - bms->last_time) / 24000;
			uint8_t bis_delay = bms->qos->latency + bms->qos->delay / 1000;
			uint8_t delta = (time_diff < bis_delay) ?
							(bis_delay - time_diff) : (time_diff - bis_delay);
			delta = ((uint32_t)bms->iso_interval * 1250 / 1000 > delta) ? 0 : 1;
			//printk("rx start last:%u,now:%u,time_diff:%u,bis_delay:%u\n",
			//	bms->last_time, now_time, time_diff, bis_delay);
			if (time_diff < bis_delay && delta) {
				return;
			}
		}
#endif
		struct audio_track_t * track;
		track = audio_system_get_audio_track_handle(AUDIO_STREAM_LE_AUDIO);
		if(track){
			if (lemusic_get_auracast_mode()) {
				slave->bt_to_dac_started = 1;
				return;
			} else {
				/*Need trigger start when "init_param.waitto_start == 1" in media init playback */
				int ret = media_player_audio_track_trigger_callback(track,1);
				if (ret) {
					printk("rx start err:%d\n", ret);
					return;
				}
				slave->bt_to_dac_started = 1;
				//Time sensitive, short log to save time.
				printk("["SYS_LOG_DOMAIN"] trigger track\n");
			}
		}
	}
}

static int lemusic_stream_config_codec(void)
{
#if RELAY_DISABLED
	return 0;
#else
	struct lemusic_master_device *master = &lemusic_get_app()->master;

	/* filter if not ready */
	if (!master->source_chan.handle || !master->codec.format) {
		SYS_LOG_INF("not ready\n");
		return -EINVAL;
	}

	SYS_LOG_INF("\n");

	return bt_manager_audio_stream_config_codec(&master->codec);
#endif
}

static int lemusic_stream_config_qos(void)
{
#if RELAY_DISABLED
	return 0;
#else
	struct lemusic_master_device *master = &lemusic_get_app()->master;

	/* filter if not ready */
	if (!master->qoss.num_of_qos || !master->qoss.interval_m) {
		SYS_LOG_INF("not ready\n");
		return -EINVAL;
	}

	SYS_LOG_INF("\n");

	return bt_manager_audio_stream_config_qos((struct bt_audio_qoss *)&master->qoss);
#endif
}

/* start master's source stream */
static int lemusic_stream_start(void)
{
#if RELAY_DISABLED
	return 0;
#else
	struct lemusic_master_device *master = &lemusic_get_app()->master;
	struct lemusic_slave_device *slave = &lemusic_get_app()->slave;

	if (master->bt_source_pending) {
		SYS_LOG_INF("pending\n");
		master->bt_source_pending_start = 1;
		return 0;
	}

	if (master->bt_source_enabled) {
		SYS_LOG_INF("already\n");
		return 0;
	}

	if (!slave->bt_sink_enabled) {
		SYS_LOG_INF("slave not enabled\n");
		return 0;
	}

	/* skip if not configured */
	if (!master->bt_source_configured) {
		SYS_LOG_INF("bt not configured\n");
		return 0;
	}

	if (master->bt_source_disallowed) {
		SYS_LOG_INF("disallowed\n");
		return 0;
	}

	master->bt_source_pending = 1;
	master->bt_source_pending_start = 1;

	SYS_LOG_INF("0x%x\n", master->audio_contexts);

	return bt_manager_audio_stream_enable_single(&master->source_chan,
					master->audio_contexts);
#endif
}

/* stop master's source stream */
static int lemusic_stream_stop(void)
{
#if RELAY_DISABLED
	return 0;
#else
	struct lemusic_master_device *master = &lemusic_get_app()->master;

	if (master->bt_source_pending) {
		SYS_LOG_INF("pending\n");
		master->bt_source_pending_start = 0;
		return 0;
	}

	if (!master->bt_source_configured || !master->bt_source_enabled) {
		SYS_LOG_INF("bt not enabled\n");
		return 0;
	}

	master->bt_source_pending = 1;
	master->bt_source_pending_start = 0;

	SYS_LOG_INF("\n");

	return bt_manager_audio_stream_disable_single(&master->source_chan);
#endif
}

/* update master's source stream */
static int lemusic_stream_update(void)
{
#if RELAY_DISABLED
	return 0;
#else
	struct lemusic_master_device *master = &lemusic_get_app()->master;

	if (!master->bt_source_enabled) {
		SYS_LOG_INF("bt not enabled\n");
		return 0;
	}

	SYS_LOG_INF("\n");

	return bt_manager_audio_stream_update_single(&master->source_chan,
					master->audio_contexts);
#endif
}

static int lemusic_discover_endpoints(struct bt_audio_endpoints *endps)
{
	struct lemusic_master_device *master = &lemusic_get_app()->master;
	struct bt_audio_endpoint *endp;
	struct bt_audio_codec *codec;
	struct bt_audio_chan *chan;
	uint8_t i, found;

	/* NOTE: only LE master will receive DISCOVER_ENDPOINTS */

	SYS_LOG_INF("%d\n", endps->num_of_endp);

	/* find 1st sink endpoint */
	for (i = 0, found = 0; i < endps->num_of_endp; i++) {
		endp = &endps->endp[i];
		if (endp->dir == BT_AUDIO_DIR_SINK) {
			found = 1;
			break;
		}
	}

	if (found == 0) {
		SYS_LOG_ERR("no sink endpoint");
		return -EINVAL;
	}

	chan = &master->source_chan;
	chan->handle = endps->handle;
	chan->id = endp->id;

	codec = &master->codec;
	codec->handle = endps->handle;
	codec->id = endp->id;
	codec->dir = BT_AUDIO_DIR_SINK;

	SYS_LOG_INF("sink endp: %d\n", chan->id);

	return lemusic_stream_config_codec();
}

static int lemusic_handle_config_codec(struct bt_audio_codec *codec)
{
	struct lemusic_master_device *master = &lemusic_get_app()->master;
	struct bt_audio_codec *store_codec = &master->codec;
	int dir;

	if (bt_manager_audio_get_type(codec->handle) != BT_TYPE_LE) {
		return 0;
	}

	/* NOTE: only slave will receive STREAM_CONFIG_CODEC */

	dir = bt_manager_audio_stream_dir(codec->handle, codec->id);
	if (dir != BT_AUDIO_DIR_SINK) {
		return 0;
	}

	SYS_EVENT_INF(EVENT_LEMUSIC_CONFIG_CODEC,codec->sample_rate,codec->locations);

	store_codec->format = codec->format;
	store_codec->target_latency = codec->target_latency;
	store_codec->target_phy = codec->target_phy;
	store_codec->frame_duration = codec->frame_duration;
	store_codec->blocks = codec->blocks;
	store_codec->sample_rate = codec->sample_rate;
	store_codec->octets = codec->octets;
	store_codec->locations = codec->locations;

	return lemusic_stream_config_codec();
}

static int lemusic_handle_prefer_qos(struct bt_audio_preferred_qos *prefer)
{
	struct lemusic_master_device *master = &lemusic_get_app()->master;
	struct bt_audio_chan *source_chan = &master->source_chan;
	struct bt_audio_qoss_1 *qoss = &master->qoss;
	struct bt_audio_qos *qos = &qoss->qos[0];
	int state;

	/* NOTE: only master will receive STREAM_PREFER_QOS */

	if ((prefer->handle == source_chan->handle) &&
		(prefer->id == source_chan->id)) {
		SYS_LOG_INF("source\n");
	} else {
		return -EINVAL;
	}

	state = bt_manager_audio_stream_state(source_chan->handle,
					source_chan->id);
	if (state != BT_AUDIO_STREAM_CODEC_CONFIGURED) {
		return 0;
	}

	/* filter for duplicate config_qos */
	if (!qoss->interval_m) {
		return 0;
	}

	qoss->num_of_qos = 1;
	qoss->sca = BT_GAP_SCA_UNKNOWN;
	qoss->packing = BT_AUDIO_PACKING_SEQUENTIAL;
	qos->handle = source_chan->handle;
	qos->id_m_to_s = source_chan->id;

	bt_manager_audio_le_vnd_send(prefer->handle, "LEA lemusic", 10);

	return lemusic_stream_config_qos();
}

/* find the existed sink chan */
static struct bt_audio_chan *find_sink_chan(uint16_t handle, uint8_t id)
{
	struct lemusic_slave_device *slave = &lemusic_get_app()->slave;
	struct bt_audio_chan *chan;
	int i;

	for (i = 0; i < NUM_OF_SINK_CHAN; i++) {
		chan = &slave->sink_chans[i];
		if (chan->handle == handle && chan->id == id) {
			return chan;
		}
	}

	return NULL;
}

/* find a new sink chan */
static struct bt_audio_chan *new_sink_chan(uint16_t handle, uint8_t id)
{
	struct lemusic_slave_device *slave = &lemusic_get_app()->slave;
	struct bt_audio_chan *chan;
	int i;

	for (i = 0; i < NUM_OF_SINK_CHAN; i++) {
		chan = &slave->sink_chans[i];
		if (chan->handle == 0 && chan->id == 0) {
			return chan;
		}
	}

	return NULL;
}

static int lemusic_handle_config_qos(struct bt_audio_qos_configured *qos)
{
	struct lemusic_master_device *master = &lemusic_get_app()->master;
	int dir;

	if (bt_manager_audio_get_type(qos->handle) != BT_TYPE_LE) {
		return 0;
	}

	if (qos->handle == master->source_chan.handle &&
		qos->id == master->source_chan.id) {
		master->bt_source_configured = 1;

		SYS_LOG_INF("master\n");

		return lemusic_stream_start();
	}

	/* slave */
	SYS_LOG_INF("slave\n");

	dir = bt_manager_audio_stream_dir(qos->handle, qos->id);
	SYS_EVENT_INF(EVENT_LEMUSIC_CONFIG_QOS,dir,qos->id,qos->latency,qos->rtn);

	if (dir == BT_AUDIO_DIR_SINK) {
		/* set QoS as master */
		master->qoss.framing = qos->framing;
		master->qoss.interval_m = qos->interval;
		master->qoss.delay_m = qos->delay;
		master->qoss.latency_m = qos->latency;
		master->qoss.qos[0].max_sdu_m = qos->max_sdu;
		master->qoss.qos[0].phy_m = qos->phy;
		master->qoss.qos[0].rtn_m = qos->rtn;

		lemusic_stream_config_qos();
	}

	SYS_LOG_INF("handle: %d, id: %d, dir: %d rtn: %d\n", qos->handle,
		qos->id, dir,qos->rtn);

	return 0;
}

static int lemusic_handle_enable(struct bt_audio_report *rep)
{
	struct lemusic_master_device *master = &lemusic_get_app()->master;
	struct lemusic_slave_device *slave = &lemusic_get_app()->slave;
	struct lemusic_bms_device *bms = &lemusic_get_app()->bms;
	struct bt_audio_chan *source_chan = NULL;
	struct bt_audio_chan *sink_chan = NULL;

	SYS_LOG_INF("h=0x%x, id=%d\n", rep->handle, rep->id);

	if ((rep->handle == master->source_chan.handle) &&
		(rep->id == master->source_chan.id)) {
		master->bt_source_enabled = 1;

		SYS_LOG_INF("master\n");

		/* master->bt_source_pending is 1 */
		master->bt_source_pending = 0;
		if (!master->bt_source_pending_start) {
			lemusic_stream_stop();
		} else {
			lemusic_sync_volume();
		}
		return 0;
	}

	/* slave */
	SYS_LOG_INF("slave\n");

	int dir = bt_manager_audio_stream_dir(rep->handle, rep->id);
	SYS_EVENT_INF(EVENT_LEMUSIC_ENABLE,dir,rep->handle,rep->id,lemusic_get_app()->tts_playing);

	if (dir == BT_AUDIO_DIR_SINK) {
		sink_chan = find_sink_chan(rep->handle, rep->id);
		if (!sink_chan) {
			sink_chan = new_sink_chan(rep->handle, rep->id);
			if (!sink_chan) {
				SYS_LOG_ERR("no sink chan handle: %d, id: %d\n",
					rep->handle, rep->id);
				return -EINVAL;
			}

			sink_chan->handle = rep->handle;
			sink_chan->id = rep->id;
			slave->num_of_sink_chan++;
			if (!slave->active_sink_chan) {
				slave->active_sink_chan = sink_chan;
				//uint8_t ret = bt_manager_lea_set_active_device(rep->handle);
				SYS_LOG_INF("act dev:0x%x\n", rep->handle);
			} else if(sink_chan->handle != slave->active_sink_chan->handle) {
				sink_chan->handle = 0;
				sink_chan->id = 0;
				SYS_LOG_ERR("err hdl:0x%x,active hdl:0x%x\n",
					rep->handle, slave->active_sink_chan->handle);
				return -EINVAL;
			}
			bt_manager_audio_stream_cb_register(sink_chan, lemusic_rx_start, NULL);
		}
	} else if (dir == BT_AUDIO_DIR_SOURCE) {
		source_chan = &slave->source_chan;
		if(!source_chan->handle){
			source_chan->handle = rep->handle;
			source_chan->id = rep->id;
			bt_manager_audio_stream_cb_register(source_chan, lemusic_tx_start,
							NULL);
		}
	}

	if (thread_timer_is_running(&lemusic_get_app()->delay_start)){
		SYS_LOG_INF("wait for restore\n");
		return 0;
	}

	if (lemusic_get_auracast_mode() && !bms->chan) {
		SYS_LOG_ERR("broad_chan not config\n");
		thread_timer_start(&bms->broadcast_start_timer, 300, 0);
		return -EINVAL;
	}

#ifdef CONFIG_PLAYTTS
	if (sink_chan) {
		//tts_manager_enable_filter();
	}

	if (lemusic_get_app()->tts_playing) {
		SYS_LOG_INF("tts\n");
		return 0;
	}
#endif


	if (sink_chan) {
		SYS_LOG_INF("sink 0x%x\n", rep->audio_contexts);
		uint8_t capture_reinit = 0;

		if (lemusic_get_auracast_mode()) {
			if (thread_timer_is_running(&lemusic_get_app()->switch_bmr_timer))
				thread_timer_stop(&lemusic_get_app()->switch_bmr_timer);

			if (bms->broadcast_source_enabled && bms->player_run) {
				lemusic_bms_stop_capture();
				lemusic_bms_exit_capture();
				capture_reinit = 1;
				lemusic_set_sink_chan_stream(NULL);
			}
		}

		lemusic_init_playback();
#if 0
		lemusic_init_capture();
#endif
		/* start playback earlier */
		lemusic_start_playback();
		slave->bt_sink_enabled = 1;

		if (capture_reinit) {
			lemusic_bms_init_capture();
			lemusic_bms_start_capture();
			lemusic_set_sink_chan_stream(slave->sink_stream);
		}

		/* initialize audio_contexts and start */
		master->audio_contexts = rep->audio_contexts;
		return lemusic_stream_start();
	}

	source_chan = &slave->source_chan;

	if ((rep->handle == source_chan->handle) &&
		(rep->id == source_chan->id)) {
		SYS_LOG_INF("source\n");
		lemusic_init_capture();
#if 0	// TODO: do not init playback
		lemusic_init_playback(1);
#endif
		/* if slave's source stream enabled */
		master->bt_source_disallowed = 1;

		/* filter if master's source channel not configured */
		if (!master->bt_source_configured) {
			SYS_LOG_INF("master not configured\n");
			return 0;
		}

		/* disable master's stream */
		return lemusic_stream_stop();
	}

	return -EINVAL;
}

static int lemusic_handle_update(struct bt_audio_report *rep)
{
	struct lemusic_master_device *master = &lemusic_get_app()->master;
	struct bt_audio_chan *sink_chan;

	SYS_LOG_INF("h=0x%x, id=%d\n", rep->handle, rep->id);

	if ((rep->handle == master->source_chan.handle) &&
		(rep->id == master->source_chan.id)) {
		SYS_LOG_INF("master\n");
		return 0;
	}

	/* slave */
	SYS_LOG_INF("slave\n");

	if (lemusic_get_app()->tts_playing) {
		SYS_LOG_INF("tts\n");
		return 0;
	}

	sink_chan = find_sink_chan(rep->handle, rep->id);
	if (sink_chan) {
		SYS_LOG_INF("sink\n");

		/* update audio_contexts */
		master->audio_contexts = rep->audio_contexts;
		lemusic_stream_update();
	}

	return 0;
}

static int lemusic_handle_disable(struct bt_audio_report *rep)
{
	struct lemusic_master_device *master = &lemusic_get_app()->master;
	struct lemusic_slave_device *slave = &lemusic_get_app()->slave;
	struct bt_audio_chan *sink_chan;

	SYS_LOG_INF("h=0x%x, id=%d\n", rep->handle, rep->id);

	if ((rep->handle == master->source_chan.handle) &&
		(rep->id == master->source_chan.id)) {
		SYS_LOG_INF("master\n");

		master->bt_source_enabled = 0;
		return 0;
	}

	sink_chan = find_sink_chan(rep->handle, rep->id);
	if (sink_chan) {
		/* slave */
		SYS_LOG_INF("slave\n");
		SYS_EVENT_INF(EVENT_LEMUSIC_DISABLE,rep->handle,rep->id);

		slave->bt_sink_enabled = 0;
		// TODO: no need to stop actually any more ...
#if 0
		bt_manager_audio_sink_stream_stop_single(&slave->sink_chan);
#endif
		lemusic_stream_stop();
	}

	return 0;
}

static int lemusic_handle_start(struct bt_audio_report *rep)
{
	struct lemusic_master_device *master = &lemusic_get_app()->master;
	struct lemusic_slave_device *slave = &lemusic_get_app()->slave;
	struct bt_audio_chan *source_chan;
	struct lemusic_bms_device *bms = &lemusic_get_app()->bms;

	SYS_LOG_INF("h=0x%x, id=%d\n", rep->handle, rep->id);

	if ((rep->handle == master->source_chan.handle) &&
		(rep->id == master->source_chan.id)) {
		SYS_LOG_INF("master\n");

		master->bt_source_started = 1;
		return 0;
	}

	/* slave */
	SYS_LOG_INF("slave\n");
	SYS_EVENT_INF(EVENT_LEMUSIC_START,rep->handle,rep->id,lemusic_get_app()->tts_playing);


	if ((rep->handle == slave->active_sink_chan->handle) &&
		(rep->id == slave->active_sink_chan->id)) {
		lemusic_get_app()->playing = 1;
#if ENABLE_BROADCAST
		if (lemusic_get_auracast_mode()) {
			if (!bms->chan) {
				SYS_LOG_ERR("broad_chan not config\n");
				return -EINVAL;
			}

			if (!bms->broadcast_source_enabled) {
//#ifdef CONFIG_BT_MUSIC_AUTO_SWITCH_BMR
				bt_manager_broadcast_source_enable(bms->broad_chan[0].handle);
//#endif
			} else if (!bms->player_run) {
				lemusic_bms_init_capture();
				lemusic_bms_start_capture();
				lemusic_set_sink_chan_stream(slave->sink_stream);
			}
		}
#endif /*ENABLE_BROADCAST*/
	}

	if (lemusic_get_app()->tts_playing) {
		SYS_LOG_INF("tts\n");
		return 0;
	}

	source_chan = &slave->source_chan;
	SYS_LOG_INF("h=0x%x, id=%d\n", source_chan->handle, source_chan->id);

	if ((rep->handle == source_chan->handle) &&
		(rep->id == source_chan->id)) {
		lemusic_start_capture();
		slave->bt_source_started = 1;
		return 0;
	}

	return 0;
}

static int lemusic_handle_stop(struct bt_audio_report *rep)
{
	struct lemusic_master_device *master = &lemusic_get_app()->master;
	struct lemusic_slave_device *slave = &lemusic_get_app()->slave;
	struct bt_audio_chan *sink_chan;

	SYS_LOG_INF("h=0x%x, id=%d\n", rep->handle, rep->id);

	if ((rep->handle == master->source_chan.handle) &&
		(rep->id == master->source_chan.id)) {
		SYS_LOG_INF("master\n");

		master->bt_source_started = 0;

		if (master->bt_source_pending) {
			master->bt_source_pending = 0;
			if (master->bt_source_pending_start) {
				lemusic_stream_start();
				return 0;
			}
		}

		/* disconnect master's stream */
		bt_manager_audio_stream_disconnect_single(&master->source_chan);
		return 0;
	}

	if ((rep->handle == slave->source_chan.handle) &&
		(rep->id == slave->source_chan.id)) {
		SYS_LOG_INF("slave source\n");
		SYS_EVENT_INF(EVENT_LEMUSIC_STOP,rep->handle,rep->id);

		slave->bt_source_started = 0;
		lemusic_stop_capture();
		lemusic_exit_capture();
	}

	sink_chan = find_sink_chan(rep->handle, rep->id);
	if (sink_chan) {
		SYS_LOG_INF("slave sink\n");
		SYS_EVENT_INF(EVENT_LEMUSIC_STOP,rep->handle,rep->id);

		lemusic_stop_playback();
		lemusic_exit_playback();

		if((slave->active_sink_chan)
			&& (sink_chan->handle == slave->active_sink_chan->handle)) {
			slave->active_sink_chan = NULL;
			lemusic_get_app()->playing = 0;
#if ENABLE_BROADCAST
			if (lemusic_get_auracast_mode()) {
				lemusic_bms_stop_capture();
				lemusic_bms_exit_capture();
				lemusic_set_sink_chan_stream(NULL);
				thread_timer_start(&lemusic_get_app()->switch_bmr_timer, 1000, 0);
			}
#endif /*ENABLE_BROADCAST*/
		}

		slave->num_of_sink_chan--;
		sink_chan->handle = 0;
		sink_chan->id = 0;
#ifdef CONFIG_PLAYTTS
		//tts_manager_disable_filter();
#endif
	}

	return 0;
}

static int lemusic_handle_release(struct bt_audio_report *rep)
{
	struct lemusic_master_device *master = &lemusic_get_app()->master;
	struct lemusic_slave_device *slave = &lemusic_get_app()->slave;
	struct bt_audio_chan *sink_chan;

	SYS_LOG_INF("h=0x%x, id=%d\n", rep->handle, rep->id);

	if ((rep->handle == master->source_chan.handle) &&
		(rep->id == master->source_chan.id)) {
		SYS_LOG_INF("master\n");

		master->bt_source_configured = 0;
		master->bt_source_enabled = 0;
		master->bt_source_started = 0;
		return 0;
	}

	if ((rep->handle == slave->source_chan.handle) &&
		(rep->id == slave->source_chan.id)) {
		SYS_LOG_INF("slave source\n");
		SYS_EVENT_INF(EVENT_LEMUSIC_RELEASE,rep->handle,rep->id);

		slave->bt_source_started = 0;
		lemusic_stop_capture();
		lemusic_exit_capture();
	}

	sink_chan = find_sink_chan(rep->handle, rep->id);
	if (sink_chan) {
		SYS_LOG_INF("slave sink\n");
		SYS_EVENT_INF(EVENT_LEMUSIC_RELEASE,rep->handle,rep->id);

		lemusic_stop_playback();
		lemusic_exit_playback();

		if((slave->active_sink_chan)
			&& (sink_chan->handle == slave->active_sink_chan->handle)) {
			slave->active_sink_chan = NULL;
			lemusic_get_app()->playing = 0;
#if ENABLE_BROADCAST
			if (lemusic_get_auracast_mode()) {
				lemusic_bms_stop_capture();
				lemusic_bms_exit_capture();
				lemusic_set_sink_chan_stream(NULL);
				thread_timer_start(&lemusic_get_app()->switch_bmr_timer, 1000, 0);
			}
#endif /*ENABLE_BROADCAST*/
		}

		slave->num_of_sink_chan--;
		slave->bt_sink_enabled = 0;
		sink_chan->handle = 0;
		sink_chan->id = 0;
#ifdef CONFIG_PLAYTTS
//      	tts_manager_disable_filter();
#endif
	}

	return 0;
}

static int lemusic_restore_codec(struct bt_audio_chan_info *info)
{
	struct lemusic_master_device *master = &lemusic_get_app()->master;
	struct bt_audio_codec *store_codec = &master->codec;

	store_codec->format = info->format;
	store_codec->target_latency = info->target_latency;
	store_codec->target_phy = BT_AUDIO_TARGET_PHY_2M;;
	store_codec->frame_duration = bt_frame_duration_from_ms(info->duration);
	store_codec->blocks = 1;
	store_codec->sample_rate = info->sample;
	store_codec->octets = info->octets;
	store_codec->locations = info->locations;

	return lemusic_stream_config_codec();
}

static int lemusic_restore_qos(struct bt_audio_chan_info *info)
{
	struct lemusic_master_device *master = &lemusic_get_app()->master;

	/* set QoS as master */
	master->qoss.framing = info->framing;
	master->qoss.interval_m = info->interval;
	master->qoss.delay_m = info->delay;
	master->qoss.latency_m = info->max_latency;
	master->qoss.qos[0].max_sdu_m = info->sdu;
	master->qoss.qos[0].phy_m = info->phy;
	master->qoss.qos[0].rtn_m = info->rtn;

	return lemusic_stream_config_qos();
}

static int lemusic_restore_state(struct bt_audio_chan *chan, bool sink)
{
	struct lemusic_master_device *master = &lemusic_get_app()->master;
	struct bt_audio_chan_info info;
	struct bt_audio_report rep;
	int state;
	int ret;

	ret = bt_manager_audio_stream_info(chan, &info);
	if (ret < 0) {
		return ret;
	}

	state = bt_manager_audio_stream_state(chan->handle, chan->id);
	if (state < 0) {
		return ret;
	}

	/* NOTE: restore codec and qos for sink_chan only */

	SYS_LOG_INF("state: %d\n", state);
	switch (state) {
	case BT_AUDIO_STREAM_CODEC_CONFIGURED:
		if (!sink) {
			break;
		}
		lemusic_restore_codec(&info);
		break;
	case BT_AUDIO_STREAM_QOS_CONFIGURED:
		if (!sink) {
			break;
		}
		lemusic_restore_codec(&info);
		lemusic_restore_qos(&info);
		break;
	case BT_AUDIO_STREAM_ENABLED:
		rep.handle = chan->handle;
		rep.id = chan->id;
		rep.audio_contexts = info.contexts;
		if (sink) {
			lemusic_restore_codec(&info);
			lemusic_restore_qos(&info);
			master->audio_contexts = info.contexts;
		}
		lemusic_handle_enable(&rep);
		break;
	case BT_AUDIO_STREAM_STARTED:
		rep.handle = chan->handle;
		rep.id = chan->id;
		rep.audio_contexts = info.contexts;
		if (sink) {
			lemusic_restore_codec(&info);
			lemusic_restore_qos(&info);
			master->audio_contexts = info.contexts;
		}

		/* FIXME: sink_chan no need to stream_start again! */
		lemusic_handle_enable(&rep);
		lemusic_handle_start(&rep);
		break;
	default:
		break;
	}

	return 0;
}

static int lemusic_handle_restore(struct bt_audio_chan_restore *res)
{
	struct lemusic_slave_device *slave = &lemusic_get_app()->slave;
	struct bt_audio_chan *chan;
	uint8_t i;
	int dir;
	int state;

	if (!res) {
		SYS_LOG_ERR("NULL");
		return -EINVAL;
	}

	/* NOTICE: res->count should be 0/1/2/3 for now */
	SYS_LOG_INF("count: %d\n", res->count);
	SYS_EVENT_INF(EVENT_LEMUSIC_RESTORE,res->count);

	lemusic_stop_playback();
	lemusic_exit_playback();

	lemusic_stop_capture();
	lemusic_exit_capture();

	memset(slave, 0, sizeof(*slave));

	for (i = 0; i < res->count; i++) {
		chan = &res->chans[i];
		dir = bt_manager_audio_stream_dir(chan->handle, chan->id);
		state = bt_manager_audio_stream_state(chan->handle, chan->id);

		SYS_LOG_INF("handle: %d, id: %d, dir: %d status %d\n", chan->handle,
			chan->id, dir,state);

		if (dir == BT_AUDIO_DIR_SINK) {
			lemusic_restore_state(chan, true);
		} else if (dir == BT_AUDIO_DIR_SOURCE) {
			lemusic_restore_state(chan, false);
		}

	}

	SYS_LOG_INF("done\n");

	return 0;
}

static void leaudio_handle_switch_notify(struct bt_manager_stream_report *rep)
{
	struct lemusic_bms_device *bms = &lemusic_get_app()->bms;

	SYS_LOG_INF("h:0x%x,stream:%d\n",rep->handle, rep->stream);
	bms->cur_stream = rep->stream;

	if ((bms->cur_stream != BT_AUDIO_STREAM_NONE)
		&& thread_timer_is_running(&lemusic_get_app()->switch_bmr_timer))
		thread_timer_stop(&lemusic_get_app()->switch_bmr_timer);
}

static void leaudio_handle_bt_scan_update(uint8_t connectable)
{
	if (1 != lemusic_get_auracast_mode()) {
		return;
	}

	SYS_LOG_INF("bt connectable:%d\n", connectable);
	if (connectable) {
		bt_manager_set_user_visual(true,false,true,0);
		bt_manager_audio_set_scan_policy(2, 3);
	} else {
		bt_manager_set_user_visual(true,false,false,0);
	}
}

static int leaudio_handle_switch(void)
{
	SYS_LOG_INF(":\n");
	return bt_manager_audio_stream_restore(BT_TYPE_LE);
}

static int lemusic_handle_connect(uint16_t *handle)
{
#if 0
	if (bt_manager_audio_get_type(*handle) != BT_TYPE_LE) {
		return 0;
	}

	if (bt_manager_audio_get_role(*handle) == BT_ROLE_SLAVE) {
		SYS_LOG_INF("slave\n");

		/* resume master scan if slave connected */
		bt_manager_audio_le_resume_scan();
		return 0;
	}

	SYS_LOG_INF("master\n");

	/* notify playing state */
	if (bt_manager_media_get_status() == BT_STATUS_PLAYING) {
		bt_manager_media_server_play();
	}

	/* for LE Master only */
	SYS_LOG_INF("mtu: %d\n", bt_manager_audio_le_conn_max_len(*handle));
	bt_manager_audio_le_vnd_register_rx_cb(*handle, lemusic_vnd_rx_cb);
#endif
	return 0;
}

static void lemusic_handle_disconnect(uint16_t *handle)
{
	struct lemusic_master_device *master = &lemusic_get_app()->master;
	struct lemusic_slave_device *slave = &lemusic_get_app()->slave;
	struct lemusic_bms_device *bms = &lemusic_get_app()->bms;

	if (*handle == master->source_chan.handle) {
		SYS_LOG_INF("master\n");

		master->bt_source_configured = 0;
		master->bt_source_enabled = 0;
		master->bt_source_started = 0;

		master->bt_source_pending = 0;
		master->bt_source_pending_start = 0;

		/* NOTICE: do not reset codec and qos! */
		memset(&master->source_chan, 0, sizeof(struct bt_audio_chan));
		return;
	}

#if 0
	if (*handle == slave->source_chan.handle ||
		*handle == slave->sink_chan.handle) {
#endif	// TODO: active_sink_chan?
	if (*handle == slave->source_chan.handle ||
		(slave->active_sink_chan &&
		*handle == slave->active_sink_chan->handle)) {
		SYS_LOG_INF("slave\n");

		slave->bt_source_started = 0;
		slave->adc_to_bt_started = 0;

		slave->bt_sink_enabled = 0;
		slave->bt_to_dac_started = 0;

		memset(&slave->source_chan, 0, sizeof(struct bt_audio_chan));
		memset(&slave->sink_chans, 0, sizeof(slave->sink_chans));
		// TODO:

		/* pause master scan if slave disconnected */
		bt_manager_audio_le_pause_scan();

#if ENABLE_BROADCAST
		if (lemusic_get_auracast_mode()) {
			bt_manager_broadcast_source_disable(bms->broad_chan[0].handle);
		}
#endif /*ENABLE_BROADCAST*/

	}
}

static int lemusic_handle_audio_disconnect(struct bt_audio_report *rep)
{
#if 0
	struct lemusic_master_device *master = &lemusic_get_app()->master;
	struct lemusic_slave_device *slave = &lemusic_get_app()->slave;

	/* only care about slave */
	if (rep->handle != slave->source_chan.handle &&
		rep->handle != slave->sink_chan.handle) {
		SYS_LOG_INF("not slave\n");
		return 0;
	}

	/* if slave's source stream disconnected */
	if (rep->id == slave->source_chan.id) {
		SYS_LOG_INF("allowed\n");
		master->bt_source_disallowed = 0;
	}

	/* filter if master's source channel not configured */
	if (!master->bt_source_configured) {
		SYS_LOG_INF("master not configured\n");
		return 0;
	}

	SYS_LOG_INF("\n");

	/* disable master's stream */
	return lemusic_stream_stop();
#else
	// TODO: what if one CIS disconnected?
	return 0;
#endif
}

static int lemusic_handle_client_volume(bool up, bool display)
{
	int ret;
	u8_t max;
	u8_t vol;
	u8_t sync_vol;

	ret = system_volume_get(AUDIO_STREAM_LE_AUDIO);
	if (ret < 0) {
		SYS_LOG_ERR("%d\n", ret);
		return -EINVAL;
	}else{
		vol = ret;
	}

	max = audio_policy_get_volume_level();

	if (up) {
		vol += 1;
		if (vol > max) {
			vol = max;
		}
	} else {
		if(vol >= 1){
			vol -= 1;
		}
	}

	sync_vol = bt_manager_music_to_lea_volume((int)vol);
	SYS_LOG_INF("vol=%d/%d, sync=%d\n", vol, max, sync_vol);
	system_volume_set(AUDIO_STREAM_LE_AUDIO, vol, display);
	bt_manager_volume_client_set(sync_vol);

#if ENABLE_PADV_APP
	if(lemusic_get_auracast_mode())
		padv_tx_data(padv_volume_map(vol,1));
#endif

	return 0;
}

static int lemusic_handle_volume_value(struct bt_volume_report *rep)
{
	struct lemusic_slave_device *slave = &lemusic_get_app()->slave;

	if (!slave->active_sink_chan ||
		rep->handle != slave->active_sink_chan->handle) {
		return 0;
	}

	SYS_LOG_INF("%d\n", rep->value);

	int vol = bt_manager_lea_to_music_volume((int)rep->value);
	audio_system_set_stream_volume(AUDIO_STREAM_LE_AUDIO,vol);
	if (slave->playback_player)
		media_player_set_volume(slave->playback_player,vol, vol);

#if ENABLE_PADV_APP
	if(lemusic_get_auracast_mode())
		padv_tx_data(padv_volume_map(vol,1));
#endif

	return bt_manager_volume_client_set(rep->value);
}

static int lemusic_handle_volume_mute(struct bt_volume_report *rep)
{
	struct lemusic_slave_device *slave = &lemusic_get_app()->slave;

	if (!slave->active_sink_chan ||
		rep->handle != slave->active_sink_chan->handle) {
		return 0;
	}

	SYS_LOG_INF("\n");

	audio_system_set_master_mute(1);
	audio_system_set_stream_mute(AUDIO_STREAM_LE_AUDIO, true);

	return bt_manager_volume_client_mute();
}

static int lemusic_handle_volume_unmute(struct bt_volume_report *rep)
{
	struct lemusic_slave_device *slave = &lemusic_get_app()->slave;

	if (!slave->active_sink_chan ||
		rep->handle != slave->active_sink_chan->handle) {
		return 0;
	}

	SYS_LOG_INF("\n");

	audio_system_set_master_mute(0);
	audio_system_set_stream_mute(AUDIO_STREAM_LE_AUDIO, false);

	return bt_manager_volume_client_unmute();
}

static int lemusic_handle_mic_mute(struct bt_mic_report *rep)
{
	SYS_LOG_INF("\n");

	audio_system_mute_microphone(1);

	return 0;
}

static int lemusic_handle_mic_unmute(struct bt_mic_report *rep)
{
	SYS_LOG_INF("\n");

	audio_system_mute_microphone(0);

	return 0;
}

static void lemusic_handle_playback_status(uint8_t is_playing)
{
	struct lemusic_app_t *lemusic = lemusic_get_app();

	if (is_playing) {
		lemusic->playing = 1;
		SYS_LOG_INF("play\n");
		bt_manager_media_server_play();
	} else {
		lemusic->playing = 0;
		SYS_LOG_INF("pause\n");
		bt_manager_media_server_pause();
	}
}

/* find the existed broad chan */
static struct bt_broadcast_chan *find_broad_chan(uint32_t handle, uint8_t id)
{
	struct lemusic_bms_device *bms = &lemusic_get_app()->bms;
	struct bt_broadcast_chan *chan;
	int i;

	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		chan = &bms->broad_chan[i];
		if (chan->handle == handle && chan->id == id) {
			return chan;
		}
	}
	return NULL;
}

/* find a new broad chan */
static struct bt_broadcast_chan *new_broad_chan(uint32_t handle, uint8_t id)
{
	struct lemusic_bms_device *bms = &lemusic_get_app()->bms;
	struct bt_broadcast_chan *chan;
	int i;

	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		chan = &bms->broad_chan[i];
		if (chan->handle == 0 && chan->id == 0) {
			return chan;
		}
	}
	return NULL;
}

static void lemusic_bms_tx_start(uint32_t handle, uint8_t id, uint16_t audio_handle)
{
	struct lemusic_bms_device *bms = &lemusic_get_app()->bms;

	if (!lemusic_get_app()
		|| !bms->enable
		|| bms->tx_start) {
		return;
	}

	if (bms->broadcast_source_enabled && bms->player) {
		int ret = media_player_trigger_audio_record_start(bms->player);
		if (ret) {
			printk("tx record err:%d\n", ret);
			return;
		}
		bms->tx_start = 1;
		//Time sensitive, short log to save time.
		printk("[" SYS_LOG_DOMAIN "] trigger record\n");
	}
}

void lemusic_bms_tx_sync_start(uint32_t handle, uint8_t id,
				 uint16_t audio_handle)
{
	struct lemusic_bms_device *bms = &lemusic_get_app()->bms;
	struct lemusic_slave_device *slave = &lemusic_get_app()->slave;

	if (!lemusic_get_app()
		|| !bms->enable
		|| bms->tx_sync_start) {
		return;
	}

	if (bms->broadcast_source_enabled && slave->playback_player) {
		int ret = media_player_trigger_audio_track_start(slave->playback_player);
		bms->tx_sync_start = 1;
		//Time sensitive, short log to save time.
		printk("[" SYS_LOG_DOMAIN "] trigger track:%d\n", ret);
	}
}

static int lemusic_bms_handle_source_config(struct bt_braodcast_configured *rep)
{
	struct lemusic_bms_device *bms = &lemusic_get_app()->bms;
	struct bt_broadcast_chan *chan;
	uint8_t vnd_buf[20] = { 0 };
	uint8_t type = 0;
	int8_t sq_mode_enable = 0;

	SYS_LOG_INF("h:0x%x id:%d\n", rep->handle, rep->id);

	chan = find_broad_chan(rep->handle, rep->id);
	if (!chan) {
		chan = new_broad_chan(rep->handle, rep->id);
		if (!chan) {
			SYS_LOG_ERR("no broad chan handle: %d, id: %d\n",
				    rep->handle, rep->id);
			return -EINVAL;
		}
		bms->num_of_broad_chan++;
	}

	chan->handle = rep->handle;
	chan->id = rep->id;

	vnd_buf[0] = VS_ID & 0xFF;
	vnd_buf[1] = VS_ID >> 8;
	memcpy(&vnd_buf[2], bms->broadcast_code, 16);
	vnd_buf[18] = SERIVCE_UUID & 0xFF;
	vnd_buf[19] = SERIVCE_UUID >> 8;

#if !VS_COMPANY_ID
	type = BT_DATA_SVC_DATA16;
#endif

	if (bms->num_of_broad_chan == 1) {
		SYS_EVENT_INF(EVENT_LEMUSIC_BIS_SOURCE_CONFIG,chan->handle,chan->id);
		bt_manager_broadcast_stream_cb_register(chan, lemusic_bms_tx_start,
							NULL);
		//bt_manager_broadcast_stream_tws_sync_cb_register(chan,
		//						 lemusic_bms_tx_sync_start);
		//2000 for media effect , 1000 for audio dma,32 sample for resample lib
		bt_manager_broadcast_stream_set_tws_sync_cb_offset(chan,
								   broadcast_get_tws_sync_offset
								   (bms->qos) - 3000 - 64 * 1000 / 48);//temporary modify
		sq_mode_enable = bt_manager_broadcast_get_sq_mode();
		/*
		When the BMS works with SQ audio configuration, it shall not include the manufacturer specific data as beacon
		*/
		if((bms->use_past == 0) && (!sq_mode_enable)){
			bt_manager_broadcast_source_vnd_ext_send(chan->handle, vnd_buf,
								 sizeof(vnd_buf), type);
			SYS_LOG_INF("no use past:\n");
		}
		bms->chan = chan;

		if (bms->use_past == 0) {
			thread_timer_start(&lemusic_get_app()->delay_start, 200, 0);
		} else {
			bt_manager_audio_stream_restore(BT_TYPE_LE);
		}

	}

	return 0;
}

#if 0
void lemusic_bms_tws_synced(void)
{
	struct lemusic_bms_device *bms;
	struct lemusic_slave_device *slave;

	if (NULL == lemusic_get_app()) {
		return;
	}
	bms = &lemusic_get_app()->bms;
	slave = &lemusic_get_app()->slave;
	if (!bms->broadcast_source_enabled) {
		return;     
	}
	if (bms->player_run) {
		lemusic_set_sink_chan_stream(slave->sink_stream);
	}
}
#endif

static int lemusic_bms_handle_source_enable(struct bt_broadcast_report *rep)
{
	struct lemusic_app_t *lemusic = lemusic_get_app();
	struct lemusic_bms_device *bms = &lemusic_get_app()->bms;
	struct lemusic_slave_device *slave = &lemusic_get_app()->slave;

	if (NULL == lemusic_get_app()) {
		return -1;
	}

	SYS_LOG_INF("handle 0x%x, id %d\n", rep->handle, rep->id);
	if (NULL == bms->chan) {
		SYS_LOG_INF("no chan\n");
		return -2;
	}

	if (rep->id != bms->chan->id) {
		SYS_LOG_INF("Skip this chan\n");
		return -3;
	}

	SYS_EVENT_INF(EVENT_LEMUSIC_BIS_SOURCE_ENABLE,bms->chan->handle,bms->chan->id);
	bt_manager_broadcast_pa_set_info_transfer(bms->broadcast_dev_handle);
	bms->broadcast_source_enabled = 1;

#if ENABLE_PADV_APP
	padv_tx_init(bms->chan->handle, AUDIO_STREAM_LE_AUDIO);
#endif

	if (lemusic->tts_playing) {
		SYS_LOG_INF("tts playing\n");
		return -4;
	}

	if (!lemusic->playing) {
		SYS_LOG_INF("stream stoped\n");
		return -5;
	}

	lemusic_bms_init_capture();
	lemusic_bms_start_capture();
	lemusic_set_sink_chan_stream(slave->sink_stream);

	return 0;
}

static int lemusic_bms_handle_source_disable(struct bt_broadcast_report *rep)
{
	struct lemusic_bms_device *bms = &lemusic_get_app()->bms;

	SYS_LOG_INF("h %d, id %d\n", rep->handle, rep->id);

	if ((rep->handle != bms->broadcast_dev_handle) &&
	    (bms->broadcast_dev_handle != 0))
	{
		SYS_LOG_WRN("Handle mismatch:0x%x,0x%x\n", rep->handle, bms->broadcast_dev_handle);
		return -1;
	}


#if ENABLE_PADV_APP
	padv_tx_deinit();
#endif
	
	lemusic_bms_stop_capture();
	lemusic_bms_exit_capture();

	struct bt_broadcast_chan * chan = find_broad_chan(rep->handle, rep->id);
	if(chan){
		if(bms->num_of_broad_chan){
			bms->num_of_broad_chan--;
			chan->handle = 0;
			chan->id = 0;
			if(chan == bms->chan){
				bms->chan = NULL;
			}
			if(!bms->num_of_broad_chan){
				bms->broadcast_source_enabled = 0;
				SYS_LOG_INF("disabled\n");
			}
		}else{
			SYS_LOG_WRN("should not be here\n");
		}
	}
	return 0;
}

static int lemusic_bms_handle_source_release(struct bt_broadcast_report *rep)
{
	struct lemusic_bms_device *bms = &lemusic_get_app()->bms;

	if (NULL == lemusic_get_app()) {
		return -1;
	}

	SYS_LOG_INF("h %d, id %d\n", rep->handle, rep->id);

	if ((rep->handle != bms->broadcast_dev_handle) &&
	    (bms->broadcast_dev_handle != 0))
	{
		SYS_LOG_WRN("Handle mismatch:0x%x,0x%x\n", rep->handle, bms->broadcast_dev_handle);
		return -1;
	}

	bms->chan = NULL;
	bms->broadcast_source_enabled = 0;
	bms->num_of_broad_chan = 0;
	bms->broadcast_dev_handle = 0;
	memset(bms->broad_chan,0,sizeof(bms->broad_chan));
	SYS_EVENT_INF(EVENT_BTMUSIC_BIS_SOURCE_RELEASE);

	return 0;
}

static void lemusic_handle_volume_restore(uint8_t *volume)
{
	int vol = *volume;
	uint8_t lea_vol_level = bt_manager_lea_to_music_volume((int)vol);
	SYS_LOG_INF("vol:%d, level: %d/%d\n", vol, lea_vol_level, audio_policy_get_volume_level());
	if (lea_vol_level > audio_policy_get_volume_level()) {
		SYS_LOG_ERR("\n");
		return;
	}

	audio_system_mutex_lock();
	system_player_volume_set(AUDIO_STREAM_LE_AUDIO, lea_vol_level);
	bt_manager_volume_set(vol, BT_VOLUME_TYPE_LE_AUDIO);
	audio_system_mutex_unlock();

#if ENABLE_PADV_APP
	if(lemusic_get_auracast_mode() && lemusic_get_app()->bms.broadcast_source_enabled)
		padv_tx_data(padv_volume_map(lea_vol_level,1));
#endif
}

static int lemusic_handle_letws_connected(uint16_t *handle)
{
#ifdef CONFIG_BT_LETWS
	uint8_t  tws_role;

	if (bt_manager_audio_get_type(*handle) == BT_TYPE_BR) {
		return 0;
	}

    tws_role = bt_manager_letws_get_dev_role();
	SYS_LOG_INF("mtu: %d,%d\n", bt_manager_audio_le_conn_max_len(*handle),tws_role);

	if (tws_role == BTSRV_TWS_SLAVE) {
        /*
		 switch bmr
		*/
		system_app_set_auracast_mode(4);
		system_app_launch_switch(DESKTOP_PLUGIN_ID_LE_MUSIC,DESKTOP_PLUGIN_ID_BMR);
	}else if (tws_role == BTSRV_TWS_MASTER){
		lemusic_stop_playback();
		lemusic_exit_playback();

		lemusic_bms_stop_capture();
		lemusic_bms_exit_capture();

		lemusic_set_sink_chan_stream(NULL);

		lemusic_set_auracast_mode(3);
	}
#endif
	return 0;
}

static int lemusic_handle_letws_disconnected(uint8_t *reason)
{
#ifdef CONFIG_BT_LETWS
	struct lemusic_app_t *lemusic = lemusic_get_app();

	if(!system_app_get_auracast_switch_status()){
		lemusic_stop_playback();
		lemusic_exit_playback();

		lemusic_bms_stop_capture();
		lemusic_bms_exit_capture();

		lemusic_set_sink_chan_stream(NULL);

		lemusic_set_auracast_mode(0);
		bt_manager_audio_stream_restore(BT_TYPE_LE);
		return 0;
	}

	system_app_set_auracast_switch_status(0);
	if(lemusic->playing){
		lemusic_stop_playback();
		lemusic_exit_playback();

		lemusic_bms_stop_capture();
		lemusic_bms_exit_capture();

		lemusic_set_sink_chan_stream(NULL);

		lemusic_bms_source_exit();
		lemusic_set_auracast_mode(1);
	}else{
		system_app_set_auracast_mode(2);
		system_app_launch_switch(DESKTOP_PLUGIN_ID_LE_MUSIC,DESKTOP_PLUGIN_ID_BMR);
	}
#endif
	return 0;
}

uint8_t lemusic_get_auracast_hq_to_sq_status(void)
{
	return auarcast_hq_switch_sq;
}

void lemusic_set_auracast_hq_to_sq_status(uint8_t switch_sq)
{
	SYS_LOG_INF("\n");
	auarcast_hq_switch_sq = switch_sq;
}

static int lemusic_bms_handle_sq_update(void)
{
	struct lemusic_app_t *lemusic = lemusic_get_app();
	int8_t sq_mode_enable;

	if (NULL == lemusic) {
		return -1;
	}
	sq_mode_enable = bt_manager_broadcast_get_sq_mode();
	SYS_LOG_INF("\n");

	if(1 == lemusic_get_auracast_mode()) {
		if(lemusic->playing) {

			lemusic_stop_playback();
			lemusic_exit_playback();

			lemusic_bms_stop_capture();
			lemusic_bms_exit_capture();
			lemusic_set_sink_chan_stream(NULL);

			if (sq_mode_enable) {
				// hq to sq
				lemusic_set_auracast_hq_to_sq_status(1);
			} else {
				if (!lemusic_get_auracast_hq_to_sq_status()) {
					lemusic_set_auracast_mode(0);
					bt_manager_audio_stream_restore(BT_TYPE_LE);
					//disable sq
					bt_manager_broadcast_set_sq_mode(0);

					return 0;
				}
				lemusic_set_auracast_hq_to_sq_status(0);
			}

			lemusic_bms_source_exit();
			lemusic_bms_source_init();
		}
	} else {
		if(lemusic->playing) {
			//bis maybe in releasing
			if (!lemusic->bms.chan) {
				if (lemusic->slave.playback_player_run) {
					lemusic_stop_playback();
					lemusic_exit_playback();
				}
				lemusic_set_auracast_mode(1);
			}
		}else{
			system_app_set_auracast_mode(2);
			system_app_launch_switch(DESKTOP_PLUGIN_ID_LE_MUSIC,DESKTOP_PLUGIN_ID_BMR);
		}
	}

	return 0;
}

void lemusic_bt_event_proc(struct app_msg *msg)
{
	SYS_LOG_INF("cmd: %d\n", msg->cmd);

	switch (msg->cmd) {
	case BT_CONNECTED:
		lemusic_view_show_connected();
		lemusic_handle_connect(msg->ptr);
		break;
	case BT_DISCONNECTED:
		lemusic_view_show_disconnected();
		lemusic_handle_disconnect(msg->ptr);
		if (app_tws_status_get_enable()) {
			bt_manager_set_user_visual(true,false,true,0);
		}
		break;
	case BT_TWS_CONNECTION_EVENT:
		if(msg->ptr){
		    SYS_LOG_INF("tws conn:");
	        lemusic_handle_letws_connected(msg->ptr);
		}
		break;
	case BT_TWS_DISCONNECTION_EVENT:
		if(msg->ptr){
	        lemusic_handle_letws_disconnected(msg->ptr);
		}
		break;
	case BT_AUDIO_DISCONNECTED:
		lemusic_handle_audio_disconnect(msg->ptr);
		break;
	case BT_AUDIO_DISCOVER_ENDPOINTS:
		lemusic_discover_endpoints(msg->ptr);
		break;
	case BT_AUDIO_STREAM_CONFIG_CODEC:
		lemusic_handle_config_codec(msg->ptr);
		break;
	case BT_AUDIO_STREAM_PREFER_QOS:
		lemusic_handle_prefer_qos(msg->ptr);
		break;
	case BT_AUDIO_STREAM_CONFIG_QOS:
		lemusic_handle_config_qos(msg->ptr);
		break;
	case BT_AUDIO_STREAM_ENABLE:
		lemusic_handle_enable(msg->ptr);
		break;
	case BT_AUDIO_STREAM_UPDATE:
		lemusic_handle_update(msg->ptr);
		break;
	case BT_AUDIO_STREAM_DISABLE:
		lemusic_handle_disable(msg->ptr);
		break;
	case BT_AUDIO_STREAM_START:
		lemusic_handle_start(msg->ptr);
		break;
	case BT_AUDIO_STREAM_STOP:
		lemusic_handle_stop(msg->ptr);
		break;
	case BT_AUDIO_STREAM_RELEASE:
		lemusic_handle_release(msg->ptr);
		break;
	case BT_AUDIO_STREAM_RESTORE:
		lemusic_handle_restore(msg->ptr);
		break;
	case BT_AUDIO_VOLUME_RESTORE:
		lemusic_handle_volume_restore(msg->ptr);
		break;

	case BT_AUDIO_SWITCH_NOTIFY:
		leaudio_handle_switch_notify(msg->ptr);
		break;

	case BT_AUDIO_UPDATE_SCAN_MODE:
		leaudio_handle_bt_scan_update((*(uint8_t *)msg->ptr));
		break;

	case BT_AUDIO_SWITCH:
		leaudio_handle_switch();
		break;

	/* volume controller */
	case BT_VOLUME_CLIENT_UP:
	case BT_VOLUME_CLIENT_UNMUTE_UP:
		lemusic_handle_client_volume(true, false);
		break;
	case BT_VOLUME_CLIENT_DOWN:
	case BT_VOLUME_CLIENT_UNMUTE_DOWN:
		lemusic_handle_client_volume(false, false);
		break;

	/* volume renderer */
	case BT_VOLUME_VALUE:
		lemusic_handle_volume_value(msg->ptr);
		break;
	case BT_VOLUME_MUTE:
		lemusic_handle_volume_mute(msg->ptr);
		break;
	case BT_VOLUME_UNMUTE:
		lemusic_handle_volume_unmute(msg->ptr);
		break;

	/* mic */
	case BT_MIC_MUTE:
		lemusic_handle_mic_mute(msg->ptr);
		break;
	case BT_MIC_UNMUTE:
		lemusic_handle_mic_unmute(msg->ptr);
		break;

	/* media device */
	case BT_MEDIA_SERVER_PLAY:
		SYS_LOG_INF("server play\n");
		bt_manager_media_play();
		break;
	case BT_MEDIA_SERVER_PAUSE:
		SYS_LOG_INF("server pause\n");
		bt_manager_media_pause();
		break;
	case BT_MEDIA_SERVER_NEXT_TRACK:
		SYS_LOG_INF("server next\n");
		bt_manager_media_play_next();
		break;
	case BT_MEDIA_SERVER_PREV_TRACK:
		SYS_LOG_INF("server prev\n");
		bt_manager_media_play_previous();
		break;

	/* media controller */
	case BT_MEDIA_PLAY:
		lemusic_handle_playback_status(1);
		break;
	case BT_MEDIA_STOP:
	case BT_MEDIA_PAUSE:
		lemusic_handle_playback_status(0);
		break;

		/* Broadcast Source */
	case BT_BROADCAST_SOURCE_CONFIG:
		lemusic_bms_handle_source_config(msg->ptr);
		break;
	case BT_BROADCAST_SOURCE_ENABLE:
		lemusic_bms_handle_source_enable(msg->ptr);
		break;
	case BT_BROADCAST_SOURCE_DISABLE:
		lemusic_bms_handle_source_disable(msg->ptr);
		break;
	case BT_BROADCAST_SOURCE_RELEASE:
		lemusic_bms_handle_source_release(msg->ptr);
		break;
	case BT_BIS_CONNECTED:
		SYS_LOG_INF("bis connected");
		break;
	case BT_BIS_DISCONNECTED:
		SYS_LOG_INF("bis disconnected");
		break;
#ifdef ENABLE_PAWR_APP
	case BT_PAWR_SYNCED:
		SYS_LOG_INF("pawr synced");
		break;
	case BT_PAWR_SYNC_LOST:
		SYS_LOG_INF("pawr sync lost");
		break;
#endif
    	case BT_BROADCAST_SOURCE_UPDATE_SQ:
		lemusic_bms_handle_sq_update();
		break;

	default:
		break;
	}
}

static void lemusic_switch_auracast()
{
	struct lemusic_app_t *lemusic = lemusic_get_app();

	SYS_LOG_INF("%d\n", lemusic_get_auracast_mode());

#ifdef CONFIG_BT_LETWS
	if(lemusic_get_auracast_mode() == 3){
		system_app_set_auracast_switch_status(1);
		bt_mamager_letws_disconnect(BT_HCI_ERR_REMOTE_USER_TERM_CONN);
		return;
	}else if (lemusic_get_auracast_mode() == 1) {
#else
	if (lemusic_get_auracast_mode()) {
#endif
		lemusic_stop_playback();
		lemusic_exit_playback();

		lemusic_bms_stop_capture();
		lemusic_bms_exit_capture();

		lemusic_set_sink_chan_stream(NULL);
		//disable sq
		bt_manager_broadcast_set_sq_mode(0);
		lemusic_set_auracast_mode(0);
		bt_manager_audio_stream_restore(BT_TYPE_LE);
	} else {
		if (lemusic->playing) {
			//bis maybe in releasing
			if (!lemusic->bms.chan) {
				if (lemusic->slave.playback_player_run) {
					lemusic_stop_playback();
					lemusic_exit_playback();
				}
				lemusic_set_auracast_mode(1);
			}
		} else {
			system_app_set_auracast_mode(2);
			system_app_launch_switch(DESKTOP_PLUGIN_ID_LE_MUSIC,DESKTOP_PLUGIN_ID_BMR);
		}
	}
}

static void lemusic_player_reset()
{
	struct lemusic_app_t *lemusic = lemusic_get_app();

	SYS_LOG_INF("\n");
	SYS_EVENT_INF(EVENT_LEMUSIC_PLAYER_RESET);
	if (!lemusic->tts_playing) {
		//stop
		if (lemusic->slave.playback_player_run) {
			lemusic_stop_playback();
			lemusic_exit_playback();
		}

		if (lemusic->bms.player_run) {
			lemusic_bms_stop_capture();
			lemusic_bms_exit_capture();
			lemusic_set_sink_chan_stream(NULL);
		}

		lemusic->bms.restart = 0;

		//start
		if (lemusic->slave.active_sink_chan) {
			if (!lemusic->slave.playback_player_run) {
				lemusic_init_playback();
				lemusic_start_playback();
			}

			if (lemusic->bms.broadcast_source_enabled
				&& (!lemusic->bms.player_run)) {
				lemusic_bms_init_capture();
				lemusic_bms_start_capture();
			}
			lemusic_set_sink_chan_stream(lemusic->slave.sink_stream);
		}
	}

}

#ifdef CONFIG_BT_SELF_APP
int otadfu_set_disable_prepare_start_resp_flag(uint8_t enable_start);
#endif

void lemusic_input_event_proc(struct app_msg *msg)
{
	struct lemusic_app_t *lemusic = lemusic_get_app();
	int status;

	switch (msg->cmd) {
	case MSG_BT_PLAY_PAUSE_RESUME:
		if (bt_manager_media_get_status() == BT_STATUS_PLAYING) {
			SYS_LOG_INF("pause\n");
			bt_manager_media_pause();
		} else {
			SYS_LOG_INF("play\n");
			bt_manager_media_play();
		}
		break;

	case MSG_BT_PLAY_NEXT:
		SYS_LOG_INF("next\n");
		bt_manager_media_play_next();
		break;

	case MSG_BT_PLAY_PREVIOUS:
		SYS_LOG_INF("prev\n");
		bt_manager_media_play_previous();
		break;

	case MSG_BT_PLAY_VOLUP:
		lemusic_handle_client_volume(true, true);
		break;

	case MSG_BT_PLAY_VOLDOWN:
		lemusic_handle_client_volume(false, true);
		break;

	case MSG_BT_PLAY_SEEKFW_START:
		if (lemusic->playing) {
			bt_manager_media_fast_forward(true);
		}
		break;

	case MSG_BT_PLAY_SEEKFW_STOP:
		if (lemusic->playing) {
			bt_manager_media_fast_forward(false);
		}
		break;

	case MSG_BT_PLAY_SEEKBW_START:
		if (lemusic->playing) {
			bt_manager_media_fast_rewind(true);
		}
		break;

	case MSG_BT_PLAY_SEEKBW_STOP:
		if (lemusic->playing) {
			bt_manager_media_fast_rewind(false);
		}
		break;

	case MSG_SWITCH_BROADCAST:
		{
		  //switch to hq
		  if (lemusic_get_auracast_hq_to_sq_status()) {
              bt_manager_audio_sq_mode_update(false);
			  break;
		  }

#ifdef CONFIG_BT_SELF_APP
			otadfu_set_disable_prepare_start_resp_flag(true);
#endif
			lemusic_switch_auracast();
#ifdef CONFIG_BT_SELF_APP
			otadfu_set_disable_prepare_start_resp_flag(false);
#endif
		}
		break;
	case MSG_AURACAST_ENTER:
		SYS_LOG_INF("MSG_AURACAST_ENTER: %d\n", system_app_get_auracast_mode());
		if(system_app_get_auracast_mode() != 1){
			lemusic_switch_auracast();
		} else if(system_app_get_auracast_mode() == 1){
			lemusic_player_reset();
		}
		break;
	case MSG_AURACAST_EXIT:
		if(system_app_get_auracast_mode() == 1 ||
			system_app_get_auracast_mode() == 2){
			lemusic_switch_auracast();
		}
		break;
	case MSG_PAUSE_PLAYER:
		status = bt_manager_media_get_status();
		if(status == BT_STATUS_PLAYING){
			bt_manager_media_pause();
		}
		break;
	case MSG_RESUME_PLAYER:
		status = bt_manager_media_get_status();
		if(status == BT_STATUS_PAUSED){
			bt_manager_media_play();
		}
		break;
	case MSG_PLAYER_RESET:
		lemusic_player_reset();
		break;
	default:
		break;
	}
}

#ifdef CONFIG_PLAYTTS
#if RELAY_DISABLED
static void lemusic_tts_restore(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	struct lemusic_app_t *lemusic = lemusic_get_app();

	struct lemusic_slave_device *slave = &lemusic_get_app()->slave;
	struct bt_audio_chan *sink_chan = slave->active_sink_chan;
	int sink_state;

	if (sink_chan) {
		SYS_LOG_INF("handle %d, id %d", sink_chan->handle, sink_chan->id);
		sink_state = bt_manager_audio_stream_state(sink_chan->handle,
							   sink_chan->id);
	} else {
		SYS_LOG_ERR("no chan\n");
		return;
	}

	/* both channels are stopped */
	if (sink_state < BT_AUDIO_STREAM_ENABLED) {
		SYS_LOG_INF("%d\n", sink_state);
		return;
	}

	if (thread_timer_is_running(&lemusic->delay_start)){
		SYS_LOG_INF("wait for restore\n");
		return;
	}

	if (!slave->playback_player_run) {
		if (lemusic->bms.player_run) {
			lemusic_bms_stop_capture();
			lemusic_bms_exit_capture();
			lemusic->bms.player_run = 0;
			lemusic_set_sink_chan_stream(NULL);
		}

		if (lemusic_get_auracast_mode()) {
			SYS_LOG_INF("bms %d", lemusic->bms.broadcast_source_enabled);
			if (lemusic->bms.broadcast_source_enabled) {
				lemusic_init_playback();
				lemusic_start_playback();

				lemusic_bms_init_capture();
				lemusic_bms_start_capture();
				lemusic_set_sink_chan_stream(lemusic->slave.sink_stream);
			}
		} else {
			SYS_LOG_INF("playback");
			lemusic_init_playback();
			lemusic_start_playback();
		}

	}

}

#else
static void lemusic_tts_restore(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	struct lemusic_slave_device *slave = &lemusic_get_app()->slave;
	struct bt_audio_chan *source_chan = &slave->source_chan;
	struct bt_audio_chan *sink_chan = slave->active_sink_chan;
	int source_state, sink_state;
	struct bt_audio_chan *chan;
	int i;

	// TODO: if active_sink_chan is released but another sink_chan active???
	// TODO: if update active_sink_chan, that will be fine!!!
	/* released or disconnected */
	if (!source_chan->handle && (!sink_chan || !sink_chan->handle)) {
		SYS_LOG_INF("return\n");
		return;
	}

	source_state = bt_manager_audio_stream_state(source_chan->handle,
						     source_chan->id);
	if (sink_chan) {
		sink_state = bt_manager_audio_stream_state(sink_chan->handle,
							   sink_chan->id);
	} else {
		sink_state = 0;
	}

	/* both channels are stopped */
	if (source_state < BT_AUDIO_STREAM_ENABLED &&
	    sink_state < BT_AUDIO_STREAM_ENABLED) {
		SYS_LOG_INF("%d, %d\n", source_state, sink_state);
		return;
	}

	SYS_LOG_INF("source %d, sink %d\n", source_state, sink_state);

	lemusic_init_capture();
	lemusic_init_playback();

	if (source_state == BT_AUDIO_STREAM_STARTED) {
		lemusic_start_capture();
		slave->bt_source_started = 1;
	}

	/* for each sink channel */
	for (i = 0; i < NUM_OF_SINK_CHAN; i++) {
		chan = &slave->sink_chans[i];
		sink_state = bt_manager_audio_stream_state(chan->handle, chan->id);
		SYS_LOG_INF("sink %d\n", sink_state);
		switch (sink_state) {
// TODO: delete BT_AUDIO_STREAM_ENABLED???
// TODO: handle_enable() should take care to fix if tts takes too much time!
// TODO: seems not necessary???
		case BT_AUDIO_STREAM_ENABLED:
			lemusic_start_playback();
			slave->bt_sink_enabled = 1;	// TODO: still need?!
			bt_manager_audio_sink_stream_start_single(chan);
			lemusic_stream_start();
			break;
		case BT_AUDIO_STREAM_STARTED:
			lemusic_start_playback();
			slave->bt_sink_enabled = 1;
			lemusic_stream_start();
			break;
		default:
			break;
		}
	}
}
#endif

void lemusic_tts_event_proc(struct app_msg *msg)
{
	struct lemusic_app_t *lemusic = lemusic_get_app();

	SYS_LOG_INF("tts %d", msg->value);
	switch (msg->value) {
	case TTS_EVENT_START_PLAY:
		lemusic->tts_playing = 1;
		if (thread_timer_is_running(&lemusic->monitor_timer)) {
			thread_timer_stop(&lemusic->monitor_timer);
		}
		//stop
		if (lemusic->slave.playback_player_run) {
			lemusic_stop_playback();
			lemusic_exit_playback();
		}
		if (lemusic->slave.capture_player_run) {
			lemusic_stop_capture();
			lemusic_exit_capture();
		}

		if (lemusic->bms.player_run) {
			lemusic_bms_stop_capture();
			lemusic_bms_exit_capture();
			lemusic->bms.player_run = 0;
			lemusic_set_sink_chan_stream(NULL);
		}

		lemusic->bms.restart = 0;

		break;
	case TTS_EVENT_STOP_PLAY:
		lemusic->tts_playing = 0;
		if (thread_timer_is_running(&lemusic->monitor_timer)) {
			thread_timer_stop(&lemusic->monitor_timer);
		}
		thread_timer_init(&lemusic->monitor_timer, lemusic_tts_restore,
				  NULL);
		thread_timer_start(&lemusic->monitor_timer, 1000, 0);
		SYS_LOG_INF("tts stop\n");
		break;
	}
}
#endif

void lemusic_app_event_proc(struct app_msg *msg)
{
	SYS_LOG_INF("cmd: %d\n", msg->cmd);

	switch (msg->cmd) {
	case MSG_LEMUSIC_MESSAGE_CMD_PLAYER_RESET:
		lemusic_player_reset();
		break;
	}
}

void lemusic_tws_event_proc(struct app_msg *msg)
{
#ifdef ENABLE_PAWR_APPXXX
	struct lemusic_app_t *lemusic = lemusic_get_app();
	struct lemusic_bms_device *bms = &lemusic->bms;

	switch (msg->cmd) {
	case TWS_EVENT_REQ_PAST_INFO:
		{
			if (bms->wait_for_past_req){
				if (thread_timer_is_running(&bms->broadcast_start_timer)){
					thread_timer_stop(&bms->broadcast_start_timer);
				}
				thread_timer_start(&bms->broadcast_start_timer, 0, 0);
				bms->wait_for_past_req = 0;
			} else {
				SYS_LOG_INF("pa_set_info_transfer:0x%x %d\n",bms->broadcast_dev_handle,
					bms->broadcast_source_enabled);
				if(bms->broadcast_source_enabled){
					bt_manager_broadcast_pa_set_info_transfer(bms->broadcast_dev_handle);
				}
			}
			break;
		}
	case TWS_EVENT_NOTIFY_SYNCED:
	    {
            SYS_LOG_INF("tws notify synced: \n");
			if (thread_timer_is_running(&bms->broadcast_wait_synced_timer)) {
		        thread_timer_stop(&bms->broadcast_wait_synced_timer);
			    lemusic_bms_tws_synced();
			}
		}	
		break;
	}
#endif
}

