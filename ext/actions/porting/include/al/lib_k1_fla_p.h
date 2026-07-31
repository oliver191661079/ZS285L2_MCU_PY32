/**
* @file lib_k1_fla_p.h  
* @brief Header of FLAC parser
* @author Longfei Shu
* @date 2024-09-25 
* @version 1.0 
* @par Copyright (c): 2014-2024 Actions Technology Co., Ltd.
*/
#ifndef _LIB_K1_FLA_P_H_
#define _LIB_K1_FLA_P_H_

#include "as_audio_codec.h"

/**  streaminfo_t */
typedef struct
{
    unsigned int min_blocksize;
    unsigned int max_blocksize;
    unsigned int min_framesize;
    unsigned int max_framesize;
    unsigned int sample_rate;
    unsigned int channels;
    unsigned int bits_per_sample;
    unsigned int total_time;
    unsigned int total_samples;
    //uint16_t md5sum[8];
    //uint8_t md5sum[16];
} streaminfo_t;

/**  parser_flac_t */
typedef struct
{
    streaminfo_t streaminfo;
    void *inbuf;
} parser_flac_t;

/** 
* Discription of as_parser_ops_flac
* @param[in]   hnd    parser handle
* @param[in]   cmd    parser command
* @param[in]   args    parser argument
* @return      0:succeeded; !=0:failed;
* @code
* asparse_param_t asparse_param;
* storage_io_t storage_io_global;
* asparse_param.storage_io = &storage_io_global;
* void *flacpar_handle = NULL;
* ret = as_parser_ops_flac(&flacpar_handle, AP_CMD_OPEN, &asparse_param);
* asparse_info_t asparse_info;
* ret = as_parser_ops_flac(flacpar_handle, AP_CMD_PARSER_HEADER, &asparse_info);
* asparse_bs_info_t asparse_bs_info;
* ret = as_parser_ops_flac(flacpar_handle, AP_CMD_GET_CHUNK, &asparse_bs_info);
* @endcode
*/
int as_parser_ops_flac(void *hnd, asparse_ex_ops_cmd_t cmd, unsigned int args);

#endif

