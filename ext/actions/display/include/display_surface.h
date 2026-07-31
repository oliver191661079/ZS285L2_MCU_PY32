/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_FRAMEWORK_INCLUDE_DISPLAY_DISPLAY_SURFACE_H_
#define ZEPHYR_FRAMEWORK_INCLUDE_DISPLAY_DISPLAY_SURFACE_H_

#include "graphic_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SURFACE_MAX_BUFFER_COUNT (1)

/**
 * @enum surface_event_id
 * @brief Enumeration with possible surface event
 *
 */
enum surface_event_id {
	SURFACE_EVT_DRAW_BEGIN = 0,
	SURFACE_EVT_DRAW_END,

	SURFACE_EVT_POST_BEGIN,
	SURFACE_EVT_POST_END,
};

/**
 * @enum surface_callback_id
 * @brief Enumeration with possible surface callback
 *
 */
enum surface_callback_id {
	SURFACE_CB_DRAW = 0,
	SURFACE_CB_POST,

	SURFACE_NUM_CBS,
};

/**
 * @brief Enumeration with possible surface flags
 *
 */
enum {
	SURFACE_FIST_DRAW = 0x1, /* first draw in one frame */
	SURFACE_LAST_DRAW = 0x2, /* first draw in one frame */
};

/**
 * @struct surface_draw_data
 * @brief Structure holding surface event data in callback SURFACE_CB_DRAW
 *
 */
typedef struct surface_draw_data {
	uint8_t flags;
	const ui_region_t *area;
} surface_draw_data_t;

/**
 * @typedef surface_callback_t
 * @brief Callback API executed when surface changed
 *
 */
typedef void (*surface_callback_t)(uint32_t event, const void *data, void *user_data);

/**
 * @struct surface
 * @brief Structure holding surface
 *
 */
typedef struct surface {
	graphic_buffer_t *buffers[SURFACE_MAX_BUFFER_COUNT];
	uint8_t buf_count;

	surface_callback_t callback[SURFACE_NUM_CBS];
	void *user_data[SURFACE_NUM_CBS];
} surface_t;

/**
 * @brief Create surface
 *
 * @param w width in pixels
 * @param h height in pixels
 * @param pixel_format display pixel format, see enum display_pixel_format
 * @param buf_count number of buffers to allocate
 *
 * @return pointer to surface structure; NULL if failed.
 */
surface_t *surface_create(uint16_t w, uint16_t h,
		uint32_t pixel_format, uint8_t buf_count);

/**
 * @brief Destroy surface
 *
 * @param surface pointer to surface structure
 *
 * @retval N/A.
 */
void surface_destroy(surface_t *surface);

/**
 * @brief surface begin to draw the current frame
 *
 * @param surface pointer to surface structure
 * @param area vdb area
 * @param flags draw flags
 *
 * @return N/A.
 */
void surface_begin_draw(surface_t *surface, const ui_region_t *area, uint8_t flags);

/**
 * @brief surface end to draw the current frame
 *
 * @param surface pointer to surface structure
 * @param area vdb area
 * @param flags draw flags
 *
 * @return N/A.
 */
void surface_end_draw(surface_t *surface, const ui_region_t *area, uint8_t flags);

/**
 * @brief surface has posted one draw area
 *
 * @param surface pointer to surface structure
 *
 * @return N/A.
 */
void surface_complete_one_post(surface_t *surface);

/**
 * @brief Register surface callback
 *
 * @param surface pointer to surface structure
 * @param callback event callback
 * @param user_data user data passed in callback function
 *
 * @return N/A.
 */
static inline void surface_register_callback(surface_t *surface,
		int callback_id, surface_callback_t callback_fn, void *user_data)
{
	surface->callback[callback_id] = callback_fn;
	surface->user_data[callback_id] = user_data;
}

/**
 * @brief Get allocated buffer count of surface
 *
 * @param surface pointer to surface structure
 *
 * @return buffer count of surface.
 */
static inline uint8_t surface_get_buffer_count(surface_t *surface)
{
	return surface->buf_count;
}

/**
 * @brief get front buffer of surface
 *
 * @param surface pointer to surface structure
 *
 * @return pointer of graphic buffer structure; NULL if failed.
 */
static inline graphic_buffer_t *surface_get_backbuffer(surface_t *surface)
{
	return surface->buffers[0];
}

/**
 * @brief Get back buffer of surface
 *
 * @param surface pointer to surface structure
 *
 * @return pointer of graphic buffer structure; NULL if failed.
 */
static inline graphic_buffer_t *surface_get_frontbuffer(surface_t *surface)
{
	return surface->buffers[surface->buf_count - 1];
}

/**
 * @brief Swap back/front buffers of surface
 *
 * @param surface pointer to surface structure
 *
 * @return N/A.
 */
static inline void surface_swap_buffers(surface_t *surface)
{
#if SURFACE_MAX_BUFFER_COUNT > 1
	if (surface->buf_count > 1) {
		graphic_buffer_t *tmp = surface->buffers[0];

		surface->buffers[0] = surface->buffers[1];
		surface->buffers[1] = tmp;
	}
#endif
}

/**
 * @brief Get pixel format of surface
 *
 * @param surface pointer to surface structure
 *
 * @return pixel format.
 */
static inline uint32_t surface_get_pixel_format(surface_t *surface)
{
	return graphic_buffer_get_pixel_format(surface->buffers[0]);
}

/**
 * @brief Get width of surface
 *
 * @param surface pointer to surface structure
 *
 * @return width in pixels.
 */
static inline uint16_t surface_get_width(surface_t *surface)
{
	return graphic_buffer_get_width(surface->buffers[0]);
}

/**
 * @brief Get height of surface
 *
 * @param surface pointer to surface structure
 *
 * @return height in pixels.
 */
static inline uint16_t surface_get_height(surface_t *surface)
{
	return graphic_buffer_get_height(surface->buffers[0]);
}

/**
 * @brief Get stride of surface
 *
 * @param surface pointer to surface structure
 *
 * @return stride in pixels.
 */
static inline uint32_t surface_get_stride(surface_t *surface)
{
	return graphic_buffer_get_stride(surface->buffers[0]);
}

#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_FRAMEWORK_INCLUDE_DISPLAY_DISPLAY_SURFACE_H_ */
