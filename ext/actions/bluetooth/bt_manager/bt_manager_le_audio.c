/*
 * Copyright (c) 2021 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <bt_manager.h>
#include <audio_system.h>
#include <volume_manager.h>
#include <audio_policy.h>
#include <property_manager.h>
#include <hex_str.h>
#include <bt_manager_inner.h>

static void bt_manager_le_audio_callback(btsrv_audio_event_e event,
				void *data, int size);

/* FIXME: move to Kconfig or any better way? */
#define NUM_PACS_CLI_PAC_PER_CONN	2
#define NUM_PACS_PAC_PER_CONN	4
#define NUM_ASCS_CLI_ASE_PER_CONN	4

#define NUM_PACS_SRV_PAC	2
#define NUM_ASCS_SRV_ASE_PER_CONN	3
#define NUM_CIS_CHAN_PER_CONN	2
#define NUM_AICS_SRV	1
#define NUM_AICS_CLI_PER_CONN	1

// TODO: broadcast source/sink ...
#define NUM_BROAD_SRC_CONN	1	// TODO: rename???
#define NUM_BROAD_SINK_CONN	1	// TODO: rename???
#define NUM_BIS_CHAN_PER_CONN	2
#define NUM_SUBGROUP_PER_BASE	2
#define NUM_BIS_PER_SUBGROUP	2
#define NUM_BROAD_INFO	4

/* Unicast Announcement Type, Generic Audio */
#define UNICAST_ANNOUNCEMENT_GENERAL    0x00
#define UNICAST_ANNOUNCEMENT_TARGETED   0x01

#ifdef CONFIG_BT_LEA_PTS_TEST
static uint8_t bt_le_audio_buf[9720] __aligned(4) __in_section_unique(bthost.bss);
#else
static uint8_t bt_le_audio_buf[CONFIG_BT_LE_AUDIO_BUF_SIZE] __aligned(4) __in_section_unique(bthost.bss);
#endif

signed int print_hex(const char* prefix, const uint8_t* data, int size);

static void le_audio_volume_init(struct bt_audio_config *config)
{
	int vol;

	if (!config->vol_srv_num) {
		return;
	}

	vol = system_volume_get(AUDIO_STREAM_LE_AUDIO);
	if (vol < 0) {
		SYS_LOG_ERR("vol: %d", vol);
	}

	config->initial_volume = bt_manager_music_to_lea_volume((int)vol);
	config->volume_step = 10;
}

void bt_manager_le_audio_get_sirk(uint8_t *p_sirk)
{
	int ret = 0;
	static uint8_t sirk_tmp[16] = {0};
	uint8_t sirk[16] = {
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0xcd, 0xce,
		0x22, 0xfd, 0xa1, 0x21, 0x09, 0x7d, 0x7d, 0x45,
	};

	if (!p_sirk) {
		SYS_LOG_ERR("param err!");
		return;
	}

#ifdef CONFIG_PROPERTY
	ret = property_get(CFG_LEA_SIRK, sirk_tmp, sizeof(sirk_tmp));
	if (ret != sizeof(sirk_tmp)) {
		ret = -1;
	}else {
		//print_hex("old_sirk:", sirk_tmp, 16);
	}
#endif

	if (ret < 0) {
		uint8_t mac_str[13];

		memset(mac_str, 0, sizeof(mac_str));
		ret = property_get(CFG_BT_MAC, mac_str, (sizeof(mac_str)-1));
		if (ret != (sizeof(mac_str)-1)) {
			SYS_LOG_ERR("mac err!");
			return;
		}
		memcpy(sirk, mac_str, (sizeof(mac_str)-1));
		memcpy(sirk_tmp, sirk, sizeof(sirk_tmp));
		property_set(CFG_LEA_SIRK, sirk_tmp, sizeof(sirk_tmp));
		print_hex("new_sirk:", sirk_tmp, 16);
	}

	memcpy(p_sirk, sirk_tmp, 16);
}

