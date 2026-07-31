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
#include "ap_autotest_mptest.h"
//#include "ap_autotest_bertest.h"

/**
 Report test result to PC.
*/
static void _pwr_test_report_result(pwr_test_arg_t *pwr_arg, uint8 ret_val)
{
    return_result_t *return_data;
    uint16 trans_bytes = 0;

    return_data = (return_result_t *)(STUB_ATT_RETURN_DATA_BUFFER);

    return_data->test_id = TESTID_PWR_TEST;

    return_data->test_result = ret_val;

    /** 0x002c delemeter
    */
    int32_to_unicode(pwr_arg->pwr_channel[0], &(return_data->return_arg[trans_bytes]), &trans_bytes, 10);
    return_data->return_arg[trans_bytes++] = 0x002c;
    int32_to_unicode(pwr_arg->pwr_channel[1], &(return_data->return_arg[trans_bytes]), &trans_bytes, 10);
    return_data->return_arg[trans_bytes++] = 0x002c;
    int32_to_unicode(pwr_arg->pwr_channel[2], &(return_data->return_arg[trans_bytes]), &trans_bytes, 10);
    return_data->return_arg[trans_bytes++] = 0x002c;
    int32_to_unicode(pwr_arg->pwr_thr_low, &(return_data->return_arg[trans_bytes]), &trans_bytes, 10);
    return_data->return_arg[trans_bytes++] = 0x002c;
    int32_to_unicode(pwr_arg->pwr_thr_high, &(return_data->return_arg[trans_bytes]), &trans_bytes, 10);

    /** 4 bytes alignment
    */
    return_data->return_arg[trans_bytes++] = 0x0000;
    if((trans_bytes % 2) != 0)
    {
        return_data->return_arg[trans_bytes++] = 0x0000;
    }

    act_test_report_result(return_data, trans_bytes*2 + 4);
}

bool att_bttool_pwr_tx_begin(uint32 channel)
{
    uint32 ret_val;

    bttools_mp_param_t tx_param;

    memset(&tx_param, 0, sizeof(tx_param));

    tx_param.mp_type = MP_ICTYPE;
    tx_param.sdk_type = 0;
    tx_param.packet_type = PKTTYPE_SET;
    tx_param.payload_len = PAYLOAD_LEN;
    tx_param.payload_type = PAYLOADTYPE_SET;
    tx_param.rf_channel = channel;
    tx_param.rf_power = RF_POWER;
    tx_param.timeout = 0;

    memcpy((uint8 *) (STUB_ATT_RW_TEMP_BUFFER + sizeof(stub_ext_cmd_t)), &tx_param, sizeof(tx_param));

    mp_task_tx_start(channel);

    ret_val = att_write_data(STUB_CMD_ATT_PWR_TX_BEGIN, sizeof(bttools_mp_param_t), STUB_ATT_RW_TEMP_BUFFER);

    if (ret_val == 0)
    {
        ret_val = att_read_data(STUB_CMD_ATT_ACK, 0, STUB_ATT_RW_TEMP_BUFFER);

        if(ret_val != 0)
        {
            print_log("pwr tx begain ack fail\n");
            return FALSE;
        }
    }
    else
    {
        print_log("pwr tx begain fail\n");
        return FALSE;
    }

    return TRUE;
}

bool att_bttool_pwr_tx_stop(void)
{
    bool ret_val;

    ret_val = att_cmd_send(STUB_CMD_ATT_PWR_TX_STOP, 3);

    mp_task_stop_sync();

    return ret_val;
}

