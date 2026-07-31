#ifndef __BT_BIP_CLIENT__
#define __BT_BIP_CLIENT__

#include <zephyr.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <atomic.h>
#include <misc/byteorder.h>
#include <misc/util.h>
#ifdef __cplusplus
extern "C" {
#endif

#define BT_BIPC_EVT_CONNECT_CFM                 (0)
#define BT_BIPC_EVT_DISCONNECT_IND              (1)
#define BT_BIPC_EVT_GET_CAP_CFM                 (2)
#define BT_BIPC_EVT_GET_IMAGE_LIST_CFM          (3)
#define BT_BIPC_EVT_GET_IMAGE_PROPERTIES_CFM    (4)
#define BT_BIPC_EVT_GET_IMAGE_CFM               (5)
#define BT_BIPC_EVT_GET_LINKED_THUMBNAIL_CFM    (6)
#define BT_BIPC_EVT_GET_LINKED_ATTACHMENT_CFM   (7)
#define BT_BIPC_EVT_DELETE_CFM                  (8)
#define BT_BIPC_EVT_ABORT_CFM                   (9)
typedef uint8_t bt_bipc_evt_t;

typedef enum {
	BT_BIPC_SUPPORTED_FEATURE_IMG_PUSH               = 0x001,
	BT_BIPC_SUPPORTED_FEATURE_IMG_PUSH_STORE         = 0x002,
	BT_BIPC_SUPPORTED_FEATURE_IMG_PUSH_PRINT         = 0x004,
	BT_BIPC_SUPPORTED_FEATURE_IMG_PUSH_DISPLAY       = 0x008,
	BT_BIPC_SUPPORTED_FEATURE_IMG_PULL               = 0x010,
	BT_BIPC_SUPPORTED_FEATURE_ADVANCEED_IMAGE_PRINT  = 0x020,
	BT_BIPC_SUPPORTED_FEATURE_AUTO_ARCHIVE           = 0x040,
	BT_BIPC_SUPPORTED_FEATURE_REMOTE_CAMERA          = 0x080,
	BT_BIPC_SUPPORTED_FEATURE_REMOTE_DISPLAY         = 0x100,
} bt_bipc_supported_feature_t;

typedef enum {
	BT_BIPC_SUPPORTED_CAP_GENERIC_IMG = 0x01,
	BT_BIPC_SUPPORTED_CAP_CAPTURING   = 0x02,
	BT_BIPC_SUPPORTED_CAP_PRINTING    = 0x04,
	BT_BIPC_SUPPORTED_CAP_DISPLAYING  = 0x08,
} bt_bipc_supported_capbilities_t;

typedef enum {
	BT_BIPC_TRANS_STRETCH = 0x01,
	BT_BIPC_TRANS_CROP    = 0x02,
	BT_BIPC_TRANS_FILL    = 0x04,
} bt_bipc_trans_t;

typedef enum {
	BT_BIPC_DATA_FLAG_CONTINUE = 0x01,
	BT_BIPC_DATA_FLAG_END      = 0x02,
} bt_bipc_data_flag_t;

typedef struct {
	uint16_t width;
	uint16_t height;
} bt_bipc_pixel_t;

typedef struct {
	char* encoding;
	bt_bipc_pixel_t min;
	bt_bipc_pixel_t max;
	uint32_t max_size;
}bt_bipc_format_t;

typedef struct {
	bt_bipc_format_t format;
	bt_bipc_trans_t trans;
}bt_bipc_perfer_format_t;

typedef struct {
	char* type;
	char* charset;
} bt_bipc_attachment_format_t;

typedef struct {
	char* created;
	char* modified;
	char* encoding;
	bt_bipc_pixel_t min;
	bt_bipc_pixel_t max;
} bt_bipc_filter_param_t;

typedef struct {
	char* handle;
	char* created;
	char* modified;
} bt_bipc_image_item_t;

typedef struct {
	int status;

	bt_bipc_supported_capbilities_t cap_mask;
	bt_bipc_supported_feature_t feature_mask;
} bt_bipc_evt_connect_cfm_t;

typedef struct {
	int status;
} bt_bipc_evt_disconnect_ind_t;

typedef struct {
	int status;
	bt_bipc_perfer_format_t* perfer;

	uint8_t format_count;
	bt_bipc_format_t* format_list;

	bt_bipc_attachment_format_t* attachment;
	bt_bipc_filter_param_t* filter;
} bt_bipc_evt_get_cap_cfm_t;

typedef struct {
	int status;
	uint16_t list_count;
	bt_bipc_image_item_t* list;
} bt_bipc_evt_get_image_list_cfm_t;

typedef struct {
	char* encoding;
	bt_bipc_pixel_t min;
	bt_bipc_pixel_t max;
	uint32_t size;
} bt_bipc_propertie_native_t;

typedef struct {
	char* encoding;
	bt_bipc_pixel_t min;
	bt_bipc_pixel_t max;
	uint32_t max_size;
	bt_bipc_trans_t trans;
} bt_bipc_propertie_variant_t;

typedef struct {
	int status;
	char* name;
	char* img_handle;
	bt_bipc_propertie_native_t native;

	uint8_t variant_count;
	bt_bipc_propertie_variant_t* variant;
} bt_bipc_evt_get_properties_cfm_t;

typedef struct {
	int status;
	bt_bipc_data_flag_t flag;
	uint16_t data_len;
	uint8_t* data;
} bt_bipc_evt_get_image_cfm_t;

typedef struct {
	int status;
} bt_bipc_evt_abort_cfm_t;

typedef struct {
	char* encoding;
	bt_bipc_pixel_t pixel;
	uint32_t size;
	uint32_t max_size;
	bt_bipc_trans_t trans;
} bt_bipc_image_desc_t;

typedef int (*bt_bip_client_evt_cb)(struct bt_conn* conn, uint8_t user_id, bt_bipc_evt_t evt, void* data);

int bt_bipc_init(bt_bip_client_evt_cb cb);

int bt_bipc_deinit(void);

uint8_t bt_bipc_connect(struct bt_conn* conn, uint16_t psm);

int bt_bipc_disconnect(struct bt_conn* conn, uint8_t user_id);

int bt_bipc_get_capabilities(struct bt_conn* conn, uint8_t user_id);

int bt_bipc_get_image_properties(struct bt_conn* conn , uint8_t user_id, char* img_handle);

int bt_bipc_get_image(struct bt_conn* conn, uint8_t user_id, char* img_handle, bt_bipc_image_desc_t* image_desc);

int bt_bipc_abort(struct bt_conn* conn, uint8_t user_id);
#ifdef __cplusplus
}
#endif
#endif
