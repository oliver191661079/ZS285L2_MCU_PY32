/*
 * Copyright (c) 2021 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt audio interface
 */

#define SYS_LOG_DOMAIN "btif_audio"
#include "btsrv_os_common.h"
#include "btsrv_inner.h"

int btif_audio_register_processer(void)
{
	return btsrv_register_msg_processer(MSG_BTSRV_AUDIO, &btsrv_audio_process);
}

int btif_audio_init(struct bt_audio_config config, btsrv_audio_callback cb,
				void *data, int size)
{
	int size_need;

	size_need = btsrv_audio_memory_configure(config);
	if (size_need > size) {
		SYS_LOG_ERR("No memory %d %d", size_need, size);
		return -ENOMEM;
	}

	btsrv_audio_memory_init(config, cb, data, size);

	return 0;
}

int btif_audio_adv_param_init(struct bt_le_adv_param *param)
{
	return btsrv_audio_param_init(param, BTSRV_AUDIO_ADV_PARAM);
}

int btif_audio_scan_param_init(struct bt_le_scan_param *param)
{
	return btsrv_audio_param_init(param, BTSRV_AUDIO_SCAN_PARAM);
}

int btif_audio_conn_create_param_init(struct bt_conn_le_create_param *param)
{
	return btsrv_audio_param_init(param, BTSRV_AUDIO_CONN_CREATE_PARAM);
}

int btif_audio_conn_param_init(struct bt_le_conn_param *param)
{
	return btsrv_audio_param_init(param, BTSRV_AUDIO_CONN_PARAM);
}

int btif_audio_conn_idle_param_init(struct bt_le_conn_param *param)
{
	return btsrv_audio_param_init(param, BTSRV_AUDIO_CONN_IDLE_PARAM);
}

int btif_audio_exit(void)
{
	return btsrv_audio_exit();
}

int btif_audio_keys_clear(void)
{
	return btsrv_event_notify(MSG_BTSRV_AUDIO,
				MSG_BTSRV_AUDIO_KEYS_CLEAR, NULL);
}

int btif_audio_start(void)
{
	return btsrv_audio_start();
}

int btif_audio_stop(void)
{
	return btsrv_audio_stop();
}

int btif_audio_services_enable(void)
{
	return btsrv_audio_services_enable();
}

int btif_audio_services_disable(void)
{
	return btsrv_audio_services_disable();
}

int btif_audio_pause(void)
{
	return btsrv_function_call(MSG_BTSRV_AUDIO, MSG_BTSRV_AUDIO_PAUSE, NULL);
}

int btif_audio_resume(void)
{
	return btsrv_function_call(MSG_BTSRV_AUDIO, MSG_BTSRV_AUDIO_RESUME, NULL);
}

int btif_audio_pause_scan(void)
{
	return btsrv_audio_pause_scan();
}

int btif_audio_resume_scan(void)
{
	return btsrv_audio_resume_scan();
}

int btif_audio_get_scan_status(void)
{
	return btsrv_audio_get_scan_status();
}

int btif_audio_scan_recv_cb_register(bt_audio_scan_recv_cb cb)
{
	return btsrv_audio_scan_recv_cb_register(cb);
}

int btif_audio_pause_adv(void)
{
	return btsrv_audio_adv_control(true);
}

int btif_audio_resume_adv(void)
{
	return btsrv_audio_adv_control(false);
}

int btif_audio_update_adv_param(struct bt_le_adv_param *param)
{
	if (!param) {
		SYS_LOG_ERR("param NULL\n");
		return -EINVAL;
	}

	return btsrv_audio_update_adv_param(param);
}

int btif_audio_init_adv_data(struct bt_data *adv_data, uint8_t count)
{
	if (!adv_data || count == 0) {
		SYS_LOG_ERR("param NULL or count is 0\n");
		return -EINVAL;
	}
	return btsrv_audio_init_adv_data(adv_data, count);
}


int btif_audio_relay_control(bool enable)
{
	return btsrv_event_notify_value(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_RELAY_CONTROL,
					enable ? 1 : 0);
}

bool btif_audio_stopped(void)
{
	return btsrv_audio_stopped();
}

int btif_audio_server_cap_init(struct bt_audio_capabilities *caps)
{
	return btsrv_pacs_cap_init(caps);
}

