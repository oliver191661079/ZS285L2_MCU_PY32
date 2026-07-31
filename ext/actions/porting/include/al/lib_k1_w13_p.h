/**
* @file lib_k1_w13_p.h
* @brief Header of W13 parser
* @author snow
* @date 2024-10-15
* @version 1.0
* @par Copyright (c): 2014-2024 Actions Technology Co., Ltd.
*/

#ifndef _LIB_K1_W13_P_H_
#define _LIB_K1_W13_P_H_
#include "as_audio_codec.h"

/*decoder need inital package*/
typedef struct
{  
  void *inbuf; ///<input ringbuf     
  void *cbbuf; ///<seek ringbuf
  int time_offset; ///<break point (ms)
  int file_len; ///<file len of this wma file
} parser_w13_t; 

/**
* Discription of as_parser_ops_w13
* @param[in]   hnd       Codec handle returned by codec
* @param[in]   cmd       Audio parser cmd
* @param[in]   args      Special arguments
* @return      0:succeeded; !=0:failed
*/
int as_parser_ops_w13(void *handle, asparse_ex_ops_cmd_t cmd, unsigned int args);

#endif
