/* keys.c - Bluetooth key handling */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <string.h>
#include <stdlib.h>
#include <atomic.h>
#include <misc/util.h>

//#include <settings/settings.h>

#include <acts_bluetooth/bluetooth.h>
#include <acts_bluetooth/conn.h>
#include <acts_bluetooth/hci.h>

#define BT_DBG_ENABLED IS_ENABLED(CONFIG_BT_DEBUG_KEYS)
#define LOG_MODULE_NAME bt_keys
#include "common/log.h"

#include "common/rpa.h"
#include "common_internal.h"
#include "gatt_internal.h"
#include "hci_core.h"
#include "smp.h"
//#include "settings.h"
#include "property.h"
#include "keys.h"
#include <hex_str.h>
#include "conn_internal.h"

static struct bt_keys key_pool[CONFIG_BT_MAX_PAIRED - CONFIG_BT_MAX_BR_PAIRED];

#define BT_KEYS_STORAGE_LEN_COMPAT (BT_KEYS_STORAGE_LEN - sizeof(uint32_t))

#if IS_ENABLED(CONFIG_BT_KEYS_OVERWRITE_OLDEST)
static uint32_t aging_counter_val;
static struct bt_keys *last_keys_updated;

struct key_data {
	bool in_use;
	uint8_t id;
};

static void find_key_in_use(struct bt_conn *conn, void *data)
{
	struct key_data *kdata = data;
	struct bt_keys *key;

	__ASSERT_NO_MSG(conn != NULL);
	__ASSERT_NO_MSG(data != NULL);

	if (conn->state == BT_CONN_CONNECTED) {
		key = bt_keys_find_addr(conn->id, bt_conn_get_dst(conn));
		if (key == NULL) {
			return;
		}

		/* Ensure that the reference returned matches the current pool item */
		if (key == &key_pool[kdata->id]) {
			kdata->in_use = true;
			BT_DBG("Connected device %s is using key_pool[%d]",
				bt_addr_le_str(bt_conn_get_dst(conn)), kdata->id);
		}
	}
}

static bool key_is_in_use(uint8_t id)
{
	struct key_data kdata = { false, id };

	bt_conn_foreach(BT_CONN_TYPE_ALL, find_key_in_use, &kdata);

	return kdata.in_use;
}

#endif /* CONFIG_BT_KEYS_OVERWRITE_OLDEST */

struct bt_keys *bt_keys_get_addr(uint8_t id, const bt_addr_le_t *addr)
{
	struct bt_keys *keys;
	int i;
	size_t first_free_slot = ARRAY_SIZE(key_pool);

	BT_DBG("%s", bt_addr_le_str(addr));

	for (i = 0; i < ARRAY_SIZE(key_pool); i++) {
		keys = &key_pool[i];

		if (keys->id == id && !bt_addr_le_cmp(&keys->addr, addr)) {
			return keys;
		}

		if (first_free_slot == ARRAY_SIZE(key_pool) &&
		    !bt_addr_le_cmp(&keys->addr, BT_ADDR_LE_ANY)) {
			first_free_slot = i;
		}
	}

#if IS_ENABLED(CONFIG_BT_KEYS_OVERWRITE_OLDEST)
	if (first_free_slot == ARRAY_SIZE(key_pool)) {
		struct bt_keys *oldest = NULL;
		bt_addr_le_t oldest_addr;

		for (i = 0; i < ARRAY_SIZE(key_pool); i++) {
			struct bt_keys *current = &key_pool[i];
			bool key_in_use = key_is_in_use(i);

			if (key_in_use) {
				continue;
			}

			if ((oldest == NULL) || (current->aging_counter < oldest->aging_counter)) {
				oldest = current;
			}
		}

		if (oldest == NULL) {
			BT_ERR("all keys in use, unable to create keys for %s", bt_addr_le_str(addr));
			return NULL;
		}

		/* Use a copy as bt_unpair will clear the oldest key. */
		bt_addr_le_copy(&oldest_addr, &oldest->addr);
		bt_unpair(oldest->id, &oldest_addr);
		if (!bt_addr_le_cmp(&oldest->addr, BT_ADDR_LE_ANY)) {
			first_free_slot = oldest - &key_pool[0];
		}
	}

#endif  /* CONFIG_BT_KEYS_OVERWRITE_OLDEST */
	if (first_free_slot < ARRAY_SIZE(key_pool)) {
		keys = &key_pool[first_free_slot];
		keys->id = id;
		bt_addr_le_copy(&keys->addr, addr);
#if IS_ENABLED(CONFIG_BT_KEYS_OVERWRITE_OLDEST)
		keys->aging_counter = ++aging_counter_val;
		last_keys_updated = keys;
#endif  /* CONFIG_BT_KEYS_OVERWRITE_OLDEST */
		BT_DBG("created %p for %s", keys, bt_addr_le_str(addr));
		return keys;
	}

