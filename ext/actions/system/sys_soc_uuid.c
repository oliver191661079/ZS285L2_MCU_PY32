/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file SoC init for Actions SoC.
 */

#include <kernel.h>
#include <device.h>
#include <init.h>
#include <soc.h>
#include <arch/cpu.h>
#include <string.h>
#include <nvram_config.h>
#include <crypto/rsa.h>
#include <mem_manager.h>

#define UUID_STR_DATA_LEN       (32)
#define UUID_MSG_SIGNATURE_LEN  (256)
#define UUID_HASH_DATA_LEN      (32)
#define UUID_SIGN_MSG_NAME "uuid_msg"

struct uuid_ctx{
    uint32_t uuid[4];
    uint8_t hash_ctx[128];
    uint8_t uuid_sig_data[UUID_MSG_SIGNATURE_LEN];
	uint8_t uuid_str[UUID_STR_DATA_LEN + 1];
};

extern int hex2bin(uint8_t *dst, const char *src, unsigned long count);
extern char *bin2hex(char *dst, const void *src, unsigned long count);

static const unsigned int g_pubkey[]   = {
    0x4ce4e013,
	0x7bd615e5, 0x2258d5c1, 0x35290b4b, 0x1a9ed5b7,
	0x70c7d54e, 0x123d305c, 0x1c7a93b4, 0xde14bbff,
	0xf8868023, 0xc412cbe6, 0xcaddc6a3, 0x64e2ace8,
	0x4bd95502, 0x09292c83, 0x3e6e2bc2, 0x9aa81098,
	0xca26d53f, 0x26d13fda, 0x080e2a27, 0x4de3320d,
	0x91aa484a, 0x764f473f, 0x2c18b321, 0x4671b549,
	0x9698cc95, 0xd854c685, 0x467da24a, 0x6f9162b5,
	0x5454bf9e, 0x37388746, 0x6b55351b, 0x8fbc5a06,
	0x35644aec, 0x18e37ef4, 0xe7362914, 0x8d6f72d0,
	0x7d2d355a, 0x1bad45fc, 0x99a64dce, 0x915c263a,
	0x653a83cb, 0x7a1a7356, 0x072a99d7, 0xa87a7447,
	0x99e448bf, 0xbcf81080, 0xe2a5f344, 0x43d15e56,
	0x572e103c, 0x4db7e967, 0x328812eb, 0x3bc104fc,
	0xd7b512f5, 0xeb4db29b, 0xe27d69fe, 0x2ec22a48,
	0x36d41dc0, 0x28121ed1, 0xc9952dae, 0xca3986c0,
	0x6e192aaa, 0x553c911e, 0x18104486, 0x98965410,
	0xaae4cc12, 0x34204f67, 0x8435aacf, 0xac4dfc4d,
	0xa7ac4a56, 0x12515267, 0x67f9e4d4, 0x79d05909,
	0x873c16b9, 0x87ff0b54, 0x1966639d, 0xbc3985d0,
	0xf5938b9a, 0x68d97327, 0x76b4a3f6, 0x163fece2,
	0x762a07a6, 0x13c9b787, 0x3ca87e9a, 0x341da32c,
	0xfe733ba7, 0x857b7ba8, 0xa0368ccb, 0xd51b73a3,
	0xb13a7f9b, 0xd7911762, 0x4b6d2cea, 0xeca59f78,
	0x46fcff9f, 0x011c2db4, 0x70c2cf6a, 0xd84653e2,
	0xf1749999, 0xf2dcf481, 0x5c5af1ff, 0x152d3483,
	0xd228fbb0, 0x77d79f52, 0x53b9743e, 0x37fcc3a9,
	0xbc160c2b, 0x415d40e5, 0x5d95f37c, 0xd98979aa,
	0x04fe4b21, 0x95fbdaf5, 0xfe105ef2, 0xe181b8f1,
	0xc06bebdc, 0xa76c6d91, 0x4a4450ef, 0xd2c50838,
	0x0c7031e7, 0x87233969, 0xce268c17, 0x69866d98,
	0x6b096f24, 0x4f8ccc81, 0xbe81bcf2, 0xc7d42b9e,
	0xc0703791, 0xe2be28cd, 0x3a395b74, 0x2eac3b6e,
};

