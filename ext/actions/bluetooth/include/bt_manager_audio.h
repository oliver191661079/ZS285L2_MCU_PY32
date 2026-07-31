/*
 * Copyright (c) 2021 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __BT_MANAGER_AUDIO_H__
#define __BT_MANAGER_AUDIO_H__

#include "btservice_api.h"
#include <media_type.h>

#define BT_AUDIO_CONTEXT_CALL	(BT_AUDIO_CONTEXT_CONVERSATIONAL | \
				BT_AUDIO_CONTEXT_RINGTONE)

#define BT_ROLE_MASTER	0
#define BT_ROLE_SLAVE	1
#define BT_ROLE_BROADCASTER    2	/* BIS broadcaster */
#define BT_ROLE_RECEIVER       3	/* BIS sync receiver */
// TODO: source/sink/assist/delegator ?

struct bt_audio_role {
	/*
	 * call_gateway
	 * volume_controller
	 * microphone_controller
	 * media_device
	 * unicast_client
	 * broadcast_assistant
	 */
	uint32_t num_master_conn : 3;	/* as a master, range: [0: 7] */

	/*
	 * call_terminal
	 * volume_renderer
	 * microphone_device
	 * media_controller
	 * unicast_server
	 * scan_delegator
	 * broadcast_sink
	 */
	uint32_t num_slave_conn : 3;	/* as a slave, range: [0: 7] */

	/* if BR coexists */
	uint32_t br : 1;

	/* if support encryption as slave */
	uint32_t encryption : 1;
	/*
	 * Authenticated Secure Connections and 128-bit key which depends
	 * on encryption.
	 */
	uint32_t secure : 1;

	/* call */
	uint32_t call_gateway : 1;
	uint32_t call_terminal: 1;

	/* volume */
	uint32_t volume_controller : 1;
	uint32_t volume_renderer : 1;

	/* microphone */
	uint32_t microphone_controller : 1;
	uint32_t microphone_device : 1;

	/* media */
	uint32_t media_controller : 1;
	uint32_t media_device : 1;

	/* unicast stream */
	uint32_t unicast_client : 1;
	uint32_t unicast_server : 1;
	/* unicast tartget announcement for adv data*/
	uint32_t target_announcement : 1;

	/* group */
	uint32_t set_coordinator : 1;
	uint32_t set_member : 1;

	/* broadcast */
	uint32_t broadcast_assistant : 1;
	uint32_t scan_delegator : 1;
	uint32_t broadcast_source : 1;
	uint32_t broadcast_sink : 1;

	/* if support remote keys */
	uint32_t remote_keys : 1;

	/* matching remote(s) with LE Audio test address(es) */
	uint32_t test_addr : 1;

	/* if enable master latency function */
	uint32_t master_latency : 1;

	/* if using trigger machanism, enabled by default for compatibility */
	uint32_t disable_trigger : 1;

	/* if set_sirk is enabled  */
	uint32_t sirk_enabled : 1;
	/* if set_sirk is encrypted (otherwise plain) */
	uint32_t sirk_encrypted : 1;

	/*
	 * Work as a unicast relay node, it supports slave and master
	 * simutaneously. It will work as a unicast server to accept
	 * streams and work as a unicast client to relay the streams
	 * to other unicast servers.
	 */
	uint32_t relay : 1;

	/* if support encryption as master */
	uint32_t master_encryption : 1;

	/* master will not connect specific slaves by name */
	uint32_t disable_name_match : 1;

	/* LE audio connection */
	uint32_t lea_connection_enable : 1;

	/* only works for set_member */
	uint8_t set_size;
	uint8_t set_rank;
	uint8_t set_sirk[16];	/* depends on sirk_enabled */

	/* Gaming audio configuration*/
	uint32_t gmas_client : 1;
	uint32_t gmas_server : 1;
	struct bt_gaming_audio_config gmas_config;
	uint16_t sink_available_contexts;
	uint16_t source_available_contexts;
};

struct bt_audio_chan_info {
	uint8_t format;	/* BT_AUDIO_CODEC_ */
	/* valid at config_codec time for master */
	uint8_t channels;	/* range: [1, 8] */
	/* valid at config_codec time for master */
	uint16_t octets;	/* octets for 1 channel (or 1 codec frame) */
	uint16_t sample;	/* sample_rate; unit: kHz */
	uint16_t kbps;	/* bit rate */
	uint16_t sdu;	/* sdu size */
	uint16_t contexts;	/* BT_AUDIO_CONTEXT_ */
	uint32_t locations;	/* BT_AUDIO_LOCATIONS_ */
	uint32_t interval;	/* sdu interval, unit: us */