	BT_ERR("unable to create keys for %s", bt_addr_le_str(addr));

	return NULL;
}

void bt_foreach_bond(uint8_t id, void (*func)(const struct bt_bond_info *info,
					   void *user_data),
		     void *user_data)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(key_pool); i++) {
		struct bt_keys *keys = &key_pool[i];

		if (keys->keys && keys->id == id) {
			struct bt_bond_info info;

			bt_addr_le_copy(&info.addr, &keys->addr);
			func(&info, user_data);
		}
	}
}

void bt_keys_foreach(int type, void (*func)(struct bt_keys *keys, void *data),
		     void *data)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(key_pool); i++) {
		if ((key_pool[i].keys & type)) {
			func(&key_pool[i], data);
		}
	}
}

struct bt_keys *bt_keys_find(int type, uint8_t id, const bt_addr_le_t *addr)
{
	int i;

	BT_DBG("type %d %s", type, bt_addr_le_str(addr));

	for (i = 0; i < ARRAY_SIZE(key_pool); i++) {
		if ((key_pool[i].keys & type) && key_pool[i].id == id &&
		    !bt_addr_le_cmp(&key_pool[i].addr, addr)) {
			return &key_pool[i];
		}
	}

	return NULL;
}

struct bt_keys *bt_keys_get_type(int type, uint8_t id, const bt_addr_le_t *addr)
{
	struct bt_keys *keys;

	BT_DBG("type %d %s", type, bt_addr_le_str(addr));

	keys = bt_keys_find(type, id, addr);
	if (keys) {
		return keys;
	}

	keys = bt_keys_get_addr(id, addr);
	if (!keys) {
		return NULL;
	}

	bt_keys_add_type(keys, type);

	return keys;
}

struct bt_keys *bt_keys_find_irk(uint8_t id, const bt_addr_le_t *addr)
{
	int i;

	BT_DBG("%s", bt_addr_le_str(addr));

	if (!bt_addr_le_is_rpa(addr)) {
		return NULL;
	}

	for (i = 0; i < ARRAY_SIZE(key_pool); i++) {
		if (!(key_pool[i].keys & BT_KEYS_IRK)) {
			continue;
		}

		if (key_pool[i].id == id &&
		    !bt_addr_cmp(&addr->a, &key_pool[i].irk.rpa)) {
			BT_DBG("cached RPA %s for %s",
			       bt_addr_str(&key_pool[i].irk.rpa),
			       bt_addr_le_str(&key_pool[i].addr));
			return &key_pool[i];
		}
	}

	for (i = 0; i < ARRAY_SIZE(key_pool); i++) {
		if (!(key_pool[i].keys & BT_KEYS_IRK)) {
			continue;
		}

		if (key_pool[i].id != id) {
			continue;
		}

		if (bt_rpa_irk_matches(key_pool[i].irk.val, &addr->a)) {
			BT_DBG("RPA %s matches %s",
			       bt_addr_str(&key_pool[i].irk.rpa),
			       bt_addr_le_str(&key_pool[i].addr));

			bt_addr_copy(&key_pool[i].irk.rpa, &addr->a);

			return &key_pool[i];
		}
	}

	BT_DBG("No IRK for %s", bt_addr_le_str(addr));

	return NULL;
}

static void match_le_conn(struct bt_conn *conn, void *data)
{
	struct bt_keys *keys = (struct bt_keys *)data;
	const bt_addr_le_t *addr = bt_conn_get_dst(conn);

	__ASSERT_NO_MSG(conn != NULL);
	__ASSERT_NO_MSG(data != NULL);

	if (!bt_addr_le_is_rpa(addr)) {
		return;
	}

	if (conn->state == BT_CONN_CONNECTED
		&& bt_rpa_irk_matches(keys->irk.val, &addr->a)) {
		BT_INFO("key %s matches conn %s",
			bt_addr_le_str(&keys->addr),
			bt_addr_le_str(addr));

		bt_addr_copy(&keys->irk.rpa, &addr->a);

		/*
		 * Update connection address and notify about identity
		 * resolved only if connection wasn't already reported
		 * with identity address. This may happen if IRK was
		 * present before ie. due to re-pairing.
		 */
		if (!bt_addr_le_is_identity(addr)) {
			bt_addr_le_copy((bt_addr_le_t *)addr, (const bt_addr_le_t *)&keys->addr);
			bt_conn_identity_resolved(conn);
		}

		if (conn->le.keys && (conn->le.keys != keys)) {
			bt_keys_clear(conn->le.keys);
			conn->le.keys = NULL;
		}
		conn->le.keys = keys;
		BT_INFO("match keys:%p\n",keys);
	}
}

