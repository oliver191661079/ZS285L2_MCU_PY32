/********************************************************************************
 *                            USDK(ATS350B_develop_stage2)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2020-10-30-上午10:59:20             1.0             build this file
 ********************************************************************************/
/*!
 * \file     xspi_nor_acts_andesc.c
 * \brief
 * \author
 * \version  1.0
 * \date  2020-10-30-上午10:59:20
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


#ifdef CONFIG_XSPI_NOR_SUSPEND_RESUME
K_MUTEX_DEFINE(xip_nor_w_mutex);
#endif

__ramfunc int xspi_nor_read_status(struct xspi_nor_info *sni, u8_t cmd)
{
    if (sni->rom_api) {
        return sni->rom_api->read_status(&sni->spi, cmd);
    } else {
#ifdef CONFIG_XSPI_MEM_ACTS
        return xspi_mem_read_status(&sni->spi, cmd);
#else
        return -EIO;
#endif
    }
}

#ifdef CONFIG_XSPI_MEM_ACTS
__ramfunc static int xspi_nor_wait_ready(struct xspi_nor_info *sni)
{
    u8_t status;

    while (1) {
        status = xspi_nor_read_status(sni, XSPI_NOR_CMD_READ_STATUS);
        if (!(status & 0x1))
            break;
    }

    return 0;
}

__ramfunc static void xspi_nor_write_data(struct xspi_nor_info *sni, u8_t cmd,
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
#endif

void xspi_nor_write_status(struct xspi_nor_info *sni, u8_t cmd,
                  u8_t *status, s32_t len)
{
#ifdef CONFIG_XSPI_NOR_AUTO_CHECK_RB
    if (xspi_controller_id(&sni->spi) == 0){
		k_sched_lock();
	}
#endif

    if (sni->rom_api) {
        sni->rom_api->write_status(&sni->spi, cmd, status, len);
    } else {
#ifdef CONFIG_XSPI_MEM_ACTS
        xspi_nor_write_data(sni, cmd, 0, 0, status, len);
#endif
    }

#ifdef CONFIG_XSPI_NOR_AUTO_CHECK_RB
    if (xspi_controller_id(&sni->spi) == 0){
		k_sched_unlock();
	}
#endif
}



#ifdef CONFIG_XSPI_MEM_ACTS
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
#endif

int xspi_nor_write(struct device *dev, off_t addr,
              const void *data, size_t len)
{
    struct xspi_nor_info *sni =
        (struct xspi_nor_info *)dev->driver_data;

    if (!len)
        return 0;

#ifdef CONFIG_XSPI_NOR_AUTO_CHECK_RB
    if (xspi_controller_id(&sni->spi) == 0){
		k_sched_lock();
#ifdef CONFIG_XSPI_NOR_SUSPEND_RESUME
		k_mutex_lock(&xip_nor_w_mutex, K_FOREVER);
#endif
	}
#endif

    if (sni->rom_api) {
		if ((u32_t)addr & (1 << 31))
			sni->rom_api->write_rdm(&sni->spi, (addr & ~(1 << 31)), data, len);
		else
        	sni->rom_api->write(&sni->spi, addr, data, len);
	} else {
#ifdef CONFIG_XSPI_MEM_ACTS
        xspi_nor_write_internal(sni, addr, data, len);
#endif
    }

#ifdef CONFIG_XSPI_NOR_AUTO_CHECK_RB
    if (xspi_controller_id(&sni->spi) == 0){
#ifdef CONFIG_XSPI_NOR_SUSPEND_RESUME
		k_mutex_unlock(&xip_nor_w_mutex);
#endif
		k_sched_unlock();
    }
#endif

    return 0;
}

#ifdef CONFIG_XSPI_MEM_ACTS
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
#endif

#ifdef CONFIG_XSPI_NOR_SUSPEND_RESUME

#define	SPINOR_CMD_PROGRAM_ERASE_RESUME		0x7a  	/* nor resume */
#define	SPINOR_CMD_PROGRAM_ERASE_SUSPEND	0x75   /* nor suspend */

__ramfunc static void xspi_nor_write_cmd(struct xspi_nor_info *sni, unsigned char cmd)
{
	struct xspi_info *spi = &sni->spi;

    spi->spimem_op_api->transfer((struct spi_info *)spi, cmd, 0, 0, 0, 0, 0, 0);
}

