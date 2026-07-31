/*
 * Copyright (c) 2016 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt service core interface
 */
#define SYS_LOG_DOMAIN "tws_utils"
#include "btsrv_os_common.h"
#include "btsrv_inner.h"

bd_address_t *GET_CONN_BT_ADDR(struct bt_conn *conn)
{
	struct bt_conn_info info;

	if (conn != NULL) {
		if (hostif_bt_conn_get_info(conn, &info) >= 0) {
			return (bd_address_t *)info.br.dst;
		}
	}

	return NULL;
}


bool btsrv_is_snoop_conn(struct bt_conn *conn)
{
	uint8_t type;

	if (conn == NULL) {
		return false;
	}

	type = hostif_bt_conn_get_type(conn);
	if (type == BT_CONN_TYPE_BR_SNOOP || type == BT_CONN_TYPE_SCO_SNOOP) {
		return true;
	} else {
		return false;
	}
}


uint32_t bt_rand32_get(void)
{
	uint32_t rand32, i;

	rand32 = os_uptime_get_32();
	if (btsrv_info) {
		for (i = 0; i < 6; i++)
			rand32 ^= (uint32_t)btsrv_info->device_addr[i];
	}

	return rand32;
}

int btsrv_set_negative_prio(void)
{
	int prio = 0;

	if(!k_is_in_isr()){
		prio = os_thread_priority_get(os_current_get());
		if (prio >= 0) {
			os_thread_priority_set(os_current_get(), -1);
		}
	}

	return prio;
}

void btsrv_revert_prio(int prio)
{
	if(!k_is_in_isr()){
		if (prio >= 0) {
			os_thread_priority_set(os_current_get(), prio);
		}
	}
}

int btsrv_property_set(const char *key, char *value, int value_len)
{
	int ret = -EIO;

#ifdef CONFIG_PROPERTY
	ret = property_set(key, value, value_len);
	btsrv_event_notify(MSG_BTSRV_BASE, MSG_BTSRV_REQ_FLUSH_NVRAM, (void *)key);
#endif
	return ret;
}

int btsrv_property_set_factory(const char *key, char *value, int value_len)
{
	int ret = -EIO;

#ifdef CONFIG_PROPERTY
	ret = property_set_factory(key, value, value_len);
	//btsrv_event_notify(MSG_BTSRV_BASE, MSG_BTSRV_REQ_FLUSH_NVRAM, (void *)key);
#endif
	return ret;
}


int btsrv_property_get(const char *key, char *value, int value_len)
{
	int ret = -EIO;

#ifdef CONFIG_PROPERTY
	ret = property_get(key, value, value_len);
#endif
	return ret;
}

//void ctrl_adjust_link_time(struct bt_conn *base_conn, int16_t adjust_val)
//{
//	hostif_bt_vs_adjust_link_time(base_conn, adjust_val);
//}
