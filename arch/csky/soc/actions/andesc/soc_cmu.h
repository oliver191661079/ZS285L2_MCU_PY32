/********************************************************************************
 *                            USDK(ZS283A)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2018-10-12-上午11:58:29             1.0             build this file
 ********************************************************************************/
/*!
 * \file     soc_cmu.h
 * \brief
 * \author
 * \version  1.0
 * \date  2018-10-12-上午11:58:29
 *******************************************************************************/

#ifndef SOC_CMU_H_
#define SOC_CMU_H_

#define     CMU_HOSC_CTL                                                     (CMU_ANALOG_REG_BASE+0x00)
#define     CMU_COREPLL_CTL                                                  (CMU_ANALOG_REG_BASE+0x04)
#define     CMU_SPLL_CTL                                                     (CMU_ANALOG_REG_BASE+0x08)
#define     CMU_APLL0_CTL                                                    (CMU_ANALOG_REG_BASE+0x0C)
#define     CMU_APLL1_CTL                                                    (CMU_ANALOG_REG_BASE+0x10)
#define     CMU_COREPLL_DEBUG                                                (CMU_ANALOG_REG_BASE+0x20)
#define     CMU_SPLL_DEBUG                                                   (CMU_ANALOG_REG_BASE+0x24)
#define     CMU_APLL0_DEBUG                                                  (CMU_ANALOG_REG_BASE+0x28)
#define     CMU_APLL1_DEBUG                                                  (CMU_ANALOG_REG_BASE+0x2C)
#define     CMU_ANADEBUG                                                     (CMU_ANALOG_REG_BASE+0x40)


/* CMU Digital registers */
#define     CMU_SYSCLK                                                        (CMU_DIGITAL_REG_BASE+0x0000)
#define     CMU_DEVCLKEN0                                                     (CMU_DIGITAL_REG_BASE+0x0004)
#define     CMU_DEVCLKEN1                                                     (CMU_DIGITAL_REG_BASE+0x0008)
#define     CMU_ASRCCLK                                                       (CMU_DIGITAL_REG_BASE+0x0010)
#define     CMU_ADDACLK                                                       (CMU_DIGITAL_REG_BASE+0x0014)
#define     CMU_I2SCLK                                                        (CMU_DIGITAL_REG_BASE+0x0018)
#define     CMU_SPDIFCLK                                                      (CMU_DIGITAL_REG_BASE+0x001C)
#define     CMU_SDCLK                                                         (CMU_DIGITAL_REG_BASE+0x0020)
#define     CMU_SPICLK                                                        (CMU_DIGITAL_REG_BASE+0x0024)
#define     CMU_IRCLK                                                         (CMU_DIGITAL_REG_BASE+0x0028)
#define     CMU_LCDCLK                                                        (CMU_DIGITAL_REG_BASE+0x002C)
#define     CMU_SEGLCDCLK                                                     (CMU_DIGITAL_REG_BASE+0x0030)
#define     CMU_FMCLK                                                         (CMU_DIGITAL_REG_BASE+0x0034)
#define     CMU_PWM0CLK                                                       (CMU_DIGITAL_REG_BASE+0x0038)
#define     CMU_PWM1CLK                                                       (CMU_DIGITAL_REG_BASE+0x003C)
#define     CMU_PWM2CLK                                                       (CMU_DIGITAL_REG_BASE+0x0040)
#define     CMU_PWM3CLK                                                       (CMU_DIGITAL_REG_BASE+0x0044)
#define     CMU_PWM4CLK                                                       (CMU_DIGITAL_REG_BASE+0x0048)
#define     CMU_PWM5CLK                                                       (CMU_DIGITAL_REG_BASE+0x004C)
#define     CMU_PWM6CLK                                                       (CMU_DIGITAL_REG_BASE+0x0050)
#define     CMU_PWM7CLK                                                       (CMU_DIGITAL_REG_BASE+0x0054)
#define     CMU_PWM8CLK                                                       (CMU_DIGITAL_REG_BASE+0x0058)
#define     CMU_LRADCCLK                                                      (CMU_DIGITAL_REG_BASE+0x005C)
#define     CMU_TIMERCLK                                                      (CMU_DIGITAL_REG_BASE+0x0060)
#define     CMU_MEMCLKEN                                                      (CMU_DIGITAL_REG_BASE+0x0080)
#define     CMU_MEMCLKEN1                                                     (CMU_DIGITAL_REG_BASE+0x0084)
#define     CMU_MEMCLKSEL                                                     (CMU_DIGITAL_REG_BASE+0x0088)
#define     CMU_HCL3K2_CTL                                                    (CMU_DIGITAL_REG_BASE+0x0090)
#define     CMU_HCL200K_CTL                                                   (CMU_DIGITAL_REG_BASE+0x0094)
#define     CMU_HCLHOSCCS                                                     (CMU_DIGITAL_REG_BASE+0x009C)
#define     CMU_DIGITALDEBUG                                                  (CMU_DIGITAL_REG_BASE+0x00A0)
#define     CMU_DSP_WAIT                                                      (CMU_DIGITAL_REG_BASE+0x00B0)
#define     CMU_DSP_AUDIO_VOLCLK_SEL                                          (CMU_DIGITAL_REG_BASE+0x00C0)
#define     CMU_TEST_CTL                                                      (CMU_DIGITAL_REG_BASE+0x00F0)


