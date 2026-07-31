/*
 * Copyright (c) 2026 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <init.h>
#include <device.h>
#include <i2c.h>
#include <misc/printk.h>
#include <string.h>
#include <soc.h>
#include <amp.h>

#define SYS_LOG_DOMAIN "acm8635"
#include <logging/sys_log.h>
#include "acm8635.h"
#include <board.h>

#if defined(CONFIG_CODEC_ACM8635_I2C_BITBANG)
#define ACM8635_I2C_BUS_LABEL	"soft-GPIO"
#elif defined(CONFIG_I2C_0)
#define ACM8635_I2C_BUS_LABEL	"hw-I2C"
#else
#define ACM8635_I2C_BUS_LABEL	"unknown"
#endif

struct acm8635_device_data {
	struct device *i2c_dev;
	u8_t i2c_addr;
	u8_t volume;
};

static struct acm8635_device_data acm8635_dev_data;
static bool acm8635_amp_playing;

#if defined(CONFIG_CODEC_ACM8635_I2C_BITBANG) || \
	(defined(CONFIG_I2C_0) && defined(CONFIG_CODEC_ACM8635))

#if defined(CONFIG_CODEC_ACM8635_I2C_BITBANG)
#define ACM8635_GPIO_PIN_RELEASE \
	(GPIO_CTL_MFP_GPIO | GPIO_CTL_GPIO_INEN | GPIO_CTL_PULLUP | \
	 GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3))
#define ACM8635_GPIO_PIN_DRIVE_LOW \
	(GPIO_CTL_MFP_GPIO | GPIO_CTL_GPIO_OUTEN | GPIO_CTL_PULLUP | \
	 GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3))

static u32_t acm8635_twi_gpio_read(u32_t pin)
{
	return !!(sys_read32(GPIO_REG_IDAT(GPIO_REG_BASE, pin)) & GPIO_BIT(pin));
}
#endif

static void acm8635_twi_gpio_mode(u32_t pin, bool output)
{
	u32_t ctl = GPIO_CTL_MFP_GPIO | GPIO_CTL_PULLUP | GPIO_CTL_SMIT |
		    GPIO_CTL_PADDRV_LEVEL(3);

	if (output) {
		ctl |= GPIO_CTL_GPIO_OUTEN;
	} else {
		ctl |= GPIO_CTL_GPIO_INEN;
	}
	sys_write32(ctl, GPIO_CTL(pin));
}

static inline void acm8635_twi_gpio_release(u32_t pin)
{
#if defined(CONFIG_CODEC_ACM8635_I2C_BITBANG)
	sys_write32(ACM8635_GPIO_PIN_RELEASE, GPIO_CTL(pin));
#else
	acm8635_twi_gpio_mode(pin, false);
#endif
}

static inline void acm8635_twi_gpio_drive_low(u32_t pin)
{
#if defined(CONFIG_CODEC_ACM8635_I2C_BITBANG)
	sys_write32(ACM8635_GPIO_PIN_DRIVE_LOW, GPIO_CTL(pin));
	sys_write32(GPIO_BIT(pin), GPIO_REG_BRR(GPIO_REG_BASE, pin));
#else
	acm8635_twi_gpio_mode(pin, true);
	sys_write32(GPIO_BIT(pin), GPIO_REG_BRR(GPIO_REG_BASE, pin));
#endif
}

static inline void acm8635_twi_gpio_line(u32_t pin, int level)
{
	if (level) {
		acm8635_twi_gpio_release(pin);
	} else {
		acm8635_twi_gpio_drive_low(pin);
	}
}

static void acm8635_i2c_gpio_bus_recovery(void)
{
	u32_t scl = CONFIG_I2C_GPIO_1_SCL_PIN;
	u32_t sda = CONFIG_I2C_GPIO_1_SDA_PIN;
	int i;

	acm8635_twi_gpio_mode(scl, true);
	acm8635_twi_gpio_mode(sda, true);
	acm8635_twi_gpio_line(scl, 1);
	acm8635_twi_gpio_line(sda, 1);
	k_sleep(K_MSEC(1));

	for (i = 0; i < 9; i++) {
		acm8635_twi_gpio_line(scl, 0);
		k_busy_wait(5);
		acm8635_twi_gpio_line(scl, 1);
		k_busy_wait(5);
	}

	acm8635_twi_gpio_line(sda, 0);
	k_busy_wait(5);
	acm8635_twi_gpio_line(scl, 0);
	k_busy_wait(5);
	acm8635_twi_gpio_line(scl, 1);
	k_busy_wait(5);
	acm8635_twi_gpio_line(sda, 1);
	k_busy_wait(50);

#if defined(CONFIG_CODEC_ACM8635_I2C_BITBANG)
	acm8635_twi_gpio_release(scl);
	acm8635_twi_gpio_release(sda);
	k_busy_wait(50);
#endif
}

static void acm8635_i2c_bus_recovery(struct acm8635_device_data *data)
{
	union dev_config i2c_cfg;

	acm8635_i2c_gpio_bus_recovery();

	if (data == NULL || data->i2c_dev == NULL) {
		return;
	}

	i2c_cfg.raw = 0;
	i2c_cfg.bits.is_master_device = 1;
#if defined(CONFIG_CODEC_ACM8635_I2C_BUS_400K)
	i2c_cfg.bits.speed = I2C_SPEED_FAST;
#else
	i2c_cfg.bits.speed = I2C_SPEED_STANDARD;
#endif
	i2c_configure(data->i2c_dev, i2c_cfg.raw);

#if defined(CONFIG_CODEC_ACM8635_I2C_BITBANG)
	SYS_LOG_INF("TWI recovery idle SCL=%u SDA=%u (expect both 1)",
		    acm8635_twi_gpio_read(CONFIG_I2C_GPIO_1_SCL_PIN),
		    acm8635_twi_gpio_read(CONFIG_I2C_GPIO_1_SDA_PIN));
#else
	SYS_LOG_INF("TWI GPIO bus recovery done");
#endif
}

#endif

static int acm8635_write_byte(struct acm8635_device_data *data, u8_t reg,
			      u8_t val)
{
	u8_t buf[2] = { reg, val };
	int ret;

#if defined(CONFIG_CODEC_ACM8635_I2C_FORCE_WRITE)
	ret = i2c_write(data->i2c_dev, buf, sizeof(buf), data->i2c_addr);
#else
	ret = i2c_reg_write_byte(data->i2c_dev, data->i2c_addr, reg, val);
#endif
	return ret;
}

static int acm8635_read_byte(struct acm8635_device_data *data, u8_t reg,
			       u8_t *val)
{
	return i2c_reg_read_byte(data->i2c_dev, data->i2c_addr, reg, val);
}

static int acm8635_load_reg_table(struct acm8635_device_data *data,
				  const struct acm8635_reg_tab *tab, u32_t count)
{
	u32_t i;
	int ret;

	for (i = 0; i < count; i++) {
		ret = acm8635_write_byte(data, tab[i].address, tab[i].data);
		if (ret != 0) {
			SYS_LOG_ERR("reg write fail @%u addr=0x%02x data=0x%02x "
				    "ret=%d",
				    i, tab[i].address, tab[i].data, ret);
			return ret;
		}
	}
	return 0;
}

static int acm8635_load_reg_tables(struct acm8635_device_data *data)
{
	int ret;

	ret = acm8635_load_reg_table(data, acm8635_reg_tab_init,
				     acm8635_reg_tab_init_count);
	if (ret != 0) {
		return ret;
	}

	SYS_LOG_INF("DSP init script done, wait %u ms",
		    CONFIG_CODEC_ACM8635_DSP_INIT_DELAY_MS);
	k_sleep(K_MSEC(CONFIG_CODEC_ACM8635_DSP_INIT_DELAY_MS));

	ret = acm8635_load_reg_table(data, acm8635_reg_tab_main,
				     acm8635_reg_tab_main_count);
	if (ret != 0) {
		return ret;
	}

	if (CONFIG_CODEC_ACM8635_DSP_MAIN_DELAY_MS > 0) {
		SYS_LOG_INF("DSP main script done, wait %u ms",
			    CONFIG_CODEC_ACM8635_DSP_MAIN_DELAY_MS);
		k_sleep(K_MSEC(CONFIG_CODEC_ACM8635_DSP_MAIN_DELAY_MS));
	}

	return 0;
}

#define ACM8635_STATE_PLAY		0x03
#define ACM8635_STATE_RPT_PLAY		0x03

static void acm8635_set_volume(struct acm8635_device_data *data, u8_t vol);
static void acm8635_log_play_state(struct acm8635_device_data *data,
				   const char *stage);

static int acm8635_read_chip_id(struct acm8635_device_data *data,
				u8_t *fc, u8_t *fd, u8_t *fe)
{
	int ret;

	ret = acm8635_write_byte(data, 0x00, 0x00);
	if (ret != 0) {
		return ret;
	}

	ret = acm8635_read_byte(data, 0xfc, fc);
	if (ret != 0) {
		return ret;
	}

	ret = acm8635_read_byte(data, 0xfd, fd);
	if (ret != 0) {
		return ret;
	}

	return acm8635_read_byte(data, 0xfe, fe);
}

static void acm8635_log_chip_id_preinit(int ret, u8_t fc, u8_t fd, u8_t fe)
{
	if (ret != 0) {
		SYS_LOG_WRN("pre-init chip id read fail ret=%d", ret);
		return;
	}

	if (fc == ACM8635_CHIP_ID_FC && fd == ACM8635_CHIP_ID_FD &&
	    fe == ACM8635_CHIP_ID_FE) {
		SYS_LOG_INF("pre-init chip id 0x%02x 0x%02x 0x%02x",
			    fc, fd, fe);
	} else {
		SYS_LOG_INF("pre-init chip id 0x%02x 0x%02x 0x%02x",
			    fc, fd, fe);
	}
}

static int acm8635_read_status_regs(struct acm8635_device_data *data,
				    u8_t *r15, u8_t *ctrl, u8_t *stat)
{
	int ret;

	ret = acm8635_write_byte(data, 0x00, 0x00);
	if (ret != 0) {
		return ret;
	}

	ret = acm8635_read_byte(data, 0x15, r15);
	if (ret != 0) {
		return ret;
	}

	ret = acm8635_read_byte(data, 0x04, ctrl);
	if (ret != 0) {
		return ret;
	}

	return acm8635_read_byte(data, 0x16, stat);
}

static bool acm8635_status_regs_dead(u8_t r15, u8_t ctrl, u8_t stat)
{
	return r15 == 0 && ctrl == 0 && stat == 0;
}

static int acm8635_probe_online(struct acm8635_device_data *data,
				u8_t *r15, u8_t *ctrl, u8_t *stat)
{
	int ret;

	ret = acm8635_read_status_regs(data, r15, ctrl, stat);
	if (ret != 0) {
		return ret;
	}

	if (acm8635_status_regs_dead(*r15, *ctrl, *stat)) {
		return -EIO;
	}

	return 0;
}

static int acm8635_dsp_configure(struct acm8635_device_data *data, u8_t vol)
{
	int ret;

	ret = acm8635_load_reg_tables(data);
	if (ret != 0) {
		return ret;
	}

	acm8635_set_volume(data, vol);
	return 0;
}

#if defined(CONFIG_CODEC_ACM8635_I2C_BITBANG) || \
	(defined(CONFIG_I2C_0) && defined(CONFIG_CODEC_ACM8635))
static void acm8635_prepare_i2c_access(struct acm8635_device_data *data,
				       const char *stage)
{
	SYS_LOG_INF("%s: TWI recovery", stage);
	acm8635_i2c_bus_recovery(data);
}
#else
static void acm8635_prepare_i2c_access(struct acm8635_device_data *data,
				       const char *stage)
{
	ARG_UNUSED(data);
	ARG_UNUSED(stage);
}
#endif

static int acm8635_ensure_i2c_online(struct acm8635_device_data *data,
				     const char *stage)
{
	u8_t r15 = 0;
	u8_t ctrl = 0;
	u8_t stat = 0;
	int ret;

	ret = acm8635_probe_online(data, &r15, &ctrl, &stat);
	if (ret == 0) {
		return 0;
	}

	SYS_LOG_WRN("%s: TWI probe fail ret=%d, recovery", stage, ret);
	acm8635_prepare_i2c_access(data, stage);

	ret = acm8635_probe_online(data, &r15, &ctrl, &stat);
	if (ret == 0) {
		SYS_LOG_INF("%s: online after recovery 0x15=0x%02x 0x04=0x%02x "
			    "0x16=0x%02x",
			    stage, r15, ctrl, stat);
		return 0;
	}

	SYS_LOG_WRN("%s: reload DSP tables (last resort)", stage);
	ret = acm8635_dsp_configure(data, data->volume);
	if (ret != 0) {
		SYS_LOG_ERR("%s: DSP re-init fail ret=%d", stage, ret);
		return ret;
	}

	ret = acm8635_probe_online(data, &r15, &ctrl, &stat);
	if (ret != 0) {
		SYS_LOG_ERR("%s: still offline after re-init ret=%d", stage, ret);
		return ret;
	}

	acm8635_log_play_state(data, "re-init done");
	return 0;
}

static void acm8635_log_play_state(struct acm8635_device_data *data,
				   const char *stage)
{
	u8_t r15 = 0;
	u8_t ctrl = 0;
	u8_t stat = 0;

	acm8635_read_status_regs(data, &r15, &ctrl, &stat);
	if (stat == ACM8635_STATE_RPT_PLAY &&
	    ctrl == ACM8635_STATE_PLAY) {
		SYS_LOG_INF("%s: 0x15=0x%02x 0x04=0x%02x 0x16=0x%02x (Play)",
			    stage, r15, ctrl, stat);
	} else {
		SYS_LOG_WRN("%s: 0x15=0x%02x 0x04=0x%02x 0x16=0x%02x "
			    "(expect Play 0x04/0x16=0x03)",
			    stage, r15, ctrl, stat);
	}
}

/* Same sequence as vendor volumeControl(): L/R/Sub DSP volume. */
static void acm8635_set_volume(struct acm8635_device_data *data, u8_t vol)
{
	const u8_t *vol_bytes;

	if (vol > 16) {
		vol = 16;
	}

	vol_bytes = &acm8635_vol_table[4 * (16 - vol)];

	acm8635_write_byte(data, 0x00, 0x04);
	acm8635_write_byte(data, 0x8c, vol_bytes[0]);
	acm8635_write_byte(data, 0x8d, vol_bytes[1]);
	acm8635_write_byte(data, 0x8e, vol_bytes[2]);
	acm8635_write_byte(data, 0x8f, vol_bytes[3]);

	acm8635_write_byte(data, 0x00, 0x04);
	acm8635_write_byte(data, 0x90, vol_bytes[0]);
	acm8635_write_byte(data, 0x91, vol_bytes[1]);
	acm8635_write_byte(data, 0x92, vol_bytes[2]);
	acm8635_write_byte(data, 0x93, vol_bytes[3]);

	acm8635_write_byte(data, 0x00, 0x0c);
	acm8635_write_byte(data, 0x5c, vol_bytes[0]);
	acm8635_write_byte(data, 0x5d, vol_bytes[1]);
	acm8635_write_byte(data, 0x5e, vol_bytes[2]);
	acm8635_write_byte(data, 0x5f, vol_bytes[3]);

	data->volume = vol;
}

