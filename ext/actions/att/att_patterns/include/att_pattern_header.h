#ifndef __ATT_PATTERN_HEADER_H
#define __ATT_PATTERN_HEADER_H

#define CFG_MAX_BT_DEV_NAME_LEN     30
#define MGR_MAX_REMOTE_NAME_LEN        (30)
#define BT_MAC_NAME_LEN_MAX        30 /*9*3 + 2*/
#define BT_BLE_NAME_LEN_MAX     30 /*9*3 + 2*/


#define TEST_BTNAME_MAX_LEN     CFG_MAX_BT_DEV_NAME_LEN /*18*3 + 2*/
#define TEST_BTBLENAME_MAX_LEN  CFG_MAX_BT_DEV_NAME_LEN /*29 + null*/

//General Test ID. Range 0x01-0xbf
#define TESTID_MODIFY_BTNAME        0x01
#define TESTID_MODIFY_BLENAME       0x02
#define TESTID_FM_TEST              0x05
#define TESTID_GPIO_TEST            0x06
#define TESTID_LINEIN_CH_TEST_ATS28XX       0x07
#define TESTID_MIC_CH_TEST          0x08
#define TESTID_FM_CH_TEST           0x09
#define TESTID_SDCARD_TEST          0x0a
#define TESTID_UHOST_TEST           0x0b
#define TESTID_LINEIN_TEST          0x0c
#define TESTID_PREPARE_PRODUCT_TEST 0x0d
#define TESTID_PRODUCT_TEST         0x0e
#define TESTID_FLASHDUMP_TEST       0x0f
#define TESTID_BURN_IC              0x10
#define TESTID_CHECK_IC             0x11
#define TESTID_FLASHDOWNLOAD_TEST   0x12
#define TESTID_MEM_UPLOAD_TEST      0x13
#define TESTID_MEM_DOWNLOAD_TEST    0x14
//#define TESTID_GPIO_TEST_ATS2823    0x15
#define TESTID_READ_BTNAME          0x16
#define TESTID_MONITOR              0x17
#define TESTID_FTMODE               0x18
#define TESTID_BQBMODE              0x19
#define TESTID_FLASHTEST            0x1a
#define TESTID_LRADC_TEST           0x1b
//#define TESTID_LINEIN_CH_TEST_ATS2823  0x1c
#define TESTID_READ_FW_VERSION      0x1d


//Special Test ID, paired with tool. Range 0xc0-0xfe
#define TESTID_MODIFY_BTADDR  0xc0
#define TESTID_BT_TEST        0xc1
#define TESTID_MP_TEST        0xc2
#define TESTID_MP_READ_TEST   0xc3
#define TESTID_READ_BTADDR    0xc4
#define TESTID_PWR_TEST       0xc5
#define TESTID_RSSI_TEST      0xc6
#define TESTID_MODIFY_BTADDR2 0xc7

//Test ID, UUT needs wait PC reply forever
#define TESTID_ID_WAIT        0xfffe

//Test ID, UUT can end test.
#define TESTID_ID_QUIT        0xffff

#define __ENTRY_CODE __attribute__((section(".entry")))
typedef enum
{
    TEST_MODE_CARD = 0,
    TEST_MODE_USB,
    TEST_MODE_UART,
    TEST_MODE_EXTEND
}test_mode_e;

enum{
    STATUS_NULL,
    STATUS_START,
    STATUS_ONGING,
};

typedef enum
{
    TEST_PASS = 0,
    TEST_BT_FAIL,
    TEST_MIC_FAIL,
    TEST_LINEIN_FAIL,
    TEST_UHOST_FAIL,
    TEST_FM_FAIL,
    TEST_IR_FAIL,
    TEST_GPIO_FAIL,
    TEST_GPIO_INIT_FAIL,
    TEST_GPIOA_SHORT_GND,
    TEST_GPIOA_OPEN,
    TEST_GPIOB_SHORT,
    TEST_GPIOB_SHORT_VCC,
    TEST_GPIOB_SHORT_GND,
    TEST_GPIOB_OPEN,
    TEST_GPIOSIO_SHORT,
    TEST_GPIOSIO_SHORT_VCC,
    TEST_GPIOSIO_SHORT_GND,
    TEST_GPIOSIO_OPEN,
    TEST_KEY_FAIL,
    TEST_SDCARD_FAIL,
    TEST_FW_ERROR,
    TEST_FW_VERSION_FAIL,
	TEST_FAIL,
}test_result_e;

