/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file peripheral reset configuration macros for Actions SoC
 */

#ifndef	_ACTIONS_SOC_RESET_H_
#define	_ACTIONS_SOC_RESET_H_

#define	RESET_ID_DMA			0
#define	RESET_ID_ASRC			1
#define	RESET_ID_DAC			2
#define	RESET_ID_ADC			3
#define	RESET_ID_I2S			4
#define	RESET_ID_SPDIFRX		5
#define	RESET_ID_SPDIFTX		6
#define	RESET_ID_AUDIO			8
#define	RESET_ID_SD0			9
#define	RESET_ID_SD1			10
#define	RESET_ID_SPI0			11
#define	RESET_ID_SPI1			12
#define	RESET_ID_SPI0_CACHE		13
#define	RESET_ID_SPI1_CACHE		14
#define	RESET_ID_UART0			15
#define	RESET_ID_UART1			16
#define	RESET_ID_I2C0			17
#define	RESET_ID_LCD			18
#define	RESET_ID_SEG_LCD		19
#define	RESET_ID_PWM			21
#define	RESET_ID_USB			23
#define	RESET_ID_USB2			24
#define	RESET_ID_IR			26
#define	RESET_ID_SPI2			29
#define	RESET_ID_BIST			30
#define	RESET_ID_BT_BB			32
#define	RESET_ID_BT_MODEM		33
#define	RESET_ID_BT_RF			34
#define	RESET_ID_DSP_ALL		60
#define	RESET_ID_DSP_PART		61
#define	RESET_ID_SCPU			62
#define	RESET_ID_MCPU			63

#define	RESET_ID_MAX_ID			63

#ifndef _ASMLANGUAGE

void acts_reset_peripheral_assert(int reset_id);
void acts_reset_peripheral_deassert(int reset_id);
void acts_reset_peripheral(int reset_id);
void acts_deep_sleep_peripheral(char *reset_id_buffer, int buffer_len);

#endif /* _ASMLANGUAGE */

#endif /* _ACTIONS_SOC_RESET_H_	*/
