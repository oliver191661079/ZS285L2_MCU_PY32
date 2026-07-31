/*
 * Copyright (c) 2021 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * This is the abstraction layer provided for Application layer for
 * Bluetooth Audio Stream/Media/Call/Volume/Microphone control.
 */

#include <bt_manager.h>
#include <media_type.h>
#include <mem_manager.h>
#include <timeline.h>

#include "ctrl_interface.h"
#include "bt_manager_inner.h"
#include "btmgr_tws_snoop_inner.h"
#include <hex_str.h>
#include <volume_manager.h>
#include <audio_system.h>
#include <audio_policy.h>
#include <sys_event.h>
#include <app_manager.h>
#include <property_manager.h>
#include <media_player.h>
#include <acts_bluetooth/host_interface.h>
#include "btservice_api.h"

#ifdef CONFIG_ACT_EVENT
#include <bt_act_event_id.h>
#include <logging/log_core.h>
LOG_MODULE_REGISTER(bt_mgr_audio, CONFIG_ACT_EVENT_APP_COMPILE_LEVEL);
#endif

#ifdef CONFIG_BT_HFP_HF
int bt_manager_hfp_sync_vol_to_remote_by_addr(uint16_t hdl, uint32_t call_vol);
#endif

/* slave app */
#define BT_AUDIO_APP_UNKNOWN     0
#define BT_AUDIO_APP_BR_CALL     1
#define BT_AUDIO_APP_BR_MUSIC    2
#define BT_AUDIO_APP_LE_CALL     3
#define BT_AUDIO_APP_LE_MUSIC    4
#define BT_AUDIO_APP_LE_TWS      5


/* slave conn priority, larger means higer priority */
#define BT_AUDIO_PRIO_NONE    0
#define BT_AUDIO_PRIO_NOTIFY  (1 << 0)
#define BT_AUDIO_PRIO_MEDIA   (1 << 1)
#define BT_AUDIO_PRIO_CALL    (1 << 2)

/* maximum channels belongs to the same connection */
#define BT_AUDIO_CHAN_MAX	4

#define BT_AUDIO_DELAY_LEA_ON_OFF_TIME    (1000)    /* 1000ms */

#define LEA_CIS_DICONN_CHECK_TIME         (3000)    /* 3000ms */

/* state of le audio on off*/
typedef enum {
	LEA_OFF = 0,
	LEA_ON = 1,
	LEA_OFF_ONGOING = 2,
	LEA_ON_ONGOING = 3,
} bt_audio_lea_on_off_state_e;

struct bt_audio_chan_restore_max {
	uint8_t count;
	struct bt_audio_chan chans[BT_AUDIO_CHAN_MAX];
};

struct bt_audio_kbps {
	uint16_t sample;
	/* NOTICE: sdu should be named frame_size */
	uint16_t sdu;
	/* NOTICE: interval should be named frame_duration */
	uint16_t interval;
	uint16_t kbps;
	uint8_t channels;
};

/* NOTICE: please try to optimize if the entries are not necessary! */
static const struct bt_audio_kbps bt_audio_kbps_table[] = {
	/*
	 * codec frame without 4-byte header
	 */
	/* 48k * 16bit * 2ch: vendor-specific */
	{ 48, 198, 10000, 158, 2 },
#if 0
	{ 48, 98, 5000, 158, 2 },
	{ 48, 102, 5000, 164, 2 },
	{ 48, 118, 5000, 188, 2 },
#endif
	/* 48k * 16bit * 2ch: standard */
	{ 48, 150, 7500, 160, 2 },
	{ 48, 200, 10000, 160, 2 },
	{ 48, 180, 7500, 192, 2 },
	{ 48, 240, 10000, 192, 2 },
	{ 48, 234, 7500, 248, 2 },
	{ 48, 310, 10000, 248, 2 },

	/* 48k * 16bit * 1ch: vendor-specific */
#if 0
	{ 48, 150, 10000, 120, 1 },
	{ 48, 130, 10000, 104, 1 },
	{ 48, 110, 10000, 88, 1 },
	{ 48, 98, 10000, 79, 1 },
	{ 48, 90, 10000, 72, 1 },
	{ 48, 80, 10000, 64, 1 },
	{ 48, 70, 10000, 56, 1 },
	{ 48, 60, 10000, 48, 1 },
	{ 48, 50, 10000, 40, 1 },
	{ 48, 40, 10000, 32, 1 },
	{ 48, 30, 10000, 24, 1 },
	{ 48, 20, 10000, 16, 1 },
	{ 48, 50, 5000, 79, 1 },
	{ 48, 52, 5000, 82, 1 },
#endif
	/* 48k * 16bit * 1ch: standard */
	{ 48, 75, 7500, 80, 1 },
	{ 48, 100, 10000, 80, 1 },
	{ 48, 90, 7500, 96, 1 },
	{ 48, 120, 10000, 96, 1 },
	{ 48, 117, 7500, 124, 1 },
	{ 48, 155, 10000, 124, 1 },

	/* 32k * 16bit * 2ch: standard */
	{ 32, 120, 7500, 128, 2 },
	{ 32, 160, 10000, 128, 2 },

	/* 32k * 16bit * 1ch: standard */
	{ 32, 60, 7500, 64, 1 },
	{ 32, 80, 10000, 64, 1 },

	/* 24k * 16bit * 2ch: standard */
	{ 24, 120, 10000, 96, 2 },

	/* 24k * 16bit * 1ch: standard */
	{ 24, 60, 10000, 48, 1 },

	/* 16k * 16bit * 2ch: standard */
	{ 16, 60, 7500, 64, 2 },
	{ 16, 80, 10000, 64, 2 },

	/* 16k * 16bit * 1ch: vendor-specific */
#if 0
	{ 16, 20, 5000, 32, 1 },
#endif
	/* 16k * 16bit * 1ch: standard */
	{ 16, 30, 7500, 32, 1 },
	{ 16, 40, 10000, 32, 1 },

	/* 8k * 16bit * 1ch: vendor-specific */
#if 0
	{ 8, 150, 10000, 120, 1 },
	{ 8, 130, 10000, 104, 1 },
	{ 8, 120, 10000, 96, 1 },
	{ 8, 110, 10000, 88, 1 },
	{ 8, 100, 10000, 80, 1 },
	{ 8, 98, 10000, 79, 1 },
	{ 8, 90, 10000, 72, 1 },
	{ 8, 80, 10000, 64, 1 },
	{ 8, 70, 10000, 56, 1 },
#endif
};

struct bt_audio_channel {
	sys_snode_t node;

	/*
	 * handle and id are enough to identify a audio stream in most cases,
	 * audio_handle is used in rare cases only, for example, get bt_clk.
	 */
	uint16_t audio_handle;
	uint8_t id;
	uint8_t dir;

	uint8_t state;	/* Audio Stream state */
	uint8_t format;
	uint8_t channels;	/* range: [1, 8] */
	uint8_t duration;	/* frame_duration; unit: ms, 7 means 7.5ms */
	uint32_t locations;	/* AUDIO_LOCATIONS_ */
	uint16_t kbps;
	uint16_t octets;
	uint16_t sdu;	/* max_sdu; unit: byte */
	uint8_t phy;
	uint8_t target_latency;
	uint32_t interval;	/* sdu_interval; unit: us */
	uint16_t sample;	/* sample_rate; unit: kHz */
	uint16_t audio_contexts;
	uint32_t delay;	/* presentation_delay */
	uint16_t max_latency;	/* max_transport_latency */
	uint8_t rtn;	/* retransmission_number */
	uint8_t framing;
	uint32_t sync_delay;
	uint32_t iso_interval;

	struct bt_audio_channel *bond_channel;

	bt_audio_trigger_cb trigger;
	bt_audio_trigger_cb tws_sync_trigger;

	bt_audio_start_cb start_cb;
	bt_audio_start_cb tws_start_cb;

	void* start_cb_param;
	void* tws_start_cb_param;
	uint8_t start:1;
	uint8_t tws_start:1;
	uint8_t cis_connected:1;

	timeline_t *period_tl;
};

struct bt_audio_call_inst {
	sys_snode_t node;

	uint16_t handle;
	uint8_t index;
	uint8_t state;

	/*
	 * UTF-8 URI format: uri_scheme:caller_id such as tel:96110
	 *
	 * end with '\0' if valid, all-zeroed if invalid
	 */
	uint8_t remote_uri[BT_AUDIO_CALL_URI_LEN];
};

struct bt_audio_conn {
	uint16_t handle;
	uint8_t type;	/* BT_TYPE_BR/BT_TYPE_LE */
	uint8_t role;	/* BT_ROLE_MASTER/BT_ROLE_SLAVE */
	bd_address_t addr;
	uint8_t addr_type;
	uint8_t prio;	/* BT_AUDIO_PRIO_CALL/BT_AUDIO_PRIO_MEDIA ... */
	uint8_t lea_vol; /*0-255*/
	uint8_t lea_vol_update:1;
	uint8_t lea_vol_sync_from_remote:1;

	uint8_t le_tws : 1;

    uint8_t is_br_tws:1;
    uint8_t is_lea:1;
	uint8_t cis_connected:2;

    uint8_t is_phone_dev:1;

	/* volume */
	uint8_t vol_cli_connected : 1;
	/* microphone */
	uint8_t mic_cli_connected : 1;

	uint8_t security_changed:1;

	uint8_t key_miss:1;

	uint8_t cis_disconn_err:1;

	uint16_t media_state;

	/* for BR Audio only */
	uint16_t call_state;

	/* fake audio_contexts, for BR Audio only */
	uint16_t fake_contexts;

	sys_slist_t chan_list;	/* list of struct bt_audio_channel */

	/* for Call Terminal, the call list is attached to a connection */
	sys_slist_t ct_list;	/* list of struct bt_audio_call_inst */

	/* for LE master */
	struct bt_audio_endpoints *endps;
	struct bt_audio_capabilities *caps;

	/* for LE slave sink chan operation */
	struct bt_audio_chan active_sink_chan;

	/* timeline owner channel */
	struct bt_audio_channel * tl_owner_chan;
	uint8_t * dev_name;
	union {
		uint32_t ascs_connect_time;
		uint32_t connect_time;
	};
	os_delayed_work cis_disconncet_work;

	sys_snode_t node;
};

struct bt_broadcast_channel {
	sys_snode_t node;

	/*
	 * handle and id are enough to identify a audio stream in most cases,
	 * audio_handle is used in rare cases only, for example, get bt_clk.
	 */
	uint16_t audio_handle;
	uint8_t id;

	uint8_t state;	/* BT Broadcast Stream state */
	uint8_t format;
	uint8_t channels;	/* range: [1, 8] */
	uint8_t duration;	/* frame_duration; unit: ms, 7 means 7.5ms */
	uint32_t locations;	/* AUDIO_LOCATIONS_ */
	uint16_t kbps;
	uint16_t octets;
	uint16_t sdu;	/* max_sdu; unit: byte */
	uint8_t phy;
#if 0
	uint8_t target_latency;
#endif

	/* NOTE: QoS is common for all BIS, need to optimize it?! */
	uint32_t interval;	/* sdu_interval; unit: us */
	uint16_t sample;	/* sample_rate; unit: kHz */
	uint16_t audio_contexts;
	uint32_t delay;	/* presentation_delay */
	uint16_t max_latency;	/* max_transport_latency */	// TODO: need?
	uint8_t rtn;	/* retransmission_number */
	uint8_t framing;
	uint32_t sync_delay;
	uint32_t iso_interval;
	int32_t media_delay_us;

	bt_broadcast_trigger_cb trigger;
	bt_broadcast_trigger_cb tws_sync_trigger;
	int32_t tws_sync_offset;

	bt_audio_start_cb start_cb;
	bt_audio_start_cb tws_start_cb;

	void* start_cb_param;
	void* tws_start_cb_param;
	uint8_t start:1;
	uint8_t tws_start:1;
	
	timeline_t *period_tl;
};

struct bt_broadcast_conn {
	uint32_t handle;
	uint8_t role;	/* BT_ROLE_MASTER/BT_ROLE_SLAVE */

	// TODO: need?
	uint8_t prio;	/* BT_AUDIO_PRIO_MEDIA ... */
	uint8_t is_stopping : 1;

	sys_slist_t chan_list;	/* list of struct bt_broadcast_channel */

	sys_snode_t node;
};

struct bt_audio_runtime {
	uint8_t le_audio_state;

	uint8_t le_audio_init : 1;
	uint8_t le_audio_start : 1;
	uint8_t le_audio_pause : 1;
	uint8_t le_audio_muted : 1;

	uint8_t le_audio_source_prefered_qos : 1;
	uint8_t le_audio_sink_prefered_qos : 1;

	uint8_t le_audio_codec_policy : 1;
	uint8_t le_audio_qos_policy : 1;
	uint8_t lea_connection_enable : 2;

	uint8_t lea_on_off_complete_work_valid : 1;
	uint8_t multi_point_disable : 1;
	os_delayed_work lea_on_off_complete_work;
	lea_on_off_complete_cb complete_cb;

	uint8_t le_audio_adv_enable;

	/* default preferred qos for all servers */
	struct bt_audio_preferred_qos_default source_qos;
	struct bt_audio_preferred_qos_default sink_qos;

	/* default codec policy for all clients */
	struct bt_audio_codec_policy codec_policy;
	/* default qos policy for all clients */
	struct bt_audio_qos_policy qos_policy;

	sys_slist_t conn_list;	/* list of struct bt_audio_conn */

	sys_slist_t broadcast_list;	/* list of struct bt_broadcast_conn */

	uint8_t broadcast_sink_enable : 1;
	uint8_t broadcast_sink_scan_enable : 1;

	int device_num;

	struct bt_audio_conn *active_slave;

	struct bt_audio_conn *inactive_slave;

	/*
	 * for Call Gateway, the call list is shared with all Call Terminals.
	 *
	 * NOTICE: support for LE Audio only for now.
	 */
	sys_slist_t cg_list;	/* list of struct bt_audio_call_inst */

	//uint8_t current_app;	/* current slave app */

	uint8_t cur_active_stream;	/* current active stream */
	uint8_t cur_active_handle;	/* current active handle */

	/*
	 * In order to support stop() and stopped event model.
	 *
	 * WARNNING: We only take care of some cases, it could be kind
	 * of complicated if we take care of all cases. So please be
	 * careful and make sure the application layer is simple enough.
	 *
	 * Because the stop process of LE Audio will be divided into 2
	 * parts if there are connections. part 1 is stop() and it will
	 * return -EINPROGRESS to indicate part 2 is existed, part 2 is
	 * stopped event which will handle exit() and so on if necessary.
	 *
	 * case 1: stop() and success, exit(), init(), start()
	 *         simple case, no pending_xxx bit is used.
	 *
	 * case 2: stop() and pending, start()
	 *         pending_start bit is set, it will be handled
	 *         in stopped event.
	 *
	 * case 3: stop() and pending, exit(), init(), start()
	 *         pending_exit, pending_init and pending_start
	 *         are set, le_audio_role will be updated in
	 *         init(), it will be handled in stopped_event.
	 */
	uint8_t le_audio_pending_exit : 1;
	uint8_t le_audio_pending_init : 1;
	uint8_t le_audio_pending_start : 1;
	/* NOTE: append more bits it necessary */
	uint8_t le_audio_pending_caps : 1;
	uint8_t le_audio_pending_conn_param : 1;
	uint8_t le_audio_pending_adv_param : 1;
	uint8_t le_audio_pending_pause_adv : 1;
	uint8_t le_audio_pending_pause_scan : 1;

	uint8_t  take_over_flag;
	struct bt_audio_role le_audio_role;
	struct bt_le_conn_param le_audio_conn_param;
	struct bt_le_adv_param le_audio_adv_param;
	bt_addr_le_t le_audio_adv_peer;
	/* FIXME: support other number of capability, 2 is enough for now */
	struct bt_audio_capabilities_2 caps;
};

typedef struct {
	uint16_t handle;
	uint8_t  addr[6];
	uint8_t	 associated;
} dev_record_t;

typedef struct {
	uint8_t need_update;
	br_lea_autoconn_info_t dev_info[BT_MAX_AUTOCONN_DEV];
}dev_order_t;

static int8_t broadcast_sq_mode = 0;

static struct bt_audio_runtime bt_audio;

static const struct bt_broadcast_stream_cb bis_stream_cb;

static struct bt_audio_conn *find_active_slave(void);

static void lea_on_off_work_callback(struct k_work *work);

static dev_order_t dev_disconnect_info = {
	.need_update = 1
};

int get_dev_order_info_cnt(dev_order_t *p_order)
{
	uint8_t dev_cnt = 0;
	br_lea_autoconn_info_t *p_item = NULL;

	if (!p_order) {
		SYS_LOG_ERR("param invalid\n");
		return 0;
	}

	p_item = &p_order->dev_info[0];
	for (int i = 0; i < BT_MAX_AUTOCONN_DEV; i++) {
		if (p_item[i].addr_valid) {
			dev_cnt++;
		}
	}

	return dev_cnt;
}

void print_dev_order_info(char *str, br_lea_autoconn_info_t *info)
{
	char addr[13];

	if ((!str) || (!info)) {
		SYS_LOG_ERR("param invalid\n");
		return;
	}

	memset(addr, 0, 13);
	printk("%s<%p>: \n", str, info);
    for (int i = 0; i < BT_MAX_AUTOCONN_DEV; i++) {
		hex_to_str(addr, (char *)&info[i].addr, 6);
		printk("%d->%d, %s, %d, %d, %d\n", i, info[i].conn_type, addr,
				info[i].addr_valid, info[i].streaming, info[i].connect_time);
    }
}

static void reset_dev_order_info_connect_time(br_lea_autoconn_info_t *p_item)
{
	if (!p_item) {
		SYS_LOG_ERR("param invalid\n");
		return;
	}

	for (int i = 0; i < BT_MAX_AUTOCONN_DEV; i++) {
		p_item[i].connect_time = 0;
    }
}

static void sort_dev_order_info_by_add_valid(dev_order_t *p_order, int n)
{
	br_lea_autoconn_info_t *p_item = NULL;
	br_lea_autoconn_info_t order_item_tmp;

	if ((!p_order) || (n <= 0)) {
		SYS_LOG_ERR("param invalid\n");
		return;
	}

	p_item = &p_order->dev_info[0];
	for (int i = 0; i < n-1; i++) {
        for (int j = 0; j < n-i-1; j++) {
            if (p_item[j].addr_valid < p_item[j+1].addr_valid) {
				memcpy(&order_item_tmp, &p_item[j], sizeof(br_lea_autoconn_info_t));
				memcpy(&p_item[j], &p_item[j+1], sizeof(br_lea_autoconn_info_t));
				memcpy(&p_item[j+1], &order_item_tmp, sizeof(br_lea_autoconn_info_t));
            }
        }
    }
}

static void sort_dev_order_info_by_connect_time(dev_order_t *p_order, int n)
{
	br_lea_autoconn_info_t *p_item = NULL;
	br_lea_autoconn_info_t order_item_tmp;

	if ((!p_order) || (n <= 0)) {
		SYS_LOG_ERR("param invalid\n");
		return;
	}

	p_item = &p_order->dev_info[0];
	for (int i = 0; i < n-1; i++) {
        for (int j = 0; j < n-i-1; j++) {
            if (p_item[j].connect_time < p_item[j+1].connect_time) {
				memcpy(&order_item_tmp, &p_item[j], sizeof(br_lea_autoconn_info_t));
				memcpy(&p_item[j], &p_item[j+1], sizeof(br_lea_autoconn_info_t));
				memcpy(&p_item[j+1], &order_item_tmp, sizeof(br_lea_autoconn_info_t));
            }
        }
    }
}

static void sort_dev_order_info_by_streaming(dev_order_t *p_order, int n)
{
	br_lea_autoconn_info_t *p_item = NULL;
	br_lea_autoconn_info_t order_item_tmp;

	if ((!p_order) || (n <= 0)) {
		SYS_LOG_ERR("param invalid\n");
		return;
	}

	p_item = &p_order->dev_info[0];
	for (int i = 0; i < n-1; i++) {
        for (int j = 0; j < n-i-1; j++) {
            if (p_item[j].streaming < p_item[j+1].streaming) {
				memcpy(&order_item_tmp, &p_item[j], sizeof(br_lea_autoconn_info_t));
				memcpy(&p_item[j], &p_item[j+1], sizeof(br_lea_autoconn_info_t));
				memcpy(&p_item[j+1], &order_item_tmp, sizeof(br_lea_autoconn_info_t));
            }
        }
    }
}

static void update_disconnect_info_to_nvram(void)
{
	property_set(CFG_BR_LEA_AUTOCONN_INFO,
		(void *)&dev_disconnect_info.dev_info[0],
		sizeof(br_lea_autoconn_info_t) * (BT_MAX_AUTOCONN_DEV));

	print_dev_order_info("info_to_nvram", &dev_disconnect_info.dev_info[0]);
}

void bt_manager_audio_load_disconnect_info_from_nvram(void)
{
	bool ret = 0;
	bt_addr_le_t dev;

	property_get(CFG_BR_LEA_AUTOCONN_INFO,
				(void *)&dev_disconnect_info.dev_info[0],
				sizeof(br_lea_autoconn_info_t) * (BT_MAX_AUTOCONN_DEV));

	os_sched_lock();
    ret = hostif_bt_le_get_last_paired_device(&dev);
	if ((!ret) && btif_br_is_reconnect_info_empty()) {
		memset(dev_disconnect_info.dev_info, 0, sizeof(br_lea_autoconn_info_t) * BT_MAX_AUTOCONN_DEV);
		SYS_LOG_INF("autoinfo is empty\n");
	}else {
		//After startup, connect time becomes invalid
		reset_dev_order_info_connect_time(&dev_disconnect_info.dev_info[0]);
	}
	os_sched_unlock();

	print_dev_order_info("info_from_nvram", &dev_disconnect_info.dev_info[0]);
}

void bt_manager_audio_set_disconnect_info_need_update(uint8_t need_update)
{
	os_sched_lock();
	dev_disconnect_info.need_update = need_update;
	os_sched_unlock();
}

int bt_manager_audio_get_auto_reconnect_info(br_lea_autoconn_info_t *info, uint8_t max_cnt)
{
	uint8_t read_cnt = 0;
	uint8_t dev_cnt = 0;

	if ((!info) || (!max_cnt)) {
		SYS_LOG_ERR("param invalid\n");
		return 0;
	}

	read_cnt = (max_cnt > BT_MAX_AUTOCONN_DEV) ? BT_MAX_AUTOCONN_DEV : max_cnt;
	memcpy((char *)info, (char *)&dev_disconnect_info.dev_info[0], (sizeof(br_lea_autoconn_info_t)*read_cnt));
	for (int i = 0; i < read_cnt; i++) {
		if (info[i].addr_valid) {
			dev_cnt++;
		}
	}

	return dev_cnt;
}

static int add_dev_to_disconnect_info(bd_address_t *addr, uint8_t addr_type,
    uint8_t conn_type, uint8_t streaming,uint32_t connect_time)
{
	uint8_t match = 0;
	br_lea_autoconn_info_t *p_item = NULL;

	if (!dev_disconnect_info.need_update) {
		SYS_LOG_INF("No need update\n");
		return -1;
	}

	p_item = &dev_disconnect_info.dev_info[0];
	//LEA,  BR only record single times
    for (int i = 0; i < BT_MAX_AUTOCONN_DEV; i++) {
        if (!memcmp(&p_item[i].addr, addr, sizeof(bd_address_t))) {
			memset(&p_item[i], 0, sizeof(br_lea_autoconn_info_t));
			match = 1;
		}
    }
	//addr_valid=0 sort to the oldest
	if (match) {
		sort_dev_order_info_by_add_valid(&dev_disconnect_info, BT_MAX_AUTOCONN_DEV);
	}

	//Move data sequentially, the latest at the top
	for (int i = (BT_MAX_AUTOCONN_DEV - 1); i > 0; i--) {
		memcpy(&p_item[i], &p_item[i - 1], sizeof(br_lea_autoconn_info_t));
	}
	p_item[0].conn_type = conn_type;
	p_item[0].connect_time = connect_time;
	p_item[0].streaming = (streaming)? 1 : 0;
	if (addr) {
		p_item[0].addr_valid = 1;
		p_item[0].addr_type = addr_type;
		memcpy(&p_item[0].addr, addr, sizeof(bd_address_t));
	}

    return 0;
}

int bt_manager_audio_add_all_dev_to_disconnect_info(void)
{
	uint8_t dev_cnt = 0;
	struct bt_audio_conn *audio_conn;
	struct bt_mgr_dev_info *dev_info = NULL;

	os_sched_lock();
	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
		if ((!audio_conn->is_phone_dev) ||
			(!(audio_conn->security_changed) &&
			(audio_conn->type == BT_TYPE_BR))) {
			continue;
		}

		/*lea connencted first
		 *then br security_changed but profile don't connected
		 *record lea info
		 */
		if ((audio_conn->type == BT_TYPE_BR) &&
			bt_manager_audio_has_same_lea_notify_connected(&audio_conn->addr)) {
			dev_info = bt_mgr_find_dev_info_by_hdl(audio_conn->handle);
			if ((dev_info) &&
				(!dev_info->a2dp_connected) &&
				(!dev_info->hf_connected)) {
				SYS_LOG_INF("ignore: %d, %d\n", dev_info->a2dp_connected, dev_info->hf_connected);
				continue;
			}
		}

		add_dev_to_disconnect_info(&audio_conn->addr, audio_conn->addr_type,
									audio_conn->type, (audio_conn->prio & BT_AUDIO_PRIO_MEDIA),
									audio_conn->connect_time);
	}

	dev_cnt = get_dev_order_info_cnt(&dev_disconnect_info);
	if(dev_cnt > 1) {
		sort_dev_order_info_by_connect_time(&dev_disconnect_info, dev_cnt);
		sort_dev_order_info_by_streaming(&dev_disconnect_info, dev_cnt);
	}
	os_sched_unlock();

	update_disconnect_info_to_nvram();

	return 0;
}

bool bt_manager_audio_is_phone_connected(bd_address_t *addr, uint8_t type)
{
	struct bt_audio_conn *audio_conn;

	if (!addr) {
		SYS_LOG_ERR("param invalid\n");
		return false;
	}

	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
		if ((!audio_conn->is_phone_dev) ||
			(audio_conn->type != type)) {
			continue;
		}
		if (0 == memcmp(&audio_conn->addr, addr, sizeof(bd_address_t))) {
			return true;
		}
	}

	return false;
}

static int bt_manager_madia_is_multi_stream_started(void)
{
	uint8_t cnt = 0;
	struct bt_audio_channel *chan;
	struct bt_audio_conn *audio_conn = NULL;

	os_sched_lock();

	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
		if (audio_conn->role == BT_ROLE_SLAVE && audio_conn->is_phone_dev) {

			SYS_SLIST_FOR_EACH_CONTAINER(&audio_conn->chan_list, chan, node) {
				SYS_LOG_DBG("%x, %x", chan->audio_contexts, chan->state);
				if ((chan->audio_contexts & BT_AUDIO_CONTEXT_MEDIA) &&
					(chan->state == BT_AUDIO_STREAM_ENABLED ||
					 chan->state == BT_AUDIO_STREAM_STARTED)) {
					cnt++;
				}
			}
		}
	}

	os_sched_unlock();

	return (cnt > 1)? 1 : 0;
}

static int bt_manager_audio_event_notify(struct bt_audio_conn* audio_conn, int event_id, void *event_data, int event_data_size)
{
	if (!audio_conn){
		return bt_manager_event_notify(event_id, event_data, event_data_size);
	}

	if (bt_manager_madia_is_multi_stream_started()) {
		if (audio_conn == find_active_slave()) {
			SYS_LOG_INF("multi stream");
			return bt_manager_event_notify(event_id, event_data, event_data_size);
		}
	}else {
		SYS_LOG_INF("single stream");
		return bt_manager_event_notify(event_id, event_data, event_data_size);
	}

	return 0;
}

void bt_manager_audio_set_scan_policy(uint8_t scan_type, uint8_t scan_policy_mode)
{
	int ret = 0;
	struct bt_scan_policy policy;
	policy.scan_type = scan_type;
	policy.scan_policy_mode = scan_policy_mode;
	ret = btif_br_set_scan_policy(&policy);
	if (ret) {
		SYS_LOG_ERR("err:%d\n", ret);
	}
	SYS_LOG_INF("type:%d,mode:%d\n", scan_type, scan_policy_mode);
}


static inline bool is_bt_le_master(struct bt_audio_conn *audio_conn)
{
	if ((audio_conn->type == BT_TYPE_LE) &&
	    (audio_conn->role == BT_ROLE_MASTER)) {
		return true;
	}

	return false;
}

static uint8_t bt_le_master_count(void)
{
	struct bt_audio_conn *audio_conn;
	uint8_t count = 0;

	os_sched_lock();

	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
		if (is_bt_le_master(audio_conn)) {
			count++;
		}
	}

	os_sched_unlock();

	return count;
}

static uint8_t get_slave_app(struct bt_audio_conn *audio_conn)
{
	if (!audio_conn) {
		return BT_AUDIO_STREAM_NONE;
	}

	if (audio_conn->type == BT_TYPE_BR) {
		if (audio_conn->prio & BT_AUDIO_PRIO_CALL) {
			return BT_AUDIO_STREAM_BR_CALL;
		} else if (audio_conn->prio & BT_AUDIO_PRIO_MEDIA) {
			return BT_AUDIO_STREAM_BR_MUSIC;
		}
	} else if (audio_conn->type == BT_TYPE_LE) {
		if (audio_conn->le_tws) {
			return BT_AUDIO_STREAM_LE_TWS;
		}
		if (audio_conn->prio & BT_AUDIO_PRIO_CALL) {
			return BT_AUDIO_STREAM_LE_CALL;
		} else if (audio_conn->prio & BT_AUDIO_PRIO_MEDIA) {
			return BT_AUDIO_STREAM_LE_MUSIC;
		}
	}

	return BT_AUDIO_STREAM_NONE;
}

struct bt_audio_conn *find_conn_by_prio(uint8_t type, uint8_t prio)
{
	struct bt_audio_conn *conn = NULL;
	struct bt_audio_conn *audio_conn;

	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
		if (audio_conn->role != BT_ROLE_SLAVE || audio_conn->is_br_tws) {
			continue;
		}

		if ((type == BT_TYPE_LE) && !audio_conn->is_lea){
			continue;
		}

		//SYS_LOG_INF("hdl 0x%x: prio %d",audio_conn->handle ,audio_conn->prio);

		if (!prio && (audio_conn->type == type)){
			conn = audio_conn;
			break;
		}else if ((audio_conn->type == type) && audio_conn->prio & prio){
			conn = audio_conn;
		}

	}
	return conn;
}
/*
 * get the slave with highest priority
 *
 * call's priority > media' priority > notification' priority, FIFO if priority is same
 *
 * @param except_active if except for current active_slave, true
 * means not include, false means include.
 */
static struct bt_audio_conn *get_highest_slave(struct bt_audio_conn *new_conn,
				bool except_active)
{
	struct bt_audio_conn *highest_slave;
	struct bt_audio_conn *audio_conn;
	uint8_t prio;

	if (except_active) {
		highest_slave = NULL;
		prio = BT_AUDIO_PRIO_NONE;
	} else {
		/* enable preemption for new conn */
		highest_slave = new_conn;
		prio = highest_slave->prio;
	}
	SYS_LOG_INF("cur hdl 0x%x: prio %d", bt_audio.active_slave->handle, bt_audio.active_slave->prio);

	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
		if (audio_conn->role != BT_ROLE_SLAVE) {
			continue;
		}

		SYS_LOG_INF("hdl 0x%x: prio %d", audio_conn->handle, audio_conn->prio);

		if (audio_conn->prio > prio) {
			highest_slave = audio_conn;
			prio = audio_conn->prio;
		}
	}

	return highest_slave;
}