static int bt_manager_init_adv_data(struct bt_audio_config* cfg)
{
	int err = 0;

	uint8_t entry = 0;
	uint8_t len = 0;
	uint32_t buf_size = BT_LE_AUDIO_ADV_MAX_ENTRY * sizeof(struct bt_data) + BT_LE_AUDIO_ADV_MAX_LEN;
	uint8_t* buf = mem_malloc(buf_size);
	if (!buf) {
		SYS_LOG_ERR("malloc faied");
		return -ENOMEM;
	}
	memset(buf, 0, buf_size);
	struct bt_data* data = (struct bt_data*)buf;
	uint8_t* adv_buf = buf + (BT_LE_AUDIO_ADV_MAX_ENTRY * sizeof(struct bt_data));

	data[entry].type = BT_DATA_FLAGS;
	data[entry].data_len = 1;
	data[entry].data = &adv_buf[len];
#ifdef CONFIG_BT_CROSS_TRANSPORT_KEY
	adv_buf[len++] = BT_LE_AD_LE_EDR_SAME_DEVICE_CTRL | BT_LE_AD_LE_EDR_SAME_DEVICE_HOST;
#else
	if (cfg->br) {
		adv_buf[len++] = BT_LE_AD_GENERAL;
	} else {
		adv_buf[len++] = BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR;
	}
#endif
	entry++;

#if 0
	/* Name entry */
#if defined(CONFIG_BT_CROSS_TRANSPORT_KEY)
	property_get_string(CFG_BT_NAME, &adv_buf[len], 31, CONFIG_BT_DEVICE_NAME);
#else
	property_get_string(CFG_BLE_AUDIO, &adv_buf[len], 31, CONFIG_BT_DEVICE_NAME);
#endif
	data[entry].type = BT_DATA_NAME_COMPLETE;
	data[entry].data_len = strlen(&adv_buf[len]);
	data[entry].data = &adv_buf[len];
	len += data[entry].data_len;
	entry++;
#endif

#ifdef CONFIG_BT_LEA_PTS_TEST
	uint8_t *rsi_data_ptr = NULL;
	err = btif_audio_update_rsi(&rsi_data_ptr);
	if (cfg->cas_srv_num && !err && rsi_data_ptr) {
		data[entry].type = BT_DATA_CSIS_RSI;
		data[entry].data_len = 6;
		data[entry].data = &adv_buf[len];
		memcpy(&adv_buf[len],rsi_data_ptr,6);
		len += data[entry].data_len;
		entry++;
	}
#endif

	if (cfg->uni_srv_num) {
		/* ASCS UUID */
		uint16_t sink_contexts = cfg->sink_available_contexts;
		uint16_t source_contexts = cfg->source_available_contexts;

		/* General Announcement */
		uint8_t ann_type = UNICAST_ANNOUNCEMENT_GENERAL;
		if (cfg->target_announcement) {
			ann_type = UNICAST_ANNOUNCEMENT_TARGETED;
		}
		SYS_LOG_INF("ann_type:%d\n",ann_type);
		data[entry].type = BT_DATA_SVC_DATA16;
		data[entry].data_len = 8;
		data[entry].data = &adv_buf[len];
		adv_buf[len++] = (uint8_t)BT_UUID_ASCS_VAL;
		adv_buf[len++] = (uint8_t)(BT_UUID_ASCS_VAL >> 8);
		adv_buf[len++] = ann_type;

		/* FIXME: initialize Available Audio Contexts until cap_init */
		/* Sink Available Audio Contexts */
		adv_buf[len++] = (uint8_t)sink_contexts;
		adv_buf[len++] = (uint8_t)(sink_contexts >> 8);
		/* Source Available Audio Contexts */
		adv_buf[len++] = (uint8_t)source_contexts;
		adv_buf[len++] = (uint8_t)(source_contexts >> 8);

		adv_buf[len++] = 0;
		entry++;
	}

	/* TMAS UUID */
	uint16_t role = btif_audio_get_local_tmas_role();
	data[entry].type = BT_DATA_SVC_DATA16;
	data[entry].data_len = 4;
	data[entry].data = &adv_buf[len];
	adv_buf[len++] = (uint8_t)BT_UUID_TMAS_VAL;
	adv_buf[len++] = (uint8_t)(BT_UUID_TMAS_VAL >> 8);
	/* TMAP Role */
	adv_buf[len++] = (uint8_t)role;
	adv_buf[len++] = (uint8_t)(role >> 8);
	entry++;

	if (cfg->gmas_server) {
		data[entry].type = BT_DATA_SVC_DATA16;
		data[entry].data_len = 3;
		data[entry].data = &adv_buf[len];
		/* GMAS UUID */
		adv_buf[len++] = (uint8_t)BT_UUID_GMAS_VAL;
		adv_buf[len++] = (uint8_t)(BT_UUID_GMAS_VAL >> 8);
		/* TMAP Role */
		adv_buf[len++] = btif_audio_get_loc_gmas_role();
		entry++;
	}

	if (cfg->vol_srv_num) {
		/* VCS UUID */
		data[entry].type = BT_DATA_SVC_DATA16;
		data[entry].data_len = 2;
		data[entry].data = &adv_buf[len];
		adv_buf[len++] = (uint8_t)BT_UUID_VCS_VAL;
		adv_buf[len++] = (uint8_t)(BT_UUID_VCS_VAL >> 8);
		entry++;
	}

	if (cfg->mic_srv_num) {
		/* MICS UUID */
		data[entry].type = BT_DATA_SVC_DATA16;
		data[entry].data_len = 2;
		data[entry].data = &adv_buf[len];
		adv_buf[len++] = (uint8_t)BT_UUID_MICS_VAL;
		adv_buf[len++] = (uint8_t)(BT_UUID_MICS_VAL >> 8);
		entry++;
	}

	if (cfg->pacs_srv_num) {
		/* PACS UUID */
		data[entry].type = BT_DATA_SVC_DATA16;
		data[entry].data_len = 2;
		data[entry].data = &adv_buf[len];
		adv_buf[len++] = (uint8_t)BT_UUID_PACS_VAL;
		adv_buf[len++] = (uint8_t)(BT_UUID_PACS_VAL >> 8);
		entry++;
	}

	if (cfg->cas_srv_num) {
        /* CAS UUID */
        data[entry].type = BT_DATA_SVC_DATA16;
        data[entry].data_len = 3;
        data[entry].data = &adv_buf[len];
        adv_buf[len++] = (uint8_t)BT_UUID_CAS_VAL;
        adv_buf[len++] = (uint8_t)(BT_UUID_CAS_VAL >> 8);
        adv_buf[len++] = bt_manager_lea_get_cas_announcement_type();
        entry++;
    }

    /*APPEARANCE TYPE */
    data[entry].type = BT_DATA_GAP_APPEARANCE;
    data[entry].data_len = 2;
    data[entry].data = &adv_buf[len];
    adv_buf[len++] = 0x41;
    adv_buf[len++] = 0x08; //Speaker
    entry++;

	err = btif_audio_init_adv_data(data, entry);
	mem_free(buf);
	return err;
}

