/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief board init functions
 */

#include <init.h>
#include <gpio.h>
#include <soc.h>
#include "board.h"
#include <device.h>
#include <pwm.h>
#include <audio_common.h>
#include "board_version.h"
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_BOARD

#ifdef CONFIG_BT_CONTROLER_BQB
extern int btdrv_get_bqb_mode(void);
#endif

#ifdef CONFIG_UART_ACTS_PORT_1
#if defined(CONFIG_CODEC_ACM8635)
#define BOARD_UART1_TX_MFP	BOARD_285L2_UART1_TX_MFP
#define BOARD_UART1_RX_MFP	BOARD_285L2_UART1_RX_MFP
#else
#define BOARD_UART1_TX_MFP	0xe
#define BOARD_UART1_RX_MFP	0xe
#endif
#endif

#ifdef CONFIG_SOC_MAPPING_PSRAM
static const struct acts_pin_config board_pin_psram_config[] =
{
	{ 11, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3)},
	{ 10, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3) },
	{ 9, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3) },
	{ BOARD_SPI1_PSRAM_CS_GPIO, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3) },
#if !defined(CONFIG_BT_MUSIC_LED_STRIP2) || \
	(CONFIG_BT_MUSIC_LED_STRIP2_GPIO != 13)
	{13, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3)},	/* so3 */
#endif
#if !defined(CONFIG_BT_MUSIC_LED_STRIP2) || \
	(CONFIG_BT_MUSIC_LED_STRIP2_GPIO != 14)
	{14, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3)},	/* so2 */
#endif

#ifdef CONFIG_XSPI1_NOR_ACTS
	{BOARD_SPI1_NOR_CS_GPIO, 0 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3) | GPIO_CTL_PULLUP}, /* spi1 nor cs, pullup */
