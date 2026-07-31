/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief OTA unloading interface
 */

#ifndef __OTA_UNLOADING_H
#define __OTA_UNLOADING_H


typedef enum
{
    OTA_UNLOADING_INIT,
    OTA_UNLOADING_START,
	OTA_UNLOADING_FINISHED,
}ota_unloading_state_e;

int ota_unloading(void);
int ota_unloading_set_state(ota_unloading_state_e state);
int ota_unloading_get_state(void);
int ota_unloading_clear_all(void);

#endif