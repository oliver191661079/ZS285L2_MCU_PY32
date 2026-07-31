/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt manager.
 */
#define SYS_LOG_NO_NEWLINE
#define SYS_LOG_DOMAIN "bt manager"

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
#include <shell/shell.h>
#include <acts_bluetooth/host_interface.h>
#include <property_manager.h>

#ifdef CONFIG_BT_SPP
#define MGR_SPP_TEST_SHELL		0
#else
#define MGR_SPP_TEST_SHELL		0
#endif

#ifdef CONFIG_BT_PBAP_CLIENT
#define MGR_PBAP_TEST_SHELL		0
#else
#define MGR_PBAP_TEST_SHELL		0
#endif

#ifdef CONFIG_BT_MAP_CLIENT
#define MGR_MAP_TEST_SHELL		1
#else
#define MGR_MAP_TEST_SHELL		0
#endif

#ifdef CONFIG_BT_BLE
#define MGR_BLE_TEST_SHELL		0
#else
#define MGR_BLE_TEST_SHELL		0
#endif

#if (defined CONFIG_BT_BLE) || (defined CONFIG_BT_SPP)
#define MGR_SPPBLE_STREAM_TEST_SHELL	0
#else
#define MGR_SPPBLE_STREAM_TEST_SHELL	0
#endif


#ifdef CONFIG_BT_MAP_CLIENT
#define TEST_MAP_PATH             "telecom/msg/inbox"
#define TEST_MAP_SET_FOLDER       "telecom/msg/inbox"
#define TEST_MAP_APP_PARAMETER_DATETIME  1
#endif

#if WAIT_TODO
extern uint8_t hci_set_acl_print_enable(uint8_t enable);
#endif

static int mgr_char2hex(const char *c, uint8_t *x)
{
	if (*c >= '0' && *c <= '9') {
		*x = *c - '0';
	} else if (*c >= 'a' && *c <= 'f') {
		*x = *c - 'a' + 10;
	} else if (*c >= 'A' && *c <= 'F') {
		*x = *c - 'A' + 10;
	} else {
		return -EINVAL;
	}

	return 0;
}

static int mgr_str2bt_addr(const char *str, bd_address_t *addr)
{
	int i, j;
	uint8_t tmp;

	if (strlen(str) != 17) {
		return -EINVAL;
	}

	for (i = 5, j = 1; *str != '\0'; str++, j++) {
		if (!(j % 3) && (*str != ':')) {
			return -EINVAL;
		} else if (*str == ':') {
			i--;
			continue;
		}

		addr->val[i] = addr->val[i] << 4;

		if (mgr_char2hex(str, &tmp) < 0) {
			return -EINVAL;
		}

		addr->val[i] |= tmp;
	}

	return 0;
}

static void btmgr_discover_result(void *result)
{
	uint8_t i;
	struct btsrv_discover_result *cb_result = result;

	if (cb_result->discover_finish) {
		SYS_LOG_INF("Discover finish\n");
	} else {
		SYS_LOG_INF("Mac %02x:%02x:%02x:%02x:%02x:%02x, rssi %i\n",
			cb_result->addr.val[5], cb_result->addr.val[4], cb_result->addr.val[3],
			cb_result->addr.val[2], cb_result->addr.val[1], cb_result->addr.val[0],
			cb_result->rssi);
		if (cb_result->len) {
			SYS_LOG_INF("Name: ");
			for (i = 0; i < cb_result->len; i++) {
				printk("%c", cb_result->name[i]);
			}
			printk("\n");
			SYS_LOG_INF("Device id: 0x%x, 0x%x, 0x%x, 0x%x\n", cb_result->device_id[0],
					cb_result->device_id[1], cb_result->device_id[2], cb_result->device_id[3]);
		}
	}
}

static int shell_cmd_btmgr_br_discover(const struct shell *shell, size_t argc, char *argv[])
{
	struct btsrv_discover_param param;

	if (argc < 2) {
		SYS_LOG_INF("Used: btmgr discover start/stop\n");
		return -EINVAL;
	}

	if (!strcmp(argv[1], "start")) {
		param.cb = &btmgr_discover_result;
		param.length = 4;
		param.num_responses = 0;
		if (bt_manager_br_start_discover(&param)) {
			SYS_LOG_ERR("Failed to start discovery\n");
		}
	} else if (!strcmp(argv[1], "stop")) {
		if (bt_manager_br_stop_discover()) {
			SYS_LOG_ERR("Failed to stop discovery\n");
		}
	} else {
		SYS_LOG_INF("Used: btmgr discover start/stop\n");
	}

	return 0;
}