int btif_audio_stream_config_codec(struct bt_audio_codec *codec)
{
	uint16_t len = sizeof(struct bt_audio_codec);

	return btsrv_event_notify_malloc(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_STREAM_CONFIG_CODEC,
					(uint8_t *)codec, len, 0);
}

int btif_audio_stream_prefer_qos(struct bt_audio_preferred_qos *qos)
{
	uint16_t len = sizeof(struct bt_audio_preferred_qos);

	return btsrv_event_notify_malloc(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_STREAM_PREFER_QOS,
					(uint8_t *)qos, len, 0);
}

int btif_audio_stream_config_qos(struct bt_audio_qoss *qoss)
{
	uint16_t len;

	len = qoss->num_of_qos * sizeof(struct bt_audio_qos);
	len += sizeof(struct bt_audio_qoss);

	return btsrv_event_notify_malloc(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_STREAM_CONFIG_QOS,
					(uint8_t *)qoss, len, 0);
}

int btif_audio_stream_bind_qos(struct bt_audio_qoss *qoss, uint8_t index)
{
	uint16_t len;

	len = qoss->num_of_qos * sizeof(struct bt_audio_qos);
	len += sizeof(struct bt_audio_qoss);

	return btsrv_event_notify_malloc(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_STREAM_BIND_QOS,
					(uint8_t *)qoss, len, index);
}

static int stream_op(struct bt_audio_chan **chans, uint8_t num_chans, int cmd,
				uint8_t code)
{
	uint16_t len = sizeof(struct btsrv_audio_stream_op);
	struct btsrv_audio_stream_op *op;
	uint8_t i;

	len += num_chans * sizeof(struct bt_audio_chan *);

	op = btsrv_event_buf_malloc(len);
	if (!op) {
		return -ENOMEM;
	}

	op->num_chans = num_chans;
	for (i = 0; i < num_chans; i++) {
		op->chans[i] = chans[i];
	}

	return btsrv_event_notify_buf(MSG_BTSRV_AUDIO, cmd, (uint8_t *)op, code);
}

static int stream_op_contexts(struct bt_audio_chan **chans, uint8_t num_chans,
				uint32_t contexts, int cmd, uint8_t code)
{
	uint16_t len = sizeof(struct btsrv_audio_stream_op_contexts);
	struct btsrv_audio_stream_op_contexts *op;
	uint8_t i;

	len += num_chans * sizeof(struct bt_audio_chan *);

	op = btsrv_event_buf_malloc(len);
	if (!op) {
		return -ENOMEM;
	}

	op->contexts = contexts;
	op->num_chans = num_chans;
	for (i = 0; i < num_chans; i++) {
		op->chans[i] = chans[i];
	}

	return btsrv_event_notify_buf(MSG_BTSRV_AUDIO, cmd, (uint8_t *)op, code);
}

int btif_audio_stream_sync_forward(struct bt_audio_chan **chans,
				uint8_t num_chans)
{
	return stream_op(chans, num_chans, MSG_BTSRV_AUDIO_STREAM_SYNC_FORWARD, 0);
}

int btif_audio_stream_enable(struct bt_audio_chan **chans,
				uint8_t num_chans, uint32_t contexts)
{
	return stream_op_contexts(chans, num_chans, contexts,
				MSG_BTSRV_AUDIO_STREAM_ENABLE, 0);
}

int btif_audio_stream_disble(struct bt_audio_chan **chans, uint8_t num_chans)
{
	return stream_op(chans, num_chans, MSG_BTSRV_AUDIO_STREAM_DISBLE, 0);
}

io_stream_t btif_audio_source_stream_create(struct bt_audio_chan **chans,
				uint8_t num_chans, uint32_t locations)
{
	return btsrv_audio_source_stream_create(chans, num_chans, locations);
}

int btif_audio_source_stream_set(struct bt_audio_chan **chans,
				uint8_t num_chans, io_stream_t stream,
				uint32_t locations)
{
	uint16_t len = sizeof(struct btsrv_audio_stream_op_set_loc);
	struct btsrv_audio_stream_op_set_loc *op;
	uint8_t i;

	len += num_chans * sizeof(struct bt_audio_chan *);

	op = mem_malloc(len);
	if (!op) {
		return -ENOMEM;
	}

	op->stream = stream;
	op->locations = locations;
	op->num_chans = num_chans;
	for (i = 0; i < num_chans; i++) {
		op->chans[i] = chans[i];
	}

	int ret = btsrv_ascs_source_stream_set(op);
	mem_free(op);
	return ret;
}

