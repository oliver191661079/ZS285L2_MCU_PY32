#ifndef __AP_AUTOTEST_CHANNEL_TEST_THD_H
#define __AP_AUTOTEST_CHANNEL_TEST_THD_H

//Table lookup for sin and cos
#define USE_TABLE

//FFT points
#define LENGTH  128

//How mnay binary bits for so many points's FFT
//bL=log2(LENGTH)
#define bL 7

#define IN_TYPE  short int
#define OUT_TYPE long int
#define LEN_TYPE long int


typedef long long __int64 ;

#define PI 3.1415926535897932384626433832795

typedef struct
{
    //SNR of calculating
    int32 SNR;
    int32 MaX_Sig_Point;
    //Max amplitude in time zone
    int32 AmpMax;
    //Min amplitude in time zone
    int32 AmpMin;
    //32bit Max amplitude in Frequency zone
    int32 SigHigh;
    //32bit Min amplitude in Frequency zone
    int32 SigLow;
    //32bit Max Noise in Frequency zone
    int32 NoiseHigh;
    //32bit Min Noise in Frequency zone
    int32 NoiseLow;
}snr_result_t;

extern const short int COS_TABLE[];
extern const short int SIN_TABLE[];

extern void InitBitRev(LEN_TYPE *BitRev);
extern void FftInput(IN_TYPE *pIn, LEN_TYPE *BitRev);
extern void FftExe(IN_TYPE *pIn, OUT_TYPE *pRe, OUT_TYPE *pIm);

extern uint32 thd_test(void *buffer_addr, channel_test_arg_t *channel_test_arg);
extern void load_win_data(void);

#endif

