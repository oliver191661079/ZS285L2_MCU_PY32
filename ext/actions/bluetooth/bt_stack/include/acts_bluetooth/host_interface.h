/** @file
 * @brief Bluetooth host interface header file.
 */

/*
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __HOST_INTERFACE_H
#define __HOST_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <acts_bluetooth/bluetooth.h>
#include <acts_bluetooth/hci.h>
#include <acts_bluetooth/conn.h>
#include <acts_bluetooth/l2cap.h>
#include <acts_bluetooth/sdp.h>
#include <acts_bluetooth/a2dp.h>
#include <acts_bluetooth/gatt.h>
#include <acts_bluetooth/hfp_hf.h>
#include <acts_bluetooth/hfp_ag.h>
#include <acts_bluetooth/avrcp_cttg.h>
#include <acts_bluetooth/spp.h>
#include <acts_bluetooth/conn.h>
#include <acts_bluetooth/gatt.h>
#include <acts_bluetooth/pbap_client.h>
#include <acts_bluetooth/map_client.h>
#include <acts_bluetooth/hid.h>
#include <acts_bluetooth/device_id.h>
#include <acts_bluetooth/iso.h>
#include <acts_bluetooth/services/fpms.h>
#include <acts_bluetooth/bip_client.h>


#define CONFIG_BT_HCI CONFIG_ACTS_BT_HCI

#define hostif_bt_addr_to_str(a, b, c)		bt_addr_to_str(a, b, c)
#define hostif_bt_addr_le_to_str(a, b, c)	bt_addr_le_to_str(a, b, c)
#define hostif_bt_addr_cmp(a, b)			bt_addr_cmp(a, b)
#define hostif_bt_addr_copy(a, b)			bt_addr_copy(a, b)

#ifdef CONFIG_BT_HCI
#define hostif_bt_stack_sync_info_len()		bt_stack_sync_info_len()
#define hostif_bt_le_stack_sync_info_len()  bt_le_stack_sync_info_len()
#define hostif_bt_read_ble_name(a, b)		bt_read_ble_name(a, b)
#else
static inline int hostif_bt_stack_sync_info_len(void)
{
	return 0;
}

static inline int hostif_bt_le_stack_sync_info_len(void)
{
	return 0;
}

static inline uint8_t hostif_bt_read_ble_name(uint8_t *name, uint8_t len)
{
	return 0;
}
#endif

/** @brief set caller to negative priority
 *
 *  @param void.
 *
 *  @return caller priority.
 */
int hostif_set_negative_prio(void);

/** @brief revert caller priority
 *
 *  @param prio.
 *
 *  @return void.
 */
void hostif_revert_prio(int prio);

/************************ hostif hci core *******************************/

/** @brief Increment a connection's reference count.
 *
 *  Increment the reference count of a connection object.
 *
 *  @note Will return NULL if the reference count is zero.
 *
 *  @param conn Connection object.
 *
 *  @return Connection object with incremented reference count, or NULL if the
 *          reference count is zero.
 */
struct bt_conn *hostif_bt_conn_ref(struct bt_conn *conn);

/** @brief Decrement a connection's reference count.
 *
 *  Decrement the reference count of a connection object.
 *
 *  @param conn Connection object.
 */
void hostif_bt_conn_unref(struct bt_conn *conn);



/** @brief Set share bluetooth address
 *
 * @param set: Set or clear
 * @param mac: Address
 *
 * @return None.
 */
void hostif_bt_set_share_bd_addr(bool set, uint8_t *mac);

/** @brief Read BR device name
 * @param name: output device name
 * @param len: buf length for read name
 *
 * @return Length of name.
 */
uint8_t hostif_bt_read_br_name(uint8_t *name, uint8_t len);

/** @brief update br local name and Names in EIR
 *
 */
void hostif_bt_update_br_name(void);


/************************ hostif hci core *******************************/
/** @brief Set bt device class
 *
 *  set bt device class. Must be the called before hostif_bt_enable.
 *
 *  @param classOfDevice bt device class
 *
 *  @return Zero on success or (negative) error code otherwise.
 */
int hostif_bt_init_class(uint32_t classOfDevice);

/** @brief write bt device class to controller directly
 *
 *  write bt device class.
 *
 *  @param classOfDevice bt device class
 *
 *  @return Zero on success or (negative) error code otherwise.
 */
int hostif_bt_write_class(uint32_t classOfDevice);

/** @brief Initialize device id
 *
 * @param device id
 *
 * @return Zero if done successfully, other indicator failed.
 */
int hostif_bt_init_device_id(uint16_t *did);

/** @brief Enable Bluetooth
 *
 *  Enable Bluetooth. Must be the called before any calls that
 *  require communication with the local Bluetooth hardware.
 *
 *  @param cb Callback to notify completion or NULL to perform the
 *  enabling synchronously.
 *
 *  @return Zero on success or (negative) error code otherwise.
 */
int hostif_bt_enable(bt_ready_cb_t cb);

/** @brief Disable Bluetooth */
int hostif_bt_disable(void);

/** @brief Register bt event callback function  */
int hostif_bt_reg_evt_cb_func(bt_evt_cb_func_t cb);

/** @brief Start BR/EDR discovery
 *
 *  Start BR/EDR discovery (inquiry) and provide results through the specified
 *  callback. When bt_br_discovery_cb_t is called it indicates that discovery
 *  has completed. If more inquiry results were received during session than
 *  fits in provided result storage, only ones with highest RSSI will be
 *  reported.
 *
 *  @param param Discovery parameters.
 *  @param cb Callback to notify discovery results.
 *
 *  @return Zero on success or error code otherwise, positive in case
 *  of protocol error or negative (POSIX) in case of stack internal error
 */
int hostif_bt_br_discovery_start(const struct bt_br_discovery_param *param,
			  bt_br_discovery_cb_t cb);

/** @brief Stop BR/EDR discovery.
 *
 *  Stops ongoing BR/EDR discovery. If discovery was stopped by this call
 *  results won't be reported
 *
 *  @return Zero on success or error code otherwise, positive in case
 *  of protocol error or negative (POSIX) in case of stack internal error
 */
int hostif_bt_br_discovery_stop(void);

/** @brief Direct set scan mode.
 *
 * Can't used bt_br_write_scan_enable and
 * bt_br_set_discoverable/bt_br_set_discoverable at same time.
 *
 *  @param scan: scan mode.
 *
 *  @return Negative if fail set to requested state or requested state has been
 *  already set. Zero if done successfully.
 */
int hostif_bt_br_write_scan_enable(uint8_t scan, complete_event_cb cb);

/** @brief BR write iac.
 *
 * @param limit_iac, true: use limit iac, false: use general iac.
 *
 *  @return Zero on success or error code otherwise, positive in case
 *  of protocol error or negative (POSIX) in case of stack internal error
 */
int hostif_bt_br_write_iac(bool limit_iac);

/** @brief BR write page scan activity
 *
 * @param interval: page scan interval.
 * @param windown: page scan windown.
 *
 *  @return Zero on success or error code otherwise, positive in case
 *  of protocol error or negative (POSIX) in case of stack internal error
 */
int hostif_bt_br_write_page_scan_activity(uint16_t interval, uint16_t windown);

/** @brief BR write inquiry scan type
 *
 * @param type: 0:Standard Scan, 1:Interlaced Scan
 *
 *  @return Zero on success or error code otherwise, positive in case
 *  of protocol error or negative (POSIX) in case of stack internal error
 */
int hostif_bt_br_write_inquiry_scan_type(uint8_t type);

/** @brief BR write page scan type
 *
 * @param type: 0:Standard Scan, 1:Interlaced Scan
 *
 *  @return Zero on success or error code otherwise, positive in case
 *  of protocol error or negative (POSIX) in case of stack internal error
 */
int hostif_bt_br_write_page_scan_type(uint8_t type);

/** @brief BR read RSSI
 *
 * @param conn: Connection object.
 * @param cb: Result callback function.
 *
 *  @return Zero on success or error code otherwise.
 */
int hostif_bt_br_read_rssi(struct bt_conn *conn, complete_event_cb cb);

/** @brief BR read link quality
 *
 * @param conn: Connection object.
 * @param cb: Result callback function.
 *
 *  @return Zero on success or error code otherwise.
 */
int hostif_bt_br_read_link_quality(struct bt_conn *conn, complete_event_cb cb);

/** @brief request remote bluetooth name
 *
 * @param addr remote bluetooth mac address
 * @param cb callback function for report remote bluetooth name
 *
 * @return Zero if done successfully, other indicator failed.
 */
int hostif_bt_remote_name_request(const bt_addr_t *addr, bt_br_remote_name_cb_t cb);

/** @brief Br set page timeout.
 *
 *  @param timeout_ms: page timeout.
 *
 *  @return Negative fail, Zero successfully.
 */
int hostif_bt_br_set_page_timeout(uint32_t timeout);

/** @brief BR write inquiry scan activity
 *
 * @param interval: inquiry scan interval.
 * @param windown: inquiry scan windown.
 *
 *  @return Zero on success or error code otherwise, positive in case
 *  of protocol error or negative (POSIX) in case of stack internal error
 */
int hostif_bt_br_write_inquiry_scan_activity(uint16_t interval, uint16_t windown);

/************************ hostif conn *******************************/
/** @brief Register connection callbacks.
 *
 *  Register callbacks to monitor the state of connections.
 *
 *  @param cb Callback struct.
 */
void hostif_bt_conn_cb_register(struct bt_conn_cb *cb);

/** @brief Unegister connection callbacks.
 *
 *  Remove the callback structure from the list of connection callbacks.
 *
 *  @param cb Callback struct. Must point to memory that remains valid.
 */
void hostif_bt_conn_cb_unregister(struct bt_conn_cb *cb);

/** @brief Get connection info
 *
 *  @param conn Connection object.
 *  @param info Connection info object.
 *
 *  @return Zero on success or (negative) error code on failure.
 */
int hostif_bt_conn_get_info(const struct bt_conn *conn, struct bt_conn_info *info);

/** @brief Get connection type
 *
 *  @param conn Connection object.
 *
 *  @return connection type.
 */
uint8_t hostif_bt_conn_get_type(const struct bt_conn *conn);

/** @brief Lookup conn Connection object by acl handle
 *
 *  @param connection acl hanlde.
 *
 *  @return conn Connection object.
 */
 struct bt_conn *hostif_bt_conn_lookup_by_handle(uint16_t handle);

/** @brief Get connection acl handle
 *
 *  @param conn Connection object.
 *
 *  @return connection acl handle.
 */
uint16_t hostif_bt_conn_get_handle(const struct bt_conn *conn);

/** @brief Get connection type
 *
 *  @param handle Connection acl handle.
 *
 *  @return connection type.
 */
uint8_t hostif_bt_conn_get_type_by_handle(uint16_t handle);

/** @brief Get connection role
 *
 *  @param handle Connection handle.
 *
 *  @return connection role.
 */
uint8_t hostif_bt_conn_get_role_by_handle(uint16_t handle);

/** @brief Get acl connection by sco connection
 *
 *  @param sco_conn Connection object.
 *
 *  @return acl connection.
 */
struct bt_conn *hostif_bt_conn_get_acl_conn_by_sco(const struct bt_conn *sco_conn);

/** @brief Set ACL snoop conn media rx channel ID
 *
 *  @param conn Connection object.
 *  @param cid Channel ID.
 *
 *  @return 0: sucess, other:  error.
 */
int hostif_bt_conn_set_snoop_media_rx_cid(struct bt_conn *conn, uint16_t cid);

/** @brief Set conn snoop switch lock
 *
 *  @param conn Connection object.
 *  @param lock Lock or unlokc switch.
 *
 *  @return 0: sucess, other:  error.
 */
int hostif_bt_conn_set_snoop_switch_lock(struct bt_conn *conn, uint8_t lock);

/** @brief Disconnect from a remote device or cancel pending connection.
 *
 *  Disconnect an active connection with the specified reason code or cancel
 *  pending outgoing connection.
 *
 *  @param conn Connection to disconnect.
 *  @param reason Reason code for the disconnection.
 *
 *  @return Zero on success or (negative) error code on failure.
 */
int hostif_bt_conn_disconnect(struct bt_conn *conn, uint8_t reason);

/** @brief Disconnect from a remote device or cancel pending connection.
 *
 *  Disconnect an active connection with the specified reason code or cancel
 *  pending outgoing connection.
 *
 *  @param handle Connection handle.
 *  @param reason Reason code for the disconnection.
 *
 *  @return Zero on success or (negative) error code on failure.
 */
int hostif_bt_conn_disconnect_by_handle(uint16_t handle, uint8_t reason);

/** @brief Disconnect gatt over br channel.
 *
 *  Disconnect gatt over br l2cap channel.
 *
 *  @param conn Connection to disconnect.
 *
 *  @return Zero on success or (negative) error code on failure.
 */
int hostif_bt_gatt_over_br_disconnect(struct bt_conn *conn);


/** @brief Initiate an BR/EDR connection to a remote device.
 *
 *  Allows initiate new BR/EDR link to remote peer using its address.
 *  Returns a new reference that the the caller is responsible for managing.
 *
 *  @param peer  Remote address.
 *  @param param Initial connection parameters.
 *
 *  @return Valid connection object on success or NULL otherwise.
 */
struct bt_conn *hostif_bt_conn_create_br(const bt_addr_t *peer,
				  const struct bt_br_conn_param *param);

/** @brief Check address is in connecting
 *
 *  @param peer  Remote address.
 *
 *  @return Valid connection object on success or NULL otherwise.
 */
struct bt_conn *hostif_bt_conn_br_acl_connecting(const bt_addr_t *addr);

