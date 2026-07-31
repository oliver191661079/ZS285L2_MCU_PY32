/*
 * Copyright (c) 2016 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt inquiry/page scan enable manager.
 *   this is manager device discoverable/connectable.
 */

#define SYS_LOG_DOMAIN "btsrv_scan"
#include "btsrv_os_common.h"
#include "btsrv_inner.h"

#define BTSRV_SCAN_DELAY_SET		1
#define BTSRV_SCAN_DELAY_RUN		200			/* 200ms */

enum {
	INQUIRY_GIAC,
	INQUIRY_LIAC,
};

enum {
	BT_NO_SCANS_ENABLE = 0,
	BT_INQUIRY_EANBEL_PAGE_DISABLE = 1,
	BT_INQUIRY_DISABLE_PAGE_ENABLE = 2,
	BT_INQUIRY_ENABLE_PAGE_ENABLE = 3,
    BT_INQUIRY_LIMIT_ENABLE = 4,
};

enum {
	BT_STANDARD_SCAN = 0,
	BT_INTERLACED_SCAN = 1,
};

struct btsrv_scan_priv {
	uint8_t user_discoverable:1;
	uint8_t user_connectable:1;
	uint8_t curr_discoverable:1;
	uint8_t curr_connectable:1;
	uint8_t curr_iac:2;
	uint8_t srv_discoverable:1;
	uint8_t srv_connectable:1;
    uint8_t tws_limit_inquiry:1;
    uint8_t user_visual_valid:1;
    uint8_t srv_visual_valid:1;
    uint8_t scan_update_force:1;
    uint8_t curr_scan_mode;
    uint8_t ext_scan_mode;
    struct bt_scan_parameter user_param[7];

#if BTSRV_SCAN_DELAY_SET
	uint8_t scan_timer_running:1;
	struct thread_timer scan_timer;
#endif
};

static const struct bt_scan_parameter default_scan_param = {
    .scan_mode = BTSRV_SCAN_MODE_DEFAULT_INQUIRY_PAGE,
	.inquiry_scan_window = 0x12,
	.inquiry_scan_interval = 0x1000,
	.inquiry_scan_type = 0,
	.page_scan_window = 0x12,
	.page_scan_interval = 0x800,
	.page_scan_type = 0,
};

static struct btsrv_scan_priv *p_scan;

static uint8_t btsrv_scan_discoverable_connectable_to_mode(uint8_t discoverable, uint8_t connectable)
{
	uint8_t mode = 0;

	if (discoverable) {
		mode |= BT_INQUIRY_EANBEL_PAGE_DISABLE;
	}

	if (connectable) {
		mode |= BT_INQUIRY_DISABLE_PAGE_ENABLE;
	}

	return mode;
}

static void btsrv_set_scan_complete_event_cb(uint8_t status, uint8_t *data, uint16_t len)
{
	SYS_LOG_INF("%d",status);
	if (status) {
        p_scan->scan_update_force = 1;
		btsrv_event_notify(MSG_BTSRV_BASE, MSG_BTSRV_UPDATE_SCAN_MODE,NULL);
		return;
	}
}


static void btsrv_scan_set_scan_mode(uint8_t discoverable, uint8_t connectable, uint8_t iac_mode,
								struct bt_scan_parameter *param)
{
	uint8_t req_visual_mode, curr_visual_mode;
	uint8_t update_visual_mode = 0;
	uint8_t update_scan_param = 0;
	uint8_t update_iac = 0;

	req_visual_mode = btsrv_scan_discoverable_connectable_to_mode(discoverable, connectable);
	curr_visual_mode = btsrv_scan_discoverable_connectable_to_mode(p_scan->curr_discoverable, p_scan->curr_connectable);

	if(p_scan->scan_update_force){
        p_scan->scan_update_force = 0;
		update_visual_mode = 1;
        update_scan_param = 1;
        update_iac = 1;
    }
    else{
    	if (req_visual_mode != curr_visual_mode) {
    		update_visual_mode = 1;
    	}
    
    	if (iac_mode != p_scan->curr_iac) {
    		update_iac = 1;
    	}
    
    	if (param->scan_mode != p_scan->curr_scan_mode) {
    		update_scan_param = 1;
    	}
    }

	if (!(update_visual_mode || update_scan_param || update_iac)) {
		return;
	}

	if (update_iac) {
		hostif_bt_br_write_iac(((iac_mode == INQUIRY_LIAC) ? true : false));
		p_scan->curr_iac = iac_mode;
	}

