/********************************************************************************
 *                            USDK(ATS350B_linux)
 *                            Module: Trace
 *                 Copyright(c) 2021 Actions Semiconductor,
 *                            All Rights Reserved. *

 ********************************************************************************/
/*!
 * \file     trace_test.c
 * \brief    Trace test functions for "trace_impl.c".
 * \author
 * \version  0.1
 * \date  Thu, May 13, 2021  4:05:46 PM
 ******************************************************************************/

#include <init.h>
#include <kernel.h>
#include <cbuf.h>
#include <trace.h>
#include <soc.h>
#include <ext_types.h>
#include <logging/sys_log.h>
#ifdef CONFIG_TRACE_UART
#include <dma.h>
#include <drivers/console/uart_console.h>
#endif

#include <os_common_api.h>
#include <input_dev.h>
#include <string.h>
#include <stdio.h>
#include <acts_ringbuf.h>
#include <uart.h>
#include <logic.h>
#include "trace_test.h"


#ifdef TRACE_TEST

void trace_test_init(void);
#ifdef TRACE_TEST_WITH_CBUF
static void trace_fish_thread(void *p1, void *p2, void *p3);
static void trace_test_create_fishes(void);
#else
struct k_sem sem_trace_test;
#endif

#ifdef LOGIC_ANALYSIS_SERIAL_TEST
static void serial_speed_test(void);
#endif

#ifdef CONFIG_TRACE_PERF_ENABLE
void trace_profile_collect(void)
{
    trace_ctx_t *trace_ctx = get_trace_ctx();

    SYS_LOG_INF("Max print time %dus.", trace_ctx->print_time / 24);
    SYS_LOG_INF("Max convert time %dus.", 1, trace_ctx->convert_time / 24);
    SYS_LOG_INF("Max hex dump time %dus", 1, trace_ctx->hex_time / 24);

    trace_ctx->print_time = 0;
    trace_ctx->hex_time = 0;
    trace_ctx->convert_time = 0;
}
#endif

static int perf_count = 5009;
static int perf_len = 512;
static int perf_lens[4] = { 35, 230, 511, 512 };

#ifdef TRACE_TEST_WITH_CBUF
static unsigned char fish_buf1[512];
#ifdef TRACE_MULTI_TASK
static unsigned char fish_buf2[512];
static unsigned char fish_buf3[512];
static unsigned char *fish_bufs[3] = { fish_buf1, fish_buf2, fish_buf3 };
#else
static unsigned char *fish_bufs[3] = { fish_buf1, NULL, NULL };
#endif
#else
static unsigned char __aligned(4) data_buf[512];
#endif

static bool trace_test_enable = false;
static const char *test_string[] = {
    "0123456789_",
    "123456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789_123456789_",
    "abcdef",
    "DEADBEEF",
    "This is a very long string. It's used to test the trace system. To make it long enough, a stuff is followed. <The Journey to the West is a classic in Chinese literature, believed to be written by Wu Cheng’en, possibly as early as 1625. It tells the story of a Buddhist monk named Tang Sanzang and his search for spiritual enlightenment. Tang Sanzang’s pilgrimage takes him along the Silk Road to India as he searches for translations of Buddhist scripture, and the adventures he has and the characters he meets take him on a spiritual odyssey that resembles a Greek epic. Tang Sanzang meets Sun Wukong, or the Monkey King, who appears to embody human sins. The Monkey King personifies chaos and corruption, perhaps government corruption, which was pervasive in China at the time the novel was written. He also symbolizes Tang Sanzang’s mind, which, during his search for spiritual enlightenment, is itself corrupt and chaotic. The Monkey King battles monsters, which represent obstacles to enlightenment, and through a series of trials, he gains spiritual power. The truth of Buddhism is stressed throughout the book, as the Buddhist characters in the story are the ones with the most power. Monkey gains his powers from Taoism, for example, and the Buddha alone is able to defeat him. The characters gain power from all three religions, however, which conveys the idea that there are different paths to spiritual enlightenment.> Long string ends here.",
    "!@#$",
    "M",
    "2L",
    "xXL",
    "XXXL",
    "0123456789abcdefghijklmnopqrstuvwxyz\r\n",
    "0123456789ABCDEFG\r\n",
};

#define TEST_STRING_NUM 12
#define LONG_STRING_INDEX 4

#define MY_STACK_SIZE 1024
//#define MY_PRIORITY CONFIG_APP_PRIORITY
#define MY_PRIORITY 10

