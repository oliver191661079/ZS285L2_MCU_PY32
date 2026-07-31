/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file bluetooth tws observer interface
 */

#include "bluetooth_tws_observer.h"
#include "media_type.h"
#include "bt_manager.h"

#include <logging/sys_log.h>

//tws中断启动最小间隔,us
#define TWS_MIN_INTERVAL     (22000)
//正式播放前提前送pcm数据时间,us
#define PRE_PCM_STREAMING_TIME     (10000)

typedef enum {
    PLAYER_TWS_STATUS_NULL,
    PLAYER_TWS_STATUS_WAIT,     /* 执行wait play后的状态 */
    PLAYER_TWS_STATUS_PCM_STREAMING,/* 开始传输pcm数据到dac */
    PLAYER_TWS_STATUS_PLAY,     /* 真正播放数据的状态 */
} player_tws_status_e;

typedef struct {
    bt_tws_observer_t tws_observer;
    media_observer_t *media_observer;
    void *btsrv_observer;
    os_timer deal_wait_timer;
    uint32_t pcm_streaming_timeout;
    uint8_t tws_role;
    uint8_t tws_status;

#if 0
    uint32_t sdm_cnt_saved;
    uint64_t irq_clk;
#endif
    uint64_t splay_local_time; /* 近端开始播放的时间 */
} simple_tws_observer_inner_t;

static simple_tws_observer_inner_t observer_inner = {0};

/*----------------------------------------------------------------------------------------------------*/

//callback from irq
static void _tws_irq_handle(uint64_t bt_clk_us)
{
    simple_tws_observer_inner_t *p = &observer_inner;

    if(p->tws_status == PLAYER_TWS_STATUS_NULL) {
        SYS_LOG_ERR("status invalid");
        return;
    }

#if 0 //def CONFIG_SOC_SERIES_LARK
    if(0 != bt_clk_us) {
        p->irq_clk = bt_clk_us;
        p->sdm_cnt_saved = media_get_sdm_cnt_saved(p->media_observer);
    }
#endif

    if(p->tws_status == PLAYER_TWS_STATUS_PCM_STREAMING) {
        p->tws_status = PLAYER_TWS_STATUS_PLAY;
        media_start_playback(p->media_observer);
        SYS_LOG_INF("start playback, tws clk: %u", (uint32_t)(bt_clk_us & 0xffffffff));
        return;
    }

    if(p->tws_status == PLAYER_TWS_STATUS_WAIT) {
        SYS_LOG_ERR("tws irq no handle, status: %d, irq clk: %u",
            p->tws_status, (uint32_t)(bt_clk_us & 0xffffffff));
    }
}

static void _deal_wait_timer_proc(os_timer *ttimer)
{
    simple_tws_observer_inner_t *p = &observer_inner;

    if(p->tws_status == PLAYER_TWS_STATUS_WAIT) {
        SYS_LOG_INF("stream");
        p->tws_status = PLAYER_TWS_STATUS_PCM_STREAMING;
#ifdef CONFIG_SOC_SERIES_LARK
        //dac_en由tws中断触发使能，太早送数据会导致中间件线程卡住比较早时间，
        //因此这里只提前10毫秒先DAC送数据
        media_set_dac_trigger_src(p->media_observer, TRIGGER_SRC_TWS_IRQ0);
#endif
        media_start_pcm_streaming(p->media_observer, 0);

        if(p->pcm_streaming_timeout) {
            os_timer_start(&p->deal_wait_timer, K_MSEC((p->pcm_streaming_timeout + 2000)/1000), K_MSEC(0));
        } else {
            _deal_wait_timer_proc(NULL);
        }
    } else if(p->tws_status == PLAYER_TWS_STATUS_PCM_STREAMING) {
        if((p->tws_role != BTSRV_TWS_NONE) && (p->splay_local_time != UINT64_MAX)) {
            SYS_LOG_ERR("tws irq no trigger");
        }

#ifdef CONFIG_SOC_SERIES_LARK
        media_set_dac_en(p->media_observer);
#endif
        _tws_irq_handle(0);
    }
}

static uint64_t _bt_tws_get_play_time_us(void *handle)
{
    simple_tws_observer_inner_t *p = (simple_tws_observer_inner_t*)handle;
    return p->splay_local_time;
}

