#include <acts_bluetooth/gatt.h>
#include <acts_bluetooth/audio/gmas.h>
#include "audio_service.h"

#ifndef CONFIG_BT_GMAS_SERVICE

const struct bt_gatt_service_static gmas_svc = {
	.attrs = NULL,
	.attr_count = 0,
};

#else /* CONFIG_BT_GMAS_SERVICE */

BT_GATT_SERVICE_DEFINE(gmas_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_GMAS),
	BT_GATT_CHARACTERISTIC(BT_UUID_GMAP_ROLE, BT_GATT_CHRC_READ, BT_AUDIO_GATT_PERM_READ,
			       bt_gmas_read_role, NULL, NULL),
#if defined(CONFIG_BT_GMAS_SERVICE_UGG)
	BT_GATT_CHARACTERISTIC(BT_UUID_GMAP_UGG_FEAT, BT_GATT_CHRC_READ, BT_AUDIO_GATT_PERM_READ,
				  bt_gmas_read_ugg_feat, NULL, NULL),
#endif /* CONFIG_BT_GMAS_SERVICE_UGG */
#if defined(CONFIG_BT_GMAS_SERVICE_UGT)
	BT_GATT_CHARACTERISTIC(BT_UUID_GMAP_UGT_FEAT, BT_GATT_CHRC_READ, BT_AUDIO_GATT_PERM_READ,
				  bt_gmas_read_ugt_feat, NULL, NULL),
#endif /* CONFIG_BT_GMAS_SERVICE_UGT */
#if defined(CONFIG_BT_GMAS_SERVICE_BGS)
	BT_GATT_CHARACTERISTIC(BT_UUID_GMAP_BGS_FEAT, BT_GATT_CHRC_READ, BT_AUDIO_GATT_PERM_READ,
				  bt_gmas_read_bgs_feat, NULL, NULL),
#endif /* CONFIG_BT_GMAS_SERVICE_BGS */
#if defined(CONFIG_BT_GMAS_SERVICE_BGR)
	BT_GATT_CHARACTERISTIC(BT_UUID_GMAP_BGR_FEAT, BT_GATT_CHRC_READ, BT_AUDIO_GATT_PERM_READ,
				  bt_gmas_read_bgr_feat, NULL, NULL),
#endif /* CONFIG_BT_GMAS_SERVICE_BGR */
);

#endif /* CONFIG_BT_GMAS_SERVICE */

