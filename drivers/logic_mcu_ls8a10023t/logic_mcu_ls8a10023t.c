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
#include <logic_mcu_ls8a10023t.h>

#define BIT(n)  (1UL << (n))

#define LS8A10023T_I2C_ADDR         0x08

struct logic_mcu_ls8a10023t_device_data_t {
	struct device *gpio_dev;
	struct device *i2c_dev;
};

static struct logic_mcu_ls8a10023t_device_data_t logic_mcu_ls8a10023t_device_data;

static u8_t power_key_flag = 0; //0: key up, 1: key down

static int _logic_mcu_ls8A10023t_i2c_reg_write(struct device *dev, u8_t reg, u8_t data)
{
    struct logic_mcu_ls8a10023t_device_data_t *dev_data = dev->driver_data;

    u8_t wr_buf[2];

    wr_buf[0] = reg;
    wr_buf[1] = data;
    
    return i2c_write(dev_data->i2c_dev, wr_buf, 2, LS8A10023T_I2C_ADDR);
}

static int _logic_mcu_ls8A10023t_i2c_reg_read(struct device *dev, u8_t reg, u8_t *data)
{
    struct logic_mcu_ls8a10023t_device_data_t *dev_data = dev->driver_data;
    struct i2c_msg msgs[2];

    /* Send the address to read */
    msgs[0].buf = &reg;/* Register address */
    msgs[0].len = 1;
    msgs[0].flags = I2C_MSG_WRITE;// | I2C_MSG_STOP;
    
    /* Read from device. RESTART as neededm and STOP after this. */
    msgs[1].buf = data;
    msgs[1].len = 1;
    msgs[1].flags = I2C_MSG_RESTART | I2C_MSG_READ | I2C_MSG_STOP;
    
    return i2c_transfer(dev_data->i2c_dev, &msgs[0], 2, LS8A10023T_I2C_ADDR);
}

static int _logic_mcu_ls8A10023t_power_on_rdy(struct device *dev)
{
    u8_t read_data = 0, write_data = 0;
    int reg_addr = 0;
    int ret = 0;

    reg_addr = 0x7b;

    ret = _logic_mcu_ls8A10023t_i2c_reg_read(dev, reg_addr, &read_data);
    if(ret != 0)
    {
        printk("[%s,%d] i2c read, reg_addr:0x%X, error_code:0x%X\n", __FUNCTION__, __LINE__, reg_addr, ret);
        return ret;
    }
    printk("[%s,%d] i2c read, reg_addr:0x%X, read_data:0x%X\n", __FUNCTION__, __LINE__, reg_addr, read_data);

    write_data = read_data & (~BIT(0));

    ret = _logic_mcu_ls8A10023t_i2c_reg_write(dev, reg_addr, write_data);
    if(ret != 0)
    {
        printk("[%s,%d] i2c write, reg_addr:0x%X, error_code:0x%X\n", __FUNCTION__, __LINE__, reg_addr, ret);
        return ret;
    }
    printk("[%s,%d] i2c write, reg_addr:0x%X, write_data:0x%X\n", __FUNCTION__, __LINE__, reg_addr, write_data);

    write_data = read_data | BIT(0);

    ret = _logic_mcu_ls8A10023t_i2c_reg_write(dev, reg_addr, write_data);
    if(ret != 0)
    {
        printk("[%s,%d] i2c write, reg_addr:0x%X, error_code:0x%X\n", __FUNCTION__, __LINE__, reg_addr, ret);
        return ret;
    }
    printk("[%s,%d] i2c write, reg_addr:0x%X, write_data:0x%X\n", __FUNCTION__, __LINE__, reg_addr, write_data);

    printk("[%s,%d] success\n", __FUNCTION__, __LINE__);

    return 0;
}

