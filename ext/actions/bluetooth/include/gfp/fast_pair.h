#ifndef FAST_PAIR_H
#define FAST_PAIR_H

#include <stdint.h>
#include <bluetooth_provider.h>
#include "crypto_provider.h"
#include "storage_provider.h"

#include <fastpair_act.h>

/** 
 * Provides common methods that will use the defined providers to implement
 * Fast Pair's logic.
 */
typedef struct {
  /** 
   * Converts a 256-bit ECDH key into Fast Pair's version of a 128-bit AES key. 
   * Visible for testing only.
   */
  void (*convert_ecdh_key_to_aes)(
    uint8_t* ecdh_shared_secret, uint8_t* aes_key);

  /** Generates a bloom filter. */
  void (*generate_bloom_filter)(uint8_t* bloom_filter, uint8_t *salt);

  /** Gets the size of the bloom filter based off the number of stored keys. */
  int (*get_bloom_filter_size)();

  /** Gets the model ID for the headset. */
  void (*get_model_id)(uint8_t* model_id);

  /** Gets the device's private anti-spoofing key. */
  void (*get_private_anti_spoofing_key)(uint8_t* private_anti_spoofing_key);

  /** Begins key based pairing by decrypting the first packet. */
  int (*init_key_based_pairing)(uint8_t* packet, int packet_length);

  /** Gets the proper response for the initial key based pairing request. */
  void (*key_based_pairing_response)(uint8_t* response,bool extended_response);

  /** Initiates key based pairing and produces a response. */
  characteristic_write_callback_t perform_key_based_pairing;

  /** Generates a proper encrypted response containing our local passkey. */
  void (*passkey_response)(uint8_t* response);

  /** Callback for when a pairing request is created. */
  int (*created_pairing_request)(uint8_t* address, uint32_t passkey);

  /** Callback for when a passkey has been written to the characteristic. */
  characteristic_write_callback_t passkey_written;

  /** Callback for when an account key is written to the characteristic. */
  characteristic_write_callback_t account_key_written;

  /** Callback for when additional data is written to the characteristic. */
  characteristic_write_callback_t additional_data_written;

  void (*set_pair_mode)(uint8_t mode);

  void (*account_key_clear)();

  /** Resets providers, visible for testing. */
  void (*reset)();

  void (*exit)();

} FastPair;

/** Initializes Fast Pair. */
FastPair* fast_pair(
  const uint8_t* model_id,
  const uint8_t* private_anti_spoofing_key,
  BluetoothProvider *bluetooth,
  StorageProvider *storage,
  CryptoProvider *crypto);

extern void fastpair_set_pair_mode(uint8 mode);
#endif // FAST_PAIR_H

