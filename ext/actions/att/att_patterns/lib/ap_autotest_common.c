/*******************************************************************************
 *                              US212A
 *                            Module: Picture
 *                 Copyright(c) 2003-2012 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>    <time>           <version >             <desc>
 *       zhangxs     2011-09-19 10:00     1.0             build this file
 *******************************************************************************/
/*!
 * \file
 * \brief
 * \author   zhangxs
 * \version  1.0
 * \date  2011/9/05
 *******************************************************************************/
#include "att_pattern_test.h"


struct att_env_var *self;

return_result_t *return_data;
u16_t trans_bytes;

/**
 * @internal
 *
 * @brief Set an interrupt's priority
 *
 * The priority is verified if ASSERT_ON is enabled. The maximum number
 * of priority levels is a little complex, as there are some hardware
 * priority levels which are reserved: three for various types of exceptions,
 * and possibly one additional to support zero latency interrupts.
 *
 * @return N/A
 */
void _irq_priority_set(unsigned int irq, unsigned int prio, uint32_t flags)
{
	unsigned int key;

	if (prio >= 4) {
		return;
	}

	key = irq_lock();

	csi_vic_set_prio(irq, prio);

	irq_unlock(key);
}

/**
 *
 * @brief Enable an interrupt line
 *
 * Enable the interrupt. After this call, the CPU will receive interrupts for
 * the specified <irq>.
 *
 * @return N/A
 */
void _arch_irq_enable(unsigned int irq)
{
	unsigned int key;

	key = irq_lock();

	csi_vic_enable_irq(irq);
	csi_vic_set_wakeup_irq(irq);

	irq_unlock(key);
}

/**
 *
 * @brief Disable an interrupt line
 *
 * Disable an interrupt line. After this call, the CPU will stop receiving
 * interrupts for the specified <irq>.
 *
 * @return N/A
 */
void _arch_irq_disable(unsigned int irq)
{
	unsigned int key;

	key = irq_lock();

	csi_vic_disable_irq(irq);
	csi_vic_clear_wakeup_irq(irq);

	irq_unlock(key);
}

int32 byte_to_unicode(uint8 byte_value, uint16 *unicode_buffer)
{
    if((byte_value <= 9))
    {
        byte_value = (byte_value + '0');
    }
    else if((byte_value >= 10) && (byte_value <= 15))
    {
        byte_value = byte_value - 10 + 'A';
    }
    else
    {
        byte_value = 0;
    }

    *unicode_buffer = byte_value;

    return 1;
}

int32 two_bytes_to_unicode(uint8 byte_value, uint16 *unicode_buffer, uint32 base)
{
    uint8 temp_value;
    uint32 i;
    int32 trans_len = 0;

    for(i = 0; i < 2; i++)
    {
        if(i == 0)
        {
            temp_value = byte_value / base;
        }
        else
        {
            temp_value = byte_value % base;
        }

        trans_len++;

        byte_to_unicode(temp_value, &(unicode_buffer[i]));
    }

    return trans_len;
}

void bytes_to_unicode(uint8 *byte_buffer, uint8 byte_index, uint8 byte_len, uint16 *unicode_buffer, uint16 *unicode_len)
{
    uint32 i;

    int32 trans_len;

    for(i = 0; i < byte_len; i++)
    {
        trans_len = two_bytes_to_unicode(byte_buffer[byte_index - i], unicode_buffer, 16);

        unicode_buffer += trans_len;

        *unicode_len += trans_len;
    }

    return;
}


void uint32_to_unicode(uint32 value, uint16 *unicode_buffer, uint16 *unicode_len, uint32 base)
{
    uint32 i;
    uint32 trans_byte;
    uint8 temp_data[12];
    uint32 div_val;

    i = 0;

    trans_byte = 0;

    div_val = value;

    while(div_val != 0)
    {
        temp_data[i] = div_val % base;
        div_val = div_val / base;
        i++;
    }

    while(i != 0)
    {
        trans_byte = byte_to_unicode(temp_data[i-1], unicode_buffer);
        unicode_buffer += trans_byte;
        *unicode_len += trans_byte;
        i--;
    }

    return;
}

