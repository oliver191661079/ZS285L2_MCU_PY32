#include "storage.h"
#include <helper.h>
#include <string.h>

#include <stdio.h>
#include <stdlib.h>
#include <mem_manager.h>
#include <sys_comm.h>

/////////////////////////////////////////////////////////////////
// Storage Provider.
//
// Provides in memory storage, this would not be suitable for 
// an actual implementation on a headset.
/////////////////////////////////////////////////////////////////
#define ACCOUNT_KEYS_OFF                                  (4)
#define VM_ACCONT_KEY_MAGIC                               (0x5a5a) 

/* 
	VRAM  VM_FAST_PAIR 
	OFFSET     SIZE      ITEM
	0          4         account_keys_length
	4          15*16     account_keys
*/
struct account_keys_info
{
	uint16_t magic;
	uint8_t keys_len;
	uint8_t reserve[1];

	uint8_t account_keys[MAX_NUM_KEYS][ACCOUNT_KEY_LENGTH];
};
#define VM_ACCONT_KEY_SIZE                                (sizeof(struct account_keys_info))

/** The current number of account keys that are stored. */
//static int account_keys_length = 0;

/** Stores the account keys in memory (up to 15 16-byte keys). */
//uint8_t account_keys[MAX_NUM_KEYS][ACCOUNT_KEY_LENGTH];
extern gfp_ble_info_t ble_account_info;

int get_num_account_keys(void)
{
	struct account_keys_info *ptr_account  = (struct account_keys_info *)mem_malloc(VM_ACCONT_KEY_SIZE);;
	static uint8_t haven_read = 0;
    int size = 0;
       
	if (haven_read == 0)
	{
		size = property_get(VM_FAST_PAIR,(char *)ptr_account,VM_ACCONT_KEY_SIZE); 
	    if ((ptr_account) && (size == VM_ACCONT_KEY_SIZE) && (ptr_account->magic == VM_ACCONT_KEY_MAGIC)){
			ble_account_info.account_keys_length = ptr_account->keys_len;
			haven_read = 1;
		}
		else{
			ble_account_info.account_keys_length = 0;
		}
	}

    mem_free(ptr_account);
  	return ble_account_info.account_keys_length;
}

void get_account_key(int index, uint8_t* output)
{
  	struct account_keys_info *buf;
	buf = (struct account_keys_info *)mem_malloc(VM_ACCONT_KEY_SIZE);
	if(!buf){
		return;
	}
	property_get(VM_FAST_PAIR,(char *)buf, VM_ACCONT_KEY_SIZE);
//	print_hex_comm("rkey:",buf, VM_ACCONT_KEY_SIZE);
	if (buf->magic == VM_ACCONT_KEY_MAGIC) {
		memcpy(output, ((uint8_t *)buf) + index * ACCOUNT_KEY_LENGTH 
			+ ACCOUNT_KEYS_OFF, ACCOUNT_KEY_LENGTH);
	}
	mem_free(buf);

//	print_hex_comm("get_account_key:",output,ACCOUNT_KEY_LENGTH);
}

void add_account_key(uint8_t* account_key)
{
  // Override the last value in the case where we have too many keys.
  // TODO(klinker41): This should override based on a last used timestamp, not
  // simply the last item in the array.
  	struct account_keys_info *buf;
    uint8_t i;

	buf = (struct account_keys_info *)mem_malloc(VM_ACCONT_KEY_SIZE);  // MAX_NUM_KEYS*ACCOUNT_KEY_LENGTH + 4
	if (!buf) {
		return;
	}

	int size;
	property_get(VM_FAST_PAIR,(char *)buf,VM_ACCONT_KEY_SIZE);

	if (ble_account_info.account_keys_length >= MAX_NUM_KEYS) {
		ble_account_info.account_keys_length = MAX_NUM_KEYS - 1;
	}

    for(i = 0; i <  MAX_NUM_KEYS - 1; i++){
        memcpy(buf->account_keys[MAX_NUM_KEYS - 1 - i] ,buf->account_keys[MAX_NUM_KEYS - 2 - i],ACCOUNT_KEY_LENGTH);
    }

    memcpy(((uint8_t *)buf) + ACCOUNT_KEYS_OFF, account_key, ACCOUNT_KEY_LENGTH);

	buf->keys_len = ++ble_account_info.account_keys_length;
	buf->magic = VM_ACCONT_KEY_MAGIC;

	size = property_set(VM_FAST_PAIR,(char *)buf,VM_ACCONT_KEY_SIZE);
	print_hex_comm("wkey:",buf, VM_ACCONT_KEY_SIZE);

	mem_free(buf);
}

void clear_account_key(void)
{
	struct account_keys_info *ptr_account = (struct account_keys_info *)mem_malloc(VM_ACCONT_KEY_SIZE);
	u8_t *name = (u8_t *)mem_malloc(PERSONALIZED_NAME_SIZE);
	if((!ptr_account) || (!name)){
		goto exit;
	}
	ptr_account->keys_len = ble_account_info.account_keys_length = 0;
	ptr_account->magic = 0xffff;

	memset(ptr_account, 0, VM_ACCONT_KEY_SIZE);
	memset(name, 0, PERSONALIZED_NAME_SIZE);
	property_set(VM_FAST_PAIR,(char *)ptr_account,VM_ACCONT_KEY_SIZE);
	property_set(VM_FAST_PERSONALIZED_NAME,(char *)name,PERSONALIZED_NAME_SIZE);

exit:
	if (ptr_account) {
		mem_free(ptr_account);
	}
	if (name) {
		mem_free(name);
	}
}

const StorageProvider storage = {
  .get_num_account_keys = get_num_account_keys,
  .get_account_key      = get_account_key,
  .add_account_key      = add_account_key,
  .clear                = clear_account_key,
};

void init_storage(StorageProvider** storage_p)
{
    *storage_p = (StorageProvider*)&storage;
}

