/*
 * Copyright (c) 2024 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This is the abstraction layer provided for Application layer for
 * Bluetooth le audio access policy.
 */

#include <bt_manager.h>
#include <mem_manager.h>
#include <thread_timer.h>
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

/*bt_manager_get_pair_config()->pair_mode_duration_sec = 180s*/
#define PAIRING_DURATION_MS         	((bt_manager_get_pair_config()->pair_mode_duration_sec) * (1000))
#define DIR_ADV_DURATION_MS         	((5) * (1000))
#define LEA_RECONNECT_PHONE_TIMEOUT 	((bt_manager_get_pair_config()->pair_mode_duration_sec) * (1000))
#define LEA_RECONNECT_TARGET_TIMEOUT 	((10) * (1000))
#define LEA_HALT_RESUME_TARGET_TIMEOUT	LEA_RECONNECT_TARGET_TIMEOUT

#define ADDR_VAL(addr)              (addr)->a.val[0],(addr)->a.val[1],(addr)->a.val[2],\
                                     (addr)->a.val[3],(addr)->a.val[4],(addr)->a.val[5]

typedef int (* lea_policy_state_event_handle_func)(btmgr_lea_policy_event_e event, void *param, uint8_t len);

/* state of le audio access policy*/
typedef enum {
	LEA_POLICY_STATE_WAITING = 0,
	LEA_POLICY_STATE_AUTO_RECONNECTING,
	LEA_POLICY_STATE_PAIRING,
	LEA_POLICY_STATE_LINKLOSS_RECONNECTING,
	LEA_POLICY_STATE_CONNECTED,
	LEA_POLICY_STATE_HALT_RESUME,
	LEA_POLICY_STATE_MAX_NUM,
} btmgr_lea_policy_state_e;

typedef enum {
	LEA_NON_CONN_NON_DISC = 0,
	LEA_PUB_UNDIRECT_OPEN_CONN_TARGET_DISC,
	LEA_PUB_UNDIRECT_OPEN_CONN_GENERAL_DISC,
	LEA_RPA_DIRECT_OPEN_CONN_TARGET_DISC,
	LEA_RPA_DIRECT_OPEN_CONN_GENERAL_DISC,
	LEA_RPA_DIRECT_LIMIT_CONN_TARGET_DISC,
	LEA_RPA_DIRECT_LIMIT_CONN_GENERAL_DISC,
	LEA_RPA_UNDIRECT_OPEN_CONN_TARGET_DISC, //target+no list
	LEA_RPA_UNDIRECT_OPEN_CONN_GENERAL_DISC, //general+no list
	LEA_RPA_UNDIRECT_LIMIT_CONN_TARGET_DISC, //target+list
	LEA_RPA_UNDIRECT_LIMIT_CONN_GENERAL_DISC, //general+list
	LEA_POLICY_VISUAL_MAX_NUM,
} btmgr_lea_policy_visual_mode_e;

typedef enum {
	ADDRESS_RESOLUTION_DISABLE,
	ADDRESS_RESOLUTION_ENABLE,
	ADDRESS_RESOLUTION_MAX_NUM,
}btmgr_lea_policy_address_resolution_e;

/* structure of le audio access policy*/
typedef struct {
	uint8_t max_dev_num:5;
	uint8_t btoff : 1;
	uint8_t policy_enable : 1;
	uint8_t halt_phone : 1;
	uint8_t cas_update :1;
	uint8_t cas_announcement_type;
	btmgr_lea_policy_state_e lea_policy_state;
	uint32_t option;
	lea_policy_state_event_handle_func *funcs;
	bt_addr_le_t last_lea_dev_info;
	struct thread_timer lea_policy_timer;
	uint8_t halt_dev_num;
	btmgr_lea_policy_visual_mode_e lea_visual_mode;
	btmgr_lea_policy_address_resolution_e address_resolution;
} btmgr_lea_policy_t;

typedef struct {
	uint32_t event;
	void *param;
	uint8_t len;
} lea_policy_event_param_t;

static int lea_policy_state_waiting_handler(btmgr_lea_policy_event_e event, void *param, uint8_t len);
static int lea_policy_state_pairing_handler(btmgr_lea_policy_event_e event, void *param, uint8_t len);
static int lea_policy_state_auto_reconnecting_handler(btmgr_lea_policy_event_e event, void *param, uint8_t len);
static int lea_policy_state_linkloss_reconnecting_handler(btmgr_lea_policy_event_e event, void *param, uint8_t len);
static int lea_policy_state_connected_handler(btmgr_lea_policy_event_e event, void *param, uint8_t len);
static int lea_policy_state_halt_resume_handler(btmgr_lea_policy_event_e event, void *param, uint8_t len);
static void timeout_handler (struct thread_timer *ttimer, void *expiry_fn_arg);

static btmgr_lea_policy_t btmgr_lea_policy_ctx = {0};
static lea_policy_state_event_handle_func lea_policy_state_event_handler[LEA_POLICY_STATE_MAX_NUM] =
{
	lea_policy_state_waiting_handler,
	lea_policy_state_auto_reconnecting_handler,
	lea_policy_state_pairing_handler,
	lea_policy_state_linkloss_reconnecting_handler,
	lea_policy_state_connected_handler,
	lea_policy_state_halt_resume_handler,
};

struct lea_policy_phone_addr {
	bt_addr_le_t addr;
	sys_snode_t node;
};

/* list of lea phone for reconnection  */
static sys_slist_t lea_policy_phone_addr_list = {NULL, NULL};
static struct thread_timer lea_phone_reconnect_timer;

static uint8_t lea_get_address_resolution(btmgr_lea_policy_visual_mode_e visual_mode)
{
	uint8_t address_resolution = 0;

	if ((visual_mode == LEA_RPA_DIRECT_LIMIT_CONN_TARGET_DISC)||
		(visual_mode == LEA_RPA_DIRECT_LIMIT_CONN_GENERAL_DISC)||
		(visual_mode == LEA_RPA_UNDIRECT_LIMIT_CONN_TARGET_DISC)||
		(visual_mode == LEA_RPA_UNDIRECT_LIMIT_CONN_GENERAL_DISC)) {
		address_resolution = 1;
	}
	return address_resolution;
}

static uint8_t lea_get_target_announcement(btmgr_lea_policy_visual_mode_e visual_mode)
{
	uint8_t target_announcement = 0;

	if ((visual_mode == LEA_PUB_UNDIRECT_OPEN_CONN_TARGET_DISC)||
		(visual_mode == LEA_RPA_DIRECT_OPEN_CONN_TARGET_DISC)||
		(visual_mode == LEA_RPA_DIRECT_LIMIT_CONN_TARGET_DISC)||
		(visual_mode == LEA_RPA_UNDIRECT_OPEN_CONN_TARGET_DISC)||
		(visual_mode == LEA_RPA_UNDIRECT_LIMIT_CONN_TARGET_DISC)) {
		target_announcement = 1;
	}
	return target_announcement;
}

