/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file system ble advertise
 */

#include <bt_manager.h>
#include <thread_timer.h>
#include "list.h"
#include <ctype.h>
#include <mem_manager.h>
#include <bt_manager_ble.h>
#include <bt_manager_pawr.h>
#include <acts_bluetooth/gap.h>
#include <stdlib.h>
#include <property_manager.h>
#include <app_launch.h>
#ifdef CONFIG_BT_SELFAPP_ADV
#include "selfapp_api.h"
#endif
#include "system_app.h"

#define GFP_BLE_ADV_POWER (-13)
#define GFP_BLE_ADV_RF_POWER (-20)

#ifdef CONFIG_GFP_PROFILE
#define ADV_INTERVAL_MS (50)
#else
#define ADV_INTERVAL_MS (200)
#endif

#define ADV_DATA_MAX (4)

/* BLE 远程控制 GATT 服务 UUID（与 system_app_ble_remote_cmd.c 一致） */
#define BLE_REMOTE_CMD_SVC_UUID16  0xFFE0U

#define TWS_WAIT_TIME_MS (2000)
#define TWS_SLAVE_WAIT_TIME_MS (TWS_WAIT_TIME_MS)
#define TWS_MASTER_WAIT_TIME_MS (TWS_WAIT_TIME_MS/3)
#define TWS_MASTER_WAIT_TIME1_MS (2500)

//#include <sys_comm.h>
int print_hex_comm(const char *prefix, const void *data, unsigned int size);

typedef enum {
	ADV_TYPE_OTA = 0,
	ADV_TYPE_GFP,		// google fast pair
	ADV_TYPE_LEAUDIO,
	ADV_TYPE_TWS,
	ADV_TYPE_MAX,
	ADV_TYPE_NONE,
} btmgr_adv_type_e;

typedef struct {
	uint8 adv_enabled;
	uint8 advert_state;
	uint8 cur_adv_cnt;
	uint8 total_adv_cnt;
	/*return status: btmgr_adv_state_e */
	int (*adv_get_state)(void);
	int (*adv_enable)(void);
	int (*adv_disable)(void);
} btmgr_adv_register_func_t;

typedef struct {
	struct list_head node;
	char var_id;
	u8_t used;
	u8_t size;
	u8_t data[1];
} ble_adv_var_t;

typedef struct {
	uint8_t len;
	uint8_t type;
	uint8_t data[29];
} adv_data_t;

struct ble_adv_mngr {
	u8_t wait_reconn_phone;
	u8_t advert_state;
	u8_t adv_index;
	u8_t wait_tws_paired;
	u8_t tws_role;
	u8_t total_time_cnt;
	u8_t curr_time_cnt;
	u32_t begin_wait_time;

	struct thread_timer check_adv_timer;
	btmgr_adv_register_func_t *adv[ADV_TYPE_MAX];
    adv_data_t *adv_data;
    u16_t adv_data_size;
};

static struct ble_adv_mngr ble_adv_info;

static void sys_check_ble_adv_stop(void)
{
	struct ble_adv_mngr *p = &ble_adv_info;

	if (p->adv_index >= ADV_TYPE_MAX) {
		return;
	}
	if (p->adv[p->adv_index] && p->adv[p->adv_index]->advert_state
	    && p->adv[p->adv_index]->adv_disable) {
		//adv disable
		if (p->adv_index != ADV_TYPE_LEAUDIO) {
			p->adv[p->adv_index]->advert_state = false;
			p->adv[p->adv_index]->adv_disable();
			//SYS_LOG_INF("rrr:%d\n",p->adv_index);
		} else if ((p->adv_index == ADV_TYPE_LEAUDIO)
			   &&
			   ((p->adv[p->adv_index]->
			     adv_get_state() & ADV_STATE_BIT_ENABLE) == 0)) {
			p->adv[p->adv_index]->advert_state = false;
			p->adv[p->adv_index]->adv_disable();
		}
	}

}

