/*

*/
#define SYS_LOG_DOMAIN "btmgr_ble_tws"

#include <os_common_api.h>

#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <msg_manager.h>
#include <mem_manager.h>
#include <acts_bluetooth/host_interface.h>
#include <bt_manager.h>
#include "bt_manager_inner.h"
#include <sys_event.h>
#include <hex_str.h>

#include <property_manager.h>	

#include "btmgr_letws_inner.h"

#define LETWS_PAIR_SEARCH_UNIT_TIME_MS		(100)
#define LETWS_PAIR_SEARCH_ONE_TIME_MAX_DURATION (5*LETWS_PAIR_SEARCH_UNIT_TIME_MS)

#define CFG_BTMGR_LETWS_INFO	"BTMGR_LETWS_INFO"

#define TWS_LOCAL_NAME	"tws_local"
#define TWS_LOCAL_NAME_LEN		(sizeof(TWS_LOCAL_NAME) - 1)

static uint8_t tws_match_name[33];
static uint8_t tws_manuf_data[sizeof(tws_match_name) + 1]; //name + mismatch_rssi


#define  LETWS_SYNC_GROUP_LEN   2
#define  LETWS_SYNC_SEND_LEN	(32)

#define  LETWS_AIRTOUCH_TIME_DURATION     3000000  //3s

static struct btmgr_letws_context_t letws_context;

struct btmgr_letws_context_t *btmgr_get_letws_context(void)
{
	return &letws_context;
}

int bt_manager_letws_get_dev_role(void)
{
	int ret = 0;

	if (letws_context.letws_connected) {
        ret = letws_context.tws_role;
	}
	return ret;
}

uint16_t bt_manager_letws_get_handle(void)
{
	int ret = 0;

	if (letws_context.letws_connected) {
	   ret = letws_context.tws_handle;
	}	
	return ret;
}

struct letws_phone_conn_local_info letws_phone_conn_local_info;

struct letws_phone_conn_local_info *letws_phone_conn_local_info_get(void)
{
	return &letws_phone_conn_local_info;
}

static int bt_manager_letws_phone_conn_local_info_set(struct letws_phone_conn_info *p)
{
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();
	struct letws_phone_conn_local_info *p_local_info = letws_phone_conn_local_info_get();

	if (context == NULL)
	{
		SYS_LOG_ERR("\n");
		return -1;
	}

	SYS_LOG_INF("\n");

	memcpy(&p_local_info->info, p, sizeof(struct letws_phone_conn_info));
	p_local_info->vaild = 1;

	return 0;
}

static int bt_manager_letws_phone_conn_local_info_clean(void)
{
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();
	struct letws_phone_conn_local_info *p_local_info = letws_phone_conn_local_info_get();

	if (context == NULL)
	{
		SYS_LOG_ERR("\n");
		return -1;
	}

	SYS_LOG_INF("\n");

	memset(p_local_info, 0, sizeof(struct letws_phone_conn_local_info));
	return 0;
}

int bt_manager_letws_dump_state(void)
{
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();
	int ret;
    bt_mgr_saved_letws_info_t letws_save_info;

	os_mutex_lock(&context->letws_mutex, OS_FOREVER);

	printk("***letws dump state start***\n");

    if (!context)
    {
	    printk("letws:context null\n");
    }
    else 
    {
	    printk("letws:state:%d,%d,%d,%d\n", context->tws_role, context->temp_tws_role, context->letws_mode_state, context->letws_mode_state_temp);
    }

	os_mutex_unlock(&context->letws_mutex);

	ret = property_get(CFG_BTMGR_LETWS_INFO, (char *)&letws_save_info, sizeof(bt_mgr_saved_letws_info_t));
    if (ret >= 0) {
	    printk("letws:role:%d,addr_type:%d\n", letws_save_info.dev_role, letws_save_info.addr.type);
		print_buffer(&letws_save_info.addr.a, 1, 6, 16, 0);
	}

	printk("***letws dump state end***\n");

	return 0;
}



static void bt_manager_letws_send_protocol(uint8_t id, uint8_t *data, uint8_t len)
{
    uint8_t send_len;
    uint8_t command[LETWS_SYNC_SEND_LEN];
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();

	if ((!context) || (!context->tws_handle)) {
		return;
	}

	if (len > (LETWS_SYNC_SEND_LEN - 3)) {
		SYS_LOG_INF("Too length %d", len);
		return;
	}
	send_len = 4;
	//COMMAND_REQ_HANDSHAKE,defined in broadcast.h
	command[0] = 0xee;
	command[1] = 0xee;
	command[2] = id;
	command[3] = len;
    if (data) {
		memcpy(&command[4], data, len);
		send_len += len;
	} else {
 	    command[3] = 0;     
 	}
	bt_manager_audio_le_vnd_send(context->tws_handle, command, send_len);
}

void bt_manager_letws_version_info(void)
{   
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();
	struct letws_version_feature cmd;
	
	if (!context) {
		return;
	}
	memset(&cmd, 0, sizeof(cmd));
	cmd.version = 0;
	cmd.feature = 0;
	bt_manager_letws_send_protocol(LETWS_SYNC_VERSION_INFO,(uint8_t *)&cmd, sizeof(cmd));
}

void bt_manager_letws_send_conn_info(void)
{
    struct letws_phone_conn_info info;
	struct letws_phone_conn_local_info *p_local_info = letws_phone_conn_local_info_get();
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();
	uint16_t active_hdl = bt_manager_find_active_slave_handle();
	int status = bt_manager_media_get_status();

	if (!context) {
		return;
	}
    memset(&info, 0, sizeof(info));

	SYS_LOG_INF("local info vaild:%d\n", p_local_info->vaild);

	if (p_local_info->vaild)
	{
		memcpy(&info, &p_local_info->info, sizeof(struct letws_phone_conn_info));
	}
	else
	{
		if (active_hdl) {
			info.phone_connect = 1;
		}
		info.aux_status = context->aux_plugin;

		if (status == BT_STATUS_PLAYING || info.aux_status)
		{
			info.expect_role = 1; // master
		}
		bt_manager_letws_phone_conn_local_info_set(&info);
	}

    SYS_LOG_INF("letws p_c:%d,a_s:%d,e_r%d\n",info.phone_connect,info.aux_status, info.expect_role);
	bt_manager_letws_send_protocol(LETWS_SYNC_PHONE_CONN_INFO,(uint8_t *)&info, sizeof(info));
}

