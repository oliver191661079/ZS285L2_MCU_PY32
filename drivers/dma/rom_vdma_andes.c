/********************************************************************************
 *                            USDK(US283A_master)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2018-1-5-下午3:23:20             1.0             build this file
 ********************************************************************************/
/*!
 * \file     rom_vdma_andes.c
 * \brief    DMA物理层驱动相关函数
 * \author
 * \version  1.0
 * \date  2018-1-5-下午3:23:20
 *******************************************************************************/
#include "rom_vdma_andes.h"
#include <csi_core.h>

#ifndef CONFIG_USE_ROM_VDMA

#define     PSRAM_DMA_BASE_ADDR  (0x68000000)
#define     MEMORY_CTL           (0xc00a0000)

static inline uint32_t DMA_PSRAM_ADDR_CONVERT(unsigned int cpu_addr)
{
    if(cpu_addr >= sys_read32(SPI1_DCACHE_START_ADDR)
        && cpu_addr <= sys_read32(SPI1_DCACHE_END_ADDR))
    {
        cpu_addr -= sys_read32(SPI1_DCACHE_START_ADDR);
        cpu_addr += PSRAM_DMA_BASE_ADDR;
    }

    return cpu_addr;
}

static inline void set_dma_data(rom_dma_data_t *p_rom_data, struct dma_controller *dmac, struct dma *dma)
{
    dmac->dma = dma;
    dma->dmac_num = dmac - p_rom_data->dmacs;
}

static inline void free_dma_controller(struct dma_controller *dmac)
{
    dmac->dma = NULL;
}

static inline void free_dma_xfer(struct dma *dma)
{
    dma->dmac_num = DMA_INVALID_DMACS_NUM;
    set_dma_state(dma, DMA_STATE_IDLE);
}

static inline void free_dma_data(struct dma_controller *dmac, struct dma *dma)
{
    free_dma_controller(dmac);
    free_dma_xfer(dma);
}

void _rom_dma_set_irq(unsigned int dma_no, unsigned int irq_enable, unsigned int half_irq_enable)
{
    if (irq_enable) {
        sys_write32(sys_read32(DMAIE) | (0x1 << dma_no), DMAIE);

        //需要注意DMAIP是写1清0
        sys_write32((0x1 << dma_no), DMAIP);
    } else {
        sys_write32(sys_read32(DMAIE) & (~(0x1 << dma_no)), DMAIE);

        //需要注意DMAIP是写1清0
        sys_write32((0x1 << dma_no), DMAIP);
    }

    if (half_irq_enable) {
        sys_write32(sys_read32(DMAIE) | (0x100 << dma_no), DMAIE);

        //需要注意DMAIP是写1清0
        sys_write32((0x100 << dma_no), DMAIP);
    } else {
        sys_write32(sys_read32(DMAIE) & (~(0x100 << dma_no)), DMAIE);

        //需要注意DMAIP是写1清0
        sys_write32((0x100 << dma_no), DMAIP);
    }
}

struct dma_controller *_rom_dma_controller_alloc(rom_dma_data_t *p_rom_data, struct dma *dma)
{
    int i;

    for (i = 0; i < p_rom_data->dma_num; i++) {
        if (p_rom_data->dmacs[i].channel_assigned == 0) {
            p_rom_data->dmacs[i].channel_assigned = 1;
            set_dma_data(p_rom_data, &(p_rom_data->dmacs[i]), dma);
            return &(p_rom_data->dmacs[i]);
        }
    }

    return NULL;
}


struct dma_controller *_rom_dma_controller_is_alloc(rom_dma_data_t *p_rom_data, struct dma *dma)
{
    struct dma_controller *dmac;

    if (dma->dmac_num == DMA_INVALID_DMACS_NUM) {
        return NULL;
    }

    dmac = p_rom_data->dmacs + dma->dmac_num;

    if (dmac->channel_assigned == 1) {
        return dmac;
    }

    return NULL;
}

void _rom_dma_controller_free(struct dma_controller *dmac, struct dma *dma)
{
    dmac->channel_assigned = 0;
    free_dma_data(dmac, dma);
}


int _rom_dma_start_request_trans(rom_dma_data_t *p_rom_data, struct dma_controller *dmac)
{
    struct dma *dma;

    if (!sys_slist_is_empty(&p_rom_data->dma_req_list)) {
        dma = (struct dma *) SYS_SLIST_PEEK_HEAD_CONTAINER(&p_rom_data->dma_req_list, dma, node);
        sys_slist_find_and_remove(&p_rom_data->dma_req_list, &dma->node);
        set_dma_data(p_rom_data, dmac, dma);
        p_rom_data->dma_cb->reg_write(dma, dmac->dma_chan_num);
        p_rom_data->dma_cb->trigger_start(p_rom_data, (int)dma_num_to_reg(dmac->dma_chan_num));
        return 0;
    }
    return -1;
}