static int shell_cmd_btmgr_br_connect(const struct shell *shell, size_t argc, char *argv[])
{
	bd_address_t addr;
	int err;

	if (argc < 2) {
		SYS_LOG_INF("CMD link: btmgr br_connect F4:4E:FD:xx:xx:xx\n");
		return -EINVAL;
	}

	err = mgr_str2bt_addr(argv[1], &addr);
	if (err) {
		SYS_LOG_INF("Invalid peer address (err %d)\n", err);
		return err;
	}

	return bt_manager_br_connect(&addr);
}

static int shell_cmd_btmgr_br_disconnect(const struct shell *shell, size_t argc, char *argv[])
{
	bd_address_t addr;
	int err;

	if (argc < 2) {
		SYS_LOG_INF("CMD link: btmgr br_connect F4:4E:FD:xx:xx:xx\n");
		return -EINVAL;
	}

	err = mgr_str2bt_addr(argv[1], &addr);
	if (err) {
		SYS_LOG_INF("Invalid peer address (err %d)\n", err);
		return err;
	}

	return bt_manager_br_disconnect(&addr);
}

#if MGR_SPP_TEST_SHELL
/* UUID: "00006666-0000-1000-8000-00805F9B34FB" */
static const uint8_t test_spp_uuid[16] = {0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,	\
										0x00, 0x10, 0x00, 0x00, 0x66, 0x66, 0x00, 0x00};
static uint8_t test_spp_channel;
static uint8_t test_spp_start_tx;

#define SPP_TEST_SEND_SIZE			600
#define SPP_TEST_WORK_INTERVAL		1		/* 1ms */

static uint8_t spp_test_buf[SPP_TEST_SEND_SIZE];
static const char spp_tx_trigger[] = "1122334455";
static const char spp_tx_sample[] = "Test spp send data";
static os_delayed_work spp_test_work;

static void test_spp_connect_failed_cb(uint8_t channel)
{
	SYS_LOG_INF("channel:%d\n", channel);
	if (test_spp_channel == channel) {
		test_spp_channel = 0;
	}
}

static void test_spp_connected_cb(uint8_t channel, uint8_t *uuid)
{
	SYS_LOG_INF("channel:%d\n", channel);
	test_spp_channel = channel;
#if WAIT_TODO
	hci_set_acl_print_enable(0);
#endif
}

static void test_spp_disconnected_cb(uint8_t channel)
{
	SYS_LOG_INF("channel:%d\n", channel);
	if (test_spp_channel == channel) {
		test_spp_channel = 0;
	}
#if WAIT_TODO
	hci_set_acl_print_enable(1);
#endif
}

static void spp_test_delaywork(os_work *work)
{
	static uint32_t curr_time;
	static uint32_t pre_time;
	static uint32_t count;
	int ret;

	if (test_spp_start_tx && test_spp_channel) {
		ret = bt_manager_spp_send_data(test_spp_channel, spp_test_buf, SPP_TEST_SEND_SIZE);
		os_delayed_work_submit(&spp_test_work, SPP_TEST_WORK_INTERVAL);

		if (ret > 0) {
			count += SPP_TEST_SEND_SIZE;
			curr_time = k_uptime_get_32();
			if ((curr_time - pre_time) >= 1000) {
				printk("Tx: %d byte\n", count);
				count = 0;
				pre_time = curr_time;
			}
		}
	}
}

static void test_spp_start_stop_send_delaywork(void)
{
	uint8_t value = 0;
	int i;

	if (test_spp_start_tx == 0) {
		for (i = 0; i < SPP_TEST_SEND_SIZE; i++) {
			spp_test_buf[i] = value++;
		}

		test_spp_start_tx = 1;
		os_delayed_work_init(&spp_test_work, spp_test_delaywork);
		os_delayed_work_submit(&spp_test_work, SPP_TEST_WORK_INTERVAL);
		SYS_LOG_INF("SPP tx start\n");
	} else {
		test_spp_start_tx = 0;
		os_delayed_work_cancel(&spp_test_work);
		SYS_LOG_INF("SPP tx stop\n");
	}
}