int btif_audio_sink_stream_set(struct bt_audio_chan *chan,
				io_stream_t stream)
{
	struct btsrv_audio_stream_op_set op = {
		.chan = *chan,
		.stream = stream,
	};

	return btsrv_ascs_sink_stream_set(&op);
}

int btif_audio_sink_stream_start(struct bt_audio_chan **chans,
				uint8_t num_chans)
{
	return stream_op(chans, num_chans, MSG_BTSRV_AUDIO_STREAM_START, 0);
}

int btif_audio_sink_stream_stop(struct bt_audio_chan **chans,
				uint8_t num_chans)
{
	return stream_op(chans, num_chans, MSG_BTSRV_AUDIO_STREAM_STOP, 0);
}

int btif_audio_stream_update(struct bt_audio_chan **chans,
				uint8_t num_chans, uint32_t contexts)
{
	return stream_op_contexts(chans, num_chans, contexts,
				MSG_BTSRV_AUDIO_STREAM_UPDATE, 0);
}

int btif_audio_stream_release(struct bt_audio_chan **chans,
				uint8_t num_chans)
{
	return stream_op(chans, num_chans, MSG_BTSRV_AUDIO_STREAM_RELEASE, 0);
}

int btif_audio_stream_released(struct bt_audio_chan *chan)
{
	return btsrv_event_notify_malloc(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_STREAM_RELEASED,
					(uint8_t *)chan, sizeof(*chan), 0);
}

int btif_audio_stream_disconnect(struct bt_audio_chan **chans,
				uint8_t num_chans)
{
	return stream_op(chans, num_chans, MSG_BTSRV_AUDIO_STREAM_DISCONNECT, 0);
}

void *btif_audio_stream_bandwidth_alloc(struct bt_audio_qoss *qoss)
{
	return btsrv_ascs_bandwidth_alloc(qoss);
}

int btif_audio_stream_bandwidth_free(void *cig_handle)
{
	return btsrv_event_notify(MSG_BTSRV_AUDIO,
				MSG_BTSRV_AUDIO_STREAM_BANDWIDTH_FREE,
				cig_handle);
}

int btif_audio_stream_cb_register(struct bt_audio_stream_cb *cb)
{
	return btsrv_ascs_stream_cb_register(cb);
}

int btif_audio_stream_set_tws_sync_cb_offset(struct bt_audio_chan *chan,int32_t offset_from_syncdelay)
{
	struct btsrv_audio_stream_op_set_offset op = {
		.chan = *chan,
		.offset_from_syncdelay = offset_from_syncdelay,
	};

	return btsrv_event_notify_malloc(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_STREAM_SET_TWS_SYNC_CB_OFFSET,
					(uint8_t *)&op, sizeof(op), 0);
}

int btif_audio_volume_up(uint16_t handle)
{
	return btsrv_event_notify_value(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_VOLUME_UP, handle);
}

int btif_audio_volume_down(uint16_t handle)
{
	return btsrv_event_notify_value(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_VOLUME_DOWN, handle);
}

int btif_audio_volume_set(uint16_t handle, uint8_t value)
{
	int data = (handle << 16) | value;

	return btsrv_event_notify_value(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_VOLUME_SET, data);
}

int btif_audio_volume_mute(uint16_t handle)
{
	return btsrv_event_notify_value(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_VOLUME_MUTE, handle);
}

int btif_audio_volume_unmute(uint16_t handle)
{
	return btsrv_event_notify_value(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_VOLUME_UNMUTE, handle);
}

int btif_audio_volume_unmute_up(uint16_t handle)
{
	return btsrv_event_notify_value(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_VOLUME_UNMUTE_UP, handle);
}

int btif_audio_volume_unmute_down(uint16_t handle)
{
	return btsrv_event_notify_value(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_VOLUME_UNMUTE_DOWN, handle);
}

int btif_audio_mic_mute(uint16_t handle)
{
	return btsrv_event_notify_value(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_MIC_MUTE, handle);
}
int btif_audio_mic_unmute(uint16_t handle)
{
	return btsrv_event_notify_value(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_MIC_UNMUTE, handle);
}

int btif_mics_mute_get(uint16_t handle)
{
	return btsrv_mics_mute_get(handle);
}

