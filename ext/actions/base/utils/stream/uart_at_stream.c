/*******************************************************************************
 *                                      US283C
 *                            Module: usp Driver
 *                 Copyright(c) 2003-2017 Actions Semiconductor,
 *                            All Rights Reserved.
 *
 * History:
 *      <author>    <time>           <version >             <desc>
 *       wuyufan    2023-4-27  11:39     1.0             build this file
*******************************************************************************/
/*!
 * \file     at_parser.c
 * \brief    at parser
 * \author   wuyufan
 * \par      GENERAL DESCRIPTION:
 *               function related to at parser
 * \par      EXTERNALIZED FUNCTIONS:
 *
 * \version 1.0
 * \date  2023-4-27
*******************************************************************************/
#include <device.h>
#include <stream.h>
#include <logging/sys_log.h>
#include <uart.h>
#include <acts_ringbuf.h>
//#include <malloc.h>
#include <soc.h>
#include <string.h>
#include <ringbuff_stream.h>
#include "uart_at_stream.h"
#include <misc/byteorder.h>

#define RAW_BUFFER_SIZE                 256
#define FRAME_BUFFER_SIZE               256
#define MAX_PASSTHROUGH_DATA_SIZE       (255)

#define AT_PASS_THROUGH_FLAG           (0xcc)
#define AT_OTA_DATA_FLAG               (0xdd)

#define AT_STREAM_NO_FRAME   -1
#define AT_STREAM_END_FRAME  -2
#define AT_STREAM_ERR_FRAME  -3

#define UART_AT_STREAM_DEV_NAME          CONFIG_UART_ACTS_PORT_1_NAME
#define UART_AT_STREAM_BAUD              CONFIG_UART_ACTS_PORT_1_BAUD_RATE

#define AT_STREAM_RX_TREHAD_STACK_SIZE 		2048

struct uart_at_stream_obj{
	os_sem *read_sem;
	struct acts_ringbuf *cbuf;
	uint32_t drop_cnt;
	void *priv_data;
	void (*read_cb)(void *priv_data);
};

struct at_stream_ctx{
	struct device *uart_dev;
    u8_t *raw_buf;
    u8_t opened;
	u8_t init;
	uint8_t end_char_mode;
	uint8_t quotes_flag;
	at_stream_state_e state;

	uint8_t *frame_buf;
	uint16_t frame_buf_index;


	uint8_t passthrough_data_length;

	struct uart_at_stream_obj at_obj;
	struct uart_at_stream_obj data_obj;
	struct uart_at_stream_obj ota_obj;
	struct k_thread rx_thread;
	k_thread_stack_t rx_stack;

	struct k_poll_event evt[3];
};


static struct at_stream_ctx  g_at_stream_ctx;

OS_MUTEX_DEFINE(at_stream_mutex);

struct at_stream_ctx *at_stream_get_ctx(void)
{
	return &g_at_stream_ctx;
}

static void at_stream_parse_state_reset(struct at_stream_ctx *ctx)
{
	ctx->frame_buf_index = 0;
	ctx->quotes_flag = false;
	ctx->state = AT_STREAM_AT_COMMAND;
}

