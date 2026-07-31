/** @file
 * @brief Bluetooth GATT shell functions
 *
 */

/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <zephyr/types.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <misc/byteorder.h>
#include <zephyr.h>

#include <acts_bluetooth/bluetooth.h>
#include <acts_bluetooth/conn.h>
#include <acts_bluetooth/gatt.h>

#include <shell/shell.h>

#include "bt.h"

#if defined(CONFIG_BT_EATT)
extern int bt_eatt_connect(struct bt_conn *conn, u8_t num_channels);
extern int bt_eatt_disconnect(struct bt_conn *conn);
#endif /* CONFIG_BT_EATT */

#define CHAR_SIZE_MAX           512

static void hexdump(const u8_t *data, size_t len)
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

#if defined(CONFIG_BT_GATT_CLIENT)
static void exchange_func(struct bt_conn *conn, u8_t err,
			  struct bt_gatt_exchange_params *params)
{
	printk("Exchange %s\n", err == 0U ? "successful" :
	       "failed");

	(void)memset(params, 0, sizeof(*params));
}

static struct bt_gatt_exchange_params exchange_params;

static int cmd_exchange_mtu(int argc, char *argv[])
{
	int err;

	if (!default_conn) {
		printk("Not connected\n");
		return -ENOEXEC;
	}

	if (exchange_params.func) {
		printk("MTU Exchange ongoing\n");
		return -ENOEXEC;
	}

	exchange_params.func = exchange_func;

	err = bt_gatt_exchange_mtu(default_conn, &exchange_params);
	if (err) {
		printk("Exchange failed (err %d)\n", err);
	} else {
		printk("Exchange pending\n");
	}

	return err;
}

static struct bt_gatt_discover_params discover_params;
static struct bt_uuid_16 uuid = BT_UUID_INIT_16(0);

static void print_chrc_props(u8_t properties)
{
	printk("Properties: ");

	if (properties & BT_GATT_CHRC_BROADCAST) {
		printk("[bcast]");
	}

	if (properties & BT_GATT_CHRC_READ) {
		printk("[read]");
	}

	if (properties & BT_GATT_CHRC_WRITE) {
		printk("[write]");
	}

	if (properties & BT_GATT_CHRC_WRITE_WITHOUT_RESP) {
		printk("[write w/w rsp]");
	}

	if (properties & BT_GATT_CHRC_NOTIFY) {
		printk("[notify]");
	}

	if (properties & BT_GATT_CHRC_INDICATE) {
		printk("[indicate]");
	}

	if (properties & BT_GATT_CHRC_AUTH) {
		printk("[auth]");
	}

	if (properties & BT_GATT_CHRC_EXT_PROP) {
		printk("[ext prop]");
	}

	printk("\n");
}

static u8_t discover_func(struct bt_conn *conn,
			     const struct bt_gatt_attr *attr,
			     struct bt_gatt_discover_params *params)
{
	struct bt_gatt_service_val *gatt_service;
	struct bt_gatt_chrc *gatt_chrc;
	struct bt_gatt_include *gatt_include;
	char str[BT_UUID_STR_LEN];

	if (!attr) {
		printk("Discover complete\n");
		(void)memset(params, 0, sizeof(*params));
		return BT_GATT_ITER_STOP;
	}

	switch (params->type) {
	case BT_GATT_DISCOVER_SECONDARY:
	case BT_GATT_DISCOVER_PRIMARY:
		gatt_service = attr->user_data;
		bt_uuid_to_str(gatt_service->uuid, str, sizeof(str));
		printk("Service %s found: start handle %x, "
		       "end_handle %x\n", str, attr->handle,
		       gatt_service->end_handle);
		break;
	case BT_GATT_DISCOVER_CHARACTERISTIC:
		gatt_chrc = attr->user_data;
		bt_uuid_to_str(gatt_chrc->uuid, str, sizeof(str));
		printk("Characteristic %s found: handle %x\n",
		       str, attr->handle);
		print_chrc_props(gatt_chrc->properties);
		break;
	case BT_GATT_DISCOVER_INCLUDE:
		gatt_include = attr->user_data;
		bt_uuid_to_str(gatt_include->uuid, str, sizeof(str));
		printk("Include %s found: handle %x, start %x, "
			    "end %x\n", str, attr->handle,
			    gatt_include->start_handle,
			    gatt_include->end_handle);
		break;
	default:
		bt_uuid_to_str(attr->uuid, str, sizeof(str));
		printk("Descriptor %s found: handle %x\n", str,
			    attr->handle);
		break;
	}

