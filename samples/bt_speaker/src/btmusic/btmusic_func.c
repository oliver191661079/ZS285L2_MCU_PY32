/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt music function.
 */

#include "btmusic.h"

#ifdef CONFIG_ESD_MANAGER
#include "esd_manager.h"
#endif
#ifdef CONFIG_PLAYTTS
#include "tts_manager.h"
#endif

void btmusic_delay_resume(struct thread_timer *ttimer, void *expiry_fn_arg)
{
	bt_manager_media_play();
}

#ifdef CONFIG_ESD_MANAGER
void btmusic_esd_restore(void)
{
	struct btmusic_app_t *btmusic = btmusic_get_app();

	if (esd_manager_check_esd()) {
		u8_t play_state = 0;

		esd_manager_restore_scene(TAG_PLAY_STATE, &play_state, 1);

		if (play_state) {
			if (thread_timer_is_running(&btmusic->resume_timer)) {
				thread_timer_stop(&btmusic->resume_timer);
			}
			thread_timer_start(&btmusic->resume_timer, 500, 0);
		}
#ifdef CONFIG_PLAYTTS
		tts_manager_unlock();
#endif
		esd_manager_reset_finished();
		return;
	}
}
#endif
