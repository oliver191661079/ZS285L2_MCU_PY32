/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file system audio policy
 */

#include <os_common_api.h>
#include <mem_manager.h>
#include <string.h>
#include <audio_system.h>
#include <audio_policy.h>
#if (1 == CONFIG_DSP_HFP_VERSION)
#include <tflv/tflv.h>
#include <sdfs.h>
#endif
#ifdef CONFIG_DVFS
#include <dvfs.h>
#endif
#include "app_common.h"

#include "system_app.h"

#ifndef CONFIG_AUDIO_VOLUME_PA
#error "No volume table as dB unit!"
#endif

#define VOL_THREHOLD 0xFF
#define MAX_PA_LEVEL 0x28

#ifdef CONFIG_MEDIA
static u16_t voice_da_table[MAX_AUDIO_VOL_LEVEL+1] = {
	0,  4860,  6113, 7684, 9657, 12137, 15254, 19172,
	22068, 23262, 24521, 25848, 27247, 28722, 30277, 31916,
	31926,
};

static u8_t voice_pa_table[MAX_AUDIO_VOL_LEVEL+1] = {
	0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28,
	0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28, 0x28,
	0x28,
};

#ifdef CONFIG_MUSIC_EXTERNAL_EFFECT
static u32_t music_da_table[MAX_AUDIO_VOL_LEVEL + 1] = {
	-685, -560, -500, -440,
	-380, -320, -280, -240,
	-200, -160, -120, -100,
	-80, -60, -40, -20,
	0
};
#ifdef VOLUME_ON_EXXTERNAL_EFFECT
static u8_t music_pa_table[MAX_AUDIO_VOL_LEVEL+1] = {
	0x28, 0x28, 0x28, 0x28,
	0x28, 0x28, 0x28, 0x28,
	0x28, 0x28, 0x28, 0x28,
	0x28, 0x28, 0x28, 0x28,
	0x28
};
#else
static u8_t music_pa_table[MAX_AUDIO_VOL_LEVEL+1] = {
	0x00, 0x10, 0x12, 0x15,
	0x17, 0x1a, 0x1d, 0x1f,
	0x20, 0x21, 0x22, 0x23,
	0x24, 0x25, 0x26, 0x27,
	0x28
};
#endif

static u8_t music_dac_gain_table[MAX_AUDIO_VOL_LEVEL+1] = {
    0xbf, 0xbf, 0xbf, 0xbf,
    0xbf, 0xbf, 0xbf, 0xbf,
    0xbf, 0xbf, 0xbf, 0xbf,
    0xbf, 0xbf, 0xbf, 0xbf,
    0xbf
};

#else

#if (MAX_AUDIO_VOL_LEVEL == 32)
//music da effect is not used by DSP.
static u32_t music_da_table[MAX_AUDIO_VOL_LEVEL+1] = {
	0x40000000, 0x40000000, 0x40000000, 0x40000000,
	0x40000000, 0x40000000, 0x40000000, 0x40000000,
	0x40000000, 0x40000000, 0x40000000, 0x40000000,
	0x40000000, 0x40000000, 0x40000000, 0x40000000,
	0x40000000, 0x40000000, 0x40000000, 0x40000000,
	0x40000000, 0x40000000, 0x40000000, 0x40000000,
	0x40000000, 0x40000000, 0x40000000, 0x40000000,
	0x40000000, 0x40000000, 0x40000000, 0x40000000,
	0x40000000
};

static u8_t music_pa_table[MAX_AUDIO_VOL_LEVEL+1] = {
    0x00, 0x04, 0x08, 0x0A, 0x0C, 0x0E, 0x10, 0x11,
    0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
    0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21,
    0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x28,
    0x28
};

static u8_t music_dac_gain_table[MAX_AUDIO_VOL_LEVEL+1] = {
    0xbf, 0xbf, 0xbf, 0xbf,
    0xbf, 0xbf, 0xbf, 0xbf,
    0xbf, 0xbf, 0xbf, 0xbf,
    0xbf, 0xbf, 0xbf, 0xbf,
    0xbf, 0xbf, 0xbf, 0xbf,
    0xbf, 0xbf, 0xbf, 0xbf,
    0xbf, 0xbf, 0xbf, 0xbf,
    0xbf, 0xbf, 0xbf, 0xbf,
    0xbf
};

//Volume tables for another volume range
#else

