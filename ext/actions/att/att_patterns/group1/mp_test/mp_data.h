//////////////////////////////////////////////////////////////////
// packet_info.h
// Version : v1.0
// Update :
//////////////////////////////////////////////////////////////////
#ifndef _MP_DATA_H_
#define _MP_DATA_H_

#include "modem.h"

#define MP_RECEIVE_PACKET_TIMEOUT      5000
#define MP_RECEIVE_BUFFER_SIZE          16

enum
{
	MSG_MP_KEY_TXPACKET,
	MSG_MP_KEY_RXPACKET,
	MSG_MP_KEY_SINGLETONE,
	MSG_MP_KEY_ADDCAP,
    MSG_MP_KEY_DOWNCAP,
    MSG_MP_KEY_END,
};


enum
{
	MSG_MP_DATA_READY = MSG_MP_KEY_END + 1,
    MSG_MP_TASK_END ,
};


enum
{
	MP_CORE_STATE_NONE,
	MP_CORE_SEND_PACKET,
	MP_CORE_RECEIVE_PACKET,
	MP_CORE_SINGLE_TONE,
};

enum
{
    MP_RX_RESULT_SUCCESS,
	MP_RX_RESULT_TIMEOUT,
};

enum APP_MP_CallbackEventCodeEnum
{
    MP_INIT_COMPLETE_EVENT = 1,
    MP_RX_PACKET_VALID = 2,
};

typedef struct
{
   uint8 channel;
   uint8 power;
   uint8 pkt_type;
   uint8 payload_type;
   uint8 payload_len;
   uint8 reserve[3];
   uint32 timeout;
   uint32 packets;
}mp_packet_ctrl_t;



typedef struct
{
   uint16 pkt_len;
   uint16 payload_len;

   uint8 pkt_type;
   uint8 payload_data_type;

   uint8 rf_channel;
   uint8 rf_power_index;

 //  uint32 rx_data_bit_error_cnt;

}mp_packet_info_t;

typedef void (*mp_msg_callback_func)(uint8_t event, void *param);

typedef struct
{
    uint8 mp_core_state;
    uint8 mp_thread_exit;
    uint8 mp_task_running;
    uint8 mp_task_end;
    uint8 mp_thread_sleep_time;
    uint8 mp_tx_packet_step;
    uint8 mp_rx_packet_step;
    uint8 mp_single_tone_step;

    uint8 mp_rx_packet_running;
    uint8 mp_rx_result;
    uint8 mp_cap_trim;
    uint8 mp_rx_data_ready;
    uint8 mp_valid_packets_cn;

    uint16 mp_timeout_cn;

    uint32 mp_timeout_total;
    uint32 mp_run_time_start;
    uint32 mp_rx_time_start;

    uint32 mp_packets_total;
    uint32 mp_packets_cn;
    uint32 mp_cap_val;
  //  mp_packet_info_t npacket_send_info;
  //  mp_packet_info_t npacket_receive_info;
    mp_packet_info_t mp_packet_info;

    mp_msg_callback_func mp_event_callback;

    int16 rssi_val[16];
    int16 cfo_val[16];
    int16 rssi_tmp[8];
    int16 cfo_tmp[8];
    uint32 ber_val;
    uint32 ber_tmp;
}mp_manager_info_t;

#endif

