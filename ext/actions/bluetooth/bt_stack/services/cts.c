/** @file
 *  @brief CTS Service sample
 */

/*
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <init.h>
#include <stdbool.h>
#include <zephyr/types.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <misc/printk.h>
#include <misc/byteorder.h>
#include <misc/__assert.h>
#include <zephyr.h>

#include <acts_bluetooth/bluetooth.h>
#include <acts_bluetooth/conn.h>
#include <acts_bluetooth/uuid.h>
#include <acts_bluetooth/gatt.h>
#include <acts_bluetooth/services/cts.h>

#include <../include/acts_bluetooth/services/date_time.h>

#define CT_MANUAL_TIME_UPDATE    (1 << 0)
#define CT_EXTERNAL_REFERENCE_TIME_UPDATE    (1 << 1)
#define CT_CHANGE_OF_TIME_ZONE    (1 << 2)                 
#define CT_CHANGE_OF_DST    (1 << 3)

#define DTS_OFFSET_STANDARD_TIME     0
#define DTS_OFFSET_HALF_AN_HOUR_DAYLIGHT_TIME    2     
#define DTS_OFFSET_DAYLIGHT_TIME    4 
#define DTS_OFFSET_DOUBLE_DAYLIGHT_TIME    8
#define DTS_OFFSET_UNKNOW    255

#define TIME_SOURCE_UNKNOWN    0
#define TIME_SOURCE_NETWORK_TIME_PROTOCOL    1
#define TIME_SOURCE_GPS    2
#define TIME_SOURCE_RADIO_TIME_SIGNAL    3
#define TIME_SOURCE_MANUAL    4
#define TIME_SOURCE_ATOMIC_CLOCK    5
#define TIME_SOURCE_CELLULAR_NETWORK    6
//#define TIME_SOURCE_RESERVED_FOR_FUTURE_USE    7~255

struct day_date_time {
	struct date_time date_time;
	u8_t day_of_week;  /* 1->monday, 7->sunday, 0->unknow  */
} __packed;

struct exact_time_256  {
	struct day_date_time  day_date_time;
	u8_t fractions256;
} __packed;

struct cts_current_time {
	struct exact_time_256  exact_time_256;
	u8_t adjust_reason;
} __packed;

struct cts_local_time_information {
	s8_t  time_zone;  // utc+8  112.5 degree ~ 127.5 degree
	u8_t daylight_saving_time;
} __packed;

struct cts_reference_time_information {
	u8_t source;
	u8_t accuracy;  /* 0~253, 254->accuracy out of range, 255->accuracy unknown	 */
	u8_t days_since_update; /*0~254,  255-> 255 or more days */
	u8_t hours_since_update; /* 0~23, 255-> 255 or more hours */
} __packed;

struct bt_gatt_cts {
	struct cts_current_time ct;
	u8_t ct_ccc;
	struct cts_local_time_information lti;
	struct cts_reference_time_information rti;
 } __packed;

static struct bt_gatt_cts gatt_cts;

static void cts_ct_ccc_cfg_changed(const struct bt_gatt_attr *attr, u16_t value)
{
	gatt_cts.ct_ccc = (value == BT_GATT_CCC_NOTIFY) ? 1 : 0;	
}

static ssize_t cts_ct_read(struct bt_conn *conn, const struct bt_gatt_attr *attr,
		       void *buf, u16_t len, u16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, &gatt_cts.ct,
				 sizeof(struct cts_current_time));
}

static ssize_t cts_rti_read(struct bt_conn *conn, const struct bt_gatt_attr *attr,
		       void *buf, u16_t len, u16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, &gatt_cts.rti,
				 sizeof(gatt_cts.rti));
}

static ssize_t cts_lti_read(struct bt_conn *conn, const struct bt_gatt_attr *attr,
		       void *buf, u16_t len, u16_t offset)
{
	return bt_gatt_attr_read(conn, attr, buf, len, offset, &gatt_cts.lti,
				 sizeof(gatt_cts.lti));
}

/* Current Time Service Declaration */
static struct bt_gatt_attr cts_attrs[] = {
	BT_GATT_PRIMARY_SERVICE(BT_UUID_CTS),
	BT_GATT_CHARACTERISTIC(BT_UUID_CTS_CURRENT_TIME, 
	               BT_GATT_CHRC_READ |BT_GATT_CHRC_NOTIFY,
                   BT_GATT_PERM_READ, cts_ct_read, NULL,NULL),
	BT_GATT_CCC(cts_ct_ccc_cfg_changed,
		    BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),	
	
