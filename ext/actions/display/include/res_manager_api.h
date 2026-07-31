#ifndef __RES_MANAGER_API_H__
#define __RES_MANAGER_API_H__

#include <fs/fs.h>

typedef enum 
{
	RESOURCE_TYPE_GROUP=3,
	RESOURCE_TYPE_PICTURE=4,
	RESOURCE_TYPE_TEXT=2,
	RESOURCE_TYPE_PICREGION=1, //fixme
} resource_type_e;

typedef enum
{
	RESOURCE_BITMAP_FORMAT_RGB565,
	RESOURCE_BITMAP_FORMAT_ARGB8565,
	RESOURCE_BITMAP_FORMAT_RGB888,
	RESOURCE_BITMAP_FORMAT_ARGB8888,
} resource_bitmap_format_e;

/*!
*\brief
    当前应用资源文件总的数据结构
*/
typedef struct style_s
{
	/*! style文件句柄*/
	struct fs_file_t style_fp;
	/*! 图片资源文件句柄*/
	struct fs_file_t pic_fp;
	/*! 文本资源文件句柄*/
	struct fs_file_t text_fp;
	/*! 场景的总数 */
	uint32_t sum; 
	/*! 场景的索引 */
	int32_t* scenes;
} resource_info_t;

/*!
*\brief
    场景的数据结构
*/
typedef struct
{		  
	/*! 横坐标  */
	int16_t  x;
	/*! 纵坐标  */
	int16_t  y;
	/*! 宽度	*/
	int16_t  width;
	/*! 高度	*/
	int16_t  height;
	/*! 背景色  */
	uint32_t background;	 
	/*! scene 是否可见	*/
	uint8_t  visible;
	/*! scene 是否透明 */
	uint8_t   opaque;
	/*! scene 的透明度	*/
	uint16_t  transparence;
	
	/*! 该scene 中孩子资源的数目*/
	uint32_t resource_sum;
	/*! 孩子的偏移量 */
	uint32_t child_offset;
	/*! 场景方向*/
	uint8_t direction;
	uint8_t keys[ 16 ];
} resource_scene_t;


/*!
*\brief
    资源组的数据结构
*/
typedef struct
{
	/*!资源类型*/
	uint32_t type;
	/*! 相对于上一级结构的x坐标  */
	int32_t x;
	/*! 相对于上一级结构的y坐标  */
	int32_t y;   
	/*! 宽度  */
	uint32_t width;
	/*! 高度  */
	uint32_t height;
	/*! 该资源组中资源的数目*/
	uint32_t resource_sum;
	/*! 该资源组中资源的偏移，内部加载资源用，调用者不需要关心*/
	uint32_t child_offset;	
	/*! 该资源组对应的的资源数据原点，调用者不需要关心*/
	uint32_t start;
} resource_group_t;

/*!
*\brief
    图片资源的数据结构
*/
typedef struct
{
	/*!资源类型*/
	uint32_t type;	
	/*!资源id*/
	uint32_t id;
	/*! 相对于上一级结构的x坐标  */
	int32_t x;
	/*! 相对于上一级结构的y坐标  */
	int32_t y;
	/*! 宽度  */
	uint32_t width;
	/*! 高度  */
	uint32_t height;
	/*! 每像素字节数  */
	uint32_t bytes_per_pixel;
	/*! 位图格式  */
	uint32_t format;	
	/*! 位图buffer  */
	uint8_t* buffer;
} resource_bitmap_t;

/*!
*\brief
    字符串资源的数据结构
*/
typedef struct
{
	/*!资源类型*/
	uint32_t type;
	/*!资源id*/
	uint32_t id;
	/*! 相对于上一级结构的x坐标  */
	int32_t x;
	/*! 相对于上一级结构的y坐标  */
	int32_t y;
	/*! 宽度  */
	uint32_t width;
	/*! 高度  */
	uint32_t height;
	/*! 字体大小  */
	uint32_t font_size;
	/*! 文本颜色  */
	uint32_t color;
	/*! 文本内容（默认utf8编码）*/
	uint8_t* buffer;	
} resource_text_t;

/*!
 *\brief
 	图片组资源的数据结构
*/
typedef struct
{
	uint32_t type;
 	int32_t x;
	int32_t y;
	uint32_t width;
	uint32_t height;
	uint32_t frames;
	uint32_t pic_offset;
	uint32_t start;
	uint32_t* id;
}resource_picregion_t;


void res_manager_init(void);

/************************************************************************************/
/*!
 * \par  Description:
 *     清除资源缓存
 * \param[in]    无 
 * \return       无
 ************************************************************************************/
void res_manager_clear_cache(void);

