#ifndef _LVGL_RES_LOADER_H
#define _LVGL_RES_LOADER_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include <lvgl.h>
#include <res_manager_api.h>

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

typedef struct
{
	/*! scene id*/
	uint32_t id;
	/*! 横坐标*/
	lv_coord_t  x;
	/*! 纵坐标*/
	lv_coord_t  y;
	/*! 宽度	*/
	uint16_t  width;
	/*! 高度	*/
	uint16_t  height;
	/*! 背景色*/
	lv_color_t background;
	/*! scene 的透明度*/
	uint32_t  transparence;
	/*! res manager场景数据结构的指针*/
	resource_scene_t* scene_data;
} lvgl_res_scene_t;

typedef struct
{
	/*! group id*/
	uint32_t id;
	/*! 相对于上一级结构的x坐标  */
	lv_coord_t x;
	/*! 相对于上一级结构的y坐标  */
	lv_coord_t y;
	/*! 宽度  */
	uint16_t width;
	/*! 高度  */
	uint16_t height;
	/*! res manager资源组数据结构的指针*/
	resource_group_t* group_data;
} lvgl_res_group_t;

typedef struct
{
	/*! 相对于上一级结构的x坐标  */
	lv_coord_t x;
	/*! 相对于上一级结构的y坐标  */
	lv_coord_t y;
	/*! 宽度  */
	uint16_t width;
	/*! 高度  */
	uint16_t height;
	/*! 图片组帧数 */
	uint32_t frames;
	/*! 图片组id*/
	resource_picregion_t* picreg_data;
}lvgl_res_picregion_t;

typedef struct
{
	/*! 横坐标 */
	lv_coord_t  x;
	/*! 纵坐标 */
	lv_coord_t  y;
	/*! 宽度	*/
	uint16_t  width;
	/*! 高度	*/
	uint16_t  height;
	lv_color_t color;
	uint8_t* txt;
} lvgl_res_string_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/
/************************************************************************************/
/*!
 * \par  Description:
 *     清除资源缓存
 * \return       无
 * \note		
 ************************************************************************************/ 
void lvgl_res_cache_clear(void);

/************************************************************************************/
/*!
 * \par  Description:
 *     打开资源和样式文件
 * \param[in]    style_path   样式文件路径
 * \param[in]    picture_path   图片资源文件路径
 * \param[in]    text_path   文本资源文件路径
 * \return       成功返回0，失败返回-1
 * \note		系统初始化时调用一次
 ************************************************************************************/
int lvgl_res_loader_open(const char* sty_file, const char* pic_file, const char* str_file);

/************************************************************************************/
/*!
 * \par  Description:
 *     关闭资源和样式文件
 * \return       无
 * \note		系统关机时调用一次 
 ************************************************************************************/
void lvgl_res_loader_close(void);

/************************************************************************************/
/*!
 * \par  Description:
 *     加载场景
 * \param[in]    scene_id   场景id
 * \return       成功返回场景资源数据指针，失败返回NULL
 ************************************************************************************/
int lvgl_res_load_scene(uint32_t scene_id, lvgl_res_scene_t* scene);

/************************************************************************************/
/*!
 * \par  Description:
 *    	从场景中加载资资源组 
 * \param[in]    scene   场景数据指针
 * \param[in]    id   	资源组id
 * \return       成功返回资源组数据指针，失败返回NULL
 ************************************************************************************/
int lvgl_res_load_group_from_scene(lvgl_res_scene_t* scene, uint32_t id, lvgl_res_group_t* group);

/************************************************************************************/
/*!
 * \par  Description:
 *    	从资源组中加载资源组
 * \param[in]    group   资源组数据指针 
 * \param[in]    id   	资源组id
 * \return       成功返回资源组数据指针，失败返回NULL
 ************************************************************************************/
int lvgl_res_load_group_from_group(lvgl_res_group_t* group, uint32_t id, lvgl_res_group_t* subgroup);

/************************************************************************************/
/*!
 * \par  Description:
 *    	从场景中加载图片
 * \param[in]    scene   场景数据指针
 * \param[in]    id   	存放图片资源id的数组指针 
 * \param[out]   img   	存放lvgl图片数据的数组指针 
 * \param[out]   pt   	存放图片坐标的数组指针 
 * \param[in]	 num	数组元素个数
 * \return       成功返回0，失败返回-1
 ************************************************************************************/
int lvgl_res_load_pictures_from_scene(lvgl_res_scene_t* scene, const uint32_t* id, lv_img_dsc_t* img, lv_point_t* pt, uint32_t num);

