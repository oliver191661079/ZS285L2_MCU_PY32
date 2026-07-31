/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file ram_dump memory config
 */

#include <string.h>
#include <zephyr/types.h>
#include <soc.h>
#include <linker/linker-defs.h>
#include "ramdump_core.h"

const ramd_item_t ramdump_mem_regions[] = {
	//sram region
	{(uintptr_t)&__app_bss_start, (uintptr_t)0x60000},
	{(uintptr_t)0x80000, (uintptr_t)0x9c000},

	{(uintptr_t)0x30000, (uintptr_t)0x38a00},

	//reg region
	//memory controller
	{(uintptr_t)MEMORY_CONTROLLER_REG_BASE, (uintptr_t)(MEMORY_CONTROLLER_REG_BASE + 0x400)},

	//dma controller
	{(uintptr_t)DMA_REG_BASE, (uintptr_t)(DMA_REG_BASE + 0x800)},

	//CMU ANA
	{(uintptr_t)CMU_ANALOG_REG_BASE, (uintptr_t)(CMU_ANALOG_REG_BASE + 0x44)},

	//CMU DIGITAL
	{(uintptr_t)CMU_DIGITAL_REG_BASE, (uintptr_t)(CMU_DIGITAL_REG_BASE + 0x100)},

	//SPI0
	{(uintptr_t)SPI0_REG_BASE, (uintptr_t)(SPI0_REG_BASE + 0x24)},

	//DAC
	{(uintptr_t)AUDIO_DAC_REG_BASE, (uintptr_t)(AUDIO_DAC_REG_BASE + 0x30)},

	//ADC
	{(uintptr_t)AUDIO_ADC_REG_BASE, (uintptr_t)(AUDIO_ADC_REG_BASE + 0x30)},

	//I2STX
	{(uintptr_t)AUDIO_I2STX0_REG_BASE, (uintptr_t)(AUDIO_I2STX0_REG_BASE + 0x30)},

	//dcache region
	{(uintptr_t)0x68000000 , (uintptr_t)0x68008000},
	//{(uintptr_t)&__ramfunc_start, (uintptr_t)(&_end)},

	{0, 0} /* End of list */
};