static int16_t at_stream_parse_at_command_byte(uint8_t byte, struct at_stream_ctx *ctx)
{
	int frame_end = false;
	uint8_t *data_ptr;

	if(!ctx->at_obj.cbuf){
		return 0;
	}

	//判断是否是控制字符
	if(byte == AT_PARSER_BS_C || byte == AT_PARSER_DEL_C){
		if(ctx->frame_buf_index > 0){
			ctx->frame_buf_index--;
			return AT_STREAM_NO_FRAME;
		}
	}

	//判断是否出现"字符，“字符内部的结束字符会被忽略
	if(!ctx->quotes_flag){
		switch(ctx->end_char_mode){
			case AT_CHAR_MODE_WITH_NULL:
				if(byte == '\0'){
					frame_end = true;
				}
				break;

			case AT_CHAR_MODE_WITH_CR:
				if(byte == '\r'){
					frame_end = true;
				}
				break;

			case AT_CHAR_MODE_WITH_LF:
				if(byte == '\n'){
					frame_end = true;
				}
				break;

			case AT_CHAR_MODE_WITH_CRLF:
				if(byte == '\n'){
					if(ctx->frame_buf_index > 0 && \
						ctx->frame_buf[ctx->frame_buf_index - 1] == '\r'){
						frame_end = true;
					}
				}
				break;
		}
	}

	if(frame_end){

		ctx->frame_buf[ctx->frame_buf_index] = '\0';
		ctx->frame_buf_index++;

		data_ptr = ctx->frame_buf;
		while(data_ptr != &ctx->frame_buf[ctx->frame_buf_index]){
			if (*data_ptr > AT_PARSER_SPACE) {
				break;
			}else if(*data_ptr == '\0'){
				at_stream_parse_state_reset(ctx);
				return 0;
			}
			data_ptr++;
		}

		if(acts_ringbuf_space(ctx->at_obj.cbuf) >= ctx->frame_buf_index){
			acts_ringbuf_put(ctx->at_obj.cbuf, ctx->frame_buf, ctx->frame_buf_index);
		}else{
			ctx->at_obj.drop_cnt++;
		}

		at_stream_parse_state_reset(ctx);
		os_sem_give(ctx->at_obj.read_sem);

		return 0;
	}

	//接下来是正常的可打印字符，进行缓存
	if(ctx->frame_buf_index > (FRAME_BUFFER_SIZE - 2)){
		printk("parse buf overflow\n");
		at_stream_parse_state_reset(ctx);
		return AT_STREAM_ERR_FRAME;
	}

	ctx->frame_buf[ctx->frame_buf_index] = byte;
	ctx->frame_buf_index++;

	if(byte == AT_PARSER_DOUBLE_QUOTE){
		ctx->quotes_flag = !ctx->quotes_flag;
	}

	return 0;
}

static int at_stream_parse_passthrough_length_filed(uint8_t byte, struct at_stream_ctx *ctx)
{
	if(byte != 0 && byte <= MAX_PASSTHROUGH_DATA_SIZE){
		ctx->passthrough_data_length = byte;
		if(ctx->state == AT_STREAM_PASSTHROUGH_FLAG){
			ctx->state = AT_STREAM_PASSTHROUGH_DATA;
		}else{
			ctx->state = AT_STREAM_OTA_DATA_DATA;
		}
	}else{
		at_stream_parse_state_reset(ctx);
	}

	return 0;
}

static int at_stream_parse_passthrough_data_filed(uint8_t byte, struct at_stream_ctx *ctx)
{
	if(ctx->frame_buf_index < ctx->passthrough_data_length){
		ctx->frame_buf[ctx->frame_buf_index] = byte;
		ctx->frame_buf_index++;
	}

	if(ctx->frame_buf_index == ctx->passthrough_data_length){
		if(ctx->state == AT_STREAM_PASSTHROUGH_DATA){
			if(acts_ringbuf_space(ctx->data_obj.cbuf) >= (ctx->frame_buf_index + 1)){
				acts_ringbuf_put(ctx->data_obj.cbuf, &ctx->frame_buf_index, 1);
				acts_ringbuf_put(ctx->data_obj.cbuf, ctx->frame_buf, ctx->frame_buf_index);
			}else{
				ctx->data_obj.drop_cnt++;
			}
			at_stream_parse_state_reset(ctx);
			os_sem_give(ctx->data_obj.read_sem);
		}else{
			printk("parser frame len %d\n", ctx->frame_buf_index);
			print_buffer(ctx->frame_buf, 1, 16, 16, -1);
			if(acts_ringbuf_space(ctx->ota_obj.cbuf) >= (ctx->frame_buf_index + 1)){
				acts_ringbuf_put(ctx->ota_obj.cbuf, &ctx->frame_buf_index, 1);
				acts_ringbuf_put(ctx->ota_obj.cbuf, ctx->frame_buf, ctx->frame_buf_index);
			}else{
				ctx->ota_obj.drop_cnt++;
			}
			at_stream_parse_state_reset(ctx);
			os_sem_give(ctx->ota_obj.read_sem);
		}


	}

	return 0;
}