	return BT_GATT_ITER_CONTINUE;
}

static int cmd_discover(int argc, char *argv[])
{
	int err;

	if (!default_conn) {
		printk("Not connected\n");
		return -ENOEXEC;
	}

	if (discover_params.func) {
		printk("Discover ongoing\n");
		return -ENOEXEC;
	}

	discover_params.func = discover_func;
	discover_params.start_handle = 0x0001;
	discover_params.end_handle = 0xffff;

	if (argc > 1) {
		/* Only set the UUID if the value is valid (non zero) */
		uuid.val = strtoul(argv[1], NULL, 16);
		if (uuid.val) {
			printk("uuid type: 0x%x, val: 0x%x\n", uuid.uuid.type, uuid.val);
			discover_params.uuid = &uuid.uuid;
		}
	}

	if (argc > 2) {
		discover_params.start_handle = strtoul(argv[2], NULL, 16);
		if (argc > 3) {
			discover_params.end_handle = strtoul(argv[3], NULL, 16);
		}
	}

	if (!strcmp(argv[0], "discover")) {
		discover_params.type = BT_GATT_DISCOVER_ATTRIBUTE;
	} else if (!strcmp(argv[0], "discover-secondary")) {
		discover_params.type = BT_GATT_DISCOVER_SECONDARY;
	} else if (!strcmp(argv[0], "discover-include")) {
		discover_params.type = BT_GATT_DISCOVER_INCLUDE;
	} else if (!strcmp(argv[0], "discover-characteristic")) {
		discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
	} else if (!strcmp(argv[0], "discover-descriptor")) {
		discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;
	} else {
		discover_params.type = BT_GATT_DISCOVER_PRIMARY;
	}

	err = bt_gatt_discover(default_conn, &discover_params);
	if (err) {
		printk("Discover failed (err %d)\n", err);
	} else {
		printk("Discover pending\n");
	}

	return err;
}

static struct bt_gatt_read_params read_params;

static u8_t read_func(struct bt_conn *conn, u8_t err,
			 struct bt_gatt_read_params *params,
			 const void *data, u16_t length)
{
	printk("Read complete: err 0x%02x length %u\n", err, length);

	if (!data) {
		(void)memset(params, 0, sizeof(*params));
		return BT_GATT_ITER_STOP;
	}

	return BT_GATT_ITER_CONTINUE;
}

static int cmd_read(int argc, char *argv[])
{
	int err;

	if (!default_conn) {
		printk("Not connected\n");
		return -ENOEXEC;
	}

	if (read_params.func) {
		printk("Read ongoing\n");
		return -ENOEXEC;
	}

	read_params.func = read_func;
	read_params.handle_count = 1;
	read_params.single.handle = strtoul(argv[1], NULL, 16);
	read_params.single.offset = 0U;

	if (argc > 2) {
		read_params.single.offset = strtoul(argv[2], NULL, 16);
	}

	err = bt_gatt_read(default_conn, &read_params);
	if (err) {
		printk("Read failed (err %d)\n", err);
	} else {
		printk("Read pending\n");
	}

	return err;
}

static int cmd_mread(int argc, char *argv[])
{
	u16_t h[8];
	size_t i;
	int err;

	if (!default_conn) {
		printk("Not connected\n");
		return -ENOEXEC;
	}

	if (read_params.func) {
		printk("Read ongoing\n");
		return -ENOEXEC;
	}

	if ((argc - 1) >  ARRAY_SIZE(h)) {
		printk("Enter max %lu handle items to read\n",
			    ARRAY_SIZE(h));
		return -EINVAL;
	}

	for (i = 0; i < argc - 1; i++) {
		h[i] = strtoul(argv[i + 1], NULL, 16);
	}

	read_params.func = read_func;
	read_params.handle_count = i;
	read_params.handles = h; /* not used in read func */

	err = bt_gatt_read(default_conn, &read_params);
	if (err) {
		printk("GATT multiple read request failed (err %d)\n",
			    err);
	}

	return err;
}