static void test_spp_receive_data_cb(uint8_t channel, uint8_t *data, uint32_t len)
{
	static uint32_t curr_time;
	static uint32_t pre_time;
	static uint32_t count;

	/* SYS_LOG_INF("channel:%d, rx: %d\n", channel, len); */
	if (len == strlen(spp_tx_trigger)) {
		if (memcmp(data, spp_tx_trigger, len) == 0) {
			test_spp_start_stop_send_delaywork();
		}
	}

	count += len;
	curr_time = k_uptime_get_32();
	if ((curr_time - pre_time) >= 1000) {
		printk("Rx: %d byte\n", count);
		count = 0;
		pre_time = curr_time;
	}
}

static const struct btmgr_spp_cb test_spp_cb = {
	.connect_failed = test_spp_connect_failed_cb,
	.connected = test_spp_connected_cb,
	.disconnected = test_spp_disconnected_cb,
	.receive_data = test_spp_receive_data_cb,
};

static int shell_cmd_btspp_reg(const struct shell *shell, size_t argc, char *argv[])
{
	static uint8_t reg_flag = 0;

	if (reg_flag) {
		SYS_LOG_INF("Already register\n");
		return 0;
	}

	if (bt_manager_spp_reg_uuid((uint8_t *)test_spp_uuid, (struct btmgr_spp_cb *)&test_spp_cb)) {
		reg_flag = 1;
	}
	return 0;
}

static int shell_cmd_btspp_spp_connect(const struct shell *shell, size_t argc, char *argv[])
{
	bd_address_t addr;
	int err;

	if (test_spp_channel) {
		SYS_LOG_INF("Already connect channel %d\n", test_spp_channel);
		return 0;
	}

	if (argc < 2) {
		SYS_LOG_INF("CMD link: btspp spp_connect F4:4E:FD:xx:xx:xx\n");
		return -EINVAL;
	}

	err = mgr_str2bt_addr(argv[1], &addr);
	if (err) {
		SYS_LOG_INF("Invalid peer address (err %d)\n", err);
		return err;
	}

	test_spp_channel = bt_manager_spp_connect(&addr, (uint8_t *)test_spp_uuid, (struct btmgr_spp_cb *)&test_spp_cb);
	if (test_spp_channel == 0) {
		SYS_LOG_INF("Failed to do spp connect\n");
	} else {
		SYS_LOG_INF("SPP connect channel %d\n", test_spp_channel);
	}

	return 0;
}

static int shell_cmd_btspp_spp_disconnect(const struct shell *shell, size_t argc, char *argv[])
{
	if (test_spp_channel == 0) {
		SYS_LOG_INF("SPP not connected\n");
	} else {
		bt_manager_spp_disconnect(test_spp_channel);
	}

	return 0;
}

static int shell_cmd_btspp_send(const struct shell *shell, size_t argc, char *argv[])
{
	uint8_t *data;
	uint16_t len;

	if (test_spp_channel == 0) {
		SYS_LOG_INF("SPP not connected\n");
	} else {
		if (argc >= 2) {
			data = argv[1];
			len = strlen(argv[1]);
		} else {
			data = (uint8_t *)spp_tx_sample;
			len = strlen(spp_tx_sample);
		}
		bt_manager_spp_send_data(test_spp_channel, data, len);
		SYS_LOG_INF("Send data len %d\n", len);
	}

	return 0;
}
#endif

#if MGR_PBAP_TEST_SHELL
/* PBAP path
 * telecom/pb.vcf
 * telecom/ich.vcf
 * telecom/och.vcf
 * telecom/mch.vcf
 * telecom/cch.vcf
 * telecom/spd.vcf
 * telecom/fav.vcf
 * SIM1/telecom/pb.vcf
 * SIM1/telecom/ich.vcf
 * SIM1/telecom/och.vcf
 * SIM1/telecom/mch.vcf
 * SIM1/telecom/cch.vcf
 */
#define TEST_PBAP_PATH "telecom/pb.vcf"
static uint8_t test_app_id;

