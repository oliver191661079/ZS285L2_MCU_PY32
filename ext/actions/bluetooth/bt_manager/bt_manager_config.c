/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt manager config.
*/
#define SYS_LOG_DOMAIN "bt manager"
#define SYS_LOG_LEVEL 4		/* 4, debug */
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
#include "btmgr_tws_snoop_inner.h"

static uint8_t default_pre_mac[3] = {0x50, 0xC0, 0xF0};

uint8_t bt_manager_config_get_tws_limit_inquiry(void)
{
	return 0;
}

uint8_t bt_manager_config_get_tws_compare_high_mac(void)
{
	return 1;
}

uint8_t bt_manager_config_get_tws_compare_device_id(void)
{
	return 0;
}

void bt_manager_config_set_pre_bt_mac(uint8_t *mac)
{
	memcpy(mac, default_pre_mac, sizeof(default_pre_mac));
}

void bt_manager_updata_pre_bt_mac(uint8_t *mac)
{
	memcpy(default_pre_mac, mac, sizeof(default_pre_mac));
}

bool bt_manager_config_enable_tws_sync_event(void)
{
	return false;
}

uint8_t bt_manager_config_connect_phone_num(void)
{
	uint8_t num;

	btmgr_feature_cfg_t * cfg =  bt_manager_get_feature_config();

	if (cfg->sp_dual_phone) {
		num = 2;
	} else {
		num = 1;
	}

	if (cfg->sp_phone_takeover) {
		num++;
	}

	return num;
}

bool bt_manager_config_support_a2dp_aac(void)
{
#ifdef CONFIG_BT_A2DP_AAC
	return true;
#else
	return false;
#endif
}

bool bt_manager_config_support_a2dp_ldac(void)
{
#ifdef CONFIG_BT_A2DP_LDAC
	if (bt_manager_config_pts_test()) {
		return false;
	} else {
		return true;
	}
#else
	return false;
#endif

}

bool bt_manager_config_pts_test(void)
{
#ifdef  CONFIG_BT_PTS_TEST
	if (bt_pts_is_enabled()) {
		return true;
	}
#endif
	return false;
}

bool bt_manager_config_lea_pts_test(void)
{
#ifdef  CONFIG_BT_LEA_PTS_TEST
	if (bt_manager_config_pts_test()) {
		return true;
	}
#endif
	return false;
}

uint16_t bt_manager_config_volume_sync_delay_ms(void)
{
	return 3000;
}

uint32_t bt_manager_config_bt_class(void)
{
	if (bt_manager_audio_is_lea_open()) {
		return 0x244414;		/* Rendering,Audio, Audio/Video, Wearable Headset Device, Loudspeaker, LE Audio*/
	} else {
		return 0x240414;		/* Rendering,Audio, Audio/Video, Wearable Headset Device, Loudspeaker*/
	}
}

uint16_t *bt_manager_config_get_device_id(void)
{
	/*vendor id Source,default_vendor_id,product_id,version_id*/
	static uint16_t device_id[4] = {0x0001, CONFIG_BT_VENDOR_ID, CONFIG_BT_PRODUCT_ID, 0x0001};

	return (uint16_t *)device_id;
}

int bt_manager_config_expect_tws_connect_role(void)
{
	/* If use expect tws role, Recommend user todo:
	 * use diff device ID to dicide tws master/slave role,
	 * and in bt_manager_tws_discover_check_device function check
	 * disvoer tws device is expect tws role.
	 */

	//return BTSRV_TWS_MASTER;		/* Expect as tws master */
	//return BTSRV_TWS_SLAVE;		/* Expect as tws slave */
	//return BTSRV_TWS_NONE;		/* Decide tws role by bt service */
	struct btmgr_tws_context_t* context = btmgr_get_tws_context();
	int tmp_config_expect_role = BTSRV_TWS_NONE;

	if (context && context->expect_role_cb)
	{
	   tmp_config_expect_role = context->expect_role_cb();
	}
    SYS_LOG_INF("expect_role:%d\n",tmp_config_expect_role);
	return tmp_config_expect_role;
}

int bt_manager_config_expect_phone_role(void){
	int role = -1;
#if defined(CONFIG_BT_PHONE_ROLE_MASTER)
	role= CONTROLER_ROLE_MASTER;
#elif defined(CONFIG_BT_PHONE_ROLE_SLAVE)
	role= CONTROLER_ROLE_SLAVE;
#endif
	return role;
}

