/*******************************************************************************
 *                              US212A
 *                            Module: Picture
 *                 Copyright(c) 2003-2012 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>    <time>           <version >             <desc>
 *       zhangxs     2011-09-19 10:00     1.0             build this file
 *******************************************************************************/
#include "att_pattern_test.h"
#include "ap_autotest_loader.h"

uint8 g_cur_line_num;
autotest_test_info_t g_test_info;
uint8 g_skip_product_test;
struct att_env_var g_att_env_var;
uint8 att_cmd_arg_buffer[80];

static void test_init(void)
{
	self = &g_att_env_var;
    g_att_env_var.rw_temp_buffer = STUB_ATT_RW_TEMP_BUFFER;
	g_att_env_var.return_data_buffer = STUB_ATT_RETURN_DATA_BUFFER;
	g_att_env_var.line_buffer = (u16_t  *)(STUB_ATT_RW_TEMP_BUFFER + sizeof(stub_ext_cmd_t));
	g_att_env_var.arg_buffer = att_cmd_arg_buffer;
	g_att_env_var.skip_product_test = &g_skip_product_test;

    g_test_info.test_id = 0;

    g_cur_line_num = 1;

    self->test_mode = TEST_MODE_UART;
}


__ENTRY_CODE void pattern_main(void *p1, void *p2, void *p3)
{
	void (*exit_routinue)(void);

	memset(&__bss_start, 0,
		 ((u32_t) &__bss_end - (u32_t) &__bss_start));

    memset(STUB_ATT_RW_TEMP_BUFFER, 0x00, 0x1000);

	g_att_env_var.api = (att_interface_api_t *)p1;

	g_att_env_var.dev_tbl = (att_interface_dev_t*)p3;

	test_init();

    test_dispatch();
	exit_routinue = (void (*)(void))p2;

	exit_routinue();

    return;
}


