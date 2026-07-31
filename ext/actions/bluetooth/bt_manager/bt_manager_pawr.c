/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt pawr manager.
 */
#define SYS_LOG_DOMAIN "btmgr_pawr"

#include <os_common_api.h>

#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <misc/byteorder.h>

#include <msg_manager.h>
#include <mem_manager.h>
#include <acts_bluetooth/host_interface.h>
#include <bt_manager.h>
#include <bt_manager_pawr.h>
#include "bt_manager_inner.h"
#include <sys_event.h>
#include <hex_str.h>
#include <acts_ringbuf.h>
#include <property_manager.h>


/******************************************************************/
//pawr  adv
#ifdef CONFIG_BT_PAWR

#define NUM_RSP_SLOTS	  2
#define NUM_SUBEVENTS	  1
//#define PACKET_SIZE	  5
#define SUBEVENT_INTERVAL 32//60//60/*80*///16

#define PAWR_SEND_BUFF_SIZE 512
#define PAWR_RSP_BUFF_SIZE 512
#define PAWR_RSP_DATA_COUNT 2

#define PAWR_MAX_SUBEVENT_DATA_LEN 59
#define PAWR_MAX_RSP_DATA_LEN      59

#define RSP_SLOT_SPACING       2 //((PAWR_MAX_RSP_DATA_LEN + 3)*4/125)

#define PER_SYNC_TIMEOUT  0xaa
#define PAWR_CREATE_SYNC_TIME		(20*1000)		/* 20s */

static uint8_t pawr_name[33];
static uint8_t pawr_force_mac[7] = {0};

struct pawr_mgr_adv_info {

    uint8_t advertising;
    uint8_t remote_dev_flag;
    uint8_t pending_release;
    bt_addr_le_t local_ble_addr;
	bt_addr_le_t remote_ble_addr;
    uint8_t pawr_subevent_data[PAWR_MAX_SUBEVENT_DATA_LEN];
    uint8_t pawr_send_buff[PAWR_SEND_BUFF_SIZE];

    struct acts_ringbuf *pawr_send_cache_buff;
	struct bt_le_ext_adv *pawr_adv;
	bt_pawr_vnd_rsp_cb rsp_cb;
	uint32_t last_time;
};

struct pawr_mgr_receive_info {
    bool per_adv_found;
	bt_addr_le_t local_ble_addr;
    uint8_t pending_release;
    uint8_t per_sid;
    uint8_t pa_synced;
    uint8_t pawr_rsp_data_len;
	uint8_t rsp_data_expect_count;
	uint8_t rsp_data_send_count;
    uint8_t pawr_rsp_data[PAWR_MAX_RSP_DATA_LEN];
    uint8_t pawr_rsp_buff[PAWR_RSP_BUFF_SIZE];
    struct acts_ringbuf *pawr_rsp_cache_buff;
	struct bt_le_per_adv_sync *pawr_sync;
	bt_pawr_vnd_rec_cb rec_data_cb;
	struct bt_le_scan_param scan_param;
	int rssi;
};

struct pawr_mgr_adv_info *pawr_adv_info = NULL;
struct pawr_mgr_receive_info *pawr_receive_info = NULL;
static os_delayed_work pawr_sync_create_work;
static bool pawr_start = false;

static const struct bt_le_adv_param ext_adv_params = {
	/* BT_LE_EXT_ADV_NCONN */
	.id = BT_ID_DEFAULT,
	/* [150ms, 150ms] by default */
	.interval_min = BT_GAP_ADV_FAST_INT_MAX_2,
	.interval_max = BT_GAP_ADV_FAST_INT_MAX_2,
    .options = BT_LE_ADV_OPT_EXT_ADV | BT_LE_ADV_OPT_USE_IDENTITY,
	.sid = BT_EXT_ADV_SID_PAWR,
};

// static const struct bt_le_per_adv_param per_adv_params = {
// 	.interval_min = 80,
// 	.interval_max = 80,
// 	.options = 0,
// 	.num_subevents = NUM_SUBEVENTS,
// 	.subevent_interval = SUBEVENT_INTERVAL,
// 	.response_slot_delay = 9/*0x2*/,
// 	.response_slot_spacing = 10/*20*/,
// 	.num_response_slots = NUM_RSP_SLOTS,
// };
#define RUNNING_CYCLES(end, start)	((uint32_t)((long)(end) - (long)(start)))

static void pawr_check_remote_dev_sync_lost(void)
{
    uint32_t cycles;
    uint32_t now_time;
	int err = 0;
	
	os_sched_lock();
    now_time = k_cycle_get_32();
	os_sched_unlock();
	
	if ((pawr_adv_info->last_time) && pawr_adv_info->remote_dev_flag) {
       cycles = RUNNING_CYCLES(now_time, pawr_adv_info->last_time);
       //SYS_LOG_INF("request_tine:%d \n",cycles);
	   if (cycles > ((PER_SYNC_TIMEOUT-15)*10*1000*24)) {

		  pawr_adv_info->remote_dev_flag = 0;
		  pawr_adv_info->last_time = 0;
	      bt_manager_event_notify(BT_PAWR_SYNC_LOST, NULL, 0);
		  if (pawr_adv_info->pawr_adv && (!pawr_adv_info->pending_release)) {
			err = hostif_bt_le_ext_adv_start(pawr_adv_info->pawr_adv, BT_LE_EXT_ADV_START_DEFAULT);
			SYS_LOG_INF("restart ext adv:%d \n",err);
		  }
		  SYS_LOG_INF("lost:\n");
	   }
    }
}

