#ifndef __CONFIG_SERIAL_FLASHER_INNER_H
#define __CONFIG_SERIAL_FLASHER_INNER_H

#include "serial_flasher.h"

#define OTA_READ_DATA_SIZE      (220)
#define OTA_MAX_READ_DATA_SIZE  (220 * 64)

struct serial_flasher;

struct serial_flasher{
    io_stream_t letws_stream;
    os_sem read_sem;
    uint8_t negotiation_done;
    uint8_t upgrade_end;
	uint8_t upgrade_result;

	uint8_t state;
	int send_buf_size;
	uint8_t *send_buf;
	int read_len;
	uint8_t read_mask[OTA_MAX_READ_DATA_SIZE / OTA_READ_DATA_SIZE / 8];
	int read_done_len;
	uint8_t last_psn;
	uint8_t host_features;
	uint16_t ota_unit_size;
	int stream_opened;
	uint8_t *frame_buffer;
	uint8_t *read_ptr;
	uint8_t tx_seq;
	uint8_t rx_seq;

    uint16_t host_wait_timeout;
    uint16_t device_restart_timeout;

	uint32_t read_timeout;
	struct device *flash_dev;
	const struct partition_entry *part;
	uint32_t read_offset;
	uint32_t mcu_file_size;
	uint8_t progress;
	int (*binary_read_callback)(struct serial_flasher *ctx, uint32_t offset, uint8_t *buf, uint32_t len);
	serial_flasher_cb_t cb;

	struct acts_ringbuf *cbuf;
};

int serial_flasher_process_command(struct serial_flasher *ctx, uint32_t *processed_cmd);

int serial_flasher_port_init(struct serial_flasher *ctx, const char *dev_name, uint32_t recv_buf_size, void (*send_cb)(uint8_t *data, uint32_t len));

int serial_flasher_port_deinit(struct serial_flasher *ctx);

int serial_flasher_request_upgrade(struct serial_flasher *ctx);

int serial_flasher_cancel_upgrade(struct serial_flasher *ctx);


int serial_flasher_connect_negotiation(struct serial_flasher *ctx);

int serial_flasher_send_negotiation_result(struct serial_flasher *ctx, int result);

#endif