int btif_audio_mic_mute_disable(uint16_t handle)
{
	return btsrv_event_notify_value(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_MIC_MUTE_DISABLE, handle);
}

int btif_audio_mic_gain_set(uint16_t handle, uint8_t value)
{
	int data = (handle << 16) | value;

	return btsrv_event_notify_value(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_MIC_GAIN_SET, data);
}

int btif_mcs_media_state_get(uint16_t handle)
{
	return btsrv_event_notify_value(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_MEDIA_GET_MCS_STATE, handle);
}

int btif_audio_media_play(uint16_t handle)
{
	return btsrv_event_notify_value(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_MEDIA_PLAY, handle);
}

int btif_audio_media_pause(uint16_t handle)
{
	return btsrv_event_notify_value(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_MEDIA_PAUSE, handle);
}

int btif_audio_media_fast_rewind(uint16_t handle)
{
	return btsrv_event_notify_value(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_MEDIA_FAST_REWIND, handle);
}

int btif_audio_media_fast_forward(uint16_t handle)
{
	return btsrv_event_notify_value(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_MEDIA_FAST_FORWARD, handle);
}

int btif_audio_media_stop(uint16_t handle)
{
	return btsrv_event_notify_value(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_MEDIA_STOP, handle);
}

int btif_audio_media_play_prev(uint16_t handle)
{
	return btsrv_event_notify_value(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_MEDIA_PLAY_PREV, handle);
}

int btif_audio_media_play_next(uint16_t handle)
{
	return btsrv_event_notify_value(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_MEDIA_PLAY_NEXT, handle);
}

int btif_audio_conn_max_len(uint16_t handle)
{
	return btsrv_audio_conn_max_len(handle);
}

int btif_audio_vnd_register_rx_cb(uint16_t handle, bt_audio_vnd_rx_cb rx_cb)
{
	struct btsrv_audio_vnd_cb cb = {
		.handle = handle,
		.rx_cb = rx_cb,
	};

	return btsrv_event_notify_malloc(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_VND_CB_REGISTER,
					(uint8_t *)&cb, sizeof(cb), 0);
}

int btif_audio_cb_event_notify(bt_audio_event_proc_cb cb, void *event_param, uint32_t len)
{
	void *param_ptr = NULL;
	struct bt_audio_cb_event_proc_t event_proc = {0};
	int ret = 0;
	if (len && event_param) {
		param_ptr = mem_malloc(len);
		if (param_ptr == NULL) {
			SYS_LOG_ERR("alloc failed\n");
			return -ENOMEM;
		}
		memcpy(param_ptr, event_param, len);
	}

	event_proc.cb = cb;
	event_proc.param = param_ptr;

	ret = btsrv_event_notify_malloc(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_CB_EVENT_PROC,
					(uint8_t *)(&event_proc), sizeof(event_proc), 0);
	if (ret && param_ptr) {
		mem_free(param_ptr);
	}
	return ret;
}

int btif_audio_vnd_send(uint16_t handle, uint8_t *buf, uint16_t len)
{
	return btsrv_audio_vnd_send(handle, buf, len);
}

int btif_audio_vnd_connected(uint16_t handle)
{
	return btsrv_event_notify_value(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_VND_CONNECTED, handle);
}

int btif_audio_get_remote_name(uint16_t handle, get_remote_lea_name_cb callback)
{
	return btsrv_audio_get_remote_name(handle, callback);
}

int btif_audio_adv_cb_register(bt_audio_adv_cb start, bt_audio_adv_cb stop)
{
	struct btsrv_audio_adv_cb cb = {
		.start = start,
		.stop = stop,
	};

	return btsrv_audio_adv_cb_register(&cb);
}

int btif_audio_start_adv(void)
{
	return btsrv_audio_start_adv();
}

int btif_audio_stop_adv(void)
{
	return btsrv_audio_stop_adv();
}

int btif_audio_call_originate(uint16_t handle, uint8_t *remote_uri)
{
	struct btsrv_audio_call_originate_op op;

	op.handle = handle;
	strncpy(op.uri, remote_uri, BT_AUDIO_CALL_URI_LEN - 1);

	return btsrv_event_notify_malloc(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_CALL_ORIGINATE,
					(uint8_t *)&op, sizeof(op), 0);
}

