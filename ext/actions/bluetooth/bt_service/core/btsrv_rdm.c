/*
 * Copyright (c) 2016 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt device manager interface
 */
#define SYS_LOG_DOMAIN "btsrv_rdm"
#include "btsrv_os_common.h"
#include "btsrv_inner.h"

enum {
	BTSERV_DEV_DEACTIVE,
	BTSERV_DEV_ACTIVED,
	BTSERV_DEV_ACTIVE_PENDING,
};

typedef enum {
	BTSRV_CONNECT_ACL,
	BTSRV_CONNECT_A2DP,
	BTSRV_CONNECT_AVRCP,
	BTSRV_CONNECT_HFP,
	BTSRV_CONNECT_SPP,
	BTSRV_CONNECT_PBAP,
	BTSRV_CONNECT_HID,
	BTSRV_CONNECT_MAP,
} btsrv_rdm_connect_type;

struct rdm_device {
	sys_snode_t node;

	bd_address_t bt_addr;
	struct bt_conn *base_conn;
	struct bt_conn *sco_conn;
	uint16_t acl_hdl;
	uint16_t sco_hdl;
	uint32_t sco_creat_time;

	uint8_t sco_wait_disconnect:1;
	uint8_t tws:3;
	/* Snoop tws add */
	uint8_t snoop_role:3;
	uint8_t snoop_mode:1;
	uint8_t snoop_switch_lock:1;

	uint8_t connected:1;
	uint8_t wait_to_disconnect:1;
	uint8_t security_changed:1;
	uint8_t controler_role:1;

	uint8_t a2dp_connected:1;
	uint8_t a2dp_signal_connected:1;
	uint8_t a2dp_pending_ahead_start:1;
	uint8_t switch_sbc_state:4;
	uint8_t a2dp_active:1;
	uint8_t a2dp_priority;
	uint32_t a2dp_start_time;
	uint32_t a2dp_stop_time;
	uint8_t format;
	uint8_t sample_rate;
	uint8_t cp_type;
	/* Snoop tws add */
	uint16_t a2dp_media_rx_cid;

	uint32_t avrcp_sync_volume_start_time;		/* Just for avrcp sync volume */
	uint8_t avrcp_connected:1;
	uint8_t avrcp_sync_volume_wait:1;
	uint8_t avrcp_playstatus:1; /*0:pause, 1:play*/
    uint8_t avrcp_connecting_pending:1;
    uint8_t avrcp_playing_pending:1;
    uint8_t avrcp_playing_pended:1;
    uint8_t avrcp_connecting_pended:1;

	uint8_t absolute_volume_filtr_cnt;
	uint32_t avrcp_set_absolute_volume;
	os_delayed_work avrcp_set_absolute_volume_delay_work;
	os_delayed_work avrcp_status_pending_delay_work;

	struct{
		uint32_t song_len;
		uint32_t song_pos;
		uint8_t play_state;
	};

	uint8_t hfp_connected:1;
	uint8_t hfp_role:1;
	uint8_t hfp_notify_phone_num:1;
	uint8_t hfp_active_state:3;
	uint8_t hfp_state:6;
	uint8_t siri_state:3;
	uint8_t sco_state:2;
	uint8_t active_call:1;		/* Just for debug */
	uint8_t held_call:1;
	uint8_t incoming_call:1;
	uint8_t outgoing_call:1;
	uint8_t hfp_format;
	uint8_t hfp_sample_rate;

	uint8_t spp_connected:3;

	uint8_t hid_connected:1;
	uint8_t hid_plug:1;

	uint8_t pbap_connected:3;
	uint8_t map_connected:3;
    uint8_t tws_connected:1;
    uint8_t ios_device:1;
    uint8_t pc_device:1;

    int8_t rssi;
    uint8_t quality;
	uint16_t link_time;
    uint16_t deactive_cnt;
	uint16_t vendor_id;
	uint16_t product_id;

	uint8_t *device_name;

	struct btsrv_rdm_avrcp_pass_info avrcp_pass_info;
	/* Sniff manager info */
	struct rdm_sniff_info sniff;

    uint8_t dev_class[3];

	uint32_t profile_disconnect_timestamp;
};

struct btsrv_rdm_avrcp_cb {
	struct bt_conn *conn;
	union {
		uint8_t event_id;
		uint8_t op_id;
	};
	uint8_t status;
};

#define __RMT_DEV(_node) CONTAINER_OF(_node, struct rdm_device, node)

#define ABSOLUTE_VOLUME_FILTR_MAX_CNT	3
#define ABSOLUTE_VLOUME_INVALID         0xFF


struct btsrv_rdm_priv {
	/* TODO, protect me, no need, ensure all opration in btsrv thread */
	sys_slist_t dev_list;	/* connected device list */
	uint8_t role_type:3;		/* Current device role: tws master, tws slave, tws none */
};

static struct btsrv_rdm_priv *p_rdm;
static struct btsrv_rdm_priv btsrv_rdm;

int btsrv_rdm_hid_actived(struct bt_conn *base_conn, uint8_t actived);

static void btsrv_rdm_dev_free(struct rdm_device *dev)
{
	if (!dev) {
		SYS_LOG_ERR("param error");
		return;
	}

	if(dev->avrcp_connecting_pending || dev->avrcp_playing_pending){
		//TODO:use os_xx instead
		k_delayed_work_cancel_sync(&dev->avrcp_status_pending_delay_work,K_FOREVER);
    }
	k_delayed_work_cancel_sync(&dev->avrcp_set_absolute_volume_delay_work,K_FOREVER);
	if(dev->device_name){
		mem_free(dev->device_name);
	}
	mem_free(dev);
}

static struct rdm_device *btsrv_rdm_find_dev_by_addr(bd_address_t *addr)
{
	struct rdm_device *dev;
	sys_snode_t *node;

	SYS_SLIST_FOR_EACH_NODE(&p_rdm->dev_list, node) {
		dev = __RMT_DEV(node);
		if (dev->connected == 1 &&
			memcmp(addr, dev->bt_addr.val, sizeof(bt_addr_t)) == 0)
			return dev;
	}

	return NULL;
}

static struct rdm_device *btsrv_rdm_find_dev_by_conn(struct bt_conn *base_conn)
{
	struct rdm_device *dev;
	sys_snode_t *node;

	SYS_SLIST_FOR_EACH_NODE(&p_rdm->dev_list, node) {
		dev = __RMT_DEV(node);
		if (dev->base_conn == base_conn)
			return dev;
	}

	return NULL;
}

static struct rdm_device *_btsrv_rdm_find_dev_by_sco_conn(struct bt_conn *sco_conn)
{
	struct rdm_device *dev;
	sys_snode_t *node;

	SYS_SLIST_FOR_EACH_NODE(&p_rdm->dev_list, node) {
		dev = __RMT_DEV(node);
		if (dev->sco_conn == sco_conn)
			return dev;
	}

	return NULL;
}

static struct rdm_device *btsrv_rdm_find_dev_by_connect_type(struct bt_conn *base_conn, int type)
{
	struct rdm_device *dev;
	sys_snode_t *node;
	bool paired = false;

	SYS_SLIST_FOR_EACH_NODE(&p_rdm->dev_list, node) {
		dev = __RMT_DEV(node);

		if (type == BTSRV_CONNECT_ACL) {
			paired = (dev->connected == 1);
		} else if (type == BTSRV_CONNECT_A2DP) {
			paired = (dev->a2dp_connected == 1);
		} else if (type == BTSRV_CONNECT_AVRCP) {
			paired = (dev->avrcp_connected == 1);
		} else if (type == BTSRV_CONNECT_HFP) {
			paired = (dev->hfp_connected == 1);
		} else if (type == BTSRV_CONNECT_SPP) {
			paired = (dev->spp_connected != 0);
		} else if (type == BTSRV_CONNECT_PBAP) {
			paired = (dev->pbap_connected != 0);
		} else if (type == BTSRV_CONNECT_HID) {
			paired = (dev->hid_connected != 0);
		} else if (type == BTSRV_CONNECT_MAP) {
			paired = (dev->map_connected != 0);
		}

		if (paired && dev->base_conn == base_conn)
			return dev;
	}

	return NULL;
}

static struct rdm_device *btsrv_rdm_find_dev_by_connect_type_tws(struct bt_conn *base_conn, int type, uint8_t role)
{
	struct rdm_device *dev;
	sys_snode_t *node;
	bool paired = false;

	SYS_SLIST_FOR_EACH_NODE(&p_rdm->dev_list, node) {
		dev = __RMT_DEV(node);

		if (type == BTSRV_CONNECT_ACL) {
			paired = (dev->connected == 1);
		} else if (type == BTSRV_CONNECT_A2DP) {
			paired = (dev->a2dp_connected == 1);
		} else if (type == BTSRV_CONNECT_AVRCP) {
			paired = (dev->avrcp_connected == 1);
		} else if (type == BTSRV_CONNECT_HFP) {
			paired = (dev->hfp_connected == 1);
		}else if (type == BTSRV_CONNECT_HID) {
			paired = (dev->hid_connected == 1);
		}

		if (paired && dev->base_conn == base_conn && dev->tws == role)
			return dev;
	}

	return NULL;
}

/* Only for BTSRV_TWS_NONE */
static struct rdm_device *btsrv_rdm_find_second_dev_by_connect_type(struct bt_conn *base_conn, int type)
{
	struct rdm_device *dev;
	sys_snode_t *node;
	bool paired = false;

	SYS_SLIST_FOR_EACH_NODE(&p_rdm->dev_list, node) {
		dev = __RMT_DEV(node);

		if (type == BTSRV_CONNECT_ACL) {
			paired = (dev->connected == 1);
		} else if (type == BTSRV_CONNECT_A2DP) {
			paired = (dev->a2dp_connected == 1);
		} else if (type == BTSRV_CONNECT_AVRCP) {
			paired = (dev->avrcp_connected == 1);
		} else if (type == BTSRV_CONNECT_HFP) {
			paired = (dev->hfp_connected == 1);
		}else if (type == BTSRV_CONNECT_HID) {
			paired = (dev->hid_connected == 1);
		}

		if (paired && dev->base_conn != base_conn && dev->tws == BTSRV_TWS_NONE)
			return dev;
	}

