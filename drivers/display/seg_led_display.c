/*
 * Copyright (c) 2020 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <kernel.h>
#include <device.h>
#include <string.h>
#include <errno.h>
#include <soc.h>
#include <misc/util.h>
#include <logging/sys_log.h>
#include <display/seg_led_display.h>
#include <soc_pmu.h>

#define SEG_DISP_CTL_LCD_POWER                                            (31)
#if defined(CONFIG_SOC_SERIES_WOODPECKER) || defined(CONFIG_SOC_SERIES_WOODPECKERFPGA)
#define SEG_DISP_CTL_MODE_SEL_SHIFT                                       (16)
#define SEG_DISP_CTL_MODE_SEL_MASK                                        (0x1F << SEG_DISP_CTL_MODE_SEL_SHIFT)
#else
#define SEG_DISP_CTL_MODE_SEL_SHIFT                                       (0)
#define SEG_DISP_CTL_MODE_SEL_MASK                                        (0xF << SEG_DISP_CTL_MODE_SEL_SHIFT)
#endif
#define SEG_DISP_CTL_MODE_SEL(x)                                          ((x) << SEG_DISP_CTL_MODE_SEL_SHIFT)
#define SEG_DISP_CTL_LED_COM_DZ_SHIFT                                     (8)
#define SEG_DISP_CTL_LED_COM_DZ_MASK                                      (0x7 << SEG_DISP_CTL_LED_COM_DZ_SHIFT)
#define SEG_DISP_CTL_SEGOFF                                               (7)
#define SEG_DISP_CTL_LCD_OUT_EN                                           (5)
#define SEG_DISP_CTL_REFRSH                                               (4)

#define SEG_BIAS_EN_LED_SEG_ALL_EN                                        (4)
#define SEG_BIAS_EN_LED_CATHODE_ANODE_MODE                                (3)
#define SEG_BIAS_EN_LED_SEG_BIAS_SHIFT                                    (0)
#define SEG_BIAS_EN_LED_SEG_BIAS(x)                                       ((x) << SEG_BIAS_EN_LED_SEG_BIAS_SHIFT)

#if defined(CONFIG_SOC_SERIES_ANDES) || defined(CONFIG_SOC_SERIES_ANDESM)
#define CMU_SEGLCDCLK_SRC	                                              (4)
#define CMU_SEGLCDCLK_POST                                                (3)
#elif defined(CONFIG_SOC_SERIES_WOODPECKER) || defined(CONFIG_SOC_SERIES_WOODPECKERFPGA)
#define CMU_SEGLCDCLK_SRC	                                              (8)
#define CMU_SEGLCDCLK_POST                                                (4)
#endif
#define CMU_SEGLCDCLK_CLKDIV0_SHIFT                                       (0)
#define CMU_SEGLCDCLK_CLKDIV0_MASK	                                      (0x7 << CMU_SEGLCDCLK_CLKDIV0_SHIFT)
#define CMU_SEGLCDCLK_CLKDIV0(x)                                          ((x) << CMU_SEGLCDCLK_CLKDIV0_SHIFT)

#define MULTI_USED_SEG_LED_EN	                                          (0)
#define VOUT_CTL0_SEG_LED_EN                                              (23)

#define SEG_LED_REFRSH_TIMEOUT_MS                                         (1000)

#define SEG_LED_COMPIN_INVALID                                            (0xFF)

/* seg led hardware controller */
struct acts_seg_led_controller {
	volatile u32_t ctl;
	volatile u32_t data0;
	volatile u32_t data1;
	volatile u32_t data2;
	volatile u32_t data3;
	volatile u32_t data4;
	volatile u32_t data5;
	volatile u32_t rc;
	volatile u32_t bias;
};

struct acts_seg_led_data {
	struct k_mutex mutex;
	u8_t cur_disval[16];
	struct led_seg_map *seg_map;
	u8_t led_ready_flag : 1;
};

struct acts_seg_led_config {
	struct acts_seg_led_controller *base;
	u32_t clk_reg;
	u8_t clock_id;
	u8_t reset_id;
};

#if defined(CONFIG_LED_USE_DEFAULT_MAP)
/* icons mapping default table */
static const struct led_com_map icon_map_default[] =
{
	{ 0x00, 0x05 }, //k1 5a PLAY
	{ 0x02, 0x05 }, //k2 5b PAUSE
	{ 0x05, 0x01 }, //k3 5c USB
	{ 0x00, 0x04 }, //k4 5d SD
	{ 0x02, 0x03 }, //k5 5e COL
	{ 0x06, 0x02 }, //k6 5f AUX
	{ 0x02, 0x06 }, //k7 5g FM
	{ 0x06, 0x04 }  //k8 5h DOT
};