static void pawr_data_request_cb(struct bt_le_ext_adv *adv, const struct bt_le_per_adv_data_request *request)
{
	int err;
	uint8_t to_send;
	uint8_t p_buf[3];
	uint8_t p_size;
	struct net_buf_simple p_net_bufs;
	struct bt_le_per_adv_subevent_data_params subevent_data_params[NUM_SUBEVENTS];

	int ret;

	if (!pawr_adv_info) {
		return;
	}

	if (pawr_adv_info->pawr_adv != adv) {
		return;
	}

	if (pawr_adv_info->pending_release) {
		return;
	}

	pawr_check_remote_dev_sync_lost();

#ifdef CONFIG_BT_HFP_HF
	if(bt_manager_hfp_get_status() != BT_STATUS_HFP_NONE){
		return;
	}
#endif

	os_sched_lock();    
	if (acts_ringbuf_length(pawr_adv_info->pawr_send_cache_buff) < 2){
		goto err_exit;
	}

    ret = acts_ringbuf_get(pawr_adv_info->pawr_send_cache_buff, p_buf, 1);
	if (ret != 1) {
		SYS_LOG_WRN("want read %d bytes\n", ret);
		goto err_exit;
	}
	p_size = p_buf[0];
    ret = acts_ringbuf_get(pawr_adv_info->pawr_send_cache_buff, pawr_adv_info->pawr_subevent_data, p_size);
	if (ret != p_size) {
		SYS_LOG_WRN("want read %d(%d) bytes\n", p_size,ret);
	}
	os_sched_unlock();
	net_buf_simple_init_with_data(&p_net_bufs, pawr_adv_info->pawr_subevent_data,
					     p_size);

	/* Continuously send the same dummy data and listen to all response slots */
	to_send = MIN(request->count, ARRAY_SIZE(subevent_data_params));
	for (size_t i = 0; i < to_send; i++) {
		subevent_data_params[i].subevent =
			(request->start + i) % NUM_SUBEVENTS;
		subevent_data_params[i].response_slot_start = 0;
		subevent_data_params[i].response_slot_count = NUM_RSP_SLOTS;
		subevent_data_params[i].data = &p_net_bufs;
	}

	err = hostif_bt_le_per_adv_set_subevent_data(adv, to_send, subevent_data_params);
	if (err) {
		printk("Failed to set subevent data (err %d)\n", err);
	}
    return;

err_exit:
    os_sched_unlock();

}

static bool get_address(struct bt_data *data, void *user_data)
{
	bt_addr_le_t *addr = user_data;

	if (data->type == BT_DATA_LE_BT_DEVICE_ADDRESS) {
		memcpy(addr->a.val, &data->data[1], sizeof(addr->a.val));
		addr->type = data->data[0];

		return false;
	}

	return true;
}

static bool data_cb(struct bt_data *data, void *user_data)
{
	char *name = user_data;
	uint8_t len;

	switch (data->type) {
	case BT_DATA_NAME_SHORTENED:
	case BT_DATA_NAME_COMPLETE:
		len = MIN(data->data_len, NAME_LEN - 1);
		memcpy(name, data->data, len);
		name[len] = '\0';
		return false;
	default:
		return true;
	}
}

static bool pawr_get_vnd_rsp_data(struct bt_data *data, void *user_data)
{
	//u16_t *ver = (u16_t *)user_data;
	//printk("vnd_type:%d\n",data->type);
	switch (data->type) {
	case BT_DATA_MANUFACTURER_DATA:
		//printk("%d,%x,%x,%x,%x\n",data->data_len,data->data[0],data->data[1],data->data[2],data->data[3]);
		//*ver = pawr_vnd_protocol_deal(data->data,data->data_len,0);
		if (pawr_adv_info->rsp_cb) {
			pawr_adv_info->rsp_cb(data->data,data->data_len);
		}
		return false;
	default:
		return true;
	} 
}
static void pawr_response_cb(struct bt_le_ext_adv *adv, struct bt_le_per_adv_response_info *info,
			struct net_buf_simple *buf)
{
    //printk("pawr rsp cb:\n");
#if 1
    //uint8_t p_rsp_data[6];
	struct net_buf_simple_state state;
	bt_addr_le_t p_peer;
	char addr_str[BT_ADDR_LE_STR_LEN];
	int err = 0;
	char name[NAME_LEN];

	if (!pawr_adv_info) {
		return;
	}

	if (pawr_adv_info->pawr_adv != adv) {
		return;
	}

	if (!buf) {
		return;
	}

	if (pawr_adv_info->pending_release) {
		return;
	}

#ifdef CONFIG_BT_HFP_HF
	if(bt_manager_hfp_get_status() != BT_STATUS_HFP_NONE){
		return;
	}
#endif

	net_buf_simple_save(buf, &state);
	bt_data_parse(buf, get_address, &p_peer);		
	net_buf_simple_restore(buf, &state);

	//pawr_adv_info->remote_dev_match = 0;
	if (0 == pawr_adv_info->remote_dev_flag) {
		if (1 == pawr_force_mac[0]) {
			if (memcmp(&p_peer.a.val[0], &pawr_force_mac[1], 6)) {
				return;
			}
			SYS_LOG_INF("mac match.\n");
		} else {
			(void)memset(name, 0, sizeof(name));
			net_buf_simple_save(buf, &state);
			bt_data_parse(buf, data_cb, name);
			net_buf_simple_restore(buf, &state);
			printk(":%s \n",name);
			if (strcmp(name, pawr_name)) {
				SYS_LOG_INF("name not match.\n");
				return;
			}
			SYS_LOG_INF("name match.\n");
		}
	}

