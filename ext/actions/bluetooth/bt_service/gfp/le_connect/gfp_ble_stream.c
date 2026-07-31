/*
 * Copyright (c) 2019 Actions Semi Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file
 * @brief
 *
 * Change log:
 *
 */

#define SYS_LOG_DOMAIN "gfp_ble"
#define SYS_LOG_LEVEL 3	/* SYS_LOG_LEVEL_INFO */

#include <zephyr.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <msg_manager.h>
#include <mem_manager.h>
#include <sys_event.h>
#include <stream.h>
#include <sys_comm.h>
#include "gfp_ble_stream.h"
#include "gfp_ble_stream_ctrl.h"

#ifdef CONFIG_BLUETOOTH
#include <bt_manager.h>
#endif

#define MAX_GFPBLE_STREAM		(1)
#define GFP_BLE_BUFF_SIZE		(256)
#define GFP_BLE_SEND_LEN_ONCE	(512)
#define GFP_BLE_SEND_INTERVAL	(5)		/* 5ms */


struct gfp_ble_param_t {
	struct ble_reg_manager le_mgr;
	struct bt_gatt_attr	*keypairing_write_attr;
	struct bt_gatt_attr	*passkey_write_attr;
	struct bt_gatt_attr	*accountkey_write_attr;
	struct bt_gatt_attr	*addtional_date_write_attr;
	struct bt_gatt_attr	*addtional_passkey_write_attr;
	struct bt_gatt_attr	*message_stream_psm_attr;
	struct bt_gatt_attr	*keypairing_ccc_attr;
	struct bt_gatt_attr	*passkey_ccc_attr;
	struct bt_gatt_attr	*addtional_data_ccc_attr;
	struct bt_gatt_attr	*addtional_passkey_ccc_attr;

	u8_t rx_num;
	void (*connect_cb)(bool connected, void *param);
	u8_t connect_type;
	u8_t notify_ind_enable;
	u32_t read_timeout;
	u32_t write_timeout;
	loop_buffer_t rx_loop_buf;
	os_mutex read_mutex;
	os_sem read_sem;
	os_mutex write_mutex;
	struct bt_conn *conn;
	bool (*auth_status)(void);
};

struct bt_gatt_attr	*cur_tx_chrc_attr = NULL;
struct bt_gatt_attr	*cur_tx_attr = NULL;

static void gfp_ble_rx_date(io_stream_t handle, void *attr, u8_t *buf, u16_t len);
static io_stream_t gfp_ble_create_stream[MAX_GFPBLE_STREAM] = {0};
static OS_MUTEX_DEFINE(g_gfpble_mutex);

void gfp_tx_attr_set(void *tx_chrc_attr, void *tx_attr)
{
	cur_tx_chrc_attr = (struct bt_gatt_attr	*)tx_chrc_attr;
	cur_tx_attr = (struct bt_gatt_attr	*)tx_attr;
}

static int gfp_ble_add_stream(io_stream_t handle)
{
	int i;

	os_mutex_lock(&g_gfpble_mutex, OS_FOREVER);
	for (i = 0; i < MAX_GFPBLE_STREAM; i++) {
		if (gfp_ble_create_stream[i] == NULL) {
			gfp_ble_create_stream[i] = handle;
			break;
		}
	}
	os_mutex_unlock(&g_gfpble_mutex);

	if (i == MAX_GFPBLE_STREAM) {
		SYS_LOG_ERR("Failed to add stream handle %p", handle);
		return -EIO;
	}

	return 0;
}

static int gfp_ble_remove_stream(io_stream_t handle)
{
	int i;

	os_mutex_lock(&g_gfpble_mutex, OS_FOREVER);
	for (i = 0; i < MAX_GFPBLE_STREAM; i++) {
		if (gfp_ble_create_stream[i] == handle) {
			gfp_ble_create_stream[i] = NULL;
			break;
		}
	}
	os_mutex_unlock(&g_gfpble_mutex);

	if (i == MAX_GFPBLE_STREAM) {
		SYS_LOG_ERR("Failed to remove stream handle %p", handle);
		return -EIO;
	}

	return 0;
}

