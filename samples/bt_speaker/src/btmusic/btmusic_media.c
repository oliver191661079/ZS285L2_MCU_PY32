/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt music media.
 */
#include "btmusic.h"
#include "btmusic_ws2812.h"
#include "media_mem.h"
#include <energy_statistics.h>
#include <ringbuff_stream.h>
#include <acts_ringbuf.h>
#include "tts_manager.h"
#include "ui_manager.h"
#ifdef CONFIG_BT_SELF_APP
#include "selfapp_api.h"
#endif
#if defined(CONFIG_SYSTEM_APP_PY32_UART)
#include "../system_app/system_app.h"
#endif

#define SUPPORT_RETRANSMIT_NUM_ADJUST
#define MEDIA_PACKET_TIME_THRESHOLD       20
#define MEDIA_LEVEL_HOLD_TIME              20

void btmusic_event_notify(u32_t event, void *data, u32_t len, void *user_data)
{
	if (event == PLAYBACK_EVENT_DATA_INDICATE) {
		btmusic_player_reset_trigger();
	}
}

static io_stream_t _btmusic_a2dp_create_inputstream(int stream_type)
{
	int ret = 0;
	io_stream_t input_stream =
	    ringbuff_stream_create_ext(media_mem_get_cache_pool
				       (INPUT_PLAYBACK, stream_type),
				       media_mem_get_cache_pool_size
				       (INPUT_PLAYBACK, stream_type));

	if (!input_stream) {
		goto exit;
	}

	ret =
	    stream_open(input_stream,
			MODE_IN_OUT | MODE_READ_BLOCK | MODE_BLOCK_TIMEOUT);
	if (ret) {
		stream_destroy(input_stream);
		input_stream = NULL;
		goto exit;
	}

 exit:
	SYS_LOG_INF(" %p type %d\n", input_stream,stream_type);
	return input_stream;
}

static void set_player_effect_output_mode(media_player_t *player)
{
	int mode = CONFIG_MEDIA_EFFECT_OUTMODE;

	if ((APP_TWS_MODE_SNOOP == app_tws_status_get_mode())){
		if( bt_manager_tws_get_dev_role() == BTSRV_TWS_SLAVE) {
			mode = MEDIA_EFFECT_OUTPUT_R_ONLY;
		} else {
			mode = MEDIA_EFFECT_OUTPUT_L_ONLY;
		}
	} else if (app_tws_status_get_connected() && app_tws_status_get_enable()){
		/* 真立体声 TWS（BIS 模式）：主单元播左声道、从单元播右声道。
		 * 未连接 TWS 时（connected=false）保持 DEFAULT = 双声道。 */
		if (app_tws_status_get_role() == APP_TWS_ROLE_SECONDARY) {
			mode = MEDIA_EFFECT_OUTPUT_R_ONLY;
		} else {
			mode = MEDIA_EFFECT_OUTPUT_L_ONLY;
		}
	}

	if(btmusic_get_auracast_mode()) {
#ifdef CONFIG_BT_SELF_APP
		u8_t ch;
		ch = selfapp_get_channel();
		if(ch == 0) {
			mode = MEDIA_EFFECT_OUTPUT_DEFAULT;
		} else if (ch == 1) {
			mode = MEDIA_EFFECT_OUTPUT_L_ONLY;
		}else {
			mode = MEDIA_EFFECT_OUTPUT_R_ONLY;
		}
#endif
	}

	SYS_LOG_INF("%d\n", mode);
	if(mode != CONFIG_MEDIA_EFFECT_OUTMODE) {
		media_player_set_effect_output_mode(player, mode);
	}
}

int btmusic_init_playback()
{
	media_init_param_t init_param;
	io_stream_t input_stream = NULL;
	struct btmusic_app_t *btmusic = btmusic_get_app();
	struct bt_audio_chan *chan = &btmusic->sink_chan;
	struct bt_audio_chan_info chan_info;
	int ret;
	int stream_type = btmusic_get_auracast_mode() ? AUDIO_STREAM_SOUNDBAR : AUDIO_STREAM_MUSIC;

	if (1 == btmusic_get_auracast_mode() && 0 == btmusic->bms_source) {
		stream_type = AUDIO_STREAM_MUSIC;
	}

	if (!btmusic)
		return -EINVAL;

	if (btmusic->playback_player) {
		SYS_LOG_INF("already\n");
		return -EALREADY;
	}

	ret = bt_manager_audio_stream_info(chan, &chan_info);
	if (ret) {
		SYS_LOG_ERR("[dbg] stream_info fail ret=%d handle=0x%x id=%d",
			    ret, chan->handle, chan->id);
		return ret;
	}

	SYS_LOG_INF("[dbg] stream_info ok handle=0x%x id=%d fmt=%d sr=%d ch=%d dur=%d",
		    chan->handle, chan->id, chan_info.format, chan_info.sample,
		    chan_info.channels, chan_info.duration);

#ifdef CONFIG_PLAYTTS
	tts_manager_wait_finished(false);
#endif
	btmusic_view_show_play_paused(true);

	input_stream = _btmusic_a2dp_create_inputstream(stream_type);
	if (!input_stream) {
		goto exit;
	}

	if (bt_manager_audio_codec_type(chan_info.format) == SBC_TYPE){
		btmusic->sbc_playing = true;
	}

	audio_system_set_output_sample_rate(chan_info.sample);

	memset(&init_param, 0, sizeof(media_init_param_t));
	init_param.type = MEDIA_SRV_TYPE_PLAYBACK;
	init_param.format = bt_manager_audio_codec_type(chan_info.format);
	init_param.stream_type = stream_type;
	init_param.efx_stream_type = AUDIO_STREAM_MUSIC;
	init_param.sample_rate = chan_info.sample;
	init_param.input_stream = input_stream;
	init_param.output_stream = NULL;
	init_param.event_notify_handle = NULL;
#ifdef CONFIG_TWS
	if (APP_TWS_MODE_SNOOP == app_tws_status_get_mode()) {
		init_param.support_tws = 1;
	} else {
		init_param.support_tws = 0;
	}
#endif
	SYS_LOG_INF("tws %d", init_param.support_tws);
	init_param.dumpable = 1;
	init_param.sample_bits = 32;
	init_param.event_notify_handle = btmusic_event_notify;
	init_param.user_data = (void *)btmusic;
#ifdef CONFIG_BTMUSIC_BMS_APP
	if(btmusic_get_auracast_mode() && 1 == btmusic->bms_source){
		init_param.waitto_start = 1;
		init_param.bind_to_capture = 1;
#if (BROADCAST_DURATION == BT_FRAME_DURATION_7_5MS)
		audio_policy_set_nav_frame_size_us(7500);
#else
		audio_policy_set_nav_frame_size_us(10000);
#endif
		audio_policy_set_bis_link_delay_ms(broadcast_get_bis_link_delay(btmusic->qos));
	}
#endif

	if (audio_policy_get_out_audio_mode(init_param.stream_type) ==
	    AUDIO_MODE_STEREO) {
		init_param.channels = 2;
	} else {
		init_param.channels = 1;
	}

	btmusic->sink_stream = input_stream;

	bt_manager_volume_set(audio_system_get_stream_volume
			      (stream_type),BT_VOLUME_TYPE_BR_MUSIC);

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	if (0 == btmusic->set_dvfs_level) {
		btmusic->set_dvfs_level = SOC_DVFS_LEVEL_FULL_PERFORMANCE;
		soc_dvfs_set_level(btmusic->set_dvfs_level, "btmusic");
	}

	if (btmusic_get_auracast_mode()) {
		if (!btmusic->sbc_playing) {
			if(btmusic->bms_dvfs) {
				soc_dvfs_unset_level(btmusic->bms_dvfs, "bms_br");
				btmusic->bms_dvfs = 0;
			}
			btmusic->bms_dvfs = BCST_FREQ_HIGH;
			soc_dvfs_set_level(btmusic->bms_dvfs, "bms_br");
		}
	}
#endif

#if 0 /* 水线会导致噗噗声，蓝牙栈自有缓冲吸收 irq_lock ~5ms 间隙 */
	/*
	 * 律动灯刷新时 irq_lock ~5ms，音频 DMA 水线提升到 20ms
	 * 以保证蓝牙数据 + I2S 输出不被欠载打断。
	 */
	if (btmusic->ios_dev) {
		audio_policy_set_user_dynamic_waterlevel_ms(100);
	} else {
		audio_policy_set_user_dynamic_waterlevel_ms(20);
	}
#endif
	btmusic->playback_player = media_player_open(&init_param);
	if (!btmusic->playback_player) {
		goto err_exit;
	}
	set_player_effect_output_mode(btmusic->playback_player);

	btmusic->media_opened = 1;

	if(!btmusic_get_auracast_mode())
		bt_manager_audio_sink_stream_set(chan, input_stream);

	if (1 == btmusic_get_auracast_mode() && 0 == btmusic->bms_source)
		bt_manager_audio_sink_stream_set(chan, input_stream);

	SYS_LOG_INF("[dbg] player=%p stream=%p chan=0x%x stream_type=%d sr=%d ch=%d fmt=%d opened=%d",
		    btmusic->playback_player, input_stream, chan->handle,
		    stream_type, init_param.sample_rate, init_param.channels,
		    init_param.format, btmusic->media_opened);

	return 0;

 err_exit:
	if (input_stream) {
		stream_close(input_stream);
		stream_destroy(input_stream);
		btmusic->sink_stream = NULL;
	}
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	if (btmusic->set_dvfs_level) {
		soc_dvfs_unset_level(btmusic->set_dvfs_level, "btmusic");
		btmusic->set_dvfs_level = 0;
	}
#endif

exit:
	SYS_LOG_ERR("open failed\n");
	return -EINVAL;

}