	uint8_t target_latency;
	uint8_t duration;	/* unit: ms */
	uint16_t max_latency;	/* max_transport_latency */
	uint32_t delay;	/* presentation_delay */
	uint8_t rtn;	/* retransmission_number */
	uint8_t phy;
	uint8_t framing;
	void * chan; /* timeline_owner*/
};

/* helper for struct bt_audio_capabilities */
struct bt_audio_capabilities_1 {
	uint16_t handle;	/* for client to distinguish different server */
	uint8_t source_cap_num;
	uint8_t sink_cap_num;

	uint16_t source_available_contexts;
	uint16_t source_supported_contexts;
	uint32_t source_locations;

	uint16_t sink_available_contexts;
	uint16_t sink_supported_contexts;
	uint32_t sink_locations;

	/* source cap first if any, followed by sink cap */
	struct bt_audio_capability cap[1];
};

/* helper for struct bt_audio_capabilities */
struct bt_audio_capabilities_2 {
	uint16_t handle;	/* for client to distinguish different server */
	uint8_t source_cap_num;
	uint8_t sink_cap_num;

	uint16_t source_available_contexts;
	uint16_t source_supported_contexts;
	uint32_t source_locations;

	uint16_t sink_available_contexts;
	uint16_t sink_supported_contexts;
	uint32_t sink_locations;

	/* source cap first if any, followed by sink cap */
	struct bt_audio_capability cap[2];
};

/* helper for struct bt_audio_qoss, used to config 1 qos */
struct bt_audio_qoss_1 {
	uint8_t num_of_qos;
	uint8_t sca;
	uint8_t packing;
	uint8_t framing;

	uint32_t interval_m;
	uint32_t interval_s;

	uint16_t latency_m;
	uint16_t latency_s;

	uint32_t delay_m;
	uint32_t delay_s;

	uint32_t processing_m;
	uint32_t processing_s;

	struct bt_audio_qos qos[1];
};

/* helper for struct bt_audio_qoss, used to config 2 qos */
struct bt_audio_qoss_2 {
	uint8_t num_of_qos;
	uint8_t sca;
	uint8_t packing;
	uint8_t framing;

	uint32_t interval_m;
	uint32_t interval_s;

	uint16_t latency_m;
	uint16_t latency_s;

	uint32_t delay_m;
	uint32_t delay_s;

	uint32_t processing_m;
	uint32_t processing_s;

	struct bt_audio_qos qos[2];
};

struct bt_audio_preferred_qos_default {
	uint8_t framing;
	uint8_t phy;
	uint8_t rtn;
	uint16_t latency;	/* max_transport_latency */

	uint32_t delay_min;	/* presentation_delay_min */
	uint32_t delay_max;	/* presentation_delay_max */
	uint32_t preferred_delay_min;	/* presentation_delay_min */
	uint32_t preferred_delay_max;	/* presentation_delay_max */
};

/* FIXME: support codec policy */
struct bt_audio_codec_policy {
};

/* FIXME: support qos policy */
struct bt_audio_qos_policy {
};

void bt_manager_audio_conn_event(int event, void *data, int size);

uint8_t bt_manager_audio_get_type(uint16_t handle);

int bt_manager_audio_get_cur_dev_num(void);

int bt_manager_audio_get_role(uint16_t handle);

bool bt_manager_audio_is_ios_pc(uint16_t handle);

bool bt_manager_audio_is_ios_dev(uint16_t handle);

bool bt_manager_audio_actived(void);

bool bt_manager_check_audio_conn_is_same_dev(bd_address_t *addr, uint8_t type);

uint32_t bt_manager_audio_get_active_channel_iso_interval(void);

struct bt_audio_conn *bt_manager_get_dev_another_audio_conn(bd_address_t *addr, uint8_t type);

uint16_t bt_manager_find_active_slave_handle(void);

int bt_manager_audio_switch_multi_point(int enable);

bool bt_manager_audio_is_multi_point(void);

/*
 * BT Audio state
 *
 * UNKNOWN -> INITED: init()
 * INITED -> STARTED: start()
 * INITED -> UNKNOWN: exit()
 * STARTED -> STOPPING: stop() and return -EINPROGRESS
 * STARTED -> STOPPED: stop() and return 0 (success)
 * STOPPING -> STOPPED: stopped event
 * STOPPED -> STARTED: start()
 * STOPPED -> UNKNOWN: exit()
 */
/* BT Audio is uninitialized */
#define BT_AUDIO_STATE_UNKNOWN	0
/* BT Audio is initialized, could enter unknown/started state */
#define BT_AUDIO_STATE_INITED	1
/* BT Audio is started, could enter stopping/stopped state */
#define BT_AUDIO_STATE_STARTED	2
/* BT Audio is stopping, could enter stopped state only */
#define BT_AUDIO_STATE_STOPPING	3
/* BT Audio is stopped, could enter started/unknown state */
#define BT_AUDIO_STATE_STOPPED	4

