#include <soc.h>
#include "board.h"
#include <device.h>
#include <input_dev.h>
#include <audio_common.h>
#include "board_version.h"

#include <i2c.h>
#include <gpio.h>
#include <logging/sys_log.h>
#include "aw_params.h"
#include "aw_audio_common.h"

static struct device* my_gpio_dev = NULL;
static struct device* my_i2c_dev = NULL;

typedef struct {
	u8_t dev_addr;
	u8_t reg_addr;
} i2c_addr_t;

static u32_t i2c_write_dev(void *dev, i2c_addr_t *addr, u8_t *data, u8_t num_bytes)
{
	u8_t wr_addr[2];
	u32_t ret = 0;
	struct i2c_msg msgs[2];

	if (dev == NULL) {
		ret = -1;
		SYS_LOG_ERR("%d: i2c_gpio dev is not found\n", __LINE__);
		return ret;
	}

	wr_addr[0] = addr->reg_addr;

	msgs[0].buf = wr_addr;
	msgs[0].len = 1;
	msgs[0].flags = I2C_MSG_WRITE;

	msgs[1].buf = data;
	msgs[1].len = num_bytes;
	msgs[1].flags = I2C_MSG_WRITE;

	return i2c_transfer(dev, &msgs[0], 2, addr->dev_addr);
}

static u32_t i2c_read_dev(void *dev, i2c_addr_t *addr, u8_t *data, u8_t num_bytes)
{
	u8_t wr_addr[2];
	u32_t ret = 0;
	struct i2c_msg msgs[2];

	if (dev == NULL) {
		ret = -1;
		SYS_LOG_ERR("%d: i2c_gpio dev is not found\n", __LINE__);
		return ret;
	}

	wr_addr[0] = addr->reg_addr;

	msgs[0].buf = wr_addr;
	msgs[0].len = 1;
	msgs[0].flags = I2C_MSG_WRITE;

	msgs[1].buf = data;
	msgs[1].len = num_bytes;
	msgs[1].flags = I2C_MSG_READ|I2C_MSG_RESTART;

	return i2c_transfer(dev, &msgs[0], 2, addr->dev_addr);
}

int aw_dev0_i2c_write_func(uint16_t dev_addr, uint8_t reg_addr,
				uint8_t *pdata, uint16_t len)
{
	int ret;
	i2c_addr_t addr;

	addr.reg_addr = reg_addr;
	addr.dev_addr = (uint8_t)dev_addr;

	ret = i2c_write_dev(my_i2c_dev, &addr, pdata, len);
	if (ret != 0) {
		return -1;
	} else {
		return 0;
	}
}

int aw_dev0_i2c_read_func(uint16_t dev_addr, uint8_t reg_addr,
				uint8_t *pdata, uint16_t len)
{
	int ret = 0;
	i2c_addr_t addr;

	addr.reg_addr = reg_addr;
	addr.dev_addr = (uint8_t)dev_addr;

	ret = i2c_read_dev(my_i2c_dev, &addr, pdata, len);

	if (0 != ret){
		return -1;
	} else {
		return 0;
	}
}

void aw_dev0_reset_gpio_ctl(bool State)
{
	gpio_pin_write(my_gpio_dev, 19, State);
}

static struct aw_fill_info fill_info[] = {
	{
		.dev_index = AW_DEV_0,
		.phase_sync = AW_PHASE_SYNC_DISABLE,
		.i2c_addr = 0x36,
		.mix_chip_count = AW_DEV0_MIX_CHIP_NUM,
		.prof_info = g_dev0_prof_info,
		.i2c_read_func = aw_dev0_i2c_read_func,
		.i2c_write_func = aw_dev0_i2c_write_func,
		.reset_gpio_ctl = aw_dev0_reset_gpio_ctl,
	},
#if 0
	{
		.dev_index = AW_DEV_1,
		.phase_sync = AW_PHASE_SYNC_DISABLE,
		.i2c_addr =0x37,
		.mix_chip_count = AW_DEV1_MIX_CHIP_NUM,
		.prof_info = g_dev1_prof_info,
		.i2c_read_func = aw_dev1_i2c_read_func,
		.i2c_write_func = aw_dev1_i2c_write_func,
		.reset_gpio_ctl = aw_dev1_reset_gpio_ctl,
	},
#endif
};

void amp_aw882xx_interface_init(struct device* gpio, struct device* i2c)
{
	my_gpio_dev = gpio;
	my_i2c_dev = i2c;

	if (NULL == aw88xx_hal_iface_fops->init) {
		SYS_LOG_WRN("Not configed.");
	}

	aw88xx_hal_iface_fops->init((void *)&fill_info[AW_DEV_0]);
}