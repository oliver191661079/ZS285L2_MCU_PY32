/**
 * @file
 *
 * @brief Public APIs for the I2C drivers.
 */

/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef __DRIVERS_I2C_SLAVE_H
#define __DRIVERS_I2C_SLAVE_H

/**
 * @brief I2C Slave Interface
 * @defgroup i2c_slave interface I2C Slave Interface
 * @ingroup io_interfaces
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <zephyr/types.h>
#include <device.h>

typedef int(*i2c_rx_callback_t)(struct device *dev, u8_t *rx_data,
				int rx_data_size);

struct i2c_slave_driver_api {
	int (*register_slave)(struct device *dev, u16_t i2c_addr,
			      i2c_rx_callback_t rx_cbk);
	void (*unregister_slave)(struct device *dev, u16_t i2c_addr);
	int (*update_tx_data)(struct device *dev, u16_t i2c_addr,
			      const u8_t *buf, int count);
};

static inline int i2c_slave_register(struct device *dev, u16_t i2c_addr,
				  i2c_rx_callback_t rx_cbk)
{
	const struct i2c_slave_driver_api *api = dev->driver_api;

	return api->register_slave(dev, i2c_addr, rx_cbk);
}


static inline void i2c_slave_unregister(struct device *dev, u16_t i2c_addr)
{
	const struct i2c_slave_driver_api *api = dev->driver_api;

	api->unregister_slave(dev, i2c_addr);
}

static inline int i2c_slave_update_tx_data(struct device *dev, u16_t i2c_addr,
					   const u8_t *buf, int count)
{
	const struct i2c_slave_driver_api *api = dev->driver_api;

	return api->update_tx_data(dev, i2c_addr, buf, count);
}

#ifdef __cplusplus
}
#endif

/**
 * @}
 */


#endif /* __DRIVERS_I2C_SLAVE_H */
