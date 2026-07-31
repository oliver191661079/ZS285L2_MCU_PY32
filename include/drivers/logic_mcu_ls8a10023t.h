#ifndef __LOGIC_MCU_LS8A10023T_H__
#define __LOGIC_MCU_LS8A10023T_H__

#include <zephyr.h>

struct logic_mcu_ls8a10023t_driver_api_t {
	//i2c操作写入寄存器
    int (*power_on_rdy)(struct device *dev);
    int (*power_off_ready)(struct device *dev);
    int (*int_clear)(struct device *dev);
    int (*standby_det)(struct device *dev);
	//i2c操作读取寄存器
    int (*power_key)(struct device *dev);
	//获取按键标志
    int (*get_power_key_flag)(struct device *dev);
};

static inline int logic_mcu_ls8a10023t_power_on_rdy(struct device *dev)
{
	const struct logic_mcu_ls8a10023t_driver_api_t *api = dev->driver_api;

	return api->power_on_rdy(dev);
}

static inline int logic_mcu_ls8a10023t_power_off_ready(struct device *dev)
{
	const struct logic_mcu_ls8a10023t_driver_api_t *api = dev->driver_api;

	return api->power_off_ready(dev);
}

static inline int logic_mcu_ls8a10023t_int_clear(struct device *dev)
{
	const struct logic_mcu_ls8a10023t_driver_api_t *api = dev->driver_api;

	return api->int_clear(dev);
}

static inline int logic_mcu_ls8a10023t_standby_det(struct device *dev)
{
	const struct logic_mcu_ls8a10023t_driver_api_t *api = dev->driver_api;

	return api->standby_det(dev);
}

static inline int logic_mcu_ls8a10023t_power_key(struct device *dev)
{
	const struct logic_mcu_ls8a10023t_driver_api_t *api = dev->driver_api;

	return api->power_key(dev);
}

//应用层获取按键标志位
static inline int logic_mcu_ls8A10023t_get_power_key_flag(struct device *dev)
{
    const struct logic_mcu_ls8a10023t_driver_api_t *api = dev->driver_api;

	return api->get_power_key_flag(dev);
}

#endif //__LOGIC_MCU_LS8A10023T_H__