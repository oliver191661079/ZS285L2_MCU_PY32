/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt music app main.
 */

#include "btmusic.h"
#include "btmusic_ws2812.h"
#include "tts_manager.h"
#include <ringbuff_stream.h>

#ifdef CONFIG_SYSTEM_APP_PY32_UART
extern void py32_rhythm_set_data(const uint8_t *bands, uint8_t playing);
#endif
#ifdef CONFIG_PROPERTY
#include "property_manager.h"
#endif

#ifdef CONFIG_ACT_EVENT
#include <app_act_event_id.h>
#include <logging/log_core.h>
LOG_MODULE_DECLARE(main, CONFIG_ACT_EVENT_APP_COMPILE_LEVEL);
#endif

static struct btmusic_app_t *p_btmusic_app;

#ifdef CONFIG_BTMUSIC_BMS_APP
static struct bt_broadcast_qos source_qos = {
	.packing = BT_AUDIO_PACKING_INTERLEAVED,
	.framing = BT_AUDIO_UNFRAMED,
	.phy = BT_AUDIO_PHY_2M,
	.rtn = 2,
	.max_sdu = BROADCAST_SDU,
	/* max_transport_latency, unit: ms */
	.latency = BROADCAST_LAT,
	/* sdu_interval, unit: us */
	.interval = BROADCAST_SDU_INTERVAL,
	/* presentation_delay, unit: us */
	.delay = BCST_QOS_DELAY,
	/* processing_time, unit: us */
	.processing = 9000,
};
#endif

#ifdef CONFIG_BT_MUSIC_FREQPOINT_ENERGY_DEMO

#if !CONFIG_BT_MUSIC_RGB_RHYTHM_DEBUG
/*
 * 原厂 demo：定时打印 DSP 各频段原始能量（十六进制）。
 * 在 btmusic.h 将 CONFIG_BT_MUSIC_RGB_RHYTHM_DEBUG 置 0 时启用。
 */
static void btmusic_energy_demo_raw_hex_dump(struct thread_timer *ttimer,
					     void *expiry_fn_arg)
{
	short temp_info[12] = {0};

	ARG_UNUSED(ttimer);
	ARG_UNUSED(expiry_fn_arg);

	if (btmusic_a2dp_get_freqpoint_energy(temp_info, sizeof(temp_info)) == 0) {
		SYS_LOG_INF("fp energy(raw hex):%x,%x,%x,%x,%x,%x,%x,%x,%x\n",
			    temp_info[0],
			    temp_info[1],
			    temp_info[2],
			    temp_info[3],
			    temp_info[4],
			    temp_info[5],
			    temp_info[6],
			    temp_info[7], temp_info[8]);
	}
}
#else
/*
 * RGB 音乐律动调试：将 short 频段能量平滑、归一化后映射 bass/mid/high 与 R/G/B。
 * 目的：在 DSP 能量未就绪时仍可验证算法与日志，后续在 poll 内接 WS2812 驱动。
 * 轮询周期 100 ms（见 btmusic_main 中 energy_timer 启动参数）。
 */
#define BTMUSIC_RGB_BAND_NUM          10
#define BTMUSIC_RGB_SMOOTH_DEN        36
#define BTMUSIC_RGB_SMOOTH_NEW        4
#define BTMUSIC_RGB_DSP_SMOOTH_DEN    16
#define BTMUSIC_RGB_DSP_SMOOTH_NEW    12
#define BTMUSIC_RGB_SILENCE_FADE_DEN  240U
#define BTMUSIC_RGB_SILENCE_OFF_SUM   4U
#define BTMUSIC_RGB_FRAME_PEAK_FLOOR  4U
#define BTMUSIC_RGB_PEAK_INIT         128U
#define BTMUSIC_RGB_RAW_GAIN          16U

/*
 * 律动 LED 灯带刷新开关。
 * 置 1：调用 btmusic_ws2812_show_spectrum() 刷新 GPIO14 矩阵（irq_lock 约 5ms）。
 * 置 0：仅能量采样/平滑/日志，不刷灯带（避免音频 underrun 导致无声/死机）。
 * 测试律动时改为 1，出问题改回 0 即可还原。
 */
/*
 * 本地 WS2812 律动刷新：产品律动在 PY32，BTM 侧保持 0。
 * 置 1 仅用于本机灯带调试。
 */
