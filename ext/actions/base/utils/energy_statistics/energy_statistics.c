/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file  energy statistics interface
 */

#include <stdint.h>
#include "energy_statistics.h"

uint32_t energy_statistics(const short *pcm_data, uint32_t samples_cnt)
{
	uint32_t sample_value = 0, i;
	int32_t temp_value;

	for (i = 0; i < samples_cnt; i++)
	{
		if (pcm_data[i] < 0)
			temp_value = 0 - pcm_data[i];
		else
			temp_value = pcm_data[i];
		sample_value += (uint32_t)temp_value;
	}

	return (sample_value / samples_cnt);
}

uint32_t energy_statistics_bitdepth(const char *buf, int num, short bitdepth)
{
	uint32_t sample_value = 0;
	int32_t temp_value;
	uint32_t samples_cnt = num / (bitdepth / 8);
	short pcm_data =0;

	if (bitdepth == 16) {
		return energy_statistics((const short *)buf, samples_cnt);
	} else if (bitdepth == 24) {
		for (int i = 0; i+3 <= num; ){
			int32_t v = ((int32_t)buf[i++])<<8;
			v |= ((int32_t)buf[i++])<<16;
			v |= ((int32_t)buf[i++])<<24;
			pcm_data = v >> 16;
			if (pcm_data < 0)
				temp_value = 0 - pcm_data;
			else
				temp_value = pcm_data;
			sample_value += (uint32_t)temp_value;
		}
	} else {
		for (int i = 0; i+4 <= num; ){
			int32_t v = ((int32_t)buf[i++])<<0;
			v |= ((int32_t)buf[i++])<<8;
			v |= ((int32_t)buf[i++])<<16;
			v |= ((int32_t)buf[i++])<<24;
			pcm_data = v >> 16;
			if (pcm_data < 0)
				temp_value = 0 - pcm_data;
			else
				temp_value = pcm_data;
			sample_value += (uint32_t)temp_value;
		}
	}
	return (sample_value / samples_cnt);
}