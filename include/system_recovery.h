/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Recovery interface
 */

#ifndef __INCLUDE_SYSTEM_RECOVERY_H__
#define __INCLUDE_SYSTEM_RECOVERY_H__

#ifdef __cplusplus
extern "C" {
#endif

#define RECOVERY_MAX_FAILED_BOOT_TIMES		5

#define RECOVERY_REASON_BOOT_NORMAL		0
#define RECOVERY_REASON_UNKOWN			1
#define RECOVERY_REASON_BAD_CONFIG		2
#define RECOVERY_REASON_FORCELY			3
#define RECOVERY_REASON_OTA_FAILED		4
#define RECOVERY_REASON_BOOT_FAILED		5

/* get the reason for entering recovery */
int system_recovery_get_reason(void);

/* ota flag */
#ifdef CONFIG_SYSTEM_RECOVERY_CHECK_OTA_FLAG
int system_recovery_set_ota_flag(void);
int system_recovery_clear_ota_flag(void);
int system_recovery_get_ota_flag(void);
#else
static inline int system_recovery_set_ota_flag(void)
{
	return 0;
}

static inline int system_recovery_clear_ota_flag(void)
{
	return 0;
}

static inline int system_recovery_get_ota_flag(void)
{
	return 0;
}
#endif

/* boot flag */
#ifdef CONFIG_SYSTEM_RECOVERY_CHECK_BOOT_FLAG
int system_recovery_get_boot_flag(void);
int system_recovery_set_boot_flag(void);
int system_recovery_clear_boot_flag(void);
int system_recovery_check_boot_flag(void);
#else
static inline int system_recovery_get_boot_flag(void)
{
	return 0;
}

static inline int system_recovery_set_boot_flag(void)
{
	return 0;
}

static inline int system_recovery_clear_boot_flag(void)
{
	return 0;
}

static inline int system_recovery_check_boot_flag(void)
{
	return 0;
}
#endif

/* enter recovery forcely */
void system_recovery_reboot_recovery(void);

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif  /* __INCLUDE_SYSTEM_RECOVERY_H__ */
