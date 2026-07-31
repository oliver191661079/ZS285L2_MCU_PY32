/*
 * Copyright (c) 2017 Actions Semi Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.

 * Author: wh<wanghui@actions-semi.com>
 *
 * Change log:
 *	2018/8/10: Created by wh.
 */

#ifndef __ACT_DISPLAY_H__
#define __ACT_DISPLAY_H__

#define MAIN_DISPLAY 0
#define EXT_DISPLAY  1

typedef enum {
	HW_FUNC_DRAW_PIXEL,
	HW_FUNC_DRAW_LINE,
	HW_FUNC_DRAW_FRAME,
	HW_FUNC_FILL_FRAME,
	HW_FUNC_DISPLAY_ON,
	HW_FUNC_DISPLAY_OFF,
	HW_FUNC_DISPLAY_REGIST_CS_FUNC,
	HW_FUNC_DRAW_CHAR,

	NUMBER_OF_HW_FUNC,
} HW_FUNC_ID;

typedef enum {
	HW_FUNC_REGISTERED = (1<<0),
	HW_FUNC_ENABLED = (1<<1),
} HW_FUNC_STATE;

typedef enum {
	DISP_LED,
	DISP_SEG_LED,
	DISP_LCD,
} DISPLAY_TYPE;

typedef enum {
	COLOR_1_BIT,
	COLOR_8_BIT,
	COLOR_16_BIT,
	COLOR_24_BIT,
} COLOR_FORMAT;

typedef struct {
  void* func;
  u8_t state;
} display_hw_function;

struct display_info {
	u8_t display_type;
	u8_t color_format;
	u16_t rotation;
	u16_t width;
	u16_t height;
};

struct display_device {
	u8_t display_type;
	u8_t color_format;
	u16_t width;
	u16_t height;
	display_hw_function hw_func[NUMBER_OF_HW_FUNC];
};

struct display_device* display_device_register(const struct display_info* info);
void display_hw_function_register(struct display_device* device, u8_t func_id, void* driver);
void display_hw_function_enable(struct display_device* device, u8_t func_id);
void display_hw_function_disable(struct display_device* device, u8_t func_id);

#endif
