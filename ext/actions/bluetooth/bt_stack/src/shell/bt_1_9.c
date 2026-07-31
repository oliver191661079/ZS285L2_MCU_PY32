/** @file
 * @brief Bluetooth shell module
 *
 * Provide some Bluetooth shell commands that can be useful to applications.
 */

/*
 * Copyright (c) 2017 Intel Corporation
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <zephyr/types.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <misc/printk.h>
#include <misc/byteorder.h>
#include <misc/util.h>
#include <zephyr.h>
#include <hex_str.h>

#include <acts_bluetooth/hci.h>
#include <acts_bluetooth/bluetooth.h>
#include <acts_bluetooth/conn.h>
#include <acts_bluetooth/rfcomm.h>
#include <acts_bluetooth/sdp.h>
#include <acts_bluetooth/hci.h>

#include <shell/shell.h>

#include "bt.h"
//#include "ll.h"
//#include "hci.h"

#define DEVICE_NAME		CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN		(sizeof(DEVICE_NAME) - 1)

static u8_t selected_id = BT_ID_DEFAULT;

#if defined(CONFIG_BT_CONN)
struct bt_conn *default_conn;

/* Connection context for BR/EDR legacy pairing in sec mode 3 */
static struct bt_conn *pairing_conn;

static struct bt_le_oob oob_local;
#if defined(CONFIG_BT_SMP) || defined(CONFIG_BT_BREDR)
static struct bt_le_oob oob_remote;
#endif /* CONFIG_BT_SMP || CONFIG_BT_BREDR) */
#endif /* CONFIG_BT_CONN */

#define NAME_LEN 30

#define KEY_STR_LEN 33

#if defined(CONFIG_BT_EXT_ADV)
static u8_t selected_adv;
struct bt_le_ext_adv *adv_sets[CONFIG_BT_EXT_ADV_MAX_ADV_SET];
#endif

static size_t bin2hex(const u8_t *buf, size_t buflen, char *hex, size_t hexlen)
{
	if ((hexlen + 1) < buflen * 2) {
		return 0;
	}

	for (size_t i = 0; i < buflen; i++) {
		if (hex2char(buf[i] >> 4, &hex[2 * i]) < 0) {
			return 0;
		}
		if (hex2char(buf[i] & 0xf, &hex[2 * i + 1]) < 0) {
			return 0;
		}
	}

	hex[2 * buflen] = '\0';
	return 2 * buflen;
}

static size_t hex2bin(const char *hex, size_t hexlen, u8_t *buf, size_t buflen)
{
	u8_t dec;

	if (buflen < hexlen / 2 + hexlen % 2) {
		return 0;
	}

	/* if hexlen is uneven, insert leading zero nibble */
	if (hexlen % 2) {
		if (char2hex(hex[0], &dec) < 0) {
			return 0;
		}
		buf[0] = dec;
		hex++;
		buf++;
	}

	/* regular hex conversion */
	for (size_t i = 0; i < hexlen / 2; i++) {
		if (char2hex(hex[2 * i], &dec) < 0) {
			return 0;
		}
		buf[i] = dec << 4;

		if (char2hex(hex[2 * i + 1], &dec) < 0) {
			return 0;
		}
		buf[i] += dec;
	}

	return hexlen / 2 + hexlen % 2;
}

#if defined(CONFIG_BT_OBSERVER)
static bool data_cb(struct bt_data *data, void *user_data)
{
	char *name = user_data;

	switch (data->type) {
	case BT_DATA_NAME_SHORTENED:
	case BT_DATA_NAME_COMPLETE:
		memcpy(name, data->data, min(data->data_len, NAME_LEN - 1));
		return false;
	default:
		return true;
	}
}

static const char *phy2str(u8_t phy)
{
	switch (phy) {
	case 0: return "No packets";
	case BT_GAP_LE_PHY_1M: return "LE 1M";
	case BT_GAP_LE_PHY_2M: return "LE 2M";
	case BT_GAP_LE_PHY_CODED: return "LE Coded";
	default: return "Unknown";
	}
}

static void scan_recv(const struct bt_le_scan_recv_info *info,
		      struct net_buf_simple *buf)
{
	char le_addr[BT_ADDR_LE_STR_LEN];
	char name[NAME_LEN];

	(void)memset(name, 0, sizeof(name));

	bt_data_parse(buf, data_cb, name);

	bt_addr_le_to_str(info->addr, le_addr, sizeof(le_addr));
	printk("[DEVICE]: %s, AD evt type %u, RSSI %i %s "
		    "C:%u S:%u D:%d SR:%u E:%u Prim: %s, Secn: %s\n",
		    le_addr, info->adv_type, info->rssi, name,
		    (info->adv_props & BT_GAP_ADV_PROP_CONNECTABLE) != 0,
		    (info->adv_props & BT_GAP_ADV_PROP_SCANNABLE) != 0,
		    (info->adv_props & BT_GAP_ADV_PROP_DIRECTED) != 0,
		    (info->adv_props & BT_GAP_ADV_PROP_SCAN_RESPONSE) != 0,
		    (info->adv_props & BT_GAP_ADV_PROP_EXT_ADV) != 0,
		    phy2str(info->primary_phy), phy2str(info->secondary_phy));
}

static void scan_timeout(void)
{
	printk("Scan timeout\n");
}
#endif /* CONFIG_BT_OBSERVER */

#if defined(CONFIG_BT_BROADCASTER) && defined(CONFIG_BT_EXT_ADV)
static void adv_sent(struct bt_le_ext_adv *adv,
		     struct bt_le_ext_adv_sent_info *info)
{
	printk("Advertiser[%d] %p sent %d\n",
		    bt_le_ext_adv_get_index(adv), adv, info->num_sent);
}

static void adv_connected(struct bt_le_ext_adv *adv,
			  struct bt_le_ext_adv_connected_info *info)
{
	char str[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(info->conn), str, sizeof(str));

	printk("Advertiser[%d] %p connected by %s\n",
		    bt_le_ext_adv_get_index(adv), adv, str);
}