#ifdef TRACE_TEST_WITH_CBUF
static struct k_thread my_thread_data_t1;
#ifdef TRACE_MULTI_TASK
static struct k_thread my_thread_data_t2;
static struct k_thread my_thread_data_t3;
#endif
K_THREAD_STACK_DEFINE(my_stack_area_t1, MY_STACK_SIZE);
#ifdef TRACE_MULTI_TASK
K_THREAD_STACK_DEFINE(my_stack_area_t2, MY_STACK_SIZE);
K_THREAD_STACK_DEFINE(my_stack_area_t3, MY_STACK_SIZE);
#endif
#else
static struct k_thread my_thread_data_trace;
K_THREAD_STACK_DEFINE(my_stack_area_trace, 1024);
#endif
static void trace_test_switch(void)
{
    char buf[64];

    trace_test_enable = !trace_test_enable;

    if (trace_test_enable) {
        sprintf(buf, "Test switch on count=%d, len=%d\r\n", perf_count,
                perf_len);
        uart_print(buf);
    } else {
        sprintf(buf, "Test switch off\r\n");
        uart_print(buf);
    }
}

void trace_test_config(int key)
{
    static int mode = 0;
    char buf[64];

    if (key == 0x4000009) {
        mode++;
        perf_len = perf_lens[mode % 4];
#ifdef TRACE_TEST_WITH_CBUF
        memcpy(fish_buf1, test_string[LONG_STRING_INDEX], sizeof(fish_buf1));
#ifdef TRACE_MULTI_TASK
        memcpy(fish_buf2, test_string[LONG_STRING_INDEX], sizeof(fish_buf2));
        memcpy(fish_buf3, test_string[LONG_STRING_INDEX], sizeof(fish_buf3));
#endif
#else
        memcpy(data_buf, test_string[LONG_STRING_INDEX], sizeof(data_buf));
#endif
        sprintf(buf, "perf_len=%d\r\n", perf_len);
        uart_print(buf);
    }

    if (key == 0x4000003) {     //Vol+
        trace_test_switch();
    }
}

static void trace_report(int id, unsigned int ms, unsigned int size)
{
    char str[64];

    uart_print("Trace summary:\r\n");
    if (ms != 0) {
        sprintf(str, "Fish%d-Done %d in %dms, speed=%dB/ms\r\n", id, size, ms,
                size / ms);
    } else {
        sprintf(str, "Fish%d-Done %d in 0s!\r\n", id, size);
    }
    uart_print(str);
}

#ifdef TRACE_TEST_WITH_CBUF
static void trace_test_create_fishes(void)
{
    //k_tid_t my_tid1;

    memcpy(fish_buf1, test_string[LONG_STRING_INDEX], sizeof(fish_buf1));
#ifdef TRACE_MULTI_TASK
    memcpy(fish_buf2, test_string[LONG_STRING_INDEX], sizeof(fish_buf2));
    memcpy(fish_buf3, test_string[LONG_STRING_INDEX], sizeof(fish_buf3));
#endif

    //A higer priority trace thread
    k_thread_create(&my_thread_data_t1, my_stack_area_t1,
                    K_THREAD_STACK_SIZEOF(my_stack_area_t1), trace_fish_thread,
                    (void *)0, NULL, NULL, MY_PRIORITY - 1, 0, 30);
#ifdef TRACE_MULTI_TASK
    k_thread_create(&my_thread_data_t2, my_stack_area_t2,
                    K_THREAD_STACK_SIZEOF(my_stack_area_t2), trace_fish_thread,
                    (void *)1, NULL, NULL, MY_PRIORITY, 0, 42);
    //A lower priority trace thread
    k_thread_create(&my_thread_data_t3, my_stack_area_t3,
                    K_THREAD_STACK_SIZEOF(my_stack_area_t3), trace_fish_thread,
                    (void *)2, NULL, NULL, MY_PRIORITY + 1, 0, 53);
#endif
}

