/** @file
 *  @brief Generic Audio Common Settings
 */

/*
 * Copyright (c) 2021 Actions Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <acts_bluetooth/audio/codec.h>

#ifndef ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_COMMON_H_
#define ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_COMMON_H_

uint16_t bt_supported_sampling_from_khz(uint16_t khz);
uint8_t bt_sampling_from_khz(uint16_t khz);
uint16_t bt_sampling_to_khz(uint8_t value);
uint16_t bt_support_sampling_min(uint16_t value);
uint16_t bt_support_sampling_max(uint16_t value);
uint8_t bt_audio_channel_min(uint8_t value);
uint8_t bt_audio_channel_max(uint8_t value);
uint8_t bt_supported_frame_durations_from_ms(uint8_t ms, bool prefer);
uint8_t bt_frame_duration_from_ms(uint8_t ms);
uint8_t bt_frame_duration_to_ms(uint8_t duration);
uint8_t bt_frame_duration_from_supported(uint8_t value);
uint8_t bt_channels_by_locations(uint32_t value);

#endif /* ZEPHYR_INCLUDE_BLUETOOTH_AUDIO_COMMON_H_ */