int bt_manager_le_update_adv_data(struct bt_audio_config* cfg)
{
	if (!cfg) {
		SYS_LOG_ERR("no cfg\n");
		return -EINVAL;
	}

	return bt_manager_init_adv_data(cfg);
}

int bt_manager_le_audio_init(struct bt_audio_role *role)
{
	struct bt_audio_config c = {0};
	uint32_t num;
	int err = 0;

	if (!role) {
		return -EINVAL;
	}

	SYS_LOG_INF("num_master_conn: %d, num_slave_conn: %d",
		role->num_master_conn, role->num_slave_conn);

	c.num_master_conn = role->num_master_conn;
	c.num_slave_conn = role->num_slave_conn;
	c.lea_connection_enable = role->lea_connection_enable;
	c.br = role->br;
	c.remote_keys = role->remote_keys;
	c.test_addr = role->test_addr;
	c.master_latency = role->master_latency;
	c.sirk_enabled = role->sirk_enabled;
	c.sirk_encrypted = role->sirk_encrypted;
	if (role->sirk_enabled) {
		memcpy(c.set_sirk, role->set_sirk, 16);
	}

	c.gmas_server = role->gmas_server;
	c.gmas_config = role->gmas_config;
	c.sink_available_contexts = role->sink_available_contexts;
	c.source_available_contexts= role->source_available_contexts;
	/* disable remote_keys in test_addr case */
	if (role->test_addr) {
		c.remote_keys = 0;
	}

	c.relay = role->relay;
	c.disable_name_match = role->disable_name_match;

	if (role->num_master_conn) {
		c.master_encryption = role->master_encryption;

		if (role->broadcast_assistant || role->unicast_client) {
			c.pacs_cli_num = 1;
		}
		if (c.pacs_cli_num == 1) {
			c.pacs_cli_dev_num = role->num_master_conn;
			num = NUM_PACS_CLI_PAC_PER_CONN * role->num_master_conn;
			if (num > 31) {
				SYS_LOG_ERR("pacs_cli_pac_num: %d", num);
				return -EINVAL;
			}
			c.pacs_cli_pac_num = num;
			num = NUM_PACS_PAC_PER_CONN * role->num_master_conn;
			if (num > 31) {
				SYS_LOG_ERR("pacs_pac_num: %d", num);
				return -EINVAL;
			}
			c.pacs_pac_num = num;
		}
		if (role->set_coordinator) {
			c.cas_cli_num = 1;
			c.cas_cli_dev_num = role->num_master_conn;
		}
		if (role->call_gateway) {
			c.call_srv_num = 1;
		}
		if (role->volume_controller) {
			c.vol_cli_num = 1;
			c.vcs_cli_dev_num = role->num_master_conn;
		}
		if (role->microphone_controller) {
			c.mic_cli_num = 1;
			c.mic_cli_dev_num = role->num_master_conn;
			num = NUM_AICS_CLI_PER_CONN * role->num_master_conn;
			if (num > 31) {
				SYS_LOG_ERR("aics cli num: %d", num);
				return -EINVAL;
			}
			c.aics_cli_num = num;
		}
		if (role->media_device) {
			c.media_srv_num = 1;
		}
		if (role->unicast_client) {
			c.uni_cli_num = 1;
			c.uni_cli_dev_num = role->num_master_conn;
			num = NUM_ASCS_CLI_ASE_PER_CONN * role->num_master_conn;
			if (num > 31) {
				SYS_LOG_ERR("ascs_cli_ase_num: %d", num);
				return -EINVAL;
			}
			c.ascs_cli_ase_num = num;
		}
		if (role->broadcast_assistant) {
			c.bass_cli_num = 1;
			c.bass_cli_dev_num = role->num_master_conn;
		}
	}

	if (role->num_slave_conn) {
		c.encryption = role->encryption;
		c.target_announcement = role->target_announcement;

		c.pacs_srv_num = 1;
		c.pacs_srv_pac_num = NUM_PACS_SRV_PAC;
		if (role->set_member) {
			c.cas_srv_num = 1;
			c.set_size = role->set_size;
			c.set_rank = role->set_rank;
		}
		if (role->call_terminal) {
			c.call_cli_num = 1;
			c.call_cli_dev_num = role->num_slave_conn;
		}
		if (role->volume_renderer) {
			c.vol_srv_num = 1;
		}
		if (role->microphone_device) {
			c.mic_srv_num = 1;
			c.aics_srv_num = NUM_AICS_SRV;
		}
		if (role->media_controller) {
			c.media_cli_num = 1;
			c.mcs_cli_dev_num = role->num_slave_conn;
		}
		if (role->unicast_server) {
			c.uni_srv_num = 1;
			c.uni_srv_dev_num = role->num_slave_conn;
			num = NUM_ASCS_SRV_ASE_PER_CONN * role->num_slave_conn;
			if (num > 31) {
				SYS_LOG_ERR("ascs_srv_ase_num: %d", num);
				return -EINVAL;
			}
			c.ascs_srv_ase_num = num;
		}
		if (role->scan_delegator) {
			c.bass_srv_num = 1;
		}
	}else if(role->broadcast_sink){
		c.pacs_srv_num = 1;
		c.pacs_srv_pac_num = NUM_PACS_SRV_PAC;
	}

	num = role->num_master_conn + role->num_slave_conn;

	if (num > 0) {
		c.tmas_cli_num = 1;
		c.tmas_cli_dev_num = num;
	}

	if (role->gmas_client) {
		c.gmas_cli_num = 1;
		c.gmas_cli_dev_num = num;
	}

	num *= NUM_CIS_CHAN_PER_CONN;
	if (num > 31) {
		SYS_LOG_ERR("cis_chan_num: %d", num);
		return -EINVAL;
	}
	c.cis_chan_num = num;

#if 1
	if (role->broadcast_source) {
		c.broad_src_num = 1;
		c.broad_src_dev_num = NUM_BROAD_SRC_CONN;
	}
	if (role->broadcast_sink) {
		c.broad_sink_num = 1;
		c.broad_sink_dev_num = NUM_BROAD_SINK_CONN;
	}

	num = c.broad_src_num + c.broad_sink_num;
	if (num > 0) {
		num *= NUM_SUBGROUP_PER_BASE;
		if (num > 31) {
			SYS_LOG_ERR("subgroup_num: %d", num);
			return -EINVAL;
		}
		c.broad_subgroup_num = num;

		num *= NUM_BIS_PER_SUBGROUP;
		if (num > 31) {
			SYS_LOG_ERR("bis_num: %d", num);
			return -EINVAL;
		}
		c.broad_bis_num = num;

		num = c.broad_src_num + c.broad_sink_num;
		num *= NUM_BIS_CHAN_PER_CONN;
		if (num > 31) {
			SYS_LOG_ERR("bis_chan_num: %d", num);
			return -EINVAL;
		}
		c.bis_chan_num = num;
	}

	if (role->broadcast_assistant || role->broadcast_sink) {
		c.broad_info_num = NUM_BROAD_INFO;
	}
#endif

	c.secure = role->secure;

	le_audio_volume_init(&c);

	err = btif_audio_init(c, bt_manager_le_audio_callback,
		bt_le_audio_buf, sizeof(bt_le_audio_buf));
	if (err) {
		return err;
	}
	return bt_manager_init_adv_data(&c);
}

