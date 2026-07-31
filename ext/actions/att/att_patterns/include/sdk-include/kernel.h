/*
 * Copyright (c) 2016, Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 *
 * @brief Public kernel APIs.
 */

#ifndef _kernel__h_
#define _kernel__h_

typedef sys_dlist_t _wait_q_t;

struct k_sem {
	_wait_q_t wait_q;
	unsigned int count;
	unsigned int limit;
};

struct device;

/**
 * @brief Static device information (In ROM) Per driver instance
 *
 * @param name name of the device
 * @param init init function for the driver
 * @param config_info address of driver instance config information
 */
struct device_config {
	char	*name;
	int (*init)(struct device *device);
	const void *config_info;
};

/**
 * @brief Runtime device structure (In memory) Per driver instance
 * @param device_config Build time config information
 * @param driver_api pointer to structure containing the API functions for
 * the device type. This pointer is filled in by the driver at init time.
 * @param driver_data driver instance data. For driver use only
 */
struct device {
	const struct device_config *config;
	const void *driver_api;
	void *driver_data;
};

#ifndef __printf_like
#define __printf_like(f, a)   __attribute__((format (printf, f, a)))
#endif

extern __printf_like(1, 2) int printk(const char *fmt, ...);

extern __printf_like(1, 2) int print_log(const char *fmt, ...);

extern u32_t k_uptime_get_32(void);

void k_sem_init(struct k_sem *sem, unsigned int initial_count,
		unsigned int limit);

void k_sem_give(struct k_sem *sem);

int k_sem_take(struct k_sem *sem, s32_t timeout);

void *z_malloc(size_t size);

void free(void *ptr);

void* memset(void * s, int c, unsigned int count);

void* memcpy(void *dest, const void *src, unsigned int count);

void k_sleep(s32_t duration);

void *k_thread_create(void *new_thread,
			void *stack,
			u32_t stack_size, void (*entry)(void *, void *, void*),
			void *p1, void *p2, void *p3,
			int prio, u32_t options, s32_t delay);

#endif /* _kernel__h_ */
