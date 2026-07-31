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
 * \brief
 * \author   zhangxs
 * \version  1.0
 * \date  2011/9/05
 *******************************************************************************/
#define SYS_LOG_NO_NEWLINE
#define SYS_LOG_DOMAIN  "att read param"
#define SYS_LOG_LEVEL   ATT_SYS_LOG_LEVEL

#include "att_pattern_test.h"
#include <fw_version.h>

static uint32 _check_bt_addr_valid(uint8 *bt_addr)
{
    uint32 i;

    for (i = 0; i < 6; i++)
    {
        if ((bt_addr[i] != 0) && (bt_addr[i] != 0xff))
        {
            break;
        }
    }

    if (i == 6)
    {
        return FALSE;
    }
    else
    {
        return TRUE;
    }
}

//Used to read BT addr, supply to PC for check if address is correct.
test_result_e act_test_read_bt_addr(void *arg_buffer)
{
    uint32 i;
    uint8 cmd_data[16];
    int ret_val;
    return_result_t *return_data;
    uint16_t trans_bytes;
    uint32 bt_addr_valid = TRUE;

    bt_addr_vram_t bt_addr_vram;

    printk("\ntest read bt addr\n");

    ret_val = get_nvram_db_data(DB_DATA_BT_ADDR, bt_addr_vram.bt_addr, 6);

    if(ret_val != 0)
    {
        bt_addr_valid = FALSE;
    }

    if(bt_addr_valid == TRUE)
    {
        if (_check_bt_addr_valid(bt_addr_vram.bt_addr) == TRUE)
        {
            ret_val = 1;
        }
        else
        {
            ret_val = 0;
        }
    }
    else
    {
        ret_val = 0;
    }

    if (ret_val == 1)
    {
        for (i = 0; i < 6; i++)
        {
            cmd_data[i] = bt_addr_vram.bt_addr[i];
        }

        if(att_get_test_mode() != TEST_MODE_CARD)
        {
            print_log("bt addr: %x:%x:%x:%x:%x:%x\n", cmd_data[5], cmd_data[4], cmd_data[3], cmd_data[2], cmd_data[1],
                cmd_data[0]);
        }
        else
        {
            for (i = 0; i < 6; i++)
            {
                att_write_test_info("addr: ", cmd_data[5 - i], 1);
            }
        }
    }
    else
    {
        memset(cmd_data, 0, sizeof(cmd_data));
    }

    if (att_get_test_mode() != TEST_MODE_CARD)
    {
        return_data = (return_result_t *) (STUB_ATT_RETURN_DATA_BUFFER);

        return_data->test_id = TESTID_READ_BTADDR;

        return_data->test_result = ret_val;

        trans_bytes = 0;

        bytes_to_unicode(cmd_data, 5, 6, return_data->return_arg, &trans_bytes);

        return_data->return_arg[trans_bytes++] = 0x0000;

        //4 bytes alligment
        if ((trans_bytes % 2) != 0)
        {
            return_data->return_arg[trans_bytes++] = 0x0000;
        }

        act_test_report_result(return_data, trans_bytes * 2 + 4);
    }
    else
    {
        act_test_report_test_log(ret_val, TESTID_READ_BTADDR);
    }

    return ret_val;
}

static int32 utf8str_to_unicode(uint8 *utf8, int32 utf8Len, uint16 *unicode, uint32 *unicode_len)
{
    uint32 count = 0;
    uint8 c0, c1;
    uint32 scalar;

    while (--utf8Len >= 0)
    {
        c0 = *utf8;
        utf8++;

        if (c0 < 0x80)
        {
            *unicode = c0;

            if (*unicode == 0x00)
            {
                //count += 2;
                break;
            }
            unicode++;
            count += 2;
            continue;
        }

        /*Non ASCII, the firast byte of coding should NOT be 10xxxxxx*/
        if ((c0 & 0xc0) == 0x80)
        {
            return -1;
        }

        scalar = c0;
        if (--utf8Len < 0)
        {
            return -1;
        }

        c1 = *utf8;
        utf8++;
        /*the second byte of coding should be 10xxxxxx*/
        if ((c1 & 0xc0) != 0x80)
        {
            return -1;
        }
        scalar <<= 6;
        scalar |= (c1 & 0x3f);

        /* when 0 == r0&0x20, utf coding lenght is 2; else, the coding length > 2.*/
        if (!(c0 & 0x20))
        {
            if ((scalar != 0) && (scalar < 0x80))
            {
                return -1;
            }
            *unicode = (unsigned short) scalar & 0x7ff;
            if (*unicode == 0x00)
            {
                count += 2;
                break;
            }
            unicode++;
            count += 2;
            continue;
        }

        if (--utf8Len < 0)
        {
            return -1;
        }

        c1 = *utf8;
        utf8++;
        /*the third byte of coding should be 10xxxxxx*/
        if ((c1 & 0xc0) != 0x80)
        {
            return -1;
        }
        scalar <<= 6;
        scalar |= (c1 & 0x3f);

        /*when 0 == r0&0x20, utf coding lenght is 3; else, the coding length > 3. */
        if (!(c0 & 0x10))
        {
            if (scalar < 0x800)
            {
                return -1;
            }
            if ((scalar >= 0xd800) && (scalar < 0xe000))
            {
                return -1;
            }
            *unicode = (unsigned short) scalar & 0xffff;
            if (*unicode == 0x00)
            {
                count += 2;
                break;
            }
            unicode++;
            count += 2;
            continue;
        }

        return -1;
    }

    //4 bytes alignment
    if ((count % 4) != 0)
    {
        unicode++;
        count += 2;
        *unicode = 0x00;
    }

    *unicode_len = count;
    return 0;
}

