#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <helper.h>
#include <mem_manager.h>

//#include <crypto.h>

#include <tinycrypt/ctr_mode.h>
#include <tinycrypt/hmac.h>
#include <btservice_gfp_api.h>
#include <acts_bluetooth/host_interface.h>

#define KEY_BASED_PAIRING_REQUEST_FLAG    0x00
#define ACTION_REQUEST_FLAG    0x10

// KBPR - Key Based Pairing Request
#define KBPR_INITIATE_PAIRING_MASK (1 << 6)
#define KBPR_NOTIFY_EXISTING_NAME_MASK (1 << 5)
#define KBPR_RETROACTIVELY_WRITE_ACCOUNT_KEY_MASK (1 << 4)
#define KBPR_SEEKER_SUPPORTS_BLE_DEVICES_MASK (1 << 3)
#define KBPR_SEEKER_SUPPORTS_LE_AUDIO_MASK (1 << 2)
#define KBPR_SEEKER_ADDRESS_OFFSET 8

#define ACTION_REQUEST_DEVICE_ACTION_MASK (1 << 7)
#define ACTION_REQUEST_WILL_WRITE_DATA_CHARACTERISTIC_MASK (1 << 6)
#define REQUEST_BT_ADDRESS_OFFSET 2
#define KBPR_SEEKER_ADDRESS_OFFSET 8

#define SHOW_BATTERY_INDICATION_BYTE 0x33
#define DONT_SHOW_BATTERY_INDICATION_BYTE 0x34

// Number of salt bytes to use in advertisements
#define NEARBY_FP_SALT_SIZE 2
#define SALT_FIELD_LENGTH_AND_TYPE_BYTE ((NEARBY_FP_SALT_SIZE << 4) | 1)

#define AES_MESSAGE_SIZE_BYTES 16
#define AES_ADDITIONAL_KEY_SIZE_BYTES 16

#define SEEKER_PASSKEY_MESSAGE_TYPE 0x02
#define PROVIDER_PASSKEY_MESSAGE_TYPE 0x03
#define ACCOUNT_KEY_WRITE_MESSAGE_TYPE 0x04

#define ADDITIONAL_PASSKEY_SEEKER_FLAG      0
#define ADDITIONAL_PASSKEY_PROVIDER_FLAG    1
#define ADDITIONAL_PASSKEY_SIZE_BYTES       3

#define ADDITIONAL_PASSKEY_TYPE_OFFSET      0
#define ADDITIONAL_PASSKEY_PASSKEY_OFFSET   1
#define ADDITIONAL_PASSKEY_ADDRESS_OFFSET   4
#define ADDITIONAL_PASSKEY_STATUS_OFFSET    10

// Response for seekers that don't support BLE-only devices.
#define KEY_BASED_PAIRING_RESPONSE_FLAG 0x01
// Response for seekers that support BLE-only devices.
#define KEY_BASED_PAIRING_EXTENDED_RESPONSE_FLAG 0x02

// Is this platform a BLE-only device?
#ifndef NEARBY_FP_BLE_ONLY
#define NEARBY_FP_BLE_ONLY 0
#endif /* NEARBY_FP_BLE_ONLY */
// Does this device prefer BLE bonding?
#ifndef NEARBY_FP_PREFER_BLE_BONDING
#define NEARBY_FP_PREFER_BLE_BONDING 1
#endif /* NEARBY_FP_PREFER_BLE_BONDING */
#ifndef NEARBY_FP_SECOND_ADDR_RANDOM
#define NEARBY_FP_SECOND_ADDR_RANDOM 0
#endif /* NEARBY_FP_SECOND_ADDR_RANDOM */
#ifndef NEARBY_FP_BLE_CSIS
#define NEARBY_FP_BLE_CSIS 0
#endif /* NEARBY_FP_BLE_ONLY */


#define MESSAGE_STREAM_PSM    0x80
#define MESSAGE_STREAM_PSM_SIZE    3

typedef enum
{
	FP_MESSAGE_STREAM_PSM_STATE_UNKNOWN = 0x0,
	FP_MESSAGE_STREAM_PSM_STATE_READY = 0x01,
	FP_MESSAGE_STREAM_PSM_STATE_UNAVIAILABLE = 0x02,
}fp_message_stream_psm_state_e;

typedef enum
{
	FP_BASED_PAIR_PROVIDER_INITIATE_BONDING = 0x01,
	FP_BASED_PAIR_RETROACTIVELY_WRITING_ACCOUNT_KEY = 0x02,
	FP_BASED_PAIR_NOTIFY_EXISTING_NAME = 0x04,
	FP_BASED_PAIR_ADDITIONAL_DATA = 0x08,
    FP_BASED_PAIR_SEEKER_SUPPORTS_BLE_DEVICES = 0x10,
    FP_BASED_PAIR_SEEKER_SUPPORTS_LE_AUDIO = 0x20,
	FP_BASED_PAIR_SEEKER_INITIALTE_BONDING = 0x80,
}fp_based_pair_type_e;

static btsrv_gfp_auth_timeout_start auth_start_cb = NULL;
static btsrv_gfp_auth_timeout_stop auth_stop_cb = NULL;
static btsrv_gfp_fixed_chan_forbid fixed_chan_forbid_cb = NULL;

extern const FastPair pairer;

uint8_t* stored_private_anti_spoofing_key;
BluetoothProvider* bluetooth_provider;
StorageProvider* storage_provider;
CryptoProvider* crypto_provider;

// State variables set during various pairing stages.

