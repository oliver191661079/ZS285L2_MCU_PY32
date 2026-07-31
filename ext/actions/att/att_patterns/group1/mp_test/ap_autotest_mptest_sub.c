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
#include "ap_autotest_mptest.h"
#include "mp_app.h"

extern uint32 att_write_trim_cap(u8_t index, uint32 mode);

void act_test_report_mptest_result(int32 ret_val, mp_test_arg_t *mp_arg, cfo_return_arg_t *cfo_return_arg,
        uint32 test_mode)
{
    return_result_t *return_data;
    uint16 trans_bytes = 0;

    return_data = (return_result_t *) (STUB_ATT_RETURN_DATA_BUFFER);

    if (test_mode == 0)
    {
        return_data->test_id = TESTID_MP_TEST;

        if (ret_val == TRUE)
        {
#if 0
            if ((cfo_return_arg->cfo_index - CFO_THRESHOLD_ADJUST_VAL) > CFO_THRESHOLD_LEFT)
            {
                cfo_return_arg->cfo_index_low = cfo_return_arg->cfo_index - CFO_THRESHOLD_ADJUST_VAL;
            }
            else
            {
                cfo_return_arg->cfo_index_low = CFO_THRESHOLD_LEFT;
            }

            if ((cfo_return_arg->cfo_index + CFO_THRESHOLD_ADJUST_VAL) < CFO_THRESHOLD_RIGHT)
            {
                cfo_return_arg->cfo_index_high = cfo_return_arg->cfo_index + CFO_THRESHOLD_ADJUST_VAL;
            }
            else
            {
                cfo_return_arg->cfo_index_high = CFO_THRESHOLD_RIGHT;
            }
#endif

            cfo_return_arg->cfo_index_low = mp_arg->cfo_index_low;

            cfo_return_arg->cfo_index_high = mp_arg->cfo_index_high;
        }
        else
        {
            cfo_return_arg->cfo_index_low = mp_arg->cfo_index_low;

            cfo_return_arg->cfo_index_high = mp_arg->cfo_index_high;
        }
    }
    else
    {
        return_data->test_id = TESTID_MP_READ_TEST;

        cfo_return_arg->cfo_index_low = mp_arg->cfo_index_low;

        cfo_return_arg->cfo_index_high = mp_arg->cfo_index_high;
    }

    if (ret_val == TRUE)
    {
        print_log("test over! cfo:%d ber:%d index:%d", cfo_return_arg->cfo_val, cfo_return_arg->ber_val, cfo_return_arg->cfo_index);
    }
    else
    {
        print_log("test failed! min cfo: %d ber:%d index:%d", cfo_return_arg->cfo_val, cfo_return_arg->ber_val, cfo_return_arg->cfo_index);
    }

    return_data->test_result = ret_val;

    int32_to_unicode(mp_arg->cfo_channel[0], &(return_data->return_arg[trans_bytes]), &trans_bytes, 10);

    return_data->return_arg[trans_bytes++] = 0x002c;

    int32_to_unicode(mp_arg->cfo_channel[1], &(return_data->return_arg[trans_bytes]), &trans_bytes, 10);

    return_data->return_arg[trans_bytes++] = 0x002c;

    int32_to_unicode(mp_arg->cfo_channel[2], &(return_data->return_arg[trans_bytes]), &trans_bytes, 10);

    return_data->return_arg[trans_bytes++] = 0x002c;

    int32_to_unicode(mp_arg->cfo_test, &(return_data->return_arg[trans_bytes]), &trans_bytes, 10);

    //Terminator
    return_data->return_arg[trans_bytes++] = 0x002c;

    if (test_mode == 0)
    {
        int32_to_unicode(mp_arg->ref_cap, &(return_data->return_arg[trans_bytes]), &trans_bytes, 10);
        return_data->return_arg[trans_bytes++] = 0x002c;

        int32_to_unicode(cfo_return_arg->cfo_index_low, &(return_data->return_arg[trans_bytes]), &trans_bytes, 10);
        return_data->return_arg[trans_bytes++] = 0x002c;

        int32_to_unicode(cfo_return_arg->cfo_index_high, &(return_data->return_arg[trans_bytes]), &trans_bytes, 10);
        return_data->return_arg[trans_bytes++] = 0x002c;

        int32_to_unicode(mp_arg->cfo_index_changed, &(return_data->return_arg[trans_bytes]), &trans_bytes, 10);
        return_data->return_arg[trans_bytes++] = 0x002c;
    }

    int32_to_unicode(mp_arg->cfo_threshold_low, &(return_data->return_arg[trans_bytes]), &trans_bytes, 10);

    return_data->return_arg[trans_bytes++] = 0x002c;

    int32_to_unicode(mp_arg->cfo_threshold_high, &(return_data->return_arg[trans_bytes]), &trans_bytes, 10);

    return_data->return_arg[trans_bytes++] = 0x002c;

    if (test_mode == 0)
    {
        int32_to_unicode(mp_arg->cfo_write_mode, &(return_data->return_arg[trans_bytes]), &trans_bytes, 10);
        return_data->return_arg[trans_bytes++] = 0x002c;
    }

    int32_to_unicode(mp_arg->ber_test, &(return_data->return_arg[trans_bytes]), &trans_bytes, 10);

    return_data->return_arg[trans_bytes++] = 0x002c;

    int32_to_unicode(mp_arg->ber_threshold_low, &(return_data->return_arg[trans_bytes]), &trans_bytes, 10);

    return_data->return_arg[trans_bytes++] = 0x002c;

    int32_to_unicode(mp_arg->ber_threshold_high, &(return_data->return_arg[trans_bytes]), &trans_bytes, 10);

    return_data->return_arg[trans_bytes++] = 0x002c;

    int32_to_unicode(mp_arg->rssi_test, &(return_data->return_arg[trans_bytes]), &trans_bytes, 10);

    return_data->return_arg[trans_bytes++] = 0x002c;

    int32_to_unicode(mp_arg->rssi_threshold_low, &(return_data->return_arg[trans_bytes]), &trans_bytes, 10);

    return_data->return_arg[trans_bytes++] = 0x002c;

    int32_to_unicode(mp_arg->rssi_threshold_high, &(return_data->return_arg[trans_bytes]), &trans_bytes, 10);

    //4 bytes alignment
    if ((trans_bytes % 2) != 0)
    {
        return_data->return_arg[trans_bytes++] = 0x0000;
    }

    act_test_report_result(return_data, trans_bytes * 2 + 4);
}