static bool ble_adv_check_enable(void)
{
	struct ble_adv_mngr *p = &ble_adv_info;
	bool enable = true;
	uint32_t cur_time = os_uptime_get_32();
	uint32_t cur_wait_time_ms = 0;
#ifdef CONFIG_GFP_PROFILE
	uint8_t mode;
#endif
	int tmp_cur_tws_role = bt_manager_tws_get_dev_role();

	if (!bt_manager_is_ready()) {
		return false;
	}

	//SYS_LOG_INF("%d,%d,%d", p->wait_tws_paired, p->wait_reconn_phone, tmp_cur_tws_role);
	if ((p->wait_tws_paired == 1) || (p->wait_reconn_phone)) {
		if (tmp_cur_tws_role != BTSRV_TWS_NONE) {
			p->wait_tws_paired = 0;
			//If grouped, no need to re-connect phone, the start adv immediately.
			if (p->wait_reconn_phone == 0) {
				//    return true;
				goto adv_index_enable_check;
			}
			if (tmp_cur_tws_role == BTSRV_TWS_SLAVE) {
				p->wait_reconn_phone = 0;
				goto adv_index_enable_check;
			}
			p->tws_role = tmp_cur_tws_role;
			//enable = true;
			//goto end;
		}

		if ((p->tws_role == BTSRV_TWS_TEMP_MASTER) ||
		    (p->tws_role == BTSRV_TWS_MASTER) ||
		    /*
		       If not been grouped, and only connected to phone.
		     */
		    (p->tws_role == BTSRV_TWS_NONE)) {
			cur_wait_time_ms = TWS_MASTER_WAIT_TIME_MS;
			if (p->wait_reconn_phone) {
				cur_wait_time_ms = TWS_MASTER_WAIT_TIME1_MS;
			}
		} else if (p->tws_role == BTSRV_TWS_TEMP_SLAVE) {
			cur_wait_time_ms = TWS_SLAVE_WAIT_TIME_MS;
		}
		//
		if (cur_time - p->begin_wait_time < cur_wait_time_ms) {
			return false;
		}
		p->wait_tws_paired = 0;
		p->wait_reconn_phone = 0;
		SYS_LOG_INF(":tws_start_adv:%d:%d:%d \n", cur_wait_time_ms,
			    cur_time, p->begin_wait_time);
	}

	/* close BT when BLE is connected */
	if (bt_manager_ble_max_connected()) {
		sys_check_ble_adv_stop();
		enable = false;
		goto end;
	}

	if (bt_manager_audio_takeover_flag()) {
		sys_check_ble_adv_stop();
		enable = false;
		goto end;
	}
 adv_index_enable_check:
#ifdef CONFIG_TWS
	if (bt_manager_tws_get_dev_role() == BTSRV_TWS_SLAVE) {

		sys_check_ble_adv_stop();

		if (p->adv[ADV_TYPE_OTA]) {
			p->adv[ADV_TYPE_OTA]->adv_enabled = 0;
		}
		if (p->adv[ADV_TYPE_GFP]) {
			p->adv[ADV_TYPE_GFP]->adv_enabled = 0;
		}
		//enable = false;
	} else if (bt_manager_tws_get_dev_role() == BTSRV_TWS_MASTER) {
		if (p->adv[ADV_TYPE_OTA]
		    && (p->adv[ADV_TYPE_OTA]->adv_enabled == 0)) {
			p->adv[ADV_TYPE_OTA]->adv_enabled = 1;
		}
		if (p->adv[ADV_TYPE_GFP]
		    && (p->adv[ADV_TYPE_GFP]->adv_enabled == 0)) {
			p->adv[ADV_TYPE_GFP]->adv_enabled = 1;
		}
	}
#endif

#ifdef CONFIG_GFP_PROFILE
    mode = system_app_get_auracast_mode();
    if(mode == 1){
        if (p->adv[ADV_TYPE_GFP]){
            p->adv[ADV_TYPE_GFP]->adv_enabled = 0;
        }
    }
    else{
        if (p->adv[ADV_TYPE_GFP]
            && (p->adv[ADV_TYPE_GFP]->adv_enabled == 0)){
            p->adv[ADV_TYPE_GFP]->adv_enabled = 1;
        }
    }
	/*
	   In pair mode, GFP's adv period is ADV_INTERVAL_MS
	   2 times adv in one period
	   In not pair mode, GFP's adv period is ADV_INTERVAL_MS * 2
	   1 times adv in one period
	 */
	if (bt_manager_is_pair_mode()) {
		p->total_time_cnt = 1;
		p->adv[ADV_TYPE_GFP]->total_adv_cnt = 2;
	} else {
		p->total_time_cnt = 2;
		p->adv[ADV_TYPE_GFP]->total_adv_cnt = 1;
	}
#endif

 end:

	return enable;
}