static uint8_t lea_get_addr_type(btmgr_lea_policy_visual_mode_e visual_mode)
{
	uint8_t rpa_type = 1;

	if ((visual_mode == LEA_PUB_UNDIRECT_OPEN_CONN_TARGET_DISC) ||
		(visual_mode == LEA_PUB_UNDIRECT_OPEN_CONN_GENERAL_DISC)) {
		rpa_type = 0;
	}
	return rpa_type;
}

static uint8_t lea_get_adv_directed(btmgr_lea_policy_visual_mode_e visual_mode)
{
	uint8_t directed = 0;

	if ((visual_mode == LEA_RPA_DIRECT_OPEN_CONN_TARGET_DISC)||
		(visual_mode == LEA_RPA_DIRECT_OPEN_CONN_GENERAL_DISC)||
		(visual_mode == LEA_RPA_DIRECT_LIMIT_CONN_TARGET_DISC)||
		(visual_mode == LEA_RPA_DIRECT_LIMIT_CONN_GENERAL_DISC)) {
			directed = 1;
	}
	return directed;
}

static uint8_t lea_get_adv_limited(btmgr_lea_policy_visual_mode_e visual_mode)
{
	uint8_t limited = 0;

	if ((visual_mode == LEA_RPA_DIRECT_LIMIT_CONN_TARGET_DISC)||
		(visual_mode == LEA_RPA_DIRECT_LIMIT_CONN_GENERAL_DISC)||
		(visual_mode == LEA_RPA_UNDIRECT_LIMIT_CONN_TARGET_DISC)||
		(visual_mode == LEA_RPA_UNDIRECT_LIMIT_CONN_GENERAL_DISC)) {
			limited = 1;
	}
	return limited;
}

static btmgr_lea_policy_visual_mode_e lea_get_visual_mode(void)
{
	return btmgr_lea_policy_ctx.lea_visual_mode;
}

static void lea_set_policy_state(btmgr_lea_policy_state_e policy_state)
{
	os_sched_lock();
	if(btmgr_lea_policy_ctx.lea_policy_state != policy_state) {
		btmgr_lea_policy_ctx.lea_policy_state = policy_state;
		SYS_LOG_INF("state:%d\n", btmgr_lea_policy_ctx.lea_policy_state);
	}

	if ((btmgr_lea_policy_ctx.lea_policy_state != LEA_POLICY_STATE_MAX_NUM) &&
		(btmgr_lea_policy_ctx.lea_policy_state != LEA_POLICY_STATE_HALT_RESUME)) {
		btmgr_lea_policy_ctx.halt_phone = 0;
		btmgr_lea_policy_ctx.halt_dev_num = 0;
		SYS_LOG_INF("halt_phone_clear\n");
	}
	os_sched_unlock();
}

static void lea_policy_timer_stop(void)
{
	if (thread_timer_is_running(&btmgr_lea_policy_ctx.lea_policy_timer)) {
		thread_timer_stop(&btmgr_lea_policy_ctx.lea_policy_timer);
	}
	SYS_LOG_INF("timer stop:%d\n", 1);
}

static void lea_policy_timer_start(int32_t time_duration)
{
	if (thread_timer_is_running(&btmgr_lea_policy_ctx.lea_policy_timer)) {
		thread_timer_stop(&btmgr_lea_policy_ctx.lea_policy_timer);
	}
	thread_timer_init(&btmgr_lea_policy_ctx.lea_policy_timer, timeout_handler, NULL);
	thread_timer_start(&btmgr_lea_policy_ctx.lea_policy_timer, time_duration, 0);
	SYS_LOG_INF("timer start:%d\n", time_duration);
}

static void lea_set_visual_mode(btmgr_lea_policy_visual_mode_e visual_mode)
{
	uint8_t force_update = 0;
	uint8_t address_resolution = 0;
	uint8_t old_target_ann, new_target_ann;
	struct bt_audio_config * cfg = btif_audio_get_cfg_param();
	static bt_addr_le_t last_lea_dev_info_old = {0};

	SYS_LOG_INF("cfg: %p, %x\r\n", cfg, visual_mode);

	os_sched_lock();
	if (bt_addr_le_cmp(&btmgr_lea_policy_ctx.last_lea_dev_info, &last_lea_dev_info_old)) {
		bt_addr_le_copy(&last_lea_dev_info_old, (const bt_addr_le_t *)&btmgr_lea_policy_ctx.last_lea_dev_info);
		force_update = 1;
	}

	if ((btmgr_lea_policy_ctx.lea_visual_mode != visual_mode) 
	    || (force_update) 
	    || btmgr_lea_policy_ctx.cas_update) {

		old_target_ann = cfg->target_announcement;
		new_target_ann = lea_get_target_announcement(visual_mode);

		btmgr_lea_policy_ctx.lea_visual_mode = visual_mode;
		if (btmgr_lea_policy_ctx.lea_visual_mode == LEA_NON_CONN_NON_DISC) {
			bt_manager_le_audio_adv_disable();
		}else {
			bt_manager_le_audio_adv_disable();

			//target_announcement check
			if ((old_target_ann != new_target_ann) || btmgr_lea_policy_ctx.cas_update) {
			    btmgr_lea_policy_ctx.cas_update = 0;
				cfg->target_announcement = new_target_ann;
				bt_manager_le_update_adv_data(cfg);
			}

			//address_resolution check
			address_resolution = lea_get_address_resolution(btmgr_lea_policy_ctx.lea_visual_mode);
			if(btmgr_lea_policy_ctx.address_resolution != address_resolution) {
				btmgr_lea_policy_ctx.address_resolution = address_resolution;
				if(address_resolution) {
#if defined(CONFIG_BT_PRIVACY)
					hostif_bt_le_address_resolution_enable(ADDRESS_RESOLUTION_ENABLE);
#endif
				}else {
#if defined(CONFIG_BT_PRIVACY)
					hostif_bt_le_address_resolution_enable(ADDRESS_RESOLUTION_DISABLE);
#endif
				}
			}

			SYS_LOG_INF("changed: %x, %x, %x, %x\n", btmgr_lea_policy_ctx.lea_policy_state, cfg->target_announcement,
						btmgr_lea_policy_ctx.address_resolution, btmgr_lea_policy_ctx.lea_visual_mode);
			bt_manager_le_audio_adv_enable();
		}
	}else {
		SYS_LOG_INF("no changed:%d, %d\n", btmgr_lea_policy_ctx.lea_policy_state, visual_mode);
	}
	os_sched_unlock();
}

