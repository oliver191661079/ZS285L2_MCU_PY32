/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file system wakelock
 */

#if defined(CONFIG_SYS_LOG)
#define SYS_LOG_DOMAIN "sys_wl"
#define SYS_LOG_LEVEL 4
#include <logging/sys_log.h>
#endif

#include <os_common_api.h>
#include <string.h>
#include <sys_wakelock.h>

static uint32_t wakelocks_bitmaps;
static uint32_t wakelocks_free_timestamp;

static int _sys_wakelock_lookup(int wakelock_holder)
{
	if (wakelocks_bitmaps & (1 << wakelock_holder)) {
		return wakelock_holder;
	} else {
		return 0;
	}
}

int sys_wake_lock(int wakelock_holder)
{
	int res = 0;
	int wakelock = 0;
#ifdef CONFIG_SYS_IRQ_LOCK
	SYS_IRQ_FLAGS flags;

	sys_irq_lock(&flags);
#endif
	wakelock = _sys_wakelock_lookup(wakelock_holder);
	if (wakelock) {
		res = -EEXIST;
		goto exit;
	}

	wakelocks_bitmaps |= (1 << wakelock_holder);
	wakelocks_free_timestamp = 0;

exit:
#if CONFIG_SYS_IRQ_LOCK
	sys_irq_unlock(&flags);
#endif
	return res;
}

int sys_wake_unlock(int wakelock_holder)
{
	int res = 0;
	int wakelock = 0;
#ifdef CONFIG_SYS_IRQ_LOCK
	SYS_IRQ_FLAGS flags;

	sys_irq_lock(&flags);
#endif
	wakelock = _sys_wakelock_lookup(wakelock_holder);
	if (!wakelock) {
		res = -ESRCH;
		goto exit;
	}

	wakelocks_bitmaps &= (~(1 << wakelock_holder));

	if (!wakelocks_bitmaps) {
		wakelocks_free_timestamp = os_uptime_get_32();
	}

exit:
#if CONFIG_SYS_IRQ_LOCK
	sys_irq_unlock(&flags);
#endif
	return res;
}

int sys_wakelocks_init(void)
{
	wakelocks_free_timestamp = os_uptime_get_32();
	return 0;
}


int sys_wakelocks_check(void)
{
	return wakelocks_bitmaps;
}

uint32_t sys_wakelocks_get_free_time(void)
{
	uint32_t free_time;
#ifdef CONFIG_SYS_IRQ_LOCK
	SYS_IRQ_FLAGS flags;

	sys_irq_lock(&flags);
#endif

	if (wakelocks_bitmaps || !wakelocks_free_timestamp){
		free_time = 0;
	}else{
		free_time = os_uptime_get_32() - wakelocks_free_timestamp;
	}

#if CONFIG_SYS_IRQ_LOCK
	sys_irq_unlock(&flags);
#endif

	return free_time;
}

int sys_wakelocks_free_time_reset(void)
{
	SYS_LOG_DBG("reset");
	wakelocks_free_timestamp = os_uptime_get_32();
	return 0;
}