static void letws_ext_adv_sent_cb(struct bt_le_ext_adv *adv,
		     struct bt_le_ext_adv_sent_info *info)
{
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();
	os_mutex_lock(&context->letws_mutex, OS_FOREVER);
	if (context->ext_adv != adv) {
		SYS_LOG_ERR("adv err:%p(%p)\n", adv, context->ext_adv);
		os_mutex_unlock(&context->letws_mutex);
		return;
	}
	/*adv terminated by controller, restart adv */
	if(context->letws_mode_state == BT_LETWS_MODE_ADV){
		context->restart_adv = 1;
		SYS_LOG_INF("restart letws adv %p\n", context->ext_adv);
		os_delayed_work_submit(&context->letws_run_work,5000);
	}
	os_mutex_unlock(&context->letws_mutex);
}

static const struct bt_le_ext_adv_cb ext_adv_cbs = {
	.sent = letws_ext_adv_sent_cb,
	.connected = NULL,
	.scanned = NULL,
};

static int letws_set_ext_adv_data(struct bt_le_ext_adv *adv)
{
	struct bt_data ad_data[1];
	int items = 0;
	int err;
	uint8_t tmp_len = 0;

#if 0
	#define LEAUDIO_NAME "LEAUDIO_DEMO"
	#define LEAUDIO_NAME_LEN (sizeof(LEAUDIO_NAME) - 1)
	#define NAME_LEN 32

	static uint8_t name[33];
	
	memset(name, 0, sizeof(name));

	err = property_get(CFG_BLE_AUDIO, name,
			sizeof(name) - 1);
	if (err <= 0) {
		SYS_LOG_WRN("failed to get leaudio name,use default\n");
		memcpy(name, LEAUDIO_NAME, LEAUDIO_NAME_LEN);
	}

	ad_data[items].type = BT_DATA_NAME_COMPLETE;
	ad_data[items].data_len = strlen(name);
	ad_data[items].data = name;
	items++;
#else
    memcpy(tws_manuf_data,tws_match_name,strlen(tws_match_name));
#ifdef CONFIG_BT_LETWS_AIRTOUCH
    struct btmgr_letws_context_t* context = btmgr_get_letws_context();
	
    tmp_len = 1;	    
	tws_manuf_data[strlen(tws_match_name)] = context->mismatch_rssi; //mismatch_rssi
    SYS_LOG_INF("manuf_data:%d,%d \n",strlen(tws_manuf_data),strlen(tws_match_name));
#endif

	ad_data[items].type = BT_DATA_MANUFACTURER_DATA;
	ad_data[items].data_len = (strlen(tws_match_name) + tmp_len);
	ad_data[items].data = tws_manuf_data;
	items++;
#endif

	err = hostif_bt_le_ext_adv_set_data(adv, ad_data,
				items, NULL, 0);
	if (err != 0) {
		printk("set data: %d", err);
		return err;
	}

	SYS_LOG_INF("ok\n");

	return 0;
}


int bt_manager_letws_adv_start(uint8 dir_flag)
{	
	int err;
	struct bt_le_adv_param ext_adv_params = { 0 };
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();

	os_mutex_lock(&context->letws_mutex, OS_FOREVER);
	if (context->ext_adv) {
		SYS_LOG_ERR("tws adv is exist:");
		os_mutex_unlock(&context->letws_mutex);
		return 0;
	}

	if (dir_flag && (!context->le_remote_addr_valid)) {
		 SYS_LOG_ERR("dir ext adv err:");
	}	
	/* BT_LE_EXT_ADV_NCONN */
	ext_adv_params.id = BT_ID_DEFAULT;
	/* [150ms, 150ms] by default */
    ext_adv_params.interval_min = BT_GAP_ADV_FAST_INT_MIN_2;
    ext_adv_params.interval_max = BT_GAP_ADV_FAST_INT_MIN_2;
    ext_adv_params.options = (BT_LE_ADV_OPT_EXT_ADV | BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_NO_2M);

    if (dir_flag == 1) {   
        ext_adv_params.options |= BT_LE_ADV_OPT_DIR_MODE_LOW_DUTY; 

// #ifdef CONFIG_BT_WHITELIST
// 		ext_adv_params.options |= BT_LE_ADV_OPT_FILTER_CONN;
// #endif

// #ifdef CONFIG_BT_PRIVACY
// 	//	context->remote_ble_addr
// 		ext_adv_params.options |= BT_LE_ADV_OPT_DIR_ADDR_RPA;
// #endif
        ext_adv_params.peer = &context->remote_ble_addr;
    }
#if 1
	ext_adv_params.options |= BT_LE_ADV_OPT_USE_IDENTITY;
#endif
    ext_adv_params.sid = BT_EXT_ADV_SID_LETWS,

	err = hostif_bt_le_ext_adv_create(&ext_adv_params, &ext_adv_cbs, &context->ext_adv);
    if (err) {
		SYS_LOG_INF("create:%d \n",err);
	}

#if 1
	if (dir_flag == 0)
	{
		letws_set_ext_adv_data(context->ext_adv);
	}
#endif

	err = hostif_bt_le_ext_adv_start(context->ext_adv, BT_LE_EXT_ADV_START_DEFAULT);
	if (err) {
		SYS_LOG_INF("sta err:%d\n",err);
	}
	os_mutex_unlock(&context->letws_mutex);

	print_buffer(&context->remote_ble_addr,1,7,16,0);
    SYS_LOG_INF(":%d \n",dir_flag);
	return 0;
	
}	

