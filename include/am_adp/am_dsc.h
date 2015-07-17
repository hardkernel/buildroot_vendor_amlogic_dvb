/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file
 * \brief 解扰器模块
 *
 * \author Gong Ke <ke.gong@amlogic.com>
 * \date 2010-08-06: create the document
 ***************************************************************************/

#ifndef _AM_DSC_H
#define _AM_DSC_H

#include "am_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

/****************************************************************************
 * Error code definitions
 ****************************************************************************/

/**\brief 解扰器模块错误代码*/
enum AM_DSC_ErrorCode
{
	AM_DSC_ERROR_BASE=AM_ERROR_BASE(AM_MOD_DSC),
	AM_DSC_ERR_INVALID_DEV_NO,          /**< 设备号无效*/
	AM_DSC_ERR_BUSY,                    /**< 设备已经被打开*/
	AM_DSC_ERR_NOT_ALLOCATED,           /**< 设备没有分配*/
	AM_DSC_ERR_CANNOT_OPEN_DEV,         /**< 无法打开设备*/
	AM_DSC_ERR_NOT_SUPPORTED,           /**< 不支持的操作*/
	AM_DSC_ERR_NO_FREE_CHAN,            /**< 没有空闲的通道*/
	AM_DSC_ERR_NO_MEM,                  /**< 空闲内存不足*/
	AM_DSC_ERR_SYS,                     /**< 系统操作错误*/
	AM_DSC_ERR_INVALID_ID,              /**< 无效的设备ID*/
	AM_DSC_ERR_END
};


/****************************************************************************
 * Type definitions
 ***************************************************************************/

/**\brief 解扰器设备开启参数*/
typedef struct
{
	int  foo;
} AM_DSC_OpenPara_t;

/**\brief 控制字类型*/
typedef enum {
        AM_DSC_KEY_TYPE_EVEN,   /**< 偶控制字*/
        AM_DSC_KEY_TYPE_ODD     /**< 奇控制字*/
} AM_DSC_KeyType_t;

/**\brief 解扰器输入源*/
typedef enum {
	AM_DSC_SRC_DMX0,         /**< TS输入0*/
	AM_DSC_SRC_DMX1 ,         /**< TS输入1*/
	AM_DSC_SRC_DMX2          /**< TS输入2*/
} AM_DSC_Source_t;

/****************************************************************************
 * Function prototypes  
 ***************************************************************************/

/**\brief 打开解扰器设备
 * \param dev_no 解扰器设备号
 * \param[in] para 解扰器设备开启参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_dsc.h)
 */
extern AM_ErrorCode_t AM_DSC_Open(int dev_no, const AM_DSC_OpenPara_t *para);

/**\brief 分配一个解扰通道
 * \param dev_no 解扰器设备号
 * \param[out] chan_id 返回解扰通道ID
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_dsc.h)
 */
extern AM_ErrorCode_t AM_DSC_AllocateChannel(int dev_no, int *chan_id);

/**\brief 设定解扰通道对应流的PID值
 * \param dev_no 解扰器设备号
 * \param chan_id 解扰通道ID
 * \param pid 流的PID
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_dsc.h)
 */
extern AM_ErrorCode_t AM_DSC_SetChannelPID(int dev_no, int chan_id, uint16_t pid);

/**\brief 设定解扰通道的控制字
 * \param dev_no 解扰器设备号
 * \param chan_id 解扰通道ID
 * \param type 控制字类型
 * \param[in] key 控制字
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_dsc.h)
 */
extern AM_ErrorCode_t AM_DSC_SetKey(int dev_no, int chan_id, AM_DSC_KeyType_t type, const uint8_t *key);

/**\brief 释放一个解扰通道
 * \param dev_no 解扰器设备号
 * \param chan_id 解扰通道ID
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_dsc.h)
 */
extern AM_ErrorCode_t AM_DSC_FreeChannel(int dev_no, int chan_id);

/**\brief 关闭解扰器设备
 * \param dev_no 解扰器设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_dsc.h)
 */
extern AM_ErrorCode_t AM_DSC_Close(int dev_no);

/**\brief 设定解扰器设备的输入源
 * \param dev_no 解扰器设备号
 * \param src 输入源
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_dsc.h)
 */
extern AM_ErrorCode_t AM_DSC_SetSource(int dev_no, AM_DSC_Source_t src);

#ifdef __cplusplus
}
#endif

#endif

