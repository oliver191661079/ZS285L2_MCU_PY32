/*
 * Copyright (c) 2016 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt service core interface
 */
#define SYS_LOG_DOMAIN "btsrv_pts"
#include "btsrv_os_common.h"
#include "btsrv_inner.h"
#include <acts_bluetooth/hci.h>
#include <acts_bluetooth/bluetooth.h>

#ifdef CONFIG_BT_PTS_TEST

extern int bt_pts_conn_creat_add_sco_cmd(struct bt_conn *brle_conn);

int btsrv_pts_send_hfp_cmd(char *cmd)
{
	struct bt_conn *br_conn = btsrv_rdm_hfp_get_actived_dev();

	if (br_conn == NULL) {
		return -EIO;
	}

	hostif_bt_hfp_hf_send_cmd(br_conn, cmd);
	return 0;
}

int btsrv_pts_hfp_active_connect_sco(void)
{
	struct bt_conn *br_conn = btsrv_rdm_hfp_get_actived_dev();

	if (br_conn == NULL) {
		return -EIO;
	}

	hostif_bt_conn_create_sco(br_conn);
	return 0;
}

int btsrv_pts_avrcp_get_play_status(void)
{
	struct bt_conn *br_conn = btsrv_rdm_avrcp_get_connected_dev();

	if (br_conn == NULL) {
		return -EIO;
	}

	hostif_bt_avrcp_ct_get_play_status(br_conn);
	return 0;
}

int btsrv_pts_avrcp_reg_notify_volume_change(void)
{
	struct bt_conn *br_conn = btsrv_rdm_avrcp_get_connected_dev();

	if (br_conn == NULL) {
		return -EIO;
	}

	hostif_bt_avrcp_ct_get_capabilities(br_conn);
	os_sleep(100);
	hostif_bt_avrcp_ct_register_notification(br_conn);
	return 0;
}

static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
}

static void auth_passkey_confirm(struct bt_conn *conn, unsigned int passkey)
{
	hostif_bt_conn_ssp_confirm_reply(conn);
}

static void auth_pincode_entry(struct bt_conn *conn, bool highsec)
{
}

static void auth_cancel(struct bt_conn *conn)
{
}

static void auth_pairing_confirm(struct bt_conn *conn)
{
}

static const struct bt_conn_auth_cb auth_cb_display_yes_no = {
	.passkey_display = auth_passkey_display,
	.passkey_entry = NULL,
	.passkey_confirm = auth_passkey_confirm,
	.pincode_entry = auth_pincode_entry,
	.cancel = auth_cancel,
	.pairing_confirm = auth_pairing_confirm,
};

int btsrv_pts_register_auth_cb(bool reg_auth)
{
	if (reg_auth) {
		hostif_bt_conn_auth_cb_register(&auth_cb_display_yes_no);
	} else {
		hostif_bt_conn_auth_cb_register(NULL);
	}

	return 0;
}
#endif

#ifdef CONFIG_BT_LEA_PTS_TEST
typedef struct
{
	btsrv_pts_callback p_cb;
	uint8_t is_adv_enable;
	uint8_t adv_ann_type;
	struct bt_le_adv_param adv_param;
	struct bt_data adv_data;
	uint8_t data_value[10];
	audio_core_evt_flags_e evt_flag;
} btsrv_pts_ctx;

static btsrv_pts_ctx s_pts_ctx = {0};
static uint8_t pts_broadcast_code[16] = {0x01,0x02,0x68,0x05,0x53,0xF1,0x41,0x5A,0xA2,0x65,0xBB,0xAF,0xC6,0xEA,0x03,0xB8};

void btsrv_pts_set_adv_ann_type(void)
{
	s_pts_ctx.adv_ann_type = 1;
}

static void pts_advertise_data(uint32_t ops)
{
	struct bt_le_adv_param *param = &s_pts_ctx.adv_param;
	int err;

	param->id = BT_ID_DEFAULT;
	param->interval_min = BT_GAP_ADV_FAST_INT_MIN_2;
	param->interval_max = BT_GAP_ADV_FAST_INT_MAX_2;
	param->options = ops;

	err = hostif_bt_le_adv_start(param, &s_pts_ctx.adv_data, 1, NULL, 0);
	if (err < 0 && err != (-EALREADY)) {
		SYS_LOG_ERR("Failed to start advertising (err %d)", err);
	} else {
		SYS_LOG_INF("pts Advertising started");
	}
}