static void adv_scanned(struct bt_le_ext_adv *adv,
			struct bt_le_ext_adv_scanned_info *info)
{
	char str[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(info->addr, str, sizeof(str));

	printk("Advertiser[%d] %p scanned by %s\n",
		    bt_le_ext_adv_get_index(adv), adv, str);
}
#endif /* defined(CONFIG_BT_BROADCASTER) && defined(CONFIG_BT_EXT_ADV) */

#if !defined(CONFIG_BT_CONN)
static const char *current_prompt(void)
{
	return NULL;
}
#endif /* !CONFIG_BT_CONN */

#if defined(CONFIG_BT_CONN)
static const char *current_prompt(void)
{
	static char str[BT_ADDR_LE_STR_LEN + 2];
	static struct bt_conn_info info;

	if (!default_conn) {
		return NULL;
	}

	if (bt_conn_get_info(default_conn, &info) < 0) {
		return NULL;
	}

	if (info.type != BT_CONN_TYPE_LE) {
		return NULL;
	}

	bt_addr_le_to_str(info.le.dst, str, sizeof(str) - 2);
	strcat(str, "> ");
	return str;
}

void conn_addr_str(struct bt_conn *conn, char *addr, size_t len)
{
	struct bt_conn_info info;

	if (bt_conn_get_info(conn, &info) < 0) {
		addr[0] = '\0';
		return;
	}

	switch (info.type) {
#if defined(CONFIG_BT_BREDR)
	case BT_CONN_TYPE_BR:
		bt_addr_to_str(info.br.dst, addr, len);
		break;
#endif
	case BT_CONN_TYPE_LE:
		bt_addr_le_to_str(info.le.dst, addr, len);
		break;
	}
}

static void print_le_oob(struct bt_le_oob *oob)
{
	char addr[BT_ADDR_LE_STR_LEN];
	char c[KEY_STR_LEN];
	char r[KEY_STR_LEN];

	bt_addr_le_to_str(&oob->addr, addr, sizeof(addr));

	bin2hex(oob->le_sc_data.c, sizeof(oob->le_sc_data.c), c, sizeof(c));
	bin2hex(oob->le_sc_data.r, sizeof(oob->le_sc_data.r), r, sizeof(r));

	printk("OOB data:\n");
	printk("%-29s %-32s %-32s\n", "addr", "random", "confirm");
	printk("%29s %32s %32s\n", addr, r, c);
}

static void connected(struct bt_conn *conn, u8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	conn_addr_str(conn, addr, sizeof(addr));

	if (err) {
		printk("Failed to connect to %s (0x%02x)\n", addr,
			     err);
		goto done;
	}

	printk("Connected: %s\n", addr);

	if (!default_conn) {
		default_conn = bt_conn_ref(conn);
	}

done:
	/* clear connection reference for sec mode 3 pairing */
	if (pairing_conn) {
		bt_conn_unref(pairing_conn);
		pairing_conn = NULL;
	}
}

static void disconnected(struct bt_conn *conn, u8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	conn_addr_str(conn, addr, sizeof(addr));
	printk("Disconnected: %s (reason 0x%02x)\n", addr, reason);

	if (default_conn == conn) {
		bt_conn_unref(default_conn);
		default_conn = NULL;
	}
}

static bool le_param_req(struct bt_conn *conn, struct bt_le_conn_param *param)
{
	printk("LE conn  param req: int (0x%04x, 0x%04x) lat %d\n"
		    " to %d", param->interval_min, param->interval_max,
		    param->latency, param->timeout);

	return true;
}

static void le_param_updated(struct bt_conn *conn, u16_t interval,
			     u16_t latency, u16_t timeout)
{
	printk("LE conn param updated: int 0x%04x lat %d "
	       "to %d\n", interval, latency, timeout);
}

#if defined(CONFIG_BT_SMP)
static void identity_resolved(struct bt_conn *conn, const bt_addr_le_t *rpa,
			      const bt_addr_le_t *identity)
{
	char addr_identity[BT_ADDR_LE_STR_LEN];
	char addr_rpa[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(identity, addr_identity, sizeof(addr_identity));
	bt_addr_le_to_str(rpa, addr_rpa, sizeof(addr_rpa));

	printk("Identity resolved %s -> %s\n", addr_rpa,
	      addr_identity);
}
#endif

#if defined(CONFIG_BT_SMP) || defined(CONFIG_BT_BREDR)
static void security_changed(struct bt_conn *conn, bt_security_t level,
			     enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	conn_addr_str(conn, addr, sizeof(addr));

	if (!err) {
		printk("Security changed: %s level %u\n", addr,
		       level);
	} else {
		printk("Security failed: %s level %u reason %d\n",
		       addr, level, err);
	}
}
#endif

#if defined(CONFIG_BT_REMOTE_INFO)
static const char *ver_str(u8_t ver)
{
	const char * const str[] = {
		"1.0b", "1.1", "1.2", "2.0", "2.1", "3.0", "4.0", "4.1", "4.2",
		"5.0", "5.1",
	};

	if (ver < ARRAY_SIZE(str)) {
		return str[ver];
	}

	return "unknown";
}

static void remote_info_available(struct bt_conn *conn,
				  struct bt_conn_remote_info *remote_info)
{
	struct bt_conn_info info;

	bt_conn_get_info(conn, &info);

	if (IS_ENABLED(CONFIG_BT_REMOTE_VERSION)) {
		printk("Remote LMP version %s (0x%02x) subversion 0x%04x "
		       "manufacturer 0x%04x\n", ver_str(remote_info->version),
		       remote_info->version, remote_info->subversion,
		       remote_info->manufacturer);
	}

	if (info.type == BT_CONN_TYPE_LE) {
		u8_t features[8];
		char features_str[2 * sizeof(features) +  1];

		sys_memcpy_swap(features, remote_info->le.features,
				sizeof(features));
		bin2hex(features, sizeof(features),
			features_str, sizeof(features_str));
		printk("LE Features: 0x%s\n", features_str);
	}
}
#endif /* defined(CONFIG_BT_REMOTE_INFO) */

#if defined(CONFIG_BT_USER_DATA_LEN_UPDATE)
void le_data_len_updated(struct bt_conn *conn,
			 struct bt_conn_le_data_len_info *info)
{
	printk("LE data len updated: TX (len: %d time: %d)"
	       " RX (len: %d time: %d)\n", info->tx_max_len,
	       info->tx_max_time, info->rx_max_len, info->rx_max_time);
}
#endif

#if defined(CONFIG_BT_USER_PHY_UPDATE)
void le_phy_updated(struct bt_conn *conn,
		    struct bt_conn_le_phy_info *info)
{
	printk("LE PHY updated: TX PHY %s, RX PHY %s\n",
		    phy2str(info->tx_phy), phy2str(info->rx_phy));
}
#endif

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
	.le_param_req = le_param_req,
	.le_param_updated = le_param_updated,
#if defined(CONFIG_BT_SMP)
	.identity_resolved = identity_resolved,
#endif
#if defined(CONFIG_BT_SMP) || defined(CONFIG_BT_BREDR)
	.security_changed = security_changed,
#endif
#if defined(CONFIG_BT_REMOTE_INFO)
	.remote_info_available = remote_info_available,
#endif
#if defined(CONFIG_BT_USER_DATA_LEN_UPDATE)
	.le_data_len_updated = le_data_len_updated,
#endif
#if defined(CONFIG_BT_USER_PHY_UPDATE)
	.le_phy_updated = le_phy_updated,
#endif
};
#endif /* CONFIG_BT_CONN */

#if defined(CONFIG_BT_OBSERVER)
static struct bt_le_scan_cb scan_callbacks = {
	.recv = scan_recv,
	.timeout = scan_timeout,
};
#endif /* defined(CONFIG_BT_OBSERVER) */

#if defined(CONFIG_BT_BROADCASTER) && defined(CONFIG_BT_EXT_ADV)
static struct bt_le_ext_adv_cb adv_callbacks = {
	.sent = adv_sent,
	.connected = adv_connected,
	.scanned = adv_scanned,
};
#endif /* defined(CONFIG_BT_BROADCASTER) && defined(CONFIG_BT_EXT_ADV) */

static void bt_ready(int err)
{
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

#if 0
	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load();
	}
#endif

	if (IS_ENABLED(CONFIG_BT_SMP_OOB_LEGACY_PAIR_ONLY)) {
		bt_set_oob_data_flag(true);
	}

#if defined(CONFIG_BT_OBSERVER)
	bt_le_scan_cb_register(&scan_callbacks);
#endif

#if defined(CONFIG_BT_CONN)
	default_conn = NULL;

	bt_conn_cb_register(&conn_callbacks);
#endif /* CONFIG_BT_CONN */
}

static int cmd_init(int argc, char *argv[])
{
	int err;

	err = bt_enable(bt_ready);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
	}

	return err;
}

#if defined(CONFIG_BT_HCI) || defined(CONFIG_BT_L2CAP_DYNAMIC_CHANNEL)
static void __unused hexdump(const u8_t *data, size_t len)
{
	int n = 0;

	while (len--) {
		if (n % 16 == 0) {
			printk("%08X ", n);
		}

		printk("%02X ", *data++);

		n++;
		if (n % 8 == 0) {
			if (n % 16 == 0) {
				printk("\n");
			} else {
				printk(" ");
			}
		}
	}

	if (n % 16) {
		printk("\n");
	}
}
#endif /* CONFIG_BT_HCI || CONFIG_BT_L2CAP_DYNAMIC_CHANNEL */

#if defined(CONFIG_BT_HCI)
static int cmd_hci_cmd(int argc, char *argv[])
{
	u8_t ogf;
	u16_t ocf;
	struct net_buf *buf = NULL, *rsp;
	int err;

	ogf = strtoul(argv[1], NULL, 16);
	ocf = strtoul(argv[2], NULL, 16);

	if (argc > 3) {
		size_t i;

		buf = bt_hci_cmd_create(BT_OP(ogf, ocf), argc - 3);

		for (i = 3; i < argc; i++) {
			net_buf_add_u8(buf, strtoul(argv[i], NULL, 16));
		}
	}

	err = bt_hci_cmd_send_sync(BT_OP(ogf, ocf), buf, &rsp);
	if (err) {
		printk("HCI command failed (err %d)\n", err);
		return err;
	} else {
		hexdump(rsp->data, rsp->len);
		net_buf_unref(rsp);
	}

	return 0;
}
#endif /* CONFIG_BT_HCI */

static int cmd_name(int argc, char *argv[])
{
	int err;

	if (argc < 2) {
		printk("Bluetooth Local Name: %s\n", bt_get_name());
		return 0;
	}

	err = bt_set_name(argv[1]);
	if (err) {
		printk("Unable to set name %s (err %d)\n", argv[1],
			    err);
		return err;
	}

	return 0;
}

static int cmd_id_create(int argc, char *argv[])
{
	char addr_str[BT_ADDR_LE_STR_LEN];
	bt_addr_le_t addr;
	int err;

	if (argc > 1) {
		err = bt_addr_le_from_str(argv[1], "random", &addr);
		if (err) {
			printk("Invalid address\n");
		}
	} else {
		bt_addr_le_copy(&addr, BT_ADDR_LE_ANY);
	}

	err = bt_id_create(&addr, NULL);
	if (err < 0) {
		printk("Creating new ID failed (err %d)\n", err);
	}

	bt_addr_le_to_str(&addr, addr_str, sizeof(addr_str));
	printk("New identity (%d) created: %s\n", err, addr_str);

	return 0;
}

static int cmd_id_reset(int argc, char *argv[])
{
	char addr_str[BT_ADDR_LE_STR_LEN];
	bt_addr_le_t addr;
	u8_t id;
	int err;

	if (argc < 2) {
		printk("Identity identifier not specified\n");
		return -ENOEXEC;
	}

	id = strtol(argv[1], NULL, 10);

	if (argc > 2) {
		err = bt_addr_le_from_str(argv[2], "random", &addr);
		if (err) {
			printk("Invalid address\n");
			return err;
		}
	} else {
		bt_addr_le_copy(&addr, BT_ADDR_LE_ANY);
	}

	err = bt_id_reset(id, &addr, NULL);
	if (err < 0) {
		printk("Resetting ID %u failed (err %d)\n", id, err);
		return err;
	}

	bt_addr_le_to_str(&addr, addr_str, sizeof(addr_str));
	printk("Identity %u reset: %s\n", id, addr_str);

	return 0;
}

static int cmd_id_delete(int argc, char *argv[])
{
	u8_t id;
	int err;

	if (argc < 2) {
		printk("Identity identifier not specified\n");
		return -ENOEXEC;
	}

	id = strtol(argv[1], NULL, 10);

	err = bt_id_delete(id);
	if (err < 0) {
		printk("Deleting ID %u failed (err %d)\n", id, err);
		return err;
	}

	printk("Identity %u deleted\n", id);

	return 0;
}

static int cmd_id_show(int argc, char *argv[])
{
	bt_addr_le_t addrs[CONFIG_BT_ID_MAX];
	size_t i, count = CONFIG_BT_ID_MAX;

	bt_id_get(addrs, &count);

	for (i = 0; i < count; i++) {
		char addr_str[BT_ADDR_LE_STR_LEN];

		bt_addr_le_to_str(&addrs[i], addr_str, sizeof(addr_str));
		printk("%s%zu: %s\n", i == selected_id ? "*" : " ", i,
		      addr_str);
	}

	return 0;
}

