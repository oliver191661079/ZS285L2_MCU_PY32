#ifndef __ACT_EVENT_INNER_H
#define __ACT_EVENT_INNER_H

#include <os_common_api.h>
#include <mem_manager.h>
#include <toolchain.h>
#include <partition.h>
#include <cbuf.h>
#include <flash.h>
#include <linker/linker-defs.h>

#include "act_event.h"
#include <logging/log_core.h>

#define ACT_EVENT_THREAD_STACK_SIZE  (1024)
#define ACT_EVENT_CACHE_BUFFER_SIZE  (512)

typedef struct
{
    uint8_t init_flag :1;
    uint8_t task_ready :1;
    uint8_t level :3;
	uint8_t event_module_cnt;
	uint8_t enable : 1;
    uint32_t thread_id;
    uint32_t drop_cnt;
    uint32_t irq_cnt;
    uint32_t err_cnt;
    cbuf_t rbuf;
    uint8_t cache_buffer[ACT_EVENT_CACHE_BUFFER_SIZE];
    os_sem log_sem;
    uint8_t *module_level;
    //act_log_num_filter_t filter;
} act_event_ctrl_t;

typedef struct {
	uint8_t event_id;
	uint8_t module_id;
	uint8_t arg_bitmap; //0:none 1:byte 2:half word 3:word
	uint8_t level : 3;
	uint8_t check_bits : 5;
}__packed event_message_head_t;

typedef struct
{
	event_message_head_t head;
	uint8_t arg_value[MAX_EVENT_ARG_NUM * 4];
}__packed event_message_t;


static inline const char *act_event_source_name_get(uint8_t src_id)
{
    return src_id < log_sources_count() ? log_name_get(src_id) : NULL;
}

static inline int act_event_module_num_get(void)
{
    return log_sources_count();
}

static inline uint8_t act_event_compiled_level_get(uint8_t src_id)
{
    return log_compiled_level_get(src_id);
}

static inline void act_event_dynamic_level_set(act_event_ctrl_t * ctrl, uint8_t src_id, uint8_t level)
{
    uint8_t level_map = ctrl->module_level[src_id >> 1];

    if (src_id & 0x1) {
        level_map = (level_map & 0xf) | (level << 4);
    } else {
        level_map = (level_map & 0xf0) | level;
    }

    ctrl->module_level[src_id >> 1] = level_map;
}

static inline int act_event_dynamic_level_get(act_event_ctrl_t * ctrl, uint8_t src_id)
{
    uint8_t level_map = ctrl->module_level[src_id >> 1];

    if (src_id & 0x1) {
        return (level_map >> 4);
    } else {
        return (level_map & 0xf);
    }
}

extern act_event_ctrl_t act_event_ctrl;

int act_event_output_init(act_event_callback_t event_cb);

int act_event_output_get_used_size(void);

int act_event_output_flush(void);

int act_event_output_write(event_message_t *log_msg, uint8_t *data, uint32_t len);

int act_event_output_traverse(int (*traverse_cb)(uint8_t *data, uint32_t max_len), uint8_t *buf,
        uint32_t len);

int act_event_output_clear(void);

size_t act_event_strnlen(const char *s, size_t maxlen);

unsigned int act_event_count_not_zero_bits(unsigned int val, unsigned int bits);

int act_event_get_arg_info(event_message_t *msg, uint32_t *arg_num, uint32_t *arg_bytes);

int act_event_set_level_filter(const char *module_name, uint32_t dynamic_level);

void act_event_dump_runtime_info(void);

#endif
