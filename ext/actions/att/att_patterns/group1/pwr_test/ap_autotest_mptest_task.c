#include "att_pattern_test.h"
#include "mp_app.h"


mp_manager_info_t * mp_manager_ptr = NULL;
static thread_status_e mp_thread_status = THREAD_NULL;
mp_private_t *p_mp;
/*
void mp_system_hardware_deinit(void)
{
    sys_write32(sys_read32(CMU_DEVCLKEN1) & ~0x, mem_addr_t addr)
}
*/

void mp_manager_param_init(void)
{
    mp_manager_ptr->mp_task_running = 0;

    mp_manager_ptr->mp_task_end = 0;

    mp_manager_ptr->mp_thread_sleep_time = 10;

    mp_manager_ptr->mp_cap_val = mp_get_hosc_cap();

    mp_manager_ptr->mp_cap_trim = 0;

}

void mp_manager_packet_init(void)
{
    mp_manager_ptr->mp_packet_info.rf_channel = MP_DEFAULT_CHANNEL;
    mp_manager_ptr->mp_packet_info.rf_power_index = 10;
    mp_manager_ptr->mp_packet_info.pkt_type = MP_PKT_DH1 ;
    mp_manager_ptr->mp_packet_info.payload_data_type = K_MP_01010101;
    mp_manager_ptr->mp_packet_info.payload_len = 27;
    mp_manager_ptr->mp_packet_info.pkt_len = 72+54+0+0+24 + mp_manager_ptr->mp_packet_info.payload_len*8+0; ;
}

void mp_manager_callback(uint_t event, void *param)
{
    if(mp_manager_ptr->mp_event_callback != NULL)
    {
        mp_manager_ptr->mp_event_callback(event, param);
    }
}

void mp_manager_cbk_reg(mp_msg_callback_func cbk)
{
    mp_manager_ptr->mp_event_callback = cbk;
}

void mp_bt_control_handle(void *p1, void *p2, void *p3)
{
    mp_manager_ptr = (mp_manager_info_t *)p1;

    if(mp_manager_ptr == NULL)
    {
        return;
    }

    mp_system_hardware_init();

    mp_manager_param_init();

    mp_core_init(mp_manager_ptr);

	mp_manager_packet_init();

	mp_manager_callback(MP_INIT_COMPLETE_EVENT, NULL);

    if(mp_thread_status != THREAD_CREATE)
        goto exit;
    mp_thread_status = THREAD_RUNNING;

    while (mp_thread_status != THREAD_REQUEST_EXIT)
    {
        if(mp_manager_ptr->mp_task_running == 0)
        {
            if(mp_manager_ptr->mp_core_state == MP_CORE_STATE_NONE)
            {
                goto mp_core_loop;
            }

            if(mp_manager_ptr->mp_core_state == MP_CORE_SEND_PACKET)
            {
                mp_manager_ptr->mp_task_running = 1;
                mp_manager_ptr->mp_tx_packet_step = 0;
                printk("mp packet tx start!\n");
            }

            else if(mp_manager_ptr->mp_core_state == MP_CORE_RECEIVE_PACKET)
            {
                mp_manager_ptr->mp_task_running = 1;
                mp_manager_ptr->mp_rx_packet_step = 0;

                printk("mp packet rx start!\n");
            }

            else if(mp_manager_ptr->mp_core_state == MP_CORE_SINGLE_TONE)
            {
                mp_manager_ptr->mp_task_running = 1;
                mp_manager_ptr->mp_single_tone_step = 0;
                printk("mp single tone start!\n");
            }
            else
            {
                ;
            }

        }
        else
        {
            if(mp_manager_ptr->mp_core_state == MP_CORE_SEND_PACKET)
            {
                mp_packet_send_process();
                mp_manager_ptr->mp_thread_sleep_time = 1;
            }

            else if (mp_manager_ptr->mp_core_state == MP_CORE_RECEIVE_PACKET)
            {

                //uint32_t timestamp = k_uptime_get_32();
                mp_packet_receive_process();
                mp_manager_ptr->mp_thread_sleep_time = 1;
                if(mp_manager_ptr->mp_rx_data_ready == 1)
                {
                    //printk("rx time:%d\n",k_uptime_get_32() - timestamp);
                    mp_manager_callback(MP_RX_PACKET_VALID, mp_manager_ptr);
                    mp_manager_ptr->mp_rx_data_ready = 0;
                }
            }
            else if(mp_manager_ptr->mp_core_state == MP_CORE_SINGLE_TONE)
            {
                //mp_single_tone_process(mp_manager_ptr);
                mp_manager_ptr->mp_thread_sleep_time = 10;
            }
            else
            {
                ;

            }
            if(mp_manager_ptr->mp_task_end == 1)
            {
                mp_manager_ptr->mp_thread_sleep_time = 10;
            }
        }

mp_core_loop:
        if (mp_manager_ptr->mp_thread_exit == 1)
        {
            //mp_set_rf_idle();
            break;
        }

        k_sleep(mp_manager_ptr->mp_thread_sleep_time);
    }

    mp_manager_ptr->mp_thread_exit = 0;

exit:
    //mp_system_hardware_deinit();
    mp_manager_ptr = NULL;
    mp_thread_status = THREAD_EXIT;
    mp_controller_free_irq();
    mp_system_hardware_deinit();

    return;
}

