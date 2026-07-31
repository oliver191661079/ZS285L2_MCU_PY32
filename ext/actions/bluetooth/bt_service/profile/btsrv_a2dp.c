/*
 * Copyright (c) 2017 Actions Semi Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief btsrvice a2dp
 */

#define SYS_LOG_DOMAIN "btsrv_a2dp"
#include "btsrv_os_common.h"
#include "btsrv_inner.h"
#ifdef CONFIG_ACT_EVENT
#include <bt_act_event_id.h>
#include <logging/log_core.h>
LOG_MODULE_DECLARE(bt_br, CONFIG_ACT_EVENT_APP_COMPILE_LEVEL);
#endif

#define A2DP_CHECK_STATE_INTERVAL		50		/* 50ms */
#define A2DP_BREAK_BY_CALL_INTERVAL		500		/* 500ms */

#define AVRCP_CHECK_PROMPT_TONE_INTERVAL	200	/* 200ms */
#define AVRCP_CHECK_PROMPT_TONE_TIMEOUT		(2000 / AVRCP_CHECK_PROMPT_TONE_INTERVAL)
#define AVRCP_CHECK_POS_CHANGE_MAX_CNT		3

struct btsrv_a2dp_context_info {
	btsrv_a2dp_callback a2dp_user_callback;
	struct bt_a2dp_endpoint a2dp_sbc_endpoint[CONFIG_MAX_A2DP_ENDPOINT];
	struct bt_a2dp_endpoint a2dp_aac_endpoint[CONFIG_MAX_A2DP_ENDPOINT];
    struct bt_a2dp_endpoint a2dp_ldac_endpoint[CONFIG_MAX_A2DP_ENDPOINT];
	uint8_t a2dp_register_aac_num;
	uint8_t a2dp_register_ldac_num;
	struct thread_timer check_state_timer;
	struct thread_timer check_prompt_tone_timer;
};

struct btsrv_avrcp_getplaystatus_info {
	struct bt_conn *conn;
	uint8_t keep_cnt;
	uint8_t change_cnt;
	uint32_t song_len;
	uint32_t song_pos;
	uint8_t play_state;
} __packed;

static struct btsrv_a2dp_context_info a2dp_context;
struct btsrv_avrcp_getplaystatus_info avrcp_getplaystatus;

struct btsrv_a2dp_codec_cb {
	struct bt_conn *conn;
	uint8_t format;
	uint8_t sample_rate;
	uint8_t cp_type;
};

#if 0
static const uint8_t a2dp_sbc_codec[] = {
	BT_A2DP_AUDIO << 4,
	BT_A2DP_SBC,
	0xFF,	/* (SNK mandatory)44100, 48000, mono, dual channel, stereo, join stereo */
			/* (SNK optional) 16000, 32000 */
	0xFF,	/* (SNK mandatory) Block length: 4/8/12/16, subbands:4/8, Allocation Method: SNR, Londness */
	0x02,	/* min bitpool */
	0x35	/* max bitpool */
};

static const uint8_t a2dp_aac_codec[] = {
	BT_A2DP_AUDIO << 4,
	BT_A2DP_MPEG2,
	0xF0,	/* MPEG2 AAC LC, MPEG4 AAC LC, MPEG AAC LTP, MPEG4 AAC Scalable */
	0x01,	/* Sampling Frequecy 44100 */
	0x8F,	/* Sampling Frequecy 48000, channels 1, channels 2 */
	0xFF,	/* VBR, bit rate */
	0xFF,	/* bit rate */
	0xFF	/* bit rate */
};
#endif

static void btsrv_a2dp_state_ev_update_prio(struct bt_conn *conn, uint8_t event);
static void btsrv_a2dp_state_ev_update_play(struct bt_conn *conn, uint8_t event);

static void _btsrv_a2dp_connect_cb(struct bt_conn *conn,u8_t session_type)
{
	char addr_str[BT_ADDR_STR_LEN];
	bt_addr_t *bt_addr = NULL;

	bt_addr = (bt_addr_t *)GET_CONN_BT_ADDR(conn);
	hostif_bt_addr_to_str(bt_addr, addr_str, BT_ADDR_STR_LEN);
	SYS_LOG_INF("connected:%s type:%d", addr_str, session_type);

    if(session_type == BT_AVDTP_SIGNALING_SESSION){
        btsrv_adapter_callback(BTSRV_REQ_HIGH_PERFORMANCE, NULL);
        btsrv_event_notify(MSG_BTSRV_CONNECT, MSG_BTSRV_A2DP_SIGNALING_CONNECTED, conn);
    }
	else{
        btsrv_event_notify(MSG_BTSRV_CONNECT, MSG_BTSRV_A2DP_CONNECTED, conn);
	}
}

static void _btsrv_a2dp_disconnected_cb(struct bt_conn *conn,u8_t session_type)
{
	char addr_str[BT_ADDR_STR_LEN];
	bt_addr_t *bt_addr = NULL;

	bt_addr = (bt_addr_t *)GET_CONN_BT_ADDR(conn);
	hostif_bt_addr_to_str(bt_addr, addr_str, BT_ADDR_STR_LEN);
	SYS_LOG_INF("disconnected:%s type:%d", addr_str,session_type);
	btsrv_adapter_callback(BTSRV_REQ_HIGH_PERFORMANCE, NULL);
	btsrv_event_notify(MSG_BTSRV_A2DP, MSG_BTSRV_A2DP_DISCONNECTED, conn);
}

#ifdef CONFIG_DEBUG_DATA_RATE
#define TEST_RECORD_NUM			10
static uint32_t pre_rx_time;
static uint8_t test_index;
static uint16_t test_rx_record[TEST_RECORD_NUM][2];

static inline void _btsrv_a2dp_debug_data_rate(struct bt_conn *conn, uint16_t len)
{
	static int start_time_stamp;
	static int received_data;
	uint32_t curr_time;

	if (!start_time_stamp) {
		received_data = 0;
		start_time_stamp = k_cycle_get_32();
	}
	received_data += len;
	if ((k_cycle_get_32() - start_time_stamp) > 5 * sys_clock_hw_cycles_per_sec) {
		if (start_time_stamp != 0) {
			SYS_LOG_INF("hdl 0x%x, a2dp data rate: %d bytes/s", hostif_bt_conn_get_handle(conn), received_data / 5);
		}
		received_data = 0;
		start_time_stamp = k_cycle_get_32();
	}

	curr_time = os_uptime_get_32();
	test_rx_record[test_index][0] = curr_time - pre_rx_time;
	test_rx_record[test_index][1] = len;
	test_index++;
	if (test_index >= TEST_RECORD_NUM) {
		test_index = 0;
	}
	pre_rx_time = curr_time;
}

void debug_dump_media_rx_state(void)
{
	static uint32_t pre_dump_time;
	uint32_t curr_time;
	uint16_t time_cnt = 0, len_cnt = 0;
	int flag, i;

	curr_time = os_uptime_get_32();
	if ((curr_time - pre_dump_time) < 500) {
		return;
	}
	pre_dump_time = curr_time;

	flag = btsrv_set_negative_prio();

	printk("diff_time data_len\n");
	for (i = test_index; i < TEST_RECORD_NUM; i++) {
		time_cnt += test_rx_record[i][0];
		len_cnt += test_rx_record[i][1];
		printk("%d(%d)\t %d(%d)\n", test_rx_record[i][0], time_cnt, test_rx_record[i][1], len_cnt);
	}

	for (i = 0; i < test_index; i++) {
		time_cnt += test_rx_record[i][0];
		len_cnt += test_rx_record[i][1];
		printk("%d(%d)\t %d(%d)\n", test_rx_record[i][0], time_cnt, test_rx_record[i][1], len_cnt);
	}

	printk("%d(%d)\n", curr_time - pre_rx_time, time_cnt + (curr_time - pre_rx_time));
	btsrv_revert_prio(flag);
}
#else
void debug_dump_media_rx_state(void)
{
}
#endif

#if 1		/* Pack stream header for bt earphone specsification */
struct avdtp_data_header_t
{
    uint16_t frame_cnt;
    uint16_t seq_no;
    uint16_t frame_len;  //stream len
    uint16_t padding_len;
} __packed;

