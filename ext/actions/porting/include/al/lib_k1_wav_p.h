/**
* @file lib_k1_wav_p.h  
* @brief Header of WAV parser
* @author Longfei Shu
* @date 2024-08-23 
* @version 1.0 
* @par Copyright (c): 2014-2024 Actions Technology Co., Ltd.
*/
#ifndef _LIB_K1_WAV_P_H_
#define _LIB_K1_WAV_P_H_

#include "as_audio_codec.h"

/**  parser_wav_t */
typedef struct
{    
    int format;///<  pcm type: 0x1-LPCM, 0x2-MS, 0x6-ALAW, 0x11-IMA, etc.
    int channels;///<  pcm channels
    int samples_per_frame;///<  sample (pairs) per pcm frame
    int bits_per_sample;///<  bit depth per pcm sample
    int bytes_per_frame;///<  bytes per pcm frame
} parser_pcm_t;

/** 
* Discription of as_parser_ops_wav
* @param[in]   hnd    parser handle
* @param[in]   cmd    parser command
* @param[in]   args    parser argument
* @return      0:succeeded; !=0:failed;
* @code
* asparse_param_t asparse_param;
* storage_io_t storage_io_global;
* asparse_param.storage_io = &storage_io_global;
* void *wavpar_handle = NULL;
* ret = as_parser_ops_wav(&wavpar_handle, AP_CMD_OPEN, &asparse_param);
* asparse_info_t asparse_info;
* ret = as_parser_ops_wav(wavpar_handle, AP_CMD_PARSER_HEADER, &asparse_info);
* asparse_bs_info_t asparse_bs_info;
* ret = as_parser_ops_wav(wavpar_handle, AP_CMD_GET_CHUNK, &asparse_bs_info);
* @endcode
*/
int as_parser_ops_wav(void *hnd, asparse_ex_ops_cmd_t cmd, unsigned int args);

#endif//_LIB_K1_WAV_P_H_

