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
 * \file
 * \brief
 * \author   zhangxs
 * \version  1.0
 * \date  2011/9/05
 *******************************************************************************/
#include "att_pattern_test.h"
#include "ap_autotest_channel_test.h"
#include "ap_autotest_channel_test_SNR.h"


/******************************************************************************/
/*!
 * \par  Description:
 *   Get bit revers to binary value of given length.
 * \param[in]  N: value
 * \param[in]  L: bit length
 * \param[out] none
 * \return     N revers value
 * \par
 * \note
 * \li   Result is stoed in global variable BitRev.
 *******************************************************************************/
LEN_TYPE DoBitRev(LEN_TYPE N, LEN_TYPE L)
{
    LEN_TYPE Temp1, Temp2;
    LEN_TYPE i;
    for (i = 0; i < (L / 2); i++)
    {
        Temp1 = 0;
        Temp2 = 0;
        if ((N & (1 << i)) != 0)
        {
            Temp1 = 1;
        }

        if ((N & (1 << (L - 1 - i))) != 0)
        {
            Temp2 = 1;
        }

        N &= ~(1 << i);
        N &= ~(1 << (L - 1 - i));
        N |= (Temp2 << i);
        N |= (Temp1 << (L - 1 - i));
    }
    return N;
}

/******************************************************************************/
/*!
 * \par  Description:
 *   To initialize revers value for FFT calclulating.
 * \param[in]  none
 * \param[out] none
 * \return     none
 * \par
 * \note
 * \li   Result is stoed in global variable BitRev.
 *******************************************************************************/
void InitBitRev(LEN_TYPE *BitRev)
{
    LEN_TYPE i;
    for (i = 0; i < LENGTH; i++) //bit revers
    {
        BitRev[i] = DoBitRev(i, bL);
    }
}

/******************************************************************************/
/*!
 * \par  Description:
 *   To do revers to input data
 * \param[in]  pIn data in buffer
 * \param[out] pIn data in buffer
 * \return     none
 * \par
 * \note
 * \li   Before calling to FftExe, call this fucntion to do data revers handling.
 *******************************************************************************/
void FftInput(IN_TYPE *pIn, LEN_TYPE *BitRev)
{
    LEN_TYPE i;
    IN_TYPE Temp;
    for (i = 0; i < LENGTH; i++)
    {
        if (BitRev[i] > i) //Exchange when revers bit is bigger.
        {
            Temp = pIn[i];
            pIn[i] = pIn[BitRev[i]];
            pIn[BitRev[i]] = Temp;
        }
    }
}

/******************************************************************************/
/*!
 * \par  Description:
 *   Do FFT calculating.
 * \param[in]  pIn Input data buffer
 * \param[out] pRe output real part
 * \param[out] pIm output imaginary part
 * \return     none
 * \par
 * \note
 * \li   Before calling this function, it should call FftInput to do data revers.
 *******************************************************************************/
void FftExe(IN_TYPE *pIn, OUT_TYPE *pRe, OUT_TYPE *pIm)
{
    LEN_TYPE i, j;
    LEN_TYPE BlockSize;
    OUT_TYPE tr, ti;
    LEN_TYPE OffSet1, OffSet2;

#ifdef USE_TABLE
    LEN_TYPE OffSet0;
#endif

    long c, s;

    //First calculate points of 2.
    for (j = 0; j < LENGTH; j += 2)
    {
        tr = pIn[j + 1];
        pRe[j + 1] = (pIn[j] - tr);
        pIm[j + 1] = 0;
        pRe[j] = (pIn[j] + tr);
        pIm[j] = 0;
    }

    for (BlockSize = 4; BlockSize <= LENGTH; BlockSize <<= 1) //Then one by one
    {
        for (j = 0; j < LENGTH; j += BlockSize)
        {
            for (i = 0; i < (BlockSize / 2); i++)
            {
#ifndef USE_TABLE
                c=(long)(1024*cos(2*PI*i/BlockSize));
                s=(long)(1024*sin(2*PI*i/BlockSize));
#else
                OffSet0 = LENGTH / BlockSize * i;
                c = COS_TABLE[OffSet0];
                s = SIN_TABLE[OffSet0];
#endif

                OffSet1 = i + j;
                OffSet2 = OffSet1 + BlockSize / 2;
                tr = (OUT_TYPE)((c * pRe[OffSet2] + s * pIm[OffSet2]) / 1024);
                ti = (OUT_TYPE)((c * pIm[OffSet2] - s * pRe[OffSet2]) / 1024);
#ifdef UNITARY  //If do unitary, devide 2 in every calculte
                pRe[OffSet2]=(pRe[OffSet1]-tr)/2;
                pIm[OffSet2]=(pIm[OffSet1]-ti)/2;
                pRe[OffSet1]=(pRe[OffSet1]+tr)/2;
                pIm[OffSet1]=(pIm[OffSet1]+ti)/2;
#else
                pRe[OffSet2] = (pRe[OffSet1] - tr);
                pIm[OffSet2] = (pIm[OffSet1] - ti);
                pRe[OffSet1] = (pRe[OffSet1] + tr);
                pIm[OffSet1] = (pIm[OffSet1] + ti);
#endif
            }
        }
    }
#ifdef UNITARY
    pRe[0]/=2;
    pIm[0]/=2;
#endif
}

