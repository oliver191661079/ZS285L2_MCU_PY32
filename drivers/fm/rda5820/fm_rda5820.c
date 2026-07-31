/*
 *fm module rda5820
 *
 *
 *
 */

#include <zephyr.h>
//#include <init.h>
#include <board.h>
#include <i2c.h>
#include <device.h>
#include <string.h>
#include <misc/printk.h>
#include <fm.h>
#include <stdlib.h>
#include <logging/sys_log.h>
#include "fm_rda5820.h"
#ifdef CONFIG_NVRAM_CONFIG
#include "nvram_config.h"
#endif

enum FM_AREA {
	CH_US_AREA,
	JP_AREA,
	EU_AREA,
	MAX_AREA,
};

typedef struct {
	struct fm_tx_device rda5820_tx_dev;
	struct fm_rx_device rda5820_rx_dev;
	u32_t fm_channel_config;
} fm_rda5820_device;

typedef struct {
	u8_t dev_addr;
	u8_t reg_addr;
} i2c_addr_t;

extern int rda5820_tx_init(void);
extern int rda5820_rx_init(void);
int rda5820_set_mute(bool flag);

/* Driver instance data */
static fm_rda5820_device rda5820_info = {0};

u32_t i2c_write_dev(void *dev, i2c_addr_t *addr, u8_t *data, u8_t num_bytes)
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

u32_t i2c_read_dev(void *dev, i2c_addr_t *addr, u8_t *data, u8_t num_bytes)
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
	msgs[1].flags = I2C_MSG_READ | I2C_MSG_RESTART;

	return i2c_transfer(dev, &msgs[0], 2, addr->dev_addr);
}

u8_t rda_WriteReg(u8_t Regis_Addr, u16_t Data)
{
	u8_t i;
	int result = 0;
	u8_t writebuffer[2];
	i2c_addr_t addr;
	struct device *i2c = device_get_binding(CONFIG_I2C_1_NAME);

	addr.reg_addr = Regis_Addr;
	addr.dev_addr = FM_I2C_DEV0_ADDRESS;
	writebuffer[1] = Data;
	writebuffer[0] = Data >> 8;

	for (i = 0; i < 10; i++) {
		if (i2c_write_dev(i2c, &addr, writebuffer, 2) == 0) {
			break;
		}
	}
	if (i == 10) {
		SYS_LOG_INF("FM QN write failed\n");
		result = -1;
	}

	return result;
}

u16_t rda_ReadReg(u8_t Regis_Addr)
{
	u8_t Data[2] = {0};
	u8_t init_cnt = 0;
	u8_t ret = 0;
	u16_t tmp = 0;
	struct device *i2c = device_get_binding(CONFIG_I2C_1_NAME);

	i2c_addr_t addr;

	addr.reg_addr = Regis_Addr;
	addr.dev_addr = FM_I2C_DEV0_ADDRESS;

	for (init_cnt = 0; init_cnt < 10; init_cnt++) {
		ret = i2c_read_dev(i2c, &addr, Data, 2);
		if (ret == 0) {
			break;
		}
	}

	if (init_cnt == 10) {
		Data[0] = 0xff;
		Data[1] = 0xff;
	}

	tmp = Data[0] << 8;
	tmp |= Data[1];

	return tmp;
}

int rda5820_set_freq(u16_t freq)
{
	u16_t space, bank_max, bank_mix;
	u16_t tmp_freq, tmp;

	rda5820_set_mute(1);

	rda_WriteReg(0x09, 0x0831);

	tmp = rda_ReadReg(RDA_REGISTER_03);
	 /*step */
	if ((tmp & 0x03) == 0x00) {
		space = 100;
	} else if ((tmp & 0x03) == 0x01) {
		space = 200;
	} else {
		space = 50;
	}

	if (((tmp >> 2) & 0x03) == 0x00) {
		bank_mix = 8700;
		bank_max = 10800;
	} else if (((tmp >> 2) & 0x03) == 0x01 || ((tmp >> 2) & 0x03) == 0x10) {
		bank_mix = 7500;
		bank_max = 10800;
	} else {
		bank_mix = 6500;
		bank_max = 7600;
	}

	 /* Determine if the frequency point is in the current frequency band */
	if (freq < bank_mix || freq > bank_max) {
		SYS_LOG_INF("err!! freq :%d, bank_mix :%d, bank_max :%d", freq, bank_mix, bank_max);
		return 1;
	}
	 /* current frequency point  set[15:6] */
	tmp_freq = (freq - bank_mix) * 10 / space;

	u16_t final = (tmp_freq << 6) | (tmp & 0x3f);
	/* enble turn bit */
	final |= 0x0010;
	rda_WriteReg(RDA_REGISTER_03, final);

	rda_WriteReg(0x09, 0x0031);

	rda5820_set_mute(0);

	return 0;
}