#define BTMUSIC_RGB_RHYTHM_LED_SHOW	0

static uint32_t btmusic_rgb_smooth[BTMUSIC_RGB_BAND_NUM];
static uint32_t btmusic_rgb_peak[BTMUSIC_RGB_BAND_NUM];
static uint8_t btmusic_rgb_disp[BTMUSIC_RGB_BAND_NUM];
static uint8_t btmusic_rgb_hold_lvl[BTMUSIC_RGB_BAND_NUM];
static uint8_t btmusic_rgb_hold_valid;
static uint32_t btmusic_rgb_total_peak;
static uint8_t btmusic_rgb_zero_log_cnt;
static uint8_t btmusic_rgb_log_cnt;
static uint8_t btmusic_rgb_silence_cnt;

/* 伽马校正：压低暗部、突出高电平，使 LED 观感更接近人耳响度 */
static uint8_t btmusic_rgb_gamma(uint8_t lvl)
{
	return (uint8_t)(((uint16_t)lvl * (uint16_t)lvl) / 255U);
}

/*
 * 由低/中/高频段能量比决定色相，由总能量决定亮度，再合成 R/G/B。
 * 修改目的：避免 R/G/B 直接等于 bass/mid/high，颜色随频谱比例变化。
 */
static void btmusic_rgb_map_to_color(uint32_t low, uint32_t mid, uint32_t high,
				     uint8_t *r, uint8_t *g, uint8_t *b)
{
	uint32_t total = low + mid + high;
	uint8_t bright;
	uint32_t rr;
	uint32_t gg;
	uint32_t bb;

	if (total == 0) {
		*r = 0;
		*g = 0;
		*b = 0;
		return;
	}

	if (total > btmusic_rgb_total_peak) {
		btmusic_rgb_total_peak = total;
	}
	btmusic_rgb_total_peak = (btmusic_rgb_total_peak * 99U) / 100U;
	if (btmusic_rgb_total_peak < BTMUSIC_RGB_PEAK_INIT) {
		btmusic_rgb_total_peak = BTMUSIC_RGB_PEAK_INIT;
	}

	bright = (uint8_t)((total * 255U) / btmusic_rgb_total_peak);
	bright = btmusic_rgb_gamma(bright);

	rr = low * 255U / total;
	gg = mid * 255U / total;
	bb = high * 255U / total;

	*r = (uint8_t)((rr * bright) / 255U);
	*g = (uint8_t)((gg * bright) / 255U);
	*b = (uint8_t)((bb * bright) / 255U);
}

/* 将 DSP/回退路径的 signed 能量取绝对值并限幅到 16 位 */
static uint16_t btmusic_rgb_energy_u16(short v)
{
	int32_t x = v;

	if (x < 0) {
		x = -x;
	}
	if (x > 65535) {
		return 65535;
	}
	return (uint16_t)x;
}

/* 对指定频段区间求 0~255 电平均值 */
static uint32_t btmusic_rgb_avg_u8(const uint8_t *lvl, int from, int to)
{
	uint32_t sum = 0U;
	int n = 0;
	int i;

	for (i = from; i <= to && i < BTMUSIC_RGB_BAND_NUM; i++) {
		sum += lvl[i];
		n++;
	}
	return n ? (sum / (uint32_t)n) : 0U;
}

/*
 * DSP freqband 原始值通常仅 0~30，直接放大 + 帧内峰值归一化后送 LED。
 * 比长平滑链更跟节拍，且避免 smooth 参数错误导致能量塌缩。
 */
static void btmusic_rgb_fill_band_levels_from_raw(const short *raw,
						  uint8_t *disp,
						  uint8_t *band_lvl)
{
	uint8_t mx = 0U;
	uint8_t tmp[BTMUSIC_RGB_BAND_NUM];
	int i;

	for (i = 0; i < BTMUSIC_RGB_BAND_NUM; i++) {
		uint32_t v = (uint32_t)btmusic_rgb_energy_u16(raw[i])
			     * BTMUSIC_RGB_RAW_GAIN;

		if (v > 255U) {
			v = 255U;
		}
		tmp[i] = (uint8_t)v;
		if (tmp[i] > mx) {
			mx = tmp[i];
		}
	}

	if (mx < 2U) {
		for (i = 0; i < BTMUSIC_RGB_BAND_NUM; i++) {
			if (disp[i] > 0U) {
				disp[i] = (uint8_t)((uint16_t)disp[i] * 200U / 256U);
			}
			band_lvl[i] = disp[i];
		}
		return;
	}

	for (i = 0; i < BTMUSIC_RGB_BAND_NUM; i++) {
		uint8_t lvl = (uint8_t)((uint16_t)tmp[i] * 255U / mx);

		disp[i] = lvl;
		band_lvl[i] = lvl;
	}
}

