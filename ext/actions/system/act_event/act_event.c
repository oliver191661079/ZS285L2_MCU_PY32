#include "act_event_inner.h"
#include <string.h>

act_event_ctrl_t act_event_ctrl;

struct _k_thread_stack_element __aligned(STACK_ALIGN) act_event_thread_stack[ACT_EVENT_THREAD_STACK_SIZE];

const char act_event_level_info[] =
{
    'N',    //NONE
	'E',	//Error
    'W',    //Warn
    'I',    //Info
    'D',    //Debug
};

unsigned int act_event_count_not_zero_bits(unsigned int val, unsigned int bits)
{
    int cnt = 0;
    int i;

    for (i = 0; i < bits; i++) {
        if ((val & (1 << i)) != 0) {
            cnt++;
        }
    }

    return cnt;
}

int act_event_get_arg_info(event_message_t *msg, uint32_t *arg_num, uint32_t *arg_bytes)
{
	int i;
	uint8_t byte_idx;
	uint8_t arg_bitmap = msg->head.arg_bitmap;

	*arg_bytes = 0;
	for(i = 0; i < MAX_EVENT_ARG_NUM; i++){
		byte_idx = (arg_bitmap & 0x3);
		if(byte_idx){
			arg_bitmap >>= 2;
			if(byte_idx == 1){
				*arg_bytes += 1;
			}else if(byte_idx == 2){
				*arg_bytes += 2;
			}else{
				*arg_bytes += 4;
			}
		}else{
			break;
		}
	}

	*arg_num = i;

	return 0;
}




static void process_event_message(event_message_t *log_msg, uint32_t data_len)
{
    act_event_output_write(log_msg, (uint8_t *)log_msg, data_len);
}


void act_event_data_handler(act_event_ctrl_t *ctrl)
{
    uint32_t irq_flag;
	uint32_t buf_size;
	uint32_t arg_num, arg_bytes;

    event_message_t event_msg;

#ifdef CONFIG_ACT_EVENT_PRINT_DROP_COUNT
    uint32_t drop_cnt;

    irq_flag = irq_lock();
    drop_cnt = ctrl->drop_cnt;
    if (ctrl->drop_cnt) {
        ctrl->drop_cnt = 0;
    }
    irq_unlock(irq_flag);

    if (IS_ENABLED(CONFIG_ACT_EVENT_PRINT_DROP_COUNT)){
        if (drop_cnt){
            printk("\t\t>>>event drop:%u\r\n\n", drop_cnt);
        }
    }
#endif
    while (cbuf_get_used_space(&ctrl->rbuf) >= sizeof(event_message_head_t)) {
		buf_size = cbuf_get_used_space(&ctrl->rbuf);
        if (cbuf_read(&ctrl->rbuf, (uint8_t *)&event_msg, sizeof(event_message_head_t)) != sizeof(event_message_head_t)) {
			printk("read head data err\n");
			continue;
        }

        act_event_get_arg_info(&event_msg, &arg_num, &arg_bytes);

        if (arg_num){
            if (cbuf_read(&ctrl->rbuf, (uint8_t *)&event_msg.arg_value, arg_bytes) != arg_bytes) {
				printk("read data err:%x\n", arg_bytes);
                continue;
            }
        }

#if 0
		printk("output event module %x event %x arg num %d byte %d bitmap %x\n", event_msg.head.module_id, \
			event_msg.head.event_id, arg_num, arg_bytes, event_msg.head.arg_bitmap);
#endif
        process_event_message(&event_msg, arg_bytes + sizeof(event_message_head_t));
    }
}


void act_event_put_data(void *data, uint32_t len)
{
	int irq_flag;
    act_event_ctrl_t *ctrl = &act_event_ctrl;

	irq_flag = irq_lock();

    uint32_t free_space = cbuf_get_free_space(&ctrl->rbuf);

    if (free_space >= len) {
        cbuf_write(&ctrl->rbuf, data, len);
    }else{
        ctrl->drop_cnt++;
    }

    if (k_is_in_isr()){
        ctrl->irq_cnt++;
    }

	irq_unlock(irq_flag);

    os_sem_give(&ctrl->log_sem);

    return;
}