    if (!(pawr_adv_info->remote_dev_flag)) {
       memcpy(&(pawr_adv_info->remote_ble_addr),&p_peer,sizeof(p_peer));
	   bt_addr_le_to_str(&p_peer, addr_str, sizeof(addr_str));
	   printk("sync_dev:%s\n", addr_str);
	   pawr_adv_info->remote_dev_flag = 1;
	   net_buf_simple_save(buf, &state);
	   bt_data_parse(buf, pawr_get_vnd_rsp_data, NULL);
	   net_buf_simple_restore(buf, &state);
	   bt_manager_event_notify(BT_PAWR_SYNCED, &p_peer.a, 6);

	   //todo 1v1 : close ext adv?
	   err = hostif_bt_le_ext_adv_stop(pawr_adv_info->pawr_adv);
	   SYS_LOG_INF("ext adv stop:%d \n",err);
	} else {		
	   if (memcmp(&p_peer, &(pawr_adv_info->remote_ble_addr),sizeof(p_peer)) != 0) {
		/* No address found */
		return;
	   }		
	}

	os_sched_lock();
    pawr_adv_info->last_time = k_cycle_get_32();
    os_sched_unlock();

	bt_data_parse(buf, pawr_get_vnd_rsp_data, NULL);
#endif 	
}

static const struct bt_le_ext_adv_cb pawr_adv_cb = {
	.pawr_data_request = pawr_data_request_cb,
	.pawr_response = pawr_response_cb,
};

static int pawr_adv_init_bufs(void)
{
    int res;

	if (pawr_adv_info) {
		SYS_LOG_ERR("pawr adv info malloc already\n");
		return -EBUSY;
	}

	pawr_adv_info = mem_malloc(sizeof(struct pawr_mgr_adv_info));
	if (!pawr_adv_info) {
		SYS_LOG_ERR("pawr adv info malloc failed\n");
		res = -ENOMEM;
		goto err_exit;		
	}
	memset(pawr_adv_info,0,sizeof(struct pawr_mgr_adv_info));
	SYS_LOG_INF("\n");

    pawr_adv_info->pawr_send_cache_buff = mem_malloc(sizeof(struct acts_ringbuf));

	if (!pawr_adv_info->pawr_send_cache_buff) {
		res = -ENOMEM;
		SYS_LOG_ERR("send cache malloc failed\n");
		goto err_exit;
	}
    memset(pawr_adv_info->pawr_send_cache_buff,0,sizeof(struct acts_ringbuf));
	acts_ringbuf_init(pawr_adv_info->pawr_send_cache_buff,
				pawr_adv_info->pawr_send_buff,
				PAWR_SEND_BUFF_SIZE);
    return 0;

err_exit:

	if (pawr_adv_info) {
		mem_free(pawr_adv_info);
		pawr_adv_info = NULL;
	}	
	return res;	
}

static int pawr_adv_deinit_bufs(void)
{
    if (!pawr_adv_info){
 		return -ENOMEM;   
	}
    
	os_sched_lock();
	if (pawr_adv_info->pawr_send_cache_buff) {
		mem_free(pawr_adv_info->pawr_send_cache_buff);
		pawr_adv_info->pawr_send_cache_buff = NULL;
	}
	if (pawr_adv_info){
		mem_free(pawr_adv_info);
		pawr_adv_info = NULL;
	}
    os_sched_unlock();

    return 0;
}

#define PAWR_EXT_AD_ITEMS (2)
int bt_manager_pawr_set_ext_adv(u8_t *buf, u16_t len)
{
	struct bt_data ad_data[PAWR_EXT_AD_ITEMS];
	int items = 0;
	int err;

	if (!pawr_adv_info || !pawr_adv_info->pawr_adv) {
		SYS_LOG_ERR("pawr_adv is null.\n");
		return -EALREADY;
	}

	ad_data[items].type = BT_DATA_NAME_COMPLETE;
	ad_data[items].data_len = strlen(pawr_name);
	ad_data[items].data = pawr_name;
	items++;

	ad_data[items].type = BT_DATA_MANUFACTURER_DATA;
	ad_data[items].data_len = len;
	ad_data[items].data = buf;
	items++;

	err = hostif_bt_le_ext_adv_set_data(pawr_adv_info->pawr_adv, ad_data,
				items, NULL, 0);
	if (err != 0) {
		printk("set data: %d", err);
		return err;
	}

	return 0;
}

#endif /* #ifdef CONFIG_BT_PAWR */

