#include <acts_bluetooth/gatt.h>

#ifndef CONFIG_BT_AUDIO_VND_SERVICE
const struct bt_gatt_service_static le_audio_vnd_svc = {
	.attrs = NULL,
	.attr_count = 0,
};

#else /* CONFIG_BT_AUDIO_VND_SERVICE */

extern ssize_t btsrv_audio_vnd_write(struct bt_conn *conn,
				const struct bt_gatt_attr *attr, const void *buf,
				uint16_t len, uint16_t offset, uint8_t flags);

/* WARNNING: do NOT change the value of uuid */
static const struct bt_uuid_128 vnd_svc_uuid = BT_UUID_INIT_128(
	BT_UUID_128_ENCODE(0xe49a1800, 0xf69a, 0x11e8, 0x8eb2, 0xf2801f1b9fd1));

/* WARNNING: do NOT change the value of uuid */
static const struct bt_uuid_128 vnd_chrc_uuid = BT_UUID_INIT_128(
	BT_UUID_128_ENCODE(0xe49a2a00, 0xf69a, 0x11e8, 0x8eb2, 0xf2801f1b9fd1));

static void vnd_ccc_cfg_changed(struct bt_conn *conn, uint8_t conn_type,
					 const struct bt_gatt_attr *attr, uint16_t value)
{
}

BT_GATT_SERVICE_DEFINE(le_audio_vnd_svc,
	BT_GATT_PRIMARY_SERVICE((void *)&vnd_svc_uuid),

	BT_GATT_CHARACTERISTIC(&vnd_chrc_uuid.uuid,
			       BT_GATT_CHRC_WRITE_WITHOUT_RESP |
			       BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_WRITE,
			       NULL, btsrv_audio_vnd_write, NULL),
	BT_GATT_CCC(vnd_ccc_cfg_changed,
		    BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);
#endif /* CONFIG_BT_AUDIO_VND_SERVICE */