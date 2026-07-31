/** @file keys_br_store.h
 * @brief Bluetooth store br keys.
 *
 * Copyright (c) 2019 Actions Semi Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __KEYS_BR_STORE_H__
#define __KEYS_BR_STORE_H__

int bt_store_read_linkkey(const bt_addr_t *addr, void *key, uint8_t* flags);
int bt_store_check_linkkey(const bt_addr_t *addr);
void bt_store_write_linkkey(const bt_addr_t *addr, const void *key, uint8_t flag);
void bt_store_clear_linkkey(const bt_addr_t *addr);

void bt_store_share_db_addr_valid_set(bool set);
int bt_store_br_init(void);
int bt_store_br_deinit(void);

int bt_store_get_linkkey(struct br_linkkey_info *info, uint8_t cnt);
int bt_store_update_linkkey(struct br_linkkey_info *info, uint8_t cnt);
void bt_store_write_ori_linkkey(bt_addr_t *addr, uint8_t *link_key);
void *bt_store_sync_get_linkkey_info(uint16_t *len);
void bt_store_sync_set_linkkey_info(void *data, uint16_t len, uint8_t local_is_master);

#endif /* __KEYS_BR_STORE_H__ */