static int cmd_read_uuid(int argc, char *argv[])
{
	int err;

	if (!default_conn) {
		printk("Not connected\n");
		return -ENOEXEC;
	}

	if (read_params.func) {
		printk("Read ongoing\n");
		return -ENOEXEC;
	}

	read_params.func = read_func;
	read_params.handle_count = 0;
	read_params.by_uuid.start_handle = 0x0001;
	read_params.by_uuid.end_handle = 0xffff;

	if (argc > 1) {
		uuid.val = strtoul(argv[1], NULL, 16);
		if (uuid.val) {
			read_params.by_uuid.uuid = &uuid.uuid;
		}
	}

	if (argc > 2) {
		read_params.by_uuid.start_handle = strtoul(argv[2], NULL, 16);
		if (argc > 3) {
			read_params.by_uuid.end_handle = strtoul(argv[3],
								 NULL, 16);
		}
	}

	err = bt_gatt_read(default_conn, &read_params);
	if (err) {
		printk("Read failed (err %d)\n", err);
	} else {
		printk("Read pending\n");
	}

	return err;
}

static struct bt_gatt_write_params write_params;
static u8_t gatt_write_buf[CHAR_SIZE_MAX];

static void write_func(struct bt_conn *conn, u8_t err,
		       struct bt_gatt_write_params *params)
{
	printk("Write complete: err 0x%02x\n", err);

	(void)memset(&write_params, 0, sizeof(write_params));
}

static int cmd_write(int argc, char *argv[])
{
	int err;
	u16_t handle, offset;

	if (!default_conn) {
		printk("Not connected\n");
		return -ENOEXEC;
	}

	if (write_params.func) {
		printk("Write ongoing\n");
		return -ENOEXEC;
	}

	handle = strtoul(argv[1], NULL, 16);
	offset = strtoul(argv[2], NULL, 16);

	gatt_write_buf[0] = strtoul(argv[3], NULL, 16);
	write_params.data = gatt_write_buf;
	write_params.length = 1U;
	write_params.handle = handle;
	write_params.offset = offset;
	write_params.func = write_func;

	if (argc == 5) {
		size_t len, i;

		len = min(strtoul(argv[4], NULL, 16), sizeof(gatt_write_buf));

		for (i = 1; i < len; i++) {
			gatt_write_buf[i] = gatt_write_buf[0];
		}

		write_params.length = len;
	}

	err = bt_gatt_write(default_conn, &write_params);
	if (err) {
		printk("Write failed (err %d)\n", err);
	} else {
		printk("Write pending\n");
	}

	return err;
}

static void write_without_rsp_cb(struct bt_conn *conn, void *user_data)
{
	printk("Write transmission complete\n");
}

static int cmd_write_without_rsp(int argc, char *argv[])
{
	u16_t handle;
	u16_t repeat;
	int err;
	u16_t len;
	bool sign;
	bt_gatt_complete_func_t func = NULL;

	if (!default_conn) {
		printk("Not connected\n");
		return -ENOEXEC;
	}

	sign = !strcmp(argv[0], "signed-write");
	if (!sign) {
		if (!strcmp(argv[0], "write-without-response-cb")) {
			func = write_without_rsp_cb;
		}
	}

	handle = strtoul(argv[1], NULL, 16);
	gatt_write_buf[0] = strtoul(argv[2], NULL, 16);
	len = 1U;

	if (argc > 3) {
		int i;

		len = min(strtoul(argv[3], NULL, 16), sizeof(gatt_write_buf));

		for (i = 1; i < len; i++) {
			gatt_write_buf[i] = gatt_write_buf[0];
		}
	}

	repeat = 0U;

	if (argc > 4) {
		repeat = strtoul(argv[4], NULL, 16);
	}

	if (!repeat) {
		repeat = 1U;
	}

	while (repeat--) {
		err = bt_gatt_write_without_response_cb(default_conn, handle,
							gatt_write_buf, len,
							sign, func, NULL);
		if (err) {
			break;
		}
	}

	printk("Write Complete (err %d)\n", err);
	return err;
}

