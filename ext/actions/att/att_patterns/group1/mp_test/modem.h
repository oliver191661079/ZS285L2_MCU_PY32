
//GL5119 v0.1 hushifei 2017/2/7
//GL5119 v0.2 hushifei 2017/2/17
//#include "common.h"
//#include "GL5119_Design_Specification.h"
//#include "GL5119 Bluetooth Modem Design Specification_v0.34.h"

#ifndef _MODEM_MP_H_
#define _MODEM_MP_H_

//enable Modem debug mode to dump iq data
//note:DMA clock should >=72MHz for Debug
//#define Debug_Mode_EN

#ifdef Debug_Mode_EN
#define K_DrqSel  2 //0:fifo>8; 1:rssi && fifo>8; 2 packet_sync && fifo>8
#define K_RSSI_TH 0
#define K_DebugModeTX  0x10 //0424 zuihia 01>10//0x00:dma read ADC iq for debug;0x01:dma read DAC iq for debug;0x10:dma write iq for rx(modem demod);0x10:dma write iq for tx(RF test)
#define K_DebugModeRX  0x00//0x00:dma read ADC iq for debug;0x01:dma read DAC iq for debug;0x10:dma write iq for rx(modem demod);0x10:dma write iq for tx(RF test)
//#define K_DMA_channel  0 //select DMA 0/1/2/3/4 must use highest priority DMA0
//#define K_IQ_pair_length (2+72+54+240+4)*32 //config iq pair length:words by wangying 2017/3/20 17:18:23
#define K_DMA_reload      0 //0 normal; 1 reload
#endif

#define K_op_dac_format 0x01
#define K_op_dac_swap   0x00
#define K_tx_gain   0x1f
#define K_tx_gfsk_gain  0x02
#define K_tx_le   0x01
#define K_rx_le   0x01
#define K_tx_tgn_conf 0x00
#define K_tx_upc_en   0x01

#define K_op_adc_format   0x01
#define K_op_adc_swap 0x00
#define K_op_adc_clk_inv  0x00
#define K_op_dac_clk_inv  0x00

//#define K_rx_syncword_31_0       0x331a3ae2//0x8e89bed6 for ble, 0x331a3ae2 for classic
//#define K_rx_syncword_63_32      0x4e7a2cce//0x00 for ble, 0x4e7a2cce for classic

#define K_rx_syncword_31_0       0x331a3ae2//0x8e89bed6 for ble, 0x331a3ae2 for classic
#define K_rx_syncword_63_32      0x4e7a2cce//0x00 for ble, 0x4e7a2cce for classic

//#define K_rx_syncword_31_0       0x73345e72
//#define K_rx_syncword_63_32      0x475c58cc

//uint32 AccessCode_from_bb_br_edr[2] = {0x331a3ae2, 0x4e7a2cce};
//.nAccessCode2              = 0x5a,
//.nAccessCode0              = 0x73345e72,         // LAP = 9e8b33
//.nAccessCode1              = 0x475c58cc,

///////////////////////////////////////////////////////////////////////////////////
typedef struct _modem_control
{
    unsigned char       bLEmode;                 // the rate information is invalid if m_bLEmode bit is set
    unsigned int        nAccessCode0;              // SyncWord_31_0 // for modem rx only
    unsigned int        nAccessCode1;              // SyncWord_63_32

} MODEM_CTL;


///////////////////////////////////////////////////////////////////////////////////
#endif


