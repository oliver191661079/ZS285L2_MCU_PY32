#include <acts_bluetooth/gatt.h>
#include <acts_bluetooth/audio/cas.h>
#include "audio_service.h"

#ifndef CONFIG_BT_LEA_PTS_TEST
const struct bt_gatt_service_static cas_csis_svc = {
	.attrs = NULL,
	.attr_count = 0,
};
#endif

#ifndef CONFIG_BT_CAS_SERVICE

const struct bt_gatt_service_static cas_svc = {
	.attrs = NULL,
	.attr_count = 0,
};

const struct bt_gatt_service_static cas_csis_svc = {
	.attrs = NULL,
	.attr_count = 0,
};

#else /* CONFIG_BT_CAS_SERVICE */

#ifdef CONFIG_BT_LEA_PTS_TEST
static void cas_sirk_ccc_cfg_changed(struct bt_conn *conn, uint8_t conn_type,
					 const struct bt_gatt_attr *attr, uint16_t value)
{
}

static void cas_size_ccc_cfg_changed(struct bt_conn *conn, uint8_t conn_type,
					 const struct bt_gatt_attr *attr, uint16_t value)
{
}

static void cas_lock_ccc_cfg_changed(struct bt_conn *conn, uint8_t conn_type,
					 const struct bt_gatt_attr *attr, uint16_t value)
{
}

BT_GATT_SERVICE_DEFINE(cas_csis_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_CSIS),

	/*
	 * optional: notify if it can be modified via OOB
	 */
	BT_GATT_CHARACTERISTIC(BT_UUID_CSIS_SIRK,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_AUDIO_GATT_PERM_READ,
			       bt_csis_read_sirk, NULL, NULL),
	BT_GATT_CCC(cas_sirk_ccc_cfg_changed,
			BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),

	/*
	 * Size (optional): the number of devices comprising the Coordinated Set
	 *
	 * optional: notify if it can be modified via OOB
	 */
	BT_GATT_CHARACTERISTIC(BT_UUID_CSIS_SIZE,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY ,
			       BT_AUDIO_GATT_PERM_READ,
			       bt_csis_read_size, NULL, NULL),
	BT_GATT_CCC(cas_size_ccc_cfg_changed,
			BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),

	/*
	 * Lock (optional)
	 */
	BT_GATT_CHARACTERISTIC(BT_UUID_CSIS_LOCK,
			       BT_GATT_CHRC_WRITE |
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_AUDIO_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE,
			       bt_csis_read_lock, bt_csis_write_lock, NULL),
	BT_GATT_CCC(cas_lock_ccc_cfg_changed,
		    BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),

	/*
	 * Rank: depends on Lock
	 */
	BT_GATT_CHARACTERISTIC(BT_UUID_CSIS_RANK,
			       BT_GATT_CHRC_READ,
			       BT_AUDIO_GATT_PERM_READ,
			       bt_csis_read_rank, NULL, NULL),
);
#endif

BT_GATT_SERVICE_DEFINE(cas_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_CAS),

#ifdef CONFIG_BT_LEA_PTS_TEST
	BT_GATT_INCLUDE_SERVICE((void *)attr_cas_csis_svc),
#endif
);
#endif /* CONFIG_BT_CAS_SERVICE */