static int at_stream_parse_byte(struct at_stream_ctx *ctx, uint8_t byte)
{
	int ret_val = 0;
	//判断是否收到cc字符，如果at命令解析遇到cc字符，说明遇到透传数据
	if (ctx->state == AT_STREAM_AT_COMMAND){
		if (byte == AT_PASS_THROUGH_FLAG && ctx->data_obj.cbuf) {
			at_stream_parse_state_reset(ctx);
	  		ctx->state = AT_STREAM_PASSTHROUGH_FLAG;
	  		return AT_STREAM_NO_FRAME;
	  	}

		if (byte == AT_OTA_DATA_FLAG && ctx->ota_obj.cbuf){
			at_stream_parse_state_reset(ctx);
			ctx->state = AT_STREAM_OTA_DATA_FLAG;
			return AT_STREAM_NO_FRAME;
		}
	}


	switch(ctx->state){
		case AT_STREAM_AT_COMMAND:
			ret_val = at_stream_parse_at_command_byte(byte, ctx);
			break;

		case AT_STREAM_PASSTHROUGH_FLAG:
		case AT_STREAM_OTA_DATA_FLAG:
			ret_val = at_stream_parse_passthrough_length_filed(byte, ctx);
			break;

		case AT_STREAM_PASSTHROUGH_DATA:
		case AT_STREAM_OTA_DATA_DATA:
			ret_val = at_stream_parse_passthrough_data_filed(byte, ctx);
			break;
	}

	return ret_val;
}

int at_stream_parse_frame(struct at_stream_ctx *ctx, uint8_t byte, uint8_t isr_type)
{
	int16_t parse_data = at_stream_parse_byte(ctx, byte);

	//printk("parse recv %x index %d return %x state %d isr %d\n", byte, ctx->frame_buf_index, parse_data, ctx->state, isr_type);

	return parse_data;
}

/* @reson 0 transmission complete ,1 half transmission
*/
void at_stream_rx_isr(struct device *dev, u32_t priv_data, int reson)
{
	uint8_t *data;
	uint32_t data_len;
    struct at_stream_ctx *ctx = (struct at_stream_ctx*)priv_data;

	data_len =  RAW_BUFFER_SIZE / 2;
    if(reson == DMA_IRQ_HF){
        data = ctx->raw_buf;
    }else{
        data = ctx->raw_buf + data_len;
    }

    while(data_len){
		at_stream_parse_frame(ctx, *data, 0);
		data++;
		data_len--;
	}
}

#ifdef CONFIG_UART_DMA_RX_TIMEOUT_DRIVEN
static void at_stream_rx_timeout_handle(void *priv_data, uint8_t *data, uint32_t data_len)
{
    struct at_stream_ctx *ctx = (struct at_stream_ctx*)priv_data;

    while(data_len){
		at_stream_parse_frame(ctx, *data, 1);
		data++;
		data_len--;
	}

}
#endif
void at_stream_thread_dispatch(void *p1, void *p2, void *p3)
{
	int ret = 0;
	struct at_stream_ctx *ctx = (struct at_stream_ctx *)p1;

	printk("%s init\n", __func__);

	while(1){
        ret = k_poll(ctx->evt, 3, K_FOREVER);
        if (ret == 0) {
            if (ctx->evt[0].state == K_POLL_STATE_SEM_AVAILABLE){
                k_sem_take(ctx->at_obj.read_sem, K_NO_WAIT);
				if(ctx->at_obj.read_cb){
	                ctx->at_obj.read_cb(ctx->at_obj.priv_data);
				}
				ctx->evt[0].state = K_POLL_STATE_NOT_READY;
            }

            if (ctx->evt[1].state == K_POLL_STATE_SEM_AVAILABLE){
                k_sem_take(ctx->data_obj.read_sem, K_NO_WAIT);
				if(ctx->data_obj.read_cb){
	                ctx->data_obj.read_cb(ctx->data_obj.priv_data);
				}

				ctx->evt[1].state = K_POLL_STATE_NOT_READY;
            }

			if (ctx->evt[2].state == K_POLL_STATE_SEM_AVAILABLE){
                k_sem_take(ctx->ota_obj.read_sem, K_NO_WAIT);
				if(ctx->ota_obj.read_cb){
	                ctx->ota_obj.read_cb(ctx->ota_obj.priv_data);
				}

				ctx->evt[2].state = K_POLL_STATE_NOT_READY;
            }
        }

		printk("thread dispatch\n");
	}
}