int btsrv_pts_start_adv(pts_adv_type_e adv_type)
{
	switch(adv_type)
	{
		case PTS_ADV_TYPE_NONE:
		{
			s_pts_ctx.is_adv_enable = 0;
			hostif_bt_le_adv_stop();
			btsrv_audio_stop_adv();
			btsrv_audio_start_adv();
			break;
		}

		case PTS_ADV_TYPE_CONNECTABLE:
		{
			s_pts_ctx.is_adv_enable = 0;
			btsrv_audio_stop_adv();
			hostif_bt_le_adv_stop();
			s_pts_ctx.adv_data.type = BT_DATA_FLAGS;
			s_pts_ctx.data_value[0] = BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR;
			s_pts_ctx.adv_data.data_len = 1;
			s_pts_ctx.adv_data.data = s_pts_ctx.data_value;
			uint32_t ops = BT_LE_ADV_OPT_USE_IDENTITY | BT_LE_ADV_OPT_CONNECTABLE;

			pts_advertise_data(ops);
			break;
		}

		case TMAP_DISCOVERY:
		{
			btsrv_audio_stop_adv();
			s_pts_ctx.is_adv_enable = 1;

			s_pts_ctx.adv_data.type = BT_DATA_SVC_DATA16;
			uint16_t uuid = 0x1855;
			uint16_t role = 0x003f;
			memcpy(s_pts_ctx.data_value, (uint8_t *)&uuid, 2);
			memcpy(&s_pts_ctx.data_value[2], (uint8_t *)&role, 2);
			s_pts_ctx.adv_data.data_len = 4;
			s_pts_ctx.adv_data.data = s_pts_ctx.data_value;

			btsrv_audio_start_adv();
			s_pts_ctx.is_adv_enable = 0;
			break;
		}

		case BASS_DISCOVERY:
		{
			btsrv_audio_stop_adv();
			s_pts_ctx.is_adv_enable = 1;

			s_pts_ctx.adv_data.type = BT_DATA_SVC_DATA16;
			uint16_t uuid = 0x184F;
			memcpy(s_pts_ctx.data_value, (uint8_t *)&uuid, 2);
			s_pts_ctx.adv_data.data_len = 2;
			s_pts_ctx.adv_data.data = s_pts_ctx.data_value;

			btsrv_audio_start_adv();
			s_pts_ctx.is_adv_enable = 0;
			break;
		}

		case BAP_DISCOVERY:
		{
			btsrv_audio_stop_adv();
			s_pts_ctx.is_adv_enable = 1;

			s_pts_ctx.adv_data.type = BT_DATA_SVC_DATA16;
			uint16_t uuid = 0x184E;
			uint8_t type = 0;
			uint32_t audio_contexts = 2;
			uint8_t metadata_len = 0;
			uint8_t offset = 0;
			memcpy(&s_pts_ctx.data_value[offset], (uint8_t *)&uuid, 2);
			offset += 2;
			s_pts_ctx.data_value[offset++] = type;
			memcpy(&s_pts_ctx.data_value[offset], (uint8_t *)&audio_contexts, 4);
			offset += 4;
			s_pts_ctx.data_value[offset++] = metadata_len;
			s_pts_ctx.adv_data.data_len = offset;
			s_pts_ctx.adv_data.data = s_pts_ctx.data_value;

			btsrv_audio_start_adv();
			s_pts_ctx.is_adv_enable = 0;
			break;
		}

		case CAP_DISCOVERY:
		{
			btsrv_audio_stop_adv();
			s_pts_ctx.is_adv_enable = 1;

			s_pts_ctx.adv_data.type = BT_DATA_SVC_DATA16;
			/* Pararms:UUID = 0x1853, Announcement Type = 0x00 */
			uint16_t uuid = 0x1853;
			uint8_t type = s_pts_ctx.adv_ann_type;
			memcpy(s_pts_ctx.data_value, (uint8_t *)&uuid, 2);
			s_pts_ctx.data_value[2] = type;
			s_pts_ctx.adv_data.data_len = 3;
			s_pts_ctx.adv_data.data = s_pts_ctx.data_value;

			btsrv_audio_start_adv();
			s_pts_ctx.is_adv_enable = 0;
			break;
		}

		case CSIP_DISCOVERY:
		{
			btsrv_audio_stop_adv();
			/* start csis rsi adv */
			s_pts_ctx.is_adv_enable = 1;
			btsrv_pts_test_set_flag(PTS_TEST_FLAG_CSIP);
			btsrv_audio_start_adv();
			/* clear cisp flag */
			s_pts_ctx.is_adv_enable = 0;
			btsrv_pts_test_clear_flag(PTS_TEST_FLAG_CSIP);
			break;
		}

		case GAP_NON_DISCOVERABLE:
		{
			s_pts_ctx.is_adv_enable = 1;
			hostif_bt_le_adv_stop();

			s_pts_ctx.adv_data.type = BT_DATA_FLAGS;
			s_pts_ctx.data_value[0] = BT_LE_AD_NO_BREDR;
			s_pts_ctx.adv_data.data_len = 1;
			s_pts_ctx.adv_data.data = s_pts_ctx.data_value;

			pts_advertise_data(BT_LE_ADV_OPT_CONNECTABLE);
			break;
		}
		case GAP_NON_CONNECTABLE:
		{
			s_pts_ctx.is_adv_enable = 1;
			hostif_bt_le_adv_stop();

			s_pts_ctx.adv_data.type = BT_DATA_FLAGS;
			s_pts_ctx.data_value[0] = BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR;
			s_pts_ctx.adv_data.data_len = 1;
			s_pts_ctx.adv_data.data = s_pts_ctx.data_value;

			pts_advertise_data(BT_LE_ADV_OPT_EXT_ADV);
			break;
		}
		case GAP_LE_BR_ADV:
		{
			s_pts_ctx.is_adv_enable = 1;
			hostif_bt_le_adv_stop();

			s_pts_ctx.adv_data.type = BT_DATA_FLAGS;
			s_pts_ctx.data_value[0] = BT_LE_AD_GENERAL;
			s_pts_ctx.adv_data.data_len = 1;
			s_pts_ctx.adv_data.data = s_pts_ctx.data_value;

			pts_advertise_data(BT_LE_ADV_OPT_CONNECTABLE);
			break;
		}

		default:
			break;
	}

	return 0;
}