int read_trim_cap_efuse(uint32 mode)
{
    uint32_t trim_cap;

    int32_t ret_val;

    ret_val = freq_compensation_read(&trim_cap, mode);

    if(ret_val == TRIM_CAP_READ_NO_ERROR){
        return trim_cap;
    }else{
        return mp_get_hosc_cap();;
    }
}

#if 0
uint32 att_write_trim_cap(u8_t index, uint32 mode)
{
    uint32 ret_val;
    int value_bak;
    uint32 trim_cap = index;

    ret_val = freq_compensation_write(&trim_cap, mode);

    if(ret_val == TRIM_CAP_WRITE_NO_ERROR){
        ret_val = freq_compensation_read(&value_bak, mode);

        if(ret_val == TRIM_CAP_READ_NO_ERROR){
            if(value_bak == trim_cap){
                return TRUE;
            }
        }
        return FALSE;
    }else{
        if(ret_val == TRIM_CAP_WRITE_ERR_NO_RESOURSE){
            print_log("efuse write over!\n");
        }
        else if (ret_val == TRIM_CAP_WRITE_ERR_HW)
        {
            print_log("efuse write HW err!\n");
        }

        return FALSE;
    }
}
#endif

#if 1
static u32_t att_write_get_relative_value(u32_t old_value, u32_t new_value)
{
	if(abs((int)old_value - (int)new_value) <= 2){
		new_value = old_value;
	}

	return new_value;
}
#endif
uint32 att_write_trim_cap(u8_t index, uint32 mode)
{
    uint32 ret_val;
    uint32 value_bak;
    uint32 trim_cap = index;

#if 1
	//Read written data, and compare to new data.
	ret_val = freq_compensation_read(&value_bak, mode);
	if(ret_val == TRIM_CAP_READ_NO_ERROR){
		trim_cap = att_write_get_relative_value(value_bak, index);
		printk("old val %d set val %d new val %d\n", value_bak, index, trim_cap);
	}
#endif
    ret_val = freq_compensation_write(&trim_cap, mode);

    if(ret_val == TRIM_CAP_WRITE_NO_ERROR){
        //Re-read frequency offset, and compare.
        ret_val = freq_compensation_read(&value_bak, mode);

        if(ret_val == TRIM_CAP_READ_NO_ERROR){
            if(value_bak == trim_cap){
                return TRUE;
            }
        }
        return FALSE;
    }else{
        if(ret_val == TRIM_CAP_WRITE_ERR_NO_RESOURSE){
            print_log("efuse write over!\n");
        }
        else if (ret_val == TRIM_CAP_WRITE_ERR_HW)
        {
            print_log("efuse write HW err!\n");
        }

        return FALSE;
    }
}

