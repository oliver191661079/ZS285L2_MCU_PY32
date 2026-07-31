/*
 * Copyright (c) 2024 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _APP_TWS_H_
#define _APP_TWS_H_

#ifdef CONFIG_BT_PAWR
#define ENABLE_PAWR_APP	1
#endif

typedef enum{
	APP_TWS_ROLE_PRIMARY = 0,
	APP_TWS_ROLE_SECONDARY = 1,
} system_tws_role_e;

enum APP_TWS_MODE {
	APP_TWS_MODE_NONE = 0,
	APP_TWS_MODE_BIS = 1,
	APP_TWS_MODE_SNOOP = 2,
};

enum BT_PAWR_VERSION_RECORD {
	BT_PAWR_VERSION_INVALID = 0x0000,
	BT_PAWR_VERSION_001	= (APP_TWS_MODE_BIS << 8 | 0x01), 
	BT_PAWR_VERSION_002	= (APP_TWS_MODE_BIS << 8 | 0x02), 
	BT_PAWR_VERSION_003	= (APP_TWS_MODE_SNOOP << 8 | 0x01), 
};

#define APP_TWS_MODE(ver) (ver>>8)

#define SERIVCE_UUID	0xFDDF // Notice: Modification not allowed

#ifdef CONFIG_APP_TWS_SNOOP
#define TWS_LOCAL_VER BT_PAWR_VERSION_003 // Notice: Carefully modify
#else
#define TWS_LOCAL_VER BT_PAWR_VERSION_002 // Notice: Carefully modify
#endif
#define TWS_VER_BIS BT_PAWR_VERSION_002 // Notice: Carefully modify

//type of pawr adv data
#define PAWR_DT_LIGHT 1
#define PAWR_DT_VOLUME 2
#define PAWR_DT_KEY 3
#define PAWR_DT_CMD 4
#define PAWR_DT_CMD_REPLY 5
#define PAWR_DT_MEDIA_VERSION 6
#define PAWR_DT_TWS_VERSION 0x0F

//different pawr cmd of type PAWR_DT_CMD
#define PAWR_CMD_ENTER_SNOOP_TWS	1
#define PAWR_CMD_ENTER_BIS_TWS		2
#define PAWR_CMD_EXIT_TWS		3
#define PAWR_CMD_POWER_OFF 		4

bool app_tws_status_get_enable(void);
u8_t app_tws_status_get_mode(void);
u8_t app_tws_status_get_role(void);
bool app_tws_status_get_connected(void);
void app_tws_status_set_connected(bool connect);

void app_tws_manual_switch(void);
void app_tws_mode_select(u8_t mode, u8_t role);
void app_tws_set_enable(bool enable);
bool app_tws_get_enable(void);

void app_tws_load_tws_mode(void);

int app_tws_on_pawr_primary_sync(bool synced, u8_t *addr);
void app_tws_on_pawr_secondary_sync(bool synced, u8_t *addr);
void app_tws_on_pawr_secondary_syncing(void);
void app_tws_on_snoop_connect(bool connected);

void app_tws_bis_mode_auto_connect(u8_t role);

void app_tws_storage_clear(void);

#ifdef CONFIG_APP_TWS_SNOOP
void app_tws_on_source_switch(bool snoop_support);
#endif
void app_tws_send_app_mode_switch_msg(u8_t mode, u8_t role);

void app_tws_mode_switch(u8_t mode, u8_t role);
void app_tws_exit(void);

void app_tws_wait_pawr_cmd_reply(int cmd);
void app_tws_stop_pawr_cmd_reply(void);

#ifdef ENABLE_PAWR_APP
int pawr_response_cmd(u8_t seq, u8_t cmd, u8_t reply);
int pawr_response_key_event(u32_t key_event);
int pawr_response_vol(u8_t vol100);
int pawr_sync_volume(u8_t sync_vol);
#endif

#endif