static void bt_manager_le_audio_callback(btsrv_audio_event_e event,
				void *data, int size)
{
	SYS_LOG_DBG("%d", event);

	switch (event) {
	case BTSRV_ACL_CONNECTED:
		bt_manager_audio_conn_event(BT_CONNECTED, data, size);
		break;
	case BTSRV_ACL_DISCONNECTED:
		bt_manager_audio_conn_event(BT_DISCONNECTED, data, size);
		break;
	case BTSRV_CIS_CONNECTED:
		bt_manager_audio_conn_event(BT_AUDIO_CONNECTED, data, size);
		break;
	case BTSRV_CIS_DISCONNECTED:
		bt_manager_audio_conn_event(BT_AUDIO_DISCONNECTED, data, size);
		break;
	case BTSRV_AUDIO_STOPPED:
		bt_manager_audio_stopped_event(BT_TYPE_LE);
		break;
	case BTSRV_ASCS_CONNECT:
		bt_manager_audio_conn_event(BT_AUDIO_ASCS_CONNECTION, data, size);
		break;
	case BTSRV_ASCS_DISCONNECT:
		bt_manager_audio_conn_event(BT_AUDIO_ASCS_DISCONNECTION, data, size);
		break;
	case BTSRV_AUDIO_TWS_CONNECTED:
		bt_manager_audio_conn_event(BT_TWS_CONNECTION_EVENT, data, size);
		break;

	/* ASCS */
	case BTSRV_ASCS_CONFIG_CODEC:
		bt_manager_audio_stream_event(BT_AUDIO_STREAM_CONFIG_CODEC, data, size);
		break;
	case BTSRV_ASCS_PREFER_QOS:
		bt_manager_audio_stream_event(BT_AUDIO_STREAM_PREFER_QOS, data, size);
		break;
	case BTSRV_ASCS_CONFIG_QOS:
		bt_manager_audio_stream_event(BT_AUDIO_STREAM_CONFIG_QOS, data, size);
		break;
	case BTSRV_ASCS_ENABLING:
		bt_manager_audio_stream_event(BT_ASCS_ENABLE, data, size);
		break;	
	case BTSRV_ASCS_ENABLE:
		bt_manager_audio_stream_event(BT_AUDIO_STREAM_ENABLE, data, size);
		break;
	case BTSRV_ASCS_DISABLE:
		bt_manager_audio_stream_event(BT_AUDIO_STREAM_DISABLE, data, size);
		break;
	case BTSRV_ASCS_UPDATE:
		bt_manager_audio_stream_event(BT_AUDIO_STREAM_UPDATE, data, size);
		break;
	case BTSRV_ASCS_RECV_START:
		bt_manager_audio_stream_event(BT_AUDIO_STREAM_START, data, size);
		break;
	case BTSRV_ASCS_RECV_STOP:
		bt_manager_audio_stream_event(BT_AUDIO_STREAM_STOP, data, size);
		break;
	case BTSRV_ASCS_RELEASE:
		bt_manager_audio_stream_event(BT_AUDIO_STREAM_RELEASE, data, size);
		break;
	case BTSRV_ASCS_ASES:
		bt_manager_audio_stream_event(BT_AUDIO_DISCOVER_ENDPOINTS, data, size);
		break;
	/* PACS */
	case BTSRV_PACS_CAPS:
		bt_manager_audio_stream_event(BT_AUDIO_DISCOVER_CAPABILITY, data, size);
		break;
	/* CIS */
	case BTSRV_CIS_PARAMS:
		bt_manager_audio_stream_event(BT_AUDIO_PARAMS, data, size);
		break;

	/* TBS */
	case BTSRV_TBS_INCOMING:
		bt_manager_call_event(BT_CALL_INCOMING, data, size);
		break;
	case BTSRV_TBS_DIALING:
		bt_manager_call_event(BT_CALL_DIALING, data, size);
		break;
	case BTSRV_TBS_ALERTING:
		bt_manager_call_event(BT_CALL_ALERTING, data, size);
		break;
	case BTSRV_TBS_ACTIVE:
		bt_manager_call_event(BT_CALL_ACTIVE, data, size);
		break;
	case BTSRV_TBS_LOCALLY_HELD:
		bt_manager_call_event(BT_CALL_LOCALLY_HELD, data, size);
		break;
	case BTSRV_TBS_REMOTELY_HELD:
		bt_manager_call_event(BT_CALL_REMOTELY_HELD, data, size);
		break;
	case BTSRV_TBS_HELD:
		bt_manager_call_event(BT_CALL_HELD, data, size);
		break;
	case BTSRV_TBS_ENDED:
		bt_manager_call_event(BT_CALL_ENDED, data, size);
		break;

	/* VCS */
	case BTSRV_VCS_UP:
		bt_manager_volume_event(BT_VOLUME_VALUE, data, size);
		break;
	case BTSRV_VCS_DOWN:
		bt_manager_volume_event(BT_VOLUME_VALUE, data, size);
		break;
	case BTSRV_VCS_VALUE:
		bt_manager_volume_event(BT_VOLUME_VALUE, data, size);
		break;
	case BTSRV_VCS_MUTE:
		bt_manager_volume_event(BT_VOLUME_MUTE, data, size);
		break;
	case BTSRV_VCS_UNMUTE:
		bt_manager_volume_event(BT_VOLUME_UNMUTE, data, size);
		break;
	case BTSRV_VCS_UNMUTE_UP:
		bt_manager_volume_event(BT_VOLUME_VALUE, data, size);
		break;
	case BTSRV_VCS_UNMUTE_DOWN:
		bt_manager_volume_event(BT_VOLUME_VALUE, data, size);
		break;
	case BTSRV_VCS_CLIENT_CONNECTED:
		bt_manager_volume_event(BT_VOLUME_CLIENT_CONNECTED, data, size);
		break;
	case BTSRV_VCS_CLIENT_UP:
		bt_manager_volume_event(BT_VOLUME_CLIENT_UP, data, size);
		break;
	case BTSRV_VCS_CLIENT_DOWN:
		bt_manager_volume_event(BT_VOLUME_CLIENT_DOWN, data, size);
		break;
	case BTSRV_VCS_CLIENT_VALUE:
		bt_manager_volume_event(BT_VOLUME_CLIENT_VALUE, data, size);
		break;
	case BTSRV_VCS_CLIENT_MUTE:
		bt_manager_volume_event(BT_VOLUME_CLIENT_MUTE, data, size);
		break;
	case BTSRV_VCS_CLIENT_UNMUTE:
		bt_manager_volume_event(BT_VOLUME_CLIENT_UNMUTE, data, size);
		break;
	case BTSRV_VCS_CLIENT_UNMUTE_UP:
		bt_manager_volume_event(BT_VOLUME_CLIENT_UNMUTE_UP, data, size);
		break;
	case BTSRV_VCS_CLIENT_UNMUTE_DOWN:
		bt_manager_volume_event(BT_VOLUME_CLIENT_UNMUTE_DOWN, data, size);
		break;

	/* MICS */
	case BTSRV_MICS_MUTE:
		bt_manager_mic_event(BT_MIC_MUTE, data, size);
		break;
	case BTSRV_MICS_UNMUTE:
		bt_manager_mic_event(BT_MIC_UNMUTE, data, size);
		break;
	case BTSRV_MICS_CLIENT_CONNECTED:
		bt_manager_mic_event(BT_MIC_CLIENT_CONNECTED, data, size);
	case BTSRV_MICS_GAIN_VALUE:
		bt_manager_mic_event(BT_MIC_GAIN_VALUE, data, size);
		break;
	case BTSRV_MICS_CLIENT_MUTE:
		bt_manager_mic_event(BT_MIC_CLIENT_MUTE, data, size);
		break;
	case BTSRV_MICS_CLIENT_UNMUTE:
		bt_manager_mic_event(BT_MIC_CLIENT_UNMUTE, data, size);
		break;
	case BTSRV_MICS_CLIENT_GAIN_VALUE:
		bt_manager_mic_event(BT_MIC_CLIENT_GAIN_VALUE, data, size);
		break;

	/* MCS */
	case BTSRV_MCS_SERVER_PLAY:
		bt_manager_media_event(BT_MEDIA_SERVER_PLAY, data, size);
		break;
	case BTSRV_MCS_SERVER_PAUSE:
		bt_manager_media_event(BT_MEDIA_SERVER_PAUSE, data, size);
		break;
	case BTSRV_MCS_SERVER_STOP:
		bt_manager_media_event(BT_MEDIA_SERVER_STOP, data, size);
		break;
	case BTSRV_MCS_SERVER_FAST_REWIND:
		bt_manager_media_event(BT_MEDIA_SERVER_FAST_REWIND, data, size);
		break;
	case BTSRV_MCS_SERVER_FAST_FORWARD:
		bt_manager_media_event(BT_MEDIA_SERVER_FAST_FORWARD, data, size);
		break;
	case BTSRV_MCS_SERVER_NEXT_TRACK:
		bt_manager_media_event(BT_MEDIA_SERVER_NEXT_TRACK, data, size);
		break;
	case BTSRV_MCS_SERVER_PREV_TRACK:
		bt_manager_media_event(BT_MEDIA_SERVER_PREV_TRACK, data, size);
		break;
	case BTSRV_MCS_CONNECTED:
		bt_manager_media_event(BT_MEDIA_CONNECTED, data, size);
		break;
	case BTSRV_MCS_PLAY:
		bt_manager_media_event(BT_MEDIA_PLAY, data, size);
		break;
	case BTSRV_MCS_PAUSE:
		bt_manager_media_event(BT_MEDIA_PAUSE, data, size);
		break;
	case BTSRV_MCS_STOP:
		bt_manager_media_event(BT_MEDIA_STOP, data, size);
		break;

	/*
	 * Broadcast Audio
	 */
	case BTSRV_BIS_CONNECTED:
		bt_manager_broadcast_event(BT_BIS_CONNECTED, data, size);
		break;
	case BTSRV_BIS_DISCONNECTED:
		bt_manager_broadcast_event(BT_BIS_DISCONNECTED, data, size);
		break;
	/* broadcast source */
	case BTSRV_BROAD_SRC_CONFIG:
		bt_manager_broadcast_event(BT_BROADCAST_SOURCE_CONFIG, data, size);
		break;
	case BTSRV_BROAD_SRC_ENABLE:
		bt_manager_broadcast_event(BT_BROADCAST_SOURCE_ENABLE, data, size);
		break;
	case BTSRV_BROAD_SRC_UPDATE:
		bt_manager_broadcast_event(BT_BROADCAST_SOURCE_UPDATE, data, size);
		break;
	case BTSRV_BROAD_SRC_DISABLE:
		bt_manager_broadcast_event(BT_BROADCAST_SOURCE_DISABLE, data, size);
		break;
	case BTSRV_BROAD_SRC_RELEASE:
		bt_manager_broadcast_event(BT_BROADCAST_SOURCE_RELEASE, data, size);
		break;
	case BTSSRV_BROAD_SRC_SENDDATA_EMPTY:
	    bt_manager_broadcast_event(BT_BROADCAST_SOURCE_SENDDATA_EMPTY, data, size);
		break;

	/* broadcast sink */
	case BTSRV_BROAD_SINK_CONFIG:
		bt_manager_broadcast_event(BT_BROADCAST_SINK_CONFIG, data, size);
		break;
	case BTSRV_BROAD_SINK_ENABLE:
		bt_manager_broadcast_event(BT_BROADCAST_SINK_ENABLE, data, size);
		break;
	case BTSRV_BROAD_SINK_UPDATE:
		bt_manager_broadcast_event(BT_BROADCAST_SINK_UPDATE, data, size);
		break;
	case BTSRV_BROAD_SINK_DISABLE:
		bt_manager_broadcast_event(BT_BROADCAST_SINK_DISABLE, data, size);
		break;
	case BTSRV_BROAD_SINK_RELEASE:
		bt_manager_broadcast_event(BT_BROADCAST_SINK_RELEASE, data, size);
		break;
	case BTSRV_BROAD_SINK_BASE_CONFIG:
		bt_manager_broadcast_event(BT_BROADCAST_SINK_BASE_CONFIG, data, size);
	    break;
	case BTSRV_BROAD_SINK_SYNC:
		bt_manager_broadcast_event(BT_BROADCAST_SINK_SYNC, data, size);
	    break;

	/* broadcast assistant */
	case BTSRV_BROAD_ASST_XXX:
		bt_manager_broadcast_event(BT_BROADCAST_ASSISTANT_xxx, data, size);
		break;

	/* scan delegator */
	case BTSRV_SCAN_DELE_XXX:
		bt_manager_broadcast_event(BT_SCAN_DELEGATOR_xxx, data, size);
		break;

	default:
		break;
	}

#ifdef CONFIG_BT_LEA_PTS_TEST
	if (bt_manager_config_lea_pts_test()) {
		bt_manger_lea_event_pts_process(event, data, size);
	}
#endif

}