gfp_ble_info_t ble_account_info;

//static uint8_t fp_timeout_timer;

static fp_provider_info_t fp_provider;
static u8_t gfp_adv_data[31];
static u8_t gfp_adv_len = 0;

///////////////////////////////////////////////////////////////////
// Internal crypto functionality.
///////////////////////////////////////////////////////////////////

extern int bt_rand(void *buf, unsigned int len);

static void free_pointer(void *ptr) {
	if(ptr != NULL){
		mem_free(ptr);
	}
}

/**
 * Takes a 32-byte shared key generated from ECDH and chops it down to 16-bytes
 * to be used with AES.
 *
 * We first hash the shared key to ensure that the output is secure, then take
 * the first 16-bytes. See the Fast Pair spec for more information.
 */
void convert_ecdh_key_to_aes(uint8_t* input_ecdh_shared_key, uint8_t* output_aes_key)
{
	crypto_provider->hash(input_ecdh_shared_key, ECDH_SHARED_KEY_SIZE, input_ecdh_shared_key);
	memcpy(output_aes_key, input_ecdh_shared_key, AES_KEY_SIZE);
}

///////////////////////////////////////////////////////////////////
// Internal bloom filter functionality.
///////////////////////////////////////////////////////////////////

/** Adds a value to the provided bloom filter array. */
void add_to_bloom_filter(
	uint8_t* bloom_filter, 
	int filter_length, 
	uint8_t* value, 
	int value_length) 
{
	uint8_t h[SHA256_HASH_SIZE];
	int i, j;
	uint32_t xi = 0; 
	int start,end;
	crypto_provider->hash(value, value_length, h);
	
	// Split the 32-byte hash into 8 32-bit values (uint32_t). Then,
	// use each of those 32-bit values to calculate an index in the 
	// bloom filter and set that index to 1.
	for (i = 0; i < NUM_BLOOM_FILTER_INDEXES_PER_KEY; i++){
		xi = 0;
		start = i * BLOOM_FILTER_INDEX_BYTE_LENGTH;
		end = start + BLOOM_FILTER_INDEX_BYTE_LENGTH;
		for (j = start; j < end; j++) {
			xi = (xi << 8) + h[j];
		}
		uint32_t k = xi % (filter_length * 8);
		bloom_filter[k / 8] |= (1 << (k % 8));
	}
}

/** Gets the size of the bloom filter. */
int get_bloom_filter_size(void)
{
	int size;
	int account_key = storage_provider->get_num_account_keys();
	size = account_key + 3;

	if(account_key == MAX_NUM_KEYS){
		size += 1;
	}
	return size;
}

/** Generates a bloom filter. See get_bloom_filter_size to get its size. */
void generate_bloom_filter(uint8_t* bloom_filter, uint8_t *salt)
{
   int i;
   uint8_t salt_value[NEARBY_FP_SALT_SIZE];

   if(!bloom_filter || !salt){
       return;
   }

   memset(bloom_filter, 0, get_bloom_filter_size());  
   
   bt_rand(&salt_value, NEARBY_FP_SALT_SIZE);
   memcpy(salt,salt_value,NEARBY_FP_SALT_SIZE);

   for (i = 0; i < storage_provider->get_num_account_keys(); i++){
	   uint8_t value[ACCOUNT_KEY_LENGTH + NEARBY_FP_SALT_SIZE];
	   uint8_t account_key[ACCOUNT_KEY_LENGTH];
	   storage_provider->get_account_key(i, account_key);
	   memcpy(value, account_key, ACCOUNT_KEY_LENGTH);
	   memcpy(value + ACCOUNT_KEY_LENGTH,salt_value,NEARBY_FP_SALT_SIZE);
	   
	   add_to_bloom_filter(bloom_filter, get_bloom_filter_size(),value,ACCOUNT_KEY_LENGTH + NEARBY_FP_SALT_SIZE);
   }
}

/** Generates a bloom filter. See get_bloom_filter_size to get its size. */
void generate_bloom_filter_with_battery(uint8_t* bloom_filter, uint8_t *salt, fp_battery_value_t *battery)
{
   int i;
   uint8_t salt_value[NEARBY_FP_SALT_SIZE];

   if(!bloom_filter || !salt || !battery){
       return;
   }

   bt_rand(&salt_value, NEARBY_FP_SALT_SIZE);
   memcpy(salt,salt_value,NEARBY_FP_SALT_SIZE);

   memset(bloom_filter, 0, get_bloom_filter_size());

   for (i = 0; i < storage_provider->get_num_account_keys(); i++){
	   uint8_t value[ACCOUNT_KEY_LENGTH + NEARBY_FP_SALT_SIZE + sizeof(fp_battery_value_t)];
	   uint8_t account_key[ACCOUNT_KEY_LENGTH];
	   storage_provider->get_account_key(i, account_key);
	   memcpy(value, account_key, ACCOUNT_KEY_LENGTH);
	   memcpy(value + ACCOUNT_KEY_LENGTH,salt_value,NEARBY_FP_SALT_SIZE);
	   memcpy(&value[ACCOUNT_KEY_LENGTH + NEARBY_FP_SALT_SIZE],battery,sizeof(fp_battery_value_t));

	   add_to_bloom_filter(bloom_filter, get_bloom_filter_size(),value,ACCOUNT_KEY_LENGTH + NEARBY_FP_SALT_SIZE + sizeof(fp_battery_value_t));
   }
}

///////////////////////////////////////////////////////////////////
// General headset information.
///////////////////////////////////////////////////////////////////

