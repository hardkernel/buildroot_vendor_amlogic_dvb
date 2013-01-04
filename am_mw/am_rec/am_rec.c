/***************************************************************************
 *  Copyright C 2009 by Amlogic, Inc. All Rights Reserved.
 */
/**\file am_rec.c
 * \brief 录像管理模块
 *
 * \author Xia Lei Peng <leipeng.xia@amlogic.com>
 * \date 2011-03-30: create the document
 ***************************************************************************/

#define AM_DEBUG_LEVEL 2

#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
#include <assert.h>
#include <am_debug.h>
#include <am_mem.h>
#include <am_time.h>
#include <am_util.h>
#include <am_av.h>
#include <am_db.h>
#include <am_epg.h>
#include <am_rec.h>
#include "am_rec_internal.h"

/****************************************************************************
 * Macro definitions
 ***************************************************************************/

/**\brief 定期检查录像时间的间隔*/
#define REC_RECORD_CHECK_TIME	(10*1000)
/**\brief 开始Timeshifting录像后，检查开始播放的间隔*/
#define REC_TSHIFT_PLAY_CHECK_TIME	(1000)
/**\brief 开始Timeshifting录像后，开始播放前需要缓冲的数据长度*/
#define REC_TIMESHIFT_PLAY_BUF_SIZE	(1024*1024)
/**\brief 定时录像即将开始时，提前通知用户的间隔*/
#define REC_NOTIFY_RECORD_TIME	(60 * 1000)

/****************************************************************************
 * Type definitions
 ***************************************************************************/

/**\brief 获取录像文件长度*/
static inline long long am_rec_get_file_size(AM_REC_Recorder_t *rec)
{
	long long size = 0;
	struct stat statbuff;  
		
	if(stat(rec->rec_file_name, &statbuff) >= 0)
	{  
		size = statbuff.st_size;
	}
	
	return size;
}

/**\brief 写录像数据到文件*/
static int am_rec_data_write(int fd, uint8_t *buf, int size)
{
	int ret;
	int left = size;
	uint8_t *p = buf;

	while (left > 0)
	{
		ret = write(fd, p, left);
		if (ret == -1)
		{
			if (errno != EINTR)
			{
				AM_DEBUG(0, "Write record data failed: %s", strerror(errno));
				break;
			}
			ret = 0;
		}
		
		left -= ret;
		p += ret;
	}

	return (size - left);
}

/**\brief 录像线程*/
static void *am_rec_record_thread(void* arg)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)arg;
	int cnt, err = 0, check_time;
	uint8_t buf[256*1024];
	AM_DVR_OpenPara_t para;
	AM_DVR_StartRecPara_t spara;
	AM_REC_RecEndPara_t epara;
	
	memset(&epara, 0, sizeof(epara));
	epara.hrec = (int)rec;
	/*设置DVR设备参数*/
	memset(&para, 0, sizeof(para));
	if (AM_DVR_Open(rec->create_para.dvr_dev, &para) != AM_SUCCESS)
	{
		AM_DEBUG(0, "Open DVR%d failed", rec->create_para.dvr_dev);
		err = AM_REC_ERR_DVR;
		goto close_file;
	}

	AM_DVR_SetSource(rec->create_para.dvr_dev, rec->create_para.async_fifo_id);
	
	spara.pid_count = rec->rec_para.pid_count;
	memcpy(spara.pids, rec->rec_para.pids, sizeof(spara.pids));
	if (AM_DVR_StartRecord(rec->create_para.dvr_dev, &spara) != AM_SUCCESS)
	{
		AM_DEBUG(0, "Start DVR%d failed", rec->create_para.dvr_dev);
		err = AM_REC_ERR_DVR;
		goto close_dvr;
	}

	/*从DVR设备读取数据并存入文件*/
	while (rec->stat_flag & REC_STAT_FL_RECORDING)
	{
		if (rec->rec_start_time > 0 && rec->rec_para.total_time > 0)
		{
			AM_TIME_GetClock(&check_time);
			if ((check_time-rec->rec_start_time) >= rec->rec_para.total_time)
			{
				AM_DEBUG(1, "Reach record end time, now will stop recording...");
				break;
			}
		}
			
		cnt = AM_DVR_Read(rec->create_para.dvr_dev, buf, sizeof(buf), 1000);
		if (cnt <= 0)
		{
			AM_DEBUG(1, "No data available from DVR%d", rec->create_para.dvr_dev);
			usleep(200*1000);
			continue;
		}
		if (rec->rec_para.is_timeshift)
		{
			if (AM_AV_TimeshiftFillData(0, buf, cnt) != AM_SUCCESS)
			{
				err = AM_REC_ERR_CANNOT_WRITE_FILE;
				break;
			}
		}
		else
		{
			if (am_rec_data_write(rec->rec_fd, buf, cnt) == 0)
			{
				err = AM_REC_ERR_CANNOT_WRITE_FILE;
				break;
			}
			else if (! rec->rec_start_time)
			{
				/*已有数据写入文件，记录开始时间*/
				AM_TIME_GetClock(&rec->rec_start_time);
			}
		}
	}