#define     CMU_DSP_WAIT_DSPWEN                                               31
#define     CMU_DSP_WAIT_PSU_DSP_IDLE                                         3
#define     CMU_DSP_WAIT_PSU_DSP_COREIDLE                                     2
#define     CMU_DSP_WAIT_DSP_WAIT_AFTER_MPU                                   1
#define     CMU_DSP_WAIT_DSPDEWS                                              0

#define     CMU_SYSCLK_CORECLKDIV_SHIFT                                       4
#define     CMU_SYSCLK_CORE_CLKSEL_MASK                                       (0x3<<0)


/* CMU ADDACLK bits */
#define CMU_ADDACLK_ADCCLKSRC		12
#define CMU_ADDACLK_ADCCLKPREDIV    11
#define CMU_ADDACLK_ADCCLKDIV_e     10
#define CMU_ADDACLK_ADCCLKDIV_SHIFT  8
#define CMU_ADDACLK_ADCCLKDIV_MASK  (0x7 << CMU_ADDACLK_ADCCLKDIV_SHIFT)
#define CMU_ADDACLK_DACCLKSRC		 4
#define CMU_ADDACLK_DACCLKPREDIV     3
#define CMU_ADDACLK_DACCLKDIV_e      2
#define CMU_ADDACLK_DACCLKDIV_SHIFT  0
#define CMU_ADDACLK_DACCLKDIV_MASK  (0x7 << CMU_ADDACLK_DACCLKDIV_SHIFT)