static void sys_ble_check_advertise(struct thread_timer *ttimer,
				    void *expiry_fn_arg)
{
	struct ble_adv_mngr *p = &ble_adv_info;
	uint8_t i = 0;
	bool tmp_need_check = false;

	if (!ble_adv_check_enable()) {
		return;
	}
	p->curr_time_cnt++;
	if (p->curr_time_cnt < p->total_time_cnt) {
		return;
	} else {
		p->curr_time_cnt = 0;
	}

 adv_start_check:
	//SYS_LOG_INF("adv %d state %d\n",p->adv_index,p->adv[p->adv_index]->advert_state);
	if (p->adv[p->adv_index] && p->adv[p->adv_index]->advert_state
	    && p->adv[p->adv_index]->adv_disable) {
		//adv disable
		p->adv[p->adv_index]->advert_state = false;
		p->adv[p->adv_index]->adv_disable();

	}

	if ((p->adv[p->adv_index])
	    && (p->adv[p->adv_index]->adv_get_state() & ADV_STATE_BIT_ENABLE)) {
		//SYS_LOG_INF(":%d %d %d\n",p->adv_index,p->adv[p->adv_index]->cur_adv_cnt,p->adv[p->adv_index]->total_adv_cnt);

		if (p->adv[p->adv_index]->cur_adv_cnt >=
		    p->adv[p->adv_index]->total_adv_cnt) {
			p->adv[p->adv_index]->cur_adv_cnt = 0;
			p->adv_index++;
			tmp_need_check = true;
		}
	} else {

		p->adv_index++;
		//How many times to check?
		if (p->adv_index >= ADV_TYPE_MAX) {
			p->adv_index = 0;
		}
		i++;
		if (i <= ADV_TYPE_MAX) {
			goto adv_start_check;
		} else {
			return;
		}
	}
	if (p->adv_index >= ADV_TYPE_MAX) {
		p->adv_index = 0;
	}

	if (p->adv[p->adv_index] && p->adv[p->adv_index]->adv_enable
	    && (p->adv[p->adv_index]->adv_get_state() & ADV_STATE_BIT_ENABLE)) {
		p->adv[p->adv_index]->adv_enable();
		p->adv[p->adv_index]->cur_adv_cnt++;
		p->adv[p->adv_index]->advert_state = true;
		//SYS_LOG_INF("cur_adv_cnt:%d %d \n",p->adv_index,p->adv[p->adv_index]->cur_adv_cnt);
	} else {
		if (tmp_need_check) {
			goto adv_start_check;
		}
	}

}

