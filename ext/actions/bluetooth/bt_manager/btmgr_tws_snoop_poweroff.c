/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt manager tws snoop poweroff.
 */
#define SYS_LOG_DOMAIN "bt manager"

#include <os_common_api.h>

#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stream.h>
#include <bt_manager.h>
#include <sys_event.h>
#include <power_manager.h>
#include "bt_manager_inner.h"
#include "btservice_api.h"
#include "btmgr_tws_snoop_inner.h"

enum {
	TWS_POWEROFF_CMD_REQ,
	TWS_POWEROFF_CMD_ACK,
	TWS_POWEROFF_CMD_ACK_SLAVE_POFF_FIRST,
};

struct tws_poweroff_info {
	uint8_t cmd;
	uint8_t single_poweroff:1;
};

void btmgr_tws_poff_check_tws_role_change(void)
{
	struct bt_manager_context_t *bt_manager = bt_manager_get_context();
	struct btmgr_tws_context_t *context = btmgr_get_tws_context();

	os_mutex_lock(&bt_manager->poweroff_mutex, OS_FOREVER);

	if (bt_manager->poweroff_state == POFF_STATE_TWS_PROC) {
		if (context->poweroff_state == TWS_POFFS_WAIT_SWTO_SLAVE) {
			if (context->tws_role == BTSRV_TWS_SLAVE) {
				btmgr_tws_poff_set_state_work(POFF_STATE_TWS_PROC, TWS_POFFS_WAIT_DISCONNECTED, POWEROFF_WAIT_TIMEOUT, 1);
			}
		} else if (context->poweroff_state == TWS_POFFS_WAIT_SWTO_MASTER) {
			if (context->tws_role == BTSRV_TWS_MASTER) {
				if (btmgr_is_snoop_link_created()) {
					btif_tws_disconnect_snoop_link();
					btmgr_tws_poff_set_state_work(POFF_STATE_TWS_PROC,
									TWS_POFFS_WAIT_SNOOP_DISCONNECTED, POWEROFF_WAIT_TIMEOUT, 1);
				} else {
					btif_br_disconnect_device(BTSRV_DISCONNECT_TWS_MODE);
					btmgr_tws_poff_set_state_work(POFF_STATE_TWS_PROC,
									TWS_POFFS_WAIT_DISCONNECTED, POWEROFF_WAIT_TIMEOUT, 1);
				}
			}
		}
	}

	os_mutex_unlock(&bt_manager->poweroff_mutex);
}

void btmgr_tws_poff_check_tws_disconnected(void)
{
	struct bt_manager_context_t *bt_manager = bt_manager_get_context();
	struct btmgr_tws_context_t *context = btmgr_get_tws_context();

	os_mutex_lock(&bt_manager->poweroff_mutex, OS_FOREVER);

	if (bt_manager->poweroff_state == POFF_STATE_TWS_PROC) {
		SYS_LOG_INF("Poff check tws disconnected role %d state %d",
					context->tws_role, context->poweroff_state);
		if ((context->poweroff_state > TWS_POFFS_START) &&
			(context->poweroff_state < TWS_POFFS_DISCONNECTED) &&
			context->tws_role == BTSRV_TWS_NONE) {
			btmgr_tws_poff_set_state_work(POFF_STATE_TWS_PROC, TWS_POFFS_DISCONNECTED, POWEROFF_WAIT_MIN_TIME, 0);
		}
	}

	os_mutex_unlock(&bt_manager->poweroff_mutex);
}

void btmgr_tws_poff_check_snoop_disconnect(void)
{
	struct bt_manager_context_t *bt_manager = bt_manager_get_context();
	struct btmgr_tws_context_t *context = btmgr_get_tws_context();

	os_mutex_lock(&bt_manager->poweroff_mutex, OS_FOREVER);

	if (bt_manager->poweroff_state == POFF_STATE_TWS_PROC &&
		context->poweroff_state == TWS_POFFS_WAIT_SNOOP_DISCONNECTED) {
		if (!btmgr_is_snoop_link_created()) {
			btmgr_tws_poff_set_state_work(POFF_STATE_TWS_PROC, TWS_POFFS_SNOOP_DISCONNECTED, POWEROFF_WAIT_MIN_TIME, 0);
		}
	}

	os_mutex_unlock(&bt_manager->poweroff_mutex);
}

