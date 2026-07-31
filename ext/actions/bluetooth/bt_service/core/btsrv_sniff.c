/*
 * Copyright (c) 2016 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Manager conn sniff.
 *   this is manager conn sniff.
 */
#define SYS_LOG_DOMAIN "btsrv_sniff"
#include "btsrv_os_common.h"
#include "btsrv_inner.h"

#define SNIFF_CHECK_INTERVALE					100
#define SNIFF_CHECK_FAILED_ENTER_SNIFF_TIME		(1100)

struct btsrv_sniff_priv {
	struct thread_timer sniff_check_timer;
};

static struct btsrv_sniff_priv btsrv_sniff_priv_data;
static struct btsrv_sniff_priv *p_sniff_priv;

struct btsrv_sniff_tws_info {
	struct bt_conn *conn;
	uint8_t phone_in_active_mode:1;
	uint8_t only_check_entering_flag:1;
};

static bool btsrv_sniff_snoop_active_link_in_btcall(struct bt_conn *conn)
{
	int sco_state;

	sco_state = btsrv_rdm_hfp_get_sco_state(conn);
	if (btsrv_tws_is_snoop_active_link(conn) &&
		(sco_state == BTSRV_SCO_STATE_PHONE || sco_state == BTSRV_SCO_STATE_HFP)) {
		return true;
	} else {
		return false;
	}
}

#if 1
/* Active phone or tws with connected two phone, sniff interval 200ms,
 * Non-active phone sniff interval set by config tool.
 */
static uint16_t btsrv_sniff_cal_sniff_interval(struct bt_conn *conn, uint8_t tws_dev)
{
	struct rdm_sniff_info *active_info = NULL;
	struct bt_conn *active_conn;
	uint16_t sniff_interval;
	uint32_t sniff_time = btsrv_sniff_interval();
	int connected_phone_cnt = btsrv_rdm_get_connected_dev_cnt_by_type(BTSRV_DEVICE_PHONE);

	if ((connected_phone_cnt > 1) &&
		(tws_dev || btsrv_tws_is_snoop_active_link(conn))) {
		if (sniff_time > 200) {
			/* Active phone or tws with connected two phone, sniff interval 200ms */
			sniff_time = 200;
		}
	} else if (tws_dev ||
				((connected_phone_cnt > 1) &&
				(btsrv_rdm_get_snoop_role(conn) == BTSRV_SNOOP_MASTER) &&
				(!btsrv_tws_is_snoop_active_link(conn)))) {
		active_conn = btsrv_rdm_get_actived_phone();
		if (active_conn) {
			active_info = btsrv_rdm_get_sniff_info(active_conn);
		}

		if (active_info) {
			/* interval 280 = 175ms, 360 = 225ms */
			if (tws_dev) {
				if ((280 < active_info->sniff_interval) && (active_info->sniff_interval < 360)) {
					/* tws with one phone sniff interval 200ms,  set 200ms. */
					sniff_time = 200;
				}
			} else {
				if ((active_info->sniff_interval != 0) && (active_info->sniff_interval > 360)) {
					/* Tws active phone sniff interval not set 200ms, nonactive phone set 200ms. */
					sniff_time = 200;
				}
			}
		}
	}

	sniff_interval = (sniff_time * 1000) / 625;		/* Sniff interval = N*0.625  */
	return sniff_interval;
}
#else
/* Active phone or tws with connected two phone, sniff interval set by config tool,
 * Non-active phone sniff interval 200ms.
 */
static uint16_t btsrv_sniff_cal_sniff_interval(struct bt_conn *conn, uint8_t tws_dev)
{
	struct rdm_sniff_info *active_info = NULL, *nonactive_info = NULL;
	struct bt_conn *active_conn, *nonactive_conn;
	uint16_t sniff_interval;
	uint32_t sniff_time = btsrv_sniff_interval();
	int connected_phone_cnt = btsrv_rdm_get_connected_dev_cnt_by_type(BTSRV_DEVICE_PHONE);

	if ((connected_phone_cnt > 1) &&
		(btsrv_rdm_get_snoop_role(conn) == BTSRV_SNOOP_MASTER) &&
		(!btsrv_tws_is_snoop_active_link(conn))) {
		active_conn = btsrv_rdm_get_actived_phone();
		active_info = btsrv_rdm_get_sniff_info(active_conn);

		/* interval 280 = 175ms, 360 = 225ms */
		if ((280 > active_info->sniff_interval) || (active_info->sniff_interval > 360)) {
			/* Active phone not in sniff or sniff interval > 225ms */
			if (sniff_time > 200) {
				/* Non-active phone sniff interval 200ms */
				sniff_time = 200;
			}
		}
	} else if ((connected_phone_cnt > 1) && tws_dev) {
		active_conn = btsrv_rdm_get_actived_phone();
		active_info = btsrv_rdm_get_sniff_info(active_conn);

		if ((280 < active_info->sniff_interval) && (active_info->sniff_interval < 360)) {
			/* Active phone already set sniff interval 200ms,  tws set 200ms. */
			sniff_time = 200;
		}
	} else if ((connected_phone_cnt > 1) && btsrv_tws_is_snoop_active_link(conn)) {
		nonactive_conn = btsrv_rdm_get_nonactived_phone();
		nonactive_info = btsrv_rdm_get_sniff_info(nonactive_conn);
		if (nonactive_info->sniff_interval > 360) {
			/* Nonactive phone already set sniff interval > 200ms */
			if (sniff_time > 200) {
				/* Active phone sniff interval 200ms */
				sniff_time = 200;
			}
		}
	}

	sniff_interval = (sniff_time * 1000) / 625;		/* Sniff interval = N*0.625  */
	return sniff_interval;
}
#endif

