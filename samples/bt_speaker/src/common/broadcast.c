/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define SYS_LOG_DOMAIN "broadcast"

#include <logging/sys_log.h>
#include <btservice_api.h>
#ifdef CONFIG_PROPERTY
#include <property_manager.h>
#endif
#include <audio_system.h>
#include <volume_manager.h>
#include "broadcast.h"
#include "app_common.h"
#include <app_tws.h>

#define DEFAULT_CONTEXTS (BT_AUDIO_CONTEXT_UNSPECIFIED | BT_AUDIO_CONTEXT_MEDIA)

#define LOCAL_NAME	"bis_local"
#define LOCAL_NAME_LEN		(sizeof(LOCAL_NAME) - 1)

#define BROADCAST_NAME	"bis_broadcast"
#define BROADCAST_NAME_LEN		(sizeof(BROADCAST_NAME) - 1)

static uint8_t local_name[33];
static uint8_t broadcast_name[33];
static uint8_t tws_broadcast_name[33] = {0};

void broadcast_set_tws_broadcast_name(uint8_t *name)
{
	int len;

	if (name) {
		len = strlen(name);
		if (len < 2) {
			return;
		}
		if (len + 1 > sizeof(tws_broadcast_name)) {
			len = sizeof(tws_broadcast_name) - 1;
		}
		memcpy(tws_broadcast_name, name, len);
		tws_broadcast_name[len] = '\0';
		SYS_LOG_INF("%s", tws_broadcast_name);
	} else {
		SYS_LOG_INF("NULL");
		memset(tws_broadcast_name, 0, sizeof(tws_broadcast_name));
	}
}

char *broadcast_get_local_name(void)
{
	int ret;

#ifdef CONFIG_PROPERTY
	ret = property_get(CFG_BT_LOCAL_NAME, local_name, sizeof(local_name) - 1);
	if (ret <= 0) {
		SYS_LOG_WRN("failed to get local name\n");
		memcpy(local_name, LOCAL_NAME, LOCAL_NAME_LEN);
	}
#endif
	SYS_LOG_INF("%s", local_name);
	return local_name;
}

char *broadcast_get_broadcast_name(void)
{
	int ret;

	if (strlen(tws_broadcast_name) > 0 && 
		strlen(tws_broadcast_name) < sizeof(tws_broadcast_name)) {
		SYS_LOG_INF("tws: %s", tws_broadcast_name);
		return tws_broadcast_name;
	}

#ifdef CONFIG_PROPERTY
	ret = property_get(CFG_BT_BROADCAST_NAME, broadcast_name, sizeof(broadcast_name) - 1);
	if (ret <= 0) {
		SYS_LOG_WRN("failed to get broadcast name\n");
		memcpy(broadcast_name, BROADCAST_NAME, BROADCAST_NAME_LEN);
	}
#endif
	SYS_LOG_INF("%s", broadcast_name);
	return broadcast_name;
}

int broadcast_get_broadcast_id(void)
{
	int v;

	v = property_get_int("BROADCAST_ID", INVALID_BROADCAST_ID);

	SYS_LOG_INF("0x%x\n", v);
	return v;
}

int broadcast_get_bis_link_delay(struct bt_broadcast_qos *qos)
{
	u32_t offset;

	if(NULL == qos) {
		SYS_LOG_ERR("NULL qos");
		return 0;
	}

	offset = qos->delay - EXTERNAL_DSP_DELAY;

	if (qos->interval == 7500) {	//lc3 codec delay
		offset += 4000;
	} else {
		offset += 2500;
	}

	offset += qos->processing;
	offset += 10000;	//TODO:use sync delay
	offset += (BROADCAST_ISO_INTERVAL * 1250);
	offset /= 1000;

	SYS_LOG_INF("%dms\n", offset);
	return offset;
}

int broadcast_get_tws_sync_offset(struct bt_broadcast_qos *qos)
{
	int offset;

	if(NULL == qos) {
		SYS_LOG_ERR("NULL qos");
		return 0;
	}

	offset = qos->delay - EXTERNAL_DSP_DELAY;
	if (qos->interval == 7500) {	// lc3 codec delay
		offset += 4000;
	} else {
		offset += 2500;
	}
	// offset -= 1000;

	SYS_LOG_INF("%dus\n", offset);
	return offset;
}

struct bt_broadcast_source_create_param * broadcast_init_source_param(void)
{
	struct bt_broadcast_source_create_param* param = NULL;
	struct bt_broadcast_source_big_param* big_param = NULL;
	struct bt_le_per_adv_param* per_adv_param = NULL;
#if (BROADCAST_NUM_BIS == 2)
	struct bt_broadcast_subgroup_2* subgroup = NULL;
#else
	struct bt_broadcast_subgroup_1* subgroup = NULL;
#endif
	uint16_t appearance = 0x885;	/* Audio Source + Broadcasting Device */