#endif
};
#endif
static const struct acts_pin_config board_pin_config[] = {
	/* uart0 — GPIO2 复用为 SPI2_MOSI（UART0 仅 TX 可用） */
	{2, 7 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(0) | GPIO_CTL_PULLUP},	/* UART0_RX（将被下方 SPI1_MOSI 覆盖） */
	{3, 7 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(0) | GPIO_CTL_PULLUP},	/* UART0_TX */

#ifdef CONFIG_UART_ACTS_PORT_1
	{CONFIG_BOARD_UART1_TX_GPIO, BOARD_UART1_TX_MFP | GPIO_CTL_SMIT
	      | GPIO_CTL_GPIO_OUTEN | GPIO_CTL_PADDRV_LEVEL(3) | GPIO_CTL_PULLUP},	/* UART1_TX */
	{CONFIG_BOARD_UART1_RX_GPIO, BOARD_UART1_RX_MFP | GPIO_CTL_SMIT
	      | GPIO_CTL_PADDRV_LEVEL(3) | GPIO_CTL_PULLUP},	/* UART1_RX */
#endif

	/* spi0, nor flash */
#ifdef CONFIG_RUN_IN_SPI0_EXT_NOR
#if !defined(CONFIG_BT_MUSIC_LED_STRIP) || (CONFIG_BT_MUSIC_LED_STRIP_GPIO != 8)
	{ 8, 9 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(1)},	/* SPI0_SS */
#endif
	{ 9, 9 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(1)},	/* SPI0_SCLK */
	{10, 9 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(1)},	/* SPI0_MISO */
	{11, 9 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(1)},	/* SPI0_MOSI */
#endif

#if (CONFIG_XSPI_NOR_ACTS_IO_BUS_WIDTH == 4)
#ifndef CONFIG_RUN_IN_SPI0_EXT_NOR
	{26, 4 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(1)},	/* SPI0_IO2 */
	{27, 4 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(1)},	/* SPI0_IO3 */
#else
	{12, 9 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(1)},	/* SPI0_IO2 */
	{13, 9 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(1)},	/* SPI0_IO3 */
#endif
#endif

#if defined(CONFIG_I2C_SLAVE_ACTS) && !defined(CONFIG_CODEC_ACM8635)
	{0,  	6 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(0) | GPIO_CTL_PULLUP},	/* I2C0_SCL */
	{19,  	6 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(0) | GPIO_CTL_PULLUP},	/* I2C0_SDA */
#endif

#if defined(CONFIG_I2C_0)
#if defined(CONFIG_CODEC_ACM8635)
	{BOARD_285L2_I2C_SCL_GPIO, BOARD_285L2_I2C_SCL_PIN_CTL},	/* TWI0_SCL */
	{BOARD_285L2_I2C_SDA_GPIO, BOARD_285L2_I2C_SDA_PIN_CTL},	/* TWI0_SDA */
#else
	{0,  5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(0) | GPIO_CTL_PULLUP},	/* TWI0_SCL */
	{1,  5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(0) | GPIO_CTL_PULLUP},	/* TWI0_SDA */
#endif
#elif defined(CONFIG_I2C_GPIO_1) && defined(CONFIG_CODEC_ACM8635)
	{CONFIG_I2C_GPIO_1_SCL_PIN, GPIO_CTL_MFP_GPIO | GPIO_CTL_PULLUP | GPIO_CTL_SMIT
	      | GPIO_CTL_PADDRV_LEVEL(3)},	/* ACM8635 soft-I2C SCL */
	{CONFIG_I2C_GPIO_1_SDA_PIN, GPIO_CTL_MFP_GPIO | GPIO_CTL_PULLUP | GPIO_CTL_SMIT
	      | GPIO_CTL_PADDRV_LEVEL(3)},	/* ACM8635 soft-I2C SDA */
#endif

#if defined(CONFIG_BT_MUSIC_LED_STRIP2) && (CONFIG_BT_MUSIC_LED_STRIP2_GPIO == 2)
	{2, 13 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(0)},	/* SPI2_MOSI → WS2812 strip2 */
#endif
#if defined(CONFIG_BT_MUSIC_LED_STRIP2) && (CONFIG_BT_MUSIC_LED_STRIP2_GPIO == 14)
	/* GPIO14 已不再用于 WS2812 */
#endif

	{54, GPIO_CTL_AD_SELECT | GPIO_CTL_GPIO_INEN },	/* AUX2L */
	{55, GPIO_CTL_AD_SELECT | GPIO_CTL_GPIO_INEN },	/* AUX2R */

	/* i2s tx0 -> ACM8635 (285L2: LRCLK=6 BCLK=39 SDIN=38, 3-wire no MCLK) */
#ifdef CONFIG_AUDIO_OUT_I2STX0_SUPPORT
#if !defined(CONFIG_CODEC_ACM8635)
	{40,  0x9 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(4)},  /* mclk */
#endif
	{39,  0x9 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(4)},  /* bclk */
	{6, 0x9 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(4)},  /* lrclk */
	{38,  0x9 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(4)},  /* dout -> codec SDIN */
#endif

	/* i2s rx0 — UART1 TX 已移至 GPIO21，GPIO7 可复用为 I2SRX0 DIN */
#if defined(CONFIG_AUDIO_IN_I2SRX0_SUPPORT)
	{40,  0xa | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(4)},  /* mclk */
	{39,  0xa | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(4)},  /* bclk */
	{6, 0xa | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(4)},  /* lrclk */
	{7,   0xa | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(4)},  /* din */
#endif

#if defined(CONFIG_I2S1_PSEUDO_5WIRE)
    {35,   0xb | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(4)},  /* din */
	/* i2s rx1 */
#elif CONFIG_AUDIO_IN_I2SRX1_SUPPORT
	//BOARD_SPI1_NOR_CS_GPIO use GPIO32
	//{32,  0xb | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(4)},  /* mclk */
	{33,  0xb | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(4)},  /* bclk */
	{34, 0xb | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(4)},  /* lrclk */
	{35,   0xb | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(4)},  /* din */
#endif

	/* aout */
	{50,  GPIO_CTL_AD_SELECT},  /* aoutl/aoutlp */
	{52,  GPIO_CTL_AD_SELECT},  /* aoutr/aoutrp */

	//直驱or  差分需要设置vro & vro_s
#if ((CONFIG_AUDIO_OUT_DD_MODE == 1) || (CONFIG_AUDIO_OUT_SE_DF == 1))
	{51,  GPIO_CTL_AD_SELECT},	/* vro */
	{53,  GPIO_CTL_AD_SELECT},	/* vros */
#endif
	/* external pa (EVB); 285L2+ACM8635: GPIO21=TEST only, no ext PA GPIO */
#if defined(CONFIG_BOARD_EXTERNAL_PA_ENABLE) && !defined(CONFIG_CODEC_ACM8635)
	{21, 0},	 /* external pa ctl1 */
	{5, 0},	 /* external pa ctl2 */
#endif

#if defined(CONFIG_CODEC_ACM8635) && (CONFIG_CODEC_ACM8635_PDN_GPIO == 5)
	{5, GPIO_CTL_MFP_GPIO | GPIO_CTL_GPIO_OUTEN | GPIO_CTL_SMIT
	      | GPIO_CTL_PADDRV_LEVEL(3)},	/* ACM8635 PDN/SDZ */
#endif

	/* DMIC */
#if (CONFIG_MIC0_HW_MAPPING == 4)
	{44, 0x01 | 0}, 	   /* DMIC_CLK */
	{45, 0x01 | 0}, 	   /* DMIC_DAT */
#endif

#if defined(CONFIG_BT_MUSIC_LED_STRIP2) && (CONFIG_BT_MUSIC_LED_STRIP2_GPIO == 14)
	{14, GPIO_CTL_MFP_GPIO | GPIO_CTL_GPIO_OUTEN | GPIO_CTL_SMIT
	      | GPIO_CTL_PADDRV_LEVEL(4)},	/* strip2 WS2812 DO */
#endif

#ifdef CONFIG_XSPI1_NOR_ACTS
	{11, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(2)},
	{10, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(2)},
	{ 9, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(2)},
#if !defined(CONFIG_BT_MUSIC_LED_STRIP) || (CONFIG_BT_MUSIC_LED_STRIP_GPIO != 8)
	{ 8, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(2)},
#endif
#if !defined(CONFIG_BT_MUSIC_LED_STRIP2) || \
	(CONFIG_BT_MUSIC_LED_STRIP2_GPIO != 13)
	{13, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(2)},     /* so3 */
#endif
#if !defined(CONFIG_BT_MUSIC_LED_STRIP2) || \
	(CONFIG_BT_MUSIC_LED_STRIP2_GPIO != 14)
	{14, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(2)},     /* so2 */
#endif
#endif
};

