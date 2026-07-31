#ifndef __BT_AICS_SERVICE_H
#define __BT_AICS_SERVICE_H

#include <misc/printk.h>
#include <acts_bluetooth/gatt.h>
#include <acts_bluetooth/uuid.h>
#include <acts_bluetooth/audio/aics.h>
#include "audio_service.h"

static void aics_state_cfg_changed(struct bt_conn *conn, uint8_t conn_type,
					 const struct bt_gatt_attr *attr, uint16_t value)
{
	printk("aics state 0x%04x\n", value);
}

static void aics_input_status_cfg_changed(struct bt_conn *conn, uint8_t conn_type,
					 const struct bt_gatt_attr *attr, uint16_t value)
{
	printk("value 0x%04x\n", value);
}

static void aics_description_cfg_changed(struct bt_conn *conn, uint8_t conn_type,
					 const struct bt_gatt_attr *attr, uint16_t value)
{
	printk("value 0x%04x\n", value);
}

#define BT_AICS_SERVICE_DEFINE(_aics)	\
	BT_GATT_SERVICE_DEFINE(_aics,	\
	BT_GATT_SECONDARY_SERVICE(BT_UUID_AICS),	\
	BT_GATT_CHARACTERISTIC(BT_UUID_AICS_STATE,	\
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,	\
			       BT_AUDIO_GATT_PERM_READ,	\
			       bt_aics_read_state, NULL, NULL),	\
	BT_GATT_CCC(aics_state_cfg_changed,	\
		    BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),	\
	BT_GATT_CHARACTERISTIC(BT_UUID_AICS_GAIN_SETTINGS,	\
			       BT_GATT_CHRC_READ,	\
			       BT_AUDIO_GATT_PERM_READ,	\
			       bt_aics_read_gain_settings, NULL, NULL),	\
	BT_GATT_CHARACTERISTIC(BT_UUID_AICS_INPUT_TYPE,	\
			       BT_GATT_CHRC_READ,	\
			       BT_AUDIO_GATT_PERM_READ,	\
			       bt_aics_read_type, NULL, NULL),	\
	BT_GATT_CHARACTERISTIC(BT_UUID_AICS_INPUT_STATUS,	\
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,	\
			       BT_AUDIO_GATT_PERM_READ,	\
			       bt_aics_read_input_status, NULL, NULL),	\
	BT_GATT_CCC(aics_input_status_cfg_changed,	\
		    BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),	\
	BT_GATT_CHARACTERISTIC(BT_UUID_AICS_CONTROL,	\
			       BT_GATT_CHRC_WRITE,	\
			       BT_AUDIO_GATT_PERM_WRITE,	\
			       NULL, bt_aics_write_control, NULL),	\
	BT_GATT_CHARACTERISTIC(BT_UUID_AICS_DESCRIPTION,	\
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE_WITHOUT_RESP | BT_GATT_CHRC_NOTIFY,	\
			       BT_AUDIO_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE,	\
			       bt_aics_read_description, bt_aics_write_description, NULL),	\
	BT_GATT_CCC(aics_description_cfg_changed,	\
		    BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),	\
)

#endif /* __BT_AICS_SERVICE_H */