/* 本地律动灯已关闭；仅在同时打开 LED_SHOW + LED_RHYTHM 时需要 */
#if BTMUSIC_RGB_RHYTHM_LED_SHOW && defined(CONFIG_BT_MUSIC_LED_RHYTHM)
static uint8_t btmusic_rgb_led_skip_cnt;
#endif

static void btmusic_ws2812_show_spectrum_boosted(const uint8_t *band_lvl,
						 int band_num)
{
	struct btmusic_app_t *app = btmusic_get_app();
	uint8_t playing;

	/* 判断播放状态 */
	playing = (app && app->playback_player_run && app->media_opened) ? 1U : 0U;

	/* 律动数据只发给 PY32，BTM 不再刷本地律动灯 */
#if defined(CONFIG_SYSTEM_APP_PY32_UART)
	if (playing && band_lvl && band_num >= BTMUSIC_RGB_BAND_NUM) {
		py32_rhythm_set_data(band_lvl, playing);
	} else {
		static const uint8_t zero_bands[BTMUSIC_RGB_BAND_NUM] = {0};
		py32_rhythm_set_data(zero_bands, 0);
	}
#endif

#if BTMUSIC_RGB_RHYTHM_LED_SHOW && defined(CONFIG_BT_MUSIC_LED_RHYTHM)
	if (!playing) {
		if ((++btmusic_rgb_led_skip_cnt % 20) == 0) {
			SYS_LOG_INF("[led] skip: app=%p run=%u opened=%u",
				    app,
				    app ? app->playback_player_run : 0U,
				    app ? app->media_opened : 0U);
		}
		return;
	}
	btmusic_ws2812_show_spectrum(band_lvl, band_num);
#else
	(void)band_lvl;
	(void)band_num;
#endif
}

/* 进入 btmusic 应用或开始能量定时器前，清零平滑/峰值状态 */
static void btmusic_rgb_rhythm_debug_reset(void)
{
	int i;

	btmusic_rgb_zero_log_cnt = 0;
	btmusic_rgb_log_cnt = 0;
	btmusic_rgb_silence_cnt = 0;
	btmusic_rgb_hold_valid = 0U;
	btmusic_ws2812_spectrum_reset();
	for (i = 0; i < BTMUSIC_RGB_BAND_NUM; i++) {
		btmusic_rgb_smooth[i] = 0;
		btmusic_rgb_peak[i] = BTMUSIC_RGB_PEAK_INIT;
		btmusic_rgb_disp[i] = 0;
	}
	btmusic_rgb_total_peak = BTMUSIC_RGB_PEAK_INIT;
}

/*
 * 律动调试主循环：读频段能量 -> 平滑 -> 低/中/高分组 -> 打印 RGB 与 src。
 * src 见 btmusic_rgb_energy_src：0 真 DSP，2 为 A2DP 缓冲回退（常见）。
 */