/*
 * get bt_audio state, return BT_AUDIO_STATE_XXX (see above)
 */
uint8_t bt_manager_audio_state(uint8_t type);

/*
 * init bt_audio, paired with bt_manager_audio_exit()
 */
int bt_manager_audio_init(uint8_t type, struct bt_audio_role *role);

/*
 * exit bt_audio, paired with bt_manager_audio_init()
 */
int bt_manager_audio_exit(uint8_t type);

int bt_manager_audio_conn_disconnect(uint16_t handle);

/*
 * clear all remote keys accoring to current keys_id.
 */
int bt_manager_audio_keys_clear(uint8_t type);

/*
 * start bt_audio, paired with bt_manager_audio_stop()
 *
 * For LE Audio master, it will start scan.
 * For LE Audio slave, it will start adv.
 */
int bt_manager_audio_start(uint8_t type);

/*
 * stop bt_audio, paired with bt_manager_audio_start()
 *
 * For LE Audio master, it will stop scan, disconnect all connections
 * and be ready to exit.
 * For LE Audio slave, it will stop adv, disconnect all connections
 * and be ready to exit.
 */
int bt_manager_audio_stop(uint8_t type);

typedef void (*lea_on_off_complete_cb)(int err);
int bt_manager_audio_lea_enable(lea_on_off_complete_cb cb);
int bt_manager_audio_lea_disable(lea_on_off_complete_cb cb);
int bt_manager_audio_is_lea_open(void);

/* bt_audio is stopped */
void bt_manager_audio_stopped_event(uint8_t type);

/*
 * pause bt_audio, paired with bt_manager_audio_resume()
 *
 * For LE Audio master, it will stop scan.
 * For LE Audio slave, it will stop adv.
 */
int bt_manager_audio_pause(uint8_t type);

/*
 * resume bt_audio, paired with bt_manager_audio_pause()
 *
 * For LE Audio master, it will restore scan if necessary.
 * For LE Audio slave, it will restore adv if necessary.
 */
int bt_manager_audio_resume(uint8_t type);

/*
 * Audio Stream Control
 */

/*
 * BT Audio Stream state
 */
#define BT_AUDIO_STREAM_IDLE	0x0
/* the audio stream is codec configured, need to configure qos */
#define BT_AUDIO_STREAM_CODEC_CONFIGURED	0x1
/* the audio stream is qos configured, need to enable it */
#define BT_AUDIO_STREAM_QOS_CONFIGURED	0x2
/* the audio stream is enabled and wait for the sink to be ready */
#define BT_AUDIO_STREAM_ENABLED	0x3
/* the audio stream is ready or is transporting */
#define BT_AUDIO_STREAM_STARTED	0x4
/*
 * the audio stream is disabled, but the stream may be working still,
 * if the stream is stopped, it will transport to configured state.
 */
#define BT_AUDIO_STREAM_DISABLED	0x5

#define BT_AUDIO_STREAM_RELEASING	0x6

/* used to restore all current audio channels */
struct bt_audio_chan_restore {
	uint8_t count;
	struct bt_audio_chan chans[0];
};

int bt_manager_audio_stream_dir(uint16_t handle, uint8_t id);

int bt_manager_audio_stream_state(uint16_t handle, uint8_t id);

int bt_manager_audio_stream_info(struct bt_audio_chan *chan,
				struct bt_audio_chan_info *info);

struct bt_audio_endpoints *bt_manager_audio_client_get_ep(uint16_t handle);

struct bt_audio_capabilities *bt_manager_audio_client_get_cap(uint16_t handle);

int bt_manager_audio_get_audio_handle(uint16_t handle,uint8_t id);

uint64_t bt_manager_audio_get_le_time(uint16_t handle);

void *bt_manager_audio_get_tl_owner_chan(uint16_t handle);

int bt_manager_audio_stream_cb_register(struct bt_audio_chan *chan,
				bt_audio_trigger_cb trigger,
				bt_audio_trigger_cb period_trigger);

/*
 * Register callback(s) but will bind the trigger callback(s) of the new
 * channel to the bound channel which makes the timeline be shared by the
 * new channel and the bound channel.
 */
int bt_manager_audio_stream_cb_bind(struct bt_audio_chan *bound,
				struct bt_audio_chan *new_chan,
				bt_audio_trigger_cb trigger,
				bt_audio_trigger_cb period_trigger);

int bt_manager_audio_stream_tws_sync_cb_register(struct bt_audio_chan *chan,
				bt_audio_trigger_cb tws_sync_trigger);

int bt_manager_audio_stream_tws_sync_cb_register_1(struct bt_audio_chan *chan,
				bt_audio_start_cb tws_start_cb,void* param);

