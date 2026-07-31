/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file system app att
 */
#ifndef __SYSTEM_APP_ATT_H
#define __SYSTEM_APP_ATT_H

#include <zephyr/types.h>
#include <fw_version.h>
#include "app_ui.h"
#include <bt_manager.h>
#include "stream.h"
#include <mem_manager.h>
#include "arithmetic.h"
#include <hotplug.h>
#include <fs_manager.h>
#include <audio_hal.h>
#include <drivers/stub/stub.h>
#include <compensation.h>
#include <tts_manager.h>
#include <global_mem.h>
#include <logging/sys_log.h>
#include <flash.h>
#include <hex_str.h>
#include <tool_app.h>

#define STUB_CMD_OPEN                   (0xff00)
#define STUB_CMD_ATT_FREAD              0x0408
#define ATF_MAX_SUB_FILENAME_LENGTH  12
#define ATT_STACK_SIZE               2048

#define CFG_MAX_BT_DEV_NAME_LEN     30
#define MGR_MAX_REMOTE_NAME_LEN        (30)
#define BT_MAC_NAME_LEN_MAX        30	/*9*3 + 2 */
#define BT_BLE_NAME_LEN_MAX     30	/*9*3 + 2 */

#define TEST_BTNAME_MAX_LEN     CFG_MAX_BT_DEV_NAME_LEN	/*18*3 + 2 */
#define TEST_BTBLENAME_MAX_LEN  CFG_MAX_BT_DEV_NAME_LEN	/*29 + null */

#define MSG_ATT_BT_ENGINE_READY  1

enum {
	DB_DATA_BT_ADDR,
	DB_DATA_BT_NAME,
	DB_DATA_BT_BLE_NAME,
	DB_DATA_BT_PLIST,
	DB_DATA_BT_RF_BQB_FLAG,
};

//atf file read data request packet
typedef struct {
	stub_ext_cmd_t cmd;
	u32_t offset;
	u32_t readLen;
} __packed atf_file_read_t;

//atf file read data reply packet
typedef struct {
	stub_ext_cmd_t cmd;
	u8_t payload[0];
} __packed atf_file_read_ack_t;

#if 0
typedef struct {
	uint8 filename[11];
	uint8 reserved1[5];
	uint32 offset;
	uint32 length;
	u32_t load_addr;
	u32_t run_addr;
} atf_dir_t;
#else
typedef struct {
	uint8 filename[12];
	uint32 load_addr;
	uint32 offset;
	uint32 length;
	uint32 run_addr;
	uint32 checksum;
} atf_dir_t;
#endif

typedef struct {
	u32_t version;
	 int32(*att_write_data) (uint16 cmd, uint32 payload_len,
				 uint8 * data_addr);
	 int32(*att_read_data) (uint16 cmd, uint32 payload_len,
				uint8 * data_addr);

	int (*vprintk)(const char *fmt, va_list args);

	 u32_t(*k_uptime_get_32) (void);

	void (*k_sem_init)(struct k_sem * sem, unsigned int initial_count,
			   unsigned int limit);

	void (*k_sem_give)(struct k_sem * sem);

	int (*k_sem_take)(struct k_sem * sem, s32_t timeout);

	void *(*z_malloc)(size_t size);

	void (*free)(void *ptr);

	void (*k_sleep)(s32_t duration);

	void *(*k_thread_create)(void *new_thread,
				 void *stack,
				 u32_t stack_size, void(*entry)(void *, void *,
								void *),
				 void *p1, void *p2, void *p3, int prio,
				 u32_t options, s32_t delay);

	void *(*hal_aout_channel_open)(audio_out_init_param_t * init_param);
	int (*hal_aout_channel_close)(void *aout_channel_handle);
	int (*hal_aout_channel_write_data)(void *aout_channel_handle,
					   uint8_t * buff, uint32_t len);
	int (*hal_aout_channel_start)(void *aout_channel_handle);
	int (*hal_aout_channel_stop)(void *aout_channel_handle);

	void *(*hal_ain_channel_open)(audio_in_init_param_t * ain_setting);
	 int32_t(*hal_ain_channel_close) (void *ain_channel_handle);
	int (*hal_ain_channel_read_data)(void *ain_channel_handle,
					 uint8_t * buff, uint32_t len);
	int (*hal_ain_channel_start)(void *ain_channel_handle);
	int (*hal_ain_channel_stop)(void *ain_channel_handle);

	int (*bt_manager_get_rdm_state)(struct bt_dev_rdm_state * state);
	void (*att_btstack_install)(u8_t * addr, u8_t mode);
	void (*att_btstack_uninstall)(u8_t * addr, u8_t mode);
	int (*bt_manager_call_accept)(struct bt_audio_call * call);

	int (*get_nvram_db_data)(u8_t db_data_type, u8_t * data, u32_t len);
	int (*set_nvram_db_data)(u8_t db_data_type, u8_t * data, u32_t len);

	int (*freq_compensation_read)(uint32_t * trim_cap, uint32_t mode);

	int (*freq_compensation_write)(uint32_t * trim_cap, uint32_t mode);

	int (*read_file)(u8_t * dst_addr, u32_t dst_buffer_len,
			 const u8_t * sub_file_name, s32_t offset,
			 s32_t read_len, atf_dir_t * sub_atf_dir);
	void (*print_buffer)(const void *addr, int width, int count,
			     int linelen, unsigned long disp_addr);

//      void (*bt_manager_set_not_linkkey_connect)(void);
	void (*sys_pm_reboot)(int reboot_type);
	int (*att_flash_erase)(off_t offset, size_t size);
	int (*board_audio_device_mapping)(uint16_t logical_dev,
					  uint8_t * physical_dev,
					  uint8_t * track_flag);
	void (*enter_BQB_mode)(void);
	int (*get_BQB_info)(void);
} att_interface_api_t;

typedef struct {
	struct device *gpio_dev;
} att_interface_dev_t;

extern bool whether_usb_stub_drv_ready(void);

extern void mp_system_hardware_init(void);

extern void mp_system_hardware_deinit(void);

extern void mp_core_init(void *mp_manager);

extern int8 mp_set_packet_param(void *packet_ctrl);

extern uint8 mp_packet_send_process(void);

extern uint8 mp_packet_receive_process(void);

extern uint8 mp_single_tone_process(void *mp_manager);

extern uint32 mp_get_hosc_cap(void);

extern void mp_set_hosc_cap(uint32 cap);

extern void mp_controller_request_irq(void);

extern void mp_controller_free_irq(void);

extern int hal_aout_channel_close(void *aout_channel_handle);
extern int hal_aout_channel_write_data(void *aout_channel_handle,
				       uint8_t * buff, uint32_t len);
extern int hal_aout_channel_start(void *aout_channel_handle);
extern int hal_aout_channel_stop(void *aout_channel_handle);

extern int32_t hal_ain_channel_close(void *ain_channel_handle);
extern int hal_ain_channel_read_data(void *ain_channel_handle, uint8_t * buff,
				     uint32_t len);
extern int hal_ain_channel_start(void *ain_channel_handle);
extern int hal_ain_channel_stop(void *ain_channel_handle);

#endif
