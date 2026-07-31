/*
 * Copyright (c) 2020, Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define SYS_LOG_DOMAIN "media"
#include <stdio.h>
#include <string.h>
#include <sdfs.h>
#include <audio_system.h>
#include <media_effect_param.h>
#include <logging/sys_log.h>

#ifdef CONFIG_ACT_EVENT
#include <logging/log_core.h>
LOG_MODULE_DECLARE(media_event,CONFIG_ACT_EVENT_APP_COMPILE_LEVEL);
#endif

#if CONFIG_DSP_HFP_VERSION
#include <tflv/tflv.h>
#endif

#define BTCALL_EFX_PATTERN			"call_%d.efx"
#define LINEIN_EFX_PATTERN			"aux_%d.efx"
#define MUSIC_DRC_EFX_PATTERN		"musicdrc.efx"
#define MUSIC_NO_DRC_EFX_PATTERN	"music_%d.efx"

static const void *btcall_effect_user_addr = NULL;
static const void *linein_effect_user_addr = NULL;
static const void *music_effect_user_addr = NULL;
static u16_t music_effect_user_len = 0;

int media_effect_get_param_size(void)
{
#ifdef CONFIG_MUSIC_EXTERNAL_EFFECT
	return 2048;
#else
	return 1024;
#endif
}

int media_effect_set_user_param(uint8_t stream_type, uint8_t effect_type, const void *vaddr, int size)
{
	const void **user_addr = NULL;

	switch (stream_type) {
	case AUDIO_STREAM_VOICE:
		user_addr = &btcall_effect_user_addr;
		break;
	case AUDIO_STREAM_LINEIN:
		user_addr = &linein_effect_user_addr;
		break;
	default:
		user_addr = &music_effect_user_addr;
		music_effect_user_len = size;
		break;
	}

	if (!user_addr)
		return -EINVAL;

	*user_addr = vaddr;

	return 0;
}

static const void *_media_effect_load(const char *efx_pattern, int *expected_size, uint8_t stream_type)
{
	char efx_name[16];
	void *vaddr = NULL;
	int size = 0;

	if (strchr(efx_pattern, '%')) {
		int volume = audio_system_get_stream_volume(stream_type);
        if (volume >= 32)
            volume = 1;
		for (; volume < 32; volume++) {
			snprintf(efx_name, sizeof(efx_name), efx_pattern, volume);
			if (!sd_fmap(efx_name, &vaddr, &size))
				break;
		}
	} else {
		strncpy(efx_name, efx_pattern, sizeof(efx_name));
		sd_fmap(efx_name, &vaddr, &size);
	}

	SYS_LOG_INF("%s", efx_name);

	if (!vaddr || !size) {
		SYS_LOG_ERR("not found");
		SYS_EVENT_ERR(EVENT_PLAYER_ERROR_TYPE, 'e', __LINE__);
		return NULL;
	}
#if CONFIG_DSP_HFP_VERSION
	if (stream_type == AUDIO_STREAM_VOICE) {
		*expected_size = tflv_get_hfp_size(vaddr, (u16_t)size);
		vaddr = tflv_get_hfp_addr(vaddr, (u16_t)size);
		return vaddr;
	}
#endif
	if (*expected_size && (*expected_size != size)) {
		SYS_LOG_ERR("size incorrect %d!=%d", *expected_size, size);
		SYS_EVENT_ERR(EVENT_PLAYER_ERROR_TYPE, 'e', __LINE__);
		return NULL;
	}
	*expected_size = size;

	return vaddr;
}

const void *media_effect_get_param(uint8_t stream_type, uint8_t effect_type, int *effect_size)
{
	const void *user_addr = NULL;
	const char *efx_pattern = NULL;
	int expected_size = 0;

	switch (stream_type) {
	// case AUDIO_STREAM_TTS: /* not use effect file */
	// 	return NULL;
	case AUDIO_STREAM_VOICE:
		user_addr = btcall_effect_user_addr;
		efx_pattern = BTCALL_EFX_PATTERN;
		expected_size = ASQT_PARA_BIN_SIZE;
		break;
	case AUDIO_STREAM_LINEIN:
		user_addr = linein_effect_user_addr;
		efx_pattern = LINEIN_EFX_PATTERN;
		expected_size = ASET_PARA_BIN_SIZE;
		break;
	case AUDIO_STREAM_TWS:
		user_addr = music_effect_user_addr;
		efx_pattern = MUSIC_NO_DRC_EFX_PATTERN;
		if(music_effect_user_len > 0) {
			expected_size = music_effect_user_len ;
		} else {
			expected_size = ASET_PARA_BIN_SIZE;
		}
		break;
	default:
		user_addr = music_effect_user_addr;
	#ifdef CONFIG_MUSIC_DRC_EFFECT
		efx_pattern = MUSIC_DRC_EFX_PATTERN;
	#else
		efx_pattern = MUSIC_NO_DRC_EFX_PATTERN;
	#endif
		if(music_effect_user_len > 0) {
			expected_size = music_effect_user_len;
		} else {
			expected_size = ASET_PARA_BIN_SIZE;
		}
		break;
	}

	if (NULL == user_addr) {
		user_addr = _media_effect_load(efx_pattern, &expected_size, stream_type);
	}

	if (user_addr && effect_size) {
		*effect_size = expected_size;
	}

	SYS_LOG_INF("%d %p %d", stream_type, user_addr, *effect_size);
	//print_buffer_lazy("effect", user_addr, *effect_size);
	return user_addr;
}
