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


/****************************************************************************
 * Error code definitions
 ****************************************************************************/

/**\brief 数据库模块错误代码*/
enum AM_REC_ErrorCode
{
	AM_REC_ERROR_BASE=AM_ERROR_BASE(AM_MOD_REC),
	AM_REC_ERR_INVALID_PARAM,			/**< 参数不正确*/
	AM_REC_ERR_NO_MEM,                			/**< 空闲内存不足*/
	AM_REC_ERR_CANNOT_CREATE_THREAD,	/**< 无法创建线程*/
	AM_REC_ERR_BUSY,					/**< 已经开始录像*/
	AM_REC_ERR_SQLITE,					/**< Sqlite3执行出错*/
	AM_REC_ERR_TIME_CONFLICT,			/**< 录像时间段冲突*/
	AM_REC_ERR_CANNOT_OPEN_FILE,		/**< 无法打开录像文件*/
	AM_REC_ERR_CANNOT_WRITE_FILE,		/**< 写录像文件出错*/
	AM_REC_ERR_DVR,						/**< 操作DVR设备出错*/
	AM_REC_ERR_END
};

/****************************************************************************
 * Type definitions
 ***************************************************************************/

/**\brief 录像类型*/
typedef enum
{
	AM_REC_TYPE_EVENT,		/**< 录制一个指定的事件，可用于定时自动录像*/
	AM_REC_TYPE_SERVICE,		/**< 录制一个指定的频道*/
	AM_REC_TYPE_CHAN_NUM,	/**< 录制一个指定频道号的频道*/
	AM_REC_TYPE_PID			/**< 录制指定的PID*/
}AM_REC_RecType_t;

/**\brief 数据库中记录的录像状态*/
enum
{
	AM_REC_STAT_NOT_START,	/**< 录像未开始*/
	AM_REC_STAT_WAIT_START,	/**< 已检测到即将开始，等待用户确认后开始录像*/
	AM_REC_STAT_RECORDING,	/**< 正在录制*/
	AM_REC_STAT_COMPLETE,	/**< 录像完成*/
};

/**\brief REC事件类型*/
enum AM_REC_EventType
{
	AM_REC_EVT_BASE=AM_EVT_TYPE_BASE(AM_MOD_REC),
	AM_REC_EVT_NEW_RECORD,				/**< 监测到有新的录像即将开始, 参数为录像对应的数据库索引*/
	AM_REC_EVT_NEW_RECORD_CONFLICT,	/**< 监测到有新的录像即将开始, 但当前已有录像进行，参数为新录像对应的数据库索引*/
	AM_REC_EVT_RECORD_START,			/**< 录像开始，参数为新录像对应的数据库索引*/
	AM_REC_EVT_RECORD_END,				/**< 录像停止*/
	AM_REC_EVT_TIMESHIFTING_READY,		/**< TimeShifting初始化完毕，可以进行播放操作*/
	AM_REC_EVT_NOTIFY_ERROR,			/**< 错误通知，参数为错误代码, 见AM_REC_ErrorCode*/
	AM_REC_EVT_END
};

 /**\brief 录像管理器创建参数*/
typedef struct 
{
	int		fend_dev;	/**< 前端设备号*/
	int		dvr_dev;		/**< 录像使用的DVR设备号, 对于不同的REC需唯一*/
	int		async_fifo_id;	/**< 连接到硬件的ASYNC FIFO id，对于不同的REC需唯一*/
	sqlite3	*hdb;		/**< 数据库句柄*/
	char	store_dir[256];	/**< 录像文件存储位置*/
}AM_REC_CreatePara_t;

/**\brief Event录像*/
typedef struct
{
	int db_evt_id;	/**< 需要录像的事件的数据库索引*/
}AM_REC_Event_t;

/**\brief Service录像*/
typedef struct
{
	int db_srv_id;	/**< 需要录像的sevice的数据库索引*/
}AM_REC_Service_t;

/**\brief Channel number 录像*/
typedef struct
{
	int num;	/**< 需要录像的频道的频道号*/
	
}AM_REC_ChanNum_t;

/**\brief PID录像*/
typedef struct
{
	int pid_count;						/**< 指定PID个数*/
	int pids[AM_DVR_MAX_PID_COUNT];	/**< 指定每个PID*/
}AM_REC_Pid_t;

