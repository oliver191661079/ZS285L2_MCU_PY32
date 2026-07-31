/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __TOOL_APP_INNER_H__
#define __TOOL_APP_INNER_H__

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <zephyr.h>
#include <misc/util.h>
#include <misc/byteorder.h>
#include <acts_ringbuf.h>
#include <mem_manager.h>
#include <drivers/stub/stub_command.h>
#include <stub_hal.h>
#include <linker/section_tags.h>
#include <media_effect_param.h>
#include <app_defines.h>
#include <app_common.h>
#include <app_manager.h>
#include "desktop_manager.h"

#include "tool_app.h"
#ifndef SYS_LOG_DOMAIN
#define SYS_LOG_DOMAIN "tool"
#endif
#ifndef SYS_LOG_LEVEL
#define SYS_LOG_LEVEL SYS_LOG_LEVEL_DEBUG
#endif
#include <logging/sys_log.h>

typedef struct {
	struct stub_device *dev_stub;
#if TOOL_INIT_SYNC
	os_sem init_sem;
#endif
	char *stack;
	u16_t quit:1;
	u16_t quited:1;
	u16_t type:8;

	u16_t dev_type:3;
} act_tool_data_t;

typedef enum {
	sNotReady = 0,		/* not ready or after user stop */
	sReady,			/* parameter filled, or configuration file selected */
	sRunning,		/* debugging and running normally */
	sUserStop,		/* user stop state */
	sUserUpdate,		/* user update the parameter */
	sUserStart,		/* user testing state */
	sUserSetParam		/* download the parameter to the board */
} PC_curStatus_e;

extern act_tool_data_t g_tool_data;

static inline struct stub_device *tool_stub_dev_get(void)
{
	return g_tool_data.dev_stub;
}

static inline int tool_is_quitting(void)
{
	return g_tool_data.quit;
}

void tool_aset_loop(void);
void tool_asqt_loop(void);
void tool_dump_loop(void);
void tool_ectt_loop(void);
void tool_att_loop(void);
void tool_rett_loop(void);
void tool_common_dae_loop(void);

/*******************************************************************************
 * ASQT specific structure
 ******************************************************************************/
typedef struct {
	uint32_t eq_point_nums;	/* maximum PEQ point num */
	/* PEQ version. 0: speaker and mic share one view; 1: speaker and mic use different views */
	uint32_t eq_version;
	uint8_t fw_version[8];	/* case name, with line ending "0" */
	uint8_t sample_rate;	/* 0: 8kHz, 1: 16kHz */
	uint8_t reserve[15];	/* reserved */
} asqt_interface_config_t;

typedef struct {
#if (1 == CONFIG_DSP_HFP_VERSION)
	u8_t speaker_vol; // [0, 1, 2, … 31]
	u8_t reserved1[3]; //reserved to 0
	u8_t mic0_a_gain;
	u8_t mic0_d_gain;
	u8_t mic1_a_gain;
	u8_t mic1_d_gain;
	u8_t pa_sel; //speaker channel select [0, 1, 2, 3] 0 –stero; 1-left; 2-right
	u8_t pa_dec_left; // [0-40]
	u8_t pa_dec_right; //[0-40]
	u8_t reserved2; // reserved to 0
	u32_t volume_levels; // [17, 32, …]
	struct {
		u16_t pa_val;
		u16_t da_val;
	}vol_table[MAX_AUDIO_VOL_LEVEL + 1];
#else
	short sMaxVolume;
	short sAnalogGain;
	short sDigitalGain;
#endif
} asqt_stControlParam;

typedef struct {
	int16_t sMaxVolume;
	int16_t sAnalogGain;
	int16_t sDigitalGain;
	int16_t sRev[28];
	int16_t sDspDataLen;
} asqt_stControl_BIN;

/* maximum data streams to upload, only 6 at present */
#define ASQT_MAX_OPT_COUNT  10

typedef struct {
	uint8_t bSelArray[ASQT_MAX_OPT_COUNT];
} asqt_ST_ModuleSel;

/*******************************************************************************
 * ASET specific structure
 ******************************************************************************/
#define EQMAXPOINTCOUNT_ALL      32

#define MAX_VOLUME_TABEL_LEVEL   41

typedef enum {
	UNAUX_MODE = 0,
	AUX_MODE = 1,
} aux_mode_e;

//aset related structures
typedef struct {
	u8_t aux_mode;		//aux_mode_e
	u8_t reserved[7];
} application_properties_t;

typedef struct {
	u8_t state;		//0: not start 1:start
	u8_t upload_data;	//0: not upload 1:upload
	u8_t volume_changed;	//0:not changed 1:changed
	u8_t download_data;	//0:not download 1:download

	u8_t upload_case_info;	//0:not upload 1:upload
	u8_t main_switch_changed;	//0:not changed 1:changed
	u8_t update_audiopp;	//0:not update 1:update
	u8_t dae_changed;	//0:not update 1:update
} common_dae_status_t;

