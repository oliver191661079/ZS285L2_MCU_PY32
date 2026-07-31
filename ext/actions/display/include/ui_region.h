/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file view manager interface
 */

#ifndef __UI_REGION_H__
#define __UI_REGION_H__

/**
 * @defgroup ui_manager_apis app ui Manager APIs
 * @ingroup system_apis
 * @{
 */

#ifndef UI_ALIGN
#define UI_ALIGN(x, a)		(((x) + (a) - 1) & ~((a) - 1))
#endif

#ifndef UI_ROUND_UP
#define UI_ROUND_UP(x, align)                                   \
	(((unsigned long)(x) + ((unsigned long)(align) - 1)) & \
	 ~((unsigned long)(align) - 1))
#endif

#ifndef UI_ROUND_DOWN
#define UI_ROUND_DOWN(x, align)                                 \
	((unsigned long)(x) & ~((unsigned long)(align) - 1))
#endif

#define UI_MATH_MIN(a, b) ((a) < (b) ? (a) : (b))
#define UI_MATH_MAX(a, b) ((a) > (b) ? (a) : (b))
#define UI_MATH_ABS(x) ((x) > 0 ? (x) : (-(x)))

typedef struct _ui_region_t{
	int16_t x1;
	int16_t y1;
	int16_t x2;
	int16_t y2;
} ui_region_t;

typedef struct _ui_point_t{
	int16_t x;
	int16_t y;
} ui_point_t;

/**
 * Get the width of an area
 * @param area_p pointer to an area
 * @return the width of the area (if x1 == x2 -> width = 1)
 */
static inline int16_t ui_region_get_width(const ui_region_t * region)
{
	return (int16_t)(region->x2 - region->x1 + 1);
}

/**
 * Get the height of an area
 * @param area_p pointer to an area
 * @return the height of the area (if y1 == y2 -> height = 1)
 */
static inline int16_t ui_region_get_height(const ui_region_t * region)
{
	return (int16_t)(region->y2 - region->y1 + 1);
}

/**
 * Set the width of an area
 * @param area_p pointer to an area
 * @param w the new width of the area (w == 1 makes x1 == x2)
 */
void ui_region_set_width(ui_region_t * region, int16_t w);

/**
 * Set the height of an area
 * @param area_p pointer to an area
 * @param h the new height of the area (h == 1 makes y1 == y2)
 */
void ui_region_set_height(ui_region_t * region, int16_t h);

/**
 * Set the position of an area (width and height will be kept)
 * @param area_p pointer to an area
 * @param x the new x coordinate of the area
 * @param y the new y coordinate of the area
 */
void ui_region_set_pos(ui_region_t * area_p, int16_t x, int16_t y);

/**
 * Move the position of an area (width and height will be kept)
 * @param area_p pointer to an area
 * @param dx delata X along the the x coordinate
 * @param dy delata Y along the the y coordinate
 */
void ui_region_move(ui_region_t * area_p, int16_t dx, int16_t dy);

/**
 * Initialize an area
 * @param area_p pointer to an area
 * @param x1 left coordinate of the area
 * @param y1 top coordinate of the area
 * @param x2 right coordinate of the area
 * @param y2 bottom coordinate of the area
 */
void ui_region_set(ui_region_t * region, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);

/**
 * Get the common parts of two areas
 * @param res_p pointer to an area, the result will be stored here
 * @param a1_p pointer to the first area
 * @param a2_p pointer to the second area
 * @return false: the two area has NO common parts, res_p is invalid
 */

bool ui_region_intersect(ui_region_t *dest_region, const ui_region_t *region1, const ui_region_t *region2);

/**
 * merge two areas into a third which involves the other two
 * @param res_p pointer to an area, the result will be stored here
 * @param a1_p pointer to the first area
 * @param a2_p pointer to the second area
 */

void ui_region_merge(ui_region_t *dest_region, const ui_region_t *region1, const ui_region_t *region2);

/**
 * Check if two area has common parts
 * @param a1_p pointer to an area.
 * @param a2_p pointer to an other area
 * @return false: a1_p and a2_p has no common parts
 */

bool ui_region_is_on(const ui_region_t *region1, const ui_region_t *region2);

/**
 * Check if an area is fully on an other
 * @param region1 pointer to an area which could be in 'region2'
 * @param region2 pointer to an area which could involve 'region1'
 * @return
 */

bool ui_region_is_in(const ui_region_t *region1, const ui_region_t *region2);

/**
 * check region is valid
 * @param area_p pointer to an area
 * @return true if region is valid
 */
bool ui_region_is_valid(const ui_region_t * region);

/**
 * @} end defgroup system_apis
 */
#endif
