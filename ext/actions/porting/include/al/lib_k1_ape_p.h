/**
* @file lib_k1_ape_p.h
* @brief Header of APE parser
* @author Qiakai Cai
* @date 2024-10-15
* @version 1.0
* @par Copyright (c): 2014-2024 Actions Technology Co., Ltd.
*/
#ifndef _LIB_K1_APE_P_H_
#define _LIB_K1_APE_P_H_

#include "as_audio_codec.h"

/**  dec_info_t */
typedef struct
{
    /* Derived fields */
    unsigned int junklength;
    unsigned int firstframe;
    unsigned int totalsamples;
    /* Info from Descriptor Block */
    char magic[4];
    short fileversion;
    short padding1;
    unsigned int descriptorlength;
    unsigned int headerlength;
    unsigned int seektablelength;
    unsigned int wavheaderlength;
    unsigned int audiodatalength;
    unsigned int audiodatalength_high;
    unsigned int wavtaillength;
    unsigned char md5[16];
    /* Info from Header Block */
    unsigned short compressiontype;
    unsigned short formatflags;
    unsigned int blocksperframe;
    unsigned int finalframeblocks;
    unsigned int totalframes;
    unsigned short bps;
    unsigned short channels;
    unsigned int samplerate;
    unsigned int startframeoffset;
    unsigned int ape_seek_ms_to;	
} dec_info_t;

/**  parser_ape_t */
typedef struct
{
    dec_info_t dec_info;
    void* cbbuf; ///< ringbuf
} parser_ape_t;

/**
* Discription of as_parser_ops_ape
* @param[in]   hnd       Codec handle returned by codec
* @param[in]   cmd       Audio parser cmd
* @param[in]   args      Special arguments
* @return      0:succeeded; !=0:failed
*/
int as_parser_ops_ape(void *hnd, asparse_ex_ops_cmd_t cmd, unsigned int args);

#endif
