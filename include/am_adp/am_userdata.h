/***************************************************************************
 *  Copyright C 2013 by Amlogic, Inc. All Rights Reserved.
 */
/**\file am_userdata.h
 * \brief user data驱动模块
 *
 * \author Xia Lei Peng <leipeng.xia@amlogic.com>
 * \date 2013-3-13: create the document
 ***************************************************************************/

#ifndef _AM_USERDATA_H
#define _AM_USERDATA_H

#include <unistd.h>
#include <sys/types.h>
#include <am_types.h>

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

/****************************************************************************
 * Type definitions
 ***************************************************************************/

/**\brief USERDATA模块错误代码*/
enum AM_USERDATA_ErrorCode
{
	AM_USERDATA_ERROR_BASE=AM_ERROR_BASE(AM_MOD_USERDATA),
	AM_USERDATA_ERR_INVALID_ARG,			/**< 参数无效*/
	AM_USERDATA_ERR_INVALID_DEV_NO,		/**< 设备号无效*/
	AM_USERDATA_ERR_BUSY,				 /**< 设备已经被打开*/
	AM_USERDATA_ERR_CANNOT_OPEN_DEV,         /**< 无法打开设备*/
	AM_USERDATA_ERR_NOT_SUPPORTED,           /**< 不支持的操作*/
	AM_USERDATA_ERR_NO_MEM,                  /**< 空闲内存不足*/
	AM_USERDATA_ERR_TIMEOUT,                 /**< 等待设备数据超时*/
	AM_USERDATA_ERR_SYS,                     /**< 系统操作错误*/
	AM_USERDATA_ERR_END
};


/**\brief USERDATA设备开启参数*/
typedef struct
{
	int    foo;	
} AM_USERDATA_OpenPara_t;

/****************************************************************************
 * Function prototypes  
 ***************************************************************************/

/**\brief 打开USERDATA设备
 * \param dev_no USERDATA设备号
 * \param[in] para USERDATA设备开启参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_dvr.h)
 */
extern AM_ErrorCode_t AM_USERDATA_Open(int dev_no, const AM_USERDATA_OpenPara_t *para);

/**\brief 关闭USERDATA设备
 * \param dev_no USERDATA设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_dvr.h)
 */
extern AM_ErrorCode_t AM_USERDATA_Close(int dev_no);

/**\brief 从USERDATA读取数据
 * \param dev_no USERDATA设备号
 * \param	[out] buf 缓冲区
 * \param size	需要读取的数据长度
 * \param timeout 读取超时时间 ms 
 * \return
 *   - 实际读取的字节数
 */
extern int AM_USERDATA_Read(int dev_no, uint8_t *buf, int size, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif

