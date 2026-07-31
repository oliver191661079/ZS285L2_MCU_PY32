#include <device.h>
#include <stream.h>
#include <logging/sys_log.h>
#include <uart.h>
#include <acts_ringbuf.h>
#include <soc.h>
#include <string.h>
#include <ringbuff_stream.h>
#include "uart_stream.h"

struct uart_stream_info {
	struct device *uart_dev;
	u32_t uart_baud_rate;
	u8_t opened;
};

static int uart_stream_init(io_stream_t handle, void *param)
{
	struct uart_stream_info *info = NULL;
	struct uart_stream_param *uparam = param;

	int err = 0;

	info = malloc(sizeof(struct uart_stream_info));
	if (!info) {
		ACT_LOG_ID_ERR(ALF_STR_uart_stream_init__OOM, 0);
		err = -ENOSR;
		goto error;
	}
	memset(info, 0, sizeof(struct uart_stream_info));

	info->uart_dev = device_get_binding(uparam->uart_dev_name);
	info->uart_baud_rate = uparam->uart_baud_rate;
	if (!info->uart_dev) {
		SYS_LOG_ERR("device binding fail %s", uparam->uart_dev_name);
		err = -EIO;
		goto error;
	}

	info->opened = 0;
	handle->data = info;

	return 0;

 error:
	if (info) {
		free(info);
	}

	return err;
}

static int uart_stream_open(io_stream_t handle, stream_mode mode)
{
	struct uart_stream_info *info = handle->data;

	if (!info || info->opened)
		return -EACCES;

	os_sched_lock();

	info->opened = true;
	handle->mode = mode;
	handle->total_size = 0;
	handle->cache_size = 0;
	handle->rofs = 0;
	handle->wofs = 0;

	os_sched_unlock();

	return 0;
}

/* If there is not enough data, return directly.
 */
static int uart_stream_read(io_stream_t handle, unsigned char *buf, int num)
{
	struct uart_stream_info *info = (struct uart_stream_info *)handle->data;

	if (!info)
		return 0;

	int read_len;
	read_len = uart_fifo_read(info->uart_dev, buf, num);
	if (read_len > 0) {
		//printk("[otauart] read %d/%d\n",read_len, num);
		//print_buffer(buf, 1, read_len, 16, -1);
	}
	handle->rofs += read_len;

	return read_len;
}

static int uart_stream_tell(io_stream_t handle)
{
	struct uart_stream_info *info = (struct uart_stream_info *)handle->data;

	if (!info)
		return -EACCES;

	if (!info->opened) {
		return 0;
	}

	return 0;
}

static int uart_stream_get_length(io_stream_t handle)
{
	struct uart_stream_info *info = (struct uart_stream_info *)handle->data;

	if (!info)
		return -EACCES;

	return 0;
}

static int uart_stream_get_space(io_stream_t handle)
{
	struct uart_stream_info *info = (struct uart_stream_info *)handle->data;

	if (!info)
		return -EACCES;

	return 0;
}

static int uart_stream_write(io_stream_t handle, unsigned char *buf, int num)
{
	int tx_num = 0, ret;
	struct uart_stream_info *info = (struct uart_stream_info *)handle->data;

	if (!info)
		return -EACCES;
 write_again:
	ret = uart_fifo_fill(info->uart_dev, buf + tx_num, num - tx_num);
	printk("[otauart] write %d/%d\n", ret, num);
	if ((ret > 0)) {
		tx_num += ret;
		if (num - tx_num)
			goto write_again;
	} else
		return tx_num;

	return tx_num;
}

static int uart_stream_flush(io_stream_t handle)
{
	struct uart_stream_info *info = (struct uart_stream_info *)handle->data;
	u32_t last_remain_len;
	u8_t buf[10];
	/* 1, wait uart fifo empty */
	k_busy_wait(2000);
	do {
		last_remain_len = uart_fifo_read(info->uart_dev, buf, 10);
		/* bytes per second: info->uart_baud_rate/10
		 * us per byte: 1000000 / info->uart_baud_rate/10
		 * for example: 3M baud rate, 4us per byte
		 */
		k_busy_wait((10000000 / info->uart_baud_rate) + 50);
	} while (last_remain_len);

	return 0;
}

static int uart_stream_close(io_stream_t handle)
{
	struct uart_stream_info *info = (struct uart_stream_info *)handle->data;
	if (!info)
		return -EACCES;

	os_sched_lock();

	info->opened = false;

	os_sched_unlock();

	return 0;
}

static int uart_stream_destroy(io_stream_t handle)
{
	int res = 0;
	struct uart_stream_info *info = (struct uart_stream_info *)handle->data;

	if (!info)
		return -EACCES;

	os_sched_lock();

	free(info);

	handle->data = NULL;

	os_sched_unlock();

	return res;
}

static const stream_ops_t uart_stream_ops = {
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

io_stream_t cdc_uart_stream_create(void *param)
{
	return stream_create(&uart_stream_ops, param);
}