int bt_manager_pawr_vnd_per_send(uint8_t *buf,uint8_t len, uint8_t type)
{
#ifdef CONFIG_BT_PAWR

    uint8_t ad_data[3];
    int ret = 0;

    if (len > PAWR_MAX_SUBEVENT_DATA_LEN) {
       SYS_LOG_WRN("data too long \n");        
       return -ENOMEM;
	}   
	if (!pawr_adv_info) {
        return -ENOMEM;		
	}
	os_sched_lock();

    if (acts_ringbuf_space(pawr_adv_info->pawr_send_cache_buff) < (len + 3) ) {
       SYS_LOG_WRN("no spc");
	   goto err_exit;
	}

	ad_data[0] = len + 2; //total data len
	ad_data[1] = len + 1; //data len + type
	ad_data[2] = type;
	ret = acts_ringbuf_put(pawr_adv_info->pawr_send_cache_buff, ad_data, 3);
	if (ret != 3) {
		SYS_LOG_WRN("want write (%d) bytes\n",ret);
	}
	ret = acts_ringbuf_put(pawr_adv_info->pawr_send_cache_buff, buf, len);
    os_sched_unlock();
    if (ret != len) {
		SYS_LOG_WRN("want write %d(%d) bytes\n", len, ret);
	}
	return ret;

err_exit:
    os_sched_unlock();
    return -ENOMEM;
#else

   return 0;

#endif  /*#ifdef CONFIG_BT_PAWR*/  
}

int bt_manager_pawr_adv_start(bt_pawr_vnd_rsp_cb cb,struct bt_le_per_adv_param *per_adv_param)
{	
#ifdef CONFIG_BT_PAWR

	int err;
	struct bt_le_per_adv_param pawr_per_adv_params = { 0 };

	SYS_LOG_INF("%p", pawr_adv_info);
	if ((pawr_adv_info != NULL) && (pawr_adv_info->advertising)) {
		pawr_adv_info->rsp_cb = cb;
		SYS_LOG_INF("already");
		return -EALREADY;
	}

	err = pawr_adv_init_bufs();
	if (err) {
		printk("Failed to init bufs (err %d)\n", err);
		return -EBUSY;
	}
	pawr_adv_info->rsp_cb = cb;
	SYS_LOG_INF(":\n");

	err = property_get("PAWR_NAME", pawr_name,
			   sizeof(pawr_name) - 1);
	if (err <= 0) {
		SYS_LOG_WRN("failed to get pawr name\n");
		memcpy(pawr_name, PAWR_NAME, PAWR_NAME_LEN);
	}
	/* Create a non-connectable non-scannable advertising set */
	err = hostif_bt_le_ext_adv_create(&ext_adv_params, &pawr_adv_cb, &(pawr_adv_info->pawr_adv));
	if (err) {
		printk("Failed to create advertising set (err %d)\n", err);
		return 0;
	}

	/* Set periodic advertising parameters */
	pawr_per_adv_params.interval_min = per_adv_param->interval_min;
	pawr_per_adv_params.interval_max = per_adv_param->interval_max;
	pawr_per_adv_params.options = 0;
	pawr_per_adv_params.num_subevents = NUM_SUBEVENTS;
	pawr_per_adv_params.subevent_interval = per_adv_param->subevent_interval;
	pawr_per_adv_params.response_slot_delay = per_adv_param->response_slot_delay;
	pawr_per_adv_params.response_slot_spacing = RSP_SLOT_SPACING;
	pawr_per_adv_params.num_response_slots = NUM_RSP_SLOTS;

	SYS_LOG_INF("per_param:%d,%d,%d,%d \n", pawr_per_adv_params.interval_min, pawr_per_adv_params.subevent_interval,
		    pawr_per_adv_params.response_slot_delay, pawr_per_adv_params.response_slot_spacing);

	err = hostif_bt_le_per_adv_set_param(pawr_adv_info->pawr_adv, &pawr_per_adv_params);
	if (err) {
		printk("Failed to set periodic advertising parameters (err %d)\n", err);
		return 0;
	}
	//pawr_set_ext_adv(pawr_adv_info->pawr_adv);
	/* Enable Periodic Advertising */
	err = hostif_bt_le_per_adv_start(pawr_adv_info->pawr_adv);
	if (err) {
		printk("Failed to enable periodic advertising (err %d)\n", err);
		return 0;
	}

	printk("Start Periodic Advertising\n");
	err = hostif_bt_le_ext_adv_start(pawr_adv_info->pawr_adv, BT_LE_EXT_ADV_START_DEFAULT);
	if (err) {
		printk("Failed to start extended advertising (err %d)\n", err);
		return 0;
	}
	pawr_adv_info->advertising = 1;

#endif /* #ifdef CONFIG_BT_PAWR */

	return 0;	
}

int bt_manager_pawr_adv_stop(void)
{
#ifdef CONFIG_BT_PAWR
	int err = 0;

	SYS_LOG_INF("%p", pawr_adv_info);
	if (!pawr_adv_info) {
		return -ENOMEM;
	}
	pawr_adv_info->pending_release = 1;
	if (pawr_adv_info->pawr_adv) {

		/* Stop periodic advertising */
		err = hostif_bt_le_per_adv_stop(pawr_adv_info->pawr_adv);
		if (err) {
			SYS_LOG_ERR("per_adv: %d", err);
			return err;
		}

		/* Stop extended advertising */
		err = hostif_bt_le_ext_adv_stop(pawr_adv_info->pawr_adv);
		if (err) {
			SYS_LOG_ERR("ext_adv: %d", err);
			//return err;
		}
		hostif_bt_le_ext_adv_delete(pawr_adv_info->pawr_adv); 
	}
	pawr_adv_deinit_bufs();
	return err;
#else
    return 0;
#endif /*#ifdef CONFIG_BT_PAWR*/
}


/////////////////////////////////////////////////////////////////////////////

//adv_sync

#ifdef CONFIG_BT_PAWR

static bt_addr_le_t per_addr;

