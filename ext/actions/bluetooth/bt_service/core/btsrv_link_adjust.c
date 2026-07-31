/*
 * Copyright (c) 2016 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Adjust link time.
 *   this is manager adjust link time.
 */
#define SYS_LOG_DOMAIN "btsrv_hfp"
#include "btsrv_os_common.h"
#include "btsrv_inner.h"


#define LINK_ADJUST_INTERVALE		50
#define SINK_BLOCK_MAX				60

struct btsrv_link_adjust_priv {
	struct thread_timer auto_adjust_timer;
	uint8_t adjust_runing:1;
	uint8_t two_device_link:1;
	uint8_t tws_bt_play:1;
	uint8_t tws_adjust_state:3;

	uint8_t block_send_cnt;
	uint16_t cache_buff_size;
	uint16_t source_cache_len;
	uint16_t cache_sink_len;
};

static struct btsrv_link_adjust_priv btsrv_link_adjust;
static struct btsrv_link_adjust_priv *p_link_ajdust;
void ctrl_adjust_link_time(unsigned short acl_handle, signed short adjust_val);

static void btsrv_adjust_link_time(struct bt_conn *base_conn, uint16_t link_time)
{
	uint16_t handle;

	if (btsrv_rdm_get_link_time(base_conn) != link_time) {
		handle = hostif_bt_conn_get_handle(base_conn);
		btsrv_rdm_set_link_time(base_conn, link_time);
		ctrl_adjust_link_time(handle, (int16_t)link_time);
		SYS_LOG_INF("adjust_link 0x%x, %d", handle, link_time);
	}
}

#if 1
static bool btsrv_tws_in_reconnecting(void)
{
	bool reconnect_phone;
	uint8_t pair_state;

	pair_state = btsrv_adapter_get_pair_state();
	reconnect_phone = btsrv_autoconn_is_phone_connecting();

	if ((pair_state & (BT_PAIR_STATUS_PAIR_MODE | BT_PAIR_STATUS_TWS_SEARCH | BT_PAIR_STATUS_RECONNECT)) ||
		reconnect_phone) {
		return true;
	} else {
		return false;
	}
}

static void btsrv_snoop_adjust_link_time(void)
{
	uint16_t active_req_link_time = 0;
	uint16_t second_req_link_time = 0;
	struct bt_conn *a2dp_active_conn;
	struct bt_conn *second_conn;
	struct bt_conn *hfp_active_conn;

	a2dp_active_conn = btsrv_rdm_a2dp_get_active_dev();
	second_conn = btsrv_rdm_a2dp_get_second_dev();
	if (!a2dp_active_conn) {
		return;
	}

	hfp_active_conn = btsrv_rdm_hfp_get_actived_dev();

	if (hfp_active_conn && btsrv_rdm_hfp_in_call_state(hfp_active_conn)) {
		active_req_link_time = 0;
		second_req_link_time = 0;
	} else if (second_conn) {
#if 0
		bool second_is_sniff = btsrv_sniff_in_sniff_mode(second_conn);

		if (btsrv_rdm_is_a2dp_stream_open(a2dp_active_conn) && btsrv_rdm_is_a2dp_stream_open(second_conn)) {
			active_req_link_time = 20;
			second_req_link_time = 0;
		} else if (btsrv_rdm_is_a2dp_stream_open(a2dp_active_conn) && (!second_is_sniff)) {
			active_req_link_time = 20;
			second_req_link_time = 0;
		} else if (btsrv_rdm_is_a2dp_stream_open(a2dp_active_conn) && btsrv_tws_in_reconnecting()) {
			active_req_link_time = 20;
			second_req_link_time = 0;
		} else {
			active_req_link_time = 0;
			second_req_link_time = 0;
		}
#else
		/* Confirm with controler, keep high prioroty for active phone. */
		active_req_link_time = 20;
		second_req_link_time = 0;
#endif
	} else {
		if (btsrv_rdm_is_a2dp_stream_open(a2dp_active_conn) && btsrv_tws_in_reconnecting()) {
			active_req_link_time = 20;
			second_req_link_time = 0;
		} else {
			active_req_link_time = 0;
			second_req_link_time = 0;
		}
	}

	btsrv_adjust_link_time(a2dp_active_conn, active_req_link_time);
	if (second_conn) {
		btsrv_adjust_link_time(second_conn, second_req_link_time);
	}
}