static int cmd_id_select(int argc, char *argv[])
{
	char addr_str[BT_ADDR_LE_STR_LEN];
	bt_addr_le_t addrs[CONFIG_BT_ID_MAX];
	size_t count = CONFIG_BT_ID_MAX;
	u8_t id;

	id = strtol(argv[1], NULL, 10);

	bt_id_get(addrs, &count);
	if (count <= id) {
		printk("Invalid identity\n");
		return -ENOEXEC;
	}

	bt_addr_le_to_str(&addrs[id], addr_str, sizeof(addr_str));
	printk("Selected identity: %s\n", addr_str);
	selected_id = id;

	return 0;
}

#if defined(CONFIG_BT_OBSERVER)
static int cmd_active_scan_on(u32_t options,
			      u16_t timeout)
{
	int err;
	struct bt_le_scan_param param = {
			.type       = BT_LE_SCAN_TYPE_ACTIVE,
			.options    = BT_LE_SCAN_OPT_FILTER_DUPLICATE,
			.interval   = BT_GAP_SCAN_FAST_INTERVAL,
			.window     = BT_GAP_SCAN_FAST_WINDOW,
			.timeout    = timeout, };

	param.options |= options;

	err = bt_le_scan_start(&param, NULL);
	if (err) {
		printk("Bluetooth set active scan failed "
		      "(err %d)\n", err);
		return err;
	} else {
		printk("Bluetooth active scan enabled\n");
	}

	return 0;
}

static int cmd_passive_scan_on(u32_t options,
			       u16_t timeout)
{
	struct bt_le_scan_param param = {
			.type       = BT_LE_SCAN_TYPE_PASSIVE,
			.options    = BT_LE_SCAN_OPT_NONE,
			.interval   = 0x10,
			.window     = 0x10,
			.timeout    = timeout, };
	int err;

	param.options |= options;

	err = bt_le_scan_start(&param, NULL);
	if (err) {
		printk("Bluetooth set passive scan failed "
			    "(err %d)\n", err);
		return err;
	} else {
		printk("Bluetooth passive scan enabled\n");
	}

	return 0;
}

static int cmd_scan_off(void)
{
	int err;

	err = bt_le_scan_stop();
	if (err) {
		printk("Stopping scanning failed (err %d)\n", err);
		return err;
	} else {
		printk("Scan successfully stopped\n");
	}

	return 0;
}

static int cmd_scan(int argc, char *argv[])
{
	const char *action;
	u32_t options = 0;
	u16_t timeout = 0;

	/* Parse duplicate filtering data */
	for (size_t argn = 2; argn < argc; argn++) {
		const char *arg = argv[argn];

		if (!strcmp(arg, "dups")) {
			options |= BT_LE_SCAN_OPT_FILTER_DUPLICATE;
		} else if (!strcmp(arg, "nodups")) {
			options &= ~BT_LE_SCAN_OPT_FILTER_DUPLICATE;
		} else if (!strcmp(arg, "wl")) {
			options |= BT_LE_SCAN_OPT_FILTER_WHITELIST;
		} else if (!strcmp(arg, "coded")) {
			options |= BT_LE_SCAN_OPT_CODED;
		} else if (!strcmp(arg, "no-1m")) {
			options |= BT_LE_SCAN_OPT_NO_1M;
		} else if (!strcmp(arg, "timeout")) {
			if (++argn == argc) {
				return -EINVAL;
			}

			timeout = strtoul(argv[argn], NULL, 16);
		} else {
			return -EINVAL;
		}
	}

	action = argv[1];
	if (!strcmp(action, "on")) {
		return cmd_active_scan_on(options, timeout);
	} else if (!strcmp(action, "off")) {
		return cmd_scan_off();
	} else if (!strcmp(action, "passive")) {
		return cmd_passive_scan_on(options, timeout);
	} else {
		return -EINVAL;
	}

	return 0;
}
#endif /* CONFIG_BT_OBSERVER */

#if defined(CONFIG_BT_BROADCASTER)
static const struct bt_data ad_discov[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
};

static int cmd_advertise(int argc, char *argv[])
{
	struct bt_le_adv_param param = {};
	const struct bt_data *ad;
	size_t ad_len;
	int err;

	if (!strcmp(argv[1], "off")) {
		if (bt_le_adv_stop() < 0) {
			printk("Failed to stop advertising\n");
			return -ENOEXEC;
		} else {
			printk("Advertising stopped\n");
		}

		return 0;
	}

	param.id = selected_id;
	param.interval_min = BT_GAP_ADV_FAST_INT_MIN_2;
	param.interval_max = BT_GAP_ADV_FAST_INT_MAX_2;

	if (!strcmp(argv[1], "on")) {
		param.options = (BT_LE_ADV_OPT_CONNECTABLE |
				 BT_LE_ADV_OPT_USE_NAME);
	} else if (!strcmp(argv[1], "scan")) {
		param.options = BT_LE_ADV_OPT_USE_NAME;
	} else if (!strcmp(argv[1], "nconn")) {
		param.options = 0U;
	} else {
		goto fail;
	}

	ad = ad_discov;
	ad_len = ARRAY_SIZE(ad_discov);

	for (size_t argn = 2; argn < argc; argn++) {
		const char *arg = argv[argn];

		if (!strcmp(arg, "discov")) {
			/* Default */
		} else if (!strcmp(arg, "non_discov")) {
			ad = NULL;
			ad_len = 0;
		} else if (!strcmp(arg, "wl")) {
			param.options |= BT_LE_ADV_OPT_FILTER_SCAN_REQ;
			param.options |= BT_LE_ADV_OPT_FILTER_CONN;
		} else if (!strcmp(arg, "wl-scan")) {
			param.options |= BT_LE_ADV_OPT_FILTER_SCAN_REQ;
		} else if (!strcmp(arg, "wl-conn")) {
			param.options |= BT_LE_ADV_OPT_FILTER_CONN;
		} else if (!strcmp(arg, "identity")) {
			param.options |= BT_LE_ADV_OPT_USE_IDENTITY;
		} else if (!strcmp(arg, "no-name")) {
			param.options &= ~BT_LE_ADV_OPT_USE_NAME;
		} else {
			goto fail;
		}
	}

	err = bt_le_adv_start(&param, ad, ad_len, NULL, 0);
	if (err < 0) {
		printk("Failed to start advertising (err %d)\n",
			    err);
		return err;
	} else {
		printk("Advertising started\n");
	}

	return 0;

fail:
	return -ENOEXEC;
}

#if defined(CONFIG_BT_PERIPHERAL)
static int cmd_directed_adv(int argc, char *argv[])
{
	int err;
	bt_addr_le_t addr;
	struct bt_le_adv_param param;

	err = bt_addr_le_from_str(argv[1], argv[2], &addr);
	param = *BT_LE_ADV_CONN_DIR(&addr);
	if (err) {
		printk("Invalid peer address (err %d)\n", err);
		return err;
	}

	if (argc > 3) {
		if (!strcmp(argv[3], "low")) {
			param = *BT_LE_ADV_CONN_DIR_LOW_DUTY(&addr);
		} else {
			return -ENOEXEC;
		}
	}

	err = bt_le_adv_start(&param, NULL, 0, NULL, 0);
	if (err) {
		printk("Failed to start directed advertising (%d)\n",
			    err);
		return -ENOEXEC;
	} else {
		printk("Started directed advertising\n");
	}

	return 0;
}
#endif /* CONFIG_BT_PERIPHERAL */

#if defined(CONFIG_BT_EXT_ADV)
static bool adv_param_parse(int argc, char *argv[],
			   struct bt_le_adv_param *param)
{
	memset(param, 0, sizeof(struct bt_le_adv_param));

	if (!strcmp(argv[1], "conn-scan")) {
		param->options |= BT_LE_ADV_OPT_CONNECTABLE;
		param->options |= BT_LE_ADV_OPT_SCANNABLE;
	} else if (!strcmp(argv[1], "conn-nscan")) {
		param->options |= BT_LE_ADV_OPT_CONNECTABLE;
	} else if (!strcmp(argv[1], "nconn-scan")) {
		param->options |= BT_LE_ADV_OPT_SCANNABLE;
	} else if (!strcmp(argv[1], "nconn-nscan")) {
		/* Acceptable option, nothing to do */
	} else {
		return false;
	}

	for (size_t argn = 2; argn < argc; argn++) {
		const char *arg = argv[argn];

		if (!strcmp(arg, "ext-adv")) {
			param->options |= BT_LE_ADV_OPT_EXT_ADV;
		} else if (!strcmp(arg, "coded")) {
			param->options |= BT_LE_ADV_OPT_CODED;
		} else if (!strcmp(arg, "no-2m")) {
			param->options |= BT_LE_ADV_OPT_NO_2M;
		} else if (!strcmp(arg, "anon")) {
			param->options |= BT_LE_ADV_OPT_ANONYMOUS;
		} else if (!strcmp(arg, "tx-power")) {
			param->options |= BT_LE_ADV_OPT_USE_TX_POWER;
		} else if (!strcmp(arg, "scan-reports")) {
			param->options |= BT_LE_ADV_OPT_NOTIFY_SCAN_REQ;
		} else if (!strcmp(arg, "wl")) {
			param->options |= BT_LE_ADV_OPT_FILTER_SCAN_REQ;
			param->options |= BT_LE_ADV_OPT_FILTER_CONN;
		} else if (!strcmp(arg, "wl-scan")) {
			param->options |= BT_LE_ADV_OPT_FILTER_SCAN_REQ;
		} else if (!strcmp(arg, "wl-conn")) {
			param->options |= BT_LE_ADV_OPT_FILTER_CONN;
		} else if (!strcmp(arg, "identity")) {
			param->options |= BT_LE_ADV_OPT_USE_IDENTITY;
		} else if (!strcmp(arg, "name")) {
			param->options |= BT_LE_ADV_OPT_USE_NAME;
		} else if (!strcmp(arg, "low")) {
			param->options |= BT_LE_ADV_OPT_DIR_MODE_LOW_DUTY;
		} else if (!strcmp(arg, "directed")) {
			static bt_addr_le_t addr;

			if ((argn + 2) >= argc) {
				return false;
			}

			if (bt_addr_le_from_str(argv[argn + 1], argv[argn + 2],
						&addr)) {
				return false;
			}

			param->peer = &addr;
			argn += 2;
		} else {
			return false;
		}
	}

