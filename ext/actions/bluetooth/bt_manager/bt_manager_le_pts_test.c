/*
 * Copyright (c) 2023 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt manager LE pts test.
 */
#define SYS_LOG_NO_NEWLINE
#define SYS_LOG_DOMAIN "pts le"

#include <os_common_api.h>
#include <acts_bluetooth/bluetooth.h>
#include <acts_bluetooth/addr.h>
#include <acts_bluetooth/hci.h>

#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <msg_manager.h>
#include <mem_manager.h>
#include <bt_manager.h>
#include "bt_manager_inner.h"
#include <sys_event.h>
#include <btservice_api.h>
#include <shell/shell.h>
#include <acts_bluetooth/host_interface.h>
#include <property_manager.h>
#include "hex_str.h"
#include <acts_bluetooth/bluetooth.h>
#include <acts_bluetooth/audio/aics.h>
#include <acts_bluetooth/audio/vocs.h>
#include <acts_bluetooth/audio/mcs.h>
#include <acts_bluetooth/audio/tbs.h>
#include <acts_bluetooth/audio/bass.h>

#define PTS_TEST_LE_SHELL_MODULE    "pts_le"
#define PTS_MAC_ADDR_SIZE           (6)
#define PTS_LOCAL_NAME              "PTS-"
#define PTS_LOCAL_NAME_LEN          (sizeof(PTS_LOCAL_NAME) - 1)

typedef enum
{
	PTS_RESUME_CREATE_CONN = 0x01,
	PTS_RESUME_AICS_STATE = 0x02,
} pts_resume_flags_e;

typedef enum
{
	IDX_CODEC = 0,
	IDX_QOS = 1,
	IDX_NUM = 2,
} lc3_type_idx_e;

typedef struct
{
	uint8_t ase_id;
	uint16_t type;
} lc3_type_t;

typedef struct
{
	struct bt_conn *conn;
	bt_addr_le_t peer_addr;
	struct bt_conn_cb conn_cbs;
	struct bt_audio_chan chan[4];
	struct bt_audio_qoss_2 qoss;
	lc3_type_t lc3_type[IDX_NUM];
	pts_resume_flags_e resume_flags;
	struct bt_vocs vocs;
	struct bt_audio_call call;
	uint32_t remote_locations;
	uint32_t bis_handle;
    uint8_t bis_num;
} bt_manager_pts_ctx;

static bt_manager_pts_ctx s_ctx = {0};
static uint8_t str_traget_addr[5] = {0};

static void pts_set_resume_flag(pts_resume_flags_e flags)
{
	s_ctx.resume_flags |= flags;
}

static void pts_clear_reusme_flag(pts_resume_flags_e flags)
{
	s_ctx.resume_flags &= ~flags;
}

static uint8_t pts_need_resume(pts_resume_flags_e flag)
{
	if (flag & s_ctx.resume_flags) {
		return 1;
	}
	return 0;
}


static int pts_ble_create_acl(int argc, char *argv[]);
static int pts_ble_clear_ltk(int argc, char *argv[]);
static int pts_reset_ext_adv(int argc, char *argv[]);

static void pts_connected(struct bt_conn *conn, uint8_t err)
{
	char addr[13] = {0};
	struct bt_conn_info info = {0};
	bt_addr_le_t peer_addr = {0};
	peer_addr.type = 0;
	uint8_t type;
	if (0 != err)
	{
		type = hostif_bt_conn_get_type(conn);
		SYS_LOG_INF("pts %s conn fail: 0x%x \n", type == BT_CONN_TYPE_LE ? "LE" : "BR", err);
		if (type == BT_CONN_TYPE_LE) {
			hostif_bt_le_clear_device(&s_ctx.peer_addr);
			pts_ble_create_acl(0,NULL);
			return;
		} else {
			return;
		}
	}

	hostif_bt_conn_get_info(conn, &info);

	if (!s_ctx.conn)
	{
		memcpy(&s_ctx.peer_addr, info.le.dst, sizeof(s_ctx.peer_addr));
		s_ctx.conn = hostif_bt_conn_ref(conn);
		hex_to_str(addr, s_ctx.peer_addr.a.val, PTS_MAC_ADDR_SIZE);
		SYS_LOG_INF("hdl:0x%x,MAC: %s\n", bt_conn_get_handle(conn),addr);
	}
	else
	{
		SYS_LOG_ERR("Already connected\n");
	}
}

extern int bt_bass_remove_source(uint8_t src_id);
static void pts_disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[13] = {0};

	if (s_ctx.conn == conn)
	{
		memset(addr, 0, sizeof(addr));
		hex_to_str(addr, s_ctx.peer_addr.a.val, PTS_MAC_ADDR_SIZE);
		SYS_LOG_INF("MAC: %s, reason: 0x%x\n", addr, reason);

		if (pts_need_resume(PTS_RESUME_CREATE_CONN))
		{
			pts_ble_clear_ltk(0,NULL);
			pts_ble_create_acl(0,NULL);
		}
		else
		{
			hostif_bt_le_scan_stop();
			btif_pts_start_adv(PTS_ADV_TYPE_CONNECTABLE);
			pts_reset_ext_adv(0,NULL);
		}

		if (btif_pts_is_auto_evt(BTSRV_AUDIO_GENERATE_INCOMING_CALL)) {
			btif_audio_call_srv_terminate(&s_ctx.call, 0);
			btif_pts_clear_auto_audio_evt_flag(BTSRV_AUDIO_GENERATE_INCOMING_CALL);
		}

		s_ctx.conn = NULL;
		hostif_bt_conn_unref(conn);
		memset(&s_ctx.chan[0], 0, 2 * sizeof(struct bt_audio_chan));
		memset(&s_ctx.lc3_type[0], 0, IDX_NUM * sizeof(lc3_type_t));
		btif_pts_reset();
		bt_bass_remove_source(0);
		btif_pts_reset_bass_recv_state();

		if (pts_need_resume(PTS_RESUME_AICS_STATE))
		{
			btif_pts_aics_set_mute(BT_AICS_STATE_UNMUTED);
			btif_pts_aics_set_gain_mode(BT_AICS_MODE_MANUAL);
			pts_clear_reusme_flag(PTS_RESUME_AICS_STATE);
		}

		s_ctx.remote_locations = BT_AUDIO_LOCATIONS_FL;
	}
	else
	{
		SYS_LOG_ERR("Error conn %p(%p)\n", s_ctx.conn, conn);
	}
}

static int pts_ext_rx(uint32_t handle, const uint8_t *buf, uint16_t len)
{
	SYS_LOG_INF("rx pts ext,hdl: 0x%x,len:%d\n", handle, len);

	return 0;
}

static int pts_broadcast_sink_start(int argc, char *argv[])
{
	bt_manager_broadcast_sink_init();

	bt_manager_broadcast_scan_stop();
	uint8_t local_name[33] = {0};

	if (argc < 2)
	{
		SYS_LOG_INF("Used: pts_le pts_broadcast_sink_start xxx-xxxx(ps:PBP-E94B)\n");
		return -EINVAL;
	}

	uint32_t value = BT_AUDIO_LOCATIONS_FL | BT_AUDIO_LOCATIONS_FR;
	memcpy(local_name, PTS_LOCAL_NAME, PTS_LOCAL_NAME_LEN);
	memcpy(&local_name[PTS_LOCAL_NAME_LEN], argv[1], strlen(argv[1]));
	SYS_LOG_INF("BS loc name:%s, len:%d\n", local_name, strlen(local_name));

	bt_manager_broadcast_sink_sync_loacations(value);

	bt_manager_broadcast_filter_local_name(local_name);

	bt_manager_broadcast_sink_register_ext_rx_cb(pts_ext_rx);

	return bt_manager_broadcast_scan_start();

}

static int pts_broadcast_sink_stop(int argc, char *argv[])
{
	bt_manager_broadcast_scan_stop();
	bt_manager_broadcast_sink_exit();
	btif_pts_start_adv(PTS_ADV_TYPE_NONE);
	return 0;
}

#define DEFAULT_CONTEXTS	(BT_AUDIO_CONTEXT_UNSPECIFIED | \
				BT_AUDIO_CONTEXT_CONVERSATIONAL | \
				BT_AUDIO_CONTEXT_MEDIA)

static struct bt_broadcast_qos pts_source_qos = {
	.packing = BT_AUDIO_PACKING_INTERLEAVED,
	.framing = BT_AUDIO_UNFRAMED,
	.phy = BT_AUDIO_PHY_2M,
	.rtn = 2,
	.max_sdu = 100,
	/* max_transport_latency, unit: ms */
	.latency = 20,
	/* sdu_interval, unit: us */
	.interval = 10000,
	/* presentation_delay, unit: us */
	.delay = 40000,
	/* processing_time, unit: us */
	.processing = 9000,
};