static int _logic_mcu_ls8A10023t_power_off_ready(struct device *dev)
{
    u8_t read_data = 0, write_data = 0;
    int reg_addr = 0;
    int ret = 0;

    reg_addr = 0x7b;

    ret = _logic_mcu_ls8A10023t_i2c_reg_read(dev, reg_addr, &read_data);
    if(ret != 0)
    {
        printk("[%s,%d] i2c read, reg_addr:0x%X, error_code:0x%X\n", __FUNCTION__, __LINE__, reg_addr, ret);
        return ret;
    }
    printk("[%s,%d] i2c read, reg_addr:0x%X, read_data:0x%X\n", __FUNCTION__, __LINE__, reg_addr, read_data);

    write_data = read_data | BIT(6);  

    ret = _logic_mcu_ls8A10023t_i2c_reg_write(dev, reg_addr, write_data);
    if(ret != 0)
    {
        printk("[%s,%d] i2c write, reg_addr:0x%X, error_code:0x%X\n", __FUNCTION__, __LINE__, reg_addr, ret);
        return ret;
    }
    printk("[%s,%d] i2c write, reg_addr:0x%X, write_data:0x%X\n", __FUNCTION__, __LINE__, reg_addr, write_data);

    write_data = read_data & (~BIT(6));

    ret = _logic_mcu_ls8A10023t_i2c_reg_write(dev, reg_addr, write_data);
    if(ret != 0)
    {
        printk("[%s,%d] i2c write, reg_addr:0x%X, error_code:0x%X\n", __FUNCTION__, __LINE__, reg_addr, ret);
        return ret;
    }
    printk("[%s,%d] i2c write, reg_addr:0x%X, write_data:0x%X\n", __FUNCTION__, __LINE__, reg_addr, write_data);

    printk("[%s,%d] success\n", __FUNCTION__, __LINE__);
    return 0;
}

static int _logic_mcu_ls8A10023t_int_clear(struct device *dev)
{
    u8_t read_data =0x00, write_data = 0x00;
    int reg_addr = 0;
    int ret = 0;
    
    reg_addr = 0x7b;

    ret = _logic_mcu_ls8A10023t_i2c_reg_read(dev, reg_addr, &read_data);
    if(ret != 0)
    {
        printk("[%s,%d] i2c read, reg_addr:0x%X, error_code:0x%X\n", __FUNCTION__, __LINE__, reg_addr, ret);
        return ret;
    }
    printk("[%s,%d] i2c read, reg_addr:0x%X, read_data:0x%X\n", __FUNCTION__, __LINE__, reg_addr, read_data);

    write_data = read_data & (~BIT(2)); 

    ret = _logic_mcu_ls8A10023t_i2c_reg_write(dev, reg_addr, write_data);
    if(ret != 0)
    {
        printk("[%s,%d] i2c write, reg_addr:0x%X, error_code:0x%X\n", __FUNCTION__, __LINE__, reg_addr, ret);
        return ret;
    }
    printk("[%s,%d] i2c write, reg_addr:0x%X, write_data:0x%X\n", __FUNCTION__, __LINE__, reg_addr, write_data);

    write_data = read_data | BIT(2); 

    ret = _logic_mcu_ls8A10023t_i2c_reg_write(dev, reg_addr, write_data);
    if(ret != 0)
    {
        printk("[%s,%d] i2c write, reg_addr:0x%X, error_code:0x%X\n", __FUNCTION__, __LINE__, reg_addr, ret);
        return ret;
    }
    printk("[%s,%d] i2c write, reg_addr:0x%X, write_data:0x%X\n", __FUNCTION__, __LINE__, reg_addr, write_data);

    printk("[%s,%d] success\n", __FUNCTION__, __LINE__);
    
    return 0;
}

