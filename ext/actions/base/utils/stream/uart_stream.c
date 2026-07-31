#define SYS_LOG_DOMAIN "us"

#include <device.h>
#include <stream.h>
#include <logging/sys_log.h>
#include <uart.h>
#include <acts_ringbuf.h>
//#include <malloc.h>
#include <soc.h>
#include <string.h>
#include <ringbuff_stream.h>
#include "uart_stream.h"

#define RAW_BUFFER_SIZE                 256
#define RINGBUF_STREAM_SIZE             1536

struct uart_stream_info {
    struct device *uart_dev;
    u32_t uart_baud_rate;
    io_stream_t rbuf_stream;
    struct acts_ringbuf *rbuf;
    u8_t *raw_buf;
    u8_t opened;
    u32_t drop_cnt;
};


/* @reson 0 transmission complete ,1 half transmission
*/
void uart_rx_isr(struct device *dev, u32_t priv_data, int reson)
{
	uint8_t *data;
	uint32_t data_len;
    struct uart_stream_info* info = (struct uart_stream_info*)priv_data;

	data_len =  RAW_BUFFER_SIZE / 2;
    if(reson == DMA_IRQ_HF){
        data = info->raw_buf;
    }else{
        data = info->raw_buf + data_len;
    }

    //printk("\n[us] rx isr\n");
    if(stream_get_space(info->rbuf_stream) >= data_len){
		stream_write(info->rbuf_stream, data, data_len);
	}else{
		info->drop_cnt++;
	}
}

#ifdef CONFIG_UART_DMA_RX_TIMEOUT_DRIVEN
static void uart_rx_timeout_handle(void *priv_data, uint8_t *data, uint32_t data_len)
{
    struct uart_stream_info* info = (struct uart_stream_info*)priv_data;

    if(data_len > 0) {
        SYS_LOG_INF("%d", data_len);
    }
    if(stream_get_space(info->rbuf_stream) >= data_len){
		stream_write(info->rbuf_stream, data, data_len);
	}else{
		info->drop_cnt++;
	}
}
#endif

int uart_stream_init(io_stream_t handle,void *param)
{
    struct uart_stream_info* info = NULL;
    struct uart_stream_param *uparam= param;

    int err = 0;

    info = mem_malloc(sizeof(struct uart_stream_info));
    if (!info) {
        SYS_LOG_ERR("OOM");
        err = -ENOSR;
        goto error;
    }
    memset(info, 0, sizeof(struct uart_stream_info));

    info->raw_buf = mem_malloc(RAW_BUFFER_SIZE);
    if (!info->raw_buf) {
        SYS_LOG_ERR("OOM");
        err = -ENOSR;
        goto error;
    }

    info->uart_dev = device_get_binding(uparam->uart_dev_name);
    info->uart_baud_rate = uparam->uart_baud_rate;
    if (!info->uart_dev) {
        SYS_LOG_ERR("device binding fail %s", uparam->uart_dev_name);
        err = -EIO;
        goto error;
    }

    info->rbuf = acts_ringbuf_alloc(RINGBUF_STREAM_SIZE);
    if (!info->rbuf) {
        SYS_LOG_ERR("rbuf NULL");
        err = -ENOSR;
        goto error;
    }

    //info->rbuf_stream = stream_create(TYPE_RINGBUFF_STREAM, info->rbuf);

	info->rbuf_stream = ringbuff_stream_create(info->rbuf);
    if (!info->rbuf_stream) {
        SYS_LOG_INF("rbuf_stream NULL");
        err = -ENOSR;
        goto error;
    }

    info->opened = 0;
    handle->data = info;

    SYS_LOG_INF("uart stream init %p\n", info);
    return 0;

error:
    if (info->rbuf) {
        acts_ringbuf_free(info->rbuf);
    }

    if (info->raw_buf) {
        mem_free(info->raw_buf);
    }

    if (info) {
        mem_free(info);
    }
    return err;
}

