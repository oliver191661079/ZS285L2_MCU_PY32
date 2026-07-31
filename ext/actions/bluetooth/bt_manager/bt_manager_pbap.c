/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt manager PBAP profile.
 */
#define SYS_LOG_DOMAIN "bt manager"

#include <os_common_api.h>

#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stream.h>
#include <sys_event.h>
#include <bt_manager.h>
#include "bt_manager_inner.h"
#include "btservice_api.h"

#define BTMGR_MAX_PBAP_NUM	2
#define MGR_PBAP_INDEX_TO_APPID(x)		((x)|0x80)
#define MGR_PBAP_APPID_TO_INDEX(x)		((x)&(~0x80))

struct btmgr_pbap_info {
	uint8_t app_id;
	struct btmgr_pbap_cb *cb;
};

static struct btmgr_pbap_info mgr_pbap_info[BTMGR_MAX_PBAP_NUM];

static void *btmgr_pbap_find_free_info(void)
{
	int i;

	for (i = 0; i < BTMGR_MAX_PBAP_NUM; i++) {
		if (mgr_pbap_info[i].app_id == 0) {
			mgr_pbap_info[i].app_id = MGR_PBAP_INDEX_TO_APPID(i);
			return &mgr_pbap_info[i];
		}
	}

	return NULL;
}

static void btmgr_pbap_free_info(struct btmgr_pbap_info *info)
{
	memset(info, 0, sizeof(struct btmgr_pbap_info));
}

static void *btmgr_pbap_find_info_by_app_id(uint8_t app_id)
{
	int i;

	for (i = 0; i < BTMGR_MAX_PBAP_NUM; i++) {
		if (mgr_pbap_info[i].app_id == app_id) {
			return &mgr_pbap_info[i];
		}
	}

	return NULL;
}

static void btmgr_pbap_callback(btsrv_pbap_event_e event, uint8_t app_id, void *data, uint8_t size)
{
	struct btmgr_pbap_info *info = btmgr_pbap_find_info_by_app_id(app_id);

	if (!info) {
		return;
	}

	switch (event) {
	case BTSRV_PBAP_CONNECT_FAILED:
		if (info->cb->connect_failed) {
			info->cb->connect_failed(app_id);
		}
		btmgr_pbap_free_info(info);
		break;
	case BTSRV_PBAP_CONNECTED:
		if (info->cb->connected) {
			info->cb->connected(app_id);
		}
		break;
	case BTSRV_PBAP_DISCONNECTED:
		if (info->cb->disconnected) {
			info->cb->disconnected(app_id);
		}
		btmgr_pbap_free_info(info);
		break;
	case BTSRV_PBAP_VCARD_RESULT:
		if (info->cb->result) {
			info->cb->result(app_id, data, size);
		}
		break;
	}
}

uint8_t btmgr_pbap_get_phonebook(bd_address_t *bd, char *path, struct btmgr_pbap_cb *cb)
{
	int ret;
	struct btmgr_pbap_info *info;
	struct bt_get_pb_param param;

	if (!path || !cb) {
		return 0;
	}

	info = btmgr_pbap_find_free_info();
	if (!info) {
		return 0;
	}

	info->cb = cb;

	memcpy(&param.bd, bd, sizeof(bd_address_t));
	param.app_id = info->app_id;
	param.pb_path = path;
	param.cb = &btmgr_pbap_callback;
	ret = btif_pbap_get_phonebook(&param);
	if (ret) {
		btmgr_pbap_free_info(info);
		return 0;
	}

	return info->app_id;
}

int btmgr_pbap_abort_get(uint8_t app_id)
{
	struct btmgr_pbap_info *info = btmgr_pbap_find_info_by_app_id(app_id);

	if (!info) {
		return -EIO;
	}

	return btif_pbap_abort_get(info->app_id);
}
