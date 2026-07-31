#ifndef __AP_AUTOTEST_LOADER_H
#define __AP_AUTOTEST_LOADER_H

#define ATF_MAX_SUB_FILENAME_LENGTH  12

typedef struct
{
    uint8  test_state;
    uint8 reserved;
    uint16 test_id;
    uint16 arg_len;
    u32_t run_addr;
    char last_pt_name[13]; //8+1+3+'\0'
}autotest_test_info_t;

struct att_code_info {
    unsigned char testid;
    unsigned char type;
    unsigned char need_drv;
    const char    pt_name[13]; //8+1+3+'\0'
};

typedef struct
{
    stub_ext_cmd_t ext_cmd;
    uint8 dut_connect_mode;
    uint8 dut_work_mode;
    uint8 timeout;
    uint8 reserved;
    uint8 bdaddr[6];
    uint8 reserved2[22];
}att_start_arg_t;

typedef struct
{
    uint16 last_test_id;
    uint8 reserved[30];
}pc_test_info_t;

typedef struct
{
    stub_ext_cmd_t ext_cmd;
    pc_test_info_t pc_test_info;
}att_pc_test_info_t;


typedef enum
{
    //DUT connect to PC derectly
    DUT_CONNECT_MODE_DIRECT = 0,
    //DUT -> UDA transfer -> PC
    DUT_CONNECT_MODE_UDA,
}dut_connect_state_e;

typedef enum
{
    //DUT normal work mode, stop when test fails.
    DUT_WORK_MODE_NORMAL = 0,
    //DUT test fails and continue next test item.
    DUT_WORK_MODE_SPECIAL = 1,
}dut_work_mode_e;

extern uint8 g_cur_line_num;

extern uint8 g_skip_product_test;

extern autotest_test_info_t g_test_info;

extern struct att_env_var g_att_env_var;

extern int32 act_test_start_deal(void);

extern void test_dispatch(void);

#endif
