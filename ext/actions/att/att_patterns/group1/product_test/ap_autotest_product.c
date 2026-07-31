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
 * Author: dengcong<dengcong@actions-semi.com>
 *
 * Change log:
 *	2019/03/01: Created by dengcong.
 */

#include "att_pattern_test.h"

uint8 *p_skip_product_test = NULL;
extern int att_flash_erase(off_t offset, size_t size);

void reboot_to_card_product(void)
{
    int ret;

    //erase mbrec 0
    ret = att_flash_erase(0x00, 0x2000);
    if(ret)
    {
        print_log("erase mbrec 0 failed\n");
        return;
    }

    //erase mbrec 1
    ret = att_flash_erase(0x10000, 0x2000);
    if(ret)
    {
        print_log("erase mbrec 1 failed\n");
        return;
    }

    print_log("erase mbrec and reboot\n");
    sys_pm_reboot(0);
}


/*
 * MP test.
 */
test_result_e act_test_product_test(void *arg_buffer)
{
    print_log("product test\n");
    //Not excuted logically.
	if (*p_skip_product_test == FALSE)
    {
        print_log("reboot to card_product\n");
        reboot_to_card_product();
    }
    // else
    // {
    //     sys_pm_reboot(0);
    // }

    return TEST_PASS;
}

test_result_e act_test_prepare_product(void *arg_buffer)
{
    return_result_t *return_data;

    print_log("prepare product test\n");

    if(*p_skip_product_test == FALSE)
    {
        print_log("reboot to card_product\n");
        reboot_to_card_product();
    }

    return_data = (return_result_t *) (STUB_ATT_RETURN_DATA_BUFFER);

    return_data->test_id = TESTID_PREPARE_PRODUCT_TEST;

    return_data->test_result = 1;

    act_test_report_result(return_data, 4);

    return TEST_PASS;
}

bool __ENTRY_CODE pattern_main(struct att_env_var *para)
{
    bool ret_val = false;

    memset(&__bss_start, 0,
            ((u32_t)&__bss_end - (u32_t)&__bss_start));

    self = para;
    return_data = (return_result_t *)(self->return_data_buffer);
	p_skip_product_test = self->skip_product_test;
    // act_test_read_pwr_test_arg(self->line_buffer, self->arg_buffer, self->arg_len);
    ret_val = act_test_prepare_product(self->arg_buffer);

    return ret_val;
}
