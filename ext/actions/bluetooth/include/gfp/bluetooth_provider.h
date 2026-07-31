#ifndef BLUETOOTH_PROVIDER_H
#define BLUETOOTH_PROVIDER_H

#define MAC_ADDRESS_LENGTH 6

#include <stdint.h>
#include <stdbool.h>
#include <fastpair_act.h>
//#include <bt_manager_inner.h>
//#include <gfp_ble_stream.h>
//#include <gfp_ble_stream_ctrl.h>

/** Provides Bluetooth functionality. */
typedef struct {
  /** Registers a callback for the provided characteristic. */
  void (*add_characteristic_write_callback)(
    uint16_t service_id, 
    uint8_t* characteristic_id, 
    characteristic_write_callback_t callback);

  /** Gets the device's current ble address. */
  void (*get_ble_address)(uint8_t* address);

  /** Gets the device's public address. */
  void (*get_public_address)(uint8_t* address);
  
  /** Starts broadcasting a provided packet over BLE. */
  void (*start_broadcasting)(
      uint16_t service_id, uint8_t* packet, int packet_length);

  /** Sends a notify to the provided characteristic. */
  int (*notify)(
      uint8_t* packet,
      uint16_t packet_length,
      int number);

  /** Checks whether or not the headset is discoverable. */
  int (*is_discoverable)();

  /** Checks whether or not the headset is in pairing mode. */
  int (*is_pairing)();

  /** Sets the device's capabilities to Display/YesNo. */
  void (*set_capabilities_display_yes_no)(bool ble_devices);

  /** Sets the device's capabilities to noinput/nooutput. */
  void (*set_capabilities_noinputnooutput)(bool ble_devices);

  /** Initiate bonding with the provided address. */
  void (*initiate_bonding)(uint8_t* address);

  /** Update scan. */
  void (*update_scan)(bool valid);

  /** Sets a callback to be invoked when a pairing request comes in. */
  void (*set_pairing_request_callback)(
    btsrv_gfp_pairing_request_callback);
  
  void (*set_get_fastpair_state_callback)(
  	fp_is_fastpair_start);

  /** Confirms a pairing for the given address, meaning passkey check passed. */
  void (*confirm_pairing)(uint8_t* address, bool success,bool ble_devices);

  int (*is_paired_device)(uint8_t* address);

  uint16_t (*get_personalized_name)(uint8_t*);

  void (*update_personalized_name)(uint8_t * name,uint8_t length, bool additional);

  int (*is_lea_open)(void);

  void (*get_battery_value)(fp_battery_value_t * battery_value);

  void (*get_battery_case)(uint8_t * case_mode);

} BluetoothProvider;

#endif    // BLUETOOTH_PROVIDER_H

