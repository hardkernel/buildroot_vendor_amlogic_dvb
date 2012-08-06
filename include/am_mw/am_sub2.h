/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file
 * \brief Subtitle模块(version 2)
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2012-08-01: create the document
 ***************************************************************************/


#ifndef _AM_SUB2_H
#define _AM_SUB2_H

#include "am_types.h"
#include "am_osd.h"

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Type definitions
 ***************************************************************************/

/**\brief Subtitle分析器句柄*/
typedef void* AM_SUB2_Handle_t;

/**\brief Subtitle模块错误代码*/
enum AM_SUB2_ErrorCode
{
	AM_SUB2_ERROR_BASE=AM_ERROR_BASE(AM_MOD_SUB2),
	AM_SUB2_ERR_INVALID_PARAM,   /**< 参数无效*/
	AM_SUB2_ERR_INVALID_HANDLE,  /**< 句柄无效*/
	AM_SUB2_ERR_NOT_SUPPORTED,   /**< 不支持的操作*/
	AM_SUB2_ERR_CREATE_DECODE,   /**< 打开SUBTITLE解码器失败*/
	AM_SUB2_ERR_OPEN_PES,        /**< 打开PES通道失败*/
	AM_SUB2_ERR_SET_BUFFER,      /**< 失置PES 缓冲区失败*/
	AM_SUB2_ERR_NO_MEM,                  /**< 空闲内存不足*/
	AM_SUB2_ERR_CANNOT_CREATE_THREAD,    /**< 无法创建线程*/
	AM_SUB2_ERR_NOT_RUN,            /**< 无法创建线程*/
	AM_SUB2_INIT_DISPLAY_FAILED,    /**< 初始化显示屏幕失败*/
	AM_SUB2_ERR_END
};

/**\brief Subtitle region*/
typedef struct AM_SUB2_Region
{
    int32_t                 left;               /**< X坐标*/
    int32_t                 top;                /**< Y坐标*/
    uint32_t                width;              /**< 宽度*/
    uint32_t                height;             /**< 高度*/

    uint32_t                entry;              /**< 调色板颜色数*/
    AM_OSD_Color_t          clut[256];          /**< 调色板*/

    /* for background */
    uint32_t                background;         /**< 背景色*/

    /* for pixel map */
    uint8_t                *p_buf;              /**< 位图*/

    /* for text */
    uint32_t                fg;                 /**< 文字前景色*/
    uint32_t                bg;                 /**< 文字背景色*/

    uint32_t                length;             /**< 文字字符串长度*/
    uint16_t               *p_text;             /**< 文字字符串*/

    struct AM_SUB2_Region  *p_next;             /**< 链表中的下一个Region*/

}AM_SUB2_Region_t;

/**\brief Subtitle 显示*/
typedef struct AM_SUB2_Picture
{
    uint64_t                pts;                /**< 开始显示的PTS*/
    uint32_t                timeout;            /**< 显示时间(s)*/

    int32_t                 original_x;         /**< x坐标*/
    int32_t                 original_y;         /**< y坐标*/

    uint32_t                original_width;     /**< 宽度*/
    uint32_t                original_height;    /**< 高度*/

    AM_SUB2_Region_t       *p_region;           /**< Region链表*/

    struct AM_SUB2_Picture *p_prev;             /**< 链表中的前一个Picture*/
    struct AM_SUB2_Picture *p_next;             /**< 链表中的后一个Picture*/

}AM_SUB2_Picture_t;

/**\brief Subtitle显示回调*/
typedef void (*AM_SUB2_ShowCb_t)(AM_SUB2_Handle_t handle, AM_SUB2_Picture_t* pic);

/**\brief 取得当前PTS*/
typedef uint64_t (*AM_SUB2_GetPTS_t)(AM_SUB2_Handle_t handle, uint64_t pts);

/**\brief Subtitle参数*/
typedef struct
{
	AM_SUB2_ShowCb_t show;           /**< 显示回调*/
	AM_SUB2_GetPTS_t get_pts;        /**< 取得当前PTS*/
	uint16_t         composition_id; /**< Subtitle composition ID*/
	uint16_t         ancillary_id;   /**< Subtitle ancillary ID*/
	void            *user_data;      /**< 用户定义数据*/
}AM_SUB2_Para_t;

/**\brief 创建subtitle解析句柄
 * \param[out] handle 返回创建的新句柄
 * \param[in] para subtitle解析参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_sub2.h)
 */
extern AM_ErrorCode_t AM_SUB2_Create(AM_SUB2_Handle_t *handle, AM_SUB2_Para_t *para);

/**\brief 释放subtitle解析句柄
 * \param handle 要释放的句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_sub2.h)
 */
extern AM_ErrorCode_t AM_SUB2_Destroy(AM_SUB2_Handle_t handle);

/**\brief 取得用户定义数据
 * \param handle 句柄
 * \return 用户定义数据
 */
extern void*          AM_SUB2_GetUserData(AM_SUB2_Handle_t handle);

/**\brief 分析subtitle数据
 * \param handle 句柄
 * \param[in] buf PES数据缓冲区
 * \param size 缓冲区内数据大小
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_sub2.h)
 */
extern AM_ErrorCode_t AM_SUB2_Decode(AM_SUB2_Handle_t handle, uint8_t *buf, int size);

/**\brief 开始subtitle显示
 * \param handle 句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_sub2.h)
 */
extern AM_ErrorCode_t AM_SUB2_Start(AM_SUB2_Handle_t handle);

/**\brief 停止subtitle显示
 * \param handle 句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_sub2.h)
 */
extern AM_ErrorCode_t AM_SUB2_Stop(AM_SUB2_Handle_t handle);

#ifdef __cplusplus
}
#endif



#endif
