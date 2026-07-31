#include <misc/printk.h>
#include <acts_bluetooth/gatt.h>
#include <acts_bluetooth/uuid.h>
#include <acts_bluetooth/audio/mcs.h>
#include "audio_service.h"

#ifndef CONFIG_BT_MCS_SERVICE

const struct bt_gatt_service_static mcs_svc = {
	.attrs = NULL,
	.attr_count = 0,
};

#else /* CONFIG_BT_MCS_SERVICE */

#if ((defined(CONFIG_BT_LEA_PTS_TEST)) || (defined(CONFIG_BT_LE_AUDIO_MASTER)))
static void mcs_media_player_name_cfg_changed(struct bt_conn *conn, uint8_t conn_type,
										const struct bt_gatt_attr *attr, uint16_t value)
{
	printk("value 0x%04x\n", value);
}

static void mcs_track_changed_cfg_changed(struct bt_conn *conn, uint8_t conn_type,
									const struct bt_gatt_attr *attr, uint16_t value)
{
	printk("value 0x%04x\n", value);
}

static void mcs_media_state_cfg_changed(struct bt_conn *conn, uint8_t conn_type,
									const struct bt_gatt_attr *attr, uint16_t value)
{
	printk("value 0x%04x\n", value);
}

static void mcs_track_title_cfg_changed(struct bt_conn *conn, uint8_t conn_type,
									const struct bt_gatt_attr *attr, uint16_t value)
{
	printk("value 0x%04x\n", value);
}

static void mcs_track_duration_cfg_changed(struct bt_conn *conn, uint8_t conn_type,
										const struct bt_gatt_attr *attr, uint16_t value)
{
	printk("value 0x%04x\n", value);
}

static void mcs_track_position_cfg_changed(struct bt_conn *conn, uint8_t conn_type,
										const struct bt_gatt_attr *attr, uint16_t value)
{
	printk("value 0x%04x\n", value);
}

static void mcs_playback_speed_cfg_changed(struct bt_conn *conn, uint8_t conn_type,
										const struct bt_gatt_attr *attr, uint16_t value)
{
	printk("value 0x%04x\n", value);
}

static void mcs_media_control_point_cfg_changed(struct bt_conn *conn, uint8_t conn_type,
											const struct bt_gatt_attr *attr, uint16_t value)
{
	printk("value 0x%04x\n", value);
}

static void mcs_opcodes_supported_cfg_changed(struct bt_conn *conn, uint8_t conn_type,
										const struct bt_gatt_attr *attr, uint16_t value)
{
	printk("value 0x%04x\n", value);
}

BT_GATT_SERVICE_DEFINE(mcs_svc,
	/* BT_GATT_PRIMARY_SERVICE(BT_UUID_MCS), */
	BT_GATT_PRIMARY_SERVICE(BT_UUID_GMCS),
	BT_GATT_CHARACTERISTIC(BT_UUID_MCS_MEDIA_PLAYER_NAME,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_AUDIO_GATT_PERM_READ,
			       bt_mcs_read_media_player_name, NULL, NULL),
	BT_GATT_CCC(mcs_media_player_name_cfg_changed,
		    BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(BT_UUID_MCS_TRACK_CHANGED,
			       BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_NONE,
			       NULL, NULL, NULL),
    BT_GATT_CCC(mcs_track_changed_cfg_changed,
            BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(BT_UUID_MCS_TRACK_TITLE,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_AUDIO_GATT_PERM_READ,
			       bt_mcs_read_track_title, NULL, NULL),
	BT_GATT_CCC(mcs_track_title_cfg_changed,
            BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(BT_UUID_MCS_TRACK_DURATION,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_AUDIO_GATT_PERM_READ,
			       bt_mcs_read_track_duration, NULL, NULL),
	BT_GATT_CCC(mcs_track_duration_cfg_changed,
            BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(BT_UUID_MCS_TRACK_POSITION,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE |
				   BT_GATT_CHRC_WRITE_WITHOUT_RESP | BT_GATT_CHRC_NOTIFY,
			       BT_AUDIO_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE,
			       bt_mcs_read_track_position, bt_mcs_write_track_position, NULL),
	BT_GATT_CCC(mcs_track_position_cfg_changed,
            BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(BT_UUID_MCS_PLAYBACK_SPEED,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE |
				   BT_GATT_CHRC_WRITE_WITHOUT_RESP | BT_GATT_CHRC_NOTIFY,
			       BT_AUDIO_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE,
			       bt_mcs_read_playback_speed, bt_mcs_write_playback_speed, NULL),
	BT_GATT_CCC(mcs_playback_speed_cfg_changed,
            BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(BT_UUID_MCS_MEDIA_STATE,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
			       BT_AUDIO_GATT_PERM_READ,
			       bt_mcs_read_media_state, NULL, NULL),
	BT_GATT_CCC(mcs_media_state_cfg_changed,
		    BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(BT_UUID_MCS_MEDIA_CONTROL_POINT,
			       BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP|
				   BT_GATT_CHRC_NOTIFY,
			       BT_AUDIO_GATT_PERM_WRITE,
			       NULL, bt_mcs_write_media_control, NULL),
	BT_GATT_CCC(mcs_media_control_point_cfg_changed,
		    BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),
	BT_GATT_CHARACTERISTIC(BT_UUID_MCS_MEDIA_CONTROL_POINT_OPCODES_SUPPORTED,
                   BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                   BT_AUDIO_GATT_PERM_READ,
                   bt_mcs_read_opcodes_supported, NULL, NULL),
	BT_GATT_CCC(mcs_opcodes_supported_cfg_changed,
		    BT_GATT_PERM_READ | BT_AUDIO_GATT_PERM_WRITE),
    BT_GATT_CHARACTERISTIC(BT_UUID_CONTENT_CONTROL_ID,
                   BT_GATT_CHRC_READ,
                   BT_AUDIO_GATT_PERM_READ,
                   bt_mcs_read_ccid, NULL, NULL),
);
#else /* CONFIG_BT_LE_AUDIO_MASTER || CONFIG_BT_LEA_PTS_TEST */
const struct bt_gatt_service_static mcs_svc = {
	.attrs = NULL,
	.attr_count = 0,
};
#endif /* CONFIG_BT_LE_AUDIO_MASTER || CONFIG_BT_LEA_PTS_TEST */

#endif /* CONFIG_BT_MCS_SERVICE */
