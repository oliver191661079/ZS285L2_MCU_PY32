/*
 * Copyright (c) 2020 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Public API for RTC(Real Time Clock) Drivers
 */

#ifndef _RTC_H_
#define _RTC_H_
#include <zephyr/types.h>
#include <device.h>
#include <misc/util.h>

#ifdef __cplusplus
extern "C" {
#endif

/*!
 * struct rtc_time
 * @brief The structure to discribe TIME
 */
struct rtc_time {
	u8_t tm_sec;
	u8_t tm_min;
	u8_t tm_hour;
	u8_t tm_mday;
	u8_t tm_mon;
	u8_t tm_wday;
	u16_t tm_year;
};

/* @brief The function to translate seconds time (UTC) to rtc time. */
void rtc_time_to_tm(uint32_t time, struct rtc_time *tm);
/* @brief Check the rtc time is valid or not. */
int rtc_valid_tm(struct rtc_time *tm);
/* @brief The function to translate the rtc time to seconds time (UTC). */
int rtc_tm_to_time(struct rtc_time *tm, uint32_t *time);
/* @brief The function to print the rtc time. */
void print_rtc_time(struct rtc_time *tm);
/* @brief The number of days in the month.*/
int rtc_month_days(unsigned int month, unsigned int year);

/*!
 * struct rtc_alarm_config
 * @brief The structure to configure the alarm function.
 */
struct rtc_alarm_config {
	struct rtc_time alarm_time;         /*!< The alarm time setting */
	void (*cb_fn)(const void *cb_data);	/*!< Pointer to function to call when alarm value matches current RTC value */
	const void *cb_data;                /*!< The callback data */
};

/*!
 * struct rtc_alarm_config
 * @brief The current alarm status
 */
struct rtc_alarm_status {
	struct rtc_time alarm_time;         /*!< The alarm time setting */
	bool is_on;                         /*!< Alarm on status */
};

struct rtc_driver_api {
	void (*enable)(struct device *dev);
	void (*disable)(struct device *dev);
	int (*get_time)(struct device *dev, struct rtc_time *tm);
	int (*set_time)(struct device *dev, const struct rtc_time *tm);
	int (*set_alarm)(struct device *dev, struct rtc_alarm_config *config, bool enable);
	int (*get_alarm)(struct device *dev, struct rtc_alarm_status *sts);
	bool (*is_alarm_wakeup)(struct device *dev);
};

/**
 * @brief Enable the RTC module
 *
 * @param dev: Pointer to the device structure for the driver instance.
 *
 *  @return  N/A
 */
static inline void rtc_enable(struct device *dev)
{
	const struct rtc_driver_api *api = dev->driver_api;

	api->enable(dev);
}

/**
 * @brief Disable the RTC module
 *
 * @param dev: Pointer to the device structure for the driver instance.
 *
 *  @return  N/A
 */
static inline void rtc_disable(struct device *dev)
{
	const struct rtc_driver_api *api = dev->driver_api;

	api->disable(dev);
}

/**
 * @brief Get the RTC time
 *
 * @param dev: Pointer to the device structure for the driver instance.
 * @param tm: Pointer to rtc time structure
 *
 *  @return  0 on success, negative errno code on fail.
 */
static inline int rtc_get_time(struct device *dev, struct rtc_time *tm)
{
	const struct rtc_driver_api *api = dev->driver_api;

	return api->get_time(dev, tm);
}

/**
 * @brief Set the RTC time
 *
 * @param dev: Pointer to the device structure for the driver instance.
 * @param tm: Pointer to rtc time structure
 *
 *  @return  0 on success, negative errno code on fail.
 */
static inline int rtc_set_time(struct device *dev, const struct rtc_time *tm)
{
	const struct rtc_driver_api *api = dev->driver_api;

	return api->set_time(dev, tm);
}

/**
 * @brief Set the alarm time
 *
 * @param dev: Pointer to the device structure for the driver instance.
 * @param tm: Pointer to rtc alarm configuration
 *
 *  @return  0 on success, negative errno code on fail.
 */
static inline int rtc_set_alarm(struct device *dev, struct rtc_alarm_config *config, bool enable)
{
	const struct rtc_driver_api *api = dev->driver_api;

	return api->set_alarm(dev, config, enable);
}

/**
 * @brief Get the alarm time setting
 *
 * @param dev: Pointer to the device structure for the driver instance.
 * @param tm: Pointer to rtc time structure
 *
 *  @return  0 on success, negative errno code on fail.
 */
static inline int rtc_get_alarm(struct device *dev, struct rtc_alarm_status *sts)
{
	const struct rtc_driver_api *api = dev->driver_api;

	return api->get_alarm(dev, sts);
}

/**
 * @brief Function to get the information that whether wakeup from RTC
 *
 * Moreover, user can distinguish the ALARM wake up event from this API.
 *
 * @param dev Pointer to the device structure for the driver instance.
 *
 * @retval 1 if the rtc interrupt is pending.
 * @retval 0 if no rtc interrupt is pending.
 */
static inline bool rtc_is_alarm_wakeup(struct device *dev)
{
	struct rtc_driver_api *api = (struct rtc_driver_api *)dev->driver_api;
	return api->is_alarm_wakeup(dev);
}

#ifdef __cplusplus
}
#endif

#endif