int calc_c(int16 *buf, int num)
{
    int value = 0;

    for(int i = 0;i < num; i++)
    {
        value += buf[i];
    }

    return (value/num);
}

struct mp_test_stru *mp_test;

static void ber_test_cbk(u8_t event, void *param)
{
    mp_manager_info_t *p = (mp_manager_info_t *)param;

    if(mp_test && mp_test->receive_cfo_flag && event == MP_RX_PACKET_VALID )
    {
        mp_test->receive_cfo_flag --;

        memcpy(mp_test->cfo_val, p->cfo_val, sizeof(mp_test->cfo_val));
        memcpy(mp_test->rssi_val, p->rssi_val, sizeof(mp_test->rssi_val));
        mp_test->ber_val += p->ber_val;
        mp_test->a_rssi_val = calc_c(mp_test->rssi_val, 16);

        if(mp_test->receive_cfo_flag == 0)
        {
            k_sem_give(&mp_test->mp_sem);
        }
    }
}

int ber_test_channel(uint8 channel, uint32 cap, int16 *rssi_val,uint32 *ber_val)
{
    uint8 retry_cnt = 0;
    bool ret;

retry:

    mp_task_stop_sync();

    k_sem_reset(&mp_test->mp_sem);

    mp_set_hosc_cap(cap);

    mp_test->receive_cfo_flag = 1;
    mp_test->ber_val = 0;

    mp_task_rx_start(channel, ber_test_cbk, 600);

    k_sem_take(&mp_test->mp_sem, 800);

    if(mp_test->receive_cfo_flag)
    {
        mp_task_stop_sync();
        goto test_timeout;
    }
    mp_task_stop_sync();

    ret = att_ber_calc_result(mp_test->rssi_val, sizeof(mp_test->rssi_val) / sizeof(mp_test->rssi_val[1]), rssi_val);

    if(!ret)
    {
        goto fail;
    }

    print_log("ber val:%d\n", mp_test->ber_val);

    *ber_val = mp_test->ber_val * 10000 / (2 * 128 * 27 * 8);

    return 0;
fail:
    if(retry_cnt++ < TEST_RETRY_NUM)
    {
        goto retry;
    }

    return -1;

test_timeout:
    if(retry_cnt++ < TEST_RETRY_NUM)
    {
        goto retry;
    }
    return -2;
}

static void mp_cfo_cbk(u8_t event, void *param)
{
    mp_manager_info_t *p = (mp_manager_info_t *)param;

    if(mp_test && mp_test->receive_cfo_flag && event == MP_RX_PACKET_VALID )
    {
        mp_test->receive_cfo_flag = 0;
        memcpy(mp_test->cfo_val, p->cfo_val, sizeof(mp_test->cfo_val));
        memcpy(mp_test->rssi_val, p->rssi_val, sizeof(mp_test->rssi_val));
        mp_test->ber_val = p->ber_val;
        k_sem_give(&mp_test->mp_sem);
    }
}

