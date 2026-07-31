/********************************************************************************
 *                            USDK(US283A_master)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2018年1月12日-下午1:49:56             1.0             build this file
 ********************************************************************************/
/*!
 * \file     dcache_psram.c
 * \brief    使用DCACHE硬件访问PSRAM
 * \author
 * \version  1.0
 * \date  2018年1月12日-下午1:49:56
 *******************************************************************************/
#include <sdk.h>
#include <soc_spi.h>
#include <spi_dev.h>

#ifdef CONFIG_SOC_PSRAM_DCACHE
static const struct acts_pin_config psram_io_config[] =
{
    { 11, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3)},
    { 10, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3) },
    { 9, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3) },
    { 8, 5 | GPIO_CTL_SMIT | GPIO_CTL_PADDRV_LEVEL(3) },
};

static spi_dev_cfg_t psram_dev_cfg =
{
	//xfer instance
	NULL,

    SPI_DEFAULT_CS_PIN_NUM,

	//cs delay
	0,

	//mode
	SPIDEV_MODE3,

	//delaychain
	0,

	//MHZ
	100,
};


static void psram_dcache_init(void)
{
    spi_register_t *spi_register = (spi_register_t *)SPI1_REG_BASE;

    acts_reset_peripheral(RESET_ID_SPI1_CACHE);

    acts_clock_peripheral_enable(CLOCK_ID_SPI1_CACHE);

    sys_write32(0x4000000, SPI1_DCACHE_END_ADDR);

    sys_write32(0x2000000, SPI1_DCACHE_START_ADDR);

    //设置cpu端映射关系
    soc_memctrl_set_mapping(7, 0x2000000 | 0x0f, 0x0);

    //2M位置 设置dsp映射关系
    soc_memctrl_set_mapping(8, 0x90000000 | 0xf, 0x0);

    //bus request disable
    spi_register->ctl &= (~(1 << 15));

	// spi1 dcache enable
    sys_write32(0x01, SPI1_DCACHE_CTL);

    // set DMA access psram through dcache,避免DMA数据和CPU数据不同步
    sys_write32(sys_read32(SPI1_DCACHE_CTL) | (0x1 << 6) | (0x1 << 4), SPI1_DCACHE_CTL);

    sys_write32(0, SPI1_DCACHE_UNCACHE_ADDR_START);

    sys_write32(0x500000, SPI1_DCACHE_UNCACHE_ADDR_END);
}


static void dcache_psram_init(void)
{
	spi_host_io_param_t io_param;

	struct spi_host *host;

	host = spi_get_host_instance(1);

    io_param.is_master = TRUE;
    io_param.max_speed_mhz = 100;

    io_param.pin_config = psram_io_config;
    io_param.pins = ARRAY_SIZE(psram_io_config);

	host->api->init(host, &io_param);

	spi_cs_config(host, &psram_dev_cfg);

	host->inner_api->set_clk(host, 32);

	psram_dcache_init();
}
#else
static void dcache_ram_init(void)
{
	sys_write32(sys_read32(CMU_MEMCLKEN) | (1 << 28), CMU_MEMCLKEN);

	/* mapping psram dcache address to 0x68000000 */
	soc_memctrl_set_mapping(7, 0x68000000, 0x0);

	/* clear dcache ram */
	memset((void *)0x68000000, 0x0, 0x8000);
}
#endif

static int dcache_init(struct device *arg)
{
	ARG_UNUSED(arg);
#ifndef CONFIG_SOC_MAPPING_PSRAM_FOR_OS
#ifdef CONFIG_SOC_PSRAM_DCACHE
    dcache_psram_init();
#else
    dcache_ram_init();
#endif
#endif

    return 0;
}

SYS_INIT(dcache_init, PRE_KERNEL_1, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);