static io_stream_t find_streame_by_ble_attr(const struct bt_gatt_attr *attr)
{
	io_stream_t stream;
	int i;

	os_mutex_lock(&g_gfpble_mutex, OS_FOREVER);

	for (i = 0; i < MAX_GFPBLE_STREAM; i++) {
		stream = gfp_ble_create_stream[i];
		if (stream) {
			os_mutex_unlock(&g_gfpble_mutex);
			return stream;
		}
	}

	os_mutex_unlock(&g_gfpble_mutex);

	return NULL;
}

static ssize_t stream_ble_rx_data(struct bt_conn *conn,
				  const struct bt_gatt_attr *attr,
				  const void *buf, u16_t len, u16_t offset,
				  u8_t flags)
{
	struct gfp_ble_param_t *info;
	io_stream_t stream = find_streame_by_ble_attr(attr);

	SYS_LOG_INF("BLE rx data len %d", len);
	if (stream && stream->data) {
		info = (struct gfp_ble_param_t *)stream->data;
		if (info->connect_type == BLE_CONNECT_TYPE && (info->conn == conn)) {
			print_hex_comm("LE", buf, len);
			gfp_ble_rx_date(stream, (void *)attr, (u8_t *)buf, len);
		}
	}

	return len;
}

static void stream_ble_rx_set_notifyind(struct bt_conn *conn, uint8_t conn_type,
        const struct bt_gatt_attr *attr, u16_t value)

{
	struct gfp_ble_param_t *info;
	io_stream_t stream = find_streame_by_ble_attr(attr);

	SYS_LOG_INF("attr:%p, enable:%d", attr, value);

	if (!stream) {
		return;
	}
	info = (struct gfp_ble_param_t *)stream->data;

    if((info->conn != NULL) && (info->conn != conn)){
        SYS_LOG_INF("fp is working %p new:%p",info->conn,conn);
        return;
    }

	info->notify_ind_enable = (u8_t)value;
	if (stream && stream->data && value) {
		if (info->connect_type == NONE_CONNECT_TYPE) {
			info->connect_type = BLE_CONNECT_TYPE;
			info->conn = conn;
			if (info->connect_cb) {
				info->connect_cb(true,conn);
			}
		} else {
			SYS_LOG_WRN("Had connecte: %d", info->connect_type);
		}
	}
}

static void stream_ble_connect_cb(struct bt_conn *conn, uint8_t conn_type, uint8_t *mac, uint8_t connected)
{
	io_stream_t stream;
	struct gfp_ble_param_t *info;
	int i;
    int cnt;

	os_mutex_lock(&g_gfpble_mutex, OS_FOREVER);

	if (!connected) {
		for (i = 0; i < MAX_GFPBLE_STREAM; i++) {
			stream = gfp_ble_create_stream[i];
			if (stream) {
				info = (struct gfp_ble_param_t *)stream->data;
				if ((info->connect_type == BLE_CONNECT_TYPE) && (info->conn == conn)) {
					info->connect_type = NONE_CONNECT_TYPE;
					info->conn = NULL;
					os_sem_give(&info->read_sem);
					if (info->connect_cb) {
						info->connect_cb(false,conn);
					}
				}
			}
		}
	}
	else{
        for (i = 0; i < MAX_GFPBLE_STREAM; i++) {
            stream = gfp_ble_create_stream[i];
            if (stream){
                info = (struct gfp_ble_param_t *)stream->data;
                cnt = bt_manager_ble_get_connected_num();
                if(cnt > 1){
                    if(!info->auth_status()){
                        info->conn = conn;
                        info->connect_type = BLE_CONNECT_TYPE;
                        if (info->connect_cb) {
                            info->connect_cb(true,conn);
                        }
                    }
                }
            }
        }
    }
	os_mutex_unlock(&g_gfpble_mutex);
}

extern ssize_t btsrv_gfp_read_message_stream_psm(struct bt_conn *conn,
				const struct bt_gatt_attr *attr, void *buf,
				uint16_t len, uint16_t offset);

