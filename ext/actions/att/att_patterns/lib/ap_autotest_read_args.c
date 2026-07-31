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
#define SYS_LOG_NO_NEWLINE
#define SYS_LOG_DOMAIN  "att read arg"
#define SYS_LOG_LEVEL   ATT_SYS_LOG_LEVEL

#include "att_pattern_test.h"

/*
uint16 *act_test_read_cfg_file(uint32 line_number)
{
    uint16 *cur;
    uint16 *start;
    uint16 *begin;
    uint32 cur_line;
    uint16 *cfg_file_buffer = (uint16 *) CFG_FILE_BUFFER;

    if (g_test_ap_info->cfg_file_len > MAX_CFG_FILE_LENGTH)
    {
        //DEBUG_ATT_PRINT("cfg file length error!", 0, 0);

        return FALSE;
    }

    vfs_file_seek(g_file_sys_id, g_test_ap_info->cfg_file_offset, SEEK_SET, g_test_file_handle);

    vfs_file_read(g_file_sys_id, cfg_file_buffer, g_test_ap_info->cfg_file_len, g_test_file_handle);

    cur = cfg_file_buffer;

    cur_line = 0;

    while (1)
    {
        start = cur;

        begin = cur;

        //End flag 00 0d 00 0a
        while (*cur != 0x000a)
        {
            cur++;
        }

        while ((*start != 0x003d) && (start != cur))
        {
            start++;
        }

        //'=' flag means valid data line.
        if (*start == 0x003d)
        {
            cur_line++;
        }

        //Get target line and quit.
        if (cur_line == line_number)
        {
            break;
        }
        else
        {
            cur++;

            if ((uint32)(cur - cfg_file_buffer) > g_test_ap_info->cfg_file_len)
            {
                return 0;
            }
        }
    }

    return begin;
}
*/
static uint8 unicode_to_uint8(uint16 widechar)
{
    uint8 temp_value;

    if ((widechar >= '0') && (widechar <= '9'))
    {
        temp_value = (widechar - '0');
    }
    else if ((widechar >= 'A') && (widechar <= 'F'))
    {
        temp_value = widechar + 10 - 'A';
    }
    else if ((widechar >= 'a') && (widechar <= 'f'))
    {
        temp_value = widechar + 10 - 'a';
    }
    else
    {
        return 0;
    }

    return temp_value;
}

int32 unicode_to_int(uint16 *start, uint16 *end, uint32 base)
{
    uint32 minus_flag;
    int32 temp_value = 0;

    minus_flag = FALSE;

    while (start != end)
    {
        if (*start == '-')
        {
            minus_flag = TRUE;
        }
        else
        {
            temp_value *= base;

            temp_value += unicode_to_uint8(*start);
        }
        start++;
    }

    if (minus_flag == TRUE)
    {
        temp_value = 0 - temp_value;
    }

    return temp_value;
}

static int32 unicode_encode_utf8(uint8 *s, uint16 widechar)
{
    int32 encode_len;

    if (widechar & 0xF800)
    {
        encode_len = 3;
    }
    else if (widechar & 0xFF80)
    {
        encode_len = 2;
    }
    else
    {
        encode_len = 1;
    }

    switch (encode_len)
    {
        case 1:
        *s = (char) widechar;
        break;

        case 2:
        *s++ = 0xC0 | (widechar >> 6);
        *s = 0x80 | (widechar & 0x3F);
        break;

        case 3:
        *s++ = 0xE0 | (widechar >> 12);
        *s++ = 0x80 | ((widechar >> 6) & 0x3F);
        *s = 0x80 | (widechar & 0x3F);
        break;
    }

    return encode_len;
}

int32 unicode_to_utf8_str(uint16 *start, uint16 *end, uint8 *utf8_buffer, uint32 utf8_buffer_len)
{
    int32 encode_len;
    int32 encode_total_len;

    encode_len = 0;
    encode_total_len = 0;

    while (start != end)
    {
        encode_len = unicode_encode_utf8(utf8_buffer, *start);

        start++;

        if (encode_len + encode_total_len > utf8_buffer_len)
        {
            return FALSE;
        }

        encode_total_len += encode_len;

        utf8_buffer += encode_len;
    }

    //Terminator
    *utf8_buffer = 0;

    return TRUE;
}

int32 unicode_to_uint8_bytes(uint16 *start, uint16 *end, uint8 *byte_buffer, uint8 byte_index, uint8 byte_len)
{
    while (start != end)
    {
        byte_buffer[byte_index] = (unicode_to_uint8(*start) << 4);

        byte_buffer[byte_index] |= unicode_to_uint8(*(start + 1));

        byte_index--;

        byte_len--;

        if (byte_len == 0)
        {
            break;
        }

        start += 2;
    }

    return TRUE;
}