static void test_pbap_connect_failed_cb(uint8_t app_id)
{
	SYS_LOG_INF("app id %d\n", app_id);
}

static void test_pbap_connected_cb(uint8_t app_id)
{
	SYS_LOG_INF("app id %d\n", app_id);
}

static void test_pbap_disconnected_cb(uint8_t app_id)
{
	SYS_LOG_INF("app id %d\n", app_id);
	test_app_id = 0;
}

static void test_pbap_result_cb(uint8_t app_id, struct mgr_pbap_result *result, uint8_t size)
{
	int i;

	SYS_LOG_INF("app id %d\n", app_id);

	for (i = 0; i < size; i++) {
		SYS_LOG_INF("Type %d, len %d, value %s\n", result[i].type, result[i].len, result[i].data);
	}
}

static const struct btmgr_pbap_cb test_pbap_cb = {
	.connect_failed = test_pbap_connect_failed_cb,
	.connected = test_pbap_connected_cb,
	.disconnected = test_pbap_disconnected_cb,
	.result = test_pbap_result_cb,
};

static int shell_cmd_pbap_get(const struct shell *shell, size_t argc, char *argv[])
{

	bd_address_t addr;
	int err;

	if (argc < 2) {
		SYS_LOG_INF("CMD link: btpbap get F4:4E:FD:xx:xx:xx\n");
		return -EINVAL;
	}

	err = mgr_str2bt_addr(argv[1], &addr);
	if (err) {
		SYS_LOG_INF("Invalid peer address (err %d)\n", err);
		return err;
	}

	test_app_id = btmgr_pbap_get_phonebook(&addr, TEST_PBAP_PATH, (struct btmgr_pbap_cb *)&test_pbap_cb);
	if (!test_app_id) {
		SYS_LOG_INF("Failed to get phonebook\n");
	} else {
		SYS_LOG_INF("test_app_id %d\n", test_app_id);
	}

	return 0;
}

static int shell_cmd_pbap_abort(const struct shell *shell, size_t argc, char *argv[])
{
	if (test_app_id) {
		btmgr_pbap_abort_get(test_app_id);
	}

	return 0;
}
#endif

#if MGR_MAP_TEST_SHELL
static uint8_t map_test_app_id;


#if 0
static void btsrv_map_timer_handler(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	if (map_test_app_id) {
		btmgr_map_get_messages_listing(map_test_app_id,1,0x0A);
	}
}
#endif

static void test_map_connect_failed_cb(uint8_t app_id)
{
	SYS_LOG_INF("app id %d\n", app_id);
	//thread_timer_stop(&system_datetime_timer);
}

static void test_map_connected_cb(uint8_t app_id)
{
	SYS_LOG_INF("app id %d\n", app_id);
	map_test_app_id = app_id;
}

static void test_map_disconnected_cb(uint8_t app_id)
{
	SYS_LOG_INF("app id %d\n", app_id);
	map_test_app_id = 0;
}

static void test_map_result_cb(uint8_t app_id, struct mgr_map_result *result, uint8_t size)
{
	int i;
    uint8_t datetime_buf[24];

	SYS_LOG_INF("app id %d size:%d\n", app_id,size);

    if(size > 0){
        for (i = 0; i < size; i++) {
            if((result[i].type == TEST_MAP_APP_PARAMETER_DATETIME) && (result[i].len < 24)){
                memcpy(datetime_buf,result[i].data,result[i].len);
                datetime_buf[result[i].len] = 0;
                SYS_LOG_INF("Type %d, len %d, value %s\n", result[i].type, result[i].len, datetime_buf);
            }
        }
    }
}

static const struct btmgr_map_cb test_map_cb = {
	.connect_failed = test_map_connect_failed_cb,
	.connected = test_map_connected_cb,
	.disconnected = test_map_disconnected_cb,
	.result = test_map_result_cb,
};

