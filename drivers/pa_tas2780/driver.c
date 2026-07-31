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
#include "tas2780.h"

#define TAS2780_I2C_ADDR   0x38

#if (1 == CONFIG_BOARD_ATS2875H_RIDE_VERSION)
#define PIN_AMP_SDZ				15 //gpio high activity
#define PIN_AMP_POWER_ON2		39 //gpio high activity
#define PIN_AMP_BOOST_PWM		6
#else
#define PIN_AMP_SDZ				9 //gpio high activity
#define PIN_AMP_POWER_ON2		17 //gpio high activity
#define PIN_AMP_BOOST_PWM		16
#endif

struct pa_tas2780_device_data_t {
	struct device *gpio_dev;
	struct device *i2c_dev;
};

static struct pa_tas2780_device_data_t pa_tas2780_device_data;

static void tas2780_transmit_registers(struct device *dev, const cfg_reg *r, u32_t n) 
{
	u32_t i = 0;
	int ret;
    
    printk("[%s,%d] len = %d\n", __FUNCTION__, __LINE__, n);
    
	while (i < n) {
		ret = 0;

		switch (r[i].command)
		{
			case CFG_META_SWITCH:
				// Used in legacy applications.  Ignored here.
				break;

			case CFG_META_DELAY:
				k_busy_wait(r[i].param * 1000);
				break;

			case CFG_META_BURST:
				ret = i2c_write(dev, (uint8_t *)&r[i+1], r[i].param+1, TAS2780_I2C_ADDR);
				i +=  (r[i].param / 2) + 1;
				break;
			
			default:
				ret = i2c_write(dev, (uint8_t *)&r[i], 2, TAS2780_I2C_ADDR);
				break;
		}

		if (ret != 0)
			printk("[%s,%d] error\n", __FUNCTION__, __LINE__);

		i++;
	}
}

static void pa_tas2780_register_init(struct device *dev)
{
	struct pa_tas2780_device_data_t *data = dev->driver_data;

	printk("[%s,%d] start\n", __FUNCTION__, __LINE__);

	tas2780_transmit_registers(data->i2c_dev, Y_bridge, sizeof(Y_bridge)/sizeof(Y_bridge[0]));
	tas2780_transmit_registers(data->i2c_dev, voice_power_up_prim, sizeof(voice_power_up_prim)/sizeof(voice_power_up_prim[0]));

	printk("[%s,%d] end\n", __FUNCTION__, __LINE__);
}

static int pa_tas2780_init(struct device *dev)
{
	struct pa_tas2780_device_data_t *data = dev->driver_data;

	printk("[%s,%d] init gpio dev\n", __FUNCTION__, __LINE__);
	data->gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	if (NULL == data->gpio_dev) {
		printk("[%s,%d] gpio Device not found\n", __FUNCTION__, __LINE__);
		return -1;
	}

	gpio_pin_configure(data->gpio_dev, PIN_AMP_SDZ, GPIO_DIR_OUT | GPIO_POL_NORMAL);
	gpio_pin_configure(data->gpio_dev, PIN_AMP_POWER_ON2, GPIO_DIR_OUT | GPIO_POL_NORMAL);
	gpio_pin_configure(data->gpio_dev, PIN_AMP_BOOST_PWM, GPIO_DIR_OUT | GPIO_POL_NORMAL);

	gpio_pin_write(data->gpio_dev, PIN_AMP_SDZ, 1);
	gpio_pin_write(data->gpio_dev, PIN_AMP_POWER_ON2, 1);
	//AMP_BOOST_PWM TODO

	printk("[%s,%d] init i2c dev\n", __FUNCTION__, __LINE__);
	data->i2c_dev = device_get_binding(CONFIG_I2C_GPIO_1_NAME);
	if (NULL == data->i2c_dev) {
		printk("[%s,%d] i2c Device not found\n", __FUNCTION__, __LINE__);
		return -1;
	}

	pa_tas2780_register_init(dev);

	return 0;
}

DEVICE_AND_API_INIT(pa_tas2780, CONFIG_PA_TAS2780_DEV_NAME, pa_tas2780_init, \
		&pa_tas2780_device_data, NULL, POST_KERNEL, 47, NULL);