/* CMU I2SCLK bits */
#define CMU_I2SCLK_I2SRDCLKSRC_SHIFT		28
#define CMU_I2SCLK_I2SRDCLKSRC_MASK			(0x03 << CMU_I2SCLK_I2SRDCLKSRC_SHIFT)
#define CMU_I2SCLK_I2SRX1MCLKEXTREV			26
#define CMU_I2SCLK_I2SRX1MCLKSRC_SHIFT		24
#define CMU_I2SCLK_I2SRX1MCLKSRC_MASK		(0x03 << CMU_I2SCLK_I2SRX1MCLKSRC_SHIFT)
#define CMU_I2SCLK_I2S1CLKSRC				20
#define CMU_I2SCLK_I2S1CLKPREDIV			19
#define CMU_I2SCLK_I2S1CLKDIV_SHIFT			16
#define CMU_I2SCLK_I2S1CLKDIV_MASK			(0x07 << CMU_I2SCLK_I2S1CLKDIV_SHIFT)
#define CMU_I2SCLK_I2SRX0MCLKEXTREV			14
#define CMU_I2SCLK_I2SRX0MCLKSRC_SHIFT		12
#define CMU_I2SCLK_I2SRX0MCLKSRC_MASK		(0x03 << CMU_I2SCLK_I2SRX0MCLKSRC_SHIFT)
#define CMU_I2SCLK_I2STX0MCLKEXTREV			11
#define CMU_I2SCLK_I2STX0MCLKDACSRC			10
#define CMU_I2SCLK_I2STX0MCLKSRC_SHIFT		8
#define CMU_I2SCLK_I2STX0MCLKSRC_MASK		(0x03 << CMU_I2SCLK_I2STX0MCLKSRC_SHIFT)
#define CMU_I2SCLK_I2S0CLKSRC				4
#define CMU_I2SCLK_I2S0CLKPREDIV			3
#define CMU_I2SCLK_I2S0CLKDIV_SHIFT			0
#define CMU_I2SCLK_I2S0CLKDIV_MASK			(0x07 << CMU_I2SCLK_I2S0CLKDIV_SHIFT)

/* CMU SPDIFCLK bits */
#define CMU_SPDIFCLK_SPDIFTXCLKSRC_SHIFT		4
#define CMU_SPDIFCLK_SPDIFTXCLKSRC_MASK			(0x03 << CMU_SPDIFCLK_SPDIFTXCLKSRC_SHIFT)
#define CMU_SPDIFCLK_SPDIFRXCLKSRC_SHIFT		2
#define CMU_SPDIFCLK_SPDIFRXCLKSRC_MASK			(0x03 << CMU_SPDIFCLK_SPDIFRXCLKSRC_SHIFT)
#define CMU_SPDIFCLK_SPDIFRXCLKDIV_SHIFT		0
#define CMU_SPDIFCLK_SPDIFRXCLKDIV_MASK			(0x03 << CMU_SPDIFCLK_SPDIFRXCLKDIV_SHIFT)


/* CMU MEMCLKEN bits */
#define CMU_MEMCLKEN_URAM2CLKEN      27
#define CMU_MEMCLKEN_URAM1CLKEN      25
#define CMU_MEMCLKEN_URAM0CLKEN      23
#define CMU_MEMCLKEN_ASRCBUF1CLKEN   20
#define CMU_MEMCLKEN_ASRCBUF0CLKEN   19
#define CMU_MEMCLKEN_PCMRAM6CLKEN	 18
#define CMU_MEMCLKEN_PCMRAM5CLKEN	 17
#define CMU_MEMCLKEN_PCMRAM4CLKEN	 16
#define CMU_MEMCLKEN_PCMRAM3CLKEN	 15
#define CMU_MEMCLKEN_PCMRAM2CLKEN	 14
#define CMU_MEMCLKEN_PCMRAM1CLKEN	 13
#define CMU_MEMCLKEN_PCMRAM0CLKEN	 12

/* CMU MEMCLKSEL bits */
#define CMU_MEMCLKSEL_URAM2CLKSEL    27
#define CMU_MEMCLKSEL_URAM1CLKSEL    25
#define CMU_MEMCLKSEL_URAM0CLKSEL    23
#define CMU_MEMCLKSEL_ASRCBUF1CLKSEL 20
#define CMU_MEMCLKSEL_ASRCBUF0CLKSEL 19
#define CMU_MEMCLKSEL_PCMRAM6CLKSEL  18
#define CMU_MEMCLKSEL_PCMRAM5CLKSEL  17
#define CMU_MEMCLKSEL_PCMRAM4CLKSEL  16
#define CMU_MEMCLKSEL_PCMRAM3CLKSEL  15
#define CMU_MEMCLKSEL_PCMRAM2CLKSEL  14
#define CMU_MEMCLKSEL_PCMRAM1CLKSEL  13
#define CMU_MEMCLKSEL_PCMRAM0CLKSEL  12

