/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file ADC configuration macros for Actions SoC
 */

#ifndef	_ACTIONS_SOC_ADC_H_
#define	_ACTIONS_SOC_ADC_H_


#define	ADC_ID_AVCC			0
#define	ADC_ID_BATV			1
#define ADC_ID_TEMP         2
#define ADC_ID_DC5V         3
#define ADC_ID_SENSOR       4
#define	ADC_ID_CH1			5
#define	ADC_ID_CH2			6
#define	ADC_ID_CH3			7
#define	ADC_ID_CH4			8
#define	ADC_ID_CH5			9
#define	ADC_ID_CH6			10
#define	ADC_ID_CH7			11
#define	ADC_ID_CH8			12
#define	ADC_ID_CH9			13
#define	ADC_ID_CH10			14
#define	ADC_ID_CH11			15

#define	ADC_ID_MAX_ID			ADC_ID_CH11

#define ADC_CHAN_DATA_REG(base, chan)	((volatile u32_t)(base) + (chan) * 0x4)

#ifndef _ASMLANGUAGE

#endif /* _ASMLANGUAGE */

#endif /* _ACTIONS_SOC_ADC_H_	*/
