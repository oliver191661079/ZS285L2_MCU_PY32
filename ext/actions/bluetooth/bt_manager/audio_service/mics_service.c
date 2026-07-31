#include <misc/printk.h>
#include <acts_bluetooth/gatt.h>
#include <acts_bluetooth/uuid.h>
#include <acts_bluetooth/audio/mics.h>
#include "audio_service.h"

#ifdef CONFIG_BT_LEA_PTS_TEST
#include "aics_service.h"
#endif

#ifndef CONFIG_BT_LEA_PTS_TEST
/* FIXME: not support AICS yet */
const struct bt_gatt_service_static mics_aics_svc0 = {
	.attrs = NULL,
	.attr_count = 0,
};
#endif /*CONFIG_BT_PTS_TEST*/

const struct bt_gatt_service_static mics_aics_svc1 = {
	.attrs = NULL,
	.attr_count = 0,
};

#if defined(CONFIG_BT_LEA_PTS_TEST)&&(!defined(CONFIG_BT_MICS_SERVICE))
#define CONFIG_BT_MICS_SERVICE
#endif

#ifndef CONFIG_BT_MICS_SERVICE

const struct bt_gatt_service_static mics_svc = {
	.attrs = NULL,
	.attr_count = 0,
};

#else /* CONFIG_BT_MICS_SERVICE */

static void mics_mute_cfg_changed(struct bt_conn *conn, uint8_t conn_type,
					const struct bt_gatt_attr *attr, uint16_t value)
{
	printk("mics state 0x%04x\n", value);
}

#ifdef CONFIG_BT_LEA_PTS_TEST
BT_AICS_SERVICE_DEFINE(mics_aics_svc0);
#endif

BT_GATT_SERVICE_DEFINE(mics_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_MICS),
	BT_GATT_CHARACTERISTIC(BT_UUID_MICS_MUTE,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_NOTIFY,
                   BT_AUDIO_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE,
			       bt_mics_read_mute, bt_mics_write_mute, NULL),
    BT_GATT_CCC(mics_mute_cfg_changed,
                   BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),

#ifdef CONFIG_BT_LEA_PTS_TEST
	BT_GATT_INCLUDE_SERVICE((void *)attr_mics_aics_svc0),
#endif
);
#endif /* CONFIG_BT_MICS_SERVICE */
