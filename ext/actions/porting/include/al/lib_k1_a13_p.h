/**
* @file lib_k1_a13_p.h
* @brief Header of A13 parser
* @author Qiakai Cai
* @date 2024-9-5
* @version 1.0
* @par Copyright (c): 2014-2024 Actions Technology Co., Ltd.
*/
#ifndef _LIB_K1_A13_P_H_
#define _LIB_K1_A13_P_H_

#include "as_audio_codec.h"

/**  dec_info_t */
typedef struct
{
	int channels;	
	int object_type;
	int sample_rate;
	int ble_flag;
	int format_type;	
	int stuff_frames;
	int chunk_size;		
} dec_info_t;

/**  parser_a13_t */
typedef struct
{
	dec_info_t dec_info;
	void *inbuf;
} parser_a13_t;

/**
* Discription of as_parser_ops_a13
* @param[in]   hnd       Codec handle returned by codec
* @param[in]   cmd       Audio parser cmd
* @param[in]   args      Special arguments
* @return      0:succeeded; !=0:failed
*/
int as_parser_ops_a13(void *hnd, asparse_ex_ops_cmd_t cmd, unsigned int args);

#endif