int btif_audio_call_accept(struct bt_audio_call *call)
{
	return btsrv_event_notify_malloc(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_CALL_ACCEPT,
					(uint8_t *)call, sizeof(*call), 0);
}

int btif_audio_call_hold(struct bt_audio_call *call)
{
	return btsrv_event_notify_malloc(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_CALL_HOLD,
					(uint8_t *)call, sizeof(*call), 0);
}

int btif_audio_call_retrieve(struct bt_audio_call *call)
{
	return btsrv_event_notify_malloc(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_CALL_RETRIEVE,
					(uint8_t *)call, sizeof(*call), 0);
}

int btif_audio_call_terminate(struct bt_audio_call *call)
{
	return btsrv_event_notify_malloc(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_CALL_TERMINATE,
					(uint8_t *)call, sizeof(*call), 0);
}

int btif_audio_call_read_tbs_chrc(struct bt_conn *conn, uint8_t chrc_type)
{
	return call_cli_read_tbs_chrc(conn, chrc_type);
}

int btif_audio_call_set_strength_report_interval(struct bt_conn *conn, uint8_t interval, bool with_rsp)
{
	return call_cli_set_strength_report_interval(conn, interval, with_rsp);
}

/* call control as central */
int btif_audio_call_control_operation(struct bt_audio_call *call, uint8_t opcode)
{
	return btsrv_tbs_cli_control_operation(call, opcode);
}

int btif_audio_call_remote_originate(uint8_t *uri)
{
	return btsrv_tbs_remote_originate(uri);
}

int btif_audio_call_remote_alerting(struct bt_audio_call *call)
{
	return btsrv_tbs_remote_alerting(call);
}

int btif_audio_call_remote_accept(struct bt_audio_call *call)
{
	return btsrv_tbs_remote_accept(call);
}

int btif_audio_call_remote_hold(struct bt_audio_call *call)
{
	return btsrv_tbs_remote_hold(call);
}

int btif_audio_call_remote_retrieve(struct bt_audio_call *call)
{
	return btsrv_tbs_remote_retrieve(call);
}

int btif_audio_call_srv_terminate(struct bt_audio_call *call, uint8_t reason)
{
	return btsrv_tbs_gateway_terminate(call, reason);
}

int btif_audio_call_srv_originate(uint8_t *uri)
{
	return btsrv_tbs_gateway_originate(uri);
}

int btif_audio_call_set_target_uri(struct bt_audio_call *call, uint8_t *uri)
{
	return tbs_srv_set_target_uri(call, uri);
}

int btif_audio_call_set_friendly_name(struct bt_audio_call *call, uint8_t *uri)
{
	return tbs_srv_set_friendly_name(call, uri);
}

int btif_audio_call_set_provider(char *provider)
{
	return tbs_srv_set_provider(provider);
}

int btif_audio_call_set_technology(uint8_t technology)
{
	return tbs_srv_set_technology(technology);
}

int btif_audio_call_set_status_flags(uint8_t status)
{
	return tbs_srv_set_status_flags(status);
}

int btif_audio_call_state_notify(void)
{
	return tbs_srv_call_state_notify();
}

int btif_audio_call_srv_accept(struct bt_audio_call *call)
{
	return tbs_srv_accept(call);
}

int btif_audio_call_srv_hold(struct bt_audio_call *call)
{
	return tbs_srv_hold(call);
}

int btif_audio_call_srv_retrieve(struct bt_audio_call *call)
{
	return tbs_srv_retrieve(call);
}

uint16_t btif_audio_get_local_tmas_role(void)
{
	return tmas_srv_get_local_tmas_role();
}

uint8_t btif_audio_get_loc_gmas_role(void)
{
	return gmas_srv_get_loc_role();
}

int btif_audio_set_ble_tws_addr(bt_addr_le_t *addr)
{
	int ret;
	if(addr){
		ret = btsrv_event_notify_malloc(MSG_BTSRV_AUDIO,
						MSG_BTSRV_AUDIO_SET_BLE_TWS_ADDR,
						(uint8_t *)(addr), sizeof(bt_addr_le_t), 0);
	}else{
		ret = btsrv_event_notify_value(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_SET_BLE_TWS_ADDR, 0);
	}
	return ret;
}

int btif_broadcast_scan_param_init(struct bt_le_scan_param *param)
{
	return btsrv_broadcast_scan_param_init(param);
}

