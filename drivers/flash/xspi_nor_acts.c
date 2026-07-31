/*
 * Copyright (c) 2017 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief SPINOR Flash driver for Actions SoC
 */

#include <errno.h>
#include <kernel.h>
#include <init.h>
#include <string.h>
#include <linker/sections.h>
#include <flash.h>
#include <soc.h>
#include <board.h>
#include <gpio.h>

#include <soc_rom_xspi.h>
#include "xspi_acts.h"
#include "xspi_mem_acts.h"

#define SYS_LOG_DOMAIN "xspi_nor"
#define SYS_LOG_LEVEL SYS_LOG_LEVEL_DEBUG
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_XSPI_NOR_ACTS
#include "xspi_nor_acts.h"


static struct xspi_nor_info system_xspi_nor;


/*
	2x/4x I/O mode, some nor send address in 1x I/O mode.
*/
const u8_t x1_address_nor_id[][4] =
{
	{0x68, 0x40, 0x15, 0},		// BY25D16
};

#ifdef CONFIG_SOC_SERIES_ANDES
__ramfunc void system_nor_prepare_hook(void)
{
#ifdef CONFIG_XSPI_NOR_ROM_ACTS
    //void (*rom_prepare_hook)(void *) = (void *)XSPI_ROM_ACTS_PREPARE_HOOK_ADDR;

    //rom_prepare_hook(&system_xspi_nor.spi);
    system_xspi_nor.spi.prepare_hook(&system_xspi_nor.spi);
    /* clear watchdog to avoid unexpected reset when erase flash */
    soc_watchdog_clear();
#else
    /* ensure spinor exit QE mode when spi controller is switched to
     * other device on the same bus */
    xspi_mem_continuous_read_reset(&system_xspi_nor.spi);
#endif
}
#endif

#ifdef CONFIG_NOR_ACTS_DATA_PROTECTION_ENABLE
extern int nor_write_protection(const struct device *dev, bool enable);
extern int nor_write_protection_region(const struct device *dev, int region_id);

#endif
static int xspi_nor_write_protection(struct device *dev, bool enable)
{
#ifdef CONFIG_NOR_ACTS_DATA_PROTECTION_ENABLE
	nor_write_protection(dev, enable);
#else
	ARG_UNUSED(dev);
	ARG_UNUSED(enable);
#endif

	return 0;
}

static int xspi_nor_write_protection_region(struct device *dev, int region_id)
{
#ifdef CONFIG_NOR_ACTS_DATA_PROTECTION_ENABLE
	nor_write_protection_region(dev, region_id);
#else
	ARG_UNUSED(dev);
	ARG_UNUSED(region_id);
#endif

	return 0;
}


void spinor_resume_finished(struct xspi_nor_info *sni);

void xspi_nor_dump_info(struct xspi_nor_info *sni)
{
#ifdef CONFIG_XSPI_NOR_ACTS_DUMP_INFO
	u32_t chipid, status, status2, status3;

	chipid = xspi_nor_read_chipid(sni);
	SYS_LOG_INF("SPINOR: chipid: 0x%02x 0x%02x 0x%02x",
		chipid & 0xff,
		(chipid >> 8) & 0xff,
		(chipid >> 16) & 0xff);

	status = xspi_nor_read_status(sni, XSPI_NOR_CMD_READ_STATUS);
	status2 = xspi_nor_read_status(sni, XSPI_NOR_CMD_READ_STATUS2);
	status3 = xspi_nor_read_status(sni, XSPI_NOR_CMD_READ_STATUS3);

#ifdef CONFIG_XSPI_NOR_SUSPEND_RESUME
	if(status2 & (NOR_STATUS2_SUS1|NOR_STATUS2_SUS2)){
		SYS_LOG_INF("nor is suspend, wait resume finished\n");
		uint32_t key = irq_lock();
		spinor_resume_finished(sni);
		irq_unlock(key);
	}
#endif

	SYS_LOG_INF("SPINOR: status: 0x%02x 0x%02x 0x%02x",
		status, status2, status3);
#endif
}