static int cmd_test_map_connect(const struct shell *shell, size_t argc, char *argv[])
{
	bd_address_t addr;
	int err;
    uint8_t app_id;

	if (argc < 2) {
		SYS_LOG_INF("CMD link: btpbap get F4:4E:FD:xx:xx:xx");
		return -EINVAL;
	}

	err = mgr_str2bt_addr(argv[1], &addr);
	if (err) {
		SYS_LOG_INF("Invalid peer address (err %d)", err);
		return err;
	}

	app_id = btmgr_map_client_connect(&addr, TEST_MAP_PATH, (struct btmgr_map_cb *)&test_map_cb);
	if (!app_id) {
		SYS_LOG_INF("Failed to connect\n");
	} else {
		SYS_LOG_INF("map_test_app_id %d\n", app_id);
	}

	return 0;
}

static int cmd_test_map_set_folder(const struct shell *shell, size_t argc, char *argv[])
{
	if (map_test_app_id) {
	    btmgr_map_client_set_folder(map_test_app_id,TEST_MAP_SET_FOLDER,2);
    }
    return 0;
}

static int cmd_test_map_get_msg_list(const struct shell *shell, size_t argc, char *argv[])
{
	if (map_test_app_id) {
	    btmgr_map_get_messages_listing(map_test_app_id,1,0x0A);
    }
    return 0;
}

static int cmd_test_map_get_folder_list(const struct shell *shell, size_t argc, char *argv[])
{
	if (map_test_app_id) {
		btmgr_map_get_folder_listing(map_test_app_id);
    }
    return 0;
}

static int cmd_test_map_abort(const struct shell *shell, size_t argc, char *argv[])
{
    btmgr_map_abort_get(map_test_app_id);
	return 0;
}

static int cmd_test_map_disconnect(const struct shell *shell, size_t argc, char *argv[])
{
    btmgr_map_client_disconnect(map_test_app_id);
	return 0;
}
#endif

#if MGR_BLE_TEST_SHELL
#define SPEED_BLE_SERVICE_UUID		BT_UUID_DECLARE_16(0xFFC0)
#define SPEED_BLE_WRITE_UUID		BT_UUID_DECLARE_16(0xFFC1)
#define SPEED_BLE_READ_UUID			BT_UUID_DECLARE_16(0xFFC2)

static int ble_speed_rx_data(uint8_t *buf, uint16_t len);

#define BLE_TEST_SEND_SIZE			200
#define BLE_TEST_WORK_INTERVAL		1		/* 1ms */

static const char ble_speed_tx_trigger[] = "1122334455";
static uint8_t ble_speed_tx_flag = 0;
static uint8_t ble_speed_notify_enable = 0;
static os_delayed_work ble_speed_test_work;
uint8_t ble_speed_send_buf[BLE_TEST_SEND_SIZE];

static ssize_t speed_write_cb(struct bt_conn *conn,
			      const struct bt_gatt_attr *attr,
			      const void *buf, uint16_t len, uint16_t offset,
			      uint8_t flags)
{
	ble_speed_rx_data((uint8_t *)buf, len);
	return len;
}

static void speed_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
	SYS_LOG_INF("value: %d\n", value);
	ble_speed_notify_enable = (uint8_t)value;
}

static struct bt_gatt_attr ble_speed_attrs[] = {
	BT_GATT_PRIMARY_SERVICE(SPEED_BLE_SERVICE_UUID),

	BT_GATT_CHARACTERISTIC(SPEED_BLE_WRITE_UUID, BT_GATT_CHRC_WRITE|BT_GATT_CHRC_WRITE_WITHOUT_RESP,
						BT_GATT_PERM_WRITE, NULL, speed_write_cb, NULL),

