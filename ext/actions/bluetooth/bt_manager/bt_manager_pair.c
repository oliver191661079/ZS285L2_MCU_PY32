/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt manager connect.
 */
#define SYS_LOG_DOMAIN "bt manager"

#include <os_common_api.h>

#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stream.h>
#include <sys_event.h>
#include <acts_bluetooth/host_interface.h>
#include <mem_manager.h>
#include <bt_manager.h>
#include "bt_manager_inner.h"
#include <property_manager.h>
#include "btservice_api.h"

static const bt_mgr_event_strmap_t bt_manager_enter_pair_mode_map[] =
{
    {BT_PAIR_MODE_STATE_NONE,              STRINGIFY(BT_PAIR_MODE_STATE_NONE)},
    {BT_PAIR_MODE_TWS_SYNC,    		       STRINGIFY(BT_PAIR_MODE_TWS_SYNC)},
    {BT_PAIR_MODE_TWS_SYNC_WAIT,           STRINGIFY(BT_PAIR_MODE_TWS_SYNC_WAIT)},
    {BT_PAIR_MODE_DISCONNECT_PHONE,        STRINGIFY(BT_PAIR_MODE_DISCONNECT_PHONE)},
    {BT_PAIR_MODE_CHECK_PHONE,             STRINGIFY(BT_PAIR_MODE_CHECK_PHONE)},
    {BT_PAIR_MODE_CHECK_RECONNECT,         STRINGIFY(BT_PAIR_MODE_CHECK_RECONNECT)},
    {BT_PAIR_MODE_START_PAIR,              STRINGIFY(BT_PAIR_MODE_START_PAIR)},
    {BT_PAIR_MODE_RUNNING,                 STRINGIFY(BT_PAIR_MODE_RUNNING)},
};
#define BT_MANAGER_ENTER_PAIR_MODE_STATE_STRS (sizeof(bt_manager_enter_pair_mode_map) / sizeof(bt_mgr_event_strmap_t))
const char *bt_manager_enter_pair_mode_state2str(int num)
{
	return bt_manager_evt2str(num, BT_MANAGER_ENTER_PAIR_MODE_STATE_STRS, bt_manager_enter_pair_mode_map);
}

static const bt_mgr_event_strmap_t bt_manager_clear_paired_list_map[] =
{
    {BT_CLEAR_PAIRED_LIST_NONE,             STRINGIFY(BT_CLEAR_PAIRED_LIST_NONE)},
    {BT_CLEAR_PAIRED_LIST_TWS_SYNC,    		STRINGIFY(BT_CLEAR_PAIRED_LIST_TWS_SYNC)},
    {BT_CLEAR_PAIRED_LIST_TWS_SYNC_WAIT,    STRINGIFY(BT_CLEAR_PAIRED_LIST_TWS_SYNC_WAIT)},
    {BT_CLEAR_PAIRED_LIST_DISCONNECT_PHONE, STRINGIFY(BT_CLEAR_PAIRED_LIST_DISCONNECT_PHONE)},
    {BT_CLEAR_PAIRED_LIST_PHONE_CLEAR,      STRINGIFY(BT_CLEAR_PAIRED_LIST_PHONE_CLEAR)},
    {BT_CLEAR_PAIRED_LIST_TWS_CLEAR,        STRINGIFY(BT_CLEAR_PAIRED_LIST_TWS_CLEAR)},
    {BT_CLEAR_PAIRED_LIST_COMPLETE,         STRINGIFY(BT_CLEAR_PAIRED_LIST_COMPLETE)},
	{BT_CLEAR_PAIRED_LIST_RESTORE_STATE, 	STRINGIFY(BT_CLEAR_PAIRED_LIST_RESTORE_STATE)},
};
#define BT_MANAGER_CLEAR_PAIRED_LIST_STATE_STRS (sizeof(bt_manager_clear_paired_list_map) / sizeof(bt_mgr_event_strmap_t))
const char *bt_manager_clear_paired_list_state2str(int num)
{
	return bt_manager_evt2str(num, BT_MANAGER_CLEAR_PAIRED_LIST_STATE_STRS, bt_manager_clear_paired_list_map);
}

static const bt_mgr_event_strmap_t bt_manager_link_status_map[] =
{
    {BT_STATUS_LINK_NONE,                   STRINGIFY(BT_STATUS_LINK_NONE)},
    {BT_STATUS_WAIT_CONNECT,    		    STRINGIFY(BT_STATUS_WAIT_CONNECT)},
    {BT_STATUS_PAIR_MODE,                   STRINGIFY(BT_STATUS_PAIR_MODE)},
    {BT_STATUS_CONNECTED,                   STRINGIFY(BT_STATUS_CONNECTED)},
    {BT_STATUS_DISCONNECTED,                STRINGIFY(BT_STATUS_DISCONNECTED)},
    {BT_STATUS_TWS_PAIRED,                  STRINGIFY(BT_STATUS_TWS_PAIRED)},
    {BT_STATUS_TWS_UNPAIRED, 				STRINGIFY(BT_STATUS_TWS_UNPAIRED)},
    {BT_STATUS_TWS_PAIR_SEARCH,             STRINGIFY(BT_STATUS_TWS_PAIR_SEARCH)},
};
#define BT_MANAGER_LINK_STATUS_STRS (sizeof(bt_manager_link_status_map) / sizeof(bt_mgr_event_strmap_t))
const char *bt_manager_link_status2str(int num)
{
	return bt_manager_evt2str(num, BT_MANAGER_LINK_STATUS_STRS, bt_manager_link_status_map);
}

