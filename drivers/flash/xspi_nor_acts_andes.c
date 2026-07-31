/********************************************************************************
 *                            USDK(ATS350B_develop_stage2)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2020-10-30-上午10:40:30             1.0             build this file
 ********************************************************************************/
/*!
 * \file     xspi_nor_acts_andes.c
 * \brief
 * \author
 * \version  1.0
 * \date  2020-10-30-上午10:40:30
 *******************************************************************************/
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
#include "xspi_nor_acts.h"

__XSPI_RAMFUNC int xspi_nor_read_status(struct xspi_nor_info *sni, u8_t cmd)
{
    if (sni->rom_api)
        return sni->rom_api->read_status(&sni->spi, cmd);
    else
        return xspi_mem_read_status(&sni->spi, cmd);
}

__XSPI_RAMFUNC static int xspi_nor_wait_ready(struct xspi_nor_info *sni)
{
    u8_t status;

    while (1) {
        status = xspi_nor_read_status(sni, XSPI_NOR_CMD_READ_STATUS);
        if (!(status & 0x1))
            break;
    }

    return 0;
}

#ifdef CONFIG_XSPI_NOR_AUTO_CHECK_RB
__ramfunc void xspi_nor_auto_check_rb(u32_t key, struct xspi_info *spi)
{
#if defined(CONFIG_SYS_IRQ_LOCK)
    SYS_IRQ_FLAGS  flags;
#endif

    /* switch to normal mode */
    xspi_set_bits(spi, XSPI_CTL, XSPI_CTL_AHB_REQ, XSPI_CTL_AHB_REQ);
    /* enable auto check RB */
    xspi_write(spi, XSPI_CACHE_MODE, XSPI_CACHE_MODE_CHECK_RB_EN | XSPI_CACHE_MODE_RMS);
    /* switch to cache mode */
    xspi_set_bits(spi, XSPI_CTL, XSPI_CTL_AHB_REQ, 0);

#if defined(CONFIG_SYS_IRQ_LOCK)
    sys_irq_lock(&flags);
#endif

    irq_unlock(key);

    /* wait spinor ready */
    while (xspi_read(spi, XSPI_CACHE_MODE) & XSPI_CACHE_MODE_RB_STATUS) {
        ;
    }

#if defined(CONFIG_SYS_IRQ_LOCK)
    sys_irq_unlock(&flags);
#endif
}
#endif

__XSPI_RAMFUNC static void xspi_nor_write_data(struct xspi_nor_info *sni, u8_t cmd,
            u32_t addr, s32_t addr_len, const u8_t *buf, s32_t len)
{
    struct xspi_info *si = &sni->spi;
    u32_t key = 0;

    if (xspi_need_lock_irq(si)) {
        key = irq_lock();
    }

    xspi_mem_set_write_protect(si, false);
    xspi_mem_transfer(si, cmd, addr, addr_len, (u8_t *)buf, len,
            0, XSPI_MEM_TFLAG_WRITE_DATA);
    xspi_nor_wait_ready(sni);

    if (sni->flag & SPINOR_FLAG_UNLOCK_IRQ_WAIT_READY) {
        if (xspi_need_lock_irq(si))
            irq_unlock(key);

        xspi_nor_wait_ready(sni);
    } else {
        xspi_nor_wait_ready(sni);

        if (xspi_need_lock_irq(si))
            irq_unlock(key);
    }
}




void xspi_nor_write_status(struct xspi_nor_info *sni, u8_t cmd,
                  u8_t *status, s32_t len)
{
#ifdef CONFIG_XSPI_NOR_AUTO_CHECK_RB
    if (xspi_controller_id(&sni->spi) == 0)
        k_sched_lock();
#endif

    if (sni->rom_api)
        sni->rom_api->write_status(&sni->spi, cmd, status, len);
    else
        xspi_nor_write_data(sni, cmd, 0, 0, status, len);

#ifdef CONFIG_XSPI_NOR_AUTO_CHECK_RB
    if (xspi_controller_id(&sni->spi) == 0)
        k_sched_unlock();
#endif
}

static int xspi_nor_read_internal(struct xspi_nor_info *sni,
            u32_t addr, u8_t *buf, int len)
{
    xspi_mem_read_page(&sni->spi, addr, 3, buf, len);
    return 0;
}


