/*******************************************************************************
 *                              US212A
 *                            Module: MainMenu
 *                 Copyright(c) 2003-2012 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>    <time>           <version >             <desc>
 *       zhangxs     2011-09-05 10:00     1.0             build this file
 *******************************************************************************/
/*!
 * \file     music_channel_test.c
 * \brief    Channel test
 * \author   zhangxs
 * \version  1.0
 * \date  2011/9/05
 *******************************************************************************/
#include "ap_autotest_channel_test.h"

struct att_channel_test_stru *att_channel_test;

//1T sin wave
const uint16 pcm_table[]=
{
    0x6ba,  0x371f, 0x5f22, 0x78a6, 0x7fce, 0x7328, 0x55c9, 0x2ab2,
    0xf945, 0xc8df, 0xa0de, 0x8757, 0x802f, 0x8c7f, 0xaa63, 0xd550
};

//buffer 16*16*2*2=1k
void init_dac_buffer(uint16 *p_dac_buffer)
{
    uint8 i, j;

    for (j = 0; j < 16; j++)
    {
        for (i = 0; i < 16; i++)
        {
            //Use same data for left and right channel.
            *p_dac_buffer = pcm_table[i];
            p_dac_buffer++;
            /**p_dac_buffer = pcm_table[i];
            p_dac_buffer++;*/
        }
    }

    return;
}

int32 caculate_power_value(uint8_t *dac_buffer_addr, uint32 data_length, uint32 *power_val_array, uint32 index)
{
    uint32 i;
    uint32 power_value = 0;
    uint32 power_sample_value = 0;

    int16 *dac_buffer = (int16 *)dac_buffer_addr;

    //Samples from one single channel
    for (i = 0; i < data_length; i += 2)
    {
        if (dac_buffer[i] >= 0)
        {
            power_sample_value = (uint32)(dac_buffer[i]);
        }
        else
        {
            power_sample_value = (uint32)(-dac_buffer[i]);
        }

        power_value += power_sample_value;
    }

    power_value = power_value / (data_length / 2);

    power_val_array[index] = power_value;

    att_write_test_info("power value:", power_value, 1);
    return 0;
}

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

int32 analyse_power_val_valid(uint32 sample_cnt, uint32 *power_val_array, uint32 *p_power_val_left, uint32 *p_power_val_right)
{
    uint32 i;
    uint32 power_val;
    uint32 power_val_total;
    uint32 div_val;

    int32 diff_val;

    int32 max_diff_val;
    int32 max_diff_index;
    int32 invalid_data_flag;

    int32 analyse_flag = 0;

    uint32 sample_count = (sample_cnt >> 1);

    retry:
    power_val_total = 0;

    for(i = 0; i < sample_count; i++)
    {
        power_val_total += power_val_array[i*2 + analyse_flag];
    }

    power_val = power_val_total / i;

    invalid_data_flag = FALSE;

    max_diff_val = 0;

    while (1)
    {
        //Get scatter of status of samples, get the biggest scatter sample
        for (i = 0; i < sample_count; i++)
        {
            if (power_val_array[i*2 + analyse_flag] != INVALID_POWER_VAL)
            {
                diff_val = libc_abs(power_val_array[i*2 + analyse_flag] - power_val);

                if (diff_val > max_diff_val)
                {
                    max_diff_index = i;
                    max_diff_val = diff_val;
                }
            }
        }

        //Check if the biggest scatter sample is above limitation. If yes, skip it.
        if (max_diff_val > MAX_POWER_DIFF_VAL)
        {
            printk("invalid power: %d\n", power_val_array[max_diff_index*2 + analyse_flag]);

            printk("index: %d\n", max_diff_index);

            power_val_array[max_diff_index*2 + analyse_flag] = INVALID_POWER_VAL;

            invalid_data_flag = TRUE;

            max_diff_val = 0;

            power_val_total = 0;

            div_val = 0;

            for (i = 0; i < sample_count; i++)
            {
                if (power_val_array[i*2 + analyse_flag] != INVALID_POWER_VAL)
                {
                    power_val_total += power_val_array[i*2 + analyse_flag];

                    div_val++;
                }
            }

            power_val = (power_val_total / div_val);

            if(analyse_flag == 0)
            {
                printk("cal left power:%u\n", power_val);
            }
            else
            {
                printk("cal right power:%u\n", power_val);
            }
        }
        else
        {
            break;
        }
    }

    if (invalid_data_flag == TRUE)
    {
        power_val_total = 0;

        div_val = 0;

        for (i = 0; i < sample_count; i++)
        {
            if (power_val_array[i*2 + analyse_flag] != INVALID_POWER_VAL)
            {
                power_val_total += power_val_array[i*2 + analyse_flag];
                div_val++;
            }
            else
            {
                continue;
            }
        }

        if (div_val < 2)
        {
            return FALSE;
        }

        power_val = (power_val_total / div_val);
    }

    if(analyse_flag == 0)
    {
        *p_power_val_left = power_val;

        analyse_flag = 1;

        goto retry;
    }
    else
    {
        *p_power_val_right = power_val;
    }

    if((libc_abs(*p_power_val_left - power_val_array[sample_cnt - 2]) <= MAX_POWER_DIFF_VAL)
       && (libc_abs(*p_power_val_right - power_val_array[sample_cnt - 1]) <= MAX_POWER_DIFF_VAL))
    {
        return TRUE;
    }
    else
    {
        print_log("analyse input power val is invalid");
        return FALSE;
    }
}

