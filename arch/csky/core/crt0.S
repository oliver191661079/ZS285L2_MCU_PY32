/*
 * Copyright (c) 2013-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <offsets_short.h>
#include <toolchain.h>
#include <linker/sections.h>
#include <arch/cpu.h>

#define VIC_TSPR  0xE000EC10

GTEXT(__start)
GTEXT(_PrepC)

SECTION_FUNC(TEXT, __start)
#ifdef CONFIG_BOOTINFO_MEMORY
	lrw     r0, boot_info_addr
	stw     r1, (r0)
#endif
	lrw     r0, 0x80000100
	mtcr    r0, psr

	lrw     r0, g_exp_table
	mtcr    r0, vbr

	/* init jtag before any hardware init */
	bsr     board_jtag_init
#ifdef CONFIG_SOC_MAPPING_PSRAM_FOR_OS
	/* init psram mapping before data/bss section init */
	bsr	soc_psram_mapping_init
#else
	bsr	soc_psram_cache_init
#endif

	/* use interrupt stack until thread stack is available */
	lrw     r0, _interrupt_stack
	lrw	r1, CONFIG_ISR_STACK_SIZE
	addu    sp, r1, r0

#ifdef CONFIG_INIT_STACKS
	lrw	r1, 0xaaaaaaaa
1:
	stw	r1, (r0)
	addi	r0, 4
	cmplt	r0, sp
	bt	1b
#endif

#ifdef CONFIG_SEPARATE_IRQ_SP
	/* init hardware separate interrupt sp */
	mtcr	sp, cr<15, 1>

	mfcr	r0, cr<31, 0>
	/* 启动中断栈自动切换 */
	bseti	r0, 14

#if 0
	/* 启动中断投机压栈优化处理 */
	bseti	r0, 4
#endif

	mtcr	r0, cr<31, 0>
#endif

	/* setup tspend prio to lowest irq priority */
	lrw	r0, VIC_TSPR
	lrw	r1, 0xe0
	stw	r1, (r0)

	jbsr    _PrepC