	BT_GATT_CHARACTERISTIC(SPEED_BLE_READ_UUID, BT_GATT_CHRC_NOTIFY,
				BT_GATT_PERM_READ | BT_GATT_PERM_WRITE, NULL, NULL, NULL),
	BT_GATT_CCC(speed_ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
};

static void ble_speed_test_delaywork(os_work *work)
{
	static uint32_t curr_time;
	static uint32_t pre_time;
	static uint32_t TxCount;
	uint16_t mtu;
	static uint8_t data = 0;
	uint8_t i, j, repeat;
	int ret;

	if (ble_speed_tx_flag && ble_speed_notify_enable) {
		mtu = bt_manager_get_ble_mtu() - 3;
		mtu = (mtu > BLE_TEST_SEND_SIZE) ? BLE_TEST_SEND_SIZE : mtu;

#if 0
		if (mtu <= 23) {
			repeat = 10;
		} else if (mtu <=  64) {
			repeat = 4;
		} else {
			repeat = 1;
		}
#else
		repeat = 10;
#endif

		for (j = 0; j < repeat; j++) {
			for (i = 0; i < mtu; i++) {
				ble_speed_send_buf[i] = data++;
			}

			ret = bt_manager_ble_send_data(&ble_speed_attrs[3], &ble_speed_attrs[4], ble_speed_send_buf, mtu);
			if (ret < 0) {
				break;
			}

			TxCount += mtu;
			curr_time = k_uptime_get_32();
			if ((curr_time - pre_time) >= 1000) {
				printk("Tx: %d byte\n", TxCount);
				TxCount = 0;
				pre_time = curr_time;
			}

			os_yield();
		}
	}

	os_delayed_work_submit(&ble_speed_test_work, BLE_TEST_WORK_INTERVAL);
}

static void test_ble_speed_start_stop_delaywork(void)
{
	if (!ble_speed_tx_flag) {
		ble_speed_tx_flag = 1;
		os_delayed_work_init(&ble_speed_test_work, ble_speed_test_delaywork);
		os_delayed_work_submit(&ble_speed_test_work, BLE_TEST_WORK_INTERVAL);
		SYS_LOG_INF("BLE tx start\n");
	} else {
		ble_speed_tx_flag = 0;
		ble_speed_notify_enable = 0;
		os_delayed_work_cancel(&ble_speed_test_work);
		SYS_LOG_INF("BLE tx stop\n");
	}
}

static int ble_speed_rx_data(uint8_t *buf, uint16_t len)
{
	static uint32_t curr_time;
	static uint32_t pre_time;
	static uint32_t RxCount;

	if (len == strlen(ble_speed_tx_trigger)) {
		if (memcmp(buf, ble_speed_tx_trigger, len) == 0) {
			test_ble_speed_start_stop_delaywork();
		}
	}

	RxCount += len;
	curr_time = k_uptime_get_32();
	if ((curr_time - pre_time) >= 1000) {
		printk("Rx: %d byte\n", RxCount);
		RxCount = 0;
		pre_time = curr_time;
	}

	return 0;
}

static void ble_speed_connect_cb(uint8_t *mac, uint8_t connected)
{
	SYS_LOG_INF("BLE %s\n", connected ? "connected" : "disconnected");
	SYS_LOG_INF("MAC %2x:%2x:%2x:%2x:%2x:%2x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

	if (connected) {
#if WAIT_TODO
		hci_set_acl_print_enable(0);
#endif
	} else {
#if WAIT_TODO
		hci_set_acl_print_enable(1);
#endif
		ble_speed_tx_flag = 0;
	}
}

static struct ble_reg_manager ble_speed_mgr = {
	.link_cb = ble_speed_connect_cb,
};

static int shell_cmd_btble_reg(const struct shell *shell, size_t argc, char *argv[])
{
	static uint8_t reg_flag = 0;

	if (reg_flag) {
		SYS_LOG_INF("Already register\n");
	} else {
		ble_speed_mgr.gatt_svc.attrs = ble_speed_attrs;
		ble_speed_mgr.gatt_svc.attr_count = ARRAY_SIZE(ble_speed_attrs);

		bt_manager_ble_service_reg(&ble_speed_mgr);
		reg_flag = 1;

		bt_manager_ble_set_idle_interval(12);		/* 12*1.25 = 15ms, for ble speed test */
		SYS_LOG_INF("Register ble service\n");
	}
	return 0;
}
#endif

#if MGR_SPPBLE_STREAM_TEST_SHELL
#define SPPBLE_TEST_STACKSIZE	(1024*2)

io_stream_t sppble_stream;
static uint8_t sppble_stream_opened = 0;

static os_work_q test_sppble_q;
static os_delayed_work sppble_connect_delaywork;
static os_delayed_work sppble_disconnect_delaywork;
static os_delayed_work sppble_run_delaywork;
static uint8_t sppble_test_stack[SPPBLE_TEST_STACKSIZE] __aligned(4);

/* SPP  */
/* UUID: "00001101-0000-1000-8000-00805F9B34FB" */
static const uint8_t sppble_ota_spp_uuid[16] = {0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x01, 0x11, 0x00, 0x00};

/* BLE */
/* UUID: "e49a25f8-f69a-11e8-8eb2-f2801f1b9fd1" */
#define BLE_OTA_SERVICE_UUID BT_UUID_DECLARE_128( \
				0xd1, 0x9f, 0x1b, 0x1f, 0x80, 0xf2, 0xb2, 0x8e, \
				0xe8, 0x11, 0x9a, 0xf6, 0xf8, 0x25, 0x9a, 0xe4)

/* UUID: "e49a25e0-f69a-11e8-8eb2-f2801f1b9fd1" */
#define BLE_OTA_CHA_RX_UUID BT_UUID_DECLARE_128( \
				0xd1, 0x9f, 0x1b, 0x1f, 0x80, 0xf2, 0xb2, 0x8e, \
				0xe8, 0x11, 0x9a, 0xf6, 0xe0, 0x25, 0x9a, 0xe4)