static void acm8635_play_enable(struct acm8635_device_data *data)
{
	/* Vendor ACM86xx_UnMute(): 0x04 = 0x03, no mute/Hi-Z. */
	acm8635_write_byte(data, 0x00, 0x00);
	acm8635_write_byte(data, 0x04, ACM8635_STATE_PLAY);
}

#define ACM8635_FAULT_CLR_REG		0x01
#define ACM8635_FAULT_CLR_BIT		0x80	/* page0 0x01 bit7: clear 0x17-0x19 */

static void acm8635_read_fault_regs(struct acm8635_device_data *data,
				    u8_t *fault17, u8_t *fault18, u8_t *fault19)
{
	acm8635_write_byte(data, 0x00, 0x00);
	acm8635_read_byte(data, 0x17, fault17);
	acm8635_read_byte(data, 0x18, fault18);
	acm8635_read_byte(data, 0x19, fault19);
}

static bool acm8635_has_fault(u8_t fault17, u8_t fault18, u8_t fault19)
{
	return fault17 != 0 || fault18 != 0 || fault19 != 0;
}

/* Datasheet 10.6: write page0 0x01 bit7 (0->1) to clear error log 0x17-0x19. */
static void acm8635_fault_clear(struct acm8635_device_data *data)
{
	acm8635_write_byte(data, 0x00, 0x00);
	acm8635_write_byte(data, ACM8635_FAULT_CLR_REG, ACM8635_FAULT_CLR_BIT);
	k_sleep(K_MSEC(5));
}