void btmgr_tws_poff_info_proc(uint8_t *data, uint8_t len)
{
	struct tws_poweroff_info info;
	struct bt_manager_context_t *bt_manager = bt_manager_get_context();
	struct btmgr_tws_context_t *context = btmgr_get_tws_context();

	os_mutex_lock(&bt_manager->poweroff_mutex, OS_FOREVER);

	memcpy(&info, data, sizeof(info));
	SYS_LOG_INF("Poff info state %d cmd %d single %d", bt_manager->poweroff_state, info.cmd, info.single_poweroff);

	if (info.cmd == TWS_POWEROFF_CMD_REQ) {
		if (bt_manager->poweroff_state == POFF_STATE_NONE) {
			bt_manager->remote_req_poweroff = 1;
			bt_manager->single_poweroff = info.single_poweroff;

			info.cmd = TWS_POWEROFF_CMD_ACK;
			bt_manager_tws_send_message(TWS_BT_MGR_EVENT, TWS_EVENT_SYNC_POWEROFF_INFO,
										(uint8_t *)&info, sizeof(info));
			btmgr_tws_poff_set_state_work(POFF_STATE_START, TWS_POFFS_NONE, POWEROFF_WAIT_MIN_TIME, 0);
		} else {
			if (context->tws_role == BTSRV_TWS_SLAVE) {
				info.cmd = TWS_POWEROFF_CMD_ACK_SLAVE_POFF_FIRST;
				bt_manager_tws_send_message(TWS_BT_MGR_EVENT, TWS_EVENT_SYNC_POWEROFF_INFO,
										(uint8_t *)&info, sizeof(info));
			} else {
				info.cmd = TWS_POWEROFF_CMD_ACK;
				bt_manager_tws_send_message(TWS_BT_MGR_EVENT, TWS_EVENT_SYNC_POWEROFF_INFO,
										(uint8_t *)&info, sizeof(info));
			}
		}
	} else if (info.cmd == TWS_POWEROFF_CMD_ACK) {
		btmgr_tws_poff_set_state_work(POFF_STATE_TWS_PROC, TWS_POFFS_RX_ACK, POWEROFF_WAIT_MIN_TIME, 0);
	} else if (info.cmd == TWS_POWEROFF_CMD_ACK_SLAVE_POFF_FIRST) {
		SYS_LOG_INF("Poff set master later poff");
		bt_manager->local_req_poweroff = 0;
		bt_manager->remote_req_poweroff = 1;
		bt_manager->single_poweroff = 1;
		bt_manager->local_later_poweroff = 1;
		btmgr_tws_poff_set_state_work(POFF_STATE_TWS_PROC, TWS_POFFS_START, POWEROFF_WAIT_MIN_TIME, 0);
	}

	os_mutex_unlock(&bt_manager->poweroff_mutex);
}

void btmgr_tws_poff_set_state_work(uint8_t main_state, uint8_t tws_state, uint32_t time, uint8_t timeout_flag)
{
	struct btmgr_tws_context_t *context = btmgr_get_tws_context();

	SYS_LOG_INF("tws_poff_set %d %d %d %d", main_state, tws_state, time, timeout_flag);
	context->poweroff_state = tws_state;
	btmgr_poff_set_state_work(main_state, time, timeout_flag);
}

static void btmgr_tws_poff_none_proc(struct btmgr_tws_context_t *context)
{
	btif_tws_notify_start_power_off();

	if (context->tws_role == BTSRV_TWS_NONE) {
		btmgr_tws_poff_set_state_work(POFF_STATE_DISCONNECT_PHONE,
								TWS_POFFS_NONE, POWEROFF_WAIT_MIN_TIME, 0);
	} else {
		btmgr_tws_poff_set_state_work(POFF_STATE_TWS_PROC,
								TWS_POFFS_START, POWEROFF_WAIT_MIN_TIME, 0);
	}
}

static void btmgr_tws_send_poweroff_req_cmd(void)
{
	struct tws_poweroff_info info;
	struct bt_manager_context_t *bt_manager = bt_manager_get_context();

	info.cmd = TWS_POWEROFF_CMD_REQ;
	info.single_poweroff = bt_manager->single_poweroff;
	bt_manager_tws_send_message(TWS_BT_MGR_EVENT, TWS_EVENT_SYNC_POWEROFF_INFO,
								(uint8_t *)&info, sizeof(info));
	btmgr_tws_poff_set_state_work(POFF_STATE_TWS_PROC,
						TWS_POFFS_CMD_WAIT_ACK, POWEROFF_WAIT_TIMEOUT, 1);
}

