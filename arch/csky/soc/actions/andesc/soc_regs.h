/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file register address define for Actions SoC
 */

#ifndef	_ACTIONS_SOC_REGS_H_
#define	_ACTIONS_SOC_REGS_H_


#define RMU_REG_BASE			        0xc0000000
#define CMU_REG_BASE			        0xc0001000
#define EFUSE_BASE						0xc0010000
#define ADC_REG_BASE			        0xc0020050
#define DMA_REG_BASE			        0xc0040000
#define AUDIO_DAC_REG_BASE		        0xc0050000
#define AUDIO_ADC_REG_BASE		        0xc0051000
#define AUDIO_I2S_REG_BASE		        0xc0052000
#define AUDIO_I2STX0_REG_BASE			0xc0052000
#define AUDIO_I2SRX0_REG_BASE			0xc0052100
#define AUDIO_I2SRX1_REG_BASE			0xc0052200
#define AUDIO_SPDIFTX_REG_BASE		    0xc0053000
#define AUDIO_SPDIFRX_REG_BASE		    0xc0054000
#define MMC0_REG_BASE			        0xc0060000
#define MMC1_REG_BASE			        0xc0070000
#define GPIO_REG_BASE			        0xc0090000
#define SEG_LED_REG_BASE		        0xc00e0000
#define SPI0_REG_BASE			        0xc0100000
#define SPI1_REG_BASE			        0xc0110000
#define RTC_REG_BASE			        0xc0120000
#define I2C0_REG_BASE			        0xc0130000
#define IRC_REG_BASE			        0xc0150000
#define ASRC_REG_BASE			        0xc0160000
#define PWM_REG_BASE			        0xc0180000
#define UART0_REG_BASE			        0xc0190000
#define UART1_REG_BASE			        0xc01a0000
#define SPI2_REG_BASE			        0xc01b0000
#define CMU_DIGITAL_REG_BASE            0xc0001000
#define CMU_ANALOG_REG_BASE             0xc0000100
#define DSP_MAILBOX_REG_BASE            0xc0001100
#define SPICACHE_REG_BASE				0xc00f0000
#define SPICACHE_REG_BASE				0xc00f0000
#define SPI1_DCACHE_REG_BASE            0xc0140000


#define MEMORY_CONTROLLER_REG_BASE     0xc00a0000
#define INT_REG_BASE                   0xc00b0000
#define SEG_SREEN_REG_BASE             0xc00e0000
#define PMU_REG_BASE                   0xC0020000


/* EFUSE Address */
#define     EFUSE_CTL0					(EFUSE_BASE+0x00)
#define     EFUSE_CTL1					(EFUSE_BASE+0x04)
#define     EFUSE_CTL2					(EFUSE_BASE+0x08)
#define     EFUSE_DATA0					(EFUSE_BASE+0x0C)
#define     EFUSE_DATA1					(EFUSE_BASE+0x10)
#define     EFUSE_DATA2					(EFUSE_BASE+0x14)
#define     EFUSE_DATA3					(EFUSE_BASE+0x18)

/* RMU registers */
#define CK802_WAKEUP_EN0	0xc00b0040
#define CK802_WAKEUP_EN1	0xc00b0044

/* RMU registers */
#define RMU_MRCR0			0xc0000000
#define RMU_MRCR1			0xc0000004

/* CMU Analog registers */
#define CORE_PLL_CTL		0xc0000104
#define SPLL_CTL			0xc0000108
#define AUDIO_PLL0_CTL		0xc000010c
#define AUDIO_PLL1_CTL		0xc0000110
#define CORE_PLL_DEBUG		0xc0000120
#define SPLL_DEBUG			0xc0000124

/* PMU registers */
#define PMU_POWER_CTL		0xc0020004
#define PMU_WKEN_CTL		0xc0020014
#define PMU_WAKE_PD			0xc0020018
#define PMU_ONOFF_KEY		0xc002001c
#define PMU_VOUT_CTL		0xc0020044

/* timer0 */
#define T0_CTL				0xc0120100
#define T0_VAL				0xc0120104
#define T0_CNT				0xc0120108

/* timer1 */
#define T1_CTL				0xc0120120
#define T1_VAL				0xc0120124
#define T1_CNT				0xc0120128

#define T2_CTL              0xc0120140
#define T2_VAL              0xc0120144
#define T2_CNT              0xc0120148

/* timer3 */ /* tws used*/
#define T3_CTL				0xc0120160
#define T3_VAL				0xc0120164
#define T3_CNT				0xc0120168

/* watch dog */
#define WD_CTL				0xc012001c

/* wio0 */
#define WIO0_CTL            0xc0090140

/* HCL 4HZ */
#define HCL_4HZ				0xc0120240

/* jtag ctl */
#define JTAG_CTL            0xc0090000

#define DSP_PAGE_ADDR0      0xc00a0080
#define DSP_VCT_ADDR        0xc0000008

#define RTC_REGUPDATE       0xc0120004
#define RTC_BAK0            0xc0120030
#define RTC_BAK1            0xc0120034
#define RTC_BAK2            0xc0120038
#define RTC_BAK3            0xc012003c


#define INT_REQ_INT_OUT     (INT_REG_BASE+0x00000028)
#define INT_REQ_IN          (INT_REG_BASE+0x0000002c)
#define INT_REQ_IN_PD       (INT_REG_BASE+0x00000030)
#define INT_REQ_OUT         (INT_REG_BASE+0x00000034)

static inline void delay(volatile unsigned int count)
{
	while(count-- != 0);
}

#endif /* _ACTIONS_SOC_REGS_H_	*/