int rda5820_check_freq(struct fm_rx_device *dev, u16_t freq)
{
	u8_t value = 0;
	u16_t tmp;

	rda5820_set_mute(1);

	rda5820_set_freq(freq);
	rda_delay(100);

	tmp = rda_ReadReg(RDA_REGISTER_0b);

	value = (tmp >> 8) & 0x01;
	if (value == 1) {
		SYS_LOG_INF("freq : %d is value", freq);
	} else {
		SYS_LOG_INF("freq : %d is not value", freq);
	}

	return !value;
 }

int rda5820_set_mute(bool flag)
{
	u16_t temp = rda_ReadReg(RDA_REGISTER_02);
	 /**1: mute, 0: unmute**/
	if (flag)
		rda_WriteReg(RDA_REGISTER_02, temp & 0xBFFF);
	else
		rda_WriteReg(RDA_REGISTER_02, temp | 0x4000);

	return 0;
}

int rda5820_set_rx_mute(struct fm_rx_device *dev, bool flag)
{
	return rda5820_set_mute(flag);
}

int rda5820_set_tx_mute(struct fm_tx_device *dev, bool flag)
{
	return rda5820_set_mute(flag);
}

int rda5820_set_rx_freq(struct fm_rx_device *dev, u16_t freq)
{
	return rda5820_set_freq(freq);
}

int rda5820_set_tx_freq(struct fm_tx_device *dev, u16_t freq)
{
	return rda5820_set_freq(freq);
}


int rda5820_tx_hw_init(struct fm_tx_device *dev)
{
	u16_t chip_id;
	u16_t work_module;
	int default_freq = 910;
	char temp[5] = { 0 };

	chip_id = rda_ReadReg(RDA_REGISTER_00);
	if (chip_id == 0x5820) {
#ifndef CONFIG_FM_EXT_24M
		rda_WriteReg(RDA_REGISTER_02, 0xC001);
#else
		acts_clock_peripheral_enable(CLOCK_ID_FM_CLK);
		sys_write32(0, CMU_FMOUTCLK);/* 0 is HOSC ,1 is 1/2 HOSC */

		rda_WriteReg(RDA_REGISTER_02, 0xC051);
#endif

		switch(rda5820_info.fm_channel_config) {
		case CH_US_AREA :
			rda_WriteReg(RDA_REGISTER_03, 0x0000);
			break;

		case JP_AREA :
			rda_WriteReg(RDA_REGISTER_03, 0x0004);
			break;

		case EU_AREA :
			rda_WriteReg(RDA_REGISTER_03, 0x0002);
			break;

		default :
			rda_WriteReg(RDA_REGISTER_03, 0x0000);
			break;

		}

		rda_WriteReg(RDA_REGISTER_04, 0x0c00);
		rda_WriteReg(RDA_REGISTER_05, 0x860F);
		rda_WriteReg(RDA_REGISTER_06, 0x6000);
		rda_WriteReg(RDA_REGISTER_15, 0xF83D);
		rda_WriteReg(RDA_REGISTER_16, 0xF000);
		rda_WriteReg(RDA_REGISTER_18, 0x3410);
		rda_WriteReg(RDA_REGISTER_19, 0x00C6);
		rda_WriteReg(RDA_REGISTER_1e, 0x8808);
		rda_WriteReg(RDA_REGISTER_20, 0x0AE1);
		rda_WriteReg(RDA_REGISTER_25, 0x0488);
		rda_WriteReg(RDA_REGISTER_28, 0x03EC);
		rda_WriteReg(RDA_REGISTER_2d, 0x0716);
		rda_WriteReg(RDA_REGISTER_2e, 0x0830);
		rda_WriteReg(RDA_REGISTER_2f, 0x2AA0);
		rda_WriteReg(RDA_REGISTER_36, 0x03A4);
		rda_WriteReg(RDA_REGISTER_37, 0x35E7);
		rda_WriteReg(RDA_REGISTER_38, 0x03E0);
		rda_WriteReg(RDA_REGISTER_5b, 0x6800);

	}else {
		return -EINVAL;
	}

	/* set fm word module */
	work_module = rda_ReadReg(RDA_REGISTER_40);
	work_module &= 0xfff0;
	work_module |= 0x0001;
	rda_WriteReg(RDA_REGISTER_40, work_module);
	rda_WriteReg(RDA_REGISTER_41, 0x41FF);

	rda_WriteReg(RDA_REGISTER_68, 0x1DFF);

#ifdef CONFIG_NVRAM_CONFIG
	if (nvram_config_get("FM_DEFAULT_FREQ", temp, sizeof(temp)) < 0) {
		default_freq = 9100;
	} else {
		default_freq = atoi(temp);
	}
#endif
	rda5820_set_tx_freq(dev, default_freq);
	return 0;
}

