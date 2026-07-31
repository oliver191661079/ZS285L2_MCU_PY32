/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt ble manager.
 */
#define SYS_LOG_DOMAIN "bt manager"

#include <os_common_api.h>

#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <msg_manager.h>
#include <mem_manager.h>
#include <acts_bluetooth/host_interface.h>
#include <bt_manager.h>
#include "bt_manager_inner.h"
#include <sys_event.h>
#include <hex_str.h>

//#define GATT_OVER_BR_QOS_SETUP

extern void stream_ble_connect_cb(struct bt_conn *conn, uint8_t conn_type, uint8_t *mac, uint8_t connected);

OS_SEM_DEFINE(ind_sem, 1, 1);
static struct bt_gatt_indicate_params ind_params __IN_BT_SECTION;

static int notify_data(struct bt_conn *conn, struct bt_gatt_attr *attr, uint8_t *data, uint16_t len)
{
	int ret;

	ret = hostif_bt_gatt_notify(conn, attr, data, len);
	if (ret < 0) {
		return ret;
	} else {
		return (int)len;
	}
}

static void indicate_cb(struct bt_conn *conn, struct bt_gatt_indicate_params *attr, uint8_t err)
{
	os_sem_give(&ind_sem);
}

static int indicate_data(struct bt_conn *conn, struct bt_gatt_attr *attr, uint8_t *data, uint16_t len)
{
	int ret;

	if (os_sem_take(&ind_sem, OS_NO_WAIT) < 0) {
		return -EBUSY;
	}

	ind_params.attr = attr;
	ind_params.func = indicate_cb;
	ind_params.len = len;
	ind_params.data = data;

	ret = hostif_bt_gatt_indicate(conn, &ind_params);
	if (ret < 0) {
		return ret;
	} else {
		return (int)len;
	}
}

uint16_t bt_manager_get_gatt_over_br_mtu(struct bt_conn *conn)
{
	return (conn) ? hostif_bt_gatt_over_br_get_mtu(conn) : 0;
}

int bt_manager_gatt_over_br_send_data(struct bt_conn *conn, struct bt_gatt_attr *chrc_attr,
							struct bt_gatt_attr *des_attr, uint8_t *data, uint16_t len)
{
	struct bt_gatt_chrc *chrc = (struct bt_gatt_chrc *)(chrc_attr->user_data);

	if (!conn) {
		return -EIO;
	}

	if (len > (bt_manager_get_gatt_over_br_mtu(conn) - 3)) {
		return -EFBIG;
	}

	if (chrc->properties & BT_GATT_CHRC_NOTIFY) {
		return notify_data(conn, des_attr, data, len);
	} else if (chrc->properties & BT_GATT_CHRC_INDICATE) {
		return indicate_data(conn, des_attr, data, len);
	}
	return -EIO;
}

#ifdef GATT_OVER_BR_QOS_SETUP
static int bt_manager_gatt_over_br_setup_qos(struct bt_conn *conn)
{
    struct bt_update_qos_param qos;

    if(!conn){
        return -EIO;
    }
    memset(&qos,0,sizeof(struct bt_update_qos_param));
    qos.conn = conn;
    qos.service_type = 0x01;
    qos.token_rate = 30000;
    qos.latency = 7500;
    return btif_br_update_qos(&qos);
}
#endif

static int bt_manager_gatt_over_br_user_cb(struct bt_conn *conn, uint8_t connected)
{
	struct bt_conn_info info;

	if ((hostif_bt_conn_get_info(conn, &info) < 0) ||
	    (info.type != BT_CONN_TYPE_BR)) {
		return -1;
	}

    SYS_LOG_INF("connected:%d conn:%p", connected,conn);

	if(connected) {
		stream_ble_connect_cb(conn, BT_CONN_TYPE_BR, (uint8_t *)(info.br.dst->val), true);
#ifdef GATT_OVER_BR_QOS_SETUP
        bt_manager_gatt_over_br_setup_qos(conn);
#endif
	}else {
		stream_ble_connect_cb(conn, BT_CONN_TYPE_BR, (uint8_t *)(info.br.dst->val), false);
		os_sem_give(&ind_sem);
	}
	return 0;
}
BT_GATT_OVER_BR_USER_INFO_DEFINE(gatt_over_br_user_info, bt_manager_gatt_over_br_user_cb);