/** @brief Initiate an SCO connection to a remote device.
 *
 *  Allows initiate new SCO link to remote peer using its address.
 *  Returns a new reference that the the caller is responsible for managing.
 *
 *  @param br_conn  br connection object.
 *
 *  @return Zero for success, non-zero otherwise..
 */
int hostif_bt_conn_create_sco(struct bt_conn *br_conn);

/** @brief Send sco data to conn.
 *
 *  @param conn  Sco connection object.
 *  @param data  Sco data pointer.
 *  @param len  Sco data length.
 *
 *  @return  Zero for success, non-zero otherwise..
 */
int hostif_bt_conn_send_sco_data(struct bt_conn *conn, uint8_t *data, uint8_t len);

/** @brief bluetooth conn enter sniff
 *
 * @param conn bt_conn conn
 * @param min_interval Minimum sniff interval.
 * @param min_interval Maxmum sniff interval.
 */
int hostif_bt_conn_check_enter_sniff(struct bt_conn *conn, uint16_t min_interval, uint16_t max_interval);

/** @brief bluetooth conn check and exit sniff
 *
 * @param conn bt_conn conn
 */
int hostif_bt_conn_check_exit_sniff(struct bt_conn *conn);

/** @brief bluetooth conn get rxtx count
 *
 * @param conn bt_conn conn
 *
 *  @return  rx/tx count.
 */
uint16_t hostif_bt_conn_get_rxtx_cnt(struct bt_conn *conn);

int hostif_bt_conn_qos_setup(struct bt_conn_qos_param *qos);

/** @brief Bt send actions fix cid data.
 *
 *  @param conn  Connection object.
 *  @parem cid Actions used fix channel ID.
 *  @param data send data.
 *  @param len data len.
 *
 *  @return  Zero for success, non-zero otherwise.
 */
int hostif_bt_conn_send_act_fix_cid_data(struct bt_conn *conn, uint16_t cid, uint8_t *data, uint16_t len);

int hostif_bt_conn_tws_media_send_data(struct bt_conn *conn, uint16_t cid, uint8_t *data, uint16_t len, k_timeout_t timeout);

void* hostif_bt_conn_alloc_act_fix_cid_netbuf(uint16_t len,k_timeout_t timeout);

int hostif_bt_conn_send_act_fix_cid_data_with_netbuf(struct bt_conn *conn, uint16_t cid, uint8_t *data, uint16_t len, void*netbuf);

/** @brief check br conn ready for send data.
 *
 *  @param conn  Connection object.
 *
 *  @return  0: no ready , 1:  ready.
 */
int hostif_bt_br_conn_ready_send_data(struct bt_conn *conn);

/** @brief Get conn pending packet.
 *
 * @param conn  Connection object.
 * @param host_pending Host pending packet.
 * @param controler_pending Controler pending packet.
 *
 *  @return  0: sucess, other:  error.
 */
int hostif_bt_br_conn_pending_pkt(struct bt_conn *conn, uint8_t *host_pending, uint8_t *controler_pending);

/** @brief register br switch pending rx data.
 *
 * @param cb  call back funcion.
 *
 *  @return  NONE.
 */
void hostif_bt_l2cap_br_reg_pending_rx_data_cb_func(br_pending_rx_data_cb_t cb);

/** @brief register br feature fixed_chan
 *
 * @param cb  call back funcion.
 *
 *  @return  1: allow¡ê?other: forbid.
*/
void hostif_bt_l2cap_br_reg_feature_fixed_chan_cb_func(br_feature_fixed_chan_cb_t cb);
/** @brief Add rx pending data.
 *
 * @param addr  Conn address.
 * @param data  Add data.
 * @param addr  Add data len.
 *
 *  @return  NONE.
 */
void hostif_bt_l2cap_br_add_pending_rx_data(uint8_t *addr, uint8_t *data, uint16_t len);

void hostif_bt_l2cap_br_fixed_chan_enable(uint8_t enable);

/** @brief register auth cb.
 *
 * @param cb  call back funcion.
 *
 *  @return  0: sucess, other:  error.
 */
int hostif_bt_conn_auth_cb_register(const struct bt_conn_auth_cb *cb);

/** @brief register le auth cb.
 *
 * @param cb  call back funcion.
 *
 *  @return  0: sucess, other:  error.
 */
int hostif_bt_conn_le_auth_cb_register(const struct bt_conn_auth_cb *cb);

int hostif_bt_conn_smp_confirm_reply(struct bt_conn *conn);

int hostif_bt_conn_smp_cancel(struct bt_conn *conn);

/** @brief ssp confirm reply.
 *
 * @param conn  Connection object.
 *
 *  @return  0: sucess, other:  error.
 */
int hostif_bt_conn_ssp_confirm_reply(struct bt_conn *conn);

/** @brief ssp confirm reply.
 *
 * @param conn  Connection object.
 *
 *  @return  0: sucess, other:  error.
 */
int hostif_bt_conn_ssp_confirm_neg_reply(struct bt_conn *conn);

/** @brief Bt role discovery.
 *
 *  @param conn  Connection object.
 *  @param role  return role.
 *
 *  @return  Zero for success, non-zero otherwise.
 */
int hostif_bt_conn_role_discovery(struct bt_conn *conn, uint8_t *role);

/** @brief Bt switch role.
 *
 *  @param conn  Connection object.
 *  @param role  Switch role.
 *
 *  @return  Zero for success, non-zero otherwise.
 */
int hostif_bt_conn_switch_role(struct bt_conn *conn, uint8_t role);

/** @brief Bt set supervision timeout.
 *
 *  @param conn  Connection object.
 *  @param timeout.
 *
 *  @return  Zero for success, non-zero otherwise.
 */
int hostif_bt_conn_set_supervision_timeout(struct bt_conn *conn, uint16_t timeout);

/** @brief set conn security
 *
 *  @param conn  Connection object.
 *  @param sec security level
 *
 *  @return 0 in case of success or negative value in case of error.
 */
int hostif_bt_conn_set_security(struct bt_conn *conn, bt_security_t sec);

/** @brief drop tx buffer
 *
 *  @param conn  Connection object.
 *  @param bt_conn_buffer_match only drop buffer matched
 *  @param drop_count drop buffer count
 *
 *  @return count of buffer dropped or negative value in case of error.
 */
int hostif_bt_conn_drop_tx_buffer(struct bt_conn *conn,bt_conn_buffer_match buffer_match,uint8_t drop_count);

/************************ hostif hfp *******************************/
/** @brief Register HFP HF call back function
 *
 *  Register Handsfree callbacks to monitor the state and get the
 *  required HFP details to display.
 *
 *  @param cb callback structure.
 *
 *  @return 0 in case of success or negative value in case of error.
 */
int hostif_bt_hfp_hf_register_cb(struct bt_hfp_hf_cb *cb);