#if 0 /* DSP 能量采集已禁用 */
#ifdef CONFIG_BT_MUSIC_FREQPOINT_ENERGY_DEMO
static void btmusic_rgb_energy_path_reset(void);
#endif
#endif

int btmusic_start_playback(void)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();

	SYS_LOG_INF("%p", btmusic->playback_player);

	if (!btmusic->playback_player) {
		SYS_LOG_INF("not ready\n");
		return -EINVAL;
	}

	if (btmusic->playback_player_run) {
		SYS_LOG_INF("already\n");
		return -EINVAL;
	}

#ifdef CONFIG_BTMUSIC_BMS_APP
	if(btmusic_get_auracast_mode() && 1 == btmusic->bms_source){
		bt_manager_broadcast_stream_tws_sync_cb_register_1(btmusic->chan,
			media_player_audio_track_trigger_callback,audio_system_get_track());
	}

	btmusic->pcm_time = 0;
	btmusic->bis_delay = broadcast_get_bis_link_delay(btmusic->qos);
	SYS_LOG_INF("bis_delay %d", btmusic->bis_delay);
#endif

#if 0 /* 假律动模式下不需要 DSP 能量，且 f_band_energy_en 会破坏 A2DP 音频 */
#ifdef CONFIG_BT_MUSIC_FREQPOINT_ENERGY_DEMO
	btmusic_output_energy_sample_config(btmusic->playback_player);
	btmusic_rgb_energy_path_reset();
#endif
#endif

	media_player_fade_in(btmusic->playback_player, 120);
	media_player_play(btmusic->playback_player);

	btmusic->playback_player_run = 1;

#if defined(CONFIG_SYSTEM_APP_PY32_UART)
	system_app_py32_reapply_eq();
#endif

#if defined(CONFIG_BT_MUSIC_LED_RHYTHM)
	btmusic_ws2812_on_playback_start();
#endif

#if 0 /* DISABLED */
#ifdef CONFIG_BT_MUSIC_FREQPOINT_ENERGY_DEMO
#if CONFIG_BT_MUSIC_RGB_PCM_DUMP
	btmusic_rgb_pcm_dump_start(btmusic->playback_player);
#endif
#endif
#endif

	return 0;
}

int btmusic_stop_playback(void)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();

	if (!btmusic || !btmusic->playback_player || !btmusic->playback_player_run) {
		SYS_LOG_INF("not ready\n");
		return -EINVAL;
	}

	MSG_SEND_TIME_STAT_START();

#ifdef CONFIG_BTMUSIC_BMS_APP
	if(btmusic_get_auracast_mode()){
		bt_manager_broadcast_stream_tws_sync_cb_register_1(btmusic->chan,
			NULL,NULL);
	}
#endif

	SYS_LOG_INF("%p\n", btmusic->playback_player);

	media_player_fade_out(btmusic->playback_player, 60);

	/** reserve time to fade out*/
	os_sleep(audio_policy_get_bis_link_delay_ms() + 80);

	btmusic_view_show_play_paused(false);

	bt_manager_audio_sink_stream_set(&btmusic->sink_chan, NULL);

	if (btmusic->sink_stream) {
		stream_close(btmusic->sink_stream);
	}

	media_player_stop(btmusic->playback_player);

	MSG_SEND_TIME_STAT_STOP();

	btmusic->media_opened = 0;
#ifdef CONFIG_BTMUSIC_BMS_APP
	btmusic->pcm_time = 0;
#endif

	return 0;
}