#ifdef CONFIG_MMC_0
static const struct acts_pin_config board_pin_mmc_config[] =
{
	/* sd0 */
	{17, 0xd | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(2)},			/* SD0_CLK */
#ifdef BOARD_SDCARD_USE_INTERNAL_PULLUP
	/* sd0, internal pull-up resistances in SoC */
	{16, 0xd | GPIO_CTL_SMIT | GPIO_CTL_PULLUP | GPIO_CTL_PADDRV_LEVEL(1)}, /* SD0_CMD */
	{20, 0xd | GPIO_CTL_SMIT | GPIO_CTL_PULLUP | GPIO_CTL_PADDRV_LEVEL(1)}, /* SD0_D0 */
#else
	/* sd0, external pull-up resistances on the board */
	{16, 0xd | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(1)},	/* SD0_CMD */
	{20, 0xd | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(1)},	/* SD0_D0 */
#endif
};
#endif

#ifndef CONFIG_RUN_IN_SPI0_EXT_NOR
static const struct acts_pin_config board_pin_snor0_mfp0[] =
{
	{28, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(1)},	/* SPI0_SS */
	{29, 6 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(1)},	/* SPI0_SCLK */
	{30, 6 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(1)},	/* SPI0_MISO */
	{31, 3 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(1)},	/* SPI0_MOSI */
};