int bt_manager_br_start_discover(struct btsrv_discover_param *param)
{
	return btif_br_start_discover(param);
}

int bt_manager_br_stop_discover(void)
{
	return btif_br_stop_discover();
}

int bt_manager_br_connect(bd_address_t *bd)
{
	return btif_br_connect(bd);
}

int bt_manager_br_disconnect(bd_address_t *bd)
{
	return btif_br_disconnect(bd);
}

void bt_manager_check_link_status(void)
{
    struct bt_manager_context_t*  bt_manager = bt_manager_get_context();
    uint8_t link_status = BT_STATUS_LINK_NONE;
    uint8_t letws_role = BTSRV_TWS_NONE;

#ifdef CONFIG_BT_LETWS
    letws_role = bt_manager_letws_get_dev_role();
#endif
    if (bt_manager_audio_is_lea_open()) {
        if (bt_manager_is_tws_pair_search()){
            link_status = BT_STATUS_TWS_PAIR_SEARCH;
        }
        else if (bt_manager_audio_get_cur_dev_num() > 0){
            link_status = BT_STATUS_CONNECTED;
        }
        else if (bt_manager_is_pair_mode() &&
                (bt_manager_tws_get_dev_role() != BTSRV_TWS_SLAVE) &&
                (letws_role != BTSRV_TWS_SLAVE)){
            link_status = BT_STATUS_PAIR_MODE;
        }
        else if (bt_manager_is_wait_connect()){
            link_status = BT_STATUS_WAIT_CONNECT;
        }
        else if ((bt_manager_tws_get_dev_role() == BTSRV_TWS_MASTER) ||
                 (letws_role == BTSRV_TWS_MASTER)){
            link_status = BT_STATUS_LINK_NONE;
        }
        else if ((bt_manager_tws_get_dev_role() == BTSRV_TWS_SLAVE) ||
                 (letws_role == BTSRV_TWS_SLAVE)){
            link_status = BT_STATUS_CONNECTED;
        }
    }else {
        if (bt_manager_is_tws_pair_search()){
            link_status = BT_STATUS_TWS_PAIR_SEARCH;
        }
        else if(bt_manager->connected_phone_num > 0){
            link_status = BT_STATUS_CONNECTED;
        }
        else if (bt_manager_is_pair_mode() && bt_manager_tws_get_dev_role() != BTSRV_TWS_SLAVE){
            link_status = BT_STATUS_PAIR_MODE;
        }
        else if (bt_manager_is_wait_connect()){
            link_status = BT_STATUS_WAIT_CONNECT;
        }
        else if (bt_manager_tws_get_dev_role() == BTSRV_TWS_MASTER){
            link_status = BT_STATUS_LINK_NONE;
        }
        else if (bt_manager_tws_get_dev_role() == BTSRV_TWS_SLAVE){
            link_status = BT_STATUS_CONNECTED;
        }
    }

    if(bt_manager->bt_state != link_status){
	    bt_manager->bt_state = link_status;
	    SYS_LOG_INF("%s",bt_manager_link_status2str(link_status));
		bt_manager_update_led_display();
    }
}

#if 0
static void bt_manager_disconnect_nonactive_dev(void)
{
	btif_br_disconnect_noactive_device();
}
#endif

bool bt_manager_is_wait_connect(void)
{
    struct bt_manager_context_t*  bt_manager = bt_manager_get_context();
    if(bt_manager->pair_status & BT_PAIR_STATUS_WAIT_CONNECT){
		return true;
    }
	else{
	    return false;
	}
}

bool bt_manager_is_pair_mode(void)
{
    struct bt_manager_context_t*  bt_manager = bt_manager_get_context();
    if(bt_manager->pair_status & BT_PAIR_STATUS_PAIR_MODE){
		return true;
    }
	else{
	    return false;
	}
}

bool bt_manager_is_tws_pair_search(void)
{
    struct bt_manager_context_t*  bt_manager = bt_manager_get_context();
    if(bt_manager->pair_status & BT_PAIR_STATUS_TWS_SEARCH){
		return true;
    }
	else{
	    return false;
	}
}

bool bt_manager_is_remote_in_pair_mode(void)
{
    struct bt_manager_context_t*  bt_manager = bt_manager_get_context();
    return bt_manager->remote_in_pair_mode;
}

void bt_manager_clear_remote_pair_mode(void)
{
    struct bt_manager_context_t*  bt_manager = bt_manager_get_context();
    bt_manager->remote_in_pair_mode = 0;
}

bool bt_manager_is_tws_paired_valid(void)
{
    return btif_br_check_tws_paired_info();
}

void bt_manager_check_visual_mode(void)
{
    btif_br_update_scan_mode();     
//    bt_manager_tws_set_visual_mode();
}