	param->id = selected_id;
	param->sid = 0;
	if (param->peer &&
	    !(param->options & BT_LE_ADV_OPT_DIR_MODE_LOW_DUTY)) {
		param->interval_min = 0;
		param->interval_max = 0;
	} else {
		param->interval_min = BT_GAP_ADV_FAST_INT_MIN_2;
		param->interval_max = BT_GAP_ADV_FAST_INT_MAX_2;
	}

	return true;
}

static int cmd_adv_create(int argc, char *argv[])
{
	struct bt_le_adv_param param;
	struct bt_le_ext_adv *adv;
	u8_t adv_index;
	int err;

	if (!adv_param_parse(argc, argv, &param)) {
		return -ENOEXEC;
	}

	err = bt_le_ext_adv_create(&param, &adv_callbacks, &adv);
	if (err) {
		printk("Failed to create advertiser set (%d)\n", err);
		return -ENOEXEC;
	}

	adv_index = bt_le_ext_adv_get_index(adv);
	adv_sets[adv_index] = adv;

	printk("Created adv id: %d, adv: %p\n", adv_index, adv);

	return 0;
}

static int cmd_adv_param(int argc, char *argv[])
{
	struct bt_le_ext_adv *adv = adv_sets[selected_adv];
	struct bt_le_adv_param param;
	int err;

	if (!adv_param_parse(argc, argv, &param)) {
		return -ENOEXEC;
	}

	err = bt_le_ext_adv_update_param(adv, &param);
	if (err) {
		printk("Failed to update advertiser set (%d)\n", err);
		return -ENOEXEC;
	}

	return 0;
}

static int cmd_adv_data(int argc, char *argv[])
{
	u8_t discov_data = (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR);
	struct bt_le_ext_adv *adv = adv_sets[selected_adv];
	static u8_t hex_data[1650];
	struct bt_data *data;
	struct bt_data ad[8];
	struct bt_data sd[8];
	size_t hex_data_len;
	size_t ad_len = 0;
	size_t sd_len = 0;
	size_t *data_len;
	int err;

	if (!adv) {
		return -EINVAL;
	}

	hex_data_len = 0;
	data = ad;
	data_len = &ad_len;

	for (size_t argn = 1; argn < argc; argn++) {
		const char *arg = argv[argn];

		if (strcmp(arg, "scan-response") &&
		    *data_len == ARRAY_SIZE(ad)) {
			/* Maximum entries limit reached. */
			return -ENOEXEC;
		}

		if (!strcmp(arg, "discov")) {
			data[*data_len].type = BT_DATA_FLAGS;
			data[*data_len].data_len = sizeof(discov_data);
			data[*data_len].data = &discov_data;
			(*data_len)++;
		} else if (!strcmp(arg, "name")) {
			const char *name = bt_get_name();

			data[*data_len].type = BT_DATA_NAME_COMPLETE;
			data[*data_len].data_len = strlen(name);
			data[*data_len].data = name;
			(*data_len)++;
		} else if (!strcmp(arg, "scan-response")) {
			if (data == sd) {
				return -ENOEXEC;
			}

			data = sd;
			data_len = &sd_len;
		} else {
			size_t len;

			len = hex2bin(arg, strlen(arg), &hex_data[hex_data_len],
				      sizeof(hex_data) - hex_data_len);

			if (!len || (len - 1) != (hex_data[hex_data_len])) {
				return -ENOEXEC;
			}

			data[*data_len].type = hex_data[hex_data_len + 1];
			data[*data_len].data_len = hex_data[hex_data_len];
			data[*data_len].data = &hex_data[hex_data_len + 2];
			(*data_len)++;
			hex_data_len += len;
		}
	}

	err = bt_le_ext_adv_set_data(adv, ad_len > 0 ? ad : NULL, ad_len,
					  sd_len > 0 ? sd : NULL, sd_len);
	if (err) {
		printk("Failed to set advertising set data (%d)\n",
			    err);
		return -ENOEXEC;
	}

	return 0;
}

static int cmd_adv_start(int argc, char *argv[])
{
	struct bt_le_ext_adv *adv = adv_sets[selected_adv];
	struct bt_le_ext_adv_start_param param;
	u8_t num_events = 0;
	s32_t timeout = 0;
	int err;

	if (!adv) {
		printk("Advertiser[%d] not created\n", selected_adv);
		return -EINVAL;
	}

	for (size_t argn = 1; argn < argc; argn++) {
		const char *arg = argv[argn];

		if (!strcmp(arg, "timeout")) {
			if (++argn == argc) {
				goto fail_show_help;
			}

			timeout = strtoul(argv[argn], NULL, 16);
		}

		if (!strcmp(arg, "num_events")) {
			if (++argn == argc) {
				goto fail_show_help;
			}

			num_events = strtoul(argv[argn], NULL, 16);
		}
	}

	param.timeout = timeout;
	param.num_events = num_events;

	err = bt_le_ext_adv_start(adv, &param);
	if (err) {
		printk("Failed to start advertising set (%d)\n", err);
		return -ENOEXEC;
	}

	printk("Advertiser[%d] %p set started\n", selected_adv, adv);
	return 0;

fail_show_help:
	return -ENOEXEC;
}

static int cmd_adv_stop(int argc, char *argv[])
{
	struct bt_le_ext_adv *adv = adv_sets[selected_adv];
	int err;

	if (!adv) {
		printk("Advertiser[%d] not created\n", selected_adv);
		return -EINVAL;
	}

	err = bt_le_ext_adv_stop(adv);
	if (err) {
		printk("Failed to stop advertising set (%d)\n", err);
		return -ENOEXEC;
	}

	printk("Advertiser set stopped\n");
	return 0;
}

static int cmd_adv_delete(int argc, char *argv[])
{
	struct bt_le_ext_adv *adv = adv_sets[selected_adv];
	int err;

	if (!adv) {
		printk("Advertiser[%d] not created\n", selected_adv);
		return -EINVAL;
	}

	err = bt_le_ext_adv_delete(adv);
	if (err) {
		printk("Failed to delete advertiser set\n");
		return err;
	}

	adv_sets[selected_adv] = NULL;
	return 0;
}

static int cmd_adv_select(int argc, char *argv[])
{
	if (argc == 2) {
		u8_t id = strtol(argv[1], NULL, 10);

		if (!(id < ARRAY_SIZE(adv_sets))) {
			return -EINVAL;
		}

		selected_adv = id;
		return 0;
	}

	for (int i = 0; i < ARRAY_SIZE(adv_sets); i++) {
		if (adv_sets[i]) {
			printk("Advertiser[%d] %p\n", i, adv_sets[i]);
		}
	}

	return -ENOEXEC;
}

static int cmd_adv_oob(int argc, char *argv[])
{
	struct bt_le_ext_adv *adv = adv_sets[selected_adv];
	int err;

	if (!adv) {
		return -EINVAL;
	}

	err = bt_le_ext_adv_oob_get_local(adv, &oob_local);
	if (err) {
		printk("OOB data failed\n");
		return err;
	}

	print_le_oob(&oob_local);

	return 0;
}

static int cmd_adv_info(int argc, char *argv[])
{
	struct bt_le_ext_adv *adv = adv_sets[selected_adv];
	struct bt_le_ext_adv_info info;
	int err;

	if (!adv) {
		return -EINVAL;
	}

	err = bt_le_ext_adv_get_info(adv, &info);
	if (err) {
		printk("OOB data failed\n");
		return err;
	}

	printk("Advertiser[%d] %p", selected_adv, adv);
	printk("Id: %d, TX power: %d dBm\n", info.id, info.tx_power);

	return 0;
}
#endif /* CONFIG_BT_EXT_ADV */
#endif /* CONFIG_BT_BROADCASTER */

#if defined(CONFIG_BT_CONN)
#if defined(CONFIG_BT_CENTRAL)
static int cmd_connect_le(int argc, char *argv[])
{
	int err;
	bt_addr_le_t addr;
	struct bt_conn *conn;
	u32_t options = 0;

	err = bt_addr_le_from_str(argv[1], argv[2], &addr);
	if (err) {
		printk("Invalid peer address (err %d)\n", err);
		return err;
	}

#if defined(CONFIG_BT_EXT_ADV)
	for (size_t argn = 3; argn < argc; argn++) {
		const char *arg = argv[argn];

		if (!strcmp(arg, "coded")) {
			options |= BT_CONN_LE_OPT_CODED;
		} else if (!strcmp(arg, "no-1m")) {
			options |= BT_CONN_LE_OPT_NO_1M;
		} else {
			return -EINVAL;
		}
	}
#endif /* defined(CONFIG_BT_EXT_ADV) */

	struct bt_conn_le_create_param *create_params =
		BT_CONN_LE_CREATE_PARAM(options,
					BT_GAP_SCAN_FAST_INTERVAL,
					BT_GAP_SCAN_FAST_INTERVAL);

	err = bt_conn_le_create(&addr, create_params, BT_LE_CONN_PARAM_DEFAULT,
				&conn);
	if (err) {
		printk("Connection failed (%d)\n", err);
		return -ENOEXEC;
	} else {
		printk("Connection pending\n");

		/* unref connection obj in advance as app user */
		bt_conn_unref(conn);
	}

	return 0;
}

