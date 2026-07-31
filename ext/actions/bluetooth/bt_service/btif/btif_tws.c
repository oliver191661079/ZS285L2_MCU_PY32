/*
 * Copyright (c) 2016 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt srv tws api interface
 */

#define SYS_LOG_DOMAIN "btif_tws"
#include "btsrv_os_common.h"
#include "btsrv_inner.h"

int btif_tws_register_processer(void)
{
#ifdef CONFIG_SUPPORT_TWS
	return btsrv_register_msg_processer(MSG_BTSRV_TWS, &btsrv_tws_process);
#else
	return 0;
#endif
}

int btif_tws_context_init(void)
{
#ifdef CONFIG_SUPPORT_TWS
	int ret, flags;
	flags = btsrv_set_negative_prio();
	ret = btsrv_tws_context_init();
	btsrv_revert_prio(flags);
	return ret;
#else
    return 0;
#endif
}

int btif_tws_init(tws_init_param *param)
{
	return btsrv_function_call_malloc(MSG_BTSRV_TWS, MSG_BTSRV_TWS_INIT, (void *)param, sizeof(tws_init_param), 0);
}

int btif_tws_deinit(void)
{
	return btsrv_function_call(MSG_BTSRV_TWS, MSG_BTSRV_TWS_DEINIT,NULL);
}

int btif_tws_start_pair(int try_times)
{
	return btsrv_function_call(MSG_BTSRV_TWS, MSG_BTSRV_TWS_START_PAIR, (void *)try_times);
}

int btif_tws_end_pair(void)
{
	return btsrv_function_call(MSG_BTSRV_TWS, MSG_BTSRV_TWS_END_PAIR, NULL);
}

int btif_tws_disconnect(bd_address_t *addr)
{
	return btsrv_function_call_malloc(MSG_BTSRV_TWS, MSG_BTSRV_TWS_DISCONNECT, (void *)addr, sizeof(bd_address_t), 0);
}

int btif_tws_can_do_pair(void)
{
#ifdef CONFIG_SUPPORT_TWS
	int ret, flags;

	flags = btsrv_set_negative_prio();
	ret = btsrv_tws_can_do_pair();
	btsrv_revert_prio(flags);

	return ret;
#else
	return 0;
#endif
}

int btif_tws_is_in_connecting(void)
{
#ifdef CONFIG_SUPPORT_TWS
	int ret, flags;

	flags = btsrv_set_negative_prio();
	ret = btsrv_tws_is_in_connecting();
	btsrv_revert_prio(flags);

	return ret;
#else
	return 0;
#endif
}

int btif_tws_is_in_pair_seraching(void)
{
#ifdef CONFIG_SUPPORT_TWS
	int ret, flags;

	flags = btsrv_set_negative_prio();
	ret = btsrv_tws_is_in_pair_searching();
	btsrv_revert_prio(flags);

	return ret;
#else
	return 0;
#endif
}


int btif_tws_get_dev_role(void)
{
	int ret, flags;

	flags = btsrv_set_negative_prio();
	ret = btsrv_rdm_get_dev_role();
	btsrv_revert_prio(flags);

	return ret;
}

int btif_tws_set_input_stream(io_stream_t stream)
{
#ifdef CONFIG_SUPPORT_TWS
	int flags;

	flags = btsrv_set_negative_prio();
	btsrv_tws_set_input_stream(stream, true);
	btsrv_revert_prio(flags);

	return 0;
#endif
	return 0;
}

int btif_tws_set_sco_input_stream(io_stream_t stream)
{
#ifdef CONFIG_SUPPORT_TWS
	int flags;

	flags = btsrv_set_negative_prio();
	btsrv_tws_set_sco_input_stream(stream);
	btsrv_revert_prio(flags);

	return 0;
#endif
	return 0;
}

tws_runtime_observer_t *btif_tws_get_tws_observer(void)
{
#ifdef CONFIG_SUPPORT_TWS
	int flags;
	tws_runtime_observer_t *observer;

	flags = btsrv_set_negative_prio();
	observer = btsrv_tws_monitor_get_observer();
	btsrv_revert_prio(flags);

	return observer;
#else
	return NULL;
#endif
}

