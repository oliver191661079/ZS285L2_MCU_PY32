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

static uint32 libc_abs(int32 value)
{
    if (value > 0)
    {
        return value;
    }
    else
    {
        return (0 - value);
    }
}

bool att_bttool_tx_begin(uint32 channel)
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

    ret_val = att_write_data(STUB_CMD_ATT_CFO_TX_BEGIN, sizeof(bttools_mp_param_t), STUB_ATT_RW_TEMP_BUFFER);

    if (ret_val == 0)
    {
        ret_val = att_read_data(STUB_CMD_ATT_ACK, 0, STUB_ATT_RW_TEMP_BUFFER);

        if(ret_val != 0)
        {
            print_log("att bttool tx begin fail\n");
            return FALSE;
        }
    }
    else
    {
        print_log("att bttool tx begin fail\n");
        return FALSE;
    }

    return TRUE;
}

bool att_bttool_tx_stop(void)
{
    return att_cmd_send(STUB_CMD_ATT_CFO_TX_STOP, 2);
}

bool att_cfo_result_calc(int16 *data_buffer, uint32 result_num, int16 *cfo_return)
{
    int32 i;
    int16 cfo_val;
    int32 diff_val;
    int16 cfo_avg_val;
    int8 div_val;
    int32 max_cfo_val;
    int32 min_cfo_val;
    int32 cfo_val_total;
    int32 ori_cfo_val;
    int32 max_diff_val;
    int32 max_diff_index;
    int32 invalid_data_flag;

    div_val = 0;

    cfo_avg_val = 0;

    max_cfo_val = -INVALID_CFO_VAL;

    min_cfo_val = 0x7fff;

    cfo_val_total = 0;

    //First average of total
    for (i = 0; i < result_num; i++)
    {
        ori_cfo_val = (int16)(data_buffer[i]);

        cfo_val = ori_cfo_val;

        if (ori_cfo_val != INVALID_CFO_VAL)
        {
        //print_log("cfo result num[%d]: %d hz, pwr: %d\n", i, cfo_val, pwr_val);
            cfo_val_total += cfo_val;
            div_val++;

            if (cfo_val > max_cfo_val)
            {
                max_cfo_val = cfo_val;
            }

            if (cfo_val < min_cfo_val)
            {
                min_cfo_val = cfo_val;
            }
        }
    }

    //At least 10 array
    if (div_val < MIN_RESULT_NUM)
    {
        //Data exception, to get again.
        return FALSE;
    }

    cfo_avg_val = (cfo_val_total / div_val);

    //avarage of cfo cann't be 0, to get a proper value.
    if (cfo_avg_val == 0)
    {
        cfo_avg_val = (max_cfo_val + min_cfo_val) / 2;

        if ((cfo_avg_val == 0) && (max_cfo_val != 0))
        {
            cfo_avg_val = max_cfo_val;
        }
    }

    max_diff_val = 0;

    invalid_data_flag = FALSE;

    while (1)
    {
        //Check scatter status of samples, get the samples with big scatter.
        for (i = 0; i < result_num; i++)
        {
            ori_cfo_val = (int16)(data_buffer[i]);

            cfo_val = ori_cfo_val;

            if (ori_cfo_val != INVALID_CFO_VAL)
            {

                diff_val = libc_abs(cfo_avg_val - cfo_val);

                if (diff_val > max_diff_val)
                {
                    max_diff_index = i;
                    max_diff_val = diff_val;
                }
            }
        }

        //Check if the max scatter sample is out of limitation, if yes, remove it and get the next.
        if (max_diff_val > MAX_CFO_DIFF_VAL)
        {
            //Max it as invalid.
            data_buffer[max_diff_index] = INVALID_CFO_VAL;

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
        cfo_val_total = 0;

        div_val = 0;

        for (i = 0; i < result_num; i++)
        {
            ori_cfo_val = (int16)(data_buffer[i]);

            cfo_val = ori_cfo_val;

            if (ori_cfo_val != INVALID_CFO_VAL)
            {
                cfo_val_total += cfo_val;
                div_val++;
            }
            else
            {
                continue;
            }

            if (cfo_val > max_cfo_val)
            {
                max_cfo_val = cfo_val;
            }

            if (cfo_val < min_cfo_val)
            {
                min_cfo_val = cfo_val;
            }
        }

        //At least 5 records.
        if (div_val < MIN_RESULT_NUM)
        {
            //If more cfo invalid occurs, get new samples.
            //g_add_cfo_result_flag = TRUE;
            return FALSE;
        }

        cfo_avg_val = (cfo_val_total / div_val);

		//avarage of cfo cann't be 0, to get a proper value.
        if (cfo_avg_val == 0)
        {
            cfo_avg_val = (max_cfo_val + min_cfo_val) / 2;

            if ((cfo_avg_val == 0) && (max_cfo_val != 0))
            {
                cfo_avg_val = max_cfo_val;
            }
        }
    }

    printk("div:%d,cfo:%d\n",div_val,cfo_avg_val);
    //cfo_return_arg->cfo_val = cfo_avg_val;
    *cfo_return = cfo_avg_val;

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

    //At least 10 array.
    if (div_val < MIN_RESULT_NUM)
    {
        //Data exception, to get again.
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

	//If there is invalid sample, re-calculate rssi average.
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
            //If more rssi invalid occurs, get new samples.
            return FALSE;
        }

        rssi_avg_val = (rssi_val_total / div_val);

    }

    *rssi_return = rssi_avg_val;

    printk("div:%d,rssi:%d\n",div_val,rssi_avg_val);

    return TRUE;
}


