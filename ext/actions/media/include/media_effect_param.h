/*
 * Copyright (c) 2020, Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __MEDIA_EFFECT_PARAM_H__
#define __MEDIA_EFFECT_PARAM_H__

#include <toolchain.h>
#include <arithmetic.h>

#define ASET_DAE_PEQ_POINT_NUM  (AS_DAE_EQ_NUM_BANDS)
#define AS_DAE_EQ_NUM_BANDS  (20)
#define AS_DAE_PARA_BIN_SIZE (sizeof(((as_dae_para_info_t*)0)->aset_bin))

/* 512 bytes */
typedef struct aset_para_bin {
	char dae_para[AS_DAE_PARA_BIN_SIZE]; /* 512 bytes */
} __packed aset_para_bin_t;


typedef struct eq_band_setting
{
	/*中心频点（或者高通滤波器的截止频率）单位Hz*/
	short cutoff;//0~22000, EQ点的频---[0~22000] HZ,//ASET Point_Info_M
	/*Q值，单位.1，推荐范围是到*/
	short q;//3~300,EQ频点的Q值----[0.3~30.0]转定点//ASET Point_Info_M
	/*增益，单位.1db，荐范围是-240到+120*/
	short gain;  //-120~120, EQ频点的增益---[-12~12] DB //ASET Point_Info_M
	/*滤波器类型：是无效，是常规EQ滤波器，是高通滤波器，其它值为reserve*/
	short type; //EQ频点的类型---[0~5]六种类型//ASET Point_Info_M
} eq_band_t;


typedef struct eq_band_arr
{
	u8_t index;// 1~max
	eq_band_t eq;
}eq_arr_t;

enum
{
	EQ_TYPE_NULL,
	EQ_TYPE_SPEAKER,
	EQ_TYPE_POST,
	EQ_TYPE_SUBWOOFER,
};

typedef struct {
	u32_t magic_data;
	u32_t version;
	u32_t volume_table_level;
} effect_volume_table_info_t;

#define ASQT_DAE_PEQ_POINT_NUM  (AS_DAE_H_EQ_NUM_BANDS)

/* 288 bytes */
typedef struct asqt_para_bin {
	as_hfp_speech_para_bin_t sv_para; /* 60 bytes */
	char cRev1[12];
	char dae_para[AS_DAE_H_PARA_BIN_SIZE]; /* 192 bytes */
	int max_pa_volume;
	int current_volume;
	int mic_gain;
	int adc_gain;
	char cRev2[8];
} __packed asqt_para_bin_t;

typedef struct {
	asqt_para_bin_t *asqt_para_bin;
} __packed asqt_ext_info_t;

#ifdef CONFIG_DSP
#define MAX_VOLUME_TABEL_LEVEL   41

typedef struct {
	short sIndex;
	short sPAVal;
	int sDAVal;
} volume_per_info_t;

typedef struct {
	volume_per_info_t stVolumeInfo[MAX_VOLUME_TABEL_LEVEL];
	u8_t bEnVolumeTabel;
	u8_t bRev[7];
} volume_tabel_t;

int media_effect_get_param_size(void);
#define ASET_PARA_BIN_SIZE           (media_effect_get_param_size())

#if CONFIG_DSP_HFP_VERSION
#define ASQT_PARA_BIN_SIZE           (4 + 0x258 + 8)
#define ASQT_PARA_REAL_SIZE          ASQT_PARA_BIN_SIZE
#else
#define ASQT_PARA_BIN_SIZE           330
#define ASQT_PARA_REAL_SIZE          324
#endif
#else
#define ASET_PARA_BIN_SIZE           (sizeof(aset_para_bin_t))
#define ASQT_PARA_BIN_SIZE           sizeof(asqt_para_bin_t)
#endif

/**
 * @brief Set user effect param.
 *
 * This routine set user effect param. The new param will take effect after
 * next media_player_open.
 *
 * @param stream_type stream type, see audio_stream_type_e
 * @param effect_type effect type, reserved for future
 * @param vaddr address of user effect param
 * @param size size of user effect param
 *
 * @return 0 if succeed; non-zero if fail.
 */
int media_effect_set_user_param(u8_t stream_type, u8_t effect_type, const void *vaddr, int size);

/**
 * @brief Get effect param.
 *
 * This routine get effect param. User effect param will take
 * take precedence over effect file.
 *
 * @param stream_type stream type, see audio_stream_type_e
 * @param effect_type effect type, reserved for future
 * @param effect_size size of the effect param
 *
 * @return address of effect param.
 */
const void *media_effect_get_param(u8_t stream_type, u8_t effect_type, int *effect_size);

/**
 * @brief load effect param.
 *
 * This routine load effect param.
 *
 * @param efx_pattern effect file name
 * @param effect_size size of the effect param
 * @param stream_type stream type, see audio_stream_type_e
 *
 * @return address of effect param.
 */
const void *media_effect_load(const char *efx_pattern, int *expected_size, u8_t stream_type);


#endif /* __MEDIA_EFFECT_PARAM_H__ */