	return NULL;
}

static struct rdm_device *btsrv_rdm_a2dp_get_actived_device(void)
{
	struct rdm_device *dev;
	struct rdm_device *a2dp_active_dev = NULL, *high_prio_dev = NULL, *a2dp_connected_dev = NULL;
	sys_snode_t *node;
	uint8_t prio = 0;

	SYS_SLIST_FOR_EACH_NODE(&p_rdm->dev_list, node) {
		dev = __RMT_DEV(node);
		if (dev->tws != BTSRV_TWS_NONE || !dev->a2dp_connected) {
			continue;
		}

		if (!a2dp_active_dev && dev->a2dp_active) {
			a2dp_active_dev = dev;
		}

		if (dev->a2dp_priority > prio) {
			high_prio_dev = dev;
			prio = dev->a2dp_priority;
		}

		//if (!a2dp_connected_dev)
		{
			a2dp_connected_dev = dev;
		}
	}

	if (a2dp_active_dev) {
		return a2dp_active_dev;
	} else if (high_prio_dev) {
		return high_prio_dev;
	} else if (a2dp_connected_dev) {
		return a2dp_connected_dev;
	} else {
		return NULL;
	}
}

static struct rdm_device *btsrv_rdm_hfp_get_actived_device(void)
{
	struct rdm_device *dev;
	struct rdm_device *connected_actived_dev = NULL, *connected_pending_dev = NULL;
	struct rdm_device *actived_dev = NULL, *pending_dev = NULL;
	struct rdm_device *hfp_connected_dev = NULL, *phone_dev = NULL;
	sys_snode_t *node;

	if(NULL == p_rdm) {
		return NULL;
	}

	SYS_SLIST_FOR_EACH_NODE(&p_rdm->dev_list, node) {
		dev = __RMT_DEV(node);
		if (dev->tws != BTSRV_TWS_NONE) {
			continue;
		}

		if (dev->hfp_active_state == BTSERV_DEV_ACTIVED) {
			if (dev->hfp_connected) {
				if (!connected_actived_dev) {
					connected_actived_dev = dev;
				}
			} else {
				if (!actived_dev) {
					actived_dev = dev;
				}
			}
		} else if (dev->hfp_active_state == BTSERV_DEV_ACTIVE_PENDING) {
			if (dev->hfp_connected) {
				if (!connected_pending_dev) {
					connected_pending_dev = dev;
				}
			} else {
				if (!pending_dev) {
					pending_dev = dev;
				}
			}
		}

		if (!hfp_connected_dev && dev->hfp_connected) {
			hfp_connected_dev = dev;
		}

		if (!phone_dev) {
			phone_dev = dev;
		}
	}

	if (connected_actived_dev) {
		return connected_actived_dev;
	} else if (connected_pending_dev) {
		return connected_pending_dev;
	} else if (actived_dev) {
		return actived_dev;
	} else if (pending_dev) {
		return pending_dev;
	} else if (hfp_connected_dev) {
		return hfp_connected_dev;
	} else if (phone_dev) {
		return phone_dev;
	} else {
		return NULL;
	}
}

static struct rdm_device *btsrv_rdm_avrcp_get_actived_device(void)
{
	struct rdm_device *dev;
	struct rdm_device *avrcp_active_dev = NULL, *high_prio_dev = NULL, *avrcp_connected_dev = NULL;
	sys_snode_t *node;
	uint8_t prio = 0;

	SYS_SLIST_FOR_EACH_NODE(&p_rdm->dev_list, node) {
		dev = __RMT_DEV(node);
		if (dev->tws != BTSRV_TWS_NONE || !dev->avrcp_connected) {
			continue;
		}

		if (!avrcp_active_dev && dev->avrcp_connected) {
			avrcp_active_dev = dev;
		}

		if (dev->a2dp_priority > prio) {
			high_prio_dev = dev;
			prio = dev->a2dp_priority;
		}

		if (!avrcp_connected_dev) {
			avrcp_connected_dev = dev;
		}
	}

	if (avrcp_active_dev) {
		return avrcp_active_dev;
	} else if (high_prio_dev) {
		return high_prio_dev;
	} else if (avrcp_connected_dev) {
		return avrcp_connected_dev;
	} else {
		return NULL;
	}
}

bool btsrv_rdm_need_high_performance(void)
{
	bool high_performance = false;
	struct rdm_device *dev;
	sys_snode_t *node;

	SYS_SLIST_FOR_EACH_NODE(&p_rdm->dev_list, node) {
		dev = __RMT_DEV(node);
		if (dev->connected) {

			if (dev->tws != BTSRV_TWS_NONE){
				continue;
			}
			
			if (!dev->hfp_connected && !dev->a2dp_connected &&
				!dev->avrcp_connected && !dev->spp_connected &&
				!dev->pbap_connected && !dev->hid_connected &&
				!dev->map_connected ) {
				high_performance = true;
			}
		}
	}

	return high_performance;
}

struct bt_conn *btsrv_rdm_find_conn_by_addr(bd_address_t *addr)
{
	struct rdm_device *dev = btsrv_rdm_find_dev_by_addr(addr);

	if (dev)
		return dev->base_conn;

	return NULL;
}

struct bt_conn *btsrv_rdm_find_conn_by_hdl(uint16_t hdl)
{
	struct rdm_device *dev;
	sys_snode_t *node;

	SYS_SLIST_FOR_EACH_NODE(&p_rdm->dev_list, node) {
		dev = __RMT_DEV(node);
		if (dev->connected == 1 && dev->acl_hdl == hdl)
			return dev->base_conn;
	}

	return NULL;
}

struct bt_conn *btsrv_rdm_sync_find_conn_by_addr(bd_address_t *addr)
{
	struct rdm_device *dev;
	sys_snode_t *node;

	SYS_SLIST_FOR_EACH_NODE(&p_rdm->dev_list, node) {
		dev = __RMT_DEV(node);
		if (memcmp(addr, dev->bt_addr.val, sizeof(bt_addr_t)) == 0)
			return dev->base_conn;
	}

	return NULL;
}

void *btsrv_rdm_get_addr_by_conn(struct bt_conn *base_conn)
{
	struct rdm_device *dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		return NULL;
	}

	return dev->bt_addr.val;
}

int btsrv_rdm_get_connected_dev(rdm_connected_dev_cb cb, void *cb_param)
{
	struct rdm_device *dev;
	sys_snode_t *node;
	int count = 0;

	SYS_SLIST_FOR_EACH_NODE(&p_rdm->dev_list, node) {
		dev = __RMT_DEV(node);
		if (dev->connected) {
			count++;
			if (cb) {
				cb(dev->base_conn, dev->tws, cb_param);
			}
		}
	}

	return count;
}

int btsrv_rdm_get_connected_dev_cnt_by_type(int type)
{
	struct rdm_device *dev;
	sys_snode_t *node;
	int count = 0;

	SYS_SLIST_FOR_EACH_NODE(&p_rdm->dev_list, node) {
		dev = __RMT_DEV(node);
		if (dev->connected) {
			if (BTSRV_DEVICE_ALL == type
				|| (BTSRV_DEVICE_PHONE == type && dev->tws == BTSRV_TWS_NONE)
				|| (BTSRV_DEVICE_TWS == type && dev->tws != BTSRV_TWS_NONE)) {
				count++;
			}
		}
	}

	return count;
}
int btsrv_rdm_get_dev_state(struct bt_dev_rdm_state *state)
{
	int ret = 0;
	struct rdm_device *dev = btsrv_rdm_find_dev_by_addr(&state->addr);

	if (dev == NULL) {
		state->acl_connected = 0;
		state->hfp_connected = 0;
		state->a2dp_connected = 0;
		state->avrcp_connected = 0;
		state->hid_connected = 0;
		state->a2dp_stream_started = 0;
		state->sco_connected = 0;
		ret = -1;
	} else {
		state->acl_connected = dev->connected;
		state->hfp_connected = dev->hfp_connected;
		state->a2dp_connected = dev->a2dp_connected;
		state->avrcp_connected = dev->avrcp_connected;
		state->hid_connected = dev->hid_connected;
		state->a2dp_stream_started = (dev->a2dp_priority & A2DP_PRIOROTY_STREAM_OPEN) ? 1 : 0;
		state->sco_connected = (dev->sco_conn) ? 1 : 0;
	}

	return ret;
}

int btsrv_rdm_get_autoconn_dev(struct autoconn_info *info, int max_dev)
{
	struct rdm_device *dev;
	sys_snode_t *node;
	int cnt = 0;

	SYS_SLIST_FOR_EACH_NODE(&p_rdm->dev_list, node) {
		dev = __RMT_DEV(node);
		if (dev->connected && (dev->a2dp_connected |
			dev->avrcp_connected | dev->hfp_connected | dev->hid_connected | dev->tws_connected)) {
			memcpy(&info[cnt].addr.val, dev->bt_addr.val, sizeof(bd_address_t));
			info[cnt].addr_valid = 1;
			info[cnt].tws_role = dev->tws;
			info[cnt].a2dp = dev->a2dp_connected;
			info[cnt].avrcp = dev->avrcp_connected;
			info[cnt].hfp = dev->hfp_connected;
			info[cnt].hid = dev->hid_connected;
			info[cnt].ios = dev->ios_device;
			info[cnt].pc = dev->pc_device;

			if (dev->a2dp_connected && (!dev->hfp_connected)) {
				info[cnt].hfp_first = 0;
			} else {
				info[cnt].hfp_first = 1;
			}
			cnt++;
			if (cnt == max_dev) {
				return cnt;
			}
		}
	}

	return cnt;
}

int btsrv_rdm_base_disconnected(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_connect_type(base_conn, BTSRV_CONNECT_ACL);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected or already disconnected\n");
		return -ENODEV;
	}

	dev->connected = 0;
	return 0;
}