static void connected_dev_cb_check_sniff(struct bt_conn *conn, uint8_t tws_dev, void *cb_param)
{
	struct rdm_sniff_info *info;
	struct btsrv_sniff_tws_info *tws_sniff_info = cb_param;
	uint32_t curr_time;
	uint16_t conn_rxtx_cnt, bt_sniff_interval;

	if (btsrv_rdm_get_snoop_role(conn) == BTSRV_SNOOP_SLAVE) {
		return;
	}

	info = btsrv_rdm_get_sniff_info(conn);

	if(!info) {
		return;
	}
	
	curr_time = os_uptime_get_32();
	if (info->sniff_mode == BT_ACTIVE_MODE && info->sniff_entering) {
		if ((curr_time - info->sniff_entering_time) > SNIFF_CHECK_FAILED_ENTER_SNIFF_TIME) {
			info->sniff_entering = 0;
		}
	}

	if (tws_sniff_info->only_check_entering_flag) {
		/* In snoop siwtch state or tws slave, just check/clear sniff_entering flag. */
		return;
	}

	if (tws_dev) {
		tws_sniff_info->conn = conn;
		if (btsrv_adapter_check_wake_lock_bit(BTSRV_WAKE_LOCK_TWS_IRQ_BUSY)) {
			info->idle_start_time = curr_time;
			//SYS_LOG_INF("BTSRV_WAKE_LOCK_TWS_IRQ_BUSY:%d",btsrv_adapter_check_wake_lock_bit(BTSRV_WAKE_LOCK_TWS_IRQ_BUSY));
			return;
		}
	} else {
		if (info->sniff_mode != BT_SNIFF_MODE) {
			/* Phone not in sniff mode, update tws idle time */
			tws_sniff_info->phone_in_active_mode = 1;
		}

		if (!btsrv_rdm_is_security_changed(conn)) {
			/* Phone check enter sniff after security finish */
			info->idle_start_time = curr_time;
			return;
		}
	}

	conn_rxtx_cnt = hostif_bt_conn_get_rxtx_cnt(conn);
	if (info->conn_rxtx_cnt != conn_rxtx_cnt) {
		info->conn_rxtx_cnt = conn_rxtx_cnt;
		info->idle_start_time = curr_time;
		if(info->sniff_mode == BT_SNIFF_MODE && !info->sniff_exiting){
			hostif_bt_conn_check_exit_sniff(conn);
			info->sniff_exiting = 1;
			SYS_LOG_INF("0x%x exit sniff\n", hostif_bt_conn_get_handle(conn));
		}
		return;
	}

	if ((btsrv_rdm_get_snoop_role(conn) == BTSRV_SNOOP_MASTER) &&
		btsrv_sniff_snoop_active_link_in_btcall(conn)) {
		info->idle_start_time = curr_time;
		return;
	}

	if (info->sniff_mode == BT_ACTIVE_MODE && !info->sniff_entering) {
		if ((curr_time - info->idle_start_time) > btsrv_idle_enter_sniff_time()) {
			info->sniff_entering = 1;
			info->idle_start_time = curr_time;
			info->sniff_entering_time = curr_time;
			bt_sniff_interval = btsrv_sniff_cal_sniff_interval(conn, tws_dev);
			SYS_LOG_INF("Check 0x%x enter sniff %d", hostif_bt_conn_get_handle(conn), bt_sniff_interval);
			hostif_bt_conn_check_enter_sniff(conn, bt_sniff_interval, bt_sniff_interval);
		}
	}
}

