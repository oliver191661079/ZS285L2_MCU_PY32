#include "act_event_inner.h"
#include "flash_buffer.h"
#include <debug/ramdump.h>

#ifdef CONFIG_ACT_EVENT_OUTPUT_USER
static act_event_backend_callback_t user_backend_cb;
static void *user_backend_data;
#endif


#define CACHE_BUFFER_SIZE (CONFIG_ACT_EVENT_FLASH_CACHE_SIZE)

struct flash_log_ctx
{
    struct flash_buffer_ctx *event_flash_buffer;
    cbuf_t rbuf;
    uint8_t cache_buffer[CACHE_BUFFER_SIZE];
};

static struct flash_log_ctx flash_log;

extern const char act_event_level_info[];

static struct flash_buffer_ctx *flash_buffer_get(struct flash_log_ctx *ctx, int32_t file_id)
{
    if (file_id == ACT_EVENT_FILE_ID) {
        return ctx->event_flash_buffer;
    }

    return NULL;
}

int act_event_output_init(act_event_callback_t event_cb)
{
    struct flash_log_ctx *ctx = &flash_log;

    cbuf_init(&ctx->rbuf, ctx->cache_buffer, CACHE_BUFFER_SIZE);

#ifdef CONFIG_DEBUG_RAMDUMP
	if(ramdump_check() == 0){
		ctx->event_flash_buffer = NULL;
		act_event_ctrl.enable = false;
		return -EBUSY;
	}
#endif

#ifdef CONFIG_ACT_EVENT_DATA_FULL_MERGE
    ctx->event_flash_buffer = flash_buffer_init(PARTITION_FILE_ID_EVTBUF, true, true, &ctx->rbuf, event_cb);
#else
    ctx->event_flash_buffer = flash_buffer_init(PARTITION_FILE_ID_EVTBUF, false, true, &ctx->rbuf, event_cb);
#endif

    if (!ctx->event_flash_buffer) {
        return -EIO;
    }

    return 0;
}

static int check_event_head_valid(event_message_t *msg)
{
	uint32_t *pdata = (uint32_t *)msg;

    if(msg->head.level >= ACT_EVENT_LEVEL_MAX) {
		//printk("head level err %x\n", msg->head.level);
        return false;
    }

    if(msg->head.module_id >= act_event_ctrl.event_module_cnt) {
		//printk("head module err %x %x\n", msg->head.module_id, act_event_ctrl.event_module_cnt);
        return false;
    }

    if(msg->head.check_bits != act_event_count_not_zero_bits(*pdata, 27)) {
		//printk("head checksum err %x %x\n", msg->head.check_bits, act_event_count_not_zero_bits(*pdata, 27));
        return false;
    }

    return true;
}

int act_event_get_arg_value(event_message_t *msg, uint32_t *arg_num, uint32_t *arg_array)
{
	int i, j;
	uint8_t byte_idx;
	uint8_t arg_bitmap = msg->head.arg_bitmap;

	j = 0;
	for(i = 0; i < MAX_EVENT_ARG_NUM; i++){
		byte_idx = (arg_bitmap & 0x3);
		if(byte_idx){
			arg_bitmap >>= 2;
			if(byte_idx == 1){
				*arg_array = msg->arg_value[j++];
				arg_array++;
			}else if(byte_idx == 2){
				*arg_array = msg->arg_value[j] | (msg->arg_value[j+1] << 8);
				j += 2;
				arg_array++;
			}else{
				*arg_array = msg->arg_value[j] | (msg->arg_value[j+1] << 8) \
					| (msg->arg_value[j+2] << 16) | (msg->arg_value[j+3] << 24);
				arg_array++;
				j += 4;
			}
		}else{
			break;
		}
	}

	*arg_num = i;


	return 0;
}

