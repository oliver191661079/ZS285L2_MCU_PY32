/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file bluetooth tws observer interface
 */

#include "bluetooth_tws_observer.h"
#include "acts_ringbuf.h"
#include "media_type.h"
#include "bt_manager.h"
#include "audio_policy.h"
#include "audio_system.h"
// #include "audiolcy_common.h"
#include "stdlib.h"
// #include "soc_regs.h"

#define SYS_LOG_NO_NEWLINE
#ifdef SYS_LOG_DOMAIN
#undef SYS_LOG_DOMAIN
#endif
#define SYS_LOG_DOMAIN "bt_tws"
#include <logging/sys_log.h>



#define PKT_SEQ_BIGGER(seq0, seq1)  ((seq0 != seq1) && ((seq0 - seq1) < 0x7FFFu))
#define PKT_SEQ_NO_SMALL(seq0, seq1)  ((seq0 - seq1) < 0x7FFFu)

/*
1、如果两边都处于等待播放状态，设备收到对方的splay后，
   若收到的对方的起始播放时钟小于当前bt时钟，则做出错处理
   包号相同，则用时间较晚的中断时钟作为开始播放时间，重新设置tws中断时间
   若包号不同，则以包号大的作为起始播放包号和时钟，设置为PLAYER_TWS_USE_LOCAL_SPLAY模式，丢弃包号小的码流包
2、如果当前设备已经开始播放，收到到对方的splay包，
   则从当前缓存的码流包中，挑选一个比较靠后的包，计算播放时间，给对方发一个PLAYER_TWS_USE_LOCAL_SPLAY
   若码流缓存信息不足，则向对方发送PLAYER_TWS_NEED_NEW_SPLAY，对方收到消息后，重新发送splay过来

不能在中断、关中断、锁调度情况下调用send_pkt/set_interrupt_time/get_bt_clk_us

关于包号：
1、播歌包号，码流中自带包号，因此双耳包号是一致的，不用处理；
2、通话包号，耳机收到的第一个包号为1，依次累加，蓝牙模块会记录收到第一个包时的蓝牙时钟
   双耳的(时钟差/24)即包号差
   首包时钟比较大的一端，对包号进行修正（统一包号）

   PLAYER_TWS_NEED_NEW_SPLAY 消息：包号无意义
   PLAYER_TWS_USE_LOCAL_SPLAY 消息：统一包号
   PLAYER_TWS_USE_START_SPLAY 消息：原始包号，因为是第一个协商包，未获取到对端时钟，无法对包号进行修复，因此收到该消息后，需对包号进行修复
   TWS_SYNCINFO_PKT 同步信息包：统一包号

   计算包号差为什么除以24：
   蓝牙clk数据结构如下：
   typedef struct
   {
       uint32_t bt_clk;  // by 312.5us
       uint16_t bt_intraslot;  // by 1us
   } player_tws_bttime_pkt_t;
   首包时钟为上面数据结构中的bt_clk字段，通话一个包7.5毫秒
   7500 / 312.5 = 24

*/

//tws中断启动最小间隔,us
#define TWS_MIN_INTERVAL     (30000)
//正式播放前提前送pcm数据时间,确保pcm数据已经准备好,us
#define PRE_PCM_STREAMING_TIME     (30000)
//tws同步时钟间隔,us
#define TWS_CLK_SYNC_INTERVAL     (200000)
//tws同步包发送间隔,百毫秒，必须为TWS_CLK_SYNC_INTERVAL的倍数
#define TWS_ALIGN_INTERVAL        (2)
//有对箱接入时，禁止调节aps时间,us
#define APS_FORBIT_TIME       (1500000)
//SPLAY发送时间间隔,us
#define SEND_SPLAY_INTERVAL   (100000)
//离开始播放最小时间间隔,us
#define SPLAY_MIN_WAIT_TIME   (50000)


/* splay_flag的说明
 * 1: 本端处于PLAYING状态, 暂时只支持对端处于WAIT PLAY状态, 希望对端重新发一次splay包过来, 因为本端缓存的包里面没有对端的包号
 * 2: 本端处于PLAYING状态, 希望对端以本端的包号和时间来播放
 * 3: 本端处于WAIT PLAY状态, 若对端不是PLAYING, 希望对端考虑是否接受本端的包号和时间, 若对端是PLAYING, 希望对端提供可以播放的包号和时间
 */
#define PLAYER_TWS_NEED_NEW_SPLAY    (1)
#define PLAYER_TWS_USE_LOCAL_SPLAY   (2)
#define PLAYER_TWS_USE_START_SPLAY   (3)

#define PLAYER_TWS_SYNCITEM_MAXNUM      (10)      /* TWS同步信息包最大缓存个数 */
#define PLAYER_TWS_ERRCOUNT_MAXNUM      (10)      /* TWS失去同步状态的超时时间 */
#define PLAYER_TWS_DATA_MAXNUM      (5)      /* 暂存从对端接收到的消息个数 */
#define PLAYER_TWS_MSG_MAXNUM      (30)      /* 暂存从中间件api接收到的消息个数 */
#define STREAM_PKT_MAXNUM      (30)      /* 已收到的码流包信息 */
#define BT_TWS_LOOP_INTERVAL     (2)


typedef enum {
    TWS_STARTPLAY_PKT = 1,       /* 启动播放包 */
    TWS_STARTSTOP_PKT,       /* 停止播放包 */
    TWS_SYNCINFO_PKT,    /* 同步信息包 */
    TWS_SDM_INFO_PKT,    /* 同步信息包 */
    TWS_PLAYRATE_PKT,        /* 速度调节包 */
    TWS_INTTIME_PKT,         /* 中断时间包 */
    TWS_STREAM_INIT_PKT,

    TWS_USER_TYPE_PKT = 0x10,   /* 用户自定义包 */
} player_tws_type_e;

/* TWS数据包, 需和tws_sync_cmd_t保持一致
 */
typedef struct {
    uint8_t nego_seq;  /* 参考 player_tws_group_e */
    uint8_t type;   /* 参考 player_tws_type_e */
    uint8_t format; /* 音频格式, 避免不同场景的数据包误判 */
    uint8_t payload_len;
    uint8_t payload_data[16]; /* buf需要4字节对齐, 避免直接赋值导致死机 */
} player_tws_pkt_t;

typedef struct {
    uint64_t splay_time; /* 开始播放的时钟 us */
    uint32_t first_clk; /* mSBC,CVSD: 第一个数据包的接收时钟; SBC,AAC: 本端起始播放反推计算使用包时长(避免sbc不固定) */
    uint16_t pkt_seq; /* 开始播放的首包号 */
    uint8_t  splay_flag; /* 本端发送SPLAY的时候告知对端的操作类型, PLAYER_TWS_NEED_NEW_SPLAY... */
    int8_t   reverse_pkts;  /* 本端splay_time是指pkt_seq + reverse_pkts的预计播放时钟 */
} __attribute__((packed)) player_tws_startplay_pkt_t;

typedef struct {
    uint64_t startstop_time; /* 开始播放的时钟 us */
    uint8_t reserve[8];
} __attribute__((packed)) player_tws_startstop_pkt_t;

#ifdef CONFIG_SYNC_BY_SDM_CNT
typedef struct {
    uint32_t sdm_cnt_save; /* tws中断锁存的sdm cnt */
    uint32_t sdm_cnt[2]; /* 当前包和上一包播放前的的sdm cnt */
    uint16_t pkt_seq; /* 表示播放的数据包号 */
    uint16_t bttime_ms; /* 表示播放对应包号时对应的蓝牙时钟,只是用于对比是否为同一个中断，不用高精度 */
} __attribute__((packed)) player_tws_sdm_info_pkt_t;
#else
typedef struct {
    uint64_t  bttime_us; /* 表示播放对应包号时对应的蓝牙时钟 us */
    uint16_t  pkt_seq; /* 表示播放的数据包号 */
    uint8_t   reserve[6];
} __attribute__((packed)) player_tws_syncinfo_pkt_t;
#endif

typedef struct {
    uint64_t inttime;  /* 中断时间 us */
    uint8_t  aps_level;
    uint8_t  reserve[7];
} __attribute__((packed)) player_tws_playrate_pkt_t;

typedef enum {
    PLAYER_TWS_STATUS_NULL,
    PLAYER_TWS_STATUS_STREAM_ABNORMAL,
    PLAYER_TWS_STATUS_INIT,     /* 执行init后的状态 */
    PLAYER_TWS_STATUS_WAIT,     /* 执行wait play后的状态 */
    PLAYER_TWS_STATUS_COMFIRM_SEQNO,/* 确认起始seqno */
    PLAYER_TWS_STATUS_START_DECODE,/* 开始解码 */
    PLAYER_TWS_STATUS_PCM_STREAMING,/* 开始传输pcm数据到dac */
    PLAYER_TWS_STATUS_PLAY,     /* 真正播放数据的状态 */
} player_tws_status_e;

typedef struct {
    uint16_t pkt_seq; /* 包号 */
    uint16_t pkt_len; /* 包的数据长度 */
    uint16_t samples; /* 样点数 */
    uint8_t  reserve[2];
    uint64_t pkt_bttime_us; /* 蓝牙时钟 us */
#ifdef CONFIG_SYNC_BY_SDM_CNT
    uint32_t sdm_cnt; /* 当前帧播放前的的sdm cnt */
#endif
} player_tws_pinfo_unit_t;

/* TWS消息
 */
typedef struct {
    uint8_t type;   /* 参考 player_tws_type_e */
    uint8_t reserve;
#ifdef CONFIG_SYNC_BY_SDM_CNT
    uint8_t data[18];
#else
    uint8_t data[14];
#endif
} __attribute__((packed)) player_tws_msg_t;

typedef struct {
    uint16_t pkt_seq; /* 包号 */
    uint16_t pkt_len:12; /* 包的数据长度 */
    uint16_t frame_cnt:4;
} __attribute__((packed)) stream_pkt_t;

typedef struct {
    bt_tws_observer_t tws_observer;
    media_observer_t *media_observer;
    void *btsrv_observer;
    os_delayed_work loop_work;
    os_timer deal_wait_timer;
    uint16_t pcm_streaming_timeout;
    uint16_t restarting:1;
    uint16_t last_sinfo_flag:1; /* 上一个包是否为同步包 */
    uint16_t forbid_aps:1;
    uint16_t first_pkt_flag:2; /* 开始播放才开始接收包信息，故首包数据可能不完整，丢弃 */
    uint16_t sdm_diff_valid:1;
    uint16_t channels:2;
    uint16_t tws_role:3;
    uint16_t peried_irq_set:1;
    uint16_t tws1_info_valid:1;
    uint16_t reserve:3;

    int8_t   splay_reverse_pkts;    /* splay通信包中splay_time是推迟此包数后的时钟,可逆推splay_pktseq对应的时钟 */
    uint8_t  splay_reverse_timeout; /* 通信超时, 需要更新splay通信包的信息     */

    uint8_t format;/* media_type_e */
    uint8_t sync_error_count; /* 同步错误计数 */
    uint32_t sample_rate:18;  //hz
    uint32_t frame_samples:14; /* 每帧样点数 */
    uint16_t splay_pktseq; /* 起始播放包号 */
    uint16_t first_stream_pkt; /* 码流起始包号 */
    uint16_t first_play_pktseq; /* 开始播放后收到的第一个包信息的包号 */
    uint16_t remote_splay_seq; /* 让远端播放的首包号 */
    uint16_t diff_pkt;/* 本地和远端包号差 */
    uint16_t cache_time;
    uint32_t first_clk;/* 通话首包时钟 */
    uint32_t first_pkt_cyc;/* 首包接收到的本地时间 */
    uint32_t pkt_time_us;/* 每包时长，sbc可能不准确 */
    uint32_t buffer_time_cyc;/* 码流缓冲时间 */
    uint16_t frame_cnt_head;
    uint16_t frame_cnt_tail;
    uint32_t splay_aps_cyc;/* 上次发splay的本地时间，或禁止调aps起始时间 */
    uint64_t splay_local_time; /* 近端开始播放的时间 */
    uint32_t nego_start_time; /* 近端开始tws协商的时间 */

    //以下信息会被中断函数访问
    uint8_t tws_status;
    int8_t aps_level_pending;   /* 即将要设置的aps等级 */
    int8_t aps_level_request;
    uint64_t aps_inttime;

    //以下信息会被多线程访问
    struct acts_ringbuf tws_data_buffer;/* 远端耳机发过来的消息 */
    player_tws_pkt_t tws_data[PLAYER_TWS_DATA_MAXNUM];

#ifdef CONFIG_SYNC_BY_SDM_CNT
    player_tws_sdm_info_pkt_t sdm_info;
    int32_t sdm_diff;

    uint32_t sdm_cnt_saved;
    int32_t sdm_valid;
    uint64_t irq_clk;
    uint64_t irq_clk_next;
#ifdef CONFIG_SOFT_SDM_CNT
    uint32_t sample_time_ns;
    uint32_t tws1_cyc_diff;
    uint32_t buf_cnt_ori;
    uint64_t tws1_irq_clk;

#ifdef TWS1_TEST
    //for test
    uint32_t tws1_cyc_start;
    uint32_t irq_time_last;
#endif
#endif
#else
    int8_t adjust_shake_flag;   /* 防抖，master和slave前后两个包的时间差一致 */
    struct acts_ringbuf sinfo_master_buffer;/* 远端耳机同步信息包 */
    player_tws_syncinfo_pkt_t sinfo_master[PLAYER_TWS_SYNCITEM_MAXNUM];

    struct acts_ringbuf sinfo_slave_buffer;/* 本地同步信息包 */
    player_tws_syncinfo_pkt_t sinfo_slave[PLAYER_TWS_SYNCITEM_MAXNUM];
#endif

    struct acts_ringbuf msg_buffer;/* 本地给_bt_tws_loop线程发送的消息 */
    player_tws_msg_t msg_data[PLAYER_TWS_MSG_MAXNUM];

    struct acts_ringbuf stream_pkt_buffer;/* 本地码流包信息，用于计算sbc起始播放时间 */
    stream_pkt_t stream_pkt_data[STREAM_PKT_MAXNUM];

    /* 包信息单元
     */
    player_tws_pinfo_unit_t pinfo_unit[2];
    uint8_t pinfo_unit_idx;
    uint8_t save_pinfo_count; /* 包信息保存计数 */
    int16_t pinfo_seq_pendding; /* 上次清除包信息后, 不再保存同包号信息 */

    uint16_t Sync_interval;/* 同步到发送间隔 */
    uint32_t WPlay_Mintime;/* 最小协商等待时间 */
    uint32_t WPlay_Maxtime;/* 最大协商等待时间 */

    tws_send_user_type_cb tws_send_user_cb;
} bt_tws_observer_inner_t;

