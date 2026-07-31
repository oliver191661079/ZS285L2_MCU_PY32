/********************************************************************************
 *                            USDK(ATS350B_develop_stage2)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2020-10-30-上午10:39:36             1.0             build this file
 ********************************************************************************/
/*!
 * \file     xspi_nor_acts.h
 * \brief
 * \author
 * \version  1.0
 * \date  2020-10-30-上午10:39:36
 *******************************************************************************/

#ifndef XSPI_NOR_ACTS_H_
#define XSPI_NOR_ACTS_H_

/* spinor parameters */
#define XSPI_NOR_WRITE_PAGE_SIZE_BITS   8
#define XSPI_NOR_ERASE_SECTOR_SIZE_BITS 12
#define XSPI_NOR_ERASE_BLOCK_SIZE_BITS  16

#define XSPI_NOR_WRITE_PAGE_SIZE    (1u << XSPI_NOR_WRITE_PAGE_SIZE_BITS)
#define XSPI_NOR_ERASE_SECTOR_SIZE  (1u << XSPI_NOR_ERASE_SECTOR_SIZE_BITS)
#define XSPI_NOR_ERASE_BLOCK_SIZE   (1u << XSPI_NOR_ERASE_BLOCK_SIZE_BITS)

#define XSPI_NOR_WRITE_PAGE_MASK    (XSPI_NOR_WRITE_PAGE_SIZE - 1)
#define XSPI_NOR_ERASE_SECTOR_MASK  (XSPI_NOR_ERASE_SECTOR_SIZE - 1)
#define XSPI_NOR_ERASE_BLOCK_MASK   (XSPI_NOR_ERASE_BLOCK_SIZE - 1)

/* spinor commands */
#define XSPI_NOR_CMD_WRITE_PAGE     0x02    /* write one page */
#define XSPI_NOR_CMD_DISABLE_WRITE  0x04    /* disable write */
#define XSPI_NOR_CMD_READ_STATUS    0x05    /* read status1 */
#define XSPI_NOR_CMD_READ_STATUS2   0x35    /* read status2 */
#define XSPI_NOR_CMD_READ_STATUS3   0x15    /* read status3 */
#define XSPI_NOR_CMD_WRITE_STATUS   0x01    /* write status1 */
#define XSPI_NOR_CMD_WRITE_STATUS2  0x31    /* write status2 */
#define XSPI_NOR_CMD_WRITE_STATUS3  0x11    /* write status3 */
#define XSPI_NOR_CMD_ENABLE_WRITE   0x06    /* enable write */
#define XSPI_NOR_CMD_FAST_READ      0x0b    /* fast read */
#define XSPI_NOR_CMD_ERASE_SECTOR   0x20    /* 4KB erase */
#define XSPI_NOR_CMD_ERASE_BLOCK_32K    0x52    /* 32KB erase */
#define XSPI_NOR_CMD_ERASE_BLOCK    0xd8    /* 64KB erase */
#define XSPI_NOR_CMD_READ_CHIPID    0x9f    /* JEDEC ID */
#define XSPI_NOR_CMD_DISABLE_QSPI   0xff    /* disable QSPI */

