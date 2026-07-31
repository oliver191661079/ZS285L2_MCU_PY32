#ifndef _TEST_BTSTACK_COMMON_H
#define _TEST_BTSTACK_COMMON_H

#define HFP_SUPPORTED           0x01
#define A2DP_SUPPORTED          0x02
#define AVRCP_SUPPORTED         0x04
#define PROFILE_VALID           0x80
#define LINKKEY_VALID           0x40


typedef struct
{
	u8_t    remote_addr[6];
	u8_t    support_profile;
} bt_paired_dev_info_t;

typedef struct
{
	u8_t    remote_addr[6];
	u8_t    support_profile;
	u8_t    linkkey_type;
	u8_t    linkkey[16];
	u8_t    local_addr[6];
    u8_t    remote_name[16];
	u8_t    reserved[6];
} bt_paired_dev_info2_t;

typedef enum
{
    TEST_CONN_STATUS_NONE,
    TEST_CONN_STATUS_WAIT_PAIR,
    TEST_CONN_STATUS_PAIRED,
    TEST_CONN_STATUS_LINKED,
    TEST_CONN_STATUS_ERROR
} test_conn_status_e;

typedef enum
{
    TEST_BT_ERR_NONE,
    TEST_BT_ERR_HW,
    TEST_BT_ERR_PAGE_TIMEOUT,
    TEST_BT_ERR_CONNECTION_TIMEOUT,
    TEST_BT_ERR_LINKEY_MISS,
    TEST_BT_ERR_NOT_POPUP_WINDOW
} test_bt_err_status_e;

typedef struct
{
    u8_t conn_status;
    u8_t err_status;
    u8_t a2dp_status;
    u8_t hfp_status;
    u8_t num_connected;
    u8_t support_profile;
} test_btstack_status_t;


typedef struct
{
    uint8 connect_mode ;  //0-passive; 1-active
    uint8 addr_valid;
    uint16 timeout;       //in millisecond
    uint8 addr_s[6];       // start address
    uint8 addr_e[6];       // end sddress
    uint8 addr_sender[6];
    uint8 reserved[16];
}bttools_app_param_t;

#endif
