/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file DMA configuration macros for Actions SoC
 */

#ifndef	_ACTIONS_SOC_DMA_H_
#define	_ACTIONS_SOC_DMA_H_

#define	DMA_ID_MEMORY			0
#define	DMA_ID_BASEBAND_RAM		1
#define	DMA_ID_SPI2			2
#define	DMA_ID_UART0			3
#define	DMA_ID_UART1			4
#define	DMA_ID_SD1			5
#define	DMA_ID_SD0			6
#define	DMA_ID_USB0			7
#define	DMA_ID_SPI0			8
#define	DMA_ID_SPI1			9
#define	DMA_ID_AUDIO_I2S		10
#define	DMA_ID_AUDIO_DAC_FIFO0		11
#define	DMA_ID_AUDIO_ADC_FIFO		11
#define	DMA_ID_AUDIO_DAC_FIFO1		12
#define	DMA_ID_AUDIO_ASRC_FIFO0		13
#define	DMA_ID_AUDIO_ASRC_FIFO1		14
#define	DMA_ID_AUDIO_LCD		15

#define DMA_TRD_BURST8   0
#define DMA_TRD_SINGLE   1


#define	DMA_ID_MAX_ID			DMA_ID_AUDIO_LCD

#define DMA_GLOB_OFFS			0x0000
#define DMA_CHAN_OFFS			0x0100

#define DMA_CHAN(base, id)		((base) + DMA_CHAN_OFFS + (id) * 0x100)
#define DMA_GLOBAL(base)		((base) + DMA_GLOB_OFFS)

#define DMA_ACTS_MAX_CHANNELS		8

/* Maximum data sent in single transfer (Bytes) */
#define DMA_ACTS_MAX_DATA_ITEMS		0x3ffff

#define DMA_REG_BASE			        0xc0040000

#define DMA_CHANNEL_REG_BASE        (DMA_REG_BASE + 0x100)
#define DMAIP                       (DMA_REG_BASE)
#define DMAIE                       (DMA_REG_BASE+0x4)
#define DMATIMEOUTPD                (DMA_REG_BASE+0x8)
#define DMADEBUG                    (DMA_REG_BASE+0x80)

#define DMA0CTL                     (DMA_REG_BASE+0x00000100)
#define DMA0START                   (DMA_REG_BASE+0x00000104)
#define DMA0SADDR0                  (DMA_REG_BASE+0x00000108)
#define DMA0SADDR1                  (DMA_REG_BASE+0x0000010c)
#define DMA0DADDR0                  (DMA_REG_BASE+0x00000110)
#define DMA0DADDR1                  (DMA_REG_BASE+0x00000114)
#define DMA0BC                      (DMA_REG_BASE+0x00000118)
#define DMA0RC                      (DMA_REG_BASE+0x0000011c)


#define DMA_REG_SHIFT 8

#define DMA0START_DMASTART          0
#define DMA0CTL_SRCTYPE             0
#define DMA0CTL_DSTTYPE             8
#define DMA0CTL_RELOAD              15
#define DMA0CTL_AUDIOTYPE           16

#define DMA_CTL_OFFSET              (0x00)
#define DMA_START_OFFSET            (0x04)
#define DMA_SADDR_OFFSET            (0x08)
#define DMA_DADDR_OFFSET            (0x10)
#define DMA_BC_OFFSET               (0x18)
#define DMA_RC_OFFSET               (0x1c)

typedef struct dma_regs
{
    volatile unsigned int ctl;
    volatile unsigned int start;
    volatile unsigned int saddr0;
    volatile unsigned int saddr1;
    volatile unsigned int daddr0;
    volatile unsigned int daddr1;
    volatile unsigned int framelen;
    volatile unsigned int frame_remainlen;
}dma_reg_t;

typedef struct dma_regs_backup
{
	uint32_t ctrl;
	uint32_t saddr0;
	uint32_t saddr1;
	uint32_t daddr0;
	uint32_t daddr1;
	uint32_t framelen;
	uint32_t frame_remainlen;
	uint32_t dma_ie;
}dma_regs_backup_t;

bool inline acts_dma_is_started(dma_reg_t *dma_reg)
{
    return ((dma_reg->start & 1<<DMA0START_DMASTART) >> DMA0START_DMASTART);
}

static inline dma_reg_t *dma_num_to_reg(int dma_no)
{
    return (dma_reg_t *) (DMA_CHANNEL_REG_BASE + (dma_no << DMA_REG_SHIFT));
}

static inline int dma_reg_to_num(dma_reg_t *reg)
{
    return (((int) reg - DMA_CHANNEL_REG_BASE) >> DMA_REG_SHIFT);
}

static inline int is_virtual_dma_handle(int dma_handle)
{
    if ((dma_handle & 0xc0000000) == 0xc0000000) {
        return 0;
    } else {
        return 1;
    }
}

static inline void acts_dma_start(dma_reg_t *dma_reg)
{
    //启动DMA传输
    dma_reg->start |= (1 << DMA0START_DMASTART);
}

static inline int acts_dma_is_start(dma_reg_t *dma_reg)
{
    return (dma_reg->start & (1 << DMA0START_DMASTART));
}

static inline void acts_dma_disable_sam_mode(u32_t dma_reg)
{
	//clear SAM bit for DMA
	sys_write32(sys_read32(dma_reg) & (~(1 << 4)), dma_reg);
}

static inline void acts_dma_set_single_mode(u32_t dma_reg)
{
	//set single mode bit for DMA
	sys_write32(sys_read32(dma_reg) | (1 << 17), dma_reg);
}

static inline int acts_dma_get_remainlen(dma_reg_t *dma_reg)
{
	return dma_reg->frame_remainlen;
}

static inline int acts_dma_check_dma_pending(dma_reg_t *dma_reg)
{
	return (sys_read32(DMAIP) & (0x101 << dma_reg_to_num(dma_reg)));
}


void soc_dma_send_data(uint32_t usb_type, uint32_t dma_chan_base, char *s, unsigned int len);

int soc_dma_is_send_complete(u32_t dma_chan_base);


void soc_dma_ip_clear(u32_t dma_chan_idx, u32_t is_tc);

void sco_dma_mem_copy(uint32_t src_addr, uint32_t dest_addr, uint32_t data_len, uint32_t dma_reg);

#ifndef _ASMLANGUAGE

#endif /* _ASMLANGUAGE */

#endif /* _ACTIONS_SOC_DMA_H_	*/