static void btmusic_rgb_rhythm_debug_poll(struct thread_timer *ttimer,
					  void *expiry_fn_arg)
{
	short raw[BTMUSIC_RGB_BAND_NUM];
	uint8_t band_lvl[BTMUSIC_RGB_BAND_NUM];
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint32_t low_e;
	uint32_t mid_e;
	uint32_t high_e;
	int i;

	ARG_UNUSED(ttimer);
	ARG_UNUSED(expiry_fn_arg);

	int ret;

	ret = btmusic_a2dp_get_freqpoint_energy(raw, sizeof(raw));
	if (ret != 0) {
		struct btmusic_app_t *app = btmusic_get_app();

		if ((btmusic_rgb_zero_log_cnt++ % 20) == 0) {
			int sink_level = 0;

			if (app && app->sink_stream) {
				sink_level = stream_get_length(app->sink_stream);
			}
			SYS_LOG_INF("[dbg] fail ret=%d open=%u run=%u play=%u sink_lvl=%d sink=%p chan=0x%x",
				    ret,
				    app ? app->media_opened : 0U,
				    app ? app->playback_player_run : 0U,
				    app ? app->playing : 0U,
				    sink_level,
				    app ? app->sink_stream : (void *)0,
				    app ? app->sink_chan.handle : 0U);
		}
		/* 偶发读失败时继续显示上一帧有效柱形 */
		if (app && app->playback_player_run && btmusic_rgb_hold_valid) {
			btmusic_ws2812_show_spectrum_boosted(btmusic_rgb_hold_lvl,
							     BTMUSIC_RGB_BAND_NUM);
			return;
		}
#if FAKE_RHYTHM_DEMO
		/* 假律动模式：DSP 失败也继续刷新（数据在 show_spectrum 内生成） */
		if (app && app->playback_player_run) {
			btmusic_ws2812_show_spectrum_boosted(NULL, 0);
			return;
		}
#endif
		if (!app || !app->playback_player_run) {
#if BTMUSIC_RGB_RHYTHM_LED_SHOW && \
	defined(CONFIG_BT_MUSIC_LED_STRIP2) && defined(CONFIG_BT_MUSIC_LED_RHYTHM)
			btmusic_ws2812_strip2_clear();
#endif
			/* 停止播放：强制发送全零给 PY32，确保点阵屏熄灭 */
#if defined(CONFIG_SYSTEM_APP_PY32_UART)
			static const uint8_t zero_bands[BTMUSIC_RGB_BAND_NUM] = {0};
			py32_rhythm_set_data(zero_bands, 0);
#endif
		}
		return;
	}

	for (i = 0; i < BTMUSIC_RGB_BAND_NUM; i++) {
		uint32_t e = btmusic_rgb_energy_u16(raw[i]);

		e *= BTMUSIC_RGB_RAW_GAIN;
		if (e > 65535U) {
			e = 65535U;
		}

		btmusic_rgb_smooth[i] = (btmusic_rgb_smooth[i]
					 * (BTMUSIC_RGB_DSP_SMOOTH_DEN
					    - BTMUSIC_RGB_DSP_SMOOTH_NEW)
					 + e * BTMUSIC_RGB_DSP_SMOOTH_NEW)
					/ BTMUSIC_RGB_DSP_SMOOTH_DEN;
	}

	btmusic_rgb_fill_band_levels_from_raw(raw, btmusic_rgb_disp, band_lvl);

	low_e = btmusic_rgb_avg_u8(band_lvl, 0, 3);
	mid_e = btmusic_rgb_avg_u8(band_lvl, 4, 6);
	high_e = btmusic_rgb_avg_u8(band_lvl, 7, 9);

	btmusic_rgb_map_to_color(low_e, mid_e, high_e, &r, &g, &b);

	{
		short raw_peak = 0;
		int j;

		for (j = 0; j < BTMUSIC_RGB_BAND_NUM; j++) {
			short v = raw[j];

			if (v < 0) {
				v = (short)(-v);
			}
			if (v > raw_peak) {
				raw_peak = v;
			}
		}

		if (raw_peak < 2 && low_e + mid_e + high_e < 8U) {
			btmusic_rgb_silence_cnt++;
			for (i = 0; i < BTMUSIC_RGB_BAND_NUM; i++) {
				btmusic_rgb_disp[i] =
					(uint8_t)((uint16_t)btmusic_rgb_disp[i]
						  * BTMUSIC_RGB_SILENCE_FADE_DEN
						  / 256U);
				band_lvl[i] = btmusic_rgb_disp[i];
			}
			if ((btmusic_rgb_zero_log_cnt++ % 20) == 0) {
				SYS_LOG_INF("[rgb] silence fade raw_peak=%d\n",
					    (int)raw_peak);
			}
			btmusic_ws2812_show_spectrum_boosted(band_lvl,
							     BTMUSIC_RGB_BAND_NUM);
			return;
		}
	}

	btmusic_rgb_silence_cnt = 0;

	btmusic_rgb_zero_log_cnt = 0;

	memcpy(btmusic_rgb_hold_lvl, band_lvl, sizeof(btmusic_rgb_hold_lvl));
	btmusic_rgb_hold_valid = 1U;

	btmusic_ws2812_show_spectrum_boosted(band_lvl, BTMUSIC_RGB_BAND_NUM);

	if ((++btmusic_rgb_log_cnt % 10) == 0) {
		SYS_LOG_INF("[rgb] b0=%u b5=%u b9=%u R=%u G=%u B=%u L=%u src=%u\n",
			    band_lvl[0], band_lvl[5], band_lvl[9],
			    r, g, b,
			    (unsigned)(low_e + mid_e + high_e),
			    (unsigned)btmusic_rgb_energy_src
			    );
	}
}
#endif /* CONFIG_BT_MUSIC_RGB_RHYTHM_DEBUG */