bool att_bttool_pwr_read(int16 *rssi_value)
{
    uint32 ret_val;
    uint32 read_len;
    int16 rssi_temp_buffer[16]; //30 records maximum, 8 bytes per record.
    uint8* pdata;

    ret_val = att_write_data(STUB_CMD_ATT_MP_REPORT, 0, STUB_ATT_RW_TEMP_BUFFER);

    if(ret_val == 0)
    {
        read_len = 16 * 2;

        ret_val = att_read_data(STUB_CMD_ATT_MP_REPORT, read_len, STUB_ATT_RW_TEMP_BUFFER);

        if(ret_val == 0)
        {
            pdata = (uint8 *)STUB_ATT_RW_TEMP_BUFFER;
            memcpy(rssi_temp_buffer, &pdata[6], read_len);
            print_buffer(&pdata[6], 1, read_len, 16, -1);
            ret_val = att_ber_calc_result(rssi_temp_buffer,
                        sizeof(rssi_temp_buffer) / sizeof(rssi_temp_buffer[0]),  rssi_value);
        }
        else
        {
            print_log("att pwr report read fail\n");
            ret_val = FALSE;
        }
    }
    else
    {
        print_log("att pwr report write fail\n");
        ret_val = FALSE;
    }

    return ret_val;
}

int32 do_pwr_test(pwr_test_arg_t *pwr_arg, int channel, int16 *rssi_val)
{
    int32 ret_val;

    int32 retry_cnt = 0;

    uint32 timestamp;
retry:
    /** Start small package sending
    */
    ret_val = att_bttool_pwr_tx_begin(channel);
    if(ret_val == FALSE)
    {
        print_log("pwr tx start fail !");
        goto start_fail;
    }

    timestamp = k_uptime_get_32();

    k_sleep(700);

    while(k_uptime_get_32() - timestamp < 10000)
    {
        ret_val = att_bttool_pwr_read(rssi_val);
        if(ret_val)
        {
			print_log("get rssi_val %d",*rssi_val);
            break;
        }
        k_sleep(400);
    }

    if(ret_val == TRUE)
    {
        //Check signal strength.
        if(*rssi_val < pwr_arg->pwr_thr_low
                    || *rssi_val > pwr_arg->pwr_thr_high)
        {
            print_log("rssi value out of range,current rssi value:%d\n", *rssi_val);
            ret_val = FALSE;
        }
    }

    att_bttool_pwr_tx_stop();

start_fail:

    retry_cnt++;

    if(retry_cnt < 3 && ret_val == FALSE)
    {
        goto retry;
    }

    return ret_val;
}

//Check transfer power by signal strength from speacker.
test_result_e act_test_pwr_test(void *arg_buffer)
{
    uint32 i;

    uint32 test_channel_num = 0;

    pwr_test_arg_t *pwr_arg;

    int32 ret_val;

    uint32 cap_value;

    int16 rssi_val[3] = {0};

    print_log("enter pwr_test !\n");

    pwr_arg = (pwr_test_arg_t *)arg_buffer;

    ret_val = mp_init_sync();
    if(ret_val < 0)
    {
        print_log("mp init fail !\n");
        ret_val = FALSE;
        goto test_end;
    }

    cap_value = read_trim_cap_efuse(RW_TRIM_CAP_SNOR);

    mp_set_hosc_cap(cap_value);

    printk("cap :%d\n",cap_value);

    for(i = 0; i < 3; i++)
    {
        if(pwr_arg->pwr_channel[i] != 0xff)
        {
            test_channel_num++;
        }
    }

    for(i = 0; i < 3; i++)
    {
        if(pwr_arg->pwr_channel[i] == 0xff)
        {
            continue;
        }
        ret_val = do_pwr_test(pwr_arg, pwr_arg->pwr_channel[i], &rssi_val[i]);

        if(ret_val != TRUE)
        {
            print_log("channel:%d, pwr return fail\n",pwr_arg->pwr_channel[i]);
            break;
        }
    }

    if(ret_val)
    {
        pwr_arg->rssi_val = (rssi_val[0] + rssi_val[1] + rssi_val[2]) /  test_channel_num;
    }

test_end:

    mp_exit();

    _pwr_test_report_result(pwr_arg, ret_val);

    return TRUE;
}


