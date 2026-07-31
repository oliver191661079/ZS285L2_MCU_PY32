/**
 * @file hci_ecc.c
 * HCI ECC emulation
 */

/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <atomic.h>
#include <misc/stack.h>
#include <misc/byteorder.h>
#include <tinycrypt/constants.h>
#include <tinycrypt/utils.h>
#include <tinycrypt/ecc.h>
#include <tinycrypt/ecc_dh.h>

#include <acts_bluetooth/bluetooth.h>
#include <acts_bluetooth/conn.h>
#include <acts_bluetooth/hci.h>
#include <drivers/bluetooth/hci_driver.h>

extern void hci_data_log_debug(bool send, struct net_buf *buf);

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_DEBUG_HCI_CORE)
#define LOG_MODULE_NAME bt_hci_ecc
#include "common/log.h"

#include "hci_ecc.h"
#ifdef CONFIG_BT_HCI_RAW
#include <acts_bluetooth/hci_raw.h>
#include "hci_raw_internal.h"
#else
#include "hci_core.h"
#endif

//0:static_thread, 1:dynamic_thread, 2:work_queue
#define HCI_ECC_THREAD_MODE		2

enum {
	ECC_PUBLIC_KEY = 0,
	ECC_DH_KEY,
	ECC_NUM,
};

enum {
	PENDING_PUB_KEY,
	PENDING_DHKEY,
	PENDING_EXIT,
	ECC_DEINIT,
	/* Total number of flags - must be at the end of the enum */
	NUM_FLAGS,
};

enum {
	PENDING_USED,
	NUM_USED,
};

#if	(HCI_ECC_THREAD_MODE == 1)
#include "common_internal.h"
#define ECC_STACK_SIZE		(CONFIG_BT_HCI_ECC_STACK_SIZE + sizeof(struct k_thread))

static uint8_t *ecc_thread_stack[ECC_NUM];
static int ecc_start_thread(uint8_t ecc_thread_type);
#elif (HCI_ECC_THREAD_MODE == 2)
#define ECC_MAX_NUM		(CONFIG_BT_MAX_CONN - CONFIG_BT_MAX_BR_CONN)
struct ecc_info {
	struct k_delayed_work delayed_work;
	uint8_t ecc_type;
	atomic_t used;
}ecc_device[ECC_MAX_NUM];

#define ECC_STACK_SIZE		(CONFIG_BT_HCI_ECC_STACK_SIZE + sizeof(struct k_thread))
static K_THREAD_STACK_DEFINE(ecc_stack, ECC_STACK_SIZE);
struct k_work_q ecc_work_q;

static int ecc_work_init(void);
static int ecc_work_deinit(void);
static int ecc_submit(uint8_t ecc_type);
static void hci_ecc_work_hander(struct k_work *item);
#else
static struct k_thread ecc_thread_data;
static BT_STACK_NOINIT(ecc_thread_stack, CONFIG_BT_HCI_ECC_STACK_SIZE);
#endif

/* based on Core Specification 4.2 Vol 3. Part H 2.3.5.6.1 */
static const uint8_t debug_private_key_be[32] = {
	0x3f, 0x49, 0xf6, 0xd4, 0xa3, 0xc5, 0x5f, 0x38,
	0x74, 0xc9, 0xb3, 0xe3, 0xd2, 0x10, 0x3f, 0x50,
	0x4a, 0xff, 0x60, 0x7b, 0xeb, 0x40, 0xb7, 0x99,
	0x58, 0x99, 0xb8, 0xa6, 0xcd, 0x3c, 0x1a, 0xbd,
};

#if defined(CONFIG_BT_USE_DEBUG_KEYS)
static const uint8_t debug_public_key[64] = {
	0xe6, 0x9d, 0x35, 0x0e, 0x48, 0x01, 0x03, 0xcc, 0xdb, 0xfd, 0xf4, 0xac,
	0x11, 0x91, 0xf4, 0xef, 0xb9, 0xa5, 0xf9, 0xe9, 0xa7, 0x83, 0x2c, 0x5e,
	0x2c, 0xbe, 0x97, 0xf2, 0xd2, 0x03, 0xb0, 0x20, 0x8b, 0xd2, 0x89, 0x15,
	0xd0, 0x8e, 0x1c, 0x74, 0x24, 0x30, 0xed, 0x8f, 0xc2, 0x45, 0x63, 0x76,
	0x5c, 0x15, 0x52, 0x5a, 0xbf, 0x9a, 0x32, 0x63, 0x6d, 0xeb, 0x2a, 0x65,
	0x49, 0x9c, 0x80, 0xdc
};
#endif