int bt_manager_audio_current_stream(void)
{
	if (!bt_audio.active_slave) {
		return BT_AUDIO_STREAM_NONE;
	}

	return bt_audio.cur_active_stream;
}

static int switch_slave_app(struct bt_audio_conn *audio_conn)
{
	struct bt_manager_stream_report rep;
	uint8_t old_stream = bt_audio.cur_active_stream;
	uint16_t old_handle = bt_audio.cur_active_handle;
	uint8_t new_stream = get_slave_app(audio_conn);
	uint16_t new_handle = audio_conn? audio_conn->handle: 0;

	if ((old_stream == new_stream) && (old_handle == new_handle)) {
		SYS_LOG_INF("no need");
		return 0;
	}

	bt_audio.cur_active_stream = new_stream;
	bt_audio.cur_active_handle = new_handle;
	rep.handle = new_handle;
	rep.stream = new_stream;

	/* notify app to handle audio switch */
	SYS_LOG_INF("hdl:%x,stream:%d\n",rep.handle,rep.stream);
	bt_manager_audio_event_notify(NULL, BT_AUDIO_SWITCH_NOTIFY, &rep, sizeof(rep));

	bt_manager_event_notify(BT_AUDIO_SWITCH,&rep, sizeof(rep));

	return 0;
}

static void bt_manager_audio_stop_another(struct bt_audio_conn *audio_conn,struct bt_audio_conn *new_audio_conn)
{
    btmgr_sync_ctrl_cfg_t *sync_ctrl_config = bt_manager_get_sync_ctrl_config();
    if (!sync_ctrl_config->stop_another_when_one_playing) {
       return;
	}
	if ((!audio_conn) || (!new_audio_conn)) {
	   return;
	}
    SYS_LOG_INF("type:%d:%d,prio:%d:%d,state:%d:%d \n",audio_conn->type,new_audio_conn->type,
	                                audio_conn->prio,new_audio_conn->prio,audio_conn->media_state,new_audio_conn->media_state);
	if ((audio_conn->type == BT_TYPE_BR)&&
	    (new_audio_conn->type == BT_TYPE_LE)) {
		if ((audio_conn->prio & BT_AUDIO_PRIO_MEDIA)&&
		    (new_audio_conn->prio & BT_AUDIO_PRIO_MEDIA)) {
			SYS_LOG_INF("avrcp pause:\n");
			btif_avrcp_send_command_by_hdl(audio_conn->handle,BTSRV_AVRCP_CMD_PAUSE);
		}
	}
	else if ((audio_conn->type == BT_TYPE_LE)) {
		if ((audio_conn->prio & BT_AUDIO_PRIO_MEDIA)&&
			(new_audio_conn->prio & BT_AUDIO_PRIO_MEDIA)) {
			/* send mcs pause cmd */
			audio_conn->media_state = BT_STATUS_INACTIVE;
			int ret = bt_manager_le_media_pause(audio_conn->handle);
			SYS_LOG_INF("lea pause:%d\n",ret);
		}
	}
}

/*
 * free: true if BT is disconnected and going to be freed, else false.
 *
 * case 1: add prio
 * case 2: remove prio
 * case 3: audio_conn disconnected
 */
static int update_active_slave(struct bt_audio_conn *audio_conn, bool free)
{
	struct bt_audio_conn *new_active_slave;

	if (!audio_conn || audio_conn->role != BT_ROLE_SLAVE) {
		return -EINVAL;
	}

	os_sched_lock();

	if (free) {
		if (bt_audio.active_slave == audio_conn) {
			bt_audio.active_slave = get_highest_slave(NULL, true);
			goto update;
		} else {
			goto exit;
		}
	}

	if (!bt_audio.active_slave) {
		bt_audio.active_slave = audio_conn;
		goto update;
	}

	new_active_slave = get_highest_slave(audio_conn, false);

    if (bt_audio.active_slave != new_active_slave) {
		if(bt_audio.active_slave->type == BT_TYPE_LE && new_active_slave->type == BT_TYPE_LE){
			bt_manager_lea_set_active_device(new_active_slave->handle);
		}
		if(bt_audio.active_slave->type == BT_TYPE_LE){
			bt_manager_lea_set_device_policy(bt_audio.active_slave->handle,5);
		}
		bt_manager_audio_stop_another(bt_audio.active_slave,new_active_slave);
		if(new_active_slave->type == BT_TYPE_LE){
			bt_manager_lea_set_device_policy(new_active_slave->handle,0);
		}
	}

	bt_audio.active_slave = new_active_slave;

update:
	switch_slave_app(bt_audio.active_slave);

exit:
	os_sched_unlock();

	SYS_LOG_INF("active_slave: %p, %p\r\n", bt_audio.active_slave, bt_audio.inactive_slave);
	return 0;
}

static int inactive_slave_check(void)
{
	if (bt_audio.active_slave == bt_audio.inactive_slave) {
		bt_audio.inactive_slave = NULL;
	}
	SYS_LOG_INF("active_slave: %p, %p\r\n", bt_audio.active_slave, bt_audio.inactive_slave);

	return 0;
}

/*
 * check if need to add or remove prio for the audio_conn (slave only)
 *
 * case 1: stream_enable
 * case 2: stream_stop
 * case 3: stream_update
 * case 4: stream_release
 */
static int update_slave_prio(struct bt_audio_conn *audio_conn)
{
	struct bt_audio_channel *chan;
	uint8_t old_prio;
	uint8_t new_prio;
	int ret = -1;

	/* do nothing for master */
	if (audio_conn->role == BT_ROLE_MASTER) {
		return 0;
	}

	os_sched_lock();

	new_prio = BT_AUDIO_PRIO_NONE;
	old_prio = audio_conn->prio;

	if (audio_conn->fake_contexts & BT_AUDIO_CONTEXT_CALL) {
		new_prio = BT_AUDIO_PRIO_CALL;
		goto done;
	}

	SYS_SLIST_FOR_EACH_CONTAINER(&audio_conn->chan_list, chan, node) {
		if (chan->audio_contexts & BT_AUDIO_CONTEXT_CALL
			&& (chan->state == BT_AUDIO_STREAM_ENABLED
				|| chan->state == BT_AUDIO_STREAM_STARTED)) {
			new_prio = BT_AUDIO_PRIO_CALL;
			break;
		}

		if (chan->audio_contexts != 0
			&& (chan->state == BT_AUDIO_STREAM_ENABLED
				|| chan->state == BT_AUDIO_STREAM_STARTED)) {
			new_prio = BT_AUDIO_PRIO_MEDIA;
			break;
		}
	}

done:
	os_sched_unlock();
	SYS_LOG_INF("hdl: 0x%x prio %d -> %d", audio_conn->handle, old_prio, new_prio);

	if (old_prio != new_prio) {
		audio_conn->prio = new_prio;
		ret = update_active_slave(audio_conn, false);
		if ((new_prio > BT_AUDIO_PRIO_NOTIFY) && (ret >= 0)) {
			inactive_slave_check();
		}
		return ret;
	}

	if (new_prio > BT_AUDIO_PRIO_NOTIFY) {
		inactive_slave_check();
	}
	return 0;
}

static struct bt_audio_conn *get_active_slave(void)
{
	return bt_audio.active_slave;
}

/* if no active_salve, find any one! */
static struct bt_audio_conn *find_active_slave(void)
{
	struct bt_audio_conn *audio_conn = NULL;
	struct bt_audio_conn *active_conn = NULL;

	os_sched_lock();

	if ((bt_audio.device_num > 1) && (bt_audio.inactive_slave)) {
		SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
			if ((audio_conn->role == BT_ROLE_SLAVE) && (audio_conn != bt_audio.inactive_slave)
				&& audio_conn->is_phone_dev) {
				active_conn = audio_conn;
			}
		}
	}else {
		struct bt_mgr_dev_info *dev_info = NULL;
		if (bt_audio.active_slave) {
			if (bt_audio.active_slave->type == BT_TYPE_BR) {
				dev_info = bt_mgr_find_dev_info_by_hdl(bt_audio.active_slave->handle);
#ifdef CONFIG_BT_HFP_HF
                if ((dev_info) && ((dev_info->a2dp_connected) || (dev_info->hf_connected)))
#else
                if ((dev_info) && (dev_info->a2dp_connected))
#endif
				{
					active_conn = bt_audio.active_slave;
					goto done;
				}else {
					//Find from audio list
				}
			}else{
				active_conn = bt_audio.active_slave;
				goto done;
			}
		}

		SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
			if (audio_conn->role == BT_ROLE_SLAVE && audio_conn->is_phone_dev) {
				if (audio_conn->type == BT_TYPE_BR) {
					dev_info = bt_mgr_find_dev_info_by_hdl(audio_conn->handle);
#ifdef CONFIG_BT_HFP_HF
                    if ((dev_info) && ((dev_info->a2dp_connected)||( dev_info->hf_connected)))
#else
                    if ((dev_info) && (dev_info->a2dp_connected))
#endif
					{
						active_conn = audio_conn;
					}
				}else {
					active_conn = audio_conn;
				}
			}
		}
	}

done:
	os_sched_unlock();

	return active_conn;
}

/* find first slave call by state */
static struct bt_audio_call_inst *find_slave_call(struct bt_audio_conn *audio_conn,
				uint8_t state)
{
	struct bt_audio_call_inst *call;

	os_sched_lock();

	SYS_SLIST_FOR_EACH_CONTAINER(&audio_conn->ct_list, call, node) {
		if (call->state == state) {
			os_sched_unlock();
			return call;
		}
	}

	os_sched_unlock();

	return NULL;
}

static struct bt_audio_call_inst *find_fisrt_slave_call(struct bt_audio_conn *audio_conn)
{
	struct bt_audio_call_inst *call;

	os_sched_lock();

	SYS_SLIST_FOR_EACH_CONTAINER(&audio_conn->ct_list, call, node) {
		os_sched_unlock();
		return call;
	}

	os_sched_unlock();

	return NULL;
}

static uint16_t get_kbps(uint16_t sample, uint16_t sdu, uint16_t interval, uint8_t channels)
{
	const struct bt_audio_kbps *kbps = bt_audio_kbps_table;
	uint8_t i;

	for (i = 0; i < ARRAY_SIZE(bt_audio_kbps_table); i++) {
		if ((kbps->sample == sample) &&
		    (kbps->sdu == sdu) &&
		    (kbps->interval == interval) &&
		    (kbps->channels == channels)) {
			return kbps->kbps;
		}
		kbps++;
	}

	return 0;
}

__ramfunc static struct bt_audio_conn *find_conn(uint16_t handle)
{
	struct bt_audio_conn *audio_conn;

	os_sched_lock();

	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
		if (audio_conn->handle == handle) {
			os_sched_unlock();
			return audio_conn;
		}
	}

	os_sched_unlock();

	return NULL;
}

__ramfunc static struct bt_audio_channel *find_channel(uint16_t handle, uint8_t id)
{
	struct bt_audio_conn *audio_conn;
	struct bt_audio_channel *chan;

	audio_conn = find_conn(handle);
	if (!audio_conn) {
		return NULL;
	}

	os_sched_lock();

	SYS_SLIST_FOR_EACH_CONTAINER(&audio_conn->chan_list, chan, node) {
		if (chan->id == id) {
			os_sched_unlock();
			return chan;
		}
	}

	os_sched_unlock();

	return NULL;
}

static struct bt_audio_channel *find_channel2(struct bt_audio_conn *audio_conn,
				uint8_t id)
{
	struct bt_audio_channel *chan;

	os_sched_lock();

	SYS_SLIST_FOR_EACH_CONTAINER(&audio_conn->chan_list, chan, node) {
		if (chan->id == id) {
			os_sched_unlock();
			return chan;
		}
	}

	os_sched_unlock();

	return NULL;
}

static struct bt_audio_channel *find_active_channel(struct bt_audio_conn *audio_conn)
{
	struct bt_audio_channel *chan;

	os_sched_lock();

	SYS_SLIST_FOR_EACH_CONTAINER(&audio_conn->chan_list, chan, node) {
		if (chan->state == BT_AUDIO_STREAM_ENABLED
			|| chan->state == BT_AUDIO_STREAM_STARTED) {
			os_sched_unlock();
			return chan;
		}
	}

	os_sched_unlock();

	return NULL;
}

bool is_le_tws(uint16_t handle)
{
#ifdef CONFIG_BT_LETWS
	struct btmgr_letws_context_t* context = btmgr_get_letws_context();

	if (!context->le_remote_addr_valid || !btif_get_le_addr_by_handle(handle)) {
		SYS_LOG_INF("not le_remote_addr_valid");
		return false;
	}

	if (!bt_addr_le_cmp(btif_get_le_addr_by_handle(handle),
	    &context->remote_ble_addr)) {
		SYS_LOG_INF("match");
		return true;
	}
#endif
	SYS_LOG_INF("dismatch");

	return false;
}

uint16_t bt_manager_find_active_slave_handle(void)
{
	struct bt_audio_conn *audio_conn = NULL;

	audio_conn = find_active_slave();
	if (!audio_conn) {
		return 0;
	}

	return audio_conn->handle;
}

bool bt_manager_check_audio_conn_is_same_dev(bd_address_t *addr, uint8_t type)
{
	struct bt_audio_conn *p_audio_conn;

	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, p_audio_conn, node) {
			if(!p_audio_conn->is_phone_dev){
				continue;
			}
		if(!memcmp(addr,&p_audio_conn->addr,sizeof(bd_address_t))){
			if(type != p_audio_conn->type){
				return true;
			}
		}
	}
	return false;
}

struct bt_audio_conn *bt_manager_get_dev_another_audio_conn(bd_address_t *addr, uint8_t type)
{
	struct bt_audio_conn *p_audio_conn = NULL;

	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, p_audio_conn, node) {
		if (p_audio_conn->role != BT_ROLE_SLAVE) {
			continue;
		}

		if(!memcmp(addr,&p_audio_conn->addr,sizeof(bd_address_t))){
			if(type != p_audio_conn->type){
				return p_audio_conn;
			}
		}
	}
	return p_audio_conn;
}

uint32_t bt_manager_audio_get_active_channel_iso_interval(void)
{
	uint32_t iso_interval = 0;
	struct bt_audio_channel * chan = find_active_channel(get_active_slave());
	if (chan) {
		iso_interval = chan->iso_interval;
	}
	SYS_LOG_INF("val:%d\n", iso_interval);
	return iso_interval;
}

static int bt_manager_get_inactive_audio_conn(uint16_t *hdl, uint8_t *is_disconnecting)
{
	bt_mgr_dev_info_t* a2dp_dev = NULL;
	struct bt_audio_conn *audio_conn = NULL;
	struct bt_audio_conn *active_conn = NULL;
	struct bt_audio_conn *another_audio_conn = NULL;

	SYS_LOG_INF("active_slave: %p, %p\r\n", bt_audio.active_slave, bt_audio.inactive_slave);

	os_sched_lock();

	//Check if there are device being disconnected
	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
		if ((audio_conn->role == BT_ROLE_SLAVE) && (audio_conn->type == BT_TYPE_BR)) {
			a2dp_dev = bt_mgr_find_dev_info_by_hdl(audio_conn->handle);
			SYS_LOG_INF("%p, %x, %x, %x\n", audio_conn, audio_conn->handle,
						audio_conn->security_changed, audio_conn->type);
			if ((a2dp_dev) && (a2dp_dev->deemed_disconnected)) {
				//check lea
				if (bt_manager_audio_is_lea_open()) {
					another_audio_conn = bt_manager_get_dev_another_audio_conn(&audio_conn->addr, audio_conn->type);
					if ((another_audio_conn) &&
						(another_audio_conn == bt_audio.active_slave) &&
						(another_audio_conn != bt_audio.inactive_slave)) {
							continue;
					}
				}

				//disconnecting
				*hdl = audio_conn->handle;
				*is_disconnecting = 1;
				goto done;
			}
		}
	}

	if (bt_audio.inactive_slave) {
		*hdl = bt_audio.inactive_slave->handle;
		*is_disconnecting = 0;
	}else {
		bt_audio.inactive_slave = NULL;
		if (bt_audio.active_slave) {
			active_conn = bt_audio.active_slave;
		} else {
			SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
				if (audio_conn->role == BT_ROLE_SLAVE) {
					if ((audio_conn->type == BT_TYPE_LE) ||
						((audio_conn->type == BT_TYPE_BR) && (audio_conn->security_changed))) {
						active_conn = audio_conn;
					}
				}
			}
		}

		if(!active_conn)
			goto done;

		SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
			if(!audio_conn->is_phone_dev){
				continue;
			}

			if(memcmp(&active_conn->addr,&audio_conn->addr,sizeof(bd_address_t))){
				SYS_LOG_INF("Need disconnect: 0x%x\n", audio_conn->handle);
				*hdl = audio_conn->handle;
				*is_disconnecting = 0;
				goto done;
			}
		}
	}

done:
	os_sched_unlock();
	return 0;
}

int bt_manager_disconnect_lea_audio_conn(void)
{
	struct bt_audio_conn *audio_conn = NULL;

	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
		if((!audio_conn->is_phone_dev) || (!audio_conn->is_lea)){
			continue;
		}

		SYS_LOG_INF("disconnect: 0x%x\n", audio_conn->handle);
		bt_manager_audio_conn_disconnect(audio_conn->handle);
	}

	return 0;
}

int bt_manager_disconnect_inactive_audio_conn(void)
{
	bt_mgr_dev_info_t* a2dp_dev = NULL;
	struct bt_audio_conn *audio_conn = NULL;
	struct bt_audio_conn *active_conn = NULL;

	SYS_LOG_INF("active_slave: %p, %p\r\n", bt_audio.active_slave, bt_audio.inactive_slave);

	if(bt_audio.device_num <= 1)
		return 0;

	os_sched_lock();

	//Check if there are device being disconnected
	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {\
		if (audio_conn->role == BT_ROLE_SLAVE) {
			a2dp_dev = bt_mgr_find_dev_info_by_hdl(audio_conn->handle);
			SYS_LOG_INF("%x, %x", audio_conn->handle, a2dp_dev->deemed_disconnected);
			if ((a2dp_dev) && (a2dp_dev->deemed_disconnected)) {
				goto done;
			}
		}
	}

	if (bt_audio.inactive_slave) {
		if (BT_TYPE_BR == bt_audio.inactive_slave->type) {
			bt_manager_ble_disconnect_by_addr(&bt_audio.inactive_slave->addr);
		}
		bt_manager_audio_conn_disconnect(bt_audio.inactive_slave->handle);
	}else {
		bt_audio.inactive_slave = NULL;
		if (bt_audio.active_slave) {
			active_conn = bt_audio.active_slave;
		} else {
			SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
				if ((audio_conn->role == BT_ROLE_SLAVE) && (audio_conn->security_changed)) {
					active_conn = audio_conn;
				}
			}
		}

		if(!active_conn)
			goto done;

		SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
			if(!audio_conn->is_phone_dev){
				continue;
			}

			if(memcmp(&active_conn->addr,&audio_conn->addr,sizeof(bd_address_t))){
				if (BT_TYPE_BR == audio_conn->type) {
					bt_manager_ble_disconnect_by_addr(&audio_conn->addr);
				}
				SYS_LOG_INF("disconnect: 0x%x\n", audio_conn->handle);
				bt_manager_audio_conn_disconnect(audio_conn->handle);
			}
		}
	}

done:
	os_sched_unlock();
	return 0;

}

int bt_manager_disconnect_other_audio_conn(uint8_t *addr, bool only_phone_dev)
{
    struct bt_audio_conn *audio_conn = NULL;

    os_sched_lock();

    SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
		if(only_phone_dev){
			if(!audio_conn->is_phone_dev){
				continue;
			}
		}

		if(addr && !memcmp(addr, &audio_conn->addr, sizeof(bd_address_t))) {
			continue;
		} 
		else {
			SYS_LOG_INF("other: 0x%x %d\n",
			            audio_conn->handle,
			            audio_conn->addr_type);
			bt_manager_audio_conn_disconnect(audio_conn->handle);
			//disconnecting S25, but phone is still showing connection(ble doesn't disconnect)
			if (BT_TYPE_BR == audio_conn->type) {
				bt_manager_ble_disconnect_by_addr(&audio_conn->addr);
			}
		}
	}
	os_sched_unlock();
	return 0;
}

int bt_manager_audio_switch_multi_point(int enable)
{
	os_sched_lock();
	bt_audio.multi_point_disable = !enable;
	if (!enable) {
		if (bt_manager_audio_is_lea_open()) {
			check_audio_conn_preemption_pre_dev(NULL, 0, 0);
		}else {
			bt_manager_disconnect_inactive_audio_conn();
		}
		bt_manager_lea_policy_event_notify(LEA_POLICY_EVENT_SWITCH_SINGLE_POINT, NULL, 0);
	}else{
		bt_manager_lea_policy_event_notify(LEA_POLICY_EVENT_SWITCH_MULTI_POINT, NULL, 0);
	}
	os_sched_unlock();
	SYS_LOG_INF("enable:%d\n", enable);
	return 0;
}

bool bt_manager_audio_is_multi_point(void)
{
    return bt_audio.multi_point_disable ? false : true;
}

static int check_audio_conn_num(bd_address_t* new_dev_addr,uint16_t handle, int type){
#if 1
	int max_num = bt_manager_config_connect_phone_num();
	int match = 0;

	struct bt_audio_conn *audio_conn;

	os_sched_lock();

	if(bt_audio.multi_point_disable){
		max_num = 1;
		SYS_LOG_INF("set max num to 1 for auracast");
	}

	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
		if(!audio_conn->is_phone_dev){
			continue;
		}

		if(!memcmp(new_dev_addr,&audio_conn->addr,sizeof(bd_address_t))){
			if(audio_conn->type != type){
				match = 1;
			} else {
				SYS_LOG_ERR("type: %d\n", type);
				os_sched_unlock();
				if (audio_conn->handle != handle) {
					SYS_LOG_ERR("hdl: %x(%x)\n", handle, audio_conn->handle);
					bt_manager_audio_conn_disconnect(handle);
				}
				return -EINVAL;
			}
		}
	}

	os_sched_unlock();

	if(!match){
		if(bt_audio.device_num + 1 > max_num){
			SYS_LOG_ERR("max conn: %d", max_num);
			bt_manager_audio_conn_disconnect(handle);
			return -EINVAL;
		} else {
			bt_audio.device_num += 1;
		}
	}
#endif
	return 0;
}

static int le_set_cis_connected(uint16_t handle, uint8_t connected)
{
	struct bt_audio_conn *audio_conn;

	audio_conn = find_conn(handle);
	if (!audio_conn) {
		SYS_LOG_ERR("not found\n");
		return -ENODEV;
	}

	os_sched_lock();
	if(connected){
		audio_conn->cis_connected++;
	}else if(audio_conn->cis_connected){
		audio_conn->cis_connected--;
	}
	os_sched_unlock();

	SYS_LOG_INF("%p, hdl:0x%x, connected %d state:%d\n", audio_conn, handle, connected,audio_conn->cis_connected);

	return 0;
}

#if 1 /* preemption for device */
int check_audio_conn_preemption_pre_dev(bd_address_t* new_dev_addr, uint16_t handle, int type)
{
	struct bt_audio_conn *audio_conn;
	dev_record_t dev_rec[8];
	uint16_t exp_handle[6], diconnect_cnt, i, j;
	uint8_t rec_cnt, active_hdl, dev_cnt, repeat;
	char addr[13];

	if (new_dev_addr) {
		if ((type != BT_TYPE_BR) && (type != BT_TYPE_LE)) {
			return 0;
		}
	}

	memset(exp_handle, 0, sizeof(exp_handle));
	memset(dev_rec, 0xff, sizeof(dev_rec));
	diconnect_cnt = 0;
	rec_cnt = 0;
	active_hdl = 0;
	i = j = 0;
	repeat = dev_cnt = 0;

	os_sched_lock();

	/* Record all hdl and addr */
	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
		if (!audio_conn->is_phone_dev) {
			continue;
		}

		dev_rec[rec_cnt].handle = audio_conn->handle;
		memcpy(&dev_rec[rec_cnt].addr, &audio_conn->addr, sizeof(bd_address_t));
		rec_cnt++;
	}

	if (new_dev_addr) {
		dev_rec[rec_cnt].handle = handle;
		memcpy(&dev_rec[rec_cnt].addr, new_dev_addr, sizeof(bd_address_t));
		rec_cnt++;
	}

	//cal dev cnt
	for (i = 0; i < (rec_cnt-1); i++) {
		for(j = i+1; j < rec_cnt; j++) {
			if (memcmp(&dev_rec[i].addr, &dev_rec[j].addr, sizeof(bd_address_t)) == 0) {
				dev_rec[i].associated = j;
				dev_rec[j].associated = i;
				repeat++;
			}
		}
	}
	dev_cnt = rec_cnt -repeat;
	if (repeat >= rec_cnt) {
		SYS_LOG_ERR("dev_cnt err %d %d\n", rec_cnt, repeat);
	}

	for(i=0; i<rec_cnt; i++) {
		hex_to_str(addr, (char*)&dev_rec[i].addr, 6);
		SYS_LOG_INF("dev_rec[%d]: %s, 0x%x, %d (%d, %d)\n", i, addr, dev_rec[i].handle, dev_cnt, rec_cnt, repeat);
		if(dev_rec[i].associated != 0xff) {
			int idx = dev_rec[i].associated;
			hex_to_str(addr, (char*)&dev_rec[idx].addr, 6);
			SYS_LOG_INF("dev_associated[%d]: %s, 0x%x\n", i, addr, dev_rec[idx].handle);
		}
	}

	if (dev_cnt >= bt_manager_config_connect_phone_num()) {
		if (dev_cnt > 1) {
#if (CONFIG_BT_DOUBLE_PHONE_TAKEOVER_POLICY == 1)
		#if 0
			uint16_t conn_type;
			for (i=0; i<rec_cnt; i++) {
				conn_type = btif_get_type_by_handle(dev_rec[i].handle);
				//hdl and associated hdl doesn't play
				if (((BT_TYPE_LE == conn_type) && (!le_get_cis_connected(dev_rec[i].handle)) &&\
					 ((dev_rec[i].associated == 0xff) || !btif_a2dp_stream_is_open(dev_rec[dev_rec[i].associated].handle))) ||
					((BT_TYPE_BR == conn_type) && (!btif_a2dp_stream_is_open(dev_rec[i].handle)) &&\
					 ((dev_rec[i].associated == 0xff) || !le_get_cis_connected(dev_rec[dev_rec[i].associated].handle))))
				{
					exp_handle[diconnect_cnt++] = dev_rec[i].handle;
					SYS_LOG_INF("T4 kick handle: 0x%x\n", dev_rec[i].handle);
					if(dev_rec[i].associated != 0xff) {
						int idx = dev_rec[i].associated;
						exp_handle[diconnect_cnt++] = dev_rec[idx].handle;
						SYS_LOG_INF("T4_1 kick handle: 0x%x\n", dev_rec[idx].handle);
					}
					break;
				}
			}
		#else
			uint16_t hdl = 0;
			uint8_t is_disconnecting = 0;
			bt_manager_get_inactive_audio_conn(&hdl, &is_disconnecting);
			SYS_LOG_INF("T4_pre: %x, %x\n", hdl, is_disconnecting);
			if (hdl) {
				for (i=0; i<rec_cnt; i++) {
					if (dev_rec[i].handle == hdl) {
						if (!is_disconnecting) {
							exp_handle[diconnect_cnt++] = dev_rec[i].handle;
							SYS_LOG_INF("T4 kick handle: 0x%x\n", dev_rec[i].handle);
						}

						if(dev_rec[i].associated != 0xff) {
							int idx = dev_rec[i].associated;
							exp_handle[diconnect_cnt++] = dev_rec[idx].handle;
							SYS_LOG_INF("T4_1 kick handle: 0x%x\n", dev_rec[idx].handle);
						}
						break;
					}
				}
				if (i >= rec_cnt) {
					SYS_LOG_ERR("Find device fail %d %d\n", hdl, is_disconnecting);
				}
			}
		#endif
#else
			//todo
#endif
		} else if (dev_cnt == 1) {
			/* Single phone mode support kick the connected acl */
			exp_handle[diconnect_cnt++] = dev_rec[0].handle;
			SYS_LOG_INF("T5 kick handle: 0x%x\n", dev_rec[0].handle);
			if(dev_rec[0].associated != 0xff) {
				int idx = dev_rec[0].associated;
				exp_handle[diconnect_cnt++] = dev_rec[idx].handle;
				SYS_LOG_INF("T5_1 kick handle: 0x%x\n", dev_rec[idx].handle);
			}
		}
	}
	os_sched_unlock();

	if ((diconnect_cnt > sizeof(exp_handle)) || (rec_cnt > (sizeof(dev_rec)/sizeof(dev_record_t)))) {
		SYS_LOG_ERR("Over memory %d %d\n", diconnect_cnt, rec_cnt);
	}

	for (i=0; i<diconnect_cnt; i++) {
		SYS_LOG_INF("T6 kick handle: 0x%x\n", exp_handle[i]);

		if ((BT_TYPE_LE == btif_get_type_by_handle(exp_handle[i])) &&
			(bt_audio.take_over_flag != 1)) {
			bt_audio.take_over_flag = 1;
			SYS_LOG_INF("take_over:%d \n",bt_audio.take_over_flag);
		}

		if (BT_TYPE_BR == btif_get_type_by_handle(exp_handle[i])) {
			bt_manager_ble_disconnect_by_addr(btif_get_addr_by_handle(exp_handle[i]));
		}

		/* This interface direct call to stack, not the better way */
		bt_manager_audio_conn_disconnect(exp_handle[i]);
	}

	if (new_dev_addr) {
		SYS_EVENT_INF(EVENT_BT_TAKE_OVER, handle, UINT8_ARRAY_TO_INT32(new_dev_addr->val),
						exp_handle[0], exp_handle[1]);
	}

	return 0;
}
#else /* preemption for link */
static int check_audio_conn_preemption_pre_link(bd_address_t* new_dev_addr, uint16_t handle, int type)
{
	struct bt_audio_conn *audio_conn;
	dev_record_t dev_rec[8];
	uint16_t exp_handle[6], diconnect_cnt;
#if (CONFIG_BT_DOUBLE_PHONE_TAKEOVER_POLICY == 1)
	uint16_t conn_type;
#endif
	uint8_t rec_cnt, active_hdl, i;
	char addr[13];

	if ((type != BT_TYPE_BR) && (type != BT_TYPE_LE)) {
		return 0;
	}

	memset(exp_handle, 0, sizeof(exp_handle));
	memset(dev_rec, 0xff, sizeof(dev_rec));
	diconnect_cnt = 0;
	rec_cnt = 0;
	active_hdl = 0;

	os_sched_lock();

	/* Record all hdl and addr */
	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
		if (!audio_conn->is_phone_dev) {
			continue;
		}

		dev_rec[rec_cnt].handle = audio_conn->handle;
		memcpy(&dev_rec[rec_cnt].addr, &audio_conn->addr, sizeof(bd_address_t));
		rec_cnt++;
	}

	dev_rec[rec_cnt].handle = handle;
	memcpy(&dev_rec[rec_cnt].addr, new_dev_addr, sizeof(bd_address_t));
	rec_cnt++;

	for(i=0; i<rec_cnt; i++) {
		hex_to_str(addr, (char*)&dev_rec[i].addr, 6);
		SYS_LOG_INF("dev_rec[%d]: %s, 0x%x, %d\n", i, addr, dev_rec[i].handle, rec_cnt);
	}

	if (rec_cnt >= bt_manager_config_connect_phone_num()) {
#if (CONFIG_BT_DOUBLE_PHONE_TAKEOVER_POLICY == 1)
		for (i=0; i<rec_cnt; i++) {
			conn_type = btif_get_type_by_handle(dev_rec[i].handle);
			//hdl doesn't play
			if (((BT_TYPE_LE == conn_type) && (!le_get_cis_connected(dev_rec[i].handle))) ||
				((BT_TYPE_BR == conn_type) && (!btif_a2dp_stream_is_open(dev_rec[i].handle))))
			{
				exp_handle[diconnect_cnt++] = dev_rec[i].handle;
				SYS_LOG_INF("T4 kick handle: 0x%x\n", dev_rec[i].handle);
				break;
			}
		}
#else
		//todo
#endif
	}
	os_sched_unlock();

	if ((diconnect_cnt > sizeof(exp_handle)) || (rec_cnt > (sizeof(dev_rec)/sizeof(dev_record_t)))) {
		SYS_LOG_ERR("Over memory %d %d\n", diconnect_cnt, rec_cnt);
	}

	for (i=0; i<diconnect_cnt; i++) {
		SYS_LOG_INF("T6 kick handle: 0x%x\n", exp_handle[i]);

		if ((BT_TYPE_LE == btif_get_type_by_handle(exp_handle[i])) &&
			(bt_audio.take_over_flag != 1)) {
			bt_audio.take_over_flag = 1;
			SYS_LOG_INF("take_over:%d \n",bt_audio.take_over_flag);
		}

		/* This interface direct call to stack, not the better way */
		bt_manager_audio_conn_disconnect(exp_handle[i]);
	}
	SYS_EVENT_INF(EVENT_BT_TAKE_OVER, handle, UINT8_ARRAY_TO_INT32(new_dev_addr->val),
					exp_handle[0], exp_handle[1]);
	return 0;
}
#endif /* preemption for device or link */

