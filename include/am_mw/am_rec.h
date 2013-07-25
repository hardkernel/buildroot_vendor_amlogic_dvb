/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file am_rec.h
 * \brief 录像管理模块头文件
 *
 * \author Xia Lei Peng <leipeng.xia@amlogic.com>
 * \date 2011-3-30: create the document
 ***************************************************************************/

#ifndef _AM_REC_H
#define _AM_REC_H

#include <am_types.h>
#include <am_evt.h>
#include <sqlite3.h>
#include <am_dvr.h>
#include <am_av.h>

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
 * Macro definitions
 ***************************************************************************/
#define AM_REC_PATH_MAX 1024
#define AM_REC_NAME_MAX 255
#define AM_REC_SUFFIX_MAX 10

#define AM_REC_MediaInfo_t AM_AV_TimeshiftMediaInfo_t

/****************************************************************************
 * Error code definitions
 ****************************************************************************/

/**\brief 数据库模块错误代码*/
enum AM_REC_ErrorCode
{
	AM_REC_ERROR_BASE=AM_ERROR_BASE(AM_MOD_REC),
	AM_REC_ERR_INVALID_PARAM,			/**< 参数不正确*/
	AM_REC_ERR_NO_MEM,                	/**< 空闲内存不足*/
	AM_REC_ERR_CANNOT_CREATE_THREAD,	/**< 无法创建线程*/
	AM_REC_ERR_BUSY,					/**< 已经开始录像*/
	AM_REC_ERR_CANNOT_OPEN_FILE,		/**< 无法打开录像文件*/
	AM_REC_ERR_CANNOT_WRITE_FILE,		/**< 写录像文件出错*/
	AM_REC_ERR_CANNOT_ACCESS_FILE,		/**< 无法访问文件，检查权限*/
	AM_REC_ERR_DVR,						/**< 操作DVR设备出错*/
	AM_REC_ERR_END
};

/****************************************************************************
 * Type definitions
 ***************************************************************************/

/**\brief 数据库中记录的录像状态*/
enum
{
	AM_REC_STAT_NOT_START,	/**< 录像未开始*/
	AM_REC_STAT_WAIT_START,	/**< 已检测到即将开始，等待用户确认后开始录像*/
	AM_REC_STAT_RECORDING,	/**< 正在录制*/
	AM_REC_STAT_COMPLETE,	/**< 录像完成,参数为AM_REC_RecEndPara_t*/
};

/**\brief REC事件类型*/
enum AM_REC_EventType
{
	AM_REC_EVT_BASE=AM_EVT_TYPE_BASE(AM_MOD_REC),
	AM_REC_EVT_RECORD_START,			/**< 录像开始*/
	AM_REC_EVT_RECORD_END,				/**< 录像停止*/
	AM_REC_EVT_END
};

/**\brief 录像结束数据*/
typedef struct
{
	int hrec;	/**< 录像管理器句柄*/
	int error_code;	/**< 0-正常结束，其他见AM_REC_ErrorCode*/
	long long total_size;	/**< 录像完成后的文件长度*/
	int total_time;	/**< 录像完成后的总时长*/
}AM_REC_RecEndPara_t;

 /**\brief 录像管理器创建参数*/
typedef struct 
{
	int		fend_dev;		/**< 前端设备号*/
	int		dvr_dev;		/**< 录像使用的DVR设备号, 对于不同的REC需唯一*/
	int		async_fifo_id;	/**< 连接到硬件的ASYNC FIFO id，对于不同的REC需唯一*/
	char	store_dir[AM_REC_PATH_MAX];	/**< 录像文件存储位置*/
}AM_REC_CreatePara_t;

/**\brief 录像参数*/
typedef struct
{
	AM_Bool_t is_timeshift;	/**< 是否是时移录像*/
	int pmt_pid;
	AM_REC_MediaInfo_t media_info;
	int total_time;			/**< 需要录制的总时间，单位秒，<=0表示一直录像直到调用了Stop*/
	char prefix_name[AM_REC_NAME_MAX];
	char suffix_name[AM_REC_SUFFIX_MAX];
}AM_REC_RecPara_t;

/**\brief 录像信息数据*/
typedef struct
{
	char file_path[AM_REC_PATH_MAX];	/**< 当前录像文件路径*/
	long long file_size;	/**< 当前录像文件大小，即已录制数据大小*/
	int cur_rec_time;	/**< 当前录制的总时间，单位秒*/
	AM_REC_CreatePara_t create_para;	/**< 由AM_REC_Create传入的参数*/
	AM_REC_RecPara_t record_para;	/**< 由AM_REC_StartRecord传入的录像参数*/
}AM_REC_RecInfo_t;

/****************************************************************************
 * Function prototypes  
 ***************************************************************************/

/**\brief 创建一个录像管理器
 * \param [in] para 创建参数
 * \param [out] handle 返回录像管理器句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
extern AM_ErrorCode_t AM_REC_Create(AM_REC_CreatePara_t *para, int *handle);

/**\brief 销毁一个录像管理器
 * param handle 录像管理器句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
extern AM_ErrorCode_t AM_REC_Destroy(int handle);

/**\brief 开始录像
 * \param handle 录像管理器句柄
 * \param [in] start_para 录像参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
extern AM_ErrorCode_t AM_REC_StartRecord(int handle, AM_REC_RecPara_t *start_para);

/**\brief 停止录像
 * \param handle 录像管理器句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
extern AM_ErrorCode_t AM_REC_StopRecord(int handle);

/**\brief 设置用户数据
 * \param handle 录像管理器句柄
 * \param [in] user_data 用户数据
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
extern AM_ErrorCode_t AM_REC_SetUserData(int handle, void *user_data);

/**\brief 取得用户数据
 * \param handle 录像管理器句柄
 * \param [in] user_data 用户数据
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
extern AM_ErrorCode_t AM_REC_GetUserData(int handle, void **user_data);

/**\brief 设置录像保存路径
 * \param handle 录像管理器句柄
 * \param [in] path 路径
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
extern AM_ErrorCode_t AM_REC_SetRecordPath(int handle, const char *path);

/**\brief 获取当前录像信息
 * \param handle 录像管理器句柄
 * \param [out] info 当前录像信息
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
extern AM_ErrorCode_t AM_REC_GetRecordInfo(int handle, AM_REC_RecInfo_t *info);

#ifdef __cplusplus
}
#endif

#endif