static void pawr_sync_cb(struct bt_le_per_adv_sync *sync, struct bt_le_per_adv_sync_synced_info *info)
{
	struct bt_le_per_adv_sync_subevent_params params;
	//uint8_t subevents[2];
	uint8_t subevents[1];
	char le_addr[BT_ADDR_LE_STR_LEN];
	int err;
    struct bt_manager_context_t *bt_manager = bt_manager_get_context();

	bt_addr_le_to_str(info->addr, le_addr, sizeof(le_addr));

	if (!pawr_receive_info) {
		return;
	}
	if (pawr_receive_info->pawr_sync != sync) {
		return;
	}

	if (pawr_receive_info->pending_release) {
		return;
	}

	os_delayed_work_cancel(&pawr_sync_create_work);
	pawr_receive_info->pa_synced = 1;
	printk("Synced to %s with %d subevents\n", le_addr, info->num_subevents);
	params.properties = 0;
	params.num_subevents = 1/*2*/;
	params.subevents = subevents;
	subevents[0] = 0;
	//subevents[1] = 1;
	err = hostif_bt_le_per_adv_sync_subevent(sync, &params);
	if (err) {
		printk("Failed to set subevents to sync to (err %d)\n", err);
	}
	bt_manager->per_synced_count++;
	if (bt_manager->per_synced_count > bt_manager->total_per_sync_count) {
		SYS_LOG_ERR(":%d,%d \n",bt_manager->per_synced_count,bt_manager->total_per_sync_count);
	}
    bt_manager_check_per_adv_synced();

	//bt_manager_pawr_version_rsp(); // version resp, do not modify
	bt_manager_event_notify(BT_PAWR_SCAN_SYNCED, (void *)&info->addr->a.val[0], 6);
}

static int pawr_scan_start(void)
{
	int ret;

	ret = hostif_bt_le_scan_start(&pawr_receive_info->scan_param, NULL);
	SYS_LOG_INF("%d",ret);
	return ret;
}

static void pawr_term_cb(struct bt_le_per_adv_sync *sync,
		    const struct bt_le_per_adv_sync_term_info *info)
{
	char le_addr[BT_ADDR_LE_STR_LEN];
    struct bt_manager_context_t *bt_manager = bt_manager_get_context();

	bt_addr_le_to_str(info->addr, le_addr, sizeof(le_addr));

	printk("Sync term %d(reason %d),%d\n", info->sid,info->reason,pawr_receive_info->pending_release);
 
    if (!pawr_receive_info) {
		return;
	}
	if (pawr_receive_info->pawr_sync != sync) {
		return;
	}

	if (pawr_receive_info->pending_release) {
		return;
	}

	os_delayed_work_cancel(&pawr_sync_create_work);
    if (pawr_receive_info->pawr_sync)
    {
	   hostif_bt_le_per_adv_sync_delete(pawr_receive_info->pawr_sync);
	   pawr_receive_info->pawr_sync = NULL;
    } 
	
	if (bt_manager->per_synced_count) {
        bt_manager->per_synced_count--;
	}
	pawr_receive_info->per_adv_found = 0;
	if ((info->reason != 0x16) && (!pawr_receive_info->pending_release)) {
	   /*restart scan*/
       pawr_scan_start();
	}
	bt_manager_event_notify(BT_PAWR_SCAN_SYNC_LOST, (void *)&info->addr->a.val[0], 6);
	SYS_LOG_INF(":");
}

static void pawr_sync_create_timeout(os_work *work)
{
	SYS_LOG_INF("Sync timeout %d\n", pawr_receive_info->pending_release);

	if (!pawr_receive_info) {
		return;
	}

	if (!pawr_receive_info->pawr_sync) {
		return;
	}

	hostif_bt_le_per_adv_sync_delete(pawr_receive_info->pawr_sync);
	pawr_receive_info->pawr_sync = NULL;

	pawr_receive_info->per_adv_found = 0;
	if (!pawr_receive_info->pending_release) {
		/*restart scan*/
		pawr_scan_start();
	}
	bt_manager_event_notify(BT_PAWR_SCAN_SYNC_LOST, NULL, 0);
	SYS_LOG_INF(":");
}

static void pawr_send_rsp_data(struct bt_le_per_adv_sync *sync,
		    const struct bt_le_per_adv_sync_recv_info *info)
{
    struct bt_le_per_adv_response_params p_rsp_params;
    struct net_buf_simple p_rsp_bufs;
    uint8_t p_size;
    uint8_t p_buf[3];
    int ret;

    net_buf_simple_reset(&p_rsp_bufs);
	
	os_sched_lock();    
	if (acts_ringbuf_length(pawr_receive_info->pawr_rsp_cache_buff) < 2){
        if ((pawr_receive_info->pawr_rsp_data_len != 0) && 
		    ((pawr_receive_info->rsp_data_expect_count == 0) || 
			(pawr_receive_info->rsp_data_send_count < pawr_receive_info->rsp_data_expect_count))) {
          goto re_send_data;
		} else {
		  goto err_exit;
		}
	}

    ret = acts_ringbuf_get(pawr_receive_info->pawr_rsp_cache_buff, p_buf, 1);
	if (ret != 1) {
		SYS_LOG_WRN("want read %d bytes\n", ret);
		goto err_exit;
	}
	p_size = p_buf[0];
    ret = acts_ringbuf_get(pawr_receive_info->pawr_rsp_cache_buff, pawr_receive_info->pawr_rsp_data, p_size);
	if (ret != p_size) {
		SYS_LOG_WRN("want read %d(%d) bytes\n", p_size,ret);
	}
	pawr_receive_info->pawr_rsp_data_len = p_size;
    pawr_receive_info->rsp_data_send_count = 0;

re_send_data:
	os_sched_unlock();
	net_buf_simple_init_with_data(&p_rsp_bufs, pawr_receive_info->pawr_rsp_data,
					     pawr_receive_info->pawr_rsp_data_len);
    
	p_rsp_params.request_event = info->periodic_event_counter;
	p_rsp_params.request_subevent = info->subevent;
	p_rsp_params.response_subevent = info->subevent;
	p_rsp_params.response_slot = 1;

	ret = hostif_bt_le_per_adv_set_response_data(sync, &p_rsp_params, &p_rsp_bufs);
	if (ret) {
		printk("Failed to send response (err %d)\n", ret);
	}
	pawr_receive_info->rsp_data_send_count++;
	return;

err_exit:
   	os_sched_unlock();  

}