int btmusic_exit_playback()
{
	struct btmusic_app_t *btmusic = btmusic_get_app();

	SYS_LOG_INF("%p", btmusic->playback_player);

	if (!btmusic->playback_player) {
		SYS_LOG_INF("already\n");
		return -EALREADY;
	}

#if CONFIG_BT_MUSIC_RGB_PCM_DUMP
	/* 退出播放时关闭 PCM dump，恢复解码输出不被旁路消费 */
	btmusic_rgb_pcm_dump_stop();
#endif

	media_player_close(btmusic->playback_player);

	if (btmusic->sink_stream) {
		stream_destroy(btmusic->sink_stream);
		btmusic->sink_stream = NULL;
	}

#ifdef CONFIG_BTMUSIC_BMS_APP
	if(btmusic_get_auracast_mode())
		audio_policy_set_bis_link_delay_ms(0);
#endif

	btmusic->playback_player_run = 0;
	btmusic->playback_player = NULL;
	//clear restart at player stop.
	btmusic->restart = 0;

	/* 退出播放时复位全局音频水线，避免影响本地播放等其他音源 */
	audio_policy_set_user_dynamic_waterlevel_ms(0);

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	if (btmusic->set_dvfs_level) {
		soc_dvfs_unset_level(btmusic->set_dvfs_level, "btmusic");
		btmusic->set_dvfs_level = 0;
	}
#endif
	btmusic->sbc_playing = false;
	return 0;
}

#ifdef CONFIG_BT_MUSIC_FREQPOINT_ENERGY_DEMO

/* 最近一次 btmusic_a2dp_get_freqpoint_energy 使用的数据源（日志 src=） */
uint8_t btmusic_rgb_energy_src;
static uint8_t btmusic_rgb_energy_log_cnt;

#if CONFIG_BT_MUSIC_RGB_PCM_DUMP
#define BTMUSIC_RGB_PCM_DUMP_BYTES 1024
#define BTMUSIC_RGB_PCM_SAMPLE_BITS 32

static char btmusic_rgb_pcm_dump_store[BTMUSIC_RGB_PCM_DUMP_BYTES];
static struct acts_ringbuf *btmusic_rgb_pcm_ringbuf;
static uint8_t btmusic_rgb_pcm_dump_on;
static uint8_t btmusic_rgb_pcm_fail_log_cnt;
#endif

/*
 * 配置音乐 DAE 的频带/时域能量采样（10 段，f0~f9 见下方频点）。
 * 修改目的：让 DSP 向 ENERGY_BUFFER 环缓冲写入数据，供 get_freqpoint_energy 读取。
 */
void btmusic_output_energy_sample_config(media_player_t *player)
{
	effect_f_energy_set_t cfg;
	int ret;

	if (!player) {
		return;
	}

	memset(&cfg, 0, sizeof(cfg));
	cfg.t_energy_en = 0;	/* 时域能量会截走 PCM 导致 A2DP 通路无声 */
	cfg.f_band_energy_en = 1;
	cfg.f_band_energy_timewindow_ms = 10;
	cfg.f_band_bandwidth = 29;
	cfg.f_band_energy_num = 10;
	cfg.f_band_energy_f0 = 100;
	cfg.f_band_energy_f1 = 500;
	cfg.f_band_energy_f2 = 1000;
	cfg.f_band_energy_f3 = 1500;
	cfg.f_band_energy_f4 = 2000;
	cfg.f_band_energy_f5 = 3000;
	cfg.f_band_energy_f6 = 5000;
	cfg.f_band_energy_f7 = 8000;
	cfg.f_band_energy_f8 = 10000;
	cfg.f_band_energy_f9 = 15000;

	ret = media_player_set_output_energy_sample(player, &cfg);
	SYS_LOG_INF("output_energy_sample ret=%d\n", ret);
}

#if CONFIG_BT_MUSIC_RGB_PCM_DUMP
/*
 * 启动解码输出 PCM dump（挂接 DECODE_OUT1 到本地 ringbuf）。
 * 警告：acts_ringbuf_get 会取走数据，可能导致无声，仅调试时开启 CONFIG_BT_MUSIC_RGB_PCM_DUMP。
 */
void btmusic_rgb_pcm_dump_start(media_player_t *player)
{
	uint8_t tag = MEDIA_DATA_TAG_DECODE_OUT1;
	int ret;

	if (!player) {
		return;
	}

	if (btmusic_rgb_pcm_dump_on) {
		btmusic_rgb_pcm_dump_stop();
	}

	btmusic_rgb_pcm_ringbuf = acts_ringbuf_init_ext(btmusic_rgb_pcm_dump_store,
			sizeof(btmusic_rgb_pcm_dump_store));
	if (!btmusic_rgb_pcm_ringbuf) {
		SYS_LOG_ERR("pcm dump ringbuf init fail\n");
		return;
	}

	ret = media_player_dump_data(player, 1, &tag, &btmusic_rgb_pcm_ringbuf);
	if (ret != 0) {
		SYS_LOG_ERR("dump DECODE_OUT1 ret=%d\n", ret);
		acts_ringbuf_destroy_ext(btmusic_rgb_pcm_ringbuf);
		btmusic_rgb_pcm_ringbuf = NULL;
		return;
	}

	btmusic_rgb_pcm_dump_on = 1;
	SYS_LOG_INF("pcm fallback dump on (DECODE_OUT1)\n");
}

/* 停止 PCM dump 并释放旁路 ringbuf */
void btmusic_rgb_pcm_dump_stop(void)
{
	uint8_t tag = MEDIA_DATA_TAG_DECODE_OUT1;

	if (!btmusic_rgb_pcm_dump_on) {
		return;
	}

	media_player_dump_data(NULL, 1, &tag, NULL);
	if (btmusic_rgb_pcm_ringbuf) {
		acts_ringbuf_destroy_ext(btmusic_rgb_pcm_ringbuf);
		btmusic_rgb_pcm_ringbuf = NULL;
	}
	btmusic_rgb_pcm_dump_on = 0;
}

#define BTMUSIC_RGB_PCM_RB_HIGH_WATER 768U
#define BTMUSIC_RGB_PCM_RB_DROP_CHUNK 128U

static int btmusic_rgb_pcm_dump_ensure(struct btmusic_app_t *btmusic)
{
	if (btmusic_rgb_pcm_dump_on) {
		return 0;
	}
	if (!btmusic || !btmusic->playback_player) {
		return -1;
	}
	btmusic_rgb_pcm_dump_start(btmusic->playback_player);
	return btmusic_rgb_pcm_dump_on ? 0 : -1;
}

static void btmusic_rgb_spread_group(short *info, int from, int to, uint32_t lvl)
{
	int i;
	int n = to - from + 1;

	if (n <= 0) {
		return;
	}
	for (i = from; i <= to; i++) {
		info[i] = (short)((lvl * (uint32_t)(i - from + 1)) / (uint32_t)n);
	}
}

/*
 * 从 dump 旁路 PCM 分段算低/中/高能量（src=1）。
 * 用 peek 读最新数据，仅在 ringbuf 将满时丢弃旧块，避免抢光解码 PCM。
 */