/* 能量定时器回调：根据宏选择 RGB 律动调试或原厂 hex 打印 */
static void btmusic_display_freqpoint_energy(struct thread_timer *ttimer,
					     void *expiry_fn_arg)
{
#if CONFIG_BT_MUSIC_RGB_RHYTHM_DEBUG
	btmusic_rgb_rhythm_debug_poll(ttimer, expiry_fn_arg);
#else
	btmusic_energy_demo_raw_hex_dump(ttimer, expiry_fn_arg);
#endif
}

#endif /* CONFIG_BT_MUSIC_FREQPOINT_ENERGY_DEMO */

void btmusic_delay_start(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	if (!p_btmusic_app || p_btmusic_app->playback_player)
		return;

	SYS_LOG_INF("in");
	bt_manager_audio_stream_restore(BT_TYPE_BR);
}

#ifdef CONFIG_BTMUSIC_BMS_APP
int btmusic_bms_source_init(void)
{
	struct bt_broadcast_source_create_param* param;
	int ret;

	if (!p_btmusic_app)
		return -EINVAL;

	SYS_LOG_INF("%d", p_btmusic_app->bms_source);
	p_btmusic_app->bms_source = 1;

	if (thread_timer_is_running(&p_btmusic_app->broadcast_start_timer))
		thread_timer_stop(&p_btmusic_app->broadcast_start_timer);

	if (p_btmusic_app->broadcast_dev_handle) {
		SYS_LOG_WRN("already exist\n");
		p_btmusic_app->bms_source = 0;
		return -EINVAL;
	}

	param = broadcast_init_source_param();
	if(NULL == param) {
		p_btmusic_app->bms_source = 0;
		SYS_LOG_ERR("no source param\n");
		return -EINVAL;
	}

#if ENABLE_ENCRYPTION
	memcpy(p_btmusic_app->broadcast_code, param->broadcast_code, 16);
#endif

	param->qos = p_btmusic_app->qos;
	p_btmusic_app->irc = param->big_param->irc;
	ret = bt_manager_broadcast_source_create(param);
	broadcast_free_source_param(param);
	if (ret < 0) {
		p_btmusic_app->bms_source = 0;
		SYS_LOG_ERR("failed %d", ret);
		thread_timer_start(&p_btmusic_app->broadcast_start_timer, 300, 0);
		return ret;
	}

	p_btmusic_app->bms_transmit_num_index = BTMUSIC_BMS_TRANSMIT_INDEX;
	p_btmusic_app->broadcast_dev_handle = ret;
	SYS_LOG_INF("dev 0x%x\n", ret);

	return 0;
}

int btmusic_bms_source_exit(void)
{
	if (thread_timer_is_running(&p_btmusic_app->broadcast_start_timer))
		thread_timer_stop(&p_btmusic_app->broadcast_start_timer);

	if (p_btmusic_app->broadcast_dev_handle) {
		SYS_LOG_INF("0x%x\n", p_btmusic_app->broadcast_dev_handle);
		bt_manager_broadcast_source_disable(p_btmusic_app->broadcast_dev_handle);
		bt_manager_broadcast_source_release(p_btmusic_app->broadcast_dev_handle);
		p_btmusic_app->broadcast_dev_handle = 0;
	}

#if 1
    //bt_manager_pawr_adv_stop();
#endif
#ifdef CONFIG_BT_A2DP_LDAC
	bt_manager_a2dp_halt_ldac(false); // resume ldac
#endif

	p_btmusic_app->bms_source = 0;
	return 0;
}

int btmusic_get_auracast_mode(void)
{
	return system_app_get_auracast_mode();
}