/*****************************************/
/*Function: sqrt handling                                */
/*input: value to be sqrt, 32 bits32 interger           */
/*return: result, 16 bits interger           */
/****************************************/
unsigned int sqrt_fixed(unsigned long a)
{
    unsigned long i,c;
    unsigned long b = 0;
    for(i = 0x40000000; i != 0; i >>= 2)
    {
        c = i + b;
        b >>= 1;
        if(c <= a)
        {
            a -= c;
            b += i;
        }
    }
    return (unsigned int)b;
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

__int64 mul64(__int64 val0, __int64 val1)
{
    __int64 temp_val = val0 * val1;

    return temp_val;
}


/******************************************************************************/
/*!
 * \par  Description:
 *   Do spectrum analysis to record audio data
 * \param[in] pdata audio data buffer
 * \param[in] sample_rate Sample rate KHZ
 * \return    Whether samples fullfill spectrum specification
 * \par
 * \note
 * \li   Input source is requred fixed 1KHz sin wave. As no sqrt calculate, it can not do amplitude analysis; 
		But according to read part feature, it can genenrally analysiss data feature.
 *******************************************************************************/
uint8 analyse_sound_data(IN_TYPE *pdata, uint16 sample_rate, uint32 *p_snr, uint32 *max_point)
{
    uint8 i;
   // int16 max_value = 0;
    //uint32 *pRe = &Re;

    //uint32 total_value = 0;
    uint32 snr;

    uint8 Sig_Max_Point = 0;

    __int64 tmp_sum,  tmp1, tmp2,Sig_Max, Noise, Sig;

    //real part
    OUT_TYPE *Re;

    //imaginary part
    OUT_TYPE *Im;

    //revers value
    LEN_TYPE *BitRev;

    int AmpMax; //Amplitude max value
    int AmpMin; //Amplitude min value

    //To calculate middle point.
    //uint8 mean_index = (uint8)(LENGTH / sample_rate);

    AmpMax = pdata[0];
    AmpMin = pdata[0];

    //Get max and min value in time area.
    for (i = 0; i < LENGTH; i++)
    {
        if (AmpMax < pdata[i])
            AmpMax = pdata[i];
        if (AmpMin > pdata[i])
            AmpMin = pdata[i];
    }

    Re = z_malloc(LENGTH * sizeof(OUT_TYPE));

    Im = z_malloc(LENGTH * sizeof(OUT_TYPE));

    BitRev = z_malloc(LENGTH * sizeof(LEN_TYPE));

    load_win_data();

    //Ti initialize invers bit array.
    InitBitRev(BitRev);

    //Do invers to data.
    FftInput(pdata, BitRev);

    //Do fft transfer
    FftExe(pdata, Re, Im);

    Sig_Max = 0;
    tmp_sum = 0;

    //Jump first DC component. Analysis half data, as FFT has symmetry.
    for (i = 1; i < (LENGTH / 2); i++)
    {
        Re[i] = libc_abs(Re[i]);

        Im[i] = libc_abs(Im[i]);

        //libc_print("Re0", Re[i], 2);

        //libc_print("Im0", Im[i], 2);

        //Notice: 64bits muliple.
        tmp1 = mul64(Re[i], Re[i]);

		tmp2 = mul64(Im[i], Im[i]);

		tmp1 += tmp2;

        if (Sig_Max < tmp1)
        {
            Sig_Max = tmp1;
            Sig_Max_Point = i;
        }

        tmp_sum += tmp1;

        //Some data printing has Rounding error
        //libc_print("Re1", tmp1, 2);

        //libc_print("\n", 0, 0);
    }

    printk("Sig max point:%d\n", Sig_Max_Point);

   //Find signal energy max point and index, get ony half.

    Sig = Sig_Max; //Each side of max value N_sig-1

    for (i = 1; i < 5; i++)
    {
        tmp1 = mul64(Re[Sig_Max_Point - i], Re[Sig_Max_Point - i]) + mul64(Im[Sig_Max_Point - i], Im[Sig_Max_Point - i]);

        tmp2 = mul64(Re[Sig_Max_Point + i], Re[Sig_Max_Point + i])+ mul64(Im[Sig_Max_Point + i], Im[Sig_Max_Point + i]);

        Sig = Sig + tmp1 + tmp2;
    }

    Noise = tmp_sum - Sig;

    //printk("Sig:%d\n", Sig);

    //printk("Total:%d\n", tmp_sum);

    //printk("Noise:%d\n", Noise);

    snr = (int) ((((int) (Sig >> 15)) / ((int) (Noise >> 10))) << 5);//log

    printk("SNR:%u\n", snr);

    *p_snr = snr;

    *max_point = Sig_Max_Point;

    free(Re);

    free(Im);

    free(BitRev);

    return 0;
}

uint32 cal_threadshold(uint32 threadshold_db)
{
    uint32 i;
    uint32 tmp_val;
    uint32 shift_cnt = threadshold_db / 10;

    tmp_val = 1;
    for(i = 0; i < shift_cnt; i++)
    {
        tmp_val *= 10;
    }
    return tmp_val;
}


uint32 thd_test(void *buffer_addr, channel_test_arg_t *channel_test_arg)
{
    int32 i;

    uint32 snr;
    uint32 max_point;

    uint32 ret_val = TRUE;

    IN_TYPE *p_data_buffer = (IN_TYPE *)buffer_addr;

    //read_temp_file(1, LINEIN_SOUND_DATA_ADDR, SOUND_DATA_LEN);

    //Exchange left and right channel
    for(i = 0; i < (LENGTH * 2); i++)
    {
        p_data_buffer[i + (LENGTH * 2)] = p_data_buffer[i * 2 + 1];
        p_data_buffer[i] = p_data_buffer[i * 2];
    }

    if(channel_test_arg->test_left_ch == TRUE)
    {
        if(channel_test_arg->test_left_ch_SNR == TRUE)
        {
            analyse_sound_data(p_data_buffer, ADC_SAMPLE_RATE, &snr, &max_point);

            if(snr >= cal_threadshold(channel_test_arg->left_ch_SNR_threadshold)
                && channel_test_arg->left_ch_max_sig_point == max_point)
            {
                ret_val = TRUE;
            }
            else
            {
                ret_val = FALSE;
            }

            //channel_test_arg->left_ch_SNR_threadshold = snr;
            //channel_test_arg->left_ch_max_sig_point = max_point;

           // if(g_test_mode == TEST_MODE_CARD)
          //  {
           //     DEBUG_ATT_PRINT("left SNR:", snr, 2);
          //      DEBUG_ATT_PRINT("left Max Sig Point:", max_point, 2);
          //  }
        }
        else
        {
            //channel_test_arg->left_ch_SNR_threadshold = 0;
            //channel_test_arg->left_ch_max_sig_point = 0;
        }
    }


    if(channel_test_arg->test_right_ch == TRUE)
    {
        if(channel_test_arg->test_right_ch_SNR == TRUE)
        {
            analyse_sound_data(&p_data_buffer[(LENGTH * 2)], ADC_SAMPLE_RATE, &snr, &max_point);

            if(snr >= cal_threadshold(channel_test_arg->right_ch_SNR_threadshold)
                && channel_test_arg->right_ch_max_sig_point == max_point)
            {
                if(ret_val == TRUE)
                {
                    ret_val = TRUE;
                }
            }
            else
            {
                ret_val = FALSE;
            }

            //channel_test_arg->right_ch_SNR_threadshold = snr;
            //channel_test_arg->right_ch_max_sig_point = max_point;

            //if(g_test_mode == TEST_MODE_CARD)
           // {
           //     DEBUG_ATT_PRINT("right SNR:", snr, 2);
           //     DEBUG_ATT_PRINT("right Max Sig Point:", max_point, 2);
           // }
        }
        else
        {
            //channel_test_arg->right_ch_SNR_threadshold = 0;
            //channel_test_arg->right_ch_max_sig_point = 0;
        }
    }

    return ret_val;
}