int bt_manager_audio_stream_set_tws_sync_cb_offset(struct bt_audio_chan *chan,   
				int32_t offset_from_syncdelay); 

/*
 * For LE Audio slave only
 */
int bt_manager_audio_server_cap_init(struct bt_audio_capabilities *caps);

/*
 * For LE Audio slave only
 */
int bt_manager_audio_server_qos_init(bool source,
				struct bt_audio_preferred_qos_default *qos);

/*
 * For LE Audio master only
 */
int bt_manager_audio_client_codec_init(struct bt_audio_codec_policy *policy);

/*
 * For LE Audio master only
 */
int bt_manager_audio_client_qos_init(struct bt_audio_qos_policy *policy);

int bt_manager_audio_stream_config_codec(struct bt_audio_codec *codec);

int bt_manager_audio_stream_prefer_qos(struct bt_audio_preferred_qos *qos);

int bt_manager_audio_stream_config_qos(struct bt_audio_qoss *qoss);

/*
 * For LE Audio master only
 *
 * combined with bt_manager_audio_stream_bandwidth_alloc() to replace
 * bt_manager_audio_stream_config_qos().
 */
int bt_manager_audio_stream_bind_qos(struct bt_audio_qoss *qoss, uint8_t index);

/*
 * WARNNING: Move the sync point forward to optimize the transport delay in
 * some special cases, do NOT call this function if unsure.
 *
 * NOTICE: make sure called before stream_enable()/stream_enabled if used.
 */
int bt_manager_audio_stream_sync_forward(struct bt_audio_chan **chans,
				uint8_t num_chans);

int bt_manager_audio_stream_sync_forward_single(struct bt_audio_chan *chan);

int bt_manager_audio_stream_enable(struct bt_audio_chan **chans,
				uint8_t num_chans, uint32_t contexts);

int bt_manager_audio_stream_enable_single(struct bt_audio_chan *chan,
				uint32_t contexts);

int bt_manager_audio_stream_disable(struct bt_audio_chan **chans,
				uint8_t num_chans);

int bt_manager_audio_stream_disable_single(struct bt_audio_chan *chan);

/*
 * for Audio Source to create Source Stream used to send data via Bluetooth
 *
 * Kinds of samples are as follows.
 * case 1: two channels exist in one connection.
 * case 2: one channel exist for each connection.
 * case 3: two channels exist in one connection and one channel exist in
 *         another connection.
 *
 * @param shared Whether all channels will share the same date
 */
io_stream_t bt_manager_audio_source_stream_create(struct bt_audio_chan **chans,
				uint8_t num_chans, uint32_t locations);

io_stream_t bt_manager_audio_source_stream_create_single(struct bt_audio_chan *chan,
				uint32_t locations);

int bt_manager_audio_source_stream_set(struct bt_audio_chan **chans,
				uint8_t num_chans, io_stream_t stream,
				uint32_t locations);

int bt_manager_audio_source_stream_set_single(struct bt_audio_chan *chan,
				io_stream_t stream, uint32_t locations);

/*
 * for Audio Sink to set Sink Stream used to receive data via Bluetooth
 */
int bt_manager_audio_sink_stream_set(struct bt_audio_chan *chan,
				io_stream_t stream);

/*
 * for Audio Sink
 *
 * Notify the remote (Audio Source) that the local (Audio Sink) is ready to
 * receive/start the audio stream.
 *
 * Valid after stream is enabled
 */
int bt_manager_audio_sink_stream_start(struct bt_audio_chan **chans,
				uint8_t num_chans);

int bt_manager_audio_sink_stream_start_single(struct bt_audio_chan *chan);

/*
 * for Audio Sink
 *
 * Notify the remote (Audio Source) that the local (Audio Sink) is going to
 * stop the audio stream.
 *
 * Valid after stream is disabled
 */
int bt_manager_audio_sink_stream_stop(struct bt_audio_chan **chans,
				uint8_t num_chans);

int bt_manager_audio_sink_stream_stop_single(struct bt_audio_chan *chan);

int bt_manager_audio_stream_update(struct bt_audio_chan **chans,
				uint8_t num_chans, uint32_t contexts);

int bt_manager_audio_stream_update_single(struct bt_audio_chan *chan,
				uint32_t contexts);

int bt_manager_audio_stream_release(struct bt_audio_chan **chans,
				uint8_t num_chans);

int bt_manager_audio_stream_release_single(struct bt_audio_chan *chan);

int bt_manager_audio_stream_disconnect(struct bt_audio_chan **chans,
				uint8_t num_chans);

int bt_manager_audio_stream_disconnect_single(struct bt_audio_chan *chan);

