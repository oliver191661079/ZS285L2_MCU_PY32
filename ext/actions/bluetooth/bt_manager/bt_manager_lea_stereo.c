/*
 * Copyright (c) 2024 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This is the abstraction layer provided for Application layer for
 * Bluetooth le audio stereo mode.
 */

#include <bt_manager.h>
#include <mem_manager.h>
#include <thread_timer.h>
#include "bt_manager_lea_stereo.h"
#include "bt_manager_lea_policy.h"
#include "bt_manager_inner.h"
#include "btservice_api.h"
#include <acts_bluetooth/addr.h>
#include <acts_bluetooth/bluetooth.h>
#include <acts_bluetooth/host_interface.h>
#include "ctrl_interface.h"

#include <hex_str.h>
#include <sys_event.h>
#include <app_manager.h>

typedef enum {
	EVENT_NONE = 0,
	EVENT_ENTER_PREPARE,
	EVENT_ENTER_SET_PARAMS,
	EVENT_EXIT_PREPARE,
	EVENT_EXIT_SET_PARAMS,

	LEA_STEREO_EVENT_MAX_NUM,
} btmgr_lea_stereo_event_e;
	
typedef enum {
	STATE_IDLE = 0,
	STATE_ENTER_PREPARING,
	STATE_ENTER_DONE,
	STATE_EXIT_PREPARING,

	LEA_STEREO_STATE_MAX_NUM,
} btmgr_lea_stereo_state_e;

typedef struct {
	uint8_t event;
	uint8_t role; /*0-secondary(FR), 1-primary(FL), just for entering stereo mode*/
} lea_stereo_event_param_t;

/* context of lea stereo mode */
typedef struct {
	uint8_t state;
	uint8_t role : 1; /*0-secondary(FR), 1-primary(FL)*/
	uint8_t wait_disconnect : 1;
	uint8_t stereo_mode : 1;
	lea_stereo_event_param_t last_event;
} lea_stereo_mode_t;

#define SIRK_LEN    16
static uint8_t stereo_sirk[SIRK_LEN] = {
	0xB8,0x03,0xEA,0xC6,0xAF,0xBB,0x65,0xA2,
	0x5A,0x41,0xF1,0x53,0x05,0x68,0x8E,0x83
};

static lea_stereo_mode_t lea_stereo_ctx = {0};

static void bt_manager_lea_stereo_event_notify(btmgr_lea_stereo_event_e event, uint8_t role);

static void lea_stereo_acl_connected_cb(struct bt_conn *conn, u8_t err)
{

}

static void lea_stereo_acl_disconnected_cb(struct bt_conn *conn, uint8_t reason)
{
	if (lea_stereo_ctx.state == STATE_ENTER_PREPARING) {
		bt_manager_lea_stereo_event_notify(EVENT_ENTER_SET_PARAMS, 0);
	} else if (lea_stereo_ctx.state == STATE_EXIT_PREPARING) {
		bt_manager_lea_stereo_event_notify(EVENT_EXIT_SET_PARAMS, 0);
	}
}

static struct bt_conn_cb lea_stereo_conn_cbs = {
	.connected = lea_stereo_acl_connected_cb,
	.disconnected = lea_stereo_acl_disconnected_cb,
};

static void lea_stereo_enter_set_params(void)
{
	uint32_t sink_loc = 0;
	uint8_t rank = 0;
	struct bt_audio_config * cfg = btif_audio_get_cfg_param();

	if (lea_stereo_ctx.state != STATE_ENTER_PREPARING) {
		return;
	}

	if (lea_stereo_ctx.role)
	{
		sink_loc = BT_AUDIO_LOCATIONS_FL;
		rank = 1;
	} else {
		sink_loc = BT_AUDIO_LOCATIONS_FR;
		rank = 2;
	}

	btif_cas_set_size(2);
	btif_cas_set_rank(rank);
	btif_pacs_set_sink_loc(sink_loc);

	btif_audio_set_sirk(1, stereo_sirk);

	/* update rsi and ascs announcement of adv data */
#if 0
	if (lea_stereo_ctx.wait_disconnect) {
		cfg->target_announcement = 1;
	}
#endif

	btif_audio_stop_adv();
	bt_manager_le_update_adv_data(cfg);
	btif_audio_start_adv();

	bt_manager_enter_pair_mode();

	lea_stereo_ctx.state = STATE_ENTER_DONE;
}

static void lea_stereo_exit_set_params(void)
{
	uint32_t sink_loc = BT_AUDIO_LOCATIONS_FL | BT_AUDIO_LOCATIONS_FR;
	struct bt_audio_config * cfg = btif_audio_get_cfg_param();

	if (lea_stereo_ctx.state != STATE_EXIT_PREPARING) {
		return;
	}

	btif_cas_set_size(cfg->set_size);
	btif_cas_set_rank(cfg->set_rank);
	btif_pacs_set_sink_loc(sink_loc);

	if (cfg->sirk_encrypted) {
		btif_audio_set_sirk(0, cfg->set_sirk);
	} else {
		btif_audio_set_sirk(1, cfg->set_sirk);
	}

	/* update rsi and ascs announcement of adv data */
#if 0
	if (lea_stereo_ctx.wait_disconnect) {
		cfg->target_announcement = 1;
	}
#endif

	btif_audio_stop_adv();
	bt_manager_le_update_adv_data(cfg);
	btif_audio_start_adv();

	bt_manager_enter_pair_mode();
	lea_stereo_ctx.state = STATE_IDLE;

}