static uint64_t _bt_tws_get_bt_clk_us(void *handle)
{
    simple_tws_observer_inner_t *p = (simple_tws_observer_inner_t*)handle;
    tws_runtime_observer_t *observer = (tws_runtime_observer_t *)p->btsrv_observer;

#if 0 //def CONFIG_SOC_SERIES_LARK
    if(p->irq_clk != 0) {
        uint32_t sdm_cnt = media_get_sdm_cnt_realtime(p->media_observer);
        return p->irq_clk
            + media_get_sdm_cnt_delta_time(p->media_observer, (int32_t)(sdm_cnt - p->sdm_cnt_saved));
    }
#endif

    return observer->get_bt_clk_us();
}

static uint8_t _bt_tws_get_role(void *handle)
{
    simple_tws_observer_inner_t *p = (simple_tws_observer_inner_t*)handle;

    return p->tws_role;
}

static int32_t _bt_tws_set_stream_info(void *handle,
    uint8_t format, uint8_t channels, uint16_t first_pktseq, uint32_t sample_rate,
    uint16_t cache_time, uint32_t pkt_time_us, uint64_t play_time)
{
    simple_tws_observer_inner_t *p = (simple_tws_observer_inner_t*)handle;
    tws_runtime_observer_t *observer = (tws_runtime_observer_t *)p->btsrv_observer;
    uint64_t cur_time;
    int32_t hr_timeout;

    p->tws_status = PLAYER_TWS_STATUS_WAIT;
    cur_time = observer->get_bt_clk_us();
    p->tws_role = observer->get_role();

    hr_timeout = bt_manager_bt_clk_diff_us(play_time, cur_time);
    if((hr_timeout > 0) && (cur_time != UINT64_MAX)) {
        p->splay_local_time = play_time;
    }

    SYS_LOG_INF("timeout: %d, role: %d, play: %u, cur: %u\n",
        hr_timeout, p->tws_role, (uint32_t)play_time, (uint32_t)cur_time);
    if((hr_timeout > 3000000) || (hr_timeout < 0)) {
        hr_timeout = 0;
    }

    if(hr_timeout < 2000) {
        p->pcm_streaming_timeout = 0;
        _deal_wait_timer_proc(NULL);
    } else if(hr_timeout < TWS_MIN_INTERVAL) {
        p->pcm_streaming_timeout = hr_timeout - 2000;
        _deal_wait_timer_proc(NULL);
    } else {
        observer->set_interrupt_time(play_time);
        //p->pcm_streaming_timeout = PRE_PCM_STREAMING_TIME;
        //os_timer_start(&p->deal_wait_timer, K_USEC(hr_timeout - PRE_PCM_STREAMING_TIME), K_USEC(0));
        p->pcm_streaming_timeout = hr_timeout;
        _deal_wait_timer_proc(NULL);
    }

    return 0;
}

static void _bt_tws_observer_deinit(void *handle)
{
    simple_tws_observer_inner_t *p = (simple_tws_observer_inner_t*)handle;
    tws_runtime_observer_t *observer;
    uint32_t flags;

    if(NULL == p->media_observer)
        return;

    flags = irq_lock();
    p->tws_status = PLAYER_TWS_STATUS_NULL;
    irq_unlock(flags);

    os_timer_stop(&p->deal_wait_timer);

    observer = (tws_runtime_observer_t *)p->btsrv_observer;
    observer->set_interrupt_time(0);
    observer->set_interrupt_cb(NULL);
}

bt_tws_observer_t* simple_tws_observer_init(media_observer_t *media_observer, void *btsrv_observer)
{
    simple_tws_observer_inner_t *p = &observer_inner;
    tws_runtime_observer_t *observer = (tws_runtime_observer_t *)btsrv_observer;

    memset(p, 0, sizeof(simple_tws_observer_inner_t));
    p->media_observer = media_observer;
    p->btsrv_observer = btsrv_observer;
    p->tws_observer.get_play_time_us = _bt_tws_get_play_time_us;
    p->tws_observer.get_bt_clk_us = _bt_tws_get_bt_clk_us;
    p->tws_observer.get_role = _bt_tws_get_role;
    p->tws_observer.set_stream_info = _bt_tws_set_stream_info;
    p->tws_observer.deinit = _bt_tws_observer_deinit;

    p->tws_status = PLAYER_TWS_STATUS_NULL;
    p->tws_role = BTSRV_TWS_NONE;
    p->splay_local_time = UINT64_MAX;
    os_timer_init(&p->deal_wait_timer, _deal_wait_timer_proc, NULL);

    observer->set_interrupt_cb(_tws_irq_handle);

    return &p->tws_observer;
}