void act_test_report_channel_result(uint16 test_id, int32 ret_val, void *arg_buffer)
{
    return_result_t *return_data;
    uint16 trans_bytes = 0;
    channel_test_arg_t *channel_test_arg = (channel_test_arg_t *)arg_buffer;

    if (att_get_test_mode() != TEST_MODE_CARD)
    {
        return_data = (return_result_t *) (STUB_ATT_RETURN_DATA_BUFFER);

        return_data->test_id = test_id;

        return_data->test_result = ret_val;

        int32_to_unicode(channel_test_arg->left_ch_power_threadshold, &(return_data->return_arg[trans_bytes]), &trans_bytes, 10);

        return_data->return_arg[trans_bytes++] = 0x002c;

        int32_to_unicode(channel_test_arg->right_ch_power_threadshold, &(return_data->return_arg[trans_bytes]), &trans_bytes, 10);

        return_data->return_arg[trans_bytes++] = 0x002c;

        int32_to_unicode(channel_test_arg->left_ch_SNR_threadshold, &(return_data->return_arg[trans_bytes]), &trans_bytes, 10);

        return_data->return_arg[trans_bytes++] = 0x002c;

        int32_to_unicode(channel_test_arg->right_ch_SNR_threadshold, &(return_data->return_arg[trans_bytes]), &trans_bytes, 10);

        return_data->return_arg[trans_bytes++] = 0x002c;

        //4 bytes alignment
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
}

static u8_t in_type = 0;
static uint8_t physical_dev = 0;
static uint8_t track_flag = 0;

static int stream_in_handle(void *callback_data, u32_t reason)
{
    int32 ret_val;
    if(att_channel_test->test_status == STATUS_NULL)
    {
        return 0;
    }
    else if(att_channel_test->test_status == STATUS_START)
    {
        att_channel_test->time_stamp = 0;
        att_channel_test->test_status = STATUS_ONGING;
        return 0;
    }
//    static int buffer = 0;
//    buffer ++;
//    printk("buffer is 0x%x ,input buffer 0x%x \n",buffer,(u32_t)att_channel_test->input_buffer);

//    if (reason == DMA_IRQ_TC)
    {
        if(track_flag == INPUTSRC_ONLY_L){
        //left channel
        caculate_power_value(att_channel_test->input_buffer,
                        att_channel_test->input_buffer_size/2,
                        att_channel_test->power_val_array,
                        att_channel_test->sample_count++);
        }else if(track_flag == INPUTSRC_ONLY_R){
            //right channel
            caculate_power_value(att_channel_test->input_buffer + 2,
                            att_channel_test->input_buffer_size / 2,
                            att_channel_test->power_val_array,
                            att_channel_test->sample_count++);
        }else if(track_flag == INPUTSRC_L_R){

            //right channel
            caculate_power_value(att_channel_test->input_buffer + 2,
                            att_channel_test->input_buffer_size / 2,
                            att_channel_test->power_val_array,
                            att_channel_test->sample_count++);

            //left channel
            caculate_power_value(att_channel_test->input_buffer,
                            att_channel_test->input_buffer_size / 2,
                            att_channel_test->power_val_array,
                            att_channel_test->sample_count++);

        }


        if(att_channel_test->sample_count >= MIN_POWER_SAMPLE_COUNT)
        {
            ret_val = analyse_power_val_valid(att_channel_test->sample_count,
                            att_channel_test->power_val_array,
                            &att_channel_test->power_val_left,
                            &att_channel_test->power_val_right);

            if(ret_val && att_channel_test->l_ch_test
                        && att_channel_test->power_val_left < att_channel_test->l_ch_pwr_thr)
            {
                ret_val = FALSE;
            }

            if(ret_val && att_channel_test->r_ch_test
                        && att_channel_test->power_val_right < att_channel_test->r_ch_pwr_thr)
            {
                ret_val = FALSE;
            }

            if(ret_val && att_channel_test->time_stamp == 0)
            {
                att_channel_test->time_stamp = k_uptime_get_32();
                ret_val = FALSE;
            }
            else if(ret_val && k_uptime_get_32() - att_channel_test->time_stamp < 200)
            {
                ret_val = FALSE;
            }
            else if(ret_val == FALSE)
            {
                att_channel_test->time_stamp = 0;
            }

            if(ret_val == TRUE)
            {
                att_channel_test->test_status = STATUS_NULL;
                k_sem_give(&att_channel_test->channel_test_sem);
            }
            else
            {
                att_channel_test->sample_count = 0;
            }
        }
    }

    return 0;
}


void att_channel_out_deinit(void)
{
    if(att_channel_test->aout_handle)
    {
        hal_aout_channel_stop(att_channel_test->aout_handle);
        hal_aout_channel_close(att_channel_test->aout_handle);
        att_channel_test->aout_handle = NULL;
    }
}


void att_channel_in_deinit(void)
{
    if(att_channel_test->ain_handle)
    {
        hal_ain_channel_stop(att_channel_test->ain_handle);
        hal_ain_channel_close(att_channel_test->ain_handle);
    }

}

void att_channel_close(void)
{
    att_channel_in_deinit();
    att_channel_out_deinit();
}

int audio_out_callback(void *cb_data, u32_t reason)
{
    return 0;
}


void att_channel_out_init(uint8 type)
{
    //hal_audio_out_context_t* audio_out = hal_audio_out_get_context();
    //struct device * dev = audio_out->aout_dev;
    //struct acts_audio_out_data *data = dev->driver_data;
    int32_t dac_gain;

    if(track_flag != INPUTSRC_ONLY_R)
    {
        dac_gain = -8000;
    }
    else
    {
        dac_gain = -4000;
    }


    init_dac_buffer(att_channel_test->output_buffer);
    att_channel_test->aout_param.aa_mode = 0;
	att_channel_test->aout_param.need_dma = 1;
//	att_channel_test->aout_param.need_dsp = 0;
	att_channel_test->aout_param.out_to_pa = 1;
	att_channel_test->aout_param.out_to_i2s = 0;
	att_channel_test->aout_param.out_to_spdif = 0;
    att_channel_test->aout_param.dma_reload = 1;
	att_channel_test->aout_param.asrc_setting = NULL;
    att_channel_test->aout_param.channel_id = AOUT_FIFO_DAC0;

    att_channel_test->aout_param.channel_type = AUDIO_CHANNEL_DAC;
    att_channel_test->aout_param.sample_rate = ADC_SAMPLE_RATE;
    att_channel_test->aout_param.right_volume = dac_gain;
    att_channel_test->aout_param.left_volume = att_channel_test->aout_param.right_volume;
    att_channel_test->aout_param.callback = audio_out_callback;
    att_channel_test->aout_param.callback_data = NULL;
    att_channel_test->aout_param.reload_addr = (u8_t *)att_channel_test->output_buffer;
    att_channel_test->aout_param.reload_len = att_channel_test->output_buffer_size;



//	att_channel_test->aout_param.ptr_dac_setting = &att_channel_test->dac_setting_param;
//	att_channel_test->aout_param.ptr_dac_setting->dac_sample_rate = ADC_SAMPLE_RATE;
//	att_channel_test->aout_param.ptr_dac_setting->dac_gain = dac_gain;
//	att_channel_test->aout_param.ptr_dac_setting->sample_cnt_enable = 0;

//	att_channel_test->aout_param.ptr_pa_setting = NULL;
//	att_channel_test->aout_param.ptr_asrc_setting = NULL;


	att_channel_test->aout_handle = (void *)hal_aout_channel_open(&att_channel_test->aout_param);

    printk("aout handle %p\n",att_channel_test->aout_handle);
	if(att_channel_test->aout_handle == NULL) {
		printk(" %s %d, open aout fail!\n", __func__,__LINE__);
		return;
	}

	//memset(&att_channel_test->aout_dma_cfg, 0, sizeof(dma_setting_t));
#if 0
	att_channel_test->aout_dma_cfg.dma_src_buf = (u8_t *)att_channel_test->output_buffer;
    att_channel_test->aout_dma_cfg.dma_trans_len = att_channel_test->output_buffer_size;
    att_channel_test->aout_dma_cfg.dma_width = 2; //2--16bit;4--32bit;
	att_channel_test->aout_dma_cfg.dma_reload = 1;
	att_channel_test->aout_dma_cfg.dma_interleaved = 1;
    // att_channel_test->aout_dma_cfg.dma_sam = 0;

	att_channel_test->aout_dma_cfg.stream_handle = NULL;
	att_channel_test->aout_dma_cfg.stream_pdata = NULL;
	att_channel_test->aout_dma_cfg.all_complete_callback_en = 0;
	att_channel_test->aout_dma_cfg.half_complete_callback_en = 0;

    if(!data->dma_dev){
        return ;
	}
#endif

    //att_channel_test->aout_handle->dma_run_flag = false;
    //att_channel_test->aout_handle->aout_dma = dma_request(data->dma_dev, 0xff);
    //audio_out_dma_config(dev, att_channel_test->aout_handle, &att_channel_test->aout_dma_cfg);
    //acts_aout_start(dev, att_channel_test->aout_handle, true);

	hal_aout_channel_write_data(att_channel_test->aout_handle, (u8_t *)att_channel_test->output_buffer, \
		att_channel_test->output_buffer_size);

	hal_aout_channel_start(att_channel_test->aout_handle);

    return ;


}

void att_channel_in_init(uint8 type)
{

    audio_in_init_param_t ain_param;
//    adc_setting_t adc_setting_param;
    input_setting_t input_setting_param;

    if(track_flag != INPUTSRC_ONLY_R)
    {
        input_setting_param.input_gain  = 1;
    }
    else
    {
        input_setting_param.input_gain  = 3;
    }

    memset(&ain_param, 0, sizeof(ain_param));

    ain_param.need_dma = 1;
    ain_param.dma_reload = 1;
    ain_param.data_mode = AUDIO_MODE_STEREO;
    ain_param.sample_rate = SAMPLE_16KHZ;
    ain_param.channel_type = AUDIO_CHANNEL_ADC;

    ain_param.audio_device = type;
    ain_param.input_gain = input_setting_param.input_gain;
    ain_param.reload_addr = att_channel_test->input_buffer;
    ain_param.reload_len = att_channel_test->input_buffer_size;

    ain_param.callback = stream_in_handle;
    ain_param.callback_data = NULL;

	att_channel_test->ain_handle = (void *)hal_ain_channel_open(&ain_param);

    if(att_channel_test->ain_handle == NULL)
    {
		printk("%s ain open failed\n",__func__);
        return ;
    }


    hal_ain_channel_read_data(att_channel_test->ain_handle, att_channel_test->input_buffer, att_channel_test->input_buffer_size);
    hal_ain_channel_start(att_channel_test->ain_handle);


    return;
}


int32 channel_test(void *arg_buffer, uint8_t type)
{
    int32 ret_val = TRUE;
    channel_test_arg_t *channel_test_arg = (channel_test_arg_t *)arg_buffer;

    att_channel_test = z_malloc(sizeof(struct att_channel_test_stru));

    att_channel_test->input_buffer_size = 4096;
    att_channel_test->input_buffer = z_malloc(att_channel_test->input_buffer_size);

    att_channel_test->output_buffer_size = 512;
    att_channel_test->output_buffer = z_malloc(att_channel_test->output_buffer_size);

    att_channel_test->l_ch_pwr_thr = channel_test_arg->left_ch_power_threadshold;
    att_channel_test->r_ch_pwr_thr = channel_test_arg->right_ch_power_threadshold;
    att_channel_test->l_ch_test = channel_test_arg->test_left_ch;
    att_channel_test->r_ch_test = channel_test_arg->test_right_ch;

    k_sem_init(&att_channel_test->channel_test_sem, 0, 1);

    in_type = type;
    board_audio_device_mapping(in_type, &physical_dev, &track_flag);

    att_channel_out_init(type);

    att_channel_test->test_status = STATUS_START;

    att_channel_in_init(type);

    ret_val = k_sem_take(&att_channel_test->channel_test_sem, 5000);

    att_channel_close();

    if(ret_val < 0)
    {
        print_log("channel test timeout\n");
        ret_val = FALSE;
        goto exit;
    }
    else
    {
        ret_val = TRUE;
    }

    if(channel_test_arg->test_left_ch == TRUE)
    {
        if(att_channel_test->power_val_left < channel_test_arg->left_ch_power_threadshold)
        {
            print_log("left power value too low:%d\n", att_channel_test->power_val_left);
            ret_val = FALSE;
        }
    }

    if(channel_test_arg->test_right_ch == TRUE)
    {
        if(att_channel_test->power_val_right < channel_test_arg->right_ch_power_threadshold)
        {
            print_log("right power value too low:%d\n", att_channel_test->power_val_right);
            ret_val = FALSE;
        }
    }

    if(ret_val == FALSE)
    {
        channel_test_arg->left_ch_power_threadshold = att_channel_test->power_val_left;
        channel_test_arg->right_ch_power_threadshold = att_channel_test->power_val_right;
    }

    if(ret_val == TRUE)
    {
        ret_val = thd_test(att_channel_test->input_buffer + 1024, channel_test_arg);
    }
    else
    {
        channel_test_arg->left_ch_SNR_threadshold = 0;
        channel_test_arg->left_ch_max_sig_point = 0;
        channel_test_arg->right_ch_SNR_threadshold = 0;
        channel_test_arg->right_ch_max_sig_point = 0;
    }

exit:
    free(att_channel_test->output_buffer);
    free(att_channel_test->input_buffer);
    free(att_channel_test);
    return ret_val;
}
/*
 请根据实际测试情况更改一下配置！！！
 当使用测试架测试时，应该配置成n；当使用DVB板时配置成y
CONFIG_ATT_CHANNEL_AUXFD=y
*/
#if 0
#define CONFIG_ATT_CHANNEL_AUXFD 1
#else
#ifdef CONFIG_ATT_CHANNEL_AUXFD
#undef CONFIG_ATT_CHANNEL_AUXFD
#endif
#endif
test_result_e act_test_linein_channel_test(void *arg_buffer)
{
    int32 ret_val = 0;

#ifdef CONFIG_ATT_CHANNEL_AUXFD
    ret_val = channel_test(arg_buffer, AIN_LOGIC_SOURCE_ATT_AUXFD);
    if(!ret_val)
    {
        print_log("linein channel auxfd test fail\n");
    }

#else
    ret_val = channel_test(arg_buffer, AIN_LOGIC_SOURCE_ATT_AUX0);

    if(!ret_val)
    {
        ret_val = channel_test(arg_buffer, AIN_LOGIC_SOURCE_ATT_AUX1);
        if(!ret_val)
        {
            print_log("linein channel aux1 test fail\n");
            ret_val = channel_test(arg_buffer, AIN_LOGIC_SOURCE_ATT_AUXFD);

            if(!ret_val)
            {
                print_log("linein channel auxfd test fail\n");
            }
        }
    }
    else
    {
        print_log("linein channel aux0 test fail\n");
    }
#endif

    if (ret_val == FALSE)
    {
        att_write_test_info("linein channel test failed", 0, 0);
    }
    else
    {
        att_write_test_info("linein channel test ok", 0, 0);
    }

    act_test_report_channel_result(TESTID_LINEIN_CH_TEST_ATS28XX, ret_val, arg_buffer);

    return ret_val;
}

test_result_e act_test_linein_channel_test_ATS28XX(void *arg_buffer)
{
    return act_test_linein_channel_test(arg_buffer);
}

test_result_e act_test_mic_channel_test(void *arg_buffer)
{
    int32 result;

    result = channel_test(arg_buffer, AIN_LOGIC_SOURCE_MIC0);
    if(result == FALSE){
        print_log("mic channel mic0 failed\n");
        result = channel_test(arg_buffer, AIN_LOGIC_SOURCE_MIC1);
        if(result == FALSE){
            print_log("mic channel mic1 failed\n");
            result = channel_test(arg_buffer, AIN_LOGIC_SOURCE_DMIC);
            if(result == FALSE){
                print_log("mic channel DMIC failed\n");
                result = channel_test(arg_buffer, AIN_LOGIC_SOURCE_FM);
                if(result == FALSE){
                    print_log("mic channel FM failed\n");
                }
            }
        }
    }

    if (result == FALSE)
    {
        att_write_test_info("mic channel test failed", 0, 0);
    }
    else
    {
        att_write_test_info("mic channel test ok", 0, 0);
    }

    act_test_report_channel_result(TESTID_MIC_CH_TEST, result, arg_buffer);

    return result;
}

int32 act_test_read_channel_test_arg(uint16 *line_buffer, uint8 *arg_buffer, uint32 arg_len)
{
    uint16*  start = NULL;
    uint16*  end   = NULL;
    uint8 arg_num;
    channel_test_arg_t *channel_test_arg = (channel_test_arg_t *)arg_buffer;

    arg_num = 1;

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);
    channel_test_arg->test_left_ch = (uint8)unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);
    channel_test_arg->left_ch_power_threadshold = (uint32)unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);
    channel_test_arg->test_left_ch_SNR = (uint8)unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);
    channel_test_arg->left_ch_SNR_threadshold = (uint32)unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);
    channel_test_arg->left_ch_max_sig_point = unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);
    channel_test_arg->test_right_ch = (uint8)unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);
    channel_test_arg->right_ch_power_threadshold = (uint32)unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);
    channel_test_arg->test_right_ch_SNR = (uint8)unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);
    channel_test_arg->right_ch_SNR_threadshold = (uint32)unicode_to_int(start, end, 10);

    act_test_parse_test_arg(line_buffer, arg_num++, &start, &end);
    channel_test_arg->right_ch_max_sig_point = (uint8)unicode_to_int(start, end, 10);

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

	if (self->arg_len){
		act_test_read_channel_test_arg(self->line_buffer, self->arg_buffer, self->arg_len);
	}

	if (self->test_id == TESTID_LINEIN_CH_TEST_ATS28XX) {
		ret_val = act_test_linein_channel_test(self->arg_buffer);
	} else if (self->test_id == TESTID_MIC_CH_TEST) {
		ret_val = act_test_mic_channel_test(self->arg_buffer);
	}else /*TESTID_FM_CH_TEST*/ {
		//ret_val = act_test_audio_fm_in(&g_fm_rx_test_arg);
	}

    return ret_val;
}

