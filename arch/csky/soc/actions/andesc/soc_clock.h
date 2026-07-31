/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file peripheral clock configuration macros for Actions SoC
 */

#ifndef	_ACTIONS_SOC_CLOCK_H_
#define	_ACTIONS_SOC_CLOCK_H_

#define	CLOCK_ID_DMA			0
#define	CLOCK_ID_ASRC			1
#define	CLOCK_ID_AUDIO_DAC		2
#define	CLOCK_ID_AUDIO_ADC		3
#define	CLOCK_ID_I2S0_TX		4
#define	CLOCK_ID_I2S0_RX		5
#define	CLOCK_ID_I2S1_RX		6
#define	CLOCK_ID_SPDIF_TX		7
#define	CLOCK_ID_SPDIF_RX		8
#define	CLOCK_ID_SD0			9
#define	CLOCK_ID_SD1			10
#define	CLOCK_ID_SPI0			11
#define	CLOCK_ID_SPI1			12
#define	CLOCK_ID_SPI0_CACHE		13
#define	CLOCK_ID_SPI1_CACHE		14
#define	CLOCK_ID_UART0			15
#define	CLOCK_ID_UART1			16
#define	CLOCK_ID_I2C0			17
#define	CLOCK_ID_LCD			18
#define	CLOCK_ID_SEG_LCD		19
#define	CLOCK_ID_FM_CLK			20
#define	CLOCK_ID_PWM			21
#define	CLOCK_ID_LRADC			22
#define	CLOCK_ID_USB			23
#define	CLOCK_ID_TIMER0			25
#define CLOCK_ID_TIMER1			CLOCK_ID_TIMER0
#define	CLOCK_ID_IR			26
#define	CLOCK_ID_DMIC			27
#define	CLOCK_ID_I2S_SRD		28
#define	CLOCK_ID_SPI2			29
#define	CLOCK_ID_EXINT			30
#define	CLOCK_ID_EFUSE			31
#define	CLOCK_ID_BT_BB_3K2		32
#define	CLOCK_ID_BT_BB_DIG		33
#define	CLOCK_ID_BT_BB_AHB		34
#define	CLOCK_ID_BT_MODEM_AHB		35
#define	CLOCK_ID_BT_MODEM_DIG		36
#define	CLOCK_ID_BT_MODEM_INTF		37
#define	CLOCK_ID_BT_MODEM_DMA		38
#define	CLOCK_ID_BT_RF			39
#define	CLOCK_ID_DSP_COMM		60
#define	CLOCK_ID_DSP			61
#define	CLOCK_ID_SCPU			62
#define	CLOCK_ID_MCPU			63


#define	CLOCK_ID_MAX_ID			63

#ifndef _ASMLANGUAGE

void acts_clock_peripheral_enable(int clock_id);
void acts_clock_peripheral_disable(int clock_id);
void acts_deep_sleep_clock_peripheral(const char * id_buffer, int buffer_len);

#endif /* _ASMLANGUAGE */

#endif /* _ACTIONS_SOC_CLOCK_H_	*/