/************************************************************************************/
/*!
 * \par  Description:
 *    	从场景中加载字符串 
 * \param[in]    scene   场景数据指针
 * \param[in]    id   	存放字符串资源id的数组指针 
 * \param[out]   str   	存放用于lvgl显示的字符串数据的数组指针 
 * \param[in]	 num	数组元素个数
 * \return       成功返回0，失败返回-1
 ************************************************************************************/
int lvgl_res_load_strings_from_scene(lvgl_res_scene_t* scene, const uint32_t* id, lvgl_res_string_t* str, uint32_t num);

/************************************************************************************/
/*!
 * \par  Description:
 *    	从资源组中加载图片 
 * \param[in]    group  资源组数据指针  
 * \param[in]    id   	存放图片资源id的数组指针 
 * \param[out]   img   	存放lvgl图片数据的数组指针 
 * \param[out]   pt   	存放图片坐标的数组指针 
 * \param[in]	 num	数组元素个数
 * \return       成功返回0，失败返回-1
 ************************************************************************************/
int lvgl_res_load_pictures_from_group(lvgl_res_group_t* group, const uint32_t* id, lv_img_dsc_t* img, lv_point_t* pt, uint32_t num);

/************************************************************************************/
/*!
 * \par  Description:
 *    	从资源组中加载字符串
 * \param[in]    group  资源组数据指针 
 * \param[in]    id   	存放字符串资源id的数组指针 
 * \param[out]   str   	存放用于lvgl显示的字符串数据的数组指针 
 * \param[in]	 num	数组元素个数
 * \return       成功返回0，失败返回-1
 ************************************************************************************/
int lvgl_res_load_strings_from_group(lvgl_res_group_t* group, const uint32_t* id, lvgl_res_string_t* str, uint32_t num);

/************************************************************************************/
/*!
 * \par  Description:
 *     卸载图片资源
 * \param[in]    img   	存放lvgl图片数据的数组指针 
 * \param[in]	 num	数组元素个数
 * \return       无 
 * \note		减少缓存中的引用计数，如果引用计数归0，则认为缓存可释撿
 ************************************************************************************/
void lvgl_res_unload_pictures(lv_img_dsc_t* img, uint32_t num);

/************************************************************************************/
/*!
 * \par  Description:
 *     卸载字符串资渿
 * \param[in]    img   	存放字符串数据的数组指针
 * \param[in]	 num	数组元素个数
 * \return       无 
 * \note		减少缓存中的引用计数，如果引用计数归0，则认为缓存可释放 
 ************************************************************************************/
void lvgl_res_unload_strings(lvgl_res_string_t* str, uint32_t num);

/************************************************************************************/
/*!
 * \par  Description:
 *     释放资源组数据空间 
 * \return       无
 ************************************************************************************/
void lvgl_res_unload_group(lvgl_res_group_t* group);

/************************************************************************************/
/*!
 * \par  Description:
 *     释放场景数据空间
 * \return       无 
 ************************************************************************************/
void lvgl_res_unload_scene(lvgl_res_scene_t* scene);

/************************************************************************************/
/*!
 * \par  Description:
 *     释放图片区域数据空间 
 * \return       无 
 ************************************************************************************/
void lvgl_res_unload_picregion(lvgl_res_picregion_t* picreg);

/************************************************************************************/
/*!
 * \par  Description:
 *    	从资源组中加载图片区域 
 * \param[in]    group  资源组数据指针 
 * \param[in]    id   	图片区域id
 * \param[in]	 res_picreg	图片资源数据结构指针 
 * \return       成功返回0，失败返回-1 
 ************************************************************************************/
int lvgl_res_load_picregion_from_group(lvgl_res_group_t* group, const uint32_t id, lvgl_res_picregion_t* res_picreg);

/************************************************************************************/
/*!
 * \par  Description:
 *    	从场景中加载图片区域
 * \param[in]    scene  场景数据指针
 * \param[in]    id   	图片区域id
 * \param[in]	 res_picreg	图片资源数据结构指针
 * \return       成功返回0，失败返回-1 
 ************************************************************************************/
int lvgl_res_load_picregion_from_scene(lvgl_res_scene_t* scene, const uint32_t id, lvgl_res_picregion_t* res_picreg);

/************************************************************************************/
/*!
 * \par  Description:
 *    	从图片区域中加载图片
 * \param[in]    picreg  图片区域数据指针
 * \param[in]    start   要加载的起始帧数
 * \param[in]	 end	 要加载的结束帧数
 * \param[out]   img   	存放lvgl图片数据的数组指针 
 * \return       成功返回0，失败返回-1 
 ************************************************************************************/