static uint8_t sys_ble_set_adv_data(adv_data_t * adv_data, u8_t num, int8_t power)
{
	uint8_t ad_buf[31], sd_buf[31];
	int ad_len, sd_len, i;
	adv_data_t *t;
	uint8_t ret = 0;

	ad_len = sd_len = 0;

	for (i = 0; i < num; i++) {
		t = &adv_data[i];
		if (t->len > 0 && 0 == sd_len &&
		    ad_len + t->len + 1 <= sizeof(ad_buf)) {
			memcpy(&ad_buf[ad_len], t, t->len + 1);
			ad_len += t->len + 1;
			t->len = 0;
		} else if (t->len > 0 && sd_len + t->len + 1 <= sizeof(sd_buf)) {
			memcpy(&sd_buf[sd_len], t, t->len + 1);
			sd_len += t->len + 1;
			t->len = 0;
		}
	}

	if (ad_len > 0 || sd_len > 0) {
		//SYS_LOG_INF("ad_len=%d, sd_len=%d\n", ad_len, sd_len);
		/* set ble advertise ad & sd data */
		bt_manager_ble_set_adv_data(ad_buf, ad_len, sd_buf, sd_len, power);

		ret = 1;
		/* enable advertise */
		//bt_manager_ble_set_advertise_enable(true);
	}
	return ret;
}

int sys_ble_adv_register(int adv_type, btmgr_adv_register_func_t * funcs)
{
	struct ble_adv_mngr *p = &ble_adv_info;

	SYS_LOG_INF("%d", adv_type);

	if (adv_type < ADV_TYPE_MAX) {
		p->adv[adv_type] = funcs;
		if (funcs) {
			p->adv[adv_type]->adv_enabled = 1;
		}
	}
	return 0;
}

#ifdef CONFIG_BT_LEATWS
static int leatws_mgr_adv_state(void)
{
	struct ble_adv_mngr *p = &ble_adv_info;

	if (p->adv[ADV_TYPE_TWS] && (!(p->adv[ADV_TYPE_TWS]->adv_enabled))) {
		return 0;
	}

	if (bt_manager_tws_get_dev_role() == BTSRV_TWS_SLAVE) {
		return 0;
	}
	return (ADV_STATE_BIT_ENABLE | ADV_STATE_BIT_UPDATE);
}

static int leatws_mgr_adv_enable(void)
{
	adv_data_t *adv_data;
	adv_data_t *t;
	int i = 0;
	static uint32_t time_stamp = 0;
	struct ble_adv_mngr *p = &ble_adv_info;

	if (!time_stamp) {
		time_stamp = os_uptime_get_32();
	}

    if(!p->adv_data){
        SYS_LOG_ERR("no mem!");
        return -1;
    }

	memset(p->adv_data, 0, p->adv_data_size);
    adv_data = p->adv_data;

	/* flags */
	t = &adv_data[0];
	t->type = 0x01;
	t->data[0] = (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR);
	t->len = 2;

	t = &adv_data[1];
	uint16 *cfg_device_id = bt_manager_config_get_device_id();
	t->data[i++] = (cfg_device_id[1] & 0xff);
	t->data[i++] = (cfg_device_id[1] >> 8);
	memcpy(&t->data[i], "sirk:", strlen("sirk:"));
	i += strlen("sirk:");
	bt_manager_le_audio_get_sirk(&t->data[i]);
	t->type = BT_DATA_MANUFACTURER_DATA;
	t->len = 2 + strlen("sirk:") + 16 + 1;

	if ((os_uptime_get_32() - time_stamp) > 5000) {
		time_stamp = os_uptime_get_32();
		SYS_LOG_INF("%02x, %02x, %02x, %02x", t->data[0], t->data[1],
			    t->data[2], t->data[3]);
	}

	if (sys_ble_set_adv_data(adv_data, 2, CONFIG_BT_CONTROLER_BLE_MAX_TXPOWER)) {
		bt_manager_ble_set_advertise_enable(true);
	}

	return 0;
}
#endif

static int ble_mgr_adv_state(void)
{
	struct ble_adv_mngr *p = &ble_adv_info;

	if (p->adv[ADV_TYPE_OTA] && (!(p->adv[ADV_TYPE_OTA]->adv_enabled))) {
		return 0;
	}

	if (bt_manager_ble_max_connected()) {
		return 0;
	}
	if (bt_manager_tws_get_dev_role() == BTSRV_TWS_SLAVE) {
		return 0;
	}
	return (ADV_STATE_BIT_ENABLE | ADV_STATE_BIT_UPDATE);
}

