/********************************************************************************
 *                            USDK(ZS283A)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2018-10-12-下午4:51:13             1.0             build this file
 ********************************************************************************/
/*!
 * \file     soc_pmu.h
 * \brief
 * \author
 * \version  1.0
 * \date  2018-10-12-下午4:51:13
 *******************************************************************************/

#ifndef SOC_PMU_H_
#define SOC_PMU_H_

#define     SYSTEM_SET                                                        (PMU_REG_BASE+0x00)
#define     POWER_CTL                                                         (PMU_REG_BASE+0x04)
#define     BIAS_CTL                                                          (PMU_REG_BASE+0x08)
#define     BDG_CTL                                                           (PMU_REG_BASE+0x0C)
#define     TIMER_CTL                                                         (PMU_REG_BASE+0x10)
#define     WKEN_CTL                                                          (PMU_REG_BASE+0x14)
#define     WAKE_PD                                                           (PMU_REG_BASE+0x18)
#define     ONOFF_KEY                                                         (PMU_REG_BASE+0x1C)
#define     NFC_CTL                                                           (PMU_REG_BASE+0x20)
#define     SPD_CTL                                                           (PMU_REG_BASE+0x24)
#define     DCDC_CTL1                                                         (PMU_REG_BASE+0x30)
#define     DCDC_CTL2                                                         (PMU_REG_BASE+0x34)
#define     LDO_CTL                                                           (PMU_REG_BASE+0x40)
#define     VOUT_CTL                                                          (PMU_REG_BASE+0x44)
#define     MULTI_USED                                                        (PMU_REG_BASE+0x48)
#define     PMUADC_CTL                                                        (PMU_REG_BASE+0x50)
#define     BATADC_DATA                                                       (PMU_REG_BASE+0x54)
#define     TEMPADC_DATA                                                      (PMU_REG_BASE+0x58)
#define     DC5VADC_DATA                                                      (PMU_REG_BASE+0x5C)
#define     SENSADC_DATA                                                      (PMU_REG_BASE+0x60)
#define     LRADC1_DATA                                                       (PMU_REG_BASE+0x64)
#define     LRADC2_DATA                                                       (PMU_REG_BASE+0x68)
#define     LRADC3_DATA                                                       (PMU_REG_BASE+0x6C)
#define     LRADC4_DATA                                                       (PMU_REG_BASE+0x70)
#define     LRADC5_DATA                                                       (PMU_REG_BASE+0x74)
#define     LRADC6_DATA                                                       (PMU_REG_BASE+0x78)
#define     LRADC7_DATA                                                       (PMU_REG_BASE+0x7C)
#define     LRADC8_DATA                                                       (PMU_REG_BASE+0x80)
#define     LRADC9_DATA                                                       (PMU_REG_BASE+0x84)
#define     LRADC10_DATA                                                      (PMU_REG_BASE+0x88)
#define     LRADC11_DATA                                                      (PMU_REG_BASE+0x8C)
#define     AVCCADC_DATA                                                      (PMU_REG_BASE+0x90)

#define     ADC_DATA_BASE   BATADC_DATA

#define     ONOFF_KEY_ONOFF_PRESS_0         0
#define     PMUADC_CTL_DC5VADC_EN           3
#define     PMUADC_CTL_LRADC1_EN            5
#define     LRADC1_DATA_LRADC1_MASK         (0x3FF<<0)

unsigned int soc_pmu_get_vdd_voltage();
void soc_pmu_set_vdd_voltage(unsigned int volt_mv);
void pmu_init(void);

void pmu_spll_init(void);

#endif /* SOC_PMU_H_ */
