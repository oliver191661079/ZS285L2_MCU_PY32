/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <kernel.h>
#include <init.h>
#include <soc.h>
#include <nvram_config.h>
#include <system_recovery.h>

#define SYS_LOG_DOMAIN "recovery"
#define SYS_LOG_LEVEL SYS_LOG_LEVEL_INFO
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_SYSTEM_RECOVERY

#define RECOVERY_CONFIG_ENTER_RECOVERY		"RCFG_ENTER_RECOVERY"
#define RECOVERY_CONFIG_OTA_STATUS		"RCFG_OTA_STATUS"
#define RECOVERY_CONFIG_BOOT_FLAG		"RCFG_BOOT_FLAG"

#define OTA_STATUS_IN_PROCESS			0x52504e49 /* 'INPR' */
#define OTA_STATUS_DONE				0x454e4f44 /* 'DONE */

#define FLAG_ENTER_RECOVERY			0x4f434552 /* 'RECO */

/* the reason for why we need enter recovery */
static int system_recovery_reason;

int system_recovery_get_reason(void)
{
	return system_recovery_reason;
}

void system_recovery_reboot_recovery(void)
{
	int data;

	data = FLAG_ENTER_RECOVERY;
#ifdef CONFIG_NVRAM_CONFIG
	nvram_config_set(RECOVERY_CONFIG_ENTER_RECOVERY, &data, sizeof(data));
#endif
	/* boot to recovery */
	sys_pm_reboot(REBOOT_TYPE_GOTO_RECOVERY);
}

#ifdef CONFIG_SYSTEM_RECOVERY_CHECK_OTA_FLAG
int system_recovery_get_ota_flag(void)
{
	int data, ret;
#ifdef CONFIG_NVRAM_CONFIG
	ret = nvram_config_get(RECOVERY_CONFIG_OTA_STATUS, &data, sizeof(data));
#endif
	if (ret < 0) {
		/* no ota flag? */
		if (ret == -ENOENT)
			return 0;
		return ret;
	}

	/* has ota flag */
	return 1;
}

int system_recovery_clear_ota_flag(void)
{
	int ret;

	ACT_LOG_ID_INF(ALF_STR_system_recovery_clear_ota_flag__CLEAR_OTA_FLAG, 0);

	/* remove ota flag config */
#ifdef CONFIG_NVRAM_CONFIG
	ret = nvram_config_set(RECOVERY_CONFIG_OTA_STATUS, NULL, 0);
	if (ret) {
		ACT_LOG_ID_ERR(ALF_STR_system_recovery_clear_ota_flag__FAILED_TO_CLEAR_BOOT, 0);
		return ret;
	}
#endif
	return 0;
}

int system_recovery_set_ota_flag(void)
{
	int data, ret;

	ACT_LOG_ID_INF(ALF_STR_system_recovery_set_ota_flag__SET_OTA_FLAG, 0);
#ifdef CONFIG_NVRAM_CONFIG
	data = OTA_STATUS_IN_PROCESS;
	ret = nvram_config_set(RECOVERY_CONFIG_OTA_STATUS, &data, sizeof(data));
	if (ret) {
		ACT_LOG_ID_ERR(ALF_STR_system_recovery_set_ota_flag__FAILED_TO_SET_OTA_FL, 0);
		return ret;
	}
#endif
	return 0;
}
#endif

#ifdef CONFIG_SYSTEM_RECOVERY_CHECK_BOOT_FLAG
int system_recovery_get_boot_flag(void)
{
	int data, ret;
#ifdef CONFIG_NVRAM_CONFIG
	ret = nvram_config_get(RECOVERY_CONFIG_BOOT_FLAG, &data, sizeof(data));
#endif
	if (ret < 0) {
		/* initial value */
		data = 0;
	}

	return data;
}

int system_recovery_clear_boot_flag(void)
{
	int data, ret;

	ACT_LOG_ID_INF(ALF_STR_system_recovery_clear_boot_flag__CLEAR_BOOT_FLAG, 0);
#ifdef CONFIG_NVRAM_CONFIG
	data = 0;
	ret = nvram_config_set(RECOVERY_CONFIG_BOOT_FLAG, &data, sizeof(data));
	if (ret < 0) {
		ACT_LOG_ID_ERR(ALF_STR_system_recovery_clear_boot_flag__FAILED_TO_CLEAR_BOOT, 0);
		return ret;
	}
#endif
	return 0;
}