static void btsrv_sniff_timer_handler(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	struct btsrv_sniff_tws_info tws_sniff_info;
	struct rdm_sniff_info *info;

	if (!btsrv_sniff_enable()) {
		return;
	}

	if ((btsrv_exit_sniff_when_bis()) &&
		(btsrv_broadcast_is_connected() == 1)){
		return;
	}

	memset(&tws_sniff_info, 0, sizeof(tws_sniff_info));
	if (btsrv_tws_is_in_switch_state() || (btsrv_rdm_get_dev_role() == BTSRV_TWS_SLAVE)) {
		tws_sniff_info.only_check_entering_flag = 1;
	}
	btsrv_rdm_get_connected_dev(connected_dev_cb_check_sniff, &tws_sniff_info);

	if (tws_sniff_info.only_check_entering_flag) {
		/* In snoop siwtch state or tws slave, just check/clear sniff_entering flag. */
		return;
	}

	/* Tws enter sniff after all phone enter sniff */
	if (tws_sniff_info.conn && tws_sniff_info.phone_in_active_mode) {
		info = btsrv_rdm_get_sniff_info(tws_sniff_info.conn);
		if (info) {
			info->idle_start_time = os_uptime_get_32();
			/* Phone enter active mode, tws exit sniff mode. */
			if (info->sniff_mode == BT_SNIFF_MODE) {
				hostif_bt_conn_check_exit_sniff(tws_sniff_info.conn);
			}
		}
	}
}

void btsrv_sniff_update_idle_time(struct bt_conn *conn)
{
	struct rdm_sniff_info *info;

	if (!btsrv_sniff_enable()) {
		return;
	}

	info = btsrv_rdm_get_sniff_info(conn);
	if (!info) {
		return;
	}

	info->idle_start_time = os_uptime_get_32();
}

void btsrv_sniff_mode_change(void *param)
{
	struct btsrv_mode_change_param *in_param = param;
	struct rdm_sniff_info *info;

	info = btsrv_rdm_get_sniff_info(in_param->conn);
	if (!info) {
		return;
	}

	info->sniff_mode = in_param->mode;
	info->sniff_interval = in_param->interval;
	info->sniff_entering = 0;
	info->sniff_exiting = 0;
	info->idle_start_time = os_uptime_get_32();

	if ((btsrv_rdm_get_dev_role() == BTSRV_TWS_SLAVE) ||
		(btsrv_rdm_get_snoop_role(in_param->conn) == BTSRV_SNOOP_SLAVE)) {
		return;
	}

	if (info->sniff_mode == BT_SNIFF_MODE) {
		/* In switch state or btcall state, not enter sniff. */
		if (btsrv_tws_is_in_switch_state() ||
			btsrv_sniff_snoop_active_link_in_btcall(in_param->conn)) {
			SYS_LOG_INF("In switch state or btcall exit sniff 0x%x", hostif_bt_conn_get_handle(in_param->conn));
			hostif_bt_conn_check_exit_sniff(in_param->conn);
		}else {
			if ((btsrv_exit_sniff_when_bis()) &&
				(btsrv_broadcast_is_connected() == 1)){
				SYS_LOG_INF("In bis exit sniff 0x%x", hostif_bt_conn_get_handle(in_param->conn));
				hostif_bt_conn_check_exit_sniff(in_param->conn);
			}
		}
	} else if ((info->sniff_mode == BT_ACTIVE_MODE) &&
		(btsrv_rdm_get_snoop_role(in_param->conn) == BTSRV_SNOOP_MASTER)) {
		/* Phone exit sniff mode, tws exit sniff immediately */
		btsrv_sniff_timer_handler(NULL, NULL);
	}

	if (btsrv_rdm_get_snoop_role(in_param->conn) == BTSRV_SNOOP_MASTER) {
		btsrv_tws_sync_base_info_to_remote(in_param->conn);
	}
}

bool btsrv_sniff_in_sniff_mode(struct bt_conn *conn)
{
	struct rdm_sniff_info *info;

	info = btsrv_rdm_get_sniff_info(conn);
	if (!info) {
		return false;
	}

	return (info->sniff_mode == BT_SNIFF_MODE) ? true : false;
}

bool btsrv_sniff_in_entering_sniff(struct bt_conn *conn)
{
	struct rdm_sniff_info *info;

	info = btsrv_rdm_get_sniff_info(conn);
	if (!info) {
		return false;
	}

	return (info->sniff_entering) ? true : false;
}

void btsrv_sniff_init(void)
{
	p_sniff_priv = &btsrv_sniff_priv_data;

	memset(p_sniff_priv, 0, sizeof(struct btsrv_sniff_priv));
	thread_timer_init(&p_sniff_priv->sniff_check_timer, btsrv_sniff_timer_handler, NULL);
	thread_timer_start(&p_sniff_priv->sniff_check_timer, SNIFF_CHECK_INTERVALE, SNIFF_CHECK_INTERVALE);
}

void btsrv_sniff_deinit(void)
{
	if (p_sniff_priv == NULL) {
		return;
	}

	if (thread_timer_is_running(&p_sniff_priv->sniff_check_timer)) {
		thread_timer_stop(&p_sniff_priv->sniff_check_timer);
	}

	p_sniff_priv = NULL;
}