int bt_manager_letws_adv_stop(void) {
	
	int err;
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();

	os_mutex_lock(&context->letws_mutex, OS_FOREVER);
	if (context->ext_adv == NULL) {
		SYS_LOG_ERR("ext_adv is null:");
		os_mutex_unlock(&context->letws_mutex);
		return 0;
	}  
    /* Stop extended advertising */
	err = hostif_bt_le_ext_adv_stop(context->ext_adv);
	if (err) {
		SYS_LOG_ERR("ext_adv: %d", err);
		os_mutex_unlock(&context->letws_mutex);
		return err;
	}
	err = hostif_bt_le_ext_adv_delete(context->ext_adv);
    if (err) {
		SYS_LOG_ERR("adv del err: %d", err);
	}
	context->ext_adv = NULL;
	if(context->restart_adv){
		os_delayed_work_cancel(&context->letws_run_work);
		context->restart_adv = 0;
	}
	os_mutex_unlock(&context->letws_mutex);
	SYS_LOG_INF();
	return err;
}

static void letws_run_work_callback(struct k_work *work)
{
	SYS_LOG_INF("letws_run_work_callback:\n");
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();
	os_mutex_lock(&context->letws_mutex, OS_FOREVER);
	if(context->restart_adv && context->letws_mode_state == BT_LETWS_MODE_ADV
		&& context->ext_adv){
		int err = hostif_bt_le_ext_adv_start(context->ext_adv, BT_LE_EXT_ADV_START_DEFAULT);
		SYS_LOG_INF("letws restart adv err:%d\n",err);
		if(!err || err == -EALREADY){
			context->restart_adv = 0;
		}else{
			os_delayed_work_submit(&context->letws_run_work,2000);
		}
	}else if(context->letws_mode_state == BT_LETWS_MODE_SCAN){
		if(!bt_manager_audio_le_get_scan_status()){
			bt_manager_audio_le_resume_scan();
			os_delayed_work_submit(&context->letws_run_work,5000);
		}
	}
	os_mutex_unlock(&context->letws_mutex);

}

static uint32_t letws_gen_rand32(void)
{
	uint32_t rand32;

	rand32 = os_cycle_get_32();

	return rand32;
}

void letws_pair_search_alternate_work_cb(struct k_work *work)
{
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();
	int alternate_timeout;
	uint32_t rand32;
	uint32_t rand32_range;
	uint8_t last_letws_mode_state_temp;

	SYS_LOG_INF("\n");

	os_mutex_lock(&context->letws_mutex, OS_FOREVER);

	if (context->letws_mode_state == BT_LETWS_MODE_SCAN_ADV_ALTERNATE)
	{
		if(context->tws_handle){
			SYS_LOG_INF("letws already connect,handle:0x%x\n", context->tws_handle);
			os_mutex_unlock(&context->letws_mutex);
			return;
		}

		if (context->letws_mode_state_temp == BT_LETWS_MODE_SCAN)
		{
#ifndef CONFIG_LETWS_PAIR_SEARCH_GEN_RANDOM_TEST
			bt_manager_audio_le_pause_scan();
			bt_manager_letws_adv_start(0);
#endif
			last_letws_mode_state_temp = context->letws_mode_state_temp;
			context->letws_mode_state_temp = BT_LETWS_MODE_ADV;
		}
		else if (context->letws_mode_state_temp == BT_LETWS_MODE_ADV)
		{
#ifndef CONFIG_LETWS_PAIR_SEARCH_GEN_RANDOM_TEST
			bt_manager_letws_adv_stop();
			bt_manager_audio_le_resume_scan();
#endif
			last_letws_mode_state_temp = context->letws_mode_state_temp;
			context->letws_mode_state_temp = BT_LETWS_MODE_SCAN;
		}
		else
		{
			SYS_LOG_ERR("letws_mode_state_temp:%d\n", context->letws_mode_state_temp);
			os_mutex_unlock(&context->letws_mutex);
			return;
		}

		rand32 = letws_gen_rand32();
		rand32_range = LETWS_PAIR_SEARCH_ONE_TIME_MAX_DURATION / LETWS_PAIR_SEARCH_UNIT_TIME_MS;
		alternate_timeout = (rand32 % rand32_range + 1) * LETWS_PAIR_SEARCH_UNIT_TIME_MS;

		SYS_LOG_INF("rand:%u,range:%u,t:%d\n", rand32, rand32_range, alternate_timeout);
		SYS_LOG_INF("letws_mode:%s->%s\n", ((last_letws_mode_state_temp == BT_LETWS_MODE_SCAN) ? "scan" : "adv"),
			((context->letws_mode_state_temp == BT_LETWS_MODE_SCAN) ? "scan" : "adv"));

		os_delayed_work_submit(&context->letws_pair_search_alternate_work, alternate_timeout);
	}
	else
	{
		SYS_LOG_WRN("letws not alternate,%d\n", context->letws_mode_state);
		os_mutex_unlock(&context->letws_mutex);
		return;
	}

	os_mutex_unlock(&context->letws_mutex);
}