static const struct acts_pin_config board_pin_snor0_mfp1[] =
{
	{28, 6 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(1)},	/* SPI0_MOSI */
	{29, 7 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(1)},	/* SPI0_SS */
	{30, 7 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(1)},	/* SPI0_SCLK */
	{31, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(1)},	/* SPI0_MISO */
};
#endif
#ifdef CONFIG_BT_CONTROLER_BLE_BQB
#if (CONFIG_BT_BQB_UART_PORT == 1) && !defined(CONFIG_SYSTEM_APP_PY32_UART)
static const struct acts_pin_config board_pin_config_bqb[] = {
	{CONFIG_BOARD_UART1_TX_GPIO, BOARD_UART1_TX_MFP | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3) | GPIO_CTL_PULLUP},	/* UART1_TX */
	{CONFIG_BOARD_UART1_RX_GPIO, BOARD_UART1_RX_MFP | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3) | GPIO_CTL_PULLUP},	/* UART1_RX */
};
#endif
#endif

static const audio_input_map_t board_audio_input_map[] =  {
	{AIN_LOGIC_SOURCE_LINEIN, AIN_SOURCE_AUXFD, INPUTSRC_L_R},
	{AIN_LOGIC_SOURCE_ATT_AUXFD, AIN_SOURCE_AUXFD, INPUTSRC_L_R},
	{AIN_LOGIC_SOURCE_ATT_AUX0, AIN_SOURCE_AUX0, INPUTSRC_L_R},
	{AIN_LOGIC_SOURCE_ATT_AUX1, AIN_SOURCE_AUX1, INPUTSRC_L_R},
	{AIN_LOGIC_SOURCE_MIC0, AIN_SOURCE_ASEMIC_AUX2, INPUTSRC_L_R},
	{AIN_LOGIC_SOURCE_MIC1, AIN_SOURCE_ASEMIC, INPUTSRC_ONLY_L},
	{AIN_LOGIC_SOURCE_FM, AIN_SOURCE_AUX2, INPUTSRC_L_R},
	{AIN_LOGIC_SOURCE_DMIC, AIN_SOURCE_DMIC, INPUTSRC_ONLY_L},
};

int board_audio_device_mapping(uint16_t logical_dev, uint8_t *physical_dev, uint8_t *track_flag)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(board_audio_input_map); i++) {
		if (logical_dev == board_audio_input_map[i].logical_dev) {
			*physical_dev = board_audio_input_map[i].physical_dev;
			*track_flag = board_audio_input_map[i].track_flag;
			break;
		}
	}

	if (i == ARRAY_SIZE(board_audio_input_map)) {
		printk("can not find out audio dev %d\n", logical_dev);
		return -ENOENT;
	}

	return 0;
}

#ifdef BOARD_SDCARD_POWER_EN_GPIO

#define SD_CARD_POWER_RESET_MS	80

#define SD0_CMD_PIN		16
#define SD0_D0_PIN		20
#define SD0_CLK_PIN     17

static int pinmux_sd0_cmd, pinmux_sd0_d0, pinmux_sd0_clk;
static int sd_card_reset_ms;

static void board_mmc0_pullup_disable(void)
{
	struct device *sd_gpio_dev;

	sd_gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	if (!sd_gpio_dev)
		return;

	/* backup origin pinmux config */
	acts_pinmux_get(SD0_CMD_PIN, &pinmux_sd0_cmd);
	acts_pinmux_get(SD0_D0_PIN, &pinmux_sd0_d0);
	acts_pinmux_get(SD0_CLK_PIN, &pinmux_sd0_clk);

	/* sd_cmd pin output low level to avoid leakage */
	gpio_pin_configure(sd_gpio_dev, SD0_CMD_PIN, GPIO_DIR_OUT);
	gpio_pin_write(sd_gpio_dev, SD0_CMD_PIN, 0);

	/* sd_d0 pin output low level to avoid leakage */
	gpio_pin_configure(sd_gpio_dev, SD0_D0_PIN, GPIO_DIR_OUT);
	gpio_pin_write(sd_gpio_dev, SD0_D0_PIN, 0);

	/* sd_clk pin output low level to avoid leakage */
	gpio_pin_configure(sd_gpio_dev, SD0_CLK_PIN, GPIO_DIR_OUT);
	gpio_pin_write(sd_gpio_dev, SD0_CLK_PIN, 0);
}