static int pts_broadcast_source_start(int argc, char *argv[])
{
	uint8_t local_name[33] = {0};

	struct bt_broadcast_source_create_param *param = { 0 };
	struct bt_broadcast_source_big_param big_param;
	struct bt_le_adv_param adv_param = { 0 };
	struct bt_le_per_adv_param per_adv_param = { 0 };
	struct bt_broadcast_qos *qos = &pts_source_qos;

	struct bt_broadcast_subgroup_2 subgroup = { 0 };

	uint16_t appearance = 0x885;	/* Audio Source + Broadcasting Device */
	int ret;
	uint8_t bis_num = 1;
	uint16_t setting_type = 1621;
	if (argc != 3)
	{
		SYS_LOG_INF("Used: pts_le pts_broadcast_source_start x(bis_num) xxxx(cfg setting type)\n");
		return -EINVAL;
	}

	btif_audio_stop_adv();
	hostif_bt_le_adv_stop();
	hostif_bt_le_scan_stop();

	bis_num = strtoul(argv[1], NULL, 0);
	setting_type = strtoul(argv[2], NULL, 0);

	if (bis_num == 1) {
		subgroup.num_bis = 1;
		subgroup.locations = BT_AUDIO_LOCATIONS_FL;
	} else if (bis_num == 2) {
		subgroup.num_bis = 2;
		subgroup.bis[0].locations = BT_AUDIO_LOCATIONS_FL;
		subgroup.bis[1].locations = BT_AUDIO_LOCATIONS_FR;
	} else {
		SYS_LOG_ERR("bis_num:%d\n",bis_num);
		return -EINVAL;
	}

	switch (setting_type) {
		case 811:
		case 812:
		{
			subgroup.sample_rate = 8;
			subgroup.frame_duration = BT_FRAME_DURATION_7_5MS;
			subgroup.octets = 26;
			qos->interval = 7500;
			qos->max_sdu = 26;
			qos->rtn = 2;
			qos->latency = 8;
			if (setting_type == 812) {
				qos->rtn = 4;
				qos->latency = 45;
			}
			break;
		}
		case 1611:
		case 1612:
		{
			subgroup.sample_rate = 16;
			subgroup.frame_duration = BT_FRAME_DURATION_7_5MS;
			subgroup.octets = 30;
			qos->interval = 7500;
			qos->max_sdu = 30;
			qos->rtn = 2;
			qos->latency = 8;
			if (setting_type == 1612) {
				qos->rtn = 4;
				qos->latency = 45;
			}
			break;
		}
		case 1621:
		case 1622:
		{
			subgroup.sample_rate = 16;
			subgroup.frame_duration = BT_FRAME_DURATION_10MS;
			subgroup.octets = 40;
			qos->interval = 10000;
			qos->max_sdu = 40;
			qos->rtn = 2;
			qos->latency = 10;
			if (setting_type == 1622) {
				qos->rtn = 4;
				qos->latency = 60;
			}
			break;
		}
		case 2411:
		case 2412:
		{
			subgroup.sample_rate = 24;
			subgroup.frame_duration = BT_FRAME_DURATION_7_5MS;
			subgroup.octets = 45;
			qos->interval = 7500;
			qos->max_sdu = 45;
			qos->rtn = 2;
			qos->latency = 8;
			if (setting_type == 2412) {
				qos->rtn = 4;
				qos->latency = 45;
			}
			break;
		}
		case 2421:
		case 2422:
		{
			subgroup.sample_rate = 24;
			subgroup.frame_duration = BT_FRAME_DURATION_10MS;
			subgroup.octets = 60;
			qos->interval = 10000;
			qos->max_sdu = 60;
			qos->rtn = 2;
			qos->latency = 10;
			if (setting_type == 2422) {
				qos->rtn = 4;
				qos->latency = 60;
			}
			break;
		}
		case 4811:
		case 4831:
		case 4851:
		{
			subgroup.sample_rate = 48;
			subgroup.frame_duration = BT_FRAME_DURATION_7_5MS;
			subgroup.octets = 75;
			qos->interval = 7500;
			qos->max_sdu = 75;
			qos->rtn = 4;
			qos->latency = 15;
			if (setting_type == 4831) {
				subgroup.octets = 90;
				qos->max_sdu = 90;
			} else if (setting_type == 4851) {
				subgroup.octets = 117;
				qos->max_sdu = 117;
			}
			break;
		}

		case 4821:
		case 4841:
		case 4861:
		{
			subgroup.sample_rate = 48;
			subgroup.frame_duration = BT_FRAME_DURATION_10MS;
			subgroup.octets = 100;
			qos->interval = 10000;
			qos->max_sdu = 100;
			qos->rtn = 4;
			qos->latency = 20;
			if (setting_type == 4841) {
				subgroup.octets = 120;
				qos->max_sdu = 120;
			} else if (setting_type == 4861) {
				subgroup.octets = 155;
				qos->max_sdu = 155;
			}

			break;
		}

		case 4812:
		case 4832:
		case 4852:
		{
			subgroup.sample_rate = 48;
			subgroup.frame_duration = BT_FRAME_DURATION_7_5MS;
			subgroup.octets = 75;
			qos->interval = 7500;
			qos->max_sdu = 75;
			qos->rtn = 4;
			qos->latency = 50;
			if (setting_type == 4832) {
				subgroup.octets = 90;
				qos->max_sdu = 90;
			} else if (setting_type == 4852) {
				subgroup.octets = 117;
				qos->max_sdu = 117;
			}
			break;
		}

		case 4822:
		case 4842:
		case 4862:
		{
			subgroup.sample_rate = 48;
			subgroup.frame_duration = BT_FRAME_DURATION_10MS;
			subgroup.octets = 100;
			qos->interval = 10000;
			qos->max_sdu = 100;
			qos->rtn = 4;
			qos->latency = 65;
			if (setting_type == 4842) {
				subgroup.octets = 120;
				qos->max_sdu = 120;
			} else if (setting_type == 4862) {
				subgroup.octets = 155;
				qos->max_sdu = 155;
			}

			break;
		}

		default:
			break;
	}

	qos->delay = 40000;

	subgroup.format = BT_AUDIO_CODEC_LC3;
	subgroup.blocks = 1;
	subgroup.language = BT_LANGUAGE_UNKNOWN;
	subgroup.contexts = DEFAULT_CONTEXTS;

	if (qos->interval == BT_FRAME_DURATION_7_5MS) {
		big_param.iso_interval = 6;
	} else {
		big_param.iso_interval = 8;
	}

	big_param.max_pdu = qos->max_sdu;
	big_param.nse = 4;
	big_param.bn = 1;
	big_param.irc = 4;
	big_param.pto = 0;

	/* 90ms or 100ms */
	per_adv_param.interval_min = 80;//100ms
	per_adv_param.interval_max = 80;//100ms

	/* BT_LE_EXT_ADV_NCONN */
	adv_param.id = BT_ID_PUBLIC;
	adv_param.interval_min = 256;//160ms
	adv_param.interval_max = 256;//160ms
	adv_param.options = BT_LE_ADV_OPT_EXT_ADV;
	adv_param.options |= BT_LE_ADV_OPT_USE_IDENTITY;
	adv_param.sid = BT_EXT_ADV_SID_BROADCAST;

	param = mem_malloc(sizeof(*param));
	if (param == NULL) {
		SYS_LOG_ERR("alloc failed\n");
		return -EINVAL;
	}

	param->broadcast_id = 0;
	memcpy(local_name, "Broadcaster",11);
	param->local_name = local_name;
	param->broadcast_name = local_name;
	param->qos = qos;
	param->appearance = appearance;
	param->num_subgroups = 1;
	param->subgroup = (struct bt_broadcast_subgroup *)&subgroup;
	param->big_param = &big_param;
	param->per_adv_param = &per_adv_param;
	param->adv_param = &adv_param;

    s_ctx.bis_handle = 0;

	ret = btif_broadcast_source_create(param);
	if (ret < 0) {
		SYS_LOG_ERR("failed");
		goto exit;
	}
	s_ctx.bis_num = bis_num;
    s_ctx.bis_handle = ret;
exit:
	mem_free(param);
	return ret;
}


static int pts_broadcast_source_enable(int argc, char *argv[])
{
	btif_broadcast_source_enable(s_ctx.bis_handle);
	return 0;
}

static int pts_broadcast_source_reconfig(int argc, char *argv[])
{
	struct bt_broadcast_qos *qos = &pts_source_qos;

	struct bt_broadcast_subgroup_2 subgroup = { 0 };
	if (s_ctx.bis_num == 1) {
		subgroup.num_bis = 1;
		subgroup.locations = BT_AUDIO_LOCATIONS_FL;
	} else if (s_ctx.bis_num == 2) {
		subgroup.num_bis = 2;
		subgroup.bis[0].locations = BT_AUDIO_LOCATIONS_FL;
		subgroup.bis[1].locations = BT_AUDIO_LOCATIONS_FR;
	}
	subgroup.sample_rate = 48;
	subgroup.frame_duration = BT_FRAME_DURATION_10MS;
	subgroup.octets = 100;
	subgroup.format = BT_AUDIO_CODEC_LC3;
	subgroup.blocks = 1;
	subgroup.language = BT_LANGUAGE_UNKNOWN;
	subgroup.contexts = BT_AUDIO_CONTEXT_UNSPECIFIED|BT_AUDIO_CONTEXT_MEDIA;

	btif_broadcast_source_reconfig(s_ctx.bis_handle, qos, 1, (struct bt_broadcast_subgroup *)&subgroup);
	return 0;
}

static int pts_broadcast_source_stop(int argc, char *argv[])
{
	bt_manager_broadcast_source_disable(s_ctx.bis_handle);
	bt_manager_broadcast_source_release(s_ctx.bis_handle);
	btif_pts_start_adv(PTS_ADV_TYPE_CONNECTABLE);
	pts_reset_ext_adv(0,NULL);

	return 0;
}

static int pts_tmap_discovery(int argc, char *argv[])
{
	return btif_pts_start_adv(TMAP_DISCOVERY);
}

static int pts_bass_discovery(int argc, char *argv[])
{
	return btif_pts_start_adv(BASS_DISCOVERY);
}

static int pts_bap_discovery(int argc, char *argv[])
{
	return btif_pts_start_adv(BAP_DISCOVERY);
}

static int pts_cap_discovery(int argc, char *argv[])
{
	if (argc == 2 && (1 == strtoul(argv[1], NULL, 0)))
	{
		btif_pts_set_adv_ann_type();
	}
	return btif_pts_start_adv(CAP_DISCOVERY);
}

static int pts_csip_discovery(int argc, char *argv[])
{
	return btif_pts_start_adv(CSIP_DISCOVERY);
}

static int pts_gap_le_br_adv(int argc, char *argv[])
{
	btif_pts_start_adv(GAP_LE_BR_ADV);
	hostif_bt_le_adv_stop();
	bt_br_set_connectable(1);
	bt_br_set_discoverable(1);
	return 0;
}

static int pts_reset_ext_adv(int argc, char *argv[])
{
	bt_manager_lea_policy_disable(0);
	struct bt_le_adv_param adv_param = {0};
	adv_param.id = BT_ID_PUBLIC;
	adv_param.interval_min = BT_GAP_ADV_FAST_INT_MIN_2;
	adv_param.interval_max = BT_GAP_ADV_FAST_INT_MAX_2;
	adv_param.options = BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_USE_IDENTITY;
#if CONFIG_BT_EXT_ADV
	adv_param.options |= BT_LE_ADV_OPT_EXT_ADV | BT_LE_ADV_OPT_NO_2M;
	adv_param.sid = BT_EXT_ADV_SID_UNICAST;
#endif

	int ret = btif_audio_update_adv_param(&adv_param);
	if (ret) {
		SYS_LOG_ERR("update:%d\n",ret);
	}

	return bt_manager_audio_le_start_adv();
}

static uint8_t pts_name_match(uint8_t len, uint8_t ad_type, uint8_t *data)
{
	uint8_t str_addr_cmp[4] = {0};
	if (!memcmp(data, "PTS-", 4))
	{
		SYS_LOG_INF("pts dev,name:%s\n", data);
		if (!memcmp(str_addr_cmp, str_traget_addr, 4))
		{
			/* no target addr input, just connect to pts device. */
			return 1;
		}
		/* check for target pts device. */
		for (uint8_t idx = 4; idx < len; idx++)
		{
			if (('-' == data[idx]) && (!memcmp(&data[idx + 1], str_traget_addr, 4)))
			{
				return 1;
			}
		}
		SYS_LOG_ERR("not match target dev:%s\n", str_traget_addr);
		return 0;
	}
	else
	{
		//SYS_LOG_INF("other dev,name:%s\n", data);
		return 0;
	}
}

static uint8_t pts_adv_parse
			(
				uint8_t dest_type,
				struct net_buf_simple *ad,
				uint8_t (*func)(uint8_t len, uint8_t ad_type, uint8_t *data)
			)
{
	if (ad == NULL)
	{
		SYS_LOG_ERR("ad NULL\n");
		return 0;
	}

	uint8_t *p_data = ad->data;
	uint8_t ad_len = 0;
	uint8_t ad_type = 0;
	uint8_t offset = 0;
	uint16_t ad_num = ad->len;
	while(ad_num > 0)
	{
		ad_len = p_data[offset];
		offset += 1;
		ad_type = p_data[offset];
		offset += 1;
		if (ad_type == dest_type && func)
		{
			return func(ad_len, ad_type, &p_data[offset]);
		}

		offset += ad_len - 1;
		ad_num--;
		if (offset >= ad->size)
		{
			return 0;
		}
	}
	return 0;
}

static void pts_create_acl_cb
			(
				const bt_addr_le_t *addr,
				int8_t rssi,
				uint8_t type,
				struct net_buf_simple *ad
			)
{
	struct bt_conn_le_create_param *create_param;
	struct bt_le_conn_param *conn_param;
	struct bt_conn *conn = s_ctx.conn;
	int err = 1;
	struct bt_conn_le_create_param le_create_param = {0};
	struct bt_le_conn_param le_conn_param = {0};

	switch (type)
	{
		case BT_GAP_ADV_TYPE_ADV_IND:
		case BT_GAP_ADV_TYPE_SCAN_RSP:
		case BT_GAP_ADV_TYPE_EXT_ADV:
		case BT_GAP_ADV_TYPE_ADV_DIRECT_IND:
		{
			if (pts_adv_parse(BT_DATA_NAME_COMPLETE, ad, pts_name_match))
			{
				err = 0;
				memcpy(&s_ctx.peer_addr, addr, sizeof(s_ctx.peer_addr));
				SYS_LOG_INF("addr:%02x %02x %02x %02x %02x %02x\n",
					addr->a.val[0],addr->a.val[1],addr->a.val[2],
					addr->a.val[3],addr->a.val[4],addr->a.val[5]);
			}
			break;
		}


		default:
			break;
	}

	if (err)
	{
		return;
	}

	err = hostif_bt_le_scan_stop();
	if (err)
	{
		SYS_LOG_ERR("err:%d\n",err);
		return;
	}

	create_param = &le_create_param;
	create_param->options = BT_CONN_LE_OPT_NONE;
	create_param->interval = BT_GAP_SCAN_FAST_INTERVAL;
	create_param->window = BT_GAP_SCAN_FAST_WINDOW;
	create_param->interval_coded = 0;
	create_param->window_coded = 0;
	create_param->timeout = 0;

	conn_param = &le_conn_param;
	conn_param->interval_min = 8;
	conn_param->interval_max = 8;
	conn_param->latency = 0;
	conn_param->timeout = 1000;	/* 10s */

	err = btif_pts_dev_connect(&s_ctx.peer_addr, conn, (void *)create_param, (void *)conn_param);
	if (err)
	{
		SYS_LOG_ERR("Create conn failed (%d)", err);
		pts_ble_create_acl(0, NULL);
	}
	else
	{
		SYS_LOG_INF("le acl create conn 0x%x\n",(uint32_t)s_ctx.conn);
	}
}

static int pts_ble_create_acl(int argc, char *argv[])
{
	struct bt_le_scan_param param = {0};
	param.type = BT_LE_SCAN_TYPE_PASSIVE;
	param.options = BT_LE_SCAN_OPT_FILTER_DUPLICATE;
	param.interval = BT_GAP_SCAN_FAST_INTERVAL,
	param.window = BT_GAP_SCAN_FAST_WINDOW,
	param.timeout = 0;

	if (argc == 2)
	{
		memcpy(str_traget_addr, argv[1], 4);
		SYS_LOG_INF("str addr(the last 2 bytes) input:%s\n", str_traget_addr);
	}

	btif_audio_stop_adv();
	hostif_bt_le_adv_stop();
	hostif_bt_le_scan_stop();
	pts_set_resume_flag(PTS_RESUME_CREATE_CONN);

	int err = hostif_bt_le_scan_start(&param, pts_create_acl_cb);
	if (err) {
		SYS_LOG_ERR("failed %d\n", err);
	}
	return err;
}