static ATOMIC_DEFINE(flags, NUM_FLAGS);

#if	(HCI_ECC_THREAD_MODE == 1)
static void hci_ecc_public_key_finish_work(struct k_work *work);
K_WORK_DEFINE(ecc_pub_key_finish_work, hci_ecc_public_key_finish_work);
static void hci_ecc_dh_key_finish_work(struct k_work *work);
K_WORK_DEFINE(ecc_dh_key_finish_work, hci_ecc_dh_key_finish_work);

#else
static K_SEM_DEFINE(cmd_sem, 0, 1);
#endif

static struct {
	uint8_t private_key_be[32];

	union {
		uint8_t public_key_be[64];
		uint8_t dhkey_be[32];
	};
} ecc;

static void send_cmd_status(uint16_t opcode, uint8_t status)
{
	struct bt_hci_evt_cmd_status *evt;
	struct bt_hci_evt_hdr *hdr;
	struct net_buf *buf;

	BT_DBG("opcode %x status %x", opcode, status);

	buf = bt_buf_get_evt(BT_HCI_EVT_CMD_STATUS, false, K_FOREVER);
	bt_buf_set_type(buf, BT_BUF_EVT);

	hdr = net_buf_add(buf, sizeof(*hdr));
	hdr->evt = BT_HCI_EVT_CMD_STATUS;
	hdr->len = sizeof(*evt);

	evt = net_buf_add(buf, sizeof(*evt));
	evt->ncmd = 1U;
	evt->opcode = sys_cpu_to_le16(opcode);
	evt->status = status;

	if (IS_ENABLED(CONFIG_BT_RECV_IS_RX_THREAD)) {
		bt_recv_prio(buf);
	} else {
		bt_recv(buf);
	}
}

static uint8_t generate_keys(void)
{
#if !defined(CONFIG_BT_USE_DEBUG_KEYS)
	do {
		int rc;

		rc = uECC_make_key(ecc.public_key_be, ecc.private_key_be,
				   &curve_secp256r1);
		if (rc == TC_CRYPTO_FAIL) {
			BT_ERR("Failed to create ECC public/private pair");
			return BT_HCI_ERR_UNSPECIFIED;
		}

	/* make sure generated key isn't debug key */
	} while (memcmp(ecc.private_key_be, debug_private_key_be, 32) == 0);
#else
	sys_memcpy_swap(ecc.public_key_be, debug_public_key, 32);
	sys_memcpy_swap(&ecc.public_key_be[32], &debug_public_key[32], 32);
	memcpy(ecc.private_key_be, debug_private_key_be, 32);
#endif
	return 0;
}

static void emulate_le_p256_public_key_cmd(void)
{
	struct bt_hci_evt_le_p256_public_key_complete *evt;
	struct bt_hci_evt_le_meta_event *meta;
	struct bt_hci_evt_hdr *hdr;
	struct net_buf *buf;
	uint8_t status;

	BT_DBG("");

	status = generate_keys();

	buf = bt_buf_get_rx(BT_BUF_EVT, K_FOREVER);

	hdr = net_buf_add(buf, sizeof(*hdr));
	hdr->evt = BT_HCI_EVT_LE_META_EVENT;
	hdr->len = sizeof(*meta) + sizeof(*evt);

	meta = net_buf_add(buf, sizeof(*meta));
	meta->subevent = BT_HCI_EVT_LE_P256_PUBLIC_KEY_COMPLETE;

	evt = net_buf_add(buf, sizeof(*evt));
	evt->status = status;

	if (status) {
		(void)memset(evt->key, 0, sizeof(evt->key));
	} else {
		/* Convert X and Y coordinates from big-endian (provided
		 * by crypto API) to little endian HCI.
		 */
		sys_memcpy_swap(evt->key, ecc.public_key_be, 32);
		sys_memcpy_swap(&evt->key[32], &ecc.public_key_be[32], 32);
	}

	atomic_clear_bit(flags, PENDING_PUB_KEY);

	bt_recv(buf);
}