int btsrv_rdm_add_dev(struct bt_conn *base_conn)
{
	char addr_str[BT_ADDR_STR_LEN];
	struct rdm_device *dev;
	int i;
	struct autoconn_info *tmpInfo;
	
	bd_address_t *addr = (bd_address_t *)GET_CONN_BT_ADDR(base_conn);

	dev = btsrv_rdm_find_dev_by_connect_type(base_conn, BTSRV_CONNECT_ACL);
	if (dev != NULL) {
		SYS_LOG_WRN("already connected??\n");
		return -EEXIST;
	}

	dev = mem_malloc(sizeof(struct rdm_device));
	if (dev == NULL) {
		SYS_LOG_ERR("malloc failed!!\n");
		return -ENODEV;
	}

	memset(dev, 0, sizeof(struct rdm_device));
	memcpy(dev->bt_addr.val, addr, sizeof(bt_addr_t));
	dev->connected = 1;
	dev->base_conn = hostif_bt_conn_ref(base_conn);
	dev->acl_hdl = hostif_bt_conn_get_handle(base_conn);
	dev->song_len = GETPLAYSTATUS_INVALID_VALUE;
	dev->song_pos = GETPLAYSTATUS_INVALID_VALUE;
	sys_slist_append(&p_rdm->dev_list, &dev->node);
	dev->hfp_format = BT_CODEC_ID_CVSD;
	dev->hfp_sample_rate = 8;

	tmpInfo = mem_malloc(sizeof(struct autoconn_info)*BTSRV_SAVE_AUTOCONN_NUM);
	if (!tmpInfo) {
		SYS_LOG_ERR("malloc failed");
		return -ENOMEM;
	}
	btsrv_connect_get_auto_reconnect_info(tmpInfo,BTSRV_SAVE_AUTOCONN_NUM);
	for (i = 0; i < BTSRV_SAVE_AUTOCONN_NUM; i++) {
		if (tmpInfo[i].addr_valid) {
			if (!memcmp(tmpInfo[i].addr.val, addr->val, sizeof(bd_address_t))) {
				if(tmpInfo[i].hid){
					btsrv_rdm_hid_actived(base_conn,1);
				}
				break;
			}
		}
	}
	if (tmpInfo) {
		mem_free(tmpInfo);
	}

	hostif_bt_addr_to_str((const bt_addr_t *)addr, addr_str, BT_ADDR_STR_LEN);
	SYS_LOG_INF("add_dev %s 0x%x", addr_str, hostif_bt_conn_get_handle(dev->base_conn));
	return 0;
}

int btsrv_rdm_remove_dev(uint8_t *mac)
{
	char addr_str[BT_ADDR_STR_LEN];
	struct rdm_device *dev;
	sys_snode_t *node;
	uint8_t find = 0;

	SYS_SLIST_FOR_EACH_NODE(&p_rdm->dev_list, node) {
		dev = __RMT_DEV(node);
		if (memcmp(mac, dev->bt_addr.val, sizeof(bt_addr_t)) == 0) {
			find = 1;
			break;
		}
	}

	if (!find) {
		SYS_LOG_WRN("not connected??\n");
		return -EALREADY;
	}

	if (dev->connected || dev->hfp_connected || dev->a2dp_connected || dev->hid_connected ||
		dev->avrcp_connected || dev->spp_connected || dev->pbap_connected) {
		SYS_LOG_WRN("Still connected %d %d %d %d %d %d %d\n", dev->connected,
					dev->hfp_connected, dev->a2dp_connected, dev->hid_connected,
					dev->avrcp_connected, dev->spp_connected, dev->pbap_connected);
		return -EBUSY;
	}

	/* Tws disconnected, set device rolt to BTSRV_TWS_NONE */
	if (dev->tws != BTSRV_TWS_NONE) {
		p_rdm->role_type = BTSRV_TWS_NONE;
	}

	if (dev->sco_conn) {
		btsrv_sco_acl_disconnect_clear_sco_conn(dev->sco_conn);
		hostif_bt_conn_unref(dev->sco_conn);
		dev->sco_conn = NULL;
		SYS_LOG_WRN("Remove dev before sco_conn unref");
	}

	hostif_bt_conn_unref(dev->base_conn);
	sys_slist_find_and_remove(&p_rdm->dev_list, &dev->node);

	btsrv_rdm_dev_free(dev);

	hostif_bt_addr_to_str((const bt_addr_t *)mac, addr_str, BT_ADDR_STR_LEN);
	SYS_LOG_INF("remove_dev %s\n", addr_str);
	return 0;
}

void btsrv_rdm_set_security_changed(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return;
	}

	dev->security_changed = 1;
}

bool btsrv_rdm_is_security_changed(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return false;
	}

	return (dev->security_changed) ? true : false;
}

bool btsrv_rdm_is_connected(struct bt_conn *base_conn)
{
	return (btsrv_rdm_find_dev_by_connect_type(base_conn, BTSRV_CONNECT_ACL) != NULL);
}

bool btsrv_rdm_is_a2dp_connected(struct bt_conn *base_conn)
{
	return (btsrv_rdm_find_dev_by_connect_type(base_conn, BTSRV_CONNECT_A2DP) != NULL);
}

bool btsrv_rdm_is_avrcp_connected(struct bt_conn *base_conn)
{
	return (btsrv_rdm_find_dev_by_connect_type(base_conn, BTSRV_CONNECT_AVRCP) != NULL);
}

bool btsrv_rdm_is_hfp_connected(struct bt_conn *base_conn)
{
	return (btsrv_rdm_find_dev_by_connect_type(base_conn, BTSRV_CONNECT_HFP) != NULL);
}

bool btsrv_rdm_is_spp_connected(struct bt_conn *base_conn)
{
	return (btsrv_rdm_find_dev_by_connect_type(base_conn, BTSRV_CONNECT_SPP) != NULL);
}

bool btsrv_rdm_is_hid_connected(struct bt_conn *base_conn)
{
	return (btsrv_rdm_find_dev_by_connect_type(base_conn, BTSRV_CONNECT_HID) != NULL);
}

bool btsrv_rdm_is_ios_pc(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return false;
	}

    SYS_LOG_INF("ios %d cod %d",dev->ios_device,dev->pc_device);

    if(dev->ios_device && dev->pc_device){
        return true;
    }
    else{
	    return false;
	}
}

bool btsrv_rdm_is_ios_dev(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return false;
	}

    SYS_LOG_DBG("ios %d",dev->ios_device);

    if(dev->ios_device){
        return true;
    }
    else{
	    return false;
	}
}

bool btsrv_rdm_is_profile_connected(struct bt_conn *base_conn)
{
    struct rdm_device *dev;
    dev = btsrv_rdm_find_dev_by_conn(base_conn);
    if (dev == NULL) {
        SYS_LOG_WRN("not connected??\n");
        return false;
    }

    if(btsrv_rdm_is_a2dp_connected(base_conn) == true){
        return true;
    }
    else if(btsrv_rdm_is_hfp_connected(base_conn) == true){
        return true;
    }
    else if(btsrv_rdm_is_avrcp_connected(base_conn) == true){
        return true;
    }
    else if(btsrv_rdm_is_spp_connected(base_conn) == true){
        return true;
    }
    else if(btsrv_rdm_is_hid_connected(base_conn) == true){
        return true;
    }
    else{
        return false;
    }
}

bool btsrv_rdm_is_a2dp_signal_connected(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("dev not add\n");
		return -ENODEV;
	}

    if(dev->a2dp_signal_connected == 1){
        return true;
    }
    else{
        return false;
    }
}

int btsrv_rdm_set_a2dp_signal_connected(struct bt_conn *base_conn, bool connected)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("dev not add\n");
		return -ENODEV;
	}

	if (connected) {
		dev->a2dp_signal_connected = 1;
	} else {
		dev->a2dp_signal_connected = 0;
	}
	return 0;
}

int btsrv_rdm_set_a2dp_connected(struct bt_conn *base_conn, bool connected)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("dev not add\n");
		return -ENODEV;
	}

	if ((connected && dev->a2dp_connected) ||
		((!connected) && (!dev->a2dp_connected))) {
		SYS_LOG_WRN("a2dp already %s\n", connected ? "connected" : "disconnected");
		return -EALREADY;
	}

	if (connected) {
		dev->a2dp_connected = 1;
	} else {
		dev->a2dp_connected = 0;
	}
	return 0;
}


void btsrv_rdm_a2dp_set_clear_priority(struct bt_conn *base_conn, uint8_t prio, uint8_t set)
{
	struct rdm_device *dev;
	uint32_t curr_time;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (!dev) {
		SYS_LOG_WRN("Not dev");
		return;
	}

	if (set) {
		dev->a2dp_priority |= prio;
		if (prio & A2DP_PRIOROTY_STREAM_OPEN) {
			curr_time = os_uptime_get_32();
			dev->a2dp_start_time = curr_time;
			dev->a2dp_stop_time = 0;
			dev->avrcp_sync_volume_start_time = curr_time;
		}
	} else {
		dev->a2dp_priority &= (~prio);
		if (prio & A2DP_PRIOROTY_STREAM_OPEN) {
			curr_time = os_uptime_get_32();
			dev->a2dp_start_time = 0;
			dev->a2dp_stop_time = curr_time;
			dev->avrcp_sync_volume_start_time = 0;
		}
	}
}

uint8_t btsrv_rdm_a2dp_get_priority(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (!dev) {
		SYS_LOG_WRN("Not dev");
		return 0;
	}

	return dev->a2dp_priority;
}

void btsrv_rdm_a2dp_set_start_stop_time(struct bt_conn *base_conn, uint32_t time, uint8_t start)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (!dev) {
		SYS_LOG_WRN("Not dev");
		return;
	}

	if (start) {
		dev->a2dp_start_time = time;
		dev->a2dp_stop_time = 0;
	} else {
		dev->a2dp_start_time = 0;
		dev->a2dp_stop_time = time;
	}
}