/** @brief Handsfree client Send AT
 *
 *  Send specific AT commands to handsfree client profile.
 *
 *  @param conn Connection object.
 *  @param cmd AT command to be sent.
 *  @param user_cmd user command
 *
 *  @return 0 in case of success or negative value in case of error.
 */
int hostif_bt_hfp_hf_send_cmd(struct bt_conn *conn, char *user_cmd);

/** @brief Handsfree connect AG.
 *
 *  @param conn Connection object.
 *
 *  @return 0 in case of success or negative value in case of error.
 */
int hostif_bt_hfp_hf_connect(struct bt_conn *conn);

/** @brief Handsfree disconnect AG.
 *
 *  @param conn Connection object.
 *
 *  @return 0 in case of success or negative value in case of error.
 */
int hostif_bt_hfp_hf_disconnect(struct bt_conn *conn);

/************************ hostif hfp *******************************/
/** @brief Register HFP AG call back function
 *
 *  Register Handsfree callbacks to monitor the state and get the
 *  required HFP details to display.
 *
 *  @param cb callback structure.
 *
 *  @return 0 in case of success or negative value in case of error.
 */
int hostif_bt_hfp_ag_register_cb(struct bt_hfp_ag_cb *cb);

/** @brief Update hfp ag indicater
 *
 *  @param index (enum hfp_hf_ag_indicators ).
 *  @param value indicater value for index.
 *
 *  @return 0 in case of success or negative value in case of error.
 */
int hostif_bt_hfp_ag_update_indicator(uint8_t index, uint8_t value);

/** @brief get hfp ag indicater
 *
 *  @param index (enum hfp_hf_ag_indicators ).
 *
 *  @return 0 and positive in case of success or negative value in case of error.
 */
int hostif_bt_hfp_ag_get_indicator(uint8_t index);

/** @brief hfp ag send event to hf
 *
 *  @param conn  Connection object.
 *  @param event Event char string.
 * @param len Event char string length.
 *
 *  @return 0 in case of success or negative value in case of error.
 */
int hostif_bt_hfp_ag_send_event(struct bt_conn *conn, char *event, uint16_t len);

/** @brief HFP AG connect HF.
 *
 *  @param conn Connection object.
 *
 *  @return 0 in case of success or negative value in case of error.
 */
int hostif_bt_hfp_ag_connect(struct bt_conn *conn);

/** @brief HFP AG disconnect HF.
 *
 *  @param conn Connection object.
 *
 *  @return 0 in case of success or negative value in case of error.
 */
int hostif_bt_hfp_ag_disconnect(struct bt_conn *conn);

/** @brief HFP AG display current indicators. */
void hostif_bt_hfp_ag_display_indicator(void);

/************************ hostif a2dp *******************************/
/** @brief A2DP Connect.
 *
 *  @param conn Pointer to bt_conn structure.
 *  @param role a2dp as source or sink role
 *
 *  @return 0 in case of success and error code in case of error.
 *  of error.
 */
int hostif_bt_a2dp_connect(struct bt_conn *conn, uint8_t role);

/** @brief A2DP Disconnect.
 *
 *  @param conn Pointer to bt_conn structure.
 *
 *  @return 0 in case of success and error code in case of error.
 *  of error.
 */
int hostif_bt_a2dp_disconnect(struct bt_conn *conn);

/** @brief Endpoint Registration.
 *
 *  @param endpoint Pointer to bt_a2dp_endpoint structure.
 *  @param media_type Media type that the Endpoint is.
 *  @param role Role of Endpoint.
 *
 *  @return 0 in case of success and error code in case of error.
 */
int hostif_bt_a2dp_register_endpoint(struct bt_a2dp_endpoint *endpoint,
			      uint8_t media_type, uint8_t role);

/** @brief halt/resume registed endpoint.
 *
 *  This function is used for halt/resume registed endpoint
 *
 *  @param endpoint Pointer to bt_a2dp_endpoint structure.
 *  @param halt true: halt , false: resume;
 *
 *  @return 0 in case of success and error code in case of error.
 */
int hostif_bt_a2dp_halt_endpoint(struct bt_a2dp_endpoint *endpoint, bool halt);

/* Register app callback */
int hostif_bt_a2dp_register_cb(struct bt_a2dp_app_cb *cb);

/* Start a2dp play */
int hostif_bt_a2dp_start(struct bt_conn *conn);

/* Suspend a2dp play */
int hostif_bt_a2dp_suspend(struct bt_conn *conn);

/* Reconfig a2dp codec config */
int hostif_bt_a2dp_reconfig(struct bt_conn *conn, struct bt_a2dp_media_codec *codec);

/* Send delay report to source, delay_time: 1/10 milliseconds */
int hostif_bt_a2dp_send_delay_report(struct bt_conn *conn, uint16_t delay_time);

/* Send a2dp audio data */
int hostif_bt_a2dp_send_audio_data(struct bt_conn *conn, uint8_t *data, uint16_t len);

/* Get a2dp seted codec */
struct bt_a2dp_media_codec *hostif_bt_a2dp_get_seted_codec(struct bt_conn *conn);

/* Get a2dp role(source or sink) */
uint8_t hostif_bt_a2dp_get_a2dp_role(struct bt_conn *conn);

/* Get a2dp media rx channel ID */
uint16_t hostif_bt_a2dp_get_media_rx_cid(struct bt_conn *conn);

/************************ hostif spp *******************************/
/* Spp send data */
int hostif_bt_spp_send_data(uint8_t spp_id, uint8_t *data, uint16_t len);

/* Spp connect */
uint8_t hostif_bt_spp_connect(struct bt_conn *conn, uint8_t *uuid);

/* Spp disconnect */
int hostif_bt_spp_disconnect(uint8_t spp_id);

/* Spp register service */
uint8_t hostif_bt_spp_register_service(uint8_t *uuid);

/* Spp register call back */
int hostif_bt_spp_register_cb(struct bt_spp_app_cb *cb);

/************************ hostif avrcp *******************************/
/* Register avrcp app callback function */
int hostif_bt_avrcp_cttg_register_cb(struct bt_avrcp_app_cb *cb);

/* Connect to avrcp TG device */
int hostif_bt_avrcp_cttg_connect(struct bt_conn *conn);

/* Disonnect avrcp */
int hostif_bt_avrcp_cttg_disconnect(struct bt_conn *conn);

/* Send avrcp control key */
int hostif_bt_avrcp_ct_pass_through_cmd(struct bt_conn *conn, avrcp_op_id opid, bool push);

/* Target notify change to controller */
int hostif_bt_avrcp_tg_notify_change(struct bt_conn *conn, uint8_t play_state);

/* get current playback track id3 info */
int hostif_bt_avrcp_ct_get_id3_info(struct bt_conn *conn);

int hostif_bt_avrcp_ct_set_absolute_volume(struct bt_conn *conn, uint32_t param);

int hostif_bt_avrcp_ct_get_capabilities(struct bt_conn *conn);

int hostif_bt_avrcp_ct_get_play_status(struct bt_conn *conn);