#if !defined(CONFIG_BT_WHITELIST)
static int cmd_auto_conn(int argc, char *argv[])
{
	bt_addr_le_t addr;
	int err;

	err = bt_addr_le_from_str(argv[1], argv[2], &addr);
	if (err) {
		printk("Invalid peer address (err %d)\n", err);
		return err;
	}

	if (argc < 4) {
		return bt_le_set_auto_conn(&addr, BT_LE_CONN_PARAM_DEFAULT);
	} else if (!strcmp(argv[3], "on")) {
		return bt_le_set_auto_conn(&addr, BT_LE_CONN_PARAM_DEFAULT);
	} else if (!strcmp(argv[3], "off")) {
		return bt_le_set_auto_conn(&addr, NULL);
	} else {
		return -EINVAL;
	}

	return 0;
}
#endif /* !defined(CONFIG_BT_WHITELIST) */
#endif /* CONFIG_BT_CENTRAL */

static int cmd_disconnect(int argc, char *argv[])
{
	struct bt_conn *conn;
	int err;

	if (default_conn && argc < 3) {
		conn = bt_conn_ref(default_conn);
	} else {
		bt_addr_le_t addr;

		if (argc < 3) {
			return -EINVAL;
		}

		err = bt_addr_le_from_str(argv[1], argv[2], &addr);
		if (err) {
			printk("Invalid peer address (err %d)\n",
				    err);
			return err;
		}

		conn = bt_conn_lookup_addr_le(selected_id, &addr);
	}

	if (!conn) {
		printk("Not connected\n");
		return -ENOEXEC;
	}

	err = bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	if (err) {
		printk("Disconnection failed (err %d)\n", err);
		return err;
	}

	bt_conn_unref(conn);

	return 0;
}

static int cmd_select(int argc, char *argv[])
{
	struct bt_conn *conn;
	bt_addr_le_t addr;
	int err;

	err = bt_addr_le_from_str(argv[1], argv[2], &addr);
	if (err) {
		printk("Invalid peer address (err %d)\n", err);
		return err;
	}

	conn = bt_conn_lookup_addr_le(BT_ID_DEFAULT, &addr);
	if (!conn) {
		printk("No matching connection found\n");
		return -ENOEXEC;
	}

	if (default_conn) {
		bt_conn_unref(default_conn);
	}

	default_conn = conn;

	return 0;
}

static const char *get_conn_type_str(u8_t type)
{
	switch (type) {
	case BT_CONN_TYPE_LE: return "LE";
	case BT_CONN_TYPE_BR: return "BR/EDR";
	case BT_CONN_TYPE_SCO: return "SCO";
	default: return "Invalid";
	}
}

static const char *get_conn_role_str(u8_t role)
{
	switch (role) {
	case BT_CONN_ROLE_MASTER: return "master";
	case BT_CONN_ROLE_SLAVE: return "slave";
	default: return "Invalid";
	}
}

static void print_le_addr(const char *desc, const bt_addr_le_t *addr)
{
	char addr_str[BT_ADDR_LE_STR_LEN];

	const char *addr_desc = bt_addr_le_is_identity(addr) ? "identity" :
				bt_addr_le_is_rpa(addr) ? "resolvable" :
				"non-resolvable";

	bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));

	printk("%s address: %s (%s)\n", desc, addr_str,
		    addr_desc);
}

static int cmd_info(int argc, char *argv[])
{
	struct bt_conn *conn = NULL;
	struct bt_conn_info info;
	bt_addr_le_t addr;
	int err;

	switch (argc) {
	case 1:
		if (default_conn) {
			conn = bt_conn_ref(default_conn);
		}
		break;
	case 2:
		addr.type = BT_ADDR_LE_PUBLIC;
		err = bt_addr_from_str(argv[1], &addr.a);
		if (err) {
			printk("Invalid peer address (err %d)\n",
				    err);
			return err;
		}
		conn = bt_conn_lookup_addr_le(selected_id, &addr);
		break;
	case 3:
		err = bt_addr_le_from_str(argv[1], argv[2], &addr);

		if (err) {
			printk("Invalid peer address (err %d)\n",
				    err);
			return err;
		}
		conn = bt_conn_lookup_addr_le(selected_id, &addr);
		break;
	}

	if (!conn) {
		printk("Not connected\n");
		return -ENOEXEC;
	}

	err = bt_conn_get_info(conn, &info);
	if (err) {
		printk("Failed to get info\n");
		goto done;
	}

	printk("Type: %s, Role: %s, Id: %u\n",
		    get_conn_type_str(info.type),
		    get_conn_role_str(info.role),
		    info.id);

	if (info.type == BT_CONN_TYPE_LE) {
		print_le_addr("Remote\n", info.le.dst);
		print_le_addr("Local\n", info.le.src);
		print_le_addr("Remote on-air\n", info.le.remote);
		print_le_addr("Local on-air\n", info.le.local);

		printk("Interval: 0x%04x (%u ms)\n",
			    info.le.interval, info.le.interval * 5 / 4);
		printk("Latency: 0x%04x (%u ms)\n",
			    info.le.latency, info.le.latency * 5 / 4);
		printk("Supervision timeout: 0x%04x (%d ms)\n",
			    info.le.timeout, info.le.timeout * 10);
	}

#if defined(CONFIG_BT_BREDR)
	if (info.type == BT_CONN_TYPE_BR) {
		char addr_str[BT_ADDR_STR_LEN];

		bt_addr_to_str(info.br.dst, addr_str, sizeof(addr_str));
		printk("Peer address %s\n", addr_str);
	}
#endif /* defined(CONFIG_BT_BREDR) */

done:
	bt_conn_unref(conn);

	return err;
}

static int cmd_conn_update(int argc, char *argv[])
{
	struct bt_le_conn_param param;
	int err;

	param.interval_min = strtoul(argv[1], NULL, 16);
	param.interval_max = strtoul(argv[2], NULL, 16);
	param.latency = strtoul(argv[3], NULL, 16);
	param.timeout = strtoul(argv[4], NULL, 16);

	err = bt_conn_le_param_update(default_conn, &param);
	if (err) {
		printk("conn update failed (err %d).\n", err);
	} else {
		printk("conn update initiated.\n");
	}

	return err;
}

#if defined(CONFIG_BT_USER_DATA_LEN_UPDATE)
static u16_t tx_time_calc(u8_t phy, u16_t max_len)
{
	/* Access address + header + payload + MIC + CRC */
	u16_t total_len = 4 + 2 + max_len + 4 + 3;

	switch (phy) {
	case BT_GAP_LE_PHY_1M:
		/* 1 byte preamble, 8 us per byte */
		return 8 * (1 + total_len);
	case BT_GAP_LE_PHY_2M:
		/* 2 byte preamble, 4 us per byte */
		return 4 * (2 + total_len);
	case BT_GAP_LE_PHY_CODED:
		/* S8: Preamble + CI + TERM1 + 64 us per byte + TERM2 */
		return 80 + 16 + 24 + 64 * (total_len) + 24;
	default:
		return 0;
	}
}

static int cmd_conn_data_len_update(int argc,
				    char *argv[])
{
	struct bt_conn_le_data_len_param param;
	int err;

	param.tx_max_len = strtoul(argv[1], NULL, 10);

	if (argc > 2) {
		param.tx_max_time = strtoul(argv[2], NULL, 10);
	} else {
		/* Assume 1M if not able to retrieve PHY */
		u8_t phy = BT_GAP_LE_PHY_1M;

#if defined(CONFIG_BT_USER_PHY_UPDATE)
		struct bt_conn_info info;

		err = bt_conn_get_info(default_conn, &info);
		if (!err) {
			phy = info.le.phy->tx_phy;
		}
#endif
		param.tx_max_time = tx_time_calc(phy, param.tx_max_len);
		printk("Calculated tx time: %d\n", param.tx_max_time);
	}



	err = bt_conn_le_data_len_update(default_conn, &param);
	if (err) {
		printk("data len update failed (err %d).\n", err);
	} else {
		printk("data len update initiated.\n");
	}

	return err;
}
#endif

#if defined(CONFIG_BT_USER_PHY_UPDATE)
static int cmd_conn_phy_update(int argc,
			       char *argv[])
{
	struct bt_conn_le_phy_param param;
	int err;

	param.pref_tx_phy = strtoul(argv[1], NULL, 16);
	if (argc > 2) {
		param.pref_rx_phy = strtoul(argv[2], NULL, 16);
	} else {
		param.pref_rx_phy = param.pref_tx_phy;
	}

	err = bt_conn_le_phy_update(default_conn, &param);
	if (err) {
		printk("PHY update failed (err %d).\n", err);
	} else {
		printk("PHY update initiated.\n");
	}

	return err;
}
#endif

#if defined(CONFIG_BT_CENTRAL)
static int cmd_chan_map(int argc, char *argv[])
{
	u8_t chan_map[5] = {};
	int err;

	if (hex2bin(argv[1], strlen(argv[1]), chan_map, 5) == 0) {
		printk("Invalid channel map\n");
		return -ENOEXEC;
	}
	sys_mem_swap(chan_map, 5);

	err = bt_le_set_chan_map(chan_map);
	if (err) {
		printk("Failed to set channel map (err %d)\n", err);
	} else {
		printk("Channel map set\n");
	}

	return err;
}
#endif /* CONFIG_BT_CENTRAL */

