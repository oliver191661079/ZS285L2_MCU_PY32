/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file system wakelock
 */

#ifndef _SYS_WAKELOCK_H
#define _SYS_WAKELOCK_H
/**
 * @defgroup sys_wakelock_apis App system wakelock APIs
 * @ingroup system_apis
 * @{
 */
enum
{
	WAKELOCK_INPUT = 1,
	WAKELOCK_MESSAGE,
	WAKELOCK_BT_EVENT,
	WAKELOCK_PLAYER,
	WAKELOCK_USB_DEVICE,
	WAKELOCK_WAKE_UP,
	WAKELOCK_OTA,
	WAKELOCK_281B_TWS,
	WAKELOCK_PHAPP_TEST,
};
/**
 * @brief init system wakelock manager
 *
 * @details init system wakelock manager context and init state
 *
 * @return 0 excute success .
 * @return others excute failed .
 */
int sys_wakelocks_init(void);
/**
 * @brief hold system wakelock
 *
 * @details hold system wake lock Prevent the system entering sleep
 * @param wakelock_holder who hold the wakelock
 *
 * @return 0 excute success .
 * @return others excute failed .
 */
int sys_wake_lock(int wakelock_holder);
/**
 * @brief release system wakelock
 *
 * @details release system wake lock allowed the system entering sleep
 * @param wakelock_holder who hold the wakelock
 *
 * @return 0 excute success .
 * @return others excute failed .
 */
int sys_wake_unlock(int wakelock_holder);

/**
 * @brief check system wakelock state
 *
 * @details check system wakelock state , wakelock hold by user or not.
 *
 * @return 0 no wakelock holded by user.
 * @return others wakelock holded by user .
 */

int sys_wakelocks_check(void);

/**
 * @brief Return to the time difference between when the system
 *  wakelock is released to the present
 *
 * @return duration of  wakelock all released
 */


u32_t sys_wakelocks_get_free_time(void);


int sys_wakelocks_free_time_reset(void);

/**
 * @} end defgroup sys_wakelock_apis
 */
#endif


