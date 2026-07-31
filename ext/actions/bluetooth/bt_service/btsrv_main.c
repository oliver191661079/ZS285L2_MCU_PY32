/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt service interface
 */

#define SYS_LOG_DOMAIN "btsrv_main"
#include "btsrv_os_common.h"
#include "btsrv_inner.h"

#define MAX_BTSRV_PROCESSER		(MSG_BTSRV_MAX - MSG_BTSRV_BASE)
#define BTSRV_PORC_MONITOR_TIME	(5000)		/* unit:us */

static uint8_t btstack_ready_flag;
static btsrv_msg_process msg_processer[MAX_BTSRV_PROCESSER];



const char *bt_service_evt2str(int num, int max_num, const btsrv_event_strmap_t *strmap)
{
	int low = 0;
	int hi = max_num - 1;
	int mid;

	do {
		mid = (low + hi) >> 1;
		if (num > strmap[mid].event) {
			low = mid + 1;
		} else if (num < strmap[mid].event) {
			hi = mid - 1;
		} else {
			return strmap[mid].str;
		}
	} while (low <= hi);

	printk("evt num %d\n", num);

	return "Unknown!";
}


#if (CONFIG_DEBUG_BT_STACK == 0)
static int _bt_service_init(struct app_msg *msg)
{
	btsrv_callback cb = _btsrv_get_msg_param_callback(msg);

	btstack_ready_flag = 0;
	if (!btsrv_adapter_init(cb)) {
		SYS_LOG_ERR("btstack_init failed\n");
		return -EAGAIN;
	}

	btsrv_adapter_run();

	if (cb) {
		/* wait for ready */
		while (!btstack_ready_flag)
			os_sleep(10);
	}

	return 0;
}

static int _bt_service_exit(void)
{
	btsrv_adapter_stop();
	srv_manager_thread_exit(BLUETOOTH_SERVICE_NAME);

	return 0;
}
#endif

void bt_service_set_bt_ready(void)
{
	btstack_ready_flag = 1;
}

int btsrv_register_msg_processer(uint8_t msg_type, btsrv_msg_process processer)
{
	if ((msg_type < MSG_BTSRV_BASE) || (msg_type >= MSG_BTSRV_MAX) || !processer) {
		SYS_LOG_WRN("Unknow processer %p or msg_type %d\n", processer, msg_type);
		return -EINVAL;
	}

	msg_processer[msg_type - MSG_BTSRV_BASE] = processer;
	SYS_LOG_INF("Register %d processer\n", msg_type);
	return 0;
}

#if 0
static void btsrv_print_cmd(uint8_t cmd)
{
	char *str = NULL;
	uint8_t start_cmd;

	if (cmd >= MSG_BTSRV_MAP_CONNECT) {
		start_cmd = MSG_BTSRV_MAP_CONNECT;
		str = STRINGIFY(MSG_BTSRV_MAP_CONNECT);
	} else if (cmd >= MSG_BTSRV_TWS_INIT) {
		start_cmd = MSG_BTSRV_TWS_INIT;
		str = STRINGIFY(MSG_BTSRV_TWS_INIT);
	} else if (cmd >= MSG_BTSRV_HID_START) {
		start_cmd = MSG_BTSRV_HID_START;
		str = STRINGIFY(MSG_BTSRV_HID_START);
	} else if (cmd >= MSG_BTSRV_PBAP_CONNECT_FAILED) {
		start_cmd = MSG_BTSRV_PBAP_CONNECT_FAILED;
		str = STRINGIFY(MSG_BTSRV_PBAP_CONNECT_FAILED);
	} else if (cmd >= MSG_BTSRV_SPP_START) {
		start_cmd = MSG_BTSRV_SPP_START;
		str = STRINGIFY(MSG_BTSRV_SPP_START);
	} else if (cmd >= MSG_BTSRV_HFP_AG_START) {
		start_cmd = MSG_BTSRV_HFP_AG_START;
		str = STRINGIFY(MSG_BTSRV_HFP_AG_START);
	} else if (cmd >= MSG_BTSRV_HFP_SWITCH_SOUND_SOURCE) {
		start_cmd = MSG_BTSRV_HFP_SWITCH_SOUND_SOURCE;
		str = STRINGIFY(MSG_BTSRV_HFP_SWITCH_SOUND_SOURCE);
	} else if (cmd >= MSG_BTSRV_HFP_START) {
		start_cmd = MSG_BTSRV_HFP_START;
		str = STRINGIFY(MSG_BTSRV_HFP_START);
	} else if (cmd >= MSG_BTSRV_AVRCP_START) {
		start_cmd = MSG_BTSRV_AVRCP_START;
		str = STRINGIFY(MSG_BTSRV_AVRCP_START);
	} else if (cmd >= MSG_BTSRV_A2DP_START) {
		start_cmd = MSG_BTSRV_A2DP_START;
		str = STRINGIFY(MSG_BTSRV_A2DP_START);
	} else {
		start_cmd = MSG_BTSRV_UPDATE_SCAN_MODE;
		str = STRINGIFY(MSG_BTSRV_UPDATE_SCAN_MODE);
	}

	SYS_LOG_INF("btsrv cmd %d = %s + %d", cmd, str, (cmd - start_cmd));
}
#endif

#if (CONFIG_DEBUG_BT_STACK == 0)
void bt_service_main_loop(void *parama1, void *parama2, void *parama3)
{
	struct app_msg msg = {0};
	bool terminaltion = false;
	bool suspend = false;
	int result = 0;
	//	uint32_t start_time, cost_time;
	SYS_LOG_INF("btsrv_man_loop:\n");
	while (!terminaltion) {
		if (receive_msg(&msg, thread_timer_next_timeout())) {
			MSG_RECV_TIME_STAT_START();
            SYS_LOG_INF("type %d, cmd %d\n", msg.type, msg.cmd);
			switch (msg.type) {
			case MSG_EXIT_APP:
				_bt_service_exit();
				terminaltion = true;
				break;
			case MSG_SUSPEND_APP:
				suspend = true;
				break;
			case MSG_INIT_APP:
				if(!suspend)
					_bt_service_init(&msg);
				break;
			default:
				if ((msg.type >= MSG_BTSRV_BASE && msg.type < MSG_BTSRV_MAX &&
					msg_processer[msg.type - MSG_BTSRV_BASE]) && !suspend) {

//					start_time = k_cycle_get_32();
					msg_processer[msg.type - MSG_BTSRV_BASE](&msg);
#if 0
					cost_time = k_cyc_to_us_floor32(k_cycle_get_32() - start_time);
					if (cost_time > BTSRV_PORC_MONITOR_TIME) {
						printk("xxxx:(%s) Btsrv type %d cmd %d proc used %d us\n", __func__, msg.type, msg.cmd, cost_time);
						btsrv_print_cmd(msg.cmd);
					}
#endif
				}
				break;
			}

			if (msg.callback) {
				msg.callback(&msg, result, NULL);
			}
			MSG_RECV_TIME_STAT_STOP(msg.cmd, msg.type, msg.value);
		}
		thread_timer_handle_expired();
	}
}
#else
void bt_service_main_loop(void *parama1, void *parama2, void *parama3)
{
	struct app_msg msg = {0};
	bool terminaltion = false;

	while (!terminaltion) {
		if (receive_msg(&msg, OS_FOREVER)) {
			switch (msg.type) {
			case MSG_EXIT_APP:
				terminaltion = true;
				break;
			default:
				break;
			}
		}
	}
}
#endif
