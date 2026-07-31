/*
 * Copyright (c) 2023 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __AMP_H__
#define __AMP_H__
#include <zephyr.h>

typedef struct {
	int (*open)(struct device *dev);
	int (*close)(struct device *dev);
	int (*start)(struct device *dev);
	int (*stop)(struct device *dev);
	int (*set_vol)(struct device *dev, u8_t vol);
	int (*set_reg)(struct device *dev, u8_t addr, u16_t data);
	int (*dump_regs)(struct device *dev);
}AMP_DRIVER_API;

static inline int amp_open(struct device *dev)
{
	const AMP_DRIVER_API *api = dev->driver_api;

	return api->open(dev);
}

static inline int amp_close(struct device *dev)
{
	const AMP_DRIVER_API *api = dev->driver_api;

	return api->close(dev);
}

static inline int amp_start(struct device *dev)
{
	const AMP_DRIVER_API *api = dev->driver_api;

	return api->start(dev);
}

static inline int amp_stop(struct device *dev)
{
	const AMP_DRIVER_API *api = dev->driver_api;

	return api->stop(dev);
}

static inline int amp_set_volume(struct device *dev, u8_t vol)
{
	const AMP_DRIVER_API *api = dev->driver_api;

	return api->set_vol(dev, vol);
}

static inline int amp_set_reg(struct device *dev, u8_t addr, u16_t data)
{
	const AMP_DRIVER_API *api = dev->driver_api;

	return api->set_reg(dev, addr, data);
}

static inline int amp_dump_regs(struct device *dev)
{
	const AMP_DRIVER_API *api = dev->driver_api;

	return api->dump_regs(dev);
}

#endif /* __AMP_H__ */