int mp_cfo_test(uint8 channel, uint32 cap, int16 *cfo)
{
    uint8 retry_cnt = 0;
    bool ret;

retry:

    mp_task_stop_sync();
    k_sem_reset(&mp_test->mp_sem);
    mp_set_hosc_cap(cap);
    mp_test->receive_cfo_flag = 1;
    mp_task_rx_start(channel, mp_cfo_cbk, 400);

    k_sem_take(&mp_test->mp_sem, 600);

    if(mp_test->receive_cfo_flag == 1)
    {
        goto test_timeout;
    }
    mp_task_stop_sync();

    ret = att_cfo_result_calc(mp_test->cfo_val, sizeof(mp_test->cfo_val) / sizeof(mp_test->cfo_val[1]), cfo);

    if(!ret)
    {
        goto fail;
    }
    mp_task_rx_stop();
    return 0;
fail:
    if(retry_cnt++ < TEST_RETRY_NUM)
    {
        goto retry;
    }
    mp_task_rx_stop();
    //unexpected data.
    print_log("get cfo unexpected");
    return -1;

test_timeout:
    if(retry_cnt++ < TEST_RETRY_NUM)
    {
        goto retry;
    }
    //Timeout to get target data.
    mp_task_rx_stop();
    print_log("get cfo time_out");
    return -2;
}