static uint8_t lea_stereo_reset_device(void)
{
	uint8_t wait_disconnect = 0;

	/* disconnect all conn, and clear paired list */
	if (bt_manager_ble_is_connected()) {
		wait_disconnect = 1;
		lea_stereo_ctx.wait_disconnect = 1;
		hostif_bt_conn_cb_register(&lea_stereo_conn_cbs);
	}
	bt_manager_clear_list(BTSRV_DEVICE_ALL);
	bt_manager_lea_clear_paired_list();

	return wait_disconnect;
}

static void bt_manager_lea_stereo_event_cb(void *event_param)
{
	lea_stereo_event_param_t *ptr = (lea_stereo_event_param_t *)event_param;
	SYS_LOG_INF("evt:%d,state:%d\n", ptr->event, lea_stereo_ctx.state);
	switch (ptr->event) {
		case EVENT_ENTER_PREPARE:
		{
			lea_stereo_ctx.state = STATE_ENTER_PREPARING;
			lea_stereo_ctx.role = ptr->role;
			if (lea_stereo_reset_device()) {
				/* set params after disconnect */
				break;
			}
			/*no need break, set params right now */
		}
		case EVENT_ENTER_SET_PARAMS:
		{
			lea_stereo_enter_set_params();
			if (lea_stereo_ctx.wait_disconnect) {
				hostif_bt_conn_cb_unregister(&lea_stereo_conn_cbs);
				lea_stereo_ctx.wait_disconnect = 0;
			}
			break;
		}
		case EVENT_EXIT_PREPARE:
		{
			lea_stereo_ctx.state = STATE_EXIT_PREPARING;
			if (lea_stereo_reset_device()) {
				/* set params after disconnect */
				break;
			}
			/*no need break, set params right now */
		}
		case EVENT_EXIT_SET_PARAMS:
		{
			lea_stereo_exit_set_params();
			if (lea_stereo_ctx.wait_disconnect) {
				hostif_bt_conn_cb_unregister(&lea_stereo_conn_cbs);
				lea_stereo_ctx.wait_disconnect = 0;
			}
			break;
		}

		default:
			break;
	}

	/* process the cache event */
	if (EVENT_NONE != lea_stereo_ctx.last_event.event) {
		if (STATE_IDLE == lea_stereo_ctx.state
			&& EVENT_ENTER_PREPARE == lea_stereo_ctx.last_event.event) {
			bt_manager_lea_stereo_event_notify(lea_stereo_ctx.last_event.event, lea_stereo_ctx.last_event.role);
		}
		if (STATE_ENTER_DONE == lea_stereo_ctx.state
			&& EVENT_EXIT_PREPARE == lea_stereo_ctx.last_event.event) {
			bt_manager_lea_stereo_event_notify(EVENT_EXIT_PREPARE, 0);
		}
		lea_stereo_ctx.last_event.event = EVENT_NONE;
		lea_stereo_ctx.last_event.role = 0;
	}
}

static void bt_manager_lea_stereo_event_notify(btmgr_lea_stereo_event_e event, uint8_t role)
{
	lea_stereo_event_param_t evt_param = {0};
	evt_param.event = event;
	evt_param.role = role;

	btif_audio_cb_event_notify(bt_manager_lea_stereo_event_cb, &evt_param, sizeof(evt_param));
}

void bt_manager_lea_stereo_set_sirk(uint8_t *sirk_ptr)
{
	if (sirk_ptr) {
		memcpy(stereo_sirk, sirk_ptr, SIRK_LEN);
	}
}

void bt_manager_lea_enter_stereo_mode(bool primary)
{
	if (lea_stereo_ctx.stereo_mode) {
		SYS_LOG_ERR("already\n");
		return;
	}
	lea_stereo_ctx.stereo_mode = 1;

	/* check if event need cache */
	if (STATE_EXIT_PREPARING == lea_stereo_ctx.state
		|| STATE_ENTER_PREPARING == lea_stereo_ctx.state) {
		lea_stereo_ctx.last_event.event = EVENT_ENTER_PREPARE;
		lea_stereo_ctx.last_event.role = primary;
		SYS_LOG_INF("cache\n");
		return;
	}

	bt_manager_lea_stereo_event_notify(EVENT_ENTER_PREPARE, primary);

}

void bt_manager_lea_exit_stereo_mode(void)
{
	if (!lea_stereo_ctx.stereo_mode) {
		SYS_LOG_ERR("already\n");
		return;
	}
	lea_stereo_ctx.stereo_mode = 0;

	/* check if event need cache */
	if (STATE_EXIT_PREPARING == lea_stereo_ctx.state
		|| STATE_ENTER_PREPARING == lea_stereo_ctx.state) {
		lea_stereo_ctx.last_event.event = EVENT_EXIT_PREPARE;
		lea_stereo_ctx.last_event.role = 0;
		SYS_LOG_INF("cache\n");
		return;
	}

	bt_manager_lea_stereo_event_notify(EVENT_EXIT_PREPARE, 0);
}

/* for Blutooth ON, call this func before adv start */
void bt_manager_lea_stereo_mode_init(bool primary)
{
	uint32_t sink_loc = 0;
	uint8_t rank = 0;
	struct bt_audio_config * cfg = btif_audio_get_cfg_param();
	lea_stereo_ctx.stereo_mode = 1;
	lea_stereo_ctx.role = primary;

	if (lea_stereo_ctx.role)
	{
		sink_loc = BT_AUDIO_LOCATIONS_FL;
		rank = 1;
	} else {
		sink_loc = BT_AUDIO_LOCATIONS_FR;
		rank = 2;
	}

	btif_cas_set_size(2);
	btif_cas_set_rank(rank);
	btif_pacs_set_sink_loc(sink_loc);

	btif_audio_set_sirk(1, stereo_sirk);

	/* update rsi adv data */
	bt_manager_le_update_adv_data(cfg);

}