static bool pawr_get_vnd_per_data(struct bt_data *data, void *user_data)
{
	//u16_t *ver = (u16_t *)user_data;
	//printk("vnd_type:%d\n",data->type);
	switch (data->type) {
	case BT_DATA_MANUFACTURER_DATA:
		//printk("%d,%x,%x,%x,%x\n",data->data_len,data->data[0],data->data[1],data->data[2],data->data[3]);
		//*ver = pawr_vnd_protocol_deal(data->data,data->data_len,0);
		if (pawr_receive_info->rec_data_cb) {
			pawr_receive_info->rec_data_cb(data->data,data->data_len);
		}
		return false;
	default:
		return true;
	} 
}

static void pawr_recv_cb(struct bt_le_per_adv_sync *sync,
		    const struct bt_le_per_adv_sync_recv_info *info, struct net_buf_simple *buf)
{
    //uint8_t p_rsp_data[6];
	char le_addr[BT_ADDR_LE_STR_LEN];

	if (!pawr_receive_info) {
		return;
	}

	if (pawr_receive_info->pawr_sync != sync) {
		return;
	}

	if (pawr_receive_info->pending_release) {
		return;
	}

	bt_addr_le_to_str(info->addr, le_addr, sizeof(le_addr));
	//printk("recv %s with %d subevent\n", le_addr, info->subevent);
	
	if (buf && buf->len) {
		/* Respond with own address for the advertiser to connect to */	
		//bt_data_parse(buf, pawr_get_vnd_data,&pawr_receive_info->rec_remote_version);
		bt_data_parse(buf, pawr_get_vnd_per_data, NULL);
	} else if (buf) {
//		printk("Rec empty ind: sub %d\n", info->subevent);
	} else {
		printk("Failed to rec ind: sub %d\n", info->subevent);
	}
	pawr_send_rsp_data(sync,info);
}

static struct bt_le_per_adv_sync_cb pawr_sync_callbacks = {
	.synced = pawr_sync_cb,
	.term = pawr_term_cb,
	.recv = pawr_recv_cb,
};

static bool broadcast_audio_announcement(struct bt_data *data, void *user_data)
{
	uint32_t *broadcast_id = user_data;
	uint16_t uuid;

	if (data->type != BT_DATA_SVC_DATA16) {
		return true;
	}

    /*
	 BROADCAST_AUDIO_SIZE == 5 
	*/
	if (data->data_len != 5) {
		return true;
	}

	uuid = sys_get_le16(data->data);
	if (uuid != BT_UUID_BROADCAST_AUDIO_VAL) {
		return true;
	}

	*broadcast_id = sys_get_le24(data->data + BT_UUID_SIZE_16);

	/* Stop parsing */
	return false;
}

static void pawr_scan_recv(const struct bt_le_scan_recv_info *info, struct net_buf_simple *buf)
{
	char name[NAME_LEN];
	struct bt_le_per_adv_sync_param sync_create_param;
	int err;

	struct net_buf_simple_state state;
	uint32_t broadcast_id;


	if (info->adv_type != BT_GAP_ADV_TYPE_EXT_ADV) {
		return;
	}

	if (!pawr_receive_info) {
		return;
	}

	if (pawr_receive_info->pending_release) {
		return;
	}

	net_buf_simple_save(buf, &state);
	broadcast_id = INVALID_BROADCAST_ID;
	bt_data_parse(buf, broadcast_audio_announcement, (void *)&broadcast_id);
	net_buf_simple_restore(buf, &state);

	/* found a broadcast audio */
	if (broadcast_id != INVALID_BROADCAST_ID) {
		return;
	}

	SYS_LOG_INF("rssi %d, 0x%x.\n",info->rssi, info->addr->a.val[0]);
	if (0 == pawr_force_mac[0]) {
		if (pawr_receive_info->rssi && info->rssi < pawr_receive_info->rssi) {
			return;
		}
	}

	(void)memset(name, 0, sizeof(name));
	net_buf_simple_save(buf, &state);
	bt_data_parse(buf, data_cb, name);
	net_buf_simple_restore(buf, &state);
	printk(":%s \n",name);

	if (1 == pawr_force_mac[0]) {
		if (memcmp(&info->addr->a.val[0], &pawr_force_mac[1], 6)) {
			return;
		}
		printk("match mac.\n");
	} else if (strcmp(name, pawr_name)) {
		return;
	}

	if (!(pawr_receive_info->per_adv_found) && info->interval) {
		//bt_data_parse(buf, pawr_get_vnd_data,&pawr_receive_info->rec_remote_version);
		bt_data_parse(buf, pawr_get_vnd_per_data, NULL);

		pawr_receive_info->per_adv_found = true;

		pawr_receive_info->per_sid = info->sid;
		bt_addr_le_copy(&per_addr, info->addr);

	    printk("Creating Periodic Advertising Sync \n");
	    bt_addr_le_copy(&sync_create_param.addr, &per_addr);
	    sync_create_param.options = 0;
	    sync_create_param.sid = pawr_receive_info->per_sid;
	    sync_create_param.skip = 0;
	    sync_create_param.timeout = PER_SYNC_TIMEOUT;
	    err = hostif_bt_le_per_adv_sync_create(&sync_create_param, &(pawr_receive_info->pawr_sync));
	    if (err) {
		    printk("Failed to create sync (err %d)\n", err);
		    pawr_receive_info->per_adv_found = 0;
	    }
		// resp timout 20s.
		bt_manager_event_notify(BT_PAWR_SCAN_SYNCING, (void *)&info->addr->a.val[0], 6);
		os_delayed_work_submit(&pawr_sync_create_work, PAWR_CREATE_SYNC_TIME);
		return;
	}

	if (pawr_receive_info->per_adv_found && 
			(!memcmp(&per_addr, info->addr, 6))) {
		bt_data_parse(buf, pawr_get_vnd_per_data, NULL);
	}
}