uint32_t btsrv_rdm_a2dp_get_start_stop_time(struct bt_conn *base_conn, uint8_t start)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (!dev) {
		SYS_LOG_WRN("Not dev");
		return 0;
	}

	if (start) {
		return dev->a2dp_start_time;
	} else {
		return dev->a2dp_stop_time;
	}
}

void btsrv_rdm_a2dp_set_active(struct bt_conn *base_conn, uint8_t set)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (!dev) {
		SYS_LOG_WRN("Not dev");
		return;
	}

	dev->a2dp_active = set ? 1 : 0;
    dev->deactive_cnt = 0;
}

uint8_t btsrv_rdm_a2dp_get_active(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (!dev) {
		SYS_LOG_WRN("Not dev");
		return 0;
	}

	return dev->a2dp_active;
}

struct bt_conn *btsrv_rdm_a2dp_get_active_dev(void)
{
	struct rdm_device *dev = btsrv_rdm_a2dp_get_actived_device();

	if (dev) {
		return dev->base_conn;
	}

	return NULL;
}

struct bt_conn *btsrv_rdm_a2dp_get_second_dev(void)
{
	struct bt_conn *conn = btsrv_rdm_a2dp_get_active_dev();
	struct rdm_device *dev = btsrv_rdm_find_second_dev_by_connect_type(conn, BTSRV_CONNECT_ACL);

	if (dev) {
		return dev->base_conn;
	}

	return NULL;
}

struct bt_conn *btsrv_rdm_avrcp_get_active_dev(void)
{
	struct rdm_device *dev = btsrv_rdm_avrcp_get_actived_device();

	if (dev) {
		return dev->base_conn;
	}

	return NULL;
}

int btsrv_rdm_is_a2dp_stream_open(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_connect_type(base_conn, BTSRV_CONNECT_ACL);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return 0;
	}

	return (dev->a2dp_priority & A2DP_PRIOROTY_STREAM_OPEN) ? 1 : 0;
}

int btsrv_rdm_is_a2dp_stop_wait(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_connect_type(base_conn, BTSRV_CONNECT_ACL);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return 0;
	}

	return (dev->a2dp_priority & (A2DP_PRIOROTY_STOP_WAIT|A2DP_PRIOROTY_CALL_WAIT)) ? 1 : 0;
}


int btsrv_rdm_a2dp_set_codec_info(struct bt_conn *base_conn, uint8_t format, uint8_t sample_rate, uint8_t cp_type)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_connect_type(base_conn, BTSRV_CONNECT_ACL);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return -ENODEV;
	}

	dev->format = format;
	dev->sample_rate = sample_rate;
	dev->cp_type = cp_type;

	return 0;
}

int btsrv_rdm_a2dp_get_codec_info(struct bt_conn *base_conn, uint8_t *format, uint8_t *sample_rate, uint8_t *cp_type)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_connect_type(base_conn, BTSRV_CONNECT_ACL);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return -ENODEV;
	}

	if (format) {
		*format = dev->format;
	}

	if (sample_rate) {
		*sample_rate = dev->sample_rate;
	}

	if (cp_type) {
		*cp_type = dev->cp_type;
	}

	return 0;
}

void btsrv_rdm_get_a2dp_acitve_mac(bd_address_t *addr)
{
	struct rdm_device *dev = btsrv_rdm_a2dp_get_actived_device();

	if (dev) {
		memcpy(addr, &dev->bt_addr, sizeof(bd_address_t));
	} else {
		memset(addr, 0, sizeof(bd_address_t));
	}
}

uint16_t btsrv_rdm_get_a2dp_acitve_hdl(void)
{
	struct rdm_device *dev = btsrv_rdm_a2dp_get_actived_device();

	if (dev) {
		return dev->acl_hdl;
	} else {
		return 0;
	}
}


int btsrv_rdm_set_a2dp_pending_ahead_start(struct bt_conn *base_conn, uint8_t start)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return -ENODEV;
	}

	dev->a2dp_pending_ahead_start = start ? 1 : 0;
	return 0;
}

uint8_t btsrv_rdm_get_a2dp_pending_ahead_start(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return 0;
	}

	return dev->a2dp_pending_ahead_start;
}

void btsrv_rdm_avrcp_delay_set_absolute_volume(struct bt_conn *base_conn, uint8_t volume)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if ((!dev)||(!dev->avrcp_connected)) {
		SYS_LOG_WRN("not connected??\n");
		return;
	}

	if (dev->avrcp_set_absolute_volume == volume) {
		return;
	}

	dev->avrcp_set_absolute_volume = volume;

	if (os_delayed_work_remaining_get(&dev->avrcp_set_absolute_volume_delay_work) > 0) {
		dev->absolute_volume_filtr_cnt++;
	}

	os_delayed_work_cancel(&dev->avrcp_set_absolute_volume_delay_work);

	if (dev->absolute_volume_filtr_cnt >= ABSOLUTE_VOLUME_FILTR_MAX_CNT) {
		dev->absolute_volume_filtr_cnt = 0;
		os_delayed_work_submit(&dev->avrcp_set_absolute_volume_delay_work, 0);
	}else {
		os_delayed_work_submit(&dev->avrcp_set_absolute_volume_delay_work, 50);
	}
}


void btsrv_rdm_avrcp_set_absolute_volume_delay_proc(os_work *work)
{
	struct rdm_device* dev;
	struct rdm_device* dev_tmp = CONTAINER_OF(work, struct rdm_device, avrcp_set_absolute_volume_delay_work);

	dev = btsrv_rdm_find_dev_by_conn(dev_tmp->base_conn);
	if ((!dev)||(!dev->avrcp_connected)) {
		SYS_LOG_WRN("not connected??\n");
		return;
	}

	struct btsrv_rdm_avrcp_cb param;
	param.conn = dev->base_conn;
	param.event_id = BT_AVRCP_EVENT_VOLUME_CHANGED;
	param.status = dev->avrcp_set_absolute_volume;

	btsrv_event_notify_malloc(MSG_BTSRV_AVRCP, MSG_BTSRV_AVRCP_NOTIFY_CB, (uint8_t *)&param, sizeof(param), 0);

	dev->avrcp_set_absolute_volume = ABSOLUTE_VLOUME_INVALID;
}


int btsrv_rdm_set_avrcp_connected(struct bt_conn *base_conn, bool connected)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return -ENODEV;
	}

	if ((connected && dev->avrcp_connected) ||
		((!connected) && (!dev->avrcp_connected))) {
		SYS_LOG_WRN("avrcp already %s\n", connected ? "connected" : "disconnected");
		return -EALREADY;
	}

	dev->absolute_volume_filtr_cnt = 0;
    dev->avrcp_connecting_pended = 0;

	if (connected) {
		dev->avrcp_connected = 1;
		os_delayed_work_init(&dev->avrcp_set_absolute_volume_delay_work, btsrv_rdm_avrcp_set_absolute_volume_delay_proc);
	} else {
		dev->avrcp_connected = 0;
		os_delayed_work_cancel(&dev->avrcp_set_absolute_volume_delay_work);
	}

    if(dev->avrcp_connecting_pending || dev->avrcp_playing_pending){
        dev->avrcp_connecting_pending = 0;
        dev->avrcp_playing_pending = 0;
        os_delayed_work_cancel(&dev->avrcp_status_pending_delay_work);
    }

	return 0;
}

/* Just for pts test */
struct bt_conn *btsrv_rdm_avrcp_get_connected_dev(void)
{
	struct rdm_device *dev;
	sys_snode_t *node;

	SYS_SLIST_FOR_EACH_NODE(&p_rdm->dev_list, node) {
		dev = __RMT_DEV(node);
		if (dev->avrcp_connected == 1) {
			return dev->base_conn;
		}
	}

	return NULL;
}

void btsrv_rdm_set_avrcp_sync_vol_start_time(struct bt_conn *base_conn, uint32_t start_time)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return;
	}

	dev->avrcp_sync_volume_start_time = start_time;
}

uint32_t btsrv_rdm_get_avrcp_sync_vol_start_time(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return 0;
	}

	return dev->avrcp_sync_volume_start_time;
}

void btsrv_rdm_set_avrcp_sync_volume_wait(struct bt_conn *base_conn, uint8_t set)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return;
	}

	dev->avrcp_sync_volume_wait = set ? 1 : 0;
}

uint8_t btsrv_rdm_get_avrcp_sync_volume_wait(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return 0;
	}

	return dev->avrcp_sync_volume_wait;
}

void *btsrv_rdm_avrcp_get_pass_info(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return NULL;
	}

	return &dev->avrcp_pass_info;
}

void btsrv_rdm_set_avrcp_playstatus_info(struct bt_conn *base_conn, uint32_t *song_len, uint32_t *song_pos, uint8_t *play_state)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return;
	}
	dev->song_len = *song_len;
	dev->song_pos = *song_pos;
	dev->play_state = *play_state;
}

void btsrv_rdm_get_avrcp_playstatus_info(struct bt_conn *base_conn, uint32_t *song_len, uint32_t *song_pos, uint8_t *play_state)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return;
	}

	if (song_len) {
		*song_len = dev->song_len;
	}

	if (song_pos) {
		*song_pos = dev->song_pos;
	}

	if (play_state) {
		*play_state = dev->play_state;
	}
}

void btsrv_rdm_avrcp_connecting_pending_delay_proc(os_work *work)
{
	struct rdm_device* dev;
	struct rdm_device* dev_tmp = CONTAINER_OF(work, struct rdm_device, avrcp_status_pending_delay_work);

	dev = btsrv_rdm_find_dev_by_conn(dev_tmp->base_conn);
	if ((!dev)) {
		SYS_LOG_WRN("not connected??\n");
		return;
	}

    SYS_LOG_WRN("timeout!\n");
    dev->avrcp_connecting_pending = 0;
    dev->avrcp_connecting_pended = 1;
}

void btsrv_rdm_avrcp_playing_pending_delay_proc(os_work *work)
{
	struct rdm_device* dev;
	struct rdm_device* dev_tmp = CONTAINER_OF(work, struct rdm_device, avrcp_status_pending_delay_work);

	dev = btsrv_rdm_find_dev_by_conn(dev_tmp->base_conn);
	if ((!dev)) {
		SYS_LOG_WRN("not connected??\n");
		return;
	}

    SYS_LOG_WRN("timeout!\n");
    dev->avrcp_playing_pending = 0;
    dev->avrcp_playing_pended = 1;
}