static int pts_ble_wait_connect(int argc, char *argv[])
{
	btif_audio_stop_adv();
	hostif_bt_le_adv_stop();
	hostif_bt_le_scan_stop();

	int ret = btif_pts_start_adv(PTS_ADV_TYPE_CONNECTABLE);
	if (ret) {
		SYS_LOG_ERR("%d\n",ret);
	}
	pts_reset_ext_adv(0,NULL);

	return 0;
}

static int pts_ble_create_acl_cancel(int argc, char *argv[])
{
	pts_clear_reusme_flag(PTS_RESUME_CREATE_CONN);
	memset(str_traget_addr, 0, 5);

	if (btif_pts_test_is_flag(PTS_TEST_FLAG_CSIP)) {
		btif_pts_test_clear_flag(PTS_TEST_FLAG_CSIP);
	}

	hostif_bt_le_scan_stop();

	int ret = btif_pts_start_adv(PTS_ADV_TYPE_CONNECTABLE);
	if (ret) {
		SYS_LOG_ERR("%d\n",ret);
	}
	pts_reset_ext_adv(0,NULL);

	return 0;
}

static int pts_ble_acl_disconnect(int argc, char *argv[])
{
	int err;

	err = hostif_bt_conn_disconnect(s_ctx.conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	if (err) {
		SYS_LOG_INF("Disconnection failed (err %d)\n", err);
	}

	return err;
}

static int pts_ble_clear_ltk(int argc, char *argv[])
{
	hostif_bt_le_clear_device(&s_ctx.peer_addr);
	return 0;
}

static int pts_ble_set_sc_level(int argc, char *argv[])
{
	uint8_t level = BT_SECURITY_L2;
	if (argc > 1) {
		level = strtoul(argv[1], NULL, 0);
		if (level > BT_SECURITY_L4) {
			SYS_LOG_ERR("level:%d\n", level);
			return 0;
		}
	}
	btif_pts_set_le_security_level(level);

	return 0;
}

static int pts_cfg_remote_locations(int argc, char *argv[])
{
	uint8_t loc_input = 1;
	if (argc > 1) {
		loc_input = strtoul(argv[1], NULL, 16);
	}
	if (0 == loc_input) {
		s_ctx.remote_locations = BT_AUDIO_LOCATIONS_NA;
	} else if (1 == loc_input) {
		s_ctx.remote_locations = BT_AUDIO_LOCATIONS_FL;
	} else if (2 == loc_input) {
		s_ctx.remote_locations = BT_AUDIO_LOCATIONS_FR;
	} else if (3 == loc_input) {
		s_ctx.remote_locations = BT_AUDIO_LOCATIONS_FL|BT_AUDIO_LOCATIONS_FR;
	} else if (0xff == loc_input) {
		s_ctx.remote_locations = 0xFFFFFFFF; /*not present*/
	}
	return 0;
}

static int pts_codec_config(int argc, char *argv[])
{
	struct bt_audio_preferred_qos qos = {0};
	struct bt_audio_codec codec = {0};
	uint32_t type = 0;
	uint8_t ase_id = 0;
	int ret = 0;

	if (btif_pts_is_auto_evt(BTSRV_AUDIO_ASCS_AUTO_CODEC_CONFIG))
	{
		type = s_ctx.lc3_type[IDX_CODEC].type;
		ase_id = s_ctx.lc3_type[IDX_CODEC].ase_id;
	}
	else
	{
		if (argc != 3)
		{
			SYS_LOG_INF("Used: pts_le codec_config x(ASE_ID) xxx(codec type)\n");
			return -EINVAL;
		}
		ase_id = strtoul(argv[1], NULL, 0);
		type = strtoul(argv[2], NULL, 0);
	}
	SYS_LOG_INF("id:%d,type:%d", ase_id, type);

	codec.handle = bt_conn_get_handle(s_ctx.conn);
	codec.id = ase_id;
	if (ase_id == 1 || ase_id == 2)
	{
		codec.dir = BT_AUDIO_DIR_SINK;
	}
	else
	{
		codec.dir = BT_AUDIO_DIR_SOURCE;
	}
	codec.format = BT_AUDIO_CODEC_LC3;
	codec.target_latency = BT_AUDIO_TARGET_LATENCY_LOW;
	codec.target_phy = BT_AUDIO_TARGET_PHY_2M;
	codec.blocks = 1;
	codec.locations = s_ctx.remote_locations;

	switch (type) {
	case 81:
		codec.sample_rate = 8;
		codec.octets = 26;
		codec.frame_duration = BT_FRAME_DURATION_7_5MS;
		qos.latency = 8;
		break;
	case 82:
		codec.sample_rate = 8;
		codec.octets = 30;
		codec.frame_duration = BT_FRAME_DURATION_10MS;
		qos.latency = 10;
		break;
	case 161:
		codec.sample_rate = 16;
		codec.octets = 30;
		codec.frame_duration = BT_FRAME_DURATION_7_5MS;
		qos.latency = 8;
		break;
	case 162:
		codec.sample_rate = 16;
		codec.octets = 40;
		codec.frame_duration = BT_FRAME_DURATION_10MS;
		qos.latency = 10;
		break;
	case 241:
		codec.sample_rate = 24;
		codec.octets = 45;
		codec.frame_duration = BT_FRAME_DURATION_7_5MS;
		qos.latency = 8;
		break;
	case 242:
		codec.sample_rate = 24;
		codec.octets = 60;
		codec.frame_duration = BT_FRAME_DURATION_10MS;
		qos.latency = 10;
		break;
	case 321:
		codec.sample_rate = 32;
		codec.octets = 60;
		codec.frame_duration = BT_FRAME_DURATION_7_5MS;
		qos.latency = 8;
		break;
	case 322:
		codec.sample_rate = 32;
		codec.octets = 80;
		codec.frame_duration = BT_FRAME_DURATION_10MS;
		qos.latency = 10;
		break;
	case 481:
		codec.sample_rate = 48;
		codec.octets = 75;
		codec.frame_duration = BT_FRAME_DURATION_7_5MS;
		qos.latency = 8;
		break;
	case 482:
		codec.sample_rate = 48;
		codec.octets = 100;
		codec.frame_duration = BT_FRAME_DURATION_10MS;
		qos.latency = 10;
		break;
	case 483:
		codec.sample_rate = 48;
		codec.octets = 90;
		codec.frame_duration = BT_FRAME_DURATION_7_5MS;
		qos.latency = 8;
		break;
	case 484:
		codec.sample_rate = 48;
		codec.octets = 120;
		codec.frame_duration = BT_FRAME_DURATION_10MS;
		qos.latency = 10;
		break;
	case 485:
		codec.sample_rate = 48;
		codec.octets = 117;
		codec.frame_duration = BT_FRAME_DURATION_7_5MS;
		qos.latency = 8;
		break;
	case 486:
		codec.sample_rate = 48;
		codec.octets = 155;
		codec.frame_duration = BT_FRAME_DURATION_10MS;
		qos.latency = 10;
		break;
	default:
		return -EINVAL;
	}

	ret = btif_audio_stream_config_codec(&codec);
	SYS_LOG_INF("codec config, ret:%d\n", ret);
	if (ret)
	{
		return ret;
	}

	qos.handle = bt_conn_get_handle(s_ctx.conn);
	qos.id = ase_id;
	qos.framing = BT_AUDIO_UNFRAMED_SUPPORTED;
	qos.phy = BT_AUDIO_PHY_2M;
	qos.rtn = 3;
	qos.delay_min = 35000;
	qos.delay_max = 40000;
	qos.preferred_delay_min = 35000;
	qos.preferred_delay_max = 40000;

	ret = bt_manager_audio_stream_prefer_qos(&qos);
	SYS_LOG_INF("codec qos, ret:%d\n", ret);
	return ret;
}

static int pts_qos_config(int argc, char *argv[])
{
	struct bt_audio_qoss_2 *qoss = &s_ctx.qoss;
	struct bt_audio_qos *qos = &qoss->qos[0];
	uint32_t type;
	uint8_t ase_id;
	if (btif_pts_is_auto_evt(BTSRV_AUDIO_ASCS_AUTO_QOS_CONFIG))
	{
		type = s_ctx.lc3_type[IDX_QOS].type;
		ase_id = s_ctx.lc3_type[IDX_QOS].ase_id;
	}
	else
	{
		if (argc != 3)
		{
			SYS_LOG_INF("Used: pts_le qos_config x(ASE ID) xxx(QoS type)\n");
			return -EINVAL;
		}
		ase_id = strtoul(argv[1], NULL, 0);
		type = strtoul(argv[2], NULL, 0);
	}
	SYS_LOG_INF("ase_id: %d, type: %d\n", ase_id, type);

	memset(qoss, 0, sizeof(*qoss));
	if (ase_id == 1 || ase_id == 2)
	{
		qos->id_m_to_s = ase_id;
	}
	else
	{
		qos->id_s_to_m = ase_id;
	}

	qoss->framing = BT_AUDIO_UNFRAMED;
	qoss->num_of_qos = 1;
	qoss->sca = BT_GAP_SCA_UNKNOWN;
	qoss->packing = BT_AUDIO_PACKING_SEQUENTIAL;
	qoss->interval_m = 10000;
	qoss->interval_s = 10000;
	qoss->delay_m = 40000;
	qoss->delay_s = 40000;
	qoss->processing_m = 4000;
	qoss->processing_s = 4000;

	qos->handle = bt_conn_get_handle(s_ctx.conn);
	qos->phy_m = BT_AUDIO_PHY_2M;
	qos->phy_s = BT_AUDIO_PHY_2M;

	switch (type) {
	case 1621:
		qos->max_sdu_m = 40;
		qos->rtn_m = 2;
		qoss->latency_m = 10;
		qos->max_sdu_s = 40;
		qos->rtn_s = 2;
		qoss->latency_s = 10;
		break;
	case 1622:
		qos->max_sdu_m = 40;
		qos->rtn_m = 13;
		qoss->latency_m = 95;
		qos->max_sdu_s = 40;
		qos->rtn_s = 13;
		qoss->latency_s = 95;
		break;
	case 2421:
		qos->max_sdu_m = 60;
		qos->rtn_m = 2;
		qoss->latency_m = 10;
		qos->max_sdu_s = 60;
		qos->rtn_s = 2;
		qoss->latency_s = 10;
		break;
	case 2422:
		qos->max_sdu_m = 60;
		qos->rtn_m = 13;
		qoss->latency_m = 95;
		qos->max_sdu_s = 60;
		qos->rtn_s = 13;
		qoss->latency_s = 95;
		break;
	case 3221:
		qos->max_sdu_m = 80;
		qos->rtn_m = 2;
		qoss->latency_m = 10;
		qos->max_sdu_s = 80;
		qos->rtn_s = 2;
		qoss->latency_s = 10;
		break;
	case 4821:
		qos->max_sdu_m = 100;
		qos->rtn_m = 5;
		qoss->latency_m = 20;
		qos->max_sdu_s = 100;
		qos->rtn_s = 5;
		qoss->latency_s = 20;
		break;
	case 4822:
		qos->max_sdu_m = 100;
		qos->rtn_m = 13;
		qoss->latency_m = 95;
		qos->max_sdu_s = 100;
		qos->rtn_s = 13;
		qoss->latency_s = 95;
		break;
	case 4841:
		qos->max_sdu_m = 120;
		qos->rtn_m = 5;
		qoss->latency_m = 20;
		qos->max_sdu_s = 120;
		qos->rtn_s = 5;
		qoss->latency_s = 20;
		break;
	case 4842:
		qos->max_sdu_m = 120;
		qos->rtn_m = 13;
		qoss->latency_m = 100;
		qos->max_sdu_s = 120;
		qos->rtn_s = 13;
		qoss->latency_s = 100;
		break;
	case 4861:
		qos->max_sdu_m = 155;
		qos->rtn_m = 5;
		qoss->latency_m = 20;
		qos->max_sdu_s = 155;
		qos->rtn_s = 5;
		qoss->latency_s = 20;
		break;
	case 4862:
		qos->max_sdu_m = 155;
		qos->rtn_m = 13;
		qoss->latency_m = 100;
		qos->max_sdu_s = 155;
		qos->rtn_s = 13;
		qoss->latency_s = 100;
		break;

	default:
		return -EINVAL;
	}

	return bt_manager_le_audio_stream_config_qos((struct bt_audio_qoss *)qoss);
}

static int pts_qos2_config(int argc, char *argv[])
{
	struct bt_audio_qoss_2 *qoss = &s_ctx.qoss;
	struct bt_audio_qos *qos = &qoss->qos[0];
	struct bt_audio_qos *qos2 = &qoss->qos[1];
	uint8_t id;	/* sink */
	uint8_t id2;	/* source */
	uint8_t qos_num;

	if (argc < 4) {
		SYS_LOG_INF("Used: pts_le qos2_config x(id1) x(id2) x(QoS num)\n");
		return -EINVAL;
	}

	id = strtoul(argv[1], NULL, 0);
	id2 = strtoul(argv[2], NULL, 0);
	qos_num = strtoul(argv[3], NULL, 0);
	SYS_LOG_INF("id1: %d, id2: %d, qos num: %d\n", id, id2, qos_num);

	memset(qoss, 0, sizeof(*qoss));

	qos->handle = bt_conn_get_handle(s_ctx.conn);

	if (id <= 2)//config sink ase qos
	{
		qos->id_m_to_s = id;
		qos->phy_m = BT_AUDIO_PHY_2M;
		qos->max_sdu_m = 40;
		qos->rtn_m = 2;
	}
	else//config source ase qos
	{
		qos->id_s_to_m = id;
		qos->phy_s = BT_AUDIO_PHY_2M;
		qos->max_sdu_s = 40;
		qos->rtn_s = 2;
	}

	if (qos_num == 2)
	{
		qoss->num_of_qos = 2;
		qos2->handle = bt_conn_get_handle(s_ctx.conn);
		if (id2 <= 2)//config sink ase qos
		{
			qos2->id_m_to_s = id2;
			qos2->phy_m = BT_AUDIO_PHY_2M;
			qos2->max_sdu_m = 40;
			qos2->rtn_m = 2;
		}
		else//config source ase qos
		{
			qos2->id_s_to_m = id2;
			qos2->phy_s = BT_AUDIO_PHY_2M;
			qos2->max_sdu_s = 40;
			qos2->rtn_s = 2;
		}
	}
	else
	{
		qoss->num_of_qos = 1;
		qos->id_s_to_m = id2;
		qos->phy_s = BT_AUDIO_PHY_2M;
		qos->max_sdu_s = 40;
		qos->rtn_s = 2;
	}

	qoss->framing = BT_AUDIO_UNFRAMED;
	qoss->sca = BT_GAP_SCA_UNKNOWN;
	qoss->packing = BT_AUDIO_PACKING_SEQUENTIAL;
	qoss->interval_m = 10000;
	qoss->interval_s = 10000;
	qoss->delay_m = 40000;
	qoss->delay_s = 40000;
	qoss->processing_m = 4000;
	qoss->processing_s = 4000;

	qoss->latency_m = 10;
	qoss->latency_s = 10;

	if (argc > 4) {
		uint32_t type = strtoul(argv[4], NULL, 0);
		if (type == 4821) {
			qos->max_sdu_m = 100;
			qos->rtn_m = 5;
			qoss->latency_m = 20;
			qos->max_sdu_s = 100;
			qos->rtn_s = 5;
			qoss->latency_s = 20;

			qos2->max_sdu_m = 100;
			qos2->rtn_m = 5;
			qos2->max_sdu_s = 100;
			qos2->rtn_s = 5;
		}
	}

	return bt_manager_le_audio_stream_config_qos((struct bt_audio_qoss *)qoss);
}

static int pts_qos_config_ases(int argc, char *argv[])
{
	struct bt_audio_qoss_2 *qoss = &s_ctx.qoss;
	struct bt_audio_qos *qos = &qoss->qos[0];
	struct bt_audio_qos *qos2 = &qoss->qos[1];
	uint8_t id;	    /* sink */
	uint8_t id2;	/* sink */
	uint8_t id3;	/* source */
	uint8_t id4;	/* source */
	uint8_t qos_num;

	id = 1;
	id2 = 2;
	id3 = 10;
	id4 = 11;
	qos_num = 2;

	memset(qoss, 0, sizeof(*qoss));

	qos->handle = bt_conn_get_handle(s_ctx.conn);
	qos->id_m_to_s = id;
	qos->phy_m = BT_AUDIO_PHY_2M;
	qos->max_sdu_m = 40;
	qos->rtn_m = 2;

	if (argc > 1 && 4 == strtoul(argv[1], NULL, 0)) {
		qos->id_s_to_m = id4;
		qos->phy_s = BT_AUDIO_PHY_2M;
		qos->max_sdu_s = 40;
		qos->rtn_s = 2;
	}

	qoss->num_of_qos = 2;

	qos2->handle = bt_conn_get_handle(s_ctx.conn);
	qos2->id_m_to_s = id2;
	qos2->phy_m = BT_AUDIO_PHY_2M;
	qos2->max_sdu_m = 40;
	qos2->rtn_m = 2;
	qos2->id_s_to_m = id3;
	qos2->phy_s = BT_AUDIO_PHY_2M;
	qos2->max_sdu_s = 40;
	qos2->rtn_s = 2;

	qoss->framing = BT_AUDIO_UNFRAMED;
	qoss->sca = BT_GAP_SCA_UNKNOWN;
	qoss->packing = BT_AUDIO_PACKING_SEQUENTIAL;
	qoss->interval_m = 10000;
	qoss->interval_s = 10000;
	qoss->delay_m = 40000;
	qoss->delay_s = 40000;
	qoss->processing_m = 4000;
	qoss->processing_s = 4000;

	qoss->latency_m = 10;
	qoss->latency_s = 10;

	return bt_manager_le_audio_stream_config_qos((struct bt_audio_qoss *)qoss);
}


static int pts_enable_ase(int argc, char *argv[])
{
	uint8_t id;
	if (btif_pts_is_auto_evt(BTSRV_AUDIO_ASCS_AUTO_ENABLE_ASE))
	{
		id = s_ctx.lc3_type[IDX_QOS].ase_id;
	}
	else
	{
		if (argc != 2)
		{
			SYS_LOG_INF("Used: pts_le enable_ase x(ASE ID)\n");
			return -EINVAL;
		}
		id = strtoul(argv[1], NULL, 0);
	}

	SYS_LOG_INF("id: %d", id);

	s_ctx.chan[0].handle = bt_conn_get_handle(s_ctx.conn);
	s_ctx.chan[0].id = id;
	struct bt_audio_chan *chans = &s_ctx.chan[0];
	uint8_t is_sink = (id == 1)? 1 : 0;
	uint16_t audio_context = BT_AUDIO_CONTEXT_MEDIA;
	if (btif_pts_is_auto_evt(BTSRV_AUDIO_CAS_AUTO_DISCOVER))
	{
		audio_context = btif_pts_get_pacs_aac(s_ctx.conn, is_sink);
	}
	return bt_manager_le_audio_stream_enable(&chans, 1, audio_context);
}

static int pts_enable_ase2(int argc, char *argv[])
{
	struct bt_audio_chan *chans[2];
	uint8_t id;
	uint8_t id2;

	if (argc < 3)
	{
		SYS_LOG_INF("Used: pts_le enable_ase2 x(id1) x(id2)\n");
		return -EINVAL;
	}

	id = strtoul(argv[1], NULL, 0);
	id2 = strtoul(argv[2], NULL, 0);
	SYS_LOG_INF("id1: %d, id2: %d\n", id, id2);

	s_ctx.chan[0].handle = bt_conn_get_handle(s_ctx.conn);
	s_ctx.chan[0].id = id;
	s_ctx.chan[1].handle = bt_conn_get_handle(s_ctx.conn);
	s_ctx.chan[1].id = id2;
	chans[0] = &s_ctx.chan[0];
	chans[1] = &s_ctx.chan[1];

	uint8_t is_sink = (id <= 2)? 1 : 0;
	uint16_t audio_context = BT_AUDIO_CONTEXT_MEDIA;
	if (btif_pts_is_auto_evt(BTSRV_AUDIO_CAS_AUTO_DISCOVER))
	{
		if (argc != 4)
		{
			SYS_LOG_INF("Used: pts_le enable_ase2 x(id1) x(id2) x(CCID count)\n");
			return -EINVAL;
		}

		btif_pts_set_ascs_ccid_count(strtoul(argv[3], NULL, 0));
		audio_context = btif_pts_get_pacs_aac(s_ctx.conn, is_sink);
	}

	return bt_manager_le_audio_stream_enable(chans, 2, audio_context);
}

static int pts_enable_ases(int argc, char *argv[])
{
	struct bt_audio_chan *chans[4];
	uint8_t id;
	uint8_t id2;
	uint8_t id3;
	uint8_t id4;
	uint8_t ase_num = 3;

	id = 1;
	id2 = 2;
	id3 = 10;
	id4 = 11;

	s_ctx.chan[0].handle = bt_conn_get_handle(s_ctx.conn);
	s_ctx.chan[0].id = id;
	s_ctx.chan[1].handle = bt_conn_get_handle(s_ctx.conn);
	s_ctx.chan[1].id = id2;
	s_ctx.chan[2].handle = bt_conn_get_handle(s_ctx.conn);
	s_ctx.chan[2].id = id3;

	chans[0] = &s_ctx.chan[0];
	chans[1] = &s_ctx.chan[1];
	chans[2] = &s_ctx.chan[2];


	if (argc > 1 && 4 == strtoul(argv[1], NULL, 0)) {
		ase_num = 4;
		s_ctx.chan[3].handle = bt_conn_get_handle(s_ctx.conn);
		s_ctx.chan[3].id = id4;
		chans[3] = &s_ctx.chan[3];
	}

	uint16_t audio_context = BT_AUDIO_CONTEXT_CONVERSATIONAL|BT_AUDIO_CONTEXT_MEDIA;

	return bt_manager_le_audio_stream_enable(chans, ase_num, audio_context);
}

static int pts_recv_start_ready(int argc, char *argv[])
{
	if (argc == 3)
	{
		s_ctx.chan[0].id = strtoul(argv[1], NULL, 0);
		s_ctx.chan[1].id = strtoul(argv[2], NULL, 0);
		struct bt_audio_chan *chans[2] = {&s_ctx.chan[0], &s_ctx.chan[1]};
		SYS_LOG_INF("id1: %d, id2: %d\n", chans[0]->id, chans[1]->id);
		bt_manager_le_audio_sink_stream_start(chans, 2);
		return 0;
	}
	else if (argc == 2)
	{
		uint8_t id = strtoul(argv[1], NULL, 0);
		struct bt_audio_chan *chans = &s_ctx.chan[0];
		chans->id = id;
		chans->handle = bt_conn_get_handle(s_ctx.conn);
		bt_manager_le_audio_sink_stream_start(&chans, 1);
		return 0;
	}

	uint8_t idx;
	idx = (s_ctx.chan[0].id == 10)? 0:1;
	if (s_ctx.chan[0].id == 10)
	{
		idx = 0;
	}
	else if (s_ctx.chan[1].id == 10)
	{
		idx = 1;
	}
	else
	{
		return 0;
	}
	struct bt_audio_chan *chans = &s_ctx.chan[idx];
	bt_manager_le_audio_sink_stream_start(&chans, 1);

	return 0;
}

static int pts_recv_stop_ready(int argc, char *argv[])
{
	if (argc == 3)
	{
		struct bt_audio_chan *chans[2] = {&s_ctx.chan[0], &s_ctx.chan[1]};
		SYS_LOG_INF("id1: %d, id2: %d\n", chans[0]->id, chans[1]->id);
		bt_manager_le_audio_sink_stream_stop(chans, 2);
		return 0;
	}
	else if (argc == 2)
	{
		uint8_t id = strtoul(argv[1], NULL, 0);
		struct bt_audio_chan *chans = &s_ctx.chan[0];
		chans->id = id;
		chans->handle = bt_conn_get_handle(s_ctx.conn);
		bt_manager_le_audio_sink_stream_stop(&chans, 1);
		return 0;
	}

	uint8_t idx;
	idx = (s_ctx.chan[0].id == 10)? 0:1;
	struct bt_audio_chan *chans = &s_ctx.chan[idx];
	bt_manager_le_audio_sink_stream_stop(&chans, 1);

	return 0;
}

static int pts_set_prefered_qos(int argc, char *argv[])
{
	if (argc < 2)
	{
		SYS_LOG_INF("Used: pts_le set_prefered_qos x(ASE_ID)\n");
		return -EINVAL;
	}
	uint8_t ase_id = strtoul(argv[1], NULL, 0);
	struct bt_audio_preferred_qos qos = {0};

	qos.handle = bt_conn_get_handle(s_ctx.conn);
	qos.id = ase_id;
	qos.framing = BT_AUDIO_UNFRAMED_SUPPORTED;
	qos.phy = BT_AUDIO_PHY_2M;
	qos.rtn = 3;
	qos.latency = 10;
	qos.delay_min = 35000;
	qos.delay_max = 40000;
	qos.preferred_delay_min = 35000;
	qos.preferred_delay_max = 40000;

	return bt_manager_audio_stream_prefer_qos(&qos);
}

static int pts_disable_ase(int argc, char *argv[])
{
	if (argc == 3)
	{
		struct bt_audio_chan *chans[2] = {&s_ctx.chan[0], &s_ctx.chan[1]};
		SYS_LOG_INF("id1: %d, id2: %d\n", chans[0]->id, chans[1]->id);
		bt_manager_le_audio_stream_disable(chans, 2);
		return 0;
	}

	if (argc < 2)
	{
		SYS_LOG_INF("Used: pts_le disable_ase x(ASE_ID)\n");
		return -EINVAL;
	}
	uint8_t ase_id = strtoul(argv[1], NULL, 0);

	s_ctx.chan[0].id = ase_id;
	s_ctx.chan[0].handle = bt_conn_get_handle(s_ctx.conn);
	SYS_LOG_INF("id:%d,hdl:0x%x\n",s_ctx.chan[0].id,s_ctx.chan[0].handle);

	struct bt_audio_chan *chans = &s_ctx.chan[0];

	return bt_manager_le_audio_stream_disable(&chans, 1);
}

static int pts_release_ase(int argc, char *argv[])
{
	if (argc == 3)
	{
		struct bt_audio_chan *chans[2] = {&s_ctx.chan[0], &s_ctx.chan[1]};
		SYS_LOG_INF("id1: %d, id2: %d\n", chans[0]->id, chans[1]->id);
		bt_manager_le_audio_stream_release(chans, 2);
		return 0;
	}

	if (argc < 2)
	{
		SYS_LOG_INF("Used: pts_le release_ase x(ASE_ID)\n");
		return -EINVAL;
	}
	uint8_t ase_id = strtoul(argv[1], NULL, 0);

	s_ctx.chan[0].id = ase_id;
	s_ctx.chan[0].handle = bt_conn_get_handle(s_ctx.conn);
	SYS_LOG_INF("id:%d,hdl:0x%x\n",s_ctx.chan[0].id,s_ctx.chan[0].handle);
	struct bt_audio_chan *chans = &s_ctx.chan[0];

	return bt_manager_le_audio_stream_release(&chans, 1);
}

static int pts_update_metadata(int argc, char *argv[])
{
	uint32_t contexts = BT_AUDIO_CONTEXT_MEDIA;
	if (argc < 2)
	{
		SYS_LOG_INF("Used: pts_le update_metadata x(ASE_ID)\n");
		return -EINVAL;
	}
	uint8_t ase_id = strtoul(argv[1], NULL, 0);

	s_ctx.chan[0].id = ase_id;
	s_ctx.chan[0].handle = bt_conn_get_handle(s_ctx.conn);
	SYS_LOG_INF("id:%d,hdl:0x%x\n",s_ctx.chan[0].id,s_ctx.chan[0].handle);
	struct bt_audio_chan *chans = &s_ctx.chan[0];

	return bt_manager_le_audio_stream_update(&chans, 1, contexts);
}

static int pts_discover_pacs(int argc, char *argv[])
{
	uint8_t flags = (BTSRV_AUDIO_PACS_AUTO_DISCOVER);
	btif_pts_set_auto_audio_evt_flag(flags);
	int ret = btif_pts_audio_core_event(s_ctx.conn, BTSRV_AUDIO_DISCOVER_PACS, NULL);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_write_pacs_sink_loc(int argc, char *argv[])
{
	int ret = btif_pts_audio_core_event(s_ctx.conn, BTSRV_AUDIO_WIRTE_PACS_SINK_LOC, NULL);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_write_pacs_source_loc(int argc, char *argv[])
{
	int ret = btif_pts_audio_core_event(s_ctx.conn, BTSRV_AUDIO_WIRTE_PACS_SOURCE_LOC, NULL);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_discover_ascs(int argc, char *argv[])
{
	uint8_t flags = (BTSRV_AUDIO_ASCS_AUTO_DISCOVER);
	btif_pts_set_auto_audio_evt_flag(flags);

	int ret = btif_pts_audio_core_event(s_ctx.conn, BTSRV_AUDIO_DISCOVER_ASCS, NULL);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_update_pacs_aac(int argc, char *argv[])
{
	uint16_t aac = 0x0FEF;
	if (argc > 1) {
		aac = strtoul(argv[1], NULL, 16);
	}
	btif_pts_set_pacs_aac(s_ctx.conn, aac, aac);
	return 0;
}

static int pts_update_pacs_sac(int argc, char *argv[])
{
	uint16_t sac = 0x0FEF;
	if (argc > 1 && strtoul(argv[1], NULL, 0)) {
		sac = strtoul(argv[1], NULL, 16);
	}
	btif_pts_set_pacs_sac(s_ctx.conn, sac, sac);
	return 0;
}

static int pts_auto_codec_config(int argc, char *argv[])
{
	if (argc != 3)
	{
		SYS_LOG_INF("Used: pts_le auto_codec_config x(ASE_ID) xxx(codec type)\n");
		return -EINVAL;
	}
	s_ctx.lc3_type[IDX_CODEC].ase_id = strtoul(argv[1], NULL, 0);
	s_ctx.lc3_type[IDX_CODEC].type = strtoul(argv[2], NULL, 0);
	uint8_t flags = (BTSRV_AUDIO_PACS_AUTO_DISCOVER
					| BTSRV_AUDIO_ASCS_AUTO_DISCOVER
					| BTSRV_AUDIO_ASCS_AUTO_CODEC_CONFIG);
	btif_pts_set_auto_audio_evt_flag(flags);
	int ret = btif_pts_audio_core_event(s_ctx.conn, BTSRV_AUDIO_DISCOVER_PACS, NULL);

	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_auto_qos_config(int argc, char *argv[])
{
	if (argc != 3)
	{
		SYS_LOG_INF("Used: pts_le auto_codec_config x(ASE_ID) xxx(codec type)\n");
		return -EINVAL;
	}
	s_ctx.lc3_type[IDX_QOS].ase_id = strtoul(argv[1], NULL, 0);
	s_ctx.lc3_type[IDX_QOS].type = strtoul(argv[2], NULL, 0);
	s_ctx.lc3_type[IDX_CODEC].type = s_ctx.lc3_type[IDX_QOS].type / 10;
	s_ctx.lc3_type[IDX_CODEC].ase_id = s_ctx.lc3_type[IDX_QOS].ase_id;
	uint8_t flags = (BTSRV_AUDIO_PACS_AUTO_DISCOVER
					| BTSRV_AUDIO_ASCS_AUTO_DISCOVER
					| BTSRV_AUDIO_ASCS_AUTO_CODEC_CONFIG
					| BTSRV_AUDIO_ASCS_AUTO_QOS_CONFIG);
	btif_pts_set_auto_audio_evt_flag(flags);
	int ret = btif_pts_audio_core_event(s_ctx.conn, BTSRV_AUDIO_DISCOVER_PACS, NULL);

	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_auto_enable_ase(int argc, char *argv[])
{
	if (argc != 2)
	{
		SYS_LOG_INF("Used: pts_le auto_enable_ase x(ASE ID)\n");
		return -EINVAL;
	}
	s_ctx.lc3_type[IDX_QOS].ase_id = strtoul(argv[1], NULL, 0);
	s_ctx.lc3_type[IDX_QOS].type = 1621;
	s_ctx.lc3_type[IDX_CODEC].type = s_ctx.lc3_type[IDX_QOS].type / 10;
	s_ctx.lc3_type[IDX_CODEC].ase_id = s_ctx.lc3_type[IDX_QOS].ase_id;
	uint8_t flags = (BTSRV_AUDIO_PACS_AUTO_DISCOVER
					| BTSRV_AUDIO_ASCS_AUTO_DISCOVER
					| BTSRV_AUDIO_ASCS_AUTO_CODEC_CONFIG
					| BTSRV_AUDIO_ASCS_AUTO_QOS_CONFIG
					| BTSRV_AUDIO_ASCS_AUTO_ENABLE_ASE);
	btif_pts_set_auto_audio_evt_flag(flags);
	int ret = btif_pts_audio_core_event(s_ctx.conn, BTSRV_AUDIO_DISCOVER_PACS, NULL);

	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

extern int bt_bass_set_sync_state(uint8_t src_id, uint8_t pa_sync_state,
			   uint32_t bis_synced[BT_BASS_MAX_SUBGROUPS],
			   uint8_t encrypted);
extern void bt_bass_server_set_conn(struct bt_conn *conn);
static int pts_set_bass_recv_state(int argc, char *argv[])
{
	uint8_t src_id = 1;
	uint8_t pa_sync_state = 1;
	uint8_t encrypt_state = 1;
	uint32_t bis_synced[1] = {0};
	bt_bass_server_set_conn(s_ctx.conn);
	bt_bass_pts_init_recv_state();
	int ret = bt_bass_set_sync_state(src_id, pa_sync_state, bis_synced, encrypt_state);
	SYS_LOG_INF("ret:%d", ret);

	return 0;
}
static int pts_update_bass_recv_state(int argc, char *argv[])
{
	uint8_t src_id = 0;
	uint8_t pa_sync_state = 2;
	uint8_t encrypt_state = 1;
	uint32_t bis_synced[1] = {1};
	if (argc > 1) {
		pa_sync_state = strtoul(argv[1], NULL, 0);
	}
	if (argc > 2) {
		encrypt_state = strtoul(argv[2], NULL, 0);
	}
	if (encrypt_state == 3) {
		bis_synced[0] = 0;
	}

	bt_bass_server_set_conn(s_ctx.conn);
	int ret = bt_bass_set_sync_state(src_id, pa_sync_state, bis_synced, encrypt_state);
	SYS_LOG_INF("ret:%d", ret);
	return 0;
}

static int pts_discover_tmas(int argc, char *argv[])
{
	int ret = btif_pts_audio_core_event(s_ctx.conn, BTSRV_AUDIO_DISCOVER_TMAS, NULL);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_discover_vcs(int argc, char *argv[])
{
	int ret = btif_pts_audio_core_event(s_ctx.conn, BTSRV_AUDIO_DISCOVER_VCS, NULL);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_vcs_update_volume(int argc, char *argv[])
{
	uint8_t vol_val = 100;
	if (argc == 2)
	{
		vol_val = strtoul(argv[1], NULL, 0);
		SYS_LOG_INF("vol_val:%d\n",vol_val);
	}

	int ret = btif_audio_volume_set(bt_conn_get_handle(s_ctx.conn), vol_val);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_vcs_unmute(int argc, char *argv[])
{
	int ret = btif_audio_volume_unmute(bt_conn_get_handle(s_ctx.conn));
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_vcs_mute(int argc, char *argv[])
{
	int ret = btif_audio_volume_mute(bt_conn_get_handle(s_ctx.conn));
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_vcs_read_state(int argc, char *argv[])
{
	int ret = btif_pts_audio_core_event(s_ctx.conn, BTSRV_AUDIO_READ_VCS_STATE, NULL);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_vcs_set_state(int argc, char *argv[])
{
	/*volume,mute,counter*/
	uint8_t state[3] = {0};
	uint8_t vol_step = 1;

	if (argc != 3)
	{
		SYS_LOG_INF("Used: pts_le vcs_set_state xx(volume) x(1-mute,0-unmute)\n");
		return -EINVAL;
	}
	state[0] = strtoul(argv[1], NULL, 0);
	state[1] = strtoul(argv[2], NULL, 0);
	state[2] = 1;

	btif_pts_vcs_set_state(state, vol_step);
	return 0;
}

static int pts_vcs_notify_state(int argc, char *argv[])
{

	int ret = btif_pts_vcs_notify_state(s_ctx.conn);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

#if (defined(CONFIG_BT_VCS_SERVICE) && defined(CONFIG_BT_VOCS))
extern const struct bt_gatt_service_static vocs_svc;
static struct bt_vocs_server *pts_get_vocs_svc(struct bt_conn *conn, uint16_t handle)
{
	return &s_ctx.vocs.srv;
}

static int pts_vocs_init(int argc, char *argv[])
{
	int err = 0;
	struct bt_vocs *vocs = &s_ctx.vocs;
	struct bt_vocs_register_param param = {0};
	vocs->srv.service_p = (struct bt_gatt_service *)&vocs_svc;
	param.cb = NULL;
	param.location = BT_AUDIO_LOCATIONS_FL;//BT_AUDIO_LOCATIONS_FR;
	param.offset = 1;
	err = bt_vocs_register(vocs, &param);
	if (err) {
		SYS_LOG_ERR("err %d\n", err);
		return err;
	}
	bt_vocs_server_cb_register(pts_get_vocs_svc);

	return 0;
}
#endif /*CONFIG_BT_VCS_SERVICE && CONFIG_BT_VOCS*/

static int pts_discover_cas(int argc, char *argv[])
{
	uint8_t flags = (BTSRV_AUDIO_PACS_AUTO_DISCOVER
					| BTSRV_AUDIO_ASCS_AUTO_DISCOVER
					| BTSRV_AUDIO_CAS_AUTO_DISCOVER);

	btif_pts_set_auto_audio_evt_flag(flags);
	int ret = btif_pts_audio_core_event(s_ctx.conn, BTSRV_AUDIO_DISCOVER_PACS, NULL);

	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_discover_csis(int argc, char *argv[])
{
	int ret = btif_pts_audio_core_event(s_ctx.conn, BTSRV_AUDIO_DISCOVER_CAS_CSIS, NULL);

	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_discover_mics(int argc, char *argv[])
{
	int ret = btif_pts_audio_core_event(s_ctx.conn, BTSRV_AUDIO_DISCOVER_MICS, NULL);

	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_mics_read_mute(int argc, char *argv[])
{
	int ret = btif_mics_mute_get(bt_conn_get_handle(s_ctx.conn));

	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_read_csis_lock(int argc, char *argv[])
{
	int ret = btif_pts_audio_core_event(s_ctx.conn, BTSRV_AUDIO_READ_CSIS_LOCK, NULL);

	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_write_csis_lock(int argc, char *argv[])
{
	int ret = btif_pts_audio_core_event(s_ctx.conn, BTSRV_AUDIO_CSIS_WRITE_LOCK, NULL);

	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_write_csis_unlock(int argc, char *argv[])
{
	int ret = btif_pts_audio_core_event(s_ctx.conn, BTSRV_AUDIO_CSIS_WRITE_UNLOCK, NULL);

	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_mics_mute(int argc, char *argv[])
{
	int ret = btif_audio_mic_mute(bt_conn_get_handle(s_ctx.conn));
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_mics_disable(int argc, char *argv[])
{
	int ret = btif_audio_mic_mute_disable(bt_conn_get_handle(s_ctx.conn));
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_set_aics_mute_state(int argc, char *argv[])
{
	if (argc != 2)
	{
		SYS_LOG_INF("Used: pts_le set_aics_mute_state x(mute state)\n");
		return -EINVAL;
	}

	/* unmute--0, mute--1, disable--2 */
	uint8_t mute_state = strtoul(argv[1], NULL, 0);
	btif_pts_aics_set_mute(mute_state);
	pts_set_resume_flag(PTS_RESUME_AICS_STATE);
	return 0;
}

static int pts_set_aics_gain_mode(int argc, char *argv[])
{
	if (argc != 2)
	{
		SYS_LOG_INF("Used: pts_le set_aics_gain_mode x(gain mode)\n");
		return -EINVAL;
	}

	/* manual only--0, auto only--1, manual--2, auto--3 */
	uint8_t gain_mode = strtoul(argv[1], NULL, 0);
	btif_pts_aics_set_gain_mode(gain_mode);
	btif_pts_aics_state_notify(s_ctx.conn);
	pts_set_resume_flag(PTS_RESUME_AICS_STATE);
	return 0;
}

static int pts_csip_test(int argc, char *argv[])
{
	/* pts csip cases do not use ext adv */
	btif_pts_test_set_flag(PTS_TEST_FLAG_CSIP);
	return 0;
}

static int pts_set_csis_sirk(int argc, char *argv[])
{
	uint8_t pts_sirk_value[16] = {0xB8,0x03,0xEA,0xC6,0xAF,0xBB,0x65,0xA2,
								  0x5A,0x41,0xF1,0x53,0x05,0x68,0x8E,0x83};
	if (argc != 2)
	{
		SYS_LOG_INF("Used: pts_le set_csis_sirk x(sirk type)\n");
		return -EINVAL;
	}

	/* sirk type: 0--encrypted, 1--plain text */
	uint8_t sirk_type = strtoul(argv[1], NULL, 0);
	btif_pts_csis_set_sirk(sirk_type, pts_sirk_value);
	return 0;
}

static int pts_set_csis_lock(int argc, char *argv[])
{
	uint8_t val;
	if (argc != 2)
	{
		SYS_LOG_INF("Used: pts_le set_csis_sirk x(sirk type)\n");
		return -EINVAL;
	}

	/* lock type: 1--unlock, 2--lock */
	val = strtoul(argv[1], NULL, 0);
	btif_cas_set_lock(NULL, val);
	return 0;
}

static int pts_csis_lock_notify(int argc, char *argv[])
{
	btif_cas_lock_notify(s_ctx.conn);
	return 0;
}

extern const struct bt_gatt_service_static mcs_svc;
static int pts_mcs_test(int argc, char *argv[])
{
	struct bt_uuid_16 *p_user = (struct bt_uuid_16 *)mcs_svc.attrs[0].user_data;
	p_user->val = BT_UUID_MCS_VAL;
	if (argc == 2) {
		uint8_t cmd_str[3];
		memcpy(&cmd_str[0], argv[1], 2);
		/* Exit mcs test */
		if (!memcmp(cmd_str, "-q", 2)) {
			p_user->val = BT_UUID_GMCS_VAL;
		}
	}

	return 0;
}

static int pts_discover_mcs(int argc, char *argv[])
{
	int ret = btif_pts_audio_core_event(s_ctx.conn, BTSRV_AUDIO_DISCOVER_MCS, NULL);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_read_mcs_track_duration(int argc, char *argv[])
{
	int ret = btif_pts_read_mcs_track_duration(bt_conn_get_handle(s_ctx.conn));
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_read_mcs_track_position(int argc, char *argv[])
{
	int ret = btif_pts_read_mcs_track_position(bt_conn_get_handle(s_ctx.conn));
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_write_mcs_track_position(int argc, char *argv[])
{
	int32_t position = 0;
	if (argc == 2)
	{
		position = strtol(argv[1], NULL, 0);
		SYS_LOG_INF("position:%d\n", position);
	}

	int ret = btif_pts_write_mcs_track_position(bt_conn_get_handle(s_ctx.conn), position);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_write_mcs_play(int argc, char *argv[])
{
	uint8_t opcode = BT_MCS_OPCODE_PLAY;
	int ret = btif_pts_write_media_control(bt_conn_get_handle(s_ctx.conn), opcode);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_write_mcs_pause(int argc, char *argv[])
{
	uint8_t opcode = BT_MCS_OPCODE_PAUSE;
	int ret = btif_pts_write_media_control(bt_conn_get_handle(s_ctx.conn), opcode);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

extern const struct bt_gatt_service_static gtbs_svc;
static int pts_tbs_test(int argc, char *argv[])
{
	struct bt_uuid_16 *p_user = (struct bt_uuid_16 *)gtbs_svc.attrs[0].user_data;
	if (!p_user) {
		SYS_LOG_ERR("tbs not support\n");
		return 0;
	}
	p_user->val = 0x184B;
	if (argc > 1) {
		uint8_t cmd_str[3];
		memcpy(&cmd_str[0], argv[1], 2);
		/* Exit mcs test */
		if (!memcmp(cmd_str, "-q", 2)) {
			p_user->val = 0x184C;
		}
	}

	return 0;
}

static int pts_discover_tbs(int argc, char *argv[])
{
	int ret = btif_pts_audio_core_event(s_ctx.conn, BTSRV_AUDIO_DISCOVER_TBS, NULL);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_discover_gtbs(int argc, char *argv[])
{
	int ret = btif_pts_audio_core_event(s_ctx.conn, BTSRV_AUDIO_DISCOVER_GTBS, NULL);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_read_tbs_provider_name(int argc, char *argv[])
{
	int ret = btif_audio_call_read_tbs_chrc(s_ctx.conn, TBS_CHRC_PROVIDER_NAME);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_read_tbs_uci(int argc, char *argv[])
{
	int ret = btif_audio_call_read_tbs_chrc(s_ctx.conn, TBS_CHRC_UCI);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_read_tbs_technology(int argc, char *argv[])
{
	int ret = btif_audio_call_read_tbs_chrc(s_ctx.conn, TBS_CHRC_TECHNOLOGY);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_read_tbs_uri_schemes(int argc, char *argv[])
{
	int ret = btif_audio_call_read_tbs_chrc(s_ctx.conn, TBS_CHRC_URI_SCHEMES);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_read_tbs_signal_strength(int argc, char *argv[])
{
	int ret = btif_audio_call_read_tbs_chrc(s_ctx.conn, TBS_CHRC_SIGNAL_STRENGTH);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_read_tbs_signal_strength_reporting_interval(int argc, char *argv[])
{
	int ret = btif_audio_call_read_tbs_chrc(s_ctx.conn, TBS_CHRC_STRENGTH_REPORTING_INTERVAL);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_read_tbs_current_calls(int argc, char *argv[])
{
	int ret = btif_audio_call_read_tbs_chrc(s_ctx.conn, TBS_CHRC_CURRENT_CALLS);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_read_tbs_ccid(int argc, char *argv[])
{
	int ret = btif_audio_call_read_tbs_chrc(s_ctx.conn, TBS_CHRC_CCID);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_read_tbs_status_flags(int argc, char *argv[])
{
	int ret = btif_audio_call_read_tbs_chrc(s_ctx.conn, TBS_CHRC_STATUS_FLAGS);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_read_tbs_target_uri(int argc, char *argv[])
{
	int ret = btif_audio_call_read_tbs_chrc(s_ctx.conn, TBS_CHRC_TARGET_URI);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_read_tbs_call_state(int argc, char *argv[])
{
	int ret = btif_audio_call_read_tbs_chrc(s_ctx.conn, TBS_CHRC_CALL_STATE);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_read_tbs_ccp_optional_opcodes(int argc, char *argv[])
{
	int ret = btif_audio_call_read_tbs_chrc(s_ctx.conn, TBS_CHRC_CCP_OPTIONAL_OPCODES);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_read_tbs_incoming_call(int argc, char *argv[])
{
	int ret = btif_audio_call_read_tbs_chrc(s_ctx.conn, TBS_CHRC_INCOMING_CALL);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_read_tbs_friendly_name(int argc, char *argv[])
{
	int ret = btif_audio_call_read_tbs_chrc(s_ctx.conn, TBS_CHRC_FRIENDLY_NAME);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_write_tbs_strength_report_interval(int argc, char *argv[])
{
	bool with_rsp = false;
	if (argc > 1 && strtoul(argv[1], NULL, 0)) {
		with_rsp = true;
	}
	int ret = btif_audio_call_set_strength_report_interval(s_ctx.conn, 10, with_rsp);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_call_accept(int argc, char *argv[])
{
	struct bt_audio_call call = {0};
	call.handle = bt_conn_get_handle(s_ctx.conn);
	if (argc > 1) {
		call.index = strtoul(argv[1], NULL, 0);
	}

	int ret = btif_audio_call_control_operation(&call, BT_TBS_OPCODE_ACCEPT);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_call_terminate(int argc, char *argv[])
{
	struct bt_audio_call call = {0};
	call.handle = bt_conn_get_handle(s_ctx.conn);
	if (argc > 1) {
		call.index = strtoul(argv[1], NULL, 0);
	}

	int ret = btif_audio_call_control_operation(&call, BT_TBS_OPCODE_TERMINATE);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_call_hold(int argc, char *argv[])
{
	struct bt_audio_call call = {0};
	call.handle = bt_conn_get_handle(s_ctx.conn);
	if (argc > 1) {
		call.index = strtoul(argv[1], NULL, 0);
	}

	int ret = btif_audio_call_control_operation(&call, BT_TBS_OPCODE_LOCAL_HOLD);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_call_local_retrieve(int argc, char *argv[])
{
	struct bt_audio_call call = {0};
	call.handle = bt_conn_get_handle(s_ctx.conn);
	if (argc > 1) {
		call.index = strtoul(argv[1], NULL, 0);
	}

	int ret = btif_audio_call_control_operation(&call, BT_TBS_OPCODE_LOCAL_RETRIEVE);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

#define TARGET_URI       "tel:+19991111234"
#define FRIENDLY_NAME    "tel:+19991110011"
#define OUTGOING_URI     "tel:+19991111234"
static int pts_generate_incoming_call(int argc, char *argv[])
{
	uint8_t *uri = TARGET_URI;
	int ret = btif_audio_call_remote_originate(uri);
	uint8_t flags = BTSRV_AUDIO_GENERATE_INCOMING_CALL;
	btif_pts_set_auto_audio_evt_flag(flags);

	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_call_set_target_uri(int argc, char *argv[])
{
	uint8_t *uri = TARGET_URI;
	if (argc > 1) {
		s_ctx.call.index = strtoul(argv[1], NULL, 0);
	}
	s_ctx.call.handle = bt_conn_get_handle(s_ctx.conn);

	int ret = btif_audio_call_set_target_uri(&s_ctx.call, uri);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_call_set_friendly_name(int argc, char *argv[])
{
	uint8_t *uri = FRIENDLY_NAME;
	if (argc > 1) {
		s_ctx.call.index = strtoul(argv[1], NULL, 0);
	}
	s_ctx.call.handle = bt_conn_get_handle(s_ctx.conn);

	int ret = btif_audio_call_set_friendly_name(&s_ctx.call, uri);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_call_set_provider(int argc, char *argv[])
{
	uint8_t *provider = "CMCC";
	if (argc > 1) {
		provider = argv[1];
	}

	int ret = btif_audio_call_set_provider(provider);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_call_set_technology(int argc, char *argv[])
{
	uint8_t technology = 0x05;//5G
	if (argc > 1) {
		technology = strtoul(argv[1], NULL, 0);
	}

	int ret = btif_audio_call_set_technology(technology);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_call_set_status_flags(int argc, char *argv[])
{
	uint8_t status = 3;//0,1-inband,2-silent mode,3-inhand&silent
	if (argc > 1) {
		status = strtoul(argv[1], NULL, 0);
	}

	int ret = btif_audio_call_set_status_flags(status);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_call_remote_alerting(int argc, char *argv[])
{
	if (argc > 1) {
		s_ctx.call.index = strtoul(argv[1], NULL, 0);
	}
	s_ctx.call.handle = bt_conn_get_handle(s_ctx.conn);

	int ret = btif_audio_call_remote_alerting(&s_ctx.call);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_call_remote_accept(int argc, char *argv[])
{
	if (argc > 1) {
		s_ctx.call.index = strtoul(argv[1], NULL, 0);
	}
	s_ctx.call.handle = bt_conn_get_handle(s_ctx.conn);

	int ret = btif_audio_call_remote_accept(&s_ctx.call);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_call_remote_hold(int argc, char *argv[])
{
	if (argc > 1) {
		s_ctx.call.index = strtoul(argv[1], NULL, 0);
	}
	s_ctx.call.handle = bt_conn_get_handle(s_ctx.conn);

	int ret = btif_audio_call_remote_hold(&s_ctx.call);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_call_remote_retrieve(int argc, char *argv[])
{
	if (argc > 1) {
		s_ctx.call.index = strtoul(argv[1], NULL, 0);
	}
	s_ctx.call.handle = bt_conn_get_handle(s_ctx.conn);

	int ret = btif_audio_call_remote_retrieve(&s_ctx.call);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_call_srv_originate(int argc, char *argv[])
{
	uint8_t *uri = TARGET_URI;
	int ret = btif_audio_call_srv_originate(uri);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_call_state_notify(int argc, char *argv[])
{
	int ret = btif_audio_call_state_notify();
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_call_srv_accept(int argc, char *argv[])
{
	if (argc > 1) {
		s_ctx.call.index = strtoul(argv[1], NULL, 0);
	}
	s_ctx.call.handle = bt_conn_get_handle(s_ctx.conn);

	int ret = btif_audio_call_srv_accept(&s_ctx.call);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_call_srv_hold(int argc, char *argv[])
{
	if (argc > 1) {
		s_ctx.call.index = strtoul(argv[1], NULL, 0);
	}
	s_ctx.call.handle = bt_conn_get_handle(s_ctx.conn);

	int ret = btif_audio_call_srv_hold(&s_ctx.call);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_call_srv_retrieve(int argc, char *argv[])
{
	if (argc > 1) {
		s_ctx.call.index = strtoul(argv[1], NULL, 0);
	}
	s_ctx.call.handle = bt_conn_get_handle(s_ctx.conn);

	int ret = btif_audio_call_srv_retrieve(&s_ctx.call);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_call_srv_terminate(int argc, char *argv[])
{
	if (argc > 1) {
		s_ctx.call.index = strtoul(argv[1], NULL, 0);
	}
	s_ctx.call.handle = bt_conn_get_handle(s_ctx.conn);

	int ret = btif_audio_call_srv_terminate(&s_ctx.call, 0);
	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

extern void bt_set_bondable(bool enable);

static int pts_set_bondable(int argc, char *argv[])
{
	uint8_t pts_bondable = 1;
	if (argc > 1) {
		pts_bondable = strtoul(argv[1], NULL, 0);
	}
	if (pts_bondable) {
		bt_set_bondable(1);
	} else {
		bt_set_bondable(0);
	}

	SYS_LOG_INF("bondable:%d\n", pts_bondable);
	return 0;
}


static int pts_start_unicast_audio_stream(int argc, char *argv[])
{
	if (argc != 3)
	{
		SYS_LOG_INF("Used: pts_le start_unicast_audio_stream x(ASE ID) x(CCID count)\n");
		return -EINVAL;
	}
	btif_pts_set_ascs_ccid_count(strtoul(argv[2], NULL, 0));
	s_ctx.lc3_type[IDX_QOS].ase_id = strtoul(argv[1], NULL, 0);
	s_ctx.lc3_type[IDX_QOS].type = 1621;
	s_ctx.lc3_type[IDX_CODEC].type = s_ctx.lc3_type[IDX_QOS].type / 10;
	s_ctx.lc3_type[IDX_CODEC].ase_id = s_ctx.lc3_type[IDX_QOS].ase_id;
	uint8_t flags = (BTSRV_AUDIO_ASCS_AUTO_CODEC_CONFIG
					| BTSRV_AUDIO_ASCS_AUTO_QOS_CONFIG
					| BTSRV_AUDIO_ASCS_AUTO_ENABLE_ASE
					| BTSRV_AUDIO_CAS_AUTO_DISCOVER);
	btif_pts_set_auto_audio_evt_flag(flags);
	int ret = btif_pts_audio_core_event(s_ctx.conn, BTSRV_AUDIO_AUTO_CODEC_CONFIG, NULL);

	SYS_LOG_INF("ret:%d\n", ret);
	return 0;
}

static int pts_le_audio_cap_init(void)
{
	struct bt_audio_capabilities_2 capabilities_2 = {0};

	struct bt_audio_capabilities_2 *caps = &capabilities_2;
	struct bt_audio_capability *cap;

	caps->source_cap_num = 1;
	caps->sink_cap_num = 1;

	/* Audio Source */
	cap = &caps->cap[0];
	cap->format = BT_AUDIO_CODEC_LC3;
	cap->channels = BT_AUDIO_CHANNEL_COUNTS_1;
	cap->frames = 1;

	cap->samples = bt_supported_sampling_from_khz(8) |
			bt_supported_sampling_from_khz(16) |
			bt_supported_sampling_from_khz(24) |
			bt_supported_sampling_from_khz(32) |
			bt_supported_sampling_from_khz(48);
	cap->octets_min = 26;
	cap->octets_max = 155;
	cap->durations = bt_supported_frame_durations_from_ms(10, false);
	cap->durations |= bt_supported_frame_durations_from_ms(7, false);

	cap->preferred_contexts = 0x0FEF;
	caps->source_available_contexts = 0x0FEF;
	caps->source_supported_contexts = 0x0FEF;

	caps->source_locations = BT_AUDIO_LOCATIONS_FL | BT_AUDIO_LOCATIONS_FR;

	/* Audio Sink */
	cap = &caps->cap[1];
	cap->format = BT_AUDIO_CODEC_LC3;

	cap->channels = BT_AUDIO_CHANNEL_COUNTS_1;
	cap->frames = 1;

	cap->samples = bt_supported_sampling_from_khz(8) |
			bt_supported_sampling_from_khz(16) |
			bt_supported_sampling_from_khz(24) |
			bt_supported_sampling_from_khz(32) |
			bt_supported_sampling_from_khz(48);
	cap->octets_min = 26;
	cap->octets_max = 155;
	cap->durations = bt_supported_frame_durations_from_ms(10, false);
	cap->durations |= bt_supported_frame_durations_from_ms(7, false);

	cap->preferred_contexts = 0x0FEF;
	caps->sink_available_contexts = 0x0FEF;
	caps->sink_supported_contexts = 0x0FEF;

	caps->sink_locations = BT_AUDIO_LOCATIONS_FL | BT_AUDIO_LOCATIONS_FR;

	int ret = bt_manager_audio_server_cap_init((struct bt_audio_capabilities *)caps);
	SYS_LOG_INF("ret:%d", ret);
	return ret;
}

static int pts_le_audio_qos_init(void)
{
	struct bt_audio_preferred_qos_default qos;
	uint32_t delay;

	qos.framing = BT_AUDIO_UNFRAMED_SUPPORTED;
	qos.phy = BT_AUDIO_PHY_2M;
	qos.rtn = 3;

	/* Audio Source */
	qos.latency = 10;
	delay = 10000;
	delay += 2000;	// TODO: optimize
	qos.delay_min = delay;
	qos.delay_max = 40000;	/* 40ms at most */
	qos.preferred_delay_min = delay;
	qos.preferred_delay_max = 40000;	/* 40ms at most */

	bt_manager_audio_server_qos_init(true, &qos);

	/* Audio Sink */
	delay = 10000;	// TODO: optimize
	qos.delay_min = delay;
	qos.delay_max = 40000;	/* 40ms at most */
	qos.preferred_delay_min = delay;
	qos.preferred_delay_max = 40000;	/* 40ms at most */

	bt_manager_audio_server_qos_init(false, &qos);

	return 0;
}

void pts_le_clear_keys(void)
{
	uint8_t addr[13] = {0};
	hex_to_str(addr, s_ctx.peer_addr.a.val, PTS_MAC_ADDR_SIZE);
	uint8_t is_clear = hostif_bt_le_clear_device(&s_ctx.peer_addr);
	SYS_LOG_INF("mac:%s,is_clear:%d\n", addr, is_clear);
}

int pts_le_audio_init(void)
{
	struct bt_audio_role role = {0};

	/* master */
	role.num_master_conn = 1;
	role.master_encryption = 1;
	role.lea_connection_enable = 1;
#if 1
	role.unicast_client = 1;
	role.volume_controller = 1;
	role.microphone_controller = 1;
	role.media_device = 1;
	role.disable_name_match = 1;
	role.set_coordinator = 1;
#endif

	/* slave */
	role.num_slave_conn = 1;
#if 1
	role.unicast_server = 1;
	role.volume_renderer = 1;
	role.microphone_device = 1;
	role.media_controller = 1;

	role.call_terminal = 1;
	role.call_gateway = 1;

	role.set_member = 1;
	role.set_rank = 1;
	role.set_size = 1;
	role.encryption = 1;

	/*
	 * broadcast: support broadcast_source and broadcast_sink
	 */
	role.broadcast_sink = 1;
	role.broadcast_source = 1;
	role.scan_delegator = 1;
#endif
	SYS_LOG_INF("\n");

	bt_manager_audio_init(BT_TYPE_LE, &role);

	pts_le_audio_cap_init();
	pts_le_audio_qos_init();

	bt_manager_audio_start(BT_TYPE_LE);

#if (defined(CONFIG_BT_VCS_SERVICE) && defined(CONFIG_BT_VOCS))
	pts_vocs_init(0, NULL);
#endif /*CONFIG_BT_VCS_SERVICE && CONFIG_BT_VOCS*/

	pts_reset_ext_adv(0, NULL);

	return 0;
}

extern const struct bt_gatt_service_static mcs_svc;
extern const struct bt_gatt_service_static gtbs_svc;
extern const struct bt_gatt_service_static mics_aics_svc0;
extern const struct bt_gatt_service_static mics_svc;
extern const struct bt_gatt_service_static vcs_svc;
#ifdef CONFIG_BT_VCS_SERVICE
extern const struct bt_gatt_service_static vcs_vocs_svc;
#endif
#if defined(CONFIG_BT_VOCS)
extern const struct bt_gatt_service_static vocs_svc;
#endif
void pts_le_audio_service_disable(void)
{
	if (bt_pts_is_enabled()) {
		bt_gatt_svc_disable(&vcs_svc);
	} else {
#ifndef CONFIG_BT_LE_AUDIO_MASTER
		bt_gatt_svc_disable(&mcs_svc);
		bt_gatt_svc_disable(&gtbs_svc);
#endif
		bt_gatt_svc_disable(&mics_aics_svc0);
		bt_gatt_svc_disable(&mics_svc);
#ifdef CONFIG_BT_VCS_SERVICE
		bt_gatt_svc_disable(&vcs_vocs_svc);
#endif
#if defined(CONFIG_BT_VOCS)
		bt_gatt_svc_disable(&vocs_svc);
#endif
	}
}

void bt_manger_lea_event_pts_process(btsrv_audio_event_e event, void *data, int size)
{
	switch (event) {
		/* TBS */
		case BTSRV_TBS_INCOMING:
		{
			struct bt_call_report *rep = data;
			s_ctx.call.index = rep->index;
			s_ctx.call.handle = bt_conn_get_handle(s_ctx.conn);
			break;
		}

		default:
			break;
	}
}

static void bt_manager_pts_cb(uint16_t event, uint8_t *param, int param_size)
{
	SYS_LOG_INF("evt:%d\n", event);

	switch (event)
	{
		case BTSRV_AUDIO_AUTO_CODEC_CONFIG:
		{
			pts_codec_config(0, NULL);
			break;
		}
		case BTSRV_AUDIO_AUTO_QOS_CONFIG:
		{
			pts_qos_config(0, NULL);
			break;
		}
		case BTSRV_AUDIO_AUTO_ENABLE_ASE:
		{
			pts_enable_ase(0, NULL);
			break;
		}
		case BTSRV_AUDIO_AUTO_START_READY:
		{
			pts_recv_start_ready(0, NULL);
			break;
		}

		default:
			break;
	}
}

int bt_manager_le_pts_test_start(void)
{
	s_ctx.conn_cbs.connected = pts_connected;
	s_ctx.conn_cbs.disconnected = pts_disconnected;
	hostif_bt_conn_cb_register(&s_ctx.conn_cbs);

	s_ctx.remote_locations = BT_AUDIO_LOCATIONS_FL;

	return btif_pts_start(&bt_manager_pts_cb);
}

int bt_manager_le_pts_test_stop(void)
{
	hostif_bt_conn_cb_unregister(&s_ctx.conn_cbs);
	return btif_pts_stop();
}

static const struct shell_cmd le_pts_test_commands[] =
{
	/* broadcast sink cases for BAP/TMAP/PBP... */
	{ "broadcast_sink_start", pts_broadcast_sink_start, "broadcast sink start xxx_xxxx(ps:PBP-E94B)"},
	{ "broadcast_sink_stop", pts_broadcast_sink_stop, "broadcast sink stop"},
	{ "broadcast_source_start", pts_broadcast_source_start, "broadcast source start x xxxx"},
	{ "broadcast_source_enable", pts_broadcast_source_enable, "broadcast source enable"},
	{ "broadcast_source_reconfig", pts_broadcast_source_reconfig, "broadcast source reconfig"},
	{ "broadcast_source_stop", pts_broadcast_source_stop, "broadcast source stop"},
	{ "reset_ext_adv", pts_reset_ext_adv, "reset ext adv"},
	{ "tmap_discovery", pts_tmap_discovery, "TMAP discovery by adv"},
	{ "bass_discovery", pts_bass_discovery, "BASS discovery by adv"},
	{ "bap_discovery", pts_bap_discovery, "BAP discovery by adv"},

	/* LE connect/disconnect cases */
	{ "ble_create_acl", pts_ble_create_acl, "create le acl"},
	{ "ble_create_acl_cancel", pts_ble_create_acl_cancel, "create le acl cancel"},
	{ "ble_acl_disconnect", pts_ble_acl_disconnect, "Terminate Connection Procedure"},
	{ "ble_wait_connect", pts_ble_wait_connect, "wait ble connect"},
	{ "ble_clear_ltk", pts_ble_clear_ltk, "clear ltk"},
	{ "ble_set_sc_level", pts_ble_set_sc_level, "set sc level"},
	{ "bondable", pts_set_bondable, "set bondable"},

	/* PACS cases */
	{ "update_pacs_aac", pts_update_pacs_aac, "update PACS Available Audio Contexts"},
	{ "update_pacs_sac", pts_update_pacs_sac, "update PACS Supported Audio Contexts"},

	/* BAP cases */
	{ "discover_pacs", pts_discover_pacs, "discover PACS"},
	{ "write_pacs_sink_loc", pts_write_pacs_sink_loc, "write PACS sink locations"},
	{ "write_pacs_source_loc", pts_write_pacs_source_loc, "write PACS source locations"},
	{ "discover_ascs", pts_discover_ascs, "discover ASCS"},
	{ "config_remote_locations", pts_cfg_remote_locations, "configure remote locations"},
	{ "codec_config", pts_codec_config, "codec config by unicast client/server(ASE_ID,codec_type)"},
	{ "qos_config", pts_qos_config, "config QoS by unicast client(ASE_ID,codec_type)"},
	{ "enable_ase", pts_enable_ase, "enable ase by unicast client(ASE_ID)"},
	{ "auto_codec_config", pts_auto_codec_config, "codec config by unicast client/server(ASE_ID,codec_type)"},
	{ "auto_qos_config", pts_auto_qos_config, "config QoS by unicast client(ASE_ID,codec_type)"},
	{ "auto_enable_ase", pts_auto_enable_ase, "ASE auto enable(ASE_ID)"},
	{ "recv_start_ready", pts_recv_start_ready, "ASE start ready"},
	{ "recv_stop_ready", pts_recv_stop_ready, "ASE stop ready"},
	{ "update_metadata", pts_update_metadata, "update metadata by server/client(ASE_ID)"},
	{ "qos2_config", pts_qos2_config, "config QoS2"},
	{ "qos_config_ases", pts_qos_config_ases, "config QoS for 3 or 4 ases"},
	{ "enable_ase2", pts_enable_ase2, "enable 2 ase"},
	{ "enable_ases", pts_enable_ases, "enable 3 or 4 ases"},
	{ "gap_le_br_adv", pts_gap_le_br_adv, "LE and BR/EDR adv"},

	/* ASCS cases */
	{ "set_prefered_qos", pts_set_prefered_qos, "set prefered QoS by unicast server(ASE_ID)"},
	{ "disable_ase", pts_disable_ase, "disable ASE by unicast server(ASE_ID)"},
	{ "release_ase", pts_release_ase, "release ASE by unicast server(ASE_ID)"},

	/* bass cases */
	{ "set_bass_recv_state", pts_set_bass_recv_state, "set pa_state and encrypt_state for bass"},
	{ "update_bass_recv_state", pts_update_bass_recv_state, "update pa_state and encrypt_state for bass"},

	/* TMAP cases */
	{ "discover_tmas", pts_discover_tmas, "discover TMAS"},

	/* VCP cases */
	{ "discover_vcs", pts_discover_vcs, "discover VCS"},
	{ "vcs_update_volume", pts_vcs_update_volume, "update volume"},
	{ "vcs_unmute", pts_vcs_unmute, "unmute"},
	{ "vcs_mute", pts_vcs_mute, "mute"},
	{ "vcs_read_state", pts_vcs_read_state, "vcs read state"},
	{ "vcs_set_state", pts_vcs_set_state, "vcs set state(volume,mute_state)"},
	{ "vcs_notify_state", pts_vcs_notify_state, "vcs notify state"},

	/* CAP cases */
	{ "discover_cas", pts_discover_cas, "discover CAS"},
	{ "start_unicast_audio_stream", pts_start_unicast_audio_stream, "start unicast audio stream"},
	{ "cap_discovery", pts_cap_discovery, "CAP discovery by adv"},
	{ "discover_csis", pts_discover_csis, "discover CSIS"},

	/* MICP/MICS cases*/
	{ "discover_mics", pts_discover_mics, "discover MICS"},
	{ "mics_read_mute", pts_mics_read_mute, "read mute"},
	{ "mics_mute", pts_mics_mute, "mute"},
	{ "mics_disable", pts_mics_disable, "disable"},

	/*AICS cases*/
	{ "set_aics_mute_state", pts_set_aics_mute_state, "set mute state"},
	{ "set_aics_gain_mode", pts_set_aics_gain_mode, "gain mode:0-manual only,1-auto only"},
	/*CSIP cases*/
	{ "csip_test", pts_csip_test, "set csip test flag"},
	{ "read_csis_lock", pts_read_csis_lock, "read csis lock"},
	{ "write_csis_lock", pts_write_csis_lock, "csis lock"},
	{ "write_csis_unlock", pts_write_csis_unlock, "csis unlock"},
	{ "csip_discovery", pts_csip_discovery, "csis rsi adv"},
	{ "set_csis_sirk", pts_set_csis_sirk, "set csis sirk"},
	{ "set_csis_lock", pts_set_csis_lock, "set csis lock"},
	{ "csis_lock_notify", pts_csis_lock_notify, "csis lock notify"},
	/*MCS/MCP cases*/
	{ "mcs_test", pts_mcs_test, "mcs test"},
	{ "discover_mcs", pts_discover_mcs, "discover mcs"},
	{ "read_mcs_track_duration", pts_read_mcs_track_duration, "mcs read track duration"},
	{ "read_mcs_track_position", pts_read_mcs_track_position, "mcs read track position"},
	{ "write_mcs_track_position", pts_write_mcs_track_position, "mcs write track position"},
	{ "write_mcs_play", pts_write_mcs_play, "write mcs play"},
	{ "write_mcs_pause", pts_write_mcs_pause, "write mcs pause"},
	/*CCP/TBS cases*/
	{ "tbs_test", pts_tbs_test, "tbs sever test"},
	{ "discover_tbs", pts_discover_tbs, "discover tbs"},
	{ "discover_gtbs", pts_discover_gtbs, "discover gtbs"},
	{ "read_tbs_provider_name", pts_read_tbs_provider_name, "read tbs provider name"},
	{ "read_tbs_uci", pts_read_tbs_uci, "read tbs UCI"},
	{ "read_tbs_technology", pts_read_tbs_technology, "read tbs technology"},
	{ "read_tbs_uri_schemes", pts_read_tbs_uri_schemes, "read tbs uri_schemes"},
	{ "read_tbs_signal_strength", pts_read_tbs_signal_strength, "read tbs signal_strength"},
	{ "read_tbs_signal_strength_reporting_interval",
	pts_read_tbs_signal_strength_reporting_interval,
	"read tbs signal_strength_reporting_interval"},
	{ "read_tbs_current_calls", pts_read_tbs_current_calls, "read tbs current_calls"},
	{ "read_tbs_ccid", pts_read_tbs_ccid, "read tbs ccid"},
	{ "read_tbs_status_flags", pts_read_tbs_status_flags, "read tbs status_flags"},
	{ "read_tbs_target_uri", pts_read_tbs_target_uri, "read tbs target_uri"},
	{ "read_tbs_call_state", pts_read_tbs_call_state, "read tbs call_state"},
	{ "read_tbs_ccp_optional_opcodes", pts_read_tbs_ccp_optional_opcodes, "read tbs ccp_optional_opcodes"},
	{ "read_tbs_incoming_call", pts_read_tbs_incoming_call, "read tbs incoming_call"},
	{ "read_tbs_friendly_name", pts_read_tbs_friendly_name, "read tbs friendly_name"},
	{ "write_tbs_strength_report_interval",
	pts_write_tbs_strength_report_interval,
	"write tbs strength_report_interval"},
	{ "call_accept", pts_call_accept, "ccp accept (call idx)"},
	{ "call_terminate", pts_call_terminate, "ccp terminate (call idx)"},
	{ "call_hold", pts_call_hold, "ccp hold (call idx)"},
	{ "call_local_retrieve", pts_call_local_retrieve, "ccp local_retrieve (call idx)"},
	{ "generate_incoming_call", pts_generate_incoming_call, "tbs incoming call"},
	{ "call_set_target_uri", pts_call_set_target_uri, "tbs set target uri"},
	{ "call_set_friendly_name", pts_call_set_friendly_name, "tbs set friendly name"},
	{ "call_set_provider", pts_call_set_provider, "tbs set provider name"},
	{ "call_set_technology", pts_call_set_technology, "tbs set technology"},
	{ "call_set_status_flags", pts_call_set_status_flags, "tbs set status flags"},
	{ "call_remote_alerting", pts_call_remote_alerting, "tbs remote alerting"},
	{ "call_remote_accept", pts_call_remote_accept, "tbs remote accept"},
	{ "call_remote_hold", pts_call_remote_hold, "tbs remote hold"},
	{ "call_remote_retrieve", pts_call_remote_retrieve, "tbs remote retrieve"},
	{ "call_srv_originate", pts_call_srv_originate, "tbs local originate"},
	{ "call_state_notify", pts_call_state_notify, "tbs call state notify"},
	{ "call_srv_accept", pts_call_srv_accept, "tbs call accept"},
	{ "call_srv_hold", pts_call_srv_hold, "tbs call hold"},
	{ "call_srv_retrieve", pts_call_srv_retrieve, "tbs call retrieve"},
	{ "call_srv_terminate", pts_call_srv_terminate, "tbs call terminate"},

	{ NULL, NULL, NULL}
};

SHELL_REGISTER(PTS_TEST_LE_SHELL_MODULE, le_pts_test_commands);