int hostif_bt_avrcp_ct_register_notification(struct bt_conn *conn);

int hostif_bt_avrcp_ct_get_support_feature(struct bt_conn *conn);

/************************ hostif pbap *******************************/
/* Get phonebook */
uint8_t hostif_bt_pbap_client_get_phonebook(struct bt_conn *conn, char *path, struct bt_pbap_client_user_cb *cb);

/* Abort get phonebook */
int hostif_bt_pbap_client_abort_get(struct bt_conn *conn, uint8_t user_id);

uint8_t hostif_bt_map_client_connect(struct bt_conn *conn,char *path,struct bt_map_client_user_cb *cb);

int hostif_bt_map_client_abort_get(struct bt_conn *conn, uint8_t user_id);

int hostif_bt_map_client_disconnect(struct bt_conn *conn, uint8_t user_id);

int hostif_bt_map_client_set_folder(struct bt_conn *conn, uint8_t user_id,char *path,uint8_t flags);

uint8_t hostif_bt_map_client_get_message(struct bt_conn *conn, char *path, struct bt_map_client_user_cb *cb);

int hostif_bt_map_client_get_messages_listing(struct bt_conn *conn, uint8_t user_id,uint16_t max_list_count,uint32_t parameter_mask);

int hostif_bt_map_client_get_folder_listing(struct bt_conn *conn, uint8_t user_id);

/**********************************hostif bip*************************************/
int hostif_bt_bipc_init(bt_bip_client_evt_cb cb);

int hostif_bt_bipc_deinit(void);

uint8_t hostif_bt_bipc_connect(struct bt_conn* conn, uint16_t psm);

int hostif_bt_bipc_disconnect(struct bt_conn* conn, uint8_t user_id);

int hostif_bt_bipc_get_capabilities(struct bt_conn* conn, uint8_t user_id);

int hostif_bt_bipc_get_image_properties(struct bt_conn* conn , uint8_t user_id, char* img_handle);

int hostif_bt_bipc_get_image(struct bt_conn* conn, uint8_t user_id, char* img_handle, bt_bipc_image_desc_t* image_desc);

int hostif_bt_bipc_abort(struct bt_conn* conn, uint8_t user_id);


/************************ hostif hid *******************************/
/* register sdp hid service record */
int hostif_bt_hid_register_sdp(struct bt_sdp_attribute * hid_attrs,int attrs_size);

/* Register hid app callback function */
int hostif_bt_hid_register_cb(struct bt_hid_app_cb *cb);

/* Connect to hid device */
int hostif_bt_hid_connect(struct bt_conn *conn);

/* Disonnect hid */
int hostif_bt_hid_disconnect(struct bt_conn *conn);

/* send hid data by ctrl channel */
int hostif_bt_hid_send_ctrl_data(struct bt_conn *conn,uint8_t report_type,uint8_t * data,uint16_t len);

/* send hid data by intr channel */
int hostif_bt_hid_send_intr_data(struct bt_conn *conn,uint8_t report_type,uint8_t * data,uint16_t len);

/* send hid rsp by ctrl channel */
int hostif_bt_hid_send_rsp(struct bt_conn *conn,uint8_t status);

/* register device id info on sdp */
int hostif_bt_did_register_sdp(struct device_id_info *device_id);

int hostif_bt_sdp_pnp(struct bt_conn *conn, pnp_sdp_complete_cb cb);

/************************ hostif ble *******************************/
/** @brief Start advertising
 *
 *  Set advertisement data, scan response data, advertisement parameters
 *  and start advertising.
 *
 *  @param param Advertising parameters.
 *  @param ad Data to be used in advertisement packets.
 *  @param ad_len Number of elements in ad
 *  @param sd Data to be used in scan response packets.
 *  @param sd_len Number of elements in sd
 *
 *  @return Zero on success or (negative) error code otherwise.
 */
int hostif_bt_le_adv_start(const struct bt_le_adv_param *param,
		    const struct bt_data *ad, size_t ad_len,
		    const struct bt_data *sd, size_t sd_len);

/** @brief Stop advertising
 *
 *  Stops ongoing advertising.
 *
 *  @return Zero on success or (negative) error code otherwise.
 */
int hostif_bt_le_adv_stop(void);

/** @brief Update the connection parameters.
 *
 *  @param conn Connection object.
 *  @param param Updated connection parameters.
 *
 *  @return Zero on success or (negative) error code on failure.
 */
int hostif_bt_conn_le_param_update(struct bt_conn *conn,
			    const struct bt_le_conn_param *param);

/** @brief Register GATT service.
 *
 *  Register GATT service. Applications can make use of
 *  macros such as BT_GATT_PRIMARY_SERVICE, BT_GATT_CHARACTERISTIC,
 *  BT_GATT_DESCRIPTOR, etc.
 *
 *  @param svc Service containing the available attributes
 *
 *  @return 0 in case of success or negative value in case of error.
 */
int hostif_bt_gatt_service_register(struct bt_gatt_service *svc);

/** @brief Get ATT MTU for a connection
 *
 *  Get negotiated ATT connection MTU, note that this does not equal the largest
 *  amount of attribute data that can be transferred within a single packet.
 *
 *  @param conn Connection object.
 *
 *  @return MTU in bytes
 */
uint16_t hostif_bt_gatt_get_mtu(struct bt_conn *conn);

uint16_t hostif_bt_gatt_over_br_get_mtu(struct bt_conn *conn);

/** @brief Indicate attribute value change.
 *
 *  Send an indication of attribute value change.
 *  Note: This function should only be called if CCC is declared with
 *  BT_GATT_CCC otherwise it cannot find a valid peer configuration.
 *
 *  Note: This procedure is asynchronous therefore the parameters need to
 *  remains valid while it is active.
 *
 *  @param conn Connection object.
 *  @param params Indicate parameters.
 *
 *  @return 0 in case of success or negative value in case of error.
 */
int hostif_bt_gatt_indicate(struct bt_conn *conn,
		     struct bt_gatt_indicate_params *params);

/** @brief Notify attribute value change.
 *
 *  Send notification of attribute value change, if connection is NULL notify
 *  all peer that have notification enabled via CCC otherwise do a direct
 *  notification only the given connection.
 *
 *  The attribute object must be the so called Characteristic Value Descriptor,
 *  its usually declared with BT_GATT_DESCRIPTOR after BT_GATT_CHARACTERISTIC
 *  and before BT_GATT_CCC.
 *
 *  @param conn Connection object.
 *  @param attr Characteristic Value Descriptor attribute.
 *  @param data Pointer to Attribute data.
 *  @param len Attribute value length.
 */
int hostif_bt_gatt_notify(struct bt_conn *conn, const struct bt_gatt_attr *attr,
		   const void *data, uint16_t len);