void btsrv_rdm_set_avrcp_connecting_pending(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return;
	}

    if(dev->avrcp_connecting_pending || dev->avrcp_connecting_pended){
        return;
    }

    SYS_LOG_INF("pendinng!\n");

    dev->avrcp_connecting_pending = 1;
    os_delayed_work_init(&dev->avrcp_status_pending_delay_work, btsrv_rdm_avrcp_connecting_pending_delay_proc);
    os_delayed_work_submit(&dev->avrcp_status_pending_delay_work, BTSRV_AVRCP_CONNECTING_PENDING);
}

bool btsrv_rdm_is_avrcp_connected_pending(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return false;
	}

    return dev->avrcp_connecting_pending;
}

bool btsrv_rdm_is_avrcp_connected_pended(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return true;
	}

    return dev->avrcp_connecting_pended;
}

void btsrv_rdm_set_avrcp_playing_pending(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return;
	}

    if(!dev->avrcp_connected){
        return;
    }

    if(dev->avrcp_playing_pending){
        return;
    }

    SYS_LOG_INF("pendinng!\n");

    dev->avrcp_playing_pending = 1;
    os_delayed_work_init(&dev->avrcp_status_pending_delay_work, btsrv_rdm_avrcp_playing_pending_delay_proc);
    os_delayed_work_submit(&dev->avrcp_status_pending_delay_work, BTSRV_AVRCP_PLAYING_PENDING);
}

bool btsrv_rdm_is_avrcp_playing_pending(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return false;
	}

    return dev->avrcp_playing_pending;
}

bool btsrv_rdm_is_avrcp_playing_pended(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return true;
	}

    return dev->avrcp_playing_pended;
}

int btsrv_rdm_set_hfp_connected(struct bt_conn *base_conn, bool connected)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return -ENODEV;
	}

	if ((connected && dev->hfp_connected) ||
		((!connected) && (!dev->hfp_connected))) {
		SYS_LOG_WRN("hfp already %s\n", connected ? "connected" : "disconnected");
		return -EALREADY;
	}

	if (connected) {
		dev->hfp_connected = 1;
	} else {
		dev->hfp_connected = 0;
	}
	return 0;
}

int btsrv_rdm_set_hfp_role(struct bt_conn *base_conn, uint8_t role)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return -ENODEV;
	}

	dev->hfp_role = role;
	return 0;
}

int btsrv_rdm_get_hfp_role(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return BTSRV_HFP_ROLE_HF;
	}

	return dev->hfp_role;
}

int btsrv_rdm_hfp_actived(struct bt_conn *base_conn, uint8_t actived, uint8_t force)
{
	struct rdm_device *dev;
	struct rdm_device *others;

	SYS_LOG_INF("base_conn: %p actived %d\n", base_conn, actived);
	/* active state may update during connect  */
	dev = btsrv_rdm_find_dev_by_connect_type_tws(base_conn, BTSRV_CONNECT_ACL, BTSRV_TWS_NONE);
	if (dev == NULL) {
		SYS_LOG_WRN("Not phone or not connected");
		return -ENODEV;
	} else {
		SYS_LOG_INF("Hfp active hdl 0x%x actived %d", dev->acl_hdl, actived);
	}

	others = btsrv_rdm_find_second_dev_by_connect_type(base_conn, BTSRV_CONNECT_ACL);
	if (!others) {
		if (actived) {
			dev->hfp_active_state = BTSERV_DEV_ACTIVED;
		} else {
			dev->hfp_active_state = BTSERV_DEV_DEACTIVE;
		}
#ifdef CONFIG_SUPPORT_TWS
		btsrv_tws_sync_info_type(dev->base_conn, BTSRV_SYNC_TYPE_HFP, BTSRV_HFP_JUST_SNOOP_SYNC_INFO);
#endif
		return 0;
	}

	SYS_LOG_INF("dev 0x%x hfp_active_state %d state %d", dev->acl_hdl, dev->hfp_active_state, dev->hfp_state);
	SYS_LOG_INF("others 0x%x hfp_active_state %d state %d", others->acl_hdl, others->hfp_active_state, others->hfp_state);

	/* double phone
	 * improve: we can call back to upper layer with dev active state,
	 * upper layer decide how change active state.
	 */
	if (actived) {
		if ((dev->hfp_state >= BTSRV_HFP_STATE_CALL_INCOMING
			&& dev->hfp_state <= BTSRV_HFP_STATE_SCO_ESTABLISHED) || force){
			if (others->hfp_active_state == BTSERV_DEV_ACTIVED && (others->hfp_state >= BTSRV_HFP_STATE_CALL_INCOMING
				&& others->hfp_state <= BTSRV_HFP_STATE_SCO_ESTABLISHED) && !force) {
				dev->hfp_active_state = BTSERV_DEV_ACTIVE_PENDING;
			} else {
				if (dev->hfp_active_state != BTSERV_DEV_ACTIVED) {
					dev->hfp_active_state = BTSERV_DEV_ACTIVED;
					if (others->hfp_state >= BTSRV_HFP_STATE_CALL_INCOMING
						&& others->hfp_state <= BTSRV_HFP_STATE_SCO_ESTABLISHED) {
						others->hfp_active_state = BTSERV_DEV_ACTIVE_PENDING;
					} else {
						others->hfp_active_state = BTSERV_DEV_DEACTIVE;
					}
					if (others->hfp_state >= BTSRV_HFP_STATE_CALL_INCOMING &&
						others->hfp_state <= BTSRV_HFP_STATE_SCO_ESTABLISHED){
						uint8_t data[2*sizeof(void*)];
						memcpy(data,&dev->base_conn,sizeof(void*));
						memcpy(&data[sizeof(void*)],&others->base_conn,sizeof(void*));
						btsrv_event_notify_malloc(MSG_BTSRV_HFP, MSG_BTSRV_HFP_ACTIVED_DEV_CHANGED, (void*)data,sizeof(data),0);
					}		
				}
			}
		}
	} else {
		if (others->hfp_active_state == BTSERV_DEV_ACTIVED) {
			dev->hfp_active_state = BTSERV_DEV_DEACTIVE;
		} else if (others->hfp_active_state == BTSERV_DEV_ACTIVE_PENDING) {
			others->hfp_active_state = BTSERV_DEV_ACTIVED;
			dev->hfp_active_state = BTSERV_DEV_DEACTIVE;
			uint8_t data[2*sizeof(void*)];
			memcpy(data,&others->base_conn,sizeof(void*));
			memcpy(&data[sizeof(void*)],&dev->base_conn,sizeof(void*));
			btsrv_event_notify_malloc(MSG_BTSRV_HFP, MSG_BTSRV_HFP_ACTIVED_DEV_CHANGED, (void*)data,sizeof(data),0);
		} else {
			if (dev->hfp_state != BTSRV_HFP_STATE_INIT)
				dev->hfp_active_state = BTSERV_DEV_ACTIVE_PENDING;
			else
				dev->hfp_active_state = BTSERV_DEV_DEACTIVE;
		}
	}
#ifdef CONFIG_SUPPORT_TWS
	btsrv_tws_sync_info_type(dev->base_conn, BTSRV_SYNC_TYPE_HFP, BTSRV_HFP_JUST_SNOOP_SYNC_INFO);
	btsrv_tws_sync_info_type(others->base_conn, BTSRV_SYNC_TYPE_HFP, BTSRV_HFP_JUST_SNOOP_SYNC_INFO);
	btsrv_event_notify(MSG_BTSRV_TWS, MSG_BTSRV_TWS_PHONE_ACTIVE_CHANGE, NULL);
#endif

	return 0;
}

int btsrv_rdm_hfp_set_codec_info(struct bt_conn *base_conn, uint8_t format, uint8_t sample_rate)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_connect_type(base_conn, BTSRV_CONNECT_ACL);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return -ENODEV;
	}

	dev->hfp_format = format;
	dev->hfp_sample_rate = sample_rate;

	return 0;
}

int btsrv_rdm_hfp_get_codec_info(struct bt_conn *base_conn, uint8_t *format, uint8_t *sample_rate)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_connect_type(base_conn, BTSRV_CONNECT_ACL);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return -ENODEV;
	}

	*format = dev->hfp_format;
	*sample_rate = dev->hfp_sample_rate;

	return 0;
}

int btsrv_rdm_hfp_set_notify_phone_num_state(struct bt_conn *base_conn, uint8_t state)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_connect_type(base_conn, BTSRV_CONNECT_ACL);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return -ENODEV;
	}

	dev->hfp_notify_phone_num = state;
	return 0;
}

int btsrv_rdm_hfp_get_notify_phone_num_state(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_connect_type(base_conn, BTSRV_CONNECT_ACL);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return -ENODEV;
	}

	return dev->hfp_notify_phone_num;
}

int btsrv_rdm_hfp_set_state(struct bt_conn *base_conn, uint8_t state)
{
	struct rdm_device *dev;
	int old_state;

	dev = btsrv_rdm_find_dev_by_connect_type(base_conn, BTSRV_CONNECT_ACL);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return -ENODEV;
	}
	old_state = dev->hfp_state;
	dev->hfp_state = state;
	if ((old_state < BTSRV_HFP_STATE_CALL_INCOMING || old_state > BTSRV_HFP_STATE_SCO_ESTABLISHED) &&
		(state >= BTSRV_HFP_STATE_CALL_INCOMING && state <= BTSRV_HFP_STATE_SCO_ESTABLISHED))
		btsrv_rdm_hfp_actived(base_conn, 1, 0);
	else if ((state < BTSRV_HFP_STATE_CALL_INCOMING || state > BTSRV_HFP_STATE_SCO_ESTABLISHED) &&
		(old_state >= BTSRV_HFP_STATE_CALL_INCOMING && old_state <= BTSRV_HFP_STATE_SCO_ESTABLISHED))
		btsrv_rdm_hfp_actived(base_conn, 0, 0);
	return 0;
}