static int cmd_oob(int argc, char *argv[])
{
	int err;

	err = bt_le_oob_get_local(selected_id, &oob_local);
	if (err) {
		printk("OOB data failed\n");
		return err;
	}

	print_le_oob(&oob_local);

	return 0;
}

#if defined(CONFIG_BT_SMP) || defined(CONFIG_BT_BREDR)
static int cmd_oob_remote(int argc,
			     char *argv[])
{
	int err;
	bt_addr_le_t addr;

	err = bt_addr_le_from_str(argv[1], argv[2], &addr);
	if (err) {
		printk("Invalid peer address (err %d)\n", err);
		return err;
	}

	bt_addr_le_copy(&oob_remote.addr, &addr);

	if (argc == 5) {
		hex2bin(argv[3], strlen(argv[3]), oob_remote.le_sc_data.r,
			sizeof(oob_remote.le_sc_data.r));
		hex2bin(argv[4], strlen(argv[4]), oob_remote.le_sc_data.c,
			sizeof(oob_remote.le_sc_data.c));
		bt_set_oob_data_flag(true);
	} else {
		return -ENOEXEC;
	}

	return 0;
}

static int cmd_oob_clear(int argc, char *argv[])
{
	memset(&oob_remote, 0, sizeof(oob_remote));
	bt_set_oob_data_flag(false);

	return 0;
}
#endif /* CONFIG_BT_SMP || CONFIG_BT_BREDR) */

static int cmd_clear(int argc, char *argv[])
{
	bt_addr_le_t addr;
	int err;

	if (strcmp(argv[1], "all") == 0) {
		err = bt_unpair(selected_id, NULL);
		if (err) {
			printk("Failed to clear pairings (err %d)\n",
			      err);
			return err;
		} else {
			printk("Pairings successfully cleared\n");
		}

		return 0;
	}

	if (argc < 3) {
#if defined(CONFIG_BT_BREDR)
		addr.type = BT_ADDR_LE_PUBLIC;
		err = bt_addr_from_str(argv[1], &addr.a);
#else
		printk("Both address and address type needed\n");
		return -ENOEXEC;
#endif
	} else {
		err = bt_addr_le_from_str(argv[1], argv[2], &addr);
	}

	if (err) {
		printk("Invalid address\n");
		return err;
	}

	err = bt_unpair(selected_id, &addr);
	if (err) {
		printk("Failed to clear pairing (err %d)\n", err);
	} else {
		printk("Pairing successfully cleared\n");
	}

	return err;
}
#endif /* CONFIG_BT_CONN */

#if defined(CONFIG_BT_SMP) || defined(CONFIG_BT_BREDR)
static int cmd_security(int argc, char *argv[])
{
	int err, sec;
	struct bt_conn_info info;

	if (!default_conn || (bt_conn_get_info(default_conn, &info) < 0)) {
		printk("Not connected\n");
		return -ENOEXEC;
	}

	sec = *argv[1] - '0';

	if ((info.type == BT_CONN_TYPE_BR &&
	    (sec < BT_SECURITY_L0 || sec > BT_SECURITY_L3))) {
		printk("Invalid BR/EDR security level (%d)\n", sec);
		return -ENOEXEC;
	}

	if ((info.type == BT_CONN_TYPE_LE &&
	    (sec < BT_SECURITY_L1 || sec > BT_SECURITY_L4))) {
		printk("Invalid LE security level (%d)\n", sec);
		return -ENOEXEC;
	}

	if (argc > 2) {
		if (!strcmp(argv[2], "force-pair")) {
			sec |= BT_SECURITY_FORCE_PAIR;
		} else {
			return -ENOEXEC;
		}
	}

	err = bt_conn_set_security(default_conn, sec);
	if (err) {
		printk("Setting security failed (err %d)\n", err);
	}

	return err;
}

static int cmd_bondable(int argc, char *argv[])
{
	const char *bondable;

	bondable = argv[1];
	if (!strcmp(bondable, "on")) {
		bt_set_bondable(true);
	} else if (!strcmp(bondable, "off")) {
		bt_set_bondable(false);
	} else {
		return -EINVAL;
	}

	return 0;
}

static void bond_info(const struct bt_bond_info *info, void *user_data)
{
	char addr[BT_ADDR_LE_STR_LEN];
	int *bond_count = user_data;

	bt_addr_le_to_str(&info->addr, addr, sizeof(addr));
	printk("Remote Identity: %s\n", addr);
	(*bond_count)++;
}

static int cmd_bonds(int argc, char *argv[])
{
	int bond_count = 0;

	printk("Bonded devices:\n");
	bt_foreach_bond(selected_id, bond_info, &bond_count);
	printk("Total %d\n", bond_count);

	return 0;
}

static void connection_info(struct bt_conn *conn, void *user_data)
{
	char addr[BT_ADDR_LE_STR_LEN];
	int *conn_count = user_data;

	conn_addr_str(conn, addr, sizeof(addr));
	printk("Remote Identity: %s\n", addr);
	(*conn_count)++;
}

static int cmd_connections(int argc, char *argv[])
{
	int conn_count = 0;

	printk("Connected devices:\n");
	bt_conn_foreach(BT_CONN_TYPE_ALL, connection_info, &conn_count);
	printk("Total %d\n", conn_count);

	return 0;
}

static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];
	char passkey_str[7];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	snprintk(passkey_str, 7, "%06u", passkey);

	printk("Passkey for %s: %s\n", addr, passkey_str);
}

static void auth_passkey_confirm(struct bt_conn *conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];
	char passkey_str[7];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	snprintk(passkey_str, 7, "%06u", passkey);

	printk("Confirm passkey for %s: %s\n", addr, passkey_str);
}

static void auth_passkey_entry(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Enter passkey for %s\n", addr);
}

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	conn_addr_str(conn, addr, sizeof(addr));

	printk("Pairing cancelled: %s\n", addr);

	/* clear connection reference for sec mode 3 pairing */
	if (pairing_conn) {
		bt_conn_unref(pairing_conn);
		pairing_conn = NULL;
	}
}

static void auth_pairing_confirm(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Confirm pairing for %s\n", addr);
}

#if !defined(CONFIG_BT_SMP_OOB_LEGACY_PAIR_ONLY)
static const char *oob_config_str(int oob_config)
{
	switch (oob_config) {
	case BT_CONN_OOB_LOCAL_ONLY:
		return "Local";
	case BT_CONN_OOB_REMOTE_ONLY:
		return "Remote";
	case BT_CONN_OOB_BOTH_PEERS:
		return "Local and Remote";
	case BT_CONN_OOB_NO_DATA:
	default:
		return "no";
	}
}
#endif /* !defined(CONFIG_BT_SMP_OOB_LEGACY_PAIR_ONLY) */

static void auth_pairing_oob_data_request(struct bt_conn *conn,
					  struct bt_conn_oob_info *oob_info)
{
	char addr[BT_ADDR_LE_STR_LEN];
	struct bt_conn_info info;
	int err;

	err = bt_conn_get_info(conn, &info);
	if (err) {
		return;
	}

#if !defined(CONFIG_BT_SMP_OOB_LEGACY_PAIR_ONLY)
	if (oob_info->type == BT_CONN_OOB_LE_SC) {
		struct bt_le_oob_sc_data *oobd_local =
			oob_info->lesc.oob_config != BT_CONN_OOB_REMOTE_ONLY
						  ? &oob_local.le_sc_data
						  : NULL;
		struct bt_le_oob_sc_data *oobd_remote =
			oob_info->lesc.oob_config != BT_CONN_OOB_LOCAL_ONLY
						  ? &oob_remote.le_sc_data
						  : NULL;

		if (oobd_remote &&
		    bt_addr_le_cmp(info.le.remote, &oob_remote.addr)) {
			bt_addr_le_to_str(info.le.remote, addr, sizeof(addr));
			printk("No OOB data available for remote %s\n",
			       addr);
			bt_conn_auth_cancel(conn);
			return;
		}

		if (oobd_local &&
		    bt_addr_le_cmp(info.le.local, &oob_local.addr)) {
			bt_addr_le_to_str(info.le.local, addr, sizeof(addr));
			printk("No OOB data available for local %s\n",
			       addr);
			bt_conn_auth_cancel(conn);
			return;
		}

		bt_le_oob_set_sc_data(conn, oobd_local, oobd_remote);

		bt_addr_le_to_str(info.le.dst, addr, sizeof(addr));
		printk("Set %s OOB SC data for %s\n",
			    oob_config_str(oob_info->lesc.oob_config), addr);
		return;
	}
#endif /* CONFIG_BT_SMP_OOB_LEGACY_PAIR_ONLY */

	bt_addr_le_to_str(info.le.dst, addr, sizeof(addr));
	printk("Legacy OOB TK requested from remote %s\n", addr);
}

static void auth_pairing_complete(struct bt_conn *conn, bool bonded)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("%s with %s\n", bonded ? "Bonded" : "Paired",
		    addr);
}

static void auth_pairing_failed(struct bt_conn *conn,
				enum bt_security_err reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	printk("Pairing failed with %s reason %d\n", addr,
		    reason);
}

#if defined(CONFIG_BT_BREDR)
static void auth_pincode_entry(struct bt_conn *conn, bool highsec)
{
	char addr[BT_ADDR_STR_LEN];
	struct bt_conn_info info;

	if (bt_conn_get_info(conn, &info) < 0) {
		return;
	}

	if (info.type != BT_CONN_TYPE_BR) {
		return;
	}

	bt_addr_to_str(info.br.dst, addr, sizeof(addr));

	if (highsec) {
		printk("Enter 16 digits wide PIN code for %s\n",
		       addr);
	} else {
		printk("Enter PIN code for %s\n", addr);
	}

	/*
	 * Save connection info since in security mode 3 (link level enforced
	 * security) PIN request callback is called before connected callback
	 */
	if (!default_conn && !pairing_conn) {
		pairing_conn = bt_conn_ref(conn);
	}
}
#endif