static void emulate_le_generate_dhkey(void)
{
	struct bt_hci_evt_le_generate_dhkey_complete *evt;
	struct bt_hci_evt_le_meta_event *meta;
	struct bt_hci_evt_hdr *hdr;
	struct net_buf *buf;
	int ret;

	ret = uECC_valid_public_key(ecc.public_key_be, &curve_secp256r1);
	if (ret < 0) {
		BT_ERR("public key is not valid (ret %d)", ret);
		ret = TC_CRYPTO_FAIL;
	} else {
		ret = uECC_shared_secret(ecc.public_key_be, ecc.private_key_be,
					 ecc.dhkey_be, &curve_secp256r1);
	}

	buf = bt_buf_get_rx(BT_BUF_EVT, K_FOREVER);

	hdr = net_buf_add(buf, sizeof(*hdr));
	hdr->evt = BT_HCI_EVT_LE_META_EVENT;
	hdr->len = sizeof(*meta) + sizeof(*evt);

	meta = net_buf_add(buf, sizeof(*meta));
	meta->subevent = BT_HCI_EVT_LE_GENERATE_DHKEY_COMPLETE;

	evt = net_buf_add(buf, sizeof(*evt));

	if (ret == TC_CRYPTO_FAIL) {
		evt->status = BT_HCI_ERR_UNSPECIFIED;
		(void)memset(evt->dhkey, 0xff, sizeof(evt->dhkey));
	} else {
		evt->status = 0U;
		/* Convert from big-endian (provided by crypto API) to
		 * little-endian HCI.
		 */
		sys_memcpy_swap(evt->dhkey, ecc.dhkey_be, sizeof(ecc.dhkey_be));
	}

	atomic_clear_bit(flags, PENDING_DHKEY);

	bt_recv(buf);
}

#if (HCI_ECC_THREAD_MODE == 0)
static void ecc_thread(void *p1, void *p2, void *p3)
{
	while (true) {
		k_sem_take(&cmd_sem, K_FOREVER);

		if (atomic_test_bit(flags, PENDING_EXIT)) {
			atomic_clear_bit(flags, PENDING_EXIT);
			return;
		} else if (atomic_test_bit(flags, PENDING_PUB_KEY)) {
			emulate_le_p256_public_key_cmd();
		} else if (atomic_test_bit(flags, PENDING_DHKEY)) {
			emulate_le_generate_dhkey();
		} else {
			__ASSERT(0, "Unhandled ECC command");
		}
	}
}
#endif

static void clear_ecc_events(struct net_buf *buf)
{
	struct bt_hci_cp_le_set_event_mask *cmd;

	cmd = (void *)(buf->data + sizeof(struct bt_hci_cmd_hdr));

	/*
	 * don't enable controller ECC events as those will be generated from
	 * emulation code
	 */
	cmd->events[0] &= ~0x80; /* LE Read Local P-256 PKey Compl */
	cmd->events[1] &= ~0x01; /* LE Generate DHKey Compl Event */
}

static void le_gen_dhkey(struct net_buf *buf)
{
	struct bt_hci_cp_le_generate_dhkey *cmd;
	uint8_t status;

	if (atomic_test_bit(flags, PENDING_PUB_KEY)) {
		status = BT_HCI_ERR_CMD_DISALLOWED;
		goto send_status;
	}

	if (buf->len < sizeof(struct bt_hci_cp_le_generate_dhkey)) {
		status = BT_HCI_ERR_INVALID_PARAM;
		goto send_status;
	}

	if (atomic_test_and_set_bit(flags, PENDING_DHKEY)) {
		status = BT_HCI_ERR_CMD_DISALLOWED;
		goto send_status;
	}

#if	(HCI_ECC_THREAD_MODE == 1)
	if (ecc_start_thread(ECC_DH_KEY)) {
		atomic_clear_bit(flags, PENDING_DHKEY);
		status = BT_HCI_ERR_CMD_DISALLOWED;
		goto send_status;
	}
#elif (HCI_ECC_THREAD_MODE == 2)
	if (ecc_submit(ECC_DH_KEY)) {
		status = BT_HCI_ERR_CMD_DISALLOWED;
		goto send_status;
	}
#endif

	cmd = (void *)buf->data;
	/* Convert X and Y coordinates from little-endian HCI to
	 * big-endian (expected by the crypto API).
	 */
	sys_memcpy_swap(ecc.public_key_be, cmd->key, 32);
	sys_memcpy_swap(&ecc.public_key_be[32], &cmd->key[32], 32);
#if	(HCI_ECC_THREAD_MODE == 0)
	k_sem_give(&cmd_sem);
#endif
	status = BT_HCI_ERR_SUCCESS;

send_status:
	net_buf_unref(buf);
	send_cmd_status(BT_HCI_OP_LE_GENERATE_DHKEY, status);
}