static void lea_connect_max_dev(void)
{
	lea_policy_timer_stop();
	lea_set_policy_state(LEA_POLICY_STATE_CONNECTED);
    bt_manager_lea_set_cas_announcement_type(CAS_ANNOUNCEMENT_GENERAL);
	lea_set_visual_mode(LEA_NON_CONN_NON_DISC);
}

static void lea_policy_disable(uint8_t adv_enable)
{
	lea_policy_timer_stop();
	btmgr_lea_policy_ctx.policy_enable = 0;
    btmgr_lea_policy_ctx.cas_announcement_type = CAS_ANNOUNCEMENT_GENERAL;

	if (adv_enable) {
		lea_set_policy_state(LEA_POLICY_STATE_PAIRING);
		lea_set_visual_mode(LEA_PUB_UNDIRECT_OPEN_CONN_GENERAL_DISC);
	} else {
		lea_set_policy_state(LEA_POLICY_STATE_MAX_NUM);
		lea_set_visual_mode(LEA_NON_CONN_NON_DISC);
	}
	SYS_LOG_INF("policy state:%d\n", btmgr_lea_policy_ctx.lea_policy_state);
}

static void lea_policy_enable(uint8_t halt_resume)
{
	uint8_t cur_lea_num = bt_manager_audio_get_leaudio_dev_num();

	btmgr_lea_policy_ctx.policy_enable = 1;
    btmgr_lea_policy_ctx.cas_announcement_type = CAS_ANNOUNCEMENT_GENERAL;

	int ret = btif_audio_get_last_leaudio_device(&btmgr_lea_policy_ctx.last_lea_dev_info);
	if (!ret) {
		char addr_str[BT_ADDR_LE_STR_LEN];
		bt_addr_le_to_str(&btmgr_lea_policy_ctx.last_lea_dev_info, addr_str, sizeof(addr_str));
		SYS_LOG_INF("addr:%s\n", addr_str);
	}

	if (cur_lea_num < btmgr_lea_policy_ctx.max_dev_num) {
		if (halt_resume) {
			lea_set_policy_state(LEA_POLICY_STATE_HALT_RESUME);
			lea_set_visual_mode(LEA_RPA_UNDIRECT_LIMIT_CONN_TARGET_DISC);
			lea_policy_timer_start(LEA_HALT_RESUME_TARGET_TIMEOUT);
		}else {
			lea_set_policy_state(LEA_POLICY_STATE_WAITING);
			lea_set_visual_mode(LEA_RPA_UNDIRECT_LIMIT_CONN_GENERAL_DISC);
		}
	} else {
		lea_connect_max_dev();
	}
	SYS_LOG_INF("policy state:%d, %p, %d\n", btmgr_lea_policy_ctx.lea_policy_state,
					btmgr_lea_policy_ctx.funcs, btmgr_lea_policy_ctx.max_dev_num);
}

static void lea_policy_clr_phone_addr_list(void)
{
	struct lea_policy_phone_addr *addr_node;

	if (sys_slist_is_empty(&lea_policy_phone_addr_list)) {
		return;
	}

	os_sched_lock();
	SYS_SLIST_FOR_EACH_CONTAINER(&lea_policy_phone_addr_list, addr_node, node) {
		SYS_LOG_INF("recon_addr del:%x%x%x%x%x%x\n",ADDR_VAL(&addr_node->addr));
		sys_slist_find_and_remove(&lea_policy_phone_addr_list,&addr_node->node);
		mem_free(addr_node);
	}
	os_sched_unlock();
}

static void phone_reconnect_timeout_handler(struct thread_timer *ttimer, void *arg)
{
    bt_manager_lea_set_cas_announcement_type(CAS_ANNOUNCEMENT_GENERAL);

	lea_policy_clr_phone_addr_list();
}

static void lea_policy_add_reconnect_phone(void *param)
{
	struct lea_policy_phone_addr *addr_node;
	uint8_t reason = 0;
	bt_addr_le_t *addr = NULL;

	if (!param) {
		return;
	}

	reason = ((uint8_t *)param)[0];
	if (reason != 0x08) {
		return;
	}

	if (thread_timer_is_running(&lea_phone_reconnect_timer)) {
		thread_timer_stop(&lea_phone_reconnect_timer);
	}

	addr = (bt_addr_le_t *)(&((uint8_t *)param)[1]);

	addr_node = mem_malloc(sizeof(struct lea_policy_phone_addr));
	if (!addr_node) {
		SYS_LOG_ERR("no mem\n");
		return;
	}

	bt_addr_le_copy(&addr_node->addr, (const bt_addr_le_t *)addr);

	os_sched_lock();
	sys_slist_append(&lea_policy_phone_addr_list, &addr_node->node);
	os_sched_unlock();

    bt_manager_lea_set_cas_announcement_type(CAS_ANNOUNCEMENT_TARGETED);

	thread_timer_init(&lea_phone_reconnect_timer, phone_reconnect_timeout_handler, NULL);
	thread_timer_start(&lea_phone_reconnect_timer, LEA_RECONNECT_PHONE_TIMEOUT, 0);
	SYS_LOG_INF("recon_addr add:%x%x%x%x%x%x\n",ADDR_VAL(addr));
}

static void lea_policy_delete_reconnect_phone(void *param)
{
	struct lea_policy_phone_addr *addr_node;
	bt_addr_le_t *addr = NULL;

	if (!param) {
		SYS_LOG_ERR("no addr\n");
		return;
	}

    if (sys_slist_is_empty(&lea_policy_phone_addr_list)) {
        bt_manager_lea_set_cas_announcement_type(CAS_ANNOUNCEMENT_GENERAL);
        return;
    }

	addr = (bt_addr_le_t *)param;

    if(bt_manager_lea_is_reconnected_dev(addr->a.val) == true){
        bt_manager_lea_set_cas_announcement_type(CAS_ANNOUNCEMENT_GENERAL);
    }

	os_sched_lock();
	SYS_SLIST_FOR_EACH_CONTAINER(&lea_policy_phone_addr_list, addr_node, node) {
		if (!bt_addr_le_cmp(&addr_node->addr, addr)) {
			sys_slist_find_and_remove(&lea_policy_phone_addr_list,&addr_node->node);
			mem_free(addr_node);
			SYS_LOG_INF("recon_addr del:%x%x%x%x%x%x\n",ADDR_VAL(addr));
		}
	}

    if (sys_slist_is_empty(&lea_policy_phone_addr_list)) {
        bt_manager_lea_set_cas_announcement_type(CAS_ANNOUNCEMENT_GENERAL);
    }
	os_sched_unlock();
}