static void btmgr_tws_poff_start_proc(struct btmgr_tws_context_t *context)
{
	struct bt_manager_context_t *bt_manager = bt_manager_get_context();

	if (context->tws_role == BTSRV_TWS_MASTER) {
		if (bt_manager->remote_req_poweroff) {
			if (btmgr_is_snoop_link_created()) {
				btif_tws_disconnect_snoop_link();
				btmgr_tws_poff_set_state_work(POFF_STATE_TWS_PROC,
								TWS_POFFS_WAIT_SNOOP_DISCONNECTED, POWEROFF_WAIT_TIMEOUT, 1);
			} else {
				btif_br_disconnect_device(BTSRV_DISCONNECT_TWS_MODE);
				btmgr_tws_poff_set_state_work(POFF_STATE_TWS_PROC,
								TWS_POFFS_WAIT_DISCONNECTED, POWEROFF_WAIT_TIMEOUT, 1);
			}
		} else {
			/* Local request poweroff */
			btmgr_tws_send_poweroff_req_cmd();
		}
	} else if (context->tws_role == BTSRV_TWS_SLAVE) {
		if (bt_manager->remote_req_poweroff) {
			/* Remote request poweroff */
			if (bt_manager->single_poweroff) {
				if (bt_manager_get_connected_dev_num()) {
					btmgr_tws_poff_set_state_work(POFF_STATE_TWS_PROC,
									TWS_POFFS_WAIT_SWTO_MASTER, POWEROFF_WAIT_TIMEOUT, 1);
				} else {
					btmgr_tws_poff_set_state_work(POFF_STATE_TWS_PROC,
									TWS_POFFS_WAIT_DISCONNECTED, POWEROFF_WAIT_TIMEOUT, 1);
				}
			} else {
				btmgr_tws_poff_set_state_work(POFF_STATE_TWS_PROC,
								TWS_POFFS_WAIT_DISCONNECTED, POWEROFF_WAIT_TIMEOUT, 1);
			}
		} else {
			/* Local request poweroff */
			btmgr_tws_send_poweroff_req_cmd();
		}
	} else {
		btmgr_tws_poff_set_state_work(POFF_STATE_DISCONNECT_PHONE,
								TWS_POFFS_NONE, POWEROFF_WAIT_MIN_TIME, 0);
	}
}

static void btmgr_tws_poff_wait_timeout_proc(struct btmgr_tws_context_t *context)
{
	/* Should not happend, need check why ? */
	SYS_LOG_INF("State %d wait timeout!", context->poweroff_state);

	if (context->tws_role == BTSRV_TWS_NONE) {
		btmgr_tws_poff_set_state_work(POFF_STATE_DISCONNECT_PHONE,
								TWS_POFFS_NONE, POWEROFF_WAIT_MIN_TIME, 0);
	} else {
		btif_br_disconnect_device(BTSRV_DISCONNECT_TWS_MODE);
		btmgr_tws_poff_set_state_work(POFF_STATE_TWS_PROC,
								TWS_POFFS_WAIT_FORCE_DISCONNECTED, POWEROFF_WAIT_TIMEOUT, 1);
	}
}

static void btmgr_tws_poff_force_dis_timeout_proc(struct btmgr_tws_context_t *context)
{
	SYS_LOG_INF("Tws force disconnect timeout!");
	btmgr_tws_poff_set_state_work(POFF_STATE_DISCONNECT_PHONE,
						TWS_POFFS_NONE, POWEROFF_WAIT_MIN_TIME, 0);
}