	param = app_mem_malloc(sizeof(struct bt_broadcast_source_create_param));
	if(NULL == param) {
		SYS_LOG_ERR("malloc failed");
		return NULL;
	}

	big_param = app_mem_malloc(sizeof(struct bt_broadcast_source_big_param));
	if(NULL == big_param) {
		app_mem_free(param);
		SYS_LOG_ERR("malloc failed");
		return NULL;
	}

	per_adv_param = app_mem_malloc(sizeof(struct bt_le_per_adv_param));
	if(NULL == per_adv_param ) {
		app_mem_free(big_param);
		app_mem_free(param);
		SYS_LOG_ERR("malloc failed");
		return NULL;
	}

	subgroup = app_mem_malloc(sizeof(struct bt_broadcast_subgroup_2));
	if(NULL == subgroup) {
		app_mem_free(big_param);
		app_mem_free(per_adv_param);
		app_mem_free(param);
		SYS_LOG_ERR("malloc failed");
		return NULL;
	}

	memset(param, 0, sizeof(struct bt_broadcast_source_create_param));
	memset(big_param, 0, sizeof(struct bt_broadcast_source_big_param));
	memset(per_adv_param, 0, sizeof(struct bt_le_per_adv_param));
	memset(subgroup, 0, sizeof(struct bt_broadcast_subgroup_2));

#if (BROADCAST_NUM_BIS == 2)
	if(app_tws_status_get_enable()) {
		subgroup->num_bis = 1;
	} else {
		subgroup->num_bis = 2;
	}
#else
	subgroup.num_bis = 1;
#endif

	subgroup->format = BT_AUDIO_CODEC_LC3;
	subgroup->frame_duration = BROADCAST_DURATION;
	subgroup->blocks = 1;
	subgroup->sample_rate = 48;
	subgroup->octets = BROADCAST_OCTETS;
	subgroup->language = 0;	//BT_LANGUAGE_UNKNOWN
	subgroup->contexts = DEFAULT_CONTEXTS;


#if (BROADCAST_NUM_BIS == 2)
	subgroup->bis[0].locations = BT_AUDIO_LOCATIONS_FL;
	subgroup->bis[1].locations = BT_AUDIO_LOCATIONS_FR;
	if(app_tws_status_get_enable()) {
		subgroup->locations = BT_AUDIO_LOCATIONS_FR;
	}
#else
#if (BROADCAST_CH == 2)
	subgroup->locations = BT_AUDIO_LOCATIONS_FL | BT_AUDIO_LOCATIONS_FR;
#else
	subgroup->locations = BT_AUDIO_LOCATIONS_FL;
#endif
#endif

	big_param->iso_interval = BROADCAST_ISO_INTERVAL;
	big_param->max_pdu = BROADCAST_PDU;
	big_param->nse = BROADCAST_NSE;
	big_param->bn = BROADCAST_BN;
	big_param->irc = BROADCAST_IRC;
	big_param->pto = BROADCAST_PTO;

	if(subgroup->num_bis == 1){
		big_param->irc = BROADCAST_1_IRC;
		big_param->nse = BROADCAST_1_IRC * BROADCAST_BN;
	}

	/* 100ms */
	per_adv_param->interval_min = PA_INTERVAL;
	per_adv_param->interval_max = PA_INTERVAL;

#if ENABLE_ENCRYPTION
	param->encryption = true;
	param->broadcast_code[0] = 0x55;
	param->broadcast_code[1] = 0xaa;
#endif

	param->broadcast_id = broadcast_get_broadcast_id();
	if (param->broadcast_id == INVALID_BROADCAST_ID) {
		SYS_LOG_ERR("wrong broadcast id\n");
		app_mem_free(big_param);
		app_mem_free(per_adv_param);
		app_mem_free(subgroup);
		app_mem_free(param);
		return NULL;
	}

	param->local_name = broadcast_get_local_name();
	param->broadcast_name = broadcast_get_broadcast_name();
	param->appearance = appearance;
	param->num_subgroups = 1;

	param->subgroup = (struct bt_broadcast_subgroup *)subgroup;
	param->big_param = big_param;
	param->per_adv_param = per_adv_param;

	return param;
}

