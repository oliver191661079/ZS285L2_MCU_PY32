/*
 * Copyright (c) 2013-2015 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief System/hardware module for ti_lm3s6965 platform
 *
 * This module provides routines to initialize and support board-level hardware
 * for the ti_lm3s6965 platform.
 */

#include <kernel.h>
#include <soc.h>
#include "image.h"

#define BOOT_VERSION    0x0001

#define __section__(n) __attribute__((section (n)))

#define HEAD_SECTION  __section__(".img_header")

extern void __start(void*, void*);

const image_head_t HEAD_SECTION brec_head =
{
    .magic0         = IMAGE_MAGIC0,
    .magic1         = IMAGE_MAGIC1,
    .load_addr      = CONFIG_FLASH_BASE_ADDRESS,
    .name           = "sys",
    .version        = 0x0001,
    .header_size    = sizeof(brec_head),
    .header_chksum  = 0,
    .data_chksum    = 0,
    .body_size      = 0,
    .tail_size      = 0,
    .entry          = (void (*)(void *, void *))__start,
    .attribute      = 0,
    {
        0,    //fw size
        0,
    }
};