void int32_to_unicode(int32 value, uint16 *unicode_buffer, uint16 *unicode_len, uint32 base)
{
    uint32 i;
    uint32 trans_byte;
    uint8 temp_data[12];
    uint32 div_val;

    if(value == 0)
    {
        *unicode_buffer = '0';
        *unicode_len += 1;
        return;
    }

    i = 0;
    trans_byte = 0;

    if(value < 0)
    {
        *unicode_buffer = '-';
        unicode_buffer++;
        *unicode_len += 1;
        value = 0 - value;
    }

    div_val = value;

    while(div_val != 0)
    {
        temp_data[i] = div_val % base;
        div_val = div_val / base;
        i++;
    }

    while(i != 0)
    {
        trans_byte = byte_to_unicode(temp_data[i-1], unicode_buffer);
        unicode_buffer += trans_byte;
        *unicode_len += trans_byte;
        i--;
    }

    return;
}


int32 act_test_report_result(return_result_t *write_data, uint16 payload_len)
{
    int ret_val;
    uint8 cmd_data[8];

    write_data->timeout = 5;  // Get timeout of test item.

    while(1)
    {
        ret_val = att_write_data(STUB_CMD_ATT_REPORT_TRESULT, payload_len, (u8_t *)write_data);

        if(ret_val == 0)
        {
            ret_val = att_read_data(STUB_CMD_ATT_REPORT_TRESULT, 0, (u8_t *)cmd_data);

            if(ret_val == 0)
            {
                //ACK data
                if (cmd_data[1] == 0x04 && cmd_data[2] == 0xfe)
                {
                    break;
                }
            }
        }
    }

    return ret_val;
}

//Send att command to PC without parameters, and reply has no parameters, call this interface derectly.
bool att_cmd_send(u16_t stub_cmd, u8_t try_count)
{
    uint32 ret_val;
    uint8_t count = 0;

test_start:
    ret_val = att_write_data(stub_cmd, 0, STUB_ATT_RW_TEMP_BUFFER);

    if(ret_val == 0)
    {
        ret_val = att_read_data(STUB_CMD_ATT_ACK, 0, STUB_ATT_RW_TEMP_BUFFER);

        if(ret_val == 0)
        {
            ret_val = TRUE;
        }
        else
        {
            ret_val = FALSE;
        }
    }
    else
    {
        ret_val = FALSE;
    }

    if(++count < try_count && ret_val == FALSE)
    {
        k_sleep(150);
        goto test_start;
    }

    if(ret_val == FALSE)
    {
        print_log("att cmd :0x%x fail, try count:%d\n", stub_cmd, try_count);
    }

    return ret_val;
}

void att_write_test_info(char *test_info, uint32 test_value, uint32 write_data)
{
    if (write_data == TRUE)
    {
        printk("%s %u\n",test_info, test_value);
    }
    else
    {
        printk("%s\n",test_info);
    }

    if (att_get_test_mode() == TEST_MODE_CARD)
    {
        //wriet_log_data(test_info, test_value, write_data);
    }

    return;
}

int32 act_test_report_test_log(uint32 ret_val, uint32 test_id)
{
    if (ret_val == FALSE)
    {
        att_write_test_info("test fail: ", test_id, 1);
    }
    else
    {
        att_write_test_info("test ok: ", test_id, 1);
    }

    att_write_test_info("   ", 0, 0);

    return TRUE;
}