/* digital number mapping default table */
static const struct led_com_map num_map_default[][7] =
{
	{ /* 1th digital number mapping */
		{ 0x00, 0x01 }, //1a
		{ 0x00, 0x02 }, //lb
		{ 0x03, 0x00 }, //lc
		{ 0x04, 0x00 }, //ld
		{ 0x00, 0x03 }, //le
		{ 0x01, 0x00 }, //lf
		{ 0x02, 0x00 }  //lg
	},
	{ /* 2nd digital number mapping */
		{ 0x01, 0x02 }, //2a
		{ 0x01, 0x03 }, //2b
		{ 0x04, 0x01 }, //2c
		{ 0x01, 0x05 }, //2d
		{ 0x01, 0x04 }, //2e
		{ 0x02, 0x01 }, //2f
		{ 0x03, 0x01 }  //2g
	},
	{ /* 3th digital number mapping */
		{ 0x04, 0x03 }, //3a
		{ 0x02, 0x04 }, //3b
		{ 0x03, 0x04 }, //3c
		{ 0x05, 0x00 }, //3d
		{ 0x05, 0x02 }, //3e
		{ 0x03, 0x02 }, //3f
		{ 0x04, 0x02 }  //3g
	},
	{ /* 4th digital number mapping */
		{ 0x06, 0x05 }, //4a
		{ 0x05, 0x06 }, //4b
		{ 0x04, 0x05 }, //4c
		{ 0x05, 0x03 }, //4d
		{ 0x03, 0x05 }, //4e
		{ 0x05, 0x04 }, //4f
		{ 0x04, 0x06 }  //4g
	}
};
#elif defined(CONFIG_LED_USE_1P2INCH_MAP)
/* icons mapping 1.2 inch table */
static const struct led_com_map icon_map_default[] =
{
	{ 0x00, 0x00 }, //1a PM
	{ 0x05, 0x00 }, //1f MOON
	{ 0x00, 0x04 }, //k1 ALARM1
	{ 0x01, 0x04 }, //k2 ALARM2
	{ 0x02, 0x04 }, //k3 BT
	{ 0x03, 0x04 }, //k4 FM
	{ 0x05, 0x04 }, //k6 SD
	{ 0x06, 0x04 }, //k7 TONE
	{ 0x04, 0x04 }, //k5 COL
	{ 0x06, 0x00 }, //1g DOT
	{ 0x00, 0x05 }, //k8 AUX
	{ 0X01, 0x05 }, //k9 WIFI
};

/* digital number mapping 1.2inch table */
static const struct led_com_map num_map_default[][7] =
{
	{ /* 1th digital number mapping */
		{SEG_LED_COMPIN_INVALID, SEG_LED_COMPIN_INVALID}, // NA
		{ 0x01, 0x00 }, //1b
		{ 0x02, 0x00 }, //lc
		{SEG_LED_COMPIN_INVALID, SEG_LED_COMPIN_INVALID},
		{SEG_LED_COMPIN_INVALID, SEG_LED_COMPIN_INVALID},
		{SEG_LED_COMPIN_INVALID, SEG_LED_COMPIN_INVALID},
		{SEG_LED_COMPIN_INVALID, SEG_LED_COMPIN_INVALID}
	},
	{ /* 2nd digital number mapping */
		{ 0x00, 0x01 }, //2a
		{ 0x01, 0x01 }, //2b
		{ 0x02, 0x01 }, //2c
		{ 0x03, 0x01 }, //2d
		{ 0x04, 0x01 }, //2e
		{ 0x05, 0x01 }, //2f
		{ 0x06, 0x01 }  //2g
	},
	{ /* 3th digital number mapping */
		{ 0x00, 0x02 }, //3a
		{ 0x01, 0x02 }, //3b
		{ 0x02, 0x02 }, //3c
		{ 0x03, 0x02 }, //3d
		{ 0x04, 0x02 }, //3e
		{ 0x05, 0x02 }, //3f
		{ 0x06, 0x02 }  //3g
	},
	{ /* 4th digital number mapping */
		{ 0x00, 0x03 }, //4a
		{ 0x01, 0x03 }, //4b
		{ 0x02, 0x03 }, //4c
		{ 0x03, 0x03 }, //4d
		{ 0x04, 0x03 }, //4e
		{ 0x05, 0x03 }, //4f
		{ 0x06, 0x03 }  //4g
	}
};
#endif