uint8_t btsrv_a2dp_pack_date_header(struct bt_conn *conn, uint8_t *data, uint16_t media_len, uint8_t head_len, uint8_t format, uint8_t *padding_len)
{
	struct avdtp_data_header_t header;
	uint8_t pack_header_size = sizeof(struct avdtp_data_header_t);
	uint8_t start_pos;

	/* data start from RTTP */
	if (format == BT_A2DP_SBC || format == BT_A2DP_LDAC) {
		header.frame_cnt = (uint16_t)(data[head_len - 1]&0x0F);
	} else {
		header.frame_cnt = 1;
	}

	header.seq_no = data[3] | ((uint16_t)(data[2]) << 8);
	header.frame_len = media_len;
	header.padding_len = media_len % 2;

	*padding_len = header.padding_len;
	start_pos = head_len - pack_header_size;
	memcpy(&data[start_pos], (uint8_t*)&header, pack_header_size);

#if 1
	static struct bt_conn *rec_seq_conn;
	static uint16_t rec_seq_no;
	static uint32_t rec_time;
	uint32_t curr_time;

	curr_time = os_uptime_get_32();
	if (rec_seq_conn != conn || ((curr_time - rec_time) > 300)) {
		rec_seq_conn = conn;
		rec_seq_no = header.seq_no;
	} else {
		rec_seq_no += 1;
		if (rec_seq_no != header.seq_no) {
			SYS_LOG_ERR("pkt miss %d %d\n", header.seq_no, (rec_seq_no - 1));
			SYS_EVENT_INF(EVENT_BT_A2DP_PKT_MISS, header.seq_no, (rec_seq_no-1), os_uptime_get_32());
		}
		rec_seq_no = header.seq_no;
	}

	rec_time = curr_time;
#endif

	return start_pos;
}
#else
static uint8_t btsrv_a2dp_pack_date_header(struct bt_conn *conn, uint8_t *data, uint16_t media_len, uint8_t head_len, uint8_t format, uint8_t *padding_len)
{
    *padding_len = 0;
	return head_len;
}
#endif

/** this callback dircty call to app, will in bt stack context */
static void _btsrv_a2dp_media_handler_cb(struct bt_conn *conn, uint8_t *data, uint16_t len)
{
	static uint16_t callback_handle;
	uint8_t head_len, format, sample_rate, cp_type, padding_len;

    if(!btsrv_is_pts_test()){
        if(btsrv_rdm_is_wait_to_diconnect(conn)){
            return;
        }
        else if(!btsrv_rdm_is_avrcp_connected(conn)){
            if(btsrv_rdm_is_avrcp_connected_pending(conn)){
                return;
            }
        }
        else{
            if(btsrv_rdm_is_avrcp_playing_pending(conn)){
                return;
            }
        }
    }

	if (btsrv_rdm_get_a2dp_pending_ahead_start(conn)) {
		btsrv_a2dp_media_state_change(conn, BT_A2DP_MEDIA_STATE_START);
	}

#ifdef CONFIG_SUPPORT_TWS
	if (btsrv_tws_protocol_data_cb(conn, data, len)) {
		return;
	}
#endif

	if (btsrv_adapter_leaudio_is_foreground_dev()){
		return;
	}

	/* Must get info after btsrv_tws_protocol_data_cb,
	 * tws slave update format in btsrv_tws_protocol_data_cb
	 */
	btsrv_rdm_a2dp_get_codec_info(conn, &format, &sample_rate, &cp_type);

	switch (format) {
	case BT_A2DP_SBC:
	case BT_A2DP_MPEG2:
	case BT_A2DP_LDAC:
#if 1 
		if (btsrv_rdm_a2dp_get_active_dev() != conn)
#else
		if (!(btsrv_rdm_a2dp_get_active_dev() == conn ||
			btsrv_rdm_get_dev_role() == BTSRV_TWS_SLAVE))
#endif
		{
            if (btsrv_rdm_set_deactive_cnt(conn) > 500) {
                btsrv_rdm_clr_deactive_cnt(conn);
				//bug94
				#if 0
                SYS_LOG_INF("Nonactive 0x%x a2dp data", hostif_bt_conn_get_handle(conn));
                if(!btsrv_rdm_a2dp_get_active(conn)){
                    btsrv_rdm_set_a2dp_pending_ahead_start(conn,1);
                    btsrv_a2dp_media_state_change(conn,BT_A2DP_MEDIA_STATE_START);
                }
				#endif
			}
			break;
		}

		if (format == BT_A2DP_SBC) {
			head_len = AVDTP_SBC_HEADER_LEN;
		} else if (format == BT_A2DP_LDAC) {
			head_len = AVDTP_LDAC_HEADER_LEN;
		} else {
			head_len = AVDTP_AAC_HEADER_LEN;
		}

		if (cp_type == BT_AVDTP_AV_CP_TYPE_SCMS_T) {
			head_len++;
		}
 
        if(len < head_len){
            btsrv_event_notify(MSG_BTSRV_CONNECT, MSG_BTSRV_A2DP_CHANNEL_ERR, conn);
            return;
        }

#ifdef CONFIG_DEBUG_DATA_RATE
		_btsrv_a2dp_debug_data_rate(conn, len - head_len);
#endif

#if (CONFIG_A2DP_PACK_DATE_HEADER)
		head_len = btsrv_a2dp_pack_date_header(conn, data, (len - head_len), head_len, format, &padding_len);
#else
        padding_len = 0;
#endif
//		SYS_LOG_INF("BTSRV_A2DP_DATA_INDICATED:\n");
		btsrv_a2dp_user_callback(conn, BTSRV_A2DP_DATA_INDICATED, data + head_len, (len - head_len + padding_len));
		if (callback_handle != hostif_bt_conn_get_handle(conn)) {
			callback_handle = hostif_bt_conn_get_handle(conn);
			SYS_LOG_INF("Change media cb handle 0x%x", callback_handle);
		}
		break;
	default:
		SYS_LOG_INF("A2dp not support type: 0x%x, 0x%x, %d\n", data[0], data[1], format);
		break;
	}

}

static int _btsrv_a2dp_media_state_req_cb(struct bt_conn *conn, uint8_t state)
{
	btsrv_event_notify_ext(MSG_BTSRV_A2DP, MSG_BTSRV_A2DP_MEDIA_STATE_CB, conn, state);
	return 0;
}

#define A2DP_AAC_FREQ_NUM		12
const static uint8_t a2dp_aac_freq_table[A2DP_AAC_FREQ_NUM] = {96, 88, 64, 48, 44, 32, 24, 22, 16, 12, 11, 8};

static uint8_t btsrv_convert_sample_rate(struct bt_a2dp_media_codec *codec)
{
	uint8_t sample_rate = 44, i;
	uint16_t freq;

	if (codec->head.codec_type == BT_A2DP_SBC) {
		switch (codec->sbc.freq) {
		case BT_A2DP_SBC_48000:
			sample_rate = 48;
			break;
		case BT_A2DP_SBC_44100:
			sample_rate = 44;
			break;
		case BT_A2DP_SBC_32000:
			sample_rate = 32;
			break;
		case BT_A2DP_SBC_16000:
			sample_rate = 16;
			break;
		}
	} else if (codec->head.codec_type == BT_A2DP_MPEG2) {
		freq = (codec->aac.freq0 << 4) | codec->aac.freq1;
		for (i = 0; i < A2DP_AAC_FREQ_NUM; i++) {
			if (freq & (BT_A2DP_AAC_96000 << i)) {
				sample_rate = a2dp_aac_freq_table[i];
				break;
			}
		}
	} else if (codec->head.codec_type == BT_A2DP_VENDOR) {
		if(codec->vendor.vendor_id == BT_A2DP_VENDOR_LDAC && 
			codec->vendor.vendor_codec_id == BT_A2DP_CODEC_LDAC){
			SYS_LOG_INF("%d\n",codec->vendor.spec_data[0]);
			switch (codec->vendor.spec_data[0]) {
			case BT_A2DP_LDAC_SF_44100:
				sample_rate = 44;
				break;
			case BT_A2DP_LDAC_SF_48000:
				sample_rate = 48;
				break;
			case BT_A2DP_LDAC_SF_88200:
				sample_rate = 88;
				break;
			case BT_A2DP_LDAC_SF_96000:
				sample_rate = 96;
				break;
			}
		}
	}


	return sample_rate;
}

static void _btsrv_a2dp_seted_codec_cb(struct bt_conn *conn, struct bt_a2dp_media_codec *codec, uint8_t cp_type)
{
	struct btsrv_a2dp_codec_cb param;

	param.conn = conn;
	param.format = codec->head.codec_type;
	if(param.format == BT_A2DP_VENDOR){
		if(codec->vendor.vendor_id == BT_A2DP_VENDOR_LDAC && 
			codec->vendor.vendor_codec_id == BT_A2DP_CODEC_LDAC){
			param.format = BT_A2DP_LDAC;
		}
	}
	param.sample_rate = btsrv_convert_sample_rate(codec);
	param.cp_type = cp_type;
	btsrv_event_notify_malloc(MSG_BTSRV_A2DP, MSG_BTSRV_A2DP_SET_CODEC_CB, (uint8_t *)&param, sizeof(param), 0);
}