void *at_stream_init(struct uart_at_stream_param *uparam)
{
	void *at_obj = NULL;
	struct at_stream_ctx *ctx = at_stream_get_ctx();

	if(!ctx->init){
	    ctx->raw_buf = mem_malloc(RAW_BUFFER_SIZE);
	    if (!ctx->raw_buf) {
	        return NULL;
	    }

		ctx->frame_buf = mem_malloc(FRAME_BUFFER_SIZE);
	    if (!ctx->frame_buf) {
			mem_free(ctx->raw_buf);
	        return NULL;
	    }

	    ctx->uart_dev = device_get_binding(uparam->dev_name);

	    if (!ctx->uart_dev) {
	        SYS_LOG_ERR("device binding fail %s", uparam->dev_name);
			mem_free(ctx->raw_buf);
			mem_free(ctx->frame_buf);
	        return  NULL;
	    }

		at_stream_parse_state_reset(ctx);
	}

	if(uparam->obj_type == 0){
		ctx->end_char_mode = uparam->end_char_mode;
		ctx->at_obj.cbuf = acts_ringbuf_alloc(uparam->recv_buf_size);
		ctx->at_obj.read_sem = (os_sem *)uparam->read_sem;
		ctx->at_obj.priv_data = uparam->priv_data;
		ctx->at_obj.read_cb = uparam->read_cb;
		os_sem_init(ctx->at_obj.read_sem, 0, 1);

		k_poll_event_init(&ctx->evt[0], K_POLL_TYPE_SEM_AVAILABLE, K_POLL_MODE_NOTIFY_ONLY, ctx->at_obj.read_sem);

		at_obj = (void *)&ctx->at_obj;
	}else if(uparam->obj_type == 1){
		ctx->data_obj.cbuf = acts_ringbuf_alloc(uparam->recv_buf_size);
		ctx->data_obj.read_sem = (os_sem *)uparam->read_sem;
		ctx->data_obj.priv_data = uparam->priv_data;
		ctx->data_obj.read_cb = uparam->read_cb;
		os_sem_init(ctx->data_obj.read_sem, 0, 1);

		k_poll_event_init(&ctx->evt[1], K_POLL_TYPE_SEM_AVAILABLE, K_POLL_MODE_NOTIFY_ONLY, ctx->data_obj.read_sem);

		at_obj = (void *)&ctx->data_obj;
	}else if(uparam->obj_type == 2){
		ctx->ota_obj.cbuf = acts_ringbuf_alloc(uparam->recv_buf_size);
		ctx->ota_obj.read_sem = (os_sem *)uparam->read_sem;
		ctx->ota_obj.priv_data = uparam->priv_data;
		ctx->ota_obj.read_cb = uparam->read_cb;
		os_sem_init(ctx->ota_obj.read_sem, 0, 1);

		k_poll_event_init(&ctx->evt[2], K_POLL_TYPE_SEM_AVAILABLE, K_POLL_MODE_NOTIFY_ONLY, ctx->ota_obj.read_sem);

		at_obj = (void *)&ctx->ota_obj;
	}else{
		;
	}

	if (!ctx->init){
		//create receive thread
		ctx->rx_stack = mem_malloc(AT_STREAM_RX_TREHAD_STACK_SIZE);

		if(!ctx->rx_stack){
			return NULL;
		}

		k_thread_create(&ctx->rx_thread,ctx->rx_stack, AT_STREAM_RX_TREHAD_STACK_SIZE, at_stream_thread_dispatch, \
		 (void *)ctx, NULL, NULL, 4, 0, 0);

		ctx->init = true;
	}

	return at_obj;
}