int lvgl_res_load_pictures_from_picregion(lvgl_res_picregion_t* picreg, uint32_t start, uint32_t end, lv_img_dsc_t* img);

/************************************************************************************/
/*!
 * \par  Description:
 *    	从场景中预加载图片 
 * \param[in]    scene  场景数据指针
 * \param[in]    id   	存放图片资源id的数组指针 
 * \param[in]	 num	数组元素个数
 * \return       成功返回0，失败返回-1 
 ************************************************************************************/
int lvgl_res_preload_pictures_from_scene(lvgl_res_scene_t* scene, const uint32_t* id, uint32_t num);

/************************************************************************************/
/*!
 * \par  Description:
 *    	从资源组中预加载图片
 * \param[in]    group  资源组数据指针 
 * \param[in]    id   	存放图片资源id的数组指针 
 * \param[in]	 num	数组元素个数
 * \return       成功返回0，失败返回-1 
 ************************************************************************************/
int lvgl_res_preload_pictures_from_group(lvgl_res_group_t* group, const uint32_t* id, uint32_t num);

/************************************************************************************/
/*!
 * \par  Description:
 *    	从图片区域中预加载图片 
 * \param[in]    picreg  图片区域数据指针
 * \param[in]    start   要加载的起始帧数
 * \param[in]	 end	 要加载的结束帧数
 * \return       成功返回0，失败返回-1 
 ************************************************************************************/
int lvgl_res_preload_pictures_from_picregion(lvgl_res_picregion_t* picreg, uint32_t start, uint32_t end);

/************************************************************************************/
/*!
 * \par  Description:
 *     取消资源预加载
 * \return       无 
 ************************************************************************************/
int lvgl_res_preload_cancel(void);

/************************************************************************************/
/*!
 * \par  Description:
 *    	从默认路径加载图片 
 * \param[in]    group_id  第一级资源组id，为可选项，不存在时填0
 * \param[in]    subgroup_id  第二级资源组id，为可选项，不存在时填0 
 * \param[in]    id   	存放图片资源id的数组指针，为必填项 
 * \param[out]   img   	存放lvgl图片数据的数组指针 
 * \param[out]   pt   	存放图片坐标的数组指针 
 * \param[in]	 num	数组元素个数
 * \return       成功返回0，失败返回-1
 * \note		默认路径从场景id起始，具体资源id结束，最多支持中间两级资源组
 ************************************************************************************/
int lvgl_res_load_pictures(uint32_t group_id, uint32_t subgroup_id, uint32_t* id, lv_img_dsc_t* img, lv_point_t* pt, uint32_t num);

/************************************************************************************/
/*!
 * \par  Description:
 *    	从默认路径加载字符串
 * \param[in]    group_id  第一级资源组id，为可选项，不存在时填0
 * \param[in]    subgroup_id  第二级资源组id，为可选项，不存在时填0 
 * \param[in]    id   	存放字符串资源id的数组指针，为必填项 
 * \param[out]   str   	存放用于lvgl显示的字符串数据的数组指针 
 * \param[in]	 num	数组元素个数
 * \return       成功返回0，失败返回-1
 * \note		默认路径从场景id起始，具体资源id结束，最多支持中间两级资源组 
 ************************************************************************************/
int lvgl_res_load_strings(uint32_t group_id, uint32_t subgroup_id, uint32_t* id, lvgl_res_string_t* str, uint32_t num);

/************************************************************************************/
/*!
 * \par  Description:
 *    	使用默认路径加载图片区域 
 * \param[in]    group_id  第一级资源组id，为可选项，不存在时填0
 * \param[in]    subgroup_id  第二级资源组id，为可选项，不存在时填0 
 * \param[in]    id   	图片区域id，为必填项
 * \param[in]	 res_picreg	图片资源数据结构指针 
 * \return       成功返回0，失败返回-1 
 * \note		默认路径从场景id起始，具体资源id结束，最多支持中间两级资源组 
 ************************************************************************************/
int lvgl_res_load_picregion(uint32_t group_id, uint32_t subgroup_id, uint32_t picreg_id, lvgl_res_picregion_t* res_picreg);

/************************************************************************************/
/*!
 * \par  Description:
 *    	设置默认场景 
 * \param[in]    scene_id  默认场景id
 * \return       成功返回0，失败返回-1
 ************************************************************************************/
int lvgl_res_set_default_scene(uint32_t scene_id);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*_LVGL_RES_LOADER_H*/