uint32_t act_event_process_linebuf(event_message_t *log_msg, char *log_buffer, uint32_t buffer_size)
{
    uint32_t curr_index;

	uint32_t arg_num;

	uint32_t arg_bytes[MAX_EVENT_ARG_NUM];

    act_event_get_arg_value(log_msg, &arg_num, arg_bytes);

    curr_index = 0;

	if(arg_num == 0){
		curr_index += snprintk(log_buffer, buffer_size,
								"[%s][%c] evt: 0x%x\n",
								act_event_source_name_get(log_msg->head.module_id),
								act_event_level_info[log_msg->head.level + 1],
								log_msg->head.event_id);

	}else if(arg_num == 1){
		curr_index += snprintk(log_buffer, buffer_size,
								"[%s][%c] evt: 0x%x arg:0x%x\n",
								act_event_source_name_get(log_msg->head.module_id),
								act_event_level_info[log_msg->head.level + 1],
								log_msg->head.event_id, arg_bytes[0]);

	}else if(arg_num == 2){
		curr_index += snprintk(log_buffer, buffer_size,
								"[%s][%c] evt: 0x%x arg:0x%x 0x%x\n",
								act_event_source_name_get(log_msg->head.module_id),
								act_event_level_info[log_msg->head.level + 1],
								log_msg->head.event_id, arg_bytes[0], arg_bytes[1]);

	}else if(arg_num == 3){
		curr_index += snprintk(log_buffer, buffer_size,
								"[%s][%c] evt: 0x%x arg:0x%x 0x%x 0x%x\n",
								act_event_source_name_get(log_msg->head.module_id),
								act_event_level_info[log_msg->head.level + 1],
								log_msg->head.event_id, arg_bytes[0], arg_bytes[1], arg_bytes[2]);

	}else{
		curr_index += snprintk(log_buffer, buffer_size,
								"[%s][%c] evt: 0x%x arg:0x%x 0x%x 0x%x 0x%x\n",
								act_event_source_name_get(log_msg->head.module_id),
								act_event_level_info[log_msg->head.level + 1],
								log_msg->head.event_id, arg_bytes[0], arg_bytes[1], arg_bytes[2], arg_bytes[3]);

	}

    return curr_index;
}


int act_event_output_data_traverse(struct flash_buffer_ctx *buffer_ctx, int (*traverse_cb)(uint8_t *data, uint32_t max_len), uint8_t *buf, uint32_t len)
{
    uint32_t read_len;
    uint32_t read_addr;
    uint32_t total_len;
	uint32_t arg_num;
	uint32_t arg_bytes;

    event_message_t log_msg;
    int line_size;

    read_addr = 0;
    total_len = 0;
    while(1) {
        read_len = flash_buffer_read(buffer_ctx, read_addr, (uint32_t *)&log_msg, sizeof(event_message_head_t));

        if(read_len == 0) {
			//printk("flash end %d size %d\n", read_addr, flash_buffer_get_data_size(buffer_ctx));
            break;
        }

        if(!check_event_head_valid(&log_msg)) {
            //printk("check head err addr:0x%x\n", read_addr);
            //print_buffer((const uint8_t *)&log_msg, 1, sizeof(event_message_head_t), 16, -1);
            read_addr += 1;
            continue;
        }

        //print_buffer((const uint8_t *)&item, 1, sizeof(flash_log_head_t), 16, -1);

        read_addr += read_len;

        //printk("item %d %d %d\n", item.head.arg_num, item.head.func_name_enable, item.head.timestamp_enable);

        act_event_get_arg_info(&log_msg, &arg_num, &arg_bytes);

        if(arg_bytes) {
            read_len = flash_buffer_read(buffer_ctx, read_addr, (uint32_t *)&log_msg.arg_value, arg_bytes);

            //print_buffer((const uint8_t *)&item.data, 1, read_len, 16, -1);

            if(read_len == 0) {
                printk("flash end %d size %d\n", read_addr, flash_buffer_get_data_size(buffer_ctx));
                break;
            }
        }else{
			read_len = 0;
		}

        read_addr += read_len;

        //printk("traverse addr:%x\n", read_addr);

        line_size = act_event_process_linebuf(&log_msg, buf, len);

        //printk("fmt:%s num %d value %d %x len %d\n", log_msg.nano.fmt, log_msg.nano.arg_num, log_msg.nano.arg_value[0], log_msg.nano.arg_value[1], line_size);

        if(traverse_cb) {
            traverse_cb(buf, line_size);
        }

        total_len += line_size;
    }

    return total_len;
}




