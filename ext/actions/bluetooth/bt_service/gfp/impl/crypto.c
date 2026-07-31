#include  <tinycrypt/aes.h>
#include  <tinycrypt/sha256.h>
#include  <tinycrypt/ecc_dh.h>

#include "crypto.h"
//#include "tinycrypt/ecc_dh.h"

/////////////////////////////////////////////////////////////////
// Cryptography Provider.
//
// Provides examples of how cryptographic functions can be 
// implemented. The included libraries could be used, or any
// functions that come pre-baked onto the chipset.
/////////////////////////////////////////////////////////////////

void hash(uint8_t* input, int length, uint8_t* output)
{
    tc_sha256_digest(input, length, output);
}

void encrypt(uint8_t* secret, uint8_t* value)
{
    struct tc_aes_key_sched_struct ctx;
    tc_aes128_set_encrypt_key(&ctx, secret);
    tc_aes_encrypt(value, value, &ctx);
}

void decrypt(uint8_t* secret, uint8_t* value)
{
    struct tc_aes_key_sched_struct ctx;
    tc_aes128_set_decrypt_key(&ctx, secret);
    tc_aes_decrypt(value, value, &ctx);
}

void generate_key_pair(uint8_t* public, uint8_t* private)
{
    uECC_make_key(public, private, uECC_secp256r1());
}

void generate_shared_secret(
    uint8_t* public, uint8_t* private, uint8_t* output)
{
    uECC_shared_secret(public, private, output, uECC_secp256r1());
}
const CryptoProvider crypto = {
  .hash                   = hash,
  .encrypt                = encrypt,
  .decrypt                = decrypt,
  .generate_key_pair      = generate_key_pair,
  .generate_shared_secret = generate_shared_secret,

};
void init_crypto(CryptoProvider** crypto_p)
{
    *crypto_p = (CryptoProvider*)&crypto;
}