/*
 * For LE Audio master only
 *
 * Allocate the bandwidth earlier initiated by master, combined with
 * bt_manager_audio_stream_bind_qos() instead of
 * bt_manager_audio_stream_config_qos().
 */
void *bt_manager_audio_stream_bandwidth_alloc(struct bt_audio_qoss *qoss);

/*
 * For LE Audio master only
 *
 * Free the bandwidth allocated by config_qos() initiated by master.
 */
int bt_manager_audio_stream_bandwidth_free(void *cig_handle);

int bt_manager_audio_stream_restore(uint8_t type);

void bt_manager_audio_stream_tx_adjust_clk(int8_t adjust_ap_us);

void bt_manager_audio_stream_event(int event, void *data, int size);

enum media_type bt_manager_audio_codec_type(uint8_t coding_format);

int bt_manager_media_set_user_pause_pending(uint16_t hdl);

/*
 * Media Control
 */

int bt_manager_media_play(void);

int bt_manager_media_stop(void);

int bt_manager_media_pause(void);

int bt_manager_media_playpause(void);

int bt_manager_media_play_next(void);

int bt_manager_media_play_previous(void);

int bt_manager_media_fast_forward(bool start);

int bt_manager_media_fast_rewind(bool start);

int bt_manager_media_server_play(void);

int bt_manager_media_server_stop(void);

int bt_manager_media_server_pause(void);

void bt_manager_media_event(int event, void *data, int size);

int bt_manager_media_get_status(void);

int bt_manager_media_get_local_passthrough_status(uint16_t handle);

uint16_t bt_manager_media_get_active_br_handle(void);

int bt_manager_media_get_id3_info(void);

int bt_manager_media_set_active_br_handle(uint16_t set);

/*
 * Call Control
 */

#define BT_CALL_STATE_UNKNOWN          0x00
#define BT_CALL_STATE_INCOMING         0x01
#define BT_CALL_STATE_DIALING          0x02	/* outgoing */
#define BT_CALL_STATE_ALERTING         0x03	/* outgoing */
#define BT_CALL_STATE_ACTIVE           0x04
#define BT_CALL_STATE_LOCALLY_HELD     0x05
#define BT_CALL_STATE_REMOTELY_HELD    0x06
/* locally and remotely held */
#define BT_CALL_STATE_HELD             0x07
#define BT_CALL_STATE_SIRI             0x08

struct bt_audio_call_info {
	uint8_t state;

	/*
	 * UTF-8 URI format: uri_scheme:caller_id such as tel:96110
	 *
	 * end with '\0' if valid, all-zeroed if invalid
	 */
	uint8_t remote_uri[BT_AUDIO_CALL_URI_LEN];
};

/*
 * NOTE: the following bt_manager_call APIs are used for call terminal only.
 */

/* Originate an outgoing call */
int bt_manager_call_originate(uint16_t handle, uint8_t *remote_uri);

/* Accept the incoming call */
int bt_manager_call_accept(struct bt_audio_call *call);

/* Hold the call */
int bt_manager_call_hold(struct bt_audio_call *call);

/* Active the call which is locally held */
int bt_manager_call_retrieve(struct bt_audio_call *call);

/* Join a multiparty call */
int bt_manager_call_join(struct bt_audio_call **calls, uint8_t num_calls);

/* Terminate the call, NOTICE: reason is not used yet */
int bt_manager_call_terminate(struct bt_audio_call *call, uint8_t reason);

int bt_manager_call_allow_stream_connect(bool allowed);

int bt_manager_call_hangup_another_call(void);

int bt_manager_call_holdcur_answer_call(void);

int bt_manager_call_hangupcur_answer_call(void);

int bt_manager_call_switch_sound_source(void);

void bt_manager_call_event(int event, void *data, int size);

int bt_manager_call_get_status(void);

int bt_manager_audio_new_audio_conn_for_test(uint16_t handle, int type, int role);

int bt_manager_audio_delete_audio_conn_for_test(uint16_t handle);

int bt_manager_volume_up(void);

int bt_manager_volume_down(void);

/**
 * @brief Toggle music DSP effect (DAE) on/off for current playback.
 *
 * Updates CFG_AUDIO_EFFECT and applies to the active dumpable media player if any.
 *
 * @return 0 on success, negative errno on failure from media_player_set_effect_enable.
 */
int bt_manager_audio_effect_switch(void);

#define BT_VOLUME_TYPE_BR_MUSIC          0x00
#define BT_VOLUME_TYPE_BR_CALL           0x01
#define BT_VOLUME_TYPE_LE_AUDIO          0x02

int bt_manager_volume_set(uint8_t value,int type);

int bt_manager_volume_mute(void);

int bt_manager_volume_unmute(void);

int bt_manager_volume_unmute_up(void);

