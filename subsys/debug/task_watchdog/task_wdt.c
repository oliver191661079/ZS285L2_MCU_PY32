/*
 * Copyright (c) 2020 Libre Solar Technologies GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <errno.h>
#include <debug/task_wdt.h>


/*
 * This dummy channel is used to continue feeding the hardware watchdog if the
 * task watchdog timeouts are too long for regular updates
 */
#define TASK_WDT_BACKGROUND_CHANNEL UINTPTR_MAX

#define MIN_TIMEOUT_DIFF_VALUE     (100)
/*
 * Task watchdog channel data
 */
struct task_wdt_channel {
	/* period in milliseconds used to reset the timeout, set to 0 to
	 * indicate that the channel is available
	 */
	uint32_t reload_period;
	/* abs. times when this channel expires (updated by task_wdt_feed) */
	int64_t timeout_abs_time;
	/* user data passed to the callback function */
	void *user_data;
	/* function to be called when watchdog timer expired */
	task_wdt_callback_t callback;
};

/* array of all task watchdog wdt_channels */
static struct task_wdt_channel wdt_channels[TASK_WDT_CAHNNEL_MAX];

/* timer used for watchdog handling */
static struct k_timer timer;

static void schedule_next_timeout(int64_t current_time)
{
	uintptr_t next_channel_id;	/* channel which will time out next */
	int64_t next_timeout;		/* timeout in absolute ticks of this channel */
	int64_t cur_time;

	next_channel_id = 0;
	next_timeout = INT64_MAX;

	/* find minimum timeout of all wdt_channels */
	for (int id = 0; id < ARRAY_SIZE(wdt_channels); id++) {
		if (wdt_channels[id].reload_period != 0 &&
		    wdt_channels[id].timeout_abs_time < next_timeout) {
			next_channel_id = id;
			next_timeout = wdt_channels[id].timeout_abs_time;
		}
	}

	/* update task wdt kernel timer */
	if(next_timeout != INT64_MAX){
		cur_time = k_uptime_get();
		if((next_timeout > cur_time) && \
			(next_timeout - cur_time) > MIN_TIMEOUT_DIFF_VALUE){
			next_timeout = next_timeout - cur_time;
		}else{
			next_timeout = MIN_TIMEOUT_DIFF_VALUE;
		}
		k_timer_user_data_set(&timer, (void *)next_channel_id);
		k_timer_start(&timer, (s32_t)next_timeout, 0);
	}
}

/**
 * @brief Task watchdog timer callback.
 *
 * If the device operates as intended, this function will never be called,
 * as the timer is continuously restarted with the next due timeout in the
 * task_wdt_feed() function.
 *
 * If all task watchdogs have longer timeouts than the hardware watchdog,
 * this function is called regularly (via the background channel). This
 * should be avoided by setting CONFIG_TASK_WDT_MIN_TIMEOUT to the minimum
 * task watchdog timeout used in the application.
 *
 * @param timer_id Pointer to the timer which called the function
 */
static void task_wdt_trigger(struct k_timer *timer_id)
{
	uintptr_t channel_id = (uintptr_t)k_timer_user_data_get(timer_id);

	/* If the timeout expired for a channel that has been deleted,
	 * only schedule a new timeout (the hardware watchdog, if used, will be
	 * fed right after that new timeout is scheduled).
	 */
	if (wdt_channels[channel_id].reload_period == 0) {
		schedule_next_timeout(k_uptime_get_32());
		return;
	}

	if (wdt_channels[channel_id].callback) {
		wdt_channels[channel_id].callback(channel_id,
			wdt_channels[channel_id].user_data);
	} else {
		printk("!!!task watchdog overflow %d period %d ms cur time %d\n", channel_id, wdt_channels[channel_id].reload_period, k_uptime_get_32());
		panic(NULL);
	}
}

int task_wdt_init(const struct device *hw_wdt)
{
	if (hw_wdt) {
		return -ENOTSUP;
	}

	k_timer_init(&timer, task_wdt_trigger, NULL);
	schedule_next_timeout(k_uptime_get_32());

	return 0;
}

int task_wdt_add(uint32_t channel_id, uint32_t reload_period, task_wdt_callback_t callback,
		 void *user_data)
{
	uint32_t key;

	if (reload_period == 0) {
		return -EINVAL;
	}

	if (channel_id >= ARRAY_SIZE(wdt_channels)) {
		return -EINVAL;
	}

	key = irq_lock();

	/* look for unused channel (reload_period set to 0) */
	if (wdt_channels[channel_id].reload_period == 0) {
		wdt_channels[channel_id].reload_period = reload_period;
		wdt_channels[channel_id].user_data = user_data;
		wdt_channels[channel_id].timeout_abs_time = -1;
		wdt_channels[channel_id].callback = callback;

		/* must be called after hw wdt has been started */
		task_wdt_feed(channel_id);

		irq_unlock(key);

		return 0;
	}else{
		printk("[ERR] wdt task channel %d used\n", channel_id);
	}

	irq_unlock(key);

	return -ENOMEM;
}

int task_wdt_delete(uint32_t channel_id)
{
	uint32_t key;

	if (channel_id >= ARRAY_SIZE(wdt_channels)) {
		return -EINVAL;
	}

	key = irq_lock();

	wdt_channels[channel_id].reload_period = 0;

	irq_unlock(key);

	return 0;
}

int task_wdt_feed(uint32_t channel_id)
{
	int64_t current_time;

	if (channel_id >= ARRAY_SIZE(wdt_channels)) {
		return -EINVAL;
	}

	if(wdt_channels[channel_id].reload_period == 0){
		return -EIO;
	}

	/*
	 * We need a critical section instead of a mutex while updating the
	 * wdt_channels array in order to prevent priority inversion. Otherwise,
	 * a low priority thread could be preempted before releasing the mutex
	 * and block a high priority thread that wants to feed its task wdt.
	 */
	k_sched_lock();

	current_time = k_uptime_get();

	/* feed the specified channel */
	wdt_channels[channel_id].timeout_abs_time = current_time +
		wdt_channels[channel_id].reload_period;

	//printk("wdt %d time %u %u\n", channel_id, (uint32_t)current_time, (uint32_t)wdt_channels[channel_id].timeout_abs_time);

	schedule_next_timeout(current_time);

	k_sched_unlock();

	return 0;
}

int task_wdt_feed_all(void)
{
	/* find minimum timeout of all wdt_channels */
	for (int id = 0; id < ARRAY_SIZE(wdt_channels); id++) {
		if(wdt_channels[id].reload_period != 0){
			task_wdt_feed(id);
		}
	}

	return 0;
}

int task_wdt_exit(void)
{
	for (int id = 0; id < ARRAY_SIZE(wdt_channels); id++) {
		if(wdt_channels[id].reload_period != 0){
			task_wdt_delete(id);
		}
	}

	return 0;
}
