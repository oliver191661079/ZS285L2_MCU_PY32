/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef __FM_H__
#define __FM_H__
#include <zephyr.h>

struct acts_fm_device_data {
	struct device *fm_dev;
	struct fm_tx_device *tx_subdev;
	struct fm_rx_device *rx_subdev;
	u32_t current_freq;
	u8_t mute_flag:1;
};

struct fm_tx_device {
	int (*set_freq)(struct fm_tx_device *dev, u16_t freq);
	int (*set_mute)(struct fm_tx_device *dev, bool flag);
	struct device *i2c;
};

struct fm_init_para_t {
	u32_t channel_config;
	u8_t threshold; //search threshold
};

struct fm_rx_device {
    int (*init)(struct fm_rx_device *dev, void *param);
    int (*set_freq)(struct fm_rx_device *dev, u16_t freq);
    int (*set_mute)(struct fm_rx_device *dev, bool flag);
    int (*scan_freq)(struct fm_rx_device *dev);
    int (*check_inval_freq)(struct fm_rx_device *dev, u16_t freq);
    int (*deinit)(struct fm_rx_device *dev);
	struct device *i2c;
};

int fm_reciver_set_freq(struct device *fm_dev, u32_t freq);

int fm_reciver_set_mute(struct device *fm_dev, bool flag);

int fm_reciver_check_freq(struct device *fm_dev, u32_t freq);

int fm_reciver_init(struct device *fm_dev, void *param);

int fm_reciver_deinit(struct device *fm_dev);

int fm_receiver_device_register(struct fm_rx_device *device);

int fm_transmitter_device_register(struct fm_tx_device *device);

int fm_transmitter_set_freq(struct device *fm_dev, u32_t freq);

int fm_transmitter_get_freq(struct device *fm_dev);

int fm_transmitter_set_mute(struct device *fm_dev, bool flag);


struct fm_driver_api {
	void (*enable)(struct device *dev);
	void (*disable)(struct device *dev);
	int (*set_freq)(struct device *dev, u16_t freq);
	u16_t (*get_freq)(struct device *dev);
	int (*set_mute)(struct device *dev, bool mute);
	int (*check_freq)(struct device *dev, u16_t freq);
};



#endif /* __FM_H__ */