int bt_manager_le_audio_exit(void)
{
	return btif_audio_exit();
}

int bt_manager_le_audio_keys_clear(void)
{
	return btif_audio_keys_clear();
}

int bt_manager_le_audio_start(void)
{
	return btif_audio_start();
}

/*
 * @return -EINPROGRESS in case of LE Audio is in stopping process
 *         and should wait for stopped event.
 *         0 in case of LE Audio is stopped successfully.
 *         else in case of LE Audio is failed to stop.
 */
int bt_manager_le_audio_stop(void)
{
#if 0
	SYS_LOG_INF("");

	btif_audio_stop();

	/* wait forever */
	while (!btif_audio_stopped()) {
		os_sleep(10);
	}

	SYS_LOG_INF("done");

	return 0;
#else
	return btif_audio_stop();
#endif
}

int bt_manager_lea_services_enable(void)
{
	return btif_audio_services_enable();
}

int bt_manager_lea_services_disable(void)
{
	return btif_audio_services_disable();
}

int bt_manager_le_audio_pause(void)
{
	return btif_audio_pause();
}

int bt_manager_le_audio_resume(void)
{
	return btif_audio_resume();
}

int bt_manager_le_audio_server_cap_init(struct bt_audio_capabilities *caps)
{
	return btif_audio_server_cap_init(caps);
}

