/*
 * Copyright (c) 2017 Actions Semi Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Author: johnsen<jmzhang@actions-semi.com>
 *
 * Change log:
 *	2019/5/28: Created by johnsen.
 */


#include "att_pattern_test.h"

int act_test_modify_bt_bqb_flag(u8_t flag)
{
	int ret = 0;
	u8_t ch = flag + '0';

	ret = set_nvram_db_data(DB_DATA_BT_RF_BQB_FLAG, &ch, 1);
	if (ret != 0) {
		ret = 0;
		printk("bqb flag write fail\n");
	} else {
		ret = 1;
	}

	return ret;
}

test_result_e act_test_enter_BQB_mode(void *arg_buffer)
{
    return_result_t *return_data;
    uint8 *cmd_data;
    int ret_val;
    uint16 test_id;
    int enable_val;

    return_data = (return_result_t *) (STUB_ATT_RETURN_DATA_BUFFER);

    return_data->test_id = TESTID_BQBMODE;

    if(get_BQB_info()){
        return_data->test_result = 1;
        enable_val = TEST_PASS;
    }else{
        print_log("BQB_mode is not enable\n");
        return_data->test_result = 0;
        enable_val = TEST_FAIL;
    }

    act_test_report_result(return_data, 4);

    att_read_data(STUB_CMD_ATT_GET_TEST_ID, 4, STUB_ATT_RW_TEMP_BUFFER);

    cmd_data = (uint8*)STUB_ATT_RW_TEMP_BUFFER;

    while(1)
    {
        //work timout in seconds.
        cmd_data[6] = 20;
        cmd_data[7] = 0;
        cmd_data[8] = 0;
        cmd_data[9] = 0;
        ret_val = att_write_data(STUB_CMD_ATT_GET_TEST_ID, 4, STUB_ATT_RW_TEMP_BUFFER);

        memset(STUB_ATT_RW_TEMP_BUFFER, 0x00, 20);

        if(ret_val == 0)
        {
            ret_val = att_read_data(STUB_CMD_ATT_GET_TEST_ID, 4, STUB_ATT_RW_TEMP_BUFFER);

            if(ret_val == 0)
            {
                cmd_data = (uint8 *)STUB_ATT_RW_TEMP_BUFFER;

                test_id = (cmd_data[6] | (cmd_data[7] << 8));

                if(test_id == TESTID_ID_WAIT)
                {
                    att_write_data(STUB_CMD_ATT_ACK, 0, STUB_ATT_RW_TEMP_BUFFER);
                    k_sleep(50);
                    continue;
                }
                else if(test_id == TESTID_ID_QUIT)
                {
                    break;
                }
                else
                {
                    att_write_data(STUB_CMD_ATT_ACK, 0, STUB_ATT_RW_TEMP_BUFFER);
                    print_log("bqb test error:id %d\n",test_id);
                    break;
                }
            }
        }
    }

    //enter_BQB_mode();
    //1-BQB mode, 2 - BLE BQB mode
    print_log("set bqb mode 2\n");
    if (act_test_modify_bt_bqb_flag(2)) {
        print_log("reboot as REBOOT_REASON_GOTO_BQB_ATT\n");
        sys_pm_reboot(7);
    }

    return enable_val;

}

bool __ENTRY_CODE pattern_main(struct att_env_var *para)
{
    bool ret_val = false;

	memset(&__bss_start, 0,
		 ((u32_t) &__bss_end - (u32_t) &__bss_start));

	self = para;
	return_data = (return_result_t *) (self->return_data_buffer);
	trans_bytes = 0;

	ret_val = act_test_enter_BQB_mode(self->arg_buffer);
    if(ret_val == TEST_PASS){
        return true;
    }else{
        return false;
    }
}