/* UUID: "e49a28e1-f69a-11e8-8eb2-f2801f1b9fd1" */
#define BLE_OTA_CHA_TX_UUID BT_UUID_DECLARE_128( \
				0xd1, 0x9f, 0x1b, 0x1f, 0x80, 0xf2, 0xb2, 0x8e, \
				0xe8, 0x11, 0x9a, 0xf6, 0xe1, 0x28, 0x9a, 0xe4)

static struct bt_gatt_attr ota_gatt_attr[] = {
	BT_GATT_PRIMARY_SERVICE(BLE_OTA_SERVICE_UUID),
	BT_GATT_CHARACTERISTIC(BLE_OTA_CHA_RX_UUID, BT_GATT_CHRC_WRITE_WITHOUT_RESP|BT_GATT_CHRC_READ,
							BT_GATT_PERM_WRITE, NULL, NULL, NULL),
	BT_GATT_CHARACTERISTIC(BLE_OTA_CHA_TX_UUID, BT_GATT_CHRC_NOTIFY|BT_GATT_CHRC_READ,
							BT_GATT_PERM_READ, NULL, NULL, NULL),
	BT_GATT_CCC(NULL, 0)
};

static void test_connect_delaywork(os_work *work)
{
	int ret;

	if (sppble_stream) {
		ret = stream_open(sppble_stream, MODE_IN_OUT);
		if (ret) {
			SYS_LOG_ERR("stream_open Failed\n");
		} else {
			sppble_stream_opened = 1;
			os_delayed_work_submit_to_queue(&test_sppble_q, &sppble_run_delaywork, 0);
		}
	}
}

static void test_disconnect_delaywork(os_work *work)
{
	int ret;

	if (sppble_stream) {
		ret = stream_close(sppble_stream);
		if (ret) {
			SYS_LOG_ERR("stream_close Failed\n");
		} else {
			sppble_stream_opened = 0;
		}
	}
}

static uint8_t read_buf[512];

static void test_run_delaywork(os_work *work)
{
	int ret;

	if (sppble_stream && sppble_stream_opened) {
		ret = stream_read(sppble_stream, read_buf, 512);
		if (ret > 0) {
			SYS_LOG_INF("sppble_stream rx: %d\n", ret);
			ret = stream_write(sppble_stream, read_buf, ret);
			if (ret > 0) {
				SYS_LOG_INF("sppble_stream tx: %d\n", ret);
			}
		}

		os_delayed_work_submit_to_queue(&test_sppble_q, &sppble_run_delaywork, 0);
	}
}

static void test_sppble_connect(bool connected)
{
	SYS_LOG_INF("%s\n", (connected) ? "connected" : "disconnected");
	if (connected) {
		os_delayed_work_submit_to_queue(&test_sppble_q, &sppble_connect_delaywork, 0);
	} else {
		os_delayed_work_submit_to_queue(&test_sppble_q, &sppble_disconnect_delaywork, 0);
	}
}