static void acm8635_recover_faults(struct acm8635_device_data *data,
				   const char *stage)
{
	u8_t fault17 = 0;
	u8_t fault18 = 0;
	u8_t fault19 = 0;

	acm8635_read_fault_regs(data, &fault17, &fault18, &fault19);
	SYS_LOG_WRN("%s fault before clear: 0x17=0x%02x 0x18=0x%02x 0x19=0x%02x",
		    stage, fault17, fault18, fault19);
	acm8635_fault_clear(data);
	acm8635_read_fault_regs(data, &fault17, &fault18, &fault19);
	if (acm8635_has_fault(fault17, fault18, fault19)) {
		SYS_LOG_WRN("%s fault after clear: 0x17=0x%02x 0x18=0x%02x "
			    "0x19=0x%02x",
			    stage, fault17, fault18, fault19);
	} else {
		SYS_LOG_INF("%s fault cleared: 0x17=0x%02x 0x18=0x%02x 0x19=0x%02x",
			    stage, fault17, fault18, fault19);
	}
}

static int acm8635_register_init(struct device *dev)
{
	struct acm8635_device_data *data = dev->driver_data;
	u8_t fc = 0;
	u8_t fd = 0;
	u8_t fe = 0;
	int ret;

	data->i2c_addr = CONFIG_CODEC_ACM8635_I2C_ADDR;
	SYS_LOG_INF("init addr=0x%02x", data->i2c_addr);

	ret = acm8635_read_chip_id(data, &fc, &fd, &fe);
	acm8635_log_chip_id_preinit(ret, fc, fd, fe);

	ret = acm8635_load_reg_tables(data);
	if (ret != 0) {
		SYS_LOG_ERR("init table fail ret=%d addr=0x%02x", ret,
			    data->i2c_addr);
		return ret;
	}

	acm8635_set_volume(data, CONFIG_CODEC_ACM8635_INIT_VOLUME);
	acm8635_log_play_state(data, "init done");

	SYS_LOG_INF("init addr=0x%02x vol=%u", data->i2c_addr,
		    CONFIG_CODEC_ACM8635_INIT_VOLUME);
	return 0;
}