/** Gets the headset's model ID. Visible for testing. */
void get_model_id(uint8_t* model_id) 
{
	memcpy(model_id, fp_provider.module_id , 3);
}

/** Gets the headset's private anti spoofing key. Visible for testing. */
void get_private_anti_spoofing_key(uint8_t* private_anti_spoofing_key)
{
	memcpy(private_anti_spoofing_key, stored_private_anti_spoofing_key, 32);
}

///////////////////////////////////////////////////////////////////
// General helper methods.
///////////////////////////////////////////////////////////////////

/** Comares two int arrays. */
int static array_equals(uint8_t* expected, uint8_t* actual, int length)
{
	int i;
	for (i = 0; i < length; i++)
		if (actual[i] != expected[i]) 
			return 0;
			
	return 1;
}

///////////////////////////////////////////////////////////////////
// Main pairing methods.
///////////////////////////////////////////////////////////////////

/** Begins the first steps of the pairing process. See steps 1-5 in the spec. */
int init_key_based_pairing(uint8_t* packet, int packet_length)
{
// TODO(klinker41): Keep track of failures here. If more than 10 happen, do
// not allow pairing again until after power cycle.
	int i;
	int ret = 0;
    uint8_t *key = (u8_t *)mem_malloc(AES_KEY_SIZE);
    uint8_t *buffer = (u8_t *)mem_malloc(AES_BLOCK_SIZE);
    uint8_t *ble_address = (u8_t *)mem_malloc(MAC_ADDRESS_LENGTH);
    uint8_t *public_address = (u8_t *)mem_malloc(MAC_ADDRESS_LENGTH);
    uint8_t *public_key = (u8_t *)mem_malloc(ECDH_PUBLIC_KEY_SIZE);
    uint8_t *private_key = (u8_t *)mem_malloc(ECDH_PRIVATE_KEY_SIZE);
    uint8_t *shared_key = (u8_t *)mem_malloc(ECDH_SHARED_KEY_SIZE);

	if((!key) || (!buffer) || (!ble_address) ||
		(!public_address) || (!public_key) || (!private_key) || (!shared_key)){
	       SYS_LOG_ERR("MALLOC ERROR");
		   ret = -1;
		   goto keybase_pair_exit;
	}

    bluetooth_provider->get_ble_address(ble_address);
    bluetooth_provider->get_public_address(public_address);
    ble_account_info.initial_pairing = 0;

	if (packet_length == AES_BLOCK_SIZE) {
		int success = 0;

		for (i = 0; i < storage_provider->get_num_account_keys(); i++) {
			storage_provider->get_account_key(i, key);
			memcpy(buffer, packet, AES_BLOCK_SIZE);

			// Attempt decrypting the packet using the account key. If the result 
			// contains the device's public or BLE address in bytes 2-7, use that
			// key for decryption and all is well.
			crypto_provider->decrypt(key, buffer);
            print_hex_comm("sub pairing:",buffer,AES_BLOCK_SIZE);
			if (array_equals(public_address, buffer + REQUEST_BT_ADDRESS_OFFSET, MAC_ADDRESS_LENGTH)
				|| array_equals(ble_address, buffer + REQUEST_BT_ADDRESS_OFFSET, MAC_ADDRESS_LENGTH)) {

				memcpy(packet, buffer, AES_BLOCK_SIZE);
				memcpy(ble_account_info.current_shared_key, key, AES_BLOCK_SIZE);
				success = 1;
				break;
			}
		}

		if (!success) {
            SYS_LOG_ERR("sub pairing fail!\n");
			goto keybase_pair_exit;
		}
	} 
	else if (packet_length == AES_BLOCK_SIZE + ECDH_PUBLIC_KEY_SIZE){

		// Copy the public key from the last 64 bytes of the packet and get the
		// headset's private key.
		memcpy(public_key, packet + AES_BLOCK_SIZE, ECDH_PUBLIC_KEY_SIZE);
		pairer.get_private_anti_spoofing_key(private_key);
		
		// Generate the shared key from the public/private key pair.
		crypto_provider->
		generate_shared_secret(public_key, private_key, shared_key);
		
		// Generate the actual, 128-bit AES key we'll use.
		convert_ecdh_key_to_aes(shared_key, key);
		
		// Decrypt the first 16-bytes of the packet.
		crypto_provider->decrypt(key, packet);
        print_hex_comm("based pairing:",packet,16);

		// Verify that it matches the headset's public or BLE address.
		if (!array_equals(public_address, packet + REQUEST_BT_ADDRESS_OFFSET, MAC_ADDRESS_LENGTH)
			&& !array_equals(ble_address, packet + REQUEST_BT_ADDRESS_OFFSET, MAC_ADDRESS_LENGTH)) {
			// Address doesn't match up, so quit out now.
			SYS_LOG_ERR("address doesn't match!\n");
			goto keybase_pair_exit;
		}

		// Save the new shared key for later use.
		memcpy(ble_account_info.current_shared_key, shared_key, AES_BLOCK_SIZE);

		ble_account_info.initial_pairing = 1;
	} 
	else {
		// Invalid length, quit out now.
		goto keybase_pair_exit;
	}

    if(packet_length == AES_BLOCK_SIZE + ECDH_PUBLIC_KEY_SIZE){
        if(packet[0] == KEY_BASED_PAIRING_REQUEST_FLAG){
            if (!(packet[1] & KBPR_RETROACTIVELY_WRITE_ACCOUNT_KEY_MASK)){
                if(!bluetooth_provider->is_pairing()){
                    bt_manager_enter_pair_mode();
                }
            }
        }
    }

	// Packet now contains the decoded information, so use it as needed.
	if (packet[0] == ACTION_REQUEST_FLAG) {
		if(packet[1] & ACTION_REQUEST_DEVICE_ACTION_MASK){
            SYS_LOG_ERR("Device action not implemented!");
			goto keybase_pair_exit;
		}

		if(packet[1] & ACTION_REQUEST_WILL_WRITE_DATA_CHARACTERISTIC_MASK){
			ret |= FP_BASED_PAIR_ADDITIONAL_DATA;
			goto keybase_pair_exit;
		}
	}
	else if(packet[0] == KEY_BASED_PAIRING_REQUEST_FLAG){
		if (packet[1] & KBPR_RETROACTIVELY_WRITE_ACCOUNT_KEY_MASK) {
//			if(!bluetooth_provider->is_paired_device(packet + KBPR_SEEKER_ADDRESS_OFFSET)){
//				goto keybase_pair_exit;
//			}
//			else{
				ret |= FP_BASED_PAIR_RETROACTIVELY_WRITING_ACCOUNT_KEY;
//			}
		}

		if (packet[1] & KBPR_NOTIFY_EXISTING_NAME_MASK) {
			ret |= FP_BASED_PAIR_NOTIFY_EXISTING_NAME;
		}

		if(packet[1] & KBPR_SEEKER_SUPPORTS_BLE_DEVICES_MASK){
		    ret |= FP_BASED_PAIR_SEEKER_SUPPORTS_BLE_DEVICES;
		}

		if(packet[1] & KBPR_SEEKER_SUPPORTS_LE_AUDIO_MASK){
		    ret |= FP_BASED_PAIR_SEEKER_SUPPORTS_LE_AUDIO;
		}

		if (packet[1] & KBPR_INITIATE_PAIRING_MASK) {
			// If the second bit of the second byte is 1, then we will initiate pairing
			// later and the phone's public address is in the packet.
			ble_account_info.headset_initiates_pairing = 1;
			memcpy(ble_account_info.phone_public_address, packet + KBPR_SEEKER_ADDRESS_OFFSET, MAC_ADDRESS_LENGTH);
			ret |= FP_BASED_PAIR_PROVIDER_INITIATE_BONDING;
		}
		else{
			ble_account_info.headset_initiates_pairing = 0;
			ret |= FP_BASED_PAIR_SEEKER_INITIALTE_BONDING;
		}
	}
	else{
		;
	}

keybase_pair_exit:
    free_pointer(key);
    free_pointer(buffer);
    free_pointer(ble_address);
    free_pointer(public_address);
    free_pointer(public_key);
    free_pointer(private_key);
    free_pointer(shared_key);
    return ret;
}

