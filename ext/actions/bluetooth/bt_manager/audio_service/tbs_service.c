#include <acts_bluetooth/gatt.h>
#include <acts_bluetooth/audio/tbs.h>
#include "audio_service.h"

#ifndef CONFIG_BT_TBS_SERVICE

const struct bt_gatt_service_static gtbs_svc = {
	.attrs = NULL,
	.attr_count = 0,
};

#else /* CONFIG_BT_TBS_SERVICE */

static void provider_ccc_changed(struct bt_conn *conn, uint8_t conn_type,
				const struct bt_gatt_attr *attr, uint16_t value)
{
}

static void technology_ccc_changed(struct bt_conn *conn, uint8_t conn_type,
				const struct bt_gatt_attr *attr, uint16_t value)
{
}

static void schemes_ccc_changed(struct bt_conn *conn, uint8_t conn_type,
				const struct bt_gatt_attr *attr, uint16_t value)
{
}

static void strength_ccc_changed(struct bt_conn *conn, uint8_t conn_type,
				const struct bt_gatt_attr *attr, uint16_t value)
{
}

static void calls_ccc_changed(struct bt_conn *conn, uint8_t conn_type,
				const struct bt_gatt_attr *attr, uint16_t value)
{
}

static void status_ccc_changed(struct bt_conn *conn, uint8_t conn_type,
				const struct bt_gatt_attr *attr, uint16_t value)
{
}

static void target_ccc_changed(struct bt_conn *conn, uint8_t conn_type,
				const struct bt_gatt_attr *attr, uint16_t value)
{
}

static void state_ccc_changed(struct bt_conn *conn, uint8_t conn_type,
				const struct bt_gatt_attr *attr, uint16_t value)
{
}

static void control_ccc_changed(struct bt_conn *conn, uint8_t conn_type,
				const struct bt_gatt_attr *attr, uint16_t value)
{
}

static void reason_ccc_changed(struct bt_conn *conn, uint8_t conn_type,
				const struct bt_gatt_attr *attr, uint16_t value)
{
}

static void call_ccc_changed(struct bt_conn *conn, uint8_t conn_type,
				const struct bt_gatt_attr *attr, uint16_t value)
{
}

static void friendly_ccc_changed(struct bt_conn *conn, uint8_t conn_type,
				const struct bt_gatt_attr *attr, uint16_t value)
{
}

BT_GATT_SERVICE_DEFINE(gtbs_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_GTBS),
	/* BT_GATT_PRIMARY_SERVICE(BT_UUID_TBS), */
	BT_GATT_CHARACTERISTIC(BT_UUID_TBS_PROVIDER,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
				   BT_AUDIO_GATT_PERM_READ,
				   bt_tbs_read_provider, NULL, NULL),
	BT_GATT_CCC(provider_ccc_changed,
		    BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(BT_UUID_TBS_UCI,
			       BT_GATT_CHRC_READ,
				   BT_AUDIO_GATT_PERM_READ,
				   bt_tbs_read_uci, NULL, NULL),
	BT_GATT_CHARACTERISTIC(BT_UUID_TBS_TECHNOLOGY,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
				   BT_AUDIO_GATT_PERM_READ,
				   bt_tbs_read_technology, NULL, NULL),
	BT_GATT_CCC(technology_ccc_changed,
		    BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(BT_UUID_TBS_SCHEMES,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
				   BT_AUDIO_GATT_PERM_READ,
				   bt_tbs_read_schemes, NULL, NULL),
	BT_GATT_CCC(schemes_ccc_changed,
		    BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(BT_UUID_TBS_STRENGTH,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
				   BT_AUDIO_GATT_PERM_READ,
				   bt_tbs_read_strength, NULL, NULL),
	BT_GATT_CCC(strength_ccc_changed,
		    BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(BT_UUID_TBS_INTERVAL,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE |
			       BT_GATT_CHRC_WRITE_WITHOUT_RESP,
				   BT_AUDIO_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE,
				   bt_tbs_read_interval, bt_tbs_write_interval, NULL),
	BT_GATT_CHARACTERISTIC(BT_UUID_TBS_CALLS,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
				   BT_AUDIO_GATT_PERM_READ,
				   bt_tbs_read_calls, NULL, NULL),
	BT_GATT_CCC(calls_ccc_changed,
		    BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(BT_UUID_TBS_CCID,
			       BT_GATT_CHRC_READ,
				   BT_AUDIO_GATT_PERM_READ,
				   bt_tbs_read_ccid, NULL, NULL),
	BT_GATT_CHARACTERISTIC(BT_UUID_TBS_STATUS,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
				   BT_AUDIO_GATT_PERM_READ,
				   bt_tbs_read_status, NULL, NULL),
	BT_GATT_CCC(status_ccc_changed,
		    BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(BT_UUID_TBS_TARGET,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
				   BT_AUDIO_GATT_PERM_READ,
				   bt_tbs_read_target, NULL, NULL),
	BT_GATT_CCC(target_ccc_changed,
		    BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(BT_UUID_TBS_STATE,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
				   BT_GATT_PERM_READ,
				   bt_tbs_read_state, NULL, NULL),
	BT_GATT_CCC(state_ccc_changed,
		    BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(BT_UUID_TBS_CONTROL,
			       BT_GATT_CHRC_WRITE | BT_GATT_CHRC_NOTIFY |
			       BT_GATT_CHRC_WRITE_WITHOUT_RESP,
				   BT_AUDIO_GATT_PERM_WRITE,
				   NULL, bt_tbs_write_control, NULL),
	BT_GATT_CCC(control_ccc_changed,
		    BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(BT_UUID_TBS_OPCODES,
			       BT_GATT_CHRC_READ,
				   BT_AUDIO_GATT_PERM_READ,
				   bt_tbs_read_opcodes, NULL, NULL),
	BT_GATT_CHARACTERISTIC(BT_UUID_TBS_REASON,
			       BT_GATT_CHRC_NOTIFY,
				   BT_GATT_PERM_NONE,
				   NULL, NULL, NULL),
	BT_GATT_CCC(reason_ccc_changed,
		    BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(BT_UUID_TBS_INCOMING,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
				   BT_AUDIO_GATT_PERM_READ,
				   bt_tbs_read_incoming, NULL, NULL),
	BT_GATT_CCC(call_ccc_changed,
		    BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(BT_UUID_TBS_FRIENDLY,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
				   BT_AUDIO_GATT_PERM_READ,
				   bt_tbs_read_friendly, NULL, NULL),
	BT_GATT_CCC(friendly_ccc_changed,
		    BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),
);
#endif /* CONFIG_BT_TBS_SERVICE */