static void le_p256_pub_key(struct net_buf *buf)
{
	uint8_t status;

	net_buf_unref(buf);

	if (atomic_test_bit(flags, PENDING_DHKEY)) {
		status = BT_HCI_ERR_CMD_DISALLOWED;
	} else if (atomic_test_and_set_bit(flags, PENDING_PUB_KEY)) {
		status = BT_HCI_ERR_CMD_DISALLOWED;
	} else {
#if	(HCI_ECC_THREAD_MODE == 1)
		if (ecc_start_thread(ECC_PUBLIC_KEY)) {
			atomic_clear_bit(flags, PENDING_PUB_KEY);
			status = BT_HCI_ERR_CMD_DISALLOWED;
		} else {
			status = BT_HCI_ERR_SUCCESS;
		}
#elif (HCI_ECC_THREAD_MODE == 2)
		if (ecc_submit(ECC_PUBLIC_KEY)) {
			status = BT_HCI_ERR_CMD_DISALLOWED;
		} else {
			status = BT_HCI_ERR_SUCCESS;
		}
#else
		k_sem_give(&cmd_sem);
		status = BT_HCI_ERR_SUCCESS;
#endif
	}

	send_cmd_status(BT_HCI_OP_LE_P256_PUBLIC_KEY, status);
}

int bt_hci_ecc_send(struct net_buf *buf)
{
	if (bt_buf_get_type(buf) == BT_BUF_CMD) {
		struct bt_hci_cmd_hdr *chdr = (void *)buf->data;

		switch (sys_le16_to_cpu(chdr->opcode)) {
		case BT_HCI_OP_LE_P256_PUBLIC_KEY:
			net_buf_pull(buf, sizeof(*chdr));
			le_p256_pub_key(buf);
			return 0;
		case BT_HCI_OP_LE_GENERATE_DHKEY:
			net_buf_pull(buf, sizeof(*chdr));
			le_gen_dhkey(buf);
			return 0;
		case BT_HCI_OP_LE_SET_EVENT_MASK:
			clear_ecc_events(buf);
			break;
		default:
			break;
		}
	}

	hci_data_log_debug(true, buf);
	return bt_dev.drv->send(buf);
}

int default_CSPRNG(uint8_t *dst, unsigned int len)
{
	return !bt_rand(dst, len);
}

void bt_hci_ecc_init(void)
{
#if	(HCI_ECC_THREAD_MODE == 1)
	atomic_clear_bit(flags, ECC_DEINIT);
#elif (HCI_ECC_THREAD_MODE == 2)
	ecc_work_init();
#else
	k_thread_create(&ecc_thread_data, ecc_thread_stack,
			CONFIG_BT_HCI_ECC_STACK_SIZE, ecc_thread,
			NULL, NULL, NULL, K_PRIO_PREEMPT(10), 0, K_NO_WAIT);
	//k_thread_name_set(&ecc_thread_data, "BT ECC");
#endif
}

/* Actions add start */

#if	(HCI_ECC_THREAD_MODE == 1)
static void hci_ecc_public_key_finish_work(struct k_work *work)
{
	if (ecc_thread_stack[ECC_PUBLIC_KEY]) {
        bt_free(ecc_thread_stack[ECC_PUBLIC_KEY]);
        ecc_thread_stack[ECC_PUBLIC_KEY] = NULL;
	}

	if (!ecc_thread_stack[ECC_DH_KEY]
		&& !ecc_thread_stack[ECC_PUBLIC_KEY]) {
		atomic_clear_bit(flags, PENDING_EXIT);
	}
}

static void hci_ecc_dh_key_finish_work(struct k_work *work)
{
	if (ecc_thread_stack[ECC_DH_KEY]) {
		bt_free(ecc_thread_stack[ECC_DH_KEY]);
		ecc_thread_stack[ECC_DH_KEY] = NULL;
	}

	if (!ecc_thread_stack[ECC_DH_KEY]
		&& !ecc_thread_stack[ECC_PUBLIC_KEY]) {
		atomic_clear_bit(flags, PENDING_EXIT);
	}
}

static void ecc_thread(void *p1, void *p2, void *p3)
{
	if (atomic_test_bit(flags, PENDING_PUB_KEY)) {
		emulate_le_p256_public_key_cmd();
		atomic_clear_bit(flags, PENDING_PUB_KEY);
		k_thread_priority_set(k_current_get(), -CONFIG_NUM_COOP_PRIORITIES);
		k_work_submit(&ecc_pub_key_finish_work);

	} else if (atomic_test_bit(flags, PENDING_DHKEY)) {
		emulate_le_generate_dhkey();
		atomic_clear_bit(flags, PENDING_DHKEY);
		k_thread_priority_set(k_current_get(), -CONFIG_NUM_COOP_PRIORITIES);
		k_work_submit(&ecc_dh_key_finish_work);

	} else {
		__ASSERT(0, "Unhandled ECC command");
	}
}