static bool bt_manager_tws_clear_shared_addr(void)
{
    bool ret;
    ret = btif_br_check_share_tws();
	if(ret){
		btif_br_clear_share_tws();
	}
    return ret;
}

void bt_manager_clear_paired_list_ex(void)
{
    struct bt_manager_context_t*  bt_manager = bt_manager_get_context();
	btmgr_pair_cfg_t *cfg_pair = bt_manager_get_pair_config();

	uint8_t role;
	uint8_t phone_num = 0;

    uint8_t state = bt_manager->clear_paired_list_state;

    if(bt_manager->clear_paired_list_state_update){
		bt_manager->clear_paired_list_state_update = false;
	    SYS_LOG_INF("%s",bt_manager_clear_paired_list_state2str(state));
    }

    switch(state){
		case BT_CLEAR_PAIRED_LIST_TWS_SYNC:

            bt_manager_tws_end_pair_search();

			role = bt_manager_tws_get_dev_role();
		    phone_num = bt_manager_get_connected_dev_num();

	        if((role == BTSRV_TWS_MASTER) || (role == BTSRV_TWS_SLAVE)){
			    bt_manager_tws_send_message(TWS_BT_MGR_EVENT,TWS_EVENT_CLEAR_PAIRED_LIST,NULL,0);
				bt_manager->clear_paired_list_state = BT_CLEAR_PAIRED_LIST_TWS_SYNC_WAIT;
                bt_manager->clear_paired_list_sync_wait_reply = 0;
				bt_manager->clear_paired_list_timeout = 300;
	        }
			else{
				phone_num = bt_manager_get_connected_dev_num();
				if(phone_num == 0){
					bt_manager->clear_paired_list_state = BT_CLEAR_PAIRED_LIST_PHONE_CLEAR;
				}
				else{
				    bt_manager->clear_paired_list_state = BT_CLEAR_PAIRED_LIST_DISCONNECT_PHONE;
				}
			}

            if(cfg_pair->clear_tws_When_key_clear_paired_list){
                bt_manager->clear_paired_list_disc_tws = true;
            }
			else{
				bt_manager->clear_paired_list_disc_tws = false;
			}

			if(!bt_manager->clear_paired_list_work_valid){
			    os_delayed_work_submit(&bt_manager->clear_paired_list_work,1);
				bt_manager->clear_paired_list_work_valid = true;
			}
			bt_manager->clear_paired_list_state_update = true;
			break;

		case BT_CLEAR_PAIRED_LIST_TWS_SYNC_WAIT:
            if((bt_manager->clear_paired_list_sync_wait_reply == true)||
                (bt_manager->clear_paired_list_timeout == 0)){
                bt_manager->clear_paired_list_sync_wait_reply = 0;
                role = bt_manager_tws_get_dev_role();
                phone_num = bt_manager_get_connected_dev_num();

                if((role == BTSRV_TWS_SLAVE) || (phone_num == 0)){
                    bt_manager->clear_paired_list_timeout = 30;
                    bt_manager->clear_paired_list_state = BT_CLEAR_PAIRED_LIST_PHONE_CLEAR;
                }
                else{
                    bt_manager->clear_paired_list_state = BT_CLEAR_PAIRED_LIST_DISCONNECT_PHONE;
                }
				bt_manager->clear_paired_list_state_update = true;
            }
            else{
                if(bt_manager->clear_paired_list_timeout > 0){
                    bt_manager->clear_paired_list_timeout--;
                }
            }

            if(!bt_manager->clear_paired_list_work_valid){
                os_delayed_work_submit(&bt_manager->clear_paired_list_work,1);
                bt_manager->clear_paired_list_work_valid = true;
            }
            break;

		case BT_CLEAR_PAIRED_LIST_DISCONNECT_PHONE:
            btif_br_disconnect_device(BTSRV_DISCONNECT_PHONE_MODE);

            bt_manager->clear_paired_list_state = BT_CLEAR_PAIRED_LIST_PHONE_CLEAR;
            bt_manager->clear_paired_list_timeout = 30;
            if(!bt_manager->clear_paired_list_work_valid){
                os_delayed_work_submit(&bt_manager->clear_paired_list_work,1);
                bt_manager->clear_paired_list_work_valid = true;
            }
			bt_manager->clear_paired_list_state_update = true;
            break;

		case BT_CLEAR_PAIRED_LIST_PHONE_CLEAR:
            phone_num = bt_manager_get_connected_dev_num();
            role = bt_manager_tws_get_dev_role();

            if((phone_num != 0) && (bt_manager->clear_paired_list_timeout != 0)){
                bt_manager->clear_paired_list_timeout--;
                if(!bt_manager->clear_paired_list_work_valid){
                    os_delayed_work_submit(&bt_manager->clear_paired_list_work,100);
                    bt_manager->clear_paired_list_work_valid = true;
                }
                break;
            }

			if((phone_num != 0) && (role == BTSRV_TWS_SLAVE)){
				bt_manager->clear_paired_list_state = BT_CLEAR_PAIRED_LIST_NONE;
				break;
			}

            bt_manager->clear_paired_list_timeout = 0;

			btif_br_clear_list(BTSRV_DEVICE_PHONE);

            if(cfg_pair->clear_tws_When_key_clear_paired_list){
			    bt_manager->clear_paired_list_state = BT_CLEAR_PAIRED_LIST_TWS_CLEAR;
            }
			else{
				bt_manager->clear_paired_list_state = BT_CLEAR_PAIRED_LIST_COMPLETE;
			}

			if(!bt_manager->clear_paired_list_work_valid){
			    os_delayed_work_submit(&bt_manager->clear_paired_list_work,10);
				bt_manager->clear_paired_list_work_valid = true;
			}
			bt_manager->clear_paired_list_state_update = true;
			break;

	    case BT_CLEAR_PAIRED_LIST_TWS_CLEAR:
			if(bt_manager->clear_paired_list_disc_tws){
			    btif_br_clear_list(BTSRV_DEVICE_TWS);
			}
            bt_manager->clear_paired_list_state = BT_CLEAR_PAIRED_LIST_COMPLETE;

            bt_manager->clear_paired_list_timeout = 300;

			if(!bt_manager->clear_paired_list_work_valid){
			    os_delayed_work_submit(&bt_manager->clear_paired_list_work,10);
				bt_manager->clear_paired_list_work_valid = true;
			}
			bt_manager->clear_paired_list_state_update = true;
			break;

	    case BT_CLEAR_PAIRED_LIST_COMPLETE:
            role = bt_manager_tws_get_dev_role();
            if(cfg_pair->clear_tws_When_key_clear_paired_list){
                if((role != BTSRV_TWS_NONE ) && (bt_manager->clear_paired_list_timeout != 0)){
                    bt_manager->clear_paired_list_timeout--;
                    if(!bt_manager->clear_paired_list_work_valid){
                        os_delayed_work_submit(&bt_manager->clear_paired_list_work,100);
                        bt_manager->clear_paired_list_work_valid = true;
                    }
                    break;
                }
				else{
					if(role == BTSRV_TWS_NONE){
						if(!bt_manager->clear_paired_list_disc_tws){
			                btif_br_clear_list(BTSRV_DEVICE_TWS);
						}
					}
				}
                bt_manager_tws_clear_shared_addr();
            }

			bt_manager->clear_paired_list_state = BT_CLEAR_PAIRED_LIST_RESTORE_STATE;
			bt_manager->clear_paired_list_state_update = true;
			if(!bt_manager->clear_paired_list_work_valid){
			    os_delayed_work_submit(&bt_manager->clear_paired_list_work,20);
				bt_manager->clear_paired_list_work_valid = true;
			}
            break;

        case BT_CLEAR_PAIRED_LIST_RESTORE_STATE:
            role = bt_manager_tws_get_dev_role();
            bt_manager->clear_paired_list_state = BT_CLEAR_PAIRED_LIST_NONE;
			if(bt_manager->clear_paired_list_work_valid){
				os_delayed_work_cancel(&bt_manager->clear_paired_list_work);
				bt_manager->clear_paired_list_work_valid = 0;
			}

            if (cfg_pair->enter_pair_mode_when_key_clear_paired_list){
                if(role != BTSRV_TWS_SLAVE){
                    bt_manager_start_pair_mode();
                }
            }

            if (((bt_manager->pair_status & BT_PAIR_STATUS_PAIR_MODE) == 0) &&
                (role != BTSRV_TWS_SLAVE)){
                bt_manager_start_wait_connect();
            }

			bt_manager_sys_event_notify(SYS_EVENT_CLEAR_PAIRED_LIST);
			break;

        default:
			break;
    }

}

