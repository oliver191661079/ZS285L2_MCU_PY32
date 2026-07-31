/********************************************************************************
 *                            USDK(ZS283A)
 *                            Module: SYSTEM
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>      <time>                      <version >          <desc>
 *      wuyufan   2018-10-12-下午12:48:04             1.0             build this file
 ********************************************************************************/
/*!
 * \file     soc_memctrl.h
 * \brief
 * \author
 * \version  1.0
 * \date  2018-10-12-下午12:48:04
 *******************************************************************************/

#ifndef SOC_MEMCTRL_H_
#define SOC_MEMCTRL_H_

#define     MEMORYCTL                                                         (MEMORY_CONTROLLER_REG_BASE+0x00000000)
#define     ACCESSCTL                                                         (MEMORY_CONTROLLER_REG_BASE+0x00000004)
#define     CPU_ERROR_ADDR                                                    (MEMORY_CONTROLLER_REG_BASE+0x00000008)
#define     CACHE_CONFLICT_ADDR                                               (MEMORY_CONTROLLER_REG_BASE+0x0000000C)
#define     SPI0_ICACHE_START_ADDR                                            (MEMORY_CONTROLLER_REG_BASE+0x00000010)
#define     SPI0_ICACHE_END_ADDR                                              (MEMORY_CONTROLLER_REG_BASE+0x00000014)
#define     SPI1_DCACHE_START_ADDR                                            (MEMORY_CONTROLLER_REG_BASE+0x00000018)
#define     SPI1_DCACHE_END_ADDR                                              (MEMORY_CONTROLLER_REG_BASE+0x0000001C)
#define     DBGCTL                                                            (MEMORY_CONTROLLER_REG_BASE+0x00000040)
#define     DSPPAGEADDR0                                                      (MEMORY_CONTROLLER_REG_BASE+0x00000080)
#define     DSPPAGEADDR1                                                      (MEMORY_CONTROLLER_REG_BASE+0x00000084)
#define     DSPPAGEADDR2                                                      (MEMORY_CONTROLLER_REG_BASE+0x00000088)
#define     DSPPAGEADDR3                                                      (MEMORY_CONTROLLER_REG_BASE+0x0000008c)
#define     MPUIE                                                             (MEMORY_CONTROLLER_REG_BASE+0x00000100)
#define     MPUIP                                                             (MEMORY_CONTROLLER_REG_BASE+0x00000104)
#define     MPUCTL0                                                           (MEMORY_CONTROLLER_REG_BASE+0x00000110)
#define     MPUBASE0                                                          (MEMORY_CONTROLLER_REG_BASE+0x00000114)
#define     MPUEND0                                                           (MEMORY_CONTROLLER_REG_BASE+0x00000118)
#define     MPUERRADDR0                                                       (MEMORY_CONTROLLER_REG_BASE+0x0000011c)
#define     MPUCTL1                                                           (MEMORY_CONTROLLER_REG_BASE+0x00000120)
#define     MPUBASE1                                                          (MEMORY_CONTROLLER_REG_BASE+0x00000124)
#define     MPUEND1                                                           (MEMORY_CONTROLLER_REG_BASE+0x00000128)
#define     MPUERRADDR1                                                       (MEMORY_CONTROLLER_REG_BASE+0x0000012c)
#define     MPUCTL2                                                           (MEMORY_CONTROLLER_REG_BASE+0x00000130)
#define     MPUBASE2                                                          (MEMORY_CONTROLLER_REG_BASE+0x00000134)
#define     MPUEND2                                                           (MEMORY_CONTROLLER_REG_BASE+0x00000138)
#define     MPUERRADDR2                                                       (MEMORY_CONTROLLER_REG_BASE+0x0000013c)
#define     MPUCTL3                                                           (MEMORY_CONTROLLER_REG_BASE+0x00000140)
#define     MPUBASE3                                                          (MEMORY_CONTROLLER_REG_BASE+0x00000144)
#define     MPUEND3                                                           (MEMORY_CONTROLLER_REG_BASE+0x00000148)
#define     MPUERRADDR3                                                       (MEMORY_CONTROLLER_REG_BASE+0x0000014c)
#define     SPI1_DCACHE_MPUIE                                                 (MEMORY_CONTROLLER_REG_BASE+0x00000180)
#define     SPI1_DCACHE_MPUIP                                                 (MEMORY_CONTROLLER_REG_BASE+0x00000184)
#define     SPI1_DCACHE_MPUCTL0                                               (MEMORY_CONTROLLER_REG_BASE+0x00000190)
#define     SPI1_DCACHE_MPUBASE0                                              (MEMORY_CONTROLLER_REG_BASE+0x00000194)
#define     SPI1_DCACHE_MPUEND0                                               (MEMORY_CONTROLLER_REG_BASE+0x00000198)
#define     SPI1_DCACHE_MPUERRADDR0                                           (MEMORY_CONTROLLER_REG_BASE+0x0000019c)
#define     BIST_EN0                                                          (MEMORY_CONTROLLER_REG_BASE+0x00000200)
#define     BIST_FIN0                                                         (MEMORY_CONTROLLER_REG_BASE+0x00000204)
#define     BIST_INFO0                                                        (MEMORY_CONTROLLER_REG_BASE+0x00000208)
#define     SPI_CACHE_MAPPING_ADDR0                                           (MEMORY_CONTROLLER_REG_BASE+0x00000300)
#define     SPI_CACHE_ADDR0_ENTRY                                             (MEMORY_CONTROLLER_REG_BASE+0x00000304)
#define     SPI_CACHE_MAPPING_ADDR1                                           (MEMORY_CONTROLLER_REG_BASE+0x00000308)
#define     SPI_CACHE_ADDR1_ENTRY                                             (MEMORY_CONTROLLER_REG_BASE+0x0000030c)
#define     SPI_CACHE_MAPPING_ADDR2                                           (MEMORY_CONTROLLER_REG_BASE+0x00000310)
#define     SPI_CACHE_ADDR2_ENTRY                                             (MEMORY_CONTROLLER_REG_BASE+0x00000314)
#define     SPI_CACHE_MAPPING_ADDR3                                           (MEMORY_CONTROLLER_REG_BASE+0x00000318)
#define     SPI_CACHE_ADDR3_ENTRY                                             (MEMORY_CONTROLLER_REG_BASE+0x0000031c)
#define     SPI_CACHE_MAPPING_ADDR4                                           (MEMORY_CONTROLLER_REG_BASE+0x00000320)
#define     SPI_CACHE_ADDR4_ENTRY                                             (MEMORY_CONTROLLER_REG_BASE+0x00000324)
#define     SPI_CACHE_MAPPING_ADDR5                                           (MEMORY_CONTROLLER_REG_BASE+0x00000328)
#define     SPI_CACHE_ADDR5_ENTRY                                             (MEMORY_CONTROLLER_REG_BASE+0x0000032c)
#define     SPI_CACHE_MAPPING_ADDR6                                           (MEMORY_CONTROLLER_REG_BASE+0x00000330)
#define     SPI_CACHE_ADDR6_ENTRY                                             (MEMORY_CONTROLLER_REG_BASE+0x00000334)
#define     SPI_CACHE_MAPPING_ADDR7                                           (MEMORY_CONTROLLER_REG_BASE+0x00000338)
#define     SPI_CACHE_ADDR7_ENTRY                                             (MEMORY_CONTROLLER_REG_BASE+0x0000033c)
#define     DSP_SPI1_DCACHE_MAPPING_ADDR0                                     (MEMORY_CONTROLLER_REG_BASE+0x00000380)
#define     DSP_SPI1_DCACHE_ADDR0_ENTRY                                       (MEMORY_CONTROLLER_REG_BASE+0x00000384)
#define     DSP_SPI1_DCACHE_MAPPING_ADDR1                                     (MEMORY_CONTROLLER_REG_BASE+0x00000388)
#define     DSP_SPI1_DCACHE_ADDR1_ENTRY                                       (MEMORY_CONTROLLER_REG_BASE+0x0000038c)
#define     DSP_SPI1_DCACHE_MAPPING_ADDR2                                     (MEMORY_CONTROLLER_REG_BASE+0x00000390)
#define     DSP_SPI1_DCACHE_ADDR2_ENTRY                                       (MEMORY_CONTROLLER_REG_BASE+0x00000394)
#define     DSP_SPI1_DCACHE_MAPPING_ADDR3                                     (MEMORY_CONTROLLER_REG_BASE+0x00000398)
#define     DSP_SPI1_DCACHE_ADDR3_ENTRY                                       (MEMORY_CONTROLLER_REG_BASE+0x0000039c)
#define     CODE_REPLACE_CTL                                                  (MEMORY_CONTROLLER_REG_BASE+0x00000400)
#define     CODE_REPLACE_MATCH_FLAG_EN                                        (MEMORY_CONTROLLER_REG_BASE+0x00000404)
#define     CODE_REPLACE_MATCH_FLAG                                           (MEMORY_CONTROLLER_REG_BASE+0x00000408)
#define     CODE_REPLACE_MATCH_PD                                             (MEMORY_CONTROLLER_REG_BASE+0x0000040c)
#define     ALT_DATA0                                                         (MEMORY_CONTROLLER_REG_BASE+0x00000410)
#define     ALT_DATA1                                                         (MEMORY_CONTROLLER_REG_BASE+0x00000414)
#define     ALT_DATA2                                                         (MEMORY_CONTROLLER_REG_BASE+0x00000418)
#define     ALT_DATA3                                                         (MEMORY_CONTROLLER_REG_BASE+0x0000041c)
#define     ALT_DATA4                                                         (MEMORY_CONTROLLER_REG_BASE+0x00000420)
#define     ALT_DATA5                                                         (MEMORY_CONTROLLER_REG_BASE+0x00000424)
#define     ALT_DATA6                                                         (MEMORY_CONTROLLER_REG_BASE+0x00000428)
#define     ALT_DATA7                                                         (MEMORY_CONTROLLER_REG_BASE+0x0000042c)
#define     ALT_DATA8                                                         (MEMORY_CONTROLLER_REG_BASE+0x00000430)
#define     ALT_DATA9                                                         (MEMORY_CONTROLLER_REG_BASE+0x00000434)
#define     ALT_DATA10                                                        (MEMORY_CONTROLLER_REG_BASE+0x00000438)
#define     ALT_DATA11                                                        (MEMORY_CONTROLLER_REG_BASE+0x0000043c)
#define     ALT_DATA12                                                        (MEMORY_CONTROLLER_REG_BASE+0x00000440)
#define     ALT_DATA13                                                        (MEMORY_CONTROLLER_REG_BASE+0x00000444)
#define     ALT_DATA14                                                        (MEMORY_CONTROLLER_REG_BASE+0x00000448)
#define     ALT_DATA15                                                        (MEMORY_CONTROLLER_REG_BASE+0x0000044c)
#define     DFIXADDR0                                                         (MEMORY_CONTROLLER_REG_BASE+0x00000480)
#define     DFIXADDR1                                                         (MEMORY_CONTROLLER_REG_BASE+0x00000484)
#define     DFIXADDR2                                                         (MEMORY_CONTROLLER_REG_BASE+0x00000488)
#define     DFIXADDR3                                                         (MEMORY_CONTROLLER_REG_BASE+0x0000048c)
#define     DFIXADDR4                                                         (MEMORY_CONTROLLER_REG_BASE+0x00000490)
#define     DFIXADDR5                                                         (MEMORY_CONTROLLER_REG_BASE+0x00000494)
#define     DFIXADDR6                                                         (MEMORY_CONTROLLER_REG_BASE+0x00000498)
#define     DFIXADDR7                                                         (MEMORY_CONTROLLER_REG_BASE+0x0000049c)
#define     DFIXADDR8                                                         (MEMORY_CONTROLLER_REG_BASE+0x000004a0)
#define     DFIXADDR9                                                         (MEMORY_CONTROLLER_REG_BASE+0x000004a4)
#define     DFIXADDR10                                                        (MEMORY_CONTROLLER_REG_BASE+0x000004a8)
#define     DFIXADDR11                                                        (MEMORY_CONTROLLER_REG_BASE+0x000004ac)
#define     DFIXADDR12                                                        (MEMORY_CONTROLLER_REG_BASE+0x000004b0)
#define     DFIXADDR13                                                        (MEMORY_CONTROLLER_REG_BASE+0x000004b4)
#define     DFIXADDR14                                                        (MEMORY_CONTROLLER_REG_BASE+0x000004b8)
#define     DFIXADDR15                                                        (MEMORY_CONTROLLER_REG_BASE+0x000004bc)
#define     SYMBOL_TABLE_PTR0                                                 (MEMORY_CONTROLLER_REG_BASE+0x00000600)
#define     SYMBOL_TABLE_PTR1                                                 (MEMORY_CONTROLLER_REG_BASE+0x00000604)
#define     SYMBOL_TABLE_PTR2                                                 (MEMORY_CONTROLLER_REG_BASE+0x00000608)
#define     SYMBOL_TABLE_PTR3                                                 (MEMORY_CONTROLLER_REG_BASE+0x0000060c)