int bt_manager_volume_unmute_down(void);

int bt_manager_volume_client_up(void);

int bt_manager_volume_client_down(void);

int bt_manager_volume_client_set(uint8_t value);

int bt_manager_volume_client_mute(void);

int bt_manager_volume_client_unmute(void);

void bt_manager_volume_event(int event, void *data, int size);
void bt_manager_volume_event_to_app(uint16_t handle, uint8_t volume, uint8_t from_phone);

int bt_manager_mic_mute(void);

int bt_manager_mic_unmute(void);

int bt_manager_mic_mute_disable(void);

int bt_manager_mic_gain_set(uint8_t gain);

int bt_manager_mic_client_mute(void);

int bt_manager_mic_client_unmute(void);

int bt_manager_mic_client_gain_set(uint8_t gain);

void bt_manager_mic_event(int event, void *data, int size);

/*
 * BT Auracast broadcast audio
 */

/*
 * BT Broadcast Stream state
 */
#define BT_BROADCAST_STREAM_IDLE	0x0
#if 0
/* the broadcast audio stream is codec configured, need to configure qos */
#define BT_BROADCAST_STREAM_CODEC_CONFIGURED	0x1
/* the broadcast audio stream is qos configured, need to enable it */
#define BT_BROADCAST_STREAM_QOS_CONFIGURED	0x2
/* the broadcast audio stream is is ready or is transporting */
#define BT_BROADCAST_STREAM_STREAMING	0x3
#else
/* the broadcast audio stream is codec/qos configured, need to enable it */
#define BT_BROADCAST_STREAM_CONFIGURED	0x1
/* the broadcast audio stream is is ready or is transporting */
#define BT_BROADCAST_STREAM_STREAMING	0x2
#endif

struct bt_broadcast_subgroup_1 {
	uint8_t num_bis;	/* BIS level */
	uint8_t format;	/* BT Audio Codecs */
	uint8_t frame_duration;	/* Frame_Duration */
	uint8_t blocks;	/* Codec_Frame_Blocks_Per_SDU */
	uint16_t sample_rate;	/* unit: kHz */
	uint16_t octets;	/* Octets_Per_Codec_Frame */
	uint32_t locations;	/* Audio Locations */
	uint32_t language;	/* Language types */
	uint16_t contexts;	/* Audio Context types */

	struct bt_broadcast_bis bis[1];
};

struct bt_broadcast_subgroup_2 {
	uint8_t num_bis;	/* BIS level */
	uint8_t format;	/* BT Audio Codecs */
	uint8_t frame_duration;	/* Frame_Duration */
	uint8_t blocks;	/* Codec_Frame_Blocks_Per_SDU */
	uint16_t sample_rate;	/* unit: kHz */
	uint16_t octets;	/* Octets_Per_Codec_Frame */
	uint32_t locations;	/* Audio Locations */
	uint32_t language;	/* Language types */
	uint16_t contexts;	/* Audio Context types */

	struct bt_broadcast_bis bis[2];
};

struct bt_broadcast_chan_info {
	uint8_t format;	/* BT_AUDIO_CODEC_ */
	/* valid at config_codec time for master */
	uint8_t channels;	/* range: [1, 8] */
	/* valid at config_codec time for master */
	uint16_t octets;	/* octets for 1 channel (or 1 codec frame) */
	uint16_t sample;	/* sample_rate; unit: kHz */
	uint16_t kbps;	/* bit rate */
	uint16_t sdu;	/* sdu size */
	uint16_t contexts;	/* BT_AUDIO_CONTEXT_ */
	uint32_t locations;	/* BT_AUDIO_LOCATIONS_ */
	uint32_t interval;	/* sdu interval, unit: us */

#if 0
	uint8_t target_latency;
#endif
	uint8_t duration;	/* unit: ms */
	uint16_t max_latency;	/* max_transport_latency */
	uint32_t delay;	/* presentation_delay */
	uint8_t rtn;	/* retransmission_number */
	uint8_t phy;
	uint8_t framing;
	void * chan; /* timeline_owner*/
};

int bt_manager_broadcast_get_role(uint32_t handle);

int bt_manager_broadcast_get_audio_handle(uint32_t handle,uint8_t id);

int bt_manager_broadcast_stream_state(uint32_t handle, uint8_t id);

int bt_manager_broadcast_stream_info(struct bt_broadcast_chan *chan,
				struct bt_broadcast_chan_info *info);

/*
 * media delay is delay cost by audio effect or resample etc.
 * audio is to be rendered at presentation delay from sync pointer,if we set
 * media delay for bt, then bt host will trigger start at presentation delay
 * + sync delay - media delay,audio will be rendered at right time.
 */
int bt_manager_broadcast_stream_set_media_delay(struct bt_broadcast_chan *chan,
				int32_t delay_us);

