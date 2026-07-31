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
 * \file
 * \brief
 * \author   zhangxs
 * \version  1.0
 * \date  2011/9/05
 *******************************************************************************/

#include "att_pattern_test.h"

#define PRINT_BUF_SIZE STUB_ATT_RW_TEMP_BUFFER_LEN

#define _SIGN     1
#define _ZEROPAD  2
#define _LARGE    4

#define _putc(_str, _end, _ch) \
do                             \
{                              \
    *_str++ = _ch;             \
}                              \
while (0)

const char digits[16] = "0123456789abcdef";

/*!
 * \brief format output charactors with size and parameter list.
 */
static int _vsnprintf(char* buf, size_t size, const char* fmt, va_list args)
{
    char* str = buf;
    char* end = buf + size - 1;

    ARG_UNUSED(end);

    for (; *fmt != '\0'; fmt++)
    {
        uint32 flags;
        int width;

        uint32 number;
        uint32 base;

        char num_str[16];
        int num_len;
        int sign;
        uint8 ch;
        if (*fmt != '%')
        {
            _putc(str, end, *fmt);
            continue;
        }

        fmt++;

        flags = 0, width = 0, base = 10;

        if (*fmt == '0')
        {
            flags |= _ZEROPAD;
            fmt++;
        }

        while (isdigit(*fmt))
        {
            width = (width * 10) + (*fmt - '0');
            fmt++;
        }

        switch (*fmt)
        {
            case 'c':
            {
                ch = (uint8)va_arg(args, int);

                _putc(str, end, ch);
                continue;
            }

            case 's':
            {
                char* s = va_arg(args, char*);

                while (*s != '\0')
                _putc(str, end, *s++);

                continue;
            }

            //case 'X':
            //    flags |= _LARGE;

            case 'x':
            //  case 'p':
            base = 16;
            break;

            case 'd':
            //  case 'i':
            flags |= _SIGN;

            //  case 'u':
            break;

            default:
            continue;
        }

        number = va_arg(args, uint32);

        sign = 0, num_len = 0;

        if (flags & _SIGN)
        {
            if ((int) number < 0)
            {
                number = -(int) number;

                sign = '-';
                width -= 1;
            }
        }

        if (number == 0)
        {
            num_str[num_len++] = '0';
        }
        else
        {

            while (number != 0)
            {
                char ch = digits[number % base];

                num_str[num_len++] = ch;
                number /= base;
            }
        }

        width -= num_len;

        if (sign != 0)
        _putc(str, end, sign);

        if (flags & _ZEROPAD)
        {
            while (width-- > 0)
            _putc(str, end, '0');
        }

        while (num_len-- > 0)
        _putc(str, end, num_str[num_len]);
    }

    *str = '\0';

    return (str - buf);
}

int print_log(const char* format, ...)
{
    int trans_bytes;

    print_log_t * print_log;

    va_list args;

    uint8 data_buffer[256];

    if(att_get_test_mode() == TEST_MODE_CARD)
    {
        return 0;
    }

    va_start(args, format);

    print_log = (print_log_t *) data_buffer;

    trans_bytes = _vsnprintf((char *) &(print_log->log_data), PRINT_BUF_SIZE, format, args);

    //Add terminator.
    memset(&print_log->log_data[trans_bytes], 0, 4);
    trans_bytes += 1;

    //4 bytes alignment
    trans_bytes = (((trans_bytes + 3) >> 2) << 2);

    att_write_data(STUB_CMD_ATT_PRINT_LOG, trans_bytes, data_buffer);

    printk("%s\n",(uint8 *) &(print_log->log_data));

    //As buf in UDA's USB, cannot remove this bug, else, UDA's USB data will have offset.
    att_read_data(STUB_CMD_ATT_ACK, 0, data_buffer);

    return 0;
}