static int btmusic_rgb_fill_bands_from_pcm(short *info, int nshort)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
	char pcm_buf[256];
	char drop[BTMUSIC_RGB_PCM_RB_DROP_CHUNK];
	uint32_t got;
	uint32_t rb_len;
	int third;
	uint32_t e0;
	uint32_t e1;
	uint32_t e2;

	ARG_UNUSED(nshort);

	if (btmusic_rgb_pcm_dump_ensure(btmusic) != 0 ||
	    !btmusic_rgb_pcm_ringbuf) {
		return -1;
	}

	rb_len = acts_ringbuf_length(btmusic_rgb_pcm_ringbuf);
	while (rb_len > BTMUSIC_RGB_PCM_RB_HIGH_WATER) {
		acts_ringbuf_get(btmusic_rgb_pcm_ringbuf, drop, sizeof(drop));
		rb_len = acts_ringbuf_length(btmusic_rgb_pcm_ringbuf);
	}
	if (rb_len < 12U) {
		if ((btmusic_rgb_pcm_fail_log_cnt++ % 40) == 0) {
			SYS_LOG_INF("pcm rb empty len=%u on=%u\n",
				    rb_len, btmusic_rgb_pcm_dump_on);
		}
		return -1;
	}

	got = rb_len;
	if (got > sizeof(pcm_buf)) {
		got = sizeof(pcm_buf);
	}
	if (acts_ringbuf_peek(btmusic_rgb_pcm_ringbuf, pcm_buf, got) < 12U) {
		return -1;
	}

	third = (int)got / 3;
	e0 = energy_statistics_bitdepth(pcm_buf, third,
				      BTMUSIC_RGB_PCM_SAMPLE_BITS);
	e1 = energy_statistics_bitdepth(pcm_buf + third, third,
				      BTMUSIC_RGB_PCM_SAMPLE_BITS);
	e2 = energy_statistics_bitdepth(pcm_buf + 2 * third,
					(int)got - 2 * third,
					BTMUSIC_RGB_PCM_SAMPLE_BITS);
	if (!e0 && !e1 && !e2) {
		return -1;
	}

	btmusic_rgb_spread_group(info, 0, 3, e0);
	btmusic_rgb_spread_group(info, 4, 6, e1);
	btmusic_rgb_spread_group(info, 7, 9, e2);
	btmusic_rgb_energy_src = 1;
	return 0;
}
#endif /* CONFIG_BT_MUSIC_RGB_PCM_DUMP */

#define BTMUSIC_RGB_STREAM_LVL_MAX    8000U

#define BTMUSIC_RGB_DSP_PATH_UNLOCK   255U
#define BTMUSIC_RGB_DSP_PATH_FREQBAND 0U
#define BTMUSIC_RGB_DSP_PATH_TIMEDOMAIN 1U
#define BTMUSIC_RGB_DSP_PATH_FREQPOINT 2U
#define BTMUSIC_RGB_DSP_PATH_FPINFO   3U
#define BTMUSIC_RGB_DSP_PATH_FAIL_MAX 8U

static short btmusic_rgb_energy_cache[10];
static uint8_t btmusic_rgb_energy_cache_valid;

static uint8_t btmusic_rgb_dsp_path_lock = BTMUSIC_RGB_DSP_PATH_UNLOCK;
static uint8_t btmusic_rgb_dsp_path_fail;

#if CONFIG_BT_MUSIC_RGB_STREAM_FALLBACK
static uint32_t btmusic_rgb_stream_smooth;
static uint32_t btmusic_rgb_stream_hp_base;
static int btmusic_rgb_stream_prev_len;
static uint32_t btmusic_rgb_stream_beat;
static uint32_t btmusic_rgb_stream_delta_accum;

static void btmusic_rgb_stream_fallback_reset(void)
{
	btmusic_rgb_stream_smooth = 0U;
	btmusic_rgb_stream_hp_base = 0U;
	btmusic_rgb_stream_prev_len = 0;
	btmusic_rgb_stream_beat = 0U;
	btmusic_rgb_stream_delta_accum = 0U;
}

/*
 * A2DP 缓冲长度回退：用 len 变化量作“节拍”，并拉开低/高频段差异。
 * 缓冲总量本身几乎不变，必须靠 delta 才能产生律动。
 */
static int btmusic_rgb_fill_bands_from_input_level(struct btmusic_app_t *btmusic,
						   short *info, int nshort)
{
	int len;
	int delta;
	uint32_t lvl;
	uint32_t smooth;
	uint32_t base;
	uint32_t beat;
	int i;

	ARG_UNUSED(nshort);

	if (!btmusic || !btmusic->sink_stream) {
		return -1;
	}

	len = stream_get_length(btmusic->sink_stream);
	if (len <= 0) {
		return -1;
	}

	lvl = (uint32_t)len;
	if (lvl > BTMUSIC_RGB_STREAM_LVL_MAX) {
		lvl = BTMUSIC_RGB_STREAM_LVL_MAX;
	}

	delta = len - btmusic_rgb_stream_prev_len;
	if (delta < 0) {
		delta = -delta;
	}
	btmusic_rgb_stream_prev_len = len;

	btmusic_rgb_stream_delta_accum = (btmusic_rgb_stream_delta_accum * 220U) / 256U;
	btmusic_rgb_stream_delta_accum += (uint32_t)delta;

	/* 缓冲增减 + 平滑量变化 → 节拍包络（快升慢降） */
	btmusic_rgb_stream_beat = (btmusic_rgb_stream_beat * 180U) / 256U;
	btmusic_rgb_stream_beat += (uint32_t)delta * 12U;
	btmusic_rgb_stream_beat += btmusic_rgb_stream_delta_accum / 4U;
	if (btmusic_rgb_stream_beat > 255U) {
		btmusic_rgb_stream_beat = 255U;
	}
	beat = btmusic_rgb_stream_beat;

	if (lvl > btmusic_rgb_stream_smooth) {
		btmusic_rgb_stream_smooth = (btmusic_rgb_stream_smooth * 1U
					     + lvl * 3U) / 4U;
	} else {
		btmusic_rgb_stream_smooth = (btmusic_rgb_stream_smooth * 240U
					     + lvl * 16U) / 256U;
	}
	smooth = btmusic_rgb_stream_smooth;

	base = (smooth * 255U) / BTMUSIC_RGB_STREAM_LVL_MAX;
	if (base > 255U) {
		base = 255U;
	}

	{
		uint32_t hp = (base > btmusic_rgb_stream_hp_base)
			      ? (base - btmusic_rgb_stream_hp_base)
			      : (btmusic_rgb_stream_hp_base - base);

		btmusic_rgb_stream_hp_base = (btmusic_rgb_stream_hp_base * 230U
					      + base * 26U) / 256U;
		if (hp > 255U) {
			hp = 255U;
		}
		beat += hp * 2U;
		if (beat > 255U) {
			beat = 255U;
		}
	}

	/* 10 段：左侧 bass 强/红色，右侧 treble 弱/蓝色；节拍优先打 bass */
	for (i = 0; i < 10; i++) {
		uint32_t w = 255U - (uint32_t)i * 22U;
		uint32_t v = (base * w) / 255U;
		uint32_t beat_w;

		if (i < 4) {
			beat_w = beat;
			v += beat / 2U;
		} else if (i < 7) {
			beat_w = beat * 2U / 3U;
		} else {
			beat_w = beat / 3U;
		}
		v += beat_w;
		if (v > 255U) {
			v = 255U;
		}
		info[i] = (short)v;
	}

	if ((btmusic_rgb_energy_log_cnt++ % 20) == 0) {
		SYS_LOG_INF("[energy] src=2 len=%d base=%u beat=%u b0=%d b9=%d\n",
			    len, (unsigned)base, (unsigned)beat,
			    info[0], info[9]);
	}

	btmusic_rgb_energy_src = 2;
	return 0;
}
#endif /* CONFIG_BT_MUSIC_RGB_STREAM_FALLBACK */

