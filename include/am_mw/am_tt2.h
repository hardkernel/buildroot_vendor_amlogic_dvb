/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file
 * \brief Teletext模块(version 2)
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2012-08-08: create the document
 ***************************************************************************/


#ifndef _AM_TT2_H
#define _AM_TT2_H

#include "am_types.h"
#include "am_osd.h"

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Type definitions
 ***************************************************************************/

/**\brief Teletext分析器句柄*/
typedef void* AM_TT2_Handle_t;

#define AM_TT2_ANY_SUBNO 0x3F7F

/**\brief Teletext模块错误代码*/
enum AM_TT2_ErrorCode
{
	AM_TT2_ERROR_BASE=AM_ERROR_BASE(AM_MOD_SUB2),
	AM_TT2_ERR_INVALID_PARAM,   /**< 参数无效*/
	AM_TT2_ERR_INVALID_HANDLE,  /**< 句柄无效*/
	AM_TT2_ERR_NOT_SUPPORTED,   /**< 不支持的操作*/
	AM_TT2_ERR_CREATE_DECODE,   /**< 打开Teletext解码器失败*/
	AM_TT2_ERR_OPEN_PES,        /**< 打开PES通道失败*/
	AM_TT2_ERR_SET_BUFFER,      /**< 设置PES 缓冲区失败*/
	AM_TT2_ERR_NO_MEM,                  /**< 空闲内存不足*/
	AM_TT2_ERR_CANNOT_CREATE_THREAD,    /**< 无法创建线程*/
	AM_TT2_ERR_NOT_RUN,            /**< 无法创建线程*/
	AM_TT2_INIT_DISPLAY_FAILED,    /**< 初始化显示屏幕失败*/
	AM_TT2_ERR_END
};

/**\brief Teletext颜色*/
typedef enum{
	AM_TT2_COLOR_RED,           /**< 红色*/
	AM_TT2_COLOR_GREEN,         /**< 绿色*/
	AM_TT2_COLOR_YELLOW,        /**< 黄色*/
	AM_TT2_COLOR_BLUE           /**< 蓝色*/
}AM_TT2_Color_t;

/**\brief 开始绘制*/
typedef void (*AM_TT2_DrawBegin_t)(AM_TT2_Handle_t handle);

/**\brief 结束绘制*/
typedef void (*AM_TT2_DrawEnd_t)(AM_TT2_Handle_t handle);

/**\brief 取得当前PTS*/
typedef uint64_t (*AM_TT2_GetPTS_t)(AM_TT2_Handle_t handle, uint64_t pts);

/**\brief Teletext参数*/
typedef struct
{
	AM_TT2_DrawBegin_t draw_begin;   /**< 开始绘制*/
	AM_TT2_DrawEnd_t   draw_end;     /**< 结束绘制*/
	AM_Bool_t        is_subtitle;    /**< 是否为字幕*/
	uint8_t         *bitmap;         /**< 绘图缓冲区*/
	int              pitch;          /**< 绘图缓冲区每行字节数*/
	void            *user_data;      /**< 用户定义数据*/
}AM_TT2_Para_t;

/**\brief 创建teletext解析句柄
 * \param[out] handle 返回创建的新句柄
 * \param[in] para teletext解析参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_tt2.h)
 */
extern AM_ErrorCode_t AM_TT2_Create(AM_TT2_Handle_t *handle, AM_TT2_Para_t *para);

/**\brief 释放teletext解析句柄
 * \param handle 要释放的句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_tt2.h)
 */
extern AM_ErrorCode_t AM_TT2_Destroy(AM_TT2_Handle_t handle);

/**\brief 设定是否为字幕
 * \param handle 要释放的句柄
 * \param subtitle 是否为字幕
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_tt2.h)
 */
extern AM_ErrorCode_t AM_TT2_SetSubtitleMode(AM_TT2_Handle_t handle, AM_Bool_t subtitle);

/**\brief 取得用户定义数据
 * \param handle 句柄
 * \return 用户定义数据
 */
extern void*          AM_TT2_GetUserData(AM_TT2_Handle_t handle);

/**\brief 分析teletext数据
 * \param handle 句柄
 * \param[in] buf PES数据缓冲区
 * \param size 缓冲区内数据大小
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_tt2.h)
 */
extern AM_ErrorCode_t AM_TT2_Decode(AM_TT2_Handle_t handle, uint8_t *buf, int size);

/**\brief 开始teletext显示
 * \param handle 句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_tt2.h)
 */
extern AM_ErrorCode_t AM_TT2_Start(AM_TT2_Handle_t handle);

/**\brief 停止teletext显示
 * \param handle 句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_tt2.h)
 */
extern AM_ErrorCode_t AM_TT2_Stop(AM_TT2_Handle_t handle);

/**\brief 跳转到指定页
 * \param handle 句柄
 * \param page_no 页号
 * \param sub_page_no 子页号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_tt2.h)
 */
extern AM_ErrorCode_t AM_TT2_GotoPage(AM_TT2_Handle_t handle, int page_no, int sub_page_no);

/**\brief 跳转到home页
 * \param handle 句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_tt2.h)
 */
extern AM_ErrorCode_t AM_TT2_GoHome(AM_TT2_Handle_t handle);

/**\brief 跳转到下一页
 * \param handle 句柄
 * \param dir 搜索方向，+1为正向，-1为反向
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_tt2.h)
 */
extern AM_ErrorCode_t AM_TT2_NextPage(AM_TT2_Handle_t handle, int dir);

/**\brief 根据颜色跳转到指定链接
 * \param handle 句柄
 * \param color 颜色
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_tt2.h)
 */
extern AM_ErrorCode_t AM_TT2_ColorLink(AM_TT2_Handle_t handle, AM_TT2_Color_t color);

/**\brief 设定搜索字符串
 * \param handle 句柄
 * \param pattern 搜索字符串
 * \param casefold 是否区分大小写
 * \param regex 是否用正则表达式匹配
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_tt2.h)
 */
extern AM_ErrorCode_t AM_TT2_SetSearchPattern(AM_TT2_Handle_t handle, const char *pattern, AM_Bool_t casefold, AM_Bool_t regex);

/**\brief 搜索指定页
 * \param handle 句柄
 * \param dir 搜索方向，+1为正向，-1为反向
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_tt2.h)
 */
extern AM_ErrorCode_t AM_TT2_Search(AM_TT2_Handle_t handle, int dir);

#ifdef __cplusplus
}
#endif



#endif