void personalized_name_response(int base_pairing_flag)
{
    u8_t *counter;
    u8_t *hamc_data;
    u8_t *nonce_aes;
    u8_t *response;
    u8_t *device_name;
    struct tc_aes_key_sched_struct *ctx;
    struct tc_hmac_state_struct *hmac_state;
	u32_t ramdon;
	int response_size;

    device_name = (u8_t *)mem_malloc(PERSONALIZED_NAME_SIZE);
	if(!device_name){
		return;
	}
    memset(device_name,0,PERSONALIZED_NAME_SIZE);

	size_t name_len = bluetooth_provider->get_personalized_name(device_name);
    if(name_len == 0){
        if((base_pairing_flag & FP_BASED_PAIR_RETROACTIVELY_WRITING_ACCOUNT_KEY) == 0){
            mem_free(device_name);
            return;
        }
        else{
            memset(device_name,0,PERSONALIZED_NAME_SIZE);
            name_len = property_get_string(CFG_BT_NAME, device_name, PERSONALIZED_NAME_SIZE - 1, CONFIG_BT_DEVICE_NAME);
            if(name_len == 0){
                mem_free(device_name);
                return;
            }
            else{
                name_len = strlen(device_name);
            }
        }
    }

	SYS_LOG_INF("name response:%d %s",name_len,device_name);

    counter = (u8_t *)mem_malloc(COUNTER_SIZE);
    hamc_data = (u8_t *)mem_malloc(HMAC_SHA256_SIZE);
    nonce_aes = (u8_t *)mem_malloc(AES_NONCEZ_SIZE + PERSONALIZED_NAME_SIZE);
    response = (u8_t *)mem_malloc(PERSONALIZED_NAME_SIZE + AES_NONCEZ_SIZE + HMAC_SHA256_HEAD8);
    ctx = mem_malloc(sizeof(struct tc_aes_key_sched_struct));
    hmac_state = mem_malloc(sizeof(struct tc_hmac_state_struct));
	if((!counter) || (!hamc_data) || (!nonce_aes)||
		(!response) || (!ctx) || (!hmac_state)){
		goto exit;
	}

	memset(counter,0,COUNTER_SIZE);
	bt_rand(&ramdon, 4);
	memcpy(counter + 8,&ramdon,4);
	bt_rand(&ramdon, 4);
	memcpy(counter + 12,&ramdon,4);

	memcpy(nonce_aes,counter + 8,AES_NONCEZ_SIZE);
	tc_aes128_set_encrypt_key(ctx,ble_account_info.current_shared_key);
	tc_ctr_mode(nonce_aes + 8, name_len , device_name ,name_len, counter, ctx);

	tc_hmac_set_key(hmac_state,ble_account_info.current_shared_key,16);
	tc_hmac_init(hmac_state);
	tc_hmac_update(hmac_state, nonce_aes, name_len + AES_NONCEZ_SIZE);
	tc_hmac_final(hamc_data, HMAC_SHA256_SIZE, hmac_state);

	memcpy(response,hamc_data,HMAC_SHA256_HEAD8);
	memcpy(response + HMAC_SHA256_HEAD8,nonce_aes,AES_NONCEZ_SIZE + name_len);

	response_size = HMAC_SHA256_HEAD8 + AES_NONCEZ_SIZE + name_len;

	bluetooth_provider->
		notify(
		  response,
		  response_size,
		  BLE_ADDITIONAL_DATA);

exit:
	free_pointer(counter);
	free_pointer(hamc_data);
	free_pointer(nonce_aes);
	free_pointer(response);
	free_pointer(ctx);
	free_pointer(hmac_state);
	free_pointer(device_name);
}


