/*
 * Copyright (c) 2017 Actions Semi Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief btsrvice sco
 */

#define SYS_LOG_DOMAIN "btsrv_sco"
#include "btsrv_os_common.h"
#include "btsrv_inner.h"

//#define DECODE_PLAYBACK_MCU 1

#define BTSPEECH_SCO_PACKET_SIZE             (60)   /* bytes  */
#define SCO_EXCEPT_CACHE_SIZE                   (60)
#define SCO_DISCONNECT_TIMER_INTERVAL			(50)	/* 50ms */
#define SCO_MIN_DISCONNECT_INTERVAL_TIME		(300)	/* 300ms */
#define SCO_MAX_RECORD							2
#define SCO_CHECK_DROP_MUTE_FRAME_CNT			(0)
#define BT_INVALID_BT_CLOCK						(0xFFFFFFFF)

struct sco_info_record {
	struct bt_conn *sco_conn;
	uint16_t frame_cnt;
	uint8_t frame_cached_len;
	uint8_t mute_frame : 1;
	uint8_t check_mute_finish : 1;
	uint32_t first_pkt_clk;
};

struct btsrv_sco_context_info {
	struct bt_conn *sco_conn;
	uint8_t last_seq_no : 3;
	uint8_t first_frame : 1;
	uint8_t drop_flag : 1;
	uint8_t sco_rx_last_pkg_status_flag;
	uint16_t sco_rx_pkt_cache_len;
	uint8_t *sco_rx_cache;
	uint16_t frame_cnt;
	uint16_t drop_frame;
    uint16_t error_frame;
	struct sco_info_record record[SCO_MAX_RECORD];
	btsrv_sco_callback user_cb;
	struct thread_timer sco_disconnect_timer;
	u16_t sco_except_cache_len;
	u8_t *sco_except_cache;
};

static struct btsrv_sco_context_info sco_context_info;
static struct btsrv_sco_context_info *sco_context;

static void btsrv_sco_user_callback(struct bt_conn *conn, btsrv_hfp_event_e event,  uint8_t *param, int param_size)
{
	if (sco_context && sco_context->user_cb) {
		sco_context->user_cb(hostif_bt_conn_get_handle(conn), event, param, param_size);
	}
}

static struct sco_info_record *btsrv_sco_find_record_by_conn(struct bt_conn *conn)
{
	int i;

	if (!sco_context) {
		return NULL;
	}

	for (i = 0; i < SCO_MAX_RECORD; i++) {
		if (sco_context->record[i].sco_conn == conn) {
			return &sco_context->record[i];
		}
	}

	return NULL;
}

static void _btsrv_sco_connected_cb(struct bt_conn *conn, uint8_t err)
{
	uint8_t type;
	struct sco_info_record *record = btsrv_sco_find_record_by_conn(NULL);	/* Find free record */

	type = hostif_bt_conn_get_type(conn);
	if (type != BT_CONN_TYPE_SCO && type != BT_CONN_TYPE_SCO_SNOOP) {
		return;
	}

	if (record) {
		record->sco_conn = conn;
		record->frame_cnt = 0;
		record->frame_cached_len = 0;
		record->mute_frame = 0;
		record->check_mute_finish = 0;
		record->first_pkt_clk = BT_INVALID_BT_CLOCK;
	} else {
		SYS_LOG_ERR("Not more sco record");
	}

	/* ref for bt servcie process MSG_BTSRV_SCO_CONNECTED,
	 * need unref conn after process MSG_BTSRV_SCO_CONNECTED.
	 */
	hostif_bt_conn_ref(conn);
	btsrv_event_notify_ext(MSG_BTSRV_SCO, MSG_BTSRV_SCO_CONNECTED, conn, err);
}

static void _btsrv_sco_disconnected_cb(struct bt_conn *conn, uint8_t reason)
{
	uint8_t type;
	struct sco_info_record *record = btsrv_sco_find_record_by_conn(conn);

	type = hostif_bt_conn_get_type(conn);
	if (type != BT_CONN_TYPE_SCO && type != BT_CONN_TYPE_SCO_SNOOP) {
		return;
	}

	if (record) {
		record->sco_conn = NULL;
		record->frame_cnt = 0;
		record->frame_cached_len = 0;
		record->mute_frame = 0;
		record->check_mute_finish = 0;
		record->first_pkt_clk = BT_INVALID_BT_CLOCK;
	} else {
		SYS_LOG_ERR("Sco record not exist");
	}

	/* ref for bt servcie process MSG_BTSRV_SCO_DISCONNECTED,
	 * need unref conn after process MSG_BTSRV_SCO_DISCONNECTED.
	 */
	hostif_bt_conn_ref(conn);
	btsrv_event_notify_ext(MSG_BTSRV_SCO, MSG_BTSRV_SCO_DISCONNECTED, conn, reason);
}