void act_test_change_test_timeout(uint16 timeout)
{
    uint32 ret_val;

    uint8 *cmd_data;

    cmd_data = (uint8*)STUB_ATT_RW_TEMP_BUFFER;

    //Timeout in second
    cmd_data[6] = (timeout & 0xff);
    cmd_data[7] = ((timeout >> 8) & 0xff);
    cmd_data[8] = 0;
    cmd_data[9] = 0;

    ret_val = att_write_data(STUB_CMD_ATT_GET_TEST_ID, 4, STUB_ATT_RW_TEMP_BUFFER);

    if(ret_val == 0)
    {
        ret_val = att_read_data(STUB_CMD_ATT_GET_TEST_ID, 4, STUB_ATT_RW_TEMP_BUFFER);

        if(ret_val == 0)
        {
            //Reply ACK
            ret_val = att_write_data(STUB_CMD_ATT_ACK, 0, STUB_ATT_RW_TEMP_BUFFER);
        }
    }

    return;
}

int32 att_write_data(uint16 cmd, uint32 payload_len, uint8 *data_addr)
{
	return self->api->att_write_data(cmd, payload_len, data_addr);
}

int32 att_read_data(uint16 cmd, uint32 payload_len, uint8 *data_addr)
{
	return self->api->att_read_data(cmd, payload_len, data_addr);
}


int printk(const char *fmt, ...)
{
	int ret;
	va_list ap;

	va_start(ap, fmt);
	ret = self->api->vprintk(fmt, ap);
	va_end(ap);

	return ret;
}

u32_t k_uptime_get_32(void)
{
	return self->api->k_uptime_get_32();
}

void k_sem_init(struct k_sem *sem, unsigned int initial_count,
		unsigned int limit)
{
	self->api->k_sem_init(sem, initial_count, limit);
}

void k_sem_give(struct k_sem *sem)
{
	self->api->k_sem_give(sem);
}

int k_sem_take(struct k_sem *sem, s32_t timeout)
{
	return self->api->k_sem_take(sem, timeout);
}

void *z_malloc(size_t size)
{
	return self->api->z_malloc(size);
}

void free(void *ptr)
{
	self->api->free(ptr);
}

void k_sleep(s32_t duration)
{
	self->api->k_sleep(duration);
}

void *k_thread_create(void *new_thread,
			void *stack,
			u32_t stack_size, void (*entry)(void *, void *, void*),
			void *p1, void *p2, void *p3,
			int prio, u32_t options, s32_t delay)
{
	return self->api->k_thread_create(new_thread, stack, stack_size, entry, p1, p2, p3, prio, options, delay);

}


void* hal_aout_channel_open(audio_out_init_param_t *aout_setting)
{
	return self->api->hal_aout_channel_open(aout_setting);
}

int hal_aout_channel_close(void *aout_channel_handle)
{
	return self->api->hal_aout_channel_close(aout_channel_handle);
}

int hal_aout_channel_write_data(void *aout_channel_handle, uint8_t *buff, uint32_t len)
{
	return self->api->hal_aout_channel_write_data(aout_channel_handle, buff, len);
}

int hal_aout_channel_start(void *aout_channel_handle)
{
	return self->api->hal_aout_channel_start(aout_channel_handle);
}

int hal_aout_channel_stop(void *aout_channel_handle)
{
	return self->api->hal_aout_channel_stop(aout_channel_handle);
}

void* hal_ain_channel_open(audio_in_init_param_t *ain_setting)
{
	return self->api->hal_ain_channel_open(ain_setting);
}

int32_t hal_ain_channel_close(void *ain_channel_handle)
{
	return self->api->hal_ain_channel_close(ain_channel_handle);
}

int hal_ain_channel_read_data(void *ain_channel_handle, uint8_t *buff, uint32_t len)
{
	return self->api->hal_ain_channel_read_data(ain_channel_handle, buff, len);
}
int hal_ain_channel_start(void* ain_channel_handle)
{
	return self->api->hal_ain_channel_start(ain_channel_handle);
}

int hal_ain_channel_stop(void* ain_channel_handle)
{
	return self->api->hal_ain_channel_stop(ain_channel_handle);
}


