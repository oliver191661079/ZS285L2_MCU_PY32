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
#include "att_pattern_test.h"
/*
static void act_test_modify_bt_vram(void)
{
    #define ATT_MAGIC  0xABCD

    bt_addr_vram_t*  p = (void*)STUB_ATT_RETURN_DATA_BUFFER;

    if(g_support_norflash_wp == TRUE)
    {
        //BT address is changed by ATT
        p->magic = ATT_MAGIC;

        sys_vm_write(p, VM_BTSTACK, 4);
    }
}
*/
#define BT_ADDR_LOG_FILE_LEN   (512)

#define BT_ADDR_LOG_MAGIC_L    (0x44434241)

#define BT_ADDR_LOG_MAGIC_H    (0x48474645)
/*
static const uint8 att_record_filename[] =
{
    0xff, 0xfe,
    'a',  0x00,
    't',  0x00,
    't',  0x00,
    '_',  0x00,
    'r',  0x00,
    'e',  0x00,
    'c',  0x00,
    'o',  0x00,
    'r',  0x00,
    'd',  0x00,
    '.',  0x00,
    'b',  0x00,
    'i',  0x00,
    'n',  0x00,
    0x00, 0x00
};

static uint32 cal_att_file_checksum(btaddr_log_file_t *btaddr_log_file)
{
    uint32 checksum = 0;
    uint32 i;
    uint32 *pdata = (uint32 *) btaddr_log_file;

    for (i = 0; i < ((sizeof(btaddr_log_file_t) - 4) / 4); i++)
    {
        checksum += pdata[i];
    }

    return checksum;
}

static int32 write_param_data(nvram_param_rw_t *param_rw)
{
    int32 ret_val;

    ret_val = base_param_write(param_rw);

    if(ret_val != 0)
    {
        base_param_merge_deal();

        ret_val = base_param_write(param_rw);
    }

    return ret_val;
}*/
int act_test_report_modify_bt_addr_result(bt_addr_arg_t *bt_addr_arg)
{
    uint8 cmd_data[16] = {0};
    int ret_val;
    return_result_t *return_data;
    u16_t trans_bytes;

    ret_val = get_nvram_db_data(DB_DATA_BT_ADDR, cmd_data, 6);

    if(ret_val != 0){
        printk("bt addr read fail\n");
    }

    if (memcmp(bt_addr_arg->bt_addr, cmd_data, 6) != 0)
    {
        ret_val = 0;
    }
    else
    {
        ret_val = 1;
    }

    if (ret_val == 1)
    {
        att_write_test_info("modify bt addr ok", 0, 0);
    }
    else
    {
        att_write_test_info("modify bt addr failed", 0, 0);
    }

    att_write_test_info("byte 5:", cmd_data[5], 1);
    att_write_test_info("byte 4:", cmd_data[4], 1);
    att_write_test_info("byte 3:", cmd_data[3], 1);
    att_write_test_info("byte 2:", cmd_data[2], 1);
    att_write_test_info("byte 1:", cmd_data[1], 1);
    att_write_test_info("byte 0:", cmd_data[0], 1);

    if (att_get_test_mode() != TEST_MODE_CARD)
    {
        return_data = (return_result_t *) (STUB_ATT_RETURN_DATA_BUFFER);

        return_data->test_id = self->test_id;

        return_data->test_result = ret_val;

        trans_bytes = 0;

        bytes_to_unicode(cmd_data, 5, 6, return_data->return_arg, &trans_bytes);

        //Delimeter','
        return_data->return_arg[trans_bytes++] = 0x002c;

        bytes_to_unicode(&(bt_addr_arg->bt_addr_add_mode), 0, 1, &(return_data->return_arg[trans_bytes]), &trans_bytes);

        //Delimeter','
        return_data->return_arg[trans_bytes++] = 0x002c;

        bytes_to_unicode(&(bt_addr_arg->bt_burn_mode), 0, 1, &(return_data->return_arg[trans_bytes]), &trans_bytes);

        return_data->return_arg[trans_bytes++] = 0x0000;

        if ((trans_bytes % 2) != 0)
        {
            return_data->return_arg[trans_bytes++] = 0x0000;
        }

        act_test_report_result(return_data, trans_bytes * 2 + 4);
    }
    else
    {
        if (bt_addr_arg->bt_addr_add_mode == 0)
        {
            //act_test_write_att_record_file(&(bt_addr_arg->bt_addr[0]), 0, 0);
        }

        act_test_report_test_log(ret_val, self->test_id);
    }

    return ret_val;
}