int act_event_output_get_used_size(void)
{
    struct flash_log_ctx *ctx = &flash_log;

    struct flash_buffer_ctx *buffer_ctx = flash_buffer_get(ctx, ACT_EVENT_FILE_ID);

	if(!act_event_ctrl.enable){
		printk("act_event disabled\n");
		return 0;
	}

    if (buffer_ctx) {
        return flash_buffer_get_data_size(buffer_ctx);
    } else {
        return 0;
    }
}

int act_event_output_traverse(int (*traverse_cb)(uint8_t *data, uint32_t max_len), uint8_t *buf, uint32_t len)
{
    int traverse_len = 0;

    struct flash_log_ctx *ctx = &flash_log;

    struct flash_buffer_ctx *buffer_ctx = flash_buffer_get(ctx, ACT_EVENT_FILE_ID);

    if (!buf || !len || !buffer_ctx || ((uint32_t) buf % 4) || (len % 4)) {
        printk("traverse %p %x %p\n", buf, len, buffer_ctx);
        return 0;
    }

	if(!act_event_ctrl.enable){
		printk("act_event disabled\n");
		return 0;
	}

    traverse_len = act_event_output_data_traverse(buffer_ctx, traverse_cb, buf, len);

    return traverse_len;
}

int act_event_output_flush(void)
{
	uint32_t i;
    struct flash_log_ctx *ctx = &flash_log;
    struct flash_buffer_ctx *buffer_ctx = flash_buffer_get(ctx, ACT_EVENT_FILE_ID);

	if(!act_event_ctrl.enable){
		printk("act_event disabled\n");
		return 0;
	}

	if(buffer_ctx){
		flash_buffer_flush_enable(buffer_ctx, true);

		//wait act event thread handle act event finished
		for(i = 0; i < 10; i++){
			os_sleep(10);
		}

		flash_buffer_sync(buffer_ctx, NULL);

	    return 0;
	}else{
		return -EINVAL;
	}
}

int act_event_output_clear(void)
{
    struct flash_log_ctx *ctx = &flash_log;
    struct flash_buffer_ctx *buffer_ctx = flash_buffer_get(ctx, ACT_EVENT_FILE_ID);

	if(!act_event_ctrl.enable){
		printk("act_event disabled\n");
		return 0;
	}

    if (buffer_ctx) {
        flash_buffer_clear(buffer_ctx);

        return 0;
    } else {
        return -EINVAL;
    }
}

size_t act_event_strnlen(const char *s, size_t maxlen)
{
	size_t n = 0;

	while (*s != '\0' && n < maxlen) {
		s++;
		n++;
	}

	return n;
}



static uint32_t act_event_output_data_write(event_message_t *log_msg, uint8_t *data, uint32_t len)
{
    struct flash_log_ctx *ctx = &flash_log;

    struct flash_buffer_ctx *buffer_ctx = flash_buffer_get(ctx, ACT_EVENT_FILE_ID);

    uint32_t free_space = cbuf_get_free_space(&ctx->rbuf);

	//printk("ctx %p len %d\n", buffer_ctx, len);

	if (!buffer_ctx){
		return 0;
	}

    if (len > free_space) {
	    flash_buffer_sync(buffer_ctx, NULL);
    }

    cbuf_write(&ctx->rbuf, data, len);

	//printk("event write len %d size %d\n", len, cbuf_get_used_space(&ctx->rbuf));

    return len;
}


int act_event_output_write(event_message_t *log_msg, uint8_t *data, uint32_t len)
{
	if(!act_event_ctrl.enable){
		printk("act_event disabled\n");
		return 0;
	}

    return act_event_output_data_write(log_msg, data, len);
}


