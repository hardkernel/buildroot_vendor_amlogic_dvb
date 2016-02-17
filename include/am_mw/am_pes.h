/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file
 * \brief PES分析模块
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2012-08-01: create the document
 ***************************************************************************/


#ifndef _AM_PES_H
#define _AM_PES_H

#include "am_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Type definitions
 ***************************************************************************/


/**\brief PES模块错误代码*/
enum AM_PES_ErrorCode
{
	AM_PES_ERROR_BASE=AM_ERROR_BASE(AM_MOD_PES),
	AM_PES_ERR_INVALID_PARAM,   /**< 参数无效*/
	AM_PES_ERR_INVALID_HANDLE,  /**< 句柄无效*/
	AM_PES_ERR_NO_MEM,          /**< 空闲内存不足*/
	AM_PES_ERR_END
};

/**\brief PES分析器句柄*/
typedef void* AM_PES_Handle_t;

/**\brief PES包回调函数*/
typedef void (*AM_PES_PacketCb_t)(AM_PES_Handle_t handle, uint8_t *buf, int size);

/**\brief PES过滤器参数*/
typedef struct
{
	AM_PES_PacketCb_t packet;    /**< PES包回调*/
	void             *user_data; /**< 用户定义数据*/
}AM_PES_Para_t;

/**\brief 创建一个PES分析器
 * \param[out] handle 返回创建的句柄
 * \param[in] para PES分析器参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_sub.h)
 */
AM_ErrorCode_t AM_PES_Create(AM_PES_Handle_t *handle, AM_PES_Para_t *para);

/**\brief 释放一个PES分析器
 * \param handle 句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_sub.h)
 */
AM_ErrorCode_t AM_PES_Destroy(AM_PES_Handle_t handle);

/**\brief 分析PES数据
 * \param handle 句柄
 * \param[in] buf PES数据缓冲区
 * \param size 缓冲区中数据大小
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_sub.h)
 */
AM_ErrorCode_t AM_PES_Decode(AM_PES_Handle_t handle, uint8_t *buf, int size);

/**\brief 取得分析器中用户定义数据
 * \param handle 句柄
 * \return 用户定义数据
 */
void*          AM_PES_GetUserData(AM_PES_Handle_t handle);

#ifdef __cplusplus
}
#endif



#endif