static void _btsrv_a2dp_exception_handle_cb(struct bt_conn *conn,u8_t err)
{
    if(!conn)
        return;

    SYS_LOG_ERR("conn:%p,err:%d",conn,err);

    btsrv_event_notify(MSG_BTSRV_CONNECT, MSG_BTSRV_A2DP_CHANNEL_ERR, conn);
}

static const struct bt_a2dp_app_cb btsrv_a2dp_cb = {
	.connected = _btsrv_a2dp_connect_cb,
	.disconnected = _btsrv_a2dp_disconnected_cb,
	.media_handler = _btsrv_a2dp_media_handler_cb,
	.media_state_req = _btsrv_a2dp_media_state_req_cb,
	.seted_codec = _btsrv_a2dp_seted_codec_cb,
	.exception_handler = _btsrv_a2dp_exception_handle_cb,
};

void btsrv_a2dp_halt_aac_endpoint(bool halt)
{
	int ret, i;

	for (i = 0; i < a2dp_context.a2dp_register_aac_num; i++) {
		ret = hostif_bt_a2dp_halt_endpoint(&a2dp_context.a2dp_aac_endpoint[i], halt);
		SYS_LOG_INF("%s AAC endpoint %d", (halt ? "Halt" : "Resume"), ret);
	}
}

void btsrv_a2dp_halt_ldac_endpoint(bool halt)
{
	int ret, i;

	for (i = 0; i < a2dp_context.a2dp_register_ldac_num; i++) {
		ret = hostif_bt_a2dp_halt_endpoint(&a2dp_context.a2dp_ldac_endpoint[i], halt);
		SYS_LOG_INF("%s LDAC endpoint %d", (halt ? "Halt" : "Resume"), ret);
	}
}

int btsrv_a2dp_media_state_change(struct bt_conn *conn, uint8_t state)
{
	return _btsrv_a2dp_media_state_req_cb(conn, state);
}

bool btsrv_a2dp_is_stream_start(void)
{
	struct bt_conn *conn = btsrv_rdm_a2dp_get_active_dev();
	struct bt_conn *second_conn = btsrv_rdm_a2dp_get_second_dev();

	if (conn && btsrv_rdm_is_a2dp_stream_open(conn)) {
		return true;
	} else if (second_conn && btsrv_rdm_is_a2dp_stream_open(second_conn)) {
		return true;
	} else {
		return false;
	}
}

void btsrv_a2dp_user_callback(struct bt_conn *conn, btsrv_a2dp_event_e event,  void *param, int param_size)
{
	uint8_t codec_info[2];
	uint16_t hdl = hostif_bt_conn_get_handle(conn);

	switch (event) {
	case BTSRV_A2DP_DATA_INDICATED:
	case BTSRV_A2DP_GET_INIT_DELAY_REPORT:
	case BTSRV_A2DP_REQ_DELAY_CHECK_START:
	case BTSRV_A2DP_NON_PROMPT_TONE_STARTED:
		break;
	default:
		btsrv_tws_sync_info_type(conn, BTSRV_SYNC_TYPE_A2DP, event);
		break;
	}

	if (event == BTSRV_A2DP_STREAM_STARTED ||
		event == BTSRV_A2DP_STREAM_CHECK_STARTED ||
		event == BTSRV_A2DP_NON_PROMPT_TONE_STARTED) {
		btsrv_link_adjust_quickly();		/* Adjust link prioroty before play */

		/* Callback codec info before stream start */
		btsrv_rdm_a2dp_get_codec_info(conn, &codec_info[0], &codec_info[1], NULL);
		a2dp_context.a2dp_user_callback(hdl, BTSRV_A2DP_CODEC_INFO, (void *)codec_info, sizeof(codec_info));
	}

	a2dp_context.a2dp_user_callback(hdl, event, param, param_size);
}

bool btsrv_a2dp_stream_open_used_start(struct bt_conn *conn)
{
	uint8_t prio, cmp_prio;

	prio = btsrv_rdm_a2dp_get_priority(conn);
	cmp_prio = A2DP_PRIOROTY_STREAM_OPEN | A2DP_PRIOROTY_USER_START | A2DP_PRIOROTY_AVRCP_PLAY;

	if ((prio & cmp_prio) == cmp_prio) {
		return true;
	} else {
		return false;
	}
}

void btsrv_a2dp_start_stop_check_prio_timer(uint8_t start)
{
	if (start) {
		if (!thread_timer_is_running(&a2dp_context.check_state_timer)) {
			thread_timer_start(&a2dp_context.check_state_timer, A2DP_CHECK_STATE_INTERVAL, A2DP_CHECK_STATE_INTERVAL);
		}
	} else {
		if (thread_timer_is_running(&a2dp_context.check_state_timer)) {
			thread_timer_stop(&a2dp_context.check_state_timer);
		}
	}
}

#ifdef CONFIG_BT_PHONE_FILTER_PROMPT_TONE
static void btsrv_a2dp_check_prompt_tone_playstatus(struct bt_conn *conn, uint8_t reset)
{
	if (!conn)
		return;

	avrcp_getplaystatus.conn = conn;
	if (reset) {
		avrcp_getplaystatus.song_len = GETPLAYSTATUS_INVALID_VALUE;
		avrcp_getplaystatus.song_pos = GETPLAYSTATUS_INVALID_VALUE;
		avrcp_getplaystatus.change_cnt = 0;
		avrcp_getplaystatus.keep_cnt = 0;
		avrcp_getplaystatus.play_state = BT_AVRCP_PLAYBACK_STATUS_PAUSED;
	}
	btif_avrcp_get_play_status(conn);
}
#endif

#ifdef CONFIG_BT_PHONE_FILTER_PROMPT_TONE
static void btsrv_a2dp_check_prompt_tone_timer_handler(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	uint8_t need_change = 0;
	uint8_t play_state;
	uint32_t song_len, song_pos;
	struct bt_conn *conn = (struct bt_conn *)expiry_fn_arg;
	struct bt_conn *active_conn = btsrv_rdm_a2dp_get_active_dev();
	struct bt_conn *second_conn = btsrv_rdm_a2dp_get_second_dev();

	if ((!conn) ||
		(conn != avrcp_getplaystatus.conn)) {
		SYS_LOG_ERR("%p, %p\r\n", conn, avrcp_getplaystatus.conn);
		return;
	}

	btsrv_rdm_get_avrcp_playstatus_info(conn, &song_len, &song_pos, &play_state);
	if ((play_state == BT_AVRCP_PLAYBACK_STATUS_STOPPED) ||
		(play_state == BT_AVRCP_PLAYBACK_STATUS_PAUSED)) {
		avrcp_getplaystatus.keep_cnt = 0;
		avrcp_getplaystatus.change_cnt = 0;

		//from BT_AVRCP_PLAYBACK_STATUS_PLAYING to BT_AVRCP_PLAYBACK_STATUS_PAUSED
		if(avrcp_getplaystatus.play_state == BT_AVRCP_PLAYBACK_STATUS_PLAYING) {
			SYS_LOG_INF("play_state_change: %x, %x, %x\r\n", play_state, avrcp_getplaystatus.play_state, hostif_bt_conn_get_handle(conn));
			thread_timer_stop(&a2dp_context.check_prompt_tone_timer);
		}else {
			SYS_LOG_INF("play_state: %x, %x, %x\r\n", play_state, avrcp_getplaystatus.play_state, hostif_bt_conn_get_handle(conn));
		}
	}else {
		if (song_pos != GETPLAYSTATUS_INVALID_VALUE) {
			SYS_LOG_INF("info: %x, %x\r\n", song_pos, avrcp_getplaystatus.song_pos);
			if (song_pos != avrcp_getplaystatus.song_pos) {
				avrcp_getplaystatus.song_pos = song_pos;
				avrcp_getplaystatus.keep_cnt = 0;
				avrcp_getplaystatus.change_cnt++;
				if (avrcp_getplaystatus.change_cnt >= AVRCP_CHECK_POS_CHANGE_MAX_CNT) {
					need_change = 1;
					SYS_LOG_INF("need_change_1\r\n");
				}else {
					SYS_LOG_INF("song_pos update\r\n");
				}
			}else {
				avrcp_getplaystatus.keep_cnt++;
				avrcp_getplaystatus.change_cnt = 0;
				//Continuous 5s without changing
				if (avrcp_getplaystatus.keep_cnt >= AVRCP_CHECK_PROMPT_TONE_TIMEOUT) {
					//double phone
					if ((active_conn) &&
						(second_conn) &&
						btsrv_rdm_is_a2dp_stream_open(second_conn)) {
						avrcp_getplaystatus.keep_cnt = 0;
						avrcp_getplaystatus.change_cnt = 0;
						thread_timer_stop(&a2dp_context.check_prompt_tone_timer);
					}else {
						need_change = 1;
						SYS_LOG_INF("need_change_2:%x, %x, %x\r\n", hostif_bt_conn_get_handle(conn), 
									hostif_bt_conn_get_handle(active_conn), hostif_bt_conn_get_handle(second_conn));
					}
				}
			}
		}else {
			avrcp_getplaystatus.keep_cnt = 0;
			avrcp_getplaystatus.change_cnt = 0;
		}
	}

	SYS_LOG_INF("%x, %x, %x, 0x%x\r\n", need_change, avrcp_getplaystatus.change_cnt,
				avrcp_getplaystatus.keep_cnt, hostif_bt_conn_get_handle(conn));

	avrcp_getplaystatus.play_state = play_state;

	if (need_change) {
		avrcp_getplaystatus.keep_cnt = 0;
		avrcp_getplaystatus.change_cnt = 0;
		thread_timer_stop(&a2dp_context.check_prompt_tone_timer);
		btsrv_a2dp_state_ev_update_prio(conn, BTSRV_A2DP_NON_PROMPT_TONE_STARTED);
		btsrv_a2dp_state_ev_update_play(conn, BTSRV_A2DP_NON_PROMPT_TONE_STARTED);
	}else {
		btsrv_a2dp_check_prompt_tone_playstatus(conn, 0);
	}
}
#endif