int rom_dma_get_vdma_handle(rom_dma_data_t *p_rom_data, u32_t dma_reg)
{
    int i;

    int dma_no = dma_reg_to_num((dma_reg_t *)dma_reg);

    for (i = 0; i < p_rom_data->dma_num; i++) {
        if (p_rom_data->dmacs[i].dma_chan_num == dma_no && p_rom_data->dmacs[i].channel_assigned) {
            return (int)(p_rom_data->dmacs[i].dma);
        }
    }

    return 0;
}

int rom_dma_get_hdma_handle(rom_dma_data_t *p_rom_data, int handle)
{
    struct dma *dma;

    struct dma_controller *dmac;

    dma_reg_t *dma_reg = NULL;

    if (is_virtual_dma_handle(handle)) {
        dma = (struct dma *) handle;

        dmac = _rom_dma_controller_is_alloc(p_rom_data, dma);

        if (dmac != NULL) {
            dma_reg = dma_num_to_reg(dmac->dma_chan_num);

            return (int)dma_reg;
        }
    } else{
        return handle;
    }

    return 0;
}

struct dma_controller *rom_dma_get_dmacs_by_num(rom_dma_data_t *p_rom_data, unsigned int channel_no)
{
    int i;

    for (i = 0; i < p_rom_data->dma_num; i++) {
        if (p_rom_data->dmacs[i].channel_assigned == 1 && p_rom_data->dmacs[i].dma_chan_num == channel_no) {
            return &(p_rom_data->dmacs[i]);
        }
    }

    return NULL;
}

int rom_hw_dma_trigger_start(dma_reg_t *dma_reg)
{
    //启动DMA传输
    dma_reg->start |= (1 << DMA0START_DMASTART);

    return 0;
}

int rom_vdma_trigger_start(rom_dma_data_t *p_rom_data, int handle)
{
    struct dma *dma = (struct dma *)handle;
    //判断dma是否分配了控制器
    struct dma_controller *dmac = _rom_dma_controller_is_alloc(p_rom_data, dma);

    if (dmac != NULL) {
        dma_reg_t *dma_reg = dma_num_to_reg(dmac->dma_chan_num);

        rom_hw_dma_trigger_start(dma_reg);

        return 0;
    }

    return -1;
}

int rom_dma_trigger_start(rom_dma_data_t *p_rom_data, int handle)
{
    if (is_virtual_dma_handle(handle)) {
        return rom_vdma_trigger_start(p_rom_data, handle);
    }else{
        return rom_hw_dma_trigger_start((dma_reg_t *)handle);
    }
}

/*only used for asrc reload DMA */
void rom_asrc_dma_stop(dma_reg_t *dma_reg)
{
    u32_t reg_ctl = dma_reg->ctl;
    u32_t timeout = 0x5000;

    //先清reload标志位
    dma_reg->ctl &= (~(1 << DMA0CTL_RELOAD));

    //停止dma传输
    while(dma_reg->start & (1 << DMA0START_DMASTART)){
        dma_reg->start &= (~(1 << DMA0START_DMASTART));
        timeout--;
        if(!timeout){
            break;
        }
    }

    //清DMA IE和pending
    _rom_dma_set_irq(dma_reg_to_num(dma_reg), 1, 1);

    dma_reg->ctl |= (reg_ctl & (1 << DMA0CTL_RELOAD));

    //clear SAM bit for DMA
    dma_reg->ctl &= (~(1 << 4));
}

void rom_dma_stop(dma_reg_t *dma_reg)
{
    u32_t timeout = 0x5000;

    //先清reload标志位
    dma_reg->ctl &= (~(1 << DMA0CTL_RELOAD));

    //停止dma传输
    while(dma_reg->start & (1 << DMA0START_DMASTART)){
        dma_reg->start &= (~(1 << DMA0START_DMASTART));
        timeout--;
        if(!timeout){
            break;
        }
    }

    //清DMA IE和pending
    _rom_dma_set_irq(dma_reg_to_num(dma_reg), 0, 0);
}

