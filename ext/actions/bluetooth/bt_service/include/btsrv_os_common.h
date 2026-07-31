/*
 * Copyright (c) 2016 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt service os common head file
 */

#ifndef _BTSRV_OS_COMMON_H_
#define _BTSRV_OS_COMMON_H_

#ifndef SYS_LOG_DOMAIN
#define SYS_LOG_DOMAIN "btsrv"
#endif

#include <os_common_api.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <kernel.h>
#include <misc/slist.h>
#include <stream.h>
#include <msg_manager.h>
#include <srv_manager.h>
#include <thread_timer.h>
#include <mem_manager.h>
#include <nvram_config.h>
#include <property_manager.h>
#include <acts_ringbuf.h>
#include <media_player.h>
#include <media_service.h>
#include "media_mem.h"
#include "audio_system.h"

#endif	/* _BTSRV_OS_COMMON_H_ */