void btmusic_set_auracast_mode(int mode)
{
	if (!p_btmusic_app)
		return;

	if(mode == btmusic_get_auracast_mode())
		return;

#ifdef CONFIG_BT_MUSIC_AUTO_SWITCH_BMR
	if (thread_timer_is_running(&p_btmusic_app->broadcast_switch_timer))
		thread_timer_stop(&p_btmusic_app->broadcast_switch_timer);
#endif

	SYS_EVENT_INF(EVENT_BTMUSIC_AURACAST_MODE, mode);
	if(!mode){
		if (1 == p_btmusic_app->bms_source) {
			btmusic_bms_source_exit();
		}

		//bt_manager_pawr_adv_stop();
		system_app_set_auracast_mode(0);
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		if (p_btmusic_app->bms_dvfs) {
			soc_dvfs_unset_level(p_btmusic_app->bms_dvfs, "bms_br");
			p_btmusic_app->bms_dvfs = 0;
		}
#endif
	}else{
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		if (0 == p_btmusic_app->bms_dvfs) {
			p_btmusic_app->bms_dvfs = BCST_FREQ;
			soc_dvfs_set_level(p_btmusic_app->bms_dvfs , "bms_br");
		}
#endif

		btmusic_bms_source_init();
		system_app_set_auracast_mode(1);
	}
}

#ifdef CONFIG_BT_MUSIC_AUTO_SWITCH_BMR
static void btmusic_auracast_switch_handler(struct thread_timer *ttimer,
				   void *expiry_fn_arg){
	if (NULL == p_btmusic_app) {
		return;
	}
	if(!p_btmusic_app->playing && btmusic_get_auracast_mode()){
		SYS_LOG_INF("switch to bmr\n");
		system_app_set_auracast_mode(2);
		system_app_launch_add(DESKTOP_PLUGIN_ID_BMR);
	}
}
#endif
#else

int btmusic_get_auracast_mode(void)
{
	return 0;
}
#endif

void btmusic_player_reset_trigger(void)
{
	struct app_msg msg = { 0 };

	if (NULL == p_btmusic_app) {
		return;
	}

	if (!p_btmusic_app->restart) {
		SYS_LOG_INF("restart\n");
		msg.type = MSG_BTMUSIC_APP_EVENT;
		msg.cmd = MSG_BTMUSIC_MESSAGE_CMD_PLAYER_RESET;
		send_async_msg(CONFIG_FRONT_APP_NAME, &msg);
		p_btmusic_app->restart = 1;
	}
}

#ifdef CONFIG_BTMUSIC_BMS_APP
static void btmusic_bms_start_handler(struct thread_timer *ttimer,
				   void *expiry_fn_arg){
	if (NULL == p_btmusic_app) {
		return;
	}

	SYS_LOG_INF("%d", system_app_get_auracast_mode());

	if(1 == btmusic_get_auracast_mode()){
		btmusic_bms_source_init();
	}
}
#endif