static int bt_manager_device_is_stoped(uint16_t handle)
{
	struct bt_audio_conn *audio_conn = find_conn(handle);
	if (!audio_conn) {
		SYS_LOG_ERR("0x%x isn't exist.", handle);
		return 1;
	}

	if (audio_conn->type == BT_TYPE_BR) {
		bt_mgr_dev_info_t* a2dp_dev = bt_mgr_find_dev_info_by_hdl(handle);

	    if(!a2dp_dev){
	        return 1;
	    }

		if(!btif_a2dp_stream_is_open(handle)) {
			return 1;
		}
		return 0;
	}else if (audio_conn->type == BT_TYPE_LE){
		struct bt_audio_channel *active_channel = find_active_channel(audio_conn);
		if(!active_channel){
			return 1;
		}
		return 0;
	}else{
		return 1;
	}
}

int security_audio_conn(uint16_t handle)
{
	struct bt_audio_conn *audio_conn;

	audio_conn = find_conn(handle);
	if (!audio_conn) {
		SYS_LOG_ERR("0x%x isn't exist.", handle);
		return -EINVAL;
	}

	audio_conn->security_changed = 1;
	if(audio_conn->type == BT_TYPE_BR) {
		audio_conn->connect_time = os_uptime_get_32();
		SYS_LOG_INF("security_changed<%d>\n", audio_conn->connect_time);

		/*Fix active device when switching from LEA to BR (LEA stays connected)
		 *eg: pixel7,SamsungS25Ultra_V15
		 */
		if (bt_audio.active_slave &&
			bt_manager_audio_has_same_lea_notify_connected(&(bt_audio.active_slave->addr))) {
			bt_audio.active_slave = NULL;
		}
	}

	if ((audio_conn->is_phone_dev) &&
		(bt_audio.active_slave) &&
		(bt_audio.device_num > 1) &&
		(bt_manager_device_is_stoped(bt_audio.active_slave->handle))&&
		(!bt_audio.active_slave->cis_connected)) {

		bt_audio.inactive_slave = bt_audio.active_slave;
	}
	SYS_LOG_INF("active_slave: %p, %p, %p, %d, %d, %d, %d\r\n", bt_audio.active_slave, bt_audio.inactive_slave,
				audio_conn, audio_conn->is_phone_dev, bt_audio.device_num, bt_audio.active_slave->cis_connected,
				bt_manager_device_is_stoped(bt_audio.active_slave->handle));

	return 0;
}

int audio_conn_key_miss(uint16_t handle)
{
	struct bt_audio_conn *audio_conn;

	os_sched_lock();
	audio_conn = find_conn(handle);
	if (!audio_conn) {
		os_sched_unlock();
		SYS_LOG_ERR("0x%x isn't exist.", handle);
		return -EINVAL;
	}

	audio_conn->key_miss = 1;
	os_sched_unlock();

	return 0;
}

void cis_disconn_check_delaywork(os_work *work)
{	
    struct bt_audio_conn *audio_conn;

	audio_conn =
		CONTAINER_OF(work, struct bt_audio_conn, cis_disconncet_work);

	if (!audio_conn) {
		SYS_LOG_ERR("not found\n");
		return;
	}
    SYS_LOG_INF("dis lea acl:0x%x,0x%x\n",audio_conn->handle,audio_conn->cis_connected);
	if (audio_conn->cis_connected) {
		//disconn lea
		bt_manager_audio_conn_disconnect(audio_conn->handle);
		audio_conn->cis_disconn_err = 1;
	}
}

int new_audio_conn(uint16_t handle)
{
	btmgr_feature_cfg_t * cfg =  bt_manager_get_feature_config();
	struct bt_mgr_dev_info * info = NULL;
	struct bt_audio_conn *audio_conn;
	int type = 0;
	int role;
	uint8_t le_tws = 0,is_phone_dev = 1;
	char addr[13];

	audio_conn = find_conn(handle);
	if (audio_conn) {
		SYS_LOG_ERR("0x%x alreay exist.", handle);
		return 0;
	}

	type = btif_get_type_by_handle(handle);
	if(type == BT_TYPE_SCO || type == BT_TYPE_SCO_SNOOP) {
		SYS_LOG_ERR("type: %d", type);
		return -EINVAL;
	} else if (type == BT_TYPE_BR_SNOOP){
		type = BT_TYPE_BR;
	}

	if (type == BT_TYPE_BR) {
		role = BT_ROLE_SLAVE;
		info = bt_mgr_find_dev_info_by_hdl(handle);
		if(!info || info->is_tws){
			SYS_LOG_ERR("tws: 0x%x", handle);
			return -EINVAL;
		}
	} else if (type == BT_TYPE_LE) {
		role = btif_get_conn_role(handle);
		is_phone_dev = 0;

		/*if (role == BT_ROLE_SLAVE && is_le_tws(handle))*/
		if (is_le_tws(handle))
		{
			le_tws = 1;
			SYS_LOG_INF("le tws dev:%d \n",role);
		}
		bt_manager_audio_set_scan_policy(2, 3);
	} else {
		SYS_LOG_ERR("type: %d", type);
		return -EINVAL;
	}
	bd_address_t* dev_addr = btif_get_addr_by_handle(handle);
	if (!dev_addr) {
		SYS_LOG_ERR("invalid handle 0x%x\n",handle);
		return -EINVAL;
	}

	if(is_phone_dev){
		if(check_audio_conn_num(dev_addr,handle,type)){
			return -EINVAL;
		}
		if (bt_audio.device_num <= 1) {
			bt_audio.inactive_slave = NULL;
		}
	}

	if (cfg->sp_phone_takeover && is_phone_dev) {
		if (check_audio_conn_preemption_pre_dev(dev_addr, handle, type)) {
			return -EINVAL;
		}
	}

	audio_conn = mem_malloc(sizeof(struct bt_audio_conn));
	if (!audio_conn) {
		SYS_LOG_ERR("no mem");
		return -ENOMEM;
	}
	memset(audio_conn,0,sizeof(struct bt_audio_conn));

	audio_conn->handle = handle;
	audio_conn->type = type;
	audio_conn->role = role;
	audio_conn->le_tws = le_tws;
	audio_conn->is_phone_dev = is_phone_dev;
	audio_conn->media_state = BT_STATUS_PAUSED;
	audio_conn->call_state = BT_STATUS_HFP_NONE;
	memcpy(&audio_conn->addr, dev_addr, sizeof(bd_address_t));
	memset(addr, 0, 13);
	hex_to_str(addr, (char*)&audio_conn->addr, 6);
	if(type == BT_TYPE_BR && info && info->name){
		audio_conn->dev_name = mem_malloc(strlen(info->name) + 1);
		if (!audio_conn->dev_name) {
			SYS_LOG_ERR("malloc failed!!\n");
		}else{
			memset(audio_conn->dev_name,0,strlen(info->name) + 1);
			memcpy(audio_conn->dev_name,info->name,strlen(info->name));
		}
	}

	os_sched_lock();
	sys_slist_append(&bt_audio.conn_list, &audio_conn->node);
	os_sched_unlock();

	os_delayed_work_init(&audio_conn->cis_disconncet_work, cis_disconn_check_delaywork);
	
	if (type == BT_TYPE_BR) {
		/* notify for update BT scan */
		bt_manager_lea_policy_event_notify(LEA_POLICY_EVENT_CONNECT, NULL, 0);
	}

	SYS_LOG_INF("%p, h:%d, t:%d, r:%d a:%s n:%d p:%d\n", audio_conn,
		handle, type, role, addr, bt_audio.device_num,audio_conn->is_phone_dev);

#if 0
	/* filter non active_slave */
	if ((audio_conn->role == BT_ROLE_SLAVE) && get_active_slave()) {
		return 0;
	}
#endif

	return bt_manager_event_notify(BT_CONNECTED, &handle, sizeof(handle));


}

int bt_manager_audio_new_audio_conn_for_test(uint16_t handle,int type,int role)
{
	struct bt_audio_conn *audio_conn;

	audio_conn = mem_malloc(sizeof(struct bt_audio_conn));
	if (!audio_conn) {
		SYS_LOG_ERR("no mem");
		return -ENOMEM;
	}

	audio_conn->handle = handle;
	audio_conn->type = type;
	audio_conn->role = role;

	os_sched_lock();
	sys_slist_append(&bt_audio.conn_list, &audio_conn->node);
	os_sched_unlock();

	SYS_LOG_DBG("handle: %d, type: %d, role: %d", handle, type, role);

	return 0;
}

void delete_audio_conn2(uint16_t handle, uint8_t reason, struct bt_audio_conn *audio_conn)
{
	uint8_t is_lea = 0;
	uint8_t param[10] = {0};
	uint8_t param_len = 0;
	struct bt_audio_conn *p_audio_conn;

	if(audio_conn->type == BT_TYPE_LE && !audio_conn->le_tws) {
		is_lea = 1;
        if(audio_conn->is_phone_dev){
		    bt_manager_lea_set_status(BT_STATUS_DISCONNECTED, 1, NULL);
		}
		else{
		    bt_manager_lea_set_status(BT_STATUS_DISCONNECTED, 0, NULL);
		}
		bt_addr_le_t le_addr = {0};
		le_addr.type = audio_conn->addr_type;
		memcpy(le_addr.a.val, audio_conn->addr.val, sizeof(le_addr.a.val));
		param[0] = reason;
		memcpy(&param[1], (uint8_t *)&le_addr, sizeof(bt_addr_le_t));
		param_len = 1 + sizeof(bt_addr_le_t);

		if ((audio_conn->cis_disconn_err) && (reason == 0x16)) {
			param[0] = 0x8;
			SYS_LOG_INF("CIS abnormal active disconnection \n");
		}
	}

#ifdef CONFIG_BT_LETWS
	if (audio_conn->le_tws) {
	   bt_mamager_letws_disconnected(handle,audio_conn->role,reason);
	}
#endif

	mem_free(audio_conn);

	/*notify after free audio conn*/
	if (is_lea) {
		bt_manager_lea_policy_event_notify(LEA_POLICY_EVENT_DISCONNECT, param, param_len);
	} else {
		bt_manager_lea_policy_event_notify(LEA_POLICY_EVENT_DISCONNECT, NULL, 0);
	}

	int has_le_dev = 0;
	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, p_audio_conn, node) {
		if(p_audio_conn->type == BT_TYPE_LE){
			has_le_dev = 1;
		}
	}
	if(!has_le_dev){
		bt_manager_audio_set_scan_policy(2, 0);
	}

	return;
}

int delete_audio_conn(uint16_t handle, uint8_t reason)
{
	struct bt_audio_conn *audio_conn;
	struct bt_audio_channel *chan, *ch;
	struct bt_audio_call_inst *call;
	struct bt_audio_call_inst *tmp;
	uint8_t report = 1;

	audio_conn = find_conn(handle);
	if (!audio_conn) {
		SYS_LOG_ERR("not found");
		return -ENODEV;
	}
    os_delayed_work_cancel(&audio_conn->cis_disconncet_work);

	os_sched_lock();

	/* remove all channel */
	SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&audio_conn->chan_list, chan, ch, node) {
		if (chan->period_tl) {
			timeline_release(chan->period_tl);
		}
		sys_slist_remove(&audio_conn->chan_list, NULL, &chan->node);
		mem_free(chan);
	}

	if (audio_conn->caps) {
		mem_free(audio_conn->caps);
	}

	if (audio_conn->endps) {
		mem_free(audio_conn->endps);
	}

	if (audio_conn->dev_name) {
		mem_free(audio_conn->dev_name);
	}
	/* NOTICE: need to free media/vol/mic if any */

	/* call list */
	if (is_bt_le_master(audio_conn)) {
		if (bt_le_master_count() == 0) {
			SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&bt_audio.cg_list, call,
							tmp, node) {
				sys_slist_find_and_remove(&bt_audio.cg_list, &call->node);
				mem_free(call);
			}
		}
	} else if (audio_conn->role == BT_ROLE_SLAVE) {
		SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&audio_conn->ct_list, call,
						tmp, node) {
			sys_slist_find_and_remove(&audio_conn->ct_list, &call->node);
			mem_free(call);
		}
	}

	sys_slist_find_and_remove(&bt_audio.conn_list, &audio_conn->node);

	if(audio_conn->is_phone_dev
		&& !bt_manager_check_audio_conn_is_same_dev(&audio_conn->addr,audio_conn->type))
		bt_audio.device_num--;

	if (audio_conn->role == BT_ROLE_SLAVE) {
		/* filter non active_slave */
		if (get_active_slave() && (audio_conn != get_active_slave())) {
			report = 0;
		}

		/* NOTICE: should call until audio_conn has been removed from list */
		update_active_slave(audio_conn, true);
	}

	if ((audio_conn == bt_audio.inactive_slave) || (bt_audio.device_num <= 1)) {
		bt_audio.inactive_slave = NULL;
	}

#ifdef CONFIG_BT_LETWS
	if (audio_conn->le_tws) {
	   report = 0;
	}
#endif

	if (bt_manager_audio_is_lea_open()) {
		if ((reason != BT_HCI_ERR_CONN_ALREADY_EXISTS) && (audio_conn->is_phone_dev)) {
			if ((audio_conn->type == BT_TYPE_LE) ||
				((audio_conn->type == BT_TYPE_BR) && (audio_conn->security_changed))) {
				int ret = add_dev_to_disconnect_info(&audio_conn->addr, audio_conn->addr_type,
													audio_conn->type, 0, 0);
				if (ret >= 0) {
					update_disconnect_info_to_nvram();
				}
			}
		}
	}

	delete_audio_conn2(handle, reason, audio_conn);

	os_sched_unlock();

	SYS_LOG_INF("%p, handle: %d %d, reason:0x%x", audio_conn, handle,bt_audio.device_num, reason);

	/* no need to report */
	if (report == 0) {
		return 0;
	}

	return bt_manager_event_notify(BT_DISCONNECTED, &handle, sizeof(handle));
}

int bt_manager_audio_delete_audio_conn_for_test(uint16_t handle)
{
	delete_audio_conn(handle,BT_HCI_ERR_REMOTE_USER_TERM_CONN);

	return 0;
}

int tws_audio_conn(uint16_t handle)
{
	struct bt_audio_conn *audio_conn;

	audio_conn = find_conn(handle);
	if (!audio_conn) {
		SYS_LOG_ERR("not found");
		return -ENODEV;
	}

	os_sched_lock();
	if(audio_conn->type == BT_TYPE_LE){
		audio_conn->le_tws= 1;
		SYS_LOG_INF("letws %p, handle: %d", audio_conn, handle);
#ifdef CONFIG_BT_LETWS
		os_sched_unlock();
		return bt_mamager_letws_connected(handle);
#endif
	}else if(audio_conn->type == BT_TYPE_BR){
		audio_conn->is_br_tws = 1;
		SYS_LOG_INF("brtws %p, handle: %d", audio_conn, handle);
	}
	os_sched_unlock();

	return 0;
}

int64_t bt_manager_audio_get_current_time(void *tl){
	if(tl){
		struct bt_audio_channel *chan = (struct bt_audio_channel *)timeline_get_owner(tl);
		if(chan && chan->audio_handle)
			return (int64_t)bt_manager_audio_get_le_time(chan->audio_handle);
		else
			return 0;
	} else
		return -1;
}

static void bt_get_remote_lea_name_cb(uint16_t handle, uint8_t err, const void *data, uint16_t length){
	struct bt_audio_conn *audio_conn = find_conn(handle);
	if (!audio_conn || err) {
		SYS_LOG_ERR("not found %d\n",err);
		return;
	}
	if(length > CONFIG_MAX_REMOTE_BT_NAME_LEN){
		SYS_LOG_WRN("exceed max length %d\n",length);
	}

	int name_len = MIN(CONFIG_MAX_REMOTE_BT_NAME_LEN, length);
	audio_conn->dev_name = mem_malloc(name_len + 1);
	if (!audio_conn->dev_name) {
		SYS_LOG_ERR("malloc failed!!\n");
	}else{
		memset(audio_conn->dev_name,0,name_len + 1);
		memcpy(audio_conn->dev_name,data,name_len);
	}
	SYS_LOG_INF("lea name %d\n",strlen(audio_conn->dev_name));
}

static bool bt_manager_audio_is_same_br_connected(bd_address_t* addr)
{
	struct bt_audio_conn *p_audio_conn;
	struct bt_mgr_dev_info *dev_info;

	os_sched_lock();
	if (addr) {
		SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, p_audio_conn, node) {
			if ((!p_audio_conn->is_phone_dev) || (p_audio_conn->type != BT_TYPE_BR)) {
				continue;
			}
			dev_info = bt_mgr_find_dev_info_by_hdl(p_audio_conn->handle);
			if ((dev_info) && (dev_info->a2dp_connected) &&
				(!memcmp(addr->val,&p_audio_conn->addr,sizeof(bd_address_t)))) {
				SYS_LOG_INF("same br connected\n");
				os_sched_unlock();
				return true;
			}
		}
	}
	os_sched_unlock();

	return false;
}

int ascs_connceted(uint16_t handle)
{
	struct bt_audio_conn *audio_conn;
	btmgr_feature_cfg_t * cfg =  bt_manager_get_feature_config();
	bt_addr_le_t *le_addr = NULL;
	uint8_t param_len = 0;
	uint8_t first_connect = 0;
	uint8_t tmp_flag = 0;

    if (!bt_manager_config_lea_pts_test()){
        if (bt_manager_audio_is_lea_open() &&
            !bt_manager_lea_is_connectable()) {
                bt_manager_audio_conn_disconnect(handle);
                SYS_LOG_ERR("lea is unconnetable, active disconnect!\n");
                return -EACCES;
        }
    }

	first_connect = 0;
	audio_conn = find_conn(handle);
	if (!audio_conn) {
		SYS_LOG_ERR("not found\n");
		return -ENODEV;
	}
	if (audio_conn->le_tws) {
        SYS_LOG_ERR("is le tws:");
		return -EINVAL;
	}

	bd_address_t* dev_addr = btif_get_addr_by_handle(handle);
	if (!dev_addr) {
		SYS_LOG_ERR("invalid handle 0x%x\n",handle);
		return -EINVAL;
	}

	int type = btif_get_type_by_handle(handle);

	if(check_audio_conn_num(dev_addr,handle,type)){
		return -EINVAL;
	}

	if ((bt_audio.active_slave) &&
		(bt_audio.device_num > 1) &&
		(bt_manager_device_is_stoped(bt_audio.active_slave->handle))&&
		(!bt_audio.active_slave->cis_connected)) {

		bt_audio.inactive_slave = bt_audio.active_slave;
	}
	SYS_LOG_INF("active_slave: %p, %p, %p, %d, %d, %d, %d\r\n", bt_audio.active_slave, bt_audio.inactive_slave,
				audio_conn, audio_conn->is_phone_dev, bt_audio.device_num, bt_audio.active_slave->cis_connected,
				bt_manager_device_is_stoped(bt_audio.active_slave->handle));

	if (cfg->sp_phone_takeover) {
		check_audio_conn_preemption_pre_dev(dev_addr, handle, type);
	}

	le_addr = btif_get_le_addr_by_handle(handle);
	if (le_addr) {
		param_len = sizeof(*le_addr);
	} else {
		SYS_LOG_ERR("hdl err:0x%x\n",handle);
		return -EINVAL;
	}

	os_sched_lock();
	audio_conn->is_lea = 1;
	audio_conn->is_phone_dev = 1;
	audio_conn->addr_type = le_addr->type;
	audio_conn->media_state = BT_STATUS_INACTIVE;
	audio_conn->ascs_connect_time = os_uptime_get_32();
#if 0
	struct bt_audio_conn *p_audio_conn;
	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, p_audio_conn, node) {
		if(!p_audio_conn->is_phone_dev){
			continue;
		}
		if(!memcmp(dev_addr->val,&p_audio_conn->addr,sizeof(bd_address_t))){
			if(type != p_audio_conn->type && p_audio_conn->key_miss){
				first_connect = 1;
				p_audio_conn->key_miss = 0;
				SYS_LOG_INF("same dev, no tts\n");
			}
		}
	}
#endif
	os_sched_unlock();

	if (
#if defined(CONFIG_BT_CROSS_TRANSPORT_KEY)
	!first_connect &&
	!bt_manager_lea_is_reconnected_dev(dev_addr->val)
#else
	!bt_manager_lea_is_reconnected_dev(dev_addr->val)
#endif
		) {
		tmp_flag = 1;
	}

	btif_audio_get_remote_name(handle,bt_get_remote_lea_name_cb);

	bt_manager_lea_set_status(BT_STATUS_CONNECTED, tmp_flag, dev_addr);
	bt_manager_lea_policy_event_notify(LEA_POLICY_EVENT_CONNECT, (void *)le_addr, param_len);

	if (bt_manager_audio_is_lea_open()) {
		if (bt_manager_audio_is_same_br_connected(dev_addr) == false) {
			bt_manager_check_phone_connected();
		}else {
			//lea and br connected at the same time
		}
	}else {
		bt_manager_check_phone_connected();
	}

	SYS_LOG_INF("ascs<%p, hdl: %d, %d>\n", audio_conn, handle, audio_conn->ascs_connect_time);

	return 0;
}

int le_cis_connceted(void *data, int size)
{
	struct bt_audio_channel * channel;
	struct bt_audio_cis_param *param = (struct bt_audio_cis_param *)data;
	struct bt_audio_conn *audio_conn;
	struct bt_audio_channel * active_channel;
	int type;

	audio_conn = find_conn(param->handle);
	if (!audio_conn) {
		SYS_LOG_ERR("not found\n");
		return -ENODEV;
	}

	channel = find_channel(param->handle,param->id);
	if (!channel) {
		SYS_LOG_ERR("channel not found\n");
		return -ENODEV;
	}

	active_channel = find_active_channel(audio_conn);

	os_sched_lock();
	channel->audio_handle = param->audio_handle;
	channel->sync_delay = param->sync_delay;
	channel->iso_interval = param->iso_interval;
	channel->cis_connected = 1;

	/* bond channel before create timeline */
	if (active_channel && active_channel != channel) {
		channel->bond_channel = active_channel;
	}

	if(channel->bond_channel && channel->bond_channel->period_tl){
		timeline_ref(channel->bond_channel->period_tl);
		channel->period_tl = channel->bond_channel->period_tl;
	}else{
		if (audio_conn->role == BT_ROLE_MASTER) {
			if (channel->dir == BT_AUDIO_DIR_SOURCE) {
				type = TIMELINE_TYPE_MEDIA_DECODE;
			} else {
				type = TIMELINE_TYPE_MEDIA_ENCODE;
			}
		} else {
			if (channel->dir == BT_AUDIO_DIR_SINK) {
				type = TIMELINE_TYPE_MEDIA_DECODE;
			} else {
				type = TIMELINE_TYPE_MEDIA_ENCODE;
			}
		}

		channel->period_tl = timeline_create_by_owner(type, channel->interval,channel,bt_manager_audio_get_current_time);
		if (!channel->period_tl) {
			os_sched_unlock();
			SYS_LOG_ERR("no mem for period_tl\n");
			return -ENOMEM;
		}
		timeline_start(channel->period_tl);
		audio_conn->tl_owner_chan = channel;
	}

	os_sched_unlock();

	le_set_cis_connected(param->handle, 1);

    /*
      CIS reconnection
    */
	if ((audio_conn->cis_connected == 2)) {
        
		os_delayed_work_cancel(&audio_conn->cis_disconncet_work);
		audio_conn->cis_disconn_err = 0;   
	}

	SYS_LOG_INF("hdl:0x%x,cis_hdl:0x%x, sync delay:%d,iso_interval:%d\n",param->handle,
		channel->audio_handle, channel->sync_delay, channel->iso_interval);

	bt_manager_event_notify(BT_AUDIO_CONNECTED, data, size);

	return 0;
}

int le_cis_disconnceted(void *data, int size)
{
	struct bt_audio_report *rep = (struct bt_audio_report *)data;
	struct bt_audio_channel * channel;
	struct bt_audio_conn *audio_conn;

	audio_conn = find_conn(rep->handle);
	if (!audio_conn) {
		SYS_LOG_ERR("not found\n");
		return -ENODEV;
	}

	channel = find_channel(rep->handle,rep->id);
	if (!channel) {
		SYS_LOG_ERR("channel not found\n");
		return -ENODEV;
	}

	os_sched_lock();
	channel->cis_connected = 0;

	if (channel->period_tl) {
		timeline_release(channel->period_tl);
		channel->period_tl = NULL;
	}

	if (audio_conn->tl_owner_chan == channel) {
		audio_conn->tl_owner_chan = NULL;
	}

	os_sched_unlock();

    SYS_LOG_INF("cis disconn reason:0x%x \n",rep->reason);
	if ((audio_conn->cis_connected == 2) &&
	    (rep->reason == 0x08))  {

		os_delayed_work_submit(&audio_conn->cis_disconncet_work, LEA_CIS_DICONN_CHECK_TIME);
	}
    
	le_set_cis_connected(rep->handle, 0);
	bt_manager_event_notify(BT_AUDIO_DISCONNECTED, data, size);

    /*
	 CIS all disconnected
	*/
	if ((audio_conn->cis_connected == 0)) {

        os_delayed_work_cancel(&audio_conn->cis_disconncet_work);
		audio_conn->cis_disconn_err = 0;
	}

	if (channel->state == BT_AUDIO_STREAM_RELEASING) {
		bt_manager_audio_stream_event(BT_AUDIO_STREAM_RELEASE, data, size);
	}
	return 0;
}

void *bt_manager_audio_get_tl_owner_chan(uint16_t handle)
{
	struct bt_audio_conn *audio_conn;

	audio_conn = find_conn(handle);
	if (!audio_conn) {
		SYS_LOG_ERR("not found\n");
		return NULL;
	}

	return audio_conn->tl_owner_chan;
}

void bt_manager_audio_conn_event(int event, void *data, int size)
{
	uint16_t handle;

	SYS_LOG_INF("%d", event);

	switch (event) {
	case BT_CONNECTED:
		handle = *(uint16_t *)data;
		new_audio_conn(handle);
		break;
	case BT_DISCONNECTED:
	    {
		  struct bt_disconn_report* rep = (struct bt_disconn_report*) data;
		  delete_audio_conn(rep->handle, rep->reason);
		}
		break;
	case BT_TWS_CONNECTION_EVENT:
		handle = *(uint16_t *)data;
		tws_audio_conn(handle);
		break;
	case BT_AUDIO_CONNECTED:
		le_cis_connceted(data, size);
		break;
	case BT_AUDIO_DISCONNECTED:
		le_cis_disconnceted(data, size);
		break;
	case BT_AUDIO_ASCS_CONNECTION:
		handle = *(uint16_t *)data;
		ascs_connceted(handle);
		break;
	case BT_AUDIO_ASCS_DISCONNECTION:
		break;
	case BT_SECURITY_CHANGED:
		handle = *(uint16_t *)data;
		security_audio_conn(handle);
		break;
	case BT_KEY_MISS:
		handle = *(uint16_t *)data;
		audio_conn_key_miss(handle);
		break;
	default:
		break;
	}
}

uint8_t bt_manager_audio_get_type(uint16_t handle)
{
	struct bt_audio_conn *audio_conn;

	audio_conn = find_conn(handle);
	if (!audio_conn) {
		SYS_LOG_ERR("not found");
		return 0;
	}

	return audio_conn->type;
}

int bt_manager_audio_get_cur_dev_num(void)
{
	return bt_audio.device_num;
}

bool bt_manager_audio_has_phone_encrypted(void)
{
	struct bt_audio_conn *audio_conn = NULL;

	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
		if (audio_conn->is_phone_dev) {
			if ((audio_conn->type == BT_TYPE_LE) ||
				((audio_conn->type == BT_TYPE_BR) && (audio_conn->security_changed))) {
				return true;
			}
		}
	}

	return false;
}

bool bt_manager_audio_has_same_lea_notify_connected(bd_address_t* addr)
{
	struct bt_audio_conn *p_audio_conn;

	os_sched_lock();
	if ((addr) && (bt_manager_audio_is_lea_open())) {
		SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, p_audio_conn, node) {
			if((!p_audio_conn->is_phone_dev) || (!p_audio_conn->is_lea)){
				continue;
			}
			if(!memcmp(addr->val,&p_audio_conn->addr,sizeof(bd_address_t))) {
				os_sched_unlock();
				return true;
			}
		}
	}
	os_sched_unlock();

	return false;
}

bool bt_manager_audio_has_same_br_notify_connected(bd_address_t* addr)
{
	struct bt_audio_conn *p_audio_conn;
	struct bt_mgr_dev_info *dev_info;

	os_sched_lock();
	if (addr) {
		SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, p_audio_conn, node) {
			if((!p_audio_conn->is_phone_dev) || (p_audio_conn->type != BT_TYPE_BR)) {
				continue;
			}
			dev_info = bt_mgr_find_dev_info_by_hdl(p_audio_conn->handle);
			if((dev_info) && (dev_info->notify_connected) &&
				(!memcmp(addr->val,&p_audio_conn->addr,sizeof(bd_address_t)))) {
				SYS_LOG_INF("same dev, no tts\n");
				os_sched_unlock();
				return true;
			}
		}
	}
	os_sched_unlock();

	return false;
}

