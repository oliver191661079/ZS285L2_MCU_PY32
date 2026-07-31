#include <zephyr.h>
#include <init.h>
#include <board.h>
#include <i2c.h>
#include <device.h>
#include <string.h>
#include <misc/printk.h>
#include <fm.h>
#include <stdlib.h>
#include <logging/sys_log.h>

#ifdef CONFIG_NVRAM_CONFIG
#include <nvram_config.h>
#endif

/* Driver instance data */
static struct fm_rx_device qn8035_info;

#include "fm_qn8035.h"

/*
 step
 1: set leap step to (FM)100kHz / 10kHz(AM)
 2: set leap step to (FM)200kHz / 1kHz(AM)
 0:  set leap step to (FM)50kHz / 9kHz(AM)
 */
static u8_t auto_scan_step = 0;

static u8_t qnd_div1;
static u8_t qnd_div2;
static u8_t qnd_nco;
static u8_t qnd_Chip_VID;
static u8_t qnd_xtal_div0;
static u8_t qnd_xtal_div1;
static u8_t qnd_xtal_div2;


typedef struct {
	u8_t dev_addr;
	u8_t reg_addr;
} i2c_addr_t;

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
	msgs[1].flags = I2C_MSG_READ|I2C_MSG_RESTART;

	return i2c_transfer(dev, &msgs[0], 2, addr->dev_addr);
}

u8_t QND_WriteReg(u8_t Regis_Addr, u8_t Data)
{
	u8_t i;
	int result = 0;
	u8_t writebuffer[2];
	i2c_addr_t addr;

	addr.reg_addr = Regis_Addr;
	addr.dev_addr = I2C_DEV0_ADDRESS;
	writebuffer[0] = Data;

	for (i = 0; i < 10; i++) {
		if (i2c_write_dev(qn8035_info.i2c, &addr, writebuffer, 1) == 0 {
			break;
		}
	}
	if (i == 10) {
		SYS_LOG_INF("FM QN write failed\n");
		result = -1;
	}

	return result;
}


u8_t QND_ReadReg(u8_t Regis_Addr)
{
	u8_t Data = 0;
	u8_t init_cnt = 0;
	u8_t ret = 0;

	i2c_addr_t addr;

	addr.reg_addr = Regis_Addr;
	addr.dev_addr = I2C_DEV0_ADDRESS;

	for (init_cnt = 0; init_cnt < 10; init_cnt++) {
		ret = i2c_read_dev(qn8035_info.i2c, &addr, &Data, 1);
		if (ret == 0) {
			break;
		}
	}

	if (init_cnt == 10) {
		Data = 0xff;
	}

	return Data;
}


/**********************************************************************
 void QNF_RXInit(void)
 **********************************************************************
 Description: set to SNR based MPX control. Call this function before
 tune to one specific channel

 Parameters:
 None
 Return Value:
 None
 **********************************************************************/
void QNF_RXInit(void)
{
	QNF_SetRegBit(0x1b, 0x08, 0x00); /* Let NFILT adjust freely */
	QNF_SetRegBit(0x2c, 0x3f, 0x12);
	QNF_SetRegBit(0x1d, 0x40, 0x00);
	QNF_SetRegBit(0x41, 0x0f, 0x0a);
	QND_WriteReg(0x45, 0x50);
	QNF_SetRegBit(0x3e, 0x80, 0x80);
	QNF_SetRegBit(0x41, 0xe0, 0xc0);
#if (qnd_ChipID == CHIPSUBID_QN8035A0) {
		QNF_SetRegBit(0x42, 0x10, 0x10);
	}
#endif
}

/**********************************************************************
 void QNF_SetMute(u8_t On)
 **********************************************************************
 Description: set register specified bit

 Parameters:
 On:        1: mute, 0: unmute
 Return Value:
 None
 **********************************************************************/