typedef struct {
	u8_t state;		//0: not start 1:start
	u8_t upload_data;	//0: not upload 1:upload
	u8_t volume_changed;	//0:not changed 1:changed
	u8_t download_data;	//0:not download 1:download

	u8_t upload_case_info;	//0:not upload 1:upload
	u8_t main_switch_changed;	//0:not changed 1:changed
	u8_t update_audiopp;	//0:not update 1:update
	u8_t dae_changed;	//0:not update 1:update

	u8_t upload_file;

	u8_t reserved1[23];
} aset_status_t;

typedef struct {
	s8_t bEQ_v_1_0;
	s8_t bVBass_v_1_0;
	s8_t bTE_v_1_0;
	s8_t bSurround_v_1_0;

	s8_t bLimiter_v_1_0;
	s8_t bMDRC_v_1_0;
	s8_t bSRC_v_1_0;
	s8_t bSEE_v_1_0;

	s8_t bSEW_v_1_0;
	s8_t bSD_v_1_0;
	s8_t bEQ_v_1_1;
	s8_t bMS_v_1_0;

	s8_t bVBass_v_1_1;
	s8_t bTE_v_1_1;

	s8_t bEQ_v_1_2;
	s8_t bMDRC_v_1_1;
	s8_t bComPressor_v_1_0;
	s8_t bMDRC_v_2_0;
	s8_t bDEQ_v_1_0;
	s8_t bNR_v_1_0;
	s8_t bUD_v_1_0;
	s8_t bEQ_v_1_3;
	s8_t bSW_v_1_0;
	s8_t bSurround_v_2_0;
	s8_t bVolumeTable_v_1_0;

	s8_t bSurround_v_3_0;
	s8_t bMDRC_v_3_0;
	s8_t bComPressor_v_2_0;

	s8_t bEQ_v_1_4;
	s8_t bSW_v_2_0;
	s8_t bVolumeTable_v_2_0;
	s8_t bUD_v_2_0;

	s8_t bEQ_v_1_5;
	s8_t bSW_v_3_0; //multi-channel audio effect
	s8_t bVolumeTable_v_3_0;
	s8_t bCPS_v_1_0; //compensation filter
	s8_t bMDRC_v_3_1;
	s8_t bWF_v_1_0; //flow control module
	s8_t bDEQ_v_2_0;
	s8_t bEQ_v_1_6;
	s8_t bVBass_v_2_0;

	s8_t bMDRC_v_3_1_2;
	s8_t bVBass_v_3_0;
	s8_t bComPressor_v_3_0;
	s8_t bVBass_v_4_0;
	s8_t bComPressor_v_4_0;
	s8_t bMDRC_v_4_0;
	s8_t bDEQ_v_3_0;
	s8_t bVE_v_1_0;//Voice Enhancement
	s8_t bSW_v_4_0;//multi-channel audio effect
	s8_t bVBass_v_5_0;
	s8_t bMDRC_v_4_1;
	s8_t bMonoEh_v_1_0;

	s8_t szRev[83];

	s8_t szVerInfo[8];	//Case's name and version. Shall be all UP characters.
} aset_interface_config_t;

typedef struct {
	s8_t peq_point_num;
	s8_t download_data_over;
	s8_t aux_mode;		//1-aux
	s8_t b_Max_Volumel;
	u8_t bMultiChannelMode; //enum MULTICH_MODE
	u8_t reserved[27];
	aset_interface_config_t stInterface;
} aset_case_info_t;

#define MAX_VOLUME_TABEL_LEVEL_DOT2CH   100
typedef struct {
     s16_t vol[MAX_VOLUME_TABEL_LEVEL_DOT2CH];
     s16_t da0[MAX_VOLUME_TABEL_LEVEL_DOT2CH];
     s16_t da1[MAX_VOLUME_TABEL_LEVEL_DOT2CH];
}volume_tabel_dot2ch_t;

typedef struct {
	u16_t pa0;
	u16_t pa1;
	u16_t da0;
	u16_t da1;
}PADA_0_11,PADA_TABLE_0_11[MAX_AUDIO_VOL_LEVEL+1];

typedef struct
{
	u8_t enable;
	u8_t reserved[3];
	PADA_TABLE_0_11 table;
}VOLUME_TABLE_011;

enum AUX_TYPE {
	eUnAux = 0,
	eAux,
};

enum FILE_TYPE
{
	eExf_File = 0,
	eMusic_File = 1,
	eNo_File = 0xff
};

enum MUSIC_FILE_TYPE {
	eMusic_Pa = 0,
	eMusic_Da,
	eMusic_Eda,
	eMusic_1_1
};

typedef struct _ASET_FILES_
{
    enum FILE_TYPE t;
    union {
        u8_t aux;
        enum MUSIC_FILE_TYPE mt;
        u8_t reserved;
    };
    char* name;
}ASET_FILES;

#endif				/* __TOOL_APP_INNER_H__ */