/** @brief Exchange MTU
 *
 *  This client procedure can be used to set the MTU to the maximum possible
 *  size the buffers can hold.
 *
 *  NOTE: Shall only be used once per connection.
 *
 *  @param conn Connection object.
 *  @param params Exchange MTU parameters.
 *
 *  @return 0 in case of success or negative value in case of error.
 */
int hostif_bt_gatt_exchange_mtu(struct bt_conn *conn,
			 struct bt_gatt_exchange_params *params);

/** @brief Get ccc value
 *
 *  This procedure can be used to get ccc value
 *
 *  @param attr ccc attr.
 *  @param id  local identity address.
 *  @param addr bluetooth address.
 *
 *  @return ccc value in case of success or negative value in case of error.
 */
int hostif_bt_gatt_get_ccc_value(const struct bt_gatt_attr *attr,uint8_t id, const bt_addr_le_t *addr);

/** @brief Start (LE) scanning
 *
 *  Start LE scanning with given parameters and provide results through
 *  the specified callback.
 *
 *  @param param Scan parameters.
 *  @param cb Callback to notify scan results.
 *
 *  @return Zero on success or error code otherwise, positive in case
 *  of protocol error or negative (POSIX) in case of stack internal error
 */
int hostif_bt_le_scan_start(const struct bt_le_scan_param *param, bt_le_scan_cb_t cb);

/** @brief Stop (LE) scanning.
 *
 *  Stops ongoing LE scanning.
 *
 *  @return Zero on success or error code otherwise, positive in case
 *  of protocol error or negative (POSIX) in case of stack internal error
 */
int hostif_bt_le_scan_stop(void);

/** @brief Register property flush call back function
 *
 *  @param cb flush call back function.
 *
 *  @return Zero on success or error code otherwise, positive in case
 */
int hostif_reg_flush_nvram_cb(void *cb);

/** @brief Get store linkkey
 *
 *  @param info save get linkkey information.
 *  @param cnt number of info for save linkkey.
 *
 *  @return number of get linkkey
 */
int hostif_bt_store_get_linkkey(struct br_linkkey_info *info, uint8_t cnt);

/** @brief Update store linkkey
 *
 *  @param info linkkey for update.
 *  @param cnt number of linkkey for update.
 *
 *  @return 0: successful, other failed.
 */
int hostif_bt_store_update_linkkey(struct br_linkkey_info *info, uint8_t cnt);

/** @brief Store original linkkey
 *
 *  @param addr bluetooth address.
 *  @param link_key original linkkey.
 *
 *  @return NONE.
 */
void hostif_bt_store_write_ori_linkkey(bt_addr_t *addr, uint8_t *link_key);

/** @brief Clear store linkkey
 *
 *  @param addr  NULL: clear all, addr: clear one.
 *
 *  @return NONE.
 */
void hostif_bt_store_clear_linkkey(const bt_addr_t *addr);

/** @brief find le linkkey
 *
 *  @param addr : bluetooth address.
 *
 *  @return index of linkkey
 */
int hostif_bt_le_find_linkkey(const bt_addr_le_t *addr);

/** @brief clear le linkkey
 *
 *  @param addr  NULL: clear all, addr: clear one.
 *
 *  @return NONE.
 */
void hostif_bt_le_clear_linkkey(const bt_addr_le_t *addr);

/** @brief get le linkkey num
 *
 *  @return num of le linkkey
 */
uint8_t hostif_bt_le_get_linkkey_mum(void);

extern uint32_t libbtstack_version_dump(void);
static inline uint32_t hostif_libbtstack_version_dump(void)
{
	return libbtstack_version_dump();
}

/** @brief CSB interface */
int hostif_bt_csb_set_broadcast_link_key_seed(uint8_t *seed, uint8_t len);
int hostif_bt_csb_set_reserved_lt_addr(uint8_t lt_addr);
int hostif_bt_csb_set_CSB_broadcast(uint8_t enable, uint8_t lt_addr, uint16_t interval_min, uint16_t interval_max);
int hostif_bt_csb_set_CSB_receive(uint8_t enable, struct bt_hci_evt_sync_train_receive *param, uint16_t timeout);
int hostif_bt_csb_write_sync_train_param(uint16_t interval_min, uint16_t interval_max, uint32_t trainTO, uint8_t service_data);
int hostif_bt_csb_start_sync_train(void);
int hostif_bt_csb_set_CSB_date(uint8_t lt_addr, uint8_t *data, uint8_t len);
int hostif_bt_csb_receive_sync_train(uint8_t *mac, uint16_t timeout, uint16_t windown, uint16_t interval);
int hostif_bt_csb_broadcase_by_acl(uint16_t handle, uint8_t *data, uint16_t len);

/** @brief Adjust link time
 *
 *  @param conn Connection object..
 *  @param time Adjust time.
 *
 *  @return 0: successful, other failed.
 */
int hostif_bt_vs_adjust_link_time(struct bt_conn *conn, int16_t time);

/** @brief get conn bt clock
 *
 *  @param conn Connection object..
 *  @param bt_clk Output bt clock time by 312.5us.
 *  @param bt_intraslot Output bt clock time by 1us.
 *
 *  @return 0: successful, other failed.
 */
int hostif_bt_vs_get_bt_clock(struct bt_conn *conn, uint32_t *bt_clk, uint16_t *bt_intraslot);

/** @brief set conn tws interrupt clock
 *
 *  @param conn Connection object..
 *  @param bt_clk bt clock time by 312.5us.
 *  @param bt_intraslot bt clock time by 1us.
 *
 *  @return 0: successful, other failed.
 */
int hostif_bt_vs_set_tws_int_clock(struct bt_conn *conn, uint32_t bt_clk, uint16_t bt_intraslot);

/** @brief set conn tws interrupt enable
 *
 *  @param enable Eanble/disable tws interrupt.
 *
 *  @return 0: successful, other failed.
 */
int hostif_bt_vs_enable_tws_int(uint8_t enable);

/** @brief Enable/disable acl conn flow control.
 *
 *  @param conn Connection object.
 *  @param enable Eanble/disable.
 *
 *  @return 0: successful, other failed.
 */
int hostif_bt_vs_enable_acl_flow_control(struct bt_conn *conn, uint8_t enable);

/** @brief Read add null packet count.
 *
 *  @param conn Connection object.
 *  @param *cnt Output count.
 *
 *  @return 0: successful, other failed.
 */
int hostif_bt_vs_read_add_null_cnt(struct bt_conn *conn, uint16_t *cnt);

/** @brief Set tws sync interrupt time.
 *
 *  @param conn Connection object.
 *  @param index: Tws0/Tws1.
 *  @param mode: 0, Native timer mode, 1, us timer mode.
 *  @param bt_clk: bt clock.
 *  @param bt_slot: slot time.
 *
 *  @return 0: successful, other failed.
 */
int hostif_bt_vs_set_tws_sync_int_time(struct bt_conn *conn, uint8_t index, uint8_t mode,
	uint32_t bt_clk, uint16_t bt_slot);

