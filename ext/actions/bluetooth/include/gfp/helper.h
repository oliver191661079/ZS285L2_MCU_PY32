#ifndef _HELPER_H
#define _HELPER_H

#define AES_KEY_SIZE 16
#define AES_BLOCK_SIZE 16
#define SHA256_HASH_SIZE 32
#define ECDH_PUBLIC_KEY_SIZE 64
#define ECDH_PRIVATE_KEY_SIZE 32
#define ECDH_SHARED_KEY_SIZE 32
#define NUM_BLOOM_FILTER_INDEXES_PER_KEY 8
#define BLOOM_FILTER_INDEX_BYTE_LENGTH 4

#define PERSONALIZED_NAME_SIZE 64
#define AES_NONCEZ_SIZE 8
#define AES_COUNTER_SIZE 8
#define HMAC_SHA256_SIZE 32
#define HMAC_SHA256_HEAD8 8
#define COUNTER_SIZE 16

#include <stdint.h>
#include <bluetooth_provider.h>
#include <crypto_provider.h>
#include <fast_pair.h>
#include <storage_provider.h>

typedef struct
{
    uint8_t status;
    uint8_t psm[2];
} __attribute__((packed)) gfp_message_stream_psm_t;

typedef struct
{
	uint8_t current_shared_key[AES_KEY_SIZE];
	uint8_t phone_public_address[MAC_ADDRESS_LENGTH];
	uint32_t remote_passkey;
	uint32_t local_passkey;
    uint8_t account_keys_length;
    gfp_message_stream_psm_t message_stream_psm;
    uint8_t fp_started :1;
    uint8_t account_key_writed:1;
    uint8_t headset_initiates_pairing:1;
    uint8_t prefers_le_bonding:1;
    uint8_t additional_passkey_pairing:1;
    uint8_t primary_device:1;
    uint8_t initial_pairing:1;
} __attribute__((packed)) gfp_ble_info_t;

/** Initializes Fast Pair with the implemented providers. */
FastPair* init(BluetoothProvider*, StorageProvider*, CryptoProvider*);
void fastpair_timeout_handle(os_work *work);
int additional_data_written(uint8_t* packet, int packet_length);
int additional_passkey_written(uint8_t* packet, int packet_length);
int account_key_written(uint8_t* packet, int packet_length);
int passkey_written(uint8_t* packet, int packet_length);
int perform_key_based_pairing(uint8_t* request, int request_length);
ssize_t btsrv_gfp_read_message_stream_psm(struct bt_conn *conn,
				const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset);


#endif // _HELPER_H

