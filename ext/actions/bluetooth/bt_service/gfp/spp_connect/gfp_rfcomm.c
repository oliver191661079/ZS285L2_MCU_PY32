#include <os_common_api.h>

#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <msg_manager.h>
#include <mem_manager.h>
#include <bt_manager.h>
#include <sys_event.h>
#include <btservice_api.h>
#include <shell/shell.h>
#include <acts_bluetooth/host_interface.h>
#include <property_manager.h>

#include <fast_pair.h>
#include "gfp_spp_stream_ctrl.h"


const static uint8_t rfcomm_ring_action_ack[6] = {0xff,0x01,0x00,0x02,0x04,0x01};

extern const BluetoothProvider bluetooth;
extern const FastPair pairer;

void fastpair_slience_mode(void)
{
	uint8_t id_data[4];

	id_data[0] = FP_STREAM_GROUP_BLUETOOTH_EVNET;
	id_data[1] = 0x02;
	id_data[2] = 0;
	id_data[3] = 0;
	gfp_message_stream_send_data(id_data,sizeof(id_data));
}

void fastpair_mode_id_update(void)
{
	uint8_t id_data[7];

	id_data[0] = FP_STREAM_GROUP_DEVICE_INFOMATION_EVENT;
	id_data[1] = FP_STREAM_DI_CODE_MODEL_ID;
	id_data[2] = 0x00;
	id_data[3] = 0x03;
	pairer.get_model_id(&id_data[4]);

	gfp_message_stream_send_data(id_data,sizeof(id_data));
}

void fastpair_ble_addr_update(void)
{
	uint8_t ble_data[10];

	ble_data[0] = FP_STREAM_GROUP_DEVICE_INFOMATION_EVENT;
	ble_data[1] = FP_STREAM_DI_CODE_BLE_ADDRESS_UPDATED;
	ble_data[2] = 0x00;
	ble_data[3] = 0x06;
	bluetooth.get_ble_address(&ble_data[4]);

	gfp_message_stream_send_data(ble_data,sizeof(ble_data));

}

void fastpair_capabilities_updated(void)
{
	uint8_t cap_data[5];


	cap_data[0] = FP_STREAM_GROUP_DEVICE_INFOMATION_EVENT;
	cap_data[1] = FP_STREAM_DI_CODE_CAPABILITIES;
	cap_data[2] = 0x00;
	cap_data[3] = 0x01;
	cap_data[4] = 0x00;
/*
	Bit 0: 1 if the companion app is installed, 0 otherwise
	Bit 1: 1 if silence mode is supported, 0 otherwise
	All other bits reserved for future use and should be set to 0
*/
	gfp_message_stream_send_data(cap_data,sizeof(cap_data));
}

void fastpair_platform_updated(void)
{
	uint8_t platform_data[5];
	platform_data[0] = FP_STREAM_GROUP_DEVICE_INFOMATION_EVENT;
	platform_data[1] = FP_STREAM_DI_CODE_PLATFORM_TYPE;
	platform_data[2] = 0x00;
	platform_data[3] = 0x01;
	platform_data[4] = 0x01;
/*
	Android 0x01
*/
	gfp_message_stream_send_data(platform_data,sizeof(platform_data));
}


void fastpair_initial_provider_info(void)
{
	fastpair_mode_id_update();
	fastpair_ble_addr_update();
}

#if 0
static void ring_handset(uint8 onoff)
{
	SYS_LOG_INF("RING:%d",onoff);
}
#endif

void fastpair_rfcomm_device_action_event(uint8_t* data, uint16_t size)
{
	fp_rfcomm_stream_head_t * rfcomm_stream  = (fp_rfcomm_stream_head_t *)data;
	uint8_t code = rfcomm_stream->stream_code;
	uint8_t ring_state = *(data + FP_RFCOMM_STREAM_HEAD_LEN);
	SYS_LOG_INF("ring_state 0x%x.",ring_state);

/*
For example, if the first byte of additional data is set to:
0x00 (0b00000000): All components should stop ringing
0x01 (0b00000001): Ring right, stop ringing left
0x02 (0b00000010): Ring left, stop ringing right
0x03 (0b00000011): Ring both left and right
On Providers which do not support individual ringing, only 1 bit should be considered:
0x00 (0b00000000): Stop ringing
0x01 (0b00000001): Start ringing
*/

	switch(code){
		case FP_STREAM_ACTION_CODE_RING:
		gfp_message_stream_send_data((void *)rfcomm_ring_action_ack,sizeof(rfcomm_ring_action_ack));
//		btsrv_gfp_message_stream_ring_handset(ring_state);
		break;

		default:
		break;
	}
}

static void fastpair_rfcomm_device_information_event(uint8_t* data, uint16_t size)
{
	fp_rfcomm_stream_head_t * rfcomm_stream  = (fp_rfcomm_stream_head_t *)data;
	uint8_t code = rfcomm_stream->stream_code;
 //   uint16_t length = FP_RFCOMM_DECODE2BYTE_BIG(rfcomm_stream->addition_length);

	switch(code){
		case FP_STREAM_DI_CODE_CAPABILITIES:
		//fp_device_information.remote_capabilities = *(data + FP_RFCOMM_STREAM_HEAD_LEN);
		//BT_PRINT_INFO_INT("CAPABILITIES:",fp_device_information.remote_capabilities);
		//fastpair_initial_provider_info();

		break;

		case FP_STREAM_DI_CODE_BATTERY_UPDATED:

		break;

		case FP_STREAM_DI_CODE_ACTIVE_COMPAONETS_REQ:
		//fastpair_active_updated();
		//bluetooth_provider->rfcomm_stream_write(phone_public_address,(void *)rfcomm_active_component_rsp,sizeof(rfcomm_active_component_rsp));
		break;

		case FP_STREAM_DI_CODE_PLATFORM_TYPE:
		//fastpair_platform_updated();
		break;

		default:
		break;
	}
}

int rfcomm_message_stream_deal(uint8_t* data, uint16_t size)
{
	fp_rfcomm_stream_head_t * rfcomm_stream  = (fp_rfcomm_stream_head_t *)data;
	print_hex_comm("FP RF:",data,size);

	if(size < FP_RFCOMM_STREAM_HEAD_LEN){
		SYS_LOG_ERR("fp rfcomm stream len err!");
		return -1;
	}

	switch(rfcomm_stream->stream_group){
		case FP_STREAM_GROUP_BLUETOOTH_EVNET:
		/*
			Silence Mode 
			Enable silence mode :  01 01 00 01
			Disable silence mode :	01 01 00 02
		*/
		break;
		case FP_STREAM_GROUP_COMPANION_APP_EVENT:
		/*
			Companion app event
		*/
		break;
		case FP_STREAM_GROUP_DEVICE_INFOMATION_EVENT:
		fastpair_rfcomm_device_information_event(data,size);
		break;
		case FP_STREAM_GROUP_DEVICE_ACTION_EVENT:
		fastpair_rfcomm_device_action_event(data,size);
		break;
		case FP_STREAM_GROUP_ACKNOWLEDGEMENT:
		break;
		default :
		break;
	}
	return 0;
}