static int acm8635_init(struct device *dev)
{
	struct acm8635_device_data *data = dev->driver_data;
	union dev_config i2c_cfg;

	SYS_LOG_INF("TWI mode=%s bus=%s",
		    ACM8635_I2C_BUS_LABEL, CONFIG_CODEC_ACM8635_I2C_NAME);

	data->i2c_dev = device_get_binding(CONFIG_CODEC_ACM8635_I2C_NAME);
	if (data->i2c_dev == NULL) {
		SYS_LOG_ERR("TWI %s not found", CONFIG_CODEC_ACM8635_I2C_NAME);
		return -ENODEV;
	}

	data->volume = CONFIG_CODEC_ACM8635_INIT_VOLUME;

	i2c_cfg.raw = 0;
	i2c_cfg.bits.is_master_device = 1;
#if defined(CONFIG_CODEC_ACM8635_I2C_BUS_400K)
	i2c_cfg.bits.speed = I2C_SPEED_FAST;
	SYS_LOG_INF("TWI speed=400kHz (Fast, ACM8635 max)");
#else
	i2c_cfg.bits.speed = I2C_SPEED_STANDARD;
	SYS_LOG_INF("TWI speed=100kHz (Standard)");
#endif
	i2c_configure(data->i2c_dev, i2c_cfg.raw);

#if defined(CONFIG_CODEC_ACM8635_I2C_BITBANG) || \
	(defined(CONFIG_I2C_0) && defined(CONFIG_CODEC_ACM8635))
	acm8635_i2c_bus_recovery(data);
#endif

	return acm8635_register_init(dev);
}

