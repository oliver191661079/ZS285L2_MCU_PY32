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

#ifndef _GFP_BLE_STREAM_H
#define _GFP_BLE_STREAM_H

#ifndef BOOL
#define BOOL int
#endif

#ifndef TRUE
#define TRUE    1
#endif

#ifndef FALSE
#define FALSE   0
#endif

/**bt manager gfp ble strea init param*/
struct gfp_ble_stream_init_param {
	void *gatt_attr;
	u8_t attr_size;
	void *keypairing_write_attr;
	void *passkey_write_attr;
	void *accountkey_write_attr;
	void *addtional_data_write_attr;
	void *addtional_passkey_write_attr;
	void *message_stream_psm_attr;
	void *keypairing_ccc_attr;
	void *passkey_ccc_attr;
	void *addtional_data_ccc_attr;
	void *addtional_passkey_ccc_attr;
	void *connect_cb;
	int32_t read_timeout;
	int32_t write_timeout;
	void *auth_status;
};

void gfp_tx_attr_set(void *tx_attr, void *ccc_attr);
io_stream_t gfp_ble_stream_create(void *param);

#endif	/* _GFP_BLE_STREAM_H */