	if (update_scan_param) {
		/* Disable scan mode before update scan parameter */
		if (curr_visual_mode != BT_NO_SCANS_ENABLE) {
			if (hostif_bt_br_write_scan_enable(BT_NO_SCANS_ENABLE,btsrv_set_scan_complete_event_cb) == 0) {
				curr_visual_mode = BT_NO_SCANS_ENABLE;
				p_scan->curr_discoverable = 0;
				p_scan->curr_connectable = 0;
			} else {
				SYS_LOG_ERR("Failed to write_scan_enable !!!");
			}
		}

		if (curr_visual_mode != req_visual_mode) {
			update_visual_mode = 1;
		} else {
			update_visual_mode = 0;
		}
		/*If scan mode is BT_NO_SCANS_ENABLE, the scan parameters do not need to be updated */
		if (req_visual_mode == BT_NO_SCANS_ENABLE)
		{
		    update_scan_param = 0;
	      SYS_LOG_INF("not need to be updated:\n");
		}
	}

	if (update_scan_param) {
        if((param->inquiry_scan_interval != 0) && (param->inquiry_scan_window != 0)){
		    SYS_LOG_INF("set inquiry scan %d %d\n",param->inquiry_scan_interval, param->inquiry_scan_window);
		    hostif_bt_br_write_inquiry_scan_activity(param->inquiry_scan_interval, param->inquiry_scan_window);	
            if (!param->inquiry_scan_type) {
                hostif_bt_br_write_inquiry_scan_type(BT_STANDARD_SCAN);
            } 
			else {
                hostif_bt_br_write_inquiry_scan_type(BT_INTERLACED_SCAN);
            }
		}
	}
	if (update_scan_param) {
		SYS_LOG_INF("set page scan %d %d\n",param->page_scan_interval, param->page_scan_window);
		hostif_bt_br_write_page_scan_activity(param->page_scan_interval, param->page_scan_window);
		if (!param->page_scan_type) {
			hostif_bt_br_write_page_scan_type(BT_STANDARD_SCAN);
		} else {
			hostif_bt_br_write_page_scan_type(BT_INTERLACED_SCAN);
		}
	}

	if (update_scan_param) {
        p_scan->curr_scan_mode = param->scan_mode;
	}

	if (update_visual_mode) {
		if (hostif_bt_br_write_scan_enable(req_visual_mode,btsrv_set_scan_complete_event_cb) == 0) {
			p_scan->curr_discoverable = discoverable;
			p_scan->curr_connectable = connectable;
		} else {
			SYS_LOG_ERR("Failed to write_scan_enable !!!");
		}
	}
}