#ifdef CONFIG_GFP_PROFILE
static int gfp_ble_mgr_adv_state(void)
{
	struct ble_adv_mngr *p = &ble_adv_info;

	if (p->adv[ADV_TYPE_GFP] && (!(p->adv[ADV_TYPE_GFP]->adv_enabled))) {
		return 0;
	}

	if (bt_manager_ble_max_connected()) {
		return 0;
	}
	if (bt_manager_tws_get_dev_role() == BTSRV_TWS_SLAVE) {
		return 0;
	}
	return (ADV_STATE_BIT_ENABLE | ADV_STATE_BIT_UPDATE);
}

static int gfp_ble_mgr_adv_enable(void)
{
	adv_data_t *adv_data;
	adv_data_t *t;
	int len;
    uint8_t ad_index = 0;

    struct ble_adv_mngr *p = &ble_adv_info;

    if(!p->adv_data){
        SYS_LOG_ERR("no mem!");
        return -1;
    }

	memset(p->adv_data, 0, p->adv_data_size);
    adv_data = p->adv_data;

    t = &adv_data[ad_index];
    t->type = BT_DATA_FLAGS;
    t->len = 2;
    t->data[0] = BT_LE_AD_LE_EDR_SAME_DEVICE_CTRL | BT_LE_AD_LE_EDR_SAME_DEVICE_HOST;
    ad_index++;

	/* gfp account adv */
	t = &adv_data[ad_index];
	len = gfp_service_data_get(&(t->data[0]));
	if (len >= 2) {
		/* GFP Service Data
		 */
		t->type = 0x16;
		t->len = len + 1;
	}
    ad_index++;

    /* adv tx power */
    t = &adv_data[ad_index];
    t->type = 0x0A;
    t->data[0] = GFP_BLE_ADV_POWER;
    t->len = 2;
    ad_index++;

#ifdef GFP_BLE_DEVICE
    if(bt_manager_audio_is_lea_open()){
        t = &adv_data[ad_index];
        t->type = 0x16;
        t->len = 3;
        t->data[0] = 0x53;
        t->data[1] = 0x18;
        ad_index++;
    }
#endif

	if (sys_ble_set_adv_data(adv_data, ad_index, GFP_BLE_ADV_RF_POWER)) {

#ifdef CONFIG_BT_LETWS
		bt_manager_ble_gfp_set_advertise_enable(true);
#else
		bt_manager_ble_set_advertise_enable(true);
#endif
	}

	return 0;
}
#endif

static int ble_mgr_adv_disable(void)
{
	bt_manager_ble_set_advertise_enable(false);
	return 0;
}

