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


//When get test ID is 0xffff, UUT should not send any data to ATT tool to avoid ATT no response.
int32 act_test_read_testid(uint8 *arg_buffer, uint32 arg_len)
{
    int ret_val;
    uint8 *cmd_data;
    uint8 temp_data[8];

    if(att_get_test_mode() != TEST_MODE_CARD)
    {
        cmd_data = (uint8*)STUB_ATT_RW_TEMP_BUFFER;

        //Timeout in second.
        cmd_data[6] = 20;
        cmd_data[7] = 0;
        cmd_data[8] = 0;
        cmd_data[9] = 0;

        ret_val = att_write_data(STUB_CMD_ATT_GET_TEST_ID, 4, STUB_ATT_RW_TEMP_BUFFER);

        memset(STUB_ATT_RW_TEMP_BUFFER, 0x00, 20);
        g_test_info.test_id = 0;
        g_test_info.arg_len = 0;

        if(ret_val == 0)
        {
            ret_val = att_read_data(STUB_CMD_ATT_GET_TEST_ID, 4, STUB_ATT_RW_TEMP_BUFFER);

            if(ret_val == 0)
            {
                cmd_data = (uint8 *)STUB_ATT_RW_TEMP_BUFFER;

                g_test_info.test_id = (cmd_data[6] | (cmd_data[7] << 8));

                g_test_info.arg_len = (cmd_data[8] | (cmd_data[9] << 8));

				//TESTID, TESTID_ID_QUIT no reply ACK
                if(g_test_info.test_id != TESTID_ID_QUIT)
                {
                    //Reply ACK
                    ret_val = att_write_data(STUB_CMD_ATT_ACK, 0, STUB_ATT_RW_TEMP_BUFFER);
                }

                if((g_test_info.test_id == TESTID_PRODUCT_TEST)
                    || (g_test_info.test_id == TESTID_FLASHTEST)
                    || (g_test_info.test_id == TESTID_PWR_TEST)
                    || (g_test_info.test_id == TESTID_RSSI_TEST)
                    || (g_test_info.test_id == TESTID_MP_TEST)
                    || (g_test_info.test_id == TESTID_MP_READ_TEST))
                {
                    act_test_change_test_timeout(60);
                }
            }
        }

        printk("\narg len :%d ,id: %d\n", g_test_info.arg_len, g_test_info.test_id);

        if(g_test_info.arg_len != 0)
        {
            ret_val = att_write_data(STUB_CMD_ATT_GET_TEST_ARG, 0, STUB_ATT_RW_TEMP_BUFFER);

            if(ret_val == 0)
            {
                ret_val = att_read_data(STUB_CMD_ATT_GET_TEST_ARG, g_test_info.arg_len, STUB_ATT_RW_TEMP_BUFFER);

                if(ret_val == 0)
                {
                    //Avoid using STUB_ATT_RW_TEMP_BUFFER, as it's required in parameters parsing in later phase.
                    ret_val = att_write_data(STUB_CMD_ATT_ACK, 0, temp_data);
                }
            }
         }
        if(g_test_info.test_id == 0)
        {
            if(g_att_env_var.last_test_id == TESTID_BT_TEST){
                g_test_info.test_id = TESTID_ID_QUIT;
                return 0;
            }else{
                return -1;
            }
        }
    }
    else
    {
        ret_val = 0;

        //act_test_read_test_info(g_cur_line_num, &g_test_info.test_id, arg_buffer, arg_len);

        if(g_test_info.test_id != 0xff)
        {
            g_cur_line_num++;
        }
    }
    return ret_val;
}