bool bt_manager_lea_is_reconnected_dev(uint8_t *addr_val)
{
	struct lea_policy_phone_addr *addr_node;

	if (!addr_val) {
		SYS_LOG_ERR("no addr_val\n");
		return false;
	}

	if (sys_slist_is_empty(&lea_policy_phone_addr_list)) {
		return false;
	}

	SYS_SLIST_FOR_EACH_CONTAINER(&lea_policy_phone_addr_list, addr_node, node) {
		if (!memcmp(addr_val,addr_node->addr.a.val, 6)) {
			SYS_LOG_INF("recon_addr:%x%x%x%x%x%x\n",ADDR_VAL(&addr_node->addr));
			return true;
		}
	}

	return false;
}

static void timeout_handler (
					struct thread_timer *ttimer,
				    void *expiry_fn_arg)
{
	if (!btmgr_lea_policy_ctx.policy_enable) {
		return;
	}

	lea_policy_timer_stop();

	uint8_t cur_lea_num = bt_manager_audio_get_leaudio_dev_num();
	if (cur_lea_num < btmgr_lea_policy_ctx.max_dev_num) {
		lea_set_policy_state(LEA_POLICY_STATE_WAITING);
		lea_set_visual_mode(LEA_RPA_UNDIRECT_LIMIT_CONN_GENERAL_DISC);
	} else {
		lea_connect_max_dev();
	}
}

static void enter_pairing_mode(void)
{
	SYS_LOG_INF("lea enter pair mode\n");
	lea_set_policy_state(LEA_POLICY_STATE_PAIRING);
	lea_set_visual_mode(LEA_PUB_UNDIRECT_OPEN_CONN_GENERAL_DISC);
	lea_policy_timer_start(PAIRING_DURATION_MS);
}

static int lea_policy_state_halt_resume_handler(btmgr_lea_policy_event_e event, void *param, uint8_t len)
{
	switch (event) {
		case LEA_POLICY_EVENT_CONNECT:
		{
			uint8_t cur_dev_num = bt_manager_audio_get_leaudio_dev_num();
			if (cur_dev_num >= btmgr_lea_policy_ctx.max_dev_num) {
				lea_connect_max_dev();
				break;
			}

			if (param) {
				bt_addr_le_copy(&btmgr_lea_policy_ctx.last_lea_dev_info, (const bt_addr_le_t *)param);
				/*check if need update adv data*/
				if (btmgr_lea_policy_ctx.halt_phone && cur_dev_num >= btmgr_lea_policy_ctx.halt_dev_num) {
					lea_set_policy_state(LEA_POLICY_STATE_WAITING);
					lea_set_visual_mode(LEA_RPA_UNDIRECT_LIMIT_CONN_GENERAL_DISC);
				}
			}
			break;
		}
		case LEA_POLICY_EVENT_DISCONNECT:
		{
			uint8_t expected_len = 1 + sizeof(bt_addr_le_t);
			if (NULL == param && 0 == len) {
				return 0;
			}

			if (expected_len != len) {
				SYS_LOG_ERR("param:%p,len:%d\n", param, len);
				return -1;
			}

            uint8_t reason = 0;
            if(param){
			    reason = ((uint8_t *)param)[0];
			}
			bt_addr_le_t *peer_addr = &btmgr_lea_policy_ctx.last_lea_dev_info;
			memcpy(peer_addr, &((uint8_t *)param)[1], sizeof(bt_addr_le_t));
			if (0x13 == reason
				|| 0x16 == reason) {
				break;
			}
			else {
				//linkloss
			}
			break;
		}
		case LEA_POLICY_EVENT_PAIRING:
		{
			enter_pairing_mode();
			break;
		}
		case LEA_POLICY_EVENT_EXIT_PAIRING:
		{
			timeout_handler(NULL,NULL);
			break;
		}
		case LEA_POLICY_EVENT_SWITCH_SINGLE_POINT:
		{
			btmgr_lea_policy_ctx.max_dev_num = 1;
			uint8_t cur_lea_num = bt_manager_audio_get_leaudio_dev_num();
			if (cur_lea_num >= btmgr_lea_policy_ctx.max_dev_num) {
				lea_connect_max_dev();
			}
			break;
		}
		case LEA_POLICY_EVENT_SWITCH_MULTI_POINT:
		{
			btmgr_lea_policy_ctx.max_dev_num = bt_manager_config_connect_phone_num();
			break;
		}
		default:
			SYS_LOG_INF("unkown evt:%d\n", event);
			return -1;
	}

	return 0;
}

static int lea_policy_state_waiting_handler(btmgr_lea_policy_event_e event, void *param, uint8_t len)
{
	switch (event) {
		case LEA_POLICY_EVENT_CONNECT:
		{
			uint8_t cur_dev_num = bt_manager_audio_get_leaudio_dev_num();
			bt_addr_le_t * addr = (bt_addr_le_t *)param;

			if (cur_dev_num >= btmgr_lea_policy_ctx.max_dev_num) {
				lea_connect_max_dev();
				break;
			}

			if (addr) {
				bt_addr_le_copy(&btmgr_lea_policy_ctx.last_lea_dev_info, addr);
				lea_set_visual_mode(LEA_RPA_UNDIRECT_LIMIT_CONN_GENERAL_DISC);
			}
			break;
		}
		case LEA_POLICY_EVENT_DISCONNECT:
		{
			uint8_t expected_len = 1 + sizeof(bt_addr_le_t);
			if (NULL == param && 0 == len) {
				return 0;
			}

			if (expected_len != len) {
				SYS_LOG_ERR("param:%p,len:%d\n", param, len);
				return -1;
			}

            uint8_t reason = 0;
            if(param){
			    reason = ((uint8_t *)param)[0];
			}
			bt_addr_le_t *peer_addr = &btmgr_lea_policy_ctx.last_lea_dev_info;
			memcpy(peer_addr, &((uint8_t *)param)[1], sizeof(bt_addr_le_t));
			if (0x13 == reason
				|| 0x16 == reason) {
				break;
			}
			else {
				/* reconnect*/
				lea_set_policy_state(LEA_POLICY_STATE_LINKLOSS_RECONNECTING);
				lea_set_visual_mode(LEA_RPA_UNDIRECT_LIMIT_CONN_TARGET_DISC);
				lea_policy_timer_start(DIR_ADV_DURATION_MS);
			}
			break;
		}
		case LEA_POLICY_EVENT_PAIRING:
		{
			enter_pairing_mode();
			break;
		}
		case LEA_POLICY_EVENT_EXIT_PAIRING:
		{
			timeout_handler(NULL,NULL);
			break;
		}
		case LEA_POLICY_EVENT_SWITCH_SINGLE_POINT:
		{
			btmgr_lea_policy_ctx.max_dev_num = 1;
			uint8_t cur_lea_num = bt_manager_audio_get_leaudio_dev_num();
			if (cur_lea_num >= btmgr_lea_policy_ctx.max_dev_num) {
				lea_connect_max_dev();
			}
			break;
		}
		case LEA_POLICY_EVENT_SWITCH_MULTI_POINT:
		{
			btmgr_lea_policy_ctx.max_dev_num = bt_manager_config_connect_phone_num();
			break;
		}
		default:
			SYS_LOG_INF("unkown evt:%d\n", event);
			return -1;
	}

	return 0;
}