void QNF_SetMute(u8_t On)
{
	if (On != 0) {

#if (qnd_ChipID == CHIPSUBID_QN8035A0) {
		QNF_SetRegBit(REG_DAC, 0x0b, 0x0b);
	}
#else
	{
		QNF_SetRegBit(0x4a, 0x20, 0x20);
	}
#endif
	} else {
#if (qnd_ChipID == CHIPSUBID_QN8035A0)
	{
		QNF_SetRegBit(REG_DAC, 0x0b, 0x00);
	}
#else
	{
		QNF_SetRegBit(0x4a, 0x20, 0x00);
	}
#endif
	}
}

/**********************************************************************
 void QNF_SetRegBit(u8_t reg, u8_t bitMask, u8_t data_val)
 **********************************************************************
 Description: set register specified bit

 Parameters:
 reg:        register that will be set
 bitMask:    mask specified bit of register
 data_val:    data will be set for specified bit
 Return Value:
 None
 **********************************************************************/
void QNF_SetRegBit(u8_t reg, u8_t bitMask, u8_t data_val)
{
	u8_t temp;

	temp = QND_ReadReg(reg);
	temp &= (u8_t) (~bitMask);
	temp |= data_val & bitMask;
	QND_WriteReg(reg, temp);
}

/**********************************************************************
 u16_t QNF_GetCh(void)
 **********************************************************************
 Description: get current channel frequency

 Parameters:
 None
 Return Value:
 channel frequency
 **********************************************************************/
u16_t  QNF_GetCh(void)
{
	u8_t tCh;
	u8_t tStep;
	u16_t ch = 0;
	/* set to reg: CH_STEP */
	tStep = QND_ReadReg(CH_STEP);
	tStep &= CH_CH;
	ch = tStep;
	tCh = QND_ReadReg(CH);
	ch = (ch << 8) + tCh;
	return CHREG2FREQ(ch);
}

/**********************************************************************
 void QNF_SetCh(u16_t freq)
 **********************************************************************
 Description: set channel frequency

 Parameters:
 freq:  channel frequency to be set
 Return Value:
 None
 **********************************************************************/
void QNF_SetCh(u16_t freq)
{
	u8_t tStep;
	u8_t tCh;
	u16_t f;
	/* u16_t pll_dlt; */

	if (freq == 8550) {
		/* QND_WriteReg(XTAL_DIV1, QND_XTAL_DIV1_855); */
		/* QND_WriteReg(XTAL_DIV2, QND_XTAL_DIV2_855); */
		QND_WriteReg(NCO_COMP_VAL, 0x69);
		freq = 8570;
	} else {
		QND_WriteReg(XTAL_DIV1, qnd_div1);
		QND_WriteReg(XTAL_DIV2, qnd_div2);
		QND_WriteReg(NCO_COMP_VAL, qnd_nco);
	}
	/* Manually set RX Channel index */
	QNF_SetRegBit(SYSTEM1, CCA_CH_DIS, CCA_CH_DIS);
	f = FREQ2CHREG(freq);
	/* set to reg: CH */
	tCh = (u8_t) f;
	QND_WriteReg(CH, tCh);
	/* set to reg: CH_STEP */
	tStep = QND_ReadReg(CH_STEP);
	tStep &= ~CH_CH;
	tStep |= ((u8_t) (f >> 8) & CH_CH);
	QND_WriteReg(CH_STEP, tStep);
}

/**********************************************************************
 void QNF_ConfigScan(u16_t start,u16_t stop, u8_t step)
 **********************************************************************
 Description: config start, stop, step register for FM/AM CCA or CCS

 Parameters:
 start
 Set the frequency (10kHz) where scan to be started,
 e.g. 7600 for 76.00MHz.
 stop
 Set the frequency (10kHz) where scan to be stopped,
 e.g. 10800 for 108.00MHz
 step
 1: set leap step to (FM)100kHz / 10kHz(AM)
 2: set leap step to (FM)200kHz / 1kHz(AM)
 0:  set leap step to (FM)50kHz / 9kHz(AM)
 Return Value:
 None
 **********************************************************************/