void bt_manager_letws_stop_pair_search(void)
{
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();

	os_mutex_lock(&context->letws_mutex, OS_FOREVER);

__letws_mode_state_handler:
	if (context->letws_mode_state == BT_LETWS_MODE_SCAN) {
		bt_manager_audio_le_pause_scan();
		os_delayed_work_cancel(&context->letws_run_work);
		context->letws_mode_state = BT_LETWS_MODE_STATE_NONE;

	} else if (context->letws_mode_state == BT_LETWS_MODE_ADV) {
		bt_manager_letws_adv_stop();
		context->letws_mode_state = BT_LETWS_MODE_STATE_NONE;
	} else if (context->letws_mode_state == BT_LETWS_MODE_SCAN_ADV_ALTERNATE) {
		context->letws_mode_state = context->letws_mode_state_temp;
		context->letws_mode_state_temp = BT_LETWS_MODE_STATE_NONE;
		os_delayed_work_cancel(&context->letws_pair_search_alternate_work);
		goto __letws_mode_state_handler;
	}
	os_delayed_work_cancel(&context->letws_pair_search_work);

#ifdef CONFIG_BT_LETWS_AIRTOUCH	
	os_delayed_work_cancel(&context->letws_airtouch_pair_work);
#endif //#ifdef CONFIG_BT_LETWS_AIRTOUCH

	btif_audio_scan_recv_cb_register(NULL);

	if(context->tws_role == BTSRV_TWS_PENDING){
		context->tws_role = 0;
		context->temp_tws_role = 0;
	}
	os_mutex_unlock(&context->letws_mutex);
	SYS_LOG_INF("stop_pair_search:\n");
}

static void letws_pair_search_work_callback(struct k_work *work)
{
	SYS_LOG_INF("search_work_callback:\n");
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();

	os_mutex_lock(&context->letws_mutex, OS_FOREVER);
	context->mismatch_rssi = 0;
	os_mutex_unlock(&context->letws_mutex);

	bt_manager_letws_stop_pair_search();
	bt_mamager_letws_reconnect();
}

#ifdef CONFIG_BT_LETWS_AIRTOUCH
static void letws_airtouch_pair_work_callback(struct k_work *work)
{
    SYS_LOG_INF("airtouch_pair_work_callback:\n");
    bt_manager_letws_start_pair_search(BTSRV_TWS_ALTERNATE,0,0);
}
#endif //#ifdef CONFIG_BT_LETWS_AIRTOUCH

static bool letws_scan_match(uint8_t type, const uint8_t *data, uint8_t data_len, void *user_data)
{
    uint8_t tmp_len = 0;

#ifdef CONFIG_BT_LETWS_AIRTOUCH
    tmp_len = 1;
#endif 

    switch (type) {
	case BT_DATA_MANUFACTURER_DATA:
		if ((data_len == (strlen(tws_match_name) + tmp_len))
			&& !memcmp(data, tws_match_name, strlen(tws_match_name)))
		{
			SYS_LOG_INF("manufacturer data match ok\n");
#ifdef CONFIG_BT_LETWS_AIRTOUCH
			*(uint8_t *)user_data = data[strlen(tws_match_name)];
#endif			
			return true;
		}

		break;
    }

    return false;
}

static bool adv_parse_and_match(struct net_buf_simple *ad,
                bool (*match_func)(uint8_t type, const uint8_t *data,
                uint8_t data_len, void *user_data),
                void *user_data)
{
	bool match_result = false;

    while (ad->len > 1) {
        uint8_t len = net_buf_simple_pull_u8(ad);
        uint8_t type;

        if (len == 0) {
            break;
        }

        if (len > ad->len || ad->len < 1) {
            SYS_LOG_INF("AD malformed");
            break;
        }

        type = net_buf_simple_pull_u8(ad);
        if (match_func(type, ad->data, len - 1, user_data) == true) {
			match_result = true;
            break;
        }

        net_buf_simple_pull(ad, len - 1);
    }

	return match_result;
}

bool bt_manager_letws_scan_recv_cb(const struct bt_le_scan_recv_info *info, struct net_buf_simple *buf)
{
	bool ret = false;
	uint8_t remote_mismatch_rssi;
#ifdef CONFIG_BT_LETWS_AIRTOUCH
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();

	if ((context->lock_flag) &&
	    (bt_addr_le_cmp(&context->lock_addr,info->addr))) {
			return ret;
		}
	SYS_LOG_INF("rssi:%d,%d,%d \n",info->rssi,context->lock_flag,CONFIG_BT_LETWS_AIRTOUCH_RSSI);
#endif
	switch (info->adv_type)
	{
		case BT_GAP_ADV_TYPE_ADV_IND:
		case BT_GAP_ADV_TYPE_SCAN_RSP:
		case BT_GAP_ADV_TYPE_EXT_ADV:
		{	
			ret = adv_parse_and_match(buf, letws_scan_match, (void *)&remote_mismatch_rssi);
#ifdef CONFIG_BT_LETWS_AIRTOUCH
			if (ret) {
			    SYS_LOG_INF("mismatch rssi:%d,%d \n",context->mismatch_rssi,remote_mismatch_rssi);
				if ((context->mismatch_rssi) && (remote_mismatch_rssi)){
					SYS_LOG_INF("dont care rssi:\n");
					return true;                    
				}
				if (info->rssi > (CONFIG_BT_LETWS_AIRTOUCH_RSSI)) {
					if (context->lock_flag == 0) {
						bt_addr_le_copy(&context->lock_addr,info->addr);
					    context->last_time = k_cycle_get_32();
						context->lock_flag = 1;
					} else {
						if ((u32_t)(k_cycle_get_32() - context->last_time) / 24 > LETWS_AIRTOUCH_TIME_DURATION) {
						   return true;
						}
					}

				} else {
					context->lock_flag = 0;
					context->last_time = k_cycle_get_32();
				}
			}
			return false;
#else
            return ret;
#endif 
		}	
		default:
			break;
	}

	return ret;
}