static void board_mmc0_pullup_enable(void)
{
	/* restore origin pullup pinmux config */
	acts_pinmux_set(SD0_CMD_PIN, pinmux_sd0_cmd);
	acts_pinmux_set(SD0_D0_PIN, pinmux_sd0_d0);
	acts_pinmux_set(SD0_CLK_PIN, pinmux_sd0_clk);
}

static int board_mmc_power_gpio_reset(struct device *power_gpio_dev, int power_gpio)
{
	gpio_pin_configure(power_gpio_dev, power_gpio,
			   GPIO_DIR_OUT);

	/* 0: power on, 1: power off */
	/* card vcc power off */
	gpio_pin_write(power_gpio_dev, power_gpio, 1);

	/* disable mmc0 pull-up to avoid leakage */
	board_mmc0_pullup_disable();

	k_sleep(sd_card_reset_ms);

	/* card vcc power on */
	gpio_pin_write(power_gpio_dev, power_gpio, 0);

	k_sleep(10);

	/* restore mmc0 pull-up */
	board_mmc0_pullup_enable();

	return 0;
}
#endif	/* BOARD_SDCARD_POWER_EN_GPIO */

int board_mmc_power_reset(int mmc_id, u8_t cnt)
{
#ifdef BOARD_SDCARD_POWER_EN_GPIO

	struct device *power_gpio_dev;

	if (mmc_id != 0)
		return 0;

	power_gpio_dev = device_get_binding(BOARD_SDCARD_POWER_EN_GPIO_NAME);
	if (!power_gpio_dev)
		return -EINVAL;

	sd_card_reset_ms = cnt * SD_CARD_POWER_RESET_MS;
	if (sd_card_reset_ms <= 0)
		sd_card_reset_ms = SD_CARD_POWER_RESET_MS;

	board_mmc_power_gpio_reset(power_gpio_dev, BOARD_SDCARD_POWER_EN_GPIO);

#if defined(BOARD_SDCARD_DETECT_GPIO) && (BOARD_SDCARD_DETECT_GPIO == BOARD_SDCARD_POWER_EN_GPIO)
	/* switch gpio function to input for detecting card plugin */
	gpio_pin_configure(power_gpio_dev, BOARD_SDCARD_DETECT_GPIO, GPIO_DIR_IN);
#endif

#endif	/* BOARD_SDCARD_POWER_EN_GPIO */

	return 0;
}

void board_mmc_function_reset(int mmc_id, u8_t is_lowpower_mode)
{
	if(mmc_id == 0){
#ifdef CONFIG_MMC_0
		//disable sdmmc0 cmd&data for reduce power resume
		if(is_lowpower_mode){
			u32_t i;
			for(i = 0; i < ARRAY_SIZE(board_pin_mmc_config); i++){
				sys_write32(0x1000, GPIO_CTL(board_pin_mmc_config[i].pin_num));
			}
		}else{
			acts_pinmux_setup_pins(board_pin_mmc_config, ARRAY_SIZE(board_pin_mmc_config));
		}
#endif
	}
}

/*
 * 外部 PA 的 GPIO 脚位。
 * 285L2+ACM8635 时 GPIO21 专用于 UART1 TX，不可作 PA CTL1。
 */
#if !defined(CONFIG_CODEC_ACM8635)
#define EXTERN_PA_CTL1_PIN  21
#endif
#if !(defined(CONFIG_CODEC_ACM8635) && (CONFIG_CODEC_ACM8635_PDN_GPIO == 5))
#define EXTERN_PA_CTL2_PIN  5
#endif