#ifdef CONFIG_BT_SELFAPP_ADV
static int ota_ble_mgr_adv_enable(void)
{
	int i = 0;
	adv_data_t *adv_data;
	adv_data_t *t;
	int name_len;
	u8_t len;
    struct ble_adv_mngr *p = &ble_adv_info;

    if(!p->adv_data){
        SYS_LOG_ERR("no mem!");
        return -1;
    }

    memset(p->adv_data, 0, p->adv_data_size);
    adv_data = p->adv_data;

	t = &adv_data[0];
	t->type = BT_DATA_MANUFACTURER_DATA;
	len = selfapp_get_manufacturer_data(t->data);
	t->len = len + 1;

	i = 0;
	t = &adv_data[1];
	t->type = BT_DATA_SVC_DATA16;
	t->data[i++] = (BOX_SERIVCE_UUID & 0xff);
	t->data[i++] = (BOX_SERIVCE_UUID >> 8);
	t->len = i + 1;

	/* BLE name：始终使用 BT_LE_NAME，与经典蓝牙 BT_NAME 可不同 */
	t = &adv_data[2];
	t->type = BT_DATA_NAME_COMPLETE;
	name_len = property_get(CFG_BLE_NAME, &t->data[0], 29);
	if (name_len <= 0) {
		name_len = property_get(CFG_BT_NAME, &t->data[0], 29);
	}
	if (name_len > 0) {
		t->len = strlen(&t->data[0]) + 1;
		//SYS_LOG_DBG("name_len %d.", strlen(&t->data[0]));
	} else {
		t->len = 1;
	}

	if (sys_ble_set_adv_data(adv_data, 3, CONFIG_BT_CONTROLER_BLE_MAX_TXPOWER)) {
		bt_manager_ble_set_advertise_enable(true);
	}

	return 0;
}
#else
static int ota_ble_mgr_adv_enable(void)
{
	adv_data_t *adv_data;
	adv_data_t *t;
	int name_len;
    struct ble_adv_mngr *p = &ble_adv_info;

	if (!p->adv_data){
        SYS_LOG_ERR("no mem!");
        return -1;
	}

    memset(p->adv_data, 0, p->adv_data_size);
    adv_data = p->adv_data;

	/* flags */
	t = &adv_data[0];
	t->type = 0x01;
#if defined(CONFIG_BT_CROSS_TRANSPORT_KEY)
	t->data[0] = BT_LE_AD_LE_EDR_SAME_DEVICE_CTRL | BT_LE_AD_LE_EDR_SAME_DEVICE_HOST;
#else
	t->data[0] = (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR);
#endif
	t->len = 2;

	/* BLE name：始终使用 BT_LE_NAME，与经典蓝牙 BT_NAME 可不同 */
	t = &adv_data[1];
	name_len = property_get(CFG_BLE_NAME, &t->data[0], 29);
	if (name_len <= 0) {
		name_len = property_get(CFG_BT_NAME, &t->data[0], 29);
	}
	t->type = BT_DATA_NAME_COMPLETE;
	t->len = strlen(&t->data[0]) + 1;
	//SYS_LOG_INF("%s %d name_len %d.",__func__,__LINE__,strlen(&t->data[0]));

#if defined(CONFIG_SYSTEM_APP_BLE_REMOTE_CMD)
	/* 广播携带 0xFFE0，手机可按服务 UUID 过滤 */
	t = &adv_data[2];
	t->type = BT_DATA_UUID16_ALL;
	t->data[0] = (uint8_t)(BLE_REMOTE_CMD_SVC_UUID16 & 0xFFU);
	t->data[1] = (uint8_t)(BLE_REMOTE_CMD_SVC_UUID16 >> 8);
	t->len = 3;

	if (sys_ble_set_adv_data(adv_data, 3, CONFIG_BT_CONTROLER_BLE_MAX_TXPOWER)) {
		bt_manager_ble_set_advertise_enable(true);
	}
#else
	if (sys_ble_set_adv_data(adv_data, 2, CONFIG_BT_CONTROLER_BLE_MAX_TXPOWER)) {
		bt_manager_ble_set_advertise_enable(true);
	}
#endif

	return 0;
}
#endif

#ifdef CONFIG_GFP_PROFILE
static btmgr_adv_register_func_t gfp_ble_adv_funcs = {
	.adv_enabled = 0,
	.advert_state = 0,
	.cur_adv_cnt = 0,
	.total_adv_cnt = 2,
	.adv_get_state = gfp_ble_mgr_adv_state,
	.adv_enable = gfp_ble_mgr_adv_enable,
	.adv_disable = ble_mgr_adv_disable,
};
#endif

static btmgr_adv_register_func_t ota_ble_adv_funcs = {
	.adv_enabled = 0,
	.advert_state = 0,
	.cur_adv_cnt = 0,
	.total_adv_cnt = 1,
	.adv_get_state = ble_mgr_adv_state,
	.adv_enable = ota_ble_mgr_adv_enable,
	.adv_disable = ble_mgr_adv_disable,
};