static void btsrv_a2dp_state_ev_update_prio(struct bt_conn *conn, uint8_t event)
{
	uint8_t sync_active = 0, sync_second = 0;
	struct bt_conn *active_conn = btsrv_rdm_a2dp_get_active_dev();
	struct bt_conn *second_conn = btsrv_rdm_a2dp_get_second_dev();

	if (event == BTSRV_A2DP_STREAM_STARTED) {
		btsrv_rdm_a2dp_set_clear_priority(conn, (A2DP_PRIOROTY_STREAM_OPEN | A2DP_PRIOROTY_USER_START | A2DP_PRIOROTY_AVRCP_PLAY), 1);
	} else if (event == BTSRV_A2DP_STREAM_SUSPEND) {
		btsrv_rdm_a2dp_set_clear_priority(conn, (A2DP_PRIOROTY_STREAM_OPEN | A2DP_PRIOROTY_USER_START | A2DP_PRIOROTY_AVRCP_PLAY), 0);
	}

	if (second_conn) {
		if (conn == active_conn) {
			/* Conn is active device */
			if (event == BTSRV_A2DP_STREAM_STARTED) {
#ifdef CONFIG_BT_PHONE_FILTER_PROMPT_TONE
				if (thread_timer_is_running(&a2dp_context.check_prompt_tone_timer)) {
					thread_timer_stop(&a2dp_context.check_prompt_tone_timer);
				}else {
					thread_timer_init(&a2dp_context.check_prompt_tone_timer, btsrv_a2dp_check_prompt_tone_timer_handler, active_conn);
				}
				btsrv_a2dp_check_prompt_tone_playstatus(active_conn, 1);
				thread_timer_start(&a2dp_context.check_prompt_tone_timer, AVRCP_CHECK_PROMPT_TONE_INTERVAL, AVRCP_CHECK_PROMPT_TONE_INTERVAL);;
				SYS_LOG_INF("check_prompt_tone_timer phone start 1: %x", hostif_bt_conn_get_handle(active_conn));
#else
				btsrv_rdm_a2dp_set_clear_priority(active_conn, A2DP_PRIOROTY_FIRST_USED, 1);
				btsrv_rdm_a2dp_set_clear_priority(active_conn, (A2DP_PRIOROTY_STOP_WAIT | A2DP_PRIOROTY_CALL_WAIT), 0);
				btsrv_rdm_a2dp_set_clear_priority(second_conn, A2DP_PRIOROTY_FIRST_USED, 0);
				sync_active = 1;
				sync_second = 1;
#endif
			} else if (event == BTSRV_A2DP_STREAM_SUSPEND) {
#ifdef CONFIG_BT_PHONE_FILTER_PROMPT_TONE
				if ((conn == avrcp_getplaystatus.conn) && (thread_timer_is_running(&a2dp_context.check_prompt_tone_timer))) {
					thread_timer_stop(&a2dp_context.check_prompt_tone_timer);
				}
#endif
				btsrv_rdm_a2dp_set_clear_priority(active_conn, A2DP_PRIOROTY_STOP_WAIT, 1);
				sync_active = 1;
			} else if(event == BTSRV_A2DP_NON_PROMPT_TONE_STARTED) {
#ifdef CONFIG_BT_PHONE_FILTER_PROMPT_TONE
				SYS_LOG_INF("BTSRV_A2DP_NON_PROMPT_TONE_STARTED_1: %x", hostif_bt_conn_get_handle(active_conn));
				btsrv_rdm_a2dp_set_clear_priority(active_conn, A2DP_PRIOROTY_FIRST_USED, 1);
				btsrv_rdm_a2dp_set_clear_priority(active_conn, (A2DP_PRIOROTY_STOP_WAIT | A2DP_PRIOROTY_CALL_WAIT), 0);
				btsrv_rdm_a2dp_set_clear_priority(second_conn, A2DP_PRIOROTY_FIRST_USED, 0);
				btsrv_rdm_a2dp_set_clear_priority(second_conn, (A2DP_PRIOROTY_STOP_WAIT | A2DP_PRIOROTY_CALL_WAIT), 0);
				sync_active = 1;
				sync_second = 1;
#endif
			}
		} else {
			/* Conn is second device */
			if (event == BTSRV_A2DP_STREAM_STARTED)
#ifdef CONFIG_BT_PHONE_FILTER_PROMPT_TONE
			{
				if (btsrv_stop_another_when_one_playing() || !btsrv_a2dp_stream_open_used_start(active_conn)) {
					//position timer BTSRV_A2DP_NON_PROMPT_TONE_STARTED
					if (thread_timer_is_running(&a2dp_context.check_prompt_tone_timer)) {
						thread_timer_stop(&a2dp_context.check_prompt_tone_timer);
					}else {
						thread_timer_init(&a2dp_context.check_prompt_tone_timer, btsrv_a2dp_check_prompt_tone_timer_handler, second_conn);
					}
					btsrv_a2dp_check_prompt_tone_playstatus(second_conn, 1);
					thread_timer_start(&a2dp_context.check_prompt_tone_timer, AVRCP_CHECK_PROMPT_TONE_INTERVAL, AVRCP_CHECK_PROMPT_TONE_INTERVAL);
					SYS_LOG_INF("check_prompt_tone_timer phone start 2: %x", hostif_bt_conn_get_handle(second_conn));
				}
			}else if(event == BTSRV_A2DP_NON_PROMPT_TONE_STARTED) {
				if ((btsrv_stop_another_when_one_playing() || !btsrv_a2dp_stream_open_used_start(active_conn)) && 
					(btsrv_a2dp_stream_open_used_start(second_conn))) {
					SYS_LOG_INF("BTSRV_A2DP_NON_PROMPT_TONE_STARTED_2: %x", hostif_bt_conn_get_handle(second_conn));
					btsrv_rdm_a2dp_set_clear_priority(second_conn, A2DP_PRIOROTY_FIRST_USED, 1);
					btsrv_rdm_a2dp_set_clear_priority(active_conn, A2DP_PRIOROTY_FIRST_USED, 0);
					btsrv_rdm_a2dp_set_clear_priority(active_conn, (A2DP_PRIOROTY_STOP_WAIT | A2DP_PRIOROTY_CALL_WAIT), 0);
					sync_active = 1;
					sync_second = 1;
				}
			}
#else
			{
				if (btsrv_stop_another_when_one_playing() || !btsrv_a2dp_stream_open_used_start(active_conn)) {
					btsrv_rdm_a2dp_set_clear_priority(second_conn, A2DP_PRIOROTY_FIRST_USED, 1);
					btsrv_rdm_a2dp_set_clear_priority(active_conn, A2DP_PRIOROTY_FIRST_USED, 0);
					sync_active = 1;
					sync_second = 1;
				}
			}
#endif
		}
	} else {
		if (event == BTSRV_A2DP_STREAM_STARTED) {
#ifdef CONFIG_BT_PHONE_FILTER_PROMPT_TONE
			if (thread_timer_is_running(&a2dp_context.check_prompt_tone_timer)) {
				thread_timer_stop(&a2dp_context.check_prompt_tone_timer);
			}else {
				thread_timer_init(&a2dp_context.check_prompt_tone_timer, btsrv_a2dp_check_prompt_tone_timer_handler, active_conn);
			}
			btsrv_a2dp_check_prompt_tone_playstatus(active_conn, 1);
			thread_timer_start(&a2dp_context.check_prompt_tone_timer, AVRCP_CHECK_PROMPT_TONE_INTERVAL, AVRCP_CHECK_PROMPT_TONE_INTERVAL);
			SYS_LOG_INF("check_prompt_tone_timer phone start 3: %x", hostif_bt_conn_get_handle(active_conn));
#else
			btsrv_rdm_a2dp_set_clear_priority(active_conn, A2DP_PRIOROTY_FIRST_USED, 1);
			btsrv_rdm_a2dp_set_clear_priority(active_conn, (A2DP_PRIOROTY_STOP_WAIT | A2DP_PRIOROTY_CALL_WAIT), 0);
			sync_active = 1;
#endif
		} else if (event == BTSRV_A2DP_STREAM_SUSPEND) {
#ifdef CONFIG_BT_PHONE_FILTER_PROMPT_TONE
			if (thread_timer_is_running(&a2dp_context.check_prompt_tone_timer)) {
				thread_timer_stop(&a2dp_context.check_prompt_tone_timer);
			}
#endif
			btsrv_rdm_a2dp_set_clear_priority(active_conn, A2DP_PRIOROTY_STOP_WAIT, 1);
			btsrv_rdm_a2dp_set_clear_priority(active_conn, A2DP_PRIOROTY_FIRST_USED, 0);
			sync_active = 1;
		}else if(event == BTSRV_A2DP_NON_PROMPT_TONE_STARTED) {
#ifdef CONFIG_BT_PHONE_FILTER_PROMPT_TONE
			SYS_LOG_INF("BTSRV_A2DP_NON_PROMPT_TONE_STARTED_3: %x", hostif_bt_conn_get_handle(active_conn));
			btsrv_rdm_a2dp_set_clear_priority(active_conn, A2DP_PRIOROTY_FIRST_USED, 1);
			btsrv_rdm_a2dp_set_clear_priority(active_conn, (A2DP_PRIOROTY_STOP_WAIT | A2DP_PRIOROTY_CALL_WAIT), 0);
			sync_active = 1;
#endif
		}
	}

	if (sync_active) {
		btsrv_tws_sync_info_type(active_conn, BTSRV_SYNC_TYPE_A2DP, BTSRV_A2DP_JUST_SNOOP_SYNC_INFO);
	}
	if (sync_second) {
		btsrv_tws_sync_info_type(second_conn, BTSRV_SYNC_TYPE_A2DP, BTSRV_A2DP_JUST_SNOOP_SYNC_INFO);
	}

	SYS_LOG_INF("ev_update_prio conn 0x%x event %d", hostif_bt_conn_get_handle(conn), event);
	if (active_conn) {
		SYS_LOG_INF("active 0x%x active %d prio 0x%x", hostif_bt_conn_get_handle(active_conn),
					btsrv_rdm_a2dp_get_active(active_conn), btsrv_rdm_a2dp_get_priority(active_conn));
	}
	if (second_conn) {
		SYS_LOG_INF("second 0x%x active %d prio 0x%x", hostif_bt_conn_get_handle(second_conn),
					btsrv_rdm_a2dp_get_active(second_conn), btsrv_rdm_a2dp_get_priority(second_conn));
	}
}

