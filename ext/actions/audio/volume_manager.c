/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file volume manager interface
 */

#include <os_common_api.h>
#include <kernel.h>
#include <app_manager.h>
#include <msg_manager.h>
#include <sys_event.h>
#include "ui_manager.h"
#include "audio_system.h"
#include "audio_policy.h"
#include <media_player.h>
#ifdef CONFIG_ESD_MANAGER
#include <esd_manager.h>
#endif
#ifdef CONFIG_BLUETOOTH
#include <bt_manager.h>
#endif
#ifdef CONFIG_CODEC_ACM8635
#include <amp.h>
#endif
int system_volume_get(int stream_type)
{
	return audio_system_get_stream_volume(stream_type);
}

static void _system_volume_set_notify(uint32_t volume, bool is_limited)
{
	struct app_msg new_msg = { 0 };

	new_msg.type = MSG_VOLUME_CHANGED_EVENT;
	new_msg.cmd = is_limited;
	new_msg.value = volume;

	send_async_msg("main", &new_msg);
}

int _system_volume_get_map_stream_type(int stream_type)
{
	struct audio_track_t *audio_track = audio_system_get_track();

	if (audio_track) {
		if(stream_type == AUDIO_STREAM_MUSIC &&
			audio_track->stream_type == AUDIO_STREAM_SOUNDBAR){
			stream_type = audio_track->stream_type;
		}
	}

	return stream_type;
}

void _system_volume_fade_da_volume(int stream_type, int old_volume, int volume, media_player_t *player)
{
	if (audio_policy_is_da_mute(stream_type)){
		if ((old_volume && !volume) || (!old_volume && !volume)){
			media_player_set_volume(player, volume, volume);
			media_player_fade_out(player, 30);
			return;
		}

		if (!old_volume && volume){
			media_player_fade_in(player, 30);
		}
	}

	media_player_set_volume(player, volume, volume);
}

int system_volume_set(int stream_type, int volume, bool display)
{
	int ret = 0;
	bool is_limited = false;
	static uint32_t volume_timestampe = 0;
	int old_volume = audio_system_get_stream_volume(stream_type);
	media_player_t *player = media_player_get_current_main_player();

	stream_type = _system_volume_get_map_stream_type(stream_type);
	/* set DA volume */
	if (player){
		// FIXME should get stream_type from player
		if (audio_system_get_audio_track_handle(stream_type)) {
			if (volume <= 0) {
				volume = 0;
			} else if (volume >= audio_policy_get_volume_level()) {
				volume = audio_policy_get_volume_level();
			}
			_system_volume_fade_da_volume(stream_type, old_volume, volume, player);
		}
	}

	ret = audio_system_set_stream_volume(stream_type, volume);
	if (display) {
		if (ret == MAX_VOLUME_VALUE) {
			if ((uint32_t)(k_cycle_get_32() - volume_timestampe) / (sys_clock_hw_cycles_per_sec / 1000000)> 500000) {
				sys_event_notify(SYS_EVENT_MAX_VOLUME);
				volume_timestampe = k_cycle_get_32();
			}
			is_limited = true;
		} else if (ret == MIN_VOLUME_VALUE) {
			if ((uint32_t)(k_cycle_get_32() - volume_timestampe) / (sys_clock_hw_cycles_per_sec / 1000000) > 500000) {
				sys_event_notify(SYS_EVENT_MIN_VOLUME);
				volume_timestampe = k_cycle_get_32();
			}
			is_limited = true;
		}
	}
	if (old_volume == audio_system_get_stream_volume(stream_type)) {
		is_limited = true;
		ret = old_volume;
		goto exit;
	}

	ret = volume;

#ifdef CONFIG_CODEC_ACM8635
	/* Sync volume to ACM8635 codec pre-volume registers */
	if (stream_type == AUDIO_STREAM_MUSIC ||
	    stream_type == AUDIO_STREAM_VOICE ||
	    stream_type == AUDIO_STREAM_TTS ||
	    stream_type == AUDIO_STREAM_LINEIN ||
	    stream_type == AUDIO_STREAM_USOUND ||
	    stream_type == AUDIO_STREAM_LE_AUDIO ||
	    stream_type == AUDIO_STREAM_LOCAL_MUSIC) {
		struct device *amp_dev = device_get_binding(CONFIG_AMP_DEV_NAME);
		if (amp_dev != NULL) {
			amp_set_volume(amp_dev, (u8_t)volume);
		}
	}
#endif

#ifdef CONFIG_BT_MANAGER
	audio_system_mutex_lock();
	if (stream_type == AUDIO_STREAM_SOUNDBAR ||
		stream_type == AUDIO_STREAM_MUSIC) {
		bt_manager_volume_set(volume,BT_VOLUME_TYPE_BR_MUSIC);
	} else if(stream_type == AUDIO_STREAM_VOICE){
		bt_manager_volume_set(volume,BT_VOLUME_TYPE_BR_CALL);
	} else if(stream_type == AUDIO_STREAM_LE_AUDIO){
		bt_manager_volume_set(bt_manager_music_to_lea_volume(volume),BT_VOLUME_TYPE_LE_AUDIO);
	}
	audio_system_mutex_unlock();
#endif

#ifdef CONFIG_ESD_MANAGER
	{
		uint16_t volume_info = ((stream_type & 0xff) << 8) | (volume & 0xff);
		esd_manager_save_scene(TAG_VOLUME, (uint8_t *)&volume_info, 2);
	}
#endif

#ifdef CONFIG_TWS
	bt_manager_tws_sync_volume_to_slave(NULL, stream_type, volume);
#endif
	SYS_LOG_INF("old_volume %d new_volume %d\n", old_volume, volume);
exit:

	if (display) {
		_system_volume_set_notify(ret, is_limited);
	}
	return ret;
}