static int lea_policy_state_pairing_handler(btmgr_lea_policy_event_e event, void *param, uint8_t len)
{
	switch (event) {
		case LEA_POLICY_EVENT_CONNECT:
		{
			if (param) {
				memcpy(&btmgr_lea_policy_ctx.last_lea_dev_info, param, sizeof(bt_addr_le_t));
			}

			uint8_t cur_dev_num = bt_manager_audio_get_leaudio_dev_num();
			if (cur_dev_num >= btmgr_lea_policy_ctx.max_dev_num) {
				lea_connect_max_dev();
			}else {
				lea_set_visual_mode(LEA_RPA_UNDIRECT_LIMIT_CONN_GENERAL_DISC);
			}
			break;
		}
		case LEA_POLICY_EVENT_PAIRING:
		{
			enter_pairing_mode();
			break;
		}
		case LEA_POLICY_EVENT_EXIT_PAIRING:
		{
			timeout_handler(NULL,NULL);
			break;
		}
		case LEA_POLICY_EVENT_SWITCH_SINGLE_POINT:
		{
			btmgr_lea_policy_ctx.max_dev_num = 1;
			uint8_t cur_lea_num = bt_manager_audio_get_leaudio_dev_num();
			if (cur_lea_num >= btmgr_lea_policy_ctx.max_dev_num) {
				lea_connect_max_dev();
			}
			break;
		}
		case LEA_POLICY_EVENT_SWITCH_MULTI_POINT:
		{
			btmgr_lea_policy_ctx.max_dev_num = bt_manager_config_connect_phone_num();
			break;
		}
		default:
			SYS_LOG_INF("unkown evt:%d\n", event);
			return -1;
	}
	return 0;
}

static int lea_policy_state_auto_reconnecting_handler(btmgr_lea_policy_event_e event, void *param, uint8_t len)
{
	switch (event) {
		case LEA_POLICY_EVENT_CONNECT:
		{
			uint8_t cur_dev_num = bt_manager_audio_get_leaudio_dev_num();
			if (cur_dev_num >= btmgr_lea_policy_ctx.max_dev_num) {
				lea_connect_max_dev();
			}
			break;
		}
		case LEA_POLICY_EVENT_DISCONNECT:
		{
			/* reconnect last device */
			uint8_t expected_len = 1 + sizeof(bt_addr_le_t);
			if (NULL == param && 0 == len) {
				return 0;
			}

			if (expected_len != len) {
				SYS_LOG_ERR("param:%p,len:%d\n", param, len);
				return -1;
			}
			uint8_t reason = ((uint8_t *)param)[0];
			bt_addr_le_t *peer_addr = &btmgr_lea_policy_ctx.last_lea_dev_info;
			memcpy(peer_addr, &((uint8_t *)param)[1], sizeof(bt_addr_le_t));
			if (0x13 == reason
				|| 0x16 == reason) {
				//todo
			} else {
				/* linkloss*/
				//todo
			}
			break;
		}
		case LEA_POLICY_EVENT_PAIRING:
		{
			enter_pairing_mode();
			break;
		}
		case LEA_POLICY_EVENT_SWITCH_SINGLE_POINT:
		{
			btmgr_lea_policy_ctx.max_dev_num = 1;
			uint8_t cur_lea_num = bt_manager_audio_get_leaudio_dev_num();
			if (cur_lea_num >= btmgr_lea_policy_ctx.max_dev_num) {
				lea_connect_max_dev();
			}
			break;
		}
		case LEA_POLICY_EVENT_SWITCH_MULTI_POINT:
		{
			btmgr_lea_policy_ctx.max_dev_num = bt_manager_config_connect_phone_num();
			break;
		}
		default:
			SYS_LOG_INF("unkown evt:%d\n", event);
			return -1;
	}

	return 0;
}

static int lea_policy_state_linkloss_reconnecting_handler(btmgr_lea_policy_event_e event, void *param, uint8_t len)
{
	switch (event) {
		case LEA_POLICY_EVENT_CONNECT:
		{
			uint8_t cur_dev_num = bt_manager_audio_get_leaudio_dev_num();

			if (cur_dev_num >= btmgr_lea_policy_ctx.max_dev_num) {
				lea_connect_max_dev();
				break;
			}

			/* reconnect last device */
			uint8_t expected_len = sizeof(bt_addr_le_t);
			if (NULL == param && 0 == len) {
				return 0;
			}
			if (expected_len != len) {
				SYS_LOG_ERR("param:%p,len:%d\n", param, len);
				return -1;
			}

			if (param && !bt_addr_le_cmp((const bt_addr_le_t*)param, &btmgr_lea_policy_ctx.last_lea_dev_info)) {
				timeout_handler(NULL, NULL);
			}

			break;
		}
		case LEA_POLICY_EVENT_DISCONNECT:
		{
			/* reconnect last device */
			uint8_t expected_len = 1 + sizeof(bt_addr_le_t);
			if (NULL == param && 0 == len) {
				return 0;
			}

			if (expected_len != len) {
				SYS_LOG_ERR("param:%p,len:%d\n", param, len);
				return -1;
			}
			uint8_t reason = ((uint8_t *)param)[0];
			bt_addr_le_t *peer_addr = &btmgr_lea_policy_ctx.last_lea_dev_info;
			memcpy(peer_addr, &((uint8_t *)param)[1], sizeof(bt_addr_le_t));
			if (0x13 == reason
				|| 0x16 == reason) {
				break;
			} else {
				/* reconnect */
				lea_set_policy_state(LEA_POLICY_STATE_LINKLOSS_RECONNECTING);
				lea_set_visual_mode(LEA_RPA_DIRECT_LIMIT_CONN_TARGET_DISC);
				lea_policy_timer_start(DIR_ADV_DURATION_MS);
			}
			break;
		}
		case LEA_POLICY_EVENT_PAIRING:
		{
			enter_pairing_mode();
			break;
		}
		case LEA_POLICY_EVENT_SWITCH_SINGLE_POINT:
		{
			btmgr_lea_policy_ctx.max_dev_num = 1;
			uint8_t cur_lea_num = bt_manager_audio_get_leaudio_dev_num();
			if (cur_lea_num >= btmgr_lea_policy_ctx.max_dev_num) {
				lea_connect_max_dev();
			}
			break;
		}
		case LEA_POLICY_EVENT_SWITCH_MULTI_POINT:
		{
			btmgr_lea_policy_ctx.max_dev_num = bt_manager_config_connect_phone_num();
			break;
		}
		default:
			SYS_LOG_INF("unkown evt:%d\n", event);
			return -1;
	}

	return 0;
}