void rom_dma_regs_write(struct dma *dma, int dma_no)
{
    dma_reg_t *dma_reg = dma_num_to_reg(dma_no);

    if(dma != NULL){
        set_dma_state(dma, DMA_STATE_CONFIG);

        _rom_dma_set_irq(dma_no, 1, dma->half_en);

        dma_reg->saddr0 = dma->dma_regs.saddr0;
        dma_reg->daddr0 = dma->dma_regs.daddr0;
        dma_reg->framelen = dma->dma_regs.framelen;

        if (dma->dma_regs.ctl & (1 << DMA0CTL_AUDIOTYPE)) {
            //separated stored in the memory
            if ((dma->dma_regs.ctl & (0xf << 8)) == 0) {
                //如果目的地址是内存
                dma_reg->daddr1 = dma->dma_regs.saddr0;
            } else {
                //如果源地址是内存
                dma_reg->saddr1 = dma->dma_regs.daddr0;
            }
        }

        dma_reg->start = 0;

        dma_reg->ctl = dma->dma_regs.ctl;
    }
}


int rom_dma_start(rom_dma_data_t *p_rom_data, int handle)
{
    unsigned int flags;

    int ret_val = 0;

    struct dma *dma;

    struct dma_controller *dmac;

    if (is_virtual_dma_handle(handle)) {
        dma = (struct dma *) handle;

        flags = irq_lock();

        dmac = _rom_dma_controller_alloc(p_rom_data, dma);

        if (dmac != NULL) {
            p_rom_data->dma_cb->reg_write(dma, dmac->dma_chan_num);
            p_rom_data->dma_cb->trigger_start(p_rom_data, handle);
        } else {
            /*没有dma控制器空闲，就将dma请求加入到请求链表*/
            set_dma_state(dma, DMA_STATE_WAIT);
            sys_slist_append(&p_rom_data->dma_req_list, &dma->node);
            ret_val = -1;
        }

        irq_unlock(flags);
    } else {
        p_rom_data->dma_cb->trigger_start(p_rom_data, handle);
    }

    return ret_val;
}

void rom_dma_abort(rom_dma_data_t *p_rom_data, int handle)
{
    unsigned int flags;

    struct dma *dma;

    struct dma_controller *dmac;

    dma_reg_t *reg;

    if (is_virtual_dma_handle(handle)) {
        flags = irq_lock();

        dma = (struct dma *) handle;

        if (get_dma_state(dma) == DMA_STATE_WAIT) {
            //从空闲链表去除
            sys_slist_find_and_remove(&p_rom_data->dma_req_list, &dma->node);
            free_dma_xfer(dma);

        } else {

            //判断dma是否分配了控制器
            dmac = _rom_dma_controller_is_alloc(p_rom_data, dma);

            if (dmac != NULL) {
                reg = dma_num_to_reg(dmac->dma_chan_num);

                rom_dma_stop(reg);

                free_dma_xfer(dma);

                //abort一路DMA后，检查请求队列是否有线程等待进行数据传输
                if (_rom_dma_start_request_trans(p_rom_data, dmac) < 0) {
                    _rom_dma_controller_free(dmac, dma);
                }
            }
        }

        irq_unlock(flags);
    } else {
        reg = (dma_reg_t *) handle;

        rom_dma_stop(reg);
    }
}

void rom_dma_irq_handle(rom_dma_data_t *p_rom_data, unsigned int irq)
{
    unsigned int pending;
    int phy_dma_no, irq_en;
    struct dma_controller *dmac;
    struct dma *dma;
    unsigned int flags;
    void (*dma_irq_handler)(int irq, unsigned int type, void *pdata);

    phy_dma_no = irq - IRQ_ID_DMA0;

    dmac = rom_dma_get_dmacs_by_num(p_rom_data, phy_dma_no);

    if(!dmac){
        return;
    }

    dma = dmac->dma;

    flags = irq_lock();

    //只清除这一路的dma pending位
    pending = (sys_read32(DMAIP) & (0x101 << phy_dma_no));

    sys_write32(pending, DMAIP);

    irq_en = sys_read32(DMAIE);

    if ((pending & irq_en) & (0x100 << phy_dma_no)) {
        set_dma_state(dma, DMA_STATE_IRQ_HF);

        //半满中断
        if (dma->dma_irq_handler) {
            dma_irq_handler = dma->dma_irq_handler;
            irq_unlock(flags);
            dma_irq_handler(irq, DMA_IRQ_HF, dma->pdata);
            flags = irq_lock();
        }
    }
    if ((pending & irq_en) & (0x1 << phy_dma_no)) {
        set_dma_state(dma, DMA_STATE_IRQ_TC);

        //非reload模式的dma参与重新调度
        if ((dma->dma_regs.ctl & (1 << DMA0CTL_RELOAD)) == 0) {
            //设置传输结束标志
            free_dma_xfer(dma);

            if (_rom_dma_start_request_trans(p_rom_data, dmac) < 0) {
                _rom_dma_controller_free(dmac, dma);
                sys_write32(sys_read32(DMAIE) & (~pending), DMAIE);
            }
        } else {
            dma->reload_count++;
        }

        //全满中断
        if (dma->dma_irq_handler) {
            dma_irq_handler = dma->dma_irq_handler;
            irq_unlock(flags);
            dma->dma_irq_handler(irq, DMA_IRQ_TC, dma->pdata);
            flags = irq_lock();
        }
    }

    irq_unlock(flags);
}