int at_stream_open(void *handle)
{
	struct at_stream_ctx *ctx = at_stream_get_ctx();

	os_mutex_lock(&at_stream_mutex, OS_FOREVER);

	if(ctx->opened == 0){
	    /* uart rx fifo access: dma */
	    uart_fifo_switch(ctx->uart_dev, 0, UART_FIFO_DMA);
	    uart_dma_recv_init(ctx->uart_dev, 0xff, at_stream_rx_isr, ctx);
	    uart_dma_recv_config(ctx->uart_dev, ctx->raw_buf, RAW_BUFFER_SIZE);
#ifdef CONFIG_UART_DMA_RX_TIMEOUT_DRIVEN
		uart_dma_recv_set_timeout_start(ctx->uart_dev, 500, at_stream_rx_timeout_handle, (void *)ctx);
#endif
	    uart_dma_recv_start(ctx->uart_dev);

		ctx->opened = true;
	}

	os_mutex_unlock(&at_stream_mutex);

    return 0;
}

int at_stream_write(void *handle, unsigned char *buf, int num)
{
	int tx_num = 0;
	struct at_stream_ctx *ctx = at_stream_get_ctx();

	os_mutex_lock(&at_stream_mutex, OS_FOREVER);

	if(handle == &ctx->data_obj){
		uart_poll_out(ctx->uart_dev, AT_PASS_THROUGH_FLAG);

		uart_poll_out(ctx->uart_dev, num);
	}

	if(handle == &ctx->ota_obj){
		uart_poll_out(ctx->uart_dev, AT_OTA_DATA_FLAG);

		uart_poll_out(ctx->uart_dev, num);
	}

    do {
		uart_poll_out(ctx->uart_dev, buf[tx_num++]);
    } while (tx_num < num);

	os_mutex_unlock(&at_stream_mutex);

	return 0;
}

static int at_stream_read_at_data(struct at_stream_ctx *ctx, unsigned char *buf, int num)
{
    uint8_t byte;
	uint32_t index = 0;

    while(1){
        if(acts_ringbuf_length(ctx->at_obj.cbuf) == 0){
            break;
        }

        acts_ringbuf_get(ctx->at_obj.cbuf, &byte, 1);

        if(byte != '\0'){
            buf[index] = byte;
			if(index < num){
				index++;
			}
        }else{
            buf[index] = byte;
			if(index < num){
				index++;
			}
            break;
        }
    }

    return index;
}

int at_stream_read(void *handle, unsigned char *buf, int num)
{
	struct at_stream_ctx *ctx = at_stream_get_ctx();

	if(&ctx->at_obj == handle){
		//read at data
		return at_stream_read_at_data(ctx, buf, num);
	}else if(&ctx->data_obj == handle){
		return acts_ringbuf_get(ctx->data_obj.cbuf, buf, num);
	}else if(&ctx->ota_obj == handle){
		return acts_ringbuf_get(ctx->ota_obj.cbuf, buf, num);
	}else{
		return 0;
	}
}

int at_stream_get_length(void *handle)
{
	struct at_stream_ctx *ctx = at_stream_get_ctx();

	if(&ctx->at_obj == handle){
		//read at data
		return acts_ringbuf_length(ctx->at_obj.cbuf);
	}else if(&ctx->data_obj == handle){
		return acts_ringbuf_length(ctx->data_obj.cbuf);
	}else if(&ctx->ota_obj == handle){
		return acts_ringbuf_length(ctx->ota_obj.cbuf);
	}else{
		return 0;
	}
}

int at_stream_flush(void *handle)
{
	struct at_stream_ctx *ctx = at_stream_get_ctx();

	if(&ctx->at_obj == handle){
		//read at data
		return acts_ringbuf_drop_all(ctx->at_obj.cbuf);
	}else if(&ctx->data_obj == handle){
		return acts_ringbuf_drop_all(ctx->data_obj.cbuf);
	}else if(&ctx->ota_obj == handle){
		return acts_ringbuf_drop_all(ctx->ota_obj.cbuf);
	}else{
		return 0;
	}

}

