/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_LIB_GUI_LVGL_LVGL_VIRTUAL_DISPLAY_H
#define ZEPHYR_LIB_GUI_LVGL_LVGL_VIRTUAL_DISPLAY_H

#include <display_surface.h>
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a lvgl virtual display
 *
 * The requested pixel format must have the same color depth as LVGL_COLOR_DEPTH,
 * so no color transform is required between lvgl and the display surface.
 *
 * @param surface pointer to the display surface structure
 *
 * @return pointer to the created display; NULL if failed.
 */
lv_disp_t *lvgl_virtual_display_create(surface_t *surface);

/**
 * @brief Destroy a lvgl virtual display
 *
 * @param disp pointer to a lvgl display
 *
 * @return N/A
 */
void lvgl_virtual_display_destroy(lv_disp_t *disp);

/**
 * @brief Focus on a lvgl virtual display
 *
 * @param disp pointer to a lvgl display
 *
 * @retval 0 on success else negative errno code.
 */
int lvgl_virtual_display_set_focus(lv_disp_t *disp);

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_LIB_GUI_LVGL_LVGL_VIRTUAL_DISPLAY_H */