close_dvr:
	AM_DVR_StopRecord(rec->create_para.dvr_dev);
	AM_DVR_Close(rec->create_para.dvr_dev);
close_file:
	if (rec->rec_fd != -1)
	{
		close(rec->rec_fd);
		rec->rec_fd = -1;
	}

	if (! rec->rec_para.is_timeshift)
	{
		int duration = 0;
	
		if (rec->rec_start_time > 0)
		{
			AM_TIME_GetClock(&check_time);
			duration = check_time - rec->rec_start_time;
		}
		duration /= 1000;
		epara.total_size = am_rec_get_file_size(rec);
		epara.total_time = duration;
		epara.error_code = err;
		AM_DEBUG(1, "Record end , duration %d:%02d:%02d", duration/3600, (duration%3600)/60, duration%60);
	}

	if ((rec->stat_flag & REC_STAT_FL_RECORDING))
	{
		/*通知录像结束*/
		AM_EVT_Signal((int)rec, AM_REC_EVT_RECORD_END, (void*)&epara);
		rec->stat_flag &= ~REC_STAT_FL_RECORDING;
	}
	
	return NULL;
}


/**\brief 取录像参数*/
static int am_rec_fill_rec_param(AM_REC_Recorder_t *rec, AM_REC_RecPara_t *start_para)
{
	if (start_para->pid_count <= 0)
		return AM_REC_ERR_INVALID_PARAM;
		
	snprintf(rec->rec_file_name, sizeof(rec->rec_file_name), 
		"%s/DVBRecordFiles", rec->create_para.store_dir);	
	/*尝试创建录像文件夹*/
	if (mkdir(rec->rec_file_name, 0666) && errno != EEXIST)
	{
		AM_DEBUG(0, "Cannot create record store directory '%s', error: %s.", 
			rec->rec_file_name, strerror(errno));	
		if (errno == EACCES)
			return AM_REC_ERR_CANNOT_ACCESS_FILE;
			
		return AM_REC_ERR_CANNOT_OPEN_FILE;
	}
		
	/*设置输出文件名*/
	if (rec->stat_flag & REC_STAT_FL_TIMESHIFTING)
	{
		snprintf(rec->rec_file_name, sizeof(rec->rec_file_name), 
			"%s/DVBRecordFiles/REC_TimeShifting%d.amrec", 
			rec->create_para.store_dir, 0/*AV_DEV*/);
	}
	else
	{
		int now;
		time_t tnow;
		struct stat st;
		struct tm stim;

		/*新建一个不同名的文件名*/
		AM_EPG_GetUTCTime(&now);
		tnow = (time_t)now;
		gmtime_r(&tnow, &stim);
		srand(now);
		do
		{
			now = rand();
			snprintf(rec->rec_file_name, sizeof(rec->rec_file_name), 
				"%s/DVBRecordFiles/REC_%04d%02d%02d_%d.amrec", rec->create_para.store_dir, 
				stim.tm_year + 1900, stim.tm_mon+1, stim.tm_mday, now);
			if (!stat(rec->rec_file_name, &st))
			{
				continue;
			}
			else if (errno == ENOENT)
			{
				break;
			}
			else
			{
				AM_DEBUG(1, "Try search file in %s/DVBRecordFiles Failed, error: %s", 
					rec->create_para.store_dir, strerror(errno));
				if (errno == EACCES)
					return AM_REC_ERR_CANNOT_ACCESS_FILE;
				return AM_REC_ERR_CANNOT_OPEN_FILE;
			}	
		}while(1);
	}
	
	return 0;
}