static struct bt_le_scan_cb pawr_scan_callbacks = {
	.recv = pawr_scan_recv,
};


static int pawr_receive_init_bufs(void)
{
	if (pawr_receive_info) {
		SYS_LOG_ERR("pawr rsp info malloc already\n");
		return -EBUSY;
	}

	pawr_receive_info = mem_malloc(sizeof(struct pawr_mgr_receive_info));
	if (!pawr_receive_info) {
		SYS_LOG_ERR("pawr rsp info malloc failed\n");
		return -ENOMEM;
	}
	memset(pawr_receive_info,0,sizeof(struct pawr_mgr_receive_info));
	SYS_LOG_INF("\n");

    pawr_receive_info->pawr_rsp_cache_buff = mem_malloc(sizeof(struct acts_ringbuf));

	if (!pawr_receive_info->pawr_rsp_cache_buff) {
//		res = -ENOMEM;
		goto err_exit;
	}
    memset(pawr_receive_info->pawr_rsp_cache_buff,0,sizeof(struct acts_ringbuf));
	acts_ringbuf_init(pawr_receive_info->pawr_rsp_cache_buff,
				pawr_receive_info->pawr_rsp_buff,
				PAWR_RSP_BUFF_SIZE);
    return 0;

err_exit:

	if (pawr_receive_info) {
		mem_free(pawr_receive_info);
		pawr_receive_info = NULL;
	}	
	return -ENOMEM;
}

static int pawr_receive_deinit_bufs(void)
{
    if (!pawr_receive_info){
 		return -ENOMEM;   
	}
	os_sched_lock();
	if (pawr_receive_info->pawr_rsp_cache_buff) {
		mem_free(pawr_receive_info->pawr_rsp_cache_buff);
		pawr_receive_info->pawr_rsp_cache_buff = NULL;
	}
	if (pawr_receive_info){
		mem_free(pawr_receive_info);
		pawr_receive_info = NULL;
	}
	os_sched_unlock();
    return 0;
}
#endif /*#ifdef CONFIG_BT_PAWR*/

int bt_manager_pawr_vnd_rsp_send(uint8_t *buf,uint8_t len, uint8_t type)
{
#ifdef CONFIG_BT_PAWR

    uint8_t ad_data[60];
    int ret = 0;
	uint8_t pawr_len;
	uint16_t offset = 0;
	uint16_t total;

	if (!pawr_receive_info) {
		return -ENOMEM;
	}	

	os_sched_lock();
	pawr_len = strlen(pawr_name);
	if (pawr_len > sizeof(pawr_name) - 1) {
		pawr_len = sizeof(pawr_name) - 1;
	}

	if (len + (2+sizeof(bt_addr_le_t)+2+pawr_len)> PAWR_MAX_RSP_DATA_LEN) {
		SYS_LOG_ERR("data too long.\n");        
		goto err_exit;
	}

	total = 2+sizeof(bt_addr_le_t)+2+pawr_len+2+len;//total len
	if (acts_ringbuf_space(pawr_receive_info->pawr_rsp_cache_buff) < total) {
		SYS_LOG_WRN("no spc");
		goto err_exit;
	}

	ad_data[offset++] = total;
	// le addr
	ad_data[offset++] = sizeof(bt_addr_le_t) + 1;
	ad_data[offset++] = BT_DATA_LE_BT_DEVICE_ADDRESS; 
	memcpy(&(ad_data[offset]),&(pawr_receive_info->local_ble_addr),sizeof(bt_addr_le_t));
	offset += sizeof(bt_addr_le_t);

	// name
	ad_data[offset++] = pawr_len + 1; // data len + type
	ad_data[offset++] = BT_DATA_NAME_COMPLETE;
	memcpy(&(ad_data[offset]), pawr_name, pawr_len);
	offset += pawr_len;

	//
	ad_data[offset++] = len + 1; // data len + type
	ad_data[offset++] = type;
	ret = acts_ringbuf_put(pawr_receive_info->pawr_rsp_cache_buff, ad_data, offset);
	if (ret != offset) {
		SYS_LOG_WRN("want write (%d) bytes (%d)\n",ret, offset);
	}
	ret = acts_ringbuf_put(pawr_receive_info->pawr_rsp_cache_buff, buf, len);
    os_sched_unlock();
    if (ret != len) {
		SYS_LOG_WRN("want write %d(%d) bytes\n", len, ret);
	}
	return ret;

err_exit:
    os_sched_unlock(); 
    return -ENOMEM;
#else
    return 0;
#endif /*#ifdef CONFIG_BT_PAWR*/     
}

