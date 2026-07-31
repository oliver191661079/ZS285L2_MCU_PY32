#include <acts_bluetooth/gatt.h>
#include <acts_bluetooth/audio/bass.h>
#include "audio_service.h"

#ifndef CONFIG_BT_BASS_SERVICE

const struct bt_gatt_service_static bass_svc = {
	.attrs = NULL,
	.attr_count = 0,
};

#else /* CONFIG_BT_BASS_SERVICE */

static void bass_ccc_cfg_changed(struct bt_conn *conn, uint8_t conn_type,
						 const struct bt_gatt_attr *attr, uint16_t value)
{
}

BT_GATT_SERVICE_DEFINE(bass_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_BASS),

	BT_GATT_CHARACTERISTIC(BT_UUID_BASS_CONTROL_POINT,
				BT_GATT_CHRC_WRITE_WITHOUT_RESP |
				BT_GATT_CHRC_WRITE,
				BT_AUDIO_GATT_PERM_WRITE,
				NULL, bt_bass_write_scan_control, NULL),

	/* The number of Chrc >= the number of BIGs */
	BT_GATT_CHARACTERISTIC(BT_UUID_BASS_RECV_STATE,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_AUDIO_GATT_PERM_READ,
			       bt_bass_read_receive_state, NULL, NULL),
	BT_GATT_CCC(bass_ccc_cfg_changed,
		    BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),
);
#endif /* CONFIG_BT_BASS_SERVICE */
