#include <acts_bluetooth/gatt.h>
#include <acts_bluetooth/audio/pacs.h>
#include "audio_service.h"

#ifndef CONFIG_BT_PACS_SERVICE

const struct bt_gatt_service_static pacs_svc = {
	.attrs = NULL,
	.attr_count = 0,
};

#else /* CONFIG_BT_PACS_SERVICE */

static void aac_ccc_cfg_changed(struct bt_conn *conn, uint8_t conn_type,
					 const struct bt_gatt_attr *attr, uint16_t value)
{
}

BT_GATT_SERVICE_DEFINE(pacs_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_PACS),

	BT_GATT_CHARACTERISTIC(BT_UUID_PACS_SINK_PAC,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_AUDIO_GATT_PERM_READ,
			       bt_pacs_read_sink_pac, NULL, NULL),
	BT_GATT_CCC(aac_ccc_cfg_changed,
			BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),


	BT_GATT_CHARACTERISTIC(BT_UUID_PACS_SINK_LOCATIONS,
				   BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
				   BT_AUDIO_GATT_PERM_READ,
				   bt_pacs_read_sink_locations, NULL, NULL),
	BT_GATT_CCC(aac_ccc_cfg_changed,
			BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),

	BT_GATT_CHARACTERISTIC(BT_UUID_PACS_SOURCE_PAC,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_AUDIO_GATT_PERM_READ,
			       bt_pacs_read_source_pac, NULL, NULL),
	BT_GATT_CCC(aac_ccc_cfg_changed,
			BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),


	BT_GATT_CHARACTERISTIC(BT_UUID_PACS_SOURCE_LOCATIONS,
				   BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
				   BT_AUDIO_GATT_PERM_READ,
				   bt_pacs_read_source_locations, NULL, NULL),
	BT_GATT_CCC(aac_ccc_cfg_changed,
			BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),


	BT_GATT_CHARACTERISTIC(BT_UUID_PACS_AVAILABLE_CONTEXTS,
				   BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
				   BT_AUDIO_GATT_PERM_READ,
				   bt_pacs_read_available_contexts, NULL, NULL),
	BT_GATT_CCC(aac_ccc_cfg_changed,
			BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),


	BT_GATT_CHARACTERISTIC(BT_UUID_PACS_SUPPORTED_CONTEXTS,
				   BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
				   BT_AUDIO_GATT_PERM_READ,
				   bt_pacs_read_supported_contexts, NULL, NULL),
	BT_GATT_CCC(aac_ccc_cfg_changed,
			BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),
);
#endif /* CONFIG_BT_PACS_SERVICE */
