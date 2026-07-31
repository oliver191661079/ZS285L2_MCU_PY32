#ifndef STORAGE_PROVIDER_H
#define STORAGE_PROVIDER_H

#include <stdint.h>

/* The list must be able to store at least five keys (that is, there must be at least 80 
bytes of space dedicated to this list). Providers can optionally store more than this, 
they just must make sure that the keys will fit inside of their advertising packet.  */
#define MAX_NUM_KEYS 5
#define ACCOUNT_KEY_LENGTH 16

/** Provides storage functionality for 16-byte account keys. */
typedef struct {
  /** Gets the number of account keys currently stored. */
  int (*get_num_account_keys)();

  /** Gets the account key stored at the provided index. */
  void (*get_account_key)(int index, uint8_t* output);

  /** Adds a new account key to storage. */
  void (*add_account_key)(uint8_t* account_key);

  /** Resets storage. Visible for testing only. */
  void (*clear)();
} StorageProvider;

#endif    // STORAGE_PROVIDER_H
