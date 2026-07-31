/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Wakeup GPIO driver for Actions SoC
 */

#include <errno.h>
#include <kernel.h>
#include <device.h>
#include <init.h>
#include <irq.h>
#include <gpio.h>
#include <soc.h>

#define WIO_CTL_GPIO_OUTEN		BIT(6)
#define WIO_CTL_GPIO_INEN		BIT(7)
#define WIO_CTL_PULLUP			BIT(8)
#define WIO_CTL_PULLDOWN		BIT(9)
#define WIO_CTL_PADDRV_SHIFT		(12)
#define WIO_CTL_PADDRV_LEVEL(x)		((x) << WIO_CTL_PADDRV_SHIFT)
#define WIO_CTL_PADDRV_MASK			WIO_CTL_PADDRV_LEVEL(0x7)
#define WIO_CTL_GPIO_DAT		BIT(16)

#define WIO_REG_CTL(base, pin)		((base) + (pin) * 4)

struct acts_gpio_wio_config {
	u32_t base;
	u32_t num;
};

static int gpio_wio_acts_config(struct device *dev, int access_op,
				u32_t pin, int flags)
{
	const struct acts_gpio_wio_config *info = dev->config->config_info;
    u32_t key, data, val = 0;

    if (access_op != GPIO_ACCESS_BY_PIN)
		return -ENOTSUP;

	if (pin >= info->num)
		return -EINVAL;

	/* config pull up/down */
	if ((flags & GPIO_PUD_MASK) == GPIO_PUD_PULL_UP) {
		val |= WIO_CTL_PULLUP;
	} else if ((flags & GPIO_PUD_MASK) == GPIO_PUD_PULL_DOWN) {
		val |= WIO_CTL_PULLDOWN;
	}

	/* config in/out */
	if ((flags & GPIO_DIR_MASK) == GPIO_DIR_OUT) {
		val |= WIO_CTL_GPIO_OUTEN;
	} else {
		val |= WIO_CTL_GPIO_INEN;
	}

	if ((flags & GPIO_DIR_MASK) == GPIO_DIR_OUT) {
		/* initial gpio value, normal == 0, invert == 1 */
		if ((flags & GPIO_POL_MASK) == GPIO_POL_INV)
			val |= WIO_CTL_GPIO_DAT;
	}

    if (flags & GPIO_PADDRV_SET_MASK)
    {
        data = flags & GPIO_PADDRV_LEVEL_MASK;
        data >>= GPIO_PADDRV_POS;
        val |= (data << GPIO_CTL_PADDRV_SHIFT);
    }
    else
    {
        data = sys_read32(GPIO_REG_CTL(info->base, pin));
        val |= data & GPIO_CTL_PADDRV_MASK;
    }

    key = irq_lock();

	sys_write32(val, WIO_REG_CTL(info->base, pin));

	irq_unlock(key);

	return 0;
}

static int gpio_wio_acts_write(struct device *dev, int access_op,
			   u32_t pin, u32_t value)
{
	const struct acts_gpio_wio_config *info = dev->config->config_info;
	u32_t val, key;

	if (access_op != GPIO_ACCESS_BY_PIN)
		return -ENOTSUP;

	if (pin >= info->num)
		return -EINVAL;

	key = irq_lock();

	val = sys_read32(WIO_REG_CTL(info->base, pin));

	if (value) {
		/* output high level */
		val |= WIO_CTL_GPIO_DAT;
	} else {
		/* output low level */
		val &= ~WIO_CTL_GPIO_DAT;
	}
        sys_write32(val, WIO_REG_CTL(info->base, pin));

	irq_unlock(key);

	return 0;
}

static int gpio_wio_acts_read(struct device *dev, int access_op,
			  u32_t pin, u32_t *value)
{
	const struct acts_gpio_wio_config *info = dev->config->config_info;

	if (access_op != GPIO_ACCESS_BY_PIN)
		return -ENOTSUP;

	if (pin >= info->num)
		return -EINVAL;

	*value = !!(sys_read32(WIO_REG_CTL(info->base, pin)) &
		    WIO_CTL_GPIO_DAT);

	return 0;
}

const struct gpio_driver_api gpio_wio_acts_drv_api = {
	.config = gpio_wio_acts_config,
	.write = gpio_wio_acts_write,
	.read = gpio_wio_acts_read,
};

int gpio_wio_acts_init(struct device *dev)
{
	ARG_UNUSED(dev);

	return 0;
}

static const struct acts_gpio_wio_config acts_gpio_wio_config_0 = {
	.base = GPIO_WIO_REG_BASE,
	.num = GPIO_WIO_MAX_PIN_NUM,
};

DEVICE_AND_API_INIT(gpio_wio_acts, CONFIG_GPIO_WIO_ACTS_DEV_NAME,
		    gpio_wio_acts_init, NULL, &acts_gpio_wio_config_0,
		    PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEVICE,
		    &gpio_wio_acts_drv_api);