static int _btmusic_init(void *p1, void *p2, void *p3)
{
	int ret = 0;

	if (p_btmusic_app) {
		return 0;
	}

	SYS_LOG_INF("in");

	p_btmusic_app = app_mem_malloc(sizeof(struct btmusic_app_t));
	if (!p_btmusic_app) {
		SYS_LOG_ERR("malloc fail!\n");
		return -ENOMEM;
	}

	memset(p_btmusic_app, 0, sizeof(struct btmusic_app_t));
#ifdef CONFIG_BTMUSIC_BMS_APP
	p_btmusic_app->qos = &source_qos;
#endif

	btmusic_view_init();

#ifdef CONFIG_PLAYTTS
	if(tts_manager_is_playing()){
		p_btmusic_app->tts_playing = 1;
	}
#endif

	//todo
	bt_manager_stream_enable(STREAM_TYPE_A2DP, true);

	bt_manager_set_stream_type(AUDIO_STREAM_MUSIC);

#ifdef CONFIG_BT_MUSIC_FREQPOINT_ENERGY_DEMO
	/* 音乐律动：200ms 轮询，竖列 4 颗一组频谱 */
	thread_timer_init(&p_btmusic_app->energy_timer,
			  btmusic_display_freqpoint_energy, NULL);
#if CONFIG_BT_MUSIC_RGB_RHYTHM_DEBUG
	btmusic_rgb_rhythm_debug_reset();
#endif
#if defined(CONFIG_BT_MUSIC_LED_STRIP) || defined(CONFIG_BT_MUSIC_LED_STRIP2)
	btmusic_ws2812_init();
#if defined(CONFIG_BT_MUSIC_LED_RHYTHM)
	SYS_LOG_INF("[rgb] energy timer 100ms rhythm"
#else
	SYS_LOG_INF("[rgb] ws2812 init ambient"
#endif
#if defined(CONFIG_BT_MUSIC_LED_STRIP)
		    " strip1 gpio=%d cnt=%d"
#endif
#if defined(CONFIG_BT_MUSIC_LED_STRIP2)
		    " strip2 gpio=%d cnt=%d"
#endif
		    "\n"
#if defined(CONFIG_BT_MUSIC_LED_STRIP)
		    , CONFIG_BT_MUSIC_LED_STRIP_GPIO,
		    CONFIG_BT_MUSIC_LED_STRIP_COUNT
#endif
#if defined(CONFIG_BT_MUSIC_LED_STRIP2)
		    , CONFIG_BT_MUSIC_LED_STRIP2_GPIO,
		    CONFIG_BT_MUSIC_LED_STRIP2_COUNT
#endif
		    );
/* 本地律动灯 或 PY32 外发频谱：都要跑能量定时器 */
#if (defined(CONFIG_BT_MUSIC_LED_RHYTHM) || \
     defined(CONFIG_SYSTEM_APP_PY32_UART)) && !FAKE_RHYTHM_DEMO
#if CONFIG_BT_MUSIC_RGB_RHYTHM_DEBUG
	thread_timer_start(&p_btmusic_app->energy_timer, 50, 50);
	SYS_LOG_INF("[rgb] energy timer 50ms for py32/led rhythm\n");
#else
	thread_timer_start(&p_btmusic_app->energy_timer, 200, 500);
#endif
#endif
#else
#if (CONFIG_BT_MUSIC_RGB_RHYTHM_DEBUG || \
     defined(CONFIG_SYSTEM_APP_PY32_UART)) && !FAKE_RHYTHM_DEMO
	thread_timer_start(&p_btmusic_app->energy_timer, 50, 50);
	SYS_LOG_INF("[rgb] energy timer 50ms (no local strip)\n");
#else
	thread_timer_start(&p_btmusic_app->energy_timer, 200, 500);
#endif
#endif
#endif

	thread_timer_init(&p_btmusic_app->resume_timer, btmusic_delay_resume, NULL);
	thread_timer_init(&p_btmusic_app->play_timer, btmusic_delay_start, NULL);
#ifdef CONFIG_BTMUSIC_BMS_APP
	thread_timer_init(&p_btmusic_app->broadcast_start_timer, btmusic_bms_start_handler,
			  NULL);
#ifdef CONFIG_BT_MUSIC_AUTO_SWITCH_BMR
	thread_timer_init(&p_btmusic_app->broadcast_switch_timer, btmusic_auracast_switch_handler,
			  NULL);
#endif
#endif

#ifdef CONFIG_APP_TWS
	{
		if (app_tws_status_get_enable()) {
	#ifdef CONFIG_APP_TWS_SNOOP
			if(app_tws_status_get_role() == APP_TWS_ROLE_PRIMARY) {
				app_tws_on_source_switch(true);
			}
	#endif
		} else {
			if (bt_manager_is_tws_paired_valid() && bt_manager_is_auto_reconnect_runing()) {
				SYS_LOG_INF("tws auto reconnect running.");
			} else {
				/* 开机不自动进 TWS BIS：BIS 会 set_user_visual(disc=0)，手机搜不到经典蓝牙 */
				SYS_LOG_INF("skip auto tws bis on boot");
			}
		}
	}
#endif

#ifdef CONFIG_BTMUSIC_BMS_APP
	if (btmusic_get_auracast_mode()){
		system_app_set_auracast_mode(1);
#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
		if (0 == p_btmusic_app->bms_dvfs) {
			p_btmusic_app->bms_dvfs = BCST_FREQ;
			soc_dvfs_set_level(p_btmusic_app->bms_dvfs, "bms_br");
		}
#endif
		btmusic_bms_source_init();
	} else {
		thread_timer_start(&p_btmusic_app->play_timer, 800, 0);
	}
#else
	thread_timer_start(&p_btmusic_app->play_timer, 800, 0);
#endif

