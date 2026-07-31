/*
 * Copyright (c) 2019 Actions Semiconductor Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file view manager interface
 */

#ifndef __VIEW_MANGER_H__
#define __VIEW_MANGER_H__
#include <ui_region.h>
/**
 * @defgroup ui_manager_apis app ui Manager APIs
 * @ingroup system_apis
 * @{
 */

#define TRANSMIT_ALL_KEY_EVENT  0xFFFFFFFF

#define X_ALIGN_SIZE 2 

typedef int (*ui_view_proc_t)(uint8_t view_id, uint8_t msg_id, uint32_t msg_data);

typedef int (*ui_get_state_t)(void);

typedef int (*ui_focus_cb_t)(uint32_t view_id, bool focus);

enum UI_VIEW_TYPE {
	UI_VIEW_Unknown = 0,
	UI_VIEW_UGUI,
	UI_VIEW_LVGL,
};

enum UI_VIEW_FLAGS {
	UI_FLAG_SET_FOCUS    = (1 << 0),
	UI_FLAG_HOLD_DISPLAY = (1 << 1),
	UI_FLAG_TOP_MOST     = (1 << 2),
	UI_FLAG_HIDE    	 = (1 << 3),
	UI_FLAG_DEFAULT_HIDE = (1 << 4),

	UI_FLAG_CONTENT_DIRTY = (1 << 5),
};

enum UI_VIEW_HIDDEN_ATTRIBUTE {
	HIDDEN_ATTRIBUTE_DROPDOWN = (1 << 0),
	HIDDEN_ATTRIBUTE_DROPUP = (1 << 1),
	HIDDEN_ATTRIBUTE_DROPLEFT = (1 << 2),
	HIDDEN_ATTRIBUTE_DROPRIGHT = (1 << 3),
	HIDDEN_ATTRIBUTE_MOVEDOWN = (1 << 4),
	HIDDEN_ATTRIBUTE_MOVEUP = (1 << 5),
	HIDDEN_ATTRIBUTE_MOVELEFT = (1 << 6),
	HIDDEN_ATTRIBUTE_MOVERIGHT = (1 << 7),
};

typedef struct {
    /** key value, which key is pressed */
	uint32_t key_val;

    /** key type, which key type of the pressed key */
	uint32_t key_type;

    /** app state, the state of app service to handle the message */
	uint32_t app_state;

    /** app msg, the message of app service will be send */
	uint32_t app_msg;

    /** key policy */
	uint32_t key_policy;

} ui_key_map_t;

typedef struct view_data {
	/* gui data */
	//void *context; /* gui context */
	void *display; /* gui display */
	void *surface; /* gui surface to draw on */

	/* custom data */
	void *presenter; /* view presenter passed by app */
	void *user_data; /* application defined data */
} view_data_t;

/* app entry structure */
typedef struct view_entry {
	/** app id */
	const char *app_id;
	/** proc function of view */
	ui_view_proc_t  proc;
	/** get state function of view  */
	ui_get_state_t  get_state;
	/**key map of view */
	const ui_key_map_t	*key_map;
	/**view id */
	uint8_t id;
	/**default order */
	uint8_t default_order;
	/**view type */
	uint8_t type;
	/** view width */
	uint16_t width;
	/** view height */
	uint16_t height;
} view_entry_t;

/**
 * @brief Statically define and initialize view entry for view.
 *
 * The view entry define statically,
 *
 * Each view must define the view entry info so that the system wants
 * to find view to knoe the corresponding information
 *
 * @param app_id Name of the app.
 * @param view_proc view proc function.
 * @param view_get_state view get state function.
 * @param view_key_map key map of view .
 * @param view_id view id of view .
 * @param default_order default order of view .
 * @param view_w view width .
 * @param view_h view height.
 */

#define VIEW_DEFINE(app_name, view_proc, view_get_state, view_key_map,\
		view_id, order, view_type, view_w, view_h)	\
	const struct view_entry __view_entry_##app_name	\
		__attribute__((__section__(".view_entry")))= {	\
		.app_id = #app_name,							\
		.proc = view_proc,								\
		.get_state = view_get_state,					\
		.key_map = view_key_map,		    			\
		.id = view_id,									\
		.default_order = order,							\
		.type = view_type,								\
		.width = view_w,								\
		.height = view_h,								\
	}

typedef struct {
	sys_snode_t  node;
	const view_entry_t *entry;
	ui_region_t     surface_frame;
	uint8_t   id;
	uint8_t  hidden_attribute;
	uint16_t  flags;
	uint16_t  order;

	view_data_t data;
} ui_view_context_t;


enum UI_VIEW_ANIMATION_STATE {
	ANIMATION_NONE,
	ANIMATION_START,
	ANIMATION_RUNNING,
	ANIMATION_STOP,
};

enum UI_VIEW_ANIMATION_TYPE {
	ANIMATION_SLIDE_IN_DOWN = 1,
	ANIMATION_SLIDE_IN_UP,
	ANIMATION_SLIDE_IN_LEFT,
	ANIMATION_SLIDE_IN_RIGHT,
	ANIMATION_SLIDE_OUT_DOWN,
	ANIMATION_SLIDE_OUT_UP,
	ANIMATION_SLIDE_OUT_LEFT,
	ANIMATION_SLIDE_OUT_RIGHT,
};

typedef struct {
	uint16_t view_id;
	uint16_t last_view_id;
	uint8_t animation_state;
	uint8_t animation_type;
	int16_t animation_step;
	ui_point_t animation_start;
	ui_point_t animation_stop;
} ui_view_animation_t;