static int _logic_mcu_ls8A10023t_standby_det(struct device *dev)
{
    u8_t read_data = 0, write_data = 0;
    int reg_addr = 0;
    int ret = 0;
    
    reg_addr = 0x7b;

    ret = _logic_mcu_ls8A10023t_i2c_reg_read(dev, reg_addr, &read_data);
    if(ret != 0)
    {
        printk("[%s,%d] i2c read, reg_addr:0x%X, error_code:0x%X\n", __FUNCTION__, __LINE__, reg_addr, ret);
        return ret;
    }
    printk("[%s,%d] i2c read, reg_addr:0x%X, read_data:0x%X\n", __FUNCTION__, __LINE__, reg_addr, read_data);

    write_data = read_data & (~BIT(3));

    ret = _logic_mcu_ls8A10023t_i2c_reg_write(dev, reg_addr, write_data);
    if(ret != 0)
    {
        printk("[%s,%d] i2c write, reg_addr:0x%X, error_code:0x%X\n", __FUNCTION__, __LINE__, reg_addr, ret);
        return ret;
    }
    printk("[%s,%d] i2c write, reg_addr:0x%X, write_data:0x%X\n", __FUNCTION__, __LINE__, reg_addr, write_data);
    
    write_data = read_data | BIT(3);

    ret = _logic_mcu_ls8A10023t_i2c_reg_write(dev, reg_addr, write_data);
    if(ret != 0)
    {
        printk("[%s,%d] i2c write, reg_addr:0x%X, error_code:0x%X\n", __FUNCTION__, __LINE__, reg_addr, ret);
        return ret;
    }
    printk("[%s,%d] i2c write, reg_addr:0x%X, write_data:0x%X\n", __FUNCTION__, __LINE__, reg_addr, write_data);

    printk("[%s,%d] success\n", __FUNCTION__, __LINE__);

    return 0;
}

static int _logic_mcu_ls8A10023t_power_key(struct device *dev)
{
    int ret = 0;
    u8_t read_data = 0, write_data = 0;
    int reg_addr = 0;
    static u8_t last_key_flag = 0xff; //0: key up, 1: key down
    u8_t key_flag = 0;

    reg_addr = 0x7e;

    ret = _logic_mcu_ls8A10023t_i2c_reg_read(dev, reg_addr, &read_data);
    if(ret != 0)
    {
        printk("[%s,%d] i2c read, reg_addr:0x%X, error_code:0x%X\n", __FUNCTION__, __LINE__, reg_addr, ret);
        return ret;
    }
    printk("[%s,%d] i2c read, reg_addr:0x%X, read_data:0x%X\n", __FUNCTION__, __LINE__, reg_addr, read_data);

    write_data = (read_data | BIT(4)) & (~BIT(5));

    ret = _logic_mcu_ls8A10023t_i2c_reg_write(dev, reg_addr, write_data);
    if(ret != 0)
    {
        printk("[%s,%d] i2c write, reg_addr:0x%X, error_code:0x%X\n", __FUNCTION__, __LINE__, reg_addr, ret);
        return ret;
    }
    printk("[%s,%d] i2c write, reg_addr:0x%X, write_data:0x%X\n", __FUNCTION__, __LINE__, reg_addr, write_data);

    reg_addr = 0x7d;

    ret = _logic_mcu_ls8A10023t_i2c_reg_read(dev, reg_addr, &read_data);
    if(ret != 0)
    {
        printk("[%s,%d] i2c read, reg_addr:0x%X, error_code:0x%X\n", __FUNCTION__, __LINE__, reg_addr, ret);
        return ret;
    }
    printk("[%s,%d] i2c read, reg_addr:0x%X, read_data:0x%X\n", __FUNCTION__, __LINE__, reg_addr, read_data);

    write_data = (read_data | BIT(5)) & (~BIT(3)) & (~BIT(4));

    ret = _logic_mcu_ls8A10023t_i2c_reg_write(dev, reg_addr, write_data);
    if(ret != 0)
    {
        printk("[%s,%d] i2c write, reg_addr:0x%X, error_code:0x%X\n", __FUNCTION__, __LINE__, reg_addr, ret);
        return ret;
    }
    printk("[%s,%d] i2c write, reg_addr:0x%X, write_data:0x%X\n", __FUNCTION__, __LINE__, reg_addr, write_data);

    reg_addr = 0x7c;

    ret = _logic_mcu_ls8A10023t_i2c_reg_read(dev, reg_addr, &read_data);
    if(ret != 0)
    {
        printk("[%s,%d] i2c read, reg_addr:0x%X, error_code:0x%X\n", __FUNCTION__, __LINE__, reg_addr, ret);
        return ret;
    }
    printk("[%s,%d] i2c read, reg_addr:0x%X, read_data:0x%X\n", __FUNCTION__, __LINE__, reg_addr, read_data);

    if(read_data & BIT(6))
    {
        key_flag = 0x01;
        _logic_mcu_ls8A10023t_standby_det(dev);
    }
    else
    {
        key_flag = 0x00;
    }

    reg_addr = 0x7b;

    ret = _logic_mcu_ls8A10023t_i2c_reg_read(dev, reg_addr, &read_data);
    if(ret != 0)
    {
        printk("[%s,%d] i2c read, reg_addr:0x%X, error_code:0x%X\n", __FUNCTION__, __LINE__, reg_addr, ret);
        return ret;
    }
    printk("[%s,%d] i2c read, reg_addr:0x%X, read_data:0x%X\n", __FUNCTION__, __LINE__, reg_addr, read_data);

    write_data = read_data & (~BIT(7));

    ret = _logic_mcu_ls8A10023t_i2c_reg_write(dev, reg_addr, write_data);
    if(ret != 0)
    {
        printk("[%s,%d] i2c write, reg_addr:0x%X, error_code:0x%X\n", __FUNCTION__, __LINE__, reg_addr, ret);
        return ret;
    }
    printk("[%s,%d] i2c write, reg_addr:0x%X, write_data:0x%X\n", __FUNCTION__, __LINE__, reg_addr, write_data);

    write_data = read_data | BIT(7);

    ret = _logic_mcu_ls8A10023t_i2c_reg_write(dev, reg_addr, write_data);
    if(ret != 0)
    {
        printk("[%s,%d] i2c write, reg_addr:0x%X, error_code:0x%X\n", __FUNCTION__, __LINE__, reg_addr, ret);
        return ret;
    }
    printk("[%s,%d] i2c write, reg_addr:0x%X, write_data:0x%X\n", __FUNCTION__, __LINE__, reg_addr, write_data);

    if(last_key_flag != key_flag)
    {
        last_key_flag = key_flag;
        if (power_key_flag != last_key_flag)
        {
            power_key_flag = last_key_flag;
            printk("update key_flag = 0x%X\n", last_key_flag);
        }
    }

    return power_key_flag;
}