#ifdef CONFIG_TWS
#ifndef CONFIG_TWS_BACKGROUND_BT
	bt_manager_resume_phone();
#endif
#endif

	SYS_LOG_INF("out");
	return ret;
}

static int _btmusic_exit(void)
{
	if (!p_btmusic_app)
		goto exit;

#ifdef CONFIG_BT_MUSIC_FREQPOINT_ENERGY_DEMO
	if (thread_timer_is_running(&p_btmusic_app->energy_timer))
		thread_timer_stop(&p_btmusic_app->energy_timer);
#if defined(CONFIG_BT_MUSIC_LED_STRIP) || defined(CONFIG_BT_MUSIC_LED_STRIP2)
	btmusic_ws2812_deinit();
#endif
#endif

	if (thread_timer_is_running(&p_btmusic_app->resume_timer))
		thread_timer_stop(&p_btmusic_app->resume_timer);

	if (thread_timer_is_running(&p_btmusic_app->play_timer))
		thread_timer_stop(&p_btmusic_app->play_timer);

#ifdef CONFIG_BTMUSIC_BMS_APP
	if (thread_timer_is_running(&p_btmusic_app->broadcast_start_timer))
		thread_timer_stop(&p_btmusic_app->broadcast_start_timer);

#ifdef CONFIG_BT_MUSIC_AUTO_SWITCH_BMR
	if (thread_timer_is_running(&p_btmusic_app->broadcast_switch_timer))
		thread_timer_stop(&p_btmusic_app->broadcast_switch_timer);
#endif
#endif

#if ENABLE_PADV_APP
	padv_tx_deinit();
#endif

	bt_manager_stream_enable(STREAM_TYPE_A2DP, false);

	btmusic_stop_playback();

	btmusic_exit_playback();

	/* 兜底：确保退出 btmusic 后全局音频水线复位，不影响本地播放 */
	audio_policy_set_user_dynamic_waterlevel_ms(0);

#ifdef CONFIG_BTMUSIC_BMS_APP
	btmusic_bms_stop_capture();
	btmusic_bms_exit_capture();

	btmusic_bms_source_exit();
#endif

	btmusic_view_deinit();

#ifdef CONFIG_SOC_DVFS_DYNAMIC_LEVEL
	if(p_btmusic_app->set_dvfs_level){
		soc_dvfs_unset_level(p_btmusic_app->set_dvfs_level, "btmusic");
		p_btmusic_app->set_dvfs_level = 0;
	}
	if(p_btmusic_app->bms_dvfs){
		soc_dvfs_unset_level(p_btmusic_app->bms_dvfs, "bms_br");
		p_btmusic_app->bms_dvfs = 0;
	}
#endif

	app_mem_free(p_btmusic_app);
	p_btmusic_app = NULL;

#ifdef CONFIG_PROPERTY
	property_flush_req(NULL);
#endif

 exit:
	SYS_LOG_INF("exit finished\n");

	return 0;
}

static int btmusic_proc_msg(struct app_msg *msg)
{
	SYS_LOG_INF("type %d, cmd %d, value %d\n", msg->type,
		    msg->cmd, msg->value);
	switch (msg->type) {
	case MSG_EXIT_APP:
		_btmusic_exit();
		break;
	case MSG_BT_EVENT:
		btmusic_bt_event_proc(msg);
		break;
	case MSG_INPUT_EVENT:
		btmusic_input_event_proc(msg);
		break;
#ifdef CONFIG_PLAYTTS
	case MSG_TTS_EVENT:
		btmusic_tts_event_proc(msg);
		break;
#endif
	case MSG_BTMUSIC_APP_EVENT:
		btmusic_app_event_proc(msg);
		break;
	default:
		SYS_LOG_ERR("error: 0x%x!\n", msg->type);
		break;
	}
	return 0;
}

struct btmusic_app_t *btmusic_get_app(void)
{
	return p_btmusic_app;
}

static int btmusic_dump_app_state(void)
{
	print_buffer_lazy(APP_ID_BTMUSIC, (void *)btmusic_get_app(),
					  sizeof(struct btmusic_app_t));
	return 0;
}

DESKTOP_PLUGIN_DEFINE(DESKTOP_PLUGIN_ID_BR_MUSIC, _btmusic_init, _btmusic_exit, btmusic_proc_msg, \
	btmusic_dump_app_state, NULL, NULL, NULL);