test_result_e att_mp_test(void *arg_buffer)
{
    int32 ret_val, i, test_channel_num;
    cfo_return_arg_t cfo_return_arg;
    mp_test_arg_t *mp_arg;
    int16 cap_index[3] = {0};
    int16 rssi_val[3] = {0};
    uint32 ber_val[3] = {0};
    int16 cap_temp;
    //int16 pwr_val[3];
    //uint16 cfo_index[3];
    bool ret = FALSE;

    mp_arg = (mp_test_arg_t *) arg_buffer;

    memset(&cfo_return_arg, 0, sizeof(cfo_return_arg_t));

    print_log("mp test start\n");

    ret_val = mp_init_sync();
    if(ret_val < 0)
    {
        printk("mp init fail\n");
        goto test_end;
    }

    mp_test = z_malloc(sizeof(struct mp_test_stru));
    if(!mp_test)
    {
        goto test_end;
    }

    k_sem_init(&mp_test->mp_sem, 0, 1);

    test_channel_num = 0;

    for(i = 0; i < 3; i++)
    {
        if(mp_arg->cfo_channel[i] != 0xff)
        {
            test_channel_num++;
        }
    }

    if(test_channel_num == 0)
    {
        print_log("mp channel num is zero\n");
        ret = FALSE;
        goto test_end;
    }

    if(mp_arg->cfo_test)
    {
        for(i = 0; i < 3; i++)
        {
            if(mp_arg->cfo_channel[i] == 0xff)
            {
               continue;
            }

            ret = att_bttool_tx_begin(mp_arg->cfo_channel[i]);

            if(ret == FALSE)
            {
                goto test_end;
            }

            ret = att_mp_cfo_test(mp_arg, mp_arg->cfo_channel[i], mp_arg->ref_cap, &cap_index[i]);

            if(ret == FALSE)
            {
                goto test_end;
            }

            ret = att_bttool_tx_stop();

            if(ret == FALSE)
            {
                goto test_end;
            }
        }

        if (ret == TRUE)
        {
            cfo_return_arg.cfo_index = (cap_index[0] + cap_index[1] + cap_index[2]) / (test_channel_num);
			cfo_return_arg.cfo_val = (mp_arg->cfo_threshold_low + mp_arg->cfo_threshold_high)/2;

            mp_arg->ref_cap = cfo_return_arg.cfo_index;

			print_log("mp test success cap index:%d\n", cfo_return_arg.cfo_index);
            if (mp_arg->cfo_write_mode == 0)
            {
                print_log("mp test success cap index:%d\n", cfo_return_arg.cfo_index);
            }

            //Write only efuse, report error if writting fails.
            if (mp_arg->cfo_write_mode == 1)
            {
                ret = att_write_trim_cap(cfo_return_arg.cfo_index, RW_TRIM_CAP_EFUSE);
                //ret = att_write_trim_cap_hotfix(cfo_return_arg.cfo_index, RW_TRIM_CAP_EFUSE);
                if(ret == FALSE)
                {
                    print_log("cap write efuse fail\n");
                    goto test_end;
                }
            }

            //Write efuse firstly, if fails, write nvram.
            if (mp_arg->cfo_write_mode == 2)
            {
                ret = att_write_trim_cap(cfo_return_arg.cfo_index, RW_TRIM_CAP_SNOR);
				//ret = att_write_trim_cap_hotfix(cfo_return_arg.cfo_index, RW_TRIM_CAP_SNOR);
                if(ret == FALSE)
                {
                    goto test_end;
                }
            }
        }
    }

    if(test_channel_num > 0 && mp_arg->cfo_test)
    {
        cap_temp = cfo_return_arg.cfo_index;
    }
    else
    {
        cap_temp = read_trim_cap_efuse(RW_TRIM_CAP_SNOR);
    }

    if(mp_arg->ber_test || mp_arg->rssi_test)
    {
        for(i = 0; i < 3; i++)
        {
            if(mp_arg->cfo_channel[i] == 0xff)
            {
               continue;
            }

            printk("\ntest channel :%d\n",mp_arg->cfo_channel[i]);
            ret = att_bttool_tx_begin(mp_arg->cfo_channel[i]);

            if(ret == FALSE)
            {
                goto test_end;
            }

            ret_val = ber_test_channel(mp_arg->cfo_channel[i], cap_temp, &rssi_val[i], &ber_val[i]);

            if(ret_val < 0)
            {
                ret = FALSE;
                print_log("ber or rssi test fail,channel i:%d\n", i);
                goto test_end;
            }

            ret = att_bttool_tx_stop();

            if(ret == FALSE)
            {
                print_log("bttool stop fail\n");
                goto test_end;
            }

            //ber(bit error rate)
            if(mp_arg->ber_test && (cfo_return_arg.ber_val > mp_arg->ber_threshold_high
                        /*|| cfo_return_arg.ber_val < mp_arg->ber_threshold_low */))
            {
                print_log("ber test fail ber_val:%d\n", cfo_return_arg.ber_val);
                ret = FALSE;
                goto test_end;
            }

            if(mp_arg->rssi_test && (cfo_return_arg.rssi_val > mp_arg->rssi_threshold_high
                        || cfo_return_arg.rssi_val < mp_arg->rssi_threshold_low))
            {
                print_log("rssi test fail rssi_val:%d\n", cfo_return_arg.ber_val);
                ret = FALSE;
                goto test_end;
            }
        }

        if (ret == TRUE)
        {
            cfo_return_arg.rssi_val = (rssi_val[0] + rssi_val[1] + rssi_val[2]) / (test_channel_num);

            cfo_return_arg.ber_val = (ber_val[0] + ber_val[1] + ber_val[2]) / (test_channel_num);

            print_log("mp rssi value:%d, ber:%d\n", cfo_return_arg.rssi_val, cfo_return_arg.ber_val);
        }
    }

test_end:
    mp_exit();

    if(mp_test)
    {
        free(mp_test);
        mp_test = NULL;
    }

    act_test_report_mptest_result(ret, mp_arg, &cfo_return_arg, 0);

    return TRUE;
}