static struct bt_gatt_subscribe_params subscribe_params;

static u8_t notify_func(struct bt_conn *conn,
			struct bt_gatt_subscribe_params *params,
			const void *data, u16_t length)
{
	if (!data) {
		printk("Unsubscribed\n");
		params->value_handle = 0U;
		return BT_GATT_ITER_STOP;
	}

	printk("Notification: data %p length %u\n", data, length);

	return BT_GATT_ITER_CONTINUE;
}

static int cmd_subscribe(int argc, char *argv[])
{
	int err;

	if (subscribe_params.value_handle) {
		printk("Cannot subscribe: subscription to %x "
			    "already exists\n", subscribe_params.value_handle);
		return -ENOEXEC;
	}

	if (!default_conn) {
		printk("Not connected\n");
		return -ENOEXEC;
	}

	subscribe_params.ccc_handle = strtoul(argv[1], NULL, 16);
	subscribe_params.value_handle = strtoul(argv[2], NULL, 16);
	subscribe_params.value = BT_GATT_CCC_NOTIFY;
	subscribe_params.notify = notify_func;

	if (argc > 3 && !strcmp(argv[3], "ind")) {
		subscribe_params.value = BT_GATT_CCC_INDICATE;
	}

	err = bt_gatt_subscribe(default_conn, &subscribe_params);
	if (err) {
		subscribe_params.value_handle = 0U;
		printk("Subscribe failed (err %d)\n", err);
	} else {
		printk("Subscribed\n");
	}

	return err;
}

static int cmd_unsubscribe(int argc, char *argv[])
{
	int err;

	if (!default_conn) {
		printk("Not connected\n");
		return -ENOEXEC;
	}

	if (!subscribe_params.value_handle) {
		printk("No subscription found\n");
		return -ENOEXEC;
	}

	err = bt_gatt_unsubscribe(default_conn, &subscribe_params);
	if (err) {
		printk("Unsubscribe failed (err %d)\n", err);
	} else {
		printk("Unsubscribe success\n");
	}

	return err;
}
#endif /* CONFIG_BT_GATT_CLIENT */

static struct db_stats {
	u16_t svc_count;
	u16_t attr_count;
	u16_t chrc_count;
	u16_t ccc_count;
} stats;

static u8_t print_attr(const struct bt_gatt_attr *attr, uint16_t handle,
				void *user_data)
{
	char str[BT_UUID_STR_LEN];

	stats.attr_count++;

	if (!bt_uuid_cmp(attr->uuid, BT_UUID_GATT_PRIMARY) ||
	    !bt_uuid_cmp(attr->uuid, BT_UUID_GATT_SECONDARY)) {
		stats.svc_count++;
	}

	if (!bt_uuid_cmp(attr->uuid, BT_UUID_GATT_CHRC)) {
		stats.chrc_count++;
	}

	if (!bt_uuid_cmp(attr->uuid, BT_UUID_GATT_CCC) &&
	    attr->write == bt_gatt_attr_write_ccc) {
		stats.ccc_count++;
	}

	bt_uuid_to_str(attr->uuid, str, sizeof(str));
	printk("attr %p handle 0x%04x uuid %s perm 0x%02x\n",
		    attr, attr->handle, str, attr->perm);

	return BT_GATT_ITER_CONTINUE;
}

static int cmd_show_db(int argc, char *argv[])
{
	struct bt_uuid_16 uuid;
	size_t total_len;

	memset(&stats, 0, sizeof(stats));

	if (argc > 1) {
		u16_t num_matches = 0;

		uuid.uuid.type = BT_UUID_TYPE_16;
		uuid.val = strtoul(argv[1], NULL, 16);

		if (argc > 2) {
			num_matches = strtoul(argv[2], NULL, 10);
		}

		bt_gatt_foreach_attr_type(0x0001, 0xffff, &uuid.uuid, NULL,
					  num_matches, print_attr,
					  NULL);
		return 0;
	}

	bt_gatt_foreach_attr(0x0001, 0xffff, print_attr, NULL);

	if (!stats.attr_count) {
		printk("No attribute found\n");
		return 0;
	}

	total_len = stats.svc_count * sizeof(struct bt_gatt_service);
	total_len += stats.chrc_count * sizeof(struct bt_gatt_chrc);
	total_len += stats.attr_count * sizeof(struct bt_gatt_attr);
	total_len += stats.ccc_count * sizeof(struct _bt_gatt_ccc);

	printk("=================================================\n");
	printk("Total: %u services %u attributes (%u bytes)\n",
		    stats.svc_count, stats.attr_count, total_len);

	return 0;
}