static const struct att_code_info att_patterns_list[] =
{
	{ TESTID_MODIFY_BTNAME,    0, 0, "p01_bta.bin" },
	{ TESTID_MODIFY_BLENAME,   0, 0, "p01_bta.bin" },
	{ TESTID_MODIFY_BTADDR,    0, 0, "p01_bta.bin" },
	{ TESTID_MODIFY_BTADDR2,   0, 0, "p01_bta.bin" },

	{ TESTID_READ_BTNAME,      0, 0, "p02_bta.bin" },
	{ TESTID_READ_BTADDR,      0, 0, "p02_bta.bin" },
	{ TESTID_READ_FW_VERSION,  0, 0, "p02_bta.bin" },

	{ TESTID_BQBMODE,          0, 0, "p03_bqb.bin" },
//	{ TESTID_FCCMODE,          0, 1, "p04_fcc.bin" },

	{ TESTID_MP_TEST,          0, 1, "p05_rx.bin" },
	{ TESTID_MP_READ_TEST,     0, 1, "p05_rx.bin" },

	{ TESTID_PWR_TEST,         0, 1, "p06_tx.bin" },

	{ TESTID_BT_TEST,          0, 0, "p07_btl.bin" },
//	{ TESTID_LED_TEST,         0, 0, "p10_led.bin" },
//	{ TESTID_KEY_TEST,         0, 0, "p11_key.bin" },
//	{ TESTID_CAP_CTR_START,    0, 0, "p12_bat.bin" },
//	{ TESTID_CAP_TEST,         0, 0, "p12_bat.bin" },

	{ TESTID_GPIO_TEST,        0, 0, "p18_gpio.bin" },
//	{ TESTID_AGING_PB_START,   0, 0, "p19_age.bin" },
//	{ TESTID_AGING_PB_CHECK,   0, 0, "p19_age.bin" },
	{ TESTID_LINEIN_CH_TEST_ATS28XX,   0, 0, "p20_adc.bin" },
	{ TESTID_MIC_CH_TEST,      0, 0, "p20_adc.bin" },
	{ TESTID_FM_CH_TEST,       0, 0, "p20_adc.bin" },
    { TESTID_PREPARE_PRODUCT_TEST,   0, 0, "p21_mpd.bin" },

//	{ TESTID_USER_RESERVED0,   0, 0, "p40_user.bin" },
//	{ TESTID_USER_RESERVED1,   0, 0, "p41_user.bin" },
//	{ TESTID_USER_RESERVED2,   0, 0, "p42_user.bin" },
//	{ TESTID_USER_RESERVED3,   0, 0, "p43_user.bin" },
//	{ TESTID_USER_RESERVED4,   0, 0, "p44_user.bin" },
//	{ TESTID_USER_RESERVED5,   0, 0, "p45_user.bin" },
//	{ TESTID_USER_RESERVED6,   0, 0, "p46_user.bin" },
//	{ TESTID_USER_RESERVED7,   0, 0, "p47_user.bin" },
};


static const struct att_code_info* match_testid(int testid)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(att_patterns_list); i++) {
		if (att_patterns_list[i].testid == testid)
			return &att_patterns_list[i];
	}

	return NULL;
}


int test_load_code(void)
{
	int ret_val;
    const struct att_code_info *cur_att_code_info;
    static int (*att_code_entry)(struct att_env_var *para) = NULL;
	atf_dir_t dir;

	/* load pattern */
	cur_att_code_info = match_testid(g_test_info.test_id);
	if (cur_att_code_info != NULL) {
		printk("old test %s new %s\n", g_test_info.last_pt_name, cur_att_code_info->pt_name);
		if (strncmp(g_test_info.last_pt_name, cur_att_code_info->pt_name, ATF_MAX_SUB_FILENAME_LENGTH) != 0) {
			ret_val = read_atf_sub_file(NULL, 0x10000, (const u8_t *)cur_att_code_info->pt_name, 0, -1, &dir);
			if (ret_val <= 0) {
				printk("read_atf_sub_file fail, quit!\n");
				goto err_deal;
			}

			memcpy(g_test_info.last_pt_name, cur_att_code_info->pt_name, sizeof(g_test_info.last_pt_name));

			att_code_entry = (int (*)(struct att_env_var *))((void *)(dir.run_addr));
		}

		if (att_code_entry != NULL) {

			print_log("code %s entry %x size %d\n", cur_att_code_info->pt_name, (u32_t)att_code_entry, dir.length);

			if (att_code_entry(&g_att_env_var) == false) {
				printk("att_code_entry return false, quit!\n");
				goto err_deal;
			}
		}
	} else {
		printk("match_testid fail, quit!\n");
		goto err_deal;
	}

	return 0;
err_deal:
	return -1;
}

void test_dispatch(void)
{
    int ret_val;
    uint8 att_cmd_temp_buffer[80];

    //att initialization, and send "start" command to UAD.
    act_test_start_deal();

	while (1)
	{
		ret_val = act_test_read_testid(att_cmd_temp_buffer, 80);
		if (ret_val != 0)
		{
			k_sleep(100);
			continue;
		}

		if (g_test_info.test_id == TESTID_ID_WAIT)
		{
			k_sleep(500);
			continue;
		}

		if (g_test_info.test_id == TESTID_ID_QUIT)
		{
			break;
		}

		print_log("test id:%d arg len %d", g_test_info.test_id, g_test_info.arg_len);

		g_att_env_var.test_id = g_test_info.test_id;
		g_att_env_var.arg_len = g_test_info.arg_len;
		if (test_load_code() != 0)
		{
			break;
		}
		g_att_env_var.last_test_id = g_att_env_var.test_id;
	}

	printk("test end\n");

}