static u32_t music_da_table[MAX_AUDIO_VOL_LEVEL+1] = {
	0x40000000, 0x40000000, 0x40000000, 0x40000000,
	0x40000000, 0x40000000, 0x40000000, 0x40000000,
	0x40000000, 0x40000000, 0x40000000, 0x40000000,
	0x40000000, 0x40000000, 0x40000000, 0x40000000,
	0x40000000
};

static u8_t music_pa_table[MAX_AUDIO_VOL_LEVEL+1] = {
	0x00, 0x10, 0x12, 0x15,
	0x17, 0x1a, 0x1d, 0x1f,
	0x20, 0x21, 0x22, 0x23,
	0x24, 0x25, 0x26, 0x27,
	0x28
};

static u8_t music_dac_gain_table[MAX_AUDIO_VOL_LEVEL+1] = {
    0xbf, 0xbf, 0xbf, 0xbf,
    0xbf, 0xbf, 0xbf, 0xbf,
    0xbf, 0xbf, 0xbf, 0xbf,
    0xbf, 0xbf, 0xbf, 0xbf,
    0xbf
};
#endif

#ifdef CONFIG_AUDIO_MULTI_CHANNEL_MODE
static s16_t multi_ch_da_table[2][MAX_AUDIO_VOL_LEVEL + 1] = {0,};
#endif

#endif

static struct audio_policy_t system_audio_policy = {
#ifdef CONFIG_AUDIO_OUTPUT_I2S
	.audio_out_channel = AUDIO_CHANNEL_I2STX,
#elif CONFIG_AUDIO_OUTPUT_SPDIF
	.audio_out_channel = AUDIO_CHANNEL_SPDIFTX,
#elif CONFIG_AUDIO_OUTPUT_DAC_AND_I2S
	.audio_out_channel = AUDIO_CHANNEL_DAC | AUDIO_CHANNEL_I2STX,
#elif CONFIG_AUDIO_OUTPUT_DAC_AND_SPDIF
	.audio_out_channel = AUDIO_CHANNEL_DAC | AUDIO_CHANNEL_SPDIFTX,
#elif CONFIG_AUDIO_OUTPUT_DAC_I2S_SPDIF
	.audio_out_channel = AUDIO_CHANNEL_DAC | AUDIO_CHANNEL_I2STX | AUDIO_CHANNEL_SPDIFTX,
#else
	.audio_out_channel = AUDIO_CHANNEL_DAC,
#endif

	.audio_in_linein_gain = 1,	// +6db
	.audio_in_fm_gain = 0,	// 0db
#ifdef CONFIG_BOARD_ATS2831P_EVB
	.audio_in_mic_gain = 180,	// 18db
#else
	.audio_in_mic_gain = 345,	// 34.5db
#endif

#ifdef CONFIG_AEC_TAIL_LENGTH
	.voice_aec_tail_length_16k = CONFIG_AEC_TAIL_LENGTH,
	.voice_aec_tail_length_8k = CONFIG_AEC_TAIL_LENGTH * 2,
#endif

	.tts_fixed_volume = 1,
	.volume_saveto_nvram = 1,

	.audio_out_volume_level = MAX_AUDIO_VOL_LEVEL,
	.music_da_table = music_da_table,
	.music_pa_table = music_pa_table,
	.voice_da_table = voice_da_table,
	.voice_pa_table = voice_pa_table,
	.usound_da_table = music_da_table,
	.usound_pa_table = music_pa_table,
#ifdef CONFIG_AUDIO_MULTI_CHANNEL_MODE
	.multi_ch_da0_table = multi_ch_da_table[0],
	.multi_ch_da1_table = multi_ch_da_table[1],
#endif
};

#endif				/* CONFIG_MEDIA */

#if (1 == CONFIG_DSP_HFP_VERSION)
#define MAX_PA_LEVEL 0x28

typedef struct {
	u16_t pa_val;
	u16_t da_val;
} VOLUME_TABLE_ITEM ;

typedef struct {
	u8_t mic0_a_gain;
	u8_t mic0_d_gain;
	u8_t mic1_a_gain;
	u8_t mic1_d_gain;
	u8_t pa_sel; //speaker channel select [0, 1, 2, 3] 0 –stero; 1-left; 2-right
	u8_t pa_dec_left; // [0-40]
	u8_t pa_dec_right; //[0-40]
	u8_t Reserved; // Reserved to 0
} VOL_CTL;


/*
TODO:
dB decreate to VOLL/VOLR
*/
static u16_t voice_pa_dec_l = 0;
static u16_t voice_pa_dec_r = 0;