int btsrv_rdm_hfp_get_state(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_connect_type(base_conn, BTSRV_CONNECT_ACL);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return -ENODEV;
	}

	return dev->hfp_state;
}

int btsrv_rdm_hfp_set_siri_state(struct bt_conn *base_conn, uint8_t state)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_connect_type(base_conn, BTSRV_CONNECT_HFP);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return -ENODEV;
	}

	dev->siri_state = state;
	return 0;
}

int btsrv_rdm_hfp_get_siri_state(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_connect_type(base_conn, BTSRV_CONNECT_HFP);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return 0;
	}

	return dev->siri_state;
}

int btsrv_rdm_hfp_set_sco_state(struct bt_conn *base_conn, uint8_t state)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_connect_type(base_conn, BTSRV_CONNECT_ACL);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return -ENODEV;
	}
	SYS_LOG_INF("set_sco_state %d to %d\n", dev->sco_state, state);
	dev->sco_state = state;
	return 0;
}

int btsrv_rdm_hfp_get_sco_state(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_connect_type(base_conn, BTSRV_CONNECT_ACL);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return -ENODEV;
	}
	//SYS_LOG_INF("get_sco_state %d\n", dev->sco_state);

	return dev->sco_state;
}


int btsrv_rdm_hfp_set_call_state(struct bt_conn *base_conn, uint8_t active, uint8_t held, uint8_t in, uint8_t out)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_connect_type(base_conn, BTSRV_CONNECT_ACL);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return -ENODEV;
	}

	dev->active_call = active;
	dev->held_call = held;
	dev->incoming_call = in;
	dev->outgoing_call = out;
	return 0;
}

int btsrv_rdm_hfp_get_call_state(struct bt_conn *base_conn, uint8_t *active, uint8_t *held, uint8_t *in, uint8_t *out)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_connect_type(base_conn, BTSRV_CONNECT_ACL);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return -ENODEV;
	}

	*active = dev->active_call;
	*held = dev->held_call;
	*in = dev->incoming_call;
	*out = dev->outgoing_call;
	return 0;
}

struct bt_conn *btsrv_rdm_hfp_get_actived_dev(void)
{
	struct rdm_device *dev = btsrv_rdm_hfp_get_actived_device();
	if (dev) {
		/* SYS_LOG_INF("base_conn : %p actived %d\n", dev->base_conn, dev->hfp_actived); */
		return dev->base_conn;
	}

	return NULL;
}

struct bt_conn *btsrv_rdm_hfp_get_second_dev(void)
{
	struct bt_conn *conn = btsrv_rdm_hfp_get_actived_dev();
	struct rdm_device *dev = btsrv_rdm_find_second_dev_by_connect_type(conn, BTSRV_CONNECT_ACL);

	if (dev) {
		/* SYS_LOG_INF("base_conn : %p second %d\n", dev->base_conn, dev->hfp_actived); */
		return dev->base_conn;
	}

	return NULL;
}

struct bt_conn *btsrv_rdm_hfp_get_actived_sco(void)
{
	struct rdm_device *dev = btsrv_rdm_hfp_get_actived_device();

	if (dev) {
		/* SYS_LOG_INF("base_conn : %p actived %d\n", dev->base_conn, dev->hfp_actived); */
		return dev->sco_conn;
	}

	return NULL;
}

void btsrv_rdm_get_hfp_acitve_mac(bd_address_t *addr)
{
	struct rdm_device *dev = btsrv_rdm_hfp_get_actived_device();

	if (dev) {
		memcpy(addr, &dev->bt_addr, sizeof(bd_address_t));
	} else {
		memset(addr, 0, sizeof(bd_address_t));
	}
}

uint16_t btsrv_rdm_get_hfp_acitve_hdl(void)
{
	struct rdm_device *dev = btsrv_rdm_hfp_get_actived_device();

	if (dev) {
		return dev->acl_hdl;
	} else {
		return 0;
	}
}

bool btsrv_rdm_hfp_in_call_state(struct bt_conn *base_conn)
{
	struct rdm_device *dev = btsrv_rdm_find_dev_by_conn(base_conn);

	if (dev && (dev->hfp_state > BTSRV_HFP_STATE_LINKED)) {
		return true;
	} else {
		return false;
	}
}

uint32_t btsrv_rdm_get_sco_creat_time(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected!");
		return 0;
	}

	return dev->sco_creat_time;
}

void btsrv_rdm_set_sco_disconnect_wait(struct bt_conn *base_conn, uint8_t set)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (!dev) {
		SYS_LOG_WRN("not connected");
		return;
	}

	dev->sco_wait_disconnect = set ? 1 : 0;
}

uint8_t btsrv_rdm_get_sco_disconnect_wait(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (!dev) {
		SYS_LOG_WRN("not connected");
		return 0;
	}

	return dev->sco_wait_disconnect;
}

int btsrv_rdm_set_sco_connected(struct bt_conn *base_conn, struct bt_conn *sco_conn, uint8_t connected)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("dev not add\n");
		return -ENODEV;
	}

	if (connected) {
		dev->sco_creat_time = os_uptime_get_32();
		dev->sco_conn = hostif_bt_conn_ref(sco_conn);
		dev->sco_hdl = hostif_bt_conn_get_handle(sco_conn);
	} else {
		dev->sco_creat_time = 0;
		if (dev->sco_conn) {
			hostif_bt_conn_unref(dev->sco_conn);
			dev->sco_conn = NULL;
		}
		dev->sco_hdl = 0;
	}

	return 0;
}

struct bt_conn *btsrv_rdm_get_base_conn_by_sco(struct bt_conn *sco_conn)
{
	struct rdm_device *dev;

	dev = _btsrv_rdm_find_dev_by_sco_conn(sco_conn);
	if (dev == NULL) {
		SYS_LOG_ERR("not found\n");
		return NULL;
	}

	return dev->base_conn;
}

struct bt_conn *btsrv_rdm_get_sco_by_base_conn(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (!dev) {
		SYS_LOG_WRN("Not connected");
		return NULL;
	}

	return dev->sco_conn;
}

int btsrv_rdm_set_spp_connected(struct bt_conn *base_conn, bool connected)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("dev not add\n");
		return -ENODEV;
	}

	if (!connected && !dev->spp_connected) {
		SYS_LOG_WRN("All spp already disconnected\n");
		return -EALREADY;
	}

	if (connected) {
		dev->spp_connected++;
	} else {
		dev->spp_connected--;
	}
	return 0;
}

int btsrv_rdm_set_pbap_connected(struct bt_conn *base_conn, bool connected)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("dev not add\n");
		return -ENODEV;
	}

	if (!connected && !dev->pbap_connected) {
		SYS_LOG_WRN("All pbap already disconnected\n");
		return -EALREADY;
	}

	if (connected) {
		dev->pbap_connected++;
	} else {
		dev->pbap_connected--;
	}
	return 0;
}

int btsrv_rdm_set_map_connected(struct bt_conn *base_conn, bool connected)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("dev not add\n");
		return -ENODEV;
	}

	if (!connected && !dev->map_connected) {
		SYS_LOG_WRN("All pbap already disconnected\n");
		return -EALREADY;
	}

	if (connected) {
		dev->map_connected++;
	} else {
		dev->map_connected--;
	}
	return 0;
}


int btsrv_rdm_hid_actived(struct bt_conn *base_conn, uint8_t actived)
{
	struct rdm_device *dev;
	struct rdm_device *others;

	dev = btsrv_rdm_find_dev_by_connect_type_tws(base_conn, BTSRV_CONNECT_ACL, BTSRV_TWS_NONE);
	if (dev == NULL) {
		SYS_LOG_WRN("No tws_none dev connected\n");
		return -ENODEV;
	}
	
	others = btsrv_rdm_find_second_dev_by_connect_type(base_conn, BTSRV_CONNECT_ACL);

	/* only one phone , this phone always actived */
	if (!others) {
		dev->hid_plug = 1;
		return 0;
	}else{
		if(dev->hid_connected || !others->hid_connected){
			dev->hid_plug = actived;
			others->hid_plug = !actived;
		}
	}

	return 0;
}

int btsrv_rdm_set_hid_connected(struct bt_conn *base_conn, bool connected)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("dev not add\n");
		return -ENODEV;
	}

	if (!connected && !dev->hid_connected) {
		SYS_LOG_WRN("All hid already disconnected\n");
		return -EALREADY;
	}

	if (connected) {
		dev->hid_connected = 1;
		btsrv_rdm_hid_actived(base_conn,1);
	} else {
		dev->hid_connected = 0;
		btsrv_rdm_hid_actived(base_conn,0);
	}
	return 0;
}

int btsrv_rdm_set_tws_disconnected(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("dev not add\n");
		return -ENODEV;
	}

	dev->tws_connected = 0;
	return 0;
}

static struct rdm_device *btsrv_rdm_hid_get_actived_device(void)
{
	struct rdm_device *dev;
	sys_snode_t *node;

	SYS_SLIST_FOR_EACH_NODE(&p_rdm->dev_list, node) {
		dev = __RMT_DEV(node);
		if ((dev->tws == BTSRV_TWS_NONE) && dev->hid_connected) {
			return dev;
		}
	}

	SYS_SLIST_FOR_EACH_NODE(&p_rdm->dev_list, node) {
		dev = __RMT_DEV(node);
		if ((dev->tws == BTSRV_TWS_NONE) && dev->hid_plug) {
			return dev;
		}
	}
	//hid connect info may be clear,so just use a2dp info
	return NULL;
}

struct bt_conn *btsrv_rdm_hid_get_actived(void)
{
	struct rdm_device *dev = btsrv_rdm_hid_get_actived_device();

	if (dev) {
		return dev->base_conn;
	}

	return btsrv_rdm_get_actived_phone();
}