static void btsrv_scan_check_state(void)
{
	bool reconnect_phone;
	int dev_role;
	uint8_t exp_discoverable = 0;
	uint8_t ext_connectable = 0;
	uint8_t iac_mode = INQUIRY_GIAC;
	uint8_t max_device_num;
    uint8_t pair_state;
    uint8_t scan_mode = BTSRV_SCAN_MODE_DEFAULT_INQUIRY_PAGE;
	uint8_t connected_dev;
    uint8_t check_scan = 0;
	struct bt_scan_parameter param;
    struct btsrv_info_t *btsrv_context;
	struct autoconn_info *tws_info;

#if BTSRV_SCAN_DELAY_SET
	if (thread_timer_is_running(&p_scan->scan_timer)) {
		thread_timer_stop(&p_scan->scan_timer);
	}
#endif

	btsrv_context = btsrv_adapter_get_context();
    pair_state = btsrv_adapter_get_pair_state();
	reconnect_phone = btsrv_autoconn_is_phone_connecting();
	dev_role = btsrv_rdm_get_dev_role();

    if(btsrv_tws_is_advanced_mode()){
		connected_dev = btsrv_rdm_get_connected_dev_cnt_by_type(BTSRV_DEVICE_PHONE);
        if(dev_role != BTSRV_TWS_NONE){
            max_device_num = (btsrv_cfg_device_num() > 1) ? btsrv_cfg_device_num() - 1 : 0;
        }
        else{
            max_device_num = btsrv_max_phone_num();
        }

		if ((connected_dev == 1)&&(max_device_num == 2)&& btsrv_adapter_leaudio_is_connected()){
			SYS_LOG_INF("Both LEA and BR are connected");
			connected_dev = 2;
		}
	}
    else{
	    connected_dev = btsrv_rdm_get_connected_dev(NULL,NULL);
        max_device_num = btsrv_cfg_device_num();
    }

	p_scan->tws_limit_inquiry = 0;

    if(p_scan->user_visual_valid){
		exp_discoverable = p_scan->user_discoverable;
		scan_mode = p_scan->ext_scan_mode;
    }
	else if(p_scan->srv_visual_valid){
		exp_discoverable = p_scan->srv_discoverable;
		scan_mode = p_scan->ext_scan_mode;
	}
	else if (connected_dev >= max_device_num)
	{
		exp_discoverable = 0;
	}
	else if (btsrv_connect_is_pending()) {
		exp_discoverable = 0;
	}
	else if (dev_role == BTSRV_TWS_SLAVE) {
		exp_discoverable = 0;
	}
    else if(pair_state & BT_PAIR_STATUS_NODISC_NOCON){
		exp_discoverable = 0;
    }
	else if(pair_state & BT_PAIR_STATUS_TWS_SEARCH){
        if (btsrv_scan_get_inquiry_mode()){
    		if ((connected_dev == 0)
				||(!btsrv_a2dp_is_stream_start() && !btsrv_sco_get_conn())){
    			scan_mode = BTSRV_SCAN_MODE_FAST_INQUIRY_PAGE;
    		}		 
    		if (!btsrv_tws_is_advanced_mode()) {
    		    p_scan->tws_limit_inquiry = 1;
    		}
    		exp_discoverable = 1;
    	}
    	else{
    		exp_discoverable = 0;
    	}    
    }
	else if(pair_state & BT_PAIR_STATUS_PAIR_MODE){
		if((connected_dev == 0)
			|| (!btsrv_a2dp_is_stream_start()
			&& !btsrv_sco_get_conn())){
			scan_mode = BTSRV_SCAN_MODE_FAST_INQUIRY_PAGE;
		}
		else if(btsrv_sco_get_conn()){
			scan_mode = BTSRV_SCAN_MODE_NORMAL_PAGE_EX;
		}
        else{
			scan_mode = BTSRV_SCAN_MODE_NORMAL_PAGE;
        }

        if(pair_state & BT_PAIR_STATUS_RECONNECT){
            scan_mode = BTSRV_SCAN_MODE_NORMAL_PAGE;
        }
		exp_discoverable = 1;
	}
    else if(pair_state & BT_PAIR_STATUS_RECONNECT){
#if 0
        if((btsrv_autoconn_is_nodiscoverable() == true)
			|| (btsrv_autoconn_is_phone_first_reconnect()== true)){
            exp_discoverable = 0;
        }
        else
#endif
//        if((btsrv_autoconn_is_phone_first_reconnect()== true)){
            exp_discoverable = 0;
//        }
//        else
//        {
//            exp_discoverable = 1;
//        }
		check_scan = 1;
        if(btsrv_tws_is_in_pair_searching()){
			check_scan = 0;
        }
    }
	else if(pair_state & BT_PAIR_STATUS_WAIT_CONNECT){
        if(btsrv_context->cfg.default_state_discoverable){
            exp_discoverable = 1;
        }
        else{
            exp_discoverable = 0;
            scan_mode = BTSRV_SCAN_MODE_FAST_PAGE_EX;
        }
		check_scan = 1;
        if(btsrv_tws_is_in_pair_searching()){
			check_scan = 0;
        }
	}
	else if(btsrv_context->cfg.default_state_discoverable 
		&& (btsrv_context->cfg.default_state_wait_connect_sec == 0)){
		if((connected_dev > 0) 
			&& (btsrv_context->cfg.bt_not_discoverable_when_connected)){
			exp_discoverable = 0;
		}
		else{
            exp_discoverable = 1;
		}
        check_scan = 1;
	}
	else{
		exp_discoverable = 0;
        check_scan = 1;
	}

	tws_info = btsrv_connect_get_tws_paired_info();
    if(check_scan){	
        if(tws_info != NULL && !btsrv_tws_is_advanced_mode()){
            connected_dev = btsrv_rdm_get_connected_dev_cnt_by_type(BTSRV_DEVICE_PHONE);
            if(connected_dev && btsrv_sco_get_conn()){
                scan_mode = BTSRV_SCAN_MODE_NORMAL_PAGE_EX;
            }
            else if(!btsrv_a2dp_is_stream_start() && !reconnect_phone){
                scan_mode = BTSRV_SCAN_MODE_NORMAL_PAGE_S3;
            }
            else{
                scan_mode = BTSRV_SCAN_MODE_NORMAL_PAGE;
            }			
        }
    }

    if (p_scan->tws_limit_inquiry) {
        iac_mode = INQUIRY_LIAC;
    } else {
        iac_mode = INQUIRY_GIAC;
    }

    if (btsrv_tws_is_advanced_mode()) {
	    connected_dev = btsrv_rdm_get_connected_dev(NULL,NULL);
        max_device_num = btsrv_cfg_device_num();
    }

    if(p_scan->user_visual_valid){
		ext_connectable = p_scan->user_connectable;
    }
	else if(p_scan->srv_visual_valid){
		ext_connectable = p_scan->srv_connectable;
	}
	else if(connected_dev >= max_device_num){
		ext_connectable = 0;
	}
#if 0
	else if (btsrv_connect_is_pending()) {
	    ext_connectable = 0;
	}
#endif
	else if (dev_role == BTSRV_TWS_SLAVE) {
		ext_connectable = 0;
	}
    else if(pair_state & BT_PAIR_STATUS_NODISC_NOCON){
		ext_connectable = 0;
    }
	else if(pair_state & BT_PAIR_STATUS_TWS_SEARCH){
    	ext_connectable = 1;   
    }
	else if(pair_state & BT_PAIR_STATUS_PAIR_MODE){
		ext_connectable = 1;
	}
	else if (btsrv_connect_get_gfp_status()){
	    ext_connectable = 1;
	}
    else if(pair_state & BT_PAIR_STATUS_RECONNECT){
        if((btsrv_autoconn_is_phone_first_reconnect()== true)){
			ext_connectable = 0;
        }
        else
        {
            ext_connectable = 1;
        }
    }
	else if(pair_state & BT_PAIR_STATUS_WAIT_CONNECT){
		ext_connectable = 1;
	} 		
	else {
		ext_connectable = 1;
	}

	memcpy(&param, &p_scan->user_param[scan_mode],sizeof(struct bt_scan_parameter));

	SYS_LOG_INF("disc:%d conn:%d iac:%d scan:%d state:%x",
		exp_discoverable,ext_connectable,
		iac_mode,scan_mode,
		pair_state);
    
	btsrv_scan_set_scan_mode(exp_discoverable, ext_connectable, iac_mode, &param);
}

