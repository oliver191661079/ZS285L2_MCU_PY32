/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

 /**
 * @file Ram dump core
 *
 * NOTE: All Ram dump functions cannot be called in interrupt context.
 */

#ifndef _RAMDUMP_CORE__H_
#define _RAMDUMP_CORE__H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* file magic */
enum file_magic {
	MAGIC_RAMD = 0x444d4152, /* RAMD */
	MAGIC_PRTD = 0x444d4152, /* PRTD */
	MAGIC_LZ4 = 0x20345a4c, /* LZ4 */
};


enum RAMD_TARGET_TYPE{
	RAMDUMP_TARGET_TYPE_CK802 = 0x1,
	RAMDUMP_TARGET_TYPE_CORTEX = 0x2,
};

/* ramd header */
typedef struct ramd_head {
	uint32_t magic;     /* magic (file format) */
	uint32_t version;   /* file version */
	uint32_t img_sz;    /* Size of image body (bytes). */
	uint32_t org_sz;    /* Size of origin data (bytes) */
	uint32_t esf_addr;
	uint32_t current_thread;
	uint8_t target_type;
	uint8_t reserved[7];
} ramd_head_t;

/* ramd region */
typedef struct ramd_region {
	uint32_t mem_addr;  /* memory address */
	uint32_t mem_sz;    /* memory size */
	uint32_t img_off;   /* image offset */
	uint32_t img_sz;    /* image size */
} ramd_region_t;

/* segment header */
typedef struct lz4_head {
	uint32_t magic;     /* magic (file format) */
	uint32_t hdr_size;  /* Size of header (bytes) */
	uint32_t img_sz;    /* Size of image body (bytes). */
	uint32_t org_sz;    /* Size of origin data (bytes) */
} lz4_head_t;

typedef struct ramd_item {
	uintptr_t	start;
	uintptr_t	next;
} ramd_item_t;

#define RAMD_BLK_SZ		(32*1024)

extern const ramd_item_t ramdump_mem_regions[];

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_RAM_DUMP */