static void btsrv_a2dp_state_ev_update_play(struct bt_conn *conn, uint8_t event)
{
	struct bt_conn *active_conn = btsrv_rdm_a2dp_get_active_dev();
	struct bt_conn *second_conn = btsrv_rdm_a2dp_get_second_dev();
	uint8_t active_prio, second_prio;

	if(!active_conn)
		return;

	if (!second_conn || !btsrv_rdm_is_a2dp_connected(second_conn)) {
		btsrv_rdm_a2dp_set_active(conn, 1);
		if (event == BTSRV_A2DP_STREAM_STARTED || event == BTSRV_A2DP_STREAM_SUSPEND) {
			btsrv_a2dp_user_callback(conn, event, NULL, 0);
		}else if (event == BTSRV_A2DP_NON_PROMPT_TONE_STARTED) {
#ifdef CONFIG_BT_PHONE_FILTER_PROMPT_TONE
			SYS_LOG_INF("BTSRV_A2DP_NON_PROMPT_TONE_STARTED");
			btsrv_a2dp_user_callback(conn, BTSRV_A2DP_NON_PROMPT_TONE_STARTED, NULL, 0);
#endif
		}else {
			btsrv_tws_sync_info_type(conn, BTSRV_SYNC_TYPE_A2DP, BTSRV_A2DP_JUST_SNOOP_SYNC_INFO);
		}
	} else {
		active_prio = btsrv_rdm_a2dp_get_priority(active_conn);
		second_prio = btsrv_rdm_a2dp_get_priority(second_conn);

		if (active_prio >= second_prio) {
			btsrv_rdm_a2dp_set_active(active_conn, 1);
			btsrv_rdm_a2dp_set_active(second_conn, 0);
			if (conn == active_conn) {
				if (event == BTSRV_A2DP_STREAM_STARTED || event == BTSRV_A2DP_STREAM_SUSPEND) {
					btsrv_a2dp_user_callback(active_conn, event, NULL, 0);
				}else if (event == BTSRV_A2DP_NON_PROMPT_TONE_STARTED) {
#ifdef CONFIG_BT_PHONE_FILTER_PROMPT_TONE
				SYS_LOG_INF("BTSRV_A2DP_NON_PROMPT_TONE_STARTED_1");
				btsrv_a2dp_user_callback(active_conn, BTSRV_A2DP_NON_PROMPT_TONE_STARTED, NULL, 0);
#endif
				}
			} else {
				/*
				 The inactive device pause status is also recalled to the upper level
				*/
				if (event == BTSRV_A2DP_STREAM_SUSPEND) {
	                SYS_LOG_INF("conn 0x%x event %d", hostif_bt_conn_get_handle(conn), event);
					btsrv_a2dp_user_callback(conn, event, NULL, 0);		
				}
				btsrv_tws_sync_info_type(active_conn, BTSRV_SYNC_TYPE_A2DP, BTSRV_A2DP_JUST_SNOOP_SYNC_INFO);
			}
			btsrv_tws_sync_info_type(second_conn, BTSRV_SYNC_TYPE_A2DP, BTSRV_A2DP_JUST_SNOOP_SYNC_INFO);
		} else {
			/* Active device change
			 *  Run to here, active_conn must in suspend state, just need start second conn
			 */
			SYS_LOG_INF("ev_update_play active change");
			btsrv_rdm_a2dp_set_active(active_conn, 0);
			btsrv_rdm_a2dp_set_active(second_conn, 1);
			btsrv_tws_sync_info_type(active_conn, BTSRV_SYNC_TYPE_A2DP, BTSRV_A2DP_JUST_SNOOP_SYNC_INFO);

			if (event == BTSRV_A2DP_STREAM_STARTED) {
				btsrv_a2dp_user_callback(second_conn, BTSRV_A2DP_STREAM_STARTED, NULL, 0);
			}else if (event == BTSRV_A2DP_NON_PROMPT_TONE_STARTED) {
#ifdef CONFIG_BT_PHONE_FILTER_PROMPT_TONE
				SYS_LOG_INF("BTSRV_A2DP_NON_PROMPT_TONE_STARTED_2");
				btsrv_a2dp_user_callback(second_conn, BTSRV_A2DP_NON_PROMPT_TONE_STARTED, NULL, 0);
#endif
			}else {
				btsrv_tws_sync_info_type(second_conn, BTSRV_SYNC_TYPE_A2DP, BTSRV_A2DP_JUST_SNOOP_SYNC_INFO);
			}
		}
	}

	btsrv_autoconn_info_active_update();

	SYS_LOG_INF("ev_update_play conn 0x%x event %d", hostif_bt_conn_get_handle(conn), event);
	if (active_conn) {
		SYS_LOG_INF("active 0x%x active %d prio 0x%x", hostif_bt_conn_get_handle(active_conn),
					btsrv_rdm_a2dp_get_active(active_conn), btsrv_rdm_a2dp_get_priority(active_conn));
	}
	if (second_conn) {
		SYS_LOG_INF("second 0x%x active %d prio 0x%x", hostif_bt_conn_get_handle(second_conn),
					btsrv_rdm_a2dp_get_active(second_conn), btsrv_rdm_a2dp_get_priority(second_conn));
	}
}