#if BTSRV_SCAN_DELAY_SET
static void btsrv_scan_timer_handler(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	p_scan->scan_timer_running = 1;
	btsrv_scan_check_state();
	p_scan->scan_timer_running = 0;

#ifdef CONFIG_SUPPORT_TWS
    btsrv_tws_update_visual_pair_mode();
#endif	
  SYS_LOG_INF("Delay Set:\n");
}

static void btsrv_scan_restart_timer(void)
{
	if (thread_timer_is_running(&p_scan->scan_timer)) {
		thread_timer_stop(&p_scan->scan_timer);
	}
  SYS_LOG_INF("restart:\n");
	thread_timer_start(&p_scan->scan_timer, BTSRV_SCAN_DELAY_RUN, 0);
}
#endif

int btsrv_scan_set_param(struct bt_scan_parameter *param)
{
    if((param->inquiry_scan_interval == 0) || (param->inquiry_scan_window == 0)){
        param->inquiry_scan_interval = default_scan_param.inquiry_scan_interval;
        param->inquiry_scan_window = default_scan_param.inquiry_scan_window;
        param->inquiry_scan_type = default_scan_param.inquiry_scan_type;
    }

    if((param->page_scan_interval == 0) || (param->page_scan_window == 0)){
        param->page_scan_interval = default_scan_param.page_scan_interval;
        param->page_scan_window = default_scan_param.page_scan_window;
        param->page_scan_type = default_scan_param.page_scan_type;
    }

    if (param->scan_mode < BTSRV_SCAN_MODE_MAX) {
    	memcpy(&p_scan->user_param[param->scan_mode], param, sizeof(struct bt_scan_parameter));
		SYS_LOG_INF("scan_param:%d,%d,%d,%d,%d\n",param->scan_mode,p_scan->user_param[param->scan_mode].inquiry_scan_interval,
		p_scan->user_param[param->scan_mode].inquiry_scan_window,p_scan->user_param[param->scan_mode].page_scan_interval,p_scan->user_param[param->scan_mode].page_scan_window
		);
    }

    return 0;
}

uint8_t btsrv_scan_get_visual(void)
{
    uint8_t mode = 0;

    if(p_scan->tws_limit_inquiry){
        mode = BTSRV_VISUAL_MODE_LIMIT_DISC_CON;
    }
    else{
        if (p_scan->curr_discoverable) {
            mode |= BT_INQUIRY_EANBEL_PAGE_DISABLE;
        }

        if (p_scan->curr_connectable) {
            mode |= BT_INQUIRY_DISABLE_PAGE_ENABLE;
        }
    }
    return mode;
}

uint8_t btsrv_scan_get_mode(void)
{
    return p_scan->curr_scan_mode;
}