void bt_manager_clear_paired_list(void)
{
    struct bt_manager_context_t*  bt_manager = bt_manager_get_context();

	if(bt_manager->clear_paired_list_state != BT_CLEAR_PAIRED_LIST_NONE){
		SYS_LOG_WRN("%d", bt_manager->clear_paired_list_state);
		return;
	}
	else{
		bt_manager->clear_paired_list_state = BT_CLEAR_PAIRED_LIST_TWS_SYNC;
		bt_manager->clear_paired_list_state_update = true;
		bt_manager->clear_paired_list_timeout = 0;
		if(!bt_manager->clear_paired_list_work_valid){
		    os_delayed_work_cancel(&bt_manager->clear_paired_list_work);
			bt_manager->clear_paired_list_work_valid = false;
		    bt_manager_clear_paired_list_ex();
		}
	}
}

void bt_manager_end_wait_connect(void)
{
    struct bt_manager_context_t*  bt_manager = bt_manager_get_context();     

	btmgr_pair_cfg_t * pair_cfg = bt_manager_get_pair_config();

	btif_br_get_pair_status(&bt_manager->pair_status);

	SYS_LOG_INF("%x", bt_manager->pair_status);

    if (!bt_manager_is_wait_connect()){
        return;
    }

    bt_manager->pair_status &= (~BT_PAIR_STATUS_WAIT_CONNECT);

    if(pair_cfg->default_state_wait_connect_sec > 0){
	    os_delayed_work_cancel(&bt_manager->wait_connect_work);
		bt_manager->wait_connect_work_vaild = 0;
    }

	btif_br_set_wait_connect_status(0, 0);

    bt_manager_check_visual_mode();

    bt_manager_check_link_status();    
}