static void btsrv_pts_notify_vcs_state(struct bt_conn *conn)
{
	int ret = btsrv_pts_vcs_notify_state(conn);
	SYS_LOG_INF("ret:%d\n", ret);
}

static void btsrv_pts_notify_vcs_flags(struct bt_conn *conn)
{
	int ret = btsrv_pts_vcs_notify_flags(conn);
	SYS_LOG_INF("ret:%d\n", ret);
}

static void btsrv_pts_init(btsrv_pts_callback cb)
{
	SYS_LOG_INF("pts cb:%p", cb);
	s_pts_ctx.p_cb = cb;
	btsrv_pts_start_adv(PTS_ADV_TYPE_CONNECTABLE);
}

static void btsrv_pts_deinit(void)
{
	s_pts_ctx.p_cb = NULL;
}

void btsrv_pts_set_le_audio_evt_flag(audio_core_evt_flags_e flag)
{
	s_pts_ctx.evt_flag |= flag;
}

void btsrv_pts_clear_le_audio_evt_flag(audio_core_evt_flags_e flag)
{
	s_pts_ctx.evt_flag &= ~flag;
}

void btsrv_pts_reset(void)
{
	s_pts_ctx.evt_flag = 0;
	s_pts_ctx.is_adv_enable = 0;
	s_pts_ctx.adv_ann_type = 0;
	s_pts_ctx.adv_data.type = BT_DATA_FLAGS;
	s_pts_ctx.data_value[0] = BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR;
	s_pts_ctx.adv_data.data_len = 1;
	s_pts_ctx.adv_data.data = s_pts_ctx.data_value;
}