void audio_system_volume_update_voice_table(u8_t *data, u8_t num)
{
	u8_t n;
	VOLUME_TABLE_ITEM *item = (VOLUME_TABLE_ITEM *)data;

	memset(voice_da_table, 0, sizeof(voice_da_table));
	memset(voice_pa_table, 0x28, sizeof(voice_pa_table));

	SYS_LOG_INF("num=%d\n", num);
	num = (num < (MAX_AUDIO_VOL_LEVEL+1)) ? num : (MAX_AUDIO_VOL_LEVEL + 1);
	for (n = 0; n < num; n++, item++) {
		voice_da_table[n] = (u16_t)item->da_val;
		voice_pa_table[n] = (u8_t)item->pa_val;
	}
}

void audio_system_volume_update_voice_dec(u8_t left, u8_t right)
{
	SYS_LOG_INF("left=%d, right=%d.", left, right);
	if(left > MAX_PA_LEVEL) {
		voice_pa_dec_l  = MAX_PA_LEVEL;
	} else {
		voice_pa_dec_l = left;
	}
	if(right > MAX_PA_LEVEL) {
		voice_pa_dec_r  = MAX_PA_LEVEL;
	} else {
		voice_pa_dec_r = right;
	}
}

//Mix analog gain and digital gain level to dB value
int audio_policy_change_mic_gain_to_db(u16_t analog_gain, u16_t  digital_gain)
{
	int volume = digital_gain * 30;

	if (analog_gain)
		volume += 15 * (analog_gain - 1) + 300;
	else
		volume += 260;

	return volume;
}

#ifdef CONFIG_VOICE_EFFECT
int audio_policy_set_microphone_gain(u8_t a, u8_t d)
{
	SYS_LOG_INF("a=%d, d=%d.", a, d);
	system_audio_policy.audio_in_mic_gain = audio_policy_change_mic_gain_to_db(a, d);
	SYS_LOG_INF("%d", system_audio_policy.audio_in_mic_gain);
	return 0;
}

static int audio_system_volume_load_voice_gain(void)
{
	int ret;
	char *name = "call_16.efx";
	void *addr;
	u32_t size;
	u8_t *vt_addr;
	u16_t vt_size;

	SYS_LOG_INF("%s", name);
	ret = sd_fmap(name, (void **)&addr, &size);
	if (0 == ret) {
		if (tflv_find_block((u8_t *)addr, (u16_t)size, AF_VOL_TABLE,
				    &vt_addr, &vt_size)) {
			audio_system_volume_update_voice_table(
				vt_addr + TFLV_HEAD_SIZE,
				(vt_size - TFLV_HEAD_SIZE) /
					sizeof(VOLUME_TABLE_ITEM));
		}

		if (tflv_find_block((u8_t *)addr, (u16_t)size, AF_VOL_CONTROL,
				    &vt_addr, &vt_size)) {
			VOL_CTL *vc = (VOL_CTL *)(vt_addr + TFLV_HEAD_SIZE);
			if (vc->pa_sel == 2) { //right
				audio_system_volume_update_voice_dec( 0, vc->pa_dec_right);
			} else if (vc->pa_sel == 1) { //left
				audio_system_volume_update_voice_dec(vc->pa_dec_left, 0);
			} else {
				audio_system_volume_update_voice_dec(vc->pa_dec_left, vc->pa_dec_right);
			}

			audio_policy_set_microphone_gain(vc->mic0_a_gain,  vc->mic0_d_gain);
		}
	} else {
		SYS_LOG_WRN("Failed to open %s.", name);
	}

	return 0;
}
#endif
#endif


/*
DAC volume L/R channel control
Range：0x00 ~ 0xff
Step 0.375db (3/8)
0xff +24.000dB
0xfe +23.625dB
0xfd +23.250dB
...
0xbf 0.000dB
0xbe -0.375dB
...
0x01 -71.250dB
0x00 mute
*/

/*
 * PA
音量等级(PA)  增益值(db)
step: 1
40            0.0
39            -1.0
38            -2.0
37            -3.0
36            -4.0
35            -5.0
34            -6.0
33            -7.0
32            -8.0
31            -9.0
30            -10.0
29            -11.0
28            -12.0
27            -13.0
step: 1.5
26            -14.5
25            -16.0
24            -17.5
23            -19.0
22            -20.5
21            -22.0
20            -23.5
19            -25.0
18            -26.5
17            -28.0
16            -29.5
15            -31.0
14            -32.5
step: 2.5
13            -35.0
step: 2
12	      -37.0
11	      -39.0
10	      -41.0
9	      -43.0
8	      -45.0
7	      -47.0
6	      -49.0
5	      -51.0
4	      -53.0
3             -55.0
2	      -57.0
1	      -59.0
step: 38
0	      -97.0
Gain = PA>40?0:(PA>26?-(40-PA)*1:-(40-26)*1())
*/