__ramfunc void spinor_enable_autoRB(struct xspi_info *spi)
{
    /* switch to normal mode */
    xspi_set_bits(spi, XSPI_CTL, XSPI_CTL_AHB_REQ, XSPI_CTL_AHB_REQ);
    /* enable auto check RB */
    xspi_write(spi, XSPI_CACHE_MODE, XSPI_CACHE_MODE_CHECK_RB_EN | XSPI_CACHE_MODE_RMS);
    /* switch to cache mode */
    xspi_set_bits(spi, XSPI_CTL, XSPI_CTL_AHB_REQ, 0);
}

__ramfunc int spinor_auto_check_busy(struct xspi_info *spi)
{
	return (xspi_read(spi, XSPI_CACHE_MODE) & XSPI_CACHE_MODE_RB_STATUS);
}


__ramfunc static int spinor_suspend(uint32_t key, struct xspi_nor_info *sni)
{
	int i, j, ret_val;

	//int start_time, end_time;

	SYS_IRQ_FLAGS flags;

	//start_time = k_cycle_get_32() / 24;

#if defined(CONFIG_SYS_IRQ_LOCK)
	sys_irq_lock(&flags);
#endif

	// program/erase suspend
	for(j = 0; j < 3; j++){
		xspi_nor_write_cmd(sni, SPINOR_CMD_PROGRAM_ERASE_SUSPEND);
		k_busy_wait(30);

		spinor_enable_autoRB(&sni->spi);

		if(j == 0 && key){
			irq_unlock(key);
		}

		for(i = 0; i < 100; i++) { //max 500us, tSUS must 30us
			if (0 == spinor_auto_check_busy(&sni->spi)){
				break;
			}
			k_busy_wait(5);
		}
		if(i != 100){
			break;
		}
	}

	if(j == 3 && i == 100){
	    /* wait spinor ready */
	    while (spinor_auto_check_busy(&sni->spi)) {
	        ;
	    }

		ret_val = 0;
	}else{
		ret_val = 1;
	}

	//end_time = k_cycle_get_32() / 24;

#if defined(CONFIG_SYS_IRQ_LOCK)
	sys_irq_unlock(&flags);
#endif

	//printk("suspend %x %x %x %d %x\n", ret_val, i, j, end_time - start_time, end_time);

	return ret_val;

}


__ramfunc static bool spinor_resume_and_check_idle(uint32_t key, struct xspi_nor_info *sni)
{
	bool ret;
	int i, j;
	//int start_time, end_time;

	//start_time = k_cycle_get_32() / 24;

	xspi_nor_write_cmd(sni, SPINOR_CMD_PROGRAM_ERASE_RESUME);

	k_busy_wait(30);

	for(j = 0; j < 100; j++){ // wait to exit suspend
		k_busy_wait(5);
		if (0 == (xspi_nor_read_status(sni, XSPI_NOR_CMD_READ_STATUS2) & 0x80)){
			break;
		}
	}

	if (0 == (xspi_nor_read_status(sni, XSPI_NOR_CMD_READ_STATUS) & 0x1)) {
		ret = true;
	}else {
		for(i = 0; i < 30; i++){ // handle 1000 us
			k_busy_wait(10);
			if (0 == (xspi_nor_read_status(sni, XSPI_NOR_CMD_READ_STATUS) & 0x1)){
				break;
			}
		}

		//end_time = k_cycle_get_32() / 24;

		if(i != 30){
			 //printk("resume ok %x %x %x %x %x %x\n", i , j, sys_read32(0xc0100000), sys_read32(0xc010001c), end_time - start_time, end_time);
			 ret = true;
		}else{
			//printk("resume %x %x %x %x %x %x\n", i , j, sys_read32(0xc0100000), sys_read32(0xc010001c), end_time - start_time, end_time);
			spinor_suspend(key, sni);
			ret = false;
		}
	}

	return ret;
}
__ramfunc static void spinor_wait_finished(int key, struct xspi_nor_info *sni, uint32_t time_start)
{
	int i;
	for(i = 0; i < 20000; i++){ //2000*500us= 10000ms overtimer
		key = irq_lock();
		if (spinor_resume_and_check_idle(key, sni)){
			//printk("wait finished %d us\n", k_cycle_get_32() / 24 - time_start);
			irq_unlock(key);
			break;
		}
		if(!k_is_in_isr()){
			if((i & 0x1) == 0){
				k_sched_unlock();
				k_sleep(1);
				k_sched_lock();
			}
		}
	}

	if(i == 20000){
		//SYS_LOG_ERR("nor resume error\n");
		while(xspi_nor_read_status(sni, XSPI_NOR_CMD_READ_STATUS) & 0x1); // wait nor ready
		irq_unlock(key);
	}
}

