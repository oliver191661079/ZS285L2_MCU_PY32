#ifndef __BITMAP_FONT_API_H__
#define __BITMAP_FONT_API_H__

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/

#include <fs/fs.h>
#include <stdint.h>

/*********************
 *      DEFINES
 *********************/
 #define MAX_CACHE_NUM		180
 #define MAX_FONT_PATH_LEN	64

/**********************
 *      TYPEDEFS
 **********************/

typedef enum
{
	BITMAP_FONT_FORMAT_4BPP,
	BITMAP_FONT_FORMAT_UNSUPPORTED,
}bitmap_font_format_e;

typedef struct
{
	uint32_t advance;
	uint32_t bbw;
	uint32_t bbh;
	int32_t bbx;
	int32_t bby;
}glyph_metrics_t;


typedef struct
{
	uint32_t current;
	uint32_t cached_total;
	uint32_t* glyph_index;
	glyph_metrics_t* metrics;
	uint8_t* data;
	uint32_t inited;
}bitmap_cache_t;

typedef struct
{
	struct fs_file_t font_fp;
	bitmap_cache_t* cache;
	uint8_t font_path[MAX_FONT_PATH_LEN];
	uint16_t font_size;
	int16_t ascent;
	int16_t descent;
	uint16_t default_advance;
	uint32_t font_format;
	uint32_t loca_format;
	uint32_t glyfid_format;
	uint32_t adw_format;
	uint32_t bpp;
	uint32_t bbxy_length;
	uint32_t bbwh_length;
	uint32_t adw_length;
	uint32_t compress_alg;
	uint32_t cmap_offset;
	uint32_t loca_offset;
	uint32_t glyf_offset;
}bitmap_font_t;




/**********************
 * GLOBAL PROTOTYPES
 **********************/
bitmap_font_t* bitmap_font_open(const char* file_path);

void bitmap_font_close(bitmap_font_t* font);

uint8_t * bitmap_font_get_bitmap(bitmap_font_t* font, bitmap_cache_t* cache, uint32_t unicode);
glyph_metrics_t* bitmap_font_get_glyph_dsc(bitmap_font_t* font, bitmap_cache_t *cache, uint32_t unicode);

/**********************
 *      MACROS
 **********************/
 
#ifdef __cplusplus
} /* extern "C" */
#endif	
#endif /*__BITMAP_FONT_API_H__*/