static u8_t pa_to_dac(u8_t pa)
{
	u32_t mdb;

	//pa to -mdB
	if(pa >= 40) {
		mdb = 0;
	} else if (pa >= 27) {
		mdb = 1000* (40 - pa);
	} else if (pa >= 14) {
		mdb = 1000* (40 - 27) + 1500*(27-pa);
	} else if (pa == 13) {
		mdb = 35000;
	} else if (pa >= 1) {
		mdb = 35000 + 2000*(13-pa);
	}else{
		mdb = 97000;
	}

	//-mdb to dac
	return (0xBF < mdb/375)? 0: (0xBF - mdb/375);
}

u32_t audio_system_volume_get_music_da(u8_t vol)
{
	if(vol > MAX_AUDIO_VOL_LEVEL ){
		vol = MAX_AUDIO_VOL_LEVEL;
	}
	return music_da_table[vol];
}

u32_t audio_system_volume_get_music_pa(u8_t vol)
{
	if(vol > MAX_AUDIO_VOL_LEVEL ){
		vol = MAX_AUDIO_VOL_LEVEL;
	}

	return (music_pa_table[vol] << 16) | (music_dac_gain_table[vol] << 8) | (music_dac_gain_table[vol]);
}

void audio_system_volume_update_music_pa(u8_t pa, u8_t vol)
{
	if(vol > MAX_AUDIO_VOL_LEVEL ){
		return;
	}

	if(vol >= VOL_THREHOLD) {
		music_dac_gain_table[vol]=pa_to_dac(pa);
		music_pa_table[vol]=0x28;
	}else{
		music_pa_table[vol]=pa;
		music_dac_gain_table[vol]=0xbf;
	}
}

void audio_system_volume_update_music_da(u32_t da, u8_t vol)
{
	if(vol > MAX_AUDIO_VOL_LEVEL ){
		return;
	}

	music_da_table[vol]=da;
}

void audio_system_volume_set_music_pada(u32_t v)
{
	for (int i = 0; i <= MAX_AUDIO_VOL_LEVEL; i++) {
		music_da_table[i]=v;
		music_pa_table[i]=0x28;
		music_dac_gain_table[i]=0xbf;
	}
}
void audio_system_volume_reset_music_pada(void)
{
	for (int i = 0; i <= MAX_AUDIO_VOL_LEVEL; i++) {
		music_da_table[i]=0x40000000;
		music_pa_table[i]=0x28;
		music_dac_gain_table[i]=0xbf;
#ifdef CONFIG_AUDIO_MULTI_CHANNEL_MODE
		multi_ch_da_table[0][i] = 0;
		multi_ch_da_table[1][i] = 0;
#endif
	}
}

void audio_system_volume_update_music_da_dot2ch(const s16_t* da0, const s16_t* da1, u8_t num)
{
	if (num > MAX_AUDIO_VOL_LEVEL + 1) {
		return;
	}
	for (int i=0; i < num; i++){
		multi_ch_da_table[0][i] = da0[i];
		multi_ch_da_table[1][i] = da1[i];
	}
}