static int _btsrv_sco_mscb_seq_convert(uint8_t seq)
{
	const uint8_t msbc_head_flag[4] = {0x08, 0x38, 0xc8, 0xf8};
	int i;

	for (i = 0; i < ARRAY_SIZE(msbc_head_flag); i++) {
		if (seq == msbc_head_flag[i]) {
			return i;
		}
	}
	return -1;
}

#ifdef DECODE_PLAYBACK_MCU
static int _btsrv_sco_insert_package(struct bt_conn *conn, uint8_t *data, uint8_t len, int num)
{
	SYS_LOG_INF("lost %d packages\n", num);

	while (num > 0) {
		data[0] = 0;
		data[1] = 1;
		btsrv_sco_user_callback(conn, BTSRV_SCO_DATA_INDICATED, data, BTSPEECH_SCO_PACKET_SIZE);
		sco_context->last_seq_no = (sco_context->last_seq_no & 0x03) + 1;
		num--;
	}
	return 0;
}
#endif

static int _btsrv_find_really_msbc_pkt(uint8_t *data_buf, uint16_t pkt_length)
{
    uint8_t i;
    uint8_t valid_len;
    uint8_t remain_len;

    for(i=3; i < (pkt_length - 2); i++)
    {
        if((data_buf[i] == 0xad) && (data_buf[i+1] == 0x00) && (data_buf[i+2] == 0x00))
        {
            remain_len = i - 2;
            valid_len = pkt_length - i + 2;
            memcpy(sco_context->sco_except_cache + sco_context->sco_except_cache_len, (uint8_t *)&data_buf[i-2], valid_len);
			sco_context->sco_except_cache_len += valid_len;
            //printk("get flag:%d_%d\n", remain_len, valid_len);
            return 0;
        }
    }

    return -1;
}

static int _btsrv_read_msbc_pkt(uint8_t *data, uint16_t pkt_length)
{
    int ret = -1;
	if((data[2] == 0xad) && (data[3] == 0x00) && (data[4] == 0x00))
	{
		sco_context->sco_except_cache_len = 0;
		ret = 0;
		goto exit;
	}

	//printk("wrong head:%d_%d\n", pkt_length, sco_context->sco_except_cache_len);
	if (sco_context->sco_except_cache_len > 0) {
		uint8_t remain_len;
		uint8_t start_len;
		uint8_t temp_buf[120];

		memset(temp_buf, 0, 120);
		start_len = sco_context->sco_except_cache_len;
		remain_len = pkt_length - sco_context->sco_except_cache_len;

		memcpy(temp_buf, sco_context->sco_except_cache, sco_context->sco_except_cache_len);
		sco_context->sco_except_cache_len = 0;

		memcpy(&temp_buf[start_len], data, remain_len);
		//printf("get pkt:%d_%d\n", remain_len, start_len);
		_btsrv_find_really_msbc_pkt(data, pkt_length);
		memcpy(data, temp_buf, pkt_length);
		ret = 0;

		goto exit;
	}

	if (_btsrv_find_really_msbc_pkt(data, pkt_length) < 0) {
		//printk("wrong pkt:%d\n", pkt_length);
		memset(data, 0, pkt_length);
		ret = 0;
	}

exit:
	return ret;
}