int act_event_data_send(uint32_t pack_data, ...)
{
	act_event_pack_data arg_data;
	event_message_t event_msg;
	va_list valist;
	int i, j;
	uint32_t value;
	uint32_t *pdata = (uint32_t *)&event_msg;

    arg_data.data = pack_data;

#if 0
	printk("evt %d send arg %d module %d level %d\n", arg_data.bit_data.event_id, arg_data.bit_data.arg_num, \
		arg_data.bit_data.module_id, arg_data.bit_data.level);
#endif

	va_start(valist, pack_data);

	if(!act_event_ctrl.enable){
		return -EIO;
	}

	if(arg_data.bit_data.level >= ACT_EVENT_LEVEL_MAX \
		|| arg_data.bit_data.event_id > MAX_ACT_EVENT_ID){
		//printk("invalid0 level %d module %d\n", arg_data.bit_data.level, arg_data.bit_data.event_id);
		return -EINVAL;
	}

    if ((arg_data.bit_data.level > act_event_ctrl.level) \
		|| (arg_data.bit_data.module_id >= act_event_ctrl.event_module_cnt)) {
		//printk("invalid1 level %d module %d\n", arg_data.bit_data.level, arg_data.bit_data.event_id);
        return -EINVAL;
    }

    if (arg_data.bit_data.level > act_event_dynamic_level_get(&act_event_ctrl, arg_data.bit_data.module_id)){
#if 0
		printk("invalid2 level %d module %d\n", arg_data.bit_data.level, \
			act_event_dynamic_level_get(&act_event_ctrl, arg_data.bit_data.module_id));
#endif
        return -EINVAL;
    }

	if (arg_data.bit_data.level == 0){
		return -EIO;
	}

	if(arg_data.bit_data.arg_num > MAX_EVENT_ARG_NUM ){
		arg_data.bit_data.arg_num = MAX_EVENT_ARG_NUM;
	}

	event_msg.head.module_id = arg_data.bit_data.module_id;

	event_msg.head.event_id = arg_data.bit_data.event_id;

	event_msg.head.level = arg_data.bit_data.level - 1;


	j = 0;
	event_msg.head.arg_bitmap = 0;
	for(i = 0; i < arg_data.bit_data.arg_num; i++){
		value = va_arg(valist, uint32_t);
		if(value <= 0xff){
			event_msg.arg_value[j++] = (uint8_t)value;
			event_msg.head.arg_bitmap |= (1 << (i << 1));
		}else if(value <= 0xffff){
			event_msg.arg_value[j++] = (uint8_t)value;
			event_msg.arg_value[j++] = (uint8_t)((value & 0xff00) >> 8);
			event_msg.head.arg_bitmap |= (2 << (i << 1));
		}else{
			event_msg.arg_value[j++] = (uint8_t)value;
			event_msg.arg_value[j++] = (uint8_t)((value & 0xff00) >> 8);
			event_msg.arg_value[j++] = (uint8_t)((value & 0xff0000) >> 16);
			event_msg.arg_value[j++] = (uint8_t)((value & 0xff000000) >> 24);
			event_msg.head.arg_bitmap |= (3 << (i << 1));
		}
	}

	event_msg.head.check_bits = act_event_count_not_zero_bits(*pdata, 27);

	act_event_put_data((void *)&event_msg, j + sizeof(event_message_head_t));

	return 0;

}

static int act_event_module_id_get(const char *name)
{
	uint32_t modules_cnt = act_event_module_num_get();
	const char *tmp_name;
	uint32_t i;

	for (i = 0U; i < modules_cnt; i++) {
		tmp_name = act_event_source_name_get(i);

		if (strncmp(tmp_name, name, 64) == 0) {
			return i;
		}
	}
	return -1;
}

static void act_event_module_level_init(act_event_ctrl_t *ctrl)
{
	uint32_t i;
	uint8_t compile_level;

	uint32_t modules_cnt = act_event_module_num_get();

	uint32_t level_bytes = ROUND_UP(modules_cnt, 2) >> 1;

	ctrl->module_level = (uint8_t *)mem_malloc(level_bytes);

	if(!ctrl->module_level){
		return;
	}

	memset(ctrl->module_level, 0, level_bytes);

	for (i = 0; i < modules_cnt; i++) {
		compile_level = act_event_compiled_level_get(i);
		act_event_dynamic_level_set(ctrl, i, compile_level);
	}

}

static void act_event_task_entry(void *p1, void *p2, void *p3)
{
    act_event_ctrl.task_ready = true;
    while(1){
		os_sem_take(&act_event_ctrl.log_sem, OS_FOREVER);

		if (act_event_ctrl.enable){
			act_event_data_handler(&act_event_ctrl);
		}
    }
}