int btif_tws_send_command(uint8_t *data, int len)
{
#ifdef CONFIG_SUPPORT_TWS
	int ret, flags;

	flags = btsrv_set_negative_prio();
	ret = btsrv_tws_send_user_command(data, len);
	btsrv_revert_prio(flags);

	return ret;
#else
	return 0;
#endif
}

int btif_tws_send_command_sync(uint8_t *data, int len)
{
#ifdef CONFIG_SUPPORT_TWS
	int ret, flags;

	flags = btsrv_set_negative_prio();
	ret = btsrv_tws_send_user_command_sync(data, len);
	btsrv_revert_prio(flags);

	return ret;
#else
	return 0;
#endif
}

uint32_t btif_tws_get_bt_clock(void)
{
#ifdef CONFIG_SUPPORT_TWS
	int flags;
	uint32_t clock;

	flags = btsrv_set_negative_prio();
	clock = btsrv_tws_get_bt_clock();
	btsrv_revert_prio(flags);

	return clock;
#else
	return 0xFFFFFFFF;
#endif
}

int btif_tws_set_irq_time(uint32_t bt_clock_ms)
{
#ifdef CONFIG_SUPPORT_TWS
	int flags, ret;

	flags = btsrv_set_negative_prio();
	ret = btsrv_tws_set_irq_time(bt_clock_ms);
	btsrv_revert_prio(flags);

	return ret;
#else
	return -EIO;
#endif
}

int btif_tws_wait_ready_send_sync_msg(uint32_t wait_timeout)
{
#ifdef CONFIG_SUPPORT_TWS
	int flags, ret;

	flags = btsrv_set_negative_prio();
	ret = btsrv_tws_wait_ready_send_sync_msg(wait_timeout);
	btsrv_revert_prio(flags);

	return ret;
#else
	return 0;
#endif
}

int btif_tws_set_bt_local_play(bool bt_play, bool local_play)
{
#ifdef CONFIG_SUPPORT_TWS
	int ret, flags;

	flags = btsrv_set_negative_prio();
	ret = btsrv_tws_set_bt_local_play(bt_play, local_play);
	btsrv_revert_prio(flags);

	return ret;
#endif
	return 0;
}

void btif_tws_update_tws_mode(uint8_t tws_mode, uint8_t drc_mode)
{
#ifdef CONFIG_SUPPORT_TWS
	int flags;

	flags = btsrv_set_negative_prio();
	btsrv_tws_updata_tws_mode(tws_mode, drc_mode);
	btsrv_revert_prio(flags);
#endif
}

bool btif_tws_check_is_woodpecker(void)
{
#ifdef CONFIG_SUPPORT_TWS
	int flags;
	bool ret;

	flags = btsrv_set_negative_prio();
	ret = btsrv_tws_check_is_woodpecker();
	btsrv_revert_prio(flags);

	return ret;
#else
	/* Not support tws, self as woodpecker */
	return true;
#endif
}

bool btif_tws_check_support_feature(uint32_t feature)
{
#ifdef CONFIG_SUPPORT_TWS
	int flags;
	bool ret;

	flags = btsrv_set_negative_prio();
	ret = btsrv_tws_check_support_feature(feature);
	btsrv_revert_prio(flags);

	return ret;
#else
	/* Not support tws,, self as support */
	return true;
#endif
}

void btif_tws_set_codec(uint8_t codec)
{
#ifdef CONFIG_SUPPORT_TWS
	int flags;

	flags = btsrv_set_negative_prio();
	btsrv_tws_set_codec(codec);
	btsrv_revert_prio(flags);
#endif
}

void btif_tws_set_expect_role(uint8_t role)
{
#ifdef CONFIG_SUPPORT_TWS
	int flags;
	flags = btsrv_set_negative_prio();
	btsrv_tws_set_expect_role(role);
	btsrv_revert_prio(flags);
#endif
}

int btif_tws_carete_snoop_link(void)
{
	return btsrv_function_call(MSG_BTSRV_TWS, MSG_BTSRV_TWS_DO_SNOOP_CONNECT, NULL);
}

int btif_tws_disconnect_snoop_link(void)
{
	return btsrv_function_call(MSG_BTSRV_TWS, MSG_BTSRV_TWS_DO_SNOOP_DISCONNECT, NULL);
}

