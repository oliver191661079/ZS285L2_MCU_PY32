#ifndef CRYPTO_PROVIDER_H
#define CRYPTO_PROVIDER_H

#include <stdint.h>

/** Provides crypto functionality. */
typedef struct {
  /** Hashes a value of the given length into the output. */
  void (*hash)(uint8_t* input, int length, uint8_t* output);

  /** Encrypts a value given a secret and overrides the value. */
  void (*encrypt)(uint8_t* secret, uint8_t* value);

  /** Decrypts a value given a secret and overrides the value. */
  void (*decrypt)(uint8_t* secret, uint8_t* value);

  /** Generates a key pair to be used with public key crypto. */
  void (*generate_key_pair)(uint8_t* public, uint8_t* private);

  /** Generates a shared secret provided a public key and private key. */
  void (*generate_shared_secret)(
    uint8_t* public, uint8_t* private, uint8_t* output);
} CryptoProvider;

#endif    // CRYPTO_PROVIDER_H
