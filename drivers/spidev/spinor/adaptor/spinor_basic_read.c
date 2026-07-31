#include <sdk.h>
#include <spi/spinor.h>
#include <irq_lowlevel.h>
#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_SPINOR_BASIC_READ
#if 0

extern unsigned char randomizer_enable;

//extern spinor_rom_cfg_t g_spinor_cfg;

void spinor_basic_select_spinor(int select)
{
	if(select)
		SPI_CS_LOW();
	else
		SPI_CS_HIGH();
	Time_Delay();
}

void spinor_basic_set_to_send_mode(int dma_mode)
{
	SPI_SET_RW_MODE(2);             //select work mode 0,1:write/read 2:write only 3:read only
	SPI_SET_3_WIRE_WRITE();         //switch to write mode
	if(dma_mode)
	{
		SPI_SET_BUS_WIDTH_32BIT();
		SPI_SET_TX_DMA();
	}
	else
	{
		SPI_SET_BUS_WIDTH_8BIT();
		SPI_SET_TX_CPU();
	}
	SPI_RESET_FIFO();

	Time_Delay();
}

void spinor_basic_set_to_receive_mode(int dma_mode)
{
	SPI_SET_RW_MODE(3);             //select work mode 0,1:write/read 2:write only 3:read only
	SPI_SET_3_WIRE_READ();          //switch to read mode
	if(dma_mode)
	{
		SPI_SET_BUS_WIDTH_32BIT();
		SPI_SET_RX_DMA();

	}
	else
	{
		SPI_SET_BUS_WIDTH_8BIT();
		SPI_SET_RX_CPU();
	}
	SPI_RESET_FIFO();

	Time_Delay();
}

void spinor_basic_send_data(unsigned char *send_buf, int len)
{
	spinor_basic_set_to_send_mode(0);
	while(len-- > 0)
	{
		WRITE_TX_FIFO(*send_buf++);
		//spinor_basic_wait_transfrer_complete();
		Wait_Transfr_Cmplt();
	}
}

void spinor_basic_receive_data_by_cpu(unsigned char *receive_buf, int len)
{
	spinor_basic_set_to_receive_mode(0);
	SPI_READ_COUNT(len);
	SPI_READ_START();
	while(len-- > 0)
	{
		WAIT_SPI_RX_NOT_EMPTY();
		*receive_buf++ = READ_RX_FIFO();
	}
}


//void spinor_basic_wait_transfrer_complete(void)
//{
//	while (!(act_readl(SPI_STA) & (1 << SPI_STA_TXEM)));
//	while (act_readl(SPI_STA) & (1 << SPI_STA_SPI_BUSY));
//}



void spinor_transfer_24bit_address(unsigned char *buf, unsigned int addr)
{
	union
	{
		unsigned int  addr_int;
		unsigned char addr_char[4];
	} union_addr;

	union_addr.addr_int = addr;
	buf[0] = union_addr.addr_char[2];
	buf[1] = union_addr.addr_char[1];
	buf[2] = union_addr.addr_char[0];
}

unsigned char spinor_basic_read_status_register(void)
{
	#define SPINOR_READ_STATUS_COMMAND 0x05

	unsigned int flags;
	unsigned char cmd = SPINOR_READ_STATUS_COMMAND, status;

	flags = __local_irq_save();
	spinor_basic_select_spinor(1);
	spinor_basic_send_data(&cmd, 1);
	spinor_basic_receive_data_by_cpu(&status, 1);
	spinor_basic_select_spinor(0);
	__local_irq_restore(flags);

	return status;
}

//unsigned char program_cmd[4];
//void *program_page_buf;
//unsigned char program_random;

void spinor_basic_write_enable(void)
{
    #define SPINOR_WRITE_ENABLE_COMMAND 0x06

	unsigned char cmd = SPINOR_WRITE_ENABLE_COMMAND;

	spinor_basic_select_spinor(1);
	spinor_basic_send_data(&cmd, 1);
	spinor_basic_select_spinor(0);
}

