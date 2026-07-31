/** @file
 * @brief Advance Audio Remote controller Profile header.
 */

/*
 * Copyright (c) 2015-2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __BT_AVRCP_CONTROLLER_H
#define __BT_AVRCP_CONTROLLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <bluetooth/avrcp.h>


struct bt_avrcp_app_cb {
	void (*connected)(void *session_hdl, struct bt_conn *conn);
	void (*disconnected)(void *session_hdl, struct bt_conn *conn);
	void (*notify)(void *session_hdl, struct bt_conn *conn, u8_t event_id, u8_t status);
};

/* Register avrcp sdp */
int bt_avrcp_controller_register_sdp(void);

/* Register avrcp app callback function */
int bt_avrcp_controller_register_cb(struct bt_avrcp_app_cb *cb);

/* Connect to avrcp TG device */
struct bt_avrcp *bt_avrcp_controller_connect(struct bt_conn *conn);

int bt_avrcp_controller_disconnect(void *session);

/* Send avrcp control key */
int bt_avrcp_controller_pass_through_cmd(struct bt_avrcp *session, avrcp_op_id opid);

/* Send avrcp control continue key(AVRCP_OPERATION_ID_REWIND, AVRCP_OPERATION_ID_FAST_FORWARD) */
int bt_avrcp_controller_pass_through_continue_cmd(struct bt_avrcp *session,
										avrcp_op_id opid, bool start);

#if defined(CONFIG_BT_PTS_TEST)
int bt_avrcp_controller_get_capabilities(void *session);
int bt_avrcp_controller_get_play_status(void *session);
int bt_avrcp_controller_register_notification(void *session);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __BT_AVRCP_CONTROLLER_H */