static int lea_policy_state_connected_handler(btmgr_lea_policy_event_e event, void *param, uint8_t len)
{
	switch (event) {
		case LEA_POLICY_EVENT_DISCONNECT:
		{
			uint8_t cur_lea_num = bt_manager_audio_get_leaudio_dev_num();
			if (cur_lea_num >= btmgr_lea_policy_ctx.max_dev_num) {
				lea_connect_max_dev();
				break;
			}

			uint8_t expected_len = 1 + sizeof(bt_addr_le_t);
			if (expected_len == len) {
				uint8_t reason = ((uint8_t *)param)[0];
				bt_addr_le_t *peer_addr = &btmgr_lea_policy_ctx.last_lea_dev_info;
				memcpy(peer_addr, &((uint8_t *)param)[1], sizeof(bt_addr_le_t));
				if (0x13 == reason
					|| 0x16 == reason) {
					lea_set_policy_state(LEA_POLICY_STATE_WAITING);
					lea_set_visual_mode(LEA_RPA_UNDIRECT_LIMIT_CONN_GENERAL_DISC);
				} else {
					lea_set_policy_state(LEA_POLICY_STATE_LINKLOSS_RECONNECTING);
					lea_set_visual_mode(LEA_RPA_DIRECT_LIMIT_CONN_TARGET_DISC);
					lea_policy_timer_start(DIR_ADV_DURATION_MS);
				}
			}
			break;
		}
		case LEA_POLICY_EVENT_PAIRING:
		{
			enter_pairing_mode();
			break;
		}
		case LEA_POLICY_EVENT_SWITCH_SINGLE_POINT:
		{
			btmgr_lea_policy_ctx.max_dev_num = 1;
			uint8_t cur_lea_num = bt_manager_audio_get_leaudio_dev_num();
			if (cur_lea_num >= btmgr_lea_policy_ctx.max_dev_num) {
				lea_connect_max_dev();
			}
			break;
		}
		case LEA_POLICY_EVENT_SWITCH_MULTI_POINT:
		{
			btmgr_lea_policy_ctx.max_dev_num = bt_manager_config_connect_phone_num();
			uint8_t cur_lea_num = bt_manager_audio_get_leaudio_dev_num();
			if (cur_lea_num < btmgr_lea_policy_ctx.max_dev_num) {
				lea_set_policy_state(LEA_POLICY_STATE_WAITING);
				lea_set_visual_mode(LEA_RPA_UNDIRECT_LIMIT_CONN_GENERAL_DISC);
			}
			break;
		}
		default:
			SYS_LOG_INF("unkown evt:%d\n", event);
			return -1;
	}

	return 0;
}

static void lea_policy_clear_list_handler(void)
{
	lea_policy_timer_stop();
	hostif_bt_le_clear_list();
	bt_manager_lea_set_cas_announcement_type(CAS_ANNOUNCEMENT_GENERAL);

	SYS_LOG_INF("clear_list: %d\n", btmgr_lea_policy_ctx.lea_policy_state);
	if (btmgr_lea_policy_ctx.policy_enable) {
		lea_set_policy_state(LEA_POLICY_STATE_WAITING);
		lea_set_visual_mode(LEA_RPA_UNDIRECT_LIMIT_CONN_GENERAL_DISC);
	}
}

static void lea_policy_bt_off_handler(void)
{
	/* Disable discoverable/connectable, stop pair, stop reconnect. */
	lea_policy_timer_stop();
	lea_set_policy_state(LEA_POLICY_STATE_MAX_NUM);
	lea_set_visual_mode(LEA_NON_CONN_NON_DISC);
	bt_manager_ble_disconnect_all_link();
	lea_policy_clr_phone_addr_list();
	memset(&btmgr_lea_policy_ctx.last_lea_dev_info, 0, sizeof(bt_addr_le_t));
	btmgr_lea_policy_ctx.btoff = 1;
	SYS_LOG_INF("policy state:%d\n", btmgr_lea_policy_ctx.lea_policy_state);
}

static void lea_policy_bt_on_handler(void)
{
	//bt_on: default enable policy
	btmgr_lea_policy_ctx.btoff = 0;
	btmgr_lea_policy_ctx.policy_enable = 1;
	hostif_bt_ctkd_enable(1);

	lea_set_policy_state(LEA_POLICY_STATE_WAITING);
	lea_set_visual_mode(LEA_RPA_UNDIRECT_LIMIT_CONN_GENERAL_DISC);
	SYS_LOG_INF("state:%d,max dev:%d\n", btmgr_lea_policy_ctx.lea_policy_state, btmgr_lea_policy_ctx.max_dev_num);
}

void bt_manager_lea_policy_ctx_init(uint8_t enable)
{
	os_sched_lock();
	memset(&btmgr_lea_policy_ctx, 0, sizeof(btmgr_lea_policy_ctx));
	btmgr_lea_policy_ctx.funcs = lea_policy_state_event_handler;
	btmgr_lea_policy_ctx.max_dev_num = bt_manager_config_connect_phone_num();

	btmgr_lea_policy_ctx.lea_policy_state = LEA_POLICY_STATE_MAX_NUM;
	btmgr_lea_policy_ctx.lea_visual_mode = LEA_POLICY_VISUAL_MAX_NUM;
	btmgr_lea_policy_ctx.address_resolution = ADDRESS_RESOLUTION_MAX_NUM;
	btmgr_lea_policy_ctx.cas_announcement_type = CAS_ANNOUNCEMENT_GENERAL;

	lea_policy_clr_phone_addr_list();
	os_sched_unlock();

	if (!enable) {
		return;
	}
	bt_manager_lea_policy_event_notify(LEA_POLICY_EVENT_BT_ON, NULL, 0);
}