#if 0 /* DSP 能量采集已禁用 */
static void btmusic_rgb_energy_path_reset(void)
{
	btmusic_rgb_dsp_path_lock = BTMUSIC_RGB_DSP_PATH_UNLOCK;
	btmusic_rgb_dsp_path_fail = 0U;
	btmusic_rgb_energy_src = 0U;
	btmusic_rgb_energy_cache_valid = 0U;
#if CONFIG_BT_MUSIC_RGB_STREAM_FALLBACK
	btmusic_rgb_stream_fallback_reset();
#endif
}
#endif

static void btmusic_rgb_energy_cache_store(const short *info, int nshort)
{
	int i;
	int n = nshort;

	if (n > 10) {
		n = 10;
	}
	for (i = 0; i < n; i++) {
		btmusic_rgb_energy_cache[i] = info[i];
	}
	btmusic_rgb_energy_cache_valid = 1U;
}

static int btmusic_rgb_energy_cache_restore(short *info, int nshort)
{
	int i;

	if (!btmusic_rgb_energy_cache_valid) {
		return -1;
	}
	for (i = 0; i < nshort && i < 10; i++) {
		info[i] = btmusic_rgb_energy_cache[i];
	}
	return 0;
}

/* 判断能量缓冲是否有有效频段数据（排除 ringbuf head=1 等误判） */
static int btmusic_energy_buf_has_signal(const short *info, int nshort)
{
	int i;
	short max_v = 0;
	short v;

	for (i = 0; i < nshort; i++) {
		v = info[i];
		if (v < 0) {
			v = (short)(-v);
		}
		if (v > max_v) {
			max_v = v;
		}
	}
	return max_v >= 2;
}

static short btmusic_energy_peak_value(const short *info, int nshort)
{
	int i;
	short max_v = 0;
	short v;

	for (i = 0; i < nshort; i++) {
		v = info[i];
		if (v < 0) {
			v = (short)(-v);
		}
		if (v > max_v) {
			max_v = v;
		}
	}
	return max_v;
}

static int btmusic_energy_has_band_spread(const short *info, int nshort)
{
	int i;
	int n = 0;
	short v;

	for (i = 0; i < nshort && i < 10; i++) {
		v = info[i];
		if (v < 0) {
			v = (short)(-v);
		}
		if (v >= 8) {
			n++;
		}
	}
	return n >= 2;
}

/* MFBE 未开时 ringbuf 常只有时域总能量，展开为 10 段便于柱形律动 */
static void btmusic_energy_spread_timewindow(short mono, short *info, int nshort)
{
	int i;
	int n = nshort;

	if (n > 10) {
		n = 10;
	}
	for (i = 0; i < n; i++) {
		uint32_t w = 255U - (uint32_t)i * 22U;

		info[i] = (short)((uint32_t)(mono < 0 ? -mono : mono) * w / 255U);
	}
}

/*
 * 真多频段直接通过；单点总能量则展开为 bass 强 / treble 弱的伪频带。
 * 返回 0 表示可用；*spread 非 0 表示走了伪频带路径。
 */
static int btmusic_energy_finish_bands(short *info, int nshort, int *spread)
{
	if (!btmusic_energy_buf_has_signal(info, nshort)) {
		return -1;
	}
	if (btmusic_energy_has_band_spread(info, nshort)) {
		if (spread) {
			*spread = 0;
		}
		return 0;
	}

	btmusic_energy_spread_timewindow(btmusic_energy_peak_value(info, nshort),
					 info, nshort);
	if (spread) {
		*spread = 1;
	}
	return btmusic_energy_buf_has_signal(info, nshort) ? 0 : -1;
}

/* 官方 struct API：num_points + values[] */
static int btmusic_energy_read_fp_info(struct btmusic_app_t *btmusic,
				       short *info, int nshort)
{
	media_freqpoint_energy_info_t fp;
	int i;
	int n;

	memset(&fp, 0, sizeof(fp));
	if (media_player_get_freqpoint_energy(btmusic->playback_player, &fp) != 0) {
		return -1;
	}

	n = fp.num_points;
	if (n <= 0) {
		n = 10;
	}
	if (n > 10) {
		n = 10;
	}

	memset(info, 0, nshort * (int)sizeof(short));
	for (i = 0; i < n && i < nshort; i++) {
		info[i] = fp.values[i];
	}
	return btmusic_energy_buf_has_signal(info, nshort) ? 0 : -1;
}

/*
 * 读取 DSP ENERGY_BUFFER 一帧（勿循环排空，避免与播放线程争用）。
 */
static int btmusic_energy_read_dsp_once(struct btmusic_app_t *btmusic, int type,
					short *info, int nbytes, int nshort)
{
	if (media_player_get_output_energy_sample(btmusic->playback_player,
			type, info, nbytes) != 0) {
		return -1;
	}
	if (!btmusic_energy_buf_has_signal(info, nshort)) {
		return -1;
	}
	return 0;
}

static int btmusic_energy_read_dsp_bands(struct btmusic_app_t *btmusic, int type,
					 short *info, int nbytes, int nshort)
{
	if (btmusic_energy_read_dsp_once(btmusic, type, info, nbytes, nshort) != 0) {
		return -1;
	}
	return btmusic_energy_finish_bands(info, nshort, NULL);
}

static void btmusic_energy_log_src0(const char *path, const short *info, int nshort)
{
	btmusic_rgb_energy_src = 0U;
	if ((btmusic_rgb_energy_log_cnt++ % 40) == 0) {
		SYS_LOG_INF("[energy] src=0 %s b0=%d b5=%d b9=%d\n", path,
			    nshort > 0 ? info[0] : 0,
			    nshort > 5 ? info[5] : 0,
			    nshort > 9 ? info[9] : 0);
	}
}

static int btmusic_energy_try_freqband(struct btmusic_app_t *btmusic,
				       short *info, int nbytes, int nshort)
{
	if (btmusic_energy_read_dsp_bands(btmusic,
			MEDIA_EFFECT_EXT_GET_FREQBAND_ENERGY,
			info, nbytes, nshort) != 0) {
		return -1;
	}
	btmusic_energy_log_src0("freqband", info, nshort);
	return 0;
}