int bt_manager_le_audio_stream_config_codec(struct bt_audio_codec *codec)
{
	return btif_audio_stream_config_codec(codec);
}

int bt_manager_le_audio_stream_prefer_qos(struct bt_audio_preferred_qos *qos)
{
	return btif_audio_stream_prefer_qos(qos);
}

int bt_manager_le_audio_stream_config_qos(struct bt_audio_qoss *qoss)
{
	return btif_audio_stream_config_qos(qoss);
}

int bt_manager_le_audio_stream_bind_qos(struct bt_audio_qoss *qoss, uint8_t index)
{
	return btif_audio_stream_bind_qos(qoss, index);
}

int bt_manager_le_audio_stream_sync_forward(struct bt_audio_chan **chans,
				uint8_t num_chans)
{
	return btif_audio_stream_sync_forward(chans, num_chans);
}

int bt_manager_le_audio_stream_enable(struct bt_audio_chan **chans,
				uint8_t num_chans, uint32_t contexts)
{
	return btif_audio_stream_enable(chans, num_chans, contexts);
}

int bt_manager_le_audio_stream_disable(struct bt_audio_chan **chans,
				uint8_t num_chans)
{
	return btif_audio_stream_disble(chans, num_chans);
}

io_stream_t bt_manager_le_audio_source_stream_create(struct bt_audio_chan **chans,
				uint8_t num_chans, uint32_t locations)
{
	return btif_audio_source_stream_create(chans, num_chans, locations);
}