int act_event_init(act_event_callback_t event_cb)
{
    act_event_ctrl_t *ctrl = (act_event_ctrl_t *)&act_event_ctrl;

    //only init once
    if(ctrl->init_flag){
        return 0;
    }

	os_sem_init(&ctrl->log_sem, 0, UINT_MAX);

	cbuf_init(&ctrl->rbuf, ctrl->cache_buffer, ACT_EVENT_CACHE_BUFFER_SIZE);

	ctrl->event_module_cnt = act_event_module_num_get();

    ctrl->task_ready = false;

    ctrl->level = CONFIG_ACT_EVENT_COMPILE_LEVEL;

	ctrl->enable = true;

    ctrl->thread_id = os_thread_create((char *)act_event_thread_stack,
											ACT_EVENT_THREAD_STACK_SIZE,
											act_event_task_entry,
											NULL, NULL, NULL,
											K_LOWEST_APPLICATION_THREAD_PRIO, 0, OS_NO_WAIT);

    act_event_output_init(event_cb);

	printk("log used size %d\n", act_event_output_get_used_size());

	act_event_module_level_init(ctrl);

	while(!ctrl->task_ready){
		os_sleep(10);
	}

    ctrl->init_flag = true;

#ifdef CONFIG_ACT_EVENT_INIT_DATA_CLEAR
	act_event_data_clear();
#endif

    return 0;
}

typedef int (*bt_api_transfer_cb)(uint8_t *data, uint32_t max_len);
bt_api_transfer_cb g_bt_cb = NULL;

static int act_event_bt_callback(uint8_t *data, uint32_t len)
{
    uint32_t read_len = 0;
    uint32_t str_len;
    while(read_len < len){
        if(*data){
            str_len = act_event_strnlen(data, len - read_len);
            //flow output,do not use printk
            if(g_bt_cb)
            	g_bt_cb(data, str_len);
            read_len += str_len;
            data += str_len;
        }else{
            data++;
            read_len++;
        }
    }
    return 0;
}

int act_event_syslog_transfer(int log_type, int (*traverse_cb)(uint8_t *data, uint32_t max_len))
{
	int traverse_len;
	int file_id;
    /* Print buffer */
    char print_buf[CONFIG_ACT_EVENT_LINEBUF_SIZE];
	if(traverse_cb == NULL)
		return 0;

	if(!act_event_ctrl.enable){
		printk("act_event disabled\n");
		return 0;
	}

	file_id = ACT_EVENT_FILE_ID;

	//flush flash ringbuf data to flash
	act_event_output_flush();

	g_bt_cb = traverse_cb;
    traverse_len = act_event_output_traverse(act_event_bt_callback, print_buf, sizeof(print_buf));
	g_bt_cb = NULL;
    return traverse_len;
}

int act_event_set_level_filter(const char *module_name, uint32_t dynamic_level)
{
    int  id;

	if (dynamic_level >= ACT_EVENT_LEVEL_MAX){
		printk("invalid level %d\n", dynamic_level);
		return -EINVAL;
	}

	printk("set module %s level %d\n", module_name, dynamic_level);

    if (strcmp(module_name, "all") == 0) {

        act_event_ctrl.level = dynamic_level;

    } else {

        id = act_event_module_id_get(module_name);

        /* no match tag name found */
        if (id < 0) {
            return -1;
        }

		act_event_dynamic_level_set(&act_event_ctrl, id, dynamic_level);
    }

    return 0;
}



void act_event_dump_runtime_info(void)
{
	uint32_t i;

    act_event_ctrl_t *ctrl = (act_event_ctrl_t *)&act_event_ctrl;

	printk("module cnt %d level %d\n", ctrl->event_module_cnt, ctrl->level);
	printk("drop cnt %d err cnt %d\n", ctrl->drop_cnt, ctrl->err_cnt);
	printk("rbuf size %d\n", cbuf_get_used_space(&ctrl->rbuf));

	for(i = 0; i < ctrl->event_module_cnt; i++){
		printk("%s level:%c\n", act_event_source_name_get(i), act_event_level_info[act_event_dynamic_level_get(ctrl, i)]);
	}
}

int act_event_runtime_enable(uint8_t enable)
{
	uint32_t irq_flag = irq_lock();

	act_event_ctrl.enable = enable;

	irq_unlock(irq_flag);

	return 0;
}

int act_event_data_clear(void)
{
	if(act_event_output_get_used_size()){
		act_event_output_clear();
	}

	return 0;
}