static int btmusic_energy_try_timedomain(struct btmusic_app_t *btmusic,
					 short *info, int nbytes, int nshort,
					 int *spread)
{
	if (btmusic_energy_read_dsp_once(btmusic,
			MEDIA_EFFECT_EXT_GET_TIMEDOMAIN_ENERGY,
			info, nbytes, nshort) != 0) {
		return -1;
	}
	if (btmusic_energy_finish_bands(info, nshort, spread) != 0) {
		return -1;
	}
	btmusic_energy_log_src0(*spread ? "td_tw" : "timedomain", info, nshort);
	return 0;
}

static int btmusic_energy_try_freqpoint(struct btmusic_app_t *btmusic,
					short *info, int nbytes, int nshort)
{
	if (btmusic_energy_read_dsp_bands(btmusic,
			MEDIA_EFFECT_EXT_GET_FREQPOINT_ENERGY,
			info, nbytes, nshort) != 0) {
		return -1;
	}
	btmusic_energy_log_src0("freqpoint", info, nshort);
	return 0;
}

static int btmusic_energy_try_fp_info(struct btmusic_app_t *btmusic,
				      short *info, int nshort, int *spread)
{
	if (btmusic_energy_read_fp_info(btmusic, info, nshort) != 0) {
		return -1;
	}
	if (btmusic_energy_finish_bands(info, nshort, spread) != 0) {
		return -1;
	}
	btmusic_energy_log_src0(*spread ? "fp_tw" : "fp_info", info, nshort);
	return 0;
}

static int btmusic_energy_try_locked_path(struct btmusic_app_t *btmusic,
					  short *info, int nbytes, int nshort)
{
	int spread = 0;
	int ret = -1;

	switch (btmusic_rgb_dsp_path_lock) {
	case BTMUSIC_RGB_DSP_PATH_FREQBAND:
		ret = btmusic_energy_try_freqband(btmusic, info, nbytes, nshort);
		break;
	case BTMUSIC_RGB_DSP_PATH_TIMEDOMAIN:
		ret = btmusic_energy_try_timedomain(btmusic, info, nbytes,
						    nshort, &spread);
		break;
	case BTMUSIC_RGB_DSP_PATH_FREQPOINT:
		ret = btmusic_energy_try_freqpoint(btmusic, info, nbytes, nshort);
		break;
	case BTMUSIC_RGB_DSP_PATH_FPINFO:
		ret = btmusic_energy_try_fp_info(btmusic, info, nshort, &spread);
		break;
	default:
		break;
	}

	if (ret == 0) {
		btmusic_rgb_dsp_path_fail = 0U;
		btmusic_rgb_energy_cache_store(info, nshort);
		return 0;
	}

	if (btmusic_rgb_dsp_path_lock == BTMUSIC_RGB_DSP_PATH_UNLOCK) {
		return -1;
	}

	btmusic_rgb_dsp_path_fail++;
	if (btmusic_rgb_dsp_path_fail < BTMUSIC_RGB_DSP_PATH_FAIL_MAX) {
		if (btmusic_rgb_energy_cache_restore(info, nshort) == 0) {
			return 0;
		}
		return -1;
	}

	btmusic_rgb_dsp_path_lock = BTMUSIC_RGB_DSP_PATH_UNLOCK;
	btmusic_rgb_dsp_path_fail = 0U;
	return -1;
}

/*
 * 读取 A2DP 播放频段能量（多级回退，均走 media API，禁止 CPU 直读 ENERGY_BUFFER）：
 * 1) GET_FREQBAND_ENERGY(17)（lcmusic 同款）
 * 2) GET_TIMEDOMAIN_ENERGY(16) + 伪频带展开
 * 3) GET_FREQPOINT_ENERGY(15) / fp_info
 * 4) sink 缓冲回退
 */
int btmusic_a2dp_get_freqpoint_energy(short *info, int size)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
	int nbytes;
	int nshort;
	int spread = 0;

	if (!btmusic || !btmusic->media_opened) {
		return -1;
	}

	if (!btmusic->playback_player || !btmusic->playback_player_run) {
		return -1;
	}

	if (!info || size < (int)sizeof(short)) {
		return -EINVAL;
	}

	nbytes = size;
	if (nbytes > 24) {
		nbytes = 24;
	}
	nshort = nbytes / (int)sizeof(short);

	memset(info, 0, size);
	if (btmusic_rgb_dsp_path_lock != BTMUSIC_RGB_DSP_PATH_UNLOCK) {
		if (btmusic_energy_try_locked_path(btmusic, info, nbytes,
						   nshort) == 0) {
			return 0;
		}
	}

	memset(info, 0, size);
	if (btmusic_energy_try_freqband(btmusic, info, nbytes, nshort) == 0) {
		btmusic_rgb_dsp_path_lock = BTMUSIC_RGB_DSP_PATH_FREQBAND;
		btmusic_rgb_dsp_path_fail = 0U;
		btmusic_rgb_energy_cache_store(info, nshort);
		return 0;
	}

	memset(info, 0, size);
	if (btmusic_energy_try_timedomain(btmusic, info, nbytes, nshort,
					  &spread) == 0) {
		btmusic_rgb_dsp_path_lock = BTMUSIC_RGB_DSP_PATH_TIMEDOMAIN;
		btmusic_rgb_dsp_path_fail = 0U;
		btmusic_rgb_energy_cache_store(info, nshort);
		return 0;
	}

	memset(info, 0, size);
	if (btmusic_energy_try_freqpoint(btmusic, info, nbytes, nshort) == 0) {
		btmusic_rgb_dsp_path_lock = BTMUSIC_RGB_DSP_PATH_FREQPOINT;
		btmusic_rgb_dsp_path_fail = 0U;
		btmusic_rgb_energy_cache_store(info, nshort);
		return 0;
	}

	memset(info, 0, size);
	if (btmusic_energy_try_fp_info(btmusic, info, nshort, &spread) == 0) {
		btmusic_rgb_dsp_path_lock = BTMUSIC_RGB_DSP_PATH_FPINFO;
		btmusic_rgb_dsp_path_fail = 0U;
		btmusic_rgb_energy_cache_store(info, nshort);
		return 0;
	}

#if CONFIG_BT_MUSIC_RGB_PCM_DUMP
	memset(info, 0, size);
	if (btmusic_rgb_fill_bands_from_pcm(info, nshort) == 0) {
		return 0;
	}
#endif

#if CONFIG_BT_MUSIC_RGB_STREAM_FALLBACK
	memset(info, 0, size);
	if (btmusic_rgb_fill_bands_from_input_level(btmusic, info, nshort) == 0) {
		return 0;
	}
#endif

	if ((btmusic_rgb_energy_log_cnt++ % 20) == 0) {
		SYS_LOG_INF("[energy] all paths fail open=%u run=%u\n",
			    btmusic->media_opened, btmusic->playback_player_run);
	}

	return -1;
}
#endif