bool bt_manager_audio_is_ios_pc(uint16_t handle)
{
    return btif_br_is_ios_pc(handle);
}

bool bt_manager_audio_is_ios_dev(uint16_t handle)
{
    return btif_br_is_ios_dev(handle);
}

int bt_manager_audio_get_role(uint16_t handle)
{
	struct bt_audio_conn *audio_conn;

	audio_conn = find_conn(handle);
	if (!audio_conn) {
		SYS_LOG_ERR("no conn");
		return -ENODEV;
	}

	return audio_conn->role;
}

uint16_t bt_manager_audio_get_letws_handle()
{
	struct bt_audio_conn *audio_conn = NULL;

	os_sched_lock();

	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
		if (audio_conn->le_tws) {
			os_sched_unlock();
			return audio_conn->handle;
		}
	}

	os_sched_unlock();

	return 0;
}

bool bt_manager_audio_actived(void)
{
	if (sys_slist_is_empty(&bt_audio.conn_list)) {
		return false;
	}

	return true;
}

int bt_manager_audio_get_leaudio_dev_num()
{
	struct bt_audio_conn *audio_conn = NULL;
	int num = 0;

	os_sched_lock();
	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
		if (audio_conn->is_lea){
			num++;
		}
	}
	os_sched_unlock();

	return num;
}

int bt_manager_audio_disconnect_all_phone_dev()
{
	struct bt_audio_conn *audio_conn = NULL;

	bt_manager_br_disconnect_all_phone_device();

	os_sched_lock();
	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
		if (audio_conn->is_lea){
			bt_manager_audio_conn_disconnect(audio_conn->handle);
		}
	}
	os_sched_unlock();

	int timeout = 0;
	while (bt_manager_audio_get_leaudio_dev_num() > 0 && timeout++ < 100)
	{
		os_sleep(10);
	}

	return 0;
}

uint8_t bt_manager_audio_sync_remote_active_app(uint8_t active_app, bd_address_t* active_dev_addr, bd_address_t* interrupt_dev_addr)
{
#if 0
	struct bt_audio_conn *new_active_slave = NULL;
	bd_address_t tmp_addr;

	bt_audio.remote_active_app = active_app;

	if (active_dev_addr){
		new_active_slave = find_conn_by_addr(active_dev_addr);
		if (!new_active_slave){
			new_active_slave = bt_audio.active_slave;
		}
		memcpy(&(bt_audio.tws_slave_active_dev), active_dev_addr, sizeof(bd_address_t));
	}

	if (interrupt_dev_addr){
		memset(&tmp_addr,0,sizeof(bd_address_t));
		if(!memcmp(interrupt_dev_addr, &tmp_addr, sizeof(bd_address_t))){
			bt_audio.interrupted_active_slave = NULL;
		}else{
			bt_audio.interrupted_active_slave = find_conn_by_addr(interrupt_dev_addr);
		}
		memcpy(&(bt_audio.tws_slave_interrupted_dev), interrupt_dev_addr, sizeof(bd_address_t));

		SYS_LOG_INF("interrupted dev: 0x%x",(bt_audio.interrupted_active_slave? bt_audio.interrupted_active_slave->handle:0));
	}

	if (new_active_slave){
		SYS_LOG_INF("%d, 0x%x",bt_audio.remote_active_app, new_active_slave->handle);
		bt_audio.active_slave = new_active_slave;
		switch_slave_app(bt_audio.active_slave);
	}
#endif
	return 0;
}

int bt_manager_audio_stream_dir(uint16_t handle, uint8_t id)
{
	struct bt_audio_channel *chan;

	chan = find_channel(handle, id);
	if (!chan) {
		SYS_LOG_ERR("no chan");
		return -ENODEV;
	}

	SYS_LOG_INF("%d", chan->dir);

	return chan->dir;
}

int bt_manager_audio_stream_state(uint16_t handle, uint8_t id)
{
	struct bt_audio_channel *chan;

	chan = find_channel(handle, id);
	if (!chan) {
		SYS_LOG_ERR("no chan");
		return -ENODEV;
	}

	SYS_LOG_INF("%d %d %d\n", handle, id, chan->state);

	return chan->state;
}

int bt_manager_audio_stream_info(struct bt_audio_chan *chan,
				struct bt_audio_chan_info *info)
{
	struct bt_audio_channel *channel;

	if (!chan || !info) {
		return -EINVAL;
	}

	channel = find_channel(chan->handle, chan->id);
	if (!channel) {
		SYS_LOG_ERR("no chan");
		return -ENODEV;
	}

	os_sched_lock();

	info->format = channel->format;
	info->channels = channel->channels;
	info->octets = channel->octets;
	info->sample = channel->sample;
	info->interval = channel->interval;
	info->kbps = channel->kbps;
	info->sdu = channel->sdu;
	info->locations = channel->locations;
	info->contexts = channel->audio_contexts;

	info->target_latency = channel->target_latency;
	info->duration = channel->duration;
	info->max_latency = channel->max_latency;
	info->delay = channel->delay;
	info->rtn = channel->rtn;
	info->phy = channel->phy;
	info->framing = channel->framing;
	info->chan = channel;

	os_sched_unlock();

	return 0;
}

struct bt_audio_endpoints *bt_manager_audio_client_get_ep(uint16_t handle)
{
	struct bt_audio_conn *audio_conn;

	audio_conn = find_conn(handle);
	if (!audio_conn) {
		return NULL;
	}

	if ((audio_conn->type != BT_TYPE_LE) ||
	    (audio_conn->role != BT_ROLE_MASTER)) {
		SYS_LOG_ERR("invalid %d %d", audio_conn->role,
			audio_conn->type);
		return NULL;
	}

	return audio_conn->endps;
}

struct bt_audio_capabilities *bt_manager_audio_client_get_cap(uint16_t handle)
{
	struct bt_audio_conn *audio_conn;

	audio_conn = find_conn(handle);
	if (!audio_conn) {
		return NULL;
	}

	if ((audio_conn->type != BT_TYPE_LE) ||
		(audio_conn->role != BT_ROLE_MASTER)) {
		SYS_LOG_ERR("invalid %d %d %d", handle, audio_conn->role,
			audio_conn->type);
		return NULL;
	}

	return audio_conn->caps;
}

static int stream_prefer_qos_default(uint16_t handle, uint8_t id, uint8_t dir,
				uint8_t target_latency)
{
	struct bt_audio_preferred_qos_default *default_qos;
	struct bt_audio_preferred_qos qos;

	if (dir == BT_AUDIO_DIR_SOURCE) {
		if (!bt_audio.le_audio_source_prefered_qos) {
			return 0;
		}
		default_qos = &bt_audio.source_qos;
	} else if (dir == BT_AUDIO_DIR_SINK) {
		if (!bt_audio.le_audio_sink_prefered_qos) {
			return 0;
		}
		default_qos = &bt_audio.sink_qos;
	} else {
		return 0;
	}

	qos.handle = handle;
	qos.id = id;
	qos.framing = default_qos->framing;
	qos.phy = default_qos->phy;
	qos.rtn = default_qos->rtn;
	qos.latency = default_qos->latency;
	qos.delay_min = default_qos->delay_min;
	qos.delay_max = default_qos->delay_max;
	qos.preferred_delay_min = default_qos->preferred_delay_min;
	qos.preferred_delay_max = default_qos->preferred_delay_max;

	return bt_manager_le_audio_stream_prefer_qos(&qos);
}

static struct bt_audio_channel *new_channel(struct bt_audio_conn *audio_conn)
{
	struct bt_audio_channel *chan;

	chan = mem_malloc(sizeof(struct bt_audio_channel));
	if (!chan) {
		SYS_LOG_ERR("no mem");
		return NULL;
	}

	os_sched_lock();
	sys_slist_append(&audio_conn->chan_list, &chan->node);
	os_sched_unlock();

	return chan;
}

static int stream_config_codec(void *data, int size)
{
	struct bt_audio_conn *audio_conn;
	struct bt_audio_channel *chan;
	struct bt_audio_codec *codec;

	codec = (struct bt_audio_codec *)data;

	audio_conn = find_conn(codec->handle);
	if (!audio_conn) {
		SYS_LOG_ERR("no conn");
		return -ENODEV;
	}

	chan = find_channel2(audio_conn, codec->id);
	if (chan) {	/* update codec */
		SYS_LOG_DBG("update");
	} else {	/* new codec */
		chan = new_channel(audio_conn);
		if (!chan) {
			SYS_LOG_ERR("no mem");
			return -ENOMEM;
		}
	}

	chan->id = codec->id;
	chan->dir = codec->dir;
	chan->format = codec->format;
	chan->channels = bt_channels_by_locations(codec->locations);
	/* NOTICE: some masters may not set locations when config_codec */
	if (chan->channels == 0) {
		chan->channels = 1;
	}
	chan->duration = bt_frame_duration_to_ms(codec->frame_duration);

	/* sdu & interval: for master used to config qos */
	chan->sdu = chan->channels * codec->blocks * codec->octets;
	if (chan->duration == 7) {
		chan->interval = 7500;
	} else {
		chan->interval = chan->duration * 1000;
	}
	chan->interval *= codec->blocks;

	chan->sample = codec->sample_rate;
	chan->octets = codec->octets;
	chan->locations = codec->locations;
	chan->target_latency = codec->target_latency;

	SYS_LOG_INF("id %d dir %d format %d channels %d duration %d",
			chan->id, chan->dir, chan->format, chan->channels, chan->duration);
	SYS_LOG_INF("sdu %d interval %d sample %d octets %d locations %d",
			chan->sdu, chan->interval, chan->sample, chan->octets, chan->locations);

	SYS_LOG_INF("target_latency %d", codec->target_latency);

	/* if initiated by master, no need to report message */
	if (audio_conn->role == BT_ROLE_MASTER) {
		return 0;
	}

	chan->state = BT_AUDIO_STREAM_CODEC_CONFIGURED;

	/* LE Slave */
	if (audio_conn->type == BT_TYPE_LE) {
		stream_prefer_qos_default(codec->handle, codec->id,
					codec->dir, codec->target_latency);

		/*Get mcs state*/
		if (BT_STATUS_INACTIVE == audio_conn->media_state) {
			bt_manager_le_media_mcs_state_get(codec->handle);
		}
	}

	return bt_manager_audio_event_notify(NULL,BT_AUDIO_STREAM_CONFIG_CODEC, data, size);
}

static int stream_prefer_qos(void *data, int size)
{
	struct bt_audio_preferred_qos *qos;
	struct bt_audio_channel *chan;
	struct bt_audio_conn *audio_conn;

	qos = (struct bt_audio_preferred_qos *)data;

	audio_conn = find_conn(qos->handle);
	if (!audio_conn) {
		SYS_LOG_ERR("no conn");
		return -ENODEV;
	}

	chan = find_channel(qos->handle, qos->id);
	if (!chan) {
		SYS_LOG_ERR("no chan");
		return -ENODEV;
	}

	chan->state = BT_AUDIO_STREAM_CODEC_CONFIGURED;

	return bt_manager_audio_event_notify(NULL, BT_AUDIO_STREAM_PREFER_QOS, data, size);
}

static int stream_config_qos(void *data, int size)
{
	struct bt_audio_qos_configured *qos;
	struct bt_audio_channel *chan;
	struct bt_audio_conn *audio_conn;

	qos = (struct bt_audio_qos_configured *)data;

	audio_conn = find_conn(qos->handle);
	if (!audio_conn) {
		SYS_LOG_ERR("no conn");
		return -ENODEV;
	}

	chan = find_channel(qos->handle, qos->id);
	if (!chan) {
		SYS_LOG_ERR("no chan");
		return -ENODEV;
	}

	chan->framing = qos->framing;
	chan->interval = qos->interval;
	chan->delay = qos->delay;
	chan->max_latency= qos->latency;
	if(chan->octets == qos->max_sdu && chan->channels != 1){
		chan->channels = 1;
		SYS_LOG_INF("fix for sony phone\n");
	}
	chan->sdu = qos->max_sdu;
	chan->phy = qos->phy;
	chan->rtn = qos->rtn;
	chan->kbps = get_kbps(chan->sample, chan->sdu, chan->interval, chan->channels);

	SYS_LOG_INF("framing %d interval %d delay %d max_latency %d sdu %d phy %d rtn %d kbps %d\n",
			chan->framing, chan->interval, chan->delay, chan->max_latency, chan->sdu,
			chan->phy, chan->rtn, chan->kbps);

	chan->state = BT_AUDIO_STREAM_QOS_CONFIGURED;

	return bt_manager_audio_event_notify(NULL, BT_AUDIO_STREAM_CONFIG_QOS, data, size);
}

static int stream_fake(void *data, int size)
{
	struct bt_audio_report *rep = (struct bt_audio_report *)data;
	struct bt_audio_conn *audio_conn;

	audio_conn = find_conn(rep->handle);
	if (!audio_conn) {
		SYS_LOG_ERR("no conn");
		return -ENODEV;
	}

	audio_conn->fake_contexts = rep->audio_contexts;

	SYS_LOG_INF("contexts: 0x%x", audio_conn->fake_contexts);

	update_slave_prio(audio_conn);

	return 0;
}

static int stream_enable(void *data, int size)
{
	struct bt_audio_report *rep = (struct bt_audio_report *)data;
	struct bt_audio_conn *audio_conn;
	struct bt_audio_channel *chan;

	audio_conn = find_conn(rep->handle);
	if (!audio_conn) {
		SYS_LOG_ERR("no conn");
		return -ENODEV;
	}

	chan = find_channel2(audio_conn, rep->id);
	if (!chan) {
		SYS_LOG_ERR("no chan");
		return -ENODEV;
	}

	chan->audio_contexts = rep->audio_contexts;
	chan->state = BT_AUDIO_STREAM_ENABLED;

	SYS_LOG_INF("contexts:0x%x,id:%d\n", chan->audio_contexts,chan->id);

	int dir = bt_manager_audio_stream_dir(rep->handle, rep->id);
	/* Need start right now for sink ase, so that ascs procedure can be completed for phone.*/
	if (dir == BT_AUDIO_DIR_SINK) {
		audio_conn->active_sink_chan.handle = rep->handle;
		audio_conn->active_sink_chan.id = rep->id;
		/*Need global variables*/
		bt_manager_audio_sink_stream_start_single(&audio_conn->active_sink_chan);
	}

	update_slave_prio(audio_conn);

	if ((audio_conn == bt_audio.active_slave) &&
		(bt_audio.active_slave == bt_audio.inactive_slave)) {
		bt_audio.inactive_slave = NULL;
	}
	SYS_LOG_INF("active_slave: %p, %p\r\n", bt_audio.active_slave, bt_audio.inactive_slave);

	return bt_manager_audio_event_notify(audio_conn, BT_AUDIO_STREAM_ENABLE, data, size);
}

static int stream_update(void *data, int size)
{
	struct bt_audio_report *rep = (struct bt_audio_report *)data;
	struct bt_audio_conn *audio_conn;
	struct bt_audio_channel *chan;

	audio_conn = find_conn(rep->handle);
	if (!audio_conn) {
		SYS_LOG_ERR("no conn");
		return -ENODEV;
	}

	chan = find_channel2(audio_conn, rep->id);
	if (!chan) {
		SYS_LOG_ERR("no chan");
		return -ENODEV;
	}

	chan->audio_contexts = rep->audio_contexts;

	SYS_LOG_INF("contexts:0x%x,id:%d\n", chan->audio_contexts, chan->id);

	update_slave_prio(audio_conn);

	return bt_manager_audio_event_notify(audio_conn,BT_AUDIO_STREAM_UPDATE, data, size);
}

static int stream_disable(void *data, int size)
{
	struct bt_audio_report *rep = (struct bt_audio_report *)data;
	struct bt_audio_channel *chan;
	struct bt_audio_conn *audio_conn;

	audio_conn = find_conn(rep->handle);
	if (!audio_conn) {
		SYS_LOG_ERR("no conn");
		return -ENODEV;
	}

	chan = find_channel(rep->handle, rep->id);
	if (!chan) {
		SYS_LOG_ERR("no chan");
		return -ENODEV;
	}

	chan->state = BT_AUDIO_STREAM_DISABLED;

	/* NOTICE: reset audio_contexts until stream_stop */

	return bt_manager_audio_event_notify(audio_conn, BT_AUDIO_STREAM_DISABLE, data, size);
}

static int stream_release(void *data, int size)
{
	struct bt_audio_report *rep = (struct bt_audio_report *)data;
	struct bt_audio_conn *audio_conn;
	struct bt_audio_channel *chan;
	static struct bt_audio_chan tmp;
	uint8_t released = 0;
	uint8_t reported = 0;

	audio_conn = find_conn(rep->handle);
	if (!audio_conn) {
		SYS_LOG_ERR("no conn");
		return -ENODEV;
	}

	chan = find_channel2(audio_conn, rep->id);
	if (!chan) {
		SYS_LOG_ERR("no chan");
		return -ENODEV;
	}

	os_sched_lock();

	if (chan->cis_connected) {
		chan->state = BT_AUDIO_STREAM_RELEASING;
		os_sched_unlock();
		SYS_LOG_INF("wait cis disconnect\n");
		bt_manager_audio_event_notify(audio_conn, BT_AUDIO_STREAM_RELEASE, data, size);
		return 0;
	}

	if (BT_AUDIO_STREAM_RELEASING == chan->state) {
		reported = 1;
	}

	chan->state = BT_AUDIO_STREAM_IDLE;

	/* add released operation for LE Slave only */
	if(audio_conn->type == BT_TYPE_LE){
		tmp.handle = audio_conn->handle;
		tmp.id = chan->id;
		if(audio_conn->role == BT_ROLE_SLAVE)
			released = 1;
		//TO DO:free chann later??
		int dir = bt_manager_audio_stream_dir(audio_conn->handle,chan->id);
		if(dir == BT_AUDIO_DIR_SINK){
			bt_manager_audio_sink_stream_set(&tmp, NULL);
		}else{
			bt_manager_audio_source_stream_set_single(&tmp,
					NULL, 0);
		}
	}else if (audio_conn->type == BT_TYPE_BR) {
		tmp.handle = audio_conn->handle;
		tmp.id = chan->id;
		bt_manager_audio_sink_stream_set(&tmp, NULL);
	}

	/* BT_AUDIO_STREAM_IDLE */
	sys_slist_find_and_remove(&audio_conn->chan_list, &chan->node);
	mem_free(chan);

	os_sched_unlock();

	if (!reported) {
		bt_manager_audio_event_notify(audio_conn, BT_AUDIO_STREAM_RELEASE, data, size);
	}

	update_slave_prio(audio_conn);

	/* released at last to avoid race condition */
	if (released == 1) {
		bt_manager_le_audio_stream_released(&tmp);
	}

	return 0;
}

static int stream_start(void *data, int size)
{
	struct bt_audio_report *rep = (struct bt_audio_report *)data;
	struct bt_audio_channel *chan;
	struct bt_audio_conn *audio_conn;

	audio_conn = find_conn(rep->handle);
	if (!audio_conn) {
		SYS_LOG_ERR("no conn");
		return -ENODEV;
	}

	chan = find_channel(rep->handle, rep->id);
	if (!chan) {
		SYS_LOG_ERR("no chan");
		return -ENODEV;
	}

	chan->state = BT_AUDIO_STREAM_STARTED;

#ifdef CONFIG_BT_PLAY_FIRST_STOP_RECONNECT
	bt_manager_auto_reconnect_stop();
#else
	btif_br_auto_reconnect_connection_cancel_device();
#endif

	return bt_manager_audio_event_notify(audio_conn, BT_AUDIO_STREAM_START, data, size);
}

static int stream_stop(void *data, int size)
{
	struct bt_audio_report *rep = (struct bt_audio_report *)data;
	struct bt_audio_conn *audio_conn;
	struct bt_audio_channel *chan;
	int ret;

	audio_conn = find_conn(rep->handle);
	if (!audio_conn) {
		SYS_LOG_ERR("no conn");
		return -ENODEV;
	}

	chan = find_channel2(audio_conn, rep->id);
	if (!chan) {
		SYS_LOG_ERR("no chan");
		return -ENODEV;
	}

	chan->state = BT_AUDIO_STREAM_QOS_CONFIGURED;
	/* reset audio_contexts */
	chan->audio_contexts = 0;

#ifdef CONFIG_BT_HFP_HF
	if ((rep->audio_contexts == BT_AUDIO_CONTEXT_CONVERSATIONAL) 
		&& (bt_manager_hfp_get_status() <= BT_STATUS_HFP_NONE)) {
		audio_conn->fake_contexts = 0;
	}
#else
	if (rep->audio_contexts == BT_AUDIO_CONTEXT_CONVERSATIONAL) {
		audio_conn->fake_contexts = 0;
	}
#endif
	ret = bt_manager_audio_event_notify(audio_conn,BT_AUDIO_STREAM_STOP, data, size);

	update_slave_prio(audio_conn);

	return ret;
}

static int stream_discover_cap(void *data, int size)
{
	struct bt_audio_conn *audio_conn;
	struct bt_audio_capabilities *caps;

	caps = (struct bt_audio_capabilities *)data;

	audio_conn = find_conn(caps->handle);
	if (!audio_conn) {
		SYS_LOG_ERR("no conn");
		return -ENODEV;
	}

	audio_conn->caps = mem_malloc(size);
	if (!audio_conn->caps) {
		SYS_LOG_ERR("no mem");
		return -ENOMEM;
	}

	memcpy(audio_conn->caps, caps, size);

	return bt_manager_audio_event_notify(audio_conn, BT_AUDIO_DISCOVER_CAPABILITY, data, size);
}

static int stream_discover_endp(void *data, int size)
{
	struct bt_audio_conn *audio_conn;
	struct bt_audio_endpoints *endps;

	endps = (struct bt_audio_endpoints *)data;

	audio_conn = find_conn(endps->handle);
	if (!audio_conn) {
		SYS_LOG_ERR("no conn");
		return -ENODEV;
	}

	audio_conn->endps = mem_malloc(size);
	if (!audio_conn->endps) {
		SYS_LOG_ERR("no mem");
		return -ENOMEM;
	}

	memcpy(audio_conn->endps, endps, size);

	return bt_manager_audio_event_notify(audio_conn, BT_AUDIO_DISCOVER_ENDPOINTS, data, size);
}

void bt_manager_audio_stream_event(int event, void *data, int size)
{
	SYS_LOG_INF("%d", event);

	switch (event) {
	case BT_AUDIO_STREAM_CONFIG_CODEC:
		stream_config_codec(data, size);
		break;
	case BT_AUDIO_STREAM_PREFER_QOS:
		stream_prefer_qos(data, size);
		break;
	case BT_AUDIO_STREAM_CONFIG_QOS:
		stream_config_qos(data, size);
		break;
	case BT_AUDIO_STREAM_ENABLE:
		stream_enable(data, size);
		break;
	case BT_AUDIO_STREAM_UPDATE:
		stream_update(data, size);
		break;
	case BT_AUDIO_STREAM_START:
		stream_start(data, size);
		break;
	case BT_AUDIO_STREAM_STOP:
		stream_stop(data, size);
		break;
	case BT_AUDIO_STREAM_DISABLE:
		stream_disable(data, size);
		break;
	case BT_AUDIO_STREAM_RELEASE:
		stream_release(data, size);
		break;
	case BT_AUDIO_DISCOVER_CAPABILITY:
		stream_discover_cap(data, size);
		break;
	case BT_AUDIO_DISCOVER_ENDPOINTS:
		stream_discover_endp(data, size);
		break;
	case BT_AUDIO_STREAM_FAKE:
		stream_fake(data, size);
		break;
	default:
		bt_manager_event_notify(event, data, size);
		break;
	}
}

int bt_manager_audio_get_audio_handle(uint16_t handle,uint8_t id)
{
	struct bt_audio_channel *chan;

	chan = find_channel(handle,id);
	if (!chan) {
		SYS_LOG_ERR("no chan\n");
		return -ENODEV;
	}

	return chan->audio_handle;
}

__ramfunc uint64_t bt_manager_audio_get_le_time(uint16_t handle)
{
	struct le_link_time time = {0, 0, 0};

	ctrl_get_le_link_time(handle, &time);

#if 0
	printk("time %d %llu %d %d\n", handle, (uint64_t)time.ce_cnt, time.ce_interval_us, time.ce_offs_us);
#endif

	return (uint64_t)((int64_t)time.ce_cnt * time.ce_interval_us + time.ce_offs_us);
}

__ramfunc static void stream_trigger(uint16_t handle, uint8_t id, uint16_t audio_handle)
{
	struct bt_audio_channel *chan;

	chan = find_channel(handle, id);
	if (!chan) {
		return;
	}

	chan->audio_handle = audio_handle;
	if (chan->trigger) {
		chan->trigger(handle, id, audio_handle);
	}
}

__ramfunc static void stream_period_trigger(uint16_t handle, uint8_t id,
				uint16_t audio_handle)
{
	struct bt_audio_channel *chan;

	chan = find_channel(handle, id);
	if (!chan) {
		return;
	}

	chan->audio_handle = audio_handle;

	if(chan->period_tl){
		timeline_trigger_listener(chan->period_tl);
	}
}

__ramfunc static void stream_tws_sync_trigger(uint16_t handle, uint8_t id,
				uint16_t audio_handle)
{
	struct bt_audio_channel *chan;

	chan = find_channel(handle, id);
	if (!chan) {
		return;
	}

	if (chan->tws_sync_trigger) {
		chan->tws_sync_trigger(handle, id, audio_handle);
	}
}

static const struct bt_audio_stream_cb stream_cb = {
	.tx_trigger = stream_trigger,
	.tx_period_trigger = stream_period_trigger,
	.rx_trigger = stream_trigger,
	.rx_period_trigger = stream_period_trigger,
	.tws_sync_trigger = stream_tws_sync_trigger,
};

static struct bt_audio_conn *find_le_slave(void)
{
	struct bt_audio_conn *audio_conn;

	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
		if ((audio_conn->type == BT_TYPE_LE) &&
		    (audio_conn->role == BT_ROLE_SLAVE)) {
			return audio_conn;
		}
	}

	return NULL;
}

struct bt_audio_channel *find_le_slave_chan(uint8_t dir)
{
	struct bt_audio_conn *audio_conn;
	struct bt_audio_channel *chan;

	audio_conn = find_le_slave();
	if (!audio_conn) {
		return NULL;
	}

	SYS_SLIST_FOR_EACH_CONTAINER(&audio_conn->chan_list, chan, node) {
		if (chan->dir == dir && (chan->state == BT_AUDIO_STREAM_ENABLED
			|| chan->state == BT_AUDIO_STREAM_STARTED)) {
			return chan;
		}
	}

	return NULL;
}

static inline struct bt_audio_conn *find_le_master(void)
{
	struct bt_audio_conn *audio_conn;

	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
		if ((audio_conn->type == BT_TYPE_LE) &&
		    (audio_conn->role == BT_ROLE_MASTER)) {
			return audio_conn;
		}
	}

	return NULL;
}

static struct bt_audio_channel *find_le_master_chan(uint8_t dir)
{
	struct bt_audio_conn *audio_conn;
	struct bt_audio_channel *chan;

	audio_conn = find_le_master();
	if (!audio_conn) {
		return NULL;
	}

	SYS_SLIST_FOR_EACH_CONTAINER(&audio_conn->chan_list, chan, node) {
		if (chan->dir == dir) {
			return chan;
		}
	}

	return NULL;
}

extern uint8_t _cis_fine_adjust_ap(uint16_t cis_handle,int8_t adj_ap_us);
void bt_manager_audio_stream_tx_adjust_clk(int8_t adjust_ap_us)
{
	struct bt_audio_channel *chan = find_le_master_chan(BT_AUDIO_DIR_SINK);

	if (!chan) {
		return;
	}

	int ret = _cis_fine_adjust_ap(chan->audio_handle,adjust_ap_us);
	if(ret)
		SYS_LOG_INF("%d\n",ret);
}

int bt_manager_audio_stream_cb_register(struct bt_audio_chan *chan,
				bt_audio_trigger_cb trigger,
				bt_audio_trigger_cb period_trigger)
{
	struct bt_audio_conn *audio_conn;
	struct bt_audio_channel *channel;

	if (!chan) {
		return -EINVAL;
	}

	if (!trigger) {
		return -EINVAL;
	}

	audio_conn = find_conn(chan->handle);
	if (!audio_conn) {
		return -EINVAL;
	}

	if (audio_conn->type != BT_TYPE_LE) {
		return -EINVAL;
	}

	channel = find_channel2(audio_conn, chan->id);
	if (!channel) {
		return -EINVAL;
	}

	if (channel->trigger) {
		return -EINVAL;
	}

	os_sched_lock();
	channel->trigger = trigger;
	os_sched_unlock();

	SYS_LOG_INF("chan %d %d\n",chan->handle,chan->id);

	return 0;
}

int bt_manager_audio_stream_cb_bind(struct bt_audio_chan *bond_chan,
				struct bt_audio_chan *new_chan,
				bt_audio_trigger_cb trigger,
				bt_audio_trigger_cb period_trigger)
{
	struct bt_audio_channel *bond_channel;
	struct bt_audio_channel *new_channel;
	struct bt_audio_conn *bond_conn;
	struct bt_audio_conn *new_conn;

	if (!bond_chan || !new_chan) {
		return -EINVAL;
	}

	bond_conn = find_conn(bond_chan->handle);
	if (!bond_conn) {
		return -ENODEV;
	}

	new_conn = find_conn(new_chan->handle);
	if (!new_conn) {
		return -ENODEV;
	}

	if ((bond_conn->type != BT_TYPE_LE) || (new_conn->type != BT_TYPE_LE)) {
		return -EINVAL;
	}

	bond_channel = find_channel2(bond_conn, bond_chan->id);
	if (!bond_channel) {
		return -ENODEV;
	}

	new_channel = find_channel2(new_conn, new_chan->id);
	if (!new_channel) {
		return -ENODEV;
	}

	os_sched_lock();
	new_channel->trigger = trigger;
	new_channel->bond_channel = bond_channel;
	os_sched_unlock();

	SYS_LOG_INF("chan %d %d bond with chan %d %d\n",new_chan->handle,new_chan->id
		,bond_chan->handle,bond_chan->id);
	return 0;
}

int bt_manager_audio_stream_tws_sync_cb_register(struct bt_audio_chan *chan,
				bt_audio_trigger_cb tws_sync_trigger){
	struct bt_audio_conn *audio_conn;
	struct bt_audio_channel *channel;

	if (!chan) {
		return -EINVAL;
	}

	if (!tws_sync_trigger) {
		return -EINVAL;
	}

	audio_conn = find_conn(chan->handle);
	if (!audio_conn) {
		return -EINVAL;
	}

	if (audio_conn->type != BT_TYPE_LE) {
		return -EINVAL;
	}

	channel = find_channel2(audio_conn, chan->id);
	if (!channel) {
		return -EINVAL;
	}

	if (channel->tws_sync_trigger) {
		return -EINVAL;
	}

	channel->tws_sync_trigger = tws_sync_trigger;

	return 0;
}