int32 act_test_read_pwr_test_arg(uint16 *line_buffer, uint8 *arg_buffer, uint32 arg_len)
{
    uint16*  start = NULL;
    uint16*  end   = NULL;
    uint8 arg_num;
    pwr_test_arg_t *ber_arg = (pwr_test_arg_t *)arg_buffer;

    arg_num = 1;
    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    ber_arg->pwr_channel[0] = (uint8)unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    ber_arg->pwr_channel[1] = (uint8)unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    ber_arg->pwr_channel[2] = (uint8)unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    ber_arg->pwr_thr_low = (int8)unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);

    ber_arg->pwr_thr_high = (int8)unicode_to_int(start, end, 10);

    return TRUE;
}


bool att_ber_calc_result(int16 * data_buffer,uint32 result_num, int16 *rssi_return)
{
    int32 i;
    int16 rssi_val;
    int16 diff_val;
    int16 rssi_avg_val;
    int8 div_val;
    int32 rssi_val_total;
    int16 ori_rssi_val;
    int32 max_diff_val;
    int32 max_diff_index;
    int32 invalid_data_flag;

    div_val = 0;

    rssi_avg_val = 0;

    rssi_val_total = 0;

    for (i = 0; i < result_num; i++)
    {
        ori_rssi_val = (int16)(data_buffer[i]);

        rssi_val = ori_rssi_val;

        if (ori_rssi_val != INVALID_RSSI_VAL)
        {
            rssi_val_total += rssi_val;
            div_val++;

        }
    }

    //At least 10 records.
    if (div_val < MIN_RESULT_NUM)
    {
        //Get new data when data exception.
        return FALSE;
    }

    rssi_avg_val = (rssi_val_total / div_val);

    max_diff_val = 0;

    invalid_data_flag = FALSE;

    while (1)
    {
        //Check scatter status of samples, get the samples with big scatter.
        for (i = 0; i < result_num; i++)
        {
            ori_rssi_val = (int16)(data_buffer[i]);

            rssi_val = ori_rssi_val;

            if (ori_rssi_val != INVALID_RSSI_VAL)
            {

                diff_val = abs(rssi_avg_val - rssi_val);

                if (diff_val > max_diff_val)
                {
                    max_diff_index = i;
                    max_diff_val = diff_val;
                }
            }
        }

        //Check if the max scatter sample is out of limitation, if yes, remove it and get the next.
        if (max_diff_val > MAX_RSSI_DIFF_VAL)
        {
            //Max it as invalid.
            data_buffer[max_diff_index] = INVALID_RSSI_VAL;

            invalid_data_flag = TRUE;

            max_diff_val = 0;
        }
        else
        {
            break;
        }
    }

    //If there is invalid sample, re-calculate cfo average.
    if (invalid_data_flag == TRUE)
    {
        rssi_val_total = 0;

        div_val = 0;

        for (i = 0; i < result_num; i++)
        {
            ori_rssi_val = (int16)(data_buffer[i]);

            rssi_val = ori_rssi_val;

            if (ori_rssi_val != INVALID_RSSI_VAL)
            {
                rssi_val_total += rssi_val;
                div_val++;
            }
            else
            {
                continue;
            }
        }

        //At least 5 records.
        if (div_val < MIN_RESULT_NUM)
        {
            //If more cfo invalid occurs, get new samples.
            return FALSE;
        }

        rssi_avg_val = (rssi_val_total / div_val);

    }

    *rssi_return = rssi_avg_val;

    print_log("div:%d,rssi:%d\n",div_val,rssi_avg_val);

    return TRUE;
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

bool __ENTRY_CODE pattern_main(struct att_env_var *para)
{
    bool ret_val = false;

	memset(&__bss_start, 0,
		 ((u32_t) &__bss_end - (u32_t) &__bss_start));

	self = para;
	return_data = (return_result_t *) (self->return_data_buffer);
	trans_bytes = 0;

	act_test_read_pwr_test_arg(self->line_buffer, self->arg_buffer, self->arg_len);
	ret_val = act_test_pwr_test(self->arg_buffer);

    return ret_val;
}