static int gfp_ble_register(struct gfp_ble_param_t *info)
{
	struct _bt_gatt_ccc *notify_ccc;

	info->keypairing_write_attr->write = stream_ble_rx_data;
	info->passkey_write_attr->write = stream_ble_rx_data;
	info->accountkey_write_attr->write = stream_ble_rx_data;
	info->addtional_date_write_attr->write = stream_ble_rx_data;
	info->addtional_passkey_write_attr->write = stream_ble_rx_data;
	info->message_stream_psm_attr->read = btsrv_gfp_read_message_stream_psm;

	notify_ccc = (struct _bt_gatt_ccc *)info->keypairing_ccc_attr->user_data;
	notify_ccc->cfg_changed = stream_ble_rx_set_notifyind;
	notify_ccc = (struct _bt_gatt_ccc *)info->passkey_ccc_attr->user_data;
	notify_ccc->cfg_changed = stream_ble_rx_set_notifyind;
	notify_ccc = (struct _bt_gatt_ccc *)info->addtional_data_ccc_attr->user_data;
	notify_ccc->cfg_changed = stream_ble_rx_set_notifyind;
	notify_ccc = (struct _bt_gatt_ccc *)info->addtional_passkey_ccc_attr->user_data;
	notify_ccc->cfg_changed = stream_ble_rx_set_notifyind;

	info->le_mgr.link_cb = stream_ble_connect_cb;
#ifdef CONFIG_BT_BLE
	bt_manager_ble_service_reg(&info->le_mgr);
#endif
	return 0;
}

static int gfp_ble_init(io_stream_t handle, void *param)
{
	int ret = 0;
	struct gfp_ble_param_t *info = NULL;
	struct gfp_ble_stream_init_param *init_param = (struct gfp_ble_stream_init_param *)param;

	if (gfp_ble_add_stream(handle)) {
		ret = -EIO;
		goto err_exit;
	}

	info = app_mem_malloc(sizeof(struct gfp_ble_param_t));
	if (!info) {
		SYS_LOG_ERR("cache stream info malloc failed\n");
		ret = -ENOMEM;
		goto err_exit;
	}

	memset(info, 0, sizeof(struct gfp_ble_param_t));
	info->le_mgr.gatt_svc.attrs = init_param->gatt_attr;
	info->le_mgr.gatt_svc.attr_count = init_param->attr_size;
	info->keypairing_ccc_attr = init_param->keypairing_ccc_attr;
	info->passkey_ccc_attr = init_param->passkey_ccc_attr;
	info->addtional_data_ccc_attr = init_param->addtional_data_ccc_attr;
	info->addtional_passkey_ccc_attr = init_param->addtional_passkey_ccc_attr;
	info->keypairing_write_attr = init_param->keypairing_write_attr;
	info->passkey_write_attr = init_param->passkey_write_attr;
	info->accountkey_write_attr = init_param->accountkey_write_attr;
	info->addtional_date_write_attr = init_param->addtional_data_write_attr;
	info->addtional_passkey_write_attr = init_param->addtional_passkey_write_attr;
	info->message_stream_psm_attr = init_param->message_stream_psm_attr;

	info->connect_cb = init_param->connect_cb;
	info->read_timeout = init_param->read_timeout;
	info->write_timeout = init_param->write_timeout;
	info->auth_status = init_param->auth_status;
	os_mutex_init(&info->read_mutex);
	os_sem_init(&info->read_sem, 0, 1);
	os_mutex_init(&info->write_mutex);

	handle->data = info;

	if (gfp_ble_register(info)) {
		ret = -EIO;
		goto err_exit;
	}

	return 0;

err_exit:
	if (info) {
		app_mem_free(info);
	}

	gfp_ble_remove_stream(handle);
	return ret;
}

