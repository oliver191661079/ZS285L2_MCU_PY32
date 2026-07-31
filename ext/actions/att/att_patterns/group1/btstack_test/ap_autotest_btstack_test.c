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
/*!
 * \file     user1_main.c
 * \brief    picture main module. Initialize, quit and shedule.
 * \author   zhangxs
 * \version  1.0
 * \date  2011/9/05
 *******************************************************************************/
#include <att_pattern_test.h>
#include "test_bt_stack_common.h"

#define BTTEST_STATUS_NULL              0x0000
#define BTTEST_ACL_CONNECTED            0x0001
#define BTTEST_A2DP_CONNECTED           0x0002
#define BTTEST_A2DP_STARTED             0x0004
#define BTTEST_HFP_CONNECTED            0x0008
#define BTTEST_SCO_CONNECTED            0x0010
#define BT_TEST_TIMEOUT                 90000

bool att_bttest_start(void)
{
    return att_cmd_send(STUB_CMD_ATT_BT_START, 3);
}

bool att_bttest_stop(void)
{
    return att_cmd_send(STUB_CMD_ATT_BT_STOP, 3);
}

bool att_bttest_a2dp_start(void)
{
    return att_cmd_send(STUB_CMD_ATT_BT_A2DP, 3);
}

bool att_bttest_sco_start(void)
{
    return att_cmd_send(STUB_CMD_ATT_BT_SCO, 3);
}

u32_t check_bt_status(uint8_t *addr)
{
	int ret;

	struct bt_dev_rdm_state state;

	memcpy(&state.addr, addr, sizeof(state.addr));

	ret = bt_manager_get_rdm_state(&state);

    if(ret == 0)
    {
        if(state.acl_connected)
        {
            ret |= BTTEST_ACL_CONNECTED;
        }
        if(state.a2dp_connected)
        {
            ret |= BTTEST_A2DP_CONNECTED;
        }
        if(state.a2dp_stream_started)
        {
            ret |= BTTEST_A2DP_STARTED;
        }
        if(state.hfp_connected)
        {
            ret |= BTTEST_HFP_CONNECTED;
        }
        if(state.sco_connected)
        {
            ret |= BTTEST_SCO_CONNECTED;
        }
    }else{
        ret = 0;
    }

    return ret;
}