void  QNF_ConfigScan(u16_t start, u16_t stop, u8_t step)
{
	/* calculate ch para */
	u8_t tStep = 0;
	u8_t tS;
	u16_t fStart;
	u16_t fStop;

	fStart = FREQ2CHREG(start);
	fStop = FREQ2CHREG(stop);
	/* set to reg: CH_START */
	tS = (u8_t) fStart;
	QND_WriteReg(CH_START, tS);
	tStep |= ((u8_t) (fStart >> 6) & CH_CH_START);
	/* set to reg: CH_STOP */
	tS = (u8_t) fStop;
	QND_WriteReg(CH_STOP, tS);
	tStep |= ((u8_t) (fStop >> 4) & CH_CH_STOP);
	/* set to reg: CH_STEP */
	tStep |= step << 6;
	QND_WriteReg(CH_STEP, tStep);
}

/**********************************************************************
 int QN_ChipInitialization(void)
 **********************************************************************
 Description: chip first step initialization, called only by QND_Init()

 Parameters:
 None
 Return Value:
 None
 **********************************************************************/
int QN_ChipInitialization(void)
{
	u8_t chipid;
	u8_t IsQN8035B;

	QND_WriteReg(SYSTEM1, 0x81);
	QND_Delay(50);

	chipid = QND_ReadReg(CID2);
	qnd_Chip_VID = (QND_ReadReg(CID1) & 0x03);
	IsQN8035B = QND_ReadReg(0x58) & 0x1f;

	SYS_LOG_INF("chipid 0x%x qnd_Chip_VID 0x%x IsQN8035B %d\n", chipid, qnd_Chip_VID, IsQN8035B);

	if ((chipid != CHIPID_QN8035) || (IsQN8035B != 0x13)) {
		SYS_LOG_ERR("chip id err\n");
		return -1;
	}

	QND_WriteReg(0x58, 0x13);

	#if QND_INPUT_CLOCK_TYPE == QND_CRYSTAL_CLOCK /* 32.768k */
	{
		/* Following is where change the input clock type.as crystal or oscillator. */
		QNF_SetRegBit(0x58, 0x80, QND_CRYSTAL_CLOCK);
		/* Following is where change the input clock wave type,as sine-wave or square-wave. */
		QNF_SetRegBit(0x01, 0x80,  QND_SINE_WAVE_CLOCK);
		qnd_xtal_div0 = 0x01;
		qnd_xtal_div1 = 0x08;
		qnd_xtal_div2 = 0x5C;
	}
	#elif QND_INPUT_CLOCK_TYPE == QND_INPUT_CLOCK /* cpu 24M */
	{
		/* Following is where change the input clock type.as crystal or oscillator. */
		QNF_SetRegBit(0x58, 0x80, QND_INPUT_CLOCK);
		/* Following is where change the input clock wave type,as sine-wave or square-wave. */
		QNF_SetRegBit(0x01, 0x80,  QND_DIGITAL_CLOCK);
		qnd_xtal_div0 = 0xdc;/* 0x8D; */
		qnd_xtal_div1 = 0x02;/* 0x89; */
		qnd_xtal_div2 = 0x54;/* 0x65; */
	}
	#endif


	/* Following is where change the input clock frequency. */
	QND_WriteReg(XTAL_DIV0, qnd_xtal_div0);
	QND_WriteReg(XTAL_DIV1, qnd_xtal_div1);
	QND_WriteReg(XTAL_DIV2, qnd_xtal_div2);
	QND_Delay(10);
	/********User sets chip working clock end ********/
	QND_WriteReg(0x54, 0x47); /* mod PLL setting */
	QND_WriteReg(SMP_HLD_THRD, 0xc4); /* select SNR as filter3,SM step is 2db */
	QNF_SetRegBit(0x40, 0x70, 0x70);
	QND_WriteReg(0x33, 0x9c); /* set HCC Hystersis to 5db */
	QND_WriteReg(0x2d, 0xd6); /* notch filter threshold adjusting */
	QND_WriteReg(0x43, 0x10); /* notch filter threshold enable */
	QNF_SetRegBit(SMSTART, 0x7f, SMSTART_VAL);
	QNF_SetRegBit(SNCSTART, 0x7f, SNCSTART_VAL);
	QNF_SetRegBit(HCCSTART, 0x7f, HCCSTART_VAL);
	if (qnd_Chip_VID == CHIPSUBID_QN8035A1) {
		QNF_SetRegBit(0x47, 0x0c, 0x08);
	}
	/* these variables are used in QNF_SetCh() function. */
	qnd_div1 = QND_ReadReg(XTAL_DIV1);
	qnd_div2 = QND_ReadReg(XTAL_DIV2);
	qnd_nco = QND_ReadReg(NCO_COMP_VAL);
	/* libc_print("fm-test",(qnd_div1<<8)+qnd_div2,2); */

	return 0;
}

