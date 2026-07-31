#include <soc.h>
#include <kernel.h>
#include <device.h>

extern int __ram_dsp_irq_vector;
extern int __ram_dsp_start, __ram_dsp_size;

//	/*由于dsp的irq vector固定在0xbfc20000的位置，因此将算法data也放在此处*/
//	ram_adata	: ORIGIN = 0xbfc20000, LENGTH = 0x800
#define DSP_IRQ_VECTOR_ADDR 0x60000 //((void *)&__ram_dsp_irq_vector)

//ram_dsp		: ORIGIN = 0xbfc29000, LENGTH = 0x17000
#define DSP_ADDR 0x69000 //((void *)&__ram_dsp_start)

#define PCM0_ADDR ((void *)0xBFC3C000)
#define PCM1_ADDR ((void *)0xBFC3D000)	//dsp声音输出使用了pcm1和pcm2，pcm0未使用
#define PCM2_ADDR ((void *)0xBFC3E000)
#define PCM_SIZE (4096)

#define FIRST_WRITE_DATA     (0x55aa)
#define FIRST_WAIT_DATA      (0xc)
#define SECOND_WRITE_DATA    (0xc)
#define SECOND_WAIT_DATA     (0x5)
#define THIRD_WRITE_DATA     (0x5)
#define THIRD_WAIT_DATA      (0x0)
#define LAST_WAIT_DATA       (0xf)

//M4K AND DSP 'S communications
#define  CPU_COMM_CMD_EXIST   (0x55aa)
#define  CPU_COMM_CMD_DEAL    (0xccdd)
#define  CPU_COMM_CMD_NULL    (0xeeee)
#define  SRAM_M4K_CMD_BUF_ADDR     (0x9fc09f80)
#define  SRAM_DSP_CMD_BUF_ADDR     (0x9fc09fa0)
#define  SRAM_CPU_SHARE_RAM_ADDR   (0x9fc09fc0)

//object_type
#define  COMM_OBJ_ACK         0
#define  COMM_OBJ_KERNEL      1
#define  COMM_OBJ_MMM         2

//kernel rec cmd
#define  CMD_REQ_OS_PAGEMISS  0
#define  CMD_REQ_OS_BANKPUSH  1
#define  CMD_REQ_OS_LOADDATA  2

#define DSP_RAM_OFFSET  (0x40000)

struct dsp_lib_info {
	const char name[12];
	unsigned char *ptr;
	unsigned int size;
};

struct dsp_code
{
	int offset;
	int length;
	void *address;
};

struct dsp_info
{
	uint8_t   magic[4];
	uint8_t   file_type;
	uint8_t   media_type;
	uint8_t   major_version;
	uint8_t   minor_version;

	struct dsp_code dsp_code[5];
};

struct dsp_cmd
{
    volatile unsigned short state;
    unsigned char content[28];
    volatile unsigned short data;
};

struct dsp_comm
{
	char irq_vectors[0x120];
	struct dsp_cmd mcu_to_dsp;
	struct dsp_cmd dsp_to_mcu;
};


#define DSP_MMAP_START_ADDR  (u32_t)DSP_ADDR
#define DSP_MMAP_TOTAL_SIZE  (76*1024)

#define DSP_MMAP_PAGE_SIZE   (2*1024)
#define DSP_MMAP_PAGE_NUM    (DSP_MMAP_TOTAL_SIZE / DSP_MMAP_PAGE_SIZE)


/* DSP 扩展内存映射
 */
typedef struct
{
	char  lib_name[16];
    u8_t  ext_page[(DSP_MMAP_PAGE_NUM + 7) / 8];

} dsp_mmap_ext_t;


struct dsp
{
	struct sd_file *sd_file;
	//const struct dsp_lib_info *dsp_lib;
	unsigned char *dsp_ptr;

    /* 已建立的内存页面映射
     */
    u8_t  page_map[(DSP_MMAP_PAGE_NUM + 7) / 8];

    /* DSP 扩展内存映射
     */
    u8_t*  ext_page;
};


extern struct dsp  dsps[2];


/* DSP 内存映射分配
 */
void dsp_mmap_alloc(int dsp_no, u32_t start, u32_t size);


/* DSP 内存映射释放
 */
void dsp_mmap_free(int dsp_no);


/* DSP 扩展内存映射
 */
void dsp_mmap_ext(int dsp_no, char* lib_name);