__in_section_unique(BT_TWS_OBSERVER_BSS) static bt_tws_observer_inner_t observer_inner = {0};
static uint8_t nego_seq = 0;
// static uint32_t tws1_t2_cnt = 0;
static uint64_t tws1_bt_clk = 0;

//#define TWS1_TEST
//#define TWS1_TEST2

#ifdef TWS1_TEST2
static os_delayed_work tws1_test_work;
static uint8_t tws1_test_clk_valid = 0;
static uint8_t tws1_test_clk_bak_cnt = 0;
static uint8_t tws1_test_work_cnt = 0;
static uint64_t tws1_test_clk_bak[10];
#endif

/*----------------------------------------------------------------------------------------------------*/
//将本地包号转换为统一包号
static inline uint16_t fix_pkt_seq(bt_tws_observer_inner_t *p, uint16_t pkt_seq)
{
    return pkt_seq + p->diff_pkt;
}

//将统一包号转换为本地包号
static inline uint16_t to_local_pkt_seq(bt_tws_observer_inner_t *p, uint16_t pkt_seq)
{
    return pkt_seq - p->diff_pkt;
}

static inline int32_t cyc_to_us(int32_t cyc)
{
    // if(cyc >= 0)
    //     return k_cyc_to_us_floor32(cyc);
    // else
    //     return -k_cyc_to_us_floor32(-cyc);
    return cyc/24;
}

static inline uint32_t k_us_to_cyc_floor32(uint32_t t)
{
    return t * 24;
}

static inline uint32_t k_cyc_to_ns_floor32(uint32_t t)
{
    return t * 1000 / 24;
}

static int32_t first_clk_diff(uint32_t clk1, uint32_t clk2)
{
    int32_t diff = clk1 - clk2;

    if((diff > (0x10000000/2)) && ((diff % 24) != 0)) {
        diff = clk2 + 0x10000000 - clk1;
    } else if((diff < (-0x10000000/2)) && ((diff % 24) != 0)) {
        diff = clk1 + 0x10000000 - clk2;
    }

    return (int32_t)diff;
}

static int32_t _cac_wait_time(bt_tws_observer_inner_t *p, int32_t min_wait_time, u8_t do_drop)
{
    int32_t wait_time;
    uint8_t diff_pkts = 0;

    wait_time = p->cache_time * 1000 - cyc_to_us(k_cycle_get_32() - p->first_pkt_cyc);

    // TWS device, ensure that there is min_wait_time for splay communication
    //if (wait_time < min_wait_time) { // single device in LLM, wait for min_wait_time to check a2dp data rate
    if (wait_time < min_wait_time && p->tws_role != BTSRV_TWS_NONE) {
        diff_pkts = (min_wait_time - wait_time) / p->pkt_time_us;
        //do_drop = 1;
    }

    //SYS_LOG_INF("1: %d_%d, %d_%d, %d", p->splay_pktseq, wait_time, p->cache_time, min_wait_time/1000, diff_pkts);

    if (diff_pkts > 0) {
        // drop the data right now
        if (do_drop > 0) {
            p->splay_reverse_pkts = 0;
            p->splay_reverse_timeout = 0;
            p->splay_pktseq  += diff_pkts;
            p->first_pkt_cyc += k_us_to_cyc_floor32(p->pkt_time_us * diff_pkts);

            wait_time = p->cache_time * 1000 - cyc_to_us(k_cycle_get_32() - p->first_pkt_cyc);
            //SYS_LOG_INF("2: %d_%d", p->splay_pktseq, wait_time);
        // the data would be drop after tws splay communication or timeout
        } else {
            p->splay_reverse_timeout = 0;
            p->splay_reverse_pkts = diff_pkts;
            wait_time += diff_pkts * p->pkt_time_us;

            //SYS_LOG_INF("3: %d_%d, dbg:%d", p->splay_pktseq, wait_time, p->splay_reverse_pkts);
        }
    }

    // {
    //     uint32_t fixed_cost = 0;
    //     audiolcy_cmd_entry(LCYCMD_GET_FIXED, (void *)&fixed_cost, 4);
    //     wait_time -= fixed_cost * 1000000ll / p->sample_rate;  // samples to us
    //     //SYS_LOG_INF("4: %d_%d", wait_time, fixed_cost);
    // }

    // first_stream_pkt and splay->pktseq will be corrected when receive splay info of the other
    p->first_stream_pkt = p->splay_pktseq;
    media_set_start_pkt_seq(p->media_observer, 0, p->splay_pktseq);
    return wait_time;
}

static int32_t _cac_pkt_frames(bt_tws_observer_inner_t *p, uint16_t start_seq, uint16_t *dst_seq, uint16_t count)
{
    uint16_t new_pkt_seq = start_seq;
    int32_t frame_count = 0;

    if(p->format == SBC_TYPE || p->format == LDAC_TYPE) {
        struct acts_ringbuf stream_pkt_buffer_bak;
        stream_pkt_t stream_pkt;
        uint16_t diff_pkt;

        memcpy(&stream_pkt_buffer_bak, &p->stream_pkt_buffer, sizeof(struct acts_ringbuf));

        do {
            if(acts_ringbuf_length(&p->stream_pkt_buffer) < sizeof(stream_pkt_t)) {
                memcpy(&p->stream_pkt_buffer, &stream_pkt_buffer_bak, sizeof(struct acts_ringbuf));
                return -1;
            }

            acts_ringbuf_get(&p->stream_pkt_buffer, &stream_pkt, sizeof(stream_pkt));
            if(start_seq == stream_pkt.pkt_seq)
                break;
        }while(1);

        if(PKT_SEQ_BIGGER(*dst_seq, new_pkt_seq) || (count > 0)) {
            do {
                //printk("sbc: %u, %u, %u\n",
                //    stream_pkt.pkt_seq, stream_pkt.pkt_len, stream_pkt.frame_cnt);
                new_pkt_seq++;
                frame_count += stream_pkt.frame_cnt;

                diff_pkt = new_pkt_seq - start_seq;
                if((PKT_SEQ_NO_SMALL(new_pkt_seq, *dst_seq)) && (diff_pkt >= count))
                    break;

                if(acts_ringbuf_length(&p->stream_pkt_buffer) < sizeof(stream_pkt_t))
                    break;

                if(new_pkt_seq == (uint16_t)(stream_pkt.pkt_seq + 1)) {
                    acts_ringbuf_get(&p->stream_pkt_buffer, &stream_pkt, sizeof(stream_pkt));
                } else {
                    printk("w: pkt miss, %u, %u\n", new_pkt_seq, stream_pkt.pkt_seq);
                }
            }while(1);
        }

        if(PKT_SEQ_BIGGER(*dst_seq, new_pkt_seq)) {
            printk("seq %u no receved, cur: %u\n", *dst_seq, new_pkt_seq);
            memcpy(&p->stream_pkt_buffer, &stream_pkt_buffer_bak, sizeof(struct acts_ringbuf));
            return -1;
        }

        *dst_seq = new_pkt_seq;
        memcpy(&p->stream_pkt_buffer, &stream_pkt_buffer_bak, sizeof(struct acts_ringbuf));
    } else {
        new_pkt_seq = start_seq + count;
        if(PKT_SEQ_BIGGER(new_pkt_seq, *dst_seq))
            *dst_seq = new_pkt_seq;

        frame_count = *dst_seq - start_seq;
    }

    return frame_count;
}

#ifdef CONFIG_SOFT_SDM_CNT
//callback from irq every 200ms
//all code callback from tws1 irq should be place in ram
//tws1 irq, highest priority, no be block by irq_lock
__ramfunc static void _tws1_irq_handle(uint64_t bt_clk_us)
{
    bt_tws_observer_inner_t *p = &observer_inner;
    int32_t i = 0;
    uint32_t buf_cnt = 0;
    uint32_t cyc_start = 0;
    uint32_t cyc_end = 0;

#if defined(CONFIG_AUDIO_OUT_I2STX0_SUPPORT) && !defined(CONFIG_AUDIO_OUTPUT_DAC_AND_I2S)
	volatile uint32_t *pcm_buf_cnt = (uint32_t*)(AUDIO_I2STX0_REG_BASE + 0x20);//i2s tx0
#else
	volatile uint32_t *pcm_buf_cnt = (uint32_t*)(AUDIO_DAC_REG_BASE + 0x28);//dac0
#endif

    // tws1_t2_cnt = *timer2_cnt;
    tws1_bt_clk = bt_clk_us;
#ifdef TWS1_TEST2
    tws1_test_clk_valid = 1;
#endif

    if((p->tws_status != PLAYER_TWS_STATUS_PLAY) || p->tws1_info_valid) {
        return;
    }

#if (SOFT_SDM_OSR_SHIFT == 1)
    int32_t shift_bytes = (p->channels == 1) ? 0 : 1;
    p->buf_cnt_ori = (*pcm_buf_cnt & 0xFFFF) >> shift_bytes;
    p->tws1_irq_clk = bt_clk_us;
    p->tws1_info_valid = 1;
#else
    // wait for a changed sample, so that calc ns of a sample
    if(p->channels == 2) {
		cyc_start = k_cycle_get_32();

		p->buf_cnt_ori = *pcm_buf_cnt & 0xFFFE;
		while(i++ < 10000) {
        	buf_cnt = *pcm_buf_cnt & 0xFFFE;

			if(buf_cnt != p->buf_cnt_ori)
				break;
		}

		cyc_end = k_cycle_get_32();
		p->buf_cnt_ori >>= 1;
    }
#ifdef TWS1_TEST
    p->tws1_cyc_start = cyc_start;
#endif

    p->tws1_cyc_diff = cyc_end - cyc_start;
    p->tws1_irq_clk = bt_clk_us;
    p->tws1_info_valid = 1;
#endif
}
#endif

