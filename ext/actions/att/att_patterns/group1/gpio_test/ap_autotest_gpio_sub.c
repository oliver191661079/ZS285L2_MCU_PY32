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
#include <gpio.h>
#include "ap_autotest_gpio.h"

u32_t act_test_report_gpio_result(gpio_test_arg_t *gpio_test_arg, test_result_e result, uint32 test_id)
{
    int ret_val;
    return_result_t *return_data;
    uint16 trans_bytes = 0;

    //DEBUG_ATT_PRINT("gpio test result", result, 2);

    if (result == TEST_PASS)
    {
        ret_val = 1;
        att_write_test_info("gpio test ok", 0, 0);
    }
    else
    {
        ret_val = 0;
        att_write_test_info("gpio test failed", 0, 0);
    }

    if (att_get_test_mode() != TEST_MODE_CARD)
    {
        return_data = (return_result_t *) (STUB_ATT_RETURN_DATA_BUFFER);

        return_data->test_id = test_id;

        return_data->test_result = ret_val;

        uint32_to_unicode(gpio_test_arg->gpioA_value, return_data->return_arg, &trans_bytes, 16);

        //Delimeter
        return_data->return_arg[trans_bytes++] = 0x002c;

        uint32_to_unicode(gpio_test_arg->gpioB_value, &(return_data->return_arg[trans_bytes]), &trans_bytes, 16);

        //Delimeter
        return_data->return_arg[trans_bytes++] = 0x002c;

        uint32_to_unicode(gpio_test_arg->gpioC_value, &(return_data->return_arg[trans_bytes]), &trans_bytes, 16);

        //Terminator
        return_data->return_arg[trans_bytes++] = 0x0000;

        //4 bytes align
        if ((trans_bytes % 2) != 0)
        {
            return_data->return_arg[trans_bytes++] = 0x0000;
        }

        act_test_report_result(return_data, trans_bytes * 2 + 4);
    }
    else
    {
        act_test_report_test_log(ret_val, test_id);
    }

	return ret_val;
}

/*
Define GPIO to test, bit 1 means to test.
ATT_GPIO_GROUP_1:GPIO0~GPIO31
ATT_GPIO_GROUP_2:GIPO32~
GPIO tests on mechanism:0,1,4~15,21,23,32~35,38,39,40
*/
#define ATT_GPIO_GROUP_1 0x00a0fff3
#define ATT_GPIO_GROUP_2 0x000001cf

//#define ATT_GPIO_CTRL  42

