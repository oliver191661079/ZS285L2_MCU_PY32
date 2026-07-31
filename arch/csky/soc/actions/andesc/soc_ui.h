/********************************************************************************
 *                            USDK(ZS283A)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2018-10-13-上午10:43:15             1.0             build this file
 ********************************************************************************/
/*!
 * \file     soc_ui.h
 * \brief
 * \author
 * \version  1.0
 * \date  2018-10-13-上午10:43:15
 *******************************************************************************/

#ifndef SOC_UI_H_
#define SOC_UI_H_


#define     SEG_SREEN_CTL                                                     (SEG_SREEN_REG_BASE+0x0000)
#define     SEG_SREEN_DATA0                                                   (SEG_SREEN_REG_BASE+0x0004)
#define     SEG_SREEN_DATA1                                                   (SEG_SREEN_REG_BASE+0x0008)
#define     SEG_SREEN_DATA2                                                   (SEG_SREEN_REG_BASE+0x000C)
#define     SEG_SREEN_DATA3                                                   (SEG_SREEN_REG_BASE+0x0010)
#define     SEG_SREEN_DATA4                                                   (SEG_SREEN_REG_BASE+0x0014)
#define     SEG_SREEN_DATA5                                                   (SEG_SREEN_REG_BASE+0x0018)
#define     SEG_RC_EN                                                         (SEG_SREEN_REG_BASE+0x001C)
#define     SEG_BIAS_EN                                                       (SEG_SREEN_REG_BASE+0x0020)

#define  SEG_BIAS_EN_LED_SEG_BIAS_MASK    (0x7<<0)


#endif /* SOC_UI_H_ */
