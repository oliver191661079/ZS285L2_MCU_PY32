/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file bluetooth tws observer interface
 */

#ifndef __BLUETOOTH_TWS_OBSERVER_H__
#define __BLUETOOTH_TWS_OBSERVER_H__

#include <btservice_api.h>
#include <audio_hal.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_SOC_SERIES_LARK
#define SDM_CNT_MASK  (0xFFFFFFF)
#else
#define SDM_CNT_MASK  (0xFFFFF)
#endif

//是否使用sdm cnt进行同步微调
#define CONFIG_SYNC_BY_SDM_CNT   (1)
#ifdef CONFIG_SYNC_BY_SDM_CNT
#define CONFIG_SOFT_SDM_CNT (1)
#endif

#ifdef CONFIG_SOFT_SDM_CNT
#define SOFT_SDM_OSR_SHIFT  (5)
#define SOFT_SDM_OSR  (1 << SOFT_SDM_OSR_SHIFT)
#endif

/*
 * enum audio_trigger_src
 * @brief The external sources of IRQ signals to trigger DAC/ADC digital start.
 */
typedef enum {
	TRIGGER_SRC_TIMER0 = 0,
	TRIGGER_SRC_TIMER1,
	TRIGGER_SRC_TIMER2,
	TRIGGER_SRC_TIMER3,
	TRIGGER_SRC_TIMER4,
	TRIGGER_SRC_TWS_IRQ0,
	TRIGGER_SRC_TWS_IRQ1,
} audio_trigger_src;

//b板lark
//#define CONFIG_SOC_SERIES_LARK_V2

typedef struct {
    uint16_t pkt_seq; /* 包号 */
    uint16_t pkt_len:12; /* 包的数据长度 */
    uint16_t frame_cnt:4;
    uint64_t pkt_bttime_us; /* 蓝牙时钟 us */
#ifdef CONFIG_SYNC_BY_SDM_CNT
    uint32_t sdm_cnt; /* 当前帧播放前的的sdm cnt */
#endif
    uint16_t samples; /* 当前帧pcm样点数 */
} __attribute__((packed)) tws_pkt_info_t;


typedef struct {
    int32_t (*set_start_pkt_seq)(void *handle, uint8_t start_decode, uint16_t first_seq);
    int32_t (*start_pcm_streaming)(void *handle, uint16_t playtime_us);
    int32_t (*start_playback)(void *handle);

    //level==0xFF: set default aps level
    int32_t (*set_base_aps_level)(void *handle, uint8_t level);
    int32_t (*notify_time_diff)(void *handle, int32_t diff_time);
    uint32_t (*get_sdm_cnt_saved)(void *handle);
    uint32_t (*get_sdm_cnt_realtime)(void *handle);
    int32_t (*get_sdm_cnt_delta_time)(void *handle, int32_t sdm_cnt);
    int32_t (*set_dac_trigger_src)(void *handle, audio_trigger_src src);
    int32_t (*set_dac_en)(void *handle);
} media_observer_t;

static inline int32_t media_set_start_pkt_seq(media_observer_t *handle, uint8_t start_decode, uint16_t first_seq)
{
    return handle->set_start_pkt_seq(handle, start_decode, first_seq);
}

static inline int32_t media_start_pcm_streaming(media_observer_t *handle, uint16_t playtime_us)
{
    return handle->start_pcm_streaming(handle, playtime_us);
}

static inline int32_t media_start_playback(media_observer_t *handle)
{
    return handle->start_playback(handle);
}

//level==0xFF: set default aps level
static inline int32_t media_set_base_aps_level(media_observer_t *handle, uint8_t level)
{
    return handle->set_base_aps_level(handle, level);
}

static inline int32_t media_notify_time_diff(media_observer_t *handle, int32_t diff_time)
{
    return handle->notify_time_diff(handle, diff_time);
}

static inline uint32_t media_get_sdm_cnt_saved(media_observer_t *handle)
{
    return handle->get_sdm_cnt_saved(handle);
}

static inline uint32_t media_get_sdm_cnt_realtime(media_observer_t *handle)
{
    return handle->get_sdm_cnt_realtime(handle);
}

static inline int32_t media_get_sdm_cnt_delta_time(media_observer_t *handle, int32_t sdm_cnt)
{
    return handle->get_sdm_cnt_delta_time(handle, sdm_cnt);
}

static inline int32_t media_set_dac_trigger_src(media_observer_t *handle, audio_trigger_src src)
{
    return handle->set_dac_trigger_src(handle, src);
}

static inline int32_t media_set_dac_en(media_observer_t *handle)
{
    return handle->set_dac_en(handle);
}