void mp_task_tx_stop(void)
{
    if(mp_manager_ptr && mp_manager_ptr->mp_task_running && !mp_manager_ptr->mp_task_end)
    {
        mp_manager_ptr->mp_task_end = 1;
        while(mp_manager_ptr->mp_task_end)
        {
            k_sleep(1);
        }
    }
}

void mp_task_tx_start(uint8 channel)
{
    mp_packet_ctrl_t packet_ctrl;
    if(mp_manager_ptr == NULL)
    {
        printk("mp thread not work\n");
        return;
    }
    packet_ctrl.channel = channel;
    packet_ctrl.power = 10;
    packet_ctrl.packets = 0;
    packet_ctrl.timeout = 0;

    mp_set_packet_param(&packet_ctrl);

    mp_manager_ptr->mp_core_state = MP_CORE_SEND_PACKET;
    while(!mp_manager_ptr->mp_task_running)
    {
        k_sleep(1);
    }
}

void mp_task_rx_start(uint8 channel, mp_msg_callback_func cbk, uint32_t timeout)
{
    mp_packet_ctrl_t packet_ctrl;
    if(mp_manager_ptr == NULL)
    {
        printk("mp thread not work\n");
        return;
    }
    packet_ctrl.channel = channel;
    packet_ctrl.power = 10;
    packet_ctrl.packets = 0;
    packet_ctrl.timeout = timeout;

    mp_set_packet_param(&packet_ctrl);
    mp_manager_cbk_reg(cbk);

    mp_manager_ptr->mp_core_state = MP_CORE_RECEIVE_PACKET;
    while(!mp_manager_ptr->mp_task_running)
    {
        k_sleep(1);
    }
}

void mp_task_rx_stop(void)
{

    if(mp_manager_ptr && mp_manager_ptr->mp_task_running && !mp_manager_ptr->mp_task_end){
        mp_manager_ptr->mp_task_end = 1;
        while(mp_manager_ptr->mp_task_end)
        {
            printk("wait task end\n");
            k_sleep(10);
        }
    }
}

void mp_task_stop_sync(void)
{
   if(mp_manager_ptr && mp_manager_ptr->mp_task_running && !mp_manager_ptr->mp_task_end)
    {
        mp_manager_ptr->mp_task_end = 1;
        while(mp_manager_ptr->mp_task_end)
        {
            k_sleep(1);
        }
    }
}

//Make sure mp thread has been created succussfully after this function is executed.
int mp_init_sync(void)
{
    int ret = 0;
    static mp_private_t mp;
    p_mp = &mp;
    if(mp_thread_status == THREAD_NULL)
    {
        mp_thread_status = THREAD_CREATE;
        printk("MP THREAD CREATE\n");
    }
    else
    {
        printk("mp thread started already!\n");
        return -1;
    }

//    p_mp = z_malloc(sizeof(mp_private_t));
    if (p_mp == NULL)
    {
        printk("%d: mp malloc fail!\n", __LINE__);
        ret = -1;
        return ret;
    }

//    p_mp->high_prio_thread_stack = z_malloc(2048);
    if (p_mp->high_prio_thread_stack == NULL)
    {
        printk("%d: mp thread malloc fail!\n", __LINE__);
        ret = -1;
        return ret;
    }

//    p_mp->high_prio_thread_schedule = z_malloc(512);
    if (p_mp->high_prio_thread_schedule == NULL)
    {
        ret = -1;
        return ret;
    }

    k_thread_create(&p_mp->high_prio_thread_schedule, (void *)p_mp->high_prio_thread_stack,
        2048, mp_bt_control_handle, &p_mp->manager_info, NULL, NULL, 9, 0, 0);

    while(mp_thread_status != THREAD_RUNNING)
    {
        k_sleep(1);
    }
    return ret;
}

void mp_exit(void)
{
    if(p_mp == NULL)
    {
        printk("mp thread already exist\n");
        return;
    }

    if(mp_thread_status == THREAD_RUNNING || mp_thread_status == THREAD_CREATE)
    {
        mp_thread_status = THREAD_REQUEST_EXIT;

        while(mp_thread_status != THREAD_EXIT)
        {
            k_sleep(1);
        }
        mp_thread_status = THREAD_NULL;
    }
    else if(mp_thread_status == THREAD_EXIT)
    {
        mp_thread_status = THREAD_NULL;
    }

    if(p_mp->high_prio_thread_stack != NULL)
    {
//        free(p_mp->high_prio_thread_stack);
    }

    //free resources.
//    free(p_mp);

    p_mp = NULL;
}

