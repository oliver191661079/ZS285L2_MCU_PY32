/*
 * Copyright (c) 2019 Actions Semi Co., Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief bt manager letws inner.
 */
#include "list.h"


enum
{
	LETWS_SYNC_NONE = 0X00,
	LETWS_SYNC_VERSION_INFO = 0x01,
	LETWS_SYNC_PHONE_CONN_INFO = 0x02,
};

enum {
	TWS_EXCHANGE_VERSION_NULL = 0,
	TWS_EXCHANGE_VERSION_SENDED = (0x1 << 0),
	TWS_EXCHANGE_VERSION_RECEIVED = (0x1 << 1),
	TWS_EXCHANGE_VERSION_FINISH = (TWS_EXCHANGE_VERSION_SENDED | TWS_EXCHANGE_VERSION_RECEIVED),
};

struct letws_version_feature {
	uint16_t version;
	uint32_t feature;
} __packed;

struct letws_phone_conn_info {
	uint8_t phone_connect : 1;
	uint8_t aux_status : 1;
	uint8_t bd[6];
	uint8_t expect_role;
	uint8_t stream_start;
} __packed;

struct letws_phone_conn_local_info {
	uint8_t vaild;
	struct letws_phone_conn_info info;
};