/** 
 * Produces a response to the key based pairing request. See steps 6-7 in the 
 * spec. 
 */
void key_based_pairing_response(uint8_t* response, bool extended_response)
{
	// Set the response type to byte 0.
	int i = 0;
    uint8_t flags = 0;
	uint8_t public_address[MAC_ADDRESS_LENGTH];

#if NEARBY_FP_BLE_CSIS
    uint8_t component = 0;
#endif

    if(extended_response){
	    response[i++] = KEY_BASED_PAIRING_EXTENDED_RESPONSE_FLAG;

#if NEARBY_FP_BLE_ONLY
        flags |= 0x80;
#endif /* NEARBY_FP_BLE_ONLY */

#if NEARBY_FP_PREFER_BLE_BONDING
        if(ble_account_info.prefers_le_bonding){
            flags |= 0x40;
        }
#endif /* NEARBY_FP_PREFER_BLE_BONDING */

#if NEARBY_FP_SECOND_ADDR_RANDOM
        flags |= 0x20;
#endif

        response[i++] = flags;

#if NEARBY_FP_BLE_CSIS
    // Number of the Provider's addresses. The number is either 1 or 2.
        if(ble_account_info.primary_device && ble_account_info.prefers_le_bonding){
            component = 1;
            response[i++] = 2;
        }
        else{
            response[i++] = 1;
        }
#else
        response[i++] = 1;
#endif

    }
    else{
	    response[i++] = KEY_BASED_PAIRING_RESPONSE_FLAG;
    }

	// Set the headset's public address to bytes 1-7.
	bluetooth_provider->get_public_address(public_address);
	memcpy(response + i, public_address, MAC_ADDRESS_LENGTH);

	i+= MAC_ADDRESS_LENGTH;

#if NEARBY_FP_BLE_CSIS
    if(component){
        memcpy(response + i, ble_account_info.component_id_addr, MAC_ADDRESS_LENGTH);
        i+= MAC_ADDRESS_LENGTH;
    }
#endif

	// Fill the remainder of the array with a random salt.
	for (; i < AES_MESSAGE_SIZE_BYTES; i++) {
		bt_rand(&response[i], 1);
	}

    print_hex_comm("response:",response,AES_MESSAGE_SIZE_BYTES);

	// Encrypt the response.
	crypto_provider->encrypt(ble_account_info.current_shared_key, response);

	bluetooth_provider->
	notify(
	  response,
	  AES_BLOCK_SIZE,
	  BLE_KEY_PAIRING);
}

/**
 * Performs a key based pairing request.
 *
 * This matches up with steps 1-7 in the Fast Pair 2.0 spec. A request will
 * be received, verified that it is correct, and then a response will be sent
 * via a notify to the key based pairing request characteristic.
 */
int perform_key_based_pairing(uint8_t* request, int request_length)
{

	uint8_t response[AES_BLOCK_SIZE] = { 0 };
	int ret;
    bool extended_response = false;

	ret = init_key_based_pairing(request, request_length);

	SYS_LOG_INF("key_based_pairing len:%d ret:%d", request_length,ret);

	if(!ret){
		return 0;
	}
    if(ret & FP_BASED_PAIR_SEEKER_SUPPORTS_BLE_DEVICES){
        extended_response = true;
    }

    if(bluetooth_provider->is_lea_open() && (ret & FP_BASED_PAIR_SEEKER_SUPPORTS_LE_AUDIO)){ 
        ble_account_info.prefers_le_bonding = 1;
    }
    else{
        ble_account_info.prefers_le_bonding = 0;
        if((ret & FP_BASED_PAIR_SEEKER_SUPPORTS_LE_AUDIO) == 0){
            if(((ret & FP_BASED_PAIR_ADDITIONAL_DATA) == 0) && ((ret & FP_BASED_PAIR_RETROACTIVELY_WRITING_ACCOUNT_KEY) == 0)){
                if(fixed_chan_forbid_cb){  // for pixel6 android12
                    fixed_chan_forbid_cb();
                }
            }
        }
    }

	SYS_LOG_INF("base pairing: ext:%d lea:%d pri:%d", extended_response,ble_account_info.prefers_le_bonding,ble_account_info.primary_device);

	key_based_pairing_response(response,extended_response);

	if(ret & FP_BASED_PAIR_NOTIFY_EXISTING_NAME){
	    personalized_name_response(ret);
	}

	if(ret & FP_BASED_PAIR_RETROACTIVELY_WRITING_ACCOUNT_KEY){
		return 1;
	}

	if(ret & FP_BASED_PAIR_ADDITIONAL_DATA){
		return 1;
	}

	if (!bluetooth_provider->is_pairing()) {
	// If we are not in pairing mode, exit immediately because anti-spoofing
	// key pairing should only be done in pairing mode.
		SYS_LOG_INF("not in pairing mode\n");
	}

	bluetooth_provider->set_capabilities_display_yes_no(ble_account_info.prefers_le_bonding);


	if (ble_account_info.headset_initiates_pairing) {
		bluetooth_provider->initiate_bonding(ble_account_info.phone_public_address);
	}
	ble_account_info.fp_started = 1;

	if (auth_start_cb)
		auth_start_cb(20000);
		 
    bluetooth_provider->update_scan(true);
	return 1;
}