static int ecc_start_thread(uint8_t ecc_thread_type)
{
	if (atomic_test_bit(flags, ECC_DEINIT)) {
		return -EIO;
	}

	if (ecc_thread_type >= ECC_NUM
		|| ecc_thread_stack[ecc_thread_type]) {
		return -EIO;
	}

	ecc_thread_stack[ecc_thread_type] = bt_malloc(ECC_STACK_SIZE);
	if (!ecc_thread_stack[ecc_thread_type]) {
		return -ENOMEM;
	}

	atomic_set_bit(flags, PENDING_EXIT);
	k_thread_create((struct k_thread *)ecc_thread_stack[ecc_thread_type], (k_thread_stack_t)&((ecc_thread_stack[ecc_thread_type])[sizeof(struct k_thread)]),
			(ECC_STACK_SIZE - sizeof(struct k_thread)), ecc_thread,
			NULL, NULL, NULL, K_PRIO_PREEMPT(CONFIG_BT_ECC_PRIO), 0, K_NO_WAIT);

	return 0;
}
#elif (HCI_ECC_THREAD_MODE == 2)
static int ecc_work_init(void)
{
	k_work_q_start(&ecc_work_q, ecc_stack, ECC_STACK_SIZE, K_PRIO_PREEMPT(CONFIG_BT_ECC_PRIO));
	for (int i=0; i<ECC_MAX_NUM; i++) {
		ecc_device[i].used = ATOMIC_INIT(0);
		k_delayed_work_init(&ecc_device[i].delayed_work, hci_ecc_work_hander);
	}
	atomic_clear_bit(flags, ECC_DEINIT);
	return 0;
}

static int ecc_work_deinit(void)
{
	int err;

	for (int i=0; i<ECC_MAX_NUM; i++) {
		if (atomic_test_bit(&ecc_device[i].used, PENDING_USED)) {
			err = k_delayed_work_cancel_sync(&ecc_device[i].delayed_work, K_MSEC(1500));
			if (err) {
				BT_ERR("Cancel fail: %d\r\n", err);
			}
			atomic_clear_bit(&ecc_device[i].used, PENDING_USED);
		}
	}
	atomic_set_bit(flags, ECC_DEINIT);
	return 0;
}

static void hci_ecc_work_hander(struct k_work *item)
{
	struct ecc_info *device = CONTAINER_OF(item, struct ecc_info, delayed_work);

	SYS_LOG_DBG("hci_ecc_work_hander: %d\r\n", device->ecc_type);
	if (device->ecc_type == ECC_PUBLIC_KEY) {
		emulate_le_p256_public_key_cmd();
		atomic_clear_bit(&device->used, PENDING_USED);
	} else if (device->ecc_type == ECC_DH_KEY) {
		emulate_le_generate_dhkey();
		atomic_clear_bit(&device->used, PENDING_USED);
	} else {
		__ASSERT(0, "Unhandled ECC command_1");
	}
}

static int ecc_submit(uint8_t ecc_type)
{
	int i = 0;
	int err = 0;

	if (ecc_type >= ECC_NUM) {
		BT_ERR("Param invalid!");
		return -EIO;
	}

	if (atomic_test_bit(flags, ECC_DEINIT)) {
		BT_ERR("No initialization!");
		return -EIO;
	}

	for (i=0; i<ECC_MAX_NUM; i++) {
		if (!atomic_test_bit(&ecc_device[i].used, PENDING_USED)) {
			atomic_set_bit(&ecc_device[i].used, PENDING_USED);
			ecc_device[i].ecc_type = ecc_type;
			err = k_delayed_work_submit_to_queue(&ecc_work_q, &ecc_device[i].delayed_work, 0);
			if (err) {
				BT_ERR("Submit fail: %d\r\n", err);
			}
			break;
		}
	}
	if (i >= ECC_MAX_NUM) {
		BT_ERR("No memory!");
		return -EIO;
	}

	return err;
}
#endif

void bt_hci_ecc_deinit(void)
{
#if (HCI_ECC_THREAD_MODE == 2)
	ecc_work_deinit();
#else
#if	(HCI_ECC_THREAD_MODE == 1)
	atomic_set_bit(flags, ECC_DEINIT);
#else
	atomic_set_bit(flags, PENDING_EXIT);
	k_sem_give(&cmd_sem);
#endif

	while (atomic_test_bit(flags, PENDING_EXIT)) {
		k_sleep(K_MSEC(50));
	}
#endif
}

/* Actions add end */