/************************************************************************************/
/*!
 * \par  Description:
 *     打开资源和样式文件
 * \param[in]    style_path   样式文件路径 
 * \param[in]    picture_path   图片资源文件路径
 * \param[in]    text_path   文本资源文件路径
 * \return       成功返回资源数据指针，失败返回NULL
 * \note	样式文件存放图片和文本资源的布局信息，按照场景->资源组->资源的结构存放。
 *			ui edit会生成一个头文件，定义样式文件中的场景，资源组和资源的id（宏定义），
 *			方便调用者在代码中引用，这个id和资源文件中的资源id是不一样的。
 *			图片资源文件存放图片资源的具体数据。
 *			文本资源文件存放字符串文本资源的具体数据。
 ************************************************************************************/
resource_info_t* res_manager_open_resources( const char* style_path, const char* picture_path, const char* text_path );

/************************************************************************************/
/*!
 * \par  Description:
 *     关闭资源和样式文件
 * \param[in]    res_info   资源数据指针 
 * \return       无
 ************************************************************************************/
void res_manager_close_resources( resource_info_t* res_info );

/************************************************************************************/
/*!
 * \par  Description:
 *     加载场景
 * \param[in]    res_info   资源数据指针 
 * \param[in]    scene_id   场景id
 * \return       成功返回场景数据指针，失败返回NULL
 ************************************************************************************/
resource_scene_t* res_manager_load_scene(resource_info_t* res_info, uint32_t scene_id);

/************************************************************************************/
/*!
 * \par  Description:
 *     释放场景
 * \param[in]    scene   场景数据指针 
 * \return       无
 ************************************************************************************/
void res_manager_unload_scene(resource_scene_t* scene);

/************************************************************************************/
/*!
 * \par  Description:
 *     加载场景
 * \param[in]    scene   场景数据指针 
 * \param[in]    id   资源或资源组id，这个id是定义在ui editor生成的头文件内的。
 * \return       成功返回资源或资源组数据指针，失败返回NULL
 * \note	返回的实际数据类型是根据id所对应的资源数据所决定的，因为调用者应当知道
 *			资源的类型，因此对返回的指针进行类型转换即可。场景下的子资源可能是资源组，
 *			图片资源或者字符串文本资源。
 *			如果获取是图片或者文本资源会把位图和字符串编码数据加载到缓存中。
 ************************************************************************************/
void* res_manager_get_scene_child(resource_info_t*res_info, resource_scene_t* scene, uint32_t id);

/************************************************************************************/
/*!
 * \par  Description:
 *     加载场景
 * \param[in]    resgroup   资源组数据指针 
 * \param[in]    id   资源或资源组id，这个id是定义在ui editor生成的头文件内的。
 * \return       成功返回资源或资源组数据指针，失败返回NULL
 * \note	返回的实际数据类型是根据id所对应的资源数据所决定的，因为调用者应当知道
 *			资源的类型，因此对返回的指针进行类型转换即可。 场景下的子资源可能是资源组，
 *			图片资源或者字符串文本资源。
 *			如果获取是图片或者文本资源会把位图和字符串编码数据加载到缓存中。
 *	**********************************************************************************/
void* res_manager_get_group_child(resource_info_t*res_info, resource_group_t* resgroup, uint32_t id );

/************************************************************************************/
/*!
 * \par  Description:
 *     释放资源数据
 * \param[in]    resource   资源或者资源组数据指针 
 * \return       无
 ************************************************************************************/
void res_manager_release_resource(void* resource);

/************************************************************************************/
/*!
 * \par  Description:
 *     释放资源数据结构体
 * \param[in]    resource   资源或者资源组数据指针 
 * \return       无
 ************************************************************************************/
void res_manager_free_resource_structure(void* resource);

/************************************************************************************/
/*!
 * \par  Description:
 *     释放图片资源数据
 * \param[in]    resource   图片资源数据指针 
 * \return       无
 ************************************************************************************/
void res_manager_free_bitmap_data(void* data);

/************************************************************************************/
/*!
 * \par  Description:
 *     释放资字符串源数据
 * \param[in]    resource   字符串资源数据指针 
 * \return       无
 ************************************************************************************/
void res_manager_free_text_data(void* data);

resource_bitmap_t* res_manager_preload_from_scene(resource_info_t* res_info, resource_scene_t* scene, uint32_t id);
resource_bitmap_t* res_manager_preload_from_group(resource_info_t* res_info, resource_group_t* group, uint32_t id);
resource_bitmap_t* res_manager_preload_from_picregion(resource_info_t* res_info, resource_picregion_t* picreg, uint32_t frame);


#endif /*__RES_MANAGER_API_H__*/
