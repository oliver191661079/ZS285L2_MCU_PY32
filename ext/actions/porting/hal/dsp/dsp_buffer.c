#include <logging/sys_log.h>
#define ACT_LOG_MODEL_ID ALF_MODEL_DSP_BUFFER
#include <mem_manager.h>
#include "dsp_inner.h"

struct dsp_session_buf *dsp_session_buf_init(struct dsp_session *session,
					     void *data, unsigned int size)
{
	struct dsp_session_buf *buf = mem_malloc(sizeof(*buf));
	if (buf == NULL)
		return NULL;

	acts_ringbuf_init(&buf->buf, data, ACTS_RINGBUF_NELEM(size));
	buf->session = session;
	return buf;
}

void *dsp_session_buf_get_ringbuf(struct dsp_session_buf *buf)
{
	return &buf->buf;
}

void dsp_session_buf_destroy(struct dsp_session_buf *buf)
{
	if (buf)
		mem_free(buf);
}

struct dsp_session_buf *dsp_session_buf_alloc(struct dsp_session *session, unsigned int size)
{
	struct dsp_session_buf *buf = mem_malloc(sizeof(*buf));
	if (buf == NULL)
		return NULL;

	void *data = mem_malloc(size);
	if (data == NULL) {
		mem_free(buf);
		return NULL;
	}

	acts_ringbuf_init(&buf->buf, data, ACTS_RINGBUF_NELEM(size));
	buf->session = session;
	return buf;
}

void dsp_session_buf_free(struct dsp_session_buf *buf)
{
	if (buf) {
		mem_free((void *)(buf->buf.cpu_ptr));
		mem_free(buf);
	}
}

void dsp_session_buf_reset(struct dsp_session_buf *buf)
{
	acts_ringbuf_reset(&buf->buf);
}

unsigned int dsp_session_buf_size(struct dsp_session_buf *buf)
{
	return ACTS_RINGBUF_SIZE8(acts_ringbuf_size(&buf->buf));
}

unsigned int dsp_session_buf_space(struct dsp_session_buf *buf)
{
	return ACTS_RINGBUF_SIZE8(acts_ringbuf_space(&buf->buf));
}

unsigned int dsp_session_buf_length(struct dsp_session_buf *buf)
{
	return ACTS_RINGBUF_SIZE8(acts_ringbuf_length(&buf->buf));
}

#if CONFIG_DSP_ACTIVE_POWER_LATENCY_MS >= 0
extern u32_t dsp_get_sleep_time(void);
u32_t dsp_work_time;
u32_t dsp_get_work_time(void)
{
	return dsp_work_time;
}

static void dsp_session_read_kick(struct dsp_session_buf *buf)
{
	int ret = 0;
	int wake_time = k_cycle_get_32() / 24;
#ifdef CONFIG_DSP_SUSPEND
	ret |= dsp_session_resume(buf->session);
#endif
#ifdef CONFIG_DSP_LIGHT_SLEEP
	ret |= dsp_session_kick(buf->session);
#endif
	if(ret == 0){
		dsp_work_time = k_cycle_get_32() / 24;
		SYS_LOG_DBG("kick %u (sleep %u us wake %u us)", dsp_session_buf_length(buf), (k_cycle_get_32() / 24) - dsp_get_sleep_time(),
				dsp_work_time - wake_time);
	}

}

static int count = 0;
static int wait_percent_sum = 0;
static int work_percent_sum = 0;
void reset_dsp_count(void)
{
    count = 0;
    wait_percent_sum = 0;
    work_percent_sum = 0;
}


extern struct dsp_pwr_t dsp_pwr_info_exchange;

void dsp_work_percent_count(void)
{
		static int work = 100;
        static int percent_id = 100;
        static int wait = 100;
        int aver_wait,aver_work;
		int work_percent = dsp_wait_get_work_percent();
        int wait_percent = dsp_wait_get_wait_percent();
        int percent_cnt = dsp_pwr_info_exchange.dsp_percent_cnt;
        if(work_percent > 100 || wait_percent > 100){
            return;
        }

        if(percent_cnt != percent_id)
        {
            work = work_percent;
            wait = wait_percent;

            percent_id =  percent_cnt;
            count ++;

            wait_percent_sum += wait;
            work_percent_sum += work;

            aver_wait = wait_percent_sum/count;
            aver_work = work_percent_sum/count;
            if(dsp_pwr_info_exchange.dsp_idle_count_enable)
                ACT_LOG_ID_INF(ALF_STR_dsp_work_percent_count__AV_W_D_W_D_D_AV_WA_D, 5,aver_work,work_percent,percent_cnt,aver_wait,wait);

        }
}

static void dsp_session_buf_read_kick(struct dsp_session_buf *buf, u32_t size)
{
	u32_t limit_size;

	if(dsp_session_buf_size(buf) == 4096){
		limit_size = dsp_session_buf_size(buf) - 1536;
	}else{
		limit_size = (dsp_session_buf_size(buf) >> 1);
	}

	if(!dsp_wait_get_latency_prio()){
		if(dsp_wait_cpu_get_allow_flag()){
			if ( dsp_session_buf_length(buf) < limit_size) {
				dsp_wait_disable_read_buf_condition_satisfy();
				dsp_session_read_kick(buf);
			}else{
				dsp_wait_enable_read_buf_condition_satisfy();
			}
		}
	}else{
		if ( dsp_session_buf_length(buf) == 0) {
			dsp_session_read_kick(buf);
		}

	}
    dsp_work_percent_count();

}


