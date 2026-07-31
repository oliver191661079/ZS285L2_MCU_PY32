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

#if 0
void read_bt_addr(uint8 *byte_buffer, uint32 read_mode)
{
    u8_t mac[6] = {0};
    int ret;

    ret = get_nvram_bt_addr(mac);

	if(ret == FALSE)
	{
	    memset(byte_buffer, 0x00, 6);
	    ACT_LOG_ID_ERR(ALF_STR_read_bt_addr__GET_BT_ADDR_FAILN, 0);
	}

	if (read_mode == 0)
    {
        for(int i = 0; i < 6; i++)
        {
            byte_buffer[i] = mac[5 - i];
        }
    }
    else
    {
        for(int i = 0; i < 6; i++)
        {
            byte_buffer[i] = mac[i];
        }
    }
}
#endif

int32 act_test_start_deal(void)
{
    int32 ret_val;
    //uint8 sdk_version[4];
    att_start_arg_t *att_start_arg;
    att_pc_test_info_t *att_pc_test_info;
    uint8_t try_count = 0;

    //g_test_base_time = k_uptime_get_32();

    if (att_get_test_mode() == TEST_MODE_CARD)
        return TRUE;

    att_start_arg = (att_start_arg_t *) STUB_ATT_RW_TEMP_BUFFER;

    att_pc_test_info = (att_pc_test_info_t *) STUB_ATT_RW_TEMP_BUFFER;

start:
    ret_val = att_write_data(STUB_CMD_ATT_READ_TESTINFO, 0, STUB_ATT_RW_TEMP_BUFFER);

    if (ret_val == 0)
    {
        ret_val = att_read_data(STUB_CMD_ATT_ACK, sizeof(pc_test_info_t), STUB_ATT_RW_TEMP_BUFFER);
    }
    else
    {
        printk("start deal error\n");
    }

    if(ret_val != 0 && ++try_count < 100)
    {
        k_sleep(200);
        goto start;
    }
    else if(ret_val != 0)
    {
        goto att_start;
    }


    //If previous test item is MP test, it means reboot from MP. Go next test directly.
    //Test success does not send "START" command.
    if (att_pc_test_info->pc_test_info.last_test_id == TESTID_PRODUCT_TEST)
    {
        /* Report MP success.
         */
        printk("report product result\n");

        return_result_t *return_data = (return_result_t *) (STUB_ATT_RETURN_DATA_BUFFER);

        return_data->test_id = TESTID_PRODUCT_TEST;
        return_data->test_result = 1;

        act_test_report_result(return_data, 4);

        g_skip_product_test = TRUE;

        return TRUE;
    }

att_start:
    memset(att_start_arg, 0x00, sizeof(att_start_arg_t));

    att_start_arg->dut_connect_mode = DUT_CONNECT_MODE_UDA;

    att_start_arg->dut_work_mode = DUT_WORK_MODE_NORMAL;

    att_start_arg->timeout = 5; //Get test item timeout
    att_start_arg->reserved = 0;

    g_skip_product_test = FALSE;

    ret_val = att_write_data(STUB_CMD_ATT_START, 32, STUB_ATT_RW_TEMP_BUFFER);

    if (ret_val == 0)
    {
        ret_val = att_read_data(STUB_CMD_ATT_ACK, 0, STUB_ATT_RW_TEMP_BUFFER);
        return TRUE;
    }
    else
    {
        k_sleep(100);
        goto att_start;
    }

    return FALSE;
}