void bt_manager_letws_start_pair_search(uint8_t role,int time_out_s, int dir_flag)
{
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();
	int alternate_timeout;
	uint32_t rand32;
	uint32_t rand32_range;

	if ((context->letws_mode_state == BT_LETWS_MODE_SCAN) && (!dir_flag)) {
	    SYS_LOG_WRN("pair search:? \n");
	}
	
	bt_manager_letws_stop_pair_search();

	os_mutex_lock(&context->letws_mutex, OS_FOREVER);
	if(context->tws_role == BTSRV_TWS_NONE){
		context->temp_tws_role = role;
		context->tws_role = BTSRV_TWS_PENDING;
	}else if(context->tws_handle){
		SYS_LOG_INF("letws pair failed %d 0x%x\n",context->tws_role,context->tws_handle);
		os_mutex_unlock(&context->letws_mutex);
		return;
	}

	if (dir_flag == 0)
	{
		btif_audio_scan_recv_cb_register(bt_manager_letws_scan_recv_cb);
		btif_audio_set_ble_tws_addr(NULL);
	}
	else
	{
		btif_audio_scan_recv_cb_register(NULL);
	}
	/*
	 le tws pair search
	*/
	if (role == BTSRV_TWS_MASTER) {
		bt_manager_audio_le_resume_scan();
		context->letws_mode_state = BT_LETWS_MODE_SCAN; 
		os_delayed_work_submit(&context->letws_run_work,5000);
	} else if (role == BTSRV_TWS_SLAVE) {
		//bt_manager_audio_dir_adv_init();
		bt_manager_letws_adv_start(!!dir_flag);
		context->letws_mode_state = BT_LETWS_MODE_ADV;
	} else if (role == BTSRV_TWS_ALTERNATE) {
		rand32 = letws_gen_rand32();
		rand32_range = LETWS_PAIR_SEARCH_ONE_TIME_MAX_DURATION / LETWS_PAIR_SEARCH_UNIT_TIME_MS;
		alternate_timeout = (rand32 % rand32_range + 1) * LETWS_PAIR_SEARCH_UNIT_TIME_MS;

		context->letws_mode_state_temp = (rand32 % 2 == 0) ? BT_LETWS_MODE_SCAN : BT_LETWS_MODE_ADV;

		SYS_LOG_INF("rand:%u,range:%u,t:%d\n", rand32, rand32_range, alternate_timeout);
		SYS_LOG_INF("letws_mode:%s\n", (context->letws_mode_state_temp == BT_LETWS_MODE_SCAN) ? "scan" : "adv");

		if (context->letws_mode_state_temp == BT_LETWS_MODE_SCAN)
		{
#ifndef CONFIG_LETWS_PAIR_SEARCH_GEN_RANDOM_TEST
			bt_manager_audio_le_resume_scan();
#endif
		}
		else if (context->letws_mode_state_temp == BT_LETWS_MODE_ADV)
		{
#ifndef CONFIG_LETWS_PAIR_SEARCH_GEN_RANDOM_TEST
			bt_manager_letws_adv_start(!!dir_flag);
#endif
		}
		context->letws_mode_state = BT_LETWS_MODE_SCAN_ADV_ALTERNATE;

		os_delayed_work_submit(&context->letws_pair_search_alternate_work, alternate_timeout);
	} else {
		SYS_LOG_ERR("letws input param role error,%d\n", role);
	}
	if(time_out_s){
		os_delayed_work_submit(&context->letws_pair_search_work,time_out_s*1000);
	}else{
		//os_delayed_work_submit(&bt_manager->letws_pair_search_work,60*1000);
	}
	os_mutex_unlock(&context->letws_mutex);
	SYS_LOG_INF("letws r:%d,d:%d,t:%d", role, dir_flag, time_out_s);
}

bool bt_manager_exist_letws_info(void)
{
    int ret;
	bt_mgr_saved_letws_info_t p;
	ret = property_get(CFG_BTMGR_LETWS_INFO, (char *)&p, sizeof(bt_mgr_saved_letws_info_t));
    if (ret <= 0) {
        SYS_LOG_ERR("letws info err:");
		return false;
	}
	return true;
}

void bt_manager_init_letws_info(bt_letws_vnd_rx_cb cb)
{
    int ret;
    bt_mgr_saved_letws_info_t p;
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();
	context->rx_cb = cb;
	os_mutex_init(&context->letws_mutex);
	os_delayed_work_init(&context->letws_pair_search_work, letws_pair_search_work_callback);
	os_delayed_work_init(&context->letws_pair_search_alternate_work, letws_pair_search_alternate_work_cb);
	os_delayed_work_init(&context->letws_run_work, letws_run_work_callback);
#ifdef CONFIG_BT_LETWS_AIRTOUCH	
	os_delayed_work_init(&context->letws_airtouch_pair_work, letws_airtouch_pair_work_callback);
#endif

#ifdef CONFIG_PROPERTY
	property_get_string("BT_LETWS_MATCH_NAME", tws_match_name, sizeof(tws_match_name) - 1, TWS_LOCAL_NAME);
#endif

	SYS_LOG_INF("letws match name: %s \n", tws_match_name);

    ret = property_get(CFG_BTMGR_LETWS_INFO, (char *)&p, sizeof(bt_mgr_saved_letws_info_t));
    if (ret <= 0) {
        SYS_LOG_ERR("letws info err:");
#ifdef CONFIG_BT_LETWS_AIRTOUCH
       bt_manager_airtouch_set_create_falg(1);
	   bt_manager_letws_start_pair_search(BTSRV_TWS_ALTERNATE,0,0);
#endif
		return;
	}
	memcpy(&(context->info),&p,sizeof(bt_mgr_saved_letws_info_t));
	bt_mamager_letws_reconnect();
	SYS_LOG_INF("init_letws_info:\n");
}


void bt_manager_save_letws_info(uint8_t role,bt_addr_le_t *addr)
{
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();
	os_mutex_lock(&context->letws_mutex, OS_FOREVER);

	context->info.dev_role = role;
	memcpy(&(context->info.addr),addr,sizeof(bt_addr_le_t));
	os_mutex_unlock(&context->letws_mutex);
	property_set(CFG_BTMGR_LETWS_INFO, (char *)&context->info, sizeof(bt_mgr_saved_letws_info_t));
	SYS_LOG_INF();
	print_buffer(addr,1,7,16,0);
}