static void _btsrv_sco_convert_data_to_media(struct bt_conn *conn, uint8_t *data, uint8_t len, uint8_t pkg_flag, uint8_t codec_id)
{
	uint16_t seq_no = 0;

	if (len != BTSPEECH_SCO_PACKET_SIZE) {
		SYS_LOG_ERR("len error %d codec_id %d\n", len, codec_id);
		return;
	}

	if (codec_id == BT_CODEC_ID_MSBC) {
		if (pkg_flag) {
			seq_no = (sco_context->last_seq_no & 0x03) + 1;
		} else if(data[1] == 0) {
			pkg_flag = 1;
		} else {
			seq_no = _btsrv_sco_mscb_seq_convert(data[1]) + 1;
		}
	} else {
		seq_no = (sco_context->last_seq_no & 0x03) + 1;
	}
#ifdef DECODE_PLAYBACK_MCU
	if (codec_id == BT_CODEC_ID_MSBC) {
		if (audiolcy_is_low_latency_mode()) {
			if (sco_context->first_frame) {
				_btsrv_sco_insert_package(data, len, 2);
				sco_context->first_frame = 0;
				sco_context->drop_frame = 2;
			} else if(sco_context->drop_frame != 0){
				if(sco_context->drop_flag) {
					sco_context->drop_frame--;
					sco_context->drop_flag = 0;
					return;
				} else {
					sco_context->drop_flag = 1;
				}
			}
		}
		data[0] = 0;
		data[1] = pkg_flag;
		/* print_buffer((void *)data, 1, BTSPEECH_SCO_PACKET_SIZE, 16, -1); */
		btsrv_sco_user_callback(conn, BTSRV_SCO_DATA_INDICATED, data, BTSPEECH_SCO_PACKET_SIZE);
	} else {
		uint8_t send_data[62];

		send_data[0] = 0;
		send_data[1] = pkg_flag;
		memcpy(&send_data[2], data, BTSPEECH_SCO_PACKET_SIZE);
		/* print_buffer((void *)send_data, 1, BTSPEECH_SCO_PACKET_SIZE + 2, 16, -1); */
		btsrv_sco_user_callback(conn, BTSRV_SCO_DATA_INDICATED, send_data, BTSPEECH_SCO_PACKET_SIZE + 2);
	}
#else
	if (codec_id == BT_CODEC_ID_MSBC) {
		uint8_t send_data[68];
		uint16_t offset = 0;

		//*(uint16_t *)&send_data[offset] = pkg_flag;
		*(uint16_t *)(send_data + offset) = pkg_flag;
		offset += 2;

		//*(uint16_t *)&send_data[offset] = sco_context->frame_cnt;
		*(uint16_t *)(send_data + offset) = sco_context->frame_cnt;
		offset += 2;

		//*(uint16_t *)&send_data[offset]= len - 2;
		*(uint16_t *)(send_data + offset)= len - 2;
		offset += 2;

		//*(uint16_t *)&send_data[offset]= 2;
		*(uint16_t *)(send_data + offset) = 2;
		offset += 2;

		memcpy(&send_data[offset], &data[2], len - 2);
		/* print_buffer((void *)data, 1, BTSPEECH_SCO_PACKET_SIZE, 16, -1); */
		btsrv_sco_user_callback(conn, BTSRV_SCO_DATA_INDICATED, send_data, sizeof(send_data));
	} else {
		uint8_t send_data[68];
		uint16_t offset = 0;

		//*(uint16_t *)&send_data[offset] = pkg_flag;
		*(uint16_t *)(send_data+offset) = pkg_flag;
		offset += 2;

		//*(uint16_t *)&send_data[offset] = sco_context->frame_cnt;
		*(uint16_t *)(send_data+offset) = sco_context->frame_cnt;
		offset += 2;

		//*(uint16_t *)&send_data[offset]= len;
		*(uint16_t *)(send_data+offset)= len;
		offset += 2;

		//*(uint16_t *)&send_data[offset]= 0;
		*(uint16_t *)(send_data+offset)= 0;
		offset += 2;

		memcpy(&send_data[offset], data, len);
		/* print_buffer((void *)send_data, 1, sizeof(send_data), 16, -1); */
		btsrv_sco_user_callback(conn, BTSRV_SCO_DATA_INDICATED, send_data, sizeof(send_data));
	}
#endif
	sco_context->last_seq_no = seq_no;
}

static void _btsrv_sco_save_data_to_cache(struct bt_conn *conn, uint8_t *data, uint8_t len, uint8_t pkg_status_flag, uint8_t codec_id)
{
	if (!sco_context->sco_rx_cache) {
		SYS_LOG_ERR("Drop sco data");
		return;
	}

#if 0
	if (codec_id == BT_CODEC_ID_MSBC) {
		if (sco_context->sco_rx_pkt_cache_len != 0 && _btsrv_sco_mscb_seq_convert(data[1]) >= 0) {
			sco_context->sco_rx_pkt_cache_len = 0;
		} else if (sco_context->sco_rx_pkt_cache_len == 0 &&  _btsrv_sco_mscb_seq_convert(data[1]) < 0) {
			return;
		}
	}
#endif

	if (sco_context->sco_rx_pkt_cache_len != 0) {
		if (pkg_status_flag == 0 && sco_context->sco_rx_last_pkg_status_flag == 0) {
			memcpy(sco_context->sco_rx_cache + sco_context->sco_rx_pkt_cache_len, data, len);
			sco_context->sco_rx_last_pkg_status_flag = pkg_status_flag;
		} else {
			if (sco_context->sco_rx_last_pkg_status_flag == 0) {
				memset(sco_context->sco_rx_cache, 0, sco_context->sco_rx_pkt_cache_len);
			}
			memset(sco_context->sco_rx_cache + sco_context->sco_rx_pkt_cache_len, 0, len);
			sco_context->sco_rx_last_pkg_status_flag = pkg_status_flag;
			pkg_status_flag = 1;
		}
	} else {
		if (pkg_status_flag == 0) {
			memcpy(sco_context->sco_rx_cache + sco_context->sco_rx_pkt_cache_len, data, len);
		} else {
			memset(sco_context->sco_rx_cache, 0, len);
		}
		sco_context->sco_rx_last_pkg_status_flag = pkg_status_flag;
	}

	sco_context->sco_rx_pkt_cache_len += len;

	while (sco_context->sco_rx_pkt_cache_len >= BTSPEECH_SCO_PACKET_SIZE) {
	    int ret = 0;
		if (codec_id == BT_CODEC_ID_MSBC) {
			ret = _btsrv_read_msbc_pkt(sco_context->sco_rx_cache, BTSPEECH_SCO_PACKET_SIZE);
			if (ret) {
				memset(sco_context->sco_rx_cache, 0, BTSPEECH_SCO_PACKET_SIZE);
				pkg_status_flag = 1;
				ret = 0;
			}
		}

		if (ret == 0) {
			_btsrv_sco_convert_data_to_media(conn, sco_context->sco_rx_cache, BTSPEECH_SCO_PACKET_SIZE, pkg_status_flag, codec_id);
		}
		sco_context->sco_rx_pkt_cache_len -= BTSPEECH_SCO_PACKET_SIZE;
		memcpy(sco_context->sco_rx_cache, sco_context->sco_rx_cache + BTSPEECH_SCO_PACKET_SIZE, sco_context->sco_rx_pkt_cache_len);
	}
}