static int gfp_ble_open(io_stream_t handle, stream_mode mode)
{
	struct gfp_ble_param_t *info = NULL;

	info = (struct gfp_ble_param_t *)handle->data;
	if (info->connect_type == NONE_CONNECT_TYPE) {
		return -EIO;
	}

	os_mutex_lock(&info->read_mutex, OS_FOREVER);
	if (!info->rx_loop_buf.data_buf)
	{
		info->rx_loop_buf.buf_size = GFP_BLE_BUFF_SIZE;
		info->rx_loop_buf.data_buf = (u8_t *)app_mem_malloc(info->rx_loop_buf.buf_size);
		if (!info->rx_loop_buf.data_buf) {
			os_mutex_unlock(&info->read_mutex);
			return -ENOMEM;
		}
	}

	handle->total_size = GFP_BLE_BUFF_SIZE;
	handle->cache_size = 0;
	handle->rofs = 0;
	handle->wofs = 0;
	os_mutex_unlock(&info->read_mutex);

	return 0;
}

static void gfp_ble_rx_date(io_stream_t handle, void *attr, u8_t *buf, u16_t len)
{
	struct gfp_ble_param_t *info = NULL;

	info = (struct gfp_ble_param_t *)handle->data;
	os_mutex_lock(&info->read_mutex, OS_FOREVER);
	if ((len + sizeof(u32_t) + sizeof(u16_t) + info->rx_loop_buf.data_count) <= info->rx_loop_buf.buf_size)
	{
		if (sizeof(u32_t) != loop_buffer_write(&info->rx_loop_buf, &attr, sizeof(u32_t)))
			SYS_LOG_ERR("");
		if (sizeof(u16_t) != loop_buffer_write(&info->rx_loop_buf, &len, sizeof(u16_t)))
			SYS_LOG_ERR("");	
		if(len != loop_buffer_write(&info->rx_loop_buf, buf, len))
			SYS_LOG_ERR("");

		os_sem_give(&info->read_sem);		
	}
	else
	{
		SYS_LOG_ERR("0x%x 0x%x.",
			len,(info->rx_loop_buf.buf_size - info->rx_loop_buf.data_count));
	}
	os_mutex_unlock(&info->read_mutex);
}

static int gfp_ble_get_length(io_stream_t handle)
{
	struct gfp_ble_param_t *info = NULL;

	info = (struct gfp_ble_param_t *)handle->data;
	return info->rx_loop_buf.data_count;
}

static int gfp_ble_read(io_stream_t handle, u8_t *buf, int num)
{
	struct gfp_ble_param_t *info = NULL;
	u16_t r_len;

	info = (struct gfp_ble_param_t *)handle->data;

	os_mutex_lock(&info->read_mutex, OS_FOREVER);
	if ((info->connect_type == NONE_CONNECT_TYPE) ||
		(info->rx_loop_buf.data_buf == NULL)) {
		os_mutex_unlock(&info->read_mutex);
		return -EIO;
	}

	if ((info->connect_type != NONE_CONNECT_TYPE) && (info->rx_loop_buf.data_count == 0) &&
		(info->read_timeout != OS_NO_WAIT)) {
		os_sem_reset(&info->read_sem);
		os_mutex_unlock(&info->read_mutex);

		os_sem_take(&info->read_sem, info->read_timeout);
		os_mutex_lock(&info->read_mutex, OS_FOREVER);
	}

	if (info->rx_loop_buf.data_count == 0) {
		os_mutex_unlock(&info->read_mutex);
		return 0;
	}

	r_len = loop_buffer_read(&info->rx_loop_buf, buf, num);

	os_mutex_unlock(&info->read_mutex);
	return r_len;
}

static int gfp_ble_tell(io_stream_t handle)
{
	int ret = 0;
	struct gfp_ble_param_t *info = NULL;

	info = (struct gfp_ble_param_t *)handle->data;
	if (info->connect_type == NONE_CONNECT_TYPE) {
		return -EIO;
	}

	os_mutex_lock(&info->read_mutex, OS_FOREVER);
	//ret = handle->cache_size;
	ret = info->rx_loop_buf.data_count;
	os_mutex_unlock(&info->read_mutex);

	return ret;
}