typedef struct
{
    uint8 bt_name[TEST_BTNAME_MAX_LEN];
}bt_name_arg_t;


typedef struct
{
    uint8 bt_ble_name[TEST_BTBLENAME_MAX_LEN];
}ble_name_arg_t;

typedef struct
{
    uint8 bt_addr[6];
    uint8 bt_addr_add_mode;
    uint8 bt_burn_mode;
    uint8 reserved;
}bt_addr_arg_t;

typedef struct
{
    uint8 bt_transmitter_addr[6];
    uint8 bt_test_mode;
    uint8 bt_fast_mode;
}btplay_test_arg_t;

typedef struct
{
    uint32 freq;
}fmplay_test_arg_t;

typedef struct
{
    uint32 gpioA_value;         //GPIO0-GPIO31
    uint32 gpioB_value;         //GPIO32-GPIO63
	uint32 gpioC_value;    //pc input
}gpio_test_arg_t;

typedef struct
{
    uint8 cfo_channel[3];
    uint8 cfo_test;
    uint8 cfo_index_low;
    uint8 cfo_index_high;
    uint8 cfo_index_changed;
    int8 cfo_threshold_low;
    int8 cfo_threshold_high;
    uint8 cfo_write_mode;
    uint8 reserved[2];
    int32  cfo_upt_offset;
    uint8 pwr_test;
    int8 pwr_threshold_low;
    int8 pwr_threshold_high;
    int16 ref_cap;
}mp_test_arg_t4;

typedef struct
{
    uint8 cfo_channel[3];
    uint8 cfo_test;
    int16 ref_cap;
    uint8 cfo_index_low;
    uint8 cfo_index_high;
    uint8 cfo_index_changed;
    int8 cfo_threshold_low;
    int8 cfo_threshold_high;
    uint8 cfo_write_mode;
    //uint8 reserved[2];
    int32  cfo_upt_offset;
    uint8 ber_test;
    uint32 ber_threshold_low;
    uint32 ber_threshold_high;
    uint8 rssi_test;
    int16 rssi_threshold_low;
    int16 rssi_threshold_high;
}mp_test_arg_t;

typedef struct
{
    uint8 cmp_btname_flag;
    uint8 cmp_btname[TEST_BTNAME_MAX_LEN];
    uint8 cmp_blename_flag;
    uint8 cmp_blename[TEST_BTBLENAME_MAX_LEN];
}read_btname_test_arg_t;
typedef struct
{
    uint8 cmp_fw_version_flag;
    uint8 cmp_fw_version[TEST_BTBLENAME_MAX_LEN];
}read_fw_version_test_arg_t;

typedef struct
{
    uint8  stub_head[6];
    uint16 test_id;
    uint8  test_result;
    uint8  timeout;
    uint16  return_arg[0];
}return_result_t;

typedef struct
{
    uint16 magic;
    uint8 bt_addr[6];
    uint8 burn_lock_flag;
    uint8 remote_name[MGR_MAX_REMOTE_NAME_LEN];
    uint8 device_name[BT_MAC_NAME_LEN_MAX];
    uint8 device_fix_pin_code[16];
    uint8 ble_device_name[BT_BLE_NAME_LEN_MAX];
}bt_addr_vram_t;

/** ATF input parameters.
*/
/*
typedef struct
{
    uint8 ber_channel[3];
    int8  ber_thr_low;
    int8  ber_thr_high;
    int8  rssi_thr_low;
    int8  rssi_thr_high;
}ber_test_arg_t;
*/

typedef struct
{
    uint8 pwr_channel[3];
//    int8  pwr_thr_low;
//    int8  pwr_thr_high;
    int8  pwr_thr_low;
    int8  pwr_thr_high;
    int16 rssi_val;
}pwr_test_arg_t;

typedef struct
{
    uint8 lradc1_test;
    uint8 lradc1_thr_low;
    uint8 lradc1_thr_high;
    uint8 lradc2_test;
    uint8 lradc2_thr_low;
    uint8 lradc2_thr_high;
    uint8 lradc4_test;
    uint8 lradc4_thr_low;
    uint8 lradc4_thr_high;
}lradc_test_arg_t;