int bt_manager_le_audio_source_stream_set(struct bt_audio_chan **chans,
				uint8_t num_chans, io_stream_t stream,
				uint32_t locations)
{
	return btif_audio_source_stream_set(chans, num_chans, stream, locations);
}

int bt_manager_le_audio_sink_stream_set(struct bt_audio_chan *chan,
				io_stream_t stream)
{
	return btif_audio_sink_stream_set(chan, stream);
}

int bt_manager_le_audio_sink_stream_start(struct bt_audio_chan **chans,
				uint8_t num_chans)
{
	return btif_audio_sink_stream_start(chans, num_chans);
}

int bt_manager_le_audio_sink_stream_stop(struct bt_audio_chan **chans,
				uint8_t num_chans)
{
	return btif_audio_sink_stream_stop(chans, num_chans);
}

int bt_manager_le_audio_stream_update(struct bt_audio_chan **chans,
				uint8_t num_chans, uint32_t contexts)
{
	return btif_audio_stream_update(chans, num_chans, contexts);
}

int bt_manager_le_audio_stream_release(struct bt_audio_chan **chans,
				uint8_t num_chans)
{
	return btif_audio_stream_release(chans, num_chans);
}

/* released channel to idle state after release, for slave only */
int bt_manager_le_audio_stream_released(struct bt_audio_chan *chan)
{
	return btif_audio_stream_released(chan);
}