int bt_manager_audio_stream_set_tws_sync_cb_offset(struct bt_audio_chan *chan,
				int32_t offset_from_syncdelay)
{
	struct bt_audio_conn *audio_conn;
	struct bt_audio_channel *channel;

	if (!chan) {
		return -EINVAL;
	}

	audio_conn = find_conn(chan->handle);
	if (!audio_conn) {
		return -EINVAL;
	}

	if (audio_conn->type != BT_TYPE_LE) {
		return -EINVAL;
	}

	channel = find_channel2(audio_conn, chan->id);
	if (!channel) {
		return -EINVAL;
	}

	bt_manager_le_audio_stream_set_tws_sync_cb_offset(chan,offset_from_syncdelay);
	return 0;
}

#ifdef CONFIG_BT_ADV_MANAGER

static void lea_adv_req_start_cb(void)
{
	SYS_LOG_INF("LEA ADV req start");

	bt_audio.le_audio_adv_enable = ADV_STATE_BIT_ENABLE;
	bt_audio.le_audio_adv_enable |= ADV_STATE_BIT_UPDATE;
}

static void lea_adv_req_stop_cb(void)
{
	SYS_LOG_INF("LEA ADV req stop");

	bt_audio.le_audio_adv_enable = ADV_STATE_BIT_DISABLE;
	bt_audio.le_audio_adv_enable |= ADV_STATE_BIT_UPDATE;
}

int bt_manager_le_audio_adv_state(void)
{
//	struct bt_audio_conn *audio_conn;

//	SYS_LOG_INF(":%d %d\n",bt_audio.le_audio_init,bt_audio.le_audio_start);

	if (bt_audio.le_audio_adv_enable & ADV_STATE_BIT_ENABLE){
#if (!CONFIG_BT_DOUBLE_PHONE_TAKEOVER_POLICY)
		/*if device has been connected 2 BR, don't advertising LEA*/
		if (bt_manager_get_connected_dev_num() >=2){
			return (bt_audio.le_audio_adv_enable&(~ADV_STATE_BIT_ENABLE));
		}
#endif
		/*If br calling, don't advertising LEA */
		if (bt_manager_audio_in_btcall()){
			return (bt_audio.le_audio_adv_enable&(~ADV_STATE_BIT_ENABLE));
		}

#if 0

		if (bt_audio.take_over_flag){
			return (bt_audio.le_audio_adv_enable&(~ADV_STATE_BIT_ENABLE));
		}

		btmgr_pair_cfg_t *pair_config = bt_manager_get_pair_config();
        audio_conn = find_conn_by_prio(BT_TYPE_LE, 0);
		/*if device has been connected br or LEA, don't advertising LEA */
		if ((pair_config->bt_not_discoverable_when_connected)&&
		    ((bt_manager_get_connected_dev_num() >=1)|| (audio_conn)) ) {
		   	return (bt_audio.le_audio_adv_enable&(~ADV_STATE_BIT_ENABLE));
		}

		/*If br playing, don't advertising LEA */
		if (bt_manager_audio_in_btmusic()){
			return (bt_audio.le_audio_adv_enable&(~ADV_STATE_BIT_ENABLE));
		}

		/*if device has been connected LEA, don't advertising LEA*/
		audio_conn = find_conn_by_prio(BT_TYPE_LE, 0);
		if (audio_conn){
			return (bt_audio.le_audio_adv_enable&(~ADV_STATE_BIT_ENABLE));
		}
#endif
	}
	return bt_audio.le_audio_adv_enable;
}

int bt_manager_le_audio_adv_enable(void)
{
//	btmgr_feature_cfg_t *cfg_feature = bt_manager_get_feature_config();
//	btmgr_leaudio_cfg_t* leaudio_config = bt_manager_leaudio_config();
//	bt_addr_le_t peer;
//	int ret = 0;

	SYS_LOG_INF("LEA ADV start");
#if 0
	if (leaudio_config->leaudio_dir_adv_enable){
		if (cfg_feature->sp_tws){
			if (btif_tws_get_dev_role() == BTSRV_TWS_SLAVE){
				/*TWS slave: only use LEA directed advertising*/
				ret = bt_manager_audio_get_peer_addr(&peer);
				if (ret != 0){
					return 0;
				}
				bt_manager_audio_adv_param_init(&peer);
			}else{
				/*Others: use LEA legacy advertising*/
				bt_manager_audio_adv_param_init(NULL);
			}
		}else{
			ret = bt_manager_audio_get_peer_addr(&peer);
			if (ret == 0){
				/*only use LEA directed advertising*/
				bt_manager_audio_adv_param_init(&peer);
			}else{
				bt_manager_audio_adv_param_init(NULL);
			}
		}
	}else{
		bt_manager_audio_adv_param_init(NULL);
	}
#else
//    bt_manager_audio_adv_param_init(NULL);
#endif
	if (bt_audio.le_audio_adv_enable & ADV_STATE_BIT_ENABLE){
		bt_audio.le_audio_adv_enable &= ~ADV_STATE_BIT_UPDATE;
	}

	struct bt_le_adv_param adv_param = {0};
	int ret = btif_audio_update_adv_param(bt_manager_lea_policy_get_adv_param(ADV_TYPE_EXT,&adv_param));
	if (ret) {
		SYS_LOG_ERR("update:%d\n",ret);
	}

	return bt_manager_audio_le_start_adv();
}

int bt_manager_le_audio_adv_disable(void)
{
	SYS_LOG_INF("LEA ADV stop");

	if ((bt_audio.le_audio_adv_enable & ADV_STATE_BIT_ENABLE) == 0){
		bt_audio.le_audio_adv_enable &= ~ADV_STATE_BIT_UPDATE;
	}

	return bt_manager_audio_le_stop_adv();
}

int bt_manager_audio_takeover_flag(void)
{
    return bt_audio.take_over_flag;
}

#endif

void bt_manager_audio_set_takeover_flag(uint8_t flag)
{
    bt_audio.take_over_flag = flag;
}

/*
 * Audio Stream Control
 */
static int audio_init(uint8_t type, struct bt_audio_role *role)
{
	int ret;

	if (type == BT_TYPE_LE) {
		if (bt_audio.le_audio_state == BT_AUDIO_STATE_STOPPING) {
			/* FIMXE: clear possible le_audio_pending_start bit? */
			bt_audio.le_audio_pending_init = 1;
			memcpy(&bt_audio.le_audio_role, role, sizeof(*role));
			SYS_LOG_INF("stopping");
			return 0;
		}

		if (bt_audio.le_audio_init) {
			SYS_LOG_WRN("already");
			return -EALREADY;
		}

		ret = bt_manager_le_audio_init(role);
		if (ret) {
			SYS_LOG_ERR("%d", ret);
			return ret;
		}

		struct bt_le_adv_param adv_param = {0};
		btif_audio_adv_param_init(bt_manager_lea_policy_get_adv_param(ADV_TYPE_EXT, &adv_param));
		bt_audio.le_audio_init = 1;
		bt_audio.le_audio_state = BT_AUDIO_STATE_INITED;
		bt_audio.le_audio_adv_enable = 0;
		bt_audio.lea_connection_enable = role->lea_connection_enable;
		os_delayed_work_init(&bt_audio.lea_on_off_complete_work, lea_on_off_work_callback);
		bt_audio.lea_on_off_complete_work_valid = 0;

#ifdef CONFIG_BT_ADV_MANAGER
		bt_manager_audio_le_adv_cb_register(lea_adv_req_start_cb, lea_adv_req_stop_cb);
#endif

		if (role->disable_trigger) {
			return 0;
		}

		bt_manager_le_audio_stream_cb_register((struct bt_audio_stream_cb *)&stream_cb);
		btif_broadcast_stream_cb_register((struct bt_broadcast_stream_cb *)&bis_stream_cb);
		return 0;
	}

	return -EINVAL;
}

int bt_manager_audio_init(uint8_t type, struct bt_audio_role *role)
{
	int ret;

	if (role == NULL) {
		return -EINVAL;
	}

	bt_manager_audio_load_disconnect_info_from_nvram();

	os_sched_lock();
	ret = audio_init(type, role);
	os_sched_unlock();

	return ret;
}

static int audio_exit(uint8_t type)
{
	int ret;

	if (type == BT_TYPE_LE) {
		if (!bt_audio.le_audio_init) {
			SYS_LOG_WRN("already");
			return -EALREADY;
		}

		if (bt_audio.le_audio_state == BT_AUDIO_STATE_STOPPING) {
			bt_audio.le_audio_pending_exit = 1;
			/* NOTICE: the latter exit() will clear the former init() */
			bt_audio.le_audio_pending_init = 0;
			SYS_LOG_INF("stopping");
			return 0;
		}

		if (bt_audio.le_audio_start) {
			SYS_LOG_WRN("not stop");
			return -EINVAL;
		}

		ret = bt_manager_le_audio_exit();
		if (ret) {
			SYS_LOG_ERR("%d", ret);
			return ret;
		}

		bt_audio.le_audio_init = 0;
		bt_audio.le_audio_state = BT_AUDIO_STATE_UNKNOWN;
		k_delayed_work_cancel_sync(&bt_audio.lea_on_off_complete_work, K_FOREVER);

		return 0;
	}

	return -EINVAL;
}

int bt_manager_audio_exit(uint8_t type)
{
	int ret;

	os_sched_lock();
	ret = audio_exit(type);
	os_sched_unlock();

	return ret;
}

int bt_manager_audio_conn_disconnect(uint16_t handle)
{
	return btif_conn_disconnect_by_handle(handle,
					BT_HCI_ERR_REMOTE_USER_TERM_CONN);
}

int bt_manager_audio_keys_clear(uint8_t type)
{
	if (type == BT_TYPE_LE) {
		return bt_manager_le_audio_keys_clear();
	}

	return -EINVAL;
}

int bt_manager_audio_server_cap_init(struct bt_audio_capabilities *caps)
{
	uint16_t len;

	if (!caps) {
		return -EINVAL;
	}

	if (bt_audio.le_audio_state == BT_AUDIO_STATE_STOPPING) {
		len = caps->source_cap_num + caps->sink_cap_num;
		len *= sizeof(struct bt_audio_capability);
		len += sizeof(struct bt_audio_capabilities);

		/* check size */
		if (len > sizeof(bt_audio.caps)) {
			SYS_LOG_ERR("no mem");
			return -ENOMEM;
		}

		bt_audio.le_audio_pending_caps = 1;
		memcpy(&bt_audio.caps, caps, len);
		SYS_LOG_INF("stopping");
		return 0;
	}

	return bt_manager_le_audio_server_cap_init(caps);
}

int bt_manager_audio_server_qos_init(bool source,
				struct bt_audio_preferred_qos_default *qos)
{
	struct bt_audio_preferred_qos_default *default_qos;

	if (!qos) {
		return -EINVAL;
	}

	if (source) {
		bt_audio.le_audio_source_prefered_qos = 1;
		default_qos = &bt_audio.source_qos;
	} else {
		bt_audio.le_audio_sink_prefered_qos = 1;
		default_qos = &bt_audio.sink_qos;
	}

	default_qos->framing = qos->framing;
	default_qos->phy = qos->phy;
	default_qos->rtn = qos->rtn;
	default_qos->latency = qos->latency;
	default_qos->delay_min = qos->delay_min;
	default_qos->delay_max = qos->delay_max;
	default_qos->preferred_delay_min = qos->preferred_delay_min;
	default_qos->preferred_delay_max = qos->preferred_delay_max;

	return 0;
}

int bt_manager_audio_client_codec_init(struct bt_audio_codec_policy *policy)
{
	return 0;
}

int bt_manager_audio_client_qos_init(struct bt_audio_qos_policy *policy)
{
	return 0;
}

extern const struct bt_gatt_service_static mcs_svc;
extern const struct bt_gatt_service_static gtbs_svc;
extern const struct bt_gatt_service_static mics_aics_svc0;
extern const struct bt_gatt_service_static mics_svc;
extern const struct bt_gatt_service_static vcs_svc;
#ifdef CONFIG_BT_VCS_SERVICE
extern const struct bt_gatt_service_static vcs_vocs_svc;
#endif
#if defined(CONFIG_BT_VOCS)
extern const struct bt_gatt_service_static vocs_svc;
#endif

void bt_manager_audio_service_disable(void)
{
#ifndef CONFIG_BT_LE_AUDIO_MASTER
		bt_gatt_svc_disable(&mcs_svc);
		bt_gatt_svc_disable(&gtbs_svc);
#endif
		bt_gatt_svc_disable(&mics_aics_svc0);
		bt_gatt_svc_disable(&mics_svc);
#ifdef CONFIG_BT_VCS_SERVICE
		bt_gatt_svc_disable(&vcs_vocs_svc);
#endif

#if defined(CONFIG_BT_VOCS)
		bt_gatt_svc_disable(&vocs_svc);
#endif
}

static int audio_start(uint8_t type)
{
	int ret;

	if (type == BT_TYPE_LE) {
		if (!bt_audio.le_audio_init) {
			SYS_LOG_WRN("not init");
			return -EINVAL;
		}

		if (bt_audio.le_audio_state == BT_AUDIO_STATE_STOPPING) {
			bt_audio.le_audio_pending_start = 1;
			SYS_LOG_INF("stopping");
			return 0;
		}

		if (bt_audio.le_audio_start) {
			SYS_LOG_WRN("already");
			return -EALREADY;
		}
#ifdef CONFIG_BT_LEA_PTS_TEST
		pts_le_audio_service_disable();
#else
        bt_manager_audio_service_disable();
#endif

		ret = bt_manager_le_audio_start();
		if (ret) {
			SYS_LOG_ERR("%d", ret);
			return ret;
		}

		bt_audio.le_audio_start = 1;
		bt_audio.le_audio_state = BT_AUDIO_STATE_STARTED;

		return 0;
	}

	return -EINVAL;
}

int bt_manager_audio_start(uint8_t type)
{
	int ret;

	os_sched_lock();
	ret = audio_start(type);
	os_sched_unlock();

	return ret;
}

static bool bt_manager_unpair_lea_conn(void)
{
	struct bt_audio_conn *audio_conn = NULL;
	const bt_addr_le_t *addr = NULL;
	bool ret = false;

	os_sched_lock();

	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
		if (audio_conn->is_lea) {
			addr = btif_get_le_addr_by_handle(audio_conn->handle);
			hostif_bt_le_unpair_device(addr);
			ret = true;
		}
	}

	os_sched_unlock();
	return ret;
}

static int lea_on_process(void)
{
	int ret = 0;

	bt_audio.lea_connection_enable = LEA_ON;
	hostif_bt_ctkd_enable(1);

	/* unpair phones */
	btif_br_clear_list(BTSRV_DEVICE_ALL);
	bt_manager_unpair_lea_conn();
	bt_manager_lea_clear_paired_list();

	/* set CoD */
	hostif_bt_write_class(bt_manager_config_bt_class());
	/* enable lea services*/
	ret = bt_manager_lea_services_enable();
	/* enable lea policy */
	bt_manager_lea_policy_enable(0);

	property_set_int(CFG_LEA_ONOFF_STATUS, 1);

	SYS_LOG_INF("state:%d, ret %d\n",bt_audio.lea_connection_enable, ret);

	if (bt_audio.complete_cb) {
		bt_audio.complete_cb(ret);
	}

	return ret;
}

static int lea_off_process(void)
{
	int ret = 0;

	bt_audio.lea_connection_enable = LEA_OFF;
	hostif_bt_ctkd_enable(0);

	/* disable lea policy */
	bt_manager_lea_policy_disable(0);
	/* unpair phones */
	btif_br_clear_list(BTSRV_DEVICE_ALL);
	if (bt_manager_unpair_lea_conn()) {
		/* wait for att disconnect */
		os_sleep(OS_MSEC(100));
	}
	bt_manager_lea_clear_paired_list();

	/* disable lea services*/
	ret = bt_manager_lea_services_disable();
	/* set CoD */
	hostif_bt_write_class(bt_manager_config_bt_class());

	property_set_int(CFG_LEA_ONOFF_STATUS, 0);

	SYS_LOG_INF("state:%d, ret %d\n",bt_audio.lea_connection_enable, ret);

	if (bt_audio.complete_cb) {
		bt_audio.complete_cb(ret);
	}

	return ret;
}

static void lea_on_off_work_callback(struct k_work *work)
{
	bt_audio.lea_on_off_complete_work_valid = 0;
	if (LEA_ON_ONGOING == bt_audio.lea_connection_enable) {
		lea_on_process();
	} else if (LEA_OFF_ONGOING == bt_audio.lea_connection_enable) {
		lea_off_process();
	}
}

int bt_manager_audio_lea_enable(lea_on_off_complete_cb cb)
{
	int ret = 0;

	if (bt_audio.lea_connection_enable == LEA_ON) {
		return -EALREADY;
	}

	os_sched_lock();
	bt_audio.complete_cb = cb;
	os_sched_unlock();

	/* check audio active */
	ret = bt_manager_media_pause();
	if (!ret) {
		bt_audio.lea_connection_enable = LEA_ON_ONGOING;
		if (bt_audio.lea_on_off_complete_work_valid) {
			os_delayed_work_cancel(&bt_audio.lea_on_off_complete_work);
		}
		os_delayed_work_submit(&bt_audio.lea_on_off_complete_work, BT_AUDIO_DELAY_LEA_ON_OFF_TIME);
		bt_audio.lea_on_off_complete_work_valid = 1;
		return 0;
	}

	return lea_on_process();
}

int bt_manager_audio_lea_disable(lea_on_off_complete_cb cb)
{
	int ret = 0;
	if (bt_audio.lea_connection_enable == LEA_OFF) {
		return -EALREADY;
	}

	os_sched_lock();
	bt_audio.complete_cb = cb;
	os_sched_unlock();

	/* check audio active */
	ret = bt_manager_media_pause();
	if (!ret) {
		bt_audio.lea_connection_enable = LEA_OFF_ONGOING;
		if (bt_audio.lea_on_off_complete_work_valid) {
			os_delayed_work_cancel(&bt_audio.lea_on_off_complete_work);
		}
		os_delayed_work_submit(&bt_audio.lea_on_off_complete_work, BT_AUDIO_DELAY_LEA_ON_OFF_TIME);
		bt_audio.lea_on_off_complete_work_valid = 1;
		return 0;
	}

	return lea_off_process();
}

int bt_manager_audio_sq_mode_update(int8_t sq_mode)
{
	uint8 update = 0;
	int8_t sq_mode_enable = bt_manager_broadcast_get_sq_mode();

	if (sq_mode_enable != sq_mode) {
		update = 1;
		bt_manager_broadcast_set_sq_mode(sq_mode);
	}

	if (update) {
		bt_manager_event_notify(BT_BROADCAST_SOURCE_UPDATE_SQ, NULL, 0);
	}

	return 0;
}

int bt_manager_audio_is_lea_open(void)
{
	return bt_audio.lea_connection_enable;
}

static int audio_stop(uint8_t type)
{
	int ret;

	if (type == BT_TYPE_LE) {
		if (!bt_audio.le_audio_init) {
			SYS_LOG_WRN("not init");
			return -EINVAL;
		}

		if (!bt_audio.le_audio_start) {
			SYS_LOG_WRN("already");
			return -EALREADY;
		}

		if (bt_audio.le_audio_state == BT_AUDIO_STATE_STOPPING) {
			bt_audio.le_audio_pending_start = 0;
			SYS_LOG_INF("stopping");
			return 0;
		}

		ret = bt_manager_le_audio_stop();
		if (ret == -EINPROGRESS) {
			bt_audio.le_audio_state = BT_AUDIO_STATE_STOPPING;
			SYS_LOG_INF("stopping");
			return ret;
		} else if (ret) {
			SYS_LOG_ERR("%d", ret);
			return ret;
		}

		/*
		 * stop successfully
		 */
		bt_audio.le_audio_start = 0;
		/* force to clear puase bits too */
		bt_audio.le_audio_pause = 0;
		bt_audio.le_audio_state = BT_AUDIO_STATE_STOPPED;

		return 0;
	}

	return -EINVAL;
}

int bt_manager_audio_stop(uint8_t type)
{
	int ret;

	os_sched_lock();
	ret = audio_stop(type);
	os_sched_unlock();

	return ret;
}

/* NOTE: there is no need to lock the scheduler in this context */
void bt_manager_audio_stopped_event(uint8_t type)
{
	SYS_LOG_INF("%d", type);

	if (type == BT_TYPE_LE) {
		/*
		 * stop successfully
		 */
		bt_audio.le_audio_start = 0;
		/* force to clear puase bits too */
		bt_audio.le_audio_pause = 0;
		bt_audio.le_audio_state = BT_AUDIO_STATE_STOPPED;

		/*
		 * if pending_exit is not set, all other pending bits
		 * except pending_start should not be set too.
		 */
		if (!bt_audio.le_audio_pending_exit) {
			goto pending_start;
		}

		/* le_audio_pending_exit */
		audio_exit(BT_TYPE_LE);
		bt_audio.le_audio_pending_exit = 0;

		if (bt_audio.le_audio_pending_init) {
			audio_init(BT_TYPE_LE, &bt_audio.le_audio_role);
			bt_audio.le_audio_pending_init = 0;
		}

		if (bt_audio.le_audio_pending_caps) {
			bt_manager_le_audio_server_cap_init((struct bt_audio_capabilities *)&bt_audio.caps);
			bt_audio.le_audio_pending_caps = 0;
		}

		if (bt_audio.le_audio_pending_conn_param) {
			bt_manager_audio_le_conn_param_init(&bt_audio.le_audio_conn_param);
			bt_audio.le_audio_pending_conn_param = 0;
		}

		if (bt_audio.le_audio_pending_adv_param) {
			bt_manager_audio_le_adv_param_init(&bt_audio.le_audio_adv_param);
			bt_audio.le_audio_pending_adv_param = 0;
		}

		if (bt_audio.le_audio_pending_pause_adv) {
			bt_manager_audio_le_pause_adv();
			bt_audio.le_audio_pending_pause_adv = 0;
		}

		if (bt_audio.le_audio_pending_pause_scan) {
			bt_manager_audio_le_pause_scan();
			bt_audio.le_audio_pending_pause_scan = 0;
		}

pending_start:
		if (bt_audio.le_audio_pending_start) {
			audio_start(BT_TYPE_LE);
			bt_audio.le_audio_pending_start = 0;
		}
	}
}

static int audio_pause(uint8_t type)
{
	int ret;

	if (type == BT_TYPE_LE) {
		if (!bt_audio.le_audio_init) {
			SYS_LOG_WRN("not init");
			return -EINVAL;
		}

		if (!bt_audio.le_audio_start) {
			SYS_LOG_WRN("not start");
			return -EINVAL;
		}

		if (bt_audio.le_audio_pause) {
			SYS_LOG_WRN("already");
			return -EALREADY;
		}

		ret = bt_manager_le_audio_pause();
		if (ret) {
			SYS_LOG_ERR("%d", ret);
			return ret;
		}

		bt_audio.le_audio_pause = 1;

		return 0;
	}

	return -EINVAL;
}

static int audio_resume(uint8_t type)
{
	int ret;

	if (type == BT_TYPE_LE) {
		if (!bt_audio.le_audio_init) {
			SYS_LOG_WRN("not init");
			return -EINVAL;
		}

		if (!bt_audio.le_audio_start) {
			SYS_LOG_WRN("not start");
			return -EINVAL;
		}

		if (!bt_audio.le_audio_pause) {
			SYS_LOG_WRN("not pause");
			return -EINVAL;
		}

		ret = bt_manager_le_audio_resume();
		if (ret) {
			SYS_LOG_ERR("%d", ret);
			return ret;
		}

		bt_audio.le_audio_pause = 0;

		return 0;
	}

	return -EINVAL;
}

int bt_manager_audio_pause(uint8_t type)
{
	int ret;

	os_sched_lock();
	ret = audio_pause(type);
	os_sched_unlock();

	return ret;
}

int bt_manager_audio_resume(uint8_t type)
{
	int ret;

	os_sched_lock();
	ret = audio_resume(type);
	os_sched_unlock();

	return ret;
}

int bt_manager_audio_stream_config_codec(struct bt_audio_codec *codec)
{
	uint8_t type;
	int ret;

	if (!codec) {
		return -EINVAL;
	}

	type = bt_manager_audio_get_type(codec->handle);

	if (type == BT_TYPE_LE) {
		ret = bt_manager_le_audio_stream_config_codec(codec);
		if (ret) {
			return ret;
		}

		return stream_config_codec(codec, sizeof(*codec));
	}

	return -EINVAL;
}

int bt_manager_audio_stream_prefer_qos(struct bt_audio_preferred_qos *qos)
{
	uint8_t type;

	if (!qos) {
		return -EINVAL;
	}

	type = bt_manager_audio_get_type(qos->handle);

	if (type == BT_TYPE_LE) {
		return bt_manager_le_audio_stream_prefer_qos(qos);
	}

	return -EINVAL;
}

int bt_manager_audio_stream_config_qos(struct bt_audio_qoss *qoss)
{
	uint8_t type;

	if (!qoss || !qoss->num_of_qos) {
		return -EINVAL;
	}

	type = bt_manager_audio_get_type(qoss->qos[0].handle);

	if (type == BT_TYPE_LE) {
		return bt_manager_le_audio_stream_config_qos(qoss);
	}

	return -EINVAL;
}

int bt_manager_audio_stream_bind_qos(struct bt_audio_qoss *qoss, uint8_t index)
{
	uint8_t type;

	if (!qoss || !qoss->num_of_qos) {
		return -EINVAL;
	}

	type = bt_manager_audio_get_type(qoss->qos[0].handle);

	if (type == BT_TYPE_LE) {
		return bt_manager_le_audio_stream_bind_qos(qoss, index);
	}

	return -EINVAL;
}

/* check if all channels have the same handle. */
static bool check_valid(struct bt_audio_chan **chans, uint8_t num_chans)
{
	uint16_t handle;
	uint8_t i;

	if (!chans || !num_chans || !chans[0]) {
		return false;
	}

	if (num_chans == 1) {
		return true;
	}

	handle = chans[0]->handle;
	for (i = 1; i < num_chans; i++) {
		if (!chans[i] || (handle != chans[i]->handle)) {
			return false;
		}
	}

	return true;
}

/*
 * check if the direction is valid.
 *
 * NOTICE: no need to check direction for BR Audio yet!
 */
static bool check_direction(struct bt_audio_chan **chans, uint8_t num_chans,
				bool source_stream)
{
	struct bt_audio_chan *chan;
	uint8_t i, role, dir;

	role = bt_manager_audio_get_role(chans[0]->handle);
	if (role == BT_ROLE_MASTER) {
		if (source_stream) {
			dir = BT_AUDIO_DIR_SINK;
		} else {
			dir = BT_AUDIO_DIR_SOURCE;
		}
	} else if (role == BT_ROLE_SLAVE) {
		if (source_stream) {
			dir = BT_AUDIO_DIR_SOURCE;
		} else {
			dir = BT_AUDIO_DIR_SINK;
		}
	} else {
		return false;
	}

	for (i = 0; i < num_chans; i++) {
		chan = chans[i];
		if (bt_manager_audio_stream_dir(chan->handle, chan->id) != dir) {
			return false;
		}
	}

	return true;
}

int bt_manager_audio_stream_sync_forward(struct bt_audio_chan **chans,
				uint8_t num_chans)
{
	uint8_t type;

	if (!chans || !num_chans || !chans[0]) {
		return -EINVAL;
	}

	type = bt_manager_audio_get_type(chans[0]->handle);

	if (type == BT_TYPE_LE) {
		return bt_manager_le_audio_stream_sync_forward(chans, num_chans);
	}

	return -EINVAL;
}

int bt_manager_audio_stream_sync_forward_single(struct bt_audio_chan *chan)
{
	return bt_manager_audio_stream_sync_forward(&chan, 1);
}

int bt_manager_audio_stream_enable(struct bt_audio_chan **chans,
				uint8_t num_chans, uint32_t contexts)
{
	uint8_t type;

	if (!check_valid(chans, num_chans)) {
		return -EINVAL;
	}

	type = bt_manager_audio_get_type(chans[0]->handle);

	if (type == BT_TYPE_LE) {
		return bt_manager_le_audio_stream_enable(chans, num_chans, contexts);
	}

	return -EINVAL;
}

int bt_manager_audio_stream_enable_single(struct bt_audio_chan *chan,
				uint32_t contexts)
{
	return bt_manager_audio_stream_enable(&chan, 1, contexts);
}

int bt_manager_audio_stream_disable(struct bt_audio_chan **chans,
				uint8_t num_chans)
{
	uint8_t type;

	if (!check_valid(chans, num_chans)) {
		return -EINVAL;
	}

	type = bt_manager_audio_get_type(chans[0]->handle);

	if (type == BT_TYPE_LE) {
		return bt_manager_le_audio_stream_disable(chans, num_chans);
	}

	return -EINVAL;
}

int bt_manager_audio_stream_disable_single(struct bt_audio_chan *chan)
{
	return bt_manager_audio_stream_disable(&chan, 1);
}

io_stream_t bt_manager_audio_source_stream_create(struct bt_audio_chan **chans,
				uint8_t num_chans, uint32_t locations)
{
	struct bt_audio_channel *chan;
	uint8_t type, codec;

	if (!chans || !num_chans || !chans[0]) {
		return false;
	}

	type = bt_manager_audio_get_type(chans[0]->handle);

	if (type == BT_TYPE_BR) {
		if (chans[0]->id != BT_AUDIO_ENDPOINT_CALL) {
			return NULL;
		}

		chan = find_channel(chans[0]->handle, chans[0]->id);
		if (!chan) {
			return NULL;
		}
		codec = BTSRV_SCO_MSBC;

		if (chan->format == BT_AUDIO_CODEC_MSBC) {
			codec = BTSRV_SCO_MSBC;
		} else if (chan->format == BT_AUDIO_CODEC_CVSD) {
			codec = BTSRV_SCO_CVSD;
		} else if (chan->format == BT_AUDIO_CODEC_LC3_SWB) {
			codec = BTSRV_SCO_LC3_SWB;
		}

		return sco_upload_stream_create(codec);
	} else if (type == BT_TYPE_LE) {
		if (!check_direction(chans, num_chans, true)) {
			return NULL;
		}

		return bt_manager_le_audio_source_stream_create(chans,
						num_chans, locations);
	}

	return NULL;
}

io_stream_t bt_manager_audio_source_stream_create_single(struct bt_audio_chan *chan,
				uint32_t locations)
{
	return bt_manager_audio_source_stream_create(&chan, 1, locations);
}

int bt_manager_audio_source_stream_set(struct bt_audio_chan **chans,
				uint8_t num_chans, io_stream_t stream,
				uint32_t locations)
{
	uint8_t type;

	if (!chans || !num_chans || !chans[0]) {
		return -EINVAL;
	}

	/* NOTICE: should use 1 stream for each chan later ... */
	if (num_chans != 1) {
		return -EINVAL;
	}

	type = bt_manager_audio_get_type(chans[0]->handle);

	if (type == BT_TYPE_LE) {
		if (!check_direction(chans, num_chans, true)) {
			return -EINVAL;
		}

		return bt_manager_le_audio_source_stream_set(chans,
						num_chans, stream, locations);
	}

	return -EINVAL;
}

int bt_manager_audio_source_stream_set_single(struct bt_audio_chan *chan,
				io_stream_t stream, uint32_t locations)
{
	return bt_manager_audio_source_stream_set(&chan, 1, stream, locations);
}

