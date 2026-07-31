/********************************************************************************
 *                            USDK(ZS283A)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2018-10-12-下午4:59:36             1.0             build this file
 ********************************************************************************/
/*!
 * \file     dmac.h
 * \brief
 * \author
 * \version  1.0
 * \date  2018-10-12-下午4:59:36
 *******************************************************************************/

#ifndef _ROM_VDMA_ANDES_H_
#define _ROM_VDMA_ANDES_H_

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/types.h>
#include <dma.h>
#include <soc_regs.h>
#include <soc_irq.h>
#include <soc_dma.h>
#include <soc_memctrl.h>
#include "rom_vdma_list.h"

#define DMA_CTL_TRMODE_SINGLE	    (0x1 << 17)
#define DMA_CTL_RELOAD			(0x1 << 15)
#define DMA_CTL_SRC_TYPE_SHIFT		0
#define DMA_CTL_SRC_TYPE(x)		((x) << DMA_CTL_SRC_TYPE_SHIFT)
#define DMA_CTL_SRC_TYPE_MASK		DMA_CTL_SRC_TYPE(0xf)
#define DMA_CTL_DST_TYPE_SHIFT		8
#define DMA_CTL_DST_TYPE(x)		((x) << DMA_CTL_DST_TYPE_SHIFT)
#define DMA_CTL_DST_TYPE_MASK		DMA_CTL_DST_TYPE(0xf)
#define DMA_CTL_TWS_SHIFT		13
#define DMA_CTL_TWS(x)			((x) << DMA_CTL_TWS_SHIFT)
#define DMA_CTL_SAM_SHIFT       4
#define DMA_CTL_DAM_SHIFT       12
#define DMA_CTL_TWS_MASK		DMA_CTL_TWS(0x3)
#define DMA_CTL_TWS_8BIT		DMA_CTL_TWS(3)
#define DMA_CTL_TWS_16BIT		DMA_CTL_TWS(2)
#define DMA_CTL_TWS_32BIT		DMA_CTL_TWS(1)
#define DMA_CTL_TWS_64BIT		DMA_CTL_TWS(0)


#define DMA_INVALID_DMACS_NUM       (0xff)

typedef void (*dma_irq_handler_t)(int irq, unsigned int type, void *pdata);

struct dma_controller;

typedef enum
{
    DMA_STATE_ALLOC = 0,
    DMA_STATE_WAIT,
    DMA_STATE_CONFIG,
    DMA_STATE_IRQ_HF,
    DMA_STATE_IRQ_TC,
    DMA_STATE_IDLE,
    DMA_STATE_ABORT,
    DMA_STATE_MAX,
}dma_state_e;

struct dma
{
	sys_snode_t node;
    struct dma_regs dma_regs;
    void *pdata;
    dma_irq_handler_t dma_irq_handler;
    unsigned char dmac_num;
    unsigned char dma_state : 4;
    unsigned char half_en : 1;
    unsigned char reserved : 3;
    unsigned short reload_count;
};

struct dma_controller
{
    unsigned char dma_chan_num;
    unsigned char channel_assigned;
    unsigned char reserved[2];
    struct dma *dma;
};

struct dma_wrapper_data
{
	int  dma_handle;
	void (*dma_callback)(struct device *dev, u32_t data, int reason);
	/* Client callback private data */
	void *callback_data;
};

struct rom_dma_data;

typedef struct {
    unsigned int framelen;
    unsigned int remainlen;
    unsigned short reload_count;
} dma_samples_info_t;

typedef struct{
    int (*get_vdma_handle)(struct rom_dma_data *p_rom_data, u32_t dma_reg);
    int (*get_hdma_handle)(struct rom_dma_data *p_rom_data, int handle);
    struct dma_controller *(*get_dmacs_by_num)(struct rom_dma_data *p_rom_data, unsigned int channel_no);
    int (*hw_dma_trigger_start)(dma_reg_t *dma_reg);
    int (*vdma_trigger_start)(struct rom_dma_data *p_rom_data, int handle);
    int (*trigger_start)(struct rom_dma_data *p_rom_data, int handle);
    void (*asrc_dma_stop)(dma_reg_t *dma_reg);
    void (*dma_stop)(dma_reg_t *dma_reg);
    void (*reg_write)(struct dma *dma, int dma_no);
    int (*start)(struct rom_dma_data *p_rom_data, int handle);
    void (*abort)(struct rom_dma_data *p_rom_data, int handle);
    void (*irq_handle)(struct rom_dma_data *p_rom_data, unsigned int irq);
    void (*trigger_start_by_num)(u32_t dma_no);
    void (*abort_by_num)(u32_t dma_no);
    int (*config)(int handle, uint32_t ctl, dma_irq_handler_t irq_handle, void *pdata, uint32_t half_en);
    int (*frame_config)(int dma_handle, u32_t saddr, u32_t daddr, u32_t frame_len);
    void (*poll_if_irq_disabled)(struct rom_dma_data *p_rom_data);
    bool (*query_complete)(struct rom_dma_data *p_rom_data, int dma_handle);
    int (*prestart)(struct rom_dma_data *p_rom_data, int handle);
    int (*get_samples_bytes)(struct rom_dma_data *p_rom_data, dma_reg_t *dma_reg);
    int (*get_samples_info)(struct rom_dma_data *p_rom_data, dma_reg_t *dma_reg, unsigned int *framelen,unsigned int *remainlen, unsigned short *reload_count);
	int (*wait_complete)(struct rom_dma_data *p_rom_data, int handle, unsigned int timeout_us);
}dma_rom_callback_t;

typedef struct rom_dma_data {
    uint8_t dma_num;
    struct dma_controller *dmacs;
	sys_slist_t dma_req_list;
	dma_rom_callback_t *dma_cb;
} rom_dma_data_t;

extern rom_dma_data_t *rom_dma_global_p;

static inline void set_dma_state(struct dma *dma, unsigned int state)
{
    dma->dma_state = state;
}

static inline unsigned int get_dma_state(struct dma *dma)
{
    return dma->dma_state;
}


static inline int dma_alloc_channel(int channel)
{
    return (int) dma_num_to_reg(channel);
}


void dma_irq_handle(unsigned int irq);
int dma_alloc_channel(int channel);
int is_virtual_dma_handle(int dma_handle);
int acts_dma_pre_start(int handle);
int acts_dma_get_transfer_length(int handle);
int acts_dma_get_phy_channel(int handle);

u32_t rom_get_dma_drv_api(void);

#endif /* _ROM_VDMA_ANDES_H_ */