static void btmgr_tws_poff_rx_ack_proc(struct btmgr_tws_context_t *context)
{
	struct bt_manager_context_t *bt_manager = bt_manager_get_context();

	if (context->tws_role == BTSRV_TWS_MASTER) {
		if (bt_manager->single_poweroff) {
			if (bt_manager_get_connected_dev_num()) {
            #ifdef CONFIG_SUPPORT_TWS_ROLE_CHANGE
				btmgr_tws_switch_snoop_link();
				btmgr_tws_poff_set_state_work(POFF_STATE_TWS_PROC,
								TWS_POFFS_WAIT_SWTO_SLAVE, POWEROFF_WAIT_TIMEOUT, 1);
            #else
				btif_tws_disconnect_snoop_link();
				btmgr_tws_poff_set_state_work(POFF_STATE_TWS_PROC,
								TWS_POFFS_WAIT_SNOOP_DISCONNECTED, POWEROFF_WAIT_TIMEOUT, 1);
            #endif
			} else {
				btif_br_disconnect_device(BTSRV_DISCONNECT_TWS_MODE);
				btmgr_tws_poff_set_state_work(POFF_STATE_TWS_PROC,
								TWS_POFFS_WAIT_DISCONNECTED, POWEROFF_WAIT_TIMEOUT, 1);
			}
		} else {
			if (bt_manager_get_connected_dev_num()) {
				btif_tws_disconnect_snoop_link();
				btmgr_tws_poff_set_state_work(POFF_STATE_TWS_PROC,
								TWS_POFFS_WAIT_SNOOP_DISCONNECTED, POWEROFF_WAIT_TIMEOUT, 1);
			} else {
				btif_br_disconnect_device(BTSRV_DISCONNECT_TWS_MODE);
				btmgr_tws_poff_set_state_work(POFF_STATE_TWS_PROC,
								TWS_POFFS_WAIT_DISCONNECTED, POWEROFF_WAIT_TIMEOUT, 1);
			}
		}
	} else if (context->tws_role == BTSRV_TWS_SLAVE) {
		btmgr_tws_poff_set_state_work(POFF_STATE_TWS_PROC,
						TWS_POFFS_WAIT_DISCONNECTED, POWEROFF_WAIT_TIMEOUT, 1);
	} else {
		btmgr_tws_poff_set_state_work(POFF_STATE_DISCONNECT_PHONE,
								TWS_POFFS_NONE, POWEROFF_WAIT_MIN_TIME, 0);
	}
}

static void btmgr_tws_poff_snoop_disconnected_proc(struct btmgr_tws_context_t *context)
{
	if (context->tws_role == BTSRV_TWS_MASTER) {
		btif_br_disconnect_device(BTSRV_DISCONNECT_TWS_MODE);
		btmgr_tws_poff_set_state_work(POFF_STATE_TWS_PROC,
								TWS_POFFS_WAIT_DISCONNECTED, POWEROFF_WAIT_TIMEOUT, 1);
	} else {
		SYS_LOG_INF("Should not happened!");
	}
}

static void btmgr_tws_poff_tws_disconnected_proc(struct btmgr_tws_context_t *context)
{
	btmgr_tws_poff_set_state_work(POFF_STATE_DISCONNECT_PHONE, TWS_POFFS_NONE, POWEROFF_WAIT_MIN_TIME, 0);
}

struct tws_poweroff_state_handler {
	uint8_t state;
	void (*func)(struct btmgr_tws_context_t *context);
};

static const struct tws_poweroff_state_handler tws_poweroff_handler[] = {
	{TWS_POFFS_NONE, btmgr_tws_poff_none_proc},
	{TWS_POFFS_START, btmgr_tws_poff_start_proc},
	{TWS_POFFS_CMD_WAIT_ACK, btmgr_tws_poff_wait_timeout_proc},
	{TWS_POFFS_RX_ACK, btmgr_tws_poff_rx_ack_proc},
	{TWS_POFFS_WAIT_SWTO_MASTER, btmgr_tws_poff_wait_timeout_proc},
	{TWS_POFFS_WAIT_SWTO_SLAVE, btmgr_tws_poff_wait_timeout_proc},
	{TWS_POFFS_WAIT_SNOOP_DISCONNECTED, btmgr_tws_poff_wait_timeout_proc},
	{TWS_POFFS_SNOOP_DISCONNECTED, btmgr_tws_poff_snoop_disconnected_proc},
	{TWS_POFFS_WAIT_DISCONNECTED, btmgr_tws_poff_wait_timeout_proc},
	{TWS_POFFS_WAIT_FORCE_DISCONNECTED, btmgr_tws_poff_force_dis_timeout_proc},
	{TWS_POFFS_DISCONNECTED, btmgr_tws_poff_tws_disconnected_proc},
};

void btmgr_tws_poff_state_proc(void)
{
	uint8_t i;
	struct btmgr_tws_context_t *context = btmgr_get_tws_context();

	SYS_LOG_INF("Tws poff state %d", context->poweroff_state);

	for (i = 0; i < ARRAY_SIZE(tws_poweroff_handler); i++) {
		if (context->poweroff_state == tws_poweroff_handler[i].state) {
			tws_poweroff_handler[i].func(context);
			break;
		}
	}
}
