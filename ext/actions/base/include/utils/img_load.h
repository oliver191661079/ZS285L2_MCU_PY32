#ifndef _IMG_LOAD_H_
#define _IMG_LOAD_H_

typedef enum {
	NSM_STATE_IDLE = 0,
	NSM_STATE_START_TEST,
	NSM_STATE_TX,
	NSM_STATE_RX,
} nsm_state_e;



#define NSM_CMD_SET_TX_POWER         0x0c
#define NSM_CMD_SET_TX_PACKET        0x09
#define NSM_CMD_SET_PAYLOAD          0x17
#define NSM_CMD_TX_EXCUTE            0x19
#define NSM_CMD_TX_EXCUTE_STOP       0x1a

#define NSM_CMD_SWITCH_TO_BR      0x32       //ble switch to br/edr
#define NSM_CMD_SWITCH_TO_BLE     0x31       //bt switch to ble
#define NSM_CMD_SET_CHANNEL       0x02

#define NSM_CMD_SET_RX_PACKET		0x01
#define NSM_CMD_SET_RX_MAC			0x37
#define NSM_CMD_BR_RX_EXCUTE		0x33
#define NSM_CMD_BR_RX_EXCUTE_STOP	0x34
#define NSM_CMD_GET_RX_REPORT		0x35
#define NSM_CMD_GET_RX_RSSI			0x44

#define NSM_CMD_SET_TX_PAYLOAD_LEN	0x36

#define NSM_CMD_REBOOT_ENTER_BQB     0X80


#define NSM_CMD_SET_ATTEN            10
#define NSM_CMD_ENCODE               11
#define NSM_CMD_SET_RX_DMA_MODE      13
#define NSM_CMD_GET_INIT_FREQ        14
#define NSM_CMD_GET_RF_DATA          15
#define NSM_CMD_SET_TX_POWER_OFFSET  16
#define NSM_CMD_SET_ACCESSCODE       0x18

#define NSM_CMD_DECODE_FT_P_IF_RX    0x20
#define NSM_CMD_DECODE_FT_N_IF_RX    0x21


#define NSM_CMD_FRE_HOP          0x30

u8_t img_code_is_nsm_sppble_test(void);
u32_t img_code_load_nsm(void);

#endif /* _IMG_LOAD_H_ */