#ifdef CONFIG_BTMUSIC_BMS_APP
static io_stream_t broadcast_create_source_stream(int mem_type, int block_num)
{
	io_stream_t stream = NULL;
	int buff_size;
	int ret;

	buff_size = media_mem_get_cache_pool_size_ext(mem_type, AUDIO_STREAM_SOUNDBAR, NAV_TYPE, BROADCAST_SDU, block_num);	// TODO: 3?
	if (buff_size <= 0) {
		goto exit;
	}

	stream =
	    ringbuff_stream_create_ext(media_mem_get_cache_pool
				       (mem_type, AUDIO_STREAM_SOUNDBAR),
				       buff_size);
	if (!stream) {
		goto exit;
	}

	ret = stream_open(stream, MODE_OUT);
	if (ret) {
		stream_destroy(stream);
		stream = NULL;
		goto exit;
	}

 exit:
	SYS_LOG_INF("%p\n", stream);
	return stream;
}

static io_stream_t _btmusic_bms_create_capture_input_stream(void)
{
	io_stream_t input_stream;
	int ret;

	input_stream =
	    ringbuff_stream_create_ext(media_mem_get_cache_pool
				       (INPUT_CAPTURE, AUDIO_STREAM_SOUNDBAR),
				       media_mem_get_cache_pool_size
				       (INPUT_CAPTURE, AUDIO_STREAM_SOUNDBAR));

	if (!input_stream) {
		goto exit;
	}

	ret =
	    stream_open(input_stream,
			MODE_IN_OUT | MODE_READ_BLOCK | MODE_BLOCK_TIMEOUT);
	if (ret) {
		stream_destroy(input_stream);
		input_stream = NULL;
		goto exit;
	}

 exit:
	SYS_LOG_INF("%p\n", input_stream);
	return input_stream;
}

static void _btmusic_bms_broadcast_source_stream_set(uint8 enable_flag)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
	struct bt_broadcast_chan *chan;
	int i;

	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		chan = &btmusic->broad_chan[i];
		if (chan->handle == 0 && chan->id == 0) {
			continue;
		}
		if (enable_flag) {
			bt_manager_broadcast_source_stream_set(chan,
							       btmusic->stream[i]);
			bt_manager_a2dp_auracast_status_set(true);
		} else {
			bt_manager_a2dp_auracast_status_set(false);
			bt_manager_broadcast_source_stream_set(chan, NULL);
		}

	}
}

int btmusic_bms_init_capture(void)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
	struct bt_broadcast_chan *chan;
	struct bt_broadcast_chan_info chan_info;
	io_stream_t source_stream = NULL;
	io_stream_t source_stream2 = NULL;
	io_stream_t input_stream = NULL;
	media_init_param_t init_param;
	int ret;

	if (NULL == btmusic) {
		return -EINVAL;
	}
	if (btmusic->capture_player) {
		SYS_LOG_INF("already\n");
		return -EALREADY;
	}

	chan = btmusic->chan;
	ret = bt_manager_broadcast_stream_info(chan, &chan_info);
	if (ret) {
		return ret;
	}

	/* wait until start_capture */
#ifdef CONFIG_PLAYTTS
	tts_manager_wait_finished(false);
#endif

	source_stream = broadcast_create_source_stream(OUTPUT_CAPTURE, 6);
	if (!source_stream) {
		goto exit;
	}
#if (BROADCAST_NUM_BIS == 2)
	if(!app_tws_status_get_enable()) {
		source_stream2 = broadcast_create_source_stream(OUTPUT_CAPTURE2, 6);
		if (!source_stream2) {
			goto exit;
		}
	}
#endif

	input_stream = _btmusic_bms_create_capture_input_stream();
	if (!input_stream) {
		goto exit;
	}

	memset(&init_param, 0, sizeof(media_init_param_t));

	init_param.type = MEDIA_SRV_TYPE_CAPTURE;
	init_param.stream_type = AUDIO_STREAM_SOUNDBAR;
	init_param.efx_stream_type = AUDIO_STREAM_SOUNDBAR;
	// TODO: broadcast_source no need to get chan_info???
	init_param.capture_format = NAV_TYPE;	// bt_manager_audio_codec_type(chan_info.format);
	init_param.capture_sample_rate_input = audio_system_get_output_sample_rate();	// chan_info.sample;
	init_param.capture_sample_rate_output = 48;	// chan_info.sample;
	init_param.capture_sample_bits = 32;
	//to do?
	init_param.capture_channels_input = 2;
	if(app_tws_status_get_enable()) {
		init_param.capture_channels_output = 1;
		init_param.capture_bit_rate = chan_info.kbps;
	} else {
		init_param.capture_channels_output = NUM_OF_BROAD_CHAN;
		init_param.capture_bit_rate = chan_info.kbps*BROADCAST_NUM_BIS; //BROADCAST_KBPS
	}
	init_param.capture_input_stream = input_stream;
	init_param.capture_output_stream = source_stream;
	init_param.capture_output_stream2 = source_stream2;
	init_param.waitto_start = 1;
	init_param.device_info.tx_chan.validate = 1;
	init_param.device_info.tx_chan.type = MEDIA_AUDIO_DEVICE_TYPE_BIS;
	init_param.device_info.tx_chan.bis_chan.handle = chan->handle;
	init_param.device_info.tx_chan.bis_chan.id = chan->id;
	init_param.device_info.tx_chan.timeline_owner = chan_info.chan;

	if (chan_info.duration == 7) {
		audio_policy_set_nav_frame_size_us(7500);
	} else {
		audio_policy_set_nav_frame_size_us(10000);
	}

	SYS_LOG_INF("chan_info: %d %d %d %d %d %p\n",
	     chan_info.format, chan_info.sample, chan_info.channels,
	     chan_info.kbps, chan_info.duration, chan_info.chan);
	SYS_LOG_INF("chan handle:%d, id:%d\n", chan->handle, chan->id);

	btmusic->capture_player = media_player_open(&init_param);
	if (!btmusic->capture_player) {
		goto exit;
	}

	btmusic->stream[0] = source_stream;
	btmusic->stream[1] = source_stream2;
	btmusic->input_stream = input_stream;
	SYS_LOG_INF("%p\n", btmusic->capture_player);

	return 0;

 exit:
	if (source_stream) {
		stream_close(source_stream);
		stream_destroy(source_stream);
	}
	if (source_stream2) {
		stream_close(source_stream2);
		stream_destroy(source_stream2);
	}
	if (input_stream) {
		stream_close(input_stream);
		stream_destroy(input_stream);
	}

	SYS_LOG_ERR("open failed\n");
	return -EINVAL;
}

const uint8_t bms_irc_normal[5] = {0, 1, 2, 3, BROADCAST_IRC};
const uint8_t bms_irc_tws[5] = {0, 2, 3, 4, BROADCAST_1_IRC};