int bt_manager_audio_sink_stream_set(struct bt_audio_chan *chan,
				io_stream_t stream)
{
	uint8_t type;

	if (!chan) {
		return -EINVAL;
	}

	type = bt_manager_audio_get_type(chan->handle);

	if (type == BT_TYPE_BR) {
		if (chan->id == BT_AUDIO_ENDPOINT_MUSIC) {
			return bt_manager_set_stream(STREAM_TYPE_A2DP, stream);
		} else if (chan->id == BT_AUDIO_ENDPOINT_CALL) {
			return bt_manager_set_stream(STREAM_TYPE_SCO, stream);
		}
	} else if (type == BT_TYPE_LE) {
		if (!check_direction(&chan, 1, false)) {
			return -EINVAL;
		}

		return bt_manager_le_audio_sink_stream_set(chan, stream);
	}

	return -EINVAL;
}

int bt_manager_audio_sink_stream_start(struct bt_audio_chan **chans,
				uint8_t num_chans)
{
	uint8_t type;

	if (!check_valid(chans, num_chans)) {
		return -EINVAL;
	}

	type = bt_manager_audio_get_type(chans[0]->handle);

	if (type == BT_TYPE_LE) {
		if (!check_direction(chans, num_chans, false)) {
			return -EINVAL;
		}

		return bt_manager_le_audio_sink_stream_start(chans, num_chans);
	}

	return -EINVAL;
}

int bt_manager_audio_sink_stream_start_single(struct bt_audio_chan *chan)
{
	return bt_manager_audio_sink_stream_start(&chan, 1);
}

int bt_manager_audio_sink_stream_stop(struct bt_audio_chan **chans,
				uint8_t num_chans)
{
	uint8_t type;

	if (!check_valid(chans, num_chans)) {
		return -EINVAL;
	}

	type = bt_manager_audio_get_type(chans[0]->handle);

	if (type == BT_TYPE_LE) {
		if (!check_direction(chans, num_chans, false)) {
			return -EINVAL;
		}

		return bt_manager_le_audio_sink_stream_stop(chans, num_chans);
	}

	return -EINVAL;
}

int bt_manager_audio_sink_stream_stop_single(struct bt_audio_chan *chan)
{
	return bt_manager_audio_sink_stream_stop(&chan, 1);
}

int bt_manager_audio_stream_update(struct bt_audio_chan **chans,
				uint8_t num_chans, uint32_t contexts)
{
	uint8_t type;

	if (!check_valid(chans, num_chans)) {
		return -EINVAL;
	}

	type = bt_manager_audio_get_type(chans[0]->handle);

	if (type == BT_TYPE_LE) {
		return bt_manager_le_audio_stream_update(chans, num_chans, contexts);
	}

	return -EINVAL;
}

int bt_manager_audio_stream_update_single(struct bt_audio_chan *chan,
				uint32_t contexts)
{
	return bt_manager_audio_stream_update(&chan, 1, contexts);
}

int bt_manager_audio_stream_release(struct bt_audio_chan **chans,
				uint8_t num_chans)
{
	uint8_t type;

	if (!check_valid(chans, num_chans)) {
		return -EINVAL;
	}

	type = bt_manager_audio_get_type(chans[0]->handle);

	if (type == BT_TYPE_LE) {
		return bt_manager_le_audio_stream_release(chans, num_chans);
	}

	return -EINVAL;
}

int bt_manager_audio_stream_release_single(struct bt_audio_chan *chan)
{
	return bt_manager_audio_stream_release(&chan, 1);
}

int bt_manager_audio_stream_disconnect(struct bt_audio_chan **chans,
				uint8_t num_chans)
{
	uint8_t type;

	if (!check_valid(chans, num_chans)) {
		return -EINVAL;
	}

	type = bt_manager_audio_get_type(chans[0]->handle);

	if (type == BT_TYPE_LE) {
		return bt_manager_le_audio_stream_disconnect(chans, num_chans);
	}

	return -EINVAL;
}

int bt_manager_audio_stream_disconnect_single(struct bt_audio_chan *chan)
{
	return bt_manager_audio_stream_disconnect(&chan, 1);
}

void *bt_manager_audio_stream_bandwidth_alloc(struct bt_audio_qoss *qoss)
{
	return bt_manager_le_audio_stream_bandwidth_alloc(qoss);
}

int bt_manager_audio_stream_bandwidth_free(void *cig_handle)
{
	return bt_manager_le_audio_stream_bandwidth_free(cig_handle);
}

static inline int stream_restore_br(void)
{
	return bt_manager_a2dp_check_state();
}

static int stream_restore_le(void)
{
	struct bt_audio_conn *audio_conn = get_active_slave();
	struct bt_audio_chan_restore_max restore;
	struct bt_audio_channel *chan;
	struct bt_audio_chan *ch;
	uint8_t count;
	int ret = 0;

	/* could restore LE slave only */
	if (audio_conn->type != BT_TYPE_LE) {
		return -EINVAL;
	}

	os_sched_lock();

	if (sys_slist_is_empty(&audio_conn->chan_list)) {
		os_sched_unlock();
		return -ENODEV;
	}

	count = 0;
	SYS_SLIST_FOR_EACH_CONTAINER(&audio_conn->chan_list, chan, node) {
		if (count >= BT_AUDIO_CHAN_MAX) {
			break;
		}

		ch = &restore.chans[count];
		ch->handle = audio_conn->handle;
		ch->id = chan->id;

		count++;
	}

	restore.count = count;

	os_sched_unlock();


	ret = bt_manager_event_notify(BT_AUDIO_STREAM_RESTORE, &restore,
				sizeof(restore));

	if (audio_conn->lea_vol_update) {
		bt_manager_event_notify(BT_AUDIO_VOLUME_RESTORE,
			&audio_conn->lea_vol, sizeof(audio_conn->lea_vol));
	}

	return ret;
}

#if 0
int bt_manager_media_set_user_pause_pending(uint16_t hdl)
{
	struct bt_audio_conn *audio_conn;
	bt_mgr_dev_info_t* dev_info;

	os_sched_lock();

	audio_conn = find_conn(hdl);
	if ((audio_conn) && (audio_conn->type == BT_TYPE_BR)) {
		dev_info = bt_mgr_find_dev_info_by_hdl(audio_conn->handle);
		if (dev_info) {
			dev_info->user_pause_pending = 1;
			SYS_LOG_INF("");
		}
	}

	os_sched_unlock();

	return 1;
}
#endif

int bt_manager_audio_stream_restore(uint8_t type)
{
	if (type == BT_TYPE_BR) {
		return stream_restore_br();
	} else if (type == BT_TYPE_LE) {
		return stream_restore_le();
	}

	return -EINVAL;
}

enum media_type bt_manager_audio_codec_type(uint8_t coding_format)
{
	switch (coding_format) {
	case BT_AUDIO_CODEC_LC3:
		return NAV_TYPE;
	case BT_AUDIO_CODEC_CVSD:
		return CVSD_TYPE;
	case BT_AUDIO_CODEC_MSBC:
		return MSBC_TYPE;
	case BT_AUDIO_CODEC_AAC:
		return AAC_TYPE;
	case BT_AUDIO_CODEC_SBC:
		return SBC_TYPE;
	case BT_AUDIO_CODEC_LDAC:
		return LDAC_TYPE;
	default:
		break;
	}

	return UNSUPPORT_TYPE;
}

int bt_manager_media_play(void)
{
	struct bt_audio_conn *audio_conn;

	audio_conn = find_active_slave();
	if (!audio_conn) {
		return -EINVAL;
	}

	if (audio_conn->type == BT_TYPE_BR) {
		return bt_manager_avrcp_play_by_hdl(audio_conn->handle);
	} else if (audio_conn->type == BT_TYPE_LE) {
		audio_conn->media_state = BT_STATUS_INACTIVE;
		return bt_manager_le_media_play(audio_conn->handle);
	}

	return -EINVAL;
}

int bt_manager_media_stop(void)
{
	struct bt_audio_conn *audio_conn;

	audio_conn = find_active_slave();
	if (!audio_conn) {
		return -EINVAL;
	}

	if (audio_conn->type == BT_TYPE_BR) {
		return bt_manager_avrcp_stop();
	} else if (audio_conn->type == BT_TYPE_LE) {
		audio_conn->media_state = BT_STATUS_INACTIVE;
		return bt_manager_le_media_stop(audio_conn->handle);
	}

	return -EINVAL;
}

int bt_manager_media_pause(void)
{
	struct bt_audio_conn *audio_conn;

	audio_conn = find_active_slave();
	if (!audio_conn) {
		return -EINVAL;
	}

	if ((bt_audio.device_num == 1) &&
		(audio_conn->is_phone_dev) &&
		(bt_audio.active_slave) && (audio_conn->type == BT_TYPE_BR)) {
			bt_audio.inactive_slave = bt_audio.active_slave;
	}
	SYS_LOG_INF("active_slave: %p, %p, %d, %d\r\n", bt_audio.active_slave,
				bt_audio.inactive_slave, audio_conn->is_phone_dev, bt_audio.device_num);

	if (audio_conn->type == BT_TYPE_BR) {
		return bt_manager_avrcp_pause_by_hdl(audio_conn->handle);
	} else if (audio_conn->type == BT_TYPE_LE) {
		audio_conn->media_state = BT_STATUS_INACTIVE;
		return bt_manager_le_media_pause(audio_conn->handle);
	}

	return -EINVAL;
}

int bt_manager_media_playpause(void)
{
	struct bt_audio_conn *audio_conn;
	bt_mgr_dev_info_t* dev_info;

	audio_conn = find_active_slave();
	if (!audio_conn) {
		return -EINVAL;
	}

	if (audio_conn->type == BT_TYPE_BR) {
		dev_info = bt_mgr_find_dev_info_by_hdl(audio_conn->handle);
		if (dev_info == NULL)
			return -EINVAL;

		if ((dev_info->avrcp_ext_status & BT_MANAGER_AVRCP_EXT_STATUS_PASSTHROUGH_PLAY) ||
			(dev_info->avrcp_ext_status & BT_MANAGER_AVRCP_EXT_STATUS_PLAYING)) {
			if(dev_info->a2dp_stream_started){
				bt_manager_media_pause();
			} else {
				bt_manager_media_play();
			}
		} else {
			bt_manager_media_play();
		}

		SYS_LOG_INF("hdl: 0x%x play_status: 0x%x\n",dev_info->hdl, dev_info->avrcp_ext_status);
	} else if (audio_conn->type == BT_TYPE_LE
				&& audio_conn->media_state == BT_STATUS_INACTIVE) {
		if (audio_conn->cis_connected) {
			bt_manager_media_pause();
		} else {
			bt_manager_media_play();
		}
	} else {
		if(audio_conn->media_state == BT_STATUS_PLAYING){
			bt_manager_media_pause();
		}else{
			bt_manager_media_play();
		}
	}
	return 1;
}

int bt_manager_media_play_next(void)
{
	struct bt_audio_conn *audio_conn;

	audio_conn = find_active_slave();
	if (!audio_conn) {
		return -EINVAL;
	}

	if (audio_conn->type == BT_TYPE_BR) {
		return bt_manager_avrcp_play_next_by_hdl(audio_conn->handle);
	} else if (audio_conn->type == BT_TYPE_LE) {
		return bt_manager_le_media_play_next(audio_conn->handle);
	}

	return -EINVAL;
}

int bt_manager_media_play_previous(void)
{
	struct bt_audio_conn *audio_conn;

	audio_conn = find_active_slave();
	if (!audio_conn) {
		return -EINVAL;
	}

	if (audio_conn->type == BT_TYPE_BR) {
		return bt_manager_avrcp_play_previous_by_hdl(audio_conn->handle);
	} else if (audio_conn->type == BT_TYPE_LE) {
		return bt_manager_le_media_play_previous(audio_conn->handle);
	}

	return -EINVAL;
}

int bt_manager_media_fast_forward(bool start)
{
	struct bt_audio_conn *audio_conn;

	audio_conn = find_active_slave();
	if (!audio_conn) {
		return -EINVAL;
	}

	if (audio_conn->type == BT_TYPE_BR) {
		return bt_manager_avrcp_fast_forward_by_hdl(audio_conn->handle, start);
	} else if (audio_conn->type == BT_TYPE_LE) {
		return bt_manager_le_media_fast_forward(audio_conn->handle);
	}

	return -EINVAL;
}

int bt_manager_media_fast_rewind(bool start)
{
	struct bt_audio_conn *audio_conn;

	audio_conn = find_active_slave();
	if (!audio_conn) {
		return -EINVAL;
	}

	if (audio_conn->type == BT_TYPE_BR) {
		return bt_manager_avrcp_fast_backward_by_hdl(audio_conn->handle, start);
	} else if (audio_conn->type == BT_TYPE_LE) {
		return bt_manager_le_media_fast_rewind(audio_conn->handle);
	}

	return -EINVAL;
}

int bt_manager_media_get_id3_info(void)
{
	struct bt_audio_conn *audio_conn;

	audio_conn = find_active_slave();
	if (!audio_conn) {
		return -EINVAL;
	}

	if (audio_conn->type == BT_TYPE_BR) {
		return bt_manager_avrcp_get_id3_info();
	}

	return -EINVAL;
}

void bt_manager_media_event(int event, void *data, int size)
{
	struct bt_media_report *rep = (struct bt_media_report *)data;
	struct bt_audio_conn *audio_conn;
	uint8_t change = 0;

	audio_conn = find_conn(rep->handle);
	if (!audio_conn) {
		SYS_LOG_ERR("no conn %d", event);
		return;
	}

	if (audio_conn->role == BT_ROLE_MASTER) {
		bt_manager_event_notify(event, data, size);
		return;
	}

	switch (event) {
	case BT_MEDIA_PLAY:
		audio_conn->media_state = BT_STATUS_PLAYING;
		break;
	case BT_MEDIA_PAUSE:
	case BT_MEDIA_STOP:
		audio_conn->media_state = BT_STATUS_PAUSED;
		break;
	case BT_MEDIA_UPDATE_ID3_INFO_EVENT:
		/*{
			struct bt_media_id3_info *pinfo = (struct bt_media_id3_info *)data;
			for(int i =0 ; i< pinfo->num_of_item; i++){
				if(pinfo[i].item->len && pinfo[i].item->data){
					SYS_LOG_INF("id3 %d %s\n",pinfo[i].item->id,pinfo[i].item->data);
				}
			}
		}*/
		break;
	}

	if (audio_conn == get_active_slave()) {
		if(audio_conn->type != BT_TYPE_BR || (event != BT_MEDIA_PLAY && event != BT_MEDIA_PAUSE)){
			bt_manager_event_notify(event, data, size);
		}else{
			SYS_LOG_INF("%d", event);
		}
	} else {
		SYS_LOG_INF("drop msg %p %p %d\n", audio_conn, get_active_slave(), event);
		os_sched_lock();
		if (event == BT_MEDIA_PLAY) {
			//br prio is handled in btsrv.we don't need to change prio here.
			if(audio_conn->type != BT_TYPE_BR || get_active_slave()->type != BT_TYPE_BR){
				audio_conn->prio |= BT_AUDIO_PRIO_NOTIFY;
				change = 1;
				SYS_LOG_INF("active device change %p %p\n", audio_conn, get_active_slave());
			}
			update_slave_prio(audio_conn);
			if(change){
				audio_conn->prio &= ~BT_AUDIO_PRIO_NOTIFY;
			}
		}
		os_sched_unlock();
	}
}

int bt_manager_media_server_play(void)
{
	struct bt_audio_conn *audio_conn;

	/* for each master */
	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
		if (audio_conn->role != BT_ROLE_MASTER) {
			continue;
		}

		if (audio_conn->type == BT_TYPE_LE) {
			bt_manager_le_media_play(audio_conn->handle);
		}
	}

	return 0;
}

int bt_manager_media_server_stop(void)
{
	struct bt_audio_conn *audio_conn;

	/* for each master */
	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
		if (audio_conn->role != BT_ROLE_MASTER) {
			continue;
		}

		if (audio_conn->type == BT_TYPE_LE) {
			bt_manager_le_media_stop(audio_conn->handle);
		}
	}

	return 0;
}

int bt_manager_media_server_pause(void)
{
	struct bt_audio_conn *audio_conn;

	/* for each master */
	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
		if (audio_conn->role != BT_ROLE_MASTER) {
			continue;
		}

		if (audio_conn->type == BT_TYPE_LE) {
			bt_manager_le_media_pause(audio_conn->handle);
		}
	}

	return 0;
}

int bt_manager_media_get_status(void)
{
	struct bt_audio_conn *audio_conn;
	bt_mgr_dev_info_t* dev_info;

	audio_conn = find_active_slave();
	if (!audio_conn) {
		return BT_STATUS_PAUSED;
	}

	//TODO:use audio_conn->media_state directly
	if (audio_conn->type == BT_TYPE_BR) {
		dev_info = bt_mgr_find_dev_info_by_hdl(audio_conn->handle);
		if (dev_info == NULL)
			return BT_STATUS_PAUSED;

		SYS_LOG_INF("hdl: 0x%x play_status: 0x%x\n",dev_info->hdl, dev_info->avrcp_ext_status);
		if (dev_info->avrcp_ext_status & BT_MANAGER_AVRCP_EXT_STATUS_PLAYING) {
			return BT_STATUS_PLAYING;
#ifdef CONFIG_LOGSRV_BT_DEBUG
		} else if (dev_info->a2dp_stream_started) {
			return BT_STATUS_PLAYING;
#endif
		} else {
			return BT_STATUS_PAUSED;
		}
	} else if (audio_conn->type == BT_TYPE_LE
				&& audio_conn->media_state == BT_STATUS_INACTIVE) {
		if (audio_conn->cis_connected) {
			return BT_STATUS_PLAYING;
		} else {
			return BT_STATUS_PAUSED;
		}
	}

	return audio_conn->media_state;
}

int bt_manager_media_get_local_passthrough_status(uint16_t handle)
{
	struct bt_audio_conn *audio_conn;
	bt_mgr_dev_info_t* dev_info;

	if (handle) {
		audio_conn = find_conn(handle);
	}else {
		audio_conn = find_active_slave();
	}

	if (!audio_conn) {
		return BT_STATUS_PAUSED;
	}

	//TODO:use audio_conn->media_state directly
	if (audio_conn->type == BT_TYPE_BR) {
		dev_info = bt_mgr_find_dev_info_by_hdl(audio_conn->handle);
		if (dev_info == NULL)
			return BT_STATUS_PAUSED;

		SYS_LOG_INF("hdl: 0x%x play_status: 0x%x\n",dev_info->hdl, dev_info->avrcp_ext_status);
		if (dev_info->avrcp_ext_status & BT_MANAGER_AVRCP_EXT_STATUS_PASSTHROUGH_PLAY) {
			return BT_STATUS_PLAYING;
		}else {
			return BT_STATUS_PAUSED;
		}
	} else if (audio_conn->type == BT_TYPE_LE
				&& audio_conn->media_state == BT_STATUS_INACTIVE) {
		if (audio_conn->cis_connected) {
			return BT_STATUS_PLAYING;
		} else {
			return BT_STATUS_PAUSED;
		}
	}

	return audio_conn->media_state;
}

static uint16_t media_active = 0;
int bt_manager_media_set_active_br_handle(uint16_t set)
{
	uint16_t active_hdl = 0;
	struct bt_audio_conn *audio_conn;

	if (!set) {
		active_hdl = 0;
	}else {
		audio_conn = find_active_slave();
		if (!audio_conn) {
			goto exit;
		}

		if(audio_conn->type == BT_TYPE_BR){
			active_hdl = audio_conn->handle;
		}
	}

exit:
	SYS_LOG_INF("%x, %x, %x",active_hdl, media_active, set);
	if ((media_active != active_hdl) || (!set)) {
		media_active = active_hdl;
		bt_manager_update_phone_volume(media_active, 1);
	}

	return 1;
}

uint16_t bt_manager_media_get_active_br_handle(void)
{
	return media_active;
}

static struct bt_audio_call_inst *new_bt_call(struct bt_audio_conn *audio_conn,
				uint8_t index)
{
	struct bt_audio_call_inst *call;
	sys_slist_t *list;

	if (is_bt_le_master(audio_conn)) {
		list = &bt_audio.cg_list;
	} else if (audio_conn->role == BT_ROLE_SLAVE) {
		list = &audio_conn->ct_list;
	} else {
		return NULL;
	}

	call = mem_malloc(sizeof(struct bt_audio_call_inst));
	if (!call) {
		SYS_LOG_ERR("no mem");
		return NULL;
	}

	call->handle = audio_conn->handle;
	call->index = index;

	os_sched_lock();
	sys_slist_append(list, &call->node);
	os_sched_unlock();

	return call;
}

static struct bt_audio_call_inst *find_bt_call(struct bt_audio_conn *audio_conn,
				uint8_t index)
{
	struct bt_audio_call_inst *call;
	sys_slist_t *list;

	if (is_bt_le_master(audio_conn)) {
		list = &bt_audio.cg_list;
	} else if (audio_conn->role == BT_ROLE_SLAVE) {
		list = &audio_conn->ct_list;
	} else {
		return NULL;
	}

	os_sched_lock();

	SYS_SLIST_FOR_EACH_CONTAINER(list, call, node) {
		if (call->index == index) {
			os_sched_unlock();
			return call;
		}
	}

	os_sched_unlock();

	return NULL;
}

/* if not found, create a new call */
static struct bt_audio_call_inst *find_new_bt_call(struct bt_audio_conn *audio_conn,
				uint8_t index)
{
	struct bt_audio_call_inst *call;

	call = find_bt_call(audio_conn, index);
	if (!call) {
		call = new_bt_call(audio_conn, index);
	}

	if (!call) {
		return NULL;
	}

	return call;
}

static int delete_bt_call(struct bt_audio_conn *audio_conn, uint8_t index)
{
	struct bt_audio_call_inst *call;
	sys_slist_t *list;

	if (is_bt_le_master(audio_conn)) {
		list = &bt_audio.cg_list;
	} else if (audio_conn->role == BT_ROLE_SLAVE) {
		list = &audio_conn->ct_list;
	} else {
		return -EINVAL;
	}

	call = find_bt_call(audio_conn, index);
	if (!call) {
		SYS_LOG_ERR("no call");
		return -ENODEV;
	}

	os_sched_lock();

	sys_slist_find_and_remove(list, &call->node);
	mem_free(call);

	os_sched_unlock();

	return 0;
}

int bt_manager_audio_call_info(struct bt_audio_call *call,
				struct bt_audio_call_info *info)
{
	struct bt_audio_call_inst *call_inst;
	struct bt_audio_conn *audio_conn;

	if (!info) {
		return -EINVAL;
	}

	audio_conn = find_conn(call->handle);
	if (!audio_conn) {
		SYS_LOG_ERR("no conn");
		return -ENODEV;
	}

	call_inst = find_bt_call(audio_conn, call->index);
	if (!call_inst) {
		SYS_LOG_ERR("no call");
		return -ENODEV;
	}

	os_sched_lock();

	info->state = call_inst->state;
	strncpy(info->remote_uri, call_inst->remote_uri,
		BT_AUDIO_CALL_URI_LEN - 1);

	os_sched_unlock();

	return 0;
}

int bt_manager_call_originate(uint16_t handle, uint8_t *remote_uri)
{
	struct bt_audio_conn *audio_conn;

	if (handle == 0) {
		audio_conn = find_active_slave();
	} else {
		audio_conn = find_conn(handle);
	}

	if (!audio_conn) {
		return -ENODEV;
	}

	if (audio_conn->type == BT_TYPE_LE) {
		return bt_manager_le_call_originate(handle, remote_uri);
	}

	return -EINVAL;
}

int bt_manager_call_accept(struct bt_audio_call *call)
{
	struct bt_audio_call_inst *call_inst;
	struct bt_audio_conn *audio_conn;
	struct bt_audio_call tmp;

	/* default: select current active call */
	if (!call) {
		audio_conn = find_active_slave();
		if (!audio_conn) {
			return -ENODEV;
		}

		if (audio_conn->type == BT_TYPE_BR) {
#ifdef CONFIG_BT_HFP_HF
			return bt_manager_hfp_accept_call();
#else
			return 0;
#endif
		} else if (audio_conn->type == BT_TYPE_LE) {
			/* FIXME: find a incoming call? */
			call_inst = find_slave_call(audio_conn, BT_CALL_STATE_INCOMING);
			if (!call_inst) {
				return -ENODEV;
			}

			tmp.handle = call_inst->handle;
			tmp.index = call_inst->index;
			return bt_manager_le_call_accept(&tmp);
		}

		return -EINVAL;
	}

	/* select by upper layer */

	audio_conn = find_conn(call->handle);
	if (!audio_conn) {
		return -EINVAL;
	}

	if (audio_conn->type == BT_TYPE_BR) {
#ifdef CONFIG_BT_HFP_HF
		return bt_manager_hfp_accept_call();
#else
		return 0;
#endif
	} else if (audio_conn->type == BT_TYPE_LE) {
		return bt_manager_le_call_accept(call);
	}

	return -EINVAL;
}

int bt_manager_call_hold(struct bt_audio_call *call)
{
	struct bt_audio_call_inst *call_inst;
	struct bt_audio_conn *audio_conn;
	struct bt_audio_call tmp;

	/* default: select current active call */
	if (!call) {
		audio_conn = find_active_slave();
		if (!audio_conn) {
			return -ENODEV;
		}

		if (audio_conn->type == BT_TYPE_BR) {
			return -EINVAL;
		} else if (audio_conn->type == BT_TYPE_LE) {
			/* FIXME: find a active call? */
			call_inst = find_slave_call(audio_conn, BT_CALL_STATE_ACTIVE);
			if (!call_inst) {
				return -ENODEV;
			}

			tmp.handle = call_inst->handle;
			tmp.index = call_inst->index;
			return bt_manager_le_call_hold(&tmp);
		}

		return -EINVAL;
	}

	/* select by upper layer */

	audio_conn = find_conn(call->handle);
	if (!audio_conn) {
		return -EINVAL;
	}

	if (audio_conn->type == BT_TYPE_BR) {
		return -EINVAL;
	} else if (audio_conn->type == BT_TYPE_LE) {
		return bt_manager_le_call_hold(call);
	}

	return -EINVAL;
}

int bt_manager_call_retrieve(struct bt_audio_call *call)
{
	struct bt_audio_call_inst *call_inst;
	struct bt_audio_conn *audio_conn;
	struct bt_audio_call tmp;

	/* default: select current active call */
	if (!call) {
		audio_conn = find_active_slave();
		if (!audio_conn) {
			return -ENODEV;
		}

		if (audio_conn->type == BT_TYPE_BR) {
			return -EINVAL;
		} else if (audio_conn->type == BT_TYPE_LE) {
			/* FIXME: find a locally held call? */
			call_inst = find_slave_call(audio_conn, BT_CALL_STATE_LOCALLY_HELD);
			if (!call_inst) {
				call_inst = find_slave_call(audio_conn, BT_CALL_STATE_HELD);
			}

			if (!call_inst) {
				return -ENODEV;
			}

			tmp.handle = call_inst->handle;
			tmp.index = call_inst->index;
			return bt_manager_le_call_retrieve(&tmp);
		}

		return -EINVAL;
	}

	/* select by upper layer */

	audio_conn = find_conn(call->handle);
	if (!audio_conn) {
		return -EINVAL;
	}

	if (audio_conn->type == BT_TYPE_BR) {
		return -EINVAL;
	} else if (audio_conn->type == BT_TYPE_LE) {
		return bt_manager_le_call_retrieve(call);
	}

	return -EINVAL;
}

int bt_manager_call_join(struct bt_audio_call **calls, uint8_t num_calls)
{
	return -EINVAL;
}

int bt_manager_call_terminate(struct bt_audio_call *call, uint8_t reason)
{
	struct bt_audio_call_inst *call_inst;
	struct bt_audio_conn *audio_conn;
	struct bt_audio_call tmp;

	/* default: select current active call */
	if (!call) {
		audio_conn = find_active_slave();
		if (!audio_conn) {
			return -ENODEV;
		}

		if (audio_conn->type == BT_TYPE_BR) {
#ifdef CONFIG_BT_HFP_HF
			return bt_manager_hfp_hangup_call();
#else
			return 0;
#endif
		} else if (audio_conn->type == BT_TYPE_LE) {
			/* FIXME: find a active call? */
			call_inst = find_slave_call(audio_conn, BT_CALL_STATE_ACTIVE);
			if (!call_inst) {
				/* FIXME: find the first call? */
				call_inst = find_fisrt_slave_call(audio_conn);
			}

			if (!call_inst) {
				return -ENODEV;
			}

			tmp.handle = call_inst->handle;
			tmp.index = call_inst->index;
			return bt_manager_le_call_terminate(&tmp);
		}

		return -EINVAL;
	}

	/* select by upper layer */

	audio_conn = find_conn(call->handle);
	if (!audio_conn) {
		return -EINVAL;
	}

	if (audio_conn->type == BT_TYPE_BR) {
#ifdef CONFIG_BT_HFP_HF
		return bt_manager_hfp_hangup_call();
#else
		return 0;
#endif
	} else if (audio_conn->type == BT_TYPE_LE) {
		return bt_manager_le_call_terminate(call);
	}

	return -EINVAL;
}

int bt_manager_call_hangup_another_call(void)
{
	struct bt_audio_conn *audio_conn;

	audio_conn = find_active_slave();
	if (!audio_conn) {
		return -EINVAL;
	}

	if (audio_conn->type == BT_TYPE_BR) {
#ifdef CONFIG_BT_HFP_HF
		return bt_manager_hfp_hangup_another_call();
#else
		return 0;
#endif
	}

	return -EINVAL;
}

int bt_manager_call_holdcur_answer_call(void)
{
	struct bt_audio_conn *audio_conn;

	audio_conn = find_active_slave();
	if (!audio_conn) {
		return -EINVAL;
	}

	if (audio_conn->type == BT_TYPE_BR) {
#ifdef CONFIG_BT_HFP_HF
		return bt_manager_hfp_holdcur_answer_call();
#else
		return 0;
#endif
	}

	return -EINVAL;
}

int bt_manager_call_hangupcur_answer_call(void)
{
	struct bt_audio_conn *audio_conn;

	audio_conn = find_active_slave();
	if (!audio_conn) {
		return -EINVAL;
	}

	if (audio_conn->type == BT_TYPE_BR) {
#ifdef CONFIG_BT_HFP_HF
		return bt_manager_hfp_hangupcur_answer_call();
#else
		return 0;
#endif
	}

	return -EINVAL;
}

int bt_manager_call_switch_sound_source(void)
{
	struct bt_audio_conn *audio_conn;

	audio_conn = find_active_slave();
	if (!audio_conn) {
		return -EINVAL;
	}

	if (audio_conn->type == BT_TYPE_BR) {
		return btif_hfp_hf_switch_sound_source();
	}

	return -EINVAL;
}

int bt_manager_call_allow_stream_connect(bool allowed)
{
	return bt_manager_allow_sco_connect(allowed);
}

int bt_manager_call_start_siri(void)
{
	struct bt_audio_conn *audio_conn;

	audio_conn = find_active_slave();
	if (!audio_conn) {
		return -EINVAL;
	}

	if (audio_conn->type == BT_TYPE_BR) {
#ifdef CONFIG_BT_HFP_HF
		return bt_manager_hfp_start_siri();
#else
		return 0;
#endif
	}

	return -EINVAL;
}

int bt_manager_call_stop_siri(void)
{
	struct bt_audio_conn *audio_conn;

	audio_conn = find_active_slave();
	if (!audio_conn) {
		return -EINVAL;
	}

	if (audio_conn->type == BT_TYPE_BR) {
#ifdef CONFIG_BT_HFP_HF
		return bt_manager_hfp_stop_siri();
#else
		return 0;
#endif
	}

	return -EINVAL;
}

