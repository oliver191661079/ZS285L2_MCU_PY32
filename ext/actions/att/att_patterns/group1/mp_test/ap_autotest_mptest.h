#ifndef __AP_AUTOTEST_MPTEST_H
#define __AP_AUTOTEST_MPTEST_H

#include "mp_app.h"
#include <compensation.h>

//Buffer rx CONTROLLER
#define ATT_RINGBUFFER_ADDR  STUB_BUFFER_TEST_ITEM_ADDR
#define ATT_RINGBUFFER_LEN   16 * 1024

#define INVALID_CFO_VAL         (0x7fff)
#define INVALID_PWR_VAL         (0x7fff)
#define INVALID_RSSI_VAL         (0x7fff)

#define MAX_CFO_DIFF_VAL        (5)

#define MAX_RSSI_DIFF_VAL        (10)

#define TEST_RETRY_NUM      (3)

#define MIN_RESULT_NUM          (10)

#define CFO_RESULT_NUM          (16)

typedef enum
{
    BT_PAYLOAD_TYPE_ALL0 = 0,
    BT_PAYLOAD_TYPE_ALL1 = 1,
    BT_PAYLOAD_TYPE_0101 = 2,
    BT_PAYLOAD_TYPE_1010 = 3,
    BT_PAYLOAD_TYPE_0x0_0xF = 4,
    BT_PAYLOAD_TYPE_0000_1111 = 5,
    BT_PAYLOAD_TYPE_1111_0000 = 6,
    BT_PAYLOAD_TYPE_PRBS9 = 7,
    //////////////////////////////////
    BT_PAYLOAD_TYPE_NUM = 8
} BT_PAYLOAD_TYPE;

typedef enum
{
    BT_PKT_1DH1 = 0,
    BT_PKT_1DH3,
    BT_PKT_1DH5,
    BT_PKT_2DH1,
    BT_PKT_2DH3,
    BT_PKT_2DH5,
    BT_PKT_3DH1,
    BT_PKT_3DH3,
    BT_PKT_3DH5,
    BT_PKT_LE,
    //////////////////////////////////////////////////
    BT_PKT_TYPE_NULL,
    BT_PKT_TYPE_NUM
} BT_PKT_TYPE;

typedef struct
{
    uint16 read_ptr;
    uint16 write_ptr;
    uint16 readable_len;
    uint8 *read_buffer;
} ringbuf_rw_t;

typedef enum
{
    UART_HCI_STATUS_RX_NULL = 0,
    UART_HCI_STATUS_RX_TYPE,
    UART_HCI_STATUS_RX_HEADER,
    UART_HCI_STATUS_RX_DATA
}rev_pkt_state_e;


typedef struct
{
    uint16 (*get_data_len)(void);
    void (*read_data)(uint32 buf_addr, uint32 data_len);
    uint16 minReqLen;
    uint16 pktHdrLen;
    uint8 pktType;
    rev_pkt_state_e reqRxParseStatus;
    uint8 headerBuffer[32];
} btt_hci_deal_t;

typedef struct
{
    uint8 pkt_index;
    uint8 total_node;
    uint16 next_pkt_offset;
} cmd_pkt_head_t;

typedef struct
{
    uint8 cmd_len;
    uint8 cmd_data[0];
} cmd_node_t;

typedef struct
{
    cmd_pkt_head_t pkt_head;
    cmd_node_t cmd_node[0];
} cmd_pkt_t;

typedef enum
{
    PKT_CFO_BEGINE_INDEX = 0, PKT_CFO_STOP_INDEX = 1, PKT_CFO_UPDATE_INDEX = 2,
} pkt_index_e;

typedef struct
{
    uint8 ic_type;
    uint8 channel;
    uint8 tx_gain_idx;
    uint8 tx_gain_val;
    uint8 payload;
    uint8 pkt_type;
    uint8 tx_dac;
    uint8 whitening_cv;
    uint16 pkt_header;
    //uint16 tx_pkt_cnt;
    uint8 reserved0[2];
    uint32 hit_target_l;
    uint32 hit_target_h;
    uint8 sut_state; //0:default begin 1:continue 2:stop
    uint8 report_interval; //milli-second
    uint8 skip_report_count;
    uint8 once_report_pkts;
    uint8 report_timeout;
    uint8 recv_cfo_count;
    //uint8 sut_download_patch;
    uint8 reserved[6];
} cfo_param_t;