int btif_broadcast_scan_start(void)
{
	return btsrv_event_notify_ext(MSG_BTSRV_AUDIO,
				MSG_BTSRV_AUDIO_BROADCAST, NULL,
				BTSRV_BROADCAST_SCAN_START);
}

int btif_broadcast_scan_stop(void)
{
	return btsrv_event_notify_ext(MSG_BTSRV_AUDIO,
				MSG_BTSRV_AUDIO_BROADCAST, NULL,
				BTSRV_BROADCAST_SCAN_STOP);
}

int btif_broadcast_source_create(struct bt_broadcast_source_create_param *param)
{
	return btsrv_broadcast_source_create(param);
}

int btif_broadcast_source_reconfig(uint32_t handle,
				struct bt_broadcast_qos *qos,
				uint8_t num_subgroups,
				struct bt_broadcast_subgroup *subgroup)
{
	struct btsrv_broadcast_reconfig_param *param;
	struct bt_broadcast_subgroup *tmp;
	uint8_t i;
	uint16_t len;

	// TODO: move to bt_manager layer?!
	if (!qos && !(num_subgroups && subgroup)) {
		return -EINVAL;
	}

	len = sizeof(struct btsrv_broadcast_reconfig_param);

	for (i = 0; i < num_subgroups; i++) {
		tmp = &subgroup[i];
		len += sizeof(struct bt_broadcast_subgroup);
		len += tmp->num_bis * sizeof(struct bt_broadcast_bis);
	}

	param = btsrv_event_buf_malloc(len);
	if (!param) {
		return -ENOMEM;
	}

	param->handle = handle;

	if (num_subgroups && subgroup) {
		param->codec_valid = 1;
		param->num_subgroups = num_subgroups;
		len -= sizeof(struct btsrv_broadcast_reconfig_param);
		memcpy(param->subgroup, subgroup, len);
	} else {
		param->codec_valid = 0;
	}

	if (qos) {
		param->qos_valid = 1;
		memcpy(&param->qos, qos, sizeof(*qos));
	} else {
		param->qos_valid = 0;
	} 

	return btsrv_event_notify_buf(MSG_BTSRV_AUDIO,
				MSG_BTSRV_AUDIO_BROADCAST,
				(uint8_t *)param,
				BTSRV_BROADCAST_SOURCE_RECONFIG);
}

#if 0
int btif_broadcast_source_update(struct bt_audio_chan **chans,
				uint8_t num_chans, uint16_t contexts,
				uint32_t language)
{
	uint16_t len = sizeof(struct btsrv_broadcast_update_param);
	struct btsrv_broadcast_update_param *param;
	uint8_t i;

	len += num_chans * sizeof(struct bt_audio_chan *);

	param = btsrv_event_buf_malloc(len);
	if (!param) {
		return -ENOMEM;
	}

	param->language = language;
	param->contexts = contexts;
	param->num_chans = num_chans;
	for (i = 0; i < num_chans; i++) {
		param->chans[i] = chans[i];
	}

	return btsrv_event_notify_buf(MSG_BTSRV_AUDIO,
				MSG_BTSRV_AUDIO_BROADCAST,
				(uint8_t *)param,
				BTSRV_BROADCAST_SOURCE_UPDATE);
}
#else
// TODO: it can only update for a subgroup!!!
int btif_broadcast_source_update(struct bt_broadcast_chan *chan,
				uint16_t contexts, uint32_t language)
{
	uint16_t len = sizeof(struct btsrv_broadcast_update_param);
	struct btsrv_broadcast_update_param *param;

	param = btsrv_event_buf_malloc(len);
	if (!param) {
		return -ENOMEM;
	}

	param->handle = chan->handle;
	param->id = chan->id;
	param->language = language;
	param->contexts = contexts;

	return btsrv_event_notify_buf(MSG_BTSRV_AUDIO,
				MSG_BTSRV_AUDIO_BROADCAST,
				(uint8_t *)param,
				BTSRV_BROADCAST_SOURCE_UPDATE);
}
#endif

// TODO: support broadcast start/stop controlled by app_layer

// TODO: handle or struct bt_broadcast_chan *chan???
int btif_broadcast_source_enable(uint32_t handle)
{
	return btsrv_event_notify_value_ext(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_BROADCAST, handle,
					BTSRV_BROADCAST_SOURCE_ENABLE);
}

