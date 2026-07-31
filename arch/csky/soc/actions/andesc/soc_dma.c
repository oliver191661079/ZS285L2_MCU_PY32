#include <soc.h>

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

void uart_spi_dma_send_data(uint32_t dma_chan_base, char *s, unsigned int len)
{
    u32_t start_reg;

	u32_t start_addr = (u32_t)s;

    start_addr = DMA_PSRAM_ADDR_CONVERT(start_addr);

    /* reload dma */
    sys_write32(start_addr, dma_chan_base + DMA_SADDR_OFFSET);
    sys_write32(len, dma_chan_base + DMA_BC_OFFSET);

    /* start dma */
    start_reg = dma_chan_base + DMA_START_OFFSET;
    sys_write32(sys_read32(start_reg) | (1 << DMA0START_DMASTART), start_reg);
}

void soc_dma_send_data(uint32_t usb_type, uint32_t dma_chan_base, char *s, unsigned int len)
{
    if(usb_type == 0){
        uart_spi_dma_send_data(dma_chan_base, s, len);
    }
}

int soc_dma_is_send_complete(u32_t dma_chan_base)
{
    u32_t start_reg;

    /* start dma */
    start_reg = dma_chan_base + DMA_START_OFFSET;
    if ((sys_read32(start_reg) & (1 << DMA0START_DMASTART)) == 0)
    {
        return ((sys_read32(MEMORY_CTL) & (1 << 25)) == 0);
    }

    return 0;
}

void soc_dma_ip_clear(u32_t dma_chan_idx, u32_t is_tc)
{
    if(is_tc){
        sys_write32((1 << dma_chan_idx), DMAIP);
    }else{
        sys_write32((0x100 << dma_chan_idx), DMAIP);
    }
}

void sco_dma_mem_copy(uint32_t src_addr, uint32_t dest_addr, uint32_t data_len, uint32_t dma_reg)
{
    dma_reg_t *reg = (dma_reg_t *)dma_reg;

	u32_t dma_idx = dma_reg_to_num(reg);

    reg->ctl = 0x6000;

    reg->saddr0 = src_addr;
    reg->daddr0 = dest_addr;
    reg->framelen = data_len;
    reg->start = (1 << DMA0START_DMASTART);

    while (1){
        if (0 == reg->start){
            break;
        }
    }

    sys_write32((0x101 << (dma_idx)), DMAIP);
}