int system_volume_down(int stream_type, int decrement)
{
	return system_volume_set(stream_type,
				system_volume_get(stream_type) - decrement, true);
}

int system_volume_up(int stream_type, int increment)
{
	return system_volume_set(stream_type,
				system_volume_get(stream_type) + increment, true);
}

#if 0
static void _system_volume_change_notify(uint32_t volume)
{
#ifdef CONFIG_BT_MANAGER
	struct bt_volume_report rep;
	rep.handle = 0;//to do
	rep.value = volume;

	bt_manager_volume_event(BT_VOLUME_VALUE, &rep, sizeof(rep));
#endif
}

void system_volume_sync_remote_to_device(uint32_t stream_type, uint32_t volume)
{
	int old_volume = audio_system_get_stream_volume(stream_type);

	if (old_volume == volume) {
		return;
	}

	SYS_LOG_INF("old_volume %d new_volume %d\n", old_volume, volume);

#ifdef CONFIG_TWS
	bt_manager_tws_sync_volume_to_slave(NULL, stream_type, volume);
#endif

#ifdef CONFIG_ESD_MANAGER
	{
		uint16_t volume_info = ((stream_type & 0xff) << 8) | (volume & 0xff);
		esd_manager_save_scene(TAG_VOLUME, (uint8_t *)&volume_info, 2);
	}
#endif

	_system_volume_change_notify(volume);

#ifdef CONFIG_TWS
	if (bt_manager_tws_get_dev_role() == BTSRV_TWS_SLAVE) {
		return;
	}
#endif
}
#endif

int system_player_volume_set(int stream_type, int volume)
{
	int ret = 0;
	int old_volume = audio_system_get_stream_volume(stream_type);
	media_player_t *player = media_player_get_current_main_player();

	stream_type = _system_volume_get_map_stream_type(stream_type);
	/* set DA volume */
	if (player){
		// FIXME should get stream_type from player
		if (audio_system_get_audio_track_handle(stream_type)){
			_system_volume_fade_da_volume(stream_type, old_volume, volume, player);
		}
	}

	ret = audio_system_set_stream_volume(stream_type, volume);

	if (old_volume == audio_system_get_stream_volume(stream_type)) {
		ret = old_volume;
		goto exit;
	}
	ret = volume;

#ifdef CONFIG_CODEC_ACM8635
	/* Sync volume to ACM8635 codec pre-volume registers.
	 * System volume 0-16 maps directly to ACM8635 vol table (0dB..-109.5dB).
	 */
	if (stream_type == AUDIO_STREAM_MUSIC ||
	    stream_type == AUDIO_STREAM_VOICE ||
	    stream_type == AUDIO_STREAM_TTS ||
	    stream_type == AUDIO_STREAM_LINEIN ||
	    stream_type == AUDIO_STREAM_USOUND ||
	    stream_type == AUDIO_STREAM_LE_AUDIO ||
	    stream_type == AUDIO_STREAM_LOCAL_MUSIC) {
		struct device *amp_dev = device_get_binding(CONFIG_AMP_DEV_NAME);
		if (amp_dev != NULL) {
			amp_set_volume(amp_dev, (u8_t)volume);
		}
	}
#endif

	SYS_LOG_INF("old_volume %d new_volume %d\n", old_volume, volume);
exit:

	return ret;
}

int system_player_smart_control_volume_set(int volume)
{
	audio_policy_set_smart_control_gain(volume);
	media_player_t *player = media_player_get_current_main_player();
	if (player){
		media_player_set_smart_control_volume(player, volume);
	}

	return 0;
}