int btif_broadcast_source_disable(uint32_t handle)
{
/*
  The complete disconnection time of the big terminate controller is about 200ms, 
  in order to avoid insufficient memory 
  when the disconnection is not yet released and another application is made 
*/	
#if 0
	return btsrv_event_notify_value_ext(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_BROADCAST, handle,
					BTSRV_BROADCAST_SOURCE_DISABLE);
#else
    return btsrv_broadcast_source_disable(handle);
#endif 
}

int btif_broadcast_source_release(uint32_t handle)
{
#if 0
	return btsrv_event_notify_value_ext(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_BROADCAST, handle,
					BTSRV_BROADCAST_SOURCE_RELEASE);
#else
    return btsrv_broadcast_source_release(handle);
#endif 
}

int btif_broadcast_source_stream_set(struct bt_broadcast_chan *chan,
				io_stream_t stream)
{
	return btsrv_broadcast_source_stream_set(chan, stream);
}

int btif_broadcast_source_ext_adv_send(uint32_t handle, uint8_t *buf,
				uint16_t len, uint8_t type)
{
	return btsrv_broadcast_source_ext_adv_send(handle, buf, len, type);
}

int btif_broadcast_source_per_adv_send(uint32_t handle, uint8_t *buf,
				uint16_t len, uint8_t type)
{
	return btsrv_broadcast_source_per_adv_send(handle, buf, len, type);
}

int btif_broadcast_sink_sync(uint32_t handle, uint32_t bis_indexes,
				const uint8_t broadcast_code[16])
{
	return btsrv_broadcast_sink_sync(handle, bis_indexes, broadcast_code);
}

int btif_broadcast_sink_release(uint32_t handle)
{
	return btsrv_event_notify_value_ext(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_BROADCAST, handle,
					BTSRV_BROADCAST_SINK_RELEASE);
}

int btif_broadcast_sink_stream_set(struct bt_broadcast_chan *chan,
				io_stream_t stream)
{
	return btsrv_broadcast_sink_stream_set(chan, stream);
}

int btif_broadcast_sink_register_ext_rx_cb(bt_broadcast_vnd_rx_cb cb)
{
	return btsrv_broadcast_sink_register_ext_rx_cb(cb);
}

int btif_broadcast_sink_register_per_rx_cb(uint32_t handle,
				bt_broadcast_vnd_rx_cb cb)
{
	return btsrv_broadcast_sink_register_per_rx_cb(handle, cb);
}

int btif_broadcast_register_scan_filter_cb(bt_broadcast_scan_filter_cb cb)
{
	return btsrv_broadcast_register_scan_filter_cb(cb);
}

int btif_broadcast_source_register_per_tx_cb(uint32_t handle,
				bt_broadcast_vnd_rx_cb cb)
{
   return btsrv_broadcast_source_register_per_tx_cb(handle,cb);
}

int btif_broadcast_sink_sync_loacations(uint32_t loacations)
{
	return btsrv_broadcast_sink_sync_loacations(loacations);
}

int btif_broadcast_filter_local_name(uint8_t *name)
{
	return btsrv_broadcast_filter_local_name(name);
}

int btif_broadcast_filter_broadcast_name(uint8_t *name)
{
	return btsrv_broadcast_filter_broadcast_name(name);
}

int btif_broadcast_filter_broadcast_id(uint32_t id)
{
	return btsrv_broadcast_filter_broadcast_id(id);
}

int btif_broadcast_filter_company_id(uint32_t id)
{
	return btsrv_broadcast_filter_company_id(id);
}

int btif_broadcast_filter_uuid16(uint16_t value)
{
	return btsrv_broadcast_filter_uuid16(value);
}

int btif_broadcast_filter_rssi(int8_t rssi)
{
	return btsrv_broadcast_filter_rssi(rssi);
}

int btif_broadcast_clear_filter(void)
{
	return btsrv_broadcast_clear_filter();
}

int btif_broadcast_sink_sync_pa(void)	// TODO:
{
	return 0;
}

int btif_broadcast_assistant_xxx(void)
{
	return 0;
}

int btif_broadcast_assistant_scan_start(uint16_t handle)
{
	return btsrv_event_notify_value_ext(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_BROADCAST, handle,
					BTSRV_BROADCAST_ASST_SCAN_START);
}