void dsp_session_write_kick(struct dsp_session_buf *buf)
{
	int ret = 0;
	int wake_time = k_cycle_get_32() / 24;

#ifdef CONFIG_DSP_SUSPEND
	ret |= dsp_session_resume(buf->session);
#endif

#ifdef CONFIG_DSP_LIGHT_SLEEP
	ret |= dsp_session_kick(buf->session);
#endif

	if(ret == 0){
		dsp_work_time = k_cycle_get_32() / 24;
		SYS_LOG_DBG("kick %u (sleep %u us wake %u us)", dsp_session_buf_space(buf), (k_cycle_get_32() / 24) - dsp_get_sleep_time(),\
				dsp_work_time - wake_time);
	}

}

void dsp_session_buf_write_kick(struct dsp_session_buf *buf, u32_t size)
{
	if(!dsp_wait_get_latency_prio()){
		if(dsp_wait_cpu_get_allow_flag()){
			if ((dsp_session_buf_space(buf) < (dsp_session_buf_size(buf) / 2))) {
				dsp_wait_disable_write_buf_condition_satisfy();
				dsp_session_write_kick(buf);
			}else{
				dsp_wait_enable_write_buf_condition_satisfy();
			}
		}
	}else{
		if (dsp_session_buf_space(buf) < (dsp_session_buf_size(buf) / 2)) {
			dsp_session_write_kick(buf);
		}
	}
    dsp_work_percent_count();
}

#endif

int dsp_session_buf_read(struct dsp_session_buf *buf, void *data, unsigned int size)
{
	int len = ACTS_RINGBUF_SIZE8(acts_ringbuf_get(&buf->buf, data, ACTS_RINGBUF_NELEM(size)));

#if CONFIG_DSP_ACTIVE_POWER_LATENCY_MS >= 0
	dsp_session_buf_read_kick(buf, size);
#endif

	return len;
}

int dsp_session_buf_write(struct dsp_session_buf *buf, const void *data, unsigned int size)
{
	int len = ACTS_RINGBUF_SIZE8(acts_ringbuf_put(&buf->buf, data, ACTS_RINGBUF_NELEM(size)));

#if CONFIG_DSP_ACTIVE_POWER_LATENCY_MS >= 0
	dsp_session_buf_write_kick(buf, size);
#endif

	return len;
}

int dsp_session_buf_read_to_buffer(struct dsp_session_buf *buf, void *buffer, unsigned int size)
{
	int len = ACTS_RINGBUF_SIZE8(
			acts_ringbuf_read(&buf->buf, buffer, ACTS_RINGBUF_NELEM(size),
					NULL));
#if CONFIG_DSP_ACTIVE_POWER_LATENCY_MS >= 0
	dsp_session_buf_read_kick(buf, size);
#endif

    return len;
}

int dsp_session_buf_read_to_stream(struct dsp_session_buf *buf,
		void *stream, unsigned int size,
		dsp_session_buf_write_fn stream_write)
{
	int len = ACTS_RINGBUF_SIZE8(
			acts_ringbuf_read(&buf->buf, stream, ACTS_RINGBUF_NELEM(size),
					(acts_ringbuf_write_fn)stream_write));

	//printk("%s, read len 0x%x from dsp outbuf to outstream!\n\n", __func__, len);

#if CONFIG_DSP_ACTIVE_POWER_LATENCY_MS >= 0
	dsp_session_buf_read_kick(buf, size);
#endif

	return len;
}

int dsp_session_buf_write_from_buffer(struct dsp_session_buf *buf, void *buffer, unsigned int size)
{
    int len = ACTS_RINGBUF_SIZE8(
            acts_ringbuf_write(&buf->buf, buffer, ACTS_RINGBUF_NELEM(size),
                    NULL));

	//printk("%s: %d: write to dsp session in_buf: %p,  len: 0x%x\n\n", __func__, __LINE__, buffer, len);

#if CONFIG_DSP_ACTIVE_POWER_LATENCY_MS >= 0
	dsp_session_buf_write_kick(buf, size);
#endif

    return len;
}

int dsp_session_buf_write_from_stream(struct dsp_session_buf *buf,
		void *stream, unsigned int size,
		dsp_session_buf_read_fn stream_read)
{
	int len = ACTS_RINGBUF_SIZE8(
			acts_ringbuf_write(&buf->buf, stream, ACTS_RINGBUF_NELEM(size),
					(acts_ringbuf_read_fn)stream_read));

	//printk("%s, dsp session inbuf write 0x%x finish!\n\n", __func__, len);

#if CONFIG_DSP_ACTIVE_POWER_LATENCY_MS >= 0
	dsp_session_buf_write_kick(buf, size);
#endif

	return len;
}

size_t dsp_session_buf_drop_all(struct dsp_session_buf *buf)
{
	return acts_ringbuf_drop_all(&buf->buf);
}