static void xspi_nor_enable_status_qe(struct xspi_nor_info *sni)
{
	uint16_t status;

	/* MACRONIX's spinor has different QE bit */
	if (XSPI_NOR_MANU_ID_MACRONIX == (sni->chipid & 0xff)) {
		status = xspi_nor_read_status(sni, XSPI_NOR_CMD_READ_STATUS);
		if (!(status & 0x40)) {
			/* set QE bit to disable HOLD/WP pin function */
			status |= 0x40;
			xspi_nor_write_status(sni, XSPI_NOR_CMD_WRITE_STATUS,
					    (u8_t *)&status, 1);
		}

		return;
	}

	/* check QE bit */
	status = xspi_nor_read_status(sni, XSPI_NOR_CMD_READ_STATUS2);
	if (!(status & 0x2)) {
		/* set QE bit to disable HOLD/WP pin function, for WinBond */
		status |= 0x2;
		xspi_nor_write_status(sni, XSPI_NOR_CMD_WRITE_STATUS2,
				    (u8_t *)&status, 1);

		/* check QE bit again */
		status = xspi_nor_read_status(sni, XSPI_NOR_CMD_READ_STATUS2);
		if (!(status & 0x2)) {
			/* oh, let's try old write status cmd, for GigaDevice/Berg */
			status = ((status | 0x2) << 8) |
				 xspi_nor_read_status(sni, XSPI_NOR_CMD_READ_STATUS);
			xspi_nor_write_status(sni, XSPI_NOR_CMD_WRITE_STATUS,
					    (u8_t *)&status, 2);

		}
	}
}

int xspi_crc_error_exception(void)
{
    //if crc error
    if(sys_read32(SPI0_REG_BASE + XSPI_STATUS) & 0x02){

        //clear error PD and error status
        sys_write32(sys_read32(SPI0_REG_BASE + XSPI_STATUS), (SPI0_REG_BASE + XSPI_STATUS));

        printk("spi sta: %x\n", sys_read32(SPI0_REG_BASE + XSPI_STATUS));

        printk("crc err: %x\n", sys_read32(SPI0_REG_BASE + XSPI_CACHE_ERR_ADDR));

        return 0;
    }

    return 1;
}


static const struct flash_driver_api xspi_nor_api = {
	.read = xspi_nor_read,
	.write = xspi_nor_write,
	.erase = xspi_nor_erase,
	.write_protection = xspi_nor_write_protection,
	.write_protection_region = xspi_nor_write_protection_region,
};

void acts_xspi_nor_init_internal(struct xspi_nor_info *sni)
{
	struct xspi_info *si = (struct xspi_info *)&sni->spi;
	u32_t key;
	int i;

	key = irq_lock();

	if (si->bus_width > 1)
	{
		for (i = 0; i < ARRAY_SIZE(x1_address_nor_id); i++)
		{
			if (0 == memcmp(&sni->chipid, x1_address_nor_id[i], 3))
			{
				SYS_LOG_INF("spinor uses 1x io mode");
				xspi_setup_1x_address(si);
				break;
			}
		}
	}

	if (si->bus_width == 4) {
		xspi_nor_enable_status_qe(sni);

		/* enable 4x mode */
		xspi_setup_bus_width(si, 4);
	} else if (si->bus_width == 2) {
		/* enable 2x mode */
		xspi_setup_bus_width(si, 2);
	} else {
		/* enable 1x mode */
		xspi_setup_bus_width(si, 1);
	}

	soc_freq_set_spi_clk(xspi_controller_id(si), si->freq_mhz);

#if 0
	/* setup spi0 clock to 78MHz, coreclk/2 */
#if CONFIG_SOC_CPU_CLK_FREQ < 160000
	/* corepll / 2 */
	sys_write32((sys_read32(CMU_SPICLK) & ~0xff) | 0x01, CMU_SPICLK);
#else
	/* corepll / 3 */
	//sys_write32((sys_read32(CMU_SPICLK) & ~0xff) | 0x02, CMU_SPICLK);
	sys_write32(0x81, CMU_SPICLK);
#endif
#endif

	xspi_setup_delaychain(si, si->delay_chain);

	irq_unlock(key);
}

#ifdef CONFIG_SOC_SERIES_ANDESC

void atcs_xspi_nor_power_ctrl(bool poweron)
{
	u32_t flag = irq_lock();
	xspi_nor_power_ctrl(&system_xspi_nor, poweron);
	irq_unlock(flag);
}

int acts_xspi_nor_enable_suspend(uint8_t enable)
{
	u32_t flag = irq_lock();
	system_xspi_nor.enable_suspend = enable;
	irq_unlock(flag);

	return 0;
}

#endif

