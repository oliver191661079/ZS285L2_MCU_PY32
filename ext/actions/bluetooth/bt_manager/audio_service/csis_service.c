#include <sys/printk.h>
#include <acts_bluetooth/gatt.h>
#include <acts_bluetooth/audio/csis.h>
#include "audio_service.h"

#ifndef CONFIG_BT_CSIS_SERVICE



#else /* CONFIG_BT_CSIS_SERVICE */

static void sirk_cfg_changed(const struct bt_gatt_attr *attr,
				     uint16_t value)
{
	printk("value 0x%04x\n", value);
}

static void size_cfg_changed(const struct bt_gatt_attr *attr,
				     uint16_t value)
{
	printk("value 0x%04x\n", value);
}

static void lock_cfg_changed(const struct bt_gatt_attr *attr,
				     uint16_t value)
{
	printk("value 0x%04x\n", value);
}

BT_GATT_SERVICE_DEFINE(csis_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_CSIS),
	BT_GATT_CHARACTERISTIC(BT_UUID_CSIS_SIRK,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_AUDIO_GATT_PERM_READ,
			       bt_csis_read_sirk, NULL, NULL),
    BT_GATT_CCC(sirk_cfg_changed,
		    BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(BT_UUID_CSIS_SIZE,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_AUDIO_GATT_PERM_READ,
			       bt_csis_read_size, NULL, NULL),
    BT_GATT_CCC(size_cfg_changed,
		    BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(BT_UUID_CSIS_LOCK,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_NOTIFY,
			       BT_AUDIO_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE,
			       bt_csis_read_lock, bt_csis_write_lock, NULL),
    BT_GATT_CCC(lock_cfg_changed,
		    BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(BT_UUID_CSIS_RANK,
			       BT_GATT_CHRC_READ,
			       BT_AUDIO_GATT_PERM_READ,
			       bt_csis_read_rank, NULL, NULL),
);
#endif /* CONFIG_BT_CSIS_SERVICE */