//callback from irq
static void _tws_irq_handle(uint64_t bt_clk_us)
{
    bt_tws_observer_inner_t *p = &observer_inner;

    if(p->tws_status <= PLAYER_TWS_STATUS_INIT) {
        SYS_LOG_ERR("status invalid");
        return;
    }

#ifdef CONFIG_SOFT_SDM_CNT
    //if(0 != bt_clk_us) {
    //    _tws1_irq_handle(bt_clk_us);
    //}
#endif

    /* 调节速度中断(TWS播放时才有效)
     */
    if((p->aps_level_pending >= 0)
        && (abs((int32_t)(p->aps_inttime - bt_clk_us)) < 3000)
        && (p->tws_status == PLAYER_TWS_STATUS_PLAY)) {
        media_set_base_aps_level(p->media_observer, p->aps_level_pending);
        SYS_LOG_INF("aps int:%d", p->aps_level_pending);
        p->aps_level_pending = -1;
    }

#if defined(CONFIG_SYNC_BY_SDM_CNT) && !defined(CONFIG_SOFT_SDM_CNT)
    if(0 != bt_clk_us) {
#if 0
        if(p->irq_clk != UINT64_MAX) {
            uint32_t sdm_cnt_saved = media_get_sdm_cnt_saved(p->media_observer);
            uint64_t cac_clk = p->irq_clk
                + media_get_sdm_cnt_delta_time(p->media_observer, (int32_t)(sdm_cnt_saved - p->sdm_cnt_saved));
            int32_t diff_clk = (int32_t)(bt_clk_us - cac_clk);
            printk("diff_clk: %d\n", diff_clk);
        }
#endif

        p->sdm_cnt_saved = media_get_sdm_cnt_saved(p->media_observer);
        p->irq_clk = bt_clk_us;
        p->irq_clk_next = bt_clk_us;
        p->sdm_valid = 500000;
    }
#endif

    if(p->tws_status == PLAYER_TWS_STATUS_PCM_STREAMING) {
        p->tws_status = PLAYER_TWS_STATUS_PLAY;
        media_start_playback(p->media_observer);
        SYS_LOG_INF("start playback, tws clk: %u, %u",
            (uint32_t)(p->splay_local_time & 0xffffffff),
            (uint32_t)(bt_clk_us & 0xffffffff));
        // audiolcy_cmd_entry(LCYCMD_SET_START, NULL, 0);
    }

    if(p->tws_status == PLAYER_TWS_STATUS_WAIT) {
        SYS_LOG_ERR("tws irq no handle, status: %d, aps pending: %d, play clk: %u, aps clk: %u, irq clk: %u",
            p->tws_status, p->aps_level_pending,
            (uint32_t)(p->splay_local_time & 0xffffffff),
            (uint32_t)(p->aps_inttime & 0xffffffff),
            (uint32_t)(bt_clk_us & 0xffffffff));
    }
}

static void _drop_old_packet_hdr(bt_tws_observer_inner_t *p, uint16_t cur_pkt_seq)
{
    stream_pkt_t stream_pkt;

    do
    {   // decoded and output to dac, read out it's hdr info
        if(acts_ringbuf_length(&p->stream_pkt_buffer) < sizeof(stream_pkt))
            break;

        acts_ringbuf_peek(&p->stream_pkt_buffer, &stream_pkt, sizeof(stream_pkt));
        if(PKT_SEQ_NO_SMALL(stream_pkt.pkt_seq, cur_pkt_seq))
            break;

        p->frame_cnt_head += stream_pkt.frame_cnt;
        acts_ringbuf_get(&p->stream_pkt_buffer, NULL, sizeof(stream_pkt));
    }while(1);
}

static int32_t _get_buffer_time(bt_tws_observer_inner_t *p)
{
    int32_t buffer_time;
    uint32_t flags = irq_lock();

    _drop_old_packet_hdr(p, p->splay_pktseq);

    buffer_time = (p->frame_cnt_tail - p->frame_cnt_head)
        * (int64_t)p->frame_samples * 1000000 / p->sample_rate
        // + k_cyc_to_us_floor32(k_cycle_get_32() - p->buffer_time_cyc);
        + (k_cycle_get_32() - p->buffer_time_cyc) / 24;

    irq_unlock(flags);

    return buffer_time;
}