int acts_xspi_nor_init(struct device *dev)
{
	system_xspi_nor.rom_api = (void *)rom_get_xspi_nor_api();

	system_xspi_nor.spi.prepare_hook = (void *)rom_get_xspi_prepare_hook();
	struct xspi_nor_info *sni = (struct xspi_nor_info *)dev->driver_data;

#ifdef CONFIG_SOC_SERIES_ANDES
#ifndef CONFIG_XSPI_NOR_ROM_ACTS
	xspi_mem_prepare_hook_install(system_nor_prepare_hook);
#endif
#endif

#ifdef CONFIG_SOC_SERIES_ANDESC
    system_xspi_nor.spi.spimem_op_api = (const struct spimem_operation_api *)rom_get_spimem_drv_api();
#endif

	sni->chipid = xspi_nor_read_chipid(sni);

	//xspi_nor_write_protection(dev, true);
	xspi_nor_write_protection_region(dev, FLASH_PROTECT_REGION_ALL_EXCLUDE_NVRAM);

	xspi_nor_dump_info(sni);

	acts_xspi_nor_init_internal(sni);

//	dev->driver_api = &xspi_nor_api;

	return 0;
}

/* system XIP spinor */
#ifdef CONFIG_XSPI_NOR_ROM_ACTS
static struct xspi_nor_info system_xspi_nor = {
	.rom_api = (void *)NULL,
	.spi = {
		.base = SPI0_REG_BASE,
		.bus_width = CONFIG_XSPI_NOR_ACTS_IO_BUS_WIDTH,
		.delay_chain = CONFIG_XSPI_NOR_ACTS_DELAY_CHAIN,
		.freq_mhz = CONFIG_XSPI_ACTS_FREQ_MHZ,
		.flag = 0,
		.dma_base = 0,

		.prepare_hook = (void *)NULL,
#ifdef CONFIG_SOC_SERIES_ANDESC
		.sys_irq_lock_cb = sys_irq_lock,
		.sys_irq_unlock_cb = sys_irq_unlock,
#endif
	},
#ifdef CONFIG_XSPI_NOR_AUTO_CHECK_RB
	.flag = SPINOR_FLAG_UNLOCK_IRQ_WAIT_READY,
#else
	.flag = 0,
#endif
};
#else
static struct xspi_nor_info system_xspi_nor = {
	.spi = {
		.base = SPI0_REG_BASE,
		.spiclk_reg = CMU_SPICLK,
		.bus_width = CONFIG_XSPI_NOR_ACTS_IO_BUS_WIDTH,
		.delay_chain = CONFIG_XSPI_NOR_ACTS_DELAY_CHAIN,
		.freq_mhz = CONFIG_XSPI_ACTS_FREQ_MHZ,
		.flag = 0,
	}

	.clock_id = CLOCK_ID_SPI0,
	.reset_id = RESET_ID_SPI0,
	.dma_id = DMA_ID_SPI0,
	.dma_chan = -1,
};
#endif
bool state = false;
DEVICE_AND_API_INIT(xspi_nor_acts, CONFIG_XSPI_NOR_ACTS_DEV_NAME, acts_xspi_nor_init,
	     &system_xspi_nor, NULL, PRE_KERNEL_1,
	     CONFIG_XSPI_NOR_ACTS_DEV_INIT_PRIORITY, &xspi_nor_api);

#ifdef CONFIG_XSPI1_NOR_ACTS

K_MUTEX_DEFINE(flash_1_mutex);

static int xspi_nor1_read(struct device *dev, off_t addr,
             void *data, size_t len)
{
    int ret;

	k_mutex_lock(&flash_1_mutex, K_FOREVER);

	ret = xspi_nor_read(dev, addr, data, len);

	k_mutex_unlock(&flash_1_mutex);

    return ret;
}

static int xspi_nor1_write(struct device *dev, off_t addr,
              const void *data, size_t len)
{
    int ret;

	k_mutex_lock(&flash_1_mutex, K_FOREVER);

	ret = xspi_nor_write(dev, addr, data, len);

	k_mutex_unlock(&flash_1_mutex);

    return ret;
}

static int xspi_nor1_erase(struct device *dev, off_t addr,
              size_t size)
{
    int ret;

	k_mutex_lock(&flash_1_mutex, K_FOREVER);

	ret = xspi_nor_erase(dev, addr, size);

	k_mutex_unlock(&flash_1_mutex);

    return ret;
}

static const struct flash_driver_api xspi_nor1_api = {
	.read = xspi_nor1_read,
	.write = xspi_nor1_write,
	.erase = xspi_nor1_erase,
	.write_protection = xspi_nor_write_protection,
	.write_protection_region = xspi_nor_write_protection_region,
};

