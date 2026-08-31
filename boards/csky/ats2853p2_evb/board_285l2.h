/*
 * Copyright (c) 2026 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * 285L2 音响高压板 V2.1 — 引脚与功能对照（相对 ATS2853P2 IC5）
 *
 * 固件已启用                          原理图有、固件未用
 * ─────────────────────────────────   ─────────────────────────────
 * UART0  GPIO2/3   调试日志             SD 卡 (GPIO16/17/20)
 * UART1  GPIO21/22 PY32 (TX MFP=0xe / RX MFP=0xe, 表6.4)
 * I2C    GPIO0/19  ACM8635 (TWI MFP=6, 表6.2)
 * GPIO5         ACM8635 PDN（板级 early init 拉高一次，驱动不控）
 * I2STX0 GPIO6/39/38 → ACM8635         CS86756 (IC2) 无驱动
 * GPIO4         耳机插入检测（输入上拉，插入=低）
 * GPIO8         WS2812 氛围灯           DMIC GPIO44/45
 * GPIO14        AMBIC 律动矩阵          LCD/PWM 指示灯
 * USB  DP/DM                           OTA / SD 卡应用
 * BT   RF                               EVB 外置 PA GPIO5+21 模式（285L2 不用）
 */

#ifndef __INC_BOARD_285L2_H
#define __INC_BOARD_285L2_H

#include <soc.h>

/* UART */
#define BOARD_285L2_UART0_RX_GPIO	2
#define BOARD_285L2_UART0_TX_GPIO	3
#define BOARD_285L2_UART1_TX_GPIO	21	/* PY32 硬件 UART1 TX，MFP=0xe */
#define BOARD_285L2_UART1_RX_GPIO	22	/* PY32，MFP=0xe（表6.4） */
#define BOARD_285L2_UART1_TX_MFP	0xe	/* UART1 TX 硬件模式 */
#define BOARD_285L2_UART1_RX_MFP	0xe

/* ACM8635 (IC3) — I2C + I2S TX */
#define BOARD_285L2_I2C_SCL_GPIO	0
#define BOARD_285L2_I2C_SDA_GPIO	19
#define BOARD_285L2_HP_DETECT_GPIO	4	/* 耳机检测孔：内部上拉，未插=高，插入=低 */
#define BOARD_285L2_ACM8635_PDN_GPIO	5	/* 经 R22 22Ω；低有效关机，运行=高 */
#define BOARD_285L2_ACM8635_PDN_DELAY_MS 10
#define BOARD_285L2_I2S_LRCLK_GPIO	6
#define BOARD_285L2_I2S_BCLK_GPIO	39
#define BOARD_285L2_I2S_DOUT_GPIO	38	/* SoC DOUT → ACM8635 SDIN */

/* LED / 灯带 */
#define BOARD_285L2_WS2812_AMBIENT_GPIO	8
#define BOARD_285L2_WS2812_RHYTHM_GPIO	2	/* SPI2_MOSI (MFP=13)，DMA 驱动 */

/*
 * I2C/TWI0（表6.2）：GPIO0=SCL、GPIO19=SDA。
 * 硬件 TWI: MFP=6 + I2C_0；软件 TWI: GPIO bitbang + I2C_GPIO_1（见 CODEC_ACM8635_I2C_BITBANG）。
 */
#define BOARD_285L2_I2C_HW_MFP		6

/* board.c acts_pin_config 与 device 文档一致的 SMIT+上拉+驱动等级0 */
#define BOARD_285L2_I2C_PIN_CTL(mfp) \
	((mfp) | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3) | GPIO_CTL_PULLUP)

#define BOARD_285L2_PIN_CTL(mfp) \
	((mfp) | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(0) | GPIO_CTL_PULLUP)

/* WS2812 / AMBIC1010：GPIO 输出模式、无上拉（开漏输出不可用）、驱动等级 4 */
#define BOARD_285L2_WS2812_PIN_CTL \
	(GPIO_CTL_MFP_GPIO | GPIO_CTL_GPIO_OUTEN | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(4))

#define BOARD_285L2_UART1_TX_PIN_CTL \
	BOARD_285L2_PIN_CTL(BOARD_285L2_UART1_TX_MFP)
#define BOARD_285L2_UART1_RX_PIN_CTL \
	BOARD_285L2_PIN_CTL(BOARD_285L2_UART1_RX_MFP)
#define BOARD_285L2_I2C_SCL_PIN_CTL \
	BOARD_285L2_I2C_PIN_CTL(BOARD_285L2_I2C_HW_MFP)
#define BOARD_285L2_I2C_SDA_PIN_CTL \
	BOARD_285L2_I2C_PIN_CTL(BOARD_285L2_I2C_HW_MFP)

#endif /* __INC_BOARD_285L2_H */
