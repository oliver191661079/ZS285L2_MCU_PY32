#ifndef CONFIG_SERIAL_FLASHER_H
#define CONFIG_SERIAL_FLASHER_H

#include "ota_upgrade.h"

struct serial_flasher;

typedef enum{
	SERIAL_FLASHER_START_UPGRADE,
	SERIAL_FLASHER_STOP_UPGRADE,
	SERIAL_FLASHER_UPDATE_PROCESS,
}serial_flasher_state_e;

typedef enum{
	SERIAL_FLASHER_TYPE_NONE,
	SERIAL_FLASHER_TYPE_APP,
	SERIAL_FLASHER_TYPE_UDISK,
	SERIAL_FLASHER_TYPE_SDCARD,
}serial_flasher_type_e;

typedef int (*serial_flasher_cb_t)(serial_flasher_state_e state, int param);

extern struct serial_flasher *serial_flasher_init(const char *flash_dev_name, serial_flasher_cb_t cb, void (*send_cb)(uint8_t *buf, uint32_t len), int file_size);

extern int serial_flasher_deinit(struct serial_flasher *ctx);

extern int serial_flasher_routinue(struct serial_flasher *ctx);

extern int serial_flasher_save_rx_data(struct serial_flasher *ctx, uint8_t *data, uint32_t len);

extern int serial_flasher_mcu_upgrade_breakpoint_save(uint8_t need_upgrade, uint8_t type, uint32_t file_size);

extern int serial_flasher_is_mcu_need_upgrade(void);

extern int serial_flasher_is_sdcard_upgrade(void);

extern int serial_flasher_get_mcu_file_size(void);

#endif