int board_extern_pa_class_select(u8_t pa_class)
{
	struct device *pa_gpio_dev;

	pa_gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	if (!pa_gpio_dev)
		return -1;

	if (pa_class == EXT_PA_CLASS_AB) {
		SYS_LOG_INF("open external PA class AB");
#ifdef EXTERN_PA_CTL1_PIN
		gpio_pin_configure(pa_gpio_dev, EXTERN_PA_CTL1_PIN, GPIO_DIR_OUT);
		gpio_pin_write(pa_gpio_dev, EXTERN_PA_CTL1_PIN, 1);
#endif
#ifdef EXTERN_PA_CTL2_PIN
		gpio_pin_configure(pa_gpio_dev, EXTERN_PA_CTL2_PIN, GPIO_DIR_OUT);
		gpio_pin_write(pa_gpio_dev, EXTERN_PA_CTL2_PIN, 0);
#endif
	} else if (pa_class == EXT_PA_CLASS_D) {
		SYS_LOG_INF("open external PA class D");
#ifdef EXTERN_PA_CTL1_PIN
		gpio_pin_configure(pa_gpio_dev, EXTERN_PA_CTL1_PIN, GPIO_DIR_OUT);
		gpio_pin_write(pa_gpio_dev, EXTERN_PA_CTL1_PIN, 1);
#endif
#ifdef EXTERN_PA_CTL2_PIN
		gpio_pin_configure(pa_gpio_dev, EXTERN_PA_CTL2_PIN, GPIO_DIR_OUT);
		gpio_pin_write(pa_gpio_dev, EXTERN_PA_CTL2_PIN, 1);
#endif
	} else {
		SYS_LOG_ERR("invalid pa class:%d", pa_class);
		return -EINVAL;
	}

	return 0;
}

void board_extern_pa_ctl(uint8_t mode)
{
#ifdef CONFIG_BT_CONTROLER_BQB
	if (btdrv_get_bqb_mode() == 0) {
#endif
		struct device *pa_gpio_dev;

		pa_gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
		if (!pa_gpio_dev)
			return;

		if(mode == 0) {
			SYS_LOG_INF("close external PA");
#ifdef EXTERN_PA_CTL1_PIN
			gpio_pin_configure(pa_gpio_dev, EXTERN_PA_CTL1_PIN, GPIO_DIR_OUT);
			gpio_pin_write(pa_gpio_dev, EXTERN_PA_CTL1_PIN, 0);
#endif
#ifdef EXTERN_PA_CTL2_PIN
			gpio_pin_configure(pa_gpio_dev, EXTERN_PA_CTL2_PIN, GPIO_DIR_OUT);
			gpio_pin_write(pa_gpio_dev, EXTERN_PA_CTL2_PIN, 0);
#endif

		} else if(mode == 1) {
#if (CONFIG_EXTERN_PA_CLASS == 0)
			board_extern_pa_class_select(EXT_PA_CLASS_AB);
#else
			board_extern_pa_class_select(EXT_PA_CLASS_D);
#endif
		} else {
			SYS_LOG_ERR("invalid PA working mode:%d", mode);
		}
#ifdef CONFIG_BT_CONTROLER_BQB
	}
#endif
}
#ifdef CONFIG_SOC_MAPPING_PSRAM
void board_init_psram_pins(void)
{
	acts_pinmux_setup_pins(board_pin_psram_config, ARRAY_SIZE(board_pin_psram_config));
#ifdef CONFIG_XSPI1_NOR_ACTS
    sys_write32(sys_read32(0xc00900A4) | (1 << 6), 0xc00900A4);
	sys_write32(1 << (40 - 32), 0xc009010C);
#endif
}
#endif

