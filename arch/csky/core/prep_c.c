/*
 * Copyright (c) 2013-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Full C support initialization
 *
 *
 * Initialization of full C support: zero the .bss, copy the .data if XIP,
 * call _Cstart().
 *
 * Stack is available in this module, but not the global data/bss until their
 * initialization is performed.
 */

#include <kernel.h>
#include <zephyr/types.h>
#include <toolchain.h>
#include <linker/linker-defs.h>
#include <nano_internal.h>
#include <string.h>

extern FUNC_NORETURN void _Cstart(void);

#ifdef CONFIG_ROM_PERF_ENABLE
extern void print_brom_performance(void);
#endif

/**
 *
 * @brief Prepare to and run C code
 *
 * This routine prepares for the execution of and runs C code.
 *
 * @return N/A
 */

void _PrepC(void)
{

#ifdef CONFIG_ROM_PERF_ENABLE
    print_brom_performance();
#endif

	_bss_zero();
#ifdef CONFIG_XIP
	_data_copy();
#endif
	_Cstart();
	CODE_UNREACHABLE;
}