static int acm8635_amp_open(struct device *dev)
{
	ARG_UNUSED(dev);
	/* Register tables loaded at device init, same as AW882XX open hook. */
	SYS_LOG_INF("open (init already done)");
	return 0;
}

static bool acm8635_is_play_state(u8_t state)
{
	return state == ACM8635_STATE_PLAY;
}

static int acm8635_amp_start(struct device *dev)
{
	struct acm8635_device_data *data = dev->driver_data;
	u8_t state = 0;
	u8_t fault17 = 0;
	u8_t fault18 = 0;
	u8_t fault19 = 0;
	u8_t r15 = 0;
	u8_t stat = 0;
	u8_t ctrl = 0;
	int ret;

	if (data->i2c_dev == NULL) {
		SYS_LOG_ERR("start: TWI not ready");
		return -ENODEV;
	}

	ret = acm8635_ensure_i2c_online(data, "start");
	if (ret != 0) {
		SYS_LOG_ERR("start: chip not ready ret=%d", ret);
		return ret;
	}

	acm8635_write_byte(data, 0x00, 0x00);
	if (acm8635_read_byte(data, 0x04, &state) == 0 &&
	    acm8635_is_play_state(state) && acm8635_amp_playing) {
		return 0;
	}

	SYS_LOG_INF("start: vol=%u at %u ms", data->volume, k_uptime_get_32());

	acm8635_read_fault_regs(data, &fault17, &fault18, &fault19);
	if (acm8635_has_fault(fault17, fault18, fault19)) {
		acm8635_recover_faults(data, "start");
	}

	acm8635_set_volume(data, data->volume);
	acm8635_play_enable(data);
	k_sleep(K_MSEC(10));
	acm8635_amp_playing = true;

	acm8635_read_status_regs(data, &r15, &ctrl, &stat);
	acm8635_read_fault_regs(data, &fault17, &fault18, &fault19);
	if (acm8635_status_regs_dead(r15, ctrl, stat)) {
		SYS_LOG_WRN("start: status all zero after play");
	}
	if (stat != ACM8635_STATE_RPT_PLAY || ctrl != ACM8635_STATE_PLAY) {
		acm8635_play_enable(data);
		k_sleep(K_MSEC(10));
		acm8635_read_status_regs(data, &r15, &ctrl, &stat);
		acm8635_read_fault_regs(data, &fault17, &fault18, &fault19);
	}
	acm8635_log_play_state(data, "after start");
	if (acm8635_has_fault(fault17, fault18, fault19)) {
		acm8635_recover_faults(data, "after start");
		acm8635_play_enable(data);
		k_sleep(K_MSEC(10));
		acm8635_log_play_state(data, "after start");
		acm8635_read_status_regs(data, &r15, &ctrl, &stat);
		acm8635_read_fault_regs(data, &fault17, &fault18, &fault19);
	}

	if (acm8635_has_fault(fault17, fault18, fault19)) {
		SYS_LOG_WRN("after start: 0x15=0x%02x 0x04=0x%02x 0x16=0x%02x "
			    "0x17=0x%02x 0x18=0x%02x 0x19=0x%02x",
			    r15, ctrl, stat, fault17, fault18, fault19);
	} else {
		SYS_LOG_INF("after start: 0x15=0x%02x 0x04=0x%02x 0x16=0x%02x "
			    "0x17=0x%02x 0x18=0x%02x 0x19=0x%02x",
			    r15, ctrl, stat, fault17, fault18, fault19);
	}

	return 0;
}

