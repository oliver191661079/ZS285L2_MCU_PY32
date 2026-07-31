#ifndef __OTA_BACKEND_UART_AT_STREAM_H
#define __OTA_BACKEND_UART_AT_STREAM_H


struct ota_backend_letws_param {
	int (*send_cb)(uint8_t *data, uint32_t len);
};

extern struct ota_backend *ota_backend_letws_ota_stream_init(ota_backend_notify_cb_t cb, struct ota_backend_letws_param *param);

extern int ota_backend_letws_rx_data_write(struct ota_backend *backend, uint8_t *data, uint32_t len);

#endif