static void btsrv_a2dp_timer_check_update_play(void)
{
	struct bt_conn *active_conn = btsrv_rdm_a2dp_get_active_dev();
	struct bt_conn *second_conn = btsrv_rdm_a2dp_get_second_dev();
	uint8_t active_prio, second_prio;

	active_prio = btsrv_rdm_a2dp_get_priority(active_conn);
	second_prio = btsrv_rdm_a2dp_get_priority(second_conn);

	if (second_prio > active_prio) {
		SYS_LOG_INF("xxxactive 0x%x active %d prio 0x%x", hostif_bt_conn_get_handle(active_conn),
					btsrv_rdm_a2dp_get_active(active_conn), btsrv_rdm_a2dp_get_priority(active_conn));
		SYS_LOG_INF("xxxsecond 0x%x active %d prio 0x%x", hostif_bt_conn_get_handle(second_conn),
					btsrv_rdm_a2dp_get_active(second_conn), btsrv_rdm_a2dp_get_priority(second_conn));

		btsrv_rdm_a2dp_set_active(active_conn, 0);
		btsrv_rdm_a2dp_set_clear_priority(active_conn, A2DP_PRIOROTY_FIRST_USED, 0);
		if (btsrv_rdm_is_a2dp_stream_open(active_conn)) {
			if (btsrv_a2dp_stream_open_used_start(active_conn)) {
				btsrv_a2dp_user_callback(active_conn, BTSRV_A2DP_STREAM_SUSPEND, NULL, 0);
			} else {
				btsrv_rdm_a2dp_set_clear_priority(active_conn, A2DP_PRIOROTY_USER_START, 1);
				btsrv_tws_sync_info_type(active_conn, BTSRV_SYNC_TYPE_A2DP, BTSRV_A2DP_JUST_SNOOP_SYNC_INFO);
			}
		} else {
			btsrv_tws_sync_info_type(active_conn, BTSRV_SYNC_TYPE_A2DP, BTSRV_A2DP_JUST_SNOOP_SYNC_INFO);
		}

		btsrv_rdm_a2dp_set_active(second_conn, 1);
		btsrv_rdm_a2dp_set_clear_priority(second_conn, A2DP_PRIOROTY_FIRST_USED, 1);
		if (btsrv_rdm_is_a2dp_stream_open(second_conn)) {
			btsrv_rdm_a2dp_set_clear_priority(second_conn, A2DP_PRIOROTY_USER_START, 1);
#ifdef CONFIG_BT_PHONE_FILTER_PROMPT_TONE
				SYS_LOG_INF("BTSRV_A2DP_NON_PROMPT_TONE_STARTED");
				btsrv_a2dp_user_callback(second_conn, BTSRV_A2DP_NON_PROMPT_TONE_STARTED, NULL, 0);
#else
				btsrv_a2dp_user_callback(second_conn, BTSRV_A2DP_STREAM_STARTED, NULL, 0);
#endif
		} else {
			btsrv_tws_sync_info_type(second_conn, BTSRV_SYNC_TYPE_A2DP, BTSRV_A2DP_JUST_SNOOP_SYNC_INFO);
		}

		btsrv_autoconn_info_active_update();

		SYS_LOG_INF("active 0x%x active %d prio 0x%x", hostif_bt_conn_get_handle(active_conn),
					btsrv_rdm_a2dp_get_active(active_conn), btsrv_rdm_a2dp_get_priority(active_conn));
		SYS_LOG_INF("second 0x%x active %d prio 0x%x", hostif_bt_conn_get_handle(second_conn),
					btsrv_rdm_a2dp_get_active(second_conn), btsrv_rdm_a2dp_get_priority(second_conn));
	}
}

static void btsrv_a2dp_check_prio_timer_handler(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	uint8_t need_timer = 0, need_check_play = 0;
	struct bt_conn *active_conn = btsrv_rdm_a2dp_get_active_dev();
	struct bt_conn *second_conn = btsrv_rdm_a2dp_get_second_dev();
	uint32_t curr_time, stop_time;

	if (btsrv_rdm_get_dev_role() == BTSRV_TWS_SLAVE) {
		goto check_exit;
	}

	curr_time = os_uptime_get_32();

	if (active_conn) {
		if (btsrv_rdm_a2dp_get_priority(active_conn) & A2DP_PRIOROTY_STOP_WAIT) {
			need_timer = 1;
			stop_time = btsrv_rdm_a2dp_get_start_stop_time(active_conn, 0);

			if ((curr_time - stop_time) <= A2DP_BREAK_BY_CALL_INTERVAL) {
				if (btsrv_rdm_hfp_in_call_state(active_conn)) {
					btsrv_rdm_a2dp_set_clear_priority(active_conn, A2DP_PRIOROTY_STOP_WAIT, 0);
					btsrv_rdm_a2dp_set_clear_priority(active_conn, A2DP_PRIOROTY_CALL_WAIT, 1);
					btsrv_tws_sync_info_type(active_conn, BTSRV_SYNC_TYPE_A2DP, BTSRV_A2DP_JUST_SNOOP_SYNC_INFO);
				}
			} else if ((curr_time - stop_time) > btsrv_a2dp_status_stopped_delay_ms()) {
				btsrv_rdm_a2dp_set_clear_priority(active_conn, A2DP_PRIOROTY_STOP_WAIT, 0);
				btsrv_tws_sync_info_type(active_conn, BTSRV_SYNC_TYPE_A2DP, BTSRV_A2DP_JUST_SNOOP_SYNC_INFO);
				need_check_play = 1;
			}
		} else if (btsrv_rdm_a2dp_get_priority(active_conn) & A2DP_PRIOROTY_CALL_WAIT) {
			need_timer = 1;
			if (!btsrv_rdm_hfp_in_call_state(active_conn)) {
				btsrv_rdm_a2dp_set_start_stop_time(active_conn, os_uptime_get_32(), 0);
				btsrv_rdm_a2dp_set_clear_priority(active_conn, A2DP_PRIOROTY_STOP_WAIT, 1);
				btsrv_rdm_a2dp_set_clear_priority(active_conn, A2DP_PRIOROTY_CALL_WAIT, 0);
				btsrv_tws_sync_info_type(active_conn, BTSRV_SYNC_TYPE_A2DP, BTSRV_A2DP_JUST_SNOOP_SYNC_INFO);
			}
		}
	}

	if (second_conn) {
		if (btsrv_rdm_a2dp_get_priority(second_conn) & (A2DP_PRIOROTY_STOP_WAIT | A2DP_PRIOROTY_CALL_WAIT)) {
			SYS_LOG_INF("Need check why 0x%x 0x%x %d %d 0x%x 0x%x",
						hostif_bt_conn_get_handle(active_conn), hostif_bt_conn_get_handle(second_conn),
						btsrv_rdm_a2dp_get_active(active_conn), btsrv_rdm_a2dp_get_active(second_conn),
						btsrv_rdm_a2dp_get_priority(active_conn), btsrv_rdm_a2dp_get_priority(second_conn));
			need_timer = 1;
			stop_time = btsrv_rdm_a2dp_get_start_stop_time(second_conn, 0);
			if ((curr_time - stop_time) > btsrv_a2dp_status_stopped_delay_ms()) {
				btsrv_rdm_a2dp_set_clear_priority(second_conn, A2DP_PRIOROTY_STOP_WAIT, 0);
				btsrv_tws_sync_info_type(second_conn, BTSRV_SYNC_TYPE_A2DP, BTSRV_A2DP_JUST_SNOOP_SYNC_INFO);
				need_check_play = 1;
			}
		}
	}

	if (second_conn && need_check_play) {
		if ((btsrv_rdm_a2dp_get_priority(second_conn) & A2DP_PRIOROTY_AVRCP_PLAY)){
			btsrv_a2dp_timer_check_update_play();
			btsrv_event_notify(MSG_BTSRV_TWS, MSG_BTSRV_TWS_PHONE_ACTIVE_CHANGE, NULL);
		}else{
			SYS_LOG_INF("second 0x%x active %d prio 0x%x avrcp unplay ", hostif_bt_conn_get_handle(second_conn),
				btsrv_rdm_a2dp_get_active(second_conn), btsrv_rdm_a2dp_get_priority(second_conn));
		}
	}else if (need_check_play){
		btsrv_a2dp_user_callback(active_conn, BTSRV_A2DP_STREAM_STARTED_AVRCP_PAUSED, NULL, 0);
	}

check_exit:
	if (!need_timer) {
		thread_timer_stop(&a2dp_context.check_state_timer);
	}
}

static void btsrv_a2dp_disconnected_update_play(struct bt_conn *conn)
{
	struct bt_conn *active_conn = btsrv_rdm_a2dp_get_active_dev();
	struct bt_conn *second_conn = btsrv_rdm_a2dp_get_second_dev();

	if (!active_conn || conn != active_conn) {
		return;
	}

	/* conn is active_conn */
	//if (btsrv_a2dp_stream_open_used_start(active_conn)) {
	if(btsrv_rdm_is_a2dp_stream_open(active_conn)){
		btsrv_rdm_a2dp_set_clear_priority(active_conn, A2DP_PRIOROTY_ALL, 0);
		btsrv_a2dp_user_callback(active_conn, BTSRV_A2DP_DISCONNECTED_STREAM_SUSPEND, NULL, 0);
	}
	btsrv_rdm_a2dp_set_active(active_conn, 0);

	if (second_conn && btsrv_a2dp_stream_open_used_start(second_conn)) {
		btsrv_rdm_a2dp_set_active(second_conn, 1);
		btsrv_a2dp_user_callback(second_conn, BTSRV_A2DP_REQ_DELAY_CHECK_START, NULL, 0);
	}

	btsrv_autoconn_info_active_update();
}