int bt_manager_call_dial_last(void)
{
	struct bt_audio_conn *audio_conn;

	audio_conn = find_active_slave();
	if (!audio_conn) {
		return -EINVAL;
	}

	if (audio_conn->type == BT_TYPE_BR) {
#ifdef CONFIG_BT_HFP_HF
		return bt_manager_hfp_dial_last_number();
#else
		return 0;
#endif
	}

	return -EINVAL;
}

void bt_manager_call_event(int event, void *data, int size)
{
	struct bt_call_report *rep = (struct bt_call_report *)data;
	struct bt_audio_conn *audio_conn;
	struct bt_audio_call_inst *call;
	struct phone_num_param {
		struct bt_call_report rep;
		uint8_t data[20];
	} sparam;

	SYS_LOG_INF("%d", event);

	audio_conn = find_conn(rep->handle);
	if (!audio_conn) {
		SYS_LOG_ERR("no conn");
		return;
	}

	/* FIXME: support master later ... */
	if (audio_conn->role != BT_ROLE_SLAVE) {
		return;
	}

	if (audio_conn->type == BT_TYPE_BR) {
		if (event == BT_CALL_ENDED) {
			audio_conn->call_state = BT_STATUS_HFP_NONE;
			audio_conn->fake_contexts = 0;
			goto report;
		}

		switch (event) {
		case BT_CALL_RING_STATR_EVENT:
			memset(&sparam,0,sizeof(sparam));
			sparam.rep.handle = rep->handle;
			sparam.rep.index = 1;
			if (strlen(rep->uri) > sizeof(sparam.data) -1)
				memcpy(sparam.data,rep->uri,sizeof(sparam.data) -1);
			else
				memcpy(sparam.data,rep->uri,strlen(rep->uri));

			bt_manager_event_notify(BT_CALL_RING_STATR_EVENT, (void*)&sparam, sizeof(struct phone_num_param));
			return;

		case BT_CALL_INCOMING:
			audio_conn->call_state = BT_STATUS_INCOMING;
			break;
		case BT_CALL_DIALING:
			audio_conn->call_state = BT_STATUS_OUTGOING;
			break;
		case BT_CALL_ALERTING:
			audio_conn->call_state = BT_STATUS_OUTGOING;
			break;
		case BT_CALL_ACTIVE:
			audio_conn->call_state = BT_STATUS_ONGOING;
			break;
		case BT_CALL_3WAYIN:
			audio_conn->call_state = BT_STATUS_3WAYIN;
			break;
		case BT_CALL_MULTIPARTY:
			audio_conn->call_state = BT_STATUS_MULTIPARTY;
			break;
		case BT_CALL_3WAYOUT:
			audio_conn->call_state = BT_STATUS_3WAYOUT;
			break;
		case BT_CALL_SIRI_MODE:
			audio_conn->call_state = BT_STATUS_SIRI;
			break;
		default:
			break;
		}

		goto report;
	}

	/* LE Audio */
	if (event == BT_CALL_ENDED) {
		delete_bt_call(audio_conn, rep->index);
		goto report;
	}

	/* the first message may come with any state */
	call = find_new_bt_call(audio_conn, rep->index);
	if (!call) {
		SYS_LOG_ERR("no call");
		return;
	}

	switch (event) {
	case BT_CALL_INCOMING:
		call->state = BT_CALL_STATE_INCOMING;
		strncpy(call->remote_uri, rep->uri, BT_AUDIO_CALL_URI_LEN - 1);
		break;
	case BT_CALL_DIALING:
		call->state = BT_CALL_STATE_DIALING;
		break;
	case BT_CALL_ALERTING:
		call->state = BT_CALL_STATE_ALERTING;
		break;
	case BT_CALL_ACTIVE:
		call->state = BT_CALL_STATE_ACTIVE;
		break;
	case BT_CALL_LOCALLY_HELD:
		call->state = BT_CALL_STATE_LOCALLY_HELD;
		break;
	case BT_CALL_REMOTELY_HELD:
		call->state = BT_CALL_STATE_REMOTELY_HELD;
		break;
	case BT_CALL_HELD:
		call->state = BT_CALL_STATE_HELD;
		break;
	default:
		break;
	}

	/* report event */
report:
	if (audio_conn == get_active_slave()) {
		bt_manager_event_notify(event, data, size);
	} else {
		SYS_LOG_INF("drop msg %p %p", audio_conn, get_active_slave());
	}
}

static uint16_t get_le_call_status(struct bt_audio_conn *audio_conn)
{
	struct bt_audio_call_inst *call;
	uint16_t status;
	uint8_t count;

	status = BT_STATUS_HFP_NONE;

	os_sched_lock();

	if (sys_slist_is_empty(&audio_conn->ct_list)) {
		goto exit;
	}

	count = 0;
	SYS_SLIST_FOR_EACH_CONTAINER(&audio_conn->ct_list, call, node) {
		count++;
	}

	if (count == 1) {
		call = SYS_SLIST_PEEK_HEAD_CONTAINER(&audio_conn->ct_list, call, node);
		switch (call->state) {
		case BT_CALL_STATE_INCOMING:
			status = BT_STATUS_INCOMING;
			break;
		case BT_CALL_STATE_DIALING:
		case BT_CALL_STATE_ALERTING:
			status = BT_STATUS_OUTGOING;
			break;
		case BT_CALL_STATE_ACTIVE:
			status = BT_STATUS_ONGOING;
			break;
		/* FIXME: what if held */
		default:
			break;
		}
	} else if (count > 1) {
		status = BT_STATUS_MULTIPARTY;
	}

exit:
	os_sched_unlock();

	return status;
}

int bt_manager_call_get_status(void)
{
	struct bt_audio_conn *audio_conn;

	audio_conn = find_active_slave();
	if (!audio_conn) {
		return BT_STATUS_HFP_NONE;
	}

	if (audio_conn->type == BT_TYPE_BR) {
		return audio_conn->call_state;
	} else if (audio_conn->type == BT_TYPE_LE) {
		return get_le_call_status(audio_conn);
	}

	return BT_STATUS_HFP_NONE;
}

int bt_manager_volume_up(void)
{
	struct bt_audio_conn *audio_conn;

	audio_conn = find_active_slave();
	if (!audio_conn) {
		return -EINVAL;
	}

	if (audio_conn->type == BT_TYPE_LE) {
		return bt_manager_le_volume_up(audio_conn->handle);
	}

#ifdef CONFIG_VOLUME_MANAGER
	if (audio_conn->type == BT_TYPE_BR) {
		uint32_t music_vol = audio_system_get_stream_volume(AUDIO_STREAM_MUSIC);
		uint32_t max_volume = audio_policy_get_volume_level();

		if (music_vol >= max_volume) {
			return 0;
		}
		bt_manager_avrcp_sync_vol_to_local(audio_conn->handle,
						   (uint8_t)(music_vol + 1), false);
		return 0;
	}
#endif

	return -EINVAL;
}

int bt_manager_volume_down(void)
{
	struct bt_audio_conn *audio_conn;

	audio_conn = find_active_slave();
	if (!audio_conn) {
		return -EINVAL;
	}

	if (audio_conn->type == BT_TYPE_LE) {
		return bt_manager_le_volume_down(audio_conn->handle);
	}

#ifdef CONFIG_VOLUME_MANAGER
	if (audio_conn->type == BT_TYPE_BR) {
		uint32_t music_vol = audio_system_get_stream_volume(AUDIO_STREAM_MUSIC);

		if (music_vol == 0) {
			return 0;
		}
		bt_manager_avrcp_sync_vol_to_local(audio_conn->handle,
						   (uint8_t)(music_vol - 1), false);
		return 0;
	}
#endif

	return -EINVAL;
}

int bt_manager_audio_effect_switch(void)
{
	media_player_t *player;
	int en = property_get_int(CFG_AUDIO_EFFECT, 1);
	int next = en ? 0 : 1;

	property_set_int(CFG_AUDIO_EFFECT, next);

	player = media_player_get_current_dumpable_player();
	if (player) {
		return media_player_set_effect_enable(player, next != 0);
	}

	return 0;
}

int bt_manager_volume_set(uint8_t value,int type)
{
	struct bt_audio_conn *audio_conn;
	uint16_t media_handle = bt_manager_media_get_active_br_handle();

	if (media_handle) {
		audio_conn = find_conn(media_handle);
	}else {
		audio_conn = find_active_slave();
	}
	//SYS_LOG_INF("%p %d %d %d %x\n", audio_conn, audio_conn->type,type,value,media_handle);
	if (!audio_conn) {
		return -EINVAL;
	}

	if (audio_conn->type == BT_TYPE_LE) {
		if (type == BT_VOLUME_TYPE_LE_AUDIO) {
			if (bt_audio.le_audio_muted && value) {
				bt_manager_le_volume_unmute(audio_conn->handle);
				bt_audio.le_audio_muted = 0;
			}
			audio_conn->lea_vol = value;
			audio_conn->lea_vol_update = 1;

			return bt_manager_le_volume_set(audio_conn->handle, value);
		}
	} else if (audio_conn->type == BT_TYPE_BR) {
		if (audio_conn->call_state != BT_STATUS_HFP_NONE) {
#ifdef CONFIG_BT_HFP_HF
			if(type == BT_VOLUME_TYPE_BR_CALL)
				bt_manager_hfp_sync_vol_to_remote_by_addr(audio_conn->handle, value);
#endif
		} else {
			if (find_channel2(audio_conn, BT_AUDIO_ENDPOINT_CALL)){
#ifdef CONFIG_BT_HFP_HF
				if(type == BT_VOLUME_TYPE_BR_CALL)
					bt_manager_hfp_sync_vol_to_remote_by_addr(audio_conn->handle, value);
#endif
			}else{
				if(type == BT_VOLUME_TYPE_BR_MUSIC)
					bt_manager_avrcp_sync_vol_to_remote_by_addr(audio_conn->handle, value, 0);
			}
		}
	}

	return -EINVAL;
}

int bt_manager_volume_mute(void)
{
	struct bt_audio_conn *audio_conn;

	audio_conn = find_active_slave();
	if (!audio_conn) {
		return -EINVAL;
	}

	if (audio_conn->type == BT_TYPE_LE) {
		return bt_manager_le_volume_mute(audio_conn->handle);
	}

	return -EINVAL;
}

int bt_manager_volume_unmute(void)
{
	struct bt_audio_conn *audio_conn;

	audio_conn = find_active_slave();
	if (!audio_conn) {
		return -EINVAL;
	}

	if (audio_conn->type == BT_TYPE_LE) {
		return bt_manager_le_volume_unmute(audio_conn->handle);
	}

	return -EINVAL;
}

int bt_manager_volume_unmute_up(void)
{
	struct bt_audio_conn *audio_conn;

	audio_conn = find_active_slave();
	if (!audio_conn) {
		return -EINVAL;
	}

	if (audio_conn->type == BT_TYPE_LE) {
		return bt_manager_le_volume_unmute_up(audio_conn->handle);
	}

	return -EINVAL;
}

int bt_manager_volume_unmute_down(void)
{
	struct bt_audio_conn *audio_conn;

	audio_conn = find_active_slave();
	if (!audio_conn) {
		return -EINVAL;
	}

	if (audio_conn->type == BT_TYPE_LE) {
		return bt_manager_le_volume_down(audio_conn->handle);
	}

	return -EINVAL;
}

int bt_manager_volume_client_up(void)
{
	struct bt_audio_conn *audio_conn;

	/* for each master */
	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
		if (audio_conn->role != BT_ROLE_MASTER) {
			continue;
		}

		if (audio_conn->type == BT_TYPE_LE &&
		    audio_conn->vol_cli_connected) {
			bt_manager_le_volume_up(audio_conn->handle);
		}
	}

	return 0;
}

int bt_manager_volume_client_down(void)
{
	struct bt_audio_conn *audio_conn;

	/* for each master */
	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
		if (audio_conn->role != BT_ROLE_MASTER) {
			continue;
		}

		if (audio_conn->type == BT_TYPE_LE &&
		    audio_conn->vol_cli_connected) {
			bt_manager_le_volume_down(audio_conn->handle);
		}
	}

	return 0;
}

int bt_manager_volume_client_set(uint8_t value)
{
	struct bt_audio_conn *audio_conn;

	/* for each master */
	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
		if (audio_conn->role != BT_ROLE_MASTER) {
			continue;
		}

		if (audio_conn->type == BT_TYPE_LE &&
		    audio_conn->vol_cli_connected) {
			bt_manager_le_volume_set(audio_conn->handle, value);
		}
	}

	return 0;
}

int bt_manager_volume_client_mute(void)
{
	struct bt_audio_conn *audio_conn;

	/* for each master */
	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
		if (audio_conn->role != BT_ROLE_MASTER) {
			continue;
		}

		if (audio_conn->type == BT_TYPE_LE &&
		    audio_conn->vol_cli_connected) {
			bt_manager_le_volume_mute(audio_conn->handle);
		}
	}

	return 0;
}

int bt_manager_volume_client_unmute(void)
{
	struct bt_audio_conn *audio_conn;

	/* for each master */
	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
		if (audio_conn->role != BT_ROLE_MASTER) {
			continue;
		}

		if (audio_conn->type == BT_TYPE_LE &&
		    audio_conn->vol_cli_connected) {
			bt_manager_le_volume_unmute(audio_conn->handle);
		}
	}

	return 0;
}

int bt_manager_volume_client_unmute_up(void)
{
	struct bt_audio_conn *audio_conn;

	/* for each master */
	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
		if (audio_conn->role != BT_ROLE_MASTER) {
			continue;
		}

		if (audio_conn->type == BT_TYPE_LE &&
		    audio_conn->vol_cli_connected) {
			bt_manager_le_volume_unmute_up(audio_conn->handle);
		}
	}

	return 0;
}

int bt_manager_volume_client_unmute_down(void)
{
	struct bt_audio_conn *audio_conn;

	/* for each master */
	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
		if (audio_conn->role != BT_ROLE_MASTER) {
			continue;
		}

		if (audio_conn->type == BT_TYPE_LE &&
		    audio_conn->vol_cli_connected) {
			bt_manager_le_volume_unmute_down(audio_conn->handle);
		}
	}

	return 0;
}

static int volume_client_connected(void *data, int size)
{
	struct bt_volume_report *rep = (struct bt_volume_report *)data;
	struct bt_audio_conn *audio_conn;

	audio_conn = find_conn(rep->handle);
	if (!audio_conn) {
		SYS_LOG_ERR("no conn");
		return -ENODEV;
	}

	audio_conn->vol_cli_connected = 1;

	return 0;
}

void bt_manager_volume_event(int event, void *data, int size)
{
	SYS_LOG_INF("%d", event);

	if (event == BT_VOLUME_CLIENT_CONNECTED) {
		volume_client_connected(data, size);
	} else {
		/* only active dev can change volume */
		struct bt_volume_report *rep = (struct bt_volume_report *)data;
		struct bt_audio_conn *audio_conn;

		audio_conn = find_conn(rep->handle);
		if (!audio_conn) {
			SYS_LOG_ERR("no conn");
			return;
		}

		switch (event) {
			case BT_VOLUME_VALUE:
			{
				int vol_new = bt_manager_lea_to_music_volume((int)rep->value);				
				int vol_old = bt_manager_lea_to_music_volume((int)audio_conn->lea_vol);

				if(!audio_conn->lea_vol_sync_from_remote && rep->value == 0xff){
					audio_conn->lea_vol_sync_from_remote = 1;
					int vol = system_volume_get(AUDIO_STREAM_LE_AUDIO);
					SYS_LOG_INF("vol: %d", vol);
					if(vol < audio_policy_get_volume_level() - 4){
						SYS_LOG_INF("drop max vol level\n");
						vol = bt_manager_music_to_lea_volume(vol);
						bt_manager_le_volume_set(audio_conn->handle, vol);
						return;
					}
				}
				if (audio_conn->lea_vol != rep->value) {
					audio_conn->lea_vol = rep->value;
					audio_conn->lea_vol_update = 1;
				}

				if (audio_conn->lea_vol_update && vol_old == vol_new) {
					SYS_LOG_INF("vol level not chg:%d\n", vol_old);
					audio_conn->lea_vol_sync_from_remote = 1;
					return;
				}

				if (audio_conn == find_active_slave()) {
					system_player_volume_set(AUDIO_STREAM_LE_AUDIO, vol_new);
				}

				break;
			}
			case BT_VOLUME_MUTE:
			{
				bt_audio.le_audio_muted = 1;
				break;
			}
			case BT_VOLUME_UNMUTE:
			{
				bt_audio.le_audio_muted = 0;
				break;
			}

			default:
			break;
		}
		audio_conn->lea_vol_sync_from_remote = 1;
	}

	bt_manager_event_notify(event, data, size);
}

void bt_manager_volume_event_to_app(uint16_t handle, uint8_t volume, uint8_t from_phone)
{
	struct bt_volume_report rep;
	rep.handle = handle;
	rep.value = volume;
	rep.from_phone = from_phone;

	bt_manager_event_notify(BT_VOLUME_VALUE,&rep, sizeof(rep));
}

int bt_manager_mic_mute(void)
{
	struct bt_audio_conn *audio_conn;

	audio_conn = find_active_slave();
	if (!audio_conn) {
		return -EINVAL;
	}

	if (audio_conn->type == BT_TYPE_LE) {
		return bt_manager_le_mic_mute(audio_conn->handle);
	}

	return -EINVAL;
}

int bt_manager_mic_unmute(void)
{
	struct bt_audio_conn *audio_conn;

	audio_conn = find_active_slave();
	if (!audio_conn) {
		return -EINVAL;
	}

	if (audio_conn->type == BT_TYPE_LE) {
		return bt_manager_le_mic_unmute(audio_conn->handle);
	}

	return -EINVAL;
}

int bt_manager_mic_mute_disable(void)
{
	struct bt_audio_conn *audio_conn;

	audio_conn = find_active_slave();
	if (!audio_conn) {
		return -EINVAL;
	}

	if (audio_conn->type == BT_TYPE_LE) {
		return bt_manager_le_mic_mute_disable(audio_conn->handle);
	}

	return -EINVAL;
}

int bt_manager_mic_gain_set(uint8_t gain)
{
	struct bt_audio_conn *audio_conn;

	audio_conn = find_active_slave();
	if (!audio_conn) {
		return -EINVAL;
	}

	if (audio_conn->type == BT_TYPE_LE) {
		return bt_manager_le_mic_gain_set(audio_conn->handle, gain);
	}

	return -EINVAL;
}

int bt_manager_mic_client_mute(void)
{
	struct bt_audio_conn *audio_conn;

	/* for each master */
	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
		if (audio_conn->role != BT_ROLE_MASTER) {
			continue;
		}

		if (audio_conn->type == BT_TYPE_LE &&
		    audio_conn->mic_cli_connected) {
			bt_manager_le_mic_mute(audio_conn->handle);
		}
	}

	return 0;
}

int bt_manager_mic_client_unmute(void)
{
	struct bt_audio_conn *audio_conn;

	/* for each master */
	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
		if (audio_conn->role != BT_ROLE_MASTER) {
			continue;
		}

		if (audio_conn->type == BT_TYPE_LE &&
		    audio_conn->mic_cli_connected) {
			bt_manager_le_mic_unmute(audio_conn->handle);
		}
	}

	return 0;
}

int bt_manager_mic_client_gain_set(uint8_t gain)
{
	struct bt_audio_conn *audio_conn;

	/* for each master */
	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
		if (audio_conn->role != BT_ROLE_MASTER) {
			continue;
		}

		if (audio_conn->type == BT_TYPE_LE &&
		    audio_conn->mic_cli_connected) {
			bt_manager_le_mic_gain_set(audio_conn->handle, gain);
		}
	}

	return 0;
}

static int microphone_client_connected(void *data, int size)
{
	struct bt_mic_report *rep = (struct bt_mic_report *)data;
	struct bt_audio_conn *audio_conn;

	audio_conn = find_conn(rep->handle);
	if (!audio_conn) {
		SYS_LOG_ERR("no conn");
		return -ENODEV;
	}

	audio_conn->mic_cli_connected = 1;

	return 0;
}

void bt_manager_mic_event(int event, void *data, int size)
{
	SYS_LOG_INF("%d", event);

	if (event == BT_MIC_CLIENT_CONNECTED) {
		microphone_client_connected(data, size);
	}

	bt_manager_event_notify(event, data, size);
}

__ramfunc static struct bt_broadcast_conn *find_broadcast(uint32_t handle)
{
	struct bt_broadcast_conn *broadcast;

	os_sched_lock();

	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.broadcast_list, broadcast, node) {
		if (broadcast->handle == handle) {
			os_sched_unlock();
			return broadcast;
		}
	}

	os_sched_unlock();

	return NULL;
}

static struct bt_broadcast_conn *find_broadcast2(uint32_t handle, uint8_t role)
{
	struct bt_broadcast_conn *broadcast;

	os_sched_lock();

	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.broadcast_list, broadcast, node) {
		if (broadcast->handle == handle && broadcast->role == role) {
			os_sched_unlock();
			return broadcast;
		}
	}

	os_sched_unlock();

	return NULL;
}

static struct bt_broadcast_conn *new_broadcast(uint32_t handle, uint8_t role)
{
	struct bt_broadcast_conn *broadcast;

	broadcast = mem_malloc(sizeof(struct bt_broadcast_conn));
	if (!broadcast) {
		SYS_LOG_ERR("no mem");
		return NULL;
	}

	broadcast->handle = handle;
	broadcast->role = role;

	os_sched_lock();
	sys_slist_append(&bt_audio.broadcast_list, &broadcast->node);
	os_sched_unlock();

	SYS_LOG_INF("handle: 0x%x, role: %d\n", handle, role);

	return broadcast;
}

static int delete_broadcast(uint32_t handle, uint8_t role)
{
	struct bt_broadcast_channel *channel, *ch;
	struct bt_broadcast_conn *broadcast;

	broadcast = find_broadcast2(handle,role);
	if (!broadcast) {
		SYS_LOG_ERR("not found (0x%x)", handle);
		return -ENODEV;
	}

	SYS_LOG_INF("handle: 0x%x\n", handle);

	os_sched_lock();

	/* remove all channel */
	SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&broadcast->chan_list, channel, ch, node) {
		if (channel->period_tl) {
			timeline_release(channel->period_tl);
		}
		sys_slist_remove(&broadcast->chan_list, NULL, &channel->node);
		mem_free(channel);
	}

	sys_slist_find_and_remove(&bt_audio.broadcast_list, &broadcast->node);

	mem_free(broadcast);

	os_sched_unlock();

	return 0;
}

__ramfunc static struct bt_broadcast_channel *find_broadcast_channel(uint32_t handle, uint8_t id)
{
	struct bt_broadcast_conn *broadcast;
	struct bt_broadcast_channel *channel;

	broadcast = find_broadcast(handle);
	if (!broadcast) {
		return NULL;
	}

	os_sched_lock();

	SYS_SLIST_FOR_EACH_CONTAINER(&broadcast->chan_list, channel, node) {
		if (channel->id == id) {
			os_sched_unlock();
			return channel;
		}
	}

	os_sched_unlock();

	return NULL;
}

static struct bt_broadcast_channel *find_broadcast_channel2(struct bt_broadcast_conn *broadcast, uint8_t id)
{
	struct bt_broadcast_channel *channel;

	os_sched_lock();

	SYS_SLIST_FOR_EACH_CONTAINER(&broadcast->chan_list, channel, node) {
		if (channel->id == id) {
			os_sched_unlock();
			return channel;
		}
	}

	os_sched_unlock();

	return NULL;
}

static struct bt_broadcast_channel *new_broadcast_channel(struct bt_broadcast_conn *broadcast)
{
	struct bt_broadcast_channel *channel;

	channel = mem_malloc(sizeof(struct bt_broadcast_channel));
	if (!channel) {
		SYS_LOG_ERR("no mem");
		return NULL;
	}

	os_sched_lock();
	sys_slist_append(&broadcast->chan_list, &channel->node);
	os_sched_unlock();

	return channel;
}

int bt_manager_broadcast_get_role(uint32_t handle)
{
	struct bt_broadcast_conn *broadcast;

	broadcast = find_broadcast(handle);
	if (!broadcast) {
		SYS_LOG_ERR("no conn");
		return -ENODEV;
	}

	return broadcast->role;
}

int8_t bt_manager_broadcast_get_sq_mode(void)
{
	return broadcast_sq_mode;
}

/*
 0:disable
 1:enable
*/
int8_t bt_manager_broadcast_set_sq_mode(int8_t sq_mode)
{
	broadcast_sq_mode = sq_mode;
	SYS_LOG_INF("set sq mode:%d",broadcast_sq_mode);
	return 0;
}

int bt_manager_broadcast_get_audio_handle(uint32_t handle,uint8_t id)
{
	struct bt_broadcast_channel *channel;

	channel = find_broadcast_channel(handle, id);
	if (!channel) {
		SYS_LOG_ERR("no chan\n");
		return -ENODEV;
	}

	return channel->audio_handle;
}

int bt_manager_broadcast_stream_state(uint32_t handle, uint8_t id)
{
	struct bt_broadcast_channel *channel;

	channel = find_broadcast_channel(handle, id);
	if (!channel) {
		SYS_LOG_ERR("no chan");
		return -ENODEV;
	}

	SYS_LOG_INF("%d", channel->state);

	return channel->state;
}

int bt_manager_broadcast_stream_info(struct bt_broadcast_chan *chan,
				struct bt_broadcast_chan_info *info)
{
	struct bt_broadcast_channel *channel;

	if (!chan || !info) {
		return -EINVAL;
	}

	channel = find_broadcast_channel(chan->handle, chan->id);
	if (!channel) {
		SYS_LOG_ERR("no chan\n");
		return -ENODEV;
	}

	os_sched_lock();

	info->format = channel->format;
	info->channels = channel->channels;
	info->octets = channel->octets;
	info->sample = channel->sample;
	info->interval = channel->interval;
	info->kbps = channel->kbps;
	info->sdu = channel->sdu;
	info->locations = channel->locations;
	info->contexts = channel->audio_contexts;

#if 0
	info->target_latency = channel->target_latency;
#endif
	info->duration = channel->duration;
	info->max_latency = channel->max_latency;
	info->delay = channel->delay;
	info->rtn = channel->rtn;
	info->phy = channel->phy;
	info->framing = channel->framing;
	info->chan = channel;

	os_sched_unlock();

	return 0;
}

__ramfunc static void bis_stream_trigger(uint32_t handle, uint8_t id, uint16_t audio_handle)
{
	struct bt_broadcast_channel *channel;

	channel = find_broadcast_channel(handle, id);
	if (!channel) {
		return;
	}

	if (channel->start_cb && !channel->start) {
		int offset = (channel->sync_delay + channel->delay - channel->media_delay_us)
			% channel->iso_interval;
		struct le_link_time time = {0, 0, 0};
		ctrl_get_le_link_time(audio_handle, &time);
		if(time.ce_offs_us < 0)
			time.ce_offs_us += channel->iso_interval;
		channel->start_cb(channel->start_cb_param,offset - time.ce_offs_us);
		channel->start = 1;
		//printk("sss %d %d %d %d\n",time.ce_offs_us,channel->sync_delay, channel->delay - channel->media_delay_us,channel->iso_interval);

	}

	if (channel->trigger) {
		channel->trigger(handle, id, audio_handle);
	}
}

__ramfunc static void bis_stream_period_trigger(uint32_t handle, uint8_t id,
				uint16_t audio_handle)
{
	struct bt_broadcast_channel *channel;

	channel = find_broadcast_channel(handle, id);
	if (!channel) {
		return;
	}

	if(channel->period_tl){
		timeline_trigger_listener(channel->period_tl);
	}
}

__ramfunc static void bis_stream_sync_trigger(uint32_t handle, uint8_t id,
				uint16_t audio_handle)
{
	struct bt_broadcast_channel *channel;

	channel = find_broadcast_channel(handle, id);
	if (!channel) {
		return;
	}

	if (channel->tws_start_cb && !channel->tws_start) {
		int offset = (channel->sync_delay + channel->tws_sync_offset)
			% channel->iso_interval;
		struct le_link_time time = {0, 0, 0};
		ctrl_get_le_link_time(audio_handle, &time);
		if(time.ce_offs_us < 0)
			time.ce_offs_us += channel->iso_interval;
		channel->tws_start_cb(channel->tws_start_cb_param,offset - time.ce_offs_us);
		channel->tws_start = 1;
		printk("tws %d %d %d\n",time.ce_offs_us,channel->sync_delay + channel->tws_sync_offset,channel->iso_interval);
	}

	if (channel->tws_sync_trigger) {
		channel->tws_sync_trigger(handle, id, audio_handle);
	}
}

static const struct bt_broadcast_stream_cb bis_stream_cb = {
	.tx_trigger = bis_stream_trigger,
	.tx_period_trigger = bis_stream_period_trigger,
	.rx_trigger = bis_stream_trigger,
	.rx_period_trigger = bis_stream_period_trigger,
	.tws_sync_trigger = bis_stream_sync_trigger,
};

int64_t bt_manager_broadcast_get_current_time(void *tl){
	if(tl){
		struct bt_broadcast_channel *chan = (struct bt_broadcast_channel *)timeline_get_owner(tl);
		if(chan && chan->audio_handle)
			return (int64_t)bt_manager_audio_get_le_time(chan->audio_handle);
		else
			return 0;
	} else
		return -1;
}

int bt_manager_broadcast_stream_set_media_delay(struct bt_broadcast_chan *chan,
				int32_t delay_us){

	struct bt_broadcast_conn *broadcast;
	struct bt_broadcast_channel *channel;

	if (!chan) {
		return -EINVAL;
	}

	broadcast = find_broadcast(chan->handle);
	if (!broadcast) {
		return -EINVAL;
	}

	channel = find_broadcast_channel2(broadcast, chan->id);
	if (!channel) {
		return -EINVAL;
	}

	channel->media_delay_us = delay_us;

	btif_broadcast_stream_set_media_delay(chan,delay_us);

	SYS_LOG_INF("%p delay %d\n", channel,delay_us);

	return 0;
}

int bt_manager_broadcast_stream_cb_register(struct bt_broadcast_chan *chan,
				bt_broadcast_trigger_cb trigger,
				bt_broadcast_trigger_cb period_trigger)
{
	struct bt_broadcast_conn *broadcast;
	struct bt_broadcast_channel *channel;

	if (!chan) {
		return -EINVAL;
	}

	if (!trigger) {
		return -EINVAL;
	}

	broadcast = find_broadcast(chan->handle);
	if (!broadcast) {
		return -EINVAL;
	}

	channel = find_broadcast_channel2(broadcast, chan->id);
	if (!channel) {
		return -EINVAL;
	}

	if (channel->trigger) {
		return -EINVAL;
	}

	os_sched_lock();
	channel->trigger = trigger;
	os_sched_unlock();

	return 0;
}

int bt_manager_broadcast_stream_start_cb_register(struct bt_broadcast_chan *chan,
				bt_audio_start_cb start_cb,void* param){

	struct bt_broadcast_conn *broadcast;
	struct bt_broadcast_channel *channel;

	if (!chan) {
		return -EINVAL;
	}

	broadcast = find_broadcast(chan->handle);
	if (!broadcast) {
		return -EINVAL;
	}

	channel = find_broadcast_channel2(broadcast, chan->id);
	if (!channel) {
		return -EINVAL;
	}

	uint32_t flags = irq_lock();
	channel->start_cb = start_cb;
	channel->start_cb_param = param;
	channel->start = 0;
	irq_unlock(flags);

	SYS_LOG_INF("%p\n", channel);

	return 0;
}