/**********************************************************************
 void QND_TuneToCH(u16_t ch)
 **********************************************************************
 Description: Tune to the specific channel. call QND_SetSysMode() before
 call this function
 Parameters:
 ch
 Set the frequency (10kHz) to be tuned,
 e.g. 101.30MHz will be set to 10130.
 Return Value:
 None
 **********************************************************************/
void  QND_TuneToCH(u16_t ch)
{
	u8_t reg;
	u8_t imrFlag = 0;

	QNF_SetRegBit(REG_REF, ICPREF, 0x0a);
	QNF_RXInit();
#if (qnd_IsQN8035B == 0x13)
	{
		if ((ch == 7630) || (ch == 8580) || (ch == 9340) || (ch == 9390) || (ch == 9530) || (ch == 9980)
				|| (ch == 10480)) {
			imrFlag = 1;
		}
	}
#elif((qnd_ChipID == CHIPSUBID_QN8035A0) || (qnd_ChipID == CHIPSUBID_QN8035A1))
	{
		if ((ch == 6910) || (ch == 7290) || (ch == 8430)) {
			imrFlag = 1;
		} else if (qnd_ChipID == CHIPSUBID_QN8035A1) {
			if ((ch == 7860) || (ch == 10710)) {
				imrFlag = 1;
			}
		}
	}
#endif
	if (imrFlag == 1) {
		QNF_SetRegBit(CCA, IMR, IMR);
	} else {
		QNF_SetRegBit(CCA, IMR, 0x00);
	}
	QNF_ConfigScan(ch, ch, auto_scan_step);
	QNF_SetCh(ch);

#if ((qnd_ChipID == CHIPSUBID_QN8035A0) || (qnd_ChipID == CHIPSUBID_QN8035A1))
	{
		QNF_SetRegBit(SYSTEM1, CHSC, CHSC);
	}
#else
	{
		QNF_SetRegBit(0x55, 0x80, 0x80);
		QND_Delay(ENABLE_2K_SPEED_PLL_DELAY);
		QNF_SetRegBit(0x55, 0x80, 0x00);
	}
#endif

	/* Auto tuning */
	QND_WriteReg(0x4f, 0x80);
	reg = QND_ReadReg(0x4f);
	reg >>= 1;
	QND_WriteReg(0x4f, reg);
#if (qnd_ChipID == CHIPSUBID_QN8035A0)
	{
		QND_Delay(300);
	}
#endif
	QND_Delay(CH_SETUP_DELAY_TIME / 10);
	QNF_SetRegBit(REG_REF, ICPREF, 0x00);
}

/***********************************************************************
 void QND_RXSetTH(u8_t th)
 ***********************************************************************
 Description: Setting the threshold value of automatic scan channel
 th:
 Setting threshold for quality of channel to be searched,
 the range of th value:CCA_SENSITIVITY_LEVEL_0 ~ CCA_SENSITIVITY_LEVEL_9
 Return Value:
 None
 ***********************************************************************/
static const u16_t rssi_snr_TH_tbl[10] = {

	CCA_SENSITIVITY_LEVEL_0, CCA_SENSITIVITY_LEVEL_1,
	CCA_SENSITIVITY_LEVEL_2, CCA_SENSITIVITY_LEVEL_3,
	CCA_SENSITIVITY_LEVEL_4, CCA_SENSITIVITY_LEVEL_5,
	CCA_SENSITIVITY_LEVEL_6, CCA_SENSITIVITY_LEVEL_7,
	CCA_SENSITIVITY_LEVEL_8, CCA_SENSITIVITY_LEVEL_9
};

