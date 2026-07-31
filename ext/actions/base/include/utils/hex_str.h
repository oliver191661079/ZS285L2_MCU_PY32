/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file hex to string interface
 */
#ifndef __HEX_STR_H__
#define __HEX_STR_H__

#include <stdint.h>
#include <stddef.h>
#include <errno.h>

size_t hex_to_str(char *pszDest, char *pbSrc, int nLen);

void str_to_hex(char *pbDest, char *pszSrc, int nLen);

int char2hex(char c, uint8_t *x);

int hex2char(uint8_t x, char *c);

uint8_t u8_to_dec(char *buf, uint8_t buflen, uint8_t value);
#endif