int bt_manager_le_audio_stream_disconnect(struct bt_audio_chan **chans,
				uint8_t num_chans)
{
	return btif_audio_stream_disconnect(chans, num_chans);
}

void *bt_manager_le_audio_stream_bandwidth_alloc(struct bt_audio_qoss *qoss)
{
	return btif_audio_stream_bandwidth_alloc(qoss);
}

int bt_manager_le_audio_stream_bandwidth_free(void *cig_handle)
{
	return btif_audio_stream_bandwidth_free(cig_handle);
}

int bt_manager_le_audio_stream_cb_register(struct bt_audio_stream_cb *cb)
{
	return btif_audio_stream_cb_register(cb);
}

int bt_manager_le_audio_stream_set_tws_sync_cb_offset(struct bt_audio_chan *chan,int32_t offset_from_syncdelay)
{
	return btif_audio_stream_set_tws_sync_cb_offset(chan,offset_from_syncdelay);
}

int bt_manager_le_volume_up(uint16_t handle)
{
	return btif_audio_volume_up(handle);
}

int bt_manager_le_volume_down(uint16_t handle)
{
	return btif_audio_volume_down(handle);
}

int bt_manager_le_volume_set(uint16_t handle, uint8_t value)
{
	return btif_audio_volume_set(handle, value);
}

int bt_manager_le_volume_mute(uint16_t handle)
{
	return btif_audio_volume_mute(handle);
}

int bt_manager_le_volume_unmute(uint16_t handle)
{
	return btif_audio_volume_unmute(handle);
}

int bt_manager_le_volume_unmute_up(uint16_t handle)
{
	return btif_audio_volume_unmute_up(handle);
}

int bt_manager_le_volume_unmute_down(uint16_t handle)
{
	return btif_audio_volume_unmute_down(handle);
}

int bt_manager_le_mic_mute(uint16_t handle)
{
	return btif_audio_mic_mute(handle);
}

int bt_manager_le_mic_unmute(uint16_t handle)
{
	return btif_audio_mic_unmute(handle);
}

int bt_manager_le_mic_mute_disable(uint16_t handle)
{
	return btif_audio_mic_mute_disable(handle);
}

int bt_manager_le_mic_gain_set(uint16_t handle, uint8_t gain)
{
	return btif_audio_mic_gain_set(handle, gain);
}

int bt_manager_le_media_mcs_state_get(uint16_t handle)
{
	return btif_mcs_media_state_get(handle);
}

int bt_manager_le_media_play(uint16_t handle)
{
	return btif_audio_media_play(handle);
}

int bt_manager_le_media_pause(uint16_t handle)
{
	return btif_audio_media_pause(handle);
}

int bt_manager_le_media_fast_rewind(uint16_t handle)
{
	return btif_audio_media_fast_rewind(handle);
}

int bt_manager_le_media_fast_forward(uint16_t handle)
{
	return btif_audio_media_fast_forward(handle);
}

int bt_manager_le_media_stop(uint16_t handle)
{
	return btif_audio_media_stop(handle);
}

int bt_manager_le_media_play_previous(uint16_t handle)
{
	return btif_audio_media_play_prev(handle);
}

int bt_manager_le_media_play_next(uint16_t handle)
{
	return btif_audio_media_play_next(handle);
}

int bt_manager_audio_le_conn_max_len(uint16_t handle)
{
	if (!handle) {
		return -EINVAL;
	}

	return btif_audio_conn_max_len(handle);
}

int bt_manager_audio_le_vnd_register_rx_cb(uint16_t handle,
				bt_audio_vnd_rx_cb rx_cb)
{
	if (!handle || !rx_cb) {
		return -EINVAL;
	}

	return btif_audio_vnd_register_rx_cb(handle, rx_cb);
}

int bt_manager_audio_le_vnd_send(uint16_t handle, uint8_t *buf, uint16_t len)
{
	if (!handle || !buf || !len) {
		return -EINVAL;
	}

	return btif_audio_vnd_send(handle, buf, len);
}

int bt_manager_audio_le_adv_cb_register(bt_audio_adv_cb start,
				bt_audio_adv_cb stop)
{
	return btif_audio_adv_cb_register(start, stop);
}

int bt_manager_audio_le_start_adv(void)
{
	return btif_audio_start_adv();
}

int bt_manager_audio_le_stop_adv(void)
{
	return btif_audio_stop_adv();
}

int bt_manager_audio_le_relay_control(bool enable)
{
	return btif_audio_relay_control(enable);
}

int bt_manager_le_call_originate(uint16_t handle, uint8_t *remote_uri)
{
	return btif_audio_call_originate(handle, remote_uri);
}

int bt_manager_le_call_accept(struct bt_audio_call *call)
{
	return btif_audio_call_accept(call);
}

int bt_manager_le_call_hold(struct bt_audio_call *call)
{
	return btif_audio_call_hold(call);
}

int bt_manager_le_call_retrieve(struct bt_audio_call *call)
{
	return btif_audio_call_retrieve(call);
}

int bt_manager_le_call_terminate(struct bt_audio_call *call)
{
	return btif_audio_call_terminate(call);
}