static int shell_cmd_sppble_stream_reg(const struct shell *shell, size_t argc, char *argv[])
{
	struct sppble_stream_init_param init_param;
	static uint8_t reg_flag = 0;

	if (reg_flag) {
		SYS_LOG_INF("Already register\n");
		return -EIO;
	}

	os_work_q_start(&test_sppble_q, (os_thread_stack_t *)sppble_test_stack, SPPBLE_TEST_STACKSIZE, 11);
	os_delayed_work_init(&sppble_connect_delaywork, test_connect_delaywork);
	os_delayed_work_init(&sppble_disconnect_delaywork, test_disconnect_delaywork);
	os_delayed_work_init(&sppble_run_delaywork, test_run_delaywork);

	memset(&init_param, 0, sizeof(struct sppble_stream_init_param));
	init_param.spp_uuid = (uint8_t *)sppble_ota_spp_uuid;
	init_param.gatt_attr = ota_gatt_attr;
	init_param.attr_size = ARRAY_SIZE(ota_gatt_attr);
	init_param.tx_chrc_attr = &ota_gatt_attr[3];
	init_param.tx_attr = &ota_gatt_attr[4];
	init_param.tx_ccc_attr = &ota_gatt_attr[5];
	init_param.rx_attr = &ota_gatt_attr[2];
	init_param.connect_cb = test_sppble_connect;
	init_param.read_timeout = OS_FOREVER;	/* K_FOREVER, K_NO_WAIT,  K_MSEC(ms) */
	init_param.write_timeout = OS_FOREVER;

	/* Just call stream_create once, for register spp/ble service
	 * not need call stream_destroy
	 */
	sppble_stream = sppble_stream_create(&init_param);
	if (!sppble_stream) {
		SYS_LOG_ERR("stream_create failed\n");
	}

	reg_flag = 1;
	return 0;
}
#endif

SHELL_STATIC_SUBCMD_SET_CREATE(bt_mgr_cmds,
	SHELL_CMD(discover, NULL, "BR discover", shell_cmd_btmgr_br_discover),
	SHELL_CMD(br_connect, NULL, "BR connect", shell_cmd_btmgr_br_connect),
	SHELL_CMD(br_disconnect, NULL, "BR disconnect", shell_cmd_btmgr_br_disconnect),
#if MGR_SPP_TEST_SHELL
	SHELL_CMD(spp_reg, NULL, "Register spp uuid", shell_cmd_btspp_reg),
	SHELL_CMD(spp_connect, NULL, "SPP connect", shell_cmd_btspp_spp_connect),
	SHELL_CMD(spp_disconnect, NULL, "SPP disconnect", shell_cmd_btspp_spp_disconnect),
	SHELL_CMD(spp_send, NULL, "SPP send data", shell_cmd_btspp_send),
#endif
#if MGR_PBAP_TEST_SHELL
	SHELL_CMD(pbap_get, NULL, "Get phonebook", shell_cmd_pbap_get),
	SHELL_CMD(pbap_abort, NULL, "Abort get phonebook", shell_cmd_pbap_abort),
#endif
#if MGR_MAP_TEST_SHELL
	SHELL_CMD(map_connect, NULL, "Connect MSE", cmd_test_map_connect),
	SHELL_CMD(map_abort, NULL, "Abort get message", cmd_test_map_abort),
	SHELL_CMD(map_disconnect, NULL, "Disconnect MSE", cmd_test_map_disconnect),
	SHELL_CMD(map_setfolder, NULL, "Set Folder", cmd_test_map_set_folder),
	SHELL_CMD(map_getmsglist, NULL, "Get Messages Listing", cmd_test_map_get_msg_list),
	SHELL_CMD(map_getfolderlist, NULL, "Get Folder Listing", cmd_test_map_get_folder_list),
#endif
#if MGR_BLE_TEST_SHELL
	SHELL_CMD(ble_reg, NULL, "Register ble service", shell_cmd_btble_reg),
#endif
#if MGR_SPPBLE_STREAM_TEST_SHELL
	SHELL_CMD(sppble_stream_reg, NULL, "Register sppble stream", shell_cmd_sppble_stream_reg),
#endif
	SHELL_SUBCMD_SET_END
);

static int cmd_bt_mgr(const struct shell *shell, size_t argc, char **argv)
{
	if (argc == 1) {
		shell_help(shell);
		return SHELL_CMD_HELP_PRINTED;
	}

	shell_error(shell, "%s unknown parameter: %s", argv[0], argv[1]);

	return -EINVAL;
}

SHELL_CMD_REGISTER(btmgr, &bt_mgr_cmds, "Bluetooth manager test shell commands", cmd_bt_mgr);