int at_stream_close(void *handle)
{
	struct at_stream_ctx *ctx = at_stream_get_ctx();

	if(&ctx->at_obj == handle){
		return -EIO;
	}else{
		return 0;
	}

}

int at_stream_destory(void *handle)
{
	struct at_stream_ctx *ctx = at_stream_get_ctx();

	if(&ctx->at_obj == handle){
		return -EIO;
	}else if(&ctx->data_obj == handle){
		acts_ringbuf_free(ctx->data_obj.cbuf);
	}else if(&ctx->ota_obj == handle){
		acts_ringbuf_free(ctx->ota_obj.cbuf);
	}else{
		;
	}

	return 0;
}


#if 0

#include <device.h>
#include <init.h>

struct uart_at_demo
{
    os_sem read_sem;
    void *ctx;
};


static struct uart_at_demo uart_at_ctx;
static struct uart_at_demo uart_passthrough_ctx;
static struct uart_at_demo uart_ota_data_ctx;

static int at_stream_test(struct device *arg)
{
    int stream_len;
    char at_buffer[256];
    struct uart_at_stream_param param;

	printk("at stream init at parser object\n");

    param.dev_name = CONFIG_AT_PARSER_ON_DEV_NAME;
    param.read_sem =  &uart_at_ctx.read_sem;
    param.recv_buf_size = 1024;
    param.obj_type = 0;
    param.end_char_mode = AT_CHAR_MODE_WITH_CRLF;

    uart_at_ctx.ctx = at_stream_init(&param);

    at_stream_open(uart_at_ctx.ctx);

	printk("at stream init pass through object\n");
    param.dev_name = CONFIG_AT_PARSER_ON_DEV_NAME;
    param.read_sem =  &uart_passthrough_ctx.read_sem;
    param.recv_buf_size = 1024;
    param.obj_type = 1;
    param.end_char_mode = 0;

    uart_passthrough_ctx.ctx = at_stream_init(&param);

    at_stream_open(uart_passthrough_ctx.ctx);

	printk("at stream init ota object\n");

    param.dev_name = CONFIG_AT_PARSER_ON_DEV_NAME;
    param.read_sem =  &uart_ota_data_ctx.read_sem;
    param.recv_buf_size = 1024;
    param.obj_type = 2;
    param.end_char_mode = 0;

    uart_ota_data_ctx.ctx = at_stream_init(&param);

    at_stream_open(uart_ota_data_ctx.ctx);

    while(1){

        os_sem_take(&uart_at_ctx.read_sem, OS_FOREVER);

        stream_len = at_stream_get_length(uart_at_ctx.ctx);

		printk("at stream read at data %d\n", stream_len);

        if(!stream_len){
            continue;
        }

        at_stream_read(uart_at_ctx.ctx, at_buffer, sizeof(at_buffer));

        printk("read line %s\n", at_buffer);

        os_sem_take(&uart_passthrough_ctx.read_sem, OS_FOREVER);

        stream_len = at_stream_get_length(uart_passthrough_ctx.ctx);

		printk("at stream read pass through data %d\n", stream_len);

        if(!stream_len){
            continue;
        }

        at_stream_read(uart_passthrough_ctx.ctx, at_buffer, stream_len);

        at_stream_write(uart_passthrough_ctx.ctx, at_buffer, stream_len);

        printk("read pass through data %d\n", stream_len);

        print_buffer(at_buffer, 1, stream_len, 16, -1);

        os_sem_take(&uart_ota_data_ctx.read_sem, OS_FOREVER);

        stream_len = at_stream_get_length(uart_ota_data_ctx.ctx);

		printk("at stream read ota data %d\n", stream_len);

        if(!stream_len){
            continue;
        }

        at_stream_read(uart_ota_data_ctx.ctx, at_buffer, stream_len);

        at_stream_write(uart_ota_data_ctx.ctx, at_buffer, stream_len);

        printk("read ota data %d\n", stream_len);

        print_buffer(at_buffer, 1, stream_len, 16, -1);
    }

	return 0;
}

SYS_INIT(at_stream_test, APPLICATION, 20);




#endif