__ramfunc  void spinor_resume_finished(struct xspi_nor_info *sni)
{
	xspi_nor_write_cmd(sni, SPINOR_CMD_PROGRAM_ERASE_RESUME);
	k_busy_wait(5);
	while(xspi_nor_read_status(sni, XSPI_NOR_CMD_READ_STATUS) & 0x1); // wait nor ready
}

__ramfunc void system_xip_nor_wait_ready(uint32_t key, struct xspi_info *spi)
{
	uint32_t time_tsus_start = 0;
	uint32_t ret_val;
	struct xspi_nor_info *sni = CONTAINER_OF(spi, struct xspi_nor_info, spi);

	//time_tsus_start = k_cycle_get_32() / 24;
	ret_val = spinor_suspend(key, sni);
	k_sched_unlock();
	k_sched_lock();
	if(ret_val){
		spinor_wait_finished(key, sni, time_tsus_start);
	}
}


#endif


int xspi_nor_erase(struct device *dev, off_t addr,
              size_t size)
{
    struct xspi_nor_info *sni =
        (struct xspi_nor_info *)dev->driver_data;

#ifdef CONFIG_XSPI_NOR_SUSPEND_RESUME
	uint32_t b_suspend = false;
#endif

    if (!size)
        return 0;

    if (addr & XSPI_NOR_ERASE_SECTOR_MASK || size & XSPI_NOR_ERASE_SECTOR_MASK)
        return -EINVAL;

#ifdef CONFIG_XSPI_NOR_AUTO_CHECK_RB
    if (xspi_controller_id(&sni->spi) == 0){
        k_sched_lock();

#ifdef CONFIG_XSPI_NOR_SUSPEND_RESUME
		k_mutex_lock(&xip_nor_w_mutex, K_FOREVER);

		// erase 256kb, not suspend &resume
		if((size < XSPI_NOR_ERASE_BLOCK_SIZE*4) && (sni->enable_suspend)){
			b_suspend = true;
		}
		if(b_suspend){
			sni->flag |= SPINOR_FLAG_UNLOCK_IRQ_WAIT_READY;
			//sni->spi.flag |= XSPI_INFO_FLAG_SPI_SUSPEND;
			sni->spi.wait_rb_ready = system_xip_nor_wait_ready;
		}
#endif

    }
#endif

    soc_watchdog_clear();

    if (sni->rom_api) {
	    sni->rom_api->erase(&sni->spi, addr, size);
    } else {
#ifdef CONFIG_XSPI_MEM_ACTS
        xspi_nor_erase_internal(sni, addr, size);
#endif
    }

#ifdef CONFIG_XSPI_NOR_AUTO_CHECK_RB
    if (xspi_controller_id(&sni->spi) == 0){
#ifdef CONFIG_XSPI_NOR_SUSPEND_RESUME
		if(b_suspend){
			sni->spi.wait_rb_ready = NULL;
			sni->flag &= ~(SPINOR_FLAG_UNLOCK_IRQ_WAIT_READY);
			//sni->spi.flag &= ~(XSPI_INFO_FLAG_SPI_SUSPEND);
		}
		k_mutex_unlock(&xip_nor_w_mutex);
		k_sched_unlock();
#else
        k_sched_unlock();
#endif
	}
#endif

    return 0;
}

int xspi_nor_read(struct device *dev, off_t addr,
             void *data, size_t len)
{
    struct xspi_nor_info *sni =
        (struct xspi_nor_info *)dev->driver_data;

    if (!len)
        return 0;

    if (sni->rom_api) {
        sni->rom_api->read(&sni->spi, addr, data, len);
    } else {
#ifdef CONFIG_XSPI_MEM_ACTS
        xspi_nor_read_internal(sni, addr, data, len);
#endif
    }

    return 0;
}

u32_t xspi_nor_read_chipid(struct xspi_nor_info *sni)
{
    u32_t chipid = 0;

    if (sni->rom_api) {
        chipid = sni->rom_api->read_chipid(&sni->spi);
    } else {
#ifdef CONFIG_XSPI_MEM_ACTS
        xspi_mem_read_chipid(&sni->spi, &chipid, 3);
#endif
    }

    return chipid;
}

__ramfunc int xspi_nor_power_ctrl(struct xspi_nor_info *sni, u8_t is_poweron)
{
    if (sni->rom_api){
        sni->rom_api->deep_sleep(&sni->spi, is_poweron);
		return 0;
    }else{
        return -EIO;
    }
}