int bt_manager_pawr_receive_start(bt_pawr_vnd_rec_cb cb, struct bt_le_scan_param *param, int8 rssi)
{	
#ifdef CONFIG_BT_PAWR
	int err;

    struct bt_manager_context_t *bt_manager = bt_manager_get_context();
    bd_address_t bd_addr;

    err = pawr_receive_init_bufs();
	if (err) {
		SYS_LOG_WRN("failed to init rsp buf\n");
		return -EBUSY;
	}

    btif_br_get_local_mac(&bd_addr);
    memcpy(pawr_receive_info->local_ble_addr.a.val,bd_addr.val,sizeof(bd_address_t));
    pawr_receive_info->local_ble_addr.type = 0;

    err = property_get("PAWR_NAME", pawr_name,
			sizeof(pawr_name) - 1);
	if (err <= 0) {
		SYS_LOG_WRN("failed to get pawr name\n");
		memcpy(pawr_name, PAWR_NAME, PAWR_NAME_LEN);
	}

	hostif_bt_le_scan_cb_register(&pawr_scan_callbacks);
	hostif_bt_le_per_adv_sync_cb_register(&pawr_sync_callbacks);

	if (false == pawr_start) {
		os_delayed_work_init(&pawr_sync_create_work, pawr_sync_create_timeout);
		pawr_start = true;
	}

	if (!param) {
		pawr_receive_info->scan_param.type = BT_LE_SCAN_TYPE_PASSIVE;
		pawr_receive_info->scan_param.options = BT_LE_SCAN_OPT_NONE/*BT_LE_SCAN_OPT_FILTER_DUPLICATE*//*BT_LE_SCAN_OPT_NONE*/;
		/* [30ms, 60ms], almost 50% duty cycle by default */
		pawr_receive_info->scan_param.interval = 0x60;//176;
		pawr_receive_info->scan_param.window = 0x30;//70;
		pawr_receive_info->scan_param.timeout = 0;
	} else {
		memcpy(&pawr_receive_info->scan_param, param, sizeof(struct bt_le_scan_param));
	}

	err = pawr_scan_start();
	if (err) {
		printk("failed (err %d)\n", err);
        pawr_receive_deinit_bufs();
		return 0;
	}
	pawr_receive_info->rec_data_cb = cb;
	pawr_receive_info->rssi = rssi;

	//bt_manager_pawr_version_rsp();

	pawr_receive_info->rsp_data_expect_count = 0/*PAWR_RSP_DATA_COUNT*/;
	bt_manager->total_per_sync_count++;
	SYS_LOG_INF(":%d \n",bt_manager->total_per_sync_count);
	return err;
#else
    return 0;
#endif  /*#ifdef CONFIG_BT_PAWR*/ 
	
}

int bt_manager_pawr_receive_stop(void)
{
#ifdef CONFIG_BT_PAWR
    int err;
    struct bt_manager_context_t *bt_manager = bt_manager_get_context();
    if (!pawr_receive_info)
	{
        SYS_LOG_WRN("absent\n");
		return -ENOMEM;
	}

	if (true == pawr_start) {
		os_delayed_work_cancel(&pawr_sync_create_work);
	}

    if (pawr_receive_info) {
		pawr_receive_info->pending_release = 1;
	}
    err = hostif_bt_le_scan_stop();
    if (err) {
	   SYS_LOG_INF("err: %d", err);
    }

    if (pawr_receive_info->pawr_sync)
    {
	   hostif_bt_le_per_adv_sync_delete(pawr_receive_info->pawr_sync);
	   pawr_receive_info->pawr_sync = NULL;
    } 
    hostif_bt_le_scan_cb_unregister(&pawr_scan_callbacks);
    hostif_bt_le_per_adv_sync_cb_unregister(&pawr_sync_callbacks);
    pawr_receive_deinit_bufs();

	if (bt_manager->total_per_sync_count) {
		bt_manager->total_per_sync_count--;
	}
	if (bt_manager->per_synced_count) {
		bt_manager->per_synced_count--;
	}
    
	SYS_LOG_INF(":\n");
    return err;
#else
    return 0;
#endif /*#ifdef CONFIG_BT_PAWR*/    
}

void bt_manager_pawr_match_mac_set(u8_t *mac)
{
#ifdef CONFIG_BT_PAWR
	if (!mac) {
		pawr_force_mac[0] = 0;
		return;
	}

	pawr_force_mac[0] = 1;
	memcpy(&pawr_force_mac[1],mac,6);
	SYS_LOG_INF("0x%x%x%x%x%x%x.\n",
		pawr_force_mac[1] , pawr_force_mac[2],
		pawr_force_mac[3] , pawr_force_mac[4],
		pawr_force_mac[5] , pawr_force_mac[6]);

#else
#endif
}