	BT_GATT_CHARACTERISTIC(BT_UUID_CTS_LOCAL_TIMR_INFO, 
	               BT_GATT_CHRC_READ,
	               BT_GATT_PERM_READ,cts_lti_read, NULL, NULL),

	BT_GATT_CHARACTERISTIC(BT_UUID_CTS_REFERENCE_TIME_INFO,
	               BT_GATT_CHRC_READ,
                   BT_GATT_PERM_READ,cts_rti_read, NULL, NULL),
};

static struct bt_gatt_service cts_svc = BT_GATT_SERVICE(cts_attrs);

static void set_current_time(void)
{
	/* year */
	gatt_cts.ct.exact_time_256.day_date_time.date_time.year = 2018;
	
	/* months */
	gatt_cts.ct.exact_time_256.day_date_time.date_time.month = 4;
	
	/* day */
	gatt_cts.ct.exact_time_256.day_date_time.date_time.day = 1;
	
	/* hours */
	gatt_cts.ct.exact_time_256.day_date_time.date_time.hours = 19;

	/* minutes */
	gatt_cts.ct.exact_time_256.day_date_time.date_time.minutes = 20;
	
	/* seconds */
	gatt_cts.ct.exact_time_256.day_date_time.date_time.seconds = 15;
	
	/* 'Day of Week' part of 'Day Date Time' */
	gatt_cts.ct.exact_time_256.day_date_time.day_of_week = 2; 

	/* 'Fractions 256 part of 'Exact Time 256' */
	gatt_cts.ct.exact_time_256.fractions256 = 0;

	/* Adjust reason */
	gatt_cts.ct.adjust_reason = 0;
}

static int cts_init(struct device *dev)
{
	ARG_UNUSED(dev);

	memset(&gatt_cts, 0 , sizeof(struct bt_gatt_cts));
	gatt_cts.lti.time_zone = 8 * 4; //15min *4 = 1h
	gatt_cts.lti.daylight_saving_time = DTS_OFFSET_STANDARD_TIME;
	gatt_cts.rti.source = TIME_SOURCE_NETWORK_TIME_PROTOCOL;
	gatt_cts.rti.accuracy = 8;  // 1 s
	gatt_cts.rti.days_since_update = 0;
	gatt_cts.rti.hours_since_update = 0;
	
	set_current_time();

    return 0;
}

void bt_cts_set_current_time_year(uint16_t value)
{
    gatt_cts.ct.exact_time_256.day_date_time.date_time.year = value;
	gatt_cts.ct.adjust_reason = CT_MANUAL_TIME_UPDATE;    
}

void bt_cts_set_current_time_month(uint8_t value)
{
    gatt_cts.ct.exact_time_256.day_date_time.date_time.month = value;   
	gatt_cts.ct.adjust_reason = CT_MANUAL_TIME_UPDATE;
}

void bt_cts_set_current_time_day(uint8_t value)
{
    gatt_cts.ct.exact_time_256.day_date_time.date_time.day= value;   
	gatt_cts.ct.adjust_reason = CT_MANUAL_TIME_UPDATE;
}

void bt_cts_set_current_time_hours(uint8_t value)
{
    gatt_cts.ct.exact_time_256.day_date_time.date_time.hours= value;   
	gatt_cts.ct.adjust_reason = CT_MANUAL_TIME_UPDATE;
}

void bt_cts_set_current_time_minutes(uint8_t value)
{
    gatt_cts.ct.exact_time_256.day_date_time.date_time.minutes= value;   
	gatt_cts.ct.adjust_reason = CT_MANUAL_TIME_UPDATE;
}

void bt_cts_set_current_time_seconds(uint8_t value)
{
    gatt_cts.ct.exact_time_256.day_date_time.date_time.seconds= value;   
	gatt_cts.ct.adjust_reason = CT_MANUAL_TIME_UPDATE;
}

void bt_cts_set_current_time_week(uint8_t value)
{
    gatt_cts.ct.exact_time_256.day_date_time.day_of_week = value;
	gatt_cts.ct.adjust_reason = CT_MANUAL_TIME_UPDATE;
}

void bt_cts_notify(void)
{
	if (!gatt_cts.ct_ccc || !gatt_cts.ct.adjust_reason) {
		return;
	}
	if (!bt_gatt_notify(NULL, &cts_attrs[2], &gatt_cts.ct, sizeof(struct cts_current_time))) {
		gatt_cts.ct.adjust_reason = 0;
	}
}

int bt_register_cts_svc(void)
{
	return bt_gatt_service_register(&cts_svc);
}

SYS_INIT(cts_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