test_result_e att_mp_read_test(void *arg_buffer)
{
    int32 ret_val, i, test_channel_num;
    cfo_return_arg_t cfo_return_arg;
    mp_test_arg_t *mp_arg;
    int16 cfo_val[3] = {0};
    int16 rssi_val[3] = {0};
    uint32 ber_val[3] = {0};
    int16 cap_value;

    bool ret = TRUE;

    mp_arg = (mp_test_arg_t *) arg_buffer;

    memset(&cfo_return_arg, 0, sizeof(cfo_return_arg_t));

    printk("mp read test start\n");

    ret_val = mp_init_sync();
    if(ret_val < 0)
    {
        print_log("mp init fail\n");
        goto test_end;
    }

    mp_test = z_malloc(sizeof(struct mp_test_stru));
    if(!mp_test)
    {
        goto test_end;
    }

    k_sem_init(&mp_test->mp_sem, 0, 1);

    test_channel_num = 0;

    for(i = 0; i < 3; i++)
    {
        if(mp_arg->cfo_channel[i] != 0xff)
        {
            test_channel_num++;
        }
    }

    if(test_channel_num == 0)
    {
        print_log("mp channel num is zero\n");
        ret = FALSE;
        goto test_end;
    }

    cap_value = read_trim_cap_efuse(RW_TRIM_CAP_SNOR);
	print_log("cap %d pf\n", cap_value);

    if(mp_arg->cfo_test)
    {
        for(i = 0; i < 3; i++)
        {
            if(mp_arg->cfo_channel[i] == 0xff)
            {
                continue;
            }

            ret = att_bttool_tx_begin(mp_arg->cfo_channel[i]);

            if(ret == FALSE)
            {
                goto test_end;
            }

            ret_val = mp_cfo_test(mp_arg->cfo_channel[i], cap_value, &cfo_val[i]);
			cfo_val[i] = 0 - cfo_val[i];

            if(ret_val < 0)
            {
                print_log("mp read test,channel:%d,return fail\n", mp_arg->cfo_channel[i]);
                ret = FALSE;
                goto test_end;
            }

            ret = att_bttool_tx_stop();

            if(ret == FALSE)
            {
                print_log("bttool stop fail\n");
                goto test_end;
            }

            if(cfo_val[i] < mp_arg->cfo_threshold_low || cfo_val[i] > mp_arg->cfo_threshold_high)
            {
                print_log("mp read test fail,channel:%d,cfo:%d\n", mp_arg->cfo_channel[i], cfo_val[i]);
                ret = FALSE;
                goto test_end;
            }
        }
    }

    if(ret == TRUE)
    {
        cfo_return_arg.cfo_val = (cfo_val[0] + cfo_val[1] + cfo_val[2]) / (test_channel_num);
        print_log("mp read cfo value:%d\n", cfo_return_arg.cfo_val);
    }

    if(mp_arg->ber_test || mp_arg->rssi_test)
    {
        for(i = 0; i < 3; i++)
        {
            if(mp_arg->cfo_channel[i] == 0xff)
            {
               continue;
            }

            ret = att_bttool_tx_begin(mp_arg->cfo_channel[i]);

            if(ret == FALSE)
            {
                goto test_end;
            }

            ret_val = ber_test_channel(mp_arg->cfo_channel[i], cap_value, &rssi_val[i], &ber_val[i]);

            if(ret_val < 0)
            {
                ret = FALSE;
                print_log("ber or rssi test fail,channel:%d\n", mp_arg->cfo_channel[i]);
                goto test_end;
            }

            ret = att_bttool_tx_stop();

            if(ret == FALSE)
            {
                goto test_end;
            }

            //ber(bit error rate)
            if(mp_arg->ber_test && (ber_val[i] > mp_arg->ber_threshold_high
                        /*|| ber_val[i] > mp_arg->ber_threshold_low*/))
            {
                print_log("ber test fail ber_val:%d\n", ber_val[i]);
                ret = FALSE;
                goto test_end;
            }

            if(mp_arg->rssi_test && (rssi_val[i] > mp_arg->rssi_threshold_high
                        || rssi_val[i] < mp_arg->rssi_threshold_low))
            {
                print_log("rssi test fail rssi_val:%d\n", rssi_val[i]);
                ret = FALSE;
                goto test_end;
            }
        }

        if (ret == TRUE)
        {
            cfo_return_arg.rssi_val = (rssi_val[0] + rssi_val[1] + rssi_val[2]) / (test_channel_num);

            cfo_return_arg.ber_val = (ber_val[0] + ber_val[1] + ber_val[2]) / (test_channel_num);

            print_log("mp read rssi value:%d,ber:%d\n", cfo_return_arg.rssi_val, cfo_return_arg.ber_val);
        }
    }

test_end:

    mp_exit();

    if(mp_test)
    {
        free(mp_test);
        mp_test = NULL;
    }

    act_test_report_mptest_result(ret, mp_arg, &cfo_return_arg, 1);

    return TRUE;
}