void passkey_response(uint8_t* response)
{
	int i;
	response[0] = PROVIDER_PASSKEY_MESSAGE_TYPE;
	response[1] = ble_account_info.local_passkey >> 16;
	response[2] = ble_account_info.local_passkey >> 8;
	response[3] = ble_account_info.local_passkey;
	
	for (i = 4; i < AES_BLOCK_SIZE; i++) {
		bt_rand(&response[i], 1);
	}
	crypto_provider->encrypt(ble_account_info.current_shared_key, response);
}

void key_based_pairing_finish(void)
{
    if (auth_stop_cb)
        auth_stop_cb();

    if(ble_account_info.fp_started){
        ble_account_info.remote_passkey = 0;
        ble_account_info.local_passkey = 0;
        ble_account_info.fp_started = 0;
        ble_account_info.additional_passkey_pairing = 0;
        ble_account_info.primary_device = 0;
        ble_account_info.initial_pairing = 0;
	}
	bluetooth_provider->set_capabilities_noinputnooutput(ble_account_info.prefers_le_bonding);
}
int check_passkey_match(void)
{  
	int ret = 0;
	uint32 temp_remote_passkey, temp_local_passkey;

    SYS_LOG_INF("rmt:%x local:%x", ble_account_info.remote_passkey,ble_account_info.local_passkey);

	if ((ble_account_info.remote_passkey != 0) && (ble_account_info.local_passkey != 0)){ 	   
		temp_remote_passkey = ble_account_info.remote_passkey;
		temp_local_passkey = ble_account_info.local_passkey;
	
		if (temp_remote_passkey == temp_local_passkey){
			/* TODO: 正常情况下phone_public_address此时已经保存了远端手机的地址，
			除非我们confirm request的处理晚于接收到手机端的confirm value，目前来看应该不可能 */
			bluetooth_provider->confirm_pairing(ble_account_info.phone_public_address, 1,ble_account_info.prefers_le_bonding);
			//print_hex_comm("phone_public_address:",ble_account_info.phone_public_address,6);
			uint8_t packet[AES_BLOCK_SIZE];
			passkey_response(packet);
			bluetooth_provider->notify(
			packet, 
			AES_BLOCK_SIZE, 
			BLE_PASS_KEY);
			
			ret = 1;
		}
		else{
			bluetooth_provider->confirm_pairing(ble_account_info.phone_public_address, 0,ble_account_info.prefers_le_bonding);
			ret = 0;
		}
		/* 恢复为默认的IO cap */

        ble_account_info.remote_passkey = 0;
        ble_account_info.local_passkey = 0;

        if(!ble_account_info.initial_pairing){

            key_based_pairing_finish();
        }

	}
	return ret;
}

/** Called when a pairing request has been created. */
static int created_pairing_request(uint8_t* address, uint32_t passkey)
{
	ble_account_info.local_passkey = passkey;
	memcpy(ble_account_info.phone_public_address, address, MAC_ADDRESS_LENGTH);

	if(!ble_account_info.additional_passkey_pairing){
	    return check_passkey_match();
    }
 
	return 0;
}

/** Called when we receive a passkey from the seeker. */
int passkey_written(uint8_t* packet, int packet_length)
{
	crypto_provider->decrypt(ble_account_info.current_shared_key, packet);
	if (packet[0] != 0x02){
		return 0;
	}  
	// Decode the passkey from bytes 1-3.
	ble_account_info.remote_passkey = (packet[1] << 16) + (packet[2] << 8) + (packet[3]);
	return check_passkey_match();
}

/** Called when an account key has been written by the seeker. */
int account_key_written(uint8_t* packet, int packet_length)
{
	if (packet_length == ACCOUNT_KEY_LENGTH) {
		crypto_provider->decrypt(ble_account_info.current_shared_key, packet);
		storage_provider->add_account_key(packet);
		ble_account_info.account_key_writed = 1;

		gfp_adv_len = 0;  // ���� ACCOUTN ADV 

        key_based_pairing_finish();
        bluetooth_provider->update_scan(false);
		print_hex_comm("account_key_written:",packet,packet_length);

		return 1;
	}
	return 0;
}