void bt_keys_match_le_conn(struct bt_keys *keys)
{
	bt_conn_foreach(BT_CONN_TYPE_LE, match_le_conn, (void *)keys);
}

struct bt_keys *bt_keys_find_addr(uint8_t id, const bt_addr_le_t *addr)
{
	int i;

	BT_DBG("%s", bt_addr_le_str(addr));

	for (i = 0; i < ARRAY_SIZE(key_pool); i++) {
		if (key_pool[i].id == id &&
		    !bt_addr_le_cmp(&key_pool[i].addr, addr)) {
			return &key_pool[i];
		}
	}

	return NULL;
}

void bt_keys_add_type(struct bt_keys *keys, int type)
{
	keys->keys |= type;
}

/* Actions add start */
static int bt_keys_get_id(struct bt_keys *keys)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(key_pool); i++) {
		if (keys == &key_pool[i]) {
			return i;
		}
	}

	return -EINVAL;
}

static int bt_keys_get_str(char *str, uint8_t id)
{
	char id_str[4];

	u8_to_dec(id_str, sizeof(id_str), id);

	strcpy(str, CFG_BT_KEYS);
	str += strlen(CFG_BT_KEYS);
	*(str++) = '_';
	strcpy(str, id_str);

	return 0;
}

/* find key by id */
#ifdef CONFIG_BT_PROPERTY
static struct bt_keys *bt_keys_find_id(uint8_t id)
{
	if (id >= ARRAY_SIZE(key_pool)) {
		return NULL;
	}

	return &key_pool[id];
}
#endif
/* Actions add end */

void bt_keys_clear(struct bt_keys *keys)
{
	BT_DBG("%s (keys 0x%04x)", bt_addr_le_str(&keys->addr), keys->keys);
	int ret;

	if (keys->state & BT_KEYS_ID_ADDED) {
		bt_id_del(keys);
	}

	if (IS_ENABLED(CONFIG_BT_PROPERTY)) {
		uint8_t id;
		char key[BT_PROPERTY_KEY_MAX];

		/* Delete stored keys from flash */
		ret = bt_keys_get_id(keys);
		if (ret < 0) {
			BT_ERR("cannot get id:%s\n",bt_addr_le_str(&keys->addr));
			return;
		} else
			id = (uint8_t)ret;

		bt_keys_get_str(key, id);
		BT_DBG("Deleting key %s", log_strdup(key));
		bt_property_delete(key);
	}

	(void)memset(keys, 0, sizeof(*keys));
}

#if defined(CONFIG_BT_PROPERTY)
int bt_keys_store(struct bt_keys *keys)
{
	char key[BT_PROPERTY_KEY_MAX];
	int err, ret;
	uint8_t id;

#if 0
	if (keys->id) {
		char id[4];

		u8_to_dec(id, sizeof(id), keys->id);
		bt_settings_encode_key(key, sizeof(key), "keys", &keys->addr,
				       id);
	} else {
		bt_settings_encode_key(key, sizeof(key), "keys", &keys->addr,
				       NULL);
	}
#endif

	ret = bt_keys_get_id(keys);
	if (ret < 0) {
		BT_ERR("cannot get id:%s\n",bt_addr_le_str(&keys->addr));
		return -EINVAL;
	} else
		id = (uint8_t)ret;

	bt_keys_get_str(key, id);

	err = bt_property_set(key, (char *)keys, sizeof(struct bt_keys));
	if (err) {
		BT_ERR("Failed to save keys (err %d):%s", err, bt_addr_le_str(&keys->addr));
		return err;
	}

	BT_DBG("Stored keys for %s (%s)", bt_addr_le_str(&keys->addr),
	       log_strdup(key));

	return 0;
}