#if 0
// This array is from the Frequency deviation test of 5120 chip.
//The bigger cfo deviation, the bigger of Positive and Negative deviation. So here is 20 values.
const uint16 cfo_step[]=
{1, 1, 2, 3, 5, 7, 9, 10, 11, 13, 15, 16, 19, 20, 22, 24, 25, 27, 29, 31, 33, 34, 36};

//Get CFO deviation from experience.
int find_step_size_cfo(int16 cfo_val)
{
    int i, step_value;

    step_value = libc_abs(cfo_val) / 10;
    if(step_value == 0)
    {
        step_value = 1;
    }

    for(i = 1; i < sizeof(cfo_step)/sizeof(cfo_step[0]) - 1; i++)
    {
        if(step_value == cfo_step[i] || step_value < cfo_step[i + 1])
        {
            return i;
        }
    }
    return -1;
}
#endif

bool att_mp_cfo_test(mp_test_arg_t *mp_arg, uint32 channel, int16 ref_cap, int16 *return_cap)
{

    int16 cfo_val;

    int32 left;
    int32 right;
    int32 cfo_threshold_low;
    int32 cfo_threshold_high;
    int32 ret_val;
    uint32 cap_index;

    int16 last_negative_value = -INVALID_CFO_VAL;
    int16 last_positive_value = INVALID_CFO_VAL;
    uint32 n_cap_index = 0;
    uint32 p_cap_index = 0;
    uint32 try_count = 0;
	uint32 right_cfo = 0;

    bool cfo_test_end_flag = FALSE;

    cap_index = ref_cap;

    left = mp_arg->cfo_index_low;
    right = mp_arg->cfo_index_high;

    cfo_threshold_low = mp_arg->cfo_threshold_low;
    cfo_threshold_high = mp_arg->cfo_threshold_high ;

	int32 cfo_center_val = (cfo_threshold_low + cfo_threshold_high)/2;

    print_log("cfo test centor value %d\n", cfo_center_val);
	cfo_center_val = 0 - cfo_center_val;


    if(right > 240)
    {
        right = 240;
    }

    if(cap_index < left || cap_index > right)
    {
        cap_index = (left + right) / 2;
    }

retry:

    try_count ++;

    printk("test index:%d \n",cap_index);

    ret_val = mp_cfo_test(channel, cap_index, &cfo_val);

    //If timeout of first time recerving package, check if deviation of reference capacitance of first time tested us too big, which cause the timeout.
    if(try_count == 1 && ret_val == -2)
    {
        cap_index = (left + right) / 2;
        goto retry;
    }

    if (ret_val < 0)
    {
        print_log("cfo test fail,try count:%d, ret:%d\n", try_count, ret_val);
        goto test_fail;
    }

    print_log("cap_index is %d,cfo_val is %d \n",cap_index,cfo_val);
    //To make cfo deviation is arround 0 on both positive and negative sides.
    cfo_val = 0 - cfo_val;


    if(cfo_val > cfo_center_val)
    {
        if(cfo_val < last_positive_value)
        {
            //Record cfo value and capacitance value when cfo is positive value.
            last_positive_value = cfo_val;
            p_cap_index = cap_index;
        }
        else
        {
            if((cfo_val == last_positive_value) && (cap_index - left) >= 2){

            }else
            {
                print_log("mp test fail, direction err,current cfo:%d,last positive cfo:%d, cap%d\n",
                            cfo_val, last_positive_value, cap_index);
                goto test_fail;
            }
        }
        right = cap_index;
    }
    else if(cfo_val < cfo_center_val)
    {
        if(cfo_val > last_negative_value)
        {
            //Record cfo value and capacitance value when cfo is negative value.
            last_negative_value = cfo_val;
            n_cap_index = cap_index;
        }
        else
        {
            if((cfo_val == last_negative_value) && (right - cap_index) >= 2){

            }else{
                print_log("mp test fail, direction err,current cfo:%d,last negative cfo:%d, cap%d\n",
                            cfo_val, last_negative_value, cap_index);
                goto test_fail;
            }
        }
        left = cap_index;
    }
    else
    {
        //End
        last_positive_value = cfo_val;
        p_cap_index = cap_index;
        cfo_test_end_flag = TRUE;
		right_cfo = 1;
		print_log("get right cfo %dpf\n", cap_index);
        goto test_end;
    }

    //Check is test is end.
    if(right - left <= 1)
    {
        //Test end
        cfo_test_end_flag = TRUE;
        goto test_end;
    }

    //dichotomy
    cap_index = ((right + left)/2);


test_end:

    if(cfo_test_end_flag == TRUE)
    {
        //Record cfo value and capacitance value when cfo is near 0.
		if(right_cfo){
			cfo_val = last_positive_value;
			cap_index = p_cap_index;
		}else{
			cfo_val = libc_abs(last_positive_value) < libc_abs(last_negative_value)
				? last_positive_value : last_negative_value;

			cap_index = libc_abs(last_positive_value) < libc_abs(last_negative_value)
				? p_cap_index : n_cap_index;
		}

        cfo_val = 0 - cfo_val;
        
        if (cfo_val >= cfo_threshold_low && cfo_val <= cfo_threshold_high)
        {
            *return_cap = cap_index;
            printk("cfo val: %d,cap: %u,ref cap: %d,count: %d\n", cfo_val, cap_index, ref_cap, try_count);
            return TRUE;
        }
        else
        {
            print_log("not found proper cfo,current cfo val: %d, cap: %u\n", cfo_val, cap_index);
            return FALSE;
        }
    }

    print_log("test cap:%d ,[%d,%d]",cap_index,left,right);
    goto retry;

test_fail:

    return FALSE;
}