#else

static void connected_dev_cb_reset_link_time(struct bt_conn *base_conn, uint8_t tws_dev, void *cb_param)
{
	btsrv_adjust_link_time(base_conn, 0);
}

static void btsrv_tws_master_adjust_link_time(void)
{
	uint16_t active_req_link_time;
	uint16_t tws_req_link_time;
	struct bt_conn *a2dp_active_conn;
	struct bt_conn *tws_conn;
	uint8_t host_pkt, controler_pkt;

	a2dp_active_conn = btsrv_rdm_a2dp_get_active_dev();
	tws_conn = btsrv_rdm_get_tws_by_role(BTSRV_TWS_MASTER);
	if (!a2dp_active_conn || !tws_conn) {
		return;
	}

	if (p_link_ajdust->tws_adjust_state != LINK_ADJUST_RUNNING) {
		active_req_link_time = 0;
		tws_req_link_time = 0;
		goto exit_adjust;
	}

	/*printk("link adjust:%d, %d, %d\n", p_link_ajdust->cache_buff_size,
	p_link_ajdust->source_cache_len, p_link_ajdust->cache_sink_len);*/

	if (system_check_low_latencey_mode()) {
		active_req_link_time = 0;
		tws_req_link_time = 0;
	} else {
		if (!p_link_ajdust->tws_bt_play) {
			/* Not bt tws play */
			if (btsrv_sniff_in_sniff_mode(a2dp_active_conn)) {
				active_req_link_time = 0;
				tws_req_link_time = 0;
			} else {
				active_req_link_time = 0;
				tws_req_link_time = 20;
			}
		} else if ((p_link_ajdust->source_cache_len < (p_link_ajdust->cache_buff_size/4)) ||
			((p_link_ajdust->source_cache_len < (p_link_ajdust->cache_buff_size/3)) &&
			(p_link_ajdust->cache_sink_len < (p_link_ajdust->cache_buff_size/8)))){
			/* source_cache_len < (cache_buff_size/4) */
			active_req_link_time = 40;
			tws_req_link_time = 0;
		} else if (p_link_ajdust->source_cache_len < (p_link_ajdust->cache_buff_size/3)) {
			/* source_cache_len < (cache_buff_size/4) */
			active_req_link_time = 20;
			tws_req_link_time = 0;
		} else if (p_link_ajdust->block_send_cnt >= SINK_BLOCK_MAX) {
			/* Master slave link terrible */
			active_req_link_time = 0;
			tws_req_link_time = 0;
		} else if ((p_link_ajdust->source_cache_len > (p_link_ajdust->cache_buff_size*2/5)) &&
		    (p_link_ajdust->cache_sink_len > (p_link_ajdust->cache_buff_size/4))) {
			active_req_link_time = 0;
			tws_req_link_time = 40;
		} else {
			/* (cache_buff_size/4) < source_cache_len < (cache_buff_size*3/4) */
			if (p_link_ajdust->cache_sink_len > (p_link_ajdust->cache_buff_size/6)) {
				active_req_link_time = 20;
				tws_req_link_time = 20;
			} else {
				active_req_link_time = 20;
				tws_req_link_time = 0;
			}
		}
	}

	hostif_bt_br_conn_pending_pkt(tws_conn, &host_pkt, &controler_pkt);
	/*printk("link adjust: %d, %d, (%d,%d,%d) (%d,%d)\n", p_link_ajdust->source_cache_len ,
		p_link_ajdust->cache_sink_len, p_link_ajdust->block_send_cnt, host_pkt, controler_pkt,
		active_req_link_time, tws_req_link_time);*/


exit_adjust:
	btsrv_adjust_link_time(a2dp_active_conn, active_req_link_time);
	btsrv_adjust_link_time(tws_conn, tws_req_link_time);
}

