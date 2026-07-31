/*
 * Copyright (c) 2024 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt le audio stereo mode manager.
*/

#ifndef __BT_MANAGER_LEA_STEREO_H__
#define __BT_MANAGER_LEA_STEREO_H__

/**
* @brief bt manager lea enter stereo mode
*
* api for entering lea stereo mode
*/
void bt_manager_lea_enter_stereo_mode(bool primary);

/**
* @brief bt manager lea exit stereo mode
*
* api for exit lea stereo mode
*/
void bt_manager_lea_exit_stereo_mode(void);

void bt_manager_lea_stereo_set_sirk(uint8_t *sirk_ptr);


/**
* @brief bt manager lea init stereo mode
*
* api for Blutooth ON, call this func before adv start
*/
void bt_manager_lea_stereo_mode_init(bool primary);

#endif /*__BT_MANAGER_LEA_STEREO_H__*/