/* NOR Flash vendors ID */
#define XSPI_NOR_MANU_ID_ALLIANCE   0x52    /* Alliance Semiconductor */
#define XSPI_NOR_MANU_ID_AMD        0x01    /* AMD */
#define XSPI_NOR_MANU_ID_AMIC       0x37    /* AMIC */
#define XSPI_NOR_MANU_ID_ATMEL      0x1f    /* ATMEL */
#define XSPI_NOR_MANU_ID_CATALYST   0x31    /* Catalyst */
#define XSPI_NOR_MANU_ID_ESMT       0x8c    /* ESMT */
#define XSPI_NOR_MANU_ID_EON        0x1c    /* EON */
#define XSPI_NOR_MANU_ID_FD_MICRO   0xa1    /* shanghai fudan microelectronics */
#define XSPI_NOR_MANU_ID_FIDELIX    0xf8    /* FIDELIX */
#define XSPI_NOR_MANU_ID_FMD        0x0e    /* Fremont Micro Device(FMD) */
#define XSPI_NOR_MANU_ID_FUJITSU    0x04    /* Fujitsu */
#define XSPI_NOR_MANU_ID_GIGADEVICE 0xc8    /* GigaDevice */
#define XSPI_NOR_MANU_ID_GIGADEVICE2    0x51    /* GigaDevice2 */
#define XSPI_NOR_MANU_ID_HYUNDAI    0xad    /* Hyundai */
#define XSPI_NOR_MANU_ID_INTEL      0x89    /* Intel */
#define XSPI_NOR_MANU_ID_MACRONIX   0xc2    /* Macronix (MX) */
#define XSPI_NOR_MANU_ID_NANTRONIC  0xd5    /* Nantronics */
#define XSPI_NOR_MANU_ID_NUMONYX    0x20    /* Numonyx, Micron, ST */
#define XSPI_NOR_MANU_ID_PMC        0x9d    /* PMC */
#define XSPI_NOR_MANU_ID_SANYO      0x62    /* SANYO */
#define XSPI_NOR_MANU_ID_SHARP      0xb0    /* SHARP */
#define XSPI_NOR_MANU_ID_SPANSION   0x01    /* SPANSION */
#define XSPI_NOR_MANU_ID_SST        0xbf    /* SST */
#define XSPI_NOR_MANU_ID_SYNCMOS_MVC    0x40    /* SyncMOS (SM) and Mosel Vitelic Corporation (MVC) */
#define XSPI_NOR_MANU_ID_TI     0x97    /* Texas Instruments */
#define XSPI_NOR_MANU_ID_WINBOND    0xda    /* Winbond */
#define XSPI_NOR_MANU_ID_WINBOND_NEX    0xef    /* Winbond (ex Nexcom) */
#define XSPI_NOR_MANU_ID_ZH_BERG    0xe0    /* ZhuHai Berg microelectronics (Bo Guan) */

#define SPINOR_FLAG_UNLOCK_IRQ_WAIT_READY		(1 << 0)

#define XSPI_INFO_FLAG_NO_IRQ_LOCK				(1 << 0)
#define XSPI_INFO_FLAG_SPI_MODE0				(1 << 1)
#define XSPI_INFO_FLAG_USE_MIO_ADDR_DATA		(1 << 4)
#define XSPI_INFO_FLAG_SPI_SUSPEND       		(1 << 2)

#define NOR_STATUS2_SUS1			(1<<7)
#define NOR_STATUS2_SUS2			(1<<2)

struct xspi_nor_info {
	/* struct must be compatible with rom code */
	struct xspi_info spi;
	u32_t chipid;
	u32_t flag;

	struct spinor_rom_operation_api *rom_api;
	u8_t clock_id;
	u8_t reset_id;
	u8_t dma_id;
	s8_t dma_chan;
	u32_t spiclk_reg;
#ifdef CONFIG_XSPI_CS_USE_GPIO
	struct device *gpio_dev;
	u32_t cs_gpio;
#endif
	u8_t enable_suspend;
};

extern u32_t rom_get_xspi_nor_api(void);

extern u32_t rom_get_xspi_prepare_hook(void);

extern u32_t rom_get_spimem_drv_api(void);

int xspi_nor_read_status(struct xspi_nor_info *sni, u8_t cmd);

void xspi_nor_write_status(struct xspi_nor_info *sni, u8_t cmd,
                  u8_t *status, s32_t len);

int xspi_nor_write(struct device *dev, off_t addr,
              const void *data, size_t len);

int xspi_nor_erase(struct device *dev, off_t addr,
              size_t size);

int xspi_nor_read(struct device *dev, off_t addr,
             void *data, size_t len);

u32_t xspi_nor_read_chipid(struct xspi_nor_info *sni);

int xspi_nor_power_ctrl(struct xspi_nor_info *sni, u8_t is_poweron);

#endif /* XSPI_NOR_ACTS_H_ */
