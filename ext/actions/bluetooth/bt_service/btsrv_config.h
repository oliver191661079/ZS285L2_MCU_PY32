/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file media service config interface
 */
#ifndef __BTSRV_CONFIG_H__
#define __BTSRV_CONFIG_H__

#define CONFIG_MAX_BT_ISO_RX_LEN            200
#define CONFIG_MAX_BT_ISO_TX_LEN            200
#if 1	// TODO: not used
#define CONFIG_MAX_BT_LE_BROAD_EXT_ADV_LEN  48
#endif
#define CONFIG_MAX_BT_LE_BROAD_PER_ADV_LEN  160

#ifdef CONFIG_TWS
#define CONFIG_SUPPORT_TWS          1
#endif

#define CONFIG_A2DP_PACK_DATE_HEADER 1

//#define CONFIG_USE_SHARE_TWS_MAC    1
//#define CONFIG_SNOOP_SYNC_LINK      1

//#define CONFIG_SNOOP_LINK_TWS 1

#define CONFIG_MAX_A2DP_ENDPOINT	3

#define CONFIG_MAX_SPP_CHANNEL		4

#define CONFIG_MAX_PBAP_CONNECT		2

#define CONFIG_MAX_MAP_CONNECT		2

#define CONFIG_DEBUG_DATA_RATE      1

#define CONFIG_BT_EARPHONE_SPEC     1

/* Just for use shell debug bt stack, not start bt service */
#define CONFIG_DEBUG_BT_STACK		0
#endif