/**\brief 开始录像*/
static int am_rec_start_record(AM_REC_Recorder_t *rec, AM_REC_RecPara_t *start_para)
{
	int rc, ret = 0;
	
	if (rec->stat_flag & REC_STAT_FL_RECORDING)
	{
		AM_DEBUG(1, "Already recording now, cannot start");
		ret = AM_REC_ERR_BUSY;
		goto start_end;
	}
	/*取录像相关参数*/
	if ((ret = am_rec_fill_rec_param(rec, start_para)) != 0)
		goto start_end;
		
	/*打开录像文件*/
	rec->rec_fd = -1;
	if (! rec->rec_para.is_timeshift)
	{
		rec->rec_fd = open(rec->rec_file_name, O_TRUNC|O_CREAT|O_WRONLY, 0666);
		if (rec->rec_fd == -1)
		{
			AM_DEBUG(1, "Cannot open record file '%s', cannot start", rec->rec_file_name);
			ret = AM_REC_ERR_CANNOT_OPEN_FILE;
			goto start_end;
		}
	}
	
	rec->rec_start_time = 0;
	rec->stat_flag |= REC_STAT_FL_RECORDING;
	rec->rec_para = *start_para;
	rc = pthread_create(&rec->rec_thread, NULL, am_rec_record_thread, (void*)rec);
	if (rc)
	{
		AM_DEBUG(0, "Create record thread failed: %s", strerror(rc));
		ret = AM_REC_ERR_CANNOT_CREATE_THREAD;
		goto start_end;
	}

start_end:
	AM_DEBUG(0, "start record return %d", ret);
	if (ret != 0)
	{
		rec->stat_flag &= ~REC_STAT_FL_RECORDING;
		if (rec->rec_fd != -1)
			close(rec->rec_fd);
		rec->rec_fd = -1;
		rec->rec_start_time = 0;
		if (ret != AM_REC_ERR_BUSY)
		{
			AM_REC_RecEndPara_t epara;
			epara.hrec = (int)rec;
			epara.error_code = ret;
			epara.total_size = 0;
			epara.total_time = 0;
			AM_EVT_Signal((int)rec, AM_REC_EVT_RECORD_END, (void*)&epara);
		}
	}
	
	return ret;
}

/**\brief 停止录像*/
static int am_rec_stop_record(AM_REC_Recorder_t *rec)
{
	if (! (rec->stat_flag & REC_STAT_FL_RECORDING))
		return 0;

	rec->stat_flag &= ~REC_STAT_FL_RECORDING;
	/*As in record thread will call AM_EVT_Signal, wo must unlock it*/
	pthread_mutex_unlock(&rec->lock);
	pthread_join(rec->rec_thread, NULL);
	pthread_mutex_lock(&rec->lock);
	rec->rec_start_time = 0;
	
	return 0;
}

/****************************************************************************
 * API functions
 ***************************************************************************/
/**\brief 创建一个录像管理器
 * \param [in] para 创建参数
 * \param [out] handle 返回录像管理器句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
AM_ErrorCode_t AM_REC_Create(AM_REC_CreatePara_t *para, int *handle)
{
	AM_REC_Recorder_t *rec;
	pthread_mutexattr_t mta;
	
	assert(para && handle);

	*handle = 0;
	rec = (AM_REC_Recorder_t*)malloc(sizeof(AM_REC_Recorder_t));
	if (! rec)
	{
		AM_DEBUG(0, "no enough memory");
		return AM_REC_ERR_NO_MEM;
	}
	memset(rec, 0, sizeof(AM_REC_Recorder_t));
	rec->create_para = *para;

	/*尝试创建输出文件夹*/
	if (mkdir(para->store_dir, 0666) && errno != EEXIST)
	{
		AM_DEBUG(0, "**Waring:Cannot create store directory '%s'**.", para->store_dir);	
	}

	pthread_mutexattr_init(&mta);
	pthread_mutexattr_settype(&mta, PTHREAD_MUTEX_RECURSIVE_NP);
	pthread_mutex_init(&rec->lock, &mta);
	pthread_mutexattr_destroy(&mta);

	*handle = (int)rec;
	AM_DEBUG(1, "return handle %x, %p", *handle, rec);

	return AM_SUCCESS;
}

