#ifndef _FASTPAIR_ACT__H_
#define _FASTPAIR_ACT__H_
#include <stdint.h>
#include <bt_manager.h>
#include <property_manager.h>
#include <btservice_gfp_api.h>

#define FAST_PAIR_SERVICE 0xFE2C
//#define KEY_BASED_PAIRING_CHARACTERISTIC 0x1234
//#define PASSKEY_CHARACTERISTIC 0x1235
//#define ACCOUNT_KEY_CHARACTERISTIC 0x1236
//#define ADDITIONAL_DATA_CHARACTERISTIC 0x1237
#define FIRMWARE_REVISION_CHARACTERISTIC 0x2A26

#define KEY_BASED_PAIRING_CHAR_LEN                   (80)
#define PASSKEY_CHAR_LEN                             (16)
#define ACCOUNT_KEY_CHAR_LEN                         (16)

typedef uint8_t (*fp_is_fastpair_start)(void);
typedef int (* characteristic_write_callback_t)(uint8_t*, int);		

#define DISCOVERABLE_STATE_YES                            1
#define DISCOVERABLE_STATE_NO                             0
#define FP_RFCOMM_STREAM_HEAD_LEN               4

typedef  struct{
	uint8_t state;
} fp_disc_state_changed_para_t;

typedef  struct{
	uint8_t state;
	uint8_t *address;
} fp_conn_state_changed_para_t;

typedef  struct{
	uint8_t state;
	uint8_t *address;
} fp_SSP_complete_para_t;

typedef struct {
	uint8 *uuid;
	uint16 handle;
	characteristic_write_callback_t cb;		
} fp_char_write_para_t;

typedef struct {
    uint8_t module_id[3];
    uint8_t local_br_addr[6];
    uint8_t ble_addr[6];
    uint8_t battery_value[3];
    uint8_t remainining_battery[2];
    uint8_t active_component;
    uint8_t local_capabilities;
    uint8_t platform_type;
} fp_provider_info_t;

typedef struct {
    uint8_t stream_group;
    uint8_t stream_code;
    uint8_t addition_length[2];
} fp_rfcomm_stream_head_t;


typedef struct {
    uint8_t length_type;
    uint8_t left_bud;
    uint8_t right_bud;
    uint8_t battery_case;
} fp_battery_value_t;

typedef  struct{
    uint8_t rm_addr[6];
    uint8_t channel;
    uint32_t server_handle;
    uint32_t connect_handle;
} fp_rfcomm_stream_context_t;
// Message sent and received over the message stream
// See: https://developers.google.com/nearby/fast-pair/spec#MessageStream
typedef struct {
  uint8_t message_group;
  uint8_t message_code;
  // |data| length in native encoding
  uint16_t length;
  // message payload in big endian encoding
  uint8_t* data;
} nearby_message_stream_message;

typedef enum
{
    FP_EVENT_DISCOVERABLE_STATE_CHANGED = 0,
    FP_EVENT_CONNECT_STATE_CHANGED = 1,
    FP_EVENT_SSP_COMPLETE = 2,
    FP_EVENT_RFCOMM_CONNECT = 3,
    FP_EVENT_RFCOMM_DATA_IN = 4,
    FP_EVENT_RFCOMM_DISCONNECT = 5,
    FP_EVENT_BATTERY_VALUE_CHANGED = 6,
    FP_EVENT_BATTERY_CASE_CHANGED = 7,
}fp_event_type_e;


typedef enum
{
    FP_STREAM_GROUP_BLUETOOTH_EVNET = 1,
    FP_STREAM_GROUP_COMPANION_APP_EVENT = 2,
    FP_STREAM_GROUP_DEVICE_INFOMATION_EVENT = 3,
    FP_STREAM_GROUP_DEVICE_ACTION_EVENT = 4,
    FP_STREAM_GROUP_ACKNOWLEDGEMENT = 0xFF,
}fp_stream_group_e;

typedef enum
{
    FP_STREAM_DI_CODE_MODEL_ID = 0x01,
    FP_STREAM_DI_CODE_BLE_ADDRESS_UPDATED = 0x02,
    FP_STREAM_DI_CODE_BATTERY_UPDATED = 0x03,
    FP_STREAM_DI_CODE_REMAINING_BATTERY_TIME = 0x04,
    FP_STREAM_DI_CODE_ACTIVE_COMPAONETS_REQ = 0x05,
    FP_STREAM_DI_CODE_ACTIVE_COMPAONETS_RPS	= 0x06,
    FP_STREAM_DI_CODE_CAPABILITIES = 0x07,
    FP_STREAM_DI_CODE_PLATFORM_TYPE = 0x08,
}fp_stream_device_info_code_e;

typedef enum
{
	FP_STREAM_ACTION_CODE_RING = 0x01,
}fp_stream_device_action_code_e;

enum BLE_FP_CHARACTERISTICS
{
    BLE_MODEL_ID = 0,
    BLE_KEY_PAIRING,
    BLE_PASS_KEY,
    BLE_ACCOUNT_KEY,
    BLE_ADDITIONAL_DATA,
    BLE_ADDITIONAL_PASSKEY,
    BLE_FP_CHARACTERISTICS_NUM,
};

typedef struct
{
    u8_t   uuid[16];
    u8_t   properties;
    
} ble_fp_characteristic_t;

#define VM_FAST_PERSONALIZED_NAME              "FAST_PERSONALIZED_NAME"
#define VM_FAST_PAIR              			   "VM_FAST_PAIR"


extern const ble_fp_characteristic_t  ble_fp_characteristics[BLE_FP_CHARACTERISTICS_NUM];

extern void* fastpair_init(uint8* model_id, uint8 *spoofing_key);
extern int fastpair_gatt_notify(uint8* data,uint32 size,uint8 *serv_uuid);
extern int fastpair_set_advertise_data(uint8 * adv_data, uint8 adv_len);
extern int fastpair_set_advertise_valid(uint8 valid);
extern int ble_fastpair_service_init(void);
extern int ble_fastpair_service_exit(void);
extern void fastpair_set_pair_mode(uint8 mode);

#endif