/*-------------------------------------------------------------------------------------------------------------------------------------*/
typedef struct {
    uint64_t (*get_play_time_us)(void *handle);
    uint64_t (*get_bt_clk_us)(void *handle);
    uint8_t (*get_role)(void *handle);
    int32_t (*set_stream_info)(void *handle, uint8_t format, uint8_t channels,
        uint16_t first_pktseq, uint32_t sample_rate,
        uint16_t cache_time, uint32_t pkt_time_us, uint64_t play_time);
    int32_t (*aps_change_request)(void *handle, uint8_t level);
    int32_t (*set_pkt_info)(void *handle, tws_pkt_info_t *info);
    int32_t (*trigger_restart)(void *handle, uint8_t reason);
    int32_t (*on_buffer_change)(void *handle, int32_t pcm_time);
    void (*deinit)(void *handle);
} bt_tws_observer_t;


static inline uint64_t bt_tws_get_play_time_us(bt_tws_observer_t *handle)
{
    if(handle->get_play_time_us) {
        return handle->get_play_time_us(handle);
    } else {
        return 0;
    }
}

static inline uint64_t bt_tws_get_bt_clk_us(bt_tws_observer_t *handle)
{
    if(handle->get_bt_clk_us) {
        return handle->get_bt_clk_us(handle);
    } else {
        return 0;
    }
}

static inline uint8_t bt_tws_get_role(bt_tws_observer_t *handle)
{
    if(handle->get_role) {
        return handle->get_role(handle);
    } else {
        return BTSRV_TWS_NONE;
    }
}

static inline int32_t bt_tws_set_stream_info(bt_tws_observer_t *handle,
    uint8_t format, uint8_t channels,
    uint16_t first_pktseq, uint32_t sample_rate,
    uint16_t cache_time, uint32_t pkt_time_us, uint64_t play_time)
{
    if(handle->set_stream_info) {
        return handle->set_stream_info(handle, format, channels, first_pktseq, sample_rate, cache_time, pkt_time_us, play_time);
    } else {
        return -1;
    }
}

static inline int32_t bt_tws_aps_change_request(bt_tws_observer_t *handle, uint8_t level)
{
    if(handle->aps_change_request) {
        return handle->aps_change_request(handle, level);
    } else {
        return -1;
    }
}

static inline int32_t bt_tws_set_pkt_info(bt_tws_observer_t *handle, tws_pkt_info_t *info)
{
    if(handle->set_pkt_info) {
        return handle->set_pkt_info(handle, info);
    } else {
        return -1;
    }
}

static inline int32_t bt_tws_trigger_restart(bt_tws_observer_t *handle, uint8_t reason)
{
    if(handle->trigger_restart) {
        return handle->trigger_restart(handle, reason);
    } else {
        return -1;
    }
}

static inline int32_t bt_tws_on_buffer_change(bt_tws_observer_t *handle, int32_t pcm_time)
{
    if(handle->on_buffer_change) {
        return handle->on_buffer_change(handle, pcm_time);
    } else {
        return -1;
    }
}

static inline void bt_tws_observer_deinit(bt_tws_observer_t *handle)
{
    return handle->deinit(handle);
}

void bt_tws_observer_pre_init(void *btsrv_observer);
bt_tws_observer_t* bt_tws_observer_init(media_observer_t *media_observer, void *btsrv_observer);
bt_tws_observer_t* simple_tws_observer_init(media_observer_t *media_observer, void *btsrv_observer);


typedef void (*tws_send_user_type_cb)(void *param);
// size no more than 16bytes data
int32_t  tws_send_pkt_user_type(void *data, u32_t size);
uint32_t tws_send_pkt_user_cb_set(tws_send_user_type_cb user_cb);

static inline int abs(int i)
{
	return (i >= 0) ? i : -i;
}

//0xFFFFFFF*312.5us + 312.5us
#define BT_CLK_MAX_US  (uint64_t)(83886080000ll)
#define BT_CLK_MAX_MS  (BT_CLK_MAX_US / 1000)

static inline uint64_t bt_manager_bt_clk_fix_us(uint64_t bt_clk)
{
    if(bt_clk >= BT_CLK_MAX_US) {
        if(bt_clk < (BT_CLK_MAX_US*2)) {
            bt_clk -= BT_CLK_MAX_US;
        } else {
            bt_clk = BT_CLK_MAX_US - (UINT64_MAX - bt_clk);
        }
    }
    return bt_clk;
}

static inline int32_t bt_manager_bt_clk_diff_us(uint64_t bt_clk1, uint64_t bt_clk2)
{
    int64_t diff = bt_clk1 - bt_clk2;

    if(diff > (BT_CLK_MAX_US/2)) {
        diff = bt_clk2 + BT_CLK_MAX_US - bt_clk1;
    } else if(diff < (-(int64_t)BT_CLK_MAX_US/2)) {
        diff = bt_clk1 + BT_CLK_MAX_US - bt_clk2;
    }

    return (int32_t)diff;
}

#ifdef __cplusplus
}
#endif

#endif  //END OF __BLUETOOTH_TWS_OBSERVER_H__