/** Called when an additional data has been written by the seeker. */
int additional_data_written(uint8_t* packet, int packet_length)
{
	int ret = 0;
	u8_t *counter = (u8_t *)mem_malloc(COUNTER_SIZE);
    u8_t *hamc_data = (u8_t *)mem_malloc(HMAC_SHA256_SIZE);
    u8_t *nonce_aes = (u8_t *)mem_malloc(AES_NONCEZ_SIZE + PERSONALIZED_NAME_SIZE);
    u8_t *personalized_name = (u8_t *)mem_malloc(PERSONALIZED_NAME_SIZE);
    struct tc_aes_key_sched_struct *ctx = mem_malloc(sizeof(struct tc_aes_key_sched_struct));
    struct tc_hmac_state_struct *hmac_state = mem_malloc(sizeof(struct tc_hmac_state_struct));

	u8_t personalized_name_len;

	if((!counter) || (!hamc_data) || (!nonce_aes)||
		(!personalized_name) || (!ctx) || (!hmac_state)){
		SYS_LOG_ERR("MALLOC ERROR");
		ret = -1;
		goto exit;
	}

	if(packet_length <= HMAC_SHA256_HEAD8 + AES_NONCEZ_SIZE){
		SYS_LOG_ERR("ERROR");
		ret = -1;
		goto exit;
	}
	memset(personalized_name,0,PERSONALIZED_NAME_SIZE);

	memcpy(counter + AES_COUNTER_SIZE,packet + AES_COUNTER_SIZE,AES_NONCEZ_SIZE);
	memcpy(nonce_aes,packet + HMAC_SHA256_HEAD8,packet_length - HMAC_SHA256_HEAD8);

	tc_hmac_set_key(hmac_state,ble_account_info.current_shared_key,16);
	tc_hmac_init(hmac_state);
	tc_hmac_update(hmac_state, nonce_aes,packet_length - HMAC_SHA256_HEAD8);
	tc_hmac_final(hamc_data, HMAC_SHA256_SIZE, hmac_state);

	personalized_name_len =  packet_length - HMAC_SHA256_HEAD8 - AES_NONCEZ_SIZE;

	tc_aes128_set_encrypt_key(ctx, ble_account_info.current_shared_key);
	tc_ctr_mode(personalized_name, personalized_name_len , nonce_aes + AES_NONCEZ_SIZE ,personalized_name_len, counter, ctx);
	//print_hex_comm("PERSONALLIZED:",personalized_name,personalized_name_len);
	bluetooth_provider->update_personalized_name(personalized_name,personalized_name_len,true);

exit:
    free_pointer(counter);
    free_pointer(hamc_data);
    free_pointer(nonce_aes);
    free_pointer(personalized_name);
    free_pointer(ctx);
    free_pointer(hmac_state);
    return ret;
}
/** Called when an additional passkey has been written by the seeker. */
int additional_passkey_written(uint8_t* packet, int packet_length)
{
#if NEARBY_FP_BLE_CSIS
    gfp_ble_tws_sync_t tws_sync;

	if (packet_length == AES_ADDITIONAL_KEY_SIZE_BYTES) {
		crypto_provider->decrypt(ble_account_info.current_shared_key, packet);

        if(packet[ADDITIONAL_PASSKEY_TYPE_OFFSET] != ADDITIONAL_PASSKEY_SEEKER_FLAG){
            return -1;
        }
        tws_sync.cmd = FP_TWS_SYNC_SEEKER_PASSKEY;
        tws_sync.len = ADDITIONAL_PASSKEY_SIZE_BYTES;
        memcpy(tws_sync.data,&packet[ADDITIONAL_PASSKEY_PASSKEY_OFFSET],ADDITIONAL_PASSKEY_SIZE_BYTES);

        bluetooth_provider->lea_tws_tx((uint8_t *)&tws_sync,sizeof(gfp_ble_tws_sync_t));
	}
    print_hex_comm("additional_passkey_written:",packet,packet_length);

    if (auth_stop_cb)
        auth_stop_cb();

    if (auth_start_cb)
        auth_start_cb(20000);
#endif

    return 0;
}

void get_fast_pair_account_data(uint8_t *out, uint8_t *len ,fp_battery_value_t *battery)
{
	uint8_t size, index;
	uint8_t Account_Key_Data[23]; 
    uint8_t salt[NEARBY_FP_SALT_SIZE];

	index = 0;
	Account_Key_Data[index++] = 0; // Flags
	if (storage_provider->get_num_account_keys() > 0)
	{
		size = get_bloom_filter_size();
		Account_Key_Data[index++] = (size & 0x0f) << 4;  // Field length and type
		if(battery != NULL){
			generate_bloom_filter_with_battery(&Account_Key_Data[index],salt,battery);
		}
		else{
			generate_bloom_filter(&Account_Key_Data[index],salt);
		}
		index += size;
		Account_Key_Data[index++] = SALT_FIELD_LENGTH_AND_TYPE_BYTE;
        for (int si = 0; si < NEARBY_FP_SALT_SIZE; si++){
            Account_Key_Data[index++] = salt[si];
        }

		if(battery != NULL){
		   memcpy(&Account_Key_Data[index],battery,sizeof(fp_battery_value_t));
		   index += sizeof(fp_battery_value_t);
		}
	}
	memcpy(out, Account_Key_Data, index);
	*len = index;
}

/** Resets the headset by clearing storage and stopping discoverability. */
void fastpair_reset(void)
{
	//bluetooth_provider->stop_discoverable();
	storage_provider->clear();
	ble_account_info.headset_initiates_pairing = 0;
	ble_account_info.remote_passkey = 0;
	ble_account_info.local_passkey = 0;
}

void fastpair_timeout_handle(os_work *work)
{
	SYS_LOG_INF("fp timeout:%d fp start:%d",bluetooth_provider->is_pairing(),ble_account_info.fp_started);
    key_based_pairing_finish();
    bluetooth_provider->update_scan(false);
}