void bt_manager_clear_letws_info(void)
{
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();
	os_mutex_lock(&context->letws_mutex, OS_FOREVER);

	memset(&context->info,0,sizeof(bt_mgr_saved_letws_info_t));
	btif_audio_set_ble_tws_addr(NULL);
	os_mutex_unlock(&context->letws_mutex);

	property_set(CFG_BTMGR_LETWS_INFO, NULL, 0);
	SYS_LOG_INF();
}

int bt_mamager_set_remote_ble_addr(bt_addr_le_t *addr)
{
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();

	if (!addr)
	{
		SYS_LOG_ERR("addr is null:");
		return 0;
	}
	os_mutex_lock(&context->letws_mutex, OS_FOREVER);
	context->le_remote_addr_valid = 1;
	memcpy(&(context->remote_ble_addr),addr,sizeof(bt_addr_le_t));
	btif_audio_set_ble_tws_addr(&(context->remote_ble_addr));
	os_mutex_unlock(&context->letws_mutex);

	print_buffer(&context->remote_ble_addr,1,7,16,0);
	SYS_LOG_INF();
	return 0;
}

int bt_mamager_letws_disconnect(int reason)
{
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();

	os_mutex_lock(&context->letws_mutex, OS_FOREVER);

	if (context->letws_mode_state == BT_LETWS_MODE_CONNECTED) {
		//bt_manager_audio_conn_disconnect(context->tws_handle);
		btif_conn_disconnect_by_handle(context->tws_handle,reason);
		context->letws_mode_state = BT_LETWS_MODE_DISCONNECT;
		context->letws_disconn_pending = 1;
	}

	os_mutex_unlock(&context->letws_mutex);
	SYS_LOG_INF("letws disconnect:%d,%d",reason,context->letws_mode_state);
	return 0;
}

int bt_mamager_letws_reconnect(void)
{
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();

	os_mutex_lock(&context->letws_mutex, OS_FOREVER);

	if(context->letws_mode_state == BT_LETWS_MODE_STATE_NONE
		&& context->info.dev_role != BTSRV_TWS_NONE){
		memcpy(&(context->remote_ble_addr),&(context->info.addr),sizeof(bt_addr_le_t));
		context->le_remote_addr_valid = 1;
		btif_audio_set_ble_tws_addr(&(context->remote_ble_addr));

		bt_manager_letws_start_pair_search(context->info.dev_role,0,1);
	}
#ifdef CONFIG_BT_LETWS_AIRTOUCH
	else {
		if ((context->letws_mode_state == BT_LETWS_MODE_STATE_NONE) &&
		    (!context->airtouch_disable_restart)) {
           SYS_LOG_INF("air touch restart:\n");
           bt_manager_airtouch_set_create_falg(1);
		   bt_manager_letws_start_pair_search(BTSRV_TWS_ALTERNATE,0,0);			
		}
	}
#endif
	os_mutex_unlock(&context->letws_mutex);
	SYS_LOG_INF("tws recon:\n");
	return 0;
}