int btsrv_a2dp_init(struct btsrv_a2dp_start_param *param)
{
	int ret = 0;
	int i = 0, max_register;

	max_register = MIN(CONFIG_MAX_A2DP_ENDPOINT, param->sbc_endpoint_num);
	for (i = 0; i < max_register; i++) {
		a2dp_context.a2dp_sbc_endpoint[i].info.codec = (struct bt_a2dp_media_codec *)param->sbc_codec;
		a2dp_context.a2dp_sbc_endpoint[i].info.a2dp_cp_scms_t = param->a2dp_cp_scms_t;
		a2dp_context.a2dp_sbc_endpoint[i].info.a2dp_delay_report = param->a2dp_delay_report;
		ret |= hostif_bt_a2dp_register_endpoint(&a2dp_context.a2dp_sbc_endpoint[i], BT_A2DP_AUDIO, BT_A2DP_EP_SINK);
	}

	max_register = MIN(CONFIG_MAX_A2DP_ENDPOINT, param->aac_endpoint_num);
	a2dp_context.a2dp_register_aac_num = max_register;
	for (i = 0; i < max_register; i++) {
		a2dp_context.a2dp_aac_endpoint[i].info.codec = (struct bt_a2dp_media_codec *)param->aac_codec;
		a2dp_context.a2dp_aac_endpoint[i].info.a2dp_cp_scms_t = param->a2dp_cp_scms_t;
		a2dp_context.a2dp_aac_endpoint[i].info.a2dp_delay_report = param->a2dp_delay_report;
		ret |= hostif_bt_a2dp_register_endpoint(&a2dp_context.a2dp_aac_endpoint[i], BT_A2DP_AUDIO, BT_A2DP_EP_SINK);
	}

	max_register = MIN(CONFIG_MAX_A2DP_ENDPOINT, param->ldac_endpoint_num);
	a2dp_context.a2dp_register_ldac_num = max_register;
	for (i = 0; i < max_register; i++) {
		a2dp_context.a2dp_ldac_endpoint[i].info.codec = (struct bt_a2dp_media_codec *)param->ldac_codec;
		a2dp_context.a2dp_ldac_endpoint[i].info.a2dp_cp_scms_t = 0;//to do 
		a2dp_context.a2dp_ldac_endpoint[i].info.a2dp_delay_report = param->a2dp_delay_report;
		ret |= hostif_bt_a2dp_register_endpoint(&a2dp_context.a2dp_ldac_endpoint[i], BT_A2DP_AUDIO, BT_A2DP_EP_SINK);
	}
	
	if (ret) {
		SYS_LOG_ERR("bt br-a2dp-register failed\n");
		goto err;
	}

	a2dp_context.a2dp_user_callback = param->cb;
	hostif_bt_a2dp_register_cb((struct bt_a2dp_app_cb *)&btsrv_a2dp_cb);

	thread_timer_init(&a2dp_context.check_state_timer, btsrv_a2dp_check_prio_timer_handler, NULL);

	return 0;
err:
	return -1;
}

int btsrv_a2dp_deinit(void)
{
	btsrv_a2dp_start_stop_check_prio_timer(0);

	hostif_bt_a2dp_register_cb(NULL);
	a2dp_context.a2dp_user_callback = NULL;
	return 0;
}

int btsrv_a2dp_connect(struct bt_conn *conn, uint8_t role)
{
	if (!hostif_bt_a2dp_connect(conn, role)) {
		SYS_LOG_INF("Connect a2dp");
	} else {
		SYS_LOG_ERR("Connect a2dp failed");
	}

	return 0;
}

int btsrv_a2dp_disconnect(struct bt_conn *conn)
{
	if (!conn) {
		SYS_LOG_ERR("conn is NULL\n");
		return -EINVAL;
	}

	hostif_bt_a2dp_disconnect(conn);
	SYS_LOG_INF("a2dp_disconnect\n");
	return 0;
}

static int btsrv_a2dp_media_state_req_proc(struct bt_conn *conn, uint8_t state)
{
	int ev_cb = -1;
	uint16_t delay_report;

	SYS_LOG_INF("Hdl 0x%x a2dp state cb %d", hostif_bt_conn_get_handle(conn), state);

	switch (state) {
	case BT_A2DP_MEDIA_STATE_OPEN:
		delay_report = 0;
		btsrv_a2dp_user_callback(conn, BTSRV_A2DP_GET_INIT_DELAY_REPORT, &delay_report, sizeof(delay_report));
		if (delay_report) {
			hostif_bt_a2dp_send_delay_report(conn, delay_report);
		}
		break;
	case BT_A2DP_MEDIA_STATE_START:
        if(btsrv_rdm_is_avrcp_connected(conn)){
            if(!btsrv_rdm_get_avrcp_state(conn) && !btsrv_rdm_is_avrcp_playing_pended(conn)){
                btsrv_rdm_set_a2dp_pending_ahead_start(conn, 1);
                btsrv_rdm_set_avrcp_playing_pending(conn);
            }
            else{
                if(!btsrv_rdm_is_a2dp_stream_open(conn)){
                    ev_cb = BTSRV_A2DP_STREAM_STARTED;
                }
                else if (btsrv_rdm_get_a2dp_pending_ahead_start(conn)){
                    ev_cb = BTSRV_A2DP_STREAM_STARTED;
                }
                btsrv_rdm_set_a2dp_pending_ahead_start(conn, 0);
            }
        }
        else{
            if(btsrv_rdm_is_avrcp_connected_pended(conn)){
                if(!btsrv_rdm_is_a2dp_stream_open(conn)){
                    ev_cb = BTSRV_A2DP_STREAM_STARTED;
                }
                else if (btsrv_rdm_get_a2dp_pending_ahead_start(conn)){
                    ev_cb = BTSRV_A2DP_STREAM_STARTED;
                }
                btsrv_rdm_set_a2dp_pending_ahead_start(conn, 0);
            }
            else{
                btsrv_rdm_set_a2dp_pending_ahead_start(conn, 1);
                btsrv_rdm_set_avrcp_connecting_pending(conn);
            }
        }
		break;
	case BT_A2DP_MEDIA_STATE_CLOSE:
	case BT_A2DP_MEDIA_STATE_SUSPEND:
		if (btsrv_rdm_is_a2dp_stream_open(conn)) {
			ev_cb = BTSRV_A2DP_STREAM_SUSPEND;
		}
		btsrv_rdm_set_a2dp_pending_ahead_start(conn, 0);
		break;
	case BT_A2DP_MEDIA_STATE_PENDING_AHEAD_START:
		btsrv_rdm_set_a2dp_pending_ahead_start(conn, 1);
		break;
	}

	if (ev_cb > 0) {
		if (btsrv_rdm_get_dev_role() == BTSRV_TWS_SLAVE) {
			/* Tws mater send to slave state req */
			btsrv_a2dp_user_callback(conn, ev_cb, NULL, 0);
		} else {
			btsrv_a2dp_state_ev_update_prio(conn, ev_cb);
			btsrv_a2dp_state_ev_update_play(conn, ev_cb);
			btsrv_a2dp_start_stop_check_prio_timer(1);
			btsrv_event_notify(MSG_BTSRV_TWS, MSG_BTSRV_TWS_PHONE_ACTIVE_CHANGE, NULL);
		}
	}

	return 0;
}

static void btsrv_a2dp_seted_codec_proc(void *in_parm)
{
	struct btsrv_a2dp_codec_cb *param = in_parm;

	SYS_LOG_INF("hdl 0x%x codec %d freq %d cp %d", hostif_bt_conn_get_handle(param->conn),
				param->format, param->sample_rate, param->cp_type);
	btsrv_rdm_a2dp_set_codec_info(param->conn, param->format, param->sample_rate, param->cp_type);
}

static void btsrv_a2dp_check_state(void)
{
	struct bt_conn *active_conn = btsrv_rdm_a2dp_get_active_dev();

	if (active_conn){
		SYS_LOG_INF("%d",btsrv_rdm_is_a2dp_stream_open(active_conn));
	}

	if (active_conn && btsrv_rdm_is_a2dp_stream_open(active_conn)) {
		btsrv_rdm_a2dp_set_clear_priority(active_conn, A2DP_PRIOROTY_USER_START, 1);
		btsrv_a2dp_user_callback(active_conn, BTSRV_A2DP_STREAM_CHECK_STARTED, NULL, 0);
	}
}

