/*
 * Copyright (c) 2017 Nordic Semiconductor ASA
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <errno.h>

#include <zephyr.h>
#include <misc/byteorder.h>

#include <acts_bluetooth/bluetooth.h>
#include <acts_bluetooth/hci.h>
#include <acts_bluetooth/conn.h>
#include <acts_bluetooth/crypto.h>

#include <tinycrypt/constants.h>
#include <tinycrypt/hmac_prng.h>
#include <tinycrypt/aes.h>
#include <tinycrypt/utils.h>

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_DEBUG_HCI_CORE)
#define LOG_MODULE_NAME bt_crypto
#include "common/log.h"

#include "hci_core.h"

static struct tc_hmac_prng_struct prng;

static uint32_t bt_rand_gen_seed(void)
{
    uint32_t ret;
    uint32_t rand_seed = k_cycle_get_32();
    uint32_t next_seed = rand_seed;

    next_seed = (next_seed * 1103515245) + 12345;
    ret = next_seed & 0xfffc0000;

    next_seed = (next_seed * 1103515245) + 12345;
    ret |= ((next_seed & 0xffe00000) >> 14);

    next_seed = (next_seed * 1103515245) + 12345;
    ret |= ((next_seed & 0xfe000000) >> 25);

    rand_seed = next_seed;
    return ret;
}

void bt_rand_generator(uint8_t* buf, uint8_t word_nums)
{
    uint32_t ret;
    int32_t i;

    for (i = 0; i < word_nums; i++) {
        ret = bt_rand_gen_seed();
        *(buf + (4 * i) + 0) = ret & 0xff;
        *(buf + (4 * i) + 1) = (ret & 0xff00) >> 8;
        *(buf + (4 * i) + 2) = (ret & 0xff0000) >> 16;
        *(buf + (4 * i) + 3) = (ret & 0xff000000) >> 24;
    }
}

static int prng_reseed(struct tc_hmac_prng_struct *h)
{
	uint8_t seed[32];
	s64_t extra;
	size_t i;
	int ret;


	for (i = 0; i < (sizeof(seed) / 8); i++) {
#ifdef CONFIG_BT_HOST_RAND_GEN
		uint8_t rand[8];
		bt_rand_generator(rand, 2);
		memcpy(&seed[i * 8], rand, 8);
#else
		struct bt_hci_rp_le_rand *rp;
		struct net_buf *rsp;

		ret = bt_hci_cmd_send_sync(BT_HCI_OP_LE_RAND, NULL, &rsp);
		if (ret) {
			return ret;
		}

		rp = (void *)rsp->data;
		memcpy(&seed[i * 8], rp->rand, 8);

		net_buf_unref(rsp);
#endif
	}

	extra = k_uptime_get();

	ret = tc_hmac_prng_reseed(h, seed, sizeof(seed), (uint8_t *)&extra,
				  sizeof(extra));
	if (ret == TC_CRYPTO_FAIL) {
		BT_ERR("Failed to re-seed PRNG");
		return -EIO;
	}

	return 0;
}

int prng_init(void)
{
#ifndef CONFIG_BT_HOST_RAND_GEN
	struct bt_hci_rp_le_rand *rp;
	struct net_buf *rsp;
#else
	uint8_t rand[8];
#endif
	int ret;

	/* Check first that HCI_LE_Rand is supported */
	if (!BT_CMD_TEST(bt_dev.supported_commands, 27, 7)) {
		return -ENOTSUP;
	}

#ifndef CONFIG_BT_HOST_RAND_GEN
	ret = bt_hci_cmd_send_sync(BT_HCI_OP_LE_RAND, NULL, &rsp);
	if (ret) {
		return ret;
	}

	rp = (void *)rsp->data;

	ret = tc_hmac_prng_init(&prng, rp->rand, sizeof(rp->rand));

	net_buf_unref(rsp);
#else
	bt_rand_generator(rand, 2);
	ret = tc_hmac_prng_init(&prng, rand, sizeof(rand));
#endif

	if (ret == TC_CRYPTO_FAIL) {
		BT_ERR("Failed to initialize PRNG");
		return -EIO;
	}

	/* re-seed is needed after init */
	return prng_reseed(&prng);
}

int bt_rand(void *buf, size_t len)
{
	int ret;

	ret = tc_hmac_prng_generate(buf, len, &prng);
	if (ret == TC_HMAC_PRNG_RESEED_REQ) {
		ret = prng_reseed(&prng);
		if (ret) {
			return ret;
		}

		ret = tc_hmac_prng_generate(buf, len, &prng);
	}

	if (ret == TC_CRYPTO_SUCCESS) {
		return 0;
	}

	return -EIO;
}

int bt_encrypt_le(const uint8_t key[16], const uint8_t plaintext[16],
		  uint8_t enc_data[16])
{
	struct tc_aes_key_sched_struct s;
	uint8_t tmp[16];

	BT_DBG("key %s", bt_hex(key, 16));
	BT_DBG("plaintext %s", bt_hex(plaintext, 16));

	sys_memcpy_swap(tmp, key, 16);

	if (tc_aes128_set_encrypt_key(&s, tmp) == TC_CRYPTO_FAIL) {
		return -EINVAL;
	}

	sys_memcpy_swap(tmp, plaintext, 16);

	if (tc_aes_encrypt(enc_data, tmp, &s) == TC_CRYPTO_FAIL) {
		return -EINVAL;
	}

	sys_mem_swap(enc_data, 16);

	BT_DBG("enc_data %s", bt_hex(enc_data, 16));

	return 0;
}

int bt_encrypt_be(const uint8_t key[16], const uint8_t plaintext[16],
		  uint8_t enc_data[16])
{
	struct tc_aes_key_sched_struct s;

	BT_DBG("key %s", bt_hex(key, 16));
	BT_DBG("plaintext %s", bt_hex(plaintext, 16));

	if (tc_aes128_set_encrypt_key(&s, key) == TC_CRYPTO_FAIL) {
		return -EINVAL;
	}

	if (tc_aes_encrypt(enc_data, plaintext, &s) == TC_CRYPTO_FAIL) {
		return -EINVAL;
	}

	BT_DBG("enc_data %s", bt_hex(enc_data, 16));

	return 0;
}
