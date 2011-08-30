/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file
 * \brief DVR模块
 *
 * \author Xia Lei Peng <leipeng.xia@amlogic.com>
 * \date 2010-12-13: create the document
 ***************************************************************************/

#ifndef _AM_DVR_H
#define _AM_DVR_H

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

#define AM_DVR_MAX_PID_COUNT  (32)

/****************************************************************************
 * Type definitions
 ***************************************************************************/

/**\brief DVR模块错误代码*/
enum AM_DVR_ErrorCode
{
	AM_DVR_ERROR_BASE=AM_ERROR_BASE(AM_MOD_DVR),
	AM_DVR_ERR_INVALID_ARG,			/**< 参数无效*/
	AM_DVR_ERR_INVALID_DEV_NO,		/**< 设备号无效*/
	AM_DVR_ERR_BUSY,				 /**< 设备已经被打开*/
	AM_DVR_ERR_NOT_ALLOCATED,           /**< 设备没有分配*/
	AM_DVR_ERR_CANNOT_CREATE_THREAD,    /**< 无法创建线程*/
	AM_DVR_ERR_CANNOT_OPEN_DEV,         /**< 无法打开设备*/
	AM_DVR_ERR_NOT_SUPPORTED,           /**< 不支持的操作*/
	AM_DVR_ERR_NO_MEM,                  /**< 空闲内存不足*/
	AM_DVR_ERR_TIMEOUT,                 /**< 等待设备数据超时*/
	AM_DVR_ERR_SYS,                     /**< 系统操作错误*/
	AM_DVR_ERR_NO_DATA,                 /**< 没有收到数据*/
	AM_DVR_ERR_CANNOT_OPEN_OUTFILE,		/**< 无法打开输出文件*/
	AM_DVR_ERR_TOO_MANY_STREAMS,		/**< PID个数太多*/
	AM_DVR_ERR_STREAM_ALREADY_ADD,		/**< 重复添加流*/
	AM_DVR_ERR_END
};


/**\brief DVR设备开启参数*/
typedef struct
{
	int    foo;	
} AM_DVR_OpenPara_t;

/**\brief 开始录像参数*/
typedef struct
{
	int		pid_count;
	int		pids[AM_DVR_MAX_PID_COUNT];
} AM_DVR_StartRecPara_t;

/**\brief DVR源*/
typedef enum
{
	AM_DVR_SRC_ASYNC_FIFO0,
	AM_DVR_SRC_ASYNC_FIFO1
}AM_DVR_Source_t;

/****************************************************************************
 * Function prototypes  
 ***************************************************************************/

/**\brief 打开DVR设备
 * \param dev_no DVR设备号
 * \param[in] para DVR设备开启参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_dvr.h)
 */
extern AM_ErrorCode_t AM_DVR_Open(int dev_no, const AM_DVR_OpenPara_t *para);

/**\brief 关闭DVR设备
 * \param dev_no DVR设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_dvr.h)
 */
extern AM_ErrorCode_t AM_DVR_Close(int dev_no);

/**\brief 设置DVR设备缓冲区大小
 * \param dev_no DVR设备号
 * \param size 缓冲区大小
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_dvr.h)
 */
extern AM_ErrorCode_t AM_DVR_SetBufferSize(int dev_no, int size);

/**\brief 开始录像
 * \param dev_no DVR设备号
 * \param [in] para 录像参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_dvr.h)
 */
extern AM_ErrorCode_t AM_DVR_StartRecord(int dev_no, const AM_DVR_StartRecPara_t *para);

/**\brief 停止录像
 * \param dev_no DVR设备号
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_dvr.h)
 */
extern AM_ErrorCode_t AM_DVR_StopRecord(int dev_no);

/**\brief 设置DVR源
 * \param dev_no DVR设备号
 * \param	src DVR源
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_dvr.h)
 */
extern AM_ErrorCode_t AM_DVR_SetSource(int dev_no, AM_DVR_Source_t src);

/**\brief 从DVR读取录像数据
 * \param dev_no DVR设备号
 * \param	[out] buf 缓冲区
 * \param size	需要读取的数据长度
 * \param timeout 读取超时时间 ms 
 * \return
 *   - 实际读取的字节数
 */
extern int AM_DVR_Read(int dev_no, uint8_t *buf, int size, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif

