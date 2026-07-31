#ifndef _BROADCAST_H_
#define _BROADCAST_H_

#if CONFIG_EXTERNAL_DSP_DELAY > 0
#define EXTERNAL_DSP_DELAY         (CONFIG_EXTERNAL_DSP_DELAY)
#define BCST_QOS_DELAY             (14000 + CONFIG_EXTERNAL_DSP_DELAY)
#else
#define EXTERNAL_DSP_DELAY         (0)
#define BCST_QOS_DELAY             (12000)
#endif

#ifdef CONFIG_AURACAST
#define ENABLE_BROADCAST 1
#define ENABLE_PADV_APP	1
#endif

#ifdef CONFIG_BT_PAWR
#define ENABLE_PAWR_APP	1
#endif

#define FILTER_LOCAL_NAME	0

#ifndef CONFIG_AUARCAST_FILTER_POLICY_H

#define FILTER_BROADCAST_NAME	1
#define FILTER_BROADCAST_ID	1

#else

#define FILTER_BROADCAST_NAME	0
#define FILTER_BROADCAST_ID	0

#endif

#define BIS_SYNC_LOCATIONS	1	/* NOT support encryption yet */
#if BIS_SYNC_LOCATIONS
#define ENABLE_ENCRYPTION	0
#else
#define ENABLE_ENCRYPTION	1
#endif


#define COMPANY_ID	CONFIG_BT_COMPANY_ID


#define BROADCAST_NUM_BIS  2
#define SERIVCE_UUID	0xFDDF

/* 0: VS with SERIVCE_UUID; 1: VS with COMPANY_ID */
#define VS_COMPANY_ID	1
#if VS_COMPANY_ID
#define VS_ID	COMPANY_ID
#define FILTER_COMPANY_ID	1
#define FILTER_UUID16	0
#else
#define VS_ID	SERIVCE_UUID
#define FILTER_COMPANY_ID	0
#define FILTER_UUID16	1
#endif

#define SCAN_FILTER_BY_USER	0

#define NUM_OF_BROAD_CHAN  2

/*
long lasting stereo protocol
*/
#define COMMAND_IDENTIFIER         0xAA
#define COMMAND_DEVACK             0x00

#define COMMAND_SETKEYEVENT        0x31
#define COMMAND_REQ_PASTINFO       0xf1

enum {
    KEY_EVENT_NONE = 0,
    KEY_EVENT_PLAY,
	KEY_EVENT_PAUSE,
	KEY_EVENT_VOLUME_UP,
	KEY_EVENT_VOLUNE_DOWN,
};


struct broadcast_param_t {
	uint16_t pa_interval;
	uint32_t sdu_interval;
	uint16_t kbps;
	uint16_t sdu;
	uint16_t pdu;
	uint8_t octets;
	uint8_t audio_chan;
	uint8_t iso_interval;
	uint8_t nse;
	uint8_t bn;
	uint8_t irc;
	uint8_t pto;
	uint8_t latency;
	uint8_t duration;
};

/* 96kbp 1ch 7.5ms iso_interval */
#if 0
#define BROADCAST_KBPS	96
#define BROADCAST_SDU	90
#define BROADCAST_OCTETS	90
#define BROADCAST_CH	1
#define BROADCAST_ISO_INTERVAL	6
#define BROADCAST_SDU_INTERVAL	7500
#define BROADCAST_PDU	90
#define BROADCAST_NSE	3
#define BROADCAST_BN	1
#define BROADCAST_IRC	3
#define BROADCAST_PTO	0
#define BROADCAST_LAT	8
#define BROADCAST_DURATION	BT_FRAME_DURATION_7_5MS
#define PA_INTERVAL	72	// 90ms
#endif



/* 96kbp 1ch 15ms iso_interval */
#if 0
#define BROADCAST_KBPS	96
#define BROADCAST_SDU	90
#define BROADCAST_OCTETS	90
#define BROADCAST_CH	1
#define BROADCAST_ISO_INTERVAL	12
#define BROADCAST_SDU_INTERVAL	7500
#define BROADCAST_PDU	90
#define BROADCAST_NSE	6
#define BROADCAST_BN	2
#define BROADCAST_IRC	3
#define BROADCAST_PTO	0
#define BROADCAST_LAT	15
#define BROADCAST_DURATION	BT_FRAME_DURATION_7_5MS
#define PA_INTERVAL	72	// 90ms
#endif



/* 96kbp 2ch 7.5ms iso_interval */
#if 0
#define BROADCAST_KBPS	192
#define BROADCAST_SDU	180
#define BROADCAST_OCTETS	90
#define BROADCAST_CH	2
#define BROADCAST_ISO_INTERVAL	6
#define BROADCAST_SDU_INTERVAL	7500
#define BROADCAST_PDU	180
#define BROADCAST_NSE	3
#define BROADCAST_BN	1
#define BROADCAST_IRC	3
#define BROADCAST_PTO	0
#define BROADCAST_LAT	8
#define BROADCAST_DURATION	BT_FRAME_DURATION_7_5MS
#define PA_INTERVAL	72	// 90ms
#endif



/* 96kbp 2ch 15ms iso_interval */
#if 0
#define BROADCAST_KBPS	192
#define BROADCAST_SDU	180
#define BROADCAST_OCTETS	90
#define BROADCAST_CH	2
#define BROADCAST_ISO_INTERVAL	12
#define BROADCAST_SDU_INTERVAL	7500
#define BROADCAST_PDU	180
#define BROADCAST_NSE	6
#define BROADCAST_BN	2
#define BROADCAST_IRC	3
#define BROADCAST_PTO	0
#define BROADCAST_LAT	15
#define BROADCAST_DURATION	BT_FRAME_DURATION_7_5MS
#define PA_INTERVAL	72	// 90ms
#endif



/* 96kbp 1ch 20ms iso_interval */
#if 0
#define BROADCAST_KBPS	96
#define BROADCAST_SDU	120
#define BROADCAST_OCTETS	120
#define BROADCAST_CH	1
#define BROADCAST_ISO_INTERVAL	16
#define BROADCAST_SDU_INTERVAL	10000
#define BROADCAST_PDU	120
#define BROADCAST_NSE	6
#define BROADCAST_BN	2
#define BROADCAST_IRC	3
#define BROADCAST_PTO	0
#define BROADCAST_LAT	20
#define BROADCAST_DURATION	BT_FRAME_DURATION_10MS
#define PA_INTERVAL	80	// 100ms

#ifdef CONFIG_BT_PAWR
#define PAwR_INTERVAL	80	// 100ms
#define PAwR_SUB_INTERVAL	32	// 40ms
#define PAwR_RSP_DELAY	9	// 11.25ms

#endif
#endif


/* 80kbp 1ch 20ms iso_interval */
#if 1
#define BROADCAST_KBPS	80
#define BROADCAST_SDU	100
#define BROADCAST_OCTETS	100
#define BROADCAST_CH	1
#define BROADCAST_ISO_INTERVAL	16
#define BROADCAST_SDU_INTERVAL	10000
#define BROADCAST_PDU	100

#ifdef CONFIG_SRRC_TEST
#define BROADCAST_NSE	12
#else
#define BROADCAST_NSE	8
#endif

#define BROADCAST_BN	2

#ifdef CONFIG_SRRC_TEST
#define BROADCAST_IRC	6
#else
#define BROADCAST_IRC	4
#endif

#define BROADCAST_1_IRC 6

#define BROADCAST_PTO	0
#define BROADCAST_LAT	20
#define BROADCAST_DURATION	BT_FRAME_DURATION_10MS

#ifdef CONFIG_SRRC_TEST
#define PA_INTERVAL	80*3
#else
#define PA_INTERVAL	80	// 100ms
#endif

#ifdef CONFIG_BT_PAWR
#define PAwR_INTERVAL	40//80//100ms
#define PAwR_INTERVAL_MAX	48//80	// 100ms
#define PAwR_SUB_INTERVAL	16//32 // 40ms
#define PAwR_RSP_DELAY	9	// 11.25ms
#endif

#endif


/* 96kbp 1ch 10ms iso_interval */
#if 0
#define BROADCAST_KBPS	96
#define BROADCAST_SDU	120
#define BROADCAST_OCTETS	120
#define BROADCAST_CH	1
#define BROADCAST_ISO_INTERVAL	8
#define BROADCAST_SDU_INTERVAL	10000
#define BROADCAST_PDU	120

#ifndef CONFIG_BT_PAWR
#define BROADCAST_NSE	3
#define BROADCAST_IRC	3
#else
#define BROADCAST_NSE	2
#define BROADCAST_IRC	2
#endif

#define BROADCAST_BN	1

#define BROADCAST_PTO	0
#define BROADCAST_LAT	10
#define BROADCAST_DURATION	BT_FRAME_DURATION_10MS
#define PA_INTERVAL	80	// 100ms

#ifdef CONFIG_BT_PAWR
#define PAwR_INTERVAL	80	// 100ms
#define PAwR_SUB_INTERVAL	32	// 40ms
#define PAwR_RSP_DELAY	5	// 6.25ms

#endif

#endif



/* 96kbp 2ch 20ms iso_interval */
#if 0
#define BROADCAST_KBPS	192
#define BROADCAST_SDU	240
#define BROADCAST_OCTETS	120
#define BROADCAST_CH	2
#define BROADCAST_ISO_INTERVAL	16
#define BROADCAST_SDU_INTERVAL	10000
#define BROADCAST_PDU	240
#define BROADCAST_NSE	6
#define BROADCAST_BN	2
#define BROADCAST_IRC	3
#define BROADCAST_PTO	0
#define BROADCAST_LAT	20
#define BROADCAST_DURATION	BT_FRAME_DURATION_10MS
#define PA_INTERVAL	80	// 100ms
#endif



/* 96kbp 2ch 10ms iso_interval */
#if 0
#define BROADCAST_KBPS	192
#define BROADCAST_SDU	240
#define BROADCAST_OCTETS	120
#define BROADCAST_CH	2
#define BROADCAST_ISO_INTERVAL	8
#define BROADCAST_SDU_INTERVAL	10000
#define BROADCAST_PDU	240
#define BROADCAST_NSE	3
#define BROADCAST_BN	1
#define BROADCAST_IRC	3
#define BROADCAST_PTO	0
#define BROADCAST_LAT	10
#define BROADCAST_DURATION	BT_FRAME_DURATION_10MS
#define PA_INTERVAL	80	// 100ms
#endif



/* 96kbp 2ch 30ms iso_interval */
#if 0
#define BROADCAST_KBPS	192
#define BROADCAST_SDU	240
#define BROADCAST_OCTETS	120
#define BROADCAST_CH	2
#define BROADCAST_ISO_INTERVAL	24
#define BROADCAST_SDU_INTERVAL	10000
#define BROADCAST_PDU	240
#define BROADCAST_NSE	9
#define BROADCAST_BN	3
#define BROADCAST_IRC	3
#define BROADCAST_PTO	0
#define BROADCAST_LAT	40
#define BROADCAST_DURATION	BT_FRAME_DURATION_10MS
#define PA_INTERVAL	72	// 90ms
#endif



/* 96kbp 1ch 30ms iso_interval */
#if 0
#define BROADCAST_KBPS	96
#define BROADCAST_SDU	120
#define BROADCAST_OCTETS	120
#define BROADCAST_CH	1
#define BROADCAST_ISO_INTERVAL	24
#define BROADCAST_SDU_INTERVAL	10000
#define BROADCAST_PDU	120
#define BROADCAST_NSE	9
#define BROADCAST_BN	3
#define BROADCAST_IRC	3
#define BROADCAST_PTO	0
#define BROADCAST_LAT	40
#define BROADCAST_DURATION	BT_FRAME_DURATION_10MS
#define PA_INTERVAL	72	// 90ms

#ifdef CONFIG_BT_PAWR
#define PAwR_INTERVAL	72	// 90ms
#define PAwR_SUB_INTERVAL	24	// 30ms
#define PAwR_RSP_DELAY	12	// 15ms

#endif

#endif

/* 80kbp 1ch 30ms iso_interval */
#if 0
#define BROADCAST_KBPS	80
#define BROADCAST_SDU	100
#define BROADCAST_OCTETS	100
#define BROADCAST_CH	1
#define BROADCAST_ISO_INTERVAL	24
#define BROADCAST_SDU_INTERVAL	10000
#define BROADCAST_PDU	100
#define BROADCAST_NSE	9
#define BROADCAST_BN	3
#define BROADCAST_IRC	3
#define BROADCAST_PTO	0
#define BROADCAST_LAT	40
#define BROADCAST_DURATION	BT_FRAME_DURATION_10MS
#define PA_INTERVAL	72	// 90ms

#ifdef CONFIG_BT_PAWR
#define PAwR_INTERVAL	72	// 90ms
#define PAwR_SUB_INTERVAL	24	// 30ms
#define PAwR_RSP_DELAY	12	// 15ms

#endif

#endif

char *broadcast_get_local_name(void);
char *broadcast_get_broadcast_name(void);
int broadcast_get_broadcast_id(void);
int broadcast_get_bis_link_delay(struct bt_broadcast_qos *qos);
int broadcast_get_bis_link_delay_ext(struct bt_broadcast_qos *qos, uint8_t iso_interval);
int broadcast_get_tws_sync_offset(struct bt_broadcast_qos *qos);
void broadcast_set_tws_broadcast_name(uint8_t *name);
struct bt_broadcast_source_create_param * broadcast_init_source_param(void);
void broadcast_free_source_param(struct bt_broadcast_source_create_param *param);

#ifdef ENABLE_PADV_APP
int padv_tx_init(u32_t handle, u8_t stream_type);
int padv_tx_deinit(void);
int padv_tx_data(u8_t vol100);
u8_t padv_volume_map(u8_t vol, u8_t to_adv);
#endif
bool bmr_any_get(void);

#endif