#if defined(CONFIG_BT_SMP_APP_PAIRING_ACCEPT)
enum bt_security_err pairing_accept(
	struct bt_conn *conn, const struct bt_conn_pairing_feat *const feat)
{
	printk("Remote pairing features: "
	       "IO: 0x%02x, OOB: %d, AUTH: 0x%02x, Key: %d, "
	       "Init Kdist: 0x%02x, Resp Kdist: 0x%02x\n",
	       feat->io_capability, feat->oob_data_flag,
	       feat->auth_req, feat->max_enc_key_size,
	       feat->init_key_dist, feat->resp_key_dist);

	return BT_SECURITY_ERR_SUCCESS;
}
#endif /* CONFIG_BT_SMP_APP_PAIRING_ACCEPT */

static struct bt_conn_auth_cb auth_cb_display = {
	.passkey_display = auth_passkey_display,
	.passkey_entry = NULL,
	.passkey_confirm = NULL,
#if defined(CONFIG_BT_BREDR)
	.pincode_entry = auth_pincode_entry,
#endif
	.oob_data_request = NULL,
	.cancel = auth_cancel,
	.pairing_confirm = auth_pairing_confirm,
	.pairing_failed = auth_pairing_failed,
	.pairing_complete = auth_pairing_complete,
#if defined(CONFIG_BT_SMP_APP_PAIRING_ACCEPT)
	.pairing_accept = pairing_accept,
#endif
};

static struct bt_conn_auth_cb auth_cb_display_yes_no = {
	.passkey_display = auth_passkey_display,
	.passkey_entry = NULL,
	.passkey_confirm = auth_passkey_confirm,
#if defined(CONFIG_BT_BREDR)
	.pincode_entry = auth_pincode_entry,
#endif
	.oob_data_request = NULL,
	.cancel = auth_cancel,
	.pairing_confirm = auth_pairing_confirm,
	.pairing_failed = auth_pairing_failed,
	.pairing_complete = auth_pairing_complete,
#if defined(CONFIG_BT_SMP_APP_PAIRING_ACCEPT)
	.pairing_accept = pairing_accept,
#endif
};

static struct bt_conn_auth_cb auth_cb_input = {
	.passkey_display = NULL,
	.passkey_entry = auth_passkey_entry,
	.passkey_confirm = NULL,
#if defined(CONFIG_BT_BREDR)
	.pincode_entry = auth_pincode_entry,
#endif
	.oob_data_request = NULL,
	.cancel = auth_cancel,
	.pairing_confirm = auth_pairing_confirm,
	.pairing_failed = auth_pairing_failed,
	.pairing_complete = auth_pairing_complete,
#if defined(CONFIG_BT_SMP_APP_PAIRING_ACCEPT)
	.pairing_accept = pairing_accept,
#endif
};

static struct bt_conn_auth_cb auth_cb_confirm = {
#if defined(CONFIG_BT_BREDR)
	.pincode_entry = auth_pincode_entry,
#endif
	.oob_data_request = NULL,
	.cancel = auth_cancel,
	.pairing_confirm = auth_pairing_confirm,
	.pairing_failed = auth_pairing_failed,
	.pairing_complete = auth_pairing_complete,
#if defined(CONFIG_BT_SMP_APP_PAIRING_ACCEPT)
	.pairing_accept = pairing_accept,
#endif
};

static struct bt_conn_auth_cb auth_cb_all = {
	.passkey_display = auth_passkey_display,
	.passkey_entry = auth_passkey_entry,
	.passkey_confirm = auth_passkey_confirm,
#if defined(CONFIG_BT_BREDR)
	.pincode_entry = auth_pincode_entry,
#endif
	.oob_data_request = auth_pairing_oob_data_request,
	.cancel = auth_cancel,
	.pairing_confirm = auth_pairing_confirm,
	.pairing_failed = auth_pairing_failed,
	.pairing_complete = auth_pairing_complete,
#if defined(CONFIG_BT_SMP_APP_PAIRING_ACCEPT)
	.pairing_accept = pairing_accept,
#endif
};

static struct bt_conn_auth_cb auth_cb_oob = {
	.passkey_display = NULL,
	.passkey_entry = NULL,
	.passkey_confirm = NULL,
#if defined(CONFIG_BT_BREDR)
	.pincode_entry = NULL,
#endif
	.oob_data_request = auth_pairing_oob_data_request,
	.cancel = auth_cancel,
	.pairing_confirm = NULL,
	.pairing_failed = auth_pairing_failed,
	.pairing_complete = auth_pairing_complete,
#if defined(CONFIG_BT_SMP_APP_PAIRING_ACCEPT)
	.pairing_accept = pairing_accept,
#endif
};


static int cmd_auth(int argc, char *argv[])
{
	int err;

	if (!strcmp(argv[1], "all")) {
		err = bt_conn_auth_cb_register(&auth_cb_all);
	} else if (!strcmp(argv[1], "input")) {
		err = bt_conn_auth_cb_register(&auth_cb_input);
	} else if (!strcmp(argv[1], "display")) {
		err = bt_conn_auth_cb_register(&auth_cb_display);
	} else if (!strcmp(argv[1], "yesno")) {
		err = bt_conn_auth_cb_register(&auth_cb_display_yes_no);
	} else if (!strcmp(argv[1], "confirm")) {
		err = bt_conn_auth_cb_register(&auth_cb_confirm);
	} else if (!strcmp(argv[1], "oob")) {
		err = bt_conn_auth_cb_register(&auth_cb_oob);
	} else if (!strcmp(argv[1], "none")) {
		err = bt_conn_auth_cb_register(NULL);
	} else {
		return -EINVAL;
	}

	return err;
}

static int cmd_auth_cancel(int argc, char *argv[])
{
	struct bt_conn *conn;

	if (default_conn) {
		conn = default_conn;
	} else if (pairing_conn) {
		conn = pairing_conn;
	} else {
		conn = NULL;
	}

	if (!conn) {
		printk("Not connected\n");
		return -ENOEXEC;
	}

	bt_conn_auth_cancel(conn);

	return 0;
}

static int cmd_auth_passkey_confirm(int argc, char *argv[])
{
	if (!default_conn) {
		printk("Not connected\n");
		return -ENOEXEC;
	}

	bt_conn_auth_passkey_confirm(default_conn);
	return 0;
}

static int cmd_auth_pairing_confirm(int argc, char *argv[])
{
	if (!default_conn) {
		printk("Not connected\n");
		return -ENOEXEC;
	}

	bt_conn_auth_pairing_confirm(default_conn);
	return 0;
}

#if defined(CONFIG_BT_WHITELIST)
static int cmd_wl_add(int argc, char *argv[])
{
	bt_addr_le_t addr;
	int err;

	err = bt_addr_le_from_str(argv[1], argv[2], &addr);
	if (err) {
		printk("Invalid peer address (err %d)\n", err);
		return err;
	}

	err = bt_le_whitelist_add(&addr);
	if (err) {
		printk("Add to whitelist failed (err %d)\n", err);
		return err;
	}

	return 0;
}

static int cmd_wl_rem(int argc, char *argv[])
{
	bt_addr_le_t addr;
	int err;

	err = bt_addr_le_from_str(argv[1], argv[2], &addr);
	if (err) {
		printk("Invalid peer address (err %d)\n", err);
		return err;
	}

	err = bt_le_whitelist_rem(&addr);
	if (err) {
		printk("Remove from whitelist failed (err %d)\n",
			    err);
		return err;
	}
	return 0;
}

static int cmd_wl_clear(int argc, char *argv[])
{
	int err;

	err = bt_le_whitelist_clear();
	if (err) {
		printk("Clearing whitelist failed (err %d)\n", err);
		return err;
	}

	return 0;
}

#if defined(CONFIG_BT_CENTRAL)
static int cmd_wl_connect(int argc, char *argv[])
{
	int err;
	const char *action = argv[1];
	u32_t options = 0;

#if defined(CONFIG_BT_EXT_ADV)
	for (size_t argn = 2; argn < argc; argn++) {
		const char *arg = argv[argn];

		if (!strcmp(arg, "coded")) {
			options |= BT_CONN_LE_OPT_CODED;
		} else if (!strcmp(arg, "no-1m")) {
			options |= BT_CONN_LE_OPT_NO_1M;
		} else {
			return -EINVAL;
		}
	}
#endif /* defined(CONFIG_BT_EXT_ADV) */
	struct bt_conn_le_create_param *create_params =
		BT_CONN_LE_CREATE_PARAM(options,
					BT_GAP_SCAN_FAST_INTERVAL,
					BT_GAP_SCAN_FAST_WINDOW);

	if (!strcmp(action, "on")) {
		err = bt_conn_le_create_auto(create_params,
					     BT_LE_CONN_PARAM_DEFAULT);
		if (err) {
			printk("Auto connect failed (err %d)\n", err);
			return err;
		}
	} else if (!strcmp(action, "off")) {
		err = bt_conn_create_auto_stop();
		if (err) {
			printk("Auto connect stop failed (err %d)\n",
			       err);
		}
		return err;
	}

	return 0;
}
#endif /* CONFIG_BT_CENTRAL */
#endif /* defined(CONFIG_BT_WHITELIST) */

#if defined(CONFIG_BT_FIXED_PASSKEY)
static int cmd_fixed_passkey(int argc, char *argv[])
{
	unsigned int passkey;
	int err;

	if (argc < 2) {
		bt_passkey_set(BT_PASSKEY_INVALID);
		printk("Fixed passkey cleared\n");
		return 0;
	}

	passkey = atoi(argv[1]);
	if (passkey > 999999) {
		printk("Passkey should be between 0-999999\n");
		return -ENOEXEC;
	}

	err = bt_passkey_set(passkey);
	if (err) {
		printk("Setting fixed passkey failed (err %d)\n",
		       err);
	}

	return err;
}
#endif