int32 act_test_read_mptest_arg(uint16 *line_buffer, uint8 *arg_buffer, uint32 arg_len)
{
    uint16 *start = 0;
    uint16 *end = 0;
    uint8 arg_num;

    mp_test_arg_t *mp_arg = (mp_test_arg_t *) arg_buffer;

    //if(arg_len < sizeof(mp_test_arg_t))
    //{
    //    DEBUG_ATT_PRINT("argument too long", 0, 0);
    //    while(1);
    //}

    printk("\n arg len :%d\n",arg_len);
    arg_num = 1;

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    mp_arg->cfo_channel[0] = (uint8) unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    mp_arg->cfo_channel[1] = (uint8) unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    mp_arg->cfo_channel[2] = (uint8) unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    mp_arg->cfo_test = (uint8) unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    mp_arg->ref_cap = (uint16) unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    mp_arg->cfo_index_low = (uint8) unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    mp_arg->cfo_index_high = (uint8) unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    mp_arg->cfo_index_changed = (uint8) unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    mp_arg->cfo_threshold_low = (int8) unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    mp_arg->cfo_threshold_high = (int8) unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    mp_arg->cfo_write_mode = (uint8) unicode_to_int(start, end, 10);

    //act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    //mp_arg->cfo_upt_offset = (int32) unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    mp_arg->ber_test = (uint8) unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    mp_arg->ber_threshold_low = (uint32) unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    mp_arg->ber_threshold_high = (uint32) unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    mp_arg->rssi_test = (uint8) unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    mp_arg->rssi_threshold_low = (int16) unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    mp_arg->rssi_threshold_high = (int16) unicode_to_int(start, end, 10);

    return TRUE;
}

int32 act_test_read_mptest_read_arg(uint16 *line_buffer, uint8 *arg_buffer, uint32 arg_len)
{
    uint16 *start = 0;
    uint16 *end = 0;
    uint8 arg_num;

    mp_test_arg_t *mp_arg = (mp_test_arg_t *) arg_buffer;

    //if(arg_len < sizeof(mp_test_arg_t))
    //{
    //    DEBUG_ATT_PRINT("argument too long", 0, 0);
    //    while(1);
    //}

    printk("\n arg len :%d\n",arg_len);
    arg_num = 1;

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    mp_arg->cfo_channel[0] = (uint8) unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    mp_arg->cfo_channel[1] = (uint8) unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    mp_arg->cfo_channel[2] = (uint8) unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    mp_arg->cfo_test = (uint8) unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    mp_arg->cfo_threshold_low = (int8) unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    mp_arg->cfo_threshold_high = (int8) unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    mp_arg->ber_test = (uint8) unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    mp_arg->ber_threshold_low = (uint32) unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    mp_arg->ber_threshold_high = (uint32) unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    mp_arg->rssi_test = (uint8) unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    mp_arg->rssi_threshold_low = (int16) unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    mp_arg->rssi_threshold_high = (int16) unicode_to_int(start, end, 10);

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


	if (self->test_id == TESTID_MP_TEST) {
		act_test_read_mptest_arg(self->line_buffer, self->arg_buffer, self->arg_len);
		ret_val = att_mp_test(self->arg_buffer);
	} else if (self->test_id == TESTID_MP_READ_TEST) {
		act_test_read_mptest_read_arg(self->line_buffer, self->arg_buffer, self->arg_len);
		ret_val = att_mp_read_test(self->arg_buffer);
	}else /*TESTID_FM_CH_TEST*/ {
		//ret_val = act_test_audio_fm_in(&g_fm_rx_test_arg);
	}

    return ret_val;
}