#if defined(CONFIG_BT_GATT_DYNAMIC_DB)
/* Custom Service Variables */
static struct bt_uuid_128 vnd_uuid = BT_UUID_INIT_128(
	0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
	0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);
static struct bt_uuid_128 vnd_auth_uuid = BT_UUID_INIT_128(
	0xf2, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
	0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);
static const struct bt_uuid_128 vnd_long_uuid1 = BT_UUID_INIT_128(
	0xf3, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
	0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);
static const struct bt_uuid_128 vnd_long_uuid2 = BT_UUID_INIT_128(
	0xde, 0xad, 0xfa, 0xce, 0x78, 0x56, 0x34, 0x12,
	0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

static u8_t vnd_value[] = { 'V', 'e', 'n', 'd', 'o', 'r' };

static struct bt_uuid_128 vnd1_uuid = BT_UUID_INIT_128(
	0xf4, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
	0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

static const struct bt_uuid_128 vnd1_echo_uuid = BT_UUID_INIT_128(
	0xf5, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
	0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

static u8_t echo_enabled;

static void vnd1_ccc_cfg_changed(const struct bt_gatt_attr *attr, u16_t value)
{
	echo_enabled = (value == BT_GATT_CCC_NOTIFY) ? 1 : 0;
}

static ssize_t write_vnd1(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			  const void *buf, u16_t len, u16_t offset,
			  u8_t flags)
{
	if (echo_enabled) {
		printk("Echo attr len %u\n", len);
		bt_gatt_notify(conn, attr, buf, len);
	}

	return len;
}

static ssize_t read_vnd(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			void *buf, u16_t len, u16_t offset)
{
	const char *value = attr->user_data;

	return bt_gatt_attr_read(conn, attr, buf, len, offset, value,
				 strlen(value));
}

static ssize_t write_vnd(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			 const void *buf, u16_t len, u16_t offset,
			 u8_t flags)
{
	u8_t *value = attr->user_data;

	if (offset + len > sizeof(vnd_value)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	memcpy(value + offset, buf, len);

	return len;
}

#define MAX_DATA 30
static u8_t vnd_long_value1[MAX_DATA] = { 'V', 'e', 'n', 'd', 'o', 'r' };
static u8_t vnd_long_value2[MAX_DATA] = { 'S', 't', 'r', 'i', 'n', 'g' };

static ssize_t read_long_vnd(struct bt_conn *conn,
			     const struct bt_gatt_attr *attr, void *buf,
			     u16_t len, u16_t offset)
{
	u8_t *value = attr->user_data;

	return bt_gatt_attr_read(conn, attr, buf, len, offset, value,
				 sizeof(vnd_long_value1));
}

static ssize_t write_long_vnd(struct bt_conn *conn,
			      const struct bt_gatt_attr *attr, const void *buf,
			      u16_t len, u16_t offset, u8_t flags)
{
	u8_t *value = attr->user_data;

	if (flags & BT_GATT_WRITE_FLAG_PREPARE) {
		return 0;
	}

	if (offset + len > sizeof(vnd_long_value1)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	/* Copy to buffer */
	memcpy(value + offset, buf, len);

	return len;
}

static struct bt_gatt_attr vnd_attrs[] = {
	/* Vendor Primary Service Declaration */
	BT_GATT_PRIMARY_SERVICE(&vnd_uuid),

	BT_GATT_CHARACTERISTIC(&vnd_auth_uuid.uuid,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
			       BT_GATT_PERM_READ_AUTHEN |
			       BT_GATT_PERM_WRITE_AUTHEN,
			       read_vnd, write_vnd, vnd_value),

	BT_GATT_CHARACTERISTIC(&vnd_long_uuid1.uuid, BT_GATT_CHRC_READ |
			       BT_GATT_CHRC_WRITE | BT_GATT_CHRC_EXT_PROP,
			       BT_GATT_PERM_READ | BT_GATT_PERM_WRITE |
			       BT_GATT_PERM_PREPARE_WRITE |
			       BT_GATT_PERM_READ_AUTHEN |
			       BT_GATT_PERM_WRITE_AUTHEN,
			       read_long_vnd, write_long_vnd,
			       &vnd_long_value1),

	BT_GATT_CHARACTERISTIC(&vnd_long_uuid2.uuid, BT_GATT_CHRC_READ |
			       BT_GATT_CHRC_WRITE | BT_GATT_CHRC_EXT_PROP,
			       BT_GATT_PERM_READ | BT_GATT_PERM_WRITE |
			       BT_GATT_PERM_PREPARE_WRITE,
			       read_long_vnd, write_long_vnd,
			       &vnd_long_value2),
};

static struct bt_gatt_service vnd_svc = BT_GATT_SERVICE(vnd_attrs);

static struct bt_gatt_attr vnd1_attrs[] = {
	/* Vendor Primary Service Declaration */
	BT_GATT_PRIMARY_SERVICE(&vnd1_uuid),

	BT_GATT_CHARACTERISTIC(&vnd1_echo_uuid.uuid,
			       BT_GATT_CHRC_WRITE_WITHOUT_RESP |
			       BT_GATT_CHRC_NOTIFY,
			       BT_GATT_PERM_WRITE, NULL, write_vnd1, NULL),
	BT_GATT_CCC(vnd1_ccc_cfg_changed,
		    BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
};

static struct bt_gatt_service vnd1_svc = BT_GATT_SERVICE(vnd1_attrs);

static int cmd_register_test_svc(int argc, char *argv[])
{
	bt_gatt_service_register(&vnd_svc);
	bt_gatt_service_register(&vnd1_svc);

	printk("Registering test vendor services\n");
	return 0;
}

static int cmd_unregister_test_svc(int argc, char *argv[])
{
	bt_gatt_service_unregister(&vnd_svc);
	bt_gatt_service_unregister(&vnd1_svc);

	printk("Unregistering test vendor services\n");
	return 0;
}

static void notify_cb(struct bt_conn *conn, void *user_data)
{
	printk("Nofication sent to conn %p\n", conn);
}

static int cmd_notify(int argc, char *argv[])
{
	struct bt_gatt_notify_params params;
	u8_t data = 0;

	if (!echo_enabled) {
		printk("Nofication not enabled\n");
		return -ENOEXEC;
	}

	if (argc > 1) {
		data = strtoul(argv[1], NULL, 16);
	}

	memset(&params, 0, sizeof(params));

	params.uuid = &vnd1_echo_uuid.uuid;
	params.attr = vnd1_attrs;
	params.data = &data;
	params.len = sizeof(data);
	params.func = notify_cb;

	bt_gatt_notify_cb(NULL, &params);

	return 0;
}

static struct bt_uuid_128 met_svc_uuid = BT_UUID_INIT_128(
	0x01, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
	0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

static const struct bt_uuid_128 met_char_uuid = BT_UUID_INIT_128(
	0x02, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12,
	0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12);

static u8_t met_char_value[CHAR_SIZE_MAX] = {
	'M', 'e', 't', 'r', 'i', 'c', 's' };

static ssize_t read_met(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			void *buf, u16_t len, u16_t offset)
{
	const char *value = attr->user_data;
	u16_t value_len;

	value_len = min(strlen(value), CHAR_SIZE_MAX);

	return bt_gatt_attr_read(conn, attr, buf, len, offset, value,
				 value_len);
}

static u32_t write_count;
static u32_t write_len;
static u32_t write_rate;

static ssize_t write_met(struct bt_conn *conn, const struct bt_gatt_attr *attr,
			 const void *buf, u16_t len, u16_t offset,
			 u8_t flags)
{
	u8_t *value = attr->user_data;
	static u32_t cycle_stamp;
	u32_t delta;

	if (offset + len > sizeof(met_char_value)) {
		return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
	}

	memcpy(value + offset, buf, len);

	delta = k_cycle_get_32() - cycle_stamp;
	/* delta = (u32_t)k_cyc_to_ns_floor64(delta); */

	/* if last data rx-ed was greater than 1 second in the past,
	 * reset the metrics.
	 */
	if (delta > 1000000000) {
		write_count = 0U;
		write_len = 0U;
		write_rate = 0U;
		cycle_stamp = k_cycle_get_32();
	} else {
		write_count++;
		write_len += len;
		write_rate = ((u64_t)write_len << 3) * 1000000000U / delta;
	}

	return len;
}

static struct bt_gatt_attr met_attrs[] = {
	BT_GATT_PRIMARY_SERVICE(&met_svc_uuid),

	BT_GATT_CHARACTERISTIC(&met_char_uuid.uuid,
			       BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE,
			       BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
			       read_met, write_met, met_char_value),
};

static struct bt_gatt_service met_svc = BT_GATT_SERVICE(met_attrs);

static int cmd_metrics(int argc, char *argv[])
{
	int err = 0;

	if (argc < 2) {
		printk("Write: count= %u, len= %u, rate= %u bps.\n",
		       write_count, write_len, write_rate);

		return -ENOEXEC;
	}

	if (!strcmp(argv[1], "on")) {
		printk("Registering GATT metrics test Service.\n");
		err = bt_gatt_service_register(&met_svc);
	} else if (!strcmp(argv[1], "off")) {
		printk("Unregistering GATT metrics test Service.\n");
		err = bt_gatt_service_unregister(&met_svc);
	} else {
		printk("Incorrect value: %s\n", argv[1]);
		return -ENOEXEC;
	}

	if (!err) {
		printk("GATT write cmd metrics %s.\n", argv[1]);
	}

	return err;
}
#endif /* CONFIG_BT_GATT_DYNAMIC_DB */

static u8_t get_cb(const struct bt_gatt_attr *attr, uint16_t handle,
		void *user_data)
{
	u8_t buf[256];
	ssize_t ret;
	char str[BT_UUID_STR_LEN];

	bt_uuid_to_str(attr->uuid, str, sizeof(str));
	printk("attr %p uuid %s perm 0x%02x\n", attr, str,
		    attr->perm);

	if (!attr->read) {
		return BT_GATT_ITER_CONTINUE;
	}

	ret = attr->read(NULL, attr, (void *)buf, sizeof(buf), 0);
	if (ret < 0) {
		printk("Failed to read: %d\n", ret);
		return BT_GATT_ITER_STOP;
	}

	hexdump(buf, ret);

	return BT_GATT_ITER_CONTINUE;
}

static int cmd_get(int argc, char *argv[])
{
	u16_t start, end;

	start = strtoul(argv[1], NULL, 16);
	end = start;

	if (argc > 2) {
		end = strtoul(argv[2], NULL, 16);
	}

	bt_gatt_foreach_attr(start, end, get_cb, NULL);

	return 0;
}

struct set_data {
	int argc;
	char **argv;
	int err;
};

static u8_t set_cb(const struct bt_gatt_attr *attr, uint16_t handle,
		void *user_data)
{
	struct set_data *data = user_data;
	u8_t buf[256];
	size_t i;
	ssize_t ret;

	if (!attr->write) {
		printk("Write not supported\n");
		data->err = -ENOENT;
		return BT_GATT_ITER_CONTINUE;
	}

	for (i = 0; i < data->argc; i++) {
		buf[i] = strtoul(data->argv[i], NULL, 16);
	}

	ret = attr->write(NULL, attr, (void *)buf, i, 0, 0);
	if (ret < 0) {
		data->err = ret;
		printk("Failed to write: %d\n", ret);
		return BT_GATT_ITER_STOP;
	}

	return BT_GATT_ITER_CONTINUE;
}

static int cmd_set(int argc, char *argv[])
{
	u16_t handle;
	struct set_data data;

	handle = strtoul(argv[1], NULL, 16);

	data.argc = argc - 2;
	data.argv = argv + 2;
	data.err = 0;

	bt_gatt_foreach_attr(handle, handle, set_cb, &data);

	if (data.err < 0) {
		return -ENOEXEC;
	}

	bt_gatt_foreach_attr(handle, handle, get_cb, NULL);

	return 0;
}

#if defined(CONFIG_BT_EATT)
static int cmd_eatt_connect(int argc, char *argv[])
{
	u8_t num_channels;
	int err;

	if (!default_conn) {
		printk("Not connected\n");
		return -ENOEXEC;
	}

	if (argc != 2) {
		return -EINVAL;
	}

	num_channels = strtoul(argv[1], NULL, 16);

	err = bt_eatt_connect(default_conn, num_channels);
	if (err) {
		printk("EATT Connection failed (%d)\n", err);
		return -ENOEXEC;
	} else {
		printk("EATT Connection pending\n");
	}

	return 0;
}

static int cmd_eatt_disconnect(int argc, char *argv[])
{
	int err;

	if (!default_conn) {
		printk("Not connected\n");
		return -ENOEXEC;
	}

	if (argc != 1) {
		return -EINVAL;
	}

	err = bt_eatt_disconnect(default_conn);
	if (err) {
		printk("EATT Disconnection failed (%d)\n", err);
		return -ENOEXEC;
	} else {
		printk("EATT Disconnection pending\n");
	}

	return 0;
}
#endif /* #if CONFIG_BT_EATT */

#define HELP_NONE "[none]"

static const struct shell_cmd gatt_commands[] = {
#if defined(CONFIG_BT_GATT_CLIENT)
	{ "discover", cmd_discover,
		      "[UUID] [start handle] [end handle]" },
	{ "discover-characteristic", cmd_discover,
		      "[UUID] [start handle] [end handle]" },
	{ "discover-descriptor", cmd_discover,
		      "[UUID] [start handle] [end handle]" },
	{ "discover-include", cmd_discover,
		      "[UUID] [start handle] [end handle]" },
	{ "discover-primary", cmd_discover,
		      "[UUID] [start handle] [end handle]" },
	{ "discover-secondary", cmd_discover,
		      "[UUID] [start handle] [end handle]" },
	{ "exchange-mtu", cmd_exchange_mtu, HELP_NONE },
	{ "read", cmd_read, "<handle> [offset]" },
	{ "read-uuid", cmd_read_uuid, "<UUID> [start handle] [end handle]" },
	{ "read-multiple", cmd_mread, "<handle 1> <handle 2> ..." },
	{ "signed-write", cmd_write_without_rsp, "<handle> <data> [length] [repeat]" },
	{ "subscribe", cmd_subscribe, "<CCC handle> <value handle> [ind]" },
	{ "write", cmd_write, "<handle> <offset> <data> [length]" },
	{ "write-without-response", cmd_write_without_rsp,
		      "<handle> <data> [length] [repeat]" },
	{ "write-without-response-cb", cmd_write_without_rsp,
		      "<handle> <data> [length] [repeat]" },
	{ "unsubscribe", cmd_unsubscribe, HELP_NONE },
#endif /* CONFIG_BT_GATT_CLIENT */
	{ "get", cmd_get, "<start handle> [end handle]" },
	{ "set", cmd_set, "<handle> [data...]" },
	{ "show-db", cmd_show_db, "[uuid] [num_matches]" },
#if defined(CONFIG_BT_GATT_DYNAMIC_DB)
	{ "metrics", cmd_metrics,
		      "register vendr char and measure rx <value: on, off>" },
	{ "register", cmd_register_test_svc,
		      "register pre-predefined test service" },
	{ "unregister", cmd_unregister_test_svc,
		      "unregister pre-predefined test service" },
	{ "notify", cmd_notify, "[data]" },
#endif /* CONFIG_BT_GATT_DYNAMIC_DB */
#if defined(CONFIG_BT_EATT)
	{ "eatt-connect", cmd_eatt_connect, "num_channels" },
	{ "eatt-disconnect", cmd_eatt_disconnect, HELP_NONE },
#endif /* CONFIG_BT_EATT */
	{ NULL, NULL, NULL }
};

SHELL_REGISTER("gatt", gatt_commands);