void bt_manager_start_wait_connect(void)
{
    struct bt_manager_context_t*  bt_manager = bt_manager_get_context();
	
	btmgr_pair_cfg_t * pair_cfg = bt_manager_get_pair_config();

    uint32_t timeout = pair_cfg->default_state_wait_connect_sec * 1000;
	
	btif_br_get_pair_status(&bt_manager->pair_status);

	SYS_LOG_INF("%x",bt_manager->pair_status);

    if((bt_manager_is_wait_connect()) && (timeout != 0)){
	    os_delayed_work_cancel(&bt_manager->wait_connect_work);
		bt_manager->wait_connect_work_vaild = 0;
    }

    bt_manager->pair_status |= BT_PAIR_STATUS_WAIT_CONNECT;

    if(timeout > 0){	
	    os_delayed_work_submit(&bt_manager->wait_connect_work,timeout);
		bt_manager->wait_connect_work_vaild = 1;
    }

    btif_br_set_wait_connect_status(1, timeout);
	
    bt_manager_check_visual_mode();

    bt_manager_check_link_status();
}

void bt_manager_end_pair_mode(void)
{
    struct bt_manager_context_t*  bt_manager = bt_manager_get_context();

	SYS_LOG_INF("%x",bt_manager_is_pair_mode());

	btif_br_get_pair_status(&bt_manager->pair_status);

    if (!bt_manager_is_pair_mode()){
        return;
    }

    bt_manager->pair_mode_state = BT_PAIR_MODE_STATE_NONE;

    if(bt_manager->pair_mode_work_valid == 1){
	    os_delayed_work_cancel(&bt_manager->pair_mode_work);
		bt_manager->pair_mode_work_valid = 0;
    }
    
    bt_manager->pair_status &= (~BT_PAIR_STATUS_PAIR_MODE);

	if(bt_manager_tws_get_dev_role() == BTSRV_TWS_MASTER){
        bt_manager_tws_send_message(TWS_BT_MGR_EVENT,
			TWS_EVENT_SYNC_PAIR_MODE,
			&bt_manager->pair_status,
			sizeof(uint8_t));
    }

    btif_br_set_pair_mode_status(0, 0);
    bt_manager_lea_policy_event_notify(LEA_POLICY_EVENT_EXIT_PAIRING, NULL, 0);

    bt_manager_check_visual_mode();

    bt_manager_check_link_status();
}


void bt_manager_start_pair_mode_ex(bool stop_reconnect_tws)
{
    struct bt_manager_context_t*  bt_manager = bt_manager_get_context();

    btmgr_pair_cfg_t *pair_cfg = bt_manager_get_pair_config();	

    uint32_t timeout = pair_cfg->pair_mode_duration_sec * 1000;

	SYS_LOG_INF("stop tws:%d timeout:%d",stop_reconnect_tws,timeout);

#if 0
    if(stop_reconnect_tws){
        btif_br_auto_reconnect_stop(BTSRV_STOP_AUTO_RECONNECT_ALL);
    }
	else{
		btif_br_auto_reconnect_stop(BTSRV_STOP_AUTO_RECONNECT_PHONE);
	}
#endif

    if(bt_manager->pair_mode_work_valid == 1){
	    os_delayed_work_cancel(&bt_manager->pair_mode_work);
    }

    bt_manager_end_wait_connect();

	btif_br_get_pair_status(&bt_manager->pair_status);

    bt_manager->pair_status |= BT_PAIR_STATUS_PAIR_MODE;

	bt_manager->pair_mode_state = BT_PAIR_MODE_RUNNING;
	
    if(timeout > 0){
	    os_delayed_work_submit(&bt_manager->pair_mode_work,timeout);
		bt_manager->pair_mode_work_valid = 1;
    }

    if(bt_manager_tws_get_dev_role() == BTSRV_TWS_MASTER){
        bt_manager_tws_send_message(TWS_BT_MGR_EVENT,
			TWS_EVENT_SYNC_PAIR_MODE,
			&bt_manager->pair_status,
			sizeof(uint8_t));
    }

    btif_br_set_pair_mode_status(1, timeout);
    bt_manager_lea_policy_event_notify(LEA_POLICY_EVENT_PAIRING, NULL, 0);

//    hostif_bt_le_force_rpa_timeout();  // for gfp 3.3 use id addr in pair mode, not need update .

    bt_manager_check_visual_mode();

    bt_manager_check_link_status();
}

void bt_manager_start_pair_mode(void)
{
    bt_manager_start_pair_mode_ex(true);
}

