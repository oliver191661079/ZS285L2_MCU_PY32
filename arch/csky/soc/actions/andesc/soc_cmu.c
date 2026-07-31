/********************************************************************************
 *                            USDK(ZS283A)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2019-2-18-下午1:38:13             1.0             build this file
 ********************************************************************************/
/*!
 * \file     soc_cmu.c
 * \brief
 * \author
 * \version  1.0
 * \date  2019-2-18-下午1:38:13
 *******************************************************************************/
#include <kernel.h>
#include <device.h>
#include <soc.h>
#include <misc/printk.h>
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_SOC_CMU


#define     HOSC_CTL_HOSCI_BC_SEL_e                                           26
#define     HOSC_CTL_HOSCI_BC_SEL_SHIFT                                       24
#define     HOSC_CTL_HOSCI_BC_SEL_MASK                                        (0x7<<24)
#define     HOSC_CTL_HOSCI_TC_SEL_e                                           23
#define     HOSC_CTL_HOSCI_TC_SEL_SHIFT                                       19
#define     HOSC_CTL_HOSCI_TC_SEL_MASK                                        (0x1F<<19)
#define     HOSC_CTL_HOSCO_BC_SEL_e                                           18
#define     HOSC_CTL_HOSCO_BC_SEL_SHIFT                                       16
#define     HOSC_CTL_HOSCO_BC_SEL_MASK                                        (0x7<<16)
#define     HOSC_CTL_HOSCO_TC_SEL_e                                           15
#define     HOSC_CTL_HOSCO_TC_SEL_SHIFT                                       11
#define     HOSC_CTL_HOSCO_TC_SEL_MASK                                        (0x1F<<11)
#define     HOSC_CTL_HGMC_e                                                   10
#define     HOSC_CTL_HGMC_SHIFT                                               6
#define     HOSC_CTL_HGMC_MASK                                                (0x1F<<6)
#define     HOSC_CTL_BT_HOSC_SEL                                              5
#define     HOSC_CTL_VDD_HOSC_SEL                                             4
#define     HOSC_CTL_HOSC_EN                                                  0

/*!
 * \brief 获取 HOSC 电容值
 * \n  电容值为 x10 的定点数
 */
extern int soc_get_hosc_cap(void)
{
    int  cap = 0;

    uint32_t  hosc_ctl = sys_read32(CMU_HOSC_CTL);

    uint32_t  bc_sel = (hosc_ctl & HOSC_CTL_HOSCO_BC_SEL_MASK) >> HOSC_CTL_HOSCO_BC_SEL_SHIFT;
    uint32_t  tc_sel = (hosc_ctl & HOSC_CTL_HOSCO_TC_SEL_MASK) >> HOSC_CTL_HOSCO_TC_SEL_SHIFT;

    cap = bc_sel * 30 + tc_sel;

    return cap;
}


/*!
 * \brief 设置 HOSC 电容值
 * \n  电容值为 x10 的定点数
 */
extern void soc_set_hosc_cap(int cap)
{
    printk("set hosc cap:%d.%d pf\n", cap / 10, cap % 10);

    uint32_t  hosc_ctl = sys_read32(CMU_HOSC_CTL);

    uint32_t  bc_sel = cap / 30;
    uint32_t  tc_sel = cap % 30;

    hosc_ctl &= ~(
        HOSC_CTL_HOSCI_BC_SEL_MASK |
        HOSC_CTL_HOSCI_TC_SEL_MASK |
        HOSC_CTL_HOSCO_BC_SEL_MASK |
        HOSC_CTL_HOSCO_TC_SEL_MASK);

    hosc_ctl |= (
        (bc_sel << HOSC_CTL_HOSCI_BC_SEL_SHIFT) |
        (tc_sel << HOSC_CTL_HOSCI_TC_SEL_SHIFT) |
        (bc_sel << HOSC_CTL_HOSCO_BC_SEL_SHIFT) |
        (tc_sel << HOSC_CTL_HOSCO_TC_SEL_SHIFT));

    sys_write32(hosc_ctl, CMU_HOSC_CTL);

    k_busy_wait(100);
}

void soc_cmu_init(void)
{
    /* 不改变频偏值,频偏值在compensation驱动里面设置 */
    sys_write32((sys_read32(CMU_HOSC_CTL) & 0xfffff800) | 0x131, CMU_HOSC_CTL);
}



