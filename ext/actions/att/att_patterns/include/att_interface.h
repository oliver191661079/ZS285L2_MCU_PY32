#ifndef __ATT_INTERFACE_H
#define __ATT_INTERFACE_H

int32 att_write_data(uint16 cmd, uint32 payload_len, uint8 *data_addr);

int32 att_read_data(uint16 cmd, uint32 payload_len, uint8 *data_addr);

bool att_cmd_send(u16_t stub_cmd, u8_t try_count);

void act_test_change_test_timeout(uint16 timeout);

int get_nvram_db_data(u8_t db_type, u8_t *addr, u32_t len);

int set_nvram_db_data(u8_t db_type, u8_t *addr, u32_t len);

enum
{
    DB_DATA_BT_ADDR,
    DB_DATA_BT_NAME,
    DB_DATA_BT_BLE_NAME,
    DB_DATA_BT_PLIST,
    DB_DATA_BT_RF_BQB_FLAG,
};

struct bt_dev_rdm_state
{
    u8_t addr[6];
    u8_t acl_connected : 1;
    u8_t hfp_connected : 1;
    u8_t a2dp_connected : 1;
    u8_t avrcp_connected : 1;
    u8_t hid_connected : 1;
    u8_t sco_connected : 1;
    u8_t a2dp_stream_started : 1;
};

struct bt_audio_call {
	/*
	 * For call gateway, "handle" will not be used to distinguish diffrent
	 * call (should use "index" instead), because for multiple terminals
	 * cases, there may be only 1 call actually.
	 *
	 * For call terminal, "handle" will be used to distinguish diffrent
	 * call gateway, "index" will be used to distinguish diffrent call of
	 * the same call gateway.
	 */
	uint16_t handle;
	uint8_t index;	/* call index */

	uint8_t state;

	/* URI format for LE: uri_scheme:caller_id with UTF-8 */
	uint8_t *uri;
};


#if 0
typedef struct
{
    uint8 filename[11];
    uint8 reserved1[5];
    uint32 offset;
    uint32 length;
	u32_t load_addr;
	u32_t run_addr;
} atf_dir_t;
#else
typedef struct
{
    uint8 filename[12];
    uint32 load_addr;
    uint32 offset;
    uint32 length;
    uint32 run_addr;
    uint32 checksum;
} atf_dir_t;
#endif

typedef struct
{
    u32_t version;
	int32 (*att_write_data)(uint16 cmd, uint32 payload_len, uint8 *data_addr);
	int32 (*att_read_data)(uint16 cmd, uint32 payload_len, uint8 *data_addr);

	int (*vprintk)(const char *fmt, va_list args);

	u32_t (*k_uptime_get_32)(void);

	void (*k_sem_init)(struct k_sem *sem, unsigned int initial_count,
			unsigned int limit);

	void (*k_sem_give)(struct k_sem *sem);

	int (*k_sem_take)(struct k_sem *sem, s32_t timeout);

	void *(*z_malloc)(size_t size);

	void (*free)(void *ptr);

	void (*k_sleep)(s32_t duration);

	void *(*k_thread_create)(void *new_thread,
				void *stack,
				u32_t stack_size, void (*entry)(void *, void *, void*),
				void *p1, void *p2, void *p3,
				int prio, u32_t options, s32_t delay);

	void* (*hal_aout_channel_open)(audio_out_init_param_t *init_param);
	int (*hal_aout_channel_close)(void *aout_channel_handle);
	int (*hal_aout_channel_write_data)(void *aout_channel_handle, uint8_t *buff, uint32_t len);
	int (*hal_aout_channel_start)(void *aout_channel_handle);
	int (*hal_aout_channel_stop)(void *aout_channel_handle);

	void* (*hal_ain_channel_open)(audio_in_init_param_t *ain_setting);
	int32_t (*hal_ain_channel_close)(void *ain_channel_handle);
	int (*hal_ain_channel_read_data)(void *ain_channel_handle, uint8_t *buff, uint32_t len);
	int (*hal_ain_channel_start)(void* ain_channel_handle);
	int (*hal_ain_channel_stop)(void* ain_channel_handle);

	int (*bt_manager_get_rdm_state)(struct bt_dev_rdm_state *state);
	void (*att_btstack_install)(u8_t *addr, u8_t mode);
	void (*att_btstack_uninstall)(u8_t *addr, u8_t mode);
    int (*bt_manager_call_accept)(struct bt_audio_call *call);

	int (*get_nvram_db_data)(u8_t db_data_type, u8_t* data, u32_t len);
	int (*set_nvram_db_data)(u8_t db_data_type, u8_t* data, u32_t len);


	int (*freq_compensation_read)(uint32_t *trim_cap, uint32_t mode);

	int (*freq_compensation_write)(uint32_t *trim_cap, uint32_t mode);

	int (*read_file)(u8_t *dst_addr, u32_t dst_buffer_len, const u8_t *sub_file_name, s32_t offset, s32_t read_len, atf_dir_t *sub_atf_dir);
	void (*print_buffer)(const void *addr, int width, int count, int linelen, \
		unsigned long disp_addr);

//	void (*bt_manager_set_not_linkkey_connect)(void);
	void (*sys_pm_reboot)(int reboot_type);
	int (*att_flash_erase)(off_t offset, size_t size);
    int (*board_audio_device_mapping)(uint16_t logical_dev, uint8_t *physical_dev, uint8_t *track_flag);
    void (*enter_BQB_mode)(void);
    int (*get_BQB_info)(void);
}att_interface_api_t;


typedef struct
{
    struct device *gpio_dev;
} att_interface_dev_t;

int bt_manager_get_rdm_state(struct bt_dev_rdm_state *state);
void bt_manager_install_stack(u8_t *addr, u8_t mode);
void bt_manager_uninstall_stack(u8_t *addr, u8_t mode);
void bt_manager_check_accept_call(struct bt_audio_call *call);

int read_atf_sub_file(u8_t *dst_addr, u32_t dst_buffer_len, const u8_t *sub_file_name, s32_t offset, s32_t read_len, atf_dir_t *sub_atf_dir);

extern int board_audio_device_mapping(uint16_t logical_dev, uint8_t *physical_dev, uint8_t *track_flag);
extern void enter_BQB_mode(void);
extern int get_BQB_info(void);


void print_buffer(const void *addr, int width, int count, int linelen, \
	unsigned long disp_addr);

#define REBOOT_TYPE_NORMAL		0x0
#define REBOOT_TYPE_GOTO_ADFU		0x1000
#define REBOOT_TYPE_GOTO_BTSYS		0x1100
#define REBOOT_TYPE_GOTO_WIFISYS	0x1200
#define REBOOT_TYPE_GOTO_SYSTEM		0x1200
#define REBOOT_TYPE_GOTO_RECOVERY	0x1300
void sys_pm_reboot(int reboot_type);
#endif