#ifdef CONFIG_DEBUG_DATA_RATE
static inline void _btsrv_sco_debug_data_rate(struct bt_conn *conn, uint8_t len, uint8_t pkg_flag)
{
	static int start_time_stamp;
	static int received_data;
	static int frame_cnt;
	static int err_frame_cnt;

	if (!start_time_stamp) {
		received_data = 0;
		start_time_stamp = k_cycle_get_32();
	}
	received_data += len;
	frame_cnt++;
	if (pkg_flag) {
		err_frame_cnt++;
	}
	if ((k_cycle_get_32() - start_time_stamp) > 5 * sys_clock_hw_cycles_per_sec) {
		if (start_time_stamp != 0) {
			SYS_LOG_INF("sco 0x%x data rate: %d b/s frame cnt (%d / %d) error rate %d %%",
					hostif_bt_conn_get_handle(conn),
					received_data / 5, err_frame_cnt, frame_cnt, err_frame_cnt * 100 / frame_cnt);
		}
		frame_cnt = 0;
		err_frame_cnt = 0;
		received_data = 0;
		start_time_stamp = k_cycle_get_32();
	}
}
#endif

static bool _btsrv_sco_check_mute_frame(uint8_t *data, uint8_t len)
{
	if (data[6] == 0 && data[7] == 0 && data[8] == 0 && data[9] == 0) {
		return true;
	}
	return false;
}

static void _btsrv_sco_data_avaliable_cb(struct bt_conn *conn, uint8_t *data, uint8_t len, uint8_t pkg_flag)
{
	uint8_t codec_id = 0, sample_rate = 0;
	struct bt_conn *base_conn = btsrv_rdm_get_base_conn_by_sco(conn);
	struct sco_info_record *record = btsrv_sco_find_record_by_conn(conn);
    int err;

	if (record) {
		record->frame_cached_len += len;
		if (record->frame_cached_len >= BTSPEECH_SCO_PACKET_SIZE) {
			record->frame_cnt++;
			record->frame_cached_len -= BTSPEECH_SCO_PACKET_SIZE;
		}

		if (record->frame_cnt < SCO_CHECK_DROP_MUTE_FRAME_CNT && !record->check_mute_finish) {
			if (!record->mute_frame) {
				if (_btsrv_sco_check_mute_frame(data, len)) {
					record->mute_frame = 1;
				} else {
					SYS_LOG_DBG("drop frame %d", record->frame_cnt);
					return;
				}
			} else {
				if (!_btsrv_sco_check_mute_frame(data, len)) {
					pkg_flag = 1;
					SYS_LOG_INF("fill zero  %d", record->frame_cnt);
				}
			}
		} else {
			record->check_mute_finish = 1;
		}
	} else {
		SYS_LOG_ERR("Can't find Sco record for conn 0x%x", hostif_bt_conn_get_handle(conn));
	}

	if (!base_conn ||
		(btsrv_rdm_hfp_get_actived_sco() != conn && base_conn != btsrv_rdm_get_tws_by_role(BTSRV_TWS_MASTER)
		 && base_conn != btsrv_rdm_get_tws_by_role(BTSRV_TWS_SLAVE))) {
		return;
	}

#ifdef CONFIG_SNOOP_LINK_TWS
	if (sco_context->sco_conn != conn) {
		return;
	}
#else
	if(base_conn == btsrv_rdm_get_tws_by_role(BTSRV_TWS_MASTER) && sco_context->sco_conn){
		btsrv_event_notify_malloc(MSG_BTSRV_TWS, MSG_BTSRV_TWS_SCO_DATA, data, len, len);
		return;
	}
#endif

	btsrv_rdm_hfp_get_codec_info(base_conn, &codec_id, &sample_rate);
	if (codec_id != BT_CODEC_ID_MSBC && codec_id != BT_CODEC_ID_CVSD && codec_id != BT_CODEC_ID_LC3_SWB) {
		SYS_LOG_ERR("error codec id %d\n", codec_id);
		return;
	}

	if (conn == btsrv_rdm_hfp_get_actived_sco()) {
		if (record) {
			sco_context->frame_cnt = record->frame_cnt;
		} else {
			sco_context->frame_cnt++;
		}
	}

#ifdef CONFIG_DEBUG_DATA_RATE
	_btsrv_sco_debug_data_rate(conn, len, pkg_flag);
#endif

    if (!btsrv_is_pts_test()) {
        if(pkg_flag == 0x02){
            sco_context->error_frame++;
            if(sco_context->error_frame >= 200){
                err = btsrv_adapter_do_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
                sco_context->error_frame = 0;
                SYS_LOG_ERR("disconnect sco self!\n");
            }
        }
        else{
            sco_context->error_frame = 0;
        }
    }

	if (len == BTSPEECH_SCO_PACKET_SIZE) {
		int ret = 0;

		if (codec_id == BT_CODEC_ID_MSBC) {
			ret = _btsrv_read_msbc_pkt(data, len);
			if (ret) {
				memset(data, 0, len);
				pkg_flag = 1;
				ret = 0;
			}
		}

		if (ret == 0) {
			_btsrv_sco_convert_data_to_media(conn, data, len, pkg_flag, codec_id);
		}
	} else {
		_btsrv_sco_save_data_to_cache(conn, data, len, pkg_flag, codec_id);
	}
}