test_result_e test_gpio(gpio_test_arg_t *gpio_test_arg)
{
    test_result_e ret = 0;
    uint32 *gpio_bak_array;
    uint32 gpio_reg;
    u32_t GPIO_GROUP_1_result = 0, GPIO_GROUP_2_result = 0;
    uint8 i, test_num = 0;
    u32_t  value, flag;
    struct device *gpio_dev;

    gpio_dev = self->dev_tbl->gpio_dev;

	if(!gpio_dev){
		printk("gpio_dev is null\n");
        return TEST_GPIO_INIT_FAIL;
	}

    if(gpio_test_arg->gpioC_value > 57)
    {
        printk("gpio test parameter error:%d\n",gpio_test_arg->gpioC_value);
        gpio_test_arg->gpioC_value = 0;
        return TEST_GPIO_INIT_FAIL;
    }

    for (i = 0; i < 64; i++)
    {
        if(i < 32)
        {
            if(gpio_test_arg->gpioA_value  & (1 << i))
            {
                test_num = i;
            }
        }
        else
        {
            if(gpio_test_arg->gpioB_value  & (1 << (i - 32)))
            {
                test_num = i;
            }
        }
    }
    gpio_bak_array = z_malloc(4 * (test_num + 1));
    flag = irq_lock();

    gpio_reg = GPIO_REG_BASE;

    for (i = 0; i < test_num + 1; i++, gpio_reg += 4)
    {
        gpio_bak_array[i] = sys_read32(gpio_reg);
    }

    for (i = 0; i < test_num; i++)
    {
        if(i < 32)
        {
            if(gpio_test_arg->gpioA_value  & (1 << i))
            {
                gpio_pin_configure(gpio_dev, i, GPIO_DIR_OUT | GPIO_PUD_PULL_DOWN);
                gpio_pin_write(gpio_dev, i, 0);
            }
        }
        else
        {
            if(gpio_test_arg->gpioB_value  & (1 << (i - 32)))
            {
                gpio_pin_configure(gpio_dev, i, GPIO_DIR_OUT | GPIO_PUD_PULL_DOWN);
                gpio_pin_write(gpio_dev, i, 0);
            }
        }
    }

    gpio_pin_configure(gpio_dev, gpio_test_arg->gpioC_value, GPIO_DIR_OUT | GPIO_PUD_PULL_DOWN);

    for (i = 0; i < test_num; i++)
    {
        ret = 0;
        if(i < 32)
        {
            if(gpio_test_arg->gpioA_value & (1 << i))
            {
                ret = 1;
            }
        }
        else
        {
            if(gpio_test_arg->gpioB_value & (1 << (i - 32)))
            {
                ret = 1;
            }
        }

        if(ret == 1)
        {
            gpio_pin_configure(gpio_dev, i, GPIO_DIR_IN);

            gpio_pin_write(gpio_dev, gpio_test_arg->gpioC_value, 0);
            k_sleep(1);
            gpio_pin_read(gpio_dev, i, &value);
            if(value != 1)
            {
                if(i < 32)
                {
                    GPIO_GROUP_1_result |= 1 << i;
                }
                else
                {
                    GPIO_GROUP_2_result |= 1 << (i - 32);
                }
                gpio_pin_configure(gpio_dev, i, GPIO_DIR_OUT | GPIO_PUD_PULL_DOWN);
                continue;
            }

            gpio_pin_write(gpio_dev, gpio_test_arg->gpioC_value, 1);
            k_sleep(1);
            gpio_pin_read(gpio_dev, i, &value);
            gpio_pin_configure(gpio_dev, i, GPIO_DIR_OUT | GPIO_PUD_PULL_DOWN);
            if(value != 0)
            {
                if(i < 32)
                {
                    GPIO_GROUP_1_result |= 1 << i;
                }
                else
                {
                    GPIO_GROUP_2_result |= 1 << (i - 32);
                }
                continue;
            }
        }

    }
    ret = TEST_PASS;

    gpio_reg = GPIO_REG_BASE;
    for (i = 0; i < test_num + 1; i++, gpio_reg += 4)
    {
        sys_write32(gpio_bak_array[i], gpio_reg);
    }

    irq_unlock(flag);

    free(gpio_bak_array);

    gpio_test_arg->gpioA_value = GPIO_GROUP_1_result;
    gpio_test_arg->gpioB_value = GPIO_GROUP_2_result;
    gpio_test_arg->gpioC_value = 1;

    if(GPIO_GROUP_1_result || GPIO_GROUP_2_result)
    {
        for (i = 0; i < test_num; i++)
        {
            if(i < 32)
            {
                if(GPIO_GROUP_1_result  & (1 << i))
                {
                    print_log("gpio %d test fail\n",i);
                }
            }
            else
            {
                if(GPIO_GROUP_2_result  & (1 << (i - 32)))
                {
                    print_log("gpio %d test fail\n",i);
                }
            }
        }
       ret = TEST_GPIO_FAIL;
    }

    return ret;
}

test_result_e act_test_gpio_test(void *arg_buffer)
{
   // int32 index;
    test_result_e ret_val;
    //DEBUG_ATT_PRINT("start gpio test", 0, 0);

    //DEBUG_ATT_PRINT("GPIO_A  :", *((uint32 *)arg_buffer), 2);
    //DEBUG_ATT_PRINT("GPIO_B  :", *(((uint32 *)arg_buffer)+1), 2);
    //DEBUG_ATT_PRINT("GPIO_SIO:", *(((uint32 *)arg_buffer)+2), 2);

    gpio_test_arg_t *gpio_test_arg = (gpio_test_arg_t *) arg_buffer;

    ret_val = test_gpio(gpio_test_arg);

    ret_val = act_test_report_gpio_result(gpio_test_arg, ret_val, TESTID_GPIO_TEST);

	return ret_val;
}

static int32 act_test_read_gpio_arg(uint16 *line_buffer, uint8 *arg_buffer, uint32 arg_len)
{
    uint16 *start = 0;
    uint16 *end = 0;

    gpio_test_arg_t *gpio_arg = (gpio_test_arg_t *) arg_buffer;

    //if(arg_len < sizeof(gpio_test_arg_t))
    //{
    //    DEBUG_ATT_PRINT("argument too long", 0, 0);
    //    while(1);
    //}

    act_test_parse_test_arg(line_buffer, 1, &start, &end);

    gpio_arg->gpioA_value = unicode_to_int(start, end, 16);

    act_test_parse_test_arg(line_buffer, 2, &start, &end);

    gpio_arg->gpioB_value = unicode_to_int(start, end, 16);

    act_test_parse_test_arg(line_buffer, 3, &start, &end);

    gpio_arg->gpioC_value = unicode_to_int(start, end, 16);

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

	if (self->test_id == TESTID_GPIO_TEST) {
		act_test_read_gpio_arg(self->line_buffer, self->arg_buffer, self->arg_len);
		ret_val = act_test_gpio_test((gpio_test_arg_t *)self->arg_buffer);
	}else /*TESTID_FM_CH_TEST*/ {
		//ret_val = act_test_audio_fm_in(&g_fm_rx_test_arg);
	}

    return ret_val;
}


