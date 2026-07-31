/*
 * Copyright (c) 2018 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file gui api interface 
 */
#ifndef __GUI_API_H__
#define __GUI_API_H__

int GUI_UTF8_to_Unicode(u8_t* utf8, int len, u16_t** unicode);
int GUI_GBK_to_Unicode(u8_t* gbk, int len, u16_t** unicode);
int GUI_MBCS_to_Unicode(u8_t* mbcs, int len, u16_t** unicode);
int GUI_Unicode_to_UTF8(u16_t* unicode, int len, u8_t** utf8);
int GUI_Unicode_to_MBCS(u16_t* unicode, int len, u8_t** mbcs);
int GUI_UTF8_to_MBCS(u8_t* utf8, int len, u8_t** mbcs);
int GUI_MBCS_to_UTF8(u8_t* mbcs, int len, u8_t** utf8);

#endif