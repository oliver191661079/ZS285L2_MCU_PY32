#include <zephyr.h>
#include <init.h>
#include <board.h>
#include <i2c.h>
#include <device.h>
#include <string.h>
#include <misc/printk.h>
#include <fm.h>
#include <stdlib.h>

#ifdef CONFIG_NVRAM_CONFIG
#include <nvram_config.h>
#endif

#define SYS_LOG_DOMAIN "FM"
#define SYS_LOG_LEVEL SYS_LOG_LEVEL_INFO
#include <logging/sys_log.h>

#define  ID_QN8027		0x40
#define  QN8027_ADDR    0x2C

//register defines
#define REG_SYSTEM     0x00
#define REG_CH1        0x01
#define REG_GPLT       0x02
#define REG_XTL        0x03
#define REG_VGA        0x04
#define REG_CID1       0x05
#define REG_CID2       0x06

/* Driver instance data */
static struct fm_tx_device qn8027_info;
/*********************************************************************************
 * \         int QND_WriteReg(u8_t RegAddr, u8_t Data)
 * \
 * \Description
 * \
 * \Arguments :  RegAddr
 * \Returns :   0 success -1 fail
 * \
 * \Notes :
 * \
*********************************************************************************/
int QND_TX_WriteReg(struct fm_device *dev, u8_t RegAddr, u8_t data)
{

	u8_t wr_addr[2];
	u8_t wr_data[2];
	struct i2c_msg msgs[2];
	int ret = 0;

	wr_addr[0] = RegAddr;

	msgs[0].buf = wr_addr;
	msgs[0].len = 1;
	msgs[0].flags = I2C_MSG_WRITE;

	memset(wr_data, 0, sizeof(wr_data));
	wr_data[0] = data;
	msgs[1].buf = wr_data;
	msgs[1].len = 1;
	msgs[1].flags = I2C_MSG_WRITE;

	ret = i2c_transfer(dev->i2c, &msgs[0], 2, QN8027_ADDR);

	return ret;
}
/*********************************************************************************
 * \           u8_t QND_ReadReg(u8_t RegAddr)
 * \
 * \Description
 * \
 * \Arguments :  RegAddr
 * \Returns :
 * \
 * \Notes :
 * \
*********************************************************************************/
static u8_t QND_TX_ReadReg(struct fm_device *dev, u8_t RegAddr)
{
	struct i2c_msg msgs[2];
	u8_t wr_addr[2];
	u8_t wr_data[2];
	int ret = 0;
	int try_cnt = 10;

	wr_addr[0] = RegAddr;

	msgs[0].buf = wr_addr;
	msgs[0].len = 1;
	msgs[0].flags = I2C_MSG_WRITE;

	memset(wr_data, 0, sizeof(wr_data));
	msgs[1].buf = wr_data;
	msgs[1].len = 1;
	msgs[1].flags = I2C_MSG_READ | I2C_MSG_RESTART;

	do {
		ret = i2c_transfer(dev->i2c, &msgs[0], 2, QN8027_ADDR);
	} while(ret != 0 && try_cnt-- > 0);

	return wr_data[0];
}

static void QND_SetRegBit(struct fm_device *dev, u8_t reg, u8_t bitMask, u8_t data_val)
{
    u8_t temp;

    temp = QND_TX_ReadReg(dev, reg);

    temp &= (u8_t)(~bitMask);
    temp |= data_val;

    QND_TX_WriteReg(dev, reg, temp);
}


int qn8027_set_mute(struct fm_device *dev, bool flag)
{
    if(flag)
        QND_SetRegBit(dev, REG_SYSTEM, 0x08, 0x08); //bit3 = 1;
    else
        QND_SetRegBit(dev, REG_SYSTEM, 0x08, 0x00); //bit3 = 0;

    k_busy_wait(20 * 1000);

	return 0;
}


int qn8027_set_freq(struct fm_device *dev , u16_t freq)
{
    u8_t temp1,freq_msb,freq_lsb;
    u16_t temp;

    if (freq > 1080 || freq < 875) {
        freq = 875;
    }

    temp = (freq - 760) * 2;
    freq_msb = (u8_t)(temp >> 8);
    freq_lsb = (u8_t) temp;

    temp1 = QND_TX_ReadReg(dev, REG_SYSTEM)&0xfc;

    QND_TX_WriteReg(dev, REG_SYSTEM, (freq_msb | temp1));
    QND_TX_WriteReg(dev, REG_CH1, freq_lsb);

	SYS_LOG_INF(" freq : %d MHZ ",freq);

	return 0;
}


static void qn8027_hw_init(struct fm_device *dev)
{
	k_busy_wait(20 * 1000); //delay 20 ms

    QND_SetRegBit(dev, REG_SYSTEM, 0x80, 0x80); //

    k_busy_wait(20 * 1000); //delay 20 ms

#ifdef CONFIG_FM_EXT_24M
	QND_SetRegBit(dev, REG_XTL, 0xC0, 0x40);     //inject single-end sin wave clock from XTAL1
#else
    QND_SetRegBit(dev, REG_XTL, 0xC0, 0x00);     //inject single-end sin wave clock from XTAL1
#endif

    QND_SetRegBit(dev, REG_VGA, 0x80, 0x80);     //R04[7]=1,XTAL1 inject frequency is 24MHZ

    //recalibration
    QND_SetRegBit(dev, REG_SYSTEM, 0x40, 0x40);  //R00[6]=1
    QND_SetRegBit(dev, REG_SYSTEM, 0x40, 0x00);  //R00[6]=0

    k_busy_wait(20 * 1000);//delay 20 ms Do recalibration

    QND_TX_WriteReg(dev, 0x18, 0xe4); //imporve SNR performance
    QND_TX_WriteReg(dev, 0x1b, 0xf0); //imporve SNR performance

    QND_SetRegBit(dev, REG_VGA, 0x7F, 0x32);     //RIN 10k, input gain 0dB, Tx gain 0dB


    QND_TX_WriteReg(dev, REG_GPLT, 0xb9);   //disable RF PA off function

    QND_TX_WriteReg(dev, REG_SYSTEM, 0x22); //transmitting

    k_busy_wait(20 * 1000);


}

int qn8027_init(struct device *dev)
{
	int default_freq = 910;
	char temp[4] = { 0 };
	struct fm_tx_device *qn8027 = &qn8027_info;

	qn8027->i2c = device_get_binding(CONFIG_I2C_0_NAME);
	if (!qn8027->i2c) {
		SYS_LOG_ERR("Device not found\n");
		return -1;
	}

	union dev_config i2c_cfg;
	i2c_cfg.raw = 0;
	i2c_cfg.bits.is_master_device = 1;
	i2c_cfg.bits.speed = I2C_SPEED_STANDARD;
	i2c_configure(qn8027->i2c, i2c_cfg.raw);

	qn8027->set_freq = qn8027_set_freq;
	qn8027->set_mute = qn8027_set_mute;

	fm_transmitter_device_register(&qn8027_info);

	qn8027_hw_init(qn8027);

#ifdef CONFIG_NVRAM_CONFIG
	if(nvram_config_get("FM_DEFAULT_FREQ", temp, sizeof(temp)) < 0){
		default_freq = 910;
	}else {
		default_freq = atoi(temp);
	}
#endif

    fm_transmitter_set_freq(default_freq);
	return 0;
}

SYS_INIT(qn8027_init, POST_KERNEL, 80);