int bt_manager_broadcast_stream_tws_sync_cb_register(struct bt_broadcast_chan *chan,
				bt_broadcast_trigger_cb tws_sync_trigger)
{
	struct bt_broadcast_conn *broadcast;
	struct bt_broadcast_channel *channel;

	if (!chan) {
		return -EINVAL;
	}

	if (!tws_sync_trigger) {
		return -EINVAL;
	}

	broadcast = find_broadcast2(chan->handle,BT_ROLE_BROADCASTER);
	if (!broadcast) {
		return -EINVAL;
	}

	channel = find_broadcast_channel2(broadcast, chan->id);
	if (!channel) {
		return -EINVAL;
	}

	if (channel->tws_sync_trigger) {
		return -EINVAL;
	}

	channel->tws_sync_trigger = tws_sync_trigger;

	return 0;
}

int bt_manager_broadcast_stream_tws_sync_cb_register_1(struct bt_broadcast_chan *chan,
				bt_audio_start_cb tws_start_cb,void* param){

	struct bt_broadcast_conn *broadcast;
	struct bt_broadcast_channel *channel;

	if (!chan) {
		return -EINVAL;
	}

	broadcast = find_broadcast2(chan->handle,BT_ROLE_BROADCASTER);
	if (!broadcast) {
		return -EINVAL;
	}

	channel = find_broadcast_channel2(broadcast, chan->id);
	if (!channel) {
		return -EINVAL;
	}

	uint32_t flags = irq_lock();
	channel->tws_start_cb = tws_start_cb;
	channel->tws_start_cb_param = param;
	channel->tws_start = 0;
	irq_unlock(flags);

	return 0;
}

int bt_manager_broadcast_stream_set_tws_sync_cb_offset(struct bt_broadcast_chan *chan,
				int32_t offset_from_syncdelay)
{
	struct bt_broadcast_conn *broadcast;
	struct bt_broadcast_channel *channel;

	if (!chan) {
		return -EINVAL;
	}

	broadcast = find_broadcast(chan->handle);
	if (!broadcast) {
		return -EINVAL;
	}

	channel = find_broadcast_channel2(broadcast, chan->id);
	if (!channel) {
		return -EINVAL;
	}

	channel->tws_sync_offset = offset_from_syncdelay;

	btif_broadcast_stream_set_tws_sync_cb_offset(chan,offset_from_syncdelay);
	return 0;
}

int bt_manager_broadcast_stream_restore(void)
{
	return 0;	// TODO:
}

int bt_manager_broadcast_source_create(struct bt_broadcast_source_create_param *param)
{
	if (!param || !param->qos || !param->num_subgroups || !param->subgroup) {
		return -EINVAL;
	}

	struct bt_broadcast_conn *broadcast = find_broadcast2(param->broadcast_id,BT_ROLE_BROADCASTER);
	if(broadcast){
		if(broadcast->is_stopping)
			SYS_LOG_ERR("wait last broadcast releasing\n");
		else
			SYS_LOG_ERR("already exist\n");
		return -EINVAL;
	}

	return btif_broadcast_source_create(param);
}

int bt_manager_broadcast_source_reconfig(void)
{
	return 0;	// TODO:
}

int bt_manager_broadcast_source_enable(uint32_t handle)
{
	return btif_broadcast_source_enable(handle);
}

int bt_manager_broadcast_source_update(void)
{
	return 0;
}

int bt_manager_broadcast_source_disable(uint32_t handle)
{
	struct bt_broadcast_conn *broadcast;
	os_sched_lock();
	broadcast = find_broadcast2(handle,BT_ROLE_BROADCASTER);
	if (!broadcast) {
		os_sched_unlock();
		SYS_LOG_ERR("no conn (0x%x)", handle);
		return -ENODEV;
	}
	broadcast->is_stopping = 1;
	os_sched_unlock();

	return btif_broadcast_source_disable(handle);
}

int bt_manager_broadcast_source_release(uint32_t handle)
{
	struct bt_broadcast_conn *broadcast;
	os_sched_lock();
	broadcast = find_broadcast2(handle,BT_ROLE_BROADCASTER);
	if (!broadcast) {
		os_sched_unlock();
		SYS_LOG_ERR("no conn (0x%x)", handle);
		return -ENODEV;
	}
	broadcast->is_stopping = 1;
	os_sched_unlock();

	return btif_broadcast_source_release(handle);
}

int bt_manager_broadcast_source_stream_set(struct bt_broadcast_chan *chan,
				io_stream_t stream)
{
	return btif_broadcast_source_stream_set(chan, stream);
}

int bt_manager_broadcast_source_vnd_ext_send(uint32_t handle, uint8_t *buf,
				uint16_t len, uint8_t type)
{
	return btif_broadcast_source_ext_adv_send(handle, buf, len, type);
}

int bt_manager_broadcast_source_vnd_per_send(uint32_t handle, uint8_t *buf,
				uint16_t len, uint8_t type)
{
	return btif_broadcast_source_per_adv_send(handle, buf, len, type);
}

int bt_manager_broadcast_source_vnd_control_send(uint32_t handle, uint8_t *buf,
				uint16_t len)
{
	return 0;
}

int bt_manager_broadcast_sink_init()
{
	os_sched_lock();
	bt_audio.broadcast_sink_enable = 1;
	os_sched_unlock();
	return 0;
}

int bt_manager_broadcast_sink_exit()
{
	SYS_LOG_INF("\n");
	os_sched_lock();
	if(bt_audio.broadcast_sink_scan_enable){
		bt_manager_broadcast_scan_stop();
		bt_audio.broadcast_sink_scan_enable = 0;
	}
	struct bt_broadcast_conn *broadcast;

	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.broadcast_list, broadcast, node) {
		if (broadcast->role == BT_ROLE_RECEIVER) {
			bt_manager_broadcast_sink_release(broadcast->handle);
		}
	}
	bt_audio.broadcast_sink_enable = 0;
	os_sched_unlock();
	return 0;
}

int bt_manager_broadcast_sink_sync_pa(uint32_t handle)
{
	//return btif_broadcast_sink_sync(handle);
	return 0;
}

int bt_manager_broadcast_sink_sync(uint32_t handle, uint32_t bis_indexes,
				const uint8_t broadcast_code[16])
{
	return btif_broadcast_sink_sync(handle, bis_indexes, broadcast_code);
}

int bt_manager_broadcast_sink_release(uint32_t handle)
{
	SYS_LOG_INF("handle: 0x%x\n", handle);
	return btif_broadcast_sink_release(handle);
}

int bt_manager_broadcast_sink_stream_set(struct bt_broadcast_chan *chan,
				io_stream_t stream)
{
	return btif_broadcast_sink_stream_set(chan, stream);
}

int bt_manager_broadcast_sink_register_ext_rx_cb(bt_broadcast_vnd_rx_cb cb)
{
	return btif_broadcast_sink_register_ext_rx_cb(cb);
}

int bt_manager_broadcast_sink_register_per_rx_cb(uint32_t handle,
				bt_broadcast_vnd_rx_cb cb)
{
	return btif_broadcast_sink_register_per_rx_cb(handle, cb);
}

int bt_manager_broadcast_source_register_per_tx_cb(uint32_t handle,
				bt_broadcast_vnd_rx_cb cb)
{
    return btif_broadcast_source_register_per_tx_cb(handle, cb);
}				

int bt_manager_broadcast_sink_sync_loacations(uint32_t loacations)
{
	return btif_broadcast_sink_sync_loacations(loacations);
}

int bt_manager_broadcast_register_scan_filter_cb(bt_broadcast_scan_filter_cb cb)
{
    return btif_broadcast_register_scan_filter_cb(cb);
}

int bt_manager_broadcast_filter_local_name(uint8_t *name)
{
	return btif_broadcast_filter_local_name(name);
}

int bt_manager_broadcast_filter_broadcast_name(uint8_t *name)
{
	return btif_broadcast_filter_broadcast_name(name);
}

int bt_manager_broadcast_filter_broadcast_id(uint32_t id)
{
	return btif_broadcast_filter_broadcast_id(id);
}

int bt_manager_broadcast_filter_company_id(uint32_t id)
{
	return btif_broadcast_filter_company_id(id);
}

int bt_manager_broadcast_filter_uuid16(uint16_t value)
{
	return btif_broadcast_filter_uuid16(value);
}

int bt_manager_broadcast_filter_rssi(int8_t rssi)
{
	return btif_broadcast_filter_rssi(rssi);
}

int bt_manager_broadcast_clear_filter(void)
{
	return btif_broadcast_clear_filter();
}

int bt_manager_broadcast_scan_param_init(struct bt_le_scan_param *param)
{
	/* If user param is NULL, default param will be used. */
	return btif_broadcast_scan_param_init(param);
}

int bt_manager_broadcast_scan_start(void)
{
    struct bt_manager_context_t *bt_manager = bt_manager_get_context();
	bt_manager->total_per_sync_count++;
	os_sched_lock();
	bt_audio.broadcast_sink_scan_enable = 1;
	os_sched_unlock();
	return btif_broadcast_scan_start();
}

int bt_manager_broadcast_scan_stop(void)
{
    struct bt_manager_context_t *bt_manager = bt_manager_get_context();
	if (bt_manager->total_per_sync_count) {
		bt_manager->total_per_sync_count--;
	}
	os_sched_lock();
	bt_audio.broadcast_sink_scan_enable = 0;
	os_sched_unlock();
	return btif_broadcast_scan_stop();
}

int bt_manager_broadcast_pa_set_info_transfer(uint32_t handle)
{
	return btif_broadcast_pa_set_info_transfer(handle);
}

int bt_manager_broadcast_past_subscribe(uint16_t handle)
{
	return btif_broadcast_past_subscribe(handle);
}

int bt_manager_broadcast_past_unsubscribe(uint16_t handle)
{
	return btif_broadcast_past_unsubscribe(handle);
}

static int broadcast_config(bool source, void *data, int size)
{
	struct bt_broadcast_channel *channel;
	struct bt_braodcast_configured *rep;
	struct bt_broadcast_conn *broadcast;
	uint16_t duration;
	uint8_t role;

	rep = (struct bt_braodcast_configured *)data;

	os_sched_lock();
	if(!source && !bt_audio.broadcast_sink_enable){
		bt_manager_broadcast_sink_release(rep->handle);
		os_sched_unlock();
		SYS_LOG_INF("release broadcast if sink disabled\n");
		return 0;
	}

	if (source) {
		role = BT_ROLE_BROADCASTER;
	} else {
		role = BT_ROLE_RECEIVER;
	}

	broadcast = find_broadcast2(rep->handle,role);
	if (!broadcast) {
		broadcast = new_broadcast(rep->handle, role);
		if (!broadcast) {
			SYS_LOG_ERR("no conn");
			os_sched_unlock();
			return -ENOMEM;
		}
	}

	channel = new_broadcast_channel(broadcast);
	if (!channel) {
		SYS_LOG_ERR("no chan");
		os_sched_unlock();
		return -ENOMEM;
	}

	channel->format = rep->format;
	channel->id = rep->id;
	channel->state = BT_BROADCAST_STREAM_CONFIGURED;
	channel->duration = bt_frame_duration_to_ms(rep->frame_duration);
	channel->sample = rep->sample_rate;
	channel->octets = rep->octets;
	channel->locations = rep->locations;
	channel->channels = bt_channels_by_locations(rep->locations);

	channel->interval = rep->interval;
	channel->framing = rep->framing;
	channel->delay = rep->delay;
	channel->sdu = rep->max_sdu;
	channel->phy = rep->phy;
	channel->rtn = rep->rtn;

	if (channel->duration == 7) {
		duration = 7500;
	} else {
		duration = channel->duration * 1000;
	}
	channel->kbps = get_kbps(channel->sample, channel->sdu,
				duration, channel->channels);
	os_sched_unlock();

	SYS_LOG_INF("%d : %d : %d : %d : %d\n",
			channel->sample, channel->sdu,channel->channels,duration,
			channel->kbps);
    SYS_EVENT_INF(EVENT_BTMGR_BROAD_CONFIG,0,channel->sample,
            channel->kbps);

	if (source) {
		return bt_manager_event_notify(BT_BROADCAST_SOURCE_CONFIG, data, size);
	}

	return bt_manager_event_notify(BT_BROADCAST_SINK_CONFIG, data, size);
}

static int broadcast_source_enable(void *data, int size)
{
	struct bt_broadcast_channel *channel;
	struct bt_broadcast_conn *broadcast;
	struct bt_broadcast_report *rep;

	rep = (struct bt_broadcast_report *)data;

	broadcast = find_broadcast2(rep->handle,BT_ROLE_BROADCASTER);
	if (!broadcast) {
		SYS_LOG_ERR("no conn (0x%x)", rep->handle);
		return -ENODEV;
	}

	channel = find_broadcast_channel(rep->handle, rep->id);
	if (!channel) {
		SYS_LOG_ERR("no chan");
		return -ENODEV;
	}

	channel->state = BT_BROADCAST_STREAM_STREAMING;
	channel->audio_contexts = rep->audio_contexts;
    SYS_EVENT_INF(EVENT_BTMGR_BROAD_SOURCE_ENABLE,0,0);
	return bt_manager_event_notify(BT_BROADCAST_SOURCE_ENABLE, data, size);
}

static int broadcast_source_update(void *data, int size)
{
	struct bt_broadcast_channel *channel;
	struct bt_broadcast_conn *broadcast;
	struct bt_broadcast_report *rep;

	rep = (struct bt_broadcast_report *)data;

	broadcast = find_broadcast2(rep->handle,BT_ROLE_BROADCASTER);
	if (!broadcast) {
		SYS_LOG_ERR("no conn (0x%x)", rep->handle);
		return -ENODEV;
	}

	channel = find_broadcast_channel(rep->handle, rep->id);
	if (!channel) {
		SYS_LOG_ERR("no chan");
		return -ENODEV;
	}

	channel->audio_contexts = rep->audio_contexts;

	return bt_manager_event_notify(BT_BROADCAST_SOURCE_UPDATE, data, size);
}

static int broadcast_source_disable(void *data, int size)
{
	struct bt_broadcast_channel *channel;
	struct bt_broadcast_conn *broadcast;
	struct bt_broadcast_report *rep;

	rep = (struct bt_broadcast_report *)data;

	broadcast = find_broadcast2(rep->handle,BT_ROLE_BROADCASTER);
	if (!broadcast) {
		SYS_LOG_ERR("no conn (0x%x)", rep->handle);
		return -ENODEV;
	}

	channel = find_broadcast_channel(rep->handle, rep->id);
	if (!channel) {
		SYS_LOG_ERR("no chan");
		return -ENODEV;
	}

	channel->state = BT_BROADCAST_STREAM_CONFIGURED;
    SYS_EVENT_INF(EVENT_BTMGR_BROAD_SOURCE_DISABLE,0,0);
	return bt_manager_event_notify(BT_BROADCAST_SOURCE_DISABLE, data, size);
}

static int broadcast_source_release(void *data, int size)
{
	struct bt_broadcast_report *rep;

	rep = (struct bt_broadcast_report *)data;

	delete_broadcast(rep->handle,BT_ROLE_BROADCASTER);

	return bt_manager_event_notify(BT_BROADCAST_SOURCE_RELEASE, data, size);
}

static int broadcast_sink_enable(void *data, int size)
{
	struct bt_broadcast_channel *channel;
	struct bt_broadcast_report *rep;

	rep = (struct bt_broadcast_report *)data;

	channel = find_broadcast_channel(rep->handle, rep->id);
	if (!channel) {
		SYS_LOG_ERR("no chan");
		return -ENODEV;
	}

	channel->state = BT_BROADCAST_STREAM_STREAMING;
	channel->audio_contexts = rep->audio_contexts;
    SYS_EVENT_INF(EVENT_BTMGR_BROAD_SINK_ENABLE,0,0);
	return bt_manager_event_notify(BT_BROADCAST_SINK_ENABLE, data, size);
}

static int broadcast_sink_update(void *data, int size)
{
	struct bt_broadcast_channel *channel;
	struct bt_broadcast_report *rep;

	rep = (struct bt_broadcast_report *)data;

	channel = find_broadcast_channel(rep->handle, rep->id);
	if (!channel) {
		SYS_LOG_ERR("no chan");
		return -ENODEV;
	}

	channel->audio_contexts = rep->audio_contexts;

	return bt_manager_event_notify(BT_BROADCAST_SINK_ENABLE, data, size);
}

static int broadcast_sink_disable(void *data, int size)
{
	struct bt_broadcast_report *rep;
    int ret;

	rep = (struct bt_broadcast_report *)data;

#if 0
	struct bt_broadcast_channel *channel;
	struct bt_broadcast_conn *broadcast;

	broadcast = find_broadcast(rep->handle);
	if (!broadcast) {
		SYS_LOG_ERR("no conn");
		return -ENODEV;
	}

	channel = find_broadcast_channel2(broadcast, rep->id);
	if (!channel) {
		SYS_LOG_ERR("no chan");
		return -ENODEV;
	}

	channel->state = BT_BROADCAST_STREAM_CONFIGURED;
#else
	ret = delete_broadcast(rep->handle,BT_ROLE_RECEIVER);
    SYS_EVENT_INF(EVENT_BTMGR_BROAD_SINK_DISABLE,0,ret);
#endif

	/* delete broadcast channel */
#if 0	// TODO:
	os_sched_lock();

	if (channel->tl) {
		timeline_release(channel->tl);
	}
	if (channel->period_tl) {
		timeline_release(channel->period_tl);
	}
	sys_slist_remove(&broadcast->chan_list, NULL, &channel->node);
	mem_free(channel);

	os_sched_unlock();
#endif
    struct bt_manager_context_t *bt_manager = bt_manager_get_context();

	if ((!ret) && bt_manager->per_synced_count) {
	   bt_manager->per_synced_count--;
	}
	SYS_LOG_INF("pa_sync_cnt:%d,reason:%d",bt_manager->per_synced_count,rep->reason);
	return bt_manager_event_notify(BT_BROADCAST_SINK_DISABLE, data, size);
}

static int broadcast_sink_release(void *data, int size)
{
	struct bt_broadcast_report *rep;
    struct bt_manager_context_t *bt_manager = bt_manager_get_context();
	int ret;

	rep = (struct bt_broadcast_report *)data;

	ret = delete_broadcast(rep->handle,BT_ROLE_RECEIVER);

	if ((!ret) && bt_manager->per_synced_count) {
	   bt_manager->per_synced_count--;
	}
	SYS_LOG_INF("pa_sync_cnt:%d,reason:%d",bt_manager->per_synced_count,rep->reason);
    SYS_EVENT_INF(EVENT_BTMGR_BROAD_SINK_RELEASE,0,ret);
	return bt_manager_event_notify(BT_BROADCAST_SINK_RELEASE, data, size);
}

static int broadcast_sink_base_config(void *data, int size)
{
    struct bt_manager_context_t *bt_manager = bt_manager_get_context();
	bt_manager->per_synced_count++;
	if (bt_manager->per_synced_count > bt_manager->total_per_sync_count) {
		SYS_LOG_ERR(":%d,%d \n",bt_manager->per_synced_count,bt_manager->total_per_sync_count);
	}
    bt_manager_check_per_adv_synced();
	return bt_manager_event_notify(BT_BROADCAST_SINK_BASE_CONFIG, data, size);
}

static int broadcast_sink_sync(void *data, int size)
{
	struct bt_broadcast_conn *broadcast;
	uint32_t handle;

	handle = *(uint32_t *)data;

	os_sched_lock();

	broadcast = new_broadcast(handle, BT_ROLE_RECEIVER);
	if (!broadcast) {
		SYS_LOG_ERR("no conn");
		os_sched_unlock();
		return -ENOMEM;
	}
	os_sched_unlock();

	SYS_LOG_INF("0x%x\n",handle);

	return bt_manager_event_notify(BT_BROADCAST_SINK_SYNC, data, size);
}

static int broadcast_bis_connected(void *data, int size)
{
    struct bt_broadcast_bis_param *param;
	struct bt_broadcast_channel *channel;
	struct bt_broadcast_conn *broadcast;
	int type;

	param = (struct bt_broadcast_bis_param *)data;

	broadcast = find_broadcast(param->handle);
	if (!broadcast) {
		SYS_LOG_ERR("no conn");
		return -ENODEV;
	}

	channel = find_broadcast_channel(param->handle, param->id);
	if (!channel) {
		SYS_LOG_ERR("no chan");
		return -ENODEV;
	}
	os_sched_lock();
	channel->audio_handle = param->audio_handle;
	channel->sync_delay = param->sync_delay;
	channel->iso_interval = param->iso_interval;

	if (broadcast->role == BT_ROLE_BROADCASTER) {
		type = TIMELINE_TYPE_MEDIA_ENCODE;
	} else {
		type = TIMELINE_TYPE_MEDIA_DECODE;
	}

	channel->period_tl = timeline_create_by_owner(type, channel->interval,channel,bt_manager_broadcast_get_current_time);
	if (!channel->period_tl) {
		os_sched_unlock();
		SYS_LOG_ERR("no mem for period_tl");
		return -ENOMEM;
	}
	timeline_start(channel->period_tl);

	os_sched_unlock();
	SYS_LOG_INF(" delay %d interval %d\n",param->sync_delay,channel->iso_interval);
	return bt_manager_event_notify(BT_BIS_CONNECTED, data, size);
}

void bt_manager_broadcast_event(int event, void *data, int size)
{
	SYS_LOG_INF("%d", event);

	switch (event) {
	/* broadcast source */
	case BT_BROADCAST_SOURCE_CONFIG:
		broadcast_config(true, data, size);
		break;
	case BT_BROADCAST_SOURCE_ENABLE:
		broadcast_source_enable(data, size);
		break;
	case BT_BROADCAST_SOURCE_UPDATE:
		broadcast_source_update(data, size);
		break;
	case BT_BROADCAST_SOURCE_DISABLE:
		broadcast_source_disable(data, size);
		break;
	case BT_BROADCAST_SOURCE_RELEASE:
		broadcast_source_release(data, size);
		break;
	case BT_BROADCAST_SOURCE_SENDDATA_EMPTY:
	    bt_manager_event_notify(BT_REQ_RESTART_PLAY, NULL, 0);
		break;

	/* broadcast sink */
	case BT_BROADCAST_SINK_CONFIG:
		broadcast_config(false, data, size);
		break;
	case BT_BROADCAST_SINK_ENABLE:
		broadcast_sink_enable(data, size);
		break;
	case BT_BROADCAST_SINK_UPDATE:
		broadcast_sink_update(data, size);
		break;
	case BT_BROADCAST_SINK_DISABLE:
		broadcast_sink_disable(data, size);
		break;
	case BT_BROADCAST_SINK_RELEASE:
		broadcast_sink_release(data, size);
		break;
	case BT_BROADCAST_SINK_BASE_CONFIG:
	    //bt_manager_event_notify(BT_BROADCAST_SINK_BASE_CONFIG, data, size);
		broadcast_sink_base_config(data, size);
		break;
	case BT_BROADCAST_SINK_SYNC:
		broadcast_sink_sync(data, size);
		break;
	case BT_BIS_CONNECTED:
		broadcast_bis_connected(data, size);
		break;
	default:
		bt_manager_event_notify(event, data, size);
		break;
	}
}

int bt_manager_audio_le_adv_param_init(struct bt_le_adv_param *param)
{
	if (!param) {
		return -EINVAL;
	}

	if (bt_audio.le_audio_state == BT_AUDIO_STATE_STOPPING) {
		bt_audio.le_audio_pending_adv_param = 1;
		memcpy(&bt_audio.le_audio_adv_param, param, sizeof(*param));
		if (param->peer) {
			bt_addr_le_copy(&bt_audio.le_audio_adv_peer, param->peer);
			bt_audio.le_audio_adv_param.peer = &bt_audio.le_audio_adv_peer;
		}
		SYS_LOG_INF("stopping");
		return 0;
	}

	return btif_audio_adv_param_init(param);
}

int bt_manager_audio_le_scan_param_init(struct bt_le_scan_param *param)
{
	if (!param) {
		return -EINVAL;
	}

	return btif_audio_scan_param_init(param);
}

int bt_manager_audio_le_conn_create_param_init(struct bt_conn_le_create_param *param)
{
	if (!param) {
		return -EINVAL;
	}

	return btif_audio_conn_create_param_init(param);
}

int bt_manager_audio_le_conn_param_init(struct bt_le_conn_param *param)
{
	if (!param) {
		return -EINVAL;
	}

	if (bt_audio.le_audio_state == BT_AUDIO_STATE_STOPPING) {
		bt_audio.le_audio_pending_conn_param = 1;
		memcpy(&bt_audio.le_audio_conn_param, param, sizeof(*param));
		SYS_LOG_INF("stopping");
		return 0;
	}

	return btif_audio_conn_param_init(param);
}

int bt_manager_audio_le_conn_idle_param_init(struct bt_le_conn_param *param)
{
	if (!param) {
		return -EINVAL;
	}

	return btif_audio_conn_idle_param_init(param);
}

int bt_manager_audio_le_pause_scan(void)
{
	if (bt_audio.le_audio_state == BT_AUDIO_STATE_STOPPING) {
		bt_audio.le_audio_pending_pause_scan = 1;
		SYS_LOG_INF("stopping");
		return 0;
	}

	return btif_audio_pause_scan();
}

int bt_manager_audio_le_resume_scan(void)
{
	if (bt_audio.le_audio_state == BT_AUDIO_STATE_STOPPING) {
		bt_audio.le_audio_pending_pause_scan = 0;
		SYS_LOG_INF("stopping");
		return 0;
	}

	return btif_audio_resume_scan();
}

int bt_manager_audio_le_get_scan_status(void)
{
	if (bt_audio.le_audio_state == BT_AUDIO_STATE_STOPPING) {
		SYS_LOG_INF("stopping");
		return 0;
	}

	return btif_audio_get_scan_status();
}

int bt_manager_audio_le_pause_adv(void)
{
	if (bt_audio.le_audio_state == BT_AUDIO_STATE_STOPPING) {
		bt_audio.le_audio_pending_pause_adv = 1;
		SYS_LOG_INF("stopping");
		return 0;
	}

	return btif_audio_pause_adv();
}

int bt_manager_audio_le_resume_adv(void)
{
	if (bt_audio.le_audio_state == BT_AUDIO_STATE_STOPPING) {
		bt_audio.le_audio_pending_pause_adv = 0;
		SYS_LOG_INF("stopping");
		return 0;
	}

	return btif_audio_resume_adv();
}

int bt_manager_audio_tws_connected(void)
{
#if 0
	struct bt_audio_conn *audio_conn;

	audio_conn = find_conn_by_prio(BT_TYPE_LE, 0);
	if (!audio_conn || !audio_conn->is_lea){
		return 0;
	}

	bt_manager_sync_volume_from_phone_leaudio(&(audio_conn->addr), audio_conn->volume);
#endif
	return 0;
}

int bt_manager_audio_in_btcall(void)
{
	/* If br calling, don't advertising LEA */
	if ((bt_audio.active_slave != NULL) && (bt_audio.active_slave->prio & BT_AUDIO_PRIO_CALL)) {
		return 1;
	}

	if (bt_audio.cur_active_stream == BT_AUDIO_STREAM_BR_CALL) {
		return 1;
	}

	return 0;
}

int bt_manager_audio_in_btmusic(void)
{
	if (bt_audio.cur_active_stream == BT_AUDIO_STREAM_BR_MUSIC) {
		return 1;
	}

	return 0;
}

int bt_manager_set_leaudio_foreground_dev(bool enable){

	return btif_set_leaudio_foreground_dev(enable);
}

int bt_manager_is_idle_status(){
	return !bt_audio.device_num && !bt_manager_is_pair_mode();
}

char *bt_manager_audio_get_last_connected_dev_name(void)
{
	struct bt_audio_conn *p_audio_conn;
	struct bt_audio_conn *p_phone_dev = NULL;
	uint32_t connect_time = 0;

	os_sched_lock();
	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, p_audio_conn, node) {
		if(p_audio_conn->is_phone_dev && p_audio_conn->dev_name){
			if(p_audio_conn->type == BT_TYPE_LE){
				if(p_audio_conn->ascs_connect_time > connect_time){
					connect_time = p_audio_conn->ascs_connect_time;
					p_phone_dev = p_audio_conn;
				}
			}else if(p_audio_conn->type == BT_TYPE_BR){
				struct bt_mgr_dev_info * info = bt_mgr_find_dev_info_by_hdl(p_audio_conn->handle);
				if(info && info->a2dp_connected && info->a2dp_connect_time > connect_time){
					if(bt_manager_audio_is_ios_dev(p_audio_conn->handle) || (k_uptime_get_32() - info->a2dp_connect_time > 2000)){
						connect_time = info->a2dp_connect_time;
						p_phone_dev = p_audio_conn;
					}
				}
			}
		}
	}
	os_sched_unlock();

	if (p_phone_dev) {
		return p_phone_dev->dev_name;
	}else{
		return NULL;
	}
}

uint32_t bt_manager_music_to_lea_volume(uint32_t music_vol)
{
	uint32_t  lea_vol = 0;

	uint32_t  max_volume = audio_policy_get_volume_level();

	if (music_vol == 0) {
		lea_vol = 0;
	} else if (music_vol >= max_volume) {
		lea_vol = 255;
	} else {
		lea_vol = (music_vol * 256 / max_volume);
	}

    SYS_LOG_INF("vol -> lea vol: %d,%d \n",music_vol,lea_vol);
	return lea_vol;
}


uint32_t bt_manager_lea_to_music_volume(uint32_t lea_vol)
{
	uint32_t  music_vol = 0;
	uint32_t  max_volume = audio_policy_get_volume_level();


	if (lea_vol == 0) {
		music_vol = 0;
	} else if (lea_vol >= 255) {
		music_vol = max_volume;
	} else {
	    music_vol = (lea_vol + 1) * max_volume / 256;
	}

    SYS_LOG_INF("lea vol -> vol:%d,%d \n",lea_vol,music_vol);
	return music_vol;
}

void bt_manager_pawr_check_connected_dev(void)
{
#ifdef CONFIG_BT_PAWR
    uint8_t phone_num = bt_manager_audio_get_cur_dev_num();
    if(phone_num > 1){
        if(bt_manager_audio_is_lea_open()) {
            check_audio_conn_preemption_pre_dev(NULL, 0, 0);
        }else{
            bt_manager_disconnect_inactive_audio_conn();
        }
    }
#endif
}

void bt_manager_audio_dump_info(void)
{
	struct bt_audio_conn *audio_conn;
	char addr[13];

	printk("Bt manager audio info\n");

	SYS_SLIST_FOR_EACH_CONTAINER(&bt_audio.conn_list, audio_conn, node) {
		hex_to_str(addr, (char*)&audio_conn->addr, 6);
		printk("Audio MAC %s hdl 0x%x type %d role %d\n", addr, audio_conn->handle, audio_conn->type, audio_conn->role);
		printk("\t prio %d is_phone %d lea %d le_tws %d br_tws %d\n", audio_conn->prio, audio_conn->is_phone_dev,
				 audio_conn->is_lea, audio_conn->le_tws, audio_conn->is_br_tws);
	}

	printk("last dev name %s\n",bt_manager_audio_get_last_connected_dev_name());

	printk("\n");
}