static void _deal_wait_timer_proc(os_timer *ttimer)
{
    bt_tws_observer_inner_t *p = &observer_inner;

    if(p->tws_status == PLAYER_TWS_STATUS_COMFIRM_SEQNO) {
        int32_t pass_time = cyc_to_us(k_cycle_get_32() - p->first_pkt_cyc);
        int32_t buffer_time = _get_buffer_time(p);
        int32_t cache_time = p->cache_time * 1000 - TWS_MIN_INTERVAL;

        printk("pass_time: %d, buffer_time: %d\n", pass_time, buffer_time);

        // check a2dp data rate
        if (p->format == SBC_TYPE || p->format == AAC_TYPE || p->format == LDAC_TYPE) {
            if(((buffer_time < (pass_time * 1 / 2)) || (buffer_time > (pass_time * 3 / 2)))
                && ((buffer_time < (cache_time * 2 / 3)) || (buffer_time > (cache_time * 4 / 3)))){
                printk("data rate abnormal\n");
                p->tws_status = PLAYER_TWS_STATUS_STREAM_ABNORMAL;
                return;
            }
        }

        SYS_LOG_INF("comfirm seq: %u, fix seq: %u",
            p->splay_pktseq, fix_pkt_seq(p, p->splay_pktseq));
        p->first_stream_pkt = p->splay_pktseq;
        media_set_start_pkt_seq(p->media_observer, 1, p->splay_pktseq);
        p->tws_status = PLAYER_TWS_STATUS_PCM_STREAMING;
#ifdef CONFIG_SOC_SERIES_LARK
        //dac_en由tws中断触发使能，太早送数据会导致中间件线程卡住比较长时间，
        //因此这里只提前30毫秒向DAC送数据
        media_set_dac_trigger_src(p->media_observer, TRIGGER_SRC_TWS_IRQ0);
#endif
        media_start_pcm_streaming(p->media_observer, p->pcm_streaming_timeout);
        if(p->pcm_streaming_timeout != 0) {
            os_timer_start(&p->deal_wait_timer, K_MSEC(p->pcm_streaming_timeout/1000), K_MSEC(0));
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

static int32_t _set_tws_play_time(bt_tws_observer_inner_t *p, uint64_t splay_time, uint16_t pktseq)
{
    tws_runtime_observer_t *observer = (tws_runtime_observer_t *)p->btsrv_observer;
    int32_t hr_timeout;
    int32_t ret;
    uint64_t bt_clk;
    int32_t priority_bak  = os_thread_priority_get(os_current_get());

    splay_time = bt_manager_bt_clk_fix_us(splay_time);
    if(p->tws_status != PLAYER_TWS_STATUS_WAIT){
        return -1;
    }

    //避免中间被调度走，导致定时器时间设置误差太大
    os_thread_priority_set(os_current_get(), -CONFIG_NUM_COOP_PRIORITIES);
    bt_clk = observer->get_bt_clk_us();
    if((bt_manager_bt_clk_diff_us(splay_time, bt_clk + TWS_MIN_INTERVAL) < 0)
        || PKT_SEQ_BIGGER(p->first_stream_pkt, pktseq)){
        int32_t frame_count;
        uint16_t dif_pkt = 0;

        if(bt_manager_bt_clk_diff_us(splay_time, bt_clk + TWS_MIN_INTERVAL) < 0) {
            dif_pkt = bt_manager_bt_clk_diff_us(bt_clk + TWS_MIN_INTERVAL, splay_time) / p->pkt_time_us + 1;
        }
        if(PKT_SEQ_BIGGER(p->first_stream_pkt, pktseq)
            && (dif_pkt < (p->first_stream_pkt - pktseq))) {
            dif_pkt = p->first_stream_pkt - pktseq;
        }

        frame_count = _cac_pkt_frames(p, pktseq, &pktseq, dif_pkt);
        if(frame_count > 0) {
            printk("splay_time before fix: %u, %d\n", (uint32_t)splay_time, frame_count);
            splay_time += frame_count * p->frame_samples * 1000000ll / p->sample_rate;
            splay_time = bt_manager_bt_clk_fix_us(splay_time);
        }
    }

    hr_timeout = bt_manager_bt_clk_diff_us(splay_time, bt_clk);
    if((hr_timeout < TWS_MIN_INTERVAL) || (hr_timeout > 500000)) {
        SYS_LOG_ERR("new splay timer invalid: %d", hr_timeout);
        goto ERROUT;
    }

    p->tws_status = PLAYER_TWS_STATUS_COMFIRM_SEQNO;
    p->first_pkt_cyc += k_us_to_cyc_floor32(p->pkt_time_us * (pktseq - p->splay_pktseq));
    p->splay_local_time = splay_time;
    p->splay_pktseq = pktseq;
    p->first_stream_pkt = p->splay_pktseq;
    media_set_start_pkt_seq(p->media_observer, 0, p->splay_pktseq);

#if 1
    //系统时钟，硬件中断有时不准，改用查询方式
    p->pcm_streaming_timeout = 0;
#else
    if(hr_timeout < (TWS_MIN_INTERVAL + 2000)) {
        p->pcm_streaming_timeout = hr_timeout + 25000;
        _deal_wait_timer_proc(NULL);
    } else {
        p->pcm_streaming_timeout = TWS_MIN_INTERVAL + 25000;
        os_timer_start(&p->deal_wait_timer, K_USEC(hr_timeout - TWS_MIN_INTERVAL), K_USEC(0));
    }
#endif

    ret = observer->set_interrupt_time(splay_time);
    if(ret != 0) {
        //SYS_LOG_ERR("set tws irq fail: %d", ret);
    }

    os_thread_priority_set(os_current_get(), priority_bak);
    printk("_set_tws_play_time: %d_%u, %u_%u\n", pktseq, (u32_t)hr_timeout, (u32_t)splay_time, (u32_t)bt_clk);
    return 0;

ERROUT:
    os_thread_priority_set(os_current_get(), priority_bak);
    return -1;
}

static int32_t _single_play_comfirm(bt_tws_observer_inner_t *p, int32_t wait_time)
{
    p->tws_status = PLAYER_TWS_STATUS_COMFIRM_SEQNO;
    p->splay_local_time = UINT64_MAX;
    p->pcm_streaming_timeout = PRE_PCM_STREAMING_TIME;

    wait_time -= p->pcm_streaming_timeout;
    if((wait_time < 1000) || (wait_time > 250000)) {
        _deal_wait_timer_proc(NULL);
    } else {
        os_timer_start(&p->deal_wait_timer, K_MSEC(wait_time/1000), K_MSEC(0));
    }
    return 0;
}

//callback from normal thread
static void _tws_recv_pkt_handle(void *buf, int32_t size)
{
    bt_tws_observer_inner_t *p = &observer_inner;

    if((p->tws_status == PLAYER_TWS_STATUS_NULL) || p->restarting) {
        return;
    }

    if(size != sizeof(player_tws_pkt_t)) {
        //SYS_LOG_ERR("size: %d invalid, normal size: %d", size, sizeof(player_tws_pkt_t));
        return;
    }

    if(acts_ringbuf_space(&p->tws_data_buffer) < sizeof(player_tws_pkt_t)) {
        SYS_LOG_ERR("tws data buffer full");
        return;
    }

    acts_ringbuf_put(&p->tws_data_buffer, buf, size);
}

/*----------------------------------------------------------------------------------------------------*/
static int32_t _tws_send_pkt(bt_tws_observer_inner_t *p, void *data, uint8_t type, uint8_t nego_seq)
{
    tws_runtime_observer_t *observer = (tws_runtime_observer_t *)p->btsrv_observer;
    player_tws_pkt_t pkt;
    int32_t ret;

    pkt.nego_seq = nego_seq;
    pkt.type = type;
    pkt.format = p->format;
    pkt.payload_len = sizeof(pkt.payload_data);
    memcpy(pkt.payload_data, data, sizeof(pkt.payload_data));

    ret = observer->send_pkt(&pkt, sizeof(pkt));
    if(ret != sizeof(pkt)) {
        return -1;
    }
    return 0;
}

int32_t tws_send_pkt_user_type(void *data, u32_t size)
{
    bt_tws_observer_inner_t *p = &observer_inner;

    // abort, observer_inner not init
    if (p->tws_status < PLAYER_TWS_STATUS_INIT) {
        return -1;
    }

    if (size > 16) {
        printk("<W> tws_send_pkt too much %d > 16\n", size);
    }

    return _tws_send_pkt(p, data, TWS_USER_TYPE_PKT, 0);
}

uint32_t tws_send_pkt_user_cb_set(tws_send_user_type_cb user_cb)
{
    bt_tws_observer_inner_t *p = &observer_inner;

    if (p->tws_status < PLAYER_TWS_STATUS_INIT) {
        return 0;
    }

    p->tws_send_user_cb = user_cb;
    return 1;
}

static int32_t tws_deal_pkt_user_type(bt_tws_observer_inner_t *p, player_tws_pkt_t *tws_pkt)
{
    void *user_param = (void *)(tws_pkt->payload_data);
    if (p->tws_send_user_cb) {
        p->tws_send_user_cb(user_param);
    }

    return 0;
}

/* 检测包信息: 0 表示不保存包信息单元, -1 表示需要重启播放器, 1 表示正常存储包信息单元
 */
static int32_t _tws_check_pinfo_unit(bt_tws_observer_inner_t *p, uint16_t pkt_seq, uint16_t pkt_len, uint16_t samples)
{
    if(pkt_len == 0) {
        return 0;
    }

    // pinfo cleared, don't need to save the info of the same pkt any more
    if (p->pinfo_seq_pendding >= 0 && p->pinfo_seq_pendding == pkt_seq) {
        return 0;
    }

    p->pinfo_seq_pendding = -1;
    if((pkt_seq == p->pinfo_unit[p->pinfo_unit_idx].pkt_seq)
        && (p->pinfo_unit[p->pinfo_unit_idx].pkt_len != 0)) {
        int32_t invalid = 0;

        if(p->format == SBC_TYPE || p->format == LDAC_TYPE) {
            if(p->pinfo_unit[p->pinfo_unit_idx].samples >= 2048)
                invalid = 1;
        } else {
            invalid = 1;
        }

        if(invalid) {
            SYS_LOG_ERR("pkt info invalid: %d_%d_%d, %d_%d_%d",
                p->pinfo_unit[p->pinfo_unit_idx].pkt_seq,
                p->pinfo_unit[p->pinfo_unit_idx].pkt_len,
                p->pinfo_unit[p->pinfo_unit_idx].samples,
                pkt_seq, pkt_len, samples);
        }

        p->pinfo_unit[p->pinfo_unit_idx].samples += samples;
        return 0;
    }

    if(pkt_seq != (uint16_t)(p->pinfo_unit[p->pinfo_unit_idx].pkt_seq + 1)) {
        if(p->pinfo_unit[p->pinfo_unit_idx].pkt_len != 0)
            return -1;
    }

    return 1;
}

static int32_t _tws_update_sync_info(bt_tws_observer_inner_t *p, tws_pkt_info_t *pkt_info)
{
    /* 更新idx及其内容
     */
    p->pinfo_unit_idx ++;
    if(p->pinfo_unit_idx > 1) {
        p->pinfo_unit_idx = 0;
    }

    p->pinfo_unit[p->pinfo_unit_idx].pkt_len = pkt_info->pkt_len;
    p->pinfo_unit[p->pinfo_unit_idx].pkt_seq = pkt_info->pkt_seq;
    p->pinfo_unit[p->pinfo_unit_idx].samples = pkt_info->samples;
    p->pinfo_unit[p->pinfo_unit_idx].pkt_bttime_us = pkt_info->pkt_bttime_us;
#ifdef CONFIG_SYNC_BY_SDM_CNT
    p->pinfo_unit[p->pinfo_unit_idx].sdm_cnt = pkt_info->sdm_cnt;
#endif

    /* 确保3次之后才处理SPLAY包避免开始的包不全导致计算对端播放时间出错
     */
    p->save_pinfo_count ++;
    if(p->save_pinfo_count > 3) {
        p->save_pinfo_count = 3;
    }

    if(pkt_info->pkt_seq == p->remote_splay_seq) {
        printk("new splay time, seq:%u, time: %u_%u\n",
            fix_pkt_seq(p, pkt_info->pkt_seq),
            (uint32_t)((pkt_info->pkt_bttime_us >> 32) & 0xffffffff),
            (uint32_t)(pkt_info->pkt_bttime_us & 0xffffffff));
    }

    return 0;
}

/* 根据本端和对端的当前信息计算对端的SPLAY
 */
static int32_t _tws_calculate_new_splay(bt_tws_observer_inner_t *p, player_tws_startplay_pkt_t *startplay)
{
    int32_t frame_count = 0;

    startplay->splay_flag = PLAYER_TWS_USE_LOCAL_SPLAY;
    startplay->first_clk = p->first_clk;

    /* 要求对端以指定帧播放(当本端还没有足够的播放信息时, 让对端重新发起SPLAY)
     */
    if((p->save_pinfo_count < 3) && (p->splay_local_time == UINT64_MAX)) {
        goto ERROUT;
    }

    startplay->pkt_seq = to_local_pkt_seq(p, startplay->pkt_seq);
    if (p->save_pinfo_count < 3) {
        frame_count = _cac_pkt_frames(p, p->splay_pktseq, &startplay->pkt_seq, 0);
        if(frame_count < 0) {
            goto ERROUT;
        }

        startplay->splay_time = p->splay_local_time
            + frame_count * p->frame_samples * 1000000ll / p->sample_rate;
    } else {
        frame_count = _cac_pkt_frames(p, p->pinfo_unit[p->pinfo_unit_idx].pkt_seq, &startplay->pkt_seq, 0);
        if(frame_count < 0) {
            goto ERROUT;
        }

        startplay->splay_time = p->pinfo_unit[p->pinfo_unit_idx].pkt_bttime_us
            + frame_count * p->frame_samples * 1000000ll / p->sample_rate;
    }

    startplay->splay_time = bt_manager_bt_clk_fix_us(startplay->splay_time);
    p->remote_splay_seq = startplay->pkt_seq;
    startplay->pkt_seq = fix_pkt_seq(p, startplay->pkt_seq);
    SYS_LOG_INF("new splay, remote seq:%u, time: %u_%u, local seq: %u, frame_samples: %u",
        startplay->pkt_seq,
        (uint32_t)((startplay->splay_time >> 32) & 0xffffffff),
        (uint32_t)(startplay->splay_time & 0xffffffff),
        fix_pkt_seq(p, p->pinfo_unit[p->pinfo_unit_idx].pkt_seq),
        p->frame_samples);
    return 0;

ERROUT:
    SYS_LOG_INF("need new splay, pinfo: %u, sinfo: %u, cur seq: %u",
        p->save_pinfo_count,
        acts_ringbuf_length(&p->stream_pkt_buffer)/sizeof(stream_pkt_t),
        fix_pkt_seq(p, p->pinfo_unit[p->pinfo_unit_idx].pkt_seq));
    startplay->splay_flag = PLAYER_TWS_NEED_NEW_SPLAY;
    return -1;
}

/* 对端要求本端发相同的SPLAY过去
 */
static int32_t _tws_handle_newly_splay(bt_tws_observer_inner_t *p)
{
    //tws_runtime_observer_t *observer = (tws_runtime_observer_t *)p->btsrv_observer;
    player_tws_startplay_pkt_t startplay;

    if(p->tws_status != PLAYER_TWS_STATUS_WAIT) {
        return 0;
    }
    memset(&startplay,0,sizeof(player_tws_startplay_pkt_t));
    startplay.splay_flag = PLAYER_TWS_USE_START_SPLAY;
    startplay.pkt_seq =  p->splay_pktseq;
    startplay.splay_time = p->splay_local_time;
    startplay.first_clk = p->first_clk;
    if(_tws_send_pkt(p, &startplay, TWS_STARTPLAY_PKT, nego_seq) < 0) {
        SYS_LOG_ERR("_tws_send_start_play fail.");
        return -1;
    }

    p->splay_aps_cyc = k_cycle_get_32();
    SYS_LOG_INF("new splay, seq: %u, first clk: %u, time: %u_%u, nego_seq: %u",
        p->splay_pktseq, p->first_clk,
        (uint32_t)((startplay.splay_time >> 32) & 0xffffffff),
        (uint32_t)(startplay.splay_time & 0xffffffff),
        nego_seq);
    return 0;
}

/* 对端要求本端解码指定的包播放
 */
static int32_t _tws_handle_local_splay(bt_tws_observer_inner_t *p, player_tws_startplay_pkt_t *startplay, uint64_t bttus)
{
    int32_t ret;

    if(p->tws_status > PLAYER_TWS_STATUS_WAIT) {
        SYS_LOG_ERR("already playing, remote seq: %u, time: %u_%u, local seq: %u, time: %u_%u",
            startplay->pkt_seq,
            (uint32_t)((startplay->splay_time >> 32) & 0xffffffff),
            (uint32_t)(startplay->splay_time & 0xffffffff),
            fix_pkt_seq(p, p->splay_pktseq),
            (uint32_t)((p->splay_local_time >> 32) & 0xffffffff),
            (uint32_t)(p->splay_local_time & 0xffffffff));
        return 0;
    }

    ret = _set_tws_play_time(p, startplay->splay_time, to_local_pkt_seq(p, startplay->pkt_seq));
    if(ret < 0)
        return -1;

    SYS_LOG_INF("handle local splay, seq: %u, time: %u_%u",
        startplay->pkt_seq,
        (uint32_t)((startplay->splay_time >> 32) & 0xffffffff),
        (uint32_t)(startplay->splay_time & 0xffffffff));

    return 0;
}

/* 对端告诉本端正在启动播放
 */
static int32_t _tws_handle_start_splay(bt_tws_observer_inner_t *p, player_tws_startplay_pkt_t *startplay, uint64_t bttus, uint8_t nego_seq)
{
    uint32_t flags, temp_pkttime;
    uint64_t expect_time;

    if(p->format == MSBC_TYPE || p->format == CVSD_TYPE) {
        /* 确保两边都使用统一的包号来比较
         */
        int32_t diff_clk = first_clk_diff(startplay->first_clk, p->first_clk);
        if(diff_clk > 0) {
            startplay->pkt_seq += diff_clk / 24;
            SYS_LOG_INF("new seq:%d", startplay->pkt_seq);
        }
    }

    if(p->tws_status > PLAYER_TWS_STATUS_WAIT) {
        // follow splay in ALLM, sync apt lcy to the other first
        // if (audiolcy_get_latency_mode() == LCYMODE_ADAPTIVE) {
        //     audiolcy_cmd_entry(LCYCMD_APT_SYNCLCY, NULL, 0);
        // }
        goto NEW_SPLAY;
    }

    /* 如果通信较慢己方反推已超时, 需要纠正对方包信息
     * 对端包号小于当前包号, 使用当前包号, 否则使用对端包号和时间
     * 根据reverse_pkts反推splay_pktseq所对应的时钟expect_time
     * 将expect_time与当前时间比较, 计算满足的播放时钟, 有需要可以适当丢弃一些数据
     */

    if (p->splay_reverse_timeout > 0 && p->splay_reverse_pkts > 0) {
        startplay->pkt_seq += p->splay_reverse_pkts;
        startplay->reverse_pkts -= p->splay_reverse_pkts;
        printk("remote seq fix %d_%d\n", startplay->pkt_seq, startplay->reverse_pkts);
        p->splay_reverse_timeout = 0;
        p->splay_reverse_pkts = 0;
    }

    if(PKT_SEQ_BIGGER(startplay->pkt_seq, fix_pkt_seq(p, p->splay_pktseq))
        || ((startplay->pkt_seq == fix_pkt_seq(p, p->splay_pktseq))
        && (bt_manager_bt_clk_diff_us(p->splay_local_time, startplay->splay_time) < 0))) {

        if (p->format == MSBC_TYPE || p->format == CVSD_TYPE) {
            temp_pkttime = p->pkt_time_us;
        } else {
            // sbc and aac: startplay->first_clk is p->pkt_time_us of the other device
            temp_pkttime = startplay->first_clk;
        }
        expect_time = startplay->splay_time - startplay->reverse_pkts * temp_pkttime;
        p->splay_reverse_pkts = 0;
        _set_tws_play_time(p, expect_time, to_local_pkt_seq(p, startplay->pkt_seq));
    } else {
        expect_time = p->splay_local_time - p->splay_reverse_pkts * p->pkt_time_us;
        p->splay_reverse_pkts = 0;
        _set_tws_play_time(p, expect_time, p->splay_pktseq);
    }

NEW_SPLAY:
    /* 确定对端开始播放的蓝牙时钟
     */
    _tws_calculate_new_splay(p, startplay);

    /* 发送给对端样机
     */
    if(_tws_send_pkt(p, startplay, TWS_STARTPLAY_PKT, nego_seq) < 0) {
        SYS_LOG_ERR("_tws_send_start_play fail.");
        return -1;
    }

    flags = irq_lock();
    //有接入时一段时间不能进行缓冲区调节
    p->splay_aps_cyc = k_cycle_get_32();
    p->forbid_aps = 1;
    p->aps_level_pending = -1;
    //对端接入后使用基准时钟播放
    media_set_base_aps_level(p->media_observer, 0xFF);
    irq_unlock(flags);

    return 0;
}

/* 返回-1: 表示失败, 需重启播放器
 */
static void _tws_deal_startplay(bt_tws_observer_inner_t *p, player_tws_pkt_t *tws_pkt)
{
    tws_runtime_observer_t *observer = (tws_runtime_observer_t *)p->btsrv_observer;
    player_tws_startplay_pkt_t *startplay = (player_tws_startplay_pkt_t *)(tws_pkt->payload_data);
    uint64_t bt_clk;

    if(p->tws_status < PLAYER_TWS_STATUS_WAIT) {
        return;
    }

    bt_clk = observer->get_bt_clk_us();
    if(bt_clk == UINT64_MAX) {
        SYS_LOG_ERR("get_bt_clk_us fail");
        return;
    }

    if(p->format == MSBC_TYPE || p->format == CVSD_TYPE) {
        p->first_clk = observer->get_first_pkt_clk();
        int32_t diff_clk = first_clk_diff(p->first_clk, startplay->first_clk);
        if(diff_clk <= 0)
            p->diff_pkt = 0;
        else
            p->diff_pkt = (uint16_t)(diff_clk / 24);
    }

    SYS_LOG_INF("deal SPlay btclk %u_%u",
        (uint32_t)((bt_clk >> 32) & 0xffffffff),(uint32_t)(bt_clk & 0xffffffff));
    printk("remote seq: %u, clk: %u, time: %u_%u, flag: %u, nego_seq: %u, %d\n",
        startplay->pkt_seq, startplay->first_clk,
        (uint32_t)((startplay->splay_time >> 32) & 0xffffffff),
        (uint32_t)(startplay->splay_time & 0xffffffff),
        startplay->splay_flag, tws_pkt->nego_seq, startplay->reverse_pkts);
    printk("local  seq: %u, clk: %u, time: %u_%u, status: %u, seq diff: %u, %d\n",
        fix_pkt_seq(p, p->splay_pktseq), p->first_clk,
        (uint32_t)((p->splay_local_time >> 32) & 0xffffffff),
        (uint32_t)(p->splay_local_time & 0xffffffff),
        p->tws_status, p->diff_pkt, p->splay_reverse_pkts);

    switch(startplay->splay_flag) {
    case PLAYER_TWS_NEED_NEW_SPLAY:
        _tws_handle_newly_splay(p);
        break;
    case PLAYER_TWS_USE_LOCAL_SPLAY:
        if(tws_pkt->nego_seq != nego_seq)
            break;
        _tws_handle_local_splay(p, startplay, bt_clk);
        break;

    case PLAYER_TWS_USE_START_SPLAY:
        _tws_handle_start_splay(p, startplay, bt_clk, tws_pkt->nego_seq);
        break;

    default:
        SYS_LOG_ERR("splay_flag invalid");
        break;
    }
}

#ifdef CONFIG_SYNC_BY_SDM_CNT
static int32_t _tws_deal_sdm_info(bt_tws_observer_inner_t *p, player_tws_pkt_t *tws_pkt)
{
    player_tws_sdm_info_pkt_t *sdm_info = (player_tws_sdm_info_pkt_t *)(tws_pkt->payload_data);
    int32_t sdm_diff;

    printk("deal sdm Info, seq: %u, time: %u, sdm cnt: %u/%u, save: %u\n",
        sdm_info->pkt_seq, sdm_info->bttime_ms,
        sdm_info->sdm_cnt[0], sdm_info->sdm_cnt[1], sdm_info->sdm_cnt_save);

    if(p->tws_status != PLAYER_TWS_STATUS_PLAY)
        return -1;
    if(p->sdm_info.bttime_ms != sdm_info->bttime_ms)
        return -1;
    do
    {
        if(p->sdm_info.pkt_seq == sdm_info->pkt_seq) {
            p->sdm_diff = sdm_info->sdm_cnt[0] - p->sdm_info.sdm_cnt[0];
            p->sdm_diff_valid = 1;
            break;
        }
        if((p->sdm_info.pkt_seq + 1) == sdm_info->pkt_seq) {
            p->sdm_diff = sdm_info->sdm_cnt[1] - p->sdm_info.sdm_cnt[0];
            p->sdm_diff_valid = 1;
            break;
        }
        if(p->sdm_info.pkt_seq == (sdm_info->pkt_seq + 1)) {
            p->sdm_diff = sdm_info->sdm_cnt[0] - p->sdm_info.sdm_cnt[1];
            p->sdm_diff_valid = 1;
            break;
        }
    }while(0);

    if(!p->sdm_diff_valid) {
        SYS_LOG_ERR("sync fail");
        p->tws_observer.trigger_restart(p, 0);
        return -1;
    }
    //printk("sdm diff: %d, %d\n", p->sdm_diff, (int)(p->sdm_info.pkt_seq - sdm_info->pkt_seq));

    sdm_diff = p->sdm_info.sdm_cnt_save - sdm_info->sdm_cnt_save + p->sdm_diff;
    sdm_diff = media_get_sdm_cnt_delta_time(p->media_observer, sdm_diff);

    media_notify_time_diff(p->media_observer, sdm_diff);
    return 1;
}

#else

static int32_t _tws_deal_syncinfo(bt_tws_observer_inner_t *p, player_tws_pkt_t *tws_pkt)
{
    player_tws_syncinfo_pkt_t *syncinfo = (player_tws_syncinfo_pkt_t *)(tws_pkt->payload_data);

    printk("deal SInfo, seq: %u, time: %u_%u, status: %u\n",
        syncinfo->pkt_seq,
        (uint32_t)((syncinfo->bttime_us >> 32) & 0xffffffff),
        (uint32_t)(syncinfo->bttime_us & 0xffffffff),
        p->tws_status);

    if(p->tws_status != PLAYER_TWS_STATUS_PLAY)
        return -1;

    if(acts_ringbuf_space(&p->sinfo_master_buffer) < sizeof(player_tws_syncinfo_pkt_t)) {
        SYS_LOG_ERR("sinfo_master_buffer full");
        acts_ringbuf_get(&p->sinfo_master_buffer, NULL, sizeof(player_tws_syncinfo_pkt_t));
    }

    acts_ringbuf_put(&p->sinfo_master_buffer, syncinfo, sizeof(player_tws_syncinfo_pkt_t));

    return 1;
}

/* 返回-1:slave_pool或maste_pool没有item
   返回 0:遍历完后没有找到相同包号的item
   返回 1:找到相同包号的item并从参数传出来
 */
static int32_t _tws_search_sinfo_item(bt_tws_observer_inner_t *p, player_tws_syncinfo_pkt_t *slave, player_tws_syncinfo_pkt_t *maste)
{
    uint8_t slave_keep = 1;
    uint8_t maste_keep = 1;
    player_tws_syncinfo_pkt_t slave_item;
    player_tws_syncinfo_pkt_t maste_item;
    uint8_t item_size = sizeof(player_tws_syncinfo_pkt_t);
    struct acts_ringbuf *slave_pool = &p->sinfo_slave_buffer;
    struct acts_ringbuf *maste_pool = &p->sinfo_master_buffer;

    while(1)
    {
        if(slave_keep == 1) {
            if(acts_ringbuf_length(slave_pool) < item_size)
                return -1;

            slave_keep = 0;
            acts_ringbuf_peek(slave_pool, &slave_item, item_size);
        }

        if(maste_keep == 1) {
            if(acts_ringbuf_length(maste_pool) < item_size)
                return -1;

            maste_keep = 0;
            acts_ringbuf_peek(maste_pool, &maste_item, item_size);
        }

        if(fix_pkt_seq(p, slave_item.pkt_seq) == maste_item.pkt_seq) {
            //SYS_LOG_INF("item get:%d", maste_item.pkt_seq);
            acts_ringbuf_get(maste_pool, NULL, item_size);
            acts_ringbuf_get(slave_pool, NULL, item_size);
            memcpy(slave, &slave_item, item_size);
            memcpy(maste, &maste_item, item_size);
            break;
        }

        //if(slave_item.pkt_seq > maste_item.pkt_seq)
        if(PKT_SEQ_BIGGER(fix_pkt_seq(p, slave_item.pkt_seq), maste_item.pkt_seq)) {
            maste_keep = 1;
            acts_ringbuf_get(maste_pool, NULL, item_size);
            //SYS_LOG_INF("item mkp:%d_%d", maste_item.pkt_seq, fix_pkt_seq(p, slave_item.pkt_seq));
        } else {
            slave_keep = 1;
            acts_ringbuf_get(slave_pool, NULL, item_size);
            //SYS_LOG_INF("item skp:%d_%d", maste_item.pkt_seq, fix_pkt_seq(p, slave_item.pkt_seq));
        }
    }

    return 1;
}

static int32_t _tws_align_samples(bt_tws_observer_inner_t *p)
{
    player_tws_syncinfo_pkt_t slave_item, maste_item;
    int32_t clk_diff;
    int32_t result = 0;
    int8_t shake_flag;

    result = _tws_search_sinfo_item(p, &slave_item, &maste_item);
    if(result <= 0) {
        p->sync_error_count = (result == 0) ? (p->sync_error_count + 1) : 0;
        if(p->sync_error_count >= PLAYER_TWS_ERRCOUNT_MAXNUM) {
            SYS_LOG_ERR("long time no sync info");
            p->sync_error_count = 0;
            p->tws_observer.trigger_restart(p, 0);
        }
        return 0;
    }

    p->sync_error_count = 0;
    if(slave_item.bttime_us == maste_item.bttime_us)
        return 0;

    clk_diff = (int32_t)(maste_item.bttime_us - slave_item.bttime_us);
    shake_flag = (clk_diff < 0) ? 1 : -1;

    if((maste_item.pkt_seq % p->Sync_interval) == 0) {
        p->adjust_shake_flag = shake_flag;
        printk("adjust1, seq: %u, diff: %d us\n",  maste_item.pkt_seq, clk_diff);
        return 0;
    }

    if(p->adjust_shake_flag == 0)
        return 0;

    printk("adjust2, seq: %u, diff: %d us\n",  maste_item.pkt_seq, clk_diff);
    if(p->adjust_shake_flag == shake_flag) {
        media_notify_time_diff(p->media_observer, clk_diff);
    }

    p->adjust_shake_flag = 0;
    return 0;
}
#endif

static int32_t _tws_deal_playrate(bt_tws_observer_inner_t *p, player_tws_pkt_t *tws_pkt)
{
    tws_runtime_observer_t *observer = (tws_runtime_observer_t *)p->btsrv_observer;
    player_tws_playrate_pkt_t *playrate = (player_tws_playrate_pkt_t *)(tws_pkt->payload_data);
    uint32_t flags;

    SYS_LOG_INF("deal APSInfo, level: %u, time: %u_%u, status: %u",
        playrate->aps_level,
        (uint32_t)((playrate->inttime >> 32) & 0xffffffff),
        (uint32_t)(playrate->inttime & 0xffffffff),
        p->tws_status);

    if(p->tws_status != PLAYER_TWS_STATUS_PLAY)
        return 0;

#if defined(CONFIG_SYNC_BY_SDM_CNT) && !defined(CONFIG_SOFT_SDM_CNT)
    if((playrate->inttime < p->irq_clk_next) || (p->irq_clk_next == UINT64_MAX))
#endif
        observer->set_interrupt_time(playrate->inttime);

    /* 加入中断信号队列
     */
    flags = irq_lock();
    p->aps_level_pending = playrate->aps_level;
    p->aps_inttime = playrate->inttime;
    irq_unlock(flags);

    return 0;
}

static int32_t _tws_handle_set_stream_info(void)
{
    bt_tws_observer_inner_t *p = &observer_inner;
    tws_runtime_observer_t *observer = (tws_runtime_observer_t *)p->btsrv_observer;
    player_tws_startplay_pkt_t startplay;
    int32_t wait_time;
    int32_t ret = 0;
    uint16_t WPlay_Mintime;
    uint16_t WPlay_Maxtime;

    if(p->tws_status != PLAYER_TWS_STATUS_INIT) {
        SYS_LOG_ERR("bt tws status: %d", p->tws_status);
        return -1;
    }

    memset(&startplay, 0, sizeof(player_tws_startplay_pkt_t));
    p->first_clk = observer->get_first_pkt_clk();
    p->aps_level_pending = -1;
    p->first_pkt_flag = 1;
    p->tws_status = PLAYER_TWS_STATUS_WAIT;

    audio_policy_get_snoop_tws_sync_param(p->format,
        &WPlay_Mintime, &WPlay_Maxtime, &p->Sync_interval);
    p->WPlay_Mintime = WPlay_Mintime * 1000;
    p->WPlay_Maxtime = WPlay_Maxtime * 1000;

    //wait_time = _cac_wait_time(p, (int32_t)p->WPlay_Mintime, 1);  // drop data right now
    wait_time = _cac_wait_time(p, (int32_t)p->WPlay_Mintime, 0);    // wait for reply, drop less data
    printk("WPlay_Mintime: %u, WPlay_Maxtime: %u, Sync_interval: %u, cache_time: %u\n",
        p->WPlay_Mintime, p->WPlay_Maxtime, p->Sync_interval, p->cache_time);

    do
    {
        if(p->tws_role == BTSRV_TWS_NONE)
            break;

        nego_seq++;
        startplay.splay_flag = PLAYER_TWS_USE_START_SPLAY;
        startplay.pkt_seq = p->splay_pktseq;
        startplay.splay_time = observer->get_bt_clk_us() + wait_time;
        startplay.reverse_pkts = p->splay_reverse_pkts;
        if (p->format == MSBC_TYPE || p->format == CVSD_TYPE) {
            startplay.first_clk = p->first_clk;
        } else {
            startplay.first_clk = p->pkt_time_us; // sbc and aac: first_clk is pkt_time
        }

        startplay.splay_time = bt_manager_bt_clk_fix_us(startplay.splay_time);
        p->splay_local_time = startplay.splay_time;
        p->splay_aps_cyc = k_cycle_get_32();
        p->forbid_aps = 1;

        ret = _tws_send_pkt(p, &startplay, TWS_STARTPLAY_PKT, nego_seq);
        if(ret < 0) {
            SYS_LOG_ERR("_tws_send_start_play fail.");
            p->tws_role = BTSRV_TWS_NONE;
            break;
        }
    }while(0);

    if(p->tws_role == BTSRV_TWS_NONE) {
        _single_play_comfirm(p, wait_time);
    }

    SYS_LOG_INF("TWS start, role: %d, seq: %u, first clk: %u, time: %u_%u, nego_seq: %u, pkt time: %u",
        p->tws_role, p->splay_pktseq, p->first_clk,
        (uint32_t)((startplay.splay_time >> 32) & 0xffffffff),
        (uint32_t)(startplay.splay_time & 0xffffffff),
        nego_seq, p->pkt_time_us);

    return 0;
}

static int32_t _tws_handle_aps_change_request(bt_tws_observer_inner_t *p, uint8_t level)
{
    tws_runtime_observer_t *observer = (tws_runtime_observer_t *)p->btsrv_observer;
    player_tws_playrate_pkt_t playrate;
    uint32_t flags;

    if((p->tws_role == BTSRV_TWS_SLAVE)
        || (p->tws_status != PLAYER_TWS_STATUS_PLAY)
        || (p->aps_level_pending == level))
        return 0;

    if(p->tws_role == BTSRV_TWS_NONE){
        goto ERROUT;
    }

	memset(&playrate, 0,sizeof(player_tws_playrate_pkt_t));

    /* 获取蓝牙时钟
     */
    playrate.inttime = observer->get_bt_clk_us();
    if(playrate.inttime == 0) {
        SYS_LOG_ERR("get_bt_clk_us fail.");
        goto ERROUT;
    }

    playrate.inttime = bt_manager_bt_clk_fix_us(playrate.inttime + 150000); /* 150 ms */
#if defined(CONFIG_SYNC_BY_SDM_CNT) && !defined(CONFIG_SOFT_SDM_CNT)
    if((p->irq_clk_next != UINT64_MAX) && (abs((int32_t)(p->irq_clk_next - playrate.inttime)) < 30000)) {
        printk("fix aps time: %u to %u\n",
            (uint32_t)playrate.inttime, (uint32_t)(p->irq_clk_next + 30000));
        playrate.inttime = p->irq_clk_next + 30000;
    }
#endif

    /* 告诉对端调节速度
     */
    playrate.aps_level = level;
    if(_tws_send_pkt(p, &playrate, TWS_PLAYRATE_PKT, 0) < 0) {
        SYS_LOG_ERR("_tws_send_play_rate fail.");
        goto ERROUT;
    }

    SYS_LOG_INF("aps int set, level: %u, time: %u_%u",
        level,
        (uint32_t)((playrate.inttime >> 32) & 0xffffffff),
        (uint32_t)(playrate.inttime & 0xffffffff));

#if defined(CONFIG_SYNC_BY_SDM_CNT) && !defined(CONFIG_SOFT_SDM_CNT)
    if((playrate->inttime < p->irq_clk_next) || (p->irq_clk_next == UINT64_MAX))
#endif
        observer->set_interrupt_time(playrate.inttime);

    /* 加入中断信号队列
     */
    flags = irq_lock();
    p->aps_level_pending = level;
    p->aps_inttime = playrate.inttime;
    irq_unlock(flags);
    return 0;

ERROUT:
    media_set_base_aps_level(p->media_observer, level);
    return 0;
}

static int32_t _tws_handle_new_pkt_info(bt_tws_observer_inner_t *p, tws_pkt_info_t *info)
{
    int32_t ret;
    stream_pkt_t stream_pkt;

    // a new pkt received, save it's hdr info
    if(info->samples == 0xFFFFU) {
        if(acts_ringbuf_space(&p->stream_pkt_buffer) < sizeof(stream_pkt)) {
            acts_ringbuf_get(&p->stream_pkt_buffer, &stream_pkt, sizeof(stream_pkt));
            p->frame_cnt_head += stream_pkt.frame_cnt;
        }

        stream_pkt.pkt_seq = info->pkt_seq;
        stream_pkt.pkt_len = info->pkt_len;
        stream_pkt.frame_cnt = info->frame_cnt;
        acts_ringbuf_put(&p->stream_pkt_buffer, &stream_pkt, sizeof(stream_pkt));
        return 0;
    } else {
        _drop_old_packet_hdr(p, info->pkt_seq);
    }

    if(p->tws_status != PLAYER_TWS_STATUS_PLAY)
        return 0;

    //aac两包一帧，第一包解码没有输出，清除包信息，避免时间计算不准确
    if(info->samples == 0) {
        p->pinfo_seq_pendding = info->pkt_seq;
        SYS_LOG_INF("pinfo clear: %d_%d", info->pkt_seq, info->pkt_len);
        goto ERROUT;
    }

    //丢弃不完整的包避免统计时间不正确
    if(p->first_pkt_flag) {
        if(p->first_pkt_flag == 1) {
            p->first_play_pktseq = info->pkt_seq;
            p->first_pkt_flag = 2;
            SYS_LOG_INF("splay_pktseq: %u, %u", p->first_play_pktseq, fix_pkt_seq(p, p->first_play_pktseq));
        }

        if((info->pkt_seq == p->first_play_pktseq) && (p->format == SBC_TYPE || p->format == LDAC_TYPE))
            return 0;

        p->first_pkt_flag = 0;
    }

    // don't need to save pinfo (single device)
    if (UINT64_MAX == info->pkt_bttime_us) {
        if (p->save_pinfo_count > 0) {
            p->pinfo_unit_idx = 0;
            p->save_pinfo_count = 0;
            memset(p->pinfo_unit, 0, sizeof(p->pinfo_unit));
        }

        //if((info->pkt_seq % p->Sync_interval) == 0) {
        //    printk("SInfo, seq: %u, len: %u\n", info->pkt_seq, info->pkt_len);
        //}

        p->frame_samples = info->samples;
        return 0;
    }

    /* 检查包头信息
     */
    ret = _tws_check_pinfo_unit(p, info->pkt_seq, info->pkt_len, info->samples);
    if(ret == 0)
        return 0;
    if(ret < 0) {
        SYS_LOG_ERR("pkt miss, restart: %u_%u",
            fix_pkt_seq(p, info->pkt_seq),
            fix_pkt_seq(p, p->pinfo_unit[p->pinfo_unit_idx].pkt_seq));
        goto ERROUT;
    }

    p->frame_samples = info->samples;
    _tws_update_sync_info(p, info);
    info->pkt_seq = fix_pkt_seq(p, info->pkt_seq);
    if(((info->pkt_seq % p->Sync_interval) == 0) || (p->last_sinfo_flag == 1)) {
        printk("SInfo, seq: %u, len: %u, time: %u\n",
            info->pkt_seq, info->pkt_len,
            (uint32_t)p->pinfo_unit[p->pinfo_unit_idx].pkt_bttime_us);

#ifndef CONFIG_SYNC_BY_SDM_CNT
        player_tws_syncinfo_pkt_t syncinfo;

        if(p->last_sinfo_flag == 1)
            p->last_sinfo_flag = 0;
        if((info->pkt_seq % p->Sync_interval) == 0)
            p->last_sinfo_flag = 1;

        memset(&syncinfo, 0, sizeof(player_tws_syncinfo_pkt_t));
        syncinfo.pkt_seq = info->pkt_seq;
        syncinfo.bttime_us = p->pinfo_unit[p->pinfo_unit_idx].pkt_bttime_us;

        if(p->tws_role == BTSRV_TWS_MASTER)
            _tws_send_pkt(p, &syncinfo, TWS_SYNCINFO_PKT, 0);

        syncinfo.pkt_seq = to_local_pkt_seq(p, info->pkt_seq);
        if(acts_ringbuf_space(&p->sinfo_slave_buffer) < sizeof(player_tws_syncinfo_pkt_t))
            acts_ringbuf_get(&p->sinfo_slave_buffer, NULL, sizeof(player_tws_syncinfo_pkt_t));
        acts_ringbuf_put(&p->sinfo_slave_buffer, &syncinfo, sizeof(player_tws_syncinfo_pkt_t));
#endif
    }

    return 0;

ERROUT:
    p->pinfo_unit_idx = 0;
    p->save_pinfo_count = 0;
    memset(p->pinfo_unit, 0, sizeof(p->pinfo_unit));

    //丢包重启播放
    if((info->samples != 0) && (p->tws_role != BTSRV_TWS_NONE))
        p->tws_observer.trigger_restart(p, 0);
    return 0;
}

static void _check_tws_role_change(bt_tws_observer_inner_t *p, tws_runtime_observer_t *observer)
{
    int32_t role;

    role = observer->get_role();
    if(role == p->tws_role)
        return;

    if(BTSRV_TWS_NONE == p->tws_role) {
#ifdef CONFIG_SYNC_BY_SDM_CNT
        p->sdm_valid = 0;
        p->irq_clk = UINT64_MAX;
#ifndef CONFIG_SOFT_SDM_CNT
        p->irq_clk_next = UINT64_MAX;
        p->peried_irq_set = 0;
        observer->set_period_interrupt_time(0, 0);
#endif
#endif

        p->pinfo_unit_idx = 0;
        p->save_pinfo_count = 0;
        p->sdm_diff_valid = 0;
        memset(p->pinfo_unit, 0, sizeof(p->pinfo_unit));
    }
    if(p->tws_status == PLAYER_TWS_STATUS_PLAY) {
        p->splay_local_time = UINT64_MAX;
    }

#ifdef CONFIG_SYNC_BY_SDM_CNT
    memset(&p->sdm_info, 0, sizeof(p->sdm_info));
#else
    acts_ringbuf_drop_all(&p->sinfo_master_buffer);
    acts_ringbuf_drop_all(&p->sinfo_slave_buffer);
#endif

    p->aps_level_pending = -1;
    p->aps_level_request = -1;
    p->splay_aps_cyc = k_cycle_get_32();
    p->forbid_aps = 1;
    p->tws_role = role;
    p->first_clk = observer->get_first_pkt_clk();
    SYS_LOG_INF("TWS role change: %d, clk: %u", p->tws_role, p->first_clk);

    // audiolcy_cmd_entry(LCYCMD_SET_TWSROLE, (void *)role, 4);
}

static void _sync_bt_clk(bt_tws_observer_inner_t *p, tws_runtime_observer_t *observer)
{
#ifdef CONFIG_SOFT_SDM_CNT
    if(p->sdm_valid > 0) {
        p->sdm_valid -= BT_TWS_LOOP_INTERVAL * 1000;
        if(p->sdm_valid <= 0) {
            p->irq_clk = UINT64_MAX;
            printk("need resync clk\n");
        }
    }

    if(!p->tws1_info_valid)
        return;

#ifdef TWS1_TEST
    uint32_t irq_time = k_cyc_to_us_floor32(p->tws1_cyc_start);
#endif

    uint32_t ns_diff = k_cyc_to_ns_floor32(p->tws1_cyc_diff);
    if(ns_diff > p->sample_time_ns) {
        ns_diff = p->sample_time_ns;
    }

    p->sdm_cnt_saved = ((p->buf_cnt_ori & 0x7FFF) << SOFT_SDM_OSR_SHIFT)
        + ((p->sample_time_ns - ns_diff) << SOFT_SDM_OSR_SHIFT) / p->sample_time_ns;

    p->irq_clk = p->tws1_irq_clk;
    p->sdm_valid = 500000;
    p->tws1_info_valid = 0;

#ifdef TWS1_TEST
    printk("irq: %u, %u, %u, %u, %u\n",
            irq_time - p->irq_time_last,
            (uint32_t)p->sdm_cnt_saved,
            (uint32_t)p->irq_clk,
            (uint32_t)ns_diff,
            (uint32_t)p->buf_cnt_ori);
    p->irq_time_last = irq_time;
#endif

    if(((BTSRV_TWS_MASTER == p->tws_role) || (BTSRV_TWS_SLAVE == p->tws_role))
        && (p->tws_status == PLAYER_TWS_STATUS_PLAY)
        && (((p->irq_clk / TWS_CLK_SYNC_INTERVAL) % TWS_ALIGN_INTERVAL) == 0)
        && (p->save_pinfo_count > 1)){
        p->sdm_info.bttime_ms = (uint32_t)(p->irq_clk/1000);
        p->sdm_info.sdm_cnt_save = p->sdm_cnt_saved;
        p->sdm_info.sdm_cnt[0] = p->pinfo_unit[p->pinfo_unit_idx].sdm_cnt;
        p->sdm_info.sdm_cnt[1] = p->pinfo_unit[(p->pinfo_unit_idx + 1) % 2].sdm_cnt;
        p->sdm_info.pkt_seq = fix_pkt_seq(p, p->pinfo_unit[p->pinfo_unit_idx].pkt_seq);

        if(BTSRV_TWS_MASTER == p->tws_role) {
            _tws_send_pkt(p, &p->sdm_info, TWS_SDM_INFO_PKT, 0);
        }

        printk("sdm Info, seq: %u, time: %u, sdm cnt: %u/%u, save: %u\n",
            p->sdm_info.pkt_seq, p->sdm_info.bttime_ms,
            p->sdm_info.sdm_cnt[0], p->sdm_info.sdm_cnt[1], p->sdm_info.sdm_cnt_save);
    }

#elif defined(CONFIG_SYNC_BY_SDM_CNT)

    if(p->sdm_valid > 0) {
        p->sdm_valid -= BT_TWS_LOOP_INTERVAL * 1000;
        if(p->sdm_valid <= 0) {
            p->irq_clk = UINT64_MAX;
            p->irq_clk_next = UINT64_MAX;
            p->peried_irq_set = 0;
            observer->set_period_interrupt_time(0, 0);
            printk("need resync clk\n");
        }
    }

    if(((BTSRV_TWS_MASTER == p->tws_role) || (BTSRV_TWS_SLAVE == p->tws_role))
        && (p->tws_status == PLAYER_TWS_STATUS_PLAY)
        && (p->irq_clk == p->irq_clk_next)){
        uint64_t bt_clk = observer->get_bt_clk_us();

        do{
            if(p->irq_clk == UINT64_MAX) {
                p->irq_clk_next = bt_clk + TWS_MIN_INTERVAL + 10000;
                break;
            }
            if(((p->irq_clk_next % TWS_ALIGN_INTERVAL) == 0)
                && (p->save_pinfo_count > 1)){
                p->sdm_info.bttime_ms = (uint32_t)(p->irq_clk/1000);
                p->sdm_info.sdm_cnt_save = p->sdm_cnt_saved;
                p->sdm_info.sdm_cnt[0] = p->pinfo_unit[p->pinfo_unit_idx].sdm_cnt;
                p->sdm_info.sdm_cnt[1] = p->pinfo_unit[(p->pinfo_unit_idx + 1) % 2].sdm_cnt;
                p->sdm_info.pkt_seq = fix_pkt_seq(p, p->pinfo_unit[p->pinfo_unit_idx].pkt_seq);

                if(BTSRV_TWS_MASTER == p->tws_role) {
                    _tws_send_pkt(p, &p->sdm_info, TWS_SDM_INFO_PKT, 0);
                }

                printk("sdm Info, seq: %u, time: %u, sdm cnt: %u/%u, save: %u\n",
                    p->sdm_info.pkt_seq, p->irq_clk,
                    p->sdm_info.sdm_cnt[0], p->sdm_info.sdm_cnt[1], p->sdm_info.sdm_cnt_save);
            }

            p->irq_clk_next = (bt_clk + TWS_CLK_SYNC_INTERVAL + 30000) / TWS_CLK_SYNC_INTERVAL * TWS_CLK_SYNC_INTERVAL;
        }while(0);

        if(!p->peried_irq_set) {
            if(p->irq_clk_next % TWS_CLK_SYNC_INTERVAL) {
                observer->set_interrupt_time(p->irq_clk_next);
            } else {
                observer->set_period_interrupt_time(p->irq_clk_next, TWS_CLK_SYNC_INTERVAL);
                p->peried_irq_set = 1;
            }

            printk("sync clk: %u, %u\n", (uint32_t)bt_clk, (uint32_t)p->irq_clk_next);
        }

        p->sdm_valid = 500000;
    }
#endif  //CONFIG_SYNC_BY_SDM_CNT
}

static void _bt_tws_loop(os_work *work)
{
    bt_tws_observer_inner_t *p = &observer_inner;
    tws_runtime_observer_t *observer = (tws_runtime_observer_t *)p->btsrv_observer;
    uint32_t flags;

    // printk("twss:%d\n", p->tws_status);
    if((p->tws_status == PLAYER_TWS_STATUS_NULL) || p->restarting)
        return;

    _check_tws_role_change(p, observer);
    _sync_bt_clk(p, observer);

    //handle remote msg
    while(acts_ringbuf_length(&p->tws_data_buffer) >= sizeof(player_tws_pkt_t)) {
        player_tws_pkt_t tws_pkt;

        acts_ringbuf_get(&p->tws_data_buffer, &tws_pkt, sizeof(tws_pkt));
        if(tws_pkt.format != p->format)
            continue;

        switch(tws_pkt.type) {
        case TWS_STARTPLAY_PKT:
            _tws_deal_startplay(p, &tws_pkt);
            break;
#ifdef CONFIG_SYNC_BY_SDM_CNT
        case TWS_SDM_INFO_PKT:
            if(p->tws_role == BTSRV_TWS_SLAVE)
                _tws_deal_sdm_info(p, &tws_pkt);
            break;
#else
        case TWS_SYNCINFO_PKT:
            if(p->tws_role == BTSRV_TWS_SLAVE)
                _tws_deal_syncinfo(p, &tws_pkt);
            break;
#endif
        case TWS_PLAYRATE_PKT:
            if(p->tws_role == BTSRV_TWS_SLAVE)
                _tws_deal_playrate(p, &tws_pkt);
            break;

        case TWS_USER_TYPE_PKT:
            tws_deal_pkt_user_type(p, &tws_pkt);
            break;

        default:
            SYS_LOG_ERR("recv invalid pkt, nego_seq: %d, type: %d, len: %d",
                tws_pkt.nego_seq, tws_pkt.type, tws_pkt.payload_len);
            break;
        }
    }

    //handle local msg
    while(acts_ringbuf_length(&p->msg_buffer) >= sizeof(player_tws_msg_t)) {
        player_tws_msg_t tws_msg;

        acts_ringbuf_get(&p->msg_buffer, &tws_msg, sizeof(tws_msg));

        switch(tws_msg.type) {
        case TWS_STREAM_INIT_PKT:
            _tws_handle_set_stream_info();
            break;
        case TWS_SYNCINFO_PKT:
            _tws_handle_new_pkt_info(p, (tws_pkt_info_t*)tws_msg.data);
            break;
        case TWS_PLAYRATE_PKT:
            _tws_handle_aps_change_request(p, tws_msg.data[0]);
            break;
        default:
            SYS_LOG_ERR("recv invalid msg, type: %d", tws_msg.type);
            break;
        }
    }

    if((p->aps_level_request >= 0) && !p->forbid_aps) {
        _tws_handle_aps_change_request(p, p->aps_level_request);
        p->aps_level_request = -1;
    }
    if(p->aps_level_pending >= 0) {
        uint64_t bt_clk = observer->get_bt_clk_us();
        uint64_t aps_inttime = p->aps_inttime + 10000;
        if(bt_manager_bt_clk_diff_us(bt_clk, aps_inttime) > 0) {
            media_set_base_aps_level(p->media_observer, p->aps_level_pending);
            SYS_LOG_INF("aps int no triger:%d", p->aps_level_pending);
            p->aps_level_pending = -1;
        }
    }
    if(p->forbid_aps && (cyc_to_us(k_cycle_get_32() - p->splay_aps_cyc) >= APS_FORBIT_TIME)) {
        p->forbid_aps = 0;
    }

#ifndef CONFIG_SYNC_BY_SDM_CNT
    if((p->tws_status == PLAYER_TWS_STATUS_PLAY) && (p->tws_role == BTSRV_TWS_SLAVE)) {
        _tws_align_samples(p);
    }
#endif

    //negotiation abnormal check (single device switch to tws device with TTS)
    if (p->tws_status == PLAYER_TWS_STATUS_WAIT) {
        int32_t temp_value = 0;
        temp_value = cyc_to_us(k_cycle_get_32() - p->splay_aps_cyc);

        // splay_reverse_pkts timeout, re-new splay seq info and drop data by playback
        if (p->splay_reverse_pkts > 0 && p->splay_reverse_timeout == 0 \
         && temp_value >= p->splay_reverse_pkts * p->pkt_time_us)
        {
            p->first_pkt_cyc += k_us_to_cyc_floor32(p->splay_reverse_pkts * p->pkt_time_us);
            p->splay_pktseq  += p->splay_reverse_pkts;
            p->first_stream_pkt = p->splay_pktseq;
            p->splay_reverse_timeout = 1;
            media_set_start_pkt_seq(p->media_observer, 0, p->splay_pktseq);
            printk("reverse_pkts timeout %d\n", p->splay_pktseq);
        }

        if (temp_value >= SEND_SPLAY_INTERVAL) {
            if((BTSRV_TWS_NONE == p->tws_role)
                || (cyc_to_us(k_cycle_get_32() - p->nego_start_time) >= p->WPlay_Maxtime)) {
                if(BTSRV_TWS_NONE == p->tws_role) {
                    printk("switch to tws none\n");
                } else {
                    printk("nego timeout\n");
                }

                _single_play_comfirm(p, _cac_wait_time(p, TWS_MIN_INTERVAL, 1));
            } else {
                _tws_handle_newly_splay(p);
            }
        }
    }
    //tws播放时，检查时间，提前30毫秒开始解码送数据；以及异常检查
    do {
        if((p->tws_status <= PLAYER_TWS_STATUS_WAIT)
            || (p->tws_status >= PLAYER_TWS_STATUS_PLAY)
            || (p->splay_local_time == UINT64_MAX)) {
            break;
        }

        if(BTSRV_TWS_NONE == p->tws_role) {
            p->tws_role = BTSRV_TWS_NONE;
            _single_play_comfirm(p, _cac_wait_time(p, TWS_MIN_INTERVAL, 1));
            break;
        }

        uint64_t bt_clk = observer->get_bt_clk_us();
        if(p->tws_status == PLAYER_TWS_STATUS_COMFIRM_SEQNO) {
            // -10000: 10ms reserved for playback to drop data, if needed
            uint64_t decode_time = bt_manager_bt_clk_fix_us(p->splay_local_time - TWS_MIN_INTERVAL - 10000);
            if(bt_manager_bt_clk_diff_us(bt_clk, decode_time) >= 0) {
                _deal_wait_timer_proc(NULL);  // start decoding and pcm streaming
            }
        } else if(p->tws_status == PLAYER_TWS_STATUS_PCM_STREAMING) {
            uint64_t play_time = bt_manager_bt_clk_fix_us(p->splay_local_time + 10000);
            if(bt_manager_bt_clk_diff_us(bt_clk, play_time) >= 0) {
                _deal_wait_timer_proc(NULL);  // start playback
            }
        }
    }while(0);

    // audiolcy_cmd_entry(LCYCMD_MAIN, NULL, 0);

    flags = irq_lock();
    if(p->tws_status != PLAYER_TWS_STATUS_NULL)
        os_delayed_work_submit(&p->loop_work, BT_TWS_LOOP_INTERVAL);
    irq_unlock(flags);
}

/*----------------------------------------------------------------------------------------------------*/
static uint64_t _bt_tws_get_play_time_us(void *handle)
{
    bt_tws_observer_inner_t *p = (bt_tws_observer_inner_t*)handle;

    return p->splay_local_time;
}

static uint64_t _bt_tws_get_bt_clk_us(void *handle)
{
    bt_tws_observer_inner_t *p = (bt_tws_observer_inner_t*)handle;

#if defined(CONFIG_SYNC_BY_SDM_CNT) && !defined(CONFIG_SOFT_SDM_CNT)
//lark在mcu频率低时，sdm cnt无法使用
    if((p->sdm_valid > 0) && (p->irq_clk != UINT64_MAX)) {
        uint32_t sdm_cnt = media_get_sdm_cnt_realtime(p->media_observer);
        return p->irq_clk
            + media_get_sdm_cnt_delta_time(p->media_observer, (int32_t)(sdm_cnt - p->sdm_cnt_saved));
    } else {
        //需提供精确的时钟用于计算包播放时间
        return UINT64_MAX;
    }
#elif defined(CONFIG_SOFT_SDM_CNT)
    uint64_t cur_time;
    uint64_t cur_time_bt;
    // volatile uint32_t *timer2_cnt = (uint32_t*)(T2_CNT);
    tws_runtime_observer_t *observer = (tws_runtime_observer_t *)p->btsrv_observer;

    cur_time_bt = observer->get_bt_clk_us();
    cur_time = bt_manager_bt_clk_fix_us(tws1_bt_clk /* + (*timer2_cnt - tws1_t2_cnt) / 24 */);

    if(abs((int32_t)(cur_time - cur_time_bt)) > 3000) {
        SYS_LOG_INF("wanning: use bt clk, %u, %u, %d, %d",
            (uint32_t)cur_time, (uint32_t)cur_time_bt,
            (int32_t)(cur_time - tws1_bt_clk),
            (int32_t)(cur_time - cur_time_bt));
        return cur_time_bt;
    } else {
        return cur_time;
    }
#else
    tws_runtime_observer_t *observer = (tws_runtime_observer_t *)p->btsrv_observer;
    return observer->get_bt_clk_us();
#endif
}

static uint8_t _bt_tws_get_role(void *handle)
{
    bt_tws_observer_inner_t *p = (bt_tws_observer_inner_t*)handle;
    return p->tws_role;
}

extern uint32_t get_sample_rate_hz(uint8_t fs_khz);
static int32_t _bt_tws_set_stream_info(void *handle,
    uint8_t format, uint8_t channels,
    uint16_t first_pktseq, uint32_t sample_rate,
    uint16_t cache_time, uint32_t pkt_time_us, uint64_t play_time)
{
    bt_tws_observer_inner_t *p = (bt_tws_observer_inner_t*)handle;
    player_tws_msg_t tws_msg;

    if((p->tws_status != PLAYER_TWS_STATUS_NULL)
        && (p->tws_status != PLAYER_TWS_STATUS_STREAM_ABNORMAL)){
        SYS_LOG_ERR("bt tws status: %d", p->tws_status);
        return -1;
    }

    p->first_pkt_cyc = k_cycle_get_32() - k_us_to_cyc_floor32(pkt_time_us);
    p->format = format;
    p->channels = channels;
    p->splay_pktseq = first_pktseq;
    p->sample_rate = sample_rate;
    p->cache_time = cache_time;   // target latency ms
    p->pkt_time_us = pkt_time_us;
    p->tws_status = PLAYER_TWS_STATUS_INIT;

    // printk("twss2:%d\n", p->tws_status);

#ifdef CONFIG_SOFT_SDM_CNT
    p->sample_time_ns = 1000000000 / get_sample_rate_hz(audio_system_get_output_sample_rate());//to do
#endif

    if(0 == p->nego_start_time) {
        //set default frame_samples, will be modified after first frame decode out
        switch(format) {
        case AAC_TYPE:
            p->frame_samples = 1024;
            break;
        case SBC_TYPE:
            p->frame_samples = 128;
            break;
        case MSBC_TYPE:
            p->frame_samples = 120;
            break;
        case CVSD_TYPE:
            p->frame_samples = 60;
            break;
        case LDAC_TYPE:
            p->frame_samples = p->sample_rate > 48000 ? 256 : 128;
            break;
        default:
            break;
        }
    }

    p->nego_start_time = k_cycle_get_32();
    tws_msg.type = TWS_STREAM_INIT_PKT;
    acts_ringbuf_put(&p->msg_buffer, &tws_msg, sizeof(tws_msg));

    os_delayed_work_submit(&p->loop_work, 0);
    return 0;
}

static int32_t _bt_tws_aps_change_request(void *handle, uint8_t level)
{
    bt_tws_observer_inner_t *p = (bt_tws_observer_inner_t*)handle;

    if((p->tws_role == BTSRV_TWS_SLAVE)
        || (p->tws_status != PLAYER_TWS_STATUS_PLAY)
        || (p->aps_level_pending == level)
        || (p->aps_level_request == level)
        || p->restarting)
        return 0;
    if(p->tws_role == BTSRV_TWS_NONE)
        goto ERROUT;

    p->aps_level_request = level;
    return 0;

ERROUT:
    media_set_base_aps_level(p->media_observer, level);
    return 0;
}

static int32_t _bt_tws_set_pkt_info(void *handle, tws_pkt_info_t *info)
{
    bt_tws_observer_inner_t *p = (bt_tws_observer_inner_t*)handle;
    player_tws_msg_t tws_msg;

    if((p->tws_status == PLAYER_TWS_STATUS_NULL) || p->restarting)
        return -1;
    if(p->tws_status == PLAYER_TWS_STATUS_STREAM_ABNORMAL) {
        _bt_tws_set_stream_info(handle, p->format, p->channels, info->pkt_seq, p->sample_rate, p->cache_time, p->pkt_time_us, 0);
    }

#ifdef CONFIG_SYNC_BY_SDM_CNT
    if((info->samples != 0xFFFFU) && (p->sdm_valid > 0) && (p->irq_clk != UINT64_MAX)) {
        uint32_t flags = irq_lock();

        //uint64_t bt_clk = info->pkt_bttime_us;
        info->sdm_cnt &= SDM_CNT_MASK;
        info->pkt_bttime_us = p->irq_clk
            + media_get_sdm_cnt_delta_time(p->media_observer, (int32_t)(info->sdm_cnt - p->sdm_cnt_saved));
        //printk("bt diff: %d, %u, %u\n", (int32_t)(bt_clk - info->pkt_bttime_us), info->sdm_cnt, p->sdm_cnt_saved);
        irq_unlock(flags);
    } else {
        info->pkt_bttime_us = UINT64_MAX;
    }
#endif

    tws_msg.type = TWS_SYNCINFO_PKT;
    memcpy(tws_msg.data, info, sizeof(tws_pkt_info_t));

    if(acts_ringbuf_space(&p->msg_buffer) < sizeof(player_tws_msg_t)) {
        SYS_LOG_ERR("msg buffer full");
        return -1;
    }

    if(info->samples == 0xFFFFU) {
        uint32_t flags = irq_lock();
        p->buffer_time_cyc = k_cycle_get_32();
        p->frame_cnt_tail += info->frame_cnt;
        irq_unlock(flags);
    }
    acts_ringbuf_put(&p->msg_buffer, &tws_msg, sizeof(tws_msg));
    return 0;
}

static int32_t _bt_tws_trigger_restart(void *handle, uint8_t reason)
{
    bt_tws_observer_inner_t *p = (bt_tws_observer_inner_t*)handle;

    if(p->restarting) {
        return 0;
    }

    printk("request restart player\n");
    p->restarting = 1;
    return bt_manager_event_notify(BT_REQ_RESTART_PLAY, NULL, 0);
}

static int32_t _bt_tws_on_buffer_change(void *handle, int32_t pcm_time)
{
    //bt_tws_observer_inner_t *p = (bt_tws_observer_inner_t*)handle;

    //Inaccurate at startup
    //p->buffer_time_cyc = k_cycle_get_32() - k_us_to_cyc_floor32(pcm_time);
    return 0;
}

static void _bt_tws_observer_deinit(void *handle)
{
    bt_tws_observer_inner_t *p = (bt_tws_observer_inner_t*)handle;
    tws_runtime_observer_t *observer;
    uint32_t flags;

    if(NULL == p->media_observer)
        return;

    flags = irq_lock();
    p->aps_level_pending = -1;
    p->aps_level_request = -1;
    p->tws_status = PLAYER_TWS_STATUS_NULL;
    irq_unlock(flags);

    k_delayed_work_cancel_sync(&p->loop_work,K_FOREVER);
    os_timer_stop(&p->deal_wait_timer);

    observer = (tws_runtime_observer_t *)p->btsrv_observer;
    observer->set_interrupt_time(0);
    observer->set_interrupt_cb(NULL);
    observer->set_recv_pkt_cb(NULL);
#ifdef CONFIG_SOFT_SDM_CNT
    //observer->set_twssrv_period_irq_cb(NULL);
#elif defined(CONFIG_SYNC_BY_SDM_CNT)
    observer->set_period_interrupt_time(0, 0);
#endif
}

bt_tws_observer_t* bt_tws_observer_init(media_observer_t *media_observer, void *btsrv_observer)
{
    bt_tws_observer_inner_t *p = &observer_inner;
    tws_runtime_observer_t *observer = (tws_runtime_observer_t *)btsrv_observer;

    memset(p, 0, sizeof(bt_tws_observer_inner_t));

    acts_ringbuf_init(&p->tws_data_buffer, p->tws_data, sizeof(p->tws_data));
    acts_ringbuf_init(&p->msg_buffer, p->msg_data, sizeof(p->msg_data));
    acts_ringbuf_init(&p->stream_pkt_buffer, p->stream_pkt_data, sizeof(p->stream_pkt_data));
#ifndef CONFIG_SYNC_BY_SDM_CNT
    acts_ringbuf_init(&p->sinfo_master_buffer, p->sinfo_master, sizeof(p->sinfo_master));
    acts_ringbuf_init(&p->sinfo_slave_buffer, p->sinfo_slave, sizeof(p->sinfo_slave));
#endif

    p->media_observer = media_observer;
    p->btsrv_observer = btsrv_observer;
    p->tws_observer.get_play_time_us = _bt_tws_get_play_time_us;
    p->tws_observer.get_bt_clk_us = _bt_tws_get_bt_clk_us;
    p->tws_observer.get_role = _bt_tws_get_role;
    p->tws_observer.set_stream_info = _bt_tws_set_stream_info;
    p->tws_observer.aps_change_request = _bt_tws_aps_change_request;
    p->tws_observer.set_pkt_info = _bt_tws_set_pkt_info;
    p->tws_observer.trigger_restart = _bt_tws_trigger_restart;
    p->tws_observer.on_buffer_change = _bt_tws_on_buffer_change;
    p->tws_observer.deinit = _bt_tws_observer_deinit;

    p->tws_status = PLAYER_TWS_STATUS_NULL;
    p->aps_level_pending = -1;
    p->aps_level_request = -1;
    p->pinfo_seq_pendding = -1;
    p->tws_role = observer->get_role();
    p->splay_local_time = UINT64_MAX;
    p->irq_clk = UINT64_MAX;
    p->irq_clk_next = UINT64_MAX;
    os_delayed_work_init(&p->loop_work, _bt_tws_loop);
    os_timer_init(&p->deal_wait_timer, _deal_wait_timer_proc, NULL);

    observer->set_interrupt_cb(_tws_irq_handle);
    observer->set_recv_pkt_cb(_tws_recv_pkt_handle);
#ifdef CONFIG_SOFT_SDM_CNT
    observer->set_twssrv_period_irq_cb(_tws1_irq_handle);
#endif

    // audiolcy_cmd_entry(LCYCMD_SET_TWSROLE, (void *)((u32_t)p->tws_role), 4);

    return &p->tws_observer;
}

#ifdef TWS1_TEST2
static void _bt_tws1_test_work(os_work *work)
{
    if(tws1_test_clk_valid) {
        tws1_test_clk_valid = 0;

        tws1_test_clk_bak[tws1_test_clk_bak_cnt] = tws1_bt_clk;
        tws1_test_clk_bak_cnt++;
    }

    tws1_test_work_cnt++;
    if((tws1_test_clk_bak_cnt == 10) || (tws1_test_work_cnt > 20)) {
        int32_t i;

        printk("tws1 clk %d: ", tws1_test_clk_bak_cnt);
        for(i=0; i<tws1_test_clk_bak_cnt; i++) {
            printk("%u_%u ", (uint32_t)(tws1_test_clk_bak[i] >> 32), (uint32_t)tws1_test_clk_bak[i]);
        }
        printk("\n");

        tws1_test_clk_bak_cnt = 0;
        tws1_test_work_cnt = 0;
    }

    os_delayed_work_submit(&tws1_test_work, 100);
}
#endif

void bt_tws_observer_pre_init(void *btsrv_observer)
{
    bt_tws_observer_inner_t *p = &observer_inner;

    memset(p, 0, sizeof(bt_tws_observer_inner_t));

#ifdef CONFIG_SOFT_SDM_CNT
    tws_runtime_observer_t *observer = (tws_runtime_observer_t *)btsrv_observer;

#ifdef TWS1_TEST2
    os_delayed_work_init(&tws1_test_work, _bt_tws1_test_work);
    os_delayed_work_submit(&tws1_test_work, 20);
#endif

    observer->set_twssrv_period_irq_cb(_tws1_irq_handle);
#endif
}