int bt_manager_get_rdm_state(struct bt_dev_rdm_state *state)
{
	return self->api->bt_manager_get_rdm_state(state);
}

void bt_manager_install_stack(u8_t *addr, u8_t mode)
{
	self->api->att_btstack_install(addr, mode);
}

void bt_manager_uninstall_stack(u8_t *addr, u8_t mode)
{
	self->api->att_btstack_uninstall(addr, mode);
}

void bt_manager_check_accept_call(struct bt_audio_call *call)
{
	self->api->bt_manager_call_accept(call);
}

int get_nvram_db_data(u8_t db_type, u8_t* addr, u32_t len)
{
	return self->api->get_nvram_db_data(db_type, addr, len);
}

int set_nvram_db_data(u8_t db_type, u8_t* addr, u32_t len)
{
	return self->api->set_nvram_db_data(db_type, addr, len);
}

int freq_compensation_read(uint32_t *trim_cap, uint32_t mode)
{
	return self->api->freq_compensation_read(trim_cap, mode);
}

int freq_compensation_write(uint32_t *trim_cap, uint32_t mode)
{
	return self->api->freq_compensation_write(trim_cap, mode);
}
void enter_BQB_mode(void)
{
    self->api->enter_BQB_mode();
}
int get_BQB_info(void)
{
    return self->api->get_BQB_info();

}


//void bt_manager_set_not_linkkey_connect(void)
//{
//	return self->api->bt_manager_set_not_linkkey_connect();
//}

void sys_pm_reboot(int reboot_type)
{
	return self->api->sys_pm_reboot(reboot_type);
}

int att_flash_erase(off_t offset, size_t size)
{
    return self->api->att_flash_erase(offset,size);
}
int board_audio_device_mapping(uint16_t logical_dev, uint8_t *physical_dev, uint8_t *track_flag)
{
    return self->api->board_audio_device_mapping(logical_dev,physical_dev,track_flag);
}

#if 0

u32_t mp_get_hosc_cap(void)
{
	return self->api->mp_get_hosc_cap();
}

void mp_set_hosc_cap(uint32 cap)
{
	self->api->mp_set_hosc_cap(cap);
}

void mp_system_hardware_init(void)
{
	self->api->mp_system_hardware_init();
}

void mp_core_init(void * mp_manager)
{
	self->api->mp_core_init(mp_manager);
}

void mp_controller_free_irq(void)
{
	self->api->mp_controller_free_irq();
}

void mp_system_hardware_deinit(void)
{
	self->api->mp_system_hardware_deinit();
}

u8_t mp_packet_send_process(void)
{
	return self->api->mp_packet_send_process();
}

u8_t mp_packet_receive_process(void)
{
	return self->api->mp_packet_receive_process();
}

int8 mp_set_packet_param(void * packet_ctrl)
{
	return self->api->mp_set_packet_param(packet_ctrl);
}
#endif

/**
 *  @brief  read data from one sub file in atf file by name
 *
 *  @param  dst_addr  : buffer address
 *  @param  dst_buffer_len : buffer size
 *  @param  sub_file_name : sub file name
 *  @param  offset    : offset of the atf file head
 *  @param  read_len  : read bytes
 *
 *  @return  negative errno code on fail, or return read bytes
 */
int read_atf_sub_file(u8_t *dst_addr, u32_t dst_buffer_len, const u8_t *sub_file_name, s32_t offset, s32_t read_len, atf_dir_t *sub_atf_dir)
{
	printk("%p\n", self->api->read_file);
	return self->api->read_file(dst_addr, dst_buffer_len, sub_file_name, offset, read_len, sub_atf_dir);
}

u8_t att_get_test_mode(void)
{
	return self->test_mode;
}

void print_buffer(const void *addr, int width, int count, int linelen, \
	unsigned long disp_addr)
{
	self->api->print_buffer(addr, width, count, linelen, disp_addr);
}