static int board_keyadc_init(void)
{
	u32_t value = 0;
	u32_t pmuadc_ctl_val;

 	/* LRADC1: wio0  */
 	value = sys_read32(WIO0_CTL);
 	value = (value & (~(0x0000000F))) | (1 << 3);
 	sys_write32(value, WIO0_CTL);
	k_busy_wait(200);
	/* TODO: add lradc1 init */

	acts_clock_peripheral_enable(CLOCK_ID_LRADC); /*default device clock enable*/
	pmuadc_ctl_val = sys_read32(PMUADC_CTL);
	if (!(pmuadc_ctl_val & BIT(CONFIG_INPUT_DEV_ACTS_ADCKEY_ADC_CHAN))) {
		/* enable adc channel if channel is not enabled */
		pmuadc_ctl_val |= BIT(CONFIG_INPUT_DEV_ACTS_ADCKEY_ADC_CHAN);
		sys_write32(pmuadc_ctl_val, PMUADC_CTL);
		k_busy_wait(1000);
	}

	return 0;
}

#define EN_CARD_PRODUCT_KEY

#define REBOOT_TYPE_RTC_BAK_NUM        2

#ifdef EN_CARD_PRODUCT_KEY
void check_card_product_key(void)
{
    int i;
    int adc_val;
    int type;

	/* if enter card boot and return to check_card_product_key(void) function, means card boot failed, we must clear flag */
    type = soc_pm_rtc_bak_read(REBOOT_TYPE_RTC_BAK_NUM);
    if(((type>>16) == REBOOT_REASON_MAGIC)
        && (type & 0xffff) == REBOOT_TYPE_GOTO_CARDBOOT){
        soc_pm_rtc_bak_write(0, REBOOT_TYPE_RTC_BAK_NUM);
		return;
	}

    for (i = 0; i < 3; i++) {
        adc_val = sys_read32(ADC_CHAN_DATA_REG(PMUADC_CTL, CONFIG_INPUT_DEV_ACTS_ADCKEY_ADC_CHAN));
        printk("adc_val %d \n", adc_val);
        if (adc_val > 10) {
            break;
        } else {
            k_busy_wait(10);
        }
    }

    if (i >= 3) {
        printk("%s: launch card\n", __func__);

        soc_pm_reboot(REBOOT_TYPE_GOTO_CARDBOOT, REBOOT_TYPE_RTC_BAK_NUM);
    }
}
#endif

int rom_launch_card_boot(void);

void check_card_product(void)
{
#ifdef EN_CARD_PRODUCT_KEY
    int type;

    type = soc_pm_rtc_bak_read(REBOOT_TYPE_RTC_BAK_NUM);
    if((type>>16 == REBOOT_REASON_MAGIC)
        && (type & 0xffff) == REBOOT_TYPE_GOTO_CARDBOOT){

        //printk("enter cardboot\n");
        rom_launch_card_boot();
    }
#endif
}
#if defined(CONFIG_CODEC_ACM8635) && (CONFIG_CODEC_ACM8635_PDN_GPIO >= 0)
static void board_acm8635_pdn_init(void)
{
	u32_t pin = (u32_t)CONFIG_CODEC_ACM8635_PDN_GPIO;

	/* Boot once: drive PDN to run; ACM8635 driver does not touch it later. */
	sys_write32(GPIO_CTL_MFP_GPIO | GPIO_CTL_GPIO_OUTEN | GPIO_CTL_SMIT |
		    GPIO_CTL_PADDRV_LEVEL(3), GPIO_CTL(pin));
#if defined(CONFIG_CODEC_ACM8635_PDN_ACTIVE_LOW) && !CONFIG_CODEC_ACM8635_PDN_ACTIVE_LOW
	sys_write32(GPIO_BIT(pin), GPIO_REG_BRR(GPIO_REG_BASE, pin));
#else
	sys_write32(GPIO_BIT(pin), GPIO_REG_BSR(GPIO_REG_BASE, pin));
#endif
}
#endif