#ifdef BOARD_SPI1_NOR_CS_GPIO
void xspi1_nor_set_cs(struct xspi_info *sri, int value)
{
	if (value) {
		xspi_set_bits(sri, XSPI_CTL, XSPI_CTL_SS, XSPI_CTL_SS);
		sys_write32(GPIO_BIT(BOARD_SPI1_NOR_CS_GPIO), GPIO_REG_BSR(GPIO_REG_BASE, BOARD_SPI1_NOR_CS_GPIO));

#ifdef BOARD_SPI1_PSRAM_CS_GPIO
		/* spi1_ss restore to spi cs function */
		sys_write32((sys_read32(GPIO_CTL(BOARD_SPI1_PSRAM_CS_GPIO)) & ~GPIO_CTL_GPIO_OUTEN) | 0x5,
			GPIO_CTL(BOARD_SPI1_PSRAM_CS_GPIO));
#endif
	} else {
#ifdef BOARD_SPI1_PSRAM_CS_GPIO
		/*spi1_ss switch to gpio function and output high level */
		sys_write32(GPIO_BIT(BOARD_SPI1_PSRAM_CS_GPIO), GPIO_REG_BSR(GPIO_REG_BASE, BOARD_SPI1_PSRAM_CS_GPIO));
		sys_write32((sys_read32(GPIO_CTL(BOARD_SPI1_PSRAM_CS_GPIO)) & ~GPIO_CTL_MFP_MASK) | GPIO_CTL_GPIO_OUTEN,
			GPIO_CTL(BOARD_SPI1_PSRAM_CS_GPIO));
#endif

		sys_write32(GPIO_BIT(BOARD_SPI1_NOR_CS_GPIO), GPIO_REG_BRR(GPIO_REG_BASE, BOARD_SPI1_NOR_CS_GPIO));
		xspi_set_bits(sri, XSPI_CTL, XSPI_CTL_SS, 0);
	}
	if (!state) {
		state = true;
	}
}
#endif

static struct xspi_nor_info xspi1_nor = {
	.rom_api = NULL,
	.spi = {
		.base = SPI1_REG_BASE,
		.bus_width = 4,
		.delay_chain = 6,
		.freq_mhz = CONFIG_XSPI1_ACTS_FREQ_MHZ,
		.flag = XSPI_FLAG_NO_IRQ_LOCK,
		//.flag = 0,
		//.dma_base = 0xc0040800,
		.dma_base = 0,

		.prepare_hook = NULL,
#ifdef BOARD_SPI1_NOR_CS_GPIO
		.set_cs = xspi1_nor_set_cs,
#endif
	},
};

int acts_xspi1_nor_init(struct device *dev)
{


	xspi1_nor.rom_api = (void *)rom_get_xspi_nor_api();

	xspi1_nor.spi.prepare_hook = (void *)rom_get_xspi_prepare_hook();
	struct xspi_nor_info *sni = (struct xspi_nor_info *)dev->driver_data;

#ifdef CONFIG_SOC_SERIES_ANDESC
    xspi1_nor.spi.spimem_op_api = (const struct spimem_operation_api *)rom_get_spimem_drv_api();
#endif

#ifdef BOARD_SPI1_NOR_CS_GPIO
	sys_write32(GPIO_BIT(BOARD_SPI1_NOR_CS_GPIO), GPIO_REG_BSR(GPIO_REG_BASE, BOARD_SPI1_NOR_CS_GPIO));
	sys_write32(sys_read32(GPIO_CTL(BOARD_SPI1_NOR_CS_GPIO)) | GPIO_CTL_GPIO_OUTEN,
		GPIO_CTL(BOARD_SPI1_NOR_CS_GPIO));
#endif

#ifndef CONFIG_SOC_MAPPING_PSRAM_FOR_OS
	acts_clock_peripheral_enable(CLOCK_ID_SPI1);
	acts_reset_peripheral(RESET_ID_SPI1);
#endif

	sni->chipid = xspi_nor_read_chipid(sni);
	xspi_nor_dump_info(sni);

	acts_xspi_nor_init_internal(sni);

	//dev->driver_api = &xspi_nor_api;

	return 0;
}

DEVICE_AND_API_INIT(spi1_xspi_nor_acts, CONFIG_XSPI1_NOR_ACTS_DEV_NAME, acts_xspi1_nor_init,
	     &xspi1_nor, NULL, PRE_KERNEL_1,
	     CONFIG_XSPI1_NOR_ACTS_DEV_INIT_PRIORITY, &xspi_nor1_api);
#endif /* CONFIG_XSPI1_NOR_ACTS*/