void rom_dma_trigger_start_by_num(u32_t dma_no)
{
    rom_hw_dma_trigger_start(dma_num_to_reg(dma_no));
}

void rom_dma_abort_by_num(u32_t dma_no)
{
    rom_dma_stop(dma_num_to_reg(dma_no));
}


int rom_dma_config(int handle, uint32_t ctl, dma_irq_handler_t irq_handle, void *pdata, uint32_t half_en)
{
    struct dma *dma;

    dma_reg_t *reg;

    if (is_virtual_dma_handle(handle)) {
        dma = (struct dma *) handle;
        if (irq_handle != NULL) {
            dma->dma_irq_handler = irq_handle;
            dma->pdata = pdata;
        }

        dma->dma_regs.ctl = ctl;
        dma->half_en = half_en;
    } else {
        reg = (dma_reg_t *) handle;

        if (irq_handle != NULL) {
            //允许DMA中断
            _rom_dma_set_irq(dma_reg_to_num(reg), 1, half_en);
        }

        reg->ctl = ctl;
    }

    return 0;
}

int rom_dma_frame_config(int dma_handle, uint32_t saddr, uint32_t daddr, uint32_t frame_len)
{
    struct dma *dma;
    dma_reg_t *regs;

    saddr = DMA_PSRAM_ADDR_CONVERT(saddr);
    daddr = DMA_PSRAM_ADDR_CONVERT(daddr);

    if (is_virtual_dma_handle(dma_handle)) {
        dma = (struct dma *) dma_handle;

        dma->dma_regs.saddr0 = saddr;
        dma->dma_regs.daddr0 = daddr;
        dma->dma_regs.framelen = frame_len;

    } else {
        regs = (dma_reg_t *) dma_handle;

        regs->saddr0 = saddr;

        if (regs->ctl & (1 << DMA0CTL_AUDIOTYPE)) {
            //separated stored in the memory
            if ((regs->ctl & (0xf << 8)) != 0) {
                //如果源地址是内存
                regs->saddr1 = saddr;
            }
        }

        regs->daddr0 = daddr;

        regs->framelen = frame_len;
    }

    return 0;
}

void rom_dma_poll_if_irq_disabled(rom_dma_data_t *p_rom_data)
{
    unsigned int psr_status;
    unsigned int phy_dma_no;

    psr_status = csi_get_sp();

    /*如果是关中断情况下调用，就需要自行处理中断响应*/
    if (((psr_status & (1 << 6)) == 0)
        || ((psr_status & (1 << 8)) == 0)) {
        int i;

        for (i = 0; i < p_rom_data->dma_num; i++) {
            if (p_rom_data->dmacs[i].channel_assigned == 1) {
                phy_dma_no = p_rom_data->dmacs[i].dma_chan_num;

                //pending位有置起来并且允许中断，才真正响应DMA中断
                if ((sys_read32(DMAIP) & (0x101 << phy_dma_no))
                        && ((sys_read32(DMAIE) & (0x101 << phy_dma_no)))) {
                    rom_dma_irq_handle(p_rom_data, IRQ_ID_DMA0 + phy_dma_no);
                }
            }
        }
    }
}


bool rom_dma_query_complete(rom_dma_data_t *p_rom_data, int dma_handle)
{
    struct dma *dma;

    int ret_val;

    if (is_virtual_dma_handle(dma_handle)) {
        p_rom_data->dma_cb->poll_if_irq_disabled(p_rom_data);

        dma = (struct dma *) dma_handle;

        ret_val = (get_dma_state(dma) == DMA_STATE_IDLE);
    } else {
        ret_val = ((((dma_reg_t *) dma_handle)->start & 0x01) == 0);
    }

    //dma已经空闲，需要检查memory controller dma busy位
    if (ret_val == 1){
        return ((sys_read32(MEMORY_CTL) & (1 << 25)) == 0);
    }else{
        return 0;
    }
}