static uint8_t* sys_get_uuid_hash(struct uuid_ctx *ctx)
{
	rom_sha256_init(ctx->hash_ctx);

	rom_sha256_update(ctx->hash_ctx, ctx->uuid_str, UUID_STR_DATA_LEN);

	return (uint8_t *)rom_sha256_final(ctx->hash_ctx);
}

static int sys_uuid_verify(void)
{
    struct uuid_ctx *ctx;
    uint8_t *hash_data;
    int rlen, ret;

    ctx = mem_malloc(sizeof(struct uuid_ctx));

    if(!ctx){
        return -ENOMEM;
    }

    soc_get_system_uuid(ctx->uuid);
	bin2hex(ctx->uuid_str, ctx->uuid, 16);
	ctx->uuid_str[32] =  0;
    printk("uuid: %x-%x-%x-%x, hex=%s\r\n", ctx->uuid[0], ctx->uuid[1], ctx->uuid[2], ctx->uuid[3], ctx->uuid_str);

    hash_data = sys_get_uuid_hash(ctx);

	print_buffer_lazy("uuid sha256:", hash_data, 32);

	rlen = nvram_config_get_factory(UUID_SIGN_MSG_NAME, ctx->uuid_sig_data, UUID_MSG_SIGNATURE_LEN);
	if( rlen != UUID_MSG_SIGNATURE_LEN){
		printk("uuid nv-r: sign rlen=%d != %d ", rlen, UUID_MSG_SIGNATURE_LEN);
        mem_free(ctx);
		return -1;
	}


    ret = RSA_verify_hash((const unsigned char *)g_pubkey, ctx->uuid_sig_data, UUID_MSG_SIGNATURE_LEN, hash_data, UUID_HASH_DATA_LEN);

	if(ret != 0){
        printk("uuid verify fail %d\n", ret);
	}else{
		printk("uuid verify ok\r\n");
	}

    mem_free(ctx);

	return ret;
}

static int uuid_init(struct device *dev)
{
	sys_uuid_verify();
	return 0;
}

SYS_INIT(uuid_init, PRE_KERNEL_2, 10);




/*sign msg write to nvram*/
static const char uuid_sign_msg_hex[513] = \
"29484c36343b2ba23c53d919c7403efc2332dae0809e4816c567148a20d4e70ead6d800ab2363dd04d178c9291ce67fd69cc2fd77f1d45f99dc83fc786c227b9070f1a6243b17430a52db2352119ce5762de28484aef72322f2a025bcb1c13dfcc56f907c32e1f9e9b73054e656cdccaca53fcfaaee2bbdeb6191de6b22878a522337ec290c5f05bf57089f471d6feee2c8d99def6244f11353de9459b7a83d30943f3e1006a47390bca1c3e2ce9548016c96216b4e410ee44d23177acfca945cd29f77aa6d90803595d91f4a78f5978c163cf19f58e806328260b9c0eb851c8d7ec5c37e3e442f8eb7a0105f348501575031766eadcc8a3cc9c76a7a2531fa2";

int uuid_sign_msg_write(int is_read)
{
	char  str_msg[33];
	int i,  rlen, len;
    char *sign_msg = mem_malloc(UUID_MSG_SIGNATURE_LEN);

    if(!sign_msg){
        return -ENOMEM;
    }

	if(is_read) {
		rlen = nvram_config_get_factory(UUID_SIGN_MSG_NAME, sign_msg, UUID_MSG_SIGNATURE_LEN);
		printk("uuid nv: rlen= %d\r\n", rlen);
		str_msg[32] = 0;
		i = 0;
		while(rlen > 0){
			if( rlen > 16 )
				len = 16;
			else
				len = rlen;
			bin2hex(str_msg, &sign_msg[i*16], 16);
			printk("%d-hex:%s\r\n", i, str_msg);
			rlen -= len;
			i++;
		}
	} else{
			rlen = hex2bin(sign_msg, uuid_sign_msg_hex, UUID_MSG_SIGNATURE_LEN);
			printk("write sig msg to nv: wlen=%d\r\n", rlen);

			if(rlen == 0){
				nvram_config_set_factory(UUID_SIGN_MSG_NAME, sign_msg , UUID_MSG_SIGNATURE_LEN);
			}
	}
	return 0;
}