uint32 act_test_read_test_id(uint16 *line_buffer)
{
    uint16 *start;
    uint16 *cur;
    uint32 test_id;

    start = line_buffer;
    cur = start;

    while (*cur != 0x003d)
    {
        cur++;
    }

    //test id is decimal string.
    test_id = unicode_to_int(start, cur, 10);

    return test_id;
}

uint8 *act_test_parse_test_arg(uint16 *line_buffer, uint8 arg_number, uint16 **start, uint16 **end)
{
    uint16 *cur;
    //uint32 test_id;
    uint8 cur_arg_num;

    cur = line_buffer;

    //Card mode needs filter charactors before '='
    //USB mode get charactors derectly.
    if (att_get_test_mode() == TEST_MODE_CARD)
    {
        while (*cur != 0x003d)
        {
            cur++;
        }

        //skip '='
        cur++;
    }

    cur_arg_num = 0;

    while (cur_arg_num < arg_number)
    {
        *start = cur;

        //',' means a new parameter; 0x0d0a is line terminator.
        while ((*cur != 0x002c) && (*cur != 0x000d) && (*cur != 0x0000))
        {
            cur++;
        }

        *end = cur;

        cur_arg_num++;

        cur++;
    }

    return 0;
}










int32 act_test_read_lradc_test_arg(uint16 *line_buffer, uint8 *arg_buffer, uint32 arg_len)
{
    uint16*  start = NULL;
    uint16*  end   = NULL;
    uint8 arg_num;
    lradc_test_arg_t *lradc_arg = (lradc_test_arg_t *)arg_buffer;

    arg_num = 1;

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);
    lradc_arg->lradc1_test = (uint8)unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);
    lradc_arg->lradc1_thr_low = (uint8)unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);
    lradc_arg->lradc1_thr_high = (uint8)unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);
    lradc_arg->lradc2_test = (uint8)unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);
    lradc_arg->lradc2_thr_low = (uint8)unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);
    lradc_arg->lradc2_thr_high = (uint8)unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);
    lradc_arg->lradc4_test = (uint8)unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);
    lradc_arg->lradc4_thr_low = (uint8)unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);
    lradc_arg->lradc4_thr_high = (uint8)unicode_to_int(start, end, 10);

    return TRUE;
}


#if 0
const att_task_read_arg_t autotest_readarg_ops[] =
{
    { TESTID_MODIFY_BTNAME, act_test_read_bt_name_arg },

    { TESTID_MODIFY_BLENAME, act_test_read_ble_name_arg },

    { TESTID_MODIFY_BTADDR, act_test_read_bt_addr_arg },

    { TESTID_MODIFY_BTADDR2, act_test_read_bt_addr_arg2 },

    { TESTID_READ_BTNAME, act_test_read_btname_arg },

    { TESTID_BT_TEST, act_test_read_btplay_arg },

    { TESTID_MP_TEST, act_test_read_mptest_arg },

    { TESTID_MP_READ_TEST, act_test_read_mptest_read_arg },

    { TESTID_PWR_TEST, act_test_read_pwr_test_arg},

    { TESTID_GPIO_TEST, act_test_read_gpio_arg },

    { TESTID_LINEIN_CH_TEST_ATS28XX, act_test_read_channel_test_arg},

    { TESTID_MIC_CH_TEST, act_test_read_channel_test_arg},

    { TESTID_READ_FW_VERSION, act_test_read_fw_version_arg },
};

int32 act_test_read_test_arg(uint16 test_id, uint16 *line_buffer, uint8 *arg_buffer, uint32 arg_len)
{
    uint32 i;

    for (i = 0; i < sizeof(autotest_readarg_ops) / sizeof(att_task_read_arg_t); i++)
    {
        if (autotest_readarg_ops[i].test_id == test_id)
        {
            autotest_readarg_ops[i].read_arg_func(line_buffer, arg_buffer, arg_len);
            printk("\nget id:0x%x\n",test_id);
            return TRUE;
        }
    }

    return FALSE;
}
#endif

int32 act_test_read_test_info(uint8 read_line, uint8 *test_id, uint8 *arg_buffer, uint32 arg_len)
{
/*
    uint16 *line_buffer;

    line_buffer = act_test_read_cfg_file(read_line);

    //Finish file parsing, test is done.
    if (line_buffer == 0)
    {
        *test_id = 0xff;
        return;
    }

    *test_id = act_test_read_test_id(line_buffer);

    return act_test_read_test_arg(*test_id, line_buffer, arg_buffer, arg_len);
    */
    return 0;
}