void bt_manager_enter_pair_mode_ex(void)
{
    struct bt_manager_context_t*  bt_manager = bt_manager_get_context();
	btmgr_pair_cfg_t *cfg_pair = bt_manager_get_pair_config();
    btmgr_feature_cfg_t *cfg_feature = bt_manager_get_feature_config();

	uint8_t phone_num = 0;
    uint8_t role = bt_manager_tws_get_dev_role();
    uint8_t disc_mode = 0;
    if(bt_manager->enter_pair_mode_state_update){
		bt_manager->enter_pair_mode_state_update = false;
	    SYS_LOG_INF("role:%d state:%s",role,bt_manager_enter_pair_mode_state2str(bt_manager->pair_mode_state));
    }

#if 0
    if(btif_br_is_auto_reconnect_runing() == true){
		if(bt_manager->stop_reconnect_tws_in_pair_mode){
			btif_br_auto_reconnect_stop(BTSRV_STOP_AUTO_RECONNECT_ALL);
			bt_manager->stop_reconnect_tws_in_pair_mode = 0;
		}
		else{
			btif_br_auto_reconnect_stop(BTSRV_STOP_AUTO_RECONNECT_PHONE);
		}
    }
#endif

    if(bt_manager_tws_get_dev_role() == BTSRV_TWS_SLAVE){
        switch(bt_manager->pair_mode_state){
            case BT_PAIR_MODE_TWS_SYNC:
			    bt_manager_tws_send_message(TWS_BT_MGR_EVENT,TWS_EVENT_ENTER_PAIR_MODE,NULL,0);
				bt_manager->pair_mode_state = BT_PAIR_MODE_TWS_SYNC_WAIT;
			    bt_manager->pair_mode_sync_wait_reply = 0;
				bt_manager->pair_mode_timeout = 300;
				bt_manager->enter_pair_mode_state_update = true;
				if(!bt_manager->pair_mode_work_valid){
					os_delayed_work_submit(&bt_manager->pair_mode_work,1);
			        bt_manager->pair_mode_work_valid = true;
				}
				break;
			case BT_PAIR_MODE_TWS_SYNC_WAIT:
				if((bt_manager->pair_mode_sync_wait_reply == true)||
					(bt_manager->pair_mode_timeout == 0)){
				    bt_manager->pair_mode_state = BT_PAIR_MODE_CHECK_PHONE;
					bt_manager->pair_mode_timeout = 30;
					bt_manager->pair_mode_sync_wait_reply = 0;
					bt_manager->enter_pair_mode_state_update = true;
				}
                else{
					if(bt_manager->pair_mode_timeout > 0){
						bt_manager->pair_mode_timeout--;
					}
                }
				if(!bt_manager->pair_mode_work_valid){
                    os_delayed_work_submit(&bt_manager->pair_mode_work,1);
                    bt_manager->pair_mode_work_valid = true;
			    }
				break;
		    case BT_PAIR_MODE_CHECK_PHONE:
			    phone_num = bt_manager_get_connected_dev_num();
                if(phone_num != 0){
					if(bt_manager->pair_mode_timeout != 0){
						bt_manager->pair_mode_timeout--;
						if(!bt_manager->pair_mode_work_valid){
						    os_delayed_work_submit(&bt_manager->pair_mode_work,100);
				            bt_manager->pair_mode_work_valid = true;
						}
						break;
					}
                }
				if(bt_manager->pair_mode_work_valid){
                    os_delayed_work_cancel(&bt_manager->pair_mode_work);
					bt_manager->pair_mode_work_valid = 0;
                }

				if(cfg_pair->clear_paired_list_when_enter_pair_mode){
                    btif_br_clear_list(BTSRV_DEVICE_PHONE);
                }
				bt_manager->pair_mode_state = BT_PAIR_MODE_STATE_NONE;
			    break;

			default:
				break;
        }
    }
    else
    {
        switch(bt_manager->pair_mode_state){
			case BT_PAIR_MODE_TWS_SYNC:
                if(bt_manager_tws_get_dev_role() == BTSRV_TWS_MASTER){
					bt_manager_tws_send_message(TWS_BT_MGR_EVENT,TWS_EVENT_ENTER_PAIR_MODE,NULL,0);
					bt_manager->pair_mode_state = BT_PAIR_MODE_TWS_SYNC_WAIT;
			        bt_manager->pair_mode_sync_wait_reply = 0;
				    bt_manager->pair_mode_timeout = 300;
                }
				else{
				    bt_manager->pair_mode_state = BT_PAIR_MODE_DISCONNECT_PHONE;
				}

				if(!bt_manager->pair_mode_work_valid){
                    os_delayed_work_submit(&bt_manager->pair_mode_work,1);
                    bt_manager->pair_mode_work_valid = true;
				}
				bt_manager->enter_pair_mode_state_update = true;
				break;
			case BT_PAIR_MODE_TWS_SYNC_WAIT:
				if((bt_manager->pair_mode_sync_wait_reply == true)||
					(bt_manager->pair_mode_timeout == 0)){
				    bt_manager->pair_mode_state = BT_PAIR_MODE_DISCONNECT_PHONE;
					bt_manager->pair_mode_sync_wait_reply = 0;
					bt_manager->enter_pair_mode_state_update = true;
				}
                else{
					if(bt_manager->pair_mode_timeout > 0){
						bt_manager->pair_mode_timeout--;
					}
                }
				if(!bt_manager->pair_mode_work_valid){
                    os_delayed_work_submit(&bt_manager->pair_mode_work,1);
                    bt_manager->pair_mode_work_valid = true;
			    }
				break;
            case BT_PAIR_MODE_DISCONNECT_PHONE:
				phone_num = bt_manager_audio_get_cur_dev_num();
                if ((cfg_pair->clear_paired_list_when_enter_pair_mode) ||
                    (cfg_pair->disconnect_all_phones_when_enter_pair_mode)){
					disc_mode = 1;
                }
				else if(cfg_feature->sp_device_num == 2){
                    if(bt_manager_tws_get_dev_role() == BTSRV_TWS_MASTER){
                        disc_mode = 2;
                        if (phone_num > 0){
                            disc_mode = 1;
                        }
                    }
                    else{
                        if(phone_num == 2){
                            disc_mode = 2;
                        }
                    }
				}
                else if(phone_num == 2){
                    if((bt_manager_tws_get_dev_role() != BTSRV_TWS_MASTER) && 
                        (cfg_feature->sp_phone_takeover == 1)) {
                        disc_mode = 0;
                    }else {
                        disc_mode = 2;
                    }
                }

                if ((cfg_feature->sp_dual_phone == 0) && 
                    (phone_num > 0 ) && (cfg_feature->sp_phone_takeover == 0))
                {
                    disc_mode = 1;						
                }

                if ((!bt_manager_audio_is_multi_point()) && 
                    (phone_num > 0 ) && (cfg_feature->sp_phone_takeover == 0)){
                    if(phone_num > 1){
                    	disc_mode = 2;
                    }
                    else{
                    	disc_mode = 1;
                    }
                }

				if(disc_mode){
					if(disc_mode == 1){
//						btif_br_disconnect_device(BTSRV_DISCONNECT_PHONE_MODE);
                        bt_manager_disconnect_other_audio_conn(NULL,true);
					}
					else{
						if (bt_manager_audio_is_lea_open()) {
							check_audio_conn_preemption_pre_dev(NULL, 0, 0);
						}else {
							bt_manager_disconnect_inactive_audio_conn();
						}
					}
					bt_manager->pair_mode_state = BT_PAIR_MODE_CHECK_PHONE;
				}
				else{
					bt_manager->pair_mode_state = BT_PAIR_MODE_CHECK_RECONNECT;
				}

				if(!bt_manager->pair_mode_work_valid){
					if(bt_manager->pair_mode_state == BT_PAIR_MODE_CHECK_PHONE){
                        os_delayed_work_submit(&bt_manager->pair_mode_work,100);
                        bt_manager->pair_mode_timeout = 30;
					}
                    else if(bt_manager->pair_mode_state == BT_PAIR_MODE_CHECK_RECONNECT){
                        os_delayed_work_submit(&bt_manager->pair_mode_work,100);
                        bt_manager->pair_mode_timeout = 50;
                    }
					else{
                        os_delayed_work_submit(&bt_manager->pair_mode_work,1);
                        bt_manager->pair_mode_timeout = 0;
					}
                    bt_manager->pair_mode_work_valid = true;
				}
				bt_manager->enter_pair_mode_state_update = true;
                break;
			case BT_PAIR_MODE_CHECK_PHONE:
			    phone_num = bt_manager_audio_get_cur_dev_num();

                if(cfg_pair->clear_paired_list_when_enter_pair_mode
                    || cfg_pair->disconnect_all_phones_when_enter_pair_mode){
                    if((phone_num != 0) && (bt_manager->pair_mode_timeout > 0)){
                        bt_manager->pair_mode_timeout--;
                        if(!bt_manager->pair_mode_work_valid){
                            os_delayed_work_submit(&bt_manager->pair_mode_work,100);
                            bt_manager->pair_mode_work_valid = true;
                        }
                        break;
                    }
                }
                if(phone_num >= bt_manager_config_connect_phone_num()){
                    if(bt_manager->pair_mode_timeout > 0){
                        bt_manager->pair_mode_timeout--;
                        if(!bt_manager->pair_mode_work_valid){
                            os_delayed_work_submit(&bt_manager->pair_mode_work,100);
                            bt_manager->pair_mode_work_valid = true;
                        }
                        break;
                    }
                }

				if (cfg_pair->clear_paired_list_when_enter_pair_mode){
                    btif_br_clear_list(BTSRV_DEVICE_PHONE);
                }

                bt_manager->pair_mode_state = BT_PAIR_MODE_CHECK_RECONNECT;
				bt_manager->enter_pair_mode_state_update = true;
				if(!bt_manager->pair_mode_work_valid){
                    os_delayed_work_submit(&bt_manager->pair_mode_work,100);
                    bt_manager->pair_mode_work_valid = true;
                    bt_manager->pair_mode_timeout = 50;
				}
                break;

            case BT_PAIR_MODE_CHECK_RECONNECT:
                if(bt_manager->auto_reconnect_startup == true){
                    if(btif_br_is_auto_reconnect_phone_first() == true){
                        if(bt_manager->pair_mode_timeout > 0){
                            bt_manager->pair_mode_timeout--;
                            if(!bt_manager->pair_mode_work_valid){
                                os_delayed_work_submit(&bt_manager->pair_mode_work,100);
                                bt_manager->pair_mode_work_valid = true;
                            }
                            break;
                        }
                    }
                }

                if(!bt_manager->auto_reconnect_timeout){
                    os_delayed_work_cancel(&bt_manager->auto_reconnect_work);
                    os_delayed_work_cancel(&bt_manager->auto_reconnect_second_dev_work);
                    btif_br_auto_reconnect_stop(BTSRV_STOP_AUTO_RECONNECT_PHONE);
                }

                bt_manager->pair_mode_state = BT_PAIR_MODE_START_PAIR;
                bt_manager->enter_pair_mode_state_update = true;

                if(!bt_manager->pair_mode_work_valid){
                    os_delayed_work_submit(&bt_manager->pair_mode_work,10);
                    bt_manager->pair_mode_work_valid = true;
                }
                break;

            case BT_PAIR_MODE_START_PAIR:
			    if(bt_manager->auto_reconnect_startup == true){
                    bt_manager_start_pair_mode_ex(false);
			    }
				else{
					bt_manager_start_pair_mode_ex(true);
				}
				bt_manager->bt_state = BT_STATUS_PAIR_MODE;

                if(bt_manager->personalized_name_update){
                    bt_manager->personalized_name_update = 0;
                    btif_br_update_devie_name();
                }
				break;

			default:
				break;
        }
    }
}