int system_recovery_set_boot_flag(void)
{
	int data, ret;

	ACT_LOG_ID_INF(ALF_STR_system_recovery_set_boot_flag__SET_BOOT_FLAG, 0);

	/* get original boot flag */
	data = system_recovery_get_boot_flag();

	/* increase boot times, need clear after boot successfully in application */
	data++;
#ifdef CONFIG_NVRAM_CONFIG
	ACT_LOG_ID_INF(ALF_STR_system_recovery_set_boot_flag__SET_BOOT_FLAG_D, 1, data);
	ret = nvram_config_set(RECOVERY_CONFIG_BOOT_FLAG, &data, sizeof(data));
	if (ret) {
		ACT_LOG_ID_ERR(ALF_STR_system_recovery_set_boot_flag__FAILED_TO_SET_BOOT_F, 0);
		return ret;
	}
#endif
	return 0;
}

int system_recovery_check_boot_flag(void)
{
	int data;

	ACT_LOG_ID_DBG(ALF_STR_system_recovery_check_boot_flag__CHECK_BOOT_FLAG, 0);

	data = system_recovery_get_boot_flag();
	if (data > RECOVERY_MAX_FAILED_BOOT_TIMES) {
		/* failed to boot too much, need enter recovery! */
		ACT_LOG_ID_ERR(ALF_STR_system_recovery_check_boot_flag__FAILED_TO_BOOT_D_TIM, 1, data);
		return 1;
	}

	return 0;
}
#endif

#ifdef CONFIG_SYSTEM_RECOVERY_APP
static int check_recovery_flags(void)
{
	unsigned int data;
	int ret;

	ACT_LOG_ID_DBG(ALF_STR_check_recovery_flags__CHECK_RECOVERY_FLAG, 0);
#ifdef CONFIG_NVRAM_CONFIG
	/* check enter recovery flag */
	ret = nvram_config_get(RECOVERY_CONFIG_ENTER_RECOVERY, &data, sizeof(data));
	if (ret > 0 && data == FLAG_ENTER_RECOVERY) {
		/* enter recovery forcely! */
		ACT_LOG_ID_ERR(ALF_STR_check_recovery_flags__RECOVERY_FLAG_IS_SET, 0);
	#ifdef CONFIG_NVRAM_CONFIG
		/* clear enter recovery flag */
		nvram_config_set(RECOVERY_CONFIG_ENTER_RECOVERY, NULL, 0);
	#endif
		return RECOVERY_REASON_FORCELY;
	}
#endif

#ifdef CONFIG_SYSTEM_RECOVERY_CHECK_OTA_FLAG
	/* check ota-in-process flag */
	ret = system_recovery_get_ota_flag();
	if (ret) {
		/* OTA is not finished, need enter recovery! */
		ACT_LOG_ID_ERR(ALF_STR_check_recovery_flags__OTA_IS_NOT_DONE_NORM, 0);
		return RECOVERY_REASON_OTA_FAILED;
	}
#endif

#ifdef CONFIG_SYSTEM_RECOVERY_CHECK_BOOT_FLAG
	/* check boot flag */
	ret = system_recovery_check_boot_flag();
	if (ret) {
		/* OTA is not finished, need enter recovery! */
		ACT_LOG_ID_ERR(ALF_STR_check_recovery_flags__NORMAL_BOOT_FAILED_G, 0);
		return RECOVERY_REASON_BOOT_FAILED;
	}

	/* set boot flag */
	ret = system_recovery_set_boot_flag();
	if (ret) {
		ACT_LOG_ID_ERR(ALF_STR_check_recovery_flags__FAILED_TO_SET_BOOT_F, 0);
		return RECOVERY_REASON_BAD_CONFIG;
	}
#endif

	/* enter recovery forcely! */
	ACT_LOG_ID_INF(ALF_STR_check_recovery_flags__NORMAL_BOOT, 0);

	return RECOVERY_REASON_BOOT_NORMAL;
}
#endif

static int system_recovery_init(struct device *dev)
{
	int reason;

	ARG_UNUSED(dev);

	ACT_LOG_ID_INF(ALF_STR_system_recovery_init__INT_SYSTEM_RECOVERY, 0);

#ifdef CONFIG_SYSTEM_RECOVERY_APP
	reason = check_recovery_flags();
	if (reason == RECOVERY_REASON_BOOT_NORMAL) {
		/* boot normally */
		sys_pm_reboot(REBOOT_TYPE_GOTO_WIFISYS);

		/* never run to here */
	} else {
		ACT_LOG_ID_ERR(ALF_STR_system_recovery_init__ENTER_RECOVERY_REASO, 1, reason);
	}

#else
	reason = RECOVERY_REASON_BOOT_NORMAL;
#endif

	system_recovery_reason = reason;

	return 0;
}

SYS_INIT(system_recovery_init, PRE_KERNEL_1, CONFIG_SYSTEM_RECOVERY_INIT_PRIORITY);
