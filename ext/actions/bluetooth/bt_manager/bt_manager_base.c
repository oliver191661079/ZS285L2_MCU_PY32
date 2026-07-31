/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt manager base function, all this function just for bt manager, not export.
 */
//#define SYS_LOG_NO_NEWLINE
#define SYS_LOG_DOMAIN "bt manager base"

#include <os_common_api.h>

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
#include <acts_bluetooth/host_interface.h>
#include <property_manager.h>

void btmgr_poff_check_phone_disconnected(void)
{
	struct bt_manager_context_t *bt_manager = bt_manager_get_context();

	os_mutex_lock(&bt_manager->poweroff_mutex, OS_FOREVER);

	if ((bt_manager->poweroff_state == POFF_STATE_WAIT_DISCONNECT_PHONE) &&
		(bt_manager_get_connected_dev_num() == 0)) {
		btmgr_poff_set_state_work(POFF_STATE_PHONE_DISCONNECTED, POWEROFF_WAIT_MIN_TIME, 0);
	}

	os_mutex_unlock(&bt_manager->poweroff_mutex);
}

void btmgr_poff_set_state_work(uint8_t state, uint32_t time, uint8_t timeout_flag)
{
	struct bt_manager_context_t *bt_manager = bt_manager_get_context();

	SYS_LOG_INF("poff_set %d %d %d", state, time, timeout_flag);
	bt_manager->poweroff_state = state;
	bt_manager->poweroff_wait_timeout = timeout_flag ? 1 : 0;
	os_delayed_work_submit(&bt_manager->poweroff_proc_work, time);
}

static void btmgr_poff_none_proc(struct bt_manager_context_t *bt_manager)
{
	SYS_LOG_INF("Should not happend!");
}

static void btmgr_poff_start_proc(struct bt_manager_context_t *bt_manager)
{
	if (bt_manager->local_req_poweroff ||
		(bt_manager->remote_req_poweroff && (bt_manager->single_poweroff == 0))) {
		/* Disable discoverable/connectable, stop pair, stop reconnect. */
        btif_base_set_power_off();
		bt_manager_set_user_visual(true, false, false, 0);
		bt_manager_tws_end_pair_search();
        os_delayed_work_cancel(&bt_manager->auto_reconnect_work);
		os_delayed_work_cancel(&bt_manager->auto_reconnect_second_dev_work);
		btif_br_auto_reconnect_stop(BTSRV_STOP_AUTO_RECONNECT_ALL);

		//close lea adv
		bt_manager_lea_policy_disable(0);
	}

	btmgr_poff_set_state_work(POFF_STATE_TWS_PROC, POWEROFF_WAIT_MIN_TIME, 0);
}

static void btmgr_poff_tws_proc(struct bt_manager_context_t *bt_manager)
{
	/* Tws module process snoop link disconnect and tws disconnect  */
	btmgr_tws_poff_state_proc();
}

void btmgr_poff_disconnect_device(int disconnect_mode)
{
	btif_autoconn_info_active_clear_check();
	if (bt_manager_audio_is_lea_open()) {
		bt_manager_audio_add_all_dev_to_disconnect_info();
	}
	btif_br_set_autoconn_info_need_update(0);
	bt_manager_audio_set_disconnect_info_need_update(0);
	btif_br_disconnect_device(disconnect_mode);
	bt_manager_lea_policy_event_notify(LEA_POLICY_EVENT_BT_OFF, NULL, 0);
}

static void btmgr_poff_disconnect_phone_proc(struct bt_manager_context_t *bt_manager)
{
	if (bt_manager->local_req_poweroff ||
		(bt_manager->remote_req_poweroff && (bt_manager->single_poweroff == 0))) {
		if (bt_manager_get_connected_dev_num()) {
			btmgr_poff_disconnect_device(BTSRV_DISCONNECT_PHONE_MODE);
			btmgr_poff_set_state_work(POFF_STATE_WAIT_DISCONNECT_PHONE, POWEROFF_WAIT_TIMEOUT, 1);
		} else {
			btmgr_poff_set_state_work(POFF_STATE_FINISH, POWEROFF_WAIT_MIN_TIME, 0);
		}
	} else {
		btmgr_poff_set_state_work(POFF_STATE_FINISH, POWEROFF_WAIT_MIN_TIME, 0);
	}
}