static int keys_set(uint8_t id, property_read_cb read_cb, char *cb_arg)
{
#if 0
	struct bt_keys *keys;
	bt_addr_le_t addr;
	uint8_t id;
	ssize_t len;
	int err;
	char val[BT_KEYS_STORAGE_LEN];
	const char *next;

	if (!name) {
		BT_ERR("Insufficient number of arguments");
		return -EINVAL;
	}
#endif

	struct bt_keys *keys;
	int len;
	char val[BT_KEYS_STORAGE_LEN];

	len = read_cb(cb_arg, val, sizeof(val));
	if (len < 0) {
		BT_DBG("Keys %s not exist", cb_arg);
		return -EINVAL;
	}

	BT_DBG("name %s val %s", log_strdup(cb_arg),
	       (len) ? bt_hex(val, sizeof(val)) : "(null)");

#if 0
	err = bt_settings_decode_key(name, &addr);
	if (err) {
		BT_ERR("Unable to decode address %s", name);
		return -EINVAL;
	}

	settings_name_next(name, &next);

	if (!next) {
		id = BT_ID_DEFAULT;
	} else {
		id = strtol(next, NULL, 10);
	}
#endif

	if (!len) {
		//keys = bt_keys_find(BT_KEYS_ALL, id, &addr);
		keys = bt_keys_find_id(id);
		if (keys) {
			(void)memset(keys, 0, sizeof(*keys));
			BT_DBG("Cleared keys for %s", bt_addr_le_str(&keys->addr));
		} else {
			BT_WARN("Unable to find deleted keys for %d",
				id);
		}

		return 0;
	}

	//keys = bt_keys_get_addr(id, &addr);
	keys = bt_keys_find_id(id);
	if (!keys) {
		BT_ERR("Failed to allocate keys for %d", id);
		return -ENOMEM;
	}
	if (len != BT_KEYS_STORAGE_LEN) {
		do {
			/* Load shorter structure for compatibility with old
			 * records format with no counter.
			 */
			if (IS_ENABLED(CONFIG_BT_KEYS_OVERWRITE_OLDEST) &&
			    len == BT_KEYS_STORAGE_LEN_COMPAT) {
				BT_WARN("Keys for %s have no aging counter",
					bt_addr_le_str(&keys->addr));
				memcpy(keys->storage_start, val, len);
				continue;
			}

			BT_ERR("Invalid key length %zd != %zu", len,
			       BT_KEYS_STORAGE_LEN);
			bt_keys_clear(keys);

			return -EINVAL;
		} while (0);
	} else {
		memcpy(keys, val, len);
	}

	BT_DBG("Successfully restored keys for %s", bt_addr_le_str(&keys->addr));
#if IS_ENABLED(CONFIG_BT_KEYS_OVERWRITE_OLDEST)
	if (aging_counter_val < keys->aging_counter) {
		aging_counter_val = keys->aging_counter;
	}
#endif  /* CONFIG_BT_KEYS_OVERWRITE_OLDEST */
	return 0;
}

static void id_add(struct bt_keys *keys, void *user_data)
{
	bt_id_add(keys);
}

static int keys_commit(void)
{
	BT_DBG("");

	/* We do this in commit() rather than add() since add() may get
	 * called multiple times for the same address, especially if
	 * the keys were already removed.
	 */
	if (IS_ENABLED(CONFIG_BT_CENTRAL) && IS_ENABLED(CONFIG_BT_PRIVACY)) {
		bt_keys_foreach(BT_KEYS_ALL, id_add, NULL);
	} else {
		bt_keys_foreach(BT_KEYS_IRK, id_add, NULL);
	}

	return 0;
}

#if 0
SETTINGS_STATIC_HANDLER_DEFINE(bt_keys, "bt/keys", NULL, keys_set, keys_commit,
			       NULL);
#endif

void bt_keys_load(void)
{
	int i;
	char key[BT_PROPERTY_KEY_MAX];

	for (i = 0; i < ARRAY_SIZE(key_pool); i++) {
		bt_keys_get_str(key, i);
		keys_set(i, bt_property_get, key);
	}

	keys_commit();
}

#endif /* CONFIG_BT_PROPERTY */

#if IS_ENABLED(CONFIG_BT_KEYS_OVERWRITE_OLDEST)
void bt_keys_update_usage(uint8_t id, const bt_addr_le_t *addr)
{
	struct bt_keys *keys = bt_keys_find_addr(id, addr);

	if (!keys) {
		return;
	}

	if (last_keys_updated == keys) {
		return;
	}

	keys->aging_counter = ++aging_counter_val;
	last_keys_updated = keys;

	BT_DBG("Aging counter for %s is set to %u", bt_addr_le_str(addr),
	       keys->aging_counter);

	if (IS_ENABLED(CONFIG_BT_KEYS_SAVE_AGING_COUNTER_ON_PAIRING)) {
		bt_keys_store(keys);
	}
}

#endif  /* CONFIG_BT_KEYS_OVERWRITE_OLDEST */

int acts_le_keys_find_link_key(const bt_addr_le_t *addr)
{
	struct bt_keys *p_key = NULL;
	int i;

	for (i = 0; i < ARRAY_SIZE(key_pool); i++) {
		p_key = bt_keys_find_addr(i,addr);
		if (p_key) {
			return i;
		}
	}
	return -ENODEV;
}