static int cmd_auth_passkey(int argc, char *argv[])
{
	unsigned int passkey;
	int err;

	if (!default_conn) {
		printk("Not connected\n");
		return -ENOEXEC;
	}

	passkey = atoi(argv[1]);
	if (passkey > 999999) {
		printk("Passkey should be between 0-999999\n");
		return -EINVAL;
	}

	err = bt_conn_auth_passkey_entry(default_conn, passkey);
	if (err) {
		printk("Failed to set passkey (%d)\n", err);
		return err;
	}

	return 0;
}

#if !defined(CONFIG_BT_SMP_SC_PAIR_ONLY)
static int cmd_auth_oob_tk(int argc, char *argv[])
{
	u8_t tk[16];
	size_t len;
	int err;

	len = hex2bin(argv[1], strlen(argv[1]), tk, sizeof(tk));
	if (len != sizeof(tk)) {
		printk("TK should be 16 bytes\n");
		return -EINVAL;
	}

	err = bt_le_oob_set_legacy_tk(default_conn, tk);
	if (err) {
		printk("Failed to set TK (%d)\n", err);
		return err;
	}

	return 0;
}
#endif /* !defined(CONFIG_BT_SMP_SC_PAIR_ONLY) */
#endif /* CONFIG_BT_SMP) || CONFIG_BT_BREDR */


#define HELP_NONE "[none]"
#define HELP_ADDR_LE "<address: XX:XX:XX:XX:XX:XX> <type: (public|random)>"

#if defined(CONFIG_BT_EXT_ADV)
#define EXT_ADV_SCAN_OPT " [coded] [no-1m]"
#define EXT_ADV_PARAM "<type: conn-scan conn-nscan, nconn-scan nconn-nscan> " \
		      "[ext-adv] [no-2m] [coded] "                            \
		      "[whitelist: wl, wl-scan, wl-conn] [identity] [name]"   \
		      "[directed "HELP_ADDR_LE"] [mode: low] "
#else
#define EXT_ADV_SCAN_OPT ""
#endif /* defined(CONFIG_BT_EXT_ADV) */

static const struct shell_cmd bt_commands[] = {
	{ "init", cmd_init, HELP_NONE },
#if defined(CONFIG_BT_HCI)
	{ "hci-cmd", cmd_hci_cmd, "<ogf> <ocf> [data]" },
#endif
	{ "id-create", cmd_id_create, "[addr]" },
	{ "id-reset", cmd_id_reset, "<id> [addr]" },
	{ "id-delete", cmd_id_delete, "<id>" },
	{ "id-show", cmd_id_show, HELP_NONE },
	{ "id-select", cmd_id_select, "<id>" },
	{ "name", cmd_name, "[name]" },
#if defined(CONFIG_BT_OBSERVER)
	{ "scan", cmd_scan,
		      "<value: on, passive, off> [filter: dups, nodups] [wl]"
		      EXT_ADV_SCAN_OPT },
#endif /* CONFIG_BT_OBSERVER */
#if defined(CONFIG_BT_BROADCASTER)
	{ "advertise", cmd_advertise,
		      "<type: off, on, scan, nconn> [mode: discov, non_discov] "
		      "[whitelist: wl, wl-scan, wl-conn] [identity] [no-name]"},
#if defined(CONFIG_BT_PERIPHERAL)
	{ "directed-adv", cmd_directed_adv, HELP_ADDR_LE " [mode: low]" },
#endif /* CONFIG_BT_PERIPHERAL */
#if defined(CONFIG_BT_EXT_ADV)
	{ "adv-create", cmd_adv_create, EXT_ADV_PARAM },
	{ "adv-param", cmd_adv_param, EXT_ADV_PARAM },
	{ "adv-data", cmd_adv_data, "<data> [scan-response <data>] "
				      "<type: discov, name, hex>" },
	{ "adv-start", cmd_adv_start, "[timeout] [num_events]" },
	{ "adv-stop", cmd_adv_stop, "" },
	{ "adv-delete", cmd_adv_delete, "" },
	{ "adv-select", cmd_adv_select, "[adv]" },
	{ "adv-oob", cmd_adv_oob, HELP_NONE },
	{ "adv-info", cmd_adv_info, HELP_NONE },
#endif
#endif /* CONFIG_BT_BROADCASTER */
#if defined(CONFIG_BT_CONN)
#if defined(CONFIG_BT_CENTRAL)
	{ "connect", cmd_connect_le, HELP_ADDR_LE EXT_ADV_SCAN_OPT },
#if !defined(CONFIG_BT_WHITELIST)
	{ "auto-conn", cmd_auto_conn, HELP_ADDR_LE },
#endif /* !defined(CONFIG_BT_WHITELIST) */
#endif /* CONFIG_BT_CENTRAL */
	{ "disconnect", cmd_disconnect, HELP_NONE },
	{ "select", cmd_select, HELP_ADDR_LE },
	{ "info", cmd_info, HELP_ADDR_LE },
	{ "conn-update", cmd_conn_update, "<min> <max> <latency> <timeout>" },
#if defined(CONFIG_BT_USER_DATA_LEN_UPDATE)
	{ "data-len-update", cmd_conn_data_len_update, "<tx_max_len> [tx_max_time]" },
#endif
#if defined(CONFIG_BT_USER_PHY_UPDATE)
	{ "phy-update", cmd_conn_phy_update, "<tx_phy> [rx_phy]" },
#endif
#if defined(CONFIG_BT_CENTRAL)
	{ "channel-map", cmd_chan_map, "<channel-map: XXXXXXXXXX> (36-0)" },
#endif /* CONFIG_BT_CENTRAL */
	{ "oob", cmd_oob, NULL },
	{ "clear", cmd_clear, "<remote: addr, all>" },
#if defined(CONFIG_BT_SMP) || defined(CONFIG_BT_BREDR)
	{ "security", cmd_security, "<security level BR/EDR: 0 - 3, "
				      "LE: 1 - 4> [force-pair]" },
	{ "bondable", cmd_bondable, "<bondable: on, off>" },
	{ "bonds", cmd_bonds, HELP_NONE },
	{ "connections", cmd_connections, HELP_NONE },
	{ "auth", cmd_auth,
		      "<method: all, input, display, yesno, confirm, "
		      "oob, none>" },
	{ "auth-cancel", cmd_auth_cancel, HELP_NONE },
	{ "auth-passkey", cmd_auth_passkey, "<passkey>" },
	{ "auth-passkey-confirm", cmd_auth_passkey_confirm, HELP_NONE },
	{ "auth-pairing-confirm", cmd_auth_pairing_confirm, HELP_NONE },
#if !defined(CONFIG_BT_SMP_SC_PAIR_ONLY)
	{ "auth-oob-tk", cmd_auth_oob_tk, "<tk>" },
#endif /* !defined(CONFIG_BT_SMP_SC_PAIR_ONLY) */
	{ "oob-remote", cmd_oob_remote,
		      HELP_ADDR_LE" <oob rand> <oob confirm>" },
	{ "oob-clear", cmd_oob_clear, HELP_NONE },
#if defined(CONFIG_BT_WHITELIST)
	{ "wl-add", cmd_wl_add, HELP_ADDR_LE },
	{ "wl-rem", cmd_wl_rem, HELP_ADDR_LE },
	{ "wl-clear", cmd_wl_clear, HELP_NONE },

#if defined(CONFIG_BT_CENTRAL)
	{ "wl-connect", cmd_wl_connect, "<on, off>" EXT_ADV_SCAN_OPT },
#endif /* CONFIG_BT_CENTRAL */
#endif /* defined(CONFIG_BT_WHITELIST) */
#if defined(CONFIG_BT_FIXED_PASSKEY)
	{ "fixed-passkey", cmd_fixed_passkey, "[passkey]" },
#endif
#endif /* CONFIG_BT_SMP || CONFIG_BT_BREDR) */
#endif /* CONFIG_BT_CONN */
#if defined(CONFIG_BT_HCI_MESH_EXT)
	{ "mesh_adv", cmd_mesh_adv, "<on, off>" },
#endif /* CONFIG_BT_HCI_MESH_EXT */
#if defined(CONFIG_BT_LL_SW_SPLIT)
#if defined(CONFIG_BT_CTLR_ADV_EXT)
#if defined(CONFIG_BT_BROADCASTER)
	{ "advx", cmd_advx, "<on off> [coded] [anon] [txp]" },
#endif /* CONFIG_BT_BROADCASTER */
#if defined(CONFIG_BT_OBSERVER)
	{ "scanx", cmd_scanx, "<on passive off> [coded]" },
#endif /* CONFIG_BT_OBSERVER */
#endif /* CONFIG_BT_CTLR_ADV_EXT */
#if defined(CONFIG_BT_CTLR_DTM)
	{ "test_tx", cmd_test_tx, "<chan> <len> <type> <phy>" },
	{ "test_rx", cmd_test_rx, "<chan> <phy> <mod_idx>" },
	{ "test_end", cmd_test_end, HELP_NONE },
#endif /* CONFIG_BT_CTLR_ADV_EXT */
#endif /* defined(CONFIG_BT_LL_SW_SPLIT) */
#if defined(CONFIG_BT_LL_SW_SPLIT)
	{ "ull_reset", cmd_ull_reset, HELP_NONE },
#endif /* CONFIG_BT_LL_SW_SPLIT */
	{ NULL, NULL, NULL }
};

SHELL_REGISTER_WITH_PROMPT("bt", bt_commands, current_prompt);
