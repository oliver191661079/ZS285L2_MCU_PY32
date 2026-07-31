/***************************************************************************
 * Copyright (c) 2018 Actions Semi Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.

 * Author: mikeyang<mikeyang@actions-semi.com>
 *
 * Description:    audio asrc head file (for asrc driver inner)
 *
 * Change log:
 *	2018/5/17: Created by mike.
 *   2018/8/1:   detach asrc in and asrc out interface
 ****************************************************************************
 */

#ifndef PLATFORM_INCLUDE_DRIVER_AUDIO_AUDIO_ASRC_H_
#define PLATFORM_INCLUDE_DRIVER_AUDIO_AUDIO_ASRC_H_

#include "audio_inner.h"


#define PCM0_ADDR ((void *)0x00030000)
#define PCM1_ADDR ((void *)0x00031000)
#define PCM2_ADDR ((void *)0x00032000)
#define URAM0_ADDR ((void *)0x00035000)
#define URAM1_ADDR ((void *)0x00035800)
#define PCM3_ADDR ((void *)0x00033000)
#define PCM4_ADDR ((void *)0x00034000)
#define PCM5_ADDR ((void *)0x00034400)
#define PCM6_ADDR ((void *)0x00034800)


#define PCM_SIZE (4096)
#define URAM_SIZE (2048)
#define PCM4_SIZE (1024)
#define PCM5_SIZE (1024)
#define PCM6_SIZE (2048)


/**
**	asrc in 0 
**/
#define K_ASRC_IN_DEC0        (1048576)
#define K_ASRC_IN_DEC1        (1048576)

#define K_ASRC_IN_LGAIN       (1048576)
#define K_ASRC_IN_RGAIN       (1048576)

// Default asrc_in_ctl
#define K_ASRC_IN_CTL         (ASRC_IN_CTL_TABLESEL_512|ASRC_IN_CTL_RAMSEL_P12|ASRC_IN_CTL_RCLKSEL_DSP|ASRC_IN_CTL_WCLKSEL_I2SRX0| \
								ASRC_IN_CTL_MODESEL_SRC|ASRC_IN_CTL_INCH0_AVERAGENUM(1))

/**
**	asrc in 1 
**/
#define K_ASRC_IN1_DEC0        (1048576)
#define K_ASRC_IN1_DEC1        (1048576)

#define K_ASRC_IN1_LGAIN       (1048576)
#define K_ASRC_IN1_RGAIN       (1048576)

// Default asrc_in1_ctl
#define K_ASRC_IN1_CTL         (ASRC_IN1_CTL_RAMSEL_P3|ASRC_IN1_CTL_RCLKSEL_DSP|ASRC_IN1_CTL_WCLKSEL_I2SRX0| \
								ASRC_IN1_CTL_MODESEL_SRC|ASRC_IN1_CTL_INCH1_AVERAGENUM(1))

/**
**	asrc out 0 
**/
#define K_ASRC_OUT0_DEC0      (1048576)
#define K_ASRC_OUT0_DEC1      (1048576)

#define K_ASRC_OUT0_LGAIN     (1048576)
#define K_ASRC_OUT0_RGAIN     (1048576)

// Default asrc_out0_ctl
#define K_ASRC_OUT0_CTL       (ASRC_OUT0_CTL_RAMSEL_P012|ASRC_OUT0_CTL_WCLKSEL_DSP|ASRC_OUT0_CTL_RCLKSEL_DAC| \
								ASRC_OUT0_CTL_MODESEL_SRC|ASRC_OUT0_CTL_OUTCH0_AVERAGENUM(1))

/**
**	asrc out 1 
**/
#define K_ASRC_OUT1_DEC0      (1048576)
#define K_ASRC_OUT1_DEC1      (1048576)

#define K_ASRC_OUT1_LGAIN     (1048576)
#define K_ASRC_OUT1_RGAIN     (1048576)

// Default asrc_out1_ctl
#define K_ASRC_OUT1_CTL       (ASRC_OUT1_CTL_WCLKSEL_DSP|ASRC_OUT1_CTL_RCLKSEL_DAC|ASRC_OUT1_CTL_MODESEL_SRC|ASRC_OUT1_CTL_OUTCH1_AVERAGENUM(1))


/************************* 半空半满 ****************************/
// number of half full sample pairs
#define K_HF_SAMP_PAIR_NUM      (768)
// number of half empty sample pairs
#define K_HE_SAMP_PAIR_NUM      (256)


/********************* ASRC table size  *******************/
#define K_ASRC0_TAB_SIZE        (3584/4)
#define K_ASRC1_TAB_SIZE        (1539-K_ASRC0_TAB_SIZE)
#define ASRC_BUF0_LEN_0         500
#define ASRC_BUF0_LEN_1         (K_ASRC0_TAB_SIZE - ASRC_BUF0_LEN_0)


/********************* ASRC FIFO ADDR *******************/
#define ADDR_ASRC_FIFO_0        (0x00036000)
#define ADDR_ASRC_FIFO_1        (0x00037000)


/******************************ASRC RAM select bitmap ********************************/

#define ASRC_MAX_RAM_NUM                         (9)

#define BITMAP_ASRC_PCMRAM0                      (1<<0)
#define BITMAP_ASRC_PCMRAM1                      (1<<1)
#define BITMAP_ASRC_PCMRAM2                      (1<<2)
#define BITMAP_ASRC_URAM0                        (1<<3)
#define BITMAP_ASRC_URAM1                        (1<<4)
#define BITMAP_ASRC_PCMRAM3                      (1<<5)
#define BITMAP_ASRC_PCMRAM4                      (1<<6)
#define BITMAP_ASRC_PCMRAM5                      (1<<7)
#define BITMAP_ASRC_PCMRAM6                      (1<<8)

/**
**	asrc out 0 ram
**/
#define ASRC_OUT0_PCMRAM012                      (0)
#define ASRC_OUT0_PCMRAM01                       (1)
#define ASRC_OUT0_PCMRAM0                        (2)
#define ASRC_OUT0_URAM0                          (3)
#define ASRC_OUT0_URAM0PCMRAM5                   (4)

/**
**	asrc out 1 ram
**/
#define ASRC_OUT1_PCMRAM2                        (0)
#define ASRC_OUT1_PCMRAM12_16bit                 (2)
#define ASRC_OUT1_PCMRAM3                        (3)
#define ASRC_OUT1_PCMRAM1                        (4)
#define ASRC_OUT1_PCMRAM12_24bit                 (5)

/**
**	asrc in 0 ram
**/
#define ASRC_IN_PCMRAM12                         (0)
#define ASRC_IN_PCMRAM2                          (1)
#define ASRC_IN_URAM1                            (2)
#define ASRC_IN_URAM1PCMRAM4                     (3)

/**
**	asrc in 1 ram
**/
#define ASRC_IN1_PCMRAM3                         (0)
#define ASRC_IN1_PCMRAM1                         (2)
#define ASRC_IN1_PCMRAM36                        (3)


//@defgroup @ref:asrcDatawidth_group  @brief:Asrc data width
//@{
#define ASRCDATAWIDTH_8BIT		((uint32_t)0x00000000U)
#define ASRCDATAWIDTH_16BIT		((uint32_t)0x00000001U)
#define ASRCDATAWIDTH_24BIT		((uint32_t)0x00000010U)
//@}

#endif /* PLATFORM_INCLUDE_DRIVER_AUDIO_AUDIO_ASRC_H_ */