/* The common bitmap table */
static const struct led_bitmap bitmap_table[] = {
	{'0', 0x3f}, {'1', 0x06}, {'2', 0x5b}, {'3', 0x4f},
	{'4', 0x66}, {'5', 0x6d}, {'6', 0x7d}, {'7', 0x07},
	{'8', 0x7f}, {'9', 0x6f}, {'a', 0x5f}, {'A', 0x77},
	{'b', 0x7c}, {'B', 0x7f}, {'c', 0x39}, {'C', 0x39},
	{'d', 0x5e}, {'D', 0x5e}, {'e', 0x79}, {'E', 0x79},
	{'f', 0x71}, {'F', 0x71}, {'g', 0x6f}, {'G', 0x7d},
	{'h', 0x76}, {'H', 0x76}, {'i', 0x04}, {'I', 0x06},
	{'j', 0x0e}, {'J', 0x0f}, {'l', 0x38}, {'L', 0x38},
	{'n', 0x37}, {'N', 0x37}, {'o', 0x5c}, {'O', 0x3f},
	{'p', 0x73}, {'P', 0x73}, {'q', 0x67}, {'Q', 0x67},
	{'r', 0x50}, {'R', 0x72}, {'s', 0x6d}, {'S', 0x6d},
	{'t', 0x78}, {'T', 0x78}, {'u', 0x3e}, {'U', 0x3e},
	{'y', 0x6e}, {'Y', 0x6e}, {'z', 0x5b}, {'Z', 0x5b},
	{':', 0x80}, {'-', 0x40}, {'_', 0x08}, {' ', 0x00},
};

static u8_t get_bitmap(u8_t c, const struct led_bitmap *table, u32_t table_len)
{
	u8_t bitmap = 0;
	u32_t i = 0;

	for (i = 0; i < table_len; i++) {
		if (table[i].character == c) {
			bitmap = (u8_t)table[i].bitmap;
			break;
		}
	}

	if (i == table_len)
		SYS_LOG_ERR("Character %c not found", c);

#if defined(CONFIG_LED_COMMON_ANODE)
	bitmap = ~bitmap;
#endif

	return bitmap;
}

static void dump_regs(struct acts_seg_led_controller *base)
{
	SYS_LOG_INF("** seg_led contoller register **");
	SYS_LOG_INF("           BASE: %08x", (u32_t)base);
	SYS_LOG_INF("   SEG_DISP_CTL: %08x", base->ctl);
	SYS_LOG_INF(" SEG_DISP_DATA0: %08x", base->data0);
	SYS_LOG_INF(" SEG_DISP_DATA1: %08x", base->data1);
	SYS_LOG_INF("      SEG_RC_EN: %08x", base->rc);
	SYS_LOG_INF("    SEG_BIAS_EN: %08x", base->bias);
#if defined(CONFIG_SOC_SERIES_ANDES) || defined(CONFIG_SOC_SERIES_ANDESM)
	SYS_LOG_INF("  CMU_SEGLCDCLK: %08x", sys_read32(CMU_SEGLCDCLK));
#else
	SYS_LOG_INF("  CMU_SEGLCDCLK: %08x", sys_read32(CMU_SEGLEDCLK));
#endif
}

static void __seg_led_phy_exit(struct device *dev)
{
	const struct acts_seg_led_config *cfg = dev->config->config_info;
	struct acts_seg_led_controller *base = cfg->base;
	struct acts_seg_led_data *data = dev->driver_data;

	k_mutex_lock(&data->mutex, K_FOREVER);

	/* segoff and disable LCD_OUT_EN  */
	base->ctl &= ~((1 << SEG_DISP_CTL_LCD_OUT_EN) | (1 << SEG_DISP_CTL_SEGOFF));
	/* disable seg led controller clock */
	acts_clock_peripheral_disable(cfg->clock_id);
	/* assert seg led controller */
	acts_reset_peripheral_assert(cfg->reset_id);

	data->led_ready_flag = false;

	k_mutex_unlock(&data->mutex);
}