int btif_tws_switch_snoop_link(void)
{
#ifdef CONFIG_SUPPORT_TWS_ROLE_CHANGE
	return btsrv_function_call(MSG_BTSRV_TWS, MSG_BTSRV_TWS_SWITCH_SNOOP_LINK, NULL);
#else
    return 0;
#endif 	
}

void btif_tws_notify_start_power_off(void)
{
#ifdef CONFIG_SUPPORT_TWS
	int flags;
	flags = btsrv_set_negative_prio();
	btsrv_tws_notify_start_power_off();
	btsrv_revert_prio(flags);
#endif
}

void btif_tws_set_visual_param(tws_visual_parameter_t *param)
{
#ifdef CONFIG_SUPPORT_TWS
	int flags;
	flags = btsrv_set_negative_prio();
	btsrv_tws_set_visual_param(param);
	btsrv_revert_prio(flags);
#endif
}

tws_visual_parameter_t * btif_tws_get_visual_param(void)
{
#ifdef CONFIG_SUPPORT_TWS
	int flags;
	tws_visual_parameter_t *param;
	flags = btsrv_set_negative_prio();
	param = btsrv_tws_get_visual_param();
	btsrv_revert_prio(flags);
	return param;
#else
    return NULL;
#endif
}

int btif_tws_set_state(uint8_t state)
{
	return btsrv_function_call(MSG_BTSRV_TWS, MSG_BTSRV_TWS_SET_STATE, (void*)(int)state);
}

uint8_t btif_tws_get_state(void)
{
	int ret, flags;

	flags = btsrv_set_negative_prio();
	ret = btsrv_tws_get_state();
	btsrv_revert_prio(flags);

	return ret;
}

int btif_tws_update_visual_pair(void)
{
	return btsrv_function_call(MSG_BTSRV_TWS, MSG_BTSRV_TWS_UPDATE_VISUAL_PAIR, NULL);
}

void btif_tws_set_pair_param(tws_pair_parameter_t *param)
{
#ifdef CONFIG_SUPPORT_TWS
	int flags;
	flags = btsrv_set_negative_prio();
	btsrv_tws_set_pair_param(param);
	btsrv_revert_prio(flags);
#endif
}

void btif_tws_set_pair_keyword(uint8_t pair_keyword)
{
#ifdef CONFIG_SUPPORT_TWS
	int flags;
	flags = btsrv_set_negative_prio();
	btsrv_tws_set_pair_keyword(pair_keyword);
	btsrv_revert_prio(flags);
#endif
}

uint8_t btif_tws_get_pair_keyword(void)
{
#ifdef CONFIG_SUPPORT_TWS
	int flags;
	uint8_t keyword;
	flags = btsrv_set_negative_prio();
	keyword = btsrv_tws_get_pair_keyword();
	btsrv_revert_prio(flags);
	return keyword;
#else
    return 0;
#endif
}

void btif_tws_set_pair_addr_match(bool enable,bd_address_t *addr)
{
#ifdef CONFIG_SUPPORT_TWS
	int flags;
	flags = btsrv_set_negative_prio();
	btsrv_tws_set_pair_addr_match(enable,addr);
	btsrv_revert_prio(flags);
#endif
}

uint8_t btif_tws_get_pair_addr_match(void)
{
#ifdef CONFIG_SUPPORT_TWS
	int flags;
	uint8_t addr_match;
	flags = btsrv_set_negative_prio();
	addr_match = btsrv_tws_get_pair_addr_match();
	btsrv_revert_prio(flags);
	return addr_match;
#else
    return 0;
#endif
}

void btif_tws_set_rssi_threshold(int8_t threshold)
{
#ifdef CONFIG_SUPPORT_TWS
	int flags;
	flags = btsrv_set_negative_prio();
	btsrv_tws_set_rssi_threshold(threshold);
	btsrv_revert_prio(flags);
#endif
}

void btif_tws_set_leaudio_active(int8_t leaudio_active)
{
#ifdef CONFIG_SUPPORT_TWS
	int flags;
	flags = btsrv_set_negative_prio();
	btsrv_tws_set_leaudio_active(leaudio_active);
	btsrv_revert_prio(flags);
#endif
}