/**\brief 销毁一个录像管理器
 * param handle 录像管理器句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
AM_ErrorCode_t AM_REC_Destroy(int handle)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)handle;
	pthread_t thread;
	
	assert(rec);
	
	AM_REC_StopRecord(handle);
	pthread_mutex_destroy(&rec->lock);
	free(rec);

	return AM_SUCCESS;
}

/**\brief 开始录像
 * \param handle 录像管理器句柄
 * \param [in] start_para 录像参数
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
AM_ErrorCode_t AM_REC_StartRecord(int handle, AM_REC_RecPara_t *start_para)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)handle;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	AM_DEBUG(1, "handle %x, %p", handle, rec);
	assert(rec && start_para);
	pthread_mutex_lock(&rec->lock);
	ret = am_rec_start_record(rec, start_para);
	pthread_mutex_unlock(&rec->lock);

	return ret;
}

/**\brief 停止录像
 * \param handle 录像管理器句柄
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
AM_ErrorCode_t AM_REC_StopRecord(int handle)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)handle;
	AM_ErrorCode_t ret = AM_SUCCESS;
	
	assert(rec);
	
	pthread_mutex_lock(&rec->lock);
	ret = am_rec_stop_record(rec);
	pthread_mutex_unlock(&rec->lock);
	
	return AM_SUCCESS;
}

/**\brief 设置用户数据
 * \param handle EPG句柄
 * \param [in] user_data 用户数据
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
AM_ErrorCode_t AM_REC_SetUserData(int handle, void *user_data)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)handle;

	if (rec)
	{
		pthread_mutex_lock(&rec->lock);
		rec->user_data = user_data;
		pthread_mutex_unlock(&rec->lock);
	}

	return AM_SUCCESS;
}

/**\brief 取得用户数据
 * \param handle Scan句柄
 * \param [in] user_data 用户数据
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
AM_ErrorCode_t AM_REC_GetUserData(int handle, void **user_data)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)handle;

	assert(user_data);
	
	if (rec)
	{
		pthread_mutex_lock(&rec->lock);
		*user_data = rec->user_data;
		pthread_mutex_unlock(&rec->lock);
	}

	return AM_SUCCESS;
}

/**\brief 设置录像保存路径
 * \param handle Scan句柄
 * \param [in] path 路径
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
AM_ErrorCode_t AM_REC_SetRecordPath(int handle, const char *path)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)handle;

	assert(rec && path);
	
	pthread_mutex_lock(&rec->lock);
	AM_DEBUG(1, "Set record store path to: '%s'", path);
	strncpy(rec->create_para.store_dir, path, sizeof(rec->create_para.store_dir)-1);
	pthread_mutex_unlock(&rec->lock);

	return AM_SUCCESS;
}

/**\brief 获取当前录像信息
 * \param handle 录像管理器句柄
 * \param [out] info 当前录像信息
 * \return
 *   - AM_SUCCESS 成功
 *   - 其他值 错误代码(见am_rec.h)
 */
AM_ErrorCode_t AM_REC_GetRecordInfo(int handle, AM_REC_RecInfo_t *info)
{
	AM_REC_Recorder_t *rec = (AM_REC_Recorder_t *)handle;

	assert(rec && info);

	memset(info, 0, sizeof(AM_REC_RecInfo_t));
	pthread_mutex_lock(&rec->lock);
	if ((rec->stat_flag & REC_STAT_FL_RECORDING)&&rec->rec_start_time!=0)
	{
		int now;
		
		strncpy(info->file_path, rec->rec_file_name, sizeof(info->file_path));
		info->file_size = am_rec_get_file_size(rec);
		AM_TIME_GetClock(&now);
		info->cur_rec_time = (now - rec->rec_start_time)/1000;
		info->create_para = rec->create_para;
		info->record_para = rec->rec_para;
	}
	pthread_mutex_unlock(&rec->lock);

	return AM_SUCCESS;
}