struct bt_le_adv_param * bt_manager_lea_policy_get_adv_param(uint8_t adv_type, struct bt_le_adv_param* param_buf)
{
	struct bt_le_adv_param *param = param_buf;
	uint32_t last_options = btmgr_lea_policy_ctx.option;
	btmgr_lea_policy_state_e lea_policy_state = btmgr_lea_policy_ctx.lea_policy_state;
	uint8_t directed = 0;
	uint8_t limited = 0;
	uint8_t rpa_type = 1;

	if (!param){
		SYS_LOG_ERR("param buff is null\n");
		return NULL;
	}

	memset(param_buf, 0, sizeof(struct bt_le_adv_param));

    //adv Common param
	param->id = BT_ID_DEFAULT;
	param->interval_min = BT_GAP_ADV_FAST_INT_MIN_2;
	param->interval_max = BT_GAP_ADV_FAST_INT_MAX_2;
	param->options = BT_LE_ADV_OPT_CONNECTABLE;

	if (ADV_TYPE_EXT == adv_type) {
		param->options |= (BT_LE_ADV_OPT_EXT_ADV | BT_LE_ADV_OPT_NO_2M);
		param->sid = BT_EXT_ADV_SID_UNICAST;
	}

	if (!btmgr_lea_policy_ctx.policy_enable) {
#ifdef CONFIG_BT_PRIVACY
		param->options |= BT_LE_ADV_OPT_ADDR_RPA;
#endif
		return param;
	}

	directed = lea_get_adv_directed(btmgr_lea_policy_ctx.lea_visual_mode);
	limited = lea_get_adv_limited(btmgr_lea_policy_ctx.lea_visual_mode);
	rpa_type = lea_get_addr_type(btmgr_lea_policy_ctx.lea_visual_mode);

	/*Direct connectable Controller-gen rpa adv*/
	if (directed) {
		if (ADV_TYPE_EXT != adv_type) {
//			SYS_LOG_ERR("reconnecting should send ext ADV!!!\n");
			return NULL;
		}

		param->options |= (BT_LE_ADV_OPT_USE_IDENTITY | BT_LE_ADV_OPT_DIR_MODE_LOW_DUTY);
#ifdef CONFIG_BT_PRIVACY
		param->options |= BT_LE_ADV_OPT_DIR_ADDR_RPA;
#endif
		if (limited) {
#ifdef CONFIG_BT_WHITELIST
			param->options |= BT_LE_ADV_OPT_FILTER_CONN;
#endif
		}
		param->interval_min = BT_GAP_ADV_FAST_INT_MIN_1;
		param->interval_max = BT_GAP_ADV_FAST_INT_MAX_1;
		param->peer = &btmgr_lea_policy_ctx.last_lea_dev_info;

		char addr_str[BT_ADDR_LE_STR_LEN];
		bt_addr_le_to_str(&btmgr_lea_policy_ctx.last_lea_dev_info, addr_str, sizeof(addr_str));
		SYS_LOG_INF("reconnect_addr:%s\n", addr_str);
	}else {
		if (rpa_type) {
#ifdef CONFIG_BT_PRIVACY
			param->options |= BT_LE_ADV_OPT_ADDR_RPA;
#endif
			/*Undirect connectable Controller-gen rpa adv*/
			if (bt_addr_le_cmp(&btmgr_lea_policy_ctx.last_lea_dev_info, BT_ADDR_LE_ANY)) {
				param->peer = &btmgr_lea_policy_ctx.last_lea_dev_info;
				param->options |= BT_LE_ADV_OPT_USE_IDENTITY;
//				SYS_LOG_INF("USE_IDENTITY: %x\n", lea_policy_state);
			}
		}else {
			param->options |= BT_LE_ADV_OPT_USE_IDENTITY;
		}

		if (limited) {
#ifdef CONFIG_BT_WHITELIST
			param->options |= BT_LE_ADV_OPT_FILTER_CONN;
#endif
		}
	}

	if (last_options != param->options) {
		SYS_LOG_INF("op chg:%x->%x, %x, %x, %x, %x\n", last_options, param->options, 
					lea_policy_state, directed, limited, rpa_type);
		btmgr_lea_policy_ctx.option = param->options;
	}

	if (param->peer != NULL && (ADV_TYPE_EXT == adv_type)) {
		char addr_str[BT_ADDR_LE_STR_LEN];
		bt_addr_le_to_str(param->peer, addr_str, sizeof(addr_str));
		SYS_LOG_INF("addr:%s\n", addr_str);

	}

	return param;
}

void bt_manager_lea_clear_paired_list(void)
{
	bt_manager_lea_policy_event_notify(LEA_POLICY_EVENT_CLIST_PAIRED_LIST, NULL, 0);
}

uint8_t bt_manager_lea_set_active_device(uint16_t handle)
{
	SYS_LOG_INF("lea_set_active_device:%x\n", handle);
	return ctrl_cis_set_active_device(handle);
}

uint8_t bt_manager_lea_set_device_policy(uint16_t handle, uint32_t policy)
{
	SYS_LOG_INF("lea_set_device_policy:%x,policy:%d\n", handle, policy);
	return ctrl_cis_set_device_policy(handle,policy);
}