void broadcast_free_source_param(struct bt_broadcast_source_create_param *param)
{
	if(NULL == param) {
		SYS_LOG_ERR("NULL param");
		return;
	}

	if(NULL != param->subgroup) {
		app_mem_free(param->subgroup);
	}

	if(NULL != param->per_adv_param) {
		app_mem_free(param->per_adv_param);
	}
	if(NULL != param->big_param) {
		app_mem_free(param->big_param);
	}

	app_mem_free(param);
}

#if ENABLE_PADV_APP

static os_delayed_work per_adv_dwork;
static bool per_adv_dwork_inited;
static u32_t padv_handle = 0;
static u8_t padv_buf[32];
static u8_t padv_stream_type = 0;

u8_t padv_volume_map(u8_t vol, u8_t to_adv)
{
	u8_t map;
	int i;

	static const u8_t table[MAX_AUDIO_VOL_LEVEL + 1] = {
		0,
		6,	12,	18,	25,
		31,	37,	43,	50,
		56,	62,	68, 75,
		81,	87, 93,	100,
	};

	if (to_adv != 0) {
		//local vol to adv vol
		if (vol > MAX_AUDIO_VOL_LEVEL)
		{
			SYS_LOG_ERR("vol max");
			vol = MAX_AUDIO_VOL_LEVEL;
		}
		map = table[vol];
	} else {
		//adv vol to local vol
		if (vol > 100) {
			SYS_LOG_ERR("%d", vol);
			vol = 100;
		}
		for (i = 0; i < MAX_AUDIO_VOL_LEVEL + 1; i++) {
			if (vol <= table[i]) {
				map = i;
				break;
			}
		}
		if (i == MAX_AUDIO_VOL_LEVEL + 1) {
			map = MAX_AUDIO_VOL_LEVEL;
		}
	}

	//SYS_LOG_INF("%d->%d, %d", vol, map, to_adv);

	return map;
}

static void per_adv_handler(struct k_work *work)
{
	u8_t vol;
	int ret_volume;

	ret_volume = system_volume_get(padv_stream_type);
	if (ret_volume >= 0) {
		vol = padv_volume_map(ret_volume, 1);
		padv_tx_data(vol);
	}

	os_delayed_work_submit(&per_adv_dwork, 1000);
}

/*
HM party series private data packet format definition, v1.8.1
UUID(2B) + PRIVATE(LTV+LTV+..+LTV)
LTV: Length(1B) + Type(1B) + Value(nB, n<=29)
Length: the length of Type + Value.
Type:
	0x01 - Lighting effects - 21 bytes data
	0x02 - Volume - 1 byte data [0-100]
	0x03 -
	0x04 -
	0x05 -
	0xF0 -
*/

int padv_tx_init(u32_t handle, u8_t stream_type)
{
	SYS_LOG_INF("0x%x", handle);
	padv_handle = handle;
	padv_stream_type = stream_type;

	if (!per_adv_dwork_inited) {
		per_adv_dwork_inited = true;
		os_delayed_work_init(&per_adv_dwork, per_adv_handler);
		os_delayed_work_submit(&per_adv_dwork, 1000);
		int ret_volume = system_volume_get(stream_type);
		if (ret_volume >= 0) {
			padv_tx_data(padv_volume_map(ret_volume, 1));
		}
	}
	return 0;
}

int padv_tx_deinit(void)
{
	SYS_LOG_INF("\n");
	padv_handle = 0;
	padv_stream_type = 0;
	if (per_adv_dwork_inited) {
		k_delayed_work_cancel_sync(&per_adv_dwork, K_FOREVER);
		per_adv_dwork_inited = false;
	}
	return 0;
}

int padv_tx_data(u8_t vol100)
{
	//Fixme: load actual lighting effect data
	static u8_t light = 0;

	u8_t type = 0; //Fixme: ? BT_DATA_SVC_DATA16
	u8_t offset = 0;

	__ASSERT(sizeof(padv_buf) >= 2+23+3, "buffer too small");

	if(0 == padv_handle) {
		return -1;
	}

	SYS_LOG_INF("light %d, vol %d", light, vol100);
	memset(padv_buf, 0, sizeof(padv_buf));

	//uuid
	padv_buf[offset++] = SERIVCE_UUID & 0xFF;
	padv_buf[offset++] = SERIVCE_UUID >> 8;

	//light effect
	padv_buf[offset++] = 1 + 21;
	padv_buf[offset++] = PAWR_DT_LIGHT;
	padv_buf[offset] = light++;
	offset+=21;

	//volume
	padv_buf[offset++] = 2;
	padv_buf[offset++] = PAWR_DT_VOLUME;
	padv_buf[offset++] = vol100;

	bt_manager_broadcast_source_vnd_per_send(padv_handle,
						 padv_buf,
						 offset, type);
	return 0;
}


#endif