#define     ROM_GLOB_ADDR             SYMBOL_TABLE_PTR2  //0xc00a011c

#define     PSRAM_DMA_BASE_ADDR  (0x68000000)

#define     CACHE_MAPPING_REGISTER_BASE	(0xc00a0300)

#define     CACHE_MAPPING_ITEM_NUM			(8)

#define     MEMORYCTL_BUSERROR_BIT          BIT(3)
#define     MEMORYCTL_CRCERROR_BIT          BIT(2)
#define     MEMORYCTL_INVALID_ADDR_BIT      BIT(5)

#define     CACHE_LINE_SIZE                 32
#define     CACHE_CRC_LINE_SIZE             34


typedef struct {
	volatile unsigned int mapping_addr;
	volatile unsigned int mapping_entry;
} cache_mapping_register_t;

typedef struct
{
    volatile uint32_t MPUCTL;
    volatile uint32_t MPUBASE;
    volatile uint32_t MPUEND;
    volatile uint32_t MPUERRADDR;
}mpu_base_register_t;

void soc_memctrl_set_mapping(int idx, u32_t cpu_addr, u32_t nor_bus_addr);
int soc_memctrl_mapping(u32_t cpu_addr, u32_t nor_phy_addr, int enable_crc);

u32_t soc_memctrl_cpu_to_nor_phy_addr(u32_t cpu_addr);

void *soc_memctrl_create_temp_mapping(u32_t nor_phy_addr, int enable_crc);
void soc_memctrl_clear_temp_mapping(void *cpu_addr);

static inline void soc_memctrl_set_dsp_mapping(int idx, u32_t map_addr,
					  u32_t phy_addr)
{
	if (idx > 3)
		return;

	sys_write32(phy_addr, DSP_SPI1_DCACHE_ADDR0_ENTRY + idx * 8);
	sys_write32(map_addr, DSP_SPI1_DCACHE_MAPPING_ADDR0 + idx * 8);
}

#endif /* SOC_MEMCTRL_H_ */