test_result_e act_test_modify_bt_addr(void *arg_buffer)
{
    int ret_val = 0;

    bt_addr_arg_t *bt_addr_arg = (bt_addr_arg_t *) arg_buffer;

    ret_val = set_nvram_db_data(DB_DATA_BT_ADDR, bt_addr_arg->bt_addr, 6);
    if(ret_val != 0){
        printk("bt addr write fail\n");
		ret_val = false;
    }else{
		ret_val = true;
	}

    act_test_report_modify_bt_addr_result(bt_addr_arg);

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
                count += 2;
                break;
            }
            unicode++;
            count += 2;
            continue;
        }

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
        if ((c1 & 0xc0) != 0x80)
        {
            return -1;
        }
        scalar <<= 6;
        scalar |= (c1 & 0x3f);

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
        if ((c1 & 0xc0) != 0x80)
        {
            return -1;
        }
        scalar <<= 6;
        scalar |= (c1 & 0x3f);

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

    if ((count % 4) != 0)
    {
        unicode++;
        count += 2;
        *unicode = 0x00;
    }

    *unicode_len = count;
    return 0;
}

int act_test_report_modify_bt_name_result(bt_name_arg_t *bt_name_arg, uint16 write_len)
{
    uint8 cmd_data[TEST_BTNAME_MAX_LEN] = {0};
    int ret_val;
    return_result_t *return_data;
    uint32 trans_bytes = 0;

    ret_val = get_nvram_db_data(DB_DATA_BT_NAME, cmd_data, sizeof(bt_name_arg->bt_name));

    if(ret_val != 0)
    {
        printk("bt name read fail\n");
    }

    if (memcmp(bt_name_arg->bt_name, cmd_data, write_len) != 0)
    {
        ret_val = 0;
    }
    else
    {
        ret_val = 1;
    }

    if (ret_val == 1)
    {
        att_write_test_info("modify bt name ok", 0, 0);
    }
    else
    {
        att_write_test_info("modify bt name failed", 0, 0);
    }

    if (att_get_test_mode() != TEST_MODE_CARD)
    {

        return_data = (return_result_t *) (STUB_ATT_RETURN_DATA_BUFFER);

        return_data->test_id = TESTID_MODIFY_BTNAME;

        return_data->test_result = ret_val;

        utf8str_to_unicode(cmd_data, sizeof(cmd_data), return_data->return_arg, &trans_bytes);

        act_test_report_result(return_data, trans_bytes + 4);
    }
    else
    {
        act_test_report_test_log(ret_val, TESTID_MODIFY_BTNAME);
    }

    return ret_val;
}

test_result_e act_test_modify_bt_name(void *arg_buffer)
{
    int ret_val = 0;
    u8_t name_len;

    bt_name_arg_t *bt_name_arg = (bt_name_arg_t *) arg_buffer;

    printk("bt name: %s\n",(char *)bt_name_arg);

    name_len = strlen((const char *)bt_name_arg->bt_name) + 1;

    ret_val = set_nvram_db_data(DB_DATA_BT_NAME, bt_name_arg->bt_name, sizeof(bt_name_arg->bt_name));
    if(ret_val != 0)
    {
    	ret_val = 0;
        printk("bt name write fail\n");
    }else{
		ret_val = 1;
	}

    act_test_report_modify_bt_name_result(bt_name_arg, name_len);

    return ret_val;
}

void act_test_report_modify_ble_name_result(ble_name_arg_t *ble_name_arg, uint16 write_len)
{
    uint8 cmd_data[TEST_BTBLENAME_MAX_LEN];
    int ret_val;
    return_result_t *return_data;
    uint32 trans_bytes = 0;

    ret_val = get_nvram_db_data(DB_DATA_BT_BLE_NAME, cmd_data, sizeof(ble_name_arg->bt_ble_name));
    if(ret_val != 0)
    {
        printk("ble name read fail\n");
    }

    if (memcmp(ble_name_arg->bt_ble_name, cmd_data, write_len) != 0)
    {
        ret_val = 0;
    }
    else
    {
        ret_val = 1;
    }

    if (ret_val == 1)
    {
        att_write_test_info("modify ble name ok", 0, 0);
    }
    else
    {
        att_write_test_info("modify ble name failed", 0, 0);
    }

    att_write_test_info((char *)cmd_data, 0, 0);

    if (att_get_test_mode() != TEST_MODE_CARD)
    {
        return_data = (return_result_t *) (STUB_ATT_RETURN_DATA_BUFFER);

        return_data->test_id = TESTID_MODIFY_BLENAME;

        return_data->test_result = ret_val;
        utf8str_to_unicode(cmd_data, sizeof(cmd_data), return_data->return_arg, &trans_bytes);

        act_test_report_result(return_data, trans_bytes + 4);
    }
    else
    {
        act_test_report_test_log(ret_val, TESTID_MODIFY_BLENAME);
    }
}

