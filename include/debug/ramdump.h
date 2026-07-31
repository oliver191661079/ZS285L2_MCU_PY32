/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

 /**
 * @file Ram dump interface
 *
 * NOTE: All Ram dump functions cannot be called in interrupt context.
 */

#ifndef _RAMDUMP__H_
#define _RAMDUMP__H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Save ramdump.
 *
 * This routine saves ramdump to flash.
 *
 * @param N/A
 *
 * @return 0 if success, otherwise return false
 */
extern int ramdump_save(void);

/**
 * @brief Dump ramdump.
 *
 * This routine stops a running thread watchdog prematurely.
 *
 * @param N/A
 *
 * @return 0 if success, otherwise return false
 */
extern int ramdump_get(int *part_offset, int *part_size);

/**
 * @brief Dump ramdump.
 *
 * This routine dump ramdump region from flash.
 *
 * @param N/A
 *
 * @return 0 if success, otherwise return false
 */
extern int ramdump_dump(void);

/**
 * @brief check ramdump.
 *
 * This routine check ramdump region from flash.
 *
 * @param N/A
 *
 * @return 0 if success, otherwise return false
 */
extern int ramdump_check(void);

/**
 * @brief clear ramdump.
 *
 * This routine clear ramdump region from flash.
 *
 * @param N/A
 *
 * @return 0 if success, otherwise return false
 */
extern int ramdump_clear(void);

/**
 * @brief traverse ramdump data.
 *
 * This routine traverse ramdump data from flash.
 *
 * @param N/A
 *
 * @return traverse data length
 */
extern int ramdump_transfer(char *print_buf, int print_sz, int (*traverse_cb)(uint8_t *data, uint32_t max_len));

/**
 * @brief print ramdump data.
 *
 * This routine print ramdump data from flash, cannot be called when player playing
 *
 * @param N/A
 *
 * @return print data length
 */
extern int ramdump_print(void);

/**
 * @brief return ramdump data size.
 *
 * This routine return ramdump data size from flash.
 *
 * @param N/A
 *
 * @return ramdump data length
 */
extern int ramdump_get_used_size(void);


#ifdef __cplusplus
}
#endif

#endif /* CONFIG_DEBUG_RAMDUMP */