static void trace_fish_thread(void *p1, void *p2, void *p3)
{
    int id;
    int count = 0;
    char str[64];
    u32_t ms;
    s64_t time_stamp = 0;
    unsigned char *buf;

    id = (int)p1;
    buf = fish_bufs[id];

    while (1) {
        if (trace_test_enable) {
            if (count == 0) {
                sprintf(str, "trace fish%d start: len=%d, count=%d\r\n", id,
                        perf_len, perf_count);
                uart_print(str);
                time_stamp = k_uptime_get_32();
                count++;
            } else if (count == perf_count + 1) {
                ms = k_uptime_delta_32(&time_stamp);
                sprintf(str, "trace fish%d stop auto: count=%d\r\n", id,
                        count - 1);
                uart_print(str);
                trace_report(id, ms, perf_len * (count - 1));
                count++;
            } else if (count > perf_count + 1) {
                k_sleep(1000);
            } else {

                str[0] = '\r';
                str[1] = '\n';
                num_to_str(id, 2, str + 2);
                str[4] = '-';
                num_to_str(count, 6, str + 5);
                str[11] = '-';
                str[12] = '-';
                str[13] = '\r';
                str[14] = '\n';
                str[15] = 0;
#ifdef TRACE_DEBUG_VERBOSE
                uart_print(str);
#endif
                memcpy(buf, str, 13);
                memcpy(buf + perf_len - 11, str + 2, 11);

                trace_output_data(buf, perf_len);
                count++;
#ifndef TRACE_STUFF_CBUF
                //Sleep to slow down trace.
                k_sleep(1);
#endif
            }

        } else {
            if (count != 0) {
                if (count < perf_count + 1) {
                    ms = k_uptime_delta_32(&time_stamp);
                    sprintf(str, "trace fish%d stop manually: count=%d\r\n", id,
                            count - 1);
                    uart_print(str);
                    trace_report(id, ms, perf_len * (count - 1));
                }
                count = 0;
            }
            k_sleep(1000);
        }
    }
}

#else
static void trace_test_no_cbuf_thread(void *p1, void *p2, void *p3)
{
    u32_t ms, count = 0;
    static s64_t time_stamp = 0;
    char str[32];
    int ret;

    while (1) {
        if (trace_test_enable) {
            if (count == 0) {
                //to print start information
                ret = k_sem_take(&sem_trace_test, 100);
                if (ret != 0) {
                    continue;
                }
                k_sem_give(&sem_trace_test);
                sprintf(str, "Trace nocbuf start: %d*%d\r\n", perf_len,
                        perf_count);
                uart_print(str);
                time_stamp = k_uptime_get_32();
                count++;
            } else if (count == perf_count + 1) {
                ms = k_uptime_delta_32(&time_stamp);

                uart_print("Trace nocbuf end auto.\r\n");
                ret = k_sem_take(&sem_trace_test, 1);
                if (ret != 0) {
                    continue;
                }
                k_sem_give(&sem_trace_test);
                trace_report(0, ms, perf_len * (count - 1));

                //reset control variables
                count = 0;
                trace_test_enable = false;
            } else {
                //sprintf(str, "\r\n%06d--", count);
                str[0] = '\r';
                str[1] = '\n';
                num_to_str(count, 6, str + 2);
                str[8] = '-';
                str[9] = '-';
                str[10] = 0;

#ifdef TRACE_DEBUG_VERBOSE
                uart_print(str);
#endif
                ret = k_sem_take(&sem_trace_test, 1);
                if (ret != 0) {
                    continue;
                }
                memcpy(data_buf, str, 10);
                memcpy(data_buf + perf_len - 8, str + 2, 8);
                hal_trace_output(data_buf, perf_len);
                count++;
            }

        } else {
            k_sleep(1000);
        }
    }
}

#endif

void trace_test_init(void)
{
#ifdef TRACE_TEST_WITH_CBUF
    trace_test_create_fishes();
#else
    k_sem_init(&sem_trace_test, 1, 1);
    k_thread_create(&my_thread_data_trace, my_stack_area_trace,
                    K_THREAD_STACK_SIZEOF(my_stack_area_trace),
                    trace_test_no_cbuf_thread, (void *)1, NULL, NULL,
                    MY_PRIORITY, 0, 0);
#endif
}


#ifdef LOGIC_ANALYSIS_SERIAL_TEST
static struct k_thread output_test_thread;
K_THREAD_STACK_DEFINE(stack_output_test, 1024);

void output_test(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);
	unsigned char buf[2]={'a', 0x0};

	while (1) {
		uart_print_data(buf, 1);
#ifdef LOGIC_ANALYZER_TRACE
		static unsigned char status=0;
		status = !status;
		if(status){
			logic_set_1(1);
		}else{
			logic_set_0(1);
		}
#endif
		k_sleep(1000);
	}
}

static void serial_speed_test(void)
{
	k_thread_create(&output_test_thread, (k_thread_stack_t)stack_output_test, K_THREAD_STACK_SIZEOF(stack_output_test), output_test, NULL, NULL, NULL, -9, 0, 3000);
}
#endif