int uart_stream_open(io_stream_t handle, stream_mode mode)
{
    struct uart_stream_info* info = handle->data;

    if (!info || info->opened)
        return -EACCES;

    os_sched_lock();

    stream_open(info->rbuf_stream, MODE_IN_OUT);

    stream_flush(info->rbuf_stream);

    /* uart rx fifo access: dma */
    uart_fifo_switch(info->uart_dev, 0, UART_FIFO_DMA);
    uart_fifo_switch(info->uart_dev, 1, UART_FIFO_CPU);
    uart_dma_recv_init(info->uart_dev, 0xff, uart_rx_isr, info);
    uart_dma_recv_config(info->uart_dev, info->raw_buf, RAW_BUFFER_SIZE);
#ifdef CONFIG_UART_DMA_RX_TIMEOUT_DRIVEN
    uart_dma_recv_set_timeout_start(info->uart_dev, 500, uart_rx_timeout_handle, (void *)info);
#endif
    uart_dma_recv_start(info->uart_dev);

    info->opened = true;
    handle->mode = mode;
    handle->total_size = info->rbuf_stream->total_size;
    handle->cache_size = 0;
    handle->rofs = 0;
    handle->wofs = 0;

    os_sched_unlock();

    SYS_LOG_INF("uart stream open %p", info);

    return 0;
}

/* If there is not enough data, return directly.
 */
int uart_stream_read(io_stream_t handle, unsigned char *buf, int num)
{
    int read_len;

    struct uart_stream_info *info = (struct uart_stream_info *)handle->data;

    if (!info)
        return 0;


    read_len = stream_read(info->rbuf_stream, buf, num);
    if (read_len > 0) {
        SYS_LOG_INF("len %d", read_len);
    }

    handle->rofs = info->rbuf_stream->rofs;

    return read_len;
}

int uart_stream_tell(io_stream_t handle)
{
    struct uart_stream_info *info = (struct uart_stream_info *)handle->data;

    if (!info)
        return -EACCES;

    if (!info->opened) {
        return 0;
    }

    if(info->rbuf_stream)
        return stream_tell(info->rbuf_stream);

    return 0;
}

int uart_stream_get_length(io_stream_t handle)
{
    struct uart_stream_info *info = (struct uart_stream_info *)handle->data;

    if(!info)
        return -EACCES;

    if(info->rbuf_stream)
        return stream_get_length(info->rbuf_stream);

    return 0;
}

int uart_stream_get_space(io_stream_t handle)
{
    struct uart_stream_info *info = (struct uart_stream_info *)handle->data;

    if (!info)
        return -EACCES;

    if (info->rbuf_stream)
        return stream_get_space(info->rbuf_stream);

    return 0;
}

int uart_stream_write(io_stream_t handle, unsigned char *buf, int num)
{
    int tx_num = 0;
    struct uart_stream_info *info = (struct uart_stream_info *)handle->data;

    if (!info)
        return -EACCES;

    SYS_LOG_INF("len %d", num);
    do {
        uart_poll_out(info->uart_dev, buf[tx_num++]);
    } while (tx_num < num);

    return tx_num;
}

int uart_stream_flush(io_stream_t handle)
{
    struct uart_stream_info *info = (struct uart_stream_info *)handle->data;

    stream_flush(info->rbuf_stream);


    SYS_LOG_INF("rbuf_stream: %d", stream_get_length(info->rbuf_stream));

    return 0;
}

int uart_stream_close(io_stream_t handle)
{
    struct uart_stream_info *info = (struct uart_stream_info *)handle->data;
    if (!info)
        return -EACCES;

    os_sched_lock();

#ifdef CONFIG_UART_DMA_RX_TIMEOUT_DRIVEN
    uart_dma_recv_set_timeout_stop(info->uart_dev);
#endif
    uart_dma_recv_stop(info->uart_dev);

    if (info->rbuf_stream){
        stream_close(info->rbuf_stream);
    }

    info->opened = false;

    os_sched_unlock();

    return 0;
}

int uart_stream_destroy(io_stream_t handle)
{
    int res = 0;
    struct uart_stream_info *info = (struct uart_stream_info *)handle->data;

    if (!info)
        return -EACCES;

    os_sched_lock();

    if (info->rbuf) {
        acts_ringbuf_free(info->rbuf);
        info->rbuf = NULL;
    }

    if (info->rbuf_stream) {
        stream_destroy(info->rbuf_stream);
        info->rbuf_stream = NULL;
    }

    mem_free(info);

    handle->data = NULL;

    os_sched_unlock();

    return res;
}

const stream_ops_t uart_stream_ops = {
    .init = uart_stream_init,
    .open = uart_stream_open,
    .read = uart_stream_read,
    .tell = uart_stream_tell,
    .get_length = uart_stream_get_length,
    .get_space = uart_stream_get_space,
    .write = uart_stream_write,
    .flush = uart_stream_flush,
    .close = uart_stream_close,
    .destroy = uart_stream_destroy,
};

io_stream_t uart_stream_create(void *param)
{
	return stream_create(&uart_stream_ops, param);
}