static void seg_led_init_clksource(struct device *dev)
{
	const struct acts_seg_led_config *cfg = dev->config->config_info;
	/* choose the HOSC as the clock source */
	sys_write32((sys_read32(cfg->clk_reg) | (1 << CMU_SEGLCDCLK_SRC)), cfg->clk_reg);
	/* seg led clk = HOSC / 512 / 2  */
	sys_write32((sys_read32(cfg->clk_reg) & (~CMU_SEGLCDCLK_CLKDIV0_MASK))
		| (0x01 << CMU_SEGLCDCLK_POST) | CMU_SEGLCDCLK_CLKDIV0(1), cfg->clk_reg);
}

static int seg_led_enable(struct device *dev)
{
	const struct acts_seg_led_config *cfg = dev->config->config_info;
	struct acts_seg_led_data *data = dev->driver_data;
	struct acts_seg_led_controller *base = cfg->base;
	u32_t reg;
	u8_t com = 0;

#if defined(CONFIG_SOC_SERIES_ANDES) || defined(CONFIG_SOC_SERIES_ANDESM)
#if defined(CONFIG_LED_DIGIT_TYPE) && ((CONFIG_LED_COM_NUM != 4) && (CONFIG_LED_COM_NUM != 8))
#error "Digital SEG LED only support 4COM and 8COM"
#endif
#endif

#if defined(CONFIG_LED_MATRIX_TYPE) && ((CONFIG_LED_COM_NUM != 7) && (CONFIG_LED_COM_NUM != 8))
#error "Matrix SEG LED only support 7COM and 8COM"
#endif

	/* configure the dead zone width */
	reg = CONFIG_LED_DZ_VAL << SEG_DISP_CTL_LED_COM_DZ_SHIFT;
	/* segment value is according to LCD_DATA */
	reg |= 1 << SEG_DISP_CTL_SEGOFF;
	/* The pads of seg led output signals as their timing */
	reg |= 1 << SEG_DISP_CTL_LCD_OUT_EN;

#if defined(CONFIG_SOC_SERIES_ANDES) || defined(CONFIG_SOC_SERIES_ANDESM)
#if (4 == CONFIG_LED_COM_NUM)
	com = 0x8;
#elif (7 == CONFIG_LED_COM_NUM)
	com = 0xC;
#elif (8 == CONFIG_LED_COM_NUM)
#if defined(CONFIG_LED_MATRIX_TYPE)
	com = 0xE;
#else
	com = 0xA;
#endif
#endif
#elif defined(CONFIG_SOC_SERIES_WOODPECKER) || defined(CONFIG_SOC_SERIES_WOODPECKERFPGA)
#if defined(CONFIG_LED_MATRIX_TYPE)
#if (7 == CONFIG_LED_COM_NUM)
	com = 0x12;
#else
	com = 0x14;
#endif
#else
	com = CONFIG_LED_COM_NUM * 2;
#endif
#endif
#if defined(CONFIG_SEG_LCD_TYPE)
com = 0x3;
#endif
#ifdef CONFIG_LED_COMMON_ANODE
	com += 1;
	base->bias = (1 << SEG_BIAS_EN_LED_SEG_ALL_EN)
					| (1 << SEG_BIAS_EN_LED_CATHODE_ANODE_MODE)
					| SEG_BIAS_EN_LED_SEG_BIAS(CONFIG_LED_BIAS_CURRENT);
#else
#ifndef CONFIG_SEG_LCD_TYPE
	base->bias = (1 << SEG_BIAS_EN_LED_SEG_ALL_EN)
				| SEG_BIAS_EN_LED_SEG_BIAS(CONFIG_LED_BIAS_CURRENT);
#else
	//sys_write32(sys_read32(0xc0020000)|(3<<24),0xc0020000);//PMU
	soc_pmu_set_seg_res_sel(1);
	soc_pmu_set_seg_dip_vcc_en(1);
	base->bias = (1 << 1);
#endif

#endif

	reg |= SEG_DISP_CTL_MODE_SEL(com);

	base->ctl = reg;

	SYS_LOG_INF("SEG-LED CTL:%08x", base->ctl);

	/* Enable seg0~seg7 restrict current */
#ifndef CONFIG_SEG_LCD_TYPE
	base->rc = 0xFF;
#endif

#if defined(CONFIG_SOC_SERIES_ANDES) || defined(CONFIG_SOC_SERIES_ANDESM)
	sys_write32((sys_read32(MULTI_USED) | (1 << MULTI_USED_SEG_LED_EN)), MULTI_USED);
#elif defined(CONFIG_SOC_SERIES_WOODPECKER) || defined(CONFIG_SOC_SERIES_WOODPECKERFPGA)
	//sys_write32((sys_read32(VOUT_CTL0) | (1 << VOUT_CTL0_SEG_LED_EN)), VOUT_CTL0);
	soc_pmu_set_seg_led_en(1);
#endif

	data->led_ready_flag = true;

	return 0;
}