test_result_e act_test_read_bt_name(void *arg_buffer)
{
    //uint32 i;

    int ret_val;

    read_btname_test_arg_t *read_btname_arg;

    bt_addr_vram_t bt_addr_vram;

    return_result_t *return_data;

    uint32 trans_bytes;

    uint32 count;

    uint32 bt_name_valid = TRUE;

    uint32 ble_name_valid = TRUE;

    read_btname_arg = (read_btname_test_arg_t *) arg_buffer;

    memset(&bt_addr_vram, 0, sizeof(bt_addr_vram));

    ret_val = get_nvram_db_data(DB_DATA_BT_NAME, bt_addr_vram.device_name, sizeof(bt_addr_vram.device_name));
    if(ret_val != 0)
    {
         bt_name_valid = FALSE;
    }

    ret_val = get_nvram_db_data(DB_DATA_BT_BLE_NAME, bt_addr_vram.ble_device_name, sizeof(bt_addr_vram.ble_device_name));
    if(ret_val != 0)
    {
        ble_name_valid = FALSE;
    }

    if(bt_name_valid == TRUE)
    {
        if (bt_addr_vram.device_name[0] != 0)
        {
            if(ble_name_valid == TRUE)
            {
                if (bt_addr_vram.ble_device_name[0] != 0)
                {
                    ret_val = TRUE;
                }
            }
        }
    }

	printk("%s bt name valid %d ble valid %d\n", __func__, bt_name_valid, ble_name_valid);

    if (ret_val == TRUE)
    {
        if(att_get_test_mode() != TEST_MODE_CARD)
        {
            print_log("read bt name: %s\n", bt_addr_vram.device_name);

            print_log("read ble name: %s\n", bt_addr_vram.ble_device_name);
        }
        else
        {
            att_write_test_info("read bt name: ", (uint32)bt_addr_vram.device_name, 1);

            att_write_test_info("read ble name: ",(uint32)bt_addr_vram.ble_device_name, 1);
        }
    }

    if (read_btname_arg->cmp_btname_flag == TRUE)
    {
        ret_val = !memcmp(bt_addr_vram.device_name, read_btname_arg->cmp_btname, strlen((const char *)bt_addr_vram.device_name) + 1);
    }

    if (ret_val == TRUE)
    {
        if (read_btname_arg->cmp_blename_flag == TRUE)
        {
            ret_val = !memcmp(bt_addr_vram.ble_device_name, read_btname_arg->cmp_blename,
                    strlen((const char *)bt_addr_vram.ble_device_name) + 1);
        }

    }

    if (att_get_test_mode() != TEST_MODE_CARD)
    {
        trans_bytes = 0;

        return_data = (return_result_t *) (STUB_ATT_RETURN_DATA_BUFFER);

        return_data->test_id = TESTID_READ_BTNAME;

        return_data->test_result = ret_val;

        //utf8->unicode may need moure buffer. It's max value is 2 times of string buffer before.
        utf8str_to_unicode(bt_addr_vram.device_name, sizeof(bt_addr_vram.device_name), return_data->return_arg, &count);

        trans_bytes += (count >> 1);

        //Add parameter delimeter','
        return_data->return_arg[trans_bytes++] = 0x002c;

        //utf8->unicode may need moure buffer. It's max value is 2 times of string buffer before.
        utf8str_to_unicode(bt_addr_vram.ble_device_name, sizeof(bt_addr_vram.ble_device_name), &(return_data->return_arg[trans_bytes]), &count);

        trans_bytes += (count >> 1);

        //4 bytes alignment
        if ((trans_bytes % 2) != 0)
        {
            return_data->return_arg[trans_bytes++] = 0x0000;
        }
        act_test_report_result(return_data, trans_bytes * 2 + 4);
    }
    else
    {
        act_test_report_test_log(ret_val, TESTID_READ_BTNAME);
    }

    return ret_val;
}