typedef struct
{
    stub_ext_cmd_t ext_cmd;
    cfo_param_t cfo_param;
} cfo_test_param_t;

typedef struct
{
    int8 cfo_index_low;
    int8 cfo_index_high;
    uint16 cfo_index;
    int16 rssi_val;
    int32 cfo_val;
    int32 pwr_val;
    uint32 ber_val;
} cfo_return_arg_t;


struct mp_test_stru
{
    uint32 mp_cap;
    struct k_sem mp_sem;
    int receive_cfo_flag;
    int16 cfo_val[CFO_RESULT_NUM];
    int16 rssi_val[CFO_RESULT_NUM];
    uint32 ber_val;
    int16 a_rssi_val;
};

typedef struct
{
    uint8 mp_type;  //0-5119  1-5120
    uint8 sdk_type;
    uint8 rf_channel;
	uint8 rf_power;
	uint8 packet_type;
	uint8 payload_type;
    uint16 payload_len;
    uint16 timeout; //milli-second
    uint8 reserved[6];
}bttools_mp_param_t;

typedef uint32 (*modify_cb_func_t)(uint8 *hci_data, int index, int channel);

#define MP_ICTYPE                           1 //MP 0:5119   1:5120
#define MP_TX_GAIN_IDX                      7
//#define MP_TX_GAIN_VAL                      0xcc
#define MP_TX_GAIN_VAL                      0xce
#define MP_TX_DAC                           0x13
#define HIT_ADDRESS_SET_L                   0x009e8b33
#define HIT_ADDRESS_SET_H                   0
#define PKTTYPE_SET		                    K_MP_00001111
#define PKTHEADER_SET                       0x1234
#define WHITENCOEFF_SET	                    0x7F                    //(TX)   --->FF (RX)
#define PAYLOADTYPE_SET                     MP_PKT_HV1//BT_PAYLOAD_TYPE_PRBS9
#define MP_REPORT_RX_INTERVAL               25
#define MP_SKIP_PKT_NUM                     0
#define MP_ONCE_REPORT_MIN_PKT_NUM          8
#define MP_RETURN_RESULT_NUM                10
#define MP_REPORT_TIMEOUT                   2
#define BER_TX_PKT_CNT                      0xffff
#define BER_TX_REPORT_INTERVAL              1
#define BER_TX_REPORT_TIMEOUT               4

#define PAYLOAD_LEN           27
#define RF_POWER               10

#define CFO_THRESHOLD_LEFT  0
#define CFO_THRESHOLD_RIGHT (240)
#define CFO_THRESHOLD_ADJUST_VAL  5

#define SEARCH_LEFT_FLAG    0x01
#define SEARCH_RIGHT_FLAG   0x02


extern void mp_task_tx_start(uint8 channel);
extern void mp_task_tx_stop(void);
extern void mp_exit(void);
extern int mp_init_sync(void);
extern void mp_task_stop_sync(void);
extern void mp_task_rx_stop(void);
extern void mp_task_rx_start(uint8 channel, mp_msg_callback_func cbk, uint32_t timeout);

extern int read_trim_cap_efuse(uint32 mode);
extern uint32 mp_get_hosc_cap(void);
extern void mp_set_hosc_cap(uint32 cap);
extern int mp_cfo_test(uint8 channel, uint32 cap, int16 *cfo);
extern bool att_bttool_tx_begin(uint32 channel);
extern bool att_bttool_tx_stop(void);
extern bool att_ber_calc_result(int16 * data_buffer,uint32 result_num, int16 *rssi_return);
extern bool att_cfo_result_calc(int16 *data_buffer, uint32 result_num, int16 *cfo_return);
extern bool att_mp_cfo_test(mp_test_arg_t *mp_arg, uint32 channel, int16 ref_cap, int16 *return_cap);

#endif

