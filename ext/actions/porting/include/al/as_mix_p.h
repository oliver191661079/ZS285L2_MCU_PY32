/*
 * Copyright (c) 2017 Actions Semi Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.

 * Author: jpl<jianpingliao@actions-semi.com>
 *
 * Change log:
 *	2019/12/16: Created by jpl.
 */

#ifndef __AS_AS_MIX_P__
#define __AS_AS_MIX_P__

typedef struct {
	int sample_rate; /* reserverd */
} as_mix_p_init_t;

//typedef struct {
//	/* [OUTPUT] global memory block length array, 3 blocks at maximum */
//	int global_buf_len[3];
//	/* [OUTPUT] share memory block length array, 3 blocks at maximum */
//	int share_buf_len[3];
//	/* [INPUT] extended parameters */
//	void* param; //here is (as_mix_p_init_t *)&as_mix_p_open_t.as_mix_p_init
//} as_mem_info_t;

typedef struct {
	as_mix_p_init_t as_mix_p_init;
	as_mem_info_t as_mem_info;
	/*! [INPUT] global memory block address array */
	void *global_buf_addr[3];
	/*! [INPUT] share memory block address array */
	void *share_buf_addr[3];
} as_mix_p_open_t;


#define MAX_CHANNEL_AS_MIX_P_INOUT    2
#define MAX_CHANNEL_MIX_SRC_IN    1

/*!
 * \brief
 *      mix processing input/output structure
 */
typedef struct {
	/*! [INPUT] input/output pcm array.
	 * For interweaved pcm, set channels as 2, pcm_in_out[0] as pcm buffer address, and pcm_in_out[1] as NULL
	 */
	void *pcm_in_out[MAX_CHANNEL_AS_MIX_P_INOUT];
	 /*! [INPUT] mixed source pcm address, only support 1 path */
	void *mix_src_in[MAX_CHANNEL_MIX_SRC_IN];
	/*! [INPUT] input pcm channels */
	int channels;
	/*! [INPUT] input pcm samples (pairs) */
	int samples;
	/*! [INPUT] input pcm sample bits, 16 or 32 */
	int sample_bits;
	/*! [INPUT] input pcm fraction bits */
	int frac_bits;
} as_mix_p_pcm_t;

/*!
 * \brief
 *      音频编码的输入输出数据结构
 */
typedef struct
{
    /*! 输入参数：pcm数据指针数组，如果需要配置乘左右交织，则请将channels设为2，pcm_in_out[0]设置为pcmbuffer首地址，pcm_in_out[1]设为零*/
    void *pcm_in[2];
	 /*! 输入参数：混音结果输出数据指针数组，32bit，>=samples */
	int *mix_out;
    /*! 输入参数：当前输入包含的声道数 */
    int channels;
    /*! 输入参数：当前输入包含的样本数，只计单个声道 */
    int samples;
    /*! 输入参数：当前输入样本点占用bit数,16 or 32*/
    int sample_bits;
    /*! 输入参数：当前输入小数点的位数，预留*/
    int frac_bits;
	 /*! 输入参数：增益db值，step 0.1db，10 代表1.0db, max 0db */
	int gain_db;
	/*! 输出参数：32bit mix_out 需要左移多少位完成高对齐*/
	int mix_out_head_room;
}as_mix_p_pcm_mono_t;

/*!
 * \brief
 *      mix ops error code
 */
typedef enum {
	/*! unexpected error */
	AS_MIX_P_RET_UNEXPECTED = -3,
	/*! not enough memory */
	AS_MIX_P_RET_OUTOFMEMORY,
	/*! unsupported format */
	AS_MIX_P_RET_UNSUPPORTED,
	/*! no error */
	AS_MIX_P_RET_OK,
	/*! data underflow */
	AS_MIX_P_RET_DATAUNDERFLOW,
} as_mix_p_ret_t;

/*!
 * \brief
 *      mix ops command
 */
typedef enum {
	/* open, with argument structure as_mix_p_open_t */
	AS_MIX_P_CMD_OPEN = 0,
	/* close */
	AS_MIX_P_CMD_CLOSE,
	/** get status */
	AS_MIX_P_CMD_GET_STATUS,
	/** frame process, with argument structure as_mix_p_pcm_t */
	AS_MIX_P_CMD_FRAME_PROCESS,
	/** get memory require size(bytes) */
	AS_MIX_P_CMD_MEM_REQUIRE,
	/** get codec verison */
	AS_MIX_P_CMD_GET_VERSION,
    /*MONO SPEAKER帧处理，输入参数：帧数据信息,as_mix_p_pcm_mono_t*/
    AS_MIX_P_CMD_FRAME_PROCESS_MG,
} as_mix_p_ex_ops_cmd_t;

/**
 * @brief as mix operation
 *
 * This routine provides as mix operation
 *
 * @param hnd handle of as mix
 * @param cmd  operation cmd, type of as_dae_ex_ops_cmd_t
 * 				for AS_MIX_P_CMD_MEM_REQUIRE, args type is as_mem_info_t* in audio_codec.h
 * 				for AS_MIX_P_CMD_OPEN, args type is as_mix_p_open_t*
 * 				for AS_MIX_P_CMD_FRAME_PROCESS, args type is as_mix_p_pcm_t*
 * @param args args of mix parama addr
 *
 * @return type of as_mix_p_ret_t;
 */
int as_mix_p_ops(void *hnd, as_mix_p_ex_ops_cmd_t cmd, unsigned int args);

#endif /* __AS_AS_MIX_P__ */