static void btmgr_poff_wait_timeout_proc(struct bt_manager_context_t *bt_manager)
{
	SYS_LOG_INF("State %d wait timeout!", bt_manager->poweroff_state);

	if (bt_manager->poweroff_state == POFF_STATE_WAIT_DISCONNECT_PHONE) {
		if (bt_manager->local_req_poweroff) {
			/* Local poweroff, not set timeout flag, bt manager deinit will do disconnect phone again */
			btmgr_poff_set_state_work(POFF_STATE_FINISH, POWEROFF_WAIT_MIN_TIME, 0);
		} else {
			btmgr_poff_set_state_work(POFF_STATE_FINISH, POWEROFF_WAIT_MIN_TIME, 1);
		}
	}
}

static void btmgr_poff_phone_disconnected_proc(struct bt_manager_context_t *bt_manager)
{
	btmgr_poff_set_state_work(POFF_STATE_FINISH, POWEROFF_WAIT_MIN_TIME, 0);
}

static void btmgr_poff_finish_proc(struct bt_manager_context_t *bt_manager)
{
	uint8_t reserved;

	if (bt_manager->local_req_poweroff) {
		reserved = (bt_manager->poweroff_wait_timeout) ? BTMGR_LREQ_POFF_RESULT_TIMEOUT : BTMGR_LREQ_POFF_RESULT_OK;
		SYS_LOG_INF("Send poff result %d", reserved);
		bt_manager_event_notify(BT_POWEROFF_RESULT, &reserved, sizeof(reserved));
		bt_manager->local_later_poweroff = 0;
	} else if (bt_manager->remote_req_poweroff && (bt_manager->single_poweroff == 0)) {
		reserved = (bt_manager->poweroff_wait_timeout) ? BTMGR_RREQ_SYNC_POFF_RESULT_TIMEOUT : BTMGR_RREQ_SYNC_POFF_RESULT_OK;
		SYS_LOG_INF("Send poff result %d", reserved);
		bt_manager_event_notify(BT_POWEROFF_RESULT, &reserved, sizeof(reserved));
		bt_manager->local_later_poweroff = 0;
	} else {
		SYS_LOG_INF("Only remote poff");
	}

	if (bt_manager->local_later_poweroff) {
		SYS_LOG_INF("Do local later poff");
		bt_manager->poweroff_state = POFF_STATE_START;
		bt_manager->local_req_poweroff = 1;
		bt_manager->remote_req_poweroff = 0;
		bt_manager->single_poweroff = 1;
		bt_manager->local_later_poweroff = 0;
		btmgr_poff_set_state_work(POFF_STATE_START, POWEROFF_WAIT_MIN_TIME, 0);
	} else {
		bt_manager->poweroff_state = POFF_STATE_NONE;
		bt_manager->local_req_poweroff = 0;
		bt_manager->remote_req_poweroff = 0;
		bt_manager->single_poweroff = 0;
		bt_manager->poweroff_wait_timeout = 0;
	}
}

struct poweroff_state_handler {
	uint8_t state;
	void (*func)(struct bt_manager_context_t *bt_manager);
};

static const struct poweroff_state_handler poweroff_handler[] = {
	{POFF_STATE_NONE, btmgr_poff_none_proc},
	{POFF_STATE_START, btmgr_poff_start_proc},
	{POFF_STATE_TWS_PROC, btmgr_poff_tws_proc},
	{POFF_STATE_DISCONNECT_PHONE, btmgr_poff_disconnect_phone_proc},
	{POFF_STATE_WAIT_DISCONNECT_PHONE, btmgr_poff_wait_timeout_proc},
	{POFF_STATE_PHONE_DISCONNECTED, btmgr_poff_phone_disconnected_proc},
	{POFF_STATE_FINISH, btmgr_poff_finish_proc},
};

void btmgr_poff_state_proc_work(os_work *work)
{
	int i;
	struct bt_manager_context_t *bt_manager = bt_manager_get_context();

	os_mutex_lock(&bt_manager->poweroff_mutex, OS_FOREVER);
	SYS_LOG_INF("Poff state %d", bt_manager->poweroff_state);

	for (i = 0; i < ARRAY_SIZE(poweroff_handler); i++) {
		if (bt_manager->poweroff_state == poweroff_handler[i].state) {
			poweroff_handler[i].func(bt_manager);
			break;
		}
	}

	os_mutex_unlock(&bt_manager->poweroff_mutex);
}