int btif_broadcast_assistant_scan_stop(uint16_t handle)
{
	return btsrv_event_notify_value_ext(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_BROADCAST, handle,
					BTSRV_BROADCAST_ASST_SCAN_STOP);
}

// TODO: argv enough, what for recv_state???
int btif_broadcast_assistant_add_source(uint16_t handle, uint32_t handle2)
{
	return 0;
}

int btif_broadcast_assistant_modify_source(uint16_t handle, uint32_t handle2)
{
	return 0;
}

int btif_broadcast_assistant_release_source(uint16_t handle, uint32_t handle2)
{
	return 0;
}

int btif_broadcast_assistant_set_code(uint16_t handle, uint32_t handle2,
				uint8_t broadcast_code[16])
{
	return 0;
}



/**********************************************/
int btif_broadcast_delegator_xxx(void)
{
	return 0;
}

int btif_broadcast_stream_cb_register(struct bt_broadcast_stream_cb *cb)
{
	return btsrv_broadcast_stream_cb_register(cb);
}

int btif_broadcast_stream_set_media_delay(struct bt_broadcast_chan *chan,
				int32_t delay_us)
{
	struct btsrv_broadcast_stream_op_set_media_delay op;
	op.chan.handle = chan->handle;
	op.chan.id = chan->id;
	op.delay_us = delay_us;

	return btsrv_event_notify_malloc(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_BROADCAST,
					(uint8_t *)&op, sizeof(op), BTSRV_BROADCAST_STREAM_SET_MEDIA_DELAY);
}

int btif_broadcast_stream_set_tws_sync_cb_offset(struct bt_broadcast_chan *chan,
				int32_t offset_from_syncdelay)
{
	struct btsrv_broadcast_stream_op_set_offset op;
	op.chan.handle = chan->handle;
	op.chan.id = chan->id;
	op.offset_from_syncdelay = offset_from_syncdelay;

	return btsrv_event_notify_malloc(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_BROADCAST,
					(uint8_t *)&op, sizeof(op), BTSRV_BROADCAST_STREAM_SET_TWS_SYNC_CB_OFFSET);
}

int btif_broadcast_source_set_retransmit(struct bt_broadcast_chan *chan,uint8_t number)
{
	return btsrv_broadcast_source_set_retransmit(chan,number);
}

int btif_broadcast_pa_set_info_transfer(uint32_t handle)
{
    return btsrv_event_notify_value_ext(MSG_BTSRV_AUDIO,MSG_BTSRV_AUDIO_BROADCAST,
                    handle,BTSRV_BROADCAST_PA_SET_INFO_TRANSFER);
}

int btif_broadcast_past_subscribe(uint16_t handle)
{
    return btsrv_event_notify_value_ext(MSG_BTSRV_AUDIO,MSG_BTSRV_AUDIO_BROADCAST,
                    handle,BTSRV_BROADCAST_PAST_SUBSCRIBE);
}

int btif_broadcast_past_unsubscribe(uint16_t handle)
{
    return btsrv_event_notify_value_ext(MSG_BTSRV_AUDIO,
					MSG_BTSRV_AUDIO_BROADCAST,
					handle,BTSRV_BROADCAST_PAST_UNSUBSCRIBE);
}

int btif_cas_set_rank(uint8_t rank)
{
	return btsrv_cas_set_rank(rank);
}

int btif_cas_set_lock(struct bt_conn *conn, uint8_t val)
{
	return btsrv_cas_set_lock(conn, val);
}

int btif_cas_lock_notify(struct bt_conn *conn)
{
	return btsrv_cas_lock_notify(conn);
}

int btif_cas_set_size(uint8_t size)
{
	return btsrv_cas_set_size(size);
}

int btif_pacs_set_sink_loc(uint32_t locations)
{
	return pacs_srv_set_sink_loc(locations);
}

int btif_audio_set_sirk(uint8_t type, uint8_t *sirk_value)
{
	return btsrv_csis_set_sirk(type, sirk_value);
}

int btif_audio_update_rsi(uint8_t **rsi_out)
{
	return btsrv_audio_update_rsi(rsi_out);
}

struct bt_audio_config * btif_audio_get_cfg_param(void)
{
	return btsrv_audio_get_cfg_param();
}

int btif_audio_get_last_leaudio_device(bt_addr_le_t* dev)
{
	return btsrv_audio_get_last_leaudio_device(dev);
}