#endif

#ifdef TRACE_HEX_TEST
extern void trace_hexdump(uint8_t* buf, uint32_t len);

static unsigned char random_data[] = {
  0x4c, 0x51, 0xa0, 0x46, 0x12, 0xe6, 0xac, 0x5b, 0x5a, 0x22, 0x6e, 0x69,
  0x66, 0x31, 0xe4, 0xb7, 0xcd, 0xea, 0x74, 0x10, 0x3b, 0xc6, 0xb6, 0x5c,
  0xba, 0x70, 0x39, 0x39, 0x47, 0xef, 0x3f, 0x50, 0xca, 0x9f, 0x90, 0x27,
  0xb1, 0x7b, 0xd6, 0x9e, 0xc9, 0x72, 0x2e, 0x66, 0x0f, 0xba, 0xd3, 0x1d,
  0x93, 0x71, 0x19, 0xd7, 0xb5, 0xf7, 0x1e, 0x47, 0x43, 0xbe, 0xdc, 0xd0,
  0x4f, 0x96, 0x91, 0x23, 0xbc, 0xaa, 0x96, 0x4b, 0xd4, 0xad, 0x2a, 0x38,
  0x7d, 0xea, 0xcc, 0x28, 0xbf, 0x41, 0x0d, 0x97, 0xc7, 0x48, 0x1b, 0x79,
  0x9c, 0x03, 0x55, 0xf5, 0x15, 0xf9, 0xac, 0xe5, 0xea, 0xa5, 0xb1, 0xce,
  0xdb, 0x71, 0x7e, 0x14, 0x44, 0xc1, 0xe0, 0x9f, 0xa3, 0xcf, 0x82, 0x32,
  0xe1, 0x45, 0xe8, 0x2d, 0x6d, 0xde, 0x82, 0x7b, 0x62, 0x80, 0x2d, 0xdc,
  0x67, 0x9a, 0xdc, 0xdd, 0xed, 0xae, 0x50, 0x19, 0xf4, 0x51, 0x0a, 0xa8,
  0x9b, 0x1e, 0xe4, 0xcd, 0xfd, 0x4a, 0xfe, 0x4a, 0x37, 0xe7, 0xeb, 0xe9,
  0x9a, 0x03, 0x49, 0x23, 0x4d, 0x29, 0x2c, 0xf9, 0x06, 0xfc, 0x15, 0x60,
  0x83, 0xe3, 0x4f, 0xd2, 0xe8, 0x11, 0xef, 0xef, 0x48, 0x53, 0xd5, 0x41,
  0xe1, 0x69, 0x63, 0x7b, 0xd5, 0xed, 0xd4, 0xda, 0x79, 0xef, 0x5e, 0x7c,
  0x76, 0xde, 0x8b, 0xcc, 0x52, 0x5d, 0x72, 0x5a, 0x2a, 0x1c, 0xc5, 0xc8,
  0xab, 0xd8, 0xd6, 0x97, 0x90, 0x0f, 0x06, 0xfb, 0x39, 0x1d, 0x45, 0x13,
  0x23, 0xc0, 0x92, 0x13, 0x4b, 0x3c, 0xa6, 0x57, 0xcb, 0x9f, 0x39, 0x58,
  0x3e, 0xe1, 0x85, 0xfc, 0xb6, 0xd5, 0xfd, 0xd2, 0x47, 0x32, 0x5c, 0x12,
  0x89, 0x28, 0x80, 0x83, 0x83, 0x3b, 0xb1, 0xa4, 0xdd, 0x6f, 0x4b, 0xac,
  0x60, 0xe8, 0xf7, 0x47, 0x0b, 0x95, 0xd6, 0x26, 0x52, 0x8f, 0x50, 0xbb,
  0x4b, 0x2d, 0x74, 0xc8
};
unsigned int random_data_len = 256;

int print_test_config(int key)
{
    static int mode = 0;
    int ret=0;
	static unsigned int len = 0;

    if (key == (0x4000000|KEY_PREVIOUSSONG)) {
        mode++;
		len += 7;
        if (len > random_data_len) {
            len = 0;
        }
        trace_hexdump(random_data, len);
        ret = 1;
    }

    if (key == (0x4000000|KEY_VOLUMEUP)) {
		len += 16;
        if (len > random_data_len) {
            len = 0;
        }
        trace_hexdump(random_data, len);
        ret = 1;
    }

    return ret;
}
#endif