typedef struct
{
    uint8 test_left_ch;
    uint8 test_right_ch;
    uint8 test_left_ch_SNR;
    uint8 test_right_ch_SNR;
    uint32 left_ch_power_threadshold;
    uint32 right_ch_power_threadshold;
    uint32 left_ch_SNR_threadshold;
    uint32 right_ch_SNR_threadshold;
    uint16 left_ch_max_sig_point;
    uint16 right_ch_max_sig_point;
}channel_test_arg_t;

typedef struct
{
    u8_t type;
    u8_t opcode;
    u8_t sub_opcode;
    u8_t reserved;
    u16_t payload_len;
}stub_ext_cmd_t;

typedef struct
{
    stub_ext_cmd_t  cmd;
    uint8 log_data[0];
}print_log_t;

struct att_env_var {
	u8_t test_mode;
	u8_t  *rw_temp_buffer;
	u8_t  *return_data_buffer;
	u8_t  *skip_product_test;
	u16_t  *line_buffer;
	u8_t  *arg_buffer;
    u16_t test_id;
    u16_t last_test_id;
    u16_t arg_len;
	att_interface_api_t *api;
	att_interface_dev_t *dev_tbl;
};

static inline void k_sem_reset(struct k_sem *sem)
{
	sem->count = 0;
}

/* Used by _bss_zero or arch-specific implementation */
extern char __bss_start[];
extern char __bss_end[];

//stub send buffer, 256 bytes
//#define STUB_ATT_RW_TEMP_BUFFER     (0x00069000)//(0x9fc3a000)
//extern u8_t *STUB_ATT_RW_TEMP_BUFFER;
extern uint8_t *STUB_ATT_RW_TEMP_BUFFER;

extern struct att_env_var *self;
extern return_result_t *return_data;
extern u16_t trans_bytes;

#define STUB_ATT_RW_TEMP_BUFFER		((u8_t *)0x60000)

#define STUB_ATT_RW_TEMP_BUFFER_LEN (256)

#define STUB_ATT_RETURN_DATA_BUFFER (STUB_ATT_RW_TEMP_BUFFER + STUB_ATT_RW_TEMP_BUFFER_LEN)

#define STUB_ATT_RETURN_DATA_LEN    (256)

//#define STUB_ONCE_BANK_READ_SIZE    (1024)

//#define STUB_ATT_READ_BANK_BUFFER   (STUB_ATT_RETURN_DATA_BUFFER + STUB_ATT_RETURN_DATA_LEN)

//#define ATT_MPDATA_TEMP_BUFFER      (STUB_ATT_READ_BANK_BUFFER + STUB_ONCE_BANK_READ_SIZE + 8)

//#define ATT_MPDATA_MEX_LENGTH       (0x800)


//#define LOG_FILE_BUFFER             (ATT_MPDATA_TEMP_BUFFER + ATT_MPDATA_MEX_LENGTH)//(0x0006a800)

//#define LOG_FILE_LEN                (2*1024)  //0x0006b000

#define STUB_BUFFER_TEST_ITEM_ADDR  (STUB_ATT_RETURN_DATA_BUFFER + STUB_ATT_RETURN_DATA_LEN)

extern void bytes_to_unicode(uint8 *byte_buffer, uint8 byte_index, uint8 byte_len, uint16 *unicode_buffer, uint16 *unicode_len);

extern void uint32_to_unicode(uint32 value, uint16 *unicode_buffer, uint16 *unicode_len, uint32 base);

extern void int32_to_unicode(int32 value, uint16 *unicode_buffer, uint16 *unicode_len, uint32 base);

extern void att_write_test_info(char *test_info, uint32 test_value, uint32 write_data);

extern int32 act_test_report_result(return_result_t *write_data, uint16 total_len);

extern int32 act_test_report_test_log(uint32 ret_val, uint32 test_id);

extern uint32 thd_test(void *buffer_addr, channel_test_arg_t *channel_test_arg);

extern u8_t att_get_test_mode(void);

extern uint8 *act_test_parse_test_arg(uint16 *line_buffer, uint8 arg_number, uint16 **start, uint16 **end);

extern int32 unicode_to_utf8_str(uint16 *start, uint16 *end, uint8 *utf8_buffer, uint32 utf8_buffer_len);

extern int32 unicode_to_uint8_bytes(uint16 *start, uint16 *end, uint8 *byte_buffer, uint8 byte_index, uint8 byte_len);

extern int32 unicode_to_int(uint16 *start, uint16 *end, uint32 base);

#endif