static int xspi_nor_write_internal(struct xspi_nor_info *sni, off_t addr,
                   const void *data, size_t len)
{
    size_t unaligned_len, remaining, write_size;

    /* unaligned write? */
    if (addr & XSPI_NOR_WRITE_PAGE_MASK)
        unaligned_len = XSPI_NOR_WRITE_PAGE_SIZE - (addr & XSPI_NOR_WRITE_PAGE_MASK);
    else
        unaligned_len = 0;

    remaining = len;
    while (remaining > 0) {
        if (unaligned_len) {
            /* write unaligned page data */
            if (unaligned_len > len)
                write_size = len;
            else
                write_size = unaligned_len;
            unaligned_len = 0;
        } else if (remaining < XSPI_NOR_WRITE_PAGE_SIZE)
            write_size = remaining;
        else
            write_size = XSPI_NOR_WRITE_PAGE_SIZE;

        xspi_nor_write_data(sni, XSPI_NOR_CMD_WRITE_PAGE, addr, 3, data, write_size);

        addr += write_size;
        data += write_size;
        remaining -= write_size;
    }

    return 0;
}

int xspi_nor_write(struct device *dev, off_t addr,
              const void *data, size_t len)
{
    struct xspi_nor_info *sni =
        (struct xspi_nor_info *)dev->driver_data;

    if (!len)
        return 0;

#ifdef CONFIG_XSPI_NOR_AUTO_CHECK_RB
    if (xspi_controller_id(&sni->spi) == 0)
        k_sched_lock();
#endif

    if (sni->rom_api)
        sni->rom_api->write(&sni->spi, addr, data, len);
    else
        xspi_nor_write_internal(sni, addr, data, len);

#ifdef CONFIG_XSPI_NOR_AUTO_CHECK_RB
    if (xspi_controller_id(&sni->spi) == 0)
        k_sched_unlock();
#endif

    return 0;
}

static int xspi_nor_erase_internal(struct xspi_nor_info *sni, off_t addr, size_t size)
{
    size_t remaining, erase_size;
    u8_t cmd;

    /* write aligned page data */
    remaining = size;
    while (remaining > 0) {
        if (addr & XSPI_NOR_ERASE_BLOCK_MASK || remaining < XSPI_NOR_ERASE_BLOCK_SIZE) {
            cmd = XSPI_NOR_CMD_ERASE_SECTOR;
            erase_size = XSPI_NOR_ERASE_SECTOR_SIZE;
        } else {
            cmd = XSPI_NOR_CMD_ERASE_BLOCK;
            erase_size = XSPI_NOR_ERASE_BLOCK_SIZE;
        }

        xspi_nor_write_data(sni, cmd, addr, 3, NULL, 0);

        addr += erase_size;
        remaining -= erase_size;
    }

    return 0;
}

int xspi_nor_erase(struct device *dev, off_t addr,
              size_t size)
{
    struct xspi_nor_info *sni =
        (struct xspi_nor_info *)dev->driver_data;

    if (!size)
        return 0;

    if (addr & XSPI_NOR_ERASE_SECTOR_MASK || size & XSPI_NOR_ERASE_SECTOR_MASK)
        return -EINVAL;

#ifdef CONFIG_XSPI_NOR_AUTO_CHECK_RB
    if (xspi_controller_id(&sni->spi) == 0)
        k_sched_lock();
#endif

    soc_watchdog_clear();

    if (sni->rom_api)
        sni->rom_api->erase(&sni->spi, addr, size);
    else
        xspi_nor_erase_internal(sni, addr, size);

#ifdef CONFIG_XSPI_NOR_AUTO_CHECK_RB
    if (xspi_controller_id(&sni->spi) == 0)
        k_sched_unlock();
#endif

    return 0;
}


u32_t xspi_nor_read_chipid(struct xspi_nor_info *sni)
{
    u32_t chipid;

    if (sni->rom_api)
        chipid = sni->rom_api->read_chipid(&sni->spi);
    else
        xspi_mem_read_chipid(&sni->spi, &chipid, 3);

    return chipid;
}


int xspi_nor_read(struct device *dev, off_t addr,
             void *data, size_t len)
{
    struct xspi_nor_info *sni =
        (struct xspi_nor_info *)dev->driver_data;

    if (!len)
        return 0;

    if (sni->rom_api)
        sni->rom_api->read(&sni->spi, addr, data, len);
    else
        xspi_nor_read_internal(sni, addr, data, len);

    return 0;
}