const struct fw_version *fw_version_get_current(void)
{
	const struct fw_version *ver =
		(struct fw_version *)soc_boot_get_fw_ver_addr();

	return ver;
}

void fw_version_dump(const struct fw_version *ver)
{
	printk("firmware version: 0x%x\n", ver->version_code);
	printk("system version: 0x%x\n",ver->system_version_code);
	printk("version name: %s\n", ver->version_name);
	printk("board name: %s\n", ver->board_name);
}


test_result_e act_test_read_fw_version(void *arg_buffer)
{
	const struct fw_version *version;

    return_result_t *return_data;

    uint32 trans_bytes;

    uint32 count;

    int ret_val = TRUE;

    read_fw_version_test_arg_t * read_fw_verison_arg = (read_fw_version_test_arg_t *) arg_buffer;

	version = fw_version_get_current();

	fw_version_dump(version);


    if (read_fw_verison_arg->cmp_fw_version_flag == TRUE)
    {
        ret_val = !memcmp(version->version_name, read_fw_verison_arg->cmp_fw_version, strlen((const char *)version->version_name) + 1);
    }else{
        /*When no comparing, use SUCCESS test as default, and PC display version derectly. */
        ret_val = TRUE;
    }


    if (att_get_test_mode() != TEST_MODE_CARD)
    {
        trans_bytes = 0;

        return_data = (return_result_t *) (STUB_ATT_RETURN_DATA_BUFFER);

        return_data->test_id = TESTID_READ_FW_VERSION;

        return_data->test_result = ret_val;

        //Add parameter delimeter','
        //return_data->return_arg[trans_bytes++] = 0x002c;

        utf8str_to_unicode((u8_t *)version->version_name, strlen((const char *)version->version_name), &(return_data->return_arg[trans_bytes]), &count);

        trans_bytes += (count >> 1);

        if ((trans_bytes % 2) != 0)
        {
            return_data->return_arg[trans_bytes++] = 0x0000;
        }
        act_test_report_result(return_data, trans_bytes * 2 + 4);
    }
    else
    {
        act_test_report_test_log(ret_val, TESTID_READ_FW_VERSION);
    }

    return ret_val;
}

int32 act_test_read_btname_arg(uint16 *line_buffer, uint8 *arg_buffer, uint32 arg_len)
{
    uint16 *start = 0;
    uint16 *end = 0;
    uint8 arg_num;

    read_btname_test_arg_t *read_btname_arg = (read_btname_test_arg_t *) arg_buffer;

    //if(arg_len < sizeof(mp_test_arg_t))
    //{
    //    DEBUG_ATT_PRINT("argument too long", 0, 0);
    //    while(1);
    //}

    arg_num = 1;

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    read_btname_arg->cmp_btname_flag = (uint8) unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    unicode_to_utf8_str(start, end, read_btname_arg->cmp_btname, TEST_BTNAME_MAX_LEN);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    read_btname_arg->cmp_blename_flag = (uint8) unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    unicode_to_utf8_str(start, end, read_btname_arg->cmp_blename, TEST_BTBLENAME_MAX_LEN);

    return TRUE;
}

int32 act_test_read_fw_version_arg(uint16 *line_buffer, uint8 *arg_buffer, uint32 arg_len)
{
    uint16 *start = 0;
    uint16 *end = 0;
    uint8 arg_num;

    read_fw_version_test_arg_t *read_fw_version_test_arg = (read_fw_version_test_arg_t *) arg_buffer;

    //if(arg_len < sizeof(mp_test_arg_t))
    //{
    //    DEBUG_ATT_PRINT("argument too long", 0, 0);
    //    while(1);
    //}

    arg_num = 1;

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    read_fw_version_test_arg->cmp_fw_version_flag= (uint8) unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    unicode_to_utf8_str(start, end, read_fw_version_test_arg->cmp_fw_version, TEST_BTNAME_MAX_LEN);

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


	if (self->test_id == TESTID_READ_BTADDR) {
		ret_val = act_test_read_bt_addr(self->arg_buffer);
	} else if (self->test_id == TESTID_READ_BTNAME) {
		act_test_read_btname_arg(self->line_buffer, self->arg_buffer, self->arg_len);
		ret_val = act_test_read_bt_name(self->arg_buffer);
	} else if (self->test_id == TESTID_READ_FW_VERSION) {
		act_test_read_fw_version_arg(self->line_buffer, self->arg_buffer, self->arg_len);
		ret_val = act_test_read_fw_version(self->arg_buffer);
	}else /*TESTID_FM_CH_TEST*/ {
		//ret_val = act_test_audio_fm_in(&g_fm_rx_test_arg);
	}

    return ret_val;
}