void QND_RXSetTH(u8_t th)
{
	u8_t rssiTH;
	u8_t snrTH;
	u16_t rssi_snr_TH;

	rssi_snr_TH = rssi_snr_TH_tbl[th];
	rssiTH = (u8_t) (rssi_snr_TH >> 8);
	snrTH = (u8_t) (rssi_snr_TH & 0xff);
	QND_WriteReg(0x4f, 0x00); /* enable auto tunning in CCA mode */
	QNF_SetRegBit(REG_REF, ICPREF, 0x0a);
	QNF_SetRegBit(GAIN_SEL, 0x08, 0x08); /* NFILT program is enabled */
	/* selection filter:filter3 */
	QNF_SetRegBit(CCA1, 0x30, 0x30);
	/* Enable the channel condition filter3 adaptation,Let ccfilter3 adjust freely */
	QNF_SetRegBit(SYSTEM_CTL2, 0x40, 0x00);
	QND_WriteReg(CCA_CNT1, 0x00);
	QNF_SetRegBit(CCA_CNT2, 0x3f, 0x03);
	/* selection the time of CCA FSM wait SNR calculator to settle:20ms */
	/* 0x00:     20ms(default) */
	/* 0x40:     40ms */
	/* 0x80:     60ms */
	/* 0xC0:     100m */
	QNF_SetRegBit(CCA_SNR_TH_1, 0xc0, 0x00);
	/* selection the time of CCA FSM wait RF front end and AGC to settle:20ms */
	/* 0x00:     10ms */
	/* 0x40:     20ms(default) */
	/* 0x80:     40ms */
	/* 0xC0:     60ms */
	QNF_SetRegBit(CCA_SNR_TH_2, 0xc0, 0x40);
	QNF_SetRegBit(CCA, 0x3f, rssiTH); /* setting RSSI threshold for CCA */
	QNF_SetRegBit(CCA_SNR_TH_1, 0x3f, snrTH); /* setting SNR threshold for CCA */
}

/***********************************************************************
 int QND_RXValidCH(u16_t freq, u8_t step);
 ***********************************************************************
 Description: to validate a ch (frequency)(if it's a valid channel)
 Freq: specific channel frequency, unit: 10Khz
 e.g. 108.00MHz will be set to 10800.
 Step:
 FM:
 QND_FMSTEP_100KHZ: set leap step to 100kHz
 QND_FMSTEP_200KHZ: set leap step to 200kHz
 QND_FMSTEP_50KHZ:  set leap step to 50kHz
 Return Value:
 0: not a valid channel
 1: a valid channel at this frequency
 -1:chip does not normally work.
 ***********************************************************************/
int  QND_RXValidCH(u16_t freq)
{
	u8_t regValue;
	u8_t timeOut = 200;
	/* for fake channel judge */
	u8_t snr, readCnt, stereoCount;
	int isValidChannelFlag;

	QNF_ConfigScan(freq, freq, 1);
	/* enter CCA mode,channel index is decided by internal CCA */
	QNF_SetRegBit(SYSTEM1, RXCCA_MASK, RX_CCA);
	do {
		regValue = QND_ReadReg(SYSTEM1);
		/* delay 5ms */
		QND_Delay(5);

		timeOut--;
	} while ((regValue & CHSC) && timeOut); /* when seeking a channel or time out,be quited the loop */
	/* read out the rxcca_fail flag of RXCCA status */
	regValue = QND_ReadReg(STATUS1) & 0x08;
	if (regValue != 0) {
		isValidChannelFlag = -1;
	} else {
		isValidChannelFlag = 0;
	}
	/* special search  handle ways for fake channel */
	if (isValidChannelFlag == 0) {
		snr = QND_ReadReg(SNR);
		/* delay 60ms */
		QND_Delay(60);

		if (snr <= 25) {
			stereoCount = 0;
			for (readCnt = 0; readCnt < 10; readCnt++) {
				QND_Delay(2);
				if ((QND_ReadReg(STATUS1) & 0x01) == 0) {
					stereoCount++;
				}

				if (stereoCount >= 3) {
					isValidChannelFlag = 1;
					break;
				}
			}
		}
	}

	return isValidChannelFlag;

}