void bt_manager_enter_pair_mode_inner(void)
{
	struct bt_manager_context_t*  bt_manager = bt_manager_get_context();

	SYS_LOG_INF("state:%d tws pair:%d remote:%d",
		bt_manager->pair_mode_state,
		bt_manager_is_tws_pair_search(),
		bt_manager->remote_in_pair_mode);

    if( /*(bt_manager->pair_mode_state != BT_PAIR_MODE_STATE_NONE) ||*/
		(bt_manager_is_tws_pair_search() == true) ||
        (bt_manager->remote_in_pair_mode == true) ) {
        return;
    }

    if(bt_manager->pair_mode_work_valid == true){
		os_delayed_work_cancel(&bt_manager->pair_mode_work);
		bt_manager->pair_mode_work_valid = 0;
	}

	bt_manager->pair_mode_state = BT_PAIR_MODE_TWS_SYNC;
	bt_manager->enter_pair_mode_state_update = true;

	bt_manager->stop_reconnect_tws_in_pair_mode = 1;
    bt_manager_enter_pair_mode_ex();
    bt_manager_audio_set_takeover_flag(0);
}

void bt_manager_enter_pair_mode(void)
{
#ifdef CONFIG_BT_ENTER_PAIR_MODE_FIRST_RECONNECT
    int ret;
    ret = bt_manager_reconnect_one_phone();
    SYS_LOG_INF("reconnect: %d\r\n", ret);
    if (ret > 0) {
        return;
    }
#endif

#if 0
    bt_manager_lea_policy_event_notify(LEA_POLICY_EVENT_PAIRING, NULL, 0);
#endif

    bt_manager_enter_pair_mode_inner();
}