u8_t gfp_service_data_get(u8_t *data)
{
	u8_t j = 0;
	uint8_t len;

	if (!bluetooth_provider)
		return 0;

	data[j++] = FAST_PAIR_SERVICE & 0xff;
	data[j++] = (FAST_PAIR_SERVICE >> 8) & 0xff;

	if(bluetooth_provider->is_pairing()/* || (bt_manager_is_wait_connect())*/)
	{
		memcpy(&data[j], fp_provider.module_id, 3);	
		j += 3;
	}
	else if (ble_account_info.account_key_writed == 1)
	{
		if (gfp_adv_len > 5)
		{
			memcpy(&data[0], gfp_adv_data, 31);
			j = gfp_adv_len;
		}
		else
		{
			get_fast_pair_account_data(&data[j], &len, NULL);
			j += len;
		}
	}else if (ble_account_info.account_key_writed == 0)
	{
		memset(&data[j],0, 2);
		j += 2;	
	}
	
	gfp_adv_len = j;
	memcpy(gfp_adv_data, &data[0], 31);
	//print_hex_comm("FP BROADCAST:",gfp_adv_data,gfp_adv_len);
	return j;
}

u8_t gfp_service_account_key_status(void)
{
	return ble_account_info.account_key_writed;
}

void broadcasting_exit(void)
{
	SYS_LOG_INF("fastpair_broadcasting_exit\n");
	if (!bluetooth_provider)
		return;
}

void account_key_clear(void)
{
	if (!storage_provider)
        return;

	storage_provider->clear();
	memset(&ble_account_info, 0, sizeof(gfp_ble_info_t));;
}

void personalized_name_clear(void)
{
	u8_t name_buffer[PERSONALIZED_NAME_SIZE];

	if (!bluetooth_provider)
        return;

    memset(name_buffer,0,PERSONALIZED_NAME_SIZE);
	bluetooth_provider->update_personalized_name(name_buffer,PERSONALIZED_NAME_SIZE,false);
}

void personalized_name_update(uint8_t *name,uint8_t size)
{
    uint8_t len = 0;
    u8_t name_buffer[PERSONALIZED_NAME_SIZE];

    if (!bluetooth_provider || !name)
        return;

    memset(name_buffer,0,PERSONALIZED_NAME_SIZE);

    if(size >= PERSONALIZED_NAME_SIZE){
        len = PERSONALIZED_NAME_SIZE - 1;
    }
    else{
        len = size;
    }
    memcpy(name_buffer,name,len);

    bluetooth_provider->update_personalized_name(name_buffer,PERSONALIZED_NAME_SIZE,false);
}

const FastPair pairer = {
	.convert_ecdh_key_to_aes	   = convert_ecdh_key_to_aes,
	.generate_bloom_filter		   = generate_bloom_filter,
	.get_bloom_filter_size		   = get_bloom_filter_size,
	.get_model_id				   = get_model_id,
	.get_private_anti_spoofing_key = get_private_anti_spoofing_key,
	.init_key_based_pairing 	   = init_key_based_pairing,
	.key_based_pairing_response    = key_based_pairing_response,
	.perform_key_based_pairing	   = perform_key_based_pairing,
	.passkey_response			   = passkey_response,
	.created_pairing_request	   = created_pairing_request,
	.passkey_written			   = passkey_written,
	.account_key_written		   = account_key_written,
	.additional_data_written	   = additional_data_written,
	//.set_pair_mode				   = set_pair_mode,
	.account_key_clear			   = account_key_clear,
	.reset						   = fastpair_reset,
};

///////////////////////////////////////////////////////////////////
// Main entry point, begins fast pairing.
///////////////////////////////////////////////////////////////////

FastPair* fast_pair(
	const uint8_t* model_id,
	const uint8_t* private_anti_spoofing_key,
	BluetoothProvider* bluetooth,
	StorageProvider* storage,
	CryptoProvider* crypto) {

	memset(&ble_account_info, 0, sizeof(gfp_ble_info_t));
	
	memcpy(fp_provider.module_id,model_id,3);
	stored_private_anti_spoofing_key = (uint8_t*)private_anti_spoofing_key;
	bluetooth_provider = bluetooth;
	storage_provider = storage;
	crypto_provider = crypto;

	bluetooth->set_pairing_request_callback(created_pairing_request);
	bluetooth->get_public_address((uint8_t *)(fp_provider.local_br_addr));
	bluetooth->get_ble_address((uint8_t *)(fp_provider.ble_addr));

	if(storage_provider->get_num_account_keys() > 0){
		ble_account_info.account_key_writed = 1;
	}
	return (FastPair*)&pairer;
}

void btsrv_gfp_timeout_start_cb(btsrv_gfp_auth_timeout_start cb)
{
	auth_start_cb = cb;
}

void btsrv_gfp_timeout_stop_cb(btsrv_gfp_auth_timeout_stop cb)
{
	auth_stop_cb = cb;
}

void btsrv_gfp_fixed_chan_forbid_cb(btsrv_gfp_fixed_chan_forbid cb)
{
	fixed_chan_forbid_cb = cb;
}

ssize_t btsrv_gfp_read_message_stream_psm(struct bt_conn *conn,
				const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset)
{
    ble_account_info.message_stream_psm.status = FP_MESSAGE_STREAM_PSM_STATE_READY;
    ble_account_info.message_stream_psm.psm[0] = 0x00;
    ble_account_info.message_stream_psm.psm[1] = MESSAGE_STREAM_PSM;
	return bt_gatt_attr_read(conn, attr, buf, len, offset, &ble_account_info.message_stream_psm,
				 MESSAGE_STREAM_PSM_SIZE);
}