int btsrv_rdm_set_tws_role(struct bt_conn *base_conn, uint8_t role)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_connect_type(base_conn, BTSRV_CONNECT_ACL);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return -ENODEV;
	}

	dev->tws = role&0x7;
	if (role != BTSRV_TWS_NONE) {
		p_rdm->role_type = role;
		SYS_LOG_INF("set_tws_role %d\n", role);
        if((role == BTSRV_TWS_MASTER) || (role == BTSRV_TWS_SLAVE)){
		    dev->tws_connected = 1;
            btif_tws_set_expect_role(role);
        }
	}
	return 0;
}

int btsrv_rdm_get_tws_role(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return -ENODEV;
	}

	return dev->tws;
}

struct bt_conn *btsrv_rdm_get_tws_by_role(uint8_t role)
{
	struct rdm_device *dev;
	sys_snode_t *node;

	SYS_SLIST_FOR_EACH_NODE(&p_rdm->dev_list, node) {
		dev = __RMT_DEV(node);
		if (dev->tws == role)
			return dev->base_conn;
	}

	return NULL;
}

int btsrv_rdm_get_dev_role(void)
{
	if (p_rdm) {
		return p_rdm->role_type;
	}

	return 0;
}

/* Get active device, check by hfp active dev and a2dp active device */
struct bt_conn *btsrv_rdm_get_actived_phone(void)
{
	struct rdm_device *hfp_dev = btsrv_rdm_hfp_get_actived_device();
	struct rdm_device *a2dp_dev = btsrv_rdm_a2dp_get_actived_device();

	if (!hfp_dev && !a2dp_dev) {
		return NULL;
	}

	if (hfp_dev && !a2dp_dev) {
		return hfp_dev->base_conn;
	} else if (!hfp_dev && a2dp_dev) {
		return a2dp_dev->base_conn;
	} else {
		if (hfp_dev->hfp_active_state == BTSERV_DEV_ACTIVED) {
			return hfp_dev->base_conn;
		} else {
			return a2dp_dev->base_conn;
		}
	}
}

uint16_t btsrv_rdm_get_actived_phone_hdl(void)
{
	struct bt_conn *conn = btsrv_rdm_get_actived_phone();
	struct rdm_device *dev;

	if (conn) {
		dev = btsrv_rdm_find_dev_by_conn(conn);
        if(dev)
		    return dev->acl_hdl;
		else
		    return 0;
	} else {
		return 0;
	}
}

struct bt_conn *btsrv_rdm_get_nonactived_phone(void)
{
	struct bt_conn *active_conn = btsrv_rdm_get_actived_phone();
	struct rdm_device *dev;

	if (!active_conn) {
		return NULL;
	} else {
		dev = btsrv_rdm_find_second_dev_by_connect_type(active_conn, BTSRV_CONNECT_ACL);
		if (dev) {
			return dev->base_conn;
		} else {
			return NULL;
		}
	}
}

int btsrv_rdm_set_snoop_role(struct bt_conn *base_conn, uint8_t role)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_connect_type(base_conn, BTSRV_CONNECT_ACL);
	if (dev == NULL) {
		return -ENODEV;
	}

	dev->snoop_role = role;
	return 0;
}

int btsrv_rdm_get_snoop_role(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		return -ENODEV;
	}

	return dev->snoop_role;
}

int btsrv_rdm_set_snoop_mode(struct bt_conn *base_conn, uint8_t mode)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_connect_type(base_conn, BTSRV_CONNECT_ACL);
	if (dev == NULL) {
		return -ENODEV;
	}

	dev->snoop_mode = mode ? 1 : 0;
	return 0;
}

int btsrv_rdm_get_snoop_mode(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		return -ENODEV;
	}

	return dev->snoop_mode;
}

int btsrv_rdm_set_snoop_switch_lock(struct bt_conn *base_conn, uint8_t lock)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_connect_type(base_conn, BTSRV_CONNECT_ACL);
	if (dev == NULL) {
		return -ENODEV;
	}

	dev->snoop_switch_lock = lock ? 1 : 0;
	return 0;
}

int btsrv_rdm_get_snoop_switch_lock(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		return -ENODEV;
	}

	return dev->snoop_switch_lock;
}

int btsrv_rdm_set_controler_role(struct bt_conn *base_conn, uint8_t role)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return -ENODEV;
	}

	dev->controler_role = role ? 1 : 0;
	return 0;
}

int btsrv_rdm_get_controler_role(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return -ENODEV;
	}

	return dev->controler_role;
}

int btsrv_rdm_set_link_time(struct bt_conn *base_conn, uint16_t link_time)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return -ENODEV;
	}

	dev->link_time = link_time;
	return 0;
}

uint16_t btsrv_rdm_get_link_time(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return 0;
	}

	return dev->link_time;
}

void btsrv_rdm_set_dev_name(struct bt_conn *base_conn, uint8_t *name)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return;
	}
	if(dev->device_name){
		mem_free(dev->device_name);
		dev->device_name = NULL;
	}
	dev->device_name = mem_malloc(strlen(name) + 1);
	if (!dev->device_name) {
		SYS_LOG_ERR("malloc failed!!\n");
		return;
	}
	memset(dev->device_name,0,strlen(name) + 1);
	memcpy(dev->device_name, name, strlen(name));
}

uint8_t *btsrv_rdm_get_dev_name(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return NULL;
	}

	return dev->device_name;
}

int btsrv_rdm_set_avrcp_state(struct bt_conn *base_conn, uint8_t state)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return -ENODEV;
	}

    if(state){
        if(dev->avrcp_playing_pending){
            dev->avrcp_playing_pending = 0;
            os_delayed_work_cancel(&dev->avrcp_status_pending_delay_work);
        }
    }

	dev->avrcp_playstatus = state ? 1 : 0;
    dev->avrcp_playing_pended = 0;
	btif_a2dp_update_avrcp_state(base_conn, dev->avrcp_playstatus);

	return 0;
}

int btsrv_rdm_get_avrcp_state(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return -ENODEV;
	}

	return dev->avrcp_playstatus;
}

void btsrv_rdm_set_wait_to_diconnect(struct bt_conn *base_conn, bool set)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return;
	}

	dev->wait_to_disconnect = set ? 1 : 0;
}

bool btsrv_rdm_is_wait_to_diconnect(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return false;
	}

	return dev->wait_to_disconnect ? true : false;
}


void btsrv_rdm_set_profile_disconnect_timestamp(struct bt_conn *base_conn, uint32_t timestamp)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return;
	}

	dev->profile_disconnect_timestamp = timestamp;
}

uint32_t btsrv_rdm_get_profile_disconnect_timestamp(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return 0;
	}

	return dev->profile_disconnect_timestamp;
}


void btsrv_rdm_set_switch_sbc_state(struct bt_conn *base_conn, uint8_t state)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev) {
		dev->switch_sbc_state = state;
	}
}

uint8_t btsrv_rdm_get_switch_sbc_state(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev) {
		return dev->switch_sbc_state;
	} else {
		return 0;
	}
}

void btsrv_rdm_set_a2dp_meida_rx_cid(struct bt_conn *base_conn, uint16_t cid)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev) {
		dev->a2dp_media_rx_cid = cid;
	}
}

int btsrv_rdm_sync_base_info(struct bt_conn *base_conn, void *local_info, void *remote_info)
{
	struct rdm_device *dev;
	struct btsrv_sync_base_info *info;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		return -EEXIST;
	}

	if (local_info) {
		info = local_info;
		memset(info, 0, sizeof(*info));
		hostif_bt_addr_copy((bt_addr_t *)&info->addr, (bt_addr_t *)&dev->bt_addr);
		info->security_changed = dev->security_changed;
		info->controler_role = dev->controler_role;
		info->sniff_mode = dev->sniff.sniff_mode;
		if(dev->device_name){
			int name_len = MIN(sizeof(info->device_name) - 1, strlen(dev->device_name));
			if(strlen(dev->device_name) > sizeof(info->device_name) - 1){
				SYS_LOG_WRN("buffer overflow\n");
			}
			memcpy(info->device_name, dev->device_name, name_len);
		}
	}

	if (remote_info) {
		info = remote_info;
		dev->security_changed = info->security_changed;
		dev->controler_role = info->controler_role;
		dev->sniff.sniff_mode = info->sniff_mode;
		if(dev->device_name){
			mem_free(dev->device_name);
			dev->device_name = NULL;
		}
		dev->device_name = mem_malloc(strlen(info->device_name) + 1);
		if (!dev->device_name) {
			SYS_LOG_ERR("malloc failed!!\n");
			return -ENOMEM;
		}
		memset(dev->device_name,0,strlen(info->device_name) + 1);
		memcpy(dev->device_name, info->device_name, strlen(info->device_name));
	}

	return 0;
}

int btsrv_rdm_sync_paired_list_info(void *local_info, void *remote_info, uint8_t max_cnt)
{
	struct btsrv_sync_paired_list_info *info;
    if(local_info){
		info = (struct btsrv_sync_paired_list_info *)local_info;
		memset(info, 0, sizeof(struct btsrv_sync_paired_list_info));
	    btsrv_connect_get_auto_reconnect_info(info->paired_list, BTSRV_SAVE_AUTOCONN_NUM);
        info->pl_len = BTSRV_SAVE_AUTOCONN_NUM;
    }

	if(remote_info){
		info = (struct btsrv_sync_paired_list_info *)remote_info;
		btsrv_sync_remote_paired_list_info(info->paired_list,info->pl_len);
	}
	return 0;
}


int btsrv_rdm_sync_a2dp_info(struct bt_conn *base_conn, void *local_info, void *remote_info)
{
	struct rdm_device *dev;
	struct btsrv_sync_a2dp_info *info;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		return -EEXIST;
	}

	if (local_info) {
		info = local_info;
		memset(info, 0, sizeof(*info));
		hostif_bt_addr_copy((bt_addr_t *)&info->addr, (bt_addr_t *)&dev->bt_addr);
		info->a2dp_connected = dev->a2dp_connected;
		info->a2dp_active = dev->a2dp_active;
		info->a2dp_priority = dev->a2dp_priority;
		info->format = dev->format;
		info->sample_rate = dev->sample_rate;
		info->cp_type = dev->cp_type;
		info->a2dp_media_rx_cid = dev->a2dp_media_rx_cid;
	}

	if (remote_info) {
		info = remote_info;
		dev->a2dp_connected = info->a2dp_connected;
		dev->a2dp_active = info->a2dp_active;
		dev->a2dp_priority = info->a2dp_priority;
		dev->format = info->format;
		dev->sample_rate = info->sample_rate;
		dev->cp_type = info->cp_type;
		dev->a2dp_media_rx_cid = info->a2dp_media_rx_cid;
	}

	return 0;
}