void bt_manager_check_phone_connected(void)
{
    struct bt_manager_context_t*  bt_manager = bt_manager_get_context();
	btmgr_pair_cfg_t *cfg_pair = bt_manager_get_pair_config();
    btmgr_feature_cfg_t *cfg_feature = bt_manager_get_feature_config();

    if ((bt_manager_audio_get_cur_dev_num() >= bt_manager_config_connect_phone_num())
		|| (cfg_pair->bt_not_discoverable_when_connected)
		|| (cfg_feature->sp_dual_phone == 0)
        || ((bt_manager->connected_phone_num >= (cfg_feature->sp_device_num -1))
            && (bt_manager_tws_get_dev_role() != BTSRV_TWS_NONE))){
        bt_manager_end_pair_mode();
        bt_manager_end_wait_connect();
    }
    else{
		if(bt_manager_is_pair_mode() == true){
			if(bt_manager->pair_mode_work_valid){
			    os_delayed_work_cancel(&bt_manager->pair_mode_work);
			}
			os_delayed_work_submit(&bt_manager->pair_mode_work,cfg_pair->pair_mode_duration_sec * 1000);
		}

		if(bt_manager_is_wait_connect() == true){
			if(bt_manager->wait_connect_work_vaild){
			    os_delayed_work_cancel(&bt_manager->wait_connect_work);
			}
			os_delayed_work_submit(&bt_manager->wait_connect_work,cfg_pair->default_state_wait_connect_sec * 1000);
		}
    }
}

void bt_manager_powon_auto_reconnect(uint8_t reboot_mode)
{
    uint16_t delay_time = 500;
 	struct bt_manager_context_t*  bt_manager = bt_manager_get_context();

    if (bt_manager_is_pair_mode() || bt_manager_is_tws_pair_search()){
        return;
    }

    if (reboot_mode & (BT_MANAGER_REBOOT_CLEAR_TWS_INFO | BT_MANAGER_REBOOT_ENTER_PAIR_MODE)){
        return;
    }
    
    if (bt_manager_tws_powon_auto_pair()){
        return;
    }

#ifdef CONFIG_BT_LETWS
    if (bt_manager_letws_get_dev_role() == BTSRV_TWS_SLAVE)
    {
       SYS_LOG_INF("tws slave not conn phone:\n");
       return; 
    }
#endif

#ifdef CONFIG_BT_LETWS_ROLE_SWITCH
    if (bt_manager_exist_letws_info()){
       delay_time = 2500;
    }
#endif



    os_delayed_work_submit(&bt_manager->auto_reconnect_work,delay_time);
    SYS_LOG_INF("powon_auto_reconnect:%d \n",delay_time);
}