extern int rda5820_tx_init(void)
{
	struct fm_tx_device *rda5820_tx = &rda5820_info.rda5820_tx_dev;
	rda5820_tx->i2c = device_get_binding(CONFIG_I2C_1_NAME);
	if (!rda5820_tx->i2c) {
		SYS_LOG_ERR("Device not found\n");
		return -1;
	}

	union dev_config i2c_cfg = {0};
	i2c_cfg.raw = 0;
	i2c_cfg.bits.is_master_device = 1;
	i2c_cfg.bits.speed = I2C_SPEED_STANDARD;
	i2c_configure(rda5820_tx->i2c, i2c_cfg.raw);

	rda5820_tx->set_freq = rda5820_set_tx_freq;
	rda5820_tx->set_mute = rda5820_set_tx_mute;

	rda_WriteReg(RDA_REGISTER_02, 0x0002);
	rda_delay(10);

	/* power on*/
	rda_WriteReg(RDA_REGISTER_02,  0xC001);
	rda_delay(20);

	rda5820_tx_hw_init(rda5820_tx);

	fm_transmitter_device_register(rda5820_tx);

	return 0;

}

int rda5820_rx_hw_init(struct fm_rx_device *dev, void *param)
{
	u16_t chip_id;
	u16_t work_module;

	struct fm_init_para_t *init_param = (struct fm_init_para_t *)param;
	rda5820_info.fm_channel_config = init_param->channel_config;

	/* get chip id */
	chip_id = rda_ReadReg(RDA_REGISTER_00);
	if (chip_id == 0x5820) {
#ifndef CONFIG_FM_EXT_24M
		rda_WriteReg(RDA_REGISTER_02, 0xC001);
#else
		acts_clock_peripheral_enable(CLOCK_ID_FM_CLK);
		sys_write32(0, CMU_FMOUTCLK);/* 0 is HOSC ,1 is 1/2 HOSC */

		rda_WriteReg(RDA_REGISTER_02, 0xC051);
#endif
		switch(rda5820_info.fm_channel_config) {
		case CH_US_AREA :
			rda_WriteReg(RDA_REGISTER_03, 0x0000);
			break;

		case JP_AREA :
			rda_WriteReg(RDA_REGISTER_03, 0x0004);
			break;

		case EU_AREA :
			rda_WriteReg(RDA_REGISTER_03, 0x0002);
			break;

		default :
			rda_WriteReg(RDA_REGISTER_03, 0x0000);
			break;

		}

		rda_WriteReg(RDA_REGISTER_04, 0x0400);
		rda_WriteReg(RDA_REGISTER_05, 0x88FF);
		rda_WriteReg(RDA_REGISTER_14, 0x2000);
		rda_WriteReg(RDA_REGISTER_15, 0x88FE);
		rda_WriteReg(RDA_REGISTER_16, 0x4C00);
		rda_WriteReg(RDA_REGISTER_1c, 0x221C);
		rda_WriteReg(RDA_REGISTER_25, 0x0E1C);
		rda_WriteReg(RDA_REGISTER_27, 0xBB6C);
		rda_WriteReg(RDA_REGISTER_5c, 0x175C);

	}else {
		return -EINVAL;
	}

	/* set fm work module */
	work_module = rda_ReadReg(RDA_REGISTER_40);
	work_module &= 0xfff0;
	rda_WriteReg(RDA_REGISTER_40, work_module);
	return 0;
}

int rda5820_deinit(struct fm_rx_device *dev)
{
	rda_WriteReg(RDA_REGISTER_02, 0x0000);
	rda_delay(100);
	rda5820_tx_hw_init(&rda5820_info.rda5820_tx_dev);
	return 0;
}

extern int rda5820_rx_init(void)
{
	struct fm_rx_device *rda5820_rx = &rda5820_info.rda5820_rx_dev;

	rda5820_rx->i2c = device_get_binding(CONFIG_I2C_1_NAME); /* CONFIG_I2C_GPIO_1_NAME */
	if (!rda5820_rx->i2c) {
		SYS_LOG_ERR("Device not found\n");
		return -1;
	}

	 union dev_config i2c_cfg = {0};
	 i2c_cfg.raw = 0;
	 i2c_cfg.bits.is_master_device = 1;
	 i2c_cfg.bits.speed = I2C_SPEED_STANDARD;
	 i2c_configure(rda5820_rx->i2c, i2c_cfg.raw);

	rda5820_rx->init = rda5820_rx_hw_init;
	rda5820_rx->set_freq = rda5820_set_rx_freq;
	rda5820_rx->set_mute = rda5820_set_rx_mute;
	rda5820_rx->check_inval_freq = rda5820_check_freq;
	rda5820_rx->deinit = rda5820_deinit;

	fm_receiver_device_register(rda5820_rx);

	return 0;
}

//SYS_INIT(rda5820_tx_init, POST_KERNEL, 80);