int btsrv_pts_process(struct app_msg *msg)
{
	int prio;
	uint8_t cmd = 0;

	if (_btsrv_get_msg_param_type(msg) != MSG_BTSRV_PTS)
	{
		return -ENOTSUP;
	}

	prio = btsrv_set_negative_prio();
	cmd = _btsrv_get_msg_param_cmd(msg);
	SYS_LOG_INF("cmd:%d\n", cmd);

	switch (cmd)
	{
		case MSG_BTSRV_PTS_START:
			SYS_LOG_INF("MSG_BTSRV_PTS_START\n");
			btsrv_pts_init(msg->ptr);
			break;
		case MSG_BTSRV_PTS_STOP:
			SYS_LOG_INF("MSG_BTSRV_PTS_STOP\n");
			btsrv_pts_deinit();
			break;
		case MSG_BTSRV_PTS_AUTO_CONFIG:
		{
			if (s_pts_ctx.p_cb)
			{
				s_pts_ctx.p_cb(BTSRV_AUDIO_AUTO_CODEC_CONFIG, NULL, 0);
			}
			break;
		}
		case MSG_BTSRV_PTS_AUTO_QOS:
		{
			if (s_pts_ctx.p_cb)
			{
				s_pts_ctx.p_cb(BTSRV_AUDIO_AUTO_QOS_CONFIG, NULL, 0);
			}
			break;
		}
		case MSG_BTSRV_PTS_AUTO_ENABLE:
		{
			if (s_pts_ctx.p_cb)
			{
				s_pts_ctx.p_cb(BTSRV_AUDIO_AUTO_ENABLE_ASE, NULL, 0);
			}
			break;
		}
		case MSG_BTSRV_PTS_START_READY:
		{
			if (s_pts_ctx.p_cb)
			{
				s_pts_ctx.p_cb(BTSRV_AUDIO_AUTO_START_READY, NULL, 0);
			}
			break;
		}
		case MSG_BTSRV_PTS_NOTIFY_VCS_STATE:
		{
			SYS_LOG_INF("MSG_BTSRV_PTS_NOTIFY_VCS_STATE\n");
			btsrv_pts_notify_vcs_state(_btsrv_get_msg_param_ptr(msg));
			break;
		}
		case MSG_BTSRV_PTS_NOTIFY_VCS_FLAGS:
		{
			SYS_LOG_INF("MSG_BTSRV_PTS_NOTIFY_VCS_FLAGS\n");
			btsrv_pts_notify_vcs_flags(_btsrv_get_msg_param_ptr(msg));
			break;
		}

		default:
			break;
	}

	btsrv_revert_prio(prio);

	return 0;
}

#endif /*CONFIG_BT_LEA_PTS_TEST*/


/*
 * For pts test lib_service
 */
void btsrv_pts_set_broadcast_code(uint8_t *p_broadcast_code)
{
#ifdef CONFIG_BT_LEA_PTS_TEST
	if (p_broadcast_code)
	{
		memcpy(p_broadcast_code,pts_broadcast_code,sizeof(pts_broadcast_code));
	}
#endif /*CONFIG_BT_PTS_TEST*/
}

struct bt_data * btsrv_pts_set_adv_data(void)
{
#ifdef CONFIG_BT_LEA_PTS_TEST
	return &s_pts_ctx.adv_data;
#else
	return NULL;
#endif /*CONFIG_BT_LEA_PTS_TEST*/
}

uint8_t btsrv_pts_is_adv_enable(void)
{
#ifdef CONFIG_BT_LEA_PTS_TEST
	return s_pts_ctx.is_adv_enable;
#else
	return 0;
#endif /*CONFIG_BT_LEA_PTS_TEST*/
}

uint8_t btsrv_pts_is_le_audio_evt_flag(audio_core_evt_flags_e flag)
{
#ifdef CONFIG_BT_LEA_PTS_TEST
	return (s_pts_ctx.evt_flag & flag);
#else
	return 0;
#endif /*CONFIG_BT_LEA_PTS_TEST*/
}

static uint8_t s_pts_test_flags = 0;

void btsrv_pts_test_set_flag(uint8_t flags)
{
	s_pts_test_flags |= flags;
}

void btsrv_pts_test_clear_flag(uint8_t flags)
{
	s_pts_test_flags &= ~flags;
}

uint8_t btsrv_pts_test_is_flag(pts_test_flags_e flag)
{
#ifdef CONFIG_BT_PTS_TEST
	if (flag & s_pts_test_flags) {
		return 1;
	}
#endif
	return 0;
}


