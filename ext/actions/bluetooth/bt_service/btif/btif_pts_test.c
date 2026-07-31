/*
 * Copyright (c) 2016 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt srv api interface
 */

#define SYS_LOG_DOMAIN "btif_pts_test"
#include "btsrv_os_common.h"
#include "btsrv_inner.h"

int btif_pts_send_hfp_cmd(char *cmd)
{
	return btsrv_pts_send_hfp_cmd(cmd);
}

int btif_pts_hfp_active_connect_sco(void)
{
	return btsrv_pts_hfp_active_connect_sco();
}

int btif_pts_avrcp_get_play_status(void)
{
	if (btsrv_is_pts_test()) {
		btsrv_pts_avrcp_get_play_status();
	}
	return 0;
}

int btif_pts_avrcp_reg_notify_volume_change(void)
{
	if (btsrv_is_pts_test()) {
		btsrv_pts_avrcp_reg_notify_volume_change();
	}
	return 0;
}

int btif_pts_register_auth_cb(bool reg_auth)
{
	if (btsrv_is_pts_test()) {
		btsrv_pts_register_auth_cb(reg_auth);
	}

	return 0;
}

#ifdef CONFIG_BT_LEA_PTS_TEST
int btif_pts_register_processer(void)
{
	int ret = 0;
	ret = btsrv_register_msg_processer(MSG_BTSRV_PTS, &btsrv_pts_process);
	SYS_LOG_INF("result:%d\n",ret);
	return ret;
}

int btif_pts_start(btsrv_pts_callback cb)
{
	return btsrv_function_call(MSG_BTSRV_PTS, MSG_BTSRV_PTS_START, cb);
}

int btif_pts_stop(void)
{
	return btsrv_function_call(MSG_BTSRV_PTS, MSG_BTSRV_PTS_STOP, NULL);
}

void btif_pts_reset(void)
{
	btsrv_pts_reset();
}

uint8_t btif_pts_get_adv_state(void)
{
	return btsrv_pts_is_adv_enable();
}

void btif_pts_set_adv_ann_type(void)
{
	btsrv_pts_set_adv_ann_type();
}

int btif_pts_start_adv(pts_adv_type_e adv_type)
{
	return btsrv_pts_start_adv(adv_type);
}

void btif_pts_test_set_flag(uint8_t flags)
{
	btsrv_pts_test_set_flag(flags);
}

void btif_pts_test_clear_flag(uint8_t flags)
{
	btsrv_pts_test_clear_flag(flags);
}

uint8_t btif_pts_test_is_flag(pts_test_flags_e flag)
{
	return btsrv_pts_test_is_flag(flag);
}

void btif_pts_set_pacs_aac(struct bt_conn *conn, uint16_t src_aac, uint16_t sink_aac)
{
	pacs_srv_set_aac(conn, src_aac, sink_aac);
}

uint16_t btif_pts_get_pacs_aac(struct bt_conn *conn, uint8_t is_sink)
{
	return pacs_srv_cli_get_aac(conn, is_sink);
}

void btif_pts_set_pacs_sac(struct bt_conn *conn, uint16_t src_aac, uint16_t sink_aac)
{
	pacs_srv_set_sac(conn, src_aac, sink_aac);
}

int btif_pts_dev_connect
(
	const bt_addr_le_t *addr,
	struct bt_conn *conn,
	void *p_create_param,
	void *p_conn_param
)
{
	return btsrv_pts_dev_connect(addr, conn, p_create_param, p_conn_param);
}

int btif_pts_audio_core_event(struct bt_conn *conn, uint16_t evt_type, void *param)
{
	return btsrv_pts_audio_core_event(conn, evt_type, param);
}

void btif_pts_set_auto_audio_evt_flag(audio_core_evt_flags_e flag)
{
	btsrv_pts_set_le_audio_evt_flag(flag);
}

void btif_pts_clear_auto_audio_evt_flag(audio_core_evt_flags_e flag)
{
	btsrv_pts_clear_le_audio_evt_flag(flag);
}

uint8_t btif_pts_is_auto_evt(audio_core_evt_flags_e flag)
{
	return btsrv_pts_is_le_audio_evt_flag(flag);
}

void btif_pts_set_bass_recv_state(struct bt_conn *conn)
{
	btsrv_pts_bass_set_state(conn);
}

void btif_pts_reset_bass_recv_state(void)
{
	btsrv_pts_bass_reset_state();
}

void btif_pts_vcs_set_state(void *state, uint8_t vol_step)
{
	btsrv_pts_vcs_set_state(state, vol_step);
}

int btif_pts_vcs_notify_state(struct bt_conn *conn)
{
	return btsrv_pts_vcs_notify_state(conn);
}

void btif_pts_set_ascs_ccid_count(uint8_t ccid_cnt)
{
	btsrv_pts_ascs_cli_set_ccid_count(ccid_cnt);
}

void btif_pts_aics_set_mute(uint8_t mute_state)
{
	btsrv_pts_mics_aics_set_mute(mute_state);
}

void btif_pts_aics_set_gain_mode(uint8_t gain_mode)
{
	btsrv_pts_mics_aics_set_gain_mode(gain_mode);
}

int btif_pts_aics_state_notify(struct bt_conn *conn)
{
	return btsrv_pts_mics_aics_state_notify(conn);
}

void btif_pts_csis_set_sirk(uint8_t type, uint8_t *sirk_value)
{
	btsrv_csis_set_sirk(type, sirk_value);
}

int btif_pts_read_mcs_track_duration(uint16_t handle)
{
	return btsrv_mcs_read_track_duration(handle);
}

int btif_pts_read_mcs_track_position(uint16_t handle)
{
	return btsrv_mcs_read_track_position(handle);
}

int btif_pts_write_mcs_track_position(uint16_t handle, int32_t position)
{
	return btsrv_mcs_write_track_position(handle, position);
}


int btif_pts_write_media_control(uint16_t handle, uint8_t opcode)
{
	return btsrv_mcs_write_media_control(handle, opcode);
}

int btif_pts_set_le_security_level(bt_security_t sec_level)
{
	return btsrv_pts_set_le_security_level(sec_level);
}
#endif