int  QND_RXSeekCH(u16_t start, u16_t stop, u8_t step)
{
#if (HARD_SEEK != 0)
	QNF_ConfigScan(start, stop, step);
	/* enter CCA mode,channel index is decided by internal CCA */
	QNF_SetRegBit(SYSTEM1, RXCCA_MASK, RX_CCA);
#endif
	return 0;
}

int qn8035_set_freq(struct fm_rx_device *dev, u16_t freq)
{
	u8_t		i;
	u16_t	result, temp;

	temp = freq;
	printk("set freq -- %d\n", freq);
	QNF_SetMute(1);
	for (i = 0; i < 3; i++) {
		QND_TuneToCH(freq);
		result = QNF_GetCh();
		if (result == temp) {
			SYS_LOG_INF("listening fm %d khz", QNF_GetCh());
			break;
		}
	}

	QNF_SetMute(0);

	return 0;

}

int qn8035_check_freq(struct fm_rx_device *dev, u16_t freq)
{
	int ret = 0;

	QNF_SetMute(1);
	QND_RXSetTH(2);
	ret = QND_RXValidCH(freq);
	SYS_LOG_INF("freq %d  ret %d ", freq, ret);

	if (freq == 10300) {
		ret = 0;
	}
	return ret;
}

int qn8035_set_mute(struct fm_rx_device *dev, bool flag)
{
	QNF_SetMute(flag);
	return 0;
}

static int qn8035_hw_init(struct fm_rx_device *dev)
{
	s8_t ret;

#ifndef CONFIG_FM_EXT_24M
	acts_clock_peripheral_enable(CLOCK_ID_FM_CLK);
#if defined(CONFIG_SOC_SERIES_ANDES) || defined(CONFIG_SOC_SERIES_ANDESM)
	sys_write32(1, CMU_FMCLK);/* 0 is 1/2 HOSC ,1 is HOSC */
#elif defined(CONFIG_SOC_SERIES_WOODPECKER)
	/* TODO */
#endif
#endif
	ret = QN_ChipInitialization();
	if (ret == 0) {
		/* enter RX mode */
		QNF_SetRegBit(SYSTEM1, STNBY_RX_MASK, RX_MODE);
		/* set mute */
		QNF_SetMute(true);
		QNF_SetRegBit(VOL_CTL, 0x3f, 0x07);
		QNF_SetRegBit(VOL_CTL, 0x40, 0x40);

		QND_RXSetTH(3);
	}

	return ret;
}

int qn8035_deinit(struct fm_rx_device *dev)
{
	QND_WriteReg(SYSTEM1, 0x21);
	QND_Delay(5);

#ifndef CONFIG_FM_EXT_24M
	acts_clock_peripheral_disable(CLOCK_ID_FM_CLK);
#endif
	return 0;
}

int qn8035_init(void)
{
	struct fm_rx_device *qn8035 = &qn8035_info;

	qn8035->i2c = device_get_binding(CONFIG_I2C_0_NAME); /* CONFIG_I2C_GPIO_1_NAME */
	if (!qn8035->i2c) {
		SYS_LOG_ERR("Device not found\n");
		return -1;
	}

	union dev_config i2c_cfg;
	i2c_cfg.raw = 0;
	i2c_cfg.bits.is_master_device = 1;
	i2c_cfg.bits.speed = I2C_SPEED_STANDARD;
	i2c_configure(qn8035->i2c, i2c_cfg.raw);

	qn8035->init = qn8035_hw_init;
	qn8035->set_freq = qn8035_set_freq;
	qn8035->set_mute = qn8035_set_mute;
	qn8035->check_inval_freq = qn8035_check_freq;
	qn8035->deinit = qn8035_deinit;

	fm_receiver_device_register(qn8035);

	return 0;
}