/**\brief 录像参数*/
typedef struct
{
	AM_REC_RecType_t	type;	/**< 录像类型*/
	union
	{
		AM_REC_Event_t		event;		/**< type=AM_REC_TYPE_EVENT时使用*/
		AM_REC_Service_t		service;		/**< type=AM_REC_TYPE_PROGRAM时使用*/
		AM_REC_ChanNum_t 	chan_num;	/**< type=AM_REC_TYPE_CHAN_NUM时使用*/
		AM_REC_Pid_t			pid;			/**< type=AM_REC_TYPE_PID时使用*/
	}u;
}AM_REC_RecPara_t;

/**\brief 定时录像参数*/
typedef struct
{
	int				start_time;	/**< 定时开始录像时间，UTC*/
	int				duration;		/**< 录像时长*/
	AM_REC_RecPara_t	rec_para;		/**< 录像参数*/
}AM_REC_TimeRecPara_t;

/**\brief TimeShifting参数*/
typedef struct
{
	int				av_dev;	/**< 进行Timeshifting播放时使用的AV设备号*/
	int				playback_dmx_dev;	/**< 进行回放时使用的demux设备号*/
	int				duration;	/**< Timeshifting总时长*/
	AM_REC_RecPara_t rec_para;	/**< 录像参数*/
}AM_REC_TimeShiftingPara_t;

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
 * \param db_rec_id 录像的数据库索引
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
extern AM_ErrorCode_t AM_REC_StartRecord(int handle, int db_rec_id);

/**\brief 停止录像
 * \param handle 录像管理器句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
extern AM_ErrorCode_t AM_REC_StopRecord(int handle);

/**\brief 增加一个定时录像
 * \param handle 录像管理器句柄
 * \param [in] para 定时录像参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
extern AM_ErrorCode_t AM_REC_AddTimeRecord(int handle, AM_REC_TimeRecPara_t *para);

/**\brief 开始TimeShifting
 * \param handle 录像管理器句柄
 * \param [in] para 录像参数, para->type不能为AM_REC_TYPE_EVENT
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
extern AM_ErrorCode_t AM_REC_StartTimeShifting(int handle, AM_REC_TimeShiftingPara_t *para);

/**\brief 播放TimeShifting
 * \param handle 录像管理器句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
extern AM_ErrorCode_t AM_REC_PlayTimeShifting(int handle);

/**\brief 停止TimeShifting
 * \param handle 录像管理器句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
extern AM_ErrorCode_t AM_REC_StopTimeShifting(int handle);

/**\brief 暂停TimeShifting播放
 * \param handle 录像管理器句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
extern AM_ErrorCode_t AM_REC_PauseTimeShifting(int handle);

/**\brief 恢复TimeShifting播放
 * \param handle 录像管理器句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
extern AM_ErrorCode_t AM_REC_ResumeTimeShifting(int handle);

/**\brief TimeShifting快速向前播放
 * \param handle 录像管理器句柄
 * \param speed 播放速度
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
extern AM_ErrorCode_t AM_REC_FastForwardTimeShifting(int handle, int speed);

/**\brief TimeShifting快速向后播放
 * \param handle 录像管理器句柄
  * \param speed 播放速度
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
extern AM_ErrorCode_t AM_REC_FastBackwardTimeShifting(int handle, int speed);

/**\brief TimeShifting设定当前播放位置
 * \param handle 录像管理器句柄
 * \param pos 播放位置
 * \param start 是否开始播放
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
extern AM_ErrorCode_t AM_REC_SeekTimeShifting(int handle, int pos, AM_Bool_t start);

/**\brief 获取TimeShifting播放信息
 * \param handle 录像管理器句柄
  * \param [out] info 播放信息
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
extern AM_ErrorCode_t AM_REC_GetTimeShiftingInfo(int handle, AM_AV_PlayStatus_t *info);

/**\brief 设置用户数据
 * \param handle EPG句柄
 * \param [in] user_data 用户数据
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
extern AM_ErrorCode_t AM_REC_SetUserData(int handle, void *user_data);

/**\brief 取得用户数据
 * \param handle Scan句柄
 * \param [in] user_data 用户数据
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
extern AM_ErrorCode_t AM_REC_GetUserData(int handle, void **user_data);

/**\brief 设置录像保存路径
 * \param handle Scan句柄
 * \param [in] path 路径
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
extern AM_ErrorCode_t AM_REC_SetRecordPath(int handle, const char *path);

/**\brief 取当前录像时长
 * \param handle Scan句柄
 * \param [in] duration 秒为单位
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
extern AM_ErrorCode_t AM_REC_GetRecordDuration(int handle, int *duration);

#ifdef __cplusplus
}
#endif

#endif

