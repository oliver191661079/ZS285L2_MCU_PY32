#include <acts_bluetooth/gatt.h>
#include <acts_bluetooth/audio/ascs.h>
#include "audio_service.h"

#ifndef CONFIG_BT_ASCS_SERVICE

const struct bt_gatt_service_static ascs_svc = {
	.attrs = NULL,
	.attr_count = 0,
};

#else /* CONFIG_BT_ASCS_SERVICE */

extern struct bt_ascs_server_cb *bt_ascs_srv_cb;
static void ase_ccc_changed(struct bt_conn *conn, uint8_t conn_type,
					 const struct bt_gatt_attr *attr, uint16_t value)
{
}

static void ase_ctrl_ccc_changed(struct bt_conn *conn, uint8_t conn_type,
					 const struct bt_gatt_attr *attr, uint16_t value)
{
	printk("ase ccc changed %p %d\n",conn,value);
}

ssize_t ase_ctrl_ccc_write(struct bt_conn *conn, uint8_t conn_type,
			     const struct bt_gatt_attr *attr, uint16_t value){
	printk("ase ccc write %p %d\n",conn,value);
	if(bt_ascs_srv_cb){
		if(value && bt_ascs_srv_cb->connect){
			bt_ascs_srv_cb->connect(conn);
		}

		if(!value && bt_ascs_srv_cb->disconnect)
			bt_ascs_srv_cb->disconnect(conn);
	}
	return sizeof(value);
}

/*
 * NOTE: support 1 Source ASE and 1 Sink ASE
 */
BT_GATT_SERVICE_DEFINE(ascs_svc,
	BT_GATT_PRIMARY_SERVICE(BT_UUID_ASCS),

	/*
	 * ASE Control Point
	 */
	BT_GATT_CHARACTERISTIC(BT_UUID_ASCS_ASE_CTRL_POINT,
			       BT_GATT_CHRC_WRITE_WITHOUT_RESP |
			       BT_GATT_CHRC_WRITE | BT_GATT_CHRC_NOTIFY,
			       BT_AUDIO_GATT_PERM_WRITE,
			       NULL, bt_ascs_write_control, NULL),
	/* NOTE: notify after client writes */
	BT_GATT_CCC_CB(ase_ctrl_ccc_changed,NULL,ase_ctrl_ccc_write,
		    BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),

	BT_GATT_CHARACTERISTIC(BT_UUID_ASCS_SINK_ASE,
				   BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
				   BT_AUDIO_GATT_PERM_READ,
				   bt_ascs_read_ase, NULL, NULL),
	/*
	 * NOTE: notify if ase_state changes or context changes
	 * such as "update metadata"
	 */
	BT_GATT_CCC(ase_ccc_changed,
			BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),

	BT_GATT_CHARACTERISTIC(BT_UUID_ASCS_SINK_ASE,
				   BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
				   BT_AUDIO_GATT_PERM_READ,
				   bt_ascs_read_ase, NULL, NULL),
	BT_GATT_CCC(ase_ccc_changed,
			BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),

	BT_GATT_CHARACTERISTIC(BT_UUID_ASCS_SOURCE_ASE,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_AUDIO_GATT_PERM_READ,
			       bt_ascs_read_ase, NULL, NULL),
	BT_GATT_CCC(ase_ccc_changed,
		    BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),
);
#endif /* CONFIG_BT_ASCS_SERVICE */