int rom_dma_prestart(rom_dma_data_t *p_rom_data, int handle)
{
    unsigned int flags;

    struct dma *dma;

    struct dma_controller *dmac;

    dma_reg_t *dma_reg = NULL;

    if (is_virtual_dma_handle(handle)) {
        dma = (struct dma *) handle;

        flags = irq_lock();

        dmac = _rom_dma_controller_alloc(p_rom_data, dma);

        if (dmac != NULL) {

            dma_reg = dma_num_to_reg(dmac->dma_chan_num);

            if(dma != NULL){
                p_rom_data->dma_cb->reg_write(dma, dmac->dma_chan_num);
            }
        }

        irq_unlock(flags);
    } else{
        dma_reg = (dma_reg_t *)handle;
    }

    return (int)dma_reg;
}

int rom_hw_dma_get_samples_bytes(struct rom_dma_data *p_rom_data,  dma_reg_t *dma_reg)
{
    dma_samples_info_t samples_info;
	struct dma *dma;

    if(!dma_reg){
        return -1;
    }

    dma = (struct dma *)p_rom_data->dma_cb->get_vdma_handle(p_rom_data, (u32_t)dma_reg);
    if(!dma_reg){
        return -1;
    }

    samples_info.framelen   = dma_reg->framelen;
    samples_info.remainlen = dma_reg->frame_remainlen;
    samples_info.reload_count = dma->reload_count;

    if(samples_info.framelen == samples_info.remainlen){
        return -1;
    }

    return samples_info.framelen - samples_info.remainlen + samples_info.reload_count * samples_info.framelen;
}


int rom_dma_get_samples_info(struct rom_dma_data *p_rom_data, dma_reg_t *dma_reg, unsigned int *framelen,unsigned int *remainlen, unsigned short *reload_count)
{
	struct dma *dma;

    if(!dma_reg){
        return -1;
    }

    dma = (struct dma *)p_rom_data->dma_cb->get_vdma_handle(p_rom_data, (u32_t)dma_reg);
    if(!dma){
        return -1;
    }

    *framelen = dma_reg->framelen;
    *remainlen = dma_reg->frame_remainlen;
    *reload_count = dma->reload_count;

    if(*framelen == *remainlen){
        return -1;
    }
    return 0;
}

int rom_dma_wait_complete(rom_dma_data_t *p_rom_data, int handle, unsigned int timeout_us)
{
    unsigned int cycles = 0, prev_cycle;
    u32_t timeout_cycles = (timeout_us * 24);

    prev_cycle = k_cycle_get_32();

    while ((timeout_us == 0 || cycles < timeout_cycles)) {
        if (p_rom_data->dma_cb->query_complete(p_rom_data, handle)) {
            break;
        }

        cycles = k_cycle_get_32() - prev_cycle;
    }

    if (timeout_us != 0 && cycles >= timeout_cycles) {
        return -ETIMEDOUT;
    }

    return 0;
}

const dma_rom_callback_t dma_rom_callback =
{
    rom_dma_get_vdma_handle,
    rom_dma_get_hdma_handle,
    rom_dma_get_dmacs_by_num,
    rom_hw_dma_trigger_start,
    rom_vdma_trigger_start,
    rom_dma_trigger_start,
    rom_asrc_dma_stop,
    rom_dma_stop,
    rom_dma_regs_write,
    rom_dma_start,
    rom_dma_abort,
    rom_dma_irq_handle,
    rom_dma_trigger_start_by_num,
    rom_dma_abort_by_num,
    rom_dma_config,
    rom_dma_frame_config,
    rom_dma_poll_if_irq_disabled,
    rom_dma_query_complete,
    rom_dma_prestart,
    rom_hw_dma_get_samples_bytes,
    rom_dma_get_samples_info,
    rom_dma_wait_complete,
};

u32_t rom_get_dma_drv_api(void)
{
	return (u32_t)&dma_rom_callback;
}

#endif


int rom_dma_get_hdma_handle_fix(rom_dma_data_t *p_rom_data, int handle)
{
    struct dma *dma;

    struct dma_controller *dmac;

    dma_reg_t *dma_reg = NULL;

    if (is_virtual_dma_handle(handle)) {
        dma = (struct dma *) handle;

        dmac = p_rom_data->dmacs + dma->dmac_num;

        if (dmac != NULL) {
            dma_reg = dma_num_to_reg(dmac->dma_chan_num);

            return (int)dma_reg;
        }
    } else{
        return handle;
    }

    return 0;
}