#ifdef CONFIG_MUSIC_EFFECT
#ifndef CONFIG_MUSIC_EXTERNAL_EFFECT
static int audio_system_volume_load_music_gain(void)
{
	int ret;
	struct sd_file *f;
	char *name;
	u8_t mode;

	mode = audio_system_get_multi_channel_mode();
	SYS_LOG_INF("mode=%d\n", mode);

	if (mode != MULTI_CH_MODE_11) {
		name = "music_da.bin";
		f = sd_fopen(name);
		if (f != NULL) {
			SYS_LOG_INF("load %s\n", name);
			ret = sd_fread(f, music_da_table,
						sizeof(music_da_table));
			if (ret != sizeof(music_da_table)) {
				SYS_LOG_WRN("read %s ret=%d\n", name, ret);
			}
			sd_fclose(f);
		} else {
			SYS_LOG_WRN("Failed to open %s.\n", name);
		}

		name = "music_pa.bin";
		f = sd_fopen(name);
		if (f != NULL) {
			u8_t table[MAX_AUDIO_VOL_LEVEL + 1];
			SYS_LOG_INF("load %s\n", name);
			ret = sd_fread(f, table, (MAX_AUDIO_VOL_LEVEL + 1));
			if (ret != (MAX_AUDIO_VOL_LEVEL + 1)) {
				SYS_LOG_WRN("read %s ret=%d\n", name, ret);
			}
			for (int i = 0; i < MAX_AUDIO_VOL_LEVEL + 1; i++) {
				audio_system_volume_update_music_pa(table[i],
									i);
			}
			sd_fclose(f);
		} else {
			SYS_LOG_WRN("Failed to open %s.\n", name);
		}
	}


#ifdef CONFIG_AUDIO_MULTI_CHANNEL_MODE
	if ((mode == MULTI_CH_MODE_2_1) || (mode == MULTI_CH_MODE_2_2) ||
	    (mode == MULTI_CH_MODE_2_11)) {
		name = "musiceda.bin";
		f = sd_fopen(name);
		if (f != NULL) {
			SYS_LOG_INF("load %s\n", name);
			ret = sd_fread(f, multi_ch_da_table,
				       sizeof(multi_ch_da_table));
			if (ret != sizeof(multi_ch_da_table)) {
				SYS_LOG_WRN("read %s ret=%d\n", name, ret);
			}
			sd_fclose(f);
		} else {
			SYS_LOG_WRN("Failed to open %s.\n", name);
		}
	}

	if (mode == MULTI_CH_MODE_11) {
		name = "music1_1.bin";
		f = sd_fopen(name);
		if (f != NULL) {
			u8_t buf[8];
			SYS_LOG_INF("load %s\n", name);
			for (int i = 0; i < MAX_AUDIO_VOL_LEVEL + 1; i++) {
				ret = sd_fread(f, buf, 8);
				if (ret == 8) {
					//Take low byte of PA channel 0 as music pa.
					//Warning: support only one channel PA.
					audio_system_volume_update_music_pa(
						buf[0], i);
					multi_ch_da_table[0][i] =
						buf[4] | (buf[5] << 8);
					multi_ch_da_table[1][i] =
						buf[6] | (buf[7] << 8);
				} else {
					SYS_LOG_WRN(
						"Can not load music1_1.bin\n");
				}
			}
			sd_fclose(f);
		} else {
			SYS_LOG_WRN("Can not open musice1_1.bin\n");
		}
	}
#endif

	return 0;
}
#endif
#endif

void audio_system_volume_dump(void)
{
	print_buffer_lazy("music da", music_da_table, sizeof(music_da_table));
	print_buffer_lazy("pa", music_pa_table, sizeof(music_pa_table));
	print_buffer_lazy("dac gain", music_dac_gain_table, sizeof(music_dac_gain_table));
#ifdef CONFIG_AUDIO_MULTI_CHANNEL_MODE
	print_buffer_lazy("multi-ch da", multi_ch_da_table, sizeof(multi_ch_da_table));
#endif
}

void system_audio_policy_init(void)
{
#ifdef CONFIG_VOICE_EFFECT
#if (1 == CONFIG_DSP_HFP_VERSION)
	audio_system_volume_load_voice_gain();
#endif
#endif

#ifdef CONFIG_MUSIC_EFFECT
#ifndef CONFIG_MUSIC_EXTERNAL_EFFECT
	audio_system_volume_load_music_gain();
#endif
#endif

#ifdef CONFIG_MEDIA
	audio_policy_register(&system_audio_policy);
	audio_system_set_output_sample_rate(CONFIG_AUDIO_OUTPUT_SAMPLE_RATE);

#ifdef CONFIG_OUTPUT_RESAMPLE
	if ((system_audio_policy.audio_out_channel & AUDIO_CHANNEL_I2STX) ==
	    AUDIO_CHANNEL_I2STX
	    || (system_audio_policy.
		audio_out_channel & AUDIO_CHANNEL_SPDIFTX) ==
	    AUDIO_CHANNEL_SPDIFTX) {
#ifdef CONFIG_DVFS_DYNAMIC_LEVEL
		dvfs_set_level(DVFS_LEVEL_HIGH_PERFORMANCE, "media");
		SYS_LOG_INF("dvfs level %d\n", DVFS_LEVEL_HIGH_PERFORMANCE);
#endif
	}
#endif
#endif

}