bool bttest_status_judge(uint8_t *addr, u32_t status)
{
    u32_t ret = BTTEST_STATUS_NULL;

    ret = check_bt_status(addr);

    if((ret & status) ==  status)
    {
        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

extern void bt_manager_set_not_linkkey_connect(void);
bool bt_test_profile_mode(u8_t test_mode, u8_t *addr)
{
    uint32 ret_val = FALSE;

    uint32 start_time;
    start_time = k_uptime_get_32();

    //Use test parameters of hannel test to test audio data  from BT rx.
    //Transfer needs send 1KHz sin audio
	//bt_manager_set_not_linkkey_connect();
    while(k_uptime_get_32() - start_time < BT_TEST_TIMEOUT)
    {
        if(test_mode == 0 && bttest_status_judge(addr, BTTEST_ACL_CONNECTED))
        {
            return  TRUE;
        }
        else if(test_mode == 1 && bttest_status_judge(addr, BTTEST_A2DP_CONNECTED))
        {
            ret_val = att_bttest_a2dp_start();
            if(ret_val == FALSE)
            {
                print_log("remote a2dp start fail\n");
                return FALSE;
            }

            while(k_uptime_get_32() - start_time < BT_TEST_TIMEOUT)
            {
                k_sleep(100);

                if(bttest_status_judge(addr, BTTEST_A2DP_STARTED))
                {
                    return TRUE;
                }
            }
        } else if(test_mode == 2 && bttest_status_judge(addr, BTTEST_HFP_CONNECTED))
        {
            ret_val = att_bttest_sco_start();
            if(ret_val == FALSE)
            {
                print_log("remote sco start fail\n");
                return FALSE;
            }

            while(k_uptime_get_32() - start_time < BT_TEST_TIMEOUT)
            {
                k_sleep(100);
                bt_manager_check_accept_call(NULL);
                if(bttest_status_judge(addr, BTTEST_SCO_CONNECTED))
                {
                    return TRUE;
                }
            }
        }
        else if(test_mode == 3 && bttest_status_judge(addr, BTTEST_A2DP_CONNECTED | BTTEST_HFP_CONNECTED))
        {
            ret_val = att_bttest_a2dp_start();
            if(ret_val == FALSE)
            {
                print_log("remote a2dp start fail\n");
                return FALSE;
            }

            while(k_uptime_get_32() - start_time < BT_TEST_TIMEOUT)
            {
                k_sleep(100);

                if(bttest_status_judge(addr, BTTEST_A2DP_STARTED))
                {
                    ret_val = att_bttest_sco_start();
                    if(ret_val == FALSE)
                    {
                        print_log("remote sco start fail\n");
                        return FALSE;
                    }

                    while(k_uptime_get_32() - start_time < BT_TEST_TIMEOUT)
                    {
                        k_sleep(100);
						bt_manager_check_accept_call(NULL);

                        if(bttest_status_judge(addr, BTTEST_SCO_CONNECTED))
                        {
                            return TRUE;
                        }
                    }
                }
            }
        }
    }

    print_log("bt test timeout\n");

    return FALSE;
}

uint32 test_bt_manager_loop_deal(btplay_test_arg_t *btplay_test_arg)
{
    uint32 ret_val;
    uint8_t *addr = btplay_test_arg->bt_transmitter_addr;
    uint8_t  test_mode = btplay_test_arg->bt_test_mode;

    print_log("bt_stack test_mode  %d\n",test_mode);
    ret_val = bt_test_profile_mode(test_mode, addr);

    if (ret_val == FALSE)
    {
        att_write_test_info("btplay test failed", 0, 0);
    }
    else
    {
        att_write_test_info("btplay test ok", 0, 0);
    }

    return ret_val;
}

test_result_e act_test_bt_test(void *arg_buffer)
{
    int32 ret_val;
    return_result_t *return_data;
    uint16 trans_bytes = 0;

    btplay_test_arg_t *btplay_test_arg = (btplay_test_arg_t *) arg_buffer;

    ret_val = att_bttest_start();
    if(ret_val == FALSE)
    {
        print_log("bttool start fail\n");
        goto test_end;
    }
    bt_manager_install_stack(btplay_test_arg->bt_transmitter_addr, btplay_test_arg->bt_test_mode);

    ret_val = test_bt_manager_loop_deal(btplay_test_arg);

    bt_manager_uninstall_stack(btplay_test_arg->bt_transmitter_addr, btplay_test_arg->bt_test_mode);

    if(ret_val == FALSE)
    {
        print_log("bt test fail\n");
        att_bttest_stop();
        goto test_end;
    }

    ret_val = att_bttest_stop();
    if(ret_val == FALSE)
    {
        print_log("bttool stop fail\n");
        goto test_end;
    }

test_end:

    if (att_get_test_mode() != TEST_MODE_CARD)
    {
        return_data = (return_result_t *) (STUB_ATT_RETURN_DATA_BUFFER);

        return_data->test_id = TESTID_BT_TEST;

        return_data->test_result = ret_val;

        bytes_to_unicode(btplay_test_arg->bt_transmitter_addr, 5, 6, return_data->return_arg, &trans_bytes);

        //To add pararmeter delimete ','
        return_data->return_arg[trans_bytes++] = 0x002c;

        bytes_to_unicode(&(btplay_test_arg->bt_test_mode), 0, 1, &(return_data->return_arg[trans_bytes]), &trans_bytes);

        //To add pararmeter delimete ','
        return_data->return_arg[trans_bytes++] = 0x002c;

        bytes_to_unicode(&(btplay_test_arg->bt_fast_mode), 0, 1, &(return_data->return_arg[trans_bytes]), &trans_bytes);

        //To add string teminator
        return_data->return_arg[trans_bytes++] = 0x0000;

        //4 bytes alignment
        if ((trans_bytes % 2) != 0)
        {
            return_data->return_arg[trans_bytes++] = 0x0000;
        }

        act_test_report_result(return_data, trans_bytes * 2 + 4);
    }
    else
    {
        act_test_report_test_log(ret_val, TESTID_BT_TEST);
    }

    return ret_val;
}

int32 act_test_read_btplay_arg(uint16 *line_buffer, uint8 *arg_buffer, uint32 arg_len)
{
    uint16 *start = 0;
    uint16 *end = 0;
    uint8 arg_num = 1;

    btplay_test_arg_t *btplay_arg = (btplay_test_arg_t *) arg_buffer;

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    //to read 6 bytes BT address
    unicode_to_uint8_bytes(start, end, btplay_arg->bt_transmitter_addr, 5, 6);

    //In card test mode, it needs to skip 5 parameters of BT addresses, take the later parmater.
    //In PC test mode, parameters are assigned by PC, no need to handle.
    if (att_get_test_mode() == TEST_MODE_CARD)
    {
        arg_num += 5;
    }
    else
    {

    }

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    btplay_arg->bt_test_mode = unicode_to_int(start, end, 16);

    if (att_get_test_mode() != TEST_MODE_CARD)
    {
        act_test_change_test_timeout(90);
    }

    return TRUE;
}

bool __ENTRY_CODE pattern_main(struct att_env_var *para)
{
    bool ret_val = false;

	memset(&__bss_start, 0,
		 ((u32_t) &__bss_end - (u32_t) &__bss_start));

	self = para;
	return_data = (return_result_t *) (self->return_data_buffer);
	trans_bytes = 0;

	if (self->arg_len){
		act_test_read_btplay_arg(self->line_buffer, self->arg_buffer, self->arg_len);
	}
	ret_val = act_test_bt_test(self->arg_buffer);

    return ret_val;
}