typedef struct {
	sys_slist_t  view_list;
	ui_region_t  update_region;
} ui_manager_context_t;

enum UI_MSG_ID {
	MSG_VIEW_CREATE,
	MSG_VIEW_DELETE,
	MSG_VIEW_SHOW,
	MSG_VIEW_HIDE,
	MSG_VIEW_PAINT,
	MSG_VIEW_SETORDER,
	MSG_VIEW_SET_POSITION,
	MSG_VIEW_SET_HIDDEN_ATTRIBUTE,
	MSG_VIEW_SET_FOCUS,
	MSG_VIEW_REFRESH,
	MSG_POST_DISPLAY,
	MSG_VIEW_DRAW,
	MSG_VIEW_SET_FOCUS_CB,
};

/**
 * @brief get view data
 *
 * This routine get view data.
 *
 * @param view_id id of view
 *
 * @return view data on success else NULL
 */
view_data_t *view_get_data(uint32_t view_id);

/**
 * @brief get view display
 *
 * This routine get view display.
 *
 * @param data view data
 *
 * @return view display on success else NULL
 */
static inline void *view_get_display(view_data_t *data)
{
	return data->display;
}

/**
 * @brief get view surface
 *
 * This routine get view gui surface.
 *
 * @param data view data
 *
 * @return view surface on success else NULL
 */
static inline void *view_get_surface(view_data_t *data)
{
	return data->surface;
}

/**
 * @brief get view data
 *
 * This routine get view data.
 *
 * @param view_id id of view
 *
 * @return view data on success else NULL
 */
static inline void *view_get_presenter(view_data_t *data)
{
	return data->presenter;
}

/**
 * @brief Create a new view
 *
 * This routine Create a new view
 *
 * @param view_id init view id
 * @param presenter view presenter
 *
 * @return true if invoked succsess.
 * @return false if invoked failed.
 */
bool view_create(uint32_t view_id, void *presenter);

/**
 * @brief destory view
 *
 * This routine destory view, delete form ui manager view list
 * and release all resource for view.
 *
 * @param view_id id of view
 *
 * @return true if invoked succsess.
 * @return false if invoked failed.
 */
bool view_delete(uint32_t view_id);

/**
 * @brief set view order
 *
 * This routine set view order, The higher the order value of the view,
 *  the higher the view
 *
 * @param view_id id of view
 * @param order of view
 *
 * @return true if invoked succsess.
 * @return false if invoked failed.
 */
bool view_set_order(uint32_t view_id, uint32_t order);

bool view_set_hidden_attribute(uint32_t view_id, int16_t drop_attribute);

int16_t view_get_hidden_attribute(uint32_t view_id);

bool view_has_moved_attribute(uint32_t view_id);

bool view_set_position(uint32_t view_id, int16_t x, int16_t y);

bool view_set_region(uint32_t view_id, const ui_region_t *dirty_region, uint32_t flags);

void view_aniation_start(ui_view_animation_t *animation,
					uint32_t view_id, uint32_t last_view_id, uint16_t animation_type, uint16_t animation_step,
				ui_point_t *start, ui_point_t *stop);

void view_aniation_process(ui_view_animation_t *animation);

void view_aniation_cancel(ui_view_animation_t *animation);

/**
 * @brief view refresh
 *
 * This routine provide view refresh
 *
 * @param view_id id of view
 *
 * @return true if invoked succsess.
 * @return false if invoked failed.
 */
int view_refresh(uint32_t view_id);

/**
 * @brief view repaint
 *
 * This routine provide view refresh
 *
 * @param view_id id of view
 *
 * @return true if invoked succsess.
 * @return false if invoked failed.
 */
int view_paint(uint32_t view_id);

/**
 * @brief view set focus
 *
 * This routine provide view set focus
 *
 * @param view_id id of view
 * @focus is focus yes or no
 *
 * @return true if invoked succsess.
 * @return false if invoked failed.
 */
int view_set_focus(uint32_t view_id, bool focus);
int view_set_hide(uint32_t view_id, bool hidden);
bool view_is_hidden(uint32_t view_id);
/**
 * @brief get focused view
 *
 * This routine provide get focused view
 *
 * @return view id of focused view, -1 no focused view .
 */
int view_get_focus(void);

int view_manager_dispatch_key_event(uint32_t key_event);

int view_manager_post_display(void);

int view_manager_notify_post(void);

void view_manager_set_drity(bool drity);

int view_manager_get_hidden_attribute_view(int16_t hidden_attribute);

int view_manager_get_dragable_view(int16_t drag_drop);

int view_manager_process_animation(void);

int view_manager_animation_start(uint16_t view_id, uint16_t last_view_id, uint16_t animation_type, uint16_t animation_step);

int view_manager_animation_cancel(uint16_t view_id);

void view_manager_set_focus_view(void);

int view_manager_get_related_view(int16_t view_id);

/**
 * @brief set dirty region of view
 *
 * This routine provide set dirty region of view
 *
 * @param view_id id of view
 * @region dirty region
 *
 * @return true if invoked succsess.
 * @return false if invoked failed.
 */

bool view_set_dirty_region(uint32_t view_id, ui_region_t region);

int view_manager_set_focus_callback(ui_focus_cb_t callback);

uint32_t view_manager_get_post_id(void);

/**
 * @} end defgroup system_apis
 */
#endif
