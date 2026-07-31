#include <acts_bluetooth/gatt.h>
#include <acts_bluetooth/audio/tmas.h>
#include "audio_service.h"

BT_GATT_SERVICE_DEFINE(tmas_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_TMAS),

	BT_GATT_CHARACTERISTIC(BT_UUID_TMAS_ROLE,
			       BT_GATT_CHRC_READ,
			       BT_AUDIO_GATT_PERM_READ,
			       bt_tmas_read_role, NULL, NULL),
);
