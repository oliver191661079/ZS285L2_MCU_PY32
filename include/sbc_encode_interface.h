/********************************************************************************
 *        Copyright(c) 2015-2016 Actions (Zhuhai) Technology Co., Limited,
 *                            All Rights Reserved.
 *
 * 描述：sbc encode api,结构体定义等。
 *
 * 作者：kehaitao
 ********************************************************************************/
#ifndef _SBC_ENCODE_INTERFACE_H            //防止重定义
#define _SBC_ENCODE_INTERFACE_H

typedef struct
{
	/** Sampling rate in hz */
    int sample_rate;
   /** Number of channels */
    int channels;
    /** Bit rate */
    int bitrate;
    /**
      * Encoding library format,
      * a loaded code library may contain multiple formats
      * such as: IMADPCM
      */
    int audio_format;
    /**
      * Once to obtain the audio encoded data playback time
      * 0 that does not limit, in ms
      */
    int chunk_time;

	int input_buffer;
    int input_len;
    int output_buffer;
    int output_len;
	int output_data_size;
} sbc_encode_param_t;

typedef struct
{
	unsigned long flags;

	uint8_t frequency;
	uint8_t blocks;
	uint8_t subbands;
	uint8_t mode;
	uint8_t allocation;
	uint8_t bitpool;
	uint8_t endian;

	void *priv;
	void *priv_alloc_base;
} sbc_obj_t;


/*****************************************************
description Init encoder parametres and malloc internal memory
param[in]   sbc  数据结构指针
param[in]   flags  编码参数标志，未使用，请填零。
retval       0：正常，1: 异常
******************************************************/
extern int sbc_init(void *p_sbc, unsigned long flags, uint8_t *sbc_encode_buffer);


/******************************************************
description Release internal malloc memory
param[in]   sbc  编码数据结构指针
retval      无
******************************************************/
extern void sbc_finish(void *p_sbc);


/******************************************************
description Encodes ONE input block into ONE output block
param[in]   sbc  数据结构指针
param[in]   input  待编码pcm首地址
param[in]   input_len  待编码数据量
param[in]   bit_depth  每个样本点的位数，16或32
param[in]   gain  增益系数，8.24格式，1<<24意味着不放大也不缩小。
param[in]   output  编码数据区首地址
param[in]   output_len  编码数据区大小
param[out]  written 编码后数据量
retval       已编码的pcm数据量，0：异常；非0：对于每次一个block编码，返回值应该跟input_len一致。
******************************************************/
extern int sbc_encode_entry(void *p_sbc,void *param,ssize_t *written);

#endif