#ifdef CONFIG_BT_BLE
static int gfp_ble_send_data(struct gfp_ble_param_t *info, u8_t *buf, int num)
{
	u16_t mtu;
	int send_len = 0, w_len, le_send, cur_len;
	int ret = 0;
	u32_t timeout = 0;
	struct bt_gatt_attr *tx_attr, *ccc_attr;

	while ((info->connect_type == BLE_CONNECT_TYPE) &&
			(info->notify_ind_enable) && (send_len < num)) {

		tx_attr = cur_tx_chrc_attr;
		ccc_attr = cur_tx_attr;

		w_len = ((num - send_len) > GFP_BLE_SEND_LEN_ONCE) ? GFP_BLE_SEND_LEN_ONCE : (num - send_len);
		mtu = bt_manager_get_ble_mtu(info->conn) - 3;
		le_send = 0;
		while (le_send < w_len) {
			cur_len = ((w_len - le_send) > mtu) ? mtu : (w_len - le_send);
			ret = bt_manager_ble_send_data(info->conn, tx_attr, ccc_attr, &buf[send_len], cur_len);
			if (ret < 0) {
				break;
			}
			send_len += cur_len;
			le_send += cur_len;
		}

		if (ret < 0) {
			if (info->write_timeout == OS_NO_WAIT) {
				break;
			} else if (info->write_timeout == OS_FOREVER) {
				os_sleep(GFP_BLE_SEND_INTERVAL);
				continue;
			} else {
				if (timeout >= info->write_timeout) {
					break;
				}

				timeout += GFP_BLE_SEND_INTERVAL;
				os_sleep(GFP_BLE_SEND_INTERVAL);
				continue;
			}
		}
	}
	return send_len;
}
#endif

static int gfp_ble_write(io_stream_t handle, u8_t *buf, int num)
{
	int ret = 0;
	struct gfp_ble_param_t *info = NULL;

	info = (struct gfp_ble_param_t *)handle->data;
	if (info->connect_type == NONE_CONNECT_TYPE) {
		return -EIO;
	}

	os_mutex_lock(&info->write_mutex, OS_FOREVER);
	if (info->connect_type == BLE_CONNECT_TYPE) {
	#ifdef CONFIG_BT_BLE
		ret = gfp_ble_send_data(info, buf, num);
	#endif
	}
	os_mutex_unlock(&info->write_mutex);

	return ret;
}

static int gfp_ble_close(io_stream_t handle)
{
	struct gfp_ble_param_t *info = NULL;

	info = (struct gfp_ble_param_t *)handle->data;
	if (info->connect_type != NONE_CONNECT_TYPE) {
		SYS_LOG_INF("Active do %d disconnect", info->connect_type);
		if (info->connect_type == BLE_CONNECT_TYPE) {
			bt_manager_ble_disconnect(info->conn);
		}
	}

	os_mutex_lock(&info->read_mutex, OS_FOREVER);
	if (info->rx_loop_buf.data_buf)
	{
		app_mem_free(info->rx_loop_buf.data_buf);
		memset(&info->rx_loop_buf, 0 , sizeof(info->rx_loop_buf));
	}
	handle->rofs = 0;
	handle->wofs = 0;
	handle->cache_size = 0;
	handle->total_size = 0;
	os_mutex_unlock(&info->read_mutex);

	return 0;
}

static int gfp_ble_destroy(io_stream_t handle)
{
	SYS_LOG_WRN("gfp stream not support destroy new!!");
	return -EIO;
}

const stream_ops_t gfp_ble_stream_ops = {
	.init = gfp_ble_init,
	.open = gfp_ble_open,
	.read = gfp_ble_read,
	.get_length = gfp_ble_get_length,
	.seek = NULL,
	.tell = gfp_ble_tell,
	.write = gfp_ble_write,
	.close = gfp_ble_close,
	.destroy = gfp_ble_destroy,
};

io_stream_t gfp_ble_stream_create(void *param)
{
	return stream_create(&gfp_ble_stream_ops, param);
}