static void btsrv_two_phone_adjust_link_time(void)
{
	uint16_t active_req_link_time;
	uint16_t second_req_link_time;
	struct bt_conn *a2dp_active_conn;
	struct bt_conn *second_conn;
	bool second_is_sniff;

	a2dp_active_conn = btsrv_rdm_a2dp_get_active_dev();
	second_conn = btsrv_rdm_a2dp_get_second_dev();
	if (!a2dp_active_conn || !second_conn) {
		return;
	}

	second_is_sniff = btsrv_sniff_in_sniff_mode(second_conn);

	if (btsrv_rdm_is_a2dp_stream_open(a2dp_active_conn) && btsrv_rdm_is_a2dp_stream_open(second_conn)) {
		active_req_link_time = 20;
		second_req_link_time = 0;
	} else if (btsrv_rdm_is_a2dp_stream_open(a2dp_active_conn) && (!second_is_sniff)) {
		active_req_link_time = 20;
		second_req_link_time = 0;
	} else {
		active_req_link_time = 0;
		second_req_link_time = 0;
	}

	btsrv_ajdust_link_time(a2dp_active_conn, active_req_link_time);
	btsrv_ajdust_link_time(second_conn, second_req_link_time);
}
#endif

static void btsrv_link_adjust_timer_handler(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	int dev_count;

	p_link_ajdust->adjust_runing = 1;
	dev_count = btsrv_rdm_get_connected_dev(NULL, NULL);
	if (dev_count == 0) {
		goto adjust_exit;
	}

#if 1
	btsrv_snoop_adjust_link_time();
#else
	if (dev_count == 1) {
		if (p_link_ajdust->two_device_link) {
			p_link_ajdust->two_device_link = 0;
			btsrv_rdm_get_connected_dev(connected_dev_cb_reset_link_time, NULL);
		}
		goto adjust_exit;
	}

	p_link_ajdust->two_device_link = 1;
	if (btsrv_rdm_get_dev_role() == BTSRV_TWS_MASTER) {
		btsrv_tws_master_adjust_link_time();
	} else {
		btsrv_two_phone_adjust_link_time();
	}
#endif

adjust_exit:
	p_link_ajdust->adjust_runing = 0;
}

int btsrv_link_adjust_set_tws_state(uint8_t adjust_state, uint16_t buff_size, uint16_t source_cache, uint16_t cache_sink)
{
	if (!p_link_ajdust) {
		return -EIO;
	}

	if (adjust_state == LINK_ADJUST_SINK_BLOCK) {
		if (p_link_ajdust->block_send_cnt < SINK_BLOCK_MAX) {
			p_link_ajdust->block_send_cnt++;
		}
		goto exit_set;
	} else if (adjust_state == LINK_ADJUST_SINK_CLEAR_BLOCK) {
		p_link_ajdust->block_send_cnt = 0;
		goto exit_set;
	}

	p_link_ajdust->tws_adjust_state = adjust_state;
	p_link_ajdust->cache_buff_size = buff_size;
	p_link_ajdust->source_cache_len = source_cache;
	p_link_ajdust->cache_sink_len = cache_sink;

exit_set:
	return 0;
}

int btsrv_link_adjust_tws_set_bt_play(bool bt_play)
{
	p_link_ajdust->tws_bt_play = bt_play ? 1 : 0;
	return 0;
}

void btsrv_link_adjust_quickly(void)
{
	if (p_link_ajdust) {
		btsrv_link_adjust_timer_handler(NULL, NULL);
	}
}

int btsrv_link_adjust_init(void)
{
	p_link_ajdust = &btsrv_link_adjust;

	memset(p_link_ajdust, 0, sizeof(struct btsrv_link_adjust_priv));
	p_link_ajdust->tws_bt_play = 1;
	thread_timer_init(&p_link_ajdust->auto_adjust_timer, btsrv_link_adjust_timer_handler, NULL);
	thread_timer_start(&p_link_ajdust->auto_adjust_timer, LINK_ADJUST_INTERVALE, LINK_ADJUST_INTERVALE);
	return 0;
}

void btsrv_link_adjust_deinit(void)
{
	if (p_link_ajdust == NULL) {
		return;
	}

	while (p_link_ajdust->adjust_runing) {
		os_sleep(10);
	}

	if (thread_timer_is_running(&p_link_ajdust->auto_adjust_timer)) {
		thread_timer_stop(&p_link_ajdust->auto_adjust_timer);
	}

	p_link_ajdust = NULL;
}