test_result_e act_test_modify_bt_ble_name(void *arg_buffer)
{
    int ret_val = 0;
    u8_t name_len;

    ble_name_arg_t *ble_name_arg = (ble_name_arg_t *) arg_buffer;

    //SYS_LOG_DBG("ble name: %s\n",ble_name_arg->bt_ble_name);

    name_len = strlen((const char *)ble_name_arg->bt_ble_name) + 1;
    ret_val = set_nvram_db_data(DB_DATA_BT_BLE_NAME, ble_name_arg->bt_ble_name, sizeof(ble_name_arg->bt_ble_name));
    if(ret_val != 0)
    {
    	ret_val = 0;
        printk("ble name write fail\n");
    }else{
		ret_val = 1;
	}

    act_test_report_modify_ble_name_result(ble_name_arg, name_len);
    return ret_val;

}

static int32 act_test_read_bt_addr_arg(uint16 *line_buffer, uint8 *arg_buffer, uint32 arg_len)
{
    uint16 *start;
    uint16 *end;

    bt_addr_arg_t *bt_addr_arg = (bt_addr_arg_t *) arg_buffer;

    act_test_parse_test_arg(line_buffer, 1, &start, &end);

    //Read byte5, byte4, byte3, It's fixed bt address normally.
    unicode_to_uint8_bytes(start, end, bt_addr_arg->bt_addr, 5, 3);

    if (att_get_test_mode() == TEST_MODE_CARD)
    {
#if 0
        //In card test mode, get bt address from bt_addr.log firstly, then from config file.
        if (act_test_read_bt_addr_from_log(bt_addr_arg->bt_addr) == FALSE)
        {
            act_test_parse_test_arg(line_buffer, 2, &start, &end);

            unicode_to_uint8_bytes(start, end, bt_addr_arg->bt_addr, 2, 3);
        }

        act_test_parse_test_arg(line_buffer, 4, &start, &end);

        bt_addr_arg->bt_addr_add_mode = unicode_to_int(start, end, 16);

        act_test_parse_test_arg(line_buffer, 5, &start, &end);

        bt_addr_arg->bt_burn_mode = unicode_to_int(start, end, 16);
#endif
    }
    else
    {
        //In other test mode, get bt address from PC sending data
        act_test_parse_test_arg(line_buffer, 2, &start, &end);

        unicode_to_uint8_bytes(start, end, bt_addr_arg->bt_addr, 2, 3);
    }

    return TRUE;
}

static int32 act_test_read_bt_addr_arg2(uint16 *line_buffer, uint8 *arg_buffer, uint32 arg_len)
{
    uint16 *start;
    uint16 *end;

    bt_addr_arg_t *bt_addr_arg = (bt_addr_arg_t *) arg_buffer;

    memset(bt_addr_arg, 0, sizeof(bt_addr_arg_t));

    act_test_parse_test_arg(line_buffer, 1, &start, &end);

    //Read byte5, byte4, byte3, It's fixed bt address normally.
    unicode_to_uint8_bytes(start, end, bt_addr_arg->bt_addr, 5, 6);

    return TRUE;
}

static int32 act_test_read_bt_name_arg(uint16 *line_buffer, uint8 *arg_buffer, uint32 arg_len)
{
    uint16 *start;
    uint16 *end;

    bt_name_arg_t *bt_name_arg = (bt_name_arg_t *) arg_buffer;

    act_test_parse_test_arg(line_buffer, 1, &start, &end);

    unicode_to_utf8_str(start, end, bt_name_arg->bt_name, TEST_BTNAME_MAX_LEN);

    return TRUE;
}

static int32 act_test_read_ble_name_arg(uint16 *line_buffer, uint8 *arg_buffer, uint32 arg_len)
{
    uint16 *start;
    uint16 *end;

    ble_name_arg_t *ble_name_arg = (ble_name_arg_t *) arg_buffer;

    act_test_parse_test_arg(line_buffer, 1, &start, &end);

    unicode_to_utf8_str(start, end, ble_name_arg->bt_ble_name, TEST_BTBLENAME_MAX_LEN);

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


	if (self->test_id == TESTID_MODIFY_BTADDR) {
		act_test_read_bt_addr_arg(self->line_buffer, self->arg_buffer, self->arg_len);
		ret_val = act_test_modify_bt_addr(self->arg_buffer);
	} else if (self->test_id == TESTID_MODIFY_BTNAME) {
		act_test_read_bt_name_arg(self->line_buffer, self->arg_buffer, self->arg_len);
		ret_val = act_test_modify_bt_name(self->arg_buffer);
	} else if (self->test_id == TESTID_MODIFY_BLENAME) {
		act_test_read_ble_name_arg(self->line_buffer, self->arg_buffer, self->arg_len);
		ret_val = act_test_modify_bt_ble_name(self->arg_buffer);
	}else if (self->test_id == TESTID_MODIFY_BTADDR2){
		act_test_read_bt_addr_arg2(self->line_buffer, self->arg_buffer, self->arg_len);
		ret_val = act_test_modify_bt_addr(self->arg_buffer);
	}else /*TESTID_FM_CH_TEST*/ {
		//ret_val = act_test_audio_fm_in(&g_fm_rx_test_arg);
	}

    return ret_val;
}