static void bt_manager_lea_policy_event_cb(void *event_param)
{
	lea_policy_event_param_t *ptr = (lea_policy_event_param_t *)event_param;

	SYS_LOG_INF("evt:%d,state:%d\n", ptr->event, btmgr_lea_policy_ctx.lea_policy_state);
	switch (ptr->event) {
		case LEA_POLICY_EVENT_BT_OFF:
			lea_policy_bt_off_handler();
			break;

		case LEA_POLICY_EVENT_BT_ON:
			lea_policy_bt_on_handler();
			break;

		case LEA_POLICY_EVENT_ENABLE:
            if(ptr->param){
				lea_policy_enable(((uint8_t *)ptr->param)[0]);
			}
			break;

		case LEA_POLICY_EVENT_DISABLE:
			if(ptr->param){
				lea_policy_disable(((uint8_t *)ptr->param)[0]);
			}
			break;

		case LEA_POLICY_EVENT_CLIST_PAIRED_LIST:
			lea_policy_clear_list_handler();
			break;

		case LEA_POLICY_EVENT_AUTO_RECONNECT:
			if ((btmgr_lea_policy_ctx.policy_enable) && (ptr->param)) {
				bt_addr_le_copy(&btmgr_lea_policy_ctx.last_lea_dev_info, (const bt_addr_le_t *)ptr->param);

				lea_set_policy_state(LEA_POLICY_STATE_AUTO_RECONNECTING);
				lea_set_visual_mode(LEA_RPA_DIRECT_LIMIT_CONN_TARGET_DISC);
				lea_policy_timer_start(DIR_ADV_DURATION_MS);
			}
			break;

		default:
			if (btmgr_lea_policy_ctx.policy_enable) {
				if (LEA_POLICY_EVENT_CONNECT == ptr->event) {
					lea_policy_delete_reconnect_phone(ptr->param);
				} else if (LEA_POLICY_EVENT_DISCONNECT == ptr->event) {
					lea_policy_add_reconnect_phone(ptr->param);
				}
				if (btmgr_lea_policy_ctx.funcs && !btmgr_lea_policy_ctx.btoff &&
					btmgr_lea_policy_ctx.lea_policy_state < LEA_POLICY_STATE_MAX_NUM) {
					btmgr_lea_policy_ctx.funcs[btmgr_lea_policy_ctx.lea_policy_state]
						(
							ptr->event,
							ptr->param,
							ptr->len
						);
				}else {
					SYS_LOG_WRN("%p, %x, %x\n", btmgr_lea_policy_ctx.funcs,
								btmgr_lea_policy_ctx.btoff, btmgr_lea_policy_ctx.lea_policy_state);
				}
			}
			break;
	}

	if (btmgr_lea_policy_ctx.max_dev_num == 1
		&& bt_manager_audio_get_leaudio_dev_num()
		&& !btmgr_lea_policy_ctx.btoff) {
		uint8_t connectable = 0;
		uint8_t bt_link_num = bt_manager_get_connected_dev_num();
		connectable = (bt_link_num == 0) ? 1 : 0;
		/* notify le audio to handle BT scan mode update */
		SYS_LOG_INF("bt connectable:%d\n", connectable);
		bt_manager_event_notify(BT_AUDIO_UPDATE_SCAN_MODE, &connectable, sizeof(connectable));
	}

	if (ptr->param) {
		mem_free(ptr->param);
	}

	SYS_LOG_INF("state:%d\n", btmgr_lea_policy_ctx.lea_policy_state);
}

void bt_manager_lea_policy_event_notify(btmgr_lea_policy_event_e event, void *param, uint8_t len)
{
	if ((event >= LEA_POLICY_EVENT_PAIRING) &&
		(!btmgr_lea_policy_ctx.policy_enable)) {
		return;
	}
	if (event >= LEA_POLICY_EVENT_MAX_NUM) {
		SYS_LOG_ERR("evt:%d\n", event);
		return;
	}
	void *param_ptr = NULL;
	int ret = 0;

	if (len) {
		param_ptr = mem_malloc(len);
		if (param_ptr == NULL) {
			SYS_LOG_ERR("alloc failed\n");
			return;
		}
		memcpy(param_ptr, param, len);
	}

	lea_policy_event_param_t evt_param = {0};
	evt_param.event = event;
	evt_param.param = param_ptr;
	evt_param.len = len;

	ret = btif_audio_cb_event_notify(bt_manager_lea_policy_event_cb, &evt_param, sizeof(evt_param));
	if (ret && param_ptr) {
		SYS_LOG_ERR("notify send failed\n");
		mem_free(param_ptr);
	}

	SYS_LOG_DBG("evt:%d,ret:%d\n", event, ret);
}

void bt_manager_lea_policy_disable(uint8_t adv_enable)
{
	if (!btmgr_lea_policy_ctx.policy_enable) {
		return;
	}
	bt_manager_lea_policy_event_notify(LEA_POLICY_EVENT_DISABLE, &adv_enable, sizeof(uint8_t));
}

void bt_manager_lea_policy_enable(uint8_t halt_resume)
{
	if (btmgr_lea_policy_ctx.policy_enable) {
		return;
	}
	bt_manager_lea_policy_event_notify(LEA_POLICY_EVENT_ENABLE, &halt_resume, sizeof(uint8_t));
}

void bt_manager_lea_halt_phone(void)
{
	if (!btmgr_lea_policy_ctx.policy_enable) {
		btmgr_lea_policy_ctx.halt_phone = 0;
		btmgr_lea_policy_ctx.halt_dev_num = 0;
		return;
	}

	SYS_LOG_INF("halt: %x\n", btmgr_lea_policy_ctx.halt_phone);
	btmgr_lea_policy_ctx.halt_dev_num = bt_manager_audio_get_leaudio_dev_num();
	if (btmgr_lea_policy_ctx.halt_dev_num) {
		btmgr_lea_policy_ctx.halt_phone = 1;
	}
	bt_manager_disconnect_lea_audio_conn();
	bt_manager_lea_policy_disable(0);
}

void bt_manager_lea_resume_phone(void)
{
	//app already enable
	if (btmgr_lea_policy_ctx.policy_enable) {
		btmgr_lea_policy_ctx.halt_phone = 0;
		btmgr_lea_policy_ctx.halt_dev_num = 0;
		SYS_LOG_WRN("%x\n", btmgr_lea_policy_ctx.policy_enable);
		return;
	}

	SYS_LOG_INF("resume: %x\n", btmgr_lea_policy_ctx.halt_phone);
	if((btmgr_lea_policy_ctx.halt_phone) &&
		(btmgr_lea_policy_ctx.halt_dev_num)){
		bt_manager_lea_policy_enable(1);
	}else{
		btmgr_lea_policy_ctx.halt_phone = 0;
		btmgr_lea_policy_ctx.halt_dev_num = 0;
		bt_manager_lea_policy_enable(0);
	}
}

uint8_t bt_manager_lea_get_halt_phone(void)
{
	return btmgr_lea_policy_ctx.halt_dev_num;
}

uint8_t bt_manager_lea_get_cas_announcement_type(void)
{
	return btmgr_lea_policy_ctx.cas_announcement_type;
}

void bt_manager_lea_set_cas_announcement_type(uint8_t type)
{
    btmgr_lea_policy_visual_mode_e visual_mode;

    if((type != CAS_ANNOUNCEMENT_GENERAL) && (type != CAS_ANNOUNCEMENT_TARGETED)){
        return;
    }

    if(btmgr_lea_policy_ctx.cas_announcement_type != type){
        btmgr_lea_policy_ctx.cas_announcement_type = type;
        btmgr_lea_policy_ctx.cas_update = 1;
        visual_mode = lea_get_visual_mode();
        lea_set_visual_mode(visual_mode);
        SYS_LOG_INF("cas announcement:%d",type);
    }
}

uint8_t bt_manager_lea_is_connectable(void)
{
	if ((btmgr_lea_policy_ctx.lea_visual_mode == LEA_NON_CONN_NON_DISC) ||
		(btmgr_lea_policy_ctx.lea_visual_mode == LEA_POLICY_VISUAL_MAX_NUM)) {
		return 0;
	}

	return 1;
}
