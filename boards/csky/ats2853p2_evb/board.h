/*
 * Copyright (c) 2015 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __INC_BOARD_H
#define __INC_BOARD_H

#include <soc.h>

#ifdef CONFIG_CODEC_ACM8635
#include "board_285l2.h"
#endif

#ifdef CONFIG_MMC_0
#define BOARD_SDCARD_DETECT_GPIO_NAME		"GPIO_0"
#define BOARD_SDCARD_DETECT_GPIO		19	/* detect */
#define BOARD_SDCARD_USE_INTERNAL_PULLUP		/* use SoC internal pull-up resistance */
#define BOARD_SDCARD_POWER_EN_GPIO_NAME		"GPIO_0"
#define BOARD_SDCARD_POWER_EN_GPIO		19	/* detect gpio is also used as power en gpio */
#endif

#define BOARD_SPI1_PSRAM_CS_GPIO_NAME		"GPIO_0"
#ifdef CONFIG_SOC_MAPPING_PSRAM_FOR_OS
#define BOARD_SPI1_PSRAM_CS_GPIO		16	/* spi1_ss */
#endif

#define BOARD_SPI1_NOR_CS_GPIO_NAME		"GPIO_0"
#define BOARD_SPI1_NOR_CS_GPIO			32 /* spi1 nor cs gpio */

#define BOARD_ADCKEY_KEY_MAPPINGS	\
	{KEY_BT,                0x0f},	\
	{KEY_PREVIOUSSONG,      0x40},	\
	{KEY_NEXTSONG,          0x80},	\
	{KEY_COMBO_VOL,         0xDC},	\
	{KEY_MENU,              0x110},	\
	{KEY_TBD,               0x1AE},	\
	{KEY_VOLUMEDOWN,        0x200},	\
	{KEY_VOLUMEUP,          0x2A0},

#define BOARD_BATTERY_CHARGING_STATUS_GPIO	15

#define BOARD_BATTERY_CAP_MAPPINGS      \
	{0, 3200},  \
	{5, 3300},  \
	{10, 3400}, \
	{20, 3550}, \
	{30, 3650}, \
	{40, 3750}, \
	{50, 3800}, \
	{60, 3850}, \
	{70, 3900}, \
	{80, 3950}, \
	{90, 4000}, \
	{100, 4050},


#define LINEIN_DETEC_ADVAL_RANGE \
        760,    \
        830


#define HOLDABLE_KEY {KEY_VOLUMEDOWN, KEY_VOLUMEUP, KEY_NEXTSONG, KEY_PREVIOUSSONG}
#define MUTIPLE_CLIK_KEY {KEY_POWER, KEY_TBD, KEY_NEXTSONG, KEY_PREVIOUSSONG}


#ifdef CONFIG_LED
/* led id, gpio , pwm id, active level*/
typedef struct {
    u8_t led_id;
    u8_t led_pin;
    u8_t led_pwm;
    u8_t active_level;
}led_map;

#ifndef CONFIG_SOC_MAPPING_PSRAM
#ifndef CONFIG_BT_CONTROLER_DEBUG_GPIO
#define BOARD_LED_MAP   \
    /*led_id , led_pin. led_pwm,  active_level    */\
	{   0,          8,          3,                  1},\
	{   1,          9,          4,                  1},\

#ifdef CONFIG_XSPI1_NOR_ACTS
#define BOARD_PWM_MAP   \
    {21,     6},\
    {21,     6},
#else
#define BOARD_PWM_MAP   \
    {8,     6},\
    {9,     6},
#endif
#endif
#endif
#endif /* CONFIG_LED */

#ifdef CONFIG_AUDIO_OUT_SPDIFTX_SUPPORT
	#define BOARD_SPDIFTX_MAP {23,  0x5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(4)},
#else
	#define BOARD_SPDIFTX_MAP
#endif

#if defined(CONFIG_AUDIO_IN_SPDIFRX_SUPPORT)
	#define BOARD_SPDIFRX_MAP { 7,  0x5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(4)},
#else
	#define BOARD_SPDIFRX_MAP
#endif

/*!
 * enum ain_logic_source_type_e
 * @brief Audio input logic devices for middleware and application.
 */
typedef enum {
	AIN_LOGIC_SOURCE_LINEIN = 0,
	AIN_LOGIC_SOURCE_ATT_AUXFD,
	AIN_LOGIC_SOURCE_ATT_AUX0,
	AIN_LOGIC_SOURCE_ATT_AUX1,
	AIN_LOGIC_SOURCE_MIC0,
	AIN_LOGIC_SOURCE_MIC1,
	AIN_LOGIC_SOURCE_FM,
	AIN_LOGIC_SOURCE_DMIC,
	AIN_LOGIC_SOURCE_USB,
} ain_logic_source_type_e;

extern void board_extern_pa_ctl(uint8_t mode);

extern int board_extern_pa_class_select(u8_t pa_class);

extern void board_init_psram_pins(void);

extern void board_mmc_function_reset(int mmc_id, u8_t is_lowpower_mode);

extern int board_audio_device_mapping(uint16_t logical_dev, uint8_t *physical_dev, uint8_t *track_flag);

#endif /* __INC_BOARD_H */