static int acm8635_amp_stop(struct device *dev)
{
	struct acm8635_device_data *data = dev->driver_data;

	if (data->i2c_dev == NULL) {
		return -ENODEV;
	}

	/* No mute/Hi-Z: vendor warns mute + PDN can break recovery. */
	SYS_LOG_INF("stop");
	acm8635_amp_playing = false;
	return 0;
}

static int acm8635_amp_close(struct device *dev)
{
	return acm8635_amp_stop(dev);
}

static int acm8635_amp_set_volume(struct device *dev, u8_t volume)
{
	struct acm8635_device_data *data = dev->driver_data;

	if (data->i2c_dev == NULL) {
		return -ENODEV;
	}

	acm8635_set_volume(data, volume);
	return 0;
}

static int acm8635_amp_set_reg(struct device *dev, u8_t addr, u16_t dat)
{
	struct acm8635_device_data *data = dev->driver_data;

	ARG_UNUSED(dev);
	return acm8635_write_byte(data, addr, (u8_t)dat);
}

static int acm8635_amp_dump_regs(struct device *dev)
{
	ARG_UNUSED(dev);
	SYS_LOG_WRN("dump_regs not implemented");
	return -ENOTSUP;
}

static const AMP_DRIVER_API acm8635_amp_driver_api = {
	.open = acm8635_amp_open,
	.close = acm8635_amp_close,
	.start = acm8635_amp_start,
	.stop = acm8635_amp_stop,
	.set_vol = acm8635_amp_set_volume,
	.set_reg = acm8635_amp_set_reg,
	.dump_regs = acm8635_amp_dump_regs,
};

DEVICE_AND_API_INIT(acm8635, CONFIG_AMP_DEV_NAME, acm8635_init,
		    &acm8635_dev_data, NULL, POST_KERNEL, 65,
		    &acm8635_amp_driver_api);
