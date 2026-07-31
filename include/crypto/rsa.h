#ifndef __RSA_H
#define __RSA_H

int rom_sha256_init(void *ctx);

int rom_sha256_update(void* ctx, const void* data, int len);

const unsigned char* rom_sha256_final(void *ctx);


#ifdef CONFIG_SOC_SECURE_BOOT

//return 0 verify ok, other value is failed
int RSA_verify_hash(const unsigned char *key_data,
               const uint8_t *signature,
               const uint32_t len,
               const uint8_t *hash,
               const uint32_t hash_len) ;

#else

static inline int RSA_verify_hash(const unsigned char *key_data,
               const uint8_t *signature,
               const uint32_t len,
               const uint8_t *hash,
               const uint32_t hash_len)
{
    return 0;
}

#endif

#endif
