/** @file
 * @brief Advance Audio Distribution Profile Internal header.
 */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/* To be called when first SEP is being registered */
int bt_a2dp_init(void);
bool bt_a2dp_is_media_rx_channel(uint16_t handle, uint16_t cid);

bool bt_a2dp_ready_sync_info(struct bt_conn *conn);
int bt_a2dp_get_sync_info(struct bt_conn *conn, void *info);
int bt_a2dp_set_sync_info(struct bt_conn *conn, void *info);
void bt_a2dp_dump_info(void);