static int seg_led_hw_init(struct device *dev)
{
	const struct acts_seg_led_config *cfg = dev->config->config_info;

#if defined(CONFIG_SOC_SERIES_ANDES) || defined(CONFIG_SOC_SERIES_ANDESM)

#if defined(CONFIG_CPU0_EJTAG_ENABLE) && (CONFIG_CPU0_EJTAG_GROUP == 2)
#error "JTAG pins and SEG-LED pins are conflict!"
#endif

#endif

#if defined(CONFIG_SOC_SERIES_WOODPECKER) || defined(CONFIG_SOC_SERIES_WOODPECKERFPGA)

#if defined(CONFIG_CPU0_EJTAG_ENABLE) && (CONFIG_CPU0_EJTAG_GROUP == 3)
#error "JTAG pins and SEG-LED pins are conflict!"
#endif

#endif

	/* enable seg led controller clock */
	acts_clock_peripheral_enable(cfg->clock_id);
	/* reset seg led controller */
	acts_reset_peripheral(cfg->reset_id);
	/* init the seg led clock source */
	seg_led_init_clksource(dev);

	return seg_led_enable(dev);
}

static int seg_led_refresh(struct device *dev)
{
	const struct acts_seg_led_config *cfg = dev->config->config_info;
	struct acts_seg_led_controller *base = cfg->base;
	u32_t start_time, curr_time;

	/* refresh the LCD/LED panel according to the LCD_DATA */
	base->ctl |= (1 << SEG_DISP_CTL_REFRSH);

	start_time = k_uptime_get();
	while ((base->ctl & (1 << 4)) != 0) {
		curr_time = k_uptime_get_32();
		if ((curr_time - start_time) >= SEG_LED_REFRSH_TIMEOUT_MS) {
			SYS_LOG_ERR("refresh reset timeout");
			dump_regs(base);
			return -ETIMEDOUT;
		}
	}
	return 0;
}

static int __seg_led_display(struct device *dev, u32_t *p_seg_disp_data)
{
	const struct acts_seg_led_config *cfg = dev->config->config_info;
	struct acts_seg_led_controller *base = cfg->base;

	base->data0 = *p_seg_disp_data++;
	base->data1 = *p_seg_disp_data++;
	base->data2 = *p_seg_disp_data++;
	base->data3 = *p_seg_disp_data;

	SYS_LOG_DBG("rammap: data0 0x%x, data1 0x%x, data2 0x%x, data3 0x%x", base->data0, base->data1, base->data2, base->data3);

	return seg_led_refresh(dev);
}

static int seg_led_display_number(struct device *dev, u8_t num_addr, u8_t c, bool display)
{
	struct acts_seg_led_data *data = dev->driver_data;
	u8_t bits;
	const struct led_com_map (*num_map)[CONFIG_LED_COM_NUM] = NULL;
	const struct led_bitmap *bit_map = NULL;
	u8_t com_index, i;
	u32_t table_len = 0;
	int ret;

	k_mutex_lock(&data->mutex, K_FOREVER);

	if (!data->led_ready_flag) {
		SYS_LOG_ERR("seg led has not ready yet");
		ret = -ENXIO;
		goto out;
	}

	if (num_addr > CONFIG_LED_NUMBER_ITEMS) {
		SYS_LOG_ERR("num_addr invalid %d", num_addr);
		ret = -EINVAL;
		goto out;
	}

	/* compatible with Dec 0 ~ 9 */
	if (c >= 0 && c <= 9)
		c += '0';

	if (!data->seg_map) {
#if defined(CONFIG_LED_USE_DEFAULT_MAP) || defined(CONFIG_LED_USE_1P2INCH_MAP)

#if (CONFIG_LED_COM_NUM != 7)&&(!defined(CONFIG_SEG_LCD_TYPE))
#error "Use default mode, the COM number shall be 7"
#endif
		num_map = num_map_default;
		bit_map = bitmap_table;
		table_len = ARRAY_SIZE(bitmap_table);
#else
		SYS_LOG_ERR("No seg map!");
		ret = -ENOENT;
		goto out;
#endif
	} else {
		num_map = data->seg_map->num_map;
		bit_map = data->seg_map->bit_map;
		table_len = LED_BITMAP_MAX_NUM;
	}

	bits = get_bitmap(c, bit_map, table_len);

	SYS_LOG_DBG("bitmap: 0x%x", bits);

	/* show the led segmemts according to the bitmap */
	for (i = 0; i < CONFIG_LED_COM_NUM; i++) {
#ifdef CONFIG_SEG_LCD_TYPE
		com_index = num_map[num_addr][i].com*4;
#else
		com_index = num_map[num_addr][i].com;
#endif
		if (SEG_LED_COMPIN_INVALID == com_index)
			continue;

		if ((bits & (1 << i)) != 0 && display) {
			/* show number */
			data->cur_disval[com_index] |= (1 << num_map[num_addr][i].pin);
		} else {
			/* hide number */
			data->cur_disval[com_index] &= (~(1 << num_map[num_addr][i].pin));
		}
	}

	ret = __seg_led_display(dev, (u32_t *)data->cur_disval);
	if (ret)
		SYS_LOG_ERR("seg led display error:%d", ret);

out:
	k_mutex_unlock(&data->mutex);

	return ret;
}