void bt_manager_letws_reset(void)
{
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();
	os_mutex_lock(&context->letws_mutex, OS_FOREVER);

	bt_manager_letws_stop_pair_search();
	if (context->tws_role == BTSRV_TWS_MASTER) {
		bt_mamager_letws_disconnect(BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	}
	bt_manager_clear_letws_info();
	os_mutex_unlock(&context->letws_mutex);

	SYS_LOG_INF("letws_reset:\n");
}

static void bt_mamager_letws_proc_sync_version_info(uint8_t *data, uint8_t len)
{
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();

	if (!context){
        SYS_LOG_INF("letws context is null:");
   		return;
	}	

	if (context->exchange_version_finish == TWS_EXCHANGE_VERSION_SENDED) {
		bt_manager_letws_send_conn_info();
	}
	context->exchange_version_finish |= TWS_EXCHANGE_VERSION_RECEIVED;
	SYS_LOG_INF();
}

uint8_t bt_mamager_letws_judge_tws_role(struct letws_phone_conn_info *info)
{
	uint8_t tws_role = 0;
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();
 
	if (!context) {
		return 0;
	}
	os_mutex_lock(&context->letws_mutex, OS_FOREVER);

#ifdef CONFIG_BT_LETWS_ROLE_SWITCH
    struct letws_phone_conn_info *local_info;
	struct letws_phone_conn_info temp_info;
    uint16_t active_hdl = bt_manager_find_active_slave_handle();
	int status = bt_manager_media_get_status();
    struct letws_phone_conn_local_info *p_local_info = letws_phone_conn_local_info_get();

#if 0
	if (active_hdl && info->phone_connect) {
		SYS_LOG_INF();
		tws_role = context->tws_role;
	} else if (active_hdl) {
		tws_role = BTSRV_TWS_MASTER;
	} else if (info->phone_connect) {
		tws_role = BTSRV_TWS_SLAVE;	   	
	} else {
		tws_role = context->tws_role;
	}
#else
	local_info = &p_local_info->info;

	SYS_LOG_INF("vaild:%d\n", p_local_info->vaild);

	if (!p_local_info->vaild)
	{
		memset(&temp_info, 0, sizeof(struct letws_phone_conn_info));

		if (active_hdl) {
			temp_info.phone_connect = 1;
		}
		temp_info.aux_status = context->aux_plugin;

		if (status == BT_STATUS_PLAYING || temp_info.aux_status)
		{
			temp_info.expect_role = 1; // master
		}
		bt_manager_letws_phone_conn_local_info_set(&temp_info);
	}

	SYS_LOG_INF("letws info:%d_%d_%d_%d_%d_%d\n", context->tws_role, local_info->phone_connect, local_info->expect_role, 
		info->phone_connect,context->aux_plugin, info->expect_role);

	if (context->tws_role == BTSRV_TWS_MASTER)
	{
		if (local_info->expect_role)
		{
			if(info->expect_role && !local_info->aux_status && info->aux_status){
				tws_role = BTSRV_TWS_SLAVE;
			}else{
				tws_role = context->tws_role;
			}
		}
		else if (info->expect_role)
		{
			tws_role = BTSRV_TWS_SLAVE;
		}
		else if (local_info->phone_connect)
		{
			tws_role = context->tws_role;
		}
		else if (info->phone_connect)
		{
			tws_role = BTSRV_TWS_SLAVE;
		}
		else
		{
			tws_role = context->tws_role;
		}
	}
	else
	{
		if (info->expect_role)
		{
			if(local_info->expect_role && local_info->aux_status && !info->aux_status){
				tws_role = BTSRV_TWS_MASTER;
			}else{
				tws_role = context->tws_role;
			}
		}
		else if (local_info->expect_role)
		{
			tws_role = BTSRV_TWS_MASTER;
		}
		else if (info->phone_connect)
		{
			tws_role = context->tws_role;
		}
		else if (local_info->phone_connect)
		{
			tws_role = BTSRV_TWS_MASTER;
		}
		else
		{
			tws_role = context->tws_role;
		}
	}

	SYS_LOG_INF("letws judge role:%d \n", tws_role);
#endif

	if (context->tws_role != tws_role) {
        context->tws_role = tws_role;
		bt_manager_save_letws_info(context->tws_role,&(context->info.addr));
	    SYS_LOG_INF("tws_role:%d \n",context->tws_role);
	}
#endif
    context->letws_connected = 1;

	if(context->rx_cb){
		letws_context.rx_cb(context->tws_handle,NULL,0);
	}
	os_mutex_unlock(&context->letws_mutex);

    bt_manager_event_notify(BT_TWS_CONNECTION_EVENT, &(context->tws_handle), sizeof(context->tws_handle));
    return tws_role;
}

static void bt_mamager_letws_proc_sync_phone_conn_info(uint8_t *data, uint16_t len)
{

	struct letws_phone_conn_info *info = (void *)data;
    
	SYS_LOG_INF("letws r_p:%d,a_s:%d,exp_role:%d,stream:%d\n", info->phone_connect,info->aux_status,
				info->expect_role, info->stream_start);
	if (bt_mamager_letws_judge_tws_role(info) == BTSRV_TWS_NONE) {
		return;
	}
	return;
}

static void bt_mamager_letws_process(uint8_t id, uint8_t *data, uint8_t len)
{

	switch (id) {
	case LETWS_SYNC_VERSION_INFO:
		bt_mamager_letws_proc_sync_version_info(data, len);
		break;
	case LETWS_SYNC_PHONE_CONN_INFO:
		bt_mamager_letws_proc_sync_phone_conn_info(data, len);
		break;
	default:
		break;
	}
}

static int bt_mamager_letws_vnd_rx_cb(uint16_t handle, const uint8_t *buf, uint16_t len)
{
	SYS_LOG_INF("letws buf: %p, len: %d\n", buf, len);
    int i;

	for (i = 0; i < 4; i++) {
		printk("0x%x \n", buf[i]);
	}
	printk("\n");

	if ((buf[0] == 0xee) && (buf[1] == 0xee)) {
       bt_mamager_letws_process(buf[2],(uint8_t *)(buf+4),buf[3]);
	}
	else if (letws_context.rx_cb) {
		letws_context.rx_cb(handle,buf,len);
	}	
    return 0;
}

int bt_mamager_letws_connected(uint16_t handle)
{ 
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();
	bt_addr_le_t *le_addr =  btif_get_le_addr_by_handle(handle);
	int role = btif_get_conn_role(handle);;

	if (!le_addr) {
		SYS_LOG_ERR("invalid handle 0x%x\n",handle);
		return -EINVAL;
	}

	os_mutex_lock(&context->letws_mutex, OS_FOREVER);

#ifdef CONFIG_BT_LETWS_AIRTOUCH
    context->lock_flag = 0;
	context->airtouch_disable_restart = 0;
	os_delayed_work_cancel(&context->letws_airtouch_pair_work);
#endif
    context->mismatch_rssi = 0;

	if ((context->letws_mode_state == BT_LETWS_MODE_STATE_NONE)|| 
	    (context->letws_disconn_pending)) {
 	    
		SYS_LOG_INF("letws_mode_state:0x%x,%d\n", context->letws_mode_state, context->letws_disconn_pending); 
		btif_conn_disconnect_by_handle(handle,0x13);
		os_mutex_unlock(&context->letws_mutex);
		return 0;
	}

	SYS_LOG_INF("letws handle:0x%x,conn_role:%d\n", handle, role);
	SYS_LOG_INF("letws dump state:%d,%d,%d,%d\n", context->tws_role, context->temp_tws_role, context->letws_mode_state, context->letws_mode_state_temp);

	if (role == BT_ROLE_MASTER)
	{
		context->tws_role = BTSRV_TWS_MASTER;
	}
	else if (role == BT_ROLE_SLAVE)
	{
		context->tws_role = BTSRV_TWS_SLAVE;
	}
	context->temp_tws_role = 0;

	context->tws_handle = handle;
    bt_manager_save_letws_info(context->tws_role,le_addr);

    bt_manager_audio_le_vnd_register_rx_cb(handle, bt_mamager_letws_vnd_rx_cb);

__letws_mode_state_handler:
	if (context->letws_mode_state == BT_LETWS_MODE_SCAN) {
		bt_manager_audio_le_pause_scan();
		os_delayed_work_cancel(&context->letws_run_work);
	} else if (context->letws_mode_state == BT_LETWS_MODE_ADV) {
		bt_manager_letws_adv_stop();
	} else if (context->letws_mode_state == BT_LETWS_MODE_SCAN_ADV_ALTERNATE)
	{
		context->letws_mode_state = context->letws_mode_state_temp;
		context->letws_mode_state_temp = BT_LETWS_MODE_STATE_NONE;
		os_delayed_work_cancel(&context->letws_pair_search_alternate_work);
		goto __letws_mode_state_handler;
	}

    context->letws_mode_state = BT_LETWS_MODE_CONNECTED;

	os_delayed_work_cancel(&context->letws_pair_search_work);
	
	os_mutex_unlock(&context->letws_mutex);

    bt_manager_letws_version_info();
	/*
	 slave ����master ����connect event
	*/
	if ((context->exchange_version_finish == TWS_EXCHANGE_VERSION_RECEIVED) || 
	    ((role == BT_ROLE_SLAVE)) ) {
		bt_manager_letws_send_conn_info();
	}
	context->exchange_version_finish |= TWS_EXCHANGE_VERSION_SENDED;
	SYS_LOG_INF("letws exchange %d",context->exchange_version_finish);

    SYS_LOG_INF("letws role:%d %d\n",context->tws_role,context->temp_tws_role);
//    print_buffer(le_addr,1,7,16,0);

	bt_manager_lea_set_status(BT_STATUS_TWS_PAIRED, 1, NULL);
 
    return 0;	
}

void bt_mamager_letws_disconnected(uint16_t handle, uint8_t role, uint8_t reason)
{
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();
    if (bt_manager_audio_get_type(handle) == BT_TYPE_BR) {
		return;
	}
	uint8_t notify = 0;

	os_mutex_lock(&context->letws_mutex, OS_FOREVER);

#ifdef CONFIG_BT_LETWS_AIRTOUCH
    context->lock_flag = 0;
#endif
   context->mismatch_rssi = 0;

	context->letws_mode_state = BT_LETWS_MODE_STATE_NONE;
	context->letws_mode_state_temp = BT_LETWS_MODE_STATE_NONE;
	SYS_LOG_INF("letws 0x%x disconnected :0x%x \n",context->tws_handle,reason);
	if(context->tws_handle && context->letws_connected){
		notify = 1;
	}
	context->tws_handle = 0;
	context->letws_connected = 0;
	context->tws_role = 0;
	context->temp_tws_role = 0;
	context->exchange_version_finish = 0;
	context->letws_disconn_pending = 0;

	bt_manager_letws_phone_conn_local_info_clean();

	if(context->rx_cb){
		letws_context.rx_cb(0,NULL,0);
	}
	os_mutex_unlock(&context->letws_mutex);
	if(reason == 0x8){
		bt_mamager_letws_reconnect();
	}

	bt_manager_lea_set_status(BT_STATUS_TWS_UNPAIRED, 0, NULL);
	if(notify){
		bt_manager_event_notify(BT_TWS_DISCONNECTION_EVENT, &reason, sizeof(reason));
	}
}

#ifdef CONFIG_BT_LETWS_AIRTOUCH

void bt_manager_airtouch_set_create_falg(uint8_t val)
{
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();
	os_mutex_lock(&context->letws_mutex, OS_FOREVER);
    context->create_flag = val;
	os_mutex_unlock(&context->letws_mutex);
    SYS_LOG_INF("set airtouch create flag:%d\n",context->create_flag);	
}

uint8_t bt_manager_airtouch_get_create_falg(void)
{
	uint8_t ret;
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();
	os_mutex_lock(&context->letws_mutex, OS_FOREVER);
	ret = context->create_flag;
	os_mutex_unlock(&context->letws_mutex);	
	return ret;
}

void bt_manager_set_mismatch_rssi(uint8_t mismatch_rssi)
{
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();
	os_mutex_lock(&context->letws_mutex, OS_FOREVER);
    context->mismatch_rssi = mismatch_rssi;
	os_mutex_unlock(&context->letws_mutex);
    SYS_LOG_INF("set mismatch rssi:%d\n",context->mismatch_rssi);
}

void bt_manager_airtouch_disable_restart(uint8_t disable_restart)
{
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();
	
	os_mutex_lock(&context->letws_mutex, OS_FOREVER); 
	context->airtouch_disable_restart = disable_restart;
	os_mutex_unlock(&context->letws_mutex);
	SYS_LOG_INF("airtouch disable restart:%d\n",context->airtouch_disable_restart);   
}

void bt_manager_letws_airtouch_pair(uint8_t role,int time_out_s, int dir_flag)
{
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();

	os_mutex_lock(&context->letws_mutex, OS_FOREVER); 
	os_delayed_work_cancel(&context->letws_airtouch_pair_work);
	os_delayed_work_submit(&context->letws_airtouch_pair_work,time_out_s*1000);
	os_mutex_unlock(&context->letws_mutex);
	SYS_LOG_INF("airtouch pair timeout:%d\n",time_out_s);
}

#endif //#ifdef CONFIG_BT_LETWS_AIRTOUCH

void bt_manager_letws_deinit(void)
{
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();

	int time_out = 0;
	os_delayed_work_cancel(&context->letws_pair_search_work);
	os_delayed_work_cancel(&context->letws_pair_search_alternate_work);
	os_delayed_work_cancel(&context->letws_run_work);
#ifdef CONFIG_BT_LETWS_AIRTOUCH
	os_delayed_work_cancel(&context->letws_airtouch_pair_work);
#endif //#ifdef CONFIG_BT_LETWS_AIRTOUCH
	bt_mamager_letws_disconnect(BT_HCI_ERR_REMOTE_POWER_OFF);
	while (context->tws_handle && time_out++ < 500) {
		os_sleep(10);
	}

	SYS_LOG_INF("letws_deinit \n");
}

void bt_manager_letws_set_aux_status(int plugin)
{
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();
	os_mutex_lock(&context->letws_mutex, OS_FOREVER);
	context->aux_plugin = plugin;
	os_mutex_unlock(&context->letws_mutex);
	SYS_LOG_INF("letws aux status %d\n",plugin);
}