/** @brief Set tws interrupt delay play time.
 *
 *  @param conn Connection object.
 *  @param index: Tws0/Tws1.
 *  @param mode: 0, Native timer mode, 1, us timer mode.
 *  @param per_int.
 *  @param bt_clk: bt clock.
 *  @param bt_slot: slot time.
 *
 *  @return 0: successful, other failed.
 */
int hostif_bt_vs_set_tws_int_delay_play_time(struct bt_conn *conn, uint8_t index, uint8_t mode,
	uint8_t per_int, uint32_t bt_clk, uint16_t bt_slot);

/** @brief Set tws interrupt clock new.
 *
 *  @param conn Connection object.
 *  @param index: Tws0/Tws1.
 *  @param mode: 0, Native timer mode, 1, us timer mode.
 *  @param bt_clk: bt clock.
 *  @param bt_slot: slot time.
 *  @param per_clk: period bt clock.
 *  @param per_slot: period slot time.
 *
 *  @return 0: successful, other failed.
 */
int hostif_bt_vs_set_tws_int_clock_new(struct bt_conn *conn, uint8_t index, uint8_t mode,
	uint32_t clk, uint16_t slot, uint32_t per_clk, uint16_t per_slot);

/** @brief enable Eanble/disable tws interrupt.
 *
 *  @param enable Eanble/disable tws interrupt.
 *  @param index: Tws0/Tws1.
 *
 *  @return 0: successful, other failed.
 */
int hostif_bt_vs_enable_tws_int_new(uint8_t enable, uint8_t index);

/** @brief Read bt us count.
 *
 *  @param *cnt Output count.
 *
 *  @return 0: successful, other failed.
 */
int hostif_bt_vs_read_bt_us_cnt(uint32_t *cnt);

/** @brief Vendor set first sync bt clock
 *
 *  @param send 0:  Rx first sync bt clock, 1: Send first sync bt clock.
 *  @param conn Connection object.
 *  @param enable 0: disable, 1: enable.
 *
 *  @return 0: successful, other failed.
 */
int hostif_bt_vs_set_1st_sync_btclk(uint8_t send, struct bt_conn *conn, uint8_t enable);

/** @brief Vendor read first sync bt clock
 *
 *  @param conn Connection object.
 *  @param bt_clk Output bt clock.
 *
 *  @return 0: successful, other failed.
 */
int hostif_bt_vs_read_1st_sync_btclk(struct bt_conn *conn, uint32_t *bt_clk);

/** @brief Vendor create acl snoop link
 *
 *  @param conn Connection object.
 *
 *  @return 0: successful, other failed.
 */
int hostif_bt_vs_create_acl_snoop_link(struct bt_conn *conn);

/** @brief Vendor disconnect acl snoop link
 *
 *  @param conn Connection object.
 *
 *  @return 0: successful, other failed.
 */
int hostif_bt_vs_disconnect_acl_snoop_link(struct bt_conn *conn);

/** @brief Vendor create sco snoop link
 *
 *  @param conn Sco connection object.
 *
 *  @return 0: successful, other failed.
 */
int hostif_bt_vs_create_sync_snoop_link(struct bt_conn *conn);

/** @brief set tws link
 *
 *  @param conn Connection object.
 *
 *  @return 0: successful, other failed.
 */
int hostif_bt_vs_set_tws_link(struct bt_conn *conn);

/** @brief vs set bt address
 *
 *  @param addr Bt address.
 *
 *  @return 0: successful, other failed.
 */
int hostif_bt_vs_write_bt_addr(bt_addr_t *addr);

/** @brief vs switch snoop link
 *
 *  @param conn Connection object.
 *
 *  @return 0: successful, other failed.
 */
int hostif_bt_vs_switch_snoop_link(struct bt_conn *conn);

/** @brief vs switch le link
 *
 *  @param conn Connection object.
 *
 *  @return 0: successful, other failed.
 */
int hostif_bt_vs_switch_le_link(struct bt_conn *conn);

/** @brief set tws visual mode
 *
 *  @param param Tws visual mode parameter
 *
 *  @return 0: successful, other failed.
 */
int hostif_bt_vs_set_tws_visual_mode(struct tws_visual_mode_param *param);

/** @brief set tws pair mode
 *
 *  @param param Tws pair mode parameter
 *
 *  @return 0: successful, other failed.
 */
int hostif_bt_vs_set_tws_pair_mode(struct tws_pair_mode_param *param);

/** @brief Vendor set snoop link active
 *
 *  @param conn Connection object.
 *
 *  @return 0: successful, other failed.
 */
int hostif_bt_vs_set_snoop_link_active(struct bt_conn *conn);
int hostif_bt_vs_set_snoop_link_inactive(struct bt_conn *conn);

/** @brief Vendor set cis link enter to background
 *
 *  @param enable : 1 enter to background.
 *
 *  @return 0: successful, other failed.
 */
int hostif_bt_vs_set_cis_link_background(uint8_t enable);

/** @brief Vendor set apll temperture compensation
 *
 *  @param enable Enable or disable apll temperture compensation.
 *
 *  @return 0: successful, other failed.
 */
int hostif_bt_vs_set_apll_temp_comp(u8_t enable);

/** @brief Vendor do apll temperture compensation
 *
 *  @return 0: successful, other failed.
 */
int hostif_bt_vs_do_apll_temp_comp(void);

/** @brief Vendor read build version of bt controller lib
 *
 * *@param str_version input buffer for version string.
 *  @param len input buffer len.
 *
 *  @return 0: successful, other failed.
 */
int hostif_bt_vs_read_bt_build_version(uint8_t* str_version, int len);

/** @brief set latency and retry for LE Master
 *
 *  @param conn Connection object..
 *  @param latency How many connection intervals Master may skip
 *  @param retry How many times Master will retry after resume
 *
 *  @return 0: successful, other failed.
 */
int hostif_bt_vs_le_set_master_latency(struct bt_conn *conn, uint16_t latency,
				uint8_t retry);
/** @brief check bt stack is ready for sync info
 *
 *  @param conn Connection object.
 *
 *  @return true: ready, false: not ready.
 */
bool hostif_bt_stack_ready_sync_info(struct bt_conn *conn);

/** @brief Get bt stack sync info
 *
 *  @param conn Connection object.
 * @param info sync info.
 *
 *  @return void *: sync info pointer.
 */
void *hostif_bt_stack_get_sync_info(struct bt_conn *conn, void *info);

/** @brief Set bt stack sync info
 *
 *  @param info Set sync info.
 * @param len sync info lenght.
 *
 *  @return conn: success; NULL: failed.
 */
struct bt_conn *hostif_bt_stack_set_sync_info(void *info, uint16_t len);

/** @brief check bt le stack is ready for sync info
 *
 *  @param conn Connection object.
 *
 *  @return true: ready, false: not ready.
 */
bool hostif_bt_le_stack_ready_sync_info(struct bt_conn *conn);