/* CMU AUDIOPLL0 CTL bits */
#define CMU_AUDIOPLL0_CTL_PMD         8
#define CMU_AUDIOPLL0_CTL_MODE        5
#define CMU_AUDIOPLL0_CTL_EN          4
#define CMU_AUDIOPLL0_CTL_APS0_SHIFT  0
#define CMU_AUDIOPLL0_CTL_APS0(x)    ((x) << CMU_AUDIOPLL0_CTL_APS0_SHIFT)
#define CMU_AUDIOPLL0_CTL_APS0_MASK	  CMU_AUDIOPLL0_CTL_APS0(0xF)

/* CMU AUDIOPLL1 CTL bits */
#define CMU_AUDIOPLL1_CTL_PMD         8
#define CMU_AUDIOPLL1_CTL_MODE        5
#define CMU_AUDIOPLL1_CTL_EN          4
#define CMU_AUDIOPLL1_CTL_APS1_SHIFT  0
#define CMU_AUDIOPLL1_CTL_APS1(x)    ((x) << CMU_AUDIOPLL1_CTL_APS1_SHIFT)
#define CMU_AUDIOPLL1_CTL_APS1_MASK	  CMU_AUDIOPLL1_CTL_APS1(0xF)


//S2模式低频运行频率
#define CMU_SYSCLK_CLKSEL_32K       0x0
//较低主频下的工作频率，CK3M由内部产生，会比
//CK24M省电
#define CMU_SYSCLK_CLKSEL_3M        0x1
//CORE PLL 正常工作的频率
#define CMU_SYSCLK_CLKSEL_COREPLL   0x2
//CK24M 启动时系统设置的时钟
#define CMU_SYSCLK_CLKSEL_HOSC      0x3
//CK64M 蓝牙时钟
#define CMU_SYSCLK_CLKSEL_64M       0x4

//corepll允许最小频率
#define MIN_CMU_COREPLL_MHZ         (36)

//卡控制器初始化所需要的频率
#define     MMC_LOW_FREQ				(0)

#define     MMC_LOW_FREQ_VAL            (0x11)

#define     CMU_SDCLK_SD0CLKSRC             6
#define     CMU_SDCLK_SD1CLKDIV_SHIFT       8

#define     CMU_SYSCLK_CPUCLKDIV_SHIFT      8

#define     CMU_SPICLK_SPI0CLKSRC_SHIFT     6
#define     CMU_SPICLK_SPI0CLKDIV_SHIFT     0
#define     CMU_SPICLK_SPI1CLKDIV_SHIFT     8
#define     CMU_SPICLK_SPI2CLKDIV_SHIFT     16


typedef enum
{
    MEM_CLK_ROM = 0,
    MEM_CLK_PCMRAM0 = 1,
    MEM_CLK_PCMRAM1 = 2,
    MEM_CLK_PCMRAM2 = 3,
    MEM_CLK_ASRCBUF0 = 4,
    MEM_CLK_ASRCBUF1 = 5,
    MEM_CLK_SD0BUF = 6,
    MEM_CLK_URAM0 = 7,
    MEM_CLK_URAM1 = 8,
    MEM_CLK_SD1BUF = 9,
    MEM_CLK_SPICACHERAM = 10,
    MEM_CLK_PCMRAM3 = 11,
    MEM_CLK_PCMRAM4 = 12,
    MEM_CLK_PCMRAM5 = 13,
    MEM_CLK_PCMRAM6 = 14,
} mem_clk_type_e;

void cmu_corepll_clk_set(unsigned int corepll_mhz);
unsigned int cmu_core_periph_clk_get_mhz(uint32_t core_type);
void cmu_asrc_clk_set(unsigned int corepll_clk, unsigned int asrc_clk);

extern int soc_get_hosc_cap(void);
extern void soc_set_hosc_cap(int cap);
void soc_cmu_init(void);


#endif /* SOC_CMU_H_ */