int btmusic_bms_check_br_audio_stream(u16_t pcm_time, u16_t normal_level, u8_t aps_status)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
	uint8_t need_adjust = false;
	u16_t level_low = 0;
	u16_t level_normal = normal_level;
	uint8_t *btn = (uint8_t *)bms_irc_normal;

	if(app_tws_status_get_enable()) {
		btn = (uint8_t *)bms_irc_tws;
	}

	if (!btmusic->capture_player_run) {
		return -EINVAL;
	}

	if (!btmusic->broadcast_source_enabled) {
		return 0;
	}

	if(normal_level > btmusic->bis_delay) {
		level_normal = normal_level - btmusic->bis_delay;
	}

	if (!btmusic->dsp_run) {
		if (pcm_time >= level_normal) {
			btmusic->dsp_run = 1;
		}
	}

	if(btmusic->dsp_run) {
		level_low = level_normal  >> 2;
		if (pcm_time <= (level_low + btmusic->bis_delay + MEDIA_PACKET_TIME_THRESHOLD)) {
			btmusic->broadcast_level_hold_time = 0;
			if(btmusic->bms_transmit_num_index != 1) {
				btmusic->bms_transmit_num_index = 1;
				need_adjust = true;
			}
		} else if (pcm_time <= ((level_normal >> 1) + btmusic->bis_delay) + MEDIA_PACKET_TIME_THRESHOLD) {
			btmusic->broadcast_level_hold_time = 0;
			if(pcm_time >= (level_normal >> 1) + btmusic->bis_delay) {
				if(btmusic->bms_transmit_num_index != 2) {
					btmusic->bms_transmit_num_index = 2;
					need_adjust = true;
				}
			} else {
				if(btmusic->bms_transmit_num_index > 2) {
					btmusic->bms_transmit_num_index = 2;
					need_adjust = true;
				}
			}
		} else if (pcm_time <= level_low * 3 + btmusic->bis_delay +  MEDIA_PACKET_TIME_THRESHOLD) {
			btmusic->broadcast_level_hold_time = 0;
			if(pcm_time >= level_low * 3 + btmusic->bis_delay) {
				if(btmusic->bms_transmit_num_index < (BTMUSIC_BMS_TRANSMIT_INDEX - 1)) {
					btmusic->bms_transmit_num_index += 1;
					need_adjust = true;
				} else {
					if(btmusic->bms_transmit_num_index != BTMUSIC_BMS_TRANSMIT_INDEX - 1) {
						btmusic->bms_transmit_num_index = BTMUSIC_BMS_TRANSMIT_INDEX - 1;
						need_adjust = true;
					}
				}
			} else {
				if(btmusic->bms_transmit_num_index > BTMUSIC_BMS_TRANSMIT_INDEX - 1) {
					btmusic->bms_transmit_num_index = BTMUSIC_BMS_TRANSMIT_INDEX - 1;
					need_adjust = true;
				} else if(btmusic->bms_transmit_num_index < 2) {
					btmusic->bms_transmit_num_index = 2;
					need_adjust = true;
				}
			}
		} else {
			if(pcm_time >= level_normal + btmusic->bis_delay) {
				if(btmusic->bms_transmit_num_index < BTMUSIC_BMS_TRANSMIT_INDEX) {
					btmusic->bms_transmit_num_index += 1;
					need_adjust = true;
				}
			} else {
				if(btmusic->bms_transmit_num_index < (BTMUSIC_BMS_TRANSMIT_INDEX - 1)) {
					btmusic->bms_transmit_num_index = BTMUSIC_BMS_TRANSMIT_INDEX - 1;
					need_adjust = true;
				} else {
					if(btmusic->bms_transmit_num_index == BTMUSIC_BMS_TRANSMIT_INDEX - 1) {
						if(pcm_time >= (level_normal + btmusic->bis_delay - MEDIA_PACKET_TIME_THRESHOLD)) {
							btmusic->broadcast_level_hold_time++;
							if(btmusic->broadcast_level_hold_time >= MEDIA_LEVEL_HOLD_TIME) {
								btmusic->broadcast_level_hold_time = 0;
								btmusic->bms_transmit_num_index = BTMUSIC_BMS_TRANSMIT_INDEX;
								need_adjust = true;
							}
						}
					}
				}
			}
		}

		if (need_adjust) {
			btif_broadcast_source_set_retransmit(btmusic->chan,
				btn[btmusic->bms_transmit_num_index]);
			SYS_LOG_INF("pcm:%d level:%d btn:%d\n", pcm_time, level_normal,
				btn[btmusic->bms_transmit_num_index]);

			btmusic->pcm_time = pcm_time;
		}
	}
	return 0;
}

int btmusic_bms_start_capture(void)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();

	if (!btmusic->capture_player) {
		SYS_LOG_INF("not ready\n");
		return -EINVAL;
	}

	if (btmusic->capture_player_run) {
		SYS_LOG_INF("already\n");
		return -EINVAL;
	}
#ifdef CONFIG_PLAYTTS
	tts_manager_wait_finished(false);
#endif

	media_player_play(btmusic->capture_player);

	_btmusic_bms_broadcast_source_stream_set(1);

	btmusic->capture_player_run = 1;

#ifdef SUPPORT_RETRANSMIT_NUM_ADJUST
	audio_asp_set_pcm_monitor_callback(btmusic_bms_check_br_audio_stream);
	btif_broadcast_source_set_retransmit(btmusic->chan,btmusic->irc);
#endif

	SYS_LOG_INF("player:%p irc:%d\n", btmusic->capture_player,btmusic->irc);

	return 0;
}

int btmusic_bms_stop_capture(void)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
	int i;

	if (!btmusic->capture_player || !btmusic->capture_player_run) {
		SYS_LOG_INF("not ready\n");
		return -EINVAL;
	}

	_btmusic_bms_broadcast_source_stream_set(0);

	media_player_stop(btmusic->capture_player);

	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		if (btmusic->stream[i]) {
			stream_close(btmusic->stream[i]);
		}
	}

	if (btmusic->input_stream) {
		stream_close(btmusic->input_stream);
	}

	btmusic->capture_player_run = 0;
	btmusic->dsp_run = 0;

#ifdef SUPPORT_RETRANSMIT_NUM_ADJUST
	audio_asp_set_pcm_monitor_callback(NULL);
#endif

	SYS_LOG_INF("%p\n", btmusic->capture_player);

	return 0;
}

int btmusic_bms_exit_capture(void)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();
	int i;

	if (!btmusic->capture_player) {
		SYS_LOG_INF("already\n");
		return -EALREADY;
	}

	media_player_close(btmusic->capture_player);
	for (i = 0; i < NUM_OF_BROAD_CHAN; i++) {
		if (btmusic->stream[i]) {
			stream_destroy(btmusic->stream[i]);
			btmusic->stream[i] = NULL;
		}
	}
	if (btmusic->input_stream) {
		stream_destroy(btmusic->input_stream);
		btmusic->input_stream = NULL;
	}

	SYS_LOG_INF("%p\n", btmusic->capture_player);

	btmusic->capture_player = NULL;
	btmusic->capture_player_run = 0;
	btmusic->dsp_run = 0;

#ifdef SUPPORT_RETRANSMIT_NUM_ADJUST
	audio_asp_set_pcm_monitor_callback(NULL);
#endif
	return 0;
}
#endif