void btsrv_scan_set_user_visual(struct bt_set_user_visual *param)
{
	p_scan->user_visual_valid = param->enable ? 1 : 0;
	p_scan->user_discoverable = param->discoverable ? 1 : 0;
	p_scan->user_connectable = param->connectable ? 1 : 0;
    if(param->scan_mode < BTSRV_SCAN_MODE_MAX){
	    p_scan->ext_scan_mode = param->scan_mode;
    }
	else{
		p_scan->ext_scan_mode = BTSRV_SCAN_MODE_DEFAULT_INQUIRY_PAGE;
	}
	if(!p_scan->user_visual_valid){
	    p_scan->user_discoverable = 0;
	    p_scan->user_connectable = 0;
	}
	btsrv_scan_check_state();
#ifdef CONFIG_SUPPORT_TWS
    btif_tws_update_visual_pair();
#endif	
}

void btsrv_inner_set_service_visual(struct bt_set_user_visual *param)
{
    p_scan->srv_visual_valid = param->enable ? 1 : 0;
    p_scan->srv_discoverable = param->discoverable ? 1 : 0;
    p_scan->srv_connectable = param->connectable ? 1 : 0;
    p_scan->ext_scan_mode = param->scan_mode;

    if(param->scan_mode < BTSRV_SCAN_MODE_MAX){
	    p_scan->ext_scan_mode = param->scan_mode;
    }
	else{
		p_scan->ext_scan_mode = BTSRV_SCAN_MODE_DEFAULT_INQUIRY_PAGE;
	}
	if(!p_scan->srv_visual_valid){
	    p_scan->srv_discoverable = 0;
	    p_scan->srv_connectable = 0;
	}
	btsrv_scan_check_state();
}

void btsrv_scan_update_mode(bool immediate)
{
#if BTSRV_SCAN_DELAY_SET
	if (immediate) {
		btsrv_scan_check_state();
	} else {
		btsrv_scan_restart_timer();
		return;
	}
#else
	btsrv_scan_check_state();
#endif

#ifdef CONFIG_SUPPORT_TWS
    btsrv_tws_update_visual_pair_mode();
#endif

}

uint8_t btsrv_scan_get_inquiry_mode(void)
{

    struct btsrv_info_t *btsrv_context;
	btsrv_context = btsrv_adapter_get_context();

	if ((btsrv_context->cfg.pair_key_mode == BTSRV_PAIR_KEY_MODE_TWO)
		|| (btsrv_context->cfg.power_on_auto_pair_search)){
		return 1;
	}
	else{
		return 0;
	}

}

static struct btsrv_scan_priv btsrv_scan;
int btsrv_scan_init(void)
{
	p_scan = &btsrv_scan;

	memset(p_scan, 0, sizeof(struct btsrv_scan_priv));

	p_scan->user_discoverable = 1;		/* Enable discoverable after start */
	p_scan->user_connectable = 1;		/* Enable connectable after start */
	p_scan->srv_discoverable = 1;		/* Enable connectable after start */
	p_scan->srv_connectable = 1;		/* Enable connectable after start */
    p_scan->tws_limit_inquiry = 0;
	p_scan->user_visual_valid = 0;
    p_scan->srv_visual_valid = 0;
	p_scan->ext_scan_mode = BTSRV_SCAN_MODE_DEFAULT_INQUIRY_PAGE;

	memcpy(&p_scan->user_param, &default_scan_param, sizeof(struct bt_scan_parameter));
    p_scan->curr_scan_mode = BTSRV_SCAN_MODE_MAX;

#if BTSRV_SCAN_DELAY_SET
	thread_timer_init(&p_scan->scan_timer, btsrv_scan_timer_handler, NULL);
#endif
	return 0;
}

void btsrv_scan_deinit(void)
{
	if (p_scan == NULL) {
		return;
	}

#if BTSRV_SCAN_DELAY_SET
	while (p_scan->scan_timer_running) {
		os_sleep(10);
	}

	if (thread_timer_is_running(&p_scan->scan_timer)) {
		thread_timer_stop(&p_scan->scan_timer);
		  SYS_LOG_INF("scan_timer stop:\n");
	}
#endif

	p_scan = NULL;
}

void btsrv_scan_dump_info(void)
{
	printk("scan info\n");
	if (p_scan == NULL) {
		SYS_LOG_INF("p_scan not init");
		return;
	}

	printk("user_discoverable %d, user_connectable %d\n", p_scan->user_discoverable, p_scan->user_connectable);
	printk("srv_discoverable %d, srv_connectable %d\n", p_scan->srv_discoverable, p_scan->srv_connectable);
	printk("curr_discoverable %d, curr_connectable %d\n\n", p_scan->curr_discoverable, p_scan->curr_connectable);
}
