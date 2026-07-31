#ifndef __BT_VOCS_SERVICE_H
#define __BT_VOCS_SERVICE_H

#include <misc/printk.h>
#include <acts_bluetooth/gatt.h>
#include <acts_bluetooth/uuid.h>
#include <acts_bluetooth/audio/vocs.h>
#include "audio_service.h"

//#ifdef CONFIG_BT_VCS_SERVICE
#ifdef CONFIG_BT_VOCS
static void offset_state_cfg_changed(struct bt_conn *conn, uint8_t conn_type,
					 const struct bt_gatt_attr *attr, uint16_t value)
{
	printk("value 0x%04x\n", value);
}

static void location_cfg_changed(struct bt_conn *conn, uint8_t conn_type,
					 const struct bt_gatt_attr *attr, uint16_t value)
{
	printk("value 0x%04x\n", value);
}

static void output_desc_cfg_changed(struct bt_conn *conn, uint8_t conn_type,
					 const struct bt_gatt_attr *attr, uint16_t value)
{
	printk("value 0x%04x\n", value);
}

#define BT_VOCS_SERVICE_DEFINE(_vocs)   \
    BT_GATT_SERVICE_DEFINE(_vocs,   \
	BT_GATT_SECONDARY_SERVICE(BT_UUID_VOCS),    \
	BT_GATT_CHARACTERISTIC(BT_UUID_VOCS_STATE,  \
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, \
			       BT_AUDIO_GATT_PERM_READ,   \
			       bt_vocs_read_offset_state, NULL, NULL),  \
	BT_GATT_CCC(offset_state_cfg_changed,   \
		    BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),    \
	BT_GATT_CHARACTERISTIC(BT_UUID_VOCS_LOCATION,   \
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE_WITHOUT_RESP | BT_GATT_CHRC_NOTIFY,   \
			       BT_AUDIO_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE,  \
			       bt_vocs_read_location, bt_vocs_write_location, NULL),    \
	BT_GATT_CCC(location_cfg_changed,   \
		    BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),    \
	BT_GATT_CHARACTERISTIC(BT_UUID_VOCS_CONTROL,    \
			       BT_GATT_CHRC_WRITE,  \
			       BT_AUDIO_GATT_PERM_WRITE,  \
			       NULL, bt_vocs_write_control, NULL),  \
	BT_GATT_CHARACTERISTIC(BT_UUID_VOCS_DESCRIPTION,    \
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE_WITHOUT_RESP | BT_GATT_CHRC_NOTIFY,   \
			       BT_AUDIO_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE,  \
			       bt_vocs_read_output_desc, bt_vocs_write_output_desc, NULL),  \
	BT_GATT_CCC(output_desc_cfg_changed,    \
		    BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),    \
)

#endif /*CONFIG_BT_VCS_SERVICE*/

#endif /* __BT_VOCS_SERVICE_H */