//ret 0 key up, ret 1 key down
static int _logic_mcu_ls8A10023t_get_power_key_flag(struct device *dev)
{
    return power_key_flag;
}

static int _logic_mcu_ls8a10023t_init(struct device *dev)
{
    struct logic_mcu_ls8a10023t_device_data_t *data = dev->driver_data;

    printk("[%s,%d] init gpio dev\n", __FUNCTION__, __LINE__);
    data->gpio_dev = device_get_binding(CONFIG_GPIO_ACTS_DEV_NAME);
	if (NULL == data->gpio_dev) {
		printk("[%s,%d] gpio Device not found\n", __FUNCTION__, __LINE__);
		return -1;
	}

    printk("[%s,%d] init i2c dev\n", __FUNCTION__, __LINE__);
	data->i2c_dev = device_get_binding(CONFIG_I2C_GPIO_1_NAME);
	if (NULL == data->i2c_dev) {
		printk("[%s,%d] i2c Device not found\n", __FUNCTION__, __LINE__);
		return -1;
	}

    union dev_config i2c_cfg;
	i2c_cfg.raw = 0;
	i2c_cfg.bits.is_master_device = 1;
	i2c_cfg.bits.speed = I2C_SPEED_STANDARD;
	i2c_configure(data->i2c_dev, i2c_cfg.raw);

    _logic_mcu_ls8A10023t_power_on_rdy(dev);
    _logic_mcu_ls8A10023t_standby_det(dev);

	return 0;
}

static const struct logic_mcu_ls8a10023t_driver_api_t logic_mcu_ls8a10023t_driver_api = {
    .power_on_rdy = _logic_mcu_ls8A10023t_power_on_rdy,
    .power_off_ready = _logic_mcu_ls8A10023t_power_off_ready,
    .int_clear = _logic_mcu_ls8A10023t_int_clear,
    .standby_det = _logic_mcu_ls8A10023t_standby_det,
    
    .power_key = _logic_mcu_ls8A10023t_power_key,
    .get_power_key_flag = _logic_mcu_ls8A10023t_get_power_key_flag,
};

DEVICE_AND_API_INIT(logic_mcu_ls8a_10023t, CONFIG_LOGIC_MCU_LS8A10023T_DEV_NAME, _logic_mcu_ls8a10023t_init, \
		&logic_mcu_ls8a10023t_device_data, NULL, POST_KERNEL, 1, &logic_mcu_ls8a10023t_driver_api);