int bt_manager_broadcast_stream_cb_register(struct bt_broadcast_chan *chan,
				bt_broadcast_trigger_cb trigger,
				bt_broadcast_trigger_cb period_trigger);

int bt_manager_broadcast_stream_start_cb_register(struct bt_broadcast_chan *chan,
				bt_audio_start_cb start_cb,void* param);

int bt_manager_broadcast_stream_tws_sync_cb_register(struct bt_broadcast_chan *chan,
				bt_broadcast_trigger_cb tws_sync_trigger);

int bt_manager_broadcast_stream_tws_sync_cb_register_1(struct bt_broadcast_chan *chan,
				bt_audio_start_cb tws_start_cb,void* param);

int bt_manager_broadcast_stream_set_tws_sync_cb_offset(struct bt_broadcast_chan *chan,
				int32_t offset_from_syncdelay);

/*
 * Broadcast Source
 */
int bt_manager_broadcast_source_create(struct bt_broadcast_source_create_param *param);

int bt_manager_broadcast_source_reconfig(void);

int bt_manager_broadcast_source_enable(uint32_t handle);

int bt_manager_broadcast_source_update(void);

int bt_manager_broadcast_source_disable(uint32_t handle);

int bt_manager_broadcast_source_release(uint32_t handle);

int bt_manager_broadcast_source_stream_set(struct bt_broadcast_chan *chan,
				io_stream_t stream);

/*
 * Send vendor-specific extended advertising data, the maximum of len is
 * limited to 64.
 *
 * Normaly type is BT_DATA_MANUFACTURER_DATA, other types are supported
 * also, such as BT_DATA_SVC_DATA16.
 * NOTICE: 0 equals to BT_DATA_MANUFACTURER_DATA!
 */
int bt_manager_broadcast_source_vnd_ext_send(uint32_t handle, uint8_t *buf,
				uint16_t len, uint8_t type);

/*
 * Send vendor-specific periodic advertising data, the maximum of len is
 * limited to 64.
 *
 * Normaly type is BT_DATA_MANUFACTURER_DATA, other types are supported
 * also, such as BT_DATA_SVC_DATA16.
 * NOTICE: 0 equals to BT_DATA_MANUFACTURER_DATA!
 */
int bt_manager_broadcast_source_vnd_per_send(uint32_t handle, uint8_t *buf,
				uint16_t len, uint8_t type);

/*
 * Send vendor-specific BIG Control subevent data, the maximum of len is
 * limited to 64.
 */
int bt_manager_broadcast_source_vnd_control_send(uint32_t handle, uint8_t *buf,
				uint16_t len);

/*
 * Broadcast Sink
 */
int bt_manager_broadcast_sink_init(void);

int bt_manager_broadcast_sink_exit(void);

int bt_manager_broadcast_sink_sync(uint32_t handle, uint32_t bis_indexes,
				const uint8_t broadcast_code[16]);

int bt_manager_broadcast_sink_release(uint32_t handle);

int bt_manager_broadcast_sink_stream_set(struct bt_broadcast_chan *chan,
				io_stream_t stream);

/* Register callback for vendor-specific extended advertising */
int bt_manager_broadcast_sink_register_ext_rx_cb(bt_broadcast_vnd_rx_cb cb);

/* Register callback for vendor-specific periodic advertising */
int bt_manager_broadcast_sink_register_per_rx_cb(uint32_t handle,
				bt_broadcast_vnd_rx_cb cb);

/* Register callback when PA sends it out */
int bt_manager_broadcast_source_register_per_tx_cb(uint32_t handle,
				bt_broadcast_vnd_rx_cb cb);

int bt_manager_broadcast_register_scan_filter_cb(bt_broadcast_scan_filter_cb cb);

/*
 * Automatic BIS(s) sync policy. For example, vendor-specific
 * broadcast source + broadcast sink case, broadcast sink
 * could sync to BIS(s) automatically.
 *
 * bt_manager_broadcast_sink_sync_loacations(): sync by loacations.
 */
int bt_manager_broadcast_sink_sync_loacations(uint32_t loacations);

int bt_manager_broadcast_filter_name(void);

int bt_manager_broadcast_filter_public_name(void);

int bt_manager_broadcast_filter_id(void);

// TODO: for Broadcast Assistant too?
/* Scan parameters for Broadcast Sink or Broadcast Assistant */
int bt_manager_broadcast_scan_param_init(struct bt_le_scan_param *param);

int bt_manager_broadcast_scan_start(void);

int bt_manager_broadcast_scan_stop(void);

/*
 * Broadcast Assistant
 */

/*
 * Scan Delegator
 */

/*
 * Broadcast General
 */
int bt_manager_broadcast_scan_start(void);

