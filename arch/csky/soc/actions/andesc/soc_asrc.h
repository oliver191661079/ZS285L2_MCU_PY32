/********************************************************************************
 *                            USDK(ZS283A)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2018-10-12-下午4:53:10             1.0             build this file
 ********************************************************************************/
/*!
 * \file     soc_asrc.h
 * \brief
 * \author
 * \version  1.0
 * \date  2018-10-12-下午4:53:10
 *******************************************************************************/

#ifndef SOC_ASRC_H_
#define SOC_ASRC_H_

//--------------ASRC_REGISTER-------------------------------------------//
//--------------Register Address---------------------------------------//
#define     ASRC_IN_CTL                                                       (ASRC_REG_BASE+0x00000000)
#define     ASRC_IN_DEC0                                                      (ASRC_REG_BASE+0x00000004)
#define     ASRC_IN_DEC1                                                      (ASRC_REG_BASE+0x00000008)
#define     ASRC_IN_NUM                                                       (ASRC_REG_BASE+0x0000000c)
#define     ASRC_IN_RFIFO                                                     (ASRC_REG_BASE+0x00000010)
#define     ASRC_IN_WFIFO                                                     (ASRC_REG_BASE+0x00000014)
#define     ASRC_IN_LGAIN                                                     (ASRC_REG_BASE+0x00000018)
#define     ASRC_IN_RGAIN                                                     (ASRC_REG_BASE+0x0000001c)
#define     ASRC_IN_IP                                                        (ASRC_REG_BASE+0x00000020)
#define     ASRC_IN_TIME                                                      (ASRC_REG_BASE+0x00000024)
#define     ASRC_IN_THRES_HF                                                  (ASRC_REG_BASE+0x00000028)
#define     ASRC_IN_THRES_HE                                                  (ASRC_REG_BASE+0x0000002c)
#define     ASRC_OUT0_CTL                                                     (ASRC_REG_BASE+0x00000030)
#define     ASRC_OUT0_DEC0                                                    (ASRC_REG_BASE+0x00000034)
#define     ASRC_OUT0_DEC1                                                    (ASRC_REG_BASE+0x00000038)
#define     ASRC_OUT0_NUM                                                     (ASRC_REG_BASE+0x0000003c)
#define     ASRC_OUT0_RFIFO                                                   (ASRC_REG_BASE+0x00000040)
#define     ASRC_OUT0_WFIFO                                                   (ASRC_REG_BASE+0x00000044)
#define     ASRC_OUT0_LGAIN                                                   (ASRC_REG_BASE+0x00000048)
#define     ASRC_OUT0_RGAIN                                                   (ASRC_REG_BASE+0x0000004c)
#define     ASRC_OUT0_IP                                                      (ASRC_REG_BASE+0x00000050)
#define     ASRC_OUT0_TIME                                                    (ASRC_REG_BASE+0x00000054)
#define     ASRC_OUT0_THRES_HF                                                (ASRC_REG_BASE+0x00000058)
#define     ASRC_OUT0_THRES_HE                                                (ASRC_REG_BASE+0x0000005c)
#define     ASRC_OUT1_CTL                                                     (ASRC_REG_BASE+0x00000060)
#define     ASRC_OUT1_DEC0                                                    (ASRC_REG_BASE+0x00000064)
#define     ASRC_OUT1_DEC1                                                    (ASRC_REG_BASE+0x00000068)
#define     ASRC_OUT1_NUM                                                     (ASRC_REG_BASE+0x0000006c)
#define     ASRC_OUT1_RFIFO                                                   (ASRC_REG_BASE+0x00000070)
#define     ASRC_OUT1_WFIFO                                                   (ASRC_REG_BASE+0x00000074)
#define     ASRC_OUT1_LGAIN                                                   (ASRC_REG_BASE+0x00000078)
#define     ASRC_OUT1_RGAIN                                                   (ASRC_REG_BASE+0x0000007c)
#define     ASRC_OUT1_IP                                                      (ASRC_REG_BASE+0x00000080)
#define     ASRC_OUT1_TIME                                                    (ASRC_REG_BASE+0x00000084)
#define     ASRC_OUT1_THRES_HF                                                (ASRC_REG_BASE+0x00000088)
#define     ASRC_OUT1_THRES_HE                                                (ASRC_REG_BASE+0x0000008c)
#define     ASRC_IN1_CTL                                                      (ASRC_REG_BASE+0x00000090)
#define     ASRC_IN1_DEC0                                                     (ASRC_REG_BASE+0x00000094)
#define     ASRC_IN1_DEC1                                                     (ASRC_REG_BASE+0x00000098)
#define     ASRC_IN1_NUM                                                      (ASRC_REG_BASE+0x0000009c)
#define     ASRC_IN1_RFIFO                                                    (ASRC_REG_BASE+0x000000a0)
#define     ASRC_IN1_WFIFO                                                    (ASRC_REG_BASE+0x000000a4)
#define     ASRC_IN1_LGAIN                                                    (ASRC_REG_BASE+0x000000a8)
#define     ASRC_IN1_RGAIN                                                    (ASRC_REG_BASE+0x000000ac)
#define     ASRC_IN1_IP                                                       (ASRC_REG_BASE+0x000000b0)
#define     ASRC_IN1_TIME                                                     (ASRC_REG_BASE+0x000000b4)
#define     ASRC_IN1_THRES_HF                                                 (ASRC_REG_BASE+0x000000b8)
#define     ASRC_IN1_THRES_HE                                                 (ASRC_REG_BASE+0x000000bc)
#define     ASRC_CLK_CTL                                                      (ASRC_REG_BASE+0x000000c0)
#define     ASRC_IN_DMACTL                                                    (ASRC_REG_BASE+0x000000c4)
#define     ASRC_OUT0_DMACTL                                                  (ASRC_REG_BASE+0x000000c8)
#define     ASRC_OUT1_DMACTL                                                  (ASRC_REG_BASE+0x000000cc)
#define     ASRC_IN1_DMACTL                                                   (ASRC_REG_BASE+0x000000d0)
#define     ASRC_INT_EN                                                       (ASRC_REG_BASE+0x000000d4)
#define     ASRC_INT_PD                                                       (ASRC_REG_BASE+0x000000d8)
#define     ASRC_IN_INT_THRES                                                 (ASRC_REG_BASE+0x000000dc)
#define     ASRC_OUT0_INT_THRES                                               (ASRC_REG_BASE+0x000000e0)
#define     ASRC_OUT1_INT_THRES                                               (ASRC_REG_BASE+0x000000e4)
#define     ASRC_IN1_INT_THRES                                                (ASRC_REG_BASE+0x000000e8)

#endif /* SOC_ASRC_H_ */
