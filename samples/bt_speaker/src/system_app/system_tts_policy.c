/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file system tts policy
 */

#include <os_common_api.h>
#include <mem_manager.h>
#include <msg_manager.h>
#include "app_ui.h"
#include <string.h>
#include <tts_manager.h>
#include <audio_system.h>

#include "system_app.h"

const tts_ui_event_map_t tts_ui_event_map[] = {

	{UI_EVENT_POWER_ON, UNBLOCK_UNINTERRUPTABLE, "poweron.mp3",},
	{UI_EVENT_POWER_OFF, UNBLOCK_UNINTERRUPTABLE|TTS_CANNOT_BE_FILTERED, "poweroff.mp3",},
	{UI_EVENT_STANDBY, 0, "standby.mp3",},
	{UI_EVENT_WAKE_UP, 0, "wakeup.mp3",},
	{UI_EVENT_LOW_POWER, 0, "batlow.mp3",},
	{UI_EVENT_NO_POWER, 0, "battlo.mp3",},

	{UI_EVENT_ENTER_PAIRING, 0, "bt_pair.mp3",},
	{UI_EVENT_CLEAR_PAIRED_LIST, 0, "clrpl.mp3",},
	{UI_EVENT_WAIT_CONNECTION, 0, "btwpr.mp3",},
	{UI_EVENT_CONNECT_SUCCESS, 0, "bt_cnt.mp3",},
	{UI_EVENT_SECOND_DEVICE_CONNECT_SUCCESS, 0, "bt_cnt.mp3",},
	{UI_EVENT_BT_DISCONNECT, 0, "bt_dcnt.mp3",},

	{UI_EVENT_TWS_START_TEAM, 0, "tws_pair.mp3",},
	{UI_EVENT_TWS_STOP_TEAM, 0, "tws_unpr.mp3",},
	{UI_EVENT_TWS_TEAM_SUCCESS, 0, "tws_cnt.mp3",},
	{UI_EVENT_TWS_DISCONNECTED, 0, "tws_dcnt.mp3",},

#if (48 == CONFIG_AUDIO_OUTPUT_SAMPLE_RATE)
	{UI_EVENT_MAX_VOLUME, TTS_MIX_MODE_WITH_TRACK|TTS_CANNOT_BE_FILTERED, "vol_max.pcm",},
#else
	{UI_EVENT_MAX_VOLUME, 0, "vol_max.mp3",},
#endif

#ifdef CONFIG_CONTROL_TTS_ENABLE
	{UI_EVENT_PLAY_START, 0, "play.mp3",},
	{UI_EVENT_PLAY_PAUSE, 0, "pause.mp3",},
	{UI_EVENT_PLAY_PREVIOUS, 0, "prev.mp3",},
	{UI_EVENT_PLAY_NEXT, 0, "next.mp3",},
#endif

	{UI_EVENT_BT_INCOMING, 0, "incoming.mp3",},

	{UI_EVENT_START_SIRI, 0, "tone.mp3",},
	{UI_EVENT_BT_CALLRING, 0, "callring.mp3"},
	{UI_EVENT_MIC_MUTE, 0, "mute.mp3",},
	{UI_EVENT_MIC_MUTE_OFF, 0, "unmute.mp3",},

	{UI_EVENT_FACTORY_DEFAULT, 0, "factory.mp3",},

	{UI_EVENT_ENTER_BTMUSIC, 0, "bt_music.mp3",},
	{UI_EVENT_ENTER_LINEIN, 0, "linein.mp3",},
	{UI_EVENT_ENTER_USOUND, 0, "usound.mp3",},
	{UI_EVENT_ENTER_SPDIF_IN, 0, "spdifin.mp3"},
	{UI_EVENT_ENTER_I2SRX_IN, 0, "i2sin.mp3"},
	{UI_EVENT_ENTER_SDMPLAYER, 0, "sdplay.mp3",},
	{UI_EVENT_ENTER_UMPLAYER, 0, "usbdisk.mp3",},
	{UI_EVENT_ENTER_NORMPLAYER, 0, "norplay.mp3",},
	{UI_EVENT_ENTER_SDPLAYBACK, 0, "sdplaybk.mp3",},
	{UI_EVENT_ENTER_UPLAYBACK, 0, "uplaybk.mp3",},
	{UI_EVENT_ENTER_RECORD, 0, "record.mp3",},
	{UI_EVENT_ENTER_MIC_IN, 0, "mic_in.mp3",},
	{UI_EVENT_ENTER_FM, 0, "fm.mp3",},
	{UI_EVENT_ENTER_AUXTWS, 0, "linein.mp3",},
	{UI_EVENT_ENTER_LEARELAY, 0, "learelay.mp3",},
	{UI_EVENT_ENTER_LEAUDIO, 0, "leaudio.mp3",},
	{UI_EVENT_ENTER_LEMUSIC, 0, "bt_le.mp3",},
	{UI_EVENT_ENTER_OTA, 0, "ota.mp3",},
	{UI_EVENT_ENTER_DEMO, 0, "demo.mp3",},

	{UI_EVENT_PLAY_ENTER_AURACAST_TTS, 0, "ent_aura.mp3",},
	{UI_EVENT_PLAY_EXIT_AURACAST_TTS, 0, "exi_aura.mp3",},

};

#ifdef CONFIG_PLAYTTS
const struct tts_config_t user_tts_config = {
	.tts_storage_media = TTS_SOTRAGE_SDFS,
	.tts_map = tts_ui_event_map,
	.tts_map_size = sizeof(tts_ui_event_map) / sizeof(tts_ui_event_map_t),
};
#endif
void system_tts_policy_init(void)
{
#ifdef CONFIG_PLAYTTS
	tts_manager_register_tts_config(&user_tts_config);
#endif
}