static int seg_led_display_number_string(struct device *dev, u8_t start_pos, const u8_t *str, bool display)
{
	u8_t str_num, i;
	int ret;

	if (!str)
		return -EINVAL;

	str_num = strlen(str);
	if ((str_num > CONFIG_LED_NUMBER_ITEMS )|| (start_pos >= CONFIG_LED_NUMBER_ITEMS)) {
		SYS_LOG_ERR("Invalid params %d %d", str_num, start_pos);
		return -EINVAL;
	}

	if ((str_num + start_pos) > CONFIG_LED_NUMBER_ITEMS)
		str_num = CONFIG_LED_NUMBER_ITEMS - start_pos;

	for (i = 0; i < str_num; i++) {
		ret = seg_led_display_number(dev, start_pos + i, str[i], display);
		if (ret) {
			SYS_LOG_DBG("display number err:%d", ret);
			return ret;
		}
	}

	return 0;
}

static int seg_led_display_icon(struct device *dev, u8_t icon_idx, bool display)
{
	struct acts_seg_led_data *data = dev->driver_data;
	const struct led_com_map *icon_map = NULL;
	u8_t com_index;
	int ret;

	k_mutex_lock(&data->mutex, K_FOREVER);

	if (!data->led_ready_flag) {
		SYS_LOG_ERR("seg led has not ready yet");
		ret = -ENXIO;
		goto out;
	}

	if (!data->seg_map) {
#if defined(CONFIG_LED_USE_DEFAULT_MAP) || defined(CONFIG_LED_USE_1P2INCH_MAP)
		icon_map = icon_map_default;
		if (icon_idx >= ARRAY_SIZE(icon_map_default)) {
			SYS_LOG_ERR("Invalid icon idx %d", icon_idx);
			return -EINVAL;
		}
#else
		SYS_LOG_ERR("No seg map!");
		ret = -ENOENT;
		goto out;
#endif
	} else {
		icon_map = data->seg_map->icon_map;
		if (icon_idx >= CONFIG_LED_NUMBER_ICONS) {
			SYS_LOG_ERR("Invalid icon idx %d", icon_idx);
			return -EINVAL;
		}
	}

	/* get table values index --- the true table byte idnex */
	com_index = icon_map[icon_idx].com;

	if (display == TRUE) {
		/* show icon */
		data->cur_disval[com_index] |= (1 << icon_map[icon_idx].pin);
	} else {
		/* hide icon */
		data->cur_disval[com_index] &= (~(1 << icon_map[icon_idx].pin));
	}

	ret = __seg_led_display(dev, (u32_t *)data->cur_disval);
	if (ret)
		SYS_LOG_ERR("seg led display error:%d", ret);

out:
	k_mutex_unlock(&data->mutex);

	return ret;
}