static int board_early_init(struct device *arg)
{
	ARG_UNUSED(arg);

	int value = 0;

	/* LRADC1: wio0  */
	value = sys_read32(WIO0_CTL);
	value = (value & (~(0x0000000F))) | (1 << 3);
	sys_write32(value, WIO0_CTL);

    board_keyadc_init();

#ifdef EN_CARD_PRODUCT_KEY
    check_card_product_key();
#endif
	/* bandgap has filter resistor  */
	value = sys_read32(BDG_CTL);
	value = (value | (1 << 6));
	sys_write32(value, BDG_CTL);

#ifdef CONFIG_PWM_ACTS
	//acts_pinmux_setup_pins(board_led_pin_config, ARRAY_SIZE(board_led_pin_config));
#endif

	acts_pinmux_setup_pins(board_pin_config, ARRAY_SIZE(board_pin_config));

#ifdef CONFIG_UART_ACTS_PORT_1
	/*
	 * acts_pinmux_set 只改 PINMUX_MODE_MASK 内的位，不会清除
	 * GPIO_CTL_GPIO_OUTEN。若 bootloader 曾将 GPIO 输出脚设为
	 * 输出（OUTEN=1），残留的 OUTEN 会阻止 UART 外设驱动引脚。
	 * 此处整字写 GPIO_CTL 确保 OUTEN 被清除。
	 */
	sys_write32((BOARD_UART1_TX_MFP) | GPIO_CTL_SMIT
		    | GPIO_CTL_PADDRV_LEVEL(3) | GPIO_CTL_PULLUP,
		    GPIO_CTL(CONFIG_BOARD_UART1_TX_GPIO));
#endif

#if defined(CONFIG_CODEC_ACM8635) && (CONFIG_CODEC_ACM8635_PDN_GPIO >= 0)
	/* PDN high at board early init; driver leaves it alone */
	board_acm8635_pdn_init();
#endif

#ifndef CONFIG_RUN_IN_SPI0_EXT_NOR
	//used as SPI_SS, default usage
	if((sys_read32(GPIO_CTL(28)) & 0xf) == 5){
		acts_pinmux_setup_pins(board_pin_snor0_mfp0, ARRAY_SIZE(board_pin_snor0_mfp0));
	}else{
		acts_pinmux_setup_pins(board_pin_snor0_mfp1, ARRAY_SIZE(board_pin_snor0_mfp1));
	}
#endif

#ifdef CONFIG_MMC_0
	acts_pinmux_setup_pins(board_pin_mmc_config, ARRAY_SIZE(board_pin_mmc_config));
#endif

	return 0;
}

void board_jtag_init(void)
{
	check_card_product();

#ifdef CONFIG_CPU0_EJTAG_ENABLE
	soc_debug_enable_jtag(SOC_JTAG_CPU0, CONFIG_CPU0_EJTAG_GROUP);
#else
	soc_debug_disable_jtag(SOC_JTAG_CPU0);
#endif

#ifdef CONFIG_DSP_EJTAG_ENABLE
	soc_debug_enable_jtag(SOC_JTAG_DSP, CONFIG_DSP_EJTAG_GROUP);
#else
	soc_debug_disable_jtag(SOC_JTAG_DSP);
#endif
}

static int board_later_init(struct device *arg)
{
	ARG_UNUSED(arg);

	printk("%s %d: \n", __func__, __LINE__);

#ifdef CONFIG_RUN_IN_SPI0_EXT_NOR
	printk("running in spi0 external nor flash\n");
#else
	printk("running in spi0 inner nor flash\n");
#endif
#ifdef CONFIG_ACTIONS_IMG_LOAD
    extern int run_test_image(void);
    run_test_image();
#endif

#ifdef CONFIG_BT_CONTROLER_BLE_BQB
	if (btdrv_get_bqb_mode() == 2) {
#if (CONFIG_BT_BQB_UART_PORT == 1) && !defined(CONFIG_SYSTEM_APP_PY32_UART)
		acts_pinmux_setup_pins(board_pin_config_bqb,
				       ARRAY_SIZE(board_pin_config_bqb));
#endif
	}
#endif

	return 0;
}

uint32_t libboard_version_get(void)
{
    return LIBBOARD_VERSION_NUMBER;
}


SYS_INIT(board_early_init, PRE_KERNEL_1, 5);

SYS_INIT(board_later_init, POST_KERNEL, 5);