#if 0
uint32 spinor_basic_enter_4B_mode(spinor_op_ctx_t *ctx)
{
    #define SPINOR_ENTER_4B_COMMAND 0xB7

    #define SPINOR_READ_CONFIGURE_STATUS_COMMAND 0x15

    unsigned char status;

    ctx->cfg->irq_flag = ctx->cfg->check_spinor_busy_and_wait(ctx);

    Write_Switch(SPINOR_ENTER_4B_COMMAND);

    ctx->cfg->irq_restore(ctx->cfg->irq_flag);

    ctx->cfg->irq_flag = ctx->cfg->check_spinor_busy_and_wait(ctx);

	status = SPI_Read_Status_Sub(SPINOR_READ_CONFIGURE_STATUS_COMMAND);

	ctx->cfg->irq_restore(ctx->cfg->irq_flag);

	ACT_LOG_ID_INF(ALF_STR_XXX__ENTER_4B_STATUSXN, 1, status);

	return status;
}

uint32 spinor_basic_exit_4B_mode(spinor_op_ctx_t *ctx)
{
    #define SPINOR_EXIT_4B_COMMAND 0xE9

    #define SPINOR_READ_CONFIGURE_STATUS_COMMAND 0x15

    unsigned char status;

    ctx->cfg->irq_flag = ctx->cfg->check_spinor_busy_and_wait(ctx);

    Write_Switch(SPINOR_EXIT_4B_COMMAND);

    ctx->cfg->irq_restore(ctx->cfg->irq_flag);

    ctx->cfg->irq_flag = ctx->cfg->check_spinor_busy_and_wait(ctx);

	status = SPI_Read_Status_Sub(SPINOR_READ_CONFIGURE_STATUS_COMMAND);

	ctx->cfg->irq_restore(ctx->cfg->irq_flag);

	ACT_LOG_ID_INF(ALF_STR_XXX__ENTER_4B_STATUSXN, 1, status);

	return status;
}

#endif
#if 0
void spinor_basic_program_one_page(void)
{
	spinor_basic_write_enable();
	spinor_basic_select_spinor(1);
	spinor_basic_send_data(program_cmd, 4);
	spinor_basic_set_to_send_mode(1);

	if(program_random)
		SPI_RESUME_RANDOMIZE();

	act_writel(0x0, DMA6CTL);		//reset dma
	act_writel((uint32)program_page_buf, DMA6SADDR0);
	act_writel(256, DMA6FrameLen);
	act_writel(0x0000C201, DMA6CTL);	//start dma
}
#endif

#if 0
unsigned int spinor_basic_wait_rb_ready_and_irq_saved(void)
{
	#define STATUS_BUSY_MASK 0x1
	unsigned int flags;

	flags = __local_irq_save();
	while(1)
	{
		if((g_spinor_cfg.dma_reg->ctl & 0x1) == 0
			&& (act_readl(SPI0_STA) & (1 << SPI0_STA_TXEM))
			&& !(act_readl(SPI0_STA) & (1 << SPI0_STA_SPI0_BUSY)))
		{
			//if(program_page_buf && program_random)
			//	SPI_PAUSE_RANDOMIZE();
			//else
				SPI_DISABLE_READ_WRITE_RANDOMIZE();
			spinor_basic_select_spinor(0);
			if(!(spinor_basic_read_status_register() & STATUS_BUSY_MASK))
			{
				//if(program_page_buf)
				//{
				//	spinor_basic_program_one_page();
				//	program_page_buf = NULL;
				//}
				//else
				//{
					break;
				//}
			}
		}
		__local_irq_restore(flags);
		flags = __local_irq_save();
	}

	return flags;
}
#endif

#endif

void spinor_basic_fast_read_sectors(unsigned int lba, void *sector_buf, int sectors, int random)
{
     spinor_read(lba, sector_buf, sectors, random);
}