#ifdef CONFIG_BT_LEATWS
static btmgr_adv_register_func_t leatws_adv_funcs = {
	.adv_enabled = 0,
	.advert_state = 0,
	.cur_adv_cnt = 0,
	.total_adv_cnt = 1,
	.adv_get_state = leatws_mgr_adv_state,
	.adv_enable = leatws_mgr_adv_enable,
	.adv_disable = ble_mgr_adv_disable,
};

int sys_ble_leatws_adv_register(void)
{
	sys_ble_adv_register(ADV_TYPE_TWS,
			     (btmgr_adv_register_func_t *) & leatws_adv_funcs);
	return 0;
}

int sys_ble_leatws_adv_unregister(void)
{
	sys_ble_adv_register(ADV_TYPE_TWS, NULL);
	return 0;
}
#endif

#ifdef LEAUDIO_LEGENCY_ADV
static btmgr_adv_register_func_t lea_adv_funcs = {
	.adv_enabled = 0,
	.advert_state = 0,
	.cur_adv_cnt = 0,
	.total_adv_cnt = 1,
	.adv_get_state = bt_manager_le_audio_adv_state,
	.adv_enable = bt_manager_le_audio_adv_enable,
	.adv_disable = bt_manager_le_audio_adv_disable,
};
#endif

int sys_ble_advertise_init(void)
{
	struct ble_adv_mngr *p = &ble_adv_info;
	uint8 is_exist_reconn_phone_flag = 0;
	int role = bt_manager_tws_get_dev_role_ext(&is_exist_reconn_phone_flag);

	memset(p, 0, sizeof(struct ble_adv_mngr));

#ifdef CONFIG_GFP_PROFILE
	sys_ble_adv_register(ADV_TYPE_GFP, &gfp_ble_adv_funcs);
#endif
	sys_ble_adv_register(ADV_TYPE_OTA, &ota_ble_adv_funcs);
#ifdef LEAUDIO_LEGENCY_ADV
	sys_ble_adv_register(ADV_TYPE_LEAUDIO, &lea_adv_funcs);
#endif

	//Enable BLE adv when start ble function.
	p->adv_index = 0;
	p->wait_reconn_phone = is_exist_reconn_phone_flag;
	p->total_time_cnt = 1;

	if ((role == BTSRV_TWS_TEMP_MASTER) || (role == BTSRV_TWS_TEMP_SLAVE)) {
		p->begin_wait_time = os_uptime_get_32();
		p->wait_tws_paired = 1;
		p->tws_role = role;
	}

    p->adv_data = mem_malloc(ADV_DATA_MAX * sizeof(adv_data_t));
    if (p->adv_data == NULL) {
        SYS_LOG_ERR("malloc failed!");
        return -1;
    }

    p->adv_data_size = ADV_DATA_MAX * sizeof(adv_data_t);
	memset(p->adv_data, 0, p->adv_data_size);

	thread_timer_init(&p->check_adv_timer, sys_ble_check_advertise, NULL);
	thread_timer_start(&p->check_adv_timer, 0, ADV_INTERVAL_MS);

	SYS_LOG_INF("tws role:%d wait_recon:%d\n", p->tws_role, p->wait_reconn_phone);
	return 0;
}

void sys_ble_advertise_deinit(void)
{
	struct ble_adv_mngr *p = &ble_adv_info;

	thread_timer_stop(&p->check_adv_timer);

	bt_manager_ble_set_advertise_enable(false);

	sys_ble_adv_register(ADV_TYPE_OTA, NULL);
#ifdef CONFIG_GFP_PROFILE
	sys_ble_adv_register(ADV_TYPE_GFP, NULL);
#endif
#ifdef LEAUDIO_LEGENCY_ADV
	sys_ble_adv_register(ADV_TYPE_LEAUDIO, NULL);
#endif
#ifdef CONFIG_BT_LEATWS
	sys_ble_adv_register(ADV_TYPE_TWS, NULL);
#endif

    if(p->adv_data){
	    mem_free(p->adv_data);
	    p->adv_data = NULL;
	    p->adv_data_size = 0;
	}

	SYS_LOG_INF("adv deinit\n");
}