int btsrv_rdm_sync_avrcp_info(struct bt_conn *base_conn, void *local_info, void *remote_info)
{
	struct rdm_device *dev;
	struct btsrv_sync_avrcp_info *info;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		return -EEXIST;
	}

	if (local_info) {
		info = local_info;
		memset(info, 0, sizeof(*info));
		hostif_bt_addr_copy((bt_addr_t *)&info->addr, (bt_addr_t *)&dev->bt_addr);
		info->avrcp_connected = dev->avrcp_connected;
	}

	if (remote_info) {
		info = remote_info;
		dev->avrcp_connected = info->avrcp_connected;
	}

	return 0;
}

int btsrv_rdm_sync_hfp_info(struct bt_conn *base_conn, void *local_info, void *remote_info)
{
	struct rdm_device *dev;
	struct btsrv_sync_hfp_info *info;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		return -EEXIST;
	}

	if (local_info) {
		info = local_info;
		memset(info, 0, sizeof(*info));
		hostif_bt_addr_copy((bt_addr_t *)&info->addr, (bt_addr_t *)&dev->bt_addr);
		info->hfp_connected = dev->hfp_connected;
		info->hfp_notify_phone_num = dev->hfp_notify_phone_num;
		info->hfp_active_state = dev->hfp_active_state;
		info->hfp_state = dev->hfp_state;
		info->siri_state = dev->siri_state;
		info->sco_state = dev->sco_state;
		info->active_call = dev->active_call;
		info->held_call = dev->held_call;
		info->incoming_call = dev->incoming_call;
		info->outgoing_call = dev->outgoing_call;
		info->hfp_format = dev->hfp_format;
		info->hfp_sample_rate = dev->hfp_sample_rate;
	}

	if (remote_info) {
		info = remote_info;
		dev->hfp_connected = info->hfp_connected;
		dev->hfp_notify_phone_num = info->hfp_notify_phone_num;
		dev->hfp_active_state = info->hfp_active_state;
		dev->hfp_state = info->hfp_state;
		dev->siri_state = info->siri_state;
		dev->sco_state = info->sco_state;
		dev->active_call = info->active_call;
		dev->held_call = info->held_call;
		dev->incoming_call = info->incoming_call;
		dev->outgoing_call = info->outgoing_call;
		dev->hfp_format = info->hfp_format;
		dev->hfp_sample_rate = info->hfp_sample_rate;
	}

	return 0;
}

void *btsrv_rdm_get_sniff_info(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		return NULL;
	}

	return &dev->sniff;
}

int btsrv_rdm_set_deactive_cnt(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		return -ENODEV;
	}
    return dev->deactive_cnt++;
}

int btsrv_rdm_clr_deactive_cnt(struct bt_conn *base_conn)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		return -ENODEV;
	}
    dev->deactive_cnt = 0;
    return 0;
}

void btsrv_rdm_set_dev_pnp_info(struct bt_conn *base_conn, uint16_t vid,uint16_t pid)
{
	struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
	if (dev == NULL) {
		SYS_LOG_WRN("not connected??\n");
		return;
	}

	dev->vendor_id = vid;
	dev->product_id = pid;

    if(vid == 0x4c){
        dev->ios_device = 1;
    }
    else{
        dev->ios_device = 0;
    }

    SYS_LOG_INF("vendor_id:0x%x product_id:0x%x ios %d",vid,pid,dev->ios_device);
}


void btsrv_rdm_set_dev_rssi(struct bt_conn *base_conn,int8_t rssi)
{
    struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
    if (dev == NULL) {
        SYS_LOG_WRN("not connected??\n");
        return;
    }
	dev->rssi = rssi;
}

int btsrv_rdm_get_dev_rssi(struct bt_conn *base_conn)
{
    struct rdm_device *dev;

    dev = btsrv_rdm_find_dev_by_conn(base_conn);
    if (dev == NULL) {
        SYS_LOG_WRN("not connected??\n");
        return -ENODEV;
    }
    return dev->rssi;
}

void btsrv_rdm_set_dev_link_quality(struct bt_conn *base_conn,int8_t quality)
{
    struct rdm_device *dev;

	dev = btsrv_rdm_find_dev_by_conn(base_conn);
    if (dev == NULL) {
        SYS_LOG_WRN("not connected??\n");
        return;
    }
	dev->quality = quality;
}

int btsrv_rdm_get_dev_link_quality(struct bt_conn *base_conn)
{
    struct rdm_device *dev;

    dev = btsrv_rdm_find_dev_by_conn(base_conn);
    if (dev == NULL) {
        SYS_LOG_WRN("not connected??\n");
        return -ENODEV;
    }
    return dev->quality;
}

void btsrv_rdm_set_dev_class(struct bt_conn *base_conn,uint8_t *cod)
{
    struct rdm_device *dev;

    dev = btsrv_rdm_find_dev_by_conn(base_conn);
    if (dev == NULL) {
        SYS_LOG_WRN("not connected??\n");
        return;
    }
    memcpy(dev->dev_class,cod,sizeof(dev->dev_class));
    if((dev->dev_class[1] & 0x1F) == 0x01){
        dev->pc_device = 1;
    }
    else{
        dev->pc_device = 0;
    }
    SYS_LOG_INF("dev class:0x%x 0x%x 0x%x pc:%d",dev->dev_class[0],dev->dev_class[1],dev->dev_class[2],dev->pc_device);
}

void btsrv_rdm_update_autoreconn_info(struct bt_conn *base_conn)
{
    struct rdm_device *dev;
    uint8_t i;
	struct autoconn_info info[BTSRV_SAVE_AUTOCONN_NUM];

    dev = btsrv_rdm_find_dev_by_conn(base_conn);
    if (dev == NULL) {
        SYS_LOG_WRN("not connected??\n");
        return;
    }

	memset(info, 0, sizeof(info));
	btsrv_connect_get_auto_reconnect_info(info, BTSRV_SAVE_AUTOCONN_NUM);
	for (i = 0; i < BTSRV_SAVE_AUTOCONN_NUM; i++) {
        if (info[i].addr_valid && !memcmp(&info[i].addr, &dev->bt_addr, sizeof(bd_address_t))) {
            dev->ios_device = info[i].ios;
            dev->pc_device = info[i].pc;
            SYS_LOG_INF("ios:%d pc:%d\n",dev->ios_device,dev->pc_device);
			break;
		}
	}
}


int btsrv_rdm_init(void)
{
	p_rdm = &btsrv_rdm;

	memset(p_rdm, 0, sizeof(struct btsrv_rdm_priv));
	sys_slist_init(&p_rdm->dev_list);
	return 0;
}

void btsrv_rdm_deinit(void)
{
	struct rdm_device *dev;
	sys_snode_t *node;

	if (p_rdm == NULL)
		return;

	SYS_SLIST_FOR_EACH_NODE(&p_rdm->dev_list, node) {
		dev = __RMT_DEV(node);

		if (dev->connected == 1) {
			btsrv_rdm_dev_free(dev);
			break;
		}
	}

	p_rdm = NULL;
}

static char *tws_role_info(uint8_t role_type)
{
	switch(role_type)
	{
	case BTSRV_TWS_MASTER:
		return "master";
	case BTSRV_TWS_SLAVE:
		return "slave";
	case BTSRV_TWS_PENDING:
		return "pending";
	}
	return "none";
}

void btsrv_rdm_dump_info(void)
{
	char addr_str[BT_ADDR_STR_LEN];
	struct rdm_device *dev;
	sys_snode_t *node;

	if (p_rdm == NULL) {
		SYS_LOG_INF("rdm not init\n");
		return;
	}

	printk("rdm info\n");
	printk("p_rdm->role_type: %d(%s)\n", p_rdm->role_type, tws_role_info(p_rdm->role_type));
	SYS_SLIST_FOR_EACH_NODE(&p_rdm->dev_list, node) {
		dev = __RMT_DEV(node);
		hostif_bt_addr_to_str((const bt_addr_t *)&dev->bt_addr, addr_str, BT_ADDR_STR_LEN);
		printk("MAC %s name %s hdl 0x%x soc hdl 0x%x\n", addr_str, dev->device_name, dev->acl_hdl, dev->sco_hdl);
		printk("Acl %d tws %d snoop %d(%d) controler %d\n", dev->connected, dev->tws, dev->snoop_role, dev->snoop_mode, dev->controler_role);
		printk("a2dp %d avrcp %d hfp %d spp %d pbap %d hid %d map %d\n", dev->a2dp_connected, dev->avrcp_connected,
				dev->hfp_connected, dev->spp_connected, dev->pbap_connected, dev->hid_connected,dev->map_connected);
		printk("A2dp active %d prio 0x%x codec %d sample %d cp %d\n", dev->a2dp_active, dev->a2dp_priority,
				dev->format, dev->sample_rate, dev->cp_type);
		printk("Hf active state %d format %d sample %d sco wait dis %d\n", dev->hfp_active_state, dev->hfp_format,
				dev->hfp_sample_rate, dev->sco_wait_disconnect);
		printk("Hf state %d siri state %d sco state %d notify state %d hfp role %d\n", dev->hfp_state, dev->siri_state,
				dev->sco_state, dev->hfp_notify_phone_num, dev->hfp_role);
		printk("Hid plug %d\n", dev->hid_plug);
		printk("Sniff mode %d interval %d entering %d exiting %d\n", dev->sniff.sniff_mode,
				dev->sniff.sniff_interval, dev->sniff.sniff_entering, dev->sniff.sniff_exiting);
	}
	printk("\n");
}
