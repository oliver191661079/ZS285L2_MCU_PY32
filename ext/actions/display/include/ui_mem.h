/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file ui memory interface 
 */

#ifndef __UI_MEMORY_H__
#define __UI_MEMORY_H__

int ui_memory_init(void);

void *ui_memory_alloc(uint32_t size);

void ui_memory_free(void *ptr);

void ui_memory_dump_info(uint32_t index);

#endif