int bt_manager_broadcast_scan_stop(void);

int bt_manager_broadcast_filter_local_name(uint8_t *name);

int bt_manager_broadcast_filter_broadcast_name(uint8_t *name);

int bt_manager_broadcast_filter_broadcast_id(uint32_t id);

int bt_manager_broadcast_filter_company_id(uint32_t id);

int bt_manager_broadcast_filter_uuid16(uint16_t value);
/*rssi: -127 to +20 */
int bt_manager_broadcast_filter_rssi(int8_t rssi);
//Clear all filtering rules
int bt_manager_broadcast_clear_filter(void);

void bt_manager_broadcast_event(int event, void *data, int size);


int bt_manager_set_leaudio_foreground_dev(bool enable);

void bt_manager_audio_dump_info(void);

int bt_manager_broadcast_past_subscribe(uint16_t handle);
int bt_manager_broadcast_past_unsubscribe(uint16_t handle);

int bt_manager_broadcast_pa_set_info_transfer(uint32_t handle);

uint16_t bt_manager_audio_get_letws_handle(void);
/*
 * Specific APIs for LE Audio
 */

/* Connectable advertisement parameter for LE Audio Slave */
int bt_manager_audio_le_adv_param_init(struct bt_le_adv_param *param);

/* Scan parameters for LE Audio Master */
int bt_manager_audio_le_scan_param_init(struct bt_le_scan_param *param);

/* Create connection parameters for LE Audio Master */
int bt_manager_audio_le_conn_create_param_init(struct bt_conn_le_create_param *param);

/* Initial connection parameters for LE Audio Master */
int bt_manager_audio_le_conn_param_init(struct bt_le_conn_param *param);

/* Idle (such as CIS non-connected) connection parameters for LE Audio Master */
int bt_manager_audio_le_conn_idle_param_init(struct bt_le_conn_param *param);

int bt_manager_audio_le_conn_max_len(uint16_t handle);

int bt_manager_audio_le_vnd_register_rx_cb(uint16_t handle,
				bt_audio_vnd_rx_cb rx_cb);

int bt_manager_audio_le_vnd_send(uint16_t handle, uint8_t *buf, uint16_t len);

int bt_manager_audio_le_adv_cb_register(bt_audio_adv_cb start,
				bt_audio_adv_cb stop);

int bt_mamager_audio_set_remote_ble_addr(bt_addr_le_t *addr);
int bt_manager_audio_dir_adv_init(void);

/*
 * start bt_audio slave adv, paired with bt_manager_audio_le_stop_adv()
 *
 * For LE Audio slave only, it will start adv. It is used to control adv
 * by the user layer.
 */
int bt_manager_audio_le_start_adv(void);
int bt_manager_audio_le_stop_adv(void);

/*
 * pause bt_audio master scan, paired with bt_manager_audio_le_resume_scan()
 *
 * For LE Audio master only, it will stop scan and disconnect all master
 * connections.
 */
int bt_manager_audio_le_pause_scan(void);
int bt_manager_audio_le_resume_scan(void);
int bt_manager_audio_le_get_scan_status(void);

/*
 * pause bt_audio slave adv, paired with bt_manager_audio_le_resume_adv()
 *
 * For LE Audio slave only, it will pause adv. It will not stop adv directly
 * but notity the user layer instead.
 */
int bt_manager_audio_le_pause_adv(void);
int bt_manager_audio_le_resume_adv(void);

int bt_manager_audio_le_relay_control(bool enable);

int bt_manager_le_audio_adv_state(void);

int bt_manager_le_audio_adv_enable(void);

int bt_manager_le_audio_adv_disable(void);

int bt_manager_audio_takeover_flag(void);

/**
 * @brief bt manager get current audio active stream
 *
 * This routine get current audio active stream
 *
 * @return bt_audio_stream_e, see @bt_audio_stream_e
 */
int bt_manager_audio_current_stream(void);

int bt_manager_audio_get_leaudio_dev_num();

int bt_manager_audio_disconnect_all_phone_dev();

int bt_manager_is_idle_status();

void bt_manager_audio_set_scan_policy(uint8_t scan_type, uint8_t scan_policy_mode);

char *bt_manager_audio_get_last_connected_dev_name(void);

int8_t bt_manager_broadcast_get_sq_mode(void);

int8_t bt_manager_broadcast_set_sq_mode(int8_t sq_mode);

int bt_manager_audio_sq_mode_update(int8_t sq_mode);

uint32_t bt_manager_music_to_lea_volume(uint32_t music_vol);

uint32_t bt_manager_lea_to_music_volume(uint32_t lea_vol);

void bt_manager_pawr_check_connected_dev(void);

#endif /* __BT_MANAGER_AUDIO_H__ */