static void btsrv_a2dp_user_pause(uint16_t hdl)
{
	struct bt_conn *second_conn;
	struct bt_conn *active_conn = btsrv_rdm_a2dp_get_active_dev();

	if(!active_conn)
		return;
	
	uint16_t active_hdl = hostif_bt_conn_get_handle(active_conn);

	if (hdl != active_hdl)
	{
		SYS_LOG_INF("active_conn 0x%x != 0x%x", active_hdl,hdl);
		return;
	}

	if (active_conn && btsrv_a2dp_stream_open_used_start(active_conn)) {
		second_conn = btsrv_rdm_a2dp_get_second_dev();
		/* If user call pause, simulate a suspend event to upper when the second phone in playing,
		 * for sigle phone, if phone not stop a2dp, upper need to call
		 * btsrv_a2dp_check_state tigger start again after upper pause timeout.
		 * for double phone , will start second phone when it is in playing state.
		 */
		SYS_LOG_INF("A2dp user pause 0x%x", hostif_bt_conn_get_handle(active_conn));

		if (second_conn && btsrv_rdm_get_avrcp_state(second_conn)
			&& btsrv_a2dp_stream_open_used_start(second_conn))
		{
			SYS_LOG_INF("second_conn 0x%x avrcp status:play\n",hostif_bt_conn_get_handle(second_conn));	
			btsrv_rdm_a2dp_set_clear_priority(active_conn, A2DP_PRIOROTY_USER_START, 0);
		}
		
		btsrv_rdm_a2dp_set_clear_priority(active_conn, A2DP_PRIOROTY_STOP_WAIT, 1);
		btsrv_rdm_a2dp_set_start_stop_time(active_conn, os_uptime_get_32(), 0);
		if (second_conn && btsrv_a2dp_stream_open_used_start(second_conn)) {
			btsrv_a2dp_user_callback(active_conn, BTSRV_A2DP_STREAM_SUSPEND, NULL, 0);
		}
		btsrv_a2dp_start_stop_check_prio_timer(1);
	}
}


static void connected_dev_cb_send_delay_report(struct bt_conn *base_conn, uint8_t tws_dev, void *cb_param)
{
	uint32_t delay_time = (uint32_t)cb_param;

	hostif_bt_a2dp_send_delay_report(base_conn, (uint16_t)delay_time);
}

static void btsrv_a2dp_send_delay_report(uint16_t delay_time)
{
	struct bt_conn *conn = btsrv_rdm_a2dp_get_active_dev();
	uint32_t cb_param = delay_time;

	if (btsrv_is_pts_test() && conn == NULL) {
		btsrv_rdm_get_connected_dev(connected_dev_cb_send_delay_report, (void *)cb_param);
		return;
	}

	if (conn) {
		hostif_bt_a2dp_send_delay_report(conn, delay_time);
	}
}

static void btsrv_a2dp_update_avrcp_playstate(struct bt_conn *conn, uint8_t avrcp_playing)
{
	struct bt_conn *active_conn = btsrv_rdm_a2dp_get_active_dev();
	uint16_t active_hdl = hostif_bt_conn_get_handle(active_conn);
	uint16_t cur_hdl = hostif_bt_conn_get_handle(conn);

	if (avrcp_playing)
	{
		if ((btsrv_rdm_a2dp_get_priority(conn) & A2DP_PRIOROTY_STREAM_OPEN) &&
			!(btsrv_rdm_a2dp_get_priority(conn) & A2DP_PRIOROTY_AVRCP_PLAY))
		{
			btsrv_rdm_a2dp_set_clear_priority(conn, A2DP_PRIOROTY_AVRCP_PLAY, 1);
			btsrv_rdm_a2dp_set_clear_priority(conn, A2DP_PRIOROTY_STOP_WAIT, 0);
            /*
			 * The streaming media has been open and has not been changed
			 * The avrcp status changes from paused to played
			 * Start to check if there is any data coming
			 * If there is data transmission, it is considered as replaying
			*/
			if ((active_hdl != cur_hdl) && (btsrv_stop_another_when_one_playing())) {
               SYS_LOG_INF("second_conn pending ahead start:\n");		
			   btsrv_a2dp_media_state_change(conn,BT_A2DP_MEDIA_STATE_PENDING_AHEAD_START);	
			}
		}
	}
	else
	{
		if (btsrv_rdm_a2dp_get_priority(conn) & A2DP_PRIOROTY_AVRCP_PLAY)
		{			
			btsrv_rdm_a2dp_set_clear_priority(conn, A2DP_PRIOROTY_AVRCP_PLAY, 0);
			if (active_hdl == cur_hdl){
				if (!(btsrv_rdm_a2dp_get_priority(conn) & A2DP_PRIOROTY_STOP_WAIT)){
					btsrv_rdm_a2dp_set_clear_priority(conn, A2DP_PRIOROTY_STOP_WAIT, 1);
					btsrv_rdm_a2dp_set_start_stop_time(active_conn, os_uptime_get_32(), 0);
				}
			}
		}
	}
	
	SYS_LOG_INF("avrcp:%d hdl:0x%x ac:%d pr:0x%x", avrcp_playing, hostif_bt_conn_get_handle(conn),
					btsrv_rdm_a2dp_get_active(conn), btsrv_rdm_a2dp_get_priority(conn));
	
	btsrv_a2dp_start_stop_check_prio_timer(1);
}


int btsrv_a2dp_process(struct app_msg *msg)
{
	struct bt_conn *conn;

	switch (_btsrv_get_msg_param_cmd(msg)) {
	case MSG_BTSRV_A2DP_START:
		SYS_LOG_INF("MSG_BTSRV_A2DP_START\n");
		btsrv_a2dp_init(msg->ptr);
		break;
	case MSG_BTSRV_A2DP_STOP:
		SYS_LOG_INF("MSG_BTSRV_A2DP_STOP\n");
		btsrv_a2dp_deinit();
		break;
	case MSG_BTSRV_A2DP_CONNECT_TO:
		SYS_LOG_INF("MSG_BTSRV_A2DP_CONNECT_TO\n");
		conn = btsrv_rdm_find_conn_by_addr(msg->ptr);
		if (conn) {
			btsrv_a2dp_connect(conn, (_btsrv_get_msg_param_reserve(msg) ? BT_A2DP_CH_SOURCE : BT_A2DP_CH_SINK));
		}
		break;
	case MSG_BTSRV_A2DP_DISCONNECT:
		SYS_LOG_INF("MSG_BTSRV_A2DP_DISCONNECT\n");
		conn = btsrv_rdm_find_conn_by_addr(msg->ptr);
		if (conn) {
			btsrv_a2dp_disconnect(conn);
		}
		break;
	case MSG_BTSRV_A2DP_CONNECTED:
		btsrv_a2dp_user_callback(msg->ptr, BTSRV_A2DP_CONNECTED, NULL, 0);
		btsrv_adapter_callback(BTSRV_RELEASE_HIGH_PERFORMANCE, NULL);		
		break;
	case MSG_BTSRV_A2DP_DISCONNECTED:

		btsrv_a2dp_disconnected_update_play(msg->ptr);
		btsrv_event_notify(MSG_BTSRV_TWS, MSG_BTSRV_TWS_PHONE_ACTIVE_CHANGE, NULL);
		btsrv_event_notify(MSG_BTSRV_CONNECT, _btsrv_get_msg_param_cmd(msg), msg->ptr);
		btsrv_a2dp_user_callback(msg->ptr, BTSRV_A2DP_DISCONNECTED, NULL, 0);
		break;
	case MSG_BTSRV_A2DP_MEDIA_STATE_CB:
		btsrv_a2dp_media_state_req_proc(msg->ptr, _btsrv_get_msg_param_reserve(msg));
		break;
	case MSG_BTSRV_A2DP_SET_CODEC_CB:
		btsrv_a2dp_seted_codec_proc(msg->ptr);
		break;
	case MSG_BTSRV_A2DP_CHECK_STATE:
		btsrv_a2dp_check_state();
		break;
	case MSG_BTSRV_A2DP_USER_PAUSE:
		btsrv_a2dp_user_pause((uint16_t)_btsrv_get_msg_param_value(msg));
		break;
	case MSG_BTSRV_A2DP_SEND_DELAY_REPORT:
		btsrv_a2dp_send_delay_report((uint16_t)_btsrv_get_msg_param_value(msg));
		break;	
	case MSG_BTSRV_A2DP_UPDATE_AVRCP_PLAYSTATE:
		btsrv_a2dp_update_avrcp_playstate(msg->ptr, (uint8_t)_btsrv_get_msg_param_reserve(msg));
		break;
	case MSG_BTSRV_A2DP_HALT_AAC:
		btsrv_a2dp_halt_aac_endpoint(_btsrv_get_msg_param_value(msg));
		break;
	case MSG_BTSRV_A2DP_HALT_LDAC:
		btsrv_a2dp_halt_ldac_endpoint(_btsrv_get_msg_param_value(msg));
		break;
	default:
		break;
	}
	return 0;
}
