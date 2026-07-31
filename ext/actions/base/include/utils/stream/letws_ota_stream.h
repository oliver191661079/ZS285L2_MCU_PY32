#ifndef __CONFIG_LETWS_OTA_STREAM_H
#define __CONFIG_LETWS_OTA_STREAM_H


struct letws_ota_stream_param{
	uint8_t sync_mode;
	int (*send_cb)(uint8_t *data, uint32_t len);
	void *cbuf;
};

io_stream_t letws_ota_stream_create(void *param);

#endif
