/** @file
 *  @brief Bluetooth SPP handling
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __BT_SPP_INTERNAL_H
#define __BT_SPP_INTERNAL_H

/**
 * @brief SPP
 * @defgroup SPP
 * @ingroup bluetooth
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

#define CFG_BT_MAX_SPP_CHANNEL 4

void bt_spp_conn_disconnect_notify(struct bt_conn *conn);

/* For snoop tws add start */
bool bt_spp_ready_sync_info(struct bt_conn *conn);
int bt_spp_get_sync_info(struct bt_conn *conn, void *info);
int bt_spp_set_sync_info(struct bt_conn *conn, void *info);
void bt_spp_dump_info(void);
/* For snoop tws add end */

#ifdef __cplusplus
}
#endif

/**
 * @}
 */
#endif /* __BT_SPP_INTERNAL_H */