void acts_le_keys_clear_link_key(const bt_addr_le_t *addr)
{
#ifdef CONFIG_BT_PROPERTY
	struct bt_keys *keys = NULL;
	char key[BT_PROPERTY_KEY_MAX];
	int i;

	for (i = 0; i < ARRAY_SIZE(key_pool); i++) {
		if (addr) {
			keys = bt_keys_find_addr(i,addr);
			if (keys) {
				bt_keys_clear(keys);
				break;
			}
		} else {
			keys = bt_keys_find_id(i);
			if (keys) {
				memset(keys, 0, sizeof(*keys));
			}

			bt_keys_get_str(key, i);
			bt_property_delete(key);
		}
	}
#endif
}

uint8_t acts_le_keys_get_link_key_num(void)
{
	uint8_t num = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(key_pool); i++) {
		if (key_pool[i].keys) {
			num++;
		}
	}
	return num;
}

void acts_le_keys_unpair_device(const bt_addr_le_t *addr)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(key_pool); i++) {
		if (!bt_addr_le_cmp(&key_pool[i].addr, addr)) {
			bt_unpair(key_pool[i].id, &key_pool[i].addr);
			bt_keys_clear(&key_pool[i]);
		}
	}
}

void acts_le_keys_clear_list(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(key_pool); i++) {
		if (bt_addr_le_cmp(&key_pool[i].addr, BT_ADDR_LE_ANY)) {
			bt_unpair(key_pool[i].id, &key_pool[i].addr);
			bt_keys_clear(&key_pool[i]);
		}
	}
}

bool acts_le_keys_is_bond(const bt_addr_le_t *addr)
{
	int i;

	if (!addr) {
		return false;
	}

	for (i = 0; i < ARRAY_SIZE(key_pool); i++) {
		if (key_pool[i].keys
			&& !bt_addr_le_cmp(&key_pool[i].addr, addr)) {
			return true;
		}
	}
	return false;
}

uint8_t bt_le_get_paired_devices(bt_addr_le_t dev_buf[], uint8_t buf_count)
{
	uint8_t i;
	uint8_t count = 0;
	if (!dev_buf || buf_count == 0) {
		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(key_pool); i++) {
		if (bt_addr_le_cmp(&key_pool[i].addr, BT_ADDR_LE_ANY)) {
			bt_addr_le_copy(&dev_buf[count++], &key_pool[i].addr);
			if (count >= buf_count){
				break;
			}
		}
	}

	return count;
}

uint8_t bt_le_get_devices(bt_addr_le_t dev_buf[], uint8_t buf_count)
{
	uint8_t i;
	uint8_t count = 0;
	if (!dev_buf || buf_count == 0) {
		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(key_pool); i++) {
		bt_addr_le_copy(&dev_buf[count++], &key_pool[i].addr);
		if (count >= buf_count){
			break;
		}
	}


	return count;
}


uint8_t bt_le_get_paired_devices_by_type(uint8_t addr_type, bt_addr_le_t dev_buf[], uint8_t buf_count)
{
	uint8_t i;
	uint8_t count = 0;
	if (!dev_buf || buf_count == 0) {
		return 0;
	}

	for (i = 0; i < ARRAY_SIZE(key_pool); i++) {
		if (key_pool[i].addr.type == addr_type && bt_addr_le_cmp(&key_pool[i].addr, BT_ADDR_LE_ANY)) {
			bt_addr_le_copy(&dev_buf[count++], &key_pool[i].addr);
			if (count >= buf_count){
				break;
			}
		}
	}

	return count;
}

bool bt_le_get_last_paired_device(bt_addr_le_t* dev)
{
	struct bt_keys *last = &key_pool[0];

	for (uint8_t i = 1; i < ARRAY_SIZE(key_pool); i++) {
		struct bt_keys *current = &key_pool[i];

		if (current->aging_counter > last->aging_counter) {
			last = current;
		}
	}

	if (last->keys && bt_addr_le_cmp(&last->addr, BT_ADDR_LE_ANY)) {
		bt_addr_le_copy(dev, &last->addr);
		return true;
	}

	return false;
}

bool bt_le_clear_device(bt_addr_le_t* dev)
{
	uint8_t i;

	for (i = 0; i < ARRAY_SIZE(key_pool); i++) {
		if (!bt_addr_le_cmp(&key_pool[i].addr, dev)) {
			bt_keys_clear(&key_pool[i]);
			return true;
		}
	}
	return false;
}