static int seg_led_clear_screen(struct device *dev, u8_t clr_mode)
{
	struct acts_seg_led_data *data = dev->driver_data;
	u8_t i = 0;
	int ret = -1;

	k_mutex_lock(&data->mutex, K_FOREVER);

	if (!data->led_ready_flag) {
		SYS_LOG_ERR("seg led has not ready yet");
		ret = -ENXIO;
		goto out;
	}

	/* clear data buffer can't use sys interface */
	if (clr_mode == LED_CLEAR_ALL) {
		for(i = 0; i < sizeof(data->cur_disval); i++)
			data->cur_disval[i] = 0;
		/* send data to led segment */
		if (data->led_ready_flag)
			ret = __seg_led_display(dev, (u32_t *)data->cur_disval);
	} else {
		for (i = 0; i < 4; i++)
			ret = seg_led_display_number(dev, i, 0, false);
	}

out:
	k_mutex_unlock(&data->mutex);

	return ret;
}

static int seg_led_sleep(struct device *dev)
{
	struct acts_seg_led_data *data = dev->driver_data;
	if (!data->led_ready_flag) {
		SYS_LOG_ERR("seg led has not ready yet");
		return -ENXIO;
	}
	__seg_led_phy_exit(dev);
	return 0;
}

static int seg_led_wake_up(struct device *dev)
{
	struct acts_seg_led_data *data = dev->driver_data;
	int ret = -ESRCH;

	if (!data->led_ready_flag) {
		ret = seg_led_hw_init(dev);
		if (ret) {
			SYS_LOG_ERR("seg led initialize failure err=%d", ret);
			return ret;
		}
		ret = __seg_led_display(dev, (u32_t *)data->cur_disval);
		if (ret)
			SYS_LOG_ERR("seg led display error:%d", ret);
	}

	return ret;
}

static int seg_led_update_seg_map(struct device *dev, struct led_seg_map *seg_map)
{
	struct acts_seg_led_data *data = dev->driver_data;
	if (!seg_map) {
		SYS_LOG_ERR("Invalid seg map");
		return -EINVAL;
	}

	if (!data->led_ready_flag) {
		SYS_LOG_ERR("seg led has not ready yet");
		return -ENXIO;
	}

	k_mutex_lock(&data->mutex, K_FOREVER);
	data->seg_map = seg_map;
	k_mutex_unlock(&data->mutex);

	return 0;
}

static int seg_led_refresh_onoff(struct device *dev, bool is_on)
{
	int ret = -1;
	struct acts_seg_led_data *data = dev->driver_data;

	if (!is_on) {
		u32_t seg_disp_data[2] = {0};
		if (data->led_ready_flag)
			ret = __seg_led_display(dev, seg_disp_data);
	} else {
		if (data->led_ready_flag)
			ret = __seg_led_display(dev, (u32_t *)data->cur_disval);
	}

	return ret;
}

static const struct seg_led_driver_api segled_driver_api = {
	.display_number		= seg_led_display_number,
	.display_number_str = seg_led_display_number_string,
	.display_icon		= seg_led_display_icon,
	.refresh_onoff		= seg_led_refresh_onoff,
	.clear_screen		= seg_led_clear_screen,
	.sleep				= seg_led_sleep,
	.wakeup				= seg_led_wake_up,
	.update_seg_map		= seg_led_update_seg_map,
};

static int seg_led_display_init(struct device *dev)
{
	int ret;
	struct acts_seg_led_data *data = dev->driver_data;

	memset(data, 0, sizeof(struct acts_seg_led_data));

	k_mutex_init(&data->mutex);

	data->led_ready_flag = false;

	ret = seg_led_hw_init(dev);
	if (ret) {
		SYS_LOG_ERR("seg led initialize failure err=%d", ret);
		return ret;
	}

	SYS_LOG_INF("sed led driver initialzied");

	return 0;
}

static struct acts_seg_led_data seg_led_data_0;

static const struct acts_seg_led_config seg_led_config_0 = {
	.base		= (void *)SEG_LED_REG_BASE,

#if defined(CONFIG_SOC_SERIES_ANDES) || defined(CONFIG_SOC_SERIES_ANDESM)
	.clk_reg	= CMU_SEGLCDCLK,
#else
	.clk_reg	= CMU_SEGLEDCLK,
#endif
	.clock_id	= CLOCK_ID_SEG_LCD,
	.reset_id	= RESET_ID_SEG_LCD,
};

DEVICE_AND_API_INIT(sed_led_acts, CONFIG_SEG_LED_DEVICE_NAME, seg_led_display_init,
	    &seg_led_data_0, &seg_led_config_0,
	    POST_KERNEL, CONFIG_IRQ_PRIO_LOW, &segled_driver_api);