/** @brief Get bt le stack sync info
 *
 *  @param conn Connection object.
 * @param info sync info.
 *
 *  @return void *: sync info pointer.
 */
void *hostif_bt_le_stack_get_sync_info(struct bt_conn *conn, void *info);

/** @brief Set bt le stack sync info
 *
 *  @param info Set sync info.
 * @param len sync info lenght.
 *
 *  @return conn: success; NULL: failed.
 */
struct bt_conn *hostif_bt_le_stack_set_sync_info(void *info, uint16_t len);

/** @brief Get bt linkkey sync info
 *
 * @param len Output lenght of sync info.
 *
 *  @return void *: sync info pointer.
 */
void *hostif_bt_store_sync_get_linkkey_info(uint16_t *len);

/** @brief Set bt linkkey sync info
 *
 *  @param info Set sync info.
 * @param len sync info lenght.
 *
 *  @return None
 */
void hostif_bt_store_sync_set_linkkey_info(void *info, uint16_t len, uint8_t local_is_master);

/** @brief dump bt stack info
 *
 * @param None.
 *
 *  @return None.
 */
void hostif_bt_stack_dump_info(void);

void hostif_bt_read_ble_mac(bt_addr_le_t *addr);



/*
 broadcast interface
*/

/** @brief Terminates a BIG as a broadcaster or receiver
 *
 *  @param big    Pointer to the BIG structure.
 *
 *  @return 0 in case of success or negative value in case of error.
 */
int hostif_bt_iso_big_terminate(struct bt_iso_big *big);

int hostif_bt_le_per_adv_sync_delete(struct bt_le_per_adv_sync *per_adv_sync);

int hostif_bt_le_ext_adv_create(const struct bt_le_adv_param *param,
			 const struct bt_le_ext_adv_cb *cb,
			 struct bt_le_ext_adv **out_adv);

int hostif_bt_le_per_adv_set_param(struct bt_le_ext_adv *adv,
			    const struct bt_le_per_adv_param *param);

int hostif_bt_le_per_adv_start(struct bt_le_ext_adv *adv);

int hostif_bt_le_ext_adv_start(struct bt_le_ext_adv *adv,
			struct bt_le_ext_adv_start_param *param);

int hostif_bt_le_per_adv_set_subevent_data(const struct bt_le_ext_adv *adv, uint8_t num_subevents,
				    const struct bt_le_per_adv_subevent_data_params *params);

int hostif_bt_le_per_adv_sync_subevent(struct bt_le_per_adv_sync *per_adv_sync,
				struct bt_le_per_adv_sync_subevent_params *params);

int hostif_bt_le_per_adv_set_response_data(struct bt_le_per_adv_sync *per_adv_sync,
				    const struct bt_le_per_adv_response_params *param,
				    const struct net_buf_simple *data);

int hostif_bt_le_per_adv_set_info_transfer(const struct bt_le_ext_adv *adv,
				    const struct bt_conn *conn,
				    uint16_t service_data);

int hostif_bt_le_per_adv_sync_transfer(const struct bt_le_per_adv_sync *per_adv_sync,
				const struct bt_conn *conn,
				uint16_t service_data);
int hostif_bt_le_per_adv_sync_transfer_subscribe(
	const struct bt_conn *conn,
	const struct bt_le_per_adv_sync_transfer_param *param);

int hostif_bt_le_per_adv_sync_transfer_unsubscribe(
	const struct bt_conn *conn);

void hostif_bt_le_scan_cb_register(struct bt_le_scan_cb *cb);

void hostif_bt_le_per_adv_sync_cb_register(struct bt_le_per_adv_sync_cb *cb);

//int hostif_bt_le_scan_start(const struct bt_le_scan_param *param, bt_le_scan_cb_t cb);

int hostif_bt_le_per_adv_sync_create(const struct bt_le_per_adv_sync_param *param,
			      struct bt_le_per_adv_sync **out_sync);

int hostif_bt_le_ext_adv_set_data(struct bt_le_ext_adv *adv,
			   const struct bt_data *ad, size_t ad_len,
			   const struct bt_data *sd, size_t sd_len);

int hostif_bt_le_per_adv_set_data(const struct bt_le_ext_adv *adv,
			   const struct bt_data *ad, size_t ad_len);

void hostif_bt_le_scan_cb_unregister(struct bt_le_scan_cb *cb);
void hostif_bt_le_per_adv_sync_cb_unregister(struct bt_le_per_adv_sync_cb *cb);
int hostif_bt_le_per_adv_stop(struct bt_le_ext_adv *adv);
int hostif_bt_le_ext_adv_stop(struct bt_le_ext_adv *adv);
int hostif_bt_le_ext_adv_delete(struct bt_le_ext_adv *adv);

int hostif_bt_iso_big_sync(struct bt_le_per_adv_sync *sync, struct bt_iso_big_sync_param *param,
		    struct bt_iso_big **out_big);
int hostif_bt_iso_big_create(struct bt_le_ext_adv *padv, struct bt_iso_big_create_param *param,
		      struct bt_iso_big **out_big);
int hostif_bt_iso_big_create_test(struct bt_le_ext_adv *padv,
				struct bt_iso_big_create_test_param *param,
				struct bt_iso_big **out_big);

int hostif_bt_set_pts_enable(bool enable);

uint8_t hostif_bt_le_get_paired_devices(bt_addr_le_t dev_buf[], uint8_t buf_count);
uint8_t hostif_bt_le_get_paired_devices_by_type(uint8_t addr_type, bt_addr_le_t dev_buf[], uint8_t buf_count);
bool hostif_bt_le_get_last_paired_device(bt_addr_le_t* dev);
bool hostif_bt_le_clear_device(bt_addr_le_t* dev);
void hostif_bt_le_unpair_device(const bt_addr_le_t *addr);
void hostif_bt_le_clear_list(void);
bool hostif_bt_le_is_bond(const bt_addr_le_t *addr);
int hostif_bt_iso_rssi_read(complete_event_cb cb);
int hostif_bt_iso_link_info_get(complete_event_cb cb);
int hostif_bt_read_link_info(struct bt_conn *conn, complete_event_cb cb);
int hostif_bt_link_info_set(struct bt_conn *conn, uint8_t *map);
void hostif_bt_le_force_rpa_timeout(void);
bool hostif_bt_le_is_cis_capable(struct bt_conn *conn);
bool hostif_bt_le_is_bis_capable(struct bt_conn *conn);

int hostif_bt_fpms_register_cb(struct bt_fpms_app_cb *cb);
int hostif_bt_fpms_send_data(struct bt_conn *conn,uint8_t *data, uint32_t len);

void hostif_bt_pa_ack_enable(uint8_t enable);

int hostif_bt_sc_enable(uint8_t enable);

int hostif_bt_ctkd_enable(uint8_t enable);

#if defined(CONFIG_BT_PRIVACY)
int hostif_bt_le_address_resolution_enable(uint8_t enable);
#endif
#endif /* __HOST_INTERFACE_H */
