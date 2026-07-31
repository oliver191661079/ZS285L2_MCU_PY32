#ifndef __ACT_EVENT_H
#define __ACT_EVENT_H

#include <logging/log_core.h>

#define MAX_EVENT_ARG_NUM   (4)

#define  ACT_EVENT_LEVEL_NONE    0
#define  ACT_EVENT_LEVEL_ERROR   1
#define  ACT_EVENT_LEVEL_WARN    2
#define  ACT_EVENT_LEVEL_INFO    3
#define  ACT_EVENT_LEVEL_DEBUG   4
#define  ACT_EVENT_LEVEL_MAX     5

#define  ACT_EVENT_FILE_ID     	 2

#define  MAX_ACT_EVENT_ID        (0xff)

#define INVALID_NANO_ARG_NUM    (15)

#define FUN_ARG_EVT_EX(_0, _1, _2, _3, _4, _5, _6, N, ...) N
#define FUN_ARG_EVT_EX_()  6, 5, 4, 3, 2, 1, 0
#define EVT_ARGS_CHECK(N) (true ? N : 0)
#define FUN_ARG_EVT_NUM_(...) FUN_ARG_EVT_EX(__VA_ARGS__)
#define FUN_ARG_EVT_NUM(...) EVT_ARGS_CHECK(FUN_ARG_EVT_EX(0, ##__VA_ARGS__, 6, 5, 4, 3, 2, 1, 0))
#define FUN_ARG_EVT_NUM_INTEGER(a) (((a) >= 0 && (a) <= MAX_EVENT_ARG_NUM) ? (a) : INVALID_NANO_ARG_NUM)

#define ACTEVENT_PACK(module_id, level, event_id, nums) \
    ((((event_id) & 0xffff) << 16) \
    | (((module_id) & 0xff) << 8) \
    | ((level & 0x7) << 4) \
    | (nums & 0xf))


typedef union{
    uint32_t data;
    struct{
        uint8_t arg_num: 4;
        uint8_t level: 3;
        uint8_t reserved : 1;
        uint8_t  module_id;
        uint16_t event_id;
    }bit_data;
}act_event_pack_data;

typedef enum
{
	ACT_EVENT_FLASH_CLEAN_START,
	ACT_EVENT_FLASH_CLEAN_END,
	ACT_EVENT_FLASH_FULL,
}act_event_callback_event_e;

typedef void (*act_event_callback_t)(uint8_t event);

extern int act_event_init(act_event_callback_t event_cb);

extern void act_event_test(void);

extern int act_event_data_send(uint32_t pack_data, ...);

extern int act_event_runtime_enable(uint8_t enable);

extern int act_event_set_level_filter(const char *module_name, uint32_t dynamic_level);

extern int act_event_data_clear(void);

extern int act_event_syslog_transfer(int log_type, int (*traverse_cb)(uint8_t *data, uint32_t max_len));

#define ACT_EVENT_SEND2(pack_data, ...) act_event_data_send(pack_data, ##__VA_ARGS__)

#define ACT_EVENT_PACK_ARGS_DATA(id, level, _event, ...) ACTEVENT_PACK(id, level, _event, (FUN_ARG_EVT_NUM_INTEGER(FUN_ARG_EVT_NUM(__VA_ARGS__))))
#define ACT_EVENT_PACK_ZERO_DATA(id, level, _event) ACTEVENT_PACK(id, level, _event, 0)


#define ACT_EVENT_ID_GET()  LOG_CONST_ID_GET(__log_current_const_data)

#ifdef CONFIG_ACT_EVENT_COMPILE_MODUL_LEVEL
#define ACT_EVENT_CONST_LEVEL_CHECK(_level)	((_level <= __log_level) &&	(_level <= CONFIG_ACT_EVENT_COMPILE_LEVEL))
#else
#define ACT_EVENT_CONST_LEVEL_CHECK(_level)	((_level <= CONFIG_ACT_EVENT_COMPILE_LEVEL))

#endif

#if 0
#define ACT_EVENT_SEND(_level, _event, ...) do { \
			if (!ACT_EVENT_CONST_LEVEL_CHECK(_level)) { \
				break; \
			} \
			printk("get arg num %d %d\n", FUN_ARG_EVT_NUM(__VA_ARGS__), FUN_ARG_EVT_NUM(__VA_ARGS__));\
			ACT_EVENT_SEND2(ACT_EVENT_PACK_NANO_DATA(ACT_EVENT_ID_GET(), _level, _event, ##__VA_ARGS__), ##__VA_ARGS__); \
		} while (false)
#endif

#define ACT_EVENT_SEND(_level, _event, ...) do { \
			if (!ACT_EVENT_CONST_LEVEL_CHECK(_level)) { \
				break; \
			} \
			if (NUM_VA_ARGS_LESS_1(_, ##__VA_ARGS__)){\
				ACT_EVENT_SEND2(ACT_EVENT_PACK_ARGS_DATA(ACT_EVENT_ID_GET(), _level, _event, ##__VA_ARGS__), ##__VA_ARGS__); \
			}else{\
				ACT_EVENT_SEND2(ACT_EVENT_PACK_ZERO_DATA(ACT_EVENT_ID_GET(), _level, _event), ##__VA_ARGS__); \
			}\
		} while (false)

#endif
