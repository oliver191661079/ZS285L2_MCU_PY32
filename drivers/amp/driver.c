/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <init.h>
#include <board.h>
#include <device.h>
#include <gpio.h>
#include <i2c.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <misc/printk.h>
#include <os_common_api.h>
#include <amp.h>
#include "aw882xx/aw_audio_common.h"

extern void amp_aw882xx_interface_init(struct device* gpio, struct device* i2c);

struct acts_amp_device_data {
	struct device *gpio_dev;
	struct device *i2c_dev;
	u8_t vol;
};

static struct acts_amp_device_data amp_dev_data = {NULL, NULL, 0};

static int acts_amp_init(struct device *dev)
{
	struct acts_amp_device_data *data = dev->driver_data;

	printk("amp init gpio dev\n");
	data->gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	if (NULL == data->gpio_dev) {
		SYS_LOG_ERR("gpio Device not found\n");
		return -1;
	}
	gpio_pin_configure(data->gpio_dev, 19, GPIO_DIR_OUT | GPIO_POL_INV);
	//gpio_pin_write(data->g_gpio_dev, 19, 0);

	printk("amp init i2c dev\n");
	data->i2c_dev = device_get_binding(CONFIG_I2C_GPIO_1_NAME);
	if (NULL == data->i2c_dev) {
		SYS_LOG_ERR("i2c Device not found\n");
		return -1;
	}
	union dev_config i2c_cfg;
	i2c_cfg.raw = 0;
	i2c_cfg.bits.is_master_device = 1;
	i2c_cfg.bits.speed = I2C_SPEED_STANDARD;
	i2c_configure(data->i2c_dev, i2c_cfg.raw);

	data->vol = 0;

	return 0;
}

static int acts_amp_open(struct device *dev)
{
	struct acts_amp_device_data *data = dev->driver_data;

	amp_aw882xx_interface_init(data->gpio_dev, data->i2c_dev);

	if (NULL == aw88xx_hal_iface_fops->set_profile_byname) {
		SYS_LOG_WRN("Not configed.");
		return -1;
	}

	/*awinic:set mode id 0, "aw_params.h" contains id information*/
	aw88xx_hal_iface_fops->set_profile_byname(AW_DEV_0, "Music");

	return 0;
}

static int acts_amp_start(struct device *dev)
{
	if(NULL == aw88xx_hal_iface_fops->ctrl_state){
		SYS_LOG_WRN("Not configed.");
		return -1;
	}
	aw88xx_hal_iface_fops->ctrl_state(AW_DEV_0, START);

	return 0;
}

static int acts_amp_stop(struct device *dev)
{
	if(NULL == aw88xx_hal_iface_fops->ctrl_state){
		SYS_LOG_WRN("Not configed.");
		return -1;
	}
	aw88xx_hal_iface_fops->ctrl_state(AW_DEV_0, STOP);
	return 0;
}

static int acts_amp_close(struct device *dev)
{
	if(NULL == aw88xx_hal_iface_fops->deinit){
		SYS_LOG_WRN("Not configed.");
		return -1;
	}
	aw88xx_hal_iface_fops->deinit(AW_DEV_0);
	return 0;
}

static int acts_amp_set_volume(struct device *dev, u8_t volume)
{
	struct acts_amp_device_data *data = dev->driver_data;

	if(NULL == aw88xx_hal_iface_fops->set_volume){
		SYS_LOG_WRN("Not configed.");
		return -1;
	}
	aw88xx_hal_iface_fops->set_volume(AW_DEV_0, volume);

	data->vol = volume;

	return 0;
}

static int acts_amp_set_reg(struct device *dev, u8_t addr, u16_t dat)
{
	if(NULL == aw88xx_hal_debug_attr->reg_store) {
		SYS_LOG_WRN("Not configed.");
		return -1;
	}

	aw88xx_hal_debug_attr->reg_store(AW_DEV_0, addr, dat);


	return 0;
}
static int acts_amp_dump_regs(struct device *dev)
{
	if(NULL == aw88xx_hal_debug_attr->reg_show) {
		SYS_LOG_WRN("Not configed.");
		return -1;
	}

	aw88xx_hal_debug_attr->reg_show(AW_DEV_0);

	return 0;
}

static const AMP_DRIVER_API amp_acts_driver_api = {
	.open = acts_amp_open,
	.close = acts_amp_close,
	.start = acts_amp_start,
	.stop  = acts_amp_stop,
	.set_vol = acts_amp_set_volume,
	.set_reg = acts_amp_set_reg,
	.dump_regs = acts_amp_dump_regs,
};

DEVICE_AND_API_INIT(amp, CONFIG_AMP_DEV_NAME, acts_amp_init, \
		&amp_dev_data, NULL, POST_KERNEL, 47, &amp_acts_driver_api);