static struct bt_conn_cb sco_conn_callbacks = {
	.connected = _btsrv_sco_connected_cb,
	.disconnected = _btsrv_sco_disconnected_cb,
	.rx_sco_data = _btsrv_sco_data_avaliable_cb,
};

/* The reason used sco_disconnect_timer
 * In btcall state, fast switch on/off phone audio(hfp profile) on phone,
 * when sco create less than 300ms, and do disconnect sco, will failed to disconnect sco,
 * add sco_disconnect_timer do disconnect sco when sco connected large than 300ms.
 */
static void btsrv_sco_start_stop_disconnect_timer(uint8_t start)
{
	if (start) {
		if (!thread_timer_is_running(&sco_context->sco_disconnect_timer)) {
			thread_timer_start(&sco_context->sco_disconnect_timer, SCO_DISCONNECT_TIMER_INTERVAL, SCO_DISCONNECT_TIMER_INTERVAL);
		}
	} else {
		if (thread_timer_is_running(&sco_context->sco_disconnect_timer)) {
			thread_timer_stop(&sco_context->sco_disconnect_timer);
		}
	}
}

static void connected_dev_cb_sco_disconnect(struct bt_conn *base_conn, uint8_t tws_dev, void *cb_param)
{
	int *need_timer = cb_param;
	uint32_t creat_time;
	struct bt_conn *sco_conn;
	struct bt_conn *second_conn;
	int other_state, err;

	if (tws_dev) {
		return;
	}

	if (btsrv_rdm_get_sco_disconnect_wait(base_conn)) {
		*need_timer = 1;

		creat_time = btsrv_rdm_get_sco_creat_time(base_conn);
		if ((os_uptime_get_32() - creat_time) > SCO_MIN_DISCONNECT_INTERVAL_TIME) {
			btsrv_rdm_set_sco_disconnect_wait(base_conn, 0);
			sco_conn = btsrv_rdm_get_sco_by_base_conn(base_conn);
			second_conn = btsrv_rdm_hfp_get_second_dev();
			other_state = btsrv_rdm_hfp_get_state(second_conn);

			if (sco_conn && (base_conn != btsrv_rdm_hfp_get_actived_dev() ||
				second_conn == NULL || other_state > BTSRV_HFP_STATE_LINKED)) {
				err = btsrv_adapter_do_conn_disconnect(sco_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
				if (!err) {
					btsrv_rdm_hfp_set_sco_state(base_conn, BTSRV_SCO_STATE_DISCONNECT);
				}
				SYS_LOG_INF("Timeout do disconnect sco err %d", err);
			}
		}
	}
}

static void btsrv_sco_disconnect_timer_handler(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	int need_timer = 0;

	btsrv_rdm_get_connected_dev(connected_dev_cb_sco_disconnect, &need_timer);
	if (!need_timer) {
		btsrv_sco_start_stop_disconnect_timer(0);
	}
}

void btsrv_sco_disconnect(struct bt_conn *sco_conn)
{
	uint32_t creat_time, diff;
	struct bt_conn *br_conn = hostif_bt_conn_get_acl_conn_by_sco(sco_conn);

	if (br_conn) {
		creat_time = btsrv_rdm_get_sco_creat_time(br_conn);
		diff = os_uptime_get_32() - creat_time;

		if (creat_time > 0 && diff > SCO_MIN_DISCONNECT_INTERVAL_TIME) {
			btsrv_rdm_set_sco_disconnect_wait(br_conn, 0);
			int err = btsrv_adapter_do_conn_disconnect(sco_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
			if (!err) {
				btsrv_rdm_hfp_set_sco_state(br_conn, BTSRV_SCO_STATE_DISCONNECT);
			}
			SYS_LOG_INF("Disconnect sco err %d", err);
		} else {
			SYS_LOG_INF("Delay disconnect sco");
			btsrv_rdm_set_sco_disconnect_wait(br_conn, 1);
			btsrv_sco_start_stop_disconnect_timer(1);
		}
	}
}

static void _btsrv_sco_connected(struct bt_conn *conn)
{
	struct bt_conn *acl_conn;

	if (sco_context) {
		acl_conn = hostif_bt_conn_get_acl_conn_by_sco(conn);
		if (!acl_conn || (btsrv_rdm_set_sco_connected(acl_conn, conn, 1) < 0)) {
			return;
		}

		if (!btsrv_info->allow_sco_connect) {
			/* disconnect sco if upper application(ota) not allow */
			btsrv_sco_disconnect(conn);
			return;
		}

		if (btsrv_rdm_get_hfp_role(acl_conn) == BTSRV_HFP_ROLE_HF)
			btsrv_hfp_proc_event(acl_conn, BTSRV_HFP_EVENT_SCO_ESTABLISHED);
		else
			btsrv_hfp_ag_proc_event(acl_conn, BTSRV_HFP_EVENT_SCO_ESTABLISHED);

		if (acl_conn == btsrv_rdm_hfp_get_actived_dev() ||
			acl_conn == btsrv_rdm_get_tws_by_role(BTSRV_TWS_SLAVE)) {
			if (!sco_context->sco_conn) {
				sco_context->sco_conn = conn;
				SYS_LOG_INF("sco_conn 0x%x", hostif_bt_conn_get_handle(conn));
			} else {
				/* Disconnect the unactive hfp sco conn */
				if (btsrv_rdm_get_snoop_role(acl_conn) == BTSRV_SNOOP_NONE) {
					btsrv_sco_disconnect(sco_context->sco_conn);
					SYS_LOG_INF("Disconnect sco_conn 0x%x", hostif_bt_conn_get_handle(sco_context->sco_conn));
				}

				btsrv_tws_active_sco_disconnected();
				/* Change to active hfp sco conn */
				sco_context->sco_conn = conn;
				SYS_LOG_INF("New sco_conn 0x%x", hostif_bt_conn_get_handle(conn));
			}
			sco_context->sco_rx_last_pkg_status_flag = 0;
			sco_context->sco_rx_pkt_cache_len = 0;
			sco_context->first_frame = 1;
			sco_context->drop_flag = 0;
			sco_context->frame_cnt = 0;
			sco_context->last_seq_no = 0;
			sco_context->sco_except_cache_len = 0;

			if (btsrv_rdm_get_snoop_role(acl_conn) == BTSRV_SNOOP_MASTER) {
#ifdef CONFIG_SNOOP_SYNC_LINK
				btsrv_event_notify(MSG_BTSRV_TWS, MSG_BTSRV_CREATE_SCO_SNOOP, conn);
#endif
			}
		} else {
			/* disconnect sco for deactive dev */
#ifdef CONFIG_SNOOP_LINK_TWS
			btsrv_sco_disconnect(conn);
#else
			if (acl_conn != btsrv_rdm_get_tws_by_role(BTSRV_TWS_MASTER)) {
				btsrv_sco_disconnect(conn);
			} else {
				btsrv_event_notify(MSG_BTSRV_TWS, MSG_BTSRV_SCO_CONNECTED, conn);
			}
#endif
		}
	}
}

static void _btsrv_snoop_sco_connected(struct bt_conn *conn)
{
	struct bt_conn *br_conn = hostif_bt_conn_get_acl_conn_by_sco(conn);

	if (sco_context) {
		if (!br_conn || (btsrv_rdm_set_sco_connected(br_conn, conn, 1) < 0)) {
			SYS_LOG_WRN("Rdm sco connected failed!");
			return;
		}

		SYS_LOG_INF("sco_conn 0x%x", hostif_bt_conn_get_handle(conn));
		sco_context->sco_conn = conn;
		sco_context->sco_rx_last_pkg_status_flag = 0;
		sco_context->sco_rx_pkt_cache_len = 0;
		sco_context->first_frame = 1;
		sco_context->drop_flag = 0;
		sco_context->frame_cnt = 0;
		sco_context->last_seq_no = 0;
		sco_context->sco_except_cache_len = 0;
	}
}

static void _btsrv_sco_disconnected(struct bt_conn *conn)
{
	struct bt_conn *br_conn = btsrv_rdm_get_base_conn_by_sco(conn);

	if (br_conn == NULL) {
		SYS_LOG_WRN("Not br conn for sco");
		return;
	}

	btsrv_rdm_set_sco_disconnect_wait(br_conn, 0);

	if (btsrv_info->allow_sco_connect) {
		if (btsrv_rdm_get_hfp_role(br_conn) == BTSRV_HFP_ROLE_HF) {
			btsrv_hfp_proc_event(br_conn, BTSRV_HFP_EVENT_SCO_RELEASED);
		} else {
			btsrv_hfp_ag_proc_event(br_conn, BTSRV_HFP_EVENT_SCO_RELEASED);
		}

#ifndef CONFIG_SNOOP_LINK_TWS
		if (br_conn == btsrv_rdm_get_tws_by_role(BTSRV_TWS_MASTER)) {
			btsrv_event_notify(MSG_BTSRV_TWS, MSG_BTSRV_SCO_DISCONNECTED, conn);
		}
#endif
	}

	if (sco_context && sco_context->sco_conn == conn) {
		sco_context->sco_conn = NULL;
		btsrv_tws_active_sco_disconnected();
		SYS_LOG_INF("sco_conn diconnected 0x%x", hostif_bt_conn_get_handle(conn));

		struct bt_conn *active_sco = btsrv_rdm_hfp_get_actived_sco();

		if (active_sco && active_sco != conn) {
			struct bt_conn *active_br_conn = btsrv_rdm_get_base_conn_by_sco(active_sco);
			SYS_LOG_INF("Resume 0x%x active sco 0x%x", hostif_bt_conn_get_handle(active_br_conn),
						hostif_bt_conn_get_handle(active_sco));
			sco_context->sco_conn = active_sco;
			if (btsrv_rdm_get_snoop_role(active_br_conn) == BTSRV_SNOOP_MASTER) {
#ifdef CONFIG_SNOOP_SYNC_LINK
				btsrv_event_notify(MSG_BTSRV_TWS, MSG_BTSRV_CREATE_SCO_SNOOP, active_sco);
#endif
			}
		}
	} else {
		SYS_LOG_INF("Unactive sco conn 0x%x active sco conn 0x%x", hostif_bt_conn_get_handle(conn),
					sco_context?hostif_bt_conn_get_handle(sco_context->sco_conn):0);
	}

	btsrv_rdm_set_sco_connected(br_conn, conn, 0);
	btsrv_rdm_hfp_set_siri_state(br_conn, HFP_SIRI_STATE_DEACTIVE);
}

static void _btsrv_snoop_sco_disconnected(struct bt_conn *conn)
{
	struct bt_conn *br_conn = btsrv_rdm_get_base_conn_by_sco(conn);

	if (br_conn == NULL) {
		SYS_LOG_WRN("Not br conn for snoop sco");
		return;
	}

	if(sco_context){
		if (sco_context->sco_conn == conn) {
			sco_context->sco_conn = NULL;
			btsrv_tws_active_sco_disconnected();
			SYS_LOG_INF("sco_conn diconnected 0x%x", hostif_bt_conn_get_handle(conn));
		} else {
			SYS_LOG_INF("Unactive sco conn 0x%x active sco conn 0x%x", hostif_bt_conn_get_handle(conn),
						hostif_bt_conn_get_handle(sco_context->sco_conn));
		}
	}

	btsrv_rdm_set_sco_connected(br_conn, conn, 0);
}

static int _btsrv_sco_init(btsrv_sco_callback cb)
{
	sco_context = &sco_context_info;
	memset(sco_context, 0, sizeof(struct btsrv_sco_context_info));

	sco_context->sco_rx_cache = media_mem_get_cache_pool(RX_SCO, AUDIO_STREAM_VOICE);

	thread_timer_init(&sco_context->sco_disconnect_timer, btsrv_sco_disconnect_timer_handler, NULL);
	sco_context->sco_except_cache = mem_malloc(SCO_EXCEPT_CACHE_SIZE);
	if(!sco_context->sco_except_cache){
		return -1;
	}	
	hostif_bt_conn_cb_register((struct bt_conn_cb *)&sco_conn_callbacks);
	sco_context->user_cb = cb;

	return 0;
}

static int _btsrv_sco_deinit(void)
{
	if (sco_context) {
		btsrv_sco_start_stop_disconnect_timer(0);
		if (sco_context->sco_except_cache) {
			mem_free(sco_context->sco_except_cache);
		}
		sco_context = NULL;
	}

	return 0;
}

static void btsrv_sco_do_disconnect(uint16_t acl_hdl)
{
	struct bt_conn *acl_conn, *sco_conn;

	acl_conn = btsrv_rdm_find_conn_by_hdl(acl_hdl);
	if (!acl_conn) {
		SYS_LOG_INF("Acl conn not exist");
		return;
	}

	sco_conn = btsrv_rdm_get_sco_by_base_conn(acl_conn);
	if (!sco_conn) {
		SYS_LOG_INF("Sco conn not exist");
		return;
	}

	btsrv_sco_disconnect(sco_conn);
}

int btsrv_sco_process(struct app_msg *msg)
{
	struct bt_conn *acl_conn = NULL;
	switch (_btsrv_get_msg_param_cmd(msg)) {
	case MSG_BTSRV_SCO_START:
		SYS_LOG_INF("MSG_BTSRV_SCO_START\n");
		_btsrv_sco_init(msg->ptr);
		break;
	case MSG_BTSRV_SCO_STOP:
		SYS_LOG_INF("MSG_BTSRV_SCO_STOP\n");
		_btsrv_sco_deinit();
		break;
	case MSG_BTSRV_SCO_DISCONNECT:
		SYS_LOG_INF("MSG_BTSRV_SCO_DISCONNECT");
		btsrv_sco_do_disconnect((uint16_t)(uint32_t)msg->ptr);
		break;
	case MSG_BTSRV_SCO_CONNECTED:
		acl_conn =hostif_bt_conn_get_acl_conn_by_sco(msg->ptr);
		SYS_LOG_INF("MSG_BTSRV_SCO_CONNECTED type 0x%x hdl 0x%x acl_hdl 0x%x",
					hostif_bt_conn_get_type(msg->ptr), 
					hostif_bt_conn_get_handle(msg->ptr),
					hostif_bt_conn_get_handle(acl_conn));		
		/* Master sco type is BT_CONN_TYPE_SCO, slave sco type is BT_CONN_TYPE_SCO_SNOOP */
		if (hostif_bt_conn_get_type(msg->ptr) == BT_CONN_TYPE_SCO) {
			_btsrv_sco_connected(msg->ptr);
		} else if (hostif_bt_conn_get_type(msg->ptr) == BT_CONN_TYPE_SCO_SNOOP) {
			btsrv_tws_update_sco_codec(msg->ptr);
			_btsrv_snoop_sco_connected(msg->ptr);
		}
		hostif_bt_conn_unref(msg->ptr);
		break;
	case MSG_BTSRV_SCO_DISCONNECTED:
		acl_conn =btsrv_rdm_get_base_conn_by_sco(msg->ptr);		
		SYS_LOG_INF("MSG_BTSRV_SCO_DISCONNECTED type 0x%x hdl 0x%x acl_hdl 0x%x",
					hostif_bt_conn_get_type(msg->ptr), 
					hostif_bt_conn_get_handle(msg->ptr),
					hostif_bt_conn_get_handle(acl_conn));		
		/* Master sco type is BT_CONN_TYPE_SCO, slave sco type is BT_CONN_TYPE_SCO_SNOOP */
		if (hostif_bt_conn_get_type(msg->ptr) == BT_CONN_TYPE_SCO) {
			_btsrv_sco_disconnected(msg->ptr);
		} else if (hostif_bt_conn_get_type(msg->ptr) == BT_CONN_TYPE_SCO_SNOOP) {
			_btsrv_snoop_sco_disconnected(msg->ptr);
		}
		hostif_bt_conn_unref(msg->ptr);		/* unref conn for ref in _btsrv_sco_disconnected_cb */
		break;
	}
	return 0;
}

void btsrv_sco_acl_disconnect_clear_sco_conn(struct bt_conn *sco_conn)
{
	if (sco_context->sco_conn == sco_conn) {
		btsrv_tws_active_sco_disconnected();
		sco_context->sco_conn = NULL;
	}
}

struct bt_conn *btsrv_sco_get_conn(void)
{
	return (sco_context) ? sco_context->sco_conn : NULL;
}

void btsrv_sco_set_first_pkt_clk(struct bt_conn *sco_conn, uint32_t clock)
{
	struct sco_info_record *record = btsrv_sco_find_record_by_conn(sco_conn);

	if (record) {
		record->first_pkt_clk = clock;
		SYS_LOG_INF("Set sco 0x%x sync_1st_pkt 0x%x", hostif_bt_conn_get_handle(sco_conn), clock);
	} else {
		SYS_LOG_INF("sco 0x%x record not exit", hostif_bt_conn_get_handle(sco_conn));
	}
}

uint32_t btsrv_sco_get_first_pkt_clk(struct bt_conn *sco_conn)
{
	struct sco_info_record *record;

	if (!sco_conn) {
		return BT_INVALID_BT_CLOCK;
	}

	record = btsrv_sco_find_record_by_conn(sco_conn);
	if (record) {
		return record->first_pkt_clk;
	} else {
		return BT_INVALID_BT_CLOCK;
	}
}

void btsrv_soc_hfp_active_dev_change(struct bt_conn *conn,struct bt_conn *last_conn)
{
	struct bt_conn *sco_con;

	btsrv_rdm_set_sco_disconnect_wait(conn, 0);

	sco_con = btsrv_rdm_get_sco_by_base_conn(last_conn);
	if (sco_con) {
		/* Disconnect non-active phone sco */
		btsrv_sco_disconnect(sco_